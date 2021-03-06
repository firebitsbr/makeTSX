
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "TZX.h"
#include "TZX_Blocks.h"


using namespace TZX_Class;
using namespace TZX_Blocks;
using namespace Utility;


// ============================================================================================
// Class Block

Block::Block()
{
}

Block::Block(const Block& other)
{
	id = other.id;
	bytes = new ByteBuffer(*(other.bytes));
}

Block::~Block()
{
	delete bytes;
}

void Block::init(BYTE blockId, BYTE hsize, char *data, size_t size)
{
	id = blockId;
	headSize = hsize;
	bytes = new ByteBuffer((char*)data, size + 1);
	bytes->SetEndian(ORDER_LITTLE_ENDIAN);
	putByte(blockId);
	if (data != NULL) {
		put(data, size);
	}
}

size_t Block::Block::getSize()
{
	return bytes->getSize();
}

BYTE Block::getId()
{
	return id;
}

BYTE Block::getHeadSize()
{
	return headSize;
}

const char* Block::getBytes()
{
	return bytes->getBytes();
}

void Block::hexDump()
{
	bytes->Seek(0);
	while (!bytes->isEOF()) {
		cout << std::setfill ('0') << std::setw(2) << std::hex << (int)bytes->ReadUByte() << " ";
	}
	cout << endl;
}

void Block::hexCharDump()
{
	DWORD pos = 0;
	BYTE c;
	while (true) {
		//Position
		cout << TXT_YELLOW << "#" << std::setfill ('0') << std::setw(4) << std::hex << pos << TXT_RESET << "  ";
		//Hex
		bytes->Seek(pos);
		if (pos < getHeadSize()) cout << TXT_B_GREEN;
		for (int cnt=0; cnt<16; cnt++) {
			if (bytes->isEOF()) {
				cout << "   ";
			} else {
				if (pos+cnt >= getHeadSize()) cout << TXT_RESET;
				cout << std::setfill ('0') << std::setw(2) << std::hex << (int)bytes->ReadUByte() << " ";
			}
		}
		//Chars
		bytes->Seek(pos);
		cout << "  ";
		if (pos < getHeadSize()) cout << TXT_B_GREEN;
		for (int cnt=0; cnt<16; cnt++) {
			if (bytes->isEOF()) {
				cout << " ";
			} else {
				if (pos+cnt >= getHeadSize()) cout << TXT_RESET;
				c = bytes->ReadUByte();
				if (c<32 || c>126) c='.';
				cout << (char)c;
			}
		}
		cout << TXT_RESET;
		//Next line
		cout << endl;
		pos += 16;
		if (bytes->isEOF()) {
			return;
		}
	}
}

string Block::toString()
{
	std::stringstream stream;

	bytes->Seek(0);
	stream << TXT_B_BLUE << "Block #" << std::hex << (int)getId() << std::dec << TXT_RESET;

	std::string result(stream.str());
	return result;
}

Block& Block::operator=(const Block &other)
{
	id = other.id;
	bytes = other.bytes;
	return *this;
}

void Block::putByte(BYTE value)
{
	bytes->WriteUByte(value);
}

void Block::putWord(WORD value)
{
	bytes->WriteUInt16(value);
}

void Block::putWord24(WORD24 value)
{
	bytes->WriteUInt24(value);
}

void Block::putDWord(DWORD value)
{
	bytes->WriteUInt32(value);
}

void Block::putString(string value)
{
	bytes->WriteString(value);
}

void Block::put(char *data, size_t size)
{
	bytes->WriteRawData((char*)data, size);
}

void Block::put(istream &is, size_t size)
{
	bytes->WriteRawData(is, size);
}

void Block::put(ByteBuffer *buff) {
	bytes->WriteRawData((char*)buff->getBytes(), buff->getSize());
}

// ============================================================================================
// Class WithPause

WORD BlockWithPause::getPause()
{
	bytes->Seek(getPausePos());
	return bytes->ReadUInt16();
}

void BlockWithPause::setPause(WORD pause)
{
	bytes->Seek(getPausePos());
	bytes->WriteUInt16(pause);
}

void BlockWithPause::addPause(WORD pause)
{
	setPause(pause + getPause());
}

// ============================================================================================
// Block #10 - Standard speed data block
//	0x00 	- 	WORD 	    Pause after this block (ms.) {1000}
//	0x02 	N 	WORD 	    Length of data that follow
//	0x04 	- 	BYTE[N]     Data as in .TAP files

Block10::Block10(WORD pause, char *data, size_t size)
{
	init(B10_STD_BLOCK, 0x05, NULL, 4 + size*sizeof(BYTE));
	putWord(pause);
	putWord(size);
	put(data, size);
}

Block10::Block10(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 4);
	aux->WriteRawData(is, 4);
	aux->Seek(2);
	int len = aux->ReadUInt16() * sizeof(BYTE);

	init(B10_STD_BLOCK, 0x05, NULL, 4 + len);
	put(aux);
	delete aux;

	put(is, len);
}

BYTE Block10::getCurrentChecksum()
{
	bytes->Seek(bytes->getSize()-1);
	return bytes->ReadUByte();
}

BYTE Block10::getChecksum(WORD ini, WORD len)
{
	BYTE checksum = 0;
	bytes->Seek(ini);
	while (len--) {
		checksum ^= bytes->ReadUByte();
	}
	return checksum;
}

bool Block10::checksumOk()
{
	return getCurrentChecksum() == getChecksum(getHeadSize(), getSize()-getHeadSize()-1);
}

string Block10::toString()
{
	std::stringstream stream;
	stream << Block::toString() << " - Standard speed data block";
	bytes->Seek(3);
	stream << " - DataLen: " << (bytes->ReadUInt16()) << " bytes\n";
	stream << "\t\tChecksum: 0x" << std::setw(2) << std::hex << (int)getCurrentChecksum() << std::dec;
	if (checksumOk()) {
		stream << TXT_B_YELLOW << " OK" << TXT_RESET;
	} else {
		stream << " " << BG_RED << TXT_B_YELLOW << TXT_BLINK << "FAIL" << TXT_RESET << 
				"(expected 0x" << std::setw(2) << std::hex << (int)getChecksum(getHeadSize(), getSize()-getHeadSize()-1) << std::dec << ")";
	}

	return stream.str();
}

// ============================================================================================
// Block #11 - Turbo speed data block
//	0x00	-	WORD		Length of PILOT pulse {2168}
//	0x02	-	WORD		Length of SYNC first pulse {667}
//	0x04	-	WORD		Length of SYNC second pulse {735}
//	0x06	-	WORD		Length of ZERO bit pulse {855}
//	0x08	-	WORD		Length of ONE bit pulse {1710}
//	0x0A	-	WORD		Length of PILOT tone (number of pulses) {8063 header (flag<128), 3223 data (flag>=128)}
//	0x0C	-	BYTE		Used bits in the last byte (other bits should be 0) {8}
//							(e.g. if this is 6, then the bits used (x) in the last byte are: xxxxxx00, where MSb is the leftmost bit, LSb is the rightmost bit)
//	0x0D	-	WORD		Pause after this block (ms.) {1000}
//	0x0F	N	BYTE[3]		Length of data that follow
//	0x12	-	BYTE[N]		Data as in .TAP files

Block11::Block11(WORD pilotlen, WORD synclen1, WORD synclen2, WORD bit0len, WORD bit1len, WORD pilotnum, BYTE rbits, WORD pause, char *data, size_t size)
{
	init(B11_TURBO_BLOCK, 0x13, NULL, 18 + size*sizeof(BYTE));
	putWord(pilotlen);
	putWord(synclen1);
	putWord(synclen2);
	putWord(bit0len);
	putWord(bit1len);
	putWord(pilotnum);
	putByte(rbits);
	putWord(pause);
	putWord24(size);
	put(data, size);
}

Block11::Block11(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 18);
	aux->WriteRawData(is, 18);
	aux->Seek(15);
	int len = aux->ReadUInt24() * sizeof(BYTE);

	init(B11_TURBO_BLOCK, 0x13, NULL, 18 + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block11::toString()
{
	std::stringstream stream;
	stream << Block::toString() << " - Turbo speed data block";
	bytes->Seek(16);
	stream << " - DataLen: " << (bytes->ReadUInt24()) << " bytes";

	return stream.str();
}

// ============================================================================================
// Block #12 - Pure tone
//	0x00	-	WORD		Length of one pulse in T-states
//	0x02	-	WORD		Number of pulses

Block12::Block12(WORD pulselen, WORD pulsenum)
{
	init(B12_PURE_TONE, 0x05, NULL, 4);
	putWord(pulselen);
	putWord(pulsenum);
}

Block12::Block12(istream &is)
{
	init(B12_PURE_TONE, 0x05, NULL, 4);
	put(is, 4);
}

string Block12::toString()
{
	return Block::toString() + " - Pure tone";
}

// ============================================================================================
// Block #13 - Sequence of pulses of various lengths
//	0x00	N	BYTE		Number of pulses
//	0x01	-	WORD[N]		Pulses' lengths

Block13::Block13(BYTE pulsenum, WORD *lengths)
{
	init(B13_PULSE_SEQ, 0x02, NULL, 1 + pulsenum * sizeof(WORD));
	putByte(pulsenum);
	for (int i=0; i<pulsenum; i++) {
		putWord(lengths[i]);
	}
}

Block13::Block13(istream &is)
{
	BYTE num = is.get();
	int len = num * sizeof(WORD);

	init(B13_PULSE_SEQ, 0x02, NULL, 1 + len);
	putByte(num);

	put(is, len);
}

string Block13::toString()
{
	return Block::toString() + " - Sequence of pulses of various lengths";
}

// ============================================================================================
// Block #14 - Pure data block
//	0x00	-	WORD		Length of ZERO bit pulse
//	0x02	-	WORD		Length of ONE bit pulse
//	0x04	-	BYTE		Used bits in last byte (other bits should be 0)
//							(e.g. if this is 6, then the bits used (x) in the last byte are: xxxxxx00, where MSb is the leftmost bit, LSb is the rightmost bit)
//	0x05	-	WORD		Pause after this block (ms.)
//	0x07	N	BYTE[3] 	Length of data that follow
//	0x0A	-	BYTE[N]		Data as in .TAP files

Block14::Block14(WORD bit0len, WORD bit1len, BYTE rbits, WORD pause, char *data, size_t size)
{
	init(B14_PURE_DATA, 0x0B, NULL, 10 + size);
	putWord(bit0len);
	putWord(bit1len);
	putByte(rbits);
	putWord(pause);
	putWord24(size);
	put(data, size);
}

Block14::Block14(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 10);
	aux->WriteRawData(is, 10);
	aux->Seek(7);
	int len = aux->ReadUInt24();

	init(B14_PURE_DATA, 0x0B, NULL, 10 + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block14::toString()
{
	return Block::toString() + " - Pure data block";
}

// ============================================================================================
// Block #15 - Direct recording block
//	0x00	-	WORD 		Number of T-states per sample (bit of data)
//	0x02	-	WORD 		Pause after this block in milliseconds (ms.)
//	0x04	-	BYTE 		Used bits (samples) in last byte of data (1-8)
//							(e.g. if this is 2, only first two samples of the last byte will be played)
//	0x05	N	BYTE[3]		Length of samples' data
//	0x08	-	BYTE[N]		Samples data. Each bit represents a state on the EAR port (i.e. one sample).
//							MSb is played first.

Block15::Block15(WORD numstates, WORD pause, BYTE rbits, char *data, size_t size)
{
	init(B15_DIRECT_REC, 0x09, NULL, 8 + size);
	putWord(numstates);
	putWord(pause);
	putByte(rbits);
	putWord24(size);
	put(data, size);
}

Block15::Block15(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 10);
	aux->WriteRawData(is, 8);
	aux->Seek(5);
	int len = aux->ReadUInt24();

	init(B15_DIRECT_REC, 0x09, NULL, 8 + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block15::toString()
{
	bytes->Seek(1);
	uint16_t tstates = bytes->ReadUInt16();
	uint16_t pause = bytes->ReadUInt16();
	bytes->ReadUByte();
	uint32_t samples = bytes->ReadUInt24();

	std::stringstream stream;
	stream << Block::toString() << " - Direct recording block";
	stream << "/tSample length: " << std::dec << tstates << " T-states" << endl <<
			  "/tNum. Samples:  " << std::dec << samples << " samples" << endl <<
			  "/tPause:         " << std::dec << pause << "ms" << endl;

	return stream.str();
}

// ============================================================================================
// Block #20 - Pause (silence) or 'Stop the tape' command
//	0x00	-	WORD		Pause duration (ms.)

Block20::Block20(WORD pause)
{
	init(B20_SILENCE_BLOCK, 0x03, NULL, 2);
	putWord(pause);
}

Block20::Block20(istream &is)
{
	init(B20_SILENCE_BLOCK, 0x03, NULL, 2);
	put(is, 2);
}

string Block20::toString()
{
	std::stringstream stream;
	stream << Block::toString() << " - Pause (silence) or 'Stop the tape' command";
	bytes->Seek(1);
	stream << " (" << std::dec << (bytes->ReadUInt16()) << "ms)";

	return stream.str();
}

// ============================================================================================
// Block #21 - Group start
//	0x00	L	BYTE		Length of the group name string
//	0x01	-	CHAR[L]		Group name in ASCII format (please keep it under 30 characters long)

Block21::Block21(string namegroup)
{
	init(B21_GRP_START, 0x02, NULL, sizeof(BYTE) + namegroup.length());
	putByte(namegroup.length());
	putString(namegroup);
}

Block21::Block21(istream &is)
{
	BYTE len = is.get();

	init(B21_GRP_START, 0x02, NULL, sizeof(BYTE) + len);
	putByte(len);

	put(is, len);
}

string Block21::toString()
{
	return Block::toString() + " - Group start";
}

// ============================================================================================
// Block #22 - Group end

Block22::Block22()
{
	init(B22_GRP_END, 0x01, NULL, 0);
}

Block22::Block22(istream &is)
{
	init(B22_GRP_END, 0x01, NULL, 0);
}

string Block22::toString()
{
	return Block::toString() + " - Group end";
}

// ============================================================================================
// Block #23 - Jump to block
//	0x00	-	WORD		Relative jump value

Block23::Block23(WORD jump)
{
	init(B23_JUMP_BLOCK, 0x03, NULL, 2);
	putWord(jump);
}

Block23::Block23(istream &is)
{
	init(B23_JUMP_BLOCK, 0x03, NULL, 2);
	put(is, 2);
}

string Block23::toString()
{
	return Block::toString() + " - Jump to block";
}

// ============================================================================================
// Block #24 - Loop start
//	0x00	-	WORD		Number of repetitions (greater than 1)

Block24::Block24(WORD numRepeats)
{
	init(B24_LOOP_START, 0x03, NULL, 2);
	putWord(numRepeats);
}

Block24::Block24(istream &is)
{
	init(B24_LOOP_START, 0x03, NULL, 2);
	put(is, 2);
}

string Block24::toString()
{
	return Block::toString() + " - Loop start";
}

// ============================================================================================
// Block #25 - Loop end

Block25::Block25()
{
	init(B25_LOOP_END, 0x01, NULL, 0);
}

Block25::Block25(istream &is)
{
	init(B25_LOOP_END, 0x01, NULL, 0);
}

string Block25::toString()
{
	return Block::toString() + " - Loop end";
}

// ============================================================================================
// Block #26 - Call sequence
//	0x00	N	WORD		Number of calls to be made
//	0x02	-	WORD[N]		Array of call block numbers (relative-signed offsets)

Block26::Block26(WORD num, WORD *calls)
{
	init(B26_CALL_SEQ, 0x03, NULL, 2 + num * sizeof(WORD));
	putWord(num);
	put((char*)calls, num * sizeof(WORD));
}

Block26::Block26(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, sizeof(WORD));
	aux->WriteRawData(is, sizeof(WORD));
	aux->Seek(0);
	int len = aux->ReadUInt16() * sizeof(WORD);

	init(B26_CALL_SEQ, 0x03, NULL, 2 + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block26::toString()
{
	return Block::toString() + " - Call sequence";
}

// ============================================================================================
// Block #27 - Return from sequence

Block27::Block27()
{
	init(B27_RET_SEQ, 0x01, NULL, 0);
}

Block27::Block27(istream &is)
{
	init(B27_RET_SEQ, 0x01, NULL, 0);
}

string Block27::toString()
{
	return Block::toString() + " - Return from sequence";
}

// ============================================================================================
// Block #28 - Select block
//	0x00	-	WORD		Length of the whole block (without these two bytes)
//	0x02	N	BYTE		Number of selections
//	0x03	-	SELECT[N]	List of selections
//
//	SELECT structure format
//	0x00	-	WORD		Relative Offset
//	0x02	L	BYTE		Length of description text
//	0x03	-	CHAR[L]		Description text (please use single line and max. 30 chars)

Block28::Block28(BYTE num, WORD *offsets, string *texts)
{
	int len = 1 + 3 * num;
	for (int i=0; i<num; i++) len += texts[i].length();

	init(B28_SEL_BLOCK, 0x04, NULL, 2 + len);
	putWord(len);
	putByte(num);
	
	for (int i=0; i<num; i++) {
		putWord(offsets[i]);
		putByte(texts[i].length());
		putString(texts[i]);
	}
}

Block28::Block28(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 2);
	aux->WriteRawData(is, 2);
	aux->Seek(0);
	int len = aux->ReadUInt16();

	init(B28_SEL_BLOCK, 0x04, NULL, 2 + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block28::toString()
{
	return Block::toString() + " - Select block";
}

// ============================================================================================
// Block #2A - Stop the tape if in 48K mode
//	0x00	0	DWORD		Length of the block without these four bytes (0)

Block2A::Block2A()
{
	init(B2A_STOP48K, 0x03, NULL, 2);
	putWord(0);
}

Block2A::Block2A(istream &is)
{
	init(B2A_STOP48K, 0x03, NULL, 2);
	put(is, 2);
}

string Block2A::toString()
{
	return Block::toString() + " - Stop the tape if in 48K mode";
}

// ============================================================================================
// Block #2B - Set signal level
//	0x00	1	DWORD		Block length (without these four bytes)
//	0x04	-	BYTE		Signal level (0=low, 1=high)

Block2B::Block2B()
{
	init(B2B_SIGNAL_LEVEL, 0x05, NULL, 5);
	putWord(1);
}

Block2B::Block2B(istream &is)
{
	init(B2B_SIGNAL_LEVEL, 0x05, NULL, 5);
	put(is, 2);
}

string Block2B::toString()
{
	return Block::toString() + " - Set signal level";
}

// ============================================================================================
// Block #30 - Text description
//	0x00	N	BYTE		Length of the text description
//	0x01	-	CHAR[N]		Text description in ASCII format

Block30::Block30(string text)
{
	init(B30_TEXT_DESCRIP, 0x02, NULL, 1 + text.length());
	putByte(text.length());
	putString(text);
}

Block30::Block30(istream &is)
{
	BYTE len = is.get();

	init(B30_TEXT_DESCRIP, 0x02, NULL, 1 + len);
	putByte(len);

	put(is, len);
}

string Block30::toString()
{
	bytes->Seek(1);
	BYTE len = bytes->ReadUByte();
	string text = bytes->ReadString(len);

	std::stringstream stream;
	stream << Block::toString() << " - Text description \"" << text << "\"";

	return stream.str();
}

// ============================================================================================
// Block #31 - Message block
//	0x00	-	BYTE		Time (in seconds) for which the message should be displayed
//	0x01	N	BYTE		Length of the text message
//	0x02	-	CHAR[N]		Message that should be displayed in ASCII format

Block31::Block31(BYTE time, string text)
{
	init(B31_MSG_BLOCK, 0x03, NULL, 2 + text.length());
	putByte(time);
	putByte(text.length());
	putString(text);
}

Block31::Block31(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 2);
	aux->WriteRawData(is, 2);
	aux->Seek(1);
	int len = aux->ReadUByte();

	init(B31_MSG_BLOCK, 0x03, NULL, 2 + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block31::toString()
{
	bytes->Seek(1);
	int time = bytes->ReadUByte();
	int len = bytes->ReadUByte();
	string text = bytes->ReadString(len);
	
	std::stringstream stream;
	stream << Block::toString() << " - Message block \"" << text << "\" (" << time << " sec.)";

	return stream.str();
}

// ============================================================================================
// Block #32 - Archive info
//	0x00	-	WORD		Length of the whole block (without these two bytes)
//	0x02	N	BYTE		Number of text strings
//	0x03	-	TEXT[N]		List of text strings
//
//	TEXT structure format
//	0x00	-	BYTE	Text identification byte:
//				00 - Full title
//				01 - Software house/publisher
//				02 - Author(s)
//				03 - Year of publication
//				04 - Language
//				05 - Game/utility type
//				06 - Price
//				07 - Protection scheme/loader
//				08 - Origin
//				FF - Comment(s)
//	0x01	L	BYTE	Length of text string
//	0x02	-	CHAR[L]	Text string in ASCII format

Block32::Block32(BYTE num, BYTE *ids, string *texts)
{
	int len = 1 + 2 * num;
	for (int i=0; i<num; i++) len += texts[i].length();
	
	init(B32_ARCHIVE_INFO, 0x04, NULL, 2 + len);
	putWord(len);
	putByte(num);
	for (int i=0; i<num; i++) {
		putByte(ids[i]);
		putByte(texts[i].length());
		putString(texts[i]);
	}
}

Block32::Block32(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 2);
	aux->WriteRawData(is, 2);
	aux->Seek(0);
	int len = aux->ReadUInt16();

	init(B32_ARCHIVE_INFO, 0x04, NULL, 2 + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block32::toString()
{
	bytes->Seek(3);
	BYTE num = bytes->ReadUByte();

	std::stringstream stream;
	stream << Block::toString() << " - Archive info";
	string idname, name;
	WORD id, nameLen;
	
	bytes->Seek(4);
	for (int i=0; i<num; i++) {
		id = bytes->ReadUByte();
		switch (id) {
			case 0x00: idname = "Full title:   "; break;
			case 0x01: idname = "Publisher:    "; break;
			case 0x02: idname = "Author(s):    "; break;
			case 0x03: idname = "Release year: "; break;
			case 0x04: idname = "Language:     "; break;
			case 0x05: idname = "Program type: "; break;
			case 0x06: idname = "Price:        "; break;
			case 0x07: idname = "Prot/loader:  "; break;
			case 0x08: idname = "Origin:       "; break;
			case 0xff: idname = "Comments:     "; break;
			default:   idname = "UNKNOWN:      "; break;
		}
		nameLen = bytes->ReadUByte();
		name = bytes->ReadString(nameLen);
		stream << endl << "\t\t" << idname << name;
	}
	return stream.str();
}

// ============================================================================================
// Block #33 - Hardware type
//	0x00	N	BYTE	Number of machines and hardware types for which info is supplied
//	0x01	-	HWINFO[N]	List of machines and hardware
//
//	HWINFO structure format
//	0x00	-	BYTE	Hardware type
//	0x01	-	BYTE	Hardware ID
//	0x02	-	BYTE	Hardware information:
//				00 - The tape RUNS on this machine or with this hardware, but may or may not use the hardware or special features of the machine.
//				01 - The tape USES the hardware or special features of the machine, such as extra memory or a sound chip.
//				02 - The tape RUNS but it DOESN'T use the hardware or special features of the machine.
//				03 - The tape DOESN'T RUN on this machine or with this hardware.

Block33::Block33(BYTE num, BYTE *hwtype, BYTE *hwid, BYTE *hwinfo)
{
	init(B33_HARDWARE_TYPE, 0x02, NULL, 1 + num * 3);
	putByte(num);
	for (int i=0; i<num; i++) {
		putByte(hwtype[i]);
		putByte(hwid[i]);
		putByte(hwinfo[i]);
	}
}

Block33::Block33(istream &is)
{
	BYTE num = is.get();
	int len = num * 3;

	init(B33_HARDWARE_TYPE, 0x02, NULL, 1 + len);
	putByte(num);

	put(is, len);
}

string Block33::toString()
{
	return Block::toString() + " - Hardware type";
}

// ============================================================================================
// Block #35 - Custom info block
//	0x00	-	CHAR[16]	Identification string (in ASCII)
//	0x10	L	DWORD		Length of the custom info
//	0x14	-	BYTE[L]		Custom info

Block35::Block35(string label, string info)
{
	init(B35_CUSTOM_INFO, 0x15, NULL, 16 + sizeof(DWORD) + info.length());
	putString((label+"          ").substr(0, 16));
	putDWord(info.length());
	putString(info);
}

Block35::Block35(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 16 + sizeof(DWORD));
	aux->WriteRawData(is, 16 + sizeof(DWORD));
	aux->Seek(16);
	int len = aux->ReadUInt32();

	init(B35_CUSTOM_INFO, 0x15, NULL, 16 + sizeof(DWORD) + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block35::toString()
{
	bytes->Seek(1);
	string label = bytes->ReadString(16);
	DWORD len = bytes->ReadUInt32();
	string info = bytes->ReadString(len);
	return Block::toString() + " - Custom info block" + 
		"\n\t\tKey:   " + label +
		"\n\t\tValue: " + info;
}

// ============================================================================================
// Block #4B - Kansas City Standard
//	0x00   N+12	DWORD		Block length without these four bytes (extension rule)
//	0x04	-	WORD		Pause after this block in milliseconds
//	0x06	-	WORD		Duration of a PILOT pulse in T-states {same as ONE pulse}
//	0x08	-	WORD		Number of pulses in the PILOT tone
//	0x0A	-	WORD		Duration of a ZERO pulse in T-states
//	0x0C	-	WORD		Duration of a ONE pulse in T-states
//	0x0E	-	BYTE		Bits 7-4: Number of ZERO pulses in a ZERO bit (0-16) {4}
//			 (bitmapped)	Bits 3-0: Number of ONE pulses in a ONE bit (0-16) {8}
//	0x0F	-	BYTE		Bits 7-6: Numbers of leading bits {1}
//			 (bitmapped)	Bit 5: Value of leading bits {0}
//							Bits 4-3: Number of trailing bits {2}
//							Bit 2: Value of trailing bits {1}
//							Bit 1: Reserved
//							Bit 0: Endianless (0 for LSb first, 1 for MSb first) {0}
//	0x10	-	BYTE[N]		Data stream

#define MSX_BINARY_ID    0xd0
#define MSX_BASIC_ID     0xd3
#define MSX_ASCII_ID     0xea
#define MSX_UNKNOWN_ID   0xff

#define MSX_BINARY_DESC "MSX BINARY Header"
#define MSX_BASIC_DESC  "MSX BASIC Header";
#define MSX_ASCII_DESC  "MSX ASCII Header";
#define MSX_DATA_DESC   "MSX DATA Block";

#define MSX_BINARY_LOAD "BLOAD\"CAS:\",R"
#define MSX_BASIC_LOAD  "CLOAD + RUN"
#define MSX_ASCII_LOAD  "RUN\"CAS:\""

Block4B::Block4B(WORD pause, WORD pilot, WORD pulses, WORD bit0len, WORD bit1len, BYTE bitcfg, BYTE bytecfg, char *data, size_t size)
{
	init(B4B_MSX_KCS, 0x11, NULL, 16 + size);
	putDWord(12 + size);
	putWord(pause);
	putWord(pilot);
	putWord(pulses);
	putWord(bit0len);
	putWord(bit1len);
	putByte(bitcfg);
	putByte(bytecfg);
	put(data, size);
}

Block4B::Block4B(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, 4);
	aux->WriteRawData(is, 4);
	aux->Seek(0);
	int len = aux->ReadUInt32();

	init(B4B_MSX_KCS, 0x11, NULL, 4 + len);
	put(aux);
	delete aux;

	put(is, len);
}

BYTE Block4B::getFileType()
{
	bytes->Seek(17);
	BYTE token = bytes->ReadUByte();
	for (int i=0; i<9; i++) {
		if (token != bytes->ReadUByte()) {
			return MSX_UNKNOWN_ID;
		}
	}
	if (token == MSX_BINARY_ID) return MSX_BINARY_ID;
	if (token == MSX_BASIC_ID) return MSX_BASIC_ID;
	if (token == MSX_ASCII_ID) return MSX_ASCII_ID;
	return MSX_UNKNOWN_ID;
}

string Block4B::getFileTypeLoad()
{
	switch (getFileType()) {
		case MSX_BINARY_ID:	//BLOAD
			return MSX_BINARY_LOAD;
		case MSX_BASIC_ID:	//CLOAD
			return MSX_BASIC_LOAD;
		case MSX_ASCII_ID:	//LOAD ASCII
			return MSX_ASCII_LOAD;
	}
	return NULL;
}

string Block4B::getFileTypeDescription()
{
	switch (getFileType()) {
		case MSX_BINARY_ID:	//BLOAD
			return MSX_BINARY_DESC;
		case MSX_BASIC_ID:	//CLOAD 
			return MSX_BASIC_DESC;
		case MSX_ASCII_ID:	//LOAD ASCII
			return MSX_ASCII_DESC;
	}
	return MSX_DATA_DESC;
}

string Block4B::toString()
{
	return toString(false);
}

string Block4B::toString(bool isBinaryBlock)
{
	std::stringstream stream;
	
	bytes->Seek(5);
	WORD pause = bytes->ReadUInt16();
	
	stream << Block::toString() << " - Kansas City Standard" << " - DataLen: " << (bytes->getSize()-17) << " bytes" << endl <<
		"\t\t" << getFileTypeDescription();
	if (getFileType()==0xff) {
		//Bloque Data
		if (isBinaryBlock) {
			bytes->Seek(17);
			WORD begin = bytes->ReadUInt16();
			WORD end = bytes->ReadUInt16();
			WORD start = bytes->ReadUInt16();
			stream << " [Begin: 0x" << std::setfill ('0') << std::setw(4) << std::hex << begin <<
					  " End: 0x" << std::setfill ('0') << std::setw(4) << std::hex << end <<
					  " Start: 0x" << std::setfill ('0') << std::setw(4) << std::hex << start << "]";
		}
	} else {
		//Bloque Header
		bytes->Seek(27);
		stream << endl << "\t\tFound:" << bytes->ReadString(6);
	}
	stream << endl << "\t\tPause " << std::dec << pause << "ms";
	return stream.str();
}

// ============================================================================================
// Block #5A - "Glue" block (90 dec, ASCII Letter 'Z')
//	0x00	-	BYTE[9]		Value: { "XTape!",0x1A,MajR,MinR }
//							Just skip these 9 bytes and you will end up on the next ID.

Block5A::Block5A()
{
	const char buff[9] = { 'X','T','a','p','e','!',0x1A,TZX_VER_HI,TZX_VER_LO };
	init(B5A_GLUE_BLOCK, 0x0A, (char*)buff, 9);
}

Block5A::Block5A(istream &is)
{
	init(B5A_GLUE_BLOCK, 0x0A, NULL, 9);
	put(is, 9);
}

string Block5A::toString()
{
	return Block::toString() + " - \"Glue\" block";
}


// =================================
// NOT IMPLEMENTED BLOCKS
// =================================


// ============================================================================================
// Block #18 - CSW recording block
//	0x00	10+N	DWORD	Block length (without these four bytes)
//	0x04	-		WORD	Pause after this block (in ms).
//	0x06	-		BYTE[3]	Sampling rate
//	0x09	-		BYTE	Compression type
//					  0x01: RLE
//					  0x02: Z-RLE
//	0x0A	-		DWORD	Number of stored pulses (after decompression, for validation purposes)
//	0x0E	-		BYTE[N]	CSW data, encoded according to the CSW file format specification.

Block18::Block18()
{
	cout << "Block #18 - CSW recording block [CONSTRUCTOR NOT YET IMPLEMENTED]";
}

Block18::Block18(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, sizeof(DWORD));
	aux->WriteRawData(is, sizeof(DWORD));
	aux->Seek(0);
	int len = aux->ReadUInt32();

	init(B18_CSW_REC, 0x13, NULL, sizeof(DWORD) + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block18::toString()
{
	return Block::toString() + " - CSW recording block";
}

// ============================================================================================
// Block #19 - Generalized data block
//	0x00	 -		DWORD		Block length (without these four bytes)
//	0x04	 -		WORD 		Pause after this block (ms)
//	0x06	TOTP	DWORD		Total number of symbols in pilot/sync block (can be 0)
//	0x0A	NPP		BYTE		Maximum number of pulses per pilot/sync symbol
//	0x0B	ASP		BYTE		Number of pilot/sync symbols in the alphabet table (0=256)
//	0x0C	TOTD	DWORD		Total number of symbols in data stream (can be 0)
//	0x10	NPD		BYTE		Maximum number of pulses per data symbol
//	0x11	ASD		BYTE		Number of data symbols in the alphabet table (0=256)
//	0x12	 -	 SYMDEF[ASP]	Pilot and sync symbols definition table
//								This field is present only if TOTP>0
//	0x12+		 PRLE[TOTP]		Pilot and sync data stream
//	 (2*NPP+1)*ASP				This field is present only if TOTP>0
//	0x12+		 SYMDEF[ASD] 	Data symbols definition table
//	 (TOTP>0)*((2*NPP+1)*ASP)+	This field is present only if TOTD>0
//	 TOTP*3
//	0x12+		  BYTE[DS]		Data stream
//	 (TOTP>0)*((2*NPP+1)*ASP)+	This field is present only if TOTD>0
//	 TOTP*3+
//	 (2*NPD+1)*ASD

Block19::Block19()
{
	cout << "Block #19 - Generalized data block [CONSTRUCTOR NOT YET IMPLEMENTED]";
}

Block19::Block19(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, sizeof(DWORD));
	aux->WriteRawData(is, sizeof(DWORD));
	aux->Seek(0);
	int len = aux->ReadUInt32();

	init(B19_GEN_DATA, 0x13, NULL, sizeof(DWORD) + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block19::toString()
{
	return Block::toString() + " - Generalized data block";
}


//=================================
// DEPRECATED BLOCKS
//=================================


// ============================================================================================
// Block #16 - C64 ROM type data block

Block16::Block16()
{
	cout << "Block #16 - C64 ROM type data block [DEPRECATED]";
}

Block16::Block16(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, sizeof(DWORD));
	aux->WriteRawData(is, sizeof(DWORD));
	aux->Seek(0);
	int len = aux->ReadUInt32();

	init(B16_C64ROM, 0x29, NULL, sizeof(DWORD) + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block16::toString()
{
	return Block::toString() + " - C64 ROM type data block [DEPRECATED]";
}

// ============================================================================================
// Block #17 - C64 turbo tape data block

Block17::Block17()
{
	cout << "Block #17 - C64 Turbo Tape Data Block [DEPRECATED]";
}

Block17::Block17(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, sizeof(DWORD));
	aux->WriteRawData(is, sizeof(DWORD));
	aux->Seek(0);
	int len = aux->ReadUInt32();

	init(B17_C64TURBO, 0x17, NULL, sizeof(DWORD) + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block17::toString()
{
	return Block::toString() + " - C64 Turbo Tape Data Block [DEPRECATED]";
}

// ============================================================================================
// Block #34 - Emulation info

Block34::Block34()
{
	cout << "Block #34 - Emulation info Block [DEPRECATED]";
}

Block34::Block34(istream &is)
{
	init(B34_EMUINFO, 0x08, NULL, 8);
	put(is, 8);
}

string Block34::toString()
{
	return Block::toString() + " - Emulation info block [DEPRECATED]";
}

// ============================================================================================
// Block #40 - Snapshot block

Block40::Block40()
{
	cout << "Block #40 - Snapshot block [DEPRECATED]";
}

Block40::Block40(istream &is)
{
	ByteBuffer *aux = new ByteBuffer(NULL, sizeof(WORD24) + 1);
	aux->WriteRawData(is, sizeof(WORD24) + 1);
	aux->Seek(1);
	int len = aux->ReadUInt24();

	init(B40_SNAPSHOT, 0x05, NULL, sizeof(WORD24) + 1 + len);
	put(aux);
	delete aux;

	put(is, len);
}

string Block40::toString()
{
	return Block::toString() + " - Snapshot block [DEPRECATED]";
}


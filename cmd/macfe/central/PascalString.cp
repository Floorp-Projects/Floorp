//----------------------------------------------------------------------------------------
// PascalString.cp 
// Copyright © 1985-1993 by Apple Computer, Inc.  All rights reserved.
//----------------------------------------------------------------------------------------

#include "PascalString.h"
// #include "xpassert.h"

#ifndef __STDIO__
#include <stdio.h>
#endif

#ifndef __STRING__
#include <string.h>
#endif

#pragma segment Main

//========================================================================================
// CLASS CString
//========================================================================================


//----------------------------------------------------------------------------------------
// CString::InsertHelper(CString):
//----------------------------------------------------------------------------------------

void CString::InsertHelper(const CString& insStr,
						  short pos,
						  short maxLength)
{
	if (pos > Length() + 1)
	{
#if qDebugMsg
		fprintf(stderr, "###CString::InsertHelper: Insert position greater than length of CString.\n");
#endif
		if (Length() < maxLength)	
			pos = Length() + 1;
	}
	
#if qDebugMsg
	if (Length() + insStr.Length() > maxLength)
		fprintf(stderr, "### CString::InsertHelper: CString truncated during insert call.\n");	
#endif

	short usableLengthOfInsertString;
	short endPosOfInsertString;
	short usableLengthOfShiftedString;
	
	if (pos + insStr.Length() > maxLength)
		usableLengthOfInsertString = maxLength - pos + 1;
	else
		usableLengthOfInsertString = insStr.Length();
	endPosOfInsertString = pos + usableLengthOfInsertString - 1;
	
	if ((endPosOfInsertString + 1) + (Length() - pos + 1) > maxLength)
		usableLengthOfShiftedString = maxLength - endPosOfInsertString;
	else
		usableLengthOfShiftedString = Length() - pos + 1;
		
	memmove(&fStr[endPosOfInsertString + 1], &fStr[pos], usableLengthOfShiftedString);
	memmove(&fStr[pos], &insStr.fStr[1], usableLengthOfInsertString);
	Length() = usableLengthOfShiftedString + endPosOfInsertString;
} // CString::InsertHelper(CString)


//----------------------------------------------------------------------------------------
// CString::InsertHelper(char*):
//----------------------------------------------------------------------------------------

void CString::InsertHelper(const char* insStr,
						  short pos,
						  short maxLength)
{
	this->InsertHelper(CStr255(insStr), pos, maxLength);
} // CString::InsertHelper(char*)



//----------------------------------------------------------------------------------------
// CString::operator char*:
//----------------------------------------------------------------------------------------

CString::operator char*() const
{
	static short currentCString = 0;
	static char cStrings[kTempCStrings][kStr255Len+1];
	
	currentCString = (currentCString + 1) % kTempCStrings;
	
	strncpy(cStrings[currentCString], (char *) &fStr[1], Length());
	cStrings[currentCString][Length()] = '\0';
	
	return cStrings[currentCString];
} // CString::operator char*


//----------------------------------------------------------------------------------------
// CString::operator long:
//----------------------------------------------------------------------------------------

CString::operator long() const
{
	// The following statement looks like it should work. Right?
	//
	//	return *((long *) &fStr[1]);
	//
	// Wrong, the C compiler generates a MOVE.L starting on a odd byte boundary for the
    // preceding statement. This is illegal on the 68000. But its _NOT_ a bug, because
    // according to the ANSI C reference manual, "A pointer to one type may be converted
    // to a pointer to another type. The resulting pointer may cause an addressing
    // exception if the subject pointer does not refer to an object suitably aligned in
    // storage".
	
	long returnLong;
	
	memcpy(&returnLong, &fStr[1], sizeof(long));
	return returnLong;
} // CString::operator long


//----------------------------------------------------------------------------------------
// CString::Pos(char*):
//----------------------------------------------------------------------------------------

unsigned char CString::Pos(const char* subStr, unsigned char startPos)
{
	char cStr[kStr255Len + 1];
	char* ptr;
	
	memcpy(cStr, &fStr[1], (size_t)Length());
	cStr[Length()] = 0;
	ptr = strstr(&cStr[startPos - 1], subStr);
	return ptr != NULL ? (ptr - cStr) + 1 : 0;
} // CString::Pos(char*)


//----------------------------------------------------------------------------------------
// CString::Pos(CString):
//----------------------------------------------------------------------------------------

unsigned char CString::Pos(const CString& subStr, unsigned char startPos)
{
	char cStr[kStr255Len + 1];

	memcpy(cStr, &subStr.fStr[1], (size_t)(subStr.Length()));
	cStr[subStr.Length()] = 0;
	return this->Pos(cStr, startPos);
} // CString::Pos(CString)


//----------------------------------------------------------------------------------------
// CString::operator const unsigned char*
//----------------------------------------------------------------------------------------
CString::operator const unsigned char*() const
{
	return (const unsigned char *) this;
} // CString::operator const unsigned char*

//----------------------------------------------------------------------------------------
// CString::Delete
//----------------------------------------------------------------------------------------
void CString::Delete(short pos, short length)
{
	if ((pos > 0) && (length > 0) && (pos <= Length()))	// should also check that pos <= kMaxLength
	{
		if (pos + length > Length())
			fStr[0] = pos - 1;
		else
		{
			::memcpy(&fStr[pos], &fStr[pos + length], Length() - (pos + length) + kLengthByte);
			fStr[0] -= length;
		}
	}
} // CString::Delete

//========================================================================================
// CLASS CStr255
//========================================================================================

const CStr255 CStr255::sEmptyString("\p");

//----------------------------------------------------------------------------------------
// CStr255::CStr255(char*):
//----------------------------------------------------------------------------------------

CStr255::CStr255(const char* str)
{
	// Truncate the C CString to 255 bytes if necessary.
	size_t len = str == NULL ? 0 : strlen(str);
	Length() = len > kStr255Len ?  kStr255Len : len;
	
	if (Length() > kStr255Len)
		Length() = kStr255Len;
	memcpy(&fStr[1], str, (size_t)Length());
} // CStr255::CStr255(char*)


//----------------------------------------------------------------------------------------
// CStr255::CStr255(long): Useful for converting OSType's into CStr255's.
//----------------------------------------------------------------------------------------

CStr255::CStr255(const long id)
{
	Length() = 4;
	memcpy(&fStr[1], &id, (size_t)Length());
} // CStr255::CStr255(long)


//----------------------------------------------------------------------------------------
// CStr255::Copy:
//----------------------------------------------------------------------------------------

CStr255 CStr255::Copy(short pos, short length)
{
	CStr255 newString;
	
	length = length > Length() - pos + kLengthByte ? Length() - pos + kLengthByte : length;
	
	if (length > 0)
	{
		memcpy(&newString.fStr[1], &fStr[pos], (size_t)length);
		newString.Length() = length;
	}
	else
		newString = "";
		
	return newString;
	
} // CStr255::Copy


//----------------------------------------------------------------------------------------
// CStr255::operator+:
//----------------------------------------------------------------------------------------

CStr255 operator+(const CString& s1,
                  const char* s2)
{
	CStr255 newStr;
	short s2Len = s2 == NULL ? 0 : (short)(strlen((const char *) s2));

	if (s1.Length() + s2Len > kStr255Len)
		newStr.Length() = kStr255Len;
	else
		newStr.Length() = s1.Length() + s2Len;
		
	memcpy(&newStr.fStr[1], &s1.fStr[1], (size_t)(s1.Length()));
	memcpy(&newStr.fStr[s1.Length() + kLengthByte], s2, newStr.Length() - s1.Length());

	return newStr;
} // CStr255::operator+


//----------------------------------------------------------------------------------------
// CStr255::operator+(char*,CString):
//----------------------------------------------------------------------------------------

CStr255 operator+(const char* s1,
                  const CString& s2)
{
	CStr255 newStr;
	short s1Len = s1 == NULL ? 0 : (short)(strlen(s1));

	if (s1Len + s2.Length() > kStr255Len)
		newStr.Length() = kStr255Len;
	else
		newStr.Length() = s1Len + s2.Length();
		
	memcpy(&newStr.fStr[1], s1, (size_t)s1Len);
	memcpy(&newStr.fStr[s1Len + kLengthByte], s2.fStr + 1, newStr.Length() - s1Len);

	return newStr;
} // CStr255::operator+(char*,CString)


//----------------------------------------------------------------------------------------
// CStr255::operator+(CString,CString):
//----------------------------------------------------------------------------------------

CStr255 operator+(const CString& s1,
                  const CString& s2)
{
	CStr255 newStr;

	if (s1.Length() + s2.Length() > kStr255Len)
		newStr.Length() = kStr255Len;
	else
		newStr.Length() = s1.Length() + s2.Length();
		
	memcpy(&newStr.fStr[1], &s1.fStr[1], (size_t)(s1.Length()));
	memcpy(&newStr.fStr[s1.Length() + kLengthByte], s2.fStr + 1, newStr.Length() - s1.Length());

	return newStr;
} // CStr255::operator+(CString,CString)


//----------------------------------------------------------------------------------------
// CStr255::operator +=(CString):  Concatenate a string
//----------------------------------------------------------------------------------------

CStr255& CStr255::operator += (const CString& str)
{
	InsertHelper (str, Length() + 1, kStr255Len);
	return *this;
} // CStr255::operator +=(CString)


//----------------------------------------------------------------------------------------
// CStr255::operator +=(char*):  Concatenate a string
//----------------------------------------------------------------------------------------

CStr255& CStr255::operator += (const char* str)
{
	InsertHelper (str, Length() + 1, kStr255Len);
	return *this;
} // CStr255::operator +=(char*)


//----------------------------------------------------------------------------------------
// CStr255::operator +=(char):  Concatenate a single character
//----------------------------------------------------------------------------------------

CStr255& CStr255::operator += (const char ch)
{
	if (++Length() <= kStr255Len)
		fStr[Length()] = ch;
	else
	{
		--Length();
#if qDebugMsg
		fprintf(stderr, "###CStr255::operator+=: Concatenation produces CStr255 overflow.\n");
#endif
	}
	
	return *this;
} // CStr255::operator +=(char)


//----------------------------------------------------------------------------------------
// CStr255::operator =:
//----------------------------------------------------------------------------------------

CStr255& CStr255::operator = (const char* str)
{
	if (str)
	{
		// Truncate the C CString to 255 bytes if necessary.
		register size_t itsSize = strlen(str);
		if (itsSize > kStr255Len)
			Length() = kStr255Len;
		else
			Length() = (unsigned char)(itsSize);

		memcpy(&fStr[1], str, (size_t)Length());
	}
	else
		Length() = 0;
	
	return *this;
} // CStr255::operator =



//========================================================================================
// CLASS CStr63
//========================================================================================


//----------------------------------------------------------------------------------------
// CStr63::CStr63(char*):
//----------------------------------------------------------------------------------------

CStr63::CStr63(const char* str)
{
	// Truncate the C CString to 63 bytes if necessary.

	Length() = str == NULL ? 0 : (unsigned char)(strlen(str));
	if (Length() > kStr63Len)
		Length() = kStr63Len;
	memcpy(&fStr[1], str, (size_t)Length());
} // CStr63::CStr63(char*)


//----------------------------------------------------------------------------------------
// CStr63::CStr63(long):
//----------------------------------------------------------------------------------------

CStr63::CStr63(const long id)
{
	Length() = 4;
	memcpy(&fStr[1], &id, (size_t)Length());
} // CStr63::CStr63(long)


//----------------------------------------------------------------------------------------
// CStr63::Copy:
//----------------------------------------------------------------------------------------

CStr63 CStr63::Copy(short pos, short length)
{
	CStr63 newString;
	
	length = length > Length() - pos + kLengthByte ? Length() - pos + kLengthByte : length;
	
	if (length > 0)
	{
		memcpy(&newString.fStr[1], &fStr[pos], (size_t)length);
		newString.Length() = length;
	}
	else
		newString = "";
		
	return newString;
	
} // CStr63::Copy


//----------------------------------------------------------------------------------------
// CStr63::operator +=(CString):  Concatenate a string
//----------------------------------------------------------------------------------------

CStr63& CStr63::operator += (const CString& str)
{
	InsertHelper (str, Length() + 1, kStr63Len);
	return *this;
} // CStr63::operator +=(CString)


//----------------------------------------------------------------------------------------
// CStr63::operator +=(char*):  Concatenate a string
//----------------------------------------------------------------------------------------

CStr63& CStr63::operator += (const char* str)
{
	InsertHelper (str, Length() + 1, kStr63Len);
	return *this;
} // CStr63::operator +=(char*)


//----------------------------------------------------------------------------------------
// CStr63::operator +=(char):  Concatenate a single character
//----------------------------------------------------------------------------------------

CStr63& CStr63::operator += (const char ch)
{
	if (++Length() <= kStr63Len)
		fStr[Length()] = ch;
	else
	{
		--Length();
#if qDebugMsg
		fprintf(stderr, "###CStr63::operator+=: Concatenation produces CStr63 overflow.\n");
#endif
	}
	
	return *this;
} // CStr63::operator +=(char)



//========================================================================================
// CLASS CStr32
//========================================================================================


//----------------------------------------------------------------------------------------
// CStr32::CStr32(char*):
//----------------------------------------------------------------------------------------

CStr32::CStr32(const char* str)
{
	// Truncate the C CString to 32 bytes if necessary.

	Length() = str == NULL ? 0 : (unsigned char)(strlen(str));
	if (Length() > kStr32Len)
		Length() = kStr32Len;
	memcpy(&fStr[1], str, (size_t)Length());
} // CStr32::CStr32(char*)


//----------------------------------------------------------------------------------------
// CStr32::CStr32(long):
//----------------------------------------------------------------------------------------

CStr32::CStr32(const long id)
{
	Length() = 4;
	memcpy(&fStr[1], &id, (size_t)Length());
} // CStr32::CStr32(long)


//----------------------------------------------------------------------------------------
// CStr32::Copy:
//----------------------------------------------------------------------------------------

CStr32 CStr32::Copy(short pos, short length)
{
	CStr32 newString;
	
	length = length > Length() - pos + kLengthByte ? Length() - pos + kLengthByte : length;
	
	if (length > 0)
	{
		memcpy(&newString.fStr[1], &fStr[pos], (size_t)length);
		newString.Length() = length;
	}
	else
		newString = "";
		
	return newString;

} // CStr32::Copy


//----------------------------------------------------------------------------------------
// CStr32::operator +=(CString):  Concatenate a string
//----------------------------------------------------------------------------------------

CStr32& CStr32::operator += (const CString& str)
{
	InsertHelper (str, Length() + 1, kStr32Len);
	return *this;
} // CStr32::operator +=(CString)


//----------------------------------------------------------------------------------------
// CStr32::operator +=(char*):  Concatenate a string
//----------------------------------------------------------------------------------------

CStr32& CStr32::operator += (const char* str)
{
	InsertHelper (str, Length() + 1, kStr32Len);
	return *this;
} // CStr32::operator +=(char*)


//----------------------------------------------------------------------------------------
// CStr32::operator +=(char):  Concatenate a single character
//----------------------------------------------------------------------------------------

CStr32& CStr32::operator += (const char ch)
{
	if (++Length() <= kStr32Len)
		fStr[Length()] = ch;
	else
	{
		--Length();
#if qDebugMsg
		fprintf(stderr,"###CStr32::operator+=: Concatenation produces CStr32 overflow.\n");
#endif
	}
	
	return *this;
} // CStr32::operator +=(char)


//========================================================================================
// CLASS CStr31
//========================================================================================


//----------------------------------------------------------------------------------------
// CStr31::CStr31(char*):
//----------------------------------------------------------------------------------------

CStr31::CStr31(const char* str)
{
	// Truncate the C CString to 31 bytes if necessary.

	Length() = str == NULL ? 0 : (unsigned char)(strlen(str));
	if (Length() > kStr31Len)
		Length() = kStr31Len;
	memcpy(&fStr[1], str, (size_t)Length());
} // CStr31::CStr31(char*)


//----------------------------------------------------------------------------------------
// CStr31::CStr31(long):
//----------------------------------------------------------------------------------------

CStr31::CStr31(const long id)
{
	Length() = 4;
	memcpy(&fStr[1], &id, (size_t)Length());
} // CStr31::CStr31(long)


//----------------------------------------------------------------------------------------
// CStr31::CStr31(char*):
//----------------------------------------------------------------------------------------

void
CStr31::operator =(const CString& str)
{
	Length() = str.Length();
	
	if (Length() > kStr31Len)
		Length() = kStr31Len;
	memcpy(&fStr[1], &str.fStr[1], (size_t)Length());
}

void
CStr31::operator =(const unsigned char* str)
{
	Length() = str == NULL ? 0 : str[0];
	
	if (Length() > kStr31Len)
		Length() = kStr31Len;
	memcpy(&fStr[1], str + 1, (size_t)Length());
}

void
CStr31::operator =(const char* str)
{
	Length() = str == NULL ? 0 : (unsigned char)(strlen(str));
	
	if (Length() > kStr31Len)
		Length() = kStr31Len;
	memcpy(&fStr[1], str, (size_t)Length());
}

//----------------------------------------------------------------------------------------
// CStr31::Copy:
//----------------------------------------------------------------------------------------

CStr31 CStr31::Copy(short pos, short length)
{
	CStr31 newString;
	
	length = length > Length() - pos + kLengthByte ? Length() - pos + kLengthByte : length;
	
	if (length > 0)
	{
		memcpy(&newString.fStr[1], &fStr[pos], (size_t)length);
		newString.Length() = length;
	}
	else
		newString = "";
		
	return newString;

} // CStr31::Copy


//----------------------------------------------------------------------------------------
// CStr31::operator +=(CString):  Concatenate a string
//----------------------------------------------------------------------------------------

CStr31& CStr31::operator += (const CString& str)
{
	InsertHelper (str, Length() + 1, kStr31Len);
	return *this;
} // CStr31::operator +=(CString)


//----------------------------------------------------------------------------------------
// CStr31::operator +=(char*):  Concatenate a string
//----------------------------------------------------------------------------------------

CStr31& CStr31::operator += (const char* str)
{
	InsertHelper (str, Length() + 1, kStr31Len);
	return *this;
} // CStr31::operator +=(char*)


//----------------------------------------------------------------------------------------
// CStr31::operator +=(char):  Concatenate a single character
//----------------------------------------------------------------------------------------

CStr31& CStr31::operator += (const char ch)
{
	if (++Length() <= kStr31Len)
		fStr[Length()] = ch;
	else
	{
		--Length();
#if qDebugMsg
		fprintf(stderr,"###CStr31::operator+=: Concatenation produces CStr31 overflow.\n");
#endif
	}
	
	return *this;
} // CStr31::operator +=(char)

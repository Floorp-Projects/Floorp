//----------------------------------------------------------------------------------------
// PascalString.h 
// Copyright © 1985-1994 by Apple Computer, Inc.  All rights reserved.
//----------------------------------------------------------------------------------------


// ¥ Updated by Jeroen Schalk - DTS
// ¥ Use MABlockMove, not memcpy(), memmove(), strncpy().
// ¥ Changed default constructors to initialize to empty string.
// ¥ Changed number of temporary strings from 4 to 8
// ¥ Make operator[] and unsigned char*() inline
// ¥ Fix bugs in constructors out of const unsigned char* str
// ¥ Optimized constructors to only move required data
// ¥ General cleanup of code for readability

#pragma once

#ifndef __PASCALSTRING__
#define __PASCALSTRING__

#ifndef __MEMORY__
#include <Memory.h>
#endif

#ifndef __TYPES__
#include <Types.h>
#endif

#ifndef __TEXTUTILS__
#include <TextUtils.h>
#endif

#ifndef __OSUTILS__
#include <OSUtils.h>
#endif

#ifndef __STRING__
#include <string.h>
#endif

// Forward declaration for all the CString classes.
struct CString;
struct CStr255;
struct CStr63;
struct CStr32;
struct CStr31;

typedef const CStr255& ConstCStr255Param;
typedef const CStr63& ConstCStr63Param;
typedef const CStr32& ConstCStr32Param;
typedef const CStr31& ConstCStr31Param;

#ifdef Length
#undef Length
#endif
// Some constants defining the length of each of the CString types.

const short kLengthByte = 1;
const short kBaseLen = 2;
const short kStr255Len = 255;
const short kStr63Len = 63;
const short kStr32Len = 32;
const short kStr31Len = 31;

// Number of temporary strings

const short kTempCStrings = 8;

//----------------------------------------------------------------------------------------
// MABlockMove: BlockMoveData() is fastest on PowerPC, memcpy() on 68K
//----------------------------------------------------------------------------------------

#if powerc
	#define MABlockMove(srcPtr, destPtr, byteCount) \
		::BlockMoveData(Ptr(srcPtr), Ptr(destPtr), Size(byteCount))
#else
	#define MABlockMove(srcPtr, destPtr, byteCount) \
		::memcpy(destPtr, srcPtr, size_t(byteCount))
#endif

//----------------------------------------------------------------------------------------
// MABlockMoveOverlap: BlockMoveData() is fastest on PowerPC, memmove() on 68K
//----------------------------------------------------------------------------------------

#if powerc
	#define MABlockMoveOverlap(srcPtr, destPtr, byteCount) \
		::BlockMoveData(Ptr(srcPtr), Ptr(destPtr), Size(byteCount))
#else
	#define MABlockMoveOverlap(srcPtr, destPtr, byteCount) \
		::memmove(destPtr, srcPtr, size_t(byteCount))
#endif

//----------------------------------------------------------------------------------------
// CString: Superclass of all Pascal string compatible string classes.
//----------------------------------------------------------------------------------------

typedef struct CString *CStringPtr, **CStringHandle;

struct CString
{
public:
	unsigned char fStr[kBaseLen];

protected:
	CString() {}
		// This is here (and protected) to stop people trying to instantiate CString.
		// To do so is very bad,  because it's suicide to make one of these!  There are
		// only 2 bytes of data!
	void InsertHelper(const CString& insStr, short pos, short maxLength);
	void InsertHelper(const char* insStr, short pos, short maxLength);

public:
	// Basic length method, inherited by all derived classes. Define one that returns a
	// reference. Can be used as an lvalue and only can be applied to non-const Strings.

	inline unsigned char& Length()
	{
		return fStr[0];
	}											// for non-const CString

	inline unsigned char Length() const
	{
		return fStr[0];
	}											// for const CString

	inline Boolean IsEmpty()
	{
		return fStr[0] <= 0;
	}											// for non-const CString

	inline Boolean IsEmpty() const
	{
		return fStr[0] <= 0;
	}											// for const CString

	// Character selector operator.

	inline unsigned char& operator[](short pos)
	{
		return fStr[pos];
	}											// for non-const CString

	inline unsigned char operator[](short pos) const
	{
		return fStr[pos];
	}											// for const CString
	
	//------------------------------------------------------------------------------------
	// CAUTION: There is a subtle difference between the (char*) and (unsigned char*)
    // conversion operators. The first converts a pascal-style string to a c-style
    // string. The second simply converts between two types (CString and Str55) both of
    // which are pascal-style strings.
	
	// Create a NULL terminated c-style string from a pascal-style CString. Used in
    // debugging to fprintf a CString.
	
	operator char*() const;
	
	// Used to create a toolbox type Str255 from our CString. This is simply a type
	// coercion! Both CString and Str255 are expected to be pascal-style strings.
	
	inline operator unsigned char*()
	{
		return (unsigned char *) this;
	}											// for non-const CString
	
	operator const unsigned char*() const;		// for const CString

	//------------------------------------------------------------------------------------
	
	// Return an ID represented as a CString to the actual ID (a long).
	
	operator long() const;

	// Relational operators that are inherited by all the derived CString types. Three of
	// each so that literal C Strings can be conveniently used for one of the operators as
	// well as two of the derive classes as operators. These are declared here but defined
	// below all the CString classes because they use constructors for CStr255 and its class
	// definition has not been encountered yet.

	friend inline Boolean operator==(const CString& s1,
							  const char* s2);
	friend inline Boolean operator==(const char* s1,
							  const CString& s2);
	friend inline Boolean operator==(const CString& s1,
							  const CString& s2);
	friend inline Boolean operator!=(const CString& s1,
							  const char* s2);
	friend inline Boolean operator!=(const char* s1,
							  const CString& s2);
	friend inline Boolean operator!=(const CString& s1,
							  const CString& s2);

	friend inline Boolean operator>(const CString& s1,
							 const char* s2);
	friend inline Boolean operator>(const char* s1,
							 const CString& s2);
	friend inline Boolean operator>(const CString& s1,
							 const CString& s2);

	friend inline Boolean operator<(const CString& s1,
							 const char* s2);
	friend inline Boolean operator<(const char* s1,
							 const CString& s2);
	friend inline Boolean operator<(const CString& s1,
							 const CString& s2);

	friend inline Boolean operator>=(const CString& s1,
							  const char* s2);
	friend inline Boolean operator>=(const char* s1,
							  const CString& s2);
	friend inline Boolean operator>=(const CString& s1,
							  const CString& s2);

	friend inline Boolean operator<=(const CString& s1,
							  const char* s2);
	friend inline Boolean operator<=(const char* s1,
							  const CString& s2);
	friend inline Boolean operator<=(const CString& s1,
							  const CString& s2);

	// Concatenation operator that are inherited by all the derived CString types. Three
	// of each so that literal C Strings can be conveniently used for one of the operators
	// as well as using any two classes derived from CString.

	friend CStr255 operator+(const CString& s1,
							 const char* s2);
	friend CStr255 operator+(const char* s1,
							 const CString& s2);
	friend CStr255 operator+(const CString& s1,
							 const CString& s2);

	// Methods that mimic the Pascal builtin CString functions for Pos, Insert and Delete.
	// Note that insert and copy is implemented in the derived classes.

	unsigned char Pos(const char* subStr, unsigned char startPos = 1);
	unsigned char Pos(const CString& subStr, unsigned char startPos = 1);
	void Delete(short pos, short length);

protected:
	inline long Min(const long a, const long b) const
	{
		return a < b ? a : b;
	}
};


//----------------------------------------------------------------------------------------
// CStr255:
//----------------------------------------------------------------------------------------

struct CStr255 : CString
{
	friend struct CStr63;
	friend struct CStr31;

private:
	unsigned char fData[kStr255Len - 1];

public:
	static const CStr255 sEmptyString;
	
	CStr255();
	CStr255(const CStr255& str);
	CStr255(const CStr63& str);
	CStr255(const CStr32& str);
	CStr255(const CStr31& str);
	CStr255(const unsigned char* str);
	CStr255(const char* str);
	CStr255(const long id);

	// Insert and Copy roughly equivalent to the Pascal Insert and Copy functions.
	
	void Insert(const CString& str, short pos);
	void Insert(const char* str, short pos);
	CStr255 Copy(short pos, short length);
					   
	// Concatenation operator
	
	CStr255& operator +=(const CString& str);
	CStr255& operator +=(const char* str);
	CStr255& operator +=(const char ch);

	// Assignment operator

	CStr255& operator =(const CStr255& str);
	CStr255& operator =(const CStr63& str);
	CStr255& operator =(const CStr32& str);
	CStr255& operator =(const CStr31& str);
	CStr255& operator =(const unsigned char* str);
	CStr255& operator =(const char aChar);
	CStr255& operator =(const char* str);
};


//----------------------------------------------------------------------------------------
// CStr63:
//----------------------------------------------------------------------------------------

struct CStr63 : CString
{
	friend struct CStr255;
	friend struct CStr31;

private:
	unsigned char fData[kStr63Len - 1];

public:
	CStr63();
	CStr63(const CStr255& str);
	CStr63(const CStr63& str);
	CStr63(const CStr32& str);
	CStr63(const CStr31& str);
	CStr63(const unsigned char* str);
	CStr63(const char* str);
	CStr63(const long id);

	// Insert and Copy roughly equivalent to the Pascal Insert and Copy functions.
	
	void Insert(const CString& str, short pos);
	void Insert(const char* str, short pos);
	CStr63 Copy(short pos, short length);

	// Concatenation operator
	
	CStr63& operator +=(const CString& str);
	CStr63& operator +=(const char* str);
	CStr63& operator +=(const char ch);
};


//----------------------------------------------------------------------------------------
// CStr32:
//----------------------------------------------------------------------------------------

struct CStr32 : CString
{
	friend struct CStr255;
	friend struct CStr63;

private:
	unsigned char fData[kStr32Len - 1];

public:
	CStr32();
	inline CStr32(unsigned char length)
	{
		fStr[0] = length;
	}

	CStr32(const CStr255& str);
	CStr32(const CStr63& str);
	CStr32(const CStr32& str);
	CStr32(const CStr31& str);
	CStr32(const unsigned char* str);
	CStr32(const char* str);
	CStr32(const long id);

	// Insert and Copy roughly equivalent to the Pascal Insert and Copy functions.
	
	void Insert(const CString& str, short pos);
	void Insert(const char* str, short pos);
	CStr32 Copy(short pos, short length);

	// Concatenation operator
	
	CStr32& operator +=(const CString& str);
	CStr32& operator +=(const char* str);
	CStr32& operator +=(const char ch);
};


//----------------------------------------------------------------------------------------
// CStr31:
//----------------------------------------------------------------------------------------

struct CStr31 : CString
{
	friend struct CStr255;
	friend struct CStr63;
	friend struct CStr32;

private:
	unsigned char fData[kStr31Len - 1];

public:
	CStr31();
	inline CStr31(unsigned char length)
	{
		fStr[0] = length;
	}

	CStr31(const CStr255& str);
	CStr31(const CStr63& str);
	CStr31(const CStr32& str);
	CStr31(const CStr31& str);
	CStr31(const unsigned char* str);
	CStr31(const char* str);
	CStr31(const long id);

	void	operator =(const CString& str);
	void	operator =(const unsigned char* str);
	void	operator =(const char* str);
	
	// Insert and Copy roughly equivalent to the Pascal Insert and Copy functions.
	
	void Insert(const CString& str, short pos);
	void Insert(const char* str, short pos);
	CStr31 Copy(short pos, short length);

	// Concatenation operator
	
	CStr31& operator +=(const CString& str);
	CStr31& operator +=(const char* str);
	CStr31& operator +=(const char ch);
};

//----------------------------------------------------------------------------------------
// CStr255 inline function definitions
//----------------------------------------------------------------------------------------

inline CStr255::CStr255()
{
	fStr[0] = 0;
}

inline CStr255::CStr255(const CStr255& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr255::CStr255(const CStr63& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr255::CStr255(const CStr32& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr255::CStr255(const CStr31& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr255::CStr255(const unsigned char* str)
{
	MABlockMove(str, fStr, str[0] + kLengthByte);
}

inline CStr255& CStr255::operator = (const CStr255& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
	
	return *this;
}

inline CStr255& CStr255::operator = (const CStr63& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
	
	return *this;
}

inline CStr255& CStr255::operator = (const CStr32& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
	
	return *this;
}

inline CStr255& CStr255::operator = (const CStr31& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
	
	return *this;
}

inline CStr255& CStr255::operator = (const unsigned char* str)
{
	MABlockMove(str, fStr, str[0] + kLengthByte);
	
	return *this;
}

inline CStr255& CStr255::operator = (const char aChar)
{
	Length() = (aChar) ? 1 : 0;
	fStr[1] = aChar;
	
	return *this;
}

inline void CStr255::Insert(const CString& str, short pos)
{
	this->InsertHelper(str, pos, kStr255Len);
}

inline void CStr255::Insert(const char* str, short pos)
{
	this->InsertHelper(str, pos, kStr255Len);
}


//----------------------------------------------------------------------------------------
// CStr63 inline function definitions
//----------------------------------------------------------------------------------------

inline CStr63::CStr63()
{
	fStr[0] = 0;
}

inline CStr63::CStr63(const CStr255& str)
{
	// Truncate the CStr255 to 63 bytes if necessary.

	Length() = str.Length() > kStr63Len ? kStr63Len : str.Length();
	MABlockMove(&str.fStr[1], &fStr[1], Length());
}

inline CStr63::CStr63(const CStr63& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr63::CStr63(const CStr32& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr63::CStr63(const CStr31& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr63::CStr63(const unsigned char* str)
{
	MABlockMove(str, fStr, Min(str[0] + kLengthByte, sizeof(CStr63)));
}

inline void CStr63::Insert(const CString& str, short pos)
{
	this->InsertHelper(str, pos, kStr63Len);
}

inline void CStr63::Insert(const char* str, short pos)
{
	this->InsertHelper(str, pos, kStr63Len);
}


//----------------------------------------------------------------------------------------
// CStr32 inline function definitions
//----------------------------------------------------------------------------------------

inline CStr32::CStr32()
{
	fStr[0] = 0;
}

inline CStr32::CStr32(const CStr255& str)
{
	// Truncate the CStr255 to 32 bytes if necessary.

	Length() = str.Length() > kStr32Len ? kStr32Len : str.Length();
	MABlockMove(&str.fStr[1], &fStr[1], Length());
}

inline CStr32::CStr32(const CStr63& str)
{
	// Truncate the CStr63 to 32 bytes if necessary.

	Length() = str.Length() > kStr32Len ? kStr32Len : str.Length();
	MABlockMove(&str.fStr[1], &fStr[1], Length());
}

inline CStr32::CStr32(const CStr32& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr32::CStr32(const CStr31& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr32::CStr32(const unsigned char* str)
{
	MABlockMove(str, fStr, Min(str[0] + kLengthByte, sizeof(CStr32)));
}

inline void CStr32::Insert(const CString& str, short pos)
{
	this->InsertHelper(str, pos, kStr32Len);
}

inline void CStr32::Insert(const char* str, short pos)
{
	this->InsertHelper(str, pos, kStr32Len);
}


//----------------------------------------------------------------------------------------
// CStr31 inline function definitions
//----------------------------------------------------------------------------------------

inline CStr31::CStr31()
{
	fStr[0] = 0;
}

inline CStr31::CStr31(const CStr255& str)
{
	// Truncate the CStr255 to 31 bytes if necessary.

	Length() = str.Length() > kStr31Len ? kStr31Len : str.Length();
	MABlockMove(&str.fStr[1], &fStr[1], Length());
}

inline CStr31::CStr31(const CStr63& str)
{
	// Truncate the CStr63 to 31 bytes if necessary.

	Length() = str.Length() > kStr31Len ? kStr31Len : str.Length();
	MABlockMove(&str.fStr[1], &fStr[1], Length());
}

inline CStr31::CStr31(const CStr32& str)
{
	// Truncate the CStr32 to 31 bytes if necessary.

	Length() = str.Length() > kStr31Len ? kStr31Len : str.Length();
	MABlockMove(&str.fStr[1], &fStr[1], Length());
}

inline CStr31::CStr31(const CStr31& str)
{
	MABlockMove(str.fStr, fStr, str.Length() + kLengthByte);
}

inline CStr31::CStr31(const unsigned char* str)
{
	MABlockMove(str, fStr, Min(str[0] + kLengthByte, sizeof(CStr31)));
}

inline void CStr31::Insert(const CString& str, short pos)
{
	this->InsertHelper(str, pos, kStr31Len);
}

inline void CStr31::Insert(const char* str, short pos)
{
	this->InsertHelper(str, pos, kStr31Len);
}


//----------------------------------------------------------------------------------------
// Inline friend function definitions for relational string operators.
//----------------------------------------------------------------------------------------

inline Boolean operator==(const CString& s1, const char* s2)
{
	return ::RelString((ConstStr255Param)&s1, CStr255(s2), false, true) == 0;
}

inline Boolean operator==(const char* s1, const CString& s2)
{
	return ::RelString(CStr255(s1), (ConstStr255Param)&s2, false, true) == 0;
}

inline Boolean operator==(const CString& s1, const CString& s2)
{
	return ::RelString((ConstStr255Param)&s1, (ConstStr255Param)&s2, false, true) == 0;
}

inline Boolean operator!=(const CString& s1, const char* s2)
{
	return ::RelString((ConstStr255Param)&s1, CStr255(s2), false, true) != 0;
}

inline Boolean operator!=(const char* s1, const CString& s2)
{
	return ::RelString(CStr255(s1), (ConstStr255Param)&s2, false, true) != 0;
}

inline Boolean operator!=(const CString& s1, const CString& s2)
{
	return ::RelString((ConstStr255Param)&s1, (ConstStr255Param)&s2, false, true) != 0;
}

inline Boolean operator>(const CString& s1, const char* s2)
{
	return ::RelString((ConstStr255Param)&s1, CStr255(s2), false, true) > 0;
}

inline Boolean operator>(const char* s1, const CString& s2)
{
	return ::RelString(CStr255(s1), (ConstStr255Param)&s2, false, true) > 0;
}

inline Boolean operator>(const CString& s1, const CString& s2)
{
	return ::RelString((ConstStr255Param)&s1, (ConstStr255Param)&s2, false, true) > 0;
}

inline Boolean operator<(const CString& s1, const char* s2)
{
	return ::RelString((ConstStr255Param)&s1, CStr255(s2), false, true) < 0;
}

inline Boolean operator<(const char* s1, const CString& s2)
{
	return ::RelString(CStr255(s1), (ConstStr255Param)&s2, false, true) < 0;
}

inline Boolean operator<(const CString& s1, const CString& s2)
{
	return ::RelString((ConstStr255Param)&s1, (ConstStr255Param)&s2, false, true) < 0;
}

inline Boolean operator>=(const CString& s1, const char* s2)
{
	return ::RelString((ConstStr255Param)&s1, CStr255(s2), false, true) >= 0;
}

inline Boolean operator>=(const char* s1, const CString& s2)
{
	return ::RelString(CStr255(s1), (ConstStr255Param)&s2, false, true) >= 0;
}

inline Boolean operator>=(const CString& s1, const CString& s2)
{
	return ::RelString((ConstStr255Param)&s1, (ConstStr255Param)&s2, false, true) >= 0;
}

inline Boolean operator<=(const CString& s1, const char* s2)
{
	return ::RelString((ConstStr255Param)&s1, CStr255(s2), false, true) <= 0;
}

inline Boolean operator<=(const char* s1, const CString& s2)
{
	return ::RelString(CStr255(s1), (ConstStr255Param)&s2, false, true) <= 0;
}

inline Boolean operator<=(const CString& s1, const CString& s2)
{
	return ::RelString((ConstStr255Param)&s1, (ConstStr255Param)&s2, false, true) <= 0;
}

#endif

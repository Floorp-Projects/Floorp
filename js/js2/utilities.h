// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#ifndef utilities_h
#define utilities_h

#include <memory>
#include <string>
#include <iostream>
#include "systemtypes.h"

using std::string;
using std::istream;
using std::ostream;
using std::auto_ptr;

namespace JavaScript {


//
// Assertions
//

	#ifdef DEBUG
	 void Assert(const char *s, const char *file, int line);

	 #define ASSERT(_expr) ((_expr) ? (void)0 : JavaScript::Assert(#_expr, __FILE__, __LINE__))
	 #define NOT_REACHED(_reasonStr) JS_Assert(_reasonStr, __FILE__, __LINE__)
	 #define DEBUG_ONLY(_stmt) _stmt
	#else
	 #define ASSERT(expr)
	 #define NOT_REACHED(reasonStr)
	 #define DEBUG_ONLY(_stmt)
	#endif


//
// Numerics
//

	template<class N> N min(N v1, N v2) {return v1 <= v2 ? v1 : v2;}
	template<class N> N max(N v1, N v2) {return v1 >= v2 ? v1 : v2;}

//
// Bit manipulation
//

	#define JS_BIT(n)       ((uint32)1 << (n))
	#define JS_BITMASK(n)   (JS_BIT(n) - 1)

	uint ceilingLog2(uint32 n);
	uint floorLog2(uint32 n);


//
// Unicode UTF-16 characters and strings
//

	// A UTF-16 character
	// Use wchar_t on platforms on which wchar_t has 16 bits; otherwise use uint16.
	// Note that in C++ wchar_t is a distinct type rather than a typedef for some integral type.
	// Like char, a char16 can be either signed or unsigned at the implementation's discretion.
	typedef wchar_t char16;
	typedef unsigned wchar_t uchar16;

	// A string of UTF-16 characters.  Nulls are allowed just like any other character.
	// The string is not null-terminated.
	// Use wstring if char16 is wchar_t.  Otherwise use basic_string<uint16>.
	typedef std::basic_string<char16> String;

  #if 0
	using std::wint_t;
	const wint_t ueof = WEOF;
  #else
	typedef int32 wint_t;	// A type that can hold any char16 plus one special value: ueof.
	const wint_t ueof = -1;
  #endif

	// Special char16s
	namespace uni {
		const char16 null = '\0';
		const char16 cr = '\r';
		const char16 lf = '\n';
		const char16 ls = 0x2028;
		const char16 ps = 0x2029;
	}
	
	inline char16 widen(char ch) {return static_cast<char16>(static_cast<uchar>(ch));}


	class CharInfo {
		// Unicode character attribute lookup tables
		static const uint8 x[];
		static const uint8 y[];
		static const uint32 a[];
	
	  public:
		// Enumerated Unicode general category types
		enum Type {
		    Unassigned            = 0,	// Cn
		    UppercaseLetter       = 1,	// Lu
		    LowercaseLetter       = 2,	// Ll
		    TitlecaseLetter       = 3,	// Lt
		    ModifierLetter        = 4,	// Lm
		    OtherLetter           = 5,	// Lo
		    NonSpacingMark        = 6,	// Mn
		    EnclosingMark         = 7,	// Me
		    CombiningSpacingMark  = 8,	// Mc
		    DecimalDigitNumber    = 9,	// Nd
		    LetterNumber          = 10,	// Nl
		    OtherNumber           = 11,	// No
		    SpaceSeparator        = 12,	// Zs
		    LineSeparator         = 13,	// Zl
		    ParagraphSeparator    = 14,	// Zp
		    Control               = 15,	// Cc
		    Format                = 16,	// Cf
		    PrivateUse            = 18,	// Co
		    Surrogate             = 19,	// Cs
		    DashPunctuation       = 20,	// Pd
		    StartPunctuation      = 21,	// Ps
		    EndPunctuation        = 22,	// Pe
		    ConnectorPunctuation  = 23,	// Pc
		    OtherPunctuation      = 24,	// Po
		    MathSymbol            = 25,	// Sm
		    CurrencySymbol        = 26,	// Sc
		    ModifierSymbol        = 27,	// Sk
		    OtherSymbol           = 28	// So
		};

		enum Group {
		    NonIdGroup,			// 0  May not be part of an identifier
		    FormatGroup,		// 1  Format control
		    IdGroup,			// 2  May start or continue a JS identifier (includes $ and _)
		    IdContinueGroup,	// 3  May continue a JS identifier  [IdContinueGroup & -2 == IdGroup]
		    WhiteGroup,			// 4  White space character (but not line break)
		    LineBreakGroup		// 5  Line break character  [LineBreakGroup & -2 == WhiteGroup]
		};

		// Character classifying and mapping macros, based on java.lang.Character
		static uint32 cCode(char16 c) {return a[y[x[static_cast<uint16>(c)>>6]<<6 | c&0x3F]];}
		static Type cType(char16 c) {return static_cast<Type>(cCode(c) & 0x1F);}
		static Group cGroup(char16 c) {return static_cast<Group>(cCode(c) >> 16 & 7);}
	};

	inline bool isAlpha(char16 c)
	{
		return ((((1 << CharInfo::UppercaseLetter) | (1 << CharInfo::LowercaseLetter) | (1 << CharInfo::TitlecaseLetter) |
				  (1 << CharInfo::ModifierLetter) | (1 << CharInfo::OtherLetter))
				 >> CharInfo::cType(c)) & 1) != 0;
	}

	inline bool isAlphanumeric(char16 c)
	{
		return ((((1 << CharInfo::UppercaseLetter) | (1 << CharInfo::LowercaseLetter) | (1 << CharInfo::TitlecaseLetter) |
				  (1 << CharInfo::ModifierLetter) | (1 << CharInfo::OtherLetter) | (1 << CharInfo::DecimalDigitNumber))
				 >> CharInfo::cType(c)) & 1) != 0;
	}

	// Return true if c can start a JavaScript identifier
	inline bool isIdLeading(char16 c) {return CharInfo::cGroup(c) == CharInfo::IdGroup;}
	// Return true if c can continue a JavaScript identifier
	inline bool isIdContinuing(char16 c) {return CharInfo::cGroup(c) & -2 == CharInfo::IdGroup;}

	// Return true if c is a Unicode decimal digit (Nd) character
	inline bool isDecimalDigit(char16 c) {return CharInfo::cType(c) == CharInfo::DecimalDigitNumber;}
	// Return true if c is a Unicode white space or line break character
	inline bool isSpace(char16 c) {return CharInfo::cGroup(c) & -2 == CharInfo::WhiteGroup;}
	// Return true if c is a Unicode line break character (LF, CR, LS, or PS)
	inline bool isLineBreak(char16 c) {return CharInfo::cGroup(c) == CharInfo::LineBreakGroup;}

	inline bool isUpper(char16 c) {return CharInfo::cType(c) == CharInfo::UppercaseLetter;}
	inline bool isLower(char16 c) {return CharInfo::cType(c) == CharInfo::LowercaseLetter;}

	char16 toUpper(char16 c);
	char16 toLower(char16 c);


//
// Growable arrays
//

	// private
	template <typename T>
	class ProtoArrayBuffer
	{
	  protected:
	    T *buffer;
	    int32 length;
	    int32 bufferSize;

	    void append(const T *elts, int32 nElts, T *cache);
	};


	// private
	template <typename T>
	void ProtoArrayBuffer<T>::append(const T *elts, int32 nElts, T *cache)
	{
	    assert(nElts >= 0);
	    int32 newLength = length + nElts;
	    if (newLength > bufferSize) {
	        // Allocate a new buffer and copy the current buffer's contents there.
	        int32 newBufferSize = newLength + bufferSize;
	        auto_ptr<T> newBuffer = new T[newBufferSize];
	        T *p = buffer;
	        T *pLimit = old + length;
	        T *q = newBuffer.get();
	        while (p != pLimit)
	            *q++ = *p++;
	        if (buffer != cache)
	            delete buffer;
	        buffer = newBuffer.release();
	        bufferSize = newBufferSize;
	    }
	    length = newLength;
	}


	// An ArrayBuffer represents an array of elements of type T.  The ArrayBuffer contains
	// storage for a fixed size array of cacheSize elements; if this size is exceeded, the
	// ArrayBuffer allocates the array from the heap.
	// Use append to append nElts elements to the end of the ArrayBuffer.
	template <typename T, int32 cacheSize>
	class ArrayBuffer: public ProtoArrayBuffer<T>
	{
	    T cache[cacheSize];

	  public:
	    ArrayBuffer() {buffer = &cache; length = cacheSize; bufferSize = cacheSize;}
	    ~ArrayBuffer() {if (buffer != &cache) delete buffer;}
	    
	    int32 size() const {return length;}
	    T *front() const {return buffer;}
	    void append(const T *elts, int32 nElts) {ProtoArrayBuffer<T>::append(elts, nElts, cache);}
	};



//
// Algorithms
//

	// Assign zero to every element between first inclusive and last exclusive.
	// This is equivalent ot fill(first, last, 0) but may be more efficient.
	template<class For>
	inline void zero(For first, For last)
	{
		while (first != last)
			*first++ = 0;
	}


	// Assign zero to n elements starting at first.
	// This is equivalent ot fill_n(first, n, 0) but may be more efficient.
	template<class For, class Size>
	inline void zero_n(For first, Size n)
	{
		while (n--)
			*first++ = 0;
	}


//
// C++ I/O
//


	// A class to remember the format of an ostream so that a function may modify it internally
	// without changing it for the caller.
	class SaveFormat
	{
		ostream &o;
		std::ios_base::fmtflags flags;
		char fill;
	  public:
		explicit SaveFormat(ostream &out);
		~SaveFormat();
	};


	void showChar(ostream &out, char16 ch);

	template<class In>
	void showString(ostream &out, In begin, In end)
	{
		while (begin != end)
			showChar(out, *begin++);
	}
	void showString(ostream &out, const String &str);

}
#endif

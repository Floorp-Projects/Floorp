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

#include "systemtypes.h"
#include <memory>
#include <new>
#include <string>
#include <iterator>
#include <iostream>
#include <cstdio>

#ifndef _WIN32	// Microsoft Visual C++ 6.0 bug: standard identifiers should be in std namespace
using std::size_t;
using std::ptrdiff_t;
using std::strlen;
using std::strcpy;
using std::sprintf;
using std::fprintf;
#define STD std
#else
#define STD
#endif
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
	 #define NOT_REACHED(_reasonStr) JavaScript::Assert(_reasonStr, __FILE__, __LINE__)
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

	// Special char16s
	namespace uni {
		const char16 null = '\0';
		const char16 cr = '\r';
		const char16 lf = '\n';
		const char16 space = ' ';
		const char16 ls = 0x2028;
		const char16 ps = 0x2029;
	}
	const uint16 firstFormatChar = 0x200C;	// Lowest Unicode Cf character
	
	inline char16 widen(char ch) {return static_cast<char16>(static_cast<uchar>(ch));}
	
	// Use char16Value to compare char16's for inequality because an implementation may have char16's
	// be either signed or unsigned.
	inline uint16 char16Value(char16 ch) {return static_cast<uint16>(ch);}

	// A string of UTF-16 characters.  Nulls are allowed just like any other character.
	// The string is not null-terminated.
	// Use wstring if char16 is wchar_t.  Otherwise use basic_string<uint16>.
	//
	// Eventually we'll want to use a custom class better suited for JavaScript that generates less
	// code bloat and separates the concepts of a fixed, read-only string from a mutable buffer that
	// is expanding.  For now, though, we use the standard basic_string.
	typedef std::basic_string<char16> String;


	typedef uint32 char16orEOF;		// A type that can hold any char16 plus one special value: ueof.
	const char16orEOF char16eof = static_cast<char16orEOF>(-1);

	// If c is a char16, return it; if c is char16eof, return the character \uFFFF.
	inline char16 char16orEOFToChar16(char16orEOF c) {return static_cast<char16>(c);}

#ifndef _WIN32
	// Return a String containing the characters of the null-terminated C string cstr
	// (without the trailing null).
	inline String widenCString(const char *cstr)
	{
		size_t len = strlen(cstr);
		const uchar *ucstr = reinterpret_cast<const uchar *>(cstr);
		return String(ucstr, ucstr+len);
	}


	// Widen and append length characters starting at chars to the end of str.
	inline void appendChars(String &str, const char *chars, size_t length)
	{
		const uchar *uchars = reinterpret_cast<const uchar *>(chars);
		str.append(uchars, uchars + length);
	}

	// Widen and insert length characters starting at chars into the given position of str.
	inline void insertChars(String &str, String::size_type pos, const char *chars, size_t length)
	{
		ASSERT(pos <= str.size());
		const uchar *uchars = reinterpret_cast<const uchar *>(chars);
		str.insert(str.begin() + pos, uchars, uchars + length);
	}
#else // Microsoft VC6 bug: String constructor and append limited to char16 iterators
	String widenCString(const char *cstr);
	void appendChars(String &str, const char *chars, size_t length);
	void insertChars(String &str, String::size_type pos, const char *chars, size_t length)
#endif
	void insertChars(String &str, String::size_type pos, const char *cstr);


	String &operator+=(String &str, const char *cstr);
	String operator+(const String &str, const char *cstr);
	String operator+(const char *cstr, const String &str);
	inline String &operator+=(String &str, char c) {return str += widen(c);}
	inline void clear(String &s) {s.resize(0);}


	class CharInfo {
		uint32 info;					// Word from table a.

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
		    IdContinueGroup,	// 3  May continue a JS identifier  [(IdContinueGroup & -2) == IdGroup]
		    WhiteGroup,			// 4  White space character (but not line break)
		    LineBreakGroup		// 5  Line break character  [(LineBreakGroup & -2) == WhiteGroup]
		};

		CharInfo() {}
		CharInfo(char16 c): info(a[y[x[static_cast<uint16>(c)>>6]<<6 | c&0x3F]]) {}
		CharInfo(const CharInfo &ci): info(ci.info) {}

		friend Type cType(const CharInfo &ci) {return static_cast<Type>(ci.info & 0x1F);}
		friend Group cGroup(const CharInfo &ci) {return static_cast<Group>(ci.info >> 16 & 7);}

		friend bool isAlpha(const CharInfo &ci)
		{
			return ((1<<UppercaseLetter | 1<<LowercaseLetter | 1<<TitlecaseLetter | 1<<ModifierLetter | 1<<OtherLetter)
					 >> cType(ci) & 1) != 0;
		}

		friend bool isAlphanumeric(const CharInfo &ci)
		{
			return ((1<<UppercaseLetter | 1<<LowercaseLetter | 1<<TitlecaseLetter | 1<<ModifierLetter | 1<<OtherLetter |
					 1<<DecimalDigitNumber | 1<<LetterNumber)
					>> cType(ci) & 1) != 0;
		}

		// Return true if this character can start a JavaScript identifier
		friend bool isIdLeading(const CharInfo &ci) {return cGroup(ci) == IdGroup;}
		// Return true if this character can continue a JavaScript identifier
		friend bool isIdContinuing(const CharInfo &ci) {return (cGroup(ci) & -2) == IdGroup;}

		// Return true if this character is a Unicode decimal digit (Nd) character
		friend bool isDecimalDigit(const CharInfo &ci) {return cType(ci) == DecimalDigitNumber;}
		// Return true if this character is a Unicode white space or line break character
		friend bool isSpace(const CharInfo &ci) {return (cGroup(ci) & -2) == WhiteGroup;}
		// Return true if this character is a Unicode line break character (LF, CR, LS, or PS)
		friend bool isLineBreak(const CharInfo &ci) {return cGroup(ci) == LineBreakGroup;}
		// Return true if this character is a Unicode format control character (Cf)
		friend bool isFormat(const CharInfo &ci) {return cGroup(ci) == FormatGroup;}

		friend bool isUpper(const CharInfo &ci) {return cType(ci) == UppercaseLetter;}
		friend bool isLower(const CharInfo &ci) {return cType(ci) == LowercaseLetter;}

		friend char16 toUpper(char16 c);
		friend char16 toLower(char16 c);
	};
	
	inline bool isASCIIDecimalDigit(char16 c) {return c >= '0' && c <= '9';}
	bool isASCIIHexDigit(char16 c, uint &digit);

	const char16 *skipWhiteSpace(const char16 *str, const char16 *strEnd);


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
// Arenas
//

#ifndef _WIN32
	// Pretend that obj points to a value of class T and call obj's destructor.
	template<class T>
	void classDestructor(void *obj)
	{
		static_cast<T *>(obj)->~T();
	}
#else	// Microsoft Visual C++ 6.0 bug workaround
	template<class T>
	struct DestructorHolder {
		static void destroy(void *obj) {static_cast<T *>(obj)->~T();}
	};
#endif

	// An arena is a region of memory from which objects either derived from ArenaObject or allocated
	// using a ArenaAllocator can be allocated.  Deleting these objects individually runs the destructors,
	// if any, but does not deallocate the memory.  On the other hand, the entire arena can be deallocated
	// as a whole.
	//
	// One may also allocate other objects in an arena by using the Arena specialization of the global
	// operator new.  However, be careful not to delete any such objects explicitly!
	//
	// Destructors can be registered for objects (or parts of objects) allocated in the arena.  These
	// destructors are called, in reverse order of being registered, at the time the arena is deallocated
	// or cleared.  When registering destructors for an object O be careful not to delete O manually because that
	// would run its destructor twice.
	class Arena {
		struct Directory {
			enum {maxNBlocks = 31};
			
			Directory *next;			// Next directory in linked list
			uint nBlocks;				// Number of blocks used in this directory
			void *blocks[maxNBlocks];	// Pointers to data blocks; only the first nBlocks are valid
			
			Directory(): nBlocks(0) {}
			void clear();
		};
		
		struct DestructorEntry;

		char *freeBegin;				// Pointer to free bytes left in current block
		char *freeEnd;					// Pointer to end of free bytes left in current block
		size_t blockSize;				// Size of individual arena blocks
		Directory *currentDirectory;	// Directory in which the last block was allocated
		Directory rootDirectory;		// Initial directory; root of linked list of Directories
		DestructorEntry *destructorEntries; // Linked list of destructor registrations, ordered from most to least recently registered
		
	  public:
		explicit Arena(size_t blockSize = 1024);
	  private:
	    Arena(const Arena&);			// No copy constructor
	    void operator=(const Arena&);	// No assignment operator
	  public:
		void clear();
		~Arena() {clear();}
		
	  private:
		void *newBlock(size_t size);
		void newDestructorEntry(void (*destructor)(void *), void *object);
	  public:
		void *allocate(size_t size);
		// Ensure that object's destructor is called at the time the arena is deallocated or cleared.
		// The destructors will be called in reverse order of being registered.
		// registerDestructor might itself runs out of memory, in which case it immediately
		// calls object's destructor before throwing bad_alloc.
#ifndef _WIN32
		template<class T> void registerDestructor(T *object) {newDestructorEntry(&classDestructor<T>, object);}
#else
		template<class T> void registerDestructor(T *object) {newDestructorEntry(&DestructorHolder<T>::destroy, object);}
#endif
	};

	
	// Objects derived from this class will be contained in the Arena passed to the new operator.
	struct ArenaObject {
		void *operator new(size_t size, Arena &arena) {return arena.allocate(size);}
		void *operator new[](size_t size, Arena &arena) {return arena.allocate(size);}
#ifndef __MWERKS__	// Metrowerks 5.3 bug: These aren't supported yet
		void operator delete(void *, Arena &) {}
		void operator delete[](void *, Arena &) {}
#endif
	  private:
		void operator delete(void *, size_t) {}
		void operator delete[](void *) {}
	};


	// Objects allocated by passing this class to standard containers will be contained in the Arena
	// passed to the ArenaAllocator's constructor.
	template<class T>
	class ArenaAllocator {
		Arena &arena;

	  public:
		typedef T value_type;
		typedef size_t size_type;
		typedef ptrdiff_t difference_type;
		typedef T *pointer;
		typedef const T *const_pointer;
		typedef T &reference;
		typedef const T &const_reference;
		
		static pointer address(reference r) {return &r;}
		static const_pointer address(const_reference r) {return &r;}
		
		ArenaAllocator(Arena &arena): arena(arena) {}
		template<class U> ArenaAllocator(const ArenaAllocator<U> &u): arena(u.arena) {}
		
		pointer allocate(size_type n, const void *hint = 0) {return static_cast<pointer>(arena.allocate(n*sizeof(T)));}
		static void deallocate(pointer, size_type) {}
		
		static void construct(pointer p, const T &val) {new(p) T(val);}
		static void destroy(pointer p) {p->~T();}
		
#ifdef __GNUC__
        // why doesn't g++ support numeric_limits<T>?
        static size_type max_size() {return size_type(-1) / sizeof(T);}
#else
		static size_type max_size() {return std::numeric_limits<size_type>::max() / sizeof(T);}
#endif
		
		template<class U> struct rebind {typedef ArenaAllocator<U> other;};
	};


	String &newArenaString(Arena &arena);
	String &newArenaString(Arena &arena, const String &str);


//
// Array auto_ptr's
//

	// An ArrayAutoPtr holds a pointer to an array initialized by new T[x].
	// A regular auto_ptr cannot be used here because it deletes its pointer using
	// delete rather than delete[].
	// An appropriate operator[] is also provided.
	template <typename T>
	class ArrayAutoPtr {
		T *ptr;
		
	  public:
		explicit ArrayAutoPtr(T *p = 0): ptr(p) {}
		ArrayAutoPtr(ArrayAutoPtr &a): ptr(a.ptr) {a.ptr = 0;}
		ArrayAutoPtr &operator=(ArrayAutoPtr &a) {reset(a.release());}
		~ArrayAutoPtr() {delete[] ptr;}
		
		T &operator*() const {return *ptr;}
		T &operator->() const {return *ptr;}
		template<class N> T &operator[](N i) const {return ptr[i];}
		T *get() const {return ptr;}
		T *release() {T *p = ptr; ptr = 0; return p;}
		void reset(T *p = 0) {delete[] ptr; ptr = p;}
	};
	
	typedef ArrayAutoPtr<char> CharAutoPtr;


//
// Growable arrays
//

	// private
	template <typename T>
	class ProtoArrayBuffer {
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
	class ArrayBuffer: public ProtoArrayBuffer<T> {
	    T cache[cacheSize];

	  public:
	    ArrayBuffer() {buffer = &cache; length = cacheSize; bufferSize = cacheSize;}
	    ~ArrayBuffer() {if (buffer != &cache) delete buffer;}
	    
	    int32 size() const {return length;}
	    T *front() const {return buffer;}
	    void append(const T *elts, int32 nElts) {ProtoArrayBuffer<T>::append(elts, nElts, cache);}
	};



//
// Linked Lists
//

	// In some cases it is desirable to manipulate ordinary C-style linked lists as though
	// they were STL-like sequences.  These classes define STL forward iterators that walk
	// through singly-linked lists of objects threaded through fields named 'next'.  The type
	// parameter E must be a class that has a member named 'next' whose type is E* or const E*.
	
	template <class E>
	class ListIterator: public std::iterator<std::forward_iterator_tag, E> {
		E *element;

	  public:
		ListIterator() {}
		explicit ListIterator(E *e): element(e) {}

		E &operator*() const {return *element;}
		E *operator->() const {return element;}
		ListIterator &operator++() {element = element->next; return *this;}
		ListIterator operator++(int) {ListIterator i(*this); element = element->next; return i;}
		friend bool operator==(const ListIterator &i, const ListIterator &j) {return i.element == j.element;}
		friend bool operator!=(const ListIterator &i, const ListIterator &j) {return i.element != j.element;}
	};

	
	template <class E>
#ifndef _WIN32 // Microsoft VC6 bug: std::iterator should support five template arguments
	class ConstListIterator: public std::iterator<std::forward_iterator_tag, E, ptrdiff_t, const E*, const E&> {
#else
	class ConstListIterator: public std::iterator<std::forward_iterator_tag, E, ptrdiff_t> {
#endif
		const E *element;

	  public:
		ConstListIterator() {}
		ConstListIterator(const ListIterator<E> &i): element(&*i) {}
		explicit ConstListIterator(const E *e): element(e) {}

		const E &operator*() const {return *element;}
		const E *operator->() const {return element;}
		ConstListIterator &operator++() {element = element->next; return *this;}
		ConstListIterator operator++(int) {ConstListIterator i(*this); element = element->next; return i;}
		friend bool operator==(const ConstListIterator &i, const ConstListIterator &j) {return i.element == j.element;}
		friend bool operator!=(const ConstListIterator &i, const ConstListIterator &j) {return i.element != j.element;}
	};


//
// C++ I/O
//


	// A class to remember the format of an ostream so that a function may modify it internally
	// without changing it for the caller.
	class SaveFormat {
#ifndef __GNUC__ // The GCC libraries don't support ios_base yet.
		ostream &o;
		std::ios_base::fmtflags flags;
		char fill;
#endif
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


//
// Exceptions
//

	// A JavaScript exception (other than out-of-memory, for which we use the standard C++
	// exception bad_alloc).
	struct Exception {
		enum Kind {
			SyntaxError,
			StackOverflow
		};
		
		Kind kind;						// The exception's kind
		String message;					// The detailed message
		String sourceFile;				// A description of the source code that caused the error
		uint32 lineNum;					// Number of line that caused the error
		uint32 charNum;					// Character offset within the line that caused the error
		uint32 pos;						// Offset within the input of the error
		String sourceLine;				// The text of the source line

		Exception(Kind kind, const String &message): kind(kind), message(message), lineNum(0), charNum(0) {}
		Exception(Kind kind, const String &message, const String &sourceFile, uint32 lineNum, uint32 charNum, uint32 pos,
				const String &sourceLine):
			kind(kind), message(message), sourceFile(sourceFile), lineNum(lineNum), charNum(charNum), pos(pos), sourceLine(sourceLine) {}
		Exception(Kind kind, const String &message, const String &sourceFile, uint32 lineNum, uint32 charNum, uint32 pos,
				const char16 *sourceLineBegin, const char16 *sourceLineEnd):
			kind(kind), message(message), sourceFile(sourceFile), lineNum(lineNum), charNum(charNum), pos(pos),
			sourceLine(sourceLineBegin, sourceLineEnd) {}

		const char *kindString() const;
		String fullMessage() const;
	};


	// Throw a StackOverflow exception if the execution stack has gotten too large.
	inline void checkStackSize() {}
}


inline void *operator new(size_t size, JavaScript::Arena &arena) {return arena.allocate(size);}
#ifndef _WIN32		// Microsoft Visual C++ 6.0 bug: new and new[] aren't distinguished
 inline void *operator new[](size_t size, JavaScript::Arena &arena) {return arena.allocate(size);}
#endif

#ifndef __MWERKS__	// Metrowerks 5.3 bug: These aren't supported yet
 // Global delete operators.  These are only called in the rare cases that a constructor throws an exception
 // and has to undo an operator new.  An explicit delete statement will never invoke these.
 inline void operator delete(void *, JavaScript::Arena &) {}
 #ifndef _WIN32		// Microsoft Visual C++ 6.0 bug: new and new[] aren't distinguished
  inline void operator delete[](void *, JavaScript::Arena &) {}
 #endif
#endif
#endif

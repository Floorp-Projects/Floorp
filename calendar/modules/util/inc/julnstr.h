/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */
/**
 *  JulianString.h: interface for the JulianString class.
 *
 *  This class is meant to work with single byte characters and
 *  null terminated strings.  Interpretation of characters is
 *  accomplished with defines in <ctype.h>.
 *
 *  This class assumes 0 terminated strings that do not contain
 *  embedded null bytes.  Methods that assume a character
 *  set assume the us-ascii characters.  If you want I18N support,
 *  and arbitrary character set support, use UnicodeString.
 *
 *  The character buffer pointer should never be NULL (0). If it
 *  is NULL, an allocation error has occurred.  Since this class
 *  does not throw exceptions, you'll just have to check by
 *  calling IsValid().
 *
 *  Access to the internal character buffer is granted and the
 *  caller can easily abuse it. A goal of this class is to be
 *  high performance, so it assumes that anyone who uses the
 *  pointer to the internal character array will do so
 *  responsibly. This means:
 *
 *      1. don't exceed the string's buffer size
 *      2. if the string buffer needs to be bigger, call 
 *         Realloc() or ReallocTo()
 *      3. if the string length changes or you make any changes 
 *         to the string's contents via its GetBuffer() pointer, 
 *         call DoneWithBuffer() when you're finished.
 *      
 *  -sman
 */

#if !defined(_JULIANSTRING_H__7E478BB1_BFAA_11D1_8E18_0060088A4B1D__INCLUDED_)
#define _JULIANSTRING_H__7E478BB1_BFAA_11D1_8E18_0060088A4B1D__INCLUDED_

#include <ctype.h>
#include <nspr.h>

#ifdef NSPR20
#include "plstr.h"
#endif

#include "julnstr.h"
#include "nscalutilexp.h"

class NS_CAL_UTIL JulianString  
{

private:
    char*  m_pBuf;               // pointer to the char buf
    size_t m_iBufSize;           // size of char buf
    size_t m_iStrlen;            // length of the string
    size_t m_iGrowBy;            // resize by this much more than needed
    int32  m_HashCode;           // something unique

public:
    /**
     *  Empty constructor:  JulianString x;
     */
    JulianString();

    /**
     *  JulianString x("hello, world");
     *  @param p          the string to copy as initial value
     *  @param iGrowBy    When resizing, make buffer this much 
     *                    larger than needed.  Default value is 32.
     */
	  JulianString(const char* p, size_t iGrowBy=32);

    /**
     *  JulianString x("hello, world");
     *  JulianString y(x);
     *  @param s          The string to copy as an initial value
     *  @param iGrowBy    When resizing, make buffer this much 
     *                    larger than needed.  Default value is 32.
     */
    JulianString(const JulianString& s,size_t iGrowBy=32);

    /**
     *  Create a new JulianString with a buffer capacity equal to
     *  the supplied iBufSize.
     *
     *  Example:
     *    JulianString x(2048);
     *  
     *  @param iBufSize   Initial buffer size
     *  @param iGrowBy    When resizing, make buffer this much 
     *                    larger than needed.  Default value is 32.
     */
    JulianString(size_t iBufSize,size_t iGrowBy=32);

    /**
     *  Destroy a JulianString. This deletes the character
     *  buffer used internally.
     */
	  virtual ~JulianString();
    
    /**
     *  Copy the supplied null terminated string into *this.
     *  @param s  the character array to be copied.
     *  @return   a reference to *this
     */
    JulianString&  operator=(const char* s);

    /**
     *  Copy the supplied JulianString into *this.
     *  @param s  the string to be copied.
     *  @return   a reference to *this
     */
    const JulianString& operator=(const JulianString& s);

    /**
     *  Append the supplied string to *this
     *  @param s  the string to append
     *  @return   a reference to *this
     */
    JulianString& operator+=(JulianString&);

    /**
     *  Append the supplied string to *this
     *  @param s  the string to append
     *  @return   a reference to *this
     */
    JulianString& operator+=(const char *);

    /**
     * create a substring from the supplied index and count. If the
     * index is negative, the characters are indexed from the right
     * side of the string.  That is:
     *
     * <pre>
     *   JulianString x("012345");
     *   x(2,3) returns a JulianString containing "234"
     *   x(-2,2) returns a JulianString containing "45"
     * 
     *
     * @param iIndex  index of first character of the sub
     * @param iCount  number of characters to copy
     * @return        a substring
     */
    JulianString operator()(int32 iIndex, int32 iCount);

    /**
     * return the character at the supplied index. If 
     * index is negative, the characters are indexed from the right
     * side of the string.  That is:
     *
     * <pre>
     *   JulianString x("012345");
     *   x[2] returns '2'
     *   x[-1] returns '5'
     * </pre>
     *
     * @param iIndex  index of first character of the sub
     * @return        a character
     */
    char operator[](int32 iIndex);

    /**
     * return a JulianString that is the substring defined by 
     * the two supplied indeces. If either index is negative
     * the return value is an empty string.  If i is greater
     * than j the return value is an empty string. If i is
     * past the end of the string the return value is an
     * empty string. If j is past the end of the string then
     * the return value is the string from i to the end of
     * the string. Examples:
     *
     * <pre>
     *   JulianString x("012345");
     *   x[2,4] returns a JulianString containing "234"
     * </pre>
     *
     * @param i   index of first character of the sub
     * @param j   index of the last character of the sub
     * @return    a JulianString
     */
    JulianString indexSubstr(int32 iIndex, int32 iCount);

    /**
     *  Encode illegal characters to something suitable for a URL
     */
    void URLEncode();

    /**
     *  Decode URLEncoded characters.
     */
    void URLDecode();

    /**
     * return the leftmost n characters as a substring. If
     * n is < 0 it is the same as calling Right(n).
     *
     * Examples:
     *
     * <pre>
     *   JulianString x("012345");
     *   x.Left(4) returns a JulianString containing "0123"
     *   x.Left(-4) returns a JulianString containing "2345"
     * </pre>
     *
     * @param n   number of characters to return
     * @return    a JulianString
     */
    JulianString Left(int32 n);

    /**
     * return the rightmost n characters as a substring. If
     * n is < 0 it is the same as calling Left(n).
     *
     * Examples:
     *
     * <pre>
     *   JulianString x("012345");
     *   x.Right(4) returns a JulianString containing "2345"
     *   x.Right(-4) returns a JulianString containing "0123"
     * </pre>
     *
     * @param n   number of characters to return
     * @return    a JulianString
     */
    JulianString Right(int32 n);

    /**
     *  Concatenate two strings and return the result.
     *  @param s1  one of the strings to concatenate
     *  @param s2  the other string to concatenate
     *  @return    the concatenated string.
     */
    JulianString operator+(JulianString&);

    /**
     *  compare this string to a char*
     *  @param x   the Julian string
     *  @param p   the string to compare to the JulianString
     *  @return    1 if they are equal, 0 if not.
     */
    int operator==(const char* y);

    /**
     *  compare this string to another
     *  @param x   one JulianString
     *  @param y   the other JulianString
     *  @return    1 if they are equal, 0 if not.
     */
    int operator==(JulianString& y);

    /**
     *  compare this string to a char*
     *  @param x   the Julian string
     *  @param p   the string to compare to the JulianString
     *  @return    0 if they are equal, 1 if not.
     */
    int operator!=(const char* y);
    
    /**
     *  Compare one JulianString to another
     *  @param x   one string
     *  @param y   the other string
     *  @return    0 if they are equal, 1 if not.
     */
    int operator!=(JulianString& y);

    /**
     *  Return a pointer to the internal character buffer used
     *  to maintain the string. If the characters are changed
     *  in any way, be sure to call DoneWithBuffer().  If the
     *  buffer needs to be resized, be sure to use Realloc() or
     *  ReallocTo().
     *  @return    pointer to the charcter buffer
     */
    char*  GetBuffer() const { return m_pBuf; }

    /**
     *  Return a pointer to internal character buffer starting
     *  at character index i. If the characters are changed
     *  in any way, be sure to call DoneWithBuffer().  If the
     *  buffer needs to be resized, be sure to use Realloc() or
     *  ReallocTo().
     *  @return    pointer to the charcter buffer
     */
    char*  GetBuffer(size_t i) const { return &m_pBuf[i]; }

    /**
     *  @return the length of the string in bytes.
     */
    size_t GetStrlen() const { return m_iStrlen; }

    /**
     *  @return the current size (capacity) of the character buffer.
     */
    size_t GetBufSize() const { return m_iBufSize; }

    /**
     *  @return TRUE if the character buffer is valid, FALSE
     *          if it is invalid (NULL).
     */
    XP_Bool IsValid() const { return (0 == m_pBuf) ? FALSE : TRUE; }

    /**
     *  When the string length changes, presumably through
     *  external use of GetBuffer(), the internal string length
     *  is set by calling this method.  Also, the hash code is
     *  reset since we have no idea what was done to the string.
     */
    void DoneWithBuffer();

    /**
     *  Verify that there is enough room to hold iLen bytes in
     *  the current buffer. If not, reallocate the buffer. This
     *  routine does not copy the old contents into the new buffer.
     *
     *  @param iLen  the size of the buffer needed
     */
    void ReallocTo(size_t iLen);

    /**
     *  Verify that there is enough room to hold iLen bytes in
     *  the current buffer. If not, reallocate the buffer. Copy
     *  the old contents into the new buffer.
     *
     *  @param iLen  the size of the buffer needed
     */
    void Realloc(size_t);

    /**
     *  compare the supplied string to *this
     *  @param p   the string to compare to *this
     *  @param ic  if true ignore case
     *  @return    -1 if the supplied string is lexicographically
     *             less than *this, 0 if they are equal, and 1 if
     *             the supplied string is greater than *this.
     */
    int32 CompareTo(const char* p, XP_Bool ic=FALSE) const;

    /**
     *  compare the supplied JulianString to *this
     *  @param s   the JulianString to compare to *this
     *  @param ic  if true ignore case
     *  @return    -1 if the supplied string is lexicographically
     *             less than *this, 0 if they are equal, and 1 if
     *             the supplied string is greater than *this.
     */
    int32  CompareTo(JulianString& s, XP_Bool ic=FALSE) const;

    /**
     *  Return the first index of the supplied string.
     *  @param p        the string to look for
     *  @param iOffset  offset within p to begin search
     *  @return         -1 if the string is not found
     *                  the offset if >= 0
     */
    int32  IndexOf(const char* p, int32 iOffset = 0) const;       

    /**
     * return the hash code. Calculate if needed.
     * @return the hash code.
     */
    int32  HashCode();

    /**
     *  remove leading and trailing whitespace.
     */
    void   Trim();

    /**
     *  Upshift the string.  The upshifting is based on toupper()
     *  in <ctype.h> 
     */
    void   ToUpper();

    /**
     *  Downshift the string.  The downshifting is based on tolower()
     *  in <ctype.h> 
     */
    void   ToLower();

    /**
     *  Get the string from application resources 
     */
    char *   GetString(int i);

    /**
     *  Get the string from application resources and load into this
     */
    JulianString& LoadString(int i);

    /**
     *  Find the first occurrence of a string within this string
     *  @param p  the string to search for
     *  @param i  the offset within this string to begin the search
     *  @return   the index where the string was found.  -1 means
     *            the string was not found.
     */
    int32 Find(const char* p, int32 i=0) const;

    /**
     *  Find the first occurrence of a string within this string
     *  @param c  the character to search for
     *  @param i  the offset within this string to begin the search
     *  @return   the index where the string was found.  -1 means
     *            the string was not found.
     */
    int32 Find(char c, int32 iOffset=0) const;

#ifdef MOZ_TREX
   /**
     *  Find the last occurrence of a string within this string
     *  @param p  the string to search for
     *  @param i  the offset within this string to begin the search
     *  @return   the index where the string was found.  -1 means
     *            the string was not found.
     */
    int32 RFind(const char* p, int32 i=0) const;

    /**
     *  Find the last occurrence of a string within this string
     *  @param c  the character to search for
     *  @param i  the offset within this string to begin the search
     *  @return   the index where the string was found.  -1 means
     *            the string was not found.
     */
    int32 RFind(char c, int32 iOffset=0) const;
#endif

    /**
     *  Prepend the supplied string to this
     *  @param p  string to prepend
     *  @return   *this
     */
    JulianString& Prepend(const char* p);

    /**
     *  Prepend the supplied string to this
     *  @param p  string to prepend
     *  @return   *this
     */
    JulianString& Prepend(JulianString& p);

    /**
     *  Replace the substring from index m to n with the supplied
     *  string. Any index less than 0 will be snapped to 0. Any
     *  index > than the last index in the current string will be
     *  snapped to the last index. If the current string is empty
     *  or uninitialized, it is simply set to the value of *p.
     *  If m > n the values are swapped.
     *
     *  @param m  starting index of substring
     *  @param n  ending index of substring
     *  @param p  string to insert
     *  @return   *this
     */
    JulianString& Replace(int32 m, int32 n, char* p);

    /**
     *  Replace the substring from index m to n with the supplied
     *  string. Any index less than 0 will be snapped to 0. Any
     *  index > than the last index in the current string will be
     *  snapped to the last index. If the current string is empty
     *  or uninitialized, it is simply set to the value of *p.
     *  If m > n the values are swapped.
     *
     *  @param m  starting index of substring
     *  @param n  ending index of substring
     *  @param p  string to insert
     *  @return   *this
     */
    JulianString& Replace(int32 m, int32 n, JulianString& p);

    /**
     *  Starting at index i, find the first occurrence of any character
     *  in p.
     *  @param i  the offset within this string to begin the search
     *  @param p  pointer to a null terminated list of characters 
     *  @return   the index where the string was found.  -1 means
     *            the string was not found.
     */
    int32 Strpbrk( int32 i, const char *p ) const;

#if 0
    /* fails on BSD_3861.1 && SunOS4.1.3_U1 comment if 0 for now */
    friend ostream&     operator<<(ostream&, JulianString&);
    friend istream&     operator>>(istream&, JulianString&);
#endif
};


#endif /* !defined(_JULIANSTRING_H__7E478BB1_BFAA_11D1_8E18_0060088A4B1D__INCLUDED_) */

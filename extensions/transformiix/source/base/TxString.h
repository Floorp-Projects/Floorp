/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Contributor(s):
 *
 * Tom Kneeland
 *    -- original author.
 *
 * Keith Visco <kvisco@ziplink.net>
 * Larry Fitzpatrick
 *
 */

#ifndef txString_h__
#define txString_h__

#include "baseutils.h"

#ifdef TX_EXE
#include "TxObject.h"
#include <iostream.h>
typedef unsigned short UNICODE_CHAR;
const PRInt32 kNotFound = -1;
#else
#include "nsString.h"
typedef PRUnichar UNICODE_CHAR;
#endif

class String
#ifdef TX_EXE
: public TxObject
#endif
{
public:
    /*
     * Default constructor.
     */
    String();

    /*
     * Copying constructor.
     */
    String(const String& aSource);

#ifdef TX_EXE
    /*
     * Constructor, allocates a buffer and copies the supplied string buffer.
     * If aLength is zero it computes the length from the supplied string.
     */
    explicit String(const UNICODE_CHAR* aSource, PRUint32 aLength = 0);
#else
    explicit String(const nsAString& aSource);
#endif
    ~String();

    /*
     * Append aSource to this string.
     */
    void append(UNICODE_CHAR aSource);
    void append(const String& aSource);
    void append(const UNICODE_CHAR* aSource, PRUint32 aLength);
#ifndef TX_EXE
    void append(const nsAString& aSource);
#endif

    /*
     * Insert aSource at aOffset in this string.
     */
    void insert(PRUint32 aOffset, UNICODE_CHAR aSource);
    void insert(PRUint32 aOffset, const String& aSource);

    /*
     * Replace characters starting at aOffset with aSource.
     */
    void replace(PRUint32 aOffset, UNICODE_CHAR aSource);
    void replace(PRUint32 aOffset, const String& aSource);

    /*
     * Delete aCount characters starting at aOffset.
     */
    void deleteChars(PRUint32 aOffset, PRUint32 aCount);

    /*
     * Returns the character at aIndex. Caller needs to check the
     * index for out-of-bounds errors.
     */
    UNICODE_CHAR charAt(PRUint32 aIndex) const;

    /*
     * Clear the string.
     */
    void clear();

    /*
     * Returns index of first occurrence of aData.
     */
    PRInt32 indexOf(UNICODE_CHAR aData,
                    PRInt32 aOffset = 0) const;
    PRInt32 indexOf(const String& aData, PRInt32 aOffset = 0) const;

    /*
     * Returns index of last occurrence of aData.
     */
    PRInt32 lastIndexOf(UNICODE_CHAR aData,
                        PRInt32 aOffset = 0) const;

    /*
     * Check equality between strings.
     */
    MBool isEqual(const String& aData) const;

    /*
     * Check equality (ignoring case) between strings.
     */
    MBool isEqualIgnoreCase(const String& aData) const;

    /*
     * Check whether the string is empty.
     */
    MBool isEmpty() const;

    /*
     * Return the length of the string.
     */
    PRUint32 length() const;

    /*
     * Returns a substring starting at start
     * Note: the dest String is cleared before use
     */
    String& subString(PRUint32 aStart, String& aDest) const;

    /*
     * Returns the subString starting at start and ending at end
     * Note: the dest String is cleared before use
     */
    String& subString(PRUint32 aStart, PRUint32 aEnd,
                      String& aDest) const;

    /*
     * Convert string to lowercase.
     */
    void toLowerCase();

    /*
     * Convert string to uppercase.
     */
    void toUpperCase();

    /*
     * Shorten the string to aLength.
     */
    void truncate(PRUint32 aLength);

#ifdef TX_EXE
    /*
     * Assignment operator. Override default assignment operator
     * only on standalone, the default will do the right thing for
     * module.
     */
    String& operator = (const String& aSource);
#else
    /*
     * Return a reference to this string's nsString.
     */
    operator nsAString&();

    /*
     * Return a const reference to this string's nsString.
     */
    operator const nsAString&() const;
#endif

#ifndef TX_EXE
private:
    nsString mString;
#else
protected:
    /*
     * Make sure the string buffer can hold aCapacity characters.
     */
    MBool ensureCapacity(PRUint32 aCapacity);

    /*
     * Allocate a new UNICODE_CHAR buffer and copy this string's
     * buffer into it. Caller needs to free the buffer.
     */
    UNICODE_CHAR* toUnicode() const;

    /*
     * Compute the unicode length of aData.
     */
    static PRUint32 unicodeLength(const UNICODE_CHAR* aData);

    friend ostream& operator << (ostream& aOutput, const String& aSource);
    friend class txCharBuffer;

    UNICODE_CHAR* mBuffer;
    PRUint32 mBufferLength;
    PRUint32 mLength;
#endif

// XXX DEPRECATED
public:
    explicit String(PRUint32 aSize);
    explicit String(const char* aSource); // XXX Used for literal strings
    void append(const char* aSource);
    MBool isEqual(const char* aData) const;
#ifndef TX_EXE
    nsString& getNSString();
    const nsString& getConstNSString() const;
#endif
// XXX DEPRECATED
};

#ifdef TX_EXE

/*
 * Class for converting char*s into Strings
 */

class NS_ConvertASCIItoUCS2 : public String
{
public:
    explicit NS_ConvertASCIItoUCS2(const char* aSource);
};

/*
 * A helper class for getting a char* buffer out of a String.
 * Don't use this directly, use NS_LossyConvertUCS2toASCII which
 * is typedef'ed to this class on standalone and will fall back
 * on the Mozilla implementation for the Mozilla module.
 */
class txCharBuffer
{
public:
    txCharBuffer(const String& aString) : mString(aString),
                                          mBuffer(0)
    {
    };

    ~txCharBuffer()
    {
        delete [] mBuffer;
    };

    const char* get()
    {
        if (!mBuffer) {
            mBuffer = new char[mString.mLength + 1];
            NS_ASSERTION(mBuffer, "out of memory");
            if (mBuffer) {
                PRUint32 loop;
                for (loop = 0; loop < mString.mLength; ++loop) {
                    mBuffer[loop] = (char)mString.mBuffer[loop];
                }
                mBuffer[mString.mLength] = 0;
            }
        }
        return mBuffer;
    }

private:
    const String& mString;
    char* mBuffer;
};

typedef txCharBuffer NS_LossyConvertUCS2toASCII;

/*
 * Translate UNICODE_CHARs to Chars and output to the provided stream.
 */
ostream& operator << (ostream& aOutput, const String& aSource);

inline UNICODE_CHAR String::charAt(PRUint32 aIndex) const
{
  NS_ASSERTION(aIndex < mLength, "|charAt| out-of-range");
  return mBuffer[aIndex];
}

inline MBool String::isEmpty() const
{
  return (mLength == 0);
}

inline PRUint32 String::length() const
{
  return mLength;
}
#else
// txMozillaString.h contains all inline implementations for the 
// Mozilla module.
#include "txMozillaString.h"
#endif

#endif // txString_h__

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
 * Please see release.txt distributed with this file for more information.
 *
 */
// Tom Kneeland (3/17/99)
//
//  Implementation of a simple string class
//
// Modification History:
// Who  When      What
// TK   03/17/99  Created
// TK   03/23/99  Released without "lastIndexOf" functions
// TK   04/02/99  Added support for 'const' strings, and added
//                'operator=' for constant char*.
// TK   04/09/99  Overloaded the output operator (<<).  Currently it only
//                supports outputing the String to a C sytle character based
//                stream.
// TK   04/09/99  Provided support for the extraction of the DOM_CHAR
//                representation of the string.  The new method, "toDomChar()"
//                returns a constant pointer to the internal DOM_CHAR string
//                buffer.
// TK   04/10/99  Added the implementation for appending an array of DOM_CHARs
//                to a string.  It should be noted that a length needs to be
//                provided in order to determine the length of the source
//                array.
// TK   04/22/99  Fixed a bug where setting a string equal to NULL would cause
//                a core dump.  Also added support for constructing a string
//                using the NULL identifier.
//                Modified the output operator (<<) to accept a const String
//                reference.  This eliminates a wasteful copy constructor call.
// TK   04/28/99  Modified the clear() method to leave the DOM_CHAR array
//                in place.
// TK   04/28/99  Added 3 new member functions: insert, deleteChars, and
//                replace.
// TK   05/05/99  Added support for implicit integer conversion.  This allows
//                integers to be appended, inserted, and used as replacements
//                for DOM_CHARs. To support this feature, ConvertInt has been
//                added which converts the given integer to a string and stores
//                it in the target.
// TK   05/05/99  Converted the typedef DOM_CHAR to UNICODE_CHAR.
//
// KV   07/29/99  Added lastIndexOf methods
// KV   07/29/99  Changed indexOf methods with no offset, to call the
//                indexOf methods with offset of 0. This allows re-use of
//                code, makes it easier to debug, and minimizes the size of
//                the implementation
// LF  08/06/1999   In method #operator=,
//                  added line: return *this
// KV  08/11/1999  changed charAt to return -1, if index is out of bounds, instead of 0,
//                 since 0, is a valid character, and this makes my code more compatible
//                 with Java
// KV  08/11/1999  removed PRBool, uses baseutils.h (MBool)
// TK  12/03/1999  Made some of the interface functions virtual, to support
//                 wrapping Mozilla nsStrings in a String interface
// TK  12/09/1999  Since "String" can be extended, we can not be certin of its
//                 implementation, therefore any function accepting a String
//                 object as an argument must only deal with its public
//                 interface.  The following member functions have been
//                 modified: append, insert, replace, indexOf, isEqual,
//                           lastIndexOf, and subString
//
//                 Modified subString(Int32 start, String& dest) to simmply
//                 call subString(Int32 start, Int32 end, String& dest).  This
//                 helps with code reuse.
//
//                 Made ConvetInt a protected member function so it is
//                 available to classes derrived from String.  This is possible
//                 since the implementation of ConvertInt only uses the public
//                 interface of String
//
//                 Made UnicodeLength a protected member function since it
//                 only calculates the length of a null terminated UNICODE_CHAR
//                 array.
// TK  12/17/1999  To support non-null terminated UNICODE_CHAR* arrays, an
//                 additional insert function has been added that accepts a
//                 length parameter.
//
//                 Modified append(const UNICODE_CHAR* source) to simply
//                 calculate the length of the UNICODE_CHAR array, and then
//                 defer its processing to
//                 append(const UNICODE_CHAR* source, Int32 sourceLength)
// TK  12/21/1999  To support non-null terminated UNICODE_CHAR* arrays, an
//                 additional replace function has been added that accepts a
//                 length parameter.
//
//                 Modified replace(Int32 offset, const UNICODE_CHAR* source)
//                 to simply call the new replace function passing the computed
//                 length of the null terminated UNICODE_CHAR array.
// TK  12/22/1999  Enhanced Trim() to to remove additional "white space"
//                 characters (added \n, \t, and \r).
//
// TK  02/14/2000  Added a constructon  which accepts a UNICODE_CHAR* array, and
//                 its associated length.
//
// TK  03/30/2000  Changed toChar to toCharArray and provided an overloaded
//                 version which will instantiate its own character buffer.

#ifndef MITRE_STRING
#define MITRE_STRING

#include "MITREObject.h"
#include "baseutils.h"
#include <iostream.h>

#ifdef MOZ_XSL
#include "nsString.h"
typedef PRUnichar UNICODE_CHAR;
#else
typedef unsigned short UNICODE_CHAR;
#endif

#ifndef NULL
 #define NULL 0
#endif

#define NOT_FOUND -1

class String : public MITREObject
{
  //Translate UNICODE_CHARs to Chars and output to the provided stream
  friend ostream& operator<<(ostream& output, const String& source);

  public:
    String();                     //Default Constructor, create an empty string
    String(Int32 initSize);       //Create an empty string of a specific size
    String(const String& source); //Create a copy of the source string
    String(const char* source);   //Create a string from the characters
    String(const UNICODE_CHAR* source);
    String(const UNICODE_CHAR* source, Int32 length);
#ifdef MOZ_XSL
    String(nsString* theNSString);
#endif

    ~String();                    //Destroy the string, and free memory


    //Assign source to this string
    virtual String& operator=(const String& source);
    virtual String& operator=(const char* source);
    virtual String& operator=(const UNICODE_CHAR* source);
    virtual String& operator=(Int32 source);

    //Grow buffer if necessary and append the source
    virtual void append(UNICODE_CHAR source);
    virtual void append(char source);
    virtual void append(const String& source);
    virtual void append(const char* source);
    virtual void append(const UNICODE_CHAR* source);
    virtual void append(const UNICODE_CHAR* source, Int32 length);
    virtual void append(Int32 source);

    //Provide the ability to insert data into the middle of a string
    virtual void insert(Int32 offset, const UNICODE_CHAR source);
    virtual void insert(Int32 offset, const char source);
    virtual void insert(Int32 offset, const String& source);
    virtual void insert(Int32 offset, const char* source);
    virtual void insert(Int32 offset, const UNICODE_CHAR* source);
    virtual void insert(Int32 offset, const UNICODE_CHAR* source,
                        Int32 sourceLength);
    virtual void insert(Int32 offset, Int32 source);

    //Provide the ability to replace one or more characters
    virtual void replace(Int32 offset, const UNICODE_CHAR source);
    virtual void replace(Int32 offset, const char source);
    virtual void replace(Int32 offset, const String& source);
    virtual void replace(Int32 offset, const char* source);
    virtual void replace(Int32 offset, const UNICODE_CHAR* source);
    virtual void replace(Int32 offset, const UNICODE_CHAR* source,
                         Int32 srcLength);
    virtual void replace(Int32 offset, Int32 source);

    //Provide the ability to delete a range of charactes
    virtual void deleteChars(Int32 offset, Int32 count);

    /**
     * Returns the character at index.
     * If the index is out of bounds, -1 will be returned.
    **/
    virtual UNICODE_CHAR charAt(Int32 index) const;

    virtual void clear();                 //Clear string

    virtual void ensureCapacity(Int32 capacity); //Make sure buffer is at least
                                                 //'size'

    //Returns index of first occurrence of data
    virtual Int32 indexOf(UNICODE_CHAR data) const;
    virtual Int32 indexOf(UNICODE_CHAR data, Int32 offset) const;
    virtual Int32 indexOf(const String& data) const;
    virtual Int32 indexOf(const String& data, Int32 offset) const;

    virtual MBool isEqual(const String& data) const; //Check equality between
                                                     //strings
#ifdef MOZ_XSL
    virtual MBool isEqualIgnoreCase(const String& data) const;
#endif

    //Returns index of last occurrence of data
    virtual Int32 lastIndexOf(UNICODE_CHAR data) const;
    virtual Int32 lastIndexOf(UNICODE_CHAR data, Int32 offset) const;
    virtual Int32 lastIndexOf(const String& data) const;
    virtual Int32 lastIndexOf(const String& data, Int32 offset) const;

    virtual Int32 length() const;               //Returns the length

    /**
     * Sets the Length of this String, if length is less than 0, it will
     * be set to 0; if length > current length, the string will be extended
     * and padded with '\0' null characters. Otherwise the String
     * will be truncated
    **/
    virtual void setLength(Int32 length);

    /**
     * Sets the Length of this String, if length is less than 0, it will
     * be set to 0; if length > current length, the string will be extended
     * and padded with given pad character. Otherwise the String
     * will be truncated
    **/
    virtual void setLength(Int32 length, UNICODE_CHAR padChar);

    /**
     * Returns a substring starting at start
     * Note: the dest String is cleared before use
    **/
    virtual String& subString(Int32 start, String& dest) const;

    /**
     * Returns the subString starting at start and ending at end
     * Note: the dest String is cleared before use
    **/
    virtual String& subString(Int32 start, Int32 end, String& dest) const;

    //Convert the internal rep. to a char buffer
    virtual char* toCharArray() const;
    virtual char* toCharArray(char* dest) const;
    virtual UNICODE_CHAR* toUnicode(UNICODE_CHAR* dest) const;
    virtual const UNICODE_CHAR* toUnicode() const;

    virtual void toLowerCase();           //Convert string to lowercase
    virtual void toUpperCase();           //Convert string to uppercase
    virtual void trim();                  //Trim whitespace from both ends

    virtual void reverse();               //Reverse the string

#ifdef MOZ_XSL
    virtual nsString& getNSString();
    virtual const nsString& getConstNSString() const;
#endif

  protected:
    //Convert an Int into a String
    //TK 12/09/1999 - Make this function available to Derrived classes
    String& ConvertInt(Int32 value, String& target);

    //Calculates the length of a null terminated UNICODE_CHAR array
    Int32 UnicodeLength(const UNICODE_CHAR* data);

  private:
#ifdef MOZ_XSL
    nsString* ptrNSString;
#else
    Int32     strLength;
    Int32     bufferLength;
    UNICODE_CHAR* strBuffer;
#endif

    //String copies itself to the destination
    void copyString(UNICODE_CHAR* dest);

    //Compare the two string representations for equality
    MBool isEqual(const UNICODE_CHAR* data, const UNICODE_CHAR* search,
                   Int32 length) const;
};

ostream& operator<<(ostream& output, const String& source);

#endif


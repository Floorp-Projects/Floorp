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

#ifndef MITRE_STRING
#define MITRE_STRING

#include "MITREObject.h"
#include "baseutils.h"
#include <iostream.h>


typedef unsigned short UNICODE_CHAR;

#ifndef NULL
typedef 0 NULL;
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

    ~String();                    //Destroy the string, and free memory


    //Assign source to this string
    String& operator=(const String& source);
    String& operator=(const char* source);
    String& operator=(const UNICODE_CHAR* source);
    String& operator=(Int32 source);

    //Grow buffer if necessary and append the source
    void append(const UNICODE_CHAR source);
    void append(const char source);
    void append(const String& source);
    void append(const char* source);
    void append(const UNICODE_CHAR* source);
    void append(const UNICODE_CHAR* source, Int32 length);
    void append(Int32 source);

    //Provide the ability to insert data into the middle of a string
    void insert(Int32 offset, const UNICODE_CHAR source);
    void insert(Int32 offset, const char source);
    void insert(Int32 offset, const String& source);
    void insert(Int32 offset, const char* source);
    void insert(Int32 offset, const UNICODE_CHAR* source);
    void insert(Int32 offset, Int32 source);

    //Provide the ability to replace one or more characters
    void replace(Int32 offset, const UNICODE_CHAR source);
    void replace(Int32 offset, const char source);
    void replace(Int32 offset, const String& source);
    void replace(Int32 offset, const char* source);
    void replace(Int32 offset, const UNICODE_CHAR* source);
    void replace(Int32 offset, Int32 source);

    //Provide the ability to delete a range of charactes
    void deleteChars(Int32 offset, Int32 count);

    /**
     * Returns the character at index.
     * If the index is out of bounds, -1 will be returned.
    **/
    UNICODE_CHAR charAt(Int32 index) const;

    void clear();                 //Clear string

    void ensureCapacity(Int32 capacity); //Make sure buffer is at least 'size'

    //Returns index of first occurrence of data
    Int32 indexOf(UNICODE_CHAR data) const;
    Int32 indexOf(UNICODE_CHAR data, Int32 offset) const;
    Int32 indexOf(const String& data) const;
    Int32 indexOf(const String& data, Int32 offset) const;

    MBool isEqual(const String& data) const; //Check equality between strings

    //Returns index of last occurrence of data
    Int32 lastIndexOf(UNICODE_CHAR data) const;
    Int32 lastIndexOf(UNICODE_CHAR data, Int32 offset) const;
    Int32 lastIndexOf(const String& data) const;
    Int32 lastIndexOf(const String& data, Int32 offset) const;

    Int32 length() const;               //Returns the length of the string

    /**
     * Sets the Length of this String, if length is less than 0, it will
     * be set to 0; if length > current length, the string will be extended
     * and padded with '\0' null characters. Otherwise the String
     * will be truncated
    **/
    void setLength(Int32 length);

    /**
     * Sets the Length of this String, if length is less than 0, it will
     * be set to 0; if length > current length, the string will be extended
     * and padded with given pad character. Otherwise the String
     * will be truncated
    **/
    void setLength(Int32 length, UNICODE_CHAR padChar);

    /**
     * Returns a substring starting at start
     * Note: the dest String is cleared before use
    **/
    String& subString(Int32 start, String& dest) const;

    /**
     * Returns the subString starting at start and ending at end
     * Note: the dest String is cleared before use
    **/
    String& subString(Int32 start, Int32 end, String& dest) const;

    //Convert the internal rep. to a char buffer
    char* toChar(char* dest) const;
    UNICODE_CHAR* toUnicode(UNICODE_CHAR* dest) const;

    void toLowerCase();           //Convert string to lowercase
    void toUpperCase();           //Convert string to uppercase
    void trim();                  //Trim whitespace from both ends of string

    void reverse();               //Reverse the string

  private:
    Int32     strLength;
    Int32     bufferLength;
    UNICODE_CHAR* strBuffer;

    //String copies itself to the destination
    void copyString(UNICODE_CHAR* dest);

    //Compare the two string representations for equality
    MBool isEqual(const UNICODE_CHAR* data, const UNICODE_CHAR* search,
                   Int32 length) const;

    //Convert an Int into a String
    String& ConvertInt(Int32 value, String& target);

    //Calculates the length of a null terminated UNICODE_CHAR array
    Int32 UnicodeLength(const UNICODE_CHAR* data);
};

ostream& operator<<(ostream& output, const String& source);

#endif

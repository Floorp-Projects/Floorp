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

// Tom Kneeland (12/03/1999)
//
//  Wrapper class to convert nsString, into a MITRE/TransforMIIX compatiable
//  string.
//
// Modification History:
// Who  When        What
// TK   01/13/2000  Added a means to retrieve the nsString object.  This
//                  provides an efficient way to retreive nsString data from
//                  Mozilla functions expecting nsString references as
//                  destination objects (like nsIDOMNode::GetNodeName)
//                  Also provided a means to obtain a const reference to the
//                  nsString object to support const object/references.
//
// TK  03/30/2000  Changed toChar to toCharArray and provided an overloaded
//                 version which will instantiate its own character buffer.

#ifndef MITRE_MOZILLA_STRING
#define MITRE_MOZILLA_STRING

#include "String.h"
#include "MITREObject.h"
#include "baseutils.h"
#include "nsString.h"
#include <iostream.h>

typedef unsigned long SPECIAL_CHAR;

class MozillaString : public String
{
  //Translate nsString to Chars and output to the provided stream
  friend ostream& operator<<(ostream& output, const String& source);

  public:
    //Default constructor ( nsString() )
    //
    MozillaString();

    //Create a new sting by assuming control of the provided nsString
    //
    MozillaString(nsString* theNSString);

    //Create an empty string of a specific size
    //( nsString(), SetCapacity(initSize) )
    //
    MozillaString(Int32 initSize);

    //Create a copy of the source string
    //
    MozillaString(const MozillaString& source);

    //Create a string from the characters ( nsString(source, -1) )
    //
    MozillaString(const char* source);

    //Create a string from the Unicode Characters
    //( nsString(source, length(source)) )
    //
    MozillaString(const UNICODE_CHAR* source, Int32 srcLength);

    //Create a string from the given String object.
    //Use the UNICODE_CHAR* representation of the string to perform the copy
    //( nsString(source, length(source)) )
    MozillaString(const String& source);

    //Destroy the ptrNSString;
    //
    ~MozillaString();

    //Assign source to this string
    virtual String& operator=(const String& source);
    virtual String& operator=(const char* source);
    virtual String& operator=(Int32 source);
    String& operator=(const MozillaString& source);

    //Append methods for various inputs
    virtual void append(UNICODE_CHAR source);
    virtual void append(char source);
    virtual void append(const String& source);

    //Need to provide a means to append one mozstring to another.  This seems
    //to be necessary because nsString seems to get confused if its own
    //Unicode buffer is passed itself for appending (ie a MozString is appended
    //to itself using on the functions provided by the public String
    //interface).
    void append(const MozillaString& source);

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
     void insert(Int32 offset, const UNICODE_CHAR* source,
                        Int32 srcLength);
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

    //Returns index of last occurrence of data
    virtual Int32 lastIndexOf(UNICODE_CHAR data) const;
    virtual Int32 lastIndexOf(UNICODE_CHAR data, Int32 offset) const;
    virtual Int32 lastIndexOf(const String& data) const;
    virtual Int32 lastIndexOf(const String& data, Int32 offset) const;

    virtual Int32 length() const;                //Returns the length

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

    virtual nsString& getNSString();
    virtual const nsString& getConstNSString() const;

  private:
    nsString* ptrNSString;

    //String copies itself to the destination
    //void copyString(SPECIAL_CHAR* dest);
};
#endif

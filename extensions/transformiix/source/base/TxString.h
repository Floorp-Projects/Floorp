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

#ifndef MITRE_STRING
#define MITRE_STRING

#include "TxObject.h"
#include "baseutils.h"
#include <iostream.h>

#ifdef TX_EXE
typedef unsigned short UNICODE_CHAR;
#else
#include "nsString.h"
typedef PRUnichar UNICODE_CHAR;
#endif

#ifndef NULL
 #define NULL 0
#endif

#define NOT_FOUND -1

class String : public TxObject
{
  //Translate UNICODE_CHARs to Chars and output to the provided stream
  friend ostream& operator<<(ostream& output, const String& source);

  public:
    String();                     //Default Constructor, create an empty string
    String(PRInt32 initSize);       //Create an empty string of a specific size
    String(const String& source); //Create a copy of the source string
    String(const char* source);   //Create a string from the characters
    String(const UNICODE_CHAR* source);
    String(const UNICODE_CHAR* source, PRInt32 length);
#ifndef TX_EXE
    String(nsString* theNSString);
#endif

    ~String();                    //Destroy the string, and free memory


    //Assign source to this string
    virtual String& operator=(const String& source);
    virtual String& operator=(const char* source);
    virtual String& operator=(const UNICODE_CHAR* source);
    virtual String& operator=(PRInt32 source);

    //Grow buffer if necessary and append the source
    virtual void append(UNICODE_CHAR source);
    virtual void append(char source);
    virtual void append(const String& source);
    virtual void append(const char* source);
    virtual void append(const UNICODE_CHAR* source);
    virtual void append(const UNICODE_CHAR* source, PRInt32 length);
    virtual void append(PRInt32 source);

    //Provide the ability to insert data into the middle of a string
    virtual void insert(PRInt32 offset, const UNICODE_CHAR source);
    virtual void insert(PRInt32 offset, const char source);
    virtual void insert(PRInt32 offset, const String& source);
    virtual void insert(PRInt32 offset, const char* source);
    virtual void insert(PRInt32 offset, const UNICODE_CHAR* source);
    virtual void insert(PRInt32 offset, const UNICODE_CHAR* source,
                        PRInt32 sourceLength);
    virtual void insert(PRInt32 offset, PRInt32 source);

    //Provide the ability to replace one or more characters
    virtual void replace(PRInt32 offset, const UNICODE_CHAR source);
    virtual void replace(PRInt32 offset, const char source);
    virtual void replace(PRInt32 offset, const String& source);
    virtual void replace(PRInt32 offset, const char* source);
    virtual void replace(PRInt32 offset, const UNICODE_CHAR* source);
    virtual void replace(PRInt32 offset, const UNICODE_CHAR* source,
                         PRInt32 srcLength);
    virtual void replace(PRInt32 offset, PRInt32 source);

    //Provide the ability to delete a range of charactes
    virtual void deleteChars(PRInt32 offset, PRInt32 count);

    /**
     * Returns the character at index.
     * If the index is out of bounds, -1 will be returned.
    **/
    virtual UNICODE_CHAR charAt(PRInt32 index) const;

    virtual void clear();                 //Clear string

    virtual void ensureCapacity(PRInt32 capacity); //Make sure buffer is at least
                                                 //'size'

    //Returns index of first occurrence of data
    virtual PRInt32 indexOf(UNICODE_CHAR data) const;
    virtual PRInt32 indexOf(UNICODE_CHAR data, PRInt32 offset) const;
    virtual PRInt32 indexOf(const String& data) const;
    virtual PRInt32 indexOf(const String& data, PRInt32 offset) const;

    virtual MBool isEqual(const String& data) const; //Check equality between
                                                     //strings
#ifndef TX_EXE
    virtual MBool isEqualIgnoreCase(const String& data) const;
#endif

    //Returns index of last occurrence of data
    virtual PRInt32 lastIndexOf(UNICODE_CHAR data) const;
    virtual PRInt32 lastIndexOf(UNICODE_CHAR data, PRInt32 offset) const;
    virtual PRInt32 lastIndexOf(const String& data) const;
    virtual PRInt32 lastIndexOf(const String& data, PRInt32 offset) const;

    //Checks whether the string is empty
    virtual MBool isEmpty() const;

    virtual PRInt32 length() const;               //Returns the length

    /**
     * Sets the Length of this String, if length is less than 0, it will
     * be set to 0; if length > current length, the string will be extended
     * and padded with '\0' null characters. Otherwise the String
     * will be truncated
    **/
    virtual void setLength(PRInt32 length);

    /**
     * Sets the Length of this String, if length is less than 0, it will
     * be set to 0; if length > current length, the string will be extended
     * and padded with given pad character. Otherwise the String
     * will be truncated
    **/
    virtual void setLength(PRInt32 length, UNICODE_CHAR padChar);

    /**
     * Returns a substring starting at start
     * Note: the dest String is cleared before use
    **/
    virtual String& subString(PRInt32 start, String& dest) const;

    /**
     * Returns the subString starting at start and ending at end
     * Note: the dest String is cleared before use
    **/
    virtual String& subString(PRInt32 start, PRInt32 end, String& dest) const;

    //Convert the internal rep. to a char buffer
    virtual char* toCharArray() const;
    virtual char* toCharArray(char* dest) const;
    virtual UNICODE_CHAR* toUnicode(UNICODE_CHAR* dest) const;
    virtual const UNICODE_CHAR* toUnicode() const;

    virtual void toLowerCase();           //Convert string to lowercase
    virtual void toUpperCase();           //Convert string to uppercase
    virtual void trim();                  //Trim whitespace from both ends

    virtual void reverse();               //Reverse the string

#ifndef TX_EXE
    virtual nsString& getNSString();
    virtual const nsString& getConstNSString() const;
#endif

  protected:
    //Convert an Int into a String
    //TK 12/09/1999 - Make this function available to Derrived classes
    String& ConvertInt(PRInt32 value, String& target);

    //Calculates the length of a null terminated UNICODE_CHAR array
    PRInt32 UnicodeLength(const UNICODE_CHAR* data);

  private:
#ifndef TX_EXE
    nsString* ptrNSString;
#else
    PRInt32     strLength;
    PRInt32     bufferLength;
    UNICODE_CHAR* strBuffer;
#endif

    //String copies itself to the destination
    void copyString(UNICODE_CHAR* dest);

    //Compare the two string representations for equality
    MBool isEqual(const UNICODE_CHAR* data, const UNICODE_CHAR* search,
                   PRInt32 length) const;
};

ostream& operator<<(ostream& output, const String& source);

#endif


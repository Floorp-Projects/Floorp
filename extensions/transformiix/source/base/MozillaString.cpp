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
// JS   20/09/2000  Modified charAt() to return -1 if outof range. Matches TxString
// TK   01/13/2000  Added a means to retrieve the nsString object.  This
//                  provides an efficient way to retreive nsString data from
//                  Mozilla functions expecting nsString references as
//                  destination objects (like nsIDOMNode::GetNodeName)

#include <stdlib.h>
#include <string.h>
#include "TxString.h"
#include <iostream.h>

//
//Default constructor ( nsString() )
//
String::String()
{
  ptrNSString = new nsString();
}

//
//Create an nsString with the specified size
//
String::String(Int32 initSize)
{
  ptrNSString = new nsString();
  ptrNSString->SetCapacity(initSize);
}

//
//Create an nsString from the provided String object
//Use the Unicode representation to perform the copy
// (nsString(source, length(source)) )
String::String(const String& source)
{
  ptrNSString = new nsString(*source.ptrNSString);
}

//
//Create a string from the characters ( nsString(source, -1) )
//  NOTE: Since, by definition, this C Style string is null terminated, simply
//        accept the default length (-1) to the nsString constructor, and let
//        it calculate its length.
//
//
String::String(const char* source)
{
  ptrNSString = new nsString();
  ptrNSString->AssignWithConversion(source);
}

//
//Create a string from the Unicode Characters
//( nsString(source, length(source)) )
//  NOTE: The length passed to this constructor does not include the NULL
//        terminator (in C fashion).
//
String::String(const UNICODE_CHAR* source)
{
  ptrNSString = new nsString((PRUnichar *)source);
}

//
//Create a string from the Unicode Characters
//( nsString(source, length(source)) )
//  NOTE: The length passed to this constructor does not include the NULL
//        terminator (in C fashion).
//
String::String(const UNICODE_CHAR* source, Int32 srcLength)
{
  ptrNSString = new nsString((PRUnichar *)source, srcLength);
}

//
//Create a new sting by assuming control of the provided nsString
//
String::String(nsString* theNSString)
{
  ptrNSString = theNSString;
}

//
//Destroy the nsString, and free memory
//
String::~String()
{
  delete ptrNSString;
}

//
//Convert the UNICODE_CHARs of this String to Chars, and output them to the given
//ostream
//
ostream& operator<<(ostream& output, const String& source)
{
  Int32 outputLoop;

  for (outputLoop=0;outputLoop<source.length();outputLoop++)
    output << (char)source.charAt(outputLoop);

  return output;
}

//
//Overloaded '=' operator to assign the value of the source string to this
//string.  Only use String's public interface to ensure compatibility with
//String, and any other object derrived from the String
//interface.  ( nsString::Assign(PRUnichar*, PRInt32) )
//
String& String::operator=(const String& source)
{
  //Assign the Unicode Char buffer to the nsString
  ptrNSString->Assign((PRUnichar *)source.toUnicode(), source.length());
  return *this;
}

//
//Overloaded '=' operator to assign the value of the source C string to this
//string.  ( nsString::Assign(const char*, PRInt32) )
//
String& String::operator=(const char* source)
{
  ptrNSString->AssignWithConversion(source);
  return *this;
}

//
//Overloaded '=' operator to assign the value of a UNICODE_CHAR string to this
//string.  Note:  The soucre is "NULL" terminated.
//
String& String::operator=(const UNICODE_CHAR* source)
{
  ptrNSString->Assign((PRUnichar *)source);
  return *this;
}

//
//Overloaded '=' operator to assign an integer to this string.
//
String& String::operator=(Int32 source)
{
  //Since String::ConvertInt only uses String's public interface, use it to
  //convert "source", and store it in this object
  return ConvertInt(source, *this);
} //-- operator=

//
//Append the source character ( nsString::Append(PRUnichar) )
//
void String::append(UNICODE_CHAR source)
{
  ptrNSString->Append((PRUnichar)source);
}

//
//Append a character to the string (nsString::Append(char) )
//
void String::append(char source)
{
  ptrNSString->AppendWithConversion(source);
}

//
//Append String.  Only use String's public interface to ensure compatibility
//with all classes derrived from String.
//Ultimately use ( nsString::Append(const PRUnichar*, PRInt32) ) or
//               ( nsString::Append(const nsString&) )
//
void String::append(const String& source)
{
  //There are issues if we try to append a string to itself using its unicode
  //buffer!  So if the provided source object is equal to this, then we are
  //appending this String to itself, so cast source to a String
  //object, and go after its nsString implementation.
  if (this == &source)
    ptrNSString->Append(*((String)source).ptrNSString);
  else
    ptrNSString->Append((PRUnichar *)source.toUnicode(), source.length());
}

//
//Append a string of characters (null terminated arry of chars)
//( nsString::Append(const char*, PRInt32) )
//
void String::append(const char* source)
{
  ptrNSString->AppendWithConversion(source);
}

//
//Append a string of unicode chars (null terminated array of Unicode chars)
//( nsString::Append(const PRUnichar*, PRInt32) )
//
void String::append(const UNICODE_CHAR* source)
{
  ptrNSString->Append((PRUnichar *)source, UnicodeLength(source));
}

//
//Append a string of DOM Characters whose length is also defined
//( nsString::Append(const PRUnichar*, PRInt32) )
//
void String::append(const UNICODE_CHAR* source, Int32 length)
{
  ptrNSString->Append((PRUnichar *)source, length);
}

//
//Convert source from an integer to a string, and append it to the current
//string.  ( nsString::Append(PRInt32, PRInt32 aRadix=10) )
//
void String::append(Int32 source)
{
  ptrNSString->AppendInt(source);
}

//
//Insert a single UNICODE_CHAR into the string starting at offset
//( nsString::Insert(PRUnichar, PRUint32) )
//
void String::insert(Int32 offset, const UNICODE_CHAR source)
{
  ptrNSString->Insert((PRUnichar)source, offset);
}

//
//Insert a single C type character into the string starting at offset
//nsString does not seem to support the insertion of a char (it seems to be
//commented out) so just use nsString::Insert(PRUnichar, PRUint32).
//
void String::insert(Int32 offset, const char source)
{
  ptrNSString->Insert((PRUnichar)source, offset);
}

//
//Insert the source string starting at the current offset
//Only use the public interface of source, since we must support all classes
//derrived from String.
//( nsString::Insert(const PRUnichar*, PRuint32, PRInt32) )
//
void String::insert(Int32 offset, const String& source)
{
  //There are issues if we try to insert a string into itself using its unicode
  //buffer!  So if the provided source object is equal to this, then we are
  //appending this String to itself, so cast source to a String
  //object, and go after its nsString implementation.
  if (this == &source)
    ptrNSString->Insert(*((String)source).ptrNSString, offset);
  else
    ptrNSString->Insert((PRUnichar *)source.toUnicode(), offset, source.length());
}

//
//Insert the source "C" type string into this string starting at offset.
//( nsString::Insert(const char*, PRUint32, PrInt32) )
//
void String::insert(Int32 offset, const char* source)
{
  ptrNSString->InsertWithConversion(source, offset);
}

//
//Insert the source UNICODE_CHAR type string into this string starting at
//offset.  Note that the source is Null Terminated.
//( nsString::Insert(const PRUnichar*, PRuint32, PRInt32) )
//
void String::insert(Int32 offset, const UNICODE_CHAR* source)
{
  ptrNSString->Insert((PRUnichar *)source, offset, UnicodeLength(source));
}

//
//Insert the source UNICODE_CHAR type string into this string starting at
//offset.  Note that the array is not null terminated, so the lenght must be
//provided.
//
void String::insert(Int32 offset, const UNICODE_CHAR* source,
                           Int32 srcLength)
{
  ptrNSString->Insert((PRUnichar *)source, offset, srcLength);
}

//
//Convert source from an integer to a string, and then insert.
//
void String::insert(Int32 offset, Int32 source)
{
  String convertString;

  insert(offset, ConvertInt(source, convertString));
}

//
//Replace the character specified by offset with the UNICODE_CHAR source
//
void String::replace(Int32 offset, const UNICODE_CHAR source)
{
  replace(offset, &source, 1);
}

//
//Replace the character specified by offset with the C style character source
//
void String::replace(Int32 offset, const char source)
{
  replace(offset, (UNICODE_CHAR)source);
}

//
//Replace the substring starting at offset with the String specified by source.
//
void String::replace(Int32 offset, const String& source)
{
  Int32 numToCut = 0;

  //There are issues if we try to replace a string using a portion of itself
  //using its unicode buffer!  So to try and be efficient, if source is equal
  //to this, we will insert source at offset+source.length, then we will cut
  //out the portion of the current string from offset to source.length
  if (this == &source)
  {
    numToCut = (offset + source.length() > length()) ? length() - offset :
                                                       source.length();
    ptrNSString->Insert(*((String)source).ptrNSString ,
                        offset + source.length());
    ptrNSString->Cut(offset, numToCut);
  }
  else
    replace(offset, source.toUnicode(), source.length());
}

//
//Replace the substring starting at offset with the "C" style character string.
//See replace for a Unicode String of a specified lenght below for details
void String::replace(Int32 offset, const char* source)
{
  Int32 srcLength = strlen(source);
  ptrNSString->Cut(offset, srcLength);
  ptrNSString->InsertWithConversion(source, offset, srcLength);
}

//
//Replace the substring starting at offset with the Unicode string.
//
void String::replace(Int32 offset, const UNICODE_CHAR* source)
{
  replace(offset, source, UnicodeLength(source));
}

//Replace the substring starting at offset witht he Unicode string of specified
//length.
//nsString does not appear to provide direct support for replacing a
//character by another.  So we will break the operation into pieces.
//( nsString::Cut(PRUint32, PRInt32) ) - Remove piece being replaced
//( nsString::Insert(PRUnichar*, PRInt32) ) - Insert the new piece
void String::replace(Int32 offset, const UNICODE_CHAR* source,
                            Int32 srcLength)
{
  ptrNSString->Cut(offset, srcLength);
  ptrNSString->Insert((PRUnichar *)source, offset, srcLength);
}

//
//Convert source from an integer to a String, and perform a replacement.
//
void String::replace(Int32 offset, Int32 source)
{
  String convertString;

  replace(offset, ConvertInt(source, convertString));
}

/**
 * Sets the Length of this String, if length is less than 0, it will
 * be set to 0; if length > current length, the string will be extended
 * and padded with '\0' null characters. Otherwise the String
 * will be truncated
**/
void String::setLength(Int32 length) {
    setLength(length, '\0');
} //-- setLength

/**
 * Sets the Length of this String, if length is less than 0, it will
 * be set to 0; if length > current length, the string will be extended
 * and padded with given pad character. Otherwise the String
 * will be truncated.
 * It is not clear what nsString::Truncate(PRInt32) will do if it is presented
 * with a length larger than the current string size.  It is clear how ever
 * that nsString does not support padding the string with a specified
 * character, so this function will need to be broken into a couple of
 * pieces.  One if a simple truncation is taking place, and another if
 * the stirng is being lengthened and padded.
**/
void String::setLength(Int32 length, UNICODE_CHAR padChar)
{
  Int32 strLength = ptrNSString->Length();

  if (length < strLength)
  {
    ptrNSString->Truncate(length);
  }
  else if (length > strLength)
  {
    ptrNSString->SetCapacity(length);
    for(Int32 i=strLength; i < length; i++)
      ptrNSString->Append((PRUnichar)padChar);
  }
} //-- setLength

//
//Delete the "substring" starting at "offset" and proceeding for "count" number
//of characters (or until the end of the string, whichever comes first).
//
void String::deleteChars(Int32 offset, Int32 count)
{
  ptrNSString->Cut(offset, count);
}

//Retrieve the character stored at "index" (starting from 0)
//If the index is out of bounds, -1 will be returned.
//( PRUnichar nsString::CharAt(PRUint32) )
UNICODE_CHAR String::charAt(Int32 index) const
{
  if ((index < length()) && (index >= 0))
    return ptrNSString->CharAt(index);
  else
    return (UNICODE_CHAR)-1;
}

//
//Clear out the string by simply setting the length to zero.  The buffer is
//left intact. Apparently ( nsString::Truncate() ), by default, will clear all
//chars from the string.
//
void String::clear()
{
  ptrNSString->Truncate();
}

//
//Make sure the nsString has room for 'capacity' characters.
//( nsString::SetCapacity(PRUint32) )
//
void String::ensureCapacity(Int32 capacity)
{
  ptrNSString->SetCapacity(capacity);
}

/**
 * Performs a CASE SENSITIVE search of the string for the first occurence
 * of 'data'.  If found return the index, else return NOT_FOUND.
 * -- changed by kvisco to call indexOf(UNICODE_CHAR, Int32)
**/
Int32 String::indexOf(UNICODE_CHAR data) const
{
    return indexOf(data, 0);
} //-- indexOf

//
//Starting at 'offset' perform a CASE SENSITIVE search of the string looking
//for the first occurence of 'data'.  If found return the index, else return
//NOT_FOUND.  If the offset is less than zero, then start at zero.
//( nsString::FindChar(PRUnichar, PRBool, PRInt32) )
//
Int32 String::indexOf(UNICODE_CHAR data, Int32 offset) const
{
  Int32 searchIndex = offset < 0 ? searchIndex = 0 : searchIndex = offset;

  return ptrNSString->FindChar(data, PR_FALSE, searchIndex);
} //-- indexOf

//
//Returns the index of the first occurence of data.
//
Int32 String::indexOf(const String& data) const
{
  return indexOf(data, 0);
}

//
//Returns the index of the first occurrence of data starting at offset.
//Unfortunately there is no mention of how nsString determins the length of a
//PRUnichar* array, all other member functions that take such a data type also
//take a length.  So we will play it safe, and construct an nsString from
//data's strBuffer, and use that to perform the search.
//( nsString::Find(const nsString&, PRBool, PRInt32) )
//
Int32 String::indexOf(const String& data, Int32 offset) const
{
  Int32 searchIndex = offset < 0 ? searchIndex = 0 : searchIndex = offset;

  nsString nsStrData((PRUnichar *)data.toUnicode());

  return ptrNSString->Find(nsStrData, PR_FALSE, searchIndex);
}

//
//Check for equality between this string, and data.
//( nsString::Equals(const PRUnichar*, PRBool, PRInt32) )
//
MBool String::isEqual(const String& data) const
{
  if (this == &data)
    return MB_TRUE;
  else if (ptrNSString->Length() != data.length())
    return MB_FALSE;
  else
  {
    if (ptrNSString->EqualsWithConversion((PRUnichar *)data.toUnicode(),
                            PR_FALSE, data.length()) == PR_TRUE)
      return MB_TRUE;
    else
      return MB_FALSE;
  }
}

/**
 * Returns index of last occurrence of data
 * <BR />
 * Added implementation 19990729 (kvisco)
**/
Int32 String::lastIndexOf(UNICODE_CHAR data) const
{
  return ptrNSString->RFindChar(data);
} //-- lastIndexOf

/**
 * Returns the index of the last occurrence of data starting at offset.
 * NOTE: offset start counting from the LEFT, just like nsString expects.
 * ( nsString::RFindChar(PRUnichar, PRBool, PRInt32) )
 * <BR />
 * Added implementation 19990729 (kvisco)
**/
Int32 String::lastIndexOf(UNICODE_CHAR data, Int32 offset) const
{
  return ptrNSString->RFindChar(data, PR_FALSE, offset);
} //-- lastIndexOf

/**
 * Returns the index of the last occurrence of data
 * <BR />
 * Added implementation 19990729 (kvisco)
**/
Int32 String::lastIndexOf(const String& data) const
{
  return lastIndexOf(data, data.length());
} //-- lastIndexOf

/**
 * Returns the index of the last occurrence of data starting at offset.
 * Since nsString::RFind does not describe how it determins the length of a
 * PRUnichar* array, we will take the safe road by converting our String
 * object into an nsString, and then using that for the search.
 * ( nsString::RFind(const nsString&, PRBool, PRInt32) )
 * <BR />
 * Added implementation 19990729 (kvisco)
**/
Int32 String::lastIndexOf(const String& data, Int32 offset) const
{
  nsString nsData((PRUnichar *)data.toUnicode(), data.length());

  return ptrNSString->RFind(nsData, PR_FALSE, offset);
}

//Retreive the length of this string ( PrInt32 nsString::Length() )
Int32 String::length() const
{
  return ptrNSString->Length();
}

//
//Returns a subString starting at start
//
String& String::subString(Int32 start, String& dest) const
{
  return subString(start, ptrNSString->Length(), dest);
}

/**
 * Returns the subString starting at start and ending at end
 * Note: the dest String is cleared before use
 * For efficiency we will simply retreive characters from our nsString object
 * with nsString::CharAt(PRInt32).  Storage in dest will be through String's
 * public interface, to ensure compatiability with all classes derrived from
 * String.
**/
String& String::subString(Int32 start, Int32 end, String& dest) const
{
  Int32 srcLoop;
  Int32 strLength = ptrNSString->Length();

  start = start < 0? 0 : start;
  end = end > strLength? strLength : end;

  dest.clear();
  if ((start < end))
  {
    dest.ensureCapacity(end - start);
    for (srcLoop=start;srcLoop<end;srcLoop++)
      dest.append(ptrNSString->CharAt(srcLoop));
  }

  return dest;
}

/**
 * Instantiate a new character buffer (remembering the null terminator) and pass
 * it to toCharArray(char*).
**/
char* String::toCharArray() const
{
  char* tmpBuffer = new char[ptrNSString->Length()+1];
  if (memset(tmpBuffer,' ',ptrNSString->Length())){
    return toCharArray(tmpBuffer);
  } else {
    return nsnull;
  }
}

/**
 * Convert the internally represented string to a character buffer.  Store
 * the resultant character array in the buffer provided by the caller. A
 * null terminator will be placed at the end of the array, make sure
 * space has been provided.
 * Use ( nsString::ToCString() ) to retrieve the nsString's buffer.
**/
char* String::toCharArray(char* dest) const
{
  ptrNSString->ToCString(dest, nsCRT::strlen(dest));
  return dest;
}

/**
 * Returns the internal UNICODE_CHAR array so the caller can have access to the
 * to the UNICODE_CHAR representation of the string. Will not place a null
 * terminator at the end of the array, as in a call to toChar will do.
 * Use ( nsString::GetUnicode() ) to retreive the nsString's buffer, then
 * copy it to dest.
**/
UNICODE_CHAR* String::toUnicode(UNICODE_CHAR* dest) const
{
  Int32 copyLoop;
  Int32 strLength = ptrNSString->Length();
  const UNICODE_CHAR* strBuffer = (PRUnichar *)ptrNSString->GetUnicode();

  for (copyLoop=0;copyLoop<strLength;copyLoop++)
   dest[copyLoop] = strBuffer[copyLoop];

  return dest;
}

//
//This fuction returns the actual UNICODE_CHAR* buffer storing the string.
//This provides a more efficient means to interact with the buffer in a read
//only fahsion.
//
const UNICODE_CHAR* String::toUnicode() const
{
  return (PRUnichar *)ptrNSString->GetUnicode();
}

//
//Convert String to lowercase ( nsString::ToLowerCase() )
//
void String::toLowerCase()
{
  ptrNSString->ToLowerCase();
}

//
//Convert String to uppercase ( nsString::ToUpperCase() )
//
void String::toUpperCase()
{
  ptrNSString->ToUpperCase();
}

//
//Trim whitespace from both ends of String
//( nsString::Trim(const char*, PRBool, PRBool) )
//Currently we trim only spaces!
//
void String::trim()
{
  ptrNSString->Trim(" \n\t\r");
}

//
//Cause the string to reverse itself
//nsString does not appear to have a reversal method, so we will use
//nsString::CharAt(PRUint32) and nsString::SetCharAt(PRUnichar, PRUint32) like
//in Stirng.
//
void String::reverse()
{
  Int32 reverseLoop;
  Int32 strLength = ptrNSString->Length();
  UNICODE_CHAR tempChar;

  for (reverseLoop=0;reverseLoop<(strLength/2); reverseLoop++)
    {
      tempChar = ptrNSString->CharAt(reverseLoop);
      ptrNSString->SetCharAt(ptrNSString->CharAt(strLength - reverseLoop - 1),
                             reverseLoop);
      ptrNSString->SetCharAt(tempChar, strLength - reverseLoop - 1);
    }
}

//
//Compare the two string representations for equality
//
MBool String::isEqual(const UNICODE_CHAR* data, const UNICODE_CHAR* search,
                      Int32 length) const
{
  Int32 compLoop = 0;

  while (compLoop < length)
    {
    if (data[compLoop] != search[compLoop])
      return MB_FALSE;

    compLoop++;
    }

  return MB_TRUE;
}

//
//Convert an Int32 into a String by storing it in target
//
String& String::ConvertInt(Int32 value, String& target)
{
  UNICODE_CHAR charDigit;

  target.clear();

  while (value)
    {
      charDigit = (value % 10) + 48;
      target.append(charDigit);
      value /=10;
    }

  target.reverse();

  return target;
}

//
//Calculate the length of a null terminated UNICODE_CHAR string
//
Int32 String::UnicodeLength(const UNICODE_CHAR* data)
{
  Int32 index = 0;

  //Count UNICODE_CHARs Until a Unicode "NULL" is found.
  while (data[index] != 0x0000)
    index++;

  return index;
}

//
//Retrieve a reference to the nsString object
//
nsString& String::getNSString()
{
  return *ptrNSString;
}

//
//Retrieve a const reference to the nsString object
//
const nsString& String::getConstNSString() const
{
  return *ptrNSString;
}

//
//String copies itself to the destination
//
//void String::copyString(SPECIAL_CHAR* dest)
//{
//}

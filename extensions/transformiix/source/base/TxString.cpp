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


#include <stdlib.h>
#include <string.h>
#include "TxString.h"
#include <iostream.h>

//
//Default Constructor, create an empty String
//
String::String()
{
  strBuffer = NULL;
  bufferLength = 0;
  strLength = 0;
}

//
//Create an empty String of a specific size
//
String::String(PRInt32 initSize)
{
  strBuffer = new UNICODE_CHAR[initSize];
  bufferLength = initSize;
  strLength = 0;
}

//
//Create a copy of the source String
//TK 12/09/1999 - To ensure compatibility with sub classes of String, this
//                constructor has been modified to use String's public
//                interface only.
//
String::String(const String& source)
{
  PRInt32 copyLoop;

  //Allocate space for the source string
  strLength = source.length();

  //-- modified by kvisco to only use necessay amount of space
  //-- was: bufferLength = source.bufferLength;
  bufferLength = strLength;

  strBuffer = new UNICODE_CHAR[bufferLength];

  //Copy the new string data after the old data
  for (copyLoop=0;copyLoop<strLength;copyLoop++)
    strBuffer[copyLoop] = source.charAt(copyLoop);

}

//
//Create a String from the characters
//
String::String(const char* source)
{
  PRInt32 copyLoop;

  if (source)
    {
      strLength = bufferLength = strlen(source);

      //Set up the new buffer to be the same size as the input char array.
      strBuffer = new UNICODE_CHAR[bufferLength];

      //Now store the char array in the new string buffer.
      //Can't use strcpy since the destination is a UNICODE_CHAR buffer.
      //Also make sure to pick up the null terminator at the end of the char
      //array.
      for (copyLoop=0; copyLoop<strLength; copyLoop++)
        strBuffer[copyLoop] = (PRInt32)source[copyLoop];
    }
  else
    {
      //Create an empty string
      strBuffer = NULL;
      bufferLength = 0;
      strLength = 0;
    }
}

//
//Create a String from a null terminated array of UNICODE_CHARs
//
String::String(const UNICODE_CHAR* source)
{
  PRInt32 copyLoop;

  if (source)
    {
      strLength = bufferLength = UnicodeLength(source);

      //Set up the new buffer to be the same size as the input char array.
      strBuffer = new UNICODE_CHAR[bufferLength];

      //Now store the char array in the new string buffer.
      //Can't use strcpy since the destination is a UNICODE_CHAR buffer.
      //Also make sure to pick up the null terminator at the end of the char
      //array.
      for (copyLoop=0; copyLoop<strLength; copyLoop++)
        strBuffer[copyLoop] = source[copyLoop];
    }
  else
    {
      //Create an empty string
      strBuffer = NULL;
      bufferLength = 0;
      strLength = 0;
    }
}

//
//Create a String from a UNICODE_CHAR buffer and a supplied string length
//
String::String(const UNICODE_CHAR* source, PRInt32 length)
{
  PRInt32 copyLoop;

  if (source)
    {
      strLength = bufferLength = length;

      //Set up the new buffer to be the same size as the input char array.
      strBuffer = new UNICODE_CHAR[bufferLength];

      //Now store the char array in the new string buffer.
      //Can't use strcpy since the destination is a UNICODE_CHAR buffer.
      //Also make sure to pick up the null terminator at the end of the char
      //array.
      for (copyLoop=0; copyLoop<strLength; copyLoop++)
        strBuffer[copyLoop] = source[copyLoop];
    }
  else
    {
      //Create an empty string
      strBuffer = NULL;
      bufferLength = 0;
      strLength = 0;
    }
}

//
//Destroy the String, and free memory
//
String::~String()
{
  //cout << "~String() deleting " << *this <<endl;
  if (strBuffer)
    delete strBuffer;
}

//
//Convert the UNICODE_CHARs of this String to Chars, and output them to the given
//ostream
//
ostream& operator<<(ostream& output, const String& source)
{
  PRInt32 outputLoop;

  for (outputLoop=0;outputLoop<source.length();outputLoop++)
    output << (char)source.charAt(outputLoop);

  return output;
}


//
//Overloaded '=' operator to assign the value of the source string to this
//string.
//TK 12/09/1999 - Modified to interact only with the public interface of the
//                source "String" object.  This ensures compatibility with
//                subclasses of String.
//
String& String::operator=(const String& source)
{
  PRInt32 copyLoop;
  //Make sure string is not being copied onto itself
  if (&source != this) {
    //Check to see if this string is already big enough for source
    //-- set size of current string length to 0 before making this call (kvisco)
    strLength = 0;
    ensureCapacity(source.length());

    for (copyLoop=0; copyLoop<source.length(); copyLoop++)
      strBuffer[copyLoop] = source.charAt(copyLoop);

    strLength = copyLoop;
  }
  return *this;
}

//
//Overloaded '=' operator to assign the value of the source C string to this
//string
//
String& String::operator=(const char* source)
{
  PRInt32 copyLoop;
  PRInt32 sourceLength;

  if (source)
    {
      sourceLength = strlen(source);
      ensureCapacity(sourceLength);

      for (copyLoop=0; copyLoop<sourceLength; copyLoop++)
        strBuffer[copyLoop] = source[copyLoop];

      strLength = copyLoop;
    }
  else
    clear();

  return *this;
}

//
//Overloaded '=' operator to assign the value of a UNICODE_CHAR string to this
//string.  Note:  The soucre is "NULL" terminated.
//
String& String::operator=(const UNICODE_CHAR* source)
{
  PRInt32 copyLoop;
  PRInt32 sourceLength;

  if (source)
    {
      sourceLength = UnicodeLength(source);
      ensureCapacity(sourceLength);

      for (copyLoop=0; copyLoop<sourceLength; copyLoop++)
        strBuffer[copyLoop] = source[copyLoop];

      strLength = copyLoop;
    }
  else
    clear();

  return *this;
}

//
//Overloaded '=' operator to assign an integer to this string.
//
String& String::operator=(PRInt32 source)
{
  ConvertInt(source, *this);
  return *this; //-- added, LF
} //-- operator=

//
//Grow buffer by 1 if necessary, and append the source character
//
void String::append(UNICODE_CHAR source)
{
  //Enlarge string buffer to fit source character if necessary
  ensureCapacity(1);

  //Insert the character and increment the length
  strBuffer[strLength++] = source;
}

//
//Append a character to the string
//
void String::append(char source)
{
  append((UNICODE_CHAR)source);
}

//
//Append String
//TK 12/09/199 - Modified to use the "source" String's public interface only,
//               to ensure compatibility with sub classes
//
void String::append(const String& source)
{
  PRInt32 copyLoop;

  //Enlarge buffer to fit string if necessary
  ensureCapacity(source.length());

  //Copy the new string data after the old data
  for (copyLoop=0;copyLoop<source.length();copyLoop++)
    strBuffer[strLength+copyLoop] = source.charAt(copyLoop);

  //Update the length
  strLength += source.length();
}

//
//Append a string of characters (null terminated arry of chars)
//
void String::append(const char* source)
{
  PRInt32 copyLoop;
  PRInt32 initLength = strlen(source);

  //Enlarge buffer to fit new string if necessary
  ensureCapacity(initLength);

  //Copy the new string data after the old data
  for (copyLoop=0;copyLoop<initLength;copyLoop++)
    strBuffer[strLength+copyLoop] = source[copyLoop];

  //Update the length
  strLength += initLength;
}

//
//Append a string of unicode chars (null terminated array of Unicode chars)
//
void String::append(const UNICODE_CHAR* source)
{
  append(source, UnicodeLength(source));
}


//
//Append a string of DOM Characters whose length is also defined
//
void String::append(const UNICODE_CHAR* source, PRInt32 length)
{
  PRInt32 copyLoop;

  //Enlarge buffer to fit new string if necessary
  ensureCapacity(length);

  //Copy the new string data after the old data
  for (copyLoop=0;copyLoop<length;copyLoop++)
    strBuffer[strLength+copyLoop] = source[copyLoop];

  //Update the length
  strLength += length;
}

//
//Convert source from an integer to a string, and append it to the current
//string
//
void String::append(PRInt32 source)
{
  String convertString;

  append(ConvertInt(source, convertString));
}

//
//Insert a single UNICODE_CHAR into the string starting at offset
//
void String::insert(PRInt32 offset, const UNICODE_CHAR source)
{
  PRInt32 moveLoop;
  PRInt32 moveIndex;

  offset = offset < 0 ? 0 : offset;

  if (offset < strLength)
    {
      moveLoop = strLength - offset;
      moveIndex = strLength - 1;

      //Enlarge string buffer to fit source character
      ensureCapacity(1);

      //Bump all characters down one position to make room for new character
      while (moveLoop--)
        strBuffer[moveIndex+1] = strBuffer[moveIndex--];

      strBuffer[moveIndex+1] = source;
      strLength += 1;
    }
  else
    append(source);
}

//
//Insert a single C type character into the string starting at offset
//
void String::insert(PRInt32 offset, const char source)
{
  insert(offset, (UNICODE_CHAR)source);
}

//
//Insert the source string starting at the current offset
//TK 12/09/1999 - Modified to use the "source" String's public interface only,
//                to ensure compatibility with subclasses of String
//
void String::insert(PRInt32 offset, const String& source)
{
  PRInt32 moveLoop;
  PRInt32 moveIndex;
  PRInt32 copyLoop;

  offset = offset < 0 ? 0 : offset;

  if (offset < strLength)
    {
      moveLoop = strLength - offset;
      moveIndex = strLength - 1;

      //Enlarge string buffer to fit source character
      ensureCapacity(source.length());

      //Bump all characters down one position to make room for new character
      while (moveLoop--)
        strBuffer[moveIndex+source.length()] = strBuffer[moveIndex--];

      moveIndex += 1;
      for (copyLoop=0;copyLoop<source.strLength;copyLoop++)
        strBuffer[moveIndex+copyLoop] = source.charAt(copyLoop);

      strLength += source.length();
    }
  else
    append(source);
}

//
//Insert the source "C" type string into this string starting at offset
//
void String::insert(PRInt32 offset, const char* source)
{
  PRInt32 moveLoop;
  PRInt32 moveIndex;
  PRInt32 copyLoop;
  PRInt32 sourceLength;

  offset = offset < 0 ? 0 : offset;

  if (offset < strLength)
    {
      moveLoop = strLength - offset;
      moveIndex = strLength - 1;
      sourceLength = strlen(source);

      //Enlarge string buffer to fit source character
      ensureCapacity(sourceLength);

      //Bump all characters down to make room for new character
      while (moveLoop--)
        strBuffer[moveIndex+sourceLength] = strBuffer[moveIndex--];

      moveIndex += 1;
      for (copyLoop=0;copyLoop<sourceLength;copyLoop++)
        strBuffer[moveIndex+copyLoop] = source[copyLoop];

      strLength += sourceLength;
    }
  else
    append(source);
}

//
//Insert the source UNICODE_CHAR type string into this string starting at
//offset.  Note that the source is Null Terminated.
//
void String::insert(PRInt32 offset, const UNICODE_CHAR* source)
{
  insert(offset, source, UnicodeLength(source));
}

void String::insert(PRInt32 offset, const UNICODE_CHAR* source,
                    PRInt32 sourceLength)
{
  PRInt32 moveLoop;
  PRInt32 moveIndex;
  PRInt32 copyLoop;

  offset = offset < 0 ? 0 : offset;

  if (offset < strLength)
    {
      moveLoop = strLength - offset;
      moveIndex = strLength - 1;

      //Enlarge string buffer to fit source character
      ensureCapacity(sourceLength);

      //Bump all characters down to make room for new character
      while (moveLoop--)
        strBuffer[moveIndex+sourceLength] = strBuffer[moveIndex--];

      moveIndex += 1;
      for (copyLoop=0;copyLoop<sourceLength;copyLoop++)
        strBuffer[moveIndex+copyLoop] = source[copyLoop];

      strLength += sourceLength;
    }
  else
    append(source, sourceLength);
}

//
//Convert source from an integer to a string, and then insert.
//
void String::insert(PRInt32 offset, PRInt32 source)
{
  String convertString;

  insert(offset, ConvertInt(source, convertString));
}

//
//Replace the character specified by offset with the UNICODE_CHAR source
//
void String::replace(PRInt32 offset, const UNICODE_CHAR source)
{
  offset = offset < 0 ? 0 : offset;

  if (offset < strLength)
    {
      strBuffer[offset] = source;
    }
  else
    append(source);
}

//
//Replace the character specified by offset with the C style character source
//
void String::replace(PRInt32 offset, const char source)
{
  replace(offset, (UNICODE_CHAR)source);
}

//
//Replace the substring starting at offset with the String specified by source.
//Enlarge the string buffer if source will require more characters than
//currently available.
//TK 12/09/1999 - Modified to use the "source" String's public interface only,
//                to ensure compatibility with classes derived from String
//
void String::replace(PRInt32 offset, const String& source)
{
  PRInt32 totalOffset = 0;
  PRInt32 replaceLoop;

  if (offset < strLength)
    {
      offset = offset < 0 ? 0 : offset;
      totalOffset = offset + source.length();

      if (totalOffset > strLength)
        {
        ensureCapacity(totalOffset - strLength);
        strLength += totalOffset - strLength;
        }

      for (replaceLoop=0;replaceLoop<source.length();replaceLoop++)
        strBuffer[offset + replaceLoop] = source.charAt(replaceLoop);
    }
  else
    append(source);
}

//
//Replace the substring starting at offset with the "C" style character string.
//enlarge the string buffer if source will require more characters than
//currently available.
//
void String::replace(PRInt32 offset, const char* source)
{
  PRInt32 totalOffset = 0;
  PRInt32 replaceLoop;
  PRInt32 sourceLength;

  if (offset < strLength)
    {
      offset = offset < 0 ? 0 : offset;
      sourceLength = strlen(source);
      totalOffset = offset + sourceLength;

      if (totalOffset > strLength)
        {
        ensureCapacity(totalOffset - strLength);
        strLength += totalOffset - strLength;
        }

      for (replaceLoop=0;replaceLoop<sourceLength;replaceLoop++)
        strBuffer[offset + replaceLoop] = source[replaceLoop];
    }
  else
    append(source);
}

//
//Replace the substring starting at offset with the Unicode string.
//enlarge the string buffer if source will require more characters than
//currently available.
//TK 12/21/1999 - Simply deffer functionality to the replace function
//                below (accepting a length for the Unicode buffer).
//
void String::replace(PRInt32 offset, const UNICODE_CHAR* source)
{
  replace(offset, source, UnicodeLength(source));
}

//
//Replace the substring starting at offset with the Unicode string.
//enlarge the string buffer if source will require more characters than
//currently available.
//
void String::replace(PRInt32 offset, const UNICODE_CHAR* source, PRInt32 srcLength)
{
  PRInt32 totalOffset = 0;
  PRInt32 replaceLoop;

  if (offset < strLength)
    {
      offset = offset < 0 ? 0 : offset;
      totalOffset = offset + srcLength;

      if (totalOffset > strLength)
        {
        ensureCapacity(totalOffset - strLength);
        strLength += totalOffset - strLength;
        }

      for (replaceLoop=0;replaceLoop<srcLength;replaceLoop++)
        strBuffer[offset + replaceLoop] = source[replaceLoop];
    }
  else
    append(source);
}


//
//Convert source from an integer to a String, and perform a replacement.
//
void String::replace(PRInt32 offset, PRInt32 source)
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
void String::setLength(PRInt32 length) {
    setLength(length, '\0');
} //-- setLength

/**
 * Sets the Length of this String, if length is less than 0, it will
 * be set to 0; if length > current length, the string will be extended
 * and padded with given pad character. Otherwise the String
 * will be truncated
**/
void String::setLength(PRInt32 length, UNICODE_CHAR padChar) {
    if ( length < 0 ) strLength = 0;
    else if ( length > strLength ) {
        PRInt32 diff = length-strLength;
        ensureCapacity(diff);
        for ( PRInt32 i = strLength; i < length; i++ )
            strBuffer[i] = padChar;
        strLength = length;
    }
    else strLength = length;
} //-- setLength

//
//Delete the "substring" starting at "offset" and proceeding for "count" number
//of characters (or until the end of the string, whichever comes first).
//
void String::deleteChars(PRInt32 offset, PRInt32 count)
{
  PRInt32 deleteLoop;
  PRInt32 offsetCount;

  offset = offset < 0 ? 0 : offset;
  offsetCount = offset +  count;

  if (offsetCount < strLength)
    {
      for (deleteLoop=0;deleteLoop<strLength-offsetCount;deleteLoop++)
        strBuffer[offset + deleteLoop] = strBuffer[offsetCount + deleteLoop];

      strLength -= count;
    }
  else
    strLength = offset;
}


/**
 * Returns the character at index.
 * If the index is out of bounds, -1 will be returned.
**/
UNICODE_CHAR String::charAt(PRInt32 index) const
{
  if ((index < strLength) && (index >= 0))
    return strBuffer[index];
  else
    return (UNICODE_CHAR)-1;
}

//
//Clear out the string by simply setting the length to zero.  The buffer is
//left intact.
//
void String::clear()
{
  strLength = 0;
}

//
//Make sure the buffer has room for 'capacity' UNICODE_CHARS.
//
void String::ensureCapacity(PRInt32 capacity)
{
    UNICODE_CHAR* tempStrBuffer = NULL;

    //Check for the desired capacity
    PRInt32 freeSpace = bufferLength - strLength; //(added by kvisco)

    if (freeSpace < capacity) {

        //-- modified by kvisco to only add needed capacity,
        //-- not extra bytes as before
        //-- old : bufferLength += capacity;
        bufferLength += capacity - freeSpace;

        tempStrBuffer = new UNICODE_CHAR[bufferLength];
        copyString(tempStrBuffer);

        //If the old string contained any data, delete it, and save the new.
        if (strBuffer)
            delete strBuffer;

        strBuffer = tempStrBuffer;
    }
}

/**
 * Performs a CASE SENSITIVE search of the string for the first occurence
 * of 'data'.  If found return the index, else return NOT_FOUND.
 * -- changed by kvisco to call indexOf(UNICODE_CHAR, PRInt32)
**/
PRInt32 String::indexOf(UNICODE_CHAR data) const
{
    return indexOf(data, 0);
} //-- indexOf

//
//Starting at 'offset' perform a CASE SENSITIVE search of the string looking
//for the first occurence of 'data'.  If found return the index, else return
//NOT_FOUND.  If the offset is less than zero, then start at zero.
//
PRInt32 String::indexOf(UNICODE_CHAR data, PRInt32 offset) const
{
  PRInt32 searchIndex = offset < 0 ? searchIndex = 0 : searchIndex = offset;

  while (1)
    {
    if (searchIndex >= strLength)
      return NOT_FOUND;
    else if (strBuffer[searchIndex] == data)
      return searchIndex;
    else
      ++searchIndex;
    }
} //-- indexOf

//
//Returns the index of the first occurence of data
//TK 12/09/1999 - Modified to simply use indexOf(const String&, PRInt32).
//
PRInt32 String::indexOf(const String& data) const
{
  return indexOf(data, 0);
}

//
//Returns the index of the first occurrence of data starting at offset
//TK 12/09/1999 - Modified to use the "data" String's public interface to
//                retreive the Unicode Char buffer when calling isEqual.
//                This ensures compatibility with classes derrived from String.
//
PRInt32 String::indexOf(const String& data, PRInt32 offset) const
{
  PRInt32 searchIndex = offset < 0 ? 0 : offset;

  while (1)
  {
    if (searchIndex <= (strLength - data.length()))
    {
      if (isEqual(&strBuffer[searchIndex], data.toUnicode(), data.length()))
        return searchIndex;
    }
    else
      return NOT_FOUND;

    searchIndex++;
  }
}

//
//Check for equality between this string, and data
//TK 12/09/1999 - Modified to use data.toUnicode() public member function
//                when working with data's unicode buffer.  This ensures
//                compatibility with derrived classes.
//
MBool String::isEqual(const String& data) const
{
  if (this == &data)
    return MB_TRUE;
  else if (strLength != data.length())
    return MB_FALSE;
  else
    return isEqual(strBuffer, data.toUnicode(), data.length());
}

/**
 * Returns index of last occurrence of data
**/
PRInt32 String::lastIndexOf(UNICODE_CHAR data) const
{
    return lastIndexOf(data, strLength-1);
} //-- lastIndexOf

/**
 * Returns the index of the last occurrence of data starting at offset
**/
PRInt32 String::lastIndexOf(UNICODE_CHAR data, PRInt32 offset) const
{
    if ((offset < 0) || (offset >= strLength)) return NOT_FOUND;

    PRInt32 searchIndex = offset;
    while (searchIndex >= 0) {
        if (strBuffer[searchIndex] == data) return searchIndex;
        --searchIndex;
    }
    return NOT_FOUND;

} //-- lastIndexOf

/**
 * Returns the index of the last occurrence of data
**/
PRInt32 String::lastIndexOf(const String& data) const
{
    return lastIndexOf(data, strLength-1);
} //-- lastIndexOf

/**
 * Returns the index of the last occurrence of data starting at offset
**/
PRInt32 String::lastIndexOf(const String& data, PRInt32 offset) const
{
  PRInt32 searchIndex;
  const UNICODE_CHAR* dataStrBuffer = NULL;

  if ((offset < 0) || (offset >= strLength))
    return NOT_FOUND;
  else
  {
    searchIndex = offset;

    //If there is not enough space between searchIndex and the length of the of
    //the string for "data" to appear, then there is no reason to search it.
    if ((strLength - searchIndex) < data.length())
      searchIndex = strLength - data.length();

    dataStrBuffer = data.toUnicode();
    while (searchIndex >= 0)
    {
      if (isEqual(&strBuffer[searchIndex], data.toUnicode(), data.length()))
        return searchIndex;
      --searchIndex;
    }
  }
    return NOT_FOUND;
}

//
//Checks whether the string is empty
//
MBool String::isEmpty() const
{
    return (strLength == 0);
}

//
//Returns the length of the String
//
PRInt32 String::length() const
{
  return strLength;
}

//
//Returns a subString starting at start
//TK 12/09/1999 - Modified to simply use subString(PRInt32, PRInt32, String&)
//
String& String::subString(PRInt32 start, String& dest) const
{
  return subString(start, strLength, dest);
}

/**
 * Returns the subString starting at start and ending at end
 * Note: the dest String is cleared before use
 * TK 12/09/1999 - Modified to use the "dest" String's public interface to
 *                 ensure compatibility wtih derrived classes.
**/
String& String::subString(PRInt32 start, PRInt32 end, String& dest) const
{
  PRInt32 srcLoop;
  PRInt32 destLoop = 0;

  start = start < 0? 0 : start;
  end = end > strLength? strLength : end;

  dest.clear();
  if ((start < end))
    {
    dest.ensureCapacity(end - start);
    for (srcLoop=start;srcLoop<end;srcLoop++)
      dest.append(strBuffer[srcLoop]);
    }

  return dest;
}

/**
 * Instantiate a new character buffer (remembering the null terminator) and pass
 * it to toChar(char*).
**/
char* String::toCharArray() const
{
  char* tmpBuffer = new char[strLength+1];

  return toCharArray(tmpBuffer);
}

/**
 * Convert the internally represented string to a character buffer.  Store
 * the resultant character array in the buffer provided by the caller. A
 * null terminator will be placed at the end of the array, make sure
 * space has been provided.
**/
char* String::toCharArray(char* dest) const
{
  PRInt32 copyLoop;

  for (copyLoop=0;copyLoop<strLength;copyLoop++)
   dest[copyLoop] = strBuffer[copyLoop];

  //Place a NULL terminator at the end of the character buffer
  dest[copyLoop] = 0;

  return dest;
}

/**
 * Returns the internal UNICODE_CHAR array so the caller can have access to the
 * to the UNICODE_CHAR representation of the string. Will not place a null
 * terminator at the end of the array, as in a call to toChar will do.
**/
UNICODE_CHAR* String::toUnicode(UNICODE_CHAR* dest) const
{
  PRInt32 copyLoop;

  for (copyLoop=0;copyLoop<strLength;copyLoop++)
    dest[copyLoop] = strBuffer[copyLoop];

  //-- removed null terminator at end (kvisco)
  return dest;
}

//
//This fuction returns the actual UNICODE_CHAR* buffer storing the string.
//This provides a more efficient means to interact with the buffer in a read
//only fahsion.
//
const UNICODE_CHAR* String::toUnicode() const
{
  return strBuffer;
}

//
//Convert String to lowercase
//
void String::toLowerCase()
{
  PRInt32 conversionLoop;

  for (conversionLoop=0;conversionLoop<strLength;conversionLoop++)
    {
    if ((strBuffer[conversionLoop] >= 'A') &&
        (strBuffer[conversionLoop] <= 'Z'))
      strBuffer[conversionLoop] += 32;
    }
}

//
//Convert String to uppercase
//
void String::toUpperCase()
{
  PRInt32 conversionLoop;

  for (conversionLoop=0;conversionLoop<strLength;conversionLoop++)
    {
    if ((strBuffer[conversionLoop] >= 'a') &&
        (strBuffer[conversionLoop] <= 'z'))
      strBuffer[conversionLoop] -= 32;
    }
}

//
//Trim whitespace from both ends of String
//
void String::trim()
{
  PRInt32 trimLoop = strLength - 1;
  PRInt32 cutLoop;
  MBool done = MB_FALSE;

  //As long as we are not working on an emtpy string, trim from the right
  //first, so we don't have to move useless spaces when we trim from the left.
  if (strLength > 0)
  {
    while (!done)
    {
      switch (strBuffer[trimLoop])
      {
        case ' ' :
        case '\t' :
        case '\n' :
        case '\r' :
          --strLength;
          --trimLoop;
        break;

        default :
          done = MB_TRUE;
        break;
      }
    }
  }

  //Now, if there are any characters left to the string, Trim to the left.
  //First count the number of "left" spaces.  Then move all characters to the
  //left by that ammount.
  if (strLength > 0)
  {
    done = MB_FALSE;
    trimLoop = 0;
    while (!done)
    {
      switch (strBuffer[trimLoop])
      {
        case ' ' :
        case '\t' :
        case '\n' :
        case '\r' :
          ++trimLoop;
        break;

        default :
          done = MB_TRUE;
        break;
      }
    }

    if (trimLoop < strLength)
    {
      for (cutLoop=trimLoop;cutLoop<strLength;cutLoop++)
        strBuffer[cutLoop-trimLoop] = strBuffer[cutLoop];
    }

    strLength -= trimLoop;
  }
}

//
//Cause the string to reverse itself
//
void String::reverse()
{
  PRInt32 reverseLoop;
  UNICODE_CHAR tempChar;

  for (reverseLoop=0;reverseLoop<(strLength/2); reverseLoop++)
    {
      tempChar = strBuffer[reverseLoop];
      strBuffer[reverseLoop] = strBuffer[strLength - reverseLoop - 1];
      strBuffer[strLength - reverseLoop - 1] = tempChar;
    }
}

//
//String copies itself to the destination
//
void String::copyString(UNICODE_CHAR* dest)
{
  PRInt32 copyLoop;

  for (copyLoop=0;copyLoop<strLength;copyLoop++)
   dest[copyLoop] = strBuffer[copyLoop];
}

//
//Compare the two string representations for equality
//
MBool String::isEqual(const UNICODE_CHAR* data, const UNICODE_CHAR* search,
                      PRInt32 length) const
{
  PRInt32 compLoop = 0;

  while (compLoop < length)
    {
    if (data[compLoop] != search[compLoop])
      return MB_FALSE;

    compLoop++;
    }

  return MB_TRUE;
}

//
//Convert an PRInt32 into a String by storing it in target
//
String& String::ConvertInt(PRInt32 value, String& target)
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
PRInt32 String::UnicodeLength(const UNICODE_CHAR* data)
{
  PRInt32 index = 0;

  //Count UNICODE_CHARs Until a Unicode "NULL" is found.
  while (data[index] != 0x0000)
    index++;

  return index;
}

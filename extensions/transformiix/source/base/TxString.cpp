/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "TxString.h"
#include <stdlib.h>
#include <string.h>

String::String() : mBuffer(0),
                   mBufferLength(0),
                   mLength(0)
{
}

String::String(const String& aSource) : mBuffer(aSource.toUnicode()),
                                        mBufferLength(aSource.mLength),
                                        mLength(aSource.mLength)
{
}

String::String(const UNICODE_CHAR* aSource,
               PRUint32 aLength) : mBuffer(0),
                                   mBufferLength(0),
                                   mLength(0)
{
  if (!aSource) {
    return;
  }

  if (aLength == 0) {
    aLength = unicodeLength(aSource);
  }
  if (!ensureCapacity(aLength)) {
    return;
  }
  memcpy(mBuffer, aSource, aLength * sizeof(UNICODE_CHAR));
  mLength = aLength;
}

String::~String()
{
  delete [] mBuffer;
}

void String::append(UNICODE_CHAR aSource)
{
  if (!ensureCapacity(1)) {
    return;
  }
  mBuffer[mLength] = aSource;
  ++mLength;
}

void String::append(const String& aSource)
{
  if (!ensureCapacity(aSource.mLength)) {
    return;
  }
  memcpy(&mBuffer[mLength], aSource.mBuffer,
         aSource.mLength * sizeof(UNICODE_CHAR));
  mLength += aSource.mLength;
}

void String::insert(PRUint32 aOffset, UNICODE_CHAR aSource)
{
  if (!ensureCapacity(1)) {
    return;
  }
  if (aOffset < mLength) {
    memmove(&mBuffer[aOffset + 1], &mBuffer[aOffset],
            (mLength - aOffset) * sizeof(UNICODE_CHAR));
  }
  mBuffer[aOffset] = aSource;
  mLength += 1;
}

void String::insert(PRUint32 aOffset, const String& aSource)
{
  if (!ensureCapacity(aSource.mLength)) {
    return;
  }
  if (aOffset < mLength) {
    memmove(&mBuffer[aOffset + aSource.mLength], &mBuffer[aOffset],
            (mLength - aOffset) * sizeof(UNICODE_CHAR));
  }
  memcpy(&mBuffer[aOffset], aSource.mBuffer,
         aSource.mLength * sizeof(UNICODE_CHAR));
  mLength += aSource.mLength;
}

void String::replace(PRUint32 aOffset, UNICODE_CHAR aSource)
{
  if (aOffset < mLength) {
    mBuffer[aOffset] = aSource;
  }
  else {
    append(aSource);
  }
}

void String::replace(PRUint32 aOffset, const String& aSource)
{
  if (aOffset < mLength) {
    PRUint32 finalLength = aOffset + aSource.mLength;

    if (finalLength > mLength) {
      if (!ensureCapacity(finalLength - mBufferLength)) {
        return;
      }
      mLength = finalLength;
    }
    memcpy(&mBuffer[aOffset], aSource.mBuffer,
           aSource.mLength * sizeof(UNICODE_CHAR));
  }
  else {
    append(aSource);
  }
}

void String::deleteChars(PRUint32 aOffset, PRUint32 aCount)
{
  PRUint32 cutEnd = aOffset + aCount;

  if (cutEnd < mLength) {
    memmove(&mBuffer[aOffset], &mBuffer[cutEnd],
            (mLength - cutEnd) * sizeof(UNICODE_CHAR));
    mLength -= aCount;
  }
  else {
    mLength = aOffset;
  }
}

void String::clear()
{
  mLength = 0;
}

PRInt32 String::indexOf(UNICODE_CHAR aData,
                        PRInt32 aOffset) const
{
  NS_ASSERTION(aOffset >= 0, "Passed negative offset to indexOf.");
  if (aOffset < 0) {
    return kNotFound;
  }

  PRInt32 searchIndex = aOffset;

  while (searchIndex < (PRInt32)mLength) {
    if (mBuffer[searchIndex] == aData) {
      return searchIndex;
    }
    ++searchIndex;
  }
  return kNotFound;
}

PRInt32 String::indexOf(const String& aData, PRInt32 aOffset) const
{
  NS_ASSERTION(aOffset >= 0, "Passed negative offset to indexOf.");
  if (aOffset < 0) {
    return kNotFound;
  }

  PRInt32 searchIndex = aOffset;
  PRInt32 searchLimit = mLength - aData.mLength;

  while (searchIndex <= searchLimit) {
    if (memcmp(&mBuffer[searchIndex], aData.mBuffer,
        aData.mLength * sizeof(UNICODE_CHAR)) == 0) {
      return searchIndex;
    }
    ++searchIndex;
  }
  return kNotFound;
}

PRInt32 String::lastIndexOf(UNICODE_CHAR aData,
                            PRInt32 aOffset) const
{
  NS_ASSERTION(aOffset >= 0, "Passed negative offset to lastIndexOf.");
  if (aOffset < 0) {
     return kNotFound;
  }

  PRUint32 searchIndex = mLength - aOffset;
  while (--searchIndex >= 0) {
    if (mBuffer[searchIndex] == aData) {
      return searchIndex;
    }
  }
  return kNotFound;
}

MBool String::isEqual(const String& aData) const
{
  if (mLength != aData.mLength) {
    return MB_FALSE;
  }
  return (memcmp(mBuffer, aData.mBuffer, mLength * sizeof(UNICODE_CHAR)) == 0);
}

MBool String::isEqualIgnoreCase(const String& aData) const
{
  if (mLength != aData.mLength) {
    return MB_FALSE;
  }

  UNICODE_CHAR thisChar, otherChar;
  PRUint32 compLoop = 0;
  while (compLoop < mLength) {
    thisChar = mBuffer[compLoop];
    if ((thisChar >= 'A') && (thisChar <= 'Z')) {
      thisChar += 32;
    }
    otherChar = aData.mBuffer[compLoop];
    if ((otherChar >= 'A') && (otherChar <= 'Z')) {
      otherChar += 32;
    }
    if (thisChar != otherChar) {
      return MB_FALSE;
    }
    ++compLoop;
  }
  return MB_TRUE;
}

void String::truncate(PRUint32 aLength)
{
  NS_ASSERTION(aLength <= mBufferLength, "truncate can't increase buffer");
  mLength = (aLength > mBufferLength) ? mBufferLength : aLength;
}

String& String::subString(PRUint32 aStart, String& aDest) const
{
  return subString(aStart, mLength, aDest);
}

String& String::subString(PRUint32 aStart, PRUint32 aEnd,
                          String& aDest) const
{
  PRUint32 end = (aEnd > mLength) ? mLength : aEnd;

  aDest.clear();
  if (aStart >= end) {
    return aDest;
  }
  PRUint32 substrLength = end - aStart;

  if (!aDest.ensureCapacity(substrLength)) {
    return aDest;
  }
  memcpy(aDest.mBuffer, &mBuffer[aStart],
         substrLength * sizeof(UNICODE_CHAR));
  aDest.mLength = substrLength;

  return aDest;
}

void String::toLowerCase()
{
  PRUint32 conversionLoop;

  for (conversionLoop = 0; conversionLoop < mLength; ++conversionLoop) {
    if ((mBuffer[conversionLoop] >= 'A') &&
        (mBuffer[conversionLoop] <= 'Z'))
      mBuffer[conversionLoop] += 32;
  }
}

void String::toUpperCase()
{
  PRUint32 conversionLoop;

  for (conversionLoop = 0; conversionLoop < mLength; ++conversionLoop) {
    if ((mBuffer[conversionLoop] >= 'a') &&
        (mBuffer[conversionLoop] <= 'z'))
      mBuffer[conversionLoop] -= 32;
  }
}

String& String::operator = (const String& aSource)
{
  delete [] mBuffer;
  mBuffer = aSource.toUnicode();
  mBufferLength = aSource.mLength;
  mLength = aSource.mLength;
  return *this;
}

MBool String::ensureCapacity(PRUint32 aCapacity)
{
  PRUint32 needed = aCapacity + mLength;
  if (needed >= 0x8000) {
    NS_ASSERTION(0, "Asked for too large a buffer.");
    return MB_FALSE;
  }

  if (needed < mBufferLength) {
    return MB_TRUE;
  }

  UNICODE_CHAR* tempBuffer = new UNICODE_CHAR[needed];
  NS_ASSERTION(tempBuffer, "Couldn't allocate string buffer.");
  if (!tempBuffer) {
    return MB_FALSE;
  }

  if (mLength > 0) {
    memcpy(tempBuffer, mBuffer, mLength * sizeof(UNICODE_CHAR));
  }
  delete [] mBuffer;
  mBuffer = tempBuffer;
  mBufferLength = needed;
  return MB_TRUE;
}

UNICODE_CHAR* String::toUnicode() const
{
  if (mLength == 0) {
    return 0;
  }
  UNICODE_CHAR* tmpBuffer = new UNICODE_CHAR[mLength];
  NS_ASSERTION(tmpBuffer, "out of memory");
  if (tmpBuffer) {
    memcpy(tmpBuffer, mBuffer, mLength * sizeof(UNICODE_CHAR));
  }
  return tmpBuffer;
}

PRUint32 String::unicodeLength(const UNICODE_CHAR* aData)
{
  PRUint32 index = 0;

  // Count UNICODE_CHARs Until a Unicode "NULL" is found.
  while (aData[index] != 0x0000) {
    ++index;
  }
  return index;
}

ostream& operator<<(ostream& aOutput, const String& aSource)
{
  PRUint32 outputLoop;

  for (outputLoop = 0; outputLoop < aSource.mLength; ++outputLoop) {
    aOutput << (char)aSource.charAt(outputLoop);
  }
  return aOutput;
}

// XXX DEPRECATED
String::String(PRUint32 aSize) : mBuffer(0),
                                        mBufferLength(0),
                                        mLength(0)
{
  ensureCapacity(aSize);
}

String::String(const char* aSource) : mBuffer(0),
                                      mBufferLength(0),
                                      mLength(0)
{
  if (!aSource) {
    return;
  }

  PRUint32 length = strlen(aSource);
  if (!ensureCapacity(length)) {
    return;
  }
  PRUint32 counter;
  for (counter = 0; counter < length; ++counter) {
    mBuffer[counter] = (UNICODE_CHAR)aSource[counter];
  }
  mLength = length;
}

void String::append(const char* aSource)
{
  if (!aSource) {
    return;
  }

  PRUint32 length = strlen(aSource);
  if (!ensureCapacity(length)) {
    return;
  }
  PRUint32 counter;
  for (counter = 0; counter < length; ++counter) {
    mBuffer[mLength + counter] = (UNICODE_CHAR)aSource[counter];
  }
  mLength += length;
}

MBool String::isEqual(const char* aData) const
{
  if (!aData) {
    return MB_FALSE;
  }

  PRUint32 length = strlen(aData);
  if (length != mLength) {
    return MB_FALSE;
  }

  PRUint32 counter;
  for (counter = 0; counter < length; ++counter) {
    if (mBuffer[counter] != (UNICODE_CHAR)aData[counter]) {
      return MB_FALSE;
    }
  }
  return MB_TRUE;
}

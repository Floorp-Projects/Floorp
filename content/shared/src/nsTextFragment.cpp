/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsTextFragment.h"
#include "nsReadableUtils.h"

nsTextFragment::nsTextFragment(const nsTextFragment& aOther)
  : m1b(nsnull),
    mAllBits(0)
{
  if (aOther.Is2b()) {
    SetTo(aOther.Get2b(), aOther.GetLength());
  }
  else {
    SetTo(aOther.Get1b(), aOther.GetLength());
  }
}

nsTextFragment::nsTextFragment(const char* aString)
  : m1b(nsnull),
    mAllBits(0)
{
  SetTo(aString, strlen(aString));
}

nsTextFragment::nsTextFragment(const nsString& aString)
  : m1b(nsnull),
    mAllBits(0)
{
  SetTo(aString.GetUnicode(), aString.Length());
}

nsTextFragment&
nsTextFragment::operator=(const nsTextFragment& aOther)
{
  if (aOther.Is2b()) {
    SetTo(aOther.Get2b(), aOther.GetLength());
  }
  else {
    SetTo(aOther.Get1b(), aOther.GetLength());
  }
  return *this;
}

nsTextFragment&
nsTextFragment::operator=(const char* aString)
{
  SetTo(aString, nsCRT::strlen(aString));
  return *this;
}

nsTextFragment&
nsTextFragment::operator=(const PRUnichar* aString)
{
  SetTo(aString, nsCRT::strlen(aString));
  return *this;
}

nsTextFragment&
nsTextFragment::operator=(const nsAReadableString& aString)
{
  ReleaseText();

  PRUint32 length = aString.Length();
  if (length > 0) {
    if (IsASCII(aString)) {
      m1b = NS_REINTERPRET_CAST(unsigned char *, ToNewCString(aString));
      mState.mIs2b = 0;
    }
    else {
      m2b = ToNewUnicode(aString);
      mState.mIs2b = 1;
    }
    mState.mInHeap = 1;
    mState.mLength = (PRInt32)length;
  }

  return *this;
}

void
nsTextFragment::SetTo(PRUnichar* aBuffer, PRInt32 aLength, PRBool aRelease)
{
  ReleaseText();

  m2b = aBuffer;
  mState.mIs2b = 1;
  mState.mInHeap = aRelease ? 1 : 0;
  mState.mLength = aLength;
}

void
nsTextFragment::SetTo(const char* aBuffer, PRInt32 aLength)
{
  ReleaseText();
  if (0 != aLength) {
    unsigned char* nt = (unsigned char*)nsMemory::Alloc(aLength*sizeof(unsigned char));
    if (nsnull != nt) {
      nsCRT::memcpy(nt, aBuffer, sizeof(unsigned char) * aLength);

      m1b = nt;
      mState.mIs2b = 0;
      mState.mInHeap = 1;
      mState.mLength = aLength;
    }
  }
}

void
nsTextFragment::CopyTo(PRUnichar* aDest, PRInt32 aOffset, PRInt32 aCount)
{
  if (aOffset < 0) aOffset = 0;
  if (aOffset + aCount > GetLength()) {
    aCount = mState.mLength - aOffset;
  }
  if (0 != aCount) {
    if (mState.mIs2b) {
      nsCRT::memcpy(aDest, m2b + aOffset, sizeof(PRUnichar) * aCount);
    }
    else {
      unsigned char* cp = m1b + aOffset;
      unsigned char* end = cp + aCount;
      while (cp < end) {
        *aDest++ = PRUnichar(*cp++);
      }
    }
  }
}

void
nsTextFragment::CopyTo(char* aDest, PRInt32 aOffset, PRInt32 aCount)
{
  if (aOffset < 0) aOffset = 0;
  if (aOffset + aCount > GetLength()) {
    aCount = mState.mLength - aOffset;
  }
  if (0 != aCount) {
    if (mState.mIs2b) {
      PRUnichar* cp = m2b + aOffset;
      PRUnichar* end = cp + aCount;
      while (cp < end) {
        *aDest++ = (unsigned char) (*cp++);
      }
    }
    else {
      nsCRT::memcpy(aDest, m1b + aOffset, sizeof(char) * aCount);
    }
  }
}

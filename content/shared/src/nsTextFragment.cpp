/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsTextFragment.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
#endif

nsTextFragment::~nsTextFragment()
{
  ReleaseText();
}

void
nsTextFragment::ReleaseText()
{
  if (mState.mLength && m1b && mState.mInHeap) {
    if (mState.mIs2b) {
      nsMemory::Free(m2b);
    }
    else {
      nsMemory::Free(m1b);
    }
  }
  m1b = nsnull;
  mState.mIs2b = 0;
  mState.mInHeap = 0;
  mState.mLength = 0;
}

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

nsTextFragment::nsTextFragment(const PRUnichar* aString)
  : m1b(nsnull),
    mAllBits(0)
{
  SetTo(aString, nsCRT::strlen(aString));
}

nsTextFragment::nsTextFragment(const nsString& aString)
  : m1b(nsnull),
    mAllBits(0)
{
  SetTo(aString.get(), aString.Length());
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

#ifdef IBMBIDI
PRBool
#else
void
#endif
nsTextFragment::SetTo(const PRUnichar* aBuffer, PRInt32 aLength)
{
#ifdef IBMBIDI
  PRBool bidiEnabled = PR_FALSE;
#endif // IBMBIDI
  ReleaseText();
  if (0 != aLength) {
    // See if we need to store the data in ucs2 or not
    PRBool need2 = PR_FALSE;
    const PRUnichar* ucp = aBuffer;
    const PRUnichar* uend = aBuffer + aLength;
    while (ucp < uend) {
      PRUnichar ch = *ucp++;
      if (ch >> 8) {
        need2 = PR_TRUE;
#ifdef IBMBIDI
        if (CHAR_IS_BIDI(ch) ) {
          bidiEnabled = PR_TRUE;
#endif // IBMBIDI
        break;
#ifdef IBMBIDI
        }
#endif // IBMBIDI
      }
    }

    if (need2) {
      // Use ucs2 storage because we have to
      PRUnichar* nt = (PRUnichar*)nsMemory::Alloc(aLength*sizeof(PRUnichar));
      if (nsnull != nt) {
        // Copy data
        nsCRT::memcpy(nt, aBuffer, sizeof(PRUnichar) * aLength);

        // Setup our fields
        m2b = nt;
        mState.mIs2b = 1;
        mState.mInHeap = 1;
        mState.mLength = aLength;
      }
    }
    else {
      // Use 1 byte storage because we can
      unsigned char* nt = (unsigned char*)nsMemory::Alloc(aLength*sizeof(unsigned char));
      if (nsnull != nt) {
        // Copy data
        unsigned char* cp = nt;
        unsigned char* end = nt + aLength;
        while (cp < end) {
          *cp++ = (unsigned char) *aBuffer++;
        }

        // Setup our fields
        m1b = nt;
        mState.mIs2b = 0;
        mState.mInHeap = 1;
        mState.mLength = aLength;
      }
    }
  }
#ifdef IBMBIDI
  return bidiEnabled;
#endif // IBMBIDI
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
nsTextFragment::AppendTo(nsString& aString) const
{
  if (mState.mIs2b) {
    aString.Append(m2b, mState.mLength);
  }
  else {
    aString.AppendWithConversion((char*)m1b, mState.mLength);
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

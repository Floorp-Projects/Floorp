/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsTextFragment.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsBidiUtils.h"
#include "nsUnicharUtils.h"

// Static buffer used for newline fragments
static unsigned char sNewLineCharacter = '\n';


nsTextFragment::~nsTextFragment()
{
  ReleaseText();
}

void
nsTextFragment::ReleaseText()
{
  if (mState.mLength && m1b && mState.mInHeap) {
    unsigned char *buf = NS_CONST_CAST(unsigned char *, m1b);

    nsMemory::Free(buf); // m1b == m2b as far as nsMemory is concerned
  }

  m1b = nsnull;

  // Set mState.mIs2b, mState.mInHeap, and mState.mLength = 0 with mAllBits;
  mAllBits = 0;
}

nsTextFragment::nsTextFragment(const nsTextFragment& aOther)
  : m1b(nsnull), mAllBits(0)
{
  if (aOther.Is2b()) {
    SetTo(aOther.Get2b(), aOther.GetLength());
  } else {
    SetTo(aOther.Get1b(), aOther.GetLength());
  }
}

nsTextFragment::nsTextFragment(const char *aString)
  : m1b(nsnull), mAllBits(0)
{
  SetTo(aString, strlen(aString));
}

nsTextFragment::nsTextFragment(const PRUnichar *aString)
  : m1b(nsnull), mAllBits(0)
{
  SetTo(aString, nsCRT::strlen(aString));
}

nsTextFragment::nsTextFragment(const nsString& aString)
  : m1b(nsnull), mAllBits(0)
{
  SetTo(aString.get(), aString.Length());
}

nsTextFragment&
nsTextFragment::operator=(const nsTextFragment& aOther)
{
  if (aOther.Is2b()) {
    SetTo(aOther.Get2b(), aOther.GetLength());
  } else {
    SetTo(aOther.Get1b(), aOther.GetLength());
  }

  if (aOther.mState.mIsBidi) {
    // Carry over BIDI state from aOther
    mState.mIsBidi = PR_TRUE;
  }

  return *this;
}

nsTextFragment&
nsTextFragment::operator=(const char *aString)
{
  SetTo(aString, strlen(aString));

  return *this;
}

nsTextFragment&
nsTextFragment::operator=(const PRUnichar *aString)
{
  SetTo(aString, nsCRT::strlen(aString));

  return *this;
}

nsTextFragment&
nsTextFragment::operator=(const nsAString& aString)
{
  ReleaseText();

  PRUint32 length = aString.Length();

  if (length > 0) {
    PRBool in_heap = PR_TRUE;

    if (IsASCII(aString)) {
      if (length == 1 && aString.First() == '\n') {
        m1b = &sNewLineCharacter;

        in_heap = PR_FALSE;
      } else {
        m1b = (unsigned char *)ToNewCString(aString);
      }

      mState.mIs2b = PR_FALSE;
    } else {
      m2b = ToNewUnicode(aString);
      mState.mIs2b = PR_TRUE;
    }

    mState.mInHeap = in_heap;
    mState.mLength = length;
  }

  return *this;
}

void
nsTextFragment::SetTo(PRUnichar *aBuffer, PRInt32 aLength, PRBool aRelease)
{
  ReleaseText();

  m2b = aBuffer;
  mState.mIs2b = PR_TRUE;
  mState.mInHeap = aRelease;
  mState.mLength = aLength;
}

void
nsTextFragment::SetTo(const PRUnichar* aBuffer, PRInt32 aLength)
{
  ReleaseText();

  if (aLength != 0) {
    // See if we need to store the data in ucs2 or not
    PRBool need2 = PR_FALSE;
    const PRUnichar *ucp = aBuffer;
    const PRUnichar *uend = aBuffer + aLength;
    while (ucp < uend) {
      PRUnichar ch = *ucp++;
      if (ch >> 8) {
        need2 = PR_TRUE;
        break;
      }
    }

    if (need2) {
      // Use ucs2 storage because we have to
      m2b = (const PRUnichar *)nsMemory::Clone(aBuffer,
                                               aLength * sizeof(PRUnichar));

      if (!m2b) {
        NS_ERROR("Failed to clone string buffer!");

        return;
      }

      // Setup our fields
      mState.mIs2b = PR_TRUE;
      mState.mInHeap = PR_TRUE;
      mState.mLength = aLength;
    } else {
      // Use 1 byte storage because we can

      PRBool in_heap = PR_TRUE;

      if (aLength == 1 && *aBuffer == '\n') {
        m1b = &sNewLineCharacter;

        in_heap = PR_FALSE;
      } else {
        unsigned char *nt =
          (unsigned char *)nsMemory::Alloc(aLength * sizeof(char));

        if (!nt) {
          NS_ERROR("Failed to allocate string buffer!");

          return;
        }

        // Copy data
        for (PRUint32 i = 0; i < (PRUint32)aLength; ++i) {
          nt[i] = (unsigned char)aBuffer[i];
        }

        m1b = nt;
      }

      // Setup our fields
      mState.mIs2b = PR_FALSE;
      mState.mInHeap = in_heap;
      mState.mLength = aLength;
    }
  }
}

void
nsTextFragment::SetTo(const char *aBuffer, PRInt32 aLength)
{
  ReleaseText();
  if (aLength != 0) {
    PRBool in_heap = PR_TRUE;

    if (aLength == 1 && *aBuffer == '\n') {
      m1b = &sNewLineCharacter;

      in_heap = PR_FALSE;
    } else {
      m1b = (unsigned char *)nsMemory::Clone(aBuffer, aLength * sizeof(char));

      if (!m1b) {
        NS_ERROR("Failed to allocate string buffer!");

        return;
      }
    }

    // Setup our fields
    mState.mIs2b = PR_FALSE;
    mState.mInHeap = in_heap;
    mState.mLength = aLength;
  }
}

void
nsTextFragment::AppendTo(nsAString& aString) const
{
  if (mState.mIs2b) {
    aString.Append(m2b, mState.mLength);
  } else {
    AppendASCIItoUTF16(Substring((char *)m1b, ((char *)m1b) + mState.mLength),
                       aString);
  }
}

void
nsTextFragment::AppendTo(nsACString& aCString) const
{
  if (mState.mIs2b) {
    LossyAppendUTF16toASCII(Substring((PRUnichar *)m2b,
                                      (PRUnichar *)m2b + mState.mLength),
                            aCString);
  } else {
    aCString.Append((char *)m1b, mState.mLength);
  }
}

void
nsTextFragment::CopyTo(PRUnichar *aDest, PRInt32 aOffset, PRInt32 aCount)
{
  NS_ASSERTION(aOffset >= 0, "Bad offset passed to nsTextFragment::CopyTo()!");
  NS_ASSERTION(aCount >= 0, "Bad count passed to nsTextFragment::CopyTo()!");

  if (aOffset < 0) {
    aOffset = 0;
  }

  if (aOffset + aCount > GetLength()) {
    aCount = mState.mLength - aOffset;
  }

  if (aCount != 0) {
    if (mState.mIs2b) {
      memcpy(aDest, m2b + aOffset, sizeof(PRUnichar) * aCount);
    } else {
      unsigned const char *cp = m1b + aOffset;
      unsigned const char *end = cp + aCount;
      while (cp < end) {
        *aDest++ = PRUnichar(*cp++);
      }
    }
  }
}

void
nsTextFragment::CopyTo(char *aDest, PRInt32 aOffset, PRInt32 aCount)
{
  NS_ASSERTION(aOffset >= 0, "Bad offset passed to nsTextFragment::CopyTo()!");
  NS_ASSERTION(aCount >= 0, "Bad count passed to nsTextFragment::CopyTo()!");

  if (aOffset < 0) {
    aOffset = 0;
  }

  if (aOffset + aCount > GetLength()) {
    aCount = mState.mLength - aOffset;
  }

  if (aCount != 0) {
    if (mState.mIs2b) {
      const PRUnichar *cp = m2b + aOffset;
      const PRUnichar *end = cp + aCount;
      while (cp < end) {
        *aDest++ = (char)(*cp++);
      }
    } else {
      memcpy(aDest, m1b + aOffset, sizeof(char) * aCount);
    }
  }
}

// To save time we only do this when we really want to know, not during
// every allocation
void
nsTextFragment::SetBidiFlag()
{
  if (mState.mIs2b && !mState.mIsBidi) {
    const PRUnichar* cp = m2b;
    const PRUnichar* end = cp + mState.mLength;
    while (cp < end) {
      PRUnichar ch1 = *cp++;
      PRUint32 utf32Char = ch1;
      if (IS_HIGH_SURROGATE(ch1) &&
          cp < end &&
          IS_LOW_SURROGATE(*cp)) {
        PRUnichar ch2 = *cp++;
        utf32Char = SURROGATE_TO_UCS4(ch1, ch2);
      }
      if (UTF32_CHAR_IS_BIDI(utf32Char) ) {
        mState.mIsBidi = PR_TRUE;
        break;
      }
    }
  }
}

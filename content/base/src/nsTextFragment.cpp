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

/*
 * A class which represents a fragment of text (eg inside a text
 * node); if only codepoints below 256 are used, the text is stored as
 * a char*; otherwise the text is stored as a PRUnichar*
 */

#include "nsTextFragment.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsBidiUtils.h"
#include "nsUnicharUtils.h"
#include "nsUTF8Utils.h"
#include "mozilla/SSE.h"

#define TEXTFRAG_WHITE_AFTER_NEWLINE 50
#define TEXTFRAG_MAX_NEWLINES 7

// Static buffer used for common fragments
static char* sSpaceSharedString[TEXTFRAG_MAX_NEWLINES + 1];
static char* sTabSharedString[TEXTFRAG_MAX_NEWLINES + 1];
static char sSingleCharSharedString[256];

// static
nsresult
nsTextFragment::Init()
{
  // Create whitespace strings
  PRUint32 i;
  for (i = 0; i <= TEXTFRAG_MAX_NEWLINES; ++i) {
    sSpaceSharedString[i] = new char[1 + i + TEXTFRAG_WHITE_AFTER_NEWLINE];
    sTabSharedString[i] = new char[1 + i + TEXTFRAG_WHITE_AFTER_NEWLINE];
    NS_ENSURE_TRUE(sSpaceSharedString[i] && sTabSharedString[i],
                   NS_ERROR_OUT_OF_MEMORY);
    sSpaceSharedString[i][0] = ' ';
    sTabSharedString[i][0] = ' ';
    PRUint32 j;
    for (j = 1; j < 1 + i; ++j) {
      sSpaceSharedString[i][j] = '\n';
      sTabSharedString[i][j] = '\n';
    }
    for (; j < (1 + i + TEXTFRAG_WHITE_AFTER_NEWLINE); ++j) {
      sSpaceSharedString[i][j] = ' ';
      sTabSharedString[i][j] = '\t';
    }
  }

  // Create single-char strings
  for (i = 0; i < 256; ++i) {
    sSingleCharSharedString[i] = i;
  }

  return NS_OK;
}

// static
void
nsTextFragment::Shutdown()
{
  PRUint32  i;
  for (i = 0; i <= TEXTFRAG_MAX_NEWLINES; ++i) {
    delete [] sSpaceSharedString[i];
    delete [] sTabSharedString[i];
    sSpaceSharedString[i] = nsnull;
    sTabSharedString[i] = nsnull;
  }
}

nsTextFragment::~nsTextFragment()
{
  ReleaseText();
  MOZ_COUNT_DTOR(nsTextFragment);
}

void
nsTextFragment::ReleaseText()
{
  if (mState.mLength && m1b && mState.mInHeap) {
    nsMemory::Free(m2b); // m1b == m2b as far as nsMemory is concerned
  }

  m1b = nsnull;
  mState.mIsBidi = false;

  // Set mState.mIs2b, mState.mInHeap, and mState.mLength = 0 with mAllBits;
  mAllBits = 0;
}

nsTextFragment&
nsTextFragment::operator=(const nsTextFragment& aOther)
{
  ReleaseText();

  if (aOther.mState.mLength) {
    if (!aOther.mState.mInHeap) {
      m1b = aOther.m1b; // This will work even if aOther is using m2b
    }
    else {
      m2b = static_cast<PRUnichar*>
                       (nsMemory::Clone(aOther.m2b, aOther.mState.mLength *
                                    (aOther.mState.mIs2b ? sizeof(PRUnichar) : sizeof(char))));
    }

    if (m1b) {
      mAllBits = aOther.mAllBits;
    }
  }

  return *this;
}

static inline PRInt32
FirstNon8BitUnvectorized(const PRUnichar *str, const PRUnichar *end)
{
#if PR_BYTES_PER_WORD == 4
  const size_t mask = 0xff00ff00;
  const PRUint32 alignMask = 0x3;
  const PRUint32 numUnicharsPerWord = 2;
#elif PR_BYTES_PER_WORD == 8
  const size_t mask = 0xff00ff00ff00ff00;
  const PRUint32 alignMask = 0x7;
  const PRUint32 numUnicharsPerWord = 4;
#else
#error Unknown platform!
#endif

  const PRInt32 len = end - str;
  PRInt32 i = 0;

  // Align ourselves to a word boundary.
  PRInt32 alignLen =
    NS_MIN(len, PRInt32(((-NS_PTR_TO_INT32(str)) & alignMask) / sizeof(PRUnichar)));
  for (; i < alignLen; i++) {
    if (str[i] > 255)
      return i;
  }

  // Check one word at a time.
  const PRInt32 wordWalkEnd = ((len - i) / numUnicharsPerWord) * numUnicharsPerWord;
  for (; i < wordWalkEnd; i += numUnicharsPerWord) {
    const size_t word = *reinterpret_cast<const size_t*>(str + i);
    if (word & mask)
      return i;
  }

  // Take care of the remainder one character at a time.
  for (; i < len; i++) {
    if (str[i] > 255)
      return i;
  }

  return -1;
}

#ifdef MOZILLA_MAY_SUPPORT_SSE2
namespace mozilla {
  namespace SSE2 {
    PRInt32 FirstNon8Bit(const PRUnichar *str, const PRUnichar *end);
  }
}
#endif

/*
 * This function returns -1 if all characters in str are 8 bit characters.
 * Otherwise, it returns a value less than or equal to the index of the first
 * non-8bit character in str. For example, if first non-8bit character is at
 * position 25, it may return 25, or for example 24, or 16. But it guarantees
 * there is no non-8bit character before returned value.
 */
static inline PRInt32
FirstNon8Bit(const PRUnichar *str, const PRUnichar *end)
{
#ifdef MOZILLA_MAY_SUPPORT_SSE2
  if (mozilla::supports_sse2()) {
    return mozilla::SSE2::FirstNon8Bit(str, end);
  }
#endif

  return FirstNon8BitUnvectorized(str, end);
}

void
nsTextFragment::SetTo(const PRUnichar* aBuffer, PRInt32 aLength, bool aUpdateBidi)
{
  ReleaseText();

  if (aLength == 0) {
    return;
  }
  
  PRUnichar firstChar = *aBuffer;
  if (aLength == 1 && firstChar < 256) {
    m1b = sSingleCharSharedString + firstChar;
    mState.mInHeap = false;
    mState.mIs2b = false;
    mState.mLength = 1;

    return;
  }

  const PRUnichar *ucp = aBuffer;
  const PRUnichar *uend = aBuffer + aLength;

  // Check if we can use a shared string
  if (aLength <= 1 + TEXTFRAG_WHITE_AFTER_NEWLINE + TEXTFRAG_MAX_NEWLINES &&
     (firstChar == ' ' || firstChar == '\n' || firstChar == '\t')) {
    if (firstChar == ' ') {
      ++ucp;
    }

    const PRUnichar* start = ucp;
    while (ucp < uend && *ucp == '\n') {
      ++ucp;
    }
    const PRUnichar* endNewLine = ucp;

    PRUnichar space = ucp < uend && *ucp == '\t' ? '\t' : ' ';
    while (ucp < uend && *ucp == space) {
      ++ucp;
    }

    if (ucp == uend &&
        endNewLine - start <= TEXTFRAG_MAX_NEWLINES &&
        ucp - endNewLine <= TEXTFRAG_WHITE_AFTER_NEWLINE) {
      char** strings = space == ' ' ? sSpaceSharedString : sTabSharedString;
      m1b = strings[endNewLine - start];

      // If we didn't find a space in the beginning, skip it now.
      if (firstChar != ' ') {
        ++m1b;
      }

      mState.mInHeap = false;
      mState.mIs2b = false;
      mState.mLength = aLength;

      return;        
    }
  }

  // See if we need to store the data in ucs2 or not
  PRInt32 first16bit = FirstNon8Bit(ucp, uend);

  if (first16bit != -1) { // aBuffer contains no non-8bit character
    // Use ucs2 storage because we have to
    m2b = (PRUnichar *)nsMemory::Clone(aBuffer,
                                       aLength * sizeof(PRUnichar));
    if (!m2b) {
      return;
    }

    mState.mIs2b = true;
    if (aUpdateBidi) {
      UpdateBidiFlag(aBuffer + first16bit, aLength - first16bit);
    }

  } else {
    // Use 1 byte storage because we can
    char* buff = (char *)nsMemory::Alloc(aLength * sizeof(char));
    if (!buff) {
      return;
    }

    // Copy data
    LossyConvertEncoding16to8 converter(buff);
    copy_string(aBuffer, aBuffer+aLength, converter);
    m1b = buff;
    mState.mIs2b = false;
  }

  // Setup our fields
  mState.mInHeap = true;
  mState.mLength = aLength;
}

void
nsTextFragment::CopyTo(PRUnichar *aDest, PRInt32 aOffset, PRInt32 aCount)
{
  NS_ASSERTION(aOffset >= 0, "Bad offset passed to nsTextFragment::CopyTo()!");
  NS_ASSERTION(aCount >= 0, "Bad count passed to nsTextFragment::CopyTo()!");

  if (aOffset < 0) {
    aOffset = 0;
  }

  if (PRUint32(aOffset + aCount) > GetLength()) {
    aCount = mState.mLength - aOffset;
  }

  if (aCount != 0) {
    if (mState.mIs2b) {
      memcpy(aDest, m2b + aOffset, sizeof(PRUnichar) * aCount);
    } else {
      const char *cp = m1b + aOffset;
      const char *end = cp + aCount;
      LossyConvertEncoding8to16 converter(aDest);
      copy_string(cp, end, converter);
    }
  }
}

void
nsTextFragment::Append(const PRUnichar* aBuffer, PRUint32 aLength, bool aUpdateBidi)
{
  // This is a common case because some callsites create a textnode
  // with a value by creating the node and then calling AppendData.
  if (mState.mLength == 0) {
    SetTo(aBuffer, aLength, aUpdateBidi);

    return;
  }

  // Should we optimize for aData.Length() == 0?

  if (mState.mIs2b) {
    // Already a 2-byte string so the result will be too
    PRUnichar* buff = (PRUnichar*)nsMemory::Realloc(m2b, (mState.mLength + aLength) * sizeof(PRUnichar));
    if (!buff) {
      return;
    }

    memcpy(buff + mState.mLength, aBuffer, aLength * sizeof(PRUnichar));
    mState.mLength += aLength;
    m2b = buff;

    if (aUpdateBidi) {
      UpdateBidiFlag(aBuffer, aLength);
    }

    return;
  }

  // Current string is a 1-byte string, check if the new data fits in one byte too.
  PRInt32 first16bit = FirstNon8Bit(aBuffer, aBuffer + aLength);

  if (first16bit != -1) { // aBuffer contains no non-8bit character
    // The old data was 1-byte, but the new is not so we have to expand it
    // all to 2-byte
    PRUnichar* buff = (PRUnichar*)nsMemory::Alloc((mState.mLength + aLength) *
                                                  sizeof(PRUnichar));
    if (!buff) {
      return;
    }

    // Copy data into buff
    LossyConvertEncoding8to16 converter(buff);
    copy_string(m1b, m1b+mState.mLength, converter);

    memcpy(buff + mState.mLength, aBuffer, aLength * sizeof(PRUnichar));
    mState.mLength += aLength;
    mState.mIs2b = true;

    if (mState.mInHeap) {
      nsMemory::Free(m2b);
    }
    m2b = buff;

    mState.mInHeap = true;

    if (aUpdateBidi) {
      UpdateBidiFlag(aBuffer + first16bit, aLength - first16bit);
    }

    return;
  }

  // The new and the old data is all 1-byte
  char* buff;
  if (mState.mInHeap) {
    buff = (char*)nsMemory::Realloc(const_cast<char*>(m1b),
                                    (mState.mLength + aLength) * sizeof(char));
    if (!buff) {
      return;
    }
  }
  else {
    buff = (char*)nsMemory::Alloc((mState.mLength + aLength) * sizeof(char));
    if (!buff) {
      return;
    }

    memcpy(buff, m1b, mState.mLength);
    mState.mInHeap = true;
  }

  // Copy aBuffer into buff.
  LossyConvertEncoding16to8 converter(buff + mState.mLength);
  copy_string(aBuffer, aBuffer + aLength, converter);

  m1b = buff;
  mState.mLength += aLength;

}

/* virtual */ size_t
nsTextFragment::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  if (Is2b()) {
    return aMallocSizeOf(m2b);
  }

  if (mState.mInHeap) {
    return aMallocSizeOf(m1b);
  }

  return 0;
}

// To save time we only do this when we really want to know, not during
// every allocation
void
nsTextFragment::UpdateBidiFlag(const PRUnichar* aBuffer, PRUint32 aLength)
{
  if (mState.mIs2b && !mState.mIsBidi) {
    const PRUnichar* cp = aBuffer;
    const PRUnichar* end = cp + aLength;
    while (cp < end) {
      PRUnichar ch1 = *cp++;
      PRUint32 utf32Char = ch1;
      if (NS_IS_HIGH_SURROGATE(ch1) &&
          cp < end &&
          NS_IS_LOW_SURROGATE(*cp)) {
        PRUnichar ch2 = *cp++;
        utf32Char = SURROGATE_TO_UCS4(ch1, ch2);
      }
      if (UTF32_CHAR_IS_BIDI(utf32Char) || IS_BIDI_CONTROL_CHAR(utf32Char)) {
        mState.mIsBidi = true;
        break;
      }
    }
  }
}

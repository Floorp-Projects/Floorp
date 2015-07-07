/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class which represents a fragment of text (eg inside a text
 * node); if only codepoints below 256 are used, the text is stored as
 * a char*; otherwise the text is stored as a char16_t*
 */

#include "nsTextFragment.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsBidiUtils.h"
#include "nsUnicharUtils.h"
#include "nsUTF8Utils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SSE.h"
#include "nsTextFragmentImpl.h"
#include <algorithm>

#define TEXTFRAG_WHITE_AFTER_NEWLINE 50
#define TEXTFRAG_MAX_NEWLINES 7

// Static buffer used for common fragments
static char* sSpaceSharedString[TEXTFRAG_MAX_NEWLINES + 1];
static char* sTabSharedString[TEXTFRAG_MAX_NEWLINES + 1];
static char sSingleCharSharedString[256];

using mozilla::CheckedUint32;

// static
nsresult
nsTextFragment::Init()
{
  // Create whitespace strings
  uint32_t i;
  for (i = 0; i <= TEXTFRAG_MAX_NEWLINES; ++i) {
    sSpaceSharedString[i] = new char[1 + i + TEXTFRAG_WHITE_AFTER_NEWLINE];
    sTabSharedString[i] = new char[1 + i + TEXTFRAG_WHITE_AFTER_NEWLINE];
    NS_ENSURE_TRUE(sSpaceSharedString[i] && sTabSharedString[i],
                   NS_ERROR_OUT_OF_MEMORY);
    sSpaceSharedString[i][0] = ' ';
    sTabSharedString[i][0] = ' ';
    uint32_t j;
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
  uint32_t  i;
  for (i = 0; i <= TEXTFRAG_MAX_NEWLINES; ++i) {
    delete [] sSpaceSharedString[i];
    delete [] sTabSharedString[i];
    sSpaceSharedString[i] = nullptr;
    sTabSharedString[i] = nullptr;
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
    free(m2b); // m1b == m2b as far as free is concerned
  }

  m1b = nullptr;
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
      CheckedUint32 m2bSize = aOther.mState.mLength;
      m2bSize *= (aOther.mState.mIs2b ? sizeof(char16_t) : sizeof(char));
      m2b = nullptr;
      if (m2bSize.isValid()) {
        m2b = static_cast<char16_t*>(malloc(m2bSize.value()));
      }

      if (m2b) {
        memcpy(m2b, aOther.m2b, m2bSize.value());
      } else {
        // allocate a buffer for a single REPLACEMENT CHARACTER
        m2b = static_cast<char16_t*>(moz_xmalloc(sizeof(char16_t)));
        m2b[0] = 0xFFFD; // REPLACEMENT CHARACTER
        mState.mIs2b = true;
        mState.mInHeap = true;
        mState.mLength = 1;
      }
    }

    if (m1b) {
      mAllBits = aOther.mAllBits;
    }
  }

  return *this;
}

static inline int32_t
FirstNon8BitUnvectorized(const char16_t *str, const char16_t *end)
{
  typedef Non8BitParameters<sizeof(size_t)> p;
  const size_t mask = p::mask();
  const uint32_t alignMask = p::alignMask();
  const uint32_t numUnicharsPerWord = p::numUnicharsPerWord();
  const int32_t len = end - str;
  int32_t i = 0;

  // Align ourselves to a word boundary.
  int32_t alignLen =
    std::min(len, int32_t(((-NS_PTR_TO_INT32(str)) & alignMask) / sizeof(char16_t)));
  for (; i < alignLen; i++) {
    if (str[i] > 255)
      return i;
  }

  // Check one word at a time.
  const int32_t wordWalkEnd = ((len - i) / numUnicharsPerWord) * numUnicharsPerWord;
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
    int32_t FirstNon8Bit(const char16_t *str, const char16_t *end);
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
static inline int32_t
FirstNon8Bit(const char16_t *str, const char16_t *end)
{
#ifdef MOZILLA_MAY_SUPPORT_SSE2
  if (mozilla::supports_sse2()) {
    return mozilla::SSE2::FirstNon8Bit(str, end);
  }
#endif

  return FirstNon8BitUnvectorized(str, end);
}

bool
nsTextFragment::SetTo(const char16_t* aBuffer, int32_t aLength, bool aUpdateBidi)
{
  ReleaseText();

  if (aLength == 0) {
    return true;
  }
  
  char16_t firstChar = *aBuffer;
  if (aLength == 1 && firstChar < 256) {
    m1b = sSingleCharSharedString + firstChar;
    mState.mInHeap = false;
    mState.mIs2b = false;
    mState.mLength = 1;

    return true;
  }

  const char16_t *ucp = aBuffer;
  const char16_t *uend = aBuffer + aLength;

  // Check if we can use a shared string
  if (aLength <= 1 + TEXTFRAG_WHITE_AFTER_NEWLINE + TEXTFRAG_MAX_NEWLINES &&
     (firstChar == ' ' || firstChar == '\n' || firstChar == '\t')) {
    if (firstChar == ' ') {
      ++ucp;
    }

    const char16_t* start = ucp;
    while (ucp < uend && *ucp == '\n') {
      ++ucp;
    }
    const char16_t* endNewLine = ucp;

    char16_t space = ucp < uend && *ucp == '\t' ? '\t' : ' ';
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

      return true;        
    }
  }

  // See if we need to store the data in ucs2 or not
  int32_t first16bit = FirstNon8Bit(ucp, uend);

  if (first16bit != -1) { // aBuffer contains no non-8bit character
    // Use ucs2 storage because we have to
    CheckedUint32 m2bSize = aLength;
    m2bSize *= sizeof(char16_t);
    if (!m2bSize.isValid()) {
      return false;
    }

    m2b = static_cast<char16_t*>(malloc(m2bSize.value()));
    if (!m2b) {
      return false;
    }
    memcpy(m2b, aBuffer, m2bSize.value());

    mState.mIs2b = true;
    if (aUpdateBidi) {
      UpdateBidiFlag(aBuffer + first16bit, aLength - first16bit);
    }

  } else {
    // Use 1 byte storage because we can
    char* buff = static_cast<char*>(malloc(aLength));
    if (!buff) {
      return false;
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

  return true;
}

void
nsTextFragment::CopyTo(char16_t *aDest, int32_t aOffset, int32_t aCount)
{
  NS_ASSERTION(aOffset >= 0, "Bad offset passed to nsTextFragment::CopyTo()!");
  NS_ASSERTION(aCount >= 0, "Bad count passed to nsTextFragment::CopyTo()!");

  if (aOffset < 0) {
    aOffset = 0;
  }

  if (uint32_t(aOffset + aCount) > GetLength()) {
    aCount = mState.mLength - aOffset;
  }

  if (aCount != 0) {
    if (mState.mIs2b) {
      memcpy(aDest, m2b + aOffset, sizeof(char16_t) * aCount);
    } else {
      const char *cp = m1b + aOffset;
      const char *end = cp + aCount;
      LossyConvertEncoding8to16 converter(aDest);
      copy_string(cp, end, converter);
    }
  }
}

bool
nsTextFragment::Append(const char16_t* aBuffer, uint32_t aLength, bool aUpdateBidi)
{
  // This is a common case because some callsites create a textnode
  // with a value by creating the node and then calling AppendData.
  if (mState.mLength == 0) {
    return SetTo(aBuffer, aLength, aUpdateBidi);
  }

  // Should we optimize for aData.Length() == 0?

  CheckedUint32 length = mState.mLength;
  length += aLength;

  if (!length.isValid()) {
    return false;
  }

  if (mState.mIs2b) {
    length *= sizeof(char16_t);
    if (!length.isValid()) {
      return false;
    }

    // Already a 2-byte string so the result will be too
    char16_t* buff = static_cast<char16_t*>(realloc(m2b, length.value()));
    if (!buff) {
      return false;
    }

    memcpy(buff + mState.mLength, aBuffer, aLength * sizeof(char16_t));
    mState.mLength += aLength;
    m2b = buff;

    if (aUpdateBidi) {
      UpdateBidiFlag(aBuffer, aLength);
    }

    return true;
  }

  // Current string is a 1-byte string, check if the new data fits in one byte too.
  int32_t first16bit = FirstNon8Bit(aBuffer, aBuffer + aLength);

  if (first16bit != -1) { // aBuffer contains no non-8bit character
    length *= sizeof(char16_t);
    if (!length.isValid()) {
      return false;
    }

    // The old data was 1-byte, but the new is not so we have to expand it
    // all to 2-byte
    char16_t* buff = static_cast<char16_t*>(malloc(length.value()));
    if (!buff) {
      return false;
    }

    // Copy data into buff
    LossyConvertEncoding8to16 converter(buff);
    copy_string(m1b, m1b+mState.mLength, converter);

    memcpy(buff + mState.mLength, aBuffer, aLength * sizeof(char16_t));
    mState.mLength += aLength;
    mState.mIs2b = true;

    if (mState.mInHeap) {
      free(m2b);
    }
    m2b = buff;

    mState.mInHeap = true;

    if (aUpdateBidi) {
      UpdateBidiFlag(aBuffer + first16bit, aLength - first16bit);
    }

    return true;
  }

  // The new and the old data is all 1-byte
  char* buff;
  if (mState.mInHeap) {
    buff = static_cast<char*>(realloc(const_cast<char*>(m1b), length.value()));
    if (!buff) {
      return false;
    }
  }
  else {
    buff = static_cast<char*>(malloc(length.value()));
    if (!buff) {
      return false;
    }

    memcpy(buff, m1b, mState.mLength);
    mState.mInHeap = true;
  }

  // Copy aBuffer into buff.
  LossyConvertEncoding16to8 converter(buff + mState.mLength);
  copy_string(aBuffer, aBuffer + aLength, converter);

  m1b = buff;
  mState.mLength += aLength;

  return true;
}

/* virtual */ size_t
nsTextFragment::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
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
nsTextFragment::UpdateBidiFlag(const char16_t* aBuffer, uint32_t aLength)
{
  if (mState.mIs2b && !mState.mIsBidi) {
    const char16_t* cp = aBuffer;
    const char16_t* end = cp + aLength;
    while (cp < end) {
      char16_t ch1 = *cp++;
      uint32_t utf32Char = ch1;
      if (NS_IS_HIGH_SURROGATE(ch1) &&
          cp < end &&
          NS_IS_LOW_SURROGATE(*cp)) {
        char16_t ch2 = *cp++;
        utf32Char = SURROGATE_TO_UCS4(ch1, ch2);
      }
      if (UTF32_CHAR_IS_BIDI(utf32Char) || IsBidiControl(utf32Char)) {
        mState.mIsBidi = true;
        break;
      }
    }
  }
}

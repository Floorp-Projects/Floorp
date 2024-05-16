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
#include "nsBidiUtils.h"
#include "nsUnicharUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SSE.h"
#include "mozilla/ppc.h"
#include "nsTextFragmentImpl.h"
#include <algorithm>

#define TEXTFRAG_WHITE_AFTER_NEWLINE 50
#define TEXTFRAG_MAX_NEWLINES 7

// Static buffer used for common fragments
static char* sSpaceSharedString[TEXTFRAG_MAX_NEWLINES + 1];
static char* sTabSharedString[TEXTFRAG_MAX_NEWLINES + 1];
static char sSingleCharSharedString[256];

using namespace mozilla;

// static
nsresult nsTextFragment::Init() {
  // Create whitespace strings
  uint32_t i;
  for (i = 0; i <= TEXTFRAG_MAX_NEWLINES; ++i) {
    sSpaceSharedString[i] = new char[1 + i + TEXTFRAG_WHITE_AFTER_NEWLINE];
    sTabSharedString[i] = new char[1 + i + TEXTFRAG_WHITE_AFTER_NEWLINE];
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
void nsTextFragment::Shutdown() {
  uint32_t i;
  for (i = 0; i <= TEXTFRAG_MAX_NEWLINES; ++i) {
    delete[] sSpaceSharedString[i];
    delete[] sTabSharedString[i];
    sSpaceSharedString[i] = nullptr;
    sTabSharedString[i] = nullptr;
  }
}

nsTextFragment::~nsTextFragment() {
  ReleaseText();
  MOZ_COUNT_DTOR(nsTextFragment);
}

void nsTextFragment::ReleaseText() {
  if (mState.mIs2b) {
    NS_RELEASE(m2b);
  } else if (mState.mLength && m1b && mState.mInHeap) {
    free(const_cast<char*>(m1b));
  }

  m1b = nullptr;
  mState.mIsBidi = false;

  // Set mState.mIs2b, mState.mInHeap, and mState.mLength = 0 with mAllBits;
  mAllBits = 0;
}

nsTextFragment& nsTextFragment::operator=(const nsTextFragment& aOther) {
  ReleaseText();

  if (aOther.mState.mLength) {
    if (!aOther.mState.mInHeap) {
      MOZ_ASSERT(!aOther.mState.mIs2b);
      m1b = aOther.m1b;
    } else if (aOther.mState.mIs2b) {
      m2b = aOther.m2b;
      NS_ADDREF(m2b);
    } else {
      m1b = static_cast<char*>(malloc(aOther.mState.mLength));
      if (m1b) {
        memcpy(const_cast<char*>(m1b), aOther.m1b, aOther.mState.mLength);
      } else {
        // allocate a buffer for a single REPLACEMENT CHARACTER
        m2b = StringBuffer::Alloc(sizeof(char16_t) * 2).take();
        if (!m2b) {
          MOZ_CRASH("OOM!");
        }
        char16_t* data = static_cast<char16_t*>(m2b->Data());
        data[0] = 0xFFFD;  // REPLACEMENT CHARACTER
        data[1] = char16_t(0);
        mState.mIs2b = true;
        mState.mInHeap = true;
        mState.mLength = 1;
        return *this;
      }
    }

    mAllBits = aOther.mAllBits;
  }

  return *this;
}

static inline int32_t FirstNon8BitUnvectorized(const char16_t* str,
                                               const char16_t* end) {
  using p = Non8BitParameters<sizeof(size_t)>;
  const size_t mask = p::mask();
  const uint32_t alignMask = p::alignMask();
  const uint32_t numUnicharsPerWord = p::numUnicharsPerWord();
  const int32_t len = end - str;
  int32_t i = 0;

  // Align ourselves to a word boundary.
  int32_t alignLen = std::min(
      len, int32_t(((-NS_PTR_TO_INT32(str)) & alignMask) / sizeof(char16_t)));
  for (; i < alignLen; i++) {
    if (str[i] > 255) return i;
  }

  // Check one word at a time.
  const int32_t wordWalkEnd =
      ((len - i) / numUnicharsPerWord) * numUnicharsPerWord;
  for (; i < wordWalkEnd; i += numUnicharsPerWord) {
    const size_t word = *reinterpret_cast<const size_t*>(str + i);
    if (word & mask) return i;
  }

  // Take care of the remainder one character at a time.
  for (; i < len; i++) {
    if (str[i] > 255) return i;
  }

  return -1;
}

#if defined(MOZILLA_MAY_SUPPORT_SSE2)
#  include "nsTextFragmentGenericFwd.h"
#endif

#ifdef __powerpc__
namespace mozilla {
namespace VMX {
int32_t FirstNon8Bit(const char16_t* str, const char16_t* end);
}  // namespace VMX
}  // namespace mozilla
#endif

/*
 * This function returns -1 if all characters in str are 8 bit characters.
 * Otherwise, it returns a value less than or equal to the index of the first
 * non-8bit character in str. For example, if first non-8bit character is at
 * position 25, it may return 25, or for example 24, or 16. But it guarantees
 * there is no non-8bit character before returned value.
 */
static inline int32_t FirstNon8Bit(const char16_t* str, const char16_t* end) {
#ifdef MOZILLA_MAY_SUPPORT_SSE2
  if (mozilla::supports_sse2()) {
    return mozilla::FirstNon8Bit<xsimd::sse2>(str, end);
  }
#elif defined(__powerpc__)
  if (mozilla::supports_vmx()) {
    return mozilla::VMX::FirstNon8Bit(str, end);
  }
#endif

  return FirstNon8BitUnvectorized(str, end);
}

bool nsTextFragment::SetTo(const char16_t* aBuffer, uint32_t aLength,
                           bool aUpdateBidi, bool aForce2b) {
  if (MOZ_UNLIKELY(aLength > NS_MAX_TEXT_FRAGMENT_LENGTH)) {
    return false;
  }

  if (aForce2b && mState.mIs2b && !m2b->IsReadonly()) {
    // Try to re-use our existing StringBuffer.
    uint32_t storageSize = m2b->StorageSize();
    uint32_t neededSize = aLength * sizeof(char16_t);
    if (!neededSize) {
      if (storageSize < AutoStringDefaultStorageSize) {
        // If we're storing small enough StringBuffer, let's preserve it.
        static_cast<char16_t*>(m2b->Data())[0] = char16_t(0);
        mState.mLength = 0;
        mState.mIsBidi = false;
        return true;
      }
    } else if (neededSize < storageSize &&
               (storageSize / 2) <
                   (neededSize + AutoStringDefaultStorageSize)) {
      // Don't try to reuse the existing StringBuffer, if it would have lots of
      // unused space.
      memcpy(m2b->Data(), aBuffer, neededSize);
      static_cast<char16_t*>(m2b->Data())[aLength] = char16_t(0);
      mState.mLength = aLength;
      mState.mIsBidi = false;
      if (aUpdateBidi) {
        UpdateBidiFlag(aBuffer, aLength);
      }
      return true;
    }
  }

  if (aLength == 0) {
    ReleaseText();
    return true;
  }

  char16_t firstChar = *aBuffer;
  if (!aForce2b && aLength == 1 && firstChar < 256) {
    ReleaseText();
    m1b = sSingleCharSharedString + firstChar;
    mState.mInHeap = false;
    mState.mIs2b = false;
    mState.mLength = 1;
    return true;
  }

  const char16_t* ucp = aBuffer;
  const char16_t* uend = aBuffer + aLength;

  // Check if we can use a shared string
  if (!aForce2b &&
      aLength <= 1 + TEXTFRAG_WHITE_AFTER_NEWLINE + TEXTFRAG_MAX_NEWLINES &&
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

    if (ucp == uend && endNewLine - start <= TEXTFRAG_MAX_NEWLINES &&
        ucp - endNewLine <= TEXTFRAG_WHITE_AFTER_NEWLINE) {
      ReleaseText();
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
  int32_t first16bit = aForce2b ? 0 : FirstNon8Bit(ucp, uend);

  if (first16bit != -1) {  // aBuffer contains no non-8bit character
    // Use ucs2 storage because we have to
    CheckedUint32 size = CheckedUint32(aLength) + 1;
    if (!size.isValid()) {
      return false;
    }
    size *= sizeof(char16_t);
    if (!size.isValid()) {
      return false;
    }

    RefPtr<StringBuffer> newBuffer = StringBuffer::Alloc(size.value());
    if (!newBuffer) {
      return false;
    }

    ReleaseText();
    memcpy(newBuffer->Data(), aBuffer, aLength * sizeof(char16_t));
    static_cast<char16_t*>(newBuffer->Data())[aLength] = char16_t(0);

    m2b = newBuffer.forget().take();
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

    ReleaseText();
    // Copy data
    LossyConvertUtf16toLatin1(Span(aBuffer, aLength), Span(buff, aLength));
    m1b = buff;
    mState.mIs2b = false;
  }

  // Setup our fields
  mState.mInHeap = true;
  mState.mLength = aLength;

  return true;
}

void nsTextFragment::CopyTo(char16_t* aDest, uint32_t aOffset,
                            uint32_t aCount) {
  const CheckedUint32 endOffset = CheckedUint32(aOffset) + aCount;
  if (!endOffset.isValid() || endOffset.value() > GetLength()) {
    aCount = mState.mLength - aOffset;
  }

  if (aCount) {
    if (mState.mIs2b) {
      memcpy(aDest, Get2b() + aOffset, sizeof(char16_t) * aCount);
    } else {
      const char* cp = m1b + aOffset;
      ConvertLatin1toUtf16(Span(cp, aCount), Span(aDest, aCount));
    }
  }
}

bool nsTextFragment::Append(const char16_t* aBuffer, uint32_t aLength,
                            bool aUpdateBidi, bool aForce2b) {
  if (!aLength) {
    return true;
  }

  // This is a common case because some callsites create a textnode
  // with a value by creating the node and then calling AppendData.
  if (mState.mLength == 0) {
    return SetTo(aBuffer, aLength, aUpdateBidi, aForce2b);
  }

  // Should we optimize for aData.Length() == 0?

  // FYI: Don't use CheckedInt in this method since here is very hot path
  //      in some performance tests.
  if (NS_MAX_TEXT_FRAGMENT_LENGTH - mState.mLength < aLength) {
    return false;  // Would be overflown if we'd keep handling.
  }

  if (mState.mIs2b) {
    size_t size = mState.mLength + aLength + 1;
    if (SIZE_MAX / sizeof(char16_t) < size) {
      return false;  // Would be overflown if we'd keep handling.
    }
    size *= sizeof(char16_t);

    // Already a 2-byte string so the result will be too
    StringBuffer* buff = nullptr;
    StringBuffer* bufferToRelease = nullptr;
    if (m2b->IsReadonly()) {
      buff = StringBuffer::Alloc(size).take();
      if (!buff) {
        return false;
      }
      bufferToRelease = m2b;
      memcpy(static_cast<char16_t*>(buff->Data()), m2b->Data(),
             mState.mLength * sizeof(char16_t));
    } else {
      buff = StringBuffer::Realloc(m2b, size);
      if (!buff) {
        return false;
      }
    }

    char16_t* data = static_cast<char16_t*>(buff->Data());
    memcpy(data + mState.mLength, aBuffer, aLength * sizeof(char16_t));
    mState.mLength += aLength;
    m2b = buff;
    data[mState.mLength] = char16_t(0);

    NS_IF_RELEASE(bufferToRelease);

    if (aUpdateBidi) {
      UpdateBidiFlag(aBuffer, aLength);
    }

    return true;
  }

  // Current string is a 1-byte string, check if the new data fits in one byte
  // too.
  int32_t first16bit = aForce2b ? 0 : FirstNon8Bit(aBuffer, aBuffer + aLength);

  if (first16bit != -1) {  // aBuffer contains no non-8bit character
    size_t size = mState.mLength + aLength + 1;
    if (SIZE_MAX / sizeof(char16_t) < size) {
      return false;  // Would be overflown if we'd keep handling.
    }
    size *= sizeof(char16_t);

    // The old data was 1-byte, but the new is not so we have to expand it
    // all to 2-byte
    StringBuffer* buff = StringBuffer::Alloc(size).take();
    if (!buff) {
      return false;
    }

    // Copy data into buff
    char16_t* data = static_cast<char16_t*>(buff->Data());
    ConvertLatin1toUtf16(Span(m1b, mState.mLength), Span(data, mState.mLength));

    memcpy(data + mState.mLength, aBuffer, aLength * sizeof(char16_t));
    mState.mLength += aLength;
    mState.mIs2b = true;

    if (mState.mInHeap) {
      free(const_cast<char*>(m1b));
    }
    data[mState.mLength] = char16_t(0);
    m2b = buff;

    mState.mInHeap = true;

    if (aUpdateBidi) {
      UpdateBidiFlag(aBuffer + first16bit, aLength - first16bit);
    }

    return true;
  }

  // The new and the old data is all 1-byte
  size_t size = mState.mLength + aLength;
  MOZ_ASSERT(sizeof(char) == 1);
  char* buff;
  if (mState.mInHeap) {
    buff = static_cast<char*>(realloc(const_cast<char*>(m1b), size));
    if (!buff) {
      return false;
    }
  } else {
    buff = static_cast<char*>(malloc(size));
    if (!buff) {
      return false;
    }

    memcpy(buff, m1b, mState.mLength);
    mState.mInHeap = true;
  }

  // Copy aBuffer into buff.
  LossyConvertUtf16toLatin1(Span(aBuffer, aLength),
                            Span(buff + mState.mLength, aLength));

  m1b = buff;
  mState.mLength += aLength;

  return true;
}

/* virtual */
size_t nsTextFragment::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  if (Is2b()) {
    return m2b->SizeOfIncludingThisIfUnshared(aMallocSizeOf);
  }

  if (mState.mInHeap) {
    return aMallocSizeOf(m1b);
  }

  return 0;
}

// To save time we only do this when we really want to know, not during
// every allocation
void nsTextFragment::UpdateBidiFlag(const char16_t* aBuffer, uint32_t aLength) {
  if (mState.mIs2b && !mState.mIsBidi) {
    if (HasRTLChars(Span(aBuffer, aLength))) {
      mState.mIsBidi = true;
    }
  }
}

bool nsTextFragment::TextEquals(const nsTextFragment& aOther) const {
  if (!Is2b()) {
    // We're 1-byte.
    if (!aOther.Is2b()) {
      nsDependentCSubstring ourStr(Get1b(), GetLength());
      return ourStr.Equals(
          nsDependentCSubstring(aOther.Get1b(), aOther.GetLength()));
    }

    // We're 1-byte, the other thing is 2-byte.  Instead of implementing a
    // separate codepath for this, just use our code below.
    return aOther.TextEquals(*this);
  }

  nsDependentSubstring ourStr(Get2b(), GetLength());
  if (aOther.Is2b()) {
    return ourStr.Equals(
        nsDependentSubstring(aOther.Get2b(), aOther.GetLength()));
  }

  // We can't use EqualsASCII here, because the other string might not
  // actually be ASCII.  Just roll our own compare; do it in the simple way.
  // Bug 1532356 tracks not having to roll our own.
  if (GetLength() != aOther.GetLength()) {
    return false;
  }

  const char16_t* ourChars = Get2b();
  const char* otherChars = aOther.Get1b();
  for (uint32_t i = 0; i < GetLength(); ++i) {
    if (ourChars[i] != static_cast<char16_t>(otherChars[i])) {
      return false;
    }
  }

  return true;
}

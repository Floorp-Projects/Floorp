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

#ifndef nsTextFragment_h___
#define nsTextFragment_h___

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

#include "nsCharTraits.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsReadableUtils.h"
#include "nsISupportsImpl.h"

// XXX should this normalize the code to keep a \u0000 at the end?

// XXX nsTextFragmentPool?

/**
 * A fragment of text. If mIs2b is 1 then the m2b pointer is valid
 * otherwise the m1b pointer is valid. If m1b is used then each byte
 * of data represents a single ucs2 character with the high byte being
 * zero.
 *
 * This class does not have a virtual destructor therefore it is not
 * meant to be subclassed.
 */
class nsTextFragment final {
 public:
  static nsresult Init();
  static void Shutdown();

  /**
   * Default constructor. Initialize the fragment to be empty.
   */
  nsTextFragment() : m1b(nullptr), mAllBits(0) {
    MOZ_COUNT_CTOR(nsTextFragment);
    NS_ASSERTION(sizeof(FragmentBits) == 4, "Bad field packing!");
  }

  ~nsTextFragment();

  /**
   * Change the contents of this fragment to be a copy of the
   * the argument fragment, or to "" if unable to allocate enough memory.
   */
  nsTextFragment& operator=(const nsTextFragment& aOther);

  /**
   * Return true if this fragment is represented by char16_t data
   */
  bool Is2b() const { return mState.mIs2b; }

  /**
   * Return true if this fragment contains Bidi text
   * For performance reasons this flag is only set if explicitely requested (by
   * setting the aUpdateBidi argument on SetTo or Append to true).
   */
  bool IsBidi() const { return mState.mIsBidi; }

  /**
   * Get a pointer to constant char16_t data.
   */
  const char16_t* Get2b() const {
    MOZ_ASSERT(Is2b(), "not 2b text");
    return static_cast<char16_t*>(m2b->Data());
  }

  /**
   * Get a pointer to constant char data.
   */
  const char* Get1b() const {
    NS_ASSERTION(!Is2b(), "not 1b text");
    return (const char*)m1b;
  }

  /**
   * Get the length of the fragment. The length is the number of logical
   * characters, not the number of bytes to store the characters.
   */
  uint32_t GetLength() const { return mState.mLength; }

  bool CanGrowBy(size_t n) const {
    return n < (1 << 29) && mState.mLength + n < (1 << 29);
  }

  /**
   * Change the contents of this fragment to be a copy of the given
   * buffer. If aUpdateBidi is true, contents of the fragment will be scanned,
   * and mState.mIsBidi will be turned on if it includes any Bidi characters.
   * If aForce2b is true, aBuffer will be stored as char16_t as is.  Then,
   * you can access the value faster but may waste memory if all characters
   * are less than U+0100.
   */
  bool SetTo(const char16_t* aBuffer, int32_t aLength, bool aUpdateBidi,
             bool aForce2b);

  bool SetTo(const nsString& aString, bool aUpdateBidi, bool aForce2b) {
    ReleaseText();
    if (aForce2b && !aUpdateBidi) {
      nsStringBuffer* buffer = nsStringBuffer::FromString(aString);
      if (buffer) {
        NS_ADDREF(m2b = buffer);
        mState.mInHeap = true;
        mState.mIs2b = true;
        mState.mLength = aString.Length();
        return true;
      }
    }

    return SetTo(aString.get(), aString.Length(), aUpdateBidi, aForce2b);
  }

  /**
   * Append aData to the end of this fragment. If aUpdateBidi is true, contents
   * of the fragment will be scanned, and mState.mIsBidi will be turned on if
   * it includes any Bidi characters.
   * If aForce2b is true, the string will be stored as char16_t as is.  Then,
   * you can access the value faster but may waste memory if all characters
   * are less than U+0100.
   */
  bool Append(const char16_t* aBuffer, uint32_t aLength, bool aUpdateBidi,
              bool aForce2b);

  /**
   * Append the contents of this string fragment to aString
   */
  void AppendTo(nsAString& aString) const {
    if (!AppendTo(aString, mozilla::fallible)) {
      aString.AllocFailed(aString.Length() + GetLength());
    }
  }

  /**
   * Append the contents of this string fragment to aString
   * @return false if an out of memory condition is detected, true otherwise
   */
  [[nodiscard]] bool AppendTo(nsAString& aString,
                              const mozilla::fallible_t& aFallible) const {
    if (mState.mIs2b) {
      if (aString.IsEmpty()) {
        m2b->ToString(mState.mLength, aString);
        return true;
      }
      bool ok = aString.Append(Get2b(), mState.mLength, aFallible);
      if (!ok) {
        return false;
      }

      return true;
    } else {
      return AppendASCIItoUTF16(Substring(m1b, mState.mLength), aString,
                                aFallible);
    }
  }

  /**
   * Append a substring of the contents of this string fragment to aString.
   * @param aOffset where to start the substring in this text fragment
   * @param aLength the length of the substring
   */
  void AppendTo(nsAString& aString, int32_t aOffset, int32_t aLength) const {
    if (!AppendTo(aString, aOffset, aLength, mozilla::fallible)) {
      aString.AllocFailed(aString.Length() + aLength);
    }
  }

  /**
   * Append a substring of the contents of this string fragment to aString.
   * @param aString the string in which to append
   * @param aOffset where to start the substring in this text fragment
   * @param aLength the length of the substring
   * @return false if an out of memory condition is detected, true otherwise
   */
  [[nodiscard]] bool AppendTo(nsAString& aString, int32_t aOffset,
                              int32_t aLength,
                              const mozilla::fallible_t& aFallible) const {
    if (mState.mIs2b) {
      bool ok = aString.Append(Get2b() + aOffset, aLength, aFallible);
      if (!ok) {
        return false;
      }

      return true;
    } else {
      return AppendASCIItoUTF16(Substring(m1b + aOffset, aLength), aString,
                                aFallible);
    }
  }

  /**
   * Make a copy of the fragments contents starting at offset for
   * count characters. The offset and count will be adjusted to
   * lie within the fragments data. The fragments data is converted if
   * necessary.
   */
  void CopyTo(char16_t* aDest, int32_t aOffset, int32_t aCount);

  /**
   * Return the character in the text-fragment at the given
   * index. This always returns a char16_t.
   */
  char16_t CharAt(int32_t aIndex) const {
    MOZ_ASSERT(uint32_t(aIndex) < mState.mLength, "bad index");
    return mState.mIs2b ? Get2b()[aIndex]
                        : static_cast<unsigned char>(m1b[aIndex]);
  }

  /**
   * IsHighSurrogateFollowedByLowSurrogateAt() returns true if character at
   * aIndex is high surrogate and it's followed by low surrogate.
   */
  inline bool IsHighSurrogateFollowedByLowSurrogateAt(int32_t aIndex) const {
    MOZ_ASSERT(aIndex >= 0);
    MOZ_ASSERT(aIndex < mState.mLength);
    if (!mState.mIs2b || aIndex + 1 >= mState.mLength) {
      return false;
    }
    return NS_IS_SURROGATE_PAIR(Get2b()[aIndex], Get2b()[aIndex + 1]);
  }

  /**
   * IsLowSurrogateFollowingHighSurrogateAt() returns true if character at
   * aIndex is low surrogate and it follows high surrogate.
   */
  inline bool IsLowSurrogateFollowingHighSurrogateAt(int32_t aIndex) const {
    MOZ_ASSERT(aIndex >= 0);
    MOZ_ASSERT(aIndex < mState.mLength);
    if (!mState.mIs2b || aIndex <= 0) {
      return false;
    }
    return NS_IS_SURROGATE_PAIR(Get2b()[aIndex - 1], Get2b()[aIndex]);
  }

  /**
   * ScalarValueAt() returns a Unicode scalar value at aIndex.  If the character
   * at aIndex is a high surrogate followed by low surrogate, returns character
   * code for the pair.  If the index is low surrogate, or a high surrogate but
   * not in a pair, returns 0.
   */
  inline char32_t ScalarValueAt(int32_t aIndex) const {
    MOZ_ASSERT(aIndex >= 0);
    MOZ_ASSERT(aIndex < mState.mLength);
    if (!mState.mIs2b) {
      return static_cast<unsigned char>(m1b[aIndex]);
    }
    char16_t ch = Get2b()[aIndex];
    if (!IS_SURROGATE(ch)) {
      return ch;
    }
    if (aIndex + 1 < mState.mLength && NS_IS_HIGH_SURROGATE(ch)) {
      char16_t nextCh = Get2b()[aIndex + 1];
      if (NS_IS_LOW_SURROGATE(nextCh)) {
        return SURROGATE_TO_UCS4(ch, nextCh);
      }
    }
    return 0;
  }

  void SetBidi(bool aBidi) { mState.mIsBidi = aBidi; }

  struct FragmentBits {
    // uint32_t to ensure that the values are unsigned, because we
    // want 0/1, not 0/-1!
    // Making these bool causes Windows to not actually pack them,
    // which causes crashes because we assume this structure is no more than
    // 32 bits!
    uint32_t mInHeap : 1;
    uint32_t mIs2b : 1;
    uint32_t mIsBidi : 1;
    // Note that when you change the bits of mLength, you also need to change
    // NS_MAX_TEXT_FRAGMENT_LENGTH.
    uint32_t mLength : 29;
  };

#define NS_MAX_TEXT_FRAGMENT_LENGTH (static_cast<uint32_t>(0x1FFFFFFF))

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  /**
   * Check whether the text in this fragment is the same as the text in the
   * other fragment.
   */
  [[nodiscard]] bool TextEquals(const nsTextFragment& aOther) const;

 private:
  void ReleaseText();

  /**
   * Scan the contents of the fragment and turn on mState.mIsBidi if it
   * includes any Bidi characters.
   */
  void UpdateBidiFlag(const char16_t* aBuffer, uint32_t aLength);

  union {
    nsStringBuffer* m2b;
    const char* m1b;  // This is const since it can point to shared data
  };

  union {
    uint32_t mAllBits;
    FragmentBits mState;
  };
};

#endif /* nsTextFragment_h___ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FakeString_h__
#define mozilla_dom_FakeString_h__

#include "nsString.h"
#include "nsStringBuffer.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Span.h"

namespace mozilla {
namespace dom {
namespace binding_detail {
// A struct that has a layout compatible with nsAString, so that
// reinterpret-casting a FakeString as a const nsAString is safe, but much
// faster constructor and destructor behavior. FakeString uses inline storage
// for small strings and an nsStringBuffer for longer strings.  It can also
// point to a literal (static-lifetime) string that's compiled into the binary,
// or point at the buffer of an nsAString whose lifetime is longer than that of
// the FakeString.
struct FakeString {
  FakeString()
      : mDataFlags(nsString::DataFlags::TERMINATED),
        mClassFlags(nsString::ClassFlags(0)) {}

  ~FakeString() {
    if (mDataFlags & nsString::DataFlags::REFCOUNTED) {
      MOZ_ASSERT(mDataInitialized);
      nsStringBuffer::FromData(mData)->Release();
    }
  }

  // Share aString's string buffer, if it has one; otherwise, make this string
  // depend upon aString's data.  aString should outlive this instance of
  // FakeString.
  void ShareOrDependUpon(const nsAString& aString) {
    RefPtr<nsStringBuffer> sharedBuffer = nsStringBuffer::FromString(aString);
    if (!sharedBuffer) {
      InitData(aString.BeginReading(), aString.Length());
      if (!aString.IsTerminated()) {
        mDataFlags &= ~nsString::DataFlags::TERMINATED;
      }
    } else {
      AssignFromStringBuffer(sharedBuffer.forget(), aString.Length());
    }
  }

  void Truncate() { InitData(nsString::char_traits::sEmptyBuffer, 0); }

  void SetIsVoid(bool aValue) {
    MOZ_ASSERT(aValue, "We don't support SetIsVoid(false) on FakeString!");
    Truncate();
    mDataFlags |= nsString::DataFlags::VOIDED;
  }

  nsString::char_type* BeginWriting() {
    MOZ_ASSERT(IsMutable());
    MOZ_ASSERT(mDataInitialized);
    return mData;
  }

  nsString::size_type Length() const { return mLength; }

  operator mozilla::Span<const nsString::char_type>() const {
    MOZ_ASSERT(mDataInitialized);
    return mozilla::MakeSpan(mData, Length());
  }

  operator mozilla::Span<nsString::char_type>() {
    return mozilla::MakeSpan(BeginWriting(), Length());
  }

  // Reserve space to write aLength chars, not including null-terminator.
  bool SetLength(nsString::size_type aLength, mozilla::fallible_t const&) {
    // Use mInlineStorage for small strings.
    if (aLength < sInlineCapacity) {
      InitData(mInlineStorage, aLength);
      mDataFlags |= nsString::DataFlags::INLINE;
    } else {
      RefPtr<nsStringBuffer> buf =
          nsStringBuffer::Alloc((aLength + 1) * sizeof(nsString::char_type));
      if (MOZ_UNLIKELY(!buf)) {
        return false;
      }

      AssignFromStringBuffer(buf.forget(), aLength);
    }

    MOZ_ASSERT(mDataInitialized);
    mData[mLength] = char16_t(0);
    return true;
  }

  // Returns false on allocation failure.
  bool EnsureMutable() {
    MOZ_ASSERT(mDataInitialized);

    if (IsMutable()) {
      return true;
    }

    RefPtr<nsStringBuffer> buffer;
    if (mDataFlags & nsString::DataFlags::REFCOUNTED) {
      // Make sure we'll drop it when we're done.
      buffer = dont_AddRef(nsStringBuffer::FromData(mData));
      // And make sure we don't release it twice by accident.
    }
    const nsString::char_type* oldChars = mData;

    mDataFlags = nsString::DataFlags::TERMINATED;
#ifdef DEBUG
    // Reset mDataInitialized because we're explicitly reinitializing
    // it via the SetLength call.
    mDataInitialized = false;
#endif  // DEBUG
    // SetLength will make sure we have our own buffer to work with.  Note that
    // we may be transitioning from having a (short) readonly stringbuffer to
    // our inline storage or whatnot.  That's all fine; SetLength is responsible
    // for setting up our flags correctly.
    if (!SetLength(Length(), fallible)) {
      return false;
    }
    MOZ_ASSERT(oldChars != mData, "Should have new chars now!");
    MOZ_ASSERT(IsMutable(), "Why are we still not mutable?");
    memcpy(mData, oldChars, Length() * sizeof(nsString::char_type));
    return true;
  }

  void AssignFromStringBuffer(already_AddRefed<nsStringBuffer> aBuffer,
                              size_t aLength) {
    InitData(static_cast<nsString::char_type*>(aBuffer.take()->Data()),
             aLength);
    mDataFlags |= nsString::DataFlags::REFCOUNTED;
  }

  // The preferred way to assign literals to a FakeString.  This should only be
  // called with actual C++ literal strings (i.e. u"stuff") or character arrays
  // that originally come from passing such literal strings.
  template <int N>
  void AssignLiteral(const nsString::char_type (&aData)[N]) {
    AssignLiteral(aData, N - 1);
  }

  // Assign a literal to a FakeString when it's not an actual literal
  // in the code, but is known to be a literal somehow (e.g. it came
  // from an nsAString that tested true for IsLiteral()).
  void AssignLiteral(const nsString::char_type* aData, size_t aLength) {
    InitData(aData, aLength);
    mDataFlags |= nsString::DataFlags::LITERAL;
  }

  // If this ever changes, change the corresponding code in the
  // Optional<nsAString> specialization as well.
  const nsAString* ToAStringPtr() const {
    return reinterpret_cast<const nsString*>(this);
  }

  operator const nsAString&() const {
    return *reinterpret_cast<const nsString*>(this);
  }

 private:
  nsAString* ToAStringPtr() { return reinterpret_cast<nsString*>(this); }

  // mData is left uninitialized for optimization purposes.
  MOZ_INIT_OUTSIDE_CTOR nsString::char_type* mData;
  // mLength is left uninitialized for optimization purposes.
  MOZ_INIT_OUTSIDE_CTOR nsString::size_type mLength;
  nsString::DataFlags mDataFlags;
  nsString::ClassFlags mClassFlags;

  static const size_t sInlineCapacity = 64;
  nsString::char_type mInlineStorage[sInlineCapacity];
#ifdef DEBUG
  bool mDataInitialized = false;
#endif  // DEBUG

  FakeString(const FakeString& other) = delete;
  void operator=(const FakeString& other) = delete;

  void InitData(const nsString::char_type* aData, nsString::size_type aLength) {
    MOZ_ASSERT(mDataFlags == nsString::DataFlags::TERMINATED);
    MOZ_ASSERT(!mDataInitialized);
    mData = const_cast<nsString::char_type*>(aData);
    mLength = aLength;
#ifdef DEBUG
    mDataInitialized = true;
#endif  // DEBUG
  }

  bool IsMutable() {
    return (mDataFlags & nsString::DataFlags::INLINE) ||
           ((mDataFlags & nsString::DataFlags::REFCOUNTED) &&
            !nsStringBuffer::FromData(mData)->IsReadonly());
  }

  friend class NonNull<nsAString>;

  // A class to use for our static asserts to ensure our object layout
  // matches that of nsString.
  class StringAsserter;
  friend class StringAsserter;

  class StringAsserter : public nsString {
   public:
    static void StaticAsserts() {
      static_assert(offsetof(FakeString, mInlineStorage) == sizeof(nsString),
                    "FakeString should include all nsString members");
      static_assert(
          offsetof(FakeString, mData) == offsetof(StringAsserter, mData),
          "Offset of mData should match");
      static_assert(
          offsetof(FakeString, mLength) == offsetof(StringAsserter, mLength),
          "Offset of mLength should match");
      static_assert(offsetof(FakeString, mDataFlags) ==
                        offsetof(StringAsserter, mDataFlags),
                    "Offset of mDataFlags should match");
      static_assert(offsetof(FakeString, mClassFlags) ==
                        offsetof(StringAsserter, mClassFlags),
                    "Offset of mClassFlags should match");
    }
  };
};
}  // namespace binding_detail
}  // namespace dom
}  // namespace mozilla

inline void AssignFromStringBuffer(
    nsStringBuffer* aBuffer, size_t aLength,
    mozilla::dom::binding_detail::FakeString& aDest) {
  aDest.AssignFromStringBuffer(do_AddRef(aBuffer), aLength);
}

#endif /* mozilla_dom_FakeString_h__ */

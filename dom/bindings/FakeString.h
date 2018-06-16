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

namespace mozilla {
namespace dom {
namespace binding_detail {
// A struct that has the same layout as an nsString but much faster
// constructor and destructor behavior. FakeString uses inline storage
// for small strings and a nsStringBuffer for longer strings.
struct FakeString {
  FakeString() :
    mDataFlags(nsString::DataFlags::TERMINATED),
    mClassFlags(nsString::ClassFlags(0))
  {
  }

  ~FakeString() {
    if (mDataFlags & nsString::DataFlags::REFCOUNTED) {
      nsStringBuffer::FromData(mData)->Release();
    }
  }

  void Rebind(const nsString::char_type* aData, nsString::size_type aLength) {
    MOZ_ASSERT(mDataFlags == nsString::DataFlags::TERMINATED);
    mData = const_cast<nsString::char_type*>(aData);
    mLength = aLength;
  }

  // Share aString's string buffer, if it has one; otherwise, make this string
  // depend upon aString's data.  aString should outlive this instance of
  // FakeString.
  void ShareOrDependUpon(const nsAString& aString) {
    RefPtr<nsStringBuffer> sharedBuffer = nsStringBuffer::FromString(aString);
    if (!sharedBuffer) {
      Rebind(aString.Data(), aString.Length());
    } else {
      AssignFromStringBuffer(sharedBuffer.forget());
      mLength = aString.Length();
    }
  }

  void Truncate() {
    MOZ_ASSERT(mDataFlags == nsString::DataFlags::TERMINATED);
    mData = nsString::char_traits::sEmptyBuffer;
    mLength = 0;
  }

  void SetIsVoid(bool aValue) {
    MOZ_ASSERT(aValue,
               "We don't support SetIsVoid(false) on FakeString!");
    Truncate();
    mDataFlags |= nsString::DataFlags::VOIDED;
  }

  const nsString::char_type* Data() const
  {
    return mData;
  }

  nsString::char_type* BeginWriting()
  {
    return mData;
  }

  nsString::size_type Length() const
  {
    return mLength;
  }

  // Reserve space to write aLength chars, not including null-terminator.
  bool SetLength(nsString::size_type aLength, mozilla::fallible_t const&) {
    // Use mInlineStorage for small strings.
    if (aLength < sInlineCapacity) {
      SetData(mInlineStorage);
    } else {
      RefPtr<nsStringBuffer> buf = nsStringBuffer::Alloc((aLength + 1) * sizeof(nsString::char_type));
      if (MOZ_UNLIKELY(!buf)) {
        return false;
      }

      AssignFromStringBuffer(buf.forget());
    }
    mLength = aLength;
    mData[mLength] = char16_t(0);
    return true;
  }

  // If this ever changes, change the corresponding code in the
  // Optional<nsAString> specialization as well.
  const nsAString* ToAStringPtr() const {
    return reinterpret_cast<const nsString*>(this);
  }

operator const nsAString& () const {
    return *reinterpret_cast<const nsString*>(this);
  }

private:
  nsAString* ToAStringPtr() {
    return reinterpret_cast<nsString*>(this);
  }

  // mData is left uninitialized for optimization purposes.
  MOZ_INIT_OUTSIDE_CTOR nsString::char_type* mData;
  // mLength is left uninitialized for optimization purposes.
  MOZ_INIT_OUTSIDE_CTOR nsString::size_type mLength;
  nsString::DataFlags mDataFlags;
  nsString::ClassFlags mClassFlags;

  static const size_t sInlineCapacity = 64;
  nsString::char_type mInlineStorage[sInlineCapacity];

  FakeString(const FakeString& other) = delete;
  void operator=(const FakeString& other) = delete;

  void SetData(nsString::char_type* aData) {
    MOZ_ASSERT(mDataFlags == nsString::DataFlags::TERMINATED);
    mData = const_cast<nsString::char_type*>(aData);
  }
  void AssignFromStringBuffer(already_AddRefed<nsStringBuffer> aBuffer) {
    SetData(static_cast<nsString::char_type*>(aBuffer.take()->Data()));
    mDataFlags = nsString::DataFlags::REFCOUNTED | nsString::DataFlags::TERMINATED;
  }

  friend class NonNull<nsAString>;

  // A class to use for our static asserts to ensure our object layout
  // matches that of nsString.
  class StringAsserter;
  friend class StringAsserter;

  class StringAsserter : public nsString {
  public:
    static void StaticAsserts() {
      static_assert(offsetof(FakeString, mInlineStorage) ==
                      sizeof(nsString),
                    "FakeString should include all nsString members");
      static_assert(offsetof(FakeString, mData) ==
                      offsetof(StringAsserter, mData),
                    "Offset of mData should match");
      static_assert(offsetof(FakeString, mLength) ==
                      offsetof(StringAsserter, mLength),
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
} // namespace binding_detail
} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_FakeString_h__ */

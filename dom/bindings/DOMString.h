/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMString_h
#define mozilla_dom_DOMString_h

#include "nsString.h"
#include "nsStringBuffer.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nsDOMString.h"
#include "nsAtom.h"

namespace mozilla {
namespace dom {

/**
 * A class for representing string return values.  This can be either passed to
 * callees that have an nsString or nsAString out param or passed to a callee
 * that actually knows about this class and can work with it.  Such a callee may
 * call SetKnownLiveStringBuffer or SetStringBuffer or SetKnownLiveString or
 * SetKnownLiveAtom on this object.  It's only OK to call
 * SetKnownLiveStringBuffer/SetKnownLiveString/SetKnownLiveAtom if the caller of
 * the method in question plans to keep holding a strong ref to the stringbuffer
 * involved, whether it's a raw nsStringBuffer, or stored inside the string or
 * atom being passed.  In the string/atom cases that means the caller must own
 * the string or atom, and not mutate it (in the string case) for the lifetime
 * of the DOMString.
 *
 * The proper way to store a value in this class is to either to do nothing
 * (which leaves this as an empty string), to call
 * SetKnownLiveStringBuffer/SetStringBuffer with a non-null stringbuffer, to
 * call SetKnownLiveString, to call SetKnownLiveAtom, to call SetNull(), or to
 * call AsAString() and set the value in the resulting nsString.  These options
 * are mutually exclusive! Don't do more than one of them.
 *
 * The proper way to extract a value is to check IsNull().  If not null, then
 * check HasStringBuffer().  If that's true, check for a zero length, and if the
 * length is nonzero call StringBuffer().  If the length is zero this is the
 * empty string.  If HasStringBuffer() returns false, call AsAString() and get
 * the value from that.
 */
class MOZ_STACK_CLASS DOMString {
public:
  DOMString()
    : mStringBuffer(nullptr)
    , mLength(0)
    , mIsNull(false)
    , mStringBufferOwned(false)
  {}
  ~DOMString()
  {
    MOZ_ASSERT(!mString || !mStringBuffer,
               "Shouldn't have both present!");
    if (mStringBufferOwned) {
      MOZ_ASSERT(mStringBuffer);
      mStringBuffer->Release();
    }
  }

  operator nsString&()
  {
    return AsAString();
  }

  // It doesn't make any sense to convert a DOMString to a const nsString or
  // nsAString reference; this class is meant for outparams only.
  operator const nsString&() = delete;
  operator const nsAString&() = delete;

  nsString& AsAString()
  {
    MOZ_ASSERT(!mStringBuffer, "We already have a stringbuffer?");
    MOZ_ASSERT(!mIsNull, "We're already set as null");
    if (!mString) {
      mString.emplace();
    }
    return *mString;
  }

  bool HasStringBuffer() const
  {
    MOZ_ASSERT(!mString || !mStringBuffer,
               "Shouldn't have both present!");
    MOZ_ASSERT(!mIsNull, "Caller should have checked IsNull() first");
    return !mString;
  }

  // Get the stringbuffer.  This can only be called if HasStringBuffer()
  // returned true and StringBufferLength() is nonzero.  If that's true, it will
  // never return null.  Note that constructing a string from this
  // nsStringBuffer with length given by StringBufferLength() might give you
  // something that is not null-terminated.
  nsStringBuffer* StringBuffer() const
  {
    MOZ_ASSERT(!mIsNull, "Caller should have checked IsNull() first");
    MOZ_ASSERT(HasStringBuffer(),
               "Don't ask for the stringbuffer if we don't have it");
    MOZ_ASSERT(StringBufferLength() != 0, "Why are you asking for this?");
    MOZ_ASSERT(mStringBuffer,
               "If our length is nonzero, we better have a stringbuffer.");
    return mStringBuffer;
  }

  // Get the length of the stringbuffer.  Can only be called if
  // HasStringBuffer().
  uint32_t StringBufferLength() const
  {
    MOZ_ASSERT(HasStringBuffer(), "Don't call this if there is no stringbuffer");
    return mLength;
  }

  // Tell the DOMString to relinquish ownership of its nsStringBuffer to the
  // caller.  Can only be called if HasStringBuffer().
  void RelinquishBufferOwnership()
  {
    MOZ_ASSERT(HasStringBuffer(), "Don't call this if there is no stringbuffer");
    if (mStringBufferOwned) {
      // Just hand that ref over.
      mStringBufferOwned = false;
    } else {
      // Caller should end up holding a ref.
      mStringBuffer->AddRef();
    }
  }

  // Initialize the DOMString to a (nsStringBuffer, length) pair.  The length
  // does NOT have to be the full length of the (null-terminated) string in the
  // nsStringBuffer.
  void SetKnownLiveStringBuffer(nsStringBuffer* aStringBuffer, uint32_t aLength)
  {
    MOZ_ASSERT(mString.isNothing(), "We already have a string?");
    MOZ_ASSERT(!mIsNull, "We're already set as null");
    MOZ_ASSERT(!mStringBuffer, "Setting stringbuffer twice?");
    MOZ_ASSERT(aStringBuffer, "Why are we getting null?");
    mStringBuffer = aStringBuffer;
    mLength = aLength;
  }

  // Like SetKnownLiveStringBuffer, but holds a reference to the nsStringBuffer.
  void SetStringBuffer(nsStringBuffer* aStringBuffer, uint32_t aLength)
  {
    // We rely on SetKnownLiveStringBuffer to ensure our state invariants.
    SetKnownLiveStringBuffer(aStringBuffer, aLength);
    aStringBuffer->AddRef();
    mStringBufferOwned = true;
  }

  void SetKnownLiveString(const nsAString& aString)
  {
    MOZ_ASSERT(mString.isNothing(), "We already have a string?");
    MOZ_ASSERT(!mIsNull, "We're already set as null");
    MOZ_ASSERT(!mStringBuffer, "Setting stringbuffer twice?");
    nsStringBuffer* buf = nsStringBuffer::FromString(aString);
    if (buf) {
      SetKnownLiveStringBuffer(buf, aString.Length());
    } else if (aString.IsVoid()) {
      SetNull();
    } else if (!aString.IsEmpty()) {
      AsAString() = aString;
    }
  }

  enum NullHandling
  {
    eTreatNullAsNull,
    eTreatNullAsEmpty,
    eNullNotExpected
  };

  void SetKnownLiveAtom(nsAtom* aAtom, NullHandling aNullHandling)
  {
    MOZ_ASSERT(mString.isNothing(), "We already have a string?");
    MOZ_ASSERT(!mIsNull, "We're already set as null");
    MOZ_ASSERT(!mStringBuffer, "Setting stringbuffer twice?");
    MOZ_ASSERT(aAtom || aNullHandling != eNullNotExpected);
    if (aNullHandling == eNullNotExpected || aAtom) {
      if (aAtom->IsStaticAtom()) {
        // XXX: bug 1407858 will replace this with a direct assignment of the
        // static atom that doesn't go via nsString.
        AsAString().AssignLiteral(aAtom->GetUTF16String(), aAtom->GetLength());
      } else {
        // Dynamic atoms always have a string buffer.
        SetKnownLiveStringBuffer(aAtom->GetStringBuffer(), aAtom->GetLength());
      }
    } else if (aNullHandling == eTreatNullAsNull) {
      SetNull();
    }
  }

  void SetNull()
  {
    MOZ_ASSERT(!mStringBuffer, "Should have no stringbuffer if null");
    MOZ_ASSERT(mString.isNothing(), "Should have no string if null");
    mIsNull = true;
  }

  bool IsNull() const
  {
    MOZ_ASSERT(!mStringBuffer || mString.isNothing(),
               "How could we have a stringbuffer and a nonempty string?");
    return mIsNull || (mString && mString->IsVoid());
  }

  void ToString(nsAString& aString)
  {
    if (IsNull()) {
      SetDOMStringToNull(aString);
    } else if (HasStringBuffer()) {
      if (StringBufferLength() == 0) {
        aString.Truncate();
      } else {
        // Don't share the nsStringBuffer with aString if the result would not
        // be null-terminated.
        nsStringBuffer* buf = StringBuffer();
        uint32_t len = StringBufferLength();
        auto chars = static_cast<char16_t*>(buf->Data());
        if (chars[len] == '\0') {
          // Safe to share the buffer.
          buf->ToString(len, aString);
        } else {
          // We need to copy, unfortunately.
          aString.Assign(chars, len);
        }
      }
    } else {
      aString = AsAString();
    }
  }

private:
  // We need to be able to act like a string as needed
  Maybe<nsAutoString> mString;

  // For callees that know we exist, we can be a stringbuffer/length/null-flag
  // triple.
  nsStringBuffer* MOZ_UNSAFE_REF("The ways in which this can be safe are "
                                 "documented above and enforced through "
                                 "assertions") mStringBuffer;
  uint32_t mLength;
  bool mIsNull;
  bool mStringBufferOwned;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMString_h

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
 * call these setters:
 *
 *   SetKnownLiveStringBuffer
 *   SetStringBuffer
 *   SetKnownLiveString
 *   SetKnownLiveAtom
 *   SetNull
 *
 * to assign a value to the DOMString without instantiating an actual nsString
 * in the process, or use AsAString() to instantiate an nsString and work with
 * it.  These options are mutually exclusive!  Don't do more than one of them.
 *
 * It's only OK to call
 * SetKnownLiveStringBuffer/SetKnownLiveString/SetKnownLiveAtom if the caller of
 * the method in question plans to keep holding a strong ref to the stringbuffer
 * involved, whether it's a raw nsStringBuffer, or stored inside the string or
 * atom being passed.  In the string/atom cases that means the caller must own
 * the string or atom, and not mutate it (in the string case) for the lifetime
 * of the DOMString.
 *
 * The proper way to extract a value is to check IsNull().  If not null, then
 * check IsEmpty().  If neither of those is true, check HasStringBuffer().  If
 * that's true, call StringBuffer()/StringBufferLength().  If HasStringBuffer()
 * returns false, check HasLiteral, and if that returns true call
 * Literal()/LiteralLength().  If HasLiteral() is false, call AsAString() and
 * get the value from that.
 */
class MOZ_STACK_CLASS DOMString {
public:
  DOMString()
    : mStringBuffer(nullptr)
    , mLength(0)
    , mState(State::Empty)
  {}
  ~DOMString()
  {
    MOZ_ASSERT(!mString || !mStringBuffer,
               "Shouldn't have both present!");
    if (mState == State::OwnedStringBuffer) {
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
    MOZ_ASSERT(mState == State::Empty || mState == State::String,
               "Moving from nonempty state to another nonempty state?");
    MOZ_ASSERT(!mStringBuffer, "We already have a stringbuffer?");
    if (!mString) {
      mString.emplace();
      mState = State::String;
    }
    return *mString;
  }

  bool HasStringBuffer() const
  {
    MOZ_ASSERT(!mString || !mStringBuffer,
               "Shouldn't have both present!");
    MOZ_ASSERT(mState > State::Null,
               "Caller should have checked IsNull() and IsEmpty() first");
    return mState >= State::OwnedStringBuffer;
  }

  // Get the stringbuffer.  This can only be called if HasStringBuffer()
  // returned true.  If that's true, it will never return null.  Note that
  // constructing a string from this nsStringBuffer with length given by
  // StringBufferLength() might give you something that is not null-terminated.
  nsStringBuffer* StringBuffer() const
  {
    MOZ_ASSERT(HasStringBuffer(),
               "Don't ask for the stringbuffer if we don't have it");
    MOZ_ASSERT(mStringBuffer,
               "We better have a stringbuffer if we claim to");
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
    if (mState == State::OwnedStringBuffer) {
      // Just hand that ref over.
      mState = State::UnownedStringBuffer;
    } else {
      // Caller should end up holding a ref.
      mStringBuffer->AddRef();
    }
  }

  bool HasLiteral() const
  {
    MOZ_ASSERT(!mString || !mStringBuffer,
               "Shouldn't have both present!");
    MOZ_ASSERT(mState > State::Null,
               "Caller should have checked IsNull() and IsEmpty() first");
    return mState == State::Literal;
  }

  // Get the literal string.  This can only be called if HasLiteral()
  // returned true.  If that's true, it will never return null.
  const char16_t* Literal() const
  {
    MOZ_ASSERT(HasLiteral(),
               "Don't ask for the literal if we don't have it");
    MOZ_ASSERT(mLiteral,
               "We better have a literal if we claim to");
    return mLiteral;
  }

  // Get the length of the literal.  Can only be called if HasLiteral().
  uint32_t LiteralLength() const
  {
    MOZ_ASSERT(HasLiteral(), "Don't call this if there is no literal");
    return mLength;
  }

  // Initialize the DOMString to a (nsStringBuffer, length) pair.  The length
  // does NOT have to be the full length of the (null-terminated) string in the
  // nsStringBuffer.
  void SetKnownLiveStringBuffer(nsStringBuffer* aStringBuffer, uint32_t aLength)
  {
    MOZ_ASSERT(mState == State::Empty, "We're already set to a value");
    if (aLength != 0) {
      SetStringBufferInternal(aStringBuffer, aLength);
      mState = State::UnownedStringBuffer;
    }
    // else nothing to do
  }

  // Like SetKnownLiveStringBuffer, but holds a reference to the nsStringBuffer.
  void SetStringBuffer(nsStringBuffer* aStringBuffer, uint32_t aLength)
  {
    MOZ_ASSERT(mState == State::Empty, "We're already set to a value");
    if (aLength != 0) {
      SetStringBufferInternal(aStringBuffer, aLength);
      aStringBuffer->AddRef();
      mState = State::OwnedStringBuffer;
    }
    // else nothing to do
  }

  void SetKnownLiveString(const nsAString& aString)
  {
    MOZ_ASSERT(mString.isNothing(), "We already have a string?");
    MOZ_ASSERT(mState == State::Empty, "We're already set to a value");
    MOZ_ASSERT(!mStringBuffer, "Setting stringbuffer twice?");
    if (MOZ_UNLIKELY(aString.IsVoid())) {
      SetNull();
    } else if (!aString.IsEmpty()) {
      nsStringBuffer* buf = nsStringBuffer::FromString(aString);
      if (buf) {
        SetKnownLiveStringBuffer(buf, aString.Length());
      } else if (aString.IsLiteral()) {
        SetLiteralInternal(aString.BeginReading(), aString.Length());
      } else {
        AsAString() = aString;
      }
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
    MOZ_ASSERT(mState == State::Empty, "We're already set to a value");
    MOZ_ASSERT(!mStringBuffer, "Setting stringbuffer twice?");
    MOZ_ASSERT(aAtom || aNullHandling != eNullNotExpected);
    if (aNullHandling == eNullNotExpected || aAtom) {
      if (aAtom->IsStaticAtom()) {
        // Static atoms are backed by literals.
        SetLiteralInternal(aAtom->GetUTF16String(), aAtom->GetLength());
      } else {
        // Dynamic atoms always have a string buffer and never have 0 length,
        // because nsGkAtoms::_empty is a static atom.
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
    MOZ_ASSERT(mState == State::Empty, "Already set to a value?");
    mState = State::Null;
  }

  bool IsNull() const
  {
    MOZ_ASSERT(!mStringBuffer || mString.isNothing(),
               "How could we have a stringbuffer and a nonempty string?");
    return mState == State::Null || (mString && mString->IsVoid());
  }

  bool IsEmpty() const
  {
    MOZ_ASSERT(!mStringBuffer || mString.isNothing(),
               "How could we have a stringbuffer and a nonempty string?");
    // This is not exact, because we might still have an empty XPCOM string.
    // But that's OK; in that case the callers will try the XPCOM string
    // themselves.
    return mState == State::Empty;
  }

  void ToString(nsAString& aString)
  {
    if (IsNull()) {
      SetDOMStringToNull(aString);
    } else if (IsEmpty()) {
      aString.Truncate();
    } else if (HasStringBuffer()) {
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
    } else if (HasLiteral()) {
      aString.AssignLiteral(Literal(), LiteralLength());
    } else {
      aString = AsAString();
    }
  }

private:
  void SetStringBufferInternal(nsStringBuffer* aStringBuffer, uint32_t aLength)
  {
    MOZ_ASSERT(mString.isNothing(), "We already have a string?");
    MOZ_ASSERT(mState == State::Empty, "We're already set to a value");
    MOZ_ASSERT(!mStringBuffer, "Setting stringbuffer twice?");
    MOZ_ASSERT(aStringBuffer, "Why are we getting null?");
    MOZ_ASSERT(aLength != 0, "Should not have empty string here");
    mStringBuffer = aStringBuffer;
    mLength = aLength;
  }

  void SetLiteralInternal(const char16_t* aLiteral, uint32_t aLength)
  {
    MOZ_ASSERT(!mLiteral, "What's going on here?");
    mLiteral = aLiteral;
    mLength = aLength;
    mState = State::Literal;
  }

  enum class State : uint8_t
  {
    Empty, // An empty string.  Default state.
    Null,  // Null (not a string at all)

    // All states that involve actual string data should come after
    // Empty and Null.

    String, // An XPCOM string stored in mString.
    Literal, // A string literal (static lifetime).
    OwnedStringBuffer, // mStringBuffer is valid and we have a ref to it.
    UnownedStringBuffer, // mStringBuffer is valid; we are not holding a ref.
    // The two string buffer values must come last.  This lets us avoid doing
    // two tests to figure out whether we have a stringbuffer.
  };

  // We need to be able to act like a string as needed
  Maybe<nsAutoString> mString;

  union
  {
    // The nsStringBuffer in the OwnedStringBuffer/UnownedStringBuffer cases.
    nsStringBuffer* MOZ_UNSAFE_REF("The ways in which this can be safe are "
                                 "documented above and enforced through "
                                 "assertions") mStringBuffer;
    // The literal in the Literal case.
    const char16_t* mLiteral;
  };

  // Length in the stringbuffer and literal cases.
  uint32_t mLength;

  State mState;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMString_h

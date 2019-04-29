/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSValue_h
#define mozilla_dom_localstorage_LSValue_h

namespace mozilla {
namespace dom {

/**
 * Represents a LocalStorage value. From content's perspective, values (if
 * present) are always DOMStrings. This is also true from a quota-tracking
 * perspective. However, for memory and disk efficiency it's preferable to store
 * the value in alternate representations. The LSValue type exists to support
 * these alternate representations in future.
 */
class LSValue final {
  friend struct IPC::ParamTraits<LSValue>;

  nsString mBuffer;

 public:
  LSValue() {}

  explicit LSValue(const nsAString& aBuffer) : mBuffer(aBuffer) {}

  bool IsVoid() const { return mBuffer.IsVoid(); }

  void SetIsVoid(bool aVal) { mBuffer.SetIsVoid(aVal); }

  uint32_t Length() const { return mBuffer.Length(); }

  bool Equals(const LSValue& aOther) const {
    return mBuffer == aOther.mBuffer &&
           mBuffer.IsVoid() == aOther.mBuffer.IsVoid();
  }

  bool operator==(const LSValue& aOther) const { return Equals(aOther); }

  bool operator!=(const LSValue& aOther) const { return !Equals(aOther); }

  operator const nsString&() const { return mBuffer; }
};

const LSValue& VoidLSValue();

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_LSValue_h

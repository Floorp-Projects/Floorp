/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSValue_h
#define mozilla_dom_localstorage_LSValue_h

#include "SnappyUtils.h"

namespace mozilla {
namespace dom {

/**
 * Represents a LocalStorage value. From content's perspective, values (if
 * present) are always DOMStrings. This is also true from a quota-tracking
 * perspective. However, for memory and disk efficiency it's preferable to store
 * the value in alternate compressed or utf-8 encoding representations. The
 * LSValue type exists to support these alternate representations, dynamically
 * decompressing/re-encoding to utf-16 while still tracking value size on a
 * utf-16 basis for quota purposes.
 */
class LSValue final {
  friend struct IPC::ParamTraits<LSValue>;

  nsCString mBuffer;
  uint32_t mUTF16Length;
  bool mCompressed;

 public:
  LSValue() : mUTF16Length(0), mCompressed(false) {}

  explicit LSValue(const nsACString& aBuffer, uint32_t aUTF16Length,
                   bool aCompressed)
      : mBuffer(aBuffer),
        mUTF16Length(aUTF16Length),
        mCompressed(aCompressed) {}

  explicit LSValue(const nsAString& aBuffer) : mUTF16Length(aBuffer.Length()) {
    if (aBuffer.IsVoid()) {
      mBuffer.SetIsVoid(true);
      mCompressed = false;
    } else {
      CopyUTF16toUTF8(aBuffer, mBuffer);
      nsCString buffer;
      if ((mCompressed = SnappyCompress(mBuffer, buffer))) {
        mBuffer = buffer;
      }
    }
  }

  bool IsVoid() const { return mBuffer.IsVoid(); }

  void SetIsVoid(bool aVal) { mBuffer.SetIsVoid(aVal); }

  /**
   * This represents the "physical" length that the parent process uses for
   * the size of value/item computation. This can also be used to see how much
   * memory the value is using at rest or what the cost is for sending the value
   * over IPC.
   */
  uint32_t Length() const { return mBuffer.Length(); }

  /*
   * This represents the "logical" length that content sees and that is also
   * used for quota management purposes.
   */
  uint32_t UTF16Length() const { return mUTF16Length; }

  bool IsCompressed() const { return mCompressed; }

  bool Equals(const LSValue& aOther) const {
    return mBuffer == aOther.mBuffer &&
           mBuffer.IsVoid() == aOther.mBuffer.IsVoid() &&
           mUTF16Length == aOther.mUTF16Length &&
           mCompressed == aOther.mCompressed;
  }

  bool operator==(const LSValue& aOther) const { return Equals(aOther); }

  bool operator!=(const LSValue& aOther) const { return !Equals(aOther); }

  operator const nsCString&() const { return mBuffer; }

  operator Span<const char>() const { return mBuffer; }

  class Converter {
    nsString mBuffer;

   public:
    explicit Converter(const LSValue& aValue) {
      if (aValue.mBuffer.IsVoid()) {
        mBuffer.SetIsVoid(true);
      } else if (aValue.mCompressed) {
        nsCString buffer;
        MOZ_ALWAYS_TRUE(SnappyUncompress(aValue.mBuffer, buffer));
        CopyUTF8toUTF16(buffer, mBuffer);
      } else {
        CopyUTF8toUTF16(aValue.mBuffer, mBuffer);
      }
    }
    Converter(Converter&& aOther) : mBuffer(aOther.mBuffer) {}
    ~Converter() {}

    operator const nsString&() const { return mBuffer; }

   private:
    Converter() = delete;
    Converter(const Converter&) = delete;
    Converter& operator=(const Converter&) = delete;
    Converter& operator=(const Converter&&) = delete;
  };

  Converter AsString() const { return Converter(const_cast<LSValue&>(*this)); }
};

const LSValue& VoidLSValue();

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_LSValue_h

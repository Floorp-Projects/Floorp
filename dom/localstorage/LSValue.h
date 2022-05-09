/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSValue_h
#define mozilla_dom_localstorage_LSValue_h

#include <cstdint>
#include "ErrorList.h"
#include "SnappyUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Span.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTStringRepr.h"

class mozIStorageStatement;

namespace IPC {
template <typename>
struct ParamTraits;
}

namespace mozilla::dom {

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

 public:
  enum class ConversionType : uint8_t {
    NONE = 0u,
    UTF16_UTF8 = 1u,
    NUM_TYPES = 2u
  };

  enum class CompressionType : uint8_t {
    UNCOMPRESSED = 0u,
    SNAPPY = 1u,
    NUM_TYPES = 2u
  };

  nsCString mBuffer;
  uint32_t mUTF16Length;
  ConversionType mConversionType;
  CompressionType mCompressionType;

  explicit LSValue()
      : mUTF16Length(0u),
        mConversionType(ConversionType::NONE),
        mCompressionType(CompressionType::UNCOMPRESSED) {
    SetIsVoid(true);
  }

  bool InitFromString(const nsAString& aBuffer);

  nsresult InitFromStatement(mozIStorageStatement* aStatement, uint32_t aIndex);

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

  ConversionType GetConversionType() const { return mConversionType; }

  CompressionType GetCompressionType() const { return mCompressionType; }

  bool Equals(const LSValue& aOther) const {
    return mBuffer == aOther.mBuffer &&
           mBuffer.IsVoid() == aOther.mBuffer.IsVoid() &&
           mUTF16Length == aOther.mUTF16Length &&
           mConversionType == aOther.mConversionType &&
           mCompressionType == aOther.mCompressionType;
  }

  bool operator==(const LSValue& aOther) const { return Equals(aOther); }

  bool operator!=(const LSValue& aOther) const { return !Equals(aOther); }

  constexpr const nsCString& AsCString() const { return mBuffer; }

  class Converter {
    nsString mBuffer;

   public:
    explicit Converter(const LSValue& aValue);
    Converter(Converter&& aOther) = default;
    ~Converter() = default;

    operator const nsString&() const { return mBuffer; }

   private:
    Converter() = delete;
    Converter(const Converter&) = delete;
    Converter& operator=(const Converter&) = delete;
    Converter& operator=(const Converter&&) = delete;
  };

  Converter AsString() const { return Converter{*this}; }
};

const LSValue& VoidLSValue();

/**
 * XXX: This function doesn't have to be public
 * once the support for shadow writes is removed.
 */
bool PutCStringBytesToString(const nsACString& aSrc, nsString& aDest);

}  // namespace mozilla::dom

#endif  // mozilla_dom_localstorage_LSValue_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CryptoBuffer.h"
#include "secitem.h"
#include "mozilla/Base64.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla::dom {

uint8_t* CryptoBuffer::Assign(const CryptoBuffer& aData) {
  // Same as in nsTArray_Impl::operator=, but return the value
  // returned from ReplaceElementsAt to enable OOM detection
  return ReplaceElementsAt(0, Length(), aData.Elements(), aData.Length(),
                           fallible);
}

uint8_t* CryptoBuffer::Assign(const uint8_t* aData, uint32_t aLength) {
  return ReplaceElementsAt(0, Length(), aData, aLength, fallible);
}

uint8_t* CryptoBuffer::Assign(const nsACString& aString) {
  return Assign(reinterpret_cast<uint8_t const*>(aString.BeginReading()),
                aString.Length());
}

uint8_t* CryptoBuffer::Assign(const SECItem* aItem) {
  MOZ_ASSERT(aItem);
  return Assign(aItem->data, aItem->len);
}

uint8_t* CryptoBuffer::Assign(const nsTArray<uint8_t>& aData) {
  return ReplaceElementsAt(0, Length(), aData.Elements(), aData.Length(),
                           fallible);
}

uint8_t* CryptoBuffer::Assign(const ArrayBuffer& aData) {
  Clear();
  return aData.AppendDataTo(*this) ? Elements() : nullptr;
}

uint8_t* CryptoBuffer::Assign(const ArrayBufferView& aData) {
  Clear();
  return aData.AppendDataTo(*this) ? Elements() : nullptr;
}

uint8_t* CryptoBuffer::Assign(const ArrayBufferViewOrArrayBuffer& aData) {
  Clear();

  return AppendTypedArrayDataTo(aData, *this) ? Elements() : nullptr;
}

uint8_t* CryptoBuffer::Assign(const OwningArrayBufferViewOrArrayBuffer& aData) {
  Clear();

  return AppendTypedArrayDataTo(aData, *this) ? Elements() : nullptr;
}

uint8_t* CryptoBuffer::Assign(const Uint8Array& aArray) {
  Clear();
  return aArray.AppendDataTo(*this) ? Elements() : nullptr;
}

uint8_t* CryptoBuffer::AppendSECItem(const SECItem* aItem) {
  MOZ_ASSERT(aItem);
  return AppendElements(aItem->data, aItem->len, fallible);
}

uint8_t* CryptoBuffer::AppendSECItem(const SECItem& aItem) {
  return AppendElements(aItem.data, aItem.len, fallible);
}

// Helpers to encode/decode JWK's special flavor of Base64
// * No whitespace
// * No padding
// * URL-safe character set
nsresult CryptoBuffer::FromJwkBase64(const nsString& aBase64) {
  NS_ConvertUTF16toUTF8 temp(aBase64);
  temp.StripWhitespace();

  // JWK prohibits padding per RFC 7515, section 2.
  nsresult rv =
      Base64URLDecode(temp, Base64URLDecodePaddingPolicy::Reject, *this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult CryptoBuffer::ToJwkBase64(nsString& aBase64) const {
  // Shortcut for the empty octet string
  if (Length() == 0) {
    aBase64.Truncate();
    return NS_OK;
  }

  nsAutoCString base64;
  nsresult rv = Base64URLEncode(Length(), Elements(),
                                Base64URLEncodePaddingPolicy::Omit, base64);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyASCIItoUTF16(base64, aBase64);
  return NS_OK;
}

bool CryptoBuffer::ToSECItem(PLArenaPool* aArena, SECItem* aItem) const {
  aItem->type = siBuffer;
  aItem->data = nullptr;

  if (!::SECITEM_AllocItem(aArena, aItem, Length())) {
    return false;
  }
  // If this CryptoBuffer is of 0 length, aItem->data will be null. Passing
  // null to memcpy is not valid, even if the length is 0, so return early.
  if (!aItem->data) {
    MOZ_ASSERT(Length() == 0);
    return true;
  }
  memcpy(aItem->data, Elements(), Length());
  return true;
}

JSObject* CryptoBuffer::ToUint8Array(JSContext* aCx,
                                     ErrorResult& aError) const {
  return Uint8Array::Create(aCx, *this, aError);
}

JSObject* CryptoBuffer::ToArrayBuffer(JSContext* aCx,
                                      ErrorResult& aError) const {
  return ArrayBuffer::Create(aCx, *this, aError);
}

// "BigInt" comes from the WebCrypto spec
// ("unsigned long" isn't very "big", of course)
// Likewise, the spec calls for big-endian ints
bool CryptoBuffer::GetBigIntValue(unsigned long& aRetVal) {
  if (Length() > sizeof(aRetVal)) {
    return false;
  }

  aRetVal = 0;
  for (size_t i = 0; i < Length(); ++i) {
    aRetVal = (aRetVal << 8) + ElementAt(i);
  }
  return true;
}

}  // namespace mozilla::dom

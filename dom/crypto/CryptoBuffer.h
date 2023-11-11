/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CryptoBuffer_h
#define mozilla_dom_CryptoBuffer_h

#include "nsTArray.h"
#include "seccomon.h"
#include "mozilla/dom/TypedArray.h"

namespace mozilla::dom {

class ArrayBufferViewOrArrayBuffer;
class OwningArrayBufferViewOrArrayBuffer;

class CryptoBuffer : public FallibleTArray<uint8_t> {
 public:
  uint8_t* Assign(const CryptoBuffer& aData);
  uint8_t* Assign(const uint8_t* aData, uint32_t aLength);
  uint8_t* Assign(const nsACString& aString);
  uint8_t* Assign(const SECItem* aItem);
  uint8_t* Assign(const nsTArray<uint8_t>& aData);
  uint8_t* Assign(const ArrayBuffer& aData);
  uint8_t* Assign(const ArrayBufferView& aData);
  uint8_t* Assign(const ArrayBufferViewOrArrayBuffer& aData);
  uint8_t* Assign(const OwningArrayBufferViewOrArrayBuffer& aData);
  uint8_t* Assign(const Uint8Array& aArray);

  uint8_t* AppendSECItem(const SECItem* aItem);
  uint8_t* AppendSECItem(const SECItem& aItem);

  nsresult FromJwkBase64(const nsString& aBase64);
  nsresult ToJwkBase64(nsString& aBase64) const;
  bool ToSECItem(PLArenaPool* aArena, SECItem* aItem) const;
  JSObject* ToUint8Array(JSContext* aCx, ErrorResult& aError) const;
  JSObject* ToArrayBuffer(JSContext* aCx, ErrorResult& aError) const;

  bool GetBigIntValue(unsigned long& aRetVal);

  CryptoBuffer InfallibleClone() const {
    CryptoBuffer result;
    if (!result.Assign(*this)) {
      MOZ_CRASH("Out of memory");
    }
    return result;
  }
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CryptoBuffer_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/RsaHashedKeyAlgorithm.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/WebCryptoCommon.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RsaHashedKeyAlgorithm, RsaKeyAlgorithm, mHash)
NS_IMPL_ADDREF_INHERITED(RsaHashedKeyAlgorithm, RsaKeyAlgorithm)
NS_IMPL_RELEASE_INHERITED(RsaHashedKeyAlgorithm, RsaKeyAlgorithm)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RsaHashedKeyAlgorithm)
NS_INTERFACE_MAP_END_INHERITING(RsaKeyAlgorithm)

JSObject*
RsaHashedKeyAlgorithm::WrapObject(JSContext* aCx)
{
  return RsaHashedKeyAlgorithmBinding::Wrap(aCx, this);
}

bool
RsaHashedKeyAlgorithm::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  nsString hashName;
  mHash->GetName(hashName);
  return JS_WriteUint32Pair(aWriter, SCTAG_RSAHASHEDKEYALG, 0) &&
         JS_WriteUint32Pair(aWriter, mModulusLength, 0) &&
         WriteBuffer(aWriter, mPublicExponent) &&
         WriteString(aWriter, hashName) &&
         WriteString(aWriter, mName);
}

KeyAlgorithm*
RsaHashedKeyAlgorithm::Create(nsIGlobalObject* aGlobal, JSStructuredCloneReader* aReader) {
  uint32_t modulusLength, zero;
  CryptoBuffer publicExponent;
  nsString name, hash;

  bool read = JS_ReadUint32Pair(aReader, &modulusLength, &zero) &&
              ReadBuffer(aReader, publicExponent) &&
              ReadString(aReader, hash) &&
              ReadString(aReader, name);
  if (!read) {
    return nullptr;
  }

  return new RsaHashedKeyAlgorithm(aGlobal, name, modulusLength, publicExponent, hash);
}

} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HmacKeyAlgorithm_h
#define mozilla_dom_HmacKeyAlgorithm_h

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "mozilla/dom/KeyAlgorithm.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class HmacKeyAlgorithm MOZ_FINAL : public KeyAlgorithm
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HmacKeyAlgorithm, KeyAlgorithm)

  HmacKeyAlgorithm(nsIGlobalObject* aGlobal,
                   const nsString& aName,
                   uint32_t aLength,
                   const nsString& aHash)
    : KeyAlgorithm(aGlobal, aName)
    , mHash(new KeyAlgorithm(aGlobal, aHash))
    , mLength(aLength)
  {
    switch (mHash->Mechanism()) {
      case CKM_SHA_1: mMechanism = CKM_SHA_1_HMAC; break;
      case CKM_SHA256: mMechanism = CKM_SHA256_HMAC; break;
      case CKM_SHA384: mMechanism = CKM_SHA384_HMAC; break;
      case CKM_SHA512: mMechanism = CKM_SHA512_HMAC; break;
      default: mMechanism = UNKNOWN_CK_MECHANISM; break;
    }
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  KeyAlgorithm* Hash() const
  {
    return mHash;
  }

  uint32_t Length() const
  {
    return mLength;
  }

  virtual nsString ToJwkAlg() const MOZ_OVERRIDE;

  virtual bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const MOZ_OVERRIDE;
  static KeyAlgorithm* Create(nsIGlobalObject* aGlobal,
                              JSStructuredCloneReader* aReader);

protected:
  ~HmacKeyAlgorithm()
  {}

  nsRefPtr<KeyAlgorithm> mHash;
  uint32_t mLength;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HmacKeyAlgorithm_h

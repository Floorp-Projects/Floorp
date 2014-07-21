/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RsaHashedKeyAlgorithm_h
#define mozilla_dom_RsaHashedKeyAlgorithm_h

#include "nsAutoPtr.h"
#include "mozilla/dom/RsaKeyAlgorithm.h"

namespace mozilla {
namespace dom {

class RsaHashedKeyAlgorithm MOZ_FINAL : public RsaKeyAlgorithm
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RsaHashedKeyAlgorithm, RsaKeyAlgorithm)

  RsaHashedKeyAlgorithm(nsIGlobalObject* aGlobal,
                        const nsString& aName,
                        uint32_t aModulusLength,
                        const CryptoBuffer& aPublicExponent,
                        const nsString& aHashName)
    : RsaKeyAlgorithm(aGlobal, aName, aModulusLength, aPublicExponent)
    , mHash(new KeyAlgorithm(aGlobal, aHashName))
  {}

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual nsString ToJwkAlg() const MOZ_OVERRIDE;

  KeyAlgorithm* Hash() const
  {
    return mHash;
  }

  virtual bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const MOZ_OVERRIDE;
  static KeyAlgorithm* Create(nsIGlobalObject* aGlobal,
                              JSStructuredCloneReader* aReader);

private:
  ~RsaHashedKeyAlgorithm() {}

  nsRefPtr<KeyAlgorithm> mHash;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_RsaHashedKeyAlgorithm_h

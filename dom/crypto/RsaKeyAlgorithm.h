/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RsaKeyAlgorithm_h
#define mozilla_dom_RsaKeyAlgorithm_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/KeyAlgorithm.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class RsaKeyAlgorithm : public KeyAlgorithm
{
public:
  RsaKeyAlgorithm(nsIGlobalObject* aGlobal,
                  const nsString& aName,
                  uint32_t aModulusLength,
                  const CryptoBuffer& aPublicExponent)
    : KeyAlgorithm(aGlobal, aName)
    , mModulusLength(aModulusLength)
    , mPublicExponent(aPublicExponent)
  {}

  ~RsaKeyAlgorithm()
  {}

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  uint32_t ModulusLength() const
  {
    return mModulusLength;
  }

  void GetPublicExponent(JSContext* cx, JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aError) const
  {
    TypedArrayCreator<Uint8Array> creator(mPublicExponent);
    JSObject* retval = creator.Create(cx);
    if (!retval) {
      aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    } else {
      aRetval.set(retval);
    }
  }

  virtual bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const MOZ_OVERRIDE;
  static KeyAlgorithm* Create(nsIGlobalObject* aGlobal,
                              JSStructuredCloneReader* aReader);

protected:
  uint32_t mModulusLength;
  CryptoBuffer mPublicExponent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_RsaKeyAlgorithm_h

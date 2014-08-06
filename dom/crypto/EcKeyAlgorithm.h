/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EcKeyAlgorithm_h
#define mozilla_dom_EcKeyAlgorithm_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/KeyAlgorithm.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class EcKeyAlgorithm : public KeyAlgorithm
{
public:
  EcKeyAlgorithm(nsIGlobalObject* aGlobal,
                 const nsString& aName,
                 const nsString& aNamedCurve)
    : KeyAlgorithm(aGlobal, aName)
    , mNamedCurve(aNamedCurve)
  {}

  ~EcKeyAlgorithm()
  {}

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void GetNamedCurve(nsString& aRetVal) const
  {
    aRetVal.Assign(mNamedCurve);
  }

  virtual bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const MOZ_OVERRIDE;
  static KeyAlgorithm* Create(nsIGlobalObject* aGlobal,
                              JSStructuredCloneReader* aReader);

protected:
  nsString mNamedCurve;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_EcKeyAlgorithm_h

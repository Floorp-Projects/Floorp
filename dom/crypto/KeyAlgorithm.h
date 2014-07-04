/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyAlgorithm_h
#define mozilla_dom_KeyAlgorithm_h

#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsWrapperCache.h"
#include "pk11pub.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "js/StructuredClone.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class Key;
class KeyAlgorithm;

enum KeyAlgorithmStructuredCloneTags {
  SCTAG_KEYALG,
  SCTAG_AESKEYALG,
  SCTAG_HMACKEYALG,
  SCTAG_RSAKEYALG,
  SCTAG_RSAHASHEDKEYALG
};

}

template<>
struct HasDangerousPublicDestructor<dom::KeyAlgorithm>
{
  static const bool value = true;
};

namespace dom {

class KeyAlgorithm : public nsISupports,
                     public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(KeyAlgorithm)

public:
  KeyAlgorithm(nsIGlobalObject* aGlobal, const nsString& aName);

  virtual ~KeyAlgorithm();

  nsIGlobalObject* GetParentObject() const
  {
    return mGlobal;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void GetName(nsString& aRetVal) const;

  // Structured clone support methods
  virtual bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const;
  static KeyAlgorithm* Create(nsIGlobalObject* aGlobal,
                              JSStructuredCloneReader* aReader);

  // Helper method to look up NSS methods
  // Sub-classes should assign mMechanism on constructor
  CK_MECHANISM_TYPE Mechanism() const {
    return mMechanism;
  }

protected:
  nsRefPtr<nsIGlobalObject> mGlobal;
  nsString mName;
  CK_MECHANISM_TYPE mMechanism;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_KeyAlgorithm_h

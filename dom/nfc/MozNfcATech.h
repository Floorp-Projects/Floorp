/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nfc_MozNfcATech_h__
#define mozilla_dom_nfc_MozNfcATech_h__

#include "mozilla/dom/MozNFCTagBinding.h"
#include "mozilla/dom/MozNfcATechBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsISupportsImpl.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

class Promise;

class MozNfcATech : public nsISupports,
                    public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MozNfcATech)

  already_AddRefed<Promise> Transceive(const Uint8Array& aCommand,
                                       ErrorResult& aRv);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<MozNfcATech>
  Constructor(const GlobalObject& aGlobal, MozNFCTag& aNFCTag,
              ErrorResult& aRv);

private:
  MozNfcATech(nsPIDOMWindowInner* aWindow, MozNFCTag& aNFCTag);
  virtual ~MozNfcATech();

  RefPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<MozNFCTag> mTag;

  static const NFCTechType sTechnology;
};

} // namespace dom
} // namespace mozilla

#endif  // mozilla_dom_nfc_MozNfcATech_h__

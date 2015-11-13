/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozNfcATech.h"
#include "TagUtils.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla::dom::nfc;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MozNfcATech)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(MozNfcATech)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MozNfcATech)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTag)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MozNfcATech)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTag)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MozNfcATech)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MozNfcATech)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MozNfcATech)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

const NFCTechType MozNfcATech::sTechnology = NFCTechType::NFC_A;

/* static */
already_AddRefed<MozNfcATech>
MozNfcATech::Constructor(const GlobalObject& aGlobal,
                         MozNFCTag& aNFCTag,
                         ErrorResult& aRv)
{
  ErrorResult rv;
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!TagUtils::IsTechSupported(aNFCTag, sTechnology)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  RefPtr<MozNfcATech> nfcA = new MozNfcATech(win, aNFCTag);
  return nfcA.forget();
}

MozNfcATech::MozNfcATech(nsPIDOMWindow* aWindow, MozNFCTag& aNFCTag)
 : mWindow(aWindow)
 , mTag(&aNFCTag)
{
}

MozNfcATech::~MozNfcATech()
{
}

already_AddRefed<Promise>
MozNfcATech::Transceive(const Uint8Array& aCommand, ErrorResult& aRv)
{
  return TagUtils::Transceive(mTag, sTechnology, aCommand, aRv);
}

JSObject*
MozNfcATech::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozNfcATechBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

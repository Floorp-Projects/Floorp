/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozIsoDepTech.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MozIsoDepTech)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(MozIsoDepTech)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MozIsoDepTech)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTag)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MozIsoDepTech)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTag)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MozIsoDepTech)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MozIsoDepTech)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MozIsoDepTech)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

const NFCTechType MozIsoDepTech::mTechnology = NFCTechType::ISO_DEP;

/* static */
already_AddRefed<MozIsoDepTech>
MozIsoDepTech::Constructor(const GlobalObject& aGlobal,
                           MozNFCTag& aNFCTag,
                           ErrorResult& aRv)
{
  ErrorResult rv;
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  Nullable<nsTArray<NFCTechType>> techList;
  aNFCTag.GetTechList(techList, rv);
  if (rv.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (techList.IsNull() || !(techList.Value().Contains(mTechnology))) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  nsRefPtr<MozIsoDepTech> isoDep = new MozIsoDepTech(win, aNFCTag);

  return isoDep.forget();
}

MozIsoDepTech::MozIsoDepTech(nsPIDOMWindow* aWindow, MozNFCTag& aNFCTag)
 : mWindow(aWindow)
 , mTag(&aNFCTag)
{
}

MozIsoDepTech::~MozIsoDepTech()
{
}

already_AddRefed<Promise>
MozIsoDepTech::Transceive(const Uint8Array& aCommand, ErrorResult& aRv)
{
  ErrorResult rv;

  aCommand.ComputeLengthAndData();
  nsRefPtr<Promise> promise = mTag->Transceive(mTechnology, aCommand, rv);
  if (rv.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return promise.forget();
}

JSObject*
MozIsoDepTech::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozIsoDepTechBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Storage.h"

#include "mozilla/dom/StorageBinding.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Storage, mWindow, mPrincipal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Storage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Storage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Storage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
NS_INTERFACE_MAP_END

Storage::Storage(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal)
  : mWindow(aWindow)
  , mPrincipal(aPrincipal)
{
  MOZ_ASSERT(aPrincipal);
}

Storage::~Storage()
{}

bool
Storage::CanAccess(nsIPrincipal* aPrincipal)
{
  return !aPrincipal || aPrincipal->Subsumes(mPrincipal);
}

/* virtual */ JSObject*
Storage::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return StorageBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

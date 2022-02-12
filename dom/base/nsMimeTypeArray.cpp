/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMimeTypeArray.h"

#include "mozilla/dom/MimeTypeArrayBinding.h"
#include "mozilla/dom/MimeTypeBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsMimeTypeArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsMimeTypeArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsMimeTypeArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsMimeTypeArray, mWindow)

nsMimeTypeArray::nsMimeTypeArray(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow) {}

nsMimeTypeArray::~nsMimeTypeArray() = default;

JSObject* nsMimeTypeArray::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return MimeTypeArray_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* nsMimeTypeArray::GetParentObject() const {
  MOZ_ASSERT(mWindow);
  return mWindow;
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsMimeType, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsMimeType, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsMimeType)

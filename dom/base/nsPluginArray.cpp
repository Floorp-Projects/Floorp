/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginArray.h"

#include "mozilla/dom/PluginArrayBinding.h"
#include "mozilla/dom/PluginBinding.h"

#include "nsPIDOMWindow.h"

using namespace mozilla;
using namespace mozilla::dom;

nsPluginArray::nsPluginArray(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {}

nsPluginArray::~nsPluginArray() = default;

nsPIDOMWindowInner* nsPluginArray::GetParentObject() const {
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject* nsPluginArray::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return PluginArray_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPluginArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPluginArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPluginArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK(nsPluginArray, mWindow)

// nsPluginElement implementation.

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPluginElement)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPluginElement)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPluginElement)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsPluginElement)

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRInputSourceArray.h"
#include "mozilla/dom/XRSpace.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRInputSourceArray, mParent,
                                      mInputSources)
NS_IMPL_CYCLE_COLLECTING_ADDREF(XRInputSourceArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XRInputSourceArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XRInputSourceArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

XRInputSourceArray::XRInputSourceArray(nsISupports* aParent)
    : mParent(aParent) {}

JSObject* XRInputSourceArray::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return XRInputSourceArray_Binding::Wrap(aCx, this, aGivenProto);
}

uint32_t XRInputSourceArray::Length() { return mInputSources.Length(); }

XRInputSource* XRInputSourceArray::IndexedGetter(uint32_t aIndex,
                                                 bool& aFound) {
  aFound = aIndex < mInputSources.Length();
  if (!aFound) {
    return nullptr;
  }
  return mInputSources[aIndex];
}

}  // namespace dom
}  // namespace mozilla

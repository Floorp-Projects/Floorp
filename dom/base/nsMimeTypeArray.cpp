/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMimeTypeArray.h"

#include "mozilla/dom/MimeTypeArrayBinding.h"
#include "mozilla/dom/MimeTypeBinding.h"
#include "nsPluginArray.h"
#include "mozilla/StaticPrefs_pdfjs.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsMimeTypeArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsMimeTypeArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsMimeTypeArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsMimeTypeArray, mWindow, mMimeTypes[0],
                                      mMimeTypes[1])

nsMimeTypeArray::nsMimeTypeArray(
    nsPIDOMWindowInner* aWindow,
    const mozilla::Array<RefPtr<nsMimeType>, 2>& aMimeTypes)
    : mWindow(aWindow), mMimeTypes(aMimeTypes) {}

nsMimeTypeArray::~nsMimeTypeArray() = default;

JSObject* nsMimeTypeArray::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return MimeTypeArray_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* nsMimeTypeArray::GetParentObject() const {
  MOZ_ASSERT(mWindow);
  return mWindow;
}

nsMimeType* nsMimeTypeArray::IndexedGetter(uint32_t aIndex, bool& aFound) {
  if (!ForceNoPlugins() && aIndex < ArrayLength(mMimeTypes)) {
    aFound = true;
    return mMimeTypes[aIndex];
  }

  aFound = false;
  return nullptr;
}

nsMimeType* nsMimeTypeArray::NamedGetter(const nsAString& aName, bool& aFound) {
  if (ForceNoPlugins()) {
    aFound = false;
    return nullptr;
  }

  for (const auto& mimeType : mMimeTypes) {
    if (mimeType->Name().Equals(aName)) {
      aFound = true;
      return mimeType;
    }
  }

  aFound = false;
  return nullptr;
}

void nsMimeTypeArray::GetSupportedNames(nsTArray<nsString>& retval) {
  if (ForceNoPlugins()) {
    return;
  }

  for (auto& mimeType : mMimeTypes) {
    retval.AppendElement(mimeType->Name());
  }
}

bool nsMimeTypeArray::ForceNoPlugins() { return StaticPrefs::pdfjs_disabled(); }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsMimeType, mPluginElement)

nsMimeType::nsMimeType(nsPluginElement* aPluginElement, const nsAString& aName)
    : mPluginElement(aPluginElement), mName(aName) {
  MOZ_ASSERT(aPluginElement);
}

nsMimeType::~nsMimeType() = default;

JSObject* nsMimeType::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return MimeType_Binding::Wrap(aCx, this, aGivenProto);
}

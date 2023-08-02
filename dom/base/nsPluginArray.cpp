/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginArray.h"

#include "mozilla/dom/PluginArrayBinding.h"
#include "mozilla/dom/PluginBinding.h"
#include "mozilla/StaticPrefs_pdfjs.h"

#include "nsMimeTypeArray.h"
#include "nsPIDOMWindow.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

// These plugin and mime types are hard-coded by the HTML spec.
// The "main" plugin name is used with the only plugin that is
// referenced by MIME types (via nsMimeType::GetEnabledPlugin).
// The "extra" of the plugin names are associated with MIME types that
// reference the main plugin.
// This is all defined in the HTML spec, section 8.9.1.6
// "PDF Viewing Support".
static const nsLiteralString kMainPluginName = u"PDF Viewer"_ns;
static const nsLiteralString kExtraPluginNames[] = {
    u"Chrome PDF Viewer"_ns, u"Chromium PDF Viewer"_ns,
    u"Microsoft Edge PDF Viewer"_ns, u"WebKit built-in PDF"_ns};
static const nsLiteralString kMimeTypeNames[] = {u"application/pdf"_ns,
                                                 u"text/pdf"_ns};

nsPluginArray::nsPluginArray(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {
  // Create the hard-coded PDF plugin types that share MIME type arrays.
  mPlugins[0] = MakeRefPtr<nsPluginElement>(this, aWindow, kMainPluginName);

  mozilla::Array<RefPtr<nsMimeType>, 2> mimeTypes;
  for (uint32_t i = 0; i < ArrayLength(kMimeTypeNames); ++i) {
    mimeTypes[i] = MakeRefPtr<nsMimeType>(mPlugins[0], kMimeTypeNames[i]);
  }
  mMimeTypeArray = MakeRefPtr<nsMimeTypeArray>(aWindow, mimeTypes);

  for (uint32_t i = 0; i < ArrayLength(kExtraPluginNames); ++i) {
    mPlugins[i + 1] =
        MakeRefPtr<nsPluginElement>(this, aWindow, kExtraPluginNames[i]);
  }
}

nsPluginArray::~nsPluginArray() = default;

nsPIDOMWindowInner* nsPluginArray::GetParentObject() const {
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject* nsPluginArray::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return PluginArray_Binding::Wrap(aCx, this, aGivenProto);
}

nsPluginElement* nsPluginArray::IndexedGetter(uint32_t aIndex, bool& aFound) {
  if (!ForceNoPlugins() && aIndex < ArrayLength(mPlugins)) {
    aFound = true;
    return mPlugins[aIndex];
  }

  aFound = false;
  return nullptr;
}

nsPluginElement* nsPluginArray::NamedGetter(const nsAString& aName,
                                            bool& aFound) {
  if (ForceNoPlugins()) {
    aFound = false;
    return nullptr;
  }

  for (const auto& plugin : mPlugins) {
    if (plugin->Name().Equals(aName)) {
      aFound = true;
      return plugin;
    }
  }

  aFound = false;
  return nullptr;
}

void nsPluginArray::GetSupportedNames(nsTArray<nsString>& aRetval) {
  if (ForceNoPlugins()) {
    return;
  }

  for (auto& plugin : mPlugins) {
    aRetval.AppendElement(plugin->Name());
  }
}

bool nsPluginArray::ForceNoPlugins() { return StaticPrefs::pdfjs_disabled(); }

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPluginArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPluginArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPluginArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK(nsPluginArray, mPlugins[0],
                                           mPlugins[1], mPlugins[2],
                                           mPlugins[3], mPlugins[4],
                                           mMimeTypeArray, mWindow)

// nsPluginElement implementation.

nsPluginElement::nsPluginElement(nsPluginArray* aPluginArray,
                                 nsPIDOMWindowInner* aWindow,
                                 const nsAString& aName)
    : mPluginArray(aPluginArray), mWindow(aWindow), mName(aName) {}

nsPluginArray* nsPluginElement::GetParentObject() const { return mPluginArray; }

JSObject* nsPluginElement::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return Plugin_Binding::Wrap(aCx, this, aGivenProto);
}

nsMimeType* nsPluginElement::IndexedGetter(uint32_t aIndex, bool& aFound) {
  return MimeTypeArray()->IndexedGetter(aIndex, aFound);
}

nsMimeType* nsPluginElement::NamedGetter(const nsAString& aName, bool& aFound) {
  return MimeTypeArray()->NamedGetter(aName, aFound);
}

void nsPluginElement::GetSupportedNames(nsTArray<nsString>& retval) {
  return MimeTypeArray()->GetSupportedNames(retval);
}

uint32_t nsPluginElement::Length() { return MimeTypeArray()->Length(); }

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPluginElement)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPluginElement)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPluginElement)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsPluginElement, mWindow, mPluginArray)

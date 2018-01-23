/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMimeTypeArray.h"

#include "mozilla/dom/MimeTypeArrayBinding.h"
#include "mozilla/dom/MimeTypeBinding.h"
#include "nsPIDOMWindow.h"
#include "nsPluginArray.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"
#include "mozilla/dom/Navigator.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsPluginTags.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsMimeTypeArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsMimeTypeArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsMimeTypeArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsMimeTypeArray,
                                      mWindow,
                                      mMimeTypes,
                                      mCTPMimeTypes)

nsMimeTypeArray::nsMimeTypeArray(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow)
{
}

nsMimeTypeArray::~nsMimeTypeArray()
{
}

JSObject*
nsMimeTypeArray::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MimeTypeArrayBinding::Wrap(aCx, this, aGivenProto);
}

void
nsMimeTypeArray::Refresh()
{
  mMimeTypes.Clear();
  mCTPMimeTypes.Clear();
}

nsPIDOMWindowInner*
nsMimeTypeArray::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

nsMimeType*
nsMimeTypeArray::Item(uint32_t aIndex, CallerType aCallerType)
{
  bool unused;
  return IndexedGetter(aIndex, unused, aCallerType);
}

nsMimeType*
nsMimeTypeArray::NamedItem(const nsAString& aName, CallerType aCallerType)
{
  bool unused;
  return NamedGetter(aName, unused, aCallerType);
}

nsMimeType*
nsMimeTypeArray::IndexedGetter(uint32_t aIndex, bool &aFound,
                               CallerType aCallerType)
{
  aFound = false;

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return nullptr;
  }

  EnsurePluginMimeTypes();

  if (aIndex >= mMimeTypes.Length()) {
    return nullptr;
  }

  aFound = true;

  return mMimeTypes[aIndex];
}

static nsMimeType*
FindMimeType(const nsTArray<RefPtr<nsMimeType>>& aMimeTypes,
             const nsAString& aType)
{
  for (uint32_t i = 0; i < aMimeTypes.Length(); ++i) {
    nsMimeType* mimeType = aMimeTypes[i];
    if (aType.Equals(mimeType->Type())) {
      return mimeType;
    }
  }

  return nullptr;
}

nsMimeType*
nsMimeTypeArray::NamedGetter(const nsAString& aName, bool &aFound,
                             CallerType aCallerType)
{
  aFound = false;

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return nullptr;
  }

  EnsurePluginMimeTypes();

  nsString lowerName(aName);
  ToLowerCase(lowerName);

  nsMimeType* mimeType = FindMimeType(mMimeTypes, lowerName);
  if (mimeType) {
    aFound = true;
    return mimeType;
  }
  nsMimeType* hiddenType = FindMimeType(mCTPMimeTypes, lowerName);
  if (hiddenType) {
    nsPluginArray::NotifyHiddenPluginTouched(hiddenType->GetEnabledPlugin());
  }

  return nullptr;
}

uint32_t
nsMimeTypeArray::Length(CallerType aCallerType)
{
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return 0;
  }

  EnsurePluginMimeTypes();

  return mMimeTypes.Length();
}

void
nsMimeTypeArray::GetSupportedNames(nsTArray<nsString>& aRetval,
                                   CallerType aCallerType)
{
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return;
  }

  EnsurePluginMimeTypes();

  for (uint32_t i = 0; i < mMimeTypes.Length(); ++i) {
    aRetval.AppendElement(mMimeTypes[i]->Type());
  }
}

void
nsMimeTypeArray::EnsurePluginMimeTypes()
{
  if (!mMimeTypes.IsEmpty() || !mWindow) {
    return;
  }

  RefPtr<Navigator> navigator = mWindow->Navigator();

  IgnoredErrorResult rv;
  nsPluginArray *pluginArray = navigator->GetPlugins(rv);
  if (!pluginArray) {
    return;
  }

  pluginArray->GetMimeTypes(mMimeTypes);
  pluginArray->GetCTPMimeTypes(mCTPMimeTypes);
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsMimeType, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsMimeType, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsMimeType, mWindow, mPluginElement)

nsMimeType::nsMimeType(nsPIDOMWindowInner* aWindow,
                       nsPluginElement* aPluginElement,
                       const nsAString& aType,
                       const nsAString& aDescription,
                       const nsAString& aExtension)
  : mWindow(aWindow),
    mPluginElement(aPluginElement),
    mType(aType),
    mDescription(aDescription),
    mExtension(aExtension)
{
  MOZ_ASSERT(aPluginElement);
}

nsMimeType::~nsMimeType()
{
}

nsPIDOMWindowInner*
nsMimeType::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject*
nsMimeType::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MimeTypeBinding::Wrap(aCx, this, aGivenProto);
}

void
nsMimeType::GetDescription(nsString& aRetval) const
{
  aRetval = mDescription;
}

nsPluginElement*
nsMimeType::GetEnabledPlugin() const
{
  // mPluginElement might be null if we got unlinked but are still somehow being
  // called into.
  if (!mPluginElement || !mPluginElement->PluginTag()->IsEnabled()) {
    return nullptr;
  }
  return mPluginElement;
}

void
nsMimeType::GetSuffixes(nsString& aRetval) const
{
  aRetval = mExtension;
}

void
nsMimeType::GetType(nsString& aRetval) const
{
  aRetval = mType;
}

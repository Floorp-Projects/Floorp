/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMimeTypeArray.h"

#include "mozilla/dom/MimeTypeArrayBinding.h"
#include "mozilla/dom/MimeTypeBinding.h"
#include "nsIDOMNavigator.h"
#include "nsPIDOMWindow.h"
#include "nsPluginArray.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"
#include "Navigator.h"
#include "nsServiceManagerUtils.h"
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
                                      mMimeTypes)

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
}

nsPIDOMWindowInner*
nsMimeTypeArray::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

nsMimeType*
nsMimeTypeArray::Item(uint32_t aIndex)
{
  bool unused;
  return IndexedGetter(aIndex, unused);
}

nsMimeType*
nsMimeTypeArray::NamedItem(const nsAString& aName)
{
  bool unused;
  return NamedGetter(aName, unused);
}

nsMimeType*
nsMimeTypeArray::IndexedGetter(uint32_t aIndex, bool &aFound)
{
  aFound = false;

  EnsurePluginMimeTypes();

  if (aIndex >= mMimeTypes.Length()) {
    return nullptr;
  }

  aFound = true;

  return mMimeTypes[aIndex];
}

static nsMimeType*
FindMimeType(const nsTArray<RefPtr<nsMimeType> >& aMimeTypes,
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
nsMimeTypeArray::NamedGetter(const nsAString& aName, bool &aFound)
{
  aFound = false;

  EnsurePluginMimeTypes();

  nsString lowerName(aName);
  ToLowerCase(lowerName);

  nsMimeType* mimeType = FindMimeType(mMimeTypes, lowerName);
  if (mimeType) {
    aFound = true;
    return mimeType;
  }

  // Now let's check with the MIME service.
  nsCOMPtr<nsIMIMEService> mimeSrv = do_GetService("@mozilla.org/mime;1");
  if (!mimeSrv) {
    return nullptr;
  }

  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  mimeSrv->GetFromTypeAndExtension(NS_ConvertUTF16toUTF8(lowerName),
                                   EmptyCString(), getter_AddRefs(mimeInfo));
  if (!mimeInfo) {
    return nullptr;
  }

  // Now we check whether we can really claim to support this type
  nsHandlerInfoAction action = nsIHandlerInfo::saveToDisk;
  mimeInfo->GetPreferredAction(&action);
  if (action != nsIMIMEInfo::handleInternally) {
    bool hasHelper = false;
    mimeInfo->GetHasDefaultHandler(&hasHelper);

    if (!hasHelper) {
      nsCOMPtr<nsIHandlerApp> helper;
      mimeInfo->GetPreferredApplicationHandler(getter_AddRefs(helper));

      if (!helper) {
        // mime info from the OS may not have a PreferredApplicationHandler
        // so just check for an empty default description
        nsAutoString defaultDescription;
        mimeInfo->GetDefaultDescription(defaultDescription);

        if (defaultDescription.IsEmpty()) {
          // no support; just leave
          return nullptr;
        }
      }
    }
  }

  // If we got here, we support this type!  Say so.
  aFound = true;

  nsMimeType *mt = new nsMimeType(mWindow, lowerName);
  mMimeTypes.AppendElement(mt);
  return mt;
}

bool
nsMimeTypeArray::NameIsEnumerable(const nsAString& aName)
{
  return true;
}

uint32_t
nsMimeTypeArray::Length()
{
  EnsurePluginMimeTypes();

  return mMimeTypes.Length();
}

void
nsMimeTypeArray::GetSupportedNames(unsigned, nsTArray< nsString >& aRetval)
{
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

  nsCOMPtr<nsIDOMNavigator> navigator = mWindow->GetNavigator();

  if (!navigator) {
    return;
  }

  ErrorResult rv;
  nsPluginArray *pluginArray =
    static_cast<Navigator*>(navigator.get())->GetPlugins(rv);
  if (!pluginArray) {
    return;
  }

  pluginArray->GetMimeTypes(mMimeTypes);
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
}

nsMimeType::nsMimeType(nsPIDOMWindowInner* aWindow, const nsAString& aType)
  : mWindow(aWindow),
    mPluginElement(nullptr),
    mType(aType)
{
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

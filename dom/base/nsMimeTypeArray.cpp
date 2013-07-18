/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMimeTypeArray.h"
#include "mozilla/dom/MimeTypeArrayBinding.h"
#include "mozilla/dom/MimeTypeBinding.h"
#include "nsIDOMNavigator.h"
#include "nsContentUtils.h"
#include "nsPluginArray.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"
#include "Navigator.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsMimeTypeArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsMimeTypeArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsMimeTypeArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsMimeTypeArray,
                                        mMimeTypes)

nsMimeTypeArray::nsMimeTypeArray(nsWeakPtr aWindow)
  : mWindow(aWindow),
    mPluginMimeTypeCount(0)
{
  SetIsDOMBinding();
}

nsMimeTypeArray::~nsMimeTypeArray()
{
}

JSObject*
nsMimeTypeArray::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MimeTypeArrayBinding::Wrap(aCx, aScope, this);
}

void
nsMimeTypeArray::Refresh()
{
  mMimeTypes.Clear();
  mPluginMimeTypeCount = 0;
}

nsPIDOMWindow *
nsMimeTypeArray::GetParentObject() const
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));
  MOZ_ASSERT(win);
  return win;
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

  if (mMimeTypes.IsEmpty()) {
    EnsureMimeTypes();
  }

  MOZ_ASSERT(mMimeTypes.Length() >= mPluginMimeTypeCount);

  if (aIndex >= mPluginMimeTypeCount) {
    return nullptr;
  }

  aFound = true;

  return mMimeTypes[aIndex];
}

nsMimeType*
nsMimeTypeArray::NamedGetter(const nsAString& aName, bool &aFound)
{
  aFound = false;

  if (mMimeTypes.IsEmpty()) {
    EnsureMimeTypes();
  }

  for (uint32_t i = 0; i < mMimeTypes.Length(); ++i) {
    if (aName.Equals(mMimeTypes[i]->Type())) {
      aFound = true;

      return mMimeTypes[i];
    }
  }

  // Now let's check with the MIME service.
  nsCOMPtr<nsIMIMEService> mimeSrv = do_GetService("@mozilla.org/mime;1");
  if (!mimeSrv) {
    return nullptr;
  }

  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  mimeSrv->GetFromTypeAndExtension(NS_ConvertUTF16toUTF8(aName), EmptyCString(),
                                   getter_AddRefs(mimeInfo));
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

  nsMimeType *mt = new nsMimeType(mWindow, aName);
  mMimeTypes.AppendElement(mt);

  return mt;
}

uint32_t
nsMimeTypeArray::Length()
{
  if (mMimeTypes.IsEmpty()) {
    EnsureMimeTypes();
  }

  MOZ_ASSERT(mMimeTypes.Length() >= mPluginMimeTypeCount);

  return mPluginMimeTypeCount;
}

void
nsMimeTypeArray::GetSupportedNames(nsTArray< nsString >& aRetval)
{
  if (mMimeTypes.IsEmpty()) {
    EnsureMimeTypes();
  }

  for (uint32_t i = 0; i < mMimeTypes.Length(); ++i) {
    aRetval.AppendElement(mMimeTypes[i]->Type());
  }
}

void
nsMimeTypeArray::EnsureMimeTypes()
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));

  if (!mMimeTypes.IsEmpty() || !win) {
    return;
  }

  nsCOMPtr<nsIDOMNavigator> navigator;
  win->GetNavigator(getter_AddRefs(navigator));

  if (!navigator) {
    return;
  }

  ErrorResult rv;
  nsPluginArray *pluginArray =
    static_cast<Navigator*>(navigator.get())->GetPlugins(rv);
  if (!pluginArray) {
    return;
  }

  nsTArray<nsRefPtr<nsPluginElement> > plugins;
  pluginArray->GetPlugins(plugins);

  for (uint32_t i = 0; i < plugins.Length(); ++i) {
    nsPluginElement *plugin = plugins[i];

    mMimeTypes.AppendElements(plugin->MimeTypes());
  }

  mPluginMimeTypeCount = mMimeTypes.Length();
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsMimeType, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsMimeType, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsMimeType)

nsMimeType::nsMimeType(nsWeakPtr aWindow, nsPluginElement* aPluginElement,
                       uint32_t aPluginTagMimeIndex, const nsAString& aType)
  : mWindow(aWindow),
    mPluginElement(aPluginElement),
    mPluginTagMimeIndex(aPluginTagMimeIndex),
    mType(aType)
{
  SetIsDOMBinding();
}

nsMimeType::nsMimeType(nsWeakPtr aWindow, const nsAString& aType)
  : mWindow(aWindow),
    mPluginElement(nullptr),
    mPluginTagMimeIndex(0),
    mType(aType)
{
  SetIsDOMBinding();
}

nsMimeType::~nsMimeType()
{
}

nsPIDOMWindow *
nsMimeType::GetParentObject() const
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));
  MOZ_ASSERT(win);
  return win;
}

JSObject*
nsMimeType::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MimeTypeBinding::Wrap(aCx, aScope, this);
}

void
nsMimeType::GetDescription(nsString& retval) const
{
  retval.Truncate();

  if (mPluginElement) {
    CopyUTF8toUTF16(mPluginElement->PluginTag()->
                    mMimeDescriptions[mPluginTagMimeIndex], retval);
  }
}

nsPluginElement*
nsMimeType::GetEnabledPlugin() const
{
  return (mPluginElement && mPluginElement->PluginTag()->IsEnabled()) ?
    mPluginElement : nullptr;
}

void
nsMimeType::GetSuffixes(nsString& retval) const
{
  retval.Truncate();

  if (mPluginElement) {
    CopyUTF8toUTF16(mPluginElement->PluginTag()->
                    mExtensions[mPluginTagMimeIndex], retval);
  }
}

void
nsMimeType::GetType(nsString& aRetval) const
{
  aRetval = mType;
}

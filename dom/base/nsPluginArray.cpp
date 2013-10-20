/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginArray.h"

#include "mozilla/dom/PluginArrayBinding.h"
#include "mozilla/dom/PluginBinding.h"
#include "nsMimeTypeArray.h"
#include "Navigator.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsPluginHost.h"
#include "nsPluginTags.h"
#include "nsIObserverService.h"
#include "nsIWeakReference.h"
#include "mozilla/Services.h"
#include "nsIInterfaceRequestorUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

nsPluginArray::nsPluginArray(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
  SetIsDOMBinding();
}

void
nsPluginArray::Init()
{
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService) {
    obsService->AddObserver(this, "plugin-info-updated", true);
  }
}

nsPluginArray::~nsPluginArray()
{
}

nsPIDOMWindow*
nsPluginArray::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject*
nsPluginArray::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return PluginArrayBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPluginArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPluginArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPluginArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(nsPluginArray,
                                        mWindow,
                                        mPlugins)

void
nsPluginArray::GetMimeTypes(nsTArray<nsRefPtr<nsMimeType> >& aMimeTypes)
{
  aMimeTypes.Clear();

  if (!AllowPlugins()) {
    return;
  }

  EnsurePlugins();

  for (uint32_t i = 0; i < mPlugins.Length(); ++i) {
    nsPluginElement *plugin = mPlugins[i];
    aMimeTypes.AppendElements(plugin->MimeTypes());
  }
}

nsPluginElement*
nsPluginArray::Item(uint32_t aIndex)
{
  bool unused;
  return IndexedGetter(aIndex, unused);
}

nsPluginElement*
nsPluginArray::NamedItem(const nsAString& aName)
{
  bool unused;
  return NamedGetter(aName, unused);
}

void
nsPluginArray::Refresh(bool aReloadDocuments)
{
  nsRefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();

  if(!AllowPlugins() || !pluginHost) {
    return;
  }

  // NS_ERROR_PLUGINS_PLUGINSNOTCHANGED on reloading plugins indicates
  // that plugins did not change and was not reloaded
  if (pluginHost->ReloadPlugins() ==
      NS_ERROR_PLUGINS_PLUGINSNOTCHANGED) {
    nsTArray<nsRefPtr<nsPluginTag> > newPluginTags;
    pluginHost->GetPlugins(newPluginTags);

    // Check if the number of plugins we know about are different from
    // the number of plugin tags the plugin host knows about. If the
    // lengths are different, we refresh. This is safe because we're
    // notified for every plugin enabling/disabling event that
    // happens, and therefore the lengths will be in sync only when
    // the both arrays contain the same plugin tags (though as
    // different types).
    if (newPluginTags.Length() == mPlugins.Length()) {
      return;
    }
  }

  mPlugins.Clear();

  nsCOMPtr<nsIDOMNavigator> navigator;
  mWindow->GetNavigator(getter_AddRefs(navigator));

  if (!navigator) {
    return;
  }

  static_cast<mozilla::dom::Navigator*>(navigator.get())->RefreshMIMEArray();

  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(mWindow);
  if (aReloadDocuments && webNav) {
    webNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
  }
}

nsPluginElement*
nsPluginArray::IndexedGetter(uint32_t aIndex, bool &aFound)
{
  aFound = false;

  if (!AllowPlugins()) {
    return nullptr;
  }

  EnsurePlugins();

  aFound = aIndex < mPlugins.Length();

  return aFound ? mPlugins[aIndex] : nullptr;
}

void
nsPluginArray::Invalidate()
{
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService) {
    obsService->RemoveObserver(this, "plugin-info-updated");
  }
}

nsPluginElement*
nsPluginArray::NamedGetter(const nsAString& aName, bool &aFound)
{
  aFound = false;

  if (!AllowPlugins()) {
    return nullptr;
  }

  EnsurePlugins();

  for (uint32_t i = 0; i < mPlugins.Length(); ++i) {
    nsAutoString pluginName;
    nsPluginElement* plugin = mPlugins[i];
    plugin->GetName(pluginName);

    if (pluginName.Equals(aName)) {
      aFound = true;

      return plugin;
    }
  }

  return nullptr;
}

uint32_t
nsPluginArray::Length()
{
  if (!AllowPlugins()) {
    return 0;
  }

  EnsurePlugins();

  return mPlugins.Length();
}

void
nsPluginArray::GetSupportedNames(nsTArray< nsString >& aRetval)
{
  aRetval.Clear();

  if (!AllowPlugins()) {
    return;
  }

  for (uint32_t i = 0; i < mPlugins.Length(); ++i) {
    nsAutoString pluginName;
    mPlugins[i]->GetName(pluginName);

    aRetval.AppendElement(pluginName);
  }
}

NS_IMETHODIMP
nsPluginArray::Observe(nsISupports *aSubject, const char *aTopic,
                       const PRUnichar *aData) {
  if (!nsCRT::strcmp(aTopic, "plugin-info-updated")) {
    Refresh(false);
  }

  return NS_OK;
}

bool
nsPluginArray::AllowPlugins() const
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWindow);

  return docShell && docShell->PluginsAllowedInCurrentDoc();
}

void
nsPluginArray::EnsurePlugins()
{
  if (!mPlugins.IsEmpty()) {
    // We already have an array of plugin elements.
    return;
  }

  nsRefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  if (!pluginHost) {
    // We have no plugin host.
    return;
  }

  nsTArray<nsRefPtr<nsPluginTag> > pluginTags;
  pluginHost->GetPlugins(pluginTags);

  // need to wrap each of these with a nsPluginElement, which is
  // scriptable.
  for (uint32_t i = 0; i < pluginTags.Length(); ++i) {
    mPlugins.AppendElement(new nsPluginElement(mWindow, pluginTags[i]));
  }
}

// nsPluginElement implementation.

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPluginElement)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPluginElement)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPluginElement)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(nsPluginElement, mWindow, mMimeTypes)

nsPluginElement::nsPluginElement(nsPIDOMWindow* aWindow,
                                 nsPluginTag* aPluginTag)
  : mWindow(aWindow),
    mPluginTag(aPluginTag)
{
  SetIsDOMBinding();
}

nsPIDOMWindow*
nsPluginElement::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject*
nsPluginElement::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return PluginBinding::Wrap(aCx, aScope, this);
}

void
nsPluginElement::GetDescription(nsString& retval) const
{
  CopyUTF8toUTF16(mPluginTag->mDescription, retval);
}

void
nsPluginElement::GetFilename(nsString& retval) const
{
  CopyUTF8toUTF16(mPluginTag->mFileName, retval);
}

void
nsPluginElement::GetVersion(nsString& retval) const
{
  CopyUTF8toUTF16(mPluginTag->mVersion, retval);
}

void
nsPluginElement::GetName(nsString& retval) const
{
  CopyUTF8toUTF16(mPluginTag->mName, retval);
}

nsMimeType*
nsPluginElement::Item(uint32_t aIndex)
{
  EnsurePluginMimeTypes();

  return mMimeTypes.SafeElementAt(aIndex);
}

nsMimeType*
nsPluginElement::NamedItem(const nsAString& aName)
{
  bool unused;
  return NamedGetter(aName, unused);
}

nsMimeType*
nsPluginElement::IndexedGetter(uint32_t aIndex, bool &aFound)
{
  EnsurePluginMimeTypes();

  aFound = aIndex < mMimeTypes.Length();

  return aFound ? mMimeTypes[aIndex] : nullptr;
}

nsMimeType*
nsPluginElement::NamedGetter(const nsAString& aName, bool &aFound)
{
  EnsurePluginMimeTypes();

  aFound = false;

  for (uint32_t i = 0; i < mMimeTypes.Length(); ++i) {
    if (mMimeTypes[i]->Type().Equals(aName)) {
      aFound = true;

      return mMimeTypes[i];
    }
  }

  return nullptr;
}

uint32_t
nsPluginElement::Length()
{
  EnsurePluginMimeTypes();

  return mMimeTypes.Length();
}

void
nsPluginElement::GetSupportedNames(nsTArray< nsString >& retval)
{
  EnsurePluginMimeTypes();

  for (uint32_t i = 0; i < mMimeTypes.Length(); ++i) {
    retval.AppendElement(mMimeTypes[i]->Type());
  }
}

nsTArray<nsRefPtr<nsMimeType> >&
nsPluginElement::MimeTypes()
{
  EnsurePluginMimeTypes();

  return mMimeTypes;
}

void
nsPluginElement::EnsurePluginMimeTypes()
{
  if (!mMimeTypes.IsEmpty()) {
    return;
  }

  for (uint32_t i = 0; i < mPluginTag->mMimeTypes.Length(); ++i) {
    NS_ConvertUTF8toUTF16 type(mPluginTag->mMimeTypes[i]);
    mMimeTypes.AppendElement(new nsMimeType(mWindow, this, i, type));
  }
}

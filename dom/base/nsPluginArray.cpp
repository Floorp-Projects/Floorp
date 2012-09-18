/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginArray.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/PluginArrayBinding.h"
#include "mozilla/dom/PluginBinding.h"

#include "nsCharSeparatedTokenizer.h"
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
nsPluginArray::WrapObject(JSContext* aCx)
{
  return PluginArrayBinding::Wrap(aCx, this);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPluginArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPluginArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPluginArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsPluginArray,
                                      mWindow,
                                      mPlugins,
                                      mHiddenPlugins)

static void
GetPluginMimeTypes(const nsTArray<nsRefPtr<nsPluginElement> >& aPlugins,
                   nsTArray<nsRefPtr<nsMimeType> >& aMimeTypes)
{
  for (uint32_t i = 0; i < aPlugins.Length(); ++i) {
    nsPluginElement *plugin = aPlugins[i];
    aMimeTypes.AppendElements(plugin->MimeTypes());
  }
}

static bool
operator<(const nsRefPtr<nsMimeType>& lhs, const nsRefPtr<nsMimeType>& rhs)
{
  // Sort MIME types alphabetically by type name.
  return lhs->Type() < rhs->Type();
}

void
nsPluginArray::GetMimeTypes(nsTArray<nsRefPtr<nsMimeType> >& aMimeTypes,
                            nsTArray<nsRefPtr<nsMimeType> >& aHiddenMimeTypes)
{
  aMimeTypes.Clear();
  aHiddenMimeTypes.Clear();

  if (!AllowPlugins()) {
    return;
  }

  EnsurePlugins();

  GetPluginMimeTypes(mPlugins, aMimeTypes);
  GetPluginMimeTypes(mHiddenPlugins, aHiddenMimeTypes);

  // Alphabetize the enumeration order of non-hidden MIME types to reduce
  // fingerprintable entropy based on plugins' installation file times.
  aMimeTypes.Sort();
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
    uint32_t pluginCount = mPlugins.Length() + mHiddenPlugins.Length();
    if (newPluginTags.Length() == pluginCount) {
      return;
    }
  }

  mPlugins.Clear();
  mHiddenPlugins.Clear();

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

static nsPluginElement*
FindPlugin(const nsTArray<nsRefPtr<nsPluginElement> >& aPlugins,
           const nsAString& aName)
{
  for (uint32_t i = 0; i < aPlugins.Length(); ++i) {
    nsAutoString pluginName;
    nsPluginElement* plugin = aPlugins[i];
    plugin->GetName(pluginName);

    if (pluginName.Equals(aName)) {
      return plugin;
    }
  }

  return nullptr;
}

nsPluginElement*
nsPluginArray::NamedGetter(const nsAString& aName, bool &aFound)
{
  aFound = false;

  if (!AllowPlugins()) {
    return nullptr;
  }

  EnsurePlugins();

  nsPluginElement* plugin = FindPlugin(mPlugins, aName);
  if (!plugin) {
    plugin = FindPlugin(mHiddenPlugins, aName);
  }

  aFound = (plugin != nullptr);
  return plugin;
}

bool
nsPluginArray::NameIsEnumerable(const nsAString& aName)
{
  return true;
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
nsPluginArray::GetSupportedNames(unsigned, nsTArray<nsString>& aRetval)
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
                       const char16_t *aData) {
  if (!nsCRT::strcmp(aTopic, "plugin-info-updated")) {
    Refresh(false);
  }

  return NS_OK;
}

bool
nsPluginArray::AllowPlugins() const
{
  nsCOMPtr<nsIDocShell> docShell = mWindow ? mWindow->GetDocShell() : nullptr;

  return docShell && docShell->PluginsAllowedInCurrentDoc();
}

static bool
HasStringPrefix(const nsCString& str, const nsACString& prefix) {
  return str.Compare(prefix.BeginReading(), false, prefix.Length()) == 0;
}

static bool
IsPluginEnumerable(const nsTArray<nsCString>& enumerableNames,
                   const nsPluginTag* pluginTag)
{
  const nsCString& pluginName = pluginTag->mName;

  const uint32_t length = enumerableNames.Length();
  for (uint32_t i = 0; i < length; i++) {
    const nsCString& name = enumerableNames[i];
    if (HasStringPrefix(pluginName, name)) {
      return true; // don't hide plugin
    }
  }

  return false; // hide plugin!
}

static bool
operator<(const nsRefPtr<nsPluginElement>& lhs,
          const nsRefPtr<nsPluginElement>& rhs)
{
  // Sort plugins alphabetically by name.
  return lhs->PluginTag()->mName < rhs->PluginTag()->mName;
}

void
nsPluginArray::EnsurePlugins()
{
  if (!mPlugins.IsEmpty() || !mHiddenPlugins.IsEmpty()) {
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

  nsTArray<nsCString> enumerableNames;

  const nsAdoptingCString& enumerableNamesPref =
      Preferences::GetCString("plugins.enumerable_names");

  bool disablePluginHiding = !enumerableNamesPref ||
                             enumerableNamesPref.EqualsLiteral("*");

  if (!disablePluginHiding) {
    nsCCharSeparatedTokenizer tokens(enumerableNamesPref, ',');
    while (tokens.hasMoreTokens()) {
      const nsCSubstring& token = tokens.nextToken();
      if (!token.IsEmpty()) {
        enumerableNames.AppendElement(token);
      }
    }
  }

  // need to wrap each of these with a nsPluginElement, which is
  // scriptable.
  for (uint32_t i = 0; i < pluginTags.Length(); ++i) {
    nsPluginTag* pluginTag = pluginTags[i];

    // Add the plugin to the list of hidden plugins or non-hidden plugins?
    nsTArray<nsRefPtr<nsPluginElement> >& pluginArray =
        (disablePluginHiding || IsPluginEnumerable(enumerableNames, pluginTag))
        ? mPlugins
        : mHiddenPlugins;

    pluginArray.AppendElement(new nsPluginElement(mWindow, pluginTag));
  }

  // Alphabetize the enumeration order of non-hidden plugins to reduce
  // fingerprintable entropy based on plugins' installation file times.
  mPlugins.Sort();
}

// nsPluginElement implementation.

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPluginElement)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPluginElement)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPluginElement)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsPluginElement, mWindow, mMimeTypes)

nsPluginElement::nsPluginElement(nsPIDOMWindow* aWindow,
                                 nsPluginTag* aPluginTag)
  : mWindow(aWindow),
    mPluginTag(aPluginTag)
{
  SetIsDOMBinding();
}

nsPluginElement::~nsPluginElement()
{
}

nsPIDOMWindow*
nsPluginElement::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject*
nsPluginElement::WrapObject(JSContext* aCx)
{
  return PluginBinding::Wrap(aCx, this);
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

bool
nsPluginElement::NameIsEnumerable(const nsAString& aName)
{
  return true;
}

uint32_t
nsPluginElement::Length()
{
  EnsurePluginMimeTypes();

  return mMimeTypes.Length();
}

void
nsPluginElement::GetSupportedNames(unsigned, nsTArray<nsString>& retval)
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

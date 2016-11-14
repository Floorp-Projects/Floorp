/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginArray.h"

#include "mozilla/dom/PluginArrayBinding.h"
#include "mozilla/dom/PluginBinding.h"
#include "mozilla/dom/HiddenPluginEvent.h"

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
#include "nsContentUtils.h"
#include "nsIPermissionManager.h"
#include "nsIDocument.h"
#include "nsIBlocklistService.h"

using namespace mozilla;
using namespace mozilla::dom;

nsPluginArray::nsPluginArray(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow)
{
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

nsPluginArray::~nsPluginArray() = default;

static bool
ResistFingerprinting() {
  return !nsContentUtils::ThreadsafeIsCallerChrome() &&
         nsContentUtils::ResistFingerprinting();
}

nsPIDOMWindowInner*
nsPluginArray::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject*
nsPluginArray::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PluginArrayBinding::Wrap(aCx, this, aGivenProto);
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
                                      mCTPPlugins)

static void
GetPluginMimeTypes(const nsTArray<RefPtr<nsPluginElement> >& aPlugins,
                   nsTArray<RefPtr<nsMimeType> >& aMimeTypes)
{
  for (uint32_t i = 0; i < aPlugins.Length(); ++i) {
    nsPluginElement *plugin = aPlugins[i];
    aMimeTypes.AppendElements(plugin->MimeTypes());
  }
}

static bool
operator<(const RefPtr<nsMimeType>& lhs, const RefPtr<nsMimeType>& rhs)
{
  // Sort MIME types alphabetically by type name.
  return lhs->Type() < rhs->Type();
}

void
nsPluginArray::GetMimeTypes(nsTArray<RefPtr<nsMimeType>>& aMimeTypes)
{
  aMimeTypes.Clear();

  if (!AllowPlugins()) {
    return;
  }

  EnsurePlugins();

  GetPluginMimeTypes(mPlugins, aMimeTypes);

  // Alphabetize the enumeration order of non-hidden MIME types to reduce
  // fingerprintable entropy based on plugins' installation file times.
  aMimeTypes.Sort();
}

void
nsPluginArray::GetCTPMimeTypes(nsTArray<RefPtr<nsMimeType>>& aMimeTypes)
{
  aMimeTypes.Clear();

  if (!AllowPlugins()) {
    return;
  }

  EnsurePlugins();

  GetPluginMimeTypes(mCTPPlugins, aMimeTypes);

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
  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();

  if(!AllowPlugins() || !pluginHost) {
    return;
  }

  // NS_ERROR_PLUGINS_PLUGINSNOTCHANGED on reloading plugins indicates
  // that plugins did not change and was not reloaded
  if (pluginHost->ReloadPlugins() ==
      NS_ERROR_PLUGINS_PLUGINSNOTCHANGED) {
    nsTArray<nsCOMPtr<nsIInternalPluginTag> > newPluginTags;
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
  mCTPPlugins.Clear();

  nsCOMPtr<nsIDOMNavigator> navigator = mWindow->GetNavigator();

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

  if (!AllowPlugins() || ResistFingerprinting()) {
    return nullptr;
  }

  EnsurePlugins();

  aFound = aIndex < mPlugins.Length();

  if (!aFound) {
    return nullptr;
  }

  return mPlugins[aIndex];
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
FindPlugin(const nsTArray<RefPtr<nsPluginElement> >& aPlugins,
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

  if (!AllowPlugins() || ResistFingerprinting()) {
    return nullptr;
  }

  EnsurePlugins();

  nsPluginElement* plugin = FindPlugin(mPlugins, aName);
  aFound = (plugin != nullptr);
  if (!aFound) {
    nsPluginElement* hiddenPlugin = FindPlugin(mCTPPlugins, aName);
    if (hiddenPlugin) {
      NotifyHiddenPluginTouched(hiddenPlugin);
    }
  }
  return plugin;
}

void nsPluginArray::NotifyHiddenPluginTouched(nsPluginElement* aHiddenElement)
{
  HiddenPluginEventInit init;
  init.mTag = aHiddenElement->PluginTag();
  nsCOMPtr<nsIDocument> doc = aHiddenElement->GetParentObject()->GetDoc();
  RefPtr<HiddenPluginEvent> event =
    HiddenPluginEvent::Constructor(doc, NS_LITERAL_STRING("HiddenPlugin"), init);
  event->SetTarget(doc);
  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  bool dummy;
  doc->DispatchEvent(event, &dummy);
}

uint32_t
nsPluginArray::Length()
{
  if (!AllowPlugins() || ResistFingerprinting()) {
    return 0;
  }

  EnsurePlugins();

  return mPlugins.Length();
}

void
nsPluginArray::GetSupportedNames(nsTArray<nsString>& aRetval)
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
operator<(const RefPtr<nsPluginElement>& lhs,
          const RefPtr<nsPluginElement>& rhs)
{
  // Sort plugins alphabetically by name.
  return lhs->PluginTag()->Name() < rhs->PluginTag()->Name();
}

static bool
PluginShouldBeHidden(const nsCString& aName) {
  // This only supports one hidden plugin
  return Preferences::GetCString("plugins.navigator.hidden_ctp_plugin").Equals(aName);
}

void
nsPluginArray::EnsurePlugins()
{
  if (!mPlugins.IsEmpty() || !mCTPPlugins.IsEmpty()) {
    // We already have an array of plugin elements.
    return;
  }

  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  if (!pluginHost) {
    // We have no plugin host.
    return;
  }

  nsTArray<nsCOMPtr<nsIInternalPluginTag> > pluginTags;
  pluginHost->GetPlugins(pluginTags);

  // need to wrap each of these with a nsPluginElement, which is
  // scriptable.
  for (uint32_t i = 0; i < pluginTags.Length(); ++i) {
    nsCOMPtr<nsPluginTag> pluginTag = do_QueryInterface(pluginTags[i]);
    if (!pluginTag) {
      mPlugins.AppendElement(new nsPluginElement(mWindow, pluginTags[i]));
    } else if (pluginTag->IsActive()) {
      uint32_t permission = nsIPermissionManager::ALLOW_ACTION;
      uint32_t blocklistState;
      if (pluginTag->IsClicktoplay() &&
          NS_SUCCEEDED(pluginTag->GetBlocklistState(&blocklistState)) &&
          blocklistState == nsIBlocklistService::STATE_NOT_BLOCKED) {
        nsCString name;
        pluginTag->GetName(name);
        if (PluginShouldBeHidden(name)) {
          RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
          nsCString permString;
          nsresult rv = pluginHost->GetPermissionStringForTag(pluginTag, 0, permString);
          if (rv == NS_OK) {
            nsIPrincipal* principal = mWindow->GetExtantDoc()->NodePrincipal();
            nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
            permMgr->TestPermissionFromPrincipal(principal, permString.get(), &permission);
          }
        }
      }
      if (permission == nsIPermissionManager::ALLOW_ACTION) {
        mPlugins.AppendElement(new nsPluginElement(mWindow, pluginTags[i]));
      } else {
        mCTPPlugins.AppendElement(new nsPluginElement(mWindow, pluginTags[i]));
      }
    }
  }

  if (mPlugins.Length() == 0 && mCTPPlugins.Length() != 0) {
    nsCOMPtr<nsPluginTag> hiddenTag = new nsPluginTag("Hidden Plugin", NULL, "dummy.plugin", NULL, NULL,
                                                      NULL, NULL, NULL, 0, 0, false);
    mPlugins.AppendElement(new nsPluginElement(mWindow, hiddenTag));
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

nsPluginElement::nsPluginElement(nsPIDOMWindowInner* aWindow,
                                 nsIInternalPluginTag* aPluginTag)
  : mWindow(aWindow),
    mPluginTag(aPluginTag)
{
}

nsPluginElement::~nsPluginElement() = default;

nsPIDOMWindowInner*
nsPluginElement::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject*
nsPluginElement::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PluginBinding::Wrap(aCx, this, aGivenProto);
}

void
nsPluginElement::GetDescription(nsString& retval) const
{
  CopyUTF8toUTF16(mPluginTag->Description(), retval);
}

void
nsPluginElement::GetFilename(nsString& retval) const
{
  CopyUTF8toUTF16(mPluginTag->FileName(), retval);
}

void
nsPluginElement::GetVersion(nsString& retval) const
{
  CopyUTF8toUTF16(mPluginTag->Version(), retval);
}

void
nsPluginElement::GetName(nsString& retval) const
{
  CopyUTF8toUTF16(mPluginTag->Name(), retval);
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

  if (!aFound) {
    return nullptr;
  }

  return mMimeTypes[aIndex];
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
nsPluginElement::GetSupportedNames(nsTArray<nsString>& retval)
{
  EnsurePluginMimeTypes();

  for (uint32_t i = 0; i < mMimeTypes.Length(); ++i) {
    retval.AppendElement(mMimeTypes[i]->Type());
  }
}

nsTArray<RefPtr<nsMimeType> >&
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

  if (mPluginTag->MimeTypes().Length() != mPluginTag->MimeDescriptions().Length() ||
      mPluginTag->MimeTypes().Length() != mPluginTag->Extensions().Length()) {
    MOZ_ASSERT(false, "mime type arrays expected to be the same length");
    return;
  }

  for (uint32_t i = 0; i < mPluginTag->MimeTypes().Length(); ++i) {
    NS_ConvertUTF8toUTF16 type(mPluginTag->MimeTypes()[i]);
    NS_ConvertUTF8toUTF16 description(mPluginTag->MimeDescriptions()[i]);
    NS_ConvertUTF8toUTF16 extension(mPluginTag->Extensions()[i]);

    mMimeTypes.AppendElement(new nsMimeType(mWindow, this, type, description,
                                            extension));
  }
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsPluginHost.cpp - top-level plugin management code */

#include "nsPluginHost.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <new>
#include <utility>
#include "ReferrerInfo.h"
#include "js/RootingAPI.h"
#include "mozilla/ArrayIterator.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/NotNull.h"
#include "mozilla/PluginLibrary.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/TextUtils.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FakePluginTagInitBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "mozilla/fallible.h"
#include "mozilla/ipc/URIParams.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/mozalloc.h"
#include "mozilla/plugins/PluginTypes.h"
#include "npapi.h"
#include "npfunctions.h"
#include "nsComponentManagerUtils.h"
#include "nsIAsyncShutdown.h"
#include "nsIBlocklistService.h"
#include "nsICategoryManager.h"
#include "nsIChannel.h"
#include "nsIContent.h"
#include "nsIContentPolicy.h"
#include "nsID.h"
#include "nsIEffectiveTLDService.h"
#include "nsIFile.h"
#include "nsIHttpChannel.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIIDNService.h"
#include "nsIInputStream.h"
#include "nsILoadInfo.h"
#include "nsIObjectLoadingContent.h"
#include "nsIObserverService.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIPluginTag.h"
#include "nsIPrefBranch.h"
#include "nsIProtocolHandler.h"
#include "nsIReferrerInfo.h"
#include "nsIRequest.h"
#include "nsIScriptChannel.h"
#include "nsISeekableStream.h"
#include "nsIStringStream.h"
#include "nsISupportsUtils.h"
#include "nsIURI.h"
#include "nsIUploadChannel.h"
#include "nsIWeakReference.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWritablePropertyBag2.h"
#include "nsLiteralString.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginInstance.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsObjectLoadingContent.h"
#include "nsPluginInstanceOwner.h"
#include "nsPluginLogging.h"
#include "nsPluginNativeWindow.h"
#include "nsPluginTags.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsTArray.h"
#include "nsTLiteralString.h"
#include "nsTPromiseFlatString.h"
#include "nsTStringRepr.h"
#include "nsThreadUtils.h"
#include "nsVersionComparator.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsXULAppAPI.h"
#include "nscore.h"
#include "plstr.h"

#if defined(XP_WIN)
#  include "nsIWindowMediator.h"
#  include "nsIBaseWindow.h"
#  include <windows.h>
#  include <winbase.h>
#endif
#if (MOZ_WIDGET_GTK)
#  include "mozilla/WidgetUtilsGtk.h"
#endif

using namespace mozilla;
using mozilla::TimeStamp;
using mozilla::dom::Document;
using mozilla::dom::FakePluginMimeEntry;
using mozilla::dom::FakePluginTagInit;
using mozilla::dom::Promise;

// Null out a strong ref to a linked list iteratively to avoid
// exhausting the stack (bug 486349).
#define NS_ITERATIVE_UNREF_LIST(type_, list_, mNext_) \
  {                                                   \
    while (list_) {                                   \
      type_ temp = list_->mNext_;                     \
      list_->mNext_ = nullptr;                        \
      list_ = temp;                                   \
    }                                                 \
  }

static const char* kPrefDisableFullPage =
    "plugin.disable_full_page_plugin_for_types";

LazyLogModule nsPluginLogging::gNPNLog(NPN_LOG_NAME);
LazyLogModule nsPluginLogging::gNPPLog(NPP_LOG_NAME);
LazyLogModule nsPluginLogging::gPluginLog(PLUGIN_LOG_NAME);

// #defines for plugin cache and prefs
#define NS_PREF_MAX_NUM_CACHED_INSTANCES \
  "browser.plugins.max_num_cached_plugins"
// Raise this from '10' to '50' to work around a bug in Apple's current Java
// plugins on OS X Lion and SnowLeopard.  See bug 705931.
#define DEFAULT_NUMBER_OF_STOPPED_INSTANCES 50

nsIFile* nsPluginHost::sPluginTempDir;
StaticRefPtr<nsPluginHost> nsPluginHost::sInst;

// Helper to check for a MIME in a comma-delimited preference
static bool IsTypeInList(const nsCString& aMimeType, nsCString aTypeList) {
  nsAutoCString searchStr;
  searchStr.Assign(',');
  searchStr.Append(aTypeList);
  searchStr.Append(',');

  nsACString::const_iterator start, end;

  searchStr.BeginReading(start);
  searchStr.EndReading(end);

  nsAutoCString commaSeparated;
  commaSeparated.Assign(',');
  commaSeparated += aMimeType;
  commaSeparated.Append(',');

  // Lower-case the search string and MIME type to properly handle a mixed-case
  // type, as MIME types are case insensitive.
  ToLowerCase(searchStr);
  ToLowerCase(commaSeparated);

  return FindInReadable(commaSeparated, start, end);
}

namespace mozilla::plugins {
class BlocklistPromiseHandler final
    : public mozilla::dom::PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  BlocklistPromiseHandler(nsPluginTag* aTag, const bool aShouldSoftblock)
      : mTag(aTag), mShouldDisableWhenSoftblocked(aShouldSoftblock) {
    MOZ_ASSERT(mTag, "Should always be passed a plugin tag");
    sPendingBlocklistStateRequests++;
  }

  void MaybeWriteBlocklistChanges() {
    // We're called immediately when the promise resolves/rejects, and (as a
    // backup) when the handler is destroyed. To ensure we only run once, we use
    // mTag as a sentinel, setting it to nullptr when we run.
    if (!mTag) {
      return;
    }
    mTag = nullptr;
    sPendingBlocklistStateRequests--;
    // If this was the only remaining pending request, check if we need to write
    // state and if so update the child processes.
    if (!sPendingBlocklistStateRequests) {
      if (sPluginBlocklistStatesChangedSinceLastWrite) {
        sPluginBlocklistStatesChangedSinceLastWrite = false;

        RefPtr<nsPluginHost> host = nsPluginHost::GetInst();
        // We update blocklist info in content processes asynchronously
        // by just sending a new plugin list to content.
        host->IncrementChromeEpoch();
        host->BroadcastPluginsToContent();
      }

      // Now notify observers that we're done updating plugin state.
      nsCOMPtr<nsIObserverService> obsService =
          mozilla::services::GetObserverService();
      if (obsService) {
        obsService->NotifyObservers(
            nullptr, "plugin-blocklist-updates-finished", nullptr);
      }
    }
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    if (!aValue.isInt32()) {
      MOZ_ASSERT(false, "Blocklist should always return int32");
      return;
    }
    int32_t newState = aValue.toInt32();
    MOZ_ASSERT(newState >= 0 && newState < nsIBlocklistService::STATE_MAX,
               "Shouldn't get an out of bounds blocklist state");

    // Check the old and new state and see if there was a change:
    uint32_t oldState = mTag->GetBlocklistState();
    bool changed = oldState != (uint32_t)newState;
    mTag->SetBlocklistState(newState);

    if (newState == nsIBlocklistService::STATE_SOFTBLOCKED &&
        mShouldDisableWhenSoftblocked) {
      mTag->SetEnabledState(nsIPluginTag::STATE_DISABLED);
      changed = true;
    }
    sPluginBlocklistStatesChangedSinceLastWrite |= changed;

    MaybeWriteBlocklistChanges();
  }
  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    MOZ_ASSERT(false, "Shouldn't reject plugin blocklist state request");
    MaybeWriteBlocklistChanges();
  }

 private:
  ~BlocklistPromiseHandler() {
    // If we have multiple plugins and the last pending request is GC'd
    // and so never resolves/rejects, ensure we still write the blocklist.
    MaybeWriteBlocklistChanges();
  }

  RefPtr<nsPluginTag> mTag;
  bool mShouldDisableWhenSoftblocked;

  // Whether we changed any of the plugins' blocklist states since
  // we last started fetching them (async). This is reset to false
  // every time we finish fetching plugin blocklist information.
  // When this happens, if the previous value was true, we store the
  // updated list on disk and send it to child processes.
  static bool sPluginBlocklistStatesChangedSinceLastWrite;
  // How many pending blocklist state requests we've got
  static uint32_t sPendingBlocklistStateRequests;
};

NS_IMPL_ISUPPORTS0(BlocklistPromiseHandler)

bool BlocklistPromiseHandler::sPluginBlocklistStatesChangedSinceLastWrite =
    false;
uint32_t BlocklistPromiseHandler::sPendingBlocklistStateRequests = 0;
}  // namespace mozilla::plugins

nsPluginHost::nsPluginHost()
    : mPluginsLoaded(false),
      mOverrideInternalTypes(false),
      mPluginsDisabled(false),
      mPluginEpoch(0) {
  // check to see if pref is set at startup to let plugins take over in
  // full page mode for certain image mime types that we handle internally
  mOverrideInternalTypes =
      Preferences::GetBool("plugin.override_internal_types", false);

  bool waylandBackend = false;
#if defined(MOZ_WIDGET_GTK)
  waylandBackend = mozilla::widget::GdkIsWaylandDisplay();
#endif
  mPluginsDisabled =
      Preferences::GetBool("plugin.disable", false) || waylandBackend;
  if (!waylandBackend) {
    Preferences::AddStrongObserver(this, "plugin.disable");
  }

  nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
  if (obsService) {
    obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    if (XRE_IsParentProcess()) {
      obsService->AddObserver(this, "plugin-blocklist-updated", false);
    }
  }

#ifdef PLUGIN_LOGGING
  MOZ_LOG(nsPluginLogging::gNPNLog, PLUGIN_LOG_ALWAYS,
          ("NPN Logging Active!\n"));
  MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_ALWAYS,
          ("General Plugin Logging Active! (nsPluginHost::ctor)\n"));
  MOZ_LOG(nsPluginLogging::gNPPLog, PLUGIN_LOG_ALWAYS,
          ("NPP Logging Active!\n"));

  PLUGIN_LOG(PLUGIN_LOG_ALWAYS, ("nsPluginHost::ctor\n"));
  PR_LogFlush();
#endif
  // We need to ensure that plugin tag sandbox info is available. This needs to
  // be done from the main thread:
  nsPluginTag::EnsureSandboxInformation();

  // Load plugins on creation, as there's a good chance we'll need to send them
  // to content processes directly after creation.
  if (XRE_IsParentProcess()) {
    // Always increment the chrome epoch when we bring up the nsPluginHost in
    // the parent process. If the only plugins we have are cached in
    // pluginreg.dat, we won't see any plugin changes in LoadPlugins and the
    // epoch will stay the same between the parent and child process, meaning
    // plugins will never update in the child process.
    IncrementChromeEpoch();
    LoadPlugins();
  }
}

nsPluginHost::~nsPluginHost() {
  PLUGIN_LOG(PLUGIN_LOG_ALWAYS, ("nsPluginHost::dtor\n"));

  UnloadPlugins();
}

NS_IMPL_ISUPPORTS(nsPluginHost, nsIPluginHost, nsIObserver, nsITimerCallback,
                  nsISupportsWeakReference, nsINamed)

already_AddRefed<nsPluginHost> nsPluginHost::GetInst() {
  if (!sInst) {
    sInst = new nsPluginHost();
    ClearOnShutdown(&sInst);
  }

  return do_AddRef(sInst);
}

bool nsPluginHost::IsRunningPlugin(nsPluginTag* aPluginTag) { return false; }

nsresult nsPluginHost::ReloadPlugins() {
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHost::ReloadPlugins\n"));
  return NS_ERROR_PLUGINS_PLUGINSNOTCHANGED;
}

void nsPluginHost::ClearNonRunningPlugins() {
  // shutdown plugins and kill the list if there are no running plugins
  RefPtr<nsPluginTag> prev;
  RefPtr<nsPluginTag> next;

  for (RefPtr<nsPluginTag> p = mPlugins; p != nullptr;) {
    next = p->mNext;

    // only remove our plugin from the list if it's not running.
    if (!IsRunningPlugin(p)) {
      if (p == mPlugins)
        mPlugins = next;
      else
        prev->mNext = next;

      p->mNext = nullptr;

      // attempt to unload plugins whenever they are removed from the list
      p->TryUnloadPlugin(false);

      p = next;
      continue;
    }

    prev = p;
    p = next;
  }
}

nsresult nsPluginHost::ActuallyReloadPlugins() {
  nsresult rv = NS_OK;
  ClearNonRunningPlugins();

  // set flags
  mPluginsLoaded = false;

  // load them again
  rv = LoadPlugins();

  if (XRE_IsParentProcess()) {
    // If the plugin list changed, update content. If the plugin list changed
    // for the content process, it will also reload plugins.
    BroadcastPluginsToContent();
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHost::ReloadPlugins End\n"));

  return rv;
}

#define NS_RETURN_UASTRING_SIZE 128

nsresult nsPluginHost::UserAgent(const char** retstring) {
  static char resultString[NS_RETURN_UASTRING_SIZE];
  nsresult res;

  nsCOMPtr<nsIHttpProtocolHandler> http =
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &res);
  if (NS_FAILED(res)) return res;

  nsAutoCString uaString;
  res = http->GetUserAgent(uaString);

  if (NS_SUCCEEDED(res)) {
    if (NS_RETURN_UASTRING_SIZE > uaString.Length()) {
      PL_strcpy(resultString, uaString.get());
    } else {
      // Copy as much of UA string as we can (terminate at right-most space).
      PL_strncpy(resultString, uaString.get(), NS_RETURN_UASTRING_SIZE);
      for (int i = NS_RETURN_UASTRING_SIZE - 1; i >= 0; i--) {
        if (i == 0) {
          resultString[NS_RETURN_UASTRING_SIZE - 1] = '\0';
        } else if (resultString[i] == ' ') {
          resultString[i] = '\0';
          break;
        }
      }
    }
    *retstring = resultString;
  } else {
    *retstring = nullptr;
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
             ("nsPluginHost::UserAgent return=%s\n", *retstring));

  return res;
}

nsresult nsPluginHost::UnloadPlugins() {
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHost::UnloadPlugins Called\n"));

  if (!mPluginsLoaded) return NS_OK;

  // we should call nsIPluginInstance::Stop and nsIPluginInstance::SetWindow
  // for those plugins who want it
  DestroyRunningInstances(nullptr);

  nsPluginTag* pluginTag;
  for (pluginTag = mPlugins; pluginTag; pluginTag = pluginTag->mNext) {
    pluginTag->TryUnloadPlugin(true);
  }

  NS_ITERATIVE_UNREF_LIST(RefPtr<nsPluginTag>, mPlugins, mNext);

  // Lets remove any of the temporary files that we created.
  if (sPluginTempDir) {
    sPluginTempDir->Remove(true);
    NS_RELEASE(sPluginTempDir);
  }

  mPluginsLoaded = false;

  return NS_OK;
}

void nsPluginHost::OnPluginInstanceDestroyed(nsPluginTag* aPluginTag) {}

nsPluginTag* nsPluginHost::FindTagForLibrary(PRLibrary* aLibrary) {
  nsPluginTag* pluginTag;
  for (pluginTag = mPlugins; pluginTag; pluginTag = pluginTag->mNext) {
    if (pluginTag->mLibrary == aLibrary) {
      return pluginTag;
    }
  }
  return nullptr;
}

bool nsPluginHost::HavePluginForType(const nsACString& aMimeType,
                                     PluginFilter aFilter) {
  bool checkEnabled = aFilter & eExcludeDisabled;
  bool allowFake = !(aFilter & eExcludeFake);
  return FindPluginForType(aMimeType, allowFake, checkEnabled);
}

nsIInternalPluginTag* nsPluginHost::FindPluginForType(
    const nsACString& aMimeType, bool aIncludeFake, bool aCheckEnabled) {
  if (aIncludeFake) {
    nsFakePluginTag* fakeTag = FindFakePluginForType(aMimeType, aCheckEnabled);
    if (fakeTag) {
      return fakeTag;
    }
  }

  return FindNativePluginForType(aMimeType, aCheckEnabled);
}

NS_IMETHODIMP
nsPluginHost::GetPluginTagForType(const nsACString& aMimeType,
                                  uint32_t aExcludeFlags,
                                  nsIPluginTag** aResult) {
  bool includeFake = !(aExcludeFlags & eExcludeFake);
  bool includeDisabled = !(aExcludeFlags & eExcludeDisabled);

  // First look for an enabled plugin.
  RefPtr<nsIInternalPluginTag> tag =
      FindPluginForType(aMimeType, includeFake, true);
  if (!tag && includeDisabled) {
    tag = FindPluginForType(aMimeType, includeFake, false);
  }

  if (tag) {
    tag.forget(aResult);
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsPluginHost::GetStateForType(const nsACString& aMimeType,
                              uint32_t aExcludeFlags, uint32_t* aResult) {
  nsCOMPtr<nsIPluginTag> tag;
  nsresult rv =
      GetPluginTagForType(aMimeType, aExcludeFlags, getter_AddRefs(tag));
  NS_ENSURE_SUCCESS(rv, rv);

  return tag->GetEnabledState(aResult);
}

NS_IMETHODIMP
nsPluginHost::GetBlocklistStateForType(const nsACString& aMimeType,
                                       uint32_t aExcludeFlags,
                                       uint32_t* aState) {
  nsCOMPtr<nsIPluginTag> tag;
  nsresult rv =
      GetPluginTagForType(aMimeType, aExcludeFlags, getter_AddRefs(tag));
  NS_ENSURE_SUCCESS(rv, rv);
  return tag->GetBlocklistState(aState);
}

NS_IMETHODIMP
nsPluginHost::GetPermissionStringForType(const nsACString& aMimeType,
                                         uint32_t aExcludeFlags,
                                         nsACString& aPermissionString) {
  nsCOMPtr<nsIPluginTag> tag;
  nsresult rv =
      GetPluginTagForType(aMimeType, aExcludeFlags, getter_AddRefs(tag));
  NS_ENSURE_SUCCESS(rv, rv);
  return GetPermissionStringForTag(tag, aExcludeFlags, aPermissionString);
}

NS_IMETHODIMP
nsPluginHost::GetPermissionStringForTag(nsIPluginTag* aTag,
                                        uint32_t aExcludeFlags,
                                        nsACString& aPermissionString) {
  NS_ENSURE_TRUE(aTag, NS_ERROR_FAILURE);

  aPermissionString.Truncate();
  uint32_t blocklistState;
  nsresult rv = aTag->GetBlocklistState(&blocklistState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (blocklistState ==
          nsIBlocklistService::STATE_VULNERABLE_UPDATE_AVAILABLE ||
      blocklistState == nsIBlocklistService::STATE_VULNERABLE_NO_UPDATE) {
    aPermissionString.AssignLiteral("plugin-vulnerable:");
  } else {
    aPermissionString.AssignLiteral("plugin:");
  }

  nsCString niceName;
  rv = aTag->GetNiceName(niceName);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!niceName.IsEmpty(), NS_ERROR_FAILURE);

  aPermissionString.Append(niceName);

  return NS_OK;
}

bool nsPluginHost::HavePluginForExtension(const nsACString& aExtension,
                                          /* out */ nsACString& aMimeType,
                                          PluginFilter aFilter) {
  // As of FF 52, we only support flash and test plugins, so if the extension
  // types don't match for that, exit before we start loading plugins.
  //
  // XXX: Remove tst case when bug 1351885 lands.
  if (!aExtension.LowerCaseEqualsLiteral("swf") &&
      !aExtension.LowerCaseEqualsLiteral("tst")) {
    return false;
  }

  bool checkEnabled = aFilter & eExcludeDisabled;
  bool allowFake = !(aFilter & eExcludeFake);
  return FindNativePluginForExtension(aExtension, aMimeType, checkEnabled) ||
         (allowFake &&
          FindFakePluginForExtension(aExtension, aMimeType, checkEnabled));
}

void nsPluginHost::GetPlugins(
    nsTArray<nsCOMPtr<nsIInternalPluginTag>>& aPluginArray,
    bool aIncludeDisabled) {
  aPluginArray.Clear();

  LoadPlugins();

  // Append fake plugins, then normal plugins.

  uint32_t numFake = mFakePlugins.Length();
  for (uint32_t i = 0; i < numFake; i++) {
    aPluginArray.AppendElement(mFakePlugins[i]);
  }

  // Regular plugins
  nsPluginTag* plugin = mPlugins;
  while (plugin != nullptr) {
    if (plugin->IsEnabled() || aIncludeDisabled) {
      aPluginArray.AppendElement(plugin);
    }
    plugin = plugin->mNext;
  }
}

// FIXME-jsplugins Check users for order of fake v non-fake
NS_IMETHODIMP
nsPluginHost::GetPluginTags(nsTArray<RefPtr<nsIPluginTag>>& aResults) {
  LoadPlugins();

  for (nsPluginTag* plugin = mPlugins; plugin; plugin = plugin->mNext) {
    aResults.AppendElement(plugin);
  }

  for (nsIInternalPluginTag* plugin : mFakePlugins) {
    aResults.AppendElement(plugin);
  }

  return NS_OK;
}

nsPluginTag* nsPluginHost::FindPreferredPlugin(
    const nsTArray<nsPluginTag*>& matches) {
  // We prefer the plugin with the highest version number.
  /// XXX(johns): This seems to assume the only time multiple plugins will have
  ///             the same MIME type is if they're multiple versions of the same
  ///             plugin -- but since plugin filenames and pretty names can both
  ///             update, it's probably less arbitrary than just going at it
  ///             alphabetically.

  if (matches.IsEmpty()) {
    return nullptr;
  }

  nsPluginTag* preferredPlugin = matches[0];
  for (unsigned int i = 1; i < matches.Length(); i++) {
    if (mozilla::Version(matches[i]->Version().get()) >
        preferredPlugin->Version().get()) {
      preferredPlugin = matches[i];
    }
  }

  return preferredPlugin;
}

nsFakePluginTag* nsPluginHost::FindFakePluginForExtension(
    const nsACString& aExtension,
    /* out */ nsACString& aMimeType, bool aCheckEnabled) {
  if (aExtension.IsEmpty()) {
    return nullptr;
  }

  int32_t numFakePlugins = mFakePlugins.Length();
  for (int32_t i = 0; i < numFakePlugins; i++) {
    nsFakePluginTag* plugin = mFakePlugins[i];
    bool active;
    if ((!aCheckEnabled ||
         (NS_SUCCEEDED(plugin->GetActive(&active)) && active)) &&
        plugin->HasExtension(aExtension, aMimeType)) {
      return plugin;
    }
  }

  return nullptr;
}

nsFakePluginTag* nsPluginHost::FindFakePluginForType(
    const nsACString& aMimeType, bool aCheckEnabled) {
  int32_t numFakePlugins = mFakePlugins.Length();
  for (int32_t i = 0; i < numFakePlugins; i++) {
    nsFakePluginTag* plugin = mFakePlugins[i];
    bool active;
    if ((!aCheckEnabled ||
         (NS_SUCCEEDED(plugin->GetActive(&active)) && active)) &&
        plugin->HasMimeType(aMimeType)) {
      return plugin;
    }
  }

  return nullptr;
}

nsPluginTag* nsPluginHost::FindNativePluginForType(const nsACString& aMimeType,
                                                   bool aCheckEnabled) {
  if (aMimeType.IsEmpty()) {
    return nullptr;
  }

  // As of FF 52, we only support flash and test plugins, so if the mime types
  // don't match for that, exit before we start loading plugins.
  if (!nsPluginHost::CanUsePluginForMIMEType(aMimeType)) {
    return nullptr;
  }

  LoadPlugins();

  nsTArray<nsPluginTag*> matchingPlugins;

  nsPluginTag* plugin = mPlugins;
  while (plugin) {
    if ((!aCheckEnabled || plugin->IsActive()) &&
        plugin->HasMimeType(aMimeType)) {
      matchingPlugins.AppendElement(plugin);
    }
    plugin = plugin->mNext;
  }

  return FindPreferredPlugin(matchingPlugins);
}

nsPluginTag* nsPluginHost::FindNativePluginForExtension(
    const nsACString& aExtension,
    /* out */ nsACString& aMimeType, bool aCheckEnabled) {
  if (aExtension.IsEmpty()) {
    return nullptr;
  }

  LoadPlugins();

  nsTArray<nsPluginTag*> matchingPlugins;
  nsCString matchingMime;  // Don't mutate aMimeType unless returning a match
  nsPluginTag* plugin = mPlugins;

  while (plugin) {
    if (!aCheckEnabled || plugin->IsActive()) {
      if (plugin->HasExtension(aExtension, matchingMime)) {
        matchingPlugins.AppendElement(plugin);
      }
    }
    plugin = plugin->mNext;
  }

  nsPluginTag* preferredPlugin = FindPreferredPlugin(matchingPlugins);
  if (!preferredPlugin) {
    return nullptr;
  }

  // Re-fetch the matching type because of how FindPreferredPlugin works...
  preferredPlugin->HasExtension(aExtension, aMimeType);
  return preferredPlugin;
}

nsresult nsPluginHost::EnsurePluginLoaded(nsPluginTag* aPluginTag) {
  return NS_ERROR_FAILURE;
}

class nsPluginUnloadRunnable : public Runnable {
 public:
  explicit nsPluginUnloadRunnable(uint32_t aPluginId)
      : Runnable("nsPluginUnloadRunnable"), mPluginId(aPluginId) {}

  NS_IMETHOD Run() override {
    RefPtr<nsPluginHost> host = nsPluginHost::GetInst();
    if (!host) {
      return NS_OK;
    }
    nsPluginTag* pluginTag = host->PluginWithId(mPluginId);
    if (!pluginTag) {
      return NS_OK;
    }

    MOZ_ASSERT(pluginTag->mContentProcessRunningCount > 0);
    pluginTag->mContentProcessRunningCount--;

    if (!pluginTag->mContentProcessRunningCount) {
      if (!host->IsRunningPlugin(pluginTag)) {
        pluginTag->TryUnloadPlugin(false);
      }
    }
    return NS_OK;
  }

 protected:
  uint32_t mPluginId;
};

void nsPluginHost::NotifyContentModuleDestroyed(uint32_t aPluginId) {
  MOZ_ASSERT(XRE_IsParentProcess());

  // This is called in response to a message from the plugin. Don't unload the
  // plugin until the message handler is off the stack.
  RefPtr<nsPluginUnloadRunnable> runnable =
      new nsPluginUnloadRunnable(aPluginId);
  NS_DispatchToMainThread(runnable);
}

// Normalize 'host' to ACE.
nsresult nsPluginHost::NormalizeHostname(nsCString& host) {
  if (IsAscii(host)) {
    ToLowerCase(host);
    return NS_OK;
  }

  if (!mIDNService) {
    nsresult rv;
    mIDNService = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return mIDNService->ConvertUTF8toACE(host, host);
}

// Enumerate a 'sites' array returned by GetSitesWithData and determine if
// any of them have a base domain in common with 'domain'; if so, append them
// to the 'result' array. If 'firstMatchOnly' is true, return after finding the
// first match.
nsresult nsPluginHost::EnumerateSiteData(const nsACString& domain,
                                         const nsTArray<nsCString>& sites,
                                         nsTArray<nsCString>& result,
                                         bool firstMatchOnly) {
  NS_ASSERTION(!domain.IsVoid(), "null domain string");

  nsresult rv;
  if (!mTLDService) {
    mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get the base domain from the domain.
  nsCString baseDomain;
  rv = mTLDService->GetBaseDomainFromHost(domain, 0, baseDomain);
  bool isIP = rv == NS_ERROR_HOST_IS_IP_ADDRESS;
  if (isIP || rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // The base domain is the site itself. However, we must be careful to
    // normalize.
    baseDomain = domain;
    rv = NormalizeHostname(baseDomain);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (NS_FAILED(rv)) {
    return rv;
  }

  // Enumerate the array of sites with data.
  for (uint32_t i = 0; i < sites.Length(); ++i) {
    const nsCString& site = sites[i];

    // Check if the site is an IP address.
    bool siteIsIP =
        site.Length() >= 2 && site.First() == '[' && site.Last() == ']';
    if (siteIsIP != isIP) continue;

    nsCString siteBaseDomain;
    if (siteIsIP) {
      // Strip the '[]'.
      siteBaseDomain = Substring(site, 1, site.Length() - 2);
    } else {
      // Determine the base domain of the site.
      rv = mTLDService->GetBaseDomainFromHost(site, 0, siteBaseDomain);
      if (rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        // The base domain is the site itself. However, we must be careful to
        // normalize.
        siteBaseDomain = site;
        rv = NormalizeHostname(siteBaseDomain);
        NS_ENSURE_SUCCESS(rv, rv);
      } else if (NS_FAILED(rv)) {
        return rv;
      }
    }

    // At this point, we can do an exact comparison of the two domains.
    if (baseDomain != siteBaseDomain) {
      continue;
    }

    // Append the site to the result array.
    result.AppendElement(site);

    // If we're supposed to return early, do so.
    if (firstMatchOnly) {
      break;
    }
  }

  return NS_OK;
}

static bool MimeTypeIsAllowedForFakePlugin(const nsString& aMimeType) {
  static const char* const allowedFakePlugins[] = {
      // Flash
      "application/x-shockwave-flash",
      // PDF
      "application/pdf",
      "application/vnd.adobe.pdf",
      "application/vnd.adobe.pdfxml",
      "application/vnd.adobe.x-mars",
      "application/vnd.adobe.xdp+xml",
      "application/vnd.adobe.xfdf",
      "application/vnd.adobe.xfd+xml",
      "application/vnd.fdf",
  };

  for (const auto allowed : allowedFakePlugins) {
    if (aMimeType.EqualsASCII(allowed)) {
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP
nsPluginHost::RegisterFakePlugin(JS::Handle<JS::Value> aInitDictionary,
                                 JSContext* aCx, nsIFakePluginTag** aResult) {
  FakePluginTagInit initDictionary;
  if (!initDictionary.Init(aCx, aInitDictionary)) {
    return NS_ERROR_FAILURE;
  }

  for (const FakePluginMimeEntry& mimeEntry : initDictionary.mMimeEntries) {
    if (!MimeTypeIsAllowedForFakePlugin(mimeEntry.mType)) {
      return NS_ERROR_FAILURE;
    }
  }

  RefPtr<nsFakePluginTag> newTag;
  nsresult rv = nsFakePluginTag::Create(initDictionary, getter_AddRefs(newTag));
  NS_ENSURE_SUCCESS(rv, rv);

  for (const auto& existingTag : mFakePlugins) {
    if (newTag->HandlerURIMatches(existingTag->HandlerURI())) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  mFakePlugins.AppendElement(newTag);

  nsAutoCString disableFullPage;
  Preferences::GetCString(kPrefDisableFullPage, disableFullPage);
  for (uint32_t i = 0; i < newTag->MimeTypes().Length(); i++) {
    if (!IsTypeInList(newTag->MimeTypes()[i], disableFullPage)) {
      RegisterWithCategoryManager(newTag->MimeTypes()[i], ePluginRegister);
    }
  }

  newTag.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginHost::CreateFakePlugin(JS::Handle<JS::Value> aInitDictionary,
                               JSContext* aCx, nsIFakePluginTag** aResult) {
  FakePluginTagInit initDictionary;
  if (!initDictionary.Init(aCx, aInitDictionary)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsFakePluginTag> newTag;
  nsresult rv = nsFakePluginTag::Create(initDictionary, getter_AddRefs(newTag));
  NS_ENSURE_SUCCESS(rv, rv);

  newTag.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginHost::UnregisterFakePlugin(const nsACString& aHandlerURI) {
  nsCOMPtr<nsIURI> handlerURI;
  nsresult rv = NS_NewURI(getter_AddRefs(handlerURI), aHandlerURI);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < mFakePlugins.Length(); ++i) {
    if (mFakePlugins[i]->HandlerURIMatches(handlerURI)) {
      mFakePlugins.RemoveElementAt(i);
      return NS_OK;
    }
  }

  return NS_OK;
}

// FIXME-jsplugins Is this method actually needed?
NS_IMETHODIMP
nsPluginHost::GetFakePlugin(const nsACString& aMimeType,
                            nsIFakePluginTag** aResult) {
  RefPtr<nsFakePluginTag> result = FindFakePluginForType(aMimeType, false);
  if (result) {
    result.forget(aResult);
    return NS_OK;
  }

  *aResult = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}

// FIXME-jsplugins what should this do for fake plugins?
NS_IMETHODIMP
nsPluginHost::ClearSiteData(nsIPluginTag* plugin, const nsACString& domain,
                            uint64_t flags, int64_t maxAge,
                            nsIClearSiteDataCallback* callbackFunc) {
  return NS_ERROR_FAILURE;
}

// This will spin the event loop while waiting on an async
// call to GetSitesWithData
NS_IMETHODIMP
nsPluginHost::SiteHasData(nsIPluginTag* plugin, const nsACString& domain,
                          bool* result) {
  return NS_ERROR_FAILURE;
}

nsPluginHost::SpecialType nsPluginHost::GetSpecialType(
    const nsACString& aMIMEType) {
  if (aMIMEType.LowerCaseEqualsASCII("application/x-test")) {
    return eSpecialType_Test;
  }

  if (aMIMEType.LowerCaseEqualsASCII("application/x-shockwave-flash") ||
      aMIMEType.LowerCaseEqualsASCII("application/futuresplash") ||
      aMIMEType.LowerCaseEqualsASCII("application/x-shockwave-flash-test")) {
    return eSpecialType_Flash;
  }

  return eSpecialType_None;
}

// Check whether or not a tag is a live, valid tag, and that it's loaded.
bool nsPluginHost::IsLiveTag(nsIPluginTag* aPluginTag) {
  nsCOMPtr<nsIInternalPluginTag> internalTag(do_QueryInterface(aPluginTag));
  uint32_t fakeCount = mFakePlugins.Length();
  for (uint32_t i = 0; i < fakeCount; i++) {
    if (mFakePlugins[i] == internalTag) {
      return true;
    }
  }

  nsPluginTag* tag;
  for (tag = mPlugins; tag; tag = tag->mNext) {
    if (tag == internalTag) {
      return true;
    }
  }
  return false;
}

// FIXME-jsplugins what should happen with jsplugins here, if anything?
nsPluginTag* nsPluginHost::HaveSamePlugin(const nsPluginTag* aPluginTag) {
  for (nsPluginTag* tag = mPlugins; tag; tag = tag->mNext) {
    if (tag->HasSameNameAndMimes(aPluginTag)) {
      return tag;
    }
  }
  return nullptr;
}

nsPluginTag* nsPluginHost::PluginWithId(uint32_t aId) {
  for (nsPluginTag* tag = mPlugins; tag; tag = tag->mNext) {
    if (tag->mId == aId) {
      return tag;
    }
  }
  return nullptr;
}

void nsPluginHost::AddPluginTag(nsPluginTag* aPluginTag) {
  aPluginTag->mNext = mPlugins;
  mPlugins = aPluginTag;

  if (aPluginTag->IsActive()) {
    nsAutoCString disableFullPage;
    Preferences::GetCString(kPrefDisableFullPage, disableFullPage);
    for (uint32_t i = 0; i < aPluginTag->MimeTypes().Length(); i++) {
      if (!IsTypeInList(aPluginTag->MimeTypes()[i], disableFullPage)) {
        RegisterWithCategoryManager(aPluginTag->MimeTypes()[i],
                                    ePluginRegister);
      }
    }
  }
}

void nsPluginHost::UpdatePluginBlocklistState(nsPluginTag* aPluginTag,
                                              bool aShouldSoftblock) {
  nsCOMPtr<nsIBlocklistService> blocklist =
      do_GetService("@mozilla.org/extensions/blocklist;1");
  MOZ_ASSERT(blocklist, "Should be able to access the blocklist");
  if (!blocklist) {
    return;
  }
  // Asynchronously get the blocklist state.
  RefPtr<Promise> promise;
  blocklist->GetPluginBlocklistState(aPluginTag, u""_ns, u""_ns,
                                     getter_AddRefs(promise));
  MOZ_ASSERT(promise,
             "Should always get a promise for plugin blocklist state.");
  if (promise) {
    promise->AppendNativeHandler(new mozilla::plugins::BlocklistPromiseHandler(
        aPluginTag, aShouldSoftblock));
  }
}

void nsPluginHost::IncrementChromeEpoch() {
  MOZ_ASSERT(XRE_IsParentProcess());
  mPluginEpoch++;
}

uint32_t nsPluginHost::ChromeEpoch() {
  MOZ_ASSERT(XRE_IsParentProcess());
  return mPluginEpoch;
}

uint32_t nsPluginHost::ChromeEpochForContent() {
  MOZ_ASSERT(XRE_IsContentProcess());
  return mPluginEpoch;
}

void nsPluginHost::SetChromeEpochForContent(uint32_t aEpoch) {
  MOZ_ASSERT(XRE_IsContentProcess());
  mPluginEpoch = aEpoch;
}

already_AddRefed<nsIAsyncShutdownClient> GetProfileChangeTeardownPhase() {
  nsCOMPtr<nsIAsyncShutdownService> asyncShutdownSvc =
      services::GetAsyncShutdownService();
  MOZ_ASSERT(asyncShutdownSvc);
  if (NS_WARN_IF(!asyncShutdownSvc)) {
    return nullptr;
  }

  nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase;
  DebugOnly<nsresult> rv =
      asyncShutdownSvc->GetProfileChangeTeardown(getter_AddRefs(shutdownPhase));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return shutdownPhase.forget();
}

nsresult nsPluginHost::LoadPlugins() { return NS_OK; }

void nsPluginHost::FindingFinished() {}

nsresult nsPluginHost::UpdateCachedSerializablePluginList() {
  return NS_ERROR_FAILURE;
}

nsresult nsPluginHost::BroadcastPluginsToContent() { return NS_ERROR_FAILURE; }

nsresult nsPluginHost::SendPluginsToContent(dom::ContentParent* parent) {
  return NS_ERROR_FAILURE;
}

void nsPluginHost::UpdateInMemoryPluginInfo(nsPluginTag* aPluginTag) {
  if (!aPluginTag) {
    return;
  }

  // Update types with category manager
  nsAutoCString disableFullPage;
  Preferences::GetCString(kPrefDisableFullPage, disableFullPage);
  for (uint32_t i = 0; i < aPluginTag->MimeTypes().Length(); i++) {
    nsRegisterType shouldRegister;

    if (IsTypeInList(aPluginTag->MimeTypes()[i], disableFullPage)) {
      shouldRegister = ePluginUnregister;
    } else {
      nsPluginTag* plugin =
          FindNativePluginForType(aPluginTag->MimeTypes()[i], true);
      shouldRegister = plugin ? ePluginRegister : ePluginUnregister;
    }

    RegisterWithCategoryManager(aPluginTag->MimeTypes()[i], shouldRegister);
  }

  nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
  if (obsService)
    obsService->NotifyObservers(nullptr, "plugin-info-updated", nullptr);
}

// This function is not relevant for fake plugins.
void nsPluginHost::UpdatePluginInfo(nsPluginTag* aPluginTag) {
  MOZ_ASSERT(XRE_IsParentProcess());

  IncrementChromeEpoch();

  UpdateInMemoryPluginInfo(aPluginTag);
}

void nsPluginHost::RegisterWithCategoryManager(const nsCString& aMimeType,
                                               nsRegisterType aType) {
  PLUGIN_LOG(
      PLUGIN_LOG_NORMAL,
      ("nsPluginTag::RegisterWithCategoryManager type = %s, removing = %s\n",
       aMimeType.get(), aType == ePluginUnregister ? "yes" : "no"));

  nsCOMPtr<nsICategoryManager> catMan =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan) {
    return;
  }

  constexpr auto contractId =
      "@mozilla.org/content/plugin/document-loader-factory;1"_ns;

  if (aType == ePluginRegister) {
    catMan->AddCategoryEntry("Gecko-Content-Viewers", aMimeType, contractId,
                             false, /* persist: broken by bug 193031 */
                             mOverrideInternalTypes);
  } else {
    if (aType == ePluginMaybeUnregister) {
      // Bail out if this type is still used by an enabled plugin
      if (HavePluginForType(aMimeType)) {
        return;
      }
    } else {
      MOZ_ASSERT(aType == ePluginUnregister, "Unknown nsRegisterType");
    }

    // Only delete the entry if a plugin registered for it
    nsCString value;
    nsresult rv =
        catMan->GetCategoryEntry("Gecko-Content-Viewers", aMimeType, value);
    if (NS_SUCCEEDED(rv) && value == contractId) {
      catMan->DeleteCategoryEntry("Gecko-Content-Viewers", aMimeType, true);
    }
  }
}

nsresult nsPluginHost::AddHeadersToChannel(const char* aHeadersData,
                                           uint32_t aHeadersDataLen,
                                           nsIChannel* aGenericChannel) {
  nsresult rv = NS_OK;

  nsCOMPtr<nsIHttpChannel> aChannel = do_QueryInterface(aGenericChannel);
  if (!aChannel) {
    return NS_ERROR_NULL_POINTER;
  }

  // used during the manipulation of the String from the aHeadersData
  nsAutoCString headersString;
  nsAutoCString oneHeader;
  nsAutoCString headerName;
  nsAutoCString headerValue;
  int32_t crlf = 0;
  int32_t colon = 0;

  // Turn the char * buffer into an nsString.
  headersString = aHeadersData;

  // Iterate over the nsString: for each "\r\n" delimited chunk,
  // add the value as a header to the nsIHTTPChannel
  while (true) {
    crlf = headersString.Find("\r\n", true);
    if (-1 == crlf) {
      rv = NS_OK;
      return rv;
    }
    headersString.Mid(oneHeader, 0, crlf);
    headersString.Cut(0, crlf + 2);
    oneHeader.StripWhitespace();
    colon = oneHeader.Find(":");
    if (-1 == colon) {
      rv = NS_ERROR_NULL_POINTER;
      return rv;
    }
    oneHeader.Left(headerName, colon);
    colon++;
    oneHeader.Mid(headerValue, colon, oneHeader.Length() - colon);

    // FINALLY: we can set the header!

    rv = aChannel->SetRequestHeader(headerName, headerValue, true);
    if (NS_FAILED(rv)) {
      rv = NS_ERROR_NULL_POINTER;
      return rv;
    }
  }
}

nsresult nsPluginHost::StopPluginInstance(nsNPAPIPluginInstance* aInstance) {
  return NS_ERROR_FAILURE;
}

nsresult nsPluginHost::NewPluginStreamListener(
    nsIURI* aURI, nsNPAPIPluginInstance* aInstance,
    nsIStreamListener** aStreamListener) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginHost::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* someData) {
  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic)) {
    UnloadPlugins();
  }
  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    mPluginsDisabled = Preferences::GetBool("plugin.disable", false);
    // Unload or load plugins as needed
    if (mPluginsDisabled) {
      UnloadPlugins();
    } else {
      LoadPlugins();
    }
  }
  if (XRE_IsParentProcess() && !strcmp("plugin-blocklist-updated", aTopic)) {
    // The blocklist has updated. Asynchronously get blocklist state for all
    // items. The promise resolution handler takes care of checking if anything
    // changed, and writing an updated state to file, as well as sending data to
    // child processes.
    nsPluginTag* plugin = mPlugins;
    while (plugin) {
      UpdatePluginBlocklistState(plugin);
      plugin = plugin->mNext;
    }
  }
  return NS_OK;
}

nsresult nsPluginHost::ParsePostBufferToFixHeaders(const char* inPostData,
                                                   uint32_t inPostDataLen,
                                                   char** outPostData,
                                                   uint32_t* outPostDataLen) {
  if (!inPostData || !outPostData || !outPostDataLen)
    return NS_ERROR_NULL_POINTER;

  *outPostData = 0;
  *outPostDataLen = 0;

  const char CR = '\r';
  const char LF = '\n';
  const char CRLFCRLF[] = {CR, LF, CR, LF, '\0'};  // C string"\r\n\r\n"
  const char ContentLenHeader[] = "Content-length";

  AutoTArray<const char*, 8> singleLF;
  const char* pSCntlh =
      0;                 // pointer to start of ContentLenHeader in inPostData
  const char* pSod = 0;  // pointer to start of data in inPostData
  const char* pEoh = 0;  // pointer to end of headers in inPostData
  const char* pEod =
      inPostData + inPostDataLen;  // pointer to end of inPostData
  if (*inPostData == LF) {
    // If no custom headers are required, simply add a blank
    // line ('\n') to the beginning of the file or buffer.
    // so *inPostData == '\n' is valid
    pSod = inPostData + 1;
  } else {
    const char* s = inPostData;  // tmp pointer to sourse inPostData
    while (s < pEod) {
      if (!pSCntlh && (*s == 'C' || *s == 'c') &&
          (s + sizeof(ContentLenHeader) - 1 < pEod) &&
          (!PL_strncasecmp(s, ContentLenHeader,
                           sizeof(ContentLenHeader) - 1))) {
        // lets assume this is ContentLenHeader for now
        const char* p = pSCntlh = s;
        p += sizeof(ContentLenHeader) - 1;
        // search for first CR or LF == end of ContentLenHeader
        for (; p < pEod; p++) {
          if (*p == CR || *p == LF) {
            // got delimiter,
            // one more check; if previous char is a digit
            // most likely pSCntlh points to the start of ContentLenHeader
            if (*(p - 1) >= '0' && *(p - 1) <= '9') {
              s = p;
            }
            break;  // for loop
          }
        }
        if (pSCntlh == s) {  // curret ptr is the same
          pSCntlh = 0;       // that was not ContentLenHeader
          break;  // there is nothing to parse, break *WHILE LOOP* here
        }
      }

      if (*s == CR) {
        if (pSCntlh &&  // only if ContentLenHeader is found we are looking for
                        // end of headers
            ((s + sizeof(CRLFCRLF) - 1) <= pEod) &&
            !memcmp(s, CRLFCRLF, sizeof(CRLFCRLF) - 1)) {
          s += sizeof(CRLFCRLF) - 1;
          pEoh = pSod = s;  // data stars here
          break;
        }
      } else if (*s == LF) {
        if (*(s - 1) != CR) {
          singleLF.AppendElement(s);
        }
        if (pSCntlh && (s + 1 < pEod) && (*(s + 1) == LF)) {
          s++;
          singleLF.AppendElement(s);
          s++;
          pEoh = pSod = s;  // data stars here
          break;
        }
      }
      s++;
    }
  }

  // deal with output buffer
  if (!pSod) {  // lets assume whole buffer is a data
    pSod = inPostData;
  }

  uint32_t newBufferLen = 0;
  uint32_t dataLen = pEod - pSod;
  uint32_t headersLen = pEoh ? pSod - inPostData : 0;

  char* p;           // tmp ptr into new output buf
  if (headersLen) {  // we got a headers
    // this function does not make any assumption on correctness
    // of ContentLenHeader value in this case.

    newBufferLen = dataLen + headersLen;
    // in case there were single LFs in headers
    // reserve an extra space for CR will be added before each single LF
    int cntSingleLF = singleLF.Length();
    newBufferLen += cntSingleLF;

    *outPostData = p = (char*)moz_xmalloc(newBufferLen);

    // deal with single LF
    const char* s = inPostData;
    if (cntSingleLF) {
      for (int i = 0; i < cntSingleLF; i++) {
        const char* plf = singleLF.ElementAt(i);  // ptr to single LF in headers
        int n = plf - s;                          // bytes to copy
        if (n) {  // for '\n\n' there is nothing to memcpy
          memcpy(p, s, n);
          p += n;
        }
        *p++ = CR;
        s = plf;
        *p++ = *s++;
      }
    }
    // are we done with headers?
    headersLen = pEoh - s;
    if (headersLen) {            // not yet
      memcpy(p, s, headersLen);  // copy the rest
      p += headersLen;
    }
  } else if (dataLen) {  // no ContentLenHeader is found but there is a data
    // make new output buffer big enough
    // to keep ContentLenHeader+value followed by data
    uint32_t l = sizeof(ContentLenHeader) + sizeof(CRLFCRLF) + 32;
    newBufferLen = dataLen + l;
    *outPostData = p = (char*)moz_xmalloc(newBufferLen);
    headersLen =
        snprintf(p, l, "%s: %u%s", ContentLenHeader, dataLen, CRLFCRLF);
    if (headersLen ==
        l) {  // if snprintf has ate all extra space consider this as an error
      free(p);
      *outPostData = 0;
      return NS_ERROR_FAILURE;
    }
    p += headersLen;
    newBufferLen = headersLen + dataLen;
  }
  // at this point we've done with headers.
  // there is a possibility that input buffer has only headers info in it
  // which already parsed and copied into output buffer.
  // copy the data
  if (dataLen) {
    memcpy(p, pSod, dataLen);
  }

  *outPostDataLen = newBufferLen;

  return NS_OK;
}

nsresult nsPluginHost::GetPluginName(nsNPAPIPluginInstance* aPluginInstance,
                                     const char** aPluginName) {
  return NS_ERROR_FAILURE;
}

nsresult nsPluginHost::GetPluginTagForInstance(
    nsNPAPIPluginInstance* aPluginInstance, nsIPluginTag** aPluginTag) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginHost::Notify(nsITimer* timer) {
  RefPtr<nsPluginTag> pluginTag = mPlugins;
  while (pluginTag) {
    if (pluginTag->mUnloadTimer == timer) {
      if (!IsRunningPlugin(pluginTag)) {
        pluginTag->TryUnloadPlugin(false);
      }
      return NS_OK;
    }
    pluginTag = pluginTag->mNext;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPluginHost::GetName(nsACString& aName) {
  aName.AssignLiteral("nsPluginHost");
  return NS_OK;
}

#ifdef XP_WIN
// Re-enable any top level browser windows that were disabled by modal dialogs
// displayed by the crashed plugin.
static void CheckForDisabledWindows() {
  nsCOMPtr<nsIWindowMediator> wm(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!wm) return;

  nsCOMPtr<nsISimpleEnumerator> windowList;
  wm->GetAppWindowEnumerator(nullptr, getter_AddRefs(windowList));
  if (!windowList) return;

  bool haveWindows;
  do {
    windowList->HasMoreElements(&haveWindows);
    if (!haveWindows) return;

    nsCOMPtr<nsISupports> supportsWindow;
    windowList->GetNext(getter_AddRefs(supportsWindow));
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(supportsWindow));
    if (baseWin) {
      nsCOMPtr<nsIWidget> widget;
      baseWin->GetMainWidget(getter_AddRefs(widget));
      if (widget && !widget->GetParent() && widget->IsVisible() &&
          !widget->IsEnabled()) {
        nsIWidget* child = widget->GetFirstChild();
        bool enable = true;
        while (child) {
          if (child->WindowType() == eWindowType_dialog) {
            enable = false;
            break;
          }
          child = child->GetNextSibling();
        }
        if (enable) {
          widget->Enable(true);
        }
      }
    }
  } while (haveWindows);
}
#endif

nsNPAPIPluginInstance* nsPluginHost::FindInstance(const char* mimetype) {
  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance* instance = mInstances[i];

    const char* mt;
    nsresult rv = instance->GetMIMEType(&mt);
    if (NS_FAILED(rv)) continue;

    if (PL_strcasecmp(mt, mimetype) == 0) return instance;
  }

  return nullptr;
}

nsNPAPIPluginInstance* nsPluginHost::FindOldestStoppedInstance() {
  nsNPAPIPluginInstance* oldestInstance = nullptr;
  TimeStamp oldestTime = TimeStamp::Now();
  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance* instance = mInstances[i];
    if (instance->IsRunning()) continue;

    TimeStamp time = instance->StopTime();
    if (time < oldestTime) {
      oldestTime = time;
      oldestInstance = instance;
    }
  }

  return oldestInstance;
}

uint32_t nsPluginHost::StoppedInstanceCount() {
  uint32_t stoppedCount = 0;
  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance* instance = mInstances[i];
    if (!instance->IsRunning()) stoppedCount++;
  }
  return stoppedCount;
}

nsTArray<RefPtr<nsNPAPIPluginInstance>>* nsPluginHost::InstanceArray() {
  return &mInstances;
}

void nsPluginHost::DestroyRunningInstances(nsPluginTag* aPluginTag) {}

/* static */
bool nsPluginHost::CanUsePluginForMIMEType(const nsACString& aMIMEType) {
  // We only support flash as a plugin, so if the mime types don't match for
  // those, exit before we start loading plugins.
  //
  // XXX: Remove test/java cases when bug 1351885 lands.
  if (nsPluginHost::GetSpecialType(aMIMEType) ==
          nsPluginHost::eSpecialType_Flash ||
      MimeTypeIsAllowedForFakePlugin(NS_ConvertUTF8toUTF16(aMIMEType)) ||
      aMIMEType.LowerCaseEqualsLiteral("application/x-test")) {
    return true;
  }

  return false;
}

// Runnable that does an async destroy of a plugin.

class nsPluginDestroyRunnable
    : public Runnable,
      public mozilla::LinkedListElement<nsPluginDestroyRunnable> {
 public:
  explicit nsPluginDestroyRunnable(nsNPAPIPluginInstance* aInstance)
      : Runnable("nsPluginDestroyRunnable"), mInstance(aInstance) {
    sRunnableList.insertBack(this);
  }

  ~nsPluginDestroyRunnable() override { this->remove(); }

  NS_IMETHOD Run() override {
    RefPtr<nsNPAPIPluginInstance> instance;

    // Null out mInstance to make sure this code in another runnable
    // will do the right thing even if someone was holding on to this
    // runnable longer than we expect.
    instance.swap(mInstance);

    if (PluginDestructionGuard::DelayDestroy(instance)) {
      // It's still not safe to destroy the plugin, it's now up to the
      // outermost guard on the stack to take care of the destruction.
      return NS_OK;
    }

    for (auto r : sRunnableList) {
      if (r != this && r->mInstance == instance) {
        // There's another runnable scheduled to tear down
        // instance. Let it do the job.
        return NS_OK;
      }
    }

    PLUGIN_LOG(PLUGIN_LOG_NORMAL,
               ("Doing delayed destroy of instance %p\n", instance.get()));

    RefPtr<nsPluginHost> host = nsPluginHost::GetInst();
    if (host) host->StopPluginInstance(instance);

    PLUGIN_LOG(PLUGIN_LOG_NORMAL,
               ("Done with delayed destroy of instance %p\n", instance.get()));

    return NS_OK;
  }

 protected:
  RefPtr<nsNPAPIPluginInstance> mInstance;

  static mozilla::LinkedList<nsPluginDestroyRunnable> sRunnableList;
};

mozilla::LinkedList<nsPluginDestroyRunnable>
    nsPluginDestroyRunnable::sRunnableList;

mozilla::LinkedList<PluginDestructionGuard> PluginDestructionGuard::sList;

PluginDestructionGuard::PluginDestructionGuard(nsNPAPIPluginInstance* aInstance)
    : mInstance(aInstance) {
  Init();
}

PluginDestructionGuard::PluginDestructionGuard(NPP npp)
    : mInstance(npp ? static_cast<nsNPAPIPluginInstance*>(npp->ndata)
                    : nullptr) {
  Init();
}

PluginDestructionGuard::~PluginDestructionGuard() {
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread");

  this->remove();

  if (mDelayedDestroy) {
    // We've attempted to destroy the plugin instance we're holding on
    // to while we were guarding it. Do the actual destroy now, off of
    // a runnable.
    RefPtr<nsPluginDestroyRunnable> evt =
        new nsPluginDestroyRunnable(mInstance);

    NS_DispatchToMainThread(evt);
  }
}

// static
bool PluginDestructionGuard::DelayDestroy(nsNPAPIPluginInstance* aInstance) {
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread");
  NS_ASSERTION(aInstance, "Uh, I need an instance!");

  // Find the first guard on the stack and make it do a delayed
  // destroy upon destruction.

  for (auto g : sList) {
    if (g->mInstance == aInstance) {
      g->mDelayedDestroy = true;

      return true;
    }
  }

  return false;
}

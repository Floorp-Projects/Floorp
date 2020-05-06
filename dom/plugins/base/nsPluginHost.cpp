/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsPluginHost.cpp - top-level plugin management code */

#include "nscore.h"
#include "nsPluginHost.h"

#include <cstdlib>
#include <stdio.h>
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsNPAPIPluginInstance.h"
#include "nsPluginInstanceOwner.h"
#include "nsObjectLoadingContent.h"
#include "nsIEventTarget.h"
#include "nsIObserverService.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIHttpChannel.h"
#include "nsIUploadChannel.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include "nsIFile.h"
#include "nsISeekableStream.h"
#include "nsNetUtil.h"
#include "nsISimpleEnumerator.h"
#include "nsIStringStream.h"
#include "mozilla/dom/Document.h"
#include "nsPluginLogging.h"
#include "nsIScriptChannel.h"
#include "nsIBlocklistService.h"
#include "nsVersionComparator.h"
#include "nsIObjectLoadingContent.h"
#include "nsIWritablePropertyBag2.h"
#include "nsICategoryManager.h"
#include "nsPluginStreamListenerPeer.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FakePluginTagInitBinding.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/plugins/PluginBridge.h"
#include "mozilla/plugins/PluginTypes.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/ipc/URIUtils.h"

#include "nsEnumeratorUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"

#include "nsXULAppAPI.h"

// for the dialog

#include "nsNetCID.h"
#include "mozilla/Sprintf.h"
#include "nsThreadUtils.h"
#include "nsQueryObject.h"

#include "nsIWeakReferenceUtils.h"
#include "nsPluginNativeWindow.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "nsIImageLoadingContent.h"
#include "nsVersionComparator.h"
#include "ReferrerInfo.h"

#include "mozilla/dom/Promise.h"

#include "PluginFinder.h"

#if defined(XP_WIN)
#  include "nsIWindowMediator.h"
#  include "nsIBaseWindow.h"
#  include "windows.h"
#  include "winbase.h"
#endif
#if (MOZ_WIDGET_GTK)
#  include <gdk/gdk.h>
#  include <gdk/gdkx.h>
#endif

#include "npapi.h"

using namespace mozilla;
using mozilla::TimeStamp;
using mozilla::dom::Document;
using mozilla::dom::FakePluginMimeEntry;
using mozilla::dom::FakePluginTagInit;
using mozilla::dom::Promise;
using mozilla::plugins::FakePluginTag;
using mozilla::plugins::PluginTag;

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
        // XXXgijs This will wipe invalid plugin info. That's unfortunate, but
        // there's no easy way around this - re-running FindPlugins will only
        // keep invalid plugin info around long enough to use it if we force
        // it to re-create plugin tags, and in any case can cause re-entrancy
        // because it would do more async blocklist lookups. Just make sure
        // we write the blocklist info.
        PluginFinder::WritePluginInfo(host->mFlashOnly, host->mPlugins);

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
      mFlashOnly(true),
      mDoReloadOnceFindingFinished(false),
      mAddedFinderShutdownBlocker(false),
      mPluginEpoch(0) {
  // check to see if pref is set at startup to let plugins take over in
  // full page mode for certain image mime types that we handle internally
  mOverrideInternalTypes =
      Preferences::GetBool("plugin.override_internal_types", false);
  if (xpc::IsInAutomation()) {
    mFlashOnly = Preferences::GetBool("plugin.load_flash_only", true);
  }

  bool waylandBackend = false;
#if MOZ_WIDGET_GTK
  GdkDisplay* display = gdk_display_get_default();
  if (display) {
    waylandBackend = !GDK_IS_X11_DISPLAY(display);
  }
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

bool nsPluginHost::IsRunningPlugin(nsPluginTag* aPluginTag) {
  if (!aPluginTag || !aPluginTag->mPlugin) {
    return false;
  }

  if (aPluginTag->mContentProcessRunningCount) {
    return true;
  }

  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance* instance = mInstances[i].get();
    if (instance && instance->GetPlugin() == aPluginTag->mPlugin &&
        instance->IsRunning()) {
      return true;
    }
  }

  return false;
}

nsresult nsPluginHost::ReloadPlugins() {
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHost::ReloadPlugins Begin\n"));

  // If we're calling this from a content process, forward the reload request to
  // the parent process. If plugins actually changed, it will notify us
  // asynchronously later.
  if (XRE_IsContentProcess()) {
    Unused
        << mozilla::dom::ContentChild::GetSingleton()->SendMaybeReloadPlugins();
    // In content processes, always signal that plugins have not changed. We
    // will never know if they changed here unless we make slow synchronous
    // calls. This information will hopefully only be wrong once, as if there
    // has been a plugin update, we expect to have gotten notification from the
    // parent process and everything should be updated by the next time this is
    // called. See Bug 1337058 for more info.
    return NS_ERROR_PLUGINS_PLUGINSNOTCHANGED;
  }
  // this will create the initial plugin list out of cache
  // if it was not created yet
  if (!mPluginsLoaded) return LoadPlugins();

  // We're already in the process of finding more plugins. Do it again once
  // done (because maybe things have changed since we started looking).
  if (mPendingFinder) {
    mDoReloadOnceFindingFinished = true;
    return NS_ERROR_PLUGINS_PLUGINSNOTCHANGED;
  }

  // we are re-scanning plugins. New plugins may have been added, also some
  // plugins may have been removed, so we should probably shut everything down
  // but don't touch running (active and not stopped) plugins

  // check if plugins changed, no need to do anything else
  // if no changes to plugins have been made
  // We're doing this on the main thread, and we checked mPendingFinder
  // above, so we don't need to do anything else to ensure we don't re-enter.
  // Future work may make this asynchronous and do the finding away from
  // the mainthread for the reload case, too, (in which case we should use
  // mPendingFinder instead of a local copy, and update PluginFinder) but at
  // the moment this is less important than the initial finding on startup.
  RefPtr<PluginFinder> pf = new PluginFinder(mFlashOnly);
  bool pluginschanged;
  MOZ_TRY(pf->HavePluginsChanged([&pluginschanged](bool aPluginsChanged) {
    pluginschanged = aPluginsChanged;
  }));
  pf->Run();

  // if no changed detected, return an appropriate error code
  if (!pluginschanged) return NS_ERROR_PLUGINS_PLUGINSNOTCHANGED;

  return ActuallyReloadPlugins();
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

nsresult nsPluginHost::GetURL(nsISupports* pluginInst, const char* url,
                              const char* target,
                              nsNPAPIPluginStreamListener* streamListener,
                              const char* altHost, const char* referrer,
                              bool forceJSEnabled) {
  return GetURLWithHeaders(static_cast<nsNPAPIPluginInstance*>(pluginInst), url,
                           target, streamListener, altHost, referrer,
                           forceJSEnabled, 0, nullptr);
}

nsresult nsPluginHost::GetURLWithHeaders(
    nsNPAPIPluginInstance* pluginInst, const char* url, const char* target,
    nsNPAPIPluginStreamListener* streamListener, const char* altHost,
    const char* referrer, bool forceJSEnabled, uint32_t getHeadersLength,
    const char* getHeaders) {
  // we can only send a stream back to the plugin (as specified by a
  // null target) if we also have a nsNPAPIPluginStreamListener to talk to
  if (!target && !streamListener) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult rv = NS_OK;

  if (target) {
    RefPtr<nsPluginInstanceOwner> owner = pluginInst->GetOwner();
    if (owner) {
      rv = owner->GetURL(url, target, nullptr, nullptr, 0, true);
    }
  }

  if (streamListener) {
    rv = NewPluginURLStream(NS_ConvertUTF8toUTF16(url), pluginInst,
                            streamListener, nullptr, getHeaders,
                            getHeadersLength);
  }
  return rv;
}

nsresult nsPluginHost::PostURL(nsISupports* pluginInst, const char* url,
                               uint32_t postDataLen, const char* postData,
                               const char* target,
                               nsNPAPIPluginStreamListener* streamListener,
                               const char* altHost, const char* referrer,
                               bool forceJSEnabled, uint32_t postHeadersLength,
                               const char* postHeaders) {
  nsresult rv;

  // we can only send a stream back to the plugin (as specified
  // by a null target) if we also have a nsNPAPIPluginStreamListener
  // to talk to also
  if (!target && !streamListener) return NS_ERROR_ILLEGAL_VALUE;

  nsNPAPIPluginInstance* instance =
      static_cast<nsNPAPIPluginInstance*>(pluginInst);

  nsCOMPtr<nsIInputStream> postStream;
  char* dataToPost;
  uint32_t newDataToPostLen;
  ParsePostBufferToFixHeaders(postData, postDataLen, &dataToPost,
                              &newDataToPostLen);
  if (!dataToPost) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIStringInputStream> sis =
      do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  if (!sis) {
    free(dataToPost);
    return rv;
  }

  // data allocated by ParsePostBufferToFixHeaders() is managed and
  // freed by the string stream.
  postDataLen = newDataToPostLen;
  sis->AdoptData(dataToPost, postDataLen);
  postStream = sis;

  if (target) {
    RefPtr<nsPluginInstanceOwner> owner = instance->GetOwner();
    if (owner) {
      rv = owner->GetURL(url, target, postStream, (void*)postHeaders,
                         postHeadersLength, true);
    }
  }

  // if we don't have a target, just create a stream.
  if (streamListener) {
    rv =
        NewPluginURLStream(NS_ConvertUTF8toUTF16(url), instance, streamListener,
                           postStream, postHeaders, postHeadersLength);
  }
  return rv;
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
  mSerializablePlugins.Clear();
  mSerializableFakePlugins.Clear();

  mPluginsLoaded = false;

  return NS_OK;
}

void nsPluginHost::OnPluginInstanceDestroyed(nsPluginTag* aPluginTag) {
  bool hasInstance = false;
  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    if (TagForPlugin(mInstances[i]->GetPlugin()) == aPluginTag) {
      hasInstance = true;
      break;
    }
  }

  // We have some options for unloading plugins if they have no instances.
  //
  // Unloading plugins immediately can be bad - some plugins retain state
  // between instances even when there are none. This is largely limited to
  // going from one page to another, so state is retained without an instance
  // for only a very short period of time. In order to allow this to work
  // we don't unload plugins immediately by default. This is supported
  // via a hidden user pref though.
  //
  // Another reason not to unload immediately is that loading is expensive,
  // and it is better to leave popular plugins loaded.
  //
  // Our default behavior is to try to unload a plugin after a pref-controlled
  // delay once its last instance is destroyed. This seems like a reasonable
  // compromise that allows us to reclaim memory while allowing short state
  // retention and avoid perf hits for loading popular plugins.
  if (!hasInstance) {
    if (UnloadPluginsASAP()) {
      aPluginTag->TryUnloadPlugin(false);
    } else {
      if (aPluginTag->mUnloadTimer) {
        aPluginTag->mUnloadTimer->Cancel();
      } else {
        aPluginTag->mUnloadTimer = NS_NewTimer();
      }
      uint32_t unloadTimeout = StaticPrefs::dom_ipc_plugins_unloadTimeoutSecs();
      aPluginTag->mUnloadTimer->InitWithCallback(this, 1000 * unloadTimeout,
                                                 nsITimer::TYPE_ONE_SHOT);
    }
  }
}

nsresult nsPluginHost::InstantiatePluginInstance(
    const nsACString& aMimeType, nsIURI* aURL, nsObjectLoadingContent* aContent,
    nsPluginInstanceOwner** aOwner) {
  NS_ENSURE_ARG_POINTER(aOwner);

#ifdef PLUGIN_LOGGING
  nsAutoCString urlSpec;
  if (aURL) aURL->GetAsciiSpec(urlSpec);

  MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
          ("nsPluginHost::InstantiatePlugin Begin mime=%s, url=%s\n",
           PromiseFlatCString(aMimeType).get(), urlSpec.get()));

  PR_LogFlush();
#endif

  if (aMimeType.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE("Attempting to spawn a plugin with no mime type");
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsPluginInstanceOwner> instanceOwner = new nsPluginInstanceOwner();
  if (!instanceOwner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIContent> ourContent =
      do_QueryInterface(static_cast<nsIImageLoadingContent*>(aContent));
  nsresult rv = instanceOwner->Init(ourContent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsPluginTagType tagType;
  rv = instanceOwner->GetTagType(&tagType);
  if (NS_FAILED(rv)) {
    instanceOwner->Destroy();
    return rv;
  }

  if (tagType != nsPluginTagType_Embed && tagType != nsPluginTagType_Object) {
    instanceOwner->Destroy();
    return NS_ERROR_FAILURE;
  }

  rv = SetUpPluginInstance(aMimeType, aURL, instanceOwner);
  if (NS_FAILED(rv)) {
    instanceOwner->Destroy();
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsNPAPIPluginInstance> instance = instanceOwner->GetInstance();

  if (instance) {
    CreateWidget(instanceOwner);
  }

  // At this point we consider instantiation to be successful. Do not return an
  // error.
  instanceOwner.forget(aOwner);

#ifdef PLUGIN_LOGGING
  nsAutoCString urlSpec2;
  if (aURL != nullptr) aURL->GetAsciiSpec(urlSpec2);

  MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
          ("nsPluginHost::InstantiatePlugin Finished mime=%s, rv=%" PRIu32
           ", url=%s\n",
           PromiseFlatCString(aMimeType).get(), static_cast<uint32_t>(rv),
           urlSpec2.get()));

  PR_LogFlush();
#endif

  return NS_OK;
}

nsPluginTag* nsPluginHost::FindTagForLibrary(PRLibrary* aLibrary) {
  nsPluginTag* pluginTag;
  for (pluginTag = mPlugins; pluginTag; pluginTag = pluginTag->mNext) {
    if (pluginTag->mLibrary == aLibrary) {
      return pluginTag;
    }
  }
  return nullptr;
}

nsPluginTag* nsPluginHost::TagForPlugin(nsNPAPIPlugin* aPlugin) {
  nsPluginTag* pluginTag;
  for (pluginTag = mPlugins; pluginTag; pluginTag = pluginTag->mNext) {
    if (pluginTag->mPlugin == aPlugin) {
      return pluginTag;
    }
  }
  // a plugin should never exist without a corresponding tag
  NS_ERROR("TagForPlugin has failed");
  return nullptr;
}

nsresult nsPluginHost::SetUpPluginInstance(const nsACString& aMimeType,
                                           nsIURI* aURL,
                                           nsPluginInstanceOwner* aOwner) {
  NS_ENSURE_ARG_POINTER(aOwner);

  nsresult rv = TrySetUpPluginInstance(aMimeType, aURL, aOwner);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  // If we failed to load a plugin instance we'll try again after
  // reloading our plugin list. Only do that once per document to
  // avoid redundant high resource usage on pages with multiple
  // unkown instance types. We'll do that by caching the document.
  nsCOMPtr<Document> document;
  aOwner->GetDocument(getter_AddRefs(document));

  nsCOMPtr<Document> currentdocument = do_QueryReferent(mCurrentDocument);
  if (document == currentdocument) {
    return rv;
  }

  mCurrentDocument = do_GetWeakReference(document);

  // Don't try to set up an instance again if nothing changed.
  if (ReloadPlugins() == NS_ERROR_PLUGINS_PLUGINSNOTCHANGED) {
    return rv;
  }

  return TrySetUpPluginInstance(aMimeType, aURL, aOwner);
}

nsresult nsPluginHost::TrySetUpPluginInstance(const nsACString& aMimeType,
                                              nsIURI* aURL,
                                              nsPluginInstanceOwner* aOwner) {
#ifdef PLUGIN_LOGGING
  MOZ_LOG(
      nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
      ("nsPluginHost::TrySetupPluginInstance Begin mime=%s, owner=%p, url=%s\n",
       PromiseFlatCString(aMimeType).get(), aOwner,
       aURL ? aURL->GetSpecOrDefault().get() : ""));

  PR_LogFlush();
#endif

#ifdef XP_WIN
  bool changed;
  if ((mRegKeyHKLM && NS_SUCCEEDED(mRegKeyHKLM->HasChanged(&changed)) &&
       changed) ||
      (mRegKeyHKCU && NS_SUCCEEDED(mRegKeyHKCU->HasChanged(&changed)) &&
       changed)) {
    ReloadPlugins();
  }
#endif

  RefPtr<nsNPAPIPlugin> plugin;
  GetPlugin(aMimeType, getter_AddRefs(plugin));
  if (!plugin) {
    return NS_ERROR_FAILURE;
  }

  nsPluginTag* pluginTag = FindNativePluginForType(aMimeType, true);

  NS_ASSERTION(pluginTag, "Must have plugin tag here!");

  plugin->GetLibrary()->SetHasLocalInstance();

  RefPtr<nsNPAPIPluginInstance> instance = new nsNPAPIPluginInstance();

  // This will create the owning reference. The connection must be made between
  // the instance and the instance owner before initialization. Plugins can call
  // into the browser during initialization.
  aOwner->SetInstance(instance.get());

  // Add the instance to the instances list before we call NPP_New so that
  // it is "in play" before NPP_New happens. Take it out if NPP_New fails.
  mInstances.AppendElement(instance.get());

  // this should not addref the instance or owner
  // except in some cases not Java, see bug 140931
  // our COM pointer will free the peer
  nsresult rv = instance->Initialize(plugin.get(), aOwner, aMimeType);
  if (NS_FAILED(rv)) {
    mInstances.RemoveElement(instance.get());
    aOwner->SetInstance(nullptr);
    return rv;
  }

  // Cancel the plugin unload timer since we are creating
  // an instance for it.
  if (pluginTag->mUnloadTimer) {
    pluginTag->mUnloadTimer->Cancel();
  }

#ifdef PLUGIN_LOGGING
  MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_BASIC,
          ("nsPluginHost::TrySetupPluginInstance Finished mime=%s, rv=%" PRIu32
           ", owner=%p, url=%s\n",
           PromiseFlatCString(aMimeType).get(), static_cast<uint32_t>(rv),
           aOwner, aURL ? aURL->GetSpecOrDefault().get() : ""));

  PR_LogFlush();
#endif

  return rv;
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

static nsresult CreateNPAPIPlugin(nsPluginTag* aPluginTag,
                                  nsNPAPIPlugin** aOutNPAPIPlugin) {
  nsresult rv;
  rv = nsNPAPIPlugin::CreatePlugin(aPluginTag, aOutNPAPIPlugin);

  return rv;
}

nsresult nsPluginHost::EnsurePluginLoaded(nsPluginTag* aPluginTag) {
  RefPtr<nsNPAPIPlugin> plugin = aPluginTag->mPlugin;
  if (!plugin) {
    nsresult rv = CreateNPAPIPlugin(aPluginTag, getter_AddRefs(plugin));
    if (NS_FAILED(rv)) {
      return rv;
    }
    aPluginTag->mPlugin = plugin;
  }
  return NS_OK;
}

nsresult nsPluginHost::GetPluginForContentProcess(uint32_t aPluginId,
                                                  nsNPAPIPlugin** aPlugin) {
  AUTO_PROFILER_LABEL("nsPluginHost::GetPluginForContentProcess", OTHER);
  MOZ_ASSERT(XRE_IsParentProcess());

  // If plugins haven't been scanned yet, do so now
  LoadPlugins();

  nsPluginTag* pluginTag = PluginWithId(aPluginId);
  if (pluginTag) {
    // When setting up a bridge, double check with chrome to see if this plugin
    // is blocked hard. Note this does not protect against vulnerable plugins
    // that the user has explicitly allowed. :(
    if (pluginTag->IsBlocklisted()) {
      return NS_ERROR_PLUGIN_BLOCKLISTED;
    }

    nsresult rv = EnsurePluginLoaded(pluginTag);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // We only get here if a content process doesn't have a PluginModuleParent
    // for the given plugin already. Therefore, this counter is counting the
    // number of outstanding PluginModuleParents for the plugin, excluding the
    // one from the chrome process.
    pluginTag->mContentProcessRunningCount++;
    NS_ADDREF(*aPlugin = pluginTag->mPlugin);
    return NS_OK;
  }

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

nsresult nsPluginHost::GetPlugin(const nsACString& aMimeType,
                                 nsNPAPIPlugin** aPlugin) {
  nsresult rv = NS_ERROR_FAILURE;
  *aPlugin = nullptr;

  // If plugins haven't been scanned yet, do so now
  LoadPlugins();

  nsPluginTag* pluginTag = FindNativePluginForType(aMimeType, true);
  if (pluginTag) {
    rv = NS_OK;
    PLUGIN_LOG(
        PLUGIN_LOG_BASIC,
        ("nsPluginHost::GetPlugin Begin mime=%s, plugin=%s\n",
         PromiseFlatCString(aMimeType).get(), pluginTag->FileName().get()));

#ifdef DEBUG
    if (!pluginTag->FileName().IsEmpty())
      printf("For %s found plugin %s\n", PromiseFlatCString(aMimeType).get(),
             pluginTag->FileName().get());
#endif

    rv = EnsurePluginLoaded(pluginTag);
    if (NS_FAILED(rv)) {
      return rv;
    }

    NS_ADDREF(*aPlugin = pluginTag->mPlugin);
    return NS_OK;
  }

  PLUGIN_LOG(
      PLUGIN_LOG_NORMAL,
      ("nsPluginHost::GetPlugin End mime=%s, rv=%" PRIu32
       ", plugin=%p name=%s\n",
       PromiseFlatCString(aMimeType).get(), static_cast<uint32_t>(rv), *aPlugin,
       (pluginTag ? pluginTag->FileName().get() : "(not found)")));

  return rv;
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

#define ClearDataFromSitesClosure_CID                \
  {                                                  \
    0x9fb21761, 0x2403, 0x41ad, {                    \
      0x9e, 0xfd, 0x36, 0x7e, 0xc4, 0x4f, 0xa4, 0x5e \
    }                                                \
  }

// Class to hold all the data we need need for IterateMatchesAndClear and
// ClearDataFromSites
class ClearDataFromSitesClosure : public nsIClearSiteDataCallback,
                                  public nsIGetSitesWithDataCallback {
 public:
  ClearDataFromSitesClosure(nsIPluginTag* plugin, const nsACString& domain,
                            uint64_t flags, int64_t maxAge,
                            nsCOMPtr<nsIClearSiteDataCallback> callback,
                            nsPluginHost* host)
      : domain(domain),
        callback(callback),
        tag(plugin),
        flags(flags),
        maxAge(maxAge),
        host(host) {}
  NS_DECL_ISUPPORTS

  // Callback from NPP_ClearSiteData, continue to iterate the matches and clear
  NS_IMETHOD Callback(nsresult rv) override {
    if (NS_FAILED(rv)) {
      callback->Callback(rv);
      return NS_OK;
    }
    if (!matches.Length()) {
      callback->Callback(NS_OK);
      return NS_OK;
    }

    const nsCString match(matches[0]);
    matches.RemoveElement(match);
    PluginLibrary* library =
        static_cast<nsPluginTag*>(tag)->mPlugin->GetLibrary();
    rv = library->NPP_ClearSiteData(match.get(), flags, maxAge, this);
    if (NS_FAILED(rv)) {
      callback->Callback(rv);
      return NS_OK;
    }
    return NS_OK;
  }

  // Callback from NPP_GetSitesWithData, kick the iteration off to clear the
  // data
  NS_IMETHOD SitesWithData(nsTArray<nsCString>& sites) override {
    // Enumerate the sites and build a list of matches.
    nsresult rv = host->EnumerateSiteData(domain, sites, matches, false);
    Callback(rv);
    return NS_OK;
  }

  nsCString domain;
  nsCOMPtr<nsIClearSiteDataCallback> callback;
  nsTArray<nsCString> matches;
  nsIPluginTag* tag;
  uint64_t flags;
  int64_t maxAge;
  nsPluginHost* host;
  NS_DECLARE_STATIC_IID_ACCESSOR(ClearDataFromSitesClosure_CID)
 private:
  virtual ~ClearDataFromSitesClosure() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ClearDataFromSitesClosure,
                              ClearDataFromSitesClosure_CID)

NS_IMPL_ADDREF(ClearDataFromSitesClosure)
NS_IMPL_RELEASE(ClearDataFromSitesClosure)

NS_INTERFACE_MAP_BEGIN(ClearDataFromSitesClosure)
  NS_INTERFACE_MAP_ENTRY(nsIClearSiteDataCallback)
  NS_INTERFACE_MAP_ENTRY(nsIGetSitesWithDataCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIClearSiteDataCallback)
NS_INTERFACE_MAP_END

// FIXME-jsplugins what should this do for fake plugins?
NS_IMETHODIMP
nsPluginHost::ClearSiteData(nsIPluginTag* plugin, const nsACString& domain,
                            uint64_t flags, int64_t maxAge,
                            nsIClearSiteDataCallback* callbackFunc) {
  nsCOMPtr<nsIClearSiteDataCallback> callback(callbackFunc);
  // maxAge must be either a nonnegative integer or -1.
  NS_ENSURE_ARG(maxAge >= 0 || maxAge == -1);

  // Caller may give us a tag object that is no longer live.
  if (!IsLiveTag(plugin)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsPluginTag* tag = static_cast<nsPluginTag*>(plugin);

  if (!tag->IsEnabled()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We only ensure support for clearing Flash site data for now.
  // We will also attempt to clear data for any plugin that happens
  // to be loaded already.
  if (!tag->mIsFlashPlugin && !tag->mPlugin) {
    return NS_ERROR_FAILURE;
  }

  // Make sure the plugin is loaded.
  nsresult rv = EnsurePluginLoaded(tag);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PluginLibrary* library = tag->mPlugin->GetLibrary();

  // If 'domain' is the null string, clear everything.
  if (domain.IsVoid()) {
    return library->NPP_ClearSiteData(nullptr, flags, maxAge, callback);
  }
  nsCOMPtr<nsIGetSitesWithDataCallback> closure(new ClearDataFromSitesClosure(
      plugin, domain, flags, maxAge, callback, this));
  rv = library->NPP_GetSitesWithData(closure);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

#define GetSitesClosure_CID                          \
  {                                                  \
    0x4c9268ac, 0x2fd1, 0x4f2a, {                    \
      0x9a, 0x10, 0x7a, 0x09, 0xf1, 0xb7, 0x60, 0x3a \
    }                                                \
  }

// Closure to contain the data needed to handle the callback from
// NPP_GetSitesWithData
class GetSitesClosure : public nsIGetSitesWithDataCallback {
 public:
  NS_DECL_ISUPPORTS
  GetSitesClosure(const nsACString& domain, nsPluginHost* host)
      : domain(domain),
        host(host),
        result{false},
        keepWaiting(true),
        retVal(NS_ERROR_NOT_INITIALIZED) {}

  NS_IMETHOD SitesWithData(nsTArray<nsCString>& sites) override {
    retVal = HandleGetSites(sites);
    keepWaiting = false;
    return NS_OK;
  }

  nsresult HandleGetSites(nsTArray<nsCString>& sites) {
    // If there's no data, we're done.
    if (sites.IsEmpty()) {
      result = false;
      return NS_OK;
    }

    // If 'domain' is the null string, and there's data for at least one site,
    // we're done.
    if (domain.IsVoid()) {
      result = true;
      return NS_OK;
    }

    // Enumerate the sites and determine if there's a match.
    nsTArray<nsCString> matches;
    nsresult rv = host->EnumerateSiteData(domain, sites, matches, true);
    NS_ENSURE_SUCCESS(rv, rv);

    result = !matches.IsEmpty();
    return NS_OK;
  }

  nsCString domain;
  RefPtr<nsPluginHost> host;
  bool result;
  bool keepWaiting;
  nsresult retVal;
  NS_DECLARE_STATIC_IID_ACCESSOR(GetSitesClosure_CID)
 private:
  virtual ~GetSitesClosure() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(GetSitesClosure, GetSitesClosure_CID)

NS_IMPL_ISUPPORTS(GetSitesClosure, GetSitesClosure, nsIGetSitesWithDataCallback)

// This will spin the event loop while waiting on an async
// call to GetSitesWithData
NS_IMETHODIMP
nsPluginHost::SiteHasData(nsIPluginTag* plugin, const nsACString& domain,
                          bool* result) {
  // Caller may give us a tag object that is no longer live.
  if (!IsLiveTag(plugin)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // FIXME-jsplugins audit casts
  nsPluginTag* tag = static_cast<nsPluginTag*>(plugin);

  // We only ensure support for clearing Flash site data for now.
  // We will also attempt to clear data for any plugin that happens
  // to be loaded already.
  if (!tag->mIsFlashPlugin && !tag->mPlugin) {
    return NS_ERROR_FAILURE;
  }

  // Make sure the plugin is loaded.
  nsresult rv = EnsurePluginLoaded(tag);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PluginLibrary* library = tag->mPlugin->GetLibrary();

  // Get the list of sites from the plugin
  nsCOMPtr<GetSitesClosure> closure(new GetSitesClosure(domain, this));
  rv = library->NPP_GetSitesWithData(
      nsCOMPtr<nsIGetSitesWithDataCallback>(closure));
  NS_ENSURE_SUCCESS(rv, rv);
  // Spin the event loop while we wait for the async call to GetSitesWithData
  SpinEventLoopUntil([&]() { return !closure->keepWaiting; });
  *result = closure->result;
  return closure->retVal;
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

typedef NS_NPAPIPLUGIN_CALLBACK(char*, NP_GETMIMEDESCRIPTION)(void);

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
  blocklist->GetPluginBlocklistState(aPluginTag, EmptyString(), EmptyString(),
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

#ifdef XP_WIN
static void WatchRegKey(uint32_t aRoot, nsCOMPtr<nsIWindowsRegKey>& aKey) {
  if (aKey) {
    return;
  }

  aKey = do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!aKey) {
    return;
  }
  nsresult rv = aKey->Open(
      aRoot, NS_LITERAL_STRING("Software\\MozillaPlugins"),
      nsIWindowsRegKey::ACCESS_READ | nsIWindowsRegKey::ACCESS_NOTIFY);
  if (NS_FAILED(rv)) {
    aKey = nullptr;
    return;
  }
  aKey->StartWatching(true);
}
#endif

already_AddRefed<nsIAsyncShutdownClient> GetProfileChangeTeardownPhase() {
  nsCOMPtr<nsIAsyncShutdownService> asyncShutdownSvc =
      services::GetAsyncShutdown();
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

nsresult nsPluginHost::LoadPlugins() {
  // This should only be run in the parent process. On plugin list change, we'll
  // update observers in the content process as part of SetPluginsInContent
  if (XRE_IsContentProcess()) {
    return NS_OK;
  }
  // do not do anything if it is already done
  // use ReloadPlugins() to enforce loading
  if (mPluginsLoaded) return NS_OK;

  // Uh oh, someone's forcing us to load plugins, but we're already in the
  // process of doing so. Schedule a reload for when we're done:
  if (mPendingFinder) {
    mDoReloadOnceFindingFinished = true;
    return NS_OK;
  }

  if (mPluginsDisabled) return NS_OK;

#ifdef XP_WIN
  WatchRegKey(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, mRegKeyHKLM);
  WatchRegKey(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, mRegKeyHKCU);
#endif

  // This is a runnable. The main part of its work will be done away from the
  // main thread.
  mPendingFinder = new PluginFinder(mFlashOnly);
  mDoReloadOnceFindingFinished = false;
  mAddedFinderShutdownBlocker = false;
  RefPtr<nsPluginHost> self = this;
  // Note that if we're in flash only mode, which is the default, then the
  // callback will be called twice. Once for flash (or nothing, if we're not
  // (yet) aware of flash being present), and then again after we've actually
  // looked for it on disk.
  nsresult rv = mPendingFinder->DoFullSearch(
      [self](bool aPluginsChanged, RefPtr<nsPluginTag> aPlugins,
             const nsTArray<std::pair<bool, RefPtr<nsPluginTag>>>&
                 aBlocklistRequests) {
        MOZ_ASSERT(NS_IsMainThread(),
                   "Callback should only be called on the main thread.");
        self->mPluginsLoaded = true;
        if (aPluginsChanged) {
          self->ClearNonRunningPlugins();
          while (aPlugins) {
            RefPtr<nsPluginTag> pluginTag = aPlugins;
            aPlugins = aPlugins->mNext;
            self->AddPluginTag(pluginTag);
          }
          self->IncrementChromeEpoch();
          self->BroadcastPluginsToContent();
        }

        // Do blocklist queries immediately after.
        for (auto pair : aBlocklistRequests) {
          RefPtr<nsPluginTag> pluginTag = pair.second;
          bool shouldSoftblock = pair.first;
          self->UpdatePluginBlocklistState(pluginTag, shouldSoftblock);
        }

        if (aPluginsChanged) {
          nsCOMPtr<nsIObserverService> obsService =
              mozilla::services::GetObserverService();
          if (obsService) {
            obsService->NotifyObservers(nullptr, "plugins-list-updated",
                                        nullptr);
          }
        }
      });
  // Deal with the profile not being ready yet by returning NS_OK - we'll get
  // the data later.
  if (NS_FAILED(rv)) {
    mPendingFinder = nullptr;
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return NS_OK;
    }
    return rv;
  }
  bool dispatched = false;

  // If we're only looking for flash (the default), try to do so away from
  // the main thread. Note that in this case, the callback may have been called
  // already, from the cached plugin info.
  if (mFlashOnly) {
    // First add the shutdown blocker, to avoid the potential for race
    // conditions.
    nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase =
        GetProfileChangeTeardownPhase();
    if (shutdownPhase) {
      rv =
          shutdownPhase->AddBlocker(mPendingFinder, NS_LITERAL_STRING(__FILE__),
                                    __LINE__, NS_LITERAL_STRING(""));
      mAddedFinderShutdownBlocker = NS_SUCCEEDED(rv);
    }

    nsCOMPtr<nsIEventTarget> target =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = target->Dispatch(mPendingFinder, nsIEventTarget::DISPATCH_NORMAL);
      dispatched = NS_SUCCEEDED(rv);
    }
    // If we somehow failed to dispatch, remove the shutdown blocker.
    if (mAddedFinderShutdownBlocker && !dispatched) {
      shutdownPhase->RemoveBlocker(mPendingFinder);
      mAddedFinderShutdownBlocker = false;
    }
  }
  if (!dispatched) {
    mPendingFinder->Run();
    // We're running synchronously, so just remove the pending finder now.
    mPendingFinder = nullptr;
  }

  return NS_OK;
}

void nsPluginHost::FindingFinished() {
  if (mAddedFinderShutdownBlocker) {
    nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase =
        GetProfileChangeTeardownPhase();
    shutdownPhase->RemoveBlocker(mPendingFinder);
    mAddedFinderShutdownBlocker = false;
  }
  mPendingFinder = nullptr;
  if (mDoReloadOnceFindingFinished) {
    Unused << ReloadPlugins();
  }
}

nsresult nsPluginHost::SetPluginsInContent(
    uint32_t aPluginEpoch, nsTArray<mozilla::plugins::PluginTag>& aPlugins,
    nsTArray<mozilla::plugins::FakePluginTag>& aFakePlugins) {
  MOZ_ASSERT(XRE_IsContentProcess());

  nsTArray<PluginTag> plugins;

  nsTArray<FakePluginTag> fakePlugins;

  if (aPluginEpoch != ChromeEpochForContent()) {
    // Since we know we're going to be repopulating the lists anyways,
    // clear out all old entries.
    ClearNonRunningPlugins();

    SetChromeEpochForContent(aPluginEpoch);

    for (auto tag : aPlugins) {
      // Don't add the same plugin again.
      if (nsPluginTag* existing = PluginWithId(tag.id())) {
        UpdateInMemoryPluginInfo(existing);
        existing->SetBlocklistState(tag.blocklistState());
        continue;
      }

      nsPluginTag* pluginTag = new nsPluginTag(
          tag.id(), tag.name().get(), tag.description().get(),
          tag.filename().get(),
          "",  // aFullPath
          tag.version().get(), tag.mimeTypes().Clone(),
          tag.mimeDescriptions().Clone(), tag.extensions().Clone(),
          tag.isFlashPlugin(), tag.supportsAsyncRender(),
          tag.lastModifiedTime(), tag.sandboxLevel(), tag.blocklistState());
      AddPluginTag(pluginTag);
    }

    for (const auto& tag : aFakePlugins) {
      // Don't add the same plugin again.
      for (const auto& existingTag : mFakePlugins) {
        if (existingTag->Id() == tag.id()) {
          continue;
        }
      }

      RefPtr<nsFakePluginTag> pluginTag =
          *mFakePlugins.AppendElement(new nsFakePluginTag(
              tag.id(), mozilla::ipc::DeserializeURI(tag.handlerURI()),
              tag.name().get(), tag.description().get(), tag.mimeTypes(),
              tag.mimeDescriptions(), tag.extensions(), tag.niceName(),
              tag.sandboxScript()));
      nsAutoCString disableFullPage;
      Preferences::GetCString(kPrefDisableFullPage, disableFullPage);
      for (uint32_t i = 0; i < pluginTag->MimeTypes().Length(); i++) {
        if (!IsTypeInList(pluginTag->MimeTypes()[i], disableFullPage)) {
          RegisterWithCategoryManager(pluginTag->MimeTypes()[i],
                                      ePluginRegister);
        }
      }
    }

    nsCOMPtr<nsIObserverService> obsService =
        mozilla::services::GetObserverService();
    if (obsService) {
      obsService->NotifyObservers(nullptr, "plugins-list-updated", nullptr);
    }
  }

  mPluginsLoaded = true;
  return NS_OK;
}

nsresult nsPluginHost::UpdateCachedSerializablePluginList() {
  nsTArray<nsCOMPtr<nsIInternalPluginTag>> plugins;
  GetPlugins(plugins, true);
  mSerializablePlugins.Clear();
  mSerializableFakePlugins.Clear();

  for (size_t i = 0; i < plugins.Length(); i++) {
    nsCOMPtr<nsIInternalPluginTag> basetag = plugins[i];

    nsCOMPtr<nsIFakePluginTag> faketag = do_QueryInterface(basetag);
    if (faketag) {
      /// FIXME-jsplugins - We need to add a nsIInternalPluginTag->AsNative() to
      /// avoid this hacky static cast
      nsFakePluginTag* tag = static_cast<nsFakePluginTag*>(basetag.get());
      mozilla::ipc::URIParams handlerURI;
      SerializeURI(tag->HandlerURI(), handlerURI);
      mSerializableFakePlugins.AppendElement(FakePluginTag(
          tag->Id(), handlerURI, tag->Name(), tag->Description(),
          tag->MimeTypes(), tag->MimeDescriptions(), tag->Extensions(),
          tag->GetNiceFileName(), tag->SandboxScript()));
      continue;
    }

    /// FIXME-jsplugins - We need to cleanup the various plugintag classes
    /// to be more sane and avoid this dance
    nsPluginTag* tag = static_cast<nsPluginTag*>(basetag.get());

    uint32_t blocklistState;
    if (NS_WARN_IF(NS_FAILED(tag->GetBlocklistState(&blocklistState)))) {
      return NS_ERROR_FAILURE;
    }

    mSerializablePlugins.AppendElement(PluginTag(
        tag->mId, tag->Name(), tag->Description(), tag->MimeTypes(),
        tag->MimeDescriptions(), tag->Extensions(), tag->mIsFlashPlugin,
        tag->mSupportsAsyncRender, tag->FileName(), tag->Version(),
        tag->mLastModifiedTime, tag->mSandboxLevel, blocklistState));
  }
  return NS_OK;
}

nsresult nsPluginHost::BroadcastPluginsToContent() {
  MOZ_ASSERT(XRE_IsParentProcess());

  // Load plugins so that the epoch is correct.
  nsresult rv = LoadPlugins();
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = UpdateCachedSerializablePluginList();
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t newPluginEpoch = ChromeEpoch();

  nsTArray<dom::ContentParent*> parents;
  dom::ContentParent::GetAll(parents);
  for (auto p : parents) {
    Unused << p->SendSetPluginList(newPluginEpoch, mSerializablePlugins,
                                   mSerializableFakePlugins);
  }
  return NS_OK;
}

nsresult nsPluginHost::SendPluginsToContent(dom::ContentParent* parent) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(parent);
  // Load plugins so that the epoch is correct.
  nsresult rv = LoadPlugins();
  if (NS_FAILED(rv)) {
    return rv;
  }

  Unused << parent->SendSetPluginList(ChromeEpoch(), mSerializablePlugins,
                                      mSerializableFakePlugins);
  return NS_OK;
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

  NS_NAMED_LITERAL_CSTRING(
      contractId, "@mozilla.org/content/plugin/document-loader-factory;1");

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

nsresult nsPluginHost::NewPluginURLStream(
    const nsString& aURL, nsNPAPIPluginInstance* aInstance,
    nsNPAPIPluginStreamListener* aListener, nsIInputStream* aPostStream,
    const char* aHeadersData, uint32_t aHeadersDataLen) {
  nsCOMPtr<nsIURI> url;
  nsAutoString absUrl;

  if (aURL.Length() <= 0) return NS_OK;

  // get the base URI for the plugin to create an absolute url
  // in case aURL is relative
  RefPtr<nsPluginInstanceOwner> owner = aInstance->GetOwner();
  if (owner) {
    NS_MakeAbsoluteURI(absUrl, aURL, owner->GetBaseURI());
  }

  if (absUrl.IsEmpty()) absUrl.Assign(aURL);

  nsresult rv = NS_NewURI(getter_AddRefs(url), absUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsPluginStreamListenerPeer> listenerPeer =
      new nsPluginStreamListenerPeer();
  NS_ENSURE_TRUE(listenerPeer, NS_ERROR_OUT_OF_MEMORY);

  rv = listenerPeer->Initialize(url, aInstance, aListener);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<dom::Element> element;
  nsCOMPtr<Document> doc;
  if (owner) {
    owner->GetDOMElement(getter_AddRefs(element));
    owner->GetDocument(getter_AddRefs(doc));
  }

  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

  nsCOMPtr<nsIChannel> channel;
  // @arg loadgroup:
  // do not add this internal plugin's channel on the
  // load group otherwise this channel could be canceled
  // form |nsDocShell::OnLinkClickSync| bug 166613
  rv = NS_NewChannel(
      getter_AddRefs(channel), url, element,
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
          nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL,
      nsIContentPolicy::TYPE_OBJECT_SUBREQUEST,
      nullptr,  // aPerformanceStorage
      nullptr,  // aLoadGroup
      listenerPeer,
      nsIRequest::LOAD_NORMAL | nsIChannel::LOAD_BYPASS_SERVICE_WORKER);
  NS_ENSURE_SUCCESS(rv, rv);

  if (doc) {
    // And if it's a script allow it to execute against the
    // document's script context.
    nsCOMPtr<nsIScriptChannel> scriptChannel(do_QueryInterface(channel));
    if (scriptChannel) {
      scriptChannel->SetExecutionPolicy(nsIScriptChannel::EXECUTE_NORMAL);
      // Plug-ins seem to depend on javascript: URIs running synchronously
      scriptChannel->SetExecuteAsync(false);
    }
  }

  // deal with headers and post data
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    if (!aPostStream) {
      // Only set the Referer header for GET requests because IIS throws
      // errors about malformed requests if we include it in POSTs. See
      // bug 724465.
      nsCOMPtr<nsIURI> referer;
      dom::ReferrerPolicy referrerPolicy = dom::ReferrerPolicy::_empty;

      nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(element);
      if (olc) olc->GetSrcURI(getter_AddRefs(referer));

      if (!referer) {
        if (!doc) {
          return NS_ERROR_FAILURE;
        }
        referer = doc->GetDocumentURIAsReferrer();
        referrerPolicy = doc->GetReferrerPolicy();
      }
      nsCOMPtr<nsIReferrerInfo> referrerInfo =
          new dom::ReferrerInfo(referer, referrerPolicy);
      rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aPostStream) {
      // XXX it's a bit of a hack to rewind the postdata stream
      // here but it has to be done in case the post data is
      // being reused multiple times.
      nsCOMPtr<nsISeekableStream> postDataSeekable(
          do_QueryInterface(aPostStream));
      if (postDataSeekable)
        postDataSeekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);

      nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
      NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

      uploadChannel->SetUploadStream(aPostStream, EmptyCString(), -1);
    }

    if (aHeadersData) {
      rv = AddHeadersToChannel(aHeadersData, aHeadersDataLen, httpChannel);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  rv = channel->AsyncOpen(listenerPeer);
  if (NS_SUCCEEDED(rv)) listenerPeer->TrackRequest(channel);
  return rv;
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
  AUTO_PROFILER_LABEL("nsPluginHost::StopPluginInstance", OTHER);
  if (PluginDestructionGuard::DelayDestroy(aInstance)) {
    return NS_OK;
  }

  PLUGIN_LOG(
      PLUGIN_LOG_NORMAL,
      ("nsPluginHost::StopPluginInstance called instance=%p\n", aInstance));

  if (aInstance->HasStartedDestroying()) {
    return NS_OK;
  }

  Telemetry::AutoTimer<Telemetry::PLUGIN_SHUTDOWN_MS> timer;
  aInstance->Stop();

  // if the instance does not want to be 'cached' just remove it
  bool doCache = aInstance->ShouldCache();
  if (doCache) {
    // try to get the max cached instances from a pref or use default
    uint32_t cachedInstanceLimit = Preferences::GetUint(
        NS_PREF_MAX_NUM_CACHED_INSTANCES, DEFAULT_NUMBER_OF_STOPPED_INSTANCES);
    if (StoppedInstanceCount() >= cachedInstanceLimit) {
      nsNPAPIPluginInstance* oldestInstance = FindOldestStoppedInstance();
      if (oldestInstance) {
        nsPluginTag* pluginTag = TagForPlugin(oldestInstance->GetPlugin());
        oldestInstance->Destroy();
        mInstances.RemoveElement(oldestInstance);
        // TODO: Remove this check once bug 752422 was investigated
        if (pluginTag) {
          OnPluginInstanceDestroyed(pluginTag);
        }
      }
    }
  } else {
    nsPluginTag* pluginTag = TagForPlugin(aInstance->GetPlugin());
    aInstance->Destroy();
    mInstances.RemoveElement(aInstance);
    // TODO: Remove this check once bug 752422 was investigated
    if (pluginTag) {
      OnPluginInstanceDestroyed(pluginTag);
    }
  }

  return NS_OK;
}

nsresult nsPluginHost::NewPluginStreamListener(
    nsIURI* aURI, nsNPAPIPluginInstance* aInstance,
    nsIStreamListener** aStreamListener) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aStreamListener);

  RefPtr<nsPluginStreamListenerPeer> listener =
      new nsPluginStreamListenerPeer();
  nsresult rv = listener->Initialize(aURI, aInstance, nullptr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  listener.forget(aStreamListener);

  return NS_OK;
}

void nsPluginHost::CreateWidget(nsPluginInstanceOwner* aOwner) {
  aOwner->CreateWidget();

  // If we've got a native window, the let the plugin know about it.
  aOwner->CallSetWindow();
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

nsresult nsPluginHost::NewPluginNativeWindow(
    nsPluginNativeWindow** aPluginNativeWindow) {
  return PLUG_NewPluginNativeWindow(aPluginNativeWindow);
}

nsresult nsPluginHost::GetPluginName(nsNPAPIPluginInstance* aPluginInstance,
                                     const char** aPluginName) {
  nsNPAPIPluginInstance* instance =
      static_cast<nsNPAPIPluginInstance*>(aPluginInstance);
  if (!instance) return NS_ERROR_FAILURE;

  nsNPAPIPlugin* plugin = instance->GetPlugin();
  if (!plugin) return NS_ERROR_FAILURE;

  *aPluginName = TagForPlugin(plugin)->Name().get();

  return NS_OK;
}

nsresult nsPluginHost::GetPluginTagForInstance(
    nsNPAPIPluginInstance* aPluginInstance, nsIPluginTag** aPluginTag) {
  NS_ENSURE_ARG_POINTER(aPluginInstance);
  NS_ENSURE_ARG_POINTER(aPluginTag);

  nsNPAPIPlugin* plugin = aPluginInstance->GetPlugin();
  if (!plugin) return NS_ERROR_FAILURE;

  *aPluginTag = TagForPlugin(plugin);

  NS_ADDREF(*aPluginTag);
  return NS_OK;
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

void nsPluginHost::PluginCrashed(nsNPAPIPlugin* aPlugin,
                                 const nsAString& aPluginDumpID,
                                 const nsACString& aAdditionalMinidumps) {
  nsPluginTag* crashedPluginTag = TagForPlugin(aPlugin);
  MOZ_ASSERT(crashedPluginTag);

  // Notify the app's observer that a plugin crashed so it can submit
  // a crashreport.
  bool submittedCrashReport = false;
  nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
  nsCOMPtr<nsIWritablePropertyBag2> propbag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");
  if (obsService && propbag) {
    uint32_t runID = 0;
    PluginLibrary* library = aPlugin->GetLibrary();

    if (!NS_WARN_IF(!library)) {
      library->GetRunID(&runID);
    }
    propbag->SetPropertyAsUint32(NS_LITERAL_STRING("runID"), runID);

    nsCString pluginName;
    crashedPluginTag->GetName(pluginName);
    propbag->SetPropertyAsAString(NS_LITERAL_STRING("pluginName"),
                                  NS_ConvertUTF8toUTF16(pluginName));
    propbag->SetPropertyAsAString(NS_LITERAL_STRING("pluginDumpID"),
                                  aPluginDumpID);
    propbag->SetPropertyAsACString(NS_LITERAL_STRING("additionalMinidumps"),
                                   aAdditionalMinidumps);
    propbag->SetPropertyAsBool(NS_LITERAL_STRING("submittedCrashReport"),
                               submittedCrashReport);
    obsService->NotifyObservers(propbag, "plugin-crashed", nullptr);
    // see if an observer submitted a crash report.
    propbag->GetPropertyAsBool(NS_LITERAL_STRING("submittedCrashReport"),
                               &submittedCrashReport);
  }

  // Invalidate each nsPluginInstanceTag for the crashed plugin

  for (uint32_t i = mInstances.Length(); i > 0; i--) {
    nsNPAPIPluginInstance* instance = mInstances[i - 1];
    if (instance->GetPlugin() == aPlugin) {
      // notify the content node (nsIObjectLoadingContent) that the
      // plugin has crashed
      RefPtr<dom::Element> domElement;
      instance->GetDOMElement(getter_AddRefs(domElement));
      nsCOMPtr<nsIObjectLoadingContent> objectContent(
          do_QueryInterface(domElement));
      if (objectContent) {
        objectContent->PluginCrashed(crashedPluginTag, aPluginDumpID,
                                     submittedCrashReport);
      }

      instance->Destroy();
      mInstances.RemoveElement(instance);
      OnPluginInstanceDestroyed(crashedPluginTag);
    }
  }

  // Only after all instances have been invalidated is it safe to null
  // out nsPluginTag.mPlugin. The next time we try to create an
  // instance of this plugin we reload it (launch a new plugin process).

  crashedPluginTag->mPlugin = nullptr;
  crashedPluginTag->mContentProcessRunningCount = 0;

#ifdef XP_WIN
  CheckForDisabledWindows();
#endif
}

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

void nsPluginHost::DestroyRunningInstances(nsPluginTag* aPluginTag) {
  for (int32_t i = mInstances.Length(); i > 0; i--) {
    nsNPAPIPluginInstance* instance = mInstances[i - 1];
    if (instance->IsRunning() &&
        (!aPluginTag || aPluginTag == TagForPlugin(instance->GetPlugin()))) {
      instance->SetWindow(nullptr);
      instance->Stop();

      // Get rid of all the instances without the possibility of caching.
      nsPluginTag* pluginTag = TagForPlugin(instance->GetPlugin());
      instance->SetWindow(nullptr);

      RefPtr<dom::Element> domElement;
      instance->GetDOMElement(getter_AddRefs(domElement));
      nsCOMPtr<nsIObjectLoadingContent> objectContent =
          do_QueryInterface(domElement);

      instance->Destroy();

      mInstances.RemoveElement(instance);
      OnPluginInstanceDestroyed(pluginTag);

      // Notify owning content that we destroyed its plugin out from under it
      if (objectContent) {
        objectContent->PluginDestroyed();
      }
    }
  }
}

/* static */
bool nsPluginHost::CanUsePluginForMIMEType(const nsACString& aMIMEType) {
  // We only support flash as a plugin, so if the mime types don't match for
  // those, exit before we start loading plugins.
  //
  // XXX: Remove test/java cases when bug 1351885 lands.
  if (nsPluginHost::GetSpecialType(aMIMEType) ==
          nsPluginHost::eSpecialType_Flash ||
      MimeTypeIsAllowedForFakePlugin(NS_ConvertUTF8toUTF16(aMIMEType)) ||
      aMIMEType.LowerCaseEqualsLiteral("application/x-test") ||
      aMIMEType.LowerCaseEqualsLiteral("application/x-second-test") ||
      aMIMEType.LowerCaseEqualsLiteral("application/x-third-test")) {
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

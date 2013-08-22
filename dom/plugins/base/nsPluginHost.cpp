/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsPluginHost.cpp - top-level plugin management code */

#include "nscore.h"
#include "nsPluginHost.h"

#include <cstdlib>
#include <stdio.h>
#include "prio.h"
#include "prmem.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsNPAPIPluginInstance.h"
#include "nsPluginInstanceOwner.h"
#include "nsObjectLoadingContent.h"
#include "nsIHTTPHeaderListener.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIObserverService.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIHttpChannel.h"
#include "nsIUploadChannel.h"
#include "nsIByteRangeRequest.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIURL.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include "nsIProtocolProxyService2.h"
#include "nsIStreamConverterService.h"
#include "nsIFile.h"
#if defined(XP_MACOSX)
#include "nsILocalFileMac.h"
#endif
#include "nsISeekableStream.h"
#include "nsNetUtil.h"
#include "nsIProgressEventSink.h"
#include "nsIDocument.h"
#include "nsHashtable.h"
#include "nsPluginLogging.h"
#include "nsIScriptChannel.h"
#include "nsIBlocklistService.h"
#include "nsVersionComparator.h"
#include "nsIObjectLoadingContent.h"
#include "nsIWritablePropertyBag2.h"
#include "nsICategoryManager.h"
#include "nsPluginStreamListenerPeer.h"
#include "mozilla/Preferences.h"

#include "nsEnumeratorUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsISupportsPrimitives.h"

#include "nsXULAppAPI.h"
#include "nsIXULRuntime.h"

// for the dialog
#include "nsIWindowWatcher.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"

#include "nsNetCID.h"
#include "prprf.h"
#include "nsThreadUtils.h"
#include "nsIInputStreamTee.h"

#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsPluginDirServiceProvider.h"

#include "nsUnicharUtils.h"
#include "nsPluginManifestLineReader.h"

#include "nsIWeakReferenceUtils.h"
#include "nsIPresShell.h"
#include "nsPluginNativeWindow.h"
#include "nsIScriptSecurityManager.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "nsIImageLoadingContent.h"
#include "mozilla/Preferences.h"
#include "nsVersionComparator.h"

#if defined(XP_WIN)
#include "nsIWindowMediator.h"
#include "nsIBaseWindow.h"
#include "windows.h"
#include "winbase.h"
#endif

#ifdef ANDROID
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#endif

#if MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

using namespace mozilla;
using mozilla::TimeStamp;

// Null out a strong ref to a linked list iteratively to avoid
// exhausting the stack (bug 486349).
#define NS_ITERATIVE_UNREF_LIST(type_, list_, mNext_)                \
  {                                                                  \
    while (list_) {                                                  \
      type_ temp = list_->mNext_;                                    \
      list_->mNext_ = nullptr;                                        \
      list_ = temp;                                                  \
    }                                                                \
  }

// this is the name of the directory which will be created
// to cache temporary files.
#define kPluginTmpDirName NS_LITERAL_CSTRING("plugtmp")

static const char *kPrefWhitelist = "plugin.allowed_types";
static const char *kPrefDisableFullPage = "plugin.disable_full_page_plugin_for_types";

// Version of cached plugin info
// 0.01 first implementation
// 0.02 added caching of CanUnload to fix bug 105935
// 0.03 changed name, description and mime desc from string to bytes, bug 108246
// 0.04 added new mime entry point on Mac, bug 113464
// 0.05 added new entry point check for the default plugin, bug 132430
// 0.06 strip off suffixes in mime description strings, bug 53895
// 0.07 changed nsIRegistry to flat file support for caching plugins info
// 0.08 mime entry point on MachO, bug 137535
// 0.09 the file encoding is changed to UTF-8, bug 420285
// 0.10 added plugin versions on appropriate platforms, bug 427743
// 0.11 file name and full path fields now store expected values on all platforms, bug 488181
// 0.12 force refresh due to quicktime pdf claim fix, bug 611197
// 0.13 add architecture and list of invalid plugins, bug 616271
// 0.14 force refresh due to locale comparison fix, bug 611296
// 0.15 force refresh due to bug in reading Java plist MIME data, bug 638171
// 0.16 version bump to avoid importing the plugin flags in newer versions
// The current plugin registry version (and the maximum version we know how to read)
static const char *kPluginRegistryVersion = "0.16";
// The minimum registry version we know how to read
static const char *kMinimumRegistryVersion = "0.9";

static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID);
static const char kDirectoryServiceContractID[] = "@mozilla.org/file/directory_service;1";

// Registry keys for caching plugin info
static const char kPluginsRootKey[] = "software/plugins";
static const char kPluginsNameKey[] = "name";
static const char kPluginsDescKey[] = "description";
static const char kPluginsFilenameKey[] = "filename";
static const char kPluginsFullpathKey[] = "fullpath";
static const char kPluginsModTimeKey[] = "lastModTimeStamp";
static const char kPluginsCanUnload[] = "canUnload";
static const char kPluginsVersionKey[] = "version";
static const char kPluginsMimeTypeKey[] = "mimetype";
static const char kPluginsMimeDescKey[] = "description";
static const char kPluginsMimeExtKey[] = "extension";

#define kPluginRegistryFilename NS_LITERAL_CSTRING("pluginreg.dat")

#ifdef PLUGIN_LOGGING
PRLogModuleInfo* nsPluginLogging::gNPNLog = nullptr;
PRLogModuleInfo* nsPluginLogging::gNPPLog = nullptr;
PRLogModuleInfo* nsPluginLogging::gPluginLog = nullptr;
#endif

// #defines for plugin cache and prefs
#define NS_PREF_MAX_NUM_CACHED_INSTANCES "browser.plugins.max_num_cached_plugins"
// Raise this from '10' to '50' to work around a bug in Apple's current Java
// plugins on OS X Lion and SnowLeopard.  See bug 705931.
#define DEFAULT_NUMBER_OF_STOPPED_INSTANCES 50

nsIFile *nsPluginHost::sPluginTempDir;
nsPluginHost *nsPluginHost::sInst;

NS_IMPL_ISUPPORTS0(nsInvalidPluginTag)

nsInvalidPluginTag::nsInvalidPluginTag(const char* aFullPath, int64_t aLastModifiedTime)
: mFullPath(aFullPath),
  mLastModifiedTime(aLastModifiedTime),
  mSeen(false)
{}

nsInvalidPluginTag::~nsInvalidPluginTag()
{}

// Helper to check for a MIME in a comma-delimited preference
static bool
IsTypeInList(nsCString &aMimeType, nsCString aTypeList)
{
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

  return FindInReadable(commaSeparated, start, end);
}

// flat file reg funcs
static
bool ReadSectionHeader(nsPluginManifestLineReader& reader, const char *token)
{
  do {
    if (*reader.LinePtr() == '[') {
      char* p = reader.LinePtr() + (reader.LineLength() - 1);
      if (*p != ']')
        break;
      *p = 0;

      char* values[1];
      if (1 != reader.ParseLine(values, 1))
        break;
      // ignore the leading '['
      if (PL_strcmp(values[0]+1, token)) {
        break; // it's wrong token
      }
      return true;
    }
  } while (reader.NextLine());
  return false;
}

static bool UnloadPluginsASAP()
{
  return Preferences::GetBool("dom.ipc.plugins.unloadASAP", false);
}

nsPluginHost::nsPluginHost()
  // No need to initialize members to nullptr, false etc because this class
  // has a zeroing operator new.
{
  // check to see if pref is set at startup to let plugins take over in
  // full page mode for certain image mime types that we handle internally
  mOverrideInternalTypes =
    Preferences::GetBool("plugin.override_internal_types", false);

  mPluginsDisabled = Preferences::GetBool("plugin.disable", false);
  mPluginsClickToPlay = Preferences::GetBool("plugins.click_to_play", false);

  Preferences::AddStrongObserver(this, "plugin.disable");
  Preferences::AddStrongObserver(this, "plugins.click_to_play");

  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService) {
    obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    obsService->AddObserver(this, "blocklist-updated", false);
#ifdef MOZ_WIDGET_ANDROID
    obsService->AddObserver(this, "application-foreground", false);
    obsService->AddObserver(this, "application-background", false);
#endif
  }

#ifdef PLUGIN_LOGGING
  nsPluginLogging::gNPNLog = PR_NewLogModule(NPN_LOG_NAME);
  nsPluginLogging::gNPPLog = PR_NewLogModule(NPP_LOG_NAME);
  nsPluginLogging::gPluginLog = PR_NewLogModule(PLUGIN_LOG_NAME);

  PR_LOG(nsPluginLogging::gNPNLog, PLUGIN_LOG_ALWAYS,("NPN Logging Active!\n"));
  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_ALWAYS,("General Plugin Logging Active! (nsPluginHost::ctor)\n"));
  PR_LOG(nsPluginLogging::gNPPLog, PLUGIN_LOG_ALWAYS,("NPP Logging Active!\n"));

  PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("nsPluginHost::ctor\n"));
  PR_LogFlush();
#endif
}

nsPluginHost::~nsPluginHost()
{
  PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("nsPluginHost::dtor\n"));

  UnloadPlugins();
  sInst = nullptr;
}

NS_IMPL_ISUPPORTS4(nsPluginHost,
                   nsIPluginHost,
                   nsIObserver,
                   nsITimerCallback,
                   nsISupportsWeakReference)

already_AddRefed<nsPluginHost>
nsPluginHost::GetInst()
{
  if (!sInst) {
    sInst = new nsPluginHost();
    if (!sInst)
      return nullptr;
    NS_ADDREF(sInst);
  }

  nsRefPtr<nsPluginHost> inst = sInst;
  return inst.forget();
}

bool nsPluginHost::IsRunningPlugin(nsPluginTag * aPluginTag)
{
  if (!aPluginTag || !aPluginTag->mPlugin) {
    return false;
  }

  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance *instance = mInstances[i].get();
    if (instance &&
        instance->GetPlugin() == aPluginTag->mPlugin &&
        instance->IsRunning()) {
      return true;
    }
  }

  return false;
}

nsresult nsPluginHost::ReloadPlugins()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHost::ReloadPlugins Begin\n"));

  nsresult rv = NS_OK;

  // this will create the initial plugin list out of cache
  // if it was not created yet
  if (!mPluginsLoaded)
    return LoadPlugins();

  // we are re-scanning plugins. New plugins may have been added, also some
  // plugins may have been removed, so we should probably shut everything down
  // but don't touch running (active and not stopped) plugins

  // check if plugins changed, no need to do anything else
  // if no changes to plugins have been made
  // false instructs not to touch the plugin list, just to
  // look for possible changes
  bool pluginschanged = true;
  FindPlugins(false, &pluginschanged);

  // if no changed detected, return an appropriate error code
  if (!pluginschanged)
    return NS_ERROR_PLUGINS_PLUGINSNOTCHANGED;

  // shutdown plugins and kill the list if there are no running plugins
  nsRefPtr<nsPluginTag> prev;
  nsRefPtr<nsPluginTag> next;

  for (nsRefPtr<nsPluginTag> p = mPlugins; p != nullptr;) {
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

  // set flags
  mPluginsLoaded = false;

  // load them again
  rv = LoadPlugins();

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHost::ReloadPlugins End\n"));

  return rv;
}

#define NS_RETURN_UASTRING_SIZE 128

nsresult nsPluginHost::UserAgent(const char **retstring)
{
  static char resultString[NS_RETURN_UASTRING_SIZE];
  nsresult res;

  nsCOMPtr<nsIHttpProtocolHandler> http = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &res);
  if (NS_FAILED(res))
    return res;

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
        }
        else if (resultString[i] == ' ') {
          resultString[i] = '\0';
          break;
        }
      }
    }
    *retstring = resultString;
  }
  else {
    *retstring = nullptr;
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHost::UserAgent return=%s\n", *retstring));

  return res;
}

nsresult nsPluginHost::GetPrompt(nsIPluginInstanceOwner *aOwner, nsIPrompt **aPrompt)
{
  nsresult rv;
  nsCOMPtr<nsIPrompt> prompt;
  nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);

  if (wwatch) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    if (aOwner) {
      nsCOMPtr<nsIDocument> document;
      aOwner->GetDocument(getter_AddRefs(document));
      if (document) {
        domWindow = document->GetWindow();
      }
    }

    if (!domWindow) {
      wwatch->GetWindowByName(NS_LITERAL_STRING("_content").get(), nullptr, getter_AddRefs(domWindow));
    }
    rv = wwatch->GetNewPrompter(domWindow, getter_AddRefs(prompt));
  }

  NS_IF_ADDREF(*aPrompt = prompt);
  return rv;
}

nsresult nsPluginHost::GetURL(nsISupports* pluginInst,
                              const char* url,
                              const char* target,
                              nsNPAPIPluginStreamListener* streamListener,
                              const char* altHost,
                              const char* referrer,
                              bool forceJSEnabled)
{
  return GetURLWithHeaders(static_cast<nsNPAPIPluginInstance*>(pluginInst),
                           url, target, streamListener, altHost, referrer,
                           forceJSEnabled, 0, nullptr);
}

nsresult nsPluginHost::GetURLWithHeaders(nsNPAPIPluginInstance* pluginInst,
                                         const char* url,
                                         const char* target,
                                         nsNPAPIPluginStreamListener* streamListener,
                                         const char* altHost,
                                         const char* referrer,
                                         bool forceJSEnabled,
                                         uint32_t getHeadersLength,
                                         const char* getHeaders)
{
  // we can only send a stream back to the plugin (as specified by a
  // null target) if we also have a nsNPAPIPluginStreamListener to talk to
  if (!target && !streamListener)
    return NS_ERROR_ILLEGAL_VALUE;

  nsresult rv = DoURLLoadSecurityCheck(pluginInst, url);
  if (NS_FAILED(rv))
    return rv;

  if (target) {
    nsRefPtr<nsPluginInstanceOwner> owner = pluginInst->GetOwner();
    if (owner) {
      if ((0 == PL_strcmp(target, "newwindow")) ||
          (0 == PL_strcmp(target, "_new")))
        target = "_blank";
      else if (0 == PL_strcmp(target, "_current"))
        target = "_self";

      rv = owner->GetURL(url, target, nullptr, nullptr, 0);
    }
  }

  if (streamListener)
    rv = NewPluginURLStream(NS_ConvertUTF8toUTF16(url), pluginInst,
                            streamListener, nullptr,
                            getHeaders, getHeadersLength);

  return rv;
}

nsresult nsPluginHost::PostURL(nsISupports* pluginInst,
                                    const char* url,
                                    uint32_t postDataLen,
                                    const char* postData,
                                    bool isFile,
                                    const char* target,
                                    nsNPAPIPluginStreamListener* streamListener,
                                    const char* altHost,
                                    const char* referrer,
                                    bool forceJSEnabled,
                                    uint32_t postHeadersLength,
                                    const char* postHeaders)
{
  nsresult rv;

  // we can only send a stream back to the plugin (as specified
  // by a null target) if we also have a nsNPAPIPluginStreamListener
  // to talk to also
  if (!target && !streamListener)
    return NS_ERROR_ILLEGAL_VALUE;

  nsNPAPIPluginInstance* instance = static_cast<nsNPAPIPluginInstance*>(pluginInst);

  rv = DoURLLoadSecurityCheck(instance, url);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIInputStream> postStream;
  if (isFile) {
    nsCOMPtr<nsIFile> file;
    rv = CreateTempFileToPost(postData, getter_AddRefs(file));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIInputStream> fileStream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream),
                                    file,
                                    PR_RDONLY,
                                    0600,
                                    nsIFileInputStream::DELETE_ON_CLOSE |
                                    nsIFileInputStream::CLOSE_ON_EOF);
    if (NS_FAILED(rv))
      return rv;

    rv = NS_NewBufferedInputStream(getter_AddRefs(postStream), fileStream, 8192);
    if (NS_FAILED(rv))
      return rv;
  } else {
    char *dataToPost;
    uint32_t newDataToPostLen;
    ParsePostBufferToFixHeaders(postData, postDataLen, &dataToPost, &newDataToPostLen);
    if (!dataToPost)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIStringInputStream> sis = do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
    if (!sis) {
      NS_Free(dataToPost);
      return rv;
    }

    // data allocated by ParsePostBufferToFixHeaders() is managed and
    // freed by the string stream.
    postDataLen = newDataToPostLen;
    sis->AdoptData(dataToPost, postDataLen);
    postStream = sis;
  }

  if (target) {
    nsRefPtr<nsPluginInstanceOwner> owner = instance->GetOwner();
    if (owner) {
      if ((0 == PL_strcmp(target, "newwindow")) ||
          (0 == PL_strcmp(target, "_new"))) {
        target = "_blank";
      } else if (0 == PL_strcmp(target, "_current")) {
        target = "_self";
      }
      rv = owner->GetURL(url, target, postStream,
                         (void*)postHeaders, postHeadersLength);
    }
  }

  // if we don't have a target, just create a stream.  This does
  // NS_OpenURI()!
  if (streamListener)
    rv = NewPluginURLStream(NS_ConvertUTF8toUTF16(url), instance,
                            streamListener,
                            postStream, postHeaders, postHeadersLength);

  return rv;
}

/* This method queries the prefs for proxy information.
 * It has been tested and is known to work in the following three cases
 * when no proxy host or port is specified
 * when only the proxy host is specified
 * when only the proxy port is specified
 * This method conforms to the return code specified in
 * http://developer.netscape.com/docs/manuals/proxy/adminnt/autoconf.htm#1020923
 * with the exception that multiple values are not implemented.
 */

nsresult nsPluginHost::FindProxyForURL(const char* url, char* *result)
{
  if (!url || !result) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult res;

  nsCOMPtr<nsIURI> uriIn;
  nsCOMPtr<nsIProtocolProxyService> proxyService;
  nsCOMPtr<nsIProtocolProxyService2> proxyService2;
  nsCOMPtr<nsIIOService> ioService;

  proxyService = do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &res);
  if (NS_FAILED(res) || !proxyService)
    return res;

  proxyService2 = do_QueryInterface(proxyService, &res);
  if (NS_FAILED(res) || !proxyService2)
    return res;

  ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &res);
  if (NS_FAILED(res) || !ioService)
    return res;

  // make an nsURI from the argument url
  res = ioService->NewURI(nsDependentCString(url), nullptr, nullptr, getter_AddRefs(uriIn));
  if (NS_FAILED(res))
    return res;

  nsCOMPtr<nsIProxyInfo> pi;

  // Remove this with bug 778201
  res = proxyService2->DeprecatedBlockingResolve(uriIn, 0, getter_AddRefs(pi));
  if (NS_FAILED(res))
    return res;

  nsAutoCString host, type;
  int32_t port = -1;

  // These won't fail, and even if they do... we'll be ok.
  if (pi) {
    pi->GetType(type);
    pi->GetHost(host);
    pi->GetPort(&port);
  }

  if (!pi || host.IsEmpty() || port <= 0 || host.EqualsLiteral("direct")) {
    *result = PL_strdup("DIRECT");
  } else if (type.EqualsLiteral("http")) {
    *result = PR_smprintf("PROXY %s:%d", host.get(), port);
  } else if (type.EqualsLiteral("socks4")) {
    *result = PR_smprintf("SOCKS %s:%d", host.get(), port);
  } else if (type.EqualsLiteral("socks")) {
    // XXX - this is socks5, but there is no API for us to tell the
    // plugin that fact. SOCKS for now, in case the proxy server
    // speaks SOCKS4 as well. See bug 78176
    // For a long time this was returning an http proxy type, so
    // very little is probably broken by this
    *result = PR_smprintf("SOCKS %s:%d", host.get(), port);
  } else {
    NS_ASSERTION(false, "Unknown proxy type!");
    *result = PL_strdup("DIRECT");
  }

  if (nullptr == *result)
    res = NS_ERROR_OUT_OF_MEMORY;

  return res;
}

nsresult nsPluginHost::Init()
{
  return NS_OK;
}

nsresult nsPluginHost::UnloadPlugins()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHost::UnloadPlugins Called\n"));

  if (!mPluginsLoaded)
    return NS_OK;

  // we should call nsIPluginInstance::Stop and nsIPluginInstance::SetWindow
  // for those plugins who want it
  DestroyRunningInstances(nullptr);

  nsPluginTag *pluginTag;
  for (pluginTag = mPlugins; pluginTag; pluginTag = pluginTag->mNext) {
    pluginTag->TryUnloadPlugin(true);
  }

  NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsPluginTag>, mPlugins, mNext);
  NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsPluginTag>, mCachedPlugins, mNext);
  NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);

  // Lets remove any of the temporary files that we created.
  if (sPluginTempDir) {
    sPluginTempDir->Remove(true);
    NS_RELEASE(sPluginTempDir);
  }

#ifdef XP_WIN
  if (mPrivateDirServiceProvider) {
    nsCOMPtr<nsIDirectoryService> dirService =
      do_GetService(kDirectoryServiceContractID);
    if (dirService)
      dirService->UnregisterProvider(mPrivateDirServiceProvider);
    mPrivateDirServiceProvider = nullptr;
  }
#endif /* XP_WIN */

  mPluginsLoaded = false;

  return NS_OK;
}

void nsPluginHost::OnPluginInstanceDestroyed(nsPluginTag* aPluginTag)
{
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
  // Our default behavior is to try to unload a plugin three minutes after
  // its last instance is destroyed. This seems like a reasonable compromise
  // that allows us to reclaim memory while allowing short state retention
  // and avoid perf hits for loading popular plugins.
  if (!hasInstance) {
    if (UnloadPluginsASAP()) {
      aPluginTag->TryUnloadPlugin(false);
    } else {
      if (aPluginTag->mUnloadTimer) {
        aPluginTag->mUnloadTimer->Cancel();
      } else {
        aPluginTag->mUnloadTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
      }
      aPluginTag->mUnloadTimer->InitWithCallback(this, 1000 * 60 * 3, nsITimer::TYPE_ONE_SHOT);
    }
  }
}

nsresult
nsPluginHost::GetPluginTempDir(nsIFile **aDir)
{
  if (!sPluginTempDir) {
    nsCOMPtr<nsIFile> tmpDir;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                         getter_AddRefs(tmpDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = tmpDir->AppendNative(kPluginTmpDirName);

    // make it unique, and mode == 0700, not world-readable
    rv = tmpDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
    NS_ENSURE_SUCCESS(rv, rv);

    tmpDir.swap(sPluginTempDir);
  }

  return sPluginTempDir->Clone(aDir);
}

nsresult
nsPluginHost::InstantiatePluginInstance(const char *aMimeType, nsIURI* aURL,
                                        nsObjectLoadingContent *aContent,
                                        nsPluginInstanceOwner** aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);

#ifdef PLUGIN_LOGGING
  nsAutoCString urlSpec;
  if (aURL)
    aURL->GetAsciiSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHost::InstantiatePlugin Begin mime=%s, url=%s\n",
        aMimeType, urlSpec.get()));

  PR_LogFlush();
#endif

  if (!aMimeType) {
    NS_NOTREACHED("Attempting to spawn a plugin with no mime type");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsPluginInstanceOwner> instanceOwner = new nsPluginInstanceOwner();
  if (!instanceOwner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIContent> ourContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(aContent));
  nsresult rv = instanceOwner->Init(ourContent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPluginTagInfo> pti;
  rv = instanceOwner->QueryInterface(kIPluginTagInfoIID, getter_AddRefs(pti));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsPluginTagType tagType;
  rv = pti->GetTagType(&tagType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (tagType != nsPluginTagType_Embed &&
      tagType != nsPluginTagType_Applet &&
      tagType != nsPluginTagType_Object) {
    return NS_ERROR_FAILURE;
  }

  rv = SetUpPluginInstance(aMimeType, aURL, instanceOwner);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsNPAPIPluginInstance> instance;
  rv = instanceOwner->GetInstance(getter_AddRefs(instance));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (instance) {
    instanceOwner->CreateWidget();

    // If we've got a native window, the let the plugin know about it.
    instanceOwner->CallSetWindow();
  }

  // At this point we consider instantiation to be successful. Do not return an error.
  instanceOwner.forget(aOwner);

#ifdef PLUGIN_LOGGING
  nsAutoCString urlSpec2;
  if (aURL != nullptr) aURL->GetAsciiSpec(urlSpec2);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHost::InstantiatePlugin Finished mime=%s, rv=%d, url=%s\n",
        aMimeType, rv, urlSpec2.get()));

  PR_LogFlush();
#endif

  return NS_OK;
}

nsPluginTag*
nsPluginHost::FindTagForLibrary(PRLibrary* aLibrary)
{
  nsPluginTag* pluginTag;
  for (pluginTag = mPlugins; pluginTag; pluginTag = pluginTag->mNext) {
    if (pluginTag->mLibrary == aLibrary) {
      return pluginTag;
    }
  }
  return nullptr;
}

nsPluginTag*
nsPluginHost::TagForPlugin(nsNPAPIPlugin* aPlugin)
{
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

nsresult nsPluginHost::SetUpPluginInstance(const char *aMimeType,
                                           nsIURI *aURL,
                                           nsPluginInstanceOwner *aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);

  nsresult rv = TrySetUpPluginInstance(aMimeType, aURL, aOwner);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  // If we failed to load a plugin instance we'll try again after
  // reloading our plugin list. Only do that once per document to
  // avoid redundant high resource usage on pages with multiple
  // unkown instance types. We'll do that by caching the document.
  nsCOMPtr<nsIDocument> document;
  aOwner->GetDocument(getter_AddRefs(document));

  nsCOMPtr<nsIDocument> currentdocument = do_QueryReferent(mCurrentDocument);
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

nsresult
nsPluginHost::TrySetUpPluginInstance(const char *aMimeType,
                                     nsIURI *aURL,
                                     nsPluginInstanceOwner *aOwner)
{
#ifdef PLUGIN_LOGGING
  nsAutoCString urlSpec;
  if (aURL != nullptr) aURL->GetSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHost::TrySetupPluginInstance Begin mime=%s, owner=%p, url=%s\n",
        aMimeType, aOwner, urlSpec.get()));

  PR_LogFlush();
#endif

  nsRefPtr<nsNPAPIPlugin> plugin;
  GetPlugin(aMimeType, getter_AddRefs(plugin));
  if (!plugin) {
    return NS_ERROR_FAILURE;
  }

  nsPluginTag* pluginTag = FindPluginForType(aMimeType, true);

  NS_ASSERTION(pluginTag, "Must have plugin tag here!");

#if defined(MOZ_WIDGET_ANDROID) && defined(MOZ_CRASHREPORTER)
  if (pluginTag->mIsFlashPlugin) {
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("FlashVersion"), pluginTag->mVersion);
  }
#endif

  nsRefPtr<nsNPAPIPluginInstance> instance = new nsNPAPIPluginInstance();

  // This will create the owning reference. The connection must be made between the
  // instance and the instance owner before initialization. Plugins can call into
  // the browser during initialization.
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
  nsAutoCString urlSpec2;
  if (aURL)
    aURL->GetSpec(urlSpec2);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_BASIC,
        ("nsPluginHost::TrySetupPluginInstance Finished mime=%s, rv=%d, owner=%p, url=%s\n",
        aMimeType, rv, aOwner, urlSpec2.get()));

  PR_LogFlush();
#endif

  return rv;
}

bool
nsPluginHost::PluginExistsForType(const char* aMimeType)
{
  nsPluginTag *plugin = FindPluginForType(aMimeType, false);
  return nullptr != plugin;
}

NS_IMETHODIMP
nsPluginHost::GetPluginTagForType(const nsACString& aMimeType,
                                  nsIPluginTag** aResult)
{
  nsPluginTag* plugin = FindPluginForType(aMimeType.Data(), true);
  if (!plugin) {
    plugin = FindPluginForType(aMimeType.Data(), false);
  }
  if (!plugin) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  NS_ADDREF(*aResult = plugin);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginHost::GetStateForType(const nsACString &aMimeType, uint32_t* aResult)
{
  nsPluginTag *plugin = FindPluginForType(aMimeType.Data(), true);
  if (!plugin) {
    plugin = FindPluginForType(aMimeType.Data(), false);
  }
  if (!plugin) {
    return NS_ERROR_UNEXPECTED;
  }

  return plugin->GetEnabledState(aResult);
}

NS_IMETHODIMP
nsPluginHost::GetBlocklistStateForType(const char *aMimeType, uint32_t *aState)
{
  nsPluginTag *plugin = FindPluginForType(aMimeType, true);
  if (!plugin) {
    plugin = FindPluginForType(aMimeType, false);
  }
  if (!plugin) {
    return NS_ERROR_FAILURE;
  }

  *aState = plugin->GetBlocklistState();
  return NS_OK;
}

NS_IMETHODIMP
nsPluginHost::GetPermissionStringForType(const nsACString &aMimeType, nsACString &aPermissionString)
{
  aPermissionString.Truncate();
  uint32_t blocklistState;
  nsresult rv = GetBlocklistStateForType(aMimeType.Data(), &blocklistState);
  NS_ENSURE_SUCCESS(rv, rv);
  nsPluginTag *tag = FindPluginForType(aMimeType.Data(), true);
  if (!tag) {
    tag = FindPluginForType(aMimeType.Data(), false);
  }
  if (!tag) {
    return NS_ERROR_FAILURE;
  }

  if (blocklistState == nsIBlocklistService::STATE_VULNERABLE_UPDATE_AVAILABLE ||
      blocklistState == nsIBlocklistService::STATE_VULNERABLE_NO_UPDATE) {
    aPermissionString.AssignLiteral("plugin-vulnerable:");
  }
  else {
    aPermissionString.AssignLiteral("plugin:");
  }

  aPermissionString.Append(tag->GetNiceFileName());

  return NS_OK;
}

// check comma delimitered extensions
static int CompareExtensions(const char *aExtensionList, const char *aExtension)
{
  if (!aExtensionList || !aExtension)
    return -1;

  const char *pExt = aExtensionList;
  const char *pComma = strchr(pExt, ',');
  if (!pComma)
    return PL_strcasecmp(pExt, aExtension);

  int extlen = strlen(aExtension);
  while (pComma) {
    int length = pComma - pExt;
    if (length == extlen && 0 == PL_strncasecmp(aExtension, pExt, length))
      return 0;
    pComma++;
    pExt = pComma;
    pComma = strchr(pExt, ',');
  }

  // the last one
  return PL_strcasecmp(pExt, aExtension);
}

nsresult
nsPluginHost::IsPluginEnabledForExtension(const char* aExtension,
                                          const char* &aMimeType)
{
  nsPluginTag *plugin = FindPluginEnabledForExtension(aExtension, aMimeType);
  if (plugin)
    return NS_OK;

  return NS_ERROR_FAILURE;
}

void
nsPluginHost::GetPlugins(nsTArray<nsRefPtr<nsPluginTag> >& aPluginArray)
{
  aPluginArray.Clear();

  LoadPlugins();

  nsPluginTag* plugin = mPlugins;
  while (plugin != nullptr) {
    if (plugin->IsEnabled()) {
      aPluginArray.AppendElement(plugin);
    }
    plugin = plugin->mNext;
  }
}

NS_IMETHODIMP
nsPluginHost::GetPluginTags(uint32_t* aPluginCount, nsIPluginTag*** aResults)
{
  LoadPlugins();

  uint32_t count = 0;
  nsRefPtr<nsPluginTag> plugin = mPlugins;
  while (plugin != nullptr) {
    count++;
    plugin = plugin->mNext;
  }

  *aResults = static_cast<nsIPluginTag**>
                         (nsMemory::Alloc(count * sizeof(**aResults)));
  if (!*aResults)
    return NS_ERROR_OUT_OF_MEMORY;

  *aPluginCount = count;

  plugin = mPlugins;
  for (uint32_t i = 0; i < count; i++) {
    (*aResults)[i] = plugin;
    NS_ADDREF((*aResults)[i]);
    plugin = plugin->mNext;
  }

  return NS_OK;
}

nsPluginTag*
nsPluginHost::FindPreferredPlugin(const InfallibleTArray<nsPluginTag*>& matches)
{
  // We prefer the plugin with the highest version number.
  /// XXX(johns): This seems to assume the only time multiple plugins will have
  ///             the same MIME type is if they're multiple versions of the same
  ///             plugin -- but since plugin filenames and pretty names can both
  ///             update, it's probably less arbitrary than just going at it
  ///             alphabetically.

  if (matches.IsEmpty()) {
    return nullptr;
  }

  nsPluginTag *preferredPlugin = matches[0];
  for (unsigned int i = 1; i < matches.Length(); i++) {
    if (mozilla::Version(matches[i]->mVersion.get()) > preferredPlugin->mVersion.get()) {
      preferredPlugin = matches[i];
    }
  }

  return preferredPlugin;
}

nsPluginTag*
nsPluginHost::FindPluginForType(const char* aMimeType,
                                bool aCheckEnabled)
{
  if (!aMimeType) {
    return nullptr;
  }

  LoadPlugins();

  InfallibleTArray<nsPluginTag*> matchingPlugins;

  nsPluginTag *plugin = mPlugins;
  while (plugin) {
    if (!aCheckEnabled || plugin->IsActive()) {
      int32_t mimeCount = plugin->mMimeTypes.Length();
      for (int32_t i = 0; i < mimeCount; i++) {
        if (0 == PL_strcasecmp(plugin->mMimeTypes[i].get(), aMimeType)) {
          matchingPlugins.AppendElement(plugin);
          break;
        }
      }
    }
    plugin = plugin->mNext;
  }

  return FindPreferredPlugin(matchingPlugins);
}

nsPluginTag*
nsPluginHost::FindPluginEnabledForExtension(const char* aExtension,
                                            const char*& aMimeType)
{
  if (!aExtension) {
    return nullptr;
  }

  LoadPlugins();

  InfallibleTArray<nsPluginTag*> matchingPlugins;

  nsPluginTag *plugin = mPlugins;
  while (plugin) {
    if (plugin->IsActive()) {
      int32_t variants = plugin->mExtensions.Length();
      for (int32_t i = 0; i < variants; i++) {
        // mExtensionsArray[cnt] is a list of extensions separated by commas
        if (0 == CompareExtensions(plugin->mExtensions[i].get(), aExtension)) {
          matchingPlugins.AppendElement(plugin);
          break;
        }
      }
    }
    plugin = plugin->mNext;
  }

  nsPluginTag *preferredPlugin = FindPreferredPlugin(matchingPlugins);
  if (!preferredPlugin) {
    return nullptr;
  }

  int32_t variants = preferredPlugin->mExtensions.Length();
  for (int32_t i = 0; i < variants; i++) {
    // mExtensionsArray[cnt] is a list of extensions separated by commas
    if (0 == CompareExtensions(preferredPlugin->mExtensions[i].get(), aExtension)) {
      aMimeType = preferredPlugin->mMimeTypes[i].get();
      break;
    }
  }

  return preferredPlugin;
}

static nsresult CreateNPAPIPlugin(nsPluginTag *aPluginTag,
                                  nsNPAPIPlugin **aOutNPAPIPlugin)
{
  // If this is an in-process plugin we'll need to load it here if we haven't already.
  if (!nsNPAPIPlugin::RunPluginOOP(aPluginTag)) {
    if (aPluginTag->mFullPath.IsEmpty())
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1");
    file->InitWithPath(NS_ConvertUTF8toUTF16(aPluginTag->mFullPath));
    nsPluginFile pluginFile(file);
    PRLibrary* pluginLibrary = NULL;

    if (NS_FAILED(pluginFile.LoadPlugin(&pluginLibrary)) || !pluginLibrary)
      return NS_ERROR_FAILURE;

    aPluginTag->mLibrary = pluginLibrary;
  }

  nsresult rv;
  rv = nsNPAPIPlugin::CreatePlugin(aPluginTag, aOutNPAPIPlugin);

  return rv;
}

nsresult nsPluginHost::EnsurePluginLoaded(nsPluginTag* aPluginTag)
{
  nsRefPtr<nsNPAPIPlugin> plugin = aPluginTag->mPlugin;
  if (!plugin) {
    nsresult rv = CreateNPAPIPlugin(aPluginTag, getter_AddRefs(plugin));
    if (NS_FAILED(rv)) {
      return rv;
    }
    aPluginTag->mPlugin = plugin;
  }
  return NS_OK;
}

nsresult nsPluginHost::GetPlugin(const char *aMimeType, nsNPAPIPlugin** aPlugin)
{
  nsresult rv = NS_ERROR_FAILURE;
  *aPlugin = NULL;

  if (!aMimeType)
    return NS_ERROR_ILLEGAL_VALUE;

  // If plugins haven't been scanned yet, do so now
  LoadPlugins();

  nsPluginTag* pluginTag = FindPluginForType(aMimeType, true);
  if (pluginTag) {
    rv = NS_OK;
    PLUGIN_LOG(PLUGIN_LOG_BASIC,
    ("nsPluginHost::GetPlugin Begin mime=%s, plugin=%s\n",
    aMimeType, pluginTag->mFileName.get()));

#ifdef DEBUG
    if (aMimeType && !pluginTag->mFileName.IsEmpty())
      printf("For %s found plugin %s\n", aMimeType, pluginTag->mFileName.get());
#endif

    rv = EnsurePluginLoaded(pluginTag);
    if (NS_FAILED(rv)) {
      return rv;
    }

    NS_ADDREF(*aPlugin = pluginTag->mPlugin);
    return NS_OK;
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHost::GetPlugin End mime=%s, rv=%d, plugin=%p name=%s\n",
  aMimeType, rv, *aPlugin,
  (pluginTag ? pluginTag->mFileName.get() : "(not found)")));

  return rv;
}

// Normalize 'host' to ACE.
nsresult
nsPluginHost::NormalizeHostname(nsCString& host)
{
  if (IsASCII(host)) {
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
nsresult
nsPluginHost::EnumerateSiteData(const nsACString& domain,
                                const InfallibleTArray<nsCString>& sites,
                                InfallibleTArray<nsCString>& result,
                                bool firstMatchOnly)
{
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
    if (siteIsIP != isIP)
      continue;

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

NS_IMETHODIMP
nsPluginHost::RegisterPlayPreviewMimeType(const nsACString& mimeType,
                                          bool ignoreCTP,
                                          const nsACString& redirectURL)
{
  nsAutoCString mt(mimeType);
  nsAutoCString url(redirectURL);
  if (url.Length() == 0) {
    // using default play preview iframe URL, if redirectURL is not specified
    url.Assign("data:application/x-moz-playpreview;,");
    url.Append(mimeType);
  }

  nsRefPtr<nsPluginPlayPreviewInfo> playPreview =
    new nsPluginPlayPreviewInfo(mt.get(), ignoreCTP, url.get());
  mPlayPreviewMimeTypes.AppendElement(playPreview);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginHost::UnregisterPlayPreviewMimeType(const nsACString& mimeType)
{
  nsAutoCString mimeTypeToRemove(mimeType);
  for (uint32_t i = mPlayPreviewMimeTypes.Length(); i > 0; i--) {
    nsRefPtr<nsPluginPlayPreviewInfo> pp = mPlayPreviewMimeTypes[i - 1];
    if (PL_strcasecmp(pp.get()->mMimeType.get(), mimeTypeToRemove.get()) == 0) {
      mPlayPreviewMimeTypes.RemoveElementAt(i - 1);
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPluginHost::GetPlayPreviewInfo(const nsACString& mimeType,
                                 nsIPluginPlayPreviewInfo** aResult)
{
  nsAutoCString mimeTypeToFind(mimeType);
  for (uint32_t i = 0; i < mPlayPreviewMimeTypes.Length(); i++) {
    nsRefPtr<nsPluginPlayPreviewInfo> pp = mPlayPreviewMimeTypes[i];
    if (PL_strcasecmp(pp.get()->mMimeType.get(), mimeTypeToFind.get()) == 0) {
      *aResult = new nsPluginPlayPreviewInfo(pp.get());
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }
  *aResult = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsPluginHost::ClearSiteData(nsIPluginTag* plugin, const nsACString& domain,
                            uint64_t flags, int64_t maxAge)
{
  // maxAge must be either a nonnegative integer or -1.
  NS_ENSURE_ARG(maxAge >= 0 || maxAge == -1);

  // Caller may give us a tag object that is no longer live.
  if (!IsLiveTag(plugin)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

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

  // If 'domain' is the null string, clear everything.
  if (domain.IsVoid()) {
    return library->NPP_ClearSiteData(NULL, flags, maxAge);
  }

  // Get the list of sites from the plugin.
  InfallibleTArray<nsCString> sites;
  rv = library->NPP_GetSitesWithData(sites);
  NS_ENSURE_SUCCESS(rv, rv);

  // Enumerate the sites and build a list of matches.
  InfallibleTArray<nsCString> matches;
  rv = EnumerateSiteData(domain, sites, matches, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear the matches.
  for (uint32_t i = 0; i < matches.Length(); ++i) {
    const nsCString& match = matches[i];
    rv = library->NPP_ClearSiteData(match.get(), flags, maxAge);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPluginHost::SiteHasData(nsIPluginTag* plugin, const nsACString& domain,
                          bool* result)
{
  // Caller may give us a tag object that is no longer live.
  if (!IsLiveTag(plugin)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

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

  // Get the list of sites from the plugin.
  InfallibleTArray<nsCString> sites;
  rv = library->NPP_GetSitesWithData(sites);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there's no data, we're done.
  if (sites.IsEmpty()) {
    *result = false;
    return NS_OK;
  }

  // If 'domain' is the null string, and there's data for at least one site,
  // we're done.
  if (domain.IsVoid()) {
    *result = true;
    return NS_OK;
  }

  // Enumerate the sites and determine if there's a match.
  InfallibleTArray<nsCString> matches;
  rv = EnumerateSiteData(domain, sites, matches, true);
  NS_ENSURE_SUCCESS(rv, rv);

  *result = !matches.IsEmpty();
  return NS_OK;
}

bool nsPluginHost::IsJavaMIMEType(const char* aType)
{
  return aType &&
    ((0 == PL_strncasecmp(aType, "application/x-java-vm",
                          sizeof("application/x-java-vm") - 1)) ||
     (0 == PL_strncasecmp(aType, "application/x-java-applet",
                          sizeof("application/x-java-applet") - 1)) ||
     (0 == PL_strncasecmp(aType, "application/x-java-bean",
                          sizeof("application/x-java-bean") - 1)));
}

// Check whether or not a tag is a live, valid tag, and that it's loaded.
bool
nsPluginHost::IsLiveTag(nsIPluginTag* aPluginTag)
{
  nsPluginTag* tag;
  for (tag = mPlugins; tag; tag = tag->mNext) {
    if (tag == aPluginTag) {
      return true;
    }
  }
  return false;
}

nsPluginTag*
nsPluginHost::HaveSamePlugin(const nsPluginTag* aPluginTag)
{
  for (nsPluginTag* tag = mPlugins; tag; tag = tag->mNext) {
    if (tag->HasSameNameAndMimes(aPluginTag)) {
        return tag;
    }
  }
  return nullptr;
}

nsPluginTag*
nsPluginHost::FirstPluginWithPath(const nsCString& path)
{
  for (nsPluginTag* tag = mPlugins; tag; tag = tag->mNext) {
    if (tag->mFullPath.Equals(path)) {
      return tag;
    }
  }
  return nullptr;
}

namespace {

int64_t GetPluginLastModifiedTime(const nsCOMPtr<nsIFile>& localfile)
{
  PRTime fileModTime = 0;

#if defined(XP_MACOSX)
  // On OS X the date of a bundle's "contents" (i.e. of its Info.plist file)
  // is a much better guide to when it was last modified than the date of
  // its package directory.  See bug 313700.
  nsCOMPtr<nsILocalFileMac> localFileMac = do_QueryInterface(localfile);
  if (localFileMac) {
    localFileMac->GetBundleContentsLastModifiedTime(&fileModTime);
  } else {
    localfile->GetLastModifiedTime(&fileModTime);
  }
#else
  localfile->GetLastModifiedTime(&fileModTime);
#endif

  return fileModTime;
}

struct CompareFilesByTime
{
  bool
  LessThan(const nsCOMPtr<nsIFile>& a, const nsCOMPtr<nsIFile>& b) const
  {
    return GetPluginLastModifiedTime(a) < GetPluginLastModifiedTime(b);
  }

  bool
  Equals(const nsCOMPtr<nsIFile>& a, const nsCOMPtr<nsIFile>& b) const
  {
    return GetPluginLastModifiedTime(a) == GetPluginLastModifiedTime(b);
  }
};

} // anonymous namespace

typedef NS_NPAPIPLUGIN_CALLBACK(char *, NP_GETMIMEDESCRIPTION)(void);

nsresult nsPluginHost::ScanPluginsDirectory(nsIFile *pluginsDir,
                                            bool aCreatePluginList,
                                            bool *aPluginsChanged)
{
  NS_ENSURE_ARG_POINTER(aPluginsChanged);
  nsresult rv;

  *aPluginsChanged = false;

#ifdef PLUGIN_LOGGING
  nsAutoCString dirPath;
  pluginsDir->GetNativePath(dirPath);
  PLUGIN_LOG(PLUGIN_LOG_BASIC,
  ("nsPluginHost::ScanPluginsDirectory dir=%s\n", dirPath.get()));
#endif

  nsCOMPtr<nsISimpleEnumerator> iter;
  rv = pluginsDir->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv))
    return rv;

  nsAutoTArray<nsCOMPtr<nsIFile>, 6> pluginFiles;

  bool hasMore;
  while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = iter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv))
      continue;
    nsCOMPtr<nsIFile> dirEntry(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv))
      continue;

    // Sun's JRE 1.3.1 plugin must have symbolic links resolved or else it'll crash.
    // See bug 197855.
    dirEntry->Normalize();

    if (nsPluginsDir::IsPluginFile(dirEntry)) {
      pluginFiles.AppendElement(dirEntry);
    }
  }

  pluginFiles.Sort(CompareFilesByTime());

  bool warnOutdated = false;

  for (int32_t i = (pluginFiles.Length() - 1); i >= 0; i--) {
    nsCOMPtr<nsIFile>& localfile = pluginFiles[i];

    nsString utf16FilePath;
    rv = localfile->GetPath(utf16FilePath);
    if (NS_FAILED(rv))
      continue;

    int64_t fileModTime = GetPluginLastModifiedTime(localfile);

    // Look for it in our cache
    NS_ConvertUTF16toUTF8 filePath(utf16FilePath);
    nsRefPtr<nsPluginTag> pluginTag;
    RemoveCachedPluginsInfo(filePath.get(), getter_AddRefs(pluginTag));

    bool seenBefore = false;

    if (pluginTag) {
      seenBefore = true;
      // If plugin changed, delete cachedPluginTag and don't use cache
      if (fileModTime != pluginTag->mLastModifiedTime) {
        // Plugins has changed. Don't use cached plugin info.
        pluginTag = nullptr;

        // plugin file changed, flag this fact
        *aPluginsChanged = true;
      }

      // If we're not creating a list and we already know something changed then
      // we're done.
      if (!aCreatePluginList) {
        if (*aPluginsChanged) {
          return NS_OK;
        }
        continue;
      }
    }

    bool isKnownInvalidPlugin = false;
    for (nsRefPtr<nsInvalidPluginTag> invalidPlugins = mInvalidPlugins;
         invalidPlugins; invalidPlugins = invalidPlugins->mNext) {
      // If already marked as invalid, ignore it
      if (invalidPlugins->mFullPath.Equals(filePath.get()) &&
          invalidPlugins->mLastModifiedTime == fileModTime) {
        if (aCreatePluginList) {
          invalidPlugins->mSeen = true;
        }
        isKnownInvalidPlugin = true;
        break;
      }
    }
    if (isKnownInvalidPlugin) {
      continue;
    }

    // if it is not found in cache info list or has been changed, create a new one
    if (!pluginTag) {
      nsPluginFile pluginFile(localfile);

      // create a tag describing this plugin.
      PRLibrary *library = nullptr;
      nsPluginInfo info;
      memset(&info, 0, sizeof(info));
      nsresult res;
      // Opening a block for the telemetry AutoTimer
      {
        Telemetry::AutoTimer<Telemetry::PLUGIN_LOAD_METADATA> telemetry;
        res = pluginFile.GetPluginInfo(info, &library);
      }
      // if we don't have mime type don't proceed, this is not a plugin
      if (NS_FAILED(res) || !info.fMimeTypeArray) {
        nsRefPtr<nsInvalidPluginTag> invalidTag = new nsInvalidPluginTag(filePath.get(),
                                                                         fileModTime);
        pluginFile.FreePluginInfo(info);

        if (aCreatePluginList) {
          invalidTag->mSeen = true;
        }
        invalidTag->mNext = mInvalidPlugins;
        if (mInvalidPlugins) {
          mInvalidPlugins->mPrev = invalidTag;
        }
        mInvalidPlugins = invalidTag;

        // Mark aPluginsChanged so pluginreg is rewritten
        *aPluginsChanged = true;
        continue;
      }

      pluginTag = new nsPluginTag(&info);
      pluginFile.FreePluginInfo(info);
      if (!pluginTag)
        return NS_ERROR_OUT_OF_MEMORY;

      pluginTag->mLibrary = library;
      pluginTag->mLastModifiedTime = fileModTime;
      uint32_t state = pluginTag->GetBlocklistState();

      // If the blocklist says it is risky and we have never seen this
      // plugin before, then disable it.
      // If the blocklist says this is an outdated plugin, warn about
      // outdated plugins.
      if (state == nsIBlocklistService::STATE_SOFTBLOCKED && !seenBefore) {
        pluginTag->SetEnabledState(nsIPluginTag::STATE_DISABLED);
      }
      if (state == nsIBlocklistService::STATE_OUTDATED && !seenBefore) {
        warnOutdated = true;
      }

      // Plugin unloading is tag-based. If we created a new tag and loaded
      // the library in the process then we want to attempt to unload it here.
      // Only do this if the pref is set for aggressive unloading.
      if (UnloadPluginsASAP()) {
        pluginTag->TryUnloadPlugin(false);
      }
    }

    // do it if we still want it
    if (!seenBefore) {
      // We have a valid new plugin so report that plugins have changed.
      *aPluginsChanged = true;
    }

    // Avoid adding different versions of the same plugin if they are running 
    // in-process, otherwise we risk undefined behaviour.
    if (!nsNPAPIPlugin::RunPluginOOP(pluginTag)) {
      if (HaveSamePlugin(pluginTag)) {
        continue;
      }
    }

    // Don't add the same plugin again if it hasn't changed
    if (nsPluginTag* duplicate = FirstPluginWithPath(pluginTag->mFullPath)) {
      if (pluginTag->mLastModifiedTime == duplicate->mLastModifiedTime) {
        continue;
      }
    }

    // If we're not creating a plugin list, simply looking for changes,
    // then we're done.
    if (!aCreatePluginList) {
      return NS_OK;
    }

    // Add plugin tags such that the list is ordered by modification date,
    // newest to oldest. This is ugly, it'd be easier with just about anything
    // other than a single-directional linked list.
    if (mPlugins) {
      nsPluginTag *prev = nullptr;
      nsPluginTag *next = mPlugins;
      while (next) {
        if (pluginTag->mLastModifiedTime >= next->mLastModifiedTime) {
          pluginTag->mNext = next;
          if (prev) {
            prev->mNext = pluginTag;
          } else {
            mPlugins = pluginTag;
          }
          break;
        }
        prev = next;
        next = prev->mNext;
        if (!next) {
          prev->mNext = pluginTag;
        }
      }
    } else {
      mPlugins = pluginTag;
    }

    if (pluginTag->IsActive()) {
      nsAdoptingCString disableFullPage =
        Preferences::GetCString(kPrefDisableFullPage);
      for (uint32_t i = 0; i < pluginTag->mMimeTypes.Length(); i++) {
        if (!IsTypeInList(pluginTag->mMimeTypes[i], disableFullPage)) {
          RegisterWithCategoryManager(pluginTag->mMimeTypes[i],
                                      ePluginRegister);
        }
      }
    }
  }

  if (warnOutdated) {
    Preferences::SetBool("plugins.update.notifyUser", true);
  }

  return NS_OK;
}

nsresult nsPluginHost::ScanPluginsDirectoryList(nsISimpleEnumerator *dirEnum,
                                                bool aCreatePluginList,
                                                bool *aPluginsChanged)
{
    bool hasMore;
    while (NS_SUCCEEDED(dirEnum->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> supports;
      nsresult rv = dirEnum->GetNext(getter_AddRefs(supports));
      if (NS_FAILED(rv))
        continue;
      nsCOMPtr<nsIFile> nextDir(do_QueryInterface(supports, &rv));
      if (NS_FAILED(rv))
        continue;

      // don't pass aPluginsChanged directly to prevent it from been reset
      bool pluginschanged = false;
      ScanPluginsDirectory(nextDir, aCreatePluginList, &pluginschanged);

      if (pluginschanged)
        *aPluginsChanged = true;

      // if changes are detected and we are not creating the list, do not proceed
      if (!aCreatePluginList && *aPluginsChanged)
        break;
    }
    return NS_OK;
}

nsresult nsPluginHost::LoadPlugins()
{
#ifdef ANDROID
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    return NS_OK;
  }
#endif
  // do not do anything if it is already done
  // use ReloadPlugins() to enforce loading
  if (mPluginsLoaded)
    return NS_OK;

  if (mPluginsDisabled)
    return NS_OK;

  bool pluginschanged;
  nsresult rv = FindPlugins(true, &pluginschanged);
  if (NS_FAILED(rv))
    return rv;

  // only if plugins have changed will we notify plugin-change observers
  if (pluginschanged) {
    nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
    if (obsService)
      obsService->NotifyObservers(nullptr, "plugins-list-updated", nullptr);
  }

  return NS_OK;
}

// if aCreatePluginList is false we will just scan for plugins
// and see if any changes have been made to the plugins.
// This is needed in ReloadPlugins to prevent possible recursive reloads
nsresult nsPluginHost::FindPlugins(bool aCreatePluginList, bool * aPluginsChanged)
{
  Telemetry::AutoTimer<Telemetry::FIND_PLUGINS> telemetry;

  NS_ENSURE_ARG_POINTER(aPluginsChanged);

  *aPluginsChanged = false;
  nsresult rv;

  // Read cached plugins info. If the profile isn't yet available then don't
  // scan for plugins
  if (ReadPluginInfo() == NS_ERROR_NOT_AVAILABLE)
    return NS_OK;

#ifdef XP_WIN
  // Failure here is not a show-stopper so just warn.
  rv = EnsurePrivateDirServiceProvider();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to register dir service provider.");
#endif /* XP_WIN */

  nsCOMPtr<nsIProperties> dirService(do_GetService(kDirectoryServiceContractID, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISimpleEnumerator> dirList;

  // Scan plugins directories;
  // don't pass aPluginsChanged directly, to prevent its
  // possible reset in subsequent ScanPluginsDirectory calls
  bool pluginschanged = false;

  // Scan the app-defined list of plugin dirs.
  rv = dirService->Get(NS_APP_PLUGINS_DIR_LIST, NS_GET_IID(nsISimpleEnumerator), getter_AddRefs(dirList));
  if (NS_SUCCEEDED(rv)) {
    ScanPluginsDirectoryList(dirList, aCreatePluginList, &pluginschanged);

    if (pluginschanged)
      *aPluginsChanged = true;

    // if we are just looking for possible changes,
    // no need to proceed if changes are detected
    if (!aCreatePluginList && *aPluginsChanged) {
      NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsPluginTag>, mCachedPlugins, mNext);
      NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);
      return NS_OK;
    }
  } else {
#ifdef ANDROID
    LOG("getting plugins dir failed");
#endif
  }

  mPluginsLoaded = true; // at this point 'some' plugins have been loaded,
                            // the rest is optional

#ifdef XP_WIN
  bool bScanPLIDs = Preferences::GetBool("plugin.scan.plid.all", false);

    // Now lets scan any PLID directories
  if (bScanPLIDs && mPrivateDirServiceProvider) {
    rv = mPrivateDirServiceProvider->GetPLIDDirectories(getter_AddRefs(dirList));
    if (NS_SUCCEEDED(rv)) {
      ScanPluginsDirectoryList(dirList, aCreatePluginList, &pluginschanged);

      if (pluginschanged)
        *aPluginsChanged = true;

      // if we are just looking for possible changes,
      // no need to proceed if changes are detected
      if (!aCreatePluginList && *aPluginsChanged) {
        NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsPluginTag>, mCachedPlugins, mNext);
        NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);
        return NS_OK;
      }
    }
  }


  // Scan the installation paths of our popular plugins if the prefs are enabled

  // This table controls the order of scanning
  const char* const prefs[] = {NS_WIN_ACROBAT_SCAN_KEY,
                               NS_WIN_QUICKTIME_SCAN_KEY,
                               NS_WIN_WMP_SCAN_KEY};

  uint32_t size = sizeof(prefs) / sizeof(prefs[0]);

  for (uint32_t i = 0; i < size; i+=1) {
    nsCOMPtr<nsIFile> dirToScan;
    bool bExists;
    if (NS_SUCCEEDED(dirService->Get(prefs[i], NS_GET_IID(nsIFile), getter_AddRefs(dirToScan))) &&
        dirToScan &&
        NS_SUCCEEDED(dirToScan->Exists(&bExists)) &&
        bExists) {

      ScanPluginsDirectory(dirToScan, aCreatePluginList, &pluginschanged);

      if (pluginschanged)
        *aPluginsChanged = true;

      // if we are just looking for possible changes,
      // no need to proceed if changes are detected
      if (!aCreatePluginList && *aPluginsChanged) {
        NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsPluginTag>, mCachedPlugins, mNext);
        NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);
        return NS_OK;
      }
    }
  }
#endif

  // We should also consider plugins to have changed if any plugins have been removed.
  // We'll know if any were removed if they weren't taken out of the cached plugins list
  // during our scan, thus we can assume something was removed if the cached plugins list
  // contains anything.
  if (!*aPluginsChanged && mCachedPlugins) {
    *aPluginsChanged = true;
  }

  // Remove unseen invalid plugins
  nsRefPtr<nsInvalidPluginTag> invalidPlugins = mInvalidPlugins;
  while (invalidPlugins) {
    if (!invalidPlugins->mSeen) {
      nsRefPtr<nsInvalidPluginTag> invalidPlugin = invalidPlugins;

      if (invalidPlugin->mPrev) {
        invalidPlugin->mPrev->mNext = invalidPlugin->mNext;
      }
      else {
        mInvalidPlugins = invalidPlugin->mNext;
      }
      if (invalidPlugin->mNext) {
        invalidPlugin->mNext->mPrev = invalidPlugin->mPrev;
      }

      invalidPlugins = invalidPlugin->mNext;

      invalidPlugin->mPrev = NULL;
      invalidPlugin->mNext = NULL;
    }
    else {
      invalidPlugins->mSeen = false;
      invalidPlugins = invalidPlugins->mNext;
    }
  }

  // if we are not creating the list, there is no need to proceed
  if (!aCreatePluginList) {
    NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsPluginTag>, mCachedPlugins, mNext);
    NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);
    return NS_OK;
  }

  // if we are creating the list, it is already done;
  // update the plugins info cache if changes are detected
  if (*aPluginsChanged)
    WritePluginInfo();

  // No more need for cached plugins. Clear it up.
  NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsPluginTag>, mCachedPlugins, mNext);
  NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);

  return NS_OK;
}

nsresult
nsPluginHost::UpdatePluginInfo(nsPluginTag* aPluginTag)
{
  ReadPluginInfo();
  WritePluginInfo();
  NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsPluginTag>, mCachedPlugins, mNext);
  NS_ITERATIVE_UNREF_LIST(nsRefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);

  if (!aPluginTag) {
    return NS_OK;
  }

  // Update types with category manager
  nsAdoptingCString disableFullPage =
    Preferences::GetCString(kPrefDisableFullPage);
  for (uint32_t i = 0; i < aPluginTag->mMimeTypes.Length(); i++) {
    nsRegisterType shouldRegister;

    if (IsTypeInList(aPluginTag->mMimeTypes[i], disableFullPage)) {
      shouldRegister = ePluginUnregister;
    } else {
      nsPluginTag *plugin = FindPluginForType(aPluginTag->mMimeTypes[i].get(),
                                              true);
      shouldRegister = plugin ? ePluginRegister : ePluginUnregister;
    }

    RegisterWithCategoryManager(aPluginTag->mMimeTypes[i], shouldRegister);
  }

  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService)
    obsService->NotifyObservers(nullptr, "plugin-info-updated", nullptr);

  // Reload instances if needed
  if (aPluginTag->IsActive()) {
    return NS_OK;
  }

  return NS_OK;
}

/* static */ bool
nsPluginHost::IsTypeWhitelisted(const char *aMimeType)
{
  nsAdoptingCString whitelist = Preferences::GetCString(kPrefWhitelist);
  if (!whitelist.Length()) {
    return true;
  }
  nsDependentCString wrap(aMimeType);
  return IsTypeInList(wrap, whitelist);
}

void
nsPluginHost::RegisterWithCategoryManager(nsCString &aMimeType,
                                          nsRegisterType aType)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
             ("nsPluginTag::RegisterWithCategoryManager type = %s, removing = %s\n",
              aMimeType.get(), aType == ePluginUnregister ? "yes" : "no"));

  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan) {
    return;
  }

  const char *contractId =
    "@mozilla.org/content/plugin/document-loader-factory;1";

  if (aType == ePluginRegister) {
    catMan->AddCategoryEntry("Gecko-Content-Viewers",
                             aMimeType.get(),
                             contractId,
                             false, /* persist: broken by bug 193031 */
                             mOverrideInternalTypes,
                             nullptr);
  } else {
    // Only delete the entry if a plugin registered for it
    nsXPIDLCString value;
    nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers",
                                           aMimeType.get(),
                                           getter_Copies(value));
    if (NS_SUCCEEDED(rv) && strcmp(value, contractId) == 0) {
      catMan->DeleteCategoryEntry("Gecko-Content-Viewers",
                                  aMimeType.get(),
                                  true);
    }
  }
}

nsresult
nsPluginHost::WritePluginInfo()
{

  nsresult rv = NS_OK;
  nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID,&rv));
  if (NS_FAILED(rv))
    return rv;

  directoryService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                        getter_AddRefs(mPluginRegFile));

  if (!mPluginRegFile)
    return NS_ERROR_FAILURE;

  PRFileDesc* fd = nullptr;

  nsCOMPtr<nsIFile> pluginReg;

  rv = mPluginRegFile->Clone(getter_AddRefs(pluginReg));
  if (NS_FAILED(rv))
    return rv;

  nsAutoCString filename(kPluginRegistryFilename);
  filename.Append(".tmp");
  rv = pluginReg->AppendNative(filename);
  if (NS_FAILED(rv))
    return rv;

  rv = pluginReg->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (!runtime) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString arch;
  rv = runtime->GetXPCOMABI(arch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PR_fprintf(fd, "Generated File. Do not edit.\n");

  PR_fprintf(fd, "\n[HEADER]\nVersion%c%s%c%c\nArch%c%s%c%c\n",
             PLUGIN_REGISTRY_FIELD_DELIMITER,
             kPluginRegistryVersion,
             PLUGIN_REGISTRY_FIELD_DELIMITER,
             PLUGIN_REGISTRY_END_OF_LINE_MARKER,
             PLUGIN_REGISTRY_FIELD_DELIMITER,
             arch.get(),
             PLUGIN_REGISTRY_FIELD_DELIMITER,
             PLUGIN_REGISTRY_END_OF_LINE_MARKER);

  // Store all plugins in the mPlugins list - all plugins currently in use.
  PR_fprintf(fd, "\n[PLUGINS]\n");

  for (nsPluginTag *tag = mPlugins; tag; tag = tag->mNext) {
    // store each plugin info into the registry
    // filename & fullpath are on separate line
    // because they can contain field delimiter char
    PR_fprintf(fd, "%s%c%c\n%s%c%c\n%s%c%c\n",
      (tag->mFileName.get()),
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      PLUGIN_REGISTRY_END_OF_LINE_MARKER,
      (tag->mFullPath.get()),
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      PLUGIN_REGISTRY_END_OF_LINE_MARKER,
      (tag->mVersion.get()),
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      PLUGIN_REGISTRY_END_OF_LINE_MARKER);

    // lastModifiedTimeStamp|canUnload|tag->mFlags
    PR_fprintf(fd, "%lld%c%d%c%lu%c%c\n",
      tag->mLastModifiedTime,
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      false, // did store whether or not to unload in-process plugins
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      0, // legacy field for flags
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      PLUGIN_REGISTRY_END_OF_LINE_MARKER);

    //description, name & mtypecount are on separate line
    PR_fprintf(fd, "%s%c%c\n%s%c%c\n%d\n",
      (tag->mDescription.get()),
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      PLUGIN_REGISTRY_END_OF_LINE_MARKER,
      (tag->mName.get()),
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      PLUGIN_REGISTRY_END_OF_LINE_MARKER,
      tag->mMimeTypes.Length());

    // Add in each mimetype this plugin supports
    for (uint32_t i = 0; i < tag->mMimeTypes.Length(); i++) {
      PR_fprintf(fd, "%d%c%s%c%s%c%s%c%c\n",
        i,PLUGIN_REGISTRY_FIELD_DELIMITER,
        (tag->mMimeTypes[i].get()),
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        (tag->mMimeDescriptions[i].get()),
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        (tag->mExtensions[i].get()),
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        PLUGIN_REGISTRY_END_OF_LINE_MARKER);
    }
  }

  PR_fprintf(fd, "\n[INVALID]\n");

  nsRefPtr<nsInvalidPluginTag> invalidPlugins = mInvalidPlugins;
  while (invalidPlugins) {
    // fullPath
    PR_fprintf(fd, "%s%c%c\n",
      (!invalidPlugins->mFullPath.IsEmpty() ? invalidPlugins->mFullPath.get() : ""),
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      PLUGIN_REGISTRY_END_OF_LINE_MARKER);

    // lastModifiedTimeStamp
    PR_fprintf(fd, "%lld%c%c\n",
      invalidPlugins->mLastModifiedTime,
      PLUGIN_REGISTRY_FIELD_DELIMITER,
      PLUGIN_REGISTRY_END_OF_LINE_MARKER);

    invalidPlugins = invalidPlugins->mNext;
  }

  PR_Close(fd);
  nsCOMPtr<nsIFile> parent;
  rv = pluginReg->GetParent(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = pluginReg->MoveToNative(parent, kPluginRegistryFilename);
  return rv;
}

nsresult
nsPluginHost::ReadPluginInfo()
{
  const long PLUGIN_REG_MIMETYPES_ARRAY_SIZE = 12;
  const long PLUGIN_REG_MAX_MIMETYPES = 1000;

  // we need to import the legacy flags from the plugin registry once
  const bool pluginStateImported =
    Preferences::GetDefaultBool("plugin.importedState", false);

  nsresult rv;

  nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID,&rv));
  if (NS_FAILED(rv))
    return rv;

  directoryService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                        getter_AddRefs(mPluginRegFile));

  if (!mPluginRegFile) {
    // There is no profile yet, this will tell us if there is going to be one
    // in the future.
    directoryService->Get(NS_APP_PROFILE_DIR_STARTUP, NS_GET_IID(nsIFile),
                          getter_AddRefs(mPluginRegFile));
    if (!mPluginRegFile)
      return NS_ERROR_FAILURE;
    else
      return NS_ERROR_NOT_AVAILABLE;
  }

  PRFileDesc* fd = nullptr;

  nsCOMPtr<nsIFile> pluginReg;

  rv = mPluginRegFile->Clone(getter_AddRefs(pluginReg));
  if (NS_FAILED(rv))
    return rv;

  rv = pluginReg->AppendNative(kPluginRegistryFilename);
  if (NS_FAILED(rv))
    return rv;

  int64_t fileSize;
  rv = pluginReg->GetFileSize(&fileSize);
  if (NS_FAILED(rv))
    return rv;

  if (fileSize > INT32_MAX) {
    return NS_ERROR_FAILURE;
  }
  int32_t flen = int32_t(fileSize);
  if (flen == 0) {
    NS_WARNING("Plugins Registry Empty!");
    return NS_OK; // ERROR CONDITION
  }

  nsPluginManifestLineReader reader;
  char* registry = reader.Init(flen);
  if (!registry)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = pluginReg->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd);
  if (NS_FAILED(rv))
    return rv;

  // set rv to return an error on goto out
  rv = NS_ERROR_FAILURE;

  int32_t bread = PR_Read(fd, registry, flen);
  PR_Close(fd);

  if (flen > bread)
    return rv;

  if (!ReadSectionHeader(reader, "HEADER"))
    return rv;;

  if (!reader.NextLine())
    return rv;

  char* values[6];

  // VersionLiteral, kPluginRegistryVersion
  if (2 != reader.ParseLine(values, 2))
    return rv;

  // VersionLiteral
  if (PL_strcmp(values[0], "Version"))
    return rv;

  // kPluginRegistryVersion
  int32_t vdiff = mozilla::CompareVersions(values[1], kPluginRegistryVersion);
  mozilla::Version version(values[1]);
  // If this is a registry from some future version then don't attempt to read it
  if (vdiff > 0)
    return rv;
  // If this is a registry from before the minimum then don't attempt to read it
  if (version < kMinimumRegistryVersion)
    return rv;

  // Registry v0.10 and upwards includes the plugin version field
  bool regHasVersion = (version >= "0.10");

  // Registry v0.13 and upwards includes the architecture
  if (version >= "0.13") {
    char* archValues[6];

    if (!reader.NextLine()) {
      return rv;
    }

    // ArchLiteral, Architecture
    if (2 != reader.ParseLine(archValues, 2)) {
      return rv;
    }

    // ArchLiteral
    if (PL_strcmp(archValues[0], "Arch")) {
      return rv;
    }

    nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
    if (!runtime) {
      return rv;
    }

    nsAutoCString arch;
    if (NS_FAILED(runtime->GetXPCOMABI(arch))) {
      return rv;
    }

    // If this is a registry from a different architecture then don't attempt to read it
    if (PL_strcmp(archValues[1], arch.get())) {
      return rv;
    }
  }

  // Registry v0.13 and upwards includes the list of invalid plugins
  bool hasInvalidPlugins = (version >= "0.13");

  // Registry v0.16 and upwards always have 0 for their plugin flags, prefs are used instead
  const bool hasValidFlags = (version < "0.16");

  if (!ReadSectionHeader(reader, "PLUGINS"))
    return rv;

#if defined(XP_MACOSX)
  bool hasFullPathInFileNameField = false;
#else
  bool hasFullPathInFileNameField = (version < "0.11");
#endif

  while (reader.NextLine()) {
    const char *filename;
    const char *fullpath;
    nsAutoCString derivedFileName;

    if (hasInvalidPlugins && *reader.LinePtr() == '[') {
      break;
    }

    if (hasFullPathInFileNameField) {
      fullpath = reader.LinePtr();
      if (!reader.NextLine())
        return rv;
      // try to derive a file name from the full path
      if (fullpath) {
        nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1");
        file->InitWithNativePath(nsDependentCString(fullpath));
        file->GetNativeLeafName(derivedFileName);
        filename = derivedFileName.get();
      } else {
        filename = NULL;
      }

      // skip the next line, useless in this version
      if (!reader.NextLine())
        return rv;
    } else {
      filename = reader.LinePtr();
      if (!reader.NextLine())
        return rv;

      fullpath = reader.LinePtr();
      if (!reader.NextLine())
        return rv;
    }

    const char *version;
    if (regHasVersion) {
      version = reader.LinePtr();
      if (!reader.NextLine())
        return rv;
    } else {
      version = "0";
    }

    // lastModifiedTimeStamp|canUnload|tag.mFlag
    if (reader.ParseLine(values, 3) != 3)
      return rv;

    // If this is an old plugin registry mark this plugin tag to be refreshed
    int64_t lastmod = (vdiff == 0) ? nsCRT::atoll(values[0]) : -1;
    uint32_t tagflag = atoi(values[2]);
    if (!reader.NextLine())
      return rv;

    char *description = reader.LinePtr();
    if (!reader.NextLine())
      return rv;

#if MOZ_WIDGET_ANDROID
    // Flash on Android does not populate the version field, but it is tacked on to the description.
    // For example, "Shockwave Flash 11.1 r115"
    if (PL_strncmp("Shockwave Flash ", description, 16) == 0 && description[16]) {
      version = &description[16];
    }
#endif

    const char *name = reader.LinePtr();
    if (!reader.NextLine())
      return rv;

    long mimetypecount = std::strtol(reader.LinePtr(), nullptr, 10);
    if (mimetypecount == LONG_MAX || mimetypecount == LONG_MIN ||
        mimetypecount >= PLUGIN_REG_MAX_MIMETYPES || mimetypecount < 0) {
      return NS_ERROR_FAILURE;
    }

    char *stackalloced[PLUGIN_REG_MIMETYPES_ARRAY_SIZE * 3];
    char **mimetypes;
    char **mimedescriptions;
    char **extensions;
    char **heapalloced = 0;
    if (mimetypecount > PLUGIN_REG_MIMETYPES_ARRAY_SIZE - 1) {
      heapalloced = new char *[mimetypecount * 3];
      mimetypes = heapalloced;
    } else {
      mimetypes = stackalloced;
    }
    mimedescriptions = mimetypes + mimetypecount;
    extensions = mimedescriptions + mimetypecount;

    int mtr = 0; //mimetype read
    for (; mtr < mimetypecount; mtr++) {
      if (!reader.NextLine())
        break;

      //line number|mimetype|description|extension
      if (4 != reader.ParseLine(values, 4))
        break;
      int line = atoi(values[0]);
      if (line != mtr)
        break;
      mimetypes[mtr] = values[1];
      mimedescriptions[mtr] = values[2];
      extensions[mtr] = values[3];
    }

    if (mtr != mimetypecount) {
      if (heapalloced) {
        delete [] heapalloced;
      }
      return rv;
    }

    nsRefPtr<nsPluginTag> tag = new nsPluginTag(name,
      description,
      filename,
      fullpath,
      version,
      (const char* const*)mimetypes,
      (const char* const*)mimedescriptions,
      (const char* const*)extensions,
      mimetypecount, lastmod, true);
    if (heapalloced)
      delete [] heapalloced;

    // Import flags from registry into prefs for old registry versions
    if (hasValidFlags && !pluginStateImported) {
      tag->ImportFlagsToPrefs(tagflag);
    }

    PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_BASIC,
      ("LoadCachedPluginsInfo : Loading Cached plugininfo for %s\n", tag->mFileName.get()));
    tag->mNext = mCachedPlugins;
    mCachedPlugins = tag;
  }

  if (hasInvalidPlugins) {
    if (!ReadSectionHeader(reader, "INVALID")) {
      return rv;
    }

    while (reader.NextLine()) {
      const char *fullpath = reader.LinePtr();
      if (!reader.NextLine()) {
        return rv;
      }

      const char *lastModifiedTimeStamp = reader.LinePtr();
      int64_t lastmod = (vdiff == 0) ? nsCRT::atoll(lastModifiedTimeStamp) : -1;

      nsRefPtr<nsInvalidPluginTag> invalidTag = new nsInvalidPluginTag(fullpath, lastmod);

      invalidTag->mNext = mInvalidPlugins;
      if (mInvalidPlugins) {
        mInvalidPlugins->mPrev = invalidTag;
      }
      mInvalidPlugins = invalidTag;
    }
  }

  // flip the pref so we don't import the legacy flags again
  Preferences::SetBool("plugin.importedState", true);

  return NS_OK;
}

void
nsPluginHost::RemoveCachedPluginsInfo(const char *filePath, nsPluginTag **result)
{
  nsRefPtr<nsPluginTag> prev;
  nsRefPtr<nsPluginTag> tag = mCachedPlugins;
  while (tag)
  {
    if (tag->mFullPath.Equals(filePath)) {
      // Found it. Remove it from our list
      if (prev)
        prev->mNext = tag->mNext;
      else
        mCachedPlugins = tag->mNext;
      tag->mNext = nullptr;
      *result = tag;
      NS_ADDREF(*result);
      break;
    }
    prev = tag;
    tag = tag->mNext;
  }
}

#ifdef XP_WIN
nsresult
nsPluginHost::EnsurePrivateDirServiceProvider()
{
  if (!mPrivateDirServiceProvider) {
    nsresult rv;
    mPrivateDirServiceProvider = new nsPluginDirServiceProvider();
    if (!mPrivateDirServiceProvider)
      return NS_ERROR_OUT_OF_MEMORY;
    nsCOMPtr<nsIDirectoryService> dirService(do_GetService(kDirectoryServiceContractID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = dirService->RegisterProvider(mPrivateDirServiceProvider);
    if (NS_FAILED(rv))
      return rv;
  }
  return NS_OK;
}
#endif /* XP_WIN */

nsresult nsPluginHost::NewPluginURLStream(const nsString& aURL,
                                          nsNPAPIPluginInstance *aInstance,
                                          nsNPAPIPluginStreamListener* aListener,
                                          nsIInputStream *aPostStream,
                                          const char *aHeadersData,
                                          uint32_t aHeadersDataLen)
{
  nsCOMPtr<nsIURI> url;
  nsAutoString absUrl;
  nsresult rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  // get the base URI for the plugin to create an absolute url 
  // in case aURL is relative
  nsRefPtr<nsPluginInstanceOwner> owner = aInstance->GetOwner();
  if (owner) {
    rv = NS_MakeAbsoluteURI(absUrl, aURL, nsCOMPtr<nsIURI>(owner->GetBaseURI()));
  }

  if (absUrl.IsEmpty())
    absUrl.Assign(aURL);

  rv = NS_NewURI(getter_AddRefs(url), absUrl);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMElement> element;
  nsCOMPtr<nsIDocument> doc;
  if (owner) {
    owner->GetDOMElement(getter_AddRefs(element));
    owner->GetDocument(getter_AddRefs(doc));
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OBJECT_SUBREQUEST,
                                 url,
                                 (doc ? doc->NodePrincipal() : nullptr),
                                 element,
                                 EmptyCString(), //mime guess
                                 nullptr,         //extra
                                 &shouldLoad);
  if (NS_FAILED(rv))
    return rv;
  if (NS_CP_REJECTED(shouldLoad)) {
    // Disallowed by content policy
    return NS_ERROR_CONTENT_BLOCKED;
  }

  nsRefPtr<nsPluginStreamListenerPeer> listenerPeer = new nsPluginStreamListenerPeer();
  if (!listenerPeer)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = listenerPeer->Initialize(url, aInstance, aListener);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), url, nullptr,
    nullptr, /* do not add this internal plugin's channel
            on the load group otherwise this channel could be canceled
            form |nsDocShell::OnLinkClickSync| bug 166613 */
    listenerPeer);
  if (NS_FAILED(rv))
    return rv;

  if (doc) {
    // Set the owner of channel to the document principal...
    channel->SetOwner(doc->NodePrincipal());

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

      nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(element);
      if (olc)
        olc->GetSrcURI(getter_AddRefs(referer));


      if (!referer) {
        if (!doc) {
          return NS_ERROR_FAILURE;
        }
        referer = doc->GetDocumentURI();
      }

      rv = httpChannel->SetReferrer(referer);
      NS_ENSURE_SUCCESS(rv,rv);
    }

    if (aPostStream) {
      // XXX it's a bit of a hack to rewind the postdata stream
      // here but it has to be done in case the post data is
      // being reused multiple times.
      nsCOMPtr<nsISeekableStream>
      postDataSeekable(do_QueryInterface(aPostStream));
      if (postDataSeekable)
        postDataSeekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);

      nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
      NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

      uploadChannel->SetUploadStream(aPostStream, EmptyCString(), -1);
    }

    if (aHeadersData) {
      rv = AddHeadersToChannel(aHeadersData, aHeadersDataLen, httpChannel);
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  rv = channel->AsyncOpen(listenerPeer, nullptr);
  if (NS_SUCCEEDED(rv))
    listenerPeer->TrackRequest(channel);
  return rv;
}

// Called by GetURL and PostURL
nsresult
nsPluginHost::DoURLLoadSecurityCheck(nsNPAPIPluginInstance *aInstance,
                                     const char* aURL)
{
  if (!aURL || *aURL == '\0')
    return NS_OK;

  // get the base URI for the plugin element
  nsRefPtr<nsPluginInstanceOwner> owner = aInstance->GetOwner();
  if (!owner)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> baseURI = owner->GetBaseURI();
  if (!baseURI)
    return NS_ERROR_FAILURE;

  // Create an absolute URL for the target in case the target is relative
  nsCOMPtr<nsIURI> targetURL;
  NS_NewURI(getter_AddRefs(targetURL), aURL, baseURI);
  if (!targetURL)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> secMan(
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return secMan->CheckLoadURIWithPrincipal(doc->NodePrincipal(), targetURL,
                                           nsIScriptSecurityManager::STANDARD);

}

nsresult
nsPluginHost::AddHeadersToChannel(const char *aHeadersData,
                                  uint32_t aHeadersDataLen,
                                  nsIChannel *aGenericChannel)
{
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
  return rv;
}

nsresult
nsPluginHost::StopPluginInstance(nsNPAPIPluginInstance* aInstance)
{
  if (PluginDestructionGuard::DelayDestroy(aInstance)) {
    return NS_OK;
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHost::StopPluginInstance called instance=%p\n",aInstance));

  if (aInstance->HasStartedDestroying()) {
    return NS_OK;
  }

  Telemetry::AutoTimer<Telemetry::PLUGIN_SHUTDOWN_MS> timer;
  aInstance->Stop();

  // if the instance does not want to be 'cached' just remove it
  bool doCache = aInstance->ShouldCache();
  if (doCache) {
    // try to get the max cached instances from a pref or use default
    uint32_t cachedInstanceLimit =
      Preferences::GetUint(NS_PREF_MAX_NUM_CACHED_INSTANCES,
                           DEFAULT_NUMBER_OF_STOPPED_INSTANCES);
    if (StoppedInstanceCount() >= cachedInstanceLimit) {
      nsNPAPIPluginInstance *oldestInstance = FindOldestStoppedInstance();
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

nsresult nsPluginHost::NewPluginStreamListener(nsIURI* aURI,
                                               nsNPAPIPluginInstance* aInstance,
                                               nsIStreamListener **aStreamListener)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aStreamListener);

  nsRefPtr<nsPluginStreamListenerPeer> listener = new nsPluginStreamListenerPeer();
  nsresult rv = listener->Initialize(aURI, aInstance, nullptr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  listener.forget(aStreamListener);

  return NS_OK;
}

NS_IMETHODIMP nsPluginHost::Observe(nsISupports *aSubject,
                                    const char *aTopic,
                                    const PRUnichar *someData)
{
  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic)) {
    OnShutdown();
    UnloadPlugins();
    sInst->Release();
  }
  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    mPluginsDisabled = Preferences::GetBool("plugin.disable", false);
    mPluginsClickToPlay = Preferences::GetBool("plugins.click_to_play", false);
    // Unload or load plugins as needed
    if (mPluginsDisabled) {
      UnloadPlugins();
    } else {
      LoadPlugins();
    }
  }
  if (!strcmp("blocklist-updated", aTopic)) {
    nsPluginTag* plugin = mPlugins;
    while (plugin) {
      plugin->InvalidateBlocklistState();
      plugin = plugin->mNext;
    }
  }
#ifdef MOZ_WIDGET_ANDROID
  if (!strcmp("application-background", aTopic)) {
    for(uint32_t i = 0; i < mInstances.Length(); i++) {
      mInstances[i]->NotifyForeground(false);
    }
  }
  if (!strcmp("application-foreground", aTopic)) {
    for(uint32_t i = 0; i < mInstances.Length(); i++) {
      if (mInstances[i]->IsOnScreen())
        mInstances[i]->NotifyForeground(true);
    }
  }
  if (!strcmp("memory-pressure", aTopic)) {
    for(uint32_t i = 0; i < mInstances.Length(); i++) {
      mInstances[i]->MemoryPressure();
    }
  }
#endif
  return NS_OK;
}

nsresult
nsPluginHost::ParsePostBufferToFixHeaders(const char *inPostData, uint32_t inPostDataLen,
                                          char **outPostData, uint32_t *outPostDataLen)
{
  if (!inPostData || !outPostData || !outPostDataLen)
    return NS_ERROR_NULL_POINTER;

  *outPostData = 0;
  *outPostDataLen = 0;

  const char CR = '\r';
  const char LF = '\n';
  const char CRLFCRLF[] = {CR,LF,CR,LF,'\0'}; // C string"\r\n\r\n"
  const char ContentLenHeader[] = "Content-length";

  nsAutoTArray<const char*, 8> singleLF;
  const char *pSCntlh = 0;// pointer to start of ContentLenHeader in inPostData
  const char *pSod = 0;   // pointer to start of data in inPostData
  const char *pEoh = 0;   // pointer to end of headers in inPostData
  const char *pEod = inPostData + inPostDataLen; // pointer to end of inPostData
  if (*inPostData == LF) {
    // If no custom headers are required, simply add a blank
    // line ('\n') to the beginning of the file or buffer.
    // so *inPostData == '\n' is valid
    pSod = inPostData + 1;
  } else {
    const char *s = inPostData; //tmp pointer to sourse inPostData
    while (s < pEod) {
      if (!pSCntlh &&
          (*s == 'C' || *s == 'c') &&
          (s + sizeof(ContentLenHeader) - 1 < pEod) &&
          (!PL_strncasecmp(s, ContentLenHeader, sizeof(ContentLenHeader) - 1)))
      {
        // lets assume this is ContentLenHeader for now
        const char *p = pSCntlh = s;
        p += sizeof(ContentLenHeader) - 1;
        // search for first CR or LF == end of ContentLenHeader
        for (; p < pEod; p++) {
          if (*p == CR || *p == LF) {
            // got delimiter,
            // one more check; if previous char is a digit
            // most likely pSCntlh points to the start of ContentLenHeader
            if (*(p-1) >= '0' && *(p-1) <= '9') {
              s = p;
            }
            break; //for loop
          }
        }
        if (pSCntlh == s) { // curret ptr is the same
          pSCntlh = 0; // that was not ContentLenHeader
          break; // there is nothing to parse, break *WHILE LOOP* here
        }
      }

      if (*s == CR) {
        if (pSCntlh && // only if ContentLenHeader is found we are looking for end of headers
            ((s + sizeof(CRLFCRLF)-1) <= pEod) &&
            !memcmp(s, CRLFCRLF, sizeof(CRLFCRLF)-1))
        {
          s += sizeof(CRLFCRLF)-1;
          pEoh = pSod = s; // data stars here
          break;
        }
      } else if (*s == LF) {
        if (*(s-1) != CR) {
          singleLF.AppendElement(s);
        }
        if (pSCntlh && (s+1 < pEod) && (*(s+1) == LF)) {
          s++;
          singleLF.AppendElement(s);
          s++;
          pEoh = pSod = s; // data stars here
          break;
        }
      }
      s++;
    }
  }

  // deal with output buffer
  if (!pSod) { // lets assume whole buffer is a data
    pSod = inPostData;
  }

  uint32_t newBufferLen = 0;
  uint32_t dataLen = pEod - pSod;
  uint32_t headersLen = pEoh ? pSod - inPostData : 0;

  char *p; // tmp ptr into new output buf
  if (headersLen) { // we got a headers
    // this function does not make any assumption on correctness
    // of ContentLenHeader value in this case.

    newBufferLen = dataLen + headersLen;
    // in case there were single LFs in headers
    // reserve an extra space for CR will be added before each single LF
    int cntSingleLF = singleLF.Length();
    newBufferLen += cntSingleLF;

    if (!(*outPostData = p = (char*)nsMemory::Alloc(newBufferLen)))
      return NS_ERROR_OUT_OF_MEMORY;

    // deal with single LF
    const char *s = inPostData;
    if (cntSingleLF) {
      for (int i=0; i<cntSingleLF; i++) {
        const char *plf = singleLF.ElementAt(i); // ptr to single LF in headers
        int n = plf - s; // bytes to copy
        if (n) { // for '\n\n' there is nothing to memcpy
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
    if (headersLen) { // not yet
      memcpy(p, s, headersLen); // copy the rest
      p += headersLen;
    }
  } else  if (dataLen) { // no ContentLenHeader is found but there is a data
    // make new output buffer big enough
    // to keep ContentLenHeader+value followed by data
    uint32_t l = sizeof(ContentLenHeader) + sizeof(CRLFCRLF) + 32;
    newBufferLen = dataLen + l;
    if (!(*outPostData = p = (char*)nsMemory::Alloc(newBufferLen)))
      return NS_ERROR_OUT_OF_MEMORY;
    headersLen = PR_snprintf(p, l,"%s: %ld%s", ContentLenHeader, dataLen, CRLFCRLF);
    if (headersLen == l) { // if PR_snprintf has ate all extra space consider this as an error
      nsMemory::Free(p);
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

nsresult
nsPluginHost::CreateTempFileToPost(const char *aPostDataURL, nsIFile **aTmpFile)
{
  nsresult rv;
  int64_t fileSize;
  nsAutoCString filename;

  // stat file == get size & convert file:///c:/ to c: if needed
  nsCOMPtr<nsIFile> inFile;
  rv = NS_GetFileFromURLSpec(nsDependentCString(aPostDataURL),
                             getter_AddRefs(inFile));
  if (NS_FAILED(rv)) {
    nsCOMPtr<nsIFile> localFile;
    rv = NS_NewNativeLocalFile(nsDependentCString(aPostDataURL), false,
                               getter_AddRefs(localFile));
    if (NS_FAILED(rv)) return rv;
    inFile = localFile;
  }
  rv = inFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) return rv;
  rv = inFile->GetNativePath(filename);
  if (NS_FAILED(rv)) return rv;

  if (fileSize != 0) {
    nsCOMPtr<nsIInputStream> inStream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inStream), inFile);
    if (NS_FAILED(rv)) return rv;

    // Create a temporary file to write the http Content-length:
    // %ld\r\n\" header and "\r\n" == end of headers for post data to

    nsCOMPtr<nsIFile> tempFile;
    rv = GetPluginTempDir(getter_AddRefs(tempFile));
    if (NS_FAILED(rv))
      return rv;

    nsAutoCString inFileName;
    inFile->GetNativeLeafName(inFileName);
    // XXX hack around bug 70083
    inFileName.Insert(NS_LITERAL_CSTRING("post-"), 0);
    rv = tempFile->AppendNative(inFileName);

    if (NS_FAILED(rv))
      return rv;

    // make it unique, and mode == 0600, not world-readable
    rv = tempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIOutputStream> outStream;
    if (NS_SUCCEEDED(rv)) {
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outStream),
        tempFile,
        (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE),
        0600); // 600 so others can't read our form data
    }
    NS_ASSERTION(NS_SUCCEEDED(rv), "Post data file couldn't be created!");
    if (NS_FAILED(rv))
      return rv;

    char buf[1024];
    uint32_t br, bw;
    bool firstRead = true;
    while (1) {
      // Read() mallocs if buffer is null
      rv = inStream->Read(buf, 1024, &br);
      if (NS_FAILED(rv) || (int32_t)br <= 0)
        break;
      if (firstRead) {
        //"For protocols in which the headers must be distinguished from the body,
        // such as HTTP, the buffer or file should contain the headers, followed by
        // a blank line, then the body. If no custom headers are required, simply
        // add a blank line ('\n') to the beginning of the file or buffer.

        char *parsedBuf;
        // assuming first 1K (or what we got) has all headers in,
        // lets parse it through nsPluginHost::ParsePostBufferToFixHeaders()
        ParsePostBufferToFixHeaders((const char *)buf, br, &parsedBuf, &bw);
        rv = outStream->Write(parsedBuf, bw, &br);
        nsMemory::Free(parsedBuf);
        if (NS_FAILED(rv) || (bw != br))
          break;

        firstRead = false;
        continue;
      }
      bw = br;
      rv = outStream->Write(buf, bw, &br);
      if (NS_FAILED(rv) || (bw != br))
        break;
    }

    inStream->Close();
    outStream->Close();
    if (NS_SUCCEEDED(rv))
      *aTmpFile = tempFile.forget().get();
  }
  return rv;
}

nsresult
nsPluginHost::NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  return PLUG_NewPluginNativeWindow(aPluginNativeWindow);
}

nsresult
nsPluginHost::GetPluginName(nsNPAPIPluginInstance *aPluginInstance,
                            const char** aPluginName)
{
  nsNPAPIPluginInstance *instance = static_cast<nsNPAPIPluginInstance*>(aPluginInstance);
  if (!instance)
    return NS_ERROR_FAILURE;

  nsNPAPIPlugin* plugin = instance->GetPlugin();
  if (!plugin)
    return NS_ERROR_FAILURE;

  *aPluginName = TagForPlugin(plugin)->mName.get();

  return NS_OK;
}

nsresult
nsPluginHost::GetPluginTagForInstance(nsNPAPIPluginInstance *aPluginInstance,
                                      nsIPluginTag **aPluginTag)
{
  NS_ENSURE_ARG_POINTER(aPluginInstance);
  NS_ENSURE_ARG_POINTER(aPluginTag);

  nsNPAPIPlugin *plugin = aPluginInstance->GetPlugin();
  if (!plugin)
    return NS_ERROR_FAILURE;

  *aPluginTag = TagForPlugin(plugin);

  NS_ADDREF(*aPluginTag);
  return NS_OK;
}

NS_IMETHODIMP nsPluginHost::Notify(nsITimer* timer)
{
  nsRefPtr<nsPluginTag> pluginTag = mPlugins;
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

#ifdef XP_WIN
// Re-enable any top level browser windows that were disabled by modal dialogs
// displayed by the crashed plugin.
static void
CheckForDisabledWindows()
{
  nsCOMPtr<nsIWindowMediator> wm(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!wm)
    return;

  nsCOMPtr<nsISimpleEnumerator> windowList;
  wm->GetXULWindowEnumerator(nullptr, getter_AddRefs(windowList));
  if (!windowList)
    return;

  bool haveWindows;
  do {
    windowList->HasMoreElements(&haveWindows);
    if (!haveWindows)
      return;

    nsCOMPtr<nsISupports> supportsWindow;
    windowList->GetNext(getter_AddRefs(supportsWindow));
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(supportsWindow));
    if (baseWin) {
      nsCOMPtr<nsIWidget> widget;
      baseWin->GetMainWidget(getter_AddRefs(widget));
      if (widget && !widget->GetParent() &&
          widget->IsVisible() &&
          !widget->IsEnabled()) {
        nsIWidget* child = widget->GetFirstChild();
        bool enable = true;
        while (child)  {
          nsWindowType aType;
          if (NS_SUCCEEDED(child->GetWindowType(aType)) &&
              aType == eWindowType_dialog) {
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

void
nsPluginHost::PluginCrashed(nsNPAPIPlugin* aPlugin,
                            const nsAString& pluginDumpID,
                            const nsAString& browserDumpID)
{
  nsPluginTag* crashedPluginTag = TagForPlugin(aPlugin);

  // Notify the app's observer that a plugin crashed so it can submit
  // a crashreport.
  bool submittedCrashReport = false;
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  nsCOMPtr<nsIWritablePropertyBag2> propbag =
    do_CreateInstance("@mozilla.org/hash-property-bag;1");
  if (obsService && propbag) {
    propbag->SetPropertyAsAString(NS_LITERAL_STRING("pluginDumpID"),
                                  pluginDumpID);
    propbag->SetPropertyAsAString(NS_LITERAL_STRING("browserDumpID"),
                                  browserDumpID);
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
      nsCOMPtr<nsIDOMElement> domElement;
      instance->GetDOMElement(getter_AddRefs(domElement));
      nsCOMPtr<nsIObjectLoadingContent> objectContent(do_QueryInterface(domElement));
      if (objectContent) {
        objectContent->PluginCrashed(crashedPluginTag, pluginDumpID, browserDumpID,
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

#ifdef XP_WIN
  CheckForDisabledWindows();
#endif
}

nsNPAPIPluginInstance*
nsPluginHost::FindInstance(const char *mimetype)
{
  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance* instance = mInstances[i];

    const char* mt;
    nsresult rv = instance->GetMIMEType(&mt);
    if (NS_FAILED(rv))
      continue;

    if (PL_strcasecmp(mt, mimetype) == 0)
      return instance;
  }

  return nullptr;
}

nsNPAPIPluginInstance*
nsPluginHost::FindOldestStoppedInstance()
{
  nsNPAPIPluginInstance *oldestInstance = nullptr;
  TimeStamp oldestTime = TimeStamp::Now();
  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance *instance = mInstances[i];
    if (instance->IsRunning())
      continue;

    TimeStamp time = instance->StopTime();
    if (time < oldestTime) {
      oldestTime = time;
      oldestInstance = instance;
    }
  }

  return oldestInstance;
}

uint32_t
nsPluginHost::StoppedInstanceCount()
{
  uint32_t stoppedCount = 0;
  for (uint32_t i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance *instance = mInstances[i];
    if (!instance->IsRunning())
      stoppedCount++;
  }
  return stoppedCount;
}

nsTArray< nsRefPtr<nsNPAPIPluginInstance> >*
nsPluginHost::InstanceArray()
{
  return &mInstances;
}

void 
nsPluginHost::DestroyRunningInstances(nsPluginTag* aPluginTag)
{
  for (int32_t i = mInstances.Length(); i > 0; i--) {
    nsNPAPIPluginInstance *instance = mInstances[i - 1];
    if (instance->IsRunning() && (!aPluginTag || aPluginTag == TagForPlugin(instance->GetPlugin()))) {
      instance->SetWindow(nullptr);
      instance->Stop();

      // Get rid of all the instances without the possibility of caching.
      nsPluginTag* pluginTag = TagForPlugin(instance->GetPlugin());
      instance->SetWindow(nullptr);

      nsCOMPtr<nsIDOMElement> domElement;
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

// Runnable that does an async destroy of a plugin.

class nsPluginDestroyRunnable : public nsRunnable,
                                public PRCList
{
public:
  nsPluginDestroyRunnable(nsNPAPIPluginInstance *aInstance)
    : mInstance(aInstance)
  {
    PR_INIT_CLIST(this);
    PR_APPEND_LINK(this, &sRunnableListHead);
  }

  virtual ~nsPluginDestroyRunnable()
  {
    PR_REMOVE_LINK(this);
  }

  NS_IMETHOD Run()
  {
    nsRefPtr<nsNPAPIPluginInstance> instance;

    // Null out mInstance to make sure this code in another runnable
    // will do the right thing even if someone was holding on to this
    // runnable longer than we expect.
    instance.swap(mInstance);

    if (PluginDestructionGuard::DelayDestroy(instance)) {
      // It's still not safe to destroy the plugin, it's now up to the
      // outermost guard on the stack to take care of the destruction.
      return NS_OK;
    }

    nsPluginDestroyRunnable *r =
      static_cast<nsPluginDestroyRunnable*>(PR_NEXT_LINK(&sRunnableListHead));

    while (r != &sRunnableListHead) {
      if (r != this && r->mInstance == instance) {
        // There's another runnable scheduled to tear down
        // instance. Let it do the job.
        return NS_OK;
      }
      r = static_cast<nsPluginDestroyRunnable*>(PR_NEXT_LINK(r));
    }

    PLUGIN_LOG(PLUGIN_LOG_NORMAL,
               ("Doing delayed destroy of instance %p\n", instance.get()));

    nsRefPtr<nsPluginHost> host = nsPluginHost::GetInst();
    if (host)
      host->StopPluginInstance(instance);

    PLUGIN_LOG(PLUGIN_LOG_NORMAL,
               ("Done with delayed destroy of instance %p\n", instance.get()));

    return NS_OK;
  }

protected:
  nsRefPtr<nsNPAPIPluginInstance> mInstance;

  static PRCList sRunnableListHead;
};

PRCList nsPluginDestroyRunnable::sRunnableListHead =
  PR_INIT_STATIC_CLIST(&nsPluginDestroyRunnable::sRunnableListHead);

PRCList PluginDestructionGuard::sListHead =
  PR_INIT_STATIC_CLIST(&PluginDestructionGuard::sListHead);

PluginDestructionGuard::~PluginDestructionGuard()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread");

  PR_REMOVE_LINK(this);

  if (mDelayedDestroy) {
    // We've attempted to destroy the plugin instance we're holding on
    // to while we were guarding it. Do the actual destroy now, off of
    // a runnable.
    nsRefPtr<nsPluginDestroyRunnable> evt =
      new nsPluginDestroyRunnable(mInstance);

    NS_DispatchToMainThread(evt);
  }
}

// static
bool
PluginDestructionGuard::DelayDestroy(nsNPAPIPluginInstance *aInstance)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread");
  NS_ASSERTION(aInstance, "Uh, I need an instance!");

  // Find the first guard on the stack and make it do a delayed
  // destroy upon destruction.

  PluginDestructionGuard *g =
    static_cast<PluginDestructionGuard*>(PR_LIST_HEAD(&sListHead));

  while (g != &sListHead) {
    if (g->mInstance == aInstance) {
      g->mDelayedDestroy = true;

      return true;
    }
    g = static_cast<PluginDestructionGuard*>(PR_NEXT_LINK(g));
  }

  return false;
}

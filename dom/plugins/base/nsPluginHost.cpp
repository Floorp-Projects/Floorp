/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Echevarria <sean@beatnik.com>
 *   Håkan Waara <hwaara@chello.se>
 *   Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* nsPluginHost.cpp - top-level plugin management code */

#include "nscore.h"
#include "nsPluginHost.h"

#include <stdio.h>
#include "prio.h"
#include "prmem.h"
#include "nsIComponentManager.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIPluginStreamListener.h"
#include "nsIHTTPHeaderListener.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIObserverService.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIUploadChannel.h"
#include "nsIByteRangeRequest.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIURL.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIProtocolProxyService.h"
#include "nsIStreamConverterService.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsISeekableStream.h"
#include "nsNetUtil.h"
#include "nsIProgressEventSink.h"
#include "nsIDocument.h"
#include "nsICachingChannel.h"
#include "nsHashtable.h"
#include "nsIProxyInfo.h"
#include "nsPluginLogging.h"
#include "nsIPrefBranch.h"
#include "nsIScriptChannel.h"
#include "nsIBlocklistService.h"
#include "nsVersionComparator.h"
#include "nsIPrivateBrowsingService.h"
#include "nsIObjectLoadingContent.h"
#include "nsIWritablePropertyBag2.h"
#include "nsPluginStreamListenerPeer.h"

#include "nsEnumeratorUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsISupportsPrimitives.h"

#include "nsXULAppAPI.h"
#include "nsIXULRuntime.h"

// for the dialog
#include "nsIStringBundle.h"
#include "nsIWindowWatcher.h"
#include "nsPIDOMWindow.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIPrincipal.h"

#include "nsNetCID.h"
#include "nsIDOMPlugin.h"
#include "nsIDOMMimeType.h"
#include "nsMimeTypes.h"
#include "prprf.h"
#include "nsThreadUtils.h"
#include "nsIInputStreamTee.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"

#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsILocalFile.h"
#include "nsIFileChannel.h"

#include "nsPluginSafety.h"

#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"

#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsPluginDirServiceProvider.h"
#include "nsPluginError.h"

#include "nsUnicharUtils.h"
#include "nsPluginManifestLineReader.h"

#include "nsIWeakReferenceUtils.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIPresShell.h"
#include "nsIWebNavigation.h"
#include "nsISupportsArray.h"
#include "nsIDocShell.h"
#include "nsPluginNativeWindow.h"
#include "nsIScriptSecurityManager.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "nsIImageLoadingContent.h"
#include "mozilla/Preferences.h"

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

using namespace mozilla;
using mozilla::TimeStamp;

// Null out a strong ref to a linked list iteratively to avoid
// exhausting the stack (bug 486349).
#define NS_ITERATIVE_UNREF_LIST(type_, list_, mNext_)                \
  {                                                                  \
    while (list_) {                                                  \
      type_ temp = list_->mNext_;                                    \
      list_->mNext_ = nsnull;                                        \
      list_ = temp;                                                  \
    }                                                                \
  }

// this is the name of the directory which will be created
// to cache temporary files.
#define kPluginTmpDirName NS_LITERAL_CSTRING("plugtmp")

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
// The current plugin registry version (and the maximum version we know how to read)
static const char *kPluginRegistryVersion = "0.15";
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
PRLogModuleInfo* nsPluginLogging::gNPNLog = nsnull;
PRLogModuleInfo* nsPluginLogging::gNPPLog = nsnull;
PRLogModuleInfo* nsPluginLogging::gPluginLog = nsnull;
#endif

#define BRAND_PROPERTIES_URL "chrome://branding/locale/brand.properties"
#define PLUGIN_PROPERTIES_URL "chrome://global/locale/downloadProgress.properties"

// #defines for plugin cache and prefs
#define NS_PREF_MAX_NUM_CACHED_INSTANCES "browser.plugins.max_num_cached_plugins"
// Raise this from '10' to '50' to work around a bug in Apple's current Java
// plugins on OS X Lion and SnowLeopard.  See bug 705931.
#define DEFAULT_NUMBER_OF_STOPPED_INSTANCES 50

#ifdef CALL_SAFETY_ON
// By default we run OOPP, so we don't want to cover up crashes.
bool gSkipPluginSafeCalls = true;
#endif

nsIFile *nsPluginHost::sPluginTempDir;
nsPluginHost *nsPluginHost::sInst;

NS_IMPL_ISUPPORTS0(nsInvalidPluginTag)

nsInvalidPluginTag::nsInvalidPluginTag(const char* aFullPath, PRInt64 aLastModifiedTime)
: mFullPath(aFullPath),
  mLastModifiedTime(aLastModifiedTime),
  mSeen(false)
{
  
}

nsInvalidPluginTag::~nsInvalidPluginTag()
{
  
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

// Little helper struct to asynchronously reframe any presentations (embedded)
// or reload any documents (full-page), that contained plugins
// which were shutdown as a result of a plugins.refresh(1)
class nsPluginDocReframeEvent: public nsRunnable {
public:
  nsPluginDocReframeEvent(nsISupportsArray* aDocs) { mDocs = aDocs; }

  NS_DECL_NSIRUNNABLE

  nsCOMPtr<nsISupportsArray> mDocs;
};

NS_IMETHODIMP nsPluginDocReframeEvent::Run() {
  NS_ENSURE_TRUE(mDocs, NS_ERROR_FAILURE);

  PRUint32 c;
  mDocs->Count(&c);

  // for each document (which previously had a running instance), tell
  // the frame constructor to rebuild
  for (PRUint32 i = 0; i < c; i++) {
    nsCOMPtr<nsIDocument> doc (do_QueryElementAt(mDocs, i));
    if (doc) {
      nsIPresShell *shell = doc->GetShell();

      // if this document has a presentation shell, then it has frames and can be reframed
      if (shell) {
        /* A reframe will cause a fresh object frame, instance owner, and instance
         * to be created. Reframing of the entire document is necessary as we may have
         * recently found new plugins and we want a shot at trying to use them instead
         * of leaving alternate renderings.
         * We do not want to completely reload all the documents that had running plugins
         * because we could possibly trigger a script to run in the unload event handler
         * which may want to access our defunct plugin and cause us to crash.
         */

        shell->ReconstructFrames(); // causes reframe of document
      } else {  // no pres shell --> full-page plugin

        NS_NOTREACHED("all plugins should have a pres shell!");

      }
    }
  }

  return mDocs->Clear();
}

static bool UnloadPluginsASAP()
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    bool unloadPluginsASAP = false;
    rv = pref->GetBoolPref("dom.ipc.plugins.unloadASAP", &unloadPluginsASAP);
    if (NS_SUCCEEDED(rv)) {
      return unloadPluginsASAP;
    }
  }

  return false;
}

nsPluginHost::nsPluginHost()
  // No need to initialize members to nsnull, false etc because this class
  // has a zeroing operator new.
{
  // check to see if pref is set at startup to let plugins take over in
  // full page mode for certain image mime types that we handle internally
  mPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (mPrefService) {
    bool tmp;
    nsresult rv = mPrefService->GetBoolPref("plugin.override_internal_types",
                                            &tmp);
    if (NS_SUCCEEDED(rv)) {
      mOverrideInternalTypes = tmp;
    }

    rv = mPrefService->GetBoolPref("plugin.disable", &tmp);
    if (NS_SUCCEEDED(rv)) {
      mPluginsDisabled = tmp;
    }
  }

  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService) {
    obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    obsService->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC, false);
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

#ifdef MAC_CARBON_PLUGINS
  mVisiblePluginTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  mHiddenPluginTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
#endif
}

nsPluginHost::~nsPluginHost()
{
  PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("nsPluginHost::dtor\n"));

  Destroy();
  sInst = nsnull;
}

NS_IMPL_ISUPPORTS4(nsPluginHost,
                   nsIPluginHost,
                   nsIObserver,
                   nsITimerCallback,
                   nsISupportsWeakReference)

nsPluginHost*
nsPluginHost::GetInst()
{
  if (!sInst) {
    sInst = new nsPluginHost();
    if (!sInst)
      return nsnull;
    NS_ADDREF(sInst);
  }

  NS_ADDREF(sInst);
  return sInst;
}

bool nsPluginHost::IsRunningPlugin(nsPluginTag * plugin)
{
  if (!plugin || !plugin->mEntryPoint) {
    return false;
  }

  for (PRUint32 i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance *instance = mInstances[i].get();
    if (instance &&
        instance->GetPlugin() == plugin->mEntryPoint &&
        instance->IsRunning()) {
      return true;
    }
  }

  return false;
}

nsresult nsPluginHost::ReloadPlugins(bool reloadPages)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHost::ReloadPlugins Begin reloadPages=%d, active_instance_count=%d\n",
  reloadPages, mInstances.Length()));

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

  nsCOMPtr<nsISupportsArray> instsToReload;
  if (reloadPages) {
    NS_NewISupportsArray(getter_AddRefs(instsToReload));

    // Then stop any running plugin instances but hold on to the documents in the array
    // We are going to need to restart the instances in these documents later
    DestroyRunningInstances(instsToReload, nsnull);
  }

  // shutdown plugins and kill the list if there are no running plugins
  nsRefPtr<nsPluginTag> prev;
  nsRefPtr<nsPluginTag> next;

  for (nsRefPtr<nsPluginTag> p = mPlugins; p != nsnull;) {
    next = p->mNext;

    // only remove our plugin from the list if it's not running.
    if (!IsRunningPlugin(p)) {
      if (p == mPlugins)
        mPlugins = next;
      else
        prev->mNext = next;

      p->mNext = nsnull;

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

  // If we have shut down any plugin instances, we've now got to restart them.
  // Post an event to do the rest as we are going to be destroying the frame tree and we also want
  // any posted unload events to finish
  PRUint32 c;
  if (reloadPages &&
      instsToReload &&
      NS_SUCCEEDED(instsToReload->Count(&c)) &&
      c > 0) {
    nsCOMPtr<nsIRunnable> ev = new nsPluginDocReframeEvent(instsToReload);
    if (ev)
      NS_DispatchToCurrentThread(ev);
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHost::ReloadPlugins End active_instance_count=%d\n",
  mInstances.Length()));

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

  nsCAutoString uaString;
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
    *retstring = nsnull;
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
      wwatch->GetWindowByName(NS_LITERAL_STRING("_content").get(), nsnull, getter_AddRefs(domWindow));
    }
    rv = wwatch->GetNewPrompter(domWindow, getter_AddRefs(prompt));
  }

  NS_IF_ADDREF(*aPrompt = prompt);
  return rv;
}

nsresult nsPluginHost::GetURL(nsISupports* pluginInst,
                              const char* url,
                              const char* target,
                              nsIPluginStreamListener* streamListener,
                              const char* altHost,
                              const char* referrer,
                              bool forceJSEnabled)
{
  return GetURLWithHeaders(static_cast<nsNPAPIPluginInstance*>(pluginInst),
                           url, target, streamListener, altHost, referrer,
                           forceJSEnabled, nsnull, nsnull);
}

nsresult nsPluginHost::GetURLWithHeaders(nsNPAPIPluginInstance* pluginInst,
                                         const char* url,
                                         const char* target,
                                         nsIPluginStreamListener* streamListener,
                                         const char* altHost,
                                         const char* referrer,
                                         bool forceJSEnabled,
                                         PRUint32 getHeadersLength,
                                         const char* getHeaders)
{
  // we can only send a stream back to the plugin (as specified by a
  // null target) if we also have a nsIPluginStreamListener to talk to
  if (!target && !streamListener)
    return NS_ERROR_ILLEGAL_VALUE;

  nsresult rv = DoURLLoadSecurityCheck(pluginInst, url);
  if (NS_FAILED(rv))
    return rv;

  if (target) {
    nsCOMPtr<nsIPluginInstanceOwner> owner;
    rv = pluginInst->GetOwner(getter_AddRefs(owner));
    if (owner) {
      if ((0 == PL_strcmp(target, "newwindow")) ||
          (0 == PL_strcmp(target, "_new")))
        target = "_blank";
      else if (0 == PL_strcmp(target, "_current"))
        target = "_self";

      rv = owner->GetURL(url, target, nsnull, nsnull, 0);
    }
  }

  if (streamListener)
    rv = NewPluginURLStream(NS_ConvertUTF8toUTF16(url), pluginInst,
                            streamListener, nsnull,
                            getHeaders, getHeadersLength);

  return rv;
}

nsresult nsPluginHost::PostURL(nsISupports* pluginInst,
                                    const char* url,
                                    PRUint32 postDataLen,
                                    const char* postData,
                                    bool isFile,
                                    const char* target,
                                    nsIPluginStreamListener* streamListener,
                                    const char* altHost,
                                    const char* referrer,
                                    bool forceJSEnabled,
                                    PRUint32 postHeadersLength,
                                    const char* postHeaders)
{
  nsresult rv;

  // we can only send a stream back to the plugin (as specified
  // by a null target) if we also have a nsIPluginStreamListener
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
    PRUint32 newDataToPostLen;
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
    nsCOMPtr<nsIPluginInstanceOwner> owner;
    rv = instance->GetOwner(getter_AddRefs(owner));
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
  nsCOMPtr<nsIIOService> ioService;

  proxyService = do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &res);
  if (NS_FAILED(res) || !proxyService)
    return res;

  ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &res);
  if (NS_FAILED(res) || !ioService)
    return res;

  // make an nsURI from the argument url
  res = ioService->NewURI(nsDependentCString(url), nsnull, nsnull, getter_AddRefs(uriIn));
  if (NS_FAILED(res))
    return res;

  nsCOMPtr<nsIProxyInfo> pi;

  res = proxyService->Resolve(uriIn, 0, getter_AddRefs(pi));
  if (NS_FAILED(res))
    return res;

  nsCAutoString host, type;
  PRInt32 port = -1;

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

  if (nsnull == *result)
    res = NS_ERROR_OUT_OF_MEMORY;

  return res;
}

nsresult nsPluginHost::Init()
{
  return NS_OK;
}

nsresult nsPluginHost::Destroy()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHost::Destroy Called\n"));

  if (mIsDestroyed)
    return NS_OK;

  mIsDestroyed = true;

  // we should call nsIPluginInstance::Stop and nsIPluginInstance::SetWindow
  // for those plugins who want it
  DestroyRunningInstances(nsnull, nsnull);

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
    mPrivateDirServiceProvider = nsnull;
  }
#endif /* XP_WIN */

  mPrefService = nsnull; // release prefs service to avoid leaks!

  return NS_OK;
}

void nsPluginHost::OnPluginInstanceDestroyed(nsPluginTag* aPluginTag)
{
  bool hasInstance = false;
  for (PRUint32 i = 0; i < mInstances.Length(); i++) {
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

nsresult nsPluginHost::CreateListenerForChannel(nsIChannel* aChannel,
                                                nsObjectLoadingContent* aContent,
                                                nsIStreamListener** aListener)
{
  NS_PRECONDITION(aChannel && aContent,
                  "Invalid arguments to InstantiatePluginForChannel");
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return rv;

#ifdef PLUGIN_LOGGING
  if (PR_LOG_TEST(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL)) {
    nsCAutoString urlSpec;
    uri->GetAsciiSpec(urlSpec);

    PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
           ("nsPluginHost::InstantiatePluginForChannel Begin content=%p, url=%s\n",
           aContent, urlSpec.get()));

    PR_LogFlush();
  }
#endif

  // Note that we're not setting up a plugin instance here; the stream
  // listener's OnStartRequest will handle doing that.

  return NewEmbeddedPluginStreamListener(uri, aContent, nsnull, aListener);
}

nsresult
nsPluginHost::InstantiateEmbeddedPlugin(const char *aMimeType, nsIURI* aURL,
                                        nsObjectLoadingContent *aContent,
                                        nsPluginInstanceOwner** aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);

#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  if (aURL)
    aURL->GetAsciiSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHost::InstantiateEmbeddedPlugin Begin mime=%s, url=%s\n",
        aMimeType, urlSpec.get()));

  PR_LogFlush();
#endif

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

  // Security checks. Can't do security checks without a URI - hopefully the plugin
  // will take care of that.
  if (aURL) {
    nsCOMPtr<nsIScriptSecurityManager> secMan =
                    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return rv; // Better fail if we can't do security checks

    nsCOMPtr<nsIDocument> doc;
    instanceOwner->GetDocument(getter_AddRefs(doc));
    if (!doc)
      return NS_ERROR_NULL_POINTER;

    rv = secMan->CheckLoadURIWithPrincipal(doc->NodePrincipal(), aURL, 0);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDOMElement> elem;
    pti->GetDOMElement(getter_AddRefs(elem));

    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT; // default permit
    nsresult rv =
      NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OBJECT,
                                aURL,
                                doc->NodePrincipal(),
                                elem,
                                nsDependentCString(aMimeType ? aMimeType : ""),
                                nsnull, //extra
                                &shouldLoad);
    if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad))
      return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
  }

  bool isJava = false;
  nsPluginTag* pluginTag = FindPluginForType(aMimeType, true);
  if (pluginTag) {
    isJava = pluginTag->mIsJavaPlugin;
  }

  // Determine if the scheme of this URL is one we can handle internally because we should
  // only open the initial stream if it's one that we can handle internally. Otherwise
  // |NS_OpenURI| in |InstantiateEmbeddedPlugin| may open up a OS protocal registered helper app
  // Also set bCanHandleInternally to true if aAllowOpeningStreams is
  // false; we don't want to do any network traffic in that case.
  bool bCanHandleInternally = false;
  nsCAutoString scheme;
  if (aURL && NS_SUCCEEDED(aURL->GetScheme(scheme))) {
      nsCAutoString contractID(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX);
      contractID += scheme;
      ToLowerCase(contractID);
      nsCOMPtr<nsIProtocolHandler> handler = do_GetService(contractID.get());
      if (handler)
        bCanHandleInternally = true;
  }

  // if we don't have a MIME type at this point, we still have one more chance by
  // opening the stream and seeing if the server hands one back
  if (!aMimeType) {
    if (bCanHandleInternally && !aContent->SrcStreamLoading()) {
      NewEmbeddedPluginStream(aURL, aContent, nsnull);
    }
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

    // create an initial stream with data
    // don't make the stream if it's a java applet or we don't have SRC or DATA attribute
    // no need to check for "data" as it would have been converted to "src"
    const char *value;
    bool havedata = NS_SUCCEEDED(pti->GetAttribute("SRC", &value));
    if (havedata && !isJava && bCanHandleInternally && !aContent->SrcStreamLoading()) {
      NewEmbeddedPluginStream(aURL, aContent, instance.get());
    }
  }

  // At this point we consider instantiation to be successful. Do not return an error.
  instanceOwner.forget(aOwner);

#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec2;
  if (aURL != nsnull) aURL->GetAsciiSpec(urlSpec2);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHost::InstantiateEmbeddedPlugin Finished mime=%s, rv=%d, url=%s\n",
        aMimeType, rv, urlSpec2.get()));

  PR_LogFlush();
#endif

  return NS_OK;
}

nsresult nsPluginHost::InstantiateFullPagePlugin(const char *aMimeType,
                                                 nsIURI* aURI,
                                                 nsObjectLoadingContent *aContent,
                                                 nsPluginInstanceOwner **aOwner,
                                                 nsIStreamListener **aStreamListener)
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  aURI->GetSpec(urlSpec);
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHost::InstantiateFullPagePlugin Begin mime=%s, url=%s\n",
  aMimeType, urlSpec.get()));
#endif

  nsRefPtr<nsPluginInstanceOwner> instanceOwner = new nsPluginInstanceOwner();
  if (!instanceOwner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIContent> ourContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(aContent));
  nsresult rv = instanceOwner->Init(ourContent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = SetUpPluginInstance(aMimeType, aURI, instanceOwner);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsRefPtr<nsNPAPIPluginInstance> instance;
  instanceOwner->GetInstance(getter_AddRefs(instance));
  if (!instance) {
    return NS_ERROR_FAILURE;
  }

  NPWindow* win = nsnull;
  instanceOwner->GetWindow(win);
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  // Set up any widget that might be required.
  instanceOwner->CreateWidget();
  instanceOwner->CallSetWindow();

  rv = NewFullPagePluginStream(aURI, instance.get(), aStreamListener);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Call SetWindow again in case something changed.
  instanceOwner->CallSetWindow();

  instanceOwner.forget(aOwner);

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHost::InstantiateFullPagePlugin End mime=%s, rv=%d, url=%s\n",
  aMimeType, rv, urlSpec.get()));

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
  return nsnull;
}

nsPluginTag*
nsPluginHost::TagForPlugin(nsNPAPIPlugin* aPlugin)
{
  nsPluginTag* pluginTag;
  for (pluginTag = mPlugins; pluginTag; pluginTag = pluginTag->mNext) {
    if (pluginTag->mEntryPoint == aPlugin) {
      return pluginTag;
    }
  }
  // a plugin should never exist without a corresponding tag
  NS_ERROR("TagForPlugin has failed");
  return nsnull;
}

nsresult nsPluginHost::SetUpPluginInstance(const char *aMimeType,
                                           nsIURI *aURL,
                                           nsIPluginInstanceOwner *aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);

  nsresult rv = NS_OK;

  rv = TrySetUpPluginInstance(aMimeType, aURL, aOwner);

  // if we fail, refresh plugin list just in case the plugin has been
  // just added and try to instantiate plugin instance again, see bug 143178
  if (NS_FAILED(rv)) {
    // we should also make sure not to do this more than once per page
    // so if there are a few embed tags with unknown plugins,
    // we don't get unnecessary overhead
    // let's cache document to decide whether this is the same page or not
    nsCOMPtr<nsIDocument> document;
    aOwner->GetDocument(getter_AddRefs(document));

    nsCOMPtr<nsIDocument> currentdocument = do_QueryReferent(mCurrentDocument);
    if (document == currentdocument)
      return rv;

    mCurrentDocument = do_GetWeakReference(document);

    // ReloadPlugins will do the job smartly: nothing will be done
    // if no changes detected, in such a case just return
    if (NS_ERROR_PLUGINS_PLUGINSNOTCHANGED == ReloadPlugins(false))
      return rv;

    // other failure return codes may be not fatal, so we can still try
    aOwner->SetInstance(nsnull); // avoid assert about setting it twice
    rv = TrySetUpPluginInstance(aMimeType, aURL, aOwner);
  }

  return rv;
}

nsresult
nsPluginHost::TrySetUpPluginInstance(const char *aMimeType,
                                     nsIURI *aURL,
                                     nsIPluginInstanceOwner *aOwner)
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  if (aURL != nsnull) aURL->GetSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHost::TrySetupPluginInstance Begin mime=%s, owner=%p, url=%s\n",
        aMimeType, aOwner, urlSpec.get()));

  PR_LogFlush();
#endif
  
  const char* mimetype = nsnull;

  // if don't have a mimetype or no plugin can handle this mimetype
  // check by file extension
  nsPluginTag* pluginTag = FindPluginForType(aMimeType, true);
  if (!pluginTag) {
    nsCOMPtr<nsIURL> url = do_QueryInterface(aURL);
    if (!url) return NS_ERROR_FAILURE;

    nsCAutoString fileExtension;
    url->GetFileExtension(fileExtension);

    // if we don't have an extension or no plugin for this extension,
    // return failure as there is nothing more we can do
    if (fileExtension.IsEmpty() ||
        !(pluginTag = FindPluginEnabledForExtension(fileExtension.get(),
                                                    mimetype))) {
      return NS_ERROR_FAILURE;
    }
  }
  else {
    mimetype = aMimeType;
  }

  NS_ASSERTION(pluginTag, "Must have plugin tag here!");

  nsRefPtr<nsNPAPIPlugin> plugin;
  GetPlugin(mimetype, getter_AddRefs(plugin));

  nsRefPtr<nsNPAPIPluginInstance> instance;

  if (plugin) {
#if defined(XP_WIN)
    static BOOL firstJavaPlugin = FALSE;
    BOOL restoreOrigDir = FALSE;
    WCHAR origDir[_MAX_PATH];
    if (pluginTag->mIsJavaPlugin && !firstJavaPlugin) {
      DWORD dw = GetCurrentDirectoryW(_MAX_PATH, origDir);
      NS_ASSERTION(dw <= _MAX_PATH, "Failed to obtain the current directory, which may lead to incorrect class loading");
      nsCOMPtr<nsIFile> binDirectory;
      nsresult rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                           getter_AddRefs(binDirectory));

      if (NS_SUCCEEDED(rv)) {
        nsAutoString path;
        binDirectory->GetPath(path);
        restoreOrigDir = SetCurrentDirectoryW(path.get());
      }
    }
#endif

    instance = new nsNPAPIPluginInstance(plugin.get());

#if defined(XP_WIN)
    if (!firstJavaPlugin && restoreOrigDir) {
      BOOL bCheck = SetCurrentDirectoryW(origDir);
      NS_ASSERTION(bCheck, "Error restoring directory");
      firstJavaPlugin = TRUE;
    }
#endif
  }

  // it is adreffed here
  aOwner->SetInstance(instance.get());

  // this should not addref the instance or owner
  // except in some cases not Java, see bug 140931
  // our COM pointer will free the peer
  nsresult rv = instance->Initialize(aOwner, mimetype);
  if (NS_FAILED(rv)) {
    aOwner->SetInstance(nsnull);
    return rv;
  }

  // Cancel the plugin unload timer since we are creating
  // an instance for it.
  if (pluginTag->mUnloadTimer) {
    pluginTag->mUnloadTimer->Cancel();
  }

  mInstances.AppendElement(instance.get());

#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec2;
  if (aURL)
    aURL->GetSpec(urlSpec2);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_BASIC,
        ("nsPluginHost::TrySetupPluginInstance Finished mime=%s, rv=%d, owner=%p, url=%s\n",
        aMimeType, rv, aOwner, urlSpec2.get()));

  PR_LogFlush();
#endif

  return rv;
}

nsresult
nsPluginHost::IsPluginEnabledForType(const char* aMimeType)
{
  nsPluginTag *plugin = FindPluginForType(aMimeType, true);
  if (plugin)
    return NS_OK;

  // Pass false as the second arg so we can return NS_ERROR_PLUGIN_DISABLED
  // for disabled plug-ins.
  plugin = FindPluginForType(aMimeType, false);
  if (!plugin)
    return NS_ERROR_FAILURE;

  if (!plugin->IsEnabled()) {
    if (plugin->HasFlag(NS_PLUGIN_FLAG_BLOCKLISTED))
      return NS_ERROR_PLUGIN_BLOCKLISTED;
    else
      return NS_ERROR_PLUGIN_DISABLED;
  }

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

class DOMMimeTypeImpl : public nsIDOMMimeType {
public:
  NS_DECL_ISUPPORTS

  DOMMimeTypeImpl(nsPluginTag* aTag, PRUint32 aMimeTypeIndex)
  {
    if (!aTag)
      return;
    CopyUTF8toUTF16(aTag->mMimeDescriptions[aMimeTypeIndex], mDescription);
    CopyUTF8toUTF16(aTag->mExtensions[aMimeTypeIndex], mSuffixes);
    CopyUTF8toUTF16(aTag->mMimeTypes[aMimeTypeIndex], mType);
  }

  virtual ~DOMMimeTypeImpl() {
  }

  NS_METHOD GetDescription(nsAString& aDescription)
  {
    aDescription.Assign(mDescription);
    return NS_OK;
  }

  NS_METHOD GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin)
  {
    // this has to be implemented by the DOM version.
    *aEnabledPlugin = nsnull;
    return NS_OK;
  }

  NS_METHOD GetSuffixes(nsAString& aSuffixes)
  {
    aSuffixes.Assign(mSuffixes);
    return NS_OK;
  }

  NS_METHOD GetType(nsAString& aType)
  {
    aType.Assign(mType);
    return NS_OK;
  }

private:
  nsString mDescription;
  nsString mSuffixes;
  nsString mType;
};

NS_IMPL_ISUPPORTS1(DOMMimeTypeImpl, nsIDOMMimeType)

class DOMPluginImpl : public nsIDOMPlugin {
public:
  NS_DECL_ISUPPORTS

  DOMPluginImpl(nsPluginTag* aPluginTag) : mPluginTag(aPluginTag)
  {
  }

  virtual ~DOMPluginImpl() {
  }

  NS_METHOD GetDescription(nsAString& aDescription)
  {
    CopyUTF8toUTF16(mPluginTag.mDescription, aDescription);
    return NS_OK;
  }

  NS_METHOD GetFilename(nsAString& aFilename)
  {
    bool bShowPath;
    nsCOMPtr<nsIPrefBranch> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefService &&
        NS_SUCCEEDED(prefService->GetBoolPref("plugin.expose_full_path", &bShowPath)) &&
        bShowPath) {
      CopyUTF8toUTF16(mPluginTag.mFullPath, aFilename);
    } else {
      CopyUTF8toUTF16(mPluginTag.mFileName, aFilename);
    }

    return NS_OK;
  }

  NS_METHOD GetVersion(nsAString& aVersion)
  {
    CopyUTF8toUTF16(mPluginTag.mVersion, aVersion);
    return NS_OK;
  }

  NS_METHOD GetName(nsAString& aName)
  {
    CopyUTF8toUTF16(mPluginTag.mName, aName);
    return NS_OK;
  }

  NS_METHOD GetLength(PRUint32* aLength)
  {
    *aLength = mPluginTag.mMimeTypes.Length();
    return NS_OK;
  }

  NS_METHOD Item(PRUint32 aIndex, nsIDOMMimeType** aReturn)
  {
    nsIDOMMimeType* mimeType = new DOMMimeTypeImpl(&mPluginTag, aIndex);
    NS_IF_ADDREF(mimeType);
    *aReturn = mimeType;
    return NS_OK;
  }

  NS_METHOD NamedItem(const nsAString& aName, nsIDOMMimeType** aReturn)
  {
    for (int i = mPluginTag.mMimeTypes.Length() - 1; i >= 0; --i) {
      if (aName.Equals(NS_ConvertUTF8toUTF16(mPluginTag.mMimeTypes[i])))
        return Item(i, aReturn);
    }
    return NS_OK;
  }

private:
  nsPluginTag mPluginTag;
};

NS_IMPL_ISUPPORTS1(DOMPluginImpl, nsIDOMPlugin)

nsresult
nsPluginHost::GetPluginCount(PRUint32* aPluginCount)
{
  LoadPlugins();

  PRUint32 count = 0;

  nsPluginTag* plugin = mPlugins;
  while (plugin != nsnull) {
    if (plugin->IsEnabled()) {
      ++count;
    }
    plugin = plugin->mNext;
  }

  *aPluginCount = count;

  return NS_OK;
}

nsresult
nsPluginHost::GetPlugins(PRUint32 aPluginCount, nsIDOMPlugin** aPluginArray)
{
  LoadPlugins();

  nsPluginTag* plugin = mPlugins;
  for (PRUint32 i = 0; i < aPluginCount && plugin; plugin = plugin->mNext) {
    if (plugin->IsEnabled()) {
      nsIDOMPlugin* domPlugin = new DOMPluginImpl(plugin);
      NS_IF_ADDREF(domPlugin);
      aPluginArray[i++] = domPlugin;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPluginHost::GetPluginTags(PRUint32* aPluginCount, nsIPluginTag*** aResults)
{
  LoadPlugins();

  PRUint32 count = 0;
  nsRefPtr<nsPluginTag> plugin = mPlugins;
  while (plugin != nsnull) {
    count++;
    plugin = plugin->mNext;
  }

  *aResults = static_cast<nsIPluginTag**>
                         (nsMemory::Alloc(count * sizeof(**aResults)));
  if (!*aResults)
    return NS_ERROR_OUT_OF_MEMORY;

  *aPluginCount = count;

  plugin = mPlugins;
  for (PRUint32 i = 0; i < count; i++) {
    (*aResults)[i] = plugin;
    NS_ADDREF((*aResults)[i]);
    plugin = plugin->mNext;
  }

  return NS_OK;
}

nsPluginTag*
nsPluginHost::FindPluginForType(const char* aMimeType,
                                bool aCheckEnabled)
{
  if (!aMimeType) {
    return nsnull;
  }

  LoadPlugins();

  nsPluginTag *plugin = mPlugins;
  while (plugin) {
    if (!aCheckEnabled || plugin->IsEnabled()) {
      PRInt32 mimeCount = plugin->mMimeTypes.Length();
      for (PRInt32 i = 0; i < mimeCount; i++) {
        if (0 == PL_strcasecmp(plugin->mMimeTypes[i].get(), aMimeType)) {
          return plugin;
        }
      }
    }
    plugin = plugin->mNext;
  }

  return nsnull;
}

nsPluginTag*
nsPluginHost::FindPluginEnabledForExtension(const char* aExtension,
                                            const char*& aMimeType)
{
  if (!aExtension) {
    return nsnull;
  }

  LoadPlugins();

  nsPluginTag *plugin = mPlugins;
  while (plugin) {
    if (plugin->IsEnabled()) {
      PRInt32 variants = plugin->mExtensions.Length();
      for (PRInt32 i = 0; i < variants; i++) {
        // mExtensionsArray[cnt] is a list of extensions separated by commas
        if (0 == CompareExtensions(plugin->mExtensions[i].get(), aExtension)) {
          aMimeType = plugin->mMimeTypes[i].get();
          return plugin;
        }
      }
    }
    plugin = plugin->mNext;
  }

  return nsnull;
}

static nsresult CreateNPAPIPlugin(nsPluginTag *aPluginTag,
                                  nsNPAPIPlugin **aOutNPAPIPlugin)
{
  // If this is an in-process plugin we'll need to load it here if we haven't already.
  if (!nsNPAPIPlugin::RunPluginOOP(aPluginTag)) {
    if (aPluginTag->mFullPath.IsEmpty())
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1");
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

nsresult nsPluginHost::EnsurePluginLoaded(nsPluginTag* plugin)
{
  nsRefPtr<nsNPAPIPlugin> entrypoint = plugin->mEntryPoint;
  if (!entrypoint) {
    nsresult rv = CreateNPAPIPlugin(plugin, getter_AddRefs(entrypoint));
    if (NS_FAILED(rv)) {
      return rv;
    }
    plugin->mEntryPoint = entrypoint;
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

#ifdef NS_DEBUG
    if (aMimeType && !pluginTag->mFileName.IsEmpty())
      printf("For %s found plugin %s\n", aMimeType, pluginTag->mFileName.get());
#endif

    rv = EnsurePluginLoaded(pluginTag);
    if (NS_FAILED(rv)) {
      return rv;
    }

    NS_ADDREF(*aPlugin = pluginTag->mEntryPoint);
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
                                const nsTArray<nsCString>& sites,
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
  for (PRUint32 i = 0; i < sites.Length(); ++i) {
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
nsPluginHost::ClearSiteData(nsIPluginTag* plugin, const nsACString& domain,
                            PRUint64 flags, PRInt64 maxAge)
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
  if (!tag->mIsFlashPlugin && !tag->mEntryPoint) {
    return NS_ERROR_FAILURE;
  }

  // Make sure the plugin is loaded.
  nsresult rv = EnsurePluginLoaded(tag);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PluginLibrary* library = tag->mEntryPoint->GetLibrary();

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
  for (PRUint32 i = 0; i < matches.Length(); ++i) {
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
  if (!tag->mIsFlashPlugin && !tag->mEntryPoint) {
    return NS_ERROR_FAILURE;
  }

  // Make sure the plugin is loaded.
  nsresult rv = EnsurePluginLoaded(tag);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PluginLibrary* library = tag->mEntryPoint->GetLibrary();

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

nsPluginTag * nsPluginHost::HaveSamePlugin(nsPluginTag * aPluginTag)
{
  for (nsPluginTag* tag = mPlugins; tag; tag = tag->mNext) {
    if (tag->Equals(aPluginTag))
      return tag;
  }
  return nsnull;
}

bool nsPluginHost::IsDuplicatePlugin(nsPluginTag * aPluginTag)
{
  nsPluginTag * tag = HaveSamePlugin(aPluginTag);
  if (tag) {
    // if we got the same plugin, check the full path to see if this is a dup;

    // mFileName contains full path on Windows and Unix and leaf name on Mac
    // if those are not equal, we have the same plugin with  different path,
    // i.e. duplicate, return true
    if (!tag->mFileName.Equals(aPluginTag->mFileName))
      return true;

    // if they are equal, compare mFullPath fields just in case
    // mFileName contained leaf name only, and if not equal, return true
    if (!tag->mFullPath.Equals(aPluginTag->mFullPath))
      return true;
  }

  // we do not have it at all, return false
  return false;
}

typedef NS_NPAPIPLUGIN_CALLBACK(char *, NP_GETMIMEDESCRIPTION)(void);

nsresult nsPluginHost::ScanPluginsDirectory(nsIFile *pluginsDir,
                                            bool aCreatePluginList,
                                            bool *aPluginsChanged)
{
  NS_ENSURE_ARG_POINTER(aPluginsChanged);
  nsresult rv;

  *aPluginsChanged = false;

#ifdef PLUGIN_LOGGING
  nsCAutoString dirPath;
  pluginsDir->GetNativePath(dirPath);
  PLUGIN_LOG(PLUGIN_LOG_BASIC,
  ("nsPluginHost::ScanPluginsDirectory dir=%s\n", dirPath.get()));
#endif

  nsCOMPtr<nsISimpleEnumerator> iter;
  rv = pluginsDir->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv))
    return rv;

  nsAutoTArray<nsCOMPtr<nsILocalFile>, 6> pluginFiles;

  bool hasMore;
  while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = iter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv))
      continue;
    nsCOMPtr<nsILocalFile> dirEntry(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv))
      continue;

    // Sun's JRE 1.3.1 plugin must have symbolic links resolved or else it'll crash.
    // See bug 197855.
    dirEntry->Normalize();

    if (nsPluginsDir::IsPluginFile(dirEntry)) {
      pluginFiles.AppendElement(dirEntry);
    }
  }

  bool warnOutdated = false;

  for (PRUint32 i = 0; i < pluginFiles.Length(); i++) {
    nsCOMPtr<nsILocalFile>& localfile = pluginFiles[i];

    nsString utf16FilePath;
    rv = localfile->GetPath(utf16FilePath);
    if (NS_FAILED(rv))
      continue;

    PRInt64 fileModTime = LL_ZERO;
    localfile->GetLastModifiedTime(&fileModTime);

    // Look for it in our cache
    NS_ConvertUTF16toUTF8 filePath(utf16FilePath);
    nsRefPtr<nsPluginTag> pluginTag;
    RemoveCachedPluginsInfo(filePath.get(),
                            getter_AddRefs(pluginTag));

    bool enabled = true;
    bool seenBefore = false;
    if (pluginTag) {
      seenBefore = true;
      // If plugin changed, delete cachedPluginTag and don't use cache
      if (LL_NE(fileModTime, pluginTag->mLastModifiedTime)) {
        // Plugins has changed. Don't use cached plugin info.
        enabled = (pluginTag->Flags() & NS_PLUGIN_FLAG_ENABLED) != 0;
        pluginTag = nsnull;

        // plugin file changed, flag this fact
        *aPluginsChanged = true;
      }
      else {
        // if it is unwanted plugin we are checking for, get it back to the cache info list
        // if this is a duplicate plugin, too place it back in the cache info list marking unwantedness
        if (IsDuplicatePlugin(pluginTag)) {
          if (!pluginTag->HasFlag(NS_PLUGIN_FLAG_UNWANTED)) {
            // Plugin switched from wanted to unwanted
            *aPluginsChanged = true;
          }
          pluginTag->Mark(NS_PLUGIN_FLAG_UNWANTED);
          pluginTag->mNext = mCachedPlugins;
          mCachedPlugins = pluginTag;
        } else if (pluginTag->HasFlag(NS_PLUGIN_FLAG_UNWANTED)) {
          pluginTag->UnMark(NS_PLUGIN_FLAG_UNWANTED);
          // Plugin switched from unwanted to wanted
          *aPluginsChanged = true;
        }
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
      PRLibrary *library = nsnull;
      nsPluginInfo info;
      memset(&info, 0, sizeof(info));
      nsresult res = pluginFile.GetPluginInfo(info, &library);
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

      nsCOMPtr<nsIBlocklistService> blocklist = do_GetService("@mozilla.org/extensions/blocklist;1");
      if (blocklist) {
        PRUint32 state;
        rv = blocklist->GetPluginBlocklistState(pluginTag, EmptyString(),
                                                EmptyString(), &state);

        if (NS_SUCCEEDED(rv)) {
          // If the blocklist says so then block the plugin. If the blocklist says
          // it is risky and we have never seen this plugin before then disable it
          if (state == nsIBlocklistService::STATE_BLOCKED)
            pluginTag->Mark(NS_PLUGIN_FLAG_BLOCKLISTED);
          else if (state == nsIBlocklistService::STATE_SOFTBLOCKED && !seenBefore)
            enabled = false;
          else if (state == nsIBlocklistService::STATE_OUTDATED && !seenBefore)
            warnOutdated = true;
        }
      }

      if (!enabled)
        pluginTag->UnMark(NS_PLUGIN_FLAG_ENABLED);

      // if this is unwanted plugin we are checkin for, or this is a duplicate plugin,
      // add it to our cache info list so we can cache the unwantedness of this plugin
      // when we sync cached plugins to registry
      NS_ASSERTION(!pluginTag->HasFlag(NS_PLUGIN_FLAG_UNWANTED),
                   "Brand-new tags should not be unwanted");
      if (IsDuplicatePlugin(pluginTag)) {
        pluginTag->Mark(NS_PLUGIN_FLAG_UNWANTED);
        pluginTag->mNext = mCachedPlugins;
        mCachedPlugins = pluginTag;
      }

      // Plugin unloading is tag-based. If we created a new tag and loaded
      // the library in the process then we want to attempt to unload it here.
      // Only do this if the pref is set for aggressive unloading.
      if (UnloadPluginsASAP()) {
        pluginTag->TryUnloadPlugin(false);
      }
    }

    // set the flag that we want to add this plugin to the list for now
    // and see if it remains after we check several reasons not to do so
    bool bAddIt = true;
    
    if (HaveSamePlugin(pluginTag)) {
      // we cannot get here if the plugin has just been added
      // and thus |pluginTag| is not from cache, because otherwise
      // it would not be present in the list;
      bAddIt = false;
    }

    // do it if we still want it
    if (bAddIt) {
      if (!seenBefore) {
        // We have a valid new plugin so report that plugins have changed.
        *aPluginsChanged = true;
      }

      // If we're not creating a plugin list, simply looking for changes,
      // then we're done.
      if (!aCreatePluginList) {
        return NS_OK;
      }

      pluginTag->SetHost(this);

      // Add plugin tags such that the list is ordered by modification date,
      // newest to oldest. This is ugly, it'd be easier with just about anything
      // other than a single-directional linked list.
      if (mPlugins) {
        nsPluginTag *prev = nsnull;
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

      if (pluginTag->IsEnabled()) {
        pluginTag->RegisterWithCategoryManager(mOverrideInternalTypes);
      }
    }
  }

  if (warnOutdated) {
    mPrefService->SetBoolPref("plugins.update.notifyUser", true);
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
      obsService->NotifyObservers(nsnull, "plugins-list-updated", nsnull);
  }

  return NS_OK;
}

// if aCreatePluginList is false we will just scan for plugins
// and see if any changes have been made to the plugins.
// This is needed in ReloadPlugins to prevent possible recursive reloads
nsresult nsPluginHost::FindPlugins(bool aCreatePluginList, bool * aPluginsChanged)
{
  Telemetry::AutoTimer<Telemetry::FIND_PLUGINS> telemetry;

#ifdef CALL_SAFETY_ON
  // check preferences on whether or not we want to try safe calls to plugins
  NS_INIT_PLUGIN_SAFE_CALLS;
#endif

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
  bool bScanPLIDs = false;

  if (mPrefService)
    mPrefService->GetBoolPref("plugin.scan.plid.all", &bScanPLIDs);

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
  const char* const prefs[] = {NS_WIN_JRE_SCAN_KEY,
                               NS_WIN_ACROBAT_SCAN_KEY,
                               NS_WIN_QUICKTIME_SCAN_KEY,
                               NS_WIN_WMP_SCAN_KEY};

  PRUint32 size = sizeof(prefs) / sizeof(prefs[0]);

  for (PRUint32 i = 0; i < size; i+=1) {
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

  // if get to this point and did not detect changes in plugins
  // that means no plugins got updated or added
  // let's see if plugins have been removed
  if (!*aPluginsChanged) {
    // count plugins remained in cache, if there are some, that means some plugins were removed;
    // while counting, we should ignore unwanted plugins which are also present in cache
    PRUint32 cachecount = 0;
    for (nsPluginTag * cachetag = mCachedPlugins; cachetag; cachetag = cachetag->mNext) {
      if (!cachetag->HasFlag(NS_PLUGIN_FLAG_UNWANTED))
        cachecount++;
    }
    // if there is something left in cache, some plugins got removed from the directory
    // and therefor their info did not get removed from the cache info list during directory scan;
    // flag this fact
    if (cachecount > 0)
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

  if (!aPluginTag || aPluginTag->IsEnabled())
    return NS_OK;

  nsCOMPtr<nsISupportsArray> instsToReload;
  NS_NewISupportsArray(getter_AddRefs(instsToReload));
  DestroyRunningInstances(instsToReload, aPluginTag);
  
  PRUint32 c;
  if (instsToReload && NS_SUCCEEDED(instsToReload->Count(&c)) && c > 0) {
    nsCOMPtr<nsIRunnable> ev = new nsPluginDocReframeEvent(instsToReload);
    if (ev)
      NS_DispatchToCurrentThread(ev);
  }

  return NS_OK;
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

  PRFileDesc* fd = nsnull;

  nsCOMPtr<nsIFile> pluginReg;

  rv = mPluginRegFile->Clone(getter_AddRefs(pluginReg));
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString filename(kPluginRegistryFilename);
  filename.Append(".tmp");
  rv = pluginReg->AppendNative(filename);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(pluginReg, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = localFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (!runtime) {
    return NS_ERROR_FAILURE;
  }
    
  nsCAutoString arch;
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

  nsPluginTag *taglist[] = {mPlugins, mCachedPlugins};
  for (int i=0; i<(int)(sizeof(taglist)/sizeof(nsPluginTag *)); i++) {
    for (nsPluginTag *tag = taglist[i]; tag; tag=tag->mNext) {
      // from mCachedPlugins list write down only unwanted plugins
      if ((taglist[i] == mCachedPlugins) && !tag->HasFlag(NS_PLUGIN_FLAG_UNWANTED))
        continue;
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
        tag->Flags(),
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
        tag->mMimeTypes.Length() + (tag->mIsNPRuntimeEnabledJavaPlugin ? 1 : 0));

      // Add in each mimetype this plugin supports
      for (PRUint32 i = 0; i < tag->mMimeTypes.Length(); i++) {
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

      if (tag->mIsNPRuntimeEnabledJavaPlugin) {
        PR_fprintf(fd, "%d%c%s%c%s%c%s%c%c\n",
          tag->mMimeTypes.Length(), PLUGIN_REGISTRY_FIELD_DELIMITER,
          "application/x-java-vm-npruntime",
          PLUGIN_REGISTRY_FIELD_DELIMITER,
          "",
          PLUGIN_REGISTRY_FIELD_DELIMITER,
          "",
          PLUGIN_REGISTRY_FIELD_DELIMITER,
          PLUGIN_REGISTRY_END_OF_LINE_MARKER);
      }
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
  rv = localFile->GetParent(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = localFile->MoveToNative(parent, kPluginRegistryFilename);
  return rv;
}

#define PLUGIN_REG_MIMETYPES_ARRAY_SIZE 12
nsresult
nsPluginHost::ReadPluginInfo()
{
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

  PRFileDesc* fd = nsnull;

  nsCOMPtr<nsIFile> pluginReg;

  rv = mPluginRegFile->Clone(getter_AddRefs(pluginReg));
  if (NS_FAILED(rv))
    return rv;

  rv = pluginReg->AppendNative(kPluginRegistryFilename);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(pluginReg, &rv);
  if (NS_FAILED(rv))
    return rv;

  PRInt64 fileSize;
  rv = localFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 flen = PRInt64(fileSize);
  if (flen == 0) {
    NS_WARNING("Plugins Registry Empty!");
    return NS_OK; // ERROR CONDITION
  }

  nsPluginManifestLineReader reader;
  char* registry = reader.Init(flen);
  if (!registry)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = localFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd);
  if (NS_FAILED(rv))
    return rv;

  // set rv to return an error on goto out
  rv = NS_ERROR_FAILURE;

  PRInt32 bread = PR_Read(fd, registry, flen);
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
  PRInt32 vdiff = NS_CompareVersions(values[1], kPluginRegistryVersion);
  // If this is a registry from some future version then don't attempt to read it
  if (vdiff > 0)
    return rv;
  // If this is a registry from before the minimum then don't attempt to read it
  if (NS_CompareVersions(values[1], kMinimumRegistryVersion) < 0)
    return rv;

  // Registry v0.10 and upwards includes the plugin version field
  bool regHasVersion = NS_CompareVersions(values[1], "0.10") >= 0;

  // Registry v0.13 and upwards includes the architecture
  if (NS_CompareVersions(values[1], "0.13") >= 0) {
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
      
    nsCAutoString arch;
    if (NS_FAILED(runtime->GetXPCOMABI(arch))) {
      return rv;
    }
      
    // If this is a registry from a different architecture then don't attempt to read it
    if (PL_strcmp(archValues[1], arch.get())) {
      return rv;
    }
  }
  
  // Registry v0.13 and upwards includes the list of invalid plugins
  bool hasInvalidPlugins = (NS_CompareVersions(values[1], "0.13") >= 0);

  if (!ReadSectionHeader(reader, "PLUGINS"))
    return rv;

#if defined(XP_MACOSX)
  bool hasFullPathInFileNameField = false;
#else
  bool hasFullPathInFileNameField = (NS_CompareVersions(values[1], "0.11") < 0);
#endif

  while (reader.NextLine()) {
    const char *filename;
    const char *fullpath;
    nsCAutoString derivedFileName;
    
    if (hasInvalidPlugins && *reader.LinePtr() == '[') {
      break;
    }
    
    if (hasFullPathInFileNameField) {
      fullpath = reader.LinePtr();
      if (!reader.NextLine())
        return rv;
      // try to derive a file name from the full path
      if (fullpath) {
        nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1");
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
    PRInt64 lastmod = (vdiff == 0) ? nsCRT::atoll(values[0]) : -1;
    PRUint32 tagflag = atoi(values[2]);
    if (!reader.NextLine())
      return rv;

    const char *description = reader.LinePtr();
    if (!reader.NextLine())
      return rv;

    const char *name = reader.LinePtr();
    if (!reader.NextLine())
      return rv;

    int mimetypecount = atoi(reader.LinePtr());

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

    if (!tag)
      continue;

    // Mark plugin as loaded from cache
    tag->Mark(tagflag | NS_PLUGIN_FLAG_FROMCACHE);
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
      PRInt64 lastmod = (vdiff == 0) ? nsCRT::atoll(lastModifiedTimeStamp) : -1;
      
      nsRefPtr<nsInvalidPluginTag> invalidTag = new nsInvalidPluginTag(fullpath, lastmod);
      
      invalidTag->mNext = mInvalidPlugins;
      if (mInvalidPlugins) {
        mInvalidPlugins->mPrev = invalidTag;
      }
      mInvalidPlugins = invalidTag;
    }
  }
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
      tag->mNext = nsnull;
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
                                          nsIPluginStreamListener* aListener,
                                          nsIInputStream *aPostStream,
                                          const char *aHeadersData,
                                          PRUint32 aHeadersDataLen)
{
  nsCOMPtr<nsIURI> url;
  nsAutoString absUrl;
  nsresult rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  // get the full URL of the document that the plugin is embedded
  //   in to create an absolute url in case aURL is relative
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  aInstance->GetOwner(getter_AddRefs(owner));
  if (owner) {
    rv = owner->GetDocument(getter_AddRefs(doc));
    if (NS_SUCCEEDED(rv) && doc) {
      // Create an absolute URL
      rv = NS_MakeAbsoluteURI(absUrl, aURL, doc->GetDocBaseURI());
    }
  }

  if (absUrl.IsEmpty())
    absUrl.Assign(aURL);

  rv = NS_NewURI(getter_AddRefs(url), absUrl);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIPluginTagInfo> pti = do_QueryInterface(owner);
  nsCOMPtr<nsIDOMElement> element;
  if (pti)
    pti->GetDOMElement(getter_AddRefs(element));

  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OBJECT_SUBREQUEST,
                                 url,
                                 (doc ? doc->NodePrincipal() : nsnull),
                                 element,
                                 EmptyCString(), //mime guess
                                 nsnull,         //extra
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
  rv = NS_NewChannel(getter_AddRefs(channel), url, nsnull,
    nsnull, /* do not add this internal plugin's channel
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


      if (!referer)
        referer = doc->GetDocumentURI();

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
  rv = channel->AsyncOpen(listenerPeer, nsnull);
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

  // get the URL of the document that loaded the plugin
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  aInstance->GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return NS_ERROR_FAILURE;

  // Create an absolute URL for the target in case the target is relative
  nsCOMPtr<nsIURI> targetURL;
  NS_NewURI(getter_AddRefs(targetURL), aURL, doc->GetDocBaseURI());
  if (!targetURL)
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
                                  PRUint32 aHeadersDataLen,
                                  nsIChannel *aGenericChannel)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIHttpChannel> aChannel = do_QueryInterface(aGenericChannel);
  if (!aChannel) {
    return NS_ERROR_NULL_POINTER;
  }

  // used during the manipulation of the String from the aHeadersData
  nsCAutoString headersString;
  nsCAutoString oneHeader;
  nsCAutoString headerName;
  nsCAutoString headerValue;
  PRInt32 crlf = 0;
  PRInt32 colon = 0;

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
    PRUint32 cachedInstanceLimit;
    nsresult rv = NS_ERROR_FAILURE;
    if (mPrefService)
      rv = mPrefService->GetIntPref(NS_PREF_MAX_NUM_CACHED_INSTANCES, (int*)&cachedInstanceLimit);
    if (NS_FAILED(rv))
      cachedInstanceLimit = DEFAULT_NUMBER_OF_STOPPED_INSTANCES;
    
    if (StoppedInstanceCount() >= cachedInstanceLimit) {
      nsNPAPIPluginInstance *oldestInstance = FindOldestStoppedInstance();
      if (oldestInstance) {
        nsPluginTag* pluginTag = TagForPlugin(oldestInstance->GetPlugin());
        oldestInstance->Destroy();
        mInstances.RemoveElement(oldestInstance);
        OnPluginInstanceDestroyed(pluginTag);
      }
    }
  } else {
    nsPluginTag* pluginTag = TagForPlugin(aInstance->GetPlugin());
    aInstance->Destroy();
    mInstances.RemoveElement(aInstance);
    OnPluginInstanceDestroyed(pluginTag);
  }

  return NS_OK;
}

nsresult nsPluginHost::NewEmbeddedPluginStreamListener(nsIURI* aURL,
                                                       nsObjectLoadingContent *aContent,
                                                       nsNPAPIPluginInstance* aInstance,
                                                       nsIStreamListener** aListener)
{
  if (!aURL)
    return NS_OK;

  nsRefPtr<nsPluginStreamListenerPeer> listener = new nsPluginStreamListenerPeer();
  if (!listener)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;

  // if we have an instance, everything has been set up
  // if we only have an owner, then we need to pass it in
  // so the listener can set up the instance later after
  // we've determined the mimetype of the stream
  if (aInstance)
    rv = listener->InitializeEmbedded(aURL, aInstance, nsnull);
  else if (aContent)
    rv = listener->InitializeEmbedded(aURL, nsnull, aContent);
  else
    rv = NS_ERROR_ILLEGAL_VALUE;

  if (NS_SUCCEEDED(rv))
    NS_ADDREF(*aListener = listener);

  return rv;
}

// Called by InstantiateEmbeddedPlugin()
nsresult nsPluginHost::NewEmbeddedPluginStream(nsIURI* aURL,
                                               nsObjectLoadingContent *aContent,
                                               nsNPAPIPluginInstance* aInstance)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = NewEmbeddedPluginStreamListener(aURL, aContent, aInstance,
                                                getter_AddRefs(listener));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsILoadGroup> loadGroup;
    if (aContent) {
      nsCOMPtr<nsIContent> aIContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(aContent));
      doc = aIContent->GetDocument();
      if (doc) {
        loadGroup = doc->GetDocumentLoadGroup();
      }
    }
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), aURL, nsnull, loadGroup, nsnull);
    if (NS_SUCCEEDED(rv)) {
      // if this is http channel, set referrer, some servers are configured
      // to reject requests without referrer set, see bug 157796
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
      if (httpChannel && doc)
        httpChannel->SetReferrer(doc->GetDocumentURI());

      rv = channel->AsyncOpen(listener, nsnull);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }

  return rv;
}

// Called by InstantiateFullPagePlugin()
nsresult nsPluginHost::NewFullPagePluginStream(nsIURI* aURI,
                                               nsNPAPIPluginInstance *aInstance,
                                               nsIStreamListener **aStreamListener)
{
  NS_ASSERTION(aStreamListener, "Stream listener out param cannot be null");

  nsRefPtr<nsPluginStreamListenerPeer> listener = new nsPluginStreamListenerPeer();
  if (!listener)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = listener->InitializeFullPage(aURI, aInstance);
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
  if (!nsCRT::strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic)) {
    OnShutdown();
    Destroy();
    sInst->Release();
  }
  if (!nsCRT::strcmp(NS_PRIVATE_BROWSING_SWITCH_TOPIC, aTopic)) {
    // inform all active plugins of changed private mode state
    for (PRUint32 i = 0; i < mInstances.Length(); i++) {
      mInstances[i]->PrivateModeStateChanged();
    }
  }
#ifdef MOZ_WIDGET_ANDROID
  if (!nsCRT::strcmp("application-background", aTopic)) {
    for(PRUint32 i = 0; i < mInstances.Length(); i++) {
      mInstances[i]->NotifyForeground(false);
    }
  }
  if (!nsCRT::strcmp("application-foreground", aTopic)) {
    for(PRUint32 i = 0; i < mInstances.Length(); i++) {
      if (mInstances[i]->IsOnScreen())
        mInstances[i]->NotifyForeground(true);
    }
  }
  if (!nsCRT::strcmp("memory-pressure", aTopic)) {
    for(PRUint32 i = 0; i < mInstances.Length(); i++) {
      mInstances[i]->MemoryPressure();
    }
  }
#endif
  return NS_OK;
}

nsresult
nsPluginHost::HandleBadPlugin(PRLibrary* aLibrary, nsNPAPIPluginInstance *aInstance)
{
  // the |aLibrary| parameter is not needed anymore, after we added |aInstance| which
  // can also be used to look up the plugin name, but we cannot get rid of it because
  // the |nsIPluginHost| interface is deprecated which in fact means 'frozen'

  NS_ERROR("Plugin performed illegal operation");
  NS_ENSURE_ARG_POINTER(aInstance);

  if (mDontShowBadPluginMessage)
    return NS_OK;

  nsCOMPtr<nsIPluginInstanceOwner> owner;
  aInstance->GetOwner(getter_AddRefs(owner));

  nsCOMPtr<nsIPrompt> prompt;
  GetPrompt(owner, getter_AddRefs(prompt));
  if (!prompt)
    return NS_OK;

  nsCOMPtr<nsIStringBundleService> strings =
    mozilla::services::GetStringBundleService();
  if (!strings)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = strings->CreateBundle(BRAND_PROPERTIES_URL, getter_AddRefs(bundle));
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLString brandName;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(brandName));
  if (NS_FAILED(rv))
    return rv;

  rv = strings->CreateBundle(PLUGIN_PROPERTIES_URL, getter_AddRefs(bundle));
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLString title, message, checkboxMessage;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("BadPluginTitle").get(),
                                 getter_Copies(title));
  if (NS_FAILED(rv))
    return rv;

  const PRUnichar *formatStrings[] = { brandName.get() };
  if (NS_FAILED(rv = bundle->FormatStringFromName(NS_LITERAL_STRING("BadPluginMessage").get(),
                               formatStrings, 1, getter_Copies(message))))
    return rv;

  rv = bundle->GetStringFromName(NS_LITERAL_STRING("BadPluginCheckboxMessage").get(),
                                 getter_Copies(checkboxMessage));
  if (NS_FAILED(rv))
    return rv;

  nsNPAPIPlugin *plugin = aInstance->GetPlugin();
  if (!plugin)
    return NS_ERROR_FAILURE;

  nsPluginTag *pluginTag = TagForPlugin(plugin);

  // add plugin name to the message
  nsCString pluginname;
  if (pluginTag) {
    if (!pluginTag->mName.IsEmpty()) {
      pluginname = pluginTag->mName;
    } else {
      pluginname = pluginTag->mFileName;
    }
  } else {
    pluginname.AppendLiteral("???");
  }

  NS_ConvertUTF8toUTF16 msg(pluginname);
  msg.AppendLiteral("\n\n");
  msg.Append(message);

  PRInt32 buttonPressed;
  bool checkboxState = false;
  rv = prompt->ConfirmEx(title, msg.get(),
                       nsIPrompt::BUTTON_TITLE_OK * nsIPrompt::BUTTON_POS_0,
                       nsnull, nsnull, nsnull,
                       checkboxMessage, &checkboxState, &buttonPressed);


  if (NS_SUCCEEDED(rv) && checkboxState)
    mDontShowBadPluginMessage = true;

  return rv;
}

nsresult
nsPluginHost::ParsePostBufferToFixHeaders(const char *inPostData, PRUint32 inPostDataLen,
                                          char **outPostData, PRUint32 *outPostDataLen)
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

  PRUint32 newBufferLen = 0;
  PRUint32 dataLen = pEod - pSod;
  PRUint32 headersLen = pEoh ? pSod - inPostData : 0;

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
    PRUint32 l = sizeof(ContentLenHeader) + sizeof(CRLFCRLF) + 32;
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
  PRInt64 fileSize;
  nsCAutoString filename;

  // stat file == get size & convert file:///c:/ to c: if needed
  nsCOMPtr<nsIFile> inFile;
  rv = NS_GetFileFromURLSpec(nsDependentCString(aPostDataURL),
                             getter_AddRefs(inFile));
  if (NS_FAILED(rv)) {
    nsCOMPtr<nsILocalFile> localFile;
    rv = NS_NewNativeLocalFile(nsDependentCString(aPostDataURL), false,
                               getter_AddRefs(localFile));
    if (NS_FAILED(rv)) return rv;
    inFile = localFile;
  }
  rv = inFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) return rv;
  rv = inFile->GetNativePath(filename);
  if (NS_FAILED(rv)) return rv;

  if (!LL_IS_ZERO(fileSize)) {
    nsCOMPtr<nsIInputStream> inStream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inStream), inFile);
    if (NS_FAILED(rv)) return rv;

    // Create a temporary file to write the http Content-length:
    // %ld\r\n\" header and "\r\n" == end of headers for post data to

    nsCOMPtr<nsIFile> tempFile;
    rv = GetPluginTempDir(getter_AddRefs(tempFile));
    if (NS_FAILED(rv))
      return rv;

    nsCAutoString inFileName;
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
    PRUint32 br, bw;
    bool firstRead = true;
    while (1) {
      // Read() mallocs if buffer is null
      rv = inStream->Read(buf, 1024, &br);
      if (NS_FAILED(rv) || (PRInt32)br <= 0)
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
nsPluginHost::InstantiateDummyJavaPlugin(nsIPluginInstanceOwner *aOwner)
{
  // Pass false as the second arg, we want the answer to be the
  // same here whether the Java plugin is enabled or not.
  nsPluginTag *plugin = FindPluginForType("application/x-java-vm", false);

  if (!plugin || !plugin->mIsNPRuntimeEnabledJavaPlugin) {
    // No NPRuntime enabled Java plugin found, no point in
    // instantiating a dummy plugin then.

    return NS_OK;
  }

  nsresult rv = SetUpPluginInstance("application/x-java-vm", nsnull, aOwner);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsNPAPIPluginInstance> instance;
  aOwner->GetInstance(getter_AddRefs(instance));
  if (!instance)
    return NS_OK;

  instance->DefineJavaProperties();

  return NS_OK;
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

#ifdef MAC_CARBON_PLUGINS
// Flash requires a minimum of 8 events per second to avoid audio skipping.
// Since WebKit uses a hidden plugin event rate of 4 events per second Flash
// uses a Carbon timer for WebKit which fires at 8 events per second.
#define HIDDEN_PLUGIN_DELAY 125
#define VISIBLE_PLUGIN_DELAY 20
#endif

void nsPluginHost::AddIdleTimeTarget(nsIPluginInstanceOwner* objectFrame, bool isVisible)
{
#ifdef MAC_CARBON_PLUGINS
  nsTObserverArray<nsIPluginInstanceOwner*> *targetArray;
  if (isVisible) {
    targetArray = &mVisibleTimerTargets;
  } else {
    targetArray = &mHiddenTimerTargets;
  }

  if (targetArray->Contains(objectFrame)) {
    return;
  }

  targetArray->AppendElement(objectFrame);
  if (targetArray->Length() == 1) {
    if (isVisible) {
      mVisiblePluginTimer->InitWithCallback(this, VISIBLE_PLUGIN_DELAY, nsITimer::TYPE_REPEATING_SLACK);
    } else {
      mHiddenPluginTimer->InitWithCallback(this, HIDDEN_PLUGIN_DELAY, nsITimer::TYPE_REPEATING_SLACK);
    }
  }
#endif
}

void nsPluginHost::RemoveIdleTimeTarget(nsIPluginInstanceOwner* objectFrame)
{
#ifdef MAC_CARBON_PLUGINS
  bool visibleRemoved = mVisibleTimerTargets.RemoveElement(objectFrame);
  if (visibleRemoved && mVisibleTimerTargets.IsEmpty()) {
    mVisiblePluginTimer->Cancel();
  }

  bool hiddenRemoved = mHiddenTimerTargets.RemoveElement(objectFrame);
  if (hiddenRemoved && mHiddenTimerTargets.IsEmpty()) {
    mHiddenPluginTimer->Cancel();
  }

  NS_ASSERTION(!(hiddenRemoved && visibleRemoved), "Plugin instance received visible and hidden idle event notifications");
#endif
}

NS_IMETHODIMP nsPluginHost::Notify(nsITimer* timer)
{
#ifdef MAC_CARBON_PLUGINS
  if (timer == mVisiblePluginTimer) {
    nsTObserverArray<nsIPluginInstanceOwner*>::ForwardIterator iter(mVisibleTimerTargets);
    while (iter.HasMore()) {
      iter.GetNext()->SendIdleEvent();
    }
    return NS_OK;
  } else if (timer == mHiddenPluginTimer) {
    nsTObserverArray<nsIPluginInstanceOwner*>::ForwardIterator iter(mHiddenTimerTargets);
    while (iter.HasMore()) {
      iter.GetNext()->SendIdleEvent();
    }
    return NS_OK;
  }
#endif

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
  wm->GetXULWindowEnumerator(nsnull, getter_AddRefs(windowList));
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
      bool aFlag;
      nsCOMPtr<nsIWidget> widget;
      baseWin->GetMainWidget(getter_AddRefs(widget));
      if (widget && !widget->GetParent() &&
          NS_SUCCEEDED(widget->IsVisible(aFlag)) && aFlag == true &&
          NS_SUCCEEDED(widget->IsEnabled(&aFlag)) && aFlag == false) {
        nsIWidget * child = widget->GetFirstChild();
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
    obsService->NotifyObservers(propbag, "plugin-crashed", nsnull);
    // see if an observer submitted a crash report.
    propbag->GetPropertyAsBool(NS_LITERAL_STRING("submittedCrashReport"),
                               &submittedCrashReport);
  }

  // Invalidate each nsPluginInstanceTag for the crashed plugin

  for (PRUint32 i = mInstances.Length(); i > 0; i--) {
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
  // out nsPluginTag.mEntryPoint. The next time we try to create an
  // instance of this plugin we reload it (launch a new plugin process).

  crashedPluginTag->mEntryPoint = nsnull;

#ifdef XP_WIN
  CheckForDisabledWindows();
#endif
}

nsNPAPIPluginInstance*
nsPluginHost::FindInstance(const char *mimetype)
{
  for (PRUint32 i = 0; i < mInstances.Length(); i++) {
    nsNPAPIPluginInstance* instance = mInstances[i];

    const char* mt;
    nsresult rv = instance->GetMIMEType(&mt);
    if (NS_FAILED(rv))
      continue;

    if (PL_strcasecmp(mt, mimetype) == 0)
      return instance;
  }

  return nsnull;
}

nsNPAPIPluginInstance*
nsPluginHost::FindOldestStoppedInstance()
{
  nsNPAPIPluginInstance *oldestInstance = nsnull;
  TimeStamp oldestTime = TimeStamp::Now();
  for (PRUint32 i = 0; i < mInstances.Length(); i++) {
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

PRUint32
nsPluginHost::StoppedInstanceCount()
{
  PRUint32 stoppedCount = 0;
  for (PRUint32 i = 0; i < mInstances.Length(); i++) {
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
nsPluginHost::DestroyRunningInstances(nsISupportsArray* aReloadDocs, nsPluginTag* aPluginTag)
{
  for (PRInt32 i = mInstances.Length(); i > 0; i--) {
    nsNPAPIPluginInstance *instance = mInstances[i - 1];
    if (instance->IsRunning() && (!aPluginTag || aPluginTag == TagForPlugin(instance->GetPlugin()))) {
      instance->SetWindow(nsnull);
      instance->Stop();

      // If we've been passed an array to return, lets collect all our documents,
      // removing duplicates. These will be reframed (embedded) or reloaded (full-page) later
      // to kickstart our instances.
      if (aReloadDocs) {
        nsCOMPtr<nsIPluginInstanceOwner> owner;
        instance->GetOwner(getter_AddRefs(owner));
        if (owner) {
          nsCOMPtr<nsIDocument> doc;
          owner->GetDocument(getter_AddRefs(doc));
          if (doc && aReloadDocs->IndexOf(doc) == -1)  // don't allow for duplicates
            aReloadDocs->AppendElement(doc);
        }
      }

      // Get rid of all the instances without the possibility of caching.
      nsPluginTag* pluginTag = TagForPlugin(instance->GetPlugin());
      instance->SetWindow(nsnull);
      instance->Destroy();
      mInstances.RemoveElement(instance);
      OnPluginInstanceDestroyed(pluginTag);
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

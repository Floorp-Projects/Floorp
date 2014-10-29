/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginHost_h_
#define nsPluginHost_h_

#include "nsIPluginHost.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "prlink.h"
#include "prclist.h"
#include "npapi.h"
#include "nsIPluginTag.h"
#include "nsPluginsDir.h"
#include "nsPluginDirServiceProvider.h"
#include "nsAutoPtr.h"
#include "nsWeakPtr.h"
#include "nsIPrompt.h"
#include "nsWeakReference.h"
#include "MainThreadUtils.h"
#include "nsTArray.h"
#include "nsTObserverArray.h"
#include "nsITimer.h"
#include "nsPluginTags.h"
#include "nsPluginPlayPreviewInfo.h"
#include "nsIEffectiveTLDService.h"
#include "nsIIDNService.h"
#include "nsCRT.h"
#include "mozilla/plugins/PluginTypes.h"

class nsNPAPIPlugin;
class nsIComponentManager;
class nsIFile;
class nsIChannel;
class nsPluginNativeWindow;
class nsObjectLoadingContent;
class nsPluginInstanceOwner;
class nsPluginUnloadRunnable;
class nsNPAPIPluginInstance;
class nsNPAPIPluginStreamListener;
class nsIPluginInstanceOwner;
class nsIInputStream;
class nsIStreamListener;

class nsInvalidPluginTag : public nsISupports
{
  virtual ~nsInvalidPluginTag();
public:
  explicit nsInvalidPluginTag(const char* aFullPath, int64_t aLastModifiedTime = 0);

  NS_DECL_ISUPPORTS

  nsCString   mFullPath;
  int64_t     mLastModifiedTime;
  bool        mSeen;

  nsRefPtr<nsInvalidPluginTag> mPrev;
  nsRefPtr<nsInvalidPluginTag> mNext;
};

class nsPluginHost MOZ_FINAL : public nsIPluginHost,
                               public nsIObserver,
                               public nsITimerCallback,
                               public nsSupportsWeakReference
{
  virtual ~nsPluginHost();
public:
  nsPluginHost();

  static already_AddRefed<nsPluginHost> GetInst();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINHOST
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  nsresult Init();
  nsresult LoadPlugins();
  nsresult UnloadPlugins();

  nsresult SetUpPluginInstance(const char *aMimeType,
                               nsIURI *aURL,
                               nsPluginInstanceOwner *aOwner);
  bool PluginExistsForType(const char* aMimeType);

  nsresult IsPluginEnabledForExtension(const char* aExtension, const char* &aMimeType);

  void GetPlugins(nsTArray<nsRefPtr<nsPluginTag> >& aPluginArray);
  void FindPluginsForContent(uint32_t aPluginEpoch,
                             nsTArray<mozilla::plugins::PluginTag>* aPlugins,
                             uint32_t* aNewPluginEpoch);

  nsresult GetURL(nsISupports* pluginInst,
                  const char* url,
                  const char* target,
                  nsNPAPIPluginStreamListener* streamListener,
                  const char* altHost,
                  const char* referrer,
                  bool forceJSEnabled);
  nsresult PostURL(nsISupports* pluginInst,
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
                   const char* postHeaders);

  nsresult FindProxyForURL(const char* url, char* *result);
  nsresult UserAgent(const char **retstring);
  nsresult ParsePostBufferToFixHeaders(const char *inPostData, uint32_t inPostDataLen,
                                       char **outPostData, uint32_t *outPostDataLen);
  nsresult CreateTempFileToPost(const char *aPostDataURL, nsIFile **aTmpFile);
  nsresult NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow);

  void AddIdleTimeTarget(nsIPluginInstanceOwner* objectFrame, bool isVisible);
  void RemoveIdleTimeTarget(nsIPluginInstanceOwner* objectFrame);

  nsresult GetPluginName(nsNPAPIPluginInstance *aPluginInstance, const char** aPluginName);
  nsresult StopPluginInstance(nsNPAPIPluginInstance* aInstance);
  nsresult GetPluginTagForInstance(nsNPAPIPluginInstance *aPluginInstance, nsIPluginTag **aPluginTag);

  nsresult
  NewPluginURLStream(const nsString& aURL, 
                     nsNPAPIPluginInstance *aInstance, 
                     nsNPAPIPluginStreamListener *aListener,
                     nsIInputStream *aPostStream = nullptr,
                     const char *aHeadersData = nullptr, 
                     uint32_t aHeadersDataLen = 0);

  nsresult
  GetURLWithHeaders(nsNPAPIPluginInstance *pluginInst, 
                    const char* url, 
                    const char* target = nullptr,
                    nsNPAPIPluginStreamListener* streamListener = nullptr,
                    const char* altHost = nullptr,
                    const char* referrer = nullptr,
                    bool forceJSEnabled = false,
                    uint32_t getHeadersLength = 0, 
                    const char* getHeaders = nullptr);

  nsresult
  DoURLLoadSecurityCheck(nsNPAPIPluginInstance *aInstance,
                         const char* aURL);

  nsresult
  AddHeadersToChannel(const char *aHeadersData, uint32_t aHeadersDataLen, 
                      nsIChannel *aGenericChannel);

  static nsresult GetPluginTempDir(nsIFile **aDir);

  // Writes updated plugins settings to disk and unloads the plugin
  // if it is now disabled
  nsresult UpdatePluginInfo(nsPluginTag* aPluginTag);

  // Helper that checks if a type is whitelisted in plugin.allowed_types.
  // Always returns true if plugin.allowed_types is not set
  static bool IsTypeWhitelisted(const char *aType);

  // checks whether aTag is a "java" plugin tag (a tag for a plugin
  // that does Java)
  static bool IsJavaMIMEType(const char *aType);

  static nsresult PostPluginUnloadEvent(PRLibrary* aLibrary);

  void PluginCrashed(nsNPAPIPlugin* plugin,
                     const nsAString& pluginDumpID,
                     const nsAString& browserDumpID);

  nsNPAPIPluginInstance *FindInstance(const char *mimetype);
  nsNPAPIPluginInstance *FindOldestStoppedInstance();
  uint32_t StoppedInstanceCount();

  nsTArray< nsRefPtr<nsNPAPIPluginInstance> > *InstanceArray();

  void DestroyRunningInstances(nsPluginTag* aPluginTag);

  // Return the tag for |aLibrary| if found, nullptr if not.
  nsPluginTag* FindTagForLibrary(PRLibrary* aLibrary);

  // The last argument should be false if we already have an in-flight stream
  // and don't need to set up a new stream.
  nsresult InstantiatePluginInstance(const char *aMimeType, nsIURI* aURL,
                                     nsObjectLoadingContent *aContent,
                                     nsPluginInstanceOwner** aOwner);

  // Does not accept nullptr and should never fail.
  nsPluginTag* TagForPlugin(nsNPAPIPlugin* aPlugin);

  nsresult GetPlugin(const char *aMimeType, nsNPAPIPlugin** aPlugin);
  nsresult GetPluginForContentProcess(uint32_t aPluginId, nsNPAPIPlugin** aPlugin);
  void NotifyContentModuleDestroyed(uint32_t aPluginId);

  nsresult NewPluginStreamListener(nsIURI* aURL,
                                   nsNPAPIPluginInstance* aInstance,
                                   nsIStreamListener **aStreamListener);

private:
  friend class nsPluginUnloadRunnable;

  nsresult
  TrySetUpPluginInstance(const char *aMimeType, nsIURI *aURL, nsPluginInstanceOwner *aOwner);

  nsPluginTag*
  FindPreferredPlugin(const InfallibleTArray<nsPluginTag*>& matches);

  // Return an nsPluginTag for this type, if any.  If aCheckEnabled is
  // true, only enabled plugins will be returned.
  nsPluginTag*
  FindPluginForType(const char* aMimeType, bool aCheckEnabled);

  nsPluginTag*
  FindPluginEnabledForExtension(const char* aExtension, const char* &aMimeType);

  nsresult
  FindStoppedPluginForURL(nsIURI* aURL, nsIPluginInstanceOwner *aOwner);

  nsresult FindPluginsInContent(bool aCreatePluginList, bool * aPluginsChanged);

  nsresult
  FindPlugins(bool aCreatePluginList, bool * aPluginsChanged);

  // Registers or unregisters the given mime type with the category manager
  // (performs no checks - see UpdateCategoryManager)
  enum nsRegisterType { ePluginRegister, ePluginUnregister };
  void RegisterWithCategoryManager(nsCString &aMimeType, nsRegisterType aType);

  void AddPluginTag(nsPluginTag* aPluginTag);

  nsresult
  ScanPluginsDirectory(nsIFile *pluginsDir,
                       bool aCreatePluginList,
                       bool *aPluginsChanged);

  nsresult
  ScanPluginsDirectoryList(nsISimpleEnumerator *dirEnum,
                           bool aCreatePluginList,
                           bool *aPluginsChanged);

  nsresult EnsurePluginLoaded(nsPluginTag* aPluginTag);

  bool IsRunningPlugin(nsPluginTag * aPluginTag);

  // Stores all plugins info into the registry
  nsresult WritePluginInfo();

  // Loads all cached plugins info into mCachedPlugins
  nsresult ReadPluginInfo();

  // Given a file path, returns the plugins info from our cache
  // and removes it from the cache.
  void RemoveCachedPluginsInfo(const char *filePath,
                               nsPluginTag **result);

  // Checks to see if a tag object is in our list of live tags.
  bool IsLiveTag(nsIPluginTag* tag);
  
  // Checks our list of live tags for an equivalent tag.
  nsPluginTag* HaveSamePlugin(const nsPluginTag * aPluginTag);
    
  // Returns the first plugin at |path|
  nsPluginTag* FirstPluginWithPath(const nsCString& path);
  nsPluginTag* PluginWithId(uint32_t aId);

  nsresult EnsurePrivateDirServiceProvider();

  void OnPluginInstanceDestroyed(nsPluginTag* aPluginTag);

  // To be used by the chrome process whenever the set of plugins changes.
  void IncrementChromeEpoch();

  // To be used by the chrome process; returns the current epoch.
  uint32_t ChromeEpoch();

  // To be used by the content process to get/set the last observed epoch value
  // from the chrome process.
  uint32_t ChromeEpochForContent();
  void SetChromeEpochForContent(uint32_t aEpoch);

  nsRefPtr<nsPluginTag> mPlugins;
  nsRefPtr<nsPluginTag> mCachedPlugins;
  nsRefPtr<nsInvalidPluginTag> mInvalidPlugins;
  nsTArray< nsRefPtr<nsPluginPlayPreviewInfo> > mPlayPreviewMimeTypes;
  bool mPluginsLoaded;

  // set by pref plugin.override_internal_types
  bool mOverrideInternalTypes;

  // set by pref plugin.disable
  bool mPluginsDisabled;
  // set by pref plugins.click_to_play
  bool mPluginsClickToPlay;

  // Any instances in this array will have valid plugin objects via GetPlugin().
  // When removing an instance it might not die - be sure to null out it's plugin.
  nsTArray< nsRefPtr<nsNPAPIPluginInstance> > mInstances;

  nsCOMPtr<nsIFile> mPluginRegFile;
#ifdef XP_WIN
  nsRefPtr<nsPluginDirServiceProvider> mPrivateDirServiceProvider;
#endif

  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
  nsCOMPtr<nsIIDNService> mIDNService;

  // Helpers for ClearSiteData and SiteHasData.
  nsresult NormalizeHostname(nsCString& host);
  nsresult EnumerateSiteData(const nsACString& domain,
                             const InfallibleTArray<nsCString>& sites,
                             InfallibleTArray<nsCString>& result,
                             bool firstMatchOnly);

  nsWeakPtr mCurrentDocument; // weak reference, we use it to id document only

  // This epoch increases each time we load the list of plugins from disk.
  // In the chrome process, this stores the actual epoch.
  // In the content process, this stores the last epoch value observed
  // when reading plugins from chrome.
  uint32_t mPluginEpoch;

  static nsIFile *sPluginTempDir;

  // We need to hold a global ptr to ourselves because we register for
  // two different CIDs for some reason...
  static nsPluginHost* sInst;
};

class MOZ_STACK_CLASS PluginDestructionGuard : protected PRCList
{
public:
  explicit PluginDestructionGuard(nsNPAPIPluginInstance *aInstance);

  explicit PluginDestructionGuard(NPP npp);

  ~PluginDestructionGuard();

  static bool DelayDestroy(nsNPAPIPluginInstance *aInstance);

protected:
  void Init()
  {
    NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread");

    mDelayedDestroy = false;

    PR_INIT_CLIST(this);
    PR_INSERT_BEFORE(this, &sListHead);
  }

  nsRefPtr<nsNPAPIPluginInstance> mInstance;
  bool mDelayedDestroy;

  static PRCList sListHead;
};

#endif // nsPluginHost_h_

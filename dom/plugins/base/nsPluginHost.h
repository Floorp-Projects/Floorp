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
#include "nsNPAPIPluginInstance.h"
#include "nsIPluginTag.h"
#include "nsPluginsDir.h"
#include "nsPluginDirServiceProvider.h"
#include "nsAutoPtr.h"
#include "nsWeakPtr.h"
#include "nsIPrompt.h"
#include "nsISupportsArray.h"
#include "nsWeakReference.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsTObserverArray.h"
#include "nsITimer.h"
#include "nsPluginTags.h"
#include "nsIEffectiveTLDService.h"
#include "nsIIDNService.h"
#include "nsCRT.h"

class nsNPAPIPlugin;
class nsIComponentManager;
class nsIFile;
class nsIChannel;
class nsPluginNativeWindow;
class nsObjectLoadingContent;
class nsPluginInstanceOwner;

#if defined(XP_MACOSX) && !defined(NP_NO_CARBON)
#define MAC_CARBON_PLUGINS
#endif

class nsInvalidPluginTag : public nsISupports
{
public:
  nsInvalidPluginTag(const char* aFullPath, PRInt64 aLastModifiedTime = 0);
  virtual ~nsInvalidPluginTag();
  
  NS_DECL_ISUPPORTS
  
  nsCString   mFullPath;
  PRInt64     mLastModifiedTime;
  bool        mSeen;
  
  nsRefPtr<nsInvalidPluginTag> mPrev;
  nsRefPtr<nsInvalidPluginTag> mNext;
};

class nsPluginHost : public nsIPluginHost,
                     public nsIObserver,
                     public nsITimerCallback,
                     public nsSupportsWeakReference
{
public:
  nsPluginHost();
  virtual ~nsPluginHost();

  static nsPluginHost* GetInst();

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
                               nsIPluginInstanceOwner *aOwner);
  nsresult IsPluginEnabledForType(const char* aMimeType);
  nsresult IsPluginEnabledForExtension(const char* aExtension, const char* &aMimeType);

  nsresult GetPluginCount(PRUint32* aPluginCount);
  nsresult GetPlugins(PRUint32 aPluginCount, nsIDOMPlugin** aPluginArray);

  nsresult GetURL(nsISupports* pluginInst,
                  const char* url,
                  const char* target,
                  nsNPAPIPluginStreamListener* streamListener,
                  const char* altHost,
                  const char* referrer,
                  bool forceJSEnabled);
  nsresult PostURL(nsISupports* pluginInst,
                   const char* url,
                   PRUint32 postDataLen,
                   const char* postData,
                   bool isFile,
                   const char* target,
                   nsNPAPIPluginStreamListener* streamListener,
                   const char* altHost,
                   const char* referrer,
                   bool forceJSEnabled,
                   PRUint32 postHeadersLength,
                   const char* postHeaders);

  nsresult FindProxyForURL(const char* url, char* *result);
  nsresult UserAgent(const char **retstring);
  nsresult ParsePostBufferToFixHeaders(const char *inPostData, PRUint32 inPostDataLen,
                                       char **outPostData, PRUint32 *outPostDataLen);
  nsresult CreateTempFileToPost(const char *aPostDataURL, nsIFile **aTmpFile);
  nsresult NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow);

  void AddIdleTimeTarget(nsIPluginInstanceOwner* objectFrame, bool isVisible);
  void RemoveIdleTimeTarget(nsIPluginInstanceOwner* objectFrame);

  nsresult GetPluginName(nsNPAPIPluginInstance *aPluginInstance, const char** aPluginName);
  nsresult StopPluginInstance(nsNPAPIPluginInstance* aInstance);
  nsresult HandleBadPlugin(PRLibrary* aLibrary, nsNPAPIPluginInstance *aInstance);
  nsresult GetPluginTagForInstance(nsNPAPIPluginInstance *aPluginInstance, nsIPluginTag **aPluginTag);

  nsresult
  NewPluginURLStream(const nsString& aURL, 
                     nsNPAPIPluginInstance *aInstance, 
                     nsNPAPIPluginStreamListener *aListener,
                     nsIInputStream *aPostStream = nsnull,
                     const char *aHeadersData = nsnull, 
                     PRUint32 aHeadersDataLen = 0);

  nsresult
  GetURLWithHeaders(nsNPAPIPluginInstance *pluginInst, 
                    const char* url, 
                    const char* target = NULL,
                    nsNPAPIPluginStreamListener* streamListener = NULL,
                    const char* altHost = NULL,
                    const char* referrer = NULL,
                    bool forceJSEnabled = false,
                    PRUint32 getHeadersLength = 0, 
                    const char* getHeaders = NULL);

  nsresult
  DoURLLoadSecurityCheck(nsNPAPIPluginInstance *aInstance,
                         const char* aURL);

  nsresult
  AddHeadersToChannel(const char *aHeadersData, PRUint32 aHeadersDataLen, 
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

  static nsresult GetPrompt(nsIPluginInstanceOwner *aOwner, nsIPrompt **aPrompt);

  static nsresult PostPluginUnloadEvent(PRLibrary* aLibrary);

  void PluginCrashed(nsNPAPIPlugin* plugin,
                     const nsAString& pluginDumpID,
                     const nsAString& browserDumpID);

  nsNPAPIPluginInstance *FindInstance(const char *mimetype);
  nsNPAPIPluginInstance *FindOldestStoppedInstance();
  PRUint32 StoppedInstanceCount();

  nsTArray< nsRefPtr<nsNPAPIPluginInstance> > *InstanceArray();

  void DestroyRunningInstances(nsISupportsArray* aReloadDocs, nsPluginTag* aPluginTag);

  // Return the tag for |aLibrary| if found, nsnull if not.
  nsPluginTag* FindTagForLibrary(PRLibrary* aLibrary);

  // The last argument should be false if we already have an in-flight stream
  // and don't need to set up a new stream.
  nsresult InstantiateEmbeddedPluginInstance(const char *aMimeType, nsIURI* aURL,
                                             nsObjectLoadingContent *aContent,
                                             nsPluginInstanceOwner** aOwner);

  nsresult InstantiateFullPagePluginInstance(const char *aMimeType,
                                             nsIURI* aURI,
                                             nsObjectLoadingContent *aContent,
                                             nsPluginInstanceOwner **aOwner,
                                             nsIStreamListener **aStreamListener);

  // Does not accept NULL and should never fail.
  nsPluginTag* TagForPlugin(nsNPAPIPlugin* aPlugin);

  nsresult GetPlugin(const char *aMimeType, nsNPAPIPlugin** aPlugin);

  nsresult NewEmbeddedPluginStreamListener(nsIURI* aURL, nsObjectLoadingContent *aContent,
                                           nsNPAPIPluginInstance* aInstance,
                                           nsIStreamListener **aStreamListener);

  nsresult NewFullPagePluginStreamListener(nsIURI* aURI,
                                           nsNPAPIPluginInstance *aInstance,
                                           nsIStreamListener **aStreamListener);

private:
  nsresult
  TrySetUpPluginInstance(const char *aMimeType, nsIURI *aURL, nsIPluginInstanceOwner *aOwner);

  nsresult
  NewEmbeddedPluginStream(nsIURI* aURL, nsObjectLoadingContent *aContent, nsNPAPIPluginInstance* aInstance);

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

  nsresult
  FindPlugins(bool aCreatePluginList, bool * aPluginsChanged);

  // Registers or unregisters the given mime type with the category manager
  // (performs no checks - see UpdateCategoryManager)
  enum nsRegisterType { ePluginRegister, ePluginUnregister };
  void RegisterWithCategoryManager(nsCString &aMimeType, nsRegisterType aType);

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

  nsresult EnsurePrivateDirServiceProvider();

  void OnPluginInstanceDestroyed(nsPluginTag* aPluginTag);

  nsRefPtr<nsPluginTag> mPlugins;
  nsRefPtr<nsPluginTag> mCachedPlugins;
  nsRefPtr<nsInvalidPluginTag> mInvalidPlugins;
  bool mPluginsLoaded;
  bool mDontShowBadPluginMessage;

  // set by pref plugin.override_internal_types
  bool mOverrideInternalTypes;

  // set by pref plugin.disable
  bool mPluginsDisabled;

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
                             const nsTArray<nsCString>& sites,
                             InfallibleTArray<nsCString>& result,
                             bool firstMatchOnly);

  nsWeakPtr mCurrentDocument; // weak reference, we use it to id document only

  static nsIFile *sPluginTempDir;

  // We need to hold a global ptr to ourselves because we register for
  // two different CIDs for some reason...
  static nsPluginHost* sInst;

#ifdef MAC_CARBON_PLUGINS
  nsCOMPtr<nsITimer> mVisiblePluginTimer;
  nsTObserverArray<nsIPluginInstanceOwner*> mVisibleTimerTargets;
  nsCOMPtr<nsITimer> mHiddenPluginTimer;
  nsTObserverArray<nsIPluginInstanceOwner*> mHiddenTimerTargets;
#endif
};

class NS_STACK_CLASS PluginDestructionGuard : protected PRCList
{
public:
  PluginDestructionGuard(nsNPAPIPluginInstance *aInstance)
    : mInstance(aInstance)
  {
    Init();
  }

  PluginDestructionGuard(NPP npp)
    : mInstance(npp ? static_cast<nsNPAPIPluginInstance*>(npp->ndata) : nsnull)
  {
    Init();
  }

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

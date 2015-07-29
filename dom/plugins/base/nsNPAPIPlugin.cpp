/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

/* This must occur *after* layers/PLayerTransaction.h to avoid typedefs conflicts. */
#include "mozilla/ArrayUtils.h"

#include "pratom.h"
#include "prmem.h"
#include "prenv.h"
#include "prclist.h"

#include "jsfriendapi.h"

#include "nsPluginHost.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginInstance.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsPluginStreamListenerPeer.h"
#include "nsIServiceManager.h"
#include "nsThreadUtils.h"
#include "mozilla/Preferences.h"
#include "nsPluginInstanceOwner.h"

#include "nsPluginsDir.h"
#include "nsPluginLogging.h"

#include "nsIDOMElement.h"
#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIUnicodeNormalizer.h"
#include "nsDOMJSUtils.h"
#include "nsIPrincipal.h"
#include "nsWildCard.h"
#include "nsContentUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIXULRuntime.h"
#include "nsIXPConnect.h"

#include "nsIObserverService.h"
#include <prinrval.h>

#ifdef MOZ_WIDGET_COCOA
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>
#include "nsCocoaFeatures.h"
#include "PluginUtilsOSX.h"
#endif

// needed for nppdf plugin
#if (MOZ_WIDGET_GTK)
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#if (MOZ_WIDGET_GTK == 2)
#include "gtk2xtbin.h"
#endif
#endif

#include "nsJSUtils.h"
#include "nsJSNPRuntime.h"
#include "nsIHttpAuthManager.h"
#include "nsICookieService.h"
#include "nsILoadContext.h"
#include "nsIDocShell.h"

#include "nsNetUtil.h"
#include "nsNetCID.h"

#include "mozilla/Mutex.h"
#include "mozilla/PluginLibrary.h"
using mozilla::PluginLibrary;

#include "mozilla/PluginPRLibrary.h"
using mozilla::PluginPRLibrary;

#include "mozilla/plugins/PluginModuleParent.h"
using mozilla::plugins::PluginModuleChromeParent;
using mozilla::plugins::PluginModuleContentParent;

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#include "mozilla/WindowsVersion.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/Compatibility.h"
#endif
#endif

#ifdef MOZ_WIDGET_ANDROID
#include <android/log.h>
#include "android_npapi.h"
#include "ANPBase.h"
#include "AndroidBridge.h"
#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#endif

#include "nsIAudioChannelAgent.h"

using namespace mozilla;
using namespace mozilla::plugins::parent;

// We should make this const...
static NPNetscapeFuncs sBrowserFuncs = {
  sizeof(sBrowserFuncs),
  (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR,
  _geturl,
  _posturl,
  _requestread,
  _newstream,
  _write,
  _destroystream,
  _status,
  _useragent,
  _memalloc,
  _memfree,
  _memflush,
  _reloadplugins,
  _getJavaEnv,
  _getJavaPeer,
  _geturlnotify,
  _posturlnotify,
  _getvalue,
  _setvalue,
  _invalidaterect,
  _invalidateregion,
  _forceredraw,
  _getstringidentifier,
  _getstringidentifiers,
  _getintidentifier,
  _identifierisstring,
  _utf8fromidentifier,
  _intfromidentifier,
  _createobject,
  _retainobject,
  _releaseobject,
  _invoke,
  _invokeDefault,
  _evaluate,
  _getproperty,
  _setproperty,
  _removeproperty,
  _hasproperty,
  _hasmethod,
  _releasevariantvalue,
  _setexception,
  _pushpopupsenabledstate,
  _poppopupsenabledstate,
  _enumerate,
  _pluginthreadasynccall,
  _construct,
  _getvalueforurl,
  _setvalueforurl,
  _getauthenticationinfo,
  _scheduletimer,
  _unscheduletimer,
  _popupcontextmenu,
  _convertpoint,
  nullptr, // handleevent, unimplemented
  nullptr, // unfocusinstance, unimplemented
  _urlredirectresponse
};

static Mutex *sPluginThreadAsyncCallLock = nullptr;
static PRCList sPendingAsyncCalls = PR_INIT_STATIC_CLIST(&sPendingAsyncCalls);

// POST/GET stream type
enum eNPPStreamTypeInternal {
  eNPPStreamTypeInternal_Get,
  eNPPStreamTypeInternal_Post
};

PRIntervalTime NS_NotifyBeginPluginCall(NSPluginCallReentry aReentryState)
{
  nsNPAPIPluginInstance::BeginPluginCall(aReentryState);
  return PR_IntervalNow();
}

// This function sends a notification using the observer service to any object
// registered to listen to the "experimental-notify-plugin-call" subject.
// Each "experimental-notify-plugin-call" notification carries with it the run
// time value in milliseconds that the call took to execute.
void NS_NotifyPluginCall(PRIntervalTime startTime, NSPluginCallReentry aReentryState)
{
  nsNPAPIPluginInstance::EndPluginCall(aReentryState);

  PRIntervalTime endTime = PR_IntervalNow() - startTime;
  nsCOMPtr<nsIObserverService> notifyUIService =
    mozilla::services::GetObserverService();
  if (!notifyUIService)
    return;

  float runTimeInSeconds = float(endTime) / PR_TicksPerSecond();
  nsAutoString runTimeString;
  runTimeString.AppendFloat(runTimeInSeconds);
  const char16_t* runTime = runTimeString.get();
  notifyUIService->NotifyObservers(nullptr, "experimental-notify-plugin-call",
                                   runTime);
}

static void CheckClassInitialized()
{
  static bool initialized = false;

  if (initialized)
    return;

  if (!sPluginThreadAsyncCallLock)
    sPluginThreadAsyncCallLock = new Mutex("nsNPAPIPlugin.sPluginThreadAsyncCallLock");

  initialized = true;

  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,("NPN callbacks initialized\n"));
}

NS_IMPL_ISUPPORTS0(nsNPAPIPlugin)

nsNPAPIPlugin::nsNPAPIPlugin()
{
  memset((void*)&mPluginFuncs, 0, sizeof(mPluginFuncs));
  mPluginFuncs.size = sizeof(mPluginFuncs);
  mPluginFuncs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;

  mLibrary = nullptr;
}

nsNPAPIPlugin::~nsNPAPIPlugin()
{
  delete mLibrary;
  mLibrary = nullptr;
}

void
nsNPAPIPlugin::PluginCrashed(const nsAString& pluginDumpID,
                             const nsAString& browserDumpID)
{
  nsRefPtr<nsPluginHost> host = nsPluginHost::GetInst();
  host->PluginCrashed(this, pluginDumpID, browserDumpID);
}

bool
nsNPAPIPlugin::RunPluginOOP(const nsPluginTag *aPluginTag)
{
  if (XRE_IsContentProcess()) {
    return true;
  }

#if (MOZ_WIDGET_GTK == 3)
  // We force OOP on Linux/GTK3 because some plugins use GTK2 and both GTK
  // libraries can't be loaded in the same process.
  return true;
#else
  if (PR_GetEnv("MOZ_DISABLE_OOP_PLUGINS")) {
    return false;
  }

  if (!aPluginTag) {
    return false;
  }

#ifdef ACCESSIBILITY
  // Certain assistive technologies don't want oop Flash, thus we have a special
  // pref for them to disable oop Flash (refer to bug 785047 for details).
  bool useA11yPref = false;
#ifdef XP_WIN
  useA11yPref =  a11y::Compatibility::IsJAWS();
#endif
#endif

#ifdef XP_WIN
  // On Windows Vista+, we force Flash to run in OOPP mode because Adobe
  // doesn't test Flash in-process and there are known stability bugs.
  if (aPluginTag->mIsFlashPlugin && IsVistaOrLater()) {
#ifdef ACCESSIBILITY
    if (!useA11yPref)
      return true;
#else
    return true;
#endif
  }
#endif

  nsIPrefBranch* prefs = Preferences::GetRootBranch();
  if (!prefs) {
    return false;
  }

  // Get per-library whitelist/blacklist pref string
  // "dom.ipc.plugins.enabled.filename.dll" and fall back to the default value
  // of "dom.ipc.plugins.enabled"
  // The "filename.dll" part can contain shell wildcard pattern

  nsAutoCString prefFile(aPluginTag->mFullPath.get());
  int32_t slashPos = prefFile.RFindCharInSet("/\\");
  if (kNotFound == slashPos)
    return false;
  prefFile.Cut(0, slashPos + 1);
  ToLowerCase(prefFile);

#ifdef XP_MACOSX
#if defined(__i386__)
  nsAutoCString prefGroupKey("dom.ipc.plugins.enabled.i386.");
#elif defined(__x86_64__)
  nsAutoCString prefGroupKey("dom.ipc.plugins.enabled.x86_64.");
#elif defined(__ppc__)
  nsAutoCString prefGroupKey("dom.ipc.plugins.enabled.ppc.");
#endif
#else
  nsAutoCString prefGroupKey("dom.ipc.plugins.enabled.");
#endif

#ifdef ACCESSIBILITY
  if (useA11yPref)
    prefGroupKey.AssignLiteral("dom.ipc.plugins.enabled.a11y.");
#endif

  if (BrowserTabsRemoteAutostart()) {
    // dom.ipc.plugins.java.enabled is obsolete in Nightly w/e10s, we've
    // flipped the default to ON and now have a force-disable pref. This
    // way we don't break non-e10s browsers.
    if (aPluginTag->mIsJavaPlugin &&
        Preferences::GetBool("dom.ipc.plugins.java.force-disable", false)) {
      return false;
    }
  } else {
    // Java plugins include a number of different file names,
    // so use the mime type (mIsJavaPlugin) and a special pref.
    if (aPluginTag->mIsJavaPlugin &&
        !Preferences::GetBool("dom.ipc.plugins.java.enabled", true)) {
      return false;
    }
  }

  uint32_t prefCount;
  char** prefNames;
  nsresult rv = prefs->GetChildList(prefGroupKey.get(),
                                    &prefCount, &prefNames);

  bool oopPluginsEnabled = false;
  bool prefSet = false;

  if (NS_SUCCEEDED(rv) && prefCount > 0) {
    uint32_t prefixLength = prefGroupKey.Length();
    for (uint32_t currentPref = 0; currentPref < prefCount; currentPref++) {
      // Get the mask
      const char* maskStart = prefNames[currentPref] + prefixLength;
      bool match = false;

      int valid = NS_WildCardValid(maskStart);
      if (valid == INVALID_SXP) {
         continue;
      }
      else if(valid == NON_SXP) {
        // mask is not a shell pattern, compare it as normal string
        match = (strcmp(prefFile.get(), maskStart) == 0);
      }
      else {
        match = (NS_WildCardMatch(prefFile.get(), maskStart, 0) == MATCH);
      }

      if (match && NS_SUCCEEDED(Preferences::GetBool(prefNames[currentPref],
                                                     &oopPluginsEnabled))) {
        prefSet = true;
        break;
      }
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefNames);
  }

  if (!prefSet) {
    oopPluginsEnabled =
#ifdef XP_MACOSX
#if defined(__i386__)
    Preferences::GetBool("dom.ipc.plugins.enabled.i386", false);
#elif defined(__x86_64__)
    Preferences::GetBool("dom.ipc.plugins.enabled.x86_64", false);
#elif defined(__ppc__)
    Preferences::GetBool("dom.ipc.plugins.enabled.ppc", false);
#endif
#else
#ifdef ACCESSIBILITY
    useA11yPref ? Preferences::GetBool("dom.ipc.plugins.enabled.a11y", false) :
#endif
    Preferences::GetBool("dom.ipc.plugins.enabled", false);
#endif
  }

  return oopPluginsEnabled;
#endif
}

inline PluginLibrary*
GetNewPluginLibrary(nsPluginTag *aPluginTag)
{
  if (!aPluginTag) {
    return nullptr;
  }

  if (XRE_IsContentProcess()) {
    return PluginModuleContentParent::LoadModule(aPluginTag->mId);
  }

  if (nsNPAPIPlugin::RunPluginOOP(aPluginTag)) {
    return PluginModuleChromeParent::LoadModule(aPluginTag->mFullPath.get(), aPluginTag->mId, aPluginTag);
  }
  return new PluginPRLibrary(aPluginTag->mFullPath.get(), aPluginTag->mLibrary);
}

// Creates an nsNPAPIPlugin object. One nsNPAPIPlugin object exists per plugin (not instance).
nsresult
nsNPAPIPlugin::CreatePlugin(nsPluginTag *aPluginTag, nsNPAPIPlugin** aResult)
{
  *aResult = nullptr;

  if (!aPluginTag) {
    return NS_ERROR_FAILURE;
  }

  CheckClassInitialized();

  nsRefPtr<nsNPAPIPlugin> plugin = new nsNPAPIPlugin();

  PluginLibrary* pluginLib = GetNewPluginLibrary(aPluginTag);
  if (!pluginLib) {
    return NS_ERROR_FAILURE;
  }

#if defined(XP_MACOSX) || defined(MOZ_WIDGET_ANDROID)
  if (!pluginLib->HasRequiredFunctions()) {
    NS_WARNING("Not all necessary functions exposed by plugin, it will not load.");
    delete pluginLib;
    return NS_ERROR_FAILURE;
  }
#endif

  plugin->mLibrary = pluginLib;
  pluginLib->SetPlugin(plugin);

// Exchange NPAPI entry points.
#if defined(XP_WIN)
  // NP_GetEntryPoints must be called before NP_Initialize on Windows.
  NPError pluginCallError;
  nsresult rv = pluginLib->NP_GetEntryPoints(&plugin->mPluginFuncs, &pluginCallError);
  if (rv != NS_OK || pluginCallError != NPERR_NO_ERROR) {
    return NS_ERROR_FAILURE;
  }

  // NP_Initialize must be called after NP_GetEntryPoints on Windows.
  rv = pluginLib->NP_Initialize(&sBrowserFuncs, &pluginCallError);
  if (rv != NS_OK || pluginCallError != NPERR_NO_ERROR) {
    return NS_ERROR_FAILURE;
  }
#elif defined(XP_MACOSX)
  // NP_Initialize must be called before NP_GetEntryPoints on Mac OS X.
  // We need to match WebKit's behavior.
  NPError pluginCallError;
  nsresult rv = pluginLib->NP_Initialize(&sBrowserFuncs, &pluginCallError);
  if (rv != NS_OK || pluginCallError != NPERR_NO_ERROR) {
    return NS_ERROR_FAILURE;
  }

  rv = pluginLib->NP_GetEntryPoints(&plugin->mPluginFuncs, &pluginCallError);
  if (rv != NS_OK || pluginCallError != NPERR_NO_ERROR) {
    return NS_ERROR_FAILURE;
  }
#elif defined(MOZ_WIDGET_GONK)
#else
  NPError pluginCallError;
  nsresult rv = pluginLib->NP_Initialize(&sBrowserFuncs, &plugin->mPluginFuncs, &pluginCallError);
  if (rv != NS_OK || pluginCallError != NPERR_NO_ERROR) {
    return NS_ERROR_FAILURE;
  }
#endif

  plugin.forget(aResult);
  return NS_OK;
}

PluginLibrary*
nsNPAPIPlugin::GetLibrary()
{
  return mLibrary;
}

NPPluginFuncs*
nsNPAPIPlugin::PluginFuncs()
{
  return &mPluginFuncs;
}

nsresult
nsNPAPIPlugin::Shutdown()
{
  NPP_PLUGIN_LOG(PLUGIN_LOG_BASIC,
                 ("NPP Shutdown to be called: this=%p\n", this));

  NPError shutdownError;
  mLibrary->NP_Shutdown(&shutdownError);

  return NS_OK;
}

nsresult
nsNPAPIPlugin::RetainStream(NPStream *pstream, nsISupports **aRetainedPeer)
{
  if (!aRetainedPeer)
    return NS_ERROR_NULL_POINTER;

  *aRetainedPeer = nullptr;

  if (!pstream || !pstream->ndata)
    return NS_ERROR_NULL_POINTER;

  nsNPAPIStreamWrapper* streamWrapper = static_cast<nsNPAPIStreamWrapper*>(pstream->ndata);
  nsNPAPIPluginStreamListener* listener = streamWrapper->GetStreamListener();
  if (!listener) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIStreamListener* streamListener = listener->GetStreamListenerPeer();
  if (!streamListener) {
    return NS_ERROR_NULL_POINTER;
  }

  *aRetainedPeer = streamListener;
  NS_ADDREF(*aRetainedPeer);
  return NS_OK;
}

// Create a new NPP GET or POST (given in the type argument) url
// stream that may have a notify callback
NPError
MakeNewNPAPIStreamInternal(NPP npp, const char *relativeURL, const char *target,
                          eNPPStreamTypeInternal type,
                          bool bDoNotify = false,
                          void *notifyData = nullptr, uint32_t len = 0,
                          const char *buf = nullptr, NPBool file = false)
{
  if (!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginDestructionGuard guard(npp);

  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *) npp->ndata;
  if (!inst || !inst->IsRunning())
    return NPERR_INVALID_INSTANCE_ERROR;

  nsCOMPtr<nsIPluginHost> pluginHostCOM = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
  nsPluginHost *pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if (!pluginHost) {
    return NPERR_GENERIC_ERROR;
  }

  nsRefPtr<nsNPAPIPluginStreamListener> listener;
  // Set aCallNotify here to false.  If pluginHost->GetURL or PostURL fail,
  // the listener's destructor will do the notification while we are about to
  // return a failure code.
  // Call SetCallNotify(true) below after we are sure we cannot return a failure
  // code.
  if (!target) {
    inst->NewStreamListener(relativeURL, notifyData,
                            getter_AddRefs(listener));
    if (listener) {
      listener->SetCallNotify(false);
    }
  }

  switch (type) {
  case eNPPStreamTypeInternal_Get:
    {
      if (NS_FAILED(pluginHost->GetURL(inst, relativeURL, target, listener,
                                       nullptr, nullptr, false)))
        return NPERR_GENERIC_ERROR;
      break;
    }
  case eNPPStreamTypeInternal_Post:
    {
      if (NS_FAILED(pluginHost->PostURL(inst, relativeURL, len, buf, file,
                                        target, listener, nullptr, nullptr,
                                        false, 0, nullptr)))
        return NPERR_GENERIC_ERROR;
      break;
    }
  default:
    NS_ERROR("how'd I get here");
  }

  if (listener) {
    // SetCallNotify(bDoNotify) here, see comment above.
    listener->SetCallNotify(bDoNotify);
  }

  return NPERR_NO_ERROR;
}

#if defined(MOZ_MEMORY_WINDOWS)
extern "C" size_t malloc_usable_size(const void *ptr);
#endif

namespace {

static char *gNPPException;

class nsPluginThreadRunnable : public nsRunnable,
                               public PRCList
{
public:
  nsPluginThreadRunnable(NPP instance, PluginThreadCallback func,
                         void *userData);
  virtual ~nsPluginThreadRunnable();

  NS_IMETHOD Run();

  bool IsForInstance(NPP instance)
  {
    return (mInstance == instance);
  }

  void Invalidate()
  {
    mFunc = nullptr;
  }

  bool IsValid()
  {
    return (mFunc != nullptr);
  }

private:
  NPP mInstance;
  PluginThreadCallback mFunc;
  void *mUserData;
};

static nsIDocument *
GetDocumentFromNPP(NPP npp)
{
  NS_ENSURE_TRUE(npp, nullptr);

  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)npp->ndata;
  NS_ENSURE_TRUE(inst, nullptr);

  PluginDestructionGuard guard(inst);

  nsRefPtr<nsPluginInstanceOwner> owner = inst->GetOwner();
  NS_ENSURE_TRUE(owner, nullptr);

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));

  return doc;
}

static already_AddRefed<nsIChannel>
GetChannelFromNPP(NPP npp)
{
  nsCOMPtr<nsIDocument> doc = GetDocumentFromNPP(npp);
  if (!doc)
    return nullptr;
  nsCOMPtr<nsPIDOMWindow> domwindow = doc->GetWindow();
  nsCOMPtr<nsIChannel> channel;
  if (domwindow) {
    nsCOMPtr<nsIDocShell> docShell = domwindow->GetDocShell();
    if (docShell) {
      docShell->GetCurrentDocumentChannel(getter_AddRefs(channel));
    }
  }
  return channel.forget();
}

static NPIdentifier
doGetIdentifier(JSContext *cx, const NPUTF8* name)
{
  NS_ConvertUTF8toUTF16 utf16name(name);

  JSString *str = ::JS_AtomizeAndPinUCStringN(cx, utf16name.get(), utf16name.Length());

  if (!str)
    return nullptr;

  return StringToNPIdentifier(cx, str);
}

#if defined(MOZ_MEMORY_WINDOWS)
BOOL
InHeap(HANDLE hHeap, LPVOID lpMem)
{
  BOOL success = FALSE;
  PROCESS_HEAP_ENTRY he;
  he.lpData = nullptr;
  while (HeapWalk(hHeap, &he) != 0) {
    if (he.lpData == lpMem) {
      success = TRUE;
      break;
    }
  }
  HeapUnlock(hHeap);
  return success;
}
#endif

} /* anonymous namespace */

NPPExceptionAutoHolder::NPPExceptionAutoHolder()
  : mOldException(gNPPException)
{
  gNPPException = nullptr;
}

NPPExceptionAutoHolder::~NPPExceptionAutoHolder()
{
  NS_ASSERTION(!gNPPException, "NPP exception not properly cleared!");

  gNPPException = mOldException;
}

nsPluginThreadRunnable::nsPluginThreadRunnable(NPP instance,
                                               PluginThreadCallback func,
                                               void *userData)
  : mInstance(instance), mFunc(func), mUserData(userData)
{
  if (!sPluginThreadAsyncCallLock) {
    // Failed to create lock, not much we can do here then...
    mFunc = nullptr;

    return;
  }

  PR_INIT_CLIST(this);

  {
    MutexAutoLock lock(*sPluginThreadAsyncCallLock);

    nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)instance->ndata;
    if (!inst || !inst->IsRunning()) {
      // The plugin was stopped, ignore this async call.
      mFunc = nullptr;

      return;
    }

    PR_APPEND_LINK(this, &sPendingAsyncCalls);
  }
}

nsPluginThreadRunnable::~nsPluginThreadRunnable()
{
  if (!sPluginThreadAsyncCallLock) {
    return;
  }

  {
    MutexAutoLock lock(*sPluginThreadAsyncCallLock);

    PR_REMOVE_LINK(this);
  }
}

NS_IMETHODIMP
nsPluginThreadRunnable::Run()
{
  if (mFunc) {
    PluginDestructionGuard guard(mInstance);

    NS_TRY_SAFE_CALL_VOID(mFunc(mUserData), nullptr,
                          NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
  }

  return NS_OK;
}

void
OnPluginDestroy(NPP instance)
{
  if (!sPluginThreadAsyncCallLock) {
    return;
  }

  {
    MutexAutoLock lock(*sPluginThreadAsyncCallLock);

    if (PR_CLIST_IS_EMPTY(&sPendingAsyncCalls)) {
      return;
    }

    nsPluginThreadRunnable *r =
      (nsPluginThreadRunnable *)PR_LIST_HEAD(&sPendingAsyncCalls);

    do {
      if (r->IsForInstance(instance)) {
        r->Invalidate();
      }

      r = (nsPluginThreadRunnable *)PR_NEXT_LINK(r);
    } while (r != &sPendingAsyncCalls);
  }
}

void
OnShutdown()
{
  NS_ASSERTION(PR_CLIST_IS_EMPTY(&sPendingAsyncCalls),
               "Pending async plugin call list not cleaned up!");

  if (sPluginThreadAsyncCallLock) {
    delete sPluginThreadAsyncCallLock;

    sPluginThreadAsyncCallLock = nullptr;
  }
}

AsyncCallbackAutoLock::AsyncCallbackAutoLock()
{
  if (sPluginThreadAsyncCallLock) {
    sPluginThreadAsyncCallLock->Lock();
  }
}

AsyncCallbackAutoLock::~AsyncCallbackAutoLock()
{
  if (sPluginThreadAsyncCallLock) {
    sPluginThreadAsyncCallLock->Unlock();
  }
}


NPP NPPStack::sCurrentNPP = nullptr;

const char *
PeekException()
{
  return gNPPException;
}

void
PopException()
{
  NS_ASSERTION(gNPPException, "Uh, no NPP exception to pop!");

  if (gNPPException) {
    free(gNPPException);

    gNPPException = nullptr;
  }
}

//
// Static callbacks that get routed back through the new C++ API
//

namespace mozilla {
namespace plugins {
namespace parent {

NPError
_geturl(NPP npp, const char* relativeURL, const char* target)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_geturl called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }

  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_GetURL: npp=%p, target=%s, url=%s\n", (void *)npp, target,
   relativeURL));

  PluginDestructionGuard guard(npp);

  // Block Adobe Acrobat from loading URLs that are not http:, https:,
  // or ftp: URLs if the given target is null.
  if (!target && relativeURL &&
      (strncmp(relativeURL, "http:", 5) != 0) &&
      (strncmp(relativeURL, "https:", 6) != 0) &&
      (strncmp(relativeURL, "ftp:", 4) != 0)) {
    nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *) npp->ndata;


    const char *name = nullptr;
    nsRefPtr<nsPluginHost> host = nsPluginHost::GetInst();
    host->GetPluginName(inst, &name);

    if (name && strstr(name, "Adobe") && strstr(name, "Acrobat")) {
      return NPERR_NO_ERROR;
    }
  }

  return MakeNewNPAPIStreamInternal(npp, relativeURL, target,
                                    eNPPStreamTypeInternal_Get);
}

NPError
_geturlnotify(NPP npp, const char* relativeURL, const char* target,
              void* notifyData)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_geturlnotify called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }

  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPN_GetURLNotify: npp=%p, target=%s, notify=%p, url=%s\n", (void*)npp,
     target, notifyData, relativeURL));

  PluginDestructionGuard guard(npp);

  return MakeNewNPAPIStreamInternal(npp, relativeURL, target,
                                    eNPPStreamTypeInternal_Get, true,
                                    notifyData);
}

NPError
_posturlnotify(NPP npp, const char *relativeURL, const char *target,
               uint32_t len, const char *buf, NPBool file, void *notifyData)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_posturlnotify called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }
  if (!buf)
    return NPERR_INVALID_PARAM;

  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPN_PostURLNotify: npp=%p, target=%s, len=%d, file=%d, "
                  "notify=%p, url=%s, buf=%s\n",
                  (void*)npp, target, len, file, notifyData, relativeURL,
                  buf));

  PluginDestructionGuard guard(npp);

  return MakeNewNPAPIStreamInternal(npp, relativeURL, target,
                                    eNPPStreamTypeInternal_Post, true,
                                    notifyData, len, buf, file);
}

NPError
_posturl(NPP npp, const char *relativeURL, const char *target,
         uint32_t len, const char *buf, NPBool file)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_posturl called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPN_PostURL: npp=%p, target=%s, file=%d, len=%d, url=%s, "
                  "buf=%s\n",
                  (void*)npp, target, file, len, relativeURL, buf));

  PluginDestructionGuard guard(npp);

  return MakeNewNPAPIStreamInternal(npp, relativeURL, target,
                                    eNPPStreamTypeInternal_Post, false, nullptr,
                                    len, buf, file);
}

NPError
_newstream(NPP npp, NPMIMEType type, const char* target, NPStream* *result)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_newstream called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_NewStream: npp=%p, type=%s, target=%s\n", (void*)npp,
   (const char *)type, target));

  NPError err = NPERR_INVALID_INSTANCE_ERROR;
  if (npp && npp->ndata) {
    nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance*)npp->ndata;

    PluginDestructionGuard guard(inst);

    nsCOMPtr<nsIOutputStream> stream;
    if (NS_SUCCEEDED(inst->NewStreamFromPlugin((const char*) type, target,
                                               getter_AddRefs(stream)))) {
      nsNPAPIStreamWrapper* wrapper = new nsNPAPIStreamWrapper(stream, nullptr);
      if (wrapper) {
        (*result) = &wrapper->mNPStream;
        err = NPERR_NO_ERROR;
      } else {
        err = NPERR_OUT_OF_MEMORY_ERROR;
      }
    } else {
      err = NPERR_GENERIC_ERROR;
    }
  }
  return err;
}

int32_t
_write(NPP npp, NPStream *pstream, int32_t len, void *buffer)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_write called from the wrong thread\n"));
    return 0;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPN_Write: npp=%p, url=%s, len=%d, buffer=%s\n", (void*)npp,
                  pstream->url, len, (char*)buffer));

  // negative return indicates failure to the plugin
  if (!npp)
    return -1;

  PluginDestructionGuard guard(npp);

  nsNPAPIStreamWrapper* wrapper = static_cast<nsNPAPIStreamWrapper*>(pstream->ndata);
  if (!wrapper) {
    return -1;
  }

  nsIOutputStream* stream = wrapper->GetOutputStream();
  if (!stream) {
    return -1;
  }

  uint32_t count = 0;
  nsresult rv = stream->Write((char *)buffer, len, &count);

  if (NS_FAILED(rv)) {
    return -1;
  }

  return (int32_t)count;
}

NPError
_destroystream(NPP npp, NPStream *pstream, NPError reason)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_write called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPN_DestroyStream: npp=%p, url=%s, reason=%d\n", (void*)npp,
                  pstream->url, (int)reason));

  if (!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginDestructionGuard guard(npp);

  nsNPAPIStreamWrapper *streamWrapper = static_cast<nsNPAPIStreamWrapper*>(pstream->ndata);
  if (!streamWrapper) {
    return NPERR_INVALID_PARAM;
  }

  nsNPAPIPluginStreamListener *listener = streamWrapper->GetStreamListener();
  if (listener) {
    // This type of stream is going from the browser to the plugin. It's either the
    // initial src/data stream or another stream resulting from NPN_GetURL* or
    // NPN_PostURL*.
    //
    // Calling OnStopBinding on the listener may cause it to be deleted due to the
    // releasing of its last references.
    listener->OnStopBinding(nullptr, NS_BINDING_ABORTED);
  } else {
    // This type of stream (NPStream) was created via NPN_NewStream. The plugin holds
    // the reference until it is to be deleted here. Deleting the wrapper will
    // release the wrapped nsIOutputStream.
    //
    // The NPStream the plugin references should always be a sub-object of its own
    // 'ndata', which is our nsNPAPIStramWrapper. See bug 548441.
    NS_ASSERTION((char*)streamWrapper <= (char*)pstream &&
                 ((char*)pstream) + sizeof(*pstream)
                     <= ((char*)streamWrapper) + sizeof(*streamWrapper),
                 "pstream is not a subobject of wrapper");
    delete streamWrapper;
  }

  // 'listener' and/or 'streamWrapper' may be invalid (deleted) at this point. Don't
  // touch them again!

  return NPERR_NO_ERROR;
}

void
_status(NPP npp, const char *message)
{
  // NPN_Status is no longer supported.
}

void
_memfree (void *ptr)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_memfree called from the wrong thread\n"));
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY, ("NPN_MemFree: ptr=%p\n", ptr));

  if (ptr)
    free(ptr);
}

uint32_t
_memflush(uint32_t size)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_memflush called from the wrong thread\n"));
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY, ("NPN_MemFlush: size=%d\n", size));

  nsMemory::HeapMinimize(true);
  return 0;
}

void
_reloadplugins(NPBool reloadPages)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_reloadplugins called from the wrong thread\n"));
    return;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPN_ReloadPlugins: reloadPages=%d\n", reloadPages));

  nsCOMPtr<nsIPluginHost> pluginHost(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID));
  if (!pluginHost)
    return;

  pluginHost->ReloadPlugins();
}

void
_invalidaterect(NPP npp, NPRect *invalidRect)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_invalidaterect called from the wrong thread\n"));
    return;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPN_InvalidateRect: npp=%p, top=%d, left=%d, bottom=%d, "
                  "right=%d\n", (void *)npp, invalidRect->top,
                  invalidRect->left, invalidRect->bottom, invalidRect->right));

  if (!npp || !npp->ndata) {
    NS_WARNING("_invalidaterect: npp or npp->ndata == 0");
    return;
  }

  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance*)npp->ndata;

  PluginDestructionGuard guard(inst);

  inst->InvalidateRect((NPRect *)invalidRect);
}

void
_invalidateregion(NPP npp, NPRegion invalidRegion)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_invalidateregion called from the wrong thread\n"));
    return;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                 ("NPN_InvalidateRegion: npp=%p, region=%p\n", (void*)npp,
                  (void*)invalidRegion));

  if (!npp || !npp->ndata) {
    NS_WARNING("_invalidateregion: npp or npp->ndata == 0");
    return;
  }

  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance*)npp->ndata;

  PluginDestructionGuard guard(inst);

  inst->InvalidateRegion((NPRegion)invalidRegion);
}

void
_forceredraw(NPP npp)
{
}

NPObject*
_getwindowobject(NPP npp)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_getwindowobject called from the wrong thread\n"));
    return nullptr;
  }

  // The window want to return here is the outer window, *not* the inner (since
  // we don't know what the plugin will do with it).
  nsIDocument* doc = GetDocumentFromNPP(npp);
  NS_ENSURE_TRUE(doc, nullptr);
  nsCOMPtr<nsPIDOMWindow> outer = do_QueryInterface(doc->GetWindow());
  NS_ENSURE_TRUE(outer, nullptr);

  AutoJSContext cx;
  JS::Rooted<JSObject*> global(cx, static_cast<nsGlobalWindow*>(outer.get())->GetGlobalJSObject());
  return nsJSObjWrapper::GetNewOrUsed(npp, cx, global);
}

NPObject*
_getpluginelement(NPP npp)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_getpluginelement called from the wrong thread\n"));
    return nullptr;
  }

  nsNPAPIPluginInstance* inst = static_cast<nsNPAPIPluginInstance*>(npp->ndata);
  if (!inst)
    return nullptr;

  nsCOMPtr<nsIDOMElement> element;
  inst->GetDOMElement(getter_AddRefs(element));

  if (!element)
    return nullptr;

  nsIDocument *doc = GetDocumentFromNPP(npp);
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }

  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(doc->GetInnerWindow()))) {
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
  NS_ENSURE_TRUE(xpc, nullptr);

  JS::RootedObject obj(cx);
  xpc->WrapNative(cx, ::JS::CurrentGlobalOrNull(cx), element,
                  NS_GET_IID(nsIDOMElement), obj.address());
  NS_ENSURE_TRUE(obj, nullptr);

  return nsJSObjWrapper::GetNewOrUsed(npp, cx, obj);
}

NPIdentifier
_getstringidentifier(const NPUTF8* name)
{
  if (!name) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS, ("NPN_getstringidentifier: passed null name"));
    return nullptr;
  }
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_getstringidentifier called from the wrong thread\n"));
  }

  AutoSafeJSContext cx;
  return doGetIdentifier(cx, name);
}

void
_getstringidentifiers(const NPUTF8** names, int32_t nameCount,
                      NPIdentifier *identifiers)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_getstringidentifiers called from the wrong thread\n"));
  }

  AutoSafeJSContext cx;

  for (int32_t i = 0; i < nameCount; ++i) {
    if (names[i]) {
      identifiers[i] = doGetIdentifier(cx, names[i]);
    } else {
      NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS, ("NPN_getstringidentifiers: passed null name"));
      identifiers[i] = nullptr;
    }
  }
}

NPIdentifier
_getintidentifier(int32_t intid)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_getstringidentifier called from the wrong thread\n"));
  }
  return IntToNPIdentifier(intid);
}

NPUTF8*
_utf8fromidentifier(NPIdentifier id)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_utf8fromidentifier called from the wrong thread\n"));
  }
  if (!id)
    return nullptr;

  if (!NPIdentifierIsString(id)) {
    return nullptr;
  }

  JSString *str = NPIdentifierToString(id);
  nsAutoString autoStr;
  AssignJSFlatString(autoStr, JS_ASSERT_STRING_IS_FLAT(str));

  return ToNewUTF8String(autoStr);
}

int32_t
_intfromidentifier(NPIdentifier id)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_intfromidentifier called from the wrong thread\n"));
  }

  if (!NPIdentifierIsInt(id)) {
    return INT32_MIN;
  }

  return NPIdentifierToInt(id);
}

bool
_identifierisstring(NPIdentifier id)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_identifierisstring called from the wrong thread\n"));
  }

  return NPIdentifierIsString(id);
}

NPObject*
_createobject(NPP npp, NPClass* aClass)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_createobject called from the wrong thread\n"));
    return nullptr;
  }
  if (!npp) {
    NS_ERROR("Null npp passed to _createobject()!");

    return nullptr;
  }

  PluginDestructionGuard guard(npp);

  if (!aClass) {
    NS_ERROR("Null class passed to _createobject()!");

    return nullptr;
  }

  NPPAutoPusher nppPusher(npp);

  NPObject *npobj;

  if (aClass->allocate) {
    npobj = aClass->allocate(npp, aClass);
  } else {
    npobj = (NPObject *)PR_Malloc(sizeof(NPObject));
  }

  if (npobj) {
    npobj->_class = aClass;
    npobj->referenceCount = 1;
    NS_LOG_ADDREF(npobj, 1, "BrowserNPObject", sizeof(NPObject));
  }

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("Created NPObject %p, NPClass %p\n", npobj, aClass));

  return npobj;
}

NPObject*
_retainobject(NPObject* npobj)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_retainobject called from the wrong thread\n"));
  }
  if (npobj) {
#ifdef NS_BUILD_REFCNT_LOGGING
    int32_t refCnt =
#endif
      PR_ATOMIC_INCREMENT((int32_t*)&npobj->referenceCount);
    NS_LOG_ADDREF(npobj, refCnt, "BrowserNPObject", sizeof(NPObject));
  }

  return npobj;
}

void
_releaseobject(NPObject* npobj)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_releaseobject called from the wrong thread\n"));
  }
  if (!npobj)
    return;

  int32_t refCnt = PR_ATOMIC_DECREMENT((int32_t*)&npobj->referenceCount);
  NS_LOG_RELEASE(npobj, refCnt, "BrowserNPObject");

  if (refCnt == 0) {
    nsNPObjWrapper::OnDestroy(npobj);

    NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                   ("Deleting NPObject %p, refcount hit 0\n", npobj));

    if (npobj->_class && npobj->_class->deallocate) {
      npobj->_class->deallocate(npobj);
    } else {
      PR_Free(npobj);
    }
  }
}

bool
_invoke(NPP npp, NPObject* npobj, NPIdentifier method, const NPVariant *args,
        uint32_t argCount, NPVariant *result)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_invoke called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class || !npobj->_class->invoke)
    return false;

  PluginDestructionGuard guard(npp);

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_Invoke(npp %p, npobj %p, method %p, args %d\n", npp,
                  npobj, method, argCount));

  return npobj->_class->invoke(npobj, method, args, argCount, result);
}

bool
_invokeDefault(NPP npp, NPObject* npobj, const NPVariant *args,
               uint32_t argCount, NPVariant *result)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_invokedefault called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class || !npobj->_class->invokeDefault)
    return false;

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_InvokeDefault(npp %p, npobj %p, args %d\n", npp,
                  npobj, argCount));

  return npobj->_class->invokeDefault(npobj, args, argCount, result);
}

bool
_evaluate(NPP npp, NPObject* npobj, NPString *script, NPVariant *result)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_evaluate called from the wrong thread\n"));
    return false;
  }
  if (!npp)
    return false;

  NPPAutoPusher nppPusher(npp);

  nsIDocument *doc = GetDocumentFromNPP(npp);
  NS_ENSURE_TRUE(doc, false);

  nsGlobalWindow* win = static_cast<nsGlobalWindow*>(doc->GetInnerWindow());
  if (NS_WARN_IF(!win || !win->FastGetGlobalJSObject())) {
    return false;
  }

  nsAutoMicroTask mt;
  dom::AutoEntryScript aes(win, "NPAPI NPN_evaluate");
  aes.TakeOwnershipOfErrorReporting();
  JSContext* cx = aes.cx();

  JS::Rooted<JSObject*> obj(cx, nsNPObjWrapper::GetNewOrUsed(npp, cx, npobj));

  if (!obj) {
    return false;
  }

  obj = JS_ObjectToInnerObject(cx, obj);
  MOZ_ASSERT(obj,
             "JS_ObjectToInnerObject should never return null with non-null "
             "input.");

  if (result) {
    // Initialize the out param to void
    VOID_TO_NPVARIANT(*result);
  }

  if (!script || !script->UTF8Length || !script->UTF8Characters) {
    // Nothing to evaluate.

    return true;
  }

  NS_ConvertUTF8toUTF16 utf16script(script->UTF8Characters,
                                    script->UTF8Length);

  nsIPrincipal *principal = doc->NodePrincipal();

  nsAutoCString specStr;
  const char *spec;

  nsCOMPtr<nsIURI> uri;
  principal->GetURI(getter_AddRefs(uri));

  if (uri) {
    uri->GetSpec(specStr);
    spec = specStr.get();
  } else {
    // No URI in a principal means it's the system principal. If the
    // document URI is a chrome:// URI, pass that in as the URI of the
    // script, else pass in null for the filename as there's no way to
    // know where this document really came from. Passing in null here
    // also means that the script gets treated by XPConnect as if it
    // needs additional protection, which is what we want for unknown
    // chrome code anyways.

    uri = doc->GetDocumentURI();
    bool isChrome = false;

    if (uri && NS_SUCCEEDED(uri->SchemeIs("chrome", &isChrome)) && isChrome) {
      uri->GetSpec(specStr);
      spec = specStr.get();
    } else {
      spec = nullptr;
    }
  }

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_Evaluate(npp %p, npobj %p, script <<<%s>>>) called\n",
                  npp, npobj, script->UTF8Characters));

  JS::CompileOptions options(cx);
  options.setFileAndLine(spec, 0)
         .setVersion(JSVERSION_DEFAULT);
  JS::Rooted<JS::Value> rval(cx);
  nsJSUtils::EvaluateOptions evalOptions(cx);
  if (obj != js::GetGlobalForObjectCrossCompartment(obj) &&
      !evalOptions.scopeChain.append(obj)) {
    return false;
  }
  obj = js::GetGlobalForObjectCrossCompartment(obj);
  nsresult rv = nsJSUtils::EvaluateString(cx, utf16script, obj, options,
                                          evalOptions, &rval);

  return NS_SUCCEEDED(rv) &&
         (!result || JSValToNPVariant(npp, cx, rval, result));
}

bool
_getproperty(NPP npp, NPObject* npobj, NPIdentifier property,
             NPVariant *result)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_getproperty called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class || !npobj->_class->getProperty)
    return false;

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_GetProperty(npp %p, npobj %p, property %p) called\n",
                  npp, npobj, property));

  if (!npobj->_class->getProperty(npobj, property, result))
    return false;

  // If a Java plugin tries to get the document.URL or document.documentURI
  // property from us, don't pass back a value that Java won't be able to
  // understand -- one that will make the URL(String) constructor throw a
  // MalformedURL exception.  Passing such a value causes Java Plugin2 to
  // crash (to throw a RuntimeException in Plugin2Manager.getDocumentBase()).
  // Also don't pass back a value that Java is likely to mishandle.

  nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*) npp->ndata;
  if (!inst)
    return false;
  nsNPAPIPlugin* plugin = inst->GetPlugin();
  if (!plugin)
    return false;
  nsRefPtr<nsPluginHost> host = nsPluginHost::GetInst();
  nsPluginTag* pluginTag = host->TagForPlugin(plugin);
  if (!pluginTag->mIsJavaPlugin)
    return true;

  if (!NPVARIANT_IS_STRING(*result))
    return true;

  NPUTF8* propertyName = _utf8fromidentifier(property);
  if (!propertyName)
    return true;
  bool notURL =
    (PL_strcasecmp(propertyName, "URL") &&
     PL_strcasecmp(propertyName, "documentURI"));
  _memfree(propertyName);
  if (notURL)
    return true;

  NPObject* window_obj = _getwindowobject(npp);
  if (!window_obj)
    return true;

  NPVariant doc_v;
  NPObject* document_obj = nullptr;
  NPIdentifier doc_id = _getstringidentifier("document");
  bool ok = npobj->_class->getProperty(window_obj, doc_id, &doc_v);
  _releaseobject(window_obj);
  if (ok) {
    if (NPVARIANT_IS_OBJECT(doc_v)) {
      document_obj = NPVARIANT_TO_OBJECT(doc_v);
    } else {
      _releasevariantvalue(&doc_v);
      return true;
    }
  } else {
    return true;
  }
  _releaseobject(document_obj);
  if (document_obj != npobj)
    return true;

  NPString urlnp = NPVARIANT_TO_STRING(*result);
  nsXPIDLCString url;
  url.Assign(urlnp.UTF8Characters, urlnp.UTF8Length);

  bool javaCompatible = false;
  if (NS_FAILED(NS_CheckIsJavaCompatibleURLString(url, &javaCompatible)))
    javaCompatible = false;
  if (javaCompatible)
    return true;

  // If Java won't be able to interpret the original value of document.URL or
  // document.documentURI, or is likely to mishandle it, pass back something
  // that Java will understand but won't be able to use to access the network,
  // and for which same-origin checks will always fail.

  if (inst->mFakeURL.IsVoid()) {
    // Abort (do an error return) if NS_MakeRandomInvalidURLString() fails.
    if (NS_FAILED(NS_MakeRandomInvalidURLString(inst->mFakeURL))) {
      _releasevariantvalue(result);
      return false;
    }
  }

  _releasevariantvalue(result);
  char* fakeurl = (char *) _memalloc(inst->mFakeURL.Length() + 1);
  strcpy(fakeurl, inst->mFakeURL);
  STRINGZ_TO_NPVARIANT(fakeurl, *result);

  return true;
}

bool
_setproperty(NPP npp, NPObject* npobj, NPIdentifier property,
             const NPVariant *value)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_setproperty called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class || !npobj->_class->setProperty)
    return false;

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_SetProperty(npp %p, npobj %p, property %p) called\n",
                  npp, npobj, property));

  return npobj->_class->setProperty(npobj, property, value);
}

bool
_removeproperty(NPP npp, NPObject* npobj, NPIdentifier property)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_removeproperty called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class || !npobj->_class->removeProperty)
    return false;

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_RemoveProperty(npp %p, npobj %p, property %p) called\n",
                  npp, npobj, property));

  return npobj->_class->removeProperty(npobj, property);
}

bool
_hasproperty(NPP npp, NPObject* npobj, NPIdentifier propertyName)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_hasproperty called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class || !npobj->_class->hasProperty)
    return false;

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_HasProperty(npp %p, npobj %p, property %p) called\n",
                  npp, npobj, propertyName));

  return npobj->_class->hasProperty(npobj, propertyName);
}

bool
_hasmethod(NPP npp, NPObject* npobj, NPIdentifier methodName)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_hasmethod called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class || !npobj->_class->hasMethod)
    return false;

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_HasMethod(npp %p, npobj %p, property %p) called\n",
                  npp, npobj, methodName));

  return npobj->_class->hasMethod(npobj, methodName);
}

bool
_enumerate(NPP npp, NPObject *npobj, NPIdentifier **identifier,
           uint32_t *count)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_enumerate called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class)
    return false;

  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                 ("NPN_Enumerate(npp %p, npobj %p) called\n", npp, npobj));

  if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(npobj->_class) ||
      !npobj->_class->enumerate) {
    *identifier = 0;
    *count = 0;
    return true;
  }

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  return npobj->_class->enumerate(npobj, identifier, count);
}

bool
_construct(NPP npp, NPObject* npobj, const NPVariant *args,
               uint32_t argCount, NPVariant *result)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_construct called from the wrong thread\n"));
    return false;
  }
  if (!npp || !npobj || !npobj->_class ||
      !NP_CLASS_STRUCT_VERSION_HAS_CTOR(npobj->_class) ||
      !npobj->_class->construct) {
    return false;
  }

  NPPExceptionAutoHolder nppExceptionHolder;
  NPPAutoPusher nppPusher(npp);

  return npobj->_class->construct(npobj, args, argCount, result);
}

void
_releasevariantvalue(NPVariant* variant)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_releasevariantvalue called from the wrong thread\n"));
  }
  switch (variant->type) {
  case NPVariantType_Void :
  case NPVariantType_Null :
  case NPVariantType_Bool :
  case NPVariantType_Int32 :
  case NPVariantType_Double :
    break;
  case NPVariantType_String :
    {
      const NPString *s = &NPVARIANT_TO_STRING(*variant);

      if (s->UTF8Characters) {
#if defined(MOZ_MEMORY_WINDOWS)
        if (malloc_usable_size((void *)s->UTF8Characters) != 0) {
          PR_Free((void *)s->UTF8Characters);
        } else {
          void *p = (void *)s->UTF8Characters;
          DWORD nheaps = 0;
          nsAutoTArray<HANDLE, 50> heaps;
          nheaps = GetProcessHeaps(0, heaps.Elements());
          heaps.AppendElements(nheaps);
          GetProcessHeaps(nheaps, heaps.Elements());
          for (DWORD i = 0; i < nheaps; i++) {
            if (InHeap(heaps[i], p)) {
              HeapFree(heaps[i], 0, p);
              break;
            }
          }
        }
#else
        free((void *)s->UTF8Characters);
#endif
      }
      break;
    }
  case NPVariantType_Object:
    {
      NPObject *npobj = NPVARIANT_TO_OBJECT(*variant);

      if (npobj)
        _releaseobject(npobj);

      break;
    }
  default:
    NS_ERROR("Unknown NPVariant type!");
  }

  VOID_TO_NPVARIANT(*variant);
}

void
_setexception(NPObject* npobj, const NPUTF8 *message)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_setexception called from the wrong thread\n"));
    return;
  }

  if (!message) return;

  if (gNPPException) {
    // If a plugin throws multiple exceptions, we'll only report the
    // last one for now.
    free(gNPPException);
  }

  gNPPException = strdup(message);
}

NPError
_getvalue(NPP npp, NPNVariable variable, void *result)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_getvalue called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_GetValue: npp=%p, var=%d\n",
                                     (void*)npp, (int)variable));

  nsresult res;

  PluginDestructionGuard guard(npp);

  // Cast NPNVariable enum to int to avoid warnings about including switch
  // cases for android_npapi.h's non-standard ANPInterface values.
  switch (static_cast<int>(variable)) {

#if defined(XP_UNIX) && !defined(XP_MACOSX)
  case NPNVxDisplay : {
#if defined(MOZ_X11)
    if (npp) {
      nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *) npp->ndata;
      bool windowless = false;
      inst->IsWindowless(&windowless);
      // The documentation on the types for many variables in NP(N|P)_GetValue
      // is vague.  Often boolean values are NPBool (1 byte), but
      // https://developer.mozilla.org/en/XEmbed_Extension_for_Mozilla_Plugins
      // treats NPPVpluginNeedsXEmbed as PRBool (int), and
      // on x86/32-bit, flash stores to this using |movl 0x1,&needsXEmbed|.
      // thus we can't use NPBool for needsXEmbed, or the three bytes above
      // it on the stack would get clobbered. so protect with the larger bool.
      int needsXEmbed = 0;
      if (!windowless) {
        res = inst->GetValueFromPlugin(NPPVpluginNeedsXEmbed, &needsXEmbed);
        // If the call returned an error code make sure we still use our default value.
        if (NS_FAILED(res)) {
          needsXEmbed = 0;
        }
      }
      if (windowless || needsXEmbed) {
        (*(Display **)result) = mozilla::DefaultXDisplay();
        return NPERR_NO_ERROR;
      }
    }
#if (MOZ_WIDGET_GTK == 2)
    // adobe nppdf calls XtGetApplicationNameAndClass(display,
    // &instance, &class) we have to init Xt toolkit before get
    // XtDisplay just call gtk_xtbin_new(w,0) once
    static GtkWidget *gtkXtBinHolder = 0;
    if (!gtkXtBinHolder) {
      gtkXtBinHolder = gtk_xtbin_new(gdk_get_default_root_window(),0);
      // it crashes on destroy, let it leak
      // gtk_widget_destroy(gtkXtBinHolder);
    }
    (*(Display **)result) =  GTK_XTBIN(gtkXtBinHolder)->xtdisplay;
    return NPERR_NO_ERROR;
#endif
#endif
    return NPERR_GENERIC_ERROR;
  }

  case NPNVxtAppContext:
    return NPERR_GENERIC_ERROR;
#endif

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_QT)
  case NPNVnetscapeWindow: {
    if (!npp || !npp->ndata)
      return NPERR_INVALID_INSTANCE_ERROR;

    nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *) npp->ndata;

    nsRefPtr<nsPluginInstanceOwner> owner = inst->GetOwner();
    NS_ENSURE_TRUE(owner, NPERR_NO_ERROR);

    if (NS_SUCCEEDED(owner->GetNetscapeWindow(result))) {
      return NPERR_NO_ERROR;
    }
    return NPERR_GENERIC_ERROR;
  }
#endif

  case NPNVjavascriptEnabledBool: {
    *(NPBool*)result = false;
    bool js = false;
    res = Preferences::GetBool("javascript.enabled", &js);
    if (NS_SUCCEEDED(res)) {
      *(NPBool*)result = js;
    }
    return NPERR_NO_ERROR;
  }

  case NPNVasdEnabledBool:
    *(NPBool*)result = false;
    return NPERR_NO_ERROR;

  case NPNVisOfflineBool: {
    bool offline = false;
    nsCOMPtr<nsIIOService> ioservice =
      do_GetService(NS_IOSERVICE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res))
      res = ioservice->GetOffline(&offline);
    if (NS_FAILED(res))
      return NPERR_GENERIC_ERROR;

    *(NPBool*)result = offline;
    return NPERR_NO_ERROR;
  }

  case NPNVToolkit: {
#ifdef MOZ_WIDGET_GTK
    *((NPNToolkitType*)result) = NPNVGtk2;
#endif

#ifdef MOZ_WIDGET_QT
    /* Fake toolkit so flash plugin works */
    *((NPNToolkitType*)result) = NPNVGtk2;
#endif
    if (*(NPNToolkitType*)result)
        return NPERR_NO_ERROR;

    return NPERR_GENERIC_ERROR;
  }

  case NPNVSupportsXEmbedBool: {
#ifdef MOZ_WIDGET_GTK
    *(NPBool*)result = true;
#elif defined(MOZ_WIDGET_QT)
    // Desktop Flash fail to initialize if browser does not support NPNVSupportsXEmbedBool
    // even when wmode!=windowed, lets return fake support
    fprintf(stderr, "Fake support for XEmbed plugins in Qt port\n");
    *(NPBool*)result = true;
#else
    *(NPBool*)result = false;
#endif
    return NPERR_NO_ERROR;
  }

  case NPNVWindowNPObject: {
    *(NPObject **)result = _getwindowobject(npp);

    return *(NPObject **)result ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
  }

  case NPNVPluginElementNPObject: {
    *(NPObject **)result = _getpluginelement(npp);

    return *(NPObject **)result ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
  }

  case NPNVSupportsWindowless: {
#if defined(XP_WIN) || defined(XP_MACOSX) || \
    (defined(MOZ_X11) && (defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_QT)))
    *(NPBool*)result = true;
#else
    *(NPBool*)result = false;
#endif
    return NPERR_NO_ERROR;
  }

  case NPNVprivateModeBool: {
    bool privacy;
    nsNPAPIPluginInstance *inst = static_cast<nsNPAPIPluginInstance*>(npp->ndata);
    if (!inst)
      return NPERR_GENERIC_ERROR;

    nsresult rv = inst->IsPrivateBrowsing(&privacy);
    if (NS_FAILED(rv))
      return NPERR_GENERIC_ERROR;
    *(NPBool*)result = (NPBool)privacy;
    return NPERR_NO_ERROR;
  }

  case NPNVdocumentOrigin: {
    nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)npp->ndata;
    if (!inst) {
      return NPERR_GENERIC_ERROR;
    }

    nsCOMPtr<nsIDOMElement> element;
    inst->GetDOMElement(getter_AddRefs(element));
    if (!element) {
      return NPERR_GENERIC_ERROR;
    }

    nsCOMPtr<nsIContent> content(do_QueryInterface(element));
    if (!content) {
      return NPERR_GENERIC_ERROR;
    }

    nsIPrincipal* principal = content->NodePrincipal();

    nsAutoString utf16Origin;
    res = nsContentUtils::GetUTFOrigin(principal, utf16Origin);
    if (NS_FAILED(res)) {
      return NPERR_GENERIC_ERROR;
    }

    nsCOMPtr<nsIUnicodeNormalizer> normalizer = do_GetService(NS_UNICODE_NORMALIZER_CONTRACTID);
    if (!normalizer) {
      return NPERR_GENERIC_ERROR;
    }

    nsAutoString normalizedUTF16Origin;
    res = normalizer->NormalizeUnicodeNFKC(utf16Origin, normalizedUTF16Origin);
    if (NS_FAILED(res)) {
      return NPERR_GENERIC_ERROR;
    }

    *(char**)result = ToNewUTF8String(normalizedUTF16Origin);
    return *(char**)result ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
  }

#ifdef XP_MACOSX
  case NPNVpluginDrawingModel: {
    if (npp) {
      nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance*)npp->ndata;
      if (inst) {
        NPDrawingModel drawingModel;
        inst->GetDrawingModel((int32_t*)&drawingModel);
        *(NPDrawingModel*)result = drawingModel;
        return NPERR_NO_ERROR;
      }
    }
    else {
      return NPERR_GENERIC_ERROR;
    }
  }

#ifndef NP_NO_QUICKDRAW
  case NPNVsupportsQuickDrawBool: {
    *(NPBool*)result = false;

    return NPERR_NO_ERROR;
  }
#endif

  case NPNVsupportsCoreGraphicsBool: {
    *(NPBool*)result = true;

    return NPERR_NO_ERROR;
  }

  case NPNVsupportsCoreAnimationBool: {
    *(NPBool*)result = true;

    return NPERR_NO_ERROR;
  }

  case NPNVsupportsInvalidatingCoreAnimationBool: {
    *(NPBool*)result = true;

    return NPERR_NO_ERROR;
  }

  case NPNVsupportsCompositingCoreAnimationPluginsBool: {
    *(NPBool*)result = PR_TRUE;

    return NPERR_NO_ERROR;
  }

#ifndef NP_NO_CARBON
  case NPNVsupportsCarbonBool: {
    *(NPBool*)result = false;

    return NPERR_NO_ERROR;
  }
#endif
  case NPNVsupportsCocoaBool: {
    *(NPBool*)result = true;

    return NPERR_NO_ERROR;
  }

  case NPNVsupportsUpdatedCocoaTextInputBool: {
    *(NPBool*)result = true;
    return NPERR_NO_ERROR;
  }

  case NPNVcontentsScaleFactor: {
    nsNPAPIPluginInstance *inst =
      (nsNPAPIPluginInstance *) (npp ? npp->ndata : nullptr);
    double scaleFactor = inst ? inst->GetContentsScaleFactor() : 1.0;
    *(double*)result = scaleFactor;
    return NPERR_NO_ERROR;
  }
#endif

#ifdef MOZ_WIDGET_ANDROID
    case kLogInterfaceV0_ANPGetValue: {
      LOG("get log interface");
      ANPLogInterfaceV0 *i = (ANPLogInterfaceV0 *) result;
      InitLogInterface(i);
      return NPERR_NO_ERROR;
    }

    case kBitmapInterfaceV0_ANPGetValue: {
      LOG("get bitmap interface");
      ANPBitmapInterfaceV0 *i = (ANPBitmapInterfaceV0 *) result;
      InitBitmapInterface(i);
      return NPERR_NO_ERROR;
    }

    case kMatrixInterfaceV0_ANPGetValue: {
      LOG("get matrix interface");
      ANPMatrixInterfaceV0 *i = (ANPMatrixInterfaceV0 *) result;
      InitMatrixInterface(i);
      return NPERR_NO_ERROR;
    }

    case kPathInterfaceV0_ANPGetValue: {
      LOG("get path interface");
      ANPPathInterfaceV0 *i = (ANPPathInterfaceV0 *) result;
      InitPathInterface(i);
      return NPERR_NO_ERROR;
    }

    case kTypefaceInterfaceV0_ANPGetValue: {
      LOG("get typeface interface");
      ANPTypefaceInterfaceV0 *i = (ANPTypefaceInterfaceV0 *) result;
      InitTypeFaceInterface(i);
      return NPERR_NO_ERROR;
    }

    case kPaintInterfaceV0_ANPGetValue: {
      LOG("get paint interface");
      ANPPaintInterfaceV0 *i = (ANPPaintInterfaceV0 *) result;
      InitPaintInterface(i);
      return NPERR_NO_ERROR;
    }

    case kCanvasInterfaceV0_ANPGetValue: {
      LOG("get canvas interface");
      ANPCanvasInterfaceV0 *i = (ANPCanvasInterfaceV0 *) result;
      InitCanvasInterface(i);
      return NPERR_NO_ERROR;
    }

    case kWindowInterfaceV0_ANPGetValue: {
      LOG("get window interface");
      ANPWindowInterfaceV0 *i = (ANPWindowInterfaceV0 *) result;
      InitWindowInterface(i);
      return NPERR_NO_ERROR;
    }

    case kAudioTrackInterfaceV0_ANPGetValue: {
      LOG("get audio interface");
      ANPAudioTrackInterfaceV0 *i = (ANPAudioTrackInterfaceV0 *) result;
      InitAudioTrackInterfaceV0(i);
      return NPERR_NO_ERROR;
    }

    case kEventInterfaceV0_ANPGetValue: {
      LOG("get event interface");
      ANPEventInterfaceV0 *i = (ANPEventInterfaceV0 *) result;
      InitEventInterface(i);
      return NPERR_NO_ERROR;
    }

    case kSystemInterfaceV0_ANPGetValue: {
      LOG("get system interface");
      ANPSystemInterfaceV0* i = reinterpret_cast<ANPSystemInterfaceV0*>(result);
      InitSystemInterface(i);
      return NPERR_NO_ERROR;
    }

    case kSurfaceInterfaceV0_ANPGetValue: {
      LOG("get surface interface");
      ANPSurfaceInterfaceV0 *i = (ANPSurfaceInterfaceV0 *) result;
      InitSurfaceInterface(i);
      return NPERR_NO_ERROR;
    }

    case kSupportedDrawingModel_ANPGetValue: {
      LOG("get supported drawing model");
      uint32_t* bits = reinterpret_cast<uint32_t*>(result);
      *bits = kBitmap_ANPDrawingModel && kSurface_ANPDrawingModel;
      return NPERR_NO_ERROR;
    }

    case kJavaContext_ANPGetValue: {
      auto ret = widget::GeckoAppShell::GetContext();
      if (!ret)
        return NPERR_GENERIC_ERROR;

      *static_cast<jobject*>(result) = ret.Forget();
      return NPERR_NO_ERROR;
    }

    case kAudioTrackInterfaceV1_ANPGetValue: {
      LOG("get audio interface v1");
      ANPAudioTrackInterfaceV1 *i = (ANPAudioTrackInterfaceV1 *) result;
      InitAudioTrackInterfaceV1(i);
      return NPERR_NO_ERROR;
    }

    case kNativeWindowInterfaceV0_ANPGetValue: {
      LOG("get native window interface v0");
      ANPNativeWindowInterfaceV0* i = (ANPNativeWindowInterfaceV0 *) result;
      InitNativeWindowInterface(i);
      return NPERR_NO_ERROR;
    }

    case kOpenGLInterfaceV0_ANPGetValue: {
      LOG("get openGL interface");
      ANPOpenGLInterfaceV0 *i = (ANPOpenGLInterfaceV0*) result;
      InitOpenGLInterface(i);
      return NPERR_NO_ERROR;
    }

    case kWindowInterfaceV1_ANPGetValue: {
      LOG("get Window interface V1");
      ANPWindowInterfaceV1 *i = (ANPWindowInterfaceV1 *) result;
      InitWindowInterfaceV1(i);
      return NPERR_NO_ERROR;
    }

    case kWindowInterfaceV2_ANPGetValue: {
      LOG("get Window interface V2");
      ANPWindowInterfaceV2 *i = (ANPWindowInterfaceV2 *) result;
      InitWindowInterfaceV2(i);
      return NPERR_NO_ERROR;
    }

    case kVideoInterfaceV0_ANPGetValue: {
      LOG("get video interface");
      ANPVideoInterfaceV0 *i = (ANPVideoInterfaceV0*) result;
      InitVideoInterfaceV0(i);
      return NPERR_NO_ERROR;
    }

    case kVideoInterfaceV1_ANPGetValue: {
      LOG("get video interface");
      ANPVideoInterfaceV1 *i = (ANPVideoInterfaceV1*) result;
      InitVideoInterfaceV1(i);
      return NPERR_NO_ERROR;
    }

    case kSystemInterfaceV1_ANPGetValue: {
      LOG("get system interface v1");
      ANPSystemInterfaceV1* i = reinterpret_cast<ANPSystemInterfaceV1*>(result);
      InitSystemInterfaceV1(i);
      return NPERR_NO_ERROR;
    }

    case kSystemInterfaceV2_ANPGetValue: {
      LOG("get system interface v2");
      ANPSystemInterfaceV2* i = reinterpret_cast<ANPSystemInterfaceV2*>(result);
      InitSystemInterfaceV2(i);
      return NPERR_NO_ERROR;
    }
#endif

  // we no longer hand out any XPCOM objects
  case NPNVDOMElement:
    // fall through
  case NPNVDOMWindow:
    // fall through
  case NPNVserviceManager:
    // old XPCOM objects, no longer supported, but null out the out
    // param to avoid crashing plugins that still try to use this.
    *(nsISupports**)result = nullptr;
    // fall through
  default:
    NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_getvalue unhandled get value: %d\n", variable));
    return NPERR_GENERIC_ERROR;
  }
}

NPError
_setvalue(NPP npp, NPPVariable variable, void *result)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_setvalue called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_SetValue: npp=%p, var=%d\n",
                                     (void*)npp, (int)variable));

  if (!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *) npp->ndata;

  NS_ASSERTION(inst, "null instance");

  if (!inst)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginDestructionGuard guard(inst);

  // Cast NPNVariable enum to int to avoid warnings about including switch
  // cases for android_npapi.h's non-standard ANPInterface values.
  switch (static_cast<int>(variable)) {

    // we should keep backward compatibility with NPAPI where the
    // actual pointer value is checked rather than its content
    // when passing booleans
    case NPPVpluginWindowBool: {
#ifdef XP_MACOSX
      // This setting doesn't apply to OS X (only to Windows and Unix/Linux).
      // See https://developer.mozilla.org/En/NPN_SetValue#section_5.  Return
      // NPERR_NO_ERROR here to conform to other browsers' behavior on OS X
      // (e.g. Safari and Opera).
      return NPERR_NO_ERROR;
#else
      NPBool bWindowless = (result == nullptr);
      return inst->SetWindowless(bWindowless);
#endif
    }
    case NPPVpluginTransparentBool: {
      NPBool bTransparent = (result != nullptr);
      return inst->SetTransparent(bTransparent);
    }

    case NPPVjavascriptPushCallerBool: {
      return NPERR_NO_ERROR;
    }

    case NPPVpluginKeepLibraryInMemory: {
      NPBool bCached = (result != nullptr);
      inst->SetCached(bCached);
      return NPERR_NO_ERROR;
    }

    case NPPVpluginUsesDOMForCursorBool: {
      bool useDOMForCursor = (result != nullptr);
      return inst->SetUsesDOMForCursor(useDOMForCursor);
    }

    case NPPVpluginIsPlayingAudio: {
      bool isMuted = !result;

      nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*) npp->ndata;
      MOZ_ASSERT(inst);

      if (isMuted && !inst->HasAudioChannelAgent()) {
        return NPERR_NO_ERROR;
      }

      nsCOMPtr<nsIAudioChannelAgent> agent;
      nsresult rv = inst->GetOrCreateAudioChannelAgent(getter_AddRefs(agent));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NPERR_NO_ERROR;
      }

      MOZ_ASSERT(agent);

      if (isMuted) {
        rv = agent->NotifyStoppedPlaying();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return NPERR_NO_ERROR;
        }
      } else {
        float volume = 0.0;
        bool muted = true;
        rv = agent->NotifyStartedPlaying(&volume, &muted);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return NPERR_NO_ERROR;
        }

        rv = inst->WindowVolumeChanged(volume, muted);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return NPERR_NO_ERROR;
        }
      }

      return NPERR_NO_ERROR;
    }

#ifndef MOZ_WIDGET_ANDROID
    // On android, their 'drawing model' uses the same constant!
    case NPPVpluginDrawingModel: {
      if (inst) {
        inst->SetDrawingModel((NPDrawingModel)NS_PTR_TO_INT32(result));
        return NPERR_NO_ERROR;
      }
      else {
        return NPERR_GENERIC_ERROR;
      }
    }
#endif

#ifdef XP_MACOSX
    case NPPVpluginEventModel: {
      if (inst) {
        inst->SetEventModel((NPEventModel)NS_PTR_TO_INT32(result));
        return NPERR_NO_ERROR;
      }
      else {
        return NPERR_GENERIC_ERROR;
      }
    }
#endif
#ifdef MOZ_WIDGET_ANDROID
  case kRequestDrawingModel_ANPSetValue:
    if (inst)
      inst->SetANPDrawingModel(NS_PTR_TO_INT32(result));
    return NPERR_NO_ERROR;
  case kAcceptEvents_ANPSetValue:
    return NPERR_NO_ERROR;
#endif
    default:
      return NPERR_GENERIC_ERROR;
  }
}

NPError
_requestread(NPStream *pstream, NPByteRange *rangeList)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_requestread called from the wrong thread\n"));
    return NPERR_INVALID_PARAM;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_RequestRead: stream=%p\n",
                                     (void*)pstream));

#ifdef PLUGIN_LOGGING
  for(NPByteRange * range = rangeList; range != nullptr; range = range->next)
    MOZ_LOG(nsPluginLogging::gNPNLog,PLUGIN_LOG_NOISY,
    ("%i-%i", range->offset, range->offset + range->length - 1));

  MOZ_LOG(nsPluginLogging::gNPNLog,PLUGIN_LOG_NOISY, ("\n\n"));
  PR_LogFlush();
#endif

  if (!pstream || !rangeList || !pstream->ndata)
    return NPERR_INVALID_PARAM;

  nsNPAPIStreamWrapper* streamWrapper = static_cast<nsNPAPIStreamWrapper*>(pstream->ndata);
  nsNPAPIPluginStreamListener* streamlistener = streamWrapper->GetStreamListener();
  if (!streamlistener) {
    return NPERR_GENERIC_ERROR;
  }

  int32_t streamtype = NP_NORMAL;

  streamlistener->GetStreamType(&streamtype);

  if (streamtype != NP_SEEK)
    return NPERR_STREAM_NOT_SEEKABLE;

  if (!streamlistener->mStreamListenerPeer)
    return NPERR_GENERIC_ERROR;

  nsresult rv = streamlistener->mStreamListenerPeer->RequestRead((NPByteRange *)rangeList);
  if (NS_FAILED(rv))
    return NPERR_GENERIC_ERROR;

  return NPERR_NO_ERROR;
}

// Deprecated, only stubbed out
void* /* OJI type: JRIEnv* */
_getJavaEnv()
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_GetJavaEnv\n"));
  return nullptr;
}

const char *
_useragent(NPP npp)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_useragent called from the wrong thread\n"));
    return nullptr;
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_UserAgent: npp=%p\n", (void*)npp));

  nsCOMPtr<nsIPluginHost> pluginHostCOM(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID));
  nsPluginHost *pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if (!pluginHost) {
    return nullptr;
  }

  const char *retstr;
  nsresult rv = pluginHost->UserAgent(&retstr);
  if (NS_FAILED(rv))
    return nullptr;

  return retstr;
}

void *
_memalloc (uint32_t size)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,("NPN_memalloc called from the wrong thread\n"));
  }
  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY, ("NPN_MemAlloc: size=%d\n", size));
  return moz_xmalloc(size);
}

// Deprecated, only stubbed out
void* /* OJI type: jref */
_getJavaPeer(NPP npp)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_GetJavaPeer: npp=%p\n", (void*)npp));
  return nullptr;
}

void
_pushpopupsenabledstate(NPP npp, NPBool enabled)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_pushpopupsenabledstate called from the wrong thread\n"));
    return;
  }
  nsNPAPIPluginInstance *inst = npp ? (nsNPAPIPluginInstance *)npp->ndata : nullptr;
  if (!inst)
    return;

  inst->PushPopupsEnabledState(enabled);
}

void
_poppopupsenabledstate(NPP npp)
{
  if (!NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("NPN_poppopupsenabledstate called from the wrong thread\n"));
    return;
  }
  nsNPAPIPluginInstance *inst = npp ? (nsNPAPIPluginInstance *)npp->ndata : nullptr;
  if (!inst)
    return;

  inst->PopPopupsEnabledState();
}

void
_pluginthreadasynccall(NPP instance, PluginThreadCallback func, void *userData)
{
  if (NS_IsMainThread()) {
    NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,("NPN_pluginthreadasynccall called from the main thread\n"));
  } else {
    NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY,("NPN_pluginthreadasynccall called from a non main thread\n"));
  }
  nsRefPtr<nsPluginThreadRunnable> evt =
    new nsPluginThreadRunnable(instance, func, userData);

  if (evt && evt->IsValid()) {
    NS_DispatchToMainThread(evt);
  }
}

NPError
_getvalueforurl(NPP instance, NPNURLVariable variable, const char *url,
                char **value, uint32_t *len)
{
  if (!instance) {
    return NPERR_INVALID_PARAM;
  }

  if (!url || !*url || !len) {
    return NPERR_INVALID_URL;
  }

  *len = 0;

  switch (variable) {
  case NPNURLVProxy:
    {
      nsCOMPtr<nsIPluginHost> pluginHostCOM(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID));
      nsPluginHost *pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
      if (pluginHost && NS_SUCCEEDED(pluginHost->FindProxyForURL(url, value))) {
        *len = *value ? strlen(*value) : 0;
        return NPERR_NO_ERROR;
      }
      break;
    }
  case NPNURLVCookie:
    {
      nsCOMPtr<nsICookieService> cookieService =
        do_GetService(NS_COOKIESERVICE_CONTRACTID);

      if (!cookieService)
        return NPERR_GENERIC_ERROR;

      // Make an nsURI from the url argument
      nsCOMPtr<nsIURI> uri;
      if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), nsDependentCString(url)))) {
        return NPERR_GENERIC_ERROR;
      }

      nsCOMPtr<nsIChannel> channel = GetChannelFromNPP(instance);

      if (NS_FAILED(cookieService->GetCookieString(uri, channel, value)) ||
          !*value) {
        return NPERR_GENERIC_ERROR;
      }

      *len = strlen(*value);
      return NPERR_NO_ERROR;
    }

    break;
  default:
    // Fall through and return an error...
    ;
  }

  return NPERR_GENERIC_ERROR;
}

NPError
_setvalueforurl(NPP instance, NPNURLVariable variable, const char *url,
                const char *value, uint32_t len)
{
  if (!instance) {
    return NPERR_INVALID_PARAM;
  }

  if (!url || !*url) {
    return NPERR_INVALID_URL;
  }

  switch (variable) {
  case NPNURLVCookie:
    {
      if (!url || !value || (0 >= len))
        return NPERR_INVALID_PARAM;

      nsresult rv = NS_ERROR_FAILURE;
      nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
      if (NS_FAILED(rv))
        return NPERR_GENERIC_ERROR;

      nsCOMPtr<nsICookieService> cookieService = do_GetService(NS_COOKIESERVICE_CONTRACTID, &rv);
      if (NS_FAILED(rv))
        return NPERR_GENERIC_ERROR;

      nsCOMPtr<nsIURI> uriIn;
      rv = ioService->NewURI(nsDependentCString(url), nullptr, nullptr, getter_AddRefs(uriIn));
      if (NS_FAILED(rv))
        return NPERR_GENERIC_ERROR;

      nsCOMPtr<nsIChannel> channel = GetChannelFromNPP(instance);

      char *cookie = (char*)value;
      char c = cookie[len];
      cookie[len] = '\0';
      rv = cookieService->SetCookieString(uriIn, nullptr, cookie, channel);
      cookie[len] = c;
      if (NS_SUCCEEDED(rv))
        return NPERR_NO_ERROR;
    }

    break;
  case NPNURLVProxy:
    // We don't support setting proxy values, fall through...
  default:
    // Fall through and return an error...
    ;
  }

  return NPERR_GENERIC_ERROR;
}

NPError
_getauthenticationinfo(NPP instance, const char *protocol, const char *host,
                       int32_t port, const char *scheme, const char *realm,
                       char **username, uint32_t *ulen, char **password,
                       uint32_t *plen)
{
  if (!instance || !protocol || !host || !scheme || !realm || !username ||
      !ulen || !password || !plen)
    return NPERR_INVALID_PARAM;

  *username = nullptr;
  *password = nullptr;
  *ulen = 0;
  *plen = 0;

  nsDependentCString proto(protocol);

  if (!proto.LowerCaseEqualsLiteral("http") &&
      !proto.LowerCaseEqualsLiteral("https"))
    return NPERR_GENERIC_ERROR;

  nsCOMPtr<nsIHttpAuthManager> authManager =
    do_GetService("@mozilla.org/network/http-auth-manager;1");
  if (!authManager)
    return NPERR_GENERIC_ERROR;

  nsNPAPIPluginInstance *inst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);
  if (!inst)
    return NPERR_GENERIC_ERROR;

  bool authPrivate = false;
  if (NS_FAILED(inst->IsPrivateBrowsing(&authPrivate)))
    return NPERR_GENERIC_ERROR;

  nsIDocument *doc = GetDocumentFromNPP(instance);
  NS_ENSURE_TRUE(doc, NPERR_GENERIC_ERROR);
  nsIPrincipal *principal = doc->NodePrincipal();

  nsAutoString unused, uname16, pwd16;
  if (NS_FAILED(authManager->GetAuthIdentity(proto, nsDependentCString(host),
                                             port, nsDependentCString(scheme),
                                             nsDependentCString(realm),
                                             EmptyCString(), unused, uname16,
                                             pwd16, authPrivate, principal))) {
    return NPERR_GENERIC_ERROR;
  }

  NS_ConvertUTF16toUTF8 uname8(uname16);
  NS_ConvertUTF16toUTF8 pwd8(pwd16);

  *username = ToNewCString(uname8);
  *ulen = *username ? uname8.Length() : 0;

  *password = ToNewCString(pwd8);
  *plen = *password ? pwd8.Length() : 0;

  return NPERR_NO_ERROR;
}

uint32_t
_scheduletimer(NPP instance, uint32_t interval, NPBool repeat, PluginTimerFunc timerFunc)
{
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)instance->ndata;
  if (!inst)
    return 0;

  return inst->ScheduleTimer(interval, repeat, timerFunc);
}

void
_unscheduletimer(NPP instance, uint32_t timerID)
{
#ifdef MOZ_WIDGET_ANDROID
  // Sometimes Flash calls this with a dead NPP instance. Ensure the one we have
  // here is valid and maps to a nsNPAPIPluginInstance.
  nsNPAPIPluginInstance *inst = nsNPAPIPluginInstance::GetFromNPP(instance);
#else
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)instance->ndata;
#endif
  if (!inst)
    return;

  inst->UnscheduleTimer(timerID);
}

NPError
_popupcontextmenu(NPP instance, NPMenu* menu)
{
#ifdef MOZ_WIDGET_COCOA
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)instance->ndata;

  double pluginX, pluginY;
  double screenX, screenY;

  const NPCocoaEvent* currentEvent = static_cast<NPCocoaEvent*>(inst->GetCurrentEvent());
  if (!currentEvent) {
    return NPERR_GENERIC_ERROR;
  }

  // Ensure that the events has an x/y value.
  if (currentEvent->type != NPCocoaEventMouseDown    &&
      currentEvent->type != NPCocoaEventMouseUp      &&
      currentEvent->type != NPCocoaEventMouseMoved   &&
      currentEvent->type != NPCocoaEventMouseEntered &&
      currentEvent->type != NPCocoaEventMouseExited  &&
      currentEvent->type != NPCocoaEventMouseDragged) {
      return NPERR_GENERIC_ERROR;
  }

  pluginX = currentEvent->data.mouse.pluginX;
  pluginY = currentEvent->data.mouse.pluginY;

  if ((pluginX < 0.0) || (pluginY < 0.0))
    return NPERR_GENERIC_ERROR;

  NPBool success = _convertpoint(instance,
                                 pluginX,  pluginY, NPCoordinateSpacePlugin,
                                 &screenX, &screenY, NPCoordinateSpaceScreen);

  if (success) {
    return mozilla::plugins::PluginUtilsOSX::ShowCocoaContextMenu(menu,
                                    screenX, screenY,
                                    nullptr,
                                    nullptr);
  } else {
    NS_WARNING("Convertpoint failed, could not created contextmenu.");
    return NPERR_GENERIC_ERROR;
  }
#else
    NS_WARNING("Not supported on this platform!");
    return NPERR_GENERIC_ERROR;
#endif
}

NPBool
_convertpoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace)
{
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)instance->ndata;
  if (!inst)
    return false;

  return inst->ConvertPoint(sourceX, sourceY, sourceSpace, destX, destY, destSpace);
}

void
_urlredirectresponse(NPP instance, void* notifyData, NPBool allow)
{
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)instance->ndata;
  if (!inst) {
    return;
  }

  inst->URLRedirectResponse(notifyData, allow);
}

} /* namespace parent */
} /* namespace plugins */
} /* namespace mozilla */

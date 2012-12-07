/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et cin sw=2 sts=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class implementing nsIObjectLoadingContent for use by
 * various content nodes that want to provide plugin/document/image
 * loading functionality (eg <embed>, <object>, <applet>, etc).
 */

// Interface headers
#include "imgLoader.h"
#include "nsEventDispatcher.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDOMDataContainerEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIExternalProtocolHandler.h"
#include "nsEventStates.h"
#include "nsIObjectFrame.h"
#include "nsIPluginDocument.h"
#include "nsIPermissionManager.h"
#include "nsPluginHost.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamConverterService.h"
#include "nsIURILoader.h"
#include "nsIURL.h"
#include "nsIWebNavigation.h"
#include "nsIWebNavigationInfo.h"
#include "nsIScriptChannel.h"
#include "nsIBlocklistService.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIAppShell.h"

#include "nsError.h"

// Util headers
#include "prenv.h"
#include "prlog.h"

#include "nsAutoPtr.h"
#include "nsCURILoader.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocShellCID.h"
#include "nsGkAtoms.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsStyleUtil.h"
#include "nsGUIEvent.h"
#include "nsUnicharUtils.h"

// Concrete classes
#include "nsFrameLoader.h"

#include "nsObjectLoadingContent.h"
#include "mozAutoDocUpdate.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIChannelPolicy.h"
#include "nsChannelPolicy.h"
#include "mozilla/dom/Element.h"
#include "sampler.h"
#include "nsObjectFrame.h"
#include "nsDOMClassInfo.h"

#include "nsWidgetsCID.h"
#include "nsContentCID.h"
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef PR_LOGGING
static PRLogModuleInfo*
GetObjectLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("objlc");
  return sLog;
}
#endif

#define LOG(args) PR_LOG(GetObjectLog(), PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(GetObjectLog(), PR_LOG_DEBUG)

static bool
InActiveDocument(nsIContent *aContent)
{
  if (!aContent->IsInDoc()) {
    return false;
  }
  nsIDocument *doc = aContent->OwnerDoc();
  return (doc && doc->IsActive());
}

///
/// Runnables and helper classes
///

class nsAsyncInstantiateEvent : public nsRunnable {
public:
  nsAsyncInstantiateEvent(nsObjectLoadingContent *aContent)
  : mContent(aContent) {}

  ~nsAsyncInstantiateEvent() {}

  NS_IMETHOD Run();

private:
  nsCOMPtr<nsIObjectLoadingContent> mContent;
};

NS_IMETHODIMP
nsAsyncInstantiateEvent::Run()
{
  nsObjectLoadingContent *objLC =
    static_cast<nsObjectLoadingContent *>(mContent.get());

  // do nothing if we've been revoked
  if (objLC->mPendingInstantiateEvent != this) {
    return NS_OK;
  }
  objLC->mPendingInstantiateEvent = nullptr;

  return objLC->SyncStartPluginInstance();
}

// Checks to see if the content for a plugin instance has a parent.
// The plugin instance is stopped if there is no parent.
class InDocCheckEvent : public nsRunnable {
public:
  InDocCheckEvent(nsObjectLoadingContent *aContent)
  : mContent(aContent) {}

  ~InDocCheckEvent() {}

  NS_IMETHOD Run();

private:
  nsCOMPtr<nsIObjectLoadingContent> mContent;
};

NS_IMETHODIMP
InDocCheckEvent::Run()
{
  nsObjectLoadingContent *objLC =
    static_cast<nsObjectLoadingContent *>(mContent.get());

  nsCOMPtr<nsIContent> content =
    do_QueryInterface(static_cast<nsIImageLoadingContent *>(objLC));

  if (!InActiveDocument(content)) {
    nsObjectLoadingContent *objLC =
      static_cast<nsObjectLoadingContent *>(mContent.get());
    objLC->UnloadObject();
  }
  return NS_OK;
}

/**
 * Helper task for firing simple events
 */
class nsSimplePluginEvent : public nsRunnable {
public:
  nsSimplePluginEvent(nsIContent* aContent, const nsAString &aEvent)
    : mContent(aContent),
      mEvent(aEvent)
  {}

  ~nsSimplePluginEvent() {}

  NS_IMETHOD Run();

private:
  nsCOMPtr<nsIContent> mContent;
  nsString mEvent;
};

NS_IMETHODIMP
nsSimplePluginEvent::Run()
{
  LOG(("OBJLC [%p]: nsSimplePluginEvent firing event \"%s\"", mContent.get(),
       mEvent.get()));
  nsContentUtils::DispatchTrustedEvent(mContent->GetDocument(), mContent,
                                       mEvent, true, true);
  return NS_OK;
}

/**
 * A task for firing PluginCrashed DOM Events.
 */
class nsPluginCrashedEvent : public nsRunnable {
public:
  nsCOMPtr<nsIContent> mContent;
  nsString mPluginDumpID;
  nsString mBrowserDumpID;
  nsString mPluginName;
  nsString mPluginFilename;
  bool mSubmittedCrashReport;

  nsPluginCrashedEvent(nsIContent* aContent,
                       const nsAString& aPluginDumpID,
                       const nsAString& aBrowserDumpID,
                       const nsAString& aPluginName,
                       const nsAString& aPluginFilename,
                       bool submittedCrashReport)
    : mContent(aContent),
      mPluginDumpID(aPluginDumpID),
      mBrowserDumpID(aBrowserDumpID),
      mPluginName(aPluginName),
      mPluginFilename(aPluginFilename),
      mSubmittedCrashReport(submittedCrashReport)
  {}

  ~nsPluginCrashedEvent() {}

  NS_IMETHOD Run();
};

NS_IMETHODIMP
nsPluginCrashedEvent::Run()
{
  LOG(("OBJLC [%p]: Firing plugin crashed event\n",
       mContent.get()));

  nsCOMPtr<nsIDOMDocument> domDoc =
    do_QueryInterface(mContent->GetDocument());
  if (!domDoc) {
    NS_WARNING("Couldn't get document for PluginCrashed event!");
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEvent> event;
  domDoc->CreateEvent(NS_LITERAL_STRING("datacontainerevents"),
                      getter_AddRefs(event));
  nsCOMPtr<nsIDOMDataContainerEvent> containerEvent(do_QueryInterface(event));
  if (!containerEvent) {
    NS_WARNING("Couldn't QI event for PluginCrashed event!");
    return NS_OK;
  }

  event->InitEvent(NS_LITERAL_STRING("PluginCrashed"), true, true);
  event->SetTrusted(true);
  event->GetInternalNSEvent()->flags |= NS_EVENT_FLAG_ONLY_CHROME_DISPATCH;

  nsCOMPtr<nsIWritableVariant> variant;

  // add a "pluginDumpID" property to this event
  variant = do_CreateInstance("@mozilla.org/variant;1");
  if (!variant) {
    NS_WARNING("Couldn't create pluginDumpID variant for PluginCrashed event!");
    return NS_OK;
  }
  variant->SetAsAString(mPluginDumpID);
  containerEvent->SetData(NS_LITERAL_STRING("pluginDumpID"), variant);

  // add a "browserDumpID" property to this event
  variant = do_CreateInstance("@mozilla.org/variant;1");
  if (!variant) {
    NS_WARNING("Couldn't create browserDumpID variant for PluginCrashed event!");
    return NS_OK;
  }
  variant->SetAsAString(mBrowserDumpID);
  containerEvent->SetData(NS_LITERAL_STRING("browserDumpID"), variant);

  // add a "pluginName" property to this event
  variant = do_CreateInstance("@mozilla.org/variant;1");
  if (!variant) {
    NS_WARNING("Couldn't create pluginName variant for PluginCrashed event!");
    return NS_OK;
  }
  variant->SetAsAString(mPluginName);
  containerEvent->SetData(NS_LITERAL_STRING("pluginName"), variant);

  // add a "pluginFilename" property to this event
  variant = do_CreateInstance("@mozilla.org/variant;1");
  if (!variant) {
    NS_WARNING("Couldn't create pluginFilename variant for PluginCrashed event!");
    return NS_OK;
  }
  variant->SetAsAString(mPluginFilename);
  containerEvent->SetData(NS_LITERAL_STRING("pluginFilename"), variant);

  // add a "submittedCrashReport" property to this event
  variant = do_CreateInstance("@mozilla.org/variant;1");
  if (!variant) {
    NS_WARNING("Couldn't create crashSubmit variant for PluginCrashed event!");
    return NS_OK;
  }
  variant->SetAsBool(mSubmittedCrashReport);
  containerEvent->SetData(NS_LITERAL_STRING("submittedCrashReport"), variant);

  nsEventDispatcher::DispatchDOMEvent(mContent, nullptr, event, nullptr, nullptr);
  return NS_OK;
}

class nsStopPluginRunnable : public nsRunnable, public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsStopPluginRunnable(nsPluginInstanceOwner* aInstanceOwner,
                       nsObjectLoadingContent* aContent)
    : mInstanceOwner(aInstanceOwner)
    , mContent(aContent)
  {
    NS_ASSERTION(aInstanceOwner, "need an owner");
    NS_ASSERTION(aContent, "need a nsObjectLoadingContent");
  }

  // nsRunnable
  NS_IMETHOD Run();

  // nsITimerCallback
  NS_IMETHOD Notify(nsITimer *timer);

private:
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<nsPluginInstanceOwner> mInstanceOwner;
  nsCOMPtr<nsIObjectLoadingContent> mContent;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsStopPluginRunnable, nsRunnable, nsITimerCallback)

NS_IMETHODIMP
nsStopPluginRunnable::Notify(nsITimer *aTimer)
{
  return Run();
}

NS_IMETHODIMP
nsStopPluginRunnable::Run()
{
  // InitWithCallback calls Release before AddRef so we need to hold a
  // strong ref on 'this' since we fall through to this scope if it fails.
  nsCOMPtr<nsITimerCallback> kungFuDeathGrip = this;
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    uint32_t currentLevel = 0;
    appShell->GetEventloopNestingLevel(&currentLevel);
    if (currentLevel > mInstanceOwner->GetLastEventloopNestingLevel()) {
      if (!mTimer)
        mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (mTimer) {
        // Fire 100ms timer to try to tear down this plugin as quickly as
        // possible once the nesting level comes back down.
        nsresult rv = mTimer->InitWithCallback(this, 100,
                                               nsITimer::TYPE_ONE_SHOT);
        if (NS_SUCCEEDED(rv)) {
          return rv;
        }
      }
      NS_ERROR("Failed to setup a timer to stop the plugin later (at a safe "
               "time). Stopping the plugin now, this might crash.");
    }
  }

  mTimer = nullptr;

  static_cast<nsObjectLoadingContent*>(mContent.get())->
    DoStopPlugin(mInstanceOwner, false, true);

  return NS_OK;
}

// You can't take the address of bitfield members, so we have two separate
// classes for these :-/

// Sets a object's mInstantiating bit to false when destroyed
class AutoSetInstantiatingToFalse {
public:
  AutoSetInstantiatingToFalse(nsObjectLoadingContent *aContent)
    : mContent(aContent) {}
  ~AutoSetInstantiatingToFalse() { mContent->mInstantiating = false; }
private:
  nsObjectLoadingContent* mContent;
};

// Sets a object's mInstantiating bit to false when destroyed
class AutoSetLoadingToFalse {
public:
  AutoSetLoadingToFalse(nsObjectLoadingContent *aContent)
    : mContent(aContent) {}
  ~AutoSetLoadingToFalse() { mContent->mIsLoading = false; }
private:
  nsObjectLoadingContent* mContent;
};

///
/// Helper functions
///

static bool
IsSuccessfulRequest(nsIRequest* aRequest)
{
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  if (NS_FAILED(rv) || NS_FAILED(status)) {
    return false;
  }

  // This may still be an error page or somesuch
  nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(aRequest));
  if (httpChan) {
    bool success;
    rv = httpChan->GetRequestSucceeded(&success);
    if (NS_FAILED(rv) || !success) {
      return false;
    }
  }

  // Otherwise, the request is successful
  return true;
}

static bool
CanHandleURI(nsIURI* aURI)
{
  nsAutoCString scheme;
  if (NS_FAILED(aURI->GetScheme(scheme))) {
    return false;
  }

  nsIIOService* ios = nsContentUtils::GetIOService();
  if (!ios)
    return false;

  nsCOMPtr<nsIProtocolHandler> handler;
  ios->GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
  if (!handler) {
    return false;
  }

  nsCOMPtr<nsIExternalProtocolHandler> extHandler =
    do_QueryInterface(handler);
  // We can handle this URI if its protocol handler is not the external one
  return extHandler == nullptr;
}

// Helper for tedious URI equality syntax when one or both arguments may be
// null and URIEquals(null, null) should be true
static bool inline
URIEquals(nsIURI *a, nsIURI *b)
{
  bool equal;
  return (!a && !b) || (a && b && NS_SUCCEEDED(a->Equals(b, &equal)) && equal);
}

static bool
IsSupportedImage(const nsCString& aMimeType)
{
  return imgLoader::SupportImageWithMimeType(aMimeType.get());
}

static void
GetExtensionFromURI(nsIURI* uri, nsCString& ext)
{
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (url) {
    url->GetFileExtension(ext);
  } else {
    nsCString spec;
    uri->GetSpec(spec);

    int32_t offset = spec.RFindChar('.');
    if (offset != kNotFound) {
      ext = Substring(spec, offset + 1, spec.Length());
    }
  }
}

/**
 * Checks whether a plugin exists and is enabled for the extension
 * in the given URI. The MIME type is returned in the mimeType out parameter.
 */
bool
IsPluginEnabledByExtension(nsIURI* uri, nsCString& mimeType)
{
  nsAutoCString ext;
  GetExtensionFromURI(uri, ext);

  if (ext.IsEmpty()) {
    return false;
  }

  nsRefPtr<nsPluginHost> pluginHost =
    already_AddRefed<nsPluginHost>(nsPluginHost::GetInst());

  if (!pluginHost) {
    NS_NOTREACHED("No pluginhost");
    return false;
  }

  const char* typeFromExt;
  nsresult rv = pluginHost->IsPluginEnabledForExtension(ext.get(), typeFromExt);
  if (NS_SUCCEEDED(rv)) {
    mimeType = typeFromExt;
    return true;
  }
  return false;
}

nsresult
IsPluginEnabledForType(const nsCString& aMIMEType)
{
  nsRefPtr<nsPluginHost> pluginHost =
    already_AddRefed<nsPluginHost>(nsPluginHost::GetInst());

  if (!pluginHost) {
    NS_NOTREACHED("No pluginhost");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = pluginHost->IsPluginEnabledForType(aMIMEType.get());

  // Check to see if the plugin is disabled before deciding if it
  // should be in the "click to play" state, since we only want to
  // display "click to play" UI for enabled plugins.
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

///
/// Member Functions
///

bool
nsObjectLoadingContent::IsSupportedDocument(const nsCString& aMimeType)
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  nsCOMPtr<nsIWebNavigationInfo> info(
    do_GetService(NS_WEBNAVIGATION_INFO_CONTRACTID));
  if (!info) {
    return false;
  }

  nsCOMPtr<nsIWebNavigation> webNav;
  nsIDocument* currentDoc = thisContent->GetCurrentDoc();
  if (currentDoc) {
    webNav = do_GetInterface(currentDoc->GetScriptGlobalObject());
  }
  
  uint32_t supported;
  nsresult rv = info->IsTypeSupported(aMimeType, webNav, &supported);

  if (NS_FAILED(rv)) {
    return false;
  }

  if (supported != nsIWebNavigationInfo::UNSUPPORTED) {
    // Don't want to support plugins as documents
    return supported != nsIWebNavigationInfo::PLUGIN;
  }

  // Try a stream converter
  // NOTE: We treat any type we can convert from as a supported type. If a
  // type is not actually supported, the URI loader will detect that and
  // return an error, and we'll fallback.
  nsCOMPtr<nsIStreamConverterService> convServ =
    do_GetService("@mozilla.org/streamConverters;1");
  bool canConvert = false;
  if (convServ) {
    rv = convServ->CanConvert(aMimeType.get(), "*/*", &canConvert);
  }
  return NS_SUCCEEDED(rv) && canConvert;
}

nsresult
nsObjectLoadingContent::BindToTree(nsIDocument* aDocument,
                                   nsIContent* aParent,
                                   nsIContent* aBindingParent,
                                   bool aCompileEventHandlers)
{
  nsImageLoadingContent::BindToTree(aDocument, aParent, aBindingParent,
                                    aCompileEventHandlers);

  if (aDocument) {
    return aDocument->AddPlugin(this);
  }
  return NS_OK;
}

void
nsObjectLoadingContent::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsImageLoadingContent::UnbindFromTree(aDeep, aNullParent);

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIObjectLoadingContent*>(this));
  MOZ_ASSERT(thisContent);
  nsIDocument* ownerDoc = thisContent->OwnerDoc();
  ownerDoc->RemovePlugin(this);

  if (mType == eType_Plugin && mInstanceOwner) {
    // we'll let the plugin continue to run at least until we get back to
    // the event loop. If we get back to the event loop and the node
    // has still not been added back to the document then we tear down the
    // plugin
    nsCOMPtr<nsIRunnable> event = new InDocCheckEvent(this);

    nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
    if (appShell) {
      appShell->RunInStableState(event);
    }
  } else if (mType != eType_Image) {
    // nsImageLoadingContent handles the image case.
    // Reset state and clear pending events
    /// XXX(johns): The implementation for GenericFrame notes that ideally we
    ///             would keep the docshell around, but trash the frameloader
    UnloadObject();
  }

}

nsObjectLoadingContent::nsObjectLoadingContent()
  : mPendingInstantiateEvent(nullptr)
  , mChannel(nullptr)
  , mType(eType_Loading)
  , mFallbackType(eFallbackAlternate)
  , mChannelLoaded(false)
  , mInstantiating(false)
  , mNetworkCreated(true)
  , mActivated(false)
  , mPlayPreviewCanceled(false)
  , mIsStopping(false)
  , mIsLoading(false)
  , mScriptRequested(false)
  , mSrcStreamLoading(false) {}

nsObjectLoadingContent::~nsObjectLoadingContent()
{
  // Should have been unbound from the tree at this point, and InDocCheckEvent
  // keeps us alive
  if (mFrameLoader) {
    NS_NOTREACHED("Should not be tearing down frame loaders at this point");
    mFrameLoader->Destroy();
  }
  if (mInstanceOwner) {
    // This is especially bad as delayed stop will try to hold on to this
    // object...
    NS_NOTREACHED("Should not be tearing down a plugin at this point!");
    StopPluginInstance();
  }
  DestroyImageLoadingContent();
}

nsresult
nsObjectLoadingContent::InstantiatePluginInstance()
{
  if (mInstanceOwner || mType != eType_Plugin || mIsLoading || mInstantiating) {
    return NS_OK;
  }
  
  mInstantiating = true;
  AutoSetInstantiatingToFalse autoInstantiating(this);

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent *>(this));

  nsIDocument* doc = thisContent->GetCurrentDoc();
  if (!doc || !InActiveDocument(thisContent)) {
    NS_ERROR("Shouldn't be calling "
             "InstantiatePluginInstance without an active document");
    return NS_ERROR_FAILURE;
  }

  // Instantiating an instance can result in script execution, which
  // can destroy this DOM object. Don't allow that for the scope
  // of this method.
  nsCOMPtr<nsIObjectLoadingContent> kungFuDeathGrip = this;

  // Flush layout so that the frame is created if possible and the plugin is
  // initialized with the latest information.
  doc->FlushPendingNotifications(Flush_Layout);
  
  if (!thisContent->GetPrimaryFrame()) {
    LOG(("OBJLC [%p]: Not instantiating plugin with no frame", this));
    return NS_OK;
  }
  
  nsresult rv = NS_ERROR_FAILURE;
  nsRefPtr<nsPluginHost> pluginHost =
    already_AddRefed<nsPluginHost>(nsPluginHost::GetInst());

  if (!pluginHost) {
    NS_NOTREACHED("No pluginhost");
    return NS_ERROR_FAILURE;
  }

  // If you add early return(s), be sure to balance this call to
  // appShell->SuspendNative() with additional call(s) to
  // appShell->ReturnNative().
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->SuspendNative();
  }

  nsCOMPtr<nsIPluginDocument> pDoc(do_QueryInterface(doc));
  bool fullPageMode = false;
  if (pDoc) {
    pDoc->GetWillHandleInstantiation(&fullPageMode);
  }

  if (fullPageMode) {
    nsCOMPtr<nsIStreamListener> stream;
    rv = pluginHost->InstantiateFullPagePluginInstance(mContentType.get(),
                                                       mURI.get(), this,
                                                       getter_AddRefs(mInstanceOwner),
                                                       getter_AddRefs(stream));
    if (NS_SUCCEEDED(rv)) {
      pDoc->SetStreamListener(stream);
    }
  } else {
    rv = pluginHost->InstantiateEmbeddedPluginInstance(mContentType.get(),
                                                       mURI.get(), this,
                                                       getter_AddRefs(mInstanceOwner));
  }

  if (appShell) {
    appShell->ResumeNative();
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Set up scripting interfaces.
  NotifyContentObjectWrapper();

  nsRefPtr<nsNPAPIPluginInstance> pluginInstance;
  GetPluginInstance(getter_AddRefs(pluginInstance));
  if (pluginInstance) {
    nsCOMPtr<nsIPluginTag> pluginTag;
    pluginHost->GetPluginTagForInstance(pluginInstance,
                                        getter_AddRefs(pluginTag));

    nsCOMPtr<nsIBlocklistService> blocklist =
      do_GetService("@mozilla.org/extensions/blocklist;1");
    if (blocklist) {
      uint32_t blockState = nsIBlocklistService::STATE_NOT_BLOCKED;
      blocklist->GetPluginBlocklistState(pluginTag, EmptyString(),
                                         EmptyString(), &blockState);
      if (blockState == nsIBlocklistService::STATE_OUTDATED) {
        // Fire plugin outdated event if necessary
        LOG(("OBJLC [%p]: Dispatching plugin outdated event for content %p\n",
             this));
        nsCOMPtr<nsIRunnable> ev = new nsSimplePluginEvent(thisContent,
                                                     NS_LITERAL_STRING("PluginOutdated"));
        nsresult rv = NS_DispatchToCurrentThread(ev);
        if (NS_FAILED(rv)) {
          NS_WARNING("failed to dispatch nsSimplePluginEvent");
        }
      }
    }
  }

  return NS_OK;
}

void
nsObjectLoadingContent::NotifyOwnerDocumentActivityChanged()
{
  if (!mInstanceOwner) {
    return;
  }

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsIDocument* ownerDoc = thisContent->OwnerDoc();
  if (!ownerDoc->IsActive()) {
    StopPluginInstance();
  }
}

// nsIRequestObserver
NS_IMETHODIMP
nsObjectLoadingContent::OnStartRequest(nsIRequest *aRequest,
                                       nsISupports *aContext)
{
  /// This must call LoadObject, even upon failure, to allow it to either
  /// proceed with the load, or trigger fallback content.

  SAMPLE_LABEL("nsObjectLoadingContent", "OnStartRequest");

  LOG(("OBJLC [%p]: Channel OnStartRequest", this));

  if (aRequest != mChannel || !aRequest) {
    // happens when a new load starts before the previous one got here
    return NS_BINDING_ABORTED;
  }

  NS_ASSERTION(!mChannelLoaded, "mChannelLoaded set already?");
  NS_ASSERTION(!mFinalListener, "mFinalListener exists already?");

  mChannelLoaded = true;

  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  NS_ASSERTION(chan, "Why is our request not a channel?");

  nsCOMPtr<nsIURI> uri;

  if (IsSuccessfulRequest(aRequest)) {
    chan->GetURI(getter_AddRefs(uri));
  }

  if (!uri) {
    LOG(("OBJLC [%p]: OnStartRequest: Request failed\n", this));
    // If the request fails, we still call LoadObject() to handle fallback
    // content and notifying of failure. (mChannelLoaded && !mChannel) indicates
    // the bad state.
    mChannel = nullptr;
    LoadObject(true, false);
    return NS_ERROR_FAILURE;
  }

  return LoadObject(true, false, aRequest);
}

NS_IMETHODIMP
nsObjectLoadingContent::OnStopRequest(nsIRequest *aRequest,
                                      nsISupports *aContext,
                                      nsresult aStatusCode)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);

  if (aRequest != mChannel) {
    return NS_BINDING_ABORTED;
  }

  mChannel = nullptr;

  if (mFinalListener) {
    // This may re-enter in the case of plugin listeners
    nsCOMPtr<nsIStreamListener> listenerGrip(mFinalListener);
    mFinalListener = nullptr;
    listenerGrip->OnStopRequest(aRequest, aContext, aStatusCode);
  }

  // Return value doesn't matter
  return NS_OK;
}


// nsIStreamListener
NS_IMETHODIMP
nsObjectLoadingContent::OnDataAvailable(nsIRequest *aRequest,
                                        nsISupports *aContext,
                                        nsIInputStream *aInputStream,
                                        uint64_t aOffset, uint32_t aCount)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);

  if (aRequest != mChannel) {
    return NS_BINDING_ABORTED;
  }

  if (mFinalListener) {
    // This may re-enter in the case of plugin listeners
    nsCOMPtr<nsIStreamListener> listenerGrip(mFinalListener);
    return listenerGrip->OnDataAvailable(aRequest, aContext, aInputStream,
                                         aOffset, aCount);
  }

  // We shouldn't have a connected channel with no final listener
  NS_NOTREACHED("Got data for channel with no connected final listener");
  mChannel = nullptr;

  return NS_ERROR_UNEXPECTED;
}

// nsIFrameLoaderOwner
NS_IMETHODIMP
nsObjectLoadingContent::GetFrameLoader(nsIFrameLoader** aFrameLoader)
{
  NS_IF_ADDREF(*aFrameLoader = mFrameLoader);
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsFrameLoader>)
nsObjectLoadingContent::GetFrameLoader()
{
  nsFrameLoader* loader = mFrameLoader;
  NS_IF_ADDREF(loader);
  return loader;
}

NS_IMETHODIMP
nsObjectLoadingContent::SwapFrameLoaders(nsIFrameLoaderOwner* aOtherLoader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetActualType(nsACString& aType)
{
  aType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetDisplayedType(uint32_t* aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::HasNewFrame(nsIObjectFrame* aFrame)
{
  if (mType == eType_Plugin) {
    if (!mInstanceOwner) {
      // We have successfully set ourselves up in LoadObject, but not spawned an
      // instance due to a lack of a frame.
      AsyncStartPluginInstance();
      return NS_OK;
    }

    // Disconnect any existing frame
    DisconnectFrame();

    // Set up relationship between instance owner and frame.
    nsObjectFrame *objFrame = static_cast<nsObjectFrame*>(aFrame);
    mInstanceOwner->SetFrame(objFrame);

    // Set up new frame to draw.
    objFrame->FixupWindow(objFrame->GetContentRectRelativeToSelf().Size());
    objFrame->InvalidateFrame();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::DisconnectFrame()
{
  if (mInstanceOwner) {
    mInstanceOwner->SetFrame(nullptr);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetPluginInstance(nsNPAPIPluginInstance** aInstance)
{
  *aInstance = nullptr;

  if (!mInstanceOwner) {
    return NS_OK;
  }

  return mInstanceOwner->GetInstance(aInstance);
}

NS_IMETHODIMP
nsObjectLoadingContent::GetContentTypeForMIMEType(const nsACString& aMIMEType,
                                                  uint32_t* aType)
{
  *aType = GetTypeOfContent(PromiseFlatCString(aMIMEType));
  return NS_OK;
}

// nsIInterfaceRequestor
NS_IMETHODIMP
nsObjectLoadingContent::GetInterface(const nsIID & aIID, void **aResult)
{
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    nsIChannelEventSink* sink = this;
    *aResult = sink;
    NS_ADDREF(sink);
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

// nsIChannelEventSink
NS_IMETHODIMP
nsObjectLoadingContent::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                               nsIChannel *aNewChannel,
                                               uint32_t aFlags,
                                               nsIAsyncVerifyRedirectCallback *cb)
{
  // If we're already busy with a new load, or have no load at all,
  // cancel the redirect.
  if (!mChannel || aOldChannel != mChannel) {
    return NS_BINDING_ABORTED;
  }

  mChannel = aNewChannel;
  cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

// <public>
nsEventStates
nsObjectLoadingContent::ObjectState() const
{
  switch (mType) {
    case eType_Loading:
      return NS_EVENT_STATE_LOADING;
    case eType_Image:
      return ImageState();
    case eType_Plugin:
    case eType_Document:
      // These are OK. If documents start to load successfully, they display
      // something, and are thus not broken in this sense. The same goes for
      // plugins.
      return nsEventStates();
    case eType_Null:
      switch (mFallbackType) {
        case eFallbackSuppressed:
          return NS_EVENT_STATE_SUPPRESSED;
        case eFallbackUserDisabled:
          return NS_EVENT_STATE_USERDISABLED;
        case eFallbackClickToPlay:
          return NS_EVENT_STATE_TYPE_CLICK_TO_PLAY;
        case eFallbackPlayPreview:
          return NS_EVENT_STATE_TYPE_PLAY_PREVIEW;
        case eFallbackDisabled:
          return NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_HANDLER_DISABLED;
        case eFallbackBlocklisted:
          return NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_HANDLER_BLOCKED;
        case eFallbackCrashed:
          return NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_HANDLER_CRASHED;
        case eFallbackUnsupported: {
          // Check to see if plugins are blocked on this platform.
          char* pluginsBlocked = PR_GetEnv("MOZ_PLUGINS_BLOCKED");
          if (pluginsBlocked && pluginsBlocked[0] == '1') {
            return NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_TYPE_UNSUPPORTED_PLATFORM;
          } else {
            return NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_TYPE_UNSUPPORTED;
          }
        }
        case eFallbackOutdated:
        case eFallbackAlternate:
          return NS_EVENT_STATE_BROKEN;
        case eFallbackVulnerableUpdatable:
          return NS_EVENT_STATE_VULNERABLE_UPDATABLE;
        case eFallbackVulnerableNoUpdate:
          return NS_EVENT_STATE_VULNERABLE_NO_UPDATE;
      }
  };
  NS_NOTREACHED("unknown type?");
  return NS_EVENT_STATE_LOADING;
}

bool
nsObjectLoadingContent::CheckLoadPolicy(int16_t *aContentPolicy)
{
  if (!aContentPolicy || !mURI) {
    NS_NOTREACHED("Doing it wrong");
    return false;
  }

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "Must be an instance of content");

  nsIDocument* doc = thisContent->OwnerDoc();

  *aContentPolicy = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OBJECT,
                                          mURI,
                                          doc->NodePrincipal(),
                                          thisContent,
                                          mContentType,
                                          nullptr, //extra
                                          aContentPolicy,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv, false);
  if (NS_CP_REJECTED(*aContentPolicy)) {
    nsAutoCString uri;
    nsAutoCString baseUri;
    mURI->GetSpec(uri);
    mURI->GetSpec(baseUri);
    LOG(("OBJLC [%p]: Content policy denied load of %s (base %s)",
         this, uri.get(), baseUri.get()));
    return false;
  }

  return true;
}

bool
nsObjectLoadingContent::CheckProcessPolicy(int16_t *aContentPolicy)
{
  if (!aContentPolicy) {
    NS_NOTREACHED("Null out variable");
    return false;
  }

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "Must be an instance of content");

  nsIDocument* doc = thisContent->OwnerDoc();
  
  int32_t objectType;
  switch (mType) {
    case eType_Image:
      objectType = nsIContentPolicy::TYPE_IMAGE;
      break;
    case eType_Document:
      objectType = nsIContentPolicy::TYPE_DOCUMENT;
      break;
    case eType_Plugin:
      objectType = nsIContentPolicy::TYPE_OBJECT;
      break;
    default:
      NS_NOTREACHED("Calling checkProcessPolicy with a unloadable type");
      return false;
  }

  *aContentPolicy = nsIContentPolicy::ACCEPT;
  nsresult rv =
    NS_CheckContentProcessPolicy(objectType,
                                 mURI,
                                 doc->NodePrincipal(),
                                 static_cast<nsIImageLoadingContent*>(this),
                                 mContentType,
                                 nullptr, //extra
                                 aContentPolicy,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv, false);

  if (NS_CP_REJECTED(*aContentPolicy)) {
    LOG(("OBJLC [%p]: CheckContentProcessPolicy rejected load", this));
    return false;
  }

  return true;
}

nsObjectLoadingContent::ParameterUpdateFlags
nsObjectLoadingContent::UpdateObjectParameters()
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "Must be an instance of content");

  uint32_t caps = GetCapabilities();
  LOG(("OBJLC [%p]: Updating object parameters", this));

  nsresult rv;
  nsAutoCString newMime;
  nsCOMPtr<nsIURI> newURI;
  nsCOMPtr<nsIURI> newBaseURI;
  ObjectType newType;
  bool isJava = false;
  // Set if this state can't be used to load anything, forces eType_Null
  bool stateInvalid = false;
  // Indicates what parameters changed.
  // eParamChannelChanged - means parameters that affect channel opening
  //                        decisions changed
  // eParamStateChanged -   means anything that affects what content we load
  //                        changed, even if the channel we'd open remains the
  //                        same.
  //
  // State changes outside of the channel parameters only matter if we've
  // already opened a channel or tried to instantiate content, whereas channel
  // parameter changes require re-opening the channel even if we haven't gotten
  // that far.
  nsObjectLoadingContent::ParameterUpdateFlags retval = eParamNoChange;

  ///
  /// Initial MIME Type
  ///
  if (thisContent->NodeInfo()->Equals(nsGkAtoms::applet)) {
    newMime.AssignLiteral("application/x-java-vm");
    isJava = true;
  } else {
    nsAutoString typeAttr;
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, typeAttr);
    if (!typeAttr.IsEmpty()) {
      CopyUTF16toUTF8(typeAttr, newMime);
      isJava = nsPluginHost::IsJavaMIMEType(newMime.get());
    }
  }

  ///
  /// classID
  ///

  if (caps & eSupportClassID) {
    nsAutoString classIDAttr;
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::classid, classIDAttr);
    if (!classIDAttr.IsEmpty()) {
      // Our classid support is limited to 'java:' ids
      rv = IsPluginEnabledForType(NS_LITERAL_CSTRING("application/x-java-vm"));
      if (NS_SUCCEEDED(rv) &&
          StringBeginsWith(classIDAttr, NS_LITERAL_STRING("java:"))) {
        newMime.Assign("application/x-java-vm");
        isJava = true;
      } else {
        // XXX(johns): Our de-facto behavior since forever was to refuse to load
        // Objects who don't have a classid we support, regardless of other type
        // or uri info leads to a valid plugin.
        newMime.Assign("");
        stateInvalid = true;
      }
    }
  }

  ///
  /// Codebase
  ///

  nsAutoString codebaseStr;
  nsCOMPtr<nsIURI> docBaseURI = thisContent->GetBaseURI();
  thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::codebase, codebaseStr);
  if (codebaseStr.IsEmpty() && thisContent->NodeInfo()->Equals(nsGkAtoms::applet)) {
    // bug 406541
    // NOTE we send the full absolute URI resolved here to java in
    //      pluginInstanceOwner to avoid disagreements between parsing of
    //      relative URIs. We need to mimic java's quirks here to make that
    //      not break things.
    codebaseStr.AssignLiteral("/"); // Java resolves codebase="" as "/"
    // XXX(johns) This doesn't catch the case of "file:" which java would
    // interpret as "file:///" but we would interpret as this document's URI
    // but with a changed scheme.
  }

  if (!codebaseStr.IsEmpty()) {
    rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(newBaseURI),
                                                   codebaseStr,
                                                   thisContent->OwnerDoc(),
                                                   docBaseURI);
    if (NS_SUCCEEDED(rv)) {
      NS_TryToSetImmutable(newBaseURI);
    } else {
      // Malformed URI
      LOG(("OBJLC [%p]: Could not parse plugin's codebase as a URI, "
           "will use document baseURI instead", this));
    }
  }

  // Otherwise, use normal document baseURI
  if (!newBaseURI) {
    newBaseURI = docBaseURI;
  }

  ///
  /// URI
  ///

  nsAutoString uriStr;
  // Different elements keep this in various locations
  if (isJava) {
    // Applet tags and embed/object with explicit java MIMEs have
    // src/data attributes that are not parsed as URIs, so we will
    // act as if URI is null
  } else if (thisContent->NodeInfo()->Equals(nsGkAtoms::object)) {
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, uriStr);
  } else if (thisContent->NodeInfo()->Equals(nsGkAtoms::embed)) {
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, uriStr);
  } else {
    // Applet tags should always have a java MIME type at this point
    NS_NOTREACHED("Unrecognized plugin-loading tag");
  }

  // Note that the baseURI changing could affect the newURI, even if uriStr did
  // not change.
  if (!uriStr.IsEmpty()) {
    rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(newURI),
                                                   uriStr,
                                                   thisContent->OwnerDoc(),
                                                   newBaseURI);
    if (NS_SUCCEEDED(rv)) {
      NS_TryToSetImmutable(newURI);
    } else {
      stateInvalid = true;
    }
  }

  // For eAllowPluginSkipChannel tags, if we have a non-plugin type, but can get
  // a plugin type from the extension, prefer that to falling back to a channel.
  if (GetTypeOfContent(newMime) != eType_Plugin && newURI &&
      (caps & eAllowPluginSkipChannel) &&
      IsPluginEnabledByExtension(newURI, newMime)) {
    LOG(("OBJLC [%p]: Using extension as type hint (%s)", this, newMime.get()));
  }

  ///
  /// Check if the original (pre-channel) content-type or URI changed, and
  /// record mOriginal{ContentType,URI}
  ///

  if ((mOriginalContentType != newMime) || !URIEquals(mOriginalURI, newURI)) {
    // These parameters changing requires re-opening the channel, so don't
    // consider the currently-open channel below
    // XXX(johns): Changing the mime type might change our decision on whether
    //             or not we load a channel, so we count changes to it as a
    //             channel parameter change for the sake of simplicity.
    retval = (ParameterUpdateFlags)(retval | eParamChannelChanged);
    LOG(("OBJLC [%p]: Channel parameters changed", this));
  }
  mOriginalContentType = newMime;
  mOriginalURI = newURI;

  ///
  /// If we have a channel, see if its MIME type should take precendence and
  /// check the final (redirected) URL
  ///

  // If we have a loaded channel and channel parameters did not change, use it
  // to determine what we would load.
  bool useChannel = mChannelLoaded && !(retval & eParamChannelChanged);
  if (mChannel && useChannel) {
    nsCString channelType;
    rv = mChannel->GetContentType(channelType);
    if (NS_FAILED(rv)) {
      NS_NOTREACHED("GetContentType failed");
      stateInvalid = true;
      channelType.Assign("");
    }

    LOG(("OBJLC [%p]: Channel has a content type of %s", this, channelType.get()));

    bool binaryChannelType = false;
    if (channelType.EqualsASCII(APPLICATION_GUESS_FROM_EXT)) {
      channelType = APPLICATION_OCTET_STREAM;
      mChannel->SetContentType(channelType);
      binaryChannelType = true;
    } else if (channelType.EqualsASCII(APPLICATION_OCTET_STREAM)
               || channelType.EqualsASCII(BINARY_OCTET_STREAM)) {
      binaryChannelType = true;
    }

    // Channel can change our URI through redirection
    rv = NS_GetFinalChannelURI(mChannel, getter_AddRefs(newURI));
    if (NS_FAILED(rv)) {
      NS_NOTREACHED("NS_GetFinalChannelURI failure");
      stateInvalid = true;
    }

    // The channel type overrides the guessed / provided type, except when:
    //
    // 1) If the channel returns a binary stream type, and we have a type hint
    //    for a non-document (we never want to display binary-as-document),
    //    use our type hint instead.
    // 2) Our type hint is a type that we support with a plugin, ignore the
    //    server's type
    //
    //    XXX(johns): HTML5's "typesmustmatch" attribute would need to be
    //                honored here if implemented

    ObjectType typeHint = newMime.IsEmpty() ? eType_Null : GetTypeOfContent(newMime);

    bool caseOne = binaryChannelType
                   && typeHint != eType_Null
                   && typeHint != eType_Document;
    bool caseTwo = typeHint == eType_Plugin;
    if (caseOne || caseTwo) {
        // Set the type we'll use for dispatch on the channel.  Otherwise we could
        // end up trying to dispatch to a nsFrameLoader, which will complain that
        // it couldn't find a way to handle application/octet-stream
        nsAutoCString typeHint, dummy;
        NS_ParseContentType(newMime, typeHint, dummy);
        if (!typeHint.IsEmpty()) {
          mChannel->SetContentType(typeHint);
        }
    } else if (binaryChannelType
               && IsPluginEnabledByExtension(newURI, newMime)) {
      mChannel->SetContentType(newMime);
    } else {
      newMime = channelType;
      if (nsPluginHost::IsJavaMIMEType(newMime.get())) {
        //   Java does not load with a channel, and being java retroactively changes
        //   how we may have interpreted the codebase to construct this URI above.
        //   Because the behavior here is more or less undefined, play it safe and
        //   reject the load.
        LOG(("OBJLC [%p]: Refusing to load with channel with java MIME",
             this));
        stateInvalid = true;
      }
    }
  }

  if (useChannel && !mChannel) {
    // - (useChannel && !mChannel) is true if a channel was opened but
    //   is no longer around, in which case we can't load.
    stateInvalid = true;
  }

  ///
  /// Determine final type
  ///
  //  1) If we have attempted channel load, or set stateInvalid above, the type
  //     is always null (fallback)
  //  2) Otherwise, If we have a loaded channel, we grabbed its mimeType above,
  //     use that type.
  //  3) Otherwise, See if we can load this as a plugin without a channel
  //     (image/document types always need a channel).
  //     - If we have indication this is a plugin (mime, extension)
  //       AND:
  //       - We have eAllowPluginSkipChannel OR
  //       - We have no URI in the first place (including java)
  //  3) Otherwise, if we have a URI, set type to loading to indicate
  //     we'd need a channel to proceed.
  //  4) Otherwise, type null to indicate unloadable content (fallback)
  //
  // XXX(johns): <embed> tags both support URIs and have
  //   eAllowPluginSkipChannel, meaning it is possible that we have a URI, but
  //   are not going to open a channel for it. The old objLC code did this (in a
  //   less obviously-intended way), so it's probably best not to change our
  //   behavior at this point.
  //

  if (stateInvalid) {
    newType = eType_Null;
    newMime.Truncate();
  } else if (useChannel) {
      // If useChannel is set above, we considered it in setting newMime
      newType = GetTypeOfContent(newMime);
      LOG(("OBJLC [%p]: Using channel type", this));
  } else if (((caps & eAllowPluginSkipChannel) || !newURI) &&
             (GetTypeOfContent(newMime) == eType_Plugin)) {
    newType = eType_Plugin;
    LOG(("OBJLC [%p]: Skipping loading channel, type plugin", this));
  } else if (newURI) {
    // We could potentially load this if we opened a channel on mURI, indicate
    // This by leaving type as loading
    newType = eType_Loading;
  } else {
    // Unloadable - no URI, and no plugin type. Non-plugin types (images,
    // documents) always load with a channel.
    newType = eType_Null;
  }

  ///
  /// Update changed values
  ///

  if (newType != mType) {
    retval = (ParameterUpdateFlags)(retval | eParamStateChanged);
    LOG(("OBJLC [%p]: Type changed from %u -> %u", this, mType, newType));
    mType = newType;
  }

  if (!URIEquals(mBaseURI, newBaseURI)) {
    if (isJava) {
      // Java bases its class loading on the base URI, so we consider the state
      // to have changed if this changes. If the object is using a relative URI,
      // mURI will have changed below regardless
      retval = (ParameterUpdateFlags)(retval | eParamStateChanged);
    }
    LOG(("OBJLC [%p]: Object effective baseURI changed", this));
    mBaseURI = newBaseURI;
  }

  if (!URIEquals(newURI, mURI)) {
    retval = (ParameterUpdateFlags)(retval | eParamStateChanged);
    LOG(("OBJLC [%p]: Object effective URI changed", this));
    mURI = newURI;
  }

  // We don't update content type when loading, as the type is not final and we
  // don't want to superfluously change between mOriginalContentType ->
  // mContentType when doing |obj.data = obj.data| with a channel and differing
  // type.
  if (mType != eType_Loading && mContentType != newMime) {
    retval = (ParameterUpdateFlags)(retval | eParamStateChanged);
    retval = (ParameterUpdateFlags)(retval | eParamContentTypeChanged);
    LOG(("OBJLC [%p]: Object effective mime type changed (%s -> %s)",
         this, mContentType.get(), newMime.get()));
    mContentType = newMime;
  }

  return retval;
}

// Only OnStartRequest should be passing the channel parameter
nsresult
nsObjectLoadingContent::LoadObject(bool aNotify,
                                   bool aForceLoad)
{
  return LoadObject(aNotify, aForceLoad, nullptr);
}

nsresult
nsObjectLoadingContent::LoadObject(bool aNotify,
                                   bool aForceLoad,
                                   nsIRequest *aLoadingChannel)
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");
  nsIDocument* doc = thisContent->OwnerDoc();
  nsresult rv = NS_OK;

  // Sanity check
  if (!InActiveDocument(thisContent)) {
    NS_NOTREACHED("LoadObject called while not bound to an active document");
    return NS_ERROR_UNEXPECTED;
  }

  // XXX(johns): In these cases, we refuse to touch our content and just
  //   remain unloaded, as per legacy behavior. It would make more sense to
  //   load fallback content initially and refuse to ever change state again.
  if (doc->IsBeingUsedAsImage() || doc->IsLoadedAsData()) {
    return NS_OK;
  }

  LOG(("OBJLC [%p]: LoadObject called, notify %u, forceload %u, channel %p",
       this, aNotify, aForceLoad, aLoadingChannel));

  // We can't re-use an already open channel, but aForceLoad may make us try
  // to load a plugin without any changes in channel state.
  if (aForceLoad && mChannelLoaded) {
    CloseChannel();
    mChannelLoaded = false;
  }

  // Save these for NotifyStateChanged();
  nsEventStates oldState = ObjectState();
  ObjectType oldType = mType;

  ParameterUpdateFlags stateChange = UpdateObjectParameters();

  if (!stateChange && !aForceLoad) {
    return NS_OK;
  }

  ///
  /// State has changed, unload existing content and attempt to load new type
  ///
  LOG(("OBJLC [%p]: LoadObject - plugin state changed (%u)",
       this, stateChange));

  // Setup fallback info. We may also change type to fallback below in case of
  // sanity/OOM/etc. errors. We default to showing alternate content
  // NOTE LoadFallback can override this in some cases
  FallbackType fallbackType = eFallbackAlternate;

  // mType can differ with GetTypeOfContent(mContentType) if we support this
  // type, but the parameters are invalid e.g. a embed tag with type "image/png"
  // but no URI -- don't show a plugin error or unknown type error in that case.
  if (mType == eType_Null && GetTypeOfContent(mContentType) == eType_Null) {
    // See if a disabled or blocked plugin could've handled this
    nsresult pluginsupport = IsPluginEnabledForType(mContentType);
    if (pluginsupport == NS_ERROR_PLUGIN_DISABLED) {
      fallbackType = eFallbackDisabled;
    } else if (pluginsupport == NS_ERROR_PLUGIN_BLOCKLISTED) {
      fallbackType = eFallbackBlocklisted;
    } else {
      // Completely unknown type
      fallbackType = eFallbackUnsupported;
    }
  }

  // Explicit user activation should reset if the object changes content types
  if (mActivated && (stateChange & eParamContentTypeChanged)) {
    LOG(("OBJLC [%p]: Content type changed, clearing activation state", this));
    mActivated = false;
  }

  // We synchronously start/stop plugin instances below, which may spin the
  // event loop. Re-entering into the load is fine, but at that point the
  // original load call needs to abort when unwinding
  // NOTE this is located *after* the state change check, a subseqent load
  //      with no subsequently changed state will be a no-op.
  if (mIsLoading) {
    LOG(("OBJLC [%p]: Re-entering into LoadObject", this));
  }
  mIsLoading = true;
  AutoSetLoadingToFalse reentryCheck(this);

  // Unload existing content, keeping in mind stopping plugins might spin the
  // event loop. Note that we check for still-open channels below
  UnloadObject(false); // Don't reset state
  if (!mIsLoading) {
    // The event loop must've spun and re-entered into LoadObject, which
    // finished the load
    LOG(("OBJLC [%p]: Re-entered into LoadObject, aborting outer load", this));
    return NS_OK;
  }

  // Determine what's going on with our channel
  if (stateChange & eParamChannelChanged) {
    // If the channel params changed, throw away the channel, but unset
    // mChannelLoaded so we'll still try to open a new one for this load if
    // necessary
    CloseChannel();
    mChannelLoaded = false;
  } else if (mType == eType_Null && mChannel) {
    // If we opened a channel but then failed to find a loadable state, throw it
    // away. mChannelLoaded will indicate that we tried to load a channel at one
    // point so we wont recurse
    CloseChannel();
  } else if (mType == eType_Loading && mChannel) {
    // We're still waiting on a channel load, already opened one, and
    // channel parameters didn't change
    return NS_OK;
  } else if (mChannelLoaded && mChannel != aLoadingChannel) {
    // The only time we should have a loaded channel with a changed state is
    // when the channel has just opened -- in which case this call should
    // have originated from OnStartRequest
    NS_NOTREACHED("Loading with a channel, but state doesn't make sense");
    return NS_OK;
  }

  //
  // Security checks
  //

  if (mType != eType_Null) {
    int16_t contentPolicy = nsIContentPolicy::ACCEPT;
    bool allowLoad = false;
    // We check load policy before opening a channel, and process policy before
    // going ahead with any final-type load
    if (mType == eType_Loading) {
      nsCOMPtr<nsIScriptSecurityManager> secMan =
        nsContentUtils::GetSecurityManager();
      if (!secMan) {
        NS_NOTREACHED("No security manager?");
      } else {
        rv = secMan->CheckLoadURIWithPrincipal(thisContent->NodePrincipal(),
                                               mURI, 0);
        allowLoad = NS_SUCCEEDED(rv) && CheckLoadPolicy(&contentPolicy);
      }
    } else {
      allowLoad = CheckProcessPolicy(&contentPolicy);
    }

    // Content policy implementations can mutate the DOM, check for re-entry
    if (!mIsLoading) {
      LOG(("OBJLC [%p]: We re-entered in content policy, leaving original load",
           this));
      return NS_OK;
    }
    
    // Load denied, switch to fallback and set disabled/suppressed if applicable
    if (!allowLoad) {
      LOG(("OBJLC [%p]: Load denied by policy", this));
      mType = eType_Null;
      if (contentPolicy == nsIContentPolicy::REJECT_TYPE) {
        // XXX(johns) This is assuming that we were rejected by
        //            nsContentBlocker, which rejects by type if permissions
        //            reject plugins
        fallbackType = eFallbackUserDisabled;
      } else {
        fallbackType = eFallbackSuppressed;
      }
    }
  }

  // Items resolved as Image/Document will not be checked for previews, as well
  // as invalid plugins (they will not have the mContentType set).
  if ((mType == eType_Null || mType == eType_Plugin) && ShouldPreview()) {
    // If plugin preview exists, we shall use it
    LOG(("OBJLC [%p]: Using plugin preview", this));
    mType = eType_Null;
    fallbackType = eFallbackPlayPreview;
  }

  // If we're a plugin but shouldn't start yet, load fallback with
  // reason click-to-play instead
  FallbackType clickToPlayReason;
  if (mType == eType_Plugin && !ShouldPlay(clickToPlayReason)) {
    LOG(("OBJLC [%p]: Marking plugin as click-to-play", this));
    mType = eType_Null;
    fallbackType = clickToPlayReason;
  }

  if (!mActivated && mType == eType_Plugin) {
    // Object passed ShouldPlay and !ShouldPreview, so it should be considered
    // activated until it changes content type
    LOG(("OBJLC [%p]: Object implicitly activated", this));
    mActivated = true;
  }

  // Sanity check: We shouldn't have any loaded resources, pending events, or
  // a final listener at this point
  if (mFrameLoader || mPendingInstantiateEvent || mInstanceOwner ||
      mFinalListener)
  {
    NS_NOTREACHED("Trying to load new plugin with existing content");
    rv = NS_ERROR_UNEXPECTED;
    return NS_OK;
  }

  // More sanity-checking:
  // If mChannel is set, mChannelLoaded should be set, and vice-versa
  if (mType != eType_Null && !!mChannel != mChannelLoaded) {
    NS_NOTREACHED("Trying to load with bad channel state");
    rv = NS_ERROR_UNEXPECTED;
    return NS_OK;
  }

  ///
  /// Attempt to load new type
  ///

  // We don't set mFinalListener until OnStartRequest has been called, to
  // prevent re-entry ugliness with CloseChannel()
  nsCOMPtr<nsIStreamListener> finalListener;
  switch (mType) {
    case eType_Image:
      if (!mChannel) {
        // We have a LoadImage() call, but UpdateObjectParameters requires a
        // channel for images, so this is not a valid state.
        NS_NOTREACHED("Attempting to load image without a channel?");
        rv = NS_ERROR_UNEXPECTED;
        break;
      }
      rv = LoadImageWithChannel(mChannel, getter_AddRefs(finalListener));
      // finalListener will receive OnStartRequest below
    break;
    case eType_Plugin:
    {
      if (mChannel) {
        nsRefPtr<nsPluginHost> pluginHost =
          already_AddRefed<nsPluginHost>(nsPluginHost::GetInst());
        if (!pluginHost) {
          NS_NOTREACHED("No pluginHost");
          rv = NS_ERROR_UNEXPECTED;
          break;
        }

        // Force a sync state change now, we need the frame created
        NotifyStateChanged(oldType, oldState, true, aNotify);
        oldType = mType;
        oldState = ObjectState();

        if (!thisContent->GetPrimaryFrame()) {
          // We're un-rendered, and can't instantiate a plugin. HasNewFrame will
          // re-start us when we can proceed.
          LOG(("OBJLC [%p]: Aborting load - plugin-type, but no frame", this));
          CloseChannel();
          break;
        }
        
        rv = pluginHost->NewEmbeddedPluginStreamListener(mURI, this, nullptr,
                                                         getter_AddRefs(finalListener));
        // finalListener will receive OnStartRequest below
      } else {
        rv = AsyncStartPluginInstance();
      }
    }
    break;
    case eType_Document:
    {
      if (!mChannel) {
        // We could mFrameLoader->LoadURI(mURI), but UpdateObjectParameters
        // requires documents have a channel, so this is not a valid state.
        NS_NOTREACHED("Attempting to load a document without a channel");
        mType = eType_Null;
        break;
      }
      
      mFrameLoader = nsFrameLoader::Create(thisContent->AsElement(),
                                           mNetworkCreated);
      if (!mFrameLoader) {
        NS_NOTREACHED("nsFrameLoader::Create failed");
        mType = eType_Null;
        break;
      }
      
      rv = mFrameLoader->CheckForRecursiveLoad(mURI);
      if (NS_FAILED(rv)) {
        LOG(("OBJLC [%p]: Aborting recursive load", this));
        mFrameLoader->Destroy();
        mFrameLoader = nullptr;
        mType = eType_Null;
        break;
      }

      // We're loading a document, so we have to set LOAD_DOCUMENT_URI
      // (especially important for firing onload)
      nsLoadFlags flags = 0;
      mChannel->GetLoadFlags(&flags);
      flags |= nsIChannel::LOAD_DOCUMENT_URI;
      mChannel->SetLoadFlags(flags);

      nsCOMPtr<nsIDocShell> docShell;
      rv = mFrameLoader->GetDocShell(getter_AddRefs(docShell));
      if (NS_FAILED(rv)) {
        NS_NOTREACHED("Could not get DocShell from mFrameLoader?");
        mType = eType_Null;
        break;
      }

      nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(docShell));
      NS_ASSERTION(req, "Docshell must be an ifreq");

      nsCOMPtr<nsIURILoader>
        uriLoader(do_GetService(NS_URI_LOADER_CONTRACTID, &rv));
      if (NS_FAILED(rv)) {
        NS_NOTREACHED("Failed to get uriLoader service");
        mType = eType_Null;
        break;
      }
      rv = uriLoader->OpenChannel(mChannel, nsIURILoader::DONT_RETARGET, req,
                                  getter_AddRefs(finalListener));
      // finalListener will receive OnStartRequest below
    }
    break;
    case eType_Loading:
      // If our type remains Loading, we need a channel to proceed
      rv = OpenChannel();
      if (NS_FAILED(rv)) {
        LOG(("OBJLC [%p]: OpenChannel returned failure (%u)", this, rv));
      }
    break;
    case eType_Null:
      // Handled below, silence compiler warnings
    break;
  };

  //
  // Loaded, handle notifications and fallback
  //
  if (NS_FAILED(rv)) {
    // If we failed in the loading hunk above, switch to fallback
    LOG(("OBJLC [%p]: Loading failed, switching to fallback", this));
    mType = eType_Null;
  }

  // If we didn't load anything, handle switching to fallback state
  if (mType == eType_Null) {
    LOG(("OBJLC [%p]: Loading fallback, type %u", this, fallbackType));
    NS_ASSERTION(!mFrameLoader && !mInstanceOwner,
                 "switched to type null but also loaded something");

    if (mChannel) {
      // If we were loading with a channel but then failed over, throw it away
      CloseChannel();
    }

    // Don't try to initialize final listener below
    finalListener = nullptr;

    // Don't notify, as LoadFallback doesn't know of our previous state
    // (so really this is just setting mFallbackType)
    LoadFallback(fallbackType, false);
  }

  // Notify of our final state
  NotifyStateChanged(oldType, oldState, false, aNotify);

  //
  // Pass load on to finalListener if loading with a channel
  //

  if (!mIsLoading) {
    LOG(("OBJLC [%p]: Re-entered before dispatching to final listener", this));
  } else if (finalListener) {
    NS_ASSERTION(mType != eType_Null && mType != eType_Loading,
                 "We should not have a final listener with a non-loaded type");
    // Note that we always enter into LoadObject() from ::OnStartRequest when
    // loading with a channel.
    mSrcStreamLoading = true;
    // Remove blocker on entering into instantiate
    // (this is otherwise unset by the stack class)
    mIsLoading = false;
    mFinalListener = finalListener;
    rv = finalListener->OnStartRequest(mChannel, nullptr);
    mSrcStreamLoading = false;
    if (NS_FAILED(rv)) {
      // Failed to load new content, but since we've already notified of our
      // transition, we can just Unload and call LoadFallback (which will notify
      // again)
      mType = eType_Null;
      // This could *also* technically re-enter if OnStartRequest fails after
      // spawning a plugin.
      mIsLoading = true;
      UnloadObject(false);
      NS_ENSURE_TRUE(mIsLoading, NS_OK);
      CloseChannel();
      LoadFallback(fallbackType, true);
    }
  }

  return NS_OK;
}

// This call can re-enter when dealing with plugin listeners
nsresult
nsObjectLoadingContent::CloseChannel()
{
  if (mChannel) {
    LOG(("OBJLC [%p]: Closing channel\n", this));
    // Null the values before potentially-reentering, and ensure they survive
    // the call
    nsCOMPtr<nsIChannel> channelGrip(mChannel);
    nsCOMPtr<nsIStreamListener> listenerGrip(mFinalListener);
    mChannel = nullptr;
    mFinalListener = nullptr;
    channelGrip->Cancel(NS_BINDING_ABORTED);
    if (listenerGrip) {
      // mFinalListener is only set by LoadObject after OnStartRequest
      listenerGrip->OnStopRequest(channelGrip, nullptr, NS_BINDING_ABORTED);
    }
  }
  return NS_OK;
}

nsresult
nsObjectLoadingContent::OpenChannel()
{
  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");
  nsIDocument* doc = thisContent->OwnerDoc();
  NS_ASSERTION(doc, "No owner document?");
  NS_ASSERTION(!mInstanceOwner && !mInstantiating,
               "opening a new channel with already loaded content");

  nsresult rv;
  mChannel = nullptr;

  // E.g. mms://
  if (!mURI || !CanHandleURI(mURI)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsILoadGroup> group = doc->GetDocumentLoadGroup();
  nsCOMPtr<nsIChannel> chan;
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_OBJECT);
  }
  rv = NS_NewChannel(getter_AddRefs(chan), mURI, nullptr, group, this,
                     nsIChannel::LOAD_CALL_CONTENT_SNIFFERS |
                     nsIChannel::LOAD_CLASSIFY_URI,
                     channelPolicy);
  NS_ENSURE_SUCCESS(rv, rv);

  // Referrer
  nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(chan));
  if (httpChan) {
    httpChan->SetReferrer(doc->GetDocumentURI());
  }

  // Set up the channel's principal and such, like nsDocShell::DoURILoad does
  nsContentUtils::SetUpChannelOwner(thisContent->NodePrincipal(),
                                    chan, mURI, true);

  nsCOMPtr<nsIScriptChannel> scriptChannel = do_QueryInterface(chan);
  if (scriptChannel) {
    // Allow execution against our context if the principals match
    scriptChannel->SetExecutionPolicy(nsIScriptChannel::EXECUTE_NORMAL);
  }

  // AsyncOpen can fail if a file does not exist.
  rv = chan->AsyncOpen(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("OBJLC [%p]: Channel opened", this));
  mChannel = chan;
  return NS_OK;
}

uint32_t
nsObjectLoadingContent::GetCapabilities() const
{
  return eSupportImages |
         eSupportPlugins |
         eSupportDocuments |
         eSupportSVG;
}

void
nsObjectLoadingContent::DestroyContent()
{
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  StopPluginInstance();
}

/* static */
void
nsObjectLoadingContent::Traverse(nsObjectLoadingContent *tmp,
                                 nsCycleCollectionTraversalCallback &cb)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mFrameLoader");
  cb.NoteXPCOMChild(static_cast<nsIFrameLoader*>(tmp->mFrameLoader));
}

void
nsObjectLoadingContent::UnloadObject(bool aResetState)
{
  // Don't notify in CancelImageRequests until we transition to a new loaded
  // state
  CancelImageRequests(false);
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  if (aResetState) {
    if (mType != eType_Plugin) {
      // This can re-enter when dealing with plugins, and StopPluginInstance
      // will handle it
      CloseChannel();
    }
    mChannelLoaded = false;
    mType = eType_Loading;
    mURI = mOriginalURI = mBaseURI = nullptr;
    mContentType.Truncate();
    mOriginalContentType.Truncate();
  }

  mScriptRequested = false;

  // This call should be last as it may re-enter
  StopPluginInstance();
}

void
nsObjectLoadingContent::NotifyStateChanged(ObjectType aOldType,
                                           nsEventStates aOldState,
                                           bool aSync,
                                           bool aNotify)
{
  LOG(("OBJLC [%p]: Notifying about state change: (%u, %llx) -> (%u, %llx)"
       " (sync %i, notify %i)", this, aOldType, aOldState.GetInternalValue(),
       mType, ObjectState().GetInternalValue(), aSync, aNotify));

  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  NS_ASSERTION(thisContent->IsElement(), "Not an element?");

  // XXX(johns): A good bit of the code below replicates UpdateState(true)

  // Unfortunately, we do some state changes without notifying
  // (e.g. in Fallback when canceling image requests), so we have to
  // manually notify object state changes.
  thisContent->AsElement()->UpdateState(false);

  if (!aNotify) {
    // We're done here
    return;
  }

  nsIDocument* doc = thisContent->GetCurrentDoc();
  if (!doc) {
    return; // Nothing to do
  }

  nsEventStates newState = ObjectState();

  if (newState != aOldState) {
    // This will trigger frame construction
    NS_ASSERTION(InActiveDocument(thisContent), "Something is confused");
    nsEventStates changedBits = aOldState ^ newState;

    {
      nsAutoScriptBlocker scriptBlocker;
      doc->ContentStateChanged(thisContent, changedBits);
    }
    if (aSync) {
      // Make sure that frames are actually constructed immediately.
      doc->FlushPendingNotifications(Flush_Frames);
    }
  } else if (aOldType != mType) {
    // If our state changed, then we already recreated frames
    // Otherwise, need to do that here
    nsCOMPtr<nsIPresShell> shell = doc->GetShell();
    if (shell) {
      shell->RecreateFramesFor(thisContent);
    }
  }
}

nsObjectLoadingContent::ObjectType
nsObjectLoadingContent::GetTypeOfContent(const nsCString& aMIMEType)
{
  if (aMIMEType.IsEmpty()) {
    return eType_Null;
  }

  uint32_t caps = GetCapabilities();

  if ((caps & eSupportImages) && IsSupportedImage(aMIMEType)) {
    return eType_Image;
  }

  // SVGs load as documents, but are their own capability
  bool isSVG = aMIMEType.LowerCaseEqualsLiteral("image/svg+xml");
  Capabilities supportType = isSVG ? eSupportSVG : eSupportDocuments;
  if ((caps & supportType) && IsSupportedDocument(aMIMEType)) {
    return eType_Document;
  }

  if ((caps & eSupportPlugins) && NS_SUCCEEDED(IsPluginEnabledForType(aMIMEType))) {
    return eType_Plugin;
  }

  return eType_Null;
}

nsObjectFrame*
nsObjectLoadingContent::GetExistingFrame()
{
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsIFrame* frame = thisContent->GetPrimaryFrame();
  nsIObjectFrame* objFrame = do_QueryFrame(frame);
  return static_cast<nsObjectFrame*>(objFrame);
}

void
nsObjectLoadingContent::CreateStaticClone(nsObjectLoadingContent* aDest) const
{
  nsImageLoadingContent::CreateStaticImageClone(aDest);

  aDest->mType = mType;
  nsObjectLoadingContent* thisObj = const_cast<nsObjectLoadingContent*>(this);
  if (thisObj->mPrintFrame.IsAlive()) {
    aDest->mPrintFrame = thisObj->mPrintFrame;
  } else {
    aDest->mPrintFrame = const_cast<nsObjectLoadingContent*>(this)->GetExistingFrame();
  }

  if (mFrameLoader) {
    nsCOMPtr<nsIContent> content =
      do_QueryInterface(static_cast<nsIImageLoadingContent*>(aDest));
    nsFrameLoader* fl = nsFrameLoader::Create(content->AsElement(), false);
    if (fl) {
      aDest->mFrameLoader = fl;
      mFrameLoader->CreateStaticClone(fl);
    }
  }
}

NS_IMETHODIMP
nsObjectLoadingContent::GetPrintFrame(nsIFrame** aFrame)
{
  *aFrame = mPrintFrame.GetFrame();
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::PluginCrashed(nsIPluginTag* aPluginTag,
                                      const nsAString& pluginDumpID,
                                      const nsAString& browserDumpID,
                                      bool submittedCrashReport)
{
  LOG(("OBJLC [%p]: Plugin Crashed, queuing crash event", this));
  NS_ASSERTION(mType == eType_Plugin, "PluginCrashed at non-plugin type");

  // Instance is dead, clean up
  mInstanceOwner = nullptr;
  CloseChannel();

  // Switch to fallback/crashed state, notify
  LoadFallback(eFallbackCrashed, true);

  // send nsPluginCrashedEvent
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  // Note that aPluginTag in invalidated after we're called, so copy 
  // out any data we need now.
  nsAutoCString pluginName;
  aPluginTag->GetName(pluginName);
  nsAutoCString pluginFilename;
  aPluginTag->GetFilename(pluginFilename);

  nsCOMPtr<nsIRunnable> ev =
    new nsPluginCrashedEvent(thisContent,
                             pluginDumpID,
                             browserDumpID,
                             NS_ConvertUTF8toUTF16(pluginName),
                             NS_ConvertUTF8toUTF16(pluginFilename),
                             submittedCrashReport);
  nsresult rv = NS_DispatchToCurrentThread(ev);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch nsPluginCrashedEvent");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::ScriptRequestPluginInstance(bool aCallerIsContentJS,
                                                    nsNPAPIPluginInstance **aResult)
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  *aResult = nullptr;

  // The first time content script attempts to access placeholder content, fire
  // an event.  Fallback types >= eFallbackClickToPlay are plugin-replacement
  // types, see header.
  if (aCallerIsContentJS && !mScriptRequested &&
      InActiveDocument(thisContent) && mType == eType_Null &&
      mFallbackType >= eFallbackClickToPlay) {
    nsCOMPtr<nsIRunnable> ev =
      new nsSimplePluginEvent(thisContent,
                              NS_LITERAL_STRING("PluginScripted"));
    nsresult rv = NS_DispatchToCurrentThread(ev);
    if (NS_FAILED(rv)) {
      NS_NOTREACHED("failed to dispatch PluginScripted event");
    }
    mScriptRequested = true;
  } else if (mType == eType_Plugin && !mInstanceOwner &&
             nsContentUtils::IsSafeToRunScript() &&
             InActiveDocument(thisContent)) {
    // If we're configured as a plugin in an active document and it's safe to
    // run scripts right now, try spawning synchronously
    SyncStartPluginInstance();
  }

  if (mInstanceOwner) {
    return mInstanceOwner->GetInstance(aResult);
  }

  // Note that returning a null plugin is expected (and happens often)
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::SyncStartPluginInstance()
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Must be able to run script in order to instantiate a plugin instance!");

  // Don't even attempt to start an instance unless the content is in
  // the document and active
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  if (!InActiveDocument(thisContent)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> kungFuURIGrip(mURI);
  nsCString contentType(mContentType);
  return InstantiatePluginInstance();
}

NS_IMETHODIMP
nsObjectLoadingContent::AsyncStartPluginInstance()
{
  // OK to have an instance already.
  if (mInstanceOwner) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsIDocument* doc = thisContent->OwnerDoc();
  if (doc->IsStaticDocument() || doc->IsBeingUsedAsImage()) {
    return NS_OK;
  }

  // We always start plugins on a runnable.
  // We don't want a script blocker on the stack during instantiation.
  nsCOMPtr<nsIRunnable> event = new nsAsyncInstantiateEvent(this);
  if (!event) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = NS_DispatchToCurrentThread(event);
  if (NS_SUCCEEDED(rv)) {
    // Remember this event.  This is a weak reference that will be cleared
    // when the event runs.
    mPendingInstantiateEvent = event;
  }

  return rv;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetSrcURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

static bool
DoDelayedStop(nsPluginInstanceOwner* aInstanceOwner,
              nsObjectLoadingContent* aContent,
              bool aDelayedStop)
{
#if (MOZ_PLATFORM_MAEMO==5)
  // Don't delay stop on Maemo/Hildon (bug 530739).
  if (aDelayedStop && aInstanceOwner->MatchPluginName("Shockwave Flash"))
    return false;
#endif

  // Don't delay stopping QuickTime (bug 425157), Flip4Mac (bug 426524),
  // XStandard (bug 430219), CMISS Zinc (bug 429604).
  if (aDelayedStop
#if !(defined XP_WIN || defined MOZ_X11)
      && !aInstanceOwner->MatchPluginName("QuickTime")
      && !aInstanceOwner->MatchPluginName("Flip4Mac")
      && !aInstanceOwner->MatchPluginName("XStandard plugin")
      && !aInstanceOwner->MatchPluginName("CMISS Zinc Plugin")
#endif
      ) {
    nsCOMPtr<nsIRunnable> evt =
      new nsStopPluginRunnable(aInstanceOwner, aContent);
    NS_DispatchToCurrentThread(evt);
    return true;
  }
  return false;
}

void
nsObjectLoadingContent::LoadFallback(FallbackType aType, bool aNotify) {
  nsEventStates oldState = ObjectState();
  ObjectType oldType = mType;

  NS_ASSERTION(!mInstanceOwner && !mFrameLoader && !mChannel,
               "LoadFallback called with loaded content");

  //
  // Fixup mFallbackType
  //
  nsCOMPtr<nsIContent> thisContent =
  do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  if (!thisContent->IsHTML() || mContentType.IsEmpty()) {
    // Don't let custom fallback handlers run outside HTML, tags without a
    // determined type should always just be alternate content
    aType = eFallbackAlternate;
  }

  if (thisContent->Tag() == nsGkAtoms::object &&
      (aType == eFallbackUnsupported ||
       aType == eFallbackDisabled ||
       aType == eFallbackBlocklisted))
  {
    // Show alternate content instead, if it exists
    for (nsIContent* child = thisContent->GetFirstChild();
         child; child = child->GetNextSibling()) {
      if (!child->IsHTML(nsGkAtoms::param) &&
          nsStyleUtil::IsSignificantChild(child, true, false)) {
        aType = eFallbackAlternate;
        break;
      }
    }
  }

  mType = eType_Null;
  mFallbackType = aType;

  // Notify
  if (!aNotify) {
    return; // done
  }

  NotifyStateChanged(oldType, oldState, false, true);
}

void
nsObjectLoadingContent::DoStopPlugin(nsPluginInstanceOwner* aInstanceOwner,
                                     bool aDelayedStop,
                                     bool aForcedReentry)
{
  // DoStopPlugin can process events and there may be pending InDocCheckEvent
  // events which can drop in underneath us and destroy the instance we are
  // about to destroy unless we prevent that with the mPluginStopping flag.
  // (aForcedReentry is only true from the callback of an earlier delayed stop)
  if (mIsStopping && !aForcedReentry) {
    return;
  }
  mIsStopping = true;

  nsRefPtr<nsPluginInstanceOwner> kungFuDeathGrip(aInstanceOwner);
  nsRefPtr<nsNPAPIPluginInstance> inst;
  aInstanceOwner->GetInstance(getter_AddRefs(inst));
  if (inst) {
    if (DoDelayedStop(aInstanceOwner, this, aDelayedStop)) {
      return;
    }

#if defined(XP_MACOSX)
    aInstanceOwner->HidePluginWindow();
#endif

    nsRefPtr<nsPluginHost> pluginHost =
      already_AddRefed<nsPluginHost>(nsPluginHost::GetInst());
    NS_ASSERTION(pluginHost, "No plugin host?");
    pluginHost->StopPluginInstance(inst);
  }

  aInstanceOwner->Destroy();
  mIsStopping = false;
}

NS_IMETHODIMP
nsObjectLoadingContent::StopPluginInstance()
{
  // Prevents any pending plugin starts from running
  mPendingInstantiateEvent = nullptr;

  if (!mInstanceOwner) {
    return NS_OK;
  }

  if (mChannel) {
    // The plugin has already used data from this channel, we'll need to
    // re-open it to handle instantiating again, even if we don't invalidate
    // our loaded state.
    /// XXX(johns): Except currently, we don't, just leaving re-opening channels
    ///             to plugins...
    LOG(("OBJLC [%p]: StopPluginInstance - Closing used channel", this));
    CloseChannel();
  }

  DisconnectFrame();

  bool delayedStop = false;
#ifdef XP_WIN
  // Force delayed stop for Real plugin only; see bug 420886, 426852.
  nsRefPtr<nsNPAPIPluginInstance> inst;
  mInstanceOwner->GetInstance(getter_AddRefs(inst));
  if (inst) {
    const char* mime = nullptr;
    if (NS_SUCCEEDED(inst->GetMIMEType(&mime)) && mime) {
      if (strcmp(mime, "audio/x-pn-realaudio-plugin") == 0) {
        delayedStop = true;
      }
    }
  }
#endif

  DoStopPlugin(mInstanceOwner, delayedStop);

  mInstanceOwner = nullptr;

  return NS_OK;
}

void
nsObjectLoadingContent::NotifyContentObjectWrapper()
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  nsCOMPtr<nsIDocument> doc = thisContent->GetDocument();
  if (!doc)
    return;

  nsIScriptGlobalObject *sgo = doc->GetScopeObject();
  if (!sgo)
    return;

  nsIScriptContext *scx = sgo->GetContext();
  if (!scx)
    return;

  JSContext *cx = scx->GetNativeContext();

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsContentUtils::XPConnect()->
  GetWrappedNativeOfNativeObject(cx, sgo->GetGlobalJSObject(), thisContent,
                                 NS_GET_IID(nsISupports),
                                 getter_AddRefs(wrapper));

  if (!wrapper) {
    // Nothing to do here if there's no wrapper for mContent. The proto
    // chain will be fixed appropriately when the wrapper is created.
    return;
  }

  JSObject *obj = nullptr;
  nsresult rv = wrapper->GetJSObject(&obj);
  if (NS_FAILED(rv))
    return;

  nsHTMLPluginObjElementSH::SetupProtoChain(wrapper, cx, obj);
}

NS_IMETHODIMP
nsObjectLoadingContent::PlayPlugin()
{
  if (!nsContentUtils::IsCallerChrome())
    return NS_OK;

  if (!mActivated) {
    mActivated = true;
    LOG(("OBJLC [%p]: Activated by user", this));
  }

  // If we're in a click-to-play or play preview state, we need to reload
  // Fallback types >= eFallbackClickToPlay are plugin-replacement types, see
  // header
  if (mType == eType_Null && mFallbackType >= eFallbackClickToPlay) {
    return LoadObject(true, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetActivated(bool *aActivated)
{
  *aActivated = mActivated;
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetPluginFallbackType(uint32_t* aPluginFallbackType)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);
  *aPluginFallbackType = mFallbackType;
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::CancelPlayPreview()
{
  if (!nsContentUtils::IsCallerChrome())
    return NS_ERROR_NOT_AVAILABLE;

  mPlayPreviewCanceled = true;
  
  // If we're in play preview state already, reload
  if (mType == eType_Null && mFallbackType == eFallbackPlayPreview) {
    return LoadObject(true, true);
  }

  return NS_OK;
}

bool
nsObjectLoadingContent::ShouldPreview()
{
  if (mPlayPreviewCanceled || mActivated)
    return false;

  nsRefPtr<nsPluginHost> pluginHost =
    already_AddRefed<nsPluginHost>(nsPluginHost::GetInst());

  return pluginHost->IsPluginPlayPreviewForType(mContentType.get());
}

bool
nsObjectLoadingContent::ShouldPlay(FallbackType &aReason)
{
  // mActivated is true if we've been activated via PlayPlugin() (e.g. user has
  // clicked through). Otherwise, only play if click-to-play is off or if page
  // is whitelisted

  nsRefPtr<nsPluginHost> pluginHost =
    already_AddRefed<nsPluginHost>(nsPluginHost::GetInst());

  bool isCTP;
  nsresult rv = pluginHost->IsPluginClickToPlayForType(mContentType, &isCTP);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (!isCTP || mActivated) {
    return true;
  }

  // set the fallback reason
  aReason = eFallbackClickToPlay;
  // (if it's click-to-play, it might be because of the blocklist)
  uint32_t state;
  rv = pluginHost->GetBlocklistStateForType(mContentType.get(), &state);
  NS_ENSURE_SUCCESS(rv, false);
  if (state == nsIBlocklistService::STATE_VULNERABLE_UPDATE_AVAILABLE) {
    aReason = eFallbackVulnerableUpdatable;
  }
  else if (state == nsIBlocklistService::STATE_VULNERABLE_NO_UPDATE) {
    aReason = eFallbackVulnerableNoUpdate;
  }

  // If plugin type is click-to-play and we have not been explicitly clicked.
  // check if permissions lets this page bypass - (e.g. user selected 'Always
  // play plugins on this page')

  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIObjectLoadingContent*>(this));
  MOZ_ASSERT(thisContent);
  nsIDocument* ownerDoc = thisContent->OwnerDoc();

  nsCOMPtr<nsIDOMWindow> window = ownerDoc->GetWindow();
  if (!window) {
    return false;
  }
  nsCOMPtr<nsIDOMWindow> topWindow;
  rv = window->GetTop(getter_AddRefs(topWindow));
  NS_ENSURE_SUCCESS(rv, false);
  nsCOMPtr<nsIDOMDocument> topDocument;
  rv = topWindow->GetDocument(getter_AddRefs(topDocument));
  NS_ENSURE_SUCCESS(rv, false);
  nsCOMPtr<nsIDocument> topDoc = do_QueryInterface(topDocument);

  nsCOMPtr<nsIPermissionManager> permissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, false);

  bool allowPerm = false;
  // For now we always say that the system principal uses click-to-play since
  // that maintains current behavior and we have tests that expect this.
  // What we really should do is disable plugins entirely in pages that use
  // the system principal, i.e. in chrome pages. That way the click-to-play
  // code here wouldn't matter at all. Bug 775301 is tracking this.
  if (!nsContentUtils::IsSystemPrincipal(topDoc->NodePrincipal())) {
    nsAutoCString permissionString;
    rv = pluginHost->GetPermissionStringForType(mContentType, permissionString);
    NS_ENSURE_SUCCESS(rv, false);
    uint32_t permission;
    rv = permissionManager->TestPermissionFromPrincipal(topDoc->NodePrincipal(),
                                                        permissionString.Data(),
                                                        &permission);
    NS_ENSURE_SUCCESS(rv, false);
    allowPerm = permission == nsIPermissionManager::ALLOW_ACTION;
  }

  return allowPerm;
}


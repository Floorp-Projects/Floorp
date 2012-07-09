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
#include "imgILoader.h"
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

#include "nsPluginError.h"

// Util headers
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
static PRLogModuleInfo* gObjectLog = PR_NewLogModule("objlc");
#endif

#define LOG(args) PR_LOG(gObjectLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gObjectLog, PR_LOG_DEBUG)

#include "mozilla/Preferences.h"

static bool gClickToPlayPlugins = false;

static void
InitPrefCache()
{
  static bool initializedPrefCache = false;
  if (!initializedPrefCache) {
    mozilla::Preferences::AddBoolVarCache(&gClickToPlayPlugins, "plugins.click_to_play");
  }
  initializedPrefCache = true;
}

class nsAsyncInstantiateEvent : public nsRunnable {
public:
  nsObjectLoadingContent *mContent;
  nsAsyncInstantiateEvent(nsObjectLoadingContent* aContent)
  : mContent(aContent)
  {
    static_cast<nsIObjectLoadingContent *>(mContent)->AddRef();
  }

  ~nsAsyncInstantiateEvent()
  {
    static_cast<nsIObjectLoadingContent *>(mContent)->Release();
  }

  NS_IMETHOD Run();
};

NS_IMETHODIMP
nsAsyncInstantiateEvent::Run()
{
  // do nothing if we've been revoked
  if (mContent->mPendingInstantiateEvent != this) {
    return NS_OK;
  }
  mContent->mPendingInstantiateEvent = nsnull;

  return mContent->SyncStartPluginInstance();
}

// Checks to see if the content for a plugin instance has a parent.
// The plugin instance is stopped if there is no parent.
class InDocCheckEvent : public nsRunnable {
public:
  nsCOMPtr<nsIContent> mContent;

  InDocCheckEvent(nsIContent* aContent)
  : mContent(aContent)
  {
  }

  ~InDocCheckEvent()
  {
  }

  NS_IMETHOD Run();
};

NS_IMETHODIMP
InDocCheckEvent::Run()
{
  if (!mContent->IsInDoc()) {
    nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(mContent);
    if (olc) {
      olc->StopPluginInstance();
    }
  }
  return NS_OK;
}

/**
 * A task for firing PluginNotFound and PluginBlocklisted DOM Events.
 */
class nsPluginErrorEvent : public nsRunnable {
public:
  nsCOMPtr<nsIContent> mContent;
  PluginSupportState mState;

  nsPluginErrorEvent(nsIContent* aContent, PluginSupportState aState)
    : mContent(aContent),
      mState(aState)
  {}

  ~nsPluginErrorEvent() {}

  NS_IMETHOD Run();
};

NS_IMETHODIMP
nsPluginErrorEvent::Run()
{
  LOG(("OBJLC []: Firing plugin not found event for content %p\n",
       mContent.get()));
  nsString type;
  switch (mState) {
    case ePluginClickToPlay:
      type = NS_LITERAL_STRING("PluginClickToPlay");
      break;
    case ePluginUnsupported:
      type = NS_LITERAL_STRING("PluginNotFound");
      break;
    case ePluginDisabled:
      type = NS_LITERAL_STRING("PluginDisabled");
      break;
    case ePluginBlocklisted:
      type = NS_LITERAL_STRING("PluginBlocklisted");
      break;
    case ePluginOutdated:
      type = NS_LITERAL_STRING("PluginOutdated");
      break;
    default:
      return NS_OK;
  }
  nsContentUtils::DispatchTrustedEvent(mContent->GetDocument(), mContent,
                                       type, true, true);

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
  LOG(("OBJLC []: Firing plugin crashed event for content %p\n",
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

  nsEventDispatcher::DispatchDOMEvent(mContent, nsnull, event, nsnull, nsnull);
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
    PRUint32 currentLevel = 0;
    appShell->GetEventloopNestingLevel(&currentLevel);
    if (currentLevel > mInstanceOwner->GetLastEventloopNestingLevel()) {
      if (!mTimer)
        mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (mTimer) {
        // Fire 100ms timer to try to tear down this plugin as quickly as
        // possible once the nesting level comes back down.
        nsresult rv = mTimer->InitWithCallback(this, 100, nsITimer::TYPE_ONE_SHOT);
        if (NS_SUCCEEDED(rv)) {
          return rv;
        }
      }
      NS_ERROR("Failed to setup a timer to stop the plugin later (at a safe "
               "time). Stopping the plugin now, this might crash.");
    }
  }

  mTimer = nsnull;

  static_cast<nsObjectLoadingContent*>(mContent.get())->
    DoStopPlugin(mInstanceOwner, false, true);

  return NS_OK;
}

class AutoNotifier {
  public:
    AutoNotifier(nsObjectLoadingContent* aContent, bool aNotify) :
      mContent(aContent), mNotify(aNotify) {
        mOldType = aContent->Type();
        mOldState = aContent->ObjectState();
    }
    ~AutoNotifier() {
      mContent->NotifyStateChanged(mOldType, mOldState, false, mNotify);
    }

    /**
     * Send notifications now, ignoring the value of mNotify. The new type and
     * state is saved, and the destructor will notify again if mNotify is true
     * and the values changed.
     */
    void Notify() {
      NS_ASSERTION(mNotify, "Should not notify when notify=false");

      mContent->NotifyStateChanged(mOldType, mOldState, true, true);
      mOldType = mContent->Type();
      mOldState = mContent->ObjectState();
    }

  private:
    nsObjectLoadingContent*            mContent;
    bool                               mNotify;
    nsObjectLoadingContent::ObjectType mOldType;
    nsEventStates                      mOldState;
};

/**
 * A class that will automatically fall back if a |rv| variable has a failure
 * code when this class is destroyed. It does not notify.
 */
class AutoFallback {
  public:
    AutoFallback(nsObjectLoadingContent* aContent, const nsresult* rv)
      : mContent(aContent), mResult(rv), mPluginState(ePluginOtherState) {}
    ~AutoFallback() {
      if (NS_FAILED(*mResult)) {
        LOG(("OBJLC [%p]: rv=%08x, falling back\n", mContent, *mResult));
        mContent->Fallback(false);
        if (mPluginState != ePluginOtherState) {
          mContent->mFallbackReason = mPluginState;
        }
      }
    }

    /**
     * This should be set to something other than ePluginOtherState to indicate
     * a specific failure that should be passed on.
     */
     void SetPluginState(PluginSupportState aState) {
       NS_ASSERTION(aState != ePluginOtherState, "Should not be setting ePluginOtherState");
       mPluginState = aState;
     }
  private:
    nsObjectLoadingContent* mContent;
    const nsresult* mResult;
    PluginSupportState mPluginState;
};

/**
 * A class that automatically sets mInstantiating to false when it goes
 * out of scope.
 */
class AutoSetInstantiatingToFalse {
  public:
    AutoSetInstantiatingToFalse(nsObjectLoadingContent* objlc) : mContent(objlc) {}
    ~AutoSetInstantiatingToFalse() { mContent->mInstantiating = false; }
  private:
    nsObjectLoadingContent* mContent;
};

// helper functions
static bool
IsSupportedImage(const nsCString& aMimeType)
{
  imgILoader* loader = nsContentUtils::GetImgLoader();
  if (!loader) {
    return false;
  }

  bool supported;
  nsresult rv = loader->SupportImageWithMimeType(aMimeType.get(), &supported);
  return NS_SUCCEEDED(rv) && supported;
}

nsresult nsObjectLoadingContent::IsPluginEnabledForType(const nsCString& aMIMEType)
{
  nsCOMPtr<nsIPluginHost> pluginHostCOM(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID));
  nsPluginHost *pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if (!pluginHost) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = pluginHost->IsPluginEnabledForType(aMIMEType.get());

  // Check to see if the plugin is disabled before deciding if it
  // should be in the "click to play" state, since we only want to
  // display "click to play" UI for enabled plugins.
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mShouldPlay) {
    nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIObjectLoadingContent*>(this));
    MOZ_ASSERT(thisContent);
    nsIDocument* ownerDoc = thisContent->OwnerDoc();

    nsCOMPtr<nsIDOMWindow> window = ownerDoc->GetWindow();
    if (!window) {
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIDOMWindow> topWindow;
    rv = window->GetTop(getter_AddRefs(topWindow));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDOMDocument> topDocument;
    rv = topWindow->GetDocument(getter_AddRefs(topDocument));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> topDoc = do_QueryInterface(topDocument);
    nsIURI* topUri = topDoc->GetDocumentURI();

    nsCOMPtr<nsIPermissionManager> permissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint32 permission;
    rv = permissionManager->TestPermission(topUri,
                                           "plugins",
                                           &permission);
    NS_ENSURE_SUCCESS(rv, rv);
    if (permission == nsIPermissionManager::ALLOW_ACTION) {
      mShouldPlay = true;
    } else {
      return NS_ERROR_PLUGIN_CLICKTOPLAY;
    }
  }

  return NS_OK;
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

    PRInt32 offset = spec.RFindChar('.');
    if (offset != kNotFound) {
      ext = Substring(spec, offset + 1, spec.Length());
    }
  }
}

/**
 * Checks whether a plugin exists and is enabled for the extension
 * in the given URI. The MIME type is returned in the mimeType out parameter.
 */
bool nsObjectLoadingContent::IsPluginEnabledByExtension(nsIURI* uri, nsCString& mimeType)
{
  if (!mShouldPlay) {
    return false;
  }

  nsCAutoString ext;
  GetExtensionFromURI(uri, ext);

  if (ext.IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsIPluginHost> pluginHostCOM(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID));
  nsPluginHost *pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if (!pluginHost) {
    return false;
  }

  const char* typeFromExt;
  if (NS_SUCCEEDED(pluginHost->IsPluginEnabledForExtension(ext.get(), typeFromExt))) {
    mimeType = typeFromExt;
    return true;
  }
  return false;
}

nsresult
nsObjectLoadingContent::BindToTree(nsIDocument* aDocument, nsIContent* /*aParent*/,
                                   nsIContent* /*aBindingParent*/,
                                   bool /*aCompileEventHandlers*/)
{
  if (aDocument) {
    return aDocument->AddPlugin(this);
  }
  return NS_OK;
}

void
nsObjectLoadingContent::UnbindFromTree(bool /*aDeep*/, bool /*aNullParent*/)
{
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIObjectLoadingContent*>(this));
  MOZ_ASSERT(thisContent);
  nsIDocument* ownerDoc = thisContent->OwnerDoc();
  ownerDoc->RemovePlugin(this);
}

nsObjectLoadingContent::nsObjectLoadingContent()
  : mPendingInstantiateEvent(nsnull)
  , mChannel(nsnull)
  , mType(eType_Loading)
  , mInstantiating(false)
  , mUserDisabled(false)
  , mSuppressed(false)
  , mNetworkCreated(true)
  , mIsStopping(false)
  , mSrcStreamLoading(false)
  , mFallbackReason(ePluginOtherState)
{
  InitPrefCache();
  // If plugins.click_to_play is false, plugins should always play
  mShouldPlay = !gClickToPlayPlugins;
  // If plugins.click_to_play is true, track the activated state of plugins.
  mActivated = !gClickToPlayPlugins;
}

nsObjectLoadingContent::~nsObjectLoadingContent()
{
  DestroyImageLoadingContent();
  if (mFrameLoader) {
    mFrameLoader->Destroy();
  }
}

nsresult
nsObjectLoadingContent::InstantiatePluginInstance(const char* aMimeType, nsIURI* aURI)
{
  if (!mShouldPlay) {
    return NS_ERROR_PLUGIN_CLICKTOPLAY;
  }

  // Don't do anything if we already have an active instance.
  if (mInstanceOwner) {
    return NS_OK;
  }

  // Don't allow re-entry into initialization code.
  if (mInstantiating) {
    return NS_OK;
  }
  mInstantiating = true;
  AutoSetInstantiatingToFalse autoInstantiating(this);

  // Instantiating an instance can result in script execution, which
  // can destroy this DOM object. Don't allow that for the scope
  // of this method.
  nsCOMPtr<nsIObjectLoadingContent> kungFuDeathGrip = this;
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  nsCOMPtr<nsIURI> baseURI;
  if (!aURI) {
    // We need some URI. If we have nothing else, use the base URI.
    // XXX(biesi): The code used to do this. Not sure why this is correct...
    GetObjectBaseURI(thisContent, getter_AddRefs(baseURI));
    aURI = baseURI;
  }

  // Flush layout so that the plugin is initialized with the latest information.
  nsIDocument* doc = thisContent->GetCurrentDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }
  if (!doc->IsActive()) {
    NS_ERROR("Shouldn't be calling InstantiatePluginInstance in an inactive document");
    return NS_ERROR_FAILURE;
  }
  doc->FlushPendingNotifications(Flush_Layout);

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPluginHost> pluginHostCOM(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID, &rv));
  nsPluginHost* pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if (NS_FAILED(rv)) {
    return rv;
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
    rv = pluginHost->InstantiateFullPagePluginInstance(aMimeType, aURI, this,
                                                       getter_AddRefs(mInstanceOwner), getter_AddRefs(stream));
    if (NS_SUCCEEDED(rv)) {
      pDoc->SetStreamListener(stream);
    }
  } else {
    rv = pluginHost->InstantiateEmbeddedPluginInstance(aMimeType, aURI, this,
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
    pluginHost->GetPluginTagForInstance(pluginInstance, getter_AddRefs(pluginTag));
    
    nsCOMPtr<nsIBlocklistService> blocklist =
    do_GetService("@mozilla.org/extensions/blocklist;1");
    if (blocklist) {
      PRUint32 blockState = nsIBlocklistService::STATE_NOT_BLOCKED;
      blocklist->GetPluginBlocklistState(pluginTag, EmptyString(),
                                         EmptyString(), &blockState);
      if (blockState == nsIBlocklistService::STATE_OUTDATED)
        FirePluginError(thisContent, ePluginOutdated);
    }
  }

  mActivated = true;
  return NS_OK;
}

void
nsObjectLoadingContent::NotifyOwnerDocumentActivityChanged()
{
  if (!mInstanceOwner) {
    return;
  }

  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
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
  SAMPLE_LABEL("nsObjectLoadingContent", "OnStartRequest");

  if (aRequest != mChannel || !aRequest) {
    // This is a bit of an edge case - happens when a new load starts before the
    // previous one got here
    return NS_BINDING_ABORTED;
  }

  AutoNotifier notifier(this, true);

  if (!IsSuccessfulRequest(aRequest)) {
    LOG(("OBJLC [%p]: OnStartRequest: Request failed\n", this));
    Fallback(false);
    return NS_BINDING_ABORTED;
  }

  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  NS_ASSERTION(chan, "Why is our request not a channel?");

  nsresult rv = NS_ERROR_UNEXPECTED;
  // This fallback variable MUST be declared after the notifier variable. Do NOT
  // change the order of the declarations!
  AutoFallback fallback(this, &rv);

  nsCString channelType;
  rv = chan->GetContentType(channelType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (channelType.EqualsASCII(APPLICATION_GUESS_FROM_EXT)) {
    channelType = APPLICATION_OCTET_STREAM;
    chan->SetContentType(channelType);
  }

  // We want to ignore the channel type if one of the following is true:
  //
  // 1) The channel type is application/octet-stream or binary/octet-stream 
  //    and we have a type hint (in mContentType) and the type hint is not a
  //    document type.
  // 2) Our type hint is a type that we support with a plugin
  //    (where "support" means it is enabled or it is click-to-play)
  //    and this object loading content has the capability to load a plugin.
  //    We have to be careful here - there might be a plugin that supports
  //    image types, so make sure the type of the content is not an image.
  bool isOctetStream = (channelType.EqualsASCII(APPLICATION_OCTET_STREAM) ||
                        channelType.EqualsASCII(BINARY_OCTET_STREAM));
  ObjectType typeOfContent = GetTypeOfContent(mContentType);
  bool caseOne = (isOctetStream &&
                  !mContentType.IsEmpty() &&
                  typeOfContent != eType_Document);
  nsresult pluginState = IsPluginEnabledForType(mContentType);
  bool pluginSupported = (NS_SUCCEEDED(pluginState) || 
                          pluginState == NS_ERROR_PLUGIN_CLICKTOPLAY);
  PRUint32 caps = GetCapabilities();
  bool caseTwo = (pluginSupported && 
                  (caps & eSupportPlugins) &&
                  typeOfContent != eType_Image &&
                  typeOfContent != eType_Document);
  if (caseOne || caseTwo) {
    // Set the type we'll use for dispatch on the channel.  Otherwise we could
    // end up trying to dispatch to a nsFrameLoader, which will complain that
    // it couldn't find a way to handle application/octet-stream
    nsCAutoString typeHint, dummy;
    NS_ParseContentType(mContentType, typeHint, dummy);
    if (!typeHint.IsEmpty()) {
      chan->SetContentType(typeHint);
    }
  } else {
    mContentType = channelType;
  }

  nsCOMPtr<nsIURI> uri;
  chan->GetURI(getter_AddRefs(uri));
  if (!uri) {
    return NS_ERROR_FAILURE;
  }

  if (mContentType.EqualsASCII(APPLICATION_OCTET_STREAM) ||
      mContentType.EqualsASCII(BINARY_OCTET_STREAM)) {
    nsCAutoString extType;
    if (IsPluginEnabledByExtension(uri, extType)) {
      mContentType = extType;
      chan->SetContentType(extType);
    }
  }

  // Now find out what type the content is
  // UnloadContent will set our type to null; need to be sure to only set it to
  // the real value on success
  ObjectType newType = GetTypeOfContent(mContentType);
  LOG(("OBJLC [%p]: OnStartRequest: Content Type=<%s> Old type=%u New Type=%u\n",
       this, mContentType.get(), mType, newType));

  // Now do a content policy check
  // XXXbz this duplicates some code in nsContentBlocker::ShouldLoad  
  PRInt32 contentPolicyType;
  switch (newType) {
    case eType_Image:
      contentPolicyType = nsIContentPolicy::TYPE_IMAGE;
      break;
    case eType_Document:
      contentPolicyType = nsIContentPolicy::TYPE_SUBDOCUMENT;
      break;
    default:
      contentPolicyType = nsIContentPolicy::TYPE_OBJECT;
      break;
  }
  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  nsIDocument* doc = thisContent->OwnerDoc();

  PRInt16 shouldProcess = nsIContentPolicy::ACCEPT;
  rv =
    NS_CheckContentProcessPolicy(contentPolicyType,
                                 uri,
                                 doc->NodePrincipal(),
                                 static_cast<nsIImageLoadingContent*>(this),
                                 mContentType,
                                 nsnull, //extra
                                 &shouldProcess,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldProcess)) {
    HandleBeingBlockedByContentPolicy(rv, shouldProcess);
    rv = NS_OK; // otherwise, the AutoFallback will make us fall back
    return NS_BINDING_ABORTED;
  }  
  
  if (mType != newType) {
    UnloadContent();
  }

  switch (newType) {
    case eType_Image:
      rv = LoadImageWithChannel(chan, getter_AddRefs(mFinalListener));
      NS_ENSURE_SUCCESS(rv, rv);

      // If we have a success result but no final listener, then the image is
      // cached. In that case, we can just return: No need to try to call the
      // final listener.
      if (!mFinalListener) {
        mType = newType;
        return NS_BINDING_ABORTED;
      }
      break;
    case eType_Document: {
      if (!mFrameLoader) {
        mFrameLoader = nsFrameLoader::Create(thisContent->AsElement(),
                                             mNetworkCreated);
        if (!mFrameLoader) {
          Fallback(false);
          return NS_ERROR_UNEXPECTED;
        }
      }

      rv = mFrameLoader->CheckForRecursiveLoad(uri);
      if (NS_FAILED(rv)) {
        Fallback(false);
        return rv;
      }

      if (mType != newType) {
        // XXX We must call this before getting the docshell to work around
        // bug 300540; when that's fixed, this if statement can be removed.
        mType = newType;
        notifier.Notify();

        if (!mFrameLoader) {
          // mFrameLoader got nulled out when we notified, which most
          // likely means the node was removed from the
          // document. Abort the load that just started.
          return NS_BINDING_ABORTED;
        }
      }

      // We're loading a document, so we have to set LOAD_DOCUMENT_URI
      // (especially important for firing onload)
      nsLoadFlags flags = 0;
      chan->GetLoadFlags(&flags);
      flags |= nsIChannel::LOAD_DOCUMENT_URI;
      chan->SetLoadFlags(flags);

      nsCOMPtr<nsIDocShell> docShell;
      rv = mFrameLoader->GetDocShell(getter_AddRefs(docShell));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(docShell));
      NS_ASSERTION(req, "Docshell must be an ifreq");

      nsCOMPtr<nsIURILoader>
        uriLoader(do_GetService(NS_URI_LOADER_CONTRACTID, &rv));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = uriLoader->OpenChannel(chan, nsIURILoader::DONT_RETARGET, req,
                                  getter_AddRefs(mFinalListener));
      break;
    }
    case eType_Plugin: {
      if (mType != newType) {
        mType = newType;
        notifier.Notify();
      }
      nsCOMPtr<nsIPluginHost> pluginHostCOM(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID));
      nsPluginHost *pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
      if (!pluginHost) {
        return NS_ERROR_NOT_AVAILABLE;
      }
      pluginHost->NewEmbeddedPluginStreamListener(uri, this, nsnull,
                                                  getter_AddRefs(mFinalListener));
      break;
    }
    case eType_Loading:
      NS_NOTREACHED("Should not have a loading type here!");
    case eType_Null:
      // Need to fallback here (instead of using the case below), so that we can
      // set mFallbackReason without it being overwritten. This is also why we
      // return early.
      Fallback(false);

      PluginSupportState pluginState = GetPluginSupportState(thisContent,
                                                             mContentType);
      // Do nothing, but fire the plugin not found event if needed
      if (pluginState != ePluginOtherState) {
        mFallbackReason = pluginState;
        FirePluginError(thisContent, pluginState);
      }
      return NS_BINDING_ABORTED;
  }

  if (mFinalListener) {
    mType = newType;

    mSrcStreamLoading = true;
    rv = mFinalListener->OnStartRequest(aRequest, aContext);
    mSrcStreamLoading = false;

    if (NS_SUCCEEDED(rv)) {
      // Plugins need to set up for NPRuntime.
      if (mType == eType_Plugin) {
        NotifyContentObjectWrapper();
      }
    } else {
      // Plugins don't fall back if there is an error here.
      if (mType == eType_Plugin) {
        rv = NS_OK; // this is necessary to avoid auto-fallback
        return NS_BINDING_ABORTED;
      }
      Fallback(false);
    }

    return rv;
  }

  Fallback(false);
  return NS_BINDING_ABORTED;
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

  mChannel = nsnull;

  if (mFinalListener) {
    mFinalListener->OnStopRequest(aRequest, aContext, aStatusCode);
    mFinalListener = nsnull;
  }

  // Return value doesn't matter
  return NS_OK;
}


// nsIStreamListener
NS_IMETHODIMP
nsObjectLoadingContent::OnDataAvailable(nsIRequest *aRequest,
                                        nsISupports *aContext,
                                        nsIInputStream *aInputStream,
                                        PRUint32 aOffset, PRUint32 aCount)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);

  if (aRequest != mChannel) {
    return NS_BINDING_ABORTED;
  }

  if (mFinalListener) {
    return mFinalListener->OnDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount);
  }

  // Abort this load if we have no listener here
  return NS_ERROR_UNEXPECTED;
}

// nsIFrameLoaderOwner
NS_IMETHODIMP
nsObjectLoadingContent::GetFrameLoader(nsIFrameLoader** aFrameLoader)
{
  *aFrameLoader = mFrameLoader;
  NS_IF_ADDREF(*aFrameLoader);
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

// nsIObjectLoadingContent
NS_IMETHODIMP
nsObjectLoadingContent::GetActualType(nsACString& aType)
{
  aType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetDisplayedType(PRUint32* aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::HasNewFrame(nsIObjectFrame* aFrame)
{
  // Not having an instance yet is OK, but try to start one now that
  // we have a frame.
  if (!mInstanceOwner) {
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
  objFrame->Invalidate(objFrame->GetContentRectRelativeToSelf());

  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::DisconnectFrame()
{
  if (mInstanceOwner) {
    mInstanceOwner->SetFrame(nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetPluginInstance(nsNPAPIPluginInstance** aInstance)
{
  *aInstance = nsnull;

  if (!mInstanceOwner) {
    return NS_OK;
  }

  return mInstanceOwner->GetInstance(aInstance);
}

NS_IMETHODIMP
nsObjectLoadingContent::GetContentTypeForMIMEType(const nsACString& aMIMEType,
                                                  PRUint32* aType)
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
                                               PRUint32 aFlags,
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
      if (mSuppressed)
        return NS_EVENT_STATE_SUPPRESSED;
      if (mUserDisabled)
        return NS_EVENT_STATE_USERDISABLED;

      // Otherwise, broken
      nsEventStates state = NS_EVENT_STATE_BROKEN;
      switch (mFallbackReason) {
        case ePluginClickToPlay:
          return NS_EVENT_STATE_TYPE_CLICK_TO_PLAY;
        case ePluginDisabled:
          state |= NS_EVENT_STATE_HANDLER_DISABLED;
          break;
        case ePluginBlocklisted:
          state |= NS_EVENT_STATE_HANDLER_BLOCKED;
          break;
        case ePluginCrashed:
          state |= NS_EVENT_STATE_HANDLER_CRASHED;
          break;
        case ePluginUnsupported:
          state |= NS_EVENT_STATE_TYPE_UNSUPPORTED;
          break;
        case ePluginOutdated:
        case ePluginOtherState:
          // Do nothing, but avoid a compile warning
          break;
      }
      return state;
  };
  NS_NOTREACHED("unknown type?");
  // this return statement only exists to avoid a compile warning
  return nsEventStates();
}

// <protected>
nsresult
nsObjectLoadingContent::LoadObject(const nsAString& aURI,
                                   bool aNotify,
                                   const nsCString& aTypeHint,
                                   bool aForceLoad)
{
  LOG(("OBJLC [%p]: Loading object: URI string=<%s> notify=%i type=<%s> forceload=%i\n",
       this, NS_ConvertUTF16toUTF8(aURI).get(), aNotify, aTypeHint.get(), aForceLoad));

  // Avoid StringToURI in order to use the codebase attribute as base URI
  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  nsIDocument* doc = thisContent->OwnerDoc();
  nsCOMPtr<nsIURI> baseURI;
  GetObjectBaseURI(thisContent, getter_AddRefs(baseURI));

  nsCOMPtr<nsIURI> uri;
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                            aURI, doc,
                                            baseURI);
  // If URI creation failed, fallback immediately - this only happens for
  // malformed URIs
  if (!uri) {
    Fallback(aNotify);
    return NS_OK;
  }

  NS_TryToSetImmutable(uri);

  return LoadObject(uri, aNotify, aTypeHint, aForceLoad);
}

void
nsObjectLoadingContent::UpdateFallbackState(nsIContent* aContent,
                                            AutoFallback& fallback,
                                            const nsCString& aTypeHint)
{
  // Notify the UI and update the fallback state
  PluginSupportState state = GetPluginSupportState(aContent, aTypeHint);
  if (state != ePluginOtherState) {
    fallback.SetPluginState(state);
    FirePluginError(aContent, state);
  }
}

nsresult
nsObjectLoadingContent::LoadObject(nsIURI* aURI,
                                   bool aNotify,
                                   const nsCString& aTypeHint,
                                   bool aForceLoad)
{
  // Only do a URI equality check for things that aren't stopped plugins.
  // This is because we still need to load again if the plugin has been stopped.
  if (mType == eType_Document || mType == eType_Image || mInstanceOwner) {
    if (mURI && aURI) {
      bool equal;
      nsresult rv = mURI->Equals(aURI, &equal);
      if (NS_SUCCEEDED(rv) && equal && !aForceLoad) {
        // URI didn't change, do nothing
        return NS_OK;
      }
      StopPluginInstance();
    }
  }

  // Need to revoke any potentially pending instantiate events
  if (mType == eType_Plugin && mPendingInstantiateEvent) {
    mPendingInstantiateEvent = nsnull;
  }

  AutoNotifier notifier(this, aNotify);

  mUserDisabled = mSuppressed = false;

  mURI = aURI;
  mContentType = aTypeHint;

  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  nsIDocument* doc = thisContent->OwnerDoc();
  if (doc->IsBeingUsedAsImage()) {
    return NS_OK;
  }

  // From here on, we will always change the content. This means that a
  // possibly-loading channel should be aborted.
  if (mChannel) {
    LOG(("OBJLC [%p]: Cancelling existing load\n", this));

    // These three statements are carefully ordered:
    // - onStopRequest should get a channel whose status is the same as the
    //   status argument
    // - onStopRequest must get a non-null channel
    mChannel->Cancel(NS_BINDING_ABORTED);
    if (mFinalListener) {
      // NOTE: Since mFinalListener is only set in onStartRequest, which takes
      // care of calling mFinalListener->OnStartRequest, mFinalListener is only
      // non-null here if onStartRequest was already called.
      mFinalListener->OnStopRequest(mChannel, nsnull, NS_BINDING_ABORTED);
      mFinalListener = nsnull;
    }
    mChannel = nsnull;
  }

  // Security checks
  if (doc->IsLoadedAsData()) {
    if (!doc->IsStaticDocument()) {
      Fallback(false);
    }
    return NS_OK;
  }

  // Can't do security checks without a URI - hopefully the plugin will take
  // care of that
  // Null URIs happen when the URL to load is specified via other means than the
  // data/src attribute, for example via custom <param> elements.
  if (aURI) {
    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(secMan, "No security manager!?");
    nsresult rv =
      secMan->CheckLoadURIWithPrincipal(thisContent->NodePrincipal(), aURI, 0);
    if (NS_FAILED(rv)) {
      Fallback(false);
      return NS_OK;
    }

    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT; // default permit
    rv =
      NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OBJECT,
                                aURI,
                                doc->NodePrincipal(),
                                static_cast<nsIImageLoadingContent*>(this),
                                aTypeHint,
                                nsnull, //extra
                                &shouldLoad,
                                nsContentUtils::GetContentPolicy(),
                                secMan);
    if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
      HandleBeingBlockedByContentPolicy(rv, shouldLoad);
      return NS_OK;
    }
  }

  nsresult rv = NS_ERROR_UNEXPECTED;
  // This fallback variable MUST be declared after the notifier variable. Do NOT
  // change the order of the declarations!
  AutoFallback fallback(this, &rv);

  PRUint32 caps = GetCapabilities();
  LOG(("OBJLC [%p]: Capabilities: %04x\n", this, caps));

  nsCAutoString overrideType;
  if ((caps & eOverrideServerType) &&
      ((!aTypeHint.IsEmpty() && NS_SUCCEEDED(IsPluginEnabledForType(aTypeHint))) ||
       (aURI && IsPluginEnabledByExtension(aURI, overrideType)))) {
    ObjectType newType;
    if (overrideType.IsEmpty()) {
      newType = GetTypeOfContent(aTypeHint);
    } else {
      mContentType = overrideType;
      newType = eType_Plugin;
    }

    if (newType != mType) {
      LOG(("OBJLC [%p]: (eOverrideServerType) Changing type from %u to %u\n", this, mType, newType));

      UnloadContent();

      // Must have a frameloader before creating a frame, or the frame will
      // create its own.
      if (!mFrameLoader && newType == eType_Document) {
        mFrameLoader = nsFrameLoader::Create(thisContent->AsElement(),
                                             mNetworkCreated);
        if (!mFrameLoader) {
          mURI = nsnull;
          return NS_OK;
        }
      }

      // Must notify here for plugins
      // If aNotify is false, we'll just wait until we get a frame and use the
      // async instantiate path.
      // XXX is this still needed? (for documents?)
      mType = newType;
      if (aNotify)
        notifier.Notify();
    }
    switch (newType) {
      case eType_Image:
        // Don't notify, because we will take care of that ourselves.
        if (aURI) {
          rv = LoadImage(aURI, aForceLoad, false);
        } else {
          rv = NS_ERROR_NOT_AVAILABLE;
        }
        break;
      case eType_Plugin:
        rv = AsyncStartPluginInstance();
        break;
      case eType_Document:
        if (aURI) {
          rv = mFrameLoader->LoadURI(aURI);
        } else {
          rv = NS_ERROR_NOT_AVAILABLE;
        }
        break;
      case eType_Loading:
        NS_NOTREACHED("Should not have a loading type here!");
      case eType_Null:
        // No need to load anything, notify of the failure.
        UpdateFallbackState(thisContent, fallback, aTypeHint);
        break;
    };
    return NS_OK;
  }

  // If the class ID specifies a supported plugin, or if we have no explicit URI
  // but a type, immediately instantiate the plugin.
  bool isSupportedClassID = false;
  nsCAutoString typeForID; // Will be set iff isSupportedClassID == true
  bool hasID = false;
  if (caps & eSupportClassID) {
    nsAutoString classid;
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::classid, classid);
    if (!classid.IsEmpty()) {
      hasID = true;
      isSupportedClassID = NS_SUCCEEDED(TypeForClassID(classid, typeForID));
    }
  }

  if (hasID && !isSupportedClassID) {
    // We have a class ID and it's unsupported.  Fallback in that case.
    rv = NS_ERROR_NOT_AVAILABLE;
    return NS_OK;
  }

  if (isSupportedClassID ||
      (!aURI && !aTypeHint.IsEmpty() &&
       GetTypeOfContent(aTypeHint) == eType_Plugin)) {
    // No URI, but we have a type. The plugin will handle the load.
    // Or: supported class id, plugin will handle the load.
    mType = eType_Plugin;

    // At this point, the stored content type
    // must be equal to our type hint. Similar,
    // our URI must be the requested URI.
    // (->Equals would suffice, but == is cheaper
    // and handles NULL)
    NS_ASSERTION(mContentType.Equals(aTypeHint), "mContentType wrong!");
    NS_ASSERTION(mURI == aURI, "mURI wrong!");

    if (isSupportedClassID) {
      // Use the classid's type
      NS_ASSERTION(!typeForID.IsEmpty(), "Must have a real type!");
      mContentType = typeForID;
      // XXX(biesi). The plugin instantiation code used to pass the base URI
      // here instead of the plugin URI for instantiation via class ID, so I
      // continue to do so. Why that is, no idea...
      GetObjectBaseURI(thisContent, getter_AddRefs(mURI));
      if (!mURI) {
        mURI = aURI;
      }
    }

    // rv is references by a stack-based object, need to assign here
    rv = AsyncStartPluginInstance();

    return rv;
  }

  if (!aURI) {
    // No URI and if we have got this far no enabled plugin supports the type
    rv = NS_ERROR_NOT_AVAILABLE;

    // We should only notify the UI if there is at least a type to go on for
    // finding a plugin to use, unless it's a supported image or document type.
    if (!aTypeHint.IsEmpty() && GetTypeOfContent(aTypeHint) == eType_Null) {
      UpdateFallbackState(thisContent, fallback, aTypeHint);
    }

    return NS_OK;
  }

  // E.g. mms://
  if (!CanHandleURI(aURI)) {
    if (aTypeHint.IsEmpty()) {
      rv = NS_ERROR_NOT_AVAILABLE;
      return NS_OK;
    }

    if (NS_SUCCEEDED(IsPluginEnabledForType(aTypeHint))) {
      mType = eType_Plugin;
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
      // No plugin to load, notify of the failure.
      UpdateFallbackState(thisContent, fallback, aTypeHint);
    }

    return NS_OK;
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
  rv = NS_NewChannel(getter_AddRefs(chan), aURI, nsnull, group, this,
                     nsIChannel::LOAD_CALL_CONTENT_SNIFFERS |
                     nsIChannel::LOAD_CLASSIFY_URI,
                     channelPolicy);
  NS_ENSURE_SUCCESS(rv, rv);

  // Referrer
  nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(chan));
  if (httpChan) {
    httpChan->SetReferrer(doc->GetDocumentURI());
  }

  // MIME Type hint
  if (!aTypeHint.IsEmpty()) {
    nsCAutoString typeHint, dummy;
    NS_ParseContentType(aTypeHint, typeHint, dummy);
    if (!typeHint.IsEmpty()) {
      chan->SetContentType(typeHint);
    }
  }

  // Set up the channel's principal and such, like nsDocShell::DoURILoad does
  nsContentUtils::SetUpChannelOwner(thisContent->NodePrincipal(),
                                    chan, aURI, true);

  nsCOMPtr<nsIScriptChannel> scriptChannel = do_QueryInterface(chan);
  if (scriptChannel) {
    // Allow execution against our context if the principals match
    scriptChannel->
      SetExecutionPolicy(nsIScriptChannel::EXECUTE_NORMAL);
  }

  // AsyncOpen can fail if a file does not exist.
  // Show fallback content in that case.
  rv = chan->AsyncOpen(this, nsnull);
  if (NS_SUCCEEDED(rv)) {
    LOG(("OBJLC [%p]: Channel opened.\n", this));

    mChannel = chan;
    mType = eType_Loading;
  }
  return NS_OK;
}

PRUint32
nsObjectLoadingContent::GetCapabilities() const
{
  return eSupportImages |
         eSupportPlugins |
         eSupportDocuments |
         eSupportSVG;
}

void
nsObjectLoadingContent::Fallback(bool aNotify)
{
  AutoNotifier notifier(this, aNotify);

  UnloadContent();
}

void
nsObjectLoadingContent::RemovedFromDocument()
{
  if (mFrameLoader) {
    // XXX This is very temporary and must go away
    mFrameLoader->Destroy();
    mFrameLoader = nsnull;

    // Clear the current URI, so that LoadObject doesn't think that we
    // have already loaded the content.
    mURI = nsnull;
  }

  // When a plugin instance node is removed from the document we'll
  // let the plugin continue to run at least until we get back to
  // the event loop. If we get back to the event loop and the node
  // has still not been added back to the document then we stop
  // the plugin.
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsCOMPtr<nsIRunnable> event = new InDocCheckEvent(thisContent);

  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->RunInStableState(event);
  }
}

/* static */
void
nsObjectLoadingContent::Traverse(nsObjectLoadingContent *tmp,
                                 nsCycleCollectionTraversalCallback &cb)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mFrameLoader");
  cb.NoteXPCOMChild(static_cast<nsIFrameLoader*>(tmp->mFrameLoader));
}

// <private>
/* static */ bool
nsObjectLoadingContent::IsSuccessfulRequest(nsIRequest* aRequest)
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

/* static */ bool
nsObjectLoadingContent::CanHandleURI(nsIURI* aURI)
{
  nsCAutoString scheme;
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
  return extHandler == nsnull;
}

bool
nsObjectLoadingContent::IsSupportedDocument(const nsCString& aMimeType)
{
  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  nsresult rv;
  nsCOMPtr<nsIWebNavigationInfo> info(
    do_GetService(NS_WEBNAVIGATION_INFO_CONTRACTID, &rv));
  PRUint32 supported;
  if (info) {
    nsCOMPtr<nsIWebNavigation> webNav;
    nsIDocument* currentDoc = thisContent->GetCurrentDoc();
    if (currentDoc) {
      webNav = do_GetInterface(currentDoc->GetScriptGlobalObject());
    }
    rv = info->IsTypeSupported(aMimeType, webNav, &supported);
  }

  if (NS_SUCCEEDED(rv)) {
    if (supported == nsIWebNavigationInfo::UNSUPPORTED) {
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

    // Don't want to support plugins as documents
    return supported != nsIWebNavigationInfo::PLUGIN;
  }

  return false;
}

void
nsObjectLoadingContent::UnloadContent()
{
  // Don't notify in CancelImageRequests. We do it ourselves.
  CancelImageRequests(false);
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nsnull;
  }
  mType = eType_Null;
  mUserDisabled = mSuppressed = false;
  mFallbackReason = ePluginOtherState;
}

void
nsObjectLoadingContent::NotifyStateChanged(ObjectType aOldType,
                                           nsEventStates aOldState,
                                           bool aSync,
                                           bool aNotify)
{
  LOG(("OBJLC [%p]: Notifying about state change: (%u, %llx) -> (%u, %llx) (sync=%i)\n",
       this, aOldType, aOldState.GetInternalValue(), mType,
       ObjectState().GetInternalValue(), aSync));

  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  NS_ASSERTION(thisContent->IsElement(), "Not an element?");

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
    NS_ASSERTION(thisContent->IsInDoc(), "Something is confused");
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

/* static */ void
nsObjectLoadingContent::FirePluginError(nsIContent* thisContent,
                                        PluginSupportState state)
{
  LOG(("OBJLC []: Dispatching nsPluginErrorEvent for content %p\n",
       thisContent));

  nsCOMPtr<nsIRunnable> ev = new nsPluginErrorEvent(thisContent, state);
  nsresult rv = NS_DispatchToCurrentThread(ev);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch nsPluginErrorEvent");
  }
}

nsObjectLoadingContent::ObjectType
nsObjectLoadingContent::GetTypeOfContent(const nsCString& aMIMEType)
{
  PRUint32 caps = GetCapabilities();

  if ((caps & eSupportImages) && IsSupportedImage(aMIMEType)) {
    return eType_Image;
  }

  bool isSVG = aMIMEType.LowerCaseEqualsLiteral("image/svg+xml");
  bool supportedSVG = isSVG && (caps & eSupportSVG);
  if (((caps & eSupportDocuments) || supportedSVG) &&
      IsSupportedDocument(aMIMEType)) {
    return eType_Document;
  }

  if ((caps & eSupportPlugins) && NS_SUCCEEDED(IsPluginEnabledForType(aMIMEType))) {
    return eType_Plugin;
  }

  return eType_Null;
}

nsresult
nsObjectLoadingContent::TypeForClassID(const nsAString& aClassID,
                                       nsACString& aType)
{
  if (StringBeginsWith(aClassID, NS_LITERAL_STRING("java:"))) {
    // Supported if we have a java plugin
    aType.AssignLiteral("application/x-java-vm");
    nsresult rv = IsPluginEnabledForType(NS_LITERAL_CSTRING("application/x-java-vm"));
    return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  // If it starts with "clsid:", this is ActiveX content
  if (StringBeginsWith(aClassID, NS_LITERAL_STRING("clsid:"), nsCaseInsensitiveStringComparator())) {
    // Check if we have a plugin for that

    if (NS_SUCCEEDED(IsPluginEnabledForType(NS_LITERAL_CSTRING("application/x-oleobject")))) {
      aType.AssignLiteral("application/x-oleobject");
      return NS_OK;
    }
    if (NS_SUCCEEDED(IsPluginEnabledForType(NS_LITERAL_CSTRING("application/oleobject")))) {
      aType.AssignLiteral("application/oleobject");
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

void
nsObjectLoadingContent::GetObjectBaseURI(nsIContent* thisContent, nsIURI** aURI)
{
  // We want to use swap(); since this is just called from this file,
  // we can assert this (callers use comptrs)
  NS_PRECONDITION(*aURI == nsnull, "URI must be inited to zero");

  // For plugins, the codebase attribute is the base URI
  nsCOMPtr<nsIURI> baseURI = thisContent->GetBaseURI();
  nsAutoString codebase;
  thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::codebase,
                       codebase);
  if (!codebase.IsEmpty()) {
    nsContentUtils::NewURIWithDocumentCharset(aURI, codebase,
                                              thisContent->OwnerDoc(),
                                              baseURI);
  } else {
    baseURI.swap(*aURI);
  }
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
nsObjectLoadingContent::HandleBeingBlockedByContentPolicy(nsresult aStatus,
                                                          PRInt16 aRetval)
{
  // Must call UnloadContent first, as it overwrites
  // mSuppressed/mUserDisabled. It also takes care of setting the type to
  // eType_Null.
  UnloadContent();
  if (NS_SUCCEEDED(aStatus)) {
    if (aRetval == nsIContentPolicy::REJECT_TYPE) {
      mUserDisabled = true;
    } else if (aRetval == nsIContentPolicy::REJECT_SERVER) {
      mSuppressed = true;
    }
  }
}

PluginSupportState
nsObjectLoadingContent::GetPluginSupportState(nsIContent* aContent,
                                              const nsCString& aContentType)
{
  if (!aContent->IsHTML()) {
    return ePluginOtherState;
  }

  if (aContent->Tag() == nsGkAtoms::embed ||
      aContent->Tag() == nsGkAtoms::applet) {
    return GetPluginDisabledState(aContentType);
  }

  bool hasAlternateContent = false;

  // Search for a child <param> with a pluginurl name
  for (nsIContent* child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTML(nsGkAtoms::param)) {
      if (child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                             NS_LITERAL_STRING("pluginurl"), eIgnoreCase)) {
        return GetPluginDisabledState(aContentType);
      }
    } else if (!hasAlternateContent) {
      hasAlternateContent =
        nsStyleUtil::IsSignificantChild(child, true, false);
    }
  }

  PluginSupportState pluginDisabledState = GetPluginDisabledState(aContentType);
  if (pluginDisabledState == ePluginClickToPlay) {
    return ePluginClickToPlay;
  } else if (hasAlternateContent) {
    return ePluginOtherState;
  } else {
    return pluginDisabledState;
  }
}

PluginSupportState
nsObjectLoadingContent::GetPluginDisabledState(const nsCString& aContentType)
{
  nsresult rv = IsPluginEnabledForType(aContentType);
  if (rv == NS_ERROR_PLUGIN_DISABLED)
    return ePluginDisabled;
  if (rv == NS_ERROR_PLUGIN_CLICKTOPLAY)
    return ePluginClickToPlay;
  if (rv == NS_ERROR_PLUGIN_BLOCKLISTED)
    return ePluginBlocklisted;
  return ePluginUnsupported;
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
  AutoNotifier notifier(this, true);
  UnloadContent();
  mFallbackReason = ePluginCrashed;
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  // Note that aPluginTag in invalidated after we're called, so copy 
  // out any data we need now.
  nsCAutoString pluginName;
  aPluginTag->GetName(pluginName);
  nsCAutoString pluginFilename;
  aPluginTag->GetFilename(pluginFilename);

  nsCOMPtr<nsIRunnable> ev = new nsPluginCrashedEvent(thisContent,
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
nsObjectLoadingContent::SyncStartPluginInstance()
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Must be able to run script in order to instantiate a plugin instance!");

  // Don't even attempt to start an instance unless the content is in
  // the document.
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  if (!thisContent->IsInDoc()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> kungFuURIGrip(mURI);
  nsCString contentType(mContentType);
  return InstantiatePluginInstance(contentType.get(), mURI.get());
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
nsObjectLoadingContent::DoStopPlugin(nsPluginInstanceOwner* aInstanceOwner,
                                     bool aDelayedStop,
                                     bool aForcedReentry)
{
  // DoStopPlugin can process events and there may be pending InDocCheckEvent
  // events which can drop in underneath us and destroy the instance we are
  // about to destroy unless we prevent that with the mIsStopping flag.
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

    nsCOMPtr<nsIPluginHost> pluginHost = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
    NS_ASSERTION(pluginHost, "Without a pluginHost, how can we have an instance to destroy?");
    static_cast<nsPluginHost*>(pluginHost.get())->StopPluginInstance(inst);
  }
  
  aInstanceOwner->Destroy();
  mIsStopping = false;
}

NS_IMETHODIMP
nsObjectLoadingContent::StopPluginInstance()
{
  if (!mInstanceOwner) {
    return NS_OK;
  }

  DisconnectFrame();

  bool delayedStop = false;
#ifdef XP_WIN
  // Force delayed stop for Real plugin only; see bug 420886, 426852.
  nsRefPtr<nsNPAPIPluginInstance> inst;
  mInstanceOwner->GetInstance(getter_AddRefs(inst));
  if (inst) {
    const char* mime = nsnull;
    if (NS_SUCCEEDED(inst->GetMIMEType(&mime)) && mime) {
      if (strcmp(mime, "audio/x-pn-realaudio-plugin") == 0) {
        delayedStop = true;
      }      
    }
  }
#endif

  DoStopPlugin(mInstanceOwner, delayedStop);

  mInstanceOwner = nsnull;

  return NS_OK;
}

void
nsObjectLoadingContent::NotifyContentObjectWrapper()
{
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

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
  
  JSObject *obj = nsnull;
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

  mShouldPlay = true;
  return LoadObject(mURI, true, mContentType, true);
}

NS_IMETHODIMP
nsObjectLoadingContent::GetActivated(bool* aActivated)
{
  *aActivated = mActivated;
  return NS_OK;
}

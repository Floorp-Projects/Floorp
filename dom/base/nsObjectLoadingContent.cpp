/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIConsoleService.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDOMCustomEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIExternalProtocolHandler.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObjectFrame.h"
#include "nsIPermissionManager.h"
#include "nsPluginHost.h"
#include "nsPluginInstanceOwner.h"
#include "nsJSNPRuntime.h"
#include "nsINestedURI.h"
#include "nsIPresShell.h"
#include "nsScriptSecurityManager.h"
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
#include "nsIXULRuntime.h"
#include "nsIScriptError.h"

#include "nsError.h"

// Util headers
#include "prenv.h"
#include "mozilla/Logging.h"

#include "nsCURILoader.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocShellCID.h"
#include "nsGkAtoms.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsStyleUtil.h"
#include "nsUnicharUtils.h"
#include "mozilla/Preferences.h"
#include "nsSandboxFlags.h"

// Concrete classes
#include "nsFrameLoader.h"

#include "nsObjectLoadingContent.h"
#include "mozAutoDocUpdate.h"
#include "nsIContentSecurityPolicy.h"
#include "GeckoProfiler.h"
#include "nsPluginFrame.h"
#include "nsDOMClassInfo.h"
#include "nsWrapperCacheInlines.h"
#include "nsDOMJSUtils.h"

#include "nsWidgetsCID.h"
#include "nsContentCID.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/PluginCrashedEvent.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/HTMLSharedObjectElement.h"
#include "nsChannelClassifier.h"

#ifdef XP_WIN
// Thanks so much, Microsoft! :(
#ifdef CreateEvent
#undef CreateEvent
#endif
#endif // XP_WIN

#ifdef XP_MACOSX
// HandlePluginCrashed() and HandlePluginInstantiated() needed from here to
// fix bug 1147521.  Should later be replaced by proper interface methods,
// maybe on nsIObjectLoadingContext.
#include "mozilla/dom/HTMLObjectElement.h"
#endif

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

static const char *kPrefJavaMIME = "plugin.java.mime";
static const char *kPrefYoutubeRewrite = "plugins.rewrite_youtube_embeds";
static const char *kPrefBlockURIs = "browser.safebrowsing.blockedURIs.enabled";

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::net;

static LogModule*
GetObjectLog()
{
  static LazyLogModule sLog("objlc");
  return sLog;
}

#define LOG(args) MOZ_LOG(GetObjectLog(), mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(GetObjectLog(), mozilla::LogLevel::Debug)

static bool
IsJavaMIME(const nsACString & aMIMEType)
{
  return
    nsPluginHost::GetSpecialType(aMIMEType) == nsPluginHost::eSpecialType_Java;
}

static bool
IsFlashMIME(const nsACString & aMIMEType)
{
  return
    nsPluginHost::GetSpecialType(aMIMEType) == nsPluginHost::eSpecialType_Flash;
}

static bool
InActiveDocument(nsIContent *aContent)
{
  if (!aContent->IsInComposedDoc()) {
    return false;
  }
  nsIDocument *doc = aContent->OwnerDoc();
  return (doc && doc->IsActive());
}

///
/// Runnables and helper classes
///

class nsAsyncInstantiateEvent : public Runnable {
public:
  explicit nsAsyncInstantiateEvent(nsObjectLoadingContent* aContent)
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

  // If objLC is no longer tracking this event, we've been canceled or
  // superseded
  if (objLC->mPendingInstantiateEvent != this) {
    return NS_OK;
  }
  objLC->mPendingInstantiateEvent = nullptr;

  return objLC->SyncStartPluginInstance();
}

// Checks to see if the content for a plugin instance should be unloaded
// (outside an active document) or stopped (in a document but unrendered). This
// is used to allow scripts to move a plugin around the document hierarchy
// without re-instantiating it.
class CheckPluginStopEvent : public Runnable {
public:
  explicit CheckPluginStopEvent(nsObjectLoadingContent* aContent)
  : mContent(aContent) {}

  ~CheckPluginStopEvent() {}

  NS_IMETHOD Run();

private:
  nsCOMPtr<nsIObjectLoadingContent> mContent;
};

NS_IMETHODIMP
CheckPluginStopEvent::Run()
{
  nsObjectLoadingContent *objLC =
    static_cast<nsObjectLoadingContent *>(mContent.get());

  // If objLC is no longer tracking this event, we've been canceled or
  // superseded. We clear this before we finish - either by calling
  // UnloadObject/StopPluginInstance, or directly if we took no action.
  if (objLC->mPendingCheckPluginStopEvent != this) {
    return NS_OK;
  }

  // CheckPluginStopEvent is queued when we either lose our frame, are removed
  // from the document, or the document goes inactive. To avoid stopping the
  // plugin when script is reparenting us or layout is rebuilding, we wait until
  // this event to decide to stop.

  nsCOMPtr<nsIContent> content =
    do_QueryInterface(static_cast<nsIImageLoadingContent *>(objLC));
  if (!InActiveDocument(content)) {
    LOG(("OBJLC [%p]: Unloading plugin outside of document", this));
    objLC->StopPluginInstance();
    return NS_OK;
  }

  if (content->GetPrimaryFrame()) {
    LOG(("OBJLC [%p]: CheckPluginStopEvent - in active document with frame"
         ", no action", this));
    objLC->mPendingCheckPluginStopEvent = nullptr;
    return NS_OK;
  }

  // In an active document, but still no frame. Flush layout to see if we can
  // regain a frame now.
  LOG(("OBJLC [%p]: CheckPluginStopEvent - No frame, flushing layout", this));
  nsIDocument* composedDoc = content->GetComposedDoc();
  if (composedDoc) {
    composedDoc->FlushPendingNotifications(Flush_Layout);
    if (objLC->mPendingCheckPluginStopEvent != this) {
      LOG(("OBJLC [%p]: CheckPluginStopEvent - superseded in layout flush",
           this));
      return NS_OK;
    } else if (content->GetPrimaryFrame()) {
      LOG(("OBJLC [%p]: CheckPluginStopEvent - frame gained in layout flush",
           this));
      objLC->mPendingCheckPluginStopEvent = nullptr;
      return NS_OK;
    }
  }

  // Still no frame, suspend plugin. HasNewFrame will restart us when we
  // become rendered again
  LOG(("OBJLC [%p]: Stopping plugin that lost frame", this));
  objLC->StopPluginInstance();

  return NS_OK;
}

/**
 * Helper task for firing simple events
 */
class nsSimplePluginEvent : public Runnable {
public:
  nsSimplePluginEvent(nsIContent* aTarget, const nsAString &aEvent)
    : mTarget(aTarget)
    , mDocument(aTarget->GetComposedDoc())
    , mEvent(aEvent)
  {
    MOZ_ASSERT(aTarget && mDocument);
  }

  nsSimplePluginEvent(nsIDocument* aTarget, const nsAString& aEvent)
    : mTarget(aTarget)
    , mDocument(aTarget)
    , mEvent(aEvent)
  {
    MOZ_ASSERT(aTarget);
  }

  nsSimplePluginEvent(nsIContent* aTarget,
                      nsIDocument* aDocument,
                      const nsAString& aEvent)
    : mTarget(aTarget)
    , mDocument(aDocument)
    , mEvent(aEvent)
  {
    MOZ_ASSERT(aTarget && aDocument);
  }

  ~nsSimplePluginEvent() {}

  NS_IMETHOD Run();

private:
  nsCOMPtr<nsISupports> mTarget;
  nsCOMPtr<nsIDocument> mDocument;
  nsString mEvent;
};

NS_IMETHODIMP
nsSimplePluginEvent::Run()
{
  if (mDocument && mDocument->IsActive()) {
    LOG(("OBJLC [%p]: nsSimplePluginEvent firing event \"%s\"", mTarget.get(),
         NS_ConvertUTF16toUTF8(mEvent).get()));
    nsContentUtils::DispatchTrustedEvent(mDocument, mTarget,
                                         mEvent, true, true);
  }
  return NS_OK;
}

/**
 * A task for firing PluginCrashed DOM Events.
 */
class nsPluginCrashedEvent : public Runnable {
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

  nsCOMPtr<nsIDocument> doc = mContent->GetComposedDoc();
  if (!doc) {
    NS_WARNING("Couldn't get document for PluginCrashed event!");
    return NS_OK;
  }

  PluginCrashedEventInit init;
  init.mPluginDumpID = mPluginDumpID;
  init.mBrowserDumpID = mBrowserDumpID;
  init.mPluginName = mPluginName;
  init.mPluginFilename = mPluginFilename;
  init.mSubmittedCrashReport = mSubmittedCrashReport;
  init.mBubbles = true;
  init.mCancelable = true;

  RefPtr<PluginCrashedEvent> event =
    PluginCrashedEvent::Constructor(doc, NS_LITERAL_STRING("PluginCrashed"), init);

  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  EventDispatcher::DispatchDOMEvent(mContent, nullptr, event, nullptr, nullptr);
  return NS_OK;
}

class nsStopPluginRunnable : public Runnable, public nsITimerCallback
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

  // Runnable
  NS_IMETHOD Run() override;

  // nsITimerCallback
  NS_IMETHOD Notify(nsITimer* timer) override;

protected:
  virtual ~nsStopPluginRunnable() {}

private:
  nsCOMPtr<nsITimer> mTimer;
  RefPtr<nsPluginInstanceOwner> mInstanceOwner;
  nsCOMPtr<nsIObjectLoadingContent> mContent;
};

NS_IMPL_ISUPPORTS_INHERITED(nsStopPluginRunnable, Runnable, nsITimerCallback)

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
  explicit AutoSetInstantiatingToFalse(nsObjectLoadingContent* aContent)
    : mContent(aContent) {}
  ~AutoSetInstantiatingToFalse() { mContent->mInstantiating = false; }
private:
  nsObjectLoadingContent* mContent;
};

// Sets a object's mInstantiating bit to false when destroyed
class AutoSetLoadingToFalse {
public:
  explicit AutoSetLoadingToFalse(nsObjectLoadingContent* aContent)
    : mContent(aContent) {}
  ~AutoSetLoadingToFalse() { mContent->mIsLoading = false; }
private:
  nsObjectLoadingContent* mContent;
};

///
/// Helper functions
///

static bool
IsSuccessfulRequest(nsIRequest* aRequest, nsresult* aStatus)
{
  nsresult rv = aRequest->GetStatus(aStatus);
  if (NS_FAILED(rv) || NS_FAILED(*aStatus)) {
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
    nsresult rv = uri->GetSpec(spec);
    if (NS_FAILED(rv)) {
      // This means we could incorrectly think a plugin is not enabled for
      // the URI when it is, but that's not so bad.
      ext.Truncate();
      return;
    }

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

  // Disables any native PDF plugins, when internal PDF viewer is enabled.
  if (ext.EqualsIgnoreCase("pdf") && nsContentUtils::IsPDFJSEnabled()) {
    return false;
  }

  // Disables any native SWF plugins, when internal SWF player is enabled.
  if (ext.EqualsIgnoreCase("swf") && nsContentUtils::IsSWFPlayerEnabled()) {
    return false;
  }

  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();

  if (!pluginHost) {
    NS_NOTREACHED("No pluginhost");
    return false;
  }

  return pluginHost->HavePluginForExtension(ext, mimeType);
}

///
/// Member Functions
///

// Helper to queue a CheckPluginStopEvent for a OBJLC object
void
nsObjectLoadingContent::QueueCheckPluginStopEvent()
{
  nsCOMPtr<nsIRunnable> event = new CheckPluginStopEvent(this);
  mPendingCheckPluginStopEvent = event;

  NS_DispatchToCurrentThread(event);
}

// Tedious syntax to create a plugin stream listener with checks and put it in
// mFinalListener
bool
nsObjectLoadingContent::MakePluginListener()
{
  if (!mInstanceOwner) {
    NS_NOTREACHED("expecting a spawned plugin");
    return false;
  }
  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  if (!pluginHost) {
    NS_NOTREACHED("No pluginHost");
    return false;
  }
  NS_ASSERTION(!mFinalListener, "overwriting a final listener");
  nsresult rv;
  RefPtr<nsNPAPIPluginInstance> inst;
  nsCOMPtr<nsIStreamListener> finalListener;
  rv = mInstanceOwner->GetInstance(getter_AddRefs(inst));
  NS_ENSURE_SUCCESS(rv, false);
  rv = pluginHost->NewPluginStreamListener(mURI, inst,
                                           getter_AddRefs(finalListener));
  NS_ENSURE_SUCCESS(rv, false);
  mFinalListener = finalListener;
  return true;
}


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
  nsIDocument* currentDoc = thisContent->GetComposedDoc();
  if (currentDoc) {
    webNav = do_GetInterface(currentDoc->GetWindow());
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

  if (mType == eType_Plugin && (mInstanceOwner || mInstantiating)) {
    // we'll let the plugin continue to run at least until we get back to
    // the event loop. If we get back to the event loop and the node
    // has still not been added back to the document then we tear down the
    // plugin
    QueueCheckPluginStopEvent();
  } else if (mType != eType_Image) {
    // nsImageLoadingContent handles the image case.
    // Reset state and clear pending events
    /// XXX(johns): The implementation for GenericFrame notes that ideally we
    ///             would keep the docshell around, but trash the frameloader
    UnloadObject();
  }
  nsIDocument* doc = thisContent->GetComposedDoc();
  if (doc && doc->IsActive()) {
    nsCOMPtr<nsIRunnable> ev = new nsSimplePluginEvent(doc,
                                                       NS_LITERAL_STRING("PluginRemoved"));
    NS_DispatchToCurrentThread(ev);
  }
}

nsObjectLoadingContent::nsObjectLoadingContent()
  : mType(eType_Loading)
  , mFallbackType(eFallbackAlternate)
  , mRunID(0)
  , mHasRunID(false)
  , mChannelLoaded(false)
  , mInstantiating(false)
  , mNetworkCreated(true)
  , mActivated(false)
  , mContentBlockingDisabled(false)
  , mIsStopping(false)
  , mIsLoading(false)
  , mScriptRequested(false)
  , mRewrittenYoutubeEmbed(false) {}

nsObjectLoadingContent::~nsObjectLoadingContent()
{
  // Should have been unbound from the tree at this point, and
  // CheckPluginStopEvent keeps us alive
  if (mFrameLoader) {
    NS_NOTREACHED("Should not be tearing down frame loaders at this point");
    mFrameLoader->Destroy();
  }
  if (mInstanceOwner || mInstantiating) {
    // This is especially bad as delayed stop will try to hold on to this
    // object...
    NS_NOTREACHED("Should not be tearing down a plugin at this point!");
    StopPluginInstance();
  }
  DestroyImageLoadingContent();
}

nsresult
nsObjectLoadingContent::InstantiatePluginInstance(bool aIsLoading)
{
  if (mInstanceOwner || mType != eType_Plugin || (mIsLoading != aIsLoading) ||
      mInstantiating) {
    // If we hit this assertion it's probably because LoadObject re-entered :(
    //
    // XXX(johns): This hackiness will go away in bug 767635
    NS_ASSERTION(mIsLoading || !aIsLoading,
                 "aIsLoading should only be true inside LoadObject");
    return NS_OK;
  }

  mInstantiating = true;
  AutoSetInstantiatingToFalse autoInstantiating(this);

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent *>(this));

  nsCOMPtr<nsIDocument> doc = thisContent->GetComposedDoc();
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
  // Flushing layout may have re-entered and loaded something underneath us
  NS_ENSURE_TRUE(mInstantiating, NS_OK);

  if (!thisContent->GetPrimaryFrame()) {
    LOG(("OBJLC [%p]: Not instantiating plugin with no frame", this));
    return NS_OK;
  }

  nsresult rv = NS_ERROR_FAILURE;
  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();

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

  RefPtr<nsPluginInstanceOwner> newOwner;
  rv = pluginHost->InstantiatePluginInstance(mContentType,
                                             mURI.get(), this,
                                             getter_AddRefs(newOwner));

  // XXX(johns): We don't suspend native inside stopping plugins...
  if (appShell) {
    appShell->ResumeNative();
  }

  if (!mInstantiating || NS_FAILED(rv)) {
    LOG(("OBJLC [%p]: Plugin instantiation failed or re-entered, "
         "killing old instance", this));
    // XXX(johns): This needs to be de-duplicated with DoStopPlugin, but we
    //             don't want to touch the protochain or delayed stop.
    //             (Bug 767635)
    if (newOwner) {
      RefPtr<nsNPAPIPluginInstance> inst;
      newOwner->GetInstance(getter_AddRefs(inst));
      newOwner->SetFrame(nullptr);
      if (inst) {
        pluginHost->StopPluginInstance(inst);
      }
      newOwner->Destroy();
    }
    return NS_OK;
  }

  mInstanceOwner = newOwner;

  if (mInstanceOwner) {
    RefPtr<nsNPAPIPluginInstance> inst;
    rv = mInstanceOwner->GetInstance(getter_AddRefs(inst));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = inst->GetRunID(&mRunID);
    mHasRunID = NS_SUCCEEDED(rv);
  }

  // Ensure the frame did not change during instantiation re-entry (common).
  // HasNewFrame would not have mInstanceOwner yet, so the new frame would be
  // dangling. (Bug 854082)
  nsIFrame* frame = thisContent->GetPrimaryFrame();
  if (frame && mInstanceOwner) {
    mInstanceOwner->SetFrame(static_cast<nsPluginFrame*>(frame));

    // Bug 870216 - Adobe Reader renders with incorrect dimensions until it gets
    // a second SetWindow call. This is otherwise redundant.
    mInstanceOwner->CallSetWindow();
  }

  // Set up scripting interfaces.
  NotifyContentObjectWrapper();

  RefPtr<nsNPAPIPluginInstance> pluginInstance;
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

    // If we have a URI but didn't open a channel yet (eAllowPluginSkipChannel)
    // or we did load with a channel but are re-instantiating, re-open the
    // channel. OpenChannel() performs security checks, and this plugin has
    // already passed content policy in LoadObject.
    if ((mURI && !mChannelLoaded) || (mChannelLoaded && !aIsLoading)) {
      NS_ASSERTION(!mChannel, "should not have an existing channel here");
      // We intentionally ignore errors here, leaving it up to the plugin to
      // deal with not having an initial stream.
      OpenChannel();
    }
  }

  nsCOMPtr<nsIRunnable> ev = \
    new nsSimplePluginEvent(thisContent,
                            doc,
                            NS_LITERAL_STRING("PluginInstantiated"));
  NS_DispatchToCurrentThread(ev);

#ifdef XP_MACOSX
  HTMLObjectElement::HandlePluginInstantiated(thisContent->AsElement());
#endif

  return NS_OK;
}

void
nsObjectLoadingContent::GetPluginAttributes(nsTArray<MozPluginParameter>& aAttributes)
{
  aAttributes = mCachedAttributes;
}

void
nsObjectLoadingContent::GetPluginParameters(nsTArray<MozPluginParameter>& aParameters)
{
  aParameters = mCachedParameters;
}

void
nsObjectLoadingContent::GetNestedParams(nsTArray<MozPluginParameter>& aParams,
                                        bool aIgnoreCodebase)
{
  nsCOMPtr<nsIDOMElement> domElement =
    do_QueryInterface(static_cast<nsIObjectLoadingContent*>(this));

  nsCOMPtr<nsIDOMHTMLCollection> allParams;
  NS_NAMED_LITERAL_STRING(xhtml_ns, "http://www.w3.org/1999/xhtml");
  domElement->GetElementsByTagNameNS(xhtml_ns,
        NS_LITERAL_STRING("param"), getter_AddRefs(allParams));

  if (!allParams)
    return;

  uint32_t numAllParams;
  allParams->GetLength(&numAllParams);
  for (uint32_t i = 0; i < numAllParams; i++) {
    nsCOMPtr<nsIDOMNode> pNode;
    allParams->Item(i, getter_AddRefs(pNode));
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(pNode);

    if (!element)
      continue;

    nsAutoString name;
    element->GetAttribute(NS_LITERAL_STRING("name"), name);

    if (name.IsEmpty())
      continue;

    nsCOMPtr<nsIDOMNode> parent;
    nsCOMPtr<nsIDOMHTMLObjectElement> domObject;
    nsCOMPtr<nsIDOMHTMLAppletElement> domApplet;
    pNode->GetParentNode(getter_AddRefs(parent));
    while (!(domObject || domApplet) && parent) {
      domObject = do_QueryInterface(parent);
      domApplet = do_QueryInterface(parent);
      nsCOMPtr<nsIDOMNode> temp;
      parent->GetParentNode(getter_AddRefs(temp));
      parent = temp;
    }

    if (domApplet) {
      parent = do_QueryInterface(domApplet);
    } else if (domObject) {
      parent = do_QueryInterface(domObject);
    } else {
      continue;
    }

    nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(domElement);
    if (parent == domNode) {
      MozPluginParameter param;
      element->GetAttribute(NS_LITERAL_STRING("name"), param.mName);
      element->GetAttribute(NS_LITERAL_STRING("value"), param.mValue);

      param.mName.Trim(" \n\r\t\b", true, true, false);
      param.mValue.Trim(" \n\r\t\b", true, true, false);

      // ignore codebase param if it was already added in the attributes array.
      if (aIgnoreCodebase && param.mName.EqualsIgnoreCase("codebase")) {
        continue;
      }

      aParams.AppendElement(param);
    }
  }
}

nsresult
nsObjectLoadingContent::BuildParametersArray()
{
  if (mCachedAttributes.Length() || mCachedParameters.Length()) {
    MOZ_ASSERT(false, "Parameters array should be empty.");
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  for (uint32_t i = 0; i != content->GetAttrCount(); i += 1) {
    MozPluginParameter param;
    const nsAttrName* attrName = content->GetAttrNameAt(i);
    nsIAtom* atom = attrName->LocalName();
    content->GetAttr(attrName->NamespaceID(), atom, param.mValue);
    atom->ToString(param.mName);
    mCachedAttributes.AppendElement(param);
  }

  bool isJava = IsJavaMIME(mContentType);

  nsCString codebase;
  if (isJava) {
      nsresult rv = mBaseURI->GetSpec(codebase);
      NS_ENSURE_SUCCESS(rv, rv);
  }

  nsAdoptingCString wmodeOverride = Preferences::GetCString("plugins.force.wmode");
  for (uint32_t i = 0; i < mCachedAttributes.Length(); i++) {
    if (!wmodeOverride.IsEmpty() && mCachedAttributes[i].mName.EqualsIgnoreCase("wmode")) {
      CopyASCIItoUTF16(wmodeOverride, mCachedAttributes[i].mValue);
      wmodeOverride.Truncate();
    } else if (!codebase.IsEmpty() && mCachedAttributes[i].mName.EqualsIgnoreCase("codebase")) {
      CopyASCIItoUTF16(codebase, mCachedAttributes[i].mValue);
      codebase.Truncate();
    }
  }

  if (!wmodeOverride.IsEmpty()) {
    MozPluginParameter param;
    param.mName = NS_LITERAL_STRING("wmode");
    CopyASCIItoUTF16(wmodeOverride, param.mValue);
    mCachedAttributes.AppendElement(param);
  }

  if (!codebase.IsEmpty()) {
    MozPluginParameter param;
    param.mName = NS_LITERAL_STRING("codebase");
    CopyASCIItoUTF16(codebase, param.mValue);
    mCachedAttributes.AppendElement(param);
  }

  // Some plugins were never written to understand the "data" attribute of the OBJECT tag.
  // Real and WMP will not play unless they find a "src" attribute, see bug 152334.
  // Nav 4.x would simply replace the "data" with "src". Because some plugins correctly
  // look for "data", lets instead copy the "data" attribute and add another entry
  // to the bottom of the array if there isn't already a "src" specified.
  if (content->IsHTMLElement(nsGkAtoms::object) &&
      !content->HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
    MozPluginParameter param;
    content->GetAttr(kNameSpaceID_None, nsGkAtoms::data, param.mValue);
    if (!param.mValue.IsEmpty()) {
      param.mName = NS_LITERAL_STRING("SRC");
      mCachedAttributes.AppendElement(param);
    }
  }

  GetNestedParams(mCachedParameters, isJava);

  return NS_OK;
}

void
nsObjectLoadingContent::NotifyOwnerDocumentActivityChanged()
{
  // XXX(johns): We cannot touch plugins or run arbitrary script from this call,
  //             as nsDocument is in a non-reentrant state.

  // If we have a plugin we want to queue an event to stop it unless we are
  // moved into an active document before returning to the event loop.
  if (mInstanceOwner || mInstantiating) {
    QueueCheckPluginStopEvent();
  }
}

// nsIRequestObserver
NS_IMETHODIMP
nsObjectLoadingContent::OnStartRequest(nsIRequest *aRequest,
                                       nsISupports *aContext)
{
  PROFILER_LABEL("nsObjectLoadingContent", "OnStartRequest",
    js::ProfileEntry::Category::NETWORK);

  LOG(("OBJLC [%p]: Channel OnStartRequest", this));

  if (aRequest != mChannel || !aRequest) {
    // happens when a new load starts before the previous one got here
    return NS_BINDING_ABORTED;
  }

  // If we already switched to type plugin, this channel can just be passed to
  // the final listener.
  if (mType == eType_Plugin) {
    if (!mInstanceOwner) {
      // We drop mChannel when stopping plugins, so something is wrong
      NS_NOTREACHED("Opened a channel in plugin mode, but don't have a plugin");
      return NS_BINDING_ABORTED;
    }
    if (MakePluginListener()) {
      return mFinalListener->OnStartRequest(aRequest, nullptr);
    } else {
      NS_NOTREACHED("Failed to create PluginStreamListener, aborting channel");
      return NS_BINDING_ABORTED;
    }
  }

  // Otherwise we should be state loading, and call LoadObject with the channel
  if (mType != eType_Loading) {
    NS_NOTREACHED("Should be type loading at this point");
    return NS_BINDING_ABORTED;
  }
  NS_ASSERTION(!mChannelLoaded, "mChannelLoaded set already?");
  NS_ASSERTION(!mFinalListener, "mFinalListener exists already?");

  mChannelLoaded = true;

  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  NS_ASSERTION(chan, "Why is our request not a channel?");

  nsresult status;
  bool success = IsSuccessfulRequest(aRequest, &status);

  if (status == NS_ERROR_BLOCKED_URI) {
    nsCOMPtr<nsIConsoleService> console(
      do_GetService("@mozilla.org/consoleservice;1"));
    if (console) {
      nsCOMPtr<nsIURI> uri;
      chan->GetURI(getter_AddRefs(uri));
      nsString message = NS_LITERAL_STRING("Blocking ") +
        NS_ConvertASCIItoUTF16(uri->GetSpecOrDefault().get()) +
        NS_LITERAL_STRING(" since it was found on an internal Firefox blocklist.");
      console->LogStringMessage(message.get());
    }
    Telemetry::Accumulate(Telemetry::PLUGIN_BLOCKED_FOR_STABILITY, 1);
    return NS_ERROR_FAILURE;
  } else if (status == NS_ERROR_TRACKING_URI) {
    return NS_ERROR_FAILURE;
  } else {
    mContentBlockingDisabled = true;
  }

  if (!success) {
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
  PROFILER_LABEL("nsObjectLoadingContent", "OnStopRequest",
    js::ProfileEntry::Category::NETWORK);

  // Handle object not loading error because source was a tracking URL.
  // We make a note of this object node by including it in a dedicated
  // array of blocked tracking nodes under its parent document.
  if (aStatusCode == NS_ERROR_TRACKING_URI) {
    nsCOMPtr<nsIContent> thisNode =
      do_QueryInterface(static_cast<nsIObjectLoadingContent*>(this));
    if (thisNode && thisNode->IsInComposedDoc()) {
      thisNode->GetComposedDoc()->AddBlockedTrackingNode(thisNode);
    }
  }

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
nsObjectLoadingContent::GetFrameLoaderXPCOM(nsIFrameLoader** aFrameLoader)
{
  NS_IF_ADDREF(*aFrameLoader = mFrameLoader);
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsFrameLoader>)
nsObjectLoadingContent::GetFrameLoader()
{
  RefPtr<nsFrameLoader> loader = mFrameLoader;
  return loader.forget();
}

NS_IMETHODIMP
nsObjectLoadingContent::GetParentApplication(mozIApplication** aApplication)
{
  if (!aApplication) {
    return NS_ERROR_FAILURE;
  }

  *aApplication = nullptr;
  return NS_OK;
}

void
nsObjectLoadingContent::PresetOpenerWindow(mozIDOMWindowProxy* aWindow, mozilla::ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsObjectLoadingContent::SetIsPrerendered()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsObjectLoadingContent::InternalSetFrameLoader(nsIFrameLoader* aNewFrameLoader)
{
  MOZ_CRASH("You shouldn't be calling this function, it doesn't make any sense on this type.");
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
  *aType = DisplayedType();
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::HasNewFrame(nsIObjectFrame* aFrame)
{
  if (mType != eType_Plugin) {
    return NS_OK;
  }

  if (!aFrame) {
    // Lost our frame. If we aren't going to be getting a new frame, e.g. we've
    // become display:none, we'll want to stop the plugin. Queue a
    // CheckPluginStopEvent
    if (mInstanceOwner || mInstantiating) {
      if (mInstanceOwner) {
        mInstanceOwner->SetFrame(nullptr);
      }
      QueueCheckPluginStopEvent();
    }
    return NS_OK;
  }

  // Have a new frame

  if (!mInstanceOwner) {
    // We are successfully setup as type plugin, but have not spawned an
    // instance due to a lack of a frame.
    AsyncStartPluginInstance();
    return NS_OK;
  }

  // Otherwise, we're just changing frames
  // Set up relationship between instance owner and frame.
  nsPluginFrame *objFrame = static_cast<nsPluginFrame*>(aFrame);
  mInstanceOwner->SetFrame(objFrame);

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

NS_IMETHODIMP
nsObjectLoadingContent::GetBaseURI(nsIURI **aResult)
{
  NS_IF_ADDREF(*aResult = mBaseURI);
  return NS_OK;
}

// nsIInterfaceRequestor
// We use a shim class to implement this so that JS consumers still
// see an interface requestor even though WebIDL bindings don't expose
// that stuff.
class ObjectInterfaceRequestorShim final : public nsIInterfaceRequestor,
                                           public nsIChannelEventSink,
                                           public nsIStreamListener
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ObjectInterfaceRequestorShim,
                                           nsIInterfaceRequestor)
  NS_DECL_NSIINTERFACEREQUESTOR
  // RefPtr<nsObjectLoadingContent> fails due to ambiguous AddRef/Release,
  // hence the ugly static cast :(
  NS_FORWARD_NSICHANNELEVENTSINK(static_cast<nsObjectLoadingContent *>
                                 (mContent.get())->)
  NS_FORWARD_NSISTREAMLISTENER  (static_cast<nsObjectLoadingContent *>
                                 (mContent.get())->)
  NS_FORWARD_NSIREQUESTOBSERVER (static_cast<nsObjectLoadingContent *>
                                 (mContent.get())->)

  explicit ObjectInterfaceRequestorShim(nsIObjectLoadingContent* aContent)
    : mContent(aContent)
  {}

protected:
  ~ObjectInterfaceRequestorShim() {}
  nsCOMPtr<nsIObjectLoadingContent> mContent;
};

NS_IMPL_CYCLE_COLLECTION(ObjectInterfaceRequestorShim, mContent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ObjectInterfaceRequestorShim)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceRequestor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ObjectInterfaceRequestorShim)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ObjectInterfaceRequestorShim)

NS_IMETHODIMP
ObjectInterfaceRequestorShim::GetInterface(const nsIID & aIID, void **aResult)
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
EventStates
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
      return EventStates();
    case eType_Null:
      switch (mFallbackType) {
        case eFallbackSuppressed:
          return NS_EVENT_STATE_SUPPRESSED;
        case eFallbackUserDisabled:
          return NS_EVENT_STATE_USERDISABLED;
        case eFallbackClickToPlay:
          return NS_EVENT_STATE_TYPE_CLICK_TO_PLAY;
        case eFallbackDisabled:
          return NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_HANDLER_DISABLED;
        case eFallbackBlocklisted:
          return NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_HANDLER_BLOCKED;
        case eFallbackCrashed:
          return NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_HANDLER_CRASHED;
        case eFallbackUnsupported:
        case eFallbackOutdated:
        case eFallbackAlternate:
          return NS_EVENT_STATE_BROKEN;
        case eFallbackVulnerableUpdatable:
          return NS_EVENT_STATE_VULNERABLE_UPDATABLE;
        case eFallbackVulnerableNoUpdate:
          return NS_EVENT_STATE_VULNERABLE_NO_UPDATE;
      }
  }
  NS_NOTREACHED("unknown type?");
  return NS_EVENT_STATE_LOADING;
}

// Returns false if mBaseURI is not acceptable for java applets.
bool
nsObjectLoadingContent::CheckJavaCodebase()
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsCOMPtr<nsIScriptSecurityManager> secMan =
    nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsINetUtil> netutil = do_GetNetUtil();
  NS_ASSERTION(thisContent && secMan && netutil, "expected interfaces");


  // Note that mBaseURI is this tag's requested base URI, not the codebase of
  // the document for security purposes
  nsresult rv = secMan->CheckLoadURIWithPrincipal(thisContent->NodePrincipal(),
                                                  mBaseURI, 0);
  if (NS_FAILED(rv)) {
    LOG(("OBJLC [%p]: Java codebase check failed", this));
    return false;
  }

  nsCOMPtr<nsIURI> principalBaseURI;
  rv = thisContent->NodePrincipal()->GetURI(getter_AddRefs(principalBaseURI));
  if (NS_FAILED(rv)) {
    NS_NOTREACHED("Failed to URI from node principal?");
    return false;
  }
  // We currently allow java's codebase to be non-same-origin, with
  // the exception of URIs that represent local files
  if (NS_URIIsLocalFile(mBaseURI) &&
      nsScriptSecurityManager::GetStrictFileOriginPolicy() &&
      !NS_RelaxStrictFileOriginPolicy(mBaseURI, principalBaseURI, true)) {
    LOG(("OBJLC [%p]: Java failed RelaxStrictFileOriginPolicy for file URI",
         this));
    return false;
  }

  return true;
}

void
nsObjectLoadingContent::MaybeRewriteYoutubeEmbed(nsIURI* aURI, nsIURI* aBaseURI, nsIURI** aOutURI)
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "Must be an instance of content");

  // We're only interested in switching out embed and object tags
  if (!thisContent->NodeInfo()->Equals(nsGkAtoms::embed) &&
      !thisContent->NodeInfo()->Equals(nsGkAtoms::object)) {
    return;
  }

  nsCOMPtr<nsIEffectiveTLDService> tldService =
    do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  // If we can't analyze the URL, just pass on through.
  if(!tldService) {
    NS_WARNING("Could not get TLD service!");
    return;
  }

  nsAutoCString currentBaseDomain;
  bool ok = NS_SUCCEEDED(tldService->GetBaseDomain(aURI, 0, currentBaseDomain));
  if (!ok) {
    // Data URIs (commonly used for things like svg embeds) won't parse
    // correctly, so just fail silently here.
    return;
  }

  // See if URL is referencing youtube
  if (!currentBaseDomain.EqualsLiteral("youtube.com")) {
    return;
  }

  // We should only rewrite URLs with paths starting with "/v/", as we shouldn't
  // touch object nodes with "/embed/" urls that already do that right thing.
  nsAutoCString path;
  aURI->GetPath(path);
  if (!StringBeginsWith(path, NS_LITERAL_CSTRING("/v/"))) {
    return;
  }

  // See if requester is planning on using the JS API.
  nsAutoCString uri;
  nsresult rv = aURI->GetSpec(uri);
  if (NS_FAILED(rv)) {
    return;
  }

  if (uri.Find("enablejsapi=1", true, 0, -1) != kNotFound) {
    Telemetry::Accumulate(Telemetry::YOUTUBE_NONREWRITABLE_EMBED_SEEN, 1);
    return;
  }

  // Some YouTube urls have parameters in path components, e.g.
  // http://youtube.com/embed/7LcUOEP7Brc&start=35. These URLs work with flash,
  // but break iframe/object embedding. If this situation occurs with rewritten
  // URLs, convert the parameters to query in order to make the video load
  // correctly as an iframe. In either case, warn about it in the
  // developer console.
  int32_t ampIndex = uri.FindChar('&', 0);
  bool replaceQuery = false;
  if (ampIndex != -1) {
    int32_t qmIndex = uri.FindChar('?', 0);
    if (qmIndex == -1 ||
        qmIndex > ampIndex) {
      replaceQuery = true;
    }
  }

  // If we've made it this far, we've got a rewritable embed. Log it in
  // telemetry.
  Telemetry::Accumulate(Telemetry::YOUTUBE_REWRITABLE_EMBED_SEEN, 1);

  // If we're pref'd off, return after telemetry has been logged.
  if (!Preferences::GetBool(kPrefYoutubeRewrite)) {
    return;
  }

  nsAutoString utf16OldURI = NS_ConvertUTF8toUTF16(uri);
  // If we need to convert the URL, it means an ampersand comes first.
  // Use the index we found earlier.
  if (replaceQuery) {
    // Replace question marks with ampersands.
    uri.ReplaceChar('?', '&');
    // Replace the first ampersand with a question mark.
    uri.SetCharAt('?', ampIndex);
  }
  // Switch out video access url formats, which should possibly allow HTML5
  // video loading.
  uri.ReplaceSubstring(NS_LITERAL_CSTRING("/v/"),
                       NS_LITERAL_CSTRING("/embed/"));
  nsAutoString utf16URI = NS_ConvertUTF8toUTF16(uri);
  rv = nsContentUtils::NewURIWithDocumentCharset(aOutURI,
                                                 utf16URI,
                                                 thisContent->OwnerDoc(),
                                                 aBaseURI);
  if (NS_FAILED(rv)) {
    return;
  }
  const char16_t* params[] = { utf16OldURI.get(), utf16URI.get() };
  const char* msgName;
  // If there's no query to rewrite, just notify in the developer console
  // that we're changing the embed.
  if (!replaceQuery) {
    msgName = "RewriteYouTubeEmbed";
  } else {
    msgName = "RewriteYouTubeEmbedPathParams";
  }
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Plugins"),
                                  thisContent->OwnerDoc(),
                                  nsContentUtils::eDOM_PROPERTIES,
                                  msgName,
                                  params, ArrayLength(params));
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

  nsContentPolicyType contentPolicyType = GetContentPolicyType();

  *aContentPolicy = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(contentPolicyType,
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
    LOG(("OBJLC [%p]: Content policy denied load of %s",
         this, mURI->GetSpecOrDefault().get()));
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
      objectType = nsIContentPolicy::TYPE_INTERNAL_IMAGE;
      break;
    case eType_Document:
      objectType = nsIContentPolicy::TYPE_DOCUMENT;
      break;
    case eType_Plugin:
      objectType = GetContentPolicyType();
      break;
    default:
      NS_NOTREACHED("Calling checkProcessPolicy with a unloadable type");
      return false;
  }

  *aContentPolicy = nsIContentPolicy::ACCEPT;
  nsresult rv =
    NS_CheckContentProcessPolicy(objectType,
                                 mURI ? mURI : mBaseURI,
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
nsObjectLoadingContent::UpdateObjectParameters(bool aJavaURI)
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "Must be an instance of content");

  uint32_t caps = GetCapabilities();
  LOG(("OBJLC [%p]: Updating object parameters", this));

  nsresult rv;
  nsAutoCString newMime;
  nsAutoString typeAttr;
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

  if (aJavaURI || thisContent->NodeInfo()->Equals(nsGkAtoms::applet)) {
    nsAdoptingCString javaMIME = Preferences::GetCString(kPrefJavaMIME);
    newMime = javaMIME;
    NS_ASSERTION(IsJavaMIME(newMime),
                 "plugin.mime.java should be recognized as java");
    isJava = true;
  } else {
    nsAutoString rawTypeAttr;
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, rawTypeAttr);
    if (!rawTypeAttr.IsEmpty()) {
      typeAttr = rawTypeAttr;
      CopyUTF16toUTF8(rawTypeAttr, newMime);
      isJava = IsJavaMIME(newMime);
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
      nsAdoptingCString javaMIME = Preferences::GetCString(kPrefJavaMIME);
      NS_ASSERTION(IsJavaMIME(javaMIME),
                   "plugin.mime.java should be recognized as java");
      RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
      if (StringBeginsWith(classIDAttr, NS_LITERAL_STRING("java:")) &&
          pluginHost &&
          pluginHost->HavePluginForType(javaMIME)) {
        newMime = javaMIME;
        isJava = true;
      } else {
        // XXX(johns): Our de-facto behavior since forever was to refuse to load
        // Objects who don't have a classid we support, regardless of other type
        // or uri info leads to a valid plugin.
        newMime.Truncate();
        stateInvalid = true;
      }
    }
  }

  ///
  /// Codebase
  ///

  nsAutoString codebaseStr;
  nsCOMPtr<nsIURI> docBaseURI = thisContent->GetBaseURI();
  bool hasCodebase = thisContent->HasAttr(kNameSpaceID_None, nsGkAtoms::codebase);
  if (hasCodebase)
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::codebase, codebaseStr);


  // Java wants the codebase attribute even if it occurs in <param> tags
  if (isJava) {
    // Find all <param> tags that are nested beneath us, but not beneath another
    // object/applet tag.
    nsTArray<MozPluginParameter> params;
    GetNestedParams(params, false);
    for (uint32_t i = 0; i < params.Length(); i++) {
      if (params[i].mName.EqualsIgnoreCase("codebase")) {
        hasCodebase = true;
        codebaseStr = params[i].mValue;
      }
    }
  }

  if (isJava && hasCodebase && codebaseStr.IsEmpty()) {
    // Java treats codebase="" as "/"
    codebaseStr.Assign('/');
    // XXX(johns): This doesn't cover the case of "https:" which java would
    //             interpret as "https:///" but we interpret as this document's
    //             URI but with a changed scheme.
  } else if (isJava && !hasCodebase) {
    // Java expects a directory as the codebase, or else it will construct
    // relative URIs incorrectly :(
    codebaseStr.Assign('.');
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

  // If we failed to build a valid URI, use the document's base URI
  if (!newBaseURI) {
    newBaseURI = docBaseURI;
  }

  ///
  /// URI
  ///

  nsAutoString uriStr;
  // Different elements keep this in various locations
  if (isJava) {
    // Applet tags and embed/object with explicit java MIMEs have src/data
    // attributes that are not meant to be parsed as URIs or opened by the
    // browser -- act as if they are null. (Setting these attributes triggers a
    // force-load, so tracking the old value to determine if they have changed
    // is not necessary.)
  } else if (thisContent->NodeInfo()->Equals(nsGkAtoms::object)) {
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, uriStr);
  } else if (thisContent->NodeInfo()->Equals(nsGkAtoms::embed)) {
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, uriStr);
  } else {
    // Applet tags should always have a java MIME type at this point
    NS_NOTREACHED("Unrecognized plugin-loading tag");
  }

  mRewrittenYoutubeEmbed = false;
  // Note that the baseURI changing could affect the newURI, even if uriStr did
  // not change.
  if (!uriStr.IsEmpty()) {
    rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(newURI),
                                                   uriStr,
                                                   thisContent->OwnerDoc(),
                                                   newBaseURI);
    nsCOMPtr<nsIURI> rewrittenURI;
    MaybeRewriteYoutubeEmbed(newURI,
                             newBaseURI,
                             getter_AddRefs(rewrittenURI));
    if (rewrittenURI) {
      newURI = rewrittenURI;
      mRewrittenYoutubeEmbed = true;
      newMime = NS_LITERAL_CSTRING("text/html");
    }

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
    if (!isJava && IsJavaMIME(newMime)) {
      return UpdateObjectParameters(true);
    }
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
  // If we have a channel and are type loading, as opposed to having an existing
  // channel for a previous load.
  bool newChannel = useChannel && mType == eType_Loading;

  if (newChannel && mChannel) {
    nsCString channelType;
    rv = mChannel->GetContentType(channelType);
    if (NS_FAILED(rv)) {
      NS_NOTREACHED("GetContentType failed");
      stateInvalid = true;
      channelType.Truncate();
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

    ObjectType typeHint = newMime.IsEmpty() ?
                          eType_Null : GetTypeOfContent(newMime);

    //
    // In order of preference:
    //
    // 1) Perform typemustmatch check.
    //    If check is sucessful use type without further checks.
    //    If check is unsuccessful set stateInvalid to true
    // 2) Use our type hint if it matches a plugin
    // 3) If we have eAllowPluginSkipChannel, use the uri file extension if
    //    it matches a plugin
    // 4) If the channel returns a binary stream type:
    //    4a) If we have a type non-null non-document type hint, use that
    //    4b) If the uri file extension matches a plugin type, use that
    // 5) Use the channel type

    bool overrideChannelType = false;
    if (thisContent->HasAttr(kNameSpaceID_None, nsGkAtoms::typemustmatch)) {
      if (!typeAttr.LowerCaseEqualsASCII(channelType.get())) {
        stateInvalid = true;
      }
    } else if (typeHint == eType_Plugin) {
      LOG(("OBJLC [%p]: Using plugin type hint in favor of any channel type",
           this));
      overrideChannelType = true;
    } else if ((caps & eAllowPluginSkipChannel) &&
               IsPluginEnabledByExtension(newURI, newMime)) {
      LOG(("OBJLC [%p]: Using extension as type hint for "
           "eAllowPluginSkipChannel tag (%s)", this, newMime.get()));
      overrideChannelType = true;
    } else if (binaryChannelType &&
               typeHint != eType_Null && typeHint != eType_Document) {
      LOG(("OBJLC [%p]: Using type hint in favor of binary channel type",
           this));
      overrideChannelType = true;
    } else if (binaryChannelType &&
               IsPluginEnabledByExtension(newURI, newMime)) {
      LOG(("OBJLC [%p]: Using extension as type hint for binary channel (%s)",
           this, newMime.get()));
      overrideChannelType = true;
    }

    if (overrideChannelType) {
      // Set the type we'll use for dispatch on the channel.  Otherwise we could
      // end up trying to dispatch to a nsFrameLoader, which will complain that
      // it couldn't find a way to handle application/octet-stream
      nsAutoCString parsedMime, dummy;
      NS_ParseResponseContentType(newMime, parsedMime, dummy);
      if (!parsedMime.IsEmpty()) {
        mChannel->SetContentType(parsedMime);
      }
    } else {
      newMime = channelType;
      if (IsJavaMIME(newMime)) {
        // Java does not load with a channel, and being java retroactively
        // changes how we may have interpreted the codebase to construct this
        // URI above.  Because the behavior here is more or less undefined, play
        // it safe and reject the load.
        LOG(("OBJLC [%p]: Refusing to load with channel with java MIME",
             this));
        stateInvalid = true;
      }
    }
  } else if (newChannel) {
    LOG(("OBJLC [%p]: We failed to open a channel, marking invalid", this));
    stateInvalid = true;
  }

  ///
  /// Determine final type
  ///
  // In order of preference:
  //  1) If we have attempted channel load, or set stateInvalid above, the type
  //     is always null (fallback)
  //  2) If we have a loaded channel, we grabbed its mimeType above, use that
  //     type.
  //  3) If we have a plugin type and no URI, use that type.
  //  4) If we have a plugin type and eAllowPluginSkipChannel, use that type.
  //  5) if we have a URI, set type to loading to indicate we'd need a channel
  //     to proceed.
  //  6) Otherwise, type null to indicate unloadable content (fallback)
  //

  if (stateInvalid) {
    newType = eType_Null;
    newMime.Truncate();
  } else if (newChannel) {
    // If newChannel is set above, we considered it in setting newMime
    newType = GetTypeOfContent(newMime);
    LOG(("OBJLC [%p]: Using channel type", this));
  } else if (((caps & eAllowPluginSkipChannel) || !newURI) &&
             GetTypeOfContent(newMime) == eType_Plugin) {
    newType = eType_Plugin;
    LOG(("OBJLC [%p]: Plugin type with no URI, skipping channel load", this));
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
  /// Handle existing channels
  ///

  if (useChannel && newType == eType_Loading) {
    // We decided to use a channel, and also that the previous channel is still
    // usable, so re-use the existing values.
    newType = mType;
    newMime = mContentType;
    newURI = mURI;
  } else if (useChannel && !newChannel) {
    // We have an existing channel, but did not decide to use one.
    retval = (ParameterUpdateFlags)(retval | eParamChannelChanged);
    useChannel = false;
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

  // If we decided to keep using info from an old channel, but also that state
  // changed, we need to invalidate it.
  if (useChannel && !newChannel && (retval & eParamStateChanged)) {
    mType = eType_Loading;
    retval = (ParameterUpdateFlags)(retval | eParamChannelChanged);
  }

  return retval;
}

// Used by PluginDocument to kick off our initial load from the already-opened
// channel.
NS_IMETHODIMP
nsObjectLoadingContent::InitializeFromChannel(nsIRequest *aChannel)
{
  LOG(("OBJLC [%p] InitializeFromChannel: %p", this, aChannel));
  if (mType != eType_Loading || mChannel) {
    // We could technically call UnloadObject() here, if consumers have a valid
    // reason for wanting to call this on an already-loaded tag.
    NS_NOTREACHED("Should not have begun loading at this point");
    return NS_ERROR_UNEXPECTED;
  }

  // Because we didn't open this channel from an initial LoadObject, we'll
  // update our parameters now, so the OnStartRequest->LoadObject doesn't
  // believe our src/type suddenly changed.
  UpdateObjectParameters();
  // But we always want to load from a channel, in this case.
  mType = eType_Loading;
  mChannel = do_QueryInterface(aChannel);
  NS_ASSERTION(mChannel, "passed a request that is not a channel");

  // OnStartRequest will now see we have a channel in the loading state, and
  // call into LoadObject. There's a possibility LoadObject will decide not to
  // load anything from a channel - it will call CloseChannel() in that case.
  return NS_OK;
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
  EventStates oldState = ObjectState();
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
    fallbackType = eFallbackUnsupported;
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

  // Determine what's going on with our channel.
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
    bool allowLoad = true;
    if (IsJavaMIME(mContentType)) {
      allowLoad = CheckJavaCodebase();
    }
    int16_t contentPolicy = nsIContentPolicy::ACCEPT;
    // If mChannelLoaded is set we presumably already passed load policy
    // If mType == eType_Loading then we call OpenChannel() which internally
    // creates a new channel and calls asyncOpen2() on that channel which
    // then enforces content policy checks.
    if (allowLoad && mURI && !mChannelLoaded && mType != eType_Loading) {
      allowLoad = CheckLoadPolicy(&contentPolicy);
    }
    // If we're loading a type now, check ProcessPolicy. Note that we may check
    // both now in the case of plugins whose type is determined before opening a
    // channel.
    if (allowLoad && mType != eType_Loading) {
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

  // Don't allow view-source scheme.
  // view-source is the only scheme to which this applies at the moment due to
  // potential timing attacks to read data from cross-origin documents. If this
  // widens we should add a protocol flag for whether the scheme is only allowed
  // in top and use something like nsNetUtil::NS_URIChainHasFlags.
  if (mType != eType_Null) {
    nsCOMPtr<nsIURI> tempURI = mURI;
    nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(tempURI);
    while (nestedURI) {
      // view-source should always be an nsINestedURI, loop and check the
      // scheme on this and all inner URIs that are also nested URIs.
      bool isViewSource = false;
      rv = tempURI->SchemeIs("view-source", &isViewSource);
      if (NS_FAILED(rv) || isViewSource) {
        LOG(("OBJLC [%p]: Blocking as effective URI has view-source scheme",
             this));
        mType = eType_Null;
        break;
      }

      nestedURI->GetInnerURI(getter_AddRefs(tempURI));
      nestedURI = do_QueryInterface(tempURI);
    }
  }

  // Items resolved as Image/Document are no candidates for content blocking,
  // as well as invalid plugins (they will not have the mContentType set).
  if ((mType == eType_Null || mType == eType_Plugin) && ShouldBlockContent()) {
    LOG(("OBJLC [%p]: Enable content blocking", this));
    mType = eType_Loading;
  }

  // If we're a plugin but shouldn't start yet, load fallback with
  // reason click-to-play instead. Items resolved as Image/Document
  // will not be checked for previews, as well as invalid plugins
  // (they will not have the mContentType set).
  FallbackType clickToPlayReason;
  if (!mActivated && (mType == eType_Null || mType == eType_Plugin) &&
      !ShouldPlay(clickToPlayReason, false)) {
    LOG(("OBJLC [%p]: Marking plugin as click-to-play", this));
    mType = eType_Null;
    fallbackType = clickToPlayReason;
  }

  if (!mActivated && mType == eType_Plugin) {
    // Object passed ShouldPlay, so it should be considered
    // activated until it changes content type
    LOG(("OBJLC [%p]: Object implicitly activated", this));
    mActivated = true;
  }

  // Sanity check: We shouldn't have any loaded resources, pending events, or
  // a final listener at this point
  if (mFrameLoader || mPendingInstantiateEvent || mInstanceOwner ||
      mPendingCheckPluginStopEvent || mFinalListener)
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


  // Cache the current attributes and parameters.
  if (mType == eType_Plugin || mType == eType_Null) {
    rv = BuildParametersArray();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We don't set mFinalListener until OnStartRequest has been called, to
  // prevent re-entry ugliness with CloseChannel()
  nsCOMPtr<nsIStreamListener> finalListener;
  // If we decide to synchronously spawn a plugin, we do it after firing
  // notifications to avoid re-entry causing notifications to fire out of order.
  bool doSpawnPlugin = false;
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

        // We'll handle this below
        doSpawnPlugin = true;
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
                                           /* aOpener = */ nullptr,
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
  }

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

    // Don't fire error events if we're falling back to click-to-play; instead
    // pretend like this is a really slow-loading plug-in instead.
    if (fallbackType != eFallbackClickToPlay) {
      MaybeFireErrorEvent();
    }

    if (mChannel) {
      // If we were loading with a channel but then failed over, throw it away
      CloseChannel();
    }

    // Don't try to initialize plugins or final listener below
    doSpawnPlugin = false;
    finalListener = nullptr;

    // Don't notify, as LoadFallback doesn't know of our previous state
    // (so really this is just setting mFallbackType)
    LoadFallback(fallbackType, false);
  }

  // Notify of our final state
  NotifyStateChanged(oldType, oldState, false, aNotify);
  NS_ENSURE_TRUE(mIsLoading, NS_OK);


  //
  // Spawning plugins and dispatching to the final listener may re-enter, so are
  // delayed until after we fire a notification, to prevent missing
  // notifications or firing them out of order.
  //
  // Note that we ensured that we entered into LoadObject() from
  // ::OnStartRequest above when loading with a channel.
  //

  rv = NS_OK;
  if (doSpawnPlugin) {
    rv = InstantiatePluginInstance(true);
    NS_ENSURE_TRUE(mIsLoading, NS_OK);
    // Create the final listener if we're loading with a channel. We can't do
    // this in the loading block above as it requires an instance.
    if (aLoadingChannel && NS_SUCCEEDED(rv)) {
      if (NS_SUCCEEDED(rv) && MakePluginListener()) {
        rv = mFinalListener->OnStartRequest(mChannel, nullptr);
        if (NS_FAILED(rv)) {
          // Plugins can reject their initial stream, but continue to run.
          CloseChannel();
          NS_ENSURE_TRUE(mIsLoading, NS_OK);
          rv = NS_OK;
        }
      }
    }
  } else if (finalListener) {
    NS_ASSERTION(mType != eType_Null && mType != eType_Loading,
                 "We should not have a final listener with a non-loaded type");
    mFinalListener = finalListener;
    rv = finalListener->OnStartRequest(mChannel, nullptr);
  }

  if (NS_FAILED(rv) && mIsLoading) {
    // Since we've already notified of our transition, we can just Unload and
    // call LoadFallback (which will notify again)
    mType = eType_Null;
    UnloadObject(false);
    NS_ENSURE_TRUE(mIsLoading, NS_OK);
    CloseChannel();
    LoadFallback(fallbackType, true);
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
      // mFinalListener is only set by LoadObject after OnStartRequest, or
      // by OnStartRequest in the case of late-opened plugin streams
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

  nsresult rv;
  mChannel = nullptr;

  // E.g. mms://
  if (!mURI || !CanHandleURI(mURI)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsILoadGroup> group = doc->GetDocumentLoadGroup();
  nsCOMPtr<nsIChannel> chan;
  RefPtr<ObjectInterfaceRequestorShim> shim =
    new ObjectInterfaceRequestorShim(this);

  bool isSandBoxed = doc->GetSandboxFlags() & SANDBOXED_ORIGIN;
  bool inherit = nsContentUtils::ChannelShouldInheritPrincipal(thisContent->NodePrincipal(),
                                                               mURI,
                                                               true,   // aInheritForAboutBlank
                                                               false); // aForceInherit
  nsSecurityFlags securityFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;
  if (inherit) {
    securityFlags |= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }
  if (isSandBoxed) {
    securityFlags |= nsILoadInfo::SEC_SANDBOXED;
  }

  nsContentPolicyType contentPolicyType = GetContentPolicyType();

  rv = NS_NewChannel(getter_AddRefs(chan),
                     mURI,
                     thisContent,
                     securityFlags,
                     contentPolicyType,
                     group, // aLoadGroup
                     shim,  // aCallbacks
                     nsIChannel::LOAD_CALL_CONTENT_SNIFFERS |
                     nsIChannel::LOAD_CLASSIFY_URI |
                     nsIChannel::LOAD_BYPASS_SERVICE_WORKER);
  NS_ENSURE_SUCCESS(rv, rv);
  if (inherit) {
    nsCOMPtr<nsILoadInfo> loadinfo = chan->GetLoadInfo();
    loadinfo->SetPrincipalToInherit(thisContent->NodePrincipal());
  }

  // Referrer
  nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(chan));
  if (httpChan) {
    httpChan->SetReferrerWithPolicy(doc->GetDocumentURI(),
                                    doc->GetReferrerPolicy());

    // Set the initiator type
    nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChan));
    if (timedChannel) {
      timedChannel->SetInitiatorType(thisContent->LocalName());
    }
  }

  nsCOMPtr<nsIScriptChannel> scriptChannel = do_QueryInterface(chan);
  if (scriptChannel) {
    // Allow execution against our context if the principals match
    scriptChannel->SetExecutionPolicy(nsIScriptChannel::EXECUTE_NORMAL);
  }

  // AsyncOpen2 can fail if a file does not exist.
  rv = chan->AsyncOpen2(shim);
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
         eSupportDocuments;
}

void
nsObjectLoadingContent::DestroyContent()
{
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  if (mInstanceOwner || mInstantiating) {
    QueueCheckPluginStopEvent();
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

  // InstantiatePluginInstance checks this after re-entrant calls and aborts if
  // it was cleared from under it
  mInstantiating = false;

  mScriptRequested = false;

  if (mIsStopping) {
    // The protochain is normally thrown out after a plugin stops, but if we
    // re-enter while stopping a plugin and try to load something new, we need
    // to throw away the old protochain in the nested unload.
    TeardownProtoChain();
    mIsStopping = false;
  }

  mCachedAttributes.Clear();
  mCachedParameters.Clear();

  // This call should be last as it may re-enter
  StopPluginInstance();
}

void
nsObjectLoadingContent::NotifyStateChanged(ObjectType aOldType,
                                           EventStates aOldState,
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

  nsIDocument* doc = thisContent->GetComposedDoc();
  if (!doc) {
    return; // Nothing to do
  }

  EventStates newState = ObjectState();

  if (newState != aOldState) {
    NS_ASSERTION(thisContent->IsInComposedDoc(), "Something is confused");
    // This will trigger frame construction
    EventStates changedBits = aOldState ^ newState;

    {
      nsAutoScriptBlocker scriptBlocker;
      doc->ContentStateChanged(thisContent, changedBits);
    }
    if (aSync) {
      NS_ASSERTION(InActiveDocument(thisContent), "Something is confused");
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

  // Faking support of the PDF content as a document for EMBED tags
  // when internal PDF viewer is enabled.
  if (aMIMEType.LowerCaseEqualsLiteral("application/pdf") &&
      nsContentUtils::IsPDFJSEnabled()) {
    return eType_Document;
  }

  // Faking support of the SWF content as a document for EMBED tags
  // when internal SWF player is enabled.
  if (aMIMEType.LowerCaseEqualsLiteral("application/x-shockwave-flash") &&
      nsContentUtils::IsSWFPlayerEnabled()) {
    return eType_Document;
  }

  if ((caps & eSupportDocuments) && IsSupportedDocument(aMIMEType)) {
    return eType_Document;
  }

  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  if ((caps & eSupportPlugins) &&
      pluginHost &&
      pluginHost->HavePluginForType(aMIMEType, nsPluginHost::eExcludeNone)) {
    // ShouldPlay will handle checking for disabled plugins
    return eType_Plugin;
  }

  return eType_Null;
}

nsPluginFrame*
nsObjectLoadingContent::GetExistingFrame()
{
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsIFrame* frame = thisContent->GetPrimaryFrame();
  nsIObjectFrame* objFrame = do_QueryFrame(frame);
  return static_cast<nsPluginFrame*>(objFrame);
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
    nsFrameLoader* fl = nsFrameLoader::Create(content->AsElement(), nullptr, false);
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
nsObjectLoadingContent::PluginDestroyed()
{
  // Called when our plugin is destroyed from under us, usually when reloading
  // plugins in plugin host. Invalidate instance owner / prototype but otherwise
  // don't take any action.
  TeardownProtoChain();
  if (mInstanceOwner) {
    mInstanceOwner->Destroy();
    mInstanceOwner = nullptr;
  }
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

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

#ifdef XP_MACOSX
  HTMLObjectElement::HandlePluginCrashed(thisContent->AsElement());
#endif

  PluginDestroyed();

  // Switch to fallback/crashed state, notify
  LoadFallback(eFallbackCrashed, true);

  // send nsPluginCrashedEvent

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

nsresult
nsObjectLoadingContent::ScriptRequestPluginInstance(JSContext* aCx,
                                                    nsNPAPIPluginInstance **aResult)
{
  // The below methods pull the cx off the stack, so make sure they match.
  //
  // NB: Sometimes there's a null cx on the stack, in which case |cx| is the
  // safe JS context. But in that case, IsCallerChrome() will return true,
  // so the ensuing expression is short-circuited.
  MOZ_ASSERT_IF(nsContentUtils::GetCurrentJSContext(),
                aCx == nsContentUtils::GetCurrentJSContext());
  bool callerIsContentJS = (nsContentUtils::GetCurrentJSContext() &&
                            !nsContentUtils::IsCallerChrome() &&
                            !nsContentUtils::IsCallerContentXBL());

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  *aResult = nullptr;

  // The first time content script attempts to access placeholder content, fire
  // an event.  Fallback types >= eFallbackClickToPlay are plugin-replacement
  // types, see header.
  if (callerIsContentJS && !mScriptRequested &&
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
  } else if (callerIsContentJS && mType == eType_Plugin && !mInstanceOwner &&
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
  mozilla::Unused << kungFuURIGrip; // This URI is not referred to within this function
  nsCString contentType(mContentType);
  return InstantiatePluginInstance();
}

NS_IMETHODIMP
nsObjectLoadingContent::AsyncStartPluginInstance()
{
  // OK to have an instance already or a pending spawn.
  if (mInstanceOwner || mPendingInstantiateEvent) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsIDocument* doc = thisContent->OwnerDoc();
  if (doc->IsStaticDocument() || doc->IsBeingUsedAsImage()) {
    return NS_OK;
  }

  nsCOMPtr<nsIRunnable> event = new nsAsyncInstantiateEvent(this);
  nsresult rv = NS_DispatchToCurrentThread(event);
  if (NS_SUCCEEDED(rv)) {
    // Track pending events
    mPendingInstantiateEvent = event;
  }

  return rv;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetSrcURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = GetSrcURI());
  return NS_OK;
}

static bool
DoDelayedStop(nsPluginInstanceOwner* aInstanceOwner,
              nsObjectLoadingContent* aContent,
              bool aDelayedStop)
{
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
  EventStates oldState = ObjectState();
  ObjectType oldType = mType;

  NS_ASSERTION(!mInstanceOwner && !mFrameLoader && !mChannel,
               "LoadFallback called with loaded content");

  //
  // Fixup mFallbackType
  //
  nsCOMPtr<nsIContent> thisContent =
  do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "must be a content");

  if (!thisContent->IsHTMLElement() || mContentType.IsEmpty()) {
    // Don't let custom fallback handlers run outside HTML, tags without a
    // determined type should always just be alternate content
    aType = eFallbackAlternate;
  }

  // We'll set this to null no matter what now, doing it here means we'll load
  // child embeds as we find them in the upcoming loop.
  mType = eType_Null;

  // Do a breadth-first traverse of node tree with the current element as root,
  // looking for the first embed we can find.
  nsTArray<nsINodeList*> childNodes;
  if ((thisContent->IsHTMLElement(nsGkAtoms::object) ||
       thisContent->IsHTMLElement(nsGkAtoms::applet)) &&
      (aType == eFallbackUnsupported ||
       aType == eFallbackDisabled ||
       aType == eFallbackBlocklisted ||
       aType == eFallbackAlternate))
  {
    for (nsIContent* child = thisContent->GetFirstChild(); child;
         child = child->GetNextNode(thisContent)) {
      if (aType != eFallbackAlternate &&
          !child->IsHTMLElement(nsGkAtoms::param) &&
          nsStyleUtil::IsSignificantChild(child, true, false)) {
        aType = eFallbackAlternate;
      }
      if (child->IsHTMLElement(nsGkAtoms::embed) &&
          thisContent->IsHTMLElement(nsGkAtoms::object)) {
        HTMLSharedObjectElement* object = static_cast<HTMLSharedObjectElement*>(child);
        if (object) {
          object->StartObjectLoad(true, true);
        }
      }
    }
  }

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
  // DoStopPlugin can process events -- There may be pending
  // CheckPluginStopEvent events which can drop in underneath us and destroy the
  // instance we are about to destroy. We prevent that with the mPluginStopping
  // flag.  (aForcedReentry is only true from the callback of an earlier delayed
  // stop)
  if (mIsStopping && !aForcedReentry) {
    return;
  }
  mIsStopping = true;

  RefPtr<nsPluginInstanceOwner> kungFuDeathGrip(aInstanceOwner);
  RefPtr<nsNPAPIPluginInstance> inst;
  aInstanceOwner->GetInstance(getter_AddRefs(inst));
  if (inst) {
    if (DoDelayedStop(aInstanceOwner, this, aDelayedStop)) {
      return;
    }

#if defined(XP_MACOSX)
    aInstanceOwner->HidePluginWindow();
#endif

    RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
    NS_ASSERTION(pluginHost, "No plugin host?");
    pluginHost->StopPluginInstance(inst);
  }

  aInstanceOwner->Destroy();

  // If we re-enter in plugin teardown UnloadObject will tear down the
  // protochain -- the current protochain could be from a new, unrelated, load.
  if (!mIsStopping) {
    LOG(("OBJLC [%p]: Re-entered in plugin teardown", this));
    return;
  }

  TeardownProtoChain();
  mIsStopping = false;
}

NS_IMETHODIMP
nsObjectLoadingContent::StopPluginInstance()
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);
  // Clear any pending events
  mPendingInstantiateEvent = nullptr;
  mPendingCheckPluginStopEvent = nullptr;

  // If we're currently instantiating, clearing this will cause
  // InstantiatePluginInstance's re-entrance check to destroy the created plugin
  mInstantiating = false;

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

  // We detach the instance owner's frame before destruction, but don't destroy
  // the instance owner until the plugin is stopped.
  mInstanceOwner->SetFrame(nullptr);

  bool delayedStop = false;
#ifdef XP_WIN
  // Force delayed stop for Real plugin only; see bug 420886, 426852.
  RefPtr<nsNPAPIPluginInstance> inst;
  mInstanceOwner->GetInstance(getter_AddRefs(inst));
  if (inst) {
    const char* mime = nullptr;
    if (NS_SUCCEEDED(inst->GetMIMEType(&mime)) && mime) {
      if (nsPluginHost::GetSpecialType(nsDependentCString(mime)) ==
          nsPluginHost::eSpecialType_RealPlayer) {
        delayedStop = true;
      }
    }
  }
#endif

  RefPtr<nsPluginInstanceOwner> ownerGrip(mInstanceOwner);
  mInstanceOwner = nullptr;

  // This can/will re-enter
  DoStopPlugin(ownerGrip, delayedStop);

  return NS_OK;
}

void
nsObjectLoadingContent::NotifyContentObjectWrapper()
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> obj(cx, thisContent->GetWrapper());
  if (!obj) {
    // Nothing to do here if there's no wrapper for mContent. The proto
    // chain will be fixed appropriately when the wrapper is created.
    return;
  }

  SetupProtoChain(cx, obj);
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

  // If we're in a click-to-play state, reload.
  // Fallback types >= eFallbackClickToPlay are plugin-replacement types, see
  // header
  if (mType == eType_Null && mFallbackType >= eFallbackClickToPlay) {
    return LoadObject(true, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::Reload(bool aClearActivation)
{
  if (aClearActivation) {
    mActivated = false;
  }

  return LoadObject(true, true);
}

NS_IMETHODIMP
nsObjectLoadingContent::GetActivated(bool *aActivated)
{
  *aActivated = Activated();
  return NS_OK;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetPluginFallbackType(uint32_t* aPluginFallbackType)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);
  *aPluginFallbackType = mFallbackType;
  return NS_OK;
}

uint32_t
nsObjectLoadingContent::DefaultFallbackType()
{
  FallbackType reason;
  bool go = ShouldPlay(reason, true);
  if (go) {
    return PLUGIN_ACTIVE;
  }
  return reason;
}

NS_IMETHODIMP
nsObjectLoadingContent::GetRunID(uint32_t* aRunID)
{
  if (NS_WARN_IF(!nsContentUtils::IsCallerChrome())) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(!aRunID)) {
    return NS_ERROR_INVALID_POINTER;
  }
  if (!mHasRunID) {
    // The plugin instance must not have a run ID, so we must
    // be running the plugin in-process.
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  *aRunID = mRunID;
  return NS_OK;
}

static bool sPrefsInitialized;
static uint32_t sSessionTimeoutMinutes;
static uint32_t sPersistentTimeoutDays;

bool
nsObjectLoadingContent::ShouldBlockContent()
{
  if (mContentBlockingDisabled || !mURI)
    return false;

  if (!IsFlashMIME(mContentType) || !Preferences::GetBool(kPrefBlockURIs)) {
    mContentBlockingDisabled = true;
    return false;
  }

  return true;
}

bool
nsObjectLoadingContent::ShouldPlay(FallbackType &aReason, bool aIgnoreCurrentType)
{
  nsresult rv;

  if (!sPrefsInitialized) {
    Preferences::AddUintVarCache(&sSessionTimeoutMinutes,
                                 "plugin.sessionPermissionNow.intervalInMinutes", 60);
    Preferences::AddUintVarCache(&sPersistentTimeoutDays,
                                 "plugin.persistentPermissionAlways.intervalInDays", 90);
    sPrefsInitialized = true;
  }

  if (BrowserTabsRemoteAutostart()) {
    bool shouldLoadInParent = nsPluginHost::ShouldLoadTypeInParent(mContentType);
    bool inParent = XRE_IsParentProcess();

    if (shouldLoadInParent != inParent) {
      // Plugins need to be locked to either the parent process or the content
      // process. If a plugin is locked to one process type, it can't be used in
      // the other. Otherwise we'll get hangs.
      aReason = eFallbackDisabled;
      return false;
    }
  }

  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();

  // at this point if it's not a plugin, we let it play/fallback
  if (!aIgnoreCurrentType && mType != eType_Plugin) {
    return true;
  }

  // Order of checks:
  // * Assume a default of click-to-play
  // * If globally disabled, per-site permissions cannot override.
  // * If blocklisted, override the reason with the blocklist reason
  // * Check per-site permissions and follow those if specified.
  // * Honor per-plugin disabled permission
  // * Blocklisted plugins are forced to CtP
  // * Check per-plugin permission and follow that.

  aReason = eFallbackClickToPlay;

  uint32_t enabledState = nsIPluginTag::STATE_DISABLED;
  pluginHost->GetStateForType(mContentType, nsPluginHost::eExcludeNone,
                              &enabledState);
  if (nsIPluginTag::STATE_DISABLED == enabledState) {
    aReason = eFallbackDisabled;
    return false;
  }

  // Before we check permissions, get the blocklist state of this plugin to set
  // the fallback reason correctly. In the content process this will involve
  // an ipc call to chrome.
  uint32_t blocklistState = nsIBlocklistService::STATE_BLOCKED;
  pluginHost->GetBlocklistStateForType(mContentType,
                                       nsPluginHost::eExcludeNone,
                                       &blocklistState);
  if (blocklistState == nsIBlocklistService::STATE_BLOCKED) {
    // no override possible
    aReason = eFallbackBlocklisted;
    return false;
  }

  if (blocklistState == nsIBlocklistService::STATE_VULNERABLE_UPDATE_AVAILABLE) {
    aReason = eFallbackVulnerableUpdatable;
  }
  else if (blocklistState == nsIBlocklistService::STATE_VULNERABLE_NO_UPDATE) {
    aReason = eFallbackVulnerableNoUpdate;
  }

  // Check the permission manager for permission based on the principal of
  // the toplevel content.

  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIObjectLoadingContent*>(this));
  MOZ_ASSERT(thisContent);
  nsIDocument* ownerDoc = thisContent->OwnerDoc();

  nsCOMPtr<nsPIDOMWindowOuter> window = ownerDoc->GetWindow();
  if (!window) {
    return false;
  }
  nsCOMPtr<nsPIDOMWindowOuter> topWindow = window->GetTop();
  NS_ENSURE_TRUE(topWindow, false);
  nsCOMPtr<nsIDocument> topDoc = topWindow->GetDoc();
  NS_ENSURE_TRUE(topDoc, false);

  nsCOMPtr<nsIPermissionManager> permissionManager = services::GetPermissionManager();
  NS_ENSURE_TRUE(permissionManager, false);

  // For now we always say that the system principal uses click-to-play since
  // that maintains current behavior and we have tests that expect this.
  // What we really should do is disable plugins entirely in pages that use
  // the system principal, i.e. in chrome pages. That way the click-to-play
  // code here wouldn't matter at all. Bug 775301 is tracking this.
  if (!nsContentUtils::IsSystemPrincipal(topDoc->NodePrincipal())) {
    nsAutoCString permissionString;
    rv = pluginHost->GetPermissionStringForType(mContentType,
                                                nsPluginHost::eExcludeNone,
                                                permissionString);
    NS_ENSURE_SUCCESS(rv, false);
    uint32_t permission;
    rv = permissionManager->TestPermissionFromPrincipal(topDoc->NodePrincipal(),
                                                        permissionString.Data(),
                                                        &permission);
    NS_ENSURE_SUCCESS(rv, false);
    if (permission != nsIPermissionManager::UNKNOWN_ACTION) {
      uint64_t nowms = PR_Now() / 1000;
      permissionManager->UpdateExpireTime(
        topDoc->NodePrincipal(), permissionString.Data(), false,
        nowms + sSessionTimeoutMinutes * 60 * 1000,
        nowms / 1000 + uint64_t(sPersistentTimeoutDays) * 24 * 60 * 60 * 1000);
    }
    switch (permission) {
    case nsIPermissionManager::ALLOW_ACTION:
      return true;
    case nsIPermissionManager::DENY_ACTION:
      aReason = eFallbackDisabled;
      return false;
    case nsIPermissionManager::PROMPT_ACTION:
      return false;
    case nsIPermissionManager::UNKNOWN_ACTION:
      break;
    default:
      MOZ_ASSERT(false);
      return false;
    }
  }

  // No site-specific permissions. Vulnerable plugins are automatically CtP
  if (blocklistState == nsIBlocklistService::STATE_VULNERABLE_UPDATE_AVAILABLE ||
      blocklistState == nsIBlocklistService::STATE_VULNERABLE_NO_UPDATE) {
    return false;
  }

  switch (enabledState) {
  case nsIPluginTag::STATE_ENABLED:
    return true;
  case nsIPluginTag::STATE_CLICKTOPLAY:
    return false;
  }
  MOZ_CRASH("Unexpected enabledState");
}

nsIDocument*
nsObjectLoadingContent::GetContentDocument(nsIPrincipal& aSubjectPrincipal)
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  if (!thisContent->IsInComposedDoc()) {
    return nullptr;
  }

  nsIDocument *sub_doc = thisContent->OwnerDoc()->GetSubDocumentFor(thisContent);
  if (!sub_doc) {
    return nullptr;
  }

  // Return null for cross-origin contentDocument.
  if (!aSubjectPrincipal.SubsumesConsideringDomain(sub_doc->NodePrincipal())) {
    return nullptr;
  }

  return sub_doc;
}

void
nsObjectLoadingContent::LegacyCall(JSContext* aCx,
                                   JS::Handle<JS::Value> aThisVal,
                                   const Sequence<JS::Value>& aArguments,
                                   JS::MutableHandle<JS::Value> aRetval,
                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  JS::Rooted<JSObject*> obj(aCx, thisContent->GetWrapper());
  MOZ_ASSERT(obj, "How did we get called?");

  // Make sure we're not dealing with an Xray.  Our DoCall code can't handle
  // random cross-compartment wrappers, so we're going to have to wrap
  // everything up into our compartment, but that means we need to check that
  // this is not an Xray situation by hand.
  if (!JS_WrapObject(aCx, &obj)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (nsDOMClassInfo::ObjectIsNativeWrapper(aCx, obj)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  obj = thisContent->GetWrapper();
  // Now wrap things up into the compartment of "obj"
  JSAutoCompartment ac(aCx, obj);
  JS::AutoValueVector args(aCx);
  if (!args.append(aArguments.Elements(), aArguments.Length())) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (size_t i = 0; i < args.length(); i++) {
    if (!JS_WrapValue(aCx, args[i])) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
  }

  JS::Rooted<JS::Value> thisVal(aCx, aThisVal);
  if (!JS_WrapValue(aCx, &thisVal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<nsNPAPIPluginInstance> pi;
  nsresult rv = ScriptRequestPluginInstance(aCx, getter_AddRefs(pi));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  // if there's no plugin around for this object, throw.
  if (!pi) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  JS::Rooted<JSObject*> pi_obj(aCx);
  JS::Rooted<JSObject*> pi_proto(aCx);

  rv = GetPluginJSObject(aCx, obj, pi, &pi_obj, &pi_proto);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (!pi_obj) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  bool ok = JS::Call(aCx, thisVal, pi_obj, args, aRetval);
  if (!ok) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  Telemetry::Accumulate(Telemetry::PLUGIN_CALLED_DIRECTLY, true);
}

void
nsObjectLoadingContent::SetupProtoChain(JSContext* aCx,
                                        JS::Handle<JSObject*> aObject)
{
  if (mType != eType_Plugin) {
    return;
  }

  if (!nsContentUtils::IsSafeToRunScript()) {
    RefPtr<SetupProtoChainRunner> runner = new SetupProtoChainRunner(this);
    nsContentUtils::AddScriptRunner(runner);
    return;
  }

  // We get called on random compartments here for some reason
  // (perhaps because WrapObject can happen on a random compartment?)
  // so make sure to enter the compartment of aObject.
  MOZ_ASSERT(aCx == nsContentUtils::GetCurrentJSContext());

  JSAutoCompartment ac(aCx, aObject);

  RefPtr<nsNPAPIPluginInstance> pi;
  nsresult rv = ScriptRequestPluginInstance(aCx, getter_AddRefs(pi));
  if (NS_FAILED(rv)) {
    return;
  }

  if (!pi) {
    // No plugin around for this object.
    return;
  }

  JS::Rooted<JSObject*> pi_obj(aCx); // XPConnect-wrapped peer object, when we get it.
  JS::Rooted<JSObject*> pi_proto(aCx); // 'pi.__proto__'

  rv = GetPluginJSObject(aCx, aObject, pi, &pi_obj, &pi_proto);
  if (NS_FAILED(rv)) {
    return;
  }

  if (!pi_obj) {
    // Didn't get a plugin instance JSObject, nothing we can do then.
    return;
  }

  // If we got an xpconnect-wrapped plugin object, set obj's
  // prototype's prototype to the scriptable plugin.

  JS::Handle<JSObject*> my_proto = GetDOMClass(aObject)->mGetProto(aCx);
  MOZ_ASSERT(my_proto);

  // Set 'this.__proto__' to pi
  if (!::JS_SetPrototype(aCx, aObject, pi_obj)) {
    return;
  }

  if (pi_proto && js::GetObjectClass(pi_proto) != js::ObjectClassPtr) {
    // The plugin wrapper has a proto that's not Object.prototype, set
    // 'pi.__proto__.__proto__' to the original 'this.__proto__'
    if (pi_proto != my_proto && !::JS_SetPrototype(aCx, pi_proto, my_proto)) {
      return;
    }
  } else {
    // 'pi' didn't have a prototype, or pi's proto was
    // 'Object.prototype' (i.e. pi is an NPRuntime wrapped JS object)
    // set 'pi.__proto__' to the original 'this.__proto__'
    if (!::JS_SetPrototype(aCx, pi_obj, my_proto)) {
      return;
    }
  }

  // Before this proto dance the objects involved looked like this:
  //
  // this.__proto__.__proto__
  //   ^      ^         ^
  //   |      |         |__ Object.prototype
  //   |      |
  //   |      |__ WebIDL prototype (shared)
  //   |
  //   |__ WebIDL object
  //
  // pi.__proto__
  // ^      ^
  // |      |__ Object.prototype or some other object
  // |
  // |__ Plugin NPRuntime JS object wrapper
  //
  // Now, after the above prototype setup the prototype chain should
  // look like this if pi.__proto__ was Object.prototype:
  //
  // this.__proto__.__proto__.__proto__
  //   ^      ^         ^         ^
  //   |      |         |         |__ Object.prototype
  //   |      |         |
  //   |      |         |__ WebIDL prototype (shared)
  //   |      |
  //   |      |__ Plugin NPRuntime JS object wrapper
  //   |
  //   |__ WebIDL object
  //
  // or like this if pi.__proto__ was some other object:
  //
  // this.__proto__.__proto__.__proto__.__proto__
  //   ^      ^         ^         ^         ^
  //   |      |         |         |         |__ Object.prototype
  //   |      |         |         |
  //   |      |         |         |__ WebIDL prototype (shared)
  //   |      |         |
  //   |      |         |__ old pi.__proto__
  //   |      |
  //   |      |__ Plugin NPRuntime JS object wrapper
  //   |
  //   |__ WebIDL object
  //
}

// static
nsresult
nsObjectLoadingContent::GetPluginJSObject(JSContext *cx,
                                          JS::Handle<JSObject*> obj,
                                          nsNPAPIPluginInstance *plugin_inst,
                                          JS::MutableHandle<JSObject*> plugin_obj,
                                          JS::MutableHandle<JSObject*> plugin_proto)
{
  // NB: We need an AutoEnterCompartment because we can be called from
  // nsPluginFrame when the plugin loads after the JS object for our content
  // node has been created.
  JSAutoCompartment ac(cx, obj);

  if (plugin_inst) {
    plugin_inst->GetJSObject(cx, plugin_obj.address());
    if (plugin_obj) {
      if (!::JS_GetPrototype(cx, plugin_obj, plugin_proto)) {
        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  return NS_OK;
}

void
nsObjectLoadingContent::TeardownProtoChain()
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  NS_ENSURE_TRUE_VOID(thisContent->GetWrapper());

  // We don't init the AutoJSAPI with our wrapper because we don't want it
  // reporting errors to our window's onerror listeners.
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> obj(cx, thisContent->GetWrapper());
  MOZ_ASSERT(obj);

  JS::Rooted<JSObject*> proto(cx);
  JSAutoCompartment ac(cx, obj);

  // Loop over the DOM element's JS object prototype chain and remove
  // all JS objects of the class sNPObjectJSWrapperClass
  DebugOnly<bool> removed = false;
  while (obj) {
    if (!::JS_GetPrototype(cx, obj, &proto)) {
      return;
    }
    if (!proto) {
      break;
    }
    // Unwrap while checking the class - if the prototype is a wrapper for
    // an NP object, that counts too.
    if (nsNPObjWrapper::IsWrapper(js::UncheckedUnwrap(proto))) {
      // We found an NPObject on the proto chain, get its prototype...
      if (!::JS_GetPrototype(cx, proto, &proto)) {
        return;
      }

      MOZ_ASSERT(!removed, "more than one NPObject in prototype chain");
      removed = true;

      // ... and pull it out of the chain.
      ::JS_SetPrototype(cx, obj, proto);
    }

    obj = proto;
  }
}

bool
nsObjectLoadingContent::DoResolve(JSContext* aCx, JS::Handle<JSObject*> aObject,
                                  JS::Handle<jsid> aId,
                                  JS::MutableHandle<JS::PropertyDescriptor> aDesc)
{
  // We don't resolve anything; we just try to make sure we're instantiated.
  // This purposefully does not fire for chrome/xray resolves, see bug 967694

  RefPtr<nsNPAPIPluginInstance> pi;
  nsresult rv = ScriptRequestPluginInstance(aCx, getter_AddRefs(pi));
  if (NS_FAILED(rv)) {
    return mozilla::dom::Throw(aCx, rv);
  }
  return true;
}

/* static */
bool
nsObjectLoadingContent::MayResolve(jsid aId)
{
  // We can resolve anything, really.
  return true;
}

void
nsObjectLoadingContent::GetOwnPropertyNames(JSContext* aCx,
                                            nsTArray<nsString>& /* unused */,
                                            ErrorResult& aRv)
{
  // Just like DoResolve, just make sure we're instantiated.  That will do
  // the work our Enumerate hook needs to do.  This purposefully does not fire
  // for xray resolves, see bug 967694
  RefPtr<nsNPAPIPluginInstance> pi;
  aRv = ScriptRequestPluginInstance(aCx, getter_AddRefs(pi));
}

void
nsObjectLoadingContent::MaybeFireErrorEvent()
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  // Queue a task to fire an error event if we're an <object> element.  The
  // queueing is important, since then we don't have to worry about reentry.
  if (thisContent->IsHTMLElement(nsGkAtoms::object)) {
    RefPtr<AsyncEventDispatcher> loadBlockingAsyncDispatcher =
      new LoadBlockingAsyncEventDispatcher(thisContent,
                                           NS_LITERAL_STRING("error"),
                                           false, false);
    loadBlockingAsyncDispatcher->PostDOMEvent();
  }
}

// SetupProtoChainRunner implementation
nsObjectLoadingContent::SetupProtoChainRunner::SetupProtoChainRunner(
    nsObjectLoadingContent* aContent)
  : mContent(aContent)
{
}

nsObjectLoadingContent::SetupProtoChainRunner::~SetupProtoChainRunner()
{
}

NS_IMETHODIMP
nsObjectLoadingContent::SetupProtoChainRunner::Run()
{
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  nsCOMPtr<nsIContent> content;
  CallQueryInterface(mContent.get(), getter_AddRefs(content));
  JS::Rooted<JSObject*> obj(cx, content->GetWrapper());
  if (!obj) {
    // No need to set up our proto chain if we don't even have an object
    return NS_OK;
  }
  nsObjectLoadingContent* objectLoadingContent =
    static_cast<nsObjectLoadingContent*>(mContent.get());
  objectLoadingContent->SetupProtoChain(cx, obj);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsObjectLoadingContent::SetupProtoChainRunner, nsIRunnable)

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et cin sw=2 sts=2:
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
 * The Original Code is Mozilla <object> loading code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * A base class implementING nsIObjectLoadingContent for use by
 * various content nodes that want to provide plugin/document/image
 * loading functionality (eg <embed>, <object>, <applet>, etc).
 */

// Interface headers
#include "imgILoader.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIExternalProtocolHandler.h"
#include "nsIEventStateManager.h"
#include "nsIObjectFrame.h"
#include "nsIPluginDocument.h"
#include "nsIPluginHost.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamConverterService.h"
#include "nsIURILoader.h"
#include "nsIURL.h"
#include "nsIWebNavigation.h"
#include "nsIWebNavigationInfo.h"

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

// Concrete classes
#include "nsFrameLoader.h"

#include "nsObjectLoadingContent.h"

static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

#ifdef PR_LOGGING
static PRLogModuleInfo* gObjectLog = PR_NewLogModule("objlc");
#endif

#define LOG(args) PR_LOG(gObjectLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gObjectLog, PR_LOG_DEBUG)

class nsAsyncInstantiateEvent : public nsRunnable {
public:
  // This stores both the content and the frame so that Instantiate calls can be
  // avoided if the frame changed in the meantime.
  nsObjectLoadingContent *mContent;
  nsIObjectFrame*         mFrame;
  nsCString               mContentType;
  nsCOMPtr<nsIURI>        mURI;

  nsAsyncInstantiateEvent(nsObjectLoadingContent* aContent,
                          nsIObjectFrame* aFrame,
                          const nsCString& aType,
                          nsIURI* aURI)
    : mContent(aContent), mFrame(aFrame), mContentType(aType), mURI(aURI)
  {
    NS_STATIC_CAST(nsIObjectLoadingContent *, mContent)->AddRef();
  }

  ~nsAsyncInstantiateEvent()
  {
    NS_STATIC_CAST(nsIObjectLoadingContent *, mContent)->Release();
  }

  NS_IMETHOD Run();
};

NS_IMETHODIMP
nsAsyncInstantiateEvent::Run()
{
  // Check if we've been "revoked"
  if (mContent->mPendingInstantiateEvent != this)
    return NS_OK;
  mContent->mPendingInstantiateEvent = nsnull;

  // Make sure that we still have the right frame (NOTE: we don't need to check
  // the type here - GetFrame() only returns object frames, and that means we're
  // a plugin)
  // Also make sure that we still refer to the same data.
  if (mContent->GetFrame() == mFrame &&
      mContent->mURI == mURI &&
      mContent->mContentType.Equals(mContentType)) {
    if (LOG_ENABLED()) {
      nsCAutoString spec;
      if (mURI) {
        mURI->GetSpec(spec);
      }
      LOG(("OBJLC [%p]: Handling Instantiate event: Type=<%s> URI=%p<%s>\n",
           mContent, mContentType.get(), mURI.get(), spec.get()));
    }

    nsresult rv = mContent->Instantiate(mContentType, mURI);
    if (NS_FAILED(rv)) {
      mContent->Fallback(PR_TRUE);
    }
  } else {
    LOG(("OBJLC [%p]: Discarding event, data changed\n", mContent));
  }

  return NS_OK;
}

/**
 * A task for firing PluginNotFound DOM Events.
 */
class nsPluginNotFoundEvent : public nsRunnable {
public:
  nsCOMPtr<nsIContent> mContent;

  nsPluginNotFoundEvent(nsIContent* aContent)
    : mContent(aContent)
  {}

  ~nsPluginNotFoundEvent() {}

  NS_IMETHOD Run();
};

NS_IMETHODIMP
nsPluginNotFoundEvent::Run()
{
  LOG(("OBJLC []: Firing plugin not found event for content %p\n",
       mContent.get()));
  nsContentUtils::DispatchTrustedEvent(mContent->GetDocument(), mContent,
                                       NS_LITERAL_STRING("PluginNotFound"),
                                       PR_TRUE, PR_TRUE);
  return NS_OK;
}

class AutoNotifier {
  public:
    AutoNotifier(nsObjectLoadingContent* aContent, PRBool aNotify) :
      mContent(aContent), mNotify(aNotify) {
        mOldType = aContent->Type();
        mOldState = aContent->ObjectState();
    }
    ~AutoNotifier() {
      if (mNotify) {
        mContent->NotifyStateChanged(mOldType, mOldState, PR_FALSE);
      }
    }

    /**
     * Send notifications now, ignoring the value of mNotify. The new type and
     * state is saved, and the destructor will notify again if mNotify is true
     * and the values changed.
     */
    void Notify() {
      NS_ASSERTION(mNotify, "Should not notify when notify=false");

      mContent->NotifyStateChanged(mOldType, mOldState, PR_TRUE);
      mOldType = mContent->Type();
      mOldState = mContent->ObjectState();
    }

  private:
    nsObjectLoadingContent*            mContent;
    PRBool                             mNotify;
    nsObjectLoadingContent::ObjectType mOldType;
    PRInt32                            mOldState;
};

/**
 * A class that will automatically fall back if a |rv| variable has a failure
 * code when this class is destroyed. It does not notify.
 */
class AutoFallback {
  public:
    AutoFallback(nsObjectLoadingContent* aContent, const nsresult* rv)
      : mContent(aContent), mResult(rv), mTypeUnsupported(PR_FALSE) {}
    ~AutoFallback() {
      if (NS_FAILED(*mResult)) {
        LOG(("OBJLC [%p]: rv=%08x, falling back\n", mContent, *mResult));
        mContent->Fallback(PR_FALSE);
        if (mTypeUnsupported) {
          mContent->mTypeUnsupported = PR_TRUE;
        }
      }
    }

    /**
     * This function can be called to indicate that, after falling back,
     * mTypeUnsupported should be set to true.
     */
    void TypeUnsupported() {
      mTypeUnsupported = PR_TRUE;
    }
  private:
    nsObjectLoadingContent* mContent;
    const nsresult* mResult;
    PRBool mTypeUnsupported;
};

/**
 * A class that automatically sets mInstantiating to false when it goes
 * out of scope.
 */
class AutoSetInstantiatingToFalse {
  public:
    AutoSetInstantiatingToFalse(nsObjectLoadingContent* objlc) : mContent(objlc) {}
    ~AutoSetInstantiatingToFalse() { mContent->mInstantiating = PR_FALSE; }
  private:
    nsObjectLoadingContent* mContent;
};

// helper functions
static PRBool
IsSupportedImage(const nsCString& aMimeType)
{
  imgILoader* loader = nsContentUtils::GetImgLoader();
  if (!loader) {
    return PR_FALSE;
  }

  PRBool supported;
  nsresult rv = loader->SupportImageWithMimeType(aMimeType.get(), &supported);
  return NS_SUCCEEDED(rv) && supported;
}

static PRBool
IsSupportedPlugin(const nsCString& aMIMEType)
{
  nsCOMPtr<nsIPluginHost> host(do_GetService("@mozilla.org/plugin/host;1"));
  if (!host) {
    return PR_FALSE;
  }
  nsresult rv = host->IsPluginEnabledForType(aMIMEType.get());
  // XXX do plugins expect to work via extension too?
  return NS_SUCCEEDED(rv);
}

nsObjectLoadingContent::nsObjectLoadingContent()
  : mChannel(nsnull)
  , mType(eType_Loading)
  , mInstantiating(PR_FALSE)
  , mUserDisabled(PR_FALSE)
  , mSuppressed(PR_FALSE)
  , mTypeUnsupported(PR_FALSE)
{
}

nsObjectLoadingContent::~nsObjectLoadingContent()
{
  DestroyImageLoadingContent();
  if (mFrameLoader) {
    mFrameLoader->Destroy();
  }
}

// nsIRequestObserver
NS_IMETHODIMP
nsObjectLoadingContent::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  if (aRequest != mChannel) {
    // This is a bit of an edge case - happens when a new load starts before the
    // previous one got here
    return NS_BINDING_ABORTED;
  }

  AutoNotifier notifier(this, PR_TRUE);

  if (!IsSuccessfulRequest(aRequest)) {
    LOG(("OBJLC [%p]: OnStartRequest: Request failed\n", this));
    Fallback(PR_FALSE);
    return NS_BINDING_ABORTED;
  }

  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  NS_ASSERTION(chan, "Why is our request not a channel?");

  nsresult rv = NS_ERROR_UNEXPECTED;
  // This fallback variable MUST be declared after the notifier variable. Do NOT
  // change the order of the declarations!
  AutoFallback fallback(this, &rv);

  rv = chan->GetContentType(mContentType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now find out what type the content is
  // UnloadContent will set our type to null; need to be sure to only set it to
  // the real value on success
  ObjectType newType = GetTypeOfContent(mContentType);
  LOG(("OBJLC [%p]: OnStartRequest: Content Type=<%s> Old type=%u New Type=%u\n",
       this, mContentType.get(), mType, newType));
  if (mType != newType) {
    UnloadContent();
  }

  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "must be a content");
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
        if (!thisContent->IsInDoc()) {
          // XXX frameloaders can't deal with not being in a document
          Fallback(PR_FALSE);
          return NS_ERROR_UNEXPECTED;
        }
        mFrameLoader = new nsFrameLoader(thisContent);
        if (!mFrameLoader) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }

      if (mType != newType) {
        // XXX We must call this before getting the docshell to work around
        // bug 300540; when that's fixed, this if statement can be removed.
        mType = newType;
        notifier.Notify();
      }

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
    case eType_Plugin:
      mInstantiating = PR_TRUE;
      if (mType != newType) {
        // This can go away once plugin loading moves to content (bug 90268)
        mType = newType;
        notifier.Notify();
      }
      nsIObjectFrame* frame;
      frame = GetFrame();
      if (!frame) {
        // Do nothing in this case: This is probably due to a display:none
        // frame. If we ever get a frame, HasNewFrame will do the right thing.
        // Abort the load though, we have no use for the data.
        mInstantiating = PR_FALSE;
        return NS_BINDING_ABORTED;
      }
      rv = frame->Instantiate(chan, getter_AddRefs(mFinalListener));
      mInstantiating = PR_FALSE;
      break;
    case eType_Loading:
      NS_NOTREACHED("Should not have a loading type here!");
    case eType_Null:
      LOG(("OBJLC [%p]: Unsupported type, falling back\n", this));
      // Need to fallback here (instead of using the case below), so that we can
      // set mTypeUnsupported without it being overwritten. This is also why we
      // return early.
      Fallback(PR_FALSE);

      PluginSupportState pluginState = GetPluginSupportState(thisContent,
                                                             mContentType);
      // Do nothing, but fire the plugin not found event if needed
      if (pluginState == ePluginUnsupported) {
        FirePluginNotFound(thisContent);
      }
      if (pluginState != ePluginDisabled) {
        mTypeUnsupported = PR_TRUE;
      }
      return NS_BINDING_ABORTED;
  }

  if (mFinalListener) {
    mType = newType;
    rv = mFinalListener->OnStartRequest(aRequest, aContext);
    if (NS_FAILED(rv)) {
      LOG(("OBJLC [%p]: mFinalListener->OnStartRequest failed (%08x), falling back\n",
           this, rv));
      Fallback(PR_FALSE);
    }
    return rv;
  }

  LOG(("OBJLC [%p]: Found no listener, falling back\n", this));
  Fallback(PR_FALSE);
  return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
nsObjectLoadingContent::OnStopRequest(nsIRequest *aRequest,
                                      nsISupports *aContext,
                                      nsresult aStatusCode)
{
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
nsObjectLoadingContent::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext, nsIInputStream *aInputStream, PRUint32 aOffset, PRUint32 aCount)
{
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
nsObjectLoadingContent::EnsureInstantiation(nsIPluginInstance** aInstance)
{
  // Must set our out parameter to null as we have various early returns with
  // an NS_OK result.
  *aInstance = nsnull;

  if (mType != eType_Plugin) {
    return NS_OK;
  }

  nsIObjectFrame* frame = GetFrame();
  if (frame) {
    // If we have a frame, we may have pending instantiate events; revoke
    // them.
    if (mPendingInstantiateEvent) {
      LOG(("OBJLC [%p]: Revoking pending instantiate event\n", this));
      mPendingInstantiateEvent = nsnull;
    }
  } else {
    // mInstantiating is true if we're in LoadObject; we shouldn't
    // recreate frames in that case, we'd confuse that function.
    if (mInstantiating) {
      return NS_OK;
    }

    // Trigger frame construction
    mInstantiating = PR_TRUE;

    nsCOMPtr<nsIContent> thisContent = 
      do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
    NS_ASSERTION(thisContent, "must be a content");

    nsIDocument* doc = thisContent->GetCurrentDoc();
    if (!doc) {
      // Nothing we can do while plugin loading is done in layout...
      return NS_OK;
    }

    PRUint32 numShells = doc->GetNumberOfShells();
    for (PRUint32 i = 0; i < numShells; ++i) {
      nsIPresShell* shell = doc->GetShellAt(i);
      shell->RecreateFramesFor(thisContent);
    }

    mInstantiating = PR_FALSE;

    frame = GetFrame();
    if (!frame) {
      return NS_OK;
    }
  }

  // We may have a plugin instance already; if so, do nothing
  nsresult rv = frame->GetPluginInstance(*aInstance);
  if (!*aInstance) {
    rv = Instantiate(mContentType, mURI);
    if (NS_SUCCEEDED(rv)) {
      rv = frame->GetPluginInstance(*aInstance);
    } else {
      Fallback(PR_TRUE);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsObjectLoadingContent::HasNewFrame(nsIObjectFrame* aFrame)
{
  LOG(("OBJLC [%p]: Got frame %p (mInstantiating=%i)\n", this, aFrame,
       mInstantiating));
  if (!mInstantiating && aFrame && mType == eType_Plugin) {
    // Asynchronously call Instantiate
    // This can go away once plugin loading moves to content
    // This must be done asynchronously to ensure that the frame is correctly
    // initialized (has a view etc)

    // "revoke" any existing instantiate event.
    mPendingInstantiateEvent = nsnull;

    // When in a plugin document, the document will take care of calling
    // instantiate
    nsCOMPtr<nsIPluginDocument> pDoc (do_QueryInterface(GetOurDocument()));
    if (pDoc) {
      return NS_OK;
    }

    nsCOMPtr<nsIRunnable> event =
        new nsAsyncInstantiateEvent(this, aFrame, mContentType, mURI);
    if (!event) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    LOG(("                 dispatching event\n"));
    nsresult rv = NS_DispatchToCurrentThread(event);
    if (NS_FAILED(rv)) {
      NS_ERROR("failed to dispatch nsAsyncInstantiateEvent");
    } else {
      // Remember this event.  This is a weak reference that will be cleared
      // when the event runs.
      mPendingInstantiateEvent = event;
    }
  }
  return NS_OK;
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
nsObjectLoadingContent::OnChannelRedirect(nsIChannel *aOldChannel,
                                          nsIChannel *aNewChannel,
                                          PRUint32    aFlags)
{
  // If we're already busy with a new load, cancel the redirect
  if (aOldChannel != mChannel) {
    return NS_BINDING_ABORTED;
  }

  mChannel = aNewChannel;
  return NS_OK;
}

// <public>
PRInt32
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
      return 0;
    case eType_Null:
      if (mSuppressed)
        return NS_EVENT_STATE_SUPPRESSED;
      if (mUserDisabled)
        return NS_EVENT_STATE_USERDISABLED;

      // Otherwise, broken
      PRInt32 state = NS_EVENT_STATE_BROKEN;
      if (mTypeUnsupported) {
        state |= NS_EVENT_STATE_TYPE_UNSUPPORTED;
      }
      return state;
  };
  NS_NOTREACHED("unknown type?");
  // this return statement only exists to avoid a compile warning
  return 0;
}

// <protected>
nsresult
nsObjectLoadingContent::LoadObject(const nsAString& aURI,
                                   PRBool aNotify,
                                   const nsCString& aTypeHint,
                                   PRBool aForceLoad)
{
  LOG(("OBJLC [%p]: Loading object: URI string=<%s> notify=%i type=<%s> forceload=%i\n",
       this, NS_ConvertUTF16toUTF8(aURI).get(), aNotify, aTypeHint.get(), aForceLoad));

  NS_ASSERTION(!mInstantiating, "LoadObject was reentered?");

  // Avoid StringToURI in order to use the codebase attribute as base URI
  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "must be a content");

  nsIDocument* doc = thisContent->GetOwnerDoc();
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

nsresult
nsObjectLoadingContent::LoadObject(nsIURI* aURI,
                                   PRBool aNotify,
                                   const nsCString& aTypeHint,
                                   PRBool aForceLoad)
{
  LOG(("OBJLC [%p]: Loading object: URI=<%p> notify=%i type=<%s> forceload=%i\n",
       this, aURI, aNotify, aTypeHint.get(), aForceLoad));

  if (mURI && aURI && !aForceLoad) {
    PRBool equal;
    nsresult rv = mURI->Equals(aURI, &equal);
    if (NS_SUCCEEDED(rv) && equal) {
      // URI didn't change, do nothing
      return NS_OK;
    }
  }

  // Need to revoke any potentially pending instantiate events
  if (mType == eType_Plugin && mPendingInstantiateEvent) {
    LOG(("OBJLC [%p]: Revoking pending instantiate event\n", this));
    mPendingInstantiateEvent = nsnull;
  }

  AutoNotifier notifier(this, aNotify);

  // AutoSetInstantiatingToFalse is instantiated after AutoNotifier, so that if
  // the AutoNotifier triggers frame construction, events can be posted as
  // appropriate.
  NS_ASSERTION(!mInstantiating, "LoadObject was reentered?");
  mInstantiating = PR_TRUE;
  AutoSetInstantiatingToFalse autoset(this);

  mUserDisabled = mSuppressed = PR_FALSE;

  mURI = aURI;
  mContentType = aTypeHint;

  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "must be a content");

  nsIDocument* doc = thisContent->GetOwnerDoc();
  if (!doc) {
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
      Fallback(PR_FALSE);
      return NS_OK;
    }

    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT; // default permit
    rv =
      NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OBJECT,
                                aURI,
                                doc->GetDocumentURI(),
                                NS_STATIC_CAST(nsIImageLoadingContent*, this),
                                aTypeHint,
                                nsnull, //extra
                                &shouldLoad,
                                nsContentUtils::GetContentPolicy());
    if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
      // Must call UnloadContent first, as it overwrites
      // mSuppressed/mUserDisabled. It also takes care of setting the type to
      // eType_Null.
      UnloadContent();
      if (NS_SUCCEEDED(rv)) {
        if (shouldLoad == nsIContentPolicy::REJECT_TYPE) {
          mUserDisabled = PR_TRUE;
        } else if (shouldLoad == nsIContentPolicy::REJECT_SERVER) {
          mSuppressed = PR_TRUE;
        }
      }
      return NS_OK;
    }
  }

  nsresult rv = NS_ERROR_UNEXPECTED;
  // This fallback variable MUST be declared after the notifier variable. Do NOT
  // change the order of the declarations!
  AutoFallback fallback(this, &rv);

  PRUint32 caps = GetCapabilities();
  LOG(("OBJLC [%p]: Capabilities: %04x\n", this, caps));

  if ((caps & eOverrideServerType) && !aTypeHint.IsEmpty()) {
    ObjectType newType = GetTypeOfContent(aTypeHint);
    if (newType != mType) {
      LOG(("OBJLC [%p]: (eOverrideServerType) Changing type from %u to %u\n", this, mType, newType));

      UnloadContent();

      // Must have a frameloader before creating a frame, or the frame will
      // create its own.
      if (!mFrameLoader && newType == eType_Document) {
        if (!thisContent->IsInDoc()) {
          // XXX frameloaders can't deal with not being in a document
          mURI = nsnull;
          return NS_OK;
        }

        mFrameLoader = new nsFrameLoader(thisContent);
        if (!mFrameLoader) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }

      // Must notify here for plugins
      // If aNotify is false, we'll just wait until we get a frame and use the
      // async instantiate path.
      mType = newType;
      if (aNotify)
        notifier.Notify();
    }
    switch (newType) {
      case eType_Image:
        // Don't notify, because we will take care of that ourselves.
        rv = LoadImage(aURI, aForceLoad, PR_FALSE);
        break;
      case eType_Plugin:
        rv = Instantiate(aTypeHint, aURI);
        break;
      case eType_Document:
        rv = mFrameLoader->LoadURI(aURI);
        break;
      case eType_Loading:
        NS_NOTREACHED("Should not have a loading type here!");
      case eType_Null:
        // No need to load anything
        PluginSupportState pluginState = GetPluginSupportState(thisContent,
                                                               aTypeHint);
        if (pluginState == ePluginUnsupported) {
          FirePluginNotFound(thisContent);
        }
        if (pluginState != ePluginDisabled) {
          fallback.TypeUnsupported();
        }

        break;
    };
    return NS_OK;
  }

  // If the class ID specifies a supported plugin, or if we have no explicit URI
  // but a type, immediately instantiate the plugin.
  PRBool isSupportedClassID = PR_FALSE;
  nsCAutoString typeForID; // Will be set iff isSupportedClassID == PR_TRUE
  PRBool hasID = PR_FALSE;
  if (caps & eSupportClassID) {
    nsAutoString classid;
    thisContent->GetAttr(kNameSpaceID_None, nsGkAtoms::classid, classid);
    if (!classid.IsEmpty()) {
      hasID = PR_TRUE;
      isSupportedClassID = NS_SUCCEEDED(TypeForClassID(classid, typeForID));
    }
  }

  if (hasID && !isSupportedClassID) {
    // We have a class ID and it's unsupported.  Fallback in that case.
    LOG(("OBJLC [%p]: invalid classid\n", this));
    rv = NS_ERROR_NOT_AVAILABLE;
    return NS_OK;
  }

  if (isSupportedClassID ||
      (!aURI && !aTypeHint.IsEmpty() &&
       GetTypeOfContent(aTypeHint) == eType_Plugin)) {
    // No URI, but we have a type. The plugin will handle the load.
    // Or: supported class id, plugin will handle the load.
    LOG(("OBJLC [%p]: (classid) Changing type from %u to eType_Plugin\n", this, mType));
    mType = eType_Plugin;
    if (aNotify)
      notifier.Notify();

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

    rv = Instantiate(mContentType, mURI);
    return NS_OK;
  }

  if (!aURI) {
    // No URI and no type... nothing we can do.
    LOG(("OBJLC [%p]: no URI\n", this));
    rv = NS_ERROR_NOT_AVAILABLE;
    return NS_OK;
  }

  if (!CanHandleURI(aURI)) {
    LOG(("OBJLC [%p]: can't handle URI\n", this));
    // E.g. mms://
    mType = eType_Plugin;
    if (aNotify)
      notifier.Notify();

    rv = Instantiate(aTypeHint, aURI);
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> group = doc->GetDocumentLoadGroup();
  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannel(getter_AddRefs(chan), aURI, nsnull, group, this);
  NS_ENSURE_SUCCESS(rv, rv);

  // Referrer
  nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(chan));
  if (httpChan) {
    httpChan->SetReferrer(doc->GetDocumentURI());
  }

  // MIME Type hint
  if (!aTypeHint.IsEmpty()) {
    chan->SetContentType(aTypeHint);
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
         eSupportDocuments
#ifdef MOZ_SVG
         | eSupportSVG
#endif
         ;
}

void
nsObjectLoadingContent::Fallback(PRBool aNotify)
{
  LOG(("OBJLC [%p]: Falling back (Notify=%i)\n", this, aNotify));

  AutoNotifier notifier(this, aNotify);

  UnloadContent();
}

void
nsObjectLoadingContent::RemovedFromDocument()
{
  LOG(("OBJLC [%p]: Removed from doc\n", this));
  if (mFrameLoader) {
    // XXX This is very temporary and must go away
    mFrameLoader->Destroy();
    mFrameLoader = nsnull;

    // Clear the current URI, so that LoadObject doesn't think that we
    // have already loaded the content.
    mURI = nsnull;
  }
}

void
nsObjectLoadingContent::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  cb.NoteXPCOMChild(mFrameLoader);
}

// <private>
/* static */ PRBool
nsObjectLoadingContent::IsSuccessfulRequest(nsIRequest* aRequest)
{
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  if (NS_FAILED(rv) || NS_FAILED(status)) {
    return PR_FALSE;
  }

  // This may still be an error page or somesuch
  nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(aRequest));
  if (httpChan) {
    PRBool success;
    rv = httpChan->GetRequestSucceeded(&success);
    if (NS_FAILED(rv) || !success) {
      return PR_FALSE;
    }
  }

  // Otherwise, the request is successful
  return PR_TRUE;
}

/* static */ PRBool
nsObjectLoadingContent::CanHandleURI(nsIURI* aURI)
{
  nsCAutoString scheme;
  if (NS_FAILED(aURI->GetScheme(scheme))) {
    return PR_FALSE;
  }

  nsIIOService* ios = nsContentUtils::GetIOService();
  if (!ios)
    return PR_FALSE;
  
  nsCOMPtr<nsIProtocolHandler> handler;
  ios->GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
  if (!handler) {
    return PR_FALSE;
  }
  
  nsCOMPtr<nsIExternalProtocolHandler> extHandler =
    do_QueryInterface(handler);
  // We can handle this URI if its protocol handler is not the external one
  return extHandler == nsnull;
}

PRBool
nsObjectLoadingContent::IsSupportedDocument(const nsCString& aMimeType)
{
  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
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
      PRBool canConvert = PR_FALSE;
      if (convServ) {
        rv = convServ->CanConvert(aMimeType.get(), "*/*", &canConvert);
      }

      return NS_SUCCEEDED(rv) && canConvert;
    }

    // Don't want to support plugins as documents
    return supported != nsIWebNavigationInfo::PLUGIN;
  }

  return PR_FALSE;
}

void
nsObjectLoadingContent::UnloadContent()
{
  // Don't notify in CancelImageRequests. We do it ourselves.
  CancelImageRequests(PR_FALSE);
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nsnull;
  }
  mType = eType_Null;
  mUserDisabled = mSuppressed = mTypeUnsupported = PR_FALSE;
}

void
nsObjectLoadingContent::NotifyStateChanged(ObjectType aOldType,
                                          PRInt32 aOldState,
                                          PRBool aSync)
{
  LOG(("OBJLC [%p]: Notifying about state change: (%u, %x) -> (%u, %x) (sync=%i)\n",
       this, aOldType, aOldState, mType, ObjectState(), aSync));

  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "must be a content");

  nsIDocument* doc = thisContent->GetCurrentDoc();
  if (!doc) {
    return; // Nothing to do
  }

  PRInt32 newState = ObjectState();

  if (newState != aOldState) {
    // This will trigger frame construction
    NS_ASSERTION(thisContent->IsInDoc(), "Something is confused");
    PRInt32 changedBits = aOldState ^ newState;

    {
      mozAutoDocUpdate upd(doc, UPDATE_CONTENT_STATE, PR_TRUE);
      doc->ContentStatesChanged(thisContent, nsnull, changedBits);
    }
    if (aSync) {
      // Make sure that frames are actually constructed, and do it after
      // EndUpdate was called.
      doc->FlushPendingNotifications(Flush_Frames);
    }
  } else if (aOldType != mType) {
    // If our state changed, then we already recreated frames
    // Otherwise, need to do that here

    PRUint32 numShells = doc->GetNumberOfShells();
    for (PRUint32 i = 0; i < numShells; ++i) {
      nsIPresShell* shell = doc->GetShellAt(i);
      shell->RecreateFramesFor(thisContent);
    }
  }
}

/* static */ void
nsObjectLoadingContent::FirePluginNotFound(nsIContent* thisContent)
{
  LOG(("OBJLC []: Dispatching PluginNotFound event for content %p\n",
       thisContent));

  nsCOMPtr<nsIRunnable> ev = new nsPluginNotFoundEvent(thisContent);
  nsresult rv = NS_DispatchToCurrentThread(ev);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch nsPluginNotFoundEvent");
  }
}

nsObjectLoadingContent::ObjectType
nsObjectLoadingContent::GetTypeOfContent(const nsCString& aMIMEType)
{
  PRUint32 caps = GetCapabilities();

  if ((caps & eSupportImages) && IsSupportedImage(aMIMEType)) {
    return eType_Image;
  }

#ifdef MOZ_SVG
  PRBool isSVG = aMIMEType.LowerCaseEqualsLiteral("image/svg+xml");
  PRBool supportedSVG = isSVG && (caps & eSupportSVG);
#else
  PRBool supportedSVG = PR_FALSE;
#endif
  if (((caps & eSupportDocuments) || supportedSVG) &&
      IsSupportedDocument(aMIMEType)) {
    return eType_Document;
  }

  if ((caps & eSupportPlugins) && IsSupportedPlugin(aMIMEType)) {
    return eType_Plugin;
  }

  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "must be a content");

  if (ShouldShowDefaultPlugin(thisContent, aMIMEType)) {
    return eType_Plugin;
  }

  return eType_Null;
}

nsresult
nsObjectLoadingContent::TypeForClassID(const nsAString& aClassID,
                                       nsACString& aType)
{
  // Need a plugin host for any class id support
  nsCOMPtr<nsIPluginHost> pluginHost(do_GetService(kCPluginManagerCID));
  if (!pluginHost) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (StringBeginsWith(aClassID, NS_LITERAL_STRING("java:"))) {
    // Supported if we have a java plugin
    aType.AssignLiteral("application/x-java-vm");
    nsresult rv = pluginHost->IsPluginEnabledForType("application/x-java-vm");
    return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  // If it starts with "clsid:", this is ActiveX content
  if (StringBeginsWith(aClassID, NS_LITERAL_STRING("clsid:"))) {
    // Check if we have a plugin for that

    if (NS_SUCCEEDED(pluginHost->IsPluginEnabledForType("application/x-oleobject"))) {
      aType.AssignLiteral("application/x-oleobject");
      return NS_OK;
    }
    if (NS_SUCCEEDED(pluginHost->IsPluginEnabledForType("application/oleobject"))) {
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
                                              thisContent->GetOwnerDoc(),
                                              baseURI);
  } else {
    baseURI.swap(*aURI);
  }
}

nsIObjectFrame*
nsObjectLoadingContent::GetFrame()
{
  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "must be a content");

  nsIDocument* doc = thisContent->GetCurrentDoc();
  if (!doc) {
    return nsnull; // No current doc -> no frame
  }

  nsIPresShell* shell = doc->GetShellAt(0);
  if (!shell) {
    return nsnull; // No presentation -> no frame
  }

  nsIFrame* frame = shell->GetPrimaryFrameFor(thisContent);
  if (!frame) {
    return nsnull;
  }

  nsIObjectFrame* objFrame;
  CallQueryInterface(frame, &objFrame);
  return objFrame;
}

nsresult
nsObjectLoadingContent::Instantiate(const nsACString& aMIMEType, nsIURI* aURI)
{
  nsIObjectFrame* frame = GetFrame();
  if (!frame) {
    LOG(("OBJLC [%p]: Attempted to instantiate, but have no frame\n", this));
    return NS_OK; // Not a failure to have no frame
  }

  nsCString typeToUse(aMIMEType);
  if (typeToUse.IsEmpty() && aURI) {
    nsCAutoString ext;
    
    nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
    if (url) {
      url->GetFileExtension(ext);
    } else {
      nsCString spec;
      aURI->GetSpec(spec);

      PRInt32 offset = spec.RFindChar('.');
      if (offset != kNotFound) {
        ext = Substring(spec, offset + 1, spec.Length());
      }
    }

    nsCOMPtr<nsIPluginHost> host(do_GetService("@mozilla.org/plugin/host;1"));
    const char* typeFromExt;
    if (host &&
        NS_SUCCEEDED(host->IsPluginEnabledForExtension(ext.get(), typeFromExt))) {
      typeToUse = typeFromExt;
    }
  }

  nsCOMPtr<nsIURI> baseURI;
  if (!aURI) {
    // We need some URI. If we have nothing else, use the base URI.
    // XXX(biesi): The code used to do this. Not sure why this is correct...
    nsCOMPtr<nsIContent> thisContent = 
      do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
    NS_ASSERTION(thisContent, "must be a content");

    GetObjectBaseURI(thisContent, getter_AddRefs(baseURI));
    aURI = baseURI;
  }

  // We'll always have a type or a URI by the time we get here
  NS_ASSERTION(aURI || !typeToUse.IsEmpty(), "Need a URI or a type");
  LOG(("OBJLC [%p]: Calling [%p]->Instantiate(<%s>, %p)\n", this, frame,
       typeToUse.get(), aURI));
  return frame->Instantiate(typeToUse.get(), aURI);
}

/* static */ PRBool
nsObjectLoadingContent::ShouldShowDefaultPlugin(nsIContent* aContent,
                                                const nsCString& aContentType)
{
  if (nsContentUtils::GetBoolPref("plugin.default_plugin_disabled", PR_FALSE)) {
    return PR_FALSE;
  }

  return GetPluginSupportState(aContent, aContentType) == ePluginUnsupported;
}

/* static */ nsObjectLoadingContent::PluginSupportState
nsObjectLoadingContent::GetPluginSupportState(nsIContent* aContent,
                                              const nsCString& aContentType)
{
  if (!aContent->IsNodeOfType(nsINode::eHTML)) {
    return ePluginOtherState;
  }

  if (aContent->Tag() == nsGkAtoms::embed ||
      aContent->Tag() == nsGkAtoms::applet) {
    return GetPluginDisabledState(aContentType);
  }

  // Search for a child <param> with a pluginurl name
  PRUint32 count = aContent->GetChildCount();
  for (PRUint32 i = 0; i < count; ++i) {
    nsIContent* child = aContent->GetChildAt(i);
    NS_ASSERTION(child, "GetChildCount lied!");

    if (child->IsNodeOfType(nsINode::eHTML) &&
        child->Tag() == nsGkAtoms::param &&
        child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                           NS_LITERAL_STRING("pluginurl"), eIgnoreCase)) {
      return GetPluginDisabledState(aContentType);
    }
  }
  return ePluginOtherState;
}

/* static */ nsObjectLoadingContent::PluginSupportState
nsObjectLoadingContent::GetPluginDisabledState(const nsCString& aContentType)
{
  nsCOMPtr<nsIPluginHost> host(do_GetService("@mozilla.org/plugin/host;1"));
  if (!host) {
    return ePluginUnsupported;
  }
  nsresult rv = host->IsPluginEnabledForType(aContentType.get());
  return rv == NS_ERROR_PLUGIN_DISABLED ? ePluginDisabled : ePluginUnsupported;
}

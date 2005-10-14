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
#include "nsIURIContentListener.h"
#include "nsIWebNavigation.h"
#include "nsIWebNavigationInfo.h"

// Util headers
#include "plevent.h"

#include "nsAutoPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocShellCID.h"
#include "nsEventQueueUtils.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsNetUtil.h"

// Concrete classes
#include "nsFrameLoader.h"

#include "nsObjectLoadingContent.h"

static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

PR_BEGIN_EXTERN_C
/* Note that these typedefs declare functions, not pointer to
   functions.  That's the only way in which they differ from
   PLHandleEventProc and PLDestroyEventProc. */
typedef void*
(PR_CALLBACK EventHandlerFunc)(PLEvent* self);
typedef void
(PR_CALLBACK EventDestructorFunc)(PLEvent* self);
PR_END_EXTERN_C

struct nsAsyncInstantiateEvent : public PLEvent {
  // This stores both the content and the frame so that Instantiate calls can be
  // avoided if the frame changed in the meantime.
  // (the content is stored implicitly as the owner)
  nsIObjectFrame*         mFrame;
  nsCString               mContentType;
  nsCOMPtr<nsIURI>        mURI;

  nsAsyncInstantiateEvent(nsObjectLoadingContent* aContent,
                          nsIObjectFrame* aFrame,
                          const nsCString& aType,
                          nsIURI* aURI)
    : mFrame(aFrame), mContentType(aType), mURI(aURI)
  {
    NS_ADDREF(NS_STATIC_CAST(nsIObjectLoadingContent*, aContent));
    PL_InitEvent(this, aContent, nsAsyncInstantiateEvent::HandleEvent,
                 nsAsyncInstantiateEvent::CleanupEvent);
  }

  ~nsAsyncInstantiateEvent()
  {
    nsIObjectLoadingContent* con = NS_STATIC_CAST(nsIObjectLoadingContent*,
                                                  PL_GetEventOwner(this));
    NS_RELEASE(con);
  }

  static EventHandlerFunc HandleEvent;
  static EventDestructorFunc CleanupEvent;
};

/* static */ void* PR_CALLBACK
nsAsyncInstantiateEvent::HandleEvent(PLEvent* event)
{
  nsAsyncInstantiateEvent* ev = NS_STATIC_CAST(nsAsyncInstantiateEvent*,
                                               event);
  nsObjectLoadingContent* con = NS_STATIC_CAST(nsObjectLoadingContent*,
                                               PL_GetEventOwner(event));
  // Make sure that we still have the right frame
  if (con->GetFrame() == ev->mFrame) {
    nsresult rv = ev->mFrame->Instantiate(ev->mContentType.get(), ev->mURI);
    if (NS_FAILED(rv)) {
      con->Fallback(PR_TRUE);
    }
  }
  return nsnull;
}

/* static */ void PR_CALLBACK
nsAsyncInstantiateEvent::CleanupEvent(PLEvent* event)
{
  nsAsyncInstantiateEvent* ev = NS_STATIC_CAST(nsAsyncInstantiateEvent*,
                                               event);
  delete ev;
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
        mContent->NotifyStateChanged(mOldType, mOldState);
      }
    }

    /**
     * Send notifications now, ignoring the value of mNotify. The new type and
     * state is saved, and the destructor will notify again if mNotify is true
     * and the values changed.
     */
    void Notify() {
      NS_ASSERTION(mNotify, "Should not notify when notify=false");

      mContent->NotifyStateChanged(mOldType, mOldState);
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
      : mContent(aContent), mResult(rv) {}
    ~AutoFallback() { if (NS_FAILED(*mResult)) mContent->Fallback(PR_FALSE); }
  private:
    nsObjectLoadingContent* mContent;
    const nsresult* mResult;
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

nsObjectLoadingContent::nsObjectLoadingContent(ObjectType aInitialType)
  : mChannel(nsnull)
  , mType(aInitialType)
  , mInstantiating(PR_FALSE)
  , mUserDisabled(PR_FALSE)
  , mSuppressed(PR_FALSE)
{
}

nsObjectLoadingContent::~nsObjectLoadingContent()
{
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
      nsCOMPtr<nsIContent> thisContent = 
        do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
      NS_ASSERTION(thisContent, "must be a content");
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
      nsCOMPtr<nsIURIContentListener> listener(do_GetInterface(docShell));
      NS_ASSERTION(listener, "No content listener on the docshell?");

#ifdef DEBUG
      // Since this is a supported document type, the docshell must support it.
      nsXPIDLCString str;
      PRBool canHandle;
      rv = listener->CanHandleContent(mContentType.get(), PR_TRUE,
                                      getter_Copies(str), &canHandle);
      NS_ASSERTION(NS_FAILED(rv) || canHandle, "Docshell should handle this!");
#endif

      PRBool abort;
      rv = listener->DoContent(mContentType.get(), PR_TRUE, aRequest,
                               getter_AddRefs(mFinalListener), &abort);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(!abort, "Docshell content listeners shouldn't abort loads");
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
      if (frame) {
        rv = frame->Instantiate(chan, getter_AddRefs(mFinalListener));
      }
      mInstantiating = PR_FALSE;
      break;
    case eType_Loading:
      NS_NOTREACHED("Should not have a loading type here!");
    case eType_Null:
      // Do nothing.
      break;
  }

  if (mFinalListener) {
    mType = newType;
    rv = mFinalListener->OnStartRequest(aRequest, aContext);
    if (NS_FAILED(rv)) {
      Fallback(PR_FALSE);
    }
    return rv;
  }

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



NS_IMETHODIMP
nsObjectLoadingContent::HasNewFrame(nsIObjectFrame* aFrame)
{
  if (!mInstantiating && aFrame && mType == eType_Plugin) {
    // Asynchronously call Instantiate
    // This can go away once plugin loading moves to content
    // This must be done asynchronously to ensure that the frame is correctly
    // initialized (has a view etc)

    // When in a plugin document, the document will take care of calling
    // instantiate
    nsCOMPtr<nsIPluginDocument> pDoc (do_QueryInterface(GetOurDocument()));
    if (pDoc) {
      return NS_OK;
    }

    nsCOMPtr<nsIEventQueue> eventQ;
    NS_GetCurrentEventQ(getter_AddRefs(eventQ));
    if (!eventQ) {
      return NS_ERROR_UNEXPECTED;
    }

    nsAsyncInstantiateEvent* ev = new nsAsyncInstantiateEvent(this, aFrame,
                                                              mContentType,
                                                              mURI);
    if (!ev) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = eventQ->PostEvent(ev);
    if (NS_FAILED(rv)) {
      PL_DestroyEvent(ev);
    }
  }
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
      return 0;
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
      
      // Otherwise, just broken
      return NS_EVENT_STATE_BROKEN;
  };
  NS_NOTREACHED("unknown type?");
  // this return statement only exists to avoid a compile warning
  return 0;
}

// <protected>
nsresult
nsObjectLoadingContent::ObjectURIChanged(const nsAString& aURI,
                                         PRBool aNotify,
                                         const nsCString& aTypeHint,
                                         PRBool aForceType,
                                         PRBool aForceLoad)
{
  // Avoid StringToURI in order to use the codebase attribute as base URI
  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "must be a content");

  nsIDocument* doc = thisContent->GetOwnerDoc();

  // For plugins, the codebase attribute is the base URI
  nsCOMPtr<nsIURI> baseURI = thisContent->GetBaseURI();
  nsCOMPtr<nsIURI> codebaseURI;
  nsAutoString codebase;
  nsresult rv = thisContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::codebase,
                                     codebase);
  if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(codebaseURI),
                                              codebase, doc, baseURI);
  } else {
    baseURI.swap(codebaseURI);
  }

  nsCOMPtr<nsIURI> uri;
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                            aURI, doc,
                                            codebaseURI);
  // If URI creation failed, fallback immediately - this only happens for
  // malformed URIs
  if (!uri) {
    Fallback(aNotify);
    return NS_OK;
  }

  return ObjectURIChanged(uri, aNotify, aTypeHint, aForceType, aForceLoad);
}

nsresult
nsObjectLoadingContent::ObjectURIChanged(nsIURI* aURI,
                                         PRBool aNotify,
                                         const nsCString& aTypeHint,
                                         PRBool aForceType,
                                         PRBool aForceLoad)
{
  if (mURI && aURI && !aForceLoad) {
    PRBool equal;
    nsresult rv = mURI->Equals(aURI, &equal);
    if (NS_SUCCEEDED(rv) && equal) {
      // URI didn't change, do nothing
      return NS_OK;
    }
  }

  AutoNotifier notifier(this, aNotify);

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
    nsresult rv = secMan->CheckLoadURIWithPrincipal(doc->GetPrincipal(), aURI, 0);
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

  if (aForceType && !aTypeHint.IsEmpty()) {
    ObjectType newType = GetTypeOfContent(aTypeHint);
    if (newType != mType) {
      mInstantiating = PR_TRUE;
      UnloadContent();

      // Must have a frameloader before creating a frame, or the frame will
      // create its own.
      if (!mFrameLoader && newType == eType_Document) {
        if (!thisContent->IsInDoc()) {
          // XXX frameloaders can't deal with not being in a document
          mInstantiating = PR_FALSE;
          mURI = nsnull;
          return NS_OK;
        }

        mFrameLoader = new nsFrameLoader(thisContent);
        if (!mFrameLoader) {
          mInstantiating = PR_FALSE;
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }

      // Must notify here for plugins
      // If aNotify is false, we'll just wait until we get a frame and use the
      // async instantiate path.
      mType = newType;
      if (aNotify)
        notifier.Notify();
      mInstantiating = PR_FALSE;
    }
    switch (newType) {
      case eType_Image:
        // Don't notify, because we will take care of that ourselves.
        rv = ImageURIChanged(aURI, aForceLoad, PR_FALSE);
        break;
      case eType_Plugin:
        nsIObjectFrame* frame;
        frame = GetFrame();
        if (frame) {
          rv = frame->Instantiate(aTypeHint.get(), aURI);
        } else {
          // Shouldn't fallback just because we have no frame
          rv = NS_OK;
        }
        break;
      case eType_Document:
        rv = mFrameLoader->LoadURI(aURI);
        break;
      case eType_Loading:
        NS_NOTREACHED("Should not have a loading type here!");
      case eType_Null:
        // No need to load anything
        break;
    };
    return NS_OK;
  }

  mInstantiating = PR_TRUE;
  // If the class ID specifies a supported plugin, or if we have no explicit URI
  // but a type, immediately instantiate the plugin.
  PRBool isSupportedClassID = PR_FALSE;
  if (GetCapabilities() & eSupportClassID) {
    nsAutoString classid;
    rv = thisContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::classid, classid);
    nsCAutoString typeForID;
    if (rv == NS_CONTENT_ATTR_HAS_VALUE)
      isSupportedClassID = NS_SUCCEEDED(TypeForClassID(classid, typeForID));
  }
  if (isSupportedClassID ||
      (!aURI && !aTypeHint.IsEmpty() &&
       GetTypeOfContent(aTypeHint) == eType_Plugin)) {
    // No URI, but we have a type. The plugin will handle the load.
    // Or: supported class id, plugin will handle the load.
    mType = eType_Plugin;
    if (aNotify)
      notifier.Notify();

    nsIObjectFrame* frame = GetFrame();
    if (frame) {
      rv = frame->Instantiate(aTypeHint.get(), aURI);
    } else {
      // Shouldn't fallback just because we have no frame
      rv = NS_OK;
    }
    mInstantiating = PR_FALSE;
    return NS_OK;
  }
  // If we get here, and we had a class ID, then it must have been unsupported.
  // Fallback in that case.
  if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
    mInstantiating = PR_FALSE;
    rv = NS_ERROR_NOT_AVAILABLE;
    return NS_OK;
  }

  if (!aURI) {
    // No URI and no type... nothing we can do.
    mInstantiating = PR_FALSE;
    rv = NS_ERROR_NOT_AVAILABLE;
    return NS_OK;
  }

  if (!CanHandleURI(aURI)) {
    // E.g. mms://
    mType = eType_Plugin;
    if (aNotify)
      notifier.Notify();

    nsIObjectFrame* frame = GetFrame();
    if (frame) {
      rv = frame->Instantiate(aTypeHint.get(), aURI);
    } else {
      // Shouldn't fallback just because we have no frame
      rv = NS_OK;
    }
    mInstantiating = PR_FALSE;
    return NS_OK;
  }

  mInstantiating = PR_FALSE;

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

    // Clear the current URI, so that ObjectURIChanged doesn't think that we
    // have already loaded the content.
    mURI = nsnull;
  }
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

  return NS_SUCCEEDED(rv) &&
    supported != nsIWebNavigationInfo::UNSUPPORTED &&
    supported != nsIWebNavigationInfo::PLUGIN;
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
  mUserDisabled = mSuppressed = PR_FALSE;
}

void
nsObjectLoadingContent::NotifyStateChanged(ObjectType aOldType,
                                          PRInt32 aOldState)
{
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

    mozAutoDocUpdate(doc, UPDATE_CONTENT_STATE, PR_TRUE);
    doc->ContentStatesChanged(thisContent, nsnull, changedBits);
  } else if (aOldType != mType) {
    // If our state changed, then we already recreated frames
    // Otherwise, need to do that here

    // Need the following line before calling RecreateFramesFor
    mozAutoDocUpdate upd(doc, UPDATE_CONTENT_STATE, PR_TRUE);

    PRUint32 numShells = doc->GetNumberOfShells();
    for (PRUint32 i = 0; i < numShells; ++i) {
      nsIPresShell* shell = doc->GetShellAt(i);
      shell->RecreateFramesFor(thisContent);
    }
  }
}

nsObjectLoadingContent::ObjectType
nsObjectLoadingContent::GetTypeOfContent(const nsCString& aMIMEType)
{
  PRUint32 caps = GetCapabilities();

  if ((caps & eSupportImages) && IsSupportedImage(aMIMEType)) {
    return eType_Image;
  }

  if ((caps & eSupportPlugins) && IsSupportedPlugin(aMIMEType)) {
    return eType_Plugin;
  }

  PRBool isSVG = aMIMEType.LowerCaseEqualsLiteral("image/svg+xml");
#ifdef MOZ_SVG
  PRBool supportedSVG = isSVG && (caps & eSupportSVG);
#else
  PRBool supportedSVG = PR_FALSE;
#endif
  if (((caps & eSupportDocuments) || supportedSVG) &&
      IsSupportedDocument(aMIMEType)) {
    return eType_Document;
  }

  nsCOMPtr<nsIContent> thisContent = 
    do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "must be a content");

  if (ShouldShowDefaultPlugin(thisContent)) {
    return eType_Plugin;
  }

  return eType_Null;
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

/* static */ PRBool
nsObjectLoadingContent::ShouldShowDefaultPlugin(nsIContent* aContent)
{
  if (nsContentUtils::GetBoolPref("plugin.default_plugin_disabled", PR_FALSE)) {
    return PR_FALSE;
  }

  if (!aContent->IsContentOfType(nsIContent::eHTML)) {
    return PR_FALSE;
  }

  if (aContent->Tag() == nsHTMLAtoms::embed ||
      aContent->Tag() == nsHTMLAtoms::applet) {
    return PR_TRUE;
  }

  // Search for a child <param> with a pluginurl name
  PRUint32 count = aContent->GetChildCount();
  for (PRUint32 i = 0; i < count; ++i) {
    nsIContent* child = aContent->GetChildAt(i);
    NS_ASSERTION(child, "GetChildCount lied!");

    if (child->IsContentOfType(nsIContent::eHTML) &&
        child->Tag() == nsHTMLAtoms::param &&
        child->AttrValueIs(kNameSpaceID_None, nsHTMLAtoms::name,
                           NS_LITERAL_STRING("pluginurl"), eIgnoreCase)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

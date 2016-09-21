/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "jsapi.h"
#include "jswrapper.h"
#include "nsCRT.h"
#include "nsError.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsJSProtocolHandler.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"

#include "nsIStreamListener.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWindowMediator.h"
#include "nsPIDOMWindow.h"
#include "nsIConsoleService.h"
#include "nsXPIDLString.h"
#include "prprf.h"
#include "nsEscape.h"
#include "nsIWebNavigation.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "nsNullPrincipal.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsIScriptChannel.h"
#include "nsIDocument.h"
#include "nsILoadInfo.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIContentSecurityPolicy.h"
#include "nsSandboxFlags.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsILoadInfo.h"
#include "nsContentSecurityManager.h"

#include "mozilla/ipc/URIUtils.h"

using mozilla::dom::AutoEntryScript;

static NS_DEFINE_CID(kJSURICID, NS_JSURI_CID);

class nsJSThunk : public nsIInputStream
{
public:
    nsJSThunk();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_FORWARD_SAFE_NSIINPUTSTREAM(mInnerStream)

    nsresult Init(nsIURI* uri);
    nsresult EvaluateScript(nsIChannel *aChannel,
                            PopupControlState aPopupState,
                            uint32_t aExecutionPolicy,
                            nsPIDOMWindowInner *aOriginalInnerWindow);

protected:
    virtual ~nsJSThunk();

    nsCOMPtr<nsIInputStream>    mInnerStream;
    nsCString                   mScript;
    nsCString                   mURL;
};

//
// nsISupports implementation...
//
NS_IMPL_ISUPPORTS(nsJSThunk, nsIInputStream)


nsJSThunk::nsJSThunk()
{
}

nsJSThunk::~nsJSThunk()
{
}

nsresult nsJSThunk::Init(nsIURI* uri)
{
    NS_ENSURE_ARG_POINTER(uri);

    // Get the script string to evaluate...
    nsresult rv = uri->GetPath(mScript);
    if (NS_FAILED(rv)) return rv;

    // Get the url.
    rv = uri->GetSpec(mURL);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

static bool
IsISO88591(const nsString& aString)
{
    for (nsString::const_char_iterator c = aString.BeginReading(),
                                   c_end = aString.EndReading();
         c < c_end; ++c) {
        if (*c > 255)
            return false;
    }
    return true;
}

static
nsIScriptGlobalObject* GetGlobalObject(nsIChannel* aChannel)
{
    // Get the global object owner from the channel
    nsCOMPtr<nsIDocShell> docShell;
    NS_QueryNotificationCallbacks(aChannel, docShell);
    if (!docShell) {
        NS_WARNING("Unable to get a docShell from the channel!");
        return nullptr;
    }

    // So far so good: get the script global from its docshell
    nsIScriptGlobalObject* global = docShell->GetScriptGlobalObject();

    NS_ASSERTION(global,
                 "Unable to get an nsIScriptGlobalObject from the "
                 "docShell!");
    return global;
}

nsresult nsJSThunk::EvaluateScript(nsIChannel *aChannel,
                                   PopupControlState aPopupState,
                                   uint32_t aExecutionPolicy,
                                   nsPIDOMWindowInner *aOriginalInnerWindow)
{
    if (aExecutionPolicy == nsIScriptChannel::NO_EXECUTION) {
        // Nothing to do here.
        return NS_ERROR_DOM_RETVAL_UNDEFINED;
    }
    
    NS_ENSURE_ARG_POINTER(aChannel);

    // Get principal of code for execution
    nsCOMPtr<nsISupports> owner;
    aChannel->GetOwner(getter_AddRefs(owner));
    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(owner);
    if (!principal) {
        nsCOMPtr<nsILoadInfo> loadInfo;
        aChannel->GetLoadInfo(getter_AddRefs(loadInfo));
        if (loadInfo && loadInfo->GetForceInheritPrincipal()) {
            principal = loadInfo->PrincipalToInherit();
        } else {
            // No execution without a principal!
            NS_ASSERTION(!owner, "Non-principal owner?");
            NS_WARNING("No principal to execute JS with");
            return NS_ERROR_DOM_RETVAL_UNDEFINED;
        }
    }

    nsresult rv;

    // CSP check: javascript: URIs disabled unless "inline" scripts are
    // allowed.
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = principal->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS(rv, rv);
    if (csp) {
        bool allowsInlineScript = true;
        rv = csp->GetAllowsInline(nsIContentPolicy::TYPE_SCRIPT,
                                  EmptyString(), // aNonce
                                  EmptyString(), // aContent
                                  0,             // aLineNumber
                                  &allowsInlineScript);

        //return early if inline scripts are not allowed
        if (!allowsInlineScript) {
          return NS_ERROR_DOM_RETVAL_UNDEFINED;
        }
    }

    // Get the global object we should be running on.
    nsIScriptGlobalObject* global = GetGlobalObject(aChannel);
    if (!global) {
        return NS_ERROR_FAILURE;
    }

    // Sandboxed document check: javascript: URI's are disabled
    // in a sandboxed document unless 'allow-scripts' was specified.
    nsIDocument* doc = aOriginalInnerWindow->GetExtantDoc();
    if (doc && doc->HasScriptsBlockedBySandbox()) {
        return NS_ERROR_DOM_RETVAL_UNDEFINED;
    }

    // Push our popup control state
    nsAutoPopupStatePusher popupStatePusher(aPopupState);

    // Make sure we still have the same inner window as we used to.
    nsCOMPtr<nsPIDOMWindowOuter> win = do_QueryInterface(global);
    nsPIDOMWindowInner *innerWin = win->GetCurrentInnerWindow();

    if (innerWin != aOriginalInnerWindow) {
        return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIScriptGlobalObject> innerGlobal = do_QueryInterface(innerWin);

    mozilla::DebugOnly<nsCOMPtr<nsIDOMWindow>> domWindow(do_QueryInterface(global, &rv));
    if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
    }

    // So far so good: get the script context from its owner.
    nsCOMPtr<nsIScriptContext> scriptContext = global->GetContext();
    if (!scriptContext)
        return NS_ERROR_FAILURE;

    nsAutoCString script(mScript);
    // Unescape the script
    NS_UnescapeURL(script);


    nsCOMPtr<nsIScriptSecurityManager> securityManager;
    securityManager = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    // New script entry point required, due to the "Create a script" step of
    // http://www.whatwg.org/specs/web-apps/current-work/#javascript-protocol
    nsAutoMicroTask mt;
    AutoEntryScript aes(innerGlobal, "javascript: URI", true);
    JSContext* cx = aes.cx();
    JS::Rooted<JSObject*> globalJSObject(cx, innerGlobal->GetGlobalJSObject());
    NS_ENSURE_TRUE(globalJSObject, NS_ERROR_UNEXPECTED);

    //-- Don't execute unless the script principal subsumes the
    //   principal of the context.
    nsIPrincipal* objectPrincipal = nsContentUtils::ObjectPrincipal(globalJSObject);

    bool subsumes;
    rv = principal->Subsumes(objectPrincipal, &subsumes);
    if (NS_FAILED(rv))
        return rv;

    if (!subsumes) {
        return NS_ERROR_DOM_RETVAL_UNDEFINED;
    }

    // Fail if someone tries to execute in a global with system principal.
    if (nsContentUtils::IsSystemPrincipal(objectPrincipal)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    JS::Rooted<JS::Value> v (cx, JS::UndefinedValue());
    // Finally, we have everything needed to evaluate the expression.
    JS::CompileOptions options(cx);
    options.setFileAndLine(mURL.get(), 1)
           .setVersion(JSVERSION_DEFAULT);
    nsJSUtils::EvaluateOptions evalOptions(cx);
    rv = nsJSUtils::EvaluateString(cx, NS_ConvertUTF8toUTF16(script),
                                   globalJSObject, options, evalOptions, &v);

    if (NS_FAILED(rv)) {
        return NS_ERROR_MALFORMED_URI;
    } else if (!v.isString()) {
        return NS_ERROR_DOM_RETVAL_UNDEFINED;
    } else {
        MOZ_ASSERT(rv != NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW,
                   "How did we get a non-undefined return value?");
        nsAutoJSString result;
        if (!result.init(cx, v)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        char *bytes;
        uint32_t bytesLen;
        NS_NAMED_LITERAL_CSTRING(isoCharset, "ISO-8859-1");
        NS_NAMED_LITERAL_CSTRING(utf8Charset, "UTF-8");
        const nsCString *charset;
        if (IsISO88591(result)) {
            // For compatibility, if the result is ISO-8859-1, we use
            // ISO-8859-1, so that people can compatibly create images
            // using javascript: URLs.
            bytes = ToNewCString(result);
            bytesLen = result.Length();
            charset = &isoCharset;
        }
        else {
            bytes = ToNewUTF8String(result, &bytesLen);
            charset = &utf8Charset;
        }
        aChannel->SetContentCharset(*charset);
        if (bytes)
            rv = NS_NewByteInputStream(getter_AddRefs(mInnerStream),
                                       bytes, bytesLen,
                                       NS_ASSIGNMENT_ADOPT);
        else
            rv = NS_ERROR_OUT_OF_MEMORY;
    }

    return rv;
}

////////////////////////////////////////////////////////////////////////////////

class nsJSChannel : public nsIChannel,
                    public nsIStreamListener,
                    public nsIScriptChannel,
                    public nsIPropertyBag2
{
public:
    nsJSChannel();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISCRIPTCHANNEL
    NS_FORWARD_SAFE_NSIPROPERTYBAG(mPropertyBag)
    NS_FORWARD_SAFE_NSIPROPERTYBAG2(mPropertyBag)

    nsresult Init(nsIURI *aURI);

    // Actually evaluate the script.
    void EvaluateScript();
    
protected:
    virtual ~nsJSChannel();

    nsresult StopAll();

    void NotifyListener();

    void CleanupStrongRefs();
    
protected:
    nsCOMPtr<nsIChannel>    mStreamChannel;
    nsCOMPtr<nsIPropertyBag2> mPropertyBag;
    nsCOMPtr<nsIStreamListener> mListener;  // Our final listener
    nsCOMPtr<nsISupports> mContext; // The context passed to AsyncOpen
    nsCOMPtr<nsPIDOMWindowInner> mOriginalInnerWindow;  // The inner window our load
                                                        // started against.
    // If we blocked onload on a document in AsyncOpen, this is the document we
    // did it on.
    nsCOMPtr<nsIDocument>   mDocumentOnloadBlockedOn;

    nsresult mStatus; // Our status

    nsLoadFlags             mLoadFlags;
    nsLoadFlags             mActualLoadFlags; // See AsyncOpen

    RefPtr<nsJSThunk>     mIOThunk;
    PopupControlState       mPopupState;
    uint32_t                mExecutionPolicy;
    bool                    mIsAsync;
    bool                    mIsActive;
    bool                    mOpenedStreamChannel;
};

nsJSChannel::nsJSChannel() :
    mStatus(NS_OK),
    mLoadFlags(LOAD_NORMAL),
    mActualLoadFlags(LOAD_NORMAL),
    mPopupState(openOverridden),
    mExecutionPolicy(NO_EXECUTION),
    mIsAsync(true),
    mIsActive(false),
    mOpenedStreamChannel(false)
{
}

nsJSChannel::~nsJSChannel()
{
}

nsresult nsJSChannel::StopAll()
{
    nsresult rv = NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIWebNavigation> webNav;
    NS_QueryNotificationCallbacks(mStreamChannel, webNav);

    NS_ASSERTION(webNav, "Can't get nsIWebNavigation from channel!");
    if (webNav) {
        rv = webNav->Stop(nsIWebNavigation::STOP_ALL);
    }

    return rv;
}

nsresult nsJSChannel::Init(nsIURI *aURI)
{
    RefPtr<nsJSURI> jsURI;
    nsresult rv = aURI->QueryInterface(kJSURICID,
                                       getter_AddRefs(jsURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // Create the nsIStreamIO layer used by the nsIStreamIOChannel.
    mIOThunk = new nsJSThunk();

    // Create a stock input stream channel...
    // Remember, until AsyncOpen is called, the script will not be evaluated
    // and the underlying Input Stream will not be created...
    nsCOMPtr<nsIChannel> channel;

    nsCOMPtr<nsIPrincipal> nullPrincipal = nsNullPrincipal::Create();

    // If the resultant script evaluation actually does return a value, we
    // treat it as html.
    // The following channel is never openend, so it does not matter what
    // securityFlags we pass; let's follow the principle of least privilege.
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                  aURI,
                                  mIOThunk,
                                  nullPrincipal,
                                  nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                                  nsIContentPolicy::TYPE_OTHER,
                                  NS_LITERAL_CSTRING("text/html"));
    if (NS_FAILED(rv)) return rv;

    rv = mIOThunk->Init(aURI);
    if (NS_SUCCEEDED(rv)) {
        mStreamChannel = channel;
        mPropertyBag = do_QueryInterface(channel);
        nsCOMPtr<nsIWritablePropertyBag2> writableBag =
            do_QueryInterface(channel);
        if (writableBag && jsURI->GetBaseURI()) {
            writableBag->SetPropertyAsInterface(NS_LITERAL_STRING("baseURI"),
                                                jsURI->GetBaseURI());
        }
    }

    return rv;
}

//
// nsISupports implementation...
//

NS_IMPL_ISUPPORTS(nsJSChannel, nsIChannel, nsIRequest, nsIRequestObserver,
                  nsIStreamListener, nsIScriptChannel, nsIPropertyBag,
                  nsIPropertyBag2)

//
// nsIRequest implementation...
//

NS_IMETHODIMP
nsJSChannel::GetName(nsACString &aResult)
{
    return mStreamChannel->GetName(aResult);
}

NS_IMETHODIMP
nsJSChannel::IsPending(bool *aResult)
{
    *aResult = mIsActive;
    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::GetStatus(nsresult *aResult)
{
    if (NS_SUCCEEDED(mStatus) && mOpenedStreamChannel) {
        return mStreamChannel->GetStatus(aResult);
    }
    
    *aResult = mStatus;
        
    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::Cancel(nsresult aStatus)
{
    mStatus = aStatus;

    if (mOpenedStreamChannel) {
        mStreamChannel->Cancel(aStatus);
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::Suspend()
{
    return mStreamChannel->Suspend();
}

NS_IMETHODIMP
nsJSChannel::Resume()
{
    return mStreamChannel->Resume();
}

//
// nsIChannel implementation
//

NS_IMETHODIMP
nsJSChannel::GetOriginalURI(nsIURI * *aURI)
{
    return mStreamChannel->GetOriginalURI(aURI);
}

NS_IMETHODIMP
nsJSChannel::SetOriginalURI(nsIURI *aURI)
{
    return mStreamChannel->SetOriginalURI(aURI);
}

NS_IMETHODIMP
nsJSChannel::GetURI(nsIURI * *aURI)
{
    return mStreamChannel->GetURI(aURI);
}

NS_IMETHODIMP
nsJSChannel::Open(nsIInputStream **aResult)
{
    nsresult rv = mIOThunk->EvaluateScript(mStreamChannel, mPopupState,
                                           mExecutionPolicy,
                                           mOriginalInnerWindow);
    NS_ENSURE_SUCCESS(rv, rv);

    return mStreamChannel->Open(aResult);
}

NS_IMETHODIMP
nsJSChannel::Open2(nsIInputStream** aStream)
{
    nsCOMPtr<nsIStreamListener> listener;
    nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
    NS_ENSURE_SUCCESS(rv, rv);
    return Open(aStream);
}

NS_IMETHODIMP
nsJSChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
#ifdef DEBUG
    {
    nsCOMPtr<nsILoadInfo> loadInfo = nsIChannel::GetLoadInfo();
    MOZ_ASSERT(!loadInfo || loadInfo->GetSecurityMode() == 0 ||
               loadInfo->GetInitialSecurityCheckDone(),
               "security flags in loadInfo but asyncOpen2() not called");
    }
#endif

    NS_ENSURE_ARG(aListener);

    // First make sure that we have a usable inner window; we'll want to make
    // sure that we execute against that inner and no other.
    nsIScriptGlobalObject* global = GetGlobalObject(this);
    if (!global) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win(do_QueryInterface(global));
    NS_ASSERTION(win, "Our global is not a window??");

    // Make sure we create a new inner window if one doesn't already exist (see
    // bug 306630).
    mOriginalInnerWindow = win->EnsureInnerWindow();
    if (!mOriginalInnerWindow) {
        return NS_ERROR_NOT_AVAILABLE;
    }
    
    mListener = aListener;
    mContext = aContext;

    mIsActive = true;

    // Temporarily set the LOAD_BACKGROUND flag to suppress load group observer
    // notifications (and hence nsIWebProgressListener notifications) from
    // being dispatched.  This is required since we suppress LOAD_DOCUMENT_URI,
    // which means that the DocLoader would not generate document start and
    // stop notifications (see bug 257875).
    mActualLoadFlags = mLoadFlags;
    mLoadFlags |= LOAD_BACKGROUND;

    // Add the javascript channel to its loadgroup so that we know if
    // network loads were canceled or not...
    nsCOMPtr<nsILoadGroup> loadGroup;
    mStreamChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
        nsresult rv = loadGroup->AddRequest(this, nullptr);
        if (NS_FAILED(rv)) {
            mIsActive = false;
            CleanupStrongRefs();
            return rv;
        }
    }

    mDocumentOnloadBlockedOn = mOriginalInnerWindow->GetExtantDoc();
    if (mDocumentOnloadBlockedOn) {
        // If we're a document channel, we need to actually block onload on our
        // _parent_ document.  This is because we don't actually set our
        // LOAD_DOCUMENT_URI flag, so a docloader we're loading in as the
        // document channel will claim to not be busy, and our parent's onload
        // could fire too early.
        nsLoadFlags loadFlags;
        mStreamChannel->GetLoadFlags(&loadFlags);
        if (loadFlags & LOAD_DOCUMENT_URI) {
            mDocumentOnloadBlockedOn =
                mDocumentOnloadBlockedOn->GetParentDocument();
        }
    }
    if (mDocumentOnloadBlockedOn) {
        mDocumentOnloadBlockedOn->BlockOnload();
    }


    mPopupState = win->GetPopupControlState();

    void (nsJSChannel::*method)();
    if (mIsAsync) {
        // post an event to do the rest
        method = &nsJSChannel::EvaluateScript;
    } else {
        EvaluateScript();
        if (mOpenedStreamChannel) {
            // That will handle notifying things
            return NS_OK;
        }

        NS_ASSERTION(NS_FAILED(mStatus), "We should have failed _somehow_");

        // mStatus is going to be NS_ERROR_DOM_RETVAL_UNDEFINED if we didn't
        // have any content resulting from the execution and NS_BINDING_ABORTED
        // if something we did causes our own load to be stopped.  Return
        // success in those cases, and error out in all others.
        if (mStatus != NS_ERROR_DOM_RETVAL_UNDEFINED &&
            mStatus != NS_BINDING_ABORTED) {
            // Note that calling EvaluateScript() handled removing us from the
            // loadgroup and marking us as not active anymore.
            CleanupStrongRefs();
            return mStatus;
        }

        // We're returning success from asyncOpen(), but we didn't open a
        // stream channel.  We'll have to notify ourselves, but make sure to do
        // it asynchronously.
        method = &nsJSChannel::NotifyListener;            
    }

    nsresult rv = NS_DispatchToCurrentThread(mozilla::NewRunnableMethod(this, method));

    if (NS_FAILED(rv)) {
        loadGroup->RemoveRequest(this, nullptr, rv);
        mIsActive = false;
        CleanupStrongRefs();
    }
    return rv;
}

NS_IMETHODIMP
nsJSChannel::AsyncOpen2(nsIStreamListener *aListener)
{
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return AsyncOpen(listener, nullptr);
}

void
nsJSChannel::EvaluateScript()
{
    // Synchronously execute the script...
    // mIsActive is used to indicate the the request is 'busy' during the
    // the script evaluation phase.  This means that IsPending() will 
    // indicate the the request is busy while the script is executing...

    // Note that we want to be in the loadgroup and pending while we evaluate
    // the script, so that we find out if the loadgroup gets canceled by the
    // script execution (in which case we shouldn't pump out data even if the
    // script returns it).
    
    if (NS_SUCCEEDED(mStatus)) {
        nsresult rv = mIOThunk->EvaluateScript(mStreamChannel, mPopupState,
                                               mExecutionPolicy,
                                               mOriginalInnerWindow);

        // Note that evaluation may have canceled us, so recheck mStatus again
        if (NS_FAILED(rv) && NS_SUCCEEDED(mStatus)) {
            mStatus = rv;
        }
    }
    
    // Remove the javascript channel from its loadgroup...
    nsCOMPtr<nsILoadGroup> loadGroup;
    mStreamChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
        loadGroup->RemoveRequest(this, nullptr, mStatus);
    }

    // Reset load flags to their original value...
    mLoadFlags = mActualLoadFlags;

    // We're no longer active, it's now up to the stream channel to do
    // the loading, if needed.
    mIsActive = false;

    if (NS_FAILED(mStatus)) {
        if (mIsAsync) {
            NotifyListener();
        }
        return;
    }
    
    // EvaluateScript() succeeded, and we were not canceled, that
    // means there's data to parse as a result of evaluating the
    // script.

    // Get the stream channels load flags (!= mLoadFlags).
    nsLoadFlags loadFlags;
    mStreamChannel->GetLoadFlags(&loadFlags);

    uint32_t disposition;
    if (NS_FAILED(mStreamChannel->GetContentDisposition(&disposition)))
        disposition = nsIChannel::DISPOSITION_INLINE;
    if (loadFlags & LOAD_DOCUMENT_URI && disposition != nsIChannel::DISPOSITION_ATTACHMENT) {
        // We're loaded as the document channel and not expecting to download
        // the result. If we go on, we'll blow away the current document. Make
        // sure that's ok. If so, stop all pending network loads.

        nsCOMPtr<nsIDocShell> docShell;
        NS_QueryNotificationCallbacks(mStreamChannel, docShell);
        if (docShell) {
            nsCOMPtr<nsIContentViewer> cv;
            docShell->GetContentViewer(getter_AddRefs(cv));

            if (cv) {
                bool okToUnload;

                if (NS_SUCCEEDED(cv->PermitUnload(&okToUnload)) &&
                    !okToUnload) {
                    // The user didn't want to unload the current
                    // page, translate this into an undefined
                    // return from the javascript: URL...
                    mStatus = NS_ERROR_DOM_RETVAL_UNDEFINED;
                }
            }
        }

        if (NS_SUCCEEDED(mStatus)) {
            mStatus = StopAll();
        }
    }

    if (NS_FAILED(mStatus)) {
        if (mIsAsync) {
            NotifyListener();
        }
        return;
    }
    
    mStatus = mStreamChannel->AsyncOpen(this, mContext);
    if (NS_SUCCEEDED(mStatus)) {
        // mStreamChannel will call OnStartRequest and OnStopRequest on
        // us, so we'll be sure to call them on our listener.
        mOpenedStreamChannel = true;

        // Now readd ourselves to the loadgroup so we can receive
        // cancellation notifications.
        mIsActive = true;
        if (loadGroup) {
            mStatus = loadGroup->AddRequest(this, nullptr);

            // If AddRequest failed, that's OK.  The key is to make sure we get
            // cancelled if needed, and that call just canceled us if it
            // failed.  We'll still get notified by the stream channel when it
            // finishes.
        }
        
    } else if (mIsAsync) {
        NotifyListener();
    }

    return;
}

void
nsJSChannel::NotifyListener()
{
    mListener->OnStartRequest(this, mContext);
    mListener->OnStopRequest(this, mContext, mStatus);

    CleanupStrongRefs();
}

void
nsJSChannel::CleanupStrongRefs()
{
    mListener = nullptr;
    mContext = nullptr;
    mOriginalInnerWindow = nullptr;
    if (mDocumentOnloadBlockedOn) {
        mDocumentOnloadBlockedOn->UnblockOnload(false);
        mDocumentOnloadBlockedOn = nullptr;
    }
}

NS_IMETHODIMP
nsJSChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;

    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    // Figure out whether the LOAD_BACKGROUND bit in aLoadFlags is
    // actually right.
    bool bogusLoadBackground = false;
    if (mIsActive && !(mActualLoadFlags & LOAD_BACKGROUND) &&
        (aLoadFlags & LOAD_BACKGROUND)) {
        // We're getting a LOAD_BACKGROUND, but it's probably just our own fake
        // flag being mirrored to us.  The one exception is if our loadgroup is
        // LOAD_BACKGROUND.
        bool loadGroupIsBackground = false;
        nsCOMPtr<nsILoadGroup> loadGroup;
        mStreamChannel->GetLoadGroup(getter_AddRefs(loadGroup));
        if (loadGroup) {
            nsLoadFlags loadGroupFlags;
            loadGroup->GetLoadFlags(&loadGroupFlags);
            loadGroupIsBackground = ((loadGroupFlags & LOAD_BACKGROUND) != 0);
        }
        bogusLoadBackground = !loadGroupIsBackground;
    }

    // Classifying a javascript: URI doesn't help us, and requires
    // NSS to boot, which we don't have in content processes.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=617838.
    aLoadFlags &= ~LOAD_CLASSIFY_URI;

    // Since the javascript channel is never the actual channel that
    // any data is loaded through, don't ever set the
    // LOAD_DOCUMENT_URI flag on it, since that could lead to two
    // 'document channels' in the loadgroup if a javascript: URL is
    // loaded while a document is being loaded in the same window.

    // XXXbz this, and a whole lot of other hackery, could go away if we'd just
    // cancel the current document load on javascript: load start like IE does.
    
    mLoadFlags = aLoadFlags & ~LOAD_DOCUMENT_URI;

    if (bogusLoadBackground) {
        aLoadFlags = aLoadFlags & ~LOAD_BACKGROUND;
    }

    mActualLoadFlags = aLoadFlags;

    // ... but the underlying stream channel should get this bit, if
    // set, since that'll be the real document channel if the
    // javascript: URL generated data.

    return mStreamChannel->SetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP
nsJSChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    return mStreamChannel->GetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsJSChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    if (aLoadGroup) {
        bool streamPending;
        nsresult rv = mStreamChannel->IsPending(&streamPending);
        NS_ENSURE_SUCCESS(rv, rv);

        if (streamPending) {
            nsCOMPtr<nsILoadGroup> curLoadGroup;
            mStreamChannel->GetLoadGroup(getter_AddRefs(curLoadGroup));

            if (aLoadGroup != curLoadGroup) {
                // Move the stream channel to our new loadgroup.  Make sure to
                // add it before removing it, so that we don't trigger onload
                // by accident.
                aLoadGroup->AddRequest(mStreamChannel, nullptr);
                if (curLoadGroup) {
                    curLoadGroup->RemoveRequest(mStreamChannel, nullptr,
                                                NS_BINDING_RETARGETED);
                }
            }
        }
    }
    
    return mStreamChannel->SetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsJSChannel::GetOwner(nsISupports* *aOwner)
{
    return mStreamChannel->GetOwner(aOwner);
}

NS_IMETHODIMP
nsJSChannel::SetOwner(nsISupports* aOwner)
{
    return mStreamChannel->SetOwner(aOwner);
}

NS_IMETHODIMP
nsJSChannel::GetLoadInfo(nsILoadInfo* *aLoadInfo)
{
    return mStreamChannel->GetLoadInfo(aLoadInfo);
}

NS_IMETHODIMP
nsJSChannel::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
    return mStreamChannel->SetLoadInfo(aLoadInfo);
}

NS_IMETHODIMP
nsJSChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aCallbacks)
{
    return mStreamChannel->GetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP
nsJSChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
    return mStreamChannel->SetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP 
nsJSChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    return mStreamChannel->GetSecurityInfo(aSecurityInfo);
}

NS_IMETHODIMP
nsJSChannel::GetContentType(nsACString &aContentType)
{
    return mStreamChannel->GetContentType(aContentType);
}

NS_IMETHODIMP
nsJSChannel::SetContentType(const nsACString &aContentType)
{
    return mStreamChannel->SetContentType(aContentType);
}

NS_IMETHODIMP
nsJSChannel::GetContentCharset(nsACString &aContentCharset)
{
    return mStreamChannel->GetContentCharset(aContentCharset);
}

NS_IMETHODIMP
nsJSChannel::SetContentCharset(const nsACString &aContentCharset)
{
    return mStreamChannel->SetContentCharset(aContentCharset);
}

NS_IMETHODIMP
nsJSChannel::GetContentDisposition(uint32_t *aContentDisposition)
{
    return mStreamChannel->GetContentDisposition(aContentDisposition);
}

NS_IMETHODIMP
nsJSChannel::SetContentDisposition(uint32_t aContentDisposition)
{
    return mStreamChannel->SetContentDisposition(aContentDisposition);
}

NS_IMETHODIMP
nsJSChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
    return mStreamChannel->GetContentDispositionFilename(aContentDispositionFilename);
}

NS_IMETHODIMP
nsJSChannel::SetContentDispositionFilename(const nsAString &aContentDispositionFilename)
{
    return mStreamChannel->SetContentDispositionFilename(aContentDispositionFilename);
}

NS_IMETHODIMP
nsJSChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
    return mStreamChannel->GetContentDispositionHeader(aContentDispositionHeader);
}

NS_IMETHODIMP
nsJSChannel::GetContentLength(int64_t *aContentLength)
{
    return mStreamChannel->GetContentLength(aContentLength);
}

NS_IMETHODIMP
nsJSChannel::SetContentLength(int64_t aContentLength)
{
    return mStreamChannel->SetContentLength(aContentLength);
}

NS_IMETHODIMP
nsJSChannel::OnStartRequest(nsIRequest* aRequest,
                            nsISupports* aContext)
{
    NS_ENSURE_TRUE(aRequest == mStreamChannel, NS_ERROR_UNEXPECTED);

    return mListener->OnStartRequest(this, aContext);    
}

NS_IMETHODIMP
nsJSChannel::OnDataAvailable(nsIRequest* aRequest,
                             nsISupports* aContext, 
                             nsIInputStream* aInputStream,
                             uint64_t aOffset,
                             uint32_t aCount)
{
    NS_ENSURE_TRUE(aRequest == mStreamChannel, NS_ERROR_UNEXPECTED);

    return mListener->OnDataAvailable(this, aContext, aInputStream, aOffset,
                                      aCount);
}

NS_IMETHODIMP
nsJSChannel::OnStopRequest(nsIRequest* aRequest,
                           nsISupports* aContext,
                           nsresult aStatus)
{
    NS_ENSURE_TRUE(aRequest == mStreamChannel, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIStreamListener> listener = mListener;

    CleanupStrongRefs();

    // Make sure aStatus matches what GetStatus() returns
    if (NS_FAILED(mStatus)) {
        aStatus = mStatus;
    }
    
    nsresult rv = listener->OnStopRequest(this, aContext, aStatus);

    nsCOMPtr<nsILoadGroup> loadGroup;
    mStreamChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
        loadGroup->RemoveRequest(this, nullptr, mStatus);
    }

    mIsActive = false;

    return rv;
}

NS_IMETHODIMP
nsJSChannel::SetExecutionPolicy(uint32_t aPolicy)
{
    NS_ENSURE_ARG(aPolicy <= EXECUTE_NORMAL);
    
    mExecutionPolicy = aPolicy;
    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::GetExecutionPolicy(uint32_t* aPolicy)
{
    *aPolicy = mExecutionPolicy;
    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::SetExecuteAsync(bool aIsAsync)
{
    if (!mIsActive) {
        mIsAsync = aIsAsync;
    }
    // else ignore this call
    NS_WARNING_ASSERTION(!mIsActive,
                         "Calling SetExecuteAsync on active channel?");

    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::GetExecuteAsync(bool* aIsAsync)
{
    *aIsAsync = mIsAsync;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsJSProtocolHandler::nsJSProtocolHandler()
{
}

nsresult
nsJSProtocolHandler::Init()
{
    return NS_OK;
}

nsJSProtocolHandler::~nsJSProtocolHandler()
{
}

NS_IMPL_ISUPPORTS(nsJSProtocolHandler, nsIProtocolHandler)

nsresult
nsJSProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJSProtocolHandler* ph = new nsJSProtocolHandler();
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

nsresult 
nsJSProtocolHandler::EnsureUTF8Spec(const nsAFlatCString &aSpec, const char *aCharset, 
                                    nsACString &aUTF8Spec)
{
  aUTF8Spec.Truncate();

  nsresult rv;
  
  if (!mTextToSubURI) {
    mTextToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  nsAutoString uStr;
  rv = mTextToSubURI->UnEscapeNonAsciiURI(nsDependentCString(aCharset), aSpec, uStr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsASCII(uStr))
    NS_EscapeURL(NS_ConvertUTF16toUTF8(uStr), esc_AlwaysCopy | esc_OnlyNonASCII, aUTF8Spec);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsJSProtocolHandler::GetScheme(nsACString &result)
{
    result = "javascript";
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::GetDefaultPort(int32_t *result)
{
    *result = -1;        // no port for javascript: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::GetProtocolFlags(uint32_t *result)
{
    *result = URI_NORELATIVE | URI_NOAUTH | URI_INHERITS_SECURITY_CONTEXT |
        URI_LOADABLE_BY_ANYONE | URI_NON_PERSISTABLE | URI_OPENING_EXECUTES_SCRIPT;
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewURI(const nsACString &aSpec,
                            const char *aCharset,
                            nsIURI *aBaseURI,
                            nsIURI **result)
{
    nsresult rv;

    // javascript: URLs (currently) have no additional structure beyond that
    // provided by standard URLs, so there is no "outer" object given to
    // CreateInstance.

    nsCOMPtr<nsIURI> url = new nsJSURI(aBaseURI);

    if (!aCharset || !nsCRT::strcasecmp("UTF-8", aCharset))
      rv = url->SetSpec(aSpec);
    else {
      nsAutoCString utf8Spec;
      rv = EnsureUTF8Spec(PromiseFlatCString(aSpec), aCharset, utf8Spec);
      if (NS_SUCCEEDED(rv)) {
        if (utf8Spec.IsEmpty())
          rv = url->SetSpec(aSpec);
        else
          rv = url->SetSpec(utf8Spec);
      }
    }

    if (NS_FAILED(rv)) {
        return rv;
    }

    url.forget(result);
    return rv;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewChannel2(nsIURI* uri,
                                 nsILoadInfo* aLoadInfo,
                                 nsIChannel** result)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(uri);

    RefPtr<nsJSChannel> channel = new nsJSChannel();
    if (!channel) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = channel->Init(uri);
    NS_ENSURE_SUCCESS(rv, rv);

    // set the loadInfo on the new channel
    rv = channel->SetLoadInfo(aLoadInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    if (NS_SUCCEEDED(rv)) {
        channel.forget(result);
    }
    return rv;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    return NewChannel2(uri, nullptr, result);
}

NS_IMETHODIMP 
nsJSProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
{
    // don't override anything.  
    *_retval = false;
    return NS_OK;
}

////////////////////////////////////////////////////////////
// nsJSURI implementation
static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);


NS_IMPL_ADDREF_INHERITED(nsJSURI, mozilla::net::nsSimpleURI)
NS_IMPL_RELEASE_INHERITED(nsJSURI, mozilla::net::nsSimpleURI)

NS_INTERFACE_MAP_BEGIN(nsJSURI)
  if (aIID.Equals(kJSURICID))
      foundInterface = static_cast<nsIURI*>(this);
  else if (aIID.Equals(kThisSimpleURIImplementationCID)) {
      // Need to return explicitly here, because if we just set foundInterface
      // to null the NS_INTERFACE_MAP_END_INHERITING will end up calling into
      // nsSimplURI::QueryInterface and finding something for this CID.
      *aInstancePtr = nullptr;
      return NS_NOINTERFACE;
  }
  else
NS_INTERFACE_MAP_END_INHERITING(mozilla::net::nsSimpleURI)

// nsISerializable methods:

NS_IMETHODIMP
nsJSURI::Read(nsIObjectInputStream* aStream)
{
    nsresult rv = mozilla::net::nsSimpleURI::Read(aStream);
    if (NS_FAILED(rv)) return rv;

    bool haveBase;
    rv = aStream->ReadBoolean(&haveBase);
    if (NS_FAILED(rv)) return rv;

    if (haveBase) {
        nsCOMPtr<nsISupports> supports;
        rv = aStream->ReadObject(true, getter_AddRefs(supports));
        if (NS_FAILED(rv)) return rv;
        mBaseURI = do_QueryInterface(supports);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsJSURI::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv = mozilla::net::nsSimpleURI::Write(aStream);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteBoolean(mBaseURI != nullptr);
    if (NS_FAILED(rv)) return rv;

    if (mBaseURI) {
        rv = aStream->WriteObject(mBaseURI, true);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

// nsIIPCSerializableURI
void
nsJSURI::Serialize(mozilla::ipc::URIParams& aParams)
{
    using namespace mozilla::ipc;

    JSURIParams jsParams;
    URIParams simpleParams;

    mozilla::net::nsSimpleURI::Serialize(simpleParams);

    jsParams.simpleParams() = simpleParams;
    if (mBaseURI) {
        SerializeURI(mBaseURI, jsParams.baseURI());
    } else {
        jsParams.baseURI() = mozilla::void_t();
    }

    aParams = jsParams;
}

bool
nsJSURI::Deserialize(const mozilla::ipc::URIParams& aParams)
{
    using namespace mozilla::ipc;

    if (aParams.type() != URIParams::TJSURIParams) {
        NS_ERROR("Received unknown parameters from the other process!");
        return false;
    }

    const JSURIParams& jsParams = aParams.get_JSURIParams();
    mozilla::net::nsSimpleURI::Deserialize(jsParams.simpleParams());

    if (jsParams.baseURI().type() != OptionalURIParams::Tvoid_t) {
        mBaseURI = DeserializeURI(jsParams.baseURI().get_URIParams());
    } else {
        mBaseURI = nullptr;
    }
    return true;
}

// nsSimpleURI methods:
/* virtual */ mozilla::net::nsSimpleURI*
nsJSURI::StartClone(mozilla::net::nsSimpleURI::RefHandlingEnum refHandlingMode,
                    const nsACString& newRef)
{
    nsCOMPtr<nsIURI> baseClone;
    if (mBaseURI) {
      // Note: We preserve ref on *base* URI, regardless of ref handling mode.
      nsresult rv = mBaseURI->Clone(getter_AddRefs(baseClone));
      if (NS_FAILED(rv)) {
        return nullptr;
      }
    }

    nsJSURI* url = new nsJSURI(baseClone);
    SetRefOnClone(url, refHandlingMode, newRef);
    return url;
}

/* virtual */ nsresult
nsJSURI::EqualsInternal(nsIURI* aOther,
                        mozilla::net::nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                        bool* aResult)
{
    NS_ENSURE_ARG_POINTER(aOther);
    NS_PRECONDITION(aResult, "null pointer for outparam");

    RefPtr<nsJSURI> otherJSURI;
    nsresult rv = aOther->QueryInterface(kJSURICID,
                                         getter_AddRefs(otherJSURI));
    if (NS_FAILED(rv)) {
        *aResult = false; // aOther is not a nsJSURI --> not equal.
        return NS_OK;
    }

    // Compare the member data that our base class knows about.
    if (!mozilla::net::nsSimpleURI::EqualsInternal(otherJSURI, aRefHandlingMode)) {
        *aResult = false;
        return NS_OK;
    }

    // Compare the piece of additional member data that we add to base class.
    nsIURI* otherBaseURI = otherJSURI->GetBaseURI();

    if (mBaseURI) {
        // (As noted in StartClone, we always honor refs on mBaseURI)
        return mBaseURI->Equals(otherBaseURI, aResult);
    }

    *aResult = !otherBaseURI;
    return NS_OK;
}

NS_IMETHODIMP 
nsJSURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    *aClassIDNoAlloc = kJSURICID;
    return NS_OK;
}


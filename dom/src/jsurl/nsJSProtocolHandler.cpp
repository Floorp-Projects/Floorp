/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "jsapi.h"
#include "nsCRT.h"
#include "nsDOMError.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsJSProtocolHandler.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
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
#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsIJSContextStack.h"
#include "nsIScriptChannel.h"
#include "nsIDocument.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIContentSecurityPolicy.h"

static NS_DEFINE_CID(kJSURICID, NS_JSURI_CID);

class nsJSThunk : public nsIInputStream
{
public:
    nsJSThunk();

    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIINPUTSTREAM(mInnerStream)

    nsresult Init(nsIURI* uri);
    nsresult EvaluateScript(nsIChannel *aChannel,
                            PopupControlState aPopupState,
                            PRUint32 aExecutionPolicy,
                            nsPIDOMWindow *aOriginalInnerWindow);

protected:
    virtual ~nsJSThunk();

    nsCOMPtr<nsIInputStream>    mInnerStream;
    nsCString                   mScript;
    nsCString                   mURL;
};

//
// nsISupports implementation...
//
NS_IMPL_THREADSAFE_ISUPPORTS1(nsJSThunk, nsIInputStream)


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
    nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner;
    NS_QueryNotificationCallbacks(aChannel, globalOwner);
    if (!globalOwner) {
        NS_WARNING("Unable to get an nsIScriptGlobalObjectOwner from the "
                   "channel!");
    }
    if (!globalOwner) {
        return nullptr;
    }

    // So far so good: get the script context from its owner.
    nsIScriptGlobalObject* global = globalOwner->GetScriptGlobalObject();

    NS_ASSERTION(global,
                 "Unable to get an nsIScriptGlobalObject from the "
                 "ScriptGlobalObjectOwner!");
    return global;
}

nsresult nsJSThunk::EvaluateScript(nsIChannel *aChannel,
                                   PopupControlState aPopupState,
                                   PRUint32 aExecutionPolicy,
                                   nsPIDOMWindow *aOriginalInnerWindow)
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
        // No execution without a principal!
        NS_ASSERTION(!owner, "Non-principal owner?");
        NS_WARNING("No principal to execute JS with");
        return NS_ERROR_DOM_RETVAL_UNDEFINED;
    }

    nsresult rv;

    // CSP check: javascript: URIs disabled unless "inline" scripts are
    // allowed.
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = principal->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS(rv, rv);
    if (csp) {
		bool allowsInline;
		rv = csp->GetAllowsInlineScript(&allowsInline);
		NS_ENSURE_SUCCESS(rv, rv);

      if (!allowsInline) {
          // gather information to log with violation report
          nsCOMPtr<nsIURI> uri;
          principal->GetURI(getter_AddRefs(uri));
          nsCAutoString asciiSpec;
          uri->GetAsciiSpec(asciiSpec);
		  csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_INLINE_SCRIPT,
								   NS_ConvertUTF8toUTF16(asciiSpec),
								   NS_ConvertUTF8toUTF16(mURL),
                                   0);
          return NS_ERROR_DOM_RETVAL_UNDEFINED;
      }
    }

    // Get the global object we should be running on.
    nsIScriptGlobalObject* global = GetGlobalObject(aChannel);
    if (!global) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(global));

    // Push our popup control state
    nsAutoPopupStatePusher popupStatePusher(win, aPopupState);

    // Make sure we still have the same inner window as we used to.
    nsPIDOMWindow *innerWin = win->GetCurrentInnerWindow();

    if (innerWin != aOriginalInnerWindow) {
        return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIScriptGlobalObject> innerGlobal = do_QueryInterface(innerWin);

    JSObject *globalJSObject = innerGlobal->GetGlobalJSObject();

    nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(global, &rv));
    if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
    }

    // So far so good: get the script context from its owner.
    nsCOMPtr<nsIScriptContext> scriptContext = global->GetContext();
    if (!scriptContext)
        return NS_ERROR_FAILURE;

    nsCAutoString script(mScript);
    // Unescape the script
    NS_UnescapeURL(script);


    nsCOMPtr<nsIScriptSecurityManager> securityManager;
    securityManager = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    bool useSandbox =
        (aExecutionPolicy == nsIScriptChannel::EXECUTE_IN_SANDBOX);

    if (!useSandbox) {
        //-- Don't outside a sandbox unless the script principal subsumes the
        //   principal of the context.
        nsCOMPtr<nsIPrincipal> objectPrincipal;
        rv = securityManager->
            GetObjectPrincipal(scriptContext->GetNativeContext(),
                               globalJSObject,
                               getter_AddRefs(objectPrincipal));
        if (NS_FAILED(rv))
            return rv;

        bool subsumes;
        rv = principal->Subsumes(objectPrincipal, &subsumes);
        if (NS_FAILED(rv))
            return rv;

        useSandbox = !subsumes;
    }

    nsString result;
    bool isUndefined;

    // Finally, we have everything needed to evaluate the expression.

    if (useSandbox) {
        // We were asked to use a sandbox, or the channel owner isn't allowed
        // to execute in this context.  Evaluate the javascript URL in a
        // sandbox to prevent it from accessing data it doesn't have
        // permissions to access.

        // First check to make sure it's OK to evaluate this script to
        // start with.  For example, script could be disabled.
        JSContext *cx = scriptContext->GetNativeContext();
        JSAutoRequest ar(cx);

        bool ok;
        rv = securityManager->CanExecuteScripts(cx, principal, &ok);
        if (NS_FAILED(rv)) {
            return rv;
        }

        if (!ok) {
            // Treat this as returning undefined from the script.  That's what
            // nsJSContext does.
            return NS_ERROR_DOM_RETVAL_UNDEFINED;
        }

        nsIXPConnect *xpc = nsContentUtils::XPConnect();

        nsCOMPtr<nsIXPConnectJSObjectHolder> sandbox;
        rv = xpc->CreateSandbox(cx, principal, getter_AddRefs(sandbox));
        NS_ENSURE_SUCCESS(rv, rv);

        jsval rval = JSVAL_VOID;

        // Push our JSContext on the context stack so the JS_ValueToString call (and
        // JS_ReportPendingException, if relevant) will use the principal of cx.
        // Note that we do this as late as possible to make popping simpler.
        nsCOMPtr<nsIJSContextStack> stack =
            do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
        if (NS_SUCCEEDED(rv)) {
            rv = stack->Push(cx);
        }
        if (NS_FAILED(rv)) {
            return rv;
        }

        rv = xpc->EvalInSandboxObject(NS_ConvertUTF8toUTF16(script), cx,
                                      sandbox, true, &rval);

        // Propagate and report exceptions that happened in the
        // sandbox.
        if (JS_IsExceptionPending(cx)) {
            JS_ReportPendingException(cx);
            isUndefined = true;
        } else {
            isUndefined = rval == JSVAL_VOID;
        }

        if (!isUndefined && NS_SUCCEEDED(rv)) {
            NS_ASSERTION(JSVAL_IS_STRING(rval), "evalInSandbox is broken");

            nsDependentJSString depStr;
            if (!depStr.init(cx, JSVAL_TO_STRING(rval))) {
                JS_ReportPendingException(cx);
                isUndefined = true;
            } else {
                result = depStr;
            }
        }

        stack->Pop(nullptr);
    } else {
        // No need to use the sandbox, evaluate the script directly in
        // the given scope.
        rv = scriptContext->EvaluateString(NS_ConvertUTF8toUTF16(script),
                                           globalJSObject, // obj
                                           principal,
                                           principal,
                                           mURL.get(),     // url
                                           1,              // line no
                                           JSVERSION_DEFAULT,
                                           &result,
                                           &isUndefined);

        // If there's an error on cx as a result of that call, report
        // it now -- either we're just running under the event loop,
        // so we shouldn't propagate JS exceptions out of here, or we
        // can't be sure that our caller is JS (and if it's not we'll
        // lose the error), or it might be JS that then proceeds to
        // cause an error of its own (which will also make us lose
        // this error).
        JSContext *cx = scriptContext->GetNativeContext();
        JSAutoRequest ar(cx);
        ::JS_ReportPendingException(cx);
    }
    
    if (NS_FAILED(rv)) {
        rv = NS_ERROR_MALFORMED_URI;
    }
    else if (isUndefined) {
        rv = NS_ERROR_DOM_RETVAL_UNDEFINED;
    }
    else {
        char *bytes;
        PRUint32 bytesLen;
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
    nsCOMPtr<nsPIDOMWindow> mOriginalInnerWindow;  // The inner window our load
                                                   // started against.
    // If we blocked onload on a document in AsyncOpen, this is the document we
    // did it on.
    nsCOMPtr<nsIDocument>   mDocumentOnloadBlockedOn;

    nsresult mStatus; // Our status

    nsLoadFlags             mLoadFlags;
    nsLoadFlags             mActualLoadFlags; // See AsyncOpen

    nsRefPtr<nsJSThunk>     mIOThunk;
    PopupControlState       mPopupState;
    PRUint32                mExecutionPolicy;
    bool                    mIsAsync;
    bool                    mIsActive;
    bool                    mOpenedStreamChannel;
};

nsJSChannel::nsJSChannel() :
    mStatus(NS_OK),
    mLoadFlags(LOAD_NORMAL),
    mActualLoadFlags(LOAD_NORMAL),
    mPopupState(openOverridden),
    mExecutionPolicy(EXECUTE_IN_SANDBOX),
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
    nsRefPtr<nsJSURI> jsURI;
    nsresult rv = aURI->QueryInterface(kJSURICID,
                                       getter_AddRefs(jsURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // Create the nsIStreamIO layer used by the nsIStreamIOChannel.
    mIOThunk = new nsJSThunk();
    if (!mIOThunk)
        return NS_ERROR_OUT_OF_MEMORY;

    // Create a stock input stream channel...
    // Remember, until AsyncOpen is called, the script will not be evaluated
    // and the underlying Input Stream will not be created...
    nsCOMPtr<nsIChannel> channel;

    // If the resultant script evaluation actually does return a value, we
    // treat it as html.
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel), aURI, mIOThunk,
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

NS_IMPL_ISUPPORTS7(nsJSChannel, nsIChannel, nsIRequest, nsIRequestObserver,
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
nsJSChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
    NS_ENSURE_ARG(aListener);

    // First make sure that we have a usable inner window; we'll want to make
    // sure that we execute against that inner and no other.
    nsIScriptGlobalObject* global = GetGlobalObject(this);
    if (!global) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(global));
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

    mDocumentOnloadBlockedOn =
        do_QueryInterface(mOriginalInnerWindow->GetExtantDocument());
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

    nsCOMPtr<nsIRunnable> ev = NS_NewRunnableMethod(this, method);
    nsresult rv = NS_DispatchToCurrentThread(ev);

    if (NS_FAILED(rv)) {
        loadGroup->RemoveRequest(this, nullptr, rv);
        mIsActive = false;
        CleanupStrongRefs();
    }
    return rv;
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

    if (loadFlags & LOAD_DOCUMENT_URI) {
        // We're loaded as the document channel. If we go on,
        // we'll blow away the current document. Make sure that's
        // ok. If so, stop all pending network loads.

        nsCOMPtr<nsIDocShell> docShell;
        NS_QueryNotificationCallbacks(mStreamChannel, docShell);
        if (docShell) {
            nsCOMPtr<nsIContentViewer> cv;
            docShell->GetContentViewer(getter_AddRefs(cv));

            if (cv) {
                bool okToUnload;

                if (NS_SUCCEEDED(cv->PermitUnload(false, &okToUnload)) &&
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
nsJSChannel::GetContentDisposition(PRUint32 *aContentDisposition)
{
    return mStreamChannel->GetContentDisposition(aContentDisposition);
}

NS_IMETHODIMP
nsJSChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
    return mStreamChannel->GetContentDispositionFilename(aContentDispositionFilename);
}

NS_IMETHODIMP
nsJSChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
    return mStreamChannel->GetContentDispositionHeader(aContentDispositionHeader);
}

NS_IMETHODIMP
nsJSChannel::GetContentLength(PRInt32 *aContentLength)
{
    return mStreamChannel->GetContentLength(aContentLength);
}

NS_IMETHODIMP
nsJSChannel::SetContentLength(PRInt32 aContentLength)
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
                             PRUint32 aOffset,
                             PRUint32 aCount)
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
nsJSChannel::SetExecutionPolicy(PRUint32 aPolicy)
{
    NS_ENSURE_ARG(aPolicy <= EXECUTE_NORMAL);
    
    mExecutionPolicy = aPolicy;
    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::GetExecutionPolicy(PRUint32* aPolicy)
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
    NS_WARN_IF_FALSE(!mIsActive, "Calling SetExecuteAsync on active channel?");

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

NS_IMPL_ISUPPORTS1(nsJSProtocolHandler, nsIProtocolHandler)

nsresult
nsJSProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJSProtocolHandler* ph = new nsJSProtocolHandler();
    if (!ph)
        return NS_ERROR_OUT_OF_MEMORY;
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
nsJSProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for javascript: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::GetProtocolFlags(PRUint32 *result)
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
      nsCAutoString utf8Spec;
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
nsJSProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    nsresult rv;
    nsJSChannel * channel;

    NS_ENSURE_ARG_POINTER(uri);

    channel = new nsJSChannel();
    if (!channel) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(channel);

    rv = channel->Init(uri);
    if (NS_SUCCEEDED(rv)) {
        *result = channel;
        NS_ADDREF(*result);
    }
    NS_RELEASE(channel);
    return rv;
}

NS_IMETHODIMP 
nsJSProtocolHandler::AllowPort(PRInt32 port, const char *scheme, bool *_retval)
{
    // don't override anything.  
    *_retval = false;
    return NS_OK;
}

////////////////////////////////////////////////////////////
// nsJSURI implementation
static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);


NS_IMPL_ADDREF_INHERITED(nsJSURI, nsSimpleURI)
NS_IMPL_RELEASE_INHERITED(nsJSURI, nsSimpleURI)

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
NS_INTERFACE_MAP_END_INHERITING(nsSimpleURI)

// nsISerializable methods:

NS_IMETHODIMP
nsJSURI::Read(nsIObjectInputStream* aStream)
{
    nsresult rv = nsSimpleURI::Read(aStream);
    if (NS_FAILED(rv)) return rv;

    bool haveBase;
    rv = aStream->ReadBoolean(&haveBase);
    if (NS_FAILED(rv)) return rv;

    if (haveBase) {
        rv = aStream->ReadObject(true, getter_AddRefs(mBaseURI));
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsJSURI::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv = nsSimpleURI::Write(aStream);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteBoolean(mBaseURI != nullptr);
    if (NS_FAILED(rv)) return rv;

    if (mBaseURI) {
        rv = aStream->WriteObject(mBaseURI, true);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

// nsSimpleURI methods:
/* virtual */ nsSimpleURI*
nsJSURI::StartClone(nsSimpleURI::RefHandlingEnum /* ignored */)
{
    nsCOMPtr<nsIURI> baseClone;
    if (mBaseURI) {
      // Note: We preserve ref on *base* URI, regardless of ref handling mode.
      nsresult rv = mBaseURI->Clone(getter_AddRefs(baseClone));
      if (NS_FAILED(rv)) {
        return nullptr;
      }
    }

    return new nsJSURI(baseClone);
}

/* virtual */ nsresult
nsJSURI::EqualsInternal(nsIURI* aOther,
                        nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                        bool* aResult)
{
    NS_ENSURE_ARG_POINTER(aOther);
    NS_PRECONDITION(aResult, "null pointer for outparam");

    nsRefPtr<nsJSURI> otherJSURI;
    nsresult rv = aOther->QueryInterface(kJSURICID,
                                         getter_AddRefs(otherJSURI));
    if (NS_FAILED(rv)) {
        *aResult = false; // aOther is not a nsJSURI --> not equal.
        return NS_OK;
    }

    // Compare the member data that our base class knows about.
    if (!nsSimpleURI::EqualsInternal(otherJSURI, aRefHandlingMode)) {
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


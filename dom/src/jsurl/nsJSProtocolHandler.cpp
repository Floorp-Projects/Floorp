/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "jsapi.h"
#include "nsCRT.h"
#include "nsDOMError.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsJSProtocolHandler.h"
#include "nsNetUtil.h"

#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIStringStream.h"
#include "nsIWindowMediator.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIJSConsoleService.h"
#include "nsIConsoleService.h"
#include "nsXPIDLString.h"
#include "prprf.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);



class nsJSThunk : public nsIInputStream
{
public:
    nsJSThunk();

    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIINPUTSTREAM(mInnerStream)

    nsresult Init(nsIURI* uri);
    nsresult EvaluateScript(nsIChannel *aChannel);
    nsresult BringUpConsole(nsIDOMWindow *aDomWindow);

protected:
    virtual ~nsJSThunk();

    nsCOMPtr<nsIURI>            mURI;
    nsCOMPtr<nsIInputStream>    mInnerStream;
};

//
// nsISupports implementation...
//
NS_IMPL_THREADSAFE_ISUPPORTS1(nsJSThunk, nsIInputStream);


nsJSThunk::nsJSThunk()
{
}

nsJSThunk::~nsJSThunk()
{
}

nsresult nsJSThunk::Init(nsIURI* uri)
{
    NS_ENSURE_ARG_POINTER(uri);

    mURI = uri;
    return NS_OK;
}

nsresult nsJSThunk::EvaluateScript(nsIChannel *aChannel)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aChannel);

    // Get the script string to evaluate...
    nsCAutoString script;
    rv = mURI->GetPath(script);
    if (NS_FAILED(rv)) return rv;

    // Get an interface requestor from the channel callbacks.
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    rv = aChannel->GetNotificationCallbacks(getter_AddRefs(callbacks));

    NS_ASSERTION(NS_SUCCEEDED(rv) && callbacks,
                 "Unable to get an nsIInterfaceRequestor from the channel");
    if (NS_FAILED(rv) || !callbacks) {
        return NS_ERROR_FAILURE;
    }

    // The requestor must be able to get a script global object owner.
    nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner;
    rv = callbacks->GetInterface(NS_GET_IID(nsIScriptGlobalObjectOwner),
                                 getter_AddRefs(globalOwner));

    NS_ASSERTION(NS_SUCCEEDED(rv) && globalOwner, 
                 "Unable to get an nsIScriptGlobalObjectOwner from the "
                 "InterfaceRequestor!");
    if (NS_FAILED(rv) || !globalOwner) {
        return NS_ERROR_FAILURE;
    }

    // So far so good: get the script context from its owner.
    nsCOMPtr<nsIScriptGlobalObject> global;
    rv = globalOwner->GetScriptGlobalObject(getter_AddRefs(global));

    NS_ASSERTION(NS_SUCCEEDED(rv) && global,
                 "Unable to get an nsIScriptGlobalObject from the "
                 "ScriptGlobalObjectOwner!");
    if (NS_FAILED(rv) || !global) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(global, &rv));
    if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
    }

    // If mURI is just "javascript:", we bring up the JavaScript console
    // and return NS_ERROR_DOM_RETVAL_UNDEFINED.
    if (script.IsEmpty()) {
        rv = BringUpConsole(domWindow);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        return NS_ERROR_DOM_RETVAL_UNDEFINED;
    }

    // Now get the DOM Document.  Accessing the document will create one
    // if necessary.  So, basically, this call ensures that a document gets
    // created -- if necessary.
    nsCOMPtr<nsIDOMDocument> doc;

    rv = domWindow->GetDocument(getter_AddRefs(doc));
    NS_ASSERTION(doc, "No DOMDocument!");
    if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIScriptContext> scriptContext;
    rv = global->GetContext(getter_AddRefs(scriptContext));
    if (NS_FAILED(rv))
        return rv;

    if (!scriptContext) return NS_ERROR_FAILURE;

    // Unescape the script
    NS_UnescapeURL(script);

    // Get the url.
    nsCAutoString url;
    rv = mURI->GetSpec(url);
    if (NS_FAILED(rv)) return rv;

    // Get principal of code for execution
    nsCOMPtr<nsISupports> owner;
    rv = aChannel->GetOwner(getter_AddRefs(owner));
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIScriptSecurityManager> securityManager;
    securityManager = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    if (owner) {
        principal = do_QueryInterface(owner, &rv);
        NS_ASSERTION(principal, "Channel's owner is not a principal");
        if (!principal)
            return NS_ERROR_FAILURE;

        //-- Don't run if the script principal is different from the
        //   principal of the context, with two exceptions: we allow
        //   the script to run if the script has the system principal
        //   or the context is about:blank.
        nsCOMPtr<nsIPrincipal> objectPrincipal;
        rv = securityManager->GetObjectPrincipal(
                                (JSContext*)scriptContext->GetNativeContext(),
                                global->GetGlobalJSObject(),
                                getter_AddRefs(objectPrincipal));
        if (NS_FAILED(rv))
            return rv;

        PRBool equals = PR_FALSE;
        if ((NS_FAILED(objectPrincipal->Equals(principal, &equals)) || !equals)) {
            // If the principals aren't equal

            nsCOMPtr<nsIPrincipal> systemPrincipal;
            securityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
            if (principal.get() != systemPrincipal.get()) {
                // and the script to be run does not have the system principal

                nsCOMPtr<nsICodebasePrincipal> 
                    objectCodebase(do_QueryInterface(objectPrincipal));
                nsXPIDLCString objectOrigin;
                rv = objectCodebase->GetOrigin(getter_Copies(objectOrigin));
                if (PL_strcmp("about:blank", objectOrigin) != 0) {
                    // and the target window is not about:blank, then
                    // don't run the script. Print a message to the console and
                    // return undefined.

                    nsCOMPtr<nsIConsoleService> 
                        console(do_GetService("@mozilla.org/consoleservice;1"));
                    if (console) {
                            console->LogStringMessage(
                                NS_LITERAL_STRING("Attempt to load a javascript: URL from one host\nin a window displaying content from another host\nwas blocked by the security manager.").get());
                    }
                    return NS_ERROR_DOM_RETVAL_UNDEFINED;
                }
            }
        }
    }
    else {
        // No owner from channel, use the current URI to generate a principal
        rv = securityManager->GetCodebasePrincipal(mURI, 
                                                   getter_AddRefs(principal));
        if (NS_FAILED(rv) || !principal) {
            return NS_ERROR_FAILURE;
        }
    }

    // Finally, we have everything needed to evaluate the expression.
    nsString result;
    PRBool bIsUndefined;
    {
        NS_ConvertUTF8toUCS2 scriptString(script);
        rv = scriptContext->EvaluateString(scriptString,
                                           nsnull,      // obj
                                           principal,
                                           url.get(),   // url
                                           1,           // line no
                                           nsnull,
                                           result,
                                           &bIsUndefined);
    }

    if (NS_FAILED(rv)) {
        rv = NS_ERROR_MALFORMED_URI;
    }
    else if (bIsUndefined) {
        rv = NS_ERROR_DOM_RETVAL_UNDEFINED;
    }
    else {
        // NS_NewStringInputStream calls ToNewCString
        // XXXbe this should not decimate! pass back UCS-2 to necko
        rv = NS_NewStringInputStream(getter_AddRefs(mInnerStream), result);
    }
    return rv;
}

nsresult nsJSThunk::BringUpConsole(nsIDOMWindow *aDomWindow)
{
    nsresult rv;

    // First, get the Window Mediator service.
    nsCOMPtr<nsIWindowMediator> windowMediator;

    windowMediator = do_GetService(kWindowMediatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // Next, find out whether there's a console already open.
    nsCOMPtr<nsIDOMWindowInternal> console;
    rv = windowMediator->GetMostRecentWindow(NS_LITERAL_STRING("global:console").get(),
                                             getter_AddRefs(console));
    if (NS_FAILED(rv)) return rv;

    if (console) {
        // If the console is already open, bring it to the top.
        rv = console->Focus();
    } else {
        nsCOMPtr<nsIJSConsoleService> jsconsole;

        jsconsole = do_GetService("@mozilla.org/embedcomp/jsconsole-service;1", &rv);
        if (NS_FAILED(rv) || !jsconsole) return rv;
        jsconsole->Open(aDomWindow);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

class nsJSChannel : public nsIChannel
{
public:
    nsJSChannel();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL

    nsresult Init(nsIURI *aURI);

protected:
    virtual ~nsJSChannel();

protected:
    nsCOMPtr<nsIChannel>    mStreamChannel;

    nsLoadFlags             mLoadFlags;

    nsJSThunk *             mIOThunk;
    PRBool                  mIsActive;
};

nsJSChannel::nsJSChannel() :
    mLoadFlags(LOAD_NORMAL),
    mIOThunk(nsnull),
    mIsActive(PR_FALSE)
{
}

nsJSChannel::~nsJSChannel()
{
    NS_IF_RELEASE(mIOThunk);
}


nsresult nsJSChannel::Init(nsIURI *aURI)
{
    nsresult rv;
    
    // Create the nsIStreamIO layer used by the nsIStreamIOChannel.
    mIOThunk= new nsJSThunk();
    if (mIOThunk == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mIOThunk);

    // Create a stock input stream channel...
    // Remember, until AsyncOpen is called, the script will not be evaluated
    // and the underlying Input Stream will not be created...
    nsCOMPtr<nsIChannel> channel;

    // If the resultant script evaluation actually does return a value, we
    // treat it as html.
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel), aURI, mIOThunk,
                                  NS_LITERAL_CSTRING("text/html"),
                                  NS_LITERAL_CSTRING(""));
    if (NS_FAILED(rv)) return rv;

    rv = mIOThunk->Init(aURI);
    if (NS_SUCCEEDED(rv)) {
        mStreamChannel = channel;
    }

    return rv;
}

//
// nsISupports implementation...
//

NS_IMPL_ADDREF(nsJSChannel)
NS_IMPL_RELEASE(nsJSChannel)

NS_INTERFACE_MAP_BEGIN(nsJSChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsIChannel)
NS_INTERFACE_MAP_END

//
// nsIRequest implementation...
//

NS_IMETHODIMP
nsJSChannel::GetName(nsACString &aResult)
{
    return mStreamChannel->GetName(aResult);
}

NS_IMETHODIMP
nsJSChannel::IsPending(PRBool *aResult)
{
    if (mIsActive) {
        *aResult = mIsActive;
        return NS_OK;
    }
    return mStreamChannel->IsPending(aResult);
}

NS_IMETHODIMP
nsJSChannel::GetStatus(nsresult *aResult)
{
    return mStreamChannel->GetStatus(aResult);
}

NS_IMETHODIMP
nsJSChannel::Cancel(nsresult aStatus)
{
    return mStreamChannel->Cancel(aStatus);
}

NS_IMETHODIMP
nsJSChannel::Suspend(void)
{
    return mStreamChannel->Suspend();
}

NS_IMETHODIMP
nsJSChannel::Resume(void)
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
    nsresult rv;

    // Synchronously execute the script...
    // mIsActive is used to indicate the the request is 'busy' during the
    // the script evaluation phase.  This means that IsPending() will 
    // indicate the the request is busy while the script is executing...
    mIsActive = PR_TRUE;
    rv = mIOThunk->EvaluateScript(mStreamChannel);

    if (NS_SUCCEEDED(rv)) {
        rv = mStreamChannel->Open(aResult);
    } else {
        // Propagate the failure down to the underlying channel...
        (void) mStreamChannel->Cancel(rv);
    }
    mIsActive = PR_FALSE;
    return rv;

}

NS_IMETHODIMP
nsJSChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
    nsresult rv;

    nsCOMPtr<nsILoadGroup> loadGroup;

    // Add the javascript channel to its loadgroup...
    mStreamChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
        (void) loadGroup->AddRequest(this, aContext);
    }

    // Synchronously execute the script...
    // mIsActive is used to indicate the the request is 'busy' during the
    // the script evaluation phase.  This means that IsPending() will 
    // indicate the the request is busy while the script is executing...
    mIsActive = PR_TRUE;
    rv = mIOThunk->EvaluateScript(mStreamChannel);

    if (NS_SUCCEEDED(rv)) {
        rv = mStreamChannel->AsyncOpen(aListener, aContext);
    } else {
        // Propagate the failure down to the underlying channel...
        (void) mStreamChannel->Cancel(rv);
    }

    // Remove the javascript channel from its loadgroup...
    if (loadGroup) {
        (void) loadGroup->RemoveRequest(this, aContext, rv);
    }
    mIsActive = PR_FALSE;
    return rv;
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
    mLoadFlags = aLoadFlags;
    //
    // Since the javascript channel is considered the 'document channel'
    // clear this bit before passing it down...  Otherwise, there will be
    // two document channels!!
    //
    return mStreamChannel->SetLoadFlags(aLoadFlags & ~(LOAD_DOCUMENT_URI));
}

NS_IMETHODIMP
nsJSChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    return mStreamChannel->GetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsJSChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
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
nsJSChannel::GetContentLength(PRInt32 *aContentLength)
{
    return mStreamChannel->GetContentLength(aContentLength);
}

NS_IMETHODIMP
nsJSChannel::SetContentLength(PRInt32 aContentLength)
{
    return mStreamChannel->SetContentLength(aContentLength);
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

NS_METHOD
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
    NS_EscapeURL(NS_ConvertUCS2toUTF8(uStr), esc_AlwaysCopy | esc_OnlyNonASCII, aUTF8Spec);

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
    *result = URI_NORELATIVE | URI_NOAUTH;
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewURI(const nsACString &aSpec,
                            const char *aCharset, // ignore charset info
                            nsIURI *aBaseURI,
                            nsIURI **result)
{
    nsresult rv;

    // javascript: URLs (currently) have no additional structure beyond that
    // provided by standard URLs, so there is no "outer" object given to
    // CreateInstance.

    nsIURI* url;
    rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            (void**)&url);

    if (NS_FAILED(rv))
        return rv;

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
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
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
nsJSProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////

static const nsModuleComponentInfo gJSModuleInfo[] = {
    { "JavaScript Protocol Handler",
      NS_JSPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "javascript",
      nsJSProtocolHandler::Create }
};

NS_IMPL_NSGETMODULE(javascript__protocol, gJSModuleInfo)

////////////////////////////////////////////////////////////////////////////////

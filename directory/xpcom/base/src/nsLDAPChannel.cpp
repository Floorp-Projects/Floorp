/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@mozilla.org>
 *   Warren Harris <warren@netscape.com>
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

#include "nsLDAPInternal.h"
#include "nsLDAPConnection.h"
#include "nsLDAPChannel.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsMimeTypes.h"
#include "nsIPipe.h"
#include "nsXPIDLString.h"
#include "nsILDAPURL.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "nsILDAPMessage.h"

#if !INVOKE_LDAP_CALLBACKS_ON_MAIN_THREAD
#include "nsNetUtil.h"
#include "nsIEventQueueService.h"
#endif

static NS_DEFINE_IID(kILDAPMessageListenerIID, NS_ILDAPMESSAGELISTENER_IID);
static NS_DEFINE_IID(kILoadGroupIID, NS_ILOADGROUP_IID);
static NS_DEFINE_IID(kIProgressEventSink, NS_IPROGRESSEVENTSINK_IID);

NS_IMPL_THREADSAFE_ISUPPORTS3(nsLDAPChannel, 
                              nsIChannel, 
                              nsIRequest,   
                              nsILDAPMessageListener);

nsLDAPChannel::nsLDAPChannel()
{
}

nsLDAPChannel::~nsLDAPChannel()
{
}

// initialize the channel
//
nsresult
nsLDAPChannel::Init(nsIURI *uri)
{
    nsresult rv;

    mStatus = NS_OK;
    mURI = uri;
    mLoadFlags = LOAD_NORMAL;
    mReadPipeOffset = 0;
    mReadPipeClosed = PR_FALSE;

    // create an LDAP connection
    //
    mConnection = do_CreateInstance("@mozilla.org/network/ldap-connection;1", 
                                    &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::Init(): could not create "
                 "@mozilla.org/network/ldap-connection;1");
        return NS_ERROR_FAILURE;
    }

    // i think that in the general case, it will be worthwhile to leave the
    // callbacks for this channel be invoked on the LDAP connection thread.
    // however, for the moment, I want to leave the code that invokes it on
    // the main thread here (though turned off), because it provides a 
    // useful demonstration of why PLEvents' lack of priorities hurts us 
    // (in this case in ldap: searches that return a lot of results).
    //
#if INVOKE_LDAP_CALLBACKS_ON_MAIN_THREAD

    // get the proxy object manager
    //
    nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = 
        do_GetService("@mozilla.org/xpcomproxy;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::Init(): could not create "
                 "proxy object manager");
        return NS_ERROR_FAILURE;
    }

    // and use it to get a proxy for this callback, saving it off in mCallback
    //
    rv = proxyObjMgr->GetProxyForObject(NS_UI_THREAD_EVENTQ,
                                        kILDAPMessageListenerIID,
                                        NS_STATIC_CAST(nsILDAPMessageListener *
                                                       , this),
                                        PROXY_ASYNC|PROXY_ALWAYS,
                                        getter_AddRefs(mCallback));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::Init(): could not create proxy object");
        return NS_ERROR_FAILURE;
    }

#else   
    mCallback = this;
#endif
    
    return NS_OK;
}

// impl code cribbed from nsJARChannel.cpp
//
NS_METHOD
nsLDAPChannel::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsLDAPChannel* ldapChannel = new nsLDAPChannel();
    if (!ldapChannel)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(ldapChannel);
    rv = ldapChannel->QueryInterface(aIID, aResult);
    NS_RELEASE(ldapChannel);
    return rv;
}

// nsIRequest methods

NS_IMETHODIMP
nsLDAPChannel::GetName(nsACString &result)
{
    if (mURI)
        return mURI->GetSpec(result);
    result.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::IsPending(PRBool *result)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::GetStatus(nsresult *status)
{
    return mStatus;
}

NS_IMETHODIMP
nsLDAPChannel::Cancel(nsresult aStatus)
{
    nsresult rv;

    // set the status
    //
    mStatus = aStatus;

    // if there is an operation running, abandon it and remove it from the 
    // queue
    //
    if (mCurrentOperation) {

        // if this fails in a non-debug build, there's not much we can do
        //
        rv = mCurrentOperation->Abandon();
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsLDAPChannel::Cancel(): "
                     "mCurrentOperation->Abandon() failed\n");
        
        // make nsCOMPtr call Release()
        // 
        mCurrentOperation = 0;

    }

    // if the read pipe exists and hasn't already been closed, close it
    //
    if (mReadPipeOut != 0 && !mReadPipeClosed) {
        
        // XXX set mReadPipeClosed?

        // if this fails in a non-debug build, there's not much we can do
        //
        rv = mReadPipeOut->Close();
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsLDAPChannel::Cancel(): "
                     "mReadPipeOut->Close() failed");
    }

    // remove self from loadgroup to stop the throbber
    //
    if (mLoadGroup) {
        rv = mLoadGroup->RemoveRequest(NS_STATIC_CAST(nsIRequest *, this), 
                                       mResponseContext, aStatus);
        if (NS_FAILED(rv)) 
            return rv;
    }

    // call listener's onstoprequest
    //
    if (mUnproxiedListener) {
        rv = mListener->OnStopRequest(this, mResponseContext, aStatus);
        if (NS_FAILED(rv)) 
            return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::Suspend(void)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::Resume(void)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIChannel methods
//

NS_IMETHODIMP
nsLDAPChannel::SetOriginalURI(nsIURI *aOriginalURI)
{
    mOriginalURI = aOriginalURI;
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::GetOriginalURI(nsIURI **aOriginalURI)
{
    *aOriginalURI = mOriginalURI ? mOriginalURI : mURI;
    NS_IF_ADDREF(*aOriginalURI);

    return NS_OK;
}

// getter for URI attribute:
//
// Returns the URL to which the channel currently refers. If a redirect
// or URI resolution occurs, this accessor returns the current location
// to which the channel is referring.
//
NS_IMETHODIMP
nsLDAPChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

// getter and setter for loadFlags attribute:
//
// The load attributes for the request. E.g. setting the load 
// attributes with the LOAD_QUIET bit set causes the loading process to
// not deliver status notifications to the program performing the load,
// and to not contribute to keeping any nsILoadGroup it may be contained
// in from firing its OnLoadComplete notification.
//
NS_IMETHODIMP
nsLDAPChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

// getter and setter for contentType attribute:
//
// The content MIME type of the channel if available. Note that the 
// content type can often be wrongly specified (wrong file extension, wrong
// MIME type, wrong document type stored on a server, etc.) and the caller
// most likely wants to verify with the actual data. 
//
NS_IMETHODIMP
nsLDAPChannel::GetContentType(nsACString &aContentType)
{
    aContentType.AssignLiteral(TEXT_PLAIN);
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::SetContentType(const nsACString &aContentType)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetContentType");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::SetContentCharset(const nsACString &aContentCharset)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetContentCharset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

// getter and setter for contentLength attribute:
//
// Returns the length of the data associated with the channel if available.
// If the length is unknown then -1 is returned.

NS_IMETHODIMP
nsLDAPChannel::GetContentLength(PRInt32 *)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::SetContentLength(PRInt32)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

// getter and setter for the owner attribute:
//
// The owner corresponding to the entity that is responsible for this
// channel. Used by security code to grant or deny privileges to
// mobile code loaded from this channel.
//
// Note: This is a strong reference to the owner, so if the owner is also
// holding a pointer to the channel, care must be taken to explicitly drop
// its reference to the channel -- otherwise a leak will result.
//
NS_IMETHODIMP
nsLDAPChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner;
    NS_IF_ADDREF(*aOwner);

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::SetOwner(nsISupports *aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

// getter and setter for the loadGroup attribute:
//
// the load group of which the channel is a currently a member.
//
NS_IMETHODIMP
nsLDAPChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mUnproxiedLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
  
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mUnproxiedLoadGroup = aLoadGroup;

    // in the case where the LDAP callbacks happen on the connection thread,
    // we'll need to call into the loadgroup from there
    //
#if INVOKE_LDAP_CALLBACKS_ON_MAIN_THREAD

    mLoadGroup = mUnproxiedLoadGroup;

#else
    nsresult rv;

    // get the proxy object manager
    //
    nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = 
        do_GetService("@mozilla.org/xpcomproxy;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::SetLoadGroup(): could not create "
                 "proxy object manager");
        return NS_ERROR_FAILURE;
    }

    // and use it to get and save a proxy for the load group
    //
    rv = proxyObjMgr->GetProxyForObject(NS_UI_THREAD_EVENTQ, kILoadGroupIID,
                                        mUnproxiedLoadGroup, 
                                        PROXY_SYNC|PROXY_ALWAYS,
                                        getter_AddRefs(mLoadGroup));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::SetLoadGroup(): could not create proxy "
                 "event");
        return NS_ERROR_FAILURE;
    }
#endif

    return NS_OK;
}

// getter and setter for the notificationCallbacks
//
// The capabilities callbacks of the channel. This is set by clients
// who wish to provide a means to receive progress, status and 
// protocol-specific notifications.
//
NS_IMETHODIMP
nsLDAPChannel::GetNotificationCallbacks(nsIInterfaceRequestor* 
                                        *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks;
    NS_IF_ADDREF(*aNotificationCallbacks);

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::SetNotificationCallbacks(nsIInterfaceRequestor* 
                                        aNotificationCallbacks)
{
    nsresult rv;

    // save off the callbacks
    //
    mCallbacks = aNotificationCallbacks;
    if (mCallbacks) {

        // get the (unproxied) event sink interface 
        //
        nsCOMPtr<nsIProgressEventSink> eventSink;
        rv = mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink), 
                                      getter_AddRefs(eventSink));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPChannel::SetNotificationCallbacks(): "
                     "mCallbacks->GetInterface failed");
            return NS_ERROR_FAILURE;
        }

#if INVOKE_LDAP_CALLBACKS_ON_MAIN_THREAD
        mEventSink = eventSink;
#else
        // get the proxy object manager
        //
        nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = 
            do_GetService("@mozilla.org/xpcomproxy;1", &rv);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPChannel::SetNotificationCallbacks(): could not "
                     "create proxy object manager");
            return NS_ERROR_FAILURE;
        }

        // and use it to get a proxy for this callback, saving it off 
        // in mEventSink
        //
        rv = proxyObjMgr->GetProxyForObject(NS_UI_THREAD_EVENTQ,
                                            kIProgressEventSink,
                                            eventSink,
                                            PROXY_ASYNC | PROXY_ALWAYS,
                                            getter_AddRefs(mEventSink));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPChannel::SetNotificationCallbacks(): "
                     "couldn't get proxy for event sink");
            return NS_ERROR_FAILURE;
        }
#endif
    }

    return NS_OK;
}

// getter for securityInfo attribute:
//
// Any security information about this channel.  This can be null.
//
NS_IMETHODIMP
nsLDAPChannel::GetSecurityInfo(nsISupports* *aSecurityInfo)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetSecurityInfo");
    return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIChannel operations

// Opens a blocking input stream to the URL's specified source.
// @param startPosition - The offset from the start of the data
//  from which to read.
// @param readCount - The number of bytes to read. If -1, everything
//  up to the end of the data is read. If greater than the end of 
//  the data, the amount available is returned in the stream.
//
NS_IMETHODIMP
nsLDAPChannel::Open(nsIInputStream* *result)
{
    NS_NOTYETIMPLEMENTED("nsLDAPChannel::OpenInputStream");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::AsyncOpen(nsIStreamListener* aListener,
                         nsISupports* aCtxt)
{
    nsresult rv;
    nsCAutoString host;
    PRInt32 port;
    PRUint32 options;

    // slurp out relevant pieces of the URL
    //
    rv = mURI->GetAsciiHost(host);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::AsyncRead(): mURI->GetAsciiHost failed\n");
        return NS_ERROR_FAILURE;
    }

    rv = mURI->GetPort(&port);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::AsyncRead(): mURI->GetPort failed\n");
        return NS_ERROR_FAILURE;
    }
    if (port == -1)
        port = LDAP_PORT;

    // QI to nsILDAPURL so that we can call one of the methods on that iface
    //
    nsCOMPtr<nsILDAPURL> mLDAPURL = do_QueryInterface(mURI, &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::AsyncRead(): QI to nsILDAPURL failed\n");
        return NS_ERROR_FAILURE;
    }

    rv = mLDAPURL->GetOptions(&options);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::AsyncRead(): mURI->GetOptions failed\n");
        return NS_ERROR_FAILURE;
    }

    rv = NS_CheckPortSafety(port, "ldap");
    if (NS_FAILED(rv))
        return rv;

    // save off the args
    //
    mResponseContext = aCtxt;
    mUnproxiedListener = aListener;

    // add ourselves to the appropriate loadgroup
    //
    if (mLoadGroup) {
        mLoadGroup->AddRequest(this, mResponseContext);
    }

    // we don't currently allow for a default host
    //
    if (host.IsEmpty())
        return NS_ERROR_MALFORMED_URI;

    // since the LDAP SDK does all the socket management, we don't have
    // an underlying transport channel to create an nsIInputStream to hand
    // back to the nsIStreamListener.  So we do it ourselves:
    //
    if (!mReadPipeIn) {
    
        // get a new pipe, propagating any error upwards
        //
        rv = NS_NewPipe(getter_AddRefs(mReadPipeIn), 
                        getter_AddRefs(mReadPipeOut), 
                        0, 0, PR_TRUE, PR_FALSE, 0);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPChannel::AsyncRead(): unable to create new pipe");
            return NS_ERROR_FAILURE;
        }
    } 

    // get an AsyncStreamListener to proxy for mListener, if we're
    // compiled to have the LDAP callbacks happen on the LDAP connection=
    // thread.
    //
#if INVOKE_LDAP_CALLBACKS_ON_MAIN_THREAD
    mListener = aListener;
#else
    rv = NS_NewAsyncStreamListener(getter_AddRefs(mListener), 
                                   mUnproxiedListener, NS_UI_THREAD_EVENTQ);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::AsyncRead(): unable to create new "
                 "AsyncStreamListener");
        return NS_ERROR_FAILURE;
    }

#endif

    // we already know the content type, so we can fire this now
    //
    mUnproxiedListener->OnStartRequest(this, mResponseContext);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::AsyncRead(): error firing OnStartRequest");
        return NS_ERROR_FAILURE;
    }
    
    // initialize it with the defaults
    // XXXdmose - need to deal with bind name
    // Need to deal with VERSION2 pref - don't know how to get it from here.
    rv = mConnection->Init(host.get(), port,
                           (options & nsILDAPURL::OPT_SECURE) ? PR_TRUE 
                           : PR_FALSE, EmptyCString(), this, nsnull, nsILDAPConnection::VERSION3);
    switch (rv) {
    case NS_OK:
        break;

    case NS_ERROR_OUT_OF_MEMORY:
    case NS_ERROR_NOT_AVAILABLE:
    case NS_ERROR_FAILURE:
        return rv;

    case NS_ERROR_ILLEGAL_VALUE:
    default:
        return NS_ERROR_UNEXPECTED;
    }

    // create and initialize an LDAP operation (to be used for the bind)
    //  
    mCurrentOperation = do_CreateInstance(
        "@mozilla.org/network/ldap-operation;1", &rv);
    if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
    }

    // our OnLDAPMessage accepts all result callbacks
    //
    rv = mCurrentOperation->Init(mConnection, mCallback, nsnull);
    if (NS_FAILED(rv))
        return NS_ERROR_UNEXPECTED; // this should never happen

    // kick off a bind operation 
    // 
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("initiating SimpleBind\n"));
    rv = mCurrentOperation->SimpleBind(EmptyCString());
    if (NS_FAILED(rv)) {

        // XXXdmose better error handling / passthrough; deal with password
        //
        NS_WARNING("mCurrentOperation->SimpleBind failed.");
        return(rv);
    }

    return NS_OK;
}

/**
 * Messages received are passed back via this function.
 *
 * @arg aMessage  The message that was returned, 0 if none was.
 *
 * void OnLDAPMessage (in nsILDAPMessage aMessage)
 */
NS_IMETHODIMP 
nsLDAPChannel::OnLDAPMessage(nsILDAPMessage *aMessage)
{
    PRInt32 messageType;

    // figure out what sort of message was returned
    //
    nsresult rv = aMessage->GetType(&messageType);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::OnLDAPMessage(): unexpected error in "
                 "nsLDAPChannel::GetType()");
        return NS_ERROR_UNEXPECTED;
    }

    switch (messageType) {

    case LDAP_RES_BIND:

        // a bind has completed
        //
        return OnLDAPBind(aMessage);
        break;

    case LDAP_RES_SEARCH_ENTRY:

        // a search entry has been returned
        //
        return OnLDAPSearchEntry(aMessage);
        break;

    case LDAP_RES_SEARCH_RESULT:

        // the search is finished; we're all done
        //  
        return OnLDAPSearchResult(aMessage);
        break;

    default:
        NS_WARNING("nsLDAPChannel::OnLDAPMessage(): unexpected LDAP message "
                   "received");

        // get the console service so we can log a message
        //
        nsCOMPtr<nsIConsoleService> consoleSvc = 
            do_GetService("@mozilla.org/consoleservice;1", &rv);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPChannel::OnLDAPMessage() couldn't get console "
                     "service");
            break;
        }

        // log the message
        //
        rv = consoleSvc->LogStringMessage(
            NS_LITERAL_STRING("LDAP: WARNING: nsLDAPChannel::OnLDAPMessage(): Unexpected LDAP message received").get());
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsLDAPChannel::OnLDAPMessage(): "
                     "consoleSvc->LogStringMessage() failed");
        break;
    }

    return NS_OK;
}

nsresult
nsLDAPChannel::OnLDAPBind(nsILDAPMessage *aMessage) 
{
    nsCOMPtr<nsILDAPURL> url;
    nsCAutoString baseDn;
    nsCAutoString filter;
    PRInt32 scope;
    nsresult rv;

    // XXX should call ldap_parse_result() here

    mCurrentOperation = 0;  // done with bind op; make nsCOMPtr release it

    // create and initialize an LDAP operation (to be used for the search
    //  
    mCurrentOperation = do_CreateInstance(
        "@mozilla.org/network/ldap-operation;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPChannel::OnLDAPBind(): couldn't create "
                 "@mozilla.org/network/ldap-operation;1");
        // XXX abort entire asyncread
        return NS_ERROR_FAILURE;
    }

    rv = mCurrentOperation->Init(mConnection, mCallback, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    // QI() the URI to an nsILDAPURL so we get can the LDAP specific portions
    //
    url = do_QueryInterface(mURI, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // get a base DN.  
    // XXXdmose - is it reasonable to barf on an empty dn?
    //
    rv = url->GetDn(baseDn);
    NS_ENSURE_SUCCESS(rv, rv);
    if (baseDn.IsEmpty()) {
        return NS_ERROR_MALFORMED_URI;
    }

    // get the scope
    //
    rv = url->GetScope(&scope);
    NS_ENSURE_SUCCESS(rv, rv);

    // and the filter 
    //
    rv = url->GetFilter(filter);
    NS_ENSURE_SUCCESS(rv, rv);

    // time to kick off the search.
    //
    // XXX what about timeouts? 
    // XXX failure is a reasonable thing; don't assert
    //
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
           ("bind completed; starting search\n"));
    rv = mCurrentOperation->SearchExt(baseDn, scope, filter, 0, 0, 0,
                                      LDAP_NO_LIMIT);
    NS_ENSURE_SUCCESS(rv,rv);
    
    return NS_OK;
}

// void onLDAPInit (in nsresult aStatus); */
//
NS_IMETHODIMP
nsLDAPChannel::OnLDAPInit(nsILDAPConnection *aConnection, nsresult aStatus)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// void OnLDAPSearchResult (in nsILDAPMessage aMessage);
//
nsresult
nsLDAPChannel::OnLDAPSearchResult(nsILDAPMessage *aMessage)
{
    PRInt32 errorCode;  // the LDAP error code
    nsresult rv;

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("result returned\n"));

    // XXX should use GetErrorString here?
    //
    rv = aMessage->GetErrorCode(&errorCode);
    if ( NS_FAILED(rv) ) {
        NS_ERROR(ldap_err2string(errorCode));
        return NS_ERROR_FAILURE;
    }

    // we're done with the current operation.  cause nsCOMPtr to Release() it
    // so that if nsLDAPChannel::Cancel gets called, that doesn't try to call 
    // mCurrentOperation->Abandon().
    //
    mCurrentOperation = 0;

    // if the read pipe exists and hasn't already been closed, close it
    //
    if (mReadPipeOut != 0 && !mReadPipeClosed) {

        // if this fails in a non-debug build, there's not much we can do
        //
        rv = mReadPipeOut->Close();
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsLDAPChannel::OnLDAPSearchResult(): "
                     "mReadPipeOut->Close() failed");
    }

    // remove self from loadgroup to stop the throbber
    //
    if (mLoadGroup) {
        rv = mLoadGroup->RemoveRequest(NS_STATIC_CAST(nsIRequest *, this), 
                                       mResponseContext, NS_OK);
        if (NS_FAILED(rv)) {
            NS_WARNING("nsLDAPChannel::OnSearchResult(): "
                       "mLoadGroup->RemoveChannel() failed");
            return rv;
        }
    }

    // call listener's onstoprequest
    //
    if (mListener) {
        rv = mListener->OnStopRequest(this, mResponseContext, NS_OK);
        if (NS_FAILED(rv)) {
            NS_WARNING("nsLDAPChannel::OnSearchResult(): "
                       "mListener->OnStopRequest failed\n");
            return rv;
        }
    }

    return NS_OK;
}

// void OnLDAPSearchEntry (in nsILDAPMessage aMessage);
//
// XXXdmose most of this function should live in nsILDAPMessage::toString()
//
nsresult
nsLDAPChannel::OnLDAPSearchEntry(nsILDAPMessage *aMessage)
{
    nsresult rv;
    nsCAutoString dn;
    nsCString entry;

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("entry returned!\n"));

    // get the DN
    // XXX better err handling
    //
    rv = aMessage->GetDn(dn);
    NS_ENSURE_SUCCESS(rv, rv);

    entry.SetCapacity(256);
    entry = NS_LITERAL_CSTRING("dn: ") + dn + NS_LITERAL_CSTRING("\n");

    char **attrs;
    PRUint32 attrCount;

    // get an array of all the attribute names
    // XXX better error-handling
    //
    rv = aMessage->GetAttributes(&attrCount, &attrs);
    NS_ENSURE_SUCCESS(rv, rv);

    // XXX is this an error?  should log in non-debug console too?
    //
    if (!attrCount) {
        NS_WARNING("Warning: entry received with no attributes");
    }

    // iterate through the attributes
    //
    for ( PRUint32 i=0 ; i < attrCount ; i++ ) {

        PRUnichar **vals;
        PRUint32 valueCount;

        // get the values of this attribute
        // XXX better failure handling
        //
        rv = aMessage->GetValues(attrs[i], &valueCount, &vals);
        if (NS_FAILED(rv)) {
            NS_WARNING("nsLDAPChannel:OnLDAPSearchEntry(): "
                       "aMessage->GetValues() failed\n");
            NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(attrCount, attrs);
            return rv;;
        }

        // print all values of this attribute
        //
        for ( PRUint32 j=0 ; j < valueCount; j++ ) {
            entry.Append(attrs[i]);
            entry.Append(": ");
            AppendUTF16toUTF8(vals[j], entry);
            entry.Append('\n');
        }
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(valueCount, vals);

    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(attrCount, attrs);

    // XXXdmose better error handling
    //
    if (NS_FAILED(rv)) {
        PR_LOG(gLDAPLogModule, PR_LOG_ERROR, 
               ("aMessage: error getting attribute\n"));
        return rv;
    }

    // separate this entry from the next
    //
    entry.Append('\n');

    // do the write
    // XXX better err handling
    //
    PRUint32 bytesWritten = 0;
    PRUint32 entryLength = entry.Length();

    rv = mReadPipeOut->Write(entry.get(), entryLength, &bytesWritten);
    NS_ENSURE_SUCCESS(rv, rv);

    // short writes shouldn't happen on blocking pipes!
    // XXX runtime error handling too
    //
    NS_ASSERTION(bytesWritten == entryLength, 
                 "nsLDAPChannel::OnLDAPSearchEntry(): "
                 "internal error: blocking pipe returned a short write");

    // XXXdmose deal more gracefully with an error here
    //
    rv = mListener->OnDataAvailable(this, mResponseContext, mReadPipeIn, 
                                    mReadPipeOffset, entryLength);
    NS_ENSURE_SUCCESS(rv, rv);

    mReadPipeOffset += entryLength;

    return NS_OK;
}

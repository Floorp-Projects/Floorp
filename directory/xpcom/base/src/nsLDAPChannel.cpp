/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 *		   Warren Harris <warren@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsLDAP.h"
#include "nsLDAPConnection.h"
#include "nsLDAPChannel.h"
#include "nsString.h"
#include "nsMimeTypes.h"
#include "nsIPipe.h"
#include "nsXPIDLString.h"
#include "nsILDAPURL.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"

#if !INVOKE_LDAP_CALLBACKS_ON_MAIN_THREAD
#include "nsNetUtil.h"
#include "nsIEventQueueService.h"
#endif

static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_IID(kILDAPMessageListenerIID, NS_ILDAPMESSAGELISTENER_IID);
static NS_DEFINE_IID(kILoadGroupIID, NS_ILOADGROUP_IID);

NS_IMPL_THREADSAFE_ISUPPORTS3(nsLDAPChannel, nsIChannel, nsIRequest,	
			      nsILDAPMessageListener);

nsLDAPChannel::nsLDAPChannel()
{
  NS_INIT_ISUPPORTS();
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

    mURI = uri;
    mStatus = NS_OK;
    mReadPipeOffset = 0;
    mReadPipeClosed = PR_FALSE;
    mLoadAttributes = LOAD_NORMAL;

    // create an LDAP connection
    //
    mConnection = do_CreateInstance("mozilla.network.ldapconnection", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

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
	do_GetService(kProxyObjectManagerCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv); 

    // and use it to get a proxy for this callback, saving it off in mCallback
    //
    rv = proxyObjMgr->GetProxyForObject(NS_UI_THREAD_EVENTQ,
					kILDAPMessageListenerIID,
					NS_STATIC_CAST(nsILDAPMessageListener *
						       , this),
					PROXY_ASYNC|PROXY_ALWAYS,
					getter_AddRefs(mCallback));
    NS_ENSURE_SUCCESS(rv, rv);
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
  if (ldapChannel == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(ldapChannel);
  rv = ldapChannel->QueryInterface(aIID, aResult);
  NS_RELEASE(ldapChannel);
  return rv;
}

// nsIRequest methods

NS_IMETHODIMP
nsLDAPChannel::GetName(PRUnichar* *result)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetName");
  return NS_ERROR_NOT_IMPLEMENTED;
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
    //
    if (mCurrentOperation) {

	// if this fails in a non-debug build, there's not much we can do
	//
	rv = mCurrentOperation->Abandon();
	NS_ASSERTION(NS_SUCCEEDED(rv), "nsLDAPChannel::Cancel(): "
		     "mCurrentOperation->Abandon() failed\n");
    }

    // if the read pipe exists and hasn't already been closed, close it
    //
    if (mReadPipeOut != 0 && !mReadPipeClosed) {

	// if this fails in a non-debug build, there's not much we can do
	//
	rv = mReadPipeOut->Close();
	NS_ASSERTION(NS_SUCCEEDED(rv), "nsLDAPChannel::Cancel(): "
		     "mReadPipeOut->Close() failed");
    }

    // remove self from loadgroup to stop the throbber
    //
    if (mLoadGroup) {
        rv = mLoadGroup->RemoveChannel(this, mResponseContext, aStatus,
				       nsnull);
        if (NS_FAILED(rv)) 
	    return rv;
    }

    // call listener's onstoprequest
    //
    if (mUnproxiedListener) {
        rv = mListener->OnStopRequest(this, mResponseContext, aStatus, nsnull);
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

// getter and setter for URI attribute:
//
// Returns the URL to which the channel currently refers. If a redirect
// or URI resolution occurs, this accessor returns the current location
// to which the channel is referring.
//
NS_IMETHODIMP
nsLDAPChannel::SetURI(nsIURI* aURI)
{
  mURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

// getter and setter for transferOffset attribute:
//
// The start offset from the beginning of the data from/to which
// reads/writes will occur. Users may set the transferOffset before making
// any of the following requests: asyncOpen, asyncRead, asyncWrite,
// openInputStream, openOutputstream.
//
NS_IMETHODIMP
nsLDAPChannel::SetTransferOffset(PRUint32 newOffset)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetTransferOffset");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::GetTransferOffset(PRUint32 *offset)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetTransferOffset");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// getter and setter for transferCount attribute
//
// Accesses the count of bytes to be transfered. For openInputStream and
// asyncRead, this specifies the amount to read, for asyncWrite, this
// specifies the amount to write (note that for openOutputStream, the
// end of the data can be signified simply by closing the stream). 
// If the transferCount is set after reading has been initiated, the
// amount specified will become the current remaining amount to read
// before the channel is closed (this can be useful if the content
// length is encoded at the start of the stream).
//
// A transferCount value of -1 means the amount is unspecified, i.e. 
// read or write all the data that is available.
//
NS_IMETHODIMP
nsLDAPChannel::SetTransferCount(PRInt32 newCount)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetTransferCount");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::GetTransferCount(PRInt32 *count)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetTransferCount");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// getter and setter for loadAttributes attribute:
//
// The load attributes for the channel. E.g. setting the load 
// attributes with the LOAD_QUIET bit set causes the loading process to
// not deliver status notifications to the program performing the load,
// and to not contribute to keeping any nsILoadGroup it may be contained
// in from firing its OnLoadComplete notification.
//
NS_IMETHODIMP
nsLDAPChannel::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
  *aLoadAttributes = mLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
  mLoadAttributes = aLoadAttributes;
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
nsLDAPChannel::GetContentType(char* *aContentType)
{
  NS_ENSURE_ARG_POINTER(aContentType);

  *aContentType = nsCRT::strdup(TEXT_PLAIN);
  if (!*aContentType) {
    return NS_ERROR_OUT_OF_MEMORY;
  } else {
    return NS_OK;
  }
}

NS_IMETHODIMP
nsLDAPChannel::SetContentType(const char *contenttype)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetContentType");
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
	do_GetService(kProxyObjectManagerCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv); 

    // and use it to get and save a proxy for the load group
    //
    rv = proxyObjMgr->GetProxyForObject(NS_UI_THREAD_EVENTQ, kILoadGroupIID,
					mUnproxiedLoadGroup, 
					PROXY_SYNC|PROXY_ALWAYS,
					getter_AddRefs(mLoadGroup));
    NS_ENSURE_SUCCESS(rv, rv);

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
  mCallbacks = aNotificationCallbacks;

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

// getter and setter for bufferSegmentSize attribute
//
// The buffer segment size is used as the initial size for any
// transfer buffers, and the increment size for whenever the buffer
// space needs to be grown.  (Note this parameter is passed along to
// any underlying nsIPipe objects.)  If unspecified, the channel
// implementation picks a default.
//
// attribute unsigned long bufferSegmentSize;
//
NS_IMETHODIMP
nsLDAPChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetBufferSegmentSize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetBufferSegmentSize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// getter and setter for the bufferMaxSize attribute
//
// Accesses the buffer maximum size. The buffer maximum size is the limit
// size that buffer will be grown to before suspending the channel.
// (Note this parameter is passed along to any underlying nsIPipe objects.)
// If unspecified, the channel implementation picks a default.
//
// attribute unsigned long bufferMaxSize;
//
NS_IMETHODIMP
nsLDAPChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetBufferMaxSize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetBufferMaxSize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Returns a local file to the channel's data if one exists, null otherwise.
//
// readonly attribute nsIFile localFile;
NS_IMETHODIMP
nsLDAPChannel::GetLocalFile(nsIFile* *aFile)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// getter and setter for pipeliningAllowed attribute
//
// Setting pipeliningAllowed causes the load of a URL (issued via asyncOpen,
// asyncRead or asyncWrite) to be deferred in order to allow the request to
// be pipelined for greater throughput efficiency. Pipelined requests will
// be forced to load when the first non-pipelined request is issued.
//
// attribute boolean pipeliningAllowed;
//
NS_IMETHODIMP
nsLDAPChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetPipeLiningAllowed");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::SetPipeliningAllowed");
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
nsLDAPChannel::OpenInputStream(nsIInputStream* *result)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::OpenInputStream");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Opens a blocking output stream to the URL's specified destination.
// @param startPosition - The offset from the start of the data
//   from which to begin writing.
//
NS_IMETHODIMP
nsLDAPChannel::OpenOutputStream(nsIOutputStream* *result)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::OpenOutputStream");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Reads asynchronously from the URL's specified source. Notifications
// are provided to the stream listener on the thread of the specified
// event queue.
// The startPosition argument designates the offset in the source where
// the data will be read.
// If the readCount == -1 then all the available data is delivered to
// the stream listener.
//
// void asyncRead(in nsIStreamListener listener,
//                in nsISupports ctxt);
NS_IMETHODIMP
nsLDAPChannel::AsyncRead(nsIStreamListener* aListener,
			 nsISupports* aCtxt)
{
    nsresult rv;
    nsXPIDLCString host;
    PRInt32 port;

    // save off the args
    //
    mResponseContext = aCtxt;
    mUnproxiedListener = aListener;

    // add ourselves to the appropriate loadgroup
    //
    if (mLoadGroup) {
	mLoadGroup->AddChannel(this, mResponseContext);
    }

    // slurp out relevant pieces of the URL
    //
    rv = mURI->GetHost(getter_Copies(host));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mURI->GetPort(&port);
    NS_ENSURE_SUCCESS(rv, rv);
    if (port == -1)
	port = LDAP_PORT;

    // we don't currently allow for a default host
    //
    if (nsCRT::strlen(host) == 0)
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
			NS_PIPE_DEFAULT_SEGMENT_SIZE, 
			NS_PIPE_DEFAULT_BUFFER_SIZE,
			PR_TRUE, PR_FALSE, nsnull);
	NS_ENSURE_SUCCESS(rv, rv);
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
    NS_ENSURE_SUCCESS(rv, rv);
#endif

    // we already know the content type, so we can fire this now
    //
    mUnproxiedListener->OnStartRequest(this, mResponseContext);

    // initialize it with the defaults
    // XXXdmose - need to deal with bind name
    //
    rv = mConnection->Init(host, port, NULL);
    NS_ENSURE_SUCCESS(rv, rv);

    // create and initialize an LDAP operation (to be used for the bind)
    //	
    mCurrentOperation = do_CreateInstance("mozilla.network.ldapoperation", 
					  &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // our OnLDAPMessage accepts all result callbacks
    //
    rv = mCurrentOperation->Init(mConnection, mCallback);
    NS_ENSURE_SUCCESS(rv, rv);

    // kick off a bind operation 
    // 
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("initiating SimpleBind\n"));
    rv = mCurrentOperation->SimpleBind(NULL);
    if (NS_FAILED(rv)) {

	// XXXdmose better error handling / passthrough; deal with password
	//
#ifdef DEBUG
	PR_fprintf(PR_STDERR, "mCurrentOperation->SimpleBind failed. rv=%d\n",
		   rv);
#endif
	return(rv);
    }

    return NS_OK;
}

// Writes asynchronously to the URL's specified destination. Notifications
// are provided to the stream observer on the thread of the specified
// event queue.
// The startPosition argument designates the offset in the destination where
// the data will be written.
// If the writeCount == -1, then all the available data in the input
// stream is written.
//
// void asyncWrite(in nsIInputStream fromStream,
//                 in nsIStreamObserver observer,
//                 in nsISupports ctxt);
NS_IMETHODIMP
nsLDAPChannel::AsyncWrite(nsIInputStream* fromStream,
			  nsIStreamObserver* observer,
			  nsISupports* ctxt)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::AsyncWrite");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsLDAPChannel::pipeWrite(char *str)
{
  PRUint32 bytesWritten=0;
  nsresult rv = NS_OK;

  rv = mReadPipeOut->Write(str, strlen(str), &bytesWritten);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXXdmose deal more gracefully with an error here
  //
  rv = mListener->OnDataAvailable(this, mResponseContext, 
				  mReadPipeIn, mReadPipeOffset, 
				  nsCRT::strlen(str));
  NS_ENSURE_SUCCESS(rv, rv);

  mReadPipeOffset += bytesWritten;
  return NS_OK;
}

/**
 * Messages received are passed back via this function.
 *
 * @arg aMessage  The message that was returned, NULL if none was.
 * @arg aRetVal   the return value from ldap_result()
 *
 * void OnLDAPMessage (in nsILDAPMessage aMessage, in PRInt32 aRetVal
 */
NS_IMETHODIMP 
nsLDAPChannel::OnLDAPMessage(nsILDAPMessage *aMessage, PRInt32 aRetVal)
{
    switch (aRetVal) {

    case LDAP_RES_BIND:
	// a bind has completed
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
	// XXX bogus.  but for now..
	//
#ifdef DEBUG	
	PR_fprintf(PR_STDERR, "unexpected LDAP message received.\n");
#endif
	break;
    }

    return NS_OK;
}

nsresult
nsLDAPChannel::OnLDAPBind(nsILDAPMessage *aMessage) 
{
    nsCOMPtr<nsILDAPURL> url;
    nsXPIDLCString baseDn;
    nsXPIDLCString filter;
    PRInt32 scope;
    nsresult rv;

    // XXX should call ldap_parse_result() here

    mCurrentOperation = 0;	// done with bind operation

    // create and initialize an LDAP operation (to be used for the bind)
    //	
    mCurrentOperation = do_CreateInstance("mozilla.network.ldapoperation", 
					  &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mCurrentOperation->Init(mConnection, mCallback);
    NS_ENSURE_SUCCESS(rv, rv);

    // QI() the URI to an nsILDAPURL so we get can the LDAP specific portions
    //
    url = do_QueryInterface(mURI, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // get a base DN.  
    // XXXdmose - is it reasonable to barf on an empty dn?
    //
    rv = url->GetDn(getter_Copies(baseDn));
    NS_ENSURE_SUCCESS(rv, rv);
    if (nsCRT::strlen(baseDn) == 0) {
	return NS_ERROR_MALFORMED_URI;
    }

    // get the scope
    //
    rv = url->GetScope(&scope);
    NS_ENSURE_SUCCESS(rv, rv);

    // and the filter 
    //
    rv = url->GetFilter(getter_Copies(filter));
    NS_ENSURE_SUCCESS(rv, rv);

    // time to kick off the search.
    //
    // XXX what about timeouts? 
    // XXX failure is a reasonable thing; don't assert
    //
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
	   ("bind completed; starting search\n"));
    rv = mCurrentOperation->SearchExt(baseDn, scope, filter, 0, LDAP_NO_LIMIT);
    NS_ENSURE_SUCCESS(rv,rv);
    
    return NS_OK;
}

// void OnLDAPSearchResult (in nsILDAPMessage aMessage);
//
nsresult
nsLDAPChannel::OnLDAPSearchResult(nsILDAPMessage *aMessage)
{
    PRInt32 errorCode;	// the LDAP error code
    nsresult rv;

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("result returned\n"));

    // XXX should use GetErrorString here?
    //
    rv = aMessage->GetErrorCode(&errorCode);
    if ( NS_FAILED(rv) ) {
#ifdef DEBUG
	PR_fprintf(PR_STDERR, " %s\n", ldap_err2string(errorCode));
#endif
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
	NS_ASSERTION(NS_SUCCEEDED(rv), "nsLDAPChannel::Cancel(): "
		     "mReadPipeOut->Close() failed");
    }

    // remove self from loadgroup to stop the throbber
    //
    if (mLoadGroup) {
        rv = mLoadGroup->RemoveChannel(this, mResponseContext, NS_OK, nsnull);
        if (NS_FAILED(rv)) 
	    return rv;
    }

    // call listener's onstoprequest
    //
    if (mListener) {
        rv = mListener->OnStopRequest(this, mResponseContext, NS_OK, nsnull);
        if (NS_FAILED(rv)) 
	    return rv;
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
    char *dn, *attr;

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("entry returned!\n"));

    // get the DN
    // XXX better err handling
    //
    rv = aMessage->GetDn(&dn);
    NS_ENSURE_SUCCESS(rv, rv);

    this->pipeWrite("dn: ");
    this->pipeWrite(dn);
    this->pipeWrite("\n");

    ldap_memfree(dn);

    // fetch the attributes
    //
    for (rv = aMessage->FirstAttribute(&attr);
	 attr != NULL;
	 rv = aMessage->NextAttribute(&attr)) {

	if ( NS_FAILED(rv) ) {
#ifdef DEBUG
	    PR_fprintf(PR_STDERR, "failure getting attribute\n");
#endif		    
	    return rv;
	}

	int i;
	char **vals;
	PRUint32 attrCount;

	// get the values of this attribute
	// XXX better failure handling
	//
	rv = aMessage->GetValues(attr, &attrCount, &vals);
	if (NS_FAILED(rv)) {
#ifdef DEBUG
	    PR_fprintf(PR_STDERR, "aMessage->GetValues\n");
#endif
	    return rv;;
	}

	// print all values of this attribute
	for ( i=0 ; vals[i] != NULL; i++ ) {
	    this->pipeWrite(attr);
	    this->pipeWrite(": ");
	    this->pipeWrite(vals[i]);
	    this->pipeWrite("\n");
	}

	ldap_value_free(vals);
	ldap_memfree(attr);
    }

    // XXXdmose better error handling
    //
    if (NS_FAILED(rv)) {
#ifdef DEBUG
	PR_fprintf(PR_STDERR, "aMessage: error getting attribute\n");
#endif
	return rv;
    }

    // separate this entry from the next
    //
    this->pipeWrite("\n");

    return NS_OK;
}

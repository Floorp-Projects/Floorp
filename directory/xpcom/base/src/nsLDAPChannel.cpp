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

#include "nsLDAPChannel.h"
#include "nsString.h"
#include "nsMimeTypes.h"
#include "nsIPipe.h"
#include "nsXPIDLString.h"

// for NS_NewAsyncStreamListener
//
#include "nsNetUtil.h"

// for defintion of NS_UI_THREAD_EVENTQ for use with NS_NewAsyncStreamListener
//
#include "nsIEventQueueService.h"

#ifdef DEBUG
#include "nspr.h"
#endif

NS_METHOD lds(class nsLDAPChannel *chan, const char *);

NS_IMPL_THREADSAFE_ISUPPORTS3(nsLDAPChannel, nsIChannel, nsIRequest,	
			      nsIRunnable);

nsLDAPChannel::nsLDAPChannel()
{
  NS_INIT_ISUPPORTS();
}

nsLDAPChannel::~nsLDAPChannel()
{
}

nsresult
nsLDAPChannel::Init(nsIURI *uri)
{
  mURI = uri;
  mReadPipeOffset = 0;
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
nsLDAPChannel::IsPending(PRBool *result)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::IsPending");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLDAPChannel::GetStatus(nsresult *status)
{
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::Cancel(nsresult status)
{
  // should assert if called after OnStop fired?
  //
  //NS_NOTYETIMPLEMENTED("nsLDAPChannel::Cancel");
  //return NS_ERROR_NOT_IMPLEMENTED;
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
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  
  return NS_OK;
}

NS_IMETHODIMP
nsLDAPChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;

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

// getter for shouldCache attribute
//
// Returns true if the data from this channel should be cached. Local files
// report false because they exist on the local disk and need not be cached.
// Input stream channels, data protocol, datetime protocol and finger 
// protocol channels also should not be cached. Http and ftp on the other 
// hand should. Note that the value of this attribute doesn't reflect any
// http headers that may specify that this channel should not be cached.
//
// readonly attribute boolean shouldCache;
//
NS_IMETHODIMP
nsLDAPChannel::GetShouldCache(PRBool *aShouldCache)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::GetShouldCache");
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

// Opens the channel asynchronously. The nsIStreamObserver's OnStartRequest
// method is called back when the channel actually becomes open, providing 
// the content type. Its OnStopRequest method is called when the channel
// becomes closed.
//
// void asyncOpen(in nsIStreamObserver observer, 
//                in nsISupports ctxt);
NS_IMETHODIMP
nsLDAPChannel::AsyncOpen(nsIStreamObserver* observer,
			 nsISupports* ctxt)
{
  NS_NOTYETIMPLEMENTED("nsLDAPChannel::AsyncOpen");
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

  // deal with the input args
  //
  mListener = aListener;
  mResponseContext = aCtxt;

  // add ourselves to the appropriate loadgroup
  //
  if (mLoadGroup) {
    mLoadGroup->AddChannel(this, nsnull);
  }

  // kick off a thread to do the work
  //
  NS_ASSERTION(!mThread, "nsLDAPChannel thread already exists!");

  rv = NS_NewThread(getter_AddRefs(mThread), this, 0, PR_JOINABLE_THREAD);
  NS_ENSURE_SUCCESS(rv, rv);

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

// for nsIRunnable.  this is the actual LDAP server interaction code for 
// AsyncRead().  it gets executed on a worker thread so we don't block 
// the main UI thread.
//
NS_IMETHODIMP
nsLDAPChannel::Run(void)
{
  nsresult rv;
  nsXPIDLCString spec;

#ifdef DEBUG_dmose  
  PR_fprintf(PR_STDERR, "nsLDAPService::Run() entered!\n");
#endif

  // XXX how does this get destroyed?
  //
  rv = NS_NewAsyncStreamListener(getter_AddRefs(mAsyncListener), mListener, 
				 NS_UI_THREAD_EVENTQ);
  NS_ENSURE_SUCCESS(rv, rv);

  // we already know the content type, so might as well fire this now
  //
  mAsyncListener->OnStartRequest(this, mResponseContext);

  // since the LDAP SDK does all the socket management, we don't have
  // an underlying transport channel to create an nsIInputStream to hand
  // back to the nsIStreamListener.  So (only on the first call to AsyncRead)
  // we do it ourselves:
  //
  if (!mReadPipeIn) {
    
    // get a new pipe, propagating any error upwards
    //
    rv = NS_NewPipe(getter_AddRefs(mReadPipeIn), getter_AddRefs(mReadPipeOut));
    NS_ENSURE_SUCCESS(rv, rv);

    // the side of the pipe used on the main UI thread cannot block
    //
    NS_ENSURE_SUCCESS(mReadPipeIn->SetNonBlocking(PR_TRUE), 
		      NS_ERROR_UNEXPECTED);

    // but the side of the pipe used by the worker thread can block
    //
    NS_ENSURE_SUCCESS(mReadPipeOut->SetNonBlocking(PR_FALSE),
		      NS_ERROR_UNEXPECTED);
  }

  // get the URI spec
  //
  rv = mURI->GetSpec(getter_Copies(spec));
  NS_ENSURE_SUCCESS(rv, rv);

  // do the search
  //
  rv = lds(this, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // close the pipe
  //	
  rv = mReadPipeOut->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  // all done
  //
  mAsyncListener->OnStopRequest(this, mResponseContext, NS_OK, nsnull);

#ifdef DEBUG
    // gdb cannot handle threads exiting, so we sit around forever
    //
    while (1) {
	PR_Sleep(20000);
    }
#endif	    

  return NS_OK;
}

// XXX this function should go away
//
nsresult
nsLDAPChannel::pipeWrite(char *str)
{
  PRUint32 bytesWritten=0;
  nsresult rv = NS_OK;

  rv = mReadPipeOut->Write(str, strlen(str), &bytesWritten);
  NS_ENSURE_SUCCESS(rv, rv);

  mAsyncListener->OnDataAvailable(this, mResponseContext, mReadPipeIn, 
				  mReadPipeOffset, strlen(str));
  mReadPipeOffset += bytesWritten;
  return NS_OK;
}

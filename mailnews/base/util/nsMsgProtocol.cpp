/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h"
#include "nsMsgProtocol.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsISocketTransportService.h"
#include "nsXPIDLString.h"
#include "nsSpecialSystemDirectory.h"
#include "nsILoadGroup.h"
#include "nsIIOService.h"
#include "nsFileStream.h"
#include "nsINetSupportDialogService.h"
#include "nsIDNSService.h"

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
NS_IMPL_ISUPPORTS3(nsMsgProtocol, nsIStreamListener, nsIStreamObserver, nsIChannel)

nsMsgProtocol::nsMsgProtocol(nsIURI * aURL, nsIURI* originalURI)
{
	NS_INIT_REFCNT();
	m_flags = 0;
	m_startPosition = 0;
	m_readCount = 0;
	m_socketIsOpen = PR_FALSE;
		
	m_tempMsgFileSpec = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_TemporaryDirectory);
	m_tempMsgFileSpec += "tempMessage.eml";
    m_url = aURL;
    m_originalUrl = originalURI ? originalURI : aURL;
}

nsMsgProtocol::~nsMsgProtocol()
{}

nsresult nsMsgProtocol::OpenNetworkSocketWithInfo(const char * aHostName, PRInt32 aGetPort, const char *connectionType)
{
  NS_ENSURE_ARG(aHostName);

  nsresult rv = NS_OK;
  nsCOMPtr<nsISocketTransportService> socketService (do_GetService(kSocketTransportServiceCID));
  NS_ENSURE_TRUE(socketService, NS_ERROR_FAILURE);
  
	m_readCount = -1; // with socket connections we want to read as much data as arrives
	m_startPosition = 0;

  rv = socketService->CreateTransportOfType(connectionType, aHostName, aGetPort, nsnull, 0, 0, getter_AddRefs(m_channel));
  if (NS_FAILED(rv)) return rv;

  m_socketIsOpen = PR_FALSE;
	return SetupTransportState();
}

nsresult nsMsgProtocol::OpenNetworkSocket(nsIURI * aURL, const char *connectionType) // open a connection on this url
{
  NS_ENSURE_ARG(aURL);

  nsXPIDLCString hostName;
	PRInt32 port = 0;

	aURL->GetPort(&port);
	aURL->GetHost(getter_Copies(hostName));

  return OpenNetworkSocketWithInfo(hostName, port, connectionType);
}

nsresult nsMsgProtocol::OpenFileSocket(nsIURI * aURL, const nsFileSpec * aFileSpec, PRUint32 aStartPosition, PRInt32 aReadCount)
{
	// mscott - file needs to be encoded directly into aURL. I should be able to get
	// rid of this method completely.

	nsresult rv = NS_OK;
	m_startPosition = aStartPosition;
	m_readCount = aReadCount;

    NS_WITH_SERVICE(nsIIOService, netService, kIOServiceCID, &rv);
	if (NS_SUCCEEDED(rv) && aURL)
	{
		// extract the file path from the uri...
		nsXPIDLCString filePath;
		aURL->GetPath(getter_Copies(filePath));
		char * urlSpec = PR_smprintf("file://%s", (const char *) filePath);

    rv = netService->NewChannel("Load", urlSpec, 
                                    nsnull,     // null base URI
                                    m_loadGroup,     // loadGroup
                                    nsnull,     // notificationCallbacks
                                    nsIChannel::LOAD_NORMAL, 
                                    nsnull,     // originalURI
                                    0, 0, 
                                    getter_AddRefs(m_channel));
		PR_FREEIF(urlSpec);

		if (NS_SUCCEEDED(rv) && m_channel)
		{
			m_socketIsOpen = PR_FALSE;
			// rv = SetupTransportState();
		}
	}

	return rv;
}

nsresult nsMsgProtocol::SetupTransportState()
{
	nsresult rv = NS_OK;

	if (!m_socketIsOpen && m_channel)
	{
		rv = m_channel->OpenOutputStream(0 /* start position */, getter_AddRefs(m_outputStream));

		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create an output stream");
		// we want to open the stream 
	} // if m_transport

	return rv;
}

nsresult nsMsgProtocol::CloseSocket()
{
	nsresult rv = NS_OK;

	// release all of our socket state
	m_socketIsOpen = PR_FALSE;
	m_outputStream = null_nsCOMPtr();

	// we need to call Cancel so that we remove the socket transport from the mActiveTransportList.  see bug #30648
	if (m_channel) {
		rv = m_channel->Cancel();
	}
	m_channel = null_nsCOMPtr();

	return rv;
}

/*
 * Writes the data contained in dataBuffer into the current output stream. It also informs
 * the transport layer that this data is now available for transmission.
 * Returns a positive number for success, 0 for failure (not all the bytes were written to the
 * stream, etc). We need to make another pass through this file to install an error system (mscott)
 */

PRInt32 nsMsgProtocol::SendData(nsIURI * aURL, const char * dataBuffer)
{
	PRUint32 writeCount = 0; 
	PRInt32 status = 0; 

//	NS_PRECONDITION(m_outputStream, "oops....we don't have an output stream...how did that happen?");
	if (dataBuffer && m_outputStream)
	{
		status = m_outputStream->Write(dataBuffer, PL_strlen(dataBuffer), &writeCount);
	}

	return status;
}

// Whenever data arrives from the connection, core netlib notifices the protocol by calling
// OnDataAvailable. We then read and process the incoming data from the input stream. 
NS_IMETHODIMP nsMsgProtocol::OnDataAvailable(nsIChannel * /* aChannel */, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
	// right now, this really just means turn around and churn through the state machine
	nsCOMPtr<nsIURI> uri = do_QueryInterface(ctxt);
	return ProcessProtocolState(uri, inStr, sourceOffset, count);
}

NS_IMETHODIMP nsMsgProtocol::OnStartRequest(nsIChannel * aChannel, nsISupports *ctxt)
{
	nsresult rv = NS_OK;
	nsCOMPtr <nsIMsgMailNewsUrl> aMsgUrl = do_QueryInterface(ctxt, &rv);
	if (NS_SUCCEEDED(rv) && aMsgUrl)
	{
		rv = aMsgUrl->SetUrlState(PR_TRUE, NS_OK);
		if (m_loadGroup)
			m_loadGroup->AddChannel(NS_STATIC_CAST(nsIChannel *, this), nsnull /* context isupports */);
	}

	// if we are set up as a channel, we should notify our channel listener that we are starting...
	// so pass in ourself as the channel and not the underlying socket or file channel the protocol
	// happens to be using
	if (m_channelListener)
		rv = m_channelListener->OnStartRequest(this, m_channelContext);

	return rv;
}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsMsgProtocol::OnStopRequest(nsIChannel * aChannel, nsISupports *ctxt, nsresult aStatus, const PRUnichar* aMsg)
{
	nsresult rv = NS_OK;

	// if we are set up as a channel, we should notify our channel listener that we are starting...
	// so pass in ourself as the channel and not the underlying socket or file channel the protocol
	// happens to be using
	if (m_channelListener)
		rv = m_channelListener->OnStopRequest(this, m_channelContext, aStatus, aMsg);
	
  nsCOMPtr <nsIMsgMailNewsUrl> aMsgUrl = do_QueryInterface(ctxt, &rv);
	if (NS_SUCCEEDED(rv) && aMsgUrl)
	{
		rv = aMsgUrl->SetUrlState(PR_FALSE, aStatus);
		if (m_loadGroup)
			m_loadGroup->RemoveChannel(NS_STATIC_CAST(nsIChannel *, this), nsnull, aStatus, nsnull);
	}

	
	// !NS_BINDING_ABORTED because we don't want to see an alert if the user 
	// cancelled the operation.  also, we'll get here because we call Cancel()
	// to force removal of the nsSocketTransport.  see CloseSocket()
	// bugs #30775 and #30648 relate to this
	if (NS_FAILED(aStatus) && (aStatus != NS_BINDING_ABORTED)) {
		NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &rv);
		if (NS_SUCCEEDED(rv) && dialog) {
			// todo, put this into a string bundle
			nsAutoString alertMsg("unknown error.");
			switch (aStatus) {
				case NS_ERROR_UNKNOWN_HOST:
						// todo, put this into a string bundle
						alertMsg = "Failed to connect to the server.";
						break;
               case NS_ERROR_CONNECTION_REFUSED:
						// todo, put this into a string bundle
						alertMsg = "Connection refused to the server.";
						break;
               case NS_ERROR_NET_TIMEOUT:
						// todo, put this into a string bundle
						alertMsg = "Connection to the server timed out.";
						break;
               default:
						break;
			}
			dialog->Alert(alertMsg.GetUnicode());
		}
	}

	return rv;
}

nsresult nsMsgProtocol::LoadUrl(nsIURI * aURL, nsISupports * aConsumer)
{
	// okay now kick us off to the next state...
	// our first state is a process state so drive the state machine...
	nsresult rv = NS_OK;
	nsCOMPtr <nsIMsgMailNewsUrl> aMsgUrl = do_QueryInterface(aURL);

	if (NS_SUCCEEDED(rv))
	{
		rv = aMsgUrl->SetUrlState(PR_TRUE, NS_OK); // set the url as a url currently being run...

    // if the url is given a stream consumer then we should use it to forward calls to...
    if (!m_channelListener && aConsumer) // if we don't have a registered listener already
    {
      m_channelListener = do_QueryInterface(aConsumer);
      if (!m_channelContext)
        m_channelContext = do_QueryInterface(aURL);
    }

		if (!m_socketIsOpen)
		{
			nsCOMPtr<nsISupports> urlSupports = do_QueryInterface(aURL);

      if (m_channel)
      {
          // put us in a state where we are always notified of incoming data
          m_channel->AsyncRead(m_startPosition, m_readCount, urlSupports ,this /* stream observer */);
          m_socketIsOpen = PR_TRUE; // mark the channel as open
      }
		} // if we got an event queue service
		else  // the connection is already open so we should begin processing our new url...
			rv = ProcessProtocolState(aURL, nsnull, 0, 0); 
	}

	return rv;
}

///////////////////////////////////////////////////////////////////////
// The rest of this file is mostly nsIChannel mumbo jumbo stuff
///////////////////////////////////////////////////////////////////////

nsresult nsMsgProtocol::SetUrl(nsIURI * aURL)
{
	m_url = dont_QueryInterface(aURL);
	return NS_OK;
}

NS_IMETHODIMP nsMsgProtocol::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
	m_loadGroup = dont_QueryInterface(aLoadGroup);
	return NS_OK;
}

NS_IMETHODIMP nsMsgProtocol::GetOriginalURI(nsIURI * *aURI)
{
    *aURI = m_originalUrl;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}
 
NS_IMETHODIMP nsMsgProtocol::GetURI(nsIURI * *aURI)
{
    *aURI = m_url;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}
 
NS_IMETHODIMP nsMsgProtocol::OpenInputStream(PRUint32 startPosition, PRInt32 readCount, nsIInputStream **_retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgProtocol::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgProtocol::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
  // we already know the content type...
  observer->OnStartRequest(this, ctxt);
  return NS_OK;
}

NS_IMETHODIMP nsMsgProtocol::AsyncRead(PRUint32 startPosition, PRInt32 readCount, nsISupports *ctxt, nsIStreamListener *listener)
{
	// set the stream listener and then load the url
	m_channelContext = ctxt;
	m_channelListener = listener;

	// the following load group code is completely bogus....
	nsresult rv = NS_OK;
	if (m_loadGroup)
	{
		nsCOMPtr<nsILoadGroupListenerFactory> factory;
		//
		// Create a load group "proxy" listener...
		//
		rv = m_loadGroup->GetGroupListenerFactory(getter_AddRefs(factory));
		if (factory) 
		{
			nsCOMPtr<nsIStreamListener> newListener;
			rv = factory->CreateLoadGroupListener(m_channelListener, getter_AddRefs(newListener));
			if (NS_SUCCEEDED(rv)) 
				m_channelListener = newListener;
		}
	} // if aLoadGroup

	return LoadUrl(m_url, nsnull);
}

NS_IMETHODIMP nsMsgProtocol::AsyncWrite(nsIInputStream *fromStream, PRUint32 startPosition, PRInt32 writeCount, nsISupports *ctxt, nsIStreamObserver *observer)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgProtocol::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP nsMsgProtocol::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
  mLoadAttributes = aLoadAttributes;
  return NS_OK;       // don't fail when trying to set this
}

NS_IMETHODIMP nsMsgProtocol::GetContentType(char * *aContentType)
{
  // as url dispatching matures, we'll be intelligent and actually start
  // opening the url before specifying the content type. This will allow
  // us to optimize the case where the message url actual refers to  
  // a part in the message that has a content type that is not message/rfc822

	*aContentType = nsCRT::strdup("message/rfc822");
	return NS_OK;
}

NS_IMETHODIMP nsMsgProtocol::SetContentType(const char *aContentType)
{
    // XXX: Do not allow the content type to be changed (yet)
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgProtocol::GetContentLength(PRInt32 * aContentLength)
{
  *aContentLength = -1;
  return NS_OK;
}

NS_IMETHODIMP nsMsgProtocol::GetOwner(nsISupports * *aPrincipal)
{
    *aPrincipal = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsMsgProtocol::SetOwner(nsISupports * aPrincipal)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgProtocol::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = m_loadGroup;
    NS_IF_ADDREF(*aLoadGroup);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgProtocol::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgProtocol::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
	return NS_OK;       // don't fail when trying to set this
}


NS_IMETHODIMP 
nsMsgProtocol::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMsgProtocol::IsPending(PRBool *result)
{
    *result = PR_TRUE;
    return NS_OK; 
}

NS_IMETHODIMP nsMsgProtocol::Cancel()
{
	return m_channel->Cancel();
}

NS_IMETHODIMP nsMsgProtocol::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgProtocol::Resume()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgProtocol::PostMessage(nsIURI* url, nsIFileSpec *fileSpec)
{
    if (!url || !fileSpec) return NS_ERROR_NULL_POINTER;

#define POST_DATA_BUFFER_SIZE 2048

    // mscott -- this function should be re-written to use the file url code
    // so it can be asynch
    nsFileSpec afileSpec;
    fileSpec->GetFileSpec(&afileSpec);
    nsInputFileStream * fileStream = new nsInputFileStream(afileSpec,
                                                           PR_RDONLY, 00700);
    if (fileStream && fileStream->is_open())
    {
        PRInt32 amtInBuffer = 0; 
        PRBool lastLineWasComplete = PR_TRUE;
        
        PRBool quoteLines = PR_TRUE;  // it is always true but I'd like to
                                      // generalize this function and then it
                                      // might not be 
        char buffer[POST_DATA_BUFFER_SIZE];
        
        if (quoteLines /* || add_crlf_to_line_endings */)
        {
            char *line;
            char * b = buffer;
            PRInt32 bsize = POST_DATA_BUFFER_SIZE;
            amtInBuffer =  0;
            do {
                lastLineWasComplete = PR_TRUE;
                PRInt32 L = 0;
                if (fileStream->eof())
                {
                    line = nsnull;
                    break;
                }
                
                if (!fileStream->readline(b, bsize-5)) 
                    lastLineWasComplete = PR_FALSE;
                line = b;
                
                L = PL_strlen(line);
                
                /* escape periods only if quote_lines_p is set
                 */
                if (quoteLines && lastLineWasComplete && line[0] == '.')
                {
                    /* This line begins with "." so we need to quote it
                       by adding another "." to the beginning of the line.
                       */
                    PRInt32 i;
                    line[L+1] = 0;
                    for (i = L; i > 0; i--)
                        line[i] = line[i-1];
                    L++;
                }
                
                if (!lastLineWasComplete || (L > 1 && line[L-2] == CR &&
                                             line[L-1] == LF))
                {
                    /* already ok */
                }
                else if(L > 0 /* && (line[L-1] == LF || line[L-1] == CR) */)
                {
                    /* only add the crlf if required
                     * we still need to do all the
                     * if comparisons here to know
                     * if the line was complete
                     */
                    if(/* add_crlf_to_line_endings */ PR_TRUE)
                    {
                        /* Change newline to CRLF. */
                          line[L++] = CR;
                          line[L++] = LF;
                          line[L] = 0;
                    }
                }
                else if (L == 0 && !fileStream->eof()
                         /* && add_crlf_to_line_endings */)
                {
                    // jt ** empty line; output CRLF
                    line[L++] = CR;
                    line[L++] = LF;
                    line[L] = 0;
                }
                
                bsize -= L;
                b += L;
                amtInBuffer += L;
                // test hack by mscott. If our buffer is almost full, then
                // send it off & reset ourselves 
                // to make more room.
                if (bsize < 100) // i chose 100 arbitrarily.
                {
                    if (*buffer)
                        SendData(url, buffer);
                    buffer[0] = '\0';
                    b = buffer; // reset buffer
                    bsize = POST_DATA_BUFFER_SIZE;
                }
                
            } while (line /* && bsize > 100 */);
        }
        
        SendData(url, buffer); 
        delete fileStream;
    }
    return NS_OK;
}

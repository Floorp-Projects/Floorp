/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsMsgProtocol.h"
#include "nsINetService.h"

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

NS_IMPL_ISUPPORTS(nsMsgProtocol, nsIStreamListener::GetIID())

nsMsgProtocol::nsMsgProtocol()
{
	NS_INIT_REFCNT();
	m_flags = 0;
	m_socketIsOpen = PR_FALSE;
}

nsMsgProtocol::~nsMsgProtocol()
{}

nsresult nsMsgProtocol::OpenNetworkSocket(nsIURL * aURL, PRUint32 aPort, const char * aHostName) // open a connection on this url
{
	nsresult rv = NS_OK;

	NS_WITH_SERVICE(nsINetService, pNetService, kNetServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && pNetService) 
	{
		rv = pNetService->CreateSocketTransport(getter_AddRefs(m_transport), aPort, aHostName);
		if (NS_SUCCEEDED(rv) && m_transport)
		{
			rv = SetupTransportState();
			m_socketIsOpen = PR_TRUE;
		}
	}

	return rv;
}

nsresult nsMsgProtocol::OpenFileSocket(nsIURL * aURL, nsFileSpec * aFileSpec)
{
	nsresult rv = NS_OK;
	
	NS_WITH_SERVICE(nsINetService, pNetService, kNetServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && pNetService && aURL) 
	{
		nsFilePath filePath(*aFileSpec);
		rv = pNetService->CreateFileSocketTransport(getter_AddRefs(m_transport), filePath);
		if (NS_SUCCEEDED(rv) && m_transport)
		{
			rv = SetupTransportState();
			m_socketIsOpen = PR_TRUE;
		}
	}

	return rv;
}

nsresult nsMsgProtocol::SetupTransportState()
{
	nsresult rv = NS_OK;

	if (m_transport)
	{
		rv = m_transport->GetOutputStream(getter_AddRefs(m_outputStream));
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create an output stream");

		rv = m_transport->GetOutputStreamConsumer(getter_AddRefs(m_outputConsumer));
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create an output consumer");

		// register self as the consumer for the socket...
		rv = m_transport->SetInputStreamConsumer((nsIStreamListener *) this);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register NNTP instance as a consumer on the socket");
	} // if m_transport

	return rv;
}

nsresult nsMsgProtocol::CloseSocket()
{
	// release all of our socket state
	m_socketIsOpen = PR_FALSE;
	m_outputStream = null_nsCOMPtr();
	m_outputConsumer = null_nsCOMPtr();
	m_transport = null_nsCOMPtr();

	return NS_OK;
}

/*
 * Writes the data contained in dataBuffer into the current output stream. It also informs
 * the transport layer that this data is now available for transmission.
 * Returns a positive number for success, 0 for failure (not all the bytes were written to the
 * stream, etc). We need to make another pass through this file to install an error system (mscott)
 */

PRInt32 nsMsgProtocol::SendData(nsIURL * aURL, const char * dataBuffer)
{
	PRUint32 writeCount = 0; 
	PRInt32 status = 0; 

	NS_PRECONDITION(m_outputStream && m_outputConsumer, "no registered consumer for our output");
	if (dataBuffer && m_outputStream)
	{
		nsresult rv = m_outputStream->Write(dataBuffer, PL_strlen(dataBuffer), &writeCount);
		if (NS_SUCCEEDED(rv) && writeCount == PL_strlen(dataBuffer))
		{
			nsCOMPtr<nsIInputStream> inputStream;
			inputStream = do_QueryInterface(m_outputStream);
			if (inputStream)
				m_outputConsumer->OnDataAvailable(aURL, inputStream, writeCount);
			status = 1; // mscott: we need some type of MK_OK? MK_SUCCESS? Arrgghhh
		}
		else // the write failed for some reason, returning 0 trips an error by the caller
			status = 0; // mscott: again, I really want to add an error code here!!
	}

	return status;
}

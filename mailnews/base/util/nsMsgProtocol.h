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

#ifndef nsMsgProtocol_h__
#define nsMsgProtocol_h__

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "nsITransport.h"

// This is a helper class used to encapsulate code shared between all of the
// mailnews protocol objects (imap, news, pop, smtp, etc.) In particular,
// it unifies the core networking code for the protocols. My hope is that
// this will make unification with Necko easier as we'll only have to change
// this class and not all of the mailnews protocols.
class NS_MSG_BASE nsMsgProtocol : public nsIStreamListener
{
public:
	nsMsgProtocol();
	virtual ~nsMsgProtocol();

	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIStreamListener interface 
	////////////////////////////////////////////////////////////////////////////////////////

	// Whenever data arrives from the connection, core netlib notifies the protocol by calling
	// OnDataAvailable. We then read and process the incoming data from the input stream. 
	// a typical protocol shouldn't need to override this method
	NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength);

	// I expect most protocols to override this method AND call into the base class
	// the base class takes the url and sets the state of the url passed in to be running
	NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

	// stop binding is a "notification" informing us that the stream associated with aURL is going away.
	// the base class implementation takes the url and sets the url state to NOT running.
	NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

	// Ideally, a protocol should only have to support the stream listener methods covered above. 
	// However, we don't have this nsIStreamListenerLite interface defined yet. Until then, we are using
	// nsIStreamListener so we need to add stubs for the heavy weight stuff we don't want to use.

	NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) { return NS_OK;}
	NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg) { return NS_OK;}
	NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) { return NS_OK;} ;

	////////////////////////////////////////////////////////////////////////////////////////
	// End of nsIStreamListenerSupport
	////////////////////////////////////////////////////////////////////////////////////////

	// LoadUrl -- A protocol typically overrides this function, sets up any local state for the url and
	// then calls the base class which opens the socket if it needs opened. If the socket is 
	// already opened then we just call ProcessProtocolState to start the churning process.
	virtual nsresult LoadUrl(nsIURL * aURL);

	// Flag manipulators
	PRBool TestFlag  (PRUint32 flag) {return flag & m_flags;}
	void   SetFlag   (PRUint32 flag) { m_flags |= flag; }
	void   ClearFlag (PRUint32 flag) { m_flags &= ~flag; }

protected:
	// methods for opening and closing a socket with core netlib....
	// mscott -okay this is lame. I should break this up into a file protocol and a socket based
	// protocool class instead of cheating and putting both methods here...
	virtual nsresult OpenNetworkSocket(nsIURL * aURL, PRUint32 aPort, const char * aHostName); // open a connection on this url
	virtual nsresult OpenFileSocket(nsIURL * aURL, const nsFileSpec * aFileSpec); // used to open a file socket connection

	// a Protocol typically overrides this method. They free any of their own connection state and then
	// they call up into the base class to free the generic connection objects
	virtual nsresult CloseSocket(); 

	virtual nsresult SetupTransportState(); // private method used by OpenNetworkSocket and OpenFileSocket

	// ProcessProtocolState - This is the function that gets churned by calls to OnDataAvailable. 
	// As data arrives on the socket, OnDataAvailable calls ProcessProtocolState.
	virtual nsresult ProcessProtocolState(nsIURL * url, nsIInputStream * inputStream, PRUint32 length) = 0;

	// SendData -- Writes the data contained in dataBuffer into the current output stream. 
	// It also informs the transport layer that this data is now available for transmission.
	// Returns a positive number for success, 0 for failure (not all the bytes were written to the
	// stream, etc). 
	virtual PRInt32 SendData(nsIURL * aURL, const char * dataBuffer);

	// Ouput stream for writing commands to the socket
	nsCOMPtr<nsITransport>		m_transport; 
	nsCOMPtr<nsIOutputStream>	m_outputStream;   // this will be obtained from the transport interface
	nsCOMPtr<nsIStreamListener> m_outputConsumer; // this will be obtained from the transport interface

	PRBool	  m_socketIsOpen; // mscott: we should look into keeping this state in the nsSocketTransport...
							  // I'm using it to make sure I open the socket the first time a URL is loaded into the connection
	PRUint32  m_flags; // used to store flag information
};

#endif /* nsMsgProtocol_h__ */

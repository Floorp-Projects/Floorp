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

#ifndef nsMsgProtocol_h__
#define nsMsgProtocol_h__

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIChannel.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"

class nsIPrompt;
class nsIMsgMailNewsUrl;

// This is a helper class used to encapsulate code shared between all of the
// mailnews protocol objects (imap, news, pop, smtp, etc.) In particular,
// it unifies the core networking code for the protocols. My hope is that
// this will make unification with Necko easier as we'll only have to change
// this class and not all of the mailnews protocols.
class NS_MSG_BASE nsMsgProtocol : public nsIStreamListener, public nsIChannel
{
public:
	nsMsgProtocol(nsIURI * aURL);
	virtual ~nsMsgProtocol();

	NS_DECL_ISUPPORTS
	// nsIChannel support
	NS_DECL_NSICHANNEL
	NS_DECL_NSIREQUEST
  
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSISTREAMOBSERVER

	// LoadUrl -- A protocol typically overrides this function, sets up any local state for the url and
	// then calls the base class which opens the socket if it needs opened. If the socket is 
	// already opened then we just call ProcessProtocolState to start the churning process.
	// aConsumer is the consumer for the url. It can be null if this argument is not appropriate
	virtual nsresult LoadUrl(nsIURI * aURL, nsISupports * aConsumer = nsnull);

	virtual nsresult SetUrl(nsIURI * aURL); // sometimes we want to set the url before we load it

	// Flag manipulators
	virtual PRBool TestFlag  (PRUint32 flag) {return flag & m_flags;}
	virtual void   SetFlag   (PRUint32 flag) { m_flags |= flag; }
	virtual void   ClearFlag (PRUint32 flag) { m_flags &= ~flag; }

protected:
	// methods for opening and closing a socket with core netlib....
	// mscott -okay this is lame. I should break this up into a file protocol and a socket based
	// protocool class instead of cheating and putting both methods here...
	virtual nsresult OpenNetworkSocket(nsIURI * aURL, const char *connectionType); // open a connection on this url
  virtual nsresult OpenNetworkSocketWithInfo(const char * aHostName, PRInt32 aGetPort, const char *connectionType); // open a connection with a specific host and port
	virtual nsresult OpenFileSocket(nsIURI * aURL, const nsFileSpec * aFileSpec, PRUint32 aStartPosition, PRInt32 aReadCount); // used to open a file socket connection

	// a Protocol typically overrides this method. They free any of their own connection state and then
	// they call up into the base class to free the generic connection objects
	virtual nsresult CloseSocket(); 

	virtual nsresult SetupTransportState(); // private method used by OpenNetworkSocket and OpenFileSocket

	// ProcessProtocolState - This is the function that gets churned by calls to OnDataAvailable. 
	// As data arrives on the socket, OnDataAvailable calls ProcessProtocolState.
	
	virtual nsresult ProcessProtocolState(nsIURI * url, nsIInputStream * inputStream, 
									      PRUint32 sourceOffset, PRUint32 length) = 0;

	// SendData -- Writes the data contained in dataBuffer into the current output stream. 
	// It also informs the transport layer that this data is now available for transmission.
	// Returns a positive number for success, 0 for failure (not all the bytes were written to the
	// stream, etc). 
    // aSupressLogging is a hint that sensitive data is being sent and should not be logged
	virtual PRInt32 SendData(nsIURI * aURL, const char * dataBuffer, PRBool aSupressLogging = PR_FALSE);

  virtual nsresult PostMessage(nsIURI* url, nsIFileSpec * fileSpec);

  virtual nsresult InitFromURI(nsIURI *aUrl);

	// Ouput stream for writing commands to the socket
	nsCOMPtr<nsIChannel>		m_channel; 
	nsCOMPtr<nsIOutputStream>	m_outputStream;   // this will be obtained from the transport interface

	PRBool	  m_socketIsOpen; // mscott: we should look into keeping this state in the nsSocketTransport...
							  // I'm using it to make sure I open the socket the first time a URL is loaded into the connection
	PRUint32	m_flags; // used to store flag information
	PRUint32	m_startPosition;
	PRInt32		m_readCount;

	nsFileSpec	m_tempMsgFileSpec;  // we currently have a hack where displaying a msg involves writing it to a temp file first

	// the following is a catch all for nsIChannel related data
	nsCOMPtr<nsIURI>            m_originalUrl;  // the original url
	nsCOMPtr<nsIURI>            m_url;          // the running url
	nsCOMPtr<nsIStreamListener> m_channelListener;
	nsCOMPtr<nsISupports>	    	m_channelContext;
	nsCOMPtr<nsILoadGroup>		  m_loadGroup;
  nsLoadFlags                 mLoadAttributes;
  nsCOMPtr<nsIProgressEventSink> mProgressEventSink;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsISupports>       mOwner;
  nsCString                   m_ContentType;

  // private helper routine used by subclasses to quickly get a reference to the correct prompt dialog
  // for a mailnews url. 
  nsresult GetPromptDialogFromUrl(nsIMsgMailNewsUrl * aMsgUrl, nsIPrompt ** aPromptDialog);

  // if a url isn't going to result in any content then we want to supress calls to
  // OnStartRequest, OnDataAvailable and OnStopRequest
  PRBool mSupressListenerNotifications;
};

#endif /* nsMsgProtocol_h__ */

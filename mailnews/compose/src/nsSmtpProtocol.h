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

#ifndef nsSmtpProtocol_h___
#define nsSmtpProtocol_h___

#include "nsIStreamListener.h"
#include "nsITransport.h"
#include "rosetta.h"
#include HG40855

#include "nsIOutputStream.h"
#include "nsISmtpUrl.h"

/* states of the machine
 */
typedef enum _SmtpState {
SMTP_RESPONSE = 0, 
SMTP_START_CONNECT,
SMTP_FINISH_CONNECT,
SMTP_LOGIN_RESPONSE,
SMTP_SEND_HELO_RESPONSE,
SMTP_SEND_VRFY_RESPONSE,
SMTP_SEND_MAIL_RESPONSE,
SMTP_SEND_RCPT_RESPONSE,
SMTP_SEND_DATA_RESPONSE,
SMTP_SEND_POST_DATA, 
SMTP_SEND_MESSAGE_RESPONSE,
SMTP_DONE,
SMTP_ERROR_DONE,
SMTP_FREE,
SMTP_EXTN_LOGIN_RESPONSE,
SMTP_SEND_EHLO_RESPONSE,
SMTP_SEND_AUTH_LOGIN_USERNAME,
SMTP_SEND_AUTH_LOGIN_PASSWORD,
SMTP_AUTH_LOGIN_RESPONSE,
SMTP_AUTH_RESPONSE
} SmtpState;

// State Flags (Note, I use the word state in terms of storing 
// state information about the connection (authentication, have we sent
// commands, etc. I do not intend it to refer to protocol state)
#define SMTP_PAUSE_FOR_READ			0x00000001  /* should we pause for the next read */
#define SMTP_EHLO_DSN_ENABLED		0x00000002
#define SMTP_AUTH_LOGIN_ENABLED		0x00000004

typedef enum _SmtpAuthMethod {
	SMTP_AUTH_NONE = 0,
	SMTP_AUTH_LOGIN = 1,
	SMTP_AUTH_PLAIN = 2
 } SmtpAuthMethod;

class nsSmtpProtocol : public nsIStreamListener
{
public:
	// Creating a protocol instance requires the URL which needs to be run AND it requires
	// a transport layer. 
	nsSmtpProtocol(nsIURL * aURL, nsITransport * transportLayer);
	
	virtual ~nsSmtpProtocol();

	PRInt32 LoadURL(nsIURL * aURL);
	PRBool  IsRunningUrl() { return m_urlInProgress;} // returns true if we are currently running a url and false otherwise...

	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIStreamListener interface 
	////////////////////////////////////////////////////////////////////////////////////////

	// mscott; I don't think we need to worry about this yet so I'll leave it stubbed out for now
	NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) { return NS_OK;} ;
	
	// Whenever data arrives from the connection, core netlib notifies the protocol by calling
	// OnDataAvailable. We then read and process the incoming data from the input stream. 
	NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength);

	NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

	// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
	NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

	// Ideally, a protocol should only have to support the stream listener methods covered above. 
	// However, we don't have this nsIStreamListenerLite interface defined yet. Until then, we are using
	// nsIStreamListener so we need to add stubs for the heavy weight stuff we don't want to use.

	NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) { return NS_OK;}
	NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg) { return NS_OK;}

	////////////////////////////////////////////////////////////////////////////////////////
	// End of nsIStreamListenerSupport
	////////////////////////////////////////////////////////////////////////////////////////

	// Flag manipulators
	PRBool TestFlag  (PRUint32 flag) {return flag & m_flags;}
	void   SetFlag   (PRUint32 flag) { m_flags |= flag; }
	void   ClearFlag (PRUint32 flag) { m_flags &= ~flag; }

private:
	// the following flag is used to determine when a url is currently being run. It is cleared on calls
	// to ::StopBinding and it is set whenever we call Load on a url
	PRBool	m_urlInProgress;	

	// Smtp Event Sinks

	// Ouput stream for writing commands to the socket
	nsITransport			* m_transport; 
	nsIOutputStream			* m_outputStream;   // this will be obtained from the transport interface
	nsIStreamListener	    * m_outputConsumer; // this will be obtained from the transport interface

	// the nsISmtpURL that is currently running
	nsISmtpUrl				* m_runningURL;
	PRUint32 m_flags;		// used to store flag information
	PRUint32 m_LastTime;

	HG60917

	// Generic state information -- What state are we in? What state do we want to go to
	// after the next response? What was the last response code? etc. 
	SmtpState	m_nextState;
    SmtpState	m_nextStateAfterResponse;
    PRInt32     m_responseCode;    /* code returned from Smtp server */
	PRInt32 	m_previousResponseCode; 
	PRInt32		m_continuationResponse;
    char       *m_responseText;   /* text returned from Smtp server */
	char	   *m_hostName;

	char	   *m_AddressCopy;
	char	   *m_addresses;
	char	   *m_addressesLeft;
	char	   *m_verifyAddress;
	void	   *m_PostData;			// mscott: I'm going to try to get rid of this and be more explicit...
	
	SmtpAuthMethod m_authMethod;

	// message specific information
	PRInt32		m_totalAmountWritten;
	PRUint32	m_totalMessageSize;
    
	char		*m_dataBuf;
    PRUint32	 m_dataBufSize;

	PRBool	  m_socketIsOpen; // mscott: we should look into keeping this state in the nsSocketTransport...
							  // I'm using it to make sure I open the socket the first time a URL is loaded into the connection

	PRInt32   m_originalContentLength; /* the content length at the time of calling graph progress */
	
	PRInt32	  ProcessSmtpState(nsIURL * url, nsIInputStream * inputStream, PRUint32 length);
	PRInt32	  CloseConnection(); // releases and closes down this protocol instance...

	// initialization function given a new url and transport layer
	void Initialize(nsIURL * aURL, nsITransport * transportLayer);

	////////////////////////////////////////////////////////////////////////////////////////
	// Communication methods --> Reading and writing protocol
	////////////////////////////////////////////////////////////////////////////////////////

	PRInt32 ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line);

	// SendData not only writes the NULL terminated data in dataBuffer to our output stream
	// but it also informs the consumer that the data has been written to the stream.
	PRInt32 SendData(const char * dataBuffer);

	////////////////////////////////////////////////////////////////////////////////////////
	// Protocol Methods --> This protocol is state driven so each protocol method is 
	//						designed to re-act to the current "state". I've attempted to 
	//						group them together based on functionality. 
	////////////////////////////////////////////////////////////////////////////////////////
	
	PRInt32 SmtpResponse(nsIInputStream * inputStream, PRUint32 length); 
	PRInt32 LoginResponse(nsIInputStream * inputStream, PRUint32 length);
	PRInt32 ExtensionLoginResponse(nsIInputStream * inputStream, PRUint32 length);
	PRInt32 SendHeloResponse(nsIInputStream * inputStream, PRUint32 length);
	PRInt32 SendEhloResponse(nsIInputStream * inputStream, PRUint32 length);	

	PRInt32 AuthLoginUsername();
	PRInt32 AuthLoginPassword();
	PRInt32 AuthLoginResponse(nsIInputStream * stream, PRUint32 length);

	PRInt32 SendVerifyResponse(); // mscott: this one is apparently unimplemented...
	PRInt32 SendMailResponse();
	PRInt32 SendRecipientResponse();
	PRInt32 SendDataResponse();
	PRInt32 SendPostData();
	PRInt32 SendMessageResponse();

	PRInt32 CheckAuthResponse();


	////////////////////////////////////////////////////////////////////////////////////////
	// End of Protocol Methods
	////////////////////////////////////////////////////////////////////////////////////////

//	PRInt32 ParseURL(nsIURL * aURL, char ** aHostAndPort, PRBool * bValP, char ** aGroup, char ** aMessageID, char ** aCommandSpecificData);
};

#endif  // nsSmtpProtocol_h___

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

#include "nsMsgProtocol.h"
#include "nsIStreamListener.h"
#include "nsCOMPtr.h"
#include "rosetta.h"
#include HG40855
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

class nsSmtpProtocol : public nsMsgProtocol
{
public:
	// Creating a protocol instance requires the URL which needs to be run.
	nsSmtpProtocol(nsIURI * aURL);
	virtual ~nsSmtpProtocol();

	virtual nsresult LoadUrl(nsIURI * aURL, nsISupports * aConsumer = nsnull);

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIStreamListener interface 
	////////////////////////////////////////////////////////////////////////////////////////

	// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
	NS_IMETHOD OnStopRequest(nsIChannel * aChannel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg);

private:
	// the nsISmtpURL that is currently running
	nsCOMPtr<nsISmtpUrl>		m_runningURL;
	PRUint32 m_LastTime;

	// Generic state information -- What state are we in? What state do we want to go to
	// after the next response? What was the last response code? etc. 
	SmtpState	m_nextState;
    SmtpState	m_nextStateAfterResponse;
    PRInt32     m_responseCode;    /* code returned from Smtp server */
	PRInt32 	m_previousResponseCode; 
	PRInt32		m_continuationResponse;
    nsCString   m_responseText;   /* text returned from Smtp server */
	PRUint32    m_port;

	char	   *m_addressCopy;
	char	   *m_addresses;
	PRUint32	m_addressesLeft;
	char	   *m_verifyAddress;
	
	SmtpAuthMethod m_authMethod;

	// message specific information
	PRInt32		m_totalAmountWritten;
	PRUint32	m_totalMessageSize;
    
	char		*m_dataBuf;
    PRUint32	 m_dataBufSize;

	PRInt32   m_originalContentLength; /* the content length at the time of calling graph progress */
	
	// initialization function given a new url and transport layer
	void Initialize(nsIURI * aURL);
	virtual nsresult ProcessProtocolState(nsIURI * url, nsIInputStream * inputStream, 
									      PRUint32 sourceOffset, PRUint32 length);

	////////////////////////////////////////////////////////////////////////////////////////
	// Communication methods --> Reading and writing protocol
	////////////////////////////////////////////////////////////////////////////////////////

	PRInt32 ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line);

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
	
	PRInt32 SendMessageInFile();

	// extract domain name from userName field in the url...
	const char * GetUserDomainName();
};

#endif  // nsSmtpProtocol_h___

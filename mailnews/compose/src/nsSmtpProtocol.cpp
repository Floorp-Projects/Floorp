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

#include "nsSmtpProtocol.h"
#include "nscore.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

#include "rosetta.h"

#include "allxpstr.h"
#include "prtime.h"
#include "prlog.h"
#include "prerror.h"
#include "nsEscape.h"

static NS_DEFINE_IID(kISmtpURLIID, NS_ISMTPURL_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIInputStreamIID, NS_IINPUTSTREAM_IID);

/* the output_buffer_size must be larger than the largest possible line
 * 2000 seems good for news
 *
 * jwz: I increased this to 4k since it must be big enough to hold the
 * entire button-bar HTML, and with the new "mailto" format, that can
 * contain arbitrarily long header fields like "references".
 *
 * fortezza: proxy auth is huge, buffer increased to 8k (sigh).
 */
#define OUTPUT_BUFFER_SIZE (4096*2)

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ADDREF(nsSmtpProtocol)
NS_IMPL_RELEASE(nsSmtpProtocol)
NS_IMPL_QUERY_INTERFACE(nsSmtpProtocol, kIStreamListenerIID); /* we need to pass in the interface ID of this interface */

nsSmtpProtocol::nsSmtpProtocol(nsIURL * aURL, nsITransport * transportLayer)
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
  Initialize(aURL, transportLayer);
}

nsSmtpProtocol::~nsSmtpProtocol()
{
	// release all of our event sinks

	// free our local state
}

void nsSmtpProtocol::Initialize(nsIURL * aURL, nsITransport * transportLayer)
{
	NS_PRECONDITION(aURL, "invalid URL passed into Smtp Protocol");

	m_flags = 0;

	// grab a reference to the transport interface
	if (transportLayer)
		NS_ADDREF(transportLayer);

	m_transport = transportLayer;

	// query the URL for a nsISmtpUrl
	m_runningURL = NULL; // initialize to NULL

	if (aURL)
	{
		nsresult rv = aURL->QueryInterface(kISmtpURLIID, (void **)&m_runningURL);
		if (NS_SUCCEEDED(rv) && m_runningURL)
		{
			// okay, now fill in our event sinks...Note that each getter ref counts before
			// it returns the interface to us...we'll release when we are done
		}
	}
	
	m_outputStream = NULL;
	m_outputConsumer = NULL;

	nsresult rv = m_transport->GetOutputStream(&m_outputStream);
	NS_ASSERTION(NS_SUCCEEDED(rv), "ooops, transport layer unable to create an output stream");
	rv = m_transport->GetOutputStreamConsumer(&m_outputConsumer);
	NS_ASSERTION(NS_SUCCEEDED(rv), "ooops, transport layer unable to provide us with an output consumer!");

	// register self as the consumer for the socket...
	rv = m_transport->SetInputStreamConsumer((nsIStreamListener *) this);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register Smtp instance as a consumer on the socket");

	const char * hostName = NULL;
	aURL->GetHost(&hostName);
	if (hostName)
		m_hostName = PL_strdup(hostName);
	else
		m_hostName = NULL;

	m_dataBuf = (char *) PR_Malloc(sizeof(char) * OUTPUT_BUFFER_SIZE);
	m_dataBufSize = OUTPUT_BUFFER_SIZE;

	m_nextState = SMTP_START_CONNECT;
	m_nextStateAfterResponse = SMTP_START_CONNECT;
	m_responseCode = 0;
	m_previousResponseCode = 0;
	m_responseText = nsnull;
	m_continuationResponse = -1; 

	m_AddressCopy = nsnull;
	m_addresses = nsnull;
	m_addressesLeft = nsnull;
	m_verifyAddress = nsnull;
	m_PostData = nsnull;	
	m_totalAmountWritten = 0;
	m_totalMessageSize = 0;

	m_originalContentLength = 0;
	m_urlInProgress = PR_FALSE;
	m_socketIsOpen = PR_FALSE;
}



/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

// Whenever data arrives from the connection, core netlib notifices the protocol by calling
// OnDataAvailable. We then read and process the incoming data from the input stream. 
NS_IMETHODIMP nsSmtpProtocol::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
	// right now, this really just means turn around and process the url
	ProcessSmtpState(aURL, aIStream, aLength);
	return NS_OK;
}

NS_IMETHODIMP nsSmtpProtocol::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
	// extract the appropriate event sinks from the url and initialize them in our protocol data
	// the URL should be queried for a nsINewsURL. If it doesn't support a news URL interface then
	// we have an error.

	return NS_OK;

}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsSmtpProtocol::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
	// what can we do? we can close the stream?
	m_urlInProgress = PR_FALSE;  // don't close the connection...we may be re-using it.
	// CloseConnection();

	// and we want to mark ourselves for deletion or some how inform our protocol manager that we are 
	// available for another url if there is one....

	return NS_OK;

}

/////////////////////////////////////////////////////////////////////////////////////////////
// End of nsIStreamListenerSupport
//////////////////////////////////////////////////////////////////////////////////////////////

PRInt32 nsSmtpProtocol::ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line)
{
	// I haven't looked into writing this yet. We have a couple of possibilities:
	// (1) insert ReadLine *yuck* into here or better yet into the nsIInputStream
	// then we can just turn around and call it here. 
	// OR
	// (2) we write "protocol" specific code for news which looks for a CRLF in the incoming
	// stream. If it finds it, that's our new line that we put into @param line. We'd
	// need a buffer (m_dataBuf) to store extra info read in from the stream.....

	// read out everything we've gotten back and return it in line...this won't work for much but it does
	// get us going...

	// XXX: please don't hold this quick "algorithm" against me. I just want to read just one
	// line for the stream. I promise this is ONLY temporary to test out NNTP. We need a generic
	// way to read one line from a stream. For now I'm going to read out one character at a time.
	// (I said it was only temporary =)) and test for newline...

	PRUint32 numBytesToRead = 0;  // MAX # bytes to read from the stream
	PRUint32 numBytesRead = 0;	  // total number bytes we have read from the stream during this call
	inputStream->GetLength(&length); // refresh the length in case it has changed...

	if (length > OUTPUT_BUFFER_SIZE)
		numBytesToRead = OUTPUT_BUFFER_SIZE;
	else
		numBytesToRead = length;

	m_dataBuf[0] = '\0';
	PRUint32 numBytesLastRead = 0;  // total number of bytes read in the last cycle...
	do
	{
		inputStream->Read(m_dataBuf, numBytesRead /* offset into m_dataBuf */, 1 /* read just one byte */, &numBytesLastRead);
		numBytesRead += numBytesLastRead;
	} while (numBytesRead <= numBytesToRead && numBytesLastRead > 0 && m_dataBuf[numBytesRead-1] != '\n');

	m_dataBuf[numBytesRead] = '\0'; // null terminate the string.

	// oops....we also want to eat up the '\n' and the \r'...
	if (numBytesRead > 1 && m_dataBuf[numBytesRead-2] == '\r')
		m_dataBuf[numBytesRead-2] = '\0'; // hit both cr and lf...
	else
		if (numBytesRead > 0 && (m_dataBuf[numBytesRead-1] == '\r' || m_dataBuf[numBytesRead-1] == '\n'))
			m_dataBuf[numBytesRead-1] = '\0';

	if (line)
		*line = m_dataBuf;
	return numBytesRead;
}

/*
 * Writes the data contained in dataBuffer into the current output stream. It also informs
 * the transport layer that this data is now available for transmission.
 * Returns a positive number for success, 0 for failure (not all the bytes were written to the
 * stream, etc). We need to make another pass through this file to install an error system (mscott)
 */

PRInt32 nsSmtpProtocol::SendData(const char * dataBuffer)
{
	PRUint32 writeCount = 0; 
	PRInt32 status = 0; 

	NS_PRECONDITION(m_outputStream && m_outputConsumer, "no registered consumer for our output");
	if (dataBuffer && m_outputStream)
	{
		nsresult rv = m_outputStream->Write(dataBuffer, 0 /* offset */, PL_strlen(dataBuffer), &writeCount);
		if (NS_SUCCEEDED(rv) && writeCount == PL_strlen(dataBuffer))
		{
			// notify the consumer that data has arrived
			// HACK ALERT: this should really be m_runningURL once we have NNTP url support...
			nsIInputStream *inputStream = NULL;
			m_outputStream->QueryInterface(kIInputStreamIID , (void **) &inputStream);
			if (inputStream)
			{
				m_outputConsumer->OnDataAvailable(m_runningURL, inputStream, writeCount);
				NS_RELEASE(inputStream);
			}
			status = 1; // mscott: we need some type of MK_OK? MK_SUCCESS? Arrgghhh
		}
		else // the write failed for some reason, returning 0 trips an error by the caller
			status = 0; // mscott: again, I really want to add an error code here!!
	}

	return status;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Begin protocol state machine functions...
//////////////////////////////////////////////////////////////////////////////////////////////

/*
 * gets the response code from the nntp server and the
 * response line
 *
 * returns the TCP return code from the read
 */
PRInt32 nsSmtpProtocol::SmtpResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char * line;
	char cont_char;
	PRInt32 status = 0;
	int err = 0;

    status = ReadLine(inputStream, buffer);

    if(status == 0)
    {
        m_nextState = SMTP_ERROR_DONE;
        ClearFlag(SMTP_PAUSE_FOR_READ);
		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_SMTP_SERVER_ERROR, m_responseTxt));
		status = MK_SMTP_SERVER_ERROR;
        return(MK_SMTP_SERVER_ERROR);
    }

    /* if TCP error of if there is not a full line yet return
     */
    if(status < 0)
	{
		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError()));
        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	}
	else if(!line)
	{
		return status;
	}

    TRACEMSG(("SMTP Rx: %s\n", line));

	cont_char = ' '; /* default */
    sscanf(line, "%d%c", &m_responseCode, &cont_char);

	if(m_continuationResponse == -1)
    {
		if (cont_char == '-')  /* begin continuation */
			m_continuationResponse = m_responseCode;

		if(PL_strlen(line) > 3)
         	StrAllocCopy(m_responseTxt, line+4);
    }
    else
    {    /* have to continue */
		if (m_continuationResponse == m_responseCode && cont_char == ' ')
			m_continuationResponse = -1;    /* ended */

        StrAllocCat(m_responseTxt, "\n");
        if(PL_strlen(line) > 3)
            StrAllocCat(m_responseTxt, line+4);
     }

	if(m_continuationResponse == -1)  /* all done with this response? */
	{
		m_nextState = m_nextStateAfterResponse;
		ClearFlag(SMTP_PAUSE_FOR_READ); /* don't pause */
	}

    return(0);  /* everything ok */
}

PRInt32 nsSmtpProtocol::LoginResponse(nsIInputStream * inputStream, PRUint32 length)
{
    PRInt32 status = 0;
	char buffer[356];

    if(m_responseCode != 220)
	{
		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER));
		return(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	}


	PR_snprintf(buffer, sizeof(buffer), "HELO %.256s" CRLF, 
				net_smtp_get_user_domain_name());
	 
	TRACEMSG(("Tx: %s", buffer));

    status = SendData(buffer);

    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_SEND_HELO_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);

    return(status);
}
    

PRInt32 nsSmtpProtocol::ExtensionLoginResponse(nsIInputStream * inputStream, PRUint32 length) 
{
    PRInt32 status = 0;
	char buffer[356];

    if(m_responseCode != 220)
	{
		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER));
		return(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	}

	PR_snprintf(buffer, sizeof(buffer), "EHLO %.256s" CRLF, 
				net_smtp_get_user_domain_name());

	TRACEMSG(("Tx: %s", buffer));

    status = SendData(buffer);

    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_SEND_EHLO_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);

    return(status);
}
    

PRInt32 nsSmtpProtocol::SendHeloResponse(nsIInputStream * inputStream, PRUint32 length)
{
    PRInt32 status = 0;
	char buffer[620];
#ifdef UNREADY_CODE
	const char *mail_add = FE_UsersMailAddress();
#else
	const char *mail_add = nsnull;
#endif

	/* don't check for a HELO response because it can be bogus and
	 * we don't care
	 */

	if(!mail_add)
	{
		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_COULD_NOT_GET_USERS_MAIL_ADDRESS));
		return(MK_COULD_NOT_GET_USERS_MAIL_ADDRESS);
	}

	if(m_verifyAddress)
	{
		PR_snprintf(buffer, sizeof(buffer), "VRFY %.256s" CRLF, m_verifyAddress);
	}
	else
	{
		/* else send the MAIL FROM: command */
		nsIMsgRFC822Parser * parser = nsnull;
		 NS_NewRFC822Parser(&parser);
		 char * s = nsnull;
		 if (parser)
		 {
			 parser->MakeFullAddress(nsnull, mail_add, s);
			 NS_RELEASE(parser);
		 }

		 if (!s)
		 {
			m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_OUT_OF_MEMORY));
			return(MK_OUT_OF_MEMORY);
		 }
		
		 if (CE_URL_S->msg_pane) 
		 {
			if (MSG_RequestForReturnReceipt(CE_URL_S->msg_pane)) 
			{
				if (CD_EHLO_DSN_ENABLED) 
				{
					PR_snprintf(buffer, sizeof(buffer), 
								"MAIL FROM:<%.256s> RET=FULL ENVID=NS40112696JT" CRLF, s);
				}
				else 
				{
#ifdef UNREADY_CODE
					FE_Alert (CE_WINDOW_ID, XP_GetString(XP_RETURN_RECEIPT_NOT_SUPPORT));
#endif
					PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, s);
				}
			}
			else if (MSG_SendingMDNInProgress(CE_URL_S->msg_pane)) 
			{
				PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, "");
			}
			else 
			{
				PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, s);
			}
		}
		else 
		{
			PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, s);
		}
		PR_FREEIF (s);
	  }

	TRACEMSG(("Tx: %s", buffer));
    status = SendData(buffer);

    m_nextState = SMTP_RESPONSE;

	if(m_verifyAddress)
    	m_nextStateAfterResponse = SMTP_SEND_VRFY_RESPONSE;
	else
    	m_nextStateAfterResponse = SMTP_SEND_MAIL_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);

    return(status);
}


PRInt32 nsSmtpProtocol::SendEhloResponse(nsIInputStream * inputStream, PRUint32 length)
{
  PRInt32 status = 0;

  if (m_responseCode != 250) 
  {
	/* EHLO not implemented */
	char buffer[384];

	HG10349
	PR_snprintf(buffer, sizeof(buffer), "HELO %.256s" CRLF, net_smtp_get_user_domain_name());

	TRACEMSG(("Tx: %s", buffer));

    status = SendData(buffer);

    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_SEND_HELO_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);
	return (status);
  }
  else 
  {
	char *ptr = NULL;
	PRBool auth_login_enabled = FALSE;

	ptr = PL_strcasestr(m_responseTxt, "DSN");
	if (ptr && nsCRT::toupper(*(ptr-1)) != 'X')
		SetFlag(SMTP_EHLO_DSN_ENABLED);
	else
		ClearFlag(SMTP_EHLO_DSN_ENABLED); 
	
	/* should we use auth login */
#ifdef UNREADY_CODE
	PREF_GetBoolPref("mail.auth_login", &auth_login_enabled);
#endif
	if (auth_login_enabled) 
	{
		/* okay user has set to use skey
		   let's see does the server have the capability */
		if (PL_strcasestr(m_responseTxt, " PLAIN") != 0)
			m_authMethod =  SMTP_AUTH_PLAIN;
		else if (strcasestr(m_responseTxt, "AUTH=LOGIN") != 0)
			m_authMethod = SMTP_AUTH_LOGIN;	/* old style */
	}

	HG40702

	HG30626

	HG16713
	{
		HG92990
	}
	
	return (status);
  }
}

HG76227

PRInt32 nsSmtpProtocol::AuthLoginResponse(nsIInputStream * stream, PRUint32 length)
{
  PRInt32 status = 0;

  switch (m_responseCode/100) 
  {
  case 2:
	  {
#ifdef UNREADY_CODE
		  char *mail_password = MSG_GetPasswordForMailHost(cd->master,  m_hostName);
#else
		char * mail_password = nsnull;
#endif
		  m_nextState = SMTP_SEND_HELO_RESPONSE;
#ifdef UNREADY_CODE
		  if (mail_password == NULL)
			MSG_SetPasswordForMailHost(cd->master, m_hostName, net_smtp_password);
#endif
		  PR_FREEIF(mail_password);
	  }
	break;
  case 3:
	m_nextState = SMTP_SEND_AUTH_LOGIN_PASSWORD;
	break;
  case 5:
  default:
	  {
		char* net_smtp_name = 0;
		char* tmp_name = 0;
#ifdef UNREADY_CODE
		PREF_CopyCharPref("mail.smtp_name", &net_smtp_name);
#endif
		PR_FREEIF(net_smtp_password);
		if (net_smtp_name)
			tmp_name = PL_strdup(net_smtp_name);
#ifdef UNREADY_CODE
        if (FE_PromptUsernameAndPassword(cur_entry->window_id,
                        NULL, &tmp_name, &net_smtp_password)) {
            m_nextState = SMTP_SEND_AUTH_LOGIN_USERNAME;
			if (tmp_name && net_smtp_name && 
				XP_STRCMP(tmp_name, net_smtp_name) != 0)
				PREF_SetCharPref("mail.smtp_name", tmp_name);
        }
        else 
#endif
		{
			/* User hit cancel, but since the client and server both say 
			 * they want auth login we're just going to return an error 
			 * and not let the msg be sent to the server
			 */
			status = MK_POP3_PASSWORD_UNDEFINED;
        }
		PR_FREEIF(net_smtp_name);
		PR_FREEIF(tmp_name);
	  }
	break;
  }
  
  return (status);
}

/* return allocated password string */
PRIVATE char *
net_smtp_prompt_for_password(ActiveEntry *cur_entry)
{
	char buffer[1024];
	char host[256];
	int len = 256;
	char *fmt = XP_GetString (XP_PASSWORD_FOR_POP3_USER);
	char *net_smtp_name = 0;
	
	nsCRT::memset(host, 0, 256);
#ifdef UNREADY_CODE
	PREF_GetCharPref("network.hosts.smtp_server", host, &len);
	PREF_CopyCharPref("mail.smtp_name", &net_smtp_name);
#endif
	PR_snprintf(buffer, sizeof (buffer), 
				fmt, net_smtp_name ? net_smtp_name : "", host);
	FREEIF(net_smtp_name);
#ifdef UNREADY_CODE
	return FE_PromptPassword(cur_entry->window_id, buffer);
#endif
}

PRInt32 nsSmtpProtocol::AuthLoginUsername()
{
  PRInt32 status = 0;
  char buffer[512];
  char *net_smtp_name = 0;
  char *base64Str = 0;

#ifdef UNREADY_CODE
  PREF_CopyCharPref("mail.smtp_name", &net_smtp_name);
  if (!net_smtp_name || !*net_smtp_name)
  {
	  PR_FREEIF(net_smtp_name);
	  return (MK_POP3_USERNAME_UNDEFINED);
  }

  if (m_authMethod == 1)
  {
	  base64Str = NET_Base64Encode(net_smtp_name,
								   PL_strlen(net_smtp_name));
  } else if (m_authMethod == 2)
  {
	  char plain_string[512];
	  int len = 1; /* first <NUL> char */
	  if (!net_smtp_password || !*net_smtp_password)
	  {
		  FREEIF(net_smtp_password);
		  net_smtp_password =  MSG_GetPasswordForMailHost(cd->master,  m_hostName);
		  if (!net_smtp_password)
			net_smtp_password = net_smtp_prompt_for_password(cur_entry);
		  if (!net_smtp_password)
			  return MK_POP3_PASSWORD_UNDEFINED;
	  }

	  nsCRT::memset(plain_string, 0, 512);
	  PR_snprintf(&plain_string[1], 510, "%s", net_smtp_name);
	  len += PL_strlen(net_smtp_name);
	  len++; /* second <NUL> char */
	  PR_snprintf(&plain_string[len], 511-len, "%s", net_smtp_password);
	  len += PL_strlen(net_smtp_password);

	  base64Str = NET_Base64Encode(plain_string, len);
  }
  if (base64Str) {
	  if (m_authMethod == SMTP_AUTH_LOGIN)
		  PR_snprintf(buffer, sizeof(buffer), "AUTH LOGIN %.256s" CRLF, base64Str);
	  else if (m_authMethod == SMTP_AUTH_PLAIN)
		  PR_snprintf(buffer, sizeof(buffer), "AUTH PLAIN %.256s" CRLF, base64Str);
	  else
		  return (MK_COMMUNICATIONS_ERROR);
	  TRACEMSG(("Tx: %s", buffer));

	  status = SendData(buffer);
	  m_nextState = SMTP_RESPONSE;
	  m_nextStateAfterResponse = SMTP_AUTH_LOGIN_RESPONSE;
	  SetFlag(SMTP_PAUSE_FOR_READ);
	  PR_FREEIF(net_smtp_name);
	  PR_FREEIF(base64Str);
	
	  return (status);
  }
#endif
  return -1;
}

PRInt32 nsSmtpProtocol::AuthLoginPassword()
{
 PRInt32 status = 0;

  /* use cached smtp password first
   * if not then use cached pop password
   * if pop password undefined 
   * sync with smtp password
   */
  
  if (!net_smtp_password || !*net_smtp_password)
  {
	  PR_FREEIF(net_smtp_password); /* in case its an empty string */
#ifdef UNREADY_CODE
	  net_smtp_password =  MSG_GetPasswordForMailHost(cd->master,  m_hostName);
#endif
  }

  if (!net_smtp_password || !*net_smtp_password) {
	  FREEIF(net_smtp_password);
	  net_smtp_password = net_smtp_prompt_for_password(cur_entry);
	  if (!net_smtp_password)
		  return MK_POP3_PASSWORD_UNDEFINED;
  }

  PR_ASSERT(net_smtp_password);
  
  if (net_smtp_password) {
	char *base64Str = NULL;
	
	base64Str = NET_Base64Encode(net_smtp_password, PL_strlen(net_smtp_password));

	if (base64Str) {
		char buffer[512];
		PR_snprintf(buffer, sizeof(buffer), "%.256s" CRLF, base64Str);
		TRACEMSG(("Tx: %s", buffer));
		
		status = SendData(buffer);
		m_nextState = SMTP_RESPONSE;
		m_nextStateAfterResponse = SMTP_AUTH_LOGIN_RESPONSE;
		SetFlag(SMTP_PAUSE_FOR_READ);
		PR_FREEIF(base64Str);

		return (status);
	}
  }

  return -1;
}

PRInt32 nsSmtpProtocol::SendVerifyResponse()
{
#if 0
	PRInt32 status = 0;
    char buffer[512];

    if(m_responseCode == 250 || m_responseCode == 251)
		return(MK_USER_VERIFIED_BY_SMTP);
	else
		return(MK_USER_NOT_VERIFIED_BY_SMTP);
#else	
	XP_ASSERT(0);
	return(-1);
#endif
}

PRInt32 nsSmtpProtocol::SendMailResponse()
{
	PRInt32 status = 0;
    char buffer[1024];

    if(m_responseCode != 250)
	{

		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_ERROR_SENDING_FROM_COMMAND), m_responseTxt);
		return(MK_ERROR_SENDING_FROM_COMMAND);  
	}

    /* Send the RCPT TO: command */
#ifdef UNREADY_CODE
	if (TestFlag(SMTP_EHLO_DSN_ENABLED) &&
		(CE_URL_S->msg_pane && 
		 MSG_RequestForReturnReceipt(CE_URL_S->msg_pane)))
#else
	if (TestFlag(SMTP_EHLO_DSN_ENABLED))
#endif
	{
		char *encodedAddress = esmtp_value_encode(m_addresses);

		if (encodedAddress) 
		{
			PR_snprintf(buffer, sizeof(buffer), "RCPT TO:<%.256s> NOTIFY=SUCCESS,FAILURE ORCPT=rfc822;%.500s" CRLF, 
				m_addresses, encodedAddress);
			PR_FREEIF(encodedAddress);
		}
		else 
		{
			status = MK_OUT_OF_MEMORY;
			return (status);
		}
	}
	else
	{
	    PR_snprintf(buffer, sizeof(buffer), "RCPT TO:<%.256s>" CRLF, m_addresses);
	}
	/* take the address we sent off the list (move the pointer to just
	   past the terminating null.) */
	m_addresses += PL_strlen (m_addresses) + 1;
	m_addressesLeft--;

	TRACEMSG(("Tx: %s", buffer));
    
    status = SendData(buffer);

    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_SEND_RCPT_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);

    return(status);
}

PRInt32 nsSmtpProtocol::SendRecipientResponse()
{
    PRInt32 status = 0;
    char buffer[16];

	if(m_responseCode != 250 && m_responseCode != 251)
	{
		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_ERROR_SENDING_RCPT_COMMAND), m_responseTxt);
        return(MK_ERROR_SENDING_RCPT_COMMAND);
	}

	if(m_addressesLeft > 0)
	{
		/* more senders to RCPT to 
		 */
        m_nextState = SMTP_SEND_MAIL_RESPONSE; 
		return(0);
	}

    /* else send the RCPT TO: command */
    PL_strcpy(buffer, "DATA" CRLF);

	TRACEMSG(("Tx: %s", buffer));
        
    status = SendData(buffer);   

    m_nextState = SMTP_RESPONSE;  
    m_nextStateAfterResponse = SMTP_SEND_DATA_RESPONSE; 
    SetFlag(SMTP_PAUSE_FOR_READ);   

    return(status);  
}

PRInt32 nsSmtpProtocol::SendDataResponse()
{
    PRInt32 status = 0;
    char * command=0;   

    if(m_responseCode != 354)
	{
		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_ERROR_SENDING_DATA_COMMAND), 
			m_responseTxt ? m_responseTxt : "");
        return(MK_ERROR_SENDING_DATA_COMMAND);
	}
#ifdef UNREADY_CODE
#ifdef XP_UNIX
	{
	  const char * FE_UsersRealMailAddress(void); /* definition */
	  const char *real_name;
	  char *s = 0;
	  XP_Bool suppress_sender_header = FALSE;

	  PREF_GetBoolPref ("mail.suppress_sender_header", &suppress_sender_header);
	  if (!suppress_sender_header)
	    {
	      real_name =  FE_UsersRealMailAddress();
	      s = (real_name ? MSG_MakeFullAddress (NULL, real_name) : 0);
	      if (real_name && !s)
		{
		  CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_COULD_NOT_GET_UID);
          return(MK_COULD_NOT_GET_UID);
		}
	      if(real_name)
		{
		  char buffer[512];
		  PR_snprintf(buffer, sizeof(buffer), "Sender: %.256s" CRLF, real_name);
		  StrAllocCat(command, buffer);
		  if(!command)
		    {
		      CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
		      return(MK_OUT_OF_MEMORY);
		    }
	      
	      TRACEMSG(("sending extra unix header: %s", command));
	      
	      status = (int) NET_BlockingWrite(CE_SOCK, command, PL_strlen(command));   
	      if(status < 0)
		{
		  TRACEMSG(("Error sending message"));
		}
        }
	    }
	}
#endif /* XP_UNIX */
#endif /* UNREADY_CODE */

	PR_FREEIF(command);

    m_nextState = SMTP_SEND_POST_DATA;
    ClearFlag(SMTP_PAUSE_FOR_READ);   /* send data directly */

#ifdef UNREADY_CODE
	NET_Progress(CE_WINDOW_ID, XP_GetString(MK_MSG_DELIV_MAIL));
#endif

#ifdef UNREADY_CODE
	/* get the size of the message */
	if(CE_URL_S->post_data_is_file)
	  {
		XP_StatStruct stat_entry;

		if(-1 != XP_Stat(CE_URL_S->post_data,
                         &stat_entry,
                         xpFileToPost))
			m_totalMessageSize = stat_entry.st_size;
	  }
	else
	  {
			m_totalMessageSize = CE_URL_S->post_data_size;
	  }
#endif

    return(status);  
}

PRInt32 nsSmtpProtocol::SendPostData()
{
    PRInt32 status = 0;
	unsigned long curtime;

	/* returns 0 on done and negative on error
	 * positive if it needs to continue.
	 */

	SendData(m_MessageToPost);
								  
	cd->pause_for_read = TRUE;

	if(status == 0)
	{
		/* normal done
		 */
        PL_strcpy(m_dataBuf, CRLF "." CRLF);
        TRACEMSG(("sending %s", cd->data_buffer));
		SendData(m_dataBuf);
#ifdef UNREADY_CODE
		NET_Progress(CE_WINDOW_ID,
					XP_GetString(XP_MESSAGE_SENT_WAITING_MAIL_REPLY));
#endif
        m_nextState = SMTP_RESPONSE;
        m_nextStateAfterResponse = SMTP_SEND_MESSAGE_RESPONSE;
        return(0);
	}

	m_totalAmountWritten += status;

	/* Update the thermo and the status bar.  This is done by hand, rather
	   than using the FE_GraphProgress* functions, because there seems to be
	   no way to make FE_GraphProgress shut up and not display anything more
	   when all the data has arrived.  At the end, we want to show the
	   "message sent; waiting for reply" status; FE_GraphProgress gets in
	   the way of that.  See bug #23414. */

#ifdef UNREADY_CODE
	curtime = XP_TIME();
	if (curtime != m_LastTime) {
		FE_Progress(CE_WINDOW_ID, XP_ProgressText(m_totalMessageSize,
												  m_totalAmountWritten,
												  0, 0));
		m_LastTime = curtime;
	}

	if(m_totalMessageSize)
		FE_SetProgressBarPercent(CE_WINDOW_ID,
						  	 m_totalAmountWritten*100/m_totalMessageSize);
#endif

    return(status);
}



PRInt32 nsSmtpProtocol::SendMessageResponse()
{
    PRInt32 status = 0;

    if(m_responseCode != 250)
	{
		m_runningURL->SetErrorMessage(NET_ExplainErrorDetails(MK_ERROR_SENDING_MESSAGE), m_responseTxt ? m_responseTxt : "");
        return(MK_ERROR_SENDING_MESSAGE);
	}

#ifdef UNREADY_CODE
	NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_MAILSENT));
#endif
	/* else */
	m_nextState = SMTP_DONE;
	return(MK_NO_DATA);
}


PRInt32 LoadUrl(nsIUrl * aURL)
{
	nsresult rv = NS_OK;
    PRInt32 status = 0; 
	int32 pref = 0;
	m_continuationResponse = -1;  /* init */
	nsISmtpUrl * smtpUrl = nsnull;
	HG77067

	if (aURL)
	{
		rv = aURL->QueryInterface(kISmtpURLIID, (void **) &smtpUrl);
		if (NS_SUCCEEDED(rv) && smtpUrl)
		{

#ifdef UNREADY_CODE
	m_hostName = PL_strdup(NET_MailRelayHost(cur_entry->window_id));
#endif
	// we should be able to get the host name from the url...

	/* make a copy of the address
	 */
	if(CE_URL_S->method == URL_POST_METHOD)
	{
		int status=0;
		char *addrs1 = 0;
		char *addrs2 = 0;
    	m_nextState = SMTP_START_CONNECT;

		/* Remove duplicates from the list, to prevent people from getting
		   more than one copy (the SMTP host may do this too, or it may not.)
		   This causes the address list to be parsed twice; this probably
		   doesn't matter.
		 */
		addrs1 = MSG_RemoveDuplicateAddresses (CE_URL_S->address+7, 0, FALSE /*removeAliasesToMe*/);

		/* Extract just the mailboxes from the full RFC822 address list.
		   This means that people can post to mailto: URLs which contain
		   full RFC822 address specs, and we will still send the right
		   thing in the SMTP RCPT command.
		 */
		if (addrs1 && *addrs1)
		{
			status = MSG_ParseRFC822Addresses (addrs1, 0, &addrs2);
			FREEIF (addrs1);
		}

		if (status < 0) return status;

		if (status == 0 || addrs2 == 0)
		  {
			m_nextState = SMTP_ERROR_DONE;
			ClearFlag(SMTP_PAUSE_FOR_READ);
			status = MK_MIME_NO_RECIPIENTS;
			CE_URL_S->error_msg = NET_ExplainErrorDetails(status);
			return status;
		  }

		CD_ADDRESS_COPY = addrs2;
		CD_MAIL_TO_ADDRESS_PTR = CD_ADDRESS_COPY;
		CD_MAIL_TO_ADDRESSES_LEFT = status;
        return(NET_ProcessMailto(cur_entry));
	}
	else
	{
		/* parse special headers and stuff from the search data in the
		   URL address.  This data is of the form

		    mailto:TO_FIELD?FIELD1=VALUE1&FIELD2=VALUE2

		   where TO_FIELD may be empty, VALUEn may (for now) only be
		   one of "cc", "bcc", "subject", "newsgroups", "references",
		   and "attachment".

		   "to" is allowed as a field/value pair as well, for consistency.
		   */
		HG15779
		/*
		   Each parameter may appear only once, but the order doesn't
		   matter.  All values must be URL-encoded.
		 */
		char *from = 0;						/* internal only */
		char *reply_to = 0;					/* internal only */
		char *to = 0;
		char *cc = 0;
		char *bcc = 0;
		char *fcc = 0;						/* internal only */
		char *newsgroups = 0;
		char *followup_to = 0;
		char *html_part = 0;				/* internal only */
		char *organization = 0;				/* internal only */
		char *subject = 0;
		char *references = 0;
		char *attachment = 0;				/* internal only */
		char *body = 0;
		char *priority = 0;
		char *newshost = 0;					/* internal only */
		HG27293

		char *newspost_url = 0;
		PRBool forcePlaintText = PR_FALSE;

		// extract info from the url...


		if(newshost)
		  {
			char *prefix = "news://";
			char *slash = XP_STRRCHR (newshost, '/');
			if (slash && HG50707)
			  {
				*slash = 0;
				prefix = "snews://";
			  }
			newspost_url = (char *) XP_ALLOC (PL_strlen (prefix) +
											  PL_strlen (newshost) + 10);
			if (newspost_url)
			  {
				XP_STRCPY (newspost_url, prefix);
				XP_STRCAT (newspost_url, newshost);
				XP_STRCAT (newspost_url, "/");
			  }
		  }

		/* Tell the message library and front end to pop up an edit window.
		 */
		cpane = MSG_ComposeMessage (CE_WINDOW_ID,
									from, reply_to, to, cc, bcc, fcc,
									newsgroups, followup_to, organization,
									subject, references, other_random_headers,
									priority, attachment, newspost_url, body,
									HG18517, HG74130, force_plain_text,
									html_part);

		if (cpane && CE_URL_S->fe_data) {
			/* Tell libmsg what to do after deliver the message */
			MSG_SetPostDeliveryActionInfo (cpane, CE_URL_S->fe_data);
		}
		else if (cpane)
		{
			MWContext *context = MSG_GetContext(cpane);
			INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);

			/* Avoid a mailtourl defaults as an unicode mail. 
			 * Set a encoding menu's charsetID of the current context for that case.
			 */
			if (IS_UNICODE_CSID(INTL_GetCSIDocCSID(csi)) || 
				IS_UNICODE_CSID(INTL_GetCSIWinCSID(csi)))
			{
				INTL_ResetCharSetID(context, FE_DefaultDocCharSetID((MWContext *) CE_WINDOW_ID));
			}
		}

		FREEIF(from);
		FREEIF(reply_to);
		FREEIF(to);
		FREEIF(cc);
		FREEIF(bcc);
		FREEIF(fcc);
		FREEIF(newsgroups);
		FREEIF(followup_to);
		FREEIF(html_part);
		FREEIF(organization);
		FREEIF(subject);
		FREEIF(references);
		FREEIF(attachment);
		FREEIF(body);
		FREEIF(other_random_headers);
		FREEIF(newshost);
		FREEIF(priority);
		FREEIF(newspost_url);

		status = MK_NO_DATA;
		PR_FREEIF(cd);	/* no one else is gonna do it! */
		return(-1);
	  }


}
/*
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
PRInt32	ProcessSmtpState(nsIURL * url, nsIInputStream * inputStream, PRUint32 length););
{
    PRInt32 status = 0;
	char	*mail_relay_host;

    TRACEMSG(("Entering NET_ProcessSMTP"));

    ClearFlag(SMTP_PAUSE_FOR_READ); /* already paused; reset */

    while(!CD_PAUSE_FOR_READ)
      {

		TRACEMSG(("In NET_ProcessSMTP with state: %d", m_nextState));

        switch(m_nextState) {

		case SMTP_RESPONSE:
			net_smtp_response (cur_entry);
			break;

        case SMTP_START_CONNECT:
			mail_relay_host = NET_MailRelayHost(CE_WINDOW_ID);
			if (PL_strlen(mail_relay_host) == 0)
			{
				status = MK_MSG_NO_SMTP_HOST;
				break;
			}
            status = NET_BeginConnect(NET_MailRelayHost(CE_WINDOW_ID), 
										NULL,
										"SMTP",
										SMTP_PORT, 
										&CE_SOCK, 
										FALSE, 
										&CD_TCP_CON_DATA, 
										CE_WINDOW_ID,
										&CE_URL_S->error_msg,
										 cur_entry->socks_host,
										 cur_entry->socks_port,
										 0);
            SetFlag(SMTP_PAUSE_FOR_READ);
            if(status == MK_CONNECTED)
              {
                m_nextState = SMTP_RESPONSE;
                m_nextStateAfterResponse = SMTP_EXTN_LOGIN_RESPONSE;
                FE_SetReadSelect(CE_WINDOW_ID, CE_SOCK);
              }
            else if(status > -1)
              {
                CE_CON_SOCK = CE_SOCK;  /* set con_sock so we can select on it */
                FE_SetConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                m_nextState = SMTP_FINISH_CONNECT;
              }
            break;

		case SMTP_FINISH_CONNECT:
            status = NET_FinishConnect(NET_MailRelayHost(CE_WINDOW_ID), 
										  "SMTP", 
										  SMTP_PORT, 
										  &CE_SOCK, 
										  &CD_TCP_CON_DATA, 
										  CE_WINDOW_ID,
										  &CE_URL_S->error_msg,
										  0);

            SetFlag(SMTP_PAUSE_FOR_READ);
            if(status == MK_CONNECTED)
              {
                m_nextState = SMTP_RESPONSE;
                m_nextStateAfterResponse = SMTP_EXTN_LOGIN_RESPONSE;
                FE_ClearConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                CE_CON_SOCK = SOCKET_INVALID;  /* reset con_sock so we don't select on it */
                FE_SetReadSelect(CE_WINDOW_ID, CE_SOCK);
              }
			else
              {
                /* unregister the old CE_SOCK from the select list
                 * and register the new value in the case that it changes
                 */
                if(CE_CON_SOCK != CE_SOCK)
                  {
                    FE_ClearConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                    CE_CON_SOCK = CE_SOCK;
                    FE_SetConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                  }
              }
            break;

	   HG26788
	   
	   case SMTP_LOGIN_RESPONSE:
            status = net_smtp_login_response(cur_entry);
            break;

       case SMTP_EXTN_LOGIN_RESPONSE:
            status = net_smtp_extension_login_response(cur_entry);
            break;

	   case SMTP_SEND_HELO_RESPONSE:
            status = net_smtp_send_helo_response(cur_entry);
            break;

	   case SMTP_SEND_EHLO_RESPONSE:
		    status = net_smtp_send_ehlo_response(cur_entry);
			break;

	   case SMTP_AUTH_LOGIN_RESPONSE:
		    status = net_smtp_auth_login_response(cur_entry);
			break;

       HG12690
			
	   case SMTP_SEND_AUTH_LOGIN_USERNAME:
		    status = net_smtp_auth_login_username(cur_entry);
			break;

	   case SMTP_SEND_AUTH_LOGIN_PASSWORD:
		    status = net_smtp_auth_login_password(cur_entry);
			break;
			
	   case SMTP_SEND_VRFY_RESPONSE:
			status = net_smtp_send_vrfy_response(cur_entry);
            break;
			
	   case SMTP_SEND_MAIL_RESPONSE:
            status = net_smtp_send_mail_response(cur_entry);
            break;
			
	   case SMTP_SEND_RCPT_RESPONSE:
            status = net_smtp_send_rcpt_response(cur_entry);
            break;
			
	   case SMTP_SEND_DATA_RESPONSE:
            status = net_smtp_send_data_response(cur_entry);
            break;
			
	   case SMTP_SEND_POST_DATA:
            status = net_smtp_send_post_data(cur_entry);
            break;
			
	   case SMTP_SEND_MESSAGE_RESPONSE:
            status = net_smtp_send_message_response(cur_entry);
            break;
        
        case SMTP_DONE:
			NET_BlockingWrite(CE_SOCK, "QUIT" CRLF, 6);
            FE_ClearReadSelect(CE_WINDOW_ID, CE_SOCK);
			net_graceful_shutdown(CE_SOCK, HG06474);
            NETCLOSE(CE_SOCK);
            m_nextState = SMTP_FREE;
            break;
        
        case SMTP_ERROR_DONE:
            if(CE_SOCK != SOCKET_INVALID)
              {
				if ( HG70187 )
					NET_BlockingWrite(CE_SOCK, "QUIT" CRLF, 6);
				FE_ClearReadSelect(CE_WINDOW_ID, CE_SOCK);
				if ( HG70187 )
					net_graceful_shutdown(CE_SOCK, HG06474);
				FE_ClearConnectSelect(CE_WINDOW_ID, CE_SOCK);
#ifdef XP_WIN
				if(cd->calling_netlib_all_the_time)
				{
					net_call_all_the_time_count--;
					cd->calling_netlib_all_the_time = FALSE;
					if(net_call_all_the_time_count == 0)
						FE_ClearCallNetlibAllTheTime(CE_WINDOW_ID);
				}
#endif /* XP_WIN */
#if defined(XP_WIN) || (defined(XP_UNIX)&&defined(UNIX_ASYNC_DNS))
                FE_ClearDNSSelect(CE_WINDOW_ID, CE_SOCK);
#endif /* XP_WIN || XP_UNIX */
                NETCLOSE(CE_SOCK);
              }
            m_nextState = SMTP_FREE;
            break;
        
        case SMTP_FREE:
            FREEIF(m_dataBuf);
            FREEIF(CD_ADDRESS_COPY);
			FREEIF(m_responseTxt);
			FREEIF(m_hostName);
            if(CD_TCP_CON_DATA)
                NET_FreeTCPConData(CD_TCP_CON_DATA);
			if (cd->write_post_data_data)
				net_free_write_post_data_object((struct WritePostDataData *) 
												cd->write_post_data_data);
            FREE(cd);

            return(-1); /* final end */
        
        default: /* should never happen !!! */
            TRACEMSG(("SMTP: BAD STATE!"));
            m_nextState = SMTP_ERROR_DONE;
            break;
        }

        /* check for errors during load and call error 
         * state if found
         */
        if(status < 0 && m_nextState != SMTP_FREE)
          {
            m_nextState = SMTP_ERROR_DONE;
            /* don't exit! loop around again and do the free case */
            ClearFlag(SMTP_PAUSE_FOR_READ);
          }
      } /* while(!CD_PAUSE_FOR_READ) */
    
    return(status);
}

static void
MessageSendingDone(URL_Struct* url, int status, MWContext* context)
{
    if (status < 0) {
		char *error_msg = NET_ExplainErrorDetails(status, 0, 0, 0, 0);
		if (error_msg) {
			FE_Alert(context, error_msg);
		}
		PR_FREEIF(error_msg);
    }
    if (url->post_data) {
		PR_FREEIF(url->post_data);
		url->post_data = NULL;
    }
    if (url->post_headers) {
		PR_FREEIF(url->post_headers);
		url->post_headers = NULL;
    }
    NET_FreeURLStruct(url);
}



int
NET_SendMessageUnattended(MWContext* context, char* to, char* subject,
						  char* otherheaders, char* body)
{
    char* urlstring;
    URL_Struct* url;
    int16 win_csid;
    int16 csid;
    char* convto;
    char* convsub;

    win_csid = INTL_DefaultWinCharSetID(context);
    csid = INTL_DefaultMailCharSetID(win_csid);
    convto = IntlEncodeMimePartIIStr(to, csid, TRUE);
    convsub = IntlEncodeMimePartIIStr(subject, csid, TRUE);

    urlstring = PR_smprintf("mailto:%s", convto ? convto : to);

    if (!urlstring) return MK_OUT_OF_MEMORY;
    url = NET_CreateURLStruct(urlstring, NET_DONT_RELOAD);
    PR_FREEIF(urlstring);
    if (!url) return MK_OUT_OF_MEMORY;

    

    url->post_headers = PR_smprintf("To: %s\n\
Subject: %s\n\
%s\n",
								 convto ? convto : to,
								 convsub ? convsub : subject,
								 otherheaders);
    if (convto) PR_FREEIF(convto);
    if (convsub) PR_FREEIF(convsub);
    if (!url->post_headers) return MK_OUT_OF_MEMORY;
	url->post_data = PL_strdup(body);
	if (!url->post_data) return MK_OUT_OF_MEMORY;
    url->post_data_size = PL_strlen(url->post_data);
    url->post_data_is_file = FALSE;
    url->method = URL_POST_METHOD;
    url->internal_url = TRUE;
    return NET_GetURL(url, FO_PRESENT, context, MessageSendingDone);
    
}
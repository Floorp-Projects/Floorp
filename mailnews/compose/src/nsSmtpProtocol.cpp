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
#include "nsMsgTransition.h" // need this to get MK_ defines...
#include "nsSmtpProtocol.h"
#include "nscore.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIMsgHeaderParser.h"
#include "nsFileStream.h"
#include "nsIMsgMailNewsUrl.h"

#include "nsMsgBaseCID.h"

#include "prtime.h"
#include "prlog.h"
#include "prerror.h"
#include "prprf.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);

extern "C" 
{
	char * NET_SACopy (char **destination, const char *source);
	char * NET_SACat (char **destination, const char *source);
}

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

////////////////////////////////////////////////////////////////////////////////////////////
// TEMPORARY HARD CODED FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////
#ifdef XP_WIN
char *XP_AppCodeName = "Mozilla";
#else
const char *XP_AppCodeName = "Mozilla";
#endif
#define NET_IS_SPACE(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isspace(x))

/*
 * This function takes an error code and associated error data
 * and creates a string containing a textual description of
 * what the error is and why it happened.
 *
 * The returned string is allocated and thus should be freed
 * once it has been used.
 *
 * This function is defined in mkmessag.c.
 */
char * NET_ExplainErrorDetails (int code, ...)
{
	char * rv = PR_smprintf("%s", "Error descriptions not implemented yet");
	return rv;
}

char * NET_SACopy (char **destination, const char *source)
{
	if(*destination)
	  {
	    PR_Free(*destination);
		*destination = 0;
	  }
    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
        if (*destination == NULL) 
 	        return(NULL);

        PL_strcpy (*destination, source);
      }
    return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
char * NET_SACat (char **destination, const char *source)
{
    if (source && *source)
      {
        if (*destination)
          {
            int length = PL_strlen (*destination);
            *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
            if (*destination == NULL)
            return(NULL);

            PL_strcpy (*destination + length, source);
          }
        else
          {
            *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
            if (*destination == NULL)
                return(NULL);

             PL_strcpy (*destination, source);
          }
      }
    return *destination;
}

/* RFC 1891 -- extended smtp value encoding scheme

  5. Additional parameters for RCPT and MAIL commands

     The extended RCPT and MAIL commands are issued by a client when it wishes to request a DSN from the
     server, under certain conditions, for a particular recipient. The extended RCPT and MAIL commands are
     identical to the RCPT and MAIL commands defined in [1], except that one or more of the following parameters
     appear after the sender or recipient address, respectively. The general syntax for extended SMTP commands is
     defined in [4]. 

     NOTE: Although RFC 822 ABNF is used to describe the syntax of these parameters, they are not, in the
     language of that document, "structured field bodies". Therefore, while parentheses MAY appear within an
     emstp-value, they are not recognized as comment delimiters. 

     The syntax for "esmtp-value" in [4] does not allow SP, "=", control characters, or characters outside the
     traditional ASCII range of 1- 127 decimal to be transmitted in an esmtp-value. Because the ENVID and
     ORCPT parameters may need to convey values outside this range, the esmtp-values for these parameters are
     encoded as "xtext". "xtext" is formally defined as follows: 

     xtext = *( xchar / hexchar ) 

     xchar = any ASCII CHAR between "!" (33) and "~" (126) inclusive, except for "+" and "=". 

	; "hexchar"s are intended to encode octets that cannot appear
	; as ASCII characters within an esmtp-value.

		 hexchar = ASCII "+" immediately followed by two upper case hexadecimal digits 

	When encoding an octet sequence as xtext:

	+ Any ASCII CHAR between "!" and "~" inclusive, except for "+" and "=",
		 MAY be encoded as itself. (A CHAR in this range MAY instead be encoded as a "hexchar", at the
		 implementor's discretion.) 

	+ ASCII CHARs that fall outside the range above must be encoded as
		 "hexchar". 

 */
/* caller must free the return buffer */
PRIVATE char *
esmtp_value_encode(char *addr)
{
	char *buffer = (char *) PR_Malloc(512); /* esmpt ORCPT allow up to 500 chars encoded addresses */
	char *bp = buffer, *bpEnd = buffer+500;
	int len, i;

	if (!buffer) return NULL;

	*bp=0;
	if (! addr || *addr == 0) /* this will never happen */
		return buffer;

	for (i=0, len=PL_strlen(addr); i < len && bp < bpEnd; i++)
	{
		if (*addr >= 0x21 && 
			*addr <= 0x7E &&
			*addr != '+' &&
			*addr != '=')
		{
			*bp++ = *addr++;
		}
		else
		{
			PR_snprintf(bp, bpEnd-bp, "+%.2X", ((int)*addr++));
			bp += PL_strlen(bp);
		}
	}
	*bp=0;
	return buffer;
}

////////////////////////////////////////////////////////////////////////////////////////////
// END OF TEMPORARY HARD CODED FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////

nsSmtpProtocol::nsSmtpProtocol(nsIURI * aURL)
{
  Initialize(aURL);
}

nsSmtpProtocol::~nsSmtpProtocol()
{
	// free our local state
	PR_FREEIF(m_addressCopy);
	PR_FREEIF(m_verifyAddress);
	PR_FREEIF(m_dataBuf);
}

void nsSmtpProtocol::Initialize(nsIURI * aURL)
{
	NS_PRECONDITION(aURL, "invalid URL passed into Smtp Protocol");
	nsresult rv = NS_OK;

	m_flags = 0;
	m_port = SMTP_PORT;

	if (aURL)
		m_runningURL = do_QueryInterface(aURL);

	// call base class to set up the url 
	rv = OpenNetworkSocket(aURL);
	
	m_dataBuf = (char *) PR_Malloc(sizeof(char) * OUTPUT_BUFFER_SIZE);
	m_dataBufSize = OUTPUT_BUFFER_SIZE;

	m_nextState = SMTP_START_CONNECT;
	m_nextStateAfterResponse = SMTP_START_CONNECT;
	m_responseCode = 0;
	m_previousResponseCode = 0;
	m_continuationResponse = -1; 
	m_authMethod = SMTP_AUTH_NONE;

	m_addressCopy = nsnull;
	m_addresses = nsnull;
	m_addressesLeft = nsnull;
	m_verifyAddress = nsnull;	
	m_totalAmountWritten = 0;
	m_totalMessageSize = 0;

	m_originalContentLength = 0;
}

const char * nsSmtpProtocol::GetUserDomainName()
{
	NS_PRECONDITION(m_runningURL, "we must be running a url in order to get the user's domain...");

	if (m_runningURL)
	{
		const char *atSignMarker = nsnull;
		m_runningURL->GetUserEmailAddress(getter_Copies(m_mailAddr));
		if (m_mailAddr)
		{
			atSignMarker = PL_strchr(m_mailAddr, '@');
			return atSignMarker ? atSignMarker+1 :  (const char *) m_mailAddr;  // return const ptr into buffer in running url...
		}
	}

	return nsnull;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsSmtpProtocol::OnStopRequest(nsIChannel * /* aChannel */, nsISupports *ctxt, nsresult aStatus, const PRUnichar *aMsg)
{
	nsMsgProtocol::OnStopRequest(nsnull, ctxt, aStatus, aMsg);

	// okay, we've been told that the send is done and the connection is going away. So 
	// we need to release all of our state
	return CloseSocket();
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
		inputStream->Read(m_dataBuf + numBytesRead, 1 /* read just one byte */, &numBytesLastRead);
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
	char * line = nsnull;
	char cont_char;
	PRInt32 status = 0;

    status = ReadLine(inputStream, length, &line);

    if(status == 0)
    {
        m_nextState = SMTP_ERROR_DONE;
        ClearFlag(SMTP_PAUSE_FOR_READ);

		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_SMTP_SERVER_ERROR, m_responseText));
		status = MK_SMTP_SERVER_ERROR;
        return(MK_SMTP_SERVER_ERROR);
    }

    /* if TCP error of if there is not a full line yet return
     */
    if(status < 0)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError()));
        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	}
	else if(!line)
	{
		return status;
	}

	cont_char = ' '; /* default */
    sscanf(line, "%d%c", &m_responseCode, &cont_char);

	if(m_continuationResponse == -1)
    {
		if (cont_char == '-')  /* begin continuation */
			m_continuationResponse = m_responseCode;

		if(PL_strlen(line) > 3)
			m_responseText = line+4;
    }
    else
    {    /* have to continue */
		if (m_continuationResponse == m_responseCode && cont_char == ' ')
			m_continuationResponse = -1;    /* ended */

		m_responseText += "\n";
        if(PL_strlen(line) > 3)
			m_responseText += line+4;
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
	nsCAutoString buffer ("HELO ");

    if(m_responseCode != 220)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER));
		return(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	}

	buffer += GetUserDomainName();
	buffer += CRLF;
	
	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    status = SendData(url, buffer);

    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_SEND_HELO_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);

    return(status);
}
    

PRInt32 nsSmtpProtocol::ExtensionLoginResponse(nsIInputStream * inputStream, PRUint32 length) 
{
    PRInt32 status = 0;
	nsCAutoString buffer("EHLO ");

    if(m_responseCode != 220)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER));
		return(MK_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	}

	buffer += GetUserDomainName();
	buffer += CRLF;

	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    status = SendData(url, buffer);

    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_SEND_EHLO_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);

    return(status);
}
    

PRInt32 nsSmtpProtocol::SendHeloResponse(nsIInputStream * inputStream, PRUint32 length)
{
    PRInt32 status = 0;
	nsCAutoString buffer;

	// extract the email addresss
	nsXPIDLCString userAddress;
	m_runningURL->GetUserEmailAddress(getter_Copies(userAddress));

	/* don't check for a HELO response because it can be bogus and
	 * we don't care
	 */

	if(!userAddress)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_COULD_NOT_GET_USERS_MAIL_ADDRESS));
		return(MK_COULD_NOT_GET_USERS_MAIL_ADDRESS);
	}

	if(m_verifyAddress)
	{
		buffer += "VRFY";
		buffer += m_verifyAddress;
		buffer += CRLF;
	}
	else
	{
		/* else send the MAIL FROM: command */
		nsCOMPtr<nsIMsgHeaderParser> parser;
         nsComponentManager::CreateInstance(kHeaderParserCID,
                                            nsnull,
                                            nsCOMTypeInfo<nsIMsgHeaderParser>::GetIID(),
                                            getter_AddRefs(parser));

		 char * s = nsnull;
		 if (parser)
			 parser->MakeFullAddress(nsnull, nsnull, userAddress, &s);
		 if (!s)
		 {
			nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
			url->SetErrorMessage(NET_ExplainErrorDetails(MK_OUT_OF_MEMORY));
			return(MK_OUT_OF_MEMORY);
		 }
#ifdef UNREADY_CODE		
		 if (CE_URL_S->msg_pane) 
		 {
			if (MSG_RequestForReturnReceipt(CE_URL_S->msg_pane)) 
			{
				if (TestFlag(SMTP_EHLO_DSN_ENABLED)) 
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
#ifdef UNREADY_CODE
			else if (MSG_SendingMDNInProgress(CE_URL_S->msg_pane)) 
			{
				PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, "");
			}
#endif
			else 
			{
				PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, s);
			}
		}
		else 
#endif
		{
			buffer = "MAIL FROM:<";
			buffer += s;
			buffer += ">" CRLF;
		}
		PR_FREEIF (s);
	  }

	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    status = SendData(url, buffer);

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
  nsCAutoString buffer;

  if (m_responseCode != 250) 
  {
	HG10349
	buffer = "HELO ";
	buffer += GetUserDomainName();
	buffer += CRLF;

	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    status = SendData(url, buffer);

    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_SEND_HELO_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);
	return (status);
  }
  else 
  {
	char *ptr = NULL;
	PRBool auth_login_enabled = PR_FALSE;

	ptr = PL_strcasestr(m_responseText, "DSN");
	if (ptr && nsCRT::ToUpper(*(ptr-1)) != 'X')
	{
		// temporary hack to disable return receipts until we have a preference to handle it...
		SetFlag(SMTP_EHLO_DSN_ENABLED);
		ClearFlag(SMTP_EHLO_DSN_ENABLED); 
	}
	else
		ClearFlag(SMTP_EHLO_DSN_ENABLED); 
	
	/* should we use auth login */
#ifdef UNREADY_CODE
	PREF_GetBoolPref("mail.auth_login", &auth_login_enabled);
#else
	auth_login_enabled = PR_FALSE;
#endif
	if (auth_login_enabled) 
	{
		/* okay user has set to use skey
		   let's see does the server have the capability */
		if (PL_strcasestr(m_responseText, " PLAIN") != 0)
			m_authMethod =  SMTP_AUTH_PLAIN;
		else if (PL_strcasestr(m_responseText, "AUTH=LOGIN") != 0)
			m_authMethod = SMTP_AUTH_LOGIN;	/* old style */
	}
#ifdef UNREADY_CODE
	HG40702

	HG30626

	HG16713
	{
		HG92990
	}
#endif

	if (m_authMethod)
	{
		m_nextState = SMTP_SEND_AUTH_LOGIN_USERNAME;
		m_nextStateAfterResponse = SMTP_AUTH_LOGIN_RESPONSE;
	}
	else 
		m_nextState = SMTP_SEND_HELO_RESPONSE;
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
		  //m_runningURL->GetUserPassword(&mailPassword);
		  m_nextState = SMTP_SEND_HELO_RESPONSE;
#ifdef UNREADY_CODE
		  const nsString * mailPassword = nsnull;
		  if (mailPassword == NULL)
			MSG_SetPasswordForMailHost(cd->master, m_hostName, net_smtp_password);
#endif
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
		PR_FREEIF(net_smtp_password);
#endif
		if (net_smtp_name)
			tmp_name = PL_strdup(net_smtp_name);
#ifdef UNREADY_CODE
        if (FE_PromptUsernameAndPassword(cur_entry->window_id,
                        NULL, &tmp_name, &net_smtp_password)) {
            m_nextState = SMTP_SEND_AUTH_LOGIN_USERNAME;
			if (tmp_name && net_smtp_name && 
				PL_strcmp(tmp_name, net_smtp_name) != 0)
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


PRInt32 nsSmtpProtocol::AuthLoginUsername()
{

#ifdef UNREADY_CODE
  char buffer[512];
  PRInt32 status = 0;
  char *net_smtp_name = 0;
  char *base64Str = 0;
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
  } 
  else if (m_authMethod == 2)
  {
	  char plain_string[512];
	  int len = 1; /* first <NUL> char */
	  if (!net_smtp_password || !*net_smtp_password)
	  {
		  PR_FREEIF(net_smtp_password);
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
	
	  nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
	  status = SendData(url, buffer);
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

  /* use cached smtp password first
   * if not then use cached pop password
   * if pop password undefined 
   * sync with smtp password
   */
#ifdef UNREADY_CODE  
  PRInt32 status = 0;
  if (!net_smtp_password || !*net_smtp_password)
  {
	  PR_FREEIF(net_smtp_password); /* in case its an empty string */
#ifdef UNREADY_CODE
	  net_smtp_password =  MSG_GetPasswordForMailHost(cd->master,  m_hostName);
#endif
  }

  if (!net_smtp_password || !*net_smtp_password) {
	  PR_FREEIF(net_smtp_password);
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
		nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
		status = SendData(url, buffer);
		m_nextState = SMTP_RESPONSE;
		m_nextStateAfterResponse = SMTP_AUTH_LOGIN_RESPONSE;
		SetFlag(SMTP_PAUSE_FOR_READ);
		PR_FREEIF(base64Str);

		return (status);
	}
  }
#endif

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
	PR_ASSERT(0);
	return(-1);
#endif
}

PRInt32 nsSmtpProtocol::SendMailResponse()
{
	PRInt32 status = 0;
	nsCAutoString buffer;

    if(m_responseCode != 250)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_ERROR_SENDING_FROM_COMMAND, m_responseText));
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
			buffer = "RCPT TO:<";
			buffer += m_addresses;
			buffer += "> NOTIFY=SUCCESS,FAILURE ORCPT=rfc822;";
			buffer += encodedAddress;
			buffer += CRLF; 
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
		buffer = "RCPT TO:<";
		buffer += m_addresses;
		buffer += ">";
		buffer += CRLF;
	}
	/* take the address we sent off the list (move the pointer to just
	   past the terminating null.) */
	m_addresses += PL_strlen (m_addresses) + 1;
	m_addressesLeft--;
	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    status = SendData(url, buffer);

    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_SEND_RCPT_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);

    return(status);
}

PRInt32 nsSmtpProtocol::SendRecipientResponse()
{
    PRInt32 status = 0;
	nsCAutoString buffer;

	if(m_responseCode != 250 && m_responseCode != 251)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_ERROR_SENDING_RCPT_COMMAND, m_responseText));
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
	buffer = "DATA";
	buffer += CRLF;
    nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);  
    status = SendData(url, buffer);   

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
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_ERROR_SENDING_DATA_COMMAND, 
			m_responseText));
        return(MK_ERROR_SENDING_DATA_COMMAND);
	}
#ifdef UNREADY_CODE
#ifdef XP_UNIX
	{
	  const char * FE_UsersRealMailAddress(void); /* definition */
	  const char *real_name;
	  char *s = 0;
	  PRBool suppress_sender_header = PR_FALSE;

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
		  NET_SACat(command, buffer);
		  if(!command)
		    {
		      CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
		      return(MK_OUT_OF_MEMORY);
		    }
	           
	      status = (int) NET_BlockingWrite(CE_SOCK, command, PL_strlen(command));   
	      if(status < 0)
		{
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
#endif /* UNREADY_CODE */

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
#endif /* UNREADY_CODE */
	  {
//		m_runningURL->GetBodySize(&m_totalMessageSize);
	  }


    return(status);  
}

// mscott: after dogfood, make a note to move this type of function into a base
// utility class....
#define POST_DATA_BUFFER_SIZE 2048

PRInt32 nsSmtpProtocol::SendMessageInFile()
{
	nsCOMPtr<nsIFileSpec> fileSpec;
	m_runningURL->GetPostMessageFile(getter_AddRefs(fileSpec));
	if (fileSpec)
	{
		// mscott -- this function should be re-written to use the file url code so it can be
		// asynch
		nsFileSpec afileSpec;
		fileSpec->GetFileSpec(&afileSpec);
		nsInputFileStream * fileStream = new nsInputFileStream(afileSpec, PR_RDONLY, 00700);
		if (fileStream && fileStream->is_open())
		{
			PRInt32 amtInBuffer = 0; 
			PRBool lastLineWasComplete = PR_TRUE;

			PRBool quoteLines = PR_TRUE;  // it is always true but I'd like to generalize this function and then it might not be
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

					if (!lastLineWasComplete || (L > 1 && line[L-2] == CR && line[L-1] == LF))
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
//                          L--;
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
					// test hack by mscott. If our buffer is almost full, then send it off & reset ourselves
					// to make more room.
					if (bsize < 100) // i chose 100 arbitrarily.
					{
						nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
						if (*buffer)
							SendData(url, buffer);
						buffer[0] = '\0';
						b = buffer; // reset buffer
						bsize = POST_DATA_BUFFER_SIZE;
					}

				} while (line /* && bsize > 100 */);
              }

			nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
			SendData(url, buffer); 
			delete fileStream;
		}
	} // if filePath

	SetFlag(SMTP_PAUSE_FOR_READ);

	// for now, we are always done at this point..we aren't making multiple calls
	// to post data...

	// always issue a '.' and CRLF when we are done...
	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
	SendData(url, CRLF "." CRLF);
#ifdef UNREADY_CODE
		NET_Progress(CE_WINDOW_ID,
					XP_GetString(XP_MESSAGE_SENT_WAITING_MAIL_REPLY));
#endif /* UNREADY_CODE */
        m_nextState = SMTP_RESPONSE;
        m_nextStateAfterResponse = SMTP_SEND_MESSAGE_RESPONSE;
        return(0);
}

PRInt32 nsSmtpProtocol::SendPostData()
{
	// mscott: as a first pass, I'm writing everything at once and am not
	// doing it in chunks...

    PRInt32 status = 0;

	/* returns 0 on done and negative on error
	 * positive if it needs to continue.
	 */

	// check to see if url is a file..if it is...call our file handler...
	PRBool postMessageInFile = PR_TRUE;
	m_runningURL->GetPostMessage(&postMessageInFile);
	if (postMessageInFile)
	{
		return SendMessageInFile();
	}

	/* Update the thermo and the status bar.  This is done by hand, rather
	   than using the FE_GraphProgress* functions, because there seems to be
	   no way to make FE_GraphProgress shut up and not display anything more
	   when all the data has arrived.  At the end, we want to show the
	   "message sent; waiting for reply" status; FE_GraphProgress gets in
	   the way of that.  See bug #23414. */

#ifdef UNREADY_CODE
	unsigned long curtime;
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
#endif /* UNREADY_CODE */

    return(status);
}



PRInt32 nsSmtpProtocol::SendMessageResponse()
{

    if(m_responseCode != 250)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
		url->SetErrorMessage(NET_ExplainErrorDetails(MK_ERROR_SENDING_MESSAGE, m_responseText));
        return(MK_ERROR_SENDING_MESSAGE);
	}

#ifdef UNREADY_CODE
	NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_MAILSENT));
#endif
	/* else */
	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
	SendData(url, "quit"CRLF); // send a quit command to close the connection with the server.
	m_nextState = SMTP_DONE;
	return(0);
}


nsresult nsSmtpProtocol::LoadUrl(nsIURI * aURL, nsISupports * /* aConsumer */)
{
	nsresult rv = NS_OK;
    PRInt32 status = 0; 
	m_continuationResponse = -1;  /* init */
	//nsISmtpUrl * smtpUrl = nsnull;
	HG77067

	if (aURL)
	{
		m_runningURL = do_QueryInterface(aURL);

#ifdef UNREADY_CODE
		m_hostName = PL_strdup(NET_MailRelayHost(cur_entry->window_id));
#endif
			
		PRBool postMessage = PR_FALSE;
		m_runningURL->GetPostMessage(&postMessage);

		if(postMessage)
		{
			char *addrs1 = 0;
			char *addrs2 = 0;
    		m_nextState = SMTP_RESPONSE;
			m_nextStateAfterResponse = SMTP_EXTN_LOGIN_RESPONSE;

			/* Remove duplicates from the list, to prevent people from getting
				more than one copy (the SMTP host may do this too, or it may not.)
				This causes the address list to be parsed twice; this probably
				doesn't matter.
			*/

			nsXPIDLCString addresses;
			nsCOMPtr<nsIMsgHeaderParser> parser;
            rv = nsComponentManager::CreateInstance(kHeaderParserCID,
                                               nsnull,
                                               nsCOMTypeInfo<nsIMsgHeaderParser>::GetIID(),
                                               getter_AddRefs(parser));

			m_runningURL->GetAllRecipients(getter_Copies(addresses));

			if (NS_SUCCEEDED(rv) && parser)
			{
				parser->RemoveDuplicateAddresses(nsnull, addresses, nsnull, PR_FALSE, &addrs1);

				/* Extract just the mailboxes from the full RFC822 address list.
				   This means that people can post to mailto: URLs which contain
				   full RFC822 address specs, and we will still send the right
				   thing in the SMTP RCPT command.
				*/
				if (addrs1 && *addrs1)
				{
					rv = parser->ParseHeaderAddresses(nsnull, addrs1, nsnull, &addrs2, &m_addressesLeft);
					PR_FREEIF (addrs1);
				}

				if (m_addressesLeft == 0 || addrs2 == nsnull) // hmm no addresses to send message to...
				{
					m_nextState = SMTP_ERROR_DONE;
					ClearFlag(SMTP_PAUSE_FOR_READ);
					status = MK_MIME_NO_RECIPIENTS;
					nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(m_runningURL);
					url->SetErrorMessage(NET_ExplainErrorDetails(status));
					return status;
				}

				m_addressCopy = addrs2;
				m_addresses = m_addressCopy;
			} // if parser
		} // if post message
		
		rv = nsMsgProtocol::LoadUrl(aURL);
	} // if we received a url!

	return rv;
}
	
/*
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
nsresult nsSmtpProtocol::ProcessProtocolState(nsIURI * url, nsIInputStream * inputStream, 
									      PRUint32 sourceOffset, PRUint32 length)
{
    PRInt32 status = 0;
    ClearFlag(SMTP_PAUSE_FOR_READ); /* already paused; reset */

    while(!TestFlag(SMTP_PAUSE_FOR_READ))
      {

        switch(m_nextState) 
		{
			case SMTP_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SmtpResponse(inputStream, length);
				break;

			case SMTP_START_CONNECT:
				SetFlag(SMTP_PAUSE_FOR_READ);
				m_nextState = SMTP_RESPONSE;
				m_nextStateAfterResponse = SMTP_EXTN_LOGIN_RESPONSE;
				break;
			case SMTP_FINISH_CONNECT:
	            SetFlag(SMTP_PAUSE_FOR_READ);
		        break;
#ifdef UNREADY_CODE
	   HG26788
#endif	   
			case SMTP_LOGIN_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = LoginResponse(inputStream, length);
				break;
			case SMTP_EXTN_LOGIN_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = ExtensionLoginResponse(inputStream, length);
				break;

			case SMTP_SEND_HELO_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SendHeloResponse(inputStream, length);
				break;
			case SMTP_SEND_EHLO_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SendEhloResponse(inputStream, length);
				break;

			case SMTP_AUTH_LOGIN_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = AuthLoginResponse(inputStream, length);
				break;
#ifdef UNREADY_CODE
       HG12690
#endif
			 case SMTP_SEND_AUTH_LOGIN_USERNAME:
				 status = AuthLoginUsername();
				 break;

			case SMTP_SEND_AUTH_LOGIN_PASSWORD:
				status = AuthLoginPassword(); 
				break;
			
			case SMTP_SEND_VRFY_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SendVerifyResponse();
				break;
			
			case SMTP_SEND_MAIL_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SendMailResponse();
				break;
			
			case SMTP_SEND_RCPT_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SendRecipientResponse();
				break;
							
			case SMTP_SEND_DATA_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SendDataResponse();
				break;
			
			case SMTP_SEND_POST_DATA:
				status = SendPostData(); 
				break;
			
			case SMTP_SEND_MESSAGE_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SendMessageResponse();
				break;
			case SMTP_DONE:
				{
					nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(m_runningURL);
					mailNewsUrl->SetUrlState(PR_FALSE, NS_OK);
					m_nextState = SMTP_FREE;
				}
				break;
        
			case SMTP_ERROR_DONE:
	            m_nextState = SMTP_FREE;
		        break;
        
			case SMTP_FREE:
				// smtp is a one time use connection so kill it if we get here...
				CloseSocket(); 
	            return NS_OK; /* final end */
        
			default: /* should never happen !!! */
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
      } /* while(!SMTP_PAUSE_FOR_READ) */
    
    return NS_OK;
}

#ifdef UNREADY_CODE
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
    convto = IntlEncodeMimePartIIStr(to, csid, PR_TRUE);
    convsub = IntlEncodeMimePartIIStr(subject, csid, PR_TRUE);

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
    url->post_data_is_file = PR_FALSE;
    url->method = URL_POST_METHOD;
    url->internal_url = PR_TRUE;
    return NET_GetURL(url, FO_PRESENT, context, MessageSendingDone);
    
}

#endif

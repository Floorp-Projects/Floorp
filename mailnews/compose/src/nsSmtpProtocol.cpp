/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "msgCore.h"
#include "nsSmtpProtocol.h"
#include "nscore.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIMsgHeaderParser.h"
#include "nsFileStream.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsString.h"
#include "nsTextFormatter.h"
#include "nsIMsgIdentity.h"
#include "nsISmtpServer.h"
#include "nsMsgComposeStringBundle.h"
#include "prtime.h"
#include "prlog.h"
#include "prerror.h"
#include "prprf.h"
#include "prmem.h"
#include "plbase64.h"
#include "nsEscape.h"
#include "nsMsgUtils.h"
#include "nsIPipe.h"
#include "nsMsgSimulateError.h"

#include "nsISSLSocketControl.h"
/* sigh, cmtcmn.h, included from nsIPSMSocketInfo.h, includes windows.h, which includes winuser.h,
   which defines PostMessage to be either PostMessageA or PostMessageW... of course it does this
   without using parameters, so any use of PostMessage now becomes PostMessageA...
   since this file is XP and doesn't need windows.h, i'm going to just undef this out for now

   when someone comes up with a better solution, please let me know.

   Stuart Parmenter <pavlov@netscape.com>
*/
#ifdef PostMessage
#undef PostMessage
#endif


#ifndef XP_UNIX
#include <stdarg.h>
#endif /* !XP_UNIX */

static PRLogModuleInfo *SMTPLogModule = nsnull;

static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);

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

/* based on in NET_ExplainErrorDetails in mkmessag.c */
nsresult nsExplainErrorDetails(nsISmtpUrl * aSmtpUrl, int code, ...)
{
  NS_ENSURE_ARG(aSmtpUrl);

	nsresult rv = NS_OK;
	va_list args;
	
  nsCOMPtr<nsIPrompt> dialog;
  aSmtpUrl->GetPrompt(getter_AddRefs(dialog));
  NS_ENSURE_TRUE(dialog, NS_ERROR_FAILURE);

  PRUnichar *  msg;
	nsXPIDLString eMsg;
  nsCOMPtr<nsIMsgStringService> smtpBundle = do_GetService(NS_MSG_SMTPSTRINGSERVICE_CONTRACTID);

	va_start (args, code);

	switch (code)	{
		case NS_ERROR_SMTP_SERVER_ERROR:
		case NS_ERROR_TCP_READ_ERROR:
		case NS_ERROR_SENDING_FROM_COMMAND:
		case NS_ERROR_SENDING_RCPT_COMMAND:
		case NS_ERROR_SENDING_DATA_COMMAND:
		case NS_ERROR_SENDING_MESSAGE:   
      smtpBundle->GetStringByID(code, getter_Copies(eMsg));
			msg = nsTextFormatter::vsmprintf(eMsg, args);
			break;
		default:
      smtpBundle->GetStringByID(NS_ERROR_COMMUNICATIONS_ERROR, getter_Copies(eMsg));
			msg = nsTextFormatter::smprintf(eMsg, code);
			break;
	}

	if (msg) 
  {
		rv = dialog->Alert(nsnull, msg);
		nsTextFormatter::smprintf_free(msg);
	}

	va_end (args);

	return rv;
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
static char *
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

NS_IMPL_ADDREF_INHERITED(nsSmtpProtocol, nsMsgAsyncWriteProtocol)
NS_IMPL_RELEASE_INHERITED(nsSmtpProtocol, nsMsgAsyncWriteProtocol)

NS_INTERFACE_MAP_BEGIN(nsSmtpProtocol)
    NS_INTERFACE_MAP_ENTRY(nsIMsgLogonRedirectionRequester)
NS_INTERFACE_MAP_END_INHERITING(nsMsgAsyncWriteProtocol)

nsSmtpProtocol::nsSmtpProtocol(nsIURI * aURL)
    : nsMsgAsyncWriteProtocol(aURL)
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
    m_prefAuthMethod = PREF_AUTH_NONE;
    m_usernamePrompted = PR_FALSE;
    m_prefTrySSL = PREF_SSL_TRY;
    m_port = SMTP_PORT;
    m_tlsInitiated = PR_FALSE;

    m_urlErrorState = NS_ERROR_FAILURE;

    if (!SMTPLogModule)
        SMTPLogModule = PR_NewLogModule("SMTP");
    
    if (aURL) 
        m_runningURL = do_QueryInterface(aURL);

    if (!mSmtpBundle)
        mSmtpBundle = do_GetService(NS_MSG_SMTPSTRINGSERVICE_CONTRACTID);
    
    // extract out message feedback if there is any.
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aURL);
    if (mailnewsUrl)
        mailnewsUrl->GetStatusFeedback(getter_AddRefs(m_statusFeedback));

    m_dataBuf = (char *) PR_Malloc(sizeof(char) * OUTPUT_BUFFER_SIZE);
    m_dataBufSize = OUTPUT_BUFFER_SIZE;

    m_nextState = SMTP_START_CONNECT;
    m_nextStateAfterResponse = SMTP_START_CONNECT;
    m_responseCode = 0;
    m_previousResponseCode = 0;
    m_continuationResponse = -1; 
    m_tlsEnabled = PR_FALSE;
    m_addressCopy = nsnull;
    m_addresses = nsnull;

  	m_addressesLeft = nsnull;
    m_verifyAddress = nsnull;	
    m_totalAmountWritten = 0;
    m_totalMessageSize = 0;
    m_originalContentLength = 0;

    // ** may want to consider caching the server capability to save lots of
    // round trip communication between the client and server
    nsCOMPtr<nsISmtpServer> smtpServer;
    m_runningURL->GetSmtpServer(getter_AddRefs(smtpServer));
    if (smtpServer) {
        smtpServer->GetAuthMethod(&m_prefAuthMethod);
        smtpServer->GetTrySSL(&m_prefTrySSL);
    }
    
    rv = RequestOverrideInfo(smtpServer);
    // if we aren't waiting for a login override, then go ahead an
    // open the network connection like we normally would have.
    if (NS_SUCCEEDED(rv) && TestFlag(SMTP_WAIT_FOR_REDIRECTION))
        return;

    nsXPIDLCString hostName;
    aURL->GetHost(getter_Copies(hostName));
    PR_LOG(SMTPLogModule, PR_LOG_ALWAYS, ("SMTP Connecting to: %s", (const char *) hostName));
    
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    nsCOMPtr<nsISmtpUrl> smtpUrl(do_QueryInterface(aURL));
    if (smtpUrl)
        smtpUrl->GetNotificationCallbacks(getter_AddRefs(callbacks));

    if (m_prefTrySSL != PREF_SSL_NEVER) {
        rv = OpenNetworkSocket(aURL, "tlsstepup", callbacks);
        if (NS_FAILED(rv) && m_prefTrySSL == PREF_SSL_TRY) {
            m_prefTrySSL = PREF_SSL_NEVER;
            rv = OpenNetworkSocket(aURL, nsnull, callbacks);
        }
    } else {
        rv = OpenNetworkSocket(aURL, nsnull, callbacks);
    }
    
    if (NS_FAILED(rv))
        return;
}

const char * nsSmtpProtocol::GetUserDomainName()
{
	nsresult rv;
	NS_PRECONDITION(m_runningURL, "we must be running a url in order to get the user's domain...");

	if (m_runningURL)
	{
		nsCOMPtr <nsIMsgIdentity> senderIdentity;
		rv = m_runningURL->GetSenderIdentity(getter_AddRefs(senderIdentity));
		if (NS_FAILED(rv) || !senderIdentity) 
			return nsnull;
				
		rv =  senderIdentity->GetEmail(getter_Copies(m_mailAddr));
		if (NS_FAILED(rv) || !((const char *)m_mailAddr)) 	
			return nsnull;
		
		const char *atSignMarker = nsnull;
		atSignMarker = PL_strchr(m_mailAddr, '@');
		return atSignMarker ? atSignMarker+1 :  (const char *) m_mailAddr;  // return const ptr into buffer in running url...
	}

	return nsnull;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsSmtpProtocol::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult aStatus)
{
	nsMsgAsyncWriteProtocol::OnStopRequest(nsnull, ctxt, aStatus);

	// okay, we've been told that the send is done and the connection is going away. So 
	// we need to release all of our state
  return nsMsgAsyncWriteProtocol::CloseSocket();
}

/////////////////////////////////////////////////////////////////////////////////////////////
// End of nsIStreamListenerSupport
//////////////////////////////////////////////////////////////////////////////////////////////

PRInt32 nsSmtpProtocol::ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line)
{
  nsCOMPtr<nsISearchableInputStream> bufferInputStr = do_QueryInterface(inputStream);
  PRUint32 numBytesLastRead = 0;  // total number of bytes read into the current line

  if (bufferInputStr)
  {
    // only try to read out an entire line...if we don't have a full line yet then wait for
    // more data....
    PRBool found = PR_FALSE;
    PRUint32 offset = 0;
    bufferInputStr->Search("\n", PR_TRUE,  &found, &offset); 
    if (found && offset < OUTPUT_BUFFER_SIZE - 1)
    {
	    m_dataBuf[0] = '\0';
      inputStream->Read(m_dataBuf, offset + 1 /* + 1 to read past the \n */, &numBytesLastRead);
      m_dataBuf[numBytesLastRead] = '\0';

      *line = m_dataBuf;
    }
	else
      return -1; // inform caller to block

  }
	else
  {
    NS_ASSERTION(0, "uhoh.....our input stream is no longer searchable");
    return 0; 
  }

  return numBytesLastRead;
}

void nsSmtpProtocol::UpdateStatus(PRInt32 aStatusID)
{
	if (m_statusFeedback)
	{
    nsXPIDLString msg;
    mSmtpBundle->GetStringByID(aStatusID, getter_Copies(msg));
		UpdateStatusWithString(msg);
	}
}

void nsSmtpProtocol::UpdateStatusWithString(const PRUnichar * aStatusString)
{
	if (m_statusFeedback && aStatusString)
		m_statusFeedback->ShowStatusString(aStatusString);
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

  if (status < 0) // we are blocked waiting for more data...
  {
     m_nextState = SMTP_RESPONSE;
     SetFlag(SMTP_PAUSE_FOR_READ);
     return 0; 
	}
	
  PR_LOG(SMTPLogModule, PR_LOG_ALWAYS, ("SMTP Response: %s", line));
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

  if (m_responseCode == 220 && m_responseText.Length() && !m_tlsInitiated)
  { 
        m_nextStateAfterResponse = SMTP_EXTN_LOGIN_RESPONSE;
  }

	if(m_continuationResponse == -1)  /* all done with this response? */
	{
		m_nextState = m_nextStateAfterResponse;
		ClearFlag(SMTP_PAUSE_FOR_READ); /* don't pause */
	}
  else
  {
    inputStream->Available(&length); // refresh the length as it has changed...
    if (!length)
       SetFlag(SMTP_PAUSE_FOR_READ);
  }

  return(0);  /* everything ok */
}

PRInt32 nsSmtpProtocol::LoginResponse(nsIInputStream * inputStream, PRUint32 length)
{
	PRInt32 status = 0;
	nsCAutoString buffer ("HELO ");

	if(m_responseCode != 220)
	{
		m_urlErrorState = NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER;
		return(NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	}

	buffer += GetUserDomainName();
	buffer += CRLF;
	
	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
  status = SendData(url, buffer.get());

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
		m_urlErrorState = NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER;
		return(NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	}

	buffer += GetUserDomainName();
	buffer += CRLF;

	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
  status = SendData(url, buffer.get());

  m_nextState = SMTP_RESPONSE;
  m_nextStateAfterResponse = SMTP_SEND_EHLO_RESPONSE;
  SetFlag(SMTP_PAUSE_FOR_READ);

  return(status);
}
    

PRInt32 nsSmtpProtocol::SendHeloResponse(nsIInputStream * inputStream, PRUint32 length)
{
	PRInt32 status = 0;
	nsCAutoString buffer;
	nsresult rv;

	// extract the email address from the identity
	nsXPIDLCString emailAddress;

	nsCOMPtr <nsIMsgIdentity> senderIdentity;
	rv = m_runningURL->GetSenderIdentity(getter_AddRefs(senderIdentity));
	if (NS_FAILED(rv) || !senderIdentity) {
		m_urlErrorState = NS_ERROR_COULD_NOT_GET_USERS_MAIL_ADDRESS;
		return(NS_ERROR_COULD_NOT_GET_USERS_MAIL_ADDRESS);
	}
	else 
  {
		senderIdentity->GetEmail(getter_Copies(emailAddress));
  }

	/* don't check for a HELO response because it can be bogus and
	 * we don't care
	 */

	if(!((const char *)emailAddress) || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_16))
	{
		m_urlErrorState = NS_ERROR_COULD_NOT_GET_USERS_MAIL_ADDRESS;
		return(NS_ERROR_COULD_NOT_GET_USERS_MAIL_ADDRESS);
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
		nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);
    char *fullAddress = nsnull;
		 if (parser) {
			 // pass nsnull for the name, since we just want the email.
			 //
			 // seems a little weird that we are passing in the emailAddress
			 // when that's the out parameter 
			 parser->MakeFullAddress(nsnull, nsnull /* name */, emailAddress /* address */, &fullAddress);
		}
#ifdef UNREADY_CODE		
		 if (CE_URL_S->msg_pane) 
		 {
			if (MSG_RequestForReturnReceipt(CE_URL_S->msg_pane)) 
			{
				if (TestFlag(SMTP_EHLO_DSN_ENABLED)) 
				{
					PR_snprintf(buffer, sizeof(buffer), 
								"MAIL FROM:<%.256s> RET=FULL ENVID=NS40112696JT" CRLF, fullAddress);
				}
				else 
				{
					FE_Alert (CE_WINDOW_ID, XP_GetString(XP_RETURN_RECEIPT_NOT_SUPPORT));
					PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, fullAddress);
				}
			}
			else if (MSG_SendingMDNInProgress(CE_URL_S->msg_pane)) 
			{
				PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, "");
			}
			else 
			{
				PR_snprintf(buffer, sizeof(buffer), "MAIL FROM:<%.256s>" CRLF, fullAddress);
			}
		}
		else 
#endif
		{
			buffer = "MAIL FROM:<";
			buffer += fullAddress;
			buffer += ">" CRLF;
		}
		PR_FREEIF (fullAddress);
	  }

	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    status = SendData(url, buffer.get());

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
    nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    
    if (m_responseCode != 250) 
    {
      /* EHLO must not be implemented by the server so fall back to the HELO case */

        if (m_prefTrySSL == PREF_SSL_ALWAYS)
        {
            m_nextState = SMTP_ERROR_DONE;
            m_urlErrorState = NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER;
            return(NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER);
        }        
        buffer = "HELO ";
        buffer += GetUserDomainName();
        buffer += CRLF;
        
        status = SendData(url, buffer.get());
        
        m_nextState = SMTP_RESPONSE;
        m_nextStateAfterResponse = SMTP_SEND_HELO_RESPONSE;
        SetFlag(SMTP_PAUSE_FOR_READ);
        return (status);
    }
    else 
    {
        char *ptr = NULL;
        
        ptr = PL_strcasestr(m_responseText.get(), "DSN");
        if (ptr && nsCRT::ToUpper(*(ptr-1)) != 'X')
            SetFlag(SMTP_EHLO_DSN_ENABLED);
        
        if (PL_strcasestr(m_responseText.get(), " PLAIN") != 0)
            SetFlag(SMTP_AUTH_PLAIN_ENABLED);

        if (PL_strcasestr(m_responseText.get(), "AUTH=LOGIN") != 0)
            SetFlag(SMTP_AUTH_LOGIN_ENABLED);	/* old style */

        if (PL_strcasestr(m_responseText.get(), "STARTTLS") != 0)
            SetFlag(SMTP_EHLO_STARTTLS_ENABLED);

        if (PL_strcasestr(m_responseText.get(), "EXTERNAL") != 0)
            SetFlag(SMTP_AUTH_EXTERNAL_ENABLED);
    }
    m_nextState = SMTP_AUTH_PROCESS_STATE;
    return status;
}

PRInt32 nsSmtpProtocol::SendTLSResponse()
{
  // only tear down our existing connection and open a new one if we received a 220 response
  // from the smtp server after we issued the STARTTLS 
  nsresult rv = NS_OK;
  if (m_responseCode == 220) 
  {
      nsCOMPtr<nsISupports> secInfo;
      nsCOMPtr<nsITransport> trans;
      nsCOMPtr<nsITransportRequest> transReq = do_QueryInterface(m_request, &rv);
      if (NS_FAILED(rv)) return rv;

      rv = transReq->GetTransport(getter_AddRefs(trans));
      if (NS_FAILED(rv)) return rv;

      rv = trans->GetSecurityInfo(getter_AddRefs(secInfo));

      if (NS_SUCCEEDED(rv) && secInfo) {
          nsCOMPtr<nsISSLSocketControl> sslControl = do_QueryInterface(secInfo, &rv);

          if (NS_SUCCEEDED(rv) && sslControl) {
              rv = sslControl->TLSStepUp();
          }
      }

    if (NS_FAILED(rv)) {
        // if we fail, should we close the connection?
        return rv;
    }

    m_nextState = SMTP_EXTN_LOGIN_RESPONSE;
    m_nextStateAfterResponse = SMTP_EXTN_LOGIN_RESPONSE;
    m_tlsEnabled = PR_TRUE;
    m_flags = 0;
  }

  return rv;
}

PRInt32 nsSmtpProtocol::ProcessAuth()
{
    PRInt32 status = 0;
    nsCAutoString buffer;
    nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);

    if (!m_tlsEnabled)
    {
        if(TestFlag(SMTP_EHLO_STARTTLS_ENABLED))
        {
            if (m_prefTrySSL != PREF_SSL_NEVER)
            {
                buffer = "STARTTLS";
                buffer += CRLF;

                status = SendData(url, buffer.get());

                m_flags = 0; // resetting the flags
                m_tlsInitiated = PR_TRUE;

                m_nextState = SMTP_RESPONSE;
                m_nextStateAfterResponse = SMTP_TLS_RESPONSE;
                SetFlag(SMTP_PAUSE_FOR_READ);
                return status;
            }
        }
        else if (m_prefTrySSL == PREF_SSL_ALWAYS)
        {
            m_nextState = SMTP_ERROR_DONE;
            m_urlErrorState = NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER;
            return(NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER);
        }
        if ((TestFlag(SMTP_AUTH_PLAIN_ENABLED) ||
             TestFlag(SMTP_AUTH_LOGIN_ENABLED)) &&
            (m_prefAuthMethod == PREF_AUTH_ANY))
        {
            m_nextState = SMTP_SEND_AUTH_LOGIN_USERNAME;
            m_nextStateAfterResponse = SMTP_AUTH_LOGIN_RESPONSE;
            return NS_OK;
        }
        m_nextState = SMTP_SEND_HELO_RESPONSE;
        return NS_OK;
    }
    else // TLS enabled
    {
        if (TestFlag(SMTP_AUTH_EXTERNAL_ENABLED))
        {
            buffer = "AUTH EXTERNAL =";
            buffer += CRLF;
            SendData(url, buffer.get());
            m_nextState = SMTP_RESPONSE;
            m_nextStateAfterResponse = SMTP_AUTH_EXTERNAL_RESPONSE;
            SetFlag(SMTP_PAUSE_FOR_READ);
            return NS_OK;
        }
        else if (TestFlag(SMTP_AUTH_LOGIN_ENABLED) ||
                 TestFlag(SMTP_AUTH_PLAIN_ENABLED))
        {
            m_nextState = SMTP_SEND_AUTH_LOGIN_USERNAME;
            m_nextStateAfterResponse = SMTP_AUTH_LOGIN_RESPONSE;
            return NS_OK;
        }
        else
        {
            m_nextState = SMTP_SEND_HELO_RESPONSE;
            return NS_OK;
        }
    }
}

PRInt32 nsSmtpProtocol::AuthLoginResponse(nsIInputStream * stream, PRUint32 length)
{
  PRInt32 status = 0;
  nsCOMPtr<nsISmtpServer> smtpServer;
  m_runningURL->GetSmtpServer(getter_AddRefs(smtpServer));

  switch (m_responseCode/100) 
  {
  case 2:
      m_nextState = SMTP_SEND_HELO_RESPONSE;
	break;
  case 3:
      m_nextState = SMTP_SEND_AUTH_LOGIN_PASSWORD;
      break;
  case 5:
  default:
      if (smtpServer)
      {
          // only forget the password if we didn't get here from the redirection.
          if (mLogonCookie.IsEmpty()) {
              smtpServer->ForgetPassword();
              if (m_usernamePrompted)
                  smtpServer->SetUsername("");
          }
          m_nextState = SMTP_SEND_AUTH_LOGIN_USERNAME;
      }
      else
      {
          status = NS_ERROR_SMTP_PASSWORD_UNDEFINED;
      }
      break;
  }
  
  return (status);
}


PRInt32 nsSmtpProtocol::AuthLoginUsername()
{
  char buffer[512];
  nsresult rv;
  PRInt32 status = 0;
  nsXPIDLCString username;
  char *base64Str = nsnull;
  nsXPIDLCString origPassword;
  nsCAutoString password;
  nsCOMPtr<nsISmtpServer> smtpServer;
  rv = m_runningURL->GetSmtpServer(getter_AddRefs(smtpServer));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  rv = smtpServer->GetUsername(getter_Copies(username));

  if (!(const char*) username || nsCRT::strlen((const char*)username) == 0) {
      rv = GetUsernamePassword(getter_Copies(username), getter_Copies(origPassword));
      m_usernamePrompted = PR_TRUE;
      password.Assign(origPassword);
      if (password.IsEmpty())
          return NS_ERROR_SMTP_PASSWORD_UNDEFINED;
  }
  else if (!TestFlag(SMTP_USE_LOGIN_REDIRECTION))
  {
    rv = GetPassword(getter_Copies(origPassword));
    password.Assign(origPassword);
    if (password.IsEmpty())
      return NS_ERROR_SMTP_PASSWORD_UNDEFINED;

  }
  else
    password.Assign(mLogonCookie);
  
  if (TestFlag(SMTP_AUTH_PLAIN_ENABLED))
  {
	  char plain_string[512];
	  int len = 1; /* first <NUL> char */

	  nsCRT::memset(plain_string, 0, 512);
	  PR_snprintf(&plain_string[1], 510, "%s", (const char*)username);
	  len += PL_strlen(username);
	  len++; /* second <NUL> char */
	  PR_snprintf(&plain_string[len], 511-len, "%s", password.get());
	  len += password.Length();

	  base64Str = PL_Base64Encode(plain_string, len, nsnull);
  }
  else
  {
	  base64Str = 
          PL_Base64Encode((const char *) username, 
                          nsCRT::strlen((const char*)username), nsnull);
  } 
  if (base64Str) {
	  if (TestFlag(SMTP_AUTH_PLAIN_ENABLED))
		  PR_snprintf(buffer, sizeof(buffer), "AUTH PLAIN %.256s" CRLF, base64Str);
	  else if (TestFlag(SMTP_AUTH_LOGIN_ENABLED))
		  PR_snprintf(buffer, sizeof(buffer), "AUTH LOGIN %.256s" CRLF, base64Str);
	  else
		  return (NS_ERROR_COMMUNICATIONS_ERROR);
	
	  nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
	  status = SendData(url, buffer, PR_TRUE);
	  m_nextState = SMTP_RESPONSE;
	  m_nextStateAfterResponse = SMTP_AUTH_LOGIN_RESPONSE;
	  SetFlag(SMTP_PAUSE_FOR_READ);
	  nsCRT::free(base64Str);
	
	  return (status);
  }
  return -1;
}

PRInt32 nsSmtpProtocol::AuthLoginPassword()
{

  /* use cached smtp password first
   * if not then use cached pop password
   * if pop password undefined 
   * sync with smtp password
   */
  PRInt32 status = 0;
  nsresult rv;
  nsXPIDLCString origPassword;
  nsCAutoString password;

  if (!TestFlag(SMTP_USE_LOGIN_REDIRECTION))
  {
    rv = GetPassword(getter_Copies(origPassword));
    PRInt32 passwordLength = nsCRT::strlen((const char *) origPassword);
    if (!(const char*) origPassword || passwordLength == 0)
	    return NS_ERROR_SMTP_PASSWORD_UNDEFINED;
	  password.Assign((const char*) origPassword);
  }
  else
    password.Assign(mLogonCookie);

  if (!password.IsEmpty()) 
  {
    char *base64Str = PL_Base64Encode(password.get(), password.Length(), nsnull);

    char buffer[512];
    PR_snprintf(buffer, sizeof(buffer), "%.256s" CRLF, base64Str);
    nsCRT::free(base64Str);
    nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    status = SendData(url, buffer, PR_TRUE);
    m_nextState = SMTP_RESPONSE;
    m_nextStateAfterResponse = SMTP_AUTH_LOGIN_RESPONSE;
    SetFlag(SMTP_PAUSE_FOR_READ);   
    return (status);
  }

  return -1;
}

PRInt32 nsSmtpProtocol::SendVerifyResponse()
{
#if 0
	PRInt32 status = 0;
    char buffer[512];

    if(m_responseCode == 250 || m_responseCode == 251)
		return(NS_USER_VERIFIED_BY_SMTP);
	else
		return(NS_USER_NOT_VERIFIED_BY_SMTP);
#else	
	PR_ASSERT(0);
	return(-1);
#endif
}

PRInt32 nsSmtpProtocol::SendMailResponse()
{
	PRInt32 status = 0;
	nsCAutoString buffer;

  if(m_responseCode != 250 || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_11))
	{
		nsresult rv = nsExplainErrorDetails(m_runningURL, NS_ERROR_SENDING_FROM_COMMAND, m_responseText.get());
		NS_ASSERTION(NS_SUCCEEDED(rv), "failed to explain SMTP error");

		m_urlErrorState = NS_ERROR_BUT_DONT_SHOW_ALERT;
		return(NS_ERROR_SENDING_FROM_COMMAND);
	}

    /* Send the RCPT TO: command */
#ifdef UNREADY_CODE
	if (TestFlag(SMTP_EHLO_DSN_ENABLED) &&
		(CE_URL_S->msg_pane && 
		 MSG_RequestForReturnReceipt(CE_URL_S->msg_pane)))
#else
	if (TestFlag(SMTP_EHLO_DSN_ENABLED) && PR_FALSE)
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
			m_urlErrorState = NS_ERROR_OUT_OF_MEMORY;
			return (NS_ERROR_OUT_OF_MEMORY);
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
    status = SendData(url, buffer.get());

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
    nsresult rv = nsExplainErrorDetails(m_runningURL, NS_ERROR_SENDING_RCPT_COMMAND, m_responseText.get());
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to explain SMTP error");

    m_urlErrorState = NS_ERROR_BUT_DONT_SHOW_ALERT;
    return(NS_ERROR_SENDING_RCPT_COMMAND);
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
    status = SendData(url, buffer.get());

    m_nextState = SMTP_RESPONSE;  
    m_nextStateAfterResponse = SMTP_SEND_DATA_RESPONSE; 
    SetFlag(SMTP_PAUSE_FOR_READ);   

    return(status);  
}


PRInt32 nsSmtpProtocol::SendData(nsIURI *url, const char *dataBuffer, PRBool aSuppressLogging)
{
  if (!dataBuffer) return -1;

  if (!aSuppressLogging) {
      PR_LOG(SMTPLogModule, PR_LOG_ALWAYS, ("SMTP Send: %s", dataBuffer));
  } else {
      PR_LOG(SMTPLogModule, PR_LOG_ALWAYS, ("Logging suppressed for this command (it probably contained authentication information)"));
  }
  return nsMsgAsyncWriteProtocol::SendData(url, dataBuffer);
}


PRInt32 nsSmtpProtocol::SendDataResponse()
{
  PRInt32 status = 0;
  char * command=0;   

  if((m_responseCode != 354) && (m_responseCode != 250)) {
    nsresult rv = nsExplainErrorDetails(m_runningURL, NS_ERROR_SENDING_DATA_COMMAND, m_responseText.get());
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to explain SMTP error");

    m_urlErrorState = NS_ERROR_BUT_DONT_SHOW_ALERT;
    return(NS_ERROR_SENDING_DATA_COMMAND);
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
               		m_urlErrorState = NS_ERROR_COULD_NOT_GET_UID;
               		return(NS_ERROR_COULD_NOT_GET_UID);
		}
	      if(real_name)
		{
		  char buffer[512];
		  PR_snprintf(buffer, sizeof(buffer), "Sender: %.256s" CRLF, real_name);
		  NS_MsgSACat(command, buffer);
		  if(!command)
		    {
		      m_urlErrorState = NS_ERROR_OUT_OF_MEMORY;
		      return(NS_ERROR_OUT_OF_MEMORY);
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

    UpdateStatus(SMTP_DELIV_MAIL);

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

PRInt32 nsSmtpProtocol::SendMessageInFile()
{
	nsCOMPtr<nsIFileSpec> fileSpec;
  nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
	m_runningURL->GetPostMessageFile(getter_AddRefs(fileSpec));
	if (url && fileSpec)
        PostMessage(url, fileSpec);

	SetFlag(SMTP_PAUSE_FOR_READ);

	// for now, we are always done at this point..we aren't making multiple calls
	// to post data...

  UpdateStatus(SMTP_DELIV_MAIL);
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

  if(((m_responseCode != 354) && (m_responseCode != 250)) || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_12)) {
    nsresult rv = nsExplainErrorDetails(m_runningURL, NS_ERROR_SENDING_MESSAGE, m_responseText.get());
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to explain SMTP error");

    m_urlErrorState = NS_ERROR_BUT_DONT_SHOW_ALERT;
    return(NS_ERROR_SENDING_MESSAGE);
	}

  UpdateStatus(SMTP_PROGRESS_MAILSENT);

    /* else */
	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
	SendData(url, "quit"CRLF); // send a quit command to close the connection with the server.
	m_nextState = SMTP_DONE;
	return(0);
}


nsresult nsSmtpProtocol::LoadUrl(nsIURI * aURL, nsISupports * aConsumer )
{
	nsresult rv = NS_OK;

  // if we are currently waiting for login redirection information
  // then hold off on loading the url....but be sure to remember 
  // aConsumer so we can use it later...
  if (TestFlag(SMTP_WAIT_FOR_REDIRECTION))
  {
    // mark a pending load...
    SetFlag(SMTP_LOAD_URL_PENDING);
    mPendingConsumer = aConsumer;
    return NS_OK;
  }
  else
    ClearFlag(SMTP_LOAD_URL_PENDING); 

  // otherwise begin loading the url

  PRInt32 status = 0; 
	m_continuationResponse = -1;  /* init */
	if (aURL)
	{
		m_runningURL = do_QueryInterface(aURL);

    // we had a bug where we failed to bring up an alert if the host
    // name was empty....so throw up an alert saying we don't have
    // a host name and inform the caller that we are not going to
    // run the url...

    nsXPIDLCString hostName;
    aURL->GetHost(getter_Copies(hostName));
    if (!hostName)
    {
       nsCOMPtr <nsIMsgMailNewsUrl> aMsgUrl = do_QueryInterface(aURL);
       if (aMsgUrl)
       {
           aMsgUrl->SetUrlState(PR_TRUE, NS_OK);
           rv = aMsgUrl->SetUrlState(PR_FALSE /* we aren't running the url */, NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER); // set the url as a url currently being run...
       }
        return NS_ERROR_BUT_DONT_SHOW_ALERT;
    }
	
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
			nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);

			m_runningURL->GetRecipients(getter_Copies(addresses));

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

				if (m_addressesLeft == 0 || addrs2 == nsnull || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_8)) // hmm no addresses to send message to...
				{
					m_nextState = SMTP_ERROR_DONE;
					ClearFlag(SMTP_PAUSE_FOR_READ);
					status = NS_MSG_NO_RECIPIENTS;
          m_urlErrorState = NS_MSG_NO_RECIPIENTS;
					return(status);
				}

				m_addressCopy = addrs2;
				m_addresses = m_addressCopy;
			} // if parser
		} // if post message
		
		rv = nsMsgProtocol::LoadUrl(aURL, aConsumer);
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
        PR_LOG(SMTPLogModule, PR_LOG_ALWAYS, ("SMTP entering state: %d",
                                              m_nextState));
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
            case SMTP_LOGIN_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = LoginResponse(inputStream, length);
                break;
            case SMTP_TLS_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = SendTLSResponse();
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
            case SMTP_AUTH_PROCESS_STATE:
                status = ProcessAuth();
                break;

            case SMTP_AUTH_EXTERNAL_RESPONSE:
			case SMTP_AUTH_LOGIN_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(SMTP_PAUSE_FOR_READ);
				else
					status = AuthLoginResponse(inputStream, length);
				break;

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
                {
					nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(m_runningURL);
					// propagate the right error code
					mailNewsUrl->SetUrlState(PR_FALSE, m_urlErrorState);
					m_nextState = SMTP_FREE;
				}
	      
                m_nextState = SMTP_FREE;
                break;
        
			case SMTP_FREE:
				// smtp is a one time use connection so kill it if we get here...
        nsMsgAsyncWriteProtocol::CloseSocket(); 
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

nsresult
nsSmtpProtocol::GetPassword(char **aPassword)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    NS_ENSURE_ARG_POINTER(aPassword);

    nsCOMPtr<nsISmtpUrl> smtpUrl = do_QueryInterface(m_runningURL, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISmtpServer> smtpServer;
    rv = smtpUrl->GetSmtpServer(getter_AddRefs(smtpServer));
    if (NS_FAILED(rv)) return rv;

    rv = smtpServer->GetPassword(aPassword);
    if (NS_FAILED(rv)) return rv;

    if (PL_strlen(*aPassword) > 0)
        return rv;
    // empty password

    nsCRT::free(*aPassword);
    *aPassword = 0;

    nsCOMPtr<nsIAuthPrompt> netPrompt;
    rv = smtpUrl->GetAuthPrompt(getter_AddRefs(netPrompt));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString username;
    nsXPIDLCString hostname;
    PRUnichar *passwordPromptString = nsnull;

    nsXPIDLString passwordTemplate;
    mSmtpBundle->GetStringByID(NS_SMTP_PASSWORD_PROMPT, getter_Copies(passwordTemplate));

    if (!passwordTemplate) return NS_ERROR_NULL_POINTER;
    
    nsXPIDLString passwordTitle;
    mSmtpBundle->GetStringByID(NS_SMTP_PASSWORD_PROMPT_TITLE, getter_Copies(passwordTitle));

    if (!passwordTitle) 
    {
        rv = NS_ERROR_NULL_POINTER;
        goto done;
    }

    rv = smtpServer->GetUsername(getter_Copies(username));
    if (NS_FAILED(rv)) goto done;
    rv = smtpServer->GetHostname(getter_Copies(hostname));
    if (NS_FAILED(rv)) goto done;

    passwordPromptString = nsTextFormatter::smprintf(passwordTemplate,
                                                     (const char *) username,
                                                     (const char *) hostname);
    if (!passwordPromptString)
    {
        rv = NS_ERROR_NULL_POINTER;
        goto done;
    }
    
    rv = smtpServer->GetPasswordWithUI(passwordPromptString, passwordTitle,
                                       netPrompt, aPassword);

done:
    if (passwordPromptString)
        nsCRT::free(passwordPromptString);

    return rv;
}

nsresult
nsSmtpProtocol::GetUsernamePassword(char **aUsername, char **aPassword)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    NS_ENSURE_ARG_POINTER(aUsername);
    NS_ENSURE_ARG_POINTER(aPassword);

    nsCOMPtr<nsISmtpUrl> smtpUrl = do_QueryInterface(m_runningURL, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISmtpServer> smtpServer;
    rv = smtpUrl->GetSmtpServer(getter_AddRefs(smtpServer));
    if (NS_FAILED(rv)) return rv;

    rv = smtpServer->GetPassword(aPassword);
    if (NS_FAILED(rv)) return rv;

    if (PL_strlen(*aPassword) > 0) {
        rv = smtpServer->GetUsername(aUsername);
        if (NS_FAILED(rv)) return rv;

        if (PL_strlen(*aUsername) > 0)
            return rv;
        
        // empty username
        nsCRT::free(*aUsername);
        *aUsername = 0;
    }
    // empty password

    nsCRT::free(*aPassword);
    *aPassword = 0;

    nsCOMPtr<nsIAuthPrompt> netPrompt;
    rv = smtpUrl->GetAuthPrompt(getter_AddRefs(netPrompt));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString hostname;
    PRUnichar *passwordPromptString = nsnull;

    nsXPIDLString passwordTemplate;
    mSmtpBundle->GetStringByID(NS_SMTP_USERNAME_PASSWORD_PROMPT, getter_Copies(passwordTemplate));

    if (!passwordTemplate) return NS_ERROR_NULL_POINTER;
    
    nsXPIDLString passwordTitle;
    mSmtpBundle->GetStringByID(NS_SMTP_PASSWORD_PROMPT_TITLE, getter_Copies(passwordTitle));

    if (!passwordTitle) 
        return NS_ERROR_NULL_POINTER;

    rv = smtpServer->GetHostname(getter_Copies(hostname));
    if (NS_FAILED(rv)) 
        return rv;

    passwordPromptString = nsTextFormatter::smprintf(passwordTemplate,
                                                     (const char *) hostname);
    if (!passwordPromptString)
        return NS_ERROR_NULL_POINTER;
    
    rv = smtpServer->GetUsernamePasswordWithUI(passwordPromptString, passwordTitle,
                                       netPrompt, aUsername, aPassword);

    if (passwordPromptString)
        nsCRT::free(passwordPromptString);

    return rv;
}


nsresult nsSmtpProtocol::RequestOverrideInfo(nsISmtpServer * aSmtpServer)
{
  NS_ENSURE_ARG(aSmtpServer);

	nsresult rv;
	nsCAutoString contractID(NS_MSGLOGONREDIRECTORSERVICE_CONTRACTID);

  // go get the redirection type...
  nsXPIDLCString redirectionTypeStr; 
  aSmtpServer->GetRedirectorType(getter_Copies(redirectionTypeStr));

  const char * redirectionType = (const char *) redirectionTypeStr;

  // if we don't have a redirection type, then get out and proceed normally.
  if (!redirectionType || !*redirectionType )
    return NS_OK;

	contractID.Append('/');
	contractID.Append(redirectionTypeStr);

	m_logonRedirector = do_GetService(contractID.get(), &rv);
	if (m_logonRedirector && NS_SUCCEEDED(rv))
	{
		nsXPIDLCString password;
		nsXPIDLCString userName;
    PRBool requiresPassword = PR_TRUE;

		aSmtpServer->GetUsername(getter_Copies(userName));
    m_logonRedirector->RequiresPassword(userName, &requiresPassword);
    if (requiresPassword)
		  GetPassword(getter_Copies(password));

    nsCOMPtr<nsIPrompt> prompter;
    m_runningURL->GetPrompt(getter_AddRefs(prompter));
		rv = m_logonRedirector->Logon(userName, password, redirectionType, prompter, NS_STATIC_CAST(nsIMsgLogonRedirectionRequester *, this), nsMsgLogonRedirectionServiceIDs::Smtp);
	}

  // this protocol instance now needs to wait until
  // we receive the login redirection information so set the appropriate state
  // flag
  SetFlag(SMTP_WAIT_FOR_REDIRECTION);
  SetFlag(SMTP_USE_LOGIN_REDIRECTION);
  
  // even though we haven't started to send the message yet, 
  // we are going off and doing an asynch operation to get the redirection
  // information. So start the url as being run.
  nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(m_runningURL);
  // this will cause another dialog to get thrown up....
	mailNewsUrl->SetUrlState(PR_TRUE /* start running url */, NS_OK);
  UpdateStatus(NS_SMTP_CONNECTING_TO_SERVER);
  // and update the status

	return rv;
}

NS_IMETHODIMP nsSmtpProtocol::OnLogonRedirectionError(const PRUnichar *pErrMsg, PRBool aBadPassword)
{
  nsCOMPtr<nsISmtpServer> smtpServer;
  m_runningURL->GetSmtpServer(getter_AddRefs(smtpServer));
  
  NS_ENSURE_TRUE(smtpServer, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(m_logonRedirector, NS_ERROR_FAILURE);

  // step (1) force a log off...
  // logoff 
  nsXPIDLCString userName;
	smtpServer->GetUsername(getter_Copies(userName));
  m_logonRedirector->Logoff(userName);
  m_logonRedirector = nsnull; // we don't care about it anymore
	
  // step (2) alert the user about the error
  nsCOMPtr<nsIPrompt> dialog;
  if (m_runningURL && pErrMsg && pErrMsg[0]) 
  {
    m_runningURL->GetPrompt(getter_AddRefs(dialog));
    if (dialog)
      dialog->Alert(nsnull, pErrMsg);
  }

  // step (3) if they entered a bad password, forget about it!
  if (aBadPassword && smtpServer)
    smtpServer->ForgetPassword();

  // step (4) we need to let the originator of the send url request know that an
  // error occurred and we aren't sending the message...in our case, this will
  // force the user back into the compose window and they can try to send it 
  // again.
  
  // this will cause another dialog to get thrown up....
  nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(m_runningURL);
	mailNewsUrl->SetUrlState(PR_FALSE /* stopped running url */, NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER);
	return NS_OK;
}
  
  /* Logon Redirection Progress */
NS_IMETHODIMP nsSmtpProtocol::OnLogonRedirectionProgress(nsMsgLogonRedirectionState pState)
{
	return NS_OK;
}

  /* reply with logon redirection data. */
NS_IMETHODIMP nsSmtpProtocol::OnLogonRedirectionReply(const PRUnichar * aHost, unsigned short aPort, const char * aCookieData,  unsigned short aCookieSize)
{
  NS_ENSURE_ARG(aHost);
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsISmtpServer> smtpServer;
  m_runningURL->GetSmtpServer(getter_AddRefs(smtpServer));
  NS_ENSURE_TRUE(smtpServer, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(m_logonRedirector, NS_ERROR_FAILURE);
 
  // we used to logoff from the requestor but we don't want to do
  // that anymore in the success case. We want to end up caching the
  // external connection for the entire session.
  m_logonRedirector = nsnull; // we don't care about it anymore
	
  // remember the logon cookie
  mLogonCookie.Assign(aCookieData, aCookieSize);

  //currently the server isn't returning a valid auth logon capability
  // this line is just a HACK to force us to use auth login.
  SetFlag(SMTP_AUTH_LOGIN_ENABLED);
  // currently the account manager isn't properly setting the authMethod
  // preference for servers which require redirectors. This is another 
  // HACK ALERT....we'll force it to be set...
  m_prefAuthMethod = PREF_AUTH_ANY;

  // now that we have a host and port to connect to, 
  // open up the channel...
  // pass in "ssl" for the last arg if you want this to be over SSL
  nsCAutoString hostCStr; hostCStr.AssignWithConversion(aHost);
  PR_LOG(SMTPLogModule, PR_LOG_ALWAYS, ("SMTP Connecting to: %s on port %d.", hostCStr.get(), aPort));
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  nsCOMPtr<nsISmtpUrl> smtpUrl(do_QueryInterface(m_runningURL));
  if (smtpUrl)
      smtpUrl->GetNotificationCallbacks(getter_AddRefs(callbacks));
  rv = OpenNetworkSocketWithInfo(hostCStr.get(), aPort, nsnull, callbacks);

  // we are no longer waiting for a logon redirection reply
  ClearFlag(SMTP_WAIT_FOR_REDIRECTION);

  // check to see if we had a pending LoadUrl call...be sure to
  // do this after we clear the SMTP_WAIT_FOR_REDIRECTION flag =).
  nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
  if (TestFlag(SMTP_LOAD_URL_PENDING))
    rv = LoadUrl(url , mPendingConsumer);

  mPendingConsumer = nsnull; // we don't need to remember this anymore...

  // since we are starting this url load out of the normal loading process,
  // we probably should stop the url from running and throw up an error dialog
  // if for some reason we didn't successfully load the url...

  // we may want to always return NS_OK regardless of an error
  return rv;
}


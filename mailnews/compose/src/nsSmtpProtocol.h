/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSmtpProtocol_h___
#define nsSmtpProtocol_h___

#include "nsMsgProtocol.h"
#include "nsIStreamListener.h"
#include "nsXPIDLString.h"
#include "nsISmtpUrl.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIMsgLogonRedirector.h"
#include "nsIMsgStringService.h"
#include "nsMsgLineBuffer.h"
#include "nsIAuthModule.h"

#include "nsCOMPtr.h"

 /* states of the machine
 */
typedef enum _SmtpState {
SMTP_RESPONSE = 0,                                  // 0
SMTP_START_CONNECT,                                 // 1
SMTP_FINISH_CONNECT,                                // 2
SMTP_SEND_HELO_RESPONSE,                            // 3
SMTP_SEND_EHLO_RESPONSE,                            // 4
SMTP_SEND_VRFY_RESPONSE,                            // 5
SMTP_SEND_MAIL_RESPONSE,                            // 6
SMTP_SEND_RCPT_RESPONSE,                            // 7
SMTP_SEND_DATA_RESPONSE,                            // 8
SMTP_SEND_POST_DATA,                                // 9
SMTP_SEND_MESSAGE_RESPONSE,                         // 10
SMTP_DONE,                                          // 11
SMTP_ERROR_DONE,                                    // 12
SMTP_FREE,                                          // 13
SMTP_AUTH_LOGIN_STEP0_RESPONSE,                     // 14
SMTP_EXTN_LOGIN_RESPONSE,                           // 15
SMTP_SEND_AUTH_LOGIN_STEP0,                         // 16
SMTP_SEND_AUTH_LOGIN_STEP1,                         // 17
SMTP_SEND_AUTH_LOGIN_STEP2,                         // 18
SMTP_AUTH_LOGIN_RESPONSE,                           // 19
SMTP_TLS_RESPONSE,                                  // 20
SMTP_AUTH_EXTERNAL_RESPONSE,                        // 21
SMTP_AUTH_PROCESS_STATE,                            // 22
SMTP_AUTH_CRAM_MD5_CHALLENGE_RESPONSE               // 23
} SmtpState;

// State Flags (Note, I use the word state in terms of storing 
// state information about the connection (authentication, have we sent
// commands, etc. I do not intend it to refer to protocol state)
#define SMTP_PAUSE_FOR_READ             0x00000001  /* should we pause for the next read */
#define SMTP_EHLO_DSN_ENABLED           0x00000002
#define SMTP_AUTH_LOGIN_ENABLED         0x00000004
#define SMTP_AUTH_PLAIN_ENABLED         0x00000008
#define SMTP_AUTH_EXTERNAL_ENABLED      0x00000010
#define SMTP_EHLO_STARTTLS_ENABLED      0x00000020

// if we are using a login redirection
// and we are waiting for it to give us the
// host and port to connect to, then this flag
// will be set...
#define SMTP_WAIT_FOR_REDIRECTION       0x00000040
// if we are using login redirection and we received a load Url
// request while we were stil waiting for the redirection information
// then we'll look in this field 
#define SMTP_LOAD_URL_PENDING           0x00000080
// if we are using login redirection, then this flag will be set.
// Note, this is different than the flag for whether we are waiting
// for login redirection information.
#define SMTP_USE_LOGIN_REDIRECTION      0x00000100
#define SMTP_ESMTP_SERVER               0x00000200
#define SMTP_AUTH_CRAM_MD5_ENABLED      0x00000400
#define SMTP_AUTH_DIGEST_MD5_ENABLED    0x00000800
#define SMTP_AUTH_NTLM_ENABLED          0x00001000
#define SMTP_AUTH_MSN_ENABLED           0x00002000
#define SMTP_AUTH_ANY_ENABLED           0x00003C1C
#define SMTP_EHLO_SIZE_ENABLED          0x00004000

typedef enum _PrefAuthMethod {
    PREF_AUTH_NONE = 0,
    PREF_AUTH_ANY = 1
} PrefAuthMethod;

typedef enum _PrefTrySSL {
    PREF_SECURE_NEVER = 0,
    PREF_SECURE_TRY_STARTTLS = 1,
    PREF_SECURE_ALWAYS_STARTTLS = 2,
    PREF_SECURE_ALWAYS_SMTPS = 3
} PrefTrySSL;

class nsSmtpProtocol : public nsMsgAsyncWriteProtocol,
                       public nsIMsgLogonRedirectionRequester
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIMSGLOGONREDIRECTIONREQUESTER

    // Creating a protocol instance requires the URL which needs to be run.
    nsSmtpProtocol(nsIURI * aURL);
    virtual ~nsSmtpProtocol();

    virtual nsresult LoadUrl(nsIURI * aURL, nsISupports * aConsumer = nsnull);
    virtual PRInt32 SendData(nsIURI * aURL, const char * dataBuffer, PRBool aSuppressLogging = PR_FALSE);

    ////////////////////////////////////////////////////////////////////////////////////////
    // we suppport the nsIStreamListener interface 
    ////////////////////////////////////////////////////////////////////////////////////////

    // stop binding is a "notification" informing us that the stream associated with aURL is going away. 
    NS_IMETHOD OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status);

private:
    // logon redirection related variables and methods
    nsresult RequestOverrideInfo(nsISmtpServer * aSmtpServer); // kicks off the request to get redirection info for the server
    nsCString mLogonCookie; // an opaque cookie we pass to certain servers to logon
    // if we are asked to load a url while we are blocked waiting for redirection information,
    // then we'll store the url consumer in mPendingConsumer until we can actually load
    // the url.
    nsCOMPtr<nsISupports> mPendingConsumer;
    // we cache the logon redirector in the short term so after we receive
    // the redirect information we can logoff the redirector...
    nsCOMPtr <nsIMsgLogonRedirector> m_logonRedirector;

    // the nsISmtpURL that is currently running
    nsCOMPtr<nsISmtpUrl> m_runningURL;

    // the error state we want to set on the url
    nsresult m_urlErrorState;
    PRUint32 m_LastTime;
    nsCOMPtr<nsIMsgStatusFeedback> m_statusFeedback;

    // Generic state information -- What state are we in? What state do we want to go to
    // after the next response? What was the last response code? etc. 
    SmtpState m_nextState;
    SmtpState m_nextStateAfterResponse;
    PRInt32 m_responseCode;    /* code returned from Smtp server */
    PRInt32 m_previousResponseCode; 
    PRInt32 m_continuationResponse;
    nsCString m_responseText;   /* text returned from Smtp server */
    nsMsgLineStreamBuffer *m_lineStreamBuffer; // used to efficiently extract lines from the incoming data stream

    char	   *m_addressCopy;
    char	   *m_addresses;
    PRUint32	m_addressesLeft;
    char	   *m_verifyAddress;
    nsXPIDLCString m_mailAddr;
    PRInt32 m_sizelimit;

    // *** the following should move to the smtp server when we support
    // multiple smtp servers
    PRInt32 m_prefAuthMethod;
    PRBool m_prefTrySecAuth;
    PRBool m_usernamePrompted;
    PRInt32 m_prefTrySSL;
    PRBool m_tlsEnabled;
  
    PRBool m_tlsInitiated;

    PRBool m_sendDone;

    PRInt32 m_totalAmountRead;
#ifdef UNREADY_CODE 
    // message specific information
    PRInt32 m_totalAmountWritten;
#endif /* UNREADY_CODE */
    PRUint32 m_totalMessageSize;
    
    char *m_dataBuf;
    PRUint32 m_dataBufSize;

    PRInt32   m_originalContentLength; /* the content length at the time of calling graph progress */

    // initialization function given a new url and transport layer
    void Initialize(nsIURI * aURL);
    virtual nsresult ProcessProtocolState(nsIURI * url, nsIInputStream * inputStream, 
                                          PRUint32 sourceOffset, PRUint32 length);

    ////////////////////////////////////////////////////////////////////////////////////////
    // Communication methods --> Reading and writing protocol
    ////////////////////////////////////////////////////////////////////////////////////////

    nsCOMPtr<nsIMsgStringService> mSmtpBundle;
    void UpdateStatus(PRInt32 aStatusID);
    void UpdateStatusWithString(const PRUnichar * aStatusString);

    ////////////////////////////////////////////////////////////////////////////////////////
    // Protocol Methods --> This protocol is state driven so each protocol method is 
    //						designed to re-act to the current "state". I've attempted to 
    //						group them together based on functionality. 
    ////////////////////////////////////////////////////////////////////////////////////////

    PRInt32 SmtpResponse(nsIInputStream * inputStream, PRUint32 length); 
    PRInt32 ExtensionLoginResponse(nsIInputStream * inputStream, PRUint32 length);
    PRInt32 SendHeloResponse(nsIInputStream * inputStream, PRUint32 length);
    PRInt32 SendEhloResponse(nsIInputStream * inputStream, PRUint32 length);	

    PRInt32 AuthLoginStep0();
    PRInt32 AuthLoginStep0Response();
    PRInt32 AuthLoginStep1();
    PRInt32 AuthLoginStep2();
    PRInt32 AuthLoginResponse(nsIInputStream * stream, PRUint32 length);

    PRInt32 SendTLSResponse();
    PRInt32 SendVerifyResponse(); // mscott: this one is apparently unimplemented...
    PRInt32 SendMailResponse();
    PRInt32 SendRecipientResponse();
    PRInt32 SendDataResponse();
    PRInt32 SendPostData();
    PRInt32 SendMessageResponse();
    PRInt32 CramMD5LoginResponse();
    PRInt32 ProcessAuth();


    ////////////////////////////////////////////////////////////////////////////////////////
    // End of Protocol Methods
    ////////////////////////////////////////////////////////////////////////////////////////

    PRInt32 SendMessageInFile();

    void GetUserDomainName(nsACString& domainName);
    nsresult GetPassword(char **aPassword);
    nsresult GetUsernamePassword(char **aUsername, char **aPassword);
    nsresult PromptForPassword(nsISmtpServer *aSmtpServer, nsISmtpUrl *aSmtpUrl, const PRUnichar **formatStrings, char **aPassword);

    void BackupAuthFlags();
    void RestoreAuthFlags();
    PRInt32 m_origAuthFlags;
};

#endif  // nsSmtpProtocol_h___

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsMsgSend.h"

#include "nsCRT.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsMsgSendPart.h"
#include "nsMsgBaseCID.h"
#include "nsMsgNewsCID.h"
#include "nsIMsgHeaderParser.h"
#include "nsISmtpService.h"  // for actually sending the message...
#include "nsINntpService.h"  // for actually posting the message...
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsEscape.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgDeliveryListener.h"
#include "nsMsgComposeStringBundle.h"
#include "nsMsgEncoders.h"
#include "nsMsgCompUtils.h"
#include "nsMsgI18N.h"
#include "nsICharsetConverterManager.h"
#include "nsIMsgSendListener.h"
#include "nsIMsgCopyServiceListener.h"
#include "nsIFileSpec.h"
#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsMsgCopy.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsMsgPrompts.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsMsgCompCID.h"
#include "nsIFileSpec.h"
#include "nsIAbAddressCollecter.h"
#include "nsAbBaseCID.h"
#include "nsCOMPtr.h"
#include "nsIDNSService.h"
#include "mozITXTToHTMLConv.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIMsgMailSession.h"
#include "nsTextFormatter.h"
#include "nsIWebShell.h"
#include "nsIPrompt.h"
#include "nsIAppShellService.h"
#include "nsMailHeaders.h"
#include "nsIDocShell.h"
#include "nsMimeTypes.h"
#include "nsISmtpUrl.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIEditorMailSupport.h"
#include "nsIDocumentEncoder.h"    // for editor output flags
#include "nsILoadGroup.h"
#include "nsMsgSendReport.h"
#include "nsMsgSimulateError.h"
#include "nsNetCID.h"
#include "nsNetError.h"
#include "nsMsgUtils.h"
#include "nsIRDFService.h"
#include "nsIMsgMdnGenerator.h"
#include "nsISmtpServer.h"
#include "nsIMsgCompose.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIMsgAccountManager.h"
#include "nsNativeCharsetUtils.h"
#include "nsIAbCard.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

#define PREF_MAIL_SEND_STRUCT "mail.send_struct"
#define PREF_MAIL_STRICTLY_MIME "mail.strictly_mime"
#define PREF_MAIL_MESSAGE_WARNING_SIZE "mailnews.message_warning_size"
#define PREF_MAIL_COLLECT_EMAIL_ADDRESS_OUTGOING "mail.collect_email_address_outgoing"

enum  { kDefaultMode = (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE) };

#ifdef XP_MAC

static char* NET_GetLocalFileFromURL(char *url)
{
  char * finalPath;
  NS_ASSERTION(PL_strncasecmp(url, "file://", 7) == 0, "invalid url");
  finalPath = (char*)PR_Malloc(strlen(url));
  if (finalPath == nsnull)
    return nsnull;
  strcpy(finalPath, url+6+1);
  return finalPath;
}

static char* NET_GetURLFromLocalFile(char *filename)
{
    /*  file:///<path>0 */
  char * finalPath = (char*)PR_Malloc(strlen(filename) + 8 + 1);
  if (finalPath == nsnull)
    return nsnull;
  finalPath[0] = 0;
  strcat(finalPath, "file://");
  strcat(finalPath, filename);
  return finalPath;
}

#endif /* XP_MAC */

static PRBool mime_use_quoted_printable_p = PR_FALSE;

//
// Ugh, we need to do this currently to access this boolean.
//
PRBool
UseQuotedPrintable(void)
{
  return mime_use_quoted_printable_p;
}

/* This function will parse a list of email addresses and groups and just
 * return a list of email addresses (recipient)
 *
 * The input could be:
 *    [recipient | group] *[,recipient | group]
 *
 * The group syntax is:
 *    group-name:[recipient *[,recipient]];
 *
 * the output will be:
 *    recipient *[, recipient]
 *
 * As the result will always be equal or smaller than the input string,
 * the extraction will be made in place. Don't need to create a new buffer.
 */
static nsresult StripOutGroupNames(char * addresses)
{
  char aChar;
  char * readPtr = addresses;           // current read position
  char * writePtr = addresses;          // current write position
  char * previousSeparator = addresses; // remember last time we wrote a recipient separator
  char * endPtr = addresses + PL_strlen(addresses);

  PRBool quoted = PR_FALSE;   // indicate if we are between double quote
  PRBool group = PR_FALSE;   // indicate if we found a group prefix
  PRBool atFound = PR_FALSE;  // indicate if we found an @ in the current recipient. group name should not have an @

  while (readPtr < endPtr)
  {
    aChar = *readPtr;
    readPtr ++;
    switch(aChar)
    {
      case '\\':
        if (*readPtr == '"') //ignore escaped quote
          readPtr ++;
        continue;

      case '"':
        quoted = !quoted;
        break;

      case '@':
        if (!quoted)
          atFound = PR_TRUE;
        break;

      case ':':
        if (!quoted && !atFound)
        {
          // ok, we found a group name
          // let's backup the write cursor to remove the group name
          writePtr = previousSeparator + 1;
          group = PR_TRUE;
          continue;
        }
        break;

      case ';':
        if (quoted || !group)
          break;
        else
          group = PR_FALSE;
          //end of the group, act like a recipient separator now...
        /* NO BREAK */

      case ',':
        if (!quoted)
        {
          atFound = PR_FALSE;
          //let check if we already have a comma separator in the output string
          if (writePtr > addresses && *(writePtr - 1) == ',')
            writePtr --;
          *writePtr = ',';
          previousSeparator = writePtr;
          writePtr ++;
          continue;
        }
        break;
    }
    *writePtr = aChar;
    writePtr ++;
  }

  if (writePtr > addresses && *(writePtr - 1) == ',')
    writePtr --;
  *writePtr = '\0';

  return NS_OK;
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMsgComposeAndSend, nsIMsgSend)

nsMsgComposeAndSend::nsMsgComposeAndSend() : 
    m_messageKey(0xffffffff)
{
#if defined(DEBUG_ducarroz)
  printf("CREATE nsMsgComposeAndSend: %x\n", this);
#endif
  mGUINotificationEnabled = PR_TRUE;
  mAbortInProcess = PR_FALSE;
  mLastErrorReported = NS_OK;
  mEditor = nsnull;
  mMultipartRelatedAttachmentCount = -1;
  mCompFields = nsnull;     /* Where to send the message once it's done */
  mSendMailAlso = PR_FALSE;
  mOutputFile = nsnull;

  m_dont_deliver_p = PR_FALSE;
  m_deliver_mode = nsMsgDeliverNow;

  m_attachments_only_p = PR_FALSE;
  m_pre_snarfed_attachments_p = PR_FALSE;
  m_digest_p = PR_FALSE;
  m_be_synchronous_p = PR_FALSE;
  m_crypto_closure = nsnull;
  m_attachment1_type = 0;
  m_attachment1_encoding = 0;
  m_attachment1_encoder_data = nsnull;
  m_attachment1_body = 0;
  m_attachment1_body_length = 0;
  m_attachment_count = 0;
  m_attachment_pending_count = 0;
  m_attachments = nsnull;
  m_status = 0;
  m_attachments_done_callback = nsnull;
  m_plaintext = nsnull;
  m_related_part = nsnull;
  m_related_body_part = nsnull;
  mOriginalHTMLBody = nsnull;

  // These are for temp file creation and return
  mReturnFileSpec = nsnull;
  mTempFileSpec = nsnull;
  mHTMLFileSpec = nsnull;
  mCopyFileSpec = nsnull;
  mCopyFileSpec2 = nsnull;
  mCopyObj = nsnull;
  mNeedToPerformSecondFCC = PR_FALSE;

  mPreloadedAttachmentCount = 0;
  mRemoteAttachmentCount = 0;
  mCompFieldLocalAttachments = 0;
  mCompFieldRemoteAttachments = 0;
  mMessageWarningSize = 0;
  
  NS_NEWXPCOM(mSendReport, nsMsgSendReport);
}

nsMsgComposeAndSend::~nsMsgComposeAndSend()
{
#if defined(DEBUG_ducarroz)
  printf("DISPOSE nsMsgComposeAndSend: %x\n", this);
#endif
  mSendReport = nsnull;
  Clear();
}

NS_IMETHODIMP nsMsgComposeAndSend::GetDefaultPrompt(nsIPrompt ** aPrompt)
{
  NS_ENSURE_ARG(aPrompt);
  *aPrompt = nsnull;
  
  nsresult rv = NS_OK;

  if (mSendProgress)
  {
    rv = mSendProgress->GetPrompter(aPrompt);
    if (NS_SUCCEEDED(rv) && *aPrompt)
      return NS_OK;
  }
  
  if (mParentWindow)
  {
    rv = mParentWindow->GetPrompter(aPrompt);
    if (NS_SUCCEEDED(rv) && *aPrompt)
      return NS_OK;
  }
  
  /* If we cannot find a prompter, try the mail3Pane window */
  nsCOMPtr<nsIMsgWindow> msgWindow;
  nsCOMPtr <nsIMsgMailSession> mailSession (do_GetService(NS_MSGMAILSESSION_CONTRACTID));
  if (mailSession)
  {
    mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
    if (msgWindow)
      rv = msgWindow->GetPromptDialog(aPrompt);
  }
  
  return rv;
}

nsresult nsMsgComposeAndSend::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks)
{
// TODO: stop using mail3pane window!
  nsCOMPtr<nsIMsgWindow> msgWindow;
  nsCOMPtr<nsIMsgMailSession> mailSession(do_GetService(NS_MSGMAILSESSION_CONTRACTID));
  mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
  if (msgWindow) {
    nsCOMPtr<nsIDocShell> docShell;
    msgWindow->GetRootDocShell(getter_AddRefs(docShell));
    nsCOMPtr<nsIInterfaceRequestor> ir(do_QueryInterface(docShell));
    if (ir) {
      *aCallbacks = ir;
      NS_ADDREF(*aCallbacks);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;  
}

void 
nsMsgComposeAndSend::Clear()
{
#ifdef NS_DEBUG
  printf("\nTHE CLEANUP ROUTINE FOR nsMsgComposeAndSend() WAS CALLED\n");
#endif
  PR_FREEIF (m_attachment1_type);
  PR_FREEIF (m_attachment1_encoding);
  PR_FREEIF (m_attachment1_body);
  PR_FREEIF (mOriginalHTMLBody);

  if (m_attachment1_encoder_data) 
  {
    MIME_EncoderDestroy(m_attachment1_encoder_data, PR_TRUE);
    m_attachment1_encoder_data = 0;
  }

  if (m_plaintext) 
  {
    if (m_plaintext->mOutFile)
      m_plaintext->mOutFile->Close();

    if (m_plaintext->mFileSpec)
    {
      m_plaintext->mFileSpec->Delete(PR_FALSE);
      delete m_plaintext->mFileSpec;
      m_plaintext->mFileSpec = nsnull;
    }
    delete m_plaintext;
    m_plaintext = nsnull;
  }

  if (mHTMLFileSpec) 
  {
    mHTMLFileSpec->Delete(PR_FALSE);
    delete mHTMLFileSpec;
    mHTMLFileSpec= nsnull;
  }

  if (mOutputFile) 
  {
    delete mOutputFile;
    mOutputFile = 0;
  }

  if (mCopyFileSpec)
  {
    nsFileSpec aFileSpec;
    mCopyFileSpec->GetFileSpec(&aFileSpec);
    if (aFileSpec.Valid())
      aFileSpec.Delete(PR_FALSE);

    // jt -- *don't* use delete someone may still holding the nsIFileSpec
    // pointer
    NS_IF_RELEASE(mCopyFileSpec);
  }

  if (mCopyFileSpec2)
  {
    nsFileSpec aFileSpec;
    mCopyFileSpec2->GetFileSpec(&aFileSpec);
    if (aFileSpec.Valid())
      aFileSpec.Delete(PR_FALSE);

    // jt -- *don't* use delete someone may still holding the nsIFileSpec
    // pointer
    NS_IF_RELEASE(mCopyFileSpec2);
  }

  if (mTempFileSpec) 
  {
    if (mReturnFileSpec == nsnull)
    {
      mTempFileSpec->Delete(PR_FALSE);
      delete mTempFileSpec;
      mTempFileSpec = nsnull;
    }
  }

  if (m_attachments)
  {
    PRUint32 i;
    for (i = 0; i < m_attachment_count; i++) 
    {
      if (m_attachments [i].m_encoder_data) 
      {
        MIME_EncoderDestroy(m_attachments[i].m_encoder_data, PR_TRUE);
        m_attachments [i].m_encoder_data = 0;
      }

      m_attachments[i].mURL = nsnull;

      // is this needed? the destructor for the attachments
      // should be freeing these strings.
      PR_FREEIF (m_attachments [i].m_type);
      PR_FREEIF (m_attachments [i].m_charset);
      PR_FREEIF (m_attachments [i].m_override_type);
      PR_FREEIF (m_attachments [i].m_override_encoding);
      PR_FREEIF (m_attachments [i].m_desired_type);
      PR_FREEIF (m_attachments [i].m_description);
      PR_FREEIF (m_attachments [i].m_x_mac_type);
      PR_FREEIF (m_attachments [i].m_x_mac_creator);
      PR_FREEIF (m_attachments [i].m_real_name);
      PR_FREEIF (m_attachments [i].m_encoding);
      PR_FREEIF (m_attachments [i].m_content_id);
      if (m_attachments[i].mOutFile)
          m_attachments[i].mOutFile->Close();
      if (m_attachments[i].mFileSpec) 
      {
        // Only Delete the file if this variable is set!
        if (m_attachments[i].mDeleteFile)
          m_attachments[i].mFileSpec->Delete(PR_FALSE);
        delete m_attachments[i].mFileSpec;
        m_attachments[i].mFileSpec = nsnull;
      }

#if defined(XP_MAC) || defined(XP_MACOSX)
      //
      // remove the appledoubled intermediate file after we done all.
      //
      if (m_attachments[i].mAppleFileSpec) 
      {
        m_attachments[i].mAppleFileSpec->Delete(PR_FALSE);
        delete m_attachments[i].mAppleFileSpec;
        m_attachments[i].mAppleFileSpec = nsnull;
      }
#endif /* XP_MAC */
    }

    delete[] m_attachments;
    m_attachment_count = m_attachment_pending_count = 0;
    m_attachments = 0;
  }

  // Cleanup listener
  mListener = nsnull;
}

static char *mime_mailto_stream_read_buffer = 0;
static char *mime_mailto_stream_write_buffer = 0;


char * mime_get_stream_write_buffer(void)
{
  if (!mime_mailto_stream_write_buffer)
    mime_mailto_stream_write_buffer = (char *) PR_Malloc(MIME_BUFFER_SIZE);
  return mime_mailto_stream_write_buffer;
}

static PRBool isEmpty( const char* _pHeaderLine )
{
	return ( nsnull == _pHeaderLine ) || ( 0 == *_pHeaderLine );
}

void nsMsgComposeAndSend::GenerateMessageId( )
{
  // When only saving the message, do not generate an id
  // This is because when really sending the message later, the characteristics which control the
  // generation may have changed, so we want to re-check them then (which we would not do if there is
  // already an id present).
  if  (   ( nsIMsgCompDeliverMode::Now == m_deliver_mode )
      ||  ( nsIMsgCompDeliverMode::Later == m_deliver_mode )
      )
  {
    // is there already a message id?
    if ( isEmpty( mCompFields->GetMessageId() ) )
    { // nope. check if we need to generate one

      // spec:
      // * if there is at least one mail target, we always generate an id
      // * if there are only news targets, we check the preference "generate_news_message_id"
      // * (then) if and only if the preference exists and is set to true, we do generate an id

      if  (   isEmpty( mCompFields->GetTo() )
          &&  isEmpty( mCompFields->GetCc() )
          &&  isEmpty( mCompFields->GetBcc() )
          ) 
      { // no mail target. assume news targets (messages without both mail and news targets
        // would not make sense, would they?)

        // what does the identity say 'bout message id generation?
        PRBool bGenerateMessageId = PR_FALSE;
        mUserIdentity->GetBoolAttribute( "generate_news_message_id", &bGenerateMessageId );
          // Note: if the preference is not found, this defaults to PR_FALSE. As we do not initially write
          // this pref when deploying Mozilla, this means that by default, no ids are generated for news targets.
        if ( !bGenerateMessageId )
          return;
      }

      // here we are, finally required to generate an id ....
      char* msgID = msg_generate_message_id( mUserIdentity );
      mCompFields->SetMessageId( msgID );
      PR_FREEIF( msgID );
    }
  }
}

// Don't I18N this line...this is per the spec!
#define   MIME_MULTIPART_BLURB     "This is a multi-part message in MIME format."

/* All of the desired attachments have been written to individual temp files,
   and we know what's in them.  Now we need to make a final temp file of the
   actual mail message, containing all of the other files after having been
   encoded as appropriate.
 */
NS_IMETHODIMP 
nsMsgComposeAndSend::GatherMimeAttachments()
{
  PRBool shouldDeleteDeliveryState = PR_TRUE;
  PRInt32 status;
  PRUint32    i;
  char *headers = 0;
  PRFileDesc  *in_file = 0;
  PRBool multipart_p = PR_FALSE;
  PRBool plaintext_is_mainbody_p = PR_FALSE; // only using text converted from HTML?
  char *buffer = 0;
  char *buffer_tail = 0;
  nsXPIDLString msg; 
  PRBool tonews;
  PRBool body_is_us_ascii = PR_TRUE;

  nsMsgSendPart* toppart = nsnull;      // The very top most container of the message
                      // that we are going to send.

  nsMsgSendPart* mainbody = nsnull;     // The leaf node that contains the text of the
                      // message we're going to contain.

  nsMsgSendPart* maincontainer = nsnull;  // The direct child of toppart that will
                      // contain the mainbody.  If mainbody is
                      // the same as toppart, then this is
                      // also the same.  But if mainbody is
                      // to end up somewhere inside of a
                      // multipart/alternative or a
                      // multipart/related, then this is that
                      // multipart object.

  nsMsgSendPart* plainpart = nsnull;    // If we converted HTML into plaintext,
                      // the message or child containing the plaintext
                      // goes here. (Need to use this to determine
                      // what headers to append/set to the main 
                      // message body.)

  PRUint32 multipartRelatedCount = GetMultipartRelatedCount(); // The number of related part we will have to generate

  nsCOMPtr<nsIPrompt> promptObject; // only used if we have to show an alert here....
  GetDefaultPrompt(getter_AddRefs(promptObject));

  char *hdrs = 0;
  PRBool maincontainerISrelatedpart = PR_FALSE;
  const char * toppart_type = nsnull;

  // If we have any attachments, we generate multipart.
  multipart_p = (m_attachment_count > 0);

  // to news is true if we have a m_field and we have a Newsgroup and it is not empty
  tonews = PR_FALSE;
  if (mCompFields) 
  {
    const char* pstrzNewsgroup = mCompFields->GetNewsgroups();
    if (pstrzNewsgroup && *pstrzNewsgroup)
      tonews = PR_TRUE;
  }

  status = m_status;
  if (status < 0)
    goto FAIL;

  if (m_attachments_only_p)
  {
    /* If we get here, we should be fetching attachments only! */
    if (m_attachments_done_callback) 
    {
      struct nsMsgAttachedFile *attachments;

      NS_ASSERTION(m_attachment_count > 0, "not more attachment");
      if (m_attachment_count <= 0) 
      {
        m_attachments_done_callback (nsnull, nsnull, nsnull);
        m_attachments_done_callback = nsnull;
        goto FAIL;
      }

      attachments = (struct nsMsgAttachedFile *)PR_Malloc((m_attachment_count + 1) * sizeof(*attachments));

      if (!attachments)
        goto FAILMEM;
      memset(attachments, 0, ((m_attachment_count + 1) * sizeof(*attachments)));
      for (i = 0; i < m_attachment_count; i++)
      {
        nsMsgAttachmentHandler *ma = &m_attachments[i];

#undef SNARF
#define SNARF(x,y) do { if((y) && *(y) && !(x)) { ((x) = (y)); ((y) = 0); }} \
           while(0)
        // Rather than copying the strings and dealing with allocation
        // failures, we'll just "move" them into the other struct (this
        // should be ok since this file uses PR_FREEIF when discarding
        // the mime_attachment objects.) 
        //
        attachments[i].orig_url = ma->mURL;
        attachments[i].file_spec = ma->mFileSpec;

        SNARF(attachments[i].type, ma->m_type);
        SNARF(attachments[i].encoding, ma->m_encoding);
        SNARF(attachments[i].description, ma->m_description);
        SNARF(attachments[i].x_mac_type, ma->m_x_mac_type);
        SNARF(attachments[i].x_mac_creator, ma->m_x_mac_creator);

#undef SNARF
        attachments[i].size = ma->m_size;
        attachments[i].unprintable_count = ma->m_unprintable_count;
        attachments[i].highbit_count = ma->m_highbit_count;
        attachments[i].ctl_count = ma->m_ctl_count;
        attachments[i].null_count = ma->m_null_count;
        attachments[i].max_line_length = ma->m_max_column;

        /* Doesn't really matter, but let's not lie about encoding
           in case it does someday. */
        if (attachments[i].highbit_count > 0 && attachments[i].encoding &&
            !PL_strcasecmp(attachments[i].encoding, ENCODING_7BIT))
          attachments[i].encoding = ENCODING_8BIT;
      }

      m_attachments_done_callback(nsnull, nsnull, attachments);
      PR_FREEIF(attachments);
      m_attachments_done_callback = nsnull;
    }
    goto FAIL;
  }


  /* If we get here, we're generating a message, so there shouldn't be an
     attachments callback. */
  NS_ASSERTION(!m_attachments_done_callback, "shouldn't have an attachement callback!");

  if (!m_attachment1_type) {
    m_attachment1_type = PL_strdup(TEXT_PLAIN);
    if (!m_attachment1_type)
      goto FAILMEM;
  }

  // If we have a text/html main part, and we need a plaintext attachment, then
  // we'll do so now.  This is an asynchronous thing, so we'll kick it off and
  // count on getting back here when it finishes.

  if (m_plaintext == nsnull &&
      (mCompFields->GetForcePlainText() ||
       mCompFields->GetUseMultipartAlternative()) &&
       m_attachment1_body && PL_strcmp(m_attachment1_type, TEXT_HTML) == 0)
  {
    //
    // If we get here, we have an HTML body, but we really need to send
    // a text/plain message, so we will write the HTML out to a disk file,
    // fire off another URL request for this local disk file and that will
    // take care of the conversion...
    //
    mHTMLFileSpec = nsMsgCreateTempFileSpec("nsmail.html");
    if (!mHTMLFileSpec)
      goto FAILMEM;

    nsOutputFileStream tempfile(*mHTMLFileSpec, kDefaultMode, 00600);
    if (! tempfile.is_open()) 
    {
      if (mSendReport)
      {
        nsAutoString error_msg;
        nsAutoString path;
        NS_CopyNativeToUnicode(
          nsDependentCString(mHTMLFileSpec->GetNativePathCString()), path);
        nsMsgBuildErrorMessageByID(NS_MSG_UNABLE_TO_OPEN_TMP_FILE, error_msg, &path, nsnull);
        mSendReport->SetMessage(nsIMsgSendReport::process_Current, error_msg.get(), PR_FALSE);
      }
      status = NS_MSG_UNABLE_TO_OPEN_TMP_FILE;
      goto FAIL;
    }

    if (mOriginalHTMLBody)
    {
      PRUint32    origLen = strlen(mOriginalHTMLBody);
      status = tempfile.write(mOriginalHTMLBody, origLen);
      if (status < int(origLen)) 
      {
        if (status >= 0)
          status = NS_MSG_ERROR_WRITING_FILE;
        goto FAIL;
      }
    }

    if (NS_FAILED(tempfile.flush()) || tempfile.failed())
    {
      status = NS_MSG_ERROR_WRITING_FILE;
      goto FAIL;
    }

    tempfile.close();

    m_plaintext = new nsMsgAttachmentHandler;
    if (!m_plaintext)
      goto FAILMEM;
    m_plaintext->SetMimeDeliveryState(this);
    m_plaintext->m_bogus_attachment = PR_TRUE;

    char *tempURL = nsMsgPlatformFileToURL (*mHTMLFileSpec);
    if (!tempURL || NS_FAILED(nsMsgNewURL(getter_AddRefs(m_plaintext->mURL), tempURL)))
    {
      delete m_plaintext;
      m_plaintext = nsnull;
      goto FAILMEM;
    }
  
    PR_FREEIF(tempURL);

    PR_FREEIF(m_plaintext->m_type);
    m_plaintext->m_type = PL_strdup(TEXT_HTML);
    PR_FREEIF(m_plaintext->m_charset);
    m_plaintext->m_charset = PL_strdup(mCompFields->GetCharacterSet());
    PR_FREEIF(m_plaintext->m_desired_type);
    m_plaintext->m_desired_type = PL_strdup(TEXT_PLAIN);
    m_attachment_pending_count ++;
    status = m_plaintext->SnarfAttachment(mCompFields);
    if (NS_FAILED(status))
      goto FAIL;
    if (m_attachment_pending_count > 0)
      return NS_OK;
  }
    
  /* Kludge to avoid having to allocate memory on the toy computers... */
  buffer = mime_get_stream_write_buffer();
  if (! buffer)
    goto FAILMEM;

  buffer_tail = buffer;

  NS_ASSERTION (m_attachment_pending_count == 0, "m_attachment_pending_count != 0");

  mComposeBundle->GetStringByID(NS_MSG_ASSEMBLING_MSG, getter_Copies(msg));
  SetStatusMessage( msg );

  /* First, open the message file.
  */
  mTempFileSpec = nsMsgCreateTempFileSpec("nsmail.eml");
  if (! mTempFileSpec)
    goto FAILMEM;

  mOutputFile = new nsOutputFileStream(*mTempFileSpec, kDefaultMode, 00600);
  if (! mOutputFile->is_open() || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_4)) 
  {
    status = NS_MSG_UNABLE_TO_OPEN_TMP_FILE;
    if (mSendReport)
    {
      nsAutoString error_msg;
      nsAutoString path;
      NS_CopyNativeToUnicode(
        nsDependentCString(mTempFileSpec->GetNativePathCString()), path);
      nsMsgBuildErrorMessageByID(NS_MSG_UNABLE_TO_OPEN_TMP_FILE, error_msg, &path, nsnull);
      mSendReport->SetMessage(nsIMsgSendReport::process_Current, error_msg.get(), PR_FALSE);
    }
    goto FAIL;
  }
  
  // generate a message id, if necessary
  GenerateMessageId( );

  mainbody = new nsMsgSendPart(this, mCompFields->GetCharacterSet());
  if (!mainbody)
    goto FAILMEM;

  mainbody->SetMainPart(PR_TRUE);
  mainbody->SetType(m_attachment1_type ? m_attachment1_type : TEXT_PLAIN);

  NS_ASSERTION(mainbody->GetBuffer() == nsnull, "not-null buffer");
  status = mainbody->SetBuffer(m_attachment1_body ? m_attachment1_body : "");
  if (status < 0)
    goto FAIL;

  /*
    Determine the encoding of the main message body before we free it.
    The proper way to do this should be to test whatever text is in mainbody
    just before writing it out, but that will require a fix that is less safe
    and takes more memory. */
  PR_FREEIF(m_attachment1_encoding);
  if (m_attachment1_body)
    mCompFields->GetBodyIsAsciiOnly(&body_is_us_ascii);

  if (nsMsgI18Nstateful_charset(mCompFields->GetCharacterSet()) ||
      body_is_us_ascii)
    m_attachment1_encoding = PL_strdup (ENCODING_7BIT);
  else if (mime_use_quoted_printable_p)
    m_attachment1_encoding = PL_strdup (ENCODING_QUOTED_PRINTABLE);
  else
    m_attachment1_encoding = PL_strdup (ENCODING_8BIT);

  PR_FREEIF (m_attachment1_body);

  maincontainer = mainbody;

  // If we were given a pre-saved collection of HTML and contained images,
  // then we want mainbody to point to the HTML lump therein.
  if (m_related_part)
  {
    // If m_related_part is of type text/html, set both maincontainer
    // and mainbody to point to it. If m_related_part is multipart/related,
    // however, set mainbody to be the first child within m_related_part.
    delete mainbody;

    // No matter what, maincontainer points to the outermost related part.
    maincontainer = m_related_part;
    maincontainerISrelatedpart = PR_TRUE;

    mainbody = m_related_part->GetChild(0);
    mainbody->SetMainPart(PR_TRUE);
  }

  if (m_plaintext) 
  {
    //
    // OK.  We have a plaintext version of the main body that we want to
    // send instead of or with the text/html.  Shove it in.
    //
    plainpart = new nsMsgSendPart(this, mCompFields->GetCharacterSet());
    if (!plainpart)
      goto FAILMEM;
    status = plainpart->SetType(TEXT_PLAIN);
    if (status < 0)
      goto FAIL;
    status = plainpart->SetFile(m_plaintext->mFileSpec);
    if (status < 0)
      goto FAIL;

    m_plaintext->mMainBody = PR_TRUE;

    m_plaintext->AnalyzeSnarfedFile(); // look for 8 bit text, long lines, etc.
    m_plaintext->PickEncoding(mCompFields->GetCharacterSet(), this);
    const char *charset = mCompFields->GetCharacterSet();
    hdrs = mime_generate_attachment_headers(m_plaintext->m_type,
                        nsnull,
                        m_plaintext->m_encoding,
                        m_plaintext->m_description,
                        m_plaintext->m_x_mac_type,
                        m_plaintext->m_x_mac_creator,
                        nsnull, 0,
                        m_digest_p,
                        m_plaintext,
                        charset,
                        charset,
                        mCompFields->GetBodyIsAsciiOnly(),
                        nsnull,
                        PR_TRUE);
    if (!hdrs)
      goto FAILMEM;
    status = plainpart->SetOtherHeaders(hdrs);
    PR_Free(hdrs);
    hdrs = nsnull;
    if (status < 0)
      goto FAIL;

    if (mCompFields->GetUseMultipartAlternative()) 
    {
      nsMsgSendPart* htmlpart = maincontainer;
      maincontainer = new nsMsgSendPart(this);
      if (!maincontainer)
        goto FAILMEM;

      // Setup the maincontainer stuff...
      status = maincontainer->SetType(MULTIPART_ALTERNATIVE);
      if (status < 0)
        goto FAIL;

      status = maincontainer->AddChild(plainpart);
      if (status < 0)
        goto FAIL;

      status = maincontainer->AddChild(htmlpart);
      if (status < 0)
        goto FAIL;

      // Create the encoder for the plaintext part here,
      // because we aren't the main part (attachment1).
      // (This, along with the rest of the routine, should really
      // be restructured so that no special treatment is given to
      // the main body text that came in. Best to put attachment1_text
      // etc. into a nsMsgSendPart, then reshuffle the parts. Sigh.)
      if (!PL_strcasecmp(m_plaintext->m_encoding, ENCODING_QUOTED_PRINTABLE))
      {
        MimeEncoderData *plaintext_enc = MIME_QPEncoderInit(mime_encoder_output_fn, this);
        if (!plaintext_enc)
        {
          status = NS_ERROR_OUT_OF_MEMORY;
          goto FAIL;
        }
        plainpart->SetEncoderData(plaintext_enc);
      }
    }
    else 
    {
      delete maincontainer; 
      if (maincontainerISrelatedpart)
        m_related_part = nsnull; // in that case, m_related_part == maincontainer which we have just deleted!
      maincontainer = plainpart;
      mainbody = maincontainer;
      PR_FREEIF(m_attachment1_type);
      m_attachment1_type = PL_strdup(TEXT_PLAIN);
      if (!m_attachment1_type)
        goto FAILMEM;

      /* Override attachment1_encoding here. */
      PR_FREEIF(m_attachment1_encoding);
      m_attachment1_encoding = PL_strdup(m_plaintext->m_encoding);

      plaintext_is_mainbody_p = PR_TRUE; // converted plaintext is mainbody
    }
  }

  // check if we need to encapsulate the message in a multipart/mixed or multipart/digest
  if (m_attachment_count > multipartRelatedCount) 
  {
    toppart = new nsMsgSendPart(this);
    if (!toppart)
      goto FAILMEM;

    status = toppart->SetType(m_digest_p ? MULTIPART_DIGEST : MULTIPART_MIXED);
    if (status < 0)
      goto FAIL;

    status = toppart->AddChild(maincontainer);
    if (status < 0)
      goto FAIL;
  }
  else
    toppart = maincontainer;

  // Is the top part a multipart container?
  // can't use m_attachment_count because it's not reliable for that
  // instead use type of main part. See bug #174396
  toppart_type = toppart->GetType(); // GetType return directly the member variable, don't free it!
  if (!m_crypto_closure && toppart_type && !PL_strncasecmp(toppart_type, "multipart/", 10))
  {
    status = toppart->SetBuffer(MIME_MULTIPART_BLURB);
    if (status < 0)
      goto FAIL;
  }

   /* Write out the message headers.
   */
  headers = mime_generate_headers (mCompFields, mCompFields->GetCharacterSet(),
                                   m_deliver_mode, promptObject, &status);
  if (status < 0) 
    goto FAIL;

  if (!headers)
    goto FAILMEM;

  // 
  // If we converted HTML into plaintext, the plaintext part (plainpart)
  // already has its content-type and content-transfer-encoding
  // ("other") headers set. 
  // 
  // In the specific case where such a plaintext part is the 
  // top level message part (iff an HTML message is being sent
  // as text only and no other attachments exist) we want to 
  // preserve the original plainpart headers, since they
  // contain accurate transfer encoding and Mac type/creator 
  // information.
  // 
  // So, in the above case we append the main message headers, 
  // otherwise we overwrite whatever headers may have existed.
  // 
  /* reordering of headers will happen in nsMsgSendPart::Write */
  if ((plainpart) && (plainpart == toppart))
    status = toppart->AppendOtherHeaders(headers);
  else
    status = toppart->SetOtherHeaders(headers);
  PR_Free(headers);
  headers = nsnull;
  if (status < 0)
    goto FAIL;

  // Set up the first part (user-typed.)  For now, do it even if the first
  // part is empty; we need to add things to skip it if this part is empty.

  // Set up encoder for the first part (message body.)
  //
  NS_ASSERTION(!m_attachment1_encoder_data, "not-null m_attachment1_encoder_data");
  if (!PL_strcasecmp(m_attachment1_encoding, ENCODING_BASE64))
  {
    m_attachment1_encoder_data = MIME_B64EncoderInit(mime_encoder_output_fn, this);
    if (!m_attachment1_encoder_data) goto FAILMEM;
  }
  else
    if (!PL_strcasecmp(m_attachment1_encoding, ENCODING_QUOTED_PRINTABLE)) {
      m_attachment1_encoder_data =
      MIME_QPEncoderInit(mime_encoder_output_fn, this);
    }

  // If we converted HTML into plaintext, the plaintext part
  // already has its type/encoding headers set. So, in the specific
  // case where such a plaintext part is the main message body
  // (iff an HTML message is being sent as text only)
  // we want to avoid generating type/encoding/digest headers;
  // in all other cases, generate such headers here.
  //
  // We really want to set up headers as a dictionary of some sort
  // so that we need not worry about duplicate header lines.
  //
  if ((!plainpart) || (plainpart != mainbody))
  {
    const char *charset = mCompFields->GetCharacterSet();
    hdrs = mime_generate_attachment_headers (m_attachment1_type,
                         nsnull,
                         m_attachment1_encoding,
                         0, 0, 0, 0, 0,
                         m_digest_p,
                         nsnull, /* no "ma"! */
                         charset,
                         charset,
                         mCompFields->GetBodyIsAsciiOnly(),
                         nsnull,
                         PR_TRUE);
    if (!hdrs)
      goto FAILMEM;
    status = mainbody->AppendOtherHeaders(hdrs);
    if (status < 0)
      goto FAIL;
  }

  PR_FREEIF(hdrs);

  status = mainbody->SetEncoderData(m_attachment1_encoder_data);
  m_attachment1_encoder_data = nsnull;
  if (status < 0)
    goto FAIL;

  //
  // Now we need to process attachments and slot them in the
  // correct heirarchy.
  //
  if (m_attachment_count > 0)
  {
    // Kludge to avoid having to allocate memory on the toy computers...
    if (! mime_mailto_stream_read_buffer)
      mime_mailto_stream_read_buffer = (char *) PR_Malloc (MIME_BUFFER_SIZE);
    buffer = mime_mailto_stream_read_buffer;
    if (! buffer)
      goto FAILMEM;
    buffer_tail = buffer;

    // Gather all of the attachments for this message that are NOT
    // part of an enclosed MHTML message!
    for (i = 0; i < m_attachment_count; i++)
    {
      nsMsgAttachmentHandler *ma = &m_attachments[i];
      if (!ma->mMHTMLPart)
        PreProcessPart(ma, toppart);
    }

    // 
    // If we have a m_related_part as a container for children, then we have to 
    // tack on these children for the part
    //
    if (m_related_part)
    {
      for (i = 0; i < m_attachment_count; i++) 
      {
        //
        // rhp: This is here because we could get here after saying OK
        // to a lot of prompts about not being able to fetch this part!
        //
        if (m_attachments[i].mPartUserOmissionOverride)
          continue;

        // Now, we need to add this part to the m_related_part member so the 
        // message will be generated correctly.
        if (m_attachments[i].mMHTMLPart)
          PreProcessPart(&(m_attachments[i]), m_related_part);
      }
    }

  }

  // Tell the user we are creating the message...
  mComposeBundle->GetStringByID(NS_MSG_CREATING_MESSAGE, getter_Copies(msg));
  SetStatusMessage( msg );

  // OK, now actually write the structure we've carefully built up.
  status = toppart->Write();
  if (status < 0)
    goto FAIL;

  /* Close down encryption stream */
  if (m_crypto_closure)
	{
	  status = m_crypto_closure->FinishCryptoEncapsulation(PR_FALSE, mSendReport);
	  m_crypto_closure = 0;
	  if (NS_FAILED(status)) goto FAIL;
	}
 
  if (mOutputFile) 
  {
    if (NS_FAILED(mOutputFile->flush()) || mOutputFile->failed()) 
    {
      status = NS_MSG_ERROR_WRITING_FILE;
      goto FAIL;
    }

    mOutputFile->close();
    delete mOutputFile;
    mOutputFile = nsnull;

    /* If we don't do this check...ZERO length files can be sent */
    if (mTempFileSpec->GetFileSize() == 0)
    {
     status = NS_MSG_ERROR_WRITING_FILE;
      goto FAIL;
    }
  }

  mComposeBundle->GetStringByID(NS_MSG_ASSEMB_DONE_MSG, getter_Copies(msg));
  SetStatusMessage( msg );

  if (m_dont_deliver_p && mListener)
  {
    //
    // Need to ditch the file spec here so that we don't delete the
    // file, since in this case, the caller wants the file
    //
    NS_NewFileSpecWithSpec(*mTempFileSpec, &mReturnFileSpec);
    delete mTempFileSpec;
    mTempFileSpec = nsnull;
    if (!mReturnFileSpec)
      NotifyListenerOnStopSending(nsnull, NS_ERROR_OUT_OF_MEMORY, nsnull, nsnull);
    else
      NotifyListenerOnStopSending(nsnull, NS_OK, nsnull, mReturnFileSpec);
  }
  else 
  {
    status = DeliverMessage();
    if (NS_SUCCEEDED(status))
      shouldDeleteDeliveryState = PR_FALSE;
  }
  goto FAIL;

FAILMEM:
  status = NS_ERROR_OUT_OF_MEMORY;

FAIL:
  if (toppart)
    delete toppart;
  toppart = nsnull;
  mainbody = nsnull;
  maincontainer = nsnull;

  PR_FREEIF(headers);
  if (in_file) 
  {
    PR_Close (in_file);
    in_file = nsnull;
  }

  if (shouldDeleteDeliveryState)
  {
    if (status < 0) 
    {
      m_status = status;
      nsresult ignoreMe;
      Fail (status, nsnull, &ignoreMe);
    }
  }

  return status;
}

PRInt32
nsMsgComposeAndSend::PreProcessPart(nsMsgAttachmentHandler  *ma,
                                    nsMsgSendPart           *toppart) // The very top most container of the message
{
  nsresult        status;
  char            *hdrs = 0;
  nsMsgSendPart   *part = nsnull;

  // If this was one of those dead parts from a quoted web page, 
  // then just return safely.
  //
  if (ma->m_bogus_attachment)
    return 0;

  // If at this point we *still* don't have a content-type, then
  // we're never going to get one.
  if (ma->m_type == nsnull) 
  {
    ma->m_type = PL_strdup(UNKNOWN_CONTENT_TYPE);
    if (ma->m_type == nsnull)
      return 0;
  }

  ma->PickEncoding (mCompFields->GetCharacterSet(), this);

  part = new nsMsgSendPart(this);
  if (!part)
    return 0;
  status = toppart->AddChild(part);
  if (NS_FAILED(status))
    return 0;
  status = part->SetType(ma->m_type);
  if (NS_FAILED(status))
    return 0;

  nsXPIDLCString turl;
  if (!ma->mURL)
    {
      if (ma->m_uri)
        turl.Adopt(nsCRT::strdup(ma->m_uri));
    }
  else
    ma->mURL->GetSpec(turl);
  hdrs = mime_generate_attachment_headers (ma->m_type,
                                           ma->m_type_param,
                                           ma->m_encoding,
                                           ma->m_description,
                                           ma->m_x_mac_type,
                                           ma->m_x_mac_creator,
                                           ma->m_real_name,
                                           turl,
                                           m_digest_p,
                                           ma,
                                           ma->m_charset, // rhp - this needs
                                                          // to be the charset
                                                          // we determine from
                                                          // the file or none
                                                          // at all! 
                                           mCompFields->GetCharacterSet(),
                                           PR_FALSE,      // bodyIsAsciiOnly to false
                                                          // for attachments
                                           ma->m_content_id,
                                           PR_FALSE);
  if (!hdrs)
    return 0;

  status = part->SetOtherHeaders(hdrs);
  PR_FREEIF(hdrs);
  if (NS_FAILED(status))
    return 0;
  status = part->SetFile(ma->mFileSpec);
  if (NS_FAILED(status))
    return 0;
  if (ma->m_encoder_data) 
  {
    status = part->SetEncoderData(ma->m_encoder_data);
    if (NS_FAILED(status))
      return 0;
    ma->m_encoder_data = nsnull;
  }

  ma->m_current_column = 0;

  if (ma->m_type &&
      (!PL_strcasecmp (ma->m_type, MESSAGE_RFC822) ||
      !PL_strcasecmp (ma->m_type, MESSAGE_NEWS))) {
    status = part->SetStripSensitiveHeaders(PR_TRUE);
    if (NS_FAILED(status))
      return 0;
  }

  return 1;
}


#if defined(XP_MAC) && defined(DEBUG)
// Compiler runs out of registers for the debug build.
#pragma global_optimizer on
#pragma optimization_level 4
#endif // XP_MAC && DEBUG

# define FROB(X) \
    if (X && *X) \
    { \
      if (*recipients) PL_strcat(recipients, ","); \
      PL_strcat(recipients, X); \
    }

nsresult nsMsgComposeAndSend::BeginCryptoEncapsulation ()
{
  // Try to create a secure compose object. If we can create it, then query to see
  // if we need to use it for this send transaction. 

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgComposeSecure> secureCompose;
  secureCompose = do_CreateInstance(NS_MSGCOMPOSESECURE_CONTRACTID, &rv);
  // it's not an error scenario of there is secure compose
  if (NS_FAILED(rv))
    return NS_OK;

  if (secureCompose)
  {
    PRBool requiresEncryptionWork = PR_FALSE;
    secureCompose->RequiresCryptoEncapsulation(mUserIdentity, mCompFields, &requiresEncryptionWork);
    if (requiresEncryptionWork)
    {
      m_crypto_closure = secureCompose;
      // bah i'd like to move the following blurb into the implementation of BeginCryptoEncapsulation; however
      // the apis for nsIMsgComposeField just aren't rich enough. It requires the implementor to jump through way
      // too many string conversions....
	    char * recipients = (char *)
      PR_MALLOC((mCompFields->GetTo()  ? strlen(mCompFields->GetTo())  : 0) +
				 (mCompFields->GetCc()  ? strlen(mCompFields->GetCc())  : 0) +
				 (mCompFields->GetBcc() ? strlen(mCompFields->GetBcc()) : 0) +
				 (mCompFields->GetNewsgroups() ? strlen(mCompFields->GetNewsgroups()) : 0) + 20);
	    if (!recipients) return NS_ERROR_OUT_OF_MEMORY;

      *recipients = 0;

	    FROB(mCompFields->GetTo())
	    FROB(mCompFields->GetCc())
	    FROB(mCompFields->GetBcc())
	    FROB(mCompFields->GetNewsgroups())

      // end section of code I'd like to move to the implementor.....
      rv = m_crypto_closure->BeginCryptoEncapsulation(mOutputFile,
                                                      recipients,
                                                      mCompFields,
                                                      mUserIdentity,
                                                      mSendReport,
                                                      (m_deliver_mode == nsMsgSaveAsDraft));

      PR_FREEIF(recipients);
    }

  }

  return rv;
}

#if defined(XP_MAC) && defined(DEBUG)
#pragma global_optimizer reset
#endif // XP_MAC && DEBUG

nsresult
mime_write_message_body(nsIMsgSend *state, char *buf, PRInt32 size)
{
  NS_ENSURE_ARG_POINTER(state);

  nsOutputFileStream * output;
  nsCOMPtr<nsIMsgComposeSecure> crypto_closure;

  state->GetOutputStream(&output);
  if (!output || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_9))
    return NS_MSG_ERROR_WRITING_FILE;

  state->GetCryptoclosure(getter_AddRefs(crypto_closure));
  if (crypto_closure)
  {
	  return crypto_closure->MimeCryptoWriteBlock (buf, size);
	}

  if (PRInt32(output->write(buf, size)) < size) 
  {
    return NS_MSG_ERROR_WRITING_FILE;
  } 
  else 
  {
    return NS_OK;
  }
}

nsresult
mime_encoder_output_fn(const char *buf, PRInt32 size, void *closure)
{
  nsMsgComposeAndSend *state = (nsMsgComposeAndSend *) closure;
  return mime_write_message_body (state, (char *) buf, size);
}

nsresult
nsMsgComposeAndSend::GetEmbeddedObjectInfo(nsIDOMNode *node, nsMsgAttachmentData *attachment, PRBool *acceptObject)
{
  NS_ENSURE_ARG_POINTER(node);
  NS_ENSURE_ARG_POINTER(attachment);
  NS_ENSURE_ARG_POINTER(acceptObject);

  nsresult rv;

  // Reset this structure to null!
  memset(attachment, 0, sizeof(nsMsgAttachmentData));
  *acceptObject = PR_FALSE;

  // Check if the object has an moz-do-not-send attribute set. If it's the case,
  // we must ignore it

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(node);
  if (domElement)
  {
    nsAutoString attributeValue;
    if (NS_SUCCEEDED(domElement->GetAttribute(NS_LITERAL_STRING("moz-do-not-send"), attributeValue)))
      if (attributeValue.Equals(NS_LITERAL_STRING("true"), nsCaseInsensitiveStringComparator()))
        return NS_OK;
  }
    
  // Now, we know the types of objects this node can be, so we will do
  // our query interface here and see what we come up with 
  nsCOMPtr<nsIDOMHTMLBodyElement>     body = (do_QueryInterface(node));
  nsCOMPtr<nsIDOMHTMLImageElement>    image = (do_QueryInterface(node));
  nsCOMPtr<nsIDOMHTMLLinkElement>     link = (do_QueryInterface(node));
  nsCOMPtr<nsIDOMHTMLAnchorElement>   anchor = (do_QueryInterface(node));
    
  // First, try to see if the body as a background image
  if (body)
  {
    nsAutoString    tUrl;
    if (NS_SUCCEEDED(body->GetBackground(tUrl)))
    {
      nsCAutoString turlC;
      turlC.AssignWithConversion(tUrl);
      if (NS_SUCCEEDED(nsMsgNewURL(&attachment->url, turlC.get())))      
        NS_IF_ADDREF(attachment->url);
      else
        return NS_OK;
     }
  }
  else if (image)        // Is this an image?
  {
    nsString    tUrl;
    nsString    tName;
    nsString    tDesc;
    
    // Create the URI
    if (NS_FAILED(image->GetSrc(tUrl)))
      return NS_ERROR_FAILURE;
    nsCAutoString turlC;
    turlC.AssignWithConversion(tUrl);
    if (NS_FAILED(nsMsgNewURL(&attachment->url, turlC.get())))
    {
      // Well, the first time failed...which means we probably didn't get
      // the full path name...
      //
      nsIDOMDocument    *ownerDocument = nsnull;
      node->GetOwnerDocument(&ownerDocument);
      if (ownerDocument)
      {
        nsIDocument     *doc = nsnull;
        if (NS_FAILED(ownerDocument->QueryInterface(NS_GET_IID(nsIDocument),(void**)&doc)) || !doc)
          return NS_ERROR_OUT_OF_MEMORY;
        
        nsCAutoString spec;
        nsIURI *uri = doc->GetDocumentURI();
        
        if (!uri)
          return NS_ERROR_OUT_OF_MEMORY;
        
        uri->GetSpec(spec);
        
        // Ok, now get the path to the root doc and tack on the name we
        // got from the GetSrc() call....
        NS_ConvertUTF8toUCS2 workURL(spec);
        
        PRInt32 loc = workURL.RFind("/");
        if (loc >= 0)
          workURL.SetLength(loc+1);
        workURL.Append(tUrl);
        NS_ConvertUCS2toUTF8 workurlC(workURL);
        if (NS_FAILED(nsMsgNewURL(&attachment->url, workurlC.get())))
        {
          // rhp - just try to continue and send it without this image.
          return NS_OK;
        }
      }
    }
    
    NS_IF_ADDREF(attachment->url);
    
    rv = image->GetName(tName);
    NS_ENSURE_SUCCESS(rv, rv);
    attachment->real_name = ToNewCString(tName);
    
    image->GetLongDesc(tDesc);
    NS_ENSURE_SUCCESS(rv, rv);
    attachment->description = ToNewCString(tDesc);
    
  }
  else if (link)        // Is this a link?
  {
    nsString    tUrl;
    
    // Create the URI
    rv = link->GetHref(tUrl);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString turlC;
    turlC.AssignWithConversion(tUrl);
    rv = nsMsgNewURL(&attachment->url, turlC.get());
    NS_ENSURE_SUCCESS(rv, rv);
    
    NS_IF_ADDREF(attachment->url);
  }
  else if (anchor)
  {
    nsString    tUrl;
    nsString    tName;
    
    // Create the URI
    rv = anchor->GetHref(tUrl);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString turlC;
    turlC.AssignWithConversion(tUrl);
    rv = nsMsgNewURL(&attachment->url, turlC.get());
    NS_ENSURE_SUCCESS(rv, rv);
    
    NS_IF_ADDREF(attachment->url);
    
    rv = anchor->GetName(tName);
    NS_ENSURE_SUCCESS(rv, rv);
    attachment->real_name = ToNewCString(tName);
  }
  else
  {
    // If we get here, we got something we didn't expect!
    // Just try to continue and send it without this thing.
    return NS_OK;
  }
    
  //
  // Before going further, check if we are dealing with a local file and
  // if it's the case be sure the file exist!
  PRBool schemeIsFile = PR_FALSE;
  rv = attachment->url->SchemeIs("file", &schemeIsFile);
  if (NS_SUCCEEDED(rv) && schemeIsFile)
  {
    nsCOMPtr<nsIFileURL> fileUrl (do_QueryInterface(attachment->url));
    if (fileUrl)
    {
      PRBool isAValidFile = PR_FALSE;

      nsCOMPtr<nsIFile> aFile;
      rv = fileUrl->GetFile(getter_AddRefs(aFile));
      if (NS_SUCCEEDED(rv) && aFile)
      {
        nsCOMPtr<nsILocalFile> aLocalFile (do_QueryInterface(aFile));
        if (aLocalFile)
        {
          rv = aLocalFile->IsFile(&isAValidFile);
          if (NS_FAILED(rv))
            isAValidFile = PR_FALSE;
        }
      }
      
      if (! isAValidFile)
        return NS_OK;
    }  
  }
  
  *acceptObject = PR_TRUE;
  return NS_OK;
}


PRUint32
nsMsgComposeAndSend::GetMultipartRelatedCount(PRBool forceToBeCalculated /*=PR_FALSE*/)
{
  nsresult                  rv = NS_OK;
  PRUint32                  count;

  if (mMultipartRelatedAttachmentCount != -1 && !forceToBeCalculated)
    return (PRUint32)mMultipartRelatedAttachmentCount;

  //First time here, let's calculate the correct number of related part we need to generate
  mMultipartRelatedAttachmentCount = 0;
  nsCOMPtr<nsIEditorMailSupport> mailEditor (do_QueryInterface(mEditor));
  if (!mailEditor)
    return 0;

  rv = mailEditor->GetEmbeddedObjects(getter_AddRefs(mEmbeddedObjectList));
  if ((NS_FAILED(rv) || (!mEmbeddedObjectList)))
    return 0;

  if (NS_SUCCEEDED(mEmbeddedObjectList->Count(&count)))
  {
    if (count > 0)
    {
      // Let parse the list to count the number of valid objects. BTW, we can remove the others from the list
      nsMsgAttachmentData attachment;

      PRInt32 i;
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr <nsISupports> isupp;

      for (i = count - 1, count = 0; i >= 0; i --)
      {
        // Reset this structure to null!
        memset(&attachment, 0, sizeof(nsMsgAttachmentData));
        
        // now we need to get the element in the array and do the magic
        // to process this element.
        //
        mEmbeddedObjectList->QueryElementAt(i, NS_GET_IID(nsIDOMNode), getter_AddRefs(node));
        if (!node)
          continue;
    
        PRBool acceptObject = PR_FALSE;
        rv = GetEmbeddedObjectInfo(node, &attachment, &acceptObject);
        if (NS_SUCCEEDED(rv) && acceptObject)
          count ++;
        else
          mEmbeddedObjectList->DeleteElementAt(i);
      }
    }
    mMultipartRelatedAttachmentCount = (PRInt32)count;
    return count;
  }
  else
    return 0;
}

nsresult
nsMsgComposeAndSend::GetBodyFromEditor()
{
  //
  // Now we have to fix up and get the HTML from the editor. After we
  // get the HTML data, we need to store it in the m_attachment_1_body
  // member variable after doing the necessary charset conversion.
  //

  // 
  // Query the editor, get the body of HTML!
  //
  nsString  format; format.AssignWithConversion(TEXT_HTML);
  PRUint32  flags = nsIDocumentEncoder::OutputFormatted  | nsIDocumentEncoder::OutputNoFormattingInPre;
  nsAutoString bodyStr;
  PRUnichar* bodyText = nsnull;
  nsresult rv;
  PRUnichar *origHTMLBody = nsnull;

  // Ok, get the body...the DOM should have been whacked with 
  // Content ID's already
  mEditor->OutputToString(format, flags, bodyStr);

 //
  // If we really didn't get a body, just return NS_OK
  //
  if (bodyStr.IsEmpty())
    return NS_OK;
  bodyText = ToNewUnicode(bodyStr);
  if (!bodyText)
    return NS_ERROR_OUT_OF_MEMORY;

  // If we are forcing this to be plain text, we should not be
  // doing this conversion.
  PRBool doConversion = PR_TRUE;

  if ( (mCompFields) && mCompFields->GetForcePlainText() )
    doConversion = PR_FALSE;

  if (doConversion)
  {
    nsCOMPtr<mozITXTToHTMLConv> conv = do_CreateInstance(MOZ_TXTTOHTMLCONV_CONTRACTID, &rv);

    if (NS_SUCCEEDED(rv)) 
    {
      PRUint32 whattodo = mozITXTToHTMLConv::kURLs;
      PRBool enable_structs = PR_FALSE;
      nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
      if (pPrefBranch)
      {
        rv = pPrefBranch->GetBoolPref(PREF_MAIL_SEND_STRUCT, &enable_structs);
        if (enable_structs)
          whattodo = whattodo | mozITXTToHTMLConv::kStructPhrase;
      }
      
      PRUnichar* wresult;
      rv = conv->ScanHTML(bodyText, whattodo, &wresult);
      if (NS_SUCCEEDED(rv))
      {
        // Save the original body for possible attachment as plain text
        // We should have what the user typed in stored in mOriginalHTMLBody
        origHTMLBody = bodyText;
        bodyText = wresult;
      }
    }
  }
  
  nsCString attachment1_body;
  // we'd better be "text/html" at this point
  char          *attachment1_type = TEXT_HTML;  

  // Convert body to mail charset
  nsXPIDLCString    outCString;
  const char  *aCharset = mCompFields->GetCharacterSet();

  if (aCharset && *aCharset)
  {
    // Convert to entities.
    // If later Editor generates entities then we can remove this.
    PRBool isAsciiOnly;
    rv = nsMsgI18NSaveAsCharset(mCompFields->GetForcePlainText() ? TEXT_PLAIN : attachment1_type, 
                                aCharset, bodyText, getter_Copies(outCString), nsnull, &isAsciiOnly);

    mCompFields->SetBodyIsAsciiOnly(isAsciiOnly);
    // body contains characters outside the current mail charset,
    // ask whether to convert to UTF-8 (bug 233361). do this only for text/plain
    if ((NS_ERROR_UENC_NOMAPPING == rv) && mCompFields->GetForcePlainText()) {
      // if nbsp then replace it by sp and try again
      PRUnichar *bodyTextPtr = bodyText;
      while (*bodyTextPtr) {
        if (0x00A0 == *bodyTextPtr)
          *bodyTextPtr = 0x0020;
        bodyTextPtr++;
      }
      
      nsXPIDLCString fallbackCharset;
      rv = nsMsgI18NSaveAsCharset(TEXT_PLAIN, aCharset, bodyText,
           getter_Copies(outCString), getter_Copies(fallbackCharset));

      if (NS_ERROR_UENC_NOMAPPING == rv) {
        PRBool sendInUTF8;
        nsCOMPtr<nsIPrompt> prompt;
        GetDefaultPrompt(getter_AddRefs(prompt));
        rv = nsMsgAskBooleanQuestionByID(prompt, NS_ERROR_MSG_MULTILINGUAL_SEND,
                                         &sendInUTF8);
        if (!sendInUTF8) {
          Recycle(bodyText);
          return NS_ERROR_MSG_MULTILINGUAL_SEND;
        }
        CopyUTF16toUTF8(bodyText, outCString);
        mCompFields->SetCharacterSet("UTF-8"); // tag as UTF-8
      }
      // re-label to the fallback charset
      else if (fallbackCharset)
        mCompFields->SetCharacterSet(fallbackCharset.get());
    }

    if (NS_SUCCEEDED(rv)) 
      attachment1_body = outCString;

    // If we have an origHTMLBody that is not null, this means that it is
    // different than the bodyText because of formatting conversions. Because of
    // this we need to do the charset conversion on this part separately
    if (origHTMLBody)
    {
      char      *newBody = nsnull;
      rv = nsMsgI18NSaveAsCharset(mCompFields->GetUseMultipartAlternative() ? TEXT_PLAIN : attachment1_type, 
                                  aCharset, origHTMLBody, &newBody);
      if (NS_SUCCEEDED(rv)) 
      {
        PR_FREEIF(origHTMLBody);
        origHTMLBody = (PRUnichar *)newBody;
      }
    }

    Recycle(bodyText);    //Don't need it anymore
  }
  else
    return NS_ERROR_FAILURE;

  // If our holder for the orignal body text is STILL null, then just 
  // just copy what we have as the original body text.
  //
  if (!origHTMLBody)
    mOriginalHTMLBody = nsCRT::strdup(attachment1_body.get());
  else
    mOriginalHTMLBody = (char *)origHTMLBody;

  rv = SnarfAndCopyBody(attachment1_body.get(), attachment1_body.Length(),
                        attachment1_type);

  return rv;
}

// for SMTP, 16k
// for our internal protocol buffers, 4k
// for news < 1000
// so we choose the minimum, because we could be sending and posting this message.
#define LINE_BREAK_MAX 990

// EnsureLineBreaks() will set m_attachment1_body and m_attachment1_body_length 
nsresult
nsMsgComposeAndSend::EnsureLineBreaks(const char *body, PRUint32 bodyLen)
{
  NS_ENSURE_ARG_POINTER(body);

  PRUint32 i;
  PRUint32 charsSinceLineBreak = 0;
  PRUint32 lastPos = 0;

  char *newBody = nsnull;
  char *newBodyPos = nsnull;
  
  // the most common way to get into the state where we have to insert
  // linebreaks is when we do HTML reply and we quote large <pre> blocks.
  // see #83381 and #84261
  // 
  // until #67334 is fixed, we'll be replacing newlines with <br>, which can lead
  // to large quoted <pre> blocks without linebreaks.
  // this hack makes it so we can at least save (as draft or template) and send or post
  // the message.
  // 
  // XXX TODO 
  // march backwards and determine the "best" place for the linebreak
  // for example, we don't want <a hrLINEBREAKref=""> or <bLINEBREAKr>
  // or "MississLINEBREAKippi" 
  for (i = 0; i < bodyLen-1; i++) {
    if (nsCRT::strncmp(body+i, NS_LINEBREAK, NS_LINEBREAK_LEN)) {
      charsSinceLineBreak++;
      if (charsSinceLineBreak == LINE_BREAK_MAX) {
        if (!newBody) {
          // in the worse case, the body will be solid, no linebreaks.
          // that will require us to insert a line break every LINE_BREAK_MAX bytes
          PRUint32 worstCaseLen = bodyLen+((bodyLen/LINE_BREAK_MAX)*NS_LINEBREAK_LEN)+1;
          newBody = (char *) PR_Malloc(worstCaseLen);
          if (!newBody) return NS_ERROR_OUT_OF_MEMORY;
          newBodyPos = newBody;
        }

        PL_strncpy(newBodyPos, body+lastPos, i - lastPos + 1);
        newBodyPos += i - lastPos + 1;
        PL_strncpy(newBodyPos, NS_LINEBREAK, NS_LINEBREAK_LEN);
        newBodyPos += NS_LINEBREAK_LEN;

        lastPos = i+1;
        charsSinceLineBreak = 0;
      }
    }
    else {
      // found a linebreak
      charsSinceLineBreak = 0;
    }
  }

  // if newBody is non-null is non-zero, we inserted a linebreak
  if (newBody) {
     // don't forget about part after the last linebreak we inserted
     PL_strcpy(newBodyPos, body+lastPos);

     m_attachment1_body = newBody;
     m_attachment1_body_length = PL_strlen(newBody);  // not worstCaseLen
  }
  else {
     // body did not require any additional linebreaks, so just use it
     // body will not have any null bytes, so we can use PL_strdup
     m_attachment1_body = PL_strdup(body);
     if (!m_attachment1_body) {
      return NS_ERROR_OUT_OF_MEMORY;
     }
     m_attachment1_body_length = bodyLen;
  }
  return NS_OK;
}

//
// This is the routine that does the magic of generating the body and the
// attachments for the multipart/related email message.
//
typedef struct
{
  nsIDOMNode    *node;
  char          *url;
} domSaveStruct;

nsresult
nsMsgComposeAndSend::ProcessMultipartRelated(PRInt32 *aMailboxCount, PRInt32 *aNewsCount)
{
  PRUint32                  multipartCount = GetMultipartRelatedCount();
  nsresult                  rv = NS_OK;
  PRUint32                  i; 
  PRInt32                   j = -1;
  PRUint32                  k;
  PRInt32                   duplicateOf;
  domSaveStruct             *domSaveArray = nsnull;
  
  // Sanity check to see if we should be here or not...if not, just return
  if (!mEditor)
    return NS_OK;
  
   if (!mEmbeddedObjectList) 
    return NS_ERROR_MIME_MPART_ATTACHMENT_ERROR;
  
  nsMsgAttachmentData   attachment;
  PRInt32               locCount = -1;

  if (multipartCount > 0)
  {
    domSaveArray = (domSaveStruct *)PR_MALLOC(sizeof(domSaveStruct) * multipartCount);
    if (!domSaveArray)
      return NS_ERROR_MIME_MPART_ATTACHMENT_ERROR;
    memset(domSaveArray, 0, sizeof(domSaveStruct) * multipartCount);
  }

  nsCOMPtr<nsIDOMNode> node;
  for (i = mPreloadedAttachmentCount; i < (mPreloadedAttachmentCount + multipartCount); i++)
  {
    // Reset this structure to null!
    memset(&attachment, 0, sizeof(nsMsgAttachmentData));
    
    // MUST set this to get placed in the correct part of the message
    m_attachments[i].mMHTMLPart = PR_TRUE;
    
    locCount++;
    m_attachments[i].mDeleteFile = PR_TRUE;
    m_attachments[i].m_done = PR_FALSE;
    m_attachments[i].SetMimeDeliveryState(this);
    
    // Ok, now we need to get the element in the array and do the magic
    // to process this element.
    //
    
    mEmbeddedObjectList->QueryElementAt(locCount, NS_GET_IID(nsIDOMNode), getter_AddRefs(node));
    if (!node)
      return NS_ERROR_MIME_MPART_ATTACHMENT_ERROR;
    
    PRBool acceptObject = PR_FALSE;
    rv = GetEmbeddedObjectInfo(node, &attachment, &acceptObject);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_MIME_MPART_ATTACHMENT_ERROR);

    if (!acceptObject)
        continue;
   
    j++;
    domSaveArray[j].node = node;
    
    // check if we have alreay attached this object, don't need to attach it twice
    duplicateOf = -1;
    for (k = mPreloadedAttachmentCount; k < i; k++)
    {
      PRBool isEqual = PR_FALSE;
      (void)attachment.url->Equals(m_attachments[k].mURL, &isEqual);
      if (isEqual)
      {
        duplicateOf = k;
        break;
      }
    }

    if (duplicateOf == -1)
    {
      //
      // Now we have to get all of the interesting information from
      // the nsIDOMNode we have in hand...
      m_attachments[i].mURL = attachment.url;
  
      PR_FREEIF(m_attachments[i].m_override_type);
      m_attachments[i].m_override_type = PL_strdup (attachment.real_type);
      PR_FREEIF(m_attachments[i].m_override_encoding);
      m_attachments[i].m_override_encoding = PL_strdup (attachment.real_encoding);
      PR_FREEIF(m_attachments[i].m_desired_type);
      m_attachments[i].m_desired_type = PL_strdup (attachment.desired_type);
      PR_FREEIF(m_attachments[i].m_description);
      m_attachments[i].m_description = PL_strdup (attachment.description);
      PR_FREEIF(m_attachments[i].m_real_name);
      m_attachments[i].m_real_name = PL_strdup (attachment.real_name);
      PR_FREEIF(m_attachments[i].m_x_mac_type);
      m_attachments[i].m_x_mac_type = PL_strdup (attachment.x_mac_type);
      PR_FREEIF(m_attachments[i].m_x_mac_creator);
      m_attachments[i].m_x_mac_creator = PL_strdup (attachment.x_mac_creator);
  
      PR_FREEIF(m_attachments[i].m_charset);
      m_attachments[i].m_charset = PL_strdup (mCompFields->GetCharacterSet());
      PR_FREEIF(m_attachments[i].m_encoding);
      m_attachments[i].m_encoding = PL_strdup ("7bit");
  
      if (m_attachments[i].mURL)
        msg_pick_real_name(&m_attachments[i], nsnull, mCompFields->GetCharacterSet());
  
      //
      // Next, generate a content id for use with this part
      //    
      nsXPIDLCString email;
      mUserIdentity->GetEmail(getter_Copies(email));
      m_attachments[i].m_content_id = mime_gen_content_id(locCount+1, email.get());  
  
      if (!m_attachments[i].m_content_id)
        return NS_ERROR_OUT_OF_MEMORY;
  
      //
      // Start counting the attachments which are going to come from mail folders
      // and from NNTP servers.
      //
      if (m_attachments[i].mURL)
      {
        nsIURI *uri = m_attachments[i].mURL;
        PRBool match = PR_FALSE;
        if ((NS_SUCCEEDED(uri->SchemeIs("mailbox", &match)) && match) ||
           (NS_SUCCEEDED(uri->SchemeIs("imap", &match)) && match))
          (*aMailboxCount)++;
        else if ((NS_SUCCEEDED(uri->SchemeIs("news", &match)) && match) ||
                (NS_SUCCEEDED(uri->SchemeIs("snews", &match)) && match))
          (*aNewsCount)++;
      }
    }
    
    // Ok, cleanup the temp structure...
    PR_Free(attachment.real_name);
    PR_Free(attachment.description);
    PR_Free(attachment.real_type);
    PR_Free(attachment.real_encoding);
    PR_Free(attachment.desired_type);
    PR_Free(attachment.x_mac_type);
    PR_Free(attachment.x_mac_creator); 
    
    //
    // Ok, while we are here, we should whack the DOM with the generated 
    // Content-ID for this object. This will be necessary for generating
    // the HTML we need.
    //
    nsString domURL;
    if (m_attachments[duplicateOf == -1 ? i : duplicateOf].m_content_id)  
    {
      nsString   newSpec(NS_LITERAL_STRING("cid:"));
      newSpec.AppendWithConversion(m_attachments[duplicateOf == -1 ? i : duplicateOf].m_content_id);

      // Now, we know the types of objects this node can be, so we will do
      // our query interface here and see what we come up with 
      nsCOMPtr<nsIDOMHTMLBodyElement>     body = (do_QueryInterface(domSaveArray[j].node));
      nsCOMPtr<nsIDOMHTMLImageElement>    image = (do_QueryInterface(domSaveArray[j].node));
      nsCOMPtr<nsIDOMHTMLLinkElement>     link = (do_QueryInterface(domSaveArray[j].node));
      nsCOMPtr<nsIDOMHTMLAnchorElement>   anchor = (do_QueryInterface(domSaveArray[j].node));

      if (anchor)
      {
        anchor->GetHref(domURL);
        anchor->SetHref(newSpec);
      }
      else if (link)
      {
        link->GetHref(domURL);
        link->SetHref(newSpec);
      }
      else if (image)
      {
        image->GetSrc(domURL);
        image->SetSrc(newSpec);
      }
      else if (body)
      {
        body->GetBackground(domURL);
        body->SetBackground(newSpec);
      }

      if (!domURL.IsEmpty())
        domSaveArray[j].url = ToNewCString(domURL);
    }
  }
  
  rv = GetBodyFromEditor();

  // 
  // Ok, now we need to un-whack the DOM or we have a screwed up document on 
  // Send failure.
  //
  for (i = 0; i < multipartCount; i++)
  {
    if ( (!domSaveArray[i].node) || (!domSaveArray[i].url) )
      continue;

    // Now, we know the types of objects this node can be, so we will do
    // our query interface here and see what we come up with 
    nsCOMPtr<nsIDOMHTMLBodyElement>     body = (do_QueryInterface(domSaveArray[i].node));
    nsCOMPtr<nsIDOMHTMLImageElement>    image = (do_QueryInterface(domSaveArray[i].node));
    nsCOMPtr<nsIDOMHTMLLinkElement>     link = (do_QueryInterface(domSaveArray[i].node));
    nsCOMPtr<nsIDOMHTMLAnchorElement>   anchor = (do_QueryInterface(domSaveArray[i].node));

      // STRING USE WARNING: hoisting the following conversion might save code-space, since it happens along every path

    if (anchor)
      anchor->SetHref(NS_ConvertASCIItoUCS2(domSaveArray[i].url));
    else if (link)
      link->SetHref(NS_ConvertASCIItoUCS2(domSaveArray[i].url));
    else if (image)
      image->SetSrc(NS_ConvertASCIItoUCS2(domSaveArray[i].url));
    else if (body)
      body->SetBackground(NS_ConvertASCIItoUCS2(domSaveArray[i].url));

    nsMemory::Free(domSaveArray[i].url);
  }

  PR_FREEIF(domSaveArray);

  //
  // Now, we have to create that first child node for the multipart
  // message that holds the body as well as the attachment handler
  // for this body part.
  //
  // If we ONLY have multipart objects, then we don't need the container
  // for the multipart section...
  //
  m_related_part = new nsMsgSendPart(this);
  if (!m_related_part)
    return NS_ERROR_OUT_OF_MEMORY;  

  m_related_part->SetMimeDeliveryState(this);
  m_related_part->SetType(MULTIPART_RELATED);
    
  // We are now going to use the m_related_part as a way to store the 
  // MHTML message for this email.
  //
  m_related_body_part = new nsMsgSendPart(this);
  if (!m_related_body_part)
    return NS_ERROR_OUT_OF_MEMORY;
  
  // Set the body contents...
  m_related_body_part->SetBuffer(m_attachment1_body);
  m_related_body_part->SetType(m_attachment1_type);
  
  m_related_part->AddChild(m_related_body_part);

  return rv;
}

nsresult
nsMsgComposeAndSend::CountCompFieldAttachments()
{
  //Reset the counters
  mCompFieldLocalAttachments = 0;
  mCompFieldRemoteAttachments = 0;

  //Get the attachments array
  nsCOMPtr<nsISupportsArray> attachmentsArray;
  mCompFields->GetAttachmentsArray(getter_AddRefs(attachmentsArray));
  if (!attachmentsArray)
    return NS_OK;

  PRUint32 i;
  PRUint32 attachmentCount = 0;
  attachmentsArray->Count(&attachmentCount);
  
  //Parse the attachments array
  nsCOMPtr<nsIMsgAttachment> element;
  nsXPIDLCString url;
  for (i = 0; i < attachmentCount; i ++)
  {
    attachmentsArray->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
    if (element)
    {
      element->GetUrl(getter_Copies(url));
      if (!url.IsEmpty())
    {
      // Check to see if this is a file URL, if so, don't retrieve
      // like a remote URL...
        if (nsMsgIsLocalFile(url.get()))
      {
        mCompFieldLocalAttachments++;
#if defined(DEBUG_ducarroz)
          printf("Counting LOCAL attachment %d: %s\n", mCompFieldLocalAttachments, url.get());
#endif
      }
      else    // This is a remote URL...
      {
        mCompFieldRemoteAttachments++;
#if defined(DEBUG_ducarroz)
          printf("Counting REMOTE attachment %d: %s\n", mCompFieldRemoteAttachments, url.get());
#endif
      }
    }
    }
  }

  return NS_OK;
}

// 
// Since we are at the head of the list, we start from ZERO.
//
nsresult
nsMsgComposeAndSend::AddCompFieldLocalAttachments()
{
  // If none, just return...
  if (mCompFieldLocalAttachments <= 0)
    return NS_OK;

  //Get the attachments array
  nsCOMPtr<nsISupportsArray> attachmentsArray;
  mCompFields->GetAttachmentsArray(getter_AddRefs(attachmentsArray));
  if (!attachmentsArray)
    return NS_OK;

  PRUint32 i;
  PRUint32  newLoc = 0;
  PRUint32 attachmentCount = 0;
  attachmentsArray->Count(&attachmentCount);
  
  //Parse the attachments array
  nsCOMPtr<nsIMsgAttachment> element;
  nsXPIDLCString url;
  for (i = 0; i < attachmentCount; i ++)
  {
    attachmentsArray->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
    if (element)
    {
      element->GetUrl(getter_Copies(url));
      if (!url.IsEmpty())
      {
      // Just look for local file:// attachments and do the right thing.
      if (nsMsgIsLocalFile(url.get()))
      {
#if defined(DEBUG_ducarroz)
        printf("Adding LOCAL attachment %d: %s\n", newLoc, url.get());
#endif
        //
        // Now we have to setup the m_attachments entry for the file://
        // URL that is passed in...
        //
        m_attachments[newLoc].mDeleteFile = PR_FALSE;

        nsMsgNewURL(getter_AddRefs(m_attachments[newLoc].mURL), url.get());

        if (m_attachments[newLoc].mFileSpec)
        {
          if (m_attachments[newLoc].mDeleteFile)
            m_attachments[newLoc].mFileSpec->Delete(PR_FALSE);
          delete (m_attachments[newLoc].mFileSpec);
          m_attachments[newLoc].mFileSpec =nsnull;
        }
        m_attachments[newLoc].mFileSpec = new nsFileSpec( nsFileURL(url.get()) );
        m_attachments[newLoc].mDeleteFile = PR_FALSE;
        if (m_attachments[newLoc].mURL)
        {
          nsAutoString proposedName;
          element->GetName(proposedName);
          msg_pick_real_name(&m_attachments[newLoc], proposedName.get(), mCompFields->GetCharacterSet());
        }

        // Now, most importantly, we need to figure out what the content type is for
        // this attachment...If we can't, then just make it application/octet-stream

        PR_FREEIF(m_attachments[newLoc].m_type);
        element->GetContentType(&m_attachments[newLoc].m_type);
        if (!m_attachments[newLoc].m_type || !(*m_attachments[newLoc].m_type))
        {
          nsresult  rv = NS_OK;
          nsCOMPtr<nsIMIMEService> mimeFinder (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
          if (NS_SUCCEEDED(rv) && mimeFinder) 
          {
            nsCOMPtr<nsIURL> fileUrl(do_CreateInstance(NS_STANDARDURL_CONTRACTID));
            if (fileUrl)
            {
              nsCAutoString fileExt;
              //First try using the real file name
              rv = fileUrl->SetFileName(nsDependentCString(m_attachments[newLoc].m_real_name));
              if (NS_SUCCEEDED(rv))
              {
                rv = fileUrl->GetFileExtension(fileExt);
                if (NS_SUCCEEDED(rv) && !fileExt.IsEmpty()) {
                  nsCAutoString type;
                  mimeFinder->GetTypeFromExtension(fileExt, type);
                  m_attachments[newLoc].m_type = ToNewCString(type);
                }
              }

              //Then try using the url if we still haven't figured out the content type
              if ((!m_attachments[newLoc].m_type) ||  (!*m_attachments[newLoc].m_type))
              {
                rv = fileUrl->SetSpec(url);
                if (NS_SUCCEEDED(rv))
                {
                  rv = fileUrl->GetFileExtension(fileExt);
                  if (NS_SUCCEEDED(rv) && !fileExt.IsEmpty()) {
                    nsCAutoString type;
                    mimeFinder->GetTypeFromExtension(fileExt, type);
                    m_attachments[newLoc].m_type = ToNewCString(type);
                  }
                }
              }
            }
          }
        }
        else
          element->GetContentTypeParam(&m_attachments[newLoc].m_type_param);

#if defined(XP_MAC) || defined(XP_MACOSX)
        //We always need to snarf the file to figure out how to send it, maybe we need to use apple double...
        m_attachments[newLoc].m_done = PR_FALSE;
        m_attachments[newLoc].SetMimeDeliveryState(this);
#else
        //We need to snarf the file to figure out how to send it only if we don't have a content type...
        if ((!m_attachments[newLoc].m_type) || (!*m_attachments[newLoc].m_type))
        {
          m_attachments[newLoc].m_done = PR_FALSE;
          m_attachments[newLoc].SetMimeDeliveryState(this);
        }
        else
        {
          m_attachments[newLoc].m_done = PR_TRUE;
          m_attachments[newLoc].SetMimeDeliveryState(nsnull);
        }
#endif
        // For local files, if they are HTML docs and we don't have a charset, we should
        // sniff the file and see if we can figure it out.
        if ( (m_attachments[newLoc].m_type) &&  (*m_attachments[newLoc].m_type) ) 
        {
          if (PL_strcasecmp(m_attachments[newLoc].m_type, TEXT_HTML) == 0)
          {
            char *tmpCharset = (char *)nsMsgI18NParseMetaCharset(m_attachments[newLoc].mFileSpec);
            if (tmpCharset[0] != '\0')
            {
              PR_FREEIF(m_attachments[newLoc].m_charset);
              m_attachments[newLoc].m_charset = PL_strdup(tmpCharset);
            }
          }
        }

        PR_FREEIF(m_attachments[newLoc].m_x_mac_type);
        element->GetMacType(&m_attachments[newLoc].m_x_mac_type);
        PR_FREEIF(m_attachments[newLoc].m_x_mac_creator);
        element->GetMacCreator(&m_attachments[newLoc].m_x_mac_creator);

        ++newLoc;
      }
    }
  }
  }
  return NS_OK;
}

nsresult
nsMsgComposeAndSend::AddCompFieldRemoteAttachments(PRUint32   aStartLocation,
                                                   PRInt32    *aMailboxCount, 
                                                   PRInt32    *aNewsCount)
{
  // If none, just return...
  if (mCompFieldRemoteAttachments <= 0)
    return NS_OK;

  //Get the attachments array
  nsCOMPtr<nsISupportsArray> attachmentsArray;
  mCompFields->GetAttachmentsArray(getter_AddRefs(attachmentsArray));
  if (!attachmentsArray)
    return NS_OK;

  PRUint32 i;
  PRUint32  newLoc = aStartLocation;
  PRUint32 attachmentCount = 0;
  attachmentsArray->Count(&attachmentCount);

  //Parse the attachments array
  nsCOMPtr<nsIMsgAttachment> element;
  nsXPIDLCString url;
  for (i = 0; i < attachmentCount; i ++)
  {
    attachmentsArray->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
    if (element)
    {
      element->GetUrl(getter_Copies(url));
      if (!url.IsEmpty())
      {
        // Just look for files that are NOT local file attachments and do 
        // the right thing.
        if (! nsMsgIsLocalFile(url.get()))
        {
#if defined(DEBUG_ducarroz)
          printf("Adding REMOTE attachment %d: %s\n", newLoc, url.get());
#endif
          PRBool isAMessageAttachment = !PL_strncasecmp(url.get(), "mailbox-message://", 18) ||
              !PL_strncasecmp(url.get(), "imap-message://", 15) ||
              !PL_strncasecmp(url.get(), "news-message://", 15);

          m_attachments[newLoc].mDeleteFile = PR_TRUE;
          m_attachments[newLoc].m_done = PR_FALSE;
          m_attachments[newLoc].SetMimeDeliveryState(this);

          if (!isAMessageAttachment)
            nsMsgNewURL(getter_AddRefs(m_attachments[newLoc].mURL), url.get());

          PR_FREEIF(m_attachments[newLoc].m_encoding);
          m_attachments[newLoc].m_encoding = PL_strdup ("7bit");

          PR_FREEIF(m_attachments[newLoc].m_x_mac_type);
          element->GetMacType(&m_attachments[newLoc].m_x_mac_type);
          PR_FREEIF(m_attachments[newLoc].m_x_mac_creator);
          element->GetMacCreator(&m_attachments[newLoc].m_x_mac_creator);

          /* Count up attachments which are going to come from mail folders
             and from NNTP servers. */
          PRBool do_add_attachment = PR_FALSE;
          if (isAMessageAttachment)
          {
            do_add_attachment = PR_TRUE;
            if (!PL_strncasecmp(url.get(), "news-message://", 15))
              (*aNewsCount)++;
            else
              (*aMailboxCount)++;              

            m_attachments[newLoc].m_uri = PL_strdup(url.get());
            m_attachments[newLoc].mURL = nsnull;
          }
          else
            do_add_attachment = (nsnull != m_attachments[newLoc].mURL);

          if (do_add_attachment)
          {
            nsAutoString proposedName;
            element->GetName(proposedName);
            msg_pick_real_name(&m_attachments[newLoc], proposedName.get(), mCompFields->GetCharacterSet());
            ++newLoc;
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult 
nsMsgComposeAndSend::HackAttachments(const nsMsgAttachmentData *attachments,
                                     const nsMsgAttachedFile *preloaded_attachments)
{ 
  //
  // First, count the total number of attachments we are going to process
  // for this operation! This is a little more complicated than you might
  // think because we have a few ways to specify attachments. Via the nsMsgAttachmentData
  // as well as the composition fields.
  //
  CountCompFieldAttachments();

  // Count the preloaded attachments!
  mPreloadedAttachmentCount = 0;

  // For now, manually add the local attachments in the comp field!
  mPreloadedAttachmentCount += mCompFieldLocalAttachments;

  if (preloaded_attachments && preloaded_attachments[0].orig_url) 
  {
    while (preloaded_attachments[mPreloadedAttachmentCount].orig_url)
      mPreloadedAttachmentCount++;
  }

  // Count the attachments we have to go retrieve! Keep in mind, that these
  // will be APPENDED to the current list of URL's that we have gathered if
  // this is a multpart/related send operation
  mRemoteAttachmentCount = GetMultipartRelatedCount();

  // For now, manually add the remote attachments in the comp field!
  mRemoteAttachmentCount += mCompFieldRemoteAttachments;

  PRInt32     tCount = 0;
  if (attachments && attachments[0].url) 
  {
    while (attachments[tCount].url)
    {
      mRemoteAttachmentCount++;
      tCount++;
    }
  }

  m_attachment_count = mPreloadedAttachmentCount + mRemoteAttachmentCount;

  // Now create the array of attachment handlers...
  m_attachments = (nsMsgAttachmentHandler *) new nsMsgAttachmentHandler[m_attachment_count];
  if (! m_attachments)
    return NS_ERROR_OUT_OF_MEMORY;

  // clear this new memory...
  memset(m_attachments, 0, (sizeof(nsMsgAttachmentHandler) * m_attachment_count)); 
  PRUint32     i;    // counter for location in attachment array...

  //
  // First, we need to attach the files that are defined in the comp fields...
  if (NS_FAILED(AddCompFieldLocalAttachments()))
    return NS_ERROR_INVALID_ARG;

  // Now handle the preloaded attachments...
  if (preloaded_attachments && preloaded_attachments[0].orig_url) 
  {
    // These are attachments which have already been downloaded to tmp files.
    // We merely need to point the internal attachment data at those tmp
    // files.
    m_pre_snarfed_attachments_p = PR_TRUE;

    for (i = mCompFieldLocalAttachments; i < mPreloadedAttachmentCount; i++) 
    {
      /* These attachments are already "snarfed". */
      m_attachments[i].mDeleteFile = PR_FALSE;
      m_attachments[i].SetMimeDeliveryState(nsnull);
      m_attachments[i].m_done = PR_TRUE;
      NS_ASSERTION (preloaded_attachments[i].orig_url, "null url");

      m_attachments[i].mURL = preloaded_attachments[i].orig_url;

      PR_FREEIF(m_attachments[i].m_type);
      m_attachments[i].m_type = PL_strdup (preloaded_attachments[i].type);

      // Set it to the compose fields for a default...
      PR_FREEIF(m_attachments[i].m_charset);
      m_attachments[i].m_charset = PL_strdup (mCompFields->GetCharacterSet());

      // If we still don't have a content type, we should really try sniff one out!
      if ((!m_attachments[i].m_type) || (!*m_attachments[i].m_type))
      {
        m_attachments[i].PickEncoding(mCompFields->GetCharacterSet(), this);
      }

      // For local files, if they are HTML docs and we don't have a charset, we should
      // sniff the file and see if we can figure it out.
      if ( (m_attachments[i].m_type) &&  (*m_attachments[i].m_type) ) 
      {
        if ( (PL_strcasecmp(m_attachments[i].m_type, TEXT_HTML) == 0) && (preloaded_attachments[i].file_spec) )
        {
          char *tmpCharset = (char *)nsMsgI18NParseMetaCharset(preloaded_attachments[i].file_spec);
          if (tmpCharset[0] != '\0')
          {
            PR_FREEIF(m_attachments[i].m_charset);
            m_attachments[i].m_charset = PL_strdup(tmpCharset);
          }
        }
      }

      PR_FREEIF(m_attachments[i].m_description);
      m_attachments[i].m_description = PL_strdup (preloaded_attachments[i].description);
      PR_FREEIF(m_attachments[i].m_real_name);
      m_attachments[i].m_real_name = PL_strdup (preloaded_attachments[i].real_name);
      PR_FREEIF(m_attachments[i].m_x_mac_type);
      m_attachments[i].m_x_mac_type = PL_strdup (preloaded_attachments[i].x_mac_type);
      PR_FREEIF(m_attachments[i].m_x_mac_creator);
      m_attachments[i].m_x_mac_creator = PL_strdup (preloaded_attachments[i].x_mac_creator);
      PR_FREEIF(m_attachments[i].m_encoding);
      m_attachments[i].m_encoding = PL_strdup (preloaded_attachments[i].encoding);

      if (m_attachments[i].mFileSpec)
      {
        if (m_attachments[i].mDeleteFile)
          m_attachments[i].mFileSpec->Delete(PR_FALSE);
        delete (m_attachments[i].mFileSpec);
        m_attachments[i].mFileSpec=nsnull;
      }
      m_attachments[i].mFileSpec = new nsFileSpec(*(preloaded_attachments[i].file_spec));

      m_attachments[i].m_size = preloaded_attachments[i].size;
      m_attachments[i].m_unprintable_count = preloaded_attachments[i].unprintable_count;
      m_attachments[i].m_highbit_count = preloaded_attachments[i].highbit_count;
      m_attachments[i].m_ctl_count = preloaded_attachments[i].ctl_count;
      m_attachments[i].m_null_count = preloaded_attachments[i].null_count;
      m_attachments[i].m_max_column = preloaded_attachments[i].max_line_length;

      /* If the attachment has an encoding, and it's not one of
      the "null" encodings, then keep it. */
      if (m_attachments[i].m_encoding &&
          PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_7BIT) &&
          PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_8BIT) &&
          PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_BINARY))
        m_attachments[i].m_already_encoded_p = PR_TRUE;

            if (m_attachments[i].mURL)
        msg_pick_real_name(&m_attachments[i], nsnull, mCompFields->GetCharacterSet());
    }
  }

  // First, handle the multipart related attachments if any...
  //
  PRInt32 mailbox_count = 0, news_count = 0;
  PRInt32 multipartRelatedCount = GetMultipartRelatedCount();

  if (multipartRelatedCount > 0)
  {
    nsresult rv = ProcessMultipartRelated(&mailbox_count, &news_count);
    if (NS_FAILED(rv))
    {
      // The destructor will take care of the m_attachment array
      return rv;
    }
  }

  //
  // Now add the comp field remote attachments...
  //
  if (NS_FAILED( AddCompFieldRemoteAttachments( (mPreloadedAttachmentCount + multipartRelatedCount), 
                                                 &mailbox_count, &news_count) ))
    return NS_ERROR_INVALID_ARG;

  //
  // Now deal remote attachments and attach multipart/related attachments (url's and such..)
  // first!
  //
  if (attachments && attachments[0].url) 
  {
    PRInt32     locCount = -1;

    for (i = (mPreloadedAttachmentCount + GetMultipartRelatedCount() + mCompFieldRemoteAttachments); i < m_attachment_count; i++) 
    {
      locCount++;
      m_attachments[i].mDeleteFile = PR_TRUE;
      m_attachments[i].m_done = PR_FALSE;
      m_attachments[i].SetMimeDeliveryState(this);
      NS_ASSERTION (attachments[locCount].url, "null url");

      m_attachments[i].mURL = attachments[locCount].url;

      PR_FREEIF(m_attachments[i].m_override_type);
      m_attachments[i].m_override_type = PL_strdup (attachments[locCount].real_type);
      PR_FREEIF(m_attachments[i].m_charset);
      m_attachments[i].m_charset = PL_strdup (mCompFields->GetCharacterSet());
      PR_FREEIF(m_attachments[i].m_override_encoding);
      m_attachments[i].m_override_encoding = PL_strdup (attachments[locCount].real_encoding);
      PR_FREEIF(m_attachments[i].m_desired_type);
      m_attachments[i].m_desired_type = PL_strdup (attachments[locCount].desired_type);
      PR_FREEIF(m_attachments[i].m_description);
      m_attachments[i].m_description = PL_strdup (attachments[locCount].description);
      PR_FREEIF(m_attachments[i].m_real_name);
      m_attachments[i].m_real_name = PL_strdup (attachments[locCount].real_name);
      PR_FREEIF(m_attachments[i].m_x_mac_type);
      m_attachments[i].m_x_mac_type = PL_strdup (attachments[locCount].x_mac_type);
      PR_FREEIF(m_attachments[i].m_x_mac_creator);
      m_attachments[i].m_x_mac_creator = PL_strdup (attachments[locCount].x_mac_creator);
      PR_FREEIF(m_attachments[i].m_encoding);
      m_attachments[i].m_encoding = PL_strdup ("7bit");

      // real name is set in the case of vcard so don't change it.  XXX STILL NEEDED?
      // m_attachments[i].m_real_name = 0;

      /* Count up attachments which are going to come from mail folders
      and from NNTP servers. */
	  if (m_attachments[i].mURL)
	  {
		nsIURI *uri = m_attachments[i].mURL;
		PRBool match = PR_FALSE;
		if ((NS_SUCCEEDED(uri->SchemeIs("mailbox", &match)) && match) ||
			(NS_SUCCEEDED(uri->SchemeIs("imap", &match)) && match))
		  mailbox_count++;
		else if ((NS_SUCCEEDED(uri->SchemeIs("news", &match)) && match) ||
			     (NS_SUCCEEDED(uri->SchemeIs("snews", &match)) && match))
	      news_count++;

	    if (uri)
	      msg_pick_real_name(&m_attachments[i], nsnull, mCompFields->GetCharacterSet());
      }
    }
  }

  PRBool needToCallGatherMimeAttachments = PR_TRUE;

  if (m_attachment_count > 0)
  {
    // If there is more than one mailbox URL, or more than one NNTP url,
    // do the load in serial rather than parallel, for efficiency.
    if (mailbox_count > 1 || news_count > 1)
      m_be_synchronous_p = PR_TRUE;
  
    m_attachment_pending_count = m_attachment_count;
  
    // Start the URL attachments loading (eventually, an exit routine will
    // call the done_callback).
  
    for (i = 0; i < m_attachment_count; i++) 
    {
      if (m_attachments[i].m_done)
      {
        m_attachment_pending_count--;
        continue;
      }
    
      //
      //  IF we get here and the URL is NULL, just dec the pending count and move on!!!
      //
      if ( (!m_attachments[i].mURL) && (!m_attachments[i].m_uri) )
      {
        m_attachments[i].m_bogus_attachment = PR_TRUE;
        m_attachments[i].m_done = PR_TRUE;
        m_attachments[i].SetMimeDeliveryState(nsnull);
        m_attachment_pending_count--;
        continue;
      }

      //
      // This only returns a failure code if NET_GetURL was not called
      // (and thus no exit routine was or will be called.) 
      //

      // Display some feedback to user...
      PRUnichar     *printfString = nsnull;

      nsXPIDLString msg; 
      mComposeBundle->GetStringByID(NS_MSG_GATHERING_ATTACHMENT, getter_Copies(msg));

      if (m_attachments[i].m_real_name)
        printfString = nsTextFormatter::smprintf(msg, m_attachments[i].m_real_name);
      else
        printfString = nsTextFormatter::smprintf(msg, "");

      if (printfString)
      {
        SetStatusMessage(printfString); 
        PR_Free(printfString);  
      }
      
      /* As SnarfAttachment will call GatherMimeAttachments when it will be done (this is an async process),
         we need to avoid to call it ourself.
      */ 
      needToCallGatherMimeAttachments = PR_FALSE;

      nsresult status = m_attachments[i].SnarfAttachment(mCompFields);
      if (NS_FAILED(status))
      {
        nsXPIDLString errorMsg; 
        nsAutoString attachmentFileName;
        nsresult rv = ConvertToUnicode(nsMsgI18NFileSystemCharset(), m_attachments[i].m_real_name, attachmentFileName);
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr<nsIStringBundle> bundle;
          const PRUnichar *params[] = { attachmentFileName.get() };
          rv = mComposeBundle->GetBundle(getter_AddRefs(bundle));
          if (NS_SUCCEEDED(rv))
          {
            bundle->FormatStringFromID(NS_ERROR_GET_CODE(NS_MSG_ERROR_ATTACHING_FILE), params, 1, getter_Copies(errorMsg));
            mSendReport->SetMessage(nsIMsgSendReport::process_Current, errorMsg, PR_FALSE);
          }
          mSendReport->SetError(nsIMsgSendReport::process_Current, NS_MSG_ERROR_ATTACHING_FILE /* status */, PR_FALSE);
        }
        return NS_MSG_ERROR_ATTACHING_FILE;
      }
      if (m_be_synchronous_p)
        break;
    }
  }

  // If no attachments - finish now (this will call the done_callback).
  if (needToCallGatherMimeAttachments)
    return GatherMimeAttachments();

  return NS_OK;
}

int nsMsgComposeAndSend::SetMimeHeader(nsMsgCompFields::MsgHeaderID header, const char *value)
{
  char * dupHeader = nsnull;
  PRInt32 ret = NS_ERROR_OUT_OF_MEMORY;

  switch (header)
  {
    case nsMsgCompFields::MSG_FROM_HEADER_ID :
    case nsMsgCompFields::MSG_TO_HEADER_ID :
    case nsMsgCompFields::MSG_REPLY_TO_HEADER_ID :
    case nsMsgCompFields::MSG_CC_HEADER_ID :
    case nsMsgCompFields::MSG_BCC_HEADER_ID :
      dupHeader = mime_fix_addr_header(value);
      break;
    
    case nsMsgCompFields::MSG_NEWSGROUPS_HEADER_ID :
    case nsMsgCompFields::MSG_FOLLOWUP_TO_HEADER_ID :
      dupHeader = mime_fix_news_header(value);
      break;
    
    case nsMsgCompFields::MSG_FCC_HEADER_ID :
    case nsMsgCompFields::MSG_ORGANIZATION_HEADER_ID :
    case nsMsgCompFields::MSG_SUBJECT_HEADER_ID :
    case nsMsgCompFields::MSG_REFERENCES_HEADER_ID :
    case nsMsgCompFields::MSG_X_TEMPLATE_HEADER_ID :
    case nsMsgCompFields::MSG_ATTACHMENTS_HEADER_ID :
      dupHeader = mime_fix_header(value);
      break;
    
    default : NS_ASSERTION(PR_FALSE, "invalid header"); // unhandled header - bad boy.
  }

  if (dupHeader) 
  {
    ret = mCompFields->SetAsciiHeader(header, dupHeader);
    PR_Free(dupHeader);
  }
  return ret;
}

nsresult
nsMsgComposeAndSend::InitCompositionFields(nsMsgCompFields *fields)
{
  nsresult        rv = NS_OK;
  const char      *pStr = nsnull;
  nsMsgCompFields *tPtr = new nsMsgCompFields();

  if (!tPtr)
    return NS_ERROR_OUT_OF_MEMORY;

  mCompFields = do_QueryInterface( tPtr );
  if (!mCompFields)
    return NS_ERROR_OUT_OF_MEMORY;

  const char *cset = fields->GetCharacterSet();
  // Make sure charset is sane...
  if (!cset || !*cset)
  {
    mCompFields->SetCharacterSet("us-ascii");
  }
  else
  {
    mCompFields->SetCharacterSet(fields->GetCharacterSet());
  }

  pStr = fields->GetMessageId();
  if (pStr)
  {
    mCompFields->SetMessageId((char *) pStr);
    /* Don't bother checking for out of memory; if it fails, then we'll just
       let the server generate the message-id, and suffer with the
       possibility of duplicate messages.*/
  }

  pStr = fields->GetNewspostUrl();
  if (pStr && *pStr) 
  {
    mCompFields->SetNewspostUrl((char *)pStr);
  }

  // Now, we will look for a URI defined as the default FCC pref. If this is set,
  // then SetFcc will use this value. The FCC field is a URI for the server that 
  // will hold the "Sent" folder...the 
  //
  // First, look at what was passed in via the "fields" structure...if that was
  // set then use it, otherwise, fall back to what is set in the prefs...
  //
  // But even before that, pay attention to the new OVERRIDE pref that will cancel
  // any and all copy operations!
  //
  PRBool    doFcc = PR_TRUE;
  rv = mUserIdentity->GetDoFcc(&doFcc);
  if (!doFcc)
  {
    // If the identity pref "fcc" is set to false, then we will not do
    // any FCC operation!
    mCompFields->SetFcc("");
  }
  else
  {
    PRBool useDefaultFCC = PR_TRUE;
    const char *fieldsFCC = fields->GetFcc();
    if (fieldsFCC && *fieldsFCC)
    {
      if (PL_strcasecmp(fieldsFCC, "nocopy://") == 0)
      {
        useDefaultFCC = PR_FALSE;
        mCompFields->SetFcc("");
      }
      else
      {
        nsCOMPtr<nsIMsgFolder> folder;
        (void)GetExistingFolder(fieldsFCC, getter_AddRefs(folder));
        if (folder)
        {
          useDefaultFCC = PR_FALSE;
          SetMimeHeader(nsMsgCompFields::MSG_FCC_HEADER_ID, fieldsFCC); 
        }
      }
    }
    
    // We use default FCC setting if it's not set or was set to an invalid folder.
    if (useDefaultFCC)
    {
      char *uri = GetFolderURIFromUserPrefs(nsMsgDeliverNow, mUserIdentity);
      if ( (uri) && (*uri) )
      {
        if (PL_strcasecmp(uri, "nocopy://") == 0)
          mCompFields->SetFcc("");
        else
          mCompFields->SetFcc(uri);
        PL_strfree(uri);
      }
      else
        mCompFields->SetFcc("");
    }
  }

  //
  // Deal with an additional FCC operation for this email.
  //
  const char *fieldsFCC2 = fields->GetFcc2();
  if ( (fieldsFCC2) && (*fieldsFCC2) )
  {
    if (PL_strcasecmp(fieldsFCC2, "nocopy://") == 0)
    {
      mCompFields->SetFcc2("");
      mNeedToPerformSecondFCC = PR_FALSE;
    }
    else
    {
      mCompFields->SetFcc2(fieldsFCC2);
      mNeedToPerformSecondFCC = PR_TRUE;
    }
  }

  mCompFields->SetNewspostUrl((char *) fields->GetNewspostUrl());

  /* strip whitespace from and duplicate header fields. */
  SetMimeHeader(nsMsgCompFields::MSG_FROM_HEADER_ID, fields->GetFrom());
  SetMimeHeader(nsMsgCompFields::MSG_REPLY_TO_HEADER_ID, fields->GetReplyTo());
  SetMimeHeader(nsMsgCompFields::MSG_TO_HEADER_ID, fields->GetTo());
  SetMimeHeader(nsMsgCompFields::MSG_CC_HEADER_ID, fields->GetCc());
  SetMimeHeader(nsMsgCompFields::MSG_BCC_HEADER_ID, fields->GetBcc());
  SetMimeHeader(nsMsgCompFields::MSG_NEWSGROUPS_HEADER_ID, fields->GetNewsgroups());
  SetMimeHeader(nsMsgCompFields::MSG_FOLLOWUP_TO_HEADER_ID, fields->GetFollowupTo());
  SetMimeHeader(nsMsgCompFields::MSG_ORGANIZATION_HEADER_ID, fields->GetOrganization());
  SetMimeHeader(nsMsgCompFields::MSG_SUBJECT_HEADER_ID, fields->GetSubject());
  SetMimeHeader(nsMsgCompFields::MSG_REFERENCES_HEADER_ID, fields->GetReferences());
  SetMimeHeader(nsMsgCompFields::MSG_X_TEMPLATE_HEADER_ID, fields->GetTemplateName());

  nsCOMPtr<nsISupportsArray> srcAttachmentArray;
  fields->GetAttachmentsArray(getter_AddRefs(srcAttachmentArray));
  if (srcAttachmentArray)
  {
    PRUint32 i;
    PRUint32 attachmentCount = 0;
    srcAttachmentArray->Count(&attachmentCount);
    if (attachmentCount > 0)
    {
      nsCOMPtr<nsIMsgAttachment> element;
      for (i = 0; i < attachmentCount; i ++)
      {
        srcAttachmentArray->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
        if (element)
          mCompFields->AddAttachment(element);
      }
    }
  }

  pStr = fields->GetOtherRandomHeaders();
  if (pStr)
    mCompFields->SetOtherRandomHeaders((char *) pStr);
 
  AddDefaultCustomHeaders();
                                                            
  pStr = fields->GetPriority();
  if (pStr)
    mCompFields->SetPriority((char *) pStr);

  mCompFields->SetAttachVCard(fields->GetAttachVCard());
  mCompFields->SetForcePlainText(fields->GetForcePlainText());
  mCompFields->SetUseMultipartAlternative(fields->GetUseMultipartAlternative());
  PRInt32 receiptType = nsIMsgMdnGenerator::eDntType; 
  fields->GetReceiptHeaderType(&receiptType);

  mCompFields->SetReturnReceipt(fields->GetReturnReceipt());
  mCompFields->SetReceiptHeaderType(receiptType);

  mCompFields->SetUuEncodeAttachments(fields->GetUuEncodeAttachments());

  mCompFields->SetBodyIsAsciiOnly(fields->GetBodyIsAsciiOnly());

  nsCOMPtr<nsISupports> secInfo;
  fields->GetSecurityInfo(getter_AddRefs(secInfo));

  mCompFields->SetSecurityInfo(secInfo);

  // Check the fields for legitimacy...
  //
  if ( m_deliver_mode != nsMsgSaveAsDraft && m_deliver_mode != nsMsgSaveAsTemplate ) 
  {
    rv = mime_sanity_check_fields (mCompFields->GetFrom(), mCompFields->GetReplyTo(),
                    mCompFields->GetTo(), mCompFields->GetCc(),
                    mCompFields->GetBcc(), mCompFields->GetFcc(),
                    mCompFields->GetNewsgroups(), mCompFields->GetFollowupTo(),
                    mCompFields->GetSubject(), mCompFields->GetReferences(),
                    mCompFields->GetOrganization(),
                    mCompFields->GetOtherRandomHeaders());
  }
  
  return rv;
}

// Add default headers to outgoing messages see Bug #61520
// mail.identity.<id#>.headers pref is a comma separated value of pref names
// containging headers to add headers are stored in
// mail.identity.<id#>.header.<header name> grab all the headers, mime encode
// them and add them to the other custom headers.
nsresult
nsMsgComposeAndSend::AddDefaultCustomHeaders() {
  nsXPIDLCString headersList;
  // get names of prefs containing headers to add
  nsresult rv = mUserIdentity->GetCharAttribute("headers",
                                                getter_Copies(headersList));
  if (NS_SUCCEEDED(rv) && !headersList.IsEmpty()) {
    PRInt32 start = 0;
    PRInt32 end = 0;
    PRInt32 len = 0;
    // preserve any custom headers that have been added through the UI
    nsCAutoString newHeaderVal(mCompFields->GetOtherRandomHeaders());
    
    while (end != -1) {
      end = headersList.FindChar(',', start);
      if (end == -1) {
        len = headersList.Length() - start;
      } else {
        len = end - start;
      }
      // grab the name of the current header pref
      nsCAutoString headerName(NS_LITERAL_CSTRING("header.") +
                               Substring(headersList, start, len));
      start = end + 1;
      
      nsXPIDLCString headerVal;
      rv = mUserIdentity->GetCharAttribute(headerName.get(),
                                           getter_Copies(headerVal));
      if (NS_SUCCEEDED(rv)) {
        PRInt32 colonIdx = headerVal.FindChar(':') + 1;
        if (colonIdx != 0) { // check that the header is *most likely* valid.
          char * convHeader =
            nsMsgI18NEncodeMimePartIIStr(headerVal.get() + colonIdx,
                                         PR_FALSE,
                                         mCompFields->GetCharacterSet(),
                                         colonIdx,
                                         PR_TRUE);
          if (convHeader) {
            newHeaderVal.Append(Substring(headerVal, 0, colonIdx));
            newHeaderVal.Append(convHeader);
            // we must terminate the header with CRLF here
            // as nsMsgCompUtils.cpp just calls PUSH_STRING
            newHeaderVal.Append("\r\n");
            PR_Free(convHeader);
          }
        }
      }
    }
    mCompFields->SetOtherRandomHeaders(newHeaderVal.get());
  }
  return rv;
}


nsresult
nsMsgComposeAndSend::SnarfAndCopyBody(const char  *attachment1_body,
                                      PRUint32    attachment1_body_length,
                                      const char  *attachment1_type)
{
  //
  // If we are here, then just process the body from what was
  // passed in the attachment_1_body field.
  //
  // Strip whitespace from beginning and end of body.
  if (attachment1_body)
  {
    // strip out whitespaces from the end of body ONLY.
    while (attachment1_body_length > 0 &&
      (attachment1_body[attachment1_body_length - 1] == ' ') )
    {
      attachment1_body_length--;
    }

    if (attachment1_body_length > 0)
    {
     // will set m_attachment1_body and m_attachment1_body_length
     nsresult rv = EnsureLineBreaks(attachment1_body, attachment1_body_length);
     NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  
  PR_FREEIF(m_attachment1_type);
  m_attachment1_type = PL_strdup (attachment1_type);
  PR_FREEIF(m_attachment1_encoding);
  m_attachment1_encoding = PL_strdup ("8bit");
  return NS_OK;
}

nsresult
nsMsgComposeAndSend::Init(
              nsIMsgIdentity  *aUserIdentity,
              const char *aAccountKey,
              nsMsgCompFields *fields,
              nsFileSpec      *sendFileSpec,
              PRBool digest_p,
              PRBool dont_deliver_p,
              nsMsgDeliverMode mode,
              nsIMsgDBHdr *msgToReplace,
              const char *attachment1_type,
              const char *attachment1_body,
              PRUint32 attachment1_body_length,
              const nsMsgAttachmentData *attachments,
              const nsMsgAttachedFile *preloaded_attachments,
              const char *password)
{
  nsresult      rv = NS_OK;
  
  //Reset last error
  mLastErrorReported = NS_OK;
  
  //Let make sure we retreive the correct number of related parts. It may have changed since last time
  GetMultipartRelatedCount(PR_TRUE);

  nsXPIDLString msg;
  if (!mComposeBundle)
    mComposeBundle = do_GetService(NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID);

  // Tell the user we are assembling the message...
  mComposeBundle->GetStringByID(NS_MSG_ASSEMBLING_MESSAGE, getter_Copies(msg));
  SetStatusMessage( msg );
  if (mSendReport)
    mSendReport->SetCurrentProcess(nsIMsgSendReport::process_BuildMessage);

  RETURN_SIMULATED_ERROR(SIMULATED_SEND_ERROR_1, NS_ERROR_FAILURE);

  // 
  // The Init() method should initialize a send operation for full
  // blown create and send operations as well as just the "send a file"
  // operations. 
  //
  m_dont_deliver_p = dont_deliver_p;
  m_deliver_mode = mode;
  mMsgToReplace = msgToReplace;

  mUserIdentity = aUserIdentity;
  mAccountKey = aAccountKey;
  NS_ASSERTION(mUserIdentity, "Got null identity!\n");
  if (!mUserIdentity) return NS_ERROR_UNEXPECTED;

  //
  // First sanity check the composition fields parameter and
  // see if we should continue
  //
  if (!fields)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = InitCompositionFields(fields);
  if (NS_FAILED(rv))
    return rv;

  //
  // At this point, if we are only creating this object to do
  // send operations on externally created RFC822 disk files, 
  // make sure we have setup the appropriate nsFileSpec and
  // move on with life.
  //
  //
  // First check to see if we are doing a send operation on an external file
  // or creating the file itself.
  //
  if (sendFileSpec)
  {
    mTempFileSpec = sendFileSpec;
    return NS_OK;
  }

  m_digest_p = digest_p;

  //
  // Needed for mime encoding!
  //
  PRBool strictly_mime = PR_TRUE; 
  nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pPrefBranch)
  {
    rv = pPrefBranch->GetBoolPref(PREF_MAIL_STRICTLY_MIME, &strictly_mime);
    rv = pPrefBranch->GetIntPref(PREF_MAIL_MESSAGE_WARNING_SIZE, (PRInt32 *) &mMessageWarningSize);
  }

  nsMsgMIMESetConformToStandard(strictly_mime);
  mime_use_quoted_printable_p = strictly_mime;

  // Ok, now watch me pull a rabbit out of my hat....what we need
  // to do here is figure out what the body will be. If this is a
  // MHTML request, then we need to do some processing of the document
  // and figure out what we need to package along with this message
  // to send. See ProcessMultipartRelated() for further details.
  //

  //
  // If we don't have an editor, then we won't be doing multipart related processing 
  // for the body, so make a copy of the one passed in.
  //
  if (!mEditor)
  {
    SnarfAndCopyBody(attachment1_body, attachment1_body_length, attachment1_type);
  }
  else if (GetMultipartRelatedCount() == 0) // Only do this if there are not embedded objects
  {
    rv = GetBodyFromEditor();
    if (NS_FAILED(rv))
      return rv;
  }

  mSmtpPassword = password;

  return HackAttachments(attachments, preloaded_attachments);
}

nsresult
SendDeliveryCallback(nsIURI *aUrl, nsresult aExitCode, nsMsgDeliveryType deliveryType, nsISupports *tagData)
{
  if (tagData)
  {
    nsCOMPtr<nsIMsgSend> msgSend = do_QueryInterface(tagData);
    if (!msgSend)
      return NS_ERROR_NULL_POINTER;

    if (deliveryType == nsMailDelivery)
    {
      if (NS_FAILED(aExitCode))
        switch (aExitCode)
        {
          case NS_ERROR_UNKNOWN_HOST:
          case NS_ERROR_UNKNOWN_PROXY_HOST:
            aExitCode = NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER;
            break;
          default:
            if (aExitCode != NS_ERROR_ABORT && !NS_IS_MSG_ERROR(aExitCode))
              aExitCode = NS_ERROR_SMTP_SEND_FAILED;
            break;
        }
      msgSend->DeliverAsMailExit(aUrl, aExitCode);
    }
    
    else if (deliveryType == nsNewsDelivery)
    {
      if (NS_FAILED(aExitCode))
        if (aExitCode != NS_ERROR_ABORT && !NS_IS_MSG_ERROR(aExitCode))
          aExitCode = NS_ERROR_POST_FAILED;
      
      msgSend->DeliverAsNewsExit(aUrl, aExitCode);
    }

    msgSend->SetRunningRequest(nsnull);
  }

  return aExitCode;
}

nsresult
nsMsgComposeAndSend::DeliverMessage()
{
  if (mSendProgress)
  {
    PRBool canceled = PR_FALSE;
    mSendProgress->GetProcessCanceledByUser(&canceled);
    if (canceled)
      return NS_ERROR_ABORT;
  }

  PRBool mail_p = ((mCompFields->GetTo() && *mCompFields->GetTo()) || 
          (mCompFields->GetCc() && *mCompFields->GetCc()) || 
          (mCompFields->GetBcc() && *mCompFields->GetBcc()));
  PRBool news_p = mCompFields->GetNewsgroups() && *(mCompFields->GetNewsgroups());
  NS_ASSERTION(!( m_deliver_mode != nsMsgSaveAsDraft && m_deliver_mode != nsMsgSaveAsTemplate)  || (mail_p || news_p), "message without destination");
  if (m_deliver_mode == nsMsgQueueForLater) 
    return QueueForLater();
  else if (m_deliver_mode == nsMsgSaveAsDraft) 
    return SaveAsDraft();
  else if (m_deliver_mode == nsMsgSaveAsTemplate)
    return SaveAsTemplate();

  //
  // Ok, we are about to send the file that we have built up...but what
  // if this is a mongo email...we should have a way to warn the user that
  // they are about to do something they may not want to do.
  //
  if (((mMessageWarningSize > 0) && (mTempFileSpec->GetFileSize() > mMessageWarningSize) && (mGUINotificationEnabled)) ||
      CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_15))
  {
    PRBool abortTheSend = PR_FALSE;
    
    nsXPIDLString msg;
    mComposeBundle->GetStringByID(NS_MSG_LARGE_MESSAGE_WARNING, getter_Copies(msg));
    
    if (msg)
    {
      PRUnichar *printfString = nsTextFormatter::smprintf(msg, mTempFileSpec->GetFileSize());

      if (printfString)
      {
        nsCOMPtr<nsIPrompt> prompt;
        GetDefaultPrompt(getter_AddRefs(prompt));

        nsMsgAskBooleanQuestionByString(prompt, printfString, &abortTheSend);
        if (!abortTheSend)
        {
          nsresult ignoreMe;
          Fail(NS_ERROR_BUT_DONT_SHOW_ALERT, printfString, &ignoreMe);
          PR_Free(printfString);
          return NS_ERROR_FAILURE;
        }
        else
          PR_Free(printfString);
      }

    }
  }

  if (news_p) 
  {
    if (mail_p) 
      mSendMailAlso = PR_TRUE;

    return DeliverFileAsNews();   /* will call DeliverFileAsMail if it needs to */
  }
  else if (mail_p) 
  {
    return DeliverFileAsMail();
  }
  else
  {
    return NS_ERROR_UNEXPECTED;
  }
  
  return NS_OK;
}

// strip out non-printable characters 
static void 
strip_nonprintable(char *string) 
{
  char *dest, *src;
  
  if ( (!string) || (!*string) ) return;
  dest=src=string;
  while (*src) 
  {
    if  ( (isprint(*src)) && (*src != ' ') )
    {
      (*dest)=(*src);
      src++; dest++;
    } 
    else 
    {
      src++;
    }
  }

  (*dest)='\0';
}

nsresult
nsMsgComposeAndSend::DeliverFileAsMail()
{
  char *buf, *buf2;
  buf = (char *) PR_Malloc ((mCompFields->GetTo() ? PL_strlen (mCompFields->GetTo())  + 10 : 0) +
               (mCompFields->GetCc() ? PL_strlen (mCompFields->GetCc())  + 10 : 0) +
               (mCompFields->GetBcc() ? PL_strlen (mCompFields->GetBcc()) + 10 : 0) +
               10);
  
  if (mSendReport)
    mSendReport->SetCurrentProcess(nsIMsgSendReport::process_SMTP);

  nsCOMPtr<nsIPrompt> promptObject;
  GetDefaultPrompt(getter_AddRefs(promptObject));

  if (!buf) 
  {
    nsXPIDLString eMsg; 
    mComposeBundle->GetStringByID(NS_ERROR_OUT_OF_MEMORY, getter_Copies(eMsg));
    
    nsresult ignoreMe;
    Fail(NS_ERROR_OUT_OF_MEMORY, eMsg, &ignoreMe);
    NotifyListenerOnStopSending(nsnull, NS_ERROR_OUT_OF_MEMORY, nsnull, nsnull);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRBool collectOutgoingAddresses = PR_TRUE;
  nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pPrefBranch)
    pPrefBranch->GetBoolPref(PREF_MAIL_COLLECT_EMAIL_ADDRESS_OUTGOING, &collectOutgoingAddresses);
 
  nsCOMPtr<nsIAbAddressCollecter> addressCollecter = 
           do_GetService(NS_ABADDRESSCOLLECTER_CONTRACTID);

  PRBool collectAddresses = (collectOutgoingAddresses && addressCollecter);
  PRBool forcePlainText = mCompFields->GetForcePlainText();
  PRBool useMultipartAlternative = mCompFields->GetUseMultipartAlternative();
  PRUint32 sendFormat = nsIAbPreferMailFormat::unknown;

  // this code is not ready yet
  // see bug #44494 for more details
  // so for now, just pass in nsIAbPreferMailFormat::unknown
  // which will have no effect on the "prefers" attribute in the ab
#if 0
  // see GenericSendMessage() in MsgComposeCommands.js for the reverse logic
  // if we choose to send both (html and plain) remember html.
  if (forcePlainText && !useMultipartAlternative)
  {
    // for now, don't remember the "plaintext" decision.
    // we could get in here because while sending html mail
    // the body was "convertible", but that doesn't mean
    // we intended to force plain text here.
    // so for now, use "unknown" which will have no effect on the
    // "prefers" attribute in the ab.
    // see bug #245520 for more details
    // sendFormat = nsIAbPreferMailFormat::plaintext;
    sendFormat = nsIAbPreferMailFormat::unknown;
  }
  else if (!forcePlainText)
    sendFormat = nsIAbPreferMailFormat::html;
  else
    NS_ASSERTION(0,"unknown send format, should not happen");
#endif

  PL_strcpy (buf, "");
  buf2 = buf + PL_strlen (buf);
  if (mCompFields->GetTo() && *mCompFields->GetTo())
  {
    PL_strcat (buf2, mCompFields->GetTo());
    addressCollecter->CollectAddress(mCompFields->GetTo(), collectAddresses /* create card if one doesn't exist */, sendFormat);
  }
  if (mCompFields->GetCc() && *mCompFields->GetCc()) {
    if (*buf2) PL_strcat (buf2, ",");
      PL_strcat (buf2, mCompFields->GetCc());
    addressCollecter->CollectAddress(mCompFields->GetCc(), collectAddresses /* create card if one doesn't exist */, sendFormat);
  }
  if (mCompFields->GetBcc() && *mCompFields->GetBcc()) {
    if (*buf2) PL_strcat (buf2, ",");
      PL_strcat (buf2, mCompFields->GetBcc());
    addressCollecter->CollectAddress(mCompFields->GetBcc(), collectAddresses /* create card if one doesn't exist */, sendFormat);
  }

  // We need undo groups to keep only the addresses
  nsresult rv = StripOutGroupNames(buf);
  NS_ENSURE_SUCCESS(rv, rv);

  // Ok, now MIME II encode this to prevent 8bit problems...
  char *convbuf = nsMsgI18NEncodeMimePartIIStr(buf, PR_TRUE, 
            mCompFields->GetCharacterSet(), 0, nsMsgMIMEGetConformToStandard());
  if (convbuf) 
  {     
    // MIME-PartII conversion 
    PR_FREEIF(buf);
    buf = convbuf;
  }

  strip_nonprintable(buf);

  convbuf = nsEscape(buf, url_Path);
  if (convbuf)
  {
      nsCRT::free(buf);
      buf = convbuf;
  }
  
  nsCOMPtr<nsISmtpService> smtpService(do_GetService(NS_SMTPSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv) && smtpService)
  {
    nsMsgDeliveryListener * aListener = new nsMsgDeliveryListener(SendDeliveryCallback, nsMailDelivery, this);
    nsCOMPtr<nsIUrlListener> uriListener = do_QueryInterface(aListener);
    if (!uriListener)
      return NS_ERROR_OUT_OF_MEMORY;

    // Note: Don't do a SetMsgComposeAndSendObject since we are in the same thread, and
    // using callbacks for notification

    nsCOMPtr<nsIFileSpec> aFileSpec;
    NS_NewFileSpecWithSpec(*mTempFileSpec, getter_AddRefs(aFileSpec));

    // we used to get the prompt from the compose window and we'd pass that in
    // to the smtp protocol as the prompt to use. But when you send a message,
    // we dismiss the compose window.....so you are parenting off of a window that
    // isn't there. To have it work correctly I think we want the alert dialogs to be modal
    // to the top most mail window...after all, that's where we are going to be sending status
    // update information too....

    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    GetNotificationCallbacks(getter_AddRefs(callbacks));

    // Tell the user we are sending the message!
    nsXPIDLString msg; 
    mComposeBundle->GetStringByID(NS_MSG_SENDING_MESSAGE, getter_Copies(msg));
    SetStatusMessage( msg );
    nsCOMPtr<nsIMsgStatusFeedback> msgStatus (do_QueryInterface(mSendProgress));
    // if the sendProgress isn't set, let's use the member variable.
    if (!msgStatus)
      msgStatus = do_QueryInterface(mStatusFeedback);

    rv = smtpService->SendMailMessage(aFileSpec, buf, mUserIdentity,
                                      mSmtpPassword.get(), uriListener, msgStatus,
                                      callbacks, nsnull, getter_AddRefs(mRunningRequest));
  }
  
  PR_FREEIF(buf); // free the buf because we are done with it....
  return rv;
}

nsresult
nsMsgComposeAndSend::DeliverFileAsNews()
{
  nsresult rv = NS_OK;
  if (!(mCompFields->GetNewsgroups()))
    return rv;
  
  if (mSendReport)
    mSendReport->SetCurrentProcess(nsIMsgSendReport::process_NNTP);
  
  nsCOMPtr<nsIPrompt> promptObject;
  GetDefaultPrompt(getter_AddRefs(promptObject));
  
  nsCOMPtr<nsINntpService> nntpService(do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv));
  
  if (NS_SUCCEEDED(rv) && nntpService) 
  {
    nsMsgDeliveryListener * aListener = new nsMsgDeliveryListener(SendDeliveryCallback, nsNewsDelivery, this);
    nsCOMPtr<nsIUrlListener> uriListener = do_QueryInterface(aListener);
    if (!uriListener)
      return NS_ERROR_OUT_OF_MEMORY;
    
    // Note: Don't do a SetMsgComposeAndSendObject since we are in the same thread, and
    // using callbacks for notification
    
    nsCOMPtr<nsIFileSpec>fileToPost;
    
    rv = NS_NewFileSpecWithSpec(*mTempFileSpec, getter_AddRefs(fileToPost));
    if (NS_FAILED(rv)) return rv;
    
    // Tell the user we are posting the message!
    nsXPIDLString msg; 
    mComposeBundle->GetStringByID(NS_MSG_POSTING_MESSAGE, getter_Copies(msg));
    SetStatusMessage( msg );
    
    nsCOMPtr <nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    if (!mailSession) return NS_ERROR_FAILURE;
    
    // JFD TODO: we should use GetDefaultPrompt instead
    nsCOMPtr<nsIMsgWindow> msgWindow;
    rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
    // see bug #163139
    // we might not have a msg window if only the compose window is open.
    if(NS_FAILED(rv))
      msgWindow = nsnull;

    rv = nntpService->PostMessage(fileToPost, mCompFields->GetNewsgroups(), mAccountKey.get(),
                                  uriListener, msgWindow, nsnull);
    if (NS_FAILED(rv)) return rv;
  }
  
  return rv;
}

NS_IMETHODIMP 
nsMsgComposeAndSend::Fail(nsresult failure_code, const PRUnichar * error_msg, nsresult *_retval)
{
  NS_ENSURE_ARG(_retval);
  *_retval = failure_code;
  
  if (NS_FAILED(failure_code))
  {
    nsCOMPtr<nsIPrompt> prompt;
    GetDefaultPrompt(getter_AddRefs(prompt));
      
    if (mSendReport)
    {
      mSendReport->SetError(nsIMsgSendReport::process_Current, failure_code, PR_FALSE);
      mSendReport->SetMessage(nsIMsgSendReport::process_Current, error_msg, PR_FALSE);
      mSendReport->DisplayReport(prompt, PR_TRUE, PR_TRUE, _retval);
    }
    else
    {
      if (failure_code != NS_ERROR_BUT_DONT_SHOW_ALERT)
        nsMsgDisplayMessageByID(prompt, NS_ERROR_SEND_FAILED);
    }
  }

  if (m_attachments_done_callback)
  {
    /* mime_free_message_state will take care of cleaning up the
       attachment files and attachment structures */
    m_attachments_done_callback (failure_code, error_msg, nsnull);
    m_attachments_done_callback = nsnull;
  }
  
  if (m_status == NS_OK)
    m_status = NS_ERROR_BUT_DONT_SHOW_ALERT;

  //Stop any pending process...
  Abort();
  
  return NS_OK;
}

nsresult
nsMsgComposeAndSend::FormatStringWithSMTPHostNameByID(PRInt32 aMsgId, PRUnichar **aString)
{
  NS_ENSURE_ARG(aString);

  nsresult rv;
  nsCOMPtr<nsISmtpService> smtpService(do_GetService(NS_SMTPSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv,rv);

  // Get the smtp hostname and format the string.
  nsXPIDLCString smtpHostName;
  nsCOMPtr<nsISmtpServer> smtpServer;
  rv = smtpService->GetSmtpServerByIdentity(mUserIdentity, getter_AddRefs(smtpServer));
  if (NS_SUCCEEDED(rv))
    smtpServer->GetHostname(getter_Copies(smtpHostName));

  nsAutoString hostStr;
  hostStr.AssignWithConversion(smtpHostName.get());
  const PRUnichar *params[] = { hostStr.get() };
  nsCOMPtr<nsIStringBundle> bundle;
  rv = mComposeBundle->GetBundle(getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv))
    bundle->FormatStringFromID(NS_ERROR_GET_CODE(aMsgId), params, 1, aString);
  return rv;
}

void
nsMsgComposeAndSend::DoDeliveryExitProcessing(nsIURI * aUri, nsresult aExitCode, PRBool aCheckForMail)
{  
  // If we fail on the news delivery, no sense in going on so just notify
  // the user and exit.
  if (NS_FAILED(aExitCode))
  {
#ifdef NS_DEBUG
  printf("\nMessage Delivery Failed!\n");
#endif

    nsXPIDLString eMsg; 
    if (aExitCode == NS_ERROR_SMTP_SEND_FAILED || aExitCode == NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER)
      FormatStringWithSMTPHostNameByID(aExitCode, getter_Copies(eMsg));
    else
    mComposeBundle->GetStringByID(aExitCode, getter_Copies(eMsg));
    
    Fail(aExitCode, eMsg, &aExitCode);
    NotifyListenerOnStopSending(nsnull, aExitCode, nsnull, nsnull);
    return;
  }
#ifdef NS_DEBUG
  else
    printf("\nMessage Delivery SUCCEEDED!\n");
#endif

  if (aCheckForMail)
  {
    if ((mCompFields->GetTo() && *mCompFields->GetTo()) || 
        (mCompFields->GetCc() && *mCompFields->GetCc()) || 
        (mCompFields->GetBcc() && *mCompFields->GetBcc()))
    {
      // If we're sending this news message to mail as well, start it now.
      // Completion and further errors will be handled there.
      DeliverFileAsMail();
      return;
    }
  }

  //
  // Tell the listeners that we are done with the sending operation...
  //
  NotifyListenerOnStopSending(nsnull, aExitCode, nsnull, nsnull);

  // If we hit here, we are done with delivery!
  //
  // Just call the DoFCC() method and if that fails, then we should just 
  // cleanup and get out. If DoFCC "succeeds", then all that means is the
  // async copy operation has been started and we will be notified later 
  // when it is done. DON'T cleanup until the copy is complete and don't
  // notify the listeners with OnStop() until we are done.
  //
  // For now, we don't need to do anything here, but the code will stay this 
  // way until later...
  //

  nsresult retCode = DoFcc();
  if (NS_FAILED(retCode))
  {
#ifdef NS_DEBUG
  printf("\nDoDeliveryExitProcessing(): DoFcc() call Failed!\n");
#endif
    return;
  } 
  else
  {
    // Either we started the copy...cleanup happens later...or cleanup will
    // be take care of by a listener...
    return;
  }
}

NS_IMETHODIMP
nsMsgComposeAndSend::DeliverAsMailExit(nsIURI *aUrl, nsresult aExitCode)
{
  DoDeliveryExitProcessing(aUrl, aExitCode, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeAndSend::DeliverAsNewsExit(nsIURI *aUrl, nsresult aExitCode)
{
  DoDeliveryExitProcessing(aUrl, aExitCode, mSendMailAlso);
  return NS_OK;
}

PRBool nsMsgComposeAndSend::CanSaveMessagesToFolder(const char *folderURL)
{
  nsresult rv;
  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
  if (NS_FAILED(rv))
    return PR_FALSE;

  nsCOMPtr<nsIRDFResource> resource;
  rv = rdf->GetResource(nsDependentCString(folderURL), getter_AddRefs(resource));
  if (NS_FAILED(rv))
    return PR_FALSE;

  nsCOMPtr <nsIMsgFolder> thisFolder;
  thisFolder = do_QueryInterface(resource, &rv);
  if (NS_FAILED(rv) || !thisFolder)
    return PR_FALSE;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = thisFolder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server)
    return PR_FALSE;

  // See if we are allowed to save/file msgs to this folder.
  PRBool canSave;
  rv = server->GetCanFileMessagesOnServer(&canSave);
  return canSave;
}

// 
// Now, start the appropriate copy operation.
//
nsresult
nsMsgComposeAndSend::DoFcc()
{
  //
  // Just cleanup and return success if we're not allowed to save msgs to FCC folder.
  //
  if (! CanSaveMessagesToFolder(mCompFields->GetFcc()))
  {
#ifdef NS_DEBUG
  printf("\nCopy operation disabled by user!\n");
#endif

    NotifyListenerOnStopSending(nsnull, NS_OK, nsnull, nsnull);
    NotifyListenerOnStopCopy(NS_OK);  // For closure of compose window...
    return NS_OK;
  }

  if (mSendReport)
    mSendReport->SetCurrentProcess(nsIMsgSendReport::process_Copy);

  //
  // If we are here, then we need to save off the FCC file to save and
  // start the copy operation. MimeDoFCC() will take care of all of this
  // for us.
  //
  nsresult rv = MimeDoFCC(mTempFileSpec,
                          nsMsgDeliverNow,
                          mCompFields->GetBcc(),
                          mCompFields->GetFcc(), 
                          mCompFields->GetNewspostUrl());
  if (NS_FAILED(rv))
  {
    //
    // If we hit here, the copy operation FAILED and we should at least tell the
    // user that it did fail but the send operation has already succeeded.
    //
    NotifyListenerOnStopCopy(rv);
  }

  return rv;
}

NS_IMETHODIMP
nsMsgComposeAndSend::NotifyListenerOnStartSending(const char *aMsgID, PRUint32 aMsgSize)
{
  if (mListener)
    mListener->OnStartSending(aMsgID, aMsgSize);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeAndSend::NotifyListenerOnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax)
{
  if (mListener)
    mListener->OnProgress(aMsgID, aProgress, aProgressMax);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeAndSend::NotifyListenerOnStatus(const char *aMsgID, const PRUnichar *aMsg)
{
  if (mListener)
    mListener->OnStatus(aMsgID, aMsg);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeAndSend::NotifyListenerOnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                                                  nsIFileSpec *returnFileSpec)
{
  if (mListener != nsnull)
    mListener->OnStopSending(aMsgID, aStatus, aMsg, returnFileSpec);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeAndSend::NotifyListenerOnStartCopy()
{
  nsCOMPtr<nsIMsgCopyServiceListener> copyListener;

  if (mListener)
  {
    copyListener = do_QueryInterface(mListener);
    if (copyListener)
      copyListener->OnStartCopy();      
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeAndSend::NotifyListenerOnProgressCopy(PRUint32 aProgress,
                                                   PRUint32 aProgressMax)
{
  nsCOMPtr<nsIMsgCopyServiceListener> copyListener;

  if (mListener)
  {
    copyListener = do_QueryInterface(mListener);
    if (copyListener)
      copyListener->OnProgress(aProgress, aProgressMax);
  }

  return NS_OK;  
}

NS_IMETHODIMP
nsMsgComposeAndSend::SetMessageKey(PRUint32 aMessageKey)
{
    m_messageKey = aMessageKey;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeAndSend::GetMessageKey(PRUint32 *aMessageKey)
{
    *aMessageKey = m_messageKey;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeAndSend::GetMessageId(nsCString* aMessageId)
{
  NS_ENSURE_ARG(aMessageId);
  
  if (mCompFields)
  {
    *aMessageId = mCompFields->GetMessageId();
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsMsgComposeAndSend::NotifyListenerOnStopCopy(nsresult aStatus)
{
  nsCOMPtr<nsIMsgCopyServiceListener> copyListener;

  // This is one per copy so make sure we clean this up first.
  if (mCopyObj)
  {
    NS_RELEASE(mCopyObj);
    mCopyObj = nsnull;
  }

  // Set a status message...
  nsXPIDLString msg;

  if (NS_SUCCEEDED(aStatus))
    mComposeBundle->GetStringByID(NS_MSG_START_COPY_MESSAGE_COMPLETE, getter_Copies(msg));
  else
    mComposeBundle->GetStringByID(NS_MSG_START_COPY_MESSAGE_FAILED, getter_Copies(msg));

  SetStatusMessage( msg );
  nsCOMPtr<nsIPrompt> prompt;
  GetDefaultPrompt(getter_AddRefs(prompt));

  if (NS_FAILED(aStatus))
  {
    PRBool retry = PR_FALSE;
    nsMsgAskBooleanQuestionByID(prompt, NS_MSG_ERROR_DOING_FCC, &retry, nsnull /* what title */);
    if (retry)
    {
      return DoFcc();
    }

  }
  // Ok, now to support a second copy operation, we need to figure
  // out which copy request just finished. If the user has requested
  // a second copy operation, then we need to fire that off, but if they
  // just wanted a single copy operation, we can tell everyone we are done
  // and move on with life. Only do the second copy if the first one worked.
  //
  if ( NS_SUCCEEDED(aStatus) && (mNeedToPerformSecondFCC) )
  {
    if (mSendReport)
      mSendReport->SetCurrentProcess(nsIMsgSendReport::process_FCC);

    mNeedToPerformSecondFCC = PR_FALSE;

    const char *fcc2 = mCompFields->GetFcc2();
    if (fcc2 && *fcc2)
    {
      nsresult rv = MimeDoFCC(mTempFileSpec,
                              nsMsgDeliverNow,
                              mCompFields->GetBcc(),
                              fcc2, 
                              mCompFields->GetNewspostUrl());
      if (NS_FAILED(rv))
        Fail(rv, nsnull, &aStatus);
      else
        return NS_OK;
    }
  }
  else if (NS_FAILED(aStatus))
  {
    //
    // If we hit here, the ASYNC copy operation FAILED and we should at least tell the
    // user that it did fail but the send operation has already succeeded. This only if
    // we are sending the message and not just saving it!
    
    Fail(aStatus, nsnull, &aStatus);
  }

  // If we are here, its real cleanup time! 
  if (mListener)
  {
    copyListener = do_QueryInterface(mListener);
    if (copyListener)
      copyListener->OnStopCopy(aStatus);
  }

  return aStatus;
}

/* This is the main driving function of this module.  It generates a
   document of type message/rfc822, which contains the stuff provided.
   The first few arguments are the standard header fields that the
   generated document should have.

   `other_random_headers' is a string of additional headers that should
   be inserted beyond the standard ones.  If provided, it is just tacked
   on to the end of the header block, so it should have newlines at the
   end of each line, shouldn't have blank lines, multi-line headers
   should be properly continued, etc.

   `digest_p' says that most of the documents we are attaching are
   themselves messages, and so we should generate a multipart/digest
   container instead of multipart/mixed.  (It's a minor difference.)

   The full text of the first attachment is provided via `attachment1_type',
   `attachment1_body' and `attachment1_body_length'.  These may all be 0
   if all attachments are provided externally.

   Subsequent attachments are provided as URLs to load, described in the
   nsMsgAttachmentData structures.

   If `dont_deliver_p' is false, then we actually deliver the message to the
   SMTP and/or NNTP server, and the message_delivery_done_callback will be
   invoked with the status.

   If `dont_deliver_p' is true, then we just generate the message, we don't
   actually deliver it, and the message_delivery_done_callback will be called
   with the name of the generated file.  The callback is responsible for both
   freeing the file name string, and deleting the file when it is done with
   it.  If an error occurred, then `status' will be negative and
   `error_message' may be an error message to display.  If status is non-
   negative, then `error_message' contains the file name (this is kind of
   a kludge...)
 */
NS_IMETHODIMP 
nsMsgComposeAndSend::CreateAndSendMessage(
              nsIEditor                         *aEditor,
              nsIMsgIdentity                    *aUserIdentity,
              const char                        *aAccountKey,
              nsIMsgCompFields                  *fields,
              PRBool                            digest_p,
              PRBool                            dont_deliver_p,
              nsMsgDeliverMode                  mode,
              nsIMsgDBHdr                       *msgToReplace,
              const char                        *attachment1_type,
              const char                        *attachment1_body,
              PRUint32                          attachment1_body_length,
              const nsMsgAttachmentData         *attachments,
              const nsMsgAttachedFile           *preloaded_attachments,
              void                              *relatedPart,
              nsIDOMWindowInternal              *parentWindow,
              nsIMsgProgress                    *progress,
              nsIMsgSendListener                *aListener,
              const char                        *password
              )
{
  nsresult      rv;
  
  
  /* First thing to do is to reset the send errors report */
  mSendReport->Reset();
  mSendReport->SetDeliveryMode(mode);

  mParentWindow = parentWindow;
  mSendProgress = progress;
  mListener = aListener;

  if (!attachment1_body || !*attachment1_body)
  {
    attachment1_body_length = 0;
    attachment1_body = (char *) nsnull;
  }

  // Set the editor for MHTML operations if necessary
  if (aEditor)
    mEditor = aEditor;

  rv = Init(aUserIdentity, aAccountKey, (nsMsgCompFields *)fields, nsnull,
          digest_p, dont_deliver_p, mode, msgToReplace,
          attachment1_type, attachment1_body,
          attachment1_body_length,
          attachments, preloaded_attachments,
          password);

  if (NS_FAILED(rv) && mSendReport)
    mSendReport->SetError(nsIMsgSendReport::process_Current, rv, PR_FALSE);

  return rv;
}

nsresult
nsMsgComposeAndSend::SendMessageFile(
              nsIMsgIdentity                    *aUserIndentity,
              const char                        *aAccountKey,
              nsIMsgCompFields                  *fields,
              nsIFileSpec                       *sendIFileSpec,
              PRBool                            deleteSendFileOnCompletion,
              PRBool                            digest_p,
              nsMsgDeliverMode                  mode,
              nsIMsgDBHdr                       *msgToReplace,
              nsIMsgSendListener                *aListener,
              nsIMsgStatusFeedback              *aStatusFeedback,
              const char                        *password
              )
{
  nsresult      rv;

  /* First thing to do is to reset the send errors report */
  mSendReport->Reset();
  mSendReport->SetDeliveryMode(mode);

  if (!fields)
    return NS_ERROR_INVALID_ARG;

  mStatusFeedback = aStatusFeedback;
  //
  // First check to see if the external file we are sending is a valid file.
  //
  if (!sendIFileSpec)
    return NS_ERROR_INVALID_ARG;

  PRBool valid;
  if (NS_FAILED(sendIFileSpec->IsValid(&valid)))
    return NS_ERROR_INVALID_ARG;

  if (!valid)
    return NS_ERROR_INVALID_ARG;

  nsFileSpec  *sendFileSpec = nsnull;
  nsFileSpec  tempFileSpec;

  if (NS_FAILED(sendIFileSpec->GetFileSpec(&tempFileSpec)))
    return NS_ERROR_UNEXPECTED;

  sendFileSpec = new nsFileSpec(tempFileSpec);
  if (!sendFileSpec)
    return NS_ERROR_OUT_OF_MEMORY;

  // Setup the listener...
  mListener = aListener;

  // Should we delete the temp file when done?
  if (!deleteSendFileOnCompletion)
  {
    NS_NewFileSpecWithSpec(*sendFileSpec, &mReturnFileSpec);
    if (!mReturnFileSpec)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = Init(aUserIndentity, aAccountKey, (nsMsgCompFields *)fields, sendFileSpec,
            digest_p, PR_FALSE, mode, msgToReplace, 
            nsnull, nsnull, nsnull,
            nsnull, nsnull,
            password);

  if (NS_SUCCEEDED(rv))
    rv = DeliverMessage();

  if (NS_FAILED(rv) && mSendReport)
    mSendReport->SetError(nsIMsgSendReport::process_Current, rv, PR_FALSE);

  return rv;
}

nsMsgAttachmentData *
BuildURLAttachmentData(nsIURI *url)
{
  int                 attachCount = 2;  // one entry and one empty entry
  nsMsgAttachmentData *attachments = nsnull;
  const char          *theName = nsnull;

  if (!url)
    return nsnull;    

  attachments = (nsMsgAttachmentData *) PR_Malloc(sizeof(nsMsgAttachmentData) * attachCount);
  if (!attachments)
    return nsnull;

  // Now get a readable name...
  nsCAutoString spec;
  url->GetSpec(spec);
  if (!spec.IsEmpty())
  {
    theName = strrchr(spec.get(), '/');
  }

  if (!theName)
    theName = "Unknown"; // Don't I18N this string...should never happen...
  else
    theName++;

  memset(attachments, 0, sizeof(nsMsgAttachmentData) * attachCount);
  attachments[0].url = url; // The URL to attach. This should be 0 to signify "end of list".
  attachments[0].real_name = (char *)PL_strdup(theName);  // The original name of this document, which will eventually show up in the 

  NS_IF_ADDREF(url);
  return attachments;
}

//
// Send the message to the magic folder, and runs the completion/failure
// callback.
//
nsresult
nsMsgComposeAndSend::SendToMagicFolder(nsMsgDeliverMode mode)
{
    nsresult rv = MimeDoFCC(mTempFileSpec,
                            mode,
                            mCompFields->GetBcc(),
                            mCompFields->GetFcc(), 
                            mCompFields->GetNewspostUrl());
    //
    // The caller of MimeDoFCC needs to deal with failure.
    //
    if (NS_FAILED(rv))
      rv = NotifyListenerOnStopCopy(rv);
    
    return rv;
}

//
// Queues the message for later delivery, and runs the completion/failure
// callback.
//
nsresult
nsMsgComposeAndSend::QueueForLater()
{
  return SendToMagicFolder(nsMsgQueueForLater);
}

//
// Save the message to the Drafts folder, and runs the completion/failure
// callback.
//
nsresult 
nsMsgComposeAndSend::SaveAsDraft()
{
  return SendToMagicFolder(nsMsgSaveAsDraft);
}

//
// Save the message to the Template folder, and runs the completion/failure
// callback.
//
nsresult
nsMsgComposeAndSend::SaveAsTemplate()
{
  return SendToMagicFolder(nsMsgSaveAsTemplate);
}

nsresult
nsMsgComposeAndSend::SaveInSentFolder()
{
  return SendToMagicFolder(nsMsgDeliverNow);
}

char*
nsMsgGetEnvelopeLine(void)
{
  static char       result[75] = "";
  PRExplodedTime    now;
  char              buffer[128] = "";

  // Generate envelope line in format of:  From - Sat Apr 18 20:01:49 1998
  //
  // Use PR_FormatTimeUSEnglish() to format the date in US English format,
  // then figure out what our local GMT offset is, and append it (since
  // PR_FormatTimeUSEnglish() can't do that.) Generate four digit years as
  // per RFC 1123 (superceding RFC 822.)
  //
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
  PR_FormatTimeUSEnglish(buffer, sizeof(buffer),
               "%a %b %d %H:%M:%S %Y",
               &now);
  
  // This value must be in ctime() format, with English abbreviations.
  // PL_strftime("... %c ...") is no good, because it is localized.
  //
  PL_strcpy(result, "From - ");
  PL_strcpy(result + 7, buffer);
  PL_strcpy(result + 7 + 24, CRLF);
  return result;
}

nsresult 
nsMsgComposeAndSend::MimeDoFCC(nsFileSpec       *input_file, 
                               nsMsgDeliverMode mode,
                               const char       *bcc_header,
                               const char       *fcc_header,
                               const char       *news_url)
{
  nsresult      status = NS_OK;
  char          *ibuffer = 0;
  PRInt32       ibuffer_size = TEN_K;
  char          *obuffer = 0;
  PRInt32       n;
  char          *envelopeLine = nsMsgGetEnvelopeLine();
  PRBool        folderIsLocal = PR_TRUE;
  char          *turi = nsnull;
  PRUnichar     *printfString = nsnull;
  nsXPIDLString folderName;
  nsXPIDLString msg; 
  nsCOMPtr<nsIMsgFolder> folder;

  // Before continuing, just check the user has not cancel the operation
  if (mSendProgress)
  {
    PRBool canceled = PR_FALSE;
    mSendProgress->GetProcessCanceledByUser(&canceled);
    if (canceled)
      return NS_ERROR_ABORT;
    else
      mSendProgress->OnProgressChange(nsnull, nsnull, 0, 0, 0, -1);
  }

  //
  // Ok, this is here to keep track of this for 2 copy operations... 
  //
  if (mCopyFileSpec)
  {
    mCopyFileSpec2 = mCopyFileSpec;
    mCopyFileSpec = nsnull;
  }

  //
  // Create the file that will be used for the copy service!
  //
  nsFileSpec *tFileSpec = nsMsgCreateTempFileSpec("nscopy.tmp"); 
  if (!tFileSpec)
    return NS_ERROR_FAILURE;

  NS_NewFileSpecWithSpec(*tFileSpec, &mCopyFileSpec);
  if (!mCopyFileSpec)
  {
    delete tFileSpec;
    return NS_ERROR_INVALID_ARG;
  }

  nsOutputFileStream tempOutfile(*tFileSpec, PR_WRONLY | PR_CREATE_FILE, 00600);
  delete tFileSpec;
  if (! tempOutfile.is_open()) 
  {   
    if (mSendReport)
    {
      nsAutoString error_msg;
      nsAutoString path;
      NS_CopyNativeToUnicode(
        nsDependentCString(tFileSpec->GetNativePathCString()), path);
      nsMsgBuildErrorMessageByID(NS_MSG_UNABLE_TO_OPEN_TMP_FILE, error_msg, &path, nsnull);
      mSendReport->SetMessage(nsIMsgSendReport::process_Current, error_msg.get(), PR_FALSE);
    }
    status = NS_MSG_UNABLE_TO_OPEN_TMP_FILE;

    NS_RELEASE(mCopyFileSpec);
    return status;
  }

  //
  // Get our files ready...
  //
  nsInputFileStream inputFile(*input_file);
  if (!inputFile.is_open() || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_2))
  {
    if (mSendReport)
    {
      nsAutoString error_msg;
      nsAutoString path;
      NS_CopyNativeToUnicode(
        nsDependentCString(input_file->GetNativePathCString()), path);
      nsMsgBuildErrorMessageByID(NS_MSG_UNABLE_TO_OPEN_FILE, error_msg, &path, nsnull);
      mSendReport->SetMessage(nsIMsgSendReport::process_Current, error_msg.get(), PR_FALSE);
    }
    status = NS_MSG_UNABLE_TO_OPEN_FILE;
    goto FAIL;
  }

  // now the buffers...
  ibuffer = nsnull;
  while (!ibuffer && (ibuffer_size >= 1024))
  {
    ibuffer = (char *) PR_Malloc(ibuffer_size);
    if (!ibuffer)
      ibuffer_size /= 2;
  }

  if (!ibuffer)
  {
    status = NS_ERROR_OUT_OF_MEMORY;
    goto FAIL;
  }

  //
  // First, we we need to put a Berkeley "From - " delimiter at the head of 
  // the file for parsing...
  //

  (void)GetExistingFolder(fcc_header, getter_AddRefs(folder));
  if ((mode == nsMsgDeliverNow || mode == nsMsgSendUnsent) && folder)
    turi = PL_strdup(fcc_header);
  else
    turi = GetFolderURIFromUserPrefs(mode, mUserIdentity);
  status = MessageFolderIsLocal(mUserIdentity, mode, turi, &folderIsLocal);
  if (NS_FAILED(status))
    goto FAIL;

  // Tell the user we are copying the message... 
  mComposeBundle->GetStringByID(NS_MSG_START_COPY_MESSAGE, getter_Copies(msg));
  if (msg)
  {
    nsCOMPtr<nsIRDFService> rdfService = do_GetService(kRDFServiceCID);
    if (rdfService)
    {
      nsCOMPtr<nsIRDFResource> res;
      rdfService->GetResource(nsDependentCString(turi), getter_AddRefs(res));
      nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(res);
      if (folder)
        folder->GetName(getter_Copies(folderName));
    }
    if (!folderName.IsEmpty())
      printfString = nsTextFormatter::smprintf(msg, folderName.get());
    else
      printfString = nsTextFormatter::smprintf(msg, "?");
    if (printfString)
    {
      SetStatusMessage(printfString); 
      PR_Free(printfString);  
    }
  }

  if ( (envelopeLine) && (folderIsLocal) )
  {
    PRInt32   len = PL_strlen(envelopeLine);
    
    n = tempOutfile.write(envelopeLine, len);
    if (n != len)
    {
      status = NS_ERROR_FAILURE;
      goto FAIL;
    }
  }

  //
  // Write out an X-Mozilla-Status header.
  //
  // This is required for the queue file, so that we can overwrite it once
  // the messages have been delivered, and so that the MSG_FLAG_QUEUED bit
  // is set.
  //
  // For FCC files, we don't necessarily need one, but we might as well put
  // one in so that it's marked as read already.
  //
  //
  // Need to add these lines for POP3 ONLY! IMAP servers will handle
  // this status information for summary file regeneration for us. 
  if ( (mode == nsMsgQueueForLater  || mode == nsMsgSaveAsDraft ||
        mode == nsMsgSaveAsTemplate || mode == nsMsgDeliverNow ||
        mode == nsMsgSendUnsent) && folderIsLocal)
  {
    char       *buf = 0;
    PRUint16   flags = 0;
    
    // for save as draft and send later, we want to leave the message as unread.
    // See Bug #198087
    if (mode == nsMsgQueueForLater)
      flags |= MSG_FLAG_QUEUED;
    else if (mode != nsMsgSaveAsDraft)
      flags |= MSG_FLAG_READ;
    buf = PR_smprintf(X_MOZILLA_STATUS_FORMAT CRLF, flags);
    if (buf)
    {
      PRInt32   len = PL_strlen(buf);
      n = tempOutfile.write(buf, len);
      PR_Free(buf);
      if (n != len)
      {
        status = NS_ERROR_FAILURE;
        goto FAIL;
      }
    }
    
    PRUint32 flags2 = 0;
    if (mode == nsMsgSaveAsTemplate)
      flags2 |= MSG_FLAG_TEMPLATE;
    if (mode == nsMsgDeliverNow || mode == nsMsgSendUnsent) 
    {
      flags2 &= ~MSG_FLAG_MDN_REPORT_NEEDED;
      flags2 |= MSG_FLAG_MDN_REPORT_SENT;
    }
    buf = PR_smprintf(X_MOZILLA_STATUS2_FORMAT CRLF, flags2);
    if (buf)
    {
      PRInt32   len = PL_strlen(buf);
      n = tempOutfile.write(buf, len);
      PR_Free(buf);
      if (n != len)
      {
        status = NS_ERROR_FAILURE;
        goto FAIL;
      }
    }
  }

  // Write out the FCC and BCC headers.
  // When writing to the Queue file, we *must* write the FCC and BCC
  // headers, or else that information would be lost.  Because, when actually
  // delivering the message (with "deliver now") we do FCC/BCC right away;
  // but when queueing for later delivery, we do FCC/BCC at delivery-time.
  //
  // The question remains of whether FCC and BCC should be written into normal
  // BCC folders (like the Sent Mail folder.)
  //
  // For FCC, there seems no point to do that; it's not information that one
  // would want to refer back to.
  //
  // For BCC, the question isn't as clear.  On the one hand, if I send someone
  // a BCC'ed copy of the message, and save a copy of it for myself (with FCC)
  // I might want to be able to look at that message later and see the list of
  // people to whom I had BCC'ed it.
  //
  // On the other hand, the contents of the BCC header is sensitive
  // information, and should perhaps not be stored at all.
  //
  // Thus the consultation of the #define SAVE_BCC_IN_FCC_FILE.
  //
  // (Note that, if there is a BCC header present in a message in some random
  // folder, and that message is forwarded to someone, then the attachment
  // code will strip out the BCC header before forwarding it.)
  //
  if ((mode == nsMsgQueueForLater || mode == nsMsgSaveAsDraft ||
       mode == nsMsgSaveAsTemplate) && fcc_header && *fcc_header)
  {
    PRInt32 L = PL_strlen(fcc_header) + 20;
    char  *buf = (char *) PR_Malloc (L);
    if (!buf)
    {
      status = NS_ERROR_OUT_OF_MEMORY;
      goto FAIL;
    }

    PR_snprintf(buf, L-1, "FCC: %s" CRLF, fcc_header);

    PRInt32   len = PL_strlen(buf);
    n = tempOutfile.write(buf, len);
    if (n != len)
    {
      status = NS_ERROR_FAILURE;
      goto FAIL;
    }
  }

  //
  // Ok, now I want to get the identity key and write it out if this is for a
  // nsMsgQueueForLater operation!
  //
  if (  (  ( nsMsgQueueForLater == mode )
        || ( nsMsgSaveAsDraft == mode )
        || ( nsMsgSaveAsTemplate == mode )
        )
     && ( mUserIdentity )
     )
  {
    char *buf = nsnull, *key = nsnull;

    if (NS_SUCCEEDED(mUserIdentity->GetKey(&key)) && (key))
    {
      buf = PR_smprintf(HEADER_X_MOZILLA_IDENTITY_KEY ": %s" CRLF, key);
      if (buf)
      {
        PRInt32 len = strlen(buf);
        n = tempOutfile.write(buf, len);
        PR_Free(buf);
        if (n != len)
        {
          status = NS_ERROR_FAILURE;
          goto FAIL;
        }
      }
    }

    if (!mAccountKey.IsEmpty())
    {
      buf = PR_smprintf(HEADER_X_MOZILLA_ACCOUNT_KEY ": %s" CRLF, mAccountKey.get());
      if (buf)
      {
        PRInt32 len = strlen(buf);
        n = tempOutfile.write(buf, len);
        PR_Free(buf);
        if (n != len)
        {
          status = NS_ERROR_FAILURE;
          goto FAIL;
        }
      }
    }
  }

  if (bcc_header && *bcc_header
#ifndef SAVE_BCC_IN_FCC_FILE
      && (mode == MSG_QueueForLater || mode == MSG_SaveAsDraft ||
          mode == MSG_SaveAsTemplate)
#endif
    )
  {
    char *convBcc;
    convBcc = nsMsgI18NEncodeMimePartIIStr(bcc_header, PR_TRUE,
                    mCompFields->GetCharacterSet(), sizeof("BCC: "),
                    nsMsgMIMEGetConformToStandard());

    PRInt32 L = strlen(convBcc ? convBcc : bcc_header) + 20;
    char *buf = (char *) PR_Malloc (L);
    if (!buf)
    {
      status = NS_ERROR_OUT_OF_MEMORY;
      goto FAIL;
    }

    PR_snprintf(buf, L-1, "BCC: %s" CRLF, convBcc ? convBcc : bcc_header);
    PRInt32   len = strlen(buf);
    n = tempOutfile.write(buf, len);
    PR_Free(buf);
    PR_Free(convBcc);
    if (n != len)
    {
      status = NS_ERROR_FAILURE;
      goto FAIL;
    }
  }

  //
  // Write out the X-Mozilla-News-Host header.
  // This is done only when writing to the queue file, not the FCC file.
  // We need this to complement the "Newsgroups" header for the case of
  // queueing a message for a non-default news host.
  //
  // Convert a URL like "snews://host:123/" to the form "host:123/secure"
  // or "news://user@host:222" to simply "host:222".
  //
  if ((mode == nsMsgQueueForLater || mode == nsMsgSaveAsDraft ||
       mode == nsMsgSaveAsTemplate) && news_url && *news_url)
  {
    PRBool secure_p = (news_url[0] == 's' || news_url[0] == 'S');
    char *orig_hap = nsMsgParseURLHost (news_url);
    char *host_and_port = orig_hap;
    if (host_and_port)
    {
      // There may be authinfo at the front of the host - it could be of
      // the form "user:password@host:port", so take off everything before
      // the first at-sign.  We don't want to store authinfo in the queue
      // folder, I guess, but would want it to be re-prompted-for at
      // delivery-time.
      //
      char *at = PL_strchr (host_and_port, '@');
      if (at)
        host_and_port = at + 1;
    }

    if ((host_and_port && *host_and_port) || !secure_p)
    {
      char *line = PR_smprintf(X_MOZILLA_NEWSHOST ": %s%s" CRLF,
                   host_and_port ? host_and_port : "",
                   secure_p ? "/secure" : "");
      PR_FREEIF(orig_hap);
      if (!line)
      {
        status = NS_ERROR_OUT_OF_MEMORY;
        goto FAIL;
      }

      PRInt32   len = PL_strlen(line);
      n = tempOutfile.write(line, len);
      PR_Free(line);
      if (n != len)
      {
        status = NS_ERROR_FAILURE;
        goto FAIL;
      }
    }

    PR_Free(orig_hap);
  }

  //
  // Read from the message file, and write to the FCC or Queue file.
  // There are two tricky parts: the first is that the message file
  // uses CRLF, and the FCC file should use LINEBREAK.  The second
  // is that the message file may have lines beginning with "From "
  // but the FCC file must have those lines mangled.
  //
  // It's unfortunate that we end up writing the FCC file a line
  // at a time, but it's the easiest way...
  //
  while (! inputFile.eof())
  {
    // check *ibuffer in case that ibuffer isn't big enough
    if (!inputFile.readline(ibuffer, ibuffer_size) && *ibuffer == 0)
    {
      status = NS_ERROR_FAILURE;
      goto FAIL;
    }

    n =  tempOutfile.write(ibuffer, PL_strlen(ibuffer));
    n += tempOutfile.write(CRLF, 2);
    if (n != (PRInt32) (PL_strlen(ibuffer) + 2)) // write failed 
    {
      status = NS_MSG_ERROR_WRITING_FILE;
      goto FAIL;
    }
  }

  //
  // Terminate with a final newline. 
  //
  n = tempOutfile.write(CRLF, 2);
  if (n != 2) // write failed 
  {
    status = NS_MSG_ERROR_WRITING_FILE;
    goto FAIL;
  }

FAIL:
  PR_Free(ibuffer);
  if (obuffer != ibuffer)
    PR_Free(obuffer);


  if (tempOutfile.is_open()) 
  {
    if (NS_FAILED(tempOutfile.flush()) || tempOutfile.failed())
      status = NS_MSG_ERROR_WRITING_FILE;

    tempOutfile.close();
    if (mCopyFileSpec)
      mCopyFileSpec->CloseStream();
  }

  if (inputFile.is_open()) 
    inputFile.close();

  // 
  // When we get here, we have to see if we have been successful so far.
  // If we have, then we should start up the async copy service operation.
  // If we weren't successful, then we should just return the error and 
  // bail out.
  //
  if (NS_SUCCEEDED(status))
  {
    //
    // If we are here, time to start the async copy service operation!
    //
    status = StartMessageCopyOperation(mCopyFileSpec, mode, turi);
  }
  PR_Free(turi);
  return status;
}

//
// This is pretty much a wrapper to the functionality that lives in the 
// nsMsgCopy class
//
nsresult 
nsMsgComposeAndSend::StartMessageCopyOperation(nsIFileSpec        *aFileSpec, 
                                               nsMsgDeliverMode   mode,
                                               char             *dest_uri)
{
  mCopyObj = new nsMsgCopy();
  if (!mCopyObj)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mCopyObj);
  //
  // Actually, we need to pick up the proper folder from the prefs and not
  // default to the default "Flagged" folder choices
  //
  nsresult    rv;
  if (dest_uri && *dest_uri)
    m_folderName = dest_uri;
  else
    m_folderName = GetFolderURIFromUserPrefs(mode, mUserIdentity);

  if (mListener)
    mListener->OnGetDraftFolderURI(m_folderName.get());

  rv = mCopyObj->StartCopyOperation(mUserIdentity, aFileSpec, mode, 
                                    this, m_folderName.get(), mMsgToReplace);
  return rv;
}

//I'm getting this each time without holding onto the feedback so that 3 pane windows can be closed
//without any chance of crashing due to holding onto a deleted feedback.
nsresult
nsMsgComposeAndSend::SetStatusMessage(const PRUnichar *aMsgString)
{
  if (mSendProgress)
    mSendProgress->OnStatusChange(nsnull, nsnull, 0, aMsgString);
  return NS_OK;
}

// For GUI notification...
nsresult
nsMsgComposeAndSend::SetGUINotificationState(PRBool aEnableFlag)
{
  mGUINotificationEnabled = aEnableFlag;
  return NS_OK;
}

/* readonly attribute nsIMsgSendReport sendReport; */
NS_IMETHODIMP
nsMsgComposeAndSend::GetSendReport(nsIMsgSendReport * *aSendReport)
{
  NS_ENSURE_ARG_POINTER(aSendReport);
  NS_IF_ADDREF(*aSendReport = mSendReport);
  return NS_OK;
}

nsresult nsMsgComposeAndSend::Abort()
{
  PRUint32 i;
  nsresult rv;
  
  if (mAbortInProcess)
    return NS_OK;
    
  mAbortInProcess = PR_TRUE;
      
  if (m_plaintext)
    rv = m_plaintext->Abort();
  
  if (m_attachments)
  {
    for (i = 0; i < m_attachment_count; i ++)
    {
      nsMsgAttachmentHandler *ma = &m_attachments[i];
      if (ma)
        rv = ma->Abort();
    }
  }

  /* stop the current running url */
  if (mRunningRequest)
  {
    mRunningRequest->Cancel(NS_ERROR_ABORT);
    mRunningRequest = nsnull;
  }
  
  mAbortInProcess = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetProcessAttachmentsSynchronously(PRBool *_retval)
{
  NS_ENSURE_ARG(_retval);
  *_retval = m_be_synchronous_p;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetAttachmentHandlers(nsMsgAttachmentHandler * *_retval)
{
  NS_ENSURE_ARG(_retval);
  *_retval = m_attachments;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetAttachmentCount(PRUint32 *aAttachmentCount)
{
  NS_ENSURE_ARG(aAttachmentCount);
  *aAttachmentCount = m_attachment_count;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetPendingAttachmentCount(PRUint32 *aPendingAttachmentCount)
{
  NS_ENSURE_ARG(aPendingAttachmentCount);
  *aPendingAttachmentCount = m_attachment_pending_count;
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeAndSend::SetPendingAttachmentCount(PRUint32 aPendingAttachmentCount)
{
  m_attachment_pending_count = aPendingAttachmentCount;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetDeliveryMode(nsMsgDeliverMode *aDeliveryMode)
{
  NS_ENSURE_ARG(aDeliveryMode);
  *aDeliveryMode = m_deliver_mode;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetProgress(nsIMsgProgress **_retval)
{
  NS_ENSURE_ARG(_retval);
  NS_IF_ADDREF(*_retval = mSendProgress);
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetOutputStream(nsOutputFileStream * *_retval)
{
  NS_ENSURE_ARG(_retval);
  *_retval = mOutputFile;
  return NS_OK;
}


/* [noscript] attribute nsIURI runningURL; */
NS_IMETHODIMP nsMsgComposeAndSend::GetRunningRequest(nsIRequest **request)
{
  NS_ENSURE_ARG(request);
  NS_IF_ADDREF(*request = mRunningRequest);
  return NS_OK;
}
NS_IMETHODIMP nsMsgComposeAndSend::SetRunningRequest(nsIRequest *request)
{
  mRunningRequest = request;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetStatus(nsresult *aStatus)
{
  NS_ENSURE_ARG(aStatus);
  *aStatus = m_status;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::SetStatus(nsresult aStatus)
{
  m_status = aStatus;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::GetCryptoclosure(nsIMsgComposeSecure ** aCryptoclosure)
{
  NS_ENSURE_ARG(aCryptoclosure);
  NS_IF_ADDREF(*aCryptoclosure = m_crypto_closure);
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeAndSend::SetCryptoclosure(nsIMsgComposeSecure * aCryptoclosure)
{
  m_crypto_closure = aCryptoclosure;
  return NS_OK;
}


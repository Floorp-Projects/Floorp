/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "msgCore.h"
#include "nsCRT.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsMsgSendPart.h"
#include "nsMsgSendFact.h"
#include "nsMsgBaseCID.h"
#include "nsMsgNewsCID.h"
#include "nsIMsgHeaderParser.h"
#include "nsISmtpService.h"  // for actually sending the message...
#include "nsINntpService.h"  // for actually posting the message...
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsMsgSend.h"
#include "nsEscape.h"
#include "nsIPref.h"
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
#include "nsMsgCopy.h"
#include "nsXPIDLString.h"
#include "nsMsgPrompts.h"
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
#include "nsINetSupportDialogService.h"
#include "nsIAppShellService.h"
#include "nsMailHeaders.h"
#include "nsIDocShell.h"
#include "nsMimeTypes.h"
#include "nsISmtpUrl.h"

// This will go away once select is passed a prompter interface
#include "nsAppShellCIDs.h" // TODO remove later
#include "nsIAppShellService.h" // TODO remove later
#include "nsIXULWindow.h" // TODO remove later

// use these macros to define a class IID for our component. Our object currently 
// supports two interfaces (nsISupports and nsIMsgCompose) so we want to define constants 
// for these two interfaces 
//
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCAddressCollecter, NS_ABADDRESSCOLLECTER_CID);
static NS_DEFINE_CID(kTXTToHTMLConvCID, MOZITXTTOHTMLCONV_CID);
static NS_DEFINE_CID(kCNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID); 

#define PREF_MAIL_SEND_STRUCT "mail.send_struct"
#define PREF_MAIL_STRICTLY_MIME "mail.strictly_mime"
#define PREF_MAIL_MESSAGE_WARNING_SIZE "mailnews.message_warning_size"
#define PREF_MAIL_COLLECT_EMAIL_ADDRESS_OUTGOING "mail.collect_email_address_outgoing"

enum  { kDefaultMode = (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE) };

#ifdef XP_MAC
#include "xp.h"                 // mac only 
#include "errors.h"
#include "m_cvstrm.h"

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

// 
// This function will be used by the factory to generate the 
// nsMsgComposeAndSend Object....
//
nsresult NS_NewMsgSend(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgComposeAndSend *pSend = new nsMsgComposeAndSend();
		if (pSend)
			return pSend->QueryInterface(NS_GET_IID(nsIMsgSend), aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was nsnull....*/
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgComposeAndSend, NS_GET_IID(nsIMsgSend));

nsMsgComposeAndSend::nsMsgComposeAndSend() : 
    m_messageKey(0xffffffff)
{
  mGUINotificationEnabled = PR_TRUE;
  mEditor = nsnull;
  mMultipartRelatedAttachmentCount = 0;
  mCompFields = nsnull;			/* Where to send the message once it's done */
  mListenerArray = nsnull;
  mListenerArrayCount = 0;
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

  NS_INIT_REFCNT();
}

nsMsgComposeAndSend::~nsMsgComposeAndSend()
{
	Clear();
}

void nsMsgComposeAndSend::GetDefaultPrompt(nsIPrompt ** aPrompt)
{

  nsCOMPtr<nsIMsgWindow>    msgWindow;
  nsCOMPtr <nsIMsgMailSession> mailSession (do_GetService(kMsgMailSessionCID));
  mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));

  if (msgWindow)
      msgWindow->GetPromptDialog(aPrompt);
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
			m_plaintext->mOutFile->close();

		if (m_plaintext->mFileSpec)
    {
		  m_plaintext->mFileSpec->Delete(PR_TRUE);
		  delete m_plaintext->mFileSpec;
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

			if (m_attachments[i].mURL)
      {
        NS_RELEASE(m_attachments[i].mURL);
        m_attachments[i].mURL = nsnull;
      }

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
			if ( (m_attachments[i].mOutFile) && (m_attachments[i].mOutFile->is_open()) )
          m_attachments[i].mOutFile->close();
			if (m_attachments[i].mFileSpec) 
      {
        // Only Delete the file if this variable is set!
        if (m_attachments[i].mDeleteFile)
  				m_attachments[i].mFileSpec->Delete(PR_FALSE);

				delete m_attachments[i].mFileSpec;
        m_attachments[i].mFileSpec = nsnull;
			}

#ifdef XP_MAC
		  //
      // remove the appledoubled intermediate file after we done all.
		  //
			if (m_attachments[i].mAppleFileSpec) 
      {
				m_attachments[i].mAppleFileSpec->Delete(PR_FALSE);
				delete m_attachments[i].mAppleFileSpec;
			}
#endif /* XP_MAC */
		}

		delete[] m_attachments;
		m_attachment_count = m_attachment_pending_count = 0;
		m_attachments = 0;
	}

  // Cleanup listener array...
  DeleteListeners();
}

static char *mime_mailto_stream_read_buffer = 0;
static char *mime_mailto_stream_write_buffer = 0;


char * mime_get_stream_write_buffer(void)
{
	if (!mime_mailto_stream_write_buffer)
		mime_mailto_stream_write_buffer = (char *) PR_Malloc(MIME_BUFFER_SIZE);
	return mime_mailto_stream_write_buffer;
}

// Don't I18N this line...this is per the spec!
#define   MIME_MULTIPART_BLURB     "This is a multi-part message in MIME format."

/* All of the desired attachments have been written to individual temp files,
   and we know what's in them.  Now we need to make a final temp file of the
   actual mail message, containing all of the other files after having been
   encoded as appropriate.
 */
int 
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
  nsMsgSendPart   *mpartcontainer = nsnull;
	nsMsgSendPart* toppart = nsnull;			// The very top most container of the message
											// that we are going to send.

	nsMsgSendPart* mainbody = nsnull;			// The leaf node that contains the text of the
											// message we're going to contain.

	nsMsgSendPart* maincontainer = nsnull;	// The direct child of toppart that will
											// contain the mainbody.  If mainbody is
											// the same as toppart, then this is
											// also the same.  But if mainbody is
											// to end up somewhere inside of a
											// multipart/alternative or a
											// multipart/related, then this is that
											// multipart object.

	nsMsgSendPart* plainpart = nsnull;		// If we converted HTML into plaintext,
											// the message or child containing the plaintext
											// goes here. (Need to use this to determine
											// what headers to append/set to the main 
											// message body.)

  nsCOMPtr<nsIPrompt> promptObject; // only used if we have to show an alert here....
  GetDefaultPrompt(getter_AddRefs(promptObject));

	char *hdrs = 0;
	PRBool maincontainerISrelatedpart = PR_FALSE;

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
			status = NS_MSG_UNABLE_TO_OPEN_TMP_FILE;
			goto FAIL;
		}

    if (mOriginalHTMLBody)
    {
      PRUint32    origLen = nsCRT::strlen(mOriginalHTMLBody);
      status = tempfile.write(mOriginalHTMLBody, origLen);
		  if (status < int(origLen)) 
      {
			  if (status >= 0)
				  status = NS_MSG_ERROR_WRITING_FILE;
			  goto FAIL;
		  }
    }

		if (tempfile.failed()) 
      goto FAIL;

    tempfile.close();

		m_plaintext = new nsMsgAttachmentHandler;
		if (!m_plaintext)
			goto FAILMEM;
		m_plaintext->m_mime_delivery_state = this;

    char *tempURL = nsMsgPlatformFileToURL (*mHTMLFileSpec);
    if (!tempURL || NS_FAILED(nsMsgNewURL(&(m_plaintext->mURL), tempURL)))
    {
      delete m_plaintext;
      m_plaintext = nsnull;
      goto FAILMEM;
    }
  
    NS_IF_ADDREF(m_plaintext->mURL);
    PR_FREEIF(tempURL);

		PR_FREEIF(m_plaintext->m_type);
		m_plaintext->m_type = PL_strdup(TEXT_HTML);
		PR_FREEIF(m_plaintext->m_charset);
		m_plaintext->m_charset = PL_strdup(mCompFields->GetCharacterSet());
		PR_FREEIF(m_plaintext->m_desired_type);
		m_plaintext->m_desired_type = PL_strdup(TEXT_PLAIN);
		m_attachment_pending_count ++;
		status = m_plaintext->SnarfAttachment(mCompFields);
		if (status < 0)
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

#ifdef UNREADY_CODE
	FE_Progress(GetContext(), XP_GetString(NS_MSG_ASSEMBLING_MSG));
#endif

	/* First, open the message file.
	*/
	mTempFileSpec = nsMsgCreateTempFileSpec("nsmail.eml");
	if (! mTempFileSpec)
		goto FAILMEM;

  mOutputFile = new nsOutputFileStream(*mTempFileSpec, kDefaultMode, 00600);
	if (! mOutputFile->is_open()) 
  {
		status = NS_MSG_UNABLE_TO_OPEN_TMP_FILE;
    if (mGUINotificationEnabled)
      nsMsgDisplayMessageByID(promptObject, NS_MSG_UNABLE_TO_OPEN_TMP_FILE);
		goto FAIL;
	}
  
	if (mCompFields->GetMessageId() == nsnull || *mCompFields->GetMessageId() == 0)
	{
		char * msgID = msg_generate_message_id(mUserIdentity);
		mCompFields->SetMessageId(msgID);
		PR_FREEIF(msgID);
	}

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

  // Check if the part contains 7 bit only. Re-label charset if necessary.
  PRBool body_is_us_ascii;
  if (nsMsgI18Nstateful_charset(mCompFields->GetCharacterSet())) {
    body_is_us_ascii = nsMsgI18N7bit_data_part(mCompFields->GetCharacterSet(), m_attachment1_body, m_attachment1_body_length); 
  }
  else {
    body_is_us_ascii = mime_7bit_data_p (m_attachment1_body, m_attachment1_body_length);
  }

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

		const char *relPartType = m_related_part->GetType();
		if (relPartType && !strcmp(relPartType, MULTIPART_RELATED))
			// outer shell is m/related,
			// mainbody wants to be the HTML lump within
			mainbody = m_related_part->GetChild(0);
		else
			// outer shell is text/html, 
			// so mainbody wants to be the same lump
			mainbody = maincontainer;

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
		m_plaintext->PickEncoding(mCompFields->GetCharacterSet());
		hdrs = mime_generate_attachment_headers(m_plaintext->m_type,
											  m_plaintext->m_encoding,
											  m_plaintext->m_description,
											  m_plaintext->m_x_mac_type,
											  m_plaintext->m_x_mac_creator,
											  nsnull, 0,
											  m_digest_p,
											  m_plaintext,
											  mCompFields->GetCharacterSet(),
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

      // Only create multipart related if its really necessary...
      if (mMultipartRelatedAttachmentCount > 0)
      {
        mpartcontainer = new nsMsgSendPart(this);
        if (!mpartcontainer)
          goto FAILMEM;

			  status = mpartcontainer->SetType(MULTIPART_RELATED);
			  if (status < 0)
				  goto FAIL;

			  status = mpartcontainer->AddChild(htmlpart);
			  if (status < 0)
				  goto FAIL;

        // Hang stuff off of the maincontainer...
        status = maincontainer->AddChild(plainpart);
			  if (status < 0)
				  goto FAIL;

        status = maincontainer->AddChild(mpartcontainer);
			  if (status < 0)
				  goto FAIL;
      }
      else    // Just a single HTML doc for the HTML mail
      {
        status = maincontainer->AddChild(plainpart);
			  if (status < 0)
				  goto FAIL;

        status = maincontainer->AddChild(htmlpart);
			  if (status < 0)
				  goto FAIL;
      }

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
				m_related_part = nsnull;
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

	if (  (multipart_p && (!mCompFields->GetUseMultipartAlternative()))  ||
        mCompFields->GetUseMultipartAlternative() && (m_attachment_count > mMultipartRelatedAttachmentCount) 
        )
  {
		toppart = new nsMsgSendPart(this);
		if (!toppart)
			goto FAILMEM;

    // This is JUST multpart related!
    if ( (mMultipartRelatedAttachmentCount > 0) &&
         (m_attachment_count == mMultipartRelatedAttachmentCount) )
  		status = toppart->SetType(MULTIPART_RELATED);
    else
	  	status = toppart->SetType(m_digest_p ? MULTIPART_DIGEST : MULTIPART_MIXED);

		if (status < 0)
			goto FAIL;

		status = toppart->AddChild(maincontainer);
		if (status < 0)
			goto FAIL;

    if (m_attachment_count > mMultipartRelatedAttachmentCount)
    {
      status = toppart->SetBuffer(MIME_MULTIPART_BLURB);
    }
    if (status < 0)
      goto FAIL;
	}
	else
  {
		toppart = maincontainer;
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
		hdrs = mime_generate_attachment_headers (m_attachment1_type,
											   m_attachment1_encoding,
											   0, 0, 0, 0, 0,
											   m_digest_p,
											   nsnull, /* no "ma"! */
											   mCompFields->GetCharacterSet(),
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
        {
          if (mCompFields->GetUseMultipartAlternative()) 
          { 
            PreProcessPart(&(m_attachments[i]), mpartcontainer);
          }
          else
          {
            if (m_attachment_count > mMultipartRelatedAttachmentCount)
              PreProcessPart(&(m_attachments[i]), m_related_part);
            else
              PreProcessPart(&(m_attachments[i]), toppart);
          }
        }
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
  
  if (mOutputFile) 
  {
		/* If we don't do this check...ZERO length files can be sent */
		if (mOutputFile->failed()) 
    {
			status = NS_MSG_ERROR_WRITING_FILE;
			goto FAIL;
		}

    mOutputFile->close();
		delete mOutputFile;
	}
	mOutputFile = nsnull;

#ifdef UNREADY_CODE
	FE_Progress(GetContext(), XP_GetString(NS_MSG_ASSEMB_DONE_MSG));
#endif

  if (m_dont_deliver_p && mListenerArrayCount > 0)
	{
    //
		// Need to ditch the file spec here so that we don't delete the
		// file, since in this case, the caller wants the file
    //
    NS_NewFileSpecWithSpec(*mTempFileSpec, &mReturnFileSpec);
    delete mTempFileSpec;
    mTempFileSpec = nsnull;
	  if (!mReturnFileSpec)
      NotifyListenersOnStopSending(nsnull, NS_ERROR_OUT_OF_MEMORY, nsnull, nsnull);
    else
      NotifyListenersOnStopSending(nsnull, NS_OK, nsnull, mReturnFileSpec);
	}
	else 
  {
		status = DeliverMessage();
		shouldDeleteDeliveryState = PR_FALSE;
	}

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
			Fail (nsnull, status, nsnull);
		}
	}

	return status;

FAILMEM:
	status = NS_ERROR_OUT_OF_MEMORY;
	goto FAIL;
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

	ma->PickEncoding (mCompFields->GetCharacterSet());

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
    *((char **)getter_Copies(turl)) = nsXPIDLCString::Copy(ma->m_uri);
  else
    ma->mURL->GetSpec(getter_Copies(turl));
  hdrs = mime_generate_attachment_headers (ma->m_type, ma->m_encoding,
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

#if defined(XP_MAC) && defined(DEBUG)
#pragma global_optimizer reset
#endif // XP_MAC && DEBUG


int
mime_write_message_body(nsMsgComposeAndSend *state, char *buf, PRInt32 size)
{
  if (PRInt32(state->mOutputFile->write(buf, size)) < size) 
  {
    return NS_MSG_ERROR_WRITING_FILE;
  } 
  else 
  {
    return 0;
  }
}

int
mime_encoder_output_fn(const char *buf, PRInt32 size, void *closure)
{
  nsMsgComposeAndSend *state = (nsMsgComposeAndSend *) closure;
  return mime_write_message_body (state, (char *) buf, size);
}

PRUint32
nsMsgComposeAndSend::GetMultipartRelatedCount(void)
{
  nsresult                  rv = NS_OK;
  nsISupportsArray          *aNodeList = nsnull;
  PRUint32                  count;

  if (!mEditor)
    return 0;

  rv = mEditor->GetEmbeddedObjects(&aNodeList);
  if ((NS_FAILED(rv) || (!aNodeList)))
    return 0;

  if (NS_SUCCEEDED(aNodeList->Count(&count)))
    return count;
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
  char          *attachment1_body = nsnull;
  char          *attachment1_type = TEXT_HTML;  // we better be "text/html" at this point
  PRInt32       attachment1_body_length = 0;

  // 
  // Query the editor, get the body of HTML!
  //
  nsString  format; format.AssignWithConversion(TEXT_HTML);
  PRUint32  flags = 0;
  PRUnichar *bodyText = NULL;
  nsresult rv;
  PRUnichar *origHTMLBody = nsnull;

  // Ok, get the body...the DOM should have been whacked with 
  // Content ID's already
  mEditor->GetContentsAs(format.GetUnicode(), flags, &bodyText);

  //
  // If we really didn't get a body, just return NS_OK
  //
  if ((!bodyText) || (!*bodyText))
    return NS_OK;

  // If we are forcing this to be plain text, we should not be
  // doing this conversion.
  PRBool doConversion = PR_TRUE;

  if ( (mCompFields) && mCompFields->GetForcePlainText() )
    doConversion = PR_FALSE;

  if (doConversion)
  {
    nsCOMPtr<mozITXTToHTMLConv> conv;
    rv = nsComponentManager::CreateInstance(kTXTToHTMLConvCID,
      NULL, NS_GET_IID(mozITXTToHTMLConv),
      (void **) getter_AddRefs(conv));
    if (NS_SUCCEEDED(rv)) 
    {
      PRUint32 whattodo = mozITXTToHTMLConv::kURLs;
      PRBool enable_structs = PR_FALSE;
      NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
      if (NS_SUCCEEDED(rv) && prefs)
      {
        rv = prefs->GetBoolPref(PREF_MAIL_SEND_STRUCT,&enable_structs);
        if (NS_FAILED(rv) || enable_structs)
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
	
  // Convert body to mail charset
	char      *outCString;
  const char  *aCharset = mCompFields->GetCharacterSet();

  if (aCharset && *aCharset)
  {
    // Convert to entities.
    // If later Editor generates entities then we can remove this.
    rv = nsMsgI18NSaveAsCharset(mCompFields->GetForcePlainText() ? TEXT_PLAIN : attachment1_type, 
                                aCharset, bodyText, &outCString);

    // body contains multilingual data, confirm send to the user
    // do this only for text/plain
    if ((NS_ERROR_UENC_NOMAPPING == rv) && mCompFields->GetForcePlainText()) {
      // if nbsp then replace it by sp and try again
      PRUnichar *bodyTextPtr = bodyText;
      while (*bodyTextPtr) {
        if (0x00A0 == *bodyTextPtr)
          *bodyTextPtr = 0x0020;
        bodyTextPtr++;
      }
      PR_FREEIF(outCString);
      rv = nsMsgI18NSaveAsCharset(TEXT_PLAIN, aCharset, bodyText, &outCString);

      if (NS_ERROR_UENC_NOMAPPING == rv) {
        PRBool proceedTheSend;
        nsCOMPtr<nsIPrompt> prompt;
        GetDefaultPrompt(getter_AddRefs(prompt));
        rv = nsMsgAskBooleanQuestionByID(prompt, NS_MSG_MULTILINGUAL_SEND, &proceedTheSend);
        if (!proceedTheSend) {
          PR_FREEIF(attachment1_body);
          PR_FREEIF(outCString);
          Recycle(bodyText);
          return NS_ERROR_BUT_DONT_SHOW_ALERT;
        }
      }
    }

    if (NS_SUCCEEDED(rv)) 
    {
      PR_FREEIF(attachment1_body);
      attachment1_body = outCString;
      Recycle(bodyText);
    }

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
  }
  else
  {
    attachment1_body = (char *)bodyText;
  }

  // If our holder for the orignal body text is STILL null, then just 
  // just copy what we have as the original body text.
  //
  if (!origHTMLBody)
    mOriginalHTMLBody = nsCRT::strdup(attachment1_body);
  else
    mOriginalHTMLBody = (char *)origHTMLBody;

  attachment1_body_length = PL_strlen(attachment1_body);
  return SnarfAndCopyBody(attachment1_body, attachment1_body_length, attachment1_type);
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
  nsISupportsArray          *aNodeList = nsnull;
  PRUint32                  i; 
  PRInt32                   j = -1;
  domSaveStruct             *domSaveArray = nsnull;
  
  // Sanity check to see if we should be here or not...if not, just return
  if (!mEditor)
    return NS_OK;
  
  // First, check to see if the multipartCount still matches
  // what we got before. It always should, BUT If it doesn't, 
  // we will have memory problems and we should just return 
  // with an error.
  if (multipartCount != mMultipartRelatedAttachmentCount) {
    return NS_ERROR_MIME_MPART_ATTACHMENT_ERROR;
  }
  
  rv = mEditor->GetEmbeddedObjects(&aNodeList);
  if ((NS_FAILED(rv) || (!aNodeList))) {
    return NS_ERROR_MIME_MPART_ATTACHMENT_ERROR;
  }
  
  nsMsgAttachmentData   attachment;
  PRInt32               locCount = -1;

  if (multipartCount > 0)
  {
    domSaveArray = (domSaveStruct *)PR_MALLOC(sizeof(domSaveStruct) * multipartCount);
    if (!domSaveArray)
      return NS_ERROR_MIME_MPART_ATTACHMENT_ERROR;
    nsCRT::memset(domSaveArray, 0, sizeof(domSaveStruct) * multipartCount);
  }

  for (i = mPreloadedAttachmentCount; i < (mPreloadedAttachmentCount + multipartCount); i++)
  {
    // Reset this structure to null!
    nsCRT::memset(&attachment, 0, sizeof(nsMsgAttachmentData));
    
    // MUST set this to get placed in the correct part of the message
    m_attachments[i].mMHTMLPart = PR_TRUE;
    
    locCount++;
    m_attachments[i].mDeleteFile = PR_TRUE;
    m_attachments[i].m_done = PR_FALSE;
    m_attachments[i].m_mime_delivery_state = this;
    
    // Ok, now we need to get the element in the array and do the magic
    // to process this element.
    //
    nsCOMPtr<nsIDOMNode>    node;
    nsISupports             *isupp = aNodeList->ElementAt(locCount);
    
    if (!isupp) {
      return NS_ERROR_MIME_MPART_ATTACHMENT_ERROR;
    }
    
    node = do_QueryInterface(isupp);
    NS_IF_RELEASE(isupp);             // make sure we cleanup
    if (!node) {
      return NS_ERROR_MIME_MPART_ATTACHMENT_ERROR;
    }

    j++;
    domSaveArray[j].node = node;
    
    // Now, we know the types of objects this node can be, so we will do
    // our query interface here and see what we come up with 
    nsCOMPtr<nsIDOMHTMLImageElement>    image = (do_QueryInterface(node));
    nsCOMPtr<nsIDOMHTMLLinkElement>     link = (do_QueryInterface(node));
    nsCOMPtr<nsIDOMHTMLAnchorElement>   anchor = (do_QueryInterface(node));
    
    // First, try to see if this is an embedded image 
    if (image)
    {
      nsString    tUrl;
      nsString    tName;
      nsString    tDesc;
      
      // Create the URI
      if (NS_FAILED(image->GetSrc(tUrl)))
        return NS_ERROR_FAILURE;
      nsCAutoString turlC;
      turlC.AssignWithConversion(tUrl);
      if (NS_FAILED(nsMsgNewURL(&attachment.url, turlC)))
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
          {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          
          nsXPIDLCString    spec;
          nsCOMPtr<nsIURI> uri = dont_AddRef(doc->GetDocumentURL());
          
          if (!uri)
            return NS_ERROR_OUT_OF_MEMORY;
          
          uri->GetSpec(getter_Copies(spec));
          
          // Ok, now get the path to the root doc and tack on the name we
          // got from the GetSrc() call....
          nsString   workURL; workURL.AssignWithConversion(spec);
          
          PRInt32 loc = workURL.RFind("/");
          if (loc >= 0)
            workURL.SetLength(loc+1);
          workURL.Append(tUrl);
          nsCAutoString workurlC;
          workurlC.AssignWithConversion(workURL);
          if (NS_FAILED(nsMsgNewURL(&attachment.url, workurlC)))
          {
            // rhp - just try to continue and send it without this image.
            continue;
          }
        }
      }
      
      NS_IF_ADDREF(attachment.url);
      
      if (NS_FAILED(image->GetName(tName)))
        return NS_ERROR_OUT_OF_MEMORY;
      attachment.real_name = tName.ToNewCString();
      
      if (NS_FAILED(image->GetLongDesc(tDesc)))
        return NS_ERROR_OUT_OF_MEMORY;
      attachment.description = tDesc.ToNewCString();
      
    }
    else if (link)        // Is this a link?
    {
      nsString    tUrl;
      
      // Create the URI
      if (NS_FAILED(link->GetHref(tUrl)))
        return NS_ERROR_FAILURE;
      nsCAutoString turlC;
      turlC.AssignWithConversion(tUrl);
      if (NS_FAILED(nsMsgNewURL(&attachment.url, turlC)))
        return NS_ERROR_OUT_OF_MEMORY;
      
      NS_IF_ADDREF(attachment.url);
    }
    else if (anchor)
    {
      nsString    tUrl;
      nsString    tName;
      
      // Create the URI
      if (NS_FAILED(anchor->GetHref(tUrl)))
        return NS_ERROR_FAILURE;
      nsCAutoString turlC;
      turlC.AssignWithConversion(tUrl);
      if (NS_FAILED(nsMsgNewURL(&attachment.url, turlC)))
        return NS_ERROR_OUT_OF_MEMORY;
      
      NS_IF_ADDREF(attachment.url);
      
      if (NS_FAILED(anchor->GetName(tName)))
        return NS_ERROR_OUT_OF_MEMORY;
      attachment.real_name = tName.ToNewCString();
    }
    else
    {
      // If we get here, we got something we didn't expect!
      // Just try to continue and send it without this thing.
      continue;
    }
    
    // 
    // Now we have to get all of the interesting information from
    // the nsIDOMNode we have in hand...
    
    if (m_attachments[i].mURL)
      NS_RELEASE(m_attachments[i].mURL);
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
      msg_pick_real_name(&m_attachments[i], mCompFields->GetCharacterSet());
    
    //
    // Next, generate a content id for use with this part
    //    
    PRUnichar   *myEmail = nsnull;
    
    if (NS_FAILED(mCompFields->GetFrom(&myEmail)))
      m_attachments[i].m_content_id = mime_gen_content_id(locCount+1, nsnull);  
    else
    {
      nsCAutoString tEmail; tEmail.AssignWithConversion(myEmail);
      m_attachments[i].m_content_id = mime_gen_content_id(locCount+1, tEmail);  
      
    }
    
    if (!m_attachments[i].m_content_id)
      return NS_ERROR_OUT_OF_MEMORY;
    
    //
    // Start counting the attachments which are going to come from mail folders
    // and from NNTP servers.
    //
    nsXPIDLCString    turl;
    if (m_attachments[i].mURL)
    {
      m_attachments[i].mURL->GetSpec(getter_Copies(turl));
      if (PL_strncasecmp(turl, "mailbox:",8) || PL_strncasecmp(turl, "IMAP:",5))
        (*aMailboxCount)++;
      else if (PL_strncasecmp(turl, "news:",5) || PL_strncasecmp(turl, "snews:",6))
        (*aNewsCount)++;
    }
    
    // Ok, cleanup the temp structure...
    PR_FREEIF(attachment.real_name);
    PR_FREEIF(attachment.description);
    PR_FREEIF(attachment.real_type);
    PR_FREEIF(attachment.real_encoding);
    PR_FREEIF(attachment.desired_type);
    PR_FREEIF(attachment.x_mac_type);
    PR_FREEIF(attachment.x_mac_creator); 
    
    //
    // Ok, while we are here, we should whack the DOM with the generated 
    // Content-ID for this object. This will be necessary for generating
    // the HTML we need.
    //
    nsString      domURL;
    if (m_attachments[i].m_content_id)  
    {
      nsString   newSpec; newSpec.AssignWithConversion("cid:");

        // STRING USE WARNING: this is probably not needed.  Strings are created empty by default.
      domURL.SetLength(0);

      newSpec.AppendWithConversion(m_attachments[i].m_content_id);
      if (anchor)
      {
        anchor->GetHref(domURL);
        anchor->SetHref(newSpec);
      }
      else if (link)
      {
        anchor->GetHref(domURL);
        anchor->SetHref(newSpec);
      }
      else if (image)
      {
        image->GetSrc(domURL);
        image->SetSrc(newSpec);
      }

      if (!domURL.IsEmpty())
        domSaveArray[j].url = domURL.ToNewCString();
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
    nsCOMPtr<nsIDOMHTMLImageElement>    image = (do_QueryInterface(domSaveArray[i].node));
    nsCOMPtr<nsIDOMHTMLLinkElement>     link = (do_QueryInterface(domSaveArray[i].node));
    nsCOMPtr<nsIDOMHTMLAnchorElement>   anchor = (do_QueryInterface(domSaveArray[i].node));

      // STRING USE WARNING: hoisting the following conversion might save code-space, since it happens along every path

    if (anchor)
      anchor->SetHref(NS_ConvertASCIItoUCS2(domSaveArray[i].url));
    else if (link)
      anchor->SetHref(NS_ConvertASCIItoUCS2(domSaveArray[i].url));
    else if (image)
      image->SetSrc(NS_ConvertASCIItoUCS2(domSaveArray[i].url));

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

  if (m_attachment_count == mMultipartRelatedAttachmentCount)
  {
    // Set the body contents...
    m_related_part->SetBuffer(m_attachment1_body);
    m_related_part->SetType(m_attachment1_type);
  }
  else    // We have a multipart related email with attachments!
  {
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
  }

  return rv;
}

nsresult
nsMsgComposeAndSend::CountCompFieldAttachments()
{
  char  *attachmentList = (char *)mCompFields->GetAttachments();
  mCompFieldLocalAttachments = 0;
  mCompFieldRemoteAttachments = 0;

  if ((!attachmentList) || (!*attachmentList))
    return NS_OK;

  // Make our local copy...
  attachmentList = PL_strdup(mCompFields->GetAttachments());

  // parse the attachment list 
#ifdef NS_DEBUG
  printf("Comp fields attachment list = %s\n", (const char*)attachmentList);
#endif

  char      *token = nsnull;
  char      *rest = NS_CONST_CAST(char*, (const char*)attachmentList);
  nsCAutoString  str;
  
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) 
  {
    str = token;
    str.StripWhitespace();
    
    if (!str.IsEmpty()) 
    {
      // Check to see if this is a file URL, if so, don't retrieve
      // like a remote URL...
      //
      if (str.CompareWithConversion("file://", PR_TRUE, 7) == 0)
      {
        mCompFieldLocalAttachments++;
#ifdef NS_DEBUG
        printf("Counting LOCAL attachment %d: %s\n", 
                mCompFieldLocalAttachments, str.GetBuffer());
#endif
      }
      else    // This is a remote URL...
      {
        mCompFieldRemoteAttachments++;
#ifdef NS_DEBUG
        printf("Counting REMOTE attachment %d: %s\n", 
                mCompFieldRemoteAttachments, str.GetBuffer());
#endif
      }

      str = "";
    }

    token = nsCRT::strtok(rest, ",", &rest);
  }

  PR_FREEIF(attachmentList);
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

  char  *attachmentList = (char *)mCompFields->GetAttachments();
  if ((!attachmentList) || (!*attachmentList))
    return NS_ERROR_FAILURE;

  // Make our local copy...
  attachmentList = PL_strdup(mCompFields->GetAttachments());

  // parse the attachment list for local attachments
  PRUint32  newLoc = 0;
  char      *token = nsnull;
  char      *rest = NS_CONST_CAST(char*, (const char*)attachmentList);
  nsCAutoString  str;
  
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) 
  {
    str = token;
    str.StripWhitespace();
    
    if (!str.IsEmpty()) 
    {
      // Just look for local file:// attachments and do the right thing.
      if (str.CompareWithConversion("file://", PR_TRUE, 7) == 0)
      {
#ifdef NS_DEBUG
        printf("Adding LOCAL attachment %d: %s\n", newLoc, str.GetBuffer());
#endif
#ifdef XP_WIN
        str.ReplaceChar('|', ':');
#endif
        //
        // Now we have to setup the m_attachments entry for the file://
        // URL that is passed in...
        //
        m_attachments[newLoc].mDeleteFile = PR_FALSE;
			  m_attachments[newLoc].m_mime_delivery_state = this;

			  // These attachments are already "snarfed"...
			  m_attachments[newLoc].m_done = PR_TRUE;

        if (m_attachments[newLoc].mURL)
          NS_RELEASE(m_attachments[newLoc].mURL);

        nsMsgNewURL(&(m_attachments[newLoc].mURL), str);

        if (m_attachments[newLoc].mFileSpec)
          delete (m_attachments[newLoc].mFileSpec);
        m_attachments[newLoc].mFileSpec = new nsFileSpec( nsFileURL((const char *) str) );

        if (m_attachments[newLoc].mURL)
          msg_pick_real_name(&m_attachments[newLoc], mCompFields->GetCharacterSet());

        // Now, most importantly, we need to figure out what the content type is for
        // this attachment...If we can't, then just make it application/octet-stream
        nsresult  rv = NS_OK;
        nsCOMPtr<nsIMIMEService> mimeFinder (do_GetService(NS_MIMESERVICE_PROGID, &rv));
        if (NS_SUCCEEDED(rv) && mimeFinder) 
        {
          char *fileExt = nsMsgGetExtensionFromFileURL(NS_ConvertASCIItoUCS2(str));
          if (fileExt)
            mimeFinder->GetTypeFromExtension(fileExt, &(m_attachments[newLoc].m_type));

          PR_FREEIF(fileExt);
        }

        // If we still don't have a content type, we should really try sniff one out!
        if ((!m_attachments[newLoc].m_type) || (!*m_attachments[newLoc].m_type))
        {
          m_attachments[newLoc].AnalyzeSnarfedFile();
          if ( (m_attachments[newLoc].m_unprintable_count > 0) || (m_attachments[newLoc].m_highbit_count > 0) )
            m_attachments[newLoc].m_type = nsCRT::strdup(APPLICATION_OCTET_STREAM);
          else
            m_attachments[newLoc].m_type = nsCRT::strdup(TEXT_PLAIN);
        }

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

        ++newLoc;
      }

      str = "";
    }

    token = nsCRT::strtok(rest, ",", &rest);
  }

  PR_FREEIF(attachmentList);
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

  char  *attachmentList = (char *)mCompFields->GetAttachments();
  if ((!attachmentList) || (!*attachmentList))
    return NS_ERROR_FAILURE;

  // Make our local copy...
  attachmentList = PL_strdup(mCompFields->GetAttachments());

  // parse the attachment list for local attachments
  PRUint32  newLoc = aStartLocation;
  char      *token = nsnull;
  char      *rest = NS_CONST_CAST(char*, (const char*)attachmentList);
  nsCAutoString  str;
  
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) 
  {
    str = token;
    str.Trim(" \t", PR_TRUE, PR_TRUE); // trim leading and trailing white spaces
    
    if (!str.IsEmpty()) 
    {
      // Just look for files that are NOT local file attachments and do 
      // the right thing.
      //
      if (str.CompareWithConversion("file://", PR_TRUE, 7) != 0)
      {
#ifdef NS_DEBUG
        printf("Adding REMOTE attachment %d: %s\n", newLoc, str.GetBuffer());
#endif

        m_attachments[newLoc].mDeleteFile = PR_TRUE;
        m_attachments[newLoc].m_done = PR_FALSE;
			  m_attachments[newLoc].m_mime_delivery_state = this;

        if (m_attachments[newLoc].mURL)
          NS_RELEASE(m_attachments[newLoc].mURL);

        nsMsgNewURL(&(m_attachments[newLoc].mURL), str);

        PR_FREEIF(m_attachments[newLoc].m_charset);
        m_attachments[newLoc].m_charset = PL_strdup(mCompFields->GetCharacterSet());
        PR_FREEIF(m_attachments[newLoc].m_encoding);
        m_attachments[newLoc].m_encoding = PL_strdup ("7bit");
        
        /* Count up attachments which are going to come from mail folders
        and from NNTP servers. */
        nsXPIDLCString turl;
        if (m_attachments[newLoc].mURL)
        {
          msg_pick_real_name(&m_attachments[newLoc],
                             mCompFields->GetCharacterSet());
          ++newLoc;
        }
        else if (str.Find("_message:") != -1)
        {
          if (str.Find("mailbox_message:") != -1 ||
              str.Find("imap_message:") != -1)
            (*aMailboxCount)++;
          else if (str.Find("news_message:") != -1 ||
                   str.Find("snews_message:") != -1)
            (*aNewsCount)++;
          
          m_attachments[newLoc].m_uri = str.ToNewCString();
          ++newLoc;
		}
      }

      str = "";
    }

    token = nsCRT::strtok(rest, ",", &rest);
  }

  PR_FREEIF(attachmentList);
  return NS_OK;
}

int 
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
  if (mEditor)
    mRemoteAttachmentCount = mMultipartRelatedAttachmentCount = GetMultipartRelatedCount();
  else
    mRemoteAttachmentCount = mMultipartRelatedAttachmentCount = 0;  // The constructor took care of this, but just to be sure

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
  nsCRT::memset(m_attachments, 0, (sizeof(nsMsgAttachmentHandler) * m_attachment_count)); 
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
      m_attachments[i].mDeleteFile = PR_FALSE;
			m_attachments[i].m_mime_delivery_state = this;

			/* These attachments are already "snarfed". */
			m_attachments[i].m_done = PR_TRUE;
			NS_ASSERTION (preloaded_attachments[i].orig_url, "null url");

      if (m_attachments[i].mURL)
        NS_RELEASE(m_attachments[i].mURL);
			m_attachments[i].mURL = preloaded_attachments[i].orig_url;

			PR_FREEIF(m_attachments[i].m_type);
			m_attachments[i].m_type = PL_strdup (preloaded_attachments[i].type);

      // Set it to the compose fields for a default...
			PR_FREEIF(m_attachments[i].m_charset);
      m_attachments[i].m_charset = PL_strdup (mCompFields->GetCharacterSet());

      // If we still don't have a content type, we should really try sniff one out!
      if ((!m_attachments[i].m_type) || (!*m_attachments[i].m_type))
      {
        m_attachments[i].PickEncoding(mCompFields->GetCharacterSet());
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
        delete (m_attachments[i].mFileSpec);
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
              msg_pick_real_name(&m_attachments[i], mCompFields->GetCharacterSet());
		}
	}

  // First, handle the multipart related attachments if any...
  //
  PRInt32     mailbox_count = 0, news_count = 0;

  if ((mEditor) && (mMultipartRelatedAttachmentCount > 0))
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
  if (NS_FAILED( AddCompFieldRemoteAttachments( (mPreloadedAttachmentCount + mMultipartRelatedAttachmentCount), 
                                                 &mailbox_count, &news_count) ))
    return NS_ERROR_INVALID_ARG;

  //
  // Now deal remote attachments and attach multipart/related attachments (url's and such..)
  // first!
  //
  if (attachments && attachments[0].url) 
  {
    PRInt32     locCount = -1;

    for (i = (mPreloadedAttachmentCount + mMultipartRelatedAttachmentCount + mCompFieldRemoteAttachments); i < m_attachment_count; i++) 
    {
      locCount++;
      m_attachments[i].mDeleteFile = PR_TRUE;
      m_attachments[i].m_done = PR_FALSE;
			m_attachments[i].m_mime_delivery_state = this;
			NS_ASSERTION (attachments[locCount].url, "null url");

      if (m_attachments[i].mURL)
        NS_RELEASE(m_attachments[i].mURL);
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

			// real name is set in the case of vcard so don't change it.
			// m_attachments[i].m_real_name = 0;

			/* Count up attachments which are going to come from mail folders
			and from NNTP servers. */
      		if (m_attachments[i].mURL)
      		{
	      		nsXPIDLCString turl;
	      		m_attachments[i].mURL->GetSpec(getter_Copies(turl));
				if (PL_strncasecmp(turl, "mailbox:",8) ||
						PL_strncasecmp(turl, "IMAP:",5))
					mailbox_count++;
				else
					if (PL_strncasecmp(turl, "news:",5) ||
							PL_strncasecmp(turl, "snews:",6))
						news_count++;

                if (m_attachments[i].mURL)
                  msg_pick_real_name(&m_attachments[i], mCompFields->GetCharacterSet());
			}
		}
  }

  if ( (attachments && attachments[0].url) || 
       (mMultipartRelatedAttachmentCount > 0) ||
       (mCompFieldRemoteAttachments > 0) )
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
        PR_FREEIF(printfString);  
      }
      
      int status = m_attachments[i].SnarfAttachment(mCompFields);
      if (status < 0)
        return status;
      if (m_be_synchronous_p)
        break;
    }
  }

  // If no attachments - finish now (this will call the done_callback).
	if (m_attachment_pending_count <= 0)
		return GatherMimeAttachments();

	return 0;
}

int nsMsgComposeAndSend::SetMimeHeader(MSG_HEADER_SET header, const char *value)
{
	char * dupHeader = nsnull;
	PRInt32	ret = NS_ERROR_OUT_OF_MEMORY;

	if (header & (MSG_FROM_HEADER_MASK | MSG_TO_HEADER_MASK | MSG_REPLY_TO_HEADER_MASK | MSG_CC_HEADER_MASK | MSG_BCC_HEADER_MASK))
		dupHeader = mime_fix_addr_header(value);
	else if (header &  (MSG_NEWSGROUPS_HEADER_MASK| MSG_FOLLOWUP_TO_HEADER_MASK))
		dupHeader = mime_fix_news_header(value);
	else  if (header & (MSG_FCC_HEADER_MASK | MSG_ORGANIZATION_HEADER_MASK |  MSG_SUBJECT_HEADER_MASK | 
                      MSG_REFERENCES_HEADER_MASK | MSG_X_TEMPLATE_HEADER_MASK | MSG_ATTACHMENTS_HEADER_MASK))
		dupHeader = mime_fix_header(value);
	else
		NS_ASSERTION(PR_FALSE, "invalid header");	// unhandled header mask - bad boy.

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
    const char *fieldsFCC = fields->GetFcc();
    if (fieldsFCC && *fieldsFCC)
    {
      if (PL_strcasecmp(fieldsFCC, "nocopy://") == 0)
        mCompFields->SetFcc("");
      else
        SetMimeHeader(MSG_FCC_HEADER_MASK, fieldsFCC); 
    }
    else
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
	mCompFields->SetDefaultBody((char *) fields->GetDefaultBody());

	/* strip whitespace from and duplicate header fields. */
	SetMimeHeader(MSG_FROM_HEADER_MASK, fields->GetFrom());
	SetMimeHeader(MSG_REPLY_TO_HEADER_MASK, fields->GetReplyTo());
	SetMimeHeader(MSG_TO_HEADER_MASK, fields->GetTo());
	SetMimeHeader(MSG_CC_HEADER_MASK, fields->GetCc());
	SetMimeHeader(MSG_BCC_HEADER_MASK, fields->GetBcc());
	SetMimeHeader(MSG_NEWSGROUPS_HEADER_MASK, fields->GetNewsgroups());
	SetMimeHeader(MSG_FOLLOWUP_TO_HEADER_MASK, fields->GetFollowupTo());
	SetMimeHeader(MSG_ORGANIZATION_HEADER_MASK, fields->GetOrganization());
	SetMimeHeader(MSG_SUBJECT_HEADER_MASK, fields->GetSubject());
	SetMimeHeader(MSG_REFERENCES_HEADER_MASK, fields->GetReferences());
	SetMimeHeader(MSG_X_TEMPLATE_HEADER_MASK, fields->GetTemplateName());

  // For the new way to deal with attachments from the FE...
  SetMimeHeader(MSG_ATTACHMENTS_HEADER_MASK, fields->GetAttachments());

	pStr = fields->GetOtherRandomHeaders();
	if (pStr)
		mCompFields->SetOtherRandomHeaders((char *) pStr);

	pStr = fields->GetPriority();
	if (pStr)
		mCompFields->SetPriority((char *) pStr);

	int i, j = (int) MSG_LAST_BOOL_HEADER_MASK;
	for (i = 0; i < j; i++) 
  {
		mCompFields->SetBoolHeader((MSG_BOOL_HEADER_SET) i, fields->GetBoolHeader((MSG_BOOL_HEADER_SET) i));
	}

	mCompFields->SetForcePlainText(fields->GetForcePlainText());
	mCompFields->SetUseMultipartAlternative(fields->GetUseMultipartAlternative());

	//
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

    if (attachment1_body_length <= 0)
      attachment1_body = 0;
    
    if (attachment1_body)
    {
      char *newb = (char *) PR_Malloc (attachment1_body_length + 1);
      if (! newb)
        return NS_ERROR_OUT_OF_MEMORY;
      nsCRT::memcpy (newb, attachment1_body, attachment1_body_length);
      newb [attachment1_body_length] = 0;
      m_attachment1_body = newb;
      m_attachment1_body_length = attachment1_body_length;
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
						  nsMsgCompFields *fields,
              nsFileSpec      *sendFileSpec,
						  PRBool digest_p,
						  PRBool dont_deliver_p,
						  nsMsgDeliverMode mode,
              nsIMessage      *msgToReplace,
						  const char *attachment1_type,
						  const char *attachment1_body,
						  PRUint32 attachment1_body_length,
						  const nsMsgAttachmentData *attachments,
						  const nsMsgAttachedFile *preloaded_attachments)
{
	nsresult      rv = NS_OK;
  nsXPIDLString msg;
  if (!mComposeBundle)
    mComposeBundle = do_GetService(NS_MSG_COMPOSESTRINGSERVICE_PROGID);

  // Tell the user we are assembling the message...
  mComposeBundle->GetStringByID(NS_MSG_ASSEMBLING_MESSAGE, getter_Copies(msg));
  SetStatusMessage( msg );

  // 
  // The Init() method should initialize a send operation for full
  // blown create and send operations as well as just the "send a file"
  // operations. 
  //
	m_dont_deliver_p = dont_deliver_p;
	m_deliver_mode = mode;
  mMsgToReplace = msgToReplace;

  mUserIdentity = aUserIdentity;
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
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv) && prefs) 
  {
    rv = prefs->GetBoolPref(PREF_MAIL_STRICTLY_MIME, &strictly_mime);
    rv = prefs->GetIntPref(PREF_MAIL_MESSAGE_WARNING_SIZE, (PRInt32 *) &mMessageWarningSize);
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

  return HackAttachments(attachments, preloaded_attachments);
}

nsresult
MailDeliveryCallback(nsIURI *aUrl, nsresult aExitCode, void *tagData)
{
  if (tagData)
  {
    nsMsgComposeAndSend *ptr = (nsMsgComposeAndSend *) tagData;

    if (!ptr) return NS_ERROR_NULL_POINTER;

	if (NS_FAILED(aExitCode))
		switch (aExitCode)
		{
			case NS_ERROR_UNKNOWN_HOST:
				aExitCode = NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER;
				break;
			default:
				if (! NS_IS_MSG_ERROR(aExitCode))
					aExitCode = NS_ERROR_SEND_FAILED;
				break;
		}
    ptr->DeliverAsMailExit(aUrl, aExitCode);

    NS_RELEASE(ptr);
  }

  return aExitCode;
}

nsresult
NewsDeliveryCallback(nsIURI *aUrl, nsresult aExitCode, void *tagData)
{
  if (tagData)
  {
    nsMsgComposeAndSend *ptr = (nsMsgComposeAndSend *) tagData;

    if (!ptr) return NS_ERROR_NULL_POINTER;

	if (NS_FAILED(aExitCode))
		switch (aExitCode)
		{
			default:
				if (! NS_IS_MSG_ERROR(aExitCode))
					aExitCode = NS_ERROR_SEND_FAILED;
				break;
		}
    ptr->DeliverAsNewsExit(aUrl, aExitCode, ptr->mSendMailAlso);

    NS_RELEASE(ptr);
  }

  return aExitCode;
}

nsresult
nsMsgComposeAndSend::DeliverMessage()
{
	PRBool mail_p = ((mCompFields->GetTo() && *mCompFields->GetTo()) || 
					(mCompFields->GetCc() && *mCompFields->GetCc()) || 
					(mCompFields->GetBcc() && *mCompFields->GetBcc()));
	PRBool news_p = (mCompFields->GetNewsgroups() && 
				    *(mCompFields->GetNewsgroups()) ? PR_TRUE : PR_FALSE);

	if ( m_deliver_mode != nsMsgSaveAsDraft &&
			m_deliver_mode != nsMsgSaveAsTemplate )
		NS_ASSERTION(mail_p || news_p, "message without destination");

	if (m_deliver_mode == nsMsgQueueForLater) 
  {
		return QueueForLater();
	}
	else if (m_deliver_mode == nsMsgSaveAsDraft) 
  {
		return SaveAsDraft();
	}
	else if (m_deliver_mode == nsMsgSaveAsTemplate) 
  {
		return SaveAsTemplate();
	}

  //
  // Ok, we are about to send the file that we have built up...but what
  // if this is a mongo email...we should have a way to warn the user that
  // they are about to do something they may not want to do.
  //
  if ( (mMessageWarningSize > 0) && (mTempFileSpec->GetFileSize() > mMessageWarningSize) && (mGUINotificationEnabled))
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
          Fail(prompt, NS_ERROR_BUT_DONT_SHOW_ALERT, printfString);
          PR_FREEIF(printfString);
          return NS_ERROR_FAILURE;
        }
        else
          PR_FREEIF(printfString);
      }

    }
  }

	if (news_p) {
      if (mail_p) {
        mSendMailAlso = PR_TRUE;
      }
      return DeliverFileAsNews();   /* will call DeliverFileAsMail if it needs to */
    }
	else if (mail_p) {
		return DeliverFileAsMail();
    }
	else {
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
  
  nsCOMPtr<nsIPrompt> promptObject;
  GetDefaultPrompt(getter_AddRefs(promptObject));

	if (!buf) 
  {
    nsXPIDLString eMsg; 
    mComposeBundle->GetStringByID(NS_ERROR_OUT_OF_MEMORY, getter_Copies(eMsg));
    
    Fail(promptObject, NS_ERROR_OUT_OF_MEMORY, eMsg);
    NotifyListenersOnStopSending(nsnull, NS_ERROR_OUT_OF_MEMORY, nsnull, nsnull);
    return NS_ERROR_OUT_OF_MEMORY;
	}

	nsresult rv;

	PRBool collectOutgoingAddresses = PR_TRUE;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
	if (NS_SUCCEEDED(rv) && prefs)
	{
		prefs->GetBoolPref(PREF_MAIL_COLLECT_EMAIL_ADDRESS_OUTGOING,&collectOutgoingAddresses);
	}
 

	NS_WITH_SERVICE(nsIAbAddressCollecter, addressCollecter,
					kCAddressCollecter, &rv);

	if (!NS_SUCCEEDED(rv))
		addressCollecter = nsnull;

	PRBool collectAddresses = (collectOutgoingAddresses && addressCollecter);

	PL_strcpy (buf, "");
	buf2 = buf + PL_strlen (buf);
	if (mCompFields->GetTo() && *mCompFields->GetTo())
	{
		PL_strcat (buf2, mCompFields->GetTo());
		if (collectAddresses)
			addressCollecter->CollectAddress(mCompFields->GetTo());
	}
	if (mCompFields->GetCc() && *mCompFields->GetCc()) {
		if (*buf2) PL_strcat (buf2, ",");
			PL_strcat (buf2, mCompFields->GetCc());
		if (collectAddresses)
			addressCollecter->CollectAddress(mCompFields->GetCc());
	}
	if (mCompFields->GetBcc() && *mCompFields->GetBcc()) {
		if (*buf2) PL_strcat (buf2, ",");
			PL_strcat (buf2, mCompFields->GetBcc());
		if (collectAddresses)
			addressCollecter->CollectAddress(mCompFields->GetBcc());
	}

  // Ok, now MIME II encode this to prevent 8bit problems...
  char *convbuf = nsMsgI18NEncodeMimePartIIStr((char *)buf, mCompFields->GetCharacterSet(),
												                       nsMsgMIMEGetConformToStandard());
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
  
  NS_WITH_SERVICE(nsISmtpService, smtpService, kSmtpServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && smtpService)
  {
    nsMsgDeliveryListener * aListener = new nsMsgDeliveryListener(MailDeliveryCallback, nsMailDelivery, this);
    mMailSendListener = do_QueryInterface(aListener);

    if (!mMailSendListener)
    {
      if (mGUINotificationEnabled)
        nsMsgDisplayMessageByID(promptObject, NS_ERROR_SENDING_MESSAGE);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Note: Don't do a SetMsgComposeAndSendObject since we are in the same thread, and
    // using callbacks for notification
    // 
  	NS_ADDREF_THIS(); // why are we forcing an addref on ourselves? this doesn't look right to me
	  nsCOMPtr<nsIFileSpec> aFileSpec;
	  NS_NewFileSpecWithSpec(*mTempFileSpec, getter_AddRefs(aFileSpec));

    // we used to get the prompt from the compose window and we'd pass that in
    // to the smtp protocol as the prompt to use. But when you send a message,
    // we dismiss the compose window.....so you are parenting off of a window that
    // isn't there. To have it work correctly I think we want the alert dialogs to be modal
    // to the top most mail window...after all, that's where we are going to be sending status
    // update information too....

    nsCOMPtr<nsIPrompt> netPrompt;
    GetDefaultPrompt(getter_AddRefs(netPrompt));
    
    if (!netPrompt)
      netPrompt = do_GetService(kCNetSupportDialogCID);

    // Tell the user we are sending the message!
    nsXPIDLString msg; 
    mComposeBundle->GetStringByID(NS_MSG_SENDING_MESSAGE, getter_Copies(msg));
    SetStatusMessage( msg );

    rv = smtpService->SendMailMessage(aFileSpec, buf, mUserIdentity,
                                      mMailSendListener, nsnull, 
                                      netPrompt, nsnull);
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
  nsCOMPtr<nsIPrompt> promptObject;
  GetDefaultPrompt(getter_AddRefs(promptObject));

  NS_WITH_SERVICE(nsINntpService, nntpService, kNntpServiceCID, &rv);

  if (NS_SUCCEEDED(rv) && nntpService) 
  {
    nsMsgDeliveryListener * aListener = new nsMsgDeliveryListener(NewsDeliveryCallback, nsNewsDelivery, this);
    mNewsPostListener = do_QueryInterface(aListener);

    if (!mNewsPostListener)
    {
      if (mGUINotificationEnabled)
        nsMsgDisplayMessageByID(promptObject, NS_ERROR_SENDING_MESSAGE);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Note: Don't do a SetMsgComposeAndSendObject since we are in the same thread, and
    // using callbacks for notification
    // 
	NS_ADDREF_THIS();  // two addrefs on ourselves? This looks bogus too....
 	AddRef();
	nsCOMPtr<nsIFileSpec>fileToPost;
	
	rv = NS_NewFileSpecWithSpec(*mTempFileSpec, getter_AddRefs(fileToPost));
	if (NS_FAILED(rv)) return rv;

  // Tell the user we are posting the message!
  nsXPIDLString msg; 
  mComposeBundle->GetStringByID(NS_MSG_POSTING_MESSAGE, getter_Copies(msg));
  SetStatusMessage( msg );

	nsCOMPtr <nsIMsgMailSession> mailSession = do_GetService(kMsgMailSessionCID, &rv);
	if (NS_FAILED(rv)) return rv;

	if (!mailSession) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMsgWindow>    msgWindow;

	rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
	if(NS_FAILED(rv))
		return rv;

	if (!msgWindow) return NS_ERROR_FAILURE;

    rv = nntpService->PostMessage(fileToPost, mCompFields->GetNewsgroups(), mCompFields->GetNewshost(), mNewsPostListener, msgWindow, nsnull);
	if (NS_FAILED(rv)) return rv;
  }

  return rv;
}

void 
nsMsgComposeAndSend::Fail(nsIPrompt * aPrompt, nsresult failure_code, const PRUnichar * error_msg)
{
  if (NS_FAILED(failure_code))
  {
    // in certain cases, we've already shown the alert
    // and we don't need to show another alert here.
    if (failure_code != NS_ERROR_BUT_DONT_SHOW_ALERT) {
      if (mGUINotificationEnabled)
      {
	      if (!error_msg)
	        nsMsgDisplayMessageByID(aPrompt, failure_code);
	      else
	        nsMsgDisplayMessageByString(aPrompt, error_msg);
      }
    }
  }

  if (m_attachments_done_callback)
	{
	  /* mime_free_message_state will take care of cleaning up the
		   attachment files and attachment structures */
	  m_attachments_done_callback (failure_code, error_msg, nsnull);
    m_attachments_done_callback = nsnull;
	}
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
    mComposeBundle->GetStringByID(aExitCode, getter_Copies(eMsg));
    
    // we need a prompt interface for the alert.....but the compose window is currently hidden...
    // so try to use the prompt interface associated with the smtp url...
    nsCOMPtr<nsISmtpUrl> smtpUrl (do_QueryInterface(aUri));
    nsCOMPtr<nsIPrompt> prompt;
    if (smtpUrl)
    {
      smtpUrl->GetPrompt(getter_AddRefs(prompt));
    }
    else
    {
      nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl (do_QueryInterface(aUri));
      if (mailnewsUrl)
      {
        nsCOMPtr <nsIMsgWindow> msgWindow;
        mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
        if (msgWindow)
          msgWindow->GetPromptDialog(getter_AddRefs(prompt));
      }
    }
    Fail(prompt, aExitCode, eMsg);
    NotifyListenersOnStopSending(nsnull, aExitCode, nsnull, nsnull);
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
  NotifyListenersOnStopSending(nsnull, aExitCode, nsnull, nsnull);

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

void
nsMsgComposeAndSend::DeliverAsMailExit(nsIURI *aUrl, nsresult aExitCode)
{
  DoDeliveryExitProcessing(aUrl, aExitCode, PR_FALSE);
  return;
}

void
nsMsgComposeAndSend::DeliverAsNewsExit(nsIURI *aUrl, nsresult aExitCode, PRBool sendMailAlso)
{
  DoDeliveryExitProcessing(aUrl, aExitCode, sendMailAlso);
  return;
}

// 
// Now, start the appropriate copy operation.
//
nsresult
nsMsgComposeAndSend::DoFcc()
{
  //
  // Just cleanup and return success if no FCC is necessary
  //
  if (!mCompFields->GetFcc() || !*mCompFields->GetFcc())
  {
#ifdef NS_DEBUG
  printf("\nCopy operation disabled by user!\n");
#endif

    NotifyListenersOnStopSending(nsnull, NS_OK, nsnull, nsnull);
    NotifyListenersOnStopCopy(NS_OK);  // For closure of compose window...
    return NS_OK;
  }

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
    NotifyListenersOnStopCopy(rv);
  }

  return rv;
}

nsresult
nsMsgComposeAndSend::SetListenerArray(nsIMsgSendListener **aListenerArray,
                                      PRUint32 aListeners)
{
  if ( (!aListenerArray) || (!*aListenerArray) )
    return NS_OK;

  // First, count the listeners passed in...
  mListenerArrayCount = aListeners;

  // now allocate an array to hold the number of entries.
  mListenerArray = (nsIMsgSendListener **) PR_Malloc(sizeof(nsIMsgSendListener *) * mListenerArrayCount);
  if (!mListenerArray)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCRT::memset(mListenerArray, 0, (sizeof(nsIMsgSendListener *) * mListenerArrayCount));
  
  // Now assign the listeners...
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
  {
    mListenerArray[i] = aListenerArray[i];
    NS_IF_ADDREF(mListenerArray[i]);
  }

  return NS_OK;
}

nsresult
nsMsgComposeAndSend::AddListener(nsIMsgSendListener *aListener)
{
  if ( (mListenerArrayCount > 0) || mListenerArray )
  {
    ++mListenerArrayCount;
    mListenerArray = (nsIMsgSendListener **) 
                  PR_Realloc(*mListenerArray, sizeof(nsIMsgSendListener *) * mListenerArrayCount);
    if (!mListenerArray)
      return NS_ERROR_OUT_OF_MEMORY;
    else
    {
      mListenerArray[mListenerArrayCount - 1] = aListener;
      return NS_OK;
    }
  }
  else
  {
    mListenerArrayCount = 1;
    mListenerArray = (nsIMsgSendListener **) PR_Malloc(sizeof(nsIMsgSendListener *) * mListenerArrayCount);
    if (!mListenerArray)
      return NS_ERROR_OUT_OF_MEMORY;

    nsCRT::memset(mListenerArray, 0, (sizeof(nsIMsgSendListener *) * mListenerArrayCount));
  
    mListenerArray[0] = aListener;
    NS_IF_ADDREF(mListenerArray[0]);
    return NS_OK;
  }
}

nsresult
nsMsgComposeAndSend::RemoveListener(nsIMsgSendListener *aListener)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] == aListener)
    {
      NS_RELEASE(mListenerArray[i]);
      mListenerArray[i] = nsnull;
      return NS_OK;
    }

  return NS_ERROR_INVALID_ARG;
}

nsresult
nsMsgComposeAndSend::DeleteListeners()
{
  if ( (mListenerArray) && (*mListenerArray) )
  {
    PRInt32 i;
    for (i=0; i<mListenerArrayCount; i++)
    {
      NS_IF_RELEASE(mListenerArray[i]);
    }
    
    PR_FREEIF(mListenerArray);
  }

  mListenerArrayCount = 0;
  return NS_OK;
}

nsresult
nsMsgComposeAndSend::NotifyListenersOnStartSending(const char *aMsgID, PRUint32 aMsgSize)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStartSending(aMsgID, aMsgSize);

  return NS_OK;
}

nsresult
nsMsgComposeAndSend::NotifyListenersOnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnProgress(aMsgID, aProgress, aProgressMax);

  return NS_OK;
}

nsresult
nsMsgComposeAndSend::NotifyListenersOnStatus(const char *aMsgID, const PRUnichar *aMsg)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStatus(aMsgID, aMsg);

  return NS_OK;
}

nsresult
nsMsgComposeAndSend::NotifyListenersOnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                                                  nsIFileSpec *returnFileSpec)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStopSending(aMsgID, aStatus, aMsg, returnFileSpec);

  return NS_OK;
}

nsresult
nsMsgComposeAndSend::NotifyListenersOnStartCopy()
{
  nsCOMPtr<nsIMsgCopyServiceListener> copyListener;

  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
  {
    if (mListenerArray[i] != nsnull)
    {
      copyListener = do_QueryInterface(mListenerArray[i]);
      if (copyListener)
        copyListener->OnStartCopy();      
    }
  }

  return NS_OK;
}

nsresult
nsMsgComposeAndSend::NotifyListenersOnProgressCopy(PRUint32 aProgress,
                                                   PRUint32 aProgressMax)
{
  nsCOMPtr<nsIMsgCopyServiceListener> copyListener;

  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
  {
    if (mListenerArray[i] != nsnull)
    {
      copyListener = do_QueryInterface(mListenerArray[i]);
      if (copyListener)
        copyListener->OnProgress(aProgress, aProgressMax);
    }
  }

  return NS_OK;  
}

nsresult
nsMsgComposeAndSend::SetMessageKey(PRUint32 aMessageKey)
{
    m_messageKey = aMessageKey;
    return NS_OK;
}

nsresult
nsMsgComposeAndSend::GetMessageId(nsCString* aMessageId)
{
    if (aMessageId && mCompFields)
    {
        *aMessageId = mCompFields->GetMessageId();
        return NS_OK;
    }
    return NS_ERROR_NULL_POINTER;
}

nsresult
nsMsgComposeAndSend::NotifyListenersOnStopCopy(nsresult aStatus)
{
  nsCOMPtr<nsIMsgCopyServiceListener> copyListener;

  PRInt32 i;

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

  // Ok, now to support a second copy operation, we need to figure
  // out which copy request just finished. If the user has requested
  // a second copy operation, then we need to fire that off, but if they
  // just wanted a single copy operation, we can tell everyone we are done
  // and move on with life. Only do the second copy if the first one worked.
  //
  if ( NS_SUCCEEDED(aStatus) && (mNeedToPerformSecondFCC) )
  {
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
      {
        //
        // If we hit here, the copy operation FAILED and we should at least tell the
        // user that it did fail but the send operation has already succeeded.
        //
        PRBool oopsGiveMeBackTheComposeWindow = PR_FALSE;

        nsXPIDLString eMsg; 
        mComposeBundle->GetStringByID(NS_MSG_FAILED_COPY_OPERATION, getter_Copies(eMsg));
        Fail(prompt, NS_ERROR_BUT_DONT_SHOW_ALERT, eMsg);

        if (mGUINotificationEnabled)
        {
          nsMsgAskBooleanQuestionByString(prompt, eMsg, &oopsGiveMeBackTheComposeWindow);
          if (!oopsGiveMeBackTheComposeWindow)
            rv = NS_OK;
          else
            aStatus = NS_ERROR_FAILURE;
        }
      }
      else
        return NS_OK;
    }
  }
  else if (NS_FAILED(aStatus))
  {
    //
    // If we hit here, the ASYNC copy operation FAILED and we should at least tell the
    // user that it did fail but the send operation has already succeeded.
    //
    PRBool oopsGiveMeBackTheComposeWindow = PR_FALSE;

    nsXPIDLString eMsg; 
    mComposeBundle->GetStringByID(NS_MSG_FAILED_COPY_OPERATION, getter_Copies(eMsg));
    Fail(prompt, NS_ERROR_BUT_DONT_SHOW_ALERT, eMsg);

    if (mGUINotificationEnabled)
    {
      nsMsgAskBooleanQuestionByString(prompt, eMsg, &oopsGiveMeBackTheComposeWindow);
      if (!oopsGiveMeBackTheComposeWindow)
    	  aStatus = NS_OK;
    }
  }

  // If we are here, its real cleanup time! 
  for (i=0; i<mListenerArrayCount; i++)
  {
    if (mListenerArray[i] != nsnull)
    {
      copyListener = do_QueryInterface(mListenerArray[i]);
      if (copyListener)
        copyListener->OnStopCopy(aStatus);
    }
  }

  return NS_OK;
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
nsresult 
nsMsgComposeAndSend::CreateAndSendMessage(
              nsIEditorShell                    *aEditor,
              nsIMsgIdentity                    *aUserIdentity,
 						  nsIMsgCompFields                  *fields,
						  PRBool                            digest_p,
						  PRBool                            dont_deliver_p,
						  nsMsgDeliverMode                  mode,
              nsIMessage                        *msgToReplace,
						  const char                        *attachment1_type,
						  const char                        *attachment1_body,
						  PRUint32                          attachment1_body_length,
						  const nsMsgAttachmentData         *attachments,
						  const nsMsgAttachedFile           *preloaded_attachments,
						  void                              *relatedPart,
              nsIMsgSendListener                **aListenerArray,
              PRUint32 aListeners)
{
  nsresult      rv;

  SetListenerArray(aListenerArray, aListeners);

  if (!attachment1_body || !*attachment1_body)
  {
		attachment1_body_length = 0;
    attachment1_body = (char *) nsnull;
  }

  // Set the editor for MHTML operations if necessary
  if (aEditor)
    mEditor = aEditor;

  rv = Init(aUserIdentity, (nsMsgCompFields *)fields, nsnull,
					digest_p, dont_deliver_p, mode, msgToReplace,
					attachment1_type, attachment1_body,
					attachment1_body_length,
					attachments, preloaded_attachments);

	if (NS_SUCCEEDED(rv))
		return NS_OK;
  else
    return rv;
}

nsresult
nsMsgComposeAndSend::SendMessageFile(
              nsIMsgIdentity                    *aUserIndentity,
 						  nsIMsgCompFields                  *fields,
              nsIFileSpec                        *sendIFileSpec,
              PRBool                            deleteSendFileOnCompletion,
						  PRBool                            digest_p,
						  nsMsgDeliverMode                  mode,
              nsIMessage                        *msgToReplace,
              nsIMsgSendListener                **aListenerArray,
              PRUint32 aListeners)
{
  nsresult      rv;

  if (!fields)
    return NS_ERROR_INVALID_ARG;

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

  // Setup the listeners...
  SetListenerArray(aListenerArray, aListeners);

  // Should we delete the temp file when done?
  if (!deleteSendFileOnCompletion)
  {
    NS_NewFileSpecWithSpec(*sendFileSpec, &mReturnFileSpec);
	  if (!mReturnFileSpec)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = Init(aUserIndentity, (nsMsgCompFields *)fields, sendFileSpec,
					    digest_p, PR_FALSE, mode, msgToReplace, 
					    nsnull, nsnull, nsnull,
					    nsnull, nsnull);
	if (NS_SUCCEEDED(rv))
  { 
    return DeliverMessage();
  }
  else
    return rv;
}

nsMsgAttachmentData *
BuildURLAttachmentData(nsIURI *url)
{
  int                 attachCount = 2;  // one entry and one empty entry
  nsMsgAttachmentData *attachments = nsnull;
  char                *theName = nsnull;
  nsXPIDLCString spec;

  if (!url)
    return nsnull;    

  attachments = (nsMsgAttachmentData *) PR_Malloc(sizeof(nsMsgAttachmentData) * attachCount);
  if (!attachments)
    return nsnull;

  // Now get a readable name...
  url->GetSpec(getter_Copies(spec));
  if (spec)
  {
    theName = PL_strrchr(spec, '/');
  }

  if (!theName)
    theName = "Unknown"; // Don't I18N this string...should never happen...
  else
    theName++;

  nsCRT::memset(attachments, 0, sizeof(nsMsgAttachmentData) * attachCount);
  attachments[0].url = url; // The URL to attach. This should be 0 to signify "end of list".
  attachments[0].real_name = (char *)PL_strdup(theName);	// The original name of this document, which will eventually show up in the 

  NS_IF_ADDREF(url);
  return attachments;
}

nsresult
nsMsgComposeAndSend::SendWebPage(nsIMsgIdentity               *aUserIndentity,
                                 nsIMsgCompFields             *fields,
                                 nsIURI                       *url,
                                 nsMsgDeliverMode              mode,
                                 nsIMsgSendListener           **aListenerArray,
                                 PRUint32 aListeners)
{
  nsresult            rv;
  nsMsgAttachmentData *tmpPageData = nsnull;
  
  //
  // First check to see if the fields are valid...
  //
  if ((!fields) || (!url) )
    return NS_ERROR_INVALID_ARG;

  tmpPageData = BuildURLAttachmentData(url);

  // Setup the listeners...
  SetListenerArray(aListenerArray, aListeners);

  /* string GetBody(); */
  PRInt32       bodyLen;
  const char    *msgBody = ((nsMsgCompFields*)fields)->GetBody();
  nsXPIDLCString body;
  if (!msgBody)
  {
    url->GetSpec(getter_Copies(body));
    msgBody = (const char *)body;
  }

  bodyLen = PL_strlen(msgBody);
  rv = CreateAndSendMessage(
              nsnull,    // no MHTML in this case...
              aUserIndentity,
 						  fields, //nsIMsgCompFields                  *fields,
						  PR_FALSE, //PRBool                            digest_p,
						  PR_FALSE, //PRBool                            dont_deliver_p,
						  mode,   //nsMsgDeliverMode                  mode,
              nsnull, //  nsIMessage      *msgToReplace,
						  TEXT_PLAIN, //const char                        *attachment1_type,
              msgBody, //const char                        *attachment1_body,
						  bodyLen, // PRUint32                          attachment1_body_length,
						  tmpPageData, // const nsMsgAttachmentData  *attachments,
						  nsnull,  // const nsMsgAttachedFile    *preloaded_attachments,
						  nsnull, // void                              *relatedPart,
              aListenerArray, aListeners);  
  return rv;
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
      NotifyListenersOnStopCopy(rv);
    
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
  char          *folderName = nsnull;
  nsXPIDLString msg; 

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

  delete tFileSpec;

  nsOutputFileStream tempOutfile(mCopyFileSpec);
  if (! tempOutfile.is_open()) 
  {	  
    // Need to determine what type of operation failed and set status accordingly. 
    switch (mode)
    {
    case nsMsgQueueForLater:
      status = NS_MSG_UNABLE_TO_SEND_LATER;
      break;
    case nsMsgSaveAsDraft:
      status = NS_MSG_UNABLE_TO_SAVE_DRAFT;
      break;
    case nsMsgSaveAsTemplate:
      status = NS_MSG_UNABLE_TO_SAVE_TEMPLATE;
      break;
    case nsMsgDeliverNow:
    default:
      status = NS_MSG_COULDNT_OPEN_FCC_FOLDER;
      break;
    }
    NS_RELEASE(mCopyFileSpec);
    return status;
  }

  //
  // Get our files ready...
  //
  nsInputFileStream inputFile(*input_file);
  if (!inputFile.is_open())
	{
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
  if (mode == nsMsgDeliverNow && fcc_header)
  	turi = PL_strdup(fcc_header);
  else
  	turi = GetFolderURIFromUserPrefs(mode, mUserIdentity);
  status = MessageFolderIsLocal(mUserIdentity, mode, turi, &folderIsLocal);
  if (NS_FAILED(status)) { goto FAIL; }

  // Tell the user we are copying the message... 
  mComposeBundle->GetStringByID(NS_MSG_START_COPY_MESSAGE, getter_Copies(msg));
  if (msg)
  {
    folderName = GetFolderNameFromURLString(turi);
    if (folderName)
      printfString = nsTextFormatter::smprintf(msg, folderName);
    else
      printfString = nsTextFormatter::smprintf(msg, "?");
    if (printfString)
    {
      SetStatusMessage(printfString);	
      PR_FREEIF(printfString);  
    }
  }

  PR_FREEIF(folderName);

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
	      mode == nsMsgSaveAsTemplate || mode == nsMsgDeliverNow) &&
        folderIsLocal
     )
	{
	  char       *buf = 0;
	  PRUint16   flags = 0;

	  flags |= MSG_FLAG_READ;
	  if (mode == nsMsgQueueForLater)
  		flags |= MSG_FLAG_QUEUED;
	  buf = PR_smprintf(X_MOZILLA_STATUS_FORMAT CRLF, flags);
	  if (buf)
	  {
      PRInt32   len = PL_strlen(buf);
      n = tempOutfile.write(buf, len);
		  PR_FREEIF(buf);
		  if (n != len)
      {
        status = NS_ERROR_FAILURE;
			  goto FAIL;
      }
	  }
	  
	  PRUint32 flags2 = 0;
	  if (mode == nsMsgSaveAsTemplate)
		  flags2 |= MSG_FLAG_TEMPLATE;
	  buf = PR_smprintf(X_MOZILLA_STATUS2_FORMAT CRLF, flags2);
	  if (buf)
	  {
      PRInt32   len = PL_strlen(buf);
      n = tempOutfile.write(buf, len);
		  PR_FREEIF(buf);
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
  if ( (mode == nsMsgQueueForLater) && (mUserIdentity) )
  {
    char    *key = nsnull;

    if (NS_SUCCEEDED(mUserIdentity->GetKey(&key)) && (key))
    {
      char *tmpLine = PR_smprintf(HEADER_X_MOZILLA_IDENTITY_KEY ": %s" CRLF, key);
      if (tmpLine)
      {
        PRInt32 len = nsCRT::strlen(tmpLine);
        n = tempOutfile.write(tmpLine, len);
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
	  PRInt32 L = PL_strlen(bcc_header) + 20;
	  char *buf = (char *) PR_Malloc (L);
	  if (!buf)
		{
		  status = NS_ERROR_OUT_OF_MEMORY;
		  goto FAIL;
		}

	  PR_snprintf(buf, L-1, "BCC: %s" CRLF, bcc_header);
    PRInt32   len = PL_strlen(buf);
    n = tempOutfile.write(buf, len);
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
		  orig_hap = 0;
		  if (!line)
			{
			  status = NS_ERROR_OUT_OF_MEMORY;
			  goto FAIL;
			}

      PRInt32   len = PL_strlen(line);
      n = tempOutfile.write(line, len);
		  PR_FREEIF(line);
		  if (n != len)
      {
        status = NS_ERROR_FAILURE;
        goto FAIL;
      }
		}

	  PR_FREEIF(orig_hap);
	  orig_hap = 0;
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
    if (!inputFile.readline(ibuffer, ibuffer_size))
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
  if (ibuffer)
  	PR_FREEIF(ibuffer);
  if (obuffer && obuffer != ibuffer)
  	PR_FREEIF(obuffer);


  if (tempOutfile.is_open()) 
  {
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
	PR_FREEIF(turi);
	return status;
}

//
// This is pretty much a wrapper to the functionality that lives in the 
// nsMsgCopy class
//
nsresult 
nsMsgComposeAndSend::StartMessageCopyOperation(nsIFileSpec        *aFileSpec, 
                                               nsMsgDeliverMode   mode,
                                               char          	  *dest_uri)
{
  nsCAutoString uri;

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
  	uri = dest_uri;
  else
  	uri = GetFolderURIFromUserPrefs(mode, mUserIdentity);
  rv = mCopyObj->StartCopyOperation(mUserIdentity, aFileSpec, mode, 
                                    this, uri, mMsgToReplace);
  return rv;
}

//I'm getting this each time without holding onto the feedback so that 3 pane windows can be closed
//without any chance of crashing due to holding onto a deleted feedback.
nsresult
nsMsgComposeAndSend::SetStatusMessage(const PRUnichar *aMsgString)
{
  PRUnichar     *progressMsg;
  nsresult rv;
  if ( (!aMsgString) || (!mGUINotificationEnabled) )
    return NS_OK;

  progressMsg = nsCRT::strdup(aMsgString);
  nsCOMPtr <nsIMsgMailSession> mailSession = do_GetService(kMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) return rv;

  if (!mailSession) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMsgWindow>    msgWindow;

  rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
  if(NS_FAILED(rv))
    return rv;

  if (!msgWindow) return NS_OK;

  nsCOMPtr<nsIMsgStatusFeedback> feedback;
  rv = msgWindow->GetStatusFeedback(getter_AddRefs(feedback));
  if(NS_FAILED(rv)) return NS_OK;

  feedback->ShowStatusString(progressMsg);
  return NS_OK;
}

// For GUI notification...
nsresult
nsMsgComposeAndSend::SetGUINotificationState(PRBool aEnableFlag)
{
  mGUINotificationEnabled = aEnableFlag;
  return NS_OK;
}


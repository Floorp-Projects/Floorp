/* Insert copyright and license here 1995 */
/*   compose.c --- generation and delivery of MIME objects.
 */

#include "msgCore.h"

#include "rosetta_mailnews.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsMsgCompose.h"
#include "nsMsgSendPart.h"
#include "secrng.h"	/* for RNG_GenerateGlobalRandomBytes() */
#include "xp_time.h"
#include "nsMsgSendFact.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgRFC822Parser.h"
#include "nsINetService.h"
#include "nsISmtpService.h"  // for actually sending the message...
#include "nsMsgCompPrefs.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsMsgSend.h"

/* use these macros to define a class IID for our component. Our object currently supports two interfaces 
   (nsISupports and nsIMsgCompose) so we want to define constants for these two interfaces */
static NS_DEFINE_IID(kIMsgSend, NS_IMSGSEND_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kMsgRFC822ParserCID, NS_MSGRFC822PARSER_CID); 
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID); 

#if 0 //JFD
#include "msg.h"
#include "ntypes.h"
#include "structs.h"
#include "xlate.h"		/* Text and PostScript converters */
#include "merrors.h"
//#include "gui.h"		/* for XP_AppCodeName */
#include "mime.h"
#include "xp_time.h"	/* For XP_LocalZoneOffset() */
#include "libi18n.h"
#include "xpgetstr.h"
#include "prtime.h"
#include "prtypes.h"
#include "msgcom.h"
#include "msgsend.h"
#include "msgsendp.h"
#include "mozdb.h"
#include "maildb.h"
#include "mailhdr.h"
#include "msgprefs.h"
#include "msgmast.h"
#include "msgcpane.h"
#include "grpinfo.h"
#include "msgcflds.h"
#include "prefapi.h"
#include "abdefn.h"
#include "prsembst.h"
#include "secrng.h"	/* for RNG_GenerateGlobalRandomBytes() */
#include "addrbook.h"
#include "imaphost.h"
#include "imapoff.h"
#include "intl_csi.h"
#include "msgimap.h"
#include "msgurlq.h"
#endif //JFD

#ifdef XP_MAC
//#pragma warn_unusedarg off
#include "errors.h"
#endif // XP_MAC

// defined in msgCompGlue.cpp
extern char * INTL_EncodeMimePartIIStr(const char *header, const char *charset, PRBool bUseMime);
extern PRBool INTL_stateful_charset(const char *charset);

extern "C"
{
	extern int MK_MSG_ASSEMBLING_MSG;
	extern int MK_MSG_ASSEMB_DONE_MSG;
	extern int MK_MSG_LOAD_ATTACHMNT;
	extern int MK_MSG_LOAD_ATTACHMNTS;
	extern int MK_MSG_DELIV_MAIL;
	extern int MK_MSG_DELIV_MAIL_DONE;
	extern int MK_MSG_DELIV_NEWS;
	extern int MK_MSG_DELIV_NEWS_DONE;
	extern int MK_MSG_QUEUEING;
	extern int MK_MSG_WRITING_TO_FCC;
	extern int MK_MSG_QUEUED;
	extern int MK_MIME_ERROR_WRITING_FILE;
	extern int MK_MIME_MULTIPART_BLURB;
	extern int MK_MIME_NO_RECIPIENTS;
	extern int MK_MIME_NO_SENDER;
	extern int MK_MSG_COULDNT_OPEN_FCC_FILE;
	extern int MK_OUT_OF_MEMORY;
	extern int MK_UNABLE_TO_OPEN_TMP_FILE;
	extern int MK_MSG_CANT_CREATE_FOLDER;
	extern int MK_MSG_SAVE_DRAFT;
	extern int MK_ADDR_BOOK_CARD;
	extern int MK_MSG_MAC_PROMPT_UUENCODE;
	extern int MK_MSG_SAVING_AS_DRAFT;
	extern int MK_MSG_SAVING_AS_TEMPLATE;
	extern int MK_MSG_UNABLE_TO_SAVE_DRAFT;
	extern int MK_MSG_UNABLE_TO_SAVE_TEMPLATE;
	extern int MK_UNABLE_TO_OPEN_FILE;
	extern int MK_IMAP_UNABLE_TO_SAVE_MESSAGE;
	extern int MK_IMAP_NO_ONLINE_FOLDER;
  extern int MK_MSG_FAILED_COPY_OPERATION;
}

#ifdef XP_MAC
#include "m_cvstrm.h"
#endif

#define TEN_K 10240
#define MK_ATTACHMENT_LOAD_FAILED -666

/* Asynchronous mailing of messages with attached URLs.

   - If there are any attachments, start their URLs going, and write each
     of them to a temp file.

   - While writing to their files, examine the data going by and decide
     what kind of encoding, if any, they need.  Also remember their content
     types.

   - Once that URLs has been saved to a temp file (or, if there were no
     attachments) generate a final temp file, of the actual message:

	 -  Generate a string of the headers.
	 -  Open the final temp file.
	 -  Write the headers.
     -  Examine the first part, and decide whether to encode it.
	 -  Write the first part to the file, possibly encoded.
	 -  Write the second and subsequent parts to the file, possibly encoded.
        (Open the first temp file and copy it to the final temp file, and so
		on, through an encoding filter.)

     - Delete the attachment temp file(s) as we finish with them.
	 - Close the final temp file.
	 - Open the news: url.
	 - Send the final temp file to NNTP.
	   If there's an error, run the callback with "failure" status.
	 - If mail succeeded, open the mailto: url.
	 - Send the final temp file to SMTP.
	   If there's an error, run the callback with "failure" status.
	 - Otherwise, run the callback with "success" status.
	 - Free everything, delete the final temp file.

  The theory behind the encoding logic:
  =====================================

  If the document is of type text/html, and the user has asked to attach it
  as source or postscript, it will be run through the appropriate converter
  (which will result in a document of type text/plain.)

  An attachment will be encoded if:

   - it is of a non-text type (in which case we will use base64); or
   - The "use QP" option has been selected and high-bit characters exist; or
   - any NULLs exist in the document; or
   - any line is longer than 900 bytes.

   - If we are encoding, and more than 10% of the document consists of
     non-ASCII characters, then we always use base64 instead of QP.

  We eschew quoted-printable in favor of base64 for documents which are likely
  to always be binary (images, sound) because, on the off chance that a GIF
  file (for example) might contain primarily bytes in the ASCII range, using
  the quoted-printable representation might cause corruption due to the
  translation of CR or LF to CRLF.  So, when we don't know that the document
  has "lines", we don't use quoted-printable.
 */


/* It's better to send a message as news before sending it as mail, because
   the NNTP server is more likely to reject the article (for any number of
   reasons) than the SMTP server is. */
#undef MAIL_BEFORE_NEWS

/* Generating a message ID here is a good because it means that if a message
   is sent to both mail and news, it will have the same ID in both places. */
#define GENERATE_MESSAGE_ID


/* For maximal compatibility, it helps to emit both
      Content-Type: <type>; name="<original-file-name>"
   as well as
      Content-Disposition: inline; filename="<original-file-name>"

  The lossage here is, RFC1341 defined the "name" parameter to Content-Type,
  but then RFC1521 deprecated it in anticipation of RFC1806, which defines
  Content-Type and the "filename" parameter.  But, RFC1521 is "Standards Track"
  while RFC1806 is still "Experimental."  So, it's probably best to just
  implement both.
 */
#define EMIT_NAME_IN_CONTENT_TYPE


/* Whether the contents of the BCC header should be preserved in the FCC'ed
   copy of a message.  See comments below, in mime_do_fcc_1().
 */
#define SAVE_BCC_IN_FCC_FILE


/* When attaching an HTML document, one must indicate the original URL of
   that document, if the receiver is to have any chance of being able to
   retreive and display the inline images, or to click on any links in the
   HTML.

   The way we have done this in the past is by inserting a <BASE> tag as the
   first line of all HTML documents we attach.  (This is kind of bad in that
   we're actually modifying the document, and it really isn't our place to
   do that.)

   The sanctioned *new* way of doing this is to insert a Content-Base header
   field on the attachment.  This is (will be) a part of the forthcoming MHTML
   spec.

   If GENERATE_CONTENT_BASE, we generate a Content-Base header.

   We used to have a MANGLE_HTML_ATTACHMENTS_WITH_BASE_TAG symbol that we
   defined, which added a BASE tag to the bodies.  We stopped doing this in
   4.0.  */
#define GENERATE_CONTENT_BASE


static PRBool mime_use_quoted_printable_p = PR_TRUE;
static PRBool mime_headers_use_quoted_printable_p = PR_FALSE;
static const char* pSmtpServer = NULL;

#ifdef XP_MAC

XP_BEGIN_PROTOS
extern OSErr my_FSSpecFromPathname(char* src_filename, FSSpec* fspec);
extern void MSG_MimeNotifyCryptoAttachmentKludge(NET_StreamClass *stream);
XP_END_PROTOS


static char* NET_GetLocalFileFromURL(char *url)
{
	char * finalPath;
	NS_ASSERTION(PL_strncasecmp(url, "file://", 7) == 0, "invalid url");
	finalPath = (char*)PR_Malloc(strlen(url));
	if (finalPath == NULL)
		return NULL;
	strcpy(finalPath, url+6+1);
	return finalPath;
}

static char* NET_GetURLFromLocalFile(char *filename)
{
    /*  file:///<path>0 */
	char * finalPath = (char*)PR_Malloc(strlen(filename) + 8 + 1);
	if (finalPath == NULL)
		return NULL;
	finalPath[0] = 0;
	strcat(finalPath, "file://");
	strcat(finalPath, filename);
	return finalPath;
}

#endif /* XP_MAC */

#include "nsIPref.h"

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

char * WH_TempName(XP_FileType /*type*/, const char * prefix)
{
	nsresult res;
	nsString tempPath = "c:\\temp\\";

	#define PREF_LENGTH 128 
	char prefValue[PREF_LENGTH];
	PRInt32 prefLength = PREF_LENGTH;
	nsIPref* prefs;
	res = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
    if (prefs && NS_SUCCEEDED(res))
	{
		prefs->Startup("prefs.js");
		res = prefs->GetCharPref("browser.download_directory", prefValue, &prefLength);
		if (NS_SUCCEEDED(res) && prefLength > 0)
			tempPath = PL_strdup(prefValue);
		
		nsServiceManager::ReleaseService(kPrefCID, prefs);
	}
	
	if (tempPath.Last() != '\\')
		tempPath += '\\';

	tempPath += prefix;
	tempPath += "01.txt";	//JFD, need to be smarter than that!

	return tempPath.ToNewCString();
}

/* this function will be used by the factory to generate an Message Send Object....*/
nsresult NS_NewMsgSend(nsIMsgSend** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgSendMimeDeliveryState* pSend = new nsMsgSendMimeDeliveryState();
		if (pSend)
			return pSend->QueryInterface(kIMsgSend, (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

void MIME_ConformToStandard (PRBool conform_p)
{
	/* 
	* If we are conforming to mime standard no matter what we set
	* for the headers preference when generating mime headers we should 
	* also conform to the standard. Otherwise, depends the preference
	* we set. For now, the headers preference is not accessible from UI.
	*/
	if (conform_p)
		mime_headers_use_quoted_printable_p = PR_TRUE;
	else
		PREF_GetBoolPref("mail.strictly_mime_headers", 
					 &mime_headers_use_quoted_printable_p);
	mime_use_quoted_printable_p = conform_p;
}

/* for right now, only for the XFE */
PRBool Get_MIME_ConformToStandard()
{
  return mime_use_quoted_printable_p;
}

nsMsgSendMimeDeliveryState::nsMsgSendMimeDeliveryState()
{
	m_pane = NULL;		/* Pane to use when loading the URLs */
	m_fe_data = NULL;		/* passed in and passed to callback */
	m_fields = NULL;			/* Where to send the message once it's done */

	m_dont_deliver_p = PR_FALSE;
	m_deliver_mode = MSG_DeliverNow;

	m_attachments_only_p = PR_FALSE;
	m_pre_snarfed_attachments_p = PR_FALSE;
	m_digest_p = PR_FALSE;
	m_be_synchronous_p = PR_FALSE;
	m_crypto_closure = NULL;
	m_attachment1_type = 0;
	m_attachment1_encoding = 0;
	m_attachment1_encoder_data = NULL;
	m_attachment1_body = 0;
	m_attachment1_body_length = 0;
	m_attachment_count = 0;
	m_attachment_pending_count = 0;
	m_attachments = NULL;
	m_status = 0;
	m_message_delivery_done_callback = NULL;
	m_attachments_done_callback = NULL;
	m_msg_file_name = NULL;
	m_msg_file = NULL;
	m_plaintext = NULL;
	m_html_filename = NULL;
	m_related_part = NULL;
	m_imapFolderInfo = NULL;
	m_imapOutgoingParser = NULL;
	m_imapLocalMailDB = NULL;


	NS_INIT_REFCNT();
}

nsMsgSendMimeDeliveryState::~nsMsgSendMimeDeliveryState()
{
	Clear();
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgSendMimeDeliveryState, nsIMsgSend::GetIID());

nsresult nsMsgSendMimeDeliveryState::SendMessage(nsIMsgCompFields *fields, const char *smtp)
{
	const char* pBody;
	PRInt32 nBodyLength;

	pSmtpServer = smtp;

	if (fields) {
		pBody = ((nsMsgCompFields *)fields)->GetBody();
		if (pBody)
			nBodyLength = PL_strlen(pBody);
		else
			nBodyLength = 0;

		printf("SendMessage to: %s\n", ((nsMsgCompFields *)fields)->GetTo());
		printf("subject: %s\n", ((nsMsgCompFields *)fields)->GetSubject());
		printf("\n%s", pBody);

		StartMessageDelivery(NULL, NULL, (nsMsgCompFields *)fields, PR_FALSE, PR_FALSE,
			MSG_DeliverNow, TEXT_PLAIN, pBody, nBodyLength, 0, NULL, NULL, NULL);
	}
	return NS_OK;
}

static char *mime_mailto_stream_read_buffer = 0;
static char *mime_mailto_stream_write_buffer = 0;

static void mime_attachment_url_exit (URL_Struct *url, int status,
									  MWContext *context);
static void mime_text_attachment_url_exit (PrintSetup *p);
static int mime_sanity_check_fields (const char *from,
									 const char *reply_to,
									 const char *to,
									 const char *cc,
									 const char *bcc,
									 const char *fcc,
									 const char *newsgroups,
									 const char *followup_to,
									 const char *subject,
									 const char *references,
									 const char *organization,
									 const char *other_random_headers);
static char *mime_generate_headers (nsMsgCompFields *fields,
									const char *charset, 
									MSG_Deliver_Mode deliver_mode);
static char *mime_generate_attachment_headers (const char *type,
											   const char *encoding,
											   const char *description,
											   const char *x_mac_type,
											   const char *x_mac_creator,
											   const char *real_name,
											   const char *base_url,
											   PRBool digest_p,
											   MSG_DeliverMimeAttachment *ma,
											   const char *charset);
static char *RFC2231ParmFolding(const char *parmName, const char *charset,
								const char *language, const char *parmValue);
#if 0
static PRBool mime_type_conversion_possible (const char *from_type,
											  const char *to_type);
#endif

#ifdef XP_UNIX
extern "C" void XFE_InitializePrintSetup (PrintSetup *p);
#endif /* XP_UNIX */

/*JFD extern "C"*/ char * NET_ExplainErrorDetails (int code, ...);

MSG_DeliverMimeAttachment::MSG_DeliverMimeAttachment()
{
	m_url_string = NULL;
	m_url = NULL;
	m_done = PR_FALSE;
	m_type = NULL;
  m_charset = NULL;
	m_override_type = NULL;
	m_override_encoding = NULL;
	m_desired_type = NULL;
	m_description = NULL;
	m_x_mac_type = NULL;
	m_x_mac_creator = NULL;
	m_encoding = NULL;
	m_real_name = NULL;
	m_mime_delivery_state = NULL;
	m_encoding = NULL;
	m_already_encoded_p = PR_FALSE;
	m_file_name = NULL;
	m_file = 0;
#ifdef XP_MAC
	m_ap_filename = NULL;
#endif
	m_decrypted_p = PR_FALSE;
	m_size = 0;
	m_unprintable_count = 0;
	m_highbit_count = 0;
	m_ctl_count = 0;
	m_null_count = 0;
	m_current_column = 0;
	m_max_column = 0;
	m_lines = 0;

	m_encoder_data = NULL;

//JFD  memset(&m_print_setup, 0, sizeof(m_print_setup));
	m_graph_progress_started = PR_FALSE;
}

MSG_DeliverMimeAttachment::~MSG_DeliverMimeAttachment()
{
}

   
extern "C" char * mime_make_separator(const char *prefix)
{
	unsigned char rand_buf[13]; 
	RNG_GenerateGlobalRandomBytes((void *) rand_buf, 12);

	return PR_smprintf("------------%s"
					 "%02X%02X%02X%02X"
					 "%02X%02X%02X%02X"
					 "%02X%02X%02X%02X",
					 prefix,
					 rand_buf[0], rand_buf[1], rand_buf[2], rand_buf[3],
					 rand_buf[4], rand_buf[5], rand_buf[6], rand_buf[7],
					 rand_buf[8], rand_buf[9], rand_buf[10], rand_buf[11]);
}

PRBool MSG_DeliverMimeAttachment::UseUUEncode_p(void)
{
	PRBool returnVal = (m_mime_delivery_state) && 
	  					(m_mime_delivery_state->m_pane) &&
	  					((nsMsgCompose*)(m_mime_delivery_state->m_pane))->
				  		GetCompBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK);
	
	return returnVal;
}

static void msg_escape_file_name (URL_Struct *m_url) 
{
    NS_ASSERTION (m_url->address && !PL_strncasecmp(m_url->address, "file:", 5), "Invalid URL type");
	if (!m_url->address || PL_strncasecmp(m_url->address, "file:", 5))
		return;

	char * new_address = NET_Escape(PL_strchr(m_url->address, ':') + 1, URL_PATH);
	NS_ASSERTION(new_address, "null ptr");
	if (!new_address)
		return;

	PR_FREEIF(m_url->address);
	m_url->address = PR_smprintf("file:%s", new_address);
	PR_FREEIF(new_address);
}

// Returns a newly-allocated string containing the MIME type to be used for
// outgoing attachments of the given document type.  The way this is determined
// will be platform-specific, based on the real filename of the file (i.e. not the temp filename)
// and some Mac creator info.
// If there is no default specified in the prefs, then this returns NULL.
static char *msg_GetMissionControlledOutgoingMIMEType(const char *filename,
													  const char *x_mac_type,
													  const char *x_mac_creator)
{
	if (!filename)
		return NULL;

#ifdef XP_WIN
	char *whereDot = PL_strrchr(filename, '.');
	if (whereDot)
	{
		char *extension = whereDot + 1;
		if (extension)
		{
			char *mcOutgoingMimeType = NULL;
			char *prefString = PR_smprintf("mime.table.extension.%s.outgoing_default_type",extension);

			if (prefString)
			{
				PREF_CopyCharPref(prefString, &mcOutgoingMimeType);
				PR_Free(prefString);
			}
			return mcOutgoingMimeType;
		}
		else
			return NULL;	// no file extension
	}
	else
		return NULL;	// no file extension
	
#endif

	return NULL;
}


static char *
RFC2231ParmFolding(const char *parmName, const char *charset, 
				   const char *language, const char *parmValue)
{
#define PR_MAX_FOLDING_LEN 75			// this is to gurantee the folded line will
								// never be greater than 78 = 75 + CRLFLWSP
	char *foldedParm = NULL;
	char *dupParm = NULL;
	PRInt32 parmNameLen = 0;
	PRInt32 parmValueLen = 0;
	PRInt32 charsetLen = 0;
	PRInt32 languageLen = 0;
	PRBool needEscape = PR_FALSE;

	NS_ASSERTION(parmName && *parmName && parmValue && *parmValue, "null parameters");
	if (!parmName || !*parmName || !parmValue || !*parmValue)
		return NULL;
	if ((charset && *charset && PL_strcasecmp(charset, "us-ascii") != 0) || 
		(language && *language && PL_strcasecmp(language, "en") != 0 &&
		 PL_strcasecmp(language, "en-us") != 0))
		needEscape = PR_TRUE;

	if (needEscape)
		dupParm = NET_Escape(parmValue, URL_PATH);
	else 
		dupParm = PL_strdup(parmValue);

	if (!dupParm)
		return NULL;

	if (needEscape)
	{
		parmValueLen = PL_strlen(dupParm);
		parmNameLen = PL_strlen(parmName);
	}

	if (needEscape)
		parmNameLen += 5;		// *=__'__'___ or *[0]*=__'__'__ or *[1]*=___
	else
		parmNameLen += 5;		// *[0]="___";
	charsetLen = charset ? PL_strlen(charset) : 0;
	languageLen = language ? PL_strlen(language) : 0;

	if ((parmValueLen + parmNameLen + charsetLen + languageLen) <
		PR_MAX_FOLDING_LEN)
	{
		foldedParm = PL_strdup(parmName);
		if (needEscape)
		{
			StrAllocCat(foldedParm, "*=");
			if (charsetLen)
				StrAllocCat(foldedParm, charset);
			StrAllocCat(foldedParm, "'");
			if (languageLen)
				StrAllocCat(foldedParm, language);
			StrAllocCat(foldedParm, "'");
		}
		else
			StrAllocCat(foldedParm, "=\"");
		StrAllocCat(foldedParm, dupParm);
		if (!needEscape)
			StrAllocCat(foldedParm, "\"");
		goto done;
	}
	else
	{
		int curLineLen = 0;
		int counter = 0;
		char digits[32];
		char *start = dupParm;
		char *end = NULL;
		char tmp = 0;

		while (parmValueLen > 0)
		{
			curLineLen = 0;
			if (counter == 0) {
				PR_FREEIF(foldedParm)
				foldedParm = PL_strdup(parmName);
			}
			else {
				if (needEscape)
					StrAllocCat(foldedParm, "\r\n ");
				else
					StrAllocCat(foldedParm, ";\r\n ");
				StrAllocCat(foldedParm, parmName);
			}
			XP_SPRINTF(digits, "*%d", counter);
			StrAllocCat(foldedParm, digits);
			curLineLen += PL_strlen(digits);
			if (needEscape)
			{
				StrAllocCat(foldedParm, "*=");
				if (counter == 0)
				{
					if (charsetLen)
						StrAllocCat(foldedParm, charset);
					StrAllocCat(foldedParm, "'");
					if (languageLen)
						StrAllocCat(foldedParm, language);
					StrAllocCat (foldedParm, "'");
					curLineLen += charsetLen;
					curLineLen += languageLen;
				}
			}
			else
			{
				StrAllocCat(foldedParm, "=\"");
			}
			counter++;
			curLineLen += parmNameLen;
			if (parmValueLen <= PR_MAX_FOLDING_LEN - curLineLen)
				end = start + parmValueLen;
			else
				end = start + (PR_MAX_FOLDING_LEN - curLineLen);

			tmp = 0;
			if (*end && needEscape)
			{
				// check to see if we are in the middle of escaped char
				if (*end == '%')
				{
					tmp = '%'; *end = nsnull;
				}
				else if (end-1 > start && *(end-1) == '%')
				{
					end -= 1; tmp = '%'; *end = nsnull;
				}
				else if (end-2 > start && *(end-2) == '%')
				{
					end -= 2; tmp = '%'; *end = nsnull;
				}
				else
				{
					tmp = *end; *end = nsnull;
				}
			}
			else
			{
				tmp = *end; *end = nsnull;
			}
			StrAllocCat(foldedParm, start);
			if (!needEscape)
				StrAllocCat(foldedParm, "\"");

			parmValueLen -= (end-start);
			if (tmp)
				*end = tmp;
			start = end;
		}
	}

done:
	PR_FREEIF(dupParm);
	return foldedParm;
}

PRInt32
MSG_DeliverMimeAttachment::SnarfAttachment ()
{
  PRInt32 status = 0;
  NS_ASSERTION (! m_done, "Already done");

  m_file_name = WH_TempName (xpFileToPost, "nsmail");
  if (! m_file_name)
	return (MK_OUT_OF_MEMORY);

  m_file = XP_FileOpen (m_file_name, xpFileToPost, XP_FILE_WRITE_BIN);
  if (! m_file)
	return MK_UNABLE_TO_OPEN_TMP_FILE; /* #### how do we pass file name? */

  m_url->fe_data = this;

  /* #### m_type is still unknown at this point.
	 We *really* need to find a way to make the textfe not blow
	 up on documents that are not text/html!!
   */
   
#ifdef XP_MAC
  if (NET_IsLocalFileURL(m_url->address) &&	// do we need to add IMAP: to this list? NET_IsLocalFileURL returns PR_FALSE always for IMAP - DMB
	  (PL_strncasecmp(m_url->address, "mailbox:", 8) != 0))
	{
	  /* convert the apple file to AppleDouble first, and then patch the
		 address in the url.
	   */
	  char* src_filename = NET_GetLocalFileFromURL (m_url->address);

		// ### mwelch Only use appledouble if we aren't uuencoding.
	  if(/*JFD isMacFile(src_filename) && */ (! UseUUEncode_p()))
		{

		  char	*separator, tmp[128];
		  NET_StreamClass *ad_encode_stream;

		  separator = mime_make_separator("ad");
		  if (!separator)
			return MK_OUT_OF_MEMORY;
						
		  m_ap_filename  = WH_TempName (xpFileToPost, "nsmail");

		  ad_encode_stream = (NET_StreamClass *)		/* need a prototype */
NULL; /* JFD			fe_MakeAppleDoubleEncodeStream (FO_CACHE_AND_MAIL_TO,
											(void*)NULL,
											m_url,
											m_mime_delivery_state->GetContext(),
											src_filename,
											m_ap_filename,
											separator,
											m_real_name);
JFD */

		  if (ad_encode_stream == NULL)
			{
			  PR_FREEIF(separator);
			  return MK_OUT_OF_MEMORY;
			}

		  do {
			status = (*ad_encode_stream->put_block)
			  ((NET_StreamClass *)ad_encode_stream->data_object, NULL, 1024);
		  } while (status == noErr);

		  if (status >= 0)
			ad_encode_stream->complete ((NET_StreamClass *)ad_encode_stream->data_object);
		  else
			ad_encode_stream->abort ((NET_StreamClass *)ad_encode_stream->data_object, status);

		  PR_Free(ad_encode_stream);

		  if (status < 0)
			{
			  PR_FREEIF(separator);
			  return status;
			}

		  PR_Free(m_url->address);
		  {
		    char * temp = WH_FileName(m_ap_filename, xpFileToPost );
		    m_url->address = XP_PlatformFileToURL(temp); // jrm 97/02/08
			if (temp)
				PR_Free(temp);
		  }
		  /* and also patch the types.
		   */

		  XP_SPRINTF(tmp, MULTIPART_APPLEDOUBLE ";\r\n boundary=\"%s\"",
					 separator);

		  PR_FREEIF(separator);

		PR_FREEIF (m_type);
		m_type = PL_strdup(tmp);
		}
	  else
		{
//JFD			if (isMacFile(src_filename))
			{
				// The only time we want to send just the data fork of a two-fork
				// Mac file is if uuencoding has been requested.
				NS_ASSERTION(UseUUEncode_p(), "not UseUUEncode_p");
				if (!((nsMsgCompose *) m_mime_delivery_state->m_pane)->m_confirmed_uuencode_p)
				{
					PRBool confirmed = FE_Confirm(m_mime_delivery_state->m_pane->GetContext(), 
													XP_GetString(MK_MSG_MAC_PROMPT_UUENCODE)); 

					// only want to do this once
					((nsMsgCompose *) m_mime_delivery_state->m_pane)->m_confirmed_uuencode_p = PR_TRUE;
					
					if (! confirmed) // cancelled
						return MK_INTERRUPTED;
				}
			}
		  /* make sure the file type and create are set.	*/
		  char filetype[32];
		  FSSpec fsSpec;
		  FInfo info;
		  Bool 	useDefault;
		  char	*macType, *macEncoding;

/*JFD
		  my_FSSpecFromPathname(src_filename, &fsSpec);
*/
		  if (FSpGetFInfo (&fsSpec, &info) == noErr)
			{
			  XP_SPRINTF(filetype, "%X", info.fdType);
			  PR_FREEIF(m_x_mac_type);
			  m_x_mac_type    = PL_strdup(filetype);

			  XP_SPRINTF(filetype, "%X", info.fdCreator);
			  PR_FREEIF(m_x_mac_creator);
			  m_x_mac_creator = PL_strdup(filetype);
			  if (m_type == NULL ||
				  !PL_strcasecmp (m_type, TEXT_PLAIN))
				{
# define TEXT_TYPE	0x54455854  /* the characters 'T' 'E' 'X' 'T' */
# define text_TYPE	0x74657874  /* the characters 't' 'e' 'x' 't' */

				  if (info.fdType != TEXT_TYPE && info.fdType != text_TYPE)
					{
/*JFD
					  FE_FileType(m_url->address, &useDefault,
								  &macType, &macEncoding);
*/

					  PR_FREEIF(m_type);
					  m_type = macType;
					}
				}
			}

		  /* don't bother to set the types if we failed in getting the file
			 info. */
		}
	  PR_FREEIF(src_filename);
	  src_filename = 0;
	}
#else

  /* if we are attaching a local file make sure the file name are escaped
   * properly
   */
  if (NET_IsLocalFileURL(m_url->address) &&
	  PL_strncasecmp (m_url->address, "file:", 5) == 0) 
  {
	  msg_escape_file_name(m_url);
  }

#endif /* XP_MAC */

  if (m_desired_type &&
	  !PL_strcasecmp (m_desired_type, TEXT_PLAIN) /* #### &&
	  mime_type_conversion_possible (m_type, m_desired_type) */ )
	{
	  /* Conversion to plain text desired.
	   */
/*JFD
	  m_print_setup.url = m_url;
	  m_print_setup.carg = this;
	  m_print_setup.completion = mime_text_attachment_url_exit;
	  m_print_setup.filename = NULL;
	  m_print_setup.out = m_file;
	  m_print_setup.eol = CRLF;
JFD */
	  PRInt32 width = 72;
	  PREF_GetIntPref("mailnews.wraplength", &width);
	  if (width == 0) width = 72;
	  else if (width < 10) width = 10;
	  else if (width > 30000) width = 30000;

	  if (m_mime_delivery_state->m_pane->GetPaneType() == MSG_COMPOSITIONPANE)
	  {
		  int lineWidth = ((nsMsgCompose *) m_mime_delivery_state->m_pane)
			  ->GetLineWidth();
		  if (lineWidth > width)
			  width = lineWidth;
	  }
/*JFD
	  m_print_setup.width = width;

	  m_url->savedData.FormList = 0;
#ifdef _USRDLL
	  if (! NDLLXL_TranslateText (m_mime_delivery_state->GetContext(), m_url,
							&m_print_setup))
		return MK_ATTACHMENT_LOAD_FAILED;
#else
	  if (! XL_TranslateText (m_mime_delivery_state->GetContext(), m_url,
						&m_print_setup))
		return MK_ATTACHMENT_LOAD_FAILED;
#endif
JFD */
	  if (m_type) PR_Free (m_type);
	  m_type = m_desired_type;
	  m_desired_type = 0;
	  if (m_encoding) PR_Free (m_encoding);
	  m_encoding = 0;
	}
/* this used to be XP_UNIX? */
#if 0
  else if (m_desired_type &&
		   !PL_strcasecmp (m_desired_type, APPLICATION_POSTSCRIPT) /* #### &&
		   mime_type_conversion_possible (m_type, m_desired_type) */ )
	{
      SHIST_SavedData saved_data;

      /* Make sure layout saves the current state of form elements. */
      LO_SaveFormData(m_mime_delivery_state->GetContext());

      /* Hold on to the saved data. */
      memcpy(&saved_data, &m_url->savedData, sizeof(SHIST_SavedData));

	  /* Conversion to postscript desired.
	   */
/*JFD
	  XFE_InitializePrintSetup (&m_print_setup);
	  m_print_setup.url = m_url;
	  m_print_setup.carg = this;
	  m_print_setup.completion = mime_text_attachment_url_exit;
	  m_print_setup.filename = NULL;
	  m_print_setup.out = m_file;
	  m_print_setup.eol = CRLF;
      memset (&m_url->savedData, 0, sizeof (SHIST_SavedData));
	  XL_TranslatePostscript (m_mime_delivery_state->GetContext(),
							  m_url, &saved_data,
							  &m_print_setup);
JFD*/
	  if (m_type) PR_Free (m_type);
	  m_type = m_desired_type;
	  m_desired_type = 0;
	  if (m_encoding) PR_Free (m_encoding);
	  m_encoding = 0;
	}
#endif /* XP_UNIX */
  else
	{
	  m_url->allow_content_change = PR_FALSE;	// don't use modified content

      if (NET_URL_Type (m_url->address) == NEWS_TYPE_URL && !m_url->msg_pane)
		  m_url->msg_pane = m_mime_delivery_state->m_pane;

/*JFD
		  MSG_UrlQueue::AddUrlToPane(m_url, mime_attachment_url_exit,
								 m_mime_delivery_state->m_pane, PR_TRUE,
								 FO_CACHE_AND_MAIL_TO);
JFD*/
	  return 0;
	}
  return status;
}


static void
mime_attachment_url_exit (URL_Struct *url, int status, MWContext *context)
{
  MSG_DeliverMimeAttachment *ma = (MSG_DeliverMimeAttachment *) url->fe_data;
  NS_ASSERTION(ma != NULL, "not-null mime attachment");
  if (ma != NULL)
	ma->UrlExit(url, status, context);
}


void MSG_DeliverMimeAttachment::UrlExit(URL_Struct *url, int status,
										MWContext *context)
{
  char *error_msg = url->error_msg;
  url->error_msg = 0;
  url->fe_data = 0;

  NS_ASSERTION(m_mime_delivery_state != NULL, "not-null m_mime_delivery_state");
  NS_ASSERTION(m_mime_delivery_state->GetContext() != NULL, "not-null context");
  NS_ASSERTION(m_url != NULL, "not-null m_url");

  if (m_graph_progress_started)
	{
	  m_graph_progress_started = PR_FALSE;
	  FE_GraphProgressDestroy (m_mime_delivery_state->GetContext(), m_url,
							   m_url->content_length, m_size);
	}

  if (status < 0)
	/* If any of the attachment URLs fail, kill them all. */
	NET_InterruptWindow (context);

  /* Close the file, but don't delete it (or the file name.) */
  XP_FileClose (m_file);
  m_file = 0;
  NET_FreeURLStruct (m_url);
  /* I'm pretty sure m_url == url */
  m_url = 0;
  url = 0;



  if (status < 0) {
	  if (m_mime_delivery_state->m_status >= 0)
		m_mime_delivery_state->m_status = status;
	  XP_FileRemove(m_file_name, xpFileToPost);
	  PR_FREEIF(m_file_name);
  }

  m_done = PR_TRUE;

  NS_ASSERTION (m_mime_delivery_state->m_attachment_pending_count > 0, "no more pending attachment");
  m_mime_delivery_state->m_attachment_pending_count--;

  if (status >= 0 && m_mime_delivery_state->m_be_synchronous_p)
	{
	  /* Find the next attachment which has not yet been loaded,
		 if any, and start it going.
	   */
	  PRInt32 i;
	  MSG_DeliverMimeAttachment *next = 0;
	  for (i = 0; i < m_mime_delivery_state->m_attachment_count; i++)
		if (!m_mime_delivery_state->m_attachments[i].m_done)
		  {
			next = &m_mime_delivery_state->m_attachments[i];
			break;
		  }
	  if (next)
		{
		  int status = next->SnarfAttachment ();
		  if (status < 0)
			{
			  m_mime_delivery_state->Fail(status, 0);
			  return;
			}
		}
	}

  if (m_mime_delivery_state->m_attachment_pending_count == 0)
	{
	  /* If this is the last attachment, then either complete the
		 delivery (if successful) or report the error by calling
		 the exit routine and terminating the delivery.
	   */
	  if (status < 0)
		{
		  m_mime_delivery_state->Fail(status, error_msg);
		  error_msg = 0;
		}
	  else
		{
		  m_mime_delivery_state->GatherMimeAttachments ();
		}
	}
  else
	{
	  /* If this is not the last attachment, but it got an error,
		 then report that error and continue (we won't actually
		 abort the delivery until all the other pending URLs have
		 caught up with the NET_InterruptWindow() we did up above.)
	  if (status < 0 && error_msg)
		FE_Alert (context, error_msg);
	  */
		
	  if (status < 0)
		{
		  m_mime_delivery_state->Fail(status, error_msg);
		  error_msg = 0;
		}
	}
  PR_FREEIF (error_msg);
}

void
MSG_DeliverMimeAttachment::AnalyzeDataChunk(const char *chunk, PRInt32 length)
{
  unsigned char *s = (unsigned char *) chunk;
  unsigned char *end = s + length;
  for (; s < end; s++)
	{
	  if (*s > 126)
		{
		  m_highbit_count++;
		  m_unprintable_count++;
		}
	  else if (*s < ' ' && *s != '\t' && *s != CR && *s != LF)
		{
		  m_unprintable_count++;
		  m_ctl_count++;
		  if (*s == 0)
			m_null_count++;
		}

	  if (*s == CR || *s == LF)
		{
		  if (s+1 < end && s[0] == CR && s[1] == LF)
			s++;
		  if (m_max_column < m_current_column)
			m_max_column = m_current_column;
		  m_current_column = 0;
		  m_lines++;
		}
	  else
		{
		  m_current_column++;
		}
	}
}

void
MSG_DeliverMimeAttachment::AnalyzeSnarfedFile(void)
{
	char chunk[256];
	XP_File fileHdl = NULL;
	PRInt32 numRead = 0;
	
	if (m_file_name && *m_file_name)
	{
		fileHdl = XP_FileOpen(m_file_name, xpFileToPost, XP_FILE_READ_BIN);
		if (fileHdl)
		{
			do
			{
				numRead = XP_FileRead(chunk, 256, fileHdl);
				if (numRead > 0)
					AnalyzeDataChunk(chunk, numRead);
			}
			while (numRead > 0);
			XP_FileClose(fileHdl);
		}
	}
}

static void
mime_text_attachment_url_exit (PrintSetup *p)
{
#if 0 //JFD
  MSG_DeliverMimeAttachment *ma = (MSG_DeliverMimeAttachment *) p->carg;
  NS_ASSERTION (p->url == ma->m_url, "different url");
  ma->m_url->fe_data = ma;  /* grr */
  mime_attachment_url_exit (p->url, p->status,
							ma->m_mime_delivery_state->GetContext());
#endif //JFD
}


PRIVATE unsigned int
mime_attachment_stream_write_ready (void *stream)
{	
  return MAX_WRITE_READY;
}

PRIVATE int
mime_attachment_stream_write (void *stream, const char *block, PRInt32 length)
{
  MSG_DeliverMimeAttachment *ma = (MSG_DeliverMimeAttachment *) stream;
  /*
  const unsigned char *s;
  const unsigned char *end;
  */  

  if (ma->m_mime_delivery_state->m_status < 0)
	return ma->m_mime_delivery_state->m_status;

  ma->m_size += length;

  if (!ma->m_graph_progress_started)
	{
	  ma->m_graph_progress_started = PR_TRUE;
	  FE_GraphProgressInit (ma->m_mime_delivery_state->GetContext(), ma->m_url,
							ma->m_url->content_length);
	}

  FE_GraphProgress (ma->m_mime_delivery_state->GetContext(), ma->m_url,
					ma->m_size, length, ma->m_url->content_length);


  /* Copy out the content type and encoding if we haven't already.
   */
  if (!ma->m_type && ma->m_url->content_type)
	{
	  ma->m_type = PL_strdup (ma->m_url->content_type);

	  /* If the URL has an encoding, and it's not one of the "null" encodings,
		 then keep it. */
	  if (ma->m_url->content_encoding &&
		  PL_strcasecmp (ma->m_url->content_encoding, ENCODING_7BIT) &&
		  PL_strcasecmp (ma->m_url->content_encoding, ENCODING_8BIT) &&
		  PL_strcasecmp (ma->m_url->content_encoding, ENCODING_BINARY))
		{
		  PR_FREEIF (ma->m_encoding);
		  ma->m_encoding = PL_strdup (ma->m_url->content_encoding);
		  ma->m_already_encoded_p = PR_TRUE;
		}

	  /* Make sure there's a string in the type field.
		 Note that UNKNOWN_CONTENT_TYPE and APPLICATION_OCTET_STREAM are
		 different; "octet-stream" means that this document was *specified*
		 as an anonymous binary type; "unknown" means that we will guess
		 whether it is text or binary based on its contents.
	   */
	  if (!ma->m_type || !*ma->m_type) {
		PR_FREEIF(ma->m_type);
		ma->m_type = PL_strdup(UNKNOWN_CONTENT_TYPE);
	  }


#if defined(XP_WIN) || defined(XP_OS2)
	  /*  WinFE tends to spew out bogus internal "zz-" types for things
		 it doesn't know, so map those to the "real" unknown type.
	   */
	  if (ma->m_type && !PL_strncasecmp (ma->m_type, "zz-", 3)) {
		PR_FREEIF(ma->m_type);
		ma->m_type = PL_strdup(UNKNOWN_CONTENT_TYPE);
	  }
#endif /* XP_WIN */

	  /* There are some of "magnus" types in the default
		 mime.types file that SGI ships in /usr/local/lib/netscape/.  These
		 types are meaningless to the end user, and the server never returns
		 them, but they're getting attached to local .cgi files anyway.
		 Remove them.
	   */
	  if (ma->m_type && !PL_strncasecmp (ma->m_type, "magnus-internal/", 16)) {
		PR_FREEIF(ma->m_type);
		ma->m_type = PL_strdup (UNKNOWN_CONTENT_TYPE);
	  }


	  /* #### COMPLETE HORRIFIC KLUDGE
		 Unfortunately, the URL_Struct shares the `encoding' slot
		 amongst the Content-Encoding and Content-Transfer-Encoding headers.
		 Content-Transfer-Encoding is required to be one of the standard
		 MIME encodings (x- types are explicitly discourgaged.)  But
		 Content-Encoding can be anything (it's HTTP, not MIME.)

		 So, to prevent binary compressed data from getting dumped into the
		 mail stream, we special case some things here.  If the encoding is
		 "x-compress" or "x-gzip", then that must have come from a
		 Content-Encoding header, So change the type to application/x-compress
		 and allow it to be encoded in base64.

		 But what if it's something we don't know?  In that case, we just
		 dump it into the mail.  For Content-Transfer-Encodings, like for
		 example, x-uuencode, that's appropriate.  But for Content-Encodings,
		 like for example, x-some-brand-new-binary-compression-algorithm,
		 that's wrong.
	   */
	  if (ma->m_encoding &&
		  (!PL_strcasecmp (ma->m_encoding, ENCODING_COMPRESS) ||
		   !PL_strcasecmp (ma->m_encoding, ENCODING_COMPRESS2)))
		{
		  PR_FREEIF(ma->m_type);
		  ma->m_type = PL_strdup(APPLICATION_COMPRESS);
		  PR_FREEIF(ma->m_encoding);
		  ma->m_encoding = PL_strdup(ENCODING_BINARY);
		  ma->m_already_encoded_p = PR_FALSE;
		}
	  else if (ma->m_encoding &&
			   (!PL_strcasecmp (ma->m_encoding, ENCODING_GZIP) ||
				!PL_strcasecmp (ma->m_encoding, ENCODING_GZIP2)))
		{
		  PR_FREEIF(ma->m_type);
		  ma->m_type = PL_strdup (APPLICATION_GZIP);
		  PR_FREEIF(ma->m_encoding);
		  ma->m_encoding = PL_strdup (ENCODING_BINARY);
		  ma->m_already_encoded_p = PR_FALSE;
		}

	  /* If the caller has passed in an overriding type for this URL,
		 then ignore what the netlib guessed it to be.  This is so that
		 we can hand it a file:/some/tmp/file and tell it that it's of
		 type message/rfc822 without having to depend on that tmp file
		 having some particular extension.
	   */
	  if (ma->m_override_type)
		{
		  PR_FREEIF(ma->m_type);
		  ma->m_type = PL_strdup (ma->m_override_type);
		  if (ma->m_override_encoding) {
		    PR_FREEIF(ma->m_encoding);
			ma->m_encoding = PL_strdup (ma->m_override_encoding);
		  }
		}

	  char *mcType = msg_GetMissionControlledOutgoingMIMEType(ma->m_real_name, ma->m_x_mac_type, ma->m_x_mac_creator);	// returns an allocated string
	  if (mcType)
	  {
		  PR_FREEIF(ma->m_type);
		  ma->m_type = mcType;
	  }

	}

  /* Cumulatively examine the data that is passing through this stream,
	 building up a histogram that will be used when deciding which encoding
	 (if any) should be used.
   */
  ma->AnalyzeDataChunk(block, length); /* calling this instead of the previous 20ish lines */

  /* Write it to the file.
   */
  while (length > 0)
	{
	  PRInt32 l;
	  l = XP_FileWrite (block, length, ma->m_file);
	  if (l < length)
		{
		  ma->m_mime_delivery_state->m_status = MK_MIME_ERROR_WRITING_FILE;
		  return ma->m_mime_delivery_state->m_status;
		}
	  block += l;
	  length -= l;
	}

  return 1;
}


PRIVATE void
mime_attachment_stream_complete (void *stream)
{	
  /* Nothing to do here - the URL exit method does our cleanup. */
}

PRIVATE void
mime_attachment_stream_abort (void *stream, int status)
{
  MSG_DeliverMimeAttachment *ma = (MSG_DeliverMimeAttachment *) stream;  

  if (ma->m_mime_delivery_state->m_status >= 0)
	ma->m_mime_delivery_state->m_status = status;

  /* Nothing else to do here - the URL exit method does our cleanup. */
}


#ifdef XP_OS2
//DSR040297 - OK, I know this looks pretty awful... but, the Visual Age compiler is mega type
//strict when it comes to function pointers & the like... So this must be extern & extern "C" to match.
XP_BEGIN_PROTOS
extern NET_StreamClass *
mime_make_attachment_stream (int /*format_out*/, void * /*closure*/,
							 URL_Struct *url, MWContext *context);
XP_END_PROTOS
extern NET_StreamClass *
#else
static NET_StreamClass *
#endif
mime_make_attachment_stream (int /*format_out*/, void * /*closure*/,
							 URL_Struct *url, MWContext *context)
{
  NET_StreamClass *stream;

  TRACEMSG(("Setting up attachment stream. Have URL: %s\n", url->address));

  stream = PR_NEW (NET_StreamClass);
  if (stream == NULL)
	return (NULL);

  memset (stream, 0, sizeof (NET_StreamClass));

/*JFD
  stream->name           = "attachment stream";
  stream->complete       = mime_attachment_stream_complete;
  stream->abort          = mime_attachment_stream_abort;
  stream->put_block      = mime_attachment_stream_write;
  stream->is_write_ready = mime_attachment_stream_write_ready;
  stream->data_object    = url->fe_data;
  stream->window_id      = context;
*/
  TRACEMSG(("Returning stream from mime_make_attachment_stream"));

  return stream;
}


/* This is how libmime/mimemoz.c lets us know that the message was decrypted.
   Oh, the humanity. */
extern "C" void
MSG_MimeNotifyCryptoAttachmentKludge(NET_StreamClass *stream)
{
  MSG_DeliverMimeAttachment *ma;
  if (!stream || !stream->data_object)
	{
	  NS_ASSERTION(0, "invalid stream or stream object");
	  return;
	}

  ma = (MSG_DeliverMimeAttachment *) stream->data_object;

  /* I'm nervous -- do some checks to make sure this void* really smells
	 like a mime_attachment struct. */
  if (!ma->m_mime_delivery_state ||
	  !ma->m_mime_delivery_state->m_pane ||
	  !ma->m_mime_delivery_state->m_pane->GetContext() ||
	   ma->m_mime_delivery_state->m_pane->GetContext() != stream->window_id)
	{
	  NS_ASSERTION(0, "invalid mime delivery state");
	  return;
	}

  /* Well ok then. */
  ma->m_decrypted_p = PR_TRUE;
  HJ97347
}


void
MSG_RegisterConverters (void)
{
#if 0 //JFD
	NET_RegisterContentTypeConverter ("*", FO_MAIL_TO,
									NULL, mime_make_attachment_stream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_TO,
									NULL, mime_make_attachment_stream);

  /* FO_MAIL_MESSAGE_TO is treated the same as FO_MAIL_TO -- this format_out
	 just means that libmime has already gotten its hands on this document
	 (which happens to be of type message/rfc822 or message/news) and has
	 altered it in some way (for example, has decrypted it.) */
  NET_RegisterContentTypeConverter ("*", FO_MAIL_MESSAGE_TO,
									NULL, mime_make_attachment_stream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_MESSAGE_TO,
									NULL, mime_make_attachment_stream);

  /* Attachment of mail and news messages happens slightly differently:
	 Rather than FO_MAIL_TO going in to mime_make_attachment_stream, it
	 goes into MIME_MessageConverter, which will then open a later stream
	 with FO_MAIL_MESSAGE_TO -- which is how it eventually gets into
	 mime_make_attachment_stream, after having been frobbed by libmime.
   */
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_MAIL_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_MAIL_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_CACHE_AND_MAIL_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_CACHE_AND_MAIL_TO,
									NULL, MIME_MessageConverter);

  /* for saving news messages */
  NET_RegisterContentTypeConverter ("*", FO_APPEND_TO_FOLDER,
                                    NULL, msg_MakeAppendToFolderStream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_APPEND_TO_FOLDER,
                                    NULL, msg_MakeAppendToFolderStream);

  /* Decoders from mimehtml.c for message/rfc822 */
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_PRESENT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_PRINT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_EMBED,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_QUOTE_MESSAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_QUOTE_HTML_MESSAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_SAVE_AS,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_SAVE_AS_TEXT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_INTERNAL_IMAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_FONT,
									NULL, MIME_MessageConverter);
  
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_CMDLINE_ATTACHMENTS,
									NULL, MIME_ToDraftConverter);

  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_OPEN_DRAFT,
									NULL, MIME_ToDraftConverter);
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_SAVE_AS_POSTSCRIPT,
									NULL, MIME_MessageConverter);
#endif /* XP_UNIX */

  /* Decoders from mimehtml.c for message/news (same as message/rfc822) */
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_PRESENT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_PRINT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_EMBED,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_QUOTE_MESSAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_QUOTE_HTML_MESSAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_SAVE_AS,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_SAVE_AS_TEXT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_INTERNAL_IMAGE,
									NULL, MIME_MessageConverter);

  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_OPEN_DRAFT,
									NULL, MIME_ToDraftConverter);
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_SAVE_AS_POSTSCRIPT,
									NULL, MIME_MessageConverter);
#endif /* XP_UNIX */

  /* Decoders from mimehtml.c for text/richtext and text/enriched */
  NET_RegisterContentTypeConverter (TEXT_RICHTEXT, FO_PRESENT,
									NULL, MIME_RichtextConverter);
  NET_RegisterContentTypeConverter (TEXT_RICHTEXT, FO_PRINT,
									NULL, MIME_RichtextConverter);
  NET_RegisterContentTypeConverter (TEXT_ENRICHED, FO_PRESENT,
									NULL, MIME_EnrichedTextConverter);
  NET_RegisterContentTypeConverter (TEXT_ENRICHED, FO_PRINT,
									NULL, MIME_EnrichedTextConverter);

  /* Decoders from mimevcrd.c for text/x-vcard */
  NET_RegisterContentTypeConverter (TEXT_VCARD, FO_PRESENT,
									NULL, MIME_VCardConverter);
  NET_RegisterContentTypeConverter (TEXT_VCARD, FO_PRINT,
									NULL, MIME_VCardConverter);

  /* Decoders from mimevcrd.c for application/directory (a SYNONYM for text/x-vcard) */
  NET_RegisterContentTypeConverter (APPLICATION_DIRECTORY, FO_PRESENT,
 									NULL, MIME_VCardConverter);
  NET_RegisterContentTypeConverter (APPLICATION_DIRECTORY, FO_PRINT,
 									NULL, MIME_VCardConverter);

  /* Decoders from mimejul.c for text/calendar */
  PRBool handle_calendar_mime = PR_TRUE;
  PREF_GetBoolPref("calendar.handle_text_calendar_mime_type", &handle_calendar_mime);
  if (handle_calendar_mime)
  {
      NET_RegisterContentTypeConverter (TEXT_CALENDAR, FO_PRESENT,
									    NULL, MIME_JulianConverter);
      NET_RegisterContentTypeConverter (TEXT_CALENDAR, FO_PRINT,
									    NULL, MIME_JulianConverter);
  }
#endif //JFD
}


static PRBool mime_7bit_data_p (const char *string, PRUint32 size)
{
	const unsigned char *s = (const unsigned char *) string;
	const unsigned char *end = s + size;
	if (s)
		for (; s < end; s++)
			if (*s > 0x7F)
				return PR_FALSE;
	return PR_TRUE;
}


static int mime_sanity_check_fields (
					const char *from,
					const char *reply_to,
					const char *to,
					const char *cc,
					const char *bcc,
					const char *fcc,
					const char *newsgroups,
					const char *followup_to,
					const char * /*subject*/,
					const char * /*references*/,
					const char * /*organization*/,
					const char * /*other_random_headers*/)
{
	if (from)
		while (XP_IS_SPACE (*from))
			from++;
	if (reply_to)
		while (XP_IS_SPACE (*reply_to))
			reply_to++;
	if (to)
		while (XP_IS_SPACE (*to))
			to++;
	if (cc)
		while (XP_IS_SPACE (*cc))
			cc++;
	if (bcc)
		while (XP_IS_SPACE (*bcc))
			bcc++;
	if (fcc)
		while (XP_IS_SPACE (*fcc))
			fcc++;
	if (newsgroups)
		while (XP_IS_SPACE (*newsgroups))
			newsgroups++;
	if (followup_to)
		while (XP_IS_SPACE (*followup_to))
			followup_to++;

	/* #### sanity check other_random_headers for newline conventions */
	if (!from || !*from)
		return MK_MIME_NO_SENDER;
	else
		if ((!to || !*to) && (!cc || !*cc) &&
				(!bcc || !*bcc) && (!newsgroups || !*newsgroups))
			return MK_MIME_NO_RECIPIENTS;
	else
		return 0;
}


/* Strips whitespace, and expands newlines into newline-tab for use in
   mail headers.  Returns a new string or 0 (if it would have been empty.)
   If addr_p is true, the addresses will be parsed and reemitted as
   rfc822 mailboxes.
 */
static char * mime_fix_header_1 (const char *string, PRBool addr_p, PRBool news_p)
{
	char *new_string;
	const char *in;
	char *out;
	PRInt32 i, old_size, new_size;

	if (!string || !*string)
		return 0;

	if (addr_p) {
		nsIMsgRFC822Parser * pRfc822;
		nsresult rv = nsComponentManager::CreateInstance(kMsgRFC822ParserCID, 
                                               NULL, 
                                               nsIMsgRFC822Parser::GetIID(), 
                                               (void **) &pRfc822);
		if (NS_SUCCEEDED(rv)) {
			char *n;
			pRfc822->ReformatRFC822Addresses(string, &n);
			pRfc822->Release();
			if (n)
				return n;
		}
	}

	old_size = PL_strlen (string);
	new_size = old_size;
	for (i = 0; i < old_size; i++)
		if (string[i] == CR || string[i] == LF)
			new_size += 2;

	new_string = (char *) PR_Malloc (new_size + 1);
	if (! new_string)
		return 0;

	in  = string;
	out = new_string;

	/* strip leading whitespace. */
	while (XP_IS_SPACE (*in))
		in++;

	/* replace CR, LF, or CRLF with CRLF-TAB. */
	while (*in) {
		if (*in == CR || *in == LF) {
			if (*in == CR && in[1] == LF)
				in++;
			in++;
			*out++ = CR;
			*out++ = LF;
			*out++ = '\t';
		}
		else
			if (news_p && *in == ',') {
				*out++ = *in++;
				/* skip over all whitespace after a comma. */
				while (XP_IS_SPACE (*in))
					in++;
			}
			else
				*out++ = *in++;
	}
	*out = 0;

	/* strip trailing whitespace. */
	while (out > in && XP_IS_SPACE (out[-1]))
		*out-- = 0;

	/* If we ended up throwing it all away, use 0 instead of "". */
	if (!*new_string) {
		PR_Free (new_string);
		new_string = 0;
	}

	return new_string;
}


static char * mime_fix_header (const char *string)
{
  return mime_fix_header_1 (string, PR_FALSE, PR_FALSE);
}

static char * mime_fix_addr_header (const char *string)
{
  return mime_fix_header_1 (string, PR_TRUE, PR_FALSE);
}

static char * mime_fix_news_header (const char *string)
{
  return mime_fix_header_1 (string, PR_FALSE, PR_TRUE);
}


#if 0
static PRBool
mime_type_conversion_possible (const char *from_type, const char *to_type)
{
  if (! to_type)
	return PR_TRUE;

  if (! from_type)
	return PR_FALSE;

  if (!PL_strcasecmp (from_type, to_type))
	/* Don't run text/plain through the text->html converter. */
	return PR_FALSE;

  if ((!PL_strcasecmp (from_type, INTERNAL_PARSER) ||
	   !PL_strcasecmp (from_type, TEXT_HTML) ||
	   !PL_strcasecmp (from_type, TEXT_MDL)) &&
	  !PL_strcasecmp (to_type, TEXT_PLAIN))
	/* Don't run UNKNOWN_CONTENT_TYPE through the text->html converter
	   (treat it as text/plain already.) */
	return PR_TRUE;

#ifdef XP_UNIX
  if ((!PL_strcasecmp (from_type, INTERNAL_PARSER) ||
	   !PL_strcasecmp (from_type, TEXT_PLAIN) ||
	   !PL_strcasecmp (from_type, TEXT_HTML) ||
	   !PL_strcasecmp (from_type, TEXT_MDL) ||
	   !PL_strcasecmp (from_type, IMAGE_GIF) ||
	   !PL_strcasecmp (from_type, IMAGE_JPG) ||
	   !PL_strcasecmp (from_type, IMAGE_PJPG) ||
	   !PL_strcasecmp (from_type, IMAGE_XBM) ||
	   !PL_strcasecmp (from_type, IMAGE_XBM2) ||
	   !PL_strcasecmp (from_type, IMAGE_XBM3) ||
	   /* always treat unknown content types as text/plain */
	   !PL_strcasecmp (from_type, UNKNOWN_CONTENT_TYPE)
	   ) &&
	  !PL_strcasecmp (to_type, APPLICATION_POSTSCRIPT))
	return PR_TRUE;
#endif /* XP_UNIX */

  return PR_FALSE;
}
#endif


static PRBool
mime_type_requires_b64_p (const char *type)
{
  if (!type || !PL_strcasecmp (type, UNKNOWN_CONTENT_TYPE))
	/* Unknown types don't necessarily require encoding.  (Note that
	   "unknown" and "application/octet-stream" aren't the same.) */
	return PR_FALSE;

  else if (!PL_strncasecmp (type, "image/", 6) ||
		   !PL_strncasecmp (type, "audio/", 6) ||
		   !PL_strncasecmp (type, "video/", 6) ||
		   !PL_strncasecmp (type, "application/", 12))
	{
	  /* The following types are application/ or image/ types that are actually
		 known to contain textual data (meaning line-based, not binary, where
		 CRLF conversion is desired rather than disasterous.)  So, if the type
		 is any of these, it does not *require* base64, and if we do need to
		 encode it for other reasons, we'll probably use quoted-printable.
		 But, if it's not one of these types, then we assume that any subtypes
		 of the non-"text/" types are binary data, where CRLF conversion would
		 corrupt it, so we use base64 right off the bat.

		 The reason it's desirable to ship these as text instead of just using
		 base64 all the time is mainly to preserve the readability of them for
		 non-MIME users: if I mail a /bin/sh script to someone, it might not
		 need to be encoded at all, so we should leave it readable if we can.

		 This list of types was derived from the comp.mail.mime FAQ, section
		 10.2.2, "List of known unregistered MIME types" on 2-Feb-96.
	   */
	  static const char *app_and_image_types_which_are_really_text[] = {
		"application/mac-binhex40",		/* APPLICATION_BINHEX */
		"application/pgp",				/* APPLICATION_PGP */
		"application/x-pgp-message",	/* APPLICATION_PGP2 */
		"application/postscript",		/* APPLICATION_POSTSCRIPT */
		"application/x-uuencode",		/* APPLICATION_UUENCODE */
		"application/x-uue",			/* APPLICATION_UUENCODE2 */
		"application/uue",				/* APPLICATION_UUENCODE4 */
		"application/uuencode",			/* APPLICATION_UUENCODE3 */
		"application/sgml",
		"application/x-csh",
		"application/x-javascript",
		"application/x-latex",
		"application/x-macbinhex40",
		"application/x-ns-proxy-autoconfig",
		"application/x-www-form-urlencoded",
		"application/x-perl",
		"application/x-sh",
		"application/x-shar",
		"application/x-tcl",
		"application/x-tex",
		"application/x-texinfo",
		"application/x-troff",
		"application/x-troff-man",
		"application/x-troff-me",
		"application/x-troff-ms",
		"application/x-troff-ms",
		"application/x-wais-source",
		"image/x-bitmap",
		"image/x-pbm",
		"image/x-pgm",
		"image/x-portable-anymap",
		"image/x-portable-bitmap",
		"image/x-portable-graymap",
		"image/x-portable-pixmap",		/* IMAGE_PPM */
		"image/x-ppm",
		"image/x-xbitmap",				/* IMAGE_XBM */
		"image/x-xbm",					/* IMAGE_XBM2 */
		"image/xbm",					/* IMAGE_XBM3 */
		"image/x-xpixmap",
		"image/x-xpm",
		0 };
	  const char **s;
	  for (s = app_and_image_types_which_are_really_text; *s; s++)
		if (!PL_strcasecmp (type, *s))
		  return PR_FALSE;

	  /* All others must be assumed to be binary formats, and need Base64. */
	  return PR_TRUE;
	}

  else
	return PR_FALSE;
}

#ifdef XP_OS2
XP_BEGIN_PROTOS
extern int mime_encoder_output_fn (const char *buf, /*JFD PRInt32 */int32 size, void *closure);
XP_END_PROTOS
#else
static int mime_encoder_output_fn (const char *buf, /*JFD PRInt32 */int32 size, void *closure);
#endif

/* Given a content-type and some info about the contents of the document,
   decide what encoding it should have.
 */
int
MSG_DeliverMimeAttachment::PickEncoding (const char *charset)
{
  // use the boolean so we only have to test for uuencode vs base64 once
  PRBool needsB64 = PR_FALSE;
  PRBool forceB64 = PR_FALSE;

  if (m_already_encoded_p)
	goto DONE;

  /* Allow users to override our percentage-wise guess on whether
	 the file is text or binary */
  PREF_GetBoolPref ("mail.file_attach_binary", &forceB64);

  if (forceB64 || mime_type_requires_b64_p (m_type))
	{
	  /* If the content-type is "image/" or something else known to be binary,
		 always use base64 (so that we don't get confused by newline
		 conversions.)
	   */
		needsB64 = PR_TRUE;
	}
  else
	{
	  /* Otherwise, we need to pick an encoding based on the contents of
		 the document.
	   */

	  PRBool encode_p;

	  if (m_max_column > 900)
		encode_p = PR_TRUE;
	  else if (mime_use_quoted_printable_p && m_unprintable_count)
		encode_p = PR_TRUE;

	  else if (m_null_count)	/* If there are nulls, we must always encode,
								   because sendmail will blow up. */
		encode_p = PR_TRUE;
#if 0
	  else if (m_ctl_count)	/* Should we encode if random other control
								   characters are present?  Probably... */
		encode_p = PR_TRUE;
#endif
	  else
		encode_p = PR_FALSE;

	  /* MIME requires a special case that these types never be encoded.
	   */
	  if (!PL_strncasecmp (m_type, "message", 7) ||
		  !PL_strncasecmp (m_type, "multipart", 9))
		{
		  encode_p = PR_FALSE;
		  if (m_desired_type && !PL_strcasecmp (m_desired_type, TEXT_PLAIN))
			{
			  PR_Free (m_desired_type);
			  m_desired_type = 0;
			}
		}
	  /* If the Mail charset is ISO_2022_JP we force it to use Base64 for attachments (bug#104255). 
		Use 7 bit for other STATFUL charsets ( e.g. ISO_2022_KR). */
	  if ((PL_strcasecmp(charset, "iso-2022-jp") == 0) &&
		  (PL_strcasecmp(m_type, TEXT_HTML) == 0))
         needsB64 = PR_TRUE;
	  else if((INTL_stateful_charset(charset)) &&
         ((PL_strcasecmp(m_type, TEXT_HTML) == 0) ||
          (PL_strcasecmp(m_type, TEXT_MDL) == 0) ||
          (PL_strcasecmp(m_type, TEXT_PLAIN) == 0) ||
          (PL_strcasecmp(m_type, TEXT_RICHTEXT) == 0) ||
          (PL_strcasecmp(m_type, TEXT_ENRICHED) == 0) ||
          (PL_strcasecmp(m_type, TEXT_VCARD) == 0) ||
          (PL_strcasecmp(m_type, APPLICATION_DIRECTORY) == 0) || /* text/x-vcard synonym */
          (PL_strcasecmp(m_type, TEXT_CSS) == 0) ||
          (PL_strcasecmp(m_type, TEXT_JSSS) == 0) ||
          (PL_strcasecmp(m_type, MESSAGE_RFC822) == 0) ||
          (PL_strcasecmp(m_type, MESSAGE_NEWS) == 0)))
      {
		PR_FREEIF(m_encoding);
        m_encoding = PL_strdup (ENCODING_7BIT);
	  }
	  else if (encode_p &&
               m_size > 500 &&
               m_unprintable_count > (m_size / 10))
		/* If the document contains more than 10% unprintable characters,
		   then that seems like a good candidate for base64 instead of
		   quoted-printable.
		 */
		  needsB64 = PR_TRUE;
	  else if (encode_p) {
		PR_FREEIF(m_encoding);
		m_encoding = PL_strdup (ENCODING_QUOTED_PRINTABLE);
	  }
	  else if (m_highbit_count > 0) {
		PR_FREEIF(m_encoding);
		m_encoding = PL_strdup (ENCODING_8BIT);
	  }
	  else {
		PR_FREEIF(m_encoding);
		m_encoding = PL_strdup (ENCODING_7BIT);
	  }
	}

  if (needsB64)
  {
	  /* 
		 ### mwelch We might have to uuencode instead of 
		            base64 the binary data.
	  */
	  PR_FREEIF(m_encoding);
	  if (UseUUEncode_p())
		  m_encoding = PL_strdup (ENCODING_UUENCODE);
	  else
		  m_encoding = PL_strdup (ENCODING_BASE64);
  }

  /* Now that we've picked an encoding, initialize the filter.
   */
  NS_ASSERTION(!m_encoder_data, "not-null m_encoder_data");
  if (!PL_strcasecmp(m_encoding, ENCODING_BASE64))
	{
	  m_encoder_data = MimeB64EncoderInit(mime_encoder_output_fn,
										  m_mime_delivery_state);
	  if (!m_encoder_data) return MK_OUT_OF_MEMORY;
	}
  else if (!PL_strcasecmp(m_encoding, ENCODING_UUENCODE))
	{
		char *tailName = NULL;
		
		if (m_url_string)
		{
		  	tailName = PL_strrchr(m_url_string, '/');
			if (tailName) {
				char * tmp = tailName;
		  		tailName = PL_strdup(tailName+1);
				PR_FREEIF(tmp);
			}
		}
		
		if (m_url && !tailName)
		{
			tailName = PL_strrchr(m_url->address, '/');
			if (tailName) {
				char * tmp = tailName;
				tailName = PL_strdup(tailName+1);
				PR_FREEIF(tmp);
			}
		}

		  m_encoder_data = MimeUUEncoderInit(tailName ? tailName : "",
	  									  mime_encoder_output_fn,
										  m_mime_delivery_state);
	  PR_FREEIF(tailName);
	  if (!m_encoder_data) return MK_OUT_OF_MEMORY;
	}
  else if (!PL_strcasecmp(m_encoding, ENCODING_QUOTED_PRINTABLE))
	{
	  m_encoder_data = MimeQPEncoderInit(mime_encoder_output_fn,
										 m_mime_delivery_state);
	  if (!m_encoder_data) return MK_OUT_OF_MEMORY;
	}
  else
	{
	  m_encoder_data = 0;
	}


  /* Do some cleanup for documents with unknown content type.

	 There are two issues: how they look to MIME users, and how they look to
	 non-MIME users.

	 If the user attaches a "README" file, which has unknown type because it
	 has no extension, we still need to send it with no encoding, so that it
	 is readable to non-MIME users.

	 But if the user attaches some random binary file, then base64 encoding
	 will have been chosen for it (above), and in this case, it won't be
	 immediately readable by non-MIME users.  However, if we type it as
	 text/plain instead of application/octet-stream, it will show up inline
	 in a MIME viewer, which will probably be ugly, and may possibly have
	 bad charset things happen as well.

	 So, the heuristic we use is, if the type is unknown, then the type is
	 set to application/octet-stream for data which needs base64 (binary data)
	 and is set to text/plain for data which didn't need base64 (unencoded or
	 lightly encoded data.)
   */
 DONE:
  if (!m_type || !*m_type || !PL_strcasecmp(m_type, UNKNOWN_CONTENT_TYPE))
	{
	  PR_FREEIF(m_type);
	  if (m_already_encoded_p)
		m_type = PL_strdup (APPLICATION_OCTET_STREAM);
	  else if (m_encoding &&
			   (!PL_strcasecmp(m_encoding, ENCODING_BASE64) ||
				!PL_strcasecmp(m_encoding, ENCODING_UUENCODE)))
		m_type = PL_strdup (APPLICATION_OCTET_STREAM);
	  else
		m_type = PL_strdup (TEXT_PLAIN);
	}
  return 0;
}


/* Some types should have a "charset=" parameter, and some shouldn't.
   This is what decides. */
PRBool mime_type_needs_charset (const char *type)
{
	/* Only text types should have charset. */
	if (!type || !*type)
		return PR_FALSE;
	else 
		if (!PL_strncasecmp (type, "text", 4))
			return PR_TRUE;
		else
			return PR_FALSE;
}


char* mime_get_stream_write_buffer(void)
{
	if (!mime_mailto_stream_write_buffer)
		mime_mailto_stream_write_buffer = (char *) PR_Malloc(MIME_BUFFER_SIZE);
	return mime_mailto_stream_write_buffer;
}

int nsMsgSendMimeDeliveryState::InitImapOfflineDB(PRUint32 flag)
{
	int status = 0;
	char *folderName = msg_MagicFolderName(m_pane->GetMaster()->GetPrefs(), flag, &status);

	if (status < 0) 
		return status;
	else if (!folderName) 
		return MK_OUT_OF_MEMORY;
	else if (NET_URL_Type(folderName) == IMAP_TYPE_URL)
	{
		nsresult err = NS_OK;
		char *dummyEnvelope = msg_GetDummyEnvelope();
		NS_ASSERTION(!m_imapOutgoingParser && !m_imapLocalMailDB, "not-null ptr");
		err = MailDB::Open(m_msg_file_name, PR_TRUE, &m_imapLocalMailDB);
		if (err != NS_OK) 
		{
			if (m_imapLocalMailDB)
				m_imapLocalMailDB->Close();
			m_imapLocalMailDB = NULL;
			status = MK_OUT_OF_MEMORY;
			goto done;
		}
		m_imapOutgoingParser = new ParseOutgoingMessage;
		if (!m_imapOutgoingParser)
		{
			m_imapLocalMailDB->Close();
			m_imapLocalMailDB = NULL;
			status = MK_OUT_OF_MEMORY;
			goto done;
		}
		m_imapOutgoingParser->Clear();
/*JFD
		m_imapOutgoingParser->SetOutFile(m_msg_file);
*/
		m_imapOutgoingParser->SetMailDB(m_imapLocalMailDB);
		m_imapOutgoingParser->SetWriteToOutFile(PR_FALSE);
		m_imapOutgoingParser->Init(0);
		m_imapOutgoingParser->StartNewEnvelope(dummyEnvelope,
											   PL_strlen(dummyEnvelope));
	}

done:
	PR_FREEIF(folderName);
	return status;
}
#define PUSH_STRING(S) \
 do { PL_strcpy (buffer_tail, S); buffer_tail += PL_strlen (S); } while(0)
#define PUSH_NEWLINE() \
 do { *buffer_tail++ = CR; *buffer_tail++ = LF; *buffer_tail = '\0'; } while(0)


/* All of the desired attachments have been written to individual temp files,
   and we know what's in them.  Now we need to make a final temp file of the
   actual mail message, containing all of the other files after having been
   encoded as appropriate.
 */
int nsMsgSendMimeDeliveryState::GatherMimeAttachments ()
{
	PRBool shouldDeleteDeliveryState = PR_TRUE;
	PRInt32 status, i;
	char *headers = 0;
	char *separator = 0;
	XP_File in_file = 0;
	PRBool multipart_p = PR_FALSE;
	PRBool plaintext_is_mainbody_p = PR_FALSE; // only using text converted from HTML?
	char *buffer = 0;
	char *buffer_tail = 0;
	char* error_msg = 0;
 
  // to news is true if we have a m_field and we have a Newsgroup and it is not empty
	PRBool tonews = PR_FALSE;
	if (m_fields) {
		const char* pstrzNewsgroup = m_fields->GetNewsgroups();
		if (pstrzNewsgroup && *pstrzNewsgroup)
			tonews = PR_TRUE;
	}

	INTL_MessageSendToNews(tonews);			// hack to make Korean Mail/News work correctly 
											// Look at libi18n/doc_ccc.c for details 
											// temp solution for bug 30725

	nsMsgSendPart* toppart = NULL;			// The very top most container of the message
											// that we are going to send.

	nsMsgSendPart* mainbody = NULL;			// The leaf node that contains the text of the
											// message we're going to contain.

	nsMsgSendPart* maincontainer = NULL;	// The direct child of toppart that will
											// contain the mainbody.  If mainbody is
											// the same as toppart, then this is
											// also the same.  But if mainbody is
											// to end up somewhere inside of a
											// multipart/alternative or a
											// multipart/related, then this is that
											// multipart object.

	nsMsgSendPart* plainpart = NULL;		// If we converted HTML into plaintext,
											// the message or child containing the plaintext
											// goes here. (Need to use this to determine
											// what headers to append/set to the main 
											// message body.)
	char *hdrs = 0;
	PRBool maincontainerISrelatedpart = PR_FALSE;

	status = m_status;
	if (status < 0)
		goto FAIL;

	if (m_attachments_only_p)
	{
		/* If we get here, we shouldn't have the "generating a message" cb. */
		NS_ASSERTION(!m_dont_deliver_p && !m_message_delivery_done_callback, "Shouldn't be here");
		if (m_attachments_done_callback) {
			struct MSG_AttachedFile *attachments;

			NS_ASSERTION(m_attachment_count > 0, "not more attachment");
			if (m_attachment_count <= 0) {
				m_attachments_done_callback (GetContext(), m_fe_data, 0, 0, 0);
				m_attachments_done_callback = 0;
				Clear();
				goto FAIL;
			}

			attachments = (struct MSG_AttachedFile *)PR_Malloc((m_attachment_count + 1) * sizeof(*attachments));

			if (!attachments)
				goto FAILMEM;
			memset(attachments, 0, ((m_attachment_count + 1) * sizeof(*attachments)));
			for (i = 0; i < m_attachment_count; i++)
			{
				MSG_DeliverMimeAttachment *ma = &m_attachments[i];

#undef SNARF
#define SNARF(x,y) do { if((y) && *(y) && !(x)) { ((x) = (y)); ((y) = 0); }} \
				   while(0)
				/* Rather than copying the strings and dealing with allocation
				 failures, we'll just "move" them into the other struct (this
				 should be ok since this file uses PR_FREEIF when discarding
				 the mime_attachment objects.) */
				SNARF(attachments[i].orig_url, ma->m_url_string);
				SNARF(attachments[i].file_name, ma->m_file_name);
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
				HJ08239

				/* Doesn't really matter, but let's not lie about encoding
				   in case it does someday. */
				if (attachments[i].highbit_count > 0 && attachments[i].encoding &&
						!PL_strcasecmp(attachments[i].encoding, ENCODING_7BIT))
					attachments[i].encoding = ENCODING_8BIT;
			}

			m_attachments_done_callback(GetContext(), m_fe_data, 0, 0, attachments);
			PR_FREEIF(attachments);
			m_attachments_done_callback = 0;
			Clear();
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

	/* If we have a text/html main part, and we need a plaintext attachment, then
	   we'll do so now.  This is an asynchronous thing, so we'll kick it off and
	   count on getting back here when it finishes. */

	if (m_plaintext == NULL &&
			(m_fields->GetForcePlainText() ||
			m_fields->GetUseMultipartAlternative()) &&
			m_attachment1_body && PL_strcmp(m_attachment1_type, TEXT_HTML) == 0)
	{
		m_html_filename = WH_TempName (xpFileToPost, "nsmail");
		if (!m_html_filename)
			goto FAILMEM;

		nsFileSpec aPath(m_html_filename);
		nsOutputFileStream tmpfile(aPath);
		if (! tmpfile.is_open()) {
			status = MK_UNABLE_TO_OPEN_TMP_FILE;
			goto FAIL;
		}

		status = tmpfile.write(m_attachment1_body, m_attachment1_body_length);
		if (status < int(m_attachment1_body_length)) {
			if (status >= 0)
				status = MK_MIME_ERROR_WRITING_FILE;
			goto FAIL;
		}

		if (tmpfile.failed()) goto FAIL;

		m_plaintext = new MSG_DeliverMimeAttachment;
		if (!m_plaintext)
			goto FAILMEM;
		m_plaintext->m_mime_delivery_state = this;
		char* temp = WH_FileName(m_html_filename, xpFileToPost);
		m_plaintext->m_url_string = XP_PlatformFileToURL(temp);
		PR_FREEIF(temp);
		if (!m_plaintext->m_url_string)
			goto FAILMEM;
		m_plaintext->m_url = NET_CreateURLStruct(m_plaintext->m_url_string, NET_DONT_RELOAD);
		if (!m_plaintext->m_url)
			goto FAILMEM;
		PR_FREEIF(m_plaintext->m_url->content_type);
		m_plaintext->m_url->content_type = PL_strdup(TEXT_HTML);
		PR_FREEIF(m_plaintext->m_type);
		m_plaintext->m_type = PL_strdup(TEXT_HTML);
		PR_FREEIF(m_plaintext->m_charset);
		m_plaintext->m_charset = PL_strdup(m_fields->GetCharacterSet());
		PR_FREEIF(m_plaintext->m_desired_type);
		m_plaintext->m_desired_type = PL_strdup(TEXT_PLAIN);
		m_attachment_pending_count ++;
		status = m_plaintext->SnarfAttachment();
		if (status < 0)
			goto FAIL;
		if (m_attachment_pending_count > 0)
			return 0;
	}
	  
	/* Kludge to avoid having to allocate memory on the toy computers... */
	buffer = mime_get_stream_write_buffer();
	if (! buffer)
		goto FAILMEM;

	buffer_tail = buffer;

	NS_ASSERTION (m_attachment_pending_count == 0, "m_attachment_pending_count != 0");

	FE_Progress(GetContext(), XP_GetString(MK_MSG_ASSEMBLING_MSG));

	/* First, open the message file.
	*/
	m_msg_file_name = WH_TempName (xpFileToPost, "nsmail");
	if (! m_msg_file_name)
		goto FAILMEM;

	{ //those brackets are needed, please to not remove it else VC++6 will complain
		nsFileSpec msgPath(m_msg_file_name);
		m_msg_file = new nsOutputFileStream(msgPath);
	}
	if (! m_msg_file->is_open()) {
		status = MK_UNABLE_TO_OPEN_TMP_FILE;
		error_msg = PR_smprintf(XP_GetString(status), m_msg_file_name);
		if (!error_msg)
			status = MK_OUT_OF_MEMORY;
		goto FAIL;
	}

	if (NET_IsOffline() && IsSaveMode()) {
		status = InitImapOfflineDB( m_deliver_mode == MSG_SaveAsTemplate ?
								  MSG_FOLDER_FLAG_TEMPLATES :
								  MSG_FOLDER_FLAG_DRAFTS );
		if (status < 0) {
			error_msg = PL_strdup (XP_GetString(status));
			goto FAIL;
		}
	}
	  
#ifdef GENERATE_MESSAGE_ID
	if (m_fields->GetMessageId() == NULL || *m_fields->GetMessageId() == 0)
		m_fields->SetMessageId(msg_generate_message_id (), NULL);
#endif /* GENERATE_MESSAGE_ID */

	mainbody = new nsMsgSendPart(this, m_fields->GetCharacterSet());
	if (!mainbody)
		goto FAILMEM;
  
	mainbody->SetMainPart(PR_TRUE);
	mainbody->SetType(m_attachment1_type ? m_attachment1_type : TEXT_PLAIN);

	NS_ASSERTION(mainbody->GetBuffer() == NULL, "not-null buffer");
	status = mainbody->SetBuffer(m_attachment1_body ? m_attachment1_body : " ");
	if (status < 0)
		goto FAIL;

	/*	### mwelch 
		Determine the encoding of the main message body before we free it.
		The proper way to do this should be to test whatever text is in mainbody
		just before writing it out, but that will require a fix that is less safe
		and takes more memory. */
	PR_FREEIF(m_attachment1_encoding);
	if (INTL_stateful_charset(m_fields->GetCharacterSet()) || /* CS_JIS or CS_2022_KR */
		  mime_7bit_data_p (m_attachment1_body, m_attachment1_body_length))
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

		// To plug a memory leak, delete the original maincontainer/mainbody.
		// 
		// NOTE: We DO NOT want to do this to the HTML or multipart/related 
		// nsMsgSendParts because they are deleted at the end of message 
		// delivery by the TapeFileSystem object 
		// (MSG_MimeRelatedSaver / mhtmlstm.cpp).
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

	if (m_plaintext) {
		// OK.  We have a plaintext version of the main body that we want to
		// send instead of or with the text/html.  Shove it in.

		plainpart = new nsMsgSendPart(this, 0/*should be m_fields->GetCharacterSet()*/);
		if (!plainpart)
			goto FAILMEM;
		status = plainpart->SetType(TEXT_PLAIN);
		if (status < 0)
			goto FAIL;
		status = plainpart->SetFile(m_plaintext->m_file_name, xpFileToPost);
		if (status < 0)
			goto FAIL;
		m_plaintext->AnalyzeSnarfedFile(); // look for 8 bit text, long lines, etc.
		m_plaintext->PickEncoding(m_fields->GetCharacterSet());
		hdrs = mime_generate_attachment_headers(m_plaintext->m_type,
											  m_plaintext->m_encoding,
											  m_plaintext->m_description,
											  m_plaintext->m_x_mac_type,
											  m_plaintext->m_x_mac_creator,
											  m_plaintext->m_real_name, 0,
											  m_digest_p,
											  m_plaintext,
											  m_fields->GetCharacterSet());
		if (!hdrs)
			goto FAILMEM;
		status = plainpart->SetOtherHeaders(hdrs);
		PR_Free(hdrs);
		hdrs = NULL;
		if (status < 0)
			goto FAIL;

		if (m_fields->GetUseMultipartAlternative()) {
			nsMsgSendPart* htmlpart = maincontainer;
			maincontainer = new nsMsgSendPart(this);
			if (!maincontainer)
				goto FAILMEM;
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
				MimeEncoderData *plaintext_enc = MimeQPEncoderInit(mime_encoder_output_fn, this);
				if (!plaintext_enc)
				{
					status = MK_OUT_OF_MEMORY;
					goto FAIL;
				}
				plainpart->SetEncoderData(plaintext_enc);
			}
		}
		else {
			delete maincontainer; //### mwelch ??? doesn't this cause a crash?!
			if (maincontainerISrelatedpart)
				m_related_part = NULL;
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

	// ###tw  This used to be this more complicated thing, but for now, if it we
	// have any attachments, we generate multipart.
	// multipart_p = (m_attachment_count > 1 ||
	//				 (m_attachment_count == 1 &&
	//				  m_attachment1_body_length > 0));
	multipart_p = (m_attachment_count > 0);

	if (multipart_p) {
		toppart = new nsMsgSendPart(this);
		if (!toppart)
			goto FAILMEM;
		status = toppart->SetType(m_digest_p ? MULTIPART_DIGEST : MULTIPART_MIXED);
		if (status < 0)
			goto FAIL;
		status = toppart->AddChild(maincontainer);
		if (status < 0)
			goto FAIL;
		if (!m_crypto_closure) {
			status = toppart->SetBuffer(XP_GetString (MK_MIME_MULTIPART_BLURB));
			if (status < 0)
				goto FAIL;
		}
	}
	else
		toppart = maincontainer;

  /* Write out the message headers.
   */
	headers = mime_generate_headers (m_fields,
								   m_fields->GetCharacterSet(),
								   m_deliver_mode);
	if (!headers)
		goto FAILMEM;

	// ### mwelch
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
	headers = NULL;
	if (status < 0)
		goto FAIL;

	/* Set up the first part (user-typed.)  For now, do it even if the first
	* part is empty; we need to add things to skip it if this part is empty.
	* ###tw
	*/


	/* Set up encoder for the first part (message body.)
	*/
	NS_ASSERTION(!m_attachment1_encoder_data, "not-null m_attachment1_encoder_data");
	if (!PL_strcasecmp(m_attachment1_encoding, ENCODING_BASE64))
	{
		m_attachment1_encoder_data = MimeB64EncoderInit(mime_encoder_output_fn, this);
		if (!m_attachment1_encoder_data) goto FAILMEM;
	}
	else
		if (!PL_strcasecmp(m_attachment1_encoding, ENCODING_QUOTED_PRINTABLE)) {
			m_attachment1_encoder_data =
			MimeQPEncoderInit(mime_encoder_output_fn, this);
			if (!m_attachment1_encoder_data)
;//JFD				goto FAILMEM;
		}

	// ### mwelch
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
											   NULL, /* no "ma"! */
											   m_fields->GetCharacterSet());
		if (!hdrs)
			goto FAILMEM;
		status = mainbody->AppendOtherHeaders(hdrs);
		if (status < 0)
			goto FAIL;
	}

	PR_FREEIF(hdrs);

	status = mainbody->SetEncoderData(m_attachment1_encoder_data);
	m_attachment1_encoder_data = NULL;
	if (status < 0)
		goto FAIL;


	/* Set up the subsequent parts.
	*/
	if (m_attachment_count > 0)
	{
		char *buffer;

		/* Kludge to avoid having to allocate memory on the toy computers... */
		if (! mime_mailto_stream_read_buffer)
			mime_mailto_stream_read_buffer = (char *) PR_Malloc (MIME_BUFFER_SIZE);
		buffer = mime_mailto_stream_read_buffer;
		if (! buffer)
			goto FAILMEM;
		buffer_tail = buffer;

		for (i = 0; i < m_attachment_count; i++)
		{
			MSG_DeliverMimeAttachment *ma = &m_attachments[i];
			char *hdrs = 0;

			nsMsgSendPart* part = NULL;

			// If at this point we *still* don't have an content-type, then
			// we're never going to get one.
			if (ma->m_type == NULL) {
				ma->m_type = PL_strdup(UNKNOWN_CONTENT_TYPE);
				if (ma->m_type == NULL)
					goto FAILMEM;
			}

			ma->PickEncoding (m_fields->GetCharacterSet());

			part = new nsMsgSendPart(this);
			if (!part)
				goto FAILMEM;
			status = toppart->AddChild(part);
			if (status < 0)
				goto FAIL;
			status = part->SetType(ma->m_type);
			if (status < 0)
				goto FAIL;

			hdrs = mime_generate_attachment_headers (ma->m_type, ma->m_encoding,
												   ma->m_description,
												   ma->m_x_mac_type,
												   ma->m_x_mac_creator,
												   ma->m_real_name,
												   ma->m_url_string,
												   m_digest_p,
												   ma,
												   m_fields->GetCharacterSet());
			if (!hdrs)
				goto FAILMEM;

			status = part->SetOtherHeaders(hdrs);
			PR_FREEIF(hdrs);
			if (status < 0)
				goto FAIL;
			status = part->SetFile(ma->m_file_name, xpFileToPost);
			if (status < 0)
				goto FAIL;
			if (ma->m_encoder_data) {
				status = part->SetEncoderData(ma->m_encoder_data);
				if (status < 0)
					goto FAIL;
				ma->m_encoder_data = NULL;
			}

			ma->m_current_column = 0;

			if (ma->m_type &&
					(!PL_strcasecmp (ma->m_type, MESSAGE_RFC822) ||
					!PL_strcasecmp (ma->m_type, MESSAGE_NEWS))) {
				status = part->SetStripSensitiveHeaders(PR_TRUE);
				if (status < 0)
					goto FAIL;
			}
		}
	}

	// OK, now actually write the structure we've carefully built up.
	status = toppart->Write();
	if (status < 0)
		goto FAIL;

	HJ45609

	if (m_msg_file) {
		/* If we don't do this check...ZERO length files can be sent */
		if (m_msg_file->failed()) {
			status = MK_MIME_ERROR_WRITING_FILE;
			goto FAIL;
		}
		delete m_msg_file;
	}
	m_msg_file = NULL;

	if (m_imapOutgoingParser)
	{
		m_imapOutgoingParser->FinishHeader();
		NS_ASSERTION(m_imapOutgoingParser->m_newMsgHdr, "no new message header");
		m_imapOutgoingParser->m_newMsgHdr->OrFlags(kIsRead);
		m_imapOutgoingParser->m_newMsgHdr->SetMessageSize (m_imapOutgoingParser->m_bytes_written);
		m_imapOutgoingParser->m_newMsgHdr->SetMessageKey(0);
		m_imapLocalMailDB->AddHdrToDB(m_imapOutgoingParser->m_newMsgHdr, NULL, PR_TRUE);
	}

	FE_Progress(GetContext(), XP_GetString(MK_MSG_ASSEMB_DONE_MSG));

	if (m_dont_deliver_p && m_message_delivery_done_callback)
	{
		m_message_delivery_done_callback (GetContext(),
											 m_fe_data, 0,
											 PL_strdup (m_msg_file_name));
		/* Need to ditch the file name here so that we don't delete the
		 file, since in this case, the FE needs the file itself. */
		PR_FREEIF (m_msg_file_name);
		m_msg_file_name = 0;
		m_message_delivery_done_callback = 0;
		Clear();
	}
	else {
		DeliverMessage();
		shouldDeleteDeliveryState = PR_FALSE;
	}

FAIL:
	if (toppart)
		delete toppart;
	toppart = NULL;
	mainbody = NULL;
	maincontainer = NULL;

	PR_FREEIF(headers);
	PR_FREEIF(separator);
	if (in_file) {
		XP_FileClose (in_file);
		in_file = NULL;
	}

	if (shouldDeleteDeliveryState)
	{
		if (status < 0) {
			m_status = status;
			Fail (status, error_msg);
		}
//JFD don't delete myself		delete this;
	}

	return status;

FAILMEM:
	status = MK_OUT_OF_MEMORY;
	goto FAIL;
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

HJ91531

#if defined(XP_MAC) && defined(DEBUG)
#pragma global_optimizer reset
#endif // XP_MAC && DEBUG


int
mime_write_message_body (nsMsgSendMimeDeliveryState *state,
						 char *buf, PRInt32 size)
{
	HJ62011
	{
		if (PRInt32(state->m_msg_file->write(buf, size)) < size) {
			return MK_MIME_ERROR_WRITING_FILE;
		} else {
			if (state->m_imapOutgoingParser) {
				state->m_imapOutgoingParser->ParseFolderLine(buf, size);
				state->m_imapOutgoingParser->m_bytes_written += size;
			}
			return 0;
		}
	}
}


#ifdef XP_OS2
extern int
#else
static int
#endif
mime_encoder_output_fn (const char *buf, /*JFD PRInt32*/int32 size, void *closure)
{
  nsMsgSendMimeDeliveryState *state = (nsMsgSendMimeDeliveryState *) closure;
  return mime_write_message_body (state, (char *) buf, size);
}


char * msg_generate_message_id (void)
{
	time_t now = XP_TIME();
	PRUint32 salt = 0;
	nsMsgCompPrefs pCompPrefs;
	const char *host = 0;
	const char *from = pCompPrefs.GetUserEmail();

	RNG_GenerateGlobalRandomBytes((void *) &salt, sizeof(salt));

	if (from) {
		host = PL_strchr (from, '@');
		if (host) {
			const char *s;
			for (s = ++host; *s; s++)
				if (!XP_IS_ALPHA(*s) && !XP_IS_DIGIT(*s) &&
						*s != '-' && *s != '_' && *s != '.') {
					host = 0;
					break;
				}
		}
	}

	if (! host)
	/* If we couldn't find a valid host name to use, we can't generate a
	   valid message ID, so bail, and let NNTP and SMTP generate them. */
		return 0;

	return PR_smprintf("<%lX.%lX@%s>",
					 (unsigned long) now, (unsigned long) salt, host);
}

static char * mime_generate_headers (nsMsgCompFields *fields,
									const char *charset,
									MSG_Deliver_Mode deliver_mode)
{
	int size = 0;
	char *buffer = 0, *buffer_tail = 0;
	PRBool isDraft = deliver_mode == MSG_SaveAsDraft ||
					deliver_mode == MSG_SaveAsTemplate ||
					deliver_mode == MSG_QueueForLater;

	const char* pFrom;
	const char* pTo;
	const char* pCc;
	const char* pMessageID;
	const char* pReplyTo;
	const char* pOrg;
	const char* pNewsGrp;
	const char* pFollow;
	const char* pSubject;
	const char* pPriority;
	const char* pReference;
	const char* pOtherHdr;

	NS_ASSERTION (fields, "null fields");
	if (!fields)
		return NULL;

	/* Multiply by 3 here to make enough room for MimePartII conversion */
	pFrom = fields->GetFrom(); if (pFrom)						size += 3 * PL_strlen (pFrom);
	pReplyTo =fields->GetReplyTo(); if (pReplyTo)				size += 3 * PL_strlen (pReplyTo);
	pTo = fields->GetTo(); if (pTo)								size += 3 * PL_strlen (pTo);
	pCc = fields->GetCc(); if (pCc)								size += 3 * PL_strlen (pCc);
	pNewsGrp = fields->GetNewsgroups(); if (pNewsGrp)			size += 3 * PL_strlen (pNewsGrp);
	pFollow= fields->GetFollowupTo(); if (pFollow)				size += 3 * PL_strlen (pFollow);
	pSubject = fields->GetSubject(); if (pSubject)				size += 3 * PL_strlen (pSubject);
	pReference = fields->GetReferences(); if (pReference)		size += 3 * PL_strlen (pReference);
	pOrg= fields->GetOrganization(); if (pOrg)					size += 3 * PL_strlen (pOrg);
	pOtherHdr= fields->GetOtherRandomHeaders(); if (pOtherHdr)	size += 3 * PL_strlen (pOtherHdr);
	pPriority = fields->GetPriority();  if (pPriority)			size += 3 * PL_strlen (pPriority);
#ifdef GENERATE_MESSAGE_ID
	pMessageID = fields->GetMessageId(); if (pMessageID)		size += PL_strlen (pMessageID);
#endif /* GENERATE_MESSAGE_ID */

	/* Add a bunch of slop for the static parts of the headers. */
	/* size += 2048; */
	size += 2560;

	buffer = (char *) PR_Malloc (size);
	if (!buffer)
		return 0; /* MK_OUT_OF_MEMORY */
	
	buffer_tail = buffer;

#ifdef GENERATE_MESSAGE_ID
	if (pMessageID && *pMessageID) {
		char *convbuf = NULL;
		PUSH_STRING ("Message-ID: ");
		PUSH_STRING (pMessageID);
		PUSH_NEWLINE ();
		/* MDN request header requires to have MessageID header presented
		* in the message in order to
		* coorelate the MDN reports to the original message. Here will be
		* the right place
		*/
		if (fields->GetReturnReceipt() && 
			(fields->GetReturnReceiptType() == 2 ||
			fields->GetReturnReceiptType() == 3) && 
			(deliver_mode != MSG_SaveAsDraft &&
			deliver_mode != MSG_SaveAsTemplate))
		{
			PRInt32 receipt_header_type = 0;

			PREF_GetIntPref("mail.receipt.request_header_type",
						  &receipt_header_type);
			// 0 = MDN Disposition-Notification-To: ; 1 = Return-Receipt-To: ; 2 =
			// both MDN DNT & RRT headers
			if (receipt_header_type == 1) {
				RRT_HEADER:
				PUSH_STRING ("Return-Receipt-To: ");
				convbuf = INTL_EncodeMimePartIIStr((char *)pFrom, charset, mime_headers_use_quoted_printable_p);
				if (convbuf) {    /* MIME-PartII conversion */
					PUSH_STRING (convbuf);
					PR_FREEIF(convbuf);
				}
				else
					PUSH_STRING (pFrom);
				PUSH_NEWLINE ();
			}
			else  {
				PUSH_STRING ("Disposition-Notification-To: ");
				convbuf = INTL_EncodeMimePartIIStr((char *)pFrom, charset,
												mime_headers_use_quoted_printable_p);
				if (convbuf) {     /* MIME-PartII conversion */
					PUSH_STRING (convbuf);
					PR_FREEIF(convbuf);
				}
				else
					PUSH_STRING (pFrom);
				PUSH_NEWLINE ();
				if (receipt_header_type == 2)
					goto RRT_HEADER;
			}
		}
#ifdef SUPPORT_X_TEMPLATE_NAME
		if (deliver_mode == MSG_SaveAsTemplate) {
			PUSH_STRING ("X-Template: ");
			char * pStr = fields->GetTemplateName();
			if (pStr) {
				convbuf = INTL_EncodeMimePartIIStr((char *)
									 pStr,
									 charset,
									 mime_headers_use_quoted_printable_p);
				if (convbuf) {
				  PUSH_STRING (convbuf);
				  PR_FREEIF(convbuf);
				}
				else
				  PUSH_STRING(pStr);
			}
			PUSH_NEWLINE ();
		}
#endif /* SUPPORT_X_TEMPLATE_NAME */
	}
#endif /* GENERATE_MESSAGE_ID */

	PRExplodedTime now;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
	int gmtoffset = now.tm_params.tp_gmt_offset / 60; /*We need the offset in minute and not in second! */

	/* Use PR_FormatTimeUSEnglish() to format the date in US English format,
	   then figure out what our local GMT offset is, and append it (since
	   PR_FormatTimeUSEnglish() can't do that.) Generate four digit years as
	   per RFC 1123 (superceding RFC 822.)
	 */
	PR_FormatTimeUSEnglish(buffer_tail, 100,
						   "Date: %a, %d %b %Y %H:%M:%S ",
						   &now);

	buffer_tail += PL_strlen (buffer_tail);
	PR_snprintf(buffer_tail, buffer + size - buffer_tail,
				"%c%02d%02d" CRLF,
				(gmtoffset >= 0 ? '+' : '-'),
				((gmtoffset >= 0 ? gmtoffset : -gmtoffset) / 60),
				((gmtoffset >= 0 ? gmtoffset : -gmtoffset) % 60));
	buffer_tail += PL_strlen (buffer_tail);

	if (pFrom && *pFrom) {
		char *convbuf;
		PUSH_STRING ("From: ");
		convbuf = INTL_EncodeMimePartIIStr((char *)pFrom, charset,
										mime_headers_use_quoted_printable_p);
		if (convbuf) {    /* MIME-PartII conversion */
			PUSH_STRING (convbuf);
			PR_Free(convbuf);
		}
		else
			PUSH_STRING (pFrom);
		PUSH_NEWLINE ();
	}

	if (pReplyTo && *pReplyTo)
	{
		char *convbuf;
		PUSH_STRING ("Reply-To: ");
		convbuf = INTL_EncodeMimePartIIStr((char *)pReplyTo, charset,
										mime_headers_use_quoted_printable_p);
		if (convbuf) {     /* MIME-PartII conversion */
			PUSH_STRING (convbuf);
			PR_Free(convbuf);
		}
		else
			PUSH_STRING (pReplyTo);
		PUSH_NEWLINE ();
	}

	if (pOrg && *pOrg)
	{
		char *convbuf;
		PUSH_STRING ("Organization: ");
		convbuf = INTL_EncodeMimePartIIStr((char *)pOrg, charset,
										mime_headers_use_quoted_printable_p);
		if (convbuf) {     /* MIME-PartII conversion */
			PUSH_STRING (convbuf);
			PR_Free(convbuf);
		}
		else
			PUSH_STRING (pOrg);
		PUSH_NEWLINE ();
	}

	// X-Sender tag
	if (fields->GetOwner())
	{
		PRBool bUseXSender = PR_FALSE;

		PREF_GetBoolPref("mail.use_x_sender", &bUseXSender);
		if (bUseXSender) {
			char *convbuf;
			char tmpBuffer[256];
			int bufSize = 256;

			*tmpBuffer = 0;

			PUSH_STRING ("X-Sender: ");

			PUSH_STRING("\"");

			PREF_GetCharPref("mail.identity.username", tmpBuffer, &bufSize);
			convbuf = INTL_EncodeMimePartIIStr((char *)tmpBuffer, charset,
										mime_headers_use_quoted_printable_p);
			if (convbuf) {     /* MIME-PartII conversion */
				PUSH_STRING (convbuf);
				PR_Free(convbuf);
			}
			else
				PUSH_STRING (tmpBuffer);

			PUSH_STRING("\" <");

			PREF_GetCharPref("mail.smtp_name", tmpBuffer, &bufSize);
			convbuf = INTL_EncodeMimePartIIStr((char *)tmpBuffer, charset,
											mime_headers_use_quoted_printable_p);
			if (convbuf) {     /* MIME-PartII conversion */
				PUSH_STRING (convbuf);
				PR_Free(convbuf);
			}
			else
				PUSH_STRING (tmpBuffer);

			PUSH_STRING ("@");

			PREF_GetCharPref("network.hosts.smtp_server", tmpBuffer, &bufSize);
			convbuf = INTL_EncodeMimePartIIStr((char *)tmpBuffer, charset,
											mime_headers_use_quoted_printable_p);
			if (convbuf) {     /* MIME-PartII conversion */
				PUSH_STRING (convbuf);
				PR_Free(convbuf);
			}
			else
				PUSH_STRING (tmpBuffer);

			PUSH_STRING(">");

			convbuf = INTL_EncodeMimePartIIStr((char *)tmpBuffer, charset,
											mime_headers_use_quoted_printable_p);

			if (!fields->GetOwner()->GetMaster()->IsUserAuthenticated())
				PUSH_STRING (" (Unverified)");
			PUSH_NEWLINE ();
		}
	}

	// X-Mozilla-Draft-Info
	if (isDraft) {
		char *htmlAction = 0;
		char *lineWidth = 0; // force plain text hard line break info

		PUSH_STRING(X_MOZILLA_DRAFT_INFO);
		PUSH_STRING(": internal/draft; ");
		if (fields->GetAttachVCard())
			PUSH_STRING("vcard=1");
		else
			PUSH_STRING("vcard=0");
		PUSH_STRING("; ");
		if (fields->GetReturnReceipt()) {
			char *type = PR_smprintf("%d", (int) fields->GetReturnReceiptType());
			if (type)
			{
				PUSH_STRING("receipt=");
				PUSH_STRING(type);
				PR_FREEIF(type);
			}
		}
		else
			PUSH_STRING("receipt=0");
		PUSH_STRING("; ");
		if (fields->GetBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK))
		  PUSH_STRING("uuencode=1");
		else
		  PUSH_STRING("uuencode=0");

		htmlAction = PR_smprintf("html=%d", 
		  ((nsMsgCompose*)fields->GetOwner())->GetHTMLAction());
		if (htmlAction) {
			PUSH_STRING("; ");
			PUSH_STRING(htmlAction);
			PR_FREEIF(htmlAction);
		}

		lineWidth = PR_smprintf("; linewidth=%d",
		  ((nsMsgCompose*)fields->GetOwner())->GetLineWidth());
		if (lineWidth) {
			PUSH_STRING(lineWidth);
			PR_FREEIF(lineWidth);
		}
		PUSH_NEWLINE ();
	}

	nsINetService * pNetService;
	nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, "netlib.dll", PR_FALSE, PR_FALSE); /*JFD - Should go away when netlib will register itself! */
	nsresult rv = nsServiceManager::GetService(kNetServiceCID, nsINetService::GetIID(), (nsISupports **)&pNetService);
	if (NS_SUCCEEDED(rv) && pNetService)
	{
		nsString aNSStr;
		char* sCStr;

		pNetService->GetAppCodeName(aNSStr);
		sCStr = aNSStr.ToNewCString();
		if (sCStr) {
			PUSH_STRING ("X-Mailer: ");
			PUSH_STRING(sCStr);
			delete [] sCStr;

			pNetService->GetAppVersion(aNSStr);
			sCStr = aNSStr.ToNewCString();
			if (sCStr) {
				PUSH_STRING (" ");
				PUSH_STRING(sCStr);
				delete [] sCStr;
			}
			PUSH_NEWLINE ();
		}

		pNetService->Release();
	}

	/* for Netscape Server, Accept-Language data sent in Mail header */
	char *acceptlang = INTL_GetAcceptLanguage();
	if( (acceptlang != NULL) && ( *acceptlang != '\0') ){
		PUSH_STRING( "X-Accept-Language: " );
		PUSH_STRING( acceptlang );
		PUSH_NEWLINE();
	}

	PUSH_STRING ("MIME-Version: 1.0" CRLF);

	if (pNewsGrp && *pNewsGrp) {
		/* turn whitespace into a comma list
		*/
		char *ptr, *ptr2;
		char *n2;
		char *convbuf;

		convbuf = INTL_EncodeMimePartIIStr((char *)pNewsGrp, charset,
										mime_headers_use_quoted_printable_p);
		if (convbuf)
	  		n2 = XP_StripLine (convbuf);
		else {
			ptr = PL_strdup(pNewsGrp);
			if (!ptr) {
				PR_FREEIF(buffer);
				return 0; /* MK_OUT_OF_MEMORY */
			}
  	  		n2 = XP_StripLine(ptr);
			NS_ASSERTION(n2 == ptr, "n2 != ptr");	/* Otherwise, the PR_Free below is
													   gonna choke badly. */
		}

		for(ptr=n2; *ptr != '\0'; ptr++) {
			/* find first non white space */
			while(!XP_IS_SPACE(*ptr) && *ptr != ',' && *ptr != '\0')
				ptr++;

			if(*ptr == '\0')
				break;

			if(*ptr != ',')
				*ptr = ',';

			/* find next non white space */
			ptr2 = ptr+1;
			while(XP_IS_SPACE(*ptr2))
				ptr2++;

			if(ptr2 != ptr+1)
				PL_strcpy(ptr+1, ptr2);
		}

		PUSH_STRING ("Newsgroups: ");
		PUSH_STRING (n2);
		PR_Free (n2);
		PUSH_NEWLINE ();
	}

	/* #### shamelessly duplicated from above */
	if (pFollow && *pFollow) {
		/* turn whitespace into a comma list
		*/
		char *ptr, *ptr2;
		char *n2;
		char *convbuf;

		convbuf = INTL_EncodeMimePartIIStr((char *)pFollow, charset,
										mime_headers_use_quoted_printable_p);
		if (convbuf)
			n2 = XP_StripLine (convbuf);
		else {
			ptr = PL_strdup(pFollow);
			if (!ptr) {
				PR_FREEIF(buffer);
				return 0; /* MK_OUT_OF_MEMORY */
			}
			n2 = XP_StripLine (ptr);
			NS_ASSERTION(n2 == ptr, "n2 != ptr");	/* Otherwise, the PR_Free below is
													   gonna choke badly. */
		}

		for (ptr=n2; *ptr != '\0'; ptr++) {
			/* find first non white space */
			while(!XP_IS_SPACE(*ptr) && *ptr != ',' && *ptr != '\0')
				ptr++;

			if(*ptr == '\0')
				break;

			if(*ptr != ',')
				*ptr = ',';

			/* find next non white space */
			ptr2 = ptr+1;
			while(XP_IS_SPACE(*ptr2))
				ptr2++;

			if(ptr2 != ptr+1)
				PL_strcpy(ptr+1, ptr2);
		}

		PUSH_STRING ("Followup-To: ");
		PUSH_STRING (n2);
		PR_Free (n2);
		PUSH_NEWLINE ();
	}

	if (pTo && *pTo) {
		char *convbuf;
		PUSH_STRING ("To: ");
		convbuf = INTL_EncodeMimePartIIStr((char *)pTo, charset,
										mime_headers_use_quoted_printable_p);
		if (convbuf) {     /* MIME-PartII conversion */
			PUSH_STRING (convbuf);
			PR_Free(convbuf);
		}
		else
			PUSH_STRING (pTo);

		PUSH_NEWLINE ();
	}
 
	if (pCc && *pCc) {
		char *convbuf;
		PUSH_STRING ("CC: ");
		convbuf = INTL_EncodeMimePartIIStr((char *)pCc, charset,
										mime_headers_use_quoted_printable_p);
		if (convbuf) {   /* MIME-PartII conversion */
			PUSH_STRING (convbuf);
			PR_Free(convbuf);
		}
		else
			PUSH_STRING (pCc);
		PUSH_NEWLINE ();
	}

	if (pSubject && *pSubject) {
		char *convbuf;
		PUSH_STRING ("Subject: ");
		convbuf = INTL_EncodeMimePartIIStr((char *)pSubject, charset,
										mime_headers_use_quoted_printable_p);
		if (convbuf) {  /* MIME-PartII conversion */
			PUSH_STRING (convbuf);
			PR_Free(convbuf);
		}
		else
			PUSH_STRING (pSubject);
		PUSH_NEWLINE ();
	}
	
	if (pPriority && *pPriority)
		if (!strcasestr(pPriority, "normal")) {
			PUSH_STRING ("X-Priority: ");
			/* Important: do not change the order of the 
			* following if statements
			*/
			if (strcasestr (pPriority, "highest"))
				PUSH_STRING("1 (");
			else if (strcasestr(pPriority, "high"))
				PUSH_STRING("2 (");
			else if (strcasestr(pPriority, "lowest"))
				PUSH_STRING("5 (");
			else if (strcasestr(pPriority, "low"))
				PUSH_STRING("4 (");

			PUSH_STRING (pPriority);
			PUSH_STRING(")");
			PUSH_NEWLINE ();
		}

	if (pReference && *pReference) {
		PUSH_STRING ("References: ");
		if (PL_strlen(pReference) >= 986) {
			char *references = PL_strdup(pReference);
			char *trimAt = PL_strchr(references+1, '<');
			char *ptr;
			// per sfraser, RFC 1036 - a message header line should not exceed
			// 998 characters including the header identifier
			// retiring the earliest reference one after the other
			// but keep the first one for proper threading
			while (references && PL_strlen(references) >= 986 && trimAt) {
				ptr = PL_strchr(trimAt+1, '<');
				if (ptr)
				  XP_MEMMOVE(trimAt, ptr, PL_strlen(ptr)+1); // including the
				else
				  break;
			}
			NS_ASSERTION(references, "null references");
			if (references) {
			  PUSH_STRING (references);
			  PR_Free(references);
			}
		}
		else
		  PUSH_STRING (pReference);
		PUSH_NEWLINE ();
	}

	if (pOtherHdr && *pOtherHdr) {
		/* Assume they already have the right newlines and continuations
		 and so on. */
		PUSH_STRING (pOtherHdr);
	}

	if (buffer_tail > buffer + size - 1)
		abort ();

	/* realloc it smaller... */
	buffer = (char*)PR_REALLOC (buffer, buffer_tail - buffer + 1);

	return buffer;
}


/* Generate headers for a form post to a mailto: URL.
   This lets the URL specify additional headers, but is careful to
   ignore headers which would be dangerous.  It may modify the URL
   (because of CC) so a new URL to actually post to is returned.
 */
int MIME_GenerateMailtoFormPostHeaders (const char *old_post_url,
									const char * /*referer*/,
									char **new_post_url_return,
									char **headers_return)
{
  char *from = 0, *to = 0, *cc = 0, *body = 0, *search = 0;
  char *extra_headers = 0;
  char *s;
  PRBool subject_p = PR_FALSE;
  PRBool sign_p = PR_FALSE, encrypt_p = PR_FALSE;
  char *rest;
  int status = 0;
  nsMsgCompFields *fields = NULL;
  static const char *forbidden_headers[] = {
	"Apparently-To",
	"BCC",
	"Content-Encoding",
	CONTENT_LENGTH,
	"Content-Transfer-Encoding",
	"Content-Type",
	"Date",
	"Distribution",
	"FCC",
	"Followup-To",
	"From",
	"Lines",
	"MIME-Version",
	"Message-ID",
	"Newsgroups",
	"Organization",
	"Reply-To",
	"Sender",
	X_MOZILLA_STATUS,
	X_MOZILLA_STATUS2,
	X_MOZILLA_NEWSHOST,
	X_UIDL,
	"XRef",
	0 };

  from = MIME_MakeFromField (CS_DEFAULT);
  if (!from) {
	status = MK_OUT_OF_MEMORY;
	goto FAIL;
  }

  to = NET_ParseURL (old_post_url, GET_PATH_PART);
  if (!to) {
	status = MK_OUT_OF_MEMORY;
	goto FAIL;
  }

  if (!*to)
	{
	  status = MK_MIME_NO_RECIPIENTS; /* rb -1; */
	  goto FAIL;
	}

  search = NET_ParseURL (old_post_url, GET_SEARCH_PART);

  rest = search;
  if (rest && *rest == '?')
	{
	  /* start past the '?' */
	  rest++;
	  rest = XP_STRTOK (rest, "&");
	  while (rest && *rest)
		{
		  char *token = rest;
		  char *value = 0;
		  char *eq;

		  rest = XP_STRTOK (0, "&");

		  eq = PL_strchr (token, '=');
		  if (eq)
			{
			  value = eq+1;
			  *eq = 0;
			}

		  if (!PL_strcasecmp (token, "subject"))
			subject_p = PR_TRUE;

		  if (value)
			/* Don't allow newlines or control characters in the value. */
			for (s = value; *s; s++)
			  if (*s < ' ' && *s != '\t')
				*s = ' ';

		  if (!PL_strcasecmp (token, "to"))
			{
			  if (to && *to)
				{
				  StrAllocCat (to, ", ");
				  StrAllocCat (to, value);
				}
			  else
				{
				  PR_FREEIF(to);
				  to = PL_strdup(value);
				}
			}
		  else if (!PL_strcasecmp (token, "cc"))
			{
			  if (cc && *cc)
				{
				  StrAllocCat (cc, ", ");
				  StrAllocCat (cc, value);
				}
			  else
				{
				  PR_FREEIF(cc);
				  cc = PL_strdup (value);
				}
			}
		  else if (!PL_strcasecmp (token, "body"))
			{
			  if (body && *body)
				{
				  StrAllocCat (body, "\n");
				  StrAllocCat (body, value);
				}
			  else
				{
				  PR_FREEIF(body);
				  body = PL_strdup (value);
				}
			}
		  else if (!PL_strcasecmp (token, "encrypt") ||
				   !PL_strcasecmp (token, "encrypted"))
			{
			  encrypt_p = (!PL_strcasecmp(value, "true") ||
						   !PL_strcasecmp(value, "yes"));
			}
		  else if (!PL_strcasecmp (token, "sign") ||
				   !PL_strcasecmp (token, "signed"))
			{
			  sign_p = (!PL_strcasecmp(value, "true") ||
						!PL_strcasecmp(value, "yes"));
			}
		  else
			{
			  const char **fh = forbidden_headers;
			  PRBool ok = PR_TRUE;
			  while (*fh)
				if (!PL_strcasecmp (token, *fh++))
				  {
					ok = PR_FALSE;
					break;
				  }
			  if (ok)
				{
				  PRBool upper_p = PR_FALSE;
				  char *s;
				  for (s = token; *s; s++)
					{
					  if (*s >= 'A' && *s <= 'Z')
						upper_p = PR_TRUE;
					  else if (*s <= ' ' || *s >= '~' || *s == ':')
						goto NOT_OK;  /* bad character in header! */
					}
				  if (!upper_p && *token >= 'a' && *token <= 'z')
					*token -= ('a' - 'A');

				  StrAllocCat (extra_headers, token);
				  StrAllocCat (extra_headers, ": ");
				  if (value)
					StrAllocCat (extra_headers, value);
				  StrAllocCat (extra_headers, CRLF);
				NOT_OK: ;
				}
			}
		}
	}

  if (!subject_p)
	{
		char* sAppName = nsnull;

		nsINetService * pNetService;
//		nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, "netlib.dll", PR_FALSE, PR_FALSE); /*JFD - Should go away when netlib will register itself! */
		nsresult rv = nsServiceManager::GetService(kNetServiceCID, nsINetService::GetIID(), (nsISupports **)&pNetService);
		if (NS_SUCCEEDED(rv) && pNetService)
		{
			nsString aNSStr;

			pNetService->GetAppCodeName(aNSStr);
			sAppName = aNSStr.ToNewCString();

			pNetService->Release();
		}
	  /* If the URL didn't provide a subject, we will. */
	  StrAllocCat (extra_headers, "Subject: Form posted from ");
	  NS_ASSERTION (sAppName, "null XP_AppCodeName");
	  StrAllocCat (extra_headers, sAppName);
	  StrAllocCat (extra_headers, CRLF);
	}

  /* Note: the `encrypt', `sign', and `body' parameters are currently
	 ignored in mailto form submissions. */

  *new_post_url_return = 0;

 /*JFD
 	nsMsgCompPrefs pCompPrefs;
	fields = MSG_CreateCompositionFields(from, 0, to, cc, 0, 0, 0, 0,
									   (char *)pCompPrefs.GetOrganization(), 0, 0,
									   extra_headers, 0, 0, 0,
									   encrypt_p, sign_p);
 */
  if (!fields)
  {
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
  }

  fields->SetDefaultBody(body, NULL);

  *headers_return = mime_generate_headers (fields, 0, MSG_DeliverNow);
  if (*headers_return == 0)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  StrAllocCat ((*new_post_url_return), "mailto:");
  if (to)
	StrAllocCat ((*new_post_url_return), to);
  if (to && cc)
	StrAllocCat ((*new_post_url_return), ",");
  if (cc)
	StrAllocCat ((*new_post_url_return), cc);

 FAIL:
  PR_FREEIF (from);
  PR_FREEIF (to);
  PR_FREEIF (cc);
  PR_FREEIF (body);
  PR_FREEIF (search);
  PR_FREEIF (extra_headers);
 /*JFD
 if (fields)
	  MSG_DestroyCompositionFields(fields);
*/

  return status;
}



static char * mime_generate_attachment_headers (const char *type, const char *encoding,
								  const char *description,
								  const char *x_mac_type,
								  const char *x_mac_creator,
								  const char *real_name,
								  const char *base_url,
								  PRBool /*digest_p*/,
								  MSG_DeliverMimeAttachment * /*ma*/,
								  const char *charset)
{
	PRInt32 buffer_size = 2048 + (base_url ? 2*PL_strlen(base_url) : 0);
	char *buffer = (char *) PR_Malloc (buffer_size);
	char *buffer_tail = buffer;

	if (! buffer)
		return 0; /* MK_OUT_OF_MEMORY */

	NS_ASSERTION (encoding, "null encoding");

	PUSH_STRING ("Content-Type: ");
	PUSH_STRING (type);

	if (mime_type_needs_charset (type)) {

	  char charset_label[65];   // Content-Type: charset
    PL_strcpy(charset_label, charset);

		/* If the characters are all 7bit, then it's better (and true) to
		claim the charset to be US-  rather than Latin1.  Should we
		do this all the time, for all charsets?  I'm not sure.  But we
		should definitely do it for Latin1. */
		if (encoding &&
				!PL_strcasecmp (encoding, "7bit") &&
				!PL_strcasecmp (charset, "iso-8859-1"))
			PL_strcpy (charset_label, "us-ascii");

		// If charset is JIS and and type is HTML
		// then no charset to be specified (apply base64 instead)
		// in order to avoid mismatch META_TAG (bug#104255).
		if ((PL_strcasecmp(charset, "iso-2022-jp") != 0) ||
				(PL_strcasecmp(type, TEXT_HTML) != 0) ||
				(PL_strcasecmp(encoding, ENCODING_BASE64) != 0)) {
			PUSH_STRING ("; charset=");
			PUSH_STRING (charset_label);
		}
	}

	if (x_mac_type && *x_mac_type) {
		PUSH_STRING ("; x-mac-type=\"");
		PUSH_STRING (x_mac_type);
		PUSH_STRING ("\"");
	}

	if (x_mac_creator && *x_mac_creator) {
		PUSH_STRING ("; x-mac-creator=\"");
		PUSH_STRING (x_mac_creator);
		PUSH_STRING ("\"");
	}

	PRInt32 parmFolding = 0;
	PREF_GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

#ifdef EMIT_NAME_IN_CONTENT_TYPE
	if (real_name && *real_name) {
		if (parmFolding == 0 || parmFolding == 1) {
			PUSH_STRING (";\r\n name=\"");
			PUSH_STRING (real_name);
			PUSH_STRING ("\"");
		}
		else // if (parmFolding == 2)
		{
			char *rfc2231Parm = RFC2231ParmFolding("name", charset,
												 INTL_GetAcceptLanguage(), real_name);
			if (rfc2231Parm) {
				PUSH_STRING(";\r\n ");
				PUSH_STRING(rfc2231Parm);
				PR_Free(rfc2231Parm);
			}
		}
	}
#endif /* EMIT_NAME_IN_CONTENT_TYPE */

	PUSH_NEWLINE ();

	PUSH_STRING ("Content-Transfer-Encoding: ");
	PUSH_STRING (encoding);
	PUSH_NEWLINE ();

	if (description && *description) {
		char *s = mime_fix_header (description);
		if (s) {
			PUSH_STRING ("Content-Description: ");
			PUSH_STRING (s);
			PUSH_NEWLINE ();
			PR_Free(s);
		}
	}

	if (real_name && *real_name) {
		char *period = PL_strrchr(real_name, '.');
		PRInt32 pref_content_disposition = 0;

		PREF_GetIntPref("mail.content_disposition_type", &pref_content_disposition);
		PUSH_STRING ("Content-Disposition: ");

		if (pref_content_disposition == 1)
			PUSH_STRING ("attachment");
		else
			if (pref_content_disposition == 2 && 
					(!PL_strcasecmp(type, TEXT_PLAIN) || 
					(period && !PL_strcasecmp(period, ".txt"))))
				PUSH_STRING("attachment");

			/* If this document is an anonymous binary file or a vcard, 
			then always show it as an attachment, never inline. */
			else
				if (!PL_strcasecmp(type, APPLICATION_OCTET_STREAM) || 
						!PL_strcasecmp(type, TEXT_VCARD) ||
						!PL_strcasecmp(type, APPLICATION_DIRECTORY)) /* text/x-vcard synonym */
					PUSH_STRING ("attachment");
				else
					PUSH_STRING ("inline");

		if (parmFolding == 0 || parmFolding == 1) {
			PUSH_STRING (";\r\n filename=\"");
			PUSH_STRING (real_name);
			PUSH_STRING ("\"" CRLF);
		}
		else // if (parmFolding == 2)
		{
			char *rfc2231Parm = RFC2231ParmFolding("filename", charset,
											 INTL_GetAcceptLanguage(), real_name);
			if (rfc2231Parm) {
				PUSH_STRING(";\r\n ");
				PUSH_STRING(rfc2231Parm);
				PUSH_NEWLINE ();
				PR_Free(rfc2231Parm);
			}
		}
	}
	else
		if (type &&
				(!PL_strcasecmp (type, MESSAGE_RFC822) ||
				!PL_strcasecmp (type, MESSAGE_NEWS)))
			PUSH_STRING ("Content-Disposition: inline" CRLF);
  
#ifdef GENERATE_CONTENT_BASE
	/* If this is an HTML document, and we know the URL it originally
	   came from, write out a Content-Base header. */
	if (type &&
			(!PL_strcasecmp (type, TEXT_HTML) ||
			!PL_strcasecmp (type, TEXT_MDL)) &&
			base_url && *base_url)
	{
		PRInt32 col = 0;
		const char *s = base_url;
		const char *colon = PL_strchr (s, ':');
		PRBool useContentLocation = PR_FALSE;   /* rhp - add this  */

		if (!colon)
			goto GIVE_UP_ON_CONTENT_BASE;  /* malformed URL? */

		/* Don't emit a content-base that points to (or into) a news or
		   mail message. */
		if (!PL_strncasecmp (s, "news:", 5) ||
				!PL_strncasecmp (s, "snews:", 6) ||
				!PL_strncasecmp (s, "IMAP:", 5) ||
				!PL_strncasecmp (s, "file:", 5) ||    /* rhp: fix targets from included HTML files */
				!PL_strncasecmp (s, "mailbox:", 8))
			goto GIVE_UP_ON_CONTENT_BASE;

		/* rhp - Put in a pref for using Content-Location instead of Content-Base.
			     This will get tweaked to default to true in 5.0
		*/
		PREF_GetBoolPref("mail.use_content_location_on_send", &useContentLocation);

		if (useContentLocation)
			PUSH_STRING ("Content-Location: \"");
		else
			PUSH_STRING ("Content-Base: \"");
		/* rhp - Pref for Content-Location usage */

/* rhp: this is to work with the Content-Location stuff */
CONTENT_LOC_HACK:

		while (*s != 0 && *s != '#')
		{
			const char *ot = buffer_tail;

			/* URLs must be wrapped at 40 characters or less. */
			if (col >= 38) {
				PUSH_STRING(CRLF "\t");
				col = 0;
			}

			if (*s == ' ')
				PUSH_STRING("%20");
			else if (*s == '\t')
				PUSH_STRING("%09");
			else if (*s == '\n')
				PUSH_STRING("%0A");
			else if (*s == '\r')
				PUSH_STRING("%0D");
			else {
				*buffer_tail++ = *s;
				*buffer_tail = '\0';
			}
			s++;
			col += (buffer_tail - ot);
		}
		PUSH_STRING ("\"" CRLF);

		/* rhp: this is to try to get around this fun problem with Content-Location */
		if (!useContentLocation) {
			PUSH_STRING ("Content-Location: \"");
			s = base_url;
			col = 0;
			useContentLocation = PR_TRUE;
			goto CONTENT_LOC_HACK;
		}
		/* rhp: this is to try to get around this fun problem with Content-Location */

GIVE_UP_ON_CONTENT_BASE:
		;
	}
#endif /* GENERATE_CONTENT_BASE */


	/* realloc it smaller... */
	buffer = (char*) PR_REALLOC (buffer, buffer_tail - buffer + 1);

	return buffer;
}


void nsMsgSendMimeDeliveryState::Fail (int failure_code, char *error_msg)
{
  if (m_message_delivery_done_callback)
	{
	  if (failure_code < 0 && !error_msg)
		error_msg = NET_ExplainErrorDetails(failure_code, 0, 0, 0, 0);
	  m_message_delivery_done_callback (GetContext(), m_fe_data,
										failure_code, error_msg);
	  PR_FREEIF(error_msg);		/* #### Is there a memory leak here?  Shouldn't
								   this free be outside the if? */
	}
  else if (m_attachments_done_callback)
	{
	  if (failure_code < 0 && !error_msg)
		error_msg = NET_ExplainErrorDetails(failure_code, 0, 0, 0, 0);

	  /* mime_free_message_state will take care of cleaning up the
		 attachment files and attachment structures */
	  m_attachments_done_callback (GetContext(),
								   m_fe_data, failure_code,
								   error_msg, 0);

	  PR_FREEIF(error_msg);		/* #### Is there a memory leak here?  Shouldn't
								   this free be outside the if? */
	}

  m_message_delivery_done_callback = 0;
  m_attachments_done_callback = 0;
  
  Clear();
}

/* Given a string, convert it to 'qtext' (quoted text) for RFC822 header purposes. */
static char *
msg_make_filename_qtext(const char *srcText, PRBool stripCRLFs)
{
	/* newString can be at most twice the original string (every char quoted). */
	char *newString = (char *) PR_Malloc(PL_strlen(srcText)*2 + 1);
	if (!newString) return NULL;

	const char *s = srcText;
	const char *end = srcText + PL_strlen(srcText);
	char *d = newString;

	while(*s)
	{
		/* 	Put backslashes in front of existing backslashes, or double quote
			characters.
			If stripCRLFs is true, don't write out CRs or LFs. Otherwise,
			write out a backslash followed by the CR but not
			linear-white-space.
			We might already have quoted pair of "\ " or "\\t" skip it. 
			*/
		if (*s == '\\' || *s == '"' || 
			(!stripCRLFs && 
			 (*s == CR && (*(s+1) != LF || 
						   (*(s+1) == LF && (s+2) < end && !XP_IS_SPACE(*(s+2)))))))
			*d++ = '\\';

		if (*s == CR)
		{
			if (stripCRLFs && *(s+1) == LF && (s+2) < end && XP_IS_SPACE(*(s+2)))
				s += 2;			// skip CRLFLWSP
		}
		else
		{
			*d++ = *s;
		}
		s++;
	}
	*d = 0;

	return newString;
}

/* Rip apart the URL and extract a reasonable value for the `real_name' slot.
 */
static void
msg_pick_real_name (MSG_DeliverMimeAttachment *attachment, const char *charset)
{
  const char *s, *s2;
  char *s3;
  char *url;

  if (attachment->m_real_name)
	return;

  url = attachment->m_url_string;

  /* Perhaps the MIME parser knows a better name than the URL itself?
	 This can happen when one attaches a MIME part from one message
	 directly into another message.
	 
	 ### mwelch Note that this function simply duplicates and returns an existing
	 			MIME header, so we don't need to process it. */
  attachment->m_real_name =
	MimeGuessURLContentName(attachment->m_mime_delivery_state->GetContext(),
							url);
  if (attachment->m_real_name)
	return;

  /* Otherwise, extract a name from the URL. */

  s = url;
  s2 = PL_strchr (s, ':');
  if (s2) s = s2 + 1;
  /* If we know the URL doesn't have a sensible file name in it,
	 don't bother emitting a content-disposition. */
  if (!PL_strncasecmp (url, "news:", 5) ||
	  !PL_strncasecmp (url, "snews:", 6) ||
	  !PL_strncasecmp (url, "IMAP:", 5) ||
	  !PL_strncasecmp (url, "mailbox:", 8))
	return;

  /* Take the part of the file name after the last / or \ */
  s2 = PL_strrchr (s, '/');
  if (s2) s = s2+1;
  s2 = PL_strrchr (s, '\\');
  
#if 0
//TODO: convert to unicode to do the truncation
  if (csid & MULTIBYTE)
  {
	  // We don't want to truncate the file name in case of the double
	  // byte file name
	  while ( s2 != NULL && 
			  s2 > s &&
			  INTL_IsLeadByte(csid, *(s2-1)))
	  {
		  s3 = (char *) s2;
		  *s3 = 0;
		  s2 = PL_strrchr(s, '\\');
		  *s3 = '\\';
	  }
  }
#endif
	  
  if (s2) s = s2+1;
  /* Copy it into the attachment struct. */
  PR_FREEIF(attachment->m_real_name);
  attachment->m_real_name = PL_strdup (s);
  /* Now trim off any named anchors or search data. */
  s3 = PL_strchr (attachment->m_real_name, '?');
  if (s3) *s3 = 0;
  s3 = PL_strchr (attachment->m_real_name, '#');
  if (s3) *s3 = 0;

  /* Now lose the %XX crap. */
  NET_UnEscape (attachment->m_real_name);

  PRInt32 parmFolding = 0;
  PREF_GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

  if (parmFolding == 0 || parmFolding == 1)
  {
	  /* Try to MIME-2 encode the filename... */
	  char *mime2Name = INTL_EncodeMimePartIIStr(attachment->m_real_name, charset, 
												mime_headers_use_quoted_printable_p);
	  if (mime2Name && (mime2Name != attachment->m_real_name))
	  {
		  PR_Free(attachment->m_real_name);
		  attachment->m_real_name = mime2Name;
	  }
   
	  /* ... and then put backslashes before special characters (RFC 822 tells us
		 to). */

	  char *qtextName = NULL;
	  
	  qtextName = msg_make_filename_qtext(attachment->m_real_name, 
										  (parmFolding == 0 ? PR_TRUE : PR_FALSE));
		  
	  if (qtextName)
	  {
		  PR_Free(attachment->m_real_name);
		  attachment->m_real_name = qtextName;
	  }
  }
  
  /* Now a special case for attaching uuencoded files...

	 If we attach a file "foo.txt.uu", we will send it out with
	 Content-Type: text/plain; Content-Transfer-Encoding: x-uuencode.
	 When saving such a file, a mail reader will generally decode it first
	 (thus removing the uuencoding.)  So, let's make life a little easier by
	 removing the indication of uuencoding from the file name itself.  (This
	 will presumably make the file name in the Content-Disposition header be
	 the same as the file name in the "begin" line of the uuencoded data.)

	 However, since there are mailers out there (including earlier versions of
	 Mozilla) that will use "foo.txt.uu" as the file name, we still need to
	 cope with that; the code which copes with that is in the MIME parser, in
	 libmime/mimei.c.
   */
  if (attachment->m_already_encoded_p &&
	  attachment->m_encoding)
	{
	  char *result = attachment->m_real_name;
	  PRInt32 L = PL_strlen(result);
	  const char **exts = 0;

	  /* #### TOTAL KLUDGE.
		 I'd like to ask the mime.types file, "what extensions correspond
		 to obj->encoding (which happens to be "x-uuencode") but doing that
		 in a non-sphagetti way would require brain surgery.  So, since
		 currently uuencode is the only content-transfer-encoding which we
		 understand which traditionally has an extension, we just special-
		 case it here!  

		 Note that it's special-cased in a similar way in libmime/mimei.c.
	   */
	  if (!PL_strcasecmp(attachment->m_encoding, ENCODING_UUENCODE) ||
		  !PL_strcasecmp(attachment->m_encoding, ENCODING_UUENCODE2) ||
		  !PL_strcasecmp(attachment->m_encoding, ENCODING_UUENCODE3) ||
		  !PL_strcasecmp(attachment->m_encoding, ENCODING_UUENCODE4))
		{
		  static const char *uue_exts[] = { "uu", "uue", 0 };
		  exts = uue_exts;
		}

	  while (exts && *exts)
		{
		  const char *ext = *exts;
		  PRInt32 L2 = PL_strlen(ext);
		  if (L > L2 + 1 &&							/* long enough */
			  result[L - L2 - 1] == '.' &&			/* '.' in right place*/
			  !PL_strcasecmp(ext, result + (L - L2)))	/* ext matches */
			{
			  result[L - L2 - 1] = 0;		/* truncate at '.' and stop. */
			  break;
			}
		  exts++;
		}
	}
}


int nsMsgSendMimeDeliveryState::HackAttachments(
						  const struct MSG_AttachmentData *attachments,
						  const struct MSG_AttachedFile *preloaded_attachments)
{
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(GetContext());
	if (preloaded_attachments)
		NS_ASSERTION(!attachments, "not-null attachments");
	if (attachments)
		NS_ASSERTION(!preloaded_attachments, "not-null preloaded attachments");

	if (preloaded_attachments && preloaded_attachments[0].orig_url) {
		/* These are attachments which have already been downloaded to tmp files.
		We merely need to point the internal attachment data at those tmp
		files.
		*/
		PRInt32 i;

		m_pre_snarfed_attachments_p = PR_TRUE;

		m_attachment_count = 0;
		while (preloaded_attachments[m_attachment_count].orig_url)
			m_attachment_count++;

		m_attachments = (MSG_DeliverMimeAttachment *)
		new MSG_DeliverMimeAttachment[m_attachment_count];

		if (! m_attachments)
			return MK_OUT_OF_MEMORY;

		for (i = 0; i < m_attachment_count; i++) {
			m_attachments[i].m_mime_delivery_state = this;
			/* These attachments are already "snarfed". */
			m_attachments[i].m_done = PR_TRUE;
			NS_ASSERTION (preloaded_attachments[i].orig_url, "null url");
			PR_FREEIF(m_attachments[i].m_url_string);
			m_attachments[i].m_url_string = PL_strdup (preloaded_attachments[i].orig_url);
			PR_FREEIF(m_attachments[i].m_type);
			m_attachments[i].m_type = PL_strdup (preloaded_attachments[i].type);
			PR_FREEIF(m_attachments[i].m_charset);
			m_attachments[i].m_charset = PL_strdup (m_fields->GetCharacterSet());
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
			PR_FREEIF(m_attachments[i].m_file_name);
			m_attachments[i].m_file_name = PL_strdup (preloaded_attachments[i].file_name);

			m_attachments[i].m_size = preloaded_attachments[i].size;
			m_attachments[i].m_unprintable_count =
			preloaded_attachments[i].unprintable_count;
			m_attachments[i].m_highbit_count =
			preloaded_attachments[i].highbit_count;
			m_attachments[i].m_ctl_count = preloaded_attachments[i].ctl_count;
			m_attachments[i].m_null_count =
			preloaded_attachments[i].null_count;
			m_attachments[i].m_max_column =
			preloaded_attachments[i].max_line_length;

			/* If the attachment has an encoding, and it's not one of
			the "null" encodings, then keep it. */
			if (m_attachments[i].m_encoding &&
					PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_7BIT) &&
					PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_8BIT) &&
					PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_BINARY))
				m_attachments[i].m_already_encoded_p = PR_TRUE;

			msg_pick_real_name(&m_attachments[i], m_fields->GetCharacterSet());
		}
	}
	else 
		if (attachments && attachments[0].url) {
		/* These are attachments which have already been downloaded to tmp files.
		We merely need to point the internal attachment data at those tmp
		files.  We will delete the tmp files as we attach them.
		*/
		PRInt32 i;
		int mailbox_count = 0, news_count = 0;

		m_attachment_count = 0;
		while (attachments[m_attachment_count].url)
			m_attachment_count++;

		m_attachments = (MSG_DeliverMimeAttachment *)
		new MSG_DeliverMimeAttachment[m_attachment_count];

		if (! m_attachments)
			return MK_OUT_OF_MEMORY;

		for (i = 0; i < m_attachment_count; i++) {
			m_attachments[i].m_mime_delivery_state = this;
			NS_ASSERTION (attachments[i].url, "null url");
			PR_FREEIF(m_attachments[i].m_url_string);
			m_attachments[i].m_url_string = PL_strdup (attachments[i].url);
			PR_FREEIF(m_attachments[i].m_override_type);
			m_attachments[i].m_override_type = PL_strdup (attachments[i].real_type);
			PR_FREEIF(m_attachments[i].m_charset);
			m_attachments[i].m_charset = PL_strdup (m_fields->GetCharacterSet());
			PR_FREEIF(m_attachments[i].m_override_encoding);
			m_attachments[i].m_override_encoding = PL_strdup (attachments[i].real_encoding);
			PR_FREEIF(m_attachments[i].m_desired_type);
			m_attachments[i].m_desired_type = PL_strdup (attachments[i].desired_type);
			PR_FREEIF(m_attachments[i].m_description);
			m_attachments[i].m_description = PL_strdup (attachments[i].description);
			PR_FREEIF(m_attachments[i].m_real_name);
			m_attachments[i].m_real_name = PL_strdup (attachments[i].real_name);
			PR_FREEIF(m_attachments[i].m_x_mac_type);
			m_attachments[i].m_x_mac_type = PL_strdup (attachments[i].x_mac_type);
			PR_FREEIF(m_attachments[i].m_x_mac_creator);
			m_attachments[i].m_x_mac_creator = PL_strdup (attachments[i].x_mac_creator);
			PR_FREEIF(m_attachments[i].m_encoding);
			m_attachments[i].m_encoding = PL_strdup ("7bit");
			PR_FREEIF(m_attachments[i].m_url);
			m_attachments[i].m_url =
			NET_CreateURLStruct (m_attachments[i].m_url_string,
						 NET_DONT_RELOAD);

			// real name is set in the case of vcard so don't change it.
			// m_attachments[i].m_real_name = 0;

			/* Count up attachments which are going to come from mail folders
			and from NNTP servers. */
			if (PL_strncasecmp(m_attachments[i].m_url_string, "mailbox:",8) ||
					PL_strncasecmp(m_attachments[i].m_url_string, "IMAP:",5))
				mailbox_count++;
			else
				if (PL_strncasecmp(m_attachments[i].m_url_string, "news:",5) ||
						PL_strncasecmp(m_attachments[i].m_url_string, "snews:",6))
					news_count++;

			msg_pick_real_name(&m_attachments[i], m_fields->GetCharacterSet());
		}

		/* If there is more than one mailbox URL, or more than one NNTP url,
		do the load in serial rather than parallel, for efficiency.
		*/
		if (mailbox_count > 1 || news_count > 1)
			m_be_synchronous_p = PR_TRUE;

		m_attachment_pending_count = m_attachment_count;

		/* Start the URL attachments loading (eventually, an exit routine will
		call the done_callback). */

		if (m_attachment_count == 1)
			FE_Progress(GetContext(), XP_GetString(MK_MSG_LOAD_ATTACHMNT));
		else
			FE_Progress(GetContext(), XP_GetString(MK_MSG_LOAD_ATTACHMNTS));

		for (i = 0; i < m_attachment_count; i++) {
			/* This only returns a failure code if NET_GetURL was not called
			(and thus no exit routine was or will be called.) */
			int status = m_attachments [i].SnarfAttachment();
			if (status < 0)
				return status;

			if (m_be_synchronous_p)
				break;
		}
	}

	if (m_attachment_pending_count <= 0)
		/* No attachments - finish now (this will call the done_callback). */
		GatherMimeAttachments ();

	return 0;
}


void nsMsgSendMimeDeliveryState::StartMessageDelivery(
						  MSG_Pane   *pane,
						  void       *fe_data,
						  nsMsgCompFields *fields,
						  PRBool digest_p,
						  PRBool dont_deliver_p,
						  MSG_Deliver_Mode mode,
						  const char *attachment1_type,
						  const char *attachment1_body,
						  PRUint32 attachment1_body_length,
						  const struct MSG_AttachmentData *attachments,
						  const struct MSG_AttachedFile *preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
						  nsMsgSendPart *relatedPart,
//#endif
						  void (*message_delivery_done_callback)
						       (MWContext *context,
								void *fe_data,
								int status,
								const char *error_message))
{
	int failure = 0;

	if (!attachment1_body || !*attachment1_body)
		attachment1_type = attachment1_body = 0;


	failure = Init(pane, fe_data, fields, 
					digest_p, dont_deliver_p, mode,
					attachment1_type, attachment1_body,
					attachment1_body_length,
					attachments, preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
					relatedPart,
//#endif
					message_delivery_done_callback);
	if (failure >= 0)
		return;

	char *err_msg = NET_ExplainErrorDetails (failure);

	if (pane)
		message_delivery_done_callback (pane->GetContext(), fe_data, failure, err_msg);
	if (err_msg)
		PR_Free (err_msg);
}

int nsMsgSendMimeDeliveryState::SetMimeHeader(MSG_HEADER_SET header, const char *value)
{
	char * dupHeader = NULL;
	PRInt32	ret = MK_OUT_OF_MEMORY;

	if (header & (MSG_FROM_HEADER_MASK | MSG_TO_HEADER_MASK | MSG_REPLY_TO_HEADER_MASK | MSG_CC_HEADER_MASK | MSG_BCC_HEADER_MASK))
		dupHeader = mime_fix_addr_header(value);
	else if (header &  (MSG_NEWSGROUPS_HEADER_MASK| MSG_FOLLOWUP_TO_HEADER_MASK))
		dupHeader = mime_fix_news_header(value);
	else  if (header & (MSG_FCC_HEADER_MASK | MSG_ORGANIZATION_HEADER_MASK |  MSG_SUBJECT_HEADER_MASK | MSG_REFERENCES_HEADER_MASK | MSG_X_TEMPLATE_HEADER_MASK))
		dupHeader = mime_fix_header(value);
	else
		NS_ASSERTION(PR_FALSE, "invalid header");	// unhandled header mask - bad boy.

	if (dupHeader) {
		m_fields->SetHeader(header, dupHeader, &ret);
		PR_Free(dupHeader);
	}
	return ret;
}



int
nsMsgSendMimeDeliveryState::Init(
						  MSG_Pane *pane,
						  void      *fe_data,
						  nsMsgCompFields *fields,
						  PRBool digest_p,
						  PRBool dont_deliver_p,
						  MSG_Deliver_Mode mode,

						  const char *attachment1_type,
						  const char *attachment1_body,
						  PRUint32 attachment1_body_length,
						  const struct MSG_AttachmentData *attachments,
						  const struct MSG_AttachedFile *preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
						  nsMsgSendPart *relatedPart,
//#endif
						  void (*message_delivery_done_callback)
						       (MWContext *context,
								void *fe_data,
								int status,
								const char *error_message))
{
	int failure = 0;
	const char* pStr = NULL;
	m_pane = pane;
	m_fe_data = fe_data;
	m_message_delivery_done_callback = message_delivery_done_callback;

	m_related_part = relatedPart;
	if (m_related_part)
		m_related_part->SetMimeDeliveryState(this);

	NS_ASSERTION (fields, "null fields");
	if (!fields)
		return MK_OUT_OF_MEMORY; /* rb -1; */

	if (m_fields)
		m_fields->Release();

	m_fields = new nsMsgCompFields;
	if (m_fields)
		m_fields->AddRef();
	else
		return MK_OUT_OF_MEMORY;

  // TODO: We should read the pref "mail.strictly_mime_headers" somewhere
  // and pass it to MIME_ConformToStandard().
  MIME_ConformToStandard(PR_TRUE);
  // Get the default charset from pref, use this for the charset for now.
  // TODO: For reply/forward, original charset need to be used instead.
  // TODO: Also need to update charset for the charset menu.
  m_fields->SetCharacterSet(fields->GetCharacterSet(), NULL);

	m_fields->SetOwner(pane);

#ifdef GENERATE_MESSAGE_ID
	pStr = fields->GetMessageId();
	if (pStr)
	{
		m_fields->SetMessageId((char *) pStr, NULL);
		/* Don't bother checking for out of memory; if it fails, then we'll just
		   let the server generate the message-id, and suffer with the
		   possibility of duplicate messages.*/
	}
#endif /* GENERATE_MESSAGE_ID */

	/* Strip whitespace from beginning and end of body. */
	if (attachment1_body)
	{
		/* BUG 115202 -- we used to strip out whitespaces from
		   beginning of body. We are not doing that anymore. */
		while (attachment1_body_length > 0 &&
			XP_IS_SPACE (attachment1_body [attachment1_body_length - 1]))
		{
			attachment1_body_length--;
		}
		if (attachment1_body_length <= 0)
			attachment1_body = 0;

		if (attachment1_body)
		{
			char *newb = (char *) PR_Malloc (attachment1_body_length + 1);
			if (! newb)
				return MK_OUT_OF_MEMORY;
			memcpy (newb, attachment1_body, attachment1_body_length);
			newb [attachment1_body_length] = 0;
			m_attachment1_body = newb;
			m_attachment1_body_length = attachment1_body_length;
		}
	}

	pStr = fields->GetNewspostUrl();
	if (!pStr || !*pStr) {
		HJ41792
	}

	m_fields->SetNewspostUrl((char *) fields->GetNewspostUrl(), NULL);
	m_fields->SetDefaultBody((char *) fields->GetDefaultBody(), NULL);
	PR_FREEIF(m_attachment1_type);
	m_attachment1_type = PL_strdup (attachment1_type);
	PR_FREEIF(m_attachment1_encoding);
	m_attachment1_encoding = PL_strdup ("8bit");

	/* strip whitespace from and duplicate header fields. */
	SetMimeHeader(MSG_FROM_HEADER_MASK, fields->GetFrom());
	SetMimeHeader(MSG_REPLY_TO_HEADER_MASK, fields->GetReplyTo());
	SetMimeHeader(MSG_TO_HEADER_MASK, fields->GetTo());
	SetMimeHeader(MSG_CC_HEADER_MASK, fields->GetCc());
	SetMimeHeader(MSG_FCC_HEADER_MASK, fields->GetFcc());
	SetMimeHeader(MSG_BCC_HEADER_MASK, fields->GetBcc());
	SetMimeHeader(MSG_NEWSGROUPS_HEADER_MASK, fields->GetNewsgroups());
	SetMimeHeader(MSG_FOLLOWUP_TO_HEADER_MASK, fields->GetFollowupTo());
	SetMimeHeader(MSG_ORGANIZATION_HEADER_MASK, fields->GetOrganization());
	SetMimeHeader(MSG_SUBJECT_HEADER_MASK, fields->GetSubject());
	SetMimeHeader(MSG_REFERENCES_HEADER_MASK, fields->GetReferences());
	SetMimeHeader(MSG_X_TEMPLATE_HEADER_MASK, fields->GetTemplateName());

	pStr = fields->GetOtherRandomHeaders();
	if (pStr)
		m_fields->SetOtherRandomHeaders((char *) pStr, NULL);

	pStr = fields->GetPriority();
	if (pStr)
		m_fields->SetPriority((char *) pStr, NULL);

	int i, j = (int) MSG_LAST_BOOL_HEADER_MASK;
	for (i = 0; i < j; i++) {
		m_fields->SetBoolHeader((MSG_BOOL_HEADER_SET) i,
		fields->GetBoolHeader((MSG_BOOL_HEADER_SET) i), NULL);
	}

	m_fields->SetForcePlainText(fields->GetForcePlainText());
	m_fields->SetUseMultipartAlternative(fields->GetUseMultipartAlternative());

	if (pane && m_fields->GetReturnReceipt()) {
		if (m_fields->GetReturnReceiptType() == 1 ||
				m_fields->GetReturnReceiptType() == 3)
			pane->SetRequestForReturnReceipt(PR_TRUE);
		else
			pane->SetRequestForReturnReceipt(PR_FALSE);
	}

	/* Check the fields for legitimacy, and run the callback if they're not
	   ok.  */
	if (mode != MSG_SaveAsDraft && mode != MSG_SaveAsTemplate ) {
		failure = mime_sanity_check_fields (m_fields->GetFrom(), m_fields->GetReplyTo(),
										m_fields->GetTo(), m_fields->GetCc(),
										m_fields->GetBcc(), m_fields->GetFcc(),
										m_fields->GetNewsgroups(), m_fields->GetFollowupTo(),
										m_fields->GetSubject(), m_fields->GetReferences(),
										m_fields->GetOrganization(),
										m_fields->GetOtherRandomHeaders());
	}
	if (failure)
		return failure;

	m_digest_p = digest_p;
	m_dont_deliver_p = dont_deliver_p;
	m_deliver_mode = mode;
	// m_msg_file_name = WH_TempName (xpFileToPost, "nsmail");

	failure = HackAttachments(attachments, preloaded_attachments);
	return failure;
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
   MSG_AttachmentData structures.

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

#if 0 //JFD - We shouldn't use it anymore...
extern "C" void
MSG_StartMessageDelivery (MSG_Pane *pane,
						  void      *fe_data,
						  nsMsgCompFields *fields,
						  PRBool digest_p,
						  PRBool dont_deliver_p,
						  const char *attachment1_type,
						  const char *attachment1_body,
						  PRUint32 attachment1_body_length,
						  const struct MSG_AttachmentData *attachments,
						  void *relatedPart,
						  void (*message_delivery_done_callback)
						       (MWContext *context,
								void *fe_data,
								int status,
								const char *error_message))
{
  nsMsgSendMimeDeliveryState::StartMessageDelivery(pane, fe_data, fields,
							 digest_p, dont_deliver_p,
						     MSG_DeliverNow, /* ####??? */
							 attachment1_type,
							 attachment1_body, attachment1_body_length,
							 attachments, 0, 
						     (nsMsgSendPart *) relatedPart,
#ifdef XP_OS2
//DSR040297 - Casting away extern "C"
//DSR040297 - Note: this simple little cast switches the function pointers from extern "C" 
//to non-extern "C" pointer (aka a C++ function). Don't try to change the static method, I've
//tried it and it only gets uglier. This (for what its worth) is at least only a few casts in
//spots where it makes sense.
						     (void (*) (MWContext *, void *, int, const char *))
#endif
						     message_delivery_done_callback);
}

extern "C" void
msg_StartMessageDeliveryWithAttachments (MSG_Pane *pane,
										 void      *fe_data,
										 nsMsgCompFields *fields,
										 PRBool digest_p,
										 PRBool dont_deliver_p,
										 MSG_Deliver_Mode mode,
										 const char *attachment1_type,
										 const char *attachment1_body,
										 PRUint32 attachment1_body_length,
										 const struct MSG_AttachedFile *attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
										 void *relatedPart,
//#endif
										 void (*message_delivery_done_callback)
										      (MWContext *context,
											   void *fe_data,
											   int status,
											   const char *error_message),
										  const char *smtp)
{
  nsMsgSendMimeDeliveryState::StartMessageDelivery(pane, fe_data, fields,
							  digest_p, dont_deliver_p, mode,
							  attachment1_type, attachment1_body,
							  attachment1_body_length,
							  0, attachments,
							  (nsMsgSendPart *) relatedPart,
#ifdef XP_OS2
//DSR040297 - see comment above about 'Casting away extern "C"'
							  (void (*) (MWContext *, void *, int, const char *))
#endif
							  message_delivery_done_callback);
}
#endif //JFD

extern "C" int
msg_DownloadAttachments (MSG_Pane *pane,
						 void *fe_data,
						 const struct MSG_AttachmentData *attachments,
						 void (*attachments_done_callback)
						      (MWContext *context,
							   void *fe_data,
							   int status, const char *error_message,
							   struct MSG_AttachedFile *attachments))
{
  nsMsgSendMimeDeliveryState *state = 0;
  int failure = 0;

  NS_ASSERTION(attachments && attachments[0].url, "invalid attachment parameter");
  /* if (!attachments || !attachments[0].url)
	{
	  failure = -1;
	  goto FAIL;
	} */ /* The only possible error above is out of memory and it is handled
			in MSG_CompositionPane::DownloadAttachments() */
  state = new nsMsgSendMimeDeliveryState;

  if (! state)
	{
	  failure = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  state->m_pane = pane;
  state->m_fe_data = fe_data;
  state->m_attachments_only_p = PR_TRUE;
  state->m_attachments_done_callback =
#ifdef XP_OS2
//DSR040297 - see comment above about 'Casting away extern "C"'
					(void(*)(MWContext*,void*,int,const char*,MSG_AttachedFile*))
#endif
					attachments_done_callback;

  failure = state->HackAttachments(attachments, 0);
  if (failure >= 0)
	return 0;

 FAIL:
  NS_ASSERTION (failure, "Failed");

  /* in this case, our NET_GetURL exit routine has already freed
	 the state */
  if (failure != MK_ATTACHMENT_LOAD_FAILED)
	{
	  char *err_msg = NET_ExplainErrorDetails (failure);
	  attachments_done_callback (state->GetContext(), fe_data, failure,
								 err_msg, 0);

	  if (err_msg) PR_Free (err_msg);
#ifdef XP_MAC
		// ### mwelch The MacFE wants this error thrown as an exception.
		//            This is because of the way that error recovery occurs
		//            inside the compose session object.
		if (failure == MK_INTERRUPTED)
			failure = userCanceledErr;
#endif
	}
  delete state;
  return failure;
}

HJ70669

void nsMsgSendMimeDeliveryState::Clear()
{
	PR_FREEIF (m_attachment1_type);
	PR_FREEIF (m_attachment1_encoding);
	PR_FREEIF (m_attachment1_body);

	if (m_fields) {
		m_fields->Release();
		m_fields = NULL;
	}

	if (m_attachment1_encoder_data) {
		MimeEncoderDestroy(m_attachment1_encoder_data, PR_TRUE);
		m_attachment1_encoder_data = 0;
	}

	if (m_plaintext) {
		if (m_plaintext->m_file)
			XP_FileClose(m_plaintext->m_file);
		XP_FileRemove(m_plaintext->m_file_name, xpFileToPost);
		PR_FREEIF(m_plaintext->m_file_name);
		delete m_plaintext;
		m_plaintext = NULL;
	}

	if (m_html_filename) {
		XP_FileRemove(m_html_filename, xpFileToPost);
		PR_FREEIF(m_html_filename);
	}

	if (m_msg_file) {
		delete m_msg_file;
		m_msg_file = 0;
		NS_ASSERTION (m_msg_file_name, "null file name");
	}


	if (m_imapOutgoingParser) {
		delete m_imapOutgoingParser;
		m_imapOutgoingParser = NULL;
	}

	if (m_imapLocalMailDB) {
		m_imapLocalMailDB->Close();
		m_imapLocalMailDB = NULL;
		XP_FileRemove (m_msg_file_name, xpMailFolderSummary);
	}

	if (m_msg_file_name) {
		XP_FileRemove (m_msg_file_name, xpFileToPost);
		PR_FREEIF (m_msg_file_name);
	}

	HJ82388

	if (m_attachments)
	{
		int i;
		for (i = 0; i < m_attachment_count; i++) {
			if (m_attachments [i].m_encoder_data) {
				MimeEncoderDestroy(m_attachments [i].m_encoder_data, PR_TRUE);
				m_attachments [i].m_encoder_data = 0;
			}

			PR_FREEIF (m_attachments [i].m_url_string);
			if (m_attachments [i].m_url)
				NET_FreeURLStruct (m_attachments [i].m_url);
			PR_FREEIF (m_attachments [i].m_type);
			PR_FREEIF (m_attachments [i].m_override_type);
			PR_FREEIF (m_attachments [i].m_override_encoding);
			PR_FREEIF (m_attachments [i].m_desired_type);
			PR_FREEIF (m_attachments [i].m_description);
			PR_FREEIF (m_attachments [i].m_x_mac_type);
			PR_FREEIF (m_attachments [i].m_x_mac_creator);
			PR_FREEIF (m_attachments [i].m_real_name);
			PR_FREEIF (m_attachments [i].m_encoding);
			if (m_attachments [i].m_file)
				XP_FileClose (m_attachments [i].m_file);
			if (m_attachments [i].m_file_name) {
				if (!m_pre_snarfed_attachments_p)
					XP_FileRemove (m_attachments [i].m_file_name, xpFileToPost);
				PR_FREEIF (m_attachments [i].m_file_name);
			}
#ifdef XP_MAC
		  /* remove the appledoubled intermediate file after we done all.
		   */
			if (m_attachments [i].m_ap_filename) {
				XP_FileRemove (m_attachments [i].m_ap_filename, xpFileToPost);
				PR_FREEIF (m_attachments [i].m_ap_filename);
			}
#endif /* XP_MAC */
		}
		delete[] m_attachments;
		m_attachment_count = m_attachment_pending_count = 0;
		m_attachments = 0;
	}
}


void nsMsgSendMimeDeliveryState::DeliverMessage ()
{
	PRBool mail_p = ((m_fields->GetTo() && *m_fields->GetTo()) || 
					(m_fields->GetCc() && *m_fields->GetCc()) || 
					(m_fields->GetBcc() && *m_fields->GetBcc()));
	PRBool news_p = (m_fields->GetNewsgroups() && 
				    *(m_fields->GetNewsgroups()) ? PR_TRUE : PR_FALSE);

	if ( m_deliver_mode != MSG_SaveAsDraft &&
			m_deliver_mode != MSG_SaveAsTemplate )
		NS_ASSERTION(mail_p || news_p, "message without destination");

	if (m_deliver_mode == MSG_QueueForLater) {
		QueueForLater();
		return;
	}
	else if (m_deliver_mode == MSG_SaveAsDraft) {
		SaveAsDraft();
		return;
	}
	else if (m_deliver_mode == MSG_SaveAsTemplate) {
		SaveAsTemplate();
		return;
	}

    
#if 0 //JFD - was XP_UNIX only
{
	int status = msg_DeliverMessageExternally(GetContext(), m_msg_file_name);
	if (status != 0) {
		if (status < 0)
			Fail (status, 0);
		else {
			/* The message has now been delivered successfully. */
			MWContext *context = GetContext();
			if (m_message_delivery_done_callback)
				m_message_delivery_done_callback (context, m_fe_data, 0, NULL);
			m_message_delivery_done_callback = 0;

			Clear();

			/* When attaching, even though the context has
			   active_url_count == 0, XFE_AllConnectionsComplete() **is**
			   called.  However, when not attaching, and not delivering right
			   away, we don't actually open any URLs, so we need to destroy
			   the window ourself.  Ugh!!
			 */
			if (m_attachment_count == 0)
				MSG_MailCompositionAllConnectionsComplete(MSG_FindPane(context, MSG_ANYPANE));
		}
		return;
	}
}
#endif /* XP_UNIX */


#ifdef MAIL_BEFORE_NEWS
	if (mail_p)
		DeliverFileAsMail ();   /* May call ...as_news() next. */
	else if (news_p)
		DeliverFileAsNews ();
#else  /* !MAIL_BEFORE_NEWS */
	if (news_p)
		DeliverFileAsNews ();   /* May call ...as_mail() next. */
	else if (mail_p)
		DeliverFileAsMail ();
#endif /* !MAIL_BEFORE_NEWS */
	else
		abort ();
}


static void mime_deliver_as_mail_exit (URL_Struct *, int status, MWContext *);
static void mime_deliver_as_news_exit (URL_Struct *url, int status,
									   MWContext *);

void nsMsgSendMimeDeliveryState::DeliverFileAsMail ()
{
	char *buf, *buf2;

	FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_MAIL));

	buf = (char *) PR_Malloc ((m_fields->GetTo() ? PL_strlen (m_fields->GetTo())  + 10 : 0) +
						   (m_fields->GetCc() ? PL_strlen (m_fields->GetCc())  + 10 : 0) +
						   (m_fields->GetBcc() ? PL_strlen (m_fields->GetBcc()) + 10 : 0) +
						   10);
	if (! buf) {
		Fail (MK_OUT_OF_MEMORY, 0);
		return;
	}

	PL_strcpy (buf, "");
	buf2 = buf + PL_strlen (buf);
	if (m_fields->GetTo())
		PL_strcat (buf2, m_fields->GetTo());
	if (m_fields->GetCc()) {
		if (*buf2) PL_strcat (buf2, ",");
			PL_strcat (buf2, m_fields->GetCc());
	}
	if (m_fields->GetBcc()) {
		if (*buf2) PL_strcat (buf2, ",");
			PL_strcat (buf2, m_fields->GetBcc());
	}

	nsISmtpService * smtpService = nsnull;
	nsFilePath filePath (m_msg_file_name ? m_msg_file_name : "");

	nsresult rv = nsServiceManager::GetService(kSmtpServiceCID, nsISmtpService::GetIID(), (nsISupports **)&smtpService);
	if (NS_SUCCEEDED(rv) && smtpService)
	{	
		rv = smtpService->SendMailMessage(filePath, buf, nsnull, nsnull);
		nsServiceManager::ReleaseService(kSmtpServiceCID, smtpService);
	}

	PR_FREEIF(buf); // free the buf because we are done with it....
}


void nsMsgSendMimeDeliveryState::DeliverFileAsNews ()
{
	URL_Struct *url = NET_CreateURLStruct (m_fields->GetNewspostUrl(), NET_DONT_RELOAD);
	if (! url) {
		Fail (MK_OUT_OF_MEMORY, 0);
		return;
	}

	FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_NEWS));

	/*  put the filename of the message into the post data field and set a flag
	  in the URL struct to specify that it is a file.
	*/
	PR_FREEIF(url->post_data);
	url->post_data = PL_strdup(m_msg_file_name);
	url->post_data_size = PL_strlen(url->post_data);
	url->post_data_is_file = PR_TRUE;
	url->method = URL_POST_METHOD;

	url->fe_data = this;
	url->internal_url = PR_TRUE;

	url->msg_pane = m_pane;

	/* We can ignore the return value of NET_GetURL() because we have
	 handled the error in mime_deliver_as_news_exit(). */

/*JFD
  MSG_UrlQueue::AddUrlToPane (url, mime_deliver_as_news_exit, m_pane, PR_TRUE);
*/
}

static void
mime_deliver_as_mail_exit (URL_Struct *url, int status,
						   MWContext * /*context*/)
{
  nsMsgSendMimeDeliveryState *state =
	(nsMsgSendMimeDeliveryState *) url->fe_data;

  state->DeliverAsMailExit(url, status);
}

void
nsMsgSendMimeDeliveryState::DeliverAsMailExit(URL_Struct *url, int status)
{
  char *error_msg = 0;

  url->fe_data = 0;
  if (status < 0 && url->error_msg)
    {
	  error_msg = url->error_msg;
	  url->error_msg = 0;
    }
  NET_FreeURLStruct (url);

  if (status < 0)
	{
	  Fail (status, error_msg);
	}
#ifdef MAIL_BEFORE_NEWS
  else if (m_newsgroups)
	{
	  /* If we're sending this mail message to news as well, start it now.
		 Completion and further errors will be handled there.
	   */
	  DeliverFileAsNews ();
	}
#endif /* MAIL_BEFORE_NEWS */
  else
	{
	  status = 0;
	  /*rhp- The message has now been sent successfully! BUT if we fail on the copy 
       to Send folder operation we need to make it better known to the user 
       that ONLY the copy failed. 
     */
	  if (m_fields->GetFcc())
    {
      MSG_Pane *pane = NULL;
      int  retCode = DoFcc();
      if (retCode == FCC_FAILURE)
      {
        // If we hit here, the copy operation FAILED and we should at least tell the
        // user that it did fail but the send operation has already succeeded.
        if (FE_Confirm(GetContext(), XP_GetString(MK_MSG_FAILED_COPY_OPERATION)))
		{
			status = MK_INTERRUPTED;
		}
      } 
      else if ( retCode == FCC_ASYNC_SUCCESS )
      {
		  return; // this is a async imap online append message operation; let
		          // url exit function to deal with proper clean up work
      }
    }

	  FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_MAIL_DONE));
	  if (m_message_delivery_done_callback)
		m_message_delivery_done_callback (GetContext(),
										  m_fe_data, status, NULL);
	  m_message_delivery_done_callback = 0;
    Clear();
//JFD don't delete myself	  delete this;
	}
}

static void
mime_deliver_as_news_exit (URL_Struct *url, int status,
						   MWContext * /*context*/)
{
  nsMsgSendMimeDeliveryState *state =
	(nsMsgSendMimeDeliveryState *) url->fe_data;
  state->DeliverAsNewsExit(url, status);
}

void
nsMsgSendMimeDeliveryState::DeliverAsNewsExit(URL_Struct *url, int status)
{
  char *error_msg = 0;
  
  url->fe_data = 0;
  if (status < 0 && url->error_msg)
    {
	  error_msg = url->error_msg;
	  url->error_msg = 0;
    }
  NET_FreeURLStruct (url);

  if (status < 0)
	{
	  Fail (status, error_msg);
	}
#ifndef MAIL_BEFORE_NEWS
  else if ((m_fields->GetTo() && *m_fields->GetTo()) || 
		   (m_fields->GetCc() && *m_fields->GetCc()) || 
		   (m_fields->GetBcc() && *m_fields->GetBcc()))
	{
	  /* If we're sending this news message to mail as well, start it now.
		 Completion and further errors will be handled there.
	   */
	  DeliverFileAsMail ();
	}
#endif /* !MAIL_BEFORE_NEWS */
  else
  {
	  status = 0;
	  /*rhp- The message has now been sent successfully! BUT if we fail on the copy 
       to Send folder operation we need to make it better known to the user 
       that ONLY the copy failed. 
     */
	  if (m_fields->GetFcc())
    {
      MSG_Pane *pane = NULL;
      int  retCode = DoFcc();
      if (retCode == FCC_FAILURE)
      {
        // If we hit here, the copy operation FAILED and we should at least tell the
        // user that it did fail but the send operation has already succeeded.
        if (FE_Confirm(GetContext(), XP_GetString(MK_MSG_FAILED_COPY_OPERATION)))
		{
			status = MK_INTERRUPTED;
		}
      } 
      else if ( retCode == FCC_ASYNC_SUCCESS )
      {
        return;			// asyn imap append message operation; let url exit routine
						// deal with the clean up work
      }
    }

	  FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_NEWS_DONE));

	  if (m_message_delivery_done_callback)
		m_message_delivery_done_callback (GetContext(),
										  m_fe_data, status, NULL);
	  m_message_delivery_done_callback = 0;
  	Clear();
//JFD Don't delete myself	  delete this;
	}
}


#ifdef XP_OS2
XP_BEGIN_PROTOS
extern PRInt32 msg_do_fcc_handle_line(char* line, PRUint32 length, void* closure);
XP_END_PROTOS
#else
static PRInt32 msg_do_fcc_handle_line(char* line, PRUint32 length, void* closure);
#endif

static int
mime_do_fcc_1 (MSG_Pane *pane,
			   const char *input_file_name,  XP_FileType input_file_type,
			   const char *output_name, XP_FileType output_file_type,
			   MSG_Deliver_Mode mode,
			   const char *bcc_header,
			   const char *fcc_header,
			   const char *news_url)
{
  int status = 0;
  XP_File in = 0;
  XP_File out = 0;
  PRBool file_existed_p;
  XP_StatStruct st;
  char *ibuffer = 0;
  int ibuffer_size = TEN_K;
  char *obuffer = 0;
  PRInt32 obuffer_size = 0, obuffer_fp = 0;
  PRInt32 n;
  PRBool summaryWasValid = PR_FALSE;
  PRBool summaryIsValid = PR_FALSE;
  PRBool mark_as_read = PR_TRUE;
  ParseOutgoingMessage *outgoingParser = NULL;
  MailDB	*mail_db = NULL;
  nsresult err = NS_OK;
  char *envelope;
  char *output_file_name = NET_ParseURL(output_name, GET_PATH_PART);
  
  if (!output_file_name || !*output_file_name) { // must be real file path
	  PR_FREEIF(output_file_name);
	  output_file_name = PL_strdup(output_name);
  }

  if (mode == MSG_QueueForLater)
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_QUEUEING));
  else if ( mode == MSG_SaveAsDraft )
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_SAVING_AS_DRAFT));
  else if ( mode == MSG_SaveAsTemplate )
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_SAVING_AS_TEMPLATE));
  else
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_WRITING_TO_FCC));

  ibuffer = NULL;
  while (!ibuffer && (ibuffer_size >= 1024))
  {
	  ibuffer = (char *) PR_Malloc (ibuffer_size);
	  if (!ibuffer)
		  ibuffer_size /= 2;
  }
  if (!ibuffer)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  file_existed_p = !XP_Stat (output_file_name, &st, output_file_type);

  if (file_existed_p)
	{
	  summaryWasValid = msg_IsSummaryValid(output_file_name, &st);
	  if (!msg_ConfirmMailFile (pane->GetContext(), output_file_name))
		{
		  PR_FREEIF(output_file_name);
		  return MK_INTERRUPTED; /* #### What a hack.  It turns out we already
									were testing for this result code and
									silently canceling the send if we ever got
									it (because it meant that the user hit the
									Stop button).  Returning it here has a
									similar effect -- the user was asked to
									confirm writing to the FCC folder, and he
									hit the Cancel button, so we now quietly
									do nothing. */

		}
	}
  else
	{
	  pane->GetMaster()->FindMailFolder(output_file_name,
										PR_TRUE /*createIfMissing*/);

	  if (-1 == XP_Stat (output_file_name, &st, output_file_type))
		FE_Alert (pane->GetContext(), XP_GetString(MK_MSG_CANT_CREATE_FOLDER));
	}


  out = XP_FileOpen (output_file_name, output_file_type, XP_FILE_APPEND_BIN);
  if (!out)
	{
	  /* #### include file name in error message! */
    /* Need to determine what type of operation failed and set status accordingly. Used to always return
       MK_MSG_COULDNT_OPEN_FCC_FILE */
    switch (mode)
		{
		case MSG_SaveAsDraft:
			status = MK_MSG_UNABLE_TO_SAVE_DRAFT;
			break;
		case MSG_SaveAsTemplate:
			status = MK_MSG_UNABLE_TO_SAVE_TEMPLATE;
			break;
		case MSG_DeliverNow:
		default:
			status = MK_MSG_COULDNT_OPEN_FCC_FILE;
			break;
		}
	  goto FAIL;
	}

  in = XP_FileOpen (input_file_name, input_file_type, XP_FILE_READ_BIN);
  if (!in)
	{
	  status = MK_UNABLE_TO_OPEN_FILE; /* rb -1; */ /* How did this happen? */
	  goto FAIL;
	}


  // set up database and outbound message parser to keep db up to date.
  outgoingParser = new ParseOutgoingMessage;
  err = MailDB::Open(output_file_name, PR_FALSE, &mail_db);

  if (err != NS_OK)
	mail_db = NULL;

  if (!outgoingParser)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  outgoingParser->SetOutFile(out);
  outgoingParser->SetMailDB(mail_db);
  outgoingParser->SetWriteToOutFile(PR_FALSE);


  /* Write a BSD mailbox envelope line to the file.
	 If the file existed, preceed it by a linebreak: this file format wants a
	 *blank* line before all "From " lines except the first.  This assumes
	 that the previous message in the file was properly closed, that is, that
	 the last line in the file ended with a linebreak.
   */
  XP_FileSeek(out, 0, SEEK_END);

  if (file_existed_p && st.st_size > 0)
	{
	  if (XP_FileWrite (LINEBREAK, LINEBREAK_LEN, out) < LINEBREAK_LEN)
		{
		  status = MK_MIME_ERROR_WRITING_FILE;
		  goto FAIL;
		}
	}

  outgoingParser->Init(XP_FileTell(out));
  envelope = msg_GetDummyEnvelope();

  if (msg_do_fcc_handle_line (envelope, PL_strlen (envelope), outgoingParser)
	  < 0)
	{
	  status = MK_MIME_ERROR_WRITING_FILE;
	  goto FAIL;
	}

  /* Write out an X-Mozilla-Status header.

	 This is required for the queue file, so that we can overwrite it once
	 the messages have been delivered, and so that the MSG_FLAG_QUEUED bit
	 is set.

	 For FCC files, we don't necessarily need one, but we might as well put
	 one in so that it's marked as read already.
   */
  if (mode == MSG_QueueForLater ||
	  mode == MSG_SaveAsDraft ||
	  mode == MSG_SaveAsTemplate ||
	  mark_as_read)
	{
	  char *buf = 0;
	  PRUint16 flags = 0;

	  mark_as_read = PR_TRUE;
	  flags |= MSG_FLAG_READ;
	  if (mode == MSG_QueueForLater )
		flags |= MSG_FLAG_QUEUED;
	  buf = PR_smprintf(X_MOZILLA_STATUS_FORMAT LINEBREAK, flags);
	  if (buf)
	  {
		  status = msg_do_fcc_handle_line(buf, PL_strlen(buf), outgoingParser);
		  PR_FREEIF(buf);
		  if (status < 0)
			goto FAIL;
	  }
	  
	  PRUint32 flags2 = 0;
	  if (mode == MSG_SaveAsTemplate)
		  flags2 |= MSG_FLAG_TEMPLATE;
	  buf = PR_smprintf(X_MOZILLA_STATUS2_FORMAT LINEBREAK, flags2);
	  if (buf)
	  {
		  status = msg_do_fcc_handle_line(buf, PL_strlen(buf), outgoingParser);
		  PR_FREEIF(buf);
		  if (status < 0)
			  goto FAIL;
	  }
	}


  /* Write out the FCC and BCC headers.
	 When writing to the Queue file, we *must* write the FCC and BCC
	 headers, or else that information would be lost.  Because, when actually
	 delivering the message (with "deliver now") we do FCC/BCC right away;
	 but when queueing for later delivery, we do FCC/BCC at delivery-time.

	 The question remains of whether FCC and BCC should be written into normal
	 BCC folders (like the Sent Mail folder.)

	 For FCC, there seems no point to do that; it's not information that one
	 would want to refer back to.

	 For BCC, the question isn't as clear.  On the one hand, if I send someone
	 a BCC'ed copy of the message, and save a copy of it for myself (with FCC)
	 I might want to be able to look at that message later and see the list of
	 people to whom I had BCC'ed it.

	 On the other hand, the contents of the BCC header is sensitive
	 information, and should perhaps not be stored at all.

	 Thus the consultation of the #define SAVE_BCC_IN_FCC_FILE.

	 (Note that, if there is a BCC header present in a message in some random
	 folder, and that message is forwarded to someone, then the attachment
	 code will strip out the BCC header before forwarding it.)
   */
  if ((mode == MSG_QueueForLater ||
	   mode == MSG_SaveAsDraft ||
	   mode == MSG_SaveAsTemplate) &&
	  fcc_header && *fcc_header)
	{
	  PRInt32 L = PL_strlen(fcc_header) + 20;
	  char *buf = (char *) PR_Malloc (L);
	  if (!buf)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}
	  PR_snprintf(buf, L-1, "FCC: %s" LINEBREAK, fcc_header);
	  status = msg_do_fcc_handle_line(buf, PL_strlen(buf), outgoingParser);
	  if (status < 0)
		goto FAIL;
	}
  if (bcc_header && *bcc_header
#ifndef SAVE_BCC_IN_FCC_FILE
	  && (mode == MSG_QueueForLater ||
		  mode == MSG_SaveAsDraft ||
		  mode == MSG_SaveAsTemplate)
#endif
	  )
	{
	  PRInt32 L = PL_strlen(bcc_header) + 20;
	  char *buf = (char *) PR_Malloc (L);
	  if (!buf)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}
	  PR_snprintf(buf, L-1, "BCC: %s" LINEBREAK, bcc_header);
	  status = msg_do_fcc_handle_line(buf, PL_strlen(buf), outgoingParser);
	  if (status < 0)
		goto FAIL;
	}


  /* Write out the X-Mozilla-News-Host header.
	 This is done only when writing to the queue file, not the FCC file.
	 We need this to complement the "Newsgroups" header for the case of
	 queueing a message for a non-default news host.

	 Convert a URL like "snews://host:123/" to the form "host:123/secure"
	 or "news://user@host:222" to simply "host:222".
   */
  if ((mode == MSG_QueueForLater ||
	   mode == MSG_SaveAsDraft ||
	   mode == MSG_SaveAsTemplate) && news_url && *news_url)
	{
	  PRBool secure_p = (news_url[0] == 's' || news_url[0] == 'S');
	  char *orig_hap = NET_ParseURL (news_url, GET_HOST_PART);
	  char *host_and_port = orig_hap;
	  if (host_and_port)
		{
		  /* There may be authinfo at the front of the host - it could be of
			 the form "user:password@host:port", so take off everything before
			 the first at-sign.  We don't want to store authinfo in the queue
			 folder, I guess, but would want it to be re-prompted-for at
			 delivery-time.
		   */
		  char *at = PL_strchr (host_and_port, '@');
		  if (at)
			host_and_port = at + 1;
		}

	  if ((host_and_port && *host_and_port) || !secure_p)
		{
		  char *line = PR_smprintf(X_MOZILLA_NEWSHOST ": %s%s" LINEBREAK,
								   host_and_port ? host_and_port : "",
								   secure_p ? "/secure" : "");
		  PR_FREEIF(orig_hap);
		  orig_hap = 0;
		  if (!line)
			{
			  status = MK_OUT_OF_MEMORY;
			  goto FAIL;
			}
		  status = msg_do_fcc_handle_line(line, PL_strlen(line),
										  outgoingParser);
		  PR_FREEIF(line);

		  if (status < 0)
			goto FAIL;
		}
	  PR_FREEIF(orig_hap);
	  orig_hap = 0;
	}


  /* Read from the message file, and write to the FCC or Queue file.
	 There are two tricky parts: the first is that the message file
	 uses CRLF, and the FCC file should use LINEBREAK.  The second
	 is that the message file may have lines beginning with "From "
	 but the FCC file must have those lines mangled.

	 It's unfortunate that we end up writing the FCC file a line
	 at a time, but it's the easiest way...
   */

  while (1)
	{
	  n = XP_FileRead (ibuffer, ibuffer_size, in);
	  if (n == 0)
		break;
	  if (n < 0) /* read failed (not eof) */
		{
		  status = n;
		  goto FAIL;
		}

	  n = (PRInt32)msg_LineBuffer (ibuffer, n,
						  &obuffer, (PRUint32 *)&obuffer_size,
						  (PRUint32*)&obuffer_fp,
						  PR_TRUE, msg_do_fcc_handle_line,
						  outgoingParser);
	  if (n < 0) /* write failed */
		{
		  status = n;
		  goto FAIL;
		}
	}

  /* If there's still stuff in the buffer (meaning the last line had no
	 newline) push it out. */
  if (obuffer_fp > 0)
	msg_do_fcc_handle_line (obuffer, obuffer_fp, outgoingParser);

  /* Terminate with a final newline. */
  if (XP_FileWrite (LINEBREAK, LINEBREAK_LEN, out) < LINEBREAK_LEN)
  {
	  status = MK_MIME_ERROR_WRITING_FILE;
  }
  else
	outgoingParser->AdvanceOutPosition(LINEBREAK_LEN);

  if (mail_db != NULL && outgoingParser != NULL &&
	  outgoingParser->m_newMsgHdr != NULL)
	{

	  outgoingParser->FinishHeader();
	  mail_db->AddHdrToDB(outgoingParser->m_newMsgHdr, NULL, PR_TRUE);
	  if (summaryWasValid)
		summaryIsValid = PR_TRUE;
	}

 FAIL:

  if (ibuffer)
	PR_Free (ibuffer);
  if (obuffer && obuffer != ibuffer)
	PR_Free (obuffer);

  if (in)
	XP_FileClose (in);

  if (out)
	{
	  if (status >= 0)
		{
		  XP_FileClose (out);
		  if (summaryIsValid) {
			msg_SetSummaryValid(output_file_name, 0, 0);
                  }
		}
	  else if (! file_existed_p)
		{
		  XP_FileClose (out);
		  XP_FileRemove (output_file_name, output_file_type);
		}
	  else
		{
		  XP_FileClose (out);
		  XP_FileTruncate (output_file_name, output_file_type, st.st_size);	/* restore original size */
		}
	}

  if (mail_db != NULL && status >= 0) {
	if ( mode == MSG_SaveAsDraft || mode == MSG_SaveAsTemplate )
	{
		MSG_PostDeliveryActionInfo *actionInfo =
		  pane->GetPostDeliveryActionInfo();
		if (actionInfo) {
/*JFD
		  if (actionInfo->m_flags & MSG_FLAG_EXPUNGED &&
			  actionInfo->m_msgKeyArray.GetSize() >= 1) {
			mail_db->DeleteMessage(actionInfo->m_msgKeyArray.GetAt(0));
			actionInfo->m_msgKeyArray.RemoveAt(0);
		  }
*/
		}
		else {
		  actionInfo = new MSG_PostDeliveryActionInfo((MSG_FolderInfo*)
			pane->GetMaster()->
			FindMailFolder(output_file_name, PR_TRUE));
		  if (actionInfo) {
			actionInfo->m_flags |= MSG_FLAG_EXPUNGED;
			pane->SetPostDeliveryActionInfo(actionInfo);
		  }
		}
/*JFD
		if (outgoingParser->m_newMsgHdr && actionInfo)
		  actionInfo->m_msgKeyArray.Add(outgoingParser->m_newMsgHdr->GetMessageKey());
*/
	}
	mail_db->Close();
  }

  PR_FREEIF(output_file_name);

  delete outgoingParser;
  if (status < 0)
	{
	  /* Fail, and terminate. */
	  return status;
	}
  else
	{
	  /* Otherwise, continue on to _deliver_as_mail or _deliver_as_news
		 or mime_queue_for_later. */
	  return 0;
	}
}

#ifdef XP_OS2
extern PRInt32
#else
static PRInt32
#endif
msg_do_fcc_handle_line(char* line, PRUint32 length, void* closure)
{
  ParseOutgoingMessage *outgoingParser = (ParseOutgoingMessage *) closure;
  PRInt32 err = 0;

  XP_File out = outgoingParser->GetOutFile();

  // if we have a DB, feed the line to the parser
  if (outgoingParser->GetMailDB() != NULL)
	{
	  if (outgoingParser->m_bytes_written == 0)
		err = outgoingParser->StartNewEnvelope(line, length);
	  else
		err = outgoingParser->ParseFolderLine(line, length);
	  if (err < 0)
		  return err;
	}

#ifdef MANGLE_INTERNAL_ENVELOPE_LINES
  /* Note: it is correct to mangle all lines beginning with "From ",
	 not just those that look like parsable message delimiters.
	 Other software expects this. */
  // If this really is the envelope, don't escape it. m_bytes_written will
  // be 0 in that case, because envelope is always first.
  if (outgoingParser->m_bytes_written > 0 && length >= 5 &&
	  line[0] == 'F' && !PL_strncmp(line, "From ", 5))
	{
	  if (XP_FileWrite (">", 1, out) < 1)
		  return MK_MIME_ERROR_WRITING_FILE;
	  outgoingParser->AdvanceOutPosition(1);
	}
#endif /* MANGLE_INTERNAL_ENVELOPE_LINES */

  /* #### if XP_FileWrite is a performance problem, we can put in a
	 call to msg_ReBuffer() here... */
  if (XP_FileWrite (line, length, out) < length)
	  return MK_MIME_ERROR_WRITING_FILE;
  outgoingParser->m_bytes_written += length;
  return 0;
}


extern "C" int
msg_DoFCC (MSG_Pane *pane,
		   const char *input_file,  XP_FileType input_file_type,
		   const char *output_file, XP_FileType output_file_type,
		   const char *bcc_header_value,
		   const char *fcc_header_value)
{
  NS_ASSERTION(pane &&
			input_file && *input_file &&
			output_file && *output_file, "invalid input or output file");
  if (! (pane &&
		 input_file && *input_file &&
		 output_file && *output_file))
	return MK_MIME_ERROR_WRITING_FILE;
  return mime_do_fcc_1 (pane,
						input_file, input_file_type,
						output_file, output_file_type,
						MSG_DeliverNow, bcc_header_value,
						fcc_header_value,
						0);
}


/* rhp: Need a new return state. Now, the following error codes are returned:
    #define FCC_FAILURE          0
    #define FCC_BLOCKING_SUCCESS 1
    #define FCC_ASYNC_SUCCESS    2
 */
int nsMsgSendMimeDeliveryState::DoFcc()
{
  if (!m_fields->GetFcc() || !*m_fields->GetFcc())
	return FCC_BLOCKING_SUCCESS;
  else if (NET_URL_Type(m_fields->GetFcc()) == IMAP_TYPE_URL &&
		   m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4())
  {
	  return SendToImapMagicFolder(MSG_FOLDER_FLAG_SENTMAIL);
  }
  else
	{
	  int status = msg_DoFCC (m_pane,
							  m_msg_file_name, xpFileToPost,
							  m_fields->GetFcc(), xpMailFolder,
							  m_fields->GetBcc(),
							  m_fields->GetFcc());

    if (status < 0)
		Fail (status, 0);

    if (status >= 0)
      return FCC_BLOCKING_SUCCESS;
    else
      return FCC_FAILURE;
	}
}

char *
nsMsgSendMimeDeliveryState::GetOnlineFolderName(PRUint32 flag, const char
											   **pDefaultName)
{
	char *onlineFolderName = NULL;

	switch (flag)
	{
	case MSG_FOLDER_FLAG_DRAFTS:
		if (pDefaultName) *pDefaultName = DRAFTS_FOLDER_NAME;
		PREF_CopyCharPref ("mail.default_drafts", &onlineFolderName);
		break;
	case MSG_FOLDER_FLAG_TEMPLATES:
		if (pDefaultName) *pDefaultName = TEMPLATES_FOLDER_NAME;
		PREF_CopyCharPref("mail.default_templates", &onlineFolderName);
		break;
	case MSG_FOLDER_FLAG_SENTMAIL:
		if (pDefaultName) *pDefaultName = SENT_FOLDER_NAME;
		onlineFolderName = PL_strdup(m_fields->GetFcc());
		break;
	default:
		NS_ASSERTION(0, "unknow flag");
		break;
	}
	return onlineFolderName;
}

PRBool
nsMsgSendMimeDeliveryState::IsSaveMode()
{
	return (m_deliver_mode == MSG_SaveAsDraft ||
			m_deliver_mode == MSG_SaveAsTemplate ||
			m_deliver_mode == MSG_SaveAs);
}


int
nsMsgSendMimeDeliveryState::SaveAsOfflineOp()
{
	NS_ASSERTION (m_imapOutgoingParser && 
			   m_imapLocalMailDB && m_imapFolderInfo, "invalid imap save destination");
	if (!m_imapOutgoingParser || !m_imapLocalMailDB || !m_imapFolderInfo)
	{
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
		return FCC_FAILURE;
	}

	int ret_code = FCC_BLOCKING_SUCCESS;
	nsresult err = NS_OK;
	MailDB *mailDB = NULL;
	MSG_IMAPFolderInfoMail *mailFolderInfo =
		m_imapFolderInfo->GetIMAPFolderInfoMail();
	NS_ASSERTION (mailFolderInfo, "null mail folder info");
	err = MailDB::Open (mailFolderInfo->GetPathname(), PR_FALSE, &mailDB);
	if (err == NS_OK)
	{
		MessageKey fakeId = mailDB->GetUnusedFakeId();
		MailMessageHdr *mailMsgHdr = NULL, *newMailMsgHdr = NULL;

		if (IsSaveMode())
		{
			// Do we have an actionInfo?
			MSG_PostDeliveryActionInfo *actionInfo =
				m_pane->GetPostDeliveryActionInfo(); 
			if (actionInfo)
			{
/*JFD
				NeoOfflineImapOperation *op = NULL;
				if (actionInfo->m_msgKeyArray.GetSize() > 0)
				{
					MessageKey msgKey = actionInfo->m_msgKeyArray.GetAt(0);
					if ((PRInt32) msgKey >= 0) // real message key
					{
						// we start with an existing draft and save it while offline 
						// delete the old message and create a new message header
						op = mailDB->GetOfflineOpForKey(msgKey, PR_TRUE);
						if (op)
						{
							op->SetImapFlagOperation(op->GetNewMessageFlags() |
													 kImapMsgDeletedFlag);
							op->unrefer();
						}
						actionInfo->m_msgKeyArray.RemoveAt(0);
						actionInfo->m_msgKeyArray.Add(fakeId);
						mailDB->DeleteMessage(msgKey);
					}
					else // faked UID; reuse it
					{
						fakeId = actionInfo->m_msgKeyArray.GetAt(0);
						mailDB->DeleteOfflineOp(fakeId);
						mailDB->DeleteMessage(fakeId);
					}
				}
				else
				{
					actionInfo->m_msgKeyArray.Add(fakeId);
				}
JFD */
			}
			else
			{
				// this is a new draft create a new actionInfo and a message
				// header 
/*JFD
				actionInfo = new MSG_PostDeliveryActionInfo(m_imapFolderInfo);
				actionInfo->m_flags |= MSG_FLAG_EXPUNGED;
				actionInfo->m_msgKeyArray.Add(fakeId);
				m_pane->SetPostDeliveryActionInfo(actionInfo);
*/
			}
		}
		newMailMsgHdr = new MailMessageHdr;

#if 0 //JFD
		mailMsgHdr = m_imapLocalMailDB->GetMailHdrForKey(0);
		if (mailMsgHdr)
		{
			if (newMailMsgHdr)
			{
				XP_File fileId = XP_FileOpen (m_msg_file_name, xpFileToPost,
											  XP_FILE_READ_BIN);
				int iSize = 10240;
				char *ibuffer = NULL;
				
				while (!ibuffer && (iSize >= 512))
				{
					ibuffer = (char *) PR_Malloc(iSize);
					if (!ibuffer)
						iSize /= 2;
				}
				
				if (fileId && ibuffer)
				{
					PRInt32 numRead = 0;
					newMailMsgHdr->CopyFromMsgHdr(mailMsgHdr,
												  m_imapLocalMailDB->GetDB(),
												  mailDB->GetDB());

					newMailMsgHdr->SetMessageKey(fakeId);
					err = mailDB->AddHdrToDB(newMailMsgHdr, NULL, PR_TRUE);

					// now write the offline message
					numRead = XP_FileRead(ibuffer, iSize, fileId);
					while(numRead > 0)
					{
						newMailMsgHdr->AddToOfflineMessage(ibuffer, numRead,
														   mailDB->GetDB());
						numRead = XP_FileRead(ibuffer, iSize, fileId);
					}
					// now add the offline op to the database
					if (mailDB->GetMaster() == NULL)
						mailDB->SetMaster(m_pane->GetMaster());
					if (mailDB->GetFolderInfo() == NULL)
						mailDB->SetFolderInfo(mailFolderInfo);
					NS_ASSERTION(mailDB->GetMaster(), "null master");
					NeoOfflineImapOperation *op =
						mailDB->GetOfflineOpForKey(fakeId, PR_TRUE);
					if (op)
					{
						op->SetAppendMsgOperation(mailFolderInfo->GetOnlineName(),
												  m_deliver_mode ==
												  MSG_SaveAsDraft ?
												  kAppendDraft :
												  kAppendTemplate);
						op->unrefer();
						/* The message has now been queued successfully. */
						if (m_message_delivery_done_callback)
							m_message_delivery_done_callback (GetContext(),
															  m_fe_data, 0, NULL);
						m_message_delivery_done_callback = 0;
						
						// Clear() clears the Fcc path
						Clear();
					}
					else
					{
						mailDB->RemoveHeaderFromDB (newMailMsgHdr);
						Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
						ret_code = FCC_FAILURE;
					}
				}
				else
				{
					delete newMailMsgHdr;
					Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
					ret_code = FCC_FAILURE;
				}
				if (fileId)
					XP_FileClose(fileId);
				PR_FREEIF(ibuffer);
			}
			else
			{
				Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
				ret_code = FCC_FAILURE;
			}
			mailMsgHdr->unrefer();
		}
		else
		{
			Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
			ret_code = FCC_FAILURE;
		}
#endif //JFD
	}
	else
	{
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
		ret_code = FCC_FAILURE;
	}
	if (mailDB)
		mailDB->Close();
	return ret_code;
}

void
nsMsgSendMimeDeliveryState::ImapAppendAddBccHeadersIfNeeded(URL_Struct *url)
{
	NS_ASSERTION(url, "null parameter");
	const char *bcc_headers = m_fields->GetBcc();
	char *post_data = NULL;
	if (bcc_headers && *bcc_headers)
	{
		post_data = WH_TempName(xpFileToPost, "nsmail");
		if (post_data)
		{
			XP_File dstFile = XP_FileOpen(post_data, xpFileToPost, XP_FILE_WRITE_BIN);
			if (dstFile)
			{
				XP_File srcFile = XP_FileOpen(m_msg_file_name, xpFileToPost,
											  XP_FILE_READ_BIN);
				if (srcFile)
				{
					char *tmpBuffer = NULL;
					int bSize = TEN_K;
					
					while (!tmpBuffer && (bSize >= 512))
					{
						tmpBuffer = (char *)PR_Malloc(bSize);
						if (!tmpBuffer)
							bSize /= 2;
					}
					int bytesRead = 0;
					if (tmpBuffer)
					{
						XP_FileWrite("Bcc: ", 5, dstFile);
						XP_FileWrite(bcc_headers, PL_strlen(bcc_headers),
									 dstFile);
						XP_FileWrite(CRLF, PL_strlen(CRLF), dstFile);
						bytesRead = XP_FileRead(tmpBuffer, bSize, srcFile);
						while (bytesRead > 0)
						{
							XP_FileWrite(tmpBuffer, bytesRead, dstFile);
							bytesRead = XP_FileRead(tmpBuffer, bSize,
													srcFile);
						}
						PR_Free(tmpBuffer);
					}
					XP_FileClose(srcFile);
				}
				XP_FileClose(dstFile);
			}
		}
	}
	else
		post_data = PL_strdup(m_msg_file_name);

	if (post_data)
	{
		MSG_Pane *urlPane = NULL;

		if (!urlPane)
			urlPane = m_pane;

		url->post_data = post_data;
		url->post_data_size = PL_strlen(post_data);
		url->post_data_is_file = PR_TRUE;
		url->method = URL_POST_METHOD;
		url->fe_data = this;
		url->internal_url = PR_TRUE;
		url->msg_pane = urlPane;
		urlPane->GetContext()->imapURLPane = urlPane;
		
/*JFD
		MSG_UrlQueue::AddUrlToPane 
			(url, nsMsgSendMimeDeliveryState::PostSendToImapMagicFolder, urlPane,
			 PR_TRUE); 
*/		
	}
	else
	{
		NET_FreeURLStruct(url);
	}
}

/* Send the message to the magic folder, and runs the completion/failure
   callback.
 */
int
nsMsgSendMimeDeliveryState::SendToImapMagicFolder ( PRUint32 flag )
{
	char *onlineFolderName = NULL;
	const char *defaultName = "";
	char *name = NULL;
	char *host = NULL;
	char *owner = NULL;
	URL_Struct* url = NULL;
	char* buf = NULL;
	int ret_code = FCC_ASYNC_SUCCESS;

#if 0 //JFD
	if (!m_imapFolderInfo)
	{
		NS_ASSERTION (m_pane, "null pane");
		NS_ASSERTION (m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4(), "not a IMAP server");

		onlineFolderName = GetOnlineFolderName(flag, &defaultName);

		if (onlineFolderName && NET_URL_Type(onlineFolderName) == IMAP_TYPE_URL)
		{
			host = NET_ParseURL(onlineFolderName, GET_HOST_PART);
			name = NET_ParseURL(onlineFolderName, GET_PATH_PART);
			owner = NET_ParseURL(onlineFolderName, GET_USERNAME_PART);
			if (owner)
				NET_UnEscape(owner);
			if (!name || !*name)
			{
				PR_FREEIF (name);	// in case of allocated empty string
				name = PR_smprintf("/%s", defaultName);
			}
			if (!owner || !*owner)
			{
/*JFD
				MSG_IMAPHost *imapHost = m_pane->GetMaster()->GetIMAPHost(host);
				if (imapHost) {
					PR_FREEIF(owner)
					owner = PL_strdup(imapHost->GetUserName());
				}
*/
			}
		}

		if (name && *name && host && *host)
		  m_imapFolderInfo = m_pane->GetMaster()->FindImapMailFolder(host,
																	 name+1,
																	 owner,
																	 PR_FALSE);
	}

	if (m_imapFolderInfo)
	{
		if (NET_IsOffline())
		{
			if (flag == MSG_FOLDER_FLAG_DRAFTS || flag == MSG_FOLDER_FLAG_TEMPLATES)
				ret_code = SaveAsOfflineOp();
			else
			{
				NS_ASSERTION(0, "shouldn't be here");	// shouldn't be here
				ret_code = FCC_FAILURE;
			}
		}
		else
		{
			buf = CreateImapAppendMessageFromFileUrl
				( m_imapFolderInfo->GetHostName(),
				  m_imapFolderInfo->GetOnlineName(),
				  m_imapFolderInfo->GetOnlineHierarchySeparator(),
				  IsSaveMode());

			if (buf)
			{
				url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
				if (url)
					// delivery state will be freed by the PostSendToImapMagicFolder
					ImapAppendAddBccHeadersIfNeeded(url);
				else
					ret_code = FCC_FAILURE;
				PR_FREEIF(buf);
			}
			else
				ret_code = FCC_FAILURE;
		}
	}
	else if (host && name && *host && *name && !NET_IsOffline())
	{
		if (m_pane->IMAPListMailboxExist())
		{
			m_pane->SetIMAPListMailboxExist(PR_FALSE);
			buf = CreateImapAppendMessageFromFileUrl
				(host,
				 name+1,
				 kOnlineHierarchySeparatorUnknown, 
				 IsSaveMode());
			if (buf)
			{
				url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
				if (url)
					// delivery state will be freed by the PostSendToImapMagicFolder
					ImapAppendAddBccHeadersIfNeeded(url);
				else
					ret_code = FCC_FAILURE;
				PR_FREEIF(buf);
			}
			else
				ret_code = FCC_FAILURE;
		}
		else
		{
			// Ok, we want to find a folder in the default personal namespace.
			// Call this to generate that folder name.
			TIMAPNamespace *ns = NULL;
			char *ourFolderName = IMAPNS_GenerateFullFolderNameWithDefaultNamespace(host, name+1, NULL /* owner */, kPersonalNamespace, &ns);
			NS_ASSERTION(ourFolderName, "null folder name");
			if (ourFolderName)
			{
				// Now, we need a hierarchy delimiter for this folder.  So find the
				// namespace that the folder will be put in, and get the real delimiter
				// from that.
				NS_ASSERTION(ns, "null name space");
				char delimiter = '/';	// a guess for now
				if (ns)
					delimiter = IMAPNS_GetDelimiterForNamespace(ns);


				buf = CreateImapListUrl(host, ourFolderName, delimiter);
				if (buf)
				{
					url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
					if (url)
					{
						url->fe_data = this;
						url->internal_url = PR_TRUE;
						url->msg_pane = m_pane;
						GetContext()->imapURLPane = m_pane;
						m_pane->SetIMAPListMailbox(name+1);
						m_pane->SetIMAPListMailboxExist(PR_FALSE);
						
						MSG_UrlQueue::AddUrlToPane
							(url, nsMsgSendMimeDeliveryState::PostListImapMailboxFolder, m_pane, PR_TRUE);
						
					}
					else
						ret_code = FCC_FAILURE;
					PR_FREEIF(buf);
				}
				else
					ret_code = FCC_FAILURE;
				PR_Free(ourFolderName);
			}
			else
				ret_code = FCC_FAILURE;
		}
	}
	else
	{
		switch (m_deliver_mode)
		{
		case MSG_SaveAsDraft:
			FE_Alert(GetContext(),XP_GetString(MK_MSG_UNABLE_TO_SAVE_DRAFT));
			break;
		case MSG_SaveAsTemplate:
			FE_Alert(GetContext(), XP_GetString(MK_MSG_UNABLE_TO_SAVE_TEMPLATE));
			break;
		case MSG_DeliverNow:
		default:
			FE_Alert(GetContext(), XP_GetString(MK_MSG_COULDNT_OPEN_FCC_FILE));
			break;
		}
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);	/* -1 rb */
		ret_code = FCC_FAILURE;
	}
	
	PR_FREEIF(host);
	PR_FREEIF(name);
	PR_FREEIF(owner);
	PR_FREEIF(onlineFolderName);
#endif //JFD
	return ret_code;
}


void
nsMsgSendMimeDeliveryState::PostCreateImapMagicFolder (	URL_Struct *url,
														int status,
														MWContext * context)
{
#if 0 //JFD
	nsMsgSendMimeDeliveryState *state =
		(nsMsgSendMimeDeliveryState*) url->fe_data;
	NS_ASSERTION(state, "null delivery state");

	if (status < 0)
	{
		state->Fail (status, 0);
		// kill the delivery state
		delete state;
	}
	else
	{
		MSG_Master::PostCreateImapFolderUrlExitFunc (url, status, context);

		const char *defaultName;
		char *onlineFolderName = NULL;
		PRUint32 flag = 
			state->m_deliver_mode == MSG_SaveAsDraft ? MSG_FOLDER_FLAG_DRAFTS :
			state->m_deliver_mode == MSG_DeliverNow ? MSG_FOLDER_FLAG_SENTMAIL :
			MSG_FOLDER_FLAG_TEMPLATES;
		onlineFolderName = state->GetOnlineFolderName(flag, &defaultName);
		if (onlineFolderName)
		{
			NS_ASSERTION(onlineFolderName, "no online folder");

			char *host = NET_ParseURL(onlineFolderName, GET_HOST_PART);
			char *name = NET_ParseURL(onlineFolderName, GET_PATH_PART);
			char *owner = NULL;
			
			if (!name || !*name)
			{
				PR_FREEIF (name);	// in case of allocated empty string
				name = PR_smprintf("/%s", defaultName);
			}
			
			NS_ASSERTION(host && *host, "null host");
			MSG_IMAPHost *imapHost = 
				state->m_pane->GetMaster()->GetIMAPHost(host);
			if (imapHost)
				owner = PL_strdup(imapHost->GetUserName());
			
			state->m_imapFolderInfo = 
				state->m_pane->GetMaster()->FindImapMailFolder(host, name+1,
															   owner, PR_FALSE);
			NS_ASSERTION(state->m_imapFolderInfo, "null imap folder info");
			
			if (state->m_imapFolderInfo)
			{
				state->m_imapFolderInfo->SetFlag(flag);
				if (state->SendToImapMagicFolder(flag) != FCC_ASYNC_SUCCESS)
				{
					state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
					delete state;
				}
			}
			else
			{
				state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
				// kill the delivery state
				delete state;
			}
			PR_FREEIF(host);
			PR_FREEIF(name);
			PR_FREEIF(owner);
		}
		else
		{
			state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);
			// kill the delivery state
			delete state;
		}
		PR_FREEIF(onlineFolderName);
	}
	NET_FreeURLStruct(url); 
#endif //JFD
}


// returns folder flag and default folder name
PRUint32 nsMsgSendMimeDeliveryState::GetFolderFlagAndDefaultName(
	const char **defaultName)
{
	PRUint32 flag = 0;
	
	NS_ASSERTION(defaultName, "null parameter");

	switch (m_deliver_mode)
	{
	case MSG_SaveAsDraft:
		*defaultName = DRAFTS_FOLDER_NAME;
		flag = MSG_FOLDER_FLAG_DRAFTS;
		break;
	case MSG_SaveAsTemplate:
		*defaultName = TEMPLATES_FOLDER_NAME;
		flag = MSG_FOLDER_FLAG_TEMPLATES;
		break;
	case MSG_DeliverNow:
		*defaultName = SENT_FOLDER_NAME;
		flag = MSG_FOLDER_FLAG_SENTMAIL;
		break;
	default:
		NS_ASSERTION(0, "unknow delivery mode");
		*defaultName = "";
		flag = 0;;
	}
	return flag;
}

/* static */ void 
nsMsgSendMimeDeliveryState::PostSubscribeImapMailboxFolder (
	URL_Struct *url,
	int status,
	MWContext *context )
{
	nsMsgSendMimeDeliveryState *state =
		(nsMsgSendMimeDeliveryState*) url->fe_data;
	NS_ASSERTION(state, "null state");
	
	if (status < 0)
	{
		state->Fail (status, 0);
		// kill the delivery state
		delete state;
	}
	else
	{
		const char *defaultName = "";
		PRUint32 flag = state->GetFolderFlagAndDefaultName(&defaultName);
		if (flag == 0)
		{
			state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0); /* -1 rb */
			// kill the delivery state
			delete state;
			NET_FreeURLStruct(url);
			return;
		}
		if (state->SendToImapMagicFolder(flag) != FCC_ASYNC_SUCCESS)
		{
			state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
			delete state;
		}
	}
	NET_FreeURLStruct(url);
}

void
nsMsgSendMimeDeliveryState::PostListImapMailboxFolder (	URL_Struct *url,
														int status,
														MWContext *context)
{
	nsMsgSendMimeDeliveryState *state =
		(nsMsgSendMimeDeliveryState*) url->fe_data;
	NS_ASSERTION(state, "null state");

	if (!state) return;

	if (status < 0)
	{
		/* rhp- This is to handle failed copy operation BUT only if we are trying to send the
		   message. If not, then this was not a Send operation and this prompt doesn't make sense. */
		if (state->m_deliver_mode == MSG_DeliverNow)
		{
			if (FE_Confirm(state->GetContext(), XP_GetString(MK_MSG_FAILED_COPY_OPERATION)))
			{        
				state->Fail (MK_INTERRUPTED, 0);
			}
			else
			{
		        // treated as successful close down the compose window
		        /* The message has now been queued successfully. */
				if (state->m_message_delivery_done_callback)
					state->m_message_delivery_done_callback 
						(state->GetContext(), state->m_fe_data, 0, NULL);
				state->m_message_delivery_done_callback = 0;
				
		        // Clear() clears the Fcc path
				state->Clear();
			}
		}
		else
		{
			state->Fail (status, 0);
		}
		// kill the delivery state
		delete state;
	}
	else
	{
		state->m_pane->SetIMAPListMailbox(NULL);
		if (state->m_pane->IMAPListMailboxExist())
		{
			char *host = NET_ParseURL(url->address, GET_HOST_PART);
			char *name = NET_ParseURL(url->address, GET_PATH_PART);

			if (host && *host && name && *name)
			{
				char *buf = 
					CreateIMAPSubscribeMailboxURL(host, name+1,
												  kOnlineHierarchySeparatorUnknown);
				if (buf)
				{
					URL_Struct *imapSubscribeUrl = 
						NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
					if (imapSubscribeUrl)
					{
						imapSubscribeUrl->fe_data = state;
						imapSubscribeUrl->internal_url = PR_TRUE;
						imapSubscribeUrl->msg_pane = state->m_pane;
						state->m_pane->GetContext()->imapURLPane = state->m_pane;
						
/*JFD
						MSG_UrlQueue::AddUrlToPane(imapSubscribeUrl,
												   PostSubscribeImapMailboxFolder,
												   state->m_pane, PR_TRUE);
*/
					}
					else
						// kill the delivery state
						delete state;
					PR_Free(buf);
				}
				else
					// kill the delivery state
					delete state;
			}
			else
				// kill the delivery state
				delete state;
			PR_FREEIF(host);
			PR_FREEIF(name);
		}
		else
		{
			const char *defaultName = "";
			PRUint32 flag = state->GetFolderFlagAndDefaultName (&defaultName);

			if (flag == 0)
			{
				state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0); /* -1 rb */
				// kill the delivery state
				delete state;
			}
			else
			{

				char *onlineFolderName =
					msg_MagicFolderName(state->m_pane->GetMaster()->GetPrefs(),
										flag, &status); 
				char *buf = NULL;
				char *host = NULL, *name = NULL /*, *owner = NULL */;
				MSG_IMAPHost *imapHost = NULL;

				if (status < 0)
				{
					char *error_msg = XP_GetString(status);
					state->Fail(status, error_msg ? PL_strdup(error_msg) : 0);
					delete state;
				}
				else if (!onlineFolderName)
				{
					state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
					delete state;
				}
				else
				{
					NS_ASSERTION(NET_URL_Type(onlineFolderName) == IMAP_TYPE_URL, "wrong url type");
					host = NET_ParseURL(onlineFolderName, GET_HOST_PART);
					name = NET_ParseURL(onlineFolderName, GET_PATH_PART);
					// owner = NET_ParseURL(onlineFolderName, GET_USERNAME_PART);
			
					if (!name || !*name)
					{
						PR_FREEIF(name);
						name = PR_smprintf("/%s", defaultName);
					}
					
					imapHost = state->m_pane->GetMaster()->GetIMAPHost(host);
					
					// Now, we need a hierarchy delimiter for this folder.  So find it
					// from its namespace
					TIMAPNamespace *nsForFolder = NULL;
					NS_ASSERTION(imapHost, "null imap host");
					if (imapHost)
					{
						char *fullCanonicalName = 
							IMAPNS_GenerateFullFolderNameWithDefaultNamespace
							(imapHost->GetHostName(), name + 1, 
							 NULL /* owner */, kPersonalNamespace, &nsForFolder);
						if (fullCanonicalName)
						{
							PR_Free(name);
							name = fullCanonicalName;
						}
					}
					// *** Create Imap magic folder
					// *** then append message to the folder
					
					NS_ASSERTION(nsForFolder, "null ptr");
					char delimiter = '/';	// a guess for now
					if (nsForFolder)	// get the real one
						delimiter = IMAPNS_GetDelimiterForNamespace(nsForFolder);
			
					buf = CreateImapMailboxCreateUrl(host, *name == '/' ? name+1 : name,
													 delimiter);
					if (buf)
					{
						URL_Struct* url_struct = NULL;
						url_struct = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
						if (url_struct)
						{
							url_struct->fe_data = state;
							url_struct->internal_url = PR_TRUE;
							url_struct->msg_pane = state->m_pane;
							state->GetContext()->imapURLPane = state->m_pane;
							
/*JFD
							MSG_UrlQueue::AddUrlToPane (url_struct,
														PostCreateImapMagicFolder,
														state->m_pane, PR_TRUE);
*/
						}
						else
							delete state; // error, kill the delivery state
						PR_FREEIF(buf);
					}
					else
						delete state; // kill the delivery state
					PR_FREEIF(onlineFolderName);
					PR_FREEIF(host);
					PR_FREEIF(name);
					// PR_FREEIF(owner);
				}
			}
		}
	}
	NET_FreeURLStruct(url); 
}

void
nsMsgSendMimeDeliveryState::SetIMAPMessageUID(MessageKey key)
{
	NS_ASSERTION(m_pane && m_pane->GetPaneType() == MSG_COMPOSITIONPANE, "invalid msg pane");
	nsMsgCompose *composePane = (nsMsgCompose*) m_pane;
/*JFD
	composePane->SetIMAPMessageUID(key);
*/
}

void
nsMsgSendMimeDeliveryState::PostSendToImapMagicFolder (	URL_Struct *url,
														int status,
														MWContext *context)
{
#if 0 //JFD
	nsMsgSendMimeDeliveryState *state = (nsMsgSendMimeDeliveryState*) url->fe_data;
	nsMsgCompose *composePane = (nsMsgCompose*) state->m_pane;
	NS_ASSERTION(state && composePane, "null state or compose pane");

	MSG_PostDeliveryActionInfo *actionInfo =
		composePane->GetPostDeliveryActionInfo();
	MailDB *mailDB = NULL;
	IDArray *idArray = new IDArray;
	char *onlineFolderName = NULL;

	if (PL_strcmp(url->post_data, state->m_msg_file_name))
		XP_FileRemove(url->post_data, xpFileToPost);

	if (status < 0)
	{ 
		/* rhp- This is to handle failed copy operation BUT only if we are trying to
		   send the message. If not, then this was not a Send operation and this
		   prompt doesn't make sense. */
		if (state->m_deliver_mode == MSG_DeliverNow)
		{
			if (FE_Confirm(state->GetContext(), 
						   XP_GetString(MK_MSG_FAILED_COPY_OPERATION)))
			{
				state->Fail (MK_INTERRUPTED, 0);
			}
			else
			{
				// treated as successful close down the compose window
				/* The message has now been queued successfully. */
				if (state->m_message_delivery_done_callback)
					state->m_message_delivery_done_callback 
						(state->GetContext(), state->m_fe_data, 0, NULL);
				state->m_message_delivery_done_callback = 0;
				
				// Clear() clears the Fcc path
				state->Clear();
			}
		}
		else
		{
			state->Fail (status, 0);
		}
	}
	else
	{
		NS_ASSERTION (composePane && composePane->GetPaneType() ==
				   MSG_COMPOSITIONPANE, "invalid compose pane"); 
		
		if (actionInfo  &&
			state->IsSaveMode() &&
			actionInfo->m_msgKeyArray.GetSize() > 1 &&
			actionInfo->m_flags & MSG_FLAG_EXPUNGED)
		{
			MSG_Pane *urlPane = actionInfo->m_folderInfo ?
				state->m_pane->GetMaster()->FindPaneOfType
				(actionInfo->m_folderInfo, MSG_THREADPANE) : NULL;
			
			composePane->DeleteIMAPOldUID(actionInfo, urlPane);
		}
		if (state->m_imapFolderInfo)
		{
			char *dbName = WH_FileName(state->m_imapFolderInfo->GetPathname(), 
									   xpMailFolderSummary);
			if (dbName)
				mailDB = (MailDB *) MessageDB::FindInCache(dbName);
			PR_FREEIF(dbName);
			MSG_Pane *urlPane = NULL;
			
			if (!urlPane)
				urlPane = state->m_pane->GetMaster()->FindPaneOfType
					(state->m_imapFolderInfo, MSG_THREADPANE);

			if (!urlPane)
				urlPane = state->m_pane->GetMaster()->FindPaneOfType
					(state->m_imapFolderInfo, MSG_MESSAGEPANE);
			
			if (!urlPane)
				urlPane = composePane;

			if (mailDB && urlPane)
			{
				char *url_string = CreateImapMailboxNoopUrl(
					state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetHostName(),
					state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetOnlineName(),
					state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetOnlineHierarchySeparator());

				if (url_string)
				{
					URL_Struct *url_struct =
						NET_CreateURLStruct(url_string,
											NET_NORMAL_RELOAD);
					NS_ASSERTION(urlPane, "null url pane");
					if (url_struct)
					{
						state->m_imapFolderInfo->SetFolderLoadingContext(urlPane->GetContext());
						urlPane->SetLoadingImapFolder(state->m_imapFolderInfo);
						url_struct->fe_data = (void *) state->m_imapFolderInfo;
						url_struct->internal_url = PR_TRUE;
						url_struct->msg_pane = urlPane;
						urlPane->GetContext()->imapURLPane = urlPane;

/*JFD
						MSG_UrlQueue::AddUrlToPane (url_struct, 
													MSG_Pane::PostNoopExitFunc,
													urlPane, PR_TRUE);
*/
					}
					PR_FREEIF(url_string);
				}
				// MessageDB::FindInCache() does not add refCount so don't call close
				// mailDB->Close();
			}
			else
			{
				idArray->Add(0);	// add dummy message key
				state->m_imapFolderInfo->UpdatePendingCounts(state->m_imapFolderInfo, 
															 idArray, PR_FALSE);
				state->m_imapFolderInfo->SummaryChanged();
				// make sure we close down the cached imap connection when we done
				// with save draft; this is save draft then send case what about the closing
				// down the compose window case?
				if (urlPane->GetPaneType() != MSG_THREADPANE && 
					state->m_imapFolderInfo->GetFlags() & MSG_FOLDER_FLAG_DRAFTS &&
					state->m_deliver_mode != MSG_SaveAsDraft)
					composePane->GetMaster()->ImapFolderClosed(state->m_imapFolderInfo);
			}
			/* The message has now been queued successfully. */
			if (state->m_message_delivery_done_callback)
				state->m_message_delivery_done_callback (state->GetContext(),
														 state->m_fe_data, 0, NULL);
			state->m_message_delivery_done_callback = 0;
			
			// Clear() clears the Fcc path
			state->Clear();
		}
		else
		{
			state->Fail(-1, 0);	// something is wrong; maybe wrong password.
		}
	}
		
	PR_FREEIF(onlineFolderName);
	if (idArray)
		delete idArray;
	
	NET_FreeURLStruct(url);
	delete state;
#endif //JFD
}

/* Send the message to the magic folder, and runs the completion/failure
   callback.
 */
void
nsMsgSendMimeDeliveryState::SendToMagicFolder ( PRUint32 flag )
{
  char *name = 0;
  int status = 0;

  name = msg_MagicFolderName(m_pane->GetMaster()->GetPrefs(), flag, &status);
  if (status < 0)
  {
	  char *error_msg = XP_GetString(status);
	  Fail (status, error_msg ? PL_strdup(error_msg) : 0);
//JFD don't delete myself	  delete this;
	  PR_FREEIF(name);
	  return;
  }
  else if (!name || *name == 0)
	{
	  PR_FREEIF(name);
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  else if (NET_URL_Type(name) == IMAP_TYPE_URL &&
	  m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4())
  {
	  if (SendToImapMagicFolder(flag) != FCC_ASYNC_SUCCESS)
	  {
		  Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
//JFD don't delete myself		  delete this;
	  }
	  PR_FREEIF(name);
	  return;
  }
  status = mime_do_fcc_1 (m_pane,
						  m_msg_file_name, xpFileToPost,
						  name, xpMailFolder,
						  m_deliver_mode, m_fields->GetBcc(), m_fields->GetFcc(),
						  (m_fields->GetNewsgroups() && *m_fields->GetNewsgroups()
						   ? m_fields->GetNewspostUrl() : 0));
  PR_FREEIF (name);

 FAIL:

  if (status < 0)
	{
	  Fail (status, 0);
	}
  else
	{
	  MWContext *context = GetContext();
	  FE_Progress(context, XP_GetString(MK_MSG_QUEUED));

	  // Clear() clears the m_related_part now. So, we need to decide
	  // whether we need to make an extra all connections complete call or not.
	  PRBool callAllConnectionsComplete =
		  (!m_related_part || !m_related_part->GetNumChildren());

	  /* The message has now been queued successfully. */
	  if (m_message_delivery_done_callback)
		m_message_delivery_done_callback (context,
										  m_fe_data, 0, NULL);
	  m_message_delivery_done_callback = 0;

	  Clear();

	  /* When attaching, even though the context has active_url_count == 0,
		 XFE_AllConnectionsComplete() **is** called.  However, when not
		 attaching, and not delivering right away, we don't actually open
		 any URLs, so we need to destroy the window ourself.  Ugh!!
	   */
	  /* Unfortunately, Multipart related message falls into the same category.
	   * If we are sending images within a html message, we'll be chaining URLs
	   * one after another. Which will get FE_AllConnectionsComplete() to be
	   * called from NET_ProcessNet(). To prevent from calling
	   * MSG_MailCompositionAllConnectionsComplete() twice and
	   * not crashing the browser we only call all connections complete
	   * when we are not an mhtml message or an mhtml message which does not 
	   * embed any images. This is really an ugly fix.
	   */
	  if (callAllConnectionsComplete && !XP_IsContextBusy(context))
		  MSG_MailCompositionAllConnectionsComplete(MSG_FindPane(context,
																 MSG_ANYPANE));
	}
//JFD don't delete myself  delete this;
}

/* Queues the message for later delivery, and runs the completion/failure
   callback.
 */
void nsMsgSendMimeDeliveryState::QueueForLater()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_QUEUE);
}


/* Save the message to the Drafts folder, and runs the completion/failure
   callback.
 */
void nsMsgSendMimeDeliveryState::SaveAsDraft()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_DRAFTS);
}

/* Save the message to the Template folder, and runs the completion/failure
   callback.
 */
void nsMsgSendMimeDeliveryState::SaveAsTemplate()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_TEMPLATES);
}

#ifdef _USRDLL

PUBLIC void
NET_RegisterDLLContentConverters()
{

  NET_RegisterContentTypeConverter ("*", FO_MAIL_TO,
										NULL, mime_make_attachment_stream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_TO,
										NULL, mime_make_attachment_stream);

  /* #### I don't understand this -- what is this function for?
	 Is this right?  I've cloned this stuff from MSG_RegisterConverters()
	 above.  --jwz */

  NET_RegisterContentTypeConverter ("*", FO_MAIL_MESSAGE_TO,
									NULL, mime_make_attachment_stream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_MESSAGE_TO,
									NULL, mime_make_attachment_stream);

  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_MAIL_MESSAGE_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_MAIL_MESSAGE_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822,
									FO_CACHE_AND_MAIL_MESSAGE_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS,
									FO_CACHE_AND_MAIL_MESSAGE_TO,
									NULL, MIME_MessageConverter);
}

#endif

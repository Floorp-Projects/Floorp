/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsCOMPtr.h"
#include "modmimee.h"
#include "libi18n.h"
#include "mimeobj.h"
#include "modlmime.h"
#include "mimei.h"
#include "prefapi.h"
#include "mimebuf.h"
#include "mimemoz2.h"
#include "mimemsg.h"

#include "prmem.h"
#include "plstr.h"
#include "prio.h"
#include "nsIPref.h"
#include "msgCore.h"
#include "nsCRT.h"
#include "nsIMsgSend.h"
#include "nsFileStream.h"
#include "nsMimeStringResources.h"
#include "nsIIOService.h"
#include "comi18n.h"
#include "nsIMsgCompFields.h"
#include "nsMsgCompCID.h"
#include "nsIStreamConverter2.h"
#include "nsIMsgComposeService.h"
#include "nsIMsgCompose.h"

//
// Header strings...
//
#define HEADER_NNTP_POSTING_HOST      "NNTP-Posting-Host"
#define MIME_HEADER_TABLE             "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>"
#define HEADER_START_JUNK             "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"
#define HEADER_MIDDLE_JUNK            ": </TH><TD>"
#define HEADER_END_JUNK               "</TD></TR>"

//
// Forward declarations...
//
extern "C" char     *MIME_StripContinuations(char *original);
int                 mime_decompose_file_init_fn ( void *stream_closure, MimeHeaders *headers );
int                 mime_decompose_file_output_fn ( char *buf, PRInt32 size, void *stream_closure );
int                 mime_decompose_file_close_fn ( void *stream_closure );
extern int          MimeHeaders_build_heads_list(MimeHeaders *hdrs);

// CID's
static NS_DEFINE_CID(kCMsgComposeServiceCID,  NS_MSGCOMPOSESERVICE_CID);       

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// THIS SHOULD ALL MOVE TO ANOTHER FILE AFTER LANDING!
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Define CIDs...
static NS_DEFINE_CID(kIOServiceCID,           NS_IOSERVICE_CID);
static NS_DEFINE_CID(kMsgCompFieldsCID,       NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID(kPrefCID,                NS_PREF_CID);

// Utility to create a nsIURI object...
nsresult 
nsMimeNewURI(nsIURI** aInstancePtrResult, const char *aSpec)
{  
  nsresult  res;

  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &res);
  if (NS_FAILED(res)) 
    return NS_ERROR_FACTORY_NOT_REGISTERED;

  return pService->NewURI(aSpec, nsnull, aInstancePtrResult);
}

//
// Hopefully, someone will write and XP call like this eventually!
//
#define     TPATH_LEN   1024

static char *
GetTheTempDirectoryOnTheSystem(void)
{
  char *retPath = (char *)PR_Malloc(TPATH_LEN);
  if (!retPath)
    return nsnull;

  retPath[0] = '\0';
#ifdef WIN32
  if (getenv("TEMP"))
    PL_strncpy(retPath, getenv("TEMP"), TPATH_LEN);  // environment variable
  else if (getenv("TMP"))
    PL_strncpy(retPath, getenv("TMP"), TPATH_LEN);   // How about this environment variable?
  else
    GetWindowsDirectory(retPath, TPATH_LEN);
#endif 

#if defined(XP_UNIX) || defined(XP_BEOS)
  char *tPath = getenv("TMPDIR");
  if (!tPath)
    PL_strncpy(retPath, "/tmp/", TPATH_LEN);
  else
    PL_strncpy(retPath, tPath, TPATH_LEN);
#endif

#ifdef XP_MAC
  PL_strncpy(retPath, "", TPATH_LEN);
#endif

  return retPath;
}

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
//
nsFileSpec * 
nsMsgCreateTempFileSpec(char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  // Age old question, where to store temp files....ugh!
  char  *tDir = GetTheTempDirectoryOnTheSystem();
  if (!tDir)
    return (new nsFileSpec("mozmail.tmp"));  // No need to I18N

  nsFileSpec *tmpSpec = new nsFileSpec(tDir);
  if (!tmpSpec)
  {
    PR_FREEIF(tDir);
    return (new nsFileSpec("mozmail.tmp"));  // No need to I18N
  }

  *tmpSpec += tFileName;
  tmpSpec->MakeUnique();

  PR_FREEIF(tDir);
  return tmpSpec;
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// END OF - THIS SHOULD ALL MOVE TO ANOTHER FILE AFTER LANDING!
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

// 
// This struct is the state we use for loading drafts and templates...
//
struct mime_draft_data 
{
  char                *url_name;           // original url name */
  nsMimeOutputType    format_out;          // intended output format; should be FO_OPEN_DRAFT */
  nsMIMESession       *stream;             // not used for now 
  MimeObject          *obj;                // The root 
  MimeDisplayOptions  *options;            // data for communicating with libmime
  MimeHeaders         *headers;            // Copy of outer most mime header 
  PRInt32             attachments_count;   // how many attachments we have 
  nsMsgAttachedFile   *attachments;        // attachments 
  nsMsgAttachedFile   *messageBody;        // message body 
  nsMsgAttachedFile   *curAttachment;		   // temp 

  nsIFileSpec         *tmpFileSpec;
  nsOutputFileStream  *tmpFileStream;      // output file handle 

  MimeDecoderData     *decoder_data;
  char                *mailcharset;        // get it from CHARSET of Content-Type 
};

// RICHIE - PROBABLY BELONGS IN A HEADER SOMEWHERE!!!
//Message Compose Editor Type 
typedef enum
{
  nsMsgDefault = 0,
  nsMsgPlainTextEditor,
  nsMsgHTMLEditor
} nsMsgEditorType;

typedef enum {
	nsMsg_RETURN_RECEIPT_BOOL_HEADER_MASK = 0,
	nsMsg_ENCRYPTED_BOOL_HEADER_MASK,
	nsMsg_SIGNED_BOOL_HEADER_MASK,
	nsMsg_UUENCODE_BINARY_BOOL_HEADER_MASK,
	nsMsg_ATTACH_VCARD_BOOL_HEADER_MASK,
	nsMsg_LAST_BOOL_HEADER_MASK			// last boolean header mask; must be the last one 
										              // DON'T remove.
} nsMsgBoolHeaderSet;

static void
mime_dump_attachments ( nsMsgAttachmentData *attachData )
{
  PRInt32     i = 0;
  struct nsMsgAttachmentData  *tmp = attachData;

  while ( (tmp) && (tmp->real_name) )
  {
	  printf("Real Name         : %s\n", tmp->real_name);

    if ( tmp->url ) 
    {
      char *spec = nsnull;

      tmp->url->GetSpec(&spec);
  	  printf("URL               : %s\n", spec);
    }

	  printf("Desired Type      : %s\n", tmp->desired_type);
    printf("Real Type         : %s\n", tmp->real_type);
	  printf("Real Encoding     : %s\n", tmp->real_encoding); 
    printf("Description       : %s\n", tmp->description);
    printf("Mac Type          : %s\n", tmp->x_mac_type);
    printf("Mac Creator       : %s\n", tmp->x_mac_creator);
    i++;
    tmp++;
  }
}

nsresult
CreateTheComposeWindow(nsIMsgCompFields     *compFields,
                       nsMsgAttachmentData  *attachmentList,
                       nsMsgEditorType      editorType)
{
nsresult            rv;
MSG_ComposeFormat   format = MSGCOMP_FORMAT_Default;

#ifdef NS_DEBUG
printf("RICHIE: Need to do something with the attachment data!!!!\n");
mime_dump_attachments ( attachmentList );
#endif

  NS_WITH_SERVICE(nsIMsgComposeService, msgComposeService, kCMsgComposeServiceCID, &rv); 
  if ((NS_FAILED(rv)) || (!msgComposeService))
    return rv; 

  if (editorType == nsMsgPlainTextEditor)
    format = MSGCOMP_FORMAT_PlainText;
  else if (editorType == nsMsgHTMLEditor)
    format = MSGCOMP_FORMAT_HTML;
    
  rv = msgComposeService->OpenComposeWindowWithCompFields(
                        nsString2("chrome://messengercompose/content/").GetUnicode(),
                        format, compFields);
  return rv;
}

nsIMsgCompFields * 
CreateCompositionFields(const char        *from,
									      const char        *reply_to,
									      const char        *to,
									      const char        *cc,
									      const char        *bcc,
									      const char        *fcc,
									      const char        *newsgroups,
									      const char        *followup_to,
									      const char        *organization,
									      const char        *subject,
									      const char        *references,
									      const char        *other_random_headers,
									      const char        *priority,
									      const char        *attachment,
                        const char        *newspost_url,
                        PRBool            xlate_p,
                        PRBool            sign_p)
{
  nsIMsgCompFields *cFields = nsnull;

  // Create the compose fields...
  nsresult rv = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, 
                                                   nsCOMTypeInfo<nsIMsgCompFields>::GetIID(), 
                                                   (void **) &cFields);   
  if (NS_FAILED(rv) || !cFields) 
    return nsnull;
  NS_ADDREF(cFields);

  // Now set all of the passed in stuff...
  cFields->SetFrom(nsString2(from).GetUnicode());
  cFields->SetSubject(nsString2(subject).GetUnicode());
  cFields->SetReplyTo(nsString2(reply_to).GetUnicode());
  cFields->SetTo(nsString2(to).GetUnicode());
  cFields->SetCc(nsString2(cc).GetUnicode());
  cFields->SetBcc(nsString2(bcc).GetUnicode());
  cFields->SetFcc(nsString2(fcc).GetUnicode());
  cFields->SetNewsgroups(nsString2(newsgroups).GetUnicode());
  cFields->SetFollowupTo(nsString2(followup_to).GetUnicode());
  cFields->SetOrganization(nsString2(organization).GetUnicode());
  cFields->SetReferences(nsString2(references).GetUnicode());
  cFields->SetOtherRandomHeaders(nsString2(other_random_headers).GetUnicode());
  cFields->SetPriority(nsString2(priority).GetUnicode());
  cFields->SetAttachments(nsString2(attachment).GetUnicode());
  cFields->SetNewspostUrl(nsString2(newspost_url).GetUnicode());

  return cFields;
}

// RICHIE:
// Not sure what this preference is in the new world.
PRBool
GetMailXlateionPreference(void)
{
  nsresult res;
  PRBool   xlate = PR_FALSE; 

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (NS_SUCCEEDED(res) && prefs)
    res = prefs->GetBoolPref("mail.unknown", &xlate);
  
  return xlate;
}

// RICHIE:
// Not sure what this preference is in the new world.
PRBool
GetMailSigningPreference(void)
{
  nsresult  res;
  PRBool    signit = PR_FALSE;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (NS_SUCCEEDED(res) && prefs)
    res = prefs->GetBoolPref("mail.unknown", &signit);

  return signit;
}
  
static int
dummy_file_write( char *buf, PRInt32 size, void *fileHandle )
{
  if (!fileHandle)
    return NS_ERROR_FAILURE;

  nsOutputFileStream  *tStream = (nsOutputFileStream *) fileHandle;
  return tStream->write(buf, size);
}

static int
mime_parse_stream_write ( nsMIMESession *stream, const char *buf, PRInt32 size )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;  
  PR_ASSERT ( mdd );

  if ( !mdd || !mdd->obj ) 
    return -1;

  return mdd->obj->clazz->parse_buffer ((char *) buf, size, mdd->obj);
}

static void
mime_free_attach_data ( nsMsgAttachmentData *attachData, int cleanupCount)
{
  PRInt32     i;
  struct nsMsgAttachmentData  *tmp = attachData;

  for ( i=0; i < cleanupCount; i++, tmp++ ) 
  {
	  if ( tmp->url ) 
      delete tmp->url;
    PR_FREEIF(tmp->real_name);

	  PR_FREEIF(tmp->desired_type);
    PR_FREEIF(tmp->real_type);
	  PR_FREEIF(tmp->real_encoding); 
    PR_FREEIF(tmp->description);
    PR_FREEIF(tmp->x_mac_type);
    PR_FREEIF(tmp->x_mac_creator);
  }
}

static void
mime_free_attachments ( nsMsgAttachedFile *attachments, int count )
{
  int               i;
  nsMsgAttachedFile *cur = attachments;

  PR_ASSERT ( attachments && count > 0 );
  if ( !attachments || count <= 0 ) 
    return;

  for ( i = 0; i < count; i++, cur++ ) 
  {
	  if ( cur->orig_url )
      NS_RELEASE(cur->orig_url);

	  PR_FREEIF ( cur->type );
	  PR_FREEIF ( cur->encoding );
	  PR_FREEIF ( cur->description );
	  PR_FREEIF ( cur->x_mac_type );
	  PR_FREEIF ( cur->x_mac_creator );
	  if ( cur->file_spec ) 
    {
      cur->file_spec->Delete(PR_FALSE);
	    delete ( cur->file_spec );
	  }
  }

  PR_FREEIF( attachments );
}

static nsMsgAttachmentData *
mime_draft_process_attachments(mime_draft_data *mdd) 
{
  nsMsgAttachmentData         *attachData = NULL, *tmp = NULL;
  nsMsgAttachedFile           *tmpFile = NULL;
  int                         i;
  PRInt32                     cleanupCount = 0;

  PR_ASSERT ( mdd->attachments_count && mdd->attachments );

  if ( !mdd->attachments || !mdd->attachments_count )
  	return nsnull;

  attachData = (nsMsgAttachmentData *) PR_MALLOC( ( (mdd->attachments_count+1) * sizeof (nsMsgAttachmentData) ) );
  if ( !attachData ) 
    return nsnull;

  nsCRT::memset ( attachData, 0, (mdd->attachments_count+1) * sizeof (nsMsgAttachmentData) );

  tmpFile = mdd->attachments;
  tmp = attachData;

  for ( i=0; i < mdd->attachments_count; i++, tmp++, tmpFile++ ) 
  {
	  if (tmpFile->type) 
    {
		  if (PL_strcasecmp ( tmpFile->type, "text/x-vcard") == 0)
			  mime_SACopy (&(tmp->real_name), tmpFile->description);
	  }

	  if ( tmpFile->orig_url ) 
    {
      char  *tmpSpec = nsnull;
      if (NS_FAILED(tmpFile->orig_url->GetSpec(&tmpSpec)))
      {
        cleanupCount = i;
        goto FAIL;
      }

      if (NS_FAILED(nsMimeNewURI(&(tmp->url), tmpSpec)))
      {
        PR_FREEIF(tmpSpec);
        cleanupCount = i;
        goto FAIL; 
      }

      NS_ADDREF(tmp->url);
	    if (!tmp->real_name)
  	    mime_SACopy ( &(tmp->real_name), tmpSpec );
	  }

	  if ( tmpFile->type ) 
    {
	    mime_SACopy ( &(tmp->desired_type), tmpFile->type );
	    mime_SACopy ( &(tmp->real_type), tmpFile->type );
	  }

	  if ( tmpFile->encoding ) 
    {
	    mime_SACopy ( &(tmp->real_encoding), tmpFile->encoding );
	  }

	  if ( tmpFile->description ) 
    {
	    mime_SACopy ( &(tmp->description), tmpFile->description );
	  }

	  if ( tmpFile->x_mac_type ) 
    {
	    mime_SACopy ( &(tmp->x_mac_type), tmpFile->x_mac_type );
	  }

	  if ( tmpFile->x_mac_creator ) 
    {
	    mime_SACopy ( &(tmp->x_mac_creator), tmpFile->x_mac_creator );
	  }
  }

  return (attachData);

FAIL:
  mime_free_attach_data (attachData, cleanupCount);
  PR_FREEIF(attachData);
  return nsnull;
}

static void 
mime_fix_up_html_address( char **addr)
{
	//
  // We need to replace paired <> they are treated as HTML tag 
  //
	if (addr && *addr && PL_strchr(*addr, '<') && PL_strchr(*addr, '>'))
	{
		char *lt = NULL;
		PRInt32 newLen = 0;
		do 
		{
			newLen = PL_strlen(*addr) + 3 + 1;
			*addr = (char *) PR_REALLOC(*addr, newLen);
			PR_ASSERT (*addr);
			lt = PL_strchr(*addr, '<');
			PR_ASSERT(lt);
      nsCRT::memmove(lt+4, lt+1, newLen - 4 - (lt - *addr));
			*lt++ = '&';
			*lt++ = 'l';
			*lt++ = 't';
			*lt = ';';
		} while (PL_strchr(*addr, '<'));
	}
}

static void  
mime_intl_mimepart_2_str(char **str, char *mcharset)
{
  //
  // RICHIE - Big question here, what charset should I be converting
  // to when I pull up old data????
  //
	if (str && *str)
	{
    // Now do conversion to UTF-8???
    char  *newStr = NULL;
    PRInt32 newStrLen;
    PRInt32 res = MIME_ConvertCharset(PR_TRUE, mcharset, "UTF-8", *str, PL_strlen(*str), 
                                    &newStr, &newStrLen);
		if ( (NS_SUCCEEDED(res)) && (newStr && newStr != *str))
		{
			PR_FREEIF(*str);
			*str = newStr;
		}
		else
		{
			MIME_StripContinuations(*str);
		}
	}
}

static void 
mime_intl_insert_message_header_1(char        **body, 
                                  char        **hdr_value,
											            char        *hdr_str, 
											            const char  *html_hdr_str,
											            char        *mailcharset,
											            PRBool      htmlEdit)
{
	if (!body || !hdr_value || !hdr_str)
		return;

	mime_intl_mimepart_2_str(hdr_value, mailcharset);
	if (htmlEdit)
	{
		mime_SACat(body, HEADER_START_JUNK);
	}
	else
	{
		mime_SACat(body, MSG_LINEBREAK);
	}
	if (!html_hdr_str)
		html_hdr_str = hdr_str;
	mime_SACat(body, html_hdr_str);
	if (htmlEdit)
	{
		mime_SACat(body, HEADER_MIDDLE_JUNK);
	}
	else
		mime_SACat(body, ": ");
	mime_SACat(body, *hdr_value);
	if (htmlEdit)
		mime_SACat(body, HEADER_END_JUNK);
}

static void 
mime_insert_all_headers(char            **body,
									      MimeHeaders     *headers,
									      nsMsgEditorType editorType,
									      char            *mailcharset)
{
	PRBool htmlEdit = editorType == nsMsgHTMLEditor;
	char *newBody = NULL;
	char *html_tag = PL_strcasestr(*body, "<HTML>");
	int i;

	if (!headers->done_p)
	{
		MimeHeaders_build_heads_list(headers);
		headers->done_p = PR_TRUE;
	}

	if (htmlEdit)
	{
		if (html_tag)
		{
			*html_tag = 0;
			mime_SACopy(&(newBody), *body);
			*html_tag = '<';
			mime_SACat(&newBody, 
					 "<HTML> <BR><BR>-------- Original Message --------");
		}
		else
		{
			mime_SACopy(&(newBody), 
					 "<HTML> <BR><BR>-------- Original Message --------");
		}
		mime_SACat(&newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 MSG_LINEBREAK MSG_LINEBREAK "-------- Original Message --------");
	}

	for (i = 0; i < headers->heads_size; i++)
	{
	  char *head = headers->heads[i];
	  char *end = (i == headers->heads_size-1
				   ? headers->all_headers + headers->all_headers_fp
				   : headers->heads[i+1]);
	  char *colon, *ocolon;
	  char *contents;
	  char *name = 0;
	  char *c2 = 0;

	  // Hack for BSD Mailbox delimiter. 
	  if (i == 0 && head[0] == 'F' && !PL_strncmp(head, "From ", 5))
		{
		  colon = head + 4;
		  contents = colon + 1;
		}
	  else
		{
		  /* Find the colon. */
		  for (colon = head; colon < end; colon++)
			if (*colon == ':') break;

		  if (colon >= end) continue;   /* junk */

		  /* Back up over whitespace before the colon. */
		  ocolon = colon;
		  for (; colon > head && IS_SPACE(colon[-1]); colon--)
			;

		  contents = ocolon + 1;
		}

	  /* Skip over whitespace after colon. */
	  while (contents <= end && IS_SPACE(*contents))
		contents++;

	  /* Take off trailing whitespace... */
	  while (end > contents && IS_SPACE(end[-1]))
		end--;

	  name = (char *)PR_MALLOC(colon - head + 1);
	  if (!name) 
      return /* MIME_OUT_OF_MEMORY */;
	  nsCRT::memcpy(name, head, colon - head);
	  name[colon - head] = 0;

	  c2 = (char *)PR_MALLOC(end - contents + 1);
	  if (!c2)
		{
		  PR_Free(name);
		  return /* MIME_OUT_OF_MEMORY */;
		}
	  nsCRT::memcpy(c2, contents, end - contents);
	  c2[end - contents] = 0;
	  
	  if (htmlEdit) mime_fix_up_html_address(&c2);
		  
	  mime_intl_insert_message_header_1(&newBody, &c2, name, name, mailcharset,
										htmlEdit); 
	  PR_Free(name);
	  PR_Free(c2);
	}

	if (htmlEdit)
	{
		mime_SACat(&newBody, "</TABLE>");
		mime_SACat(&newBody, MSG_LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(&newBody, html_tag+6);
		else
			mime_SACat(&newBody, *body);
	}
	else
	{
		mime_SACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
		mime_SACat(&newBody, *body);
	}

	if (newBody)
	{
		PR_FREEIF(*body);
		*body = newBody;
	}
}

char *
MimeGetNamedString(PRInt32 id)
{
  static char   retString[256];

  retString[0] = '\0';
  char *tString = MimeGetStringByID(id);
  if (tString)
    PL_strncpy(retString, tString, sizeof(retString)); 

  return retString;
}

static void 
mime_insert_normal_headers(char             **body,
									         MimeHeaders      *headers,
									         nsMsgEditorType  editorType,
									         char             *mailcharset)
{
	char *newBody = NULL;
	char *subject = MimeHeaders_get(headers, HEADER_SUBJECT, PR_FALSE, PR_FALSE);
	char *resent_comments = MimeHeaders_get(headers, HEADER_RESENT_COMMENTS,
											PR_FALSE, PR_FALSE);
	char *resent_date = MimeHeaders_get(headers, HEADER_RESENT_DATE, PR_FALSE,
										PR_TRUE); 
	char *resent_from = MimeHeaders_get(headers, HEADER_RESENT_FROM, PR_FALSE,
										PR_TRUE);
	char *resent_to = MimeHeaders_get(headers, HEADER_RESENT_TO, PR_FALSE, PR_TRUE);
	char *resent_cc = MimeHeaders_get(headers, HEADER_RESENT_CC, PR_FALSE, PR_TRUE);
	char *date = MimeHeaders_get(headers, HEADER_DATE, PR_FALSE, PR_TRUE);
	char *from = MimeHeaders_get(headers, HEADER_FROM, PR_FALSE, PR_TRUE);
	char *reply_to = MimeHeaders_get(headers, HEADER_REPLY_TO, PR_FALSE, PR_TRUE);
	char *organization = MimeHeaders_get(headers, HEADER_ORGANIZATION,
										 PR_FALSE, PR_FALSE);
	char *to = MimeHeaders_get(headers, HEADER_TO, PR_FALSE, PR_TRUE);
	char *cc = MimeHeaders_get(headers, HEADER_CC, PR_FALSE, PR_TRUE);
	char *bcc = MimeHeaders_get(headers, HEADER_BCC, PR_FALSE, PR_TRUE);
	char *newsgroups = MimeHeaders_get(headers, HEADER_NEWSGROUPS, PR_FALSE,
									   PR_TRUE);
	char *followup_to = MimeHeaders_get(headers, HEADER_FOLLOWUP_TO, PR_FALSE,
										PR_TRUE);
	char *references = MimeHeaders_get(headers, HEADER_REFERENCES, PR_FALSE, PR_TRUE);
	const char *html_tag = PL_strcasestr(*body, "<HTML>");
	PRBool htmlEdit = editorType == nsMsgHTMLEditor;
	
	if (!from)
		from = MimeHeaders_get(headers, HEADER_SENDER, PR_FALSE, PR_TRUE);
	if (!resent_from)
		resent_from = MimeHeaders_get(headers, HEADER_RESENT_SENDER, PR_FALSE,
									  PR_TRUE); 
	
	if (htmlEdit)
	{
		mime_SACopy(&(newBody), 
					 "<HTML> <BR><BR>-------- Original Message --------");
		mime_SACat(&newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 MSG_LINEBREAK MSG_LINEBREAK "-------- Original Message --------");
	}
	if (subject)
		mime_intl_insert_message_header_1(&newBody, &subject, HEADER_SUBJECT,
										  MimeGetNamedString(MIME_MHTML_SUBJECT),
											mailcharset, htmlEdit);
	if (resent_comments)
		mime_intl_insert_message_header_1(&newBody, &resent_comments,
										  HEADER_RESENT_COMMENTS, 
										  MimeGetNamedString(MIME_MHTML_RESENT_COMMENTS),
											mailcharset, htmlEdit);
	if (resent_date)
		mime_intl_insert_message_header_1(&newBody, &resent_date,
										  HEADER_RESENT_DATE,
										  MimeGetNamedString(MIME_MHTML_RESENT_DATE),
											mailcharset, htmlEdit);
	if (resent_from)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_from);
		mime_intl_insert_message_header_1(&newBody, &resent_from,
										  HEADER_RESENT_FROM,
										  MimeGetNamedString(MIME_MHTML_RESENT_FROM),
											mailcharset, htmlEdit);
	}
	if (resent_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_to);
		mime_intl_insert_message_header_1(&newBody, &resent_to,
										  HEADER_RESENT_TO,
										  MimeGetNamedString(MIME_MHTML_RESENT_TO),
											mailcharset, htmlEdit);
	}
	if (resent_cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_cc);
		mime_intl_insert_message_header_1(&newBody, &resent_cc,
										  HEADER_RESENT_CC,
										  MimeGetNamedString(MIME_MHTML_RESENT_CC),
											mailcharset, htmlEdit);
	}
	if (date)
		mime_intl_insert_message_header_1(&newBody, &date, HEADER_DATE,
										  MimeGetNamedString(MIME_MHTML_DATE),
										  mailcharset, htmlEdit);
	if (from)
	{
		if (htmlEdit) mime_fix_up_html_address(&from);
		mime_intl_insert_message_header_1(&newBody, &from, HEADER_FROM,
										  MimeGetNamedString(MIME_MHTML_FROM),
										  mailcharset, htmlEdit);
	}
	if (reply_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&reply_to);
		mime_intl_insert_message_header_1(&newBody, &reply_to, HEADER_REPLY_TO,
										  MimeGetNamedString(MIME_MHTML_REPLY_TO),
										  mailcharset, htmlEdit);
	}
	if (organization)
		mime_intl_insert_message_header_1(&newBody, &organization,
										  HEADER_ORGANIZATION,
										  MimeGetNamedString(MIME_MHTML_ORGANIZATION),
										  mailcharset, htmlEdit);
	if (to)
	{
		if (htmlEdit) mime_fix_up_html_address(&to);
		mime_intl_insert_message_header_1(&newBody, &to, HEADER_TO,
										  MimeGetNamedString(MIME_MHTML_TO),
										  mailcharset, htmlEdit);
	}
	if (cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&cc);
		mime_intl_insert_message_header_1(&newBody, &cc, HEADER_CC,
										  MimeGetNamedString(MIME_MHTML_CC),
										  mailcharset, htmlEdit);
	}
	if (bcc)
	{
		if (htmlEdit) mime_fix_up_html_address(&bcc);
		mime_intl_insert_message_header_1(&newBody, &bcc, HEADER_BCC,
										  MimeGetNamedString(MIME_MHTML_BCC),
										  mailcharset, htmlEdit);
	}
	if (newsgroups)
		mime_intl_insert_message_header_1(&newBody, &newsgroups, HEADER_NEWSGROUPS,
										  MimeGetNamedString(MIME_MHTML_NEWSGROUPS),
										  mailcharset, htmlEdit);
	if (followup_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&followup_to);
		mime_intl_insert_message_header_1(&newBody, &followup_to,
										  HEADER_FOLLOWUP_TO,
										  MimeGetNamedString(MIME_MHTML_FOLLOWUP_TO),
										  mailcharset, htmlEdit);
	}
	if (references)
	{
		if (htmlEdit) mime_fix_up_html_address(&references);
		mime_intl_insert_message_header_1(&newBody, &references,
										  HEADER_REFERENCES,
										  MimeGetNamedString(MIME_MHTML_REFERENCES),
										  mailcharset, htmlEdit);
	}
	if (htmlEdit)
	{
		mime_SACat(&newBody, "</TABLE>");
		mime_SACat(&newBody, MSG_LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(&newBody, html_tag+6);
		else
			mime_SACat(&newBody, *body);
	}
	else
	{
		mime_SACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
		mime_SACat(&newBody, *body);
	}
	if (newBody)
	{
		PR_Free(*body);
		*body = newBody;
	}
	PR_FREEIF(subject);
	PR_FREEIF(resent_comments);
	PR_FREEIF(resent_date);
	PR_FREEIF(resent_from);
	PR_FREEIF(resent_to);
	PR_FREEIF(resent_cc);
	PR_FREEIF(date);
	PR_FREEIF(from);
	PR_FREEIF(reply_to);
	PR_FREEIF(organization);
	PR_FREEIF(to);
	PR_FREEIF(cc);
	PR_FREEIF(bcc);
	PR_FREEIF(newsgroups);
	PR_FREEIF(followup_to);
	PR_FREEIF(references);
}

static void 
mime_insert_micro_headers(char            **body,
									        MimeHeaders     *headers,
									        nsMsgEditorType editorType,
									        char            *mailcharset)
{
	char *newBody = NULL;
	char *subject = MimeHeaders_get(headers, HEADER_SUBJECT, PR_FALSE, PR_FALSE);
	char *from = MimeHeaders_get(headers, HEADER_FROM, PR_FALSE, PR_TRUE);
	char *resent_from = MimeHeaders_get(headers, HEADER_RESENT_FROM, PR_FALSE,
										PR_TRUE); 
	char *date = MimeHeaders_get(headers, HEADER_DATE, PR_FALSE, PR_TRUE);
	char *to = MimeHeaders_get(headers, HEADER_TO, PR_FALSE, PR_TRUE);
	char *cc = MimeHeaders_get(headers, HEADER_CC, PR_FALSE, PR_TRUE);
	char *bcc = MimeHeaders_get(headers, HEADER_BCC, PR_FALSE, PR_TRUE);
	char *newsgroups = MimeHeaders_get(headers, HEADER_NEWSGROUPS, PR_FALSE,
									   PR_TRUE);
	const char *html_tag = PL_strcasestr(*body, "<HTML>");
	PRBool htmlEdit = editorType == nsMsgHTMLEditor;
	
	if (!from)
		from = MimeHeaders_get(headers, HEADER_SENDER, PR_FALSE, PR_TRUE);
	if (!resent_from)
		resent_from = MimeHeaders_get(headers, HEADER_RESENT_SENDER, PR_FALSE,
									  PR_TRUE); 
	if (!date)
		date = MimeHeaders_get(headers, HEADER_RESENT_DATE, PR_FALSE, PR_TRUE);
	
	if (htmlEdit)
	{
		mime_SACopy(&(newBody), 
					 "<HTML> <BR><BR>-------- Original Message --------");
	    mime_SACat(&newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 MSG_LINEBREAK MSG_LINEBREAK "-------- Original Message --------");
	}
	if (from)
	{
		if (htmlEdit) 
      mime_fix_up_html_address(&from);
		mime_intl_insert_message_header_1(&newBody, &from, HEADER_FROM,
										MimeGetNamedString(MIME_MHTML_FROM),
										mailcharset, htmlEdit);
	}
	if (subject)
		mime_intl_insert_message_header_1(&newBody, &subject, HEADER_SUBJECT,
										MimeGetNamedString(MIME_MHTML_SUBJECT),
											mailcharset, htmlEdit);
/*
	if (date)
		mime_intl_insert_message_header_1(&newBody, &date, HEADER_DATE,
										MimeGetNamedString(MIME_MHTML_DATE),
										mailcharset, htmlEdit);
*/
	if (resent_from)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_from);
		mime_intl_insert_message_header_1(&newBody, &resent_from,
										HEADER_RESENT_FROM,
										MimeGetNamedString(MIME_MHTML_RESENT_FROM),
										mailcharset, htmlEdit);
	}
	if (to)
	{
		if (htmlEdit) mime_fix_up_html_address(&to);
		mime_intl_insert_message_header_1(&newBody, &to, HEADER_TO,
										MimeGetNamedString(MIME_MHTML_TO),
										mailcharset, htmlEdit);
	}
	if (cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&cc);
		mime_intl_insert_message_header_1(&newBody, &cc, HEADER_CC,
										MimeGetNamedString(MIME_MHTML_CC),
										mailcharset, htmlEdit);
	}
	if (bcc)
	{
		if (htmlEdit) mime_fix_up_html_address(&bcc);
		mime_intl_insert_message_header_1(&newBody, &bcc, HEADER_BCC,
										MimeGetNamedString(MIME_MHTML_BCC),
										mailcharset, htmlEdit);
	}
	if (newsgroups)
		mime_intl_insert_message_header_1(&newBody, &newsgroups, HEADER_NEWSGROUPS,
										MimeGetNamedString(MIME_MHTML_NEWSGROUPS),
										mailcharset, htmlEdit);
	if (htmlEdit)
	{
		mime_SACat(&newBody, "</TABLE>");
		mime_SACat(&newBody, MSG_LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(&newBody, html_tag+6);
		else
			mime_SACat(&newBody, *body);
	}
	else
	{
		mime_SACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
		mime_SACat(&newBody, *body);
	}
	if (newBody)
	{
		PR_Free(*body);
		*body = newBody;
	}
	PR_FREEIF(subject);
	PR_FREEIF(from);
	PR_FREEIF(resent_from);
	PR_FREEIF(date);
	PR_FREEIF(to);
	PR_FREEIF(cc);
	PR_FREEIF(bcc);
	PR_FREEIF(newsgroups);

}

static void 
mime_insert_forwarded_message_headers(char            **body, 
												              MimeHeaders     *headers,
												              nsMsgEditorType editorType,
												              char            *mailcharset)
{
	if (body && *body && headers)
	{
		PRInt32     show_headers = 0;
    nsresult    res;
    
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
    if (NS_SUCCEEDED(res) && prefs)
      res = prefs->GetIntPref("mail.show_headers", &show_headers);

		switch (show_headers)
		{
		case 0:
			mime_insert_micro_headers(body, headers, editorType, mailcharset);
			break;
		default:
		case 1:
			mime_insert_normal_headers(body, headers, editorType, mailcharset);
			break;
		case 2:
			mime_insert_all_headers(body, headers, editorType, mailcharset);
			break;
		}
	}
}

static void
mime_parse_stream_complete (nsMIMESession *stream)
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;
  nsIMsgCompFields *fields = NULL;
  int htmlAction = 0;
  int lineWidth = 0;
  
  char *host = 0;
  char *news_host = 0;
  char *to_and_cc = 0;
  char *re_subject = 0;
  char *new_refs = 0;
  char *from = 0;
  char *repl = 0;
  char *subj = 0;
  char *id = 0;
  char *refs = 0;
  char *to = 0;
  char *cc = 0;
  char *bcc = 0; 
  char *fcc = 0; 
  char *org = 0; 
  char *grps = 0;
  char *foll = 0;
  char *priority = 0;
  char *draftInfo = 0;
  
  PRBool xlate_p = PR_FALSE;	/* #### how do we determine this? */
  PRBool sign_p = PR_FALSE;		/* #### how do we determine this? */
  PRBool forward_inline = PR_FALSE;
  
  PR_ASSERT (mdd);
  
  if (!mdd) return;
  
  if (mdd->obj) 
  {
    int status;
    
    status = mdd->obj->clazz->parse_eof ( mdd->obj, PR_FALSE );
    mdd->obj->clazz->parse_end( mdd->obj, status < 0 ? PR_TRUE : PR_FALSE );
    
    xlate_p = mdd->options->dexlate_p;
    sign_p = mdd->options->signed_p;
    
    // RICHIE
    // We need to figure out how to pass the forwarded flag along with this
    // operation.
    
    // forward_inline = (mdd->format_out != FO_CMDLINE_ATTACHMENTS);    
    if (forward_inline)
    {
#ifdef MOZ_SECURITY
      HG88890
#endif
    }
    
    PR_ASSERT ( mdd->options == mdd->obj->options );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options) 
    {
      PR_FREEIF (mdd->options->part_to_load);
      PR_Free(mdd->options);
      mdd->options = 0;
    }
    if (mdd->stream) 
    {
      mdd->stream->complete ((nsMIMESession *)mdd->stream->data_object);
      PR_Free( mdd->stream );
      mdd->stream = 0;
    }
  }

  //
  // Now, process the attachments that we have gathered from the message
  // on disk
  //
  nsMsgAttachmentData     *newAttachData = nsnull;
  if (mdd && (mdd->attachments_count > 0))
    newAttachData = mime_draft_process_attachments(mdd);
  
  //
  // time to bring up the compose windows with all the info gathered 
  //
  if ( mdd->headers )
  {
    subj = MimeHeaders_get(mdd->headers, HEADER_SUBJECT,  PR_FALSE, PR_FALSE);
    if (forward_inline)
    {
      if (subj)
      {
        char *newSubj = PR_smprintf("[Fwd: %s]", subj);
        if (newSubj)
        {
          PR_Free(subj);
          subj = newSubj;
        }
      }
    }	
    else
    {
      repl = MimeHeaders_get(mdd->headers, HEADER_REPLY_TO, PR_FALSE, PR_FALSE);
      to   = MimeHeaders_get(mdd->headers, HEADER_TO,       PR_FALSE, PR_TRUE);
      cc   = MimeHeaders_get(mdd->headers, HEADER_CC,       PR_FALSE, PR_TRUE);
      bcc   = MimeHeaders_get(mdd->headers, HEADER_BCC,       PR_FALSE, PR_TRUE);
      
      /* These headers should not be RFC-1522-decoded. */
      grps = MimeHeaders_get(mdd->headers, HEADER_NEWSGROUPS,  PR_FALSE, PR_TRUE);
      foll = MimeHeaders_get(mdd->headers, HEADER_FOLLOWUP_TO, PR_FALSE, PR_TRUE);
      
      host = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_NEWSHOST, PR_FALSE, PR_FALSE);
      if (!host)
        host = MimeHeaders_get(mdd->headers, HEADER_NNTP_POSTING_HOST, PR_FALSE, PR_FALSE);
      
      id   = MimeHeaders_get(mdd->headers, HEADER_MESSAGE_ID,  PR_FALSE, PR_FALSE);
      refs = MimeHeaders_get(mdd->headers, HEADER_REFERENCES,  PR_FALSE, PR_TRUE);
      priority = MimeHeaders_get(mdd->headers, HEADER_X_PRIORITY, PR_FALSE, PR_FALSE);
      
      mime_intl_mimepart_2_str(&repl, mdd->mailcharset);
      mime_intl_mimepart_2_str(&to, mdd->mailcharset);
      mime_intl_mimepart_2_str(&cc, mdd->mailcharset);
      mime_intl_mimepart_2_str(&bcc, mdd->mailcharset);
      mime_intl_mimepart_2_str(&grps, mdd->mailcharset);
      mime_intl_mimepart_2_str(&foll, mdd->mailcharset);
      mime_intl_mimepart_2_str(&host, mdd->mailcharset);
      
      if (host) 
      {
        char *secure = NULL;
        
        secure = PL_strcasestr(host, "secure");
        if (secure) 
        {
          *secure = 0;
          news_host = PR_smprintf ("snews://%s", host);
        }
        else 
        {
          news_host = PR_smprintf ("news://%s", host);
        }
      }
    }
    
    mime_intl_mimepart_2_str(&subj, mdd->mailcharset);
    
    fields = CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
      org, subj, refs, 0, priority, 0, news_host,
      xlate_p, sign_p);
    
    draftInfo = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_DRAFT_INFO, PR_FALSE, PR_FALSE);
    if (draftInfo && fields && !forward_inline) 
    {
      char *parm = 0;
      parm = MimeHeaders_get_parameter(draftInfo, "vcard", NULL, NULL);
      if (parm && !PL_strcmp(parm, "1"))
        fields->SetBoolHeader(nsMsg_ATTACH_VCARD_BOOL_HEADER_MASK, PR_TRUE);
      else
        fields->SetBoolHeader(nsMsg_ATTACH_VCARD_BOOL_HEADER_MASK, PR_FALSE);
      
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "receipt", NULL, NULL);
      if (parm && !PL_strcmp(parm, "0"))
        fields->SetBoolHeader(nsMsg_RETURN_RECEIPT_BOOL_HEADER_MASK, PR_FALSE); 
      else
      {
        int receiptType = 0;
        fields->SetBoolHeader(nsMsg_RETURN_RECEIPT_BOOL_HEADER_MASK, PR_TRUE); 
        sscanf(parm, "%d", &receiptType);
        fields->SetReturnReceipt((PRInt32) receiptType);
      }
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "uuencode", NULL, NULL);
      if (parm && !PL_strcmp(parm, "1"))
        fields->SetBoolHeader(nsMsg_UUENCODE_BINARY_BOOL_HEADER_MASK, PR_TRUE);
      else
        fields->SetBoolHeader(nsMsg_UUENCODE_BINARY_BOOL_HEADER_MASK, PR_FALSE);
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "html", NULL, NULL);
      if (parm)
        sscanf(parm, "%d", &htmlAction);
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "linewidth", NULL, NULL);
      if (parm)
        sscanf(parm, "%d", &lineWidth);
      PR_FREEIF(parm);
      
    }
    
    if (mdd->messageBody) 
    {
      char *body;
      PRUint32 bodyLen = 0;
      nsMsgEditorType editorType = nsMsgDefault;
      
      bodyLen = mdd->messageBody->file_spec->GetFileSize();
      body = (char *)PR_MALLOC (bodyLen + 1);
      if (body)
      {
        nsCRT::memset (body, 0, bodyLen+1);
        
        nsInputFileStream inputFile(*(mdd->messageBody->file_spec));
        if (inputFile.is_open())
          inputFile.read(body, bodyLen);
        
        inputFile.close();
        
        if (mdd->messageBody->type && *mdd->messageBody->type)
        {
          if( PL_strcasestr(mdd->messageBody->type, "text/html") != NULL )
            editorType = nsMsgHTMLEditor;
          else if ( PL_strcasestr(mdd->messageBody->type, "text/plain") != NULL )
            editorType = nsMsgPlainTextEditor;
        }
        else
        {
          editorType = nsMsgPlainTextEditor;
        }
  
        // Since we have body text, then we should set the compose fields with
        // this data.      
        if (editorType == nsMsgPlainTextEditor)
          fields->SetTheForcePlainText(PR_TRUE);
        
        if (forward_inline)
        {
          mime_insert_forwarded_message_headers(&body, mdd->headers, editorType,
                                                mdd->mailcharset);
          bodyLen = PL_strlen(body);
        }

        //
        // RICHIE - once again, we need to know what we should be converting
        // to in order to do the right thing here!!!
        //
        // Now do conversion to UTF-8???
        char      *newBody = NULL;
        PRInt32   newBodyLen;
        PRInt32 res = MIME_ConvertCharset(PR_TRUE, mdd->mailcharset, "UTF-8", body, 
                                          PL_strlen(body), &newBody, &newBodyLen);
		    if ( (NS_SUCCEEDED(res)) && (newBody && newBody != body))
		    {
			    PR_FREEIF(body);
          // RICHIE SHERRY
          //
          // Insert the META tag to tell Gecko it's UTF-8...
          //
          nsString      aNewBody("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">");
          aNewBody.Append(newBody);

          PR_FREEIF(newBody);

          fields->SetCharacterSet(nsString("UTF-8").GetUnicode());
          fields->SetBody(aNewBody.GetUnicode());
          // RICHIE SHERRY
          // RICHIE SHERRY OLD STUFF body = newBody;
          // RICHIE SHERRY OLD STUFF bodyLen = newBodyLen;
		    }
        else
        {
          fields->SetCharacterSet(nsString(mdd->mailcharset).GetUnicode());
          fields->SetBody(nsString(body).GetUnicode());
        }

        PR_FREEIF(body);
      } // end if (messageBody)
  
      //
      // At this point, we need to create a message compose window or editor
      // window via XP-COM with the information that we have retrieved from 
      // the message store.
      //
      if (mdd->format_out == nsMimeOutput::nsMimeMessageEditorTemplate)
      {
        printf("RICHIE: Time to create the EDITOR with this template - HAS a body!!!!\n");
      }
      else
      {
#ifdef NS_DEBUG
        printf("Time to create the composition window WITH a body!!!!\n");
#endif
        CreateTheComposeWindow(fields, newAttachData, editorType);
      }
      
      PR_FREEIF(body);
      mime_free_attachments (mdd->messageBody, 1);
    }
    else
    {
      //
      // At this point, we need to create a message compose window via
      // XP-COM with the information that we have retrieved from the message store.
      //
      if (mdd->format_out == nsMimeOutput::nsMimeMessageEditorTemplate)
      {
        printf("RICHIE: Time to create the EDITOR with this template - NO body!!!!\n");
        CreateTheComposeWindow(fields, newAttachData, nsMsgDefault);
      }
      else
      {
#ifdef NS_DEBUG
        printf("Time to create the composition window WITHOUT a body!!!!\n");
#endif
        CreateTheComposeWindow(fields, newAttachData, nsMsgDefault);
      }
    }    
  }
  else
  {
    fields = CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
                                      org, subj, refs, 0, priority, 0, news_host,
                                      GetMailXlateionPreference(), 
                                      GetMailSigningPreference());
    if (fields)
      CreateTheComposeWindow(fields, newAttachData, nsMsgDefault);
  }
  
  if ( mdd->headers )
    MimeHeaders_free ( mdd->headers );

  // 
  // Free the original attachment structure...
  // Make sure we only cleanup the local copy of the memory and not kill 
  // files we need on disk
  //
  if (mdd->attachments)
  {
    int               i;
    nsMsgAttachedFile *cur = mdd->attachments;

    for ( i = 0; i < mdd->attachments_count; i++, cur++ ) 
    {
	    if ( cur->file_spec ) 
      {
	      delete ( cur->file_spec );
        cur->file_spec = nsnull;
      }
    }

    mime_free_attachments( mdd->attachments, mdd->attachments_count );
  }

  PR_FREEIF(mdd);
  if (fields)
    NS_RELEASE(fields);
  
  // Release the prefs service
  MimeObject *obj = (mdd ? mdd->obj : 0);  
  if ( (obj) && (obj->options) && (obj->options->prefs) )
    nsServiceManager::ReleaseService(kPrefCID, obj->options->prefs);

  PR_Free (mdd);
  
  PR_FREEIF(host);
  PR_FREEIF(to_and_cc);
  PR_FREEIF(re_subject);
  PR_FREEIF(new_refs);
  PR_FREEIF(from);
  PR_FREEIF(repl);
  PR_FREEIF(subj);
  PR_FREEIF(id);
  PR_FREEIF(refs);
  PR_FREEIF(to);
  PR_FREEIF(cc);
  PR_FREEIF(grps);
  PR_FREEIF(foll);
  PR_FREEIF(priority);
  PR_FREEIF(draftInfo);  
}

static void
mime_parse_stream_abort (nsMIMESession *stream, int status )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;  
  PR_ASSERT (mdd);

  if (!mdd) 
    return;
  
  if (mdd->obj) 
  {
    int status;

    if ( !mdd->obj->closed_p )
      status = mdd->obj->clazz->parse_eof ( mdd->obj, PR_TRUE );
    if ( !mdd->obj->parsed_p )
      mdd->obj->clazz->parse_end( mdd->obj, PR_TRUE );
    
    PR_ASSERT ( mdd->options == mdd->obj->options );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options) 
    {
      PR_FREEIF (mdd->options->part_to_load);
      PR_Free(mdd->options);
      mdd->options = 0;
    }

    if (mdd->stream) 
    {
      mdd->stream->abort ((nsMIMESession *)mdd->stream->data_object, status);
      PR_Free( mdd->stream );
      mdd->stream = 0;
    }
  }

  if ( mdd->headers )
    MimeHeaders_free (mdd->headers);


  if (mdd->attachments)
    mime_free_attachments( mdd->attachments, mdd->attachments_count );

  PR_Free (mdd);
}

static int
make_mime_headers_copy ( void *closure, MimeHeaders *headers )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) closure;

  PR_ASSERT ( mdd && headers );
  
  if ( !mdd || ! headers ) 
    return 0;

  PR_ASSERT ( mdd->headers == NULL );

  mdd->headers = MimeHeaders_copy ( headers );
  mdd->options->done_parsing_outer_headers = PR_TRUE;

  return 0;
}

int 
mime_decompose_file_init_fn ( void *stream_closure, MimeHeaders *headers )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  nsMsgAttachedFile *attachments = 0, *newAttachment = 0;
  int nAttachments = 0;
  char *hdr_value = NULL, *parm_value = NULL;
  PRBool needURL = PR_FALSE;
  PRBool creatingMsgBody = PR_TRUE;
  PRBool bodyPart = PR_FALSE;
  
  PR_ASSERT (mdd && headers);
  if (!mdd || !headers) 
    return -1;

  if ( !mdd->options->is_multipart_msg ) 
  {
    if (mdd->options->decompose_init_count) 
    {
      mdd->options->decompose_init_count++;
      PR_ASSERT(mdd->curAttachment);
      if (mdd->curAttachment) {
        char *ct = MimeHeaders_get(headers, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE);
        if (ct)
          mime_SACopy(&(mdd->curAttachment->type), ct);
        PR_FREEIF(ct);
      }
      return 0;
    }
    else {
      mdd->options->decompose_init_count++;
    }
  }

  nAttachments = mdd->attachments_count;

  if (!nAttachments && !mdd->messageBody) 
  {
    char *charset = NULL, *contentType = NULL;
    contentType = MimeHeaders_get(headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
    if (contentType) 
    {
      charset = MimeHeaders_get_parameter(contentType, "charset", NULL, NULL);
      mdd->mailcharset = PL_strdup(charset);
      PR_FREEIF(charset);
      PR_FREEIF(contentType);
    }
    
    mdd->messageBody = PR_NEWZAP (nsMsgAttachedFile);
    PR_ASSERT (mdd->messageBody);
    if (!mdd->messageBody) 
      return MIME_OUT_OF_MEMORY;
    newAttachment = mdd->messageBody;
    creatingMsgBody = PR_TRUE;
    bodyPart = PR_TRUE;
  }
  else 
  {
    /* always allocate one more extra; don't ask me why */
    needURL = PR_TRUE;
    if ( nAttachments ) 
    {
      PR_ASSERT (mdd->attachments);
      
      attachments = (nsMsgAttachedFile *)PR_REALLOC(mdd->attachments, 
								                                    sizeof (nsMsgAttachedFile) * 
                                                    (nAttachments + 2));
    if (!attachments)
        return MIME_OUT_OF_MEMORY;
      mdd->attachments = attachments;
      mdd->attachments_count++;
    }
    else {
      PR_ASSERT (!mdd->attachments);
      
      attachments = (nsMsgAttachedFile *) PR_MALLOC ( sizeof (nsMsgAttachedFile) * 2);
      if (!attachments) 
        return MIME_OUT_OF_MEMORY;
      mdd->attachments_count++;
      mdd->attachments = attachments;
    }
    
    newAttachment = attachments + nAttachments;
    nsCRT::memset ( newAttachment, 0, sizeof (nsMsgAttachedFile) * 2 );
  }

  char *workURLSpec = nsnull;
  char *contLoc = nsnull;

  newAttachment->real_name = MimeHeaders_get_name ( headers );
  contLoc = MimeHeaders_get( headers, HEADER_CONTENT_LOCATION, PR_FALSE, PR_FALSE );
  if (!contLoc)
      contLoc = MimeHeaders_get( headers, HEADER_CONTENT_BASE, PR_FALSE, PR_FALSE );

  if ( (!contLoc) && (newAttachment->real_name) )
    workURLSpec = PL_strdup(newAttachment->real_name);
  if ( (contLoc) && (!workURLSpec) )
    workURLSpec = PL_strdup(contLoc);

  PR_FREEIF(contLoc);

  mdd->curAttachment = newAttachment;  
  newAttachment->type =  MimeHeaders_get ( headers, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE );
  
  //
  // This is to handle the degenerated Apple Double attachment.
  //
  parm_value = MimeHeaders_get( headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE );
  if (parm_value) 
  {
    char *boundary = NULL;
    char *tmp_value = NULL;
    boundary = MimeHeaders_get_parameter(parm_value, "boundary", NULL, NULL);
    if (boundary)
      tmp_value = PR_smprintf("; boundary=\"%s\"", boundary);
    if (tmp_value)
      mime_SACat(&(newAttachment->type), tmp_value);
    newAttachment->x_mac_type = MimeHeaders_get_parameter(parm_value, "x-mac-type", NULL, NULL);
    newAttachment->x_mac_creator = MimeHeaders_get_parameter(parm_value, "x-mac-creator", NULL, NULL);
    PR_FREEIF(parm_value);
    PR_FREEIF(boundary);
    PR_FREEIF(tmp_value);
  }
  newAttachment->encoding = MimeHeaders_get ( headers, HEADER_CONTENT_TRANSFER_ENCODING,
                                              PR_FALSE, PR_FALSE );
  newAttachment->description = MimeHeaders_get( headers, HEADER_CONTENT_DESCRIPTION,
                                                PR_FALSE, PR_FALSE );
  // 
  // If we came up empty for description or the orig URL, we should do something about it.
  //
  if  ( ( (!newAttachment->description) || (!*newAttachment->description) ) && (workURLSpec) )
    newAttachment->description = PL_strdup(workURLSpec);

  nsFileSpec *tmpSpec = nsnull;
  if (newAttachment->real_name)
    tmpSpec = nsMsgCreateTempFileSpec(newAttachment->real_name);
  else
  {
    char *ptr = PL_strchr(newAttachment->type, '/');
    if (!ptr)
    {
      tmpSpec = nsMsgCreateTempFileSpec("nsmail.tmp");
    }
    else
    {
      ++ptr;
      workURLSpec = PR_smprintf("nsmail.%s", ptr);
      tmpSpec = nsMsgCreateTempFileSpec(workURLSpec);
    }
  }

  // This needs to be done so the attachment structure has a handle 
  // on the temp file for this attachment...
  // if ( (tmpSpec) && (!bodyPart) )
  if (tmpSpec)
  {
    char *tmpSpecStr = PR_smprintf("file://%s", tmpSpec->GetNativePathCString());

    if (tmpSpecStr)
    {
      char *slashPtr = tmpSpecStr;
      while (*slashPtr)
      {
        if (*slashPtr == '\\') 
          *slashPtr = '/';

        slashPtr++;
      }
      nsMimeNewURI(&(newAttachment->orig_url), tmpSpecStr);
      NS_IF_ADDREF(newAttachment->orig_url);
      PR_FREEIF(tmpSpecStr);
    }
  }

  PR_FREEIF(workURLSpec);
  if (!tmpSpec)
    return MIME_OUT_OF_MEMORY;

  NS_NewFileSpecWithSpec(*tmpSpec, &mdd->tmpFileSpec);
	if (!mdd->tmpFileSpec)
    return MIME_OUT_OF_MEMORY;
  
  newAttachment->file_spec = tmpSpec;

  mdd->tmpFileStream = new nsOutputFileStream(mdd->tmpFileSpec);
  if (!mdd->tmpFileStream)
    return MIME_UNABLE_TO_OPEN_TMP_FILE;
    
  // For now, we are always going to decode all of the attachments 
  // for the message. This way, we have native data 
  if (creatingMsgBody) 
  {
    MimeDecoderData *(*fn) (int (*) (const char*, PRInt32, void*), void*) = 0;
    
    //
    // Initialize a decoder if necessary.
    //
    if (!newAttachment->encoding || mdd->options->dexlate_p)
      ;
    else if (!PL_strcasecmp(newAttachment->encoding, ENCODING_BASE64))
      fn = &MimeB64DecoderInit;
    else if (!PL_strcasecmp(newAttachment->encoding, ENCODING_QUOTED_PRINTABLE))
      fn = &MimeQPDecoderInit;
    else if (!PL_strcasecmp(newAttachment->encoding, ENCODING_UUENCODE) ||
             !PL_strcasecmp(newAttachment->encoding, ENCODING_UUENCODE2) ||
             !PL_strcasecmp(newAttachment->encoding, ENCODING_UUENCODE3) ||
             !PL_strcasecmp(newAttachment->encoding, ENCODING_UUENCODE4))
      fn = &MimeUUDecoderInit;
    
    if (fn) 
    {
      mdd->decoder_data = fn (/* The (int (*) ...) cast is to turn the `void' argument into `MimeObject'. */
                              ((int (*) (const char *, PRInt32, void *))
                              dummy_file_write), mdd->tmpFileStream);
      if (!mdd->decoder_data)
        return MIME_OUT_OF_MEMORY;
    }
  }

  return 0;
}

int 
mime_decompose_file_output_fn (char     *buf,
								               PRInt32  size,
								               void     *stream_closure )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  int ret = 0;
  
  PR_ASSERT (mdd && buf);
  if (!mdd || !buf) return -1;
  if (!size) return 0;
  
  if ( !mdd->tmpFileStream ) 
    return 0;
  
  if (mdd->decoder_data) {
    ret = MimeDecoderWrite(mdd->decoder_data, buf, size);
    if (ret == -1) return -1;
  }
  else 
  {
    ret = mdd->tmpFileStream->write(buf, size);
    if (ret < size)
      return MIME_ERROR_WRITING_FILE;
  }
  
  return 0;
}

int
mime_decompose_file_close_fn ( void *stream_closure )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  
  if ( !mdd || !mdd->tmpFileStream )
    return -1;
  
  if ( !mdd->options->is_multipart_msg ) 
  {
    if ( --mdd->options->decompose_init_count > 0 )
      return 0;
  }
  
  if (mdd->decoder_data) {
    MimeDecoderDestroy(mdd->decoder_data, PR_FALSE);
    mdd->decoder_data = 0;
  }
  
  mdd->tmpFileStream->close();
  delete mdd->tmpFileStream;
  delete mdd->tmpFileSpec;
  mdd->tmpFileSpec = nsnull;
  
  return 0;
}

extern "C" void  *
mime_bridge_create_draft_stream(
                          nsIMimeEmitter      *newEmitter,
                          nsStreamConverter   *newPluginObj2,
                          nsIURI              *uri,
                          nsMimeOutputType    format_out)
{
  int                     status = 0;
  nsMIMESession           *stream = NULL;
  struct mime_draft_data  *mdd = NULL;
  MimeObject              *obj;

  if ( !uri ) 
    return nsnull;

  mdd = PR_NEWZAP(struct mime_draft_data);
  if (!mdd) 
    return nsnull;

  char *urlString;
  if (NS_SUCCEEDED(uri->GetSpec(&urlString)))
  {
    if ((urlString) && (*urlString))
    {
      mdd->url_name = PL_strdup(urlString);
      if (!(mdd->url_name))
      {
        PR_FREEIF(mdd);
        return nsnull;
      }

      PR_FREEIF(urlString);
    }
  }

  mdd->format_out = format_out;
  mdd->options = PR_NEWZAP  ( MimeDisplayOptions );
  if ( !mdd->options ) 
  {
    PR_FREEIF(mdd->url_name);
    PR_FREEIF(mdd);
    return nsnull;
  }

  mdd->options->url = mdd->url_name;
  mdd->options->format_out = format_out;     // output format
  mdd->options->decompose_file_p = PR_TRUE;	/* new field in MimeDisplayOptions */
  mdd->options->stream_closure = mdd;
  mdd->options->html_closure = mdd;
  mdd->options->decompose_headers_info_fn = make_mime_headers_copy;
  mdd->options->decompose_file_init_fn = mime_decompose_file_init_fn;
  mdd->options->decompose_file_output_fn = mime_decompose_file_output_fn;
  mdd->options->decompose_file_close_fn = mime_decompose_file_close_fn;

  nsresult rv = nsServiceManager::GetService(kPrefCID, nsIPref::GetIID(), (nsISupports**)&(mdd->options->prefs));
  if (! (mdd->options->prefs && NS_SUCCEEDED(rv)))
	{
    PR_FREEIF(mdd);
    return nsnull;
  }

#ifdef FO_MAIL_MESSAGE_TO
  /* If we're attaching a message (for forwarding) then we must eradicate all
	 traces of xlateion from it, since forwarding someone else a message
	 that wasn't xlated for them doesn't work.  We have to dexlate it
	 before sending it.
   */
// RICHIE  mdd->options->dexlate_p = PR_TRUE;
#endif /* FO_MAIL_MESSAGE_TO */

  obj = mime_new ( (MimeObjectClass *) &mimeMessageClass, (MimeHeaders *) NULL, MESSAGE_RFC822 );
  if ( !obj ) 
  {
    PR_FREEIF(mdd->url_name);
    PR_FREEIF(mdd->options->part_to_load);
    PR_FREEIF(mdd->options);
    PR_FREEIF(mdd );
    return nsnull;
  }
  
  obj->options = mdd->options;
  mdd->obj = obj;

  stream = PR_NEWZAP ( nsMIMESession );
  if ( !stream ) 
  {
    PR_FREEIF(mdd->url_name);
    PR_FREEIF( mdd->options->part_to_load );
    PR_FREEIF ( mdd->options );
    PR_FREEIF ( mdd );
    PR_FREEIF ( obj );
    return nsnull;
  }

  stream->name = "MIME To Draft Converter Stream";
  stream->complete = mime_parse_stream_complete;
  stream->abort = mime_parse_stream_abort;
  stream->put_block = mime_parse_stream_write;
  stream->data_object = mdd;

  status = obj->clazz->initialize ( obj );
  if ( status >= 0 )
    status = obj->clazz->parse_begin ( obj );
  if ( status < 0 ) 
  {
    PR_FREEIF(mdd->url_name);
    PR_FREEIF ( stream );
    PR_FREEIF( mdd->options->part_to_load );
    PR_FREEIF ( mdd->options );
    PR_FREEIF ( mdd );
    PR_FREEIF ( obj );
    return nsnull;
  }

  return stream;
}

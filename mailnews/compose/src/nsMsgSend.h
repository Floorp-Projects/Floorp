/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef __MSGSEND_H__
#define __MSGSEND_H__

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


//
// Necessary includes
//
#include "nsIMsgSend.h"

#include "msgCore.h"
#include "prprf.h" 
#include "nsFileStream.h"
#include "nsMsgMessageFlags.h"
#include "nsIURL.h"
#include "nsMsgAttachmentHandler.h"
#include "nsMsgCompFields.h"
#include "nsIMsgSendListener.h"
#include "nsIDOMNode.h"
#include "nsIEditorShell.h"
#include "nsIUrlListener.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIMsgStringService.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgHdr.h"
#include "nsIMsgIdentity.h"
#include "nsWeakReference.h"
#include "nsIDOMWindowInternal.h"

//
// Some necessary defines...
//
#define TEN_K                 10240
#define MIME_BUFFER_SIZE		  4096

//
// Utilities for string handling
//
#define PUSH_STRING(S) \
 do { PL_strcpy (buffer_tail, S); buffer_tail += PL_strlen (S); } while(0)
#define PUSH_NEWLINE() \
 do { *buffer_tail++ = nsCRT::CR; *buffer_tail++ = nsCRT::LF; *buffer_tail = '\0'; } while(0)

//
// Forward declarations...
//
class nsMsgSendPart;
class nsMsgCopy;
class nsMsgDeliveryListener;
class nsIPrompt;
class nsIInterfaceRequestor;

class nsMsgComposeAndSend : public nsIMsgSend
{
public:
  //
  // The exit method used when downloading attachments only.
  // This still may be useful because of the fact that it is an
  // internal callback only. This way, no thread boundry issues and
  // we can get away without all of the listener array code.
  //
  void (*m_attachments_done_callback) (
									   nsresult                 status,
									   const PRUnichar              *error_msg,
									   struct nsMsgAttachedFile *attachments);
  
  //
  // Define QueryInterface, AddRef and Release for this class 
  //
	NS_DECL_ISUPPORTS

  nsMsgComposeAndSend();
	virtual     ~nsMsgComposeAndSend();


  // Delivery and completion callback routines...
  NS_IMETHOD  DeliverMessage();
  NS_IMETHOD  DeliverFileAsMail();
  NS_IMETHOD  DeliverFileAsNews();
  void        DoDeliveryExitProcessing(nsIURI * aUrl, nsresult aExitCode, PRBool aCheckForMail);

  nsresult    DoFcc();
  nsresult    StartMessageCopyOperation(nsIFileSpec        *aFileSpec, 
                                        nsMsgDeliverMode   mode,
                                        char			   *dest_uri);

  void	      Clear();

  NS_METHOD   SendToMagicFolder (nsMsgDeliverMode flag);
  nsresult    QueueForLater();
  nsresult    SaveAsDraft();
  nsresult    SaveInSentFolder();
  nsresult    SaveAsTemplate();

  //
  // FCC operations...
  //
  nsresult    MimeDoFCC (nsFileSpec           *input_file,  
			                   nsMsgDeliverMode     mode,
			                   const char           *bcc_header,
			                   const char           *fcc_header,
			                   const char           *news_url);
  
  // Init() will allow for either message creation without delivery or full
  // message creation and send operations
  //
  nsresult    Init(
                   nsIMsgIdentity  *aUserIdentity,
			             nsMsgCompFields  *fields,
                   nsFileSpec       *sendFileSpec,
			             PRBool           digest_p,
			             PRBool           dont_deliver_p,
			             nsMsgDeliverMode mode,
                   nsIMsgDBHdr      *msgToReplace,
			             const char       *attachment1_type,
			             const char       *attachment1_body,
			             PRUint32         attachment1_body_length,
			             const nsMsgAttachmentData   *attachments,
			             const nsMsgAttachedFile     *preloaded_attachments,
                   const char       *password);

  //
  // Setup the composition fields
  //
  nsresult    InitCompositionFields(nsMsgCompFields *fields);
  int         SetMimeHeader(nsMsgCompFields::MsgHeaderID header, const char *value);
  NS_IMETHOD  GetBodyFromEditor();


  //
  // Attachment processing...
  //
  int         HackAttachments(const struct nsMsgAttachmentData *attachments,
					                    const struct nsMsgAttachedFile *preloaded_attachments);
  nsresult    CountCompFieldAttachments();
  nsresult    AddCompFieldLocalAttachments();
  nsresult    AddCompFieldRemoteAttachments(PRUint32  aStartLocation, PRInt32 *aMailboxCount, PRInt32 *aNewsCount);

  // Deal with multipart related data
  nsresult    ProcessMultipartRelated(PRInt32 *aMailboxCount, PRInt32 *aNewsCount); 
  PRUint32    GetMultipartRelatedCount(void);

  // Body processing
  nsresult    SnarfAndCopyBody(const char  *attachment1_body,
						                   PRUint32    attachment1_body_length,
                               const char  *attachment1_type);

  PRInt32     PreProcessPart(nsMsgAttachmentHandler  *ma,
                             nsMsgSendPart           *toppart); // The very top most container of the message
                                                                // For part processing

  NS_DECL_NSIMSGSEND
  nsresult    SetStatusMessage(const PRUnichar *aMsgString);     // Status message method
  
  //
  // All vars necessary for this implementation
  //
  nsMsgKey                  m_messageKey;        // jt -- Draft/Template support; newly created key
  nsCOMPtr<nsIMsgIdentity>  mUserIdentity;
  nsCOMPtr<nsMsgCompFields> mCompFields;         // All needed composition fields (header, etc...)
  nsFileSpec                *mTempFileSpec;      // our temporary file
  
  nsOutputFileStream        *mOutputFile;        // the actual output file stream
  PRUint32                  mMessageWarningSize; // Warn if a message is over this size!

  PRBool                    m_dont_deliver_p;    // If set, we just return the nsFileSpec of the file
							                                   // created, instead of actually delivering message.
  nsMsgDeliverMode          m_deliver_mode;      // nsMsgDeliverNow, nsMsgQueueForLater, nsMsgSaveAsDraft, 
                                                 // nsMsgSaveAsTemplate and nsMsgSendUnsent
  nsCOMPtr<nsIMsgDBHdr>     mMsgToReplace;       // If the mode is nsMsgSaveAsDraft, this is the message it will
                                                 // replace

  // These are needed for callbacks to the FE...
  nsCOMPtr<nsIDOMWindowInternal>  mParentWindow;
  nsCOMPtr<nsIMsgProgress>        mSendProgress;
  nsCOMPtr<nsIMsgSendListener>    mListener;

  nsCOMPtr<nsIRequest>      mRunningRequest;
  PRBool                    mSendMailAlso;
  nsIFileSpec               *mReturnFileSpec;     // a holder for file spec's to be returned to caller

  // File where we stored our HTML so that we could make the plaintext form.
  nsFileSpec                *mHTMLFileSpec;

  // Variable for storing the draft name;
  nsCString                  m_folderName;
  //
  // These variables are needed for message Copy operations!
  //
  nsIFileSpec               *mCopyFileSpec;
  nsIFileSpec               *mCopyFileSpec2;
  nsMsgCopy                 *mCopyObj;
  PRBool                    mNeedToPerformSecondFCC;

  // For MHTML message creation
  nsIEditorShell            *mEditor;

  //
  // The first attachment, if any (typed in by the user.)
  //
  char                    *m_attachment1_type;
  char                    *m_attachment1_encoding;
  MimeEncoderData         *m_attachment1_encoder_data;
  char                    *m_attachment1_body;
  PRUint32                m_attachment1_body_length;
  char                    *mOriginalHTMLBody;

  // The plaintext form of the first attachment, if needed.
  nsMsgAttachmentHandler  *m_plaintext;

	// The multipart/related save object for HTML text.
  nsMsgSendPart           *m_related_part;
  nsMsgSendPart           *m_related_body_part;

  //
  // Subsequent attachments, if any.
  //
  PRUint32                m_attachment_count;
  PRUint32                m_attachment_pending_count;
  nsMsgAttachmentHandler  *m_attachments;
  nsresult                m_status; // in case some attachments fail but not all 

  PRUint32                mPreloadedAttachmentCount;
  PRUint32                mRemoteAttachmentCount;
  PRUint32                mMultipartRelatedAttachmentCount; // the number of mpart related attachments

  PRUint32                mCompFieldLocalAttachments;     // the number of file:// attachments in the comp fields
  PRUint32                mCompFieldRemoteAttachments;    // the number of remote attachments in the comp fields

  //
  // attachment states and other info...
  //
  PRBool                  m_attachments_only_p;         // If set, then we don't construct a complete
								                                        // MIME message; instead, we just retrieve the
								                                        // attachments from the network, store them in
								                                        // tmp files, and return a list of
								                                        // nsMsgAttachedFile structs which describe them.

  PRBool                  m_pre_snarfed_attachments_p;	// If true, then the attachments were
										                                    // loaded by in the background and therefore 
                                                        // we shouldn't delete the tmp files (but should 
                                                        // leave that to the caller.)

  PRBool                  m_digest_p;                   // Whether to be multipart/digest instead of
								                                        // multipart/mixed. 

  PRBool                  m_be_synchronous_p;	          // If true, we will load one URL after another,
								                                        // rather than starting all URLs going at once
								                                        // and letting them load in parallel.  This is
								                                        // more efficient if (for example) all URLs are
								                                        // known to be coming from the same news server
								                                        // or mailbox: loading them in parallel would
								                                        // cause multiple connections to the news
								                                        // server to be opened, or would cause much seek()ing.

  PRBool                  mGUINotificationEnabled;      // Should we throw up the GUI alerts on errors?
  PRBool                  mLastErrorReported;           // Last error reported to the user.
  PRBool                  mAbortInProcess;              // Used by Abort to avoid reentrance.

  void                    *m_crypto_closure;

protected:
  nsCOMPtr<nsIMsgStringService> mComposeBundle;
  nsresult GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks);
private:
  // will set m_attachment1_body & m_attachment1_body_length;
  nsresult EnsureLineBreaks(const char *body, PRUint32 body_len);

  nsCOMPtr<nsIMsgSendReport>  mSendReport;
  nsCString                   mSmtpPassword;            // store the smtp Password use during a send
};

// 
// These C routines should only be used by the nsMsgSendPart class.
//
extern nsresult mime_write_message_body(nsIMsgSend *state, char *buf, PRInt32 size);
extern char   *mime_get_stream_write_buffer(void);
extern nsresult PR_CALLBACK mime_encoder_output_fn (const char *buf, PRInt32 size, void *closure);
extern PRBool UseQuotedPrintable(void);

#endif /*  __MSGSEND_H__ */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _MsgCompose_H_
#define _MsgCompose_H_


#include "msgCore.h"
#include "prprf.h" /* should be defined into msgCore.h? */
#include "net.h" /* should be defined into msgCore.h? */
#include "intl_csi.h"
#include "msgcom.h"

#include "MsgCompGlue.h"

//#include "nsMsgPtrArray.h"
#include "nsMsgHeaderMasks.h"
#include "nsMsgFolderFlags.h"

#include "nsIMsgCompFields.h"
#include "nsIMsgCompose.h"

/*JFD 
#include "msg.h"
#include "msgpane.h"
#include "xlate.h"
*/


/* The MSG_REPLY_TYPE shares the same space as MSG_CommandType, to avoid
   possible weird errors, but is restricted to the `composition' commands
   (MSG_ReplyToSender through MSG_ForwardMessage.)
 */
typedef MSG_CommandType MSG_REPLY_TYPE;


struct MSG_AttachedFile;
typedef struct PrintSetup_ PrintSetup;
typedef struct _XPDialogState XPDialogState;

HJ08142
class MSG_NewsHost;
class MSG_HTMLRecipients;

class nsMsgCompose : public nsIMsgCompose, public MSG_Pane {
public:

	nsMsgCompose();
	virtual ~nsMsgCompose();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

	/* this is just for testing purpose, must be removed before shipping */
	NS_IMETHOD Test() {printf("nsMsgCompose: Test Succesfull\n"); return NS_OK;}

#if 0 //JFD
	NS_IMETHOD CreateAndInit(/*MWContext* */PRInt32 a_context, /* MWContext* */PRInt32 old_context,
						/* MSG_Prefs* */PRInt32 prefs, const nsIMsgCompFields* initfields,
						/* MSG_Master* */PRInt32 master);

	// Or, if you prefer, construct using below constructor and be sure to
	// soon call the Initialize() method:

	NS_IMETHOD Create(/* MWContext* */PRInt32 a_context, /* MSG_Prefs* */PRInt32 prefs,
						/* MSG_Master* */PRInt32 master);
	NS_IMETHOD Initialize(/* MWContext* */PRInt32 old_context, const nsIMsgCompFields* initfields);

	NS_IMETHOD Dispose();

	virtual MSG_PaneType GetPaneType();

	virtual void NotifyPrefsChange(NotifyCode code);

	MSG_CommandType PreviousSaveCommand();

	virtual MsgERR	GetCommandStatus(MSG_CommandType command,
										 const nsMsgViewIndex* indices,
										 PRInt32 numindices,
										 PRBool *selectable_p,
										 MSG_COMMAND_CHECK_STATE *selected_p,
										 const char **display_string,
										 PRBool * plural_p);
	virtual MsgERR DoCommand(MSG_CommandType command,
							 nsMsgViewIndex* indices, PRInt32 numindices);

	const char* GetDefaultURL();
	virtual void SetDefaultURL(const char *defaultUrl = NULL, 
							   const char *htmlPart = NULL);


	int SetCallbacks(MSG_CompositionPaneCallbacks* callbacks, void* closure);

	MSG_CompositionFields* GetInitialFields();
	

	MSG_HEADER_SET GetInterestingHeaders();
	int SetAttachmentList(struct MSG_AttachmentData*);
	PRBool NoPendingAttachments() const;
	char* GetAttachmentString();
	PRBool ShouldAutoQuote();
	const char* GetCompHeader(MSG_HEADER_SET);
	int SetCompHeader(MSG_HEADER_SET, const char*);
	PRBool GetCompBoolHeader(MSG_BOOL_HEADER_SET);
	int SetCompBoolHeader(MSG_BOOL_HEADER_SET, PRBool);
	const char* GetCompBody();
	int SetCompBody(const char*);
	void ToggleCompositionHeader(PRUint32 header);
	PRBool ShowingAllCompositionHeaders();
	PRBool ShowingCompositionHeader(PRUint32 mask);
    PRBool GetHTMLMarkup(void);
    void SetHTMLMarkup(PRBool flag);
	MsgERR QuoteMessage(int (*func)(void* closure, const char* data),
						void* closure);
	int PastePlaintextQuotation(const char* str);
	const struct MSG_AttachmentData *GetAttachmentList();
	int DownloadAttachments();
	char* UpdateHeaderContents(MSG_HEADER_SET which_header, const char* value);
	const char* GetWindowTitle();
	void SetBodyEdited(PRBool value);
	void MailCompositionAllConnectionsComplete();
	void CheckExpansion(MSG_HEADER_SET header);
	PRBool DeliveryInProgress();

	int SendMessageNow();
    int QueueMessageForLater();
	int SaveMessage();
    int SaveMessageAsDraft();
	int SaveMessageAsTemplate();

	PRBool IsDuplicatePost();
	const char* GetCompositionMessageID();
	void ClearCompositionMessageID();
	HJ13591
	HJ86782
	HJ02278
	HJ95534

	int RemoveNoCertRecipients();

	PRBool SanityCheckNewsgroups (const char *newsgroups);
	int SanityCheck(int skippast);

	HJ42055

    /* draft */
    int SetPreloadedAttachments ( MWContext *context, 
								  struct MSG_AttachmentData *attachmentData,
								  struct MSG_AttachedFile *attachments,
								  int attachments_count );

	virtual void SetIMAPMessageUID (nsMsgKey key);

    int RetrieveStandardHeaders(MSG_HeaderEntry ** return_list);
    int SetHeaderEntries(MSG_HeaderEntry * in_list,int count);
    void ClearComposeHeaders();
    
	HJ37212
	HJ42256

	int SetHTMLAction(MSG_HTMLComposeAction action) {
		m_htmlaction = action;
		return 0;
	}
	MSG_HTMLComposeAction GetHTMLAction() {return m_htmlaction;}

  int PutUpRecipientsDialog(void *pWnd = NULL);

  int ResultsRecipients(PRBool cancelled, PRInt32* nohtml, PRInt32* htmlok);

  PRBool m_confirmed_uuencode_p; // Have we confirmed sending uuencoded data?

  // For qutoing plain text to html then convert back to plain text
  void SetLineWidth(int width) { m_lineWidth = width; }
  int GetLineWidth() { return m_lineWidth; }
  // #$@%&*

protected:
	static void QuoteHTMLDone_S(URL_Struct* url, 
								int status, MWContext* context);

	void InitializeHeaders(MWContext* old_context,
						   MSG_CompositionFields* fields);

	char* FigureBcc(PRBool newsBcc);
	const char* CheckForLosingFcc(const char* fcc);

	static void GetUrlDone_S(PrintSetup*);
	void GetUrlDone(PrintSetup*);

	static void DownloadAttachmentsDone_S(MWContext *context, 
										  void *fe_data,
										  int status,
										  const char *error_message,
										  struct MSG_AttachedFile *attachmnts);

	void DownloadAttachmentsDone(MWContext* context, int status,
								 const char* error_message,
								 struct MSG_AttachedFile *attachments);

    int DoneComposeMessage(MSG_Deliver_Mode deliver_mode);

	static void DeliveryDoneCB_s(MWContext *context, void *fe_data, int status,
								 const char *error_message);
	void DeliveryDoneCB(MWContext* context, int status,
						const char* error_message);

	int RemoveNoCertRecipientsFromList(MSG_HEADER_SET header);

	PRBool HasNoMarkup();
	MSG_HTMLComposeAction DetermineHTMLAction();
	int MungeThroughRecipients(PRBool* someNonHTML, PRBool* groupNonHTML);

	static PRBool AskDialogDone_s(XPDialogState *state, char **argv, int argc,
								  unsigned int button);
	PRBool AskDialogDone(XPDialogState *state, char **argv, int argc,
						 unsigned int button);
	static PRBool RecipientDialogDone_s(XPDialogState *state, char **argv,
										int argc, unsigned int button);
	PRBool RecipientDialogDone(XPDialogState *state, char **argv, int argc,
							   unsigned int button);

	int CreateVcardAttachment ();

	MSG_NewsHost *InferNewsHost (const char *groups);

	MSG_REPLY_TYPE m_replyType;		/* The kind of message composition in
									   progress (reply, forward, etc.) */

	PRBool m_markup;					/* Whether we should generate messages
										   whose first part is text/html rather
										   than text/plain. */

	MSG_AttachmentData *m_attachData;	/* null-terminated list of the URLs and
										   desired types currently scheduled
										   for attachment.  */
	MSG_AttachedFile *m_attachedFiles;	/* The attachments which have already
										   been downloaded, and some info about
										   them. */

	char *m_defaultUrl;			/* Default URL for attaching, etc. */

	MSG_CompositionFields* m_initfields; // What all the fields were,
										 // initially.
	MSG_CompositionFields* m_fields; // Current value of all the fields.

	char* m_messageId;			// Message-Id to use for composition.

	char* m_attachmentString;	// Storage for string to display in UI for
								// the list of attachments.
	char* m_quotedText;			// The results of quoting the original text.

	/* Stuff used while quoting a message. */
	PrintSetup* m_print;
	MWContext *m_textContext;
	MWContext *m_oldContext;
	char* m_quoteUrl;
	URL_Struct *m_dummyUrl;
	Net_GetUrlExitFunc *m_exitQuoting;
	int (*m_quotefunc)(void* closure, const char* data);
	void* m_quoteclosure;
	PRBool m_deliveryInProgress;    /* True while mail is being sent. */
	PRBool m_attachmentInProgress;  /* True while attachments being
										  saved. */
	int m_pendingAttachmentsCount;

	MSG_Deliver_Mode m_deliver_mode;  /* MSG_DelverNow, MSG_QueueForLater,
									   * MSG_SaveAs,
									   * MSG_SaveAsDraft, MSG_SaveAsTemplate
									   */

    HJ21695

	PRBool m_cited;
  
	PRBool m_duplicatePost;		/* Whether we seem to be trying for a
									   second time to post the same message.
									   (If this is true, then we know to ignore
									   435 errors from the newsserver.) */

	HJ92535

	MSG_HTMLComposeAction m_htmlaction;
	MSG_HTMLRecipients* m_htmlrecip;

	int m_status;
	// I'm sure this isn't what Terry had in mind... // ### dmb
	MSG_HEADER_SET m_visible_headers;

	MSG_NewsHost* m_host;		// Which newshost we're posting to.  This is
								// lazily evaluated, so a NULL does necessarily
								// mean we have no news host specified.

	PRBool m_closeAfterSave;

	PRBool m_haveQuoted;
	PRBool m_haveAttachedVcard;

	MSG_CompositionPaneCallbacks m_callbacks;
	void* m_callbackclosure;
	int m_lineWidth; // for quoting plain text to html then convert back
						 // to plain text
#endif 0 //JFD
};


#endif /* _MsgCompose_H_ */

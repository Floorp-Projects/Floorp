/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _MsgCPane_H_
#define _MsgCPane_H_

#include "rosetta.h"
#include "msg.h"
#include "msgpane.h"
#include "xlate.h"


/* The MSG_REPLY_TYPE shares the same space as MSG_CommandType, to avoid
   possible weird errors, but is restricted to the `composition' commands
   (MSG_ReplyToSender through MSG_ForwardMessage.)
 */
typedef MSG_CommandType MSG_REPLY_TYPE;


struct MSG_AttachedFile;
typedef struct PrintSetup_ PrintSetup;
typedef struct _XPDialogState XPDialogState;

HG82621
class MSG_NewsHost;
class MSG_HTMLRecipients;

class MSG_CompositionPane : public MSG_Pane {
public:

	MSG_CompositionPane(MWContext* context, MWContext* old_context,
						MSG_Prefs* prefs, MSG_CompositionFields* initfields,
						MSG_Master* master);

	// Or, if you prefer, construct using below constructor and be sure to
	// soon call the Initialize() method:

	MSG_CompositionPane(MWContext* context, MSG_Prefs* prefs,
						MSG_Master* master);
	int Initialize(MWContext* old_context, MSG_CompositionFields* initfields);

	virtual ~MSG_CompositionPane();

	virtual MSG_PaneType GetPaneType();

	virtual void NotifyPrefsChange(NotifyCode code);

	virtual MsgERR	GetCommandStatus(MSG_CommandType command,
										 const MSG_ViewIndex* indices,
										 int32 numindices,
										 XP_Bool *selectable_p,
										 MSG_COMMAND_CHECK_STATE *selected_p,
										 const char **display_string,
										 XP_Bool * plural_p);
	virtual MsgERR DoCommand(MSG_CommandType command,
							 MSG_ViewIndex* indices, int32 numindices);

	const char* GetDefaultURL();


	int SetCallbacks(MSG_CompositionPaneCallbacks* callbacks, void* closure);

	MSG_CompositionFields* GetInitialFields();
	

	MSG_HEADER_SET GetInterestingHeaders();
	int SetAttachmentList(struct MSG_AttachmentData*);
	char* GetAttachmentString();
	XP_Bool ShouldAutoQuote();
	const char* GetCompHeader(MSG_HEADER_SET);
	int SetCompHeader(MSG_HEADER_SET, const char*);
	XP_Bool GetCompBoolHeader(MSG_BOOL_HEADER_SET);
	int SetCompBoolHeader(MSG_BOOL_HEADER_SET, XP_Bool);
	const char* GetCompBody();
	int SetCompBody(const char*);
	void ToggleCompositionHeader(uint32 header);
	XP_Bool ShowingAllCompositionHeaders();
	XP_Bool ShowingCompositionHeader(uint32 mask);
    XP_Bool GetHTMLMarkup(void);
    void SetHTMLMarkup(XP_Bool flag);
	MsgERR QuoteMessage(int (*func)(void* closure, const char* data),
						void* closure);
	int PastePlaintextQuotation(const char* str);
	const struct MSG_AttachmentData *GetAttachmentList();
	int DownloadAttachments();
	char* UpdateHeaderContents(MSG_HEADER_SET which_header, const char* value);
	const char* GetWindowTitle();
	void SetBodyEdited(XP_Bool value);
	void MailCompositionAllConnectionsComplete();
	void CheckExpansion(MSG_HEADER_SET header);
	XP_Bool DeliveryInProgress();

	int SendMessageNow();
    int QueueMessageForLater();
    int SaveMessageAsDraft();
	int SaveMessageAsTemplate();

	XP_Bool IsDuplicatePost();
	const char* GetCompositionMessageID();
	void ClearCompositionMessageID();
	HG22960

	int SanityCheck(int skippast);

    /* draft */
    int SetPreloadedAttachments ( MWContext *context, 
								  struct MSG_AttachmentData *attachmentData,
								  struct MSG_AttachedFile *attachments,
								  int attachments_count );

	virtual void SetIMAPMessageUID (MessageKey key);

    int RetrieveStandardHeaders(MSG_HeaderEntry ** return_list);
    void SetHeaderEntries(MSG_HeaderEntry * in_list,int count);
    void ClearComposeHeaders();
    
    

	int SetHTMLAction(MSG_HTMLComposeAction action) {
		m_htmlaction = action;
		return 0;
	}
	MSG_HTMLComposeAction GetHTMLAction() {return m_htmlaction;}

  int PutUpRecipientsDialog(void *pWnd = NULL);

  int ResultsRecipients(XP_Bool cancelled, int32* nohtml, int32* htmlok);

  XP_Bool m_confirmed_uuencode_p; // Have we confirmed sending uuencoded data?

  // For qutoing plain text to html then convert back to plain text
  void SetLineWidth(int width) { m_lineWidth = width; }
  int GetLineWidth() { return m_lineWidth; }

protected:
	static void QuoteHTMLDone_S(URL_Struct* url, 
								int status, MWContext* context);

	void InitializeHeaders(MWContext* old_context,
						   MSG_CompositionFields* fields);

	char* FigureBcc(XP_Bool newsBcc);
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

	HG22860

	XP_Bool HasNoMarkup();
	MSG_HTMLComposeAction DetermineHTMLAction();
	int MungeThroughRecipients(XP_Bool* someNonHTML, XP_Bool* groupNonHTML);

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

	XP_Bool m_markup;					/* Whether we should generate messages
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
	char* m_quoteUrl;
	URL_Struct *m_dummyUrl;
	Net_GetUrlExitFunc *m_exitQuoting;
	int (*m_quotefunc)(void* closure, const char* data);
	void* m_quoteclosure;
	XP_Bool m_deliveryInProgress;    /* True while mail is being sent. */
	XP_Bool m_attachmentInProgress;  /* True while attachments being
										  saved. */
	int m_pendingAttachmentsCount;

	MSG_Deliver_Mode m_deliver_mode;  /* MSG_DelverNow, MSG_QueueForLater,
									   * MSG_SaveAsDraft, MSG_SaveAsTemplate
									   */

    

	XP_Bool m_cited;
  
	XP_Bool m_duplicatePost;		/* Whether we seem to be trying for a
									   second time to post the same message.
									   (If this is true, then we know to ignore
									   435 errors from the newsserver.) */
	HG92827

	MSG_HTMLComposeAction m_htmlaction;
	MSG_HTMLRecipients* m_htmlrecip;

	int m_status;
	// I'm sure this isn't what Terry had in mind... // ### dmb
	MSG_HEADER_SET m_visible_headers;

	MSG_NewsHost* m_host;		// Which newshost we're posting to.  This is
								// lazily evaluated, so a NULL does necessarily
								// mean we have no news host specified.

	XP_Bool m_closeAfterSave;

	XP_Bool m_haveQuoted;
	XP_Bool m_haveAttachedVcard;

	MSG_CompositionPaneCallbacks m_callbacks;
	void* m_callbackclosure;
	int m_lineWidth; // for quoting plain text to html then convert back
						 // to plain text
};


#endif /* _MsgCPane_H_ */

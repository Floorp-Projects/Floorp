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

#include "rosetta.h"
#include "msg.h"
#include "msgcom.h"
#include "error.h"
#include "client.h"
#include "xpgetstr.h"
#include "msgcpane.h"
#include "msgprefs.h"
#include "msgmast.h"
#include "msgcflds.h"
#include "shist.h"
#include "msgppane.h"
#include "mime.h"
#include "msgbg.h"
#include "prefapi.h"
#include "msgurlq.h"
#include "msgsend.h"
#include "pw_public.h"
#include HG99874

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_ID_NOT_IN_FOLDER;
	extern int MK_MSG_CANT_OPEN;
	extern int MK_MSG_INBOX_L10N_NAME;
	extern int MK_MSG_OUTBOX_L10N_NAME;
	extern int MK_MSG_OUTBOX_L10N_NAME_OLD;
	extern int MK_MSG_TRASH_L10N_NAME;
	extern int MK_MSG_DRAFTS_L10N_NAME;
	extern int MK_MSG_SENT_L10N_NAME;
	extern int MK_MSG_TEMPLATES_L10N_NAME;
	extern int XP_MSG_IMAP_ACL_FULL_RIGHTS;
	extern int XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_NAME;
	extern int XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_DESCRIPTION;
}

#ifndef IMAP4_PORT_SSL_DEFAULT 
#define IMAP4_PORT_SSL_DEFAULT 993 /* use a separate port for imap4 over ssl */
#endif

#if defined(XP_MAC) && defined (__MWERKS__)
#pragma require_prototypes off
#endif

extern "C"
int ConvertMsgErrToMKErr(uint32 err); // ### Need to get from a header file...


inline MSG_CompositionPane* CastCompositionPane(MSG_Pane* pane) {
  XP_ASSERT(pane && pane->GetPaneType() == MSG_COMPOSITIONPANE);
  return (MSG_CompositionPane*) pane;
}

extern "C" MSG_Pane* MSG_FindPane(MWContext* context, MSG_PaneType type) {
  return MSG_Pane::FindPane(context, type, FALSE);
}

extern "C" XP_Bool
MSG_RequiresMailWindow (const char *) { return FALSE; }
extern "C" XP_Bool
MSG_RequiresNewsWindow (const char *) { return FALSE; }

/* If this URL requires a particular kind of window, and this is not
   that kind of window, then we need to find or create one.
 */
XP_Bool msg_NewWindowRequired (MWContext *context, const char *url)
{
  if (!context) 
	  return TRUE;
  if (context->type == MWContextSearch || context->type == MWContextPrint || context->type == MWContextBiff) 
	  return FALSE;

  /* Search URLs always run in the pane they started in */
  if (!XP_STRNCASECMP(url, "search-libmsg:", 14))
	  return FALSE;

  // If we can figure out the content type, and there is no converter,
  // return FALSE so we'll run the save as url in our window instead
  // of creating an empty browser window.
  char *contentType = MimeGetURLContentType(context, url);
  if (contentType && !NET_HaveConverterForMimeType(contentType))
	  return FALSE;

  /* This is not a browser window, and one is required. */
  if (context->type != MWContextBrowser && context->type != MWContextPane && MSG_RequiresBrowserWindow(url))
	  return TRUE;

    return FALSE; /*!msgPane && threadPane; */	// if msgPane is NULL, but we have a thread pane, return FALSE because we have the wrong window.
}

extern "C" XP_Bool MSG_NewWindowRequiredForURL (MWContext *context, URL_Struct *urlStruct)
{
	if (urlStruct->open_new_window_specified)
		return urlStruct->open_new_window;

	if (context->type != MWContextBrowser && !strncasecomp (urlStruct->address, "about:", 6) && !urlStruct->internal_url
		&& strncasecomp(urlStruct->address, "about:editfilenew", 17))
		return TRUE;

	return msg_NewWindowRequired(context, urlStruct->address);
}

extern "C"
{
	void MSG_InitMsgLib(void) { } ;
}

extern "C" MSG_Master* MSG_InitializeMail(MSG_Prefs* prefs) 
{
	MSG_Master *master = prefs->GetMasterForBiff();
	if (master)
		return (master);		// already initialized
	
	MSG_InitMsgLib();			// make sure db code is initialized

	master = new MSG_Master(prefs);
	prefs->SetMasterForBiff(master);
	return master;
}


extern "C" MSG_Pane* MSG_CreateProgressPane (MWContext *context,
										 MSG_Master *master,
										 MSG_Pane *parentPane)
{
	return new MSG_ProgressPane(context, master, parentPane);
}

extern "C" MSG_Pane* MSG_CreateCompositionPane(MWContext* context,
											   MWContext* old_context,
											   MSG_Prefs* prefs,
											   MSG_CompositionFields* fields,
											   MSG_Master* master)
{
	return new MSG_CompositionPane(context, old_context, prefs, fields, master);
}

extern "C" int
MSG_SetCompositionPaneCallbacks(MSG_Pane* composepane,
								MSG_CompositionPaneCallbacks* callbacks,
								void* closure)
{
	return CastCompositionPane(composepane)->SetCallbacks(callbacks, closure);
}

extern "C" MSG_Pane* MSG_CreateCompositionPaneNoInit(MWContext* context,
													 MSG_Prefs* prefs,
													 MSG_Master* master)
{
	return new MSG_CompositionPane(context, prefs, master);
}


extern "C" int MSG_InitializeCompositionPane(MSG_Pane* comppane,
											 MWContext* old_context,
											 MSG_CompositionFields* fields)
{
	return CastCompositionPane(comppane)->Initialize(old_context, fields);
}


extern "C" void MSG_SetFEData(MSG_Pane* pane, void* data) {
  pane->SetFEData(data);
}

extern "C" void* MSG_GetFEData(MSG_Pane* pane) {
  return pane->GetFEData();
}

extern "C" MSG_PaneType MSG_GetPaneType(MSG_Pane* pane) {
  return pane->GetPaneType();
}

extern "C" MWContext* MSG_GetContext(MSG_Pane* pane) {
	if (pane)
		return pane->GetContext();
	else
		return NULL;
}

extern "C" MSG_Prefs* MSG_GetPrefs(MSG_Pane* pane) {
  return pane->GetPrefs();
}

extern "C" void MSG_WriteNewProfileAge() {
	PREF_SetIntPref("mailnews.profile_age", MSG_IMAP_CURRENT_START_FLAGS);
}

extern "C" MSG_Prefs* MSG_GetPrefsForMaster(MSG_Master* master) {
  return master->GetPrefs();
}

extern "C" int MSG_GetURL(MSG_Pane *pane, URL_Struct* url)
{
	XP_ASSERT(pane && url);
	if (pane && url)
	{
		url->msg_pane = pane;
		return msg_GetURL(pane->GetContext(), url, FALSE);
	}
	else
		return 0;
}

extern "C" void MSG_Command(MSG_Pane* pane, MSG_CommandType command,
							MSG_ViewIndex* indices, int32 numindices) {
	int status =
		ConvertMsgErrToMKErr(pane->DoCommand(command, indices, numindices));
	if (status < 0) {
		char* pString = XP_GetString(status);
		if (pString && strlen(pString))
			FE_Alert(pane->GetContext(), pString);
	}
}

extern "C" int MSG_CommandStatus(MSG_Pane* pane,
								 MSG_CommandType command,
								 MSG_ViewIndex* indices, int32 numindices,
								 XP_Bool *selectable_p,
								 MSG_COMMAND_CHECK_STATE *selected_p,
								 const char **display_string,
								 XP_Bool *plural_p) {
  return ConvertMsgErrToMKErr(pane->GetCommandStatus(command,
													 indices, numindices,
													 selectable_p,
													 selected_p,
													 display_string,
													 plural_p));
}

extern "C" int MSG_SetToggleStatus(MSG_Pane* pane, MSG_CommandType command,
								   MSG_ViewIndex* indices, int32 numindices,
								   MSG_COMMAND_CHECK_STATE value) {
  return ConvertMsgErrToMKErr(pane->SetToggleStatus(command,
													indices, numindices,
													value));
}


extern "C" MSG_COMMAND_CHECK_STATE MSG_GetToggleStatus(MSG_Pane* pane,
													   MSG_CommandType command,
													   MSG_ViewIndex* indices,
													   int32 numindices)
{
  return pane->GetToggleStatus(command, indices, numindices);
}


extern "C" void MSG_DestroyMaster (MSG_Master* master) {
	delete master;
}


extern "C" void MSG_DestroyPane(MSG_Pane* pane) {
  delete pane;
}


extern "C" void MSG_SetLineWidth(MSG_Pane* composepane, int width)
{
	CastCompositionPane(composepane)->SetLineWidth(width);
}

extern "C" int MSG_SetHTMLAction(MSG_Pane* composepane,
								 MSG_HTMLComposeAction action)
{
	return CastCompositionPane(composepane)->SetHTMLAction(action);
}

extern "C" MSG_HTMLComposeAction MSG_GetHTMLAction(MSG_Pane* composepane)
{
	return CastCompositionPane(composepane)->GetHTMLAction();
}

extern "C" int MSG_PutUpRecipientsDialog(MSG_Pane* composepane, void *pWnd)
{
	return CastCompositionPane(composepane)->PutUpRecipientsDialog(pWnd);
}

extern "C" int MSG_ResultsRecipients(MSG_Pane* composepane,
									 XP_Bool cancelled,
									 int32* nohtml,
									 int32* htmlok)
{
	return CastCompositionPane(composepane)->ResultsRecipients(cancelled,
															   nohtml,
															   htmlok);
}

extern "C" XP_Bool MSG_GetHTMLMarkup(MSG_Pane * composepane) {
    return CastCompositionPane(composepane)->GetHTMLMarkup();
}

extern "C" XP_Bool MSG_DeliveryInProgress(MSG_Pane * composepane) {
    if (!composepane || MSG_COMPOSITIONPANE != MSG_GetPaneType(composepane))
        return FALSE;
    return CastCompositionPane(composepane)->DeliveryInProgress();
}

extern "C" void MSG_SetHTMLMarkup(MSG_Pane * composepane, XP_Bool flag) {
    CastCompositionPane(composepane)->SetHTMLMarkup(flag);
}

extern "C" const char *MSG_GetMessageIdFromState(void *state)
{
	if (state)
	{
		MSG_SendMimeDeliveryState *deliveryState =
			(MSG_SendMimeDeliveryState *) state;
		return deliveryState->m_fields->GetMessageId();
	}
	return NULL;
}

extern "C" XP_Bool MSG_IsSaveDraftDeliveryState(void *state)
{
	if (state)
		return (((MSG_SendMimeDeliveryState *) state)->m_deliver_mode ==
				MSG_SaveAsDraft);
	return FALSE;
}

extern "C" int MSG_SetPreloadedAttachments ( MSG_Pane *composepane,
											 MWContext *context,
											 void *attachmentData,
											 void *attachments,
											 int attachments_count )
{
  return ConvertMsgErrToMKErr ( CastCompositionPane(
								  composepane)->SetPreloadedAttachments (
									context, 
									(MSG_AttachmentData  *) attachmentData,
									(MSG_AttachedFile *) attachments,
									attachments_count) );
}

extern "C" MSG_Prefs* MSG_CreatePrefs() {
  return new MSG_Prefs();
}

extern "C" void MSG_DestroyPrefs(MSG_Prefs* prefs) {
  delete prefs;
}

extern "C" XP_Bool
MSG_GetNoInlineAttachments(MSG_Prefs* prefs)
{
	return prefs->GetNoInlineAttachments();
}

extern "C" XP_Bool MSG_GetAutoQuoteReply(MSG_Prefs* prefs) {
  return prefs->GetAutoQuoteReply();
}

extern MSG_Pane*
MSG_GetParentPane(MSG_Pane* progresspane)
{
	return progresspane->GetParentPane();
}


#ifdef XP_UNIX
extern "C" MSG_Pane*
MSG_MailDocument (MWContext *old_context)
{
  // For backwards compatability.
  return MSG_MailDocumentURL(old_context,NULL);
}

extern "C" MSG_Pane*
MSG_MailDocumentURL (MWContext *old_context,const char *url)
{
	// Don't allow a compose window to be created if the user hasn't 
	// specified an email address
	const char *real_addr = FE_UsersMailAddress();
	if (MISC_ValidateReturnAddress(old_context, real_addr) < 0)
		return NULL;

	MSG_CompositionFields* fields = new MSG_CompositionFields();
	if (!fields) return NULL;	// Out of memory.

  /* If url is not specified, grab current history entry. */
  if (!url) {
	  History_entry *h =
		  (old_context ? SHIST_GetCurrent (&old_context->hist) : 0);
	  if (h && h->address && *h->address) {
		  url = h->address;
	  }
  }
#if 1 /* Do this if we want to attach the target of Mail Document by default */

	if (url) {
		fields->SetHeader(MSG_ATTACHMENTS_HEADER_MASK, url);
	}
#endif

	if (old_context && old_context->title) {
		fields->SetHeader(MSG_SUBJECT_HEADER_MASK, old_context->title);
	}

	if (url) {
		fields->SetBody(url);
	}

	XP_Bool prefBool = FALSE;
	PREF_GetBoolPref("mail.attach_vcard",&prefBool);
	fields->SetAttachVCard(prefBool);

	MSG_CompositionPane* comppane =	(MSG_CompositionPane*) 
	  FE_CreateCompositionPane(old_context, fields, NULL, MSG_DEFAULT);
	if (!comppane) {
		delete fields;
		return NULL;
	}
	
	HG42420
	XP_ASSERT(comppane->GetPaneType() == MSG_COMPOSITIONPANE);
	return comppane;
}

extern "C" MSG_Pane*
MSG_Mail (MWContext *old_context)
{
	MSG_CompositionFields* fields = new MSG_CompositionFields();
	if (!fields) return NULL;	// Out of memory.

	XP_Bool prefBool = FALSE;
	PREF_GetBoolPref("mail.attach_vcard",&prefBool);
	fields->SetAttachVCard(prefBool);

	MSG_CompositionPane* comppane =	(MSG_CompositionPane*) 
	  FE_CreateCompositionPane(old_context, fields, NULL, MSG_DEFAULT);
	if (!comppane) {
		delete fields;
		return NULL;
	}
	
	HG52965
	XP_ASSERT(comppane->GetPaneType() == MSG_COMPOSITIONPANE);
	return comppane;
}

extern "C" void
MSG_ResetUUEncode(MSG_Pane *pane)
{
	CastCompositionPane(pane)->m_confirmed_uuencode_p = FALSE;
}

extern "C" MSG_CompositionFields*
MSG_CreateCompositionFields (
					const char *from,
					const char *reply_to,
					const char *to,
					const char *cc,
					const char *bcc,
					const char *fcc,
					const char *newsgroups,
					const char *followup_to,
					const char *organization,
					const char *subject,
					const char *references,
					const char *other_random_headers,
					const char *priority,
					const char *attachment,
					const char *newspost_url
					)
{
	MSG_CompositionFields* fields = new MSG_CompositionFields();

	fields->SetFrom(from);
	fields->SetReplyTo(reply_to);
	fields->SetTo(to);
	fields->SetCc(cc);
	fields->SetBcc(bcc);
	fields->SetFcc(fcc);
	fields->SetNewsgroups(newsgroups);
	fields->SetFollowupTo(followup_to);
	fields->SetOrganization(organization);
	fields->SetSubject(subject);
	fields->SetReferences(references);
	fields->SetOtherRandomHeaders(other_random_headers);
	fields->SetAttachments(attachment);
	fields->SetNewspostUrl(newspost_url);
	fields->SetPriority(priority);
	return fields;
}

extern "C" void
MSG_DestroyCompositionFields(MSG_CompositionFields *fields)
{
	delete fields;
}

#endif //XP_UNIX

extern "C" void
MSG_SetCompFieldsReceiptType(MSG_CompositionFields *fields,
							 int32 type)
{
	fields->SetReturnReceiptType(type);
}

extern "C" int32
MSG_GetCompFieldsReceiptType(MSG_CompositionFields *fields)
{
	return fields->GetReturnReceiptType();
}

extern "C" int
MSG_SetCompFieldsBoolHeader(MSG_CompositionFields *fields,
							MSG_BOOL_HEADER_SET header,
							XP_Bool bValue)
{
	return fields->SetBoolHeader(header, bValue);
}

extern "C" XP_Bool
MSG_GetCompFieldsBoolHeader(MSG_CompositionFields *fields,
							MSG_BOOL_HEADER_SET header)
{
	return fields->GetBoolHeader(header);
}

extern "C" XP_Bool
MSG_GetForcePlainText(MSG_CompositionFields* fields)
{
	return fields->GetForcePlainText();
}

#ifdef XP_UNIX
extern "C" MSG_Pane*
MSG_ComposeMessage (MWContext *old_context,
					const char *from,
					const char *reply_to,
					const char *to,
					const char *cc,
					const char *bcc,
					const char *fcc,
					const char *newsgroups,
					const char *followup_to,
					const char *organization,
					const char *subject,
					const char *references,
					const char *other_random_headers,
					const char *priority,
					const char *attachment,
					const char *newspost_url,
					const char *body,
					XP_Bool force_plain_text,
					const char* html_part
					)
{
	// Don't allow a compose window to be created if the user hasn't 
	// specified an email address
	const char *real_addr = FE_UsersMailAddress();
	if (MISC_ValidateReturnAddress(old_context, real_addr) < 0)
		return NULL;
	

	MSG_CompositionFields* fields =
		MSG_CreateCompositionFields(from, reply_to, to, cc, bcc,
									fcc, newsgroups, followup_to,
									organization, subject, references,
									other_random_headers, priority, attachment,
									newspost_url);

	fields->SetForcePlainText(force_plain_text);
	fields->SetHTMLPart(html_part);
	fields->SetDefaultBody(body);

	XP_Bool prefBool = FALSE;
	PREF_GetBoolPref("mail.attach_vcard",&prefBool);
	fields->SetAttachVCard(prefBool);

	MSG_CompositionPane* comppane =	(MSG_CompositionPane*) 
	  FE_CreateCompositionPane(old_context, fields, NULL, MSG_DEFAULT);
	if (!comppane) {
		delete fields;
		return NULL;
	}
	
	XP_ASSERT(comppane->GetPaneType() == MSG_COMPOSITIONPANE);
	return comppane;
}

#endif //XP_UNIX
extern "C" XP_Bool
MSG_ShouldAutoQuote(MSG_Pane* comppane)
{
	return CastCompositionPane(comppane)->ShouldAutoQuote();
}

extern "C" const char*
MSG_GetCompHeader(MSG_Pane* comppane, MSG_HEADER_SET header)
{
	return CastCompositionPane(comppane)->GetCompHeader(header);
}


extern "C" int
MSG_SetCompHeader(MSG_Pane* comppane, MSG_HEADER_SET header, const char* value)
{
	return CastCompositionPane(comppane)->SetCompHeader(header, value);
}


extern "C" XP_Bool
MSG_GetCompBoolHeader(MSG_Pane* comppane, MSG_BOOL_HEADER_SET header)
{
	return CastCompositionPane(comppane)->GetCompBoolHeader(header);
}


extern "C" int
MSG_SetCompBoolHeader(MSG_Pane* comppane, MSG_BOOL_HEADER_SET header, XP_Bool bValue)
{
	return CastCompositionPane(comppane)->SetCompBoolHeader(header, bValue);
}


extern "C" const char*
MSG_GetCompBody(MSG_Pane* comppane)
{
	return CastCompositionPane(comppane)->GetCompBody();
}


extern "C" int
MSG_SetCompBody(MSG_Pane* comppane, const char* value)
{
	return CastCompositionPane(comppane)->SetCompBody(value);
}



extern "C" void
MSG_QuoteMessage(MSG_Pane* comppane,
				 int (*func)(void* closure, const char* data),
				 void* closure)
{
	CastCompositionPane(comppane)->QuoteMessage(func, closure);
}


extern "C" int
MSG_SanityCheck(MSG_Pane* comppane, int skippast)
{
	return CastCompositionPane(comppane)->SanityCheck(skippast);
}




extern "C" const char*
MSG_GetAssociatedURL(MSG_Pane* comppane)
{
  return CastCompositionPane(comppane)->GetDefaultURL();
}


extern "C" void
MSG_MailCompositionAllConnectionsComplete(MSG_Pane* comppane)
{
  CastCompositionPane(comppane)->MailCompositionAllConnectionsComplete();
}


extern "C" int
MSG_PastePlaintextQuotation(MSG_Pane* comppane, const char* string)
{
	return CastCompositionPane(comppane)->PastePlaintextQuotation(string);
}

extern "C" char* 
MSG_UpdateHeaderContents(MSG_Pane* comppane,
						 MSG_HEADER_SET header,
						 const char* value)
{
  return CastCompositionPane(comppane)->UpdateHeaderContents(header, value);
}



extern "C" int 
MSG_SetAttachmentList(MSG_Pane* comppane,
					  struct MSG_AttachmentData* list)
{
  return CastCompositionPane(comppane)->SetAttachmentList(list);
}


extern "C" const struct MSG_AttachmentData*
MSG_GetAttachmentList(MSG_Pane* comppane)
{
  return CastCompositionPane(comppane)->GetAttachmentList();
}


extern "C" MSG_HEADER_SET MSG_GetInterestingHeaders(MSG_Pane* comppane)
{
	return CastCompositionPane(comppane)->GetInterestingHeaders();
}








extern "C" XP_Bool MSG_IsDuplicatePost(MSG_Pane* comppane) {
	// if we're sending from the outbox, we don't have a compostion pane, so guess NO.
	return (comppane->GetPaneType() == MSG_COMPOSITIONPANE) 
		? CastCompositionPane(comppane)->IsDuplicatePost()
		: FALSE;
}


extern "C" void
MSG_ClearCompositionMessageID(MSG_Pane* comppane)
{
	if (comppane->GetPaneType() == MSG_COMPOSITIONPANE)
		CastCompositionPane(comppane)->ClearCompositionMessageID();
}


extern "C" const char*
MSG_GetCompositionMessageID(MSG_Pane* comppane)
{
	return (comppane->GetPaneType() == MSG_COMPOSITIONPANE)
		? CastCompositionPane(comppane)->GetCompositionMessageID()
		: 0;
}

/* What are we going to do about error messages and codes? Internally, we'd
 like a nice error range system, but we need to export errors too... ###
 */
extern "C"
int ConvertMsgErrToMKErr(MsgERR err)
{
	switch (err)
	{
	case eSUCCESS:
		return 0;
	case eOUT_OF_MEMORY:
		return MK_OUT_OF_MEMORY;
	case eID_NOT_FOUND:
		return MK_MSG_ID_NOT_IN_FOLDER;
	case eUNKNOWN:
		return -1;
	}
	// Well, most likely, someone returned a negative number that
	// got cast somewhere into a MsgERR.  If so, just return that value.
	if (int(err) < 0) return int(err);
	// Punt, and return the generic unknown error.
	return -1;
}

extern "C" 
int MSG_RetrieveStandardHeaders(MSG_Pane * pane, MSG_HeaderEntry ** return_list)
{
  XP_ASSERT(pane && pane->GetPaneType() == MSG_COMPOSITIONPANE);
  return CastCompositionPane(pane)->RetrieveStandardHeaders(return_list);
}

extern "C"
void MSG_SetHeaderEntries(MSG_Pane * pane,MSG_HeaderEntry * in_list,int count)
{
  XP_ASSERT(pane && pane->GetPaneType() == MSG_COMPOSITIONPANE);
  CastCompositionPane(pane)->SetHeaderEntries(in_list,count);
}

extern "C"
void MSG_ClearComposeHeaders(MSG_Pane * pane)
{
  XP_ASSERT(pane && pane->GetPaneType() == MSG_COMPOSITIONPANE);
  CastCompositionPane(pane)->ClearComposeHeaders();
}

extern "C"
int MSG_ProcessBackground(URL_Struct* urlstruct)
{
	return msg_Background::ProcessBackground(urlstruct);
}

extern "C" XP_Bool MSG_RequestForReturnReceipt(MSG_Pane *pane)
{
	return pane->GetRequestForReturnReceipt();
}

extern "C" XP_Bool MSG_SendingMDNInProgress(MSG_Pane *pane)
{
	return pane->GetSendingMDNInProgress();
}

//
// NET_IsOffline() is declared in net.h, but libnet doesn't define it
// publically anywhere.  There's one in network/protocols/nntp/mknews.c
// but it's declared MODULE_PRIVATE and we can't see it from here,
// at least not in MOZ_MAIL_COMPOSE mode.
//
#ifndef MOZ_MAIL_NEWS
XP_Bool NET_IsOffline() { return FALSE; }
#endif /* MOZ_MAIL_NEWS */

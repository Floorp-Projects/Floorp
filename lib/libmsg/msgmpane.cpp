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
#include "errcode.h"
#include "xp_str.h"
#include "xpgetstr.h"

#include "msgmpane.h"
#include "msgtpane.h"
#include "msgfinfo.h"
#include "msgdb.h"
#include "msgdbvw.h"
#include "msgmast.h"
#include "msgprefs.h"
#include "msgsec.h"
#include "mailhdr.h"

#include "msgundmg.h"
#include "msgundac.h"
#include "mime.h"

#include "addrbook.h"

#include "libi18n.h"
#include "newshost.h"
#include "prefapi.h"
#include "maildb.h"
#include "prsembst.h"
#include "listngst.h"
#include "newsdb.h"
#include "intl_csi.h"
#include "msgimap.h"
#include "msgurlq.h"
#include "msgmdn.h"
#include "msgstrob.h"

extern "C" 
{
	extern int MK_MSG_DELETE_MESSAGE;
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_MARK_AS_READ;
	extern int MK_MSG_MARK_AS_UNREAD;
	extern int MK_MSG_NEW_NEWS_MESSAGE;
	extern int MK_MSG_POST_REPLY;
	extern int MK_MSG_POST_MAIL_REPLY;
	extern int MK_MSG_NEW_MAIL_MESSAGE;
	extern int MK_MSG_REPLY;
	extern int MK_MSG_REPLY_TO_SENDER;
	extern int MK_MSG_REPLY_TO_ALL;
	extern int MK_MSG_FORWARD_QUOTED;
	extern int MK_MSG_FORWARD;
	extern int MK_MSG_PARTIAL_MSG_FMT_1;
	extern int MK_MSG_PARTIAL_MSG_FMT_2;
	extern int MK_MSG_PARTIAL_MSG_FMT_3;
	extern int MK_MSG_ID_NOT_IN_FOLDER;
	extern int MK_MSG_FOLDER_UNREADABLE;
	extern int MK_MSG_OPEN_DRAFT;
	extern int MK_MSG_ADD_SENDER_TO_ADDRESS_BOOK;
	extern int MK_MSG_ADD_ALL_TO_ADDRESS_BOOK;
	extern int MK_MSG_CANT_OPEN;
}


#ifdef DEBUG_terry
static void
StupidAttachmentCount(MSG_Pane* pane, void*, int32 num, XP_Bool finished)
{
	XP_Trace("@@@ %d attachments (%s)\n", num, finished ? "Done" : "Not done");
	if (finished && num > 0) {
		MSG_AttachmentData* data;
		finished = FALSE;
		MSG_GetViewedAttachments(pane, &data, &finished);
		XP_ASSERT(finished);
		MSG_AttachmentData* tmp;
		for (tmp = data ; tmp->url ; tmp++) {
			XP_Trace("@@@     %s\n", tmp->url);
			XP_Trace("@@@         %s\n", tmp->real_type);
			XP_Trace("@@@         %s\n", tmp->real_name);
			XP_Trace("@@@         %s\n", tmp->description);
		}
		MSG_FreeAttachmentList(pane, data);
	}
}
#endif


MSG_MessagePane::MSG_MessagePane(MWContext* context, MSG_Master* master)
: m_PaneListener(this), MSG_Pane(context, master) 
{
	m_key = MSG_MESSAGEKEYNONE;
	m_postDeleteLoadKey = MSG_MESSAGEKEYNONE;

#ifdef DEBUG_terry

	static MSG_MessagePaneCallbacks call;
	call.AttachmentCount = StupidAttachmentCount;
	SetMessagePaneCallbacks(&call, NULL);

#endif

}


MSG_MessagePane::~MSG_MessagePane () 
{
	ClearDerivedStrings();
	InterruptContext(FALSE);	
	if (m_view) {
		m_view->Remove(&m_PaneListener);
		m_view->Close(); // drop reference count
		m_view = NULL;
	}
}


MSG_PaneType MSG_MessagePane::GetPaneType() 
{
	return MSG_MESSAGEPANE;
}


int MSG_MessagePane::SetMessagePaneCallbacks(MSG_MessagePaneCallbacks* c,
											 void* closure)
{
	m_callbacks = c;
	m_callbackclosure = closure;
	return 0;
}


MSG_MessagePaneCallbacks*
MSG_MessagePane::GetMessagePaneCallbacks(void** closure)
{
	if (closure) *closure = m_callbackclosure;
	return m_callbacks;
}



void MSG_MessagePane::NotifyPrefsChange(NotifyCode code) 
{
	if (m_folder)
	{
		if (((code == MailServerType)||(code == PopHost)) && 
            (GetFolder()->GetType() == FOLDER_IMAPMAIL))
        {
			FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderDeleted,
                           (uint32)GetFolder());
        }
		else if (code != Directory)
		{
			// We don't want to reload the message on a directory change 
			// because a directory notification can result from a prefs 
			// reload, which can in turn result from loading a message. 
			// (Holy recursion, Batman!)
			LoadMessage(m_folder, m_key, NULL, TRUE);
		}
		else // code == Directory
		{
			// If the main mail directory changes, do we not close ourselves?
		}
	}
}

MSG_FolderInfo *MSG_MessagePane::GetFolder() 
{
	return m_folder;
}

void MSG_MessagePane::SetFolder(MSG_FolderInfo *info)
{
	m_folder = info;
}

PaneListener *MSG_MessagePane::GetListener() 
{
	return &m_PaneListener;
}

MsgERR MSG_MessagePane::InitializeFolderAndView (const char *folderName)
{
	MsgERR status = 0;
	MSG_FolderInfo	*folder = GetMaster()->FindMailFolder (folderName, FALSE);

	if (m_folder != folder)	// don't reinitialize folder and view unless it has changed
	{
		if (folder)			// sometimes, folderName is NULL, so folder will be NULL, so re-use m_folder
			m_folder = folder;
		CloseView();
		char* url = m_folder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
		status = MessageDBView::OpenURL(url, GetMaster(),
											   ViewAny,
											   &m_view, TRUE /* open in foreground */);

		if (status == eSUCCESS && m_view != NULL)
			m_view->Add(&m_PaneListener); // add listener back to view
		XP_FREE(url);
	}
	
	return status;
}

void MSG_MessagePane::StartingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num)
{
	if (m_numstack == 0)
		m_PaneListener.StartKeysChanging();

	MSG_Pane::StartingUpdate(code, where, num);
}

void MSG_MessagePane::EndingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num)
{
	MSG_Pane::EndingUpdate(code, where, num);
	if (m_numstack == 0)
		m_PaneListener.EndKeysChanging();
}


void MSG_MessagePane::SwitchView(MessageDBView *newView)
{
	m_view->Remove(&m_PaneListener);
	m_view->Close();
	m_view = newView;
	m_view->AddReference();
	m_view->Add(&m_PaneListener); // add listener back to view
	newView->EnsureListed(m_key);
}

// allow_content_change indicates whether we should maintain the
// integrity of the original message from the server.  If set to TRUE,
// that means it is safe for us to fiddle with the content.  For instance,
// we are allowed to download it by IMAP MIME parts and assemble a somewhat
// empty shell.
MsgERR
MSG_MessagePane::LoadMessage(MSG_FolderInfo* folder, 
							MessageKey key,
							Net_GetUrlExitFunc *loadUrlExitFunction,
							XP_Bool allow_content_change) 
{
	XP_Bool clearMessageArea = (m_key != key);

	if (m_key != key)
		MSG_SetBiffStateAndUpdateFE(MSG_BIFF_NoMail);

#ifdef XP_MAC
	{
	// Ask FE if it's OK to load another message (the window may be closing).
	enum { MSG_PaneNotifyOKToLoadNewMessage = 999 }; // Can we make this official?
	XP_Bool ok = TRUE;
	FE_PaneChanged(this, FALSE, (MSG_PANE_CHANGED_NOTIFY_CODE)MSG_PaneNotifyOKToLoadNewMessage, (int32)&ok);
	if (!ok)
		return eSUCCESS;
	}
#endif

	if (clearMessageArea && folder && folder->GetType() == FOLDER_IMAPMAIL) 
	{
		MSG_IMAPFolderInfoMail *imapFolder =  folder->GetIMAPFolderInfoMail();
		MSG_UrlQueue *queue = MSG_UrlQueue::FindQueueWithSameContext (this);
		if (queue != NULL)
			clearMessageArea = FALSE;	
		// pseudo interrupt any message load, if we're fetching by chunks
		XP_Bool	fetchByChunks = TRUE;
		PREF_GetBoolPref("mail.imap.fetch_by_chunks", &fetchByChunks);
		if (fetchByChunks && imapFolder &&  imapFolder->GetRunningIMAPUrl())
		{
			IMAP_PseudoInterruptFetch(GetContext());
			clearMessageArea = FALSE;	
		}
	}

	// Clear message area, but only if we're loading
	// a new message. This is all very unfortunate. Clearing message
	// area causes libparse to interrupt the mail context, which happens
	// to be reparsing a folder in some cases. This is because WinFE
	// clears the message area if the view is empty (even if it was
	// empty to start with) as a side effect of trying to select something.
	if (clearMessageArea)
	{
#ifndef XP_MAC
		// macfe has its own clearing routine (CMessageView::ClearMessageArea)
		msg_ClearMessageArea(GetContext());
#endif
		m_key = key;
	}
	if (key == MSG_MESSAGEKEYNONE)
		return eSUCCESS;

	// don't reinitialize folder and view unless it has changed, or the view or DB seem invalid
	if (m_folder != folder || !m_view || !m_view->GetDB())	
	{
		m_folder = folder;
		MsgERR status = InitializeFolderAndView (NULL);
		if (status != eSUCCESS) 
			return status;
	}

	FE_PaneChanged(this, FALSE, MSG_PaneNotifyMessageLoaded, key);

	if (m_callbacks && m_callbacks->AttachmentCount) {
		m_callbacks->AttachmentCount(this, m_callbackclosure, 0, FALSE);
	}

	XP_ASSERT(m_view);
	char *url = m_folder->BuildUrl(m_view->GetDB(), key);
	if (NULL != url)
	{
		Net_GetUrlExitFunc *preExitFunc;

		m_doneLoadingFunc = NULL;
		m_doneLoading = FALSE;
		if (m_justmakecompose) 
		{
			preExitFunc = MSG_MessagePane::FinishComposeFor;
		}
		else
		{
			preExitFunc = MSG_MessagePane::FinishedLoading;
			m_doneLoadingFunc = loadUrlExitFunction;
		}

		
		if (folder->GetType() == FOLDER_IMAPMAIL)
		{
			MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) folder;
			imapFolder->SetDownloadState(MSG_IMAPFolderInfoMail::kNoDownloadInProgress);
			GetContext()->currentIMAPfolder = imapFolder;
		}

		if (m_callbacks && m_callbacks->AttachmentCount) {
			m_callbacks->AttachmentCount(this, m_callbackclosure, 0, FALSE);
		}
		
		// if an imap folder is being loaded in this context, then this
		// is the result of the user clicking on a message.  We interrupt
		// and load
		XP_Bool shouldNotInterrupt = TRUE;
		if (folder->GetType() == FOLDER_IMAPMAIL) 
		{
			XP_Bool	urlIsInMemoryCache = FALSE;
			MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
			if (imapFolder->GetFolderLoadingContext() == GetContext())
				shouldNotInterrupt = FALSE;

			URL_Struct *url_s = NET_CreateURLStruct (url, NET_IsOffline() ? NET_SUPER_RELOAD : NET_DONT_RELOAD);
			char extraFlag = 0;	// figure out extra flag before we start loading offline messages, because that marks it read.

			MSG_UrlQueue *q = NULL;
			if (url_s)
			{
				if (!NET_IsOffline())
				{
					MSG_ViewIndex viewIndex = m_view->FindKey(key, FALSE);
					if (viewIndex != MSG_VIEWINDEXNONE)
					{
						m_view->GetExtraFlag(viewIndex, &extraFlag);
					}
				}
				url_s->allow_content_change = allow_content_change;
				m_key = key;
				q = MSG_UrlQueue::AddUrlToPane(url_s, preExitFunc, this, FALSE, NET_IsOffline() ? NET_SUPER_RELOAD : NET_DONT_RELOAD);
				urlIsInMemoryCache = NET_IsURLInMemCache(url_s);
			}
			else
				return eUNKNOWN;

			XP_FREE(url);

			// if this message was previously unread, and the message is offline or 
			// in memory cache, make sure the server knows it's read

			if (!NET_IsOffline())
			{
				if (!(extraFlag & kIsRead))
				{
					if (urlIsInMemoryCache || (extraFlag & kOffline))
					{
						IDArray	keysToFlag;

						keysToFlag.Add(m_key);
						imapFolder->StoreImapFlags(this, kImapMsgSeenFlag, TRUE, keysToFlag, q);
					}
				}
			}
			
			return eSUCCESS;
		}
		URL_Struct* ustruct = NET_CreateURLStruct(url, NET_IsOffline() ? NET_SUPER_RELOAD : NET_DONT_RELOAD);
		ustruct->pre_exit_fn = preExitFunc;
		GetURL(ustruct, TRUE);	// TRUE will quietly work if any existing 
												// url is 'almost done'.

		// clear any imap copy info so download will work

		XP_FREE (url);

		return eSUCCESS;
	}
	else
		return eUNKNOWN; //PHP need better error code
}


void MSG_MessagePane::FinishedLoading(URL_Struct* ustruct, int status,
									  MWContext* context)
{
	MSG_MessagePane* pane = (MSG_MessagePane*) ustruct->msg_pane;
	XP_ASSERT(pane && pane->GetPaneType() == MSG_MESSAGEPANE);
	if (!pane || pane->GetPaneType() != MSG_MESSAGEPANE) return;
	if (pane->m_doneLoadingFunc) {
		(*pane->m_doneLoadingFunc)(ustruct, status, context);
		pane->m_doneLoadingFunc = NULL;
	}
	pane->m_doneLoading = TRUE;
	if (pane->m_callbacks && pane->m_callbacks->AttachmentCount) {
		pane->m_callbacks->AttachmentCount(pane, pane->m_callbackclosure,
										   MimeGetAttachmentCount(context),
										   TRUE);
		if (context && MIME_HasAttachments(context))
			(pane->m_view->GetDB())->MarkHasAttachments(pane->m_key, TRUE, (ChangeListener*) pane /* m_view ? */);
	}
}



void MSG_MessagePane::DestroyPane(URL_Struct* ustruct, int /*status*/,
								  MWContext* /*context*/)
{
	MSG_MessagePane* pane = (MSG_MessagePane*) ustruct->msg_pane;
	XP_ASSERT(pane->GetPaneType() == MSG_MESSAGEPANE);
	if (pane->GetPaneType() == MSG_MESSAGEPANE) {
		delete pane;
	}
}


void MSG_MessagePane::FinishComposeFor(URL_Struct* ustruct, int /*status*/,
									   MWContext* /*context*/)
{
	MSG_MessagePane* pane = (MSG_MessagePane*) ustruct->msg_pane;
	XP_ASSERT(pane->GetPaneType() == MSG_MESSAGEPANE);
	if (pane->GetPaneType() == MSG_MESSAGEPANE) {
		pane->UnregisterFromPaneList();	// Make sure that netlib won't decide
										// to attach this pane to a URL,
										// because this pane is about to go
										// away.
		if (pane->ComposeMessage(pane->m_composetype,
								 MSG_MessagePane::DestroyPane) != eSUCCESS) {
			delete pane;
		}
	}
}


MsgERR
MSG_MessagePane::MakeComposeFor(MSG_FolderInfo* folder, MessageKey key,
								MSG_CommandType type)
{
	m_justmakecompose = TRUE;
	m_composetype = type;
	XP_Bool allow_content_change = 
		((type == MSG_ReplyToSender) ||
		(type == MSG_ReplyToAll) ||
		(type == MSG_ForwardMessageQuoted));
	MsgERR status = LoadMessage(folder, key, NULL, allow_content_change);	// forwarding requires message integrity
	if (status != eSUCCESS) delete this;
	return status;
}



void
MSG_MessagePane::ClearDerivedStrings()
{
  FREEIF(m_mailToSenderUrl);
  FREEIF(m_mailToAllUrl);
  FREEIF(m_postReplyUrl);
  FREEIF(m_postAndMailUrl);
  FREEIF(m_displayedMessageSubject);
  FREEIF(m_displayedMessageId);
}


static char*
msg_get_1522_header(MWContext *context, MimeHeaders *headers,
                                        const char *name, XP_Bool all_p)
{
	char *contents = MimeHeaders_get(headers, name, FALSE, all_p);
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	if (contents && *contents) {
		char *converted = IntlDecodeMimePartIIStr(contents, INTL_GetCSIWinCSID(c),
												  FALSE);
		if (converted) {
			XP_FREE(contents);
			contents = converted;
		}
	}
	return contents;
}




void 
MSG_MessagePane::ActivateReplyOptions(MimeHeaders* headers)
{
	XP_Bool mdnNeeded = FALSE, isRead = FALSE;
	
	// Since this gets called when the text appears in the pane, this is a
	// great time to mark the message as read.

	if (!m_justmakecompose) {
		XP_Bool bMarkMessageRead = TRUE;
		char *xref = MimeHeaders_get(headers, HEADER_XREF, FALSE, FALSE);
		char *messageID = MimeHeaders_get(headers, HEADER_MESSAGE_ID, FALSE, FALSE);
		if (messageID && m_view && m_view->GetDB())
		{
			MessageDB *db = m_view->GetDB();
			// check if this is the same message-id as we think we should have for the
			// current key.
			DBMessageHdr *curHdr = db->GetDBHdrForKey(m_key);
			db->IsMDNNeeded(m_key, &mdnNeeded);
			db->IsRead(m_key, &isRead);

			if (curHdr)
			{
				XPStringObj curKeyMessageID;
				curHdr->GetMessageId(curKeyMessageID, db->GetDB());
				char	*strippedMsgID = (char *) XP_ALLOC(XP_STRLEN(messageID));
				if (strippedMsgID)
				{
					MessageHdrStruct::StripMessageId(messageID, strippedMsgID, XP_STRLEN(messageID));
					if (XP_STRCMP(strippedMsgID, curKeyMessageID))
					{
						char *newsgroups = MimeHeaders_get(headers, HEADER_NEWSGROUPS, FALSE, FALSE);
						m_key = db->GetMessageKeyForID(messageID);
						FE_PaneChanged(this, FALSE, MSG_PaneNotifyMessageLoaded, m_key);
						// ### dmb should check if we've changed newsgroups!
						FREEIF(newsgroups);

					}
				}
				FREEIF(strippedMsgID);
				delete curHdr;
			}
			if (m_folder && m_key == m_folder->GetLastMessageLoaded())
			{
				m_folder->SetLastMessageLoaded(MSG_MESSAGEKEYNONE);
				bMarkMessageRead = FALSE;
			}
		}
		if (bMarkMessageRead)
			MarkMessageKeyRead(m_key, xref);
		FREEIF(xref);
		FREEIF(messageID);
	}


	char *host = 0;
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
	char *grps = 0;
	char *foll = 0;

	HG02087

	ClearDerivedStrings();

	if (headers) {

		from = msg_get_1522_header(m_context, headers, HEADER_FROM,     FALSE);
		repl = msg_get_1522_header(m_context, headers, HEADER_REPLY_TO, FALSE);
		subj = msg_get_1522_header(m_context, headers, HEADER_SUBJECT,  FALSE);
		to   = msg_get_1522_header(m_context, headers, HEADER_TO,       TRUE);
		cc   = msg_get_1522_header(m_context, headers, HEADER_CC,       TRUE);

		/* These headers should not be RFC-1522-decoded. */
		grps = MimeHeaders_get(headers, HEADER_NEWSGROUPS,  FALSE, TRUE);
		foll = MimeHeaders_get(headers, HEADER_FOLLOWUP_TO, FALSE, TRUE);
		id   = MimeHeaders_get(headers, HEADER_MESSAGE_ID,  FALSE, FALSE);
		refs = MimeHeaders_get(headers, HEADER_REFERENCES,  FALSE, TRUE);

		if (repl) {
			FREEIF(from);
			from = repl;
			repl = 0;
		}

		if (foll) {
			FREEIF(grps);
			grps = foll;
			foll = 0;
		}


	  HG82126
	}

	if (m_folder && m_folder->IsNews()) {
		/* Decide whether cancelling this message should be allowed. */
		m_cancellationAllowed = FALSE;
		if (from) {
			char *us = MSG_MakeFullAddress (NULL, FE_UsersMailAddress ());
			char *them = MSG_ExtractRFC822AddressMailboxes(from);
			m_cancellationAllowed = (us && them && !strcasecomp (us, them));
			FREEIF(us);
			FREEIF(them);
		}
	}

	if (!headers) return;

	if (subj) {
		m_displayedMessageSubject = XP_STRDUP(subj);
	}
	if (id) {
		m_displayedMessageId = XP_STRDUP(id);
	}

	host = ComputeNewshostArg();

	if (to || cc) {
		to_and_cc = (char *)
			XP_ALLOC((to ? XP_STRLEN (to) : 0) +
					 (cc ? XP_STRLEN (cc) : 0) +
					 5);
		if (!to_and_cc) goto FAIL;
		*to_and_cc = 0;
		if (to) XP_STRCPY(to_and_cc, to);
		if (to && *to && cc && *cc) {
			XP_STRCAT(to_and_cc, ", ");
		}
		if (cc) XP_STRCAT(to_and_cc, cc);
	}

	if (id || refs) {
		int maxlen =
			(id ? XP_STRLEN(id) : 0) +
			(refs ? XP_STRLEN (refs) : 0) +
			5;
		new_refs = (char *) XP_ALLOC(maxlen);
		if (!new_refs) goto FAIL;
		*new_refs = 0;
		if (refs) XP_STRCPY (new_refs, refs);
		if (refs && id) {
			XP_STRCAT (new_refs, " ");
		}
		if (id) XP_STRCAT (new_refs, id);
	}

	if (from || grps || to_and_cc) {
		const char *s = subj;
		if (s) msg_StripRE(&s, NULL);

		re_subject = (char *) XP_ALLOC((s ? XP_STRLEN (s) : 0) + 10);
		if (!re_subject) goto FAIL;
		XP_STRCPY (re_subject, "Re: ");
		if (s) XP_STRCAT (re_subject, s);
	}

	if (from) {
		m_mailToSenderUrl = MakeMailto(from, 0, 0, re_subject, new_refs,
									   0, host, HG62212);
	}

	if (from || to_and_cc) {
		m_mailToAllUrl = MakeMailto(from, to_and_cc, 0, re_subject,
									new_refs, 0, host, HG91611);
	}

	if (grps) {
		if (!strcasecomp(grps, "poster")) {
			/* "Followup-to: poster" makes "post reply" act the same as
			   "reply to sender". */
			if (m_mailToSenderUrl) {
				m_postReplyUrl = XP_STRDUP(m_mailToSenderUrl);
			}
		} else {
			m_postReplyUrl =
				MakeMailto(0, 0, grps, re_subject, new_refs, 0, host, HG81762);
		}
	}

	if (from || grps) {
		if (grps && !strcasecomp(grps, "poster")) {
			/* "Followup-to: poster" makes "post and mail reply" act the
			   same as "reply to sender". */
			if (m_mailToSenderUrl) {
				m_postAndMailUrl = XP_STRDUP(m_mailToSenderUrl);
			}
		} else {
			m_postAndMailUrl = MakeMailto(from, 0, grps,
										  re_subject, new_refs, 0, host,
										  HG76255);
		}
	}

	if (m_folder && !m_folder->IsNews() && mdnNeeded && !isRead)
	{
		// Constructor of MSG_ProcessMdnNeededState will send out
		// an MDN report if needed.
		MSG_ProcessMdnNeededState
			processMdnNeeded(MSG_ProcessMdnNeededState::eDisplayed, 
							 this,
							 m_folder,
							 m_key,
							 headers,
							 FALSE);
		m_view->GetDB()->MarkMDNNeeded(m_key, FALSE);
		m_view->GetDB()->MarkMDNSent(m_key, TRUE);
	}

FAIL:
	FREEIF(host);
	FREEIF(to_and_cc);
	FREEIF(re_subject);
	FREEIF(new_refs);
	FREEIF(from);
	FREEIF(repl);
	FREEIF(subj);
	FREEIF(id);
	FREEIF(refs);
	FREEIF(to);
	FREEIF(cc);
	FREEIF(grps);
	FREEIF(foll);
}


void 
MSG_MessagePane::DoIncWithUIDL_s(URL_Struct *url, int status,
								 MWContext *context)
{
	XP_ASSERT(url && url->msg_pane);
	if (url && url->msg_pane) {
		XP_ASSERT(url->msg_pane->GetPaneType() == MSG_MESSAGEPANE);
		if (url->msg_pane->GetPaneType() == MSG_MESSAGEPANE) {
			((MSG_MessagePane*)url->msg_pane)->DoIncWithUIDL(url, status,
															 context);
		}
	}
}


void
MSG_MessagePane::DoIncWithUIDL(URL_Struct * /*url*/, int /*status*/,
							   MWContext * /*context*/)
{
	if (GetUIDL()) {
		DoCommand(MSG_GetNewMail, NULL, 0);
	}
}




void MSG_MessagePane::PrepareToIncUIDL(URL_Struct* url, const char* uidl)
{
	XP_ASSERT(url && uidl);
	if (!url || !uidl) return;
	FREEIF(m_incUidl);
	m_incUidl = XP_STRDUP(uidl);
	if (url->msg_pane == NULL) url->msg_pane = this;
	XP_ASSERT(url->msg_pane == this);
	if (url && url->msg_pane == this && m_incUidl) {
		url->pre_exit_fn = MSG_MessagePane::DoIncWithUIDL_s;
	}
}


void
MSG_MessagePane::IncorporateShufflePartial(URL_Struct * /* url */, int status,
										   MWContext * /* context */)
{
	FREEIF(m_incUidl);
	if (status < 0)
		return;
	XP_ASSERT(m_view);
	if (!m_view)
		return;
	MessageDB* db = m_view->GetDB();

	XP_ASSERT(db);
	if (!db)
		return;
	XP_ASSERT(m_folder);
	if (!m_folder)
		return;
	MSG_FolderInfoMail* mailfolder = m_folder->GetMailFolderInfo();

	XP_ASSERT(mailfolder);
	if (!mailfolder)
		return;

	MessageKey oldkey = m_key;
	MessageKey newkey = MSG_MESSAGEKEYNONE;
	ListContext* list = NULL;
	DBMessageHdr* hdr = NULL;

	if (db->ListLast(&list, &hdr) == eSUCCESS) {
		newkey = hdr->GetMessageKey();
		db->ListDone(list);
	}

	if (newkey == MSG_MESSAGEKEYNONE)
		return;

	MSG_ThreadPane* threadpane = GetMaster()->FindThreadPaneNamed(mailfolder->GetPathname());
	if (threadpane)
	{
		MessageDBView* threadview = threadpane->GetMsgView();
		if (threadview)
		{
			MSG_ViewIndex index = threadview->FindViewIndex(oldkey);
			if (index != MSG_VIEWINDEXNONE)
				threadview->RemoveByIndex(index);	// kill old msg line
				//threadview->SetKeyByIndex(index, newkey);
		}
	}

	hdr = db->GetDBHdrForKey(oldkey);
	if (hdr) {
		db->RemoveHeaderFromDB(hdr);
		delete hdr;
	}

	LoadMessage(m_folder, newkey, NULL, TRUE);

	// Ok, this is a little weird. The GetNewMail command rightly acquires 
	// the semaphore, and hopes that GetNewMailExit will be called to releaase
	// the semaphore. But when incorporating the rest of a truncated message,
	// the exit function is IncorporateShufflePartial, so release it here.
	m_folder->ReleaseSemaphore (this);
	mailfolder->SetGettingMail(FALSE);
}



char *
MSG_MessagePane::MakeForwardMessageUrl(XP_Bool quote_original)
{
	char *url = 0;
	char *host = ComputeNewshostArg();
	MessageHdrStruct header;
	char *fwd_subject = 0;
	const char *id = 0;
	char *attachment = 0;

	HG22987

	if (!m_view || m_key == MSG_MESSAGEKEYNONE || 
		!m_folder || !m_view->GetDB()) {
		return NULL;
	}
	if (m_view->GetDB()->GetMessageHdr(m_key, &header) != eSUCCESS) {
		return NULL;
	}

	fwd_subject = CreateForwardSubject(&header);
	id = header.m_messageId;

	if (*id) {
		attachment = m_folder->BuildUrl(m_view->GetDB(), m_key);
	}
	
	/* if we are quoting the original document, slip a cookie string in
	   at the beginning of the attachment field.  This will be removed
	   before processing. */
	
	if (quote_original && attachment) {
		char * new_attachment = PR_smprintf(MSG_FORWARD_COOKIE "%s",
											attachment);
		FREEIF(attachment);
		attachment = new_attachment;
	}
	
	url = MakeMailto(0, 0, 0, fwd_subject, 0, attachment, host, HG22296);
	FREEIF(attachment);

	FREEIF(host);
	FREEIF(fwd_subject);

	return url;
}



MsgERR
MSG_MessagePane::ComposeMessage(MSG_CommandType type, Net_GetUrlExitFunc* func)
{
	char *host = 0;
	char *url = 0;
	XP_Bool free_url_p = FALSE;
	URL_Struct *url_struct = 0;
	MSG_FolderInfoNews *newsFolder;
	MSG_PostDeliveryActionInfo *actionInfo = 0;
	XP_Bool checkForHTML = FALSE;

	HG22867

	switch (type) {
	case MSG_MailNew:
		host = ComputeNewshostArg ();
		HG32145
		url = MakeMailto (0, 0, 0, 0, 0, 0, host, HG09990);
		free_url_p = TRUE;
		break;
	case MSG_PostNew:
		// ### tw - fix me - cast hack below
		if (!m_folder->IsNews())
			break;
		newsFolder = (MSG_FolderInfoNews *) m_folder;

		HG89520
		host = ComputeNewshostArg ();
		url = MakeMailto (0, 0, newsFolder->GetNewsgroupName(), 0, 0, 0,
						  host, HG02192);
		free_url_p = TRUE;
		break;
	case MSG_ReplyToSender:
		url = m_mailToSenderUrl;
		checkForHTML = TRUE;
		break;
	case MSG_ReplyToAll:
		url = m_mailToAllUrl;
		checkForHTML = TRUE;
		break;
	case MSG_PostReply:
		url = m_postReplyUrl;
		checkForHTML = TRUE;
		break;
	case MSG_PostAndMailReply:
		url = m_postAndMailUrl;
		checkForHTML = TRUE;
		break;
	case MSG_ForwardMessageAttachment:
		url = MakeForwardMessageUrl(FALSE);
		free_url_p = TRUE;
		break;
	case MSG_ForwardMessageQuoted:
		/* last flag indicates to quote the original message. */
		url = MakeForwardMessageUrl(TRUE);
		free_url_p = TRUE;
		break;
	default:
		XP_ASSERT (0);
		break;
	}

	FREEIF(host);
	if (!url) {
		url = "mailto:";
		free_url_p = FALSE;
	}

	// This is the place to add a "&force-plain-text" to the URL, if we ever
	// again wanted the feature of forcing a plaintext composition when you
	// reply to a plaintext message.

	if (checkForHTML) {
		char* part = MimeGetHtmlPartURL(GetContext());
		if (part) {
			char* tmp = PR_smprintf("%s&html-part=%s", url, part);
			if (tmp) {
				if (free_url_p) XP_FREE(url);
				url = tmp;
				free_url_p = TRUE;
			}
		}
	}

	url_struct = NET_CreateURLStruct (url, NET_NORMAL_RELOAD);
	if (free_url_p && url) {
        XP_FREE(url);
	}
	if (!url_struct) return eOUT_OF_MEMORY;

	/* Get a ride from url_struct->fe_data to attach 
	 * post delivery action info */
	if ( type == MSG_ReplyToSender ||
		 type == MSG_ReplyToAll ||
		 type == MSG_ForwardMessageAttachment ||
		 type == MSG_ForwardMessageQuoted ) {
		actionInfo = new MSG_PostDeliveryActionInfo(m_folder);

		if (actionInfo) {
			actionInfo->m_msgKeyArray.Add(m_key);

			XP_ASSERT( url_struct->fe_data == NULL );

			switch (type) {
			case MSG_ReplyToSender:
			case MSG_ReplyToAll:
				actionInfo->m_flags |= MSG_FLAG_REPLIED;
				url_struct->fe_data = actionInfo;
				/* actionInfo will be freed by CompositionPane
				 * when we close down the Composition Window.
				 */
				break;
			case MSG_ForwardMessageAttachment:
			case MSG_ForwardMessageQuoted:
				actionInfo->m_flags |= MSG_FLAG_FORWARDED;
				url_struct->fe_data = actionInfo;
				/* actionInfo will be freed by CompositionPane
				 * when we close down the Composition Window.
				 */
				break;
			default:
				XP_FREE (actionInfo);
				break;
			}
		}
	}
	url_struct->internal_url = TRUE;
	url_struct->pre_exit_fn = func;

	GetURL (url_struct, TRUE);

	return 0;
}


MsgERR MSG_MessagePane::SetViewFromUrl (const char *url)
{
	XP_ASSERT(!m_view);
	XP_ASSERT(url);

	return MessageDBView::OpenURL(url, GetMaster(), 
		ViewAny, &m_view, TRUE);
}

MsgERR MSG_MessagePane::DeleteMessage ()
{
	MSG_ViewIndex index = m_view->FindViewIndex(m_key);
 
//	if (index != MSG_VIEWINDEXNONE)	// try using the base class method...see what happens.
//		return TrashMessages(&index, 1);

 	MessageKey	 nextKey = MSG_MESSAGEKEYNONE;
	IDArray		*headerIds = new IDArray;
	XP_Bool imapDeleteIsMoveToTrash = (m_folder) ? m_folder->DeleteIsMoveToTrash() : FALSE;

	// allocate an array for the async machine

 	if (!headerIds)
 	{
 		XP_ASSERT(FALSE);
 		return eOUT_OF_MEMORY;
	}
 
 	// fill in the header for the selected message
	headerIds->Add(m_key);
 
 	// copy the messages into the trash folder
 	if (m_folder && (!(m_folder->GetFlags() & MSG_FOLDER_FLAG_TRASH) || !imapDeleteIsMoveToTrash))
 	{
 		XP_ASSERT (m_folder->IsMail());
			
 		MSG_FolderInfoMail *trashFolder = NULL;
		const char *path = NULL;
		if (m_folder->GetType() == FOLDER_MAIL)
		{
			path = m_master->GetPrefs()->MagicFolderName (MSG_FOLDER_FLAG_TRASH);
			trashFolder = m_master->FindMailFolder (path, TRUE);
		}
		else
		{
			if (imapDeleteIsMoveToTrash)
			{
				MSG_IMAPFolderInfoMail *imapInfo = m_folder->GetIMAPFolderInfoMail();
				if (imapInfo)
				{
					MSG_FolderInfo *foundTrash = MSG_GetTrashFolderForHost(imapInfo->GetIMAPHost());
					trashFolder = foundTrash ? foundTrash->GetMailFolderInfo() : (MSG_FolderInfoMail *)NULL;
				}
			}
		}

 		// Figure out what messate to load after this one is deleted
 		// MSG_FolderInfoMail::FinishCopyingMessages does the actual load.
 		if (index != MSG_VIEWINDEXNONE)
 		{
 			nextKey = m_view->GetAt(index + 1);
 			if (nextKey == MSG_MESSAGEKEYNONE)	// if last msg, try getting previous
 				nextKey = m_view->GetAt(index - 1);
 		}
 
		if (trashFolder)
 			m_folder->StartAsyncCopyMessagesInto (trashFolder, 
												  this, 
												  m_view->GetDB(),
												  headerIds, 
												  1, 
												  m_context, 
												  NULL, // do not run in url queue
												  TRUE, 
												  nextKey);
		else if (!imapDeleteIsMoveToTrash && (m_folder->GetType() == FOLDER_IMAPMAIL) )
			((MSG_IMAPFolderInfoMail *)m_folder)->DeleteSpecifiedMessages(this, *headerIds,nextKey);
 		//PHP impedance mismatch on error codes
 	}
 	return 0;
}

// static
void MSG_MessagePane::PostDeleteLoadExitFunction(URL_Struct *URL_s, int status, MWContext *)
{
	if ((status == 0) && (URL_s->msg_pane->GetPaneType() == MSG_MESSAGEPANE))
	{
		MSG_MessagePane *msgPane = (MSG_MessagePane *) URL_s->msg_pane;
		MessageKey loadKey = msgPane->GetPostDeleteLoadKey();
		if (loadKey != MSG_MESSAGEKEYNONE)
		{
			msgPane->LoadMessage(msgPane->GetFolder(), loadKey, NULL, TRUE);
		}
	}
	
	if (URL_s->msg_pane->GetPaneType() == MSG_MESSAGEPANE)
		((MSG_MessagePane *) URL_s->msg_pane)->SetPostDeleteLoadKey(MSG_MESSAGEKEYNONE);
}

MsgERR MSG_MessagePane::DoCommand(MSG_CommandType command,
								  MSG_ViewIndex* indices, int32 numIndices)
{
	// Note that nothing in here pays the slightest attention to indices or
	// numIndices.  On messagepanes, commands effect the displayed message
	// and that's it.

	// We'd better not be trying to do any commands at all if we don't even
	// have a m_view.
	XP_ASSERT(m_view);
	if (!m_view) return eUNKNOWN;

	if (command == MSG_ForwardMessage)
	{
		int32 forwardMsgMode = 0;
		PREF_GetIntPref("mail.forward_message_mode", &forwardMsgMode);
		if (forwardMsgMode == 1)
			command = MSG_ForwardMessageQuoted;
		else if (forwardMsgMode == 2)
			command = MSG_ForwardMessageInline;
		else
			command = MSG_ForwardMessageAttachment;
	}

	MsgERR err = eSUCCESS;

	switch (command)
	{
	case MSG_DeleteMessage:
		{
			// put this bug back in (deleting message from the trash) so that
			// deleting a message from non-trash in stand-alone message will
			// work for alpha.
			// The real solution is to send a notification to the fe that
			// a delete has finished.
			err = DeleteMessage();	
//			MSG_ViewIndex idx = m_view->FindIndex(m_key);
//			err = TrashMessages (&idx, 1);
		}
		break;
	case MSG_PostNew:
	case MSG_PostReply:
	case MSG_PostAndMailReply:
	case MSG_MailNew:
	case MSG_ReplyToSender:
	case MSG_ReplyToAll:
	case MSG_ForwardMessageAttachment:
	case MSG_ForwardMessageQuoted:
        err = ComposeMessage(command, NULL);
		break;
	case MSG_AddSender:
		AddURLToAddressBook(m_mailToSenderUrl);
		break;
	case MSG_AddAll:
		AddURLToAddressBook(m_mailToAllUrl);
		break;
	case MSG_ShowAllHeaders:
	case MSG_ShowSomeHeaders:
	case MSG_ShowMicroHeaders:
		// ### mwelch Changed from m/n prefs object accessor 
		//            to prefs api (will tweak prefs object)
		PREF_SetIntPref("mail.show_headers", 
						(int) (command - MSG_ShowMicroHeaders));
		//GetMaster()->GetPrefs()->SetHeaderStyle (command);
		err = LoadMessage (m_folder, m_key, NULL, TRUE); // is there an easier way to do this?
		break;
	case MSG_SaveMessagesAs:
	case MSG_ForwardMessageInline:
	case MSG_OpenMessageAsDraft:
		{
			MSG_ViewIndex index = m_view->FindViewIndex(m_key);
			err = MSG_Pane::DoCommand(command, &index, 1);
		}
		break;
	case MSG_AttachmentsInline:
	case MSG_AttachmentsAsLinks:
		{
			MessageKey key = m_key;
			PREF_SetBoolPref("mail.inline_attachments", 
							 (command == MSG_AttachmentsInline));
			LoadMessage (m_folder, MSG_MESSAGEKEYNONE, NULL, TRUE);
			LoadMessage (m_folder, key, NULL, TRUE);
		}
		break;
	case MSG_WrapLongLines:
		{
			MessageKey key = m_key;
			XP_Bool newWrap = !GetPrefs()->GetWrapLongLines();
			PREF_SetBoolPref("mail.wrap_long_lines", newWrap);
			LoadMessage (m_folder, MSG_MESSAGEKEYNONE, NULL, TRUE);
			LoadMessage (m_folder, key, NULL, TRUE);
		}
		break;

	case MSG_Rot13Message:
		{
			m_rot13p = !m_rot13p;
			MessageKey key = m_key;
			LoadMessage (m_folder, MSG_MESSAGEKEYNONE, NULL, TRUE);
			LoadMessage (m_folder, key, NULL, TRUE);
		}
		break;
	case MSG_CompressFolder:
		{
			MSG_FolderInfoMail *mailFolder = GetFolder()->GetMailFolderInfo();
			if (mailFolder)
				err = CompressOneMailFolder(mailFolder);
			else
				XP_ASSERT(FALSE);	// MSG_CommandStatus should have failed!
			break;
		}
	default:
		if (!indices)
		{
			MSG_ViewIndex index = m_view->FindViewIndex(m_key);
			if (index != MSG_VIEWINDEXNONE )
			{
				indices = &index;
				numIndices = 1;
			}
		}
		err = MSG_Pane::DoCommand(command, indices, numIndices);
	}
	return err;
}

MsgERR MSG_MessagePane::AddToAddressBook(MSG_CommandType command, MSG_ViewIndex*indices, int32 numIndices, AB_ContainerInfo * destAB)
{
	MsgERR status = 0;
	MessageHdrStruct header;

	status = m_view->GetDB()->GetMessageHdr(m_key, &header);
	if (status != eSUCCESS)
		return status;

	switch(command)
	{
	case MSG_AddSender:
		AB_AddSenderAB2(this, destAB, header.m_author, m_mailToSenderUrl);
		break;
	case MSG_AddAll:
		AB_AddSenderAB2(this, destAB, header.m_author, m_mailToAllUrl);
		break;
	default:
		XP_ASSERT(0);
	}

	return status;
}

MsgERR MSG_MessagePane::AddURLToAddressBook(char * url)
{
	MessageHdrStruct header;

	if (!url) return 0;
	if (m_view->GetDB()->GetMessageHdr(m_key, &header) != eSUCCESS) {
		return 0;
	}
	// assume add to personal address book
	AB_AddSenderToAddBook(FE_GetAddressBook(this), GetContext(), header.m_author, url);
	return 0;
}


MsgERR MSG_MessagePane::GetCommandStatus(MSG_CommandType command,
										 const MSG_ViewIndex* indices,
										 int32 numindices,
										 XP_Bool *selectable_pP,
										 MSG_COMMAND_CHECK_STATE *selected_p,
										 const char **display_stringP,
										 XP_Bool * plural_p)
{
	MsgERR err = eSUCCESS;
	const char *display_string = NULL;
	XP_Bool	selectable_p = TRUE;
	XP_Bool showingMsg = (m_key != MSG_MESSAGEKEYNONE);
	MSG_ViewIndex	curIndex;

	XP_Bool		news_p = (m_folder && m_folder->IsNews() );

	switch (command)
	{
	case MSG_CompressFolder:
		{
			selectable_p = m_folder && m_folder->IsMail();
			break;
		}
	case MSG_DeleteMessageNoTrash:
	case MSG_DeleteMessage:
		{
			MessageDB *db = (m_view) ? m_view->GetDB() : 0;
		selectable_p = (db != NULL && NULL != db->GetMailDB() && showingMsg);
		if (selectable_p)
			selectable_p = (MSG_VIEWINDEXNONE != m_view->FindViewIndex(m_key));

								// can't delete news msg
		display_string = XP_GetString (MK_MSG_DELETE_MESSAGE);
		if (selected_p)
		  *selected_p = MSG_NotUsed;
		break;
		}
	case MSG_MarkMessagesRead:
		{
		  MessageDB *db = (m_view) ? m_view->GetDB() : 0;
		  if (m_view && db)
			{
			  XP_Bool isRead;
			  if (db->IsRead(m_view->GetAt(m_view->FindViewIndex(m_key)), &isRead) == eSUCCESS) 
				{
				  if (!isRead)
					{
					  selectable_p = TRUE;
					}
				}
			  display_string = XP_GetString (MK_MSG_MARK_AS_READ);
			  if (selected_p)
				*selected_p = MSG_NotUsed;
			}
		  break;
		}
	case MSG_MarkMessagesUnread:
		{
		  MessageDB *db = (m_view) ? m_view->GetDB() : 0;
		  if (m_view && db)
			{
			  XP_Bool isRead;
			  if (db->IsRead(m_view->GetAt(m_view->FindViewIndex(m_key)), &isRead) == eSUCCESS) 
				{
				  if (isRead)
					{
					  selectable_p = TRUE;
					}
				}
			  display_string = XP_GetString (MK_MSG_MARK_AS_UNREAD);
			  if (selected_p)
				*selected_p = MSG_NotUsed;
			}
		  break;
		}
	case MSG_AddSender:
		display_string = XP_GetString(MK_MSG_ADD_SENDER_TO_ADDRESS_BOOK);
		selectable_p = showingMsg;
		break;
	case MSG_AddAll:
		display_string = XP_GetString(MK_MSG_ADD_ALL_TO_ADDRESS_BOOK);
		selectable_p = showingMsg;
		break;
	case MSG_PostNew:
		display_string = XP_GetString(MK_MSG_NEW_NEWS_MESSAGE);
		selectable_p = news_p && m_folder->AllowsPosting();
		break;
	case MSG_PostReply:
		display_string = XP_GetString(MK_MSG_POST_REPLY);
		selectable_p = news_p && showingMsg && m_folder->AllowsPosting();
		break;
	case MSG_PostAndMailReply:
		display_string = XP_GetString(MK_MSG_POST_MAIL_REPLY);
		selectable_p = news_p && showingMsg && m_folder->AllowsPosting();
		break;
	case MSG_MailNew:
		display_string = XP_GetString(MK_MSG_NEW_MAIL_MESSAGE);
		break;
	case MSG_ReplyToSender:
		display_string = XP_GetString(MK_MSG_REPLY_TO_SENDER);
		selectable_p = showingMsg;
		break;
	case MSG_ReplyToAll:
		display_string = XP_GetString(MK_MSG_REPLY_TO_ALL);
		selectable_p = showingMsg;
		break;
	case MSG_ForwardMessageQuoted:
		display_string = XP_GetString(MK_MSG_FORWARD_QUOTED);
		selectable_p = showingMsg;
		break;
	case MSG_ForwardMessage:
	case MSG_ForwardMessageAttachment:
  	    if (plural_p)
		  *plural_p = FALSE;
		selectable_p = showingMsg;
		display_string =  XP_GetString(MK_MSG_FORWARD);
		break;
	case MSG_Rot13Message:
		selectable_p = showingMsg;
		if (selected_p)
			*selected_p = (m_rot13p) ? MSG_Checked : MSG_Unchecked;
		break;
	case MSG_ShowAllHeaders:
	case MSG_ShowSomeHeaders:
	case MSG_ShowMicroHeaders:
		if (selected_p)
			*selected_p = (command == GetPrefs()->GetHeaderStyle() ?
						   MSG_Checked : MSG_Unchecked);
		selectable_p = showingMsg;
		break;
	case MSG_AttachmentsInline:
		if (selected_p)
			*selected_p = (GetPrefs()->GetNoInlineAttachments() ?
						   MSG_Unchecked : MSG_Checked);
		selectable_p = showingMsg;
		break;
	case MSG_AttachmentsAsLinks:
		if (selected_p)
			*selected_p = (GetPrefs()->GetNoInlineAttachments() ?
						   MSG_Checked : MSG_Unchecked);
		selectable_p = showingMsg;
		break;
	case MSG_WrapLongLines:
		if (selected_p)
			*selected_p = (GetPrefs()->GetWrapLongLines() ?
						   MSG_Checked : MSG_Unchecked);
		selectable_p = showingMsg;
		break;
	default:
		if (indices == NULL && m_view)
		{
			curIndex = m_view->FindViewIndex(m_key);
			indices = &curIndex;
			numindices = 1;
		}
		return MSG_Pane::GetCommandStatus(command, indices, numindices,
										 selectable_pP, selected_p,
										 display_stringP, plural_p);
	}
	if (display_stringP)
	  *display_stringP = display_string;

	if (selectable_pP)
	  *selectable_pP = selectable_p;

	return err;
}


void 
MSG_MessagePane::GetCurMessage(MSG_FolderInfo** folder, MessageKey* key,
							   MSG_ViewIndex *index)
{
	MessageKey	curKey = (m_folder) ? m_key : MSG_MESSAGEKEYNONE;

	if (folder) *folder = m_folder;
	if (key) *key = curKey;
	if (index)
	{
		*index = (m_view && curKey != MSG_MESSAGEKEYNONE)
			? m_view->FindViewIndex(curKey) : MSG_VIEWINDEXNONE;
	}
}

int
MSG_MessagePane::GetViewedAttachments(MSG_AttachmentData** data,
									  XP_Bool* iscomplete)
{
	if (iscomplete) *iscomplete = m_doneLoading;
	return MimeGetAttachmentList(GetContext(), data);
}


void
MSG_MessagePane::FreeAttachmentList(MSG_AttachmentData* data)
{
	MimeFreeAttachmentList(data);
}


HG82821

XP_Bool MSG_MessagePane::GetThreadLineByKey(MessageKey key, MSG_MessageLine* data) 
{
	XP_ASSERT(m_view);
	if (!m_view) 
		return FALSE;

//	XP_ASSERT(key == m_key); //When we load new msg ,this isn't be true

	int numlisted = -1;
	MsgERR status = m_view->ListThreadsShort(&key, 1, data, &numlisted);
	return (status == 0 && numlisted == 1);
}


XP_Bool MSG_MessagePane::ShouldRot13Message()
{
	return m_rot13p;
}



/* This function opens a message and returns a handle to that message in the
 * msg_ptr pointer.
 *
 * The message handle will be passed to MSG_ReadMessage and MSG_CloseMessage
 * to read data and to close the message
 *
 * Return values: return a negative return value listed in merrors.h to
 * signify an error.  return zero (0) on success.
 *
 * !Set message_ptr to NULL on error!
 */

int
MSG_MessagePane::OpenMessageSock(const char *folder_name,
								 const char *msg_id, int32 msgnum,
								 void *folder_ptr, void **message_ptr,
								 int32 *content_length)
{
	if (!m_view)
		InitializeFolderAndView (folder_name);

	if (msgnum == MSG_MESSAGEKEYNONE && m_view)
	{
		MailMessageHdr *hdr = (MailMessageHdr*) m_view->GetDB()->GetDBMessageHdrForID(msg_id);
		if (hdr)
			msgnum = hdr->GetMessageKey();
	}

	if ( msgnum != MSG_MESSAGEKEYNONE ) {
	    m_key = msgnum;
		InitializeFolderAndView (folder_name);
		FE_PaneChanged(this, FALSE, MSG_PaneNotifyMessageLoaded, msgnum);
	}

	return MSG_Pane::OpenMessageSock ( folder_name, msg_id, msgnum,
									   folder_ptr, message_ptr, 
									   content_length );
}


/* Partial messages
 */


char *
MSG_MessagePane::GeneratePartialMessageBlurb(URL_Struct *url,
											 void * /*closure*/,
											 MimeHeaders *headers)

{
	char *stuff = 0;
	char *new_url = 0, *quoted = 0;
	unsigned long flags = 0;
	char dummy = 0;
	char *uidl = 0, *xmoz = 0;

	/* The message is partial if it has an X-UIDL header field, and it has
	   an X-Mozilla-Status header field which contains the `partial' bit.
	   */

	uidl = MimeHeaders_get(headers, HEADER_X_UIDL, FALSE, FALSE);
	if (!uidl) goto DONE;
	xmoz = MimeHeaders_get(headers, HEADER_X_MOZILLA_STATUS, FALSE, FALSE);
	if (!xmoz) goto DONE;

	if (1 != sscanf (xmoz, " %lx %c", &flags, &dummy)) goto DONE;
          
	if (! (flags & MSG_FLAG_PARTIAL)) goto DONE;

	quoted = NET_Escape (uidl, URL_XALPHAS);
	if (!quoted) goto DONE;
	FREEIF(uidl);

	new_url = (char *) XP_ALLOC (XP_STRLEN (url->address) +
								 XP_STRLEN (quoted) + 20);
	if (!new_url) goto DONE;

	XP_STRCPY (new_url, url->address);
	XP_ASSERT (XP_STRCHR (new_url, '?'));
	if (XP_STRCHR (new_url, '?')) {
        XP_STRCAT (new_url, "&uidl=");
	} else {
        XP_STRCAT (new_url, "?uidl=");
	}
	XP_STRCAT (new_url, quoted);
	FREEIF(quoted);

	stuff = PR_smprintf ("%s%s%s%s",
						 XP_GetString(MK_MSG_PARTIAL_MSG_FMT_1),
						 XP_GetString(MK_MSG_PARTIAL_MSG_FMT_2),
						 new_url,
						 XP_GetString(MK_MSG_PARTIAL_MSG_FMT_3));

DONE:
	FREEIF(new_url);
	FREEIF(quoted);
	FREEIF(uidl);
	FREEIF(xmoz);

	return stuff;
}

URL_Struct * MSG_MessagePane::ConstructUrlForMessage(MessageKey key)
{
	if (key == MSG_MESSAGEKEYNONE)
		key = m_key;
	return MSG_Pane::ConstructUrlForMessage(key);
}


MsgERR
MSG_MessagePane::LoadFolder(MSG_FolderInfo* f) 
{
	char	*url = NULL;
	XP_Bool	finishedLoadingFolder = FALSE;	// are we finished loading folder?
	XP_StatStruct folderst;

	if (f == m_folder) return 0;

	m_folder = f;

	if (!f)
		return CloseView();

	XP_ASSERT(f->GetType() == FOLDER_MAIL || 
			  f->GetType() == FOLDER_NEWSGROUP ||
			  f->GetType() == FOLDER_IMAPMAIL ||
			  f->GetType() == FOLDER_CATEGORYCONTAINER);

	// we need to abort any current background parsing.
	if (m_folder != NULL)
		msg_InterruptContext(GetContext(), TRUE); 

	if (m_folder && m_folder->UserNeedsToAuthenticateForFolder(FALSE) && m_master->PromptForHostPassword(GetContext(), m_folder) != 0)
		return 0;

	CloseView();

	url = f->BuildUrl(NULL, MSG_MESSAGEKEYNONE);

	ViewType typeOfView = ViewAny;

	
	MsgERR status = MessageDBView::OpenURL(url, GetMaster(),
						 				   typeOfView, &m_view, FALSE);

	
	// fire off the IMAP url to update this folder, if it is IMAP
	if ((f->GetType() == FOLDER_IMAPMAIL) && (status == eSUCCESS))
	{
		MSG_IMAPFolderInfoMail *imapFolder = f->GetIMAPFolderInfoMail();
		if (imapFolder)
		{
			imapFolder->StartUpdateOfNewlySelectedFolder(this,TRUE,NULL);
		}
		else
		{
			XP_ASSERT(FALSE);
		}
	}
	
	if (f->GetType() == FOLDER_MAIL && status != eSUCCESS /*== eOldSummaryFile*/)
	{
		MailDB	*mailDB;

		XP_Trace("blowing away old summary file\n");
		const char* pathname = ((MSG_FolderInfoMail*) f)->GetPathname();
		if (!XP_Stat(pathname, &folderst, xpMailFolderSummary) && XP_FileRemove(pathname, xpMailFolderSummary) != 0)
		{
			status = MK_MSG_CANT_OPEN;
		}
		else
		{
			MSG_Master *mailMaster = GetMaster();
			URL_Struct *url_struct;
			url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);

			status = MailDB::Open(pathname, TRUE, &mailDB, TRUE);
			if (mailDB != NULL && (status == eSUCCESS || status == eNoSummaryFile))
			{
				status = MessageDBView::OpenViewOnDB(mailDB, 
					ViewAny, &m_view);
				mailDB->Close(); // decrement ref count.
			}
			if (m_view != NULL && m_view->GetDB() != NULL)
			{
				ParseMailboxState *parseMailboxState = new ParseMailboxState(pathname);
				parseMailboxState->SetView(m_view);  
				parseMailboxState->SetPane(this);
				parseMailboxState->SetIncrementalUpdate(FALSE);
				parseMailboxState->SetMaster(mailMaster);
				parseMailboxState->SetDB(m_view->GetDB()->GetMailDB());
				parseMailboxState->SetContext(GetContext());
				parseMailboxState->SetFolder(m_folder);
				((MSG_FolderInfoMail*)f)->SetParseMailboxState(parseMailboxState);
				// fire off url to create summary file from mailbox - will work in background
				GetURL(url_struct, FALSE);
			}
		}
	}
	else if (f->IsNews())
	{
		MSG_FolderInfoNews *newsFolder = f->GetNewsFolderInfo();
		XP_ASSERT(newsFolder);
		const char* groupname = newsFolder->GetNewsgroupName();
		if (status != eSUCCESS)
		{
			if (status == eErrorOpeningDB || status == eOldSummaryFile) // corrupt database or out of date
			{
				NewsGroupDB	*newsDB;
				const char *dbFileName = newsFolder->GetXPDBFileName();

				if (dbFileName != NULL)
					XP_FileRemove(dbFileName, xpXoverCache);
				status = NewsGroupDB::Open(url, GetMaster(), &newsDB);
				if (status == eSUCCESS)
				{
					status = MessageDBView::OpenViewOnDB(newsDB, 
						ViewAny, &m_view);
					newsDB->Close();
				}
			}
		}
		if (status == eSUCCESS)	// get new articles from server.
		{
			MSG_Master *master = GetMaster();
			URL_Struct *url_struct;
			url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
			ListNewsGroupState * listState = new ListNewsGroupState(url, groupname, this);
			listState->SetMaster(master);
			listState->SetView(m_view);
			newsFolder->SetListNewsGroupState(listState);
			int status = GetURL(url_struct, FALSE);
			if (status == MK_INTERRUPTED || status == MK_OFFLINE)	
			{
				if (newsFolder->GetListNewsGroupState())	// FinishXOver not called...
				{
					delete listState;
					newsFolder->SetListNewsGroupState(NULL);
				}
				StartingUpdate(MSG_NotifyAll, 0, 0);
				EndingUpdate(MSG_NotifyAll, 0, 0);
				finishedLoadingFolder = TRUE;
			}
		}
		else if (status == eBuildViewInBackground)
		{
			status = ListThreads();
		}
	}
	else if (f->GetType() == FOLDER_IMAPMAIL && status != eSUCCESS)
	{
		const char* pathname = ((MSG_FolderInfoMail*) f)->GetPathname();
		XP_FileRemove(pathname, xpMailFolderSummary);
	}
	else if (status == eSUCCESS)
	{
		finishedLoadingFolder = TRUE;
	}
	if (status == eSUCCESS)
	{
		m_view->Add(GetListener());
	}

									// imap folders loads are asynchronous
									// this notification will happen in
									// MSG_IMAPFolderInfoMail::AllFolderHeadersAreDownloaded
	if (finishedLoadingFolder && (f->GetType() != FOLDER_IMAPMAIL))
	{
		FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderLoaded, (uint32) f);
	}

	if (m_folder)
		m_folder->SummaryChanged();	// for Will - so thread pane can display counts.

	XP_FREE(url);
	return status;
}

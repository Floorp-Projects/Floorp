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
#include "xpgetstr.h"
#include "libi18n.h"

#include "msgpane.h"
#include "msgprefs.h"
#include "msgmast.h"
#include "msghdr.h"

#include "xp_time.h"
#include "xplocale.h"
#include "msg.h"
#include "msgmast.h"

#include "prefapi.h"
#include "xp_qsort.h"
#include "intl_csi.h"
#include "xlate.h"
#include "msgurlq.h"
#include "pw_public.h"
#include "mime.h"

extern "C"
{
extern int MK_MSG_ADDRESS_BOOK;
extern int MK_MSG_COMPRESS_ALL_FOLDER;
extern int MK_MSG_EMPTY_TRASH_FOLDER;
extern int MK_MSG_ERROR_WRITING_MAIL_FOLDER;
extern int MK_MSG_GET_NEW_MAIL;
extern int MK_MSG_GET_NEW_DISCUSSION_MSGS;
extern int MK_MSG_NEW_MAIL_MESSAGE;
extern int MK_MSG_NO_POP_HOST;
extern int MK_OUT_OF_MEMORY;
extern int MK_MSG_SAVE_MESSAGE_AS;
extern int MK_MSG_SAVE_MESSAGES_AS;
extern int MK_MSG_OPEN_DRAFT;
extern int MK_MSG_ID_NOT_IN_FOLDER;
extern int MK_MSG_FOLDER_UNREADABLE;
extern int MK_MSG_DELIV_NEW_MSGS;
extern int MK_MSG_QUEUED_DELIVERY_FAILED;
extern int MK_MSG_NEWS_HOST_TABLE_INVALID;
extern int MK_MSG_CANCEL_MESSAGE;
extern int MK_MSG_MESSAGE_CANCELLED;
extern int MK_MSG_MARK_SEL_AS_READ;
extern int MK_MSG_MARK_SEL_AS_UNREAD;
extern int MK_MSG_MARK_THREAD_READ;
extern int MK_MSG_MARK_ALL_READ;
extern int MK_MSG_BACKTRACK;
extern int MK_MSG_GO_FORWARD;
extern int MK_MSG_UNABLE_MANAGE_MAIL_ACCOUNT;
extern int MK_POP3_NO_MESSAGES;
extern int MK_MSG_MANAGE_MAIL_ACCOUNT;
extern int MK_MSG_CANT_DELETE_RESERVED_FOLDER;
extern int MK_MSG_PANES_OPEN_ON_FOLDER;
extern int MK_MSG_DELETE_FOLDER_MESSAGES;
extern int MK_MSG_NO_POST_TO_DIFFERENT_HOSTS_ALLOWED;
extern int MK_MSG_GROUP_NOT_ON_SERVER;
extern int MK_MSG_NEW_NEWSGROUP;
extern int MK_MSG_ADVANCE_TO_NEXT_FOLDER;
extern int MK_MSG_FLAG_MESSAGE;
extern int MK_MSG_UNFLAG_MESSAGE;
extern int MK_MSG_RETRIEVE_FLAGGED;
extern int MK_MSG_RETRIEVE_SELECTED;
XP_Bool NET_IsNewsMessageURL (const char *url);
} 

MSG_Pane* MSG_Pane::MasterList = NULL;



typedef struct msg_incorporate_state
{
	MWContext				*context;
	MSG_FolderInfoMail	*inbox;
	MSG_Pane		*pane;
	const char* dest;
	const char *destName;
	int32 start_length;
//	int numdup;
	char *ibuffer;
	uint32 ibuffer_size;
	uint32 ibuffer_fp;
#ifdef MANGLE_INTERNAL_ENVELOPE_LINES
	XP_Bool mangle_from;			/* True if "From " lines need to be subject
								   to the Usual Mangling Conventions.*/
#endif /* MANGLE_INTERNAL_ENVELOPE_LINES */
	char* headers;
	uint32 headers_length;
	uint32 headers_maxlength;
	XP_Bool gathering_headers;
	XP_Bool expect_multiple;
	XP_Bool expect_envelope;
	ParseMailboxState *incparsestate; /* Parse state for messages */
	int status;
} msg_incorporate_state;



MSG_Pane::MSG_Pane(MWContext* context, MSG_Master* master) {
	m_context = context;
	m_nextInMasterList = MasterList;

	MasterList = this;

	m_master = master;
	if (master) {
		m_nextPane = master->GetFirstPane();
		master->SetFirstPane(this);
		m_prefs = master->GetPrefs();
		m_prefs->AddNotify(this);

		m_context->mailMaster = master;
	}
}


MSG_Pane::~MSG_Pane() {
	UnregisterFromPaneList();

	if (m_master) {
		m_master->GetPrefs()->RemoveNotify(this);
	}

	if (m_progressContext)
		PW_DestroyProgressContext(m_progressContext);
}


MSG_Pane* MSG_Pane::GetFirstPaneForContext(MWContext *context)
{

	if (context)
		return GetNextPaneForContext(NULL, context);
	
	return(NULL);
}

MSG_Pane* MSG_Pane::GetNextPaneForContext(MSG_Pane *startPane, MWContext *context)
{
	MSG_Pane* result = NULL;
	result = (startPane) ? startPane->m_nextInMasterList : MasterList;

	for (; result ; result = result->m_nextInMasterList) 
	{
		if (result->GetContext() == context)
			return result;
	}
	return NULL;
}


// Remove a pane from the pane list
// Note that if after we remove ourselves from the list, the only pane left
// belongs to the Biff (Check for New Mail) Master then we tell it to go away, which will cause
// its own hidden progress window and context to be deleted.

void MSG_Pane::UnregisterFromPaneList()
{
	if (m_master) {
		MSG_Pane* tmp = m_master->GetFirstPane();
		if (tmp == this) {
			m_master->SetFirstPane(m_nextPane);
		} else {
			for (; tmp ; tmp = tmp->m_nextPane) {
				if (tmp->m_nextPane == this) {
					tmp->m_nextPane = m_nextPane;
					break;
				}
			}
		}
	}

	MSG_Pane** ptr;
	for (ptr = &MasterList ; *ptr ; ptr = &((*ptr)->m_nextInMasterList)) {
		if (*ptr == this) {
			*ptr = this->m_nextInMasterList;
			break;
		}
	}
}

// this method can be used to find out if a pane has been deleted...
/*static*/ XP_Bool MSG_Pane::PaneInMasterList(MSG_Pane *pane)
{
	MSG_Pane* curPane;
	XP_Bool ret = FALSE;
	// it will return FALSE if pane is NULL
	for (curPane = MasterList ; curPane ; curPane = curPane->m_nextInMasterList) 
	{
		if (curPane == pane)
		{
			ret = TRUE;
			break;
		}
	}
	return ret;
}


MSG_Pane* MSG_Pane::FindPane(MWContext* context, MSG_PaneType type, XP_Bool contextMustMatch /* = FALSE */) {
  MSG_Pane* result;
  for (result = MasterList ; result ; result = result->m_nextInMasterList) {
	if (result->GetContext() == context && (type == MSG_ANYPANE ||
											result->GetPaneType() == type)) {
	  return result;
	}
  }
  if (!contextMustMatch)
  {
	  for (result = MasterList ; result ; result = result->m_nextInMasterList) {
		if (type == MSG_ANYPANE || result->GetPaneType() == type) {
		  return result;
		}
	  }
  }
  return NULL;
}

/* static */ MSG_PaneType MSG_Pane::PaneTypeForURL(const char *url)
{
	MSG_PaneType retType = MSG_ANYPANE;
	int urlType = NET_URL_Type(url);
	char *folderName = NULL;

	switch (urlType)
	{
		case MAILTO_TYPE_URL:
			retType = MSG_COMPOSITIONPANE;
			break;
		default:
			break;
	}
	FREEIF(folderName);
	return retType;
}

/* inline virtuals moved to cpp file to help compilers that don't implement virtuals
	defined in headers well.
*/
MSG_PaneType MSG_Pane::GetPaneType() {return MSG_PANE;}
void MSG_Pane::NotifyPrefsChange(NotifyCode /*code*/) {}
MSG_Pane* MSG_Pane::GetParentPane() {return NULL;}

void MSG_Pane::SetFEData(void* data) {
	m_fedata = data;
}

void* MSG_Pane::GetFEData() {
	return m_fedata;
}

XP_Bool MSG_Pane::IsLinePane() {
	return FALSE;
}

MWContext* MSG_Pane::GetContext() {
	return m_context;
}

MSG_Prefs* MSG_Pane::GetPrefs() {
	return m_prefs;
}

MsgERR
MSG_Pane::ComposeNewMessage()
{
#ifdef XP_UNIX
	return (MSG_Mail(GetContext())) ? 0 : MK_OUT_OF_MEMORY;
#else
    XP_ASSERT(FALSE);
    return MK_OUT_OF_MEMORY;
#endif //XP_UNIX
}

//This is a long one.  I'd like to break it down but some of the work
//that is done here just isn't really useful elsewhere.  This handles
//serveral selection cases in the threadwindow of the Mail and News pane.
//If the individual selected something which we can use to populate the
//send to lines, it get added into the addressee fields with the group label.
//If nothing of any use was selected, the "mailto" label is used with a blank.
//They are not allowed to select groups across news servers.  Such an act
//will result in an informative error message and bring you back to the
//mail and news pane.  
MsgERR
MSG_Pane::ComposeMessageToMany(MSG_ViewIndex* , int32 )
{
		return ComposeNewMessage();
}




char*
MSG_Pane::CreateForwardSubject(MessageHdrStruct* header)
{
	char *fwd_subject = 0;
	const char *subject = 0;
	char *conv_subject = 0;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(GetContext());

	subject = header->m_subject;

	while (XP_IS_SPACE(*subject)) subject++;

	conv_subject = IntlDecodeMimePartIIStr(subject, INTL_GetCSIWinCSID(c), FALSE);
	if (conv_subject == NULL)
		conv_subject = (char *) subject;
	fwd_subject =
		(char *) XP_ALLOC((conv_subject ? XP_STRLEN(conv_subject) : 0) + 20);
	if (!fwd_subject) goto FAIL;

	XP_STRCPY (fwd_subject, "[Fwd: ");
	if (header->m_flags & kHasRe) {
		XP_STRCAT (fwd_subject, "Re: ");
	}
	XP_STRCAT (fwd_subject, conv_subject);
	XP_STRCAT (fwd_subject, "]");

FAIL:
	if (conv_subject != subject) {
		FREEIF(conv_subject);
	}
	return fwd_subject;
}

void
MSG_Pane::InterruptContext(XP_Bool /*safetoo*/)
{
	XP_InterruptContext(m_context);
}


MsgERR
MSG_Pane::DoCommand(MSG_CommandType command, MSG_ViewIndex* indices,
					int32 numIndices)
{
	MsgERR status = 0;

	switch (command) {
		break;
	case MSG_MailNew:
		status = ComposeMessageToMany(indices, numIndices);
		break;
  default:
#ifdef DEBUG
	  FE_Alert (GetContext(), "command not implemented");
#endif
	break;
  }
  return status;
}


MsgERR
MSG_Pane::GetCommandStatus(MSG_CommandType command,
						   const MSG_ViewIndex* indices, int32 numIndices,
						   XP_Bool *selectable_pP,
						   MSG_COMMAND_CHECK_STATE *selected_pP,
						   const char **display_stringP,
						   XP_Bool *plural_pP)
{
	const char *display_string = 0;
	XP_Bool plural_p = FALSE;
	XP_Bool selectable_p = FALSE;
	XP_Bool selected_p = FALSE;
	XP_Bool selected_used_p = FALSE;
	
	switch(command) {

	case MSG_MailNew:
		display_string = XP_GetString(MK_MSG_NEW_MAIL_MESSAGE);
		// don't enable compose if we're parsing a folder
        selectable_p = TRUE;
		break;
  default:
//	XP_ASSERT(0);
	break;
  }
  if (selectable_pP)
	*selectable_pP = selectable_p;
  if (selected_pP)
  {
    if (selected_used_p)
    {
      if (selected_p)
        *selected_pP = MSG_Checked;
      else
        *selected_pP = MSG_Unchecked;
    }
    else
    {
      *selected_pP = MSG_NotUsed;
    }
  }
  if (display_stringP)
	*display_stringP = display_string;
  if (plural_pP)
	*plural_pP = plural_p;

  return 0;
}


MSG_COMMAND_CHECK_STATE
MSG_Pane::GetToggleStatus(MSG_CommandType command, MSG_ViewIndex* indices,
						  int32 numindices)
{
  MSG_COMMAND_CHECK_STATE result = MSG_NotUsed;
  if (GetCommandStatus(command, indices, numindices, NULL, &result,
					   NULL, NULL) != 0) {
	return MSG_NotUsed;
  }
  return result;
}


MsgERR
MSG_Pane::SetToggleStatus(MSG_CommandType command,
						  MSG_ViewIndex* indices, int32 numindices,
						  MSG_COMMAND_CHECK_STATE value)
{
	MsgERR	status = eSUCCESS;

	MSG_COMMAND_CHECK_STATE old = GetToggleStatus(command, indices, numindices);
	if (old == MSG_NotUsed) return eUNKNOWN;
	if (old != value) 
	{
		if ((status = DoCommand(command, indices, numindices)) == eSUCCESS)
		{
			if (GetToggleStatus(command, indices, numindices) != value) 
			{
				XP_ASSERT(0);
				return eUNKNOWN;
			}
		}
	}
	return status;
}

void MSG_Pane::SetRequestForReturnReceipt(XP_Bool bRequested)
{
	m_requestForReturnReceipt = bRequested;
}

XP_Bool MSG_Pane::GetRequestForReturnReceipt()
{
	return m_requestForReturnReceipt;
}

void MSG_Pane::SetSendingMDNInProgress(XP_Bool inProgress)
{
	m_sendingMDNInProgress = inProgress;
}

XP_Bool MSG_Pane::GetSendingMDNInProgress()
{
	return m_sendingMDNInProgress;
}

char*
MSG_Pane::MakeMailto(const char *to, const char *cc,
					const char *newsgroups,
					const char *subject, const char *references,
					const char *attachment, const char *host_data,
					XP_Bool xxx_p, XP_Bool sign_p)
{
	char *url=NULL;
#ifdef XP_UNIX
    char *to2 = 0, *cc2 = 0;
	char *out, *head;
	char *qto, *qcc, *qnewsgroups, *qsubject, *qreferences;
	char *qattachment, *qhost_data;
	char *me = MIME_MakeFromField();
	char *to_plus_me = 0;

	to2 = MSG_RemoveDuplicateAddresses (to, ((cc && *cc) ? me : 0), TRUE /*removeAliasesToMe*/);
	if (to2 && !*to2) {
		XP_FREE(to2);
		to2 = 0;
	}

	/* This to_plus_me business is so that, in reply-to-all of a message
	   to which I was a recipient, I don't go into the CC field (that's
	   what BCC/FCC are for.) */
	if (to2 && cc && me) {
		to_plus_me = (char *) XP_ALLOC(XP_STRLEN(to2) + XP_STRLEN(me) + 10);
	}
	if (to_plus_me) {
		XP_STRCPY(to_plus_me, me);
		XP_STRCAT(to_plus_me, ", ");
		XP_STRCAT(to_plus_me, to2);
	}
	FREEIF(me);

	cc2 = MSG_RemoveDuplicateAddresses (cc, (to_plus_me ? to_plus_me : to2), TRUE /*removeAliasesToMe*/);
	if (cc2 && !*cc2) {
		XP_FREE(cc2);
		cc2 = 0;
	}

	FREEIF(to_plus_me);

	/* Catch the case of "Reply To All" on a message that was from me.
	   In that case, we've got an empty To: field at this point.
	   What we should do is, promote the first CC address to the To:
	   field.  But I'll settle for promoting all of them.
	   */
	if (cc2 && *cc2 && (!to2 || !*to2)) {
		FREEIF(to2);
		to2 = cc2;
		cc2 = 0;
	}

	qto		  = to2		   ? NET_Escape (to2, URL_XALPHAS)		  : 0;
	qcc		  = cc2		   ? NET_Escape (cc2, URL_XALPHAS)		  : 0;
	qnewsgroups = newsgroups ? NET_Escape (newsgroups, URL_XALPHAS) : 0;
	qsubject	  = subject    ? NET_Escape (subject, URL_XALPHAS)    : 0;
	qreferences = references ? NET_Escape (references, URL_XALPHAS) : 0;
	qattachment = attachment ? NET_Escape (attachment, URL_XALPHAS) : 0;
	qhost_data  = host_data  ? NET_Escape (host_data, URL_XALPHAS)  : 0;

	url = (char *)
		XP_ALLOC ((qto         ? XP_STRLEN(qto)			+ 15 : 0) +
				  (qcc         ? XP_STRLEN(qcc)			+ 15 : 0) +
				  (qnewsgroups ? XP_STRLEN(qnewsgroups) + 15 : 0) +
				  (qsubject    ? XP_STRLEN(qsubject)	+ 15 : 0) +
				  (qreferences ? XP_STRLEN(qreferences) + 15 : 0) +
				  (qhost_data  ? XP_STRLEN(qhost_data)  + 15 : 0) +
				  (qattachment ? XP_STRLEN(qattachment) + 15 : 0) +
				  60);
	if (!url) goto FAIL;
	XP_STRCPY (url, "mailto:");
	head = url + XP_STRLEN (url);
	out = head;
# define PUSH_STRING(S) XP_STRCPY(out, S), out += XP_STRLEN(S)
# define PUSH_PARM(prefix,var) \
	if (var) {				   \
		if (out == head)	   \
			*out++ = '?';	   \
		else				   \
			*out++ = '&';	   \
		PUSH_STRING (prefix);  \
		*out++ = '=';		   \
		PUSH_STRING (var);	   \
	}						   \

		PUSH_PARM("to", qto);
		PUSH_PARM("cc", qcc);
		PUSH_PARM("newsgroups", qnewsgroups);
		PUSH_PARM("subject", qsubject);
		PUSH_PARM("references", qreferences);
		PUSH_PARM("attachment", qattachment);
		PUSH_PARM("newshost", qhost_data);
		{
		  char *t = "true";  /* avoid silly compiler warning */
		  HG92725
		  if (sign_p)    PUSH_PARM("sign", t);
		}
# undef PUSH_PARM
# undef PUSH_STRING

FAIL:
		FREEIF (to2);
		FREEIF (cc2);
		FREEIF (qto);
		FREEIF (qcc);
		FREEIF (qnewsgroups);
		FREEIF (qsubject);
		FREEIF (qreferences);
		FREEIF (qattachment);
		FREEIF (qhost_data);

#endif //XP_UNIX
		return url;
}

#ifdef GENERATINGPOWERPC
#pragma global_optimizer off
#endif


MSG_PaneURLChain::MSG_PaneURLChain(MSG_Pane *pane)
{
	m_pane = pane;
}

MSG_PaneURLChain::~MSG_PaneURLChain()
{
}

// override to chain urls. return non-zero to continue.
int	MSG_PaneURLChain::GetNextURL()
{
	return 0;
}

#ifndef MOZ_MAIL_NEWS
/* This is normally in mkpop3.c, of all the odd places!
   But it's required for mail compose.
 */
int
msg_GrowBuffer (uint32 desired_size, uint32 element_size, uint32 quantum,
				char **buffer, uint32 *size)
{
  if (*size <= desired_size)
	{
	  char *new_buf;
	  uint32 increment = desired_size - *size;
	  if (increment < quantum) /* always grow by a minimum of N bytes */
		increment = quantum;

#ifdef TESTFORWIN16
	  if (((*size + increment) * (element_size / sizeof(char))) >= 64000)
		{
		  /* Make sure we don't choke on WIN16 */
		  XP_ASSERT(0);
		  return MK_OUT_OF_MEMORY;
		}
#endif /* DEBUG */

	  new_buf = (*buffer
				 ? (char *) XP_REALLOC (*buffer, (*size + increment)
										* (element_size / sizeof(char)))
				 : (char *) XP_ALLOC ((*size + increment)
									  * (element_size / sizeof(char))));
	  if (! new_buf)
		return MK_OUT_OF_MEMORY;
	  *buffer = new_buf;
	  *size += increment;
	}
  return 0;
}

#endif /* ! MOZ_MAIL_NEWS */

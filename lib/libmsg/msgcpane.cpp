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
#include "dberror.h"

#include "mime.h"
#include "shist.h"
#include "xlate.h"
#include "bkmks.h"
#include "libi18n.h"
#include "xpgetstr.h"

#include "msgsec.h"
#include "msgcpane.h"
#include "msgprefs.h"
#include "msgsec.h"
#include "msgcflds.h"
#include "msgimap.h"
#include "msgurlq.h"
#include "maildb.h"
#include "addrbook.h"
#include "abcom.h"
#include "dirprefs.h"

#include "edt.h" // to invoke save on the html compose pane
#include "mhtmlstm.h"

#include "prefapi.h"
#include "htmldlgs.h"
#include "hosttbl.h"
#include "newshost.h"
#include "xp_qsort.h"
#include "intl_csi.h"

extern "C"
{
extern int MK_MSG_MSG_COMPOSITION;

extern int MK_COMMUNICATIONS_ERROR;
extern int MK_OUT_OF_MEMORY;

extern int MK_MSG_EMPTY_MESSAGE;
extern int MK_MSG_DOUBLE_INCLUDE;

extern int MK_MSG_WHY_QUEUE_SPECIAL;
extern int MK_MSG_WHY_QUEUE_SPECIAL_OLD;
extern int MK_MSG_NOT_AS_SENT_FOLDER;

extern int MK_MSG_MISSING_SUBJECT;

HG65256

extern int MK_MSG_SEND;
extern int MK_MSG_SEND_LATER;
extern int MK_MSG_ATTACH_ETC;
extern int MK_MSG_QUOTE_MESSAGE;
extern int MK_MSG_FROM;
extern int MK_MSG_REPLY_TO;
extern int MK_MSG_MAIL_TO;
extern int MK_MSG_MAIL_CC;
extern int MK_MSG_MAIL_BCC;
extern int MK_MSG_FILE_CC;
extern int MK_MSG_POST_TO;
extern int MK_MSG_FOLLOWUPS_TO;
extern int MK_MSG_SUBJECT;
extern int MK_MSG_ATTACHMENT;
extern int MK_MSG_ATTACH_AS_TEXT;
extern int MK_MSG_SAVE_DRAFT;
extern int MK_ADDR_BOOK_CARD;

extern int MK_MSG_ASK_HTML_MAIL;
extern int MK_MSG_ASK_HTML_MAIL_TITLE;
extern int MK_MSG_HTML_RECIPIENTS;
extern int MK_MSG_HTML_RECIPIENTS_TITLE;
extern int MK_MSG_EVERYONE;

extern int MK_MSG_ENTER_NAME_FOR_TEMPLATE;

#include "xp_help.h"
}



HG33234

#define ALL_HEADERS (MSG_FROM_HEADER_MASK |			\
					 MSG_REPLY_TO_HEADER_MASK |		\
					 MSG_TO_HEADER_MASK |			\
					 MSG_CC_HEADER_MASK |			\
					 MSG_BCC_HEADER_MASK |			\
					 MSG_FCC_HEADER_MASK |			\
					 MSG_NEWSGROUPS_HEADER_MASK |	\
					 MSG_FOLLOWUP_TO_HEADER_MASK |	\
					 MSG_SUBJECT_HEADER_MASK |		\
					 MSG_ATTACHMENTS_HEADER_MASK)


enum RecipientType {
	Address = 1, Domain = 2, Newsgroup = 3, GroupHierarchy = 4
};

class RecipientEntry : public MSG_ZapIt {
public:
	RecipientEntry(const char* name, const char* description,
				   RecipientType type, XP_Bool htmlok);
	~RecipientEntry();

	char* GetName() {return m_name;}
	char* GetDescription() {return m_description;}
	RecipientType GetType() {return m_type;}
	XP_Bool GetHTMLOk() {return m_htmlok;}
	XP_Bool GetNewHTMLOk() {return m_newhtmlok;}
	void SetNewOK(XP_Bool value);
	XP_Bool GetTouched() {return m_touched;}

protected:
	char* m_name;
	char* m_description;
	RecipientType m_type;
	XP_Bool m_htmlok;
	XP_Bool m_newhtmlok;
	XP_Bool m_touched;
};


RecipientEntry::RecipientEntry(const char* name, const char* description,
							   RecipientType type, XP_Bool htmlok)
{
	m_name = XP_STRDUP(name);
	m_description = XP_STRDUP(description);
	if (!m_description) {
		FREEIF(m_name);			// Checking for name being NULL is the hack
								// used to see if we're out of memory.
	}
	m_type = type;
	m_htmlok = htmlok;
}

RecipientEntry::~RecipientEntry()
{
	FREEIF(m_name);
	FREEIF(m_description);
}

void
RecipientEntry::SetNewOK(XP_Bool value)
{
	XP_ASSERT(!m_touched);
	m_touched = TRUE;
	m_newhtmlok = value;
}



class MSG_HTMLRecipients : public MSG_ZapIt {
public:
	MSG_HTMLRecipients();
	~MSG_HTMLRecipients();

	int AddOne(const char* name, const char* description,
			   RecipientType type, XP_Bool htmlok);
	MSG_RecipientList* GetList(XP_Bool htmlok);

	int SetNewList(int32* notoklist, int32* oklist);

	char** GetChangedList(RecipientType type, XP_Bool htmlok);
	void FreeChangedList(char** list);
	int GetNum() {return m_num;}

protected:
	RecipientEntry** m_list;
	int32 m_num;
	int32 m_max;
	MSG_RecipientList* m_generatedList[2];
};

MSG_HTMLRecipients::MSG_HTMLRecipients() {
}

MSG_HTMLRecipients::~MSG_HTMLRecipients() {
	delete m_generatedList[0];
	delete m_generatedList[1];
	for (int32 i=0 ; i<m_num ; i++) {
		delete m_list[i];
	}
	delete [] m_list;
}

int
MSG_HTMLRecipients::AddOne(const char* name, const char* description,
						   RecipientType type, XP_Bool htmlok)
{
	int32 i;
	for (i=0 ; i<m_num ; i++) {
		if (m_list[i]->GetType() == type &&
			XP_STRCMP(m_list[i]->GetName(), name) == 0) return 0;
	}
	if (m_num >= m_max) {
		RecipientEntry** tmp = new RecipientEntry* [m_max + 10];
		if (!tmp) return MK_OUT_OF_MEMORY;
		m_max += 10;
		for (i=0 ; i<m_num ; i++) {
			tmp[i] = m_list[i];
		}
		delete [] m_list;
		m_list = tmp;
	}
	m_list[m_num] = new RecipientEntry(name, description, type, htmlok);
	if (!m_list[m_num]) return MK_OUT_OF_MEMORY;
	if (!m_list[m_num]->GetName()) {
		delete m_list[m_num];
		return MK_OUT_OF_MEMORY;
	}
	m_num++;
	return 0;
}


MSG_RecipientList*
MSG_HTMLRecipients::GetList(XP_Bool htmlok)
{
	int32 i, j;
	if (m_generatedList[0] == NULL) {
		// Sort the entries in the list.  Within a given type, we want to
		// keep things in the order they were generated, but they need to
		// be grouped by type.  So, it's bubble-sort time.  Whee...
		for (i=1 ; i<m_num ; i++) {
			for (j = i;
				 j > 0 && m_list[j]->GetType() < m_list[j-1]->GetType();
				 j--) {
				RecipientEntry* tmp = m_list[j];
				m_list[j] = m_list[j-1];
				m_list[j-1] = tmp;
			}
		}
		
		m_generatedList[0] = new MSG_RecipientList [m_num + 1];
		if (!m_generatedList[0]) return NULL;
		m_generatedList[1] = new MSG_RecipientList [m_num + 1];
		if (!m_generatedList[1]) {
			delete [] m_generatedList[0];
			return NULL;
		}
		int32 cur[2];
		cur[0] = cur[1] = 0;
		for (i=0 ; i<m_num ; i++) {
			int w = int(m_list[i]->GetHTMLOk());
			m_generatedList[w][cur[w]].name = m_list[i]->GetDescription();
			m_generatedList[w][cur[w]].value = i;
			(cur[w])++;
		}
		for (i=0 ; i<2 ; i++) {
			m_generatedList[i][cur[i]].name = NULL;
			m_generatedList[i][cur[i]].value = -1;
		}
	}
	return m_generatedList[int(htmlok)];
}


int
MSG_HTMLRecipients::SetNewList(int32* notoklist, int32* oklist)
{
	int32 i;
#ifdef DEBUG
	for (i=0 ; i<m_num ; i++) {
		XP_ASSERT(!m_list[i]->GetTouched());
	}
#endif
	for (int w=0 ; w<2 ; w++) {
		XP_Bool ok = (w == 1);
		int32* list = ok ? oklist : notoklist;
		XP_ASSERT(list);
		if (!list) continue;
		for ( ; *list >= 0 ; list++) {
			XP_ASSERT(*list < m_num);
			if (*list >= m_num) break;
			m_list[*list]->SetNewOK(ok);
		}
	}
	int status = 0;
	for (i=0 ; i<m_num ; i++) {
		XP_ASSERT(m_list[i]->GetTouched());
		if (!m_list[i]->GetTouched()) {
			status = -1;
		}
	}
	return status;
}


char**
MSG_HTMLRecipients::GetChangedList(RecipientType type, XP_Bool htmlok)
{
	char** result = new char * [m_num + 1];
	if (!result) return NULL;
	char** tmp = result;
	for (int32 i=0 ; i<m_num ; i++) {
		if (m_list[i]->GetType() == type &&
			  m_list[i]->GetNewHTMLOk() == htmlok &&
			  m_list[i]->GetHTMLOk() != htmlok) {
			*tmp = m_list[i]->GetName();
			tmp++;
		}
	}
	*tmp = NULL;
	return result;
}


void
MSG_HTMLRecipients::FreeChangedList(char** list)
{
	delete [] list;
}


static void msg_free_attachment_list(struct MSG_AttachmentData *list);

static void
msg_delete_attached_files(struct MSG_AttachedFile *attachments)
{
	struct MSG_AttachedFile *tmp;
	if (!attachments) return;
	for (tmp = attachments; tmp->orig_url; tmp++) {
		FREEIF(tmp->orig_url);
		FREEIF(tmp->type);
		FREEIF(tmp->real_name);
		FREEIF(tmp->encoding);
		FREEIF(tmp->description);
		FREEIF(tmp->x_mac_type);
		FREEIF(tmp->x_mac_creator);
		if (tmp->file_name) {
			XP_FileRemove(tmp->file_name, xpFileToPost);
			XP_FREE(tmp->file_name);
		}
	}
	XP_FREEIF(attachments);
}


MSG_CompositionPane::MSG_CompositionPane(MWContext* context,
										 MWContext* old_context,
										 MSG_Prefs* prefs,
										 MSG_CompositionFields* fields,
										 MSG_Master* master)
: MSG_Pane(context, master) 
{
	m_prefs = prefs;
	m_htmlaction = MSG_HTMLAskUser;
	Initialize(old_context, fields);
	// make sure we have a valid folder tree - via side effect of getfoldertree
	master->GetFolderTree();
}


MSG_CompositionPane::MSG_CompositionPane(MWContext* context,
										 MSG_Prefs* prefs,
										 MSG_Master* master)
: MSG_Pane(context, master)
{
	m_prefs = prefs;
	// make sure we have a valid folder tree - via side effect of getfoldertree
	master->GetFolderTree();
}


int
MSG_CompositionPane::Initialize(MWContext* old_context,
								MSG_CompositionFields* fields)
{
	m_print = new PrintSetup;
	HG22658
								// ###tw   Should do something if this failed!

#ifndef MOZ_NEWADDR
	// Register with the personal address book
	// so that things like name completion will
	// happen with out the address book closing each time
	DIR_Server* pab = NULL;
	ABook * ab = NULL;

	DIR_GetComposeNameCompletionAddressBook (FE_GetDirServers(), &pab);
	ab = FE_GetAddressBook(this);

	if (pab && ab)
	{
		int errorID = 0;
		if ((errorID = AB_RegisterComposeWindow(ab, pab)) != 0)
			FE_Alert (FE_GetAddressBookContext(NULL, (PRBool)FALSE), XP_GetString (errorID));
	}
#endif

	InitializeHeaders(old_context, fields);
	m_visible_headers = GetInterestingHeaders();
	m_deliver_mode = MSG_DeliverNow;
	m_haveAttachedVcard = FALSE;

	m_fields->SetForcePlainText(FALSE);	// Coming into us, this field meant
										// "bring up the editor in plaintext
										// mode".  Well, that's already been
										// done at this point.  Now, we want
										// it to mean "convert this message
										// to plaintext on send".  Which we
										// do only if DetermineHTMLAction()
										// tells us to.

	return 0;
}



MSG_CompositionPane::~MSG_CompositionPane() {
	// Don't interrupt if there's nothing to interrupt because we might lose
	// mocha messages.
	if (NET_AreThereActiveConnectionsForWindow(m_context))
		msg_InterruptContext (m_context, FALSE);
	if (m_textContext != NULL) {
		msg_InterruptContext(m_textContext, TRUE);
	}

	msg_delete_attached_files (m_attachedFiles);

	FREEIF(m_defaultUrl);
	FREEIF(m_attachmentString);

	msg_free_attachment_list(m_attachData);

	delete m_print;
	m_print = NULL;

	HG28266

	if (m_context) FE_DestroyMailCompositionContext(m_context);
	m_context = NULL;

	delete m_fields;
	m_fields = NULL;
	delete m_initfields;
	m_initfields = NULL;
	delete m_htmlrecip;
	m_htmlrecip = NULL;

	DIR_Server* pab = NULL;
	ABook * ab = NULL;

#ifndef MOZ_NEWADDR
	DIR_GetComposeNameCompletionAddressBook (FE_GetDirServers(), &pab);
	ab = FE_GetAddressBook(this);

#ifdef MOZ_NEWADDR
	XP_ASSERT (pab);
#else
	XP_ASSERT (pab && ab);
#endif

	if (pab && ab)
		AB_UnregisterComposeWindow(ab, pab);
#endif

	FREEIF(m_quotedText);
	FREEIF(m_messageId);
}


MSG_PaneType
MSG_CompositionPane::GetPaneType() {
	return MSG_COMPOSITIONPANE;
}


void MSG_CompositionPane::NotifyPrefsChange(NotifyCode) {
	// ###tw  Write me!
}


HG02070

int MSG_CompositionPane::CreateVcardAttachment ()
{
	DIR_Server* pab;
	char* email;
	char* name;
	PersonEntry person;
	ABID entryID;
	int status = 0;

	if (!m_haveAttachedVcard) 
	{
#ifdef MOZ_NEWADDR
		char * vCard = NULL;
		char * filename = NULL;
		AB_LoadIdentityVCard(&vCard);
		if (vCard)
		{
			AB_ExportVCardToTempFile (vCard, &filename);
			if (vCard)
				XP_FREE(vCard); // free our allocated VCardString...
			char buf [ 2 * kMaxFullNameLength ];
			if (FE_UsersFullName()) 
				name = XP_STRDUP (FE_UsersFullName());
			// write out a content description string
			XP_SPRINTF (buf, XP_GetString (MK_ADDR_BOOK_CARD), name);
			XP_FREEIF(name);
#else
		DIR_GetPersonalAddressBook (FE_GetDirServers(), &pab);

		if (!pab)
			return -1;

		person.Initialize();
		if (FE_UsersMailAddress()) {
			email = XP_STRDUP (FE_UsersMailAddress());
			person.pEmailAddress = email;
		}
		if (FE_UsersFullName()) {
			name = XP_STRDUP (FE_UsersFullName());
			person.pGivenName = name;
		}

		AB_GetEntryIDForPerson(pab,
			FE_GetAddressBook(NULL), &entryID, &person);

		person.CleanUp();

		if (entryID != MSG_MESSAGEIDNONE) {
			char fullname [ kMaxFullNameLength ];
			char buf [ 2 * kMaxFullNameLength ];
			char * filename = NULL;

			AB_ExportToVCardTempFile(FE_GetAddressBook(NULL),
				pab, entryID, &filename);

			AB_GetFullName(pab,	FE_GetAddressBook(NULL), entryID, fullname);

			XP_SPRINTF (buf, XP_GetString (MK_ADDR_BOOK_CARD), fullname);
#endif

			char* temp = WH_FileName(filename, xpFileToPost);
			char * fileurl = NULL;
			if (temp)
			{
				fileurl = XP_PlatformFileToURL (temp);
				XP_FREE(temp);
			}
			else
				return -1;	

			// Send the vCard out with a filename which distinguishes this user. e.g. jsmith.vcf
			// The main reason to do this is for interop with Eudora, which saves off 
			// the attachments separately from the message body
			char *vCardFileName = NULL;
			const char *mailServerUserName = NET_GetPopUsername();
			if (mailServerUserName)
				vCardFileName = PR_smprintf ("%s.vcf", mailServerUserName);
			if (!vCardFileName)
			{
				vCardFileName = XP_STRDUP("vcard.vcf");
				if (!vCardFileName)
					return MK_OUT_OF_MEMORY;
			}

			char * origurl = XP_PlatformFileToURL (vCardFileName);
			int datacount = 0, filecount = 0;
			for (MSG_AttachmentData *tmp1 = m_attachData; tmp1 && tmp1->url; tmp1++) datacount++;
			for (MSG_AttachedFile *tmp = m_attachedFiles; tmp && tmp->orig_url; tmp++) filecount++;

			MSG_AttachmentData *alist;
			if (datacount) {
				alist = (MSG_AttachmentData *)
				XP_REALLOC(m_attachData, (datacount + 2) * sizeof(MSG_AttachmentData));
			}
			else {
				alist = (MSG_AttachmentData *)
					XP_ALLOC((datacount + 2) * sizeof(MSG_AttachmentData));
			}
			if (!alist)
				return MK_OUT_OF_MEMORY;
			m_attachData = alist;
			XP_MEMSET (m_attachData + datacount, 0, 2 * sizeof (MSG_AttachmentData));
			m_attachData[datacount].url = fileurl;
			m_attachData[datacount].real_type = XP_STRDUP(vCardMimeFormat);
			m_attachData[datacount].description = XP_STRDUP (buf);
			m_attachData[datacount].real_name = XP_STRDUP (vCardFileName);
			m_attachData[datacount + 1].url = NULL;
			
			MSG_AttachedFile *aflist;
			if (filecount) {
				aflist = (struct MSG_AttachedFile *)
				XP_REALLOC(m_attachedFiles, (filecount + 2) * sizeof(MSG_AttachedFile));
			}
			else {
				aflist = (struct MSG_AttachedFile *)
					XP_ALLOC((filecount + 2) * sizeof(MSG_AttachedFile));
			}

			if (!aflist)
				return MK_OUT_OF_MEMORY;

			m_attachedFiles = aflist;
			XP_MEMSET (m_attachedFiles + filecount, 0, 2 * sizeof (MSG_AttachedFile));
			m_attachedFiles[filecount].orig_url = origurl;
			m_attachedFiles[filecount].file_name = filename;
			m_attachedFiles[filecount].type = XP_STRDUP(vCardMimeFormat);
			m_attachedFiles[filecount].description = XP_STRDUP (buf);
			m_attachedFiles[filecount].real_name = XP_STRDUP (vCardFileName);
			m_attachedFiles[filecount + 1].orig_url = NULL;

			m_haveAttachedVcard = TRUE;

			XP_FREE(vCardFileName);
		}
	}
	return status;
}


char*
MSG_CompositionPane::FigureBcc(XP_Bool newsBcc)
{
	ABook* addressbook;
	char* result = NULL;
	FREEIF(result);
	const char* tmp =
		GetPrefs()->GetDefaultHeaderContents(
			newsBcc ? MSG_NEWS_BCC_HEADER_MASK : MSG_BCC_HEADER_MASK);
	if (!GetPrefs()->GetDefaultBccSelf(newsBcc)) {
		result = XP_STRDUP(tmp ? tmp : "");
	} else if (!tmp || !*tmp) {
		result = XP_STRDUP(FE_UsersMailAddress());
	} else {
		result = PR_smprintf("%s, %s", FE_UsersMailAddress(), tmp);
	}
	if (result) {
		addressbook = FE_GetAddressBook(this);
		if (addressbook) {
			char* t = AB_ExpandHeaderString(addressbook, result,
											FALSE);
			if (t) {
				XP_FREE(result);
				result = t;
			}
		}
	}
	return result;
}


const char* 
MSG_CompositionPane::CheckForLosingFcc(const char* fcc)
{
	if (fcc && *fcc) {
		char *q = GetPrefs()->MagicFolderName(MSG_FOLDER_FLAG_QUEUE);
		if (q && *q && !XP_FILENAMECMP (q, fcc)) {
			char *buf;

			/* We cannot allow them to use the queued mail folder
			   as their fcc folder, too.  Tell them why. */

			const char *q = MSG_GetQueueFolderName();
			if (q) {
				if (!strcasecomp(q,QUEUE_FOLDER_NAME_OLD))
					buf = PR_smprintf("%s%s", XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL_OLD),
												  XP_GetString(MK_MSG_NOT_AS_SENT_FOLDER));
				else buf = PR_smprintf("%s%s", XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL),
								  XP_GetString(MK_MSG_NOT_AS_SENT_FOLDER));
			}
			else buf = PR_smprintf("%s%s", XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL),
							  XP_GetString(MK_MSG_NOT_AS_SENT_FOLDER));
			if (buf) {
				FE_Alert(m_context, buf);
				XP_FREE(buf);
			}

			/* Now ignore the FCC file they passed in. */
			fcc = 0;
		}
		FREEIF(q);
	}
	return fcc;
}

MsgERR
MSG_CompositionPane::GetCommandStatus(MSG_CommandType command,
										 const MSG_ViewIndex* indices,
										 int32 numindices,
						   XP_Bool *selectable_pP,
						   MSG_COMMAND_CHECK_STATE *selected_pP,
						   const char **display_stringP,
						   XP_Bool *plural_pP)
{
	const char *display_string = 0;
	XP_Bool plural_p = FALSE;
	// N.B. default is TRUE, so you don't need to set it in each case
	XP_Bool selectable_p = TRUE;	
	XP_Bool selected_p = FALSE;
	XP_Bool selected_used_p = FALSE;

	switch (command)
	{
	case MSG_AttachAsText:
		// the WinFE uses this for lots of update, so pretend we handle it.
		display_string = XP_GetString(MK_MSG_ATTACH_AS_TEXT);
		break;
	case MSG_SendMessage:
		display_string = XP_GetString(MK_MSG_SEND);
        if (m_attachmentInProgress)
            selectable_p = FALSE;
		break;
	case MSG_SendMessageLater:
		display_string = XP_GetString(MK_MSG_SEND_LATER);
        if (m_attachmentInProgress)
            selectable_p = FALSE;
		break;
	case MSG_SaveDraft:
	case MSG_SaveDraftThenClose:
	case MSG_SaveTemplate:
	    display_string = XP_GetString(MK_MSG_SAVE_DRAFT);
        if (m_attachmentInProgress)
            selectable_p = FALSE;
		break;
	case MSG_Attach:
		display_string = XP_GetString(MK_MSG_ATTACH_ETC);
		break;

	case MSG_ShowFrom:
		display_string = XP_GetString(MK_MSG_FROM);
		selected_p = ShowingCompositionHeader(MSG_FROM_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowReplyTo:
		display_string = XP_GetString(MK_MSG_REPLY_TO);
		selected_p = ShowingCompositionHeader(MSG_REPLY_TO_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowTo:
		display_string = XP_GetString(MK_MSG_MAIL_TO);
		selected_p = ShowingCompositionHeader(MSG_TO_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowCC:
		display_string = XP_GetString(MK_MSG_MAIL_CC);
		selected_p = ShowingCompositionHeader(MSG_CC_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowBCC:
		display_string = XP_GetString(MK_MSG_MAIL_BCC);
		selected_p = ShowingCompositionHeader(MSG_BCC_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowFCC:
		display_string = XP_GetString(MK_MSG_FILE_CC);
		selected_p = ShowingCompositionHeader(MSG_FCC_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowPostTo:
		display_string = XP_GetString(MK_MSG_POST_TO);
		selected_p = ShowingCompositionHeader(MSG_NEWSGROUPS_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowFollowupTo:
		display_string = XP_GetString(MK_MSG_FOLLOWUPS_TO);
		selected_p = ShowingCompositionHeader(MSG_FOLLOWUP_TO_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowSubject:
		display_string = XP_GetString(MK_MSG_SUBJECT);
		selected_p = ShowingCompositionHeader(MSG_SUBJECT_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	case MSG_ShowAttachments:
		display_string = XP_GetString(MK_MSG_ATTACHMENT);
		selected_p = ShowingCompositionHeader(MSG_ATTACHMENTS_HEADER_MASK);
		selected_used_p = TRUE;
		break;
	default:
		selectable_p = FALSE;
		return MSG_Pane::GetCommandStatus(command, indices, numindices,
			selectable_pP, selected_pP, display_stringP, plural_pP);
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

					 
MsgERR
MSG_CompositionPane::DoCommand(MSG_CommandType command, MSG_ViewIndex* indices,
							   int32 numindices)
{
	MsgERR status = 0;
	InterruptContext(FALSE);
	switch (command) {
	case MSG_SendMessage:
		status = SendMessageNow();	/* ###tw Error-return-type mismatch! */
		break;
	case MSG_SendMessageLater:
		status = QueueMessageForLater();/* ###tw Error-return-type mismatch! */
		break;
    case MSG_SaveDraft:
	case MSG_SaveDraftThenClose:
		if (command == MSG_SaveDraftThenClose)
			m_closeAfterSave = TRUE;
		status = SaveMessageAsDraft(); /* ### Error-return-type mismatch! */
		break;
	case MSG_SaveTemplate:
		status = SaveMessageAsTemplate();
		break;
	case MSG_ShowPostTo:		// how to do this?
		ToggleCompositionHeader(MSG_NEWSGROUPS_HEADER_MASK);
		break;
	HG82762
	case MSG_ShowFrom:
		ToggleCompositionHeader(MSG_FROM_HEADER_MASK);
		break;
	case MSG_ShowReplyTo:
		ToggleCompositionHeader(MSG_REPLY_TO_HEADER_MASK);
		break;
	case MSG_ShowTo:
		ToggleCompositionHeader(MSG_TO_HEADER_MASK);
		break;
	case MSG_ShowCC:
		ToggleCompositionHeader(MSG_CC_HEADER_MASK);
		break;
	case MSG_ShowBCC:
		ToggleCompositionHeader(MSG_BCC_HEADER_MASK);
		break;
	case MSG_ShowFCC:
		ToggleCompositionHeader(MSG_FCC_HEADER_MASK);
		break;
	case MSG_ShowFollowupTo:
		ToggleCompositionHeader(MSG_FOLLOWUP_TO_HEADER_MASK);
		break;
	case MSG_ShowSubject:
		ToggleCompositionHeader(MSG_SUBJECT_HEADER_MASK);
		break;
	case MSG_ShowAttachments:
		ToggleCompositionHeader(MSG_ATTACHMENTS_HEADER_MASK);
		break;
	default:
		status = MSG_Pane::DoCommand(command, indices, numindices);
		break;
	}
	return status;
}

extern "C" void FE_MsgShowHeaders(MSG_Pane *pPane, MSG_HEADER_SET mhsHeaders);

void MSG_CompositionPane::ToggleCompositionHeader(uint32 header)
{
  if (m_visible_headers & header) {
	m_visible_headers &= ~header;
  } else {
	m_visible_headers |= header;
  }
  FE_MsgShowHeaders(this, m_visible_headers);
}

XP_Bool 
MSG_CompositionPane::ShowingAllCompositionHeaders()
{
  return m_visible_headers == ALL_HEADERS;
}

XP_Bool 
MSG_CompositionPane::ShowingCompositionHeader(uint32 mask)
{
  return (m_visible_headers & mask) == mask;
}


int
MSG_CompositionPane::SetCallbacks(MSG_CompositionPaneCallbacks* callbacks,
								  void* closure)
{
	m_callbacks = *callbacks;
	m_callbackclosure = closure;
	return 0;
}


void
MSG_CompositionPane::InitializeHeaders(MWContext* old_context,
									   MSG_CompositionFields* fields)
{
	XP_ASSERT(m_fields == NULL);
	XP_ASSERT(m_initfields == NULL);

	const char *real_addr = FE_UsersMailAddress ();
	char *real_return_address;
	const char* sig;
	XP_Bool forward_quoted;
	forward_quoted = FALSE;

	m_fields = new MSG_CompositionFields(fields);
								// ###tw  Should check for failure!

	/* hack for forward quoted.  Checks the attachment field for a cookie
	   string indicating that this is a forward quoted operation.  If a cookie
	   is found, the attachment string is slid back down over the cookie.  This
	   will put the original string back in tact. */

	const char* attachment = m_fields->GetAttachments();

	if (attachment) {
		if (!XP_STRNCMP(attachment, MSG_FORWARD_COOKIE,
						strlen(MSG_FORWARD_COOKIE))) {
			attachment += XP_STRLEN(MSG_FORWARD_COOKIE);
			forward_quoted = TRUE;      /* set forward with quote flag */
			m_fields->SetAttachments(attachment);
			attachment = m_fields->GetAttachments();
		}
	}

	m_status = -1;

	if (MISC_ValidateReturnAddress(old_context, real_addr) < 0) {
		return;
	}

	real_return_address = MIME_MakeFromField();

	XP_ASSERT (m_context->type == MWContextMessageComposition);
	XP_ASSERT (XP_FindContextOfType(0, MWContextMessageComposition));
	XP_ASSERT (!m_context->msg_cframe);

	int32 count = m_fields->GetNumForwardURL();
	if (count > 0) {
		// if forwarding one or more messages
		XP_ASSERT(*attachment == '\0');
		MSG_AttachmentData *alist = (struct MSG_AttachmentData *)
			XP_ALLOC((count + 1) * sizeof(MSG_AttachmentData));
		if (alist) {
			XP_MEMSET(alist, 0, (count + 1) * sizeof(*alist));
			for (count--; count >= 0; count--) {
				alist[count].url = (char*) m_fields->GetForwardURL(count);
				alist[count].real_name = (char*) m_fields->GetForwardURL(count);
			}
			SetAttachmentList(alist);
			XP_FREE(alist);
		}
	} else if (*attachment) {
		// forwarding a single url
		// typically a web page
		MSG_AttachmentData *alist;
		count = 1;
		alist = (struct MSG_AttachmentData *)
			XP_ALLOC((count + 1) * sizeof(MSG_AttachmentData));
		if (alist) {
			XP_MEMSET(alist, 0, (count + 1) * sizeof(*alist));
			alist[0].url = (char *)attachment;
			alist[0].real_name = XP_STRDUP ((char *)attachment);
			SetAttachmentList(alist);
		}
	}	// else if (*attachment)

	if (*attachment) {
		if (*attachment != '(') {
			m_defaultUrl = XP_STRDUP(attachment);
		}
	}
	else if (old_context) {
		History_entry *h = SHIST_GetCurrent(&old_context->hist);
		if (h && h->address) {
			m_defaultUrl = XP_STRDUP(h->address);
		}
	}

	if (!*m_fields->GetFrom()) {
		m_fields->SetFrom(real_return_address);
	}

	/* Guess what kind of reply this is based on the headers we passed in.
	 */

	const char* newsgroups = m_fields->GetNewsgroups();
	const char* to = m_fields->GetTo();
	const char* cc = m_fields->GetCc();
	const char* references = m_fields->GetReferences();

	if (count > 0 || *attachment) {
		/* if an attachment exists and the forward_quoted flag is set, this
		   is a forward quoted operation. */
		if (forward_quoted) {
			m_replyType = MSG_ForwardMessageQuoted;
			/* clear out the attachment list for forward quoted messages. */
			SetAttachmentList(NULL);
			m_pendingAttachmentsCount = 0;
		} else {
			m_replyType = MSG_ForwardMessageAttachment;
		}
	} else if (*references && *newsgroups && (*to || *cc)) {
		m_replyType = MSG_PostAndMailReply;
	} else if (*references && *newsgroups) {
		m_replyType = MSG_PostReply;
	} else if (*references && *cc) {
		m_replyType = MSG_ReplyToAll;
	} else if (*references && *to) {
		m_replyType = MSG_ReplyToSender;
	} else if (*newsgroups) {
		m_replyType = MSG_PostNew;
	} else {
		m_replyType = MSG_MailNew;
	}


HG00729


	if (!*m_fields->GetOrganization()) {
		m_fields->SetOrganization(FE_UsersOrganization());
	}

	if (!*m_fields->GetReplyTo()) {
		m_fields->
			SetReplyTo(GetPrefs()->
					   GetDefaultHeaderContents(MSG_REPLY_TO_HEADER_MASK));
	}
	if (!*m_fields->GetFcc()) 
	{
		XP_Bool useDefaultFcc = TRUE;
		/*int prefError =*/ PREF_GetBoolPref(*newsgroups ? "news.use_fcc" : "mail.use_fcc",
											&useDefaultFcc);
		if (useDefaultFcc)
		{
			m_fields->SetFcc(GetPrefs()->
				GetDefaultHeaderContents(*newsgroups ? 
					 MSG_NEWS_FCC_HEADER_MASK : MSG_FCC_HEADER_MASK));
		}
	}
	if (!*m_fields->GetBcc()) {
		char* bcc = FigureBcc(*newsgroups);
		m_fields->SetBcc(bcc);
		FREEIF(bcc);
	}

	m_fields->SetFcc(CheckForLosingFcc(m_fields->GetFcc()));

	{
	  const char *body = m_fields->GetDefaultBody();
	  if (body && *body)
		{
		  m_fields->AppendBody(body);
		  m_fields->AppendBody(LINEBREAK);
		  /* m_bodyEdited = TRUE; */
		}
	}

	HG93653
	sig = FE_UsersSignature ();
	if (sig && *sig) {
	    m_fields->AppendBody(LINEBREAK);
		/* If the sig doesn't begin with "--" followed by whitespace or a
		   newline, insert "-- \n" (the pseudo-standard sig delimiter.) */
		if (sig[0] != '-' || sig[1] != '-' ||
			(sig[2] != ' ' && sig[2] != CR && sig[2] != LF)) {
			m_fields->AppendBody("-- " LINEBREAK);
		}
		m_fields->AppendBody(sig);
	}

	FREEIF (real_return_address);


	FE_SetDocTitle(m_context, (char*) GetWindowTitle());


	m_initfields = new MSG_CompositionFields(m_fields);
								// ###tw  Should check for failure!

}


XP_Bool MSG_CompositionPane::ShouldAutoQuote() {
      if (m_haveQuoted) return FALSE;
	if (m_replyType == MSG_ForwardMessageQuoted ||
		    GetPrefs()->GetAutoQuoteReply()) {
		switch (m_replyType) {
		case MSG_ForwardMessageQuoted:
		case MSG_PostAndMailReply:
		case MSG_PostReply:
		case MSG_ReplyToAll:
		case MSG_ReplyToSender:
			return TRUE;

        default:
            break;
		}
	}
	return FALSE;
}




const char* MSG_CompositionPane::GetDefaultURL() {
  return m_defaultUrl;
}



MSG_CompositionFields*
MSG_CompositionPane::GetInitialFields()
{
	return m_initfields;
}




#define ALL_HEADERS (MSG_FROM_HEADER_MASK |			\
					 MSG_REPLY_TO_HEADER_MASK |		\
					 MSG_TO_HEADER_MASK |			\
					 MSG_CC_HEADER_MASK |			\
					 MSG_BCC_HEADER_MASK |			\
					 MSG_FCC_HEADER_MASK |			\
					 MSG_NEWSGROUPS_HEADER_MASK |	\
					 MSG_FOLLOWUP_TO_HEADER_MASK |	\
					 MSG_SUBJECT_HEADER_MASK |		\
					 MSG_ATTACHMENTS_HEADER_MASK)


MSG_HEADER_SET MSG_CompositionPane::GetInterestingHeaders()
{
	MSG_HEADER_SET desired_mask = 0;
	/* The FE has requested the list of "interesting" header fields.
	   The logic here is a bit complicated, in the interest of DWIMity.
	   */

	/* Cc, Subject, and Attachments are always interesting.
	 */
	desired_mask |= (MSG_CC_HEADER_MASK |
					 MSG_SUBJECT_HEADER_MASK /* |
					 MSG_ATTACHMENTS_HEADER_MASK */);

	/* To is interesting if:
	   - it is non-empty, or
	   - this composition window was brought up with a "mail sending"
	   command (Mail New, Reply-*, Forward, or Post and Mail).
	   */
	if (*m_fields->GetTo() ||
		m_replyType == MSG_MailNew ||
		m_replyType == MSG_ReplyToSender ||
		m_replyType == MSG_ReplyToAll ||
		m_replyType == MSG_PostAndMailReply ||
		m_replyType == MSG_ForwardMessageAttachment ||
		m_replyType == MSG_ForwardMessageQuoted)
		desired_mask |= MSG_TO_HEADER_MASK;

	/* CC is interesting if:
	   - it is non-empty, or
	   - this composition window was brought up as a reply to another
	   mail message.  (Should mail-and-post do this too?)
	   */
	if ((*m_fields->GetCc()) ||
		m_replyType == MSG_ReplyToSender ||
		m_replyType == MSG_ReplyToAll)
		desired_mask |= MSG_CC_HEADER_MASK;

	/* Reply-To and BCC are interesting if:
	   - they are non-empty, AND
	   - they are different from the default value
	   (meaning the user has edited them this session.)
	   */
	const char* reply_to = m_fields->GetReplyTo();
	const char* default_reply_to =
		GetPrefs()->GetDefaultHeaderContents(MSG_REPLY_TO_HEADER_MASK);
	if (reply_to && *reply_to &&
		((default_reply_to && *default_reply_to)
		 ? !!XP_STRCMP (reply_to, default_reply_to) : TRUE))
		desired_mask |= MSG_REPLY_TO_HEADER_MASK;

	/* (see above.) */
	const char* bcc = m_fields->GetBcc();
	const char* default_bcc =
		GetPrefs()->GetDefaultHeaderContents(MSG_BCC_HEADER_MASK);
	if (bcc && *bcc &&
		((default_bcc && *default_bcc)
		 ? !!XP_STRCMP (bcc, default_bcc) : TRUE))
		desired_mask |= MSG_BCC_HEADER_MASK;

	/* FCC is never interesting.
	 */

	/* Newsgroups is interesting if:
	   - it is non-empty, or
	   - this composition window was brought up with a "news posting"
	   command (Post New, Post Reply, or Post and Mail).
	   */

	const char* newsgroups = m_fields->GetNewsgroups();
	if ((newsgroups && *newsgroups) ||
		m_replyType == MSG_PostNew ||
		m_replyType == MSG_PostReply ||
		m_replyType == MSG_PostAndMailReply)
		desired_mask |= MSG_NEWSGROUPS_HEADER_MASK;

	/* Followup-To is interesting if:
	   - it is non-empty, AND
	   - it differs from the Newsgroups field.
	   */
	const char* followup_to = m_fields->GetFollowupTo();
	if (followup_to && *followup_to &&
		(newsgroups ? XP_STRCMP (followup_to, newsgroups) : TRUE))
		desired_mask |= MSG_FOLLOWUP_TO_HEADER_MASK;

	return desired_mask;
}


void
MSG_CompositionPane::GetUrlDone_S(PrintSetup* pptr)
{
  ((MSG_CompositionPane*) (pptr->carg))->GetUrlDone(pptr);
}


#define QUOTE_BUFFER_SIZE		10240

void
MSG_CompositionPane::GetUrlDone(PrintSetup* /*pptr*/)
{
	XP_File file;
	FREEIF(m_quoteUrl);
	m_textContext = NULL;  /* since this is called as a result of
							  TXFE_AllConnectionsComplete, we know this context
							  is going away by natural means */
	int bufSize = QUOTE_BUFFER_SIZE;

	XP_FileClose(m_print->out);

	XP_StatStruct stat;
	char* curquote = NULL;
	int32 replyOnTop = 0, replyWithExtraLines = 0;

	PREF_GetIntPref("mailnews.reply_on_top", &replyOnTop);
	PREF_GetIntPref("mailnews.reply_with_extra_lines", &replyWithExtraLines);

	int32 extra = (m_markup ? 0 : 
	               (replyWithExtraLines ? LINEBREAK_LEN * replyWithExtraLines
					: 0));

	if (XP_Stat(m_print->filename, &stat, xpTemporary) == 0) {
		m_quotedText = (char*) XP_ALLOC(stat.st_size + 1 + extra);
		
		/* Insert two line break at the begining of the quoted text */
		if (!m_quotedText) return;

		curquote = m_quotedText;

		if (!m_markup && extra && replyOnTop == 1) {
		  for (; replyWithExtraLines > 0; replyWithExtraLines--) {
			XP_STRCPY(curquote, LINEBREAK);
			curquote += LINEBREAK_LEN;
			if (m_quotefunc)
				(*m_quotefunc)(m_quoteclosure, LINEBREAK);
		  }
		}
	}

	/* Open hateful temporary file as input  */
	file = XP_FileOpen (m_print->filename, xpTemporary, XP_FILE_READ);
	if (file) {
		char* buf = NULL;
		while (!buf && (bufSize >= 512))
		{
			buf = (char*)XP_ALLOC(bufSize + 1);
			if (!buf)
				bufSize /= 2;
		}
		if (buf) {
			int32 bufferLen;
			CCCDataObject conv;
			int doConv;
			INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_context);
			int16 win_csid = INTL_GetCSIWinCSID(c);

			/*
			 * We aren't actually converting character encodings here.
			 * (Note that both the "from" and "to" are the win_csid.)
			 * This makes it call a special routine that makes sure we
			 * deal with whole multibyte characters instead of partial
			 * ones that happen to lie on the boundary of the buffer. -- erik
			 */
			conv = INTL_CreateCharCodeConverter();
			if (conv) {
				doConv = INTL_GetCharCodeConverter(win_csid, win_csid, conv);
			} else {
				doConv = 0;
			}

			while (0 < (bufferLen = XP_FileRead(buf, bufSize, file))) {
				char *newBuf;
				buf[bufferLen] = '\0';
				if (doConv) {
					newBuf = (char *)
						INTL_CallCharCodeConverter(conv,
												   (unsigned char *) buf,
												   bufferLen);
					if (!newBuf) {
						newBuf = buf;
					}
				} else {
					newBuf = buf;
				}
				if (m_quotefunc) {
					(*m_quotefunc)(m_quoteclosure, newBuf);
				}

				if (m_quotedText && curquote) {
					XP_ASSERT(curquote + bufferLen <= m_quotedText + stat.st_size + extra);
					if (curquote + bufferLen <= m_quotedText + stat.st_size + extra) {
						XP_STRCPY(curquote, newBuf);
						curquote += bufferLen;
					}
				}

				if (newBuf != buf) {
					XP_FREE(newBuf);
				}
			}

			if (!m_markup && extra && replyOnTop == 0) {
			  for (; replyWithExtraLines > 1; replyWithExtraLines--) {
				XP_STRCPY(curquote, LINEBREAK);
				curquote += LINEBREAK_LEN;
				if (m_quotefunc)
					(*m_quotefunc)(m_quoteclosure, LINEBREAK);
			  }
			}

			XP_FREE(buf);
			if (conv) {
				INTL_DestroyCharCodeConverter(conv);
			}
		}
		XP_FileClose(file);
	}
	if (curquote) *curquote = '\0';
	m_cited = TRUE;
	XP_FileRemove(m_print->filename, xpTemporary);
	FREEIF(m_print->filename);
	if (m_exitQuoting) {
		(*m_exitQuoting)(m_dummyUrl, 0, m_context);
		m_exitQuoting = NULL;
		m_dummyUrl = NULL;

		/* hack that manages to get the cursor back to normal. */
		NET_SilentInterruptWindow(m_context);
	}
	if (m_quotefunc) {
		(*m_quotefunc)(m_quoteclosure, NULL);
		m_quotefunc = NULL;
	}


	/* Re-enable the UI. */
	FE_UpdateCompToolbar (this);
}

  

class QuotePlainIntoHTML : public MSG_ZapIt {
public:
	QuotePlainIntoHTML(MWContext* context);
	~QuotePlainIntoHTML();

	int DoQuote(const char* data);
	static int32 QuoteLine_s(char* line, uint32 line_length, void* closure);
	int32 QuoteLine(char* line, uint32 line_length);
protected:
	MWContext* m_context;
	char* m_buffer;
	uint32 m_size;
	uint32 m_fp;
	XP_Bool m_insertedpre;
	char* m_outbuf;
	int32 m_outbufsize;
	int m_maxLineWidth;
	int32 m_replyOnTop;
	int32 m_replyWithExtraLines;
};


static int
MyQuoteFunc(void* closure, const char* data)
{
	return ((QuotePlainIntoHTML*) closure)->DoQuote(data);
}





QuotePlainIntoHTML::QuotePlainIntoHTML(MWContext* context)
{
	m_context = context;
	if (EDT_PasteQuoteBegin(m_context, TRUE) != EDT_COP_OK) {
		m_context = NULL;
	}
	PREF_GetIntPref("mailnews.reply_on_top", &m_replyOnTop);
	PREF_GetIntPref("mailnews.reply_with_extra_lines", &m_replyWithExtraLines);
}

QuotePlainIntoHTML::~QuotePlainIntoHTML()
{
	FREEIF(m_buffer);
	delete [] m_outbuf;
}




int
QuotePlainIntoHTML::DoQuote(const char* data)
{
	if (data) {
		if (!m_context) return 0;
		return msg_LineBuffer(data, XP_STRLEN(data), &m_buffer, &m_size, &m_fp, FALSE, 
#ifdef XP_OS2
					(int32 (_Optlink*) (char*,uint32,void*))
#endif
					QuoteLine_s, this);

	} else {
		if (m_context) {
			if (m_fp > 0) {
				QuoteLine(m_buffer, m_fp);
			}
			if (m_insertedpre) {
				EDT_PasteQuote(m_context, "</PRE></BLOCKQUOTE>");
			}
			if ( 0 == m_replyOnTop && m_replyWithExtraLines)
			  for (;m_replyWithExtraLines > 0; m_replyWithExtraLines--)
					EDT_PasteQuote(m_context, "<BR>");
			EDT_PasteQuoteEnd(m_context);

			MSG_CompositionPane *cpane = (MSG_CompositionPane *)
				MSG_FindPane(m_context, MSG_COMPOSITIONPANE);
			if (cpane)
				cpane->SetLineWidth(m_maxLineWidth);
		}
		delete this;
		return 0;
	}
}


int32
QuotePlainIntoHTML::QuoteLine_s(char* line, uint32 length, void* closure)
{
	return ((QuotePlainIntoHTML*)closure)->QuoteLine(line, length);
}

int32
QuotePlainIntoHTML::QuoteLine(char* line, uint32 length)
{
	if (length > m_maxLineWidth)
		m_maxLineWidth = length;

	if (length >= 2 && line[0] == '>' && line[1] == ' ') {
		line += 2;
		length -= 2;
		if (!m_insertedpre) {
			EDT_PasteQuote(m_context, "<BLOCKQUOTE TYPE=CITE><PRE NSCISAW>");
			m_insertedpre = TRUE;
		}
	}
	else if (!m_insertedpre) {
		if (1 == m_replyOnTop && m_replyWithExtraLines)
		  for (; m_replyWithExtraLines > 0; m_replyWithExtraLines--)
				EDT_PasteQuote(m_context, "<BR>");
	}
		
	int l = length * 2 + 50;
	if (l > m_outbufsize) {
		if (l < 512) l = 512;
		m_outbufsize = l;
		delete [] m_outbuf;
		m_outbuf = new char [m_outbufsize];
	}
	if (m_outbuf) {
		*m_outbuf = '\0';
		NET_ScanForURLs(NULL, line, length, m_outbuf, m_outbufsize, TRUE);
		EDT_PasteQuote(m_context, m_outbuf);
	}
	return 0;
}


void
MSG_CompositionPane::QuoteHTMLDone_S(URL_Struct* url, int /*status*/, MWContext* /*context*/)
{
      MSG_CompositionPane *pane = (MSG_CompositionPane *) url->fe_data;
      if (pane) {
          FREEIF(pane->m_quoteUrl);
		  pane->m_quotefunc = NULL;
		  if (pane->m_quoteclosure) {
			  delete pane->m_quoteclosure;
			  pane->m_quoteclosure = NULL;
		  }
	  }
      NET_FreeURLStruct(url);
}



MsgERR MSG_CompositionPane::QuoteMessage(int (*func)(void* closure,
													 const char* data),
										 void* closure)
{
	MsgERR status = 0;
	char* ptr;
	 m_haveQuoted = TRUE;
	if (!m_defaultUrl) return 0; /* Nothing to quote. */
	if (m_quoteUrl) return 0;	/* Currently already quoting! */

	XP_ASSERT(m_quotefunc == NULL);

	if (m_markup) {
		func = MyQuoteFunc;
		closure = new QuotePlainIntoHTML(GetContext());
		if (!closure) return MK_OUT_OF_MEMORY;
	}

	const char* htmlpart = m_fields->GetHTMLPart();
	XP_Bool quotehtml = (m_markup && htmlpart != NULL && *htmlpart != '\0');

	if (m_quotedText) {
		if (func) {
#ifdef	EXTRA_QUOTE_BEGIN
			if (m_markup) {
				if (EDT_PasteQuoteBegin(GetContext(),
										TRUE) != EDT_COP_OK) {
					return eUNKNOWN;
				}
			}
#endif
			(*func)(closure, m_quotedText);
			(*func)(closure, NULL);
		}
		return 0;
	}


	m_quotefunc = func;
	m_quoteclosure = closure;
  

	m_quoteUrl = XP_STRDUP(m_defaultUrl);
	if (!m_quoteUrl) return eOUT_OF_MEMORY;

	/* remove any position information from the url
	 */
	ptr = XP_STRCHR(m_quoteUrl, '#');
	if (ptr) *ptr = '\0';

	if (quotehtml) {
		URL_Struct* url = NET_CreateURLStruct(m_quoteUrl, NET_DONT_RELOAD);
		if (!url) return MK_OUT_OF_MEMORY;

		// This is a hack, really should be url->msg_pane, but this prevents mail
		// quoting from working at all.  We just need SOME sort of way to give
		// the msg_page to the completion function.
		url->fe_data = (void *)this;

		// Set this because when quoting messages that might have been
		// downloaded through IMAP MIME Parts on Demand (with some parts left out)
		// we think it is OK to only quote the inline parts that have been
		// downloaded.
		// (That is, we don't think it's necessary to re-download the entire message
		// from the server.  We might be wrong about this, but so far haven't seen
		// any examples to the contrary.)
		url->allow_content_change = TRUE;
		MSG_UrlQueue::AddUrlToPane (url, QuoteHTMLDone_S, this, TRUE, FO_QUOTE_HTML_MESSAGE);
		return 0;
	}
		

	XL_InitializeTextSetup(m_print);
	m_print->out = NULL;
	m_print->prefix = "> ";
	if (m_markup) {
		if (htmlpart && *htmlpart) {
			// Quoting html into html
			// We are kind of doomed on this case when force sending plain text
			// if there are hard line breaks.
			m_print->width = 999;
		}
		else {
			// Quoting plain text into html; there are hard line breaks
			// We should not reformat the line when force sending plain text
			// message
			// This is done via recording the max line width when we quote each plain
			// text line and then use the max line width if it is greater than default
			// wraplinewidth. This prevent from reformatting the qutoed line in an 
			// unwanted fashion.
			m_print->width = 998;
		}

		// m_print->width = 999;	// Cheap hack.  The EDT_PasteQuote routine
								// generates HTML that is better at wrapping
								// than the TextFE is, so try and let it do the
								// wrapping instead.
	}
	else {
		if (htmlpart && *htmlpart) {
			// We are quoting html message into plain text message
			// Use wrapline width from preference
			int32 width = 72;
			PREF_GetIntPref("mailnews.wraplength", &width);
			if (width == 0) width = 72;
			else if (width < 10) width = 10;
			else if (width > 30000) width = 30000;
			m_print->width = width - 2;
		}
		else {
			// We are quoting plain text message.to plain text
			// We shouldn't reformat the original message since everyline already
			// has hard line break <CR><LF> we simply set the m_print->width to 999.
			m_print->width = 997;
		}
		// m_print->width = 70; // The default window is 72 wide; subtract 2 for "> ".
	}
	m_print->carg = this;
	m_print->url = NET_CreateURLStruct(m_defaultUrl, NET_DONT_RELOAD);
	if (!m_print->url) {
		status = eOUT_OF_MEMORY;
		goto FAIL;
	}
	HG98265
	m_print->url->position_tag = 0;
	m_print->completion = MSG_CompositionPane::GetUrlDone_S;
	m_print->filename = WH_TempName(xpTemporary, "ns");
	if (!m_print->filename) {
		status = eOUT_OF_MEMORY;
		goto FAIL;
	}
	m_print->out = XP_FileOpen(m_print->filename, xpTemporary, XP_FILE_WRITE);
	if (!m_print->out) {
		status = 9999;				/* ###tw   Need the right error code! */
		goto FAIL;
	}
	m_print->cx = m_context;
	m_exitQuoting = NULL;
	m_dummyUrl = NET_CreateURLStruct("about:", NET_DONT_RELOAD);
	m_dummyUrl->internal_url = TRUE;
	if (m_dummyUrl) {
		FE_SetWindowLoading(m_context, m_dummyUrl, &m_exitQuoting);
		XP_ASSERT(m_exitQuoting != NULL);
	}

	/* Start the URL loading... (msg_get_url_done gets called later.) */


	m_textContext = (MWContext*) XL_TranslateText(m_context, m_print->url,
												  m_print);
	// ###tw  I'm not at all sure this cast is the
	// right thing to do here...

	return 0;
FAIL:
	FREEIF(m_print->filename);
	FREEIF(m_quoteUrl);
	if (m_print->out) {
		XP_FileClose(m_print->out);
		m_print->out = NULL;
	}
	if (m_print->url) {
		NET_FreeURLStruct(m_print->url);
		m_print->url = NULL;
	}
	return status;
}


int
MSG_CompositionPane::PastePlaintextQuotation(const char* str)
{
	if (str && *str) {
		if (EDT_PasteQuoteBegin(m_context, TRUE) != EDT_COP_OK) {
			return -1;
		}
		EDT_PasteQuote(m_context, "<BLOCKQUOTE TYPE=CITE><PRE>");
		EDT_PasteQuote(m_context, (char*) str);
		EDT_PasteQuote(m_context, "</PRE></BLOCKQUOTE>");
		EDT_PasteQuoteEnd(m_context);
	}
	return 0;
}



int 
MSG_CompositionPane::SetAttachmentList(struct MSG_AttachmentData* list)
{
	int count = 0;
	MSG_AttachmentData *tmp;
	MSG_AttachmentData *tmp2;
	int status = 0;

	ClearCompositionMessageID();	/* Since the attachment list has changed,
									   the message has changed, so make sure
									   we're using a fresh message-id when we
									   try to send it. */

	msg_free_attachment_list(m_attachData);
	m_attachData = NULL;

	for (tmp = list; tmp && tmp->url; tmp++) count++;

	if (count > 0) {
		m_attachData = (MSG_AttachmentData*)
			XP_ALLOC((count + 1) * sizeof(MSG_AttachmentData));
		if (!m_attachData) {
			FE_Alert(m_context, XP_GetString(MK_OUT_OF_MEMORY));
			return MK_OUT_OF_MEMORY;
		}

		XP_MEMSET(m_attachData, 0, (count + 1) * sizeof(MSG_AttachmentData));
	}

	if (count > 0) {
		for (tmp = list, tmp2 = m_attachData; tmp->url; tmp++, tmp2++) {
			tmp2->url = XP_STRDUP(tmp->url);
			if (tmp->desired_type) {
				tmp2->desired_type = XP_STRDUP(tmp->desired_type);
			}
			if (tmp->real_type) {
				tmp2->real_type = XP_STRDUP(tmp->real_type);
			}
			if (tmp->real_encoding) {
				tmp2->real_encoding = XP_STRDUP(tmp->real_encoding);
			}
			if (tmp->real_name) {
				tmp2->real_name = XP_STRDUP(tmp->real_name);
			}
			if (tmp->description) {
				tmp2->description = XP_STRDUP(tmp->description);
			}
			if (tmp->x_mac_type) {
				tmp2->x_mac_type = XP_STRDUP(tmp->x_mac_type);
			}
			if (tmp->x_mac_creator) {
				tmp2->x_mac_creator = XP_STRDUP(tmp->x_mac_creator);
			}
		}
	}
	status = DownloadAttachments();
	return status;
}


const struct MSG_AttachmentData *
MSG_CompositionPane::GetAttachmentList()
{
  if (m_attachData && m_attachData[0].url != NULL) return m_attachData;
  return NULL;
}


static void
msg_free_attachment_list(struct MSG_AttachmentData *list)
{
	MSG_AttachmentData* tmp;
	if (!list) return;
	for (tmp = list ; tmp->url ; tmp++) {
		XP_FREE((char*) tmp->url);
		if (tmp->desired_type) XP_FREE((char*) tmp->desired_type);
		if (tmp->real_type) XP_FREE((char*) tmp->real_type);
		if (tmp->real_encoding) XP_FREE((char*) tmp->real_encoding);
		if (tmp->real_name) XP_FREE((char*) tmp->real_name);
		if (tmp->description) XP_FREE((char*) tmp->description);
		if (tmp->x_mac_type) XP_FREE((char*) tmp->x_mac_type);
		if (tmp->x_mac_creator) XP_FREE((char*) tmp->x_mac_creator);
	}
	XP_FREEIF(list);
}



/* Whether the given saved-attachment-file thing is a match for the given
   URL (in source and type-conversion.)
 */
static XP_Bool
msg_attachments_match (MSG_AttachmentData *attachment,
					   MSG_AttachedFile *file)
{
	const char *dt;
	XP_ASSERT(attachment && file);
	if (!attachment || !file) return FALSE;
	XP_ASSERT(attachment->url && file->orig_url);
	if (!attachment->url || !file->orig_url) return FALSE;

	XP_ASSERT(file->type);
	if (!file->type) return FALSE;
	XP_ASSERT(file->file_name);
	if (XP_STRCMP(attachment->url, file->orig_url)) return FALSE;

	/* If the attachment has a conversion type specified (and it's not the
	   "no conversion" type) then this is only a match if the saved document
	   ended up with that type as well.
	   */
	dt = ((attachment->desired_type && *attachment->desired_type)
		  ? attachment->desired_type
		  : 0);
	if (dt && !strcasecomp(dt, TEXT_HTML))
		dt = 0;

	/* dt only has a value if it's "not `As Is', ie, text/plain or app/ps. */
	if (dt && XP_STRCMP(dt, file->type))
		return FALSE;

	return TRUE;
}


int
MSG_CompositionPane::DownloadAttachments()
{
	int attachment_count = 0;
	int new_download_count = 0;
	int download_overlap_count = 0;
	MSG_AttachmentData *tmp;
	MSG_AttachmentData *downloads = 0;
	MSG_AttachedFile *tmp2;
	int returnValue = 0;

	// *** Relax the rule a little bit to enable resume downloading at
	// *** send time.
	// XP_ASSERT(!m_deliveryInProgress);

	// Make sure we do not have an attachement already pending. If we do,
	// then we do not want to interrupt it. The new attachement will be picked up
	// when we go to send the message.

	if (m_attachmentInProgress)
		return MK_INTERRUPTED;       // this status value is ignored by the caller

	m_pendingAttachmentsCount = 0;	// reset m_pendingAttachmentsCount
									// in case the attachmentlist has been
									// cleared

	if (m_attachData)
		for (tmp = m_attachData; tmp->url; tmp++)
			attachment_count++;

	/* First, go through the list of desired attachments, and the list of
	   currently-saved attachments, and delete the files (and data) of the
	   ones which were attached/saved but are no longer.
	   */
	tmp2 = m_attachedFiles;
	while (tmp2 && tmp2->orig_url) {
		XP_Bool match = FALSE;
		for (tmp = m_attachData; tmp && tmp->url; tmp++) {
			if (msg_attachments_match(tmp, tmp2)) {
				match = TRUE;
				break;
			}
		}
		if (match) {
			tmp2++;
			download_overlap_count++;
		} else {
			/* Delete the file, free the strings, and pull the other entries
			   forward to cover this one. */
			int i = 0;

			if (tmp2->file_name) {
				XP_FileRemove(tmp2->file_name, xpFileToPost);
				XP_FREE(tmp2->file_name);
			}
			FREEIF(tmp2->orig_url);
			FREEIF(tmp2->type);
			FREEIF(tmp2->encoding);
			FREEIF(tmp2->description);
			FREEIF(tmp2->x_mac_type);
			FREEIF(tmp2->x_mac_creator);
			FREEIF(tmp2->real_name);
		  
                        do {
                            tmp2[i]=tmp2[i+1];
                        } while (tmp2[i++].orig_url);
		}
	}

	/* Now download any new files that are in the list.
	 */
	if (download_overlap_count != attachment_count) {
		MSG_AttachmentData *dfp;
		new_download_count = attachment_count - download_overlap_count;
		m_pendingAttachmentsCount = new_download_count;
		downloads = (MSG_AttachmentData *)
			XP_ALLOC(sizeof(MSG_AttachmentData) * (new_download_count + 1));
		if (!downloads) {
			FE_Alert(m_context, XP_GetString(MK_OUT_OF_MEMORY));
			return MK_OUT_OF_MEMORY;
		}
		XP_MEMSET(downloads, 0, sizeof(*downloads) * (new_download_count + 1));

		dfp = downloads;
		for (tmp = m_attachData; tmp && tmp->url; tmp++) {
			XP_Bool match = FALSE;
			if (m_attachedFiles)
				for (tmp2 = m_attachedFiles; tmp2->orig_url; tmp2++) {
					if (msg_attachments_match(tmp, tmp2)) {
						match = TRUE;
						break;
					}
				}
			if (!match) {
				*dfp = *tmp;
				dfp++;
			}
		}
		if (!downloads[0].url) return 0;
		// *** Relax the rule a little bit to enable resume downloading at
		// *** send time.
		// XP_ASSERT(!m_deliveryInProgress);
		XP_ASSERT(!m_attachmentInProgress);
		m_attachmentInProgress = TRUE;
		FE_UpdateCompToolbar (this);
		returnValue = msg_DownloadAttachments(this, this, downloads,
#ifdef XP_OS2
							(void (_Optlink*) (MWContext*,void*,int,const char*,MSG_AttachedFile*))
#endif
							   MSG_CompositionPane::DownloadAttachmentsDone_S);
		XP_FREE(downloads);
	}
	return returnValue;
}

void 
MSG_CompositionPane::DownloadAttachmentsDone_S(MWContext *context, 
											   void *fe_data,
											   int status,
											   const char *error_message,
											   struct MSG_AttachedFile *attachments)
{
	((MSG_CompositionPane*) fe_data)->DownloadAttachmentsDone(context, status,
															  error_message,
															  attachments);
}

void
MSG_CompositionPane::DownloadAttachmentsDone(MWContext* context, int status,
											 const char* error_message,
											 struct MSG_AttachedFile *attachments)
{
	XP_ASSERT(context == m_context);

	int old_count = 0;
	int new_count = 0;
	struct MSG_AttachedFile *tmp;
	MSG_AttachedFile *newd;

	// *** Relax the rule a little bit to enable resume downloading at
	// *** send time.
	// XP_ASSERT(!m_deliveryInProgress);
	if (m_attachmentInProgress) {
		m_attachmentInProgress = FALSE;
		FE_UpdateCompToolbar (this);
	}

	if (status < 0) goto FAIL;

	status = MK_INTERRUPTED;
	if (!m_attachData) goto FAIL;

	HG98229
	if (m_attachedFiles) {
		for (tmp = m_attachedFiles; tmp->orig_url; tmp++) old_count++;
	}
	if (attachments) {
		for (tmp = attachments; tmp->orig_url; tmp++) new_count++;
	}

	if (old_count + new_count == 0) goto FAIL;
	newd = (MSG_AttachedFile *)
		XP_REALLOC(m_attachedFiles,
				   ((old_count + new_count + 1)
					* sizeof(MSG_AttachedFile)));

	if (!newd) {
		status = MK_OUT_OF_MEMORY;
		error_message = XP_GetString(status);
		goto FAIL;
	}
	m_attachedFiles = newd;
	XP_MEMCPY(newd + old_count,
			  attachments,
			  sizeof(MSG_AttachedFile) * (new_count + 1));

	XP_ASSERT (m_pendingAttachmentsCount >= new_count);
	m_pendingAttachmentsCount -= new_count;
	if (m_deliveryInProgress) {
		m_deliveryInProgress = FALSE;
		DoneComposeMessage(m_deliver_mode);
	}
	return;

FAIL:
	XP_ASSERT(status < 0);
	if (error_message) {
		FE_Alert(context, error_message);
	}
	else if (status != MK_INTERRUPTED) {
		char *errmsg;
		errmsg = PR_smprintf(XP_GetString(MK_COMMUNICATIONS_ERROR), status);
		if (errmsg) {
			FE_Alert(context, errmsg);
			XP_FREE(errmsg);
		}
	}

	/* Since we weren't able to store it, ditch the files and the strings
	   describing them. */
	msg_delete_attached_files(attachments);
}


/* How many implementations of this are there now?  4? */
static void
msg_mid_truncate_string (const char *input, char *output, int max_length)
{
	int L = XP_STRLEN(input);
	if (L <= max_length) {
		XP_MEMCPY(output, input, L+1);
    } else {
		int mid = (max_length - 3) / 2;
		char *tmp = 0;
		if (input == output) {
			tmp = output;
			output = (char *) XP_ALLOC(max_length + 1);
			*tmp = 0;
			if (!output) return;
		}
		XP_MEMCPY(output, input, mid);
		XP_MEMCPY(output + mid, "...", 3);
		XP_MEMCPY(output + mid + 3, input + L - mid, mid + 1);

		if (tmp) {
			XP_MEMCPY(tmp, output, max_length + 1);
			XP_FREE(output);
		}
    }
}


char *
MSG_CompositionPane::GetAttachmentString()
{
	/* #### bug 8688 */

	MSG_AttachmentData *tmp;
	int count;
	int chars_per_attachment;
	int default_field_width = 63; /* 72 - some space for the word
									 "Attachment" */

	count = 0;
	for (tmp = m_attachData; tmp && tmp->url ; tmp++) count++;
	if (count <= 0) return 0;

	chars_per_attachment = (default_field_width - (count * 2)) / count;
	if (chars_per_attachment < 15) chars_per_attachment = 15;

	FREEIF(m_attachmentString);

	m_attachmentString =
		(char *) XP_ALLOC(count * (chars_per_attachment + 3) + 20);
	if (!m_attachmentString) return 0;
	*m_attachmentString = 0;

	for (tmp = m_attachData ; tmp && tmp->url ; tmp++) {
		const char *url = tmp->real_name ? tmp->real_name : tmp->url;
		const char *ptr = XP_STRCHR(url, ':');
		char *result = 0;

		if (!ptr) {
			/* No colon?  Must be a file name. */
			ptr = url;
			goto DO_FILE;
		}

		if (!XP_STRNCMP(url, "news:", 5) ||
			!XP_STRNCMP(url, "snews:", 6) ||
			!XP_STRNCMP(url, "IMAP:", 5) ||
			!XP_STRNCMP(url, "mailbox:", 8)) {
			/* ###tw Unfortunately, I don't think this stuff quite ports
			   directly to the new world, so I'm gonna disable it for now... */

			goto DONE;
		}

		/* Ok, so it must be something vaguely file-name-like.
		   Look for a slash.
		   */
	DO_FILE:
		{
			char *ptr2 = XP_STRDUP(ptr);
			if (!ptr2) goto DONE;
			char* s = XP_STRCHR(ptr2, '?');
			if (s) *s = 0;
			s = XP_STRCHR(ptr2, '#');
			if (s) *s = 0;
			s = XP_STRRCHR(ptr2, '/');
			if(!s) {
				XP_FREE(ptr2);
				goto DONE;
			}
			s++;
			if (!*s || !strcasecomp(s,"index.html") ||
				  !strcasecomp(s,"index.htm")) {
				/* This had a useless file name; take the last directory name. */
				char *s2 = s-1;
				if (*s2 == '/') s2--;
				while (s2 > ptr2 && *s2 != '/') s2--;
				if (*s2 == ':' || *s2 == '/') s2++;
				result = (char *) XP_ALLOC (s - s2 + 1);
				XP_MEMCPY (result, s2, s - s2);
				result[s - s2] = 0;
			} else {
				/* The file name is ok; use it. */
				result = XP_STRDUP (s);
			}
			NET_UnEscape (result);
			XP_FREE(ptr2);
			goto DONE;
		}

	DONE:
		if (tmp != m_attachData) {
			XP_STRCAT(m_attachmentString, "; ");
		}

		if (!result) {
			if (!XP_STRNCMP(url, "news:", 5) ||
				!XP_STRNCMP(url, "snews:", 6) ||
				!XP_STRNCMP(url, "IMAP:", 5) ||
				!XP_STRNCMP(url, "mailbox:", 8)) {
				result = XP_STRDUP("<message>");
			} else {
				result = XP_STRDUP(url);
			}
			if (!result) break;
		}

		msg_mid_truncate_string(result,
								(m_attachmentString +
								 XP_STRLEN(m_attachmentString)),
								chars_per_attachment);
		XP_FREE(result);
	}

	return m_attachmentString;
}


char*
MSG_CompositionPane::UpdateHeaderContents(MSG_HEADER_SET which_header,
										  const char* value)
{
  ABook* addressbook;
  switch (which_header) {
	case MSG_TO_HEADER_MASK:
	case MSG_CC_HEADER_MASK:
	case MSG_BCC_HEADER_MASK:
    case MSG_REPLY_TO_HEADER_MASK:
	  addressbook = FE_GetAddressBook(this);
	  if (addressbook) {
		return AB_ExpandHeaderString(addressbook, value, FALSE);
	  }
	  break;
  }
  return NULL;
}

int
MSG_CompositionPane::SetCompHeader(MSG_HEADER_SET header,
								   const char *value)
{
	XP_ASSERT(header != MSG_ATTACHMENTS_HEADER_MASK);
	ClearCompositionMessageID();
	m_fields->SetHeader(header, value);
	return 0;
}

const char*
MSG_CompositionPane::GetCompHeader(MSG_HEADER_SET header)
{
	if (header == MSG_ATTACHMENTS_HEADER_MASK) {
		return GetAttachmentString();
	} else {
		return m_fields ? m_fields->GetHeader(header) : (char *)NULL;
	}
}

int
MSG_CompositionPane::SetCompBoolHeader(MSG_BOOL_HEADER_SET header,
									   XP_Bool bValue)
{
	return m_fields->SetBoolHeader(header, bValue);
}

XP_Bool
MSG_CompositionPane::GetCompBoolHeader(MSG_BOOL_HEADER_SET header)
{
	return m_fields ? m_fields->GetBoolHeader(header) : FALSE;
}

const char*
MSG_CompositionPane::GetCompBody()
{
	return m_fields ? m_fields->GetBody() : (char *)NULL;
}

int
MSG_CompositionPane::SetCompBody(const char* value)
{
	return m_fields->SetBody(value);
}

const char*
MSG_CompositionPane::GetWindowTitle()
{
	const char *s;

	if (*m_fields->GetSubject()) {
		s = m_fields->GetSubject();
	} else if (*m_fields->GetTo()) {
		s = m_fields->GetTo();
	} else if (*m_fields->GetNewsgroups()) {
		s = m_fields->GetNewsgroups();
	} else {
		s = XP_GetString(MK_MSG_MSG_COMPOSITION);
	}
	return s;
}



int
MSG_CompositionPane::SanityCheck(int skippast)
{
	const char* body = m_fields->GetBody();
	const char* sub = m_fields->GetSubject();
	

	if (skippast == MK_MSG_DOUBLE_INCLUDE) goto AFTER_DOUBLE_INCLUDE;
	if (skippast == MK_MSG_EMPTY_MESSAGE) goto AFTER_EMPTY_MESSAGE;
	if (skippast == MK_MSG_MISSING_SUBJECT) goto AFTER_MISSING_SUBJECT;
	HG60283

	// Check if they have quoted a document and not edited it, and also
	// attached the same document.
	if (m_quotedText &&
		XP_STRNCMP(body, m_quotedText, XP_STRLEN(m_quotedText)) == 0 &&
		m_attachData &&
		m_attachData[0].url &&
		m_defaultUrl &&
		!XP_STRCMP (m_attachData[0].url, m_defaultUrl)) {
		return MK_MSG_DOUBLE_INCLUDE;
	}

AFTER_DOUBLE_INCLUDE:
	// Check if this message has no attachments, and the body has not been
	// edited.
	if (XP_STRCMP(body, m_initfields->GetBody()) == 0 &&
		(!m_attachData || !m_attachData[0].url)) {
		return MK_MSG_EMPTY_MESSAGE;
	}

AFTER_EMPTY_MESSAGE:
	// Check if they neglected to type a subject.
	if (sub) {
		while (XP_IS_SPACE(*sub)) sub++;
	}
	if (!sub || !*sub) {
		return MK_MSG_MISSING_SUBJECT;
	}

AFTER_MISSING_SUBJECT:
HG60207
	return 0;

}


void
MSG_CompositionPane::DeliveryDoneCB_s(MWContext *context, void *fe_data,
									  int status, const char *error_message)
{
	((MSG_CompositionPane*) fe_data)->DeliveryDoneCB(context, status,
													 error_message);
}

void
MSG_CompositionPane::DeliveryDoneCB(MWContext* context, int status,
									const char* error_message)
{
	XP_ASSERT(context == m_context);

	// *** We don't want to set m_status to status. The default value
	// of m_status (-1) prevents the composition pane from closing down
	// once we done with saving draft. The composition pane should remain up.
	if ((m_deliver_mode != MSG_SaveAsDraft && m_deliver_mode != MSG_SaveAsTemplate)
		|| (m_deliver_mode == MSG_SaveAsDraft && m_closeAfterSave))
	  m_status = status;

	XP_ASSERT(!m_attachmentInProgress);
	if (m_deliveryInProgress) {
		m_deliveryInProgress = FALSE;
#ifndef XP_UNIX /* Does not need this function call for UNIX. 
		   This will prevent toolbar to be enabled after msg sent on 
		   UNIX. */
		 FE_UpdateCompToolbar (this); 
#endif
	}

	if (status < 0) {
		if (error_message) {
			FE_Alert(context, error_message);
		} else if (status != MK_INTERRUPTED) {
			char *errmsg;

			HG92755
			errmsg = PR_smprintf(XP_GetString(MK_COMMUNICATIONS_ERROR),
								 status);
			if (errmsg) {
				FE_Alert(context, errmsg);
				XP_FREE(errmsg);
			}
		}
    } else {
		/* ### jht bug 45220 -- do following only when successfully deliver the message */
		/* time to delete message from draft/outbox */
		if (m_actionInfo) 
		{
			MailDB *mailDB = NULL;
			MSG_FolderInfoMail *mailFolderInfo = (m_actionInfo->m_folderInfo) ? m_actionInfo->m_folderInfo->GetMailFolderInfo() : (MSG_FolderInfoMail *)NULL;
			
			if ( mailFolderInfo && m_actionInfo->m_msgKeyArray.GetSize() > 0 ) 
			{

				MailDB::Open (mailFolderInfo->GetPathname(), FALSE, &mailDB);
				if (mailDB) {
					switch ( m_actionInfo->m_flags ) {
					case MSG_FLAG_EXPUNGED:
						if (m_deliver_mode != MSG_SaveAsDraft &&
							m_deliver_mode != MSG_SaveAsTemplate) {
							// XP_ASSERT(m_actionInfo->m_msgKeyArray.GetSize() == 1);
							if (mailFolderInfo->GetType() == FOLDER_IMAPMAIL)
							    DeleteIMAPOldDraftUID(m_actionInfo);
							else
								mailDB->DeleteMessage(m_actionInfo->m_msgKeyArray.GetAt(0));
						}
						break;
					case MSG_FLAG_FORWARDED:
						{
							uint i, size;
							IDArray readIds;
							MSG_IMAPFolderInfoMail *imapFolder = mailFolderInfo->GetIMAPFolderInfoMail();
							
							size = m_actionInfo->m_msgKeyArray.GetSize();
							if (m_deliver_mode == MSG_SaveAsDraft ||
								m_deliver_mode == MSG_SaveAsTemplate)
								size--;

							for (i=0; i < size; i++) {
							  mailDB->MarkForwarded(m_actionInfo->m_msgKeyArray.GetAt(0), TRUE);
							  if (mailFolderInfo)
							  {
								readIds.Add(m_actionInfo->m_msgKeyArray.GetAt(0));
							  }

							  m_actionInfo->m_msgKeyArray.RemoveAt(0);
							}
							if (imapFolder && (imapFolder->GetType()
											   == FOLDER_IMAPMAIL)) 
								imapFolder->StoreImapFlags
									(this, kImapMsgForwardedFlag,
									 TRUE, readIds);  
							if (m_deliver_mode == MSG_SaveAsDraft) {
								m_actionInfo->m_flags = MSG_FLAG_EXPUNGED;
								m_actionInfo->m_folderInfo = GetMaster()->
										FindMagicMailFolder(MSG_FOLDER_FLAG_DRAFTS, TRUE);

							}
							else if (m_deliver_mode == MSG_SaveAsTemplate)
							{
								m_actionInfo->m_flags = 0; // zero it out
							}
						}
					  break;
					case MSG_FLAG_REPLIED:
						{
							IDArray readIds;

							// XP_ASSERT(m_actionInfo->m_msgKeyArray.GetSize() == 1);
							mailDB->MarkReplied(m_actionInfo->m_msgKeyArray.GetAt(0), TRUE);
						
							MSG_IMAPFolderInfoMail *imapFolder = mailFolderInfo->GetIMAPFolderInfoMail();
							readIds.Add(m_actionInfo->m_msgKeyArray.GetAt(0));
							if (imapFolder && (imapFolder->GetType() == FOLDER_IMAPMAIL))
								imapFolder->StoreImapFlags(this, kImapMsgAnsweredFlag, TRUE, readIds);
							m_actionInfo->m_msgKeyArray.RemoveAt(0);
							if (m_deliver_mode == MSG_SaveAsDraft) {
								m_actionInfo->m_flags = MSG_FLAG_EXPUNGED;
								m_actionInfo->m_folderInfo = GetMaster()->
										FindMagicMailFolder(MSG_FOLDER_FLAG_DRAFTS, TRUE);

							}
							else if (m_deliver_mode == MSG_SaveAsTemplate)
							{
								m_actionInfo->m_flags = 0; // zero it out
							}
						}
						break;
					default:
						// This is the case of edit message then save as draft/template
						m_actionInfo->m_msgKeyArray.RemoveAt(0);
						if (m_deliver_mode == MSG_SaveAsDraft) {
							m_actionInfo->m_flags = MSG_FLAG_EXPUNGED;
							m_actionInfo->m_folderInfo = GetMaster()->
									FindMagicMailFolder(MSG_FOLDER_FLAG_DRAFTS, TRUE);

						}
						else if (m_deliver_mode == MSG_SaveAsTemplate)
						{
							m_actionInfo->m_flags = 0; // zero it out
						}
						break;
					}
					mailDB->Close();
				}
			}
			else if (m_actionInfo->m_folderInfo->GetType() == FOLDER_IMAPMAIL &&
					 m_actionInfo->m_msgKeyArray.GetSize() > 0 &&
					 m_deliver_mode != MSG_SaveAsDraft &&
					 m_deliver_mode != MSG_SaveAsTemplate &&
					 m_actionInfo->m_flags & MSG_FLAG_EXPUNGED)
			{
				DeleteIMAPOldDraftUID(m_actionInfo);
			}
		}

		// ### dmb This may not be right for news, but it shouldn't be harmful either
		if (m_actionInfo && m_actionInfo->m_folderInfo)
			m_actionInfo->m_folderInfo->SummaryChanged();

		if (m_deliver_mode == MSG_DeliverNow)
		{
			// If we're delivering the mail right now, tell the FE 
			// the Sent folder has new counts
			MSG_FolderInfo *folder = NULL;
			const char *fccPath = m_fields->GetFcc();

			if (fccPath && *fccPath)
				folder = FindFolderOfType(MSG_FOLDER_FLAG_SENTMAIL);

			if (folder)
				folder->SummaryChanged();
		}
		else if (m_deliver_mode == MSG_QueueForLater)
		{
			// If we're delivering the mail into the Outbox/queue folder, 
			// tell the FE the Outbox folder has new counts
			MSG_FolderInfo *folder = FindFolderOfType (MSG_FOLDER_FLAG_QUEUE);
			if (folder)
				folder->SummaryChanged();
		}
	}
}

void
MSG_CompositionPane::MailCompositionAllConnectionsComplete ()
{
	/* This may be redundant, I'm not sure... */
	if (m_deliveryInProgress) {
		m_deliveryInProgress = FALSE;
		FE_UpdateCompToolbar(this);
	}
	if (m_attachmentInProgress) {
		m_attachmentInProgress = FALSE;
		FE_UpdateCompToolbar(this);
	}

	if (m_status >= 0) {
		delete this;
	}
}


void
MSG_CompositionPane::CheckExpansion(MSG_HEADER_SET header)
{
	ABook* addressbook = FE_GetAddressBook(this);
	char* newvalue;
	if (addressbook) {
		newvalue = AB_ExpandHeaderString(addressbook, m_fields->GetHeader(header), 
			TRUE);
		if (newvalue) {
			m_fields->SetHeader(header, newvalue);
			XP_FREE(newvalue);
		}
	}
}


XP_Bool
MSG_CompositionPane::DeliveryInProgress ()
{
	/* Disable the UI if delivery, attachment loading, or quoting is in
	   progress. */
	return m_deliveryInProgress || m_attachmentInProgress || (m_quoteUrl != 0);
}

/* This function sets the markup flag to indicate that the message
   body is HTML.  Returns the previously set value.
*/

void
MSG_CompositionPane::SetHTMLMarkup(XP_Bool flag)
{
    m_markup = flag;
}

XP_Bool
MSG_CompositionPane::GetHTMLMarkup(void)
{
    return m_markup;
}

int
MSG_CompositionPane::DoneComposeMessage( MSG_Deliver_Mode deliver_mode )
{
	int attachment_count = 0;
	XP_Bool digest_p = FALSE;
	int status = 0;

	if (m_pendingAttachmentsCount) {
		m_deliveryInProgress = TRUE; // so that DoneComposeMessage is called again
		status = DownloadAttachments();
		return status;
	}

	HG92762
	const char *groups = m_fields->GetHeader(MSG_NEWSGROUPS_HEADER_MASK);
	if (groups && *groups && !m_host)
		m_host = InferNewsHost (groups);

	if (m_markup && (deliver_mode != MSG_SaveAsDraft &&
					 deliver_mode != MSG_SaveAsTemplate)) {
		MSG_HTMLComposeAction action = DetermineHTMLAction();
		if (action == MSG_HTMLAskUser)
		{
			status = -1;
#ifndef XP_UNIX	// Unix will have to make this dialog a blocking dialog for this logic to work
			if (m_callbacks.CreateAskHTMLDialog) {
				status = (*m_callbacks.CreateAskHTMLDialog)(this, m_callbackclosure);
				// if status == 0, then user wants to Send, so we continue
				// if status < 0, do the HTML dialogs
				// if status > 0, cancel
				if (status == 0) {
					 action = DetermineHTMLAction();	// reget the new action
					 XP_ASSERT(action != MSG_HTMLAskUser);
					 if (action == MSG_HTMLAskUser) {
						 // Boy, the FE is busted.  Use our own.
						 status = -1;
					 }
				}
				if (status > 0) return 0;		// we have to return 0, even in the cancel case so that an error won't get displayed
			}
#endif
			// if status still negative, do the HTML thing
			if (status < 0) {
				static XPDialogInfo dialogInfo = {
					0,				// I'll provide all my own buttons, thanks.
#ifdef XP_OS2
					(PRBool (_Optlink*) (XPDialogState*,char**,int,unsigned int))
#endif
					MSG_CompositionPane::AskDialogDone_s,
					500,
					300
				};
				XPDialogStrings* strings =
					XP_GetDialogStrings(MK_MSG_ASK_HTML_MAIL);
				if (!strings) return MK_OUT_OF_MEMORY;
				XP_MakeHTMLDialog(GetContext(), &dialogInfo,
								  MK_MSG_ASK_HTML_MAIL_TITLE, strings, this, PR_FALSE);
				return 0;
			}

		}

		switch (action) {
		case MSG_HTMLUseMultipartAlternative:
			m_fields->SetUseMultipartAlternative(TRUE);
			break;
		case MSG_HTMLConvertToPlaintext:
			m_fields->SetForcePlainText(TRUE);
			break;
		case MSG_HTMLSendAsHTML:
			break;
		default:
			XP_ASSERT(0);
			return -1;
		}
	}

	CheckExpansion(MSG_TO_HEADER_MASK);
	CheckExpansion(MSG_CC_HEADER_MASK);
	CheckExpansion(MSG_BCC_HEADER_MASK);

	HG42440

	const char* body = m_fields->GetBody();
	uint32 body_length = XP_STRLEN(body);


	for (attachment_count = 0;
		 m_attachData && m_attachData[attachment_count].url;
		 attachment_count++)
		;

	if (m_attachData && m_attachData[0].url && m_attachData[1].url ) {
		MSG_AttachmentData* s;
		digest_p = TRUE;
		for (s = m_attachData ; s->url ; s++) {
			/* When there are attachments, start out assuming it is a digest,
			   and then decide that it is not if any of the attached URLs are
			   not mail or news messages. */
			if (XP_STRNCMP(s->url, "news:", 5) != 0 &&
				XP_STRNCMP(s->url, "snews:", 6) != 0 &&
				XP_STRNCMP(s->url, "IMAP:", 5) != 0 &&
				XP_STRNCMP(s->url, "mailbox:", 8) != 0) {
				digest_p = FALSE;
			}
		}
	}

	XP_ASSERT(!m_attachmentInProgress);
	XP_ASSERT(!m_deliveryInProgress);
	m_deliveryInProgress = TRUE;
	FE_UpdateCompToolbar(this);

	if (m_messageId == NULL) {
		m_messageId = msg_generate_message_id();
		m_duplicatePost = FALSE;
	} else {
		m_duplicatePost = TRUE;
	}

	m_fields->SetMessageId(m_messageId);

	MSG_MimeRelatedSaver *fs = NULL;

#ifdef MSG_SEND_MULTIPART_RELATED
	if (m_markup)
	{
		char *pRootPartName = NULL; // set to NULL so that we're given a temp filename
		fs = new MSG_MimeRelatedSaver(this, m_context, m_fields, 
									  digest_p, deliver_mode, 
									  body, body_length,
									  m_attachedFiles,
									  DeliveryDoneCB_s,
									  &pRootPartName);
		if (fs)
		{
			EDT_SaveFileTo(m_context, 
                           ((deliver_mode == MSG_SaveAsDraft  && !m_closeAfterSave) ||
						    deliver_mode == MSG_SaveAsTemplate) ?
                              ED_FINISHED_SAVE_DRAFT : ED_FINISHED_MAIL_SEND, 
                          pRootPartName, fs, TRUE, TRUE);
			// Note: EDT_SaveFileTo will delete fs, even if it returns an error.  So
			// it is incorrect to delete it here.  Also, we ignore the result, because
			// it calls FE_Alert itself.
		}
		FREEIF(pRootPartName);
	}
	else

#endif // MSG_SEND_MULTIPART_RELATED

	{
		msg_StartMessageDeliveryWithAttachments(this, this,
												m_fields,
												digest_p, FALSE, deliver_mode,
												(m_markup ? TEXT_HTML : 
												 TEXT_PLAIN),
												body, body_length,
												m_attachedFiles,
												NULL,
#ifdef XP_OS2
												(void (_Optlink*) (MWContext*,void*,int,const char*))
#endif
												DeliveryDoneCB_s);
	}
	return 0; // Always success, because Errors were reported and handled by EDT_SaveFileTo.
}

int
MSG_CompositionPane::SendMessageNow()
{
	PREF_SetBoolPref("network.online", TRUE);	// make sure we're online.
		// remember if we're queued so we know which folder
	m_deliver_mode = MSG_DeliverNow;		
	
	HG73943
	if (m_fields->GetAttachVCard())
		CreateVcardAttachment();
	// counts we need to update.
	return DoneComposeMessage(MSG_DeliverNow);
}

int
MSG_CompositionPane::QueueMessageForLater()
{
		// remember if we're queued so we know which folder
	m_deliver_mode = MSG_QueueForLater;
	if (m_fields->GetAttachVCard())
		CreateVcardAttachment();
	// counts we need to update.
	return DoneComposeMessage(MSG_QueueForLater);
}

int
MSG_CompositionPane::SaveMessageAsDraft()
{
  
  m_deliver_mode = MSG_SaveAsDraft;    // remember if we're saved as draft so we know which
                           // folder counts we need to update.
  return DoneComposeMessage(MSG_SaveAsDraft);
}

int
MSG_CompositionPane::SaveMessageAsTemplate()
{
  m_deliver_mode = MSG_SaveAsTemplate;

#ifdef SUPPORT_X_TEMPLATE_NAME
  char *defaultName = NULL;

  defaultName = FE_Prompt (GetContext(),
						   XP_GetString(MK_MSG_ENTER_NAME_FOR_TEMPLATE), 
						   m_fields->GetSubject());
  if (defaultName && *defaultName)
  {
	  m_fields->SetTemplateName(defaultName);
	  XP_FREEIF(defaultName);
  }
#endif /* SUPPORT_X_TEMPLATE_NAME */

  return DoneComposeMessage(MSG_SaveAsTemplate);
}

static int
StuffParams(char** params, const char* name, int32 value)
{
	char* escaped = NET_EscapeHTML(name);
	if (!escaped) return MK_OUT_OF_MEMORY;
	char* tmp = PR_smprintf("<OPTION value=%ld>%s\n", (long) value, escaped);
	XP_FREE(escaped);
	if (!tmp) return MK_OUT_OF_MEMORY;
	NET_SACat(params, tmp);
	XP_FREE(tmp);
	return 0;
}

//void *pWnd used to pass in pointer to parent window of Recipients dialog
int
MSG_CompositionPane::PutUpRecipientsDialog(void *pWnd )
{
	int status;
	status = MungeThroughRecipients(NULL, NULL);
	if (status < 0) return status;
	MSG_RecipientList* ok = m_htmlrecip->GetList(TRUE);
	MSG_RecipientList* notok = m_htmlrecip->GetList(FALSE);
	if (m_callbacks.CreateRecipientsDialog) {
		status = (*m_callbacks.CreateRecipientsDialog)(this, m_callbackclosure,
													   notok, ok, pWnd);
		if (status >= 0) return status;
	}
	static XPDialogInfo dialogInfo = {
		0,				// I'll provide all my own buttons, thanks.
#ifdef XP_OS2
		(PRBool (_Optlink*) (XPDialogState*,char**,int,unsigned int))
#endif
		MSG_CompositionPane::RecipientDialogDone_s,
		600,
#ifdef XP_UNIX
		375
#else
		300
#endif
	};
	XPDialogStrings* strings =
		XP_GetDialogStrings(MK_MSG_HTML_RECIPIENTS);
	if (!strings) return MK_OUT_OF_MEMORY;


	XP_CopyDialogString(strings, 0, "\n\
function Scour(src) {\n\
    for (i=src.options.length-1 ; i>=0 ; i--) {\n\
	if (parseInt(src.options[i].value) >= 0) {\n\
	    return;\n\
	}\n\
	src.options[i] = null;\n\
    }\n\
}\n\
Scour(document.theform.nohtml);\n\
Scour(document.theform.html);\n\
function MoveItems(src, dest) {\n\
    var i, j, k, v, selectindex;\n\
    selectindex = -1;\n\
    for (i=src.options.length-1 ; i>=0 ; i--) {\n\
	if (src.options[i].selected) {\n\
	    src.options[i].selected = false;\n\
	    v = parseInt(src.options[i].value);\n\
	    selectindex = i;\n\
	    for (j=dest.options.length-1 ; j>=0 ; j--) {\n\
		if (parseInt(dest.options[j].value) < v) {\n\
		    break;\n\
		}\n\
		dest.options[j+1] = new Option(dest.options[j].text,\n\
					       dest.options[j].value);\n\
	    }\n\
	    j++;\n\
	    dest.options[j] = new Option(src.options[i].text,\n\
					 src.options[i].value);\n\
	    for (k=i ; k<src.options.length - 1 ; k++) {\n\
		src.options[k] = new Option(src.options[k+1].text,\n\
					    src.options[k+1].value);\n\
	    }\n\
	    src.options[k] = null;\n\
	}\n\
    }\n\
    if (selectindex >= src.options.length) {\n\
	selectindex = src.options.length - 1;\n\
    }\n\
    if (selectindex >= 0) {\n\
	src.options[selectindex].selected = true;\n\
    }\n\
}\n\
\n\
function DoAdd() {\n\
    MoveItems(document.theform.nohtml, document.theform.html);\n\
}\n\
\n\
function DoRemove() {\n\
    MoveItems(document.theform.html, document.theform.nohtml);\n\
}\n\
\n\
function SelectAllIn(obj, value) {\n\
    for (i=0 ; i<obj.length ; i++) {\n\
	obj[i].selected = value;\n\
    }\n\
}\n\
\n\
function SelectAll() {\n\
    SelectAllIn(document.theform.html, true);\n\
    SelectAllIn(document.theform.nohtml, true);\n\
}\n\
function Doit(value) {\n\
    document.theform.cmd.value = value;\n\
    document.theform.submit();\n\
}\n\
");

	for (int w=1 ; w<=2 ; w++) {
		MSG_RecipientList* list;
		char* params = NULL;
		for (list = (w == 1) ? notok : ok; list->value >= 0 ; list++) {
			status = StuffParams(&params, list->name, list->value);
			if (status < 0) return status;
		}
		for (list = (w == 1) ? ok : notok; list->value >= 0 ; list++) {
			status = StuffParams(&params, list->name, -1);
			if (status < 0) return status;
		}
		XP_CopyDialogString(strings, w, params ? params : "");
		FREEIF(params);
	}

	HG72999
    Chrome chrome;
    XP_MEMSET(&chrome, 0, sizeof(chrome));
    chrome.type = MWContextDialog;
    chrome.w_hint = dialogInfo.width;
    chrome.h_hint = dialogInfo.height;
    chrome.is_modal = TRUE;
    chrome.show_scrollbar = TRUE;

	XP_MakeHTMLDialogWithChrome(GetContext(), &dialogInfo,
					  		    MK_MSG_HTML_RECIPIENTS_TITLE, strings,
 								&chrome, this, PR_FALSE);
	return 0;
}


XP_Bool
MSG_CompositionPane::IsDuplicatePost() {
  return m_duplicatePost;
}

void 
MSG_CompositionPane::ClearCompositionMessageID()
{
  FREEIF(m_messageId);
}

const char*
MSG_CompositionPane::GetCompositionMessageID()
{
  return m_messageId;
}

HG20026


int 
MSG_CompositionPane::SetPreloadedAttachments ( MWContext *context,
											   struct MSG_AttachmentData *attachmentData,
											   struct MSG_AttachedFile *attachments,
											   int attachments_count )
{
  XP_ASSERT ( context == m_context );
  XP_ASSERT ( attachments && attachmentData );
  if ( !attachments || !attachmentData ) return -1;

  int status = 0;
  const char *error_message = NULL;
  
  XP_ASSERT ( m_attachData == NULL );

  m_attachData = (MSG_AttachmentData *) XP_ALLOC ( (attachments_count+1) *
												   sizeof (MSG_AttachmentData) );
  if ( !m_attachData ) {
	FE_Alert ( m_context, XP_GetString ( MK_OUT_OF_MEMORY ) );
	return MK_OUT_OF_MEMORY;
  }

  XP_MEMSET (m_attachData, 0, (attachments_count +1) * sizeof (MSG_AttachmentData));

  XP_MEMCPY ( m_attachData, attachmentData, 
			  sizeof (MSG_AttachmentData) * attachments_count );
	
  m_pendingAttachmentsCount = attachments_count;
  m_attachmentInProgress = TRUE;

  DownloadAttachmentsDone ( context, status, error_message, attachments );

  return status;
}

void 
MSG_CompositionPane::SetIMAPMessageUID ( MessageKey key )
{
	XP_ASSERT(key != MSG_MESSAGEKEYNONE);
	if (key == MSG_MESSAGEKEYNONE)
		return;
	if (!m_actionInfo)
	{
		int status = 0;
		char * onlineDraftsName = 
			msg_MagicFolderName(GetPrefs(), MSG_FOLDER_FLAG_DRAFTS, &status);

		if (!onlineDraftsName || !*onlineDraftsName)
			StrAllocCopy(onlineDraftsName, DRAFTS_FOLDER_NAME);
			
		m_actionInfo = new MSG_PostDeliveryActionInfo
			(GetMaster()->GetFolderInfo(onlineDraftsName, FALSE));

		XP_ASSERT (m_deliver_mode == MSG_SaveAsDraft);
		m_actionInfo->m_flags = MSG_FLAG_EXPUNGED;
		XP_FREEIF(onlineDraftsName);
	}
	XP_ASSERT(m_actionInfo);
	if (m_actionInfo)
	{
		m_actionInfo->m_msgKeyArray.Add(key);
	}
}

static MSG_HEADER_SET standard_header_set[] = {
    MSG_TO_HEADER_MASK,
    MSG_REPLY_TO_HEADER_MASK,
    MSG_CC_HEADER_MASK,
    MSG_BCC_HEADER_MASK,
    MSG_NEWSGROUPS_HEADER_MASK,
    MSG_FOLLOWUP_TO_HEADER_MASK
    };

#define TOTAL_HEADERS (sizeof(standard_header_set)/sizeof(MSG_HEADER_SET))

int 
MSG_CompositionPane::RetrieveStandardHeaders(MSG_HeaderEntry ** return_list)
{
  int i, total;
  const char * field;
  MSG_HeaderEntry * list = NULL;

  XP_ASSERT(return_list);

  *return_list = NULL;

  for (i=0, total=0; i<TOTAL_HEADERS; i++) {
    MSG_HeaderEntry * entry;
    int count;
    field = GetCompHeader(standard_header_set[i]);

    count = MSG_ExplodeHeaderField(standard_header_set[i],field,&entry);
    if (entry) {
      list = (MSG_HeaderEntry*)XP_REALLOC(list,(total+count)*sizeof(MSG_HeaderEntry));
      if (list == NULL) {
        XP_FREE(entry);
        return(-1);
      }
      memcpy(&list[total],entry,count*sizeof(MSG_HeaderEntry));   
      XP_FREE(entry);
      total += count;
    }
  }

  *return_list = list;
  return(total);
}

void 
MSG_CompositionPane::ClearComposeHeaders()
{
  int i;
  for (i = 0; i<TOTAL_HEADERS; i++)
    SetCompHeader(standard_header_set[i],NULL);
}

void 
MSG_CompositionPane::SetHeaderEntries(MSG_HeaderEntry * in_list, int count)
{
  if ((count != -1)&&(in_list!=NULL)) {
    int i;
    for (i=0; i<count; i++) {
      XP_ASSERT(in_list[i].header_value);
      SetCompHeader(in_list[i].header_type,in_list[i].header_value);
      XP_FREE(in_list[i].header_value);
    }
    XP_FREE(in_list);
  }
}



PRBool
MSG_CompositionPane::AskDialogDone_s(XPDialogState *state, char **argv,
									 int argc, unsigned int button)
{
	return ((MSG_CompositionPane*)state->arg)->AskDialogDone(state, argv,
															  argc, button);
}


PRBool
MSG_CompositionPane::AskDialogDone(XPDialogState * /*state*/, char **argv,
								   int argc, unsigned int /*button*/)
{
	switch (XP_ATOI(XP_FindValueInArgs("cmd", argv, argc))) {
	case 0:
		switch (XP_ATOI(XP_FindValueInArgs("mail", argv, argc))) {
		case 1:
			SetHTMLAction(MSG_HTMLUseMultipartAlternative);
			break;
		case 2:
			SetHTMLAction(MSG_HTMLConvertToPlaintext);
			break;
		case 3:
			SetHTMLAction(MSG_HTMLSendAsHTML);
			break;
		HG98276
		default:
			XP_ASSERT(0);
		}
		DoneComposeMessage(m_deliver_mode);
		return PR_FALSE;
	case 1:
		return PR_FALSE;
	case 2:
		PutUpRecipientsDialog();
		return PR_TRUE;
	case 3:
		XP_NetHelp(GetContext(), HELP_HTML_MAIL_QUESTION);
		return PR_TRUE;
	default:
		XP_ASSERT(0);
		break;
	}
	return PR_FALSE;
}

#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
int DomainCompare(const void* e1, const void* e2)
{
	return XP_STRCMP(*((char**) e1), *((char**) e2));
}

int
MSG_CompositionPane::ResultsRecipients(XP_Bool cancelled, int32* nohtml,
									   int32* htmlok)
{
	int status = 0;
	if (cancelled) return 0;
	XP_ASSERT(m_htmlrecip);
	if (!m_htmlrecip) return -1;
	status = m_htmlrecip->SetNewList(nohtml, htmlok);
	if (status < 0) return status;

	char** list = NULL;
	char** tmp;
	char* ptr;
	char* endptr;
	char* domainlist = NULL;
	char** domainstrings = NULL;
	XP_Bool changed;
	int num = 0;
	int max = 0;
	int length;
	int i, j;


	ABook* addressbook = FE_GetAddressBook(this);
	DIR_Server* pab = NULL;
	DIR_GetPersonalAddressBook(FE_GetDirServers(), &pab);

	for (int w=0 ; w<2 ; w++) {
		XP_Bool html = (w == 1);
		if (pab && addressbook) {
			list = m_htmlrecip->GetChangedList(Address, html);
			for (tmp = list ; tmp && *tmp ; tmp++) {
				PersonEntry person;
				person.Initialize();
				char* names = NULL;
				char* addresses = NULL;
				int num = MSG_ParseRFC822Addresses(*tmp, &names, &addresses);
				XP_ASSERT(num == 1);
				if (num == 1) {
					person.pEmailAddress = addresses;
					if (names && *names) {
						person.pGivenName = names;
					}
					person.HTMLmail = html;
					AB_SetHTMLForPerson(pab, addressbook, &person);
				}
				FREEIF(addresses);
				FREEIF(names);
			}
			m_htmlrecip->FreeChangedList(list);
			list = NULL;
		}
		if (m_host) {
			list = m_htmlrecip->GetChangedList(Newsgroup, html);
			for (tmp = list ; tmp && *tmp ; tmp++) {
				m_host->SetIsHTMLOKGroup(*tmp, html);
			}
			m_htmlrecip->FreeChangedList(list);
			list = m_htmlrecip->GetChangedList(GroupHierarchy, html);
			for (tmp = list ; tmp && *tmp ; tmp++) {
				m_host->SetIsHTMLOKTree(*tmp, html);
			}
			m_htmlrecip->FreeChangedList(list);
			list = NULL;
		}
	}
	if (m_host) m_host->SaveHostInfo();

	PREF_CopyCharPref("mail.htmldomains", &domainlist);
	changed = FALSE;

	length = domainlist ? XP_STRLEN(domainlist) : 0;
	for (ptr = domainlist ; ptr && *ptr ; ptr = XP_STRCHR(ptr + 1, ',')) {
		max++;
	}
	max += m_htmlrecip->GetNum() + 1; // We be paranoid.
	domainstrings = new char* [max];
	if (!domainstrings) {
		status = MK_OUT_OF_MEMORY;
		goto FAIL;
	}
	for (ptr = domainlist ; ptr && *ptr ; ptr = endptr) {
		endptr = XP_STRCHR(ptr, ',');
		if (endptr) *endptr++ = '\0';
		domainstrings[num++] = ptr;
	}
	list = m_htmlrecip->GetChangedList(Domain, FALSE);
	for (tmp = list ; tmp && *tmp ; tmp++) {
		for (i=0 ; i<num ; i++) {
			while (i<num && XP_STRCMP(domainstrings[i], *tmp) == 0) {
				num--;
				domainstrings[i] = domainstrings[num];
				changed = TRUE;
			}
		}
	}
	m_htmlrecip->FreeChangedList(list);
	list = m_htmlrecip->GetChangedList(Domain, TRUE);
	for (tmp = list ; tmp && *tmp ; tmp++) {
		domainstrings[num++] = *tmp;
		changed = TRUE;
		length += XP_STRLEN(*tmp) + 1;
	}
	if (changed) {
		// Now nuke dups.
		XP_QSORT(domainstrings, num, sizeof(char*), DomainCompare);
		for (i=0 ; i < num-1 ; i++) {
			while (i < num-1 &&
				   XP_STRCMP(domainstrings[i], domainstrings[i+1]) == 0) {
				num--;
				for (j=i+1 ; j<num ; j++) {
					domainstrings[j] = domainstrings[j+1];
				}
			}
		}
		ptr = new char [length + 1];
		if (!ptr) {
			status = MK_OUT_OF_MEMORY;
			goto FAIL;
		}
		*ptr = '\0';
		for (i=0 ; i<num ; i++) {
			XP_STRCAT(ptr, domainstrings[i]);
			if (i < num-1) XP_STRCAT(ptr, ",");
		}
		PREF_SetCharPref("mail.htmldomains", ptr);
		PREF_SavePrefFile();
		delete [] ptr;
		ptr = NULL;
	}				

 FAIL:
	m_htmlrecip->FreeChangedList(list);
	FREEIF(domainlist);
	delete [] domainstrings;
	return status;
}




PRBool
MSG_CompositionPane::RecipientDialogDone_s(XPDialogState *state, char **argv,
										   int argc, unsigned int button)
{
	return
		((MSG_CompositionPane*)state->arg)->RecipientDialogDone(state, argv,
																argc, button);
}


static void
Slurp(int32* list, const char* name, char** argv, int argc)
{
	for (; argc > 0 ; argc -= 2 , argv += 2) {
		if (XP_STRCMP(name, argv[0]) == 0) {
			*list++ = XP_ATOI(argv[1]);
		}
	}
	*list = -1;
}

PRBool
MSG_CompositionPane::RecipientDialogDone(XPDialogState * /*state*/,
										 char **argv,
										 int argc, unsigned int /*button*/)
{
	HG73622
	switch (XP_ATOI(XP_FindValueInArgs("cmd", argv, argc))) {
	case 0: {
		XP_ASSERT(argc > 0);
		if (argc <= 0) return PR_FALSE;
		int32* nohtml = new int32 [argc];
		if (!nohtml) return PR_FALSE;
		int32* htmlok = new int32 [argc];
		if (!htmlok) {
			delete [] nohtml;
			return PR_FALSE;
		}
		Slurp(nohtml, "nohtml", argv, argc);
		Slurp(htmlok, "html", argv, argc);
		ResultsRecipients(FALSE, nohtml, htmlok);
		delete [] nohtml;
		delete [] htmlok;
		return PR_FALSE;
	}
	case 1:
		ResultsRecipients(TRUE, NULL, NULL);
		return PR_FALSE;
	case 2:
		XP_NetHelp(GetContext(), HELP_HTML_MAIL_QUESTION_RECIPIENT);
		return PR_TRUE;
	default:
		XP_ASSERT(0);
		break;
	}
	return PR_FALSE;
}


XP_Bool
MSG_CompositionPane::HasNoMarkup()
{

	// we want a link with the same text and link to pass
	// <A HREF="http://warp/client/dogbert">http://warp/client/dogbert</A>

	XP_ASSERT(m_markup);
	if (!m_markup) return TRUE;
	const char* body = m_fields->GetBody();
	while (body && *body) {
		body = XP_STRCHR(body, '<');
		if (!body) break;
		char* endptr = XP_STRCHR(body, '>');
		XP_ASSERT(endptr);
		if (!endptr) break;
		char c = *++endptr;
		*endptr = '\0';
		XP_Bool recognized = FALSE;
		char* newEnd = endptr;
		
		if (XP_STRNCASECMP(body, "<A HREF=", 8) == 0) {
			char* pLinkStart = XP_STRCHR(body, '"');		// find the open quote
			if (pLinkStart) {
				++pLinkStart;								// past the open quote
				char* pLinkEnd = XP_STRCHR(pLinkStart, '"');// find the close quote
				if (pLinkEnd) {
					char c2 = *pLinkEnd;					// save this char
					*pLinkEnd = '\0';						// terminate the link

					// now pLink points to the URL
					// find the text to see if it's the same as the link
					char* pTextStart = endptr;
					*endptr = c;							// restore this char early because it's the lead char of our string
					char* pTextEnd = XP_STRCHR(pTextStart, '<');
					if (pTextEnd) {
						char c3 = *pTextEnd;
						*pTextEnd = '\0';					// terminate the text
						recognized = (XP_STRCMP(pLinkStart, pTextStart) == 0);
						*pTextEnd = c3;						// restore this char
						newEnd = pTextEnd + 1;				// skip past the opening of the </A>
					}
					*pLinkEnd = c2;							// restore this char
				}
			}
		} else {
			recognized = (XP_STRCASECMP(body, "<P>") == 0 ||
								  XP_STRCASECMP(body, "</P>") == 0 ||
								  XP_STRCASECMP(body, "<BR>") == 0 ||
								  XP_STRCASECMP(body, "<DT>") == 0 ||
								  XP_STRCASECMP(body, "</DT>") == 0 ||
								  XP_STRCASECMP(body, "<HTML>") == 0 ||
								  XP_STRCASECMP(body, "</HTML>") == 0 ||
								  XP_STRNCASECMP(body, "<BODY", 5) == 0 ||
								  XP_STRCASECMP(body, "</BODY>") == 0 ||
								  XP_STRCASECMP(body,
												"<BLOCKQUOTE TYPE=CITE>") == 0 ||
								  XP_STRCASECMP(body, "</BLOCKQUOTE>") == 0);
		}

		*endptr = c;
		body = newEnd;
		if (!recognized) return FALSE;
	}
	return TRUE;
}

int
MSG_CompositionPane::MungeThroughRecipients(XP_Bool* someNonHTML,
											XP_Bool* groupNonHTML)
{
	XP_Bool foo;
	if (!someNonHTML) someNonHTML = &foo;
	if (!groupNonHTML) groupNonHTML = &foo;
	*someNonHTML = FALSE;
	*groupNonHTML = FALSE;
	int status = 0;
	char* names = NULL;
	char* addresses = NULL;
	const char* groups;
	char* name = NULL;
	char* end;
	XP_Bool match = FALSE;
	m_host = NULL;				// Pure paranoia, in case we some day actually
								// have a UI that lets people change this.

	ABook* addressbook = FE_GetAddressBook(this);
	DIR_Server* pab = NULL;
	static int32 masks[] = {
		MSG_TO_HEADER_MASK,
		MSG_CC_HEADER_MASK,
		MSG_BCC_HEADER_MASK
	};
	char* domainlist = NULL;

	HG02726
	delete m_htmlrecip;
	m_htmlrecip = new MSG_HTMLRecipients();
	if (!m_htmlrecip) return MK_OUT_OF_MEMORY;

	for (int i=0 ; i < sizeof(masks) / sizeof(masks[0]) ; i++) {
		const char* orig = m_fields->GetHeader(masks[i]);
		if (!orig || !*orig) continue;
		char* value = NULL;
		if (addressbook) {
			value = AB_ExpandHeaderString(addressbook, orig, TRUE);
		}
		if (!value) {
			value = XP_STRDUP(orig);
			if (!value) {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			}
		}
		int num = MSG_ParseRFC822Addresses(value, &names, &addresses);
		XP_FREE(value);
		value = NULL;
		char* addr = NULL;
		char* name = NULL;
		for (int j=0 ; j<num ; j++) {
			if (addr) {
				addr = addr + XP_STRLEN(addr) + 1;
				name = name + XP_STRLEN(name) + 1;
			} else {
				addr = addresses;
				name = names;
			}
			if (!addr || !*addr) continue;
			
			// Check for a address book entry for this person.
			ABID entryID = MSG_MESSAGEIDNONE;
			if (!pab) {
				DIR_GetPersonalAddressBook (FE_GetDirServers(), &pab);
			}
			if (pab && addressbook) {
				PersonEntry person;
				person.Initialize();
				person.pEmailAddress = addr;
				if (name && *name) person.pGivenName = name;
				AB_GetEntryIDForPerson(pab, addressbook, &entryID, &person);
				if (entryID == MSG_MESSAGEIDNONE && name && *name) {
					// See if we can find an entry under any name.
					person.pGivenName = NULL;
					AB_GetEntryIDForPerson(pab, addressbook, &entryID,
										   &person);
				}
			}
			char* at = XP_STRCHR(addr, '@');
			match = FALSE;
			if (entryID != MSG_MESSAGEIDNONE) {
				AB_GetHTMLMail(pab, addressbook, entryID, &match);
			}
			
			char* tmp = MSG_MakeFullAddress(name, addr);
			status = m_htmlrecip->AddOne(tmp, addr, Address, match);
			if (status < 0) goto FAIL;
			XP_FREE(tmp);
			tmp = NULL;

			if (!at) {
				// ###tw We got to decide what to do in these cases.  But
				// for now, I'm just gonna ignore them.  Which is probably
				// exactly the wrong thing.  Fortunately, these cases are
				// now very rare, as we have code that inserts a default
				// domain.
				continue;
			}
			if (!domainlist) {
				PREF_CopyCharPref("mail.htmldomains", &domainlist);
			}
			char* domain = at + 1;
			for (;;) {
				char* dot = XP_STRCHR(domain, '.');
				if (!dot) break;
				int32 domainlength = XP_STRLEN(domain);
				char* ptr;
				char* endptr = NULL;
				XP_Bool found = FALSE;
				for (ptr = domainlist ; ptr && *ptr ; ptr = endptr) {
					endptr = XP_STRCHR(ptr, ',');
					int length;
					if (endptr) {
						length = endptr - ptr;
						endptr++;
					} else {
						length = XP_STRLEN(ptr);
					}
					if (length == domainlength) {
						if (XP_STRNCASECMP(domain, ptr, length) == 0) {
							found = TRUE;
							match = TRUE;
							break;
						}
					}
				}
				char* tmp = PR_smprintf("%s@%s",
										XP_GetString(MK_MSG_EVERYONE),
										domain);
				if (!tmp) return MK_OUT_OF_MEMORY;
				status = m_htmlrecip->AddOne(domain, tmp, Domain, found);
				XP_FREE(tmp);
				if (status < 0) goto FAIL;
				domain = dot + 1;
			}
			if (!match) *someNonHTML = TRUE;
		}
	}

	groups = m_fields->GetHeader(MSG_NEWSGROUPS_HEADER_MASK);
	if (groups && *groups && !m_host)
	{
		m_host = InferNewsHost (groups);
		if (!m_host)
			goto FAIL;
	}

	end = NULL;
	for ( ; groups && *groups ; groups = end) {
		end = XP_STRCHR(groups, ',');
		if (end) *end = '\0';
		name = XP_STRDUP(groups);
		if (end) *end++ = ',';
		if (!name) {
			status = MK_OUT_OF_MEMORY;
			goto FAIL;
		}
		char* group = XP_StripLine(name);
		match = m_host->IsHTMLOKGroup(group);
		status = m_htmlrecip->AddOne(group, group, Newsgroup, match);
		if (status < 0) goto FAIL;
		char* tmp = XP_STRDUP(group);
		if (!tmp) {
			status = MK_OUT_OF_MEMORY;
			goto FAIL;
		}
		HG02977
		for (;;) {
			XP_Bool found = m_host->IsHTMLOKTree(tmp);
			char* desc = PR_smprintf("%s.*", tmp);
			if (!desc) {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			}
			status = m_htmlrecip->AddOne(tmp, desc, GroupHierarchy, found);
			XP_FREE(desc);
			if (status < 0) {
				XP_FREE(tmp);
				tmp = NULL;
				goto FAIL;
			}
			if (found) match = TRUE;
			char* p = XP_STRRCHR(tmp, '.');
			if (p) *p = '\0';
			else break;
		}
		XP_FREE(tmp);
		tmp = NULL;
		if (!match) {
			*someNonHTML = TRUE;
			*groupNonHTML = TRUE;
		}
	}

 FAIL:
	FREEIF(names);
	FREEIF(domainlist);
	FREEIF(addresses);
	FREEIF(name);
	return status;
}



MSG_HTMLComposeAction
MSG_CompositionPane::DetermineHTMLAction()
{
	XP_Bool someNonHTML, groupNonHTML;
	int status;

	MSG_HTMLComposeAction result = GetHTMLAction();


	if (result == MSG_HTMLAskUser) {
		// Well, before we ask, see if we can figure out what to do for
		// ourselves.

		status = MungeThroughRecipients(&someNonHTML, &groupNonHTML);
		if (status < 0) return MSG_HTMLAskUser; // ###
		if (!someNonHTML) return MSG_HTMLSendAsHTML;
		if (HasNoMarkup()) {
			// No point in sending this message as HTML; send it plain.
			return MSG_HTMLConvertToPlaintext;
		}
		// See if a preference has been set to tell us what to do.  Note that
		// we do not honor that preference for newsgroups, only for e-mail
		// addresses.
		if (!groupNonHTML) {
			int32 value;
			if (PREF_GetIntPref("mail.default_html_action", &value) >= 0) {
				switch (value) {
				case 1:				// Force plaintext.
					return MSG_HTMLConvertToPlaintext;
				case 2:				// Force HTML.
					return MSG_HTMLSendAsHTML;
				case 3:				// Force multipart/alternative.
					return MSG_HTMLUseMultipartAlternative;
				}
			}
		}
	}
	return result;
}
	

MSG_NewsHost *MSG_CompositionPane::InferNewsHost (const char *group)
{
	MSG_NewsHost *retHost = NULL;

	// No news host was specified. Use our bag of tricks to try to find one
	char* host_and_port = NULL;
	XP_Bool xxx_p = FALSE;

	// Trick #1: Do we have a URL for the host in the header fields? This can happen
	// if we're posting, or if the user has typed a news URL into the newsgroups field
	const char* posturl = m_fields->GetNewspostUrl();
	if (posturl && *posturl) 
	{
		host_and_port = NET_ParseURL(posturl, GET_HOST_PART);
		HG93663
		if (host_and_port && *host_and_port) 
			retHost = GetMaster()->FindHost(host_and_port, xxx_p, -1);
	}

	if (!retHost)
	{
		msg_HostTable *hostTable = GetMaster()->GetHostTable();
		if (!hostTable)
			return retHost;
		// Trick #2: Look through all the hosts and their subscribed groups for a *unique* match
		if (!retHost)
		{
			retHost = hostTable->InferHostFromGroups (group);
			if (retHost)
			{
				const char *base = retHost->GetURLBase();
				if (base)
					m_fields->SetHeader (MSG_NEWSPOSTURL_HEADER_MASK, XP_STRDUP(base));
			}

			// Trick #3: This group isn't found among our subscribed groups, or there's more
			// than one group with the same name, so just use the default host.
			if (!retHost) 
				retHost = hostTable->GetDefaultHost(TRUE);
		}
	}

	FREEIF(host_and_port);
	return retHost;
}




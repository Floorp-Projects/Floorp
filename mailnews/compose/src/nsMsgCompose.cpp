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

#include "rosetta_mailnews.h"
#include "nsMsgSend.h"
#include "nsMsgComposeFact.h"
#include "nsMsgCompPrefs.h"
#include "nsMsgCompose.h"

/*JFD
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
*/

#ifdef UNREADY_CODE
HJ04305
#endif

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

/* this function will be used by the factory to generate an Message Compose Object....*/
nsresult NS_NewMsgCompose(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgCompose* pCompose = new nsMsgCompose();
		if (pCompose)
			return pCompose->QueryInterface(aIID, aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}


class RecipientEntry : public MSG_ZapIt {
public:
	RecipientEntry(const char* name, const char* description,
				   RecipientType type, PRBool htmlok);
	~RecipientEntry();

	char* GetName() {return m_name;}
	char* GetDescription() {return m_description;}
	RecipientType GetType() {return m_type;}
	PRBool GetHTMLOk() {return m_htmlok;}
	PRBool GetNewHTMLOk() {return m_newhtmlok;}
	void SetNewOK(PRBool value);
	PRBool GetTouched() {return m_touched;}

protected:
	char* m_name;
	char* m_description;
	RecipientType m_type;
	PRBool m_htmlok;
	PRBool m_newhtmlok;
	PRBool m_touched;
};


RecipientEntry::RecipientEntry(const char* name, const char* description,
							   RecipientType type, PRBool htmlok)
{
	m_name = PL_strdup(name);
	m_description = PL_strdup(description);
	if (!m_description) {
		PR_FREEIF(m_name);		// Checking for name being NULL is the hack
								// used to see if we're out of memory.
	}
	m_type = type;
	m_htmlok = htmlok;
	m_newhtmlok = PR_FALSE;
	m_touched = PR_FALSE;
}

RecipientEntry::~RecipientEntry()
{
	PR_FREEIF(m_name);
	PR_FREEIF(m_description);
}

void
RecipientEntry::SetNewOK(PRBool value)
{
	PR_ASSERT(!m_touched);
	m_touched = PR_TRUE;
	m_newhtmlok = value;
}



class MSG_HTMLRecipients : public MSG_ZapIt {
public:
	MSG_HTMLRecipients();
	~MSG_HTMLRecipients();

	int AddOne(const char* name, const char* description,
			   RecipientType type, PRBool htmlok);
	MSG_RecipientList* GetList(PRBool htmlok);

	int SetNewList(PRInt32* notoklist, PRInt32* oklist);

	char** GetChangedList(RecipientType type, PRBool htmlok);
	void FreeChangedList(char** list);
	int GetNum() {return m_num;}

protected:
	RecipientEntry** m_list;
	PRInt32 m_num;
	PRInt32 m_max;
	MSG_RecipientList* m_generatedList[2];
};

MSG_HTMLRecipients::MSG_HTMLRecipients() {
	m_list = NULL;
	m_num = 0;
	m_max = 0;
	m_generatedList[0] = NULL;
	m_generatedList[1] = NULL;
}

MSG_HTMLRecipients::~MSG_HTMLRecipients() {
	delete m_generatedList[0];
	delete m_generatedList[1];
	for (PRInt32 i=0 ; i<m_num ; i++) {
		delete m_list[i];
	}
	delete [] m_list;
}

int
MSG_HTMLRecipients::AddOne(const char* name, const char* description,
						   RecipientType type, PRBool htmlok)
{
	PRInt32 i;
	for (i=0 ; i<m_num ; i++) {
		if (m_list[i]->GetType() == type &&
			PL_strcmp(m_list[i]->GetName(), name) == 0) return 0;
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
MSG_HTMLRecipients::GetList(PRBool htmlok)
{
	PRInt32 i, j;
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
		PRInt32 cur[2];
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
MSG_HTMLRecipients::SetNewList(PRInt32* notoklist, PRInt32* oklist)
{
	PRInt32 i;
#ifdef DEBUG
	for (i=0 ; i<m_num ; i++) {
		PR_ASSERT(!m_list[i]->GetTouched());
	}
#endif
	for (int w=0 ; w<2 ; w++) {
		PRBool ok = (w == 1);
		PRInt32* list = ok ? oklist : notoklist;
		PR_ASSERT(list);
		if (!list) continue;
		for ( ; *list >= 0 ; list++) {
			PR_ASSERT(*list < m_num);
			if (*list >= m_num) break;
			m_list[*list]->SetNewOK(ok);
		}
	}
	int status = 0;
	for (i=0 ; i<m_num ; i++) {
		PR_ASSERT(m_list[i]->GetTouched());
		if (!m_list[i]->GetTouched()) {
			status = -1;
		}
	}
	return status;
}


char**
MSG_HTMLRecipients::GetChangedList(RecipientType type, PRBool htmlok)
{
	char** result = new char * [m_num + 1];
	if (!result) return NULL;
	char** tmp = result;
	for (PRInt32 i=0 ; i<m_num ; i++) {
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
		PR_FREEIF(tmp->orig_url);
		PR_FREEIF(tmp->type);
		PR_FREEIF(tmp->real_name);
		PR_FREEIF(tmp->encoding);
		PR_FREEIF(tmp->description);
		PR_FREEIF(tmp->x_mac_type);
		PR_FREEIF(tmp->x_mac_creator);
		if (tmp->file_name) {
			XP_FileRemove(tmp->file_name, xpFileToPost);
			PR_Free(tmp->file_name);
		}
	}
	PR_FREEIF(attachments);
}

nsMsgCompose::nsMsgCompose()
{
	m_replyType = 0;
	m_markup = PR_FALSE;
	m_attachData = NULL;
	m_attachedFiles = NULL;
	m_defaultUrl = NULL;
	m_initfields = NULL;
	m_fields = NULL;
	m_messageId = NULL;
	m_attachmentString = NULL;
	m_quotedText = NULL;
	m_print = NULL;
	m_textContext = NULL;
	m_context = NULL;
	m_oldContext = NULL;
	m_quoteUrl = NULL;
	m_dummyUrl = NULL;
	m_exitQuoting = NULL;
	m_quotefunc = NULL;
	m_quoteclosure = NULL;
	m_deliveryInProgress = PR_FALSE;
	m_attachmentInProgress = PR_FALSE;
	m_pendingAttachmentsCount = 0;
	m_deliver_mode = MSG_DeliverNow;
	m_cited = PR_FALSE;
	m_duplicatePost = PR_FALSE;
	m_htmlaction = MSG_HTMLAskUser;
	m_htmlrecip = NULL;
	m_status = 0;
	m_visible_headers = 0;
	m_host = NULL;
	m_closeAfterSave = PR_FALSE;
	m_haveQuoted = PR_FALSE;
	m_haveAttachedVcard = PR_FALSE;
	m_callbacks.CreateAskHTMLDialog = NULL;
	m_callbacks.CreateRecipientsDialog = NULL;
	m_callbackclosure = NULL;
	m_lineWidth = 0;

#ifdef UNREADY_CODE
	HJ58932
#endif

	/* the following macro is used to initialize the ref counting data */
	NS_INIT_REFCNT();
}

nsMsgCompose::~nsMsgCompose()
{
//JFD	Dispose();
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgCompose, nsIMsgCompose::GetIID());

nsresult nsMsgCompose::CreateAndInitialize(/*MWContext* */PRInt32 context,
									 /*MWContext* */PRInt32 old_context,
									 /*MSG_Prefs* */PRInt32 prefs,
									 nsIMsgCompFields* fields,
									 /*MSG_Master* */PRInt32 master)
{
	MSG_PaneCreate((MWContext*)context, (MSG_Master*)master);	/*JFD*/
	m_prefs = (MSG_Prefs*)prefs;
	SetHTMLAction(MSG_HTMLAskUser);
	Initialize(old_context, fields);
	// make sure we have a valid folder tree - via side effect of getfoldertree
	if (master)
		((MSG_Master*)master)->GetFolderTree();

	return NS_OK;
}

nsresult nsMsgCompose::Create(/*MWContext**/PRInt32 context, /*MSG_Prefs**/PRInt32 prefs, /*MSG_Master**/PRInt32 master)
{
	MSG_PaneCreate((MWContext*)context, (MSG_Master*)master);	/*JFD*/
	m_prefs = (MSG_Prefs*)prefs;
	// make sure we have a valid folder tree - via side effect of getfoldertree
	if (master)
		((MSG_Master*)master)->GetFolderTree();

	return NS_OK;
}


nsresult nsMsgCompose::Initialize(/*MWContext**/PRInt32 old_context, nsIMsgCompFields* fields)
{
/*JFD
	m_print = new PrintSetup;
*/
#ifdef UNREADY_CODE
	HJ22867
#endif

	InitializeHeaders((MWContext*)old_context, fields);
	m_visible_headers = GetInterestingHeaders();
	m_deliver_mode = MSG_DeliverNow;
	m_haveAttachedVcard = PR_FALSE;

	m_fields->SetForcePlainText(PR_FALSE);	// Coming into us, this field meant
											// "bring up the editor in plaintext
											// mode".  Well, that's already been
											// done at this point.  Now, we want
											// it to mean "convert this message
											// to plaintext on send".  Which we
											// do only if DetermineHTMLAction()
											// tells us to.
	return NS_OK;
}



nsresult nsMsgCompose::Dispose()
{
	// Don't interrupt if there's nothing to interrupt because we might lose
	// mocha messages.
	if (NET_AreThereActiveConnectionsForWindow(m_context))
		msg_InterruptContext (m_context, PR_FALSE);
	if (m_textContext != NULL)
		msg_InterruptContext(m_textContext, PR_TRUE);

/*JFD
	msg_delete_attached_files (m_attachedFiles);
*/

	PR_FREEIF(m_defaultUrl);
	PR_FREEIF(m_attachmentString);

/*JFD
	msg_free_attachment_list(m_attachData);

	if (m_print) {
		delete m_print;
		m_print = NULL;
	}
*/

#ifdef UNREADY_CODE
	HJ09384
#endif

	if (m_context)
		FE_DestroyMailCompositionContext(m_context);
	m_context = NULL;

	if (m_fields) {
		m_fields->Release();
		m_fields = NULL;
	}
	if (m_initfields) {
		m_initfields->Release();
		m_initfields = NULL;
	}
	if (m_htmlrecip) {
		delete m_htmlrecip;
		m_htmlrecip = NULL;
	}

/*JFD
	DIR_Server* pab = NULL;
	ABook * ab = NULL;
*/

	PR_FREEIF(m_quotedText);
	PR_FREEIF(m_messageId);

	return NS_OK;
}


MSG_PaneType nsMsgCompose::GetPaneType()
{
	return MSG_COMPOSITIONPANE;
}


void nsMsgCompose::NotifyPrefsChange(NotifyCode) {
	// ###tw  Write me!
}


int nsMsgCompose::CreateVcardAttachment ()
{
	nsMsgCompPrefs pCompPrefs;
	char* name;
	int status = 0;

	if (!m_haveAttachedVcard && AB_UserHasVCard() ) // don't attach a vcard if the user does not have a vcard
	{

		char * vCard = NULL;
		char * filename = NULL;
		AB_LoadIdentityVCard(&vCard);
		if (vCard)
		{
			AB_ExportVCardToTempFile (vCard, &filename);
			if (vCard)
				PR_Free(vCard); // free our allocated VCardString...
			char buf [ 2 * kMaxFullNameLength ];
			if (pCompPrefs.GetUserFullName()) 
				name = PL_strdup (pCompPrefs.GetUserFullName());
			// write out a content description string
#ifdef UNREADY_CODE
			XP_SPRINTF (buf, XP_GetString (MK_ADDR_BOOK_CARD), name);
#endif
			PR_FREEIF(name);


			char* temp = WH_FileName(filename, xpFileToPost);
			char * fileurl = NULL;
			if (temp)
			{
				fileurl = XP_PlatformFileToURL (temp);
				PR_Free(temp);
			}
			else
				return -1;	

			// Send the vCard out with a filename which distinguishes this user. e.g. jsmith.vcf
			// The main reason to do this is for interop with Eudora, which saves off 
			// the attachments separately from the message body
			char *vCardFileName = NULL;
			char *mailIdentityUserEmail = NULL;
			char *atSign = NULL;
			PREF_CopyCharPref("mail.identity.useremail", &mailIdentityUserEmail);
			if (mailIdentityUserEmail)
			{
				atSign = PL_strchr(mailIdentityUserEmail, '@');
				if (atSign) *atSign = 0;
				vCardFileName = PR_smprintf ("%s.vcf", mailIdentityUserEmail);
				PR_Free(mailIdentityUserEmail);
			}
			if (!vCardFileName)
			{
				vCardFileName = PL_strdup("vcard.vcf");
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
				PR_REALLOC(m_attachData, (datacount + 2) * sizeof(MSG_AttachmentData));
			}
			else {
				alist = (MSG_AttachmentData *)
					PR_Malloc((datacount + 2) * sizeof(MSG_AttachmentData));
			}
			if (!alist)
				return MK_OUT_OF_MEMORY;
			m_attachData = alist;
			memset (m_attachData + datacount, 0, 2 * sizeof (MSG_AttachmentData));
			m_attachData[datacount].url = fileurl;
			m_attachData[datacount].real_type = PL_strdup(vCardMimeFormat);
			m_attachData[datacount].description = PL_strdup (buf);
			m_attachData[datacount].real_name = PL_strdup (vCardFileName);
			m_attachData[datacount + 1].url = NULL;
			
			MSG_AttachedFile *aflist;
			if (filecount) {
				aflist = (struct MSG_AttachedFile *)
				PR_REALLOC(m_attachedFiles, (filecount + 2) * sizeof(MSG_AttachedFile));
			}
			else {
				aflist = (struct MSG_AttachedFile *)
					PR_Malloc((filecount + 2) * sizeof(MSG_AttachedFile));
			}

			if (!aflist)
				return MK_OUT_OF_MEMORY;

			m_attachedFiles = aflist;
			memset (m_attachedFiles + filecount, 0, 2 * sizeof (MSG_AttachedFile));
			m_attachedFiles[filecount].orig_url = origurl;
			m_attachedFiles[filecount].file_name = filename;
			m_attachedFiles[filecount].type = PL_strdup(vCardMimeFormat);
			m_attachedFiles[filecount].description = PL_strdup (buf);
			m_attachedFiles[filecount].real_name = PL_strdup (vCardFileName);
			m_attachedFiles[filecount + 1].orig_url = NULL;

			m_haveAttachedVcard = PR_TRUE;

			PR_Free(vCardFileName);
		}
	}
	return status;
}


char*
nsMsgCompose::FigureBcc(PRBool newsBcc)
{
	nsMsgCompPrefs pCompPrefs;
	char* result = NULL;
	PRBool useBcc = PR_FALSE;

	if (newsBcc)
		PREF_GetBoolPref("news.use_default_cc", &useBcc);
	else
		PREF_GetBoolPref("mail.use_default_cc", &useBcc);
		
	if (useBcc || GetPrefs()->GetDefaultBccSelf(newsBcc) != 0)
	{
		const char* tmp = useBcc ?
			GetPrefs()->GetDefaultHeaderContents(
				newsBcc ? MSG_NEWS_BCC_HEADER_MASK : MSG_BCC_HEADER_MASK) : NULL;
		if (!GetPrefs()->GetDefaultBccSelf(newsBcc)) {
			result = PL_strdup(tmp ? tmp : "");
		} else if (!tmp || !*tmp) {
			result = PL_strdup(pCompPrefs.GetUserEmail());
		} else {
			result = PR_smprintf("%s, %s", pCompPrefs.GetUserEmail(), tmp);
		}
	}
	return result;
}


const char* 
nsMsgCompose::CheckForLosingFcc(const char* fcc)
{
	if (fcc && *fcc) {
		char *q = GetPrefs()->MagicFolderName(MSG_FOLDER_FLAG_QUEUE);
		if (q && *q && !XP_FILENAMECMP (q, fcc)) {
			char *buf;

			/* We cannot allow them to use the queued mail folder
			   as their fcc folder, too.  Tell them why. */

			const char *q = MSG_GetQueueFolderName();
#ifdef UNREADY_CODE
			if (q) {
				if (!PL_strcasecmp(q,QUEUE_FOLDER_NAME_OLD))
					buf = PR_smprintf("%s%s", XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL_OLD),
												  XP_GetString(MK_MSG_NOT_AS_SENT_FOLDER));
				else buf = PR_smprintf("%s%s", XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL),
								  XP_GetString(MK_MSG_NOT_AS_SENT_FOLDER));
			}
			else buf = PR_smprintf("%s%s", XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL),
							  XP_GetString(MK_MSG_NOT_AS_SENT_FOLDER));
			if (buf) {
				FE_Alert(m_context, buf);
				PR_Free(buf);
			}
#endif

			/* Now ignore the FCC file they passed in. */
			fcc = 0;
		}
		PR_FREEIF(q);
	}
	return fcc;
}

nsresult
nsMsgCompose::GetCommandStatus(MSG_CommandType command,
										 const nsMsgViewIndex* indices,
										 PRInt32 numindices,
						   PRBool *selectable_pP,
						   MSG_COMMAND_CHECK_STATE *selected_pP,
						   const char **display_stringP,
						   PRBool *plural_pP)
{
	const char *display_string = 0;
	PRBool plural_p = PR_FALSE;
	// N.B. default is PR_TRUE, so you don't need to set it in each case
	PRBool selectable_p = PR_TRUE;	
	PRBool selected_p = PR_FALSE;
	PRBool selected_used_p = PR_FALSE;

	switch (command)
	{
	case MSG_AttachAsText:
		// the WinFE uses this for lots of update, so pretend we handle it.
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_ATTACH_AS_TEXT);
#endif
		break;
	case MSG_SendMessage:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_SEND);
#endif
        if (m_attachmentInProgress || m_deliveryInProgress)
            selectable_p = PR_FALSE;
		break;
	case MSG_SendMessageLater:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_SEND_LATER);
#endif
        if (m_attachmentInProgress || m_deliveryInProgress)
            selectable_p = PR_FALSE;
		break;
	case MSG_Save:
	case MSG_SaveDraft:
	case MSG_SaveDraftThenClose:
#ifdef UNREADY_CODE
	    display_string = XP_GetString(MK_MSG_SAVE_DRAFT);
#endif
        if (m_attachmentInProgress || m_deliveryInProgress)
            selectable_p = PR_FALSE;
		break;
	case MSG_SaveTemplate:
#ifdef UNREADY_CODE
	    display_string = XP_GetString(MK_MSG_SAVE_TEMPLATE);
#endif
        if (m_attachmentInProgress || m_deliveryInProgress)
            selectable_p = PR_FALSE;
		break;
	case MSG_Attach:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_ATTACH_ETC);
#endif
		break;

	case MSG_ShowFrom:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_FROM);
#endif
		selected_p = ShowingCompositionHeader(MSG_FROM_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowReplyTo:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_REPLY_TO);
#endif
		selected_p = ShowingCompositionHeader(MSG_REPLY_TO_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowTo:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_MAIL_TO);
#endif
		selected_p = ShowingCompositionHeader(MSG_TO_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowCC:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_MAIL_CC);
#endif
		selected_p = ShowingCompositionHeader(MSG_CC_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowBCC:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_MAIL_BCC);
#endif
		selected_p = ShowingCompositionHeader(MSG_BCC_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowFCC:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_FILE_CC);
#endif
		selected_p = ShowingCompositionHeader(MSG_FCC_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowPostTo:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_POST_TO);
#endif
		selected_p = ShowingCompositionHeader(MSG_NEWSGROUPS_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowFollowupTo:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_FOLLOWUPS_TO);
#endif
		selected_p = ShowingCompositionHeader(MSG_FOLLOWUP_TO_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowSubject:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_SUBJECT);
#endif
		selected_p = ShowingCompositionHeader(MSG_SUBJECT_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	case MSG_ShowAttachments:
#ifdef UNREADY_CODE
		display_string = XP_GetString(MK_MSG_ATTACHMENT);
#endif
		selected_p = ShowingCompositionHeader(MSG_ATTACHMENTS_HEADER_MASK);
		selected_used_p = PR_TRUE;
		break;
	default:
		selectable_p = PR_FALSE;
/*JFD
		return MSG_Pane::GetCommandStatus(command, indices, numindices,
			selectable_pP, selected_pP, display_stringP, plural_pP);
*/return 0;
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

					 
nsresult
nsMsgCompose::DoCommand(MSG_CommandType command, nsMsgViewIndex* indices,
							   PRInt32 numindices)
{
	nsresult status = 0;
	InterruptContext(PR_FALSE);
	switch (command) {
	case MSG_SendMessage:
		status = SendMessageNow();	/* ###tw Error-return-type mismatch! */
		break;
	case MSG_SendMessageLater:
		status = QueueMessageForLater();/* ###tw Error-return-type mismatch! */
		break;
	case MSG_Save:
		status = SaveMessage();
		break;
    case MSG_SaveDraft:
	case MSG_SaveDraftThenClose:
		if (command == MSG_SaveDraftThenClose)
			m_closeAfterSave = PR_TRUE;
		status = SaveMessageAsDraft(); /* ### Error-return-type mismatch! */
		break;
	case MSG_SaveTemplate:
		status = SaveMessageAsTemplate();
		break;
	case MSG_ShowPostTo:		// how to do this?
		ToggleCompositionHeader(MSG_NEWSGROUPS_HEADER_MASK);
		break;
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
/*JFD
		status = MSG_Pane::DoCommand(command, indices, numindices);
*/status=0;
		break;
	}
	return status;
}

extern "C" void FE_MsgShowHeaders(MSG_Pane *pPane, MSG_HEADER_SET mhsHeaders);

void nsMsgCompose::ToggleCompositionHeader(PRUint32 header)
{
  if (m_visible_headers & header) {
	m_visible_headers &= ~header;
  } else {
	m_visible_headers |= header;
  }
  FE_MsgShowHeaders(this, m_visible_headers);
}

PRBool 
nsMsgCompose::ShowingAllCompositionHeaders()
{
  return m_visible_headers == ALL_HEADERS;
}

PRBool 
nsMsgCompose::ShowingCompositionHeader(PRUint32 mask)
{
  return (m_visible_headers & mask) == mask;
}


int
nsMsgCompose::SetCallbacks(MSG_CompositionPaneCallbacks* callbacks,
								  void* closure)
{
	m_callbacks = *callbacks;
	m_callbackclosure = closure;
	return 0;
}

#ifdef UNREADY_CODE
HJ99161
HJ73123
#endif

void
nsMsgCompose::InitializeHeaders(MWContext* old_context, const nsIMsgCompFields* fields)
{
	nsMsgCompPrefs pCompPrefs;

	PR_ASSERT(m_fields == NULL);
	PR_ASSERT(m_initfields == NULL);

	const char *real_addr = pCompPrefs.GetUserEmail();
	char *real_return_address;
	const char* sig;
	PRBool forward_quoted;
	forward_quoted = PR_FALSE;

	m_fields = new nsMsgCompFields;
	if (!m_fields)
		return;
	m_fields->AddRef();
	if (fields)
		m_fields->Copy((nsIMsgCompFields*)fields);
	m_fields->SetOwner(this);

	m_oldContext = old_context;
	/* hack for forward quoted.  Checks the attachment field for a cookie
	   string indicating that this is a forward quoted operation.  If a cookie
	   is found, the attachment string is slid back down over the cookie.  This
	   will put the original string back in tact. */

	const char* attachment = m_fields->GetAttachments();

	if (attachment && *attachment) {
		if (!PL_strncmp(attachment, MSG_FORWARD_COOKIE,
						PL_strlen(MSG_FORWARD_COOKIE))) {
			attachment += PL_strlen(MSG_FORWARD_COOKIE);
			forward_quoted = PR_TRUE;      /* set forward with quote flag */
			m_fields->SetAttachments((char *)attachment, NULL);
			attachment = m_fields->GetAttachments();
		}
	}

	m_status = -1;

	if (MISC_ValidateReturnAddress(old_context, real_addr) < 0) {
		return;
	}

/*JFD
	real_return_address = MIME_MakeFromField(old_context->win_csid);
*/ real_return_address = (char *)pCompPrefs.GetUserEmail();/*JFD*/

/*JFD
	PR_ASSERT (m_context->type == MWContextMessageComposition);
	PR_ASSERT (XP_FindContextOfType(0, MWContextMessageComposition));
	PR_ASSERT (!m_context->msg_cframe);
*/

	PRInt32 count = m_fields->GetNumForwardURL();
	if (count > 0) {
		// if forwarding one or more messages
		PR_ASSERT(*attachment == '\0');
		MSG_AttachmentData *alist = (struct MSG_AttachmentData *)
			PR_Malloc((count + 1) * sizeof(MSG_AttachmentData));
		if (alist) {
			memset(alist, 0, (count + 1) * sizeof(*alist));
			for (count--; count >= 0; count--) {
				alist[count].url = (char*) m_fields->GetForwardURL(count);
				alist[count].real_name = (char*) m_fields->GetForwardURL(count);
			}
			SetAttachmentList(alist);
			// Don't call msg_free_attachment_list because we are not duplicating
			// url & real_name
			PR_Free(alist);;
		}
	} else if (*attachment) {
		// forwarding a single url
		// typically a web page
		MSG_AttachmentData *alist;
		count = 1;
		alist = (struct MSG_AttachmentData *)
			PR_Malloc((count + 1) * sizeof(MSG_AttachmentData));
		if (alist) {
			memset(alist, 0, (count + 1) * sizeof(*alist));
			alist[0].url = (char *)attachment;
			alist[0].real_name = (char *)attachment;
			SetAttachmentList(alist);
			// Don't call msg_free_attachment_list because we are not duplicating
			// url & real_name
			PR_Free(alist);
		}
	}	// else if (*attachment)

	if (*attachment) {
		if (*attachment != '(') {
			m_defaultUrl = PL_strdup(attachment);
		}
	}
	else if (old_context) {
		History_entry *h = SHIST_GetCurrent(&old_context->hist);
		if (h && h->address) {
			m_defaultUrl = PL_strdup(h->address);
		}
		if (m_defaultUrl)
		{
			MSG_Pane *msg_pane = MSG_FindPane(old_context,
											  MSG_MESSAGEPANE);
			if (msg_pane)
				m_fields->SetHTMLPart((char *)msg_pane->GetHTMLPart(), NULL);
		}
	}

	if (!*m_fields->GetFrom()) {
		m_fields->SetFrom(real_return_address, NULL);
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


#ifdef UNREADY_CODE
	HJ77855
#endif

	if (!*m_fields->GetOrganization()) {
		m_fields->SetOrganization((char *)pCompPrefs.GetOrganization(), NULL);
	}

	if (!*m_fields->GetReplyTo()) {
		m_fields->
			SetReplyTo((char *)GetPrefs()->
					   GetDefaultHeaderContents(MSG_REPLY_TO_HEADER_MASK), NULL);
	}
	if (!*m_fields->GetFcc()) 
	{
		PRBool useDefaultFcc = PR_TRUE;
		/*int prefError =*/ PREF_GetBoolPref(*newsgroups ? "news.use_fcc" : "mail.use_fcc",
											&useDefaultFcc);
		if (useDefaultFcc)
		{
			m_fields->SetFcc((char *)GetPrefs()->
				GetDefaultHeaderContents(*newsgroups ? 
					 MSG_NEWS_FCC_HEADER_MASK : MSG_FCC_HEADER_MASK), NULL);
		}
	}
	if (!*m_fields->GetBcc()) {
		char* bcc = FigureBcc(*newsgroups);
		m_fields->SetBcc(bcc, NULL);
		PR_FREEIF(bcc);
	}

	m_fields->SetFcc((char *)CheckForLosingFcc(m_fields->GetFcc()), NULL);

	{
		const char *body = m_fields->GetDefaultBody();
		if (body && *body)
		{
			m_fields->AppendBody((char *)body);
			m_fields->AppendBody(LINEBREAK);
			/* m_bodyEdited = PR_TRUE; */
		}
	}

	sig = FE_UsersSignature ();
	if (sig && *sig) {
	    m_fields->AppendBody(LINEBREAK);
		/* If the sig doesn't begin with "--" followed by whitespace or a
		   newline, insert "-- \n" (the pseudo-standard sig delimiter.) */
		if (sig[0] != '-' || sig[1] != '-' ||
			(sig[2] != ' ' && sig[2] != CR && sig[2] != LF)) {
			m_fields->AppendBody("-- " LINEBREAK);
		}
		m_fields->AppendBody((char *)sig);
	}

	PR_FREEIF (real_return_address);


	if (m_context)
		FE_SetDocTitle(m_context, (char*) GetWindowTitle());


	m_initfields = new nsMsgCompFields;
	if (m_initfields) {
		m_initfields->AddRef();
		m_fields->Copy((nsIMsgCompFields*)m_fields);
		m_initfields->SetOwner(this);
	}
}


PRBool nsMsgCompose::ShouldAutoQuote() {
      if (m_haveQuoted) return PR_FALSE;
	if (m_replyType == MSG_ForwardMessageQuoted ||
		    GetPrefs()->GetAutoQuoteReply()) {
		switch (m_replyType) {
		case MSG_ForwardMessageQuoted:
		case MSG_PostAndMailReply:
		case MSG_PostReply:
		case MSG_ReplyToAll:
		case MSG_ReplyToSender:
			return PR_TRUE;

        default:
            break;
		}
	}
	return PR_FALSE;
}




const char* nsMsgCompose::GetDefaultURL() {
  return m_defaultUrl;
}

void nsMsgCompose::SetDefaultURL(const char *defaultURL, 
										const char *htmlPart)
{
	PR_FREEIF(m_defaultUrl);
	PR_FREEIF(m_quotedText);
	if (defaultURL)
		m_defaultUrl = PL_strdup(defaultURL);
	m_fields->SetHTMLPart((char *)htmlPart, NULL);
}



nsIMsgCompFields* nsMsgCompose::GetInitialFields()
{
	return (nsIMsgCompFields*)m_initfields;
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


MSG_HEADER_SET nsMsgCompose::GetInterestingHeaders()
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
		 ? !!PL_strcmp (reply_to, default_reply_to) : PR_TRUE))
		desired_mask |= MSG_REPLY_TO_HEADER_MASK;

	/* (see above.) */
	const char* bcc = m_fields->GetBcc();
	const char* default_bcc =
		GetPrefs()->GetDefaultHeaderContents(MSG_BCC_HEADER_MASK);
	if (bcc && *bcc &&
		((default_bcc && *default_bcc)
		 ? !!PL_strcmp (bcc, default_bcc) : PR_TRUE))
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
		(newsgroups ? PL_strcmp (followup_to, newsgroups) : PR_TRUE))
		desired_mask |= MSG_FOLLOWUP_TO_HEADER_MASK;

	return desired_mask;
}


void
nsMsgCompose::GetUrlDone_S(PrintSetup* pptr)
{
/*JFD
  ((nsMsgCompose*) (pptr->carg))->GetUrlDone(pptr);
*/
}


#define QUOTE_BUFFER_SIZE		10240

void
nsMsgCompose::GetUrlDone(PrintSetup* /*pptr*/)
{
	XP_File file=(XP_File)nsnull;
	PR_FREEIF(m_quoteUrl);
	m_textContext = NULL;  /* since this is called as a result of
							  TXFE_AllConnectionsComplete, we know this context
							  is going away by natural means */
	int bufSize = QUOTE_BUFFER_SIZE;

/*JFD
	XP_FileClose(m_print->out);
*/
	XP_StatStruct stat;
	char* curquote = NULL;
	PRInt32 replyOnTop = 0, replyWithExtraLines = 0;

	PREF_GetIntPref("mailnews.reply_on_top", &replyOnTop);
	PREF_GetIntPref("mailnews.reply_with_extra_lines", &replyWithExtraLines);

	PRInt32 extra = (m_markup ? 0 : 
	               (replyWithExtraLines ? LINEBREAK_LEN * replyWithExtraLines
					: 0));

/*JFD
	if (XP_Stat(m_print->filename, &stat, xpTemporary) == 0) */{
		m_quotedText = (char*) PR_Malloc(stat.st_size + 1 + extra);
		
		/* Insert two line break at the begining of the quoted text */
		if (!m_quotedText) return;

		curquote = m_quotedText;

		if (!m_markup && extra && replyOnTop == 1) {
		  for (; replyWithExtraLines > 0; replyWithExtraLines--) {
			PL_strcpy(curquote, LINEBREAK);
			curquote += LINEBREAK_LEN;
			if (m_quotefunc)
				(*m_quotefunc)(m_quoteclosure, LINEBREAK);
		  }
		}
	}

	/* Open hateful temporary file as input  */
/*JFD
	file = XP_FileOpen (m_print->filename, xpTemporary, XP_FILE_READ);
*/
	if (file) {
		char* buf = NULL;
		while (!buf && (bufSize >= 512))
		{
			buf = (char*)PR_Malloc(bufSize + 1);
			if (!buf)
				bufSize /= 2;
		}
		if (buf) {
			PRInt32 bufferLen;
			CCCDataObject conv;
			int doConv;
			INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_context);
			PRInt16 win_csid = INTL_GetCSIWinCSID(c);

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
					PR_ASSERT(curquote + bufferLen <= m_quotedText + stat.st_size + extra);
					if (curquote + bufferLen <= m_quotedText + stat.st_size + extra) {
						PL_strcpy(curquote, newBuf);
						curquote += bufferLen;
					}
				}

				if (newBuf != buf) {
					PR_Free(newBuf);
				}
			}

			if (!m_markup && extra && replyOnTop == 0) {
			  for (; replyWithExtraLines > 1; replyWithExtraLines--) {
				PL_strcpy(curquote, LINEBREAK);
				curquote += LINEBREAK_LEN;
				if (m_quotefunc)
					(*m_quotefunc)(m_quoteclosure, LINEBREAK);
			  }
			}

			PR_Free(buf);
			if (conv) {
				INTL_DestroyCharCodeConverter(conv);
			}
		}
		XP_FileClose(file);
	}
	if (curquote) *curquote = '\0';
	m_cited = PR_TRUE;
/*JFD
	XP_FileRemove(m_print->filename, xpTemporary);
	PR_FREEIF(m_print->filename);
*/
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
	static PRInt32 QuoteLine_s(char* line, PRUint32 line_length, void* closure);
	PRInt32 QuoteLine(char* line, PRUint32 line_length);
protected:
	MWContext* m_context;
	char* m_buffer;
	PRUint32 m_size;
	PRUint32 m_fp;
	PRBool m_insertedpre;
	char* m_outbuf;
	PRUint32 m_outbufsize;
	PRUint32 m_maxLineWidth;
	PRInt32 m_replyOnTop;
	PRInt32 m_replyWithExtraLines;
};


static int
MyQuoteFunc(void* closure, const char* data)
{
	return ((QuotePlainIntoHTML*) closure)->DoQuote(data);
}





QuotePlainIntoHTML::QuotePlainIntoHTML(MWContext* context)
{
	m_context = context;
	if (EDT_PasteQuoteBegin(m_context, PR_TRUE) != EDT_COP_OK) {
		m_context = NULL;
	}
	PREF_GetIntPref("mailnews.reply_on_top", &m_replyOnTop);
	PREF_GetIntPref("mailnews.reply_with_extra_lines", &m_replyWithExtraLines);

	m_buffer = NULL;
	m_size = 0;
	m_fp = 0;
	m_insertedpre = PR_FALSE;
	m_outbuf = NULL;
	m_outbufsize = 0;
	m_maxLineWidth = 0;
}

QuotePlainIntoHTML::~QuotePlainIntoHTML()
{
	PR_FREEIF(m_buffer);
	delete [] m_outbuf;
}



int
QuotePlainIntoHTML::DoQuote(const char* data)
{
	if (data) {
		if (!m_context) return 0;
		return (int)msg_LineBuffer(data, PL_strlen(data), &m_buffer, &m_size, &m_fp, PR_FALSE, 
#ifdef XP_OS2
					(PRInt32 (_Optlink*) (char*,PRUint32,void*))
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

			nsMsgCompose *cpane = (nsMsgCompose *)
				MSG_FindPane(m_context, MSG_COMPOSITIONPANE);
			if (cpane)
				cpane->SetLineWidth(m_maxLineWidth);
		}
		delete this;
		return 0;
	}
}


PRInt32
QuotePlainIntoHTML::QuoteLine_s(char* line, PRUint32 length, void* closure)
{
	return ((QuotePlainIntoHTML*)closure)->QuoteLine(line, length);
}

PRInt32
QuotePlainIntoHTML::QuoteLine(char* line, PRUint32 length)
{
	if (length > m_maxLineWidth)
		m_maxLineWidth = length;

	if (length >= 2 && line[0] == '>' && line[1] == ' ') {
		line += 2;
		length -= 2;
		if (!m_insertedpre) {
			EDT_PasteQuote(m_context, "<BLOCKQUOTE TYPE=CITE><PRE NSCISAW>");
			m_insertedpre = PR_TRUE;
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
//JFD		NET_ScanForURLs(NULL, line, length, m_outbuf, m_outbufsize, PR_TRUE);
		EDT_PasteQuote(m_context, m_outbuf);
	}
	return 0;
}


void
nsMsgCompose::QuoteHTMLDone_S(URL_Struct* url, int /*status*/, MWContext* /*context*/)
{
      nsMsgCompose *pane = (nsMsgCompose *) url->fe_data;
      if (pane) {
          PR_FREEIF(pane->m_quoteUrl);
		  pane->m_quotefunc = NULL;
		  if (pane->m_quoteclosure) {
			  delete pane->m_quoteclosure;
			  pane->m_quoteclosure = NULL;
		  }
	  }
      NET_FreeURLStruct(url);
}



nsresult nsMsgCompose::QuoteMessage(int (*func)(void* closure,
													 const char* data),
										 void* closure)
{
	nsresult status = 0;
	char* ptr;
	 m_haveQuoted = PR_TRUE;
	if (!m_defaultUrl) return 0; /* Nothing to quote. */
	if (m_quoteUrl) return 0;	/* Currently already quoting! */

	PR_ASSERT(m_quotefunc == NULL);

	if (m_markup) {
		func = MyQuoteFunc;
		closure = new QuotePlainIntoHTML(GetContext());
		if (!closure) return MK_OUT_OF_MEMORY;
	}

	MSG_Pane *msgPane = FindPane(m_oldContext, MSG_MESSAGEPANE);

	char* htmlpart;
	
	if (msgPane && msgPane->GetQuoteUrl())
	{
		if (msgPane->GetQuoteHtmlPart())
		{
			htmlpart = PL_strdup(msgPane->GetQuoteHtmlPart());
			msgPane->SetQuoteHtmlPart(NULL);
		}
		else
		{
			htmlpart = NULL;
		}
	}
	else
	{
		htmlpart = m_fields->GetHTMLPart() ? PL_strdup(m_fields->GetHTMLPart()) : NULL;
	}

	PRBool quotehtml = (m_markup && htmlpart != NULL && *htmlpart != '\0');

	if (m_quotedText) {
		if (func) {
#ifdef	EXTRA_QUOTE_BEGIN
			if (m_markup) {
				if (EDT_PasteQuoteBegin(GetContext(),
										PR_TRUE) != EDT_COP_OK) {
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
  

	if (msgPane && msgPane->GetQuoteUrl())
	{
		m_quoteUrl = PL_strdup(msgPane->GetQuoteUrl());
		msgPane->SetQuoteUrl(NULL);
	}
	else
		m_quoteUrl = PL_strdup(m_defaultUrl);
	if (!m_quoteUrl) return eOUT_OF_MEMORY;

	/* remove any position information from the url
	 */
	ptr = PL_strchr(m_quoteUrl, '#');
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
		url->allow_content_change = PR_TRUE;
/*JFD
		MSG_UrlQueue::AddUrlToPane (url, QuoteHTMLDone_S, this, PR_TRUE, FO_QUOTE_HTML_MESSAGE);
*/
		PR_FREEIF(htmlpart);
		return 0;
	}
		

	XL_InitializeTextSetup(m_print);
/*JFD
	m_print->out = NULL;
	m_print->prefix = "> ";
*/
	if (m_markup) {
		if (htmlpart && *htmlpart) {
			// Quoting html into html
			// We are kind of doomed on this case when force sending plain text
			// if there are hard line breaks.
/*JFD
			m_print->width = 999;
*/
		}
		else {
			// Quoting plain text into html; there are hard line breaks
			// We should not reformat the line when force sending plain text
			// message
			// This is done via recording the max line width when we quote each plain
			// text line and then use the max line width if it is greater than default
			// wraplinewidth. This prevent from reformatting the qutoed line in an 
			// unwanted fashion.
/*JFD
			m_print->width = 998;
*/
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
			PRInt32 width = 72;
			PREF_GetIntPref("mailnews.wraplength", &width);
			if (width == 0) width = 72;
			else if (width < 10) width = 10;
			else if (width > 30000) width = 30000;
/*JFD
			m_print->width = width - 2;
*/		}
		else {
			// We are quoting plain text message.to plain text
			// We shouldn't reformat the original message since everyline already
			// has hard line break <CR><LF> we simply set the m_print->width to 999.
/*JFD
			m_print->width = 997;
*/
		}
		// m_print->width = 70; // The default window is 72 wide; subtract 2 for "> ".
	}
	PR_FREEIF(htmlpart);
/*JFD
	m_print->carg = this;
	m_print->url = NET_CreateURLStruct(m_quoteUrl, NET_DONT_RELOAD);
	if (!m_print->url) {
		status = eOUT_OF_MEMORY;
		goto FAIL;
	}
	m_print->url->position_tag = 0;
	m_print->completion = nsMsgCompose::GetUrlDone_S;
	m_print->filename = WH_TempName(xpTemporary, "ns");
	if (!m_print->filename) {
		status = eOUT_OF_MEMORY;
		goto FAIL;
	}
	m_print->out = XP_FileOpen(m_print->filename, xpTemporary, XP_FILE_WRITE);
	if (!m_print->out) {
		status = 9999;				// ###tw   Need the right error code! 
		goto FAIL;
	}
	m_print->cx = m_context;
*/
	m_exitQuoting = NULL;
	m_dummyUrl = NET_CreateURLStruct("about:", NET_DONT_RELOAD);
	m_dummyUrl->internal_url = PR_TRUE;
	if (m_dummyUrl) {
		FE_SetWindowLoading(m_context, m_dummyUrl, &m_exitQuoting);
		PR_ASSERT(m_exitQuoting != NULL);
	}

	/* Start the URL loading... (msg_get_url_done gets called later.) */


/*JFD
	m_textContext = (MWContext*) XL_TranslateText(m_context, m_print->url,
	m_print);
*/
	// ###tw  I'm not at all sure this cast is the
	// right thing to do here...

	return 0;
FAIL:
/*JFD
	PR_FREEIF(m_print->filename);
*/
	PR_FREEIF(m_quoteUrl);
/*JFD
	if (m_print->out) {
		XP_FileClose(m_print->out);
		m_print->out = NULL;
	}
	if (m_print->url) {
		NET_FreeURLStruct(m_print->url);
		m_print->url = NULL;
	}
*/
	return status;
}


int
nsMsgCompose::PastePlaintextQuotation(const char* str)
{
	if (str && *str) {
		if (EDT_PasteQuoteBegin(m_context, PR_TRUE) != EDT_COP_OK) {
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
nsMsgCompose::SetAttachmentList(struct MSG_AttachmentData* list)
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
			PR_Malloc((count + 1) * sizeof(MSG_AttachmentData));
		if (!m_attachData) {
#ifdef UNREADY_CODE
			FE_Alert(m_context, XP_GetString(MK_OUT_OF_MEMORY));
#endif
			return MK_OUT_OF_MEMORY;
		}

		memset(m_attachData, 0, (count + 1) * sizeof(MSG_AttachmentData));
	}

	if (count > 0) {
		for (tmp = list, tmp2 = m_attachData; tmp->url; tmp++, tmp2++) {
			tmp2->url = PL_strdup(tmp->url);
			if (tmp->desired_type) {
				tmp2->desired_type = PL_strdup(tmp->desired_type);
			}
			if (tmp->real_type) {
				tmp2->real_type = PL_strdup(tmp->real_type);
			}
			if (tmp->real_encoding) {
				tmp2->real_encoding = PL_strdup(tmp->real_encoding);
			}
			if (tmp->real_name) {
				tmp2->real_name = PL_strdup(tmp->real_name);
			}
			if (tmp->description) {
				tmp2->description = PL_strdup(tmp->description);
			}
			if (tmp->x_mac_type) {
				tmp2->x_mac_type = PL_strdup(tmp->x_mac_type);
			}
			if (tmp->x_mac_creator) {
				tmp2->x_mac_creator = PL_strdup(tmp->x_mac_creator);
			}
		}
	}
	status = DownloadAttachments();
	return status;
}

PRBool
nsMsgCompose::NoPendingAttachments() const
{
	return (m_pendingAttachmentsCount == 0);
}

const struct MSG_AttachmentData *
nsMsgCompose::GetAttachmentList()
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
		PR_Free((char*) tmp->url);
		if (tmp->desired_type) PR_Free((char*) tmp->desired_type);
		if (tmp->real_type) PR_Free((char*) tmp->real_type);
		if (tmp->real_encoding) PR_Free((char*) tmp->real_encoding);
		if (tmp->real_name) PR_Free((char*) tmp->real_name);
		if (tmp->description) PR_Free((char*) tmp->description);
		if (tmp->x_mac_type) PR_Free((char*) tmp->x_mac_type);
		if (tmp->x_mac_creator) PR_Free((char*) tmp->x_mac_creator);
	}
	PR_FREEIF(list);
}



/* Whether the given saved-attachment-file thing is a match for the given
   URL (in source and type-conversion.)
 */
static PRBool
msg_attachments_match (MSG_AttachmentData *attachment,
					   MSG_AttachedFile *file)
{
	const char *dt;
	PR_ASSERT(attachment && file);
	if (!attachment || !file) return PR_FALSE;
	PR_ASSERT(attachment->url && file->orig_url);
	if (!attachment->url || !file->orig_url) return PR_FALSE;

	PR_ASSERT(file->type);
	if (!file->type) return PR_FALSE;
	PR_ASSERT(file->file_name);
	if (PL_strcmp(attachment->url, file->orig_url)) return PR_FALSE;

	/* If the attachment has a conversion type specified (and it's not the
	   "no conversion" type) then this is only a match if the saved document
	   ended up with that type as well.
	   */
	dt = ((attachment->desired_type && *attachment->desired_type)
		  ? attachment->desired_type
		  : 0);
	if (dt && !PL_strcasecmp(dt, TEXT_HTML))
		dt = 0;

	/* dt only has a value if it's "not `As Is', ie, text/plain or app/ps. */
	if (dt && PL_strcmp(dt, file->type))
		return PR_FALSE;

	return PR_TRUE;
}


int
nsMsgCompose::DownloadAttachments()
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
	// PR_ASSERT(!m_deliveryInProgress);

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
		PRBool match = PR_FALSE;
		for (tmp = m_attachData; tmp && tmp->url; tmp++) {
			if (msg_attachments_match(tmp, tmp2)) {
				match = PR_TRUE;
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
				PR_Free(tmp2->file_name);
			}
			PR_FREEIF(tmp2->orig_url);
			PR_FREEIF(tmp2->type);
			PR_FREEIF(tmp2->encoding);
			PR_FREEIF(tmp2->description);
			PR_FREEIF(tmp2->x_mac_type);
			PR_FREEIF(tmp2->x_mac_creator);
			PR_FREEIF(tmp2->real_name);
		  
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
			PR_Malloc(sizeof(MSG_AttachmentData) * (new_download_count + 1));
		if (!downloads) {
#ifdef UNREADY_CODE
			FE_Alert(m_context, XP_GetString(MK_OUT_OF_MEMORY));
#endif
			return MK_OUT_OF_MEMORY;
		}
		memset(downloads, 0, sizeof(*downloads) * (new_download_count + 1));

		dfp = downloads;
		for (tmp = m_attachData; tmp && tmp->url; tmp++) {
			PRBool match = PR_FALSE;
			if (m_attachedFiles)
				for (tmp2 = m_attachedFiles; tmp2->orig_url; tmp2++) {
					if (msg_attachments_match(tmp, tmp2)) {
						match = PR_TRUE;
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
		// PR_ASSERT(!m_deliveryInProgress);
		PR_ASSERT(!m_attachmentInProgress);
		m_attachmentInProgress = PR_TRUE;
		FE_UpdateCompToolbar (this);
/*JFD
		returnValue = msg_DownloadAttachments(this, this, downloads,
#ifdef XP_OS2
							(void (_Optlink*) (MWContext*,void*,int,const char*,MSG_AttachedFile*))
#endif
							   nsMsgCompose::DownloadAttachmentsDone_S);
*/
		PR_Free(downloads);
	}
	return returnValue;
}

void 
nsMsgCompose::DownloadAttachmentsDone_S(MWContext *context, 
											   void *fe_data,
											   int status,
											   const char *error_message,
											   struct MSG_AttachedFile *attachments)
{
	((nsMsgCompose*) fe_data)->DownloadAttachmentsDone(context, status,
															  error_message,
															  attachments);
}

void
nsMsgCompose::DownloadAttachmentsDone(MWContext* context, int status,
											 const char* error_message,
											 struct MSG_AttachedFile *attachments)
{
	PR_ASSERT(context == m_context);

	int i, old_count = 0;
	int new_count = 0;
	struct MSG_AttachedFile *tmp;
	MSG_AttachedFile *newd;

	// *** Relax the rule a little bit to enable resume downloading at
	// *** send time.
	// PR_ASSERT(!m_deliveryInProgress);
	if (m_attachmentInProgress) {
		m_attachmentInProgress = PR_FALSE;
		FE_UpdateCompToolbar (this);
	}

	if (status < 0) goto FAIL;

	status = MK_INTERRUPTED;
	if (!m_attachData) goto FAIL;


	if (m_attachedFiles) {
		for (tmp = m_attachedFiles; tmp->orig_url; tmp++) old_count++;
	}
	if (attachments) {
		for (tmp = attachments; tmp->orig_url; tmp++) new_count++;
	}

	if (old_count + new_count == 0) goto FAIL;
	newd = (MSG_AttachedFile *)
		PR_REALLOC(m_attachedFiles,
				   ((old_count + new_count + 1)
					* sizeof(MSG_AttachedFile)));

	if (!newd) {
		status = MK_OUT_OF_MEMORY;
#ifdef UNREADY_CODE
		error_message = XP_GetString(status);
#endif
		goto FAIL;
	}
	m_attachedFiles = newd;

	memcpy(newd + old_count,
			  attachments,
			  sizeof(MSG_AttachedFile) * (new_count + 1));

	// memcpy doesn't allocate string, so do it
	for(i=0; i<new_count; i++)
	{
		if (attachments[i].orig_url)
			newd[old_count+i].orig_url = PL_strdup(attachments[i].orig_url);
		if (attachments[i].file_name)
			newd[old_count+i].file_name = PL_strdup(attachments[i].file_name);
		if (attachments[i].type)
			newd[old_count+i].type = PL_strdup(attachments[i].type);
		if (attachments[i].encoding)
			newd[old_count+i].encoding = PL_strdup(attachments[i].encoding);
		if (attachments[i].description)
			newd[old_count+i].description = PL_strdup(attachments[i].description);
		if (attachments[i].x_mac_type)
			newd[old_count+i].x_mac_type = PL_strdup(attachments[i].x_mac_type);
		if (attachments[i].x_mac_creator)
			newd[old_count+i].x_mac_creator = PL_strdup(attachments[i].x_mac_creator);
		if (attachments[i].real_name)
			newd[old_count+i].real_name = PL_strdup(attachments[i].real_name);
	}

	PR_ASSERT (m_pendingAttachmentsCount >= new_count);
	m_pendingAttachmentsCount -= new_count;
	if (m_deliveryInProgress) {
		m_deliveryInProgress = PR_FALSE;
		DoneComposeMessage(m_deliver_mode);
	}
	return;

FAIL:
	PR_ASSERT(status < 0);
	if (error_message) {
		FE_Alert(context, error_message);
	}
	else if (status != MK_INTERRUPTED) {
		char *errmsg;
#ifdef UNREADY_CODE
		errmsg = PR_smprintf(XP_GetString(MK_COMMUNICATIONS_ERROR), status);
		if (errmsg) {
			FE_Alert(context, errmsg);
			PR_Free(errmsg);
		}
#endif
	}
}


/* How many implementations of this are there now?  4? */
static void
msg_mid_truncate_string (const char *input, char *output, int max_length)
{
	int L = PL_strlen(input);
	if (L <= max_length) {
		memcpy(output, input, L+1);
    } else {
		int mid = (max_length - 3) / 2;
		char *tmp = 0;
		if (input == output) {
			tmp = output;
			output = (char *) PR_Malloc(max_length + 1);
			*tmp = 0;
			if (!output) return;
		}
		memcpy(output, input, mid);
		memcpy(output + mid, "...", 3);
		memcpy(output + mid + 3, input + L - mid, mid + 1);

		if (tmp) {
			memcpy(tmp, output, max_length + 1);
			PR_Free(output);
		}
    }
}


char *
nsMsgCompose::GetAttachmentString()
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

	PR_FREEIF(m_attachmentString);

	m_attachmentString =
		(char *) PR_Malloc(count * (chars_per_attachment + 3) + 20);
	if (!m_attachmentString) return 0;
	*m_attachmentString = 0;

	for (tmp = m_attachData ; tmp && tmp->url ; tmp++) {
		const char *url = tmp->real_name ? tmp->real_name : tmp->url;
		const char *ptr = PL_strchr(url, ':');
		char *result = 0;

		if (!ptr) {
			/* No colon?  Must be a file name. */
			ptr = url;
			goto DO_FILE;
		}

		if (!PL_strncmp(url, "news:", 5) ||
			!PL_strncmp(url, "snews:", 6) ||
			!PL_strncmp(url, "IMAP:", 5) ||
			!PL_strncmp(url, "mailbox:", 8)) {
			/* ###tw Unfortunately, I don't think this stuff quite ports
			   directly to the new world, so I'm gonna disable it for now... */
#ifdef NOTDEF /* ###tw */
			MWContext *c = XP_FindContextOfType (0,
												 (PL_strncmp(url, "mailbox:",
															 8)
												  ? MWContextNews
												  : MWContextMail));
			MSG_FolderPane* folderpane = NULL;
			if (c) {
				folderpane = MSG_Pane::FindPane(c, MSG_FolderPane);
			}

			if (folderpane) {
				/* OK!  This is a news/mail message, and there is a news/mail
				   context around.  So go look for it in the
				   currently-displayed thread list.  */



				struct MSG_ThreadEntry *msg;
				char *s = PL_strdup(ptr + 1);
				char *id = s;
				if (!s) goto DONE;
				if (!PL_strncmp(url, "mailbox:", 8)) {
					char *s2;
					id = PL_strstr(id, "?id=");
					if (!id) id = PL_strstr(id, "&id=");
					if (!id) goto DONE;
					id += 4;
					s2 = PL_strchr(id, '&');
					if (s2) *s2 = 0;
					NET_UnEscape (id);
				} else {
					char *s2 = PL_strrchr(id, '/');
					if (s2) id = s2+1;
					s2 = PL_strchr(id, '?');
					if (s2) *s2 = 0;
					NET_UnEscape (id);
				}

				msg = (MSG_ThreadEntry *)
					XP_Gethash (c->msgframe->msgs->message_id_table, id, 0);
				PR_Free(s);
				if (!msg) goto DO_FOLDER;

				/* Found an entry in the thread list!  So now we know the
				   subject.  Display that. */
				s = c->msgframe->msgs->string_table[msg->subject];
				if (s && *s) {
					char *conv_subject;
					conv_subject = IntlDecodeMimePartIIStr(s,
														   m_context->win_csid,
														   PR_FALSE);
					if (conv_subject == NULL) conv_subject = (char *) s;
					result = (char *) PR_Malloc (PL_strlen(conv_subject) + 10);
					if (!result) goto DONE;
					PL_strcpy(result, "\"");
					if (msg->flags & MSG_FLAG_HAS_RE) {
						PL_strcat(result, "Re: ");
					}
					PL_strcat(result, conv_subject);
					PL_strcat(result, "\"");
					if (conv_subject != s) {
						PR_Free(conv_subject);
					}
					goto DONE;
				}
			}

			DO_FOLDER:

#endif /* NOTDEF   ###tw */


#if 0
			/* Ok, we couldn't find this message in the thread list, maybe
			   because a different folder is now selected, or because the
			   mail/news window has been deleted.  So, display a folder name
			   instead.
			   */
			if (!PL_strncmp(url, "mailbox:", 8)) {
			}
#endif

			goto DONE;
		}

		/* Ok, so it must be something vaguely file-name-like.
		   Look for a slash.
		   */
	DO_FILE:
		{
			char *ptr2 = PL_strdup(ptr);
			if (!ptr2) goto DONE;
			char* s = PL_strchr(ptr2, '?');
			if (s) *s = 0;
			s = PL_strchr(ptr2, '#');
			if (s) *s = 0;
			s = PL_strrchr(ptr2, '/');
			if(!s) {
				PR_Free(ptr2);
				goto DONE;
			}
			s++;
			if (!*s || !PL_strcasecmp(s,"index.html") ||
				  !PL_strcasecmp(s,"index.htm")) {
				/* This had a useless file name; take the last directory name. */
				char *s2 = s-1;
				if (*s2 == '/') s2--;
				while (s2 > ptr2 && *s2 != '/') s2--;
				if (*s2 == ':' || *s2 == '/') s2++;
				result = (char *) PR_Malloc (s - s2 + 1);
				memcpy (result, s2, s - s2);
				result[s - s2] = 0;
			} else {
				/* The file name is ok; use it. */
				result = PL_strdup (s);
			}
			NET_UnEscape (result);
			PR_Free(ptr2);
			goto DONE;
		}

	DONE:
		if (tmp != m_attachData) {
			PL_strcat(m_attachmentString, "; ");
		}

		if (!result) {
			if (!PL_strncmp(url, "news:", 5) ||
				!PL_strncmp(url, "snews:", 6) ||
				!PL_strncmp(url, "IMAP:", 5) ||
				!PL_strncmp(url, "mailbox:", 8)) {
				result = PL_strdup("<message>");
			} else {
				result = PL_strdup(url);
			}
			if (!result) break;
		}

		msg_mid_truncate_string(result,
								(m_attachmentString +
								 PL_strlen(m_attachmentString)),
								chars_per_attachment);
		PR_Free(result);
	}

	return m_attachmentString;
}


char*
nsMsgCompose::UpdateHeaderContents(MSG_HEADER_SET which_header,
										  const char* value)
{
  switch (which_header) {
	case MSG_TO_HEADER_MASK:
	case MSG_CC_HEADER_MASK:
	case MSG_BCC_HEADER_MASK:
    case MSG_REPLY_TO_HEADER_MASK:
#ifndef MOZ_NEWADDR   // mscott: hmm I wonder if we need a new AB version of this....I'm not sure what it's doing just yet...
/*JFD
	  ABook* addressbook = FE_GetAddressBook(this);
	  if (addressbook) {
		return AB_ExpandHeaderString(addressbook, value, PR_FALSE);
	  }
*/
#endif
	  break;
  }
  return NULL;
}

PRInt32 nsMsgCompose::SetCompHeader(MSG_HEADER_SET header,
								   const char *value)
{
	PRInt32 retVal;

	PR_ASSERT(header != MSG_ATTACHMENTS_HEADER_MASK); // "Don't use this fuction for setting the attachment!");
	ClearCompositionMessageID();
	m_fields->SetHeader(header, (char *) value, &retVal);

	return retVal;
}

const char* nsMsgCompose::GetCompHeader(MSG_HEADER_SET header)
{
	if (header == MSG_ATTACHMENTS_HEADER_MASK) {
		return (char *)NULL/*JFD GetAttachmentString()*/;
	} else {
		if (m_fields) {
			char * retVal;
			m_fields->GetHeader(header, &retVal);
			return retVal;
		} else
			return (char *)NULL;
	}
}

PRInt32 nsMsgCompose::SetCompBoolHeader(MSG_BOOL_HEADER_SET header,
									   PRBool bValue)
{
	PRInt32 retVal;

	m_fields->SetBoolHeader(header, bValue, &retVal);
	return retVal;
}

PRBool nsMsgCompose::GetCompBoolHeader(MSG_BOOL_HEADER_SET header)
{
	return m_fields ? m_fields->GetBoolHeader(header) : PR_FALSE;
}

const char* nsMsgCompose::GetCompBody()
{
	return m_fields ? m_fields->GetBody() : (char *)NULL;
}

int nsMsgCompose::SetCompBody(const char* value)
{
	return m_fields->SetBody((char *) value, NULL);
}

const char*
nsMsgCompose::GetWindowTitle()
{
	const char *s;

	if (*m_fields->GetSubject()) {
		s = m_fields->GetSubject();
	} else if (*m_fields->GetTo()) {
		s = m_fields->GetTo();
	} else if (*m_fields->GetNewsgroups()) {
		s = m_fields->GetNewsgroups();
	} else {
#ifdef UNREADY_CODE
		s = XP_GetString(MK_MSG_MSG_COMPOSITION);
#else
		s = PL_strdup("not implemented");
#endif
	}
	return s;
}


PRBool nsMsgCompose::SanityCheckNewsgroups (const char *newsgroups)
{
	// This function just does minor syntax checking on the names of newsgroup 
	// to make sure they conform to Son Of 1036: 
	// http://www.stud.ifi.uio.no/~larsi/notes/son-of-rfc1036.txt
	//
	// It isn't really possible to tell whether the group actually exists,
	// so we're not going to try.
	//
	// According to the URL given above, uppercase characters A-Z are not
	// allowed in newsgroup names, but when we tried to enforce that, we got
	// bug reports from people who were trying to post to groups with capital letters

	PRBool valid = PR_TRUE;
	if (newsgroups)
	{
		while (*newsgroups && valid)
		{
			if (!(*newsgroups >= 'a' && *newsgroups <= 'z') && !(*newsgroups >= 'A' && *newsgroups <= 'Z'))
			{
				if (!(*newsgroups >= '0' && *newsgroups <= '9'))
				{
					switch (*newsgroups)
					{
					case '+':
					case '-':
					case '/':
					case '_':
					case '=':
					case '?':
					case '.':
						break; // valid char
					case ' ':
					case ',':
						break; // ok to separate names in list
					default:
						valid = PR_FALSE;
					}
				}
			}
			newsgroups++;
		}
	}
	return valid;
}


int
nsMsgCompose::SanityCheck(int skippast)
{
	const char* body = m_fields->GetBody();
	const char* sub = m_fields->GetSubject();
#ifdef UNREADY_CODE
	HJ85617
#endif
	const char *newsgroups = NULL;

	if (skippast == MK_MSG_DOUBLE_INCLUDE) goto AFTER_DOUBLE_INCLUDE;
	if (skippast == MK_MSG_EMPTY_MESSAGE) goto AFTER_EMPTY_MESSAGE;
	if (skippast == MK_MSG_MISSING_SUBJECT) goto AFTER_MISSING_SUBJECT;
#ifdef UNREADY_CODE
	HJ69290
	HJ43055
#endif
	if (skippast == MK_MSG_INVALID_NEWS_HEADER) goto AFTER_INVALID_NEWS_HEADER;
	if (skippast == MK_MSG_INVALID_FOLLOWUP_TO_HEADER) goto AFTER_INVALID_FOLLOWUP_TO_HEADER;

	// Check if they have quoted a document and not edited it, and also
	// attached the same document.
	if (m_quotedText &&
		PL_strncmp(body, m_quotedText, PL_strlen(m_quotedText)) == 0 &&
		m_attachData &&
		m_attachData[0].url &&
		m_defaultUrl &&
		!PL_strcmp (m_attachData[0].url, m_defaultUrl)) {
		return MK_MSG_DOUBLE_INCLUDE;
	}

AFTER_DOUBLE_INCLUDE:
	// Check if this message has no attachments, and the body has not been
	// edited.
	if (!m_attachData || !m_attachData[0].url)
	{
		if (PL_strcmp(body, m_initfields->GetBody()) == 0)
			return MK_MSG_EMPTY_MESSAGE;
		if (m_markup)
		{
			// Generate the empty body warning for HTML messages which only contain 
			// Composer's default HTML tags. Yes, we'll need to update this if/when 
			// Composer changes, but I don't really want to parse the body just for this
			const char *kDefaultComposerBody = 
				"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n<HTML>\n&nbsp;</HTML>\n";

			if (PL_strcmp(body, kDefaultComposerBody) == 0)
				return MK_MSG_EMPTY_MESSAGE;
		}
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
#ifdef UNREADY_CODE
	HJ32538
	HJ83505
#endif

	newsgroups = m_fields->GetHeader (MSG_NEWSGROUPS_HEADER_MASK);
	if (!SanityCheckNewsgroups (newsgroups))
		return MK_MSG_INVALID_NEWS_HEADER;

AFTER_INVALID_NEWS_HEADER:

	newsgroups = m_fields->GetHeader (MSG_FOLLOWUP_TO_HEADER_MASK);
	if (!SanityCheckNewsgroups (newsgroups))
		return MK_MSG_INVALID_FOLLOWUP_TO_HEADER;

AFTER_INVALID_FOLLOWUP_TO_HEADER:

#if 0
	if ((m_cited && !m_bodyEdited)
		/* They have quoted a document and have not edited it before sending.
		   This means that the entire document is sitting there as a citation;
		   offer to convert it to an attachment instead, since that preserves
		   more information without increasing the size of the message. */

		|| (m_cited &&
			m_attachData &&
			m_attachData[1] &&
			m_attachData[0]->url &&
			m_defaultUrl &&
			!PL_strcmp (m_attachData[0]->url, m_defaultUrl))
		/* They have quoted a document and have *also* attached it, so it's
		   in there twice.  Offer to throw one away. */
		)
		{
			int answer = FE_BogusQuotationQuery (m_context, PR_FALSE);
			switch (answer)
				{
				case 0:
					return PR_FALSE;
					break;
				case 1:
					/* protect me from myself */
		  
					break;
				case 2:
					/* let me be a loser */
					return PR_TRUE;
					break;
				default:
					PR_ASSERT (0);
					return PR_FALSE;
					break;
				}
		}
#endif

	return 0;
}


void
nsMsgCompose::DeliveryDoneCB_s(MWContext *context, void *fe_data,
									  int status, const char *error_message)
{
	((nsMsgCompose*) fe_data)->DeliveryDoneCB(context, status,
													 error_message);
}

void
nsMsgCompose::DeliveryDoneCB(MWContext* context, int status,
									const char* error_message)
{
	PR_ASSERT(context == m_context);

	// *** We don't want to set m_status to status. The default value
	// of m_status (-1) prevents the composition pane from closing down
	// once we done with saving draft. The composition pane should remain up.
	if ((m_deliver_mode != MSG_SaveAsDraft && 
		 m_deliver_mode != MSG_SaveAsTemplate && 
		 m_deliver_mode != MSG_SaveAs) || 
		(m_deliver_mode == MSG_SaveAsDraft && m_closeAfterSave))
	  m_status = status;

	PR_ASSERT(!m_attachmentInProgress);
	if (m_deliveryInProgress) {
		m_deliveryInProgress = PR_FALSE;
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
#ifdef UNREADY_CODE
			char *errmsg;
			errmsg = PR_smprintf(XP_GetString(MK_COMMUNICATIONS_ERROR),
								 status);
			if (errmsg) {
				FE_Alert(context, errmsg);
				PR_Free(errmsg);
			}
#endif
		}
    } else {
		/* ### jht bug 45220 -- do following only when successfully deliver the message */
		/* time to delete message from draft/outbox */
/*JFD
		if (m_actionInfo) 
		{
			MailDB *mailDB = NULL;
			MSG_FolderInfoMail *mailFolderInfo = (m_actionInfo->m_folderInfo) ? m_actionInfo->m_folderInfo->GetMailFolderInfo() : (MSG_FolderInfoMail *)NULL;
			
			if ( mailFolderInfo && m_actionInfo->m_msgKeyArray.GetSize() > 0 ) 
			{

				MailDB::Open (mailFolderInfo->GetPathname(), PR_FALSE, &mailDB);
				if (mailDB) {
					switch ( m_actionInfo->m_flags ) {
					case MSG_FLAG_EXPUNGED:
						if (m_deliver_mode != MSG_SaveAsDraft &&
							m_deliver_mode != MSG_SaveAsTemplate &&
							m_deliver_mode != MSG_SaveAs && 
							!(mailFolderInfo->GetFlags() & MSG_FOLDER_FLAG_TEMPLATES)) {
							// PR_ASSERT(m_actionInfo->m_msgKeyArray.GetSize() == 1);
							if (mailFolderInfo->GetType() == FOLDER_IMAPMAIL)
							    DeleteIMAPOldUID(m_actionInfo);
							else
								mailDB->DeleteMessage(m_actionInfo->m_msgKeyArray.GetAt(0));
						}
						break;
					case MSG_FLAG_FORWARDED:
					case MSG_FLAG_REPLIED:
						{
							uint i, size;
							IDArray readIds;
							MSG_IMAPFolderInfoMail *imapFolder = 
								mailFolderInfo->GetIMAPFolderInfoMail();
							
							size = m_actionInfo->m_msgKeyArray.GetSize();
							if (m_deliver_mode == MSG_SaveAsDraft ||
								m_deliver_mode == MSG_SaveAsTemplate ||
								m_deliver_mode == MSG_SaveAs)
								size--;

							for (i=0; i < size; i++) 
							{
								if (m_actionInfo->m_flags == MSG_FLAG_FORWARDED)
									mailDB->MarkForwarded
										(m_actionInfo->m_msgKeyArray.GetAt(0),
										 PR_TRUE);
								else
									mailDB->MarkReplied
										(m_actionInfo->m_msgKeyArray.GetAt(0),
										 PR_TRUE); 

								readIds.Add(m_actionInfo->m_msgKeyArray.GetAt(0));
								
								m_actionInfo->m_msgKeyArray.RemoveAt(0);
							}
							if (imapFolder && (imapFolder->GetType()
											   == FOLDER_IMAPMAIL)) 
							{
								PRBool storeFlag = PR_TRUE;
								PREF_GetBoolPref("mail.imap.store_answered_forwarded_flag", &storeFlag);
								if (storeFlag)
									imapFolder->StoreImapFlags(this, m_actionInfo->m_flags, PR_TRUE, readIds);
							}
							if (m_deliver_mode == MSG_SaveAsDraft ||
								m_deliver_mode == MSG_SaveAs ||
								m_deliver_mode == MSG_SaveAsTemplate ) 
							{
								m_actionInfo->m_flags = MSG_FLAG_EXPUNGED;
								m_actionInfo->m_folderInfo = 
									GetMaster()->FindMagicMailFolder
									(m_deliver_mode == MSG_SaveAsTemplate ?
									 MSG_FOLDER_FLAG_TEMPLATES :
									 MSG_FOLDER_FLAG_DRAFTS, PR_TRUE);

							}
						}
					    break;
					default:
						// This is the case of edit message then save as draft/template
						m_actionInfo->m_msgKeyArray.RemoveAt(0);
						if (m_deliver_mode == MSG_SaveAsDraft ||
							m_deliver_mode == MSG_SaveAsTemplate ||
							m_deliver_mode == MSG_SaveAs) {
							m_actionInfo->m_flags = MSG_FLAG_EXPUNGED;
							m_actionInfo->m_folderInfo = 
								GetMaster()->FindMagicMailFolder
								(m_deliver_mode == MSG_SaveAsTemplate ?
								 MSG_FOLDER_FLAG_TEMPLATES :
								 MSG_FOLDER_FLAG_DRAFTS, PR_TRUE);

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
					 m_deliver_mode != MSG_SaveAs &&
					 m_actionInfo->m_flags & MSG_FLAG_EXPUNGED)
			{
				// *** jht - I doubt we will ever gets here.
				PR_ASSERT(PR_FALSE);
				DeleteIMAPOldUID(m_actionInfo);
			}
		}

		// ### dmb This may not be right for news, but it shouldn't be harmful either
		if (m_actionInfo && m_actionInfo->m_folderInfo)
			m_actionInfo->m_folderInfo->SummaryChanged();

JFD*/
		if (m_deliver_mode == MSG_DeliverNow)
		{
			// If we're delivering the mail right now, tell the FE 
			// the Sent folder has new counts
			MSG_FolderInfo *folder = NULL;
			const char *fccPath = m_fields->GetFcc();

			if (fccPath && *fccPath)
				folder = FindFolderOfType(MSG_FOLDER_FLAG_SENTMAIL);

/*JFD
			if (folder)
				folder->SummaryChanged();
*/
		}
		else if (m_deliver_mode == MSG_QueueForLater)
		{
			// If we're delivering the mail into the Outbox/queue folder, 
			// tell the FE the Outbox folder has new counts
			MSG_FolderInfo *folder = FindFolderOfType (MSG_FOLDER_FLAG_QUEUE);
/*JFD
			if (folder)
				folder->SummaryChanged();
*/
		}
	}
}

void
nsMsgCompose::MailCompositionAllConnectionsComplete ()
{
	/* This may be redundant, I'm not sure... */
	if (m_deliveryInProgress) {
		m_deliveryInProgress = PR_FALSE;
		FE_UpdateCompToolbar(this);
	}
	if (m_attachmentInProgress) {
		m_attachmentInProgress = PR_FALSE;
		FE_UpdateCompToolbar(this);
	}

	if (m_status >= 0) {
		delete this;
	}
}


void
nsMsgCompose::CheckExpansion(MSG_HEADER_SET header)
{
	return;
}


PRBool
nsMsgCompose::DeliveryInProgress ()
{
	/* Disable the UI if delivery, attachment loading, or quoting is in
	   progress. */
	return m_deliveryInProgress || m_attachmentInProgress || (m_quoteUrl != 0);
}

/* This function sets the markup flag to indicate that the message
   body is HTML.  Returns the previously set value.
*/

void
nsMsgCompose::SetHTMLMarkup(PRBool flag)
{
    m_markup = flag;
}

PRBool
nsMsgCompose::GetHTMLMarkup(void)
{
    return m_markup;
}

int
nsMsgCompose::DoneComposeMessage( MSG_Deliver_Mode deliver_mode )
{
	int attachment_count = 0;
	PRBool digest_p = PR_FALSE;
	int status = 0;

	if (m_pendingAttachmentsCount) {
		m_deliveryInProgress = PR_TRUE; // so that DoneComposeMessage is called again
		status = DownloadAttachments();
		return status;
	}

	const char *groups = m_fields->GetHeader(MSG_NEWSGROUPS_HEADER_MASK);
	if (groups && *groups && !m_host)
		m_host = InferNewsHost (groups);

	if (m_markup && (deliver_mode != MSG_SaveAs &&
					 deliver_mode != MSG_SaveAsDraft &&
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
					 PR_ASSERT(action != MSG_HTMLAskUser);
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
/*JFD
				static XPDialogInfo dialogInfo = {
					0,				// I'll provide all my own buttons, thanks.
#ifdef XP_OS2
					(PRBool (_Optlink*) (XPDialogState*,char**,int,unsigned int))
#endif
					nsMsgCompose::AskDialogDone_s,
					500,
					300
				};
				XPDialogStrings* strings =
					XP_GetDialogStrings(MK_MSG_ASK_HTML_MAIL);
				if (!strings) return MK_OUT_OF_MEMORY;
				XP_MakeHTMLDialog(GetContext(), &dialogInfo,
								  MK_MSG_ASK_HTML_MAIL_TITLE, strings, this);
*/
				return 0;
			}

		}

		switch (action) {
		case MSG_HTMLUseMultipartAlternative:
			m_fields->SetUseMultipartAlternative(PR_TRUE);
			break;
		case MSG_HTMLConvertToPlaintext:
			m_fields->SetForcePlainText(PR_TRUE);
			break;
		case MSG_HTMLSendAsHTML:
			break;
		default:
			PR_ASSERT(0);
			return -1;
		}
	}

	CheckExpansion(MSG_TO_HEADER_MASK);
	CheckExpansion(MSG_CC_HEADER_MASK);
	CheckExpansion(MSG_BCC_HEADER_MASK);

#ifdef UNREADY_CODE
	HJ74362
#endif

	const char* body = m_fields->GetBody();
	PRUint32 body_length = PL_strlen(body);


	for (attachment_count = 0;
		 m_attachData && m_attachData[attachment_count].url;
		 attachment_count++)
		;

	if (m_attachData && m_attachData[0].url && m_attachData[1].url ) {
		MSG_AttachmentData* s;
		digest_p = PR_TRUE;
		for (s = m_attachData ; s->url ; s++) {
			/* When there are attachments, start out assuming it is a digest,
			   and then decide that it is not if any of the attached URLs are
			   not mail or news messages. */
			if (PL_strncmp(s->url, "news:", 5) != 0 &&
				PL_strncmp(s->url, "snews:", 6) != 0 &&
				PL_strncmp(s->url, "IMAP:", 5) != 0 &&
				PL_strncmp(s->url, "mailbox:", 8) != 0) {
				digest_p = PR_FALSE;
			}
		}
	}

	PR_ASSERT(!m_attachmentInProgress);
	PR_ASSERT(!m_deliveryInProgress);
	m_deliveryInProgress = PR_TRUE;
	FE_UpdateCompToolbar(this);
	
	if (m_messageId == NULL) {
		m_messageId = msg_generate_message_id();
		m_duplicatePost = PR_FALSE;
	} else {
		m_duplicatePost = PR_TRUE;
	}
	
	m_fields->SetMessageId(m_messageId, NULL);
	
/*JFD
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
						    deliver_mode == MSG_SaveAsTemplate || 
							deliver_mode == MSG_SaveAs ) ?
                              ED_FINISHED_SAVE_DRAFT : ED_FINISHED_MAIL_SEND, 
                          pRootPartName, fs, PR_TRUE, PR_TRUE);
			// Note: EDT_SaveFileTo will delete fs, even if it returns an error.  So
			// it is incorrect to delete it here.  Also, we ignore the result, because
			// it calls FE_Alert itself.
		}
		PR_FREEIF(pRootPartName);
	}
	else

#endif // MSG_SEND_MULTIPART_RELATED
*/
	{
/*JFD
		msg_StartMessageDeliveryWithAttachments(this, this,
												m_fields,
												digest_p, PR_FALSE, deliver_mode,
												(m_markup ? TEXT_HTML : 
												 TEXT_PLAIN),
												body, body_length,
												m_attachedFiles,
												NULL,
#ifdef XP_OS2
												(void (_Optlink*) (MWContext*,void*,int,const char*))
#endif
												DeliveryDoneCB_s);
JFD */
	}
	return 0; // Always success, because Errors were reported and handled by EDT_SaveFileTo.
}

int
nsMsgCompose::SendMessageNow()
{
	PREF_SetBoolPref("network.online", PR_TRUE);	// make sure we're online.
		// remember if we're queued so we know which folder
	m_deliver_mode = MSG_DeliverNow;		
							
	if (m_fields->GetAttachVCard())
		CreateVcardAttachment();
	// counts we need to update.
	return DoneComposeMessage(MSG_DeliverNow);
}

int
nsMsgCompose::QueueMessageForLater()
{
		// remember if we're queued so we know which folder
	m_deliver_mode = MSG_QueueForLater;
	if (m_fields->GetAttachVCard())
		CreateVcardAttachment();
	// counts we need to update.
	return DoneComposeMessage(MSG_QueueForLater);
}

int
nsMsgCompose::SaveMessage()
{
/*JFD
	if (m_actionInfo)
	{
		if (m_actionInfo && m_actionInfo->m_folderInfo->GetFlags() &
			MSG_FOLDER_FLAG_TEMPLATES)
			m_deliver_mode = MSG_SaveAsTemplate;
		else
			m_deliver_mode = MSG_SaveAsDraft;
	}
	else
*/
	{
		m_deliver_mode = MSG_SaveAsDraft;
	}

	return DoneComposeMessage(m_deliver_mode);
}

MSG_CommandType
nsMsgCompose::PreviousSaveCommand()
{
	if (m_deliver_mode == MSG_SaveAsTemplate)
		return MSG_SaveTemplate;
	else
		return MSG_SaveDraft;
}

int
nsMsgCompose::SaveMessageAsDraft()
{
  // SaveMessageAsDraft always save a new Draft; if the current msg has been saved
  // before clear the previous saved UID so we won't try to replace with the
  // new Draft.
 /*JFD
 if (m_actionInfo && m_actionInfo->m_flags == MSG_FLAG_EXPUNGED &&
	  m_actionInfo->m_msgKeyArray.GetSize() >= 1)
  {
	  // if the command is MSG_SaveDraftThenClose we do want to replace with
	  // the previous saved draft else we create a new one
	  if (!m_closeAfterSave)
		  m_actionInfo->m_msgKeyArray.RemoveAt(0);
	  if (! (m_actionInfo->m_folderInfo->GetFlags() & MSG_FOLDER_FLAG_DRAFTS))
		  m_actionInfo->m_folderInfo = GetMaster()->FindMagicMailFolder
			  (MSG_FOLDER_FLAG_DRAFTS, PR_FALSE);
  }
*/
  m_deliver_mode = MSG_SaveAsDraft;
  return DoneComposeMessage(MSG_SaveAsDraft);
}

int
nsMsgCompose::SaveMessageAsTemplate()
{
  // SaveMessageAsTemplate always save a new Template; if the current msg has
  // been saved clear the previous saved UID so we won't try to replace with
  // the new Template.
/*JFD
  if (m_actionInfo && m_actionInfo->m_flags == MSG_FLAG_EXPUNGED &&
	  m_actionInfo->m_msgKeyArray.GetSize() >= 1)
  {
	  m_actionInfo->m_msgKeyArray.RemoveAt(0);
	  if (! (m_actionInfo->m_folderInfo->GetFlags() & MSG_FOLDER_FLAG_TEMPLATES))
		  m_actionInfo->m_folderInfo = GetMaster()->FindMagicMailFolder
			  (MSG_FOLDER_FLAG_TEMPLATES, PR_FALSE);
  }
*/
  m_deliver_mode = MSG_SaveAsTemplate;

#ifdef SUPPORT_X_TEMPLATE_NAME
  char *defaultName = NULL;

#ifdef UNREADY_CODE
  defaultName = FE_Prompt (GetContext(),
						   XP_GetString(MK_MSG_ENTER_NAME_FOR_TEMPLATE), 
						   m_fields->GetSubject());
#endif
  if (defaultName && *defaultName)
  {
	  m_fields->SetTemplateName(defaultName);
	  PR_FREEIF(defaultName);
  }
#endif /* SUPPORT_X_TEMPLATE_NAME */

  return DoneComposeMessage(MSG_SaveAsTemplate);
}

static int
StuffParams(char** params, const char* name, PRInt32 value)
{
	char* escaped = NET_EscapeHTML(name);
	if (!escaped) return MK_OUT_OF_MEMORY;
	char* tmp = PR_smprintf("<OPTION value=%ld>%s\n", (long) value, escaped);
	PR_Free(escaped);
	if (!tmp) return MK_OUT_OF_MEMORY;
	NET_SACat(params, tmp);
	PR_Free(tmp);
	return 0;
}

//void *pWnd used to pass in pointer to parent window of Recipients dialog
int
nsMsgCompose::PutUpRecipientsDialog(void *pWnd )
{
	int status;
	status = MungeThroughRecipients(NULL, NULL);
	if (status < 0) return status;
	MSG_RecipientList* ok = m_htmlrecip->GetList(PR_TRUE);
	MSG_RecipientList* notok = m_htmlrecip->GetList(PR_FALSE);
	if (m_callbacks.CreateRecipientsDialog) {
		status = (*m_callbacks.CreateRecipientsDialog)(this, m_callbackclosure,
													   notok, ok, pWnd);
		if (status >= 0) return status;
	}
/*JFD
	static XPDialogInfo dialogInfo = {
		0,				// I'll provide all my own buttons, thanks.
#ifdef XP_OS2
		(PRBool (_Optlink*) (XPDialogState*,char**,int,unsigned int))
#endif
		nsMsgCompose::RecipientDialogDone_s,
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

*/
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
/*JFD
		XP_CopyDialogString(strings, w, params ? params : "");
*/
		PR_FREEIF(params);
	}

    Chrome chrome;
    memset(&chrome, 0, sizeof(chrome));
    chrome.type = MWContextDialog;
/*JFD
    chrome.w_hint = dialogInfo.width;
    chrome.h_hint = dialogInfo.height;
*/
    chrome.is_modal = PR_TRUE;
    chrome.show_scrollbar = PR_TRUE;

/*JFD
	XP_MakeHTMLDialogWithChrome(GetContext(), &dialogInfo,
					  		    MK_MSG_HTML_RECIPIENTS_TITLE, strings,
 								&chrome, this);
*/
	return 0;
}


PRBool
nsMsgCompose::IsDuplicatePost() {
  return m_duplicatePost;
}

void nsMsgCompose::ClearCompositionMessageID()
{
  PR_FREEIF(m_messageId);
}

const char* nsMsgCompose::GetCompositionMessageID()
{
  return m_messageId;
}

int
nsMsgCompose::RemoveNoCertRecipients()
{
	int status = 0;

	status = RemoveNoCertRecipientsFromList(MSG_TO_HEADER_MASK);
	if (status >= 0) {
		status = RemoveNoCertRecipientsFromList(MSG_CC_HEADER_MASK);
		if (status >= 0) {
			status = RemoveNoCertRecipientsFromList(MSG_BCC_HEADER_MASK);
		}
	}
	return status;
}

int 
nsMsgCompose::RemoveNoCertRecipientsFromList(MSG_HEADER_SET header)
{
	char *ptr, *oldptr, *list, *newlist;
	PRBool changed = PR_FALSE;
	int status = 0;

	const char* line = m_fields->GetHeader(header);
	if (!line || !*line) return 0;
//JFD	list = MSG_ExtractRFC822AddressMailboxes(line);
	if (list && *list) {
		newlist = (char *)PR_Malloc(PL_strlen(list) + 1);
		if (!newlist) {
			status = MK_OUT_OF_MEMORY;
		} else {
			*newlist = '\0';
			ptr = oldptr = list;
			do {
				if ((ptr = PL_strchr(oldptr, ',')) != NULL) {
					*ptr = '\0';
				}
				if (oldptr) {
#ifdef UNREADY_CODE
					HJ38714
#endif
					if (ptr != NULL) {
						oldptr = ptr + 1;
					}
				}
			} while (ptr != NULL);

			// Replace
			if (changed) {
				m_fields->SetHeader(header, newlist, NULL);
			}
			PR_Free(newlist);
		}
		PR_Free(list);
	}
	return status;
}


#ifdef UNREADY_CODE
HJ53211
#endif

int 
nsMsgCompose::SetPreloadedAttachments ( MWContext *context,
											   struct MSG_AttachmentData *attachmentData,
											   struct MSG_AttachedFile *attachments,
											   int attachments_count )
{
  PR_ASSERT ( context == m_context );
  PR_ASSERT ( attachments && attachmentData );
  if ( !attachments || !attachmentData ) return -1;

  int status = 0;
  const char *error_message = NULL;
  
  PR_ASSERT ( m_attachData == NULL );

  m_attachData = (MSG_AttachmentData *) PR_Malloc ( (attachments_count+1) *
												   sizeof (MSG_AttachmentData) );
  if ( !m_attachData ) {
#ifdef UNREADY_CODE
	FE_Alert ( m_context, XP_GetString ( MK_OUT_OF_MEMORY ) );
#endif
	return MK_OUT_OF_MEMORY;
  }

  memset (m_attachData, 0, (attachments_count +1) * sizeof (MSG_AttachmentData));

  memcpy ( m_attachData, attachmentData, 
			  sizeof (MSG_AttachmentData) * attachments_count );
	
  m_pendingAttachmentsCount = attachments_count;
  m_attachmentInProgress = PR_TRUE;

  DownloadAttachmentsDone ( context, status, error_message, attachments );

  return status;
}

void 
nsMsgCompose::SetIMAPMessageUID ( nsMsgKey key )
{
	PR_ASSERT (m_deliver_mode == MSG_SaveAsDraft || 
			   m_deliver_mode == MSG_SaveAs ||
			   m_deliver_mode == MSG_SaveAsTemplate);
	PR_ASSERT(key != nsMsgKey_None);

	if (key == nsMsgKey_None)
		return;

/*JFD
	if (!m_actionInfo)
	{
		m_actionInfo = new MSG_PostDeliveryActionInfo
			(GetMaster()->FindMagicMailFolder
			 ((m_deliver_mode == MSG_SaveAsTemplate ?
			   MSG_FOLDER_FLAG_TEMPLATES : MSG_FOLDER_FLAG_DRAFTS), PR_FALSE));

		m_actionInfo->m_flags = MSG_FLAG_EXPUNGED;
	}
	PR_ASSERT(m_actionInfo);

	if (m_actionInfo)
	{
		// *** This basically handle the case of if we start saving message as
		// draft and then save it again as template. we need to point to the
		// correct folder so that subsequent save can replace the previous saved
		// message 
		if (m_actionInfo->m_folderInfo == NULL ||
			(m_deliver_mode == MSG_SaveAsTemplate &&
			 !(m_actionInfo->m_folderInfo->GetFlags() &
			   MSG_FOLDER_FLAG_TEMPLATES)) ||
			(m_deliver_mode == MSG_SaveAsDraft &&
			 !(m_actionInfo->m_folderInfo->GetFlags() &
			   MSG_FOLDER_FLAG_DRAFTS)))
		{
			m_actionInfo->m_folderInfo =
				GetMaster()->FindMagicMailFolder
				((m_deliver_mode == MSG_SaveAsTemplate ?
				  MSG_FOLDER_FLAG_TEMPLATES : MSG_FOLDER_FLAG_DRAFTS), PR_FALSE);
		}

		m_actionInfo->m_msgKeyArray.Add(key);
	}
*/
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
nsMsgCompose::RetrieveStandardHeaders(MSG_HeaderEntry ** return_list)
{
  int i, total;
  const char * field;
  MSG_HeaderEntry * list = NULL;

  PR_ASSERT(return_list);

  *return_list = NULL;

  for (i=0, total=0; i<TOTAL_HEADERS; i++) {
    MSG_HeaderEntry * entry;
    int count;
    field = GetCompHeader(standard_header_set[i]);

    count = MSG_ExplodeHeaderField(standard_header_set[i],field,&entry);
    if (entry) {
      list = (MSG_HeaderEntry*)PR_REALLOC(list,(total+count)*sizeof(MSG_HeaderEntry));
      if (list == NULL) {
        PR_Free(entry);
        return(-1);
      }
      memcpy(&list[total],entry,count*sizeof(MSG_HeaderEntry));   
      PR_Free(entry);
      total += count;
    }
  }

  *return_list = list;
  return(total);
}

void 
nsMsgCompose::ClearComposeHeaders()
{
  int i;
  for (i = 0; i<TOTAL_HEADERS; i++)
    SetCompHeader(standard_header_set[i],NULL);
}

int 
nsMsgCompose::SetHeaderEntries(MSG_HeaderEntry * in_list, int count)
{
	int status = 0;
	if ((count != -1) && (in_list != NULL)) 
	{
		int i;
		for (i=0; i<count; i++) 
		{
			PR_ASSERT(in_list[i].header_value);
			status = SetCompHeader(in_list[i].header_type,in_list[i].header_value);
			PR_Free(in_list[i].header_value);
		}	
		PR_Free(in_list);
	}
	return status;
}



PRBool
nsMsgCompose::AskDialogDone_s(XPDialogState *state, char **argv,
									 int argc, unsigned int button)
{
/*JFD
	return ((nsMsgCompose*)state->arg)->AskDialogDone(state, argv,
															  argc, button);
*/return PR_FALSE;/*JFD*/
}


PRBool
nsMsgCompose::AskDialogDone(XPDialogState * /*state*/, char **argv,
								   int argc, unsigned int /*button*/)
{
/*JFD
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
		default:
			PR_ASSERT(0);
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
		PR_ASSERT(0);
		break;
	}
*/
	return PR_FALSE;
}

#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
int DomainCompare(const void* e1, const void* e2)
{
	return PL_strcmp(*((char**) e1), *((char**) e2));
}

int
nsMsgCompose::ResultsRecipients(PRBool cancelled, PRInt32* nohtml,
									   PRInt32* htmlok)
{
	int status = 0;
	if (cancelled) return 0;
	PR_ASSERT(m_htmlrecip);
	if (!m_htmlrecip) return -1;
	status = m_htmlrecip->SetNewList(nohtml, htmlok);
	if (status < 0) return status;

	char** list = NULL;
	char** tmp;
	char* ptr;
	char* endptr;
	char* domainlist = NULL;
	char** domainstrings = NULL;
	PRBool changed;
	int num = 0;
	int max = 0;
	int length;
	int i, j;

  for (int w=0 ; w<2 ; w++) 
  {
    PRBool html = (w == 1);
    list = m_htmlrecip->GetChangedList(Address, html);
    for (tmp = list ; tmp && *tmp ; tmp++) 
    {
      char* names = NULL;
      char* addresses = NULL;
      int num /*JFD = MSG_ParseRFC822Addresses(*tmp, &names, &addresses)*/;
      PR_ASSERT(num == 1);
      if (num == 1) 
      {
        (void) AB_InsertOrUpdateABookEntry(this, names, addresses, html);          
      }
      PR_FREEIF(addresses);
      PR_FREEIF(names);
    }
    m_htmlrecip->FreeChangedList(list);
    list = NULL;

		if (m_host) {
			list = m_htmlrecip->GetChangedList(Newsgroup, html);
/*JFD
				for (tmp = list ; tmp && *tmp ; tmp++) {
				m_host->SetIsHTMLOKGroup(*tmp, html);
			}
*/
			m_htmlrecip->FreeChangedList(list);
			list = m_htmlrecip->GetChangedList(GroupHierarchy, html);
/*JFD
				for (tmp = list ; tmp && *tmp ; tmp++) {
				m_host->SetIsHTMLOKTree(*tmp, html);
			}
*/
			m_htmlrecip->FreeChangedList(list);
			list = NULL;
		}
	}
/*JFD
	if (m_host) m_host->SaveHostInfo();
*/

	PREF_CopyCharPref("mail.htmldomains", &domainlist);
	changed = PR_FALSE;

	length = domainlist ? PL_strlen(domainlist) : 0;
	for (ptr = domainlist ; ptr && *ptr ; ptr = PL_strchr(ptr + 1, ',')) {
		max++;
	}
	max += m_htmlrecip->GetNum() + 1; // We be paranoid.
	domainstrings = new char* [max];
	if (!domainstrings) {
		status = MK_OUT_OF_MEMORY;
		goto FAIL;
	}
	for (ptr = domainlist ; ptr && *ptr ; ptr = endptr) {
		endptr = PL_strchr(ptr, ',');
		if (endptr) *endptr++ = '\0';
		domainstrings[num++] = ptr;
	}
	list = m_htmlrecip->GetChangedList(Domain, PR_FALSE);
	for (tmp = list ; tmp && *tmp ; tmp++) {
		for (i=0 ; i<num ; i++) {
			while (i<num && PL_strcmp(domainstrings[i], *tmp) == 0) {
				num--;
				domainstrings[i] = domainstrings[num];
				changed = PR_TRUE;
			}
		}
	}
	m_htmlrecip->FreeChangedList(list);
	list = m_htmlrecip->GetChangedList(Domain, PR_TRUE);
	for (tmp = list ; tmp && *tmp ; tmp++) {
		domainstrings[num++] = *tmp;
		changed = PR_TRUE;
		length += PL_strlen(*tmp) + 1;
	}
	if (changed) {
		// Now nuke dups.
		XP_QSORT(domainstrings, num, sizeof(char*), DomainCompare);
		for (i=0 ; i < num-1 ; i++) {
			while (i < num-1 &&
				   PL_strcmp(domainstrings[i], domainstrings[i+1]) == 0) {
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
			PL_strcat(ptr, domainstrings[i]);
			if (i < num-1) PL_strcat(ptr, ",");
		}
		PREF_SetCharPref("mail.htmldomains", ptr);
		PREF_SavePrefFile();
		delete [] ptr;
		ptr = NULL;
	}				

 FAIL:
	m_htmlrecip->FreeChangedList(list);
	PR_FREEIF(domainlist);
	delete [] domainstrings;
	return status;
}




PRBool
nsMsgCompose::RecipientDialogDone_s(XPDialogState *state, char **argv,
										   int argc, unsigned int button)
{
	return
/*JFD		((nsMsgCompose*)state->arg)->RecipientDialogDone(state, argv,
																argc, button);
*/PR_TRUE;/*JFD*/
}


static void
Slurp(PRInt32* list, const char* name, char** argv, int argc)
{
	for (; argc > 0 ; argc -= 2 , argv += 2) {
		if (PL_strcmp(name, argv[0]) == 0) {
			*list++ = XP_ATOI(argv[1]);
		}
	}
	*list = -1;
}

PRBool
nsMsgCompose::RecipientDialogDone(XPDialogState * /*state*/,
										 char **argv,
										 int argc, unsigned int /*button*/)
{
/*JFD
	switch (XP_ATOI(XP_FindValueInArgs("cmd", argv, argc))) {
	case 0: {
		PR_ASSERT(argc > 0);
		if (argc <= 0) return PR_FALSE;
		PRInt32* nohtml = new PRInt32 [argc];
		if (!nohtml) return PR_FALSE;
		PRInt32* htmlok = new PRInt32 [argc];
		if (!htmlok) {
			delete [] nohtml;
			return PR_FALSE;
		}
		Slurp(nohtml, "nohtml", argv, argc);
		Slurp(htmlok, "html", argv, argc);
		ResultsRecipients(PR_FALSE, nohtml, htmlok);
		delete [] nohtml;
		delete [] htmlok;
		return PR_FALSE;
	}
	case 1:
		ResultsRecipients(PR_TRUE, NULL, NULL);
		return PR_FALSE;
	case 2:
		XP_NetHelp(GetContext(), HELP_HTML_MAIL_QUESTION_RECIPIENT);
		return PR_TRUE;
	default:
		PR_ASSERT(0);
		break;
	}
*/
	return PR_FALSE;
}

typedef struct tagSkipData {
	char*	pTag;
	int		lenTag;		// 0 means use PL_strlen
} SkipData;


PRBool
nsMsgCompose::HasNoMarkup()
{

	// we want a link with the same text and link to pass
	// <A HREF="http://warp/client/dogbert">http://warp/client/dogbert</A>

	PR_ASSERT(m_markup);
	if (!m_markup) return PR_TRUE;
	const char* body = m_fields->GetBody();
	while (body && *body) {
		body = PL_strchr(body, '<');
		if (!body) break;
		char* endptr = PL_strchr(body, '>');
		PR_ASSERT(endptr);
		if (!endptr) break;
		char c = *++endptr;
		*endptr = '\0';
		PRBool recognized = PR_FALSE;
		char* newEnd = endptr;
		
		if (PL_strncasecmp(body, "<A HREF=", 8) == 0) {
			char* pLinkStart = PL_strchr(body, '"');		// find the open quote
			if (pLinkStart) {
				++pLinkStart;								// past the open quote
				char* pLinkEnd = PL_strchr(pLinkStart, '"');// find the close quote
				if (pLinkEnd) {
					char c2 = *pLinkEnd;					// save this char
					*pLinkEnd = '\0';						// terminate the link

					// now pLink points to the URL
					// find the text to see if it's the same as the link
					char* pTextStart = endptr;
					*endptr = c;							// restore this char early because it's the lead char of our string
					char* pTextEnd = PL_strchr(pTextStart, '<');
					if (pTextEnd) {
						char c3 = *pTextEnd;
						*pTextEnd = '\0';					// terminate the text
						recognized = (PL_strcmp(pLinkStart, pTextStart) == 0);
						*pTextEnd = c3;						// restore this char
						newEnd = pTextEnd + 1;				// skip past the opening of the </A>
					}
					*pLinkEnd = c2;							// restore this char
				}
			}
		} else {
			static SkipData pSkipTags[] =
			{
				{"<BODY", 5},
				{"<!DOCTYPE", 9},
				{"<P>", 0},
				{"</P>", 0},
				{"<BR>", 0},
				{"<DT>", 0},
				{"</DT>", 0},
				{"<HTML>", 0},
				{"</HTML>", 0},
				{"</BODY>", 0},
				{"<BLOCKQUOTE TYPE=CITE>", 0},
				{"</BLOCKQUOTE>",  0},
				{NULL, 0},
			};
			for (SkipData* pTestItem = pSkipTags; (pTestItem->pTag != NULL) && !recognized; pTestItem++) {
				// if len != 0, then use PL_strncasecmp
				if (pTestItem->lenTag != 0) {
					recognized = PL_strncasecmp(body, pTestItem->pTag, pTestItem->lenTag) == 0;
				} else {
					recognized = PL_strcasecmp(body, pTestItem->pTag) == 0;
				}
				
			}
		}

		*endptr = c;
		body = newEnd;
		if (!recognized) return PR_FALSE;
	}
	return PR_TRUE;
}

int
nsMsgCompose::MungeThroughRecipients(PRBool* someNonHTML,
											PRBool* groupNonHTML)
{
	PRBool foo;
	if (!someNonHTML) someNonHTML = &foo;
	if (!groupNonHTML) groupNonHTML = &foo;
	*someNonHTML = PR_FALSE;
	*groupNonHTML = PR_FALSE;
	int status = 0;
	char* names = NULL;
	char* addresses = NULL;
	const char* groups;
	char* name = NULL;
	char* end;
	PRBool match = PR_FALSE;
	m_host = NULL;				// Pure paranoia, in case we some day actually
								// have a UI that lets people change this.

	static PRInt32 masks[] = {
		MSG_TO_HEADER_MASK,
		MSG_CC_HEADER_MASK,
		MSG_BCC_HEADER_MASK
	};
	char* domainlist = NULL;

	delete m_htmlrecip;
	m_htmlrecip = new MSG_HTMLRecipients();
	if (!m_htmlrecip) return MK_OUT_OF_MEMORY;

	for (int i=0 ; i < sizeof(masks) / sizeof(masks[0]) ; i++) {
		const char* orig = m_fields->GetHeader(masks[i]);
		if (!orig || !*orig) continue;
		char* value = NULL;
		value = PL_strdup(orig);
		if (!value) {
			status = MK_OUT_OF_MEMORY;
			goto FAIL;
		}
		
		int num /*JFD = MSG_ParseRFC822Addresses(value, &names, &addresses)*/;
		PR_Free(value);
		value = NULL;
		char* addr = NULL;
		char* name = NULL;
		for (int j=0 ; j<num ; j++) {
			if (addr) {
				addr = addr + PL_strlen(addr) + 1;
				name = name + PL_strlen(name) + 1;
			} else {
				addr = addresses;
				name = names;
			}
			if (!addr || !*addr) continue;

			// Need to check for a address book entry for this person and if so,
      // we have to see if this person can receive MHTML mail.
      match = AB_GetHTMLCapability(this, addr);

      char* at = PL_strchr(addr, '@');
			char* tmp = MSG_MakeFullAddress(name, addr);
			status = m_htmlrecip->AddOne(tmp, addr, Address, match);
			if (status < 0) goto FAIL;
			PR_Free(tmp);
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
				char* dot = PL_strchr(domain, '.');
				if (!dot) break;
				PRInt32 domainlength = PL_strlen(domain);
				char* ptr;
				char* endptr = NULL;
				PRBool found = PR_FALSE;
				for (ptr = domainlist ; ptr && *ptr ; ptr = endptr) {
					endptr = PL_strchr(ptr, ',');
					int length;
					if (endptr) {
						length = endptr - ptr;
						endptr++;
					} else {
						length = PL_strlen(ptr);
					}
					if (length == domainlength) {
						if (PL_strncasecmp(domain, ptr, length) == 0) {
							found = PR_TRUE;
							match = PR_TRUE;
							break;
						}
					}
				}
#ifdef UNREADY_CODE
				char* tmp = PR_smprintf("%s@%s",
										XP_GetString(MK_MSG_EVERYONE),
										domain);
#endif
				if (!tmp) return MK_OUT_OF_MEMORY;
				status = m_htmlrecip->AddOne(domain, tmp, Domain, found);
				PR_Free(tmp);
				if (status < 0) goto FAIL;
				domain = dot + 1;
			}
			if (!match) *someNonHTML = PR_TRUE;
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
		end = PL_strchr(groups, ',');
		if (end) *end = '\0';
		name = PL_strdup(groups);
		if (end) *end++ = ',';
		if (!name) {
			status = MK_OUT_OF_MEMORY;
			goto FAIL;
		}
		char* group = XP_StripLine(name);
/*JFD
		match = m_host->IsHTMLOKGroup(group);
*/
		status = m_htmlrecip->AddOne(group, group, Newsgroup, match);
		if (status < 0) goto FAIL;
		char* tmp = PL_strdup(group);
		if (!tmp) {
			status = MK_OUT_OF_MEMORY;
			goto FAIL;
		}
		
		for (;;) {
/*JFD
			PRBool found = m_host->IsHTMLOKTree(tmp);
*/
			char* desc = PR_smprintf("%s.*", tmp);
			if (!desc) {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			}
/*JFD
			status = m_htmlrecip->AddOne(tmp, desc, GroupHierarchy, found);
*/
			PR_Free(desc);
			if (status < 0) {
				PR_Free(tmp);
				tmp = NULL;
				goto FAIL;
			}
/*JFD
			if (found) match = PR_TRUE;
*/
			char* p = PL_strrchr(tmp, '.');
			if (p) *p = '\0';
			else break;
		}
		PR_Free(tmp);
		tmp = NULL;
		if (!match) {
			*someNonHTML = PR_TRUE;
			*groupNonHTML = PR_TRUE;
		}
	}

 FAIL:
	PR_FREEIF(names);
	PR_FREEIF(domainlist);
	PR_FREEIF(addresses);
	PR_FREEIF(name);
	return status;
}



MSG_HTMLComposeAction
nsMsgCompose::DetermineHTMLAction()
{
	PRBool someNonHTML, groupNonHTML;
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
			PRInt32 value;
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
	

MSG_NewsHost *nsMsgCompose::InferNewsHost (const char *group)
{
	MSG_NewsHost *retHost = NULL;

	// No news host was specified. Use our bag of tricks to try to find one
	char* host_and_port = NULL;
#ifdef UNREADY_CODE
	HJ19119
#endif

	// Trick #1: Do we have a URL for the host in the header fields? This can happen
	// if we're posting, or if the user has typed a news URL into the newsgroups field
	const char* posturl = m_fields->GetNewspostUrl();
	if (posturl && *posturl) 
	{
		host_and_port = NET_ParseURL(posturl, GET_HOST_PART);
#ifdef UNREADY_CODE
		HJ96829
		if (host_and_port && *host_and_port) 
			HJ89510
#endif
	}

	if (!retHost)
	{
/*JFD
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
					m_fields->SetHeader (MSG_NEWSPOSTURL_HEADER_MASK, PL_strdup(base));
			}

			// Trick #3: This group isn't found among our subscribed groups, or there's more
			// than one group with the same name, so just use the default host.
			if (!retHost) 
				retHost = hostTable->GetDefaultHost(PR_TRUE);
		}
*/
	}

	PR_FREEIF(host_and_port);
	return retHost;
}


#ifdef UNREADY_CODE
HJ91549
HJ27863

HJ75043
#endif





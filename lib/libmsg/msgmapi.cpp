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
//
// msgmapi.cpp -- implements XP mail/news support for the Microsoft Mail API (MAPI)
//

#include "msg.h" // Leave this first for Windows precompiled headers

#if defined (XP_WIN)

#include "xp.h"
#include "msgmapi.h"
#include "msgzap.h"
#include "maildb.h"
#include "mailhdr.h"
#include "msgfinfo.h"
#include "msgpane.h"
#include "pmsgsrch.h"
#include "abcinfo.h"
#include "abpane2.h"
#include "msgstrob.h"

//*****************************************************************************
// Encapsulate the XP DB stuff required to enumerate messages

class MSG_MapiListContext : public MSG_ZapIt
{
public:
	MSG_MapiListContext () {}
	~MSG_MapiListContext ();

	MsgERR OpenDatabase (MSG_FolderInfo *folder);

	MessageKey GetFirst ();
	MessageKey GetNext ();
	MsgERR MarkRead (MessageKey key, XP_Bool read);

	lpMapiMessage GetMessage (MessageKey);

protected:

	char *ConvertDateToMapiFormat (time_t);
	char *ConvertBodyToMapiFormat (MailMessageHdr *hdr);
	void ConvertRecipientsToMapiFormat (const char *ourRecips, lpMapiRecipDesc mapiRecips, 
		int numRecips, int mapiRecipClass);

	MSG_FolderInfo *m_folder;
	MailDB *m_db;
	ListContext *m_listContext;
};


MSG_MapiListContext::~MSG_MapiListContext ()
{
	if (m_listContext && m_db)
		m_db->ListDone (m_listContext);
	if (m_db)
		m_db->Close();
}


MsgERR MSG_MapiListContext::OpenDatabase (MSG_FolderInfo *folder)
{
	MsgERR dbErr = eFAILURE;
	MSG_FolderInfoMail *mailFolder = NULL;
	if (folder && (mailFolder = folder->GetMailFolderInfo()))
	{
		XP_Bool wasCreated;
		if (FOLDER_MAIL == mailFolder->GetType())
			dbErr = MailDB::Open (mailFolder->GetPathname(), FALSE, &m_db, FALSE);
		else
			dbErr = ImapMailDB::Open (mailFolder->GetPathname(), FALSE, &m_db, mailFolder->GetMaster(), &wasCreated);

		m_folder = folder;
	}
	return dbErr;
}


MessageKey MSG_MapiListContext::GetFirst ()
{
	MessageKey key = MSG_MESSAGEKEYNONE;
	XP_ASSERT (NULL == m_listContext); 
	XP_ASSERT (NULL != m_db);

	DBMessageHdr *headers = NULL;
	if (eSUCCESS == m_db->ListFirst (&m_listContext, &headers) && headers)
	{
		key = headers->GetMessageKey ();
		delete headers;
	}

	return key;
}


MessageKey MSG_MapiListContext::GetNext ()
{
	MessageKey key = MSG_MESSAGEKEYNONE;

	XP_ASSERT (m_listContext && m_db);
	if (m_listContext && m_db)
	{
		DBMessageHdr *headers = NULL;
		if (eSUCCESS == m_db->ListNext (m_listContext, &headers))
		{
			key = headers->GetMessageKey ();
			delete headers;
		}
	}

	return key;
}


MsgERR MSG_MapiListContext::MarkRead (MessageKey key, XP_Bool read)
{
	MsgERR err = eFAILURE;
	XP_ASSERT(m_db);
	if (m_db)
		err = m_db->MarkRead (key, read);
	return err;
}


lpMapiMessage MSG_MapiListContext::GetMessage (MessageKey key)
{
	lpMapiMessage message = (lpMapiMessage) XP_CALLOC (1, sizeof(MapiMessage));
	if (message)
	{
		char scratch[256];

		MailMessageHdr *header = m_db->GetMailHdrForKey (key);
		if (header)
		{
			header->GetSubject (scratch, sizeof(scratch), TRUE /*withRe?*/, m_db->GetDB());
			message->lpszSubject = XP_STRDUP (scratch);
			message->lpszDateReceived = ConvertDateToMapiFormat (header->GetDate());

			// Pull out the flags info
			// anything to do with MAPI_SENT? Since we're only reading the Inbox, I guess not
			uint32 ourFlags = header->GetFlags();
			if (!(ourFlags & kIsRead))
				message->flFlags |= MAPI_UNREAD;
			if (ourFlags & kMDNNeeded || ourFlags & kMDNSent)
				message->flFlags |= MAPI_RECEIPT_REQUESTED;

			// Pull out the author/originator info
			message->lpOriginator = (lpMapiRecipDesc) XP_CALLOC (1, sizeof(MapiRecipDesc));
			if (message->lpOriginator)
			{
				header->GetAuthor (scratch, sizeof(scratch), m_db->GetDB());
				ConvertRecipientsToMapiFormat (scratch, message->lpOriginator, 1, MAPI_ORIG);
			}

			// Pull out the To/CC info
			int32 numToRecips = header->GetNumRecipients();
			int32 numCCRecips = header->GetNumCCRecipients();
			message->lpRecips = (lpMapiRecipDesc) XP_CALLOC (numToRecips + numCCRecips, sizeof(MapiRecipDesc));
			if (message->lpRecips)
			{
				XPStringObj ourRecips;

				header->GetRecipients (ourRecips, m_db->GetDB());
				ConvertRecipientsToMapiFormat (ourRecips, message->lpRecips, numToRecips, MAPI_TO);

				header->GetCCList (ourRecips, m_db->GetDB());
				ConvertRecipientsToMapiFormat (ourRecips, &message->lpRecips[numToRecips], numCCRecips, MAPI_CC);

				message->nRecipCount = numToRecips + numCCRecips;
			}

			// Convert any body text that we have locally
			message->lpszNoteText = ConvertBodyToMapiFormat (header);

			delete header;
		}
	}
	return message;
}


char *MSG_MapiListContext::ConvertDateToMapiFormat (time_t ourTime)
{
	char *date = (char*) XP_ALLOC(32);
	if (date)
	{
		// MAPI time format is YYYY/MM/DD HH:MM
		// Note that we're not using XP_StrfTime because that localizes the time format,
		// and the way I read the MAPI spec is that their format is canonical, not localized.
		struct tm *local = localtime (&ourTime);
		if (local)
			strftime (date, 32, "%Y/%m/%d %I:%M", local); //use %H if hours should be 24 hour format
	}
	return date;
}


void MSG_MapiListContext::ConvertRecipientsToMapiFormat (const char *ourRecips, lpMapiRecipDesc mapiRecips,
														 int numRecips, int mapiRecipClass)
{
	char *names = NULL;
	char *addresses = NULL;

	int count = MSG_ParseRFC822Addresses (ourRecips, &names, &addresses);
	if (count > 0)
	{
		char *walkNames = names;
		char *walkAddresses = addresses;
		for (int i = 0; i < count && i < numRecips; i++)
		{
			if (walkNames)
			{
				mapiRecips[i].lpszName = XP_STRDUP (walkNames);
				walkNames += XP_STRLEN (walkNames) + 1;
			}

			if (walkAddresses)
			{
				mapiRecips[i].lpszAddress = XP_STRDUP (walkAddresses);
				walkAddresses += XP_STRLEN (walkAddresses) + 1;
			}

			mapiRecips[i].ulRecipClass = mapiRecipClass;
		}
	}

	XP_FREEIF (names);
	XP_FREEIF (addresses);
}


char *MSG_MapiListContext::ConvertBodyToMapiFormat (MailMessageHdr *hdr)
{
	const int kBufLen = 64000;
	int bytesUsed = 0;
	int bytesThisLine;

	char *body = (char*) XP_ALLOC (kBufLen);
	if (body)
	{
		MSG_ScopeTerm *scope = new MSG_ScopeTerm (NULL /*search frame not needed, I hope*/, scopeMailFolder, m_folder);
		if (scope)
		{
			MSG_BodyHandler *handler = new MSG_BodyHandler (scope, hdr->GetArticleNum(), hdr->GetLineCount(), hdr, m_db);
			if (handler)
			{
				*body = '\0';
				while (TRUE)
				{
					bytesThisLine = handler->GetNextLine (&body[bytesUsed], kBufLen - bytesUsed);
					bytesUsed += bytesThisLine;
					if (0 == bytesThisLine)
						break;
					else
						bytesUsed--; // strip off null terminator
				}
        body[bytesUsed++] = '\0';   // rhp - fix last line garbage...
				delete handler;
			}
			delete scope;
		}
	}
	return body = (char*) XP_REALLOC(body, bytesUsed);
}


//*****************************************************************************
// MSGMAPI API implementation


extern "C" MessageKey MSG_GetFirstKeyInFolder (MSG_FolderInfo *folder, MSG_MapiListContext **context)
{
	MessageKey key = MSG_MESSAGEKEYNONE;

	*context = new MSG_MapiListContext ();
	if (*context)
	{
		if (eSUCCESS == (*context)->OpenDatabase (folder))
			key = (*context)->GetFirst ();
	}
	else
		delete *context;

	return key;
}


extern "C" MessageKey MSG_GetNextKeyInFolder (MSG_MapiListContext *context)
{
	MessageKey key = MSG_MESSAGEKEYNONE;

	XP_ASSERT(context);
	if (context)
	{
		key = context->GetNext();
		if (MSG_MESSAGEKEYNONE == key)  // Do we know they're always going to run to completion?
			delete context;
	}

	return key;
}


extern "C" XP_Bool MSG_GetMapiMessageById (MSG_FolderInfo *folder, MessageKey key, lpMapiMessage *message)
{
	XP_Bool success = FALSE;
	MSG_MapiListContext *context = new MSG_MapiListContext();
	if (context)
	{
		if (eSUCCESS == context->OpenDatabase(folder))
		{
			*message = context->GetMessage (key);
			success = TRUE;
		}
		delete context;
	}
	return success;
}


static void msg_FreeMAPIFile(lpMapiFileDesc f)
{
	if (f)
	{
		XP_FREEIF(f->lpszPathName);
		XP_FREEIF(f->lpszFileName);
	}
}

static void msg_FreeMAPIRecipient(lpMapiRecipDesc rd)
{
	if (rd)
	{
		XP_FREEIF(rd->lpszName);
		XP_FREEIF(rd->lpszAddress);
		XP_FREEIF(rd->lpEntryID);  
	}
}

extern "C" void MSG_FreeMapiMessage (lpMapiMessage msg)
{
	ULONG i;

	if (msg)
	{
		XP_FREEIF(msg->lpszSubject);
		XP_FREEIF(msg->lpszNoteText);
		XP_FREEIF(msg->lpszMessageType);
		XP_FREEIF(msg->lpszDateReceived);
		XP_FREEIF(msg->lpszConversationID);

		if (msg->lpOriginator)
			msg_FreeMAPIRecipient(msg->lpOriginator);

		for (i=0; i<msg->nRecipCount; i++)
			if (&(msg->lpRecips[i]) != NULL)
				msg_FreeMAPIRecipient(&(msg->lpRecips[i]));

		XP_FREEIF(msg->lpRecips);

		for (i=0; i<msg->nFileCount; i++)
			if (&(msg->lpFiles[i]) != NULL)
				msg_FreeMAPIFile(&(msg->lpFiles[i]));

		XP_FREEIF(msg->lpFiles);

		XP_FREE(msg);
	}
}


extern "C" XP_Bool MSG_MarkMapiMessageRead (MSG_FolderInfo *folder, MessageKey key, XP_Bool read)
{
	XP_Bool success = FALSE;
	MSG_MapiListContext *context = new MSG_MapiListContext();
	if (context)
	{
		if (eSUCCESS == context->OpenDatabase(folder))
		{
			if (eSUCCESS == context->MarkRead (key, read))
				success = TRUE;
		}
		delete context;
	}
	return success;
}


extern "C" int AB_MAPI_ResolveName(char * string,AB_ContainerInfo ** ctr, /* caller allocates ptr to ctr, BE fills. Caller must close the ctr when done */
						ABID * entryID)
{
	*ctr = NULL;
	*entryID = 0;
	return 0;
}

/* Caller must free the character strings returned by these functions using XP_FREE */
extern "C" char * AB_MAPI_GetEmailAddress(AB_ContainerInfo * ctr,ABID id)
{
	return XP_STRDUP("johndoe@netscape.com"); // just a temporary stub
}

extern "C" char * AB_MAPI_GetFullName(AB_ContainerInfo * ctr,ABID id)
{
	return XP_STRDUP("John Doe");
}


extern "C" char * AB_MAPI_ConvertToDescription(AB_ContainerInfo * ctr)
{
	AB_ContainerAttribValue * value;
	if (AB_GetContainerAttribute(ctr, attribName, &value) == AB_SUCCESS)
	{
		char * temp = XP_STRDUP(value->u.string);
		AB_FreeContainerAttribValue(value);
		return temp;
	}
	else
		return NULL;
}

extern "C" AB_ContainerInfo * AB_MAPI_ConvertToContainer(MWContext * context, char * description)
{
	return NULL;
}


extern "C" int AB_MAPI_CreatePropertySheetPane(MWContext * context,MSG_Master * master,AB_ContainerInfo * ctr, ABID id, MSG_Pane **  personPane)
{
	AB_PersonPane * pPane = new AB_PersonPane(context, master, ctr, id);
	*personPane = pPane;
	return AB_SUCCESS; // let Rich know what this value is???
}

extern "C" void AB_MAPI_CloseContainer(AB_ContainerInfo ** ctr)
{
	if (*ctr)
		(*ctr)->Close();
	*ctr = NULL;
}

#endif /* XP_WIN */

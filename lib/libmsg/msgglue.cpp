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
#include "dberror.h"
#include "client.h"
#include "xpgetstr.h"
#include "msgfpane.h"
#include "msgtpane.h"
#include "msgmpane.h"
#include "msgcpane.h"
#include "msgspane.h"
#include "msgprefs.h"
#include "msgmast.h"
#include "listngst.h"
#include "msgdb.h"
#include "msgdbvw.h"
#include "msgfinfo.h"
#include "msgcflds.h"
#include "newshost.h"
#include "hosttbl.h"
#include "subpane.h"
#include "newsdb.h"
#include "msgoffnw.h"
#include "shist.h"
#include "pmsgsrch.h"
#include "msgppane.h"
#include "mime.h"
#include "msgbg.h"
#include "nwsartst.h"
#include "maildb.h"
#include "mailhdr.h"
#include "imapoff.h"
#include "prefapi.h"
#include "imaphost.h"
#include "msgurlq.h"
#include "msgimap.h"
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


inline MSG_LinedPane* CastLinePane(MSG_Pane* pane) {
  XP_ASSERT(pane && pane->IsLinePane());
  return (MSG_LinedPane*) pane;
}

inline MSG_FolderPane* CastFolderPane(MSG_Pane* pane) {
  XP_ASSERT(pane && (pane->GetPaneType() == MSG_FOLDERPANE));
  return (MSG_FolderPane*) pane;
}

inline MSG_ThreadPane* CastThreadPane(MSG_Pane* pane) {
  XP_ASSERT(pane && pane->GetPaneType() == MSG_THREADPANE);
  return (MSG_ThreadPane*) pane;
}

inline MSG_MessagePane* CastMessagePane(MSG_Pane* pane) {
  XP_ASSERT(pane && pane->GetPaneType() == MSG_MESSAGEPANE);
  return (MSG_MessagePane*) pane;
}

inline MSG_CompositionPane* CastCompositionPane(MSG_Pane* pane) {
  XP_ASSERT(pane && pane->GetPaneType() == MSG_COMPOSITIONPANE);
  return (MSG_CompositionPane*) pane;
}

inline MSG_SubscribePane* CastSubscribePane(MSG_Pane* pane) {
	XP_ASSERT(pane && pane->GetPaneType() == MSG_SUBSCRIBEPANE);
	return (MSG_SubscribePane*) pane;
}

extern "C" MSG_Pane* MSG_FindPane(MWContext* context, MSG_PaneType type) {
  return MSG_Pane::FindPane(context, type, FALSE);
}

/* really find the pane of passed type with given context, NULL otherwise */
extern "C" MSG_Pane *MSG_FindPaneOfContext (MWContext *context, MSG_PaneType type)
{
  return MSG_Pane::FindPane(context, type, TRUE);
}

extern "C" MSG_Pane *MSG_FindPaneOfType(MSG_Master* master, MSG_FolderInfo *id, 
										MSG_PaneType type) 
{
  return master->FindPaneOfType(id, type);
}

extern "C" MSG_Pane *MSG_FindPaneFromUrl (MSG_Pane *pane, const char *url,
										  MSG_PaneType type)
{
	return pane->GetMaster()->FindPaneOfType (url, type);
}

HG52432

extern "C" XP_Bool
MSG_RequiresMailMsgWindow(const char *url)
{
	if (!url)
		return FALSE;
	if (!strncasecomp (url, "mailbox:", 8) || !strncasecomp (url, "IMAP:", 5))
	{
		HG83647
		char *q = XP_STRCHR (url, '?');
		/* If we're copying messages, we don't need a new window */
		if (!XP_STRNCMP (url, "mailbox:copymessages", 20))
			return FALSE;
		/* If this is a reference to a folder, we don't need a mail msg window. */
		if (!q)
			return FALSE;
		/* If it is a reference to a message, we require one. */
		else if (!XP_STRNCMP (q, "?id=", 4) || XP_STRSTR (q, "&id=") || !XP_STRNCMP(q, "?fetch",6))
		{
			if (XP_STRSTR(url, "?part=") || XP_STRSTR(url, "&part="))
				return FALSE;
			else
				return TRUE;
		}
		else
			return FALSE;
	}
	else
		return FALSE;
}

/* in mknews.c. */
extern "C" XP_Bool NET_IsNewsMessageURL (const char *url);
extern "C" XP_Bool NET_IsNewsServerURL( const char *url );

extern "C" XP_Bool
MSG_RequiresNewsMsgWindow(const char *url)
{
	if (!url) return FALSE;
	if (*url == 's' || *url == 'S')
		url++;
	if (!strncasecomp (url, "news:", 5))
	{
		/* If we're asking for a message ID, a news msg window is required. */
		return NET_IsNewsMessageURL (url);
	}
	return FALSE;
}

extern "C" XP_Bool
MSG_RequiresFolderPane(const char *url)
{
	if (!url) return FALSE;
	if (*url == 's' || *url == 'S')
		url++;

	if (!XP_STRCMP(url, "news:")
		|| !XP_STRCMP(url, "mailbox:") || (!strncasecomp (url, "news:", 5) && NET_IsNewsServerURL(url)))
		return TRUE;

	return FALSE;
}

/* Certain URLs must always be displayed in certain types of windows:
   for example, all "mailbox:" URLs must go in the mail window, and
   all "http:" URLs must go in a browser window.  These predicates
   look at the address and say which window type is required.
 */
extern "C" XP_Bool
MSG_RequiresMailWindow (const char *url)
{
	if (!url) return FALSE;
	if (!strncasecomp (url, "pop3:", 5))
		return TRUE;
	if (!strncasecomp (url, "mailbox:", 8) || !strncasecomp(url, "IMAP:", 5))
	{
		HG62453
		char *q = XP_STRCHR (url, '?');
		/* If we're copying messages, we don't need a new window */
		if (!XP_STRNCMP (url, "mailbox:copymessages", 20))
			return FALSE;
		/* If this is a reference to a folder, it requires a mail window. */
		if (!q)
			return TRUE;
		if (XP_STRSTR(url, "addmsgflags"))
			return FALSE;
		/* If this is a mailbox discovery URL, we don't necessarily need
		   a mail window.  (It can be run in the subscribe pane. */
		if (XP_STRSTR(url, "discoverallboxes") ||
			XP_STRSTR(url, "discoverchildren") ||
			XP_STRSTR(url, "discoverallandsubscribed") ||
			XP_STRSTR(url, "discoverlevelchildren"))
			return FALSE;

		// committing subscriptions can happen in the subscribe pane also
		if (XP_STRSTR(url, "subscribe") ||
			XP_STRSTR(url, "unsubscribe"))
			return FALSE;
		// should we make sure we have an "id=" ??
		if (XP_STRSTR(url, "?part=") || XP_STRSTR(url, "&part="))
			return FALSE;
		else
			return TRUE;
		/* If it is a reference to a message, it doesn't require one. */
/*		else if (XP_STRNCMP (q, "id=", 3) || XP_STRSTR (q, "&id="))
			return FALSE; */
	}
	// we'll arbitrarily say that folder panes are mail windows.
	return MSG_RequiresFolderPane(url);
}

extern "C" XP_Bool
MSG_RequiresNewsWindow (const char *url)
{
	XP_Bool	ret = FALSE;
	if (!url) return FALSE;
	if (*url == 's' || *url == 'S')
		url++;
	if (!strncasecomp (url, "news:", 5))
	{
		if (NET_IsNewsMessageURL(url))
			return TRUE;
		char *groupName = NET_ParseURL(url, GET_PATH_PART);
		if (groupName)
		{
			// don't say we need a news window for listing newsgroups or updating totals.
			// groupName comes back with leading '/', so just a '/' isn't a group.
			ret = XP_STRLEN(groupName) > 1 && !XP_STRCHR(groupName, '*');
			XP_FREE(groupName);

			// don't say we need a news window for deleting profile groups. Just
			// let the URL run in the folder pane.
			char *searchPart = NET_ParseURL(url, GET_SEARCH_PART);
			if (XP_STRSTR(searchPart, "?profile/PROFILE DEL"))
				ret = FALSE;
			if (XP_STRSTR(searchPart, "?list-ids"))
				ret = FALSE;
			XP_FREE(searchPart);
		}
	}
	  /* If we're asking for a message ID, it's ok to display those in a
		 browser window.  Otherwise, a news window is required. */
//	  return !NET_IsNewsMessageURL (url);
  return ret;
}

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

  /* otherwise this is probably an addbook: url */
  /* must check *after* browser window so http URLs from AB work */
  if (context->type == MWContextAddressBook) 
	  return FALSE;

  MSG_Pane *msgPane = MSG_FindPaneOfContext(context, MSG_MESSAGEPANE);
  // if this msg pane has an associated thread pane, and an associated folder,
  // make sure it's in the right folder
  MSG_Pane *threadPane = MSG_FindPaneOfContext(context, MSG_THREADPANE);
  MSG_Pane *folderPane = MSG_FindPaneOfContext(context, MSG_FOLDERPANE);

  if (!folderPane && MSG_RequiresFolderPane(url))
	  return TRUE;

// this checks for switching between mail and news with the dropdown (or reuse thread pane)
  if (threadPane && threadPane->GetFolder() && threadPane->GetFolder()->IsMail() && MSG_RequiresNewsWindow(url))
	  return TRUE;

  // url is running from folder pane - we need a new window for newsgroup or news msg.
  if (folderPane && !threadPane && MSG_RequiresNewsWindow(url))	 // standalone folder pane
	  return TRUE;

  if (msgPane && msgPane->GetFolder() &&!threadPane)	// standalone message window
  {
	  int urlType = NET_URL_Type(url);

	  // this is to make following links to newsgroups from standalone
	  // message panes work.
	  if (urlType == NEWS_TYPE_URL && MSG_PaneTypeForURL(url) == MSG_THREADPANE)
		  return TRUE;
  }
  if (msgPane && msgPane->GetFolder() && threadPane)
  { 
	  MSG_Pane *threadPaneForFolder = MSG_FindPaneFromUrl(msgPane, url, MSG_THREADPANE);
	  if (MSG_RequiresNewsWindow(url))
	  {
		  if (NET_IsNewsMessageURL(url))	// we don't know what folder news messages belong to.
		  {
			  if (threadPane->GetFolder() && !threadPane->GetFolder()->IsNews())
				  return TRUE;
			  else
				  return FALSE;
		  }
		  else
		  {
#ifdef FE_IMPLEMENTS_NEW_GET_FOLDER_INFO
			MSG_FolderInfo *folderForURL = MSG_GetFolderInfoFromURL(threadPane->GetMaster(), url, TRUE);
#else
			MSG_FolderInfo *folderForURL = MSG_GetFolderInfoFromURL(threadPane->GetMaster(), url);
#endif
			if (threadPane->GetFolder() != folderForURL)
				return TRUE;
		  }
	  }
	  else 
	  {
		  	// if we know it's not the right thread pane, tell fe we need a new window.
		  if (threadPane->GetFolder() && threadPaneForFolder && threadPane != threadPaneForFolder)
			  return TRUE;												
	  }
  }
  if (/* This is not a mail window, and one is required. */
      (context->type != MWContextMail && context->type != MWContextMailMsg &&
	  context->type != MWContextMailNewsProgress &&
       MSG_RequiresMailWindow (url)) ||

      /* This is not a news/mail window, and one is required. */
      (context->type != MWContextNews && context->type != MWContextMail && context->type != MWContextMailNewsProgress
	  && (context->type != MWContextNewsMsg  && context->type != MWContextMailMsg) && MSG_RequiresNewsWindow (url)) ||

	  /* this is not a message window, and one is required */
	  (msgPane == NULL && context->type != MWContextMailMsg && MSG_RequiresMailMsgWindow(url)) 

	  )  
    return TRUE;
  else
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
	void MSG_InitMsgLib(void);
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

extern "C" void MSG_ImapBiff(MWContext* context, MSG_Prefs* prefs) {
	MSG_Master *master = prefs->GetMasterForBiff();
	MSG_Pane *mailPane = NULL;

	if (master)
	{
		MSG_IMAPFolderInfoMail *inboxFolder = NULL;

		mailPane = (MSG_Pane*) master->FindFirstPaneOfType(MSG_FOLDERPANE);
		if (master->GetImapMailFolderTree() &&
			master->GetImapMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX,
			(MSG_FolderInfo**) &inboxFolder, 1))
		{
			if (!inboxFolder->GetGettingMail())
			{
				inboxFolder->SetGettingMail(TRUE);
				inboxFolder->Biff(context);
			}
		}
	}
}


extern "C" void MSG_StartImapMessageToOfflineFolderDownload(MWContext* context) {
	if (context->msgCopyInfo && context->msgCopyInfo->srcFolder)
	{
		if (context->msgCopyInfo->srcFolder->GetType() == FOLDER_IMAPMAIL)
		{
			MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) context->msgCopyInfo->srcFolder;
			imapFolder->SetDownloadState(MSG_IMAPFolderInfoMail::kDownLoadMessageForCopy);
		}
	}
}


extern "C" MSG_PaneType MSG_PaneTypeForURL(const char *url)
{
	return MSG_Pane::PaneTypeForURL(url);
}

extern "C" MSG_Pane* MSG_CreateFolderPane(MWContext* context,
										  MSG_Master* master) {
  MSG_FolderPane *pane = new MSG_FolderPane(context, master);
  if (pane)
	  pane->Init();
  return pane;
}

extern "C" MSG_Pane* MSG_CreateThreadPane(MWContext* context,
										  MSG_Master* master) {
  return new MSG_ThreadPane(context, master);
}

extern "C" MSG_Pane* MSG_CreateMessagePane(MWContext* context,
										   MSG_Master* master) {
  return new MSG_MessagePane(context, master);
}


extern "C" int MSG_SetMessagePaneCallbacks(MSG_Pane* messagepane,
										   MSG_MessagePaneCallbacks* c,
										   void* closure) {
	return CastMessagePane(messagepane)->SetMessagePaneCallbacks(c, closure);
}


extern "C" MSG_MessagePaneCallbacks*
MSG_GetMessagePaneCallbacks(MSG_Pane* messagepane,
							void** closure)
{
	return CastMessagePane(messagepane)->GetMessagePaneCallbacks(closure);
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


extern "C" MSG_Pane *MSG_CreateSearchPane (MWContext *context, MSG_Master *master)
{
	return new MSG_SearchPane (context, master);
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

extern "C" XP_Bool MSG_DisplayingRecipients(MSG_Pane* pane) 
{
	return pane ? pane->DisplayingRecipients() : FALSE;
}

extern "C" int MSG_AddToAddressBook(MSG_Pane* pane, MSG_CommandType command, MSG_ViewIndex* indices, int32 numIndices, AB_ContainerInfo * destAB)
{
	if (pane)
		return ConvertMsgErrToMKErr(pane->AddToAddressBook(command, indices, numIndices, destAB));
	else
		return 0;
}

/* We also provide a status function to support this command */
extern "C" int MSG_AddToAddressBookStatus(MSG_Pane* pane, MSG_CommandType command, MSG_ViewIndex* indices, int32 numIndices, XP_Bool *selectable_p, MSG_COMMAND_CHECK_STATE *selected_p,
									  const char **display_string, XP_Bool *plural_p, AB_ContainerInfo * /* destAB */)
{
	return ConvertMsgErrToMKErr(pane->GetCommandStatus(command,
													 indices, numIndices,
													 selectable_p,
													 selected_p,
													 display_string,
													 plural_p));
}

extern "C" int MSG_GetFolderMaxDepth(MSG_Pane* folderpane) {
  return CastFolderPane(folderpane)->GetFolderMaxDepth();
}


extern "C" XP_Bool MSG_GetFolderLineByIndex(MSG_Pane* folderpane,
											MSG_ViewIndex line,
											int32 numlines,
											MSG_FolderLine* data) {
  return CastFolderPane(folderpane)->GetFolderLineByIndex(line, numlines,
														  data);
}

extern "C" uint32 MSG_GetFolderFlags(MSG_FolderInfo *folder)
{
	return folder->GetFlags();
}


extern "C" XP_Bool MSG_GetThreadLineByIndex(MSG_Pane* threadpane,
											MSG_ViewIndex line,
											int32 numlines,
											MSG_MessageLine* data) {
  return CastThreadPane(threadpane)->GetThreadLineByIndex(line, numlines,
														  data);
}

extern "C" int MSG_GetFolderLevelByIndex( MSG_Pane *folderpane,
										  MSG_ViewIndex line ) {
  return CastFolderPane(folderpane)->GetFolderLevelByIndex( line );
}

extern "C" int MSG_GetThreadLevelByIndex( MSG_Pane *threadpane,
										  MSG_ViewIndex line ) {
  return CastThreadPane(threadpane)->GetThreadLevelByIndex( line );
}


extern "C" XP_Bool MSG_GetFolderLineById(MSG_Master* master,
										 MSG_FolderInfo* info,
										 MSG_FolderLine* data) {
  return master->GetFolderLineById(info, data);
}

extern "C" int32 MSG_GetFolderSizeOnDisk (MSG_FolderInfo *folder)
{
	return folder->GetSizeOnDisk();
}

extern "C" XP_Bool MSG_GetThreadLineById(MSG_Pane* pane,
										 MessageKey key,
										 MSG_MessageLine* data) {
	if (MSG_THREADPANE == pane->GetPaneType())
		return CastThreadPane(pane)->GetThreadLineByKey(key, data);
	else
		if (MSG_MESSAGEPANE == pane->GetPaneType())
			return CastMessagePane(pane)->GetThreadLineByKey(key, data);
		else
		{
			XP_ASSERT(FALSE);
			return FALSE;
		}
}

extern "C" MSG_ViewIndex MSG_ThreadIndexOfMsg(MSG_Pane* pane, MessageKey key)
{
	return pane->GetThreadIndexOfMsg(key);
}

extern "C" XP_Bool MSG_SetPriority(MSG_Pane *pane, /* thread or message */
						   MessageKey key, 
						   MSG_PRIORITY priority)
{
	return pane->SetMessagePriority(key, priority);
}

extern "C" const char* MSG_FormatDate(MSG_Pane* pane, time_t date) {
  return MSG_FormatDateFromContext(pane->GetContext(), date);
}


extern "C" void MSG_DestroyMaster (MSG_Master* master) {
	delete master;
}


extern "C" void MSG_DestroyPane(MSG_Pane* pane) {
  delete pane;
}


extern "C" int MSG_LoadFolder(MSG_Pane* pane, MSG_FolderInfo* folder)
{
	MsgERR status = 0; 
	if (MSG_THREADPANE == pane->GetPaneType())
		status = CastThreadPane(pane)->LoadFolder(folder);
	else if (MSG_MESSAGEPANE == pane->GetPaneType())
		status = CastMessagePane(pane)->LoadFolder(folder);

	if ((int) status == MK_MSG_CANT_OPEN)
	{
		char *statusMsg = PR_smprintf(XP_GetString(MK_MSG_CANT_OPEN), folder->GetName());
		if (statusMsg)
		{
			FE_Alert(pane->GetContext(), statusMsg);
			XP_FREE(statusMsg);
		}
	}
	return status;
//  return ConvertMsgErrToMKErr(CastThreadPane(threadpane)->LoadFolder(folder));
}

//extern "C" int MSG_LoadFolderURL(MSG_Pane *threadPane, const char *url) {
//  return ConvertMsgErrToMKErr(CastThreadPane(threadPane)->LoadFolder(url));
//}

extern "C" int MSG_LoadMessage(MSG_Pane* messagepane, MSG_FolderInfo* folder,
							   MessageKey key) {
  return
	ConvertMsgErrToMKErr(CastMessagePane(messagepane)->LoadMessage(folder,
																   key, NULL, TRUE));
}
//extern "C" int MSG_LoadMessageURL(MSG_Pane* messagepane, const char *url) {
//  return
//	ConvertMsgErrToMKErr(CastMessagePane(messagepane)->LoadMessage(url));
//}


extern "C" int MSG_OpenDraft(MSG_Pane* threadpane, MSG_FolderInfo* folder,
							 MessageKey key) {
	return
	  ConvertMsgErrToMKErr(CastThreadPane(threadpane)->OpenDraft(folder, key));
}

extern "C" void MSG_SetLineWidth(MSG_Pane* composepane, int width)
{
	CastCompositionPane(composepane)->SetLineWidth(width);
}

extern "C" void MSG_AddBacktrackMessage (MSG_Pane *pane, MSG_FolderInfo* folder,
										 MessageKey key)
{
	BacktrackManager *backtrackManager = pane->GetBacktrackManager();
	backtrackManager->AddEntry(folder, key);
}

extern "C" void MSG_SetBacktrackState(MSG_Pane *pane, MSG_BacktrackState state)
{
	pane->GetBacktrackManager()->SetState(state);
}

extern "C" MSG_BacktrackState MSG_GetBacktrackState (MSG_Pane *pane)
{
	return pane->GetBacktrackManager()->GetState();
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

extern "C" void MSG_SetUserAuthenticated (MSG_Master *master, XP_Bool bAuthenticated)
{
	master->SetUserAuthenticated(bAuthenticated);
}

extern "C" XP_Bool MSG_IsUserAuthenticated (MSG_Master *master)
{
	return master->IsUserAuthenticated();
}

extern "C" void MSG_SetMailAccountURL (MSG_Master *master, const char *urlString)
{
	master->SetMailAccountURL(urlString);
}

extern "C" void MSG_SetHostMailAccountURL (MSG_Master *master, const char *hostName, const char *urlString)
{
	MSG_IMAPHost *host = master->GetIMAPHostTable()->FindIMAPHost(hostName);
	if (host)
		host->SetAdminURL(urlString);
}

extern "C" const char* MSG_GetMailAccountURL (MSG_Master *master)
{
	return master->GetMailAccountURL();
}

extern "C" void MSG_SetHostManageFiltersURL (MSG_Master *master, const char *hostName, const char *urlString)
{
	MSG_IMAPHost *host = master->GetIMAPHostTable()->FindIMAPHost(hostName);
	if (host)
		host->SetManageFiltersURL(urlString);
}

extern "C" void MSG_SetHostManageListsURL (MSG_Master *master, const char *hostName, const char *urlString)
{
	MSG_IMAPHost *host = master->GetIMAPHostTable()->FindIMAPHost(hostName);
	if (host)
		host->SetManageListsURL(urlString);
}

extern "C" MSG_Master* MSG_GetMaster (MSG_Pane *pane)
{
	return pane->GetMaster();
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

extern "C" void MSG_SetPostDeliveryActionInfo (MSG_Pane* pane,
											   void *actionInfo) 
{
	if (pane)
		pane->SetPostDeliveryActionInfo ((MSG_PostDeliveryActionInfo
										  *) actionInfo); 
}

extern "C" uint32 MSG_GetActionInfoFlags(void *actionInfo)
{
	if (actionInfo)
		return ((MSG_PostDeliveryActionInfo *) actionInfo)->m_flags;
	else
		return 0;
}

extern "C" void MSG_SetIMAPMessageUID (MessageKey key,
									   void *state) 
{
	if (state)
		((MSG_SendMimeDeliveryState *) state)->SetIMAPMessageUID(key);
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


#ifdef XP_UNIX
extern "C" void 
MSG_IncorporateFromFile(MSG_Pane* pane, XP_File infid,
						void (*donefunc)(void*, XP_Bool),
						void* doneclosure)
{
  pane->IncorporateFromFile(infid,
			    donefunc, doneclosure);
}

#endif


extern "C" MSG_Prefs* MSG_CreatePrefs() {
  return new MSG_Prefs();
}

extern "C" void MSG_DestroyPrefs(MSG_Prefs* prefs) {
  delete prefs;
}


extern "C" void MSG_SetFolderDirectory(MSG_Prefs* prefs,
									   const char* directory) {
  prefs->SetFolderDirectory(directory);
}

extern "C" const char* MSG_GetFolderDirectory(MSG_Prefs* prefs) {
  return prefs->GetFolderDirectory();
}

extern "C" void MSG_GetCitationStyle(MSG_Prefs* prefs,
									 MSG_FONT* font,
									 MSG_CITATION_SIZE* size,
									 const char** color) {
  prefs->GetCitationStyle(font, size, color);
}

extern "C" XP_Bool MSG_GetMailServerIsIMAP4(MSG_Prefs* prefs) {
	return prefs->GetMailServerIsIMAP4();
}

HG42326

// ### mwelch Now that the standard folder names are i18n strings,
//            I'm putting them into easily accessible static pointers.
//            The reason for this is because XP strings can move around
//            in memory, and fall away at bad times.
static const char *sInboxName = NULL;
static const char *sQueueName = NULL;
static const char *sQueueOldName = NULL;
static const char *sTrashName = NULL;
static const char *sDraftsName = NULL;
static const char *sSentName = NULL;
static const char *sTemplatesName = NULL;

extern "C" const char *MSG_GetSpecialFolderName(int whichOne)
{
	const char *returnValue = NULL;
	
	if (sInboxName == NULL)
	{
		// first time through, need to initialize
#ifndef XP_OS2
		sInboxName = XP_STRDUP(XP_GetString(MK_MSG_INBOX_L10N_NAME));
		sQueueName = XP_STRDUP(XP_GetString(MK_MSG_OUTBOX_L10N_NAME));
		sQueueOldName = XP_STRDUP(XP_GetString(MK_MSG_OUTBOX_L10N_NAME_OLD));
		sTrashName = XP_STRDUP(XP_GetString(MK_MSG_TRASH_L10N_NAME));
		sDraftsName = XP_STRDUP(XP_GetString(MK_MSG_DRAFTS_L10N_NAME));
		sSentName = XP_STRDUP(XP_GetString(MK_MSG_SENT_L10N_NAME));
		sTemplatesName = XP_STRDUP(XP_GetString(MK_MSG_TEMPLATES_L10N_NAME));
#else
      sInboxName = XP_STRDUP("Inbox");
      sQueueName = XP_STRDUP("Unsent");
      sQueueOldName = XP_STRDUP("Outbox");
      sTrashName = XP_STRDUP("Trash");
      sDraftsName = XP_STRDUP("Drafts");
      sSentName = XP_STRDUP("Sent");
	  sTemplatesName = XP_STRDUP("Templates");
#endif
	}

	if(whichOne == MK_MSG_INBOX_L10N_NAME)
		returnValue = sInboxName;
	else if(whichOne == MK_MSG_OUTBOX_L10N_NAME)
		returnValue = sQueueName;
	else if(whichOne == MK_MSG_OUTBOX_L10N_NAME_OLD)
		returnValue = sQueueOldName;
	else if(whichOne == MK_MSG_TRASH_L10N_NAME)
		returnValue = sTrashName;
	else if(whichOne == MK_MSG_DRAFTS_L10N_NAME)
		returnValue = sDraftsName;
	else if(whichOne == MK_MSG_SENT_L10N_NAME)
		returnValue = sSentName;
	else if(whichOne == MK_MSG_TEMPLATES_L10N_NAME)
		returnValue = sTemplatesName;
	else
		XP_ASSERT(!"Bad index passed to MSG_GetSpecialFolderName.");
	return returnValue;
}

#ifdef XP_OS2
// Netscape implementation of Folder names is pretty flawed. The file names of
// the special folders should be the same, regardless of the language. All
// that should change is the PrettyName. This additional code allows me to
// query the pretty names, while the above code uses the actual English
// names to represent the physical files.
static const char *sInboxPrettyName = NULL;
static const char *sQueuePrettyName = NULL;
static const char *sQueueOldPrettyName = NULL;
static const char *sTrashPrettyName = NULL;
static const char *sDraftsPrettyName = NULL;

extern "C" const char *MSG_GetSpecialFolderPrettyName(int whichOne)
{
        const char *returnValue = NULL;

        if (sInboxPrettyName == NULL)
        {
                // first time through, need to initialize
                sInboxPrettyName = XP_STRDUP(XP_GetString(MK_MSG_INBOX_L10N_NAME));
                sQueuePrettyName = XP_STRDUP(XP_GetString(MK_MSG_OUTBOX_L10N_NAME));
                sQueueOldPrettyName = XP_STRDUP(XP_GetString(MK_MSG_OUTBOX_L10N_NAME_OLD));
                sTrashPrettyName = XP_STRDUP(XP_GetString(MK_MSG_TRASH_L10N_NAME));
                sDraftsPrettyName = XP_STRDUP(XP_GetString(MK_MSG_DRAFTS_L10N_NAME));
        }

        if(whichOne == MK_MSG_INBOX_L10N_NAME)
                returnValue = sInboxPrettyName;
        else if(whichOne == MK_MSG_OUTBOX_L10N_NAME)
                returnValue = sQueuePrettyName;
        else if(whichOne == MK_MSG_OUTBOX_L10N_NAME_OLD)
                returnValue = sQueueOldPrettyName;
        else if(whichOne == MK_MSG_TRASH_L10N_NAME)
                returnValue = sTrashPrettyName;
        else if(whichOne == MK_MSG_DRAFTS_L10N_NAME)
                returnValue = sDraftsPrettyName;
        else
                XP_ASSERT(!"Bad index passed to MSG_GetSpecialFolderName.");
        return returnValue;
}
#endif


// Current queue folder name
static const char *sCurrentQueueFolderName = NULL;

extern "C" const char * MSG_GetQueueFolderName() {
	// Initial value is the i18n queue folder name, if it hasn't
	// been tinkered with by MSG_SetQueueFolderName.
	if (sCurrentQueueFolderName == NULL)
		sCurrentQueueFolderName = QUEUE_FOLDER_NAME;
	// don't free the return value
	return (sCurrentQueueFolderName);
}

extern "C" const char * MSG_SetQueueFolderName(const char *newName) {
	sCurrentQueueFolderName = newName;
	return sCurrentQueueFolderName;
}

extern "C" XP_Bool
MSG_GetNoInlineAttachments(MSG_Prefs* prefs)
{
	return prefs->GetNoInlineAttachments();
}

extern "C" const char* MSG_GetPopHost(MSG_Prefs* prefs) {
  return prefs->GetPopHost();
}

extern "C" XP_Bool MSG_GetAutoQuoteReply(MSG_Prefs* prefs) {
  return prefs->GetAutoQuoteReply();
}

extern "C" void MSG_ToggleExpansion (MSG_Pane* pane, MSG_ViewIndex line,
									 int32* numchanged) {
  CastLinePane(pane)->ToggleExpansion(line, numchanged);
}


extern "C" int32 MSG_ExpansionDelta(MSG_Pane* pane, MSG_ViewIndex line) {
  return CastLinePane(pane)->ExpansionDelta(line);
}


extern "C" MSG_DragEffect MSG_DragMessagesStatus(MSG_Pane* pane,
									const MSG_ViewIndex* indices, int32 numIndices,
									const char* folderPath, MSG_DragEffect request)
{
  return pane->DragMessagesStatus(indices, numIndices, folderPath, request);
}

extern "C" int MSG_CopyMessagesInto(MSG_Pane* pane,
									const MSG_ViewIndex* indices, int32 numIndices,
									const char* folderPath) 
{
  return ConvertMsgErrToMKErr (pane->CopyMessages (indices, numIndices, folderPath, FALSE));
}

extern "C" int MSG_MoveMessagesInto(MSG_Pane* pane,
									const MSG_ViewIndex *indices, int32 numIndices,
									const char* folderPath) {
  return ConvertMsgErrToMKErr(pane->CopyMessages (indices, numIndices, folderPath, TRUE));
}

extern "C" MSG_DragEffect MSG_DragMessagesIntoFolderStatus(MSG_Pane *pane,
										   const MSG_ViewIndex *indices,
										   int32 numIndices,
										   MSG_FolderInfo *folder,
										   MSG_DragEffect request)
{
	return pane->DragMessagesStatus(indices, numIndices, folder, request);
}

extern "C" int MSG_CopyMessagesIntoFolder (MSG_Pane *pane,
										   const MSG_ViewIndex *indices,
										   int32 numIndices,
										   MSG_FolderInfo *folder)
{
	return ConvertMsgErrToMKErr( 
		pane->CopyMessages (indices, numIndices, folder,FALSE));
}

extern "C" int MSG_MoveMessagesIntoFolder (MSG_Pane *pane,
										   const MSG_ViewIndex *indices,
										   int32 numIndices,
										   MSG_FolderInfo *folder)
{
	return ConvertMsgErrToMKErr(
		pane->MoveMessages (indices, numIndices, folder));
}


extern "C" int MSG_MoveFoldersInto (MSG_Pane *folderPane,
									const MSG_ViewIndex *indices,
									int32 numIndices,
									MSG_FolderInfo *destFolder)
{
	return ConvertMsgErrToMKErr 
		(CastFolderPane(folderPane)->MoveFolders ((MSG_ViewIndex*)indices, numIndices, destFolder, TRUE /*needExitFunc*/));
}


extern "C" MSG_DragEffect MSG_DragFoldersIntoStatus(MSG_Pane *folderPane,
									const MSG_ViewIndex *indices,
									int32 numIndices,
									MSG_FolderInfo *destFolder,
									MSG_DragEffect request)
{
	return CastFolderPane(folderPane)->DragFoldersStatus(
		(MSG_ViewIndex*)indices, numIndices, destFolder, request);
}


extern "C" const char* MSG_GetFolderName(MSG_Pane* folderpane,
										 MSG_ViewIndex line) {
  return CastFolderPane(folderpane)->GetFolderName(line);
}


extern "C" int32 MSG_GetNumLines(MSG_Pane* pane) {
  return CastLinePane(pane)->GetNumLines();
}


extern "C" MSG_ViewIndex MSG_GetFolderIndex(MSG_Pane* folderpane,
										   MSG_FolderInfo* info) {
  return CastFolderPane(folderpane)->GetFolderIndex(info, FALSE);
}

/* This routine should replace the above routine when people port over to it.
	If expand is TRUE, we will expand the folderPane enough to
	expose the passed in folder info. Otherwise, if the folder is collapsed,
	we return MSG_ViewIndexNone.
*/
extern MSG_ViewIndex MSG_GetFolderIndexForInfo(MSG_Pane *folderpane, 
												MSG_FolderInfo *info,
												XP_Bool /*expand*/)
{
  return CastFolderPane(folderpane)->GetFolderIndex(info, TRUE);
}


extern "C" MSG_FolderInfo* MSG_GetFolderInfo(MSG_Pane* folderpane,
											 MSG_ViewIndex index) {
  return CastFolderPane(folderpane)->GetFolderInfo(index);
}

#ifdef FE_IMPLEMENTS_NEW_GET_FOLDER_INFO
extern "C" MSG_FolderInfo* MSG_GetFolderInfoFromURL(MSG_Master* master, const char *url, XP_Bool forGetUrl)
{
	if (url)
	{
		if (NET_URL_Type(url) == IMAP_TYPE_URL)
		{
			return master->GetFolderInfo(url, forGetUrl);	// If IMAP, we may want to auto-create the folderInfo when
															// trying to open it (i.e. someone clicked on an IMAP URL, for sharing folders)
															// Note that we may create the FolderInfo, but we do not actually create
															// the folder on the server.
		}
		else
		{
			return master->GetFolderInfo(url, FALSE);	// We don't want to auto-create any other types of folderInfos
		}
	}
	else
	{
		return NULL;	// no URL passed, return NULL.
	}
}
#else
extern "C" MSG_FolderInfo* MSG_GetFolderInfoFromURL(MSG_Master* master, const char *url)
{
	return master->GetFolderInfo(url, FALSE);
}
#endif

/* returns folder info of host owning this folder*/
extern "C" MSG_FolderInfo* GetHostFolderInfo(MSG_FolderInfo* info)
{
	MSG_FolderInfo *ret = 0;
	if (info)
	{
		MSG_IMAPFolderInfoMail	*imapFolder = info->GetIMAPFolderInfoMail();
		if (imapFolder)
			ret = imapFolder->GetIMAPContainer();
		if (!ret)
		{
			MSG_FolderInfoNews    *newsFolder = info->GetNewsFolderInfo();
			if (newsFolder && newsFolder->GetHost())
				ret = newsFolder->GetHost()->GetHostInfo();
		}
	}
	return ret;
}


extern "C" MSG_FolderInfo* MSG_GetThreadFolder(MSG_Pane* threadpane)
{
  return CastThreadPane(threadpane)->GetFolder();
}

#ifdef SUBSCRIBE_USE_OLD_API
extern "C" MSG_NewsHost *MSG_GetNewsHostFromIndex (MSG_Pane *folderPane, 
												   MSG_ViewIndex index)
{
	return CastFolderPane (folderPane)->GetNewsHostFromIndex (index);
}
#endif /* SUBSCRIBE_USE_OLD_API */

extern "C" MSG_Host *MSG_GetHostFromIndex (MSG_Pane *folderPane,
										   MSG_ViewIndex index)
{
	return CastFolderPane (folderPane)->GetHostFromIndex(index);
}

extern "C" int MSG_GetMessageLineForURL(MSG_Master *master, const char *url, MSG_MessageLine *msgLine)
{
	return master->GetMessageLineForURL(url, msgLine);
}

extern "C" MSG_ViewIndex MSG_GetMessageIndexForKey(MSG_Pane* threadpane, MessageKey key, XP_Bool expand)
{
  return CastThreadPane(threadpane)->GetMessageIndex(key, expand);
}


extern "C" MessageKey MSG_GetMessageKey(MSG_Pane* threadpane,
										 MSG_ViewIndex index) {
  return CastThreadPane(threadpane)->GetMessageKey(index);
}

extern "C" XP_Bool MSG_GetUndoMessageKey( MSG_Pane *pane, 
										  MSG_FolderInfo** pFolderInfo,
										  MessageKey* pKey)
{
	if (pFolderInfo && pKey) {
		*pKey = pane->GetUndoManager()->GetAndRemoveMsgKey();
		*pFolderInfo = pane->GetUndoManager()->GetUndoMsgFolder();
		return *pKey != MSG_MESSAGEKEYNONE;
	}
	return FALSE;
}

extern "C" UndoStatus MSG_GetUndoStatus(MSG_Pane *pane)
{
	return pane->GetUndoManager()->GetStatus();
}

extern "C" UndoMgrState MSG_GetUndoState(MSG_Pane *pane)
{
	return pane->GetUndoManager()->GetState();
}

extern "C" int MSG_GetKeyFromMessageId (MSG_Pane *pane, 
                             const char *messageId,
                             MessageKey *outId)
{
  XP_ASSERT(pane && messageId && outId);
  return pane->GetKeyFromMessageId (messageId,outId);
}

extern "C" int32 MSG_GetFolderChildren(MSG_Master* master,
									   MSG_FolderInfo* folder,
									   MSG_FolderInfo** result,
									   int32 resultsize) {
  return master->GetFolderChildren(folder, result, resultsize);
}

extern "C" MSG_FolderInfo* MSG_GetLocalMailTree(MSG_Master* master) {
	return master->GetLocalMailFolderTree();
}


extern "C" int32 MSG_GetFoldersWithFlag(MSG_Master* master,
										uint32 flags,
										MSG_FolderInfo** result,
										int32 resultsize) {
  return master->GetFoldersWithFlag(flags, result, resultsize);
}

extern "C" MSG_FolderInfo* MSG_GetCurFolder(MSG_Pane* pane) {
	return pane->GetFolder();
}

extern "C" int MSG_CreateMailFolderWithPane (MSG_Pane *invokingPane,
											 MSG_Master *master, 
											 MSG_FolderInfo *parent,
											 const char *childName) {
	return ConvertMsgErrToMKErr(master->CreateMailFolder (invokingPane, parent, childName));
}

extern "C" int MSG_CreateMailFolder (MSG_Master *master,
									 MSG_FolderInfo *parent,
									 const char *childName) {
	return ConvertMsgErrToMKErr(master->CreateMailFolder (NULL, parent, childName));
}

/* Call this from the new folder properties UI */
extern int MSG_RenameMailFolder (MSG_Pane *folderPane, MSG_FolderInfo *folder,
								 const char *newName)
{
	return ConvertMsgErrToMKErr(CastFolderPane(folderPane)->RenameFolder (folder, newName));
}

// This function is currently only used to store filter rules destination 
// folders.
extern "C" const char *MSG_GetFolderNameFromID(MSG_FolderInfo *f) {
	if (f->GetType() == FOLDER_MAIL 
		|| f->GetType() == FOLDER_IMAPMAIL)
	{
		MSG_FolderInfoMail *mailFolder = (MSG_FolderInfoMail *) f;
		return mailFolder->GetPathname();
	}
	return NULL;
}

extern "C" void MSG_GetCurMessage(MSG_Pane* messagepane,
								  MSG_FolderInfo** folder,
								  MessageKey* key,
								  MSG_ViewIndex *index) {
	CastMessagePane(messagepane)->GetCurMessage(folder, key, index);
}


extern int MSG_GetViewedAttachments(MSG_Pane* messagepane,
									MSG_AttachmentData** data,
									XP_Bool* iscomplete) {
	return CastMessagePane(messagepane)->GetViewedAttachments(data,
															  iscomplete);
}


extern void MSG_FreeAttachmentList(MSG_Pane* messagepane,
								   MSG_AttachmentData* data)
{
	CastMessagePane(messagepane)->FreeAttachmentList(data);
}



extern "C" URL_Struct * MSG_ConstructUrlForPane(MSG_Pane *pane)
{
	return pane->ConstructUrlForMessage();
}

extern "C" URL_Struct * MSG_ConstructUrlForMessage(MSG_Pane *pane, MessageKey key)
{
	return pane->ConstructUrlForMessage(key);
}

extern "C" URL_Struct*	MSG_ConstructUrlForFolder(MSG_Pane * /*pane*/, MSG_FolderInfo *folder)
{
	return MSG_Pane::ConstructUrl(folder);
}

extern "C" XP_Bool
MSG_ShouldRot13Message(MSG_Pane* messagepane)
{
	return CastMessagePane(messagepane)->ShouldRot13Message();
}

extern MSG_Pane*
MSG_GetParentPane(MSG_Pane* progresspane)
{
	return progresspane->GetParentPane();
}


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

	/* It's so cool that there are half a dozen entrypoints to
	   composition-window-creation. */
	HG62239

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

	/* It's so cool that there are half a dozen entrypoints to
	   composition-window-creation. */
	HG42933

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
					HG66663
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
	HG65243
	return fields;
}

extern "C" void
MSG_DestroyCompositionFields(MSG_CompositionFields *fields)
{
	delete fields;
}

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
					HG00282
					XP_Bool force_plain_text,
					const char* html_part
					)
{
	// Don't allow a compose window to be created if the user hasn't 
	// specified an email address
	const char *real_addr = FE_UsersMailAddress();
	if (MISC_ValidateReturnAddress(old_context, real_addr) < 0)
		return NULL;
	

	HG02872


	MSG_CompositionFields* fields =
		MSG_CreateCompositionFields(from, reply_to, to, cc, bcc,
									fcc, newsgroups, followup_to,
									organization, subject, references,
									other_random_headers, priority, attachment,
									newspost_url HG65241);

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


extern "C" XP_Bool
MSG_IsHTMLOK(MSG_Master* master, MSG_FolderInfo* group)
{
	return master->IsHTMLOK(group);
}


extern "C" int
MSG_SetIsHTMLOK(MSG_Master* master, MSG_FolderInfo* group, MWContext* context,
				XP_Bool value)
{
	return master->SetIsHTMLOK(group, context, value);
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









// ###tw   The below is all stuff called from netlib; it's probably all wrong.



extern "C" XP_Bool
MSG_BeginMailDelivery(MSG_Pane* pane)
{
  /* #### Switch to the right folder or something; return false if we
	 can't accept new mail for some reason (other than disk space.)
   */
	pane->BeginMailDelivery();
	return TRUE;
}

extern "C" void
MSG_AbortMailDelivery (MSG_Pane* pane)
{
	// not sure we have to do anything different.
	pane->EndMailDelivery();
}

extern "C" void
MSG_EndMailDelivery (MSG_Pane* pane)
{
	pane->EndMailDelivery();
}


extern "C" void*
MSG_IncorporateBegin(MSG_Pane* pane,
					 FO_Present_Types format_out,
					 char *pop3_uidl,
					 URL_Struct *url,
					 uint32 flags) 
{
	return pane->IncorporateBegin(format_out, pop3_uidl, url, flags);
}



extern "C"
int MSG_IncorporateWrite(MSG_Pane* pane, void *closure,
						 const char *block, int32 length)
{
	return pane->IncorporateWrite(closure, block, length);
}


extern "C"
int MSG_IncorporateComplete(MSG_Pane* pane, void *closure)
{
	return pane->IncorporateComplete(closure);
}


extern "C"
void MSG_ClearSenderAuthedFlag(MSG_Pane *pane, void *closure)
{
	if (pane && closure)
		pane->ClearSenderAuthedFlag(closure);
}


extern "C"
int MSG_IncorporateAbort(MSG_Pane* pane, void *closure, int status)
{
	return pane->IncorporateAbort(closure, status);
}


extern "C" int
MSG_MarkMessageKeyRead(MSG_Pane* pane, MessageKey key,
					  const char *xref)
{
  return pane->MarkMessageKeyRead(key, xref);
}


extern "C" void
MSG_ActivateReplyOptions(MSG_Pane* messagepane, MimeHeaders* headers)
{
  CastMessagePane(messagepane)->ActivateReplyOptions(headers);
}


extern "C" int
MSG_AddNewNewsGroup(MSG_Pane* pane, MSG_NewsHost* host,
					const char* groupname, int32 oldest, int32 youngest,
					const char *flag, XP_Bool bXactiveFlags)
{
	return pane->AddNewNewsGroup(host, groupname, oldest, youngest, flag, bXactiveFlags);
}

extern "C" int		
MSG_SetGroupNeedsExtraInfo(MSG_NewsHost *host, const char* groupname, XP_Bool needsExtra)
{
	return host->SetGroupNeedsExtraInfo(groupname, needsExtra);
}

extern "C" char *MSG_GetFirstGroupNeedingExtraInfo(MSG_NewsHost *host)
{
	return host->GetFirstGroupNeedingExtraInfo();
}

extern "C" int
MSG_AddPrettyName(MSG_NewsHost* host, const char *groupName,
				  const char *prettyName)
{
	return host->SetPrettyName(groupName, prettyName);
}

extern "C" int 	MSG_SetXActiveFlags(MSG_Pane* /*pane*/, char* /*groupName*/, 
								int32 /*firstPossibleArt*/, 
								int32 /*lastPossibleArt*/, 
								char* /*flags*/)
{
	return 0;
}


extern "C" time_t
MSG_NewsgroupsLastUpdatedTime(MSG_NewsHost* host)
{
	XP_ASSERT(host);
	if (!host) return 0;
	return host->getLastUpdate();
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


extern "C"
void MSG_PrepareToIncUIDL(MSG_Pane* messagepane, URL_Struct* url,
						  const char* uidl)
{
  CastMessagePane(messagepane)->PrepareToIncUIDL(url, uidl);
}


extern "C"
char* MSG_GeneratePartialMessageBlurb (MSG_Pane* messagepane,
									   URL_Struct *url, void *closure,
									   MimeHeaders *headers HG73530) {
  return CastMessagePane(messagepane)->GeneratePartialMessageBlurb(url,
																   closure,
																   headers HG73530);
}

// the following two functions are used by mkmailbx.c to process
// background message copying.

extern "C"
int MSG_BeginCopyingMessages(MWContext *context)
{
	return context->msgCopyInfo->srcFolder->BeginCopyingMessages(
				context->msgCopyInfo->dstFolder, 
				context->msgCopyInfo->srcDB,
				context->msgCopyInfo->srcArray,
				NULL, /* do not launch in url queue */
				context->msgCopyInfo->srcCount,
				context->msgCopyInfo);
}

extern "C"
int MSG_FinishCopyingMessages(MWContext *context)
{
	return context->msgCopyInfo->srcFolder->FinishCopyingMessages(
				context,
				context->msgCopyInfo->srcFolder, 
				context->msgCopyInfo->dstFolder, 
				context->msgCopyInfo->srcDB, 
				context->msgCopyInfo->srcArray, 
				context->msgCopyInfo->srcCount,
				&context->msgCopyInfo->moveState);
}

extern "C"
void MSG_MessageCopyIsCompleted(MessageCopyInfo **info)
{
	(*info)->srcFolder->CleanupCopyMessagesInto (info);
}

extern "C"
XP_Bool MSG_IsMessageCopyFinished(struct MessageCopyInfo *info)
{
	// if we have no info, probably better to say copy is finished.
	// This can happen if we're interrupted, it seems.
	return (info) ? info->moveState.moveCompleted : TRUE; 
}



extern "C"
int MSG_BeginOpenFolderSock(MSG_Pane* pane,
							const char *folder_name,
							const char *message_id, int32 msgnum,
							void **folder_ptr)
{
  return pane->BeginOpenFolderSock(folder_name, message_id, msgnum,
								   folder_ptr);
}

extern "C"
int MSG_FinishOpenFolderSock(MSG_Pane* pane,
							 const char *folder_name,
							 const char *message_id, int32 msgnum,
							 void **folder_ptr)
{
  return pane->FinishOpenFolderSock(folder_name, message_id, msgnum,
									folder_ptr);
}

extern "C"
void MSG_CloseFolderSock(MSG_Pane* pane, const char *folder_name,
						 const char *message_id, int32 msgnum,
						 void *folder_ptr)
{
  pane->CloseFolderSock(folder_name, message_id, msgnum, folder_ptr);
}

extern "C"
int MSG_OpenMessageSock(MSG_Pane* pane, const char *folder_name,
						const char *msg_id, int32 msgnum,
						void *folder_ptr, void **message_ptr,
						int32 *content_length)
{
  return pane->OpenMessageSock(folder_name, msg_id,
							   msgnum, folder_ptr,
							   message_ptr,
							   content_length);
}

extern "C"
int MSG_ReadMessageSock(MSG_Pane* pane, const char *folder_name,
						void *message_ptr, const char *message_id,
						int32 msgnum, char *buffer, int32 buffer_size)
{
  return pane->ReadMessageSock(folder_name,
							   message_ptr, message_id,
							   msgnum, buffer,
							   buffer_size);
}

extern "C"
void MSG_CloseMessageSock(MSG_Pane* pane,
						  const char *folder_name,
						  const char *message_id, int32 msgnum,
						  void *message_ptr)
{
  pane->CloseMessageSock(folder_name, message_id,
						 msgnum, message_ptr);
}


extern "C"
int MSG_BeginCompressFolder(MSG_Pane* pane, URL_Struct* url,
							const char* foldername, void** closure)
{
  return pane->BeginCompressFolder(url, foldername, closure);
}

extern "C"
int MSG_FinishCompressFolder(MSG_Pane* pane, URL_Struct* url,
							 const char* foldername, void* closure)
{
  return pane->FinishCompressFolder(url, foldername, closure);
}

extern "C"
int MSG_CloseCompressFolderSock(MSG_Pane* pane, URL_Struct* url,
								void* closure)
{
  return pane->CloseCompressFolderSock(url, closure);
}


extern "C"
int MSG_BeginDeliverQueued(MSG_Pane* pane, URL_Struct* url,
						   void** closure)
{
  return pane->BeginDeliverQueued(url, closure);
}

extern "C"
int MSG_FinishDeliverQueued(MSG_Pane* pane, URL_Struct* url,
							void* closure)
{
  return pane->FinishDeliverQueued(url, closure);
}

extern "C"
int MSG_CloseDeliverQueuedSock(MSG_Pane* pane, URL_Struct* url,
							   void* closure)
{
  return pane->CloseDeliverQueuedSock(url, closure);
}



// Navigation routines

extern "C"
int MSG_ViewNavigate(MSG_Pane* pane, MSG_MotionType motion,
						MSG_ViewIndex startIndex, 
						MessageKey *resultKey, MSG_ViewIndex *resultIndex, 
						MSG_ViewIndex *pThreadIndex,
						MSG_FolderInfo **folderInfo)
{
	  if (folderInfo)
		  *folderInfo = NULL;
	  return pane->ViewNavigate(motion, startIndex,
		  resultKey, resultIndex, pThreadIndex, folderInfo);
}

extern "C"
int MSG_DataNavigate(MSG_Pane* threadpane, MSG_MotionType motion,
						MessageKey startKey, MessageKey *resultKey, 
						MessageKey *resultThreadKey)
{
	  return CastThreadPane(threadpane)->DataNavigate(motion, startKey,
		  resultKey, resultThreadKey);
}

extern "C" int MSG_NavigateStatus(MSG_Pane* pane,
								 MSG_MotionType command,
								 MSG_ViewIndex index,
								 XP_Bool *selectable_p,
								 const char **display_string)
 {
  return ConvertMsgErrToMKErr(pane->GetNavigateStatus(command,
													 index,
													 selectable_p,
													 display_string));
}

extern "C" int MSG_MarkReadByDate (MSG_Pane* pane, time_t startDate, time_t endDate)
{
	return ConvertMsgErrToMKErr(pane->MarkReadByDate(startDate, endDate));
}

extern "C" void MSG_RecordImapMessageFlags(MSG_Pane* pane, 
									   		MessageKey msgKey, 
									   		imapMessageFlagsType flags)
{
	if (pane)	// there is no pane during a biff
	{
		MessageDBView *dbView = pane->GetMsgView();
		if (dbView)
		{
			MessageDB *db = dbView->GetDB();
			if (db)
			{
				db->MarkRead(   msgKey, (flags & kImapMsgSeenFlag) != 0);
				db->MarkReplied(msgKey, (flags & kImapMsgAnsweredFlag) != 0);
				db->MarkMarked( msgKey, (flags & kImapMsgFlaggedFlag) != 0);
			}
		}
	}
}

extern "C" void MSG_ImapMsgsDeleted(MSG_Pane *urlPane,
								const char *onlineMailboxName,
								const char *hostName,
								XP_Bool deleteAllMsgs, 
                    			const char *doomedKeyString)
{
	MSG_IMAPFolderInfoMail *affectedFolder = urlPane->GetMaster()->FindImapMailFolder(hostName, onlineMailboxName, NULL, FALSE);
	if (affectedFolder)
		affectedFolder->MessagesWereDeleted(urlPane, deleteAllMsgs, doomedKeyString);
}                    				
                    				
/* notify libmsg that inbox filtering is complete */
extern "C" void MSG_ImapInboxFilteringComplete(MSG_Pane * /*urlPane*/)
{
}
 
/* notify libmsg that the online/offline synch is complete */              				
extern "C" void MSG_ImapOnOffLineSynchComplete(MSG_Pane * /*urlPane*/)
{
}

extern "C" void MSG_GetNextURL(MSG_Pane *pane)
{
	if (pane && pane->GetURLChain())
		pane->GetURLChain()->GetNextURL();
}

/* notify libmsg that an imap folder load was interrupted */
extern "C" void MSG_InterruptImapFolderLoad(MSG_Pane *urlPane, const char *hostName, const char *onlineFolderPath)
{
	if (urlPane && onlineFolderPath)
	{
		MSG_IMAPFolderInfoMail *interruptBox = urlPane->GetMaster()->FindImapMailFolder(hostName, onlineFolderPath, NULL, FALSE);
		if (interruptBox)
			interruptBox->NotifyFolderLoaded(urlPane, TRUE);	// TRUE, load was interrupted
	}
}

extern "C" MSG_FolderInfo* MSG_FindImapFolder(MSG_Pane *urlPane, const char *hostName, const char *onlineFolderPath)
{
	MSG_FolderInfo *returnBox = NULL;
	if (urlPane && onlineFolderPath && hostName)
		returnBox = urlPane->GetMaster()->FindImapMailFolder(hostName, onlineFolderPath, NULL, FALSE);
	return returnBox;
}

extern "C"	MSG_FolderInfo *MSG_SetFolderRunningIMAPUrl(MSG_Pane *urlPane, const char *hostName, const char *onlineFolderPath, MSG_RunningState running)
{
	MSG_IMAPFolderInfoMail *imapFolder = NULL;
	if (urlPane && onlineFolderPath && MSG_Pane::PaneInMasterList(urlPane))
	{
		imapFolder = urlPane->GetMaster()->FindImapMailFolder(hostName, onlineFolderPath, NULL, FALSE);
		if (imapFolder)
		{
			// check to see that we're not already running an imap url in this folder.
#ifdef DEBUG_bienvenu
			XP_ASSERT(running != MSG_RunningOnline || imapFolder->GetRunningIMAPUrl() != MSG_RunningOnline);
#endif
			imapFolder->SetRunningIMAPUrl(running);
		}
	}
	return imapFolder;
}

extern "C" void MSG_IMAPUrlFinished(MSG_FolderInfo *folder, URL_Struct *URL_s)
{
	if (!folder)
		return;
	MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
	if (imapFolder)
	{
		// really want to check if there are offline imap events, and find a url queue,
		// and add them.
		if (imapFolder->GetHasOfflineEvents())
		{
			// find a url queue for this pane.
			MSG_UrlQueue *queue = MSG_UrlQueue::FindQueue (URL_s->msg_pane);
#ifdef DEBUG_bienvenu
			XP_ASSERT(queue);
#endif
			if (queue)	// send some sort of url so we can chain correctly - this may be the wrong one to send.
			{
				// I think this causes an allconnectionscomplete to get called because there's no bg object, so
				// the url returns right a way. This is probably a bad thing.
				queue->AddUrl("mailbox:?null", MSG_IMAPFolderInfoMail::URLFinished);
			}
		}
	}
	else
	{
		XP_ASSERT(FALSE);
	}
}


extern "C" void MSG_StoreNavigatorIMAPConnectionInMoveState(MWContext *context, 
													    TNavigatorImapConnection *connection)
{
	if (context->msgCopyInfo)
	{
		context->msgCopyInfo->moveState.imap_connection = connection;
		if (connection)
		{
			IMAP_UploadAppendMessageSize(connection, context->msgCopyInfo->moveState.msgSize,context->msgCopyInfo->moveState.msg_flags); 
			context->msgCopyInfo->moveState.haveUploadedMessageSize = TRUE;
		}
	}
}

/* If there is a cached connection, uncache it and return it */
extern "C" TNavigatorImapConnection* MSG_UnCacheImapConnection(MSG_Master* master, const char *host, const char *folderName)
{
	return master->UnCacheImapConnection(host, folderName);
}

/* Cache this connection and return TRUE if there is not one there already, else false */
extern "C" XP_Bool MSG_TryToCacheImapConnection(MSG_Master* master, const char *host, const char *folderName, 
												TNavigatorImapConnection *connection)
{
	return master->TryToCacheImapConnection(connection, host, folderName);
}


// The following are the MSG_ routines called by libnet in response to
// a news url. They turn around and delegate the commands to 
// a ListNewsGroupState object owned by the folder pane. This is a step
// towards having an nntp state object which is a subclass of some
// generic asynchronous state object to be designed later.
// One thing that bothers me about using a pane for this is that potentially
// some of this stuff could happen without a pane being up. The asynchronous
// state object architecture should address this.
extern "C" int
MSG_GetRangeOfArtsToDownload(MSG_Pane* pane, void** data, MSG_NewsHost* host,
							 const char* group_name,
							 int32 first_possible,
							 int32 last_possible,
							 int32 maxextra,
							 int32* first,
							 int32* last)
{
	ListNewsGroupState *listState = pane->GetMaster()->GetListNewsGroupState(host, group_name);
	if (data)
		*data = listState;
	if (pane != NULL && listState != NULL)
	{
		return listState->GetRangeOfArtsToDownload(
					host, group_name, first_possible, 
					last_possible, maxextra, first, last);
	}
	else
	{
//		XP_ASSERT(FALSE);
		return 0;
	}
}

int 
MSG_AddToKnownArticles(MSG_Pane* pane, MSG_NewsHost* host,
					   const char* group_name, int32 first, int32 last)
{
	ListNewsGroupState *listState =
		pane->GetMaster()->GetListNewsGroupState(host, group_name);
	if (pane != NULL && listState != NULL)
	{
		return listState->AddToKnownArticles(host, group_name, first, last);
	}
	else
	{
//		XP_ASSERT(FALSE);
		return 0;
	}
}

extern "C" int MSG_InitAddArticleKeyToGroup(MSG_Pane *pane, MSG_NewsHost* host,
									 const char* groupName,  void **parseState)
{
	*parseState = new ListNewsGroupArticleKeysState(host, groupName, pane);
	return 0;
}

extern "C" int	MSG_AddArticleKeyToGroup(void *parseState, int32 key)
{
	ListNewsGroupArticleKeysState *state = (ListNewsGroupArticleKeysState *) parseState;
	if (state)
		state->AddArticleKey(key);
	return 0;
}

extern "C" int	MSG_FinishAddArticleKeyToGroup(MSG_Pane* /*pane*/, void **parseState)
{
	ListNewsGroupArticleKeysState *state = (ListNewsGroupArticleKeysState *) *parseState;
	delete state;
	*parseState = NULL;
	return 0;
}

extern "C" int MSG_SynchronizeNewsFolder(MSG_Pane *pane, MSG_FolderInfo *folder)
{
	char *url = folder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
	URL_Struct *url_struct;
	char *syncUrl = PR_smprintf("%s/?list-ids",
								   url);
	url_struct = NET_CreateURLStruct (syncUrl, NET_NORMAL_RELOAD);
	XP_FREE (syncUrl);
	if (!url_struct)
		return MK_OUT_OF_MEMORY;
	pane->GetURL(url_struct, FALSE);
	XP_FREE(url);
	return 0;
}
	

extern "C" int
MSG_InitXOVER (MSG_Pane* pane,
			   MSG_NewsHost* host,
			   const char *group_name,
			   uint32 first_msg, uint32 last_msg,
			   uint32 oldest_msg, uint32 youngest_msg,
			   void ** parse_data)
{
	ListNewsGroupState *listState =
		pane->GetMaster()->GetListNewsGroupState(host, group_name);

	*parse_data = listState;

	if (pane != NULL && listState != NULL)
	{
		return listState->InitXOVER(host, group_name, first_msg, last_msg,
									oldest_msg, youngest_msg);
	}
	else
	{
		XP_ASSERT(FALSE);
		return 0;
	}
}

extern "C" int 
MSG_ProcessXOVER (MSG_Pane* pane, char *line, void ** data )
{
	if (data)
	{
		ListNewsGroupState *listState = (ListNewsGroupState *) *data;
		if (pane != NULL && listState != NULL)
			return listState->ProcessXOVER(line);

	}
	XP_ASSERT(FALSE);
	return 0;
}

extern "C" int 
MSG_ResetXOVER (MSG_Pane* /*pane*/, void **  data)
{
	if (data && *data)
	{
		ListNewsGroupState *listState = (ListNewsGroupState *) *data;
		return listState->ResetXOVER();
	}
	else
	{
		// we don't assert, because this can happen when we re-validate against a news server
		return 0;
	}
}


extern "C" int 
MSG_ProcessNonXOVER (MSG_Pane* pane, char *line, void ** data)
{
	if (data)
	{
		ListNewsGroupState *listState = (ListNewsGroupState *) *data;
		if (pane != NULL && listState != NULL)
			return listState->ProcessNonXOVER(line);
	}
	XP_ASSERT(FALSE);
	return 0;
}

extern "C" int 
MSG_FinishXOVER (MSG_Pane* pane, void **data, int status)
{
	// first check if pane has been deleted out from under us.
	if (!MSG_Pane::PaneInMasterList(pane))
		return 0;
	MSG_Master		*master = pane->GetMaster(); 
	ListNewsGroupState *listState = NULL;
	MSG_FolderInfo *folder = pane->GetFolder();
	MSG_FolderInfoNews *newsFolder = (folder) ? folder->GetNewsFolderInfo() : 0;
	if (data && *data)
		listState = (ListNewsGroupState *) *data;
	else
	{
		if (newsFolder)
			listState = newsFolder->GetListNewsGroupState();
	}
	if (pane!= NULL && listState != NULL)
	{
		*data = NULL;
		status = listState->FinishXOVER(status);
		// now that we can get the folderinfo from the pane, can avoid lookup in master
		if (newsFolder && newsFolder->GetListNewsGroupState() == listState)
		{
			newsFolder->SetListNewsGroupState(NULL);
			delete listState;
		}
		else
			master->ClearListNewsGroupState(listState->GetHost(),
											listState->GetGroupName());
		return status;
	}
//	XP_ASSERT(FALSE);
	return 0;
}

extern "C" XP_Bool MSG_IsOfflineArticle (MSG_Pane *pane, 
										 const char *message_id,
										const char **groupName,
										uint32 *message_number_return)
{
	if (!message_id) return FALSE;

	if (!pane->GetFolder() || !pane->GetFolder()->IsNews())
		return FALSE;

	MessageDBView *view = pane->GetMsgView();
	if (!view)
		return FALSE;
	NewsGroupDB *newsDB = view->GetDB()->GetNewsDB();
	MessageKey	msgKey = newsDB->GetMessageKeyForID(message_id);
	if (msgKey != MSG_MESSAGEKEYNONE)
	{
		MSG_FolderInfo *folderInfo = pane->GetFolder();
		if (folderInfo != NULL && folderInfo->IsNews())
		{
			MSG_FolderInfoNews *newsFolderInfo = 
				(MSG_FolderInfoNews *) folderInfo;
			*groupName = newsFolderInfo->GetNewsgroupName();
			*message_number_return = msgKey;
		}
		return newsDB->IsArticleOffline(msgKey);
	}
	else
		return FALSE;
}

extern "C" int MSG_StartOfflineRetrieval(MSG_Pane *pane,
										const char *group,
										uint32 messageNumber,
										void **offlineState)
{
	*offlineState = (void *) new MSG_OfflineNewsArtState(pane, group, messageNumber);
	return 0;
}

extern "C" int MSG_ProcessOfflineNews(void *offlineState, char *outputBuffer, int outputBufSize)
{

	MSG_OfflineNewsArtState *offlineNewsArtState = (MSG_OfflineNewsArtState *) offlineState;
	int status= offlineNewsArtState->Process(outputBuffer, outputBufSize);
	if (status <= 0)
		delete offlineNewsArtState;
	return status;
}

extern "C" int MSG_InterruptOfflineNews(void *offlineState)
{
	
	MSG_OfflineNewsArtState *offlineNewsArtState = (MSG_OfflineNewsArtState *) offlineState;
	offlineNewsArtState->Interrupt();
	
	return 0;
}

/* OFFLINE IMAP OPERATIONS and settings 
   ====================================
*/
extern "C" uint32 MSG_GetImapMessageFlags(MSG_Pane *urlPane, 
											const char *hostName,
											const char *onlineBoxName, 
											uint32 key)
{
	uint32 flags = 0;
	if (urlPane && urlPane->GetMaster())
	{
		MSG_IMAPFolderInfoMail *imapFolder = urlPane->GetMaster()->FindImapMailFolder(hostName, onlineBoxName, NULL, FALSE);
		if (imapFolder)
		{
			XP_Bool wasCreated=FALSE;
			MailDB *imapDB = NULL;
			ImapMailDB::Open(imapFolder->GetPathname(), TRUE, &imapDB, imapFolder->GetMaster(), &wasCreated);
			if (imapDB)
			{
				MailMessageHdr *mailhdr = imapDB->GetMailHdrForKey(key);
				if (mailhdr)
				{
					flags = mailhdr->GetFlags();
					delete mailhdr;
				}
				imapDB->Close();
			}
		}
	}
	return flags;
}

extern "C" void MSG_StartOfflineImapRetrieval(MSG_Pane *urlPane, 
											const char *hostName,
											const char *onlineBoxName, 
											uint32 key,
											void **offLineRetrievalData)
{
	if (urlPane && urlPane->GetMaster())
	{
		MSG_IMAPFolderInfoMail *imapFolder = urlPane->GetMaster()->FindImapMailFolder(hostName, onlineBoxName, NULL, FALSE);
		if (imapFolder)
			*offLineRetrievalData = new DisplayOfflineImapState(imapFolder, key);
	}
}

											
extern "C" uint32 MSG_GetOfflineMessageSize(void *offLineRetrievalData)
{
	return ( (DisplayOfflineImapState *) offLineRetrievalData)->GetMsgSize();
}

extern "C" int MSG_ProcessOfflineImap(void *offLineRetrievalData, char *socketBuffer, uint32 read_size)
{
	DisplayOfflineImapState *state = (DisplayOfflineImapState *) offLineRetrievalData;
	int status = state->ProcessDisplay(socketBuffer, read_size);
	if (status == 0)
		delete state;
	return status;
}

extern "C" int MSG_InterruptOfflineImap(void *offLineRetrievalData)
{
	DisplayOfflineImapState *state = (DisplayOfflineImapState *) offLineRetrievalData;
	delete state;
	return MK_INTERRUPTED;
}


// This whole routine should go away and be replaced with a mechanism whereby
// we create a url which asks for an article number in a particular newsgroup.
// It makes at least one potentially bad assumption:
//		1. The groupName returned from MSG_FolderInfoNews *can be returned
// It also doesn't try to find a news pane like the old routine did.
// Its implementation shouldn't be in msgglue, but where should it be?
// In thread pane? message pane? Master?
extern "C" void MSG_NewsGroupAndNumberOfID (MSG_Pane *pane,
										const char *message_id,
										const char **groupName,
										uint32 *message_number_return)
{
	*groupName = 0;
	*message_number_return = 0;
	if (!message_id) 
		return;

	if (!pane->GetFolder() || !pane->GetFolder()->IsNews())
		return;

	MessageDBView *view = pane->GetMsgView();
	if (!view || !view->GetDB())
		return;
	*message_number_return = view->GetDB()->GetMessageKeyForID(message_id);
	MSG_FolderInfo *folderInfo = pane->GetFolder();
	if (folderInfo != NULL && folderInfo->IsNews())
	{
		MSG_FolderInfoNews *newsFolderInfo = 
			(MSG_FolderInfoNews *) folderInfo;
		*groupName = newsFolderInfo->GetNewsgroupName();
	}

}


extern "C" int32
MSG_GetNewsRCCount(MSG_Pane* pane, MSG_NewsHost* host)
{
	return pane->GetNewsRCCount(host);
}


extern "C" char*
MSG_GetNewsRCGroup(MSG_Pane* pane, MSG_NewsHost* host)
{
	return pane->GetNewsRCGroup(host);
}


extern "C" int
MSG_DisplaySubscribedGroup(MSG_Pane* pane,
						   MSG_NewsHost* host,
						   const char *group,
						   int32 oldest_message,
						   int32 youngest_message,
						   int32 total_messages,
						   XP_Bool nowvisiting)
{
	XP_ASSERT(pane);
	if (!pane) return -1;
	return pane->DisplaySubscribedGroup(host,
										group,
										oldest_message,
										youngest_message,
										total_messages,
										nowvisiting);
}

extern "C" int 
MSG_GroupNotFound(MSG_Pane* pane, MSG_NewsHost* host, const char *group, XP_Bool opening)
{
	XP_ASSERT(pane);
	if (!pane) return -1;
	pane->GroupNotFound(host, group, opening);
	return 0;
}



extern "C" MSG_Pane*
MSG_CreateSubscribePane(MWContext* context,
						MSG_Master* master)
{
	return MSG_SubscribePane::Create(context, master);
}


#ifdef SUBSCRIBE_USE_OLD_API
extern "C" MSG_Pane*
MSG_CreateSubscribePaneOnHost(MWContext* context,
							  MSG_Master* master,
							  MSG_NewsHost* host)
{
	return MSG_SubscribePane::Create(context, master, host);
}
#endif /* SUBSCRIBE_USE_OLD_API */


extern "C" MSG_Pane*
MSG_CreateSubscribePaneForHost(MWContext* context,
							  MSG_Master* master,
							  MSG_Host* host)
{
	return MSG_SubscribePane::Create(context, master, host);
}


extern "C" int32
MSG_GetNewsHosts(MSG_Master* master, MSG_NewsHost** result, int32 resultsize)
{
	XP_ASSERT(master);
	if (!master) return -1;
	return master->GetHostTable()->GetHostList(result, resultsize);
}

extern "C" int32
MSG_GetIMAPHosts(MSG_Master* master, MSG_IMAPHost** result, int32 resultsize)
{
	XP_ASSERT(master);
	if (!master) return -1;
	MSG_IMAPHostTable *imapTable = master->GetIMAPHostTable();
	if (imapTable)
		return imapTable->GetHostList(result, resultsize);
	else
		return 0;
}

extern "C" int32
MSG_GetSubscribingIMAPHosts(MSG_Master* master, MSG_IMAPHost** result, int32 resultsize)
{
	XP_ASSERT(master);
	if (!master) return -1;
	return master->GetSubscribingIMAPHosts(result, resultsize);
}

extern "C" int32
MSG_GetSubscribingHosts(MSG_Master* master, MSG_Host** result, int32 resultsize)
{
	XP_ASSERT(master);
	if (!master) return -1;
	return master->GetSubscribingHosts(result, resultsize);
}

extern "C" XP_Bool
MSG_IsNewsHost(MSG_Host* host)
{
	return host->IsNews();
}

extern "C" XP_Bool
MSG_IsIMAPHost(MSG_Host* host)
{
	return host->IsIMAP();
}

/* Returns a pointer to the associated MSG_Host object for this MSG_NewsHost */
extern "C" MSG_Host*
MSG_GetMSGHostFromNewsHost(MSG_NewsHost *newsHost)
{
	return newsHost;
}

/* Returns a pointer to the associated MSG_Host object for this MSG_IMAPHost */
extern "C" MSG_Host*
MSG_GetMSGHostFromIMAPHost(MSG_IMAPHost *imapHost)
{
	return imapHost;
}

/* Returns a pointer to the associated MSG_NewsHost object for this MSG_Host,
   if it is a MSG_NewsHost.  Otherwise, returns NULL. */
extern MSG_NewsHost*
MSG_GetNewsHostFromMSGHost(MSG_Host *host)
{
	return host->GetNewsHost();
}

/* Returns a pointer to the associated MSG_IMAPHost object for this MSG_Host,
   if it is a MSG_IMAPHost.  Otherwise, returns NULL. */
extern MSG_IMAPHost*
MSG_GetIMAPHostFromMSGHost(MSG_Host *host)
{
	return host->GetIMAPHost();
}


extern "C" MSG_NewsHost*
MSG_CreateNewsHost(MSG_Master* master, const char* hostname,
				   XP_Bool isxxx, int32 port)
{
	XP_ASSERT(master);
	if (!master) return NULL;
	return master->AddNewsHost(hostname, isxxx, port);
}

extern "C" MSG_NewsHost*
MSG_GetDefaultNewsHost(MSG_Master* master)
{
	XP_ASSERT(master);
	if (!master) return NULL;
	return master->GetDefaultNewsHost();
}

extern "C" int
MSG_DeleteNewsHost(MSG_Master* master, MSG_NewsHost* host)
{
	// remove the host
	int status = host->RemoveHost();
	
	// refresh each folder pane
	/*
	// We don't have to do this, since it is broadcast in RemoveHost().
	MSG_FolderPane *eachPane;
	for (eachPane = (MSG_FolderPane *) master->FindFirstPaneOfType(MSG_FOLDERPANE); eachPane; 
			eachPane =  (MSG_FolderPane *) master->FindNextPaneOfType(eachPane->GetNextPane(), MSG_FOLDERPANE))
	{
	    eachPane->RedisplayAll();
	}
	*/
	return status;
}

#ifdef SUBSCRIBE_USE_OLD_API
extern "C" const char*
MSG_GetNewsHostUIName(MSG_NewsHost* host)
{
	return host->getFullUIName();
}
#endif /* SUBSCRIBE_USE_OLD_API */

extern "C" const char*
MSG_GetHostUIName(MSG_Host* host)
{
	return host->getFullUIName();
}

#ifdef SUBSCRIBE_USE_OLD_API
extern "C" const char*
MSG_GetNewsHostName(MSG_NewsHost* host)
{
	return host->getStr();
}
#endif /* SUBSCRIBE_USE_OLD_API */

extern "C" const char*
MSG_GetHostName(MSG_Host* host)
{
	MSG_IMAPHost *imapHost = host->GetIMAPHost();
	if (imapHost)
	{
		return imapHost->GetHostName();
	}
	MSG_NewsHost *newsHost = host->GetNewsHost();
	if (newsHost)
	{
		return newsHost->getStr();
	}
	return NULL;
}

HG91476

#ifdef SUBSCRIBE_USE_OLD_API
extern "C" int32
MSG_GetNewsHostPort(MSG_NewsHost* host)
{
	return host->getPort();
}
#endif /* SUBSCRIBE_USE_OLD_API */

extern "C" int32
MSG_GetHostPort(MSG_Host* host)
{
	return host->getPort();
}

extern void MSG_SupportsNewsExtensions (MSG_NewsHost *host, XP_Bool supports)
{
	XP_ASSERT(host);
	if (host)
		host->SetSupportsExtensions(supports);
}

extern "C" void MSG_AddNewsExtension (MSG_NewsHost *host, const char *ext)
{
	XP_ASSERT(host);
	if (host)
		host->AddExtension(ext);
}

extern "C" XP_Bool MSG_QueryNewsExtension (MSG_NewsHost *host, const char *ext)
{
	XP_ASSERT(host);
	if (host)
		return host->QueryExtension (ext);
	return FALSE;
}

extern "C" XP_Bool MSG_NeedsNewsExtension (MSG_NewsHost *host, const char *ext)
{
	XP_ASSERT(host);
	if (host)
		return host->NeedsExtension (ext);
	return FALSE;
}

extern "C" void MSG_AddSearchableGroup (MSG_NewsHost *host, const char *group)
{
	XP_ASSERT(host);
	if (host)
		host->AddSearchableGroup (group);
}

extern "C" void MSG_AddSearchableHeader (MSG_NewsHost *host, const char *header)
{
	XP_ASSERT(host);
	if (host)
		host->AddSearchableHeader(header);
}

extern "C" void MSG_AddPropertyForGet (MSG_NewsHost *host, const char *property,
									  const char *value)
{
	XP_ASSERT(host);
	if (host)
		host->AddPropertyForGet (property, value);
}

extern "C" void MSG_SetNewsHostPostingAllowed (MSG_NewsHost *host, XP_Bool allowed)
{
	XP_ASSERT(host);
	if (host)
		host->SetPostingAllowed(allowed);
}

extern "C" XP_Bool MSG_GetNewsHostPushAuth (MSG_NewsHost *host)
{
	XP_ASSERT(host);
	if (host)
		return host->GetPushAuth();
	return FALSE;
}

extern "C" void MSG_SetNewsHostPushAuth (MSG_NewsHost *host, XP_Bool pushAuth)
{
	XP_ASSERT(host);
	if (host)
		host->SetPushAuth(pushAuth);
}

extern "C" MSG_IMAPHost* MSG_CreateIMAPHost(MSG_Master* master,
										const char* hostname,
										XP_Bool isxxx,
										const char *userName,
										XP_Bool checkNewMail,
										int	biffInterval,
										XP_Bool rememberPassword,
										XP_Bool usingSubscription,
										XP_Bool overrideNamespaces,
										const char *personalOnlineDir,
										const char *publicOnlineDir,
										const char *otherUsersOnlineDir
										HG72523)
{
	XP_ASSERT(master);
	if (!master) return NULL;
	MSG_FolderArray *subFolders = master->GetFolderTree()->GetSubFolders();

	MSG_IMAPHost *host = master->AddIMAPHost(hostname, isxxx, userName, checkNewMail, biffInterval,rememberPassword, usingSubscription,
		overrideNamespaces, personalOnlineDir, publicOnlineDir, otherUsersOnlineDir, TRUE);

	MSG_IMAPFolderInfoMail::BuildIMAPServerTree(master, hostname, subFolders, FALSE, 1);

	HG09275
	if (host)
	{
		MSG_Pane* pane;
		for (pane = master->GetFirstPane() ; pane ; pane = pane->GetNextPane()) {
			if (pane->GetPaneType() == MSG_FOLDERPANE) {
				((MSG_FolderPane*) pane)->RedisplayAll();
			}
		}
	}
	return host;

}


/* Gets the default imap host.  Could be NULL, if the user has managed to
   configure himself that way. */
extern "C" MSG_IMAPHost* MSG_GetDefaultIMAPHost(MSG_Master* master)
{
	// don't know if we'll need this createIfMissing flag.
	MSG_IMAPHostTable *hostTable = master->GetIMAPHostTable();
	if (hostTable)
	{
		return hostTable->GetDefaultHost(FALSE);
	}
	else
		return NULL;
}

/* Deletes an imap host.  This deletes everything known about this hosts -- the
   preferences, the databases, everything.  The user had better have confirmed
   this operation before making this call. */

extern "C" int MSG_DeleteIMAPHost(MSG_Master* master, MSG_IMAPHost* host)
{
	MSG_IMAPHostTable *imapHostTable = master->GetIMAPHostTable();
	if (imapHostTable)
	{
		// remove the host
		return host->RemoveHost();
	}
	else
	{
		XP_ASSERT(FALSE);
		return -1;
	}
}

/* same as above, except takes in the host name */

extern "C" int MSG_DeleteIMAPHostByName(MSG_Master* master, const char *hostname)
{
	MSG_IMAPHostTable *imapHostTable = master->GetIMAPHostTable();
	MSG_IMAPHost *host = imapHostTable ? imapHostTable->FindIMAPHost(hostname) : 0;

	if (host)
	{
		return MSG_DeleteIMAPHost(master, host);
	}
	else
		return -1;	// host not found with given name
}

extern "C" MSG_IMAPHost *MSG_GetIMAPHostByName(MSG_Master *master, const char *hostName)
{
	MSG_IMAPHostTable *imapHostTable = master->GetIMAPHostTable();
	return imapHostTable ? imapHostTable->FindIMAPHost(hostName) : 0;
}

extern "C" void	MSG_ReorderIMAPHost(MSG_Master *master, MSG_IMAPHost *host, MSG_IMAPHost *afterHost /* NULL = pos 0 */)
{
	MSG_IMAPHostTable *imapHostTable = master->GetIMAPHostTable();
	if (imapHostTable)
	{
		master->BroadcastFolderDeleted(host->GetHostFolderInfo());
		imapHostTable->ReorderIMAPHost(host, afterHost);
		master->BroadcastFolderAdded(host->GetHostFolderInfo(), NULL);
	}
}

/* If we can find the IMAP host in our table, returns 0 and sets usingSubscription to TRUE
   if this host is configured for using subscription.  If we can't find the host, returns -1. */
extern "C" int MSG_GetIMAPHostIsUsingSubscription(MSG_Master *master, const char *hostName, XP_Bool *usingSubscription)
{
	MSG_IMAPHostTable *imapHostTable = master->GetIMAPHostTable();
	MSG_IMAPHost *host = imapHostTable ? imapHostTable->FindIMAPHost(hostName) : (MSG_IMAPHost *)NULL;
	if (host)
	{
		*usingSubscription = host->GetIsHostUsingSubscription();
		return (0);
	}
	else return (-1);
}

extern "C" XP_Bool MSG_GetIMAPHostDeleteIsMoveToTrash(MSG_Master *master, const char *hostName)
{
	MSG_IMAPHostTable *imapHostTable = master->GetIMAPHostTable();
	MSG_IMAPHost *host = imapHostTable ? imapHostTable->FindIMAPHost(hostName) : (MSG_IMAPHost *)NULL;
	return (host) ? host->GetDeleteIsMoveToTrash() : FALSE;
}

HG78271

extern "C" MSG_FolderInfo *MSG_GetTrashFolderForHost(MSG_IMAPHost *host)
{
	return host->GetTrashFolderForHost();
}

extern "C" int
MSG_SubscribeSetCallbacks(MSG_Pane* subscribepane,
						  MSG_SubscribeCallbacks* callbacks,
						  void* closure)
{
	return CastSubscribePane(subscribepane)->SetCallbacks(callbacks, closure);
}

HG24327

extern "C" int
MSG_SubscribeCancel(MSG_Pane* subscribepane)
{
	return CastSubscribePane(subscribepane)->Cancel();
}

extern "C" int
MSG_SubscribeCommit(MSG_Pane* subscribepane)
{
	return CastSubscribePane(subscribepane)->CommitSubscriptions();
}


#ifdef SUBSCRIBE_USE_OLD_API
extern "C" MSG_NewsHost*
MSG_SubscribeGetNewsHost(MSG_Pane* subscribepane)
{
	return (MSG_NewsHost *)(CastSubscribePane(subscribepane)->GetHost());
}
#endif /* SUBSCRIBE_USE_OLD_API */

extern "C" MSG_Host*
MSG_SubscribeGetHost(MSG_Pane* subscribepane)
{
	return CastSubscribePane(subscribepane)->GetHost();
}

#ifdef SUBSCRIBE_USE_OLD_API
extern "C" int
MSG_SubscribeSetNewsHost(MSG_Pane* subscribepane, MSG_NewsHost* host)
{
	return CastSubscribePane(subscribepane)->SetHost(host);
}
#endif /* SUBSCRIBE_USE_OLD_API */

extern "C" int
MSG_SubscribeSetHost(MSG_Pane* subscribepane, MSG_Host* host)
{
	return CastSubscribePane(subscribepane)->SetHost(host);
}

extern "C" MSG_SubscribeMode
MSG_SubscribeGetMode(MSG_Pane* subscribepane)
{
	return CastSubscribePane(subscribepane)->GetMode();
}

extern "C" int
MSG_SubscribeSetMode(MSG_Pane* subscribepane, MSG_SubscribeMode mode)
{
	return CastSubscribePane(subscribepane)->SetMode(mode);
}

extern "C" MSG_ViewIndex
MSG_SubscribeFindFirst(MSG_Pane* subscribepane, const char* str)
{
	return CastSubscribePane(subscribepane)->FindFirst(str);
}

extern "C" int
MSG_SubscribeFindAll(MSG_Pane* subscribepane, const char* str)
{
	return CastSubscribePane(subscribepane)->FindAll(str);
}

extern "C" XP_Bool
MSG_GetGroupNameLineByIndex(MSG_Pane* subscribepane,
							MSG_ViewIndex firstline,
							int32 numlines,
							MSG_GroupNameLine* data)
{
	return CastSubscribePane(subscribepane)->GetGroupNameLineByIndex(firstline,
																	 numlines,
																	 data);
}

extern "C" int MSG_AddSubscribedNewsgroup (MSG_Pane *pane, const char *groupUrl)
{
	MSG_FolderInfoNews *group = pane->GetMaster()->AddNewsGroup(groupUrl);
	return group ? 0 : -1;
}


// Internal routines only -- I hope these go away soon.


extern "C" char*
msg_MagicFolderName(MSG_Prefs* prefs, uint32 flag, int *pStatus)
{
  return prefs->MagicFolderName(flag, pStatus);
}

extern "C" void MSG_SetFolderPrefFlags(MSG_FolderInfo *info, int32 flags)
{
	if (info)
		info->SetFolderPrefFlags(flags);
}

extern "C" int32 MSG_GetFolderPrefFlags(MSG_FolderInfo *info)
{
	if (!info)
		return 0;
	return info->GetFolderPrefFlags();
}

extern "C" void MSG_SetFolderCSID(MSG_FolderInfo *info, int16 csid)
{
	if (info)
		info->SetFolderCSID(csid);
}

extern "C" int16 MSG_GetFolderCSID(MSG_FolderInfo *info)
{
	return (info) ? info->GetFolderCSID() : 0;
}

extern "C" void MSG_SetLastMessageLoaded(MSG_FolderInfo *info, MessageKey lastMessageLoaded)
{
	if (info)
		info->SetLastMessageLoaded(lastMessageLoaded);
}

extern "C" MessageKey MSG_GetLastMessageLoaded(MSG_FolderInfo *info)
{
	return (info) ? info->GetLastMessageLoaded() : 0;
}


extern "C" int MSG_DownloadForOffline(MSG_Master *master, MSG_Pane *pane)
{
	return master->DownloadForOffline(pane);
}
extern "C" int MSG_DownloadFolderForOffline(MSG_Master *master, MSG_Pane *pane, MSG_FolderInfo *folder)
{
	return master->DownloadForOffline(pane, folder);
}

extern "C" int MSG_GoOffline(MSG_Master *master, MSG_Pane *pane, XP_Bool downloadDiscussions, XP_Bool getNewMail, XP_Bool sendOutbox, XP_Bool getDirectories)
{
	return master->SynchronizeOffline(pane, downloadDiscussions, getNewMail, sendOutbox, getDirectories, !NET_IsOffline());
}

extern "C" int MSG_SynchronizeOffline(MSG_Master *master, MSG_Pane *pane, XP_Bool downloadDiscussions, XP_Bool getNewMail, XP_Bool sendOutbox, XP_Bool getDirectories, XP_Bool goOffline)
{
	return master->SynchronizeOffline(pane, downloadDiscussions, getNewMail, sendOutbox, getDirectories, goOffline);
}

extern "C" int MSG_SetOfflineRetrievalInfo(
	MSG_FolderInfo *newsGroup, 
	XP_Bool		useDefaults,
	XP_Bool		byReadness,
	XP_Bool		unreadOnly,
	XP_Bool		byDate,
	int32		daysOld)
{
	NewsGroupDB *newsDB = newsGroup->GetNewsFolderInfo()->OpenNewsDB(TRUE, NULL);

	if (newsDB)
	{
		MSG_RetrieveArtInfo retrieveInfo;
		retrieveInfo.m_useDefaults = useDefaults;
		retrieveInfo.m_byReadness = byReadness;
		retrieveInfo.m_unreadOnly = unreadOnly;
		retrieveInfo.m_byDate = byDate;
		retrieveInfo.m_daysOld = daysOld;

		newsDB->SetOfflineRetrievalInfo(&retrieveInfo);
		newsDB->Close();
	}
	return 0;
}

extern "C" int MSG_SetArticlePurgingInfo(
	MSG_FolderInfo *newsGroup, 
	XP_Bool			useDefaults,
	MSG_PurgeByPreferences	purgeBy,
	int32			daysToKeep)
{
	NewsGroupDB *newsDB = (newsGroup->GetNewsFolderInfo()) ? newsGroup->GetNewsFolderInfo()->OpenNewsDB(TRUE, NULL) : 0;

	if (newsDB)
	{
		MSG_PurgeInfo	purgeInfo;
		purgeInfo.m_useDefaults = useDefaults;
		purgeInfo.m_daysToKeep = daysToKeep;
		purgeInfo.m_purgeBy = purgeBy;
		newsDB->SetPurgeArticleInfo(&purgeInfo);
		newsDB->Close();
	}
	return 0;
}

extern "C" int MSG_SetHeaderPurgingInfo(
	MSG_FolderInfo *newsGroup, 
	XP_Bool	useDefaults,
	MSG_PurgeByPreferences	purgeBy,
	XP_Bool	unreadOnly,
	int32 daysToKeep,
	int32 numHeadersToKeep)
{
	NewsGroupDB *newsDB = newsGroup->IsNews() ? newsGroup->GetNewsFolderInfo()->OpenNewsDB(TRUE, NULL) : 0;

	if (newsDB)
	{
		MSG_PurgeInfo	purgeInfo;
		purgeInfo.m_useDefaults = useDefaults;
		purgeInfo.m_purgeBy = purgeBy;
		purgeInfo.m_unreadOnly = unreadOnly;
		purgeInfo.m_daysToKeep = daysToKeep;
		purgeInfo.m_numHeadersToKeep = numHeadersToKeep;
		newsDB->SetPurgeHeaderInfo(&purgeInfo);
		newsDB->Close();
	}
	return 0;
}

extern "C" void MSG_GetOfflineRetrievalInfo(MSG_FolderInfo *newsGroup,
	XP_Bool		*pUseDefaults,
	XP_Bool		*pByReadness,
	XP_Bool		*pUnreadOnly,
	XP_Bool		*pByDate,
	int32		*pDaysOld)
{
	NewsGroupDB *newsDB = newsGroup->IsNews() ? newsGroup->GetNewsFolderInfo()->OpenNewsDB(TRUE, NULL) : 0;

	if (newsDB)
	{
		MSG_RetrieveArtInfo retrieveInfo;
		newsDB->GetOfflineRetrievalInfo(&retrieveInfo);
		*pUseDefaults = retrieveInfo.m_useDefaults;
		if (retrieveInfo.m_useDefaults)
		{
			PREF_GetBoolPref("offline.news.download.unread_only",	pUnreadOnly);
			PREF_GetBoolPref("offline.news.download.by_date",		pByDate);
			PREF_GetIntPref("offline.news.download.days",			pDaysOld);	// days
		}
		else
		{
			*pByReadness = retrieveInfo.m_byReadness;
			*pUnreadOnly = retrieveInfo.m_unreadOnly;
			*pByDate = retrieveInfo.m_byDate;
			*pDaysOld = retrieveInfo.m_daysOld;
		}
		newsDB->Close();
	}
}

extern "C" void MSG_GetArticlePurgingInfo(MSG_FolderInfo *newsGroup,
	XP_Bool			*pUseDefaults,
	MSG_PurgeByPreferences	*pPurgeBy,
	int32 *pDaysToKeep)
{
	NewsGroupDB *newsDB = newsGroup->IsNews() ? newsGroup->GetNewsFolderInfo()->OpenNewsDB() : 0;

	if (newsDB)
	{
		MSG_PurgeInfo purgeInfo;
		newsDB->GetPurgeArticleInfo(&purgeInfo);
		if (purgeInfo.m_useDefaults)
		{
			XP_Bool purgeRemoveBodies;
			PREF_GetBoolPref("news.remove_bodies.by_age", &purgeRemoveBodies);
			*pPurgeBy = (purgeRemoveBodies) ? MSG_PurgeByAge : MSG_PurgeNone;
			PREF_GetIntPref("news.remove_bodies.days",	pDaysToKeep);
		}
		else
		{
			*pPurgeBy = purgeInfo.m_purgeBy;
			*pDaysToKeep = purgeInfo.m_daysToKeep;
			*pUseDefaults = purgeInfo.m_useDefaults;
		}
		newsDB->Close();
	}
}

extern "C" void MSG_GetHeaderPurgingInfo(MSG_FolderInfo *newsGroup,
	XP_Bool *pUseDefaults,
	MSG_PurgeByPreferences	*pPurgeBy,
	XP_Bool	*pUnreadOnly,
	int32 *pDaysToKeep,
	int32 *pNumHeadersToKeep)
{
	NewsGroupDB *newsDB = NULL;
	
	if (newsGroup->GetNewsFolderInfo())
		newsDB = newsGroup->GetNewsFolderInfo()->OpenNewsDB();
	MSG_PurgeInfo	purgeInfo;

	if (newsDB)
	{
		newsDB->GetPurgeHeaderInfo(&purgeInfo);
		newsDB->Close();
	}
	else
	{
		if (newsGroup && newsGroup->GetMaster() && newsGroup->GetMaster()->GetPurgeHdrInfo())
			purgeInfo = *(newsGroup->GetMaster()->GetPurgeHdrInfo());
		else
			return;
	}
	*pUseDefaults = purgeInfo.m_useDefaults;
	if (purgeInfo.m_useDefaults)
	{
		int32 keepMethod;									
		PREF_GetIntPref("news.keep.method", &keepMethod);
		*pPurgeBy = (MSG_PurgeByPreferences) keepMethod;
		PREF_GetIntPref("news.keep.days", pDaysToKeep);
		PREF_GetIntPref("news.keep.count", pNumHeadersToKeep);
		PREF_GetBoolPref("news.keep.only_unread", pUnreadOnly);
	}
	else
	{
		*pPurgeBy = purgeInfo.m_purgeBy;
		*pUnreadOnly = purgeInfo.m_unreadOnly;
		*pDaysToKeep = purgeInfo.m_daysToKeep;
		*pNumHeadersToKeep = purgeInfo.m_numHeadersToKeep;
	}
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

extern "C"
XP_Bool MSG_CleanupNeeded(MSG_Master *master)
{
	return master->CleanupNeeded();
}

extern "C"
void MSG_CleanupFolders(MSG_Pane *pane)
{
	pane->GetMaster()->CleanupFolders(pane);
}

/*
 * In general, when calling the following 4 functions we must have a pane,
 * a news folder info associated the pane. However there are cases we might not
 * have a pane and not have a news folder info associated with the pane. 
 * A typical example is send later a news
 * posting to a x newsgroup, and then later deliver the message from the
 * outbox when you close the folder pane.
 */
extern "C"
void MSG_SetNewsgroupUsername(MSG_Pane *pane, const char *username)
{
  if (!pane || !pane->GetFolder() || !pane->GetFolder()->IsNews())
	return;
  pane->GetFolder()->GetNewsFolderInfo()->SetNewsgroupUsername(username);
}

extern "C"
const char * MSG_GetNewsgroupUsername(MSG_Pane *pane)
{
  if (!pane || !pane->GetFolder() || !pane->GetFolder()->IsNews())
	return NULL;
  return pane->GetFolder()->GetNewsFolderInfo()->GetNewsgroupUsername();
}


extern "C"
void MSG_SetNewsgroupPassword(MSG_Pane *pane, const char *password)
{
  if (!pane || !pane->GetFolder() || !pane->GetFolder()->IsNews())
	return;
  pane->GetFolder()->GetNewsFolderInfo()->SetNewsgroupPassword(password);
}

extern "C"
const char * MSG_GetNewsgroupPassword(MSG_Pane *pane)
{
  if (!pane || !pane->GetFolder() || !pane->GetFolder()->IsNews())
	return NULL;
  return pane->GetFolder()->GetNewsFolderInfo()->GetNewsgroupPassword();
}

extern "C" MSG_FolderInfo* MSG_GetCategoryContainerForCategory(MSG_FolderInfo *category)
{
	if (category)
	{
		MSG_FolderInfoNews *newsFolder = category->GetNewsFolderInfo();
		if (newsFolder && newsFolder->GetHost())
			return newsFolder->GetHost()->GetCategoryContainerFolderInfo(category->GetName());
	}
	return NULL;
}

#ifdef SUBSCRIBE_USE_OLD_API
extern "C" MSG_NewsHost* MSG_GetNewsHostForFolder(MSG_FolderInfo* folder)
{
	if (folder) {
		MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
		if (newsFolder) 
			return newsFolder->GetHost();
		else if (FOLDER_CONTAINERONLY == folder->GetType())
			return ((MSG_FolderInfoContainer*) folder)->GetHost();
	}
	return NULL;
}
#endif /* SUBSCRIBE_USE_OLD_API */

extern "C" MSG_Host* MSG_GetHostForFolder(MSG_FolderInfo* folder)
{
	if (folder)
	{
		MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
		if (newsFolder) 
			return newsFolder->GetHost();
		else if (FOLDER_CONTAINERONLY == folder->GetType())
			return ((MSG_FolderInfoContainer*) folder)->GetHost();
		else if (FOLDER_IMAPSERVERCONTAINER == folder->GetType())
			return ((MSG_IMAPFolderInfoContainer*) folder)->GetIMAPHost();
		else {
			MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
			if (imapFolder)
			{
				return imapFolder->GetIMAPHost();
			}
		}
	}
	return NULL;
}


extern "C" MSG_FolderInfo *
MSG_GetFolderInfoForHost(MSG_Host *host)
{
	MSG_IMAPHost *imapHost = host->GetIMAPHost();
	if (imapHost)
	{
		return imapHost->GetHostFolderInfo();
	}
	MSG_NewsHost *newsHost = host->GetNewsHost();
	if (newsHost)
	{
		return newsHost->GetHostInfo();
	}
	return NULL;
}


/* This method gets called when the Messaging Server 4.0 Admin UI changes the
   properties of a folder (e.g., sharing). It sends the url for the folder.
   We discover which server and folder this is, and note that we need to update
   the folder properties. How this will work, I don't know.
*/
extern "C" void MSG_IMAPFolderChangedNotification(const char *folder_url)
{
	MSG_Master* master = NULL;
	char *folderURL = XP_STRDUP(folder_url);
	if (folderURL)
	{
		master = FE_GetMaster();

		XP_ASSERT (master);

		if (master)
		{
#ifdef FE_IMPLEMENTS_NEW_GET_FOLDER_INFO
			MSG_FolderInfo *folder = MSG_GetFolderInfoFromURL(master, folderURL, FALSE);
#else
			MSG_FolderInfo *folder = MSG_GetFolderInfoFromURL(master, folderURL);
#endif
			if (folder)
			{
				MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
				if (imapFolder)
				{
					if (IMAP_HostHasACLCapability(imapFolder->GetHostName()))
					{
						char *refreshACLURL = CreateIMAPRefreshACLForFolderURL(imapFolder->GetHostName(), imapFolder->GetOnlineName());
						URL_Struct *URL_s = NET_CreateURLStruct(refreshACLURL, NET_DONT_RELOAD);
						if(URL_s)	// out of memory?
						{
							// maybe we should find a folder pane and use its context if we can,
							// since the folder pane may need to know.
							URL_s->internal_url = TRUE;
							MWContext *newContext = PW_CreateProgressContext();
							pw_ptr progressWindow = PW_Create(NULL, pwStandard);
							if (progressWindow)
							{
								PW_AssociateWindowWithContext(newContext, progressWindow);
								PW_SetWindowTitle(progressWindow, "Refreshing ACL");
								PW_SetLine1(progressWindow, "Refreshing ACL...");
								PW_SetLine2(progressWindow, NULL);
								PW_SetProgressRange(progressWindow, 0, 0);
								PW_Show(progressWindow);
							}
							NET_GetURL(URL_s, FO_PRESENT, newContext, NULL);
						}
					}
				}
			}
		}
		XP_FREE(folderURL);
	}
}

extern "C" int MSG_NotifyChangeDirectoryServers()
{
	MSG_Master* master = NULL;
	MSG_Pane *pane = NULL;

	master = FE_GetMaster();

	XP_ASSERT (master);

	if (master)
	{
		pane = (MSG_Pane *) master->FindFirstPaneOfType(MSG_ADDRPANE);
		while (pane != NULL)
		{
			FE_PaneChanged(pane, FALSE, MSG_PaneDirectoriesChanged, 0);
			pane = master->FindNextPaneOfType(pane->GetNextPane(), MSG_ADDRPANE);
		}	
	}
	return 0;
}

extern "C" XP_Bool MSG_RequestForReturnReceipt(MSG_Pane *pane)
{
	return pane->GetRequestForReturnReceipt();
}

extern "C" XP_Bool MSG_SendingMDNInProgress(MSG_Pane *pane)
{
	return pane->GetSendingMDNInProgress();
}


int32 atoi32(char *ascii)
{
	char *endptr;
	int32 rvalue = XP_STRTOUL(ascii, &endptr, 10);
	return rvalue;
}


extern "C" uint32 MSG_GetIMAPMessageSizeFromDB(MSG_Pane *masterPane, const char *hostName, char *folderName, char *messageId, XP_Bool idIsUid)
{
	uint32 rv = 0;
	if (masterPane && masterPane->GetMaster())
	{
		MSG_IMAPFolderInfoMail *imapFolder = masterPane->GetMaster()->FindImapMailFolder(hostName, folderName, NULL, FALSE);
		if (imapFolder)
		{
			XP_Bool wasCreated=FALSE;
			MailDB *imapDB = NULL;
			ImapMailDB::Open(imapFolder->GetPathname(), TRUE, &imapDB, imapFolder->GetMaster(), &wasCreated);
			if (imapDB)
			{
				uint32 key = atoi32(messageId);
				MailMessageHdr *mailhdr = 0;
				XP_ASSERT(idIsUid);
				if (idIsUid)
					mailhdr = imapDB->GetMailHdrForKey(key);
				if (mailhdr)
				{
					rv = mailhdr->GetMessageSize();
					delete mailhdr;
				}
				imapDB->Close();
			}
		}
	}
	return rv;
}

extern "C" void MSG_SetFolderAdminURL(MSG_Master *master, const char *hostName, const char*mailboxName, const char *url)
{
	MSG_IMAPFolderInfoMail *imapFolder = master->FindImapMailFolder(hostName, mailboxName, NULL, FALSE);
	if (imapFolder)
		imapFolder->SetAdminUrl(url);
}

extern "C" XP_Bool MSG_GetAdminUrlForFolder(MWContext *context, MSG_FolderInfo *folder, MSG_AdminURLType type)
{
	return folder->GetAdminUrl(context, type);
}

/* use this to decide to show buttons and/or menut items */
extern "C" XP_Bool MSG_HaveAdminUrlForFolder(MSG_FolderInfo *folder, MSG_AdminURLType type)
{
	return folder->HaveAdminUrl(type);
}

extern "C" void MSG_RefreshFoldersForUpdatedIMAPHosts(MWContext *context)
{
	MSG_FolderPane *folderPane = (MSG_FolderPane *) MSG_FindPane(context, MSG_FOLDERPANE);
	if (folderPane)
	{
		folderPane->RefreshUpdatedIMAPHosts();
	}
}


// Returns a newly-allocated space-delimited list of the arbitrary headers needed to be downloaded
// for a given host, when using IMAP.  Returns NULL if there are none, or the host could not be found.
extern "C" char *MSG_GetArbitraryHeadersForHost(MSG_Master *master, const char *hostName)
{
	if (!master)
		return NULL;

	char *headersFromFilters = master->GetArbitraryHeadersForHostFromFilters(hostName);
	char *headersForMDN = master->GetArbitraryHeadersForHostFromMDN(hostName);
	if (!headersFromFilters && !headersForMDN)
		return NULL;
	if (headersFromFilters && !headersForMDN)
		return headersFromFilters;
	if (!headersFromFilters && headersForMDN)
		return headersForMDN;

	// Headers for both filters and MDN
	char *finalHeaders = PR_smprintf("%s %s",headersFromFilters,headersForMDN);
	XP_FREE(headersFromFilters);
	XP_FREE(headersForMDN);
	return finalHeaders;
}


extern "C" XP_Bool MSG_GetCanCreateSubfolderOfFolder(MSG_FolderInfo *f)
{
	if (f->GetType() != FOLDER_MAIL &&
		f->GetType() != FOLDER_IMAPMAIL &&
		f->GetType() != FOLDER_IMAPSERVERCONTAINER)
		return FALSE;

	switch (f->GetType())
	{
	case FOLDER_IMAPMAIL:
		{
			MSG_IMAPFolderInfoMail *imapInfo = f->GetIMAPFolderInfoMail();
			if (!imapInfo)
			{
				XP_ASSERT(FALSE);
				return FALSE;
			}
			return imapInfo->GetCanDropFolderIntoThisFolder();
		}
		break;
	default:	// local mail or IMAP server container
		return TRUE;
		break;
	}
}

   
// current is the currently-selected folder.  If current is NULL, returns NULL.
// Otherwise, returns the MSG_FolderInfo* of the suggested default parent folder which
// should be used in the New Folder dialog.
extern "C" MSG_FolderInfo *MSG_SuggestNewFolderParent(MSG_FolderInfo *current, MSG_Master *master)
{
	if (!current)
		return current;

	MSG_FolderInfo *tree = master->GetFolderTree();
	MSG_FolderInfo *localTree = master->GetLocalMailFolderTree();
	if (!tree || !localTree)
		return current;

	// If current is "Local Mail" or one of the IMAP servers, return current
	if (current == localTree ||
		current->GetType() == FOLDER_IMAPSERVERCONTAINER)
	{
		return current;
	}

	// Figure out a default.
	MSG_FolderInfo *defaultSuggestion = localTree;
	MSG_IMAPHostTable *imapHostTable = master->GetIMAPHostTable();
	if (imapHostTable)
	{
		MSG_IMAPHost *defaultImapHost = imapHostTable->GetDefaultHost();
		if (defaultImapHost)
			defaultSuggestion = defaultImapHost->GetHostFolderInfo();
	}

	if (current->GetType() == FOLDER_CONTAINERONLY ||	// a news host
		current->GetType() == FOLDER_NEWSGROUP ||		// a news group
		current->GetType() == FOLDER_CATEGORYCONTAINER)	// a news category container
	{
		return defaultSuggestion;	// return default -- we can't create a subfolder of a news host
									// -- not from the client's New Folder dialog, anyway
	}

	MSG_FolderInfo *parent = tree->FindParentOf(current);
	if (!parent)
		return defaultSuggestion;

	if (current->GetType() == FOLDER_MAIL)	// a local mail folder.
	{
		return parent ? parent : localTree;	// Return the parent of the currently selected folder
	}


	// If it gets here, we think it's a mail folder.
	if (MSG_GetCanCreateSubfolderOfFolder(parent))
		return parent;
	else
	{
		if (current->GetType() == FOLDER_MAIL)
			return localTree;
		else
		{
			MSG_IMAPFolderInfoMail *imapInfo = current->GetIMAPFolderInfoMail();
			if (imapInfo)
			{
				MSG_IMAPHost *host = imapInfo->GetIMAPHost();
				if (host)
					return host->GetHostFolderInfo();
			}
			return defaultSuggestion;
		}
	}
}


// Given a folder info, returns a newly allocated string containing a description
// of the folder rights for a given folder.  It is the caller's responsibility
// to free this string.
// Returns NULL if we could not get ACL rights information for this folder.

extern "C" char *MSG_GetACLRightsStringForFolder(MSG_FolderInfo *folder)
{
	char *rv = NULL;
	MSG_IMAPFolderInfoMail *imapInfo = folder->GetIMAPFolderInfoMail();
	if (imapInfo)
	{
		rv = imapInfo->CreateACLRightsStringForFolder();
	}
	else 
		rv = PR_smprintf(XP_GetString(XP_MSG_IMAP_ACL_FULL_RIGHTS));
	return rv;
}

/* Given a folder info, returns a newly allocated string with the folder
   type name.  For instance, "Personal Folder", "Public Folder", etc.
   It is the caller's responsibility to free this string.
   Returns NULL if we could not get the a type for this folder.
 */

extern "C" char *MSG_GetFolderTypeName(MSG_FolderInfo *folder)
{
	char *rv = NULL;
	MSG_IMAPFolderInfoMail *imapInfo = folder->GetIMAPFolderInfoMail();
	if (imapInfo)
	{
		rv = imapInfo->GetTypeNameForFolder();
	}
	else
		rv = PR_smprintf(XP_GetString(XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_NAME));
	return rv;
}

/* Given a folder info, returns a newly allocated string containing a description
   of the folder type.  For instance, "This is a personal mail folder that you have shared."
   It is the caller's responsibility to free this string.
   Returns NULL if we could not get the a type for this folder.
 */

extern "C" char *MSG_GetFolderTypeDescription(MSG_FolderInfo *folder)
{
	char *rv = NULL;
	MSG_IMAPFolderInfoMail *imapInfo = folder->GetIMAPFolderInfoMail();
	if (imapInfo)
	{
		rv = imapInfo->GetTypeDescriptionForFolder();
	}
	else
		rv = PR_smprintf(XP_GetString(XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_DESCRIPTION));
	return rv;
}

/* Returns TRUE if the given IMAP host supports the sharing of folders. */

extern "C" XP_Bool MSG_GetHostSupportsSharing(MSG_IMAPHost *host)
{
	return host->GetHostSupportsSharing();
}


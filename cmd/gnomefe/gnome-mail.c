/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* 
   gnomemail.c --- gnome functions for fe
                   specific mail/news stuff.
*/

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"
#include "msgcom.h"
#include "addrbook.h"
#include "dirprefs.h"

const char*
FE_UsersMailAddress()
{
  printf("FE_UsersMailAddress (empty)\n");
}

const char*
FE_UsersFullName()
{
  printf("FE_UsersFullName (empty)\n");
}

const char *
FE_UsersOrganization()
{
  printf("FE_UsersOrganization (empty)\n");
}

const char*
FE_UsersSignature()
{
  printf("FE_UsersSignature (empty)\n");
}

void
FE_ListChangeStarting(MSG_Pane* pane,
		      XP_Bool asynchronous,
		      MSG_NOTIFY_CODE notify,
		      MSG_ViewIndex where,
		      int32 num)
{
  printf("FE_ListChangeStarting (empty)\n");
}

void
FE_ListChangeFinished(MSG_Pane* pane,
		      XP_Bool asynchronous,
		      MSG_NOTIFY_CODE notify,
		      MSG_ViewIndex where,
		      int32 num)
{
  printf("FE_ListChangeFinished (empty)\n");
}

void
FE_PaneChanged(MSG_Pane *pane,
	       XP_Bool asynchronous, 
	       MSG_PANE_CHANGED_NOTIFY_CODE notify_code,
	       int32 value)
{
  printf("FE_PaneChanged (empty)\n");
}

char*
FE_GetTempFileFor(MWContext* context,
		  const char* fname,
		  XP_FileType ftype,
		  XP_FileType* rettype)
{
  printf("FE_GetTempFileFor (empty)\n");
}

void
FE_UpdateBiff(MSG_BIFF_STATE state)
{
  printf("FE_UpdateBiff (empty)\n");
}

uint32
FE_DiskSpaceAvailable (MWContext* context,
		       const char* dir)
{
  printf("FE_DiskSpaceAvailable (empty)\n");
}

MSG_Pane*
FE_CreateCompositionPane(MWContext* old_context,
			 MSG_CompositionFields* fields,
			 const char* initialText,
			 MSG_EditorType editorType)
{
  printf("FE_CreateCompositionPane (empty)\n");
}

void
FE_UpdateCompToolbar(MSG_Pane* comppane)
{
  printf("FE_UpdateCompToolbar (empty)\n");
}

void
FE_DestroyMailCompositionContext(MWContext* context)
{
  printf("FE_DestroyMailCompositionContext (empty)\n");
}

MWContext*
FE_GetAddressBookContext(MSG_Pane* pane,
			 XP_Bool viewnow)
{
  printf("FE_GetAddressBookContext (empty)\n");
}

ABook*
FE_GetAddressBook(MSG_Pane* pane)
{
  printf("FE_GetAddressBook (empty)\n");
}

int
FE_ShowPropertySheetFor (MWContext* context,
			 ABID entryID, 
			 PersonEntry* pPerson)
{
  printf("FE_ShowPropertySheetFor (empty)\n");
}

XP_List* 
FE_GetDirServers(void)
{
  printf("FE_GetDirServers (empty)\n");
}

MSG_Master*
FE_GetMaster()
{
  printf("FE_GetMaster (empty)\n");
}

XP_Bool
FE_IsAltMailUsed(MWContext* context)
{
  printf("FE_IsAltMailUsed (empty)\n");
}

MSG_IMAPUpgradeType
FE_PromptIMAPSubscriptionUpgrade(MWContext* context, const char *host)
{
  printf("FE_PromptIMAPSubscriptionUpgrade (empty)\n");
}

XP_Bool
FE_CreateSubscribePaneOnHost(MSG_Master* master,
			     MWContext* parentContext,
			     MSG_Host* host)
{
  printf("FE_CreateSubscribePaneOnHost (empty)\n");
}

const char *
FE_UsersRealMailAddress()
{
  printf("FE_UsersRealMailAddress (empty)\n");
}

void
FE_RememberPopPassword(MWContext* context,
		       const char* password)
{
  printf("FE_RememberPopPassword (empty)\n");
}

XP_Bool 
FE_NewsDownloadPrompt(MWContext *context,
		      int32 numMessagesToDownload,
		      XP_Bool *downloadAll)
{
  printf("FE_NewsDownloadPrompt (empty)\n");
}

void
FE_MsgShowHeaders(MSG_Pane *pPane,
		  MSG_HEADER_SET mhsHeaders)
{
  printf("FE_MsgShowHeaders (empty)\n");
}

/* If we're set up to deliver mail/news by running a program rather
   than by talking to SMTP/NNTP, this does it.

   Returns positive if delivery via program was successful;
   Returns negative if delivery failed;
   Returns 0 if delivery was not attempted (in which case we
   should use SMTP/NNTP instead.)

   $NS_MSG_DELIVERY_HOOK names a program which is invoked with one argument,
   a tmp file containing a message.  (Lines are terminated with CRLF.)
   This program is expected to parse the To, CC, BCC, and Newsgroups headers,
   and effect delivery to mail and/or news.  It should exit with status 0
   iff successful.

   #### This really wants to be defined in libmsg, but it wants to
   be able to use fe_perror, so...
 */
int
msg_DeliverMessageExternally(MWContext *context, const char *msg_file)
{
  printf("msg_DeliverMessageExternally (empty)\n");
}

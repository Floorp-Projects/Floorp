/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
** msgmapi.h -- implements XP mail/news support for the Microsoft Mail API (MAPI)
**
*/

#ifndef _MSGMAPI_H
#define _MSGMAPI_H

#ifdef XP_WIN

#include "msgcom.h"
#include "abcom.h"

// rhp - to fix the problem caused by MSFT...grrrrr...
#ifndef MAPI_OLE	// Because MSFT doesn't do this for us :-(
#include <mapi.h>     	// for MAPI specific types...        
#endif 

#ifdef XP_CPLUSPLUS
class MSG_MapiListContext;
#else
typedef struct MSG_MapiListContext MSG_MapiListContext;
#endif

XP_BEGIN_PROTOS

MessageKey MSG_GetFirstKeyInFolder (MSG_FolderInfo *folder, MSG_MapiListContext **cookie);
MessageKey MSG_GetNextKeyInFolder (MSG_MapiListContext *cookie);
XP_Bool MSG_GetMapiMessageById (MSG_FolderInfo *folder, MessageKey key, lpMapiMessage *message);
XP_Bool MSG_MarkMapiMessageRead (MSG_FolderInfo *folder, MessageKey key, XP_Bool read);
void MSG_FreeMapiMessage (lpMapiMessage msg);

/* Address Book Specific APIs to support MAPI calls for MAPIDetails and MAPIResolveName */

int AB_MAPI_ResolveName(
	char * string,
	AB_ContainerInfo ** ctr, /* caller allocates ptr to ctr, BE fills. Caller must close the ctr when done */
	ABID * id);

/* Caller must free the character strings returned by these functions using XP_FREE */
char * AB_MAPI_GetEmailAddress(AB_ContainerInfo * ctr,ABID id);

char * AB_MAPI_GetFullName(AB_ContainerInfo * ctr, ABID id);

char * AB_MAPI_ConvertToDescription(AB_ContainerInfo * ctr);

AB_ContainerInfo * AB_MAPI_ConvertToContainer(MWContext * context, char * description);

int AB_MAPI_CreatePropertySheetPane(
	MWContext * context,
	MSG_Master * master,
	AB_ContainerInfo * ctr, 
	ABID id,
	MSG_Pane **  personPane); /* BE fills the ptr with a person pane */

void AB_MAPI_CloseContainer(AB_ContainerInfo ** ctr);

XP_END_PROTOS

#endif /* XP_WIN */

#endif /* _MSGMAPI_H */

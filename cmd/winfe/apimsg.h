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

#ifndef _APIMSG_H
#define _APIMSG_H

#ifndef __APIAPI_H
    #include "apiapi.h"
#endif
#ifndef __NSGUIDS_H
    #include "nsguids.h"
#endif

#include "msgcom.h"
/////////////////////////////////////////////////////////////////////
// This interface applies to those MSG_Panes that are compose windows

class IMsgCompose: public IUnknown {
public:
// Initialization/Demolition
	virtual void InitializeMailCompositionContext( MSG_Pane* comppane, 
												   const char *from,
												   const char *reply_to,
												   const char *to,
												   const char *cc,
												   const char *bcc,
												   const char *fcc,
												   const char *newsgroups,
												   const char *followup_to,
												   const char *subject,
												   const char *attachment ) = 0;
	virtual void RaiseMailCompositionWindow( MSG_Pane* comppane ) = 0;
	virtual void DestroyMailCompositionContext( MWContext* context ) = 0;

// Misc Operations
	virtual void UpdateToolbar ( MSG_Pane* comppane ) = 0;
	virtual void MsgShowHeaders ( MSG_Pane* comppane, MSG_HEADER_SET headers ) = 0;
	virtual char *PromptMessageSubject( MSG_Pane* comppane ) = 0;

// Message Operations
	virtual void InsertMessageCompositionText( MSG_Pane* comppane,
											   const char *text,
											   XP_Bool leaveCursorAtBeginning) = 0;
	virtual int GetMessageBody( MSG_Pane* comppane,
								char **body,
								uint32 *body_size,
								MSG_FontCode **font_changes ) = 0;								
	virtual void DoneWithMessageBody(MSG_Pane* comppane, char* body,
									 uint32 body_size) = 0;
};

typedef IMsgCompose *LPMSGCOMPOSE;

///////////////////////////////////////////////////////////
// This interface applies to those MSG_Panes that are lists

class IMsgList: public IUnknown {
public:
// Operations
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num) = 0;
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num) = 0;
	virtual void GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							   int *focus) = 0;
	virtual void SelectItem( MSG_Pane* pane, int item ) = 0;

	virtual void CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) = 0;
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) = 0;
};

typedef IMsgList *LPMSGLIST;

/* Unfiled: 
	MSG_Pane*	FE_CreateCompositionPane( MWContext *context);
	char*		FE_GetTempFileFor( MWContext *context, const char *fname,
									XP_FileType ftype, XP_FileType *rettype);
	void		FE_UpdateBiff( MSG_BIFF_STATE state );
	uint32		FE_DiskSpaceAvailable (MWContext *context, const char *dir);
*/

#endif

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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// MailNewsCallbacks.cp

#include "MailNewsCallbacks.h"

// XP headers
#include "fe_proto.h"
//#include "errcode.h"
//#include "msglpane.h"
#include "addrbook.h"


// FE
#include "CCheckMailContext.h"
#include "uprefd.h"
#include "macutil.h"

#include "CMailFlexTable.h"
#include "CMessageWindow.h"
#include "CThreadWindow.h"

#include "UMailFolderMenus.h"
#include "CMessageFolder.h"
#include "ufilemgr.h"

//======================================
//	class CMailCallbackManager
//======================================

CMailCallbackManager* CMailCallbackManager::sMailCallbackManager = nil;

//-----------------------------------
CMailCallbackManager::CMailCallbackManager()
//-----------------------------------
{
	sMailCallbackManager = this;
}

//-----------------------------------
CMailCallbackManager::~CMailCallbackManager()
//-----------------------------------
{
	sMailCallbackManager = nil;
}

//-----------------------------------
CMailCallbackManager* CMailCallbackManager::Get()
//-----------------------------------
{
	if (!sMailCallbackManager)
		new CMailCallbackManager;
	return sMailCallbackManager;
}

//-----------------------------------
Boolean CMailCallbackManager::ValidData(MSG_Pane *inPane)
//-----------------------------------
{
	void* data = MSG_GetFEData(inPane);
	//? We WERE getting callbacks before MSG_SetFEData was called.      Assert_(data == this);
	return (data == this);
}

//-----------------------------------
void CMailCallbackManager::PaneChanged(
	MSG_Pane *inPane,
	XP_Bool asynchronous, 
	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	int32 value)
//-----------------------------------
{
	if (inNotifyCode == MSG_PaneNotifyFolderDeleted)
	{
		// A folder can be deleted even with no active pane (eg, in IMAP synchronization).
		// In this case, we get a call with inPane == NULL.
		CMailFolderMixin::UpdateMailFolderMixins();
	}
	if (ValidData(inPane))
	{
		// the backend can give us a MSG_VIEWINDEXNONE (-1) in value, which we need to change to a 0
		if (value == MSG_VIEWINDEXNONE)
			value = 0;
			
		// Because of the caching scheme for folders, we must update the caches first:
		if (inNotifyCode == MSG_PaneNotifyFolderInfoChanged)
		{
			CCachedFolderLine::FolderInfoChanged(inPane, (MSG_FolderInfo*)value);
		}
		SPaneChangeInfo info(inPane, asynchronous, inNotifyCode, value);
		BroadcastMessage(msg_PaneChanged, &info);
	}
} // CMailCallbackManager::PaneChanged

//-----------------------------------
void CMailCallbackManager::ChangeStarting(
	MSG_Pane*		inPane, 
	XP_Bool 		inAsync,
	MSG_NOTIFY_CODE inNotifyCode, 
	MSG_ViewIndex 	inWhere,
	int32 			inCount)
//-----------------------------------
{
	if (ValidData(inPane))
	{
		SLineChangeInfo info(inPane, inAsync, inNotifyCode, inWhere + 1, inCount);
		BroadcastMessage(msg_ChangeStarting, &info);
	}
} // CMailCallbackManager::ChangeStarting

//-----------------------------------
void CMailCallbackManager::ChangeFinished(
	MSG_Pane*		inPane, 
	XP_Bool 		inAsync,
	MSG_NOTIFY_CODE inNotifyCode, 
	MSG_ViewIndex 	inWhere,
	int32 			inCount)
//-----------------------------------
{
	if (ValidData(inPane))
	{
		SLineChangeInfo info(inPane, inAsync, inNotifyCode, inWhere + 1, inCount);
		BroadcastMessage(msg_ChangeFinished, &info);
	}
} // CMailCallbackManager::ChangeFinished

//======================================
// class CMailCallbackListener
//======================================

//-----------------------------------
CMailCallbackListener::CMailCallbackListener()
//-----------------------------------
:	LListener()
,	mPane(nil)
{
	CMailCallbackManager::Get()->AddUser(this);
	CMailCallbackManager::Get()->AddListener(this);
}

//-----------------------------------
CMailCallbackListener::~CMailCallbackListener()
//-----------------------------------
{
	mPane = nil; // stop derived classes from listening to callbacks.
	CMailCallbackManager::Get()->RemoveListener(this);
	CMailCallbackManager::Get()->RemoveUser(this);
}

//-----------------------------------
void CMailCallbackListener::ListenToMessage(MessageT inMessage, void* ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
		case CMailCallbackManager::msg_PaneChanged:
			if ( IsMyPane(ioParam) ) {
				SPaneChangeInfo* info = reinterpret_cast<SPaneChangeInfo*>(ioParam);
				PaneChanged(info->pane, info->notifyCode, info->value);
			}
			break;

		case CMailCallbackManager::msg_ChangeStarting:
			if ( IsMyPane(ioParam) ) {
				SLineChangeInfo* info = reinterpret_cast<SLineChangeInfo*>(ioParam);
				ChangeStarting(info->pane, info->changeCode, info->startRow, info->rowCount);
			}
			break;

		case CMailCallbackManager::msg_ChangeFinished:
			if ( IsMyPane(ioParam) ) {
				SLineChangeInfo* info = reinterpret_cast<SLineChangeInfo*>(ioParam);
				ChangeFinished(info->pane, info->changeCode, info->startRow, info->rowCount);
			}
			break;
	}
} // CMailCallbackListener::ListenToMessage

//-----------------------------------
Boolean CMailCallbackListener::IsMyPane(void* info) const
//-----------------------------------
{
	return (mPane == reinterpret_cast<SMailCallbackInfo*>(info)->pane);
}

//-----------------------------------
void CMailCallbackListener::ChangeStarting(
	MSG_Pane*		/*inPane*/,
	MSG_NOTIFY_CODE	/*inChangeCode*/,
	TableIndexT		/*inStartRow*/,
	SInt32			/*inRowCount*/)
//-----------------------------------
{
}

//-----------------------------------
void CMailCallbackListener::ChangeFinished(
	MSG_Pane*		/*inPane*/,
	MSG_NOTIFY_CODE	/*inChangeCode*/,
	TableIndexT		/*inStartRow*/,
	SInt32			/*inRowCount*/)
//-----------------------------------
{
}
//======================================


//-----------------------------------
void FE_UpdateBiff(MSG_BIFF_STATE inState) 
// 	Called when "check for mail" state changes.
//-----------------------------------
{
 	CCheckMailContext::Get()->SetState(inState);
}

//
// inDir argument is a Mac path.
uint32 FE_DiskSpaceAvailable( MWContext* inContext, const char* inDir)
//-----------------------------------
{
	Boolean isMailContext
		= (		inContext->type == MWContextMailMsg
			||	inContext->type == MWContextMail
			||	inContext->type == MWContextMessageComposition
			||	inContext->type == MWContextMailNewsProgress);
	
	if (isMailContext)
	{
		FSSpec folder = CPrefs::GetFolderSpec( CPrefs::MailFolder );
		return GetFreeSpaceInBytes( folder.vRefNum );
	}
	else
	{
		FSSpec folder;
		Str255 fileName;
		OSErr err;
		if (inDir)
		{
			XP_MEMCPY(fileName, inDir, 255);
			c2pstr((char*)fileName);
			folder.vRefNum = folder.parID = 0;
			err =  FSMakeFSSpec(folder.vRefNum, folder.parID, fileName, &folder);
		}
		else
			err = fnfErr;
		if ( err != noErr) 
		{
			XP_ASSERT(FALSE);
			return -1;
		}
		else
			return GetFreeSpaceInBytes( folder.vRefNum );
	}
	
}

//-----------------------------------
void FE_ListChangeStarting(
	MSG_Pane*		inPane, 
	XP_Bool 		inAsync,
	MSG_NOTIFY_CODE inNotifyCode, 
	MSG_ViewIndex 	inWhere,
	int32 			inCount)
//-----------------------------------
{
	CMailCallbackManager::Get()
		->ChangeStarting(inPane, inAsync, inNotifyCode, inWhere, inCount);
} // FE_ListChangeStarting
						
//-----------------------------------
void FE_ListChangeFinished(
	MSG_Pane* 		inPane, 
	XP_Bool			inAsync,
	MSG_NOTIFY_CODE	inNotifyCode, 
	MSG_ViewIndex 	inWhere,
	int32 			inCount)
//-----------------------------------
{
	CMailCallbackManager::Get()
		->ChangeFinished(inPane, inAsync, inNotifyCode, inWhere, inCount);
} // FE_ListChangeFinished

//-----------------------------------
void FE_PaneChanged(
	MSG_Pane *inPane,
	XP_Bool asynchronous, 
	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	int32 value)
//-----------------------------------
{
	CMailCallbackManager::Get()->PaneChanged(inPane, asynchronous, inNotifyCode, value);
}

// ¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥
//
// Awaiting a real composition implementation,
// at which point, these should move there.
//
// ¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥


extern "C" void FE_InsertMessageCompositionText( MWContext* /*inContext*/,  const char* /*text*/, XP_Bool /*leaveCursorAtBeginning*/);
void FE_InsertMessageCompositionText( MWContext* /*inContext*/,  const char* /*text*/, XP_Bool /*leaveCursorAtBeginning*/) {}

extern "C" void FE_MsgShowHeaders(MSG_Pane *pPane, MSG_HEADER_SET mhsHeaders);
void 		FE_MsgShowHeaders (MSG_Pane* /*comppane*/, MSG_HEADER_SET /*headers*/) {}


extern void FE_UpdateCompToolbar(MSG_Pane* /*comppane*/); // in CMailComposeWindow.cp

MSG_Master* FE_GetMaster()
{
	// implement get master
	return CMailNewsContext::GetMailMaster();
}

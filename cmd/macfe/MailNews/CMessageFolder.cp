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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CMessageFolder.cp

#include "CMessageFolder.h"

#include "MailNewsCallbacks.h"
#include "CMailNewsContext.h"

//======================================
#pragma mark --- CCachedFolderLine
//======================================

#if 0
//======================================
class CMessageFolderListener : public CMailCallbackListener
//======================================
{
public:
								CMessageFolderListener() { sMessageFolderListener = this; }
								~CMessageFolderListener() { sMessageFolderListener = nil; }
	virtual void 				PaneChanged(
									MSG_Pane* inPane,
									MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
									int32 value); // must always be supported.
	static CMessageFolderListener* sMessageFolderListener;
};
#endif // 0

//======================================
class CFolderLineComparator : public LComparator
//======================================
{
	virtual Int32			Compare(
								const void*			inItemOne,
								const void* 		inItemTwo,
								Uint32				inSizeOne,
								Uint32				inSizeTwo) const;
	virtual Int32			CompareToKey(
								const void*		inItem,
								Uint32			/* inSize */,
								const void*		inKey) const;
}; // class CFolderLineComparator

LArray* CCachedFolderLine::sLineList = nil;

//----------------------------------------------------------------------------------------
Int32 CFolderLineComparator::Compare(
								const void*			inItemOne,
								const void* 		inItemTwo,
								Uint32				/*inSizeOne*/,
								Uint32				/*inSizeTwo*/) const
//----------------------------------------------------------------------------------------
{
	return (Int32)(*(const CCachedFolderLine**)inItemOne)->mFolderLine.id
	 - (Int32)(*(const CCachedFolderLine**)inItemTwo)->mFolderLine.id;
}

//----------------------------------------------------------------------------------------
Int32 CFolderLineComparator::CompareToKey(
	const void*		inItem,
	Uint32			/* inSize */,
	const void*		inKey) const
//----------------------------------------------------------------------------------------
{
	return (Int32)(*(CCachedFolderLine**)inItem)->mFolderLine.id
	 - (Int32)(const MSG_FolderInfo*)inKey;
}

//----------------------------------------------------------------------------------------
CCachedFolderLine::CCachedFolderLine(MSG_Pane* inFolderPane, MSG_FolderInfo* inID)
//----------------------------------------------------------------------------------------
:	mRefCount(0)
{
	mFolderLine.id = inID; // set this up for the FolderInfoChanged() call.
	FolderInfoChanged(inFolderPane);
	// Listen for changes to the folder info.
	//		if (!CMessageFolderListener::sMessageFolderListener)
	//			new CMessageFolderListener;
	if (!sLineList)
	{
		// Initialize the sorted array
		sLineList = new LArray(sizeof(CCachedFolderLine*), new CFolderLineComparator, true);
		sLineList->AdjustAllocation(50); // Allow for 50 items initially
	}
	sLineList->InsertItemsAt(1, 0/*ignored*/, &this);
} // CCachedFolderLine::CCachedFolderLine

//----------------------------------------------------------------------------------------
CCachedFolderLine::~CCachedFolderLine()
//----------------------------------------------------------------------------------------
{
	if (sLineList)
	{
		sLineList->Remove(&this);
		// When the last cached folderline goes, no need to have a listener.
		if (sLineList->GetCount() == 0)
		{
			delete sLineList;
			sLineList = nil;
//			delete CMessageFolderListener::sMessageFolderListener;
		}
	}
} // CCachedFolderLine::~CCachedFolderLine

//----------------------------------------------------------------------------------------
void CCachedFolderLine::FolderInfoChanged(MSG_Pane* inFolderPane)
//----------------------------------------------------------------------------------------
{
	MSG_FolderInfo* id = mFolderLine.id; // this is non-volatile
	if (id)
	{
		MSG_ViewIndex index = MSG_VIEWINDEXNONE;
		if (inFolderPane && ::MSG_GetPaneType(inFolderPane) == MSG_FOLDERPANE)
		{
			// Only relative to a pane can we get the "elided" bit right, to tell
			// whether or not the twistie is open.
			index = ::MSG_GetFolderIndex(inFolderPane, id);
		}
		if (index != MSG_VIEWINDEXNONE)
			::MSG_GetFolderLineByIndex(inFolderPane, index, 1, &mFolderLine);
		else
			::MSG_GetFolderLineById(CMailNewsContext::GetMailMaster(), id, &mFolderLine);
	}
	else
	{
		// There is a hack in which we add a NULL MSG_FolderInfo* to the cache.
		// Rather than call msglib to initialize the folder line (which will cause
		// an assert) just zero out the folderline ourselves.
		memset(&mFolderLine, 0, sizeof(MSG_FolderLine));
	}
	Assert_(id==mFolderLine.id);
}

//----------------------------------------------------------------------------------------
void CMessageFolder::FolderLevelChanged(MSG_Pane* inFolderPane)
// Calls FolderInfoChanged on this and all its descendents.
//----------------------------------------------------------------------------------------
{
	FolderInfoChanged(inFolderPane);
	UInt32 childCount = CountSubFolders();
	if (childCount)
	{
		typedef MSG_FolderInfo* mfip;
		try
		{
			MSG_FolderInfo** childList = new mfip[childCount]; // throws...
			::MSG_GetFolderChildren(
				CMailNewsContext::GetMailMaster(),
				GetFolderInfo(),
				childList,
				childCount);
			MSG_FolderInfo** child = &childList[0];
			for (SInt32 j = 0; j < childCount; j++, child++)
			{
				CMessageFolder childFolder(*child, inFolderPane);
				childFolder.FolderLevelChanged(inFolderPane);
			}
			delete [] childList;
		}
		catch(...)
		{
		}
	}
} // CMessageFolder::FolderLevelChanged

//----------------------------------------------------------------------------------------
void CCachedFolderLine::FolderInfoChanged(MSG_Pane* inFolderPane, MSG_FolderInfo* inID)
//----------------------------------------------------------------------------------------
{
	CCachedFolderLine* folder = FindFolderFor(inID);
	if (folder)
		folder->FolderInfoChanged(inFolderPane);
}

//----------------------------------------------------------------------------------------
CCachedFolderLine* CCachedFolderLine::FindFolderFor(MSG_FolderInfo* inID)
//----------------------------------------------------------------------------------------
{
	if (!sLineList)
		return nil;
	CCachedFolderLine* result = nil;
	ArrayIndexT arrayIndex = sLineList->FetchIndexOfKey(inID);
	if (arrayIndex != LArray::index_Bad)
	{
		sLineList->FetchItemAt(arrayIndex, &result);
	}
	return result;
}

//----------------------------------------------------------------------------------------
CCachedFolderLine* CCachedFolderLine::GetFolderFor(MSG_Pane* inFolderPane, MSG_FolderInfo* inID)
//----------------------------------------------------------------------------------------
{
	CCachedFolderLine* folderLine = FindFolderFor(inID);
	if (!folderLine)
		folderLine = new CCachedFolderLine(inFolderPane, inID);
	return folderLine;
}

//----------------------------------------------------------------------------------------
CCachedFolderLine* CCachedFolderLine::GetFolderFor(MSG_Pane* inFolderPane, MSG_ViewIndex inIndex)
//----------------------------------------------------------------------------------------
{
	MSG_FolderInfo* id = ::MSG_GetFolderInfo(inFolderPane, inIndex);
	CCachedFolderLine* folderLine = FindFolderFor(id);
	if (!folderLine)
		folderLine = new CCachedFolderLine(inFolderPane, id);
	return folderLine;
}

#if 0
//======================================
#pragma mark --- CMessageFolderListener
//======================================

CMessageFolderListener* CMessageFolderListener::sMessageFolderListener = nil;

//----------------------------------------------------------------------------------------
void CMessageFolderListener::PaneChanged(
	MSG_Pane* inPane,
	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	int32 value)
//----------------------------------------------------------------------------------------
{
	if (inNotifyCode == MSG_PaneNotifyFolderInfoChanged)
		CCachedFolderLine::FolderInfoChanged((MSG_FolderInfo*)value);
}
#endif // 0

//======================================
#pragma mark --- CMessageFolder
//======================================

//----------------------------------------------------------------------------------------
CMessageFolder::CMessageFolder(TableIndexT inRow, MSG_Pane* inFolderList) 
//----------------------------------------------------------------------------------------
{
	mCachedFolderLine = CCachedFolderLine::GetFolderFor(inFolderList, inRow - 1);
	mCachedFolderLine->AddUser(this);
}

//----------------------------------------------------------------------------------------
CMessageFolder::CMessageFolder(const MSG_FolderInfo* id, MSG_Pane* inFolderPane)
//----------------------------------------------------------------------------------------
{
	mCachedFolderLine = CCachedFolderLine::GetFolderFor(inFolderPane, (MSG_FolderInfo*)id);
	mCachedFolderLine->AddUser(this);
}

//----------------------------------------------------------------------------------------
CMessageFolder::CMessageFolder(const CMessageFolder& inFolder)
//----------------------------------------------------------------------------------------
{
	mCachedFolderLine = inFolder.mCachedFolderLine;
	mCachedFolderLine->AddUser(this);
}

//----------------------------------------------------------------------------------------
CMessageFolder::~CMessageFolder()
//----------------------------------------------------------------------------------------
{
	mCachedFolderLine->RemoveUser(this);
}

//----------------------------------------------------------------------------------------
void CMessageFolder::operator=(const CMessageFolder& other)
//----------------------------------------------------------------------------------------
{
	if (!(*this == other)) // uses operator ==
	{
		mCachedFolderLine->RemoveUser(this);
		mCachedFolderLine = other.mCachedFolderLine;
		mCachedFolderLine->AddUser(this);
	}
}

//----------------------------------------------------------------------------------------
MSG_FolderInfo* CMessageFolder::GetFolderInfo() const
//----------------------------------------------------------------------------------------
{
	return mCachedFolderLine->mFolderLine.id;
}

//----------------------------------------------------------------------------------------
void CMessageFolder::SetFolderInfo(const MSG_FolderInfo* id, MSG_Pane* inFolderPane)
//----------------------------------------------------------------------------------------
{
	if (id != GetFolderInfo())
	{
		mCachedFolderLine->RemoveUser(this);
		mCachedFolderLine = CCachedFolderLine::GetFolderFor(inFolderPane, (MSG_FolderInfo*)id);
		mCachedFolderLine->AddUser(this);
	}
}

//----------------------------------------------------------------------------------------
MSG_Pane* CMessageFolder::GetThreadPane() const
//----------------------------------------------------------------------------------------
{
	return ::MSG_FindPaneOfType(
			CMailNewsContext::GetMailMaster(),
			GetFolderInfo(),
			MSG_THREADPANE);
}

//----------------------------------------------------------------------------------------
MSG_Pane* CMessageFolder::GetFolderPane()
//----------------------------------------------------------------------------------------
{
	return ::MSG_FindPane(
			nil, // Don't care about context matching
			MSG_FOLDERPANE);
}

//----------------------------------------------------------------------------------------
void CMessageFolder::FolderInfoChanged(MSG_Pane* inFolderPane)
//----------------------------------------------------------------------------------------
{
	mCachedFolderLine->FolderInfoChanged(inFolderPane);
}

//----------------------------------------------------------------------------------------
Int32 CMessageFolder::CountMessages() const
//----------------------------------------------------------------------------------------
{
	if (mCachedFolderLine->mFolderLine.unseen < 0)
		return -1; // hack: this indicates unknown.
	return mCachedFolderLine->mFolderLine.total;
}

//----------------------------------------------------------------------------------------
UInt32 CMessageFolder::GetDeletedBytes() const
//----------------------------------------------------------------------------------------
{
	if (mCachedFolderLine->mFolderLine.deletedBytes < 0)
		return -1; // hack: this indicates unknown.
	return mCachedFolderLine->mFolderLine.deletedBytes;
}

//
// Icon suite IDs
//
enum
{

	kNormalMessageFolderIconID				=	15238
,	kInboxMessageFolderIconID				=	15240
,	kInboxWithNewMailMessageFolderIconID	=	15242
,	kOutboxMessageFolderIconID				=	15244
,	kSentMailMessageFolderIconID			=	15246
,	kDraftsMessageFolderIconID				=	15248
,	kRemoteMessageFolderIconID				=	15250
,	kEmptyTrashMessageFolderIcon			=	270
,	kNonEmptyTrashMessageFolderIcon			=	270

	// Icons for message folders.  These need to be in a menu, so their numbers are 256+
	// Add 1 for the "open" state (except for trash)

,	kMailServerFolderIconID					=	15225
	// Icons for news groups/hosts
,	kNewsHostFolderIconID					=	15227
,	kNewsGroupFolderIconID					=	15227
	
};



static ResIDT gIconIDTable[] = {
//----------------------------------------------------------------------------------------
//NAME							READ											UNREAD		
//					LOCAL					ONLINE					LOCAL					ONLINE
//			DEFAULT		SPECIAL		DEFAULT 	SPECIAL		DEFAULT		SPECIAL		DEFAULT		SPECIAL
//----------------------------------------------------------------------------------------
/* Folder*/	15238	,	15238	,	15250	,	15250	,	15238	,	15394	,	15250	,	15394	,
/* Inbox */	15240	,	15240	,	15240	,	15240	,	15242	,	15242	,	15242	,	15242	,
/* Outbox*/	15244	,	15244	,	15244	,	15244	,	15244	,	15244	,	15244	,	15244	,
/* Sent */	15246	,	15246	,	15246	,	15246	,	15246	,	15246	,	15246	,	15246	,
/* Drafts*/	15248	,	15248	,	15248	,	15248	,	15248	,	15248	,	15248	,	15248	,
/* Trash */	15252	,	15252	,	15252	,	15252	,	15252	,	15252	,	15252	,	15252	,
/* Newsgrp*/	0	,		0	,	15231	,	15233	,		0	,		0	,	15231	,	15232	
};

enum {
	kSpecial		= 1 // offset
,	kOnline			= 2 // offset
,	kNewMessages			= 4	// offset
,	kKindsPerRow	= 8
,	kIconBaseIx		= 0				   // SPECIAL = Got new
,	kInboxIx		= 1 * kKindsPerRow // SPECIAL = Got new
,	kOutboxIx		= 2 * kKindsPerRow // SPECIAL = OCCUPIED
,	kSentIx			= 3 * kKindsPerRow
,	kDraftsIx		= 4 * kKindsPerRow
,	kTrashIx		= 5 * kKindsPerRow // SPECIAL = OCCUPIED
,	kNewsGroupIx	= 6 * kKindsPerRow // SPECIAL = SUBSCRIBED
};

//----------------------------------------------------------------------------------------
ResIDT CMessageFolder::GetIconID() const
// To do: deal with the "Open" state, the "read" state, etc.
//----------------------------------------------------------------------------------------
{
	UInt32 folderFlags = this->GetFolderType();
	if (GetLevel() == kRootLevel)
	{
		if ((folderFlags & MSG_FOLDER_FLAG_NEWS_HOST))
			return kNewsHostFolderIconID; // li'l computer
		return kMailServerFolderIconID; // li'l computer
	}
	short iconIndex = kIconBaseIx;
	switch (folderFlags & ~(MSG_FOLDER_FLAG_MAIL | MSG_FOLDER_FLAG_IMAPBOX))
	{	
		case MSG_FOLDER_FLAG_NEWSGROUP:
			iconIndex = kNewsGroupIx + kOnline; // always online!
			if (this->IsSubscribedNewsgroup())
				iconIndex += kSpecial; // special = subscribed
			break;
		case MSG_FOLDER_FLAG_TRASH:
			iconIndex = kTrashIx;
			if (this->CountMessages() != 0)
				iconIndex += kSpecial; // special = nonempty
			break;
		case MSG_FOLDER_FLAG_SENTMAIL:
			iconIndex = kSentIx;
			break;
		case MSG_FOLDER_FLAG_TEMPLATES:
		case MSG_FOLDER_FLAG_DRAFTS:
			iconIndex = kDraftsIx;
			break;
		case MSG_FOLDER_FLAG_QUEUE:
			iconIndex = kOutboxIx;
			if (this->CountMessages() != 0)
				iconIndex += kSpecial; // special = nonempty
			break;
		case MSG_FOLDER_FLAG_INBOX:
			iconIndex = kInboxIx;
			if (this->HasNewMessages())
				iconIndex += kSpecial; // special = got new messages
			break;
		default:
			iconIndex = kIconBaseIx;
			if (this->HasNewMessages())
				iconIndex += kSpecial; // special = got new messages
	}
	if (this->HasNewMessages() )
		iconIndex += kNewMessages;
	if ((folderFlags & MSG_FOLDER_FLAG_IMAPBOX) != 0)
		iconIndex += kOnline;
	return gIconIDTable[iconIndex];
} // CMessageFolder::GetIconID


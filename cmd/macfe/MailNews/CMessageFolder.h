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



#pragma once

enum 
{	kRootLevel = 1			// news host, local mail etc.
,	kSpecialFolderLevel = 2	// inbox, sent, drafts...
};

#include <LTableView.h>		// for TableIndexT
#include "msgcom.h"

struct CCachedFolderLine;	// forward

//========================================================================================
class CMessageFolder
//========================================================================================
{
public:
									CMessageFolder(TableIndexT inRow, MSG_Pane* inFolderPane);	
									// In the next constructor (and similar functions that
									// take a MSG_FolderInfo*, there is a MSG_Pane* param
									// which is optional.  Supplying the pane will allow
									// the ELIDED flag to work properly, because it is
									// a property of the "view" and not an intrinsic property
									// of the folder itself.
									CMessageFolder(
										const MSG_FolderInfo* id = nil,
										MSG_Pane* inFolderPane = nil);
											// inFolderPane can be nil, as can id.
									CMessageFolder(const CMessageFolder& inFolder);
									~CMessageFolder();
									
	void							operator=(const CMessageFolder& other);
	Boolean							operator==(const CMessageFolder& other) const;
									operator MSG_FolderInfo*() const { return GetFolderInfo(); }
	void							SetFolderInfo(const MSG_FolderInfo* id, MSG_Pane* inFolderPane = nil);
											// inFolderPane can be nil, as can id.
	void							FolderInfoChanged(MSG_Pane* inFolderPane = nil);
	void							FolderLevelChanged(MSG_Pane* inFolderPane = nil);
										// calls FolderInfoChanged on children, too.
										// inFolderPane can be nil.
	UInt32							GetFolderType() const;
	const char*						GetName() const;
	const char*						GetPrettyName() const;
	UInt32							GetLevel() const;
	SInt32							CountMessages() const; // neg if unknown
	SInt32							CountUnseen() const; // neg if unknown
	SInt32							CountDeepUnseen() const; // neg if unknown
	UInt32							CountSubFolders() const;
	UInt32							GetDeletedBytes() const;
	Boolean							IsOpen() const;
	MSG_ViewIndex					GetIndex() const;
	MSG_FolderInfo*					GetFolderInfo() const;
	const MSG_FolderLine*			GetFolderLine() const;
	MSG_Pane*						GetThreadPane() const; // Returns a threadpane viewing this.
	static MSG_Pane*				GetFolderPane();
	// Flag property accessors:
	UInt32							GetFolderFlags() const;
	UInt32							GetFolderPrefFlags() const;
	Boolean							HasNewMessages() const;
	Boolean							ContainsCategories() const;
	Boolean							IsInbox() const;
	Boolean							IsTrash() const;
	Boolean							IsMailServer() const;
	Boolean							IsMailFolder() const;
	Boolean							IsLocalMailFolder() const;
	Boolean							IsIMAPMailFolder() const;
	Boolean							IsNewsgroup() const;
	Boolean							IsNewsHost() const;
	Boolean							CanContainThreads() const;
	Boolean							IsSubscribedNewsgroup() const;
	ResIDT							GetIconID() const;
// Data
// NOTE WELL: This class is currently 4 bytes.  Many users rely on its being light weight,
//				and it is passed around by value, not by reference.  Note that the copy
//				constructor does the right thing with reference counting.
//				So BE VERY RELUCTANT TO ADD NEW DATA MEMBERS!
protected:
	CCachedFolderLine*				mCachedFolderLine;
}; // class CMessageFolder

//========================================================================================
class CCachedFolderLine
//========================================================================================
{
private:
								CCachedFolderLine(MSG_Pane* inFolderPane, MSG_FolderInfo* id);
									// inFolderPane can be nil.
								~CCachedFolderLine();
	static CCachedFolderLine*	FindFolderFor(MSG_FolderInfo* id);
		// doesn't create
public:
	void						AddUser(CMessageFolder*)	{ mRefCount++; }
	void						RemoveUser(CMessageFolder*) { if (--mRefCount <= 0) delete this; }
	void						FolderInfoChanged(MSG_Pane* inFolderPane);
									// inFolderPane can be nil.
	static void					FolderInfoChanged(MSG_Pane* inFolderPane, MSG_FolderInfo* inID);
									// inFolderPane can be nil.
	static CCachedFolderLine*	GetFolderFor(MSG_Pane* inFolderPane, MSG_FolderInfo* id);
									// creates if nec. inFolderPane can be nil.
	static CCachedFolderLine*	GetFolderFor(MSG_Pane* inFolderPane, MSG_ViewIndex inIndex);
									// creates if nec.inFolderPane IS REQUIRED.


//-----
// Data
//-----
public:
	MSG_FolderLine				mFolderLine;
private:
	Int32						mRefCount;
	static LArray*				sLineList;
}; // class CCachedFolderLine

#define kMessageFolderTypeMask	(	MSG_FOLDER_FLAG_NEWSGROUP | \
									MSG_FOLDER_FLAG_NEWS_HOST | \
									MSG_FOLDER_FLAG_MAIL | \
								MSG_FOLDER_FLAG_TRASH | MSG_FOLDER_FLAG_SENTMAIL | \
								MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE | \
								MSG_FOLDER_FLAG_TEMPLATES | MSG_FOLDER_FLAG_PERSONAL_SHARED | \
								MSG_FOLDER_FLAG_IMAP_OTHER_USER | MSG_FOLDER_FLAG_IMAP_PUBLIC | \
								MSG_FOLDER_FLAG_INBOX | MSG_FOLDER_FLAG_IMAPBOX)

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::operator==(const CMessageFolder& other) const
//----------------------------------------------------------------------------------------
{
	return GetFolderInfo() == other.GetFolderInfo();
}

//----------------------------------------------------------------------------------------
inline UInt32 CMessageFolder::GetFolderPrefFlags() const
//----------------------------------------------------------------------------------------
{
	return ::MSG_GetFolderPrefFlags(GetFolderInfo());
}

//----------------------------------------------------------------------------------------
inline const MSG_FolderLine* CMessageFolder::GetFolderLine() const
//----------------------------------------------------------------------------------------
{
	return &mCachedFolderLine->mFolderLine;
}

//----------------------------------------------------------------------------------------
inline UInt32 CMessageFolder::GetFolderType() const
//----------------------------------------------------------------------------------------
{	
	return (mCachedFolderLine->mFolderLine.flags & kMessageFolderTypeMask);
}

//----------------------------------------------------------------------------------------
inline const char* CMessageFolder::GetName() const
//----------------------------------------------------------------------------------------
{
	return mCachedFolderLine->mFolderLine.name;
}

//----------------------------------------------------------------------------------------
inline const char* CMessageFolder::GetPrettyName() const
//----------------------------------------------------------------------------------------
{
	return mCachedFolderLine->mFolderLine.prettyName;
}

//----------------------------------------------------------------------------------------
inline MSG_ViewIndex CMessageFolder::GetIndex() const
//----------------------------------------------------------------------------------------
{
	Assert_(false); // this is probably a bad function, now that multiple f. panes exist.
	return MSG_GetFolderIndex(GetFolderPane(), GetFolderInfo());
}

//----------------------------------------------------------------------------------------
inline UInt32 CMessageFolder::GetFolderFlags() const
//----------------------------------------------------------------------------------------
{
	return mCachedFolderLine->mFolderLine.flags;
}

//----------------------------------------------------------------------------------------
inline UInt32 CMessageFolder::GetLevel() const
//----------------------------------------------------------------------------------------
{
	return mCachedFolderLine->mFolderLine.level;
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::CanContainThreads() const
//----------------------------------------------------------------------------------------
{
	return GetLevel() > kRootLevel;
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsSubscribedNewsgroup() const
//----------------------------------------------------------------------------------------
{
#define SUBSCRIBED_NEWSGROUP (MSG_FOLDER_FLAG_SUBSCRIBED | MSG_FOLDER_FLAG_NEWSGROUP)
	return ((GetFolderFlags() & SUBSCRIBED_NEWSGROUP) == SUBSCRIBED_NEWSGROUP );
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::HasNewMessages() const
//----------------------------------------------------------------------------------------
{
	return ((GetFolderFlags() & MSG_FOLDER_FLAG_GOT_NEW) != 0);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsNewsgroup() const
//----------------------------------------------------------------------------------------
{
	return ((GetFolderFlags() & MSG_FOLDER_FLAG_NEWSGROUP) != 0);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsMailServer() const
//----------------------------------------------------------------------------------------
{
	return ((GetFolderFlags() & MSG_FOLDER_FLAG_MAIL) == MSG_FOLDER_FLAG_MAIL
		&& GetLevel() == kRootLevel);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsMailFolder() const
//----------------------------------------------------------------------------------------
{
	return ((GetFolderFlags() & MSG_FOLDER_FLAG_MAIL) == MSG_FOLDER_FLAG_MAIL);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsIMAPMailFolder() const
//----------------------------------------------------------------------------------------
{
#define IMAP_FOLDER	(MSG_FOLDER_FLAG_IMAPBOX | MSG_FOLDER_FLAG_MAIL)
	return ((GetFolderFlags() & IMAP_FOLDER) == IMAP_FOLDER);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsLocalMailFolder() const
//----------------------------------------------------------------------------------------
{
	// folder bit set and imap bit clear.
	return ((GetFolderFlags() & IMAP_FOLDER) == MSG_FOLDER_FLAG_MAIL);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsNewsHost() const
//----------------------------------------------------------------------------------------
{
	return ((GetFolderFlags() & MSG_FOLDER_FLAG_NEWS_HOST) != 0);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsInbox() const
//----------------------------------------------------------------------------------------
{
	return ((GetFolderFlags() & MSG_FOLDER_FLAG_INBOX) != 0);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsTrash() const
//----------------------------------------------------------------------------------------
{
	return ((GetFolderFlags() & MSG_FOLDER_FLAG_TRASH) != 0);
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::ContainsCategories() const
//----------------------------------------------------------------------------------------
{
#define NEWSGROUP_WITH_CATEGORIES (MSG_FOLDER_FLAG_CAT_CONTAINER | MSG_FOLDER_FLAG_NEWSGROUP)
	return ((GetFolderFlags() & NEWSGROUP_WITH_CATEGORIES) == NEWSGROUP_WITH_CATEGORIES );
}

//----------------------------------------------------------------------------------------
inline Int32 CMessageFolder::CountUnseen() const
//----------------------------------------------------------------------------------------
{
	return mCachedFolderLine->mFolderLine.unseen;
}

//----------------------------------------------------------------------------------------
inline Int32 CMessageFolder::CountDeepUnseen() const
//----------------------------------------------------------------------------------------
{
	return mCachedFolderLine->mFolderLine.deepUnseen;
}

//----------------------------------------------------------------------------------------
inline UInt32 CMessageFolder::CountSubFolders() const
//----------------------------------------------------------------------------------------
{
	return mCachedFolderLine->mFolderLine.numChildren;
}

//----------------------------------------------------------------------------------------
inline Boolean CMessageFolder::IsOpen() const
//----------------------------------------------------------------------------------------
{
	return ((mCachedFolderLine->mFolderLine.flags & MSG_FOLDER_FLAG_ELIDED) == 0);
} // CMessageFolder::IsOpen

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

#include "msg.h"
#include "msgdb.h"
#include "msgdbvw.h"
#include "thrdbvw.h"
#include "dberror.h"
#include "vwerror.h"
#include "newsdb.h"
#include "maildb.h"
#include "thrhead.h"
#include "grpinfo.h"
#include "chngntfy.h"
#include "msgmast.h"
#include "thrnewvw.h"
#include "xpgetstr.h"
#include "addrbook.h"
#include "dirprefs.h"
#include "msglpane.h"
#include "xp_qsort.h"
#include "intl_csi.h"
#include "msgimap.h"

extern "C" {
extern int MK_MSG_FIRST_MSG;
extern int MK_MSG_NEXT_MSG;
extern int MK_MSG_PREV_MSG;
extern int MK_MSG_LAST_MSG;
extern int MK_MSG_FIRST_UNREAD;
extern int MK_MSG_NEXT_UNREAD;
extern int MK_MSG_PREV_UNREAD;
extern int MK_MSG_LAST_UNREAD;
extern int MK_MSG_READ_MORE;
extern int MK_MSG_NEXTUNREAD_THREAD;
extern int MK_MSG_NEXTUNREAD_GROUP;
extern int MK_MSG_FIRST_FLAGGED;
extern int MK_MSG_NEXT_FLAGGED;
extern int MK_MSG_PREV_FLAGGED;

}
#ifdef WINDOWS
#include "windowsx.h"
#else
#define GlobalAllocPtr(a,b) XP_ALLOC(b)
#define GlobalFreePtr(p) XP_FREE(p)
#endif

ViewChangeListener::ViewChangeListener(MessageDBView *view)
{
	m_dbView = view;
}

ViewChangeListener::~ViewChangeListener()
{
}

void ViewChangeListener::OnViewChange(MSG_ViewIndex startIndex, 
	    int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener * instigator) 
{
	// propogate change to views' listeners
	// if we're not the instigator, update flags if this key is in our view
	if (instigator != &m_dbView->m_changeListener)
	{
		m_dbView->NotifyViewChangeAll(startIndex, numChanged, changeType, 
									instigator);
	}
}

void ViewChangeListener::OnViewStartChange(MSG_ViewIndex startIndex, 
	    int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener * instigator) 
{
	// propogate change to views' listeners
	// if we're not the instigator, update flags if this key is in our view
	if (instigator != &m_dbView->m_changeListener)
	{
		m_dbView->NotifyViewStartChangeAll(startIndex, numChanged, changeType, 
									instigator);
	}
}
void ViewChangeListener::OnViewEndChange(MSG_ViewIndex startIndex, 
	    int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener * instigator) 
{
	// propogate change to views' listeners
	// if we're not the instigator, update flags if this key is in our view
	if (instigator != &m_dbView->m_changeListener)
	{
		m_dbView->NotifyViewEndChangeAll(startIndex, numChanged, changeType, 
									instigator);
	}
}

void ViewChangeListener::OnKeyChange(MessageKey keyChanged, int32 flags, 
										ChangeListener *instigator) 
{
	// if we're not the instigator, update flags if this key is in our view
	if (instigator != &m_dbView->m_changeListener)
	{
		if (flags & kAdded) // message just downloaded
		{
			m_dbView->OnNewHeader(keyChanged, FALSE);
		}
		else
		{
			MSG_ViewIndex index = m_dbView->FindViewIndex(keyChanged);
			if (index != kViewIndexNone)
			{
				char extraFlag;

				m_dbView->SetExtraFlagsFromDBFlags(flags, index);
				// tell the view the extra flag changed, so it can
				// update the previous view, if any.
				if (m_dbView->GetExtraFlag(index, &extraFlag) == eSUCCESS)
					m_dbView->OnExtraFlagChanged(index, extraFlag);
				if (flags & (kExpired|kExpunged))
					m_dbView->DeleteMsgByIndex(index, FALSE);
				else
					m_dbView->NoteChange(index, 1, MSG_NotifyChanged);
			}
			else
			{
				MSG_ViewIndex threadIndex = m_dbView->ThreadIndexOfMsg(keyChanged);
				// may need to fix thread counts
				if (threadIndex != MSG_VIEWINDEXNONE)
					m_dbView->NoteChange(threadIndex, 1, MSG_NotifyChanged);

			}
		}
	}
	// propogate change to views' listeners, even if we're not instigator
	m_dbView->NotifyKeyChangeAll(keyChanged, flags,	instigator);
}

void ViewChangeListener::OnAnnouncerGoingAway (ChangeAnnouncer * instigator)
{
	m_dbView->NotifyAnnouncerGoingAway(instigator);	// shout it to the world!
	m_dbView->m_messageDB->RemoveListener(this);
	m_dbView->m_messageDB = NULL;
}

/*static*/ uint32 MessageDBView::m_publicEquivOfExtraFlags;

MessageDBView::MessageDBView() : m_changeListener(this)
{
	m_messageDB = NULL;
	m_refCount = 1;
	m_sortValid = TRUE;
	m_sortOrder = SortTypeNone;
	m_viewFlags = (ViewFlags) 0;
	if (m_publicEquivOfExtraFlags == 0)
	{
		// first, find out what the 32 bit public equivalent of the extra flags is.
		CopyExtraFlagsToDBFlags((char) 0xFF, &m_publicEquivOfExtraFlags);
		MessageDB::ConvertDBFlagsToPublicFlags(&m_publicEquivOfExtraFlags);
	}
}

MessageDBView::~MessageDBView()
{
	NotifyAnnouncerGoingAway(this);
	CacheRemove ();
	XP_ASSERT(m_messageDB == NULL); // should be NULL if no errors closing the DB
}
// static method which given a URL string returns a view on that URL.
// For example, if url = "news:secnews-alt.music.alternative", and viewType = ViewOnlyThreadsWithNew
// We will return a threaded NewsDBView on the newsgroup alt.music.alternative on
// the host secnews with only threads with new.
MsgERR MessageDBView::OpenURL(const char * url, MSG_Master* master,
							  ViewType viewType, MessageDBView **view, XP_Bool openInForeground)
{
	MailDB	*mailDB = NULL;
	NewsGroupDB *newsDB = NULL;
	MessageDBView *retView = NULL;
	MsgERR err = eSUCCESS;
	*view = NULL;
	const char *startFolder;
	char *endFolder = NULL;
	char *justFolder = NULL;

	int urlType = NET_URL_Type(url);

	switch (urlType)
	{
		case NEWS_TYPE_URL:
		// news:alt.music.alternative - news group
		// news:4agiou%24g7n@news.utdallas.edu	  - message-id
			err = NewsGroupDB::Open(url, master, &newsDB); 
			if (newsDB != NULL && err == eSUCCESS)
			{
				if (viewType == ViewAny)
					viewType = newsDB->GetViewType();	// use last opened view type.

				// allow this view opening to run in the background. We should do this
				// for mail and imap too, but that involves reworking msgtpane.cpp
				// to deal with eBuildViewInBackground.
				int32 numHeadersInDB = newsDB->GetDBFolderInfo()->GetNumMessages();
				MSG_FolderInfoNews *newsFolder = newsDB->GetFolderInfoNews();
				if (!openInForeground)	// if caller doesn't care
					openInForeground = (numHeadersInDB < 1000);
				// always open category containers in background, so we can check for new categories.
				err = OpenViewOnDB(newsDB, viewType, &retView, openInForeground);
				newsDB->SetViewType(viewType);	// remember this view type.
				newsDB->Close(); // always close - above will addref.

				*view = retView;
				if (newsDB == NULL || retView == NULL)
					return err;
			}
			break;
		case IMAP_TYPE_URL:
			{
				switch (viewType)
				{
					case ViewWatchedThreadsWithNew:
					case ViewOnlyThreadsWithNew:
					case ViewAllThreads:
					case ViewOnlyNewHeaders:
					case ViewAny:
					case ViewCacheless:
						{
							MailDB *mailDB = NULL;
							// strip off id info to get just folder path
							startFolder = url + strlen("IMAP:");
							char *host = NET_ParseURL (url, GET_HOST_PART);
							char *owner = NET_ParseURL (url, GET_USERNAME_PART);
							char *path = NET_ParseURL (url, GET_PATH_PART);
							if (!host || !path || !owner)
								return eOUT_OF_MEMORY;
							MSG_IMAPFolderInfoMail *folder = master->FindImapMailFolder(host, path + 1, owner, FALSE);
							FREEIF(host);
							FREEIF(owner);
							FREEIF(path);
							if (!folder)
								return eOUT_OF_MEMORY;	// ### dmb - what kind of error is this?
												
							XP_Bool dbWasCreated=FALSE;
							err = ImapMailDB::Open(folder->GetPathname(), TRUE , &mailDB, master,
												   &dbWasCreated); 
							if (mailDB != NULL && err == eSUCCESS)
							{
								if (viewType == ViewAny)
									viewType = mailDB->GetViewType();	// use last opened view type.

								err = OpenViewOnDB(mailDB, viewType, &retView, TRUE);
								mailDB->SetViewType(viewType);	// remember this view type.
								mailDB->Close(); // always close - above will addref.

								if (err != eSUCCESS)
									return err;

								*view = retView;
								if (mailDB == NULL || retView == NULL)
									return err;
							}
							
							if (justFolder)
								XP_FREE (justFolder);
						}
						break;
					case ViewCustom:
						{
							*view = new ThreadDBView(viewType);
							if (!*view)
								err = eOUT_OF_MEMORY;
							else
								err = eSUCCESS;
						}
						break;
					
				}
			}
			break;
		case MAILBOX_TYPE_URL:
			// strip off id info to get just folder path
			startFolder = url + strlen("mailbox:");
			justFolder = XP_STRDUP (startFolder);
			if (!justFolder)
				return eOUT_OF_MEMORY;
			endFolder =  XP_STRSTR (justFolder, "?id=");
			if (endFolder)
				endFolder[0] = '\0'; 
				
			err = MailDB::Open(justFolder, FALSE /* create? */, &mailDB); 
			if (mailDB != NULL && err == eSUCCESS)
			{
				retView = CacheLookup (mailDB, viewType);
				if (retView)
				{
					retView->AddReference ();
					mailDB->Close(); // adjust ref count down.
				}
				else
				{
					if (viewType == ViewAny)
						viewType = mailDB->GetViewType();	// use last opened view type.
					int32 numHeadersInDB = mailDB->GetDBFolderInfo()->GetNumMessages();
					if (!openInForeground)	// if caller doesn't care
						openInForeground = (numHeadersInDB < 1000);
					// try opening large mail dbs in background
					err = OpenViewOnDB(mailDB, viewType, &retView, openInForeground);
					mailDB->SetViewType(viewType);	// remember this view type.
					mailDB->Close(); // always close - above will addref.
					if (retView == NULL)
					{
						mailDB->Close();
						err = eOUT_OF_MEMORY;
					}
				}
				*view = retView;
				if (mailDB == NULL || retView == NULL)
					return err;
			}
			if (justFolder)
				XP_FREE (justFolder);
			break;
		default:
			return eBAD_URL;
	}
	
	return err;
}

// static method which creates and opens a view of the passed type on the already open db
MsgERR MessageDBView::OpenViewOnDB(MessageDB *msgDB, ViewType viewType, MessageDBView ** pRetView, XP_Bool runInForeground /* = TRUE */)
{
	MsgERR err = eSUCCESS;

	*pRetView = CacheLookup (msgDB, viewType);
	if (*pRetView)
		(*pRetView)->AddReference ();
	else
	{
		switch (viewType)
		{
		case ViewWatchedThreadsWithNew:
			// this can just filter out non-watched threads...
		case ViewOnlyThreadsWithNew:
			*pRetView = new ThreadsWithNewView(viewType);
			break;
		case ViewOnlyNewHeaders:
		case ViewAny:
		case ViewAllThreads:
			*pRetView = new ThreadDBView(viewType);
			break;
		case ViewCustom:
			*pRetView = new ThreadDBView(viewType);
			break;
		case ViewCacheless:
			*pRetView = new CachelessView(viewType);
		default:
			err = eInvalidViewType;
			break;
		}
		if (*pRetView != NULL)
		{
			err = (*pRetView)->Open(msgDB, viewType, NULL, runInForeground);
			msgDB->AddUseCount();	// add view as user of db.
		}
	}
	return err;
}


MsgERR MessageDBView::Open(MessageDB *messageDB, ViewType viewType, 
						   uint32* /*pCount*/, XP_Bool /* runInForeground = TRUE */)
{
	m_messageDB = messageDB;
	m_messageDB->AddListener(&m_changeListener);
	if (viewType == ViewAny)
		viewType = ViewAllThreads;
	m_viewType = viewType;
	if (messageDB && messageDB->GetDBFolderInfo()->GetFlags() & MSG_FOLDER_PREF_SHOWIGNORED)
		m_viewFlags = (ViewFlags) ((int32) kShowIgnored | (int32) m_viewFlags);
	if (messageDB && messageDB->GetDBFolderInfo() 
		&& (viewType == ViewOnlyNewHeaders))
		m_viewFlags = (ViewFlags) ((int32) kUnreadOnly | (int32) m_viewFlags);

	CacheAdd ();
	return eSUCCESS;
}

MsgERR	MessageDBView::Close()
{
	//if it's zero or negative, we should already have been deleted
	XP_ASSERT(m_refCount > 0); 

	MsgERR	err = eSUCCESS;

	// opening a view, even from cache, always adds a ref count to db, 
	// so we should always close it.

	if (!--m_refCount) 
	{
		if (m_messageDB != NULL) 
		{
			m_messageDB->RemoveListener(&m_changeListener);
			err = m_messageDB->Close(); 

			m_messageDB = NULL;
		}
		delete this;
	}
	else if (m_messageDB != NULL)
	{
		err = m_messageDB->Close(); 
	}
	return err;
}

MsgERR MessageDBView::Init(uint32 * /*pCount*/, XP_Bool /*runInForeground = TRUE*/)
{
	// MessageDBView is pure virtual in spirit. Subclasses need to override this.
	XP_ASSERT(FALSE);
	return eSUCCESS;
}

MsgERR		MessageDBView::InitSort(SortType /*sortType*/, SortOrder /*sortOrder*/)
{
	return eSUCCESS;
}

int32 MessageDBView::AddKeys(MessageKey * /*pOutput*/, int32 * /*pFlags*/, char * /*pLevels*/, SortType /*sortType*/, int /*numListed*/)
{
	XP_ASSERT(FALSE);
	return 0;
}

XP_Bool	MessageDBView::GetShowingIgnored()
{
	return m_viewFlags & kShowIgnored;
}

void	MessageDBView::SetShowingIgnored(XP_Bool bShowIgnored)
{
	if (bShowIgnored)
		m_viewFlags |= kShowIgnored;
	else
		m_viewFlags &= ~kShowIgnored;
}

MsgERR	MessageDBView::AddHdr(DBMessageHdr *msgHdr)
{
	char	flags = 0;
#ifdef DEBUG_bienvenu
	XP_ASSERT((int) m_idArray.GetSize() == m_flags.GetSize() && (int) m_idArray.GetSize() == m_levels.GetSize());
#endif
	if (msgHdr->GetFlags() & kIgnored && !GetShowingIgnored())
		return eSUCCESS;

	CopyDBFlagsToExtraFlags(msgHdr->GetFlags(), &flags);
	if (msgHdr->GetArticleNum() == msgHdr->GetThreadId())
		flags |= kIsThread;
	MSG_ViewIndex insertIndex = GetInsertIndex(msgHdr);
	if (insertIndex == MSG_VIEWINDEXNONE)
	{
		// if unreadonly, level is 0 because we must be the only msg in the thread.
		char levelToAdd = (m_viewFlags & kUnreadOnly) ? 0 : msgHdr->GetLevel();

		if (m_sortOrder == SortTypeAscending)
		{
			m_idArray.Add(msgHdr->GetMessageKey());
			m_flags.Add(flags);
			m_levels.Add(levelToAdd);
			NoteChange(m_idArray.GetSize() - 1, 1, MSG_NotifyInsertOrDelete);
		}
		else
		{
			m_idArray.InsertAt(0, msgHdr->GetMessageKey());
			m_flags.InsertAt(0, flags);
			m_levels.InsertAt(0, levelToAdd);
			NoteChange(0, 1, MSG_NotifyInsertOrDelete);
		}
		m_sortValid = FALSE;
	}
	else
	{
		m_idArray.InsertAt(insertIndex, msgHdr->GetMessageKey());
		m_flags.InsertAt(insertIndex, flags);
		m_levels.InsertAt(insertIndex, (m_sortType == SortByThread) ? 0 : msgHdr->GetLevel());
		NoteChange(insertIndex, 1, MSG_NotifyInsertOrDelete);
	}
	OnHeaderAddedOrDeleted();
	return eSUCCESS;
}

MsgERR	MessageDBView::InsertHdrAt(DBMessageHdr *msgHdr, MSG_ViewIndex insertIndex)
{
	char	flags = 0;
	CopyDBFlagsToExtraFlags(msgHdr->GetFlags(), &flags);

	NoteStartChange(insertIndex, 1, MSG_NotifyChanged);
	m_idArray.SetAt(insertIndex, msgHdr->GetMessageKey());
	m_flags.SetAt(insertIndex, flags);
	m_levels.SetAt(insertIndex, (m_sortType == SortByThread) ? 0 : msgHdr->GetLevel());
	NoteEndChange(insertIndex, 1, MSG_NotifyChanged);
	OnHeaderAddedOrDeleted();
	return eSUCCESS;
}

MsgERR MessageDBView::OnNewHeader(MessageKey newKey, XP_Bool /*ensureListed*/)
{
	MsgERR	err = eID_NOT_FOUND;;
	// views can override this behaviour, which is to append to view.
	// This is the mail behaviour, but threaded views might want
	// to insert in order...
	DBMessageHdr *msgHdr = m_messageDB->GetDBHdrForKey(newKey);
	if (msgHdr != NULL)
	{
		err = AddHdr(msgHdr);
		delete msgHdr;
	}
	return err;
}

XP_Bool MessageDBView::WantsThisThread(DBThreadMessageHdr * /*threadHdr*/)
{
	return TRUE;
}

MsgERR		MessageDBView::FinishedAddingHeaders()
{
	return eSUCCESS;
}

// make sure the passed key is "in" the view (e.g., for a threaded sort, this
// may just mean the parent thread is in the view).
void	MessageDBView::EnsureListed(MessageKey key)
{
	if (key != MSG_MESSAGEKEYNONE)
	{
		// find the key, expanding if neccessary.
		MSG_ViewIndex index = FindKey(key, TRUE);
		if (index == MSG_VIEWINDEXNONE)	// tell the view about it.
			OnNewHeader(key, TRUE);
	}
}

MessageKey MessageDBView::GetAt(MSG_ViewIndex index) 
{
	if (index >= m_idArray.GetSize() || index == MSG_VIEWINDEXNONE)
		return kIdNone;
	else
		return(m_idArray.GetAt(index));
}

MSG_ViewIndex	MessageDBView::FindKey(MessageKey key, XP_Bool expand)
{
	MSG_ViewIndex retIndex = MSG_VIEWINDEXNONE;
	retIndex = (MSG_ViewIndex) (m_idArray.FindIndex(key));
	if (key != MSG_MESSAGEKEYNONE && retIndex == MSG_VIEWINDEXNONE && expand && m_messageDB)
	{
		MessageKey threadKey = m_messageDB->GetKeyOfFirstMsgInThread(key);
		if (threadKey != kIdNone)
		{
			MSG_ViewIndex threadIndex = FindKey(threadKey, FALSE);
			if (threadIndex != MSG_VIEWINDEXNONE)
			{
				char flags = m_flags[threadIndex];
				if ((flags & kElided) && ExpandByIndex(threadIndex, NULL) == eSUCCESS)
					retIndex = FindKey(key, FALSE);
			}
		}
	}
	return retIndex;
}

typedef struct entryInfo
{
	MessageKey	id;
	char	bits;
} EntryInfo;  

typedef struct tagIdStr{
	EntryInfo	info;
	char		str[1];
} IdStr;

typedef struct tagIdStrPtr{
	EntryInfo	info;
	const char		*strPtr;
} IdStrPtr;

static int /* __cdecl */ FnSortIdStr(const void* pItem1, const void* pItem2)
{
	IdStr** p1 = (IdStr**)pItem1;
	IdStr** p2 = (IdStr**)pItem2;
	int retVal = XP_STRCMP((*p1)->str, (*p2)->str);	// used to be strcasecmp, but INTL sorting routine lower cases it.
	if (retVal != 0)
		return(retVal);
	if ((*p1)->info.id >= (*p2)->info.id)
		return(1);
	else
		return(-1);
}

static int /* __cdecl */ FnSortIdStrPtr(const void* pItem1, const void* pItem2)
{
	IdStrPtr** p1 = (IdStrPtr**)pItem1;
	IdStrPtr** p2 = (IdStrPtr**)pItem2;
	int retVal = XP_STRCMP((*p1)->strPtr, (*p2)->strPtr); // used to be strcasecmp, but INTL sorting routine lower cases it.
	if (retVal != 0)
		return(retVal);
	if ((*p1)->info.id >= (*p2)->info.id)
		return(1);
	else
		return(-1);
}


typedef struct tagIdWord{
	EntryInfo	info;
	uint16	word;
} IdWord;
static int /* __cdecl */ FnSortIdWord(const void* pItem1, const void* pItem2)
{
	IdWord** p1 = (IdWord**)pItem1;
	IdWord** p2 = (IdWord**)pItem2;
	long retVal = (*p1)->word - (*p2)->word;
	if (retVal > 0)
		return(1);
	else if (retVal < 0)
		return(-1);
	else if ((*p1)->info.id >= (*p2)->info.id)
		return(1);
	else
		return(-1);
}

typedef struct tagIdDWord{
	EntryInfo	info;
	uint32	dword;
} IdDWord;
static int /* __cdecl */ FnSortIdDWord(const void* pItem1, const void* pItem2)
{
	IdDWord** p1 = (IdDWord**)pItem1;
	IdDWord** p2 = (IdDWord**)pItem2;
	if ((*p1)->dword > (*p2)->dword)
		return(1);
	else if ((*p1)->dword < (*p2)->dword)
		return(-1);
	else if ((*p1)->info.id >= (*p2)->info.id)
		return(1);
	else
		return(-1);
}
MSG_ViewIndex MessageDBView::GetInsertIndex(DBMessageHdr *msgHdr)
{
	XP_Bool done = FALSE;
	XP_Bool withinOne = FALSE;
	MSG_ViewIndex retIndex = MSG_VIEWINDEXNONE;
	MSG_ViewIndex	tryIndex = GetSize() / 2;
	MSG_ViewIndex	newTryIndex;
	MSG_ViewIndex lowIndex = 0;
	MSG_ViewIndex highIndex = GetSize() - 1;
	IdDWord			dWordEntryInfo1, dWordEntryInfo2;
	IdStrPtr		strPtrInfo1, strPtrInfo2;

	if (GetSize() == 0)
		return 0;

	uint16	maxLen;
	int16 csid = (GetDB()->GetDBFolderInfo()->GetCSID() & ~CS_AUTO); 
	XPStringObj	field1Str;
	XPStringObj field2Str;
	eFieldType fieldType = GetFieldTypeAndLenForSort(m_sortType, &maxLen);
	const void	*pValue1, *pValue2;
	char *intlString1 = NULL;

	if (m_sortType == SortByThread)	// punt on threaded view for now.
		return retIndex;

	int (*comparisonFun) (const void *pItem1, const void *pItem2)=NULL;
	int retStatus = 0;
	switch (fieldType)
	{
		case kString:
			comparisonFun = FnSortIdStrPtr;
			strPtrInfo1.strPtr = intlString1 = GetStringField(msgHdr, m_sortType, csid, field1Str);
			strPtrInfo1.info.id = msgHdr->GetMessageKey();
			pValue1 = &strPtrInfo1;
			break;
		case kU16:
		case kU32:
			pValue1 = &dWordEntryInfo1;
			dWordEntryInfo1.dword = GetLongField(msgHdr, m_sortType);
			dWordEntryInfo1.info.id = msgHdr->GetMessageKey();
			comparisonFun = FnSortIdDWord;
			break;
		default:
			done = TRUE;
	}
	while (!done)
	{
		if (highIndex == lowIndex)
			break;
		MessageKey	messageKey = GetAt(tryIndex);
		DBMessageHdr *tryHdr = m_messageDB->GetDBHdrForKey(messageKey);
		char	*intlString2 = NULL;
		if (!tryHdr)
			break;
		if (fieldType == kString)
		{
			strPtrInfo2.strPtr = intlString2 = GetStringField(tryHdr, m_sortType, csid, field2Str);
			strPtrInfo2.info.id = messageKey;
			pValue2 = &strPtrInfo2;
		}
		else
		{
			dWordEntryInfo2.dword = GetLongField(tryHdr, m_sortType);
			dWordEntryInfo2.info.id = messageKey;
			pValue2 = &dWordEntryInfo2;
		}
		delete tryHdr;
		retStatus = (*comparisonFun)(&pValue1, &pValue2);
		FREEIF(intlString2);
		if (retStatus == 0)
			break;
		if (m_sortOrder == SortTypeDescending)	//switch retStatus based on sort order
			retStatus = (retStatus > 0) ? -1 : 1;

		if (retStatus < 0)
		{
			newTryIndex = tryIndex  - (tryIndex - lowIndex) / 2;
			if (newTryIndex == tryIndex)
			{
				if (!withinOne && newTryIndex > lowIndex)
				{
					newTryIndex--;
					withinOne = TRUE;
				}
			}
			highIndex = tryIndex;
		}
		else
		{
			newTryIndex = tryIndex + (highIndex - tryIndex) / 2;
			if (newTryIndex == tryIndex)
			{
				if (!withinOne && newTryIndex < highIndex)
				{
					withinOne = TRUE;
					newTryIndex++;
				}
				lowIndex = tryIndex;
			}
		}
		if (tryIndex == newTryIndex)
			break;
		else
			tryIndex = newTryIndex;
	}
	if (retStatus >= 0)
		retIndex = tryIndex + 1;
	else if (retStatus < 0)
		retIndex = tryIndex;

	FREEIF(intlString1);
	return retIndex;
}


MsgERR MessageDBView::ExternalSort(SortType sortType,
  XP_Bool sort_forward_p)
{
	SortOrder	sortOrder = (sort_forward_p) 
							? SortTypeAscending : SortTypeDescending;
	return Sort(sortType, sortOrder);
}

MsgERR	MessageDBView::Sort(SortType sortType, SortOrder sortOrder)
{
 	return SortInternal(sortType, sortOrder);
}

MsgERR MessageDBView::SortInternal(SortType sortType, SortOrder sortOrder)
{
	int arraySize;
	MsgERR 		err = eSUCCESS;
	XPByteArray *pBits = GetFlagsArray();
	XPPtrArray ptrs;
	XPStringObj	fieldStr;
	int16 csid = (GetDB()->GetDBFolderInfo()->GetCSID() & ~CS_AUTO); 

	arraySize = GetSize();
	if (sortType == m_sortType && m_sortValid)
	{
		if (sortOrder == m_sortOrder)
		{
			return eSUCCESS;
		}
		else
		{
			if (sortType != SortByThread)
			{
				// reverse the order
				ReverseSort();
				err = eSUCCESS;
			}
			else
			{
				err = ReverseThreads();
			}
			m_sortOrder = sortOrder;
			m_messageDB->SetSortInfo(sortType, sortOrder);
			return err;
		}
	}
	if (sortType == SortByThread)
		return eSUCCESS;

	uint16		maxLen;
	eFieldType	fieldType = GetFieldTypeAndLenForSort(sortType, &maxLen);
	
	int i;
	// This function uses GlobalAlloc because I don't want to fragment up out heap with these potentially large memory blocks
	// Also, freeing a heap block does not return the block to the system, but freeing a globalAlloc block does.
	IdStr** pPtrBase = (IdStr**)GlobalAllocPtr(GMEM_MOVEABLE, arraySize * sizeof(IdStr*));
	if (pPtrBase)
	{
		int	numSoFar = 0;
		// calc max possible size needed for all the rest
		uint32 maxSize = (uint32)(maxLen + sizeof(EntryInfo) + 1) * (uint32)(arraySize - numSoFar);
		uint32 maxBlockSize = (uint32) 0xf000L;
		uint32 allocSize = MIN(maxBlockSize, maxSize);
		char * pTemp = (char *)GlobalAllocPtr(GMEM_MOVEABLE, allocSize);
		char * pBase = pTemp;
		if (pTemp)
		{

			ptrs.Add(pTemp);			// keep track of this so we can free them all
			XP_Bool		more = TRUE;
					
			DBMessageHdr	*msgHdr = NULL;
			uint32			longValue;
			while (more && numSoFar < arraySize)
			{
				MessageKey thisKey = m_idArray.GetAt(numSoFar);
				if (sortType != SortById)
				{
					msgHdr = m_messageDB->GetDBHdrForKey(thisKey);
					if (msgHdr == NULL)
					{
						err = eID_NOT_FOUND;
						break;
					}
				}
				else
					msgHdr = NULL;
				// could be a problem here if the ones that appear here are different than the ones already in the array
				const char* pField;
				char	*intlString = NULL;
				int paddedFieldLen;
				int actualFieldLen;
				if (fieldType == kString)
				{
					pField = intlString = GetStringField(msgHdr, sortType, csid, fieldStr);
					actualFieldLen = (pField) ? strlen(pField) + 1 : 1;
					paddedFieldLen = actualFieldLen;
					int mod4 = actualFieldLen % 4;
					if (mod4 > 0)
						paddedFieldLen += 4 - mod4;
				}
				else
				{
					longValue =	(sortType == SortById) ? thisKey : GetLongField(msgHdr, sortType);
					pField = (const char *) &longValue;

					actualFieldLen = paddedFieldLen = maxLen;

				}
				// check to see if this entry fits into the block we have allocated so far
				// pTemp - pBase = the space we have used so far
				// sizeof(EntryInfo) + fieldLen = space we need for this entry
				// allocSize = size of the current block
				if ((uint32)(pTemp - pBase) + (uint32)sizeof(EntryInfo) + (uint32)paddedFieldLen >= allocSize)
				{
					maxSize = (uint32)(maxLen + sizeof(EntryInfo) + 1) * (uint32)(arraySize - numSoFar);
					maxBlockSize = (uint32) 0xf000L;
					allocSize = MIN(maxBlockSize, maxSize);
					pTemp = (char*)GlobalAllocPtr(GMEM_MOVEABLE, allocSize);
					if (!pTemp)
					{
						err = eOUT_OF_MEMORY;
						break;
					}
					pBase = pTemp;
					ptrs.Add(pTemp);		// remember this pointer so we can free it later
				}
				// make sure there aren't more IDs than we allocated space for
				if (numSoFar >= arraySize) 
				{
					err = eOUT_OF_MEMORY;
					break;
				}
						
				// now store this entry away in the allocated memory
				pPtrBase[numSoFar] = (IdStr*)pTemp;
				EntryInfo *info = (EntryInfo *)  pTemp;
				info->id = thisKey;
				char bits = 0;
				bits = m_flags[numSoFar];
				info->bits = bits;
				pTemp += sizeof(EntryInfo);
				int32 bytesLeft = allocSize - (int32)(pTemp - pBase);
				int32 bytesToCopy = MIN(bytesLeft, actualFieldLen);
				if (pField)
				{
					memcpy((char *)pTemp, pField, bytesToCopy);
					if (bytesToCopy < actualFieldLen)
					{
#ifdef DEBUG_bienvenu
						XP_ASSERT(FALSE);	// wow, big block
#endif
						*(pTemp + bytesToCopy) = '\0';

					}
					FREEIF(intlString);	// free intl'ized string
				}
				else
					*pTemp = 0;
				pTemp += paddedFieldLen;
				if (msgHdr)
					delete msgHdr;
				++numSoFar;
			}

			if (err == eSUCCESS)
			{
				// now sort the array based on the appropriate type of comparison
				switch(fieldType)
				{
					case kString:
						XP_QSORT(pPtrBase, numSoFar, sizeof(IdStr*), FnSortIdStr);
						break;
					case kU16:
						XP_QSORT(pPtrBase, numSoFar, sizeof(IdWord*), FnSortIdWord);
						break;
					case kU32:
						XP_QSORT(pPtrBase, numSoFar, sizeof(IdDWord*), FnSortIdDWord);
						break;
					default:
						XP_ASSERT(FALSE);		// not supposed to get here
						break;
						
				}			
				// now puts the IDs into the array in proper order
				for (i = 0; i < numSoFar; i++)
				{
					m_idArray.SetAt(i, pPtrBase[i]->info.id);
					if (pBits != NULL)
						pBits->SetAt(i, pPtrBase[i]->info.bits);
				}
				m_sortType = sortType;
				m_sortOrder = sortOrder;
				if (sortOrder == SortTypeDescending)
				{
					ReverseSort();
				}
			}
		}
	}

	// free all the memory we allocated
	for (i = 0; i < ptrs.GetSize(); i++)
	{
		GlobalFreePtr(ptrs[i]);
	}	
	if (pPtrBase)
		GlobalFreePtr(pPtrBase);
	if (err == eSUCCESS)
	{
		m_sortValid = TRUE;
		m_messageDB->SetSortInfo(sortType, sortOrder);
	}
	return err;
}

MessageDBView::eFieldType MessageDBView::GetFieldTypeAndLenForSort(SortType sortType, uint16 *pMaxLen)
{
	eFieldType	fieldType;
	uint16		maxLen;

	switch (sortType)
	{
		case SortBySubject:
			fieldType = kString;
			maxLen = kMaxSubject;
			break;
		case SortByRecipient:
			fieldType = kString;
			maxLen = kMaxRecipient;
			break;
		case SortByAuthor:
			fieldType = kString;
			maxLen = kMaxAuthor;
			break;
		case SortByDate:
			fieldType = kU32;
			maxLen = sizeof(time_t);
			break;
		case SortByPriority:
			fieldType = kU32;	
			maxLen = sizeof(uint32);
			break;
		case SortByThread:
		case SortById:
		case SortBySize:
		case SortByFlagged:
		case SortByUnread:
		case SortByStatus:
			fieldType = kU32;
			maxLen = sizeof(uint32);
			break;
		default:
			XP_ASSERT(FALSE);
			return kString;
	}
	*pMaxLen = maxLen;
	return fieldType;
}

// helper routines for internal sort. XPStringObj is only currently used by
// recipients but we should change the rest so we aren't returning pointers
// to hash strings. This routine allocates a string, so the caller has to free it.
char *MessageDBView::GetStringField(DBMessageHdr *msgHdr, SortType sortType, int16 csid, XPStringObj &string)
{
	const char *pField;

	switch (sortType)
	{
	case SortBySubject:
		if (msgHdr->GetSubject(string, FALSE, m_messageDB->GetDB()))
			pField = string;
		else
			pField = "";
		break;
	case SortByRecipient:
		msgHdr->GetNameOfRecipient(string, 0, m_messageDB->GetDB());
		pField = string ;
		if (!pField)
			pField = "";
		break;
	case SortByAuthor:
		msgHdr->GetRFC822Author(string, m_messageDB->GetDB());
		pField = string ;
		if (!pField)
			pField = "";
		break;
	default:
//		XP_ASSERT(FALSE);
		return(0);
	}
	return INTL_DecodeMimePartIIAndCreateCollationKey(pField, csid, 0);
}

uint32		MessageDBView::GetStatusSortValue(DBMessageHdr *msgHdr)
{
	uint32 sortValue = 5;
	uint32 messageFlags = m_messageDB->GetStatusFlags(msgHdr);

	if (messageFlags & MSG_FLAG_NEW)	// happily, new by definition stands alone
		return 0;

#define MSG_STATUS_MASK (MSG_FLAG_REPLIED | MSG_FLAG_FORWARDED)
	switch (messageFlags & MSG_STATUS_MASK)
	{
	case MSG_FLAG_REPLIED:
		sortValue = 2;
		break;
	case MSG_FLAG_FORWARDED|MSG_FLAG_REPLIED:
		sortValue = 1;
		break;
	case MSG_FLAG_FORWARDED:
		sortValue = 3;
		break;
	}

	if (sortValue == 5)	// none of the above flags set
	{
		if (messageFlags & MSG_FLAG_READ)	// make read a visible status in winfe.
			sortValue = 4;
	}
	return sortValue;
}

uint32		MessageDBView::GetLongField(DBMessageHdr *msgHdr, SortType sortType)
{
	switch (sortType)
	{
	case SortByDate:
		return msgHdr->GetDate();
	case SortBySize:
		return msgHdr->GetMessageSize();
	case SortById:
		return msgHdr->GetMessageKey();
	case SortByPriority: // want highest priority to have lowest value
						// so ascending sort will have highest priority first.
		return MSG_HighestPriority - msgHdr->GetPriority();
	case SortByStatus:
		return GetStatusSortValue(msgHdr);
	case SortByFlagged:
		return !(msgHdr->GetFlags() & kMsgMarked);	//make flagged come out on top.
	case SortByUnread:
	{
		XP_Bool	isRead = FALSE;
		GetDB()->IsRead(msgHdr->GetMessageKey(), &isRead);
		return !isRead;	// make unread show up at top
	}
	default:
		XP_ASSERT(FALSE);
		return 0;
	}
}

void MessageDBView::ReverseSort(void)
{
	XPByteArray *pBits = GetFlagsArray();
	int num = GetSize();
	for (int j = 0; j < (num / 2); j++)
	{
		// go up half the array swapping values
		int end = num - j - 1;
		char bits;
		if (pBits != NULL)
		{
			bits = pBits->GetAt(j);
			pBits->SetAt(j, pBits->GetAt(end));
			pBits->SetAt(end, bits);
		}
		MessageKey tempID = m_idArray.GetAt(j);
		
		m_idArray.SetAt(j, m_idArray.GetAt(end));
		m_idArray.SetAt(end, tempID);
	}
}

// reversing threads involves reversing the threads but leaving the
// expanded messages ordered relative to the thread, so we
// make a copy of each array and copy them over.
MsgERR  MessageDBView::ReverseThreads()
{
	MsgERR	err = eSUCCESS;
	XPByteArray *newFlagArray = new XPByteArray;
	IDArray *newIdArray = new IDArray;
	XPByteArray *newLevelArray = new XPByteArray;
	int sourceIndex, destIndex;
	int viewSize = GetSize();

	if (newIdArray == NULL || newFlagArray == NULL)
	{
		err = eOUT_OF_MEMORY;
		goto CLEANUP;
	}
	newIdArray->SetSize(m_idArray.GetSize());
	newFlagArray->SetSize(m_flags.GetSize());
	newLevelArray->SetSize(m_levels.GetSize());

	for (sourceIndex = 0, destIndex = viewSize - 1; sourceIndex < viewSize;)
	{
		int endThread;	// find end of current thread.
		XP_Bool inExpandedThread = FALSE;
		for (endThread = sourceIndex; endThread < viewSize; endThread++)
		{
			char flags = m_flags[endThread];
			if (!inExpandedThread && (flags & (kIsThread|kHasChildren)) && !(flags & kElided))
				inExpandedThread = TRUE;
			else if (flags & kIsThread)
			{
				if (inExpandedThread)
					endThread--;
				break;
			}
		}

		if (endThread == viewSize)
			endThread--;
		int saveEndThread = endThread;
		while (endThread >= sourceIndex)
		{
			newIdArray->SetAt(destIndex, m_idArray.GetAt(endThread));
			newFlagArray->SetAt(destIndex, m_flags.GetAt(endThread));
			newLevelArray->SetAt(destIndex, m_levels.GetAt(endThread));
			endThread--;
			destIndex--;
		}
		sourceIndex = saveEndThread + 1;
	}
	// this copies the contents of both arrays - it would be cheaper to
	// just assign the new data ptrs to the old arrays and "forget" the new 
	// arrays' data ptrs, so they won't be freed when the arrays are deleted.
	m_idArray.RemoveAll();
	m_flags.RemoveAll();
	m_levels.RemoveAll();
	m_idArray.InsertAt(0, newIdArray);
	m_flags.InsertAt(0, newFlagArray);
	m_levels.InsertAt(0, newLevelArray);

CLEANUP:
	// if we swizzle data pointers for these arrays, this won't be right.
	if (newFlagArray != NULL)
		delete newFlagArray;
	if (newIdArray != NULL)
		delete newIdArray;
	if (newLevelArray != NULL)
		delete newLevelArray;

	return err;
}

MsgERR	MessageDBView::ListThreads(MessageKey * /*pMessageNums*/,
								   int /*numToList*/,
								   MessageHdrStruct * /*pOutput*/,
								   int * /*pNumListed*/)
{
	return eNYI;
}
MsgERR	MessageDBView::ListThreadsShort(MessageKey * pMessageNums,
										int numToList,
										MSG_MessageLine * pOutput,
										int * pNumListed)
{
	MsgERR err = eSUCCESS;

	XP_BZERO(pOutput, sizeof(*pOutput) * numToList);
	int i;
	for (i = 0; i < numToList && err == eSUCCESS; i++)
	{
		{
			err = m_messageDB->GetShortMessageHdr(pMessageNums[i], pOutput + i);
			if (err == eSUCCESS)
			{
				// force non-threaded view to be flat. 
				if (m_sortType != SortByThread)
				{
					(pOutput + i)->level = 0;
//					(pOutput + i)->flags &= ~(kHasChildren);  // Not used by FE.
					(pOutput + i)->numChildren = 0;
					(pOutput + i)->numNewChildren = 0;
				}
			}
		}
	}
	if (pNumListed != NULL)
		*pNumListed = i;

	return err;
}
// list the headers of the top-level thread ids 
MsgERR	MessageDBView::ListThreadIds(MessageKey * /*startMsg*/,
									 MessageKey * /*pOutput*/,
									 int /*numToList*/,
									 int * /*numListed*/)
{
	return eNYI;
}
MsgERR	MessageDBView::ListThreadIds(ListContext * /*context*/,
									 MessageKey * /*pOutput*/,
									 int /*numToList*/,
									 int * /*numListed*/)
{
	return eNYI;
}
// return the list header information for the documents in a thread.
MsgERR	MessageDBView::ListThread(MessageKey /*threadId*/,
								  MessageKey /*startMsg*/,
								  int /*numToList*/,
								  MessageHdrStruct * /*pOutput*/,
								  int * /*pNumListed*/)
{
	return eNYI;
}
MsgERR	MessageDBView::ListThreadShort(MessageKey /*threadId*/,
									   MessageKey /*startMsg*/,
									   int /*numToList*/,
									   MSG_MessageLine * /*pOutput*/,
									   int * /*pNumListed*/)
{
	return eNYI;
}

MsgERR	MessageDBView::ListShortMsgHdrByIndex(MSG_ViewIndex startIndex, int numToList, MSG_MessageLine *pOutput, int *pNumListed)
{
	MsgERR err = eSUCCESS;

	XP_BZERO(pOutput, sizeof(*pOutput) * numToList);
	int i;
	for (i = 0; i < numToList && err == eSUCCESS; i++)
	{
		if (i + startIndex < m_idArray.GetSize())
		{
			err = m_messageDB->GetShortMessageHdr(m_idArray[i + startIndex], pOutput + i);
			if (err == eSUCCESS)
			{
				char	extraFlag = m_flags[i + startIndex];

				CopyExtraFlagsToPublicFlags(extraFlag, &((pOutput + i)->flags));
				// force non-threaded view to be flat. Wish FE's would do this
				if (m_sortType != SortByThread)
				{
					(pOutput + i)->level = 0;
				}
				else
				{
					(pOutput + i)->level = m_levels[i + startIndex];
				}
				// m_levels is only valid in the thread sort - otherwise, use kIsThread flag,
				// Ideally, we'd always use kIsThread, but this is a safer fix for now.
				// It's probably not worth keeping level array valid for sorted views.
				if (m_levels[i + startIndex] == 0 && m_sortType == SortByThread || (m_sortType != SortByThread && extraFlag & kIsThread))
				{
					DBThreadMessageHdr *thread = m_messageDB->GetDBThreadHdrForThreadID((pOutput + i)->threadId);
					if (thread != NULL)
					{
						if (m_sortType == SortByThread)	// don't set child counts if not threaded
						{
							if (m_viewFlags & kUnreadOnly)
							{
								if (extraFlag & kElided)
								{
									(pOutput + i)->numChildren = thread->GetNumNewChildren() - 1;
									if (extraFlag & kIsRead)	// count top message if it's read.
										(pOutput + i)->numChildren++;
								}
								else
									(pOutput + i)->numChildren = CountExpandedThread(i + startIndex) -1;
							}
							else
								(pOutput + i)->numChildren = thread->GetNumChildren() - 1;
							if ((int16) ((pOutput + i)->numChildren) < 0)
								(pOutput + i)->numChildren = 0;
							(pOutput + i)->numNewChildren = thread->GetNumNewChildren();
						}
						if (thread->GetFlags() & kIgnored)
							(pOutput + i)->flags |= MSG_FLAG_IGNORED;
						else
							(pOutput + i)->flags &= ~MSG_FLAG_IGNORED;
						if (thread->GetFlags() & kWatched)
							(pOutput + i)->flags |= MSG_FLAG_WATCHED;
						else
							(pOutput + i)->flags &= ~MSG_FLAG_WATCHED;
						delete thread;
					}
				}
			}
		}
		else
		{
			err = eID_NOT_FOUND;
		}
	}
	if (pNumListed != NULL)
		*pNumListed = i;

	return err;
}

MsgERR	MessageDBView::ListMsgHdrByIndex(MSG_ViewIndex startIndex, int numToList, MessageHdrStruct *pOutput, int *pNumListed)
{
	MsgERR err = eSUCCESS;

	XP_BZERO(pOutput, sizeof(*pOutput) * numToList);
	int i;
	for (i = 0; i < numToList && err == eSUCCESS; i++)
	{
		if (i + startIndex < m_idArray.GetSize())
		{
			err = m_messageDB->GetMessageHdr(m_idArray[i + startIndex], 
												pOutput + i);
			if (err == eSUCCESS)
			{
				CopyExtraFlagsToPublicFlags(m_flags[i + startIndex], 
										&((pOutput + i)->m_flags));
				// force non-threaded view to be flat. 
				if (m_sortType != SortByThread)
				{
					(pOutput + i)->m_level = 0;
					(pOutput + i)->m_flags &= ~(kHasChildren);
				}
				else
				{
					(pOutput + i)->m_level = m_levels[i + startIndex];
					if ((pOutput + i)->m_level == 0)
					{
						DBThreadMessageHdr *thread = m_messageDB->GetDBThreadHdrForThreadID((pOutput + i)->m_threadId);
						if (thread != NULL)
						{
							(pOutput + i)->m_numChildren = thread->GetNumChildren() - 1;
							(pOutput + i)->m_numNewChildren = thread->GetNumNewChildren();
							delete thread;
						}
					}
				}
			}
		}
		else
		{
			err = eID_NOT_FOUND;
		}
	}
	if (pNumListed != NULL)
		*pNumListed = i;

	return err;
}

MsgERR	MessageDBView::GetMsgLevelByIndex(MSG_ViewIndex index, int &level)
{
	MsgERR err = eSUCCESS;
	level = 0;

	if ((int) index < m_levels.GetSize() ) {
		level = m_sortType == SortByThread ? m_levels[ index ] : 0;
	} else {
		err = eID_NOT_FOUND;
	}

	return err;
}

// This counts the number of messages in an expanded thread, given the
// index of the first message in the thread.
int32 MessageDBView::CountExpandedThread(MSG_ViewIndex index)
{
	int32 numInThread = 0;
	MSG_ViewIndex startOfThread = index;
	while ((int32) startOfThread >= 0 && m_levels[startOfThread] != 0)
		startOfThread--;
	MSG_ViewIndex threadIndex = startOfThread;
	do
	{
		threadIndex++;
		numInThread++;
	}
	while ((int32) threadIndex < m_levels.GetSize() && m_levels[threadIndex] != 0);

	return numInThread;
}

// returns the number of lines that would be added (> 0) or removed (< 0) 
// if we were to try to expand/collapse the passed index.
MsgERR MessageDBView::ExpansionDelta(MSG_ViewIndex index, int32 *expansionDelta)
{
	int32 numChildren;
	MsgERR	err;

	*expansionDelta = 0;
	if ((int) index > m_idArray.GetSize())
		return eID_NOT_FOUND;
	char	flags = m_flags[index];

	if (m_sortType != SortByThread)
		return eSUCCESS;

	// The client can pass in the key of any message
	// in a thread and get the expansion delta for the thread.

	if (!(m_viewFlags & kUnreadOnly))
	{
		err = m_messageDB->GetThreadCount(m_idArray[index], &numChildren);
		if (err != eSUCCESS)
			return err;
	}
	else
	{
		numChildren = CountExpandedThread(index);
	}

	if (flags & kElided)
		*expansionDelta = numChildren - 1;
	else
		*expansionDelta = - (numChildren - 1);

	return eSUCCESS;
}

MsgERR MessageDBView::ToggleExpansion(MSG_ViewIndex index, uint32 *numChanged)
{
	MSG_ViewIndex threadIndex = ThreadIndexOfMsg(GetAt(index), index);
	if (threadIndex == MSG_VIEWINDEXNONE)
	{
		XP_ASSERT(FALSE);
		return eNotThread;
	}
	char	flags = m_flags[threadIndex];

	// if not a thread, or doesn't have children, no expand/collapse
	// If we add sub-thread expand collapse, this will need to be relaxed
	if (!(flags & kIsThread) || !(flags && kHasChildren))
		return eNotThread;
	if (flags & kElided)
		return ExpandByIndex(threadIndex, numChanged);
	else
		return CollapseByIndex(threadIndex, numChanged);

}

MsgERR MessageDBView::ExpandAll()
{
	for (int i = GetSize() - 1; i >= 0; i--) 
	{
		uint32 numExpanded;
		char flags = m_flags[i];
		if (flags & kElided)
			ExpandByIndex(i, &numExpanded);
	}
	return eSUCCESS;
}

MsgERR MessageDBView::CollapseAll()
{
	for (int i = 0; i < GetSize(); i++) 
	{
		uint32 numCollapsed;
		char flags = m_flags[i];
		if (!(flags & kElided))
			CollapseByIndex(i, &numCollapsed);
	}
	return eSUCCESS;
}


MsgERR MessageDBView::ExpandByIndex(MSG_ViewIndex index, uint32 *pNumExpanded)
{
	int				numListed;
	char			flags = m_flags[index];
	MessageKey		firstIdInThread, startMsg = kIdNone;
	MsgERR			err;
	MSG_ViewIndex	firstInsertIndex = index + 1;
	MSG_ViewIndex	insertIndex = firstInsertIndex;
	uint32			numExpanded = 0;
	IDArray			tempIDArray;
	XPByteArray		tempFlagArray;
	XPByteArray		tempLevelArray;
	XPByteArray		unreadLevelArray;

	XP_ASSERT(flags & kElided);
	flags &= ~kElided;

	if ((int) index > m_idArray.GetSize())
		return eID_NOT_FOUND;

	firstIdInThread = m_idArray[index];
	DBMessageHdr *msgHdr = m_messageDB->GetDBHdrForKey(firstIdInThread);
	if (msgHdr == NULL)
	{
		XP_ASSERT(FALSE);
		return eID_NOT_FOUND;
	}
	m_flags[index] = flags;
	NoteChange(index, 1, MSG_NotifyChanged);
	do
	{
		const int listChunk = 200;
		MessageKey	listIDs[listChunk];
		char		listFlags[listChunk];
		char		listLevels[listChunk];


		if (m_viewFlags & kUnreadOnly)
		{
			if (flags & kIsRead)
				unreadLevelArray.Add(0);	// keep top level hdr in thread, even though read.
			err = m_messageDB->ListUnreadIdsInThread(msgHdr->GetThreadId(),  &startMsg, unreadLevelArray, 
											listChunk, listIDs, listFlags, listLevels, &numListed);
		}
		else
			err = m_messageDB->ListIdsInThread(msgHdr,  &startMsg, listChunk, 
											listIDs, listFlags, listLevels, &numListed);

		// Don't add thread to view, it's already in.
		for (int i = 0; i < numListed; i++)
		{
			if (listIDs[i] != firstIdInThread)
			{
				tempIDArray.Add(listIDs[i]);
				tempFlagArray.Add(listFlags[i]);
				tempLevelArray.Add(listLevels[i]);
				insertIndex++;
			}
		}
		if (numListed < listChunk || startMsg == kIdNone)
			break;
	}
	while (err == eSUCCESS);
	numExpanded = (insertIndex - firstInsertIndex);

	NoteStartChange(firstInsertIndex, numExpanded, MSG_NotifyInsertOrDelete);

	m_idArray.InsertAt(firstInsertIndex, &tempIDArray);
	m_flags.InsertAt(firstInsertIndex, &tempFlagArray);
	m_levels.InsertAt(firstInsertIndex, &tempLevelArray);

	NoteEndChange(firstInsertIndex, numExpanded, MSG_NotifyInsertOrDelete);
	delete msgHdr;
	if (pNumExpanded != NULL)
		*pNumExpanded = numExpanded;
	return err;
}

MsgERR MessageDBView::CollapseByIndex(MSG_ViewIndex index, uint32 *pNumCollapsed)
{
	MessageKey		firstIdInThread;
	MsgERR	err;
	char	flags = m_flags[index];
	int32	threadCount = 0;

	if (flags & kElided || m_sortType != SortByThread)
		return eSUCCESS;
	flags  |= kElided;

	if (index > m_idArray.GetSize())
		return eID_NOT_FOUND;

	firstIdInThread = m_idArray[index];
	DBMessageHdr *msgHdr = m_messageDB->GetDBHdrForKey(firstIdInThread);
	if (msgHdr == NULL)
	{
		XP_ASSERT(FALSE);
		return eID_NOT_FOUND;
	}

	m_flags[index] = flags;
	NoteChange(index, 1, MSG_NotifyChanged);

	err = ExpansionDelta(index, &threadCount);
	if (err == eSUCCESS)
	{
		int32 numRemoved = threadCount; // don't count first header in thread
		NoteStartChange(index + 1, -numRemoved, MSG_NotifyInsertOrDelete);
		// start at first id after thread.
		for (int i = 1; i <= threadCount && index + 1 < m_idArray.GetSize(); i++)
		{
			m_idArray.RemoveAt(index + 1);
			m_flags.RemoveAt(index + 1);
			m_levels.RemoveAt(index + 1);
		}
		if (pNumCollapsed != NULL)
			*pNumCollapsed = numRemoved;	
		NoteEndChange(index + 1, -numRemoved, MSG_NotifyInsertOrDelete);
	}
	delete msgHdr;
	return err;
}

MsgERR MessageDBView::FindPrevUnread(MessageKey startKey, MessageKey *pResultKey,
									 MessageKey *resultThreadId)
{
	MSG_ViewIndex	startIndex = FindViewIndex(startKey);
	MSG_ViewIndex	curIndex = startIndex;
	MSG_ViewIndex	lastIndex = (uint32) GetSize() - 1;
	MsgERR	err = eID_NOT_FOUND;

	if (startIndex == kViewIndexNone)
		return eID_NOT_FOUND;

	*pResultKey = kIdNone;
	if (resultThreadId)
		*resultThreadId = kIdNone;

	for (; (int) curIndex >= 0 && (*pResultKey == kIdNone); curIndex--)
	{
		char	flags = m_flags[curIndex];

		if (curIndex != startIndex && flags & kIsThread && flags & kElided)
		{
			MessageKey	threadId = m_idArray[curIndex];
			err = m_messageDB->GetUnreadKeyInThread(threadId, pResultKey, 
													resultThreadId);
			if (err == eSUCCESS && (*pResultKey != kIdNone))
				break;
		}
		if (!(flags & kIsRead) && (curIndex != startIndex))
		{
			*pResultKey = m_idArray[curIndex];
			err = eSUCCESS;
			break;
		}
	}
	// found unread message but we don't know the thread
	if (*pResultKey != kIdNone && resultThreadId && *resultThreadId == kIdNone)
	{
		*resultThreadId = m_messageDB->GetThreadIdForMsgId(*pResultKey);
	}
	return err;
}

// Note that these routines do NOT expand collapsed threads! This mimics the old behaviour,
// but it's also because we don't remember whether a thread contains a flagged message the
// same way we remember if a thread contains new messages. It would be painful to dive down
// into each collapsed thread to update navigate status.
// We could cache this info, but it would still be expensive the first time this status needs
// to get updated.
MsgERR MessageDBView::FindFirstFlagged(MSG_ViewIndex * pResultIndex)
{
	return FindNextFlagged(0, pResultIndex);
}

MsgERR MessageDBView::FindPrevFlagged(MSG_ViewIndex startIndex, MSG_ViewIndex *pResultIndex)
{
	MSG_ViewIndex	lastIndex = (uint32) GetSize() - 1;
	MSG_ViewIndex	curIndex;

	*pResultIndex = MSG_VIEWINDEXNONE;

	if (GetSize() > 0 && IsValidIndex(startIndex))
	{

		curIndex = startIndex; 
		do 
		{
			if (curIndex != 0)
				curIndex--;

			char	flags = m_flags[curIndex];
			if (flags & kMsgMarked)
			{
				*pResultIndex = curIndex;
				break;
			}
		}
		while (curIndex != 0);
	}
	return eSUCCESS;
}

MsgERR MessageDBView::FindNextFlagged(MSG_ViewIndex startIndex, MSG_ViewIndex *pResultIndex)
{
	MSG_ViewIndex	lastIndex = (uint32) GetSize() - 1;
	MSG_ViewIndex	curIndex;

	*pResultIndex = MSG_VIEWINDEXNONE;

	if (GetSize() > 0)
	{
		for (curIndex = startIndex; curIndex <= lastIndex; curIndex++)
		{
			char	flags = m_flags[curIndex];
			if (flags & kMsgMarked)
			{
				*pResultIndex = curIndex;
				break;
			}
		}
	}
	return eSUCCESS;
}

MsgERR MessageDBView::FindFirstNew(MSG_ViewIndex * pResultIndex)
{
	MessageKey	firstNewKey = m_messageDB->GetFirstNew();
	if (pResultIndex)
		*pResultIndex = FindKey(firstNewKey, TRUE);
	return eSUCCESS;
}

// Generic routine to find next unread id. It doesn't do an expand of a
// thread with new messages, so it can't return a view index.
MsgERR MessageDBView::FindNextUnread(MessageKey startId, MessageKey *pResultKey, 
									 MessageKey *resultThreadId)
{
	MSG_ViewIndex	startIndex = FindViewIndex(startId);
	MSG_ViewIndex	curIndex = startIndex;
	MSG_ViewIndex	lastIndex = (uint32) GetSize() - 1;
	MsgERR			err = eSUCCESS;

	if (startIndex == kViewIndexNone)
		return eID_NOT_FOUND;

	*pResultKey = kIdNone;
	if (resultThreadId)
		*resultThreadId = kIdNone;

	for (; curIndex <= lastIndex && (*pResultKey == kIdNone); curIndex++)
	{
		char	flags = m_flags[curIndex];

		if (!(flags & kIsRead) && (curIndex != startIndex))
		{
			*pResultKey = m_idArray[curIndex];
			break;
		}
		// check for collapsed thread with new children
		if (m_sortType == SortByThread && flags & kIsThread && flags & kElided)
		{
			MessageKey	threadId = m_idArray[curIndex];
			err = m_messageDB->GetUnreadKeyInThread(threadId, pResultKey, 
													resultThreadId);
			if (err == eSUCCESS && (*pResultKey != kIdNone))
				break;
		}
	}
	// found unread message but we don't know the thread
	if (*pResultKey != kIdNone && resultThreadId && *resultThreadId == kIdNone)
	{
		*resultThreadId = m_messageDB->GetThreadIdForMsgId(*pResultKey);
	}
	return err;
}

MsgERR MessageDBView::DataNavigate( MessageKey startKey, MSG_MotionType motion,
					MessageKey *pResultKey, MessageKey *pThreadKey)
{
	MsgERR			err = eSUCCESS;
	MSG_ViewIndex	resultIndex;
	MSG_ViewIndex	lastIndex = (uint32) GetSize() - 1;
	MSG_ViewIndex	startIndex = FindViewIndex(startKey);

	if (startIndex == kViewIndexNone)
		return eID_NOT_FOUND;

	XP_ASSERT(pResultKey != NULL );
	if (pResultKey == NULL)
		return eBAD_PARAMETER;

	switch (motion)
	{
		case MSG_FirstMessage:
			*pResultKey = m_idArray[0];
			break;
		case MSG_NextMessage:
			// return same index and id on next on last message
			resultIndex = MIN(startIndex + 1, lastIndex);
			*pResultKey = m_idArray[resultIndex];
			break;
		case MSG_PreviousMessage:
			*pResultKey = m_idArray[(startIndex > 0) ? startIndex : 0];
			break;
		case MSG_LastMessage:
			*pResultKey = m_idArray[lastIndex];
			break;
		case MSG_FirstUnreadMessage:
			// note fall thru - is this motion ever used?
			startKey = m_idArray[0];
		case MSG_NextUnreadMessage:
			// It might be worthwhile to not actually find the next unread,
			// but just determine if there is one. Or, it might be worth
			// remembering the next unread.
			FindNextUnread(startKey, pResultKey, pThreadKey);
			break;
		case MSG_PreviousUnreadMessage:
			// will do an expand
			err = FindPrevUnread(startKey, pResultKey, pThreadKey);
			break;
		case MSG_LastUnreadMessage:
			break;

		default:
		XP_ASSERT(FALSE);	// unsupported motion.
		break;
	}
	return err;
}

typedef struct CommandStrLookup
{
	int		command;
	int		mkStringNum;
} CommandStrLookup;

// Because some compilers can't initialize static data with ints, we have the following pain
CommandStrLookup navigateCommands[] =
{
	{ MSG_FirstMessage,				0, /*MK_MSG_FIRST_MSG */},			
	{ MSG_NextMessage,				0, /*MK_MSG_NEXT_MSG */},
	{ MSG_PreviousMessage,			0, /*MK_MSG_PREV_MSG */},
	{ MSG_LastMessage,				0, /*MK_MSG_LAST_MSG */},
	{ MSG_FirstUnreadMessage,		0, /*MK_MSG_FIRST_UNREAD */},
	{ MSG_NextUnreadMessage,		0, /*MK_MSG_NEXT_UNREAD */},
	{ MSG_PreviousUnreadMessage,	0, /*MK_MSG_PREV_UNREAD */},
	{ MSG_LastUnreadMessage,		0, /*MK_MSG_LAST_UNREAD */},
	{ MSG_ReadMore,					0, /*MK_MSG_READ_MORE */},
	{ MSG_NextUnreadThread,			0, /*MK_MSG_NEXTUNREAD_THREAD */},
	{ MSG_NextUnreadGroup,			0, /*MK_MSG_NEXTUNREAD_GROUP */},
	{ MSG_FirstFlagged,				0, /*MK_MSG_FIRST_FLAGGED */},
	{ MSG_NextFlagged,				0, /*MK_MSG_NEXT_FLAGGED */},
	{ MSG_PreviousFlagged,			0, /*MK_MSG_PREVIOUS_FLAGGED */},
};

/*static*/ void MessageDBView::InitNavigateCommands()
{
	if (navigateCommands[0].mkStringNum == 0)
	{
		navigateCommands[0].mkStringNum = MK_MSG_FIRST_MSG;
		navigateCommands[1].mkStringNum = MK_MSG_NEXT_MSG;
		navigateCommands[2].mkStringNum = MK_MSG_PREV_MSG;
		navigateCommands[3].mkStringNum = MK_MSG_LAST_MSG;
		navigateCommands[4].mkStringNum = MK_MSG_FIRST_UNREAD;
		navigateCommands[5].mkStringNum = MK_MSG_NEXT_UNREAD;
		navigateCommands[6].mkStringNum = MK_MSG_PREV_UNREAD;
		navigateCommands[7].mkStringNum = MK_MSG_LAST_UNREAD;
		navigateCommands[8].mkStringNum = MK_MSG_READ_MORE;
		navigateCommands[9].mkStringNum = MK_MSG_NEXTUNREAD_THREAD;
		navigateCommands[10].mkStringNum = MK_MSG_NEXTUNREAD_GROUP;
		navigateCommands[11].mkStringNum = MK_MSG_FIRST_FLAGGED;
		navigateCommands[12].mkStringNum = MK_MSG_NEXT_FLAGGED;
		navigateCommands[13].mkStringNum = MK_MSG_PREV_FLAGGED;
	}
}

MsgERR MessageDBView::GetNavigateStatus(MSG_MotionType motion, MSG_ViewIndex index, 
						  XP_Bool *selectable_p,
						  const char **display_string)
{
	XP_Bool		enable = FALSE;
	MsgERR		err = eFAILURE;
	MessageKey	resultKey;
	MSG_ViewIndex	resultIndex = MSG_VIEWINDEXNONE;

	InitNavigateCommands();

	// warning - we no longer validate index up front because fe passes in -1 for no
	// selection, so if you use index, be sure to validate it before using it
	// as an array index.
	switch (motion)
	{
		case MSG_FirstMessage:
		case MSG_LastMessage:
			if (GetSize() > 0)
				enable = TRUE;
			break;
		case MSG_NextMessage:
			if (IsValidIndex(index) && index < (uint32) GetSize() - 1)
				enable = TRUE;
			break;
		case MSG_PreviousMessage:
			if (IsValidIndex(index) && index != 0 && GetSize() > 1)
				enable = TRUE;
			break;
		case MSG_FirstFlagged:
			err = FindFirstFlagged(&resultIndex);
			enable = (err == eSUCCESS && resultIndex != MSG_VIEWINDEXNONE);
			break;
		case MSG_NextFlagged:
			err = FindNextFlagged(index + 1, &resultIndex);
			enable = (err == eSUCCESS && resultIndex != MSG_VIEWINDEXNONE);
			break;
		case MSG_PreviousFlagged:
			if (IsValidIndex(index) && index != 0)
				err = FindPrevFlagged(index, &resultIndex);
			enable = (err == eSUCCESS && resultIndex != MSG_VIEWINDEXNONE);
			break;
		case MSG_FirstNew:
			err = FindFirstNew(&resultIndex);
			enable = (err == eSUCCESS && resultIndex != MSG_VIEWINDEXNONE);
			break;
		case MSG_LaterMessage:
			enable = GetSize() > 0;
			break;
		case MSG_ReadMore:
			enable = TRUE;	// for now, always true.
			break;
		case MSG_NextFolder:
		case MSG_NextUnreadGroup:
		case MSG_NextUnreadThread:
		case MSG_NextUnreadMessage:
		case (MSG_MotionType) MSG_ToggleThreadKilled:

			enable = TRUE;	// always enabled in nwo
			break;
		case MSG_PreviousUnreadMessage:
			if (IsValidIndex(index))
			{
				err = FindPrevUnread(m_idArray[index], &resultKey);
				enable = (resultKey != kIdNone);
			}
			break;
		default:
			XP_ASSERT(FALSE);

	}

	if (selectable_p)
		*selectable_p = enable;

//	look up motion code in CommandStrLookup.
	for (int i = 0; display_string && i < (sizeof(navigateCommands) / sizeof(navigateCommands[0])); i++)
	{
		if (navigateCommands[i].command == motion)
		{
			*display_string = XP_GetString(navigateCommands[i].mkStringNum);
			break;
		}
	}
	return eSUCCESS;
}



// Starting from startId, performs the passed in navigation, including
// any marking read needed, and returns the resultKey and index of the
// destination of the navigation. If there are no more unread in the view,
// it returns a resultId of kIdNone and an index of kViewIndexNone.
MsgERR MessageDBView::Navigate(MSG_ViewIndex startIndex, MSG_MotionType motion, 
							   MessageKey * pResultKey, MSG_ViewIndex * pResultIndex,
							   MSG_ViewIndex *pThreadIndex, XP_Bool wrap /* = TRUE */)
{
	MsgERR			err = eSUCCESS;
	MessageKey		resultThreadKey;
	MSG_ViewIndex	curIndex;
	MSG_ViewIndex	lastIndex = (GetSize() > 0) ? (uint32) GetSize() - 1 : kViewIndexNone;
	MSG_ViewIndex	threadIndex = kViewIndexNone;

	XP_ASSERT(pResultKey != NULL && pResultIndex != NULL );
	if (pResultKey == NULL || pResultIndex == NULL)
		return eBAD_PARAMETER;

	switch (motion)
	{
		case MSG_FirstMessage:
			if (GetSize() > 0)
			{
				*pResultIndex = 0;
				*pResultKey = m_idArray[0];
			}
			else
			{
				*pResultIndex = kViewIndexNone;
				*pResultKey = MSG_MESSAGEKEYNONE;
			}
			break;
		case MSG_NextMessage:
			// return same index and id on next on last message
			*pResultIndex = MIN(startIndex + 1, lastIndex);
			*pResultKey = m_idArray[*pResultIndex];
			break;
		case MSG_PreviousMessage:
			*pResultIndex = (startIndex > 0) ? startIndex - 1 : 0;
			*pResultKey = m_idArray[*pResultIndex];
			break;
		case MSG_LastMessage:
			*pResultIndex = lastIndex;
			*pResultKey = m_idArray[*pResultIndex];
			break;
		case MSG_FirstFlagged:
			err = FindFirstFlagged(pResultIndex);
			if (IsValidIndex(*pResultIndex))
				*pResultKey = m_idArray[*pResultIndex];
			break;
		case MSG_NextFlagged:
			err = FindNextFlagged(startIndex + 1, pResultIndex);
			if (IsValidIndex(*pResultIndex))
				*pResultKey = m_idArray[*pResultIndex];
			break;
		case MSG_PreviousFlagged:
			err = FindPrevFlagged(startIndex, pResultIndex);
			if (IsValidIndex(*pResultIndex))
				*pResultKey = m_idArray[*pResultIndex];
			break;
		case MSG_FirstNew:
			err = FindFirstNew(pResultIndex);
			if (IsValidIndex(*pResultIndex))
				*pResultKey = m_idArray[*pResultIndex];
			break;
		case MSG_FirstUnreadMessage:
			startIndex = kViewIndexNone;		// note fall thru - is this motion ever used?
		case MSG_NextUnreadMessage:
			for (curIndex = (startIndex == kViewIndexNone) ? 0 : startIndex; curIndex <= lastIndex && lastIndex != kViewIndexNone; curIndex++)
			{
				char	flags = m_flags[curIndex];

				// don't return start index since navigate should move
				if (!(flags & kIsRead) && (curIndex != startIndex))
				{
					*pResultIndex = curIndex;
					*pResultKey = m_idArray[*pResultIndex];
					break;
				}
				// check for collapsed thread with new children
				if (m_sortType == SortByThread && flags & kIsThread && flags & kElided)
				{
					DBThreadMessageHdr *threadHdr = m_messageDB->GetDBThreadHdrForMsgID(m_idArray[curIndex]);
					if (threadHdr == NULL)
					{
						XP_ASSERT(FALSE);
						continue;
					}
					if (threadHdr->GetNumNewChildren() > 0)
					{
						uint32 numExpanded;
						ExpandByIndex(curIndex, &numExpanded);
						lastIndex += numExpanded;
						if (pThreadIndex != NULL)
							*pThreadIndex = curIndex;
					}
					delete threadHdr;

				}
			}
			if (curIndex > lastIndex)
			{
				// wrap around by starting at index 0.
				if (wrap)
				{
					MessageKey	startKey = GetAt(startIndex);

					err = Navigate(kViewIndexNone, MSG_NextUnreadMessage, pResultKey,
							pResultIndex, pThreadIndex, FALSE);
					if (*pResultKey == startKey)	// wrapped around and found start message!
					{
						*pResultIndex = kViewIndexNone;
						*pResultKey = kIdNone;
					}
				}
				else
				{
					*pResultIndex = kViewIndexNone;
					*pResultKey = kIdNone;
				}
			}
			break;
		case MSG_PreviousUnreadMessage:
			err = FindPrevUnread(m_idArray[startIndex], pResultKey,
								&resultThreadKey);
			if (err == eSUCCESS)
			{
				*pResultIndex = FindViewIndex(*pResultKey);
				if (*pResultKey != resultThreadKey && m_sortType == SortByThread)
				{	
					threadIndex  = ThreadIndexOfMsg(*pResultKey, kViewIndexNone);
					if (*pResultIndex == kViewIndexNone)
					{
						DBThreadMessageHdr *threadHdr = m_messageDB->GetDBThreadHdrForMsgID(*pResultKey);
						if (threadHdr == NULL)
						{
							XP_ASSERT(FALSE);
							break;
						}
						if (threadHdr->GetNumNewChildren() > 0)
						{
							uint32 numExpanded;
							ExpandByIndex(threadIndex, &numExpanded);
						}
						delete threadHdr;
						*pResultIndex = FindViewIndex(*pResultKey);
					}
				}
				if (pThreadIndex != NULL)
					*pThreadIndex = threadIndex;
			}
			break;
		case MSG_LastUnreadMessage:
			break;
		case MSG_NextUnreadThread:	
			if (startIndex == kViewIndexNone)
			{
				XP_ASSERT(FALSE);
				break;
			}
			err = MarkThreadOfMsgRead(m_idArray[startIndex], startIndex, TRUE);
			if (err == eSUCCESS)
				return Navigate(startIndex, MSG_NextUnreadMessage, pResultKey,
								pResultIndex, pThreadIndex, TRUE);
			break;


		case MSG_ToggleThreadKilled:
			{
				XP_Bool	resultKilled;

				if (startIndex == kViewIndexNone)
				{
					XP_ASSERT(FALSE);
					break;
				}
				threadIndex = ThreadIndexOfMsg(GetAt(startIndex), startIndex);
				ToggleIgnored( &startIndex,	1, &resultKilled);
				if (resultKilled)
				{
					if (threadIndex != MSG_VIEWINDEXNONE)
						CollapseByIndex(threadIndex, NULL);
					return Navigate(threadIndex, MSG_NextUnreadThread, pResultKey,
							pResultIndex, pThreadIndex);
				}
				else
				{
					*pResultIndex = startIndex;
					*pResultKey = m_idArray[*pResultIndex];
					return eSUCCESS;
				}
			}
		case MSG_LaterMessage:
			if (startIndex == kViewIndexNone)
			{
				XP_ASSERT(FALSE);
				break;
			}
			m_messageDB->MarkLater(m_idArray[startIndex], 0);
			return Navigate(startIndex, MSG_NextUnreadMessage, pResultKey,
					pResultIndex, pThreadIndex);

		default:
			XP_ASSERT(FALSE);	// unsupported motion.
		break;
	}
	return err;
}

MSG_ViewIndex MessageDBView::GetIndexOfFirstDisplayedKeyInThread(DBThreadMessageHdr *threadHdr)
{
	MSG_ViewIndex	retIndex = MSG_VIEWINDEXNONE;
	int	childIndex = 0;
	// We could speed up the unreadOnly view by starting our search with the first
	// unread message in the thread. Sometimes, that will be wrong, however, so
	// let's skip it until we're sure it's neccessary.
//	(m_viewFlags & kUnreadOnly) 
//		? threadHdr->GetFirstUnreadKey(m_messageDB) : threadHdr->GetChildAt(0);
	while (retIndex == MSG_VIEWINDEXNONE && childIndex < threadHdr->GetNumChildren())
	{
		MessageKey childKey = threadHdr->GetChildAt(childIndex++);
		retIndex = FindViewIndex(childKey);
	}
	return retIndex;
}

DBMessageHdr *MessageDBView::GetFirstMessageHdrToDisplayInThread(DBThreadMessageHdr *threadHdr)
{
	DBMessageHdr	*msgHdr;

	if (m_viewFlags & kUnreadOnly)
		msgHdr = threadHdr->GetFirstUnreadChild(GetDB());
	else
		msgHdr = threadHdr->GetChildHdrAt(0);
	return msgHdr;
}

// caller must referTo hdr if they want to hold it or change it!
DBMessageHdr *MessageDBView::GetFirstDisplayedHdrInThread(DBThreadMessageHdr *threadHdr)
{
	MSG_ViewIndex viewIndex = GetIndexOfFirstDisplayedKeyInThread(threadHdr);
	MessageKey msgKey = GetAt(viewIndex);
	return (msgKey == MSG_MESSAGEKEYNONE) ? 0 : m_messageDB->GetDBHdrForKey(msgKey);
}

// Find the view index of the thread containing the passed msgKey, if
// the thread is in the view. MsgIndex is passed in as a shortcut if
// it turns out the msgKey is the first message in the thread,
// then we can avoid looking for the msgKey.
MSG_ViewIndex MessageDBView::ThreadIndexOfMsg(MessageKey msgKey, 
											  MSG_ViewIndex msgIndex /* = kViewIndexNone */,
											  int32 *pThreadCount /* = NULL */,
											  uint32 *pFlags /* = NULL */)
{
	if (m_sortType != SortByThread)
		return kViewIndexNone;
	DBThreadMessageHdr *threadHdr = m_messageDB->GetDBThreadHdrForMsgID(msgKey);
	MSG_ViewIndex retIndex = kViewIndexNone;

	if (threadHdr != NULL)
	{
		if (msgIndex == kViewIndexNone)
			msgIndex = FindViewIndex(msgKey);

		if (msgIndex == kViewIndexNone)	// key is not in view, need to find by thread
		{
			msgIndex = GetIndexOfFirstDisplayedKeyInThread(threadHdr);
			MessageKey		threadKey = (msgIndex == kViewIndexNone) ? kIdNone : GetAt(msgIndex);
			if (pFlags)
				*pFlags = threadHdr->GetFlags();
		}
		MSG_ViewIndex startOfThread = msgIndex;
		while ((int32) startOfThread >= 0 && m_levels[startOfThread] != 0)
			startOfThread--;
		retIndex = startOfThread;
		if (pThreadCount)
		{
			int32 numChildren = 0;
			MSG_ViewIndex threadIndex = startOfThread;
			do
			{
				threadIndex++;
				numChildren++;
			}
			while ((int32) threadIndex < m_levels.GetSize() && m_levels[threadIndex] != 0);
			*pThreadCount = numChildren;
		}
		delete threadHdr;
	}
	return retIndex;
}

MsgERR MessageDBView::MarkThreadOfMsgRead(MessageKey msgId, MSG_ViewIndex msgIndex, XP_Bool bRead)
{
	DBThreadMessageHdr *threadHdr = m_messageDB->GetDBThreadHdrForMsgID(msgId);
	MSG_ViewIndex	threadIndex;
	MsgERR			err;

	if (threadHdr == NULL)
	{
		XP_ASSERT(FALSE);
		return eID_NOT_FOUND;
	}
	if (msgId != threadHdr->GetChildAt(0))
		threadIndex = GetIndexOfFirstDisplayedKeyInThread(threadHdr);
	else
		threadIndex = msgIndex;
	err = MarkThreadRead(threadHdr, threadIndex, bRead);
	delete threadHdr;
	return err;
}

MsgERR MessageDBView::MarkThreadRead(DBThreadMessageHdr *threadHdr, 
									 MSG_ViewIndex threadIndex, XP_Bool bRead)
{
	XP_Bool			threadElided = TRUE;
	if (threadIndex != kViewIndexNone)
		threadElided = (m_flags[threadIndex] & kElided);

	for (int childIndex = 0; childIndex < threadHdr->GetNumChildren(); 
			childIndex++)
	{
		DBMessageHdr *msgHdr = threadHdr->GetChildHdrAt(childIndex);
		if (msgHdr == NULL)
		{
#ifdef DEBUG_bienvenu
			XP_ASSERT(FALSE);
#endif
			continue;
		}
		// don't pass in listener so we can get change notification!
		// We used to mark read through the view, but we weren't checking
		// if the key actually was in the view.
		m_messageDB->MarkHdrRead(msgHdr, bRead, NULL /* &m_changeListener */);
		delete msgHdr;
	}
	if (bRead)
		threadHdr->SetNumNewChildren(0);
	else
		threadHdr->SetNumNewChildren(threadHdr->GetNumChildren());
	return eSUCCESS;
}
// view modifications methods by index

// read/unread handling.
MsgERR MessageDBView::ToggleReadByIndex(MSG_ViewIndex index)
{
	if (!IsValidIndex(index))
		return eInvalidIndex;
	return SetReadByIndex(index, !(m_flags[index] & kIsRead));
}

MsgERR MessageDBView::SetReadByIndex(MSG_ViewIndex index, XP_Bool read)
{
	MsgERR err;

	if (!IsValidIndex(index))
		return eInvalidIndex;
	if (read)
		OrExtraFlag(index, kIsRead);
	else
		AndExtraFlag(index, ~kIsRead);

	err = m_messageDB->MarkRead(m_idArray[index], read, &m_changeListener);
	NoteChange(index, 1, MSG_NotifyChanged);
	if (m_sortType == SortByThread)
	{
		MSG_ViewIndex threadIndex = ThreadIndexOfMsg(m_idArray[index], index, NULL, NULL);
		if (threadIndex != index)
			NoteChange(threadIndex, 1, MSG_NotifyChanged);
	}
	return err;
}

MsgERR MessageDBView::SetThreadOfMsgReadByIndex(MSG_ViewIndex index, XP_Bool /*read*/)
{
	MsgERR err;

	if (!IsValidIndex(index))
		return eInvalidIndex;
	err = MarkThreadOfMsgRead(m_idArray[index], index, TRUE);
	return err;
}


static MsgERR
msg_AddNameAndAddressToAB (MWContext* context, const char *name, const char *address, XP_Bool lastOneToAdd)
{
	MsgERR err = eSUCCESS;
	PersonEntry person;
	char * tempname = NULL;
	DIR_Server* pab = NULL;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

	person.Initialize();

	if (XP_STRLEN (name))
		tempname =	XP_STRDUP (name);
	else
	{
		int len = 0;
		char * at = NULL;
		if (address)
		{
			// if the mail address doesn't contain @
			// then the name is the whole email address
			if ((at = XP_STRCHR(address, '@')) == NULL)
				tempname = MSG_ExtractRFC822AddressNames (address);
			else {
				// otherwise extract everything up to the @
				// for the name
				len = XP_STRLEN (address) - XP_STRLEN (at);
				tempname = (char *) XP_ALLOC (len + 1);
				XP_STRNCPY_SAFE (tempname, address, len + 1);
			}
		}
	}

	person.pGivenName = tempname;
	person.pEmailAddress = XP_STRDUP (address);
	person.WinCSID = INTL_GetCSIWinCSID(c);
	AB_BreakApartFirstName (FE_GetAddressBook(NULL), &person);
	DIR_GetPersonalAddressBook (FE_GetDirServers(), &pab);

	err = AB_AddUserWithUI (context, &person, pab, lastOneToAdd);

	person.CleanUp();

	return err;
}

	/* new address book APIs require a destination container */
MsgERR MessageDBView::AddSenderToABByIndex(MSG_Pane * pane, MWContext* context, MSG_ViewIndex index, XP_Bool lastOneToAdd, XP_Bool displayRecip, AB_ContainerInfo * destAB)
{
	/* taken verbatim from the old address book version except we call the new API AB_AddNameAndAddress */
	MsgERR err = eSUCCESS;
	char author[512];
	char authorname [256];
	int num;
	char * name = NULL;
	char * address = NULL;

	if (!IsValidIndex(index))
		return eInvalidIndex;
	if (!context)
		return eUNKNOWN;

	DBMessageHdr *dbHdr = m_messageDB->GetDBHdrForKey(m_idArray[index]);
	if (displayRecip)
	{
		int32 numRecips = dbHdr->GetNumRecipients();
		for (int32 i = 0; i < numRecips && err == eSUCCESS; i++)
		{
			XPStringObj recipName;
			dbHdr->GetNameOfRecipient (recipName, (int) i, m_messageDB->GetDB());

			XPStringObj recipAddress;
			dbHdr->GetMailboxOfRecipient (recipAddress, (int) i, m_messageDB->GetDB());

			err = AB_AddNameAndAddress(pane, destAB, recipName, recipAddress, lastOneToAdd);
		}
	}
	else
	{
		dbHdr->GetRFC822Author(authorname, sizeof (authorname), m_messageDB->GetDB());
		dbHdr->GetAuthor(author, sizeof (author), m_messageDB->GetDB());
		num = MSG_ParseRFC822Addresses(author, &name, &address);
		if (num < 0)
			return eOUT_OF_MEMORY;

		err = AB_AddNameAndAddress (pane, destAB, authorname, address, lastOneToAdd);
	}
	delete dbHdr;

	XP_FREEIF (name);
	XP_FREEIF (address);

	return err;
}

MsgERR MessageDBView::AddAllToABByIndex(MSG_Pane * pane, MWContext* context, MSG_ViewIndex index, XP_Bool lastOneToAdd, AB_ContainerInfo * destAB)
{
	/* copied pretty much verbatim from the old address book version except we call the new APIs. */
	MsgERR err = eSUCCESS;
	XPStringObj prettyname;	
	XPStringObj mailbox;
	int32 numRecipients = 0;
	int32 numCCList = 0;
	char name [kMaxFullNameLength];
	char address [kMaxEmailAddressLength];

	if (!IsValidIndex(index))
		return eInvalidIndex;

	DBMessageHdr *dbHdr = m_messageDB->GetDBHdrForKey(m_idArray[index]);

	numRecipients = dbHdr->GetNumRecipients();
	numCCList = dbHdr->GetNumCCRecipients();

	err = AddSenderToABByIndex (pane, context, index, (lastOneToAdd && numRecipients == 0 && numCCList == 0), FALSE, destAB);

	if (err != eSUCCESS) {
		delete dbHdr;
		return eUNKNOWN;
	}

	for (int32 i = 0; i < numRecipients && err == eSUCCESS; i++)
	{
		dbHdr->GetNameOfRecipient(prettyname, i, m_messageDB->GetDB());
		XP_STRNCPY_SAFE(name, prettyname, kMaxFullNameLength);
		dbHdr->GetMailboxOfRecipient(mailbox, i, m_messageDB->GetDB());
		XP_STRNCPY_SAFE(address, mailbox, kMaxEmailAddressLength);
		err = AB_AddNameAndAddress (pane, destAB,name, address, (lastOneToAdd && 
			( i == numRecipients - 1) && (numCCList == 0)));
	}

	for (int32 j = 0; j < numCCList && err == eSUCCESS; j++)
	{
		dbHdr->GetNameOfCCRecipient(prettyname, j, m_messageDB->GetDB());
		XP_STRNCPY_SAFE(name, prettyname, kMaxFullNameLength);
		dbHdr->GetMailboxOfCCRecipient(mailbox, j, m_messageDB->GetDB());
		XP_STRNCPY_SAFE(address, mailbox, kMaxEmailAddressLength);
		err = AB_AddNameAndAddress (pane, destAB, name, address, (lastOneToAdd && (j == numCCList - 1)));
	}

	delete dbHdr;

	return err;
}

/* mscott: DELETE THESE AS SOON AS THE NEW ADDRESS BOOK IS TURNED ON FOR EVERYONE */
MsgERR MessageDBView::AddSenderToABByIndex(MWContext * context, MSG_ViewIndex index, XP_Bool lastOneToAdd, XP_Bool displayRecip)
{
	MsgERR err = eSUCCESS;
	char author[512];
	char authorname [256];
	int num;
	char * name = NULL;
	char * address = NULL;

	if (!IsValidIndex(index))
		return eInvalidIndex;
	if (!context)
		return eUNKNOWN;

	DBMessageHdr *dbHdr = m_messageDB->GetDBHdrForKey(m_idArray[index]);
	if (displayRecip)
	{
		int32 numRecips = dbHdr->GetNumRecipients();
		for (int32 i = 0; i < numRecips && err == eSUCCESS; i++)
		{
			XPStringObj recipName;
			dbHdr->GetNameOfRecipient (recipName, (int) i, m_messageDB->GetDB());

			XPStringObj recipAddress;
			dbHdr->GetMailboxOfRecipient (recipAddress, (int) i, m_messageDB->GetDB());

			err = msg_AddNameAndAddressToAB (context, recipName, recipAddress, lastOneToAdd);
		}
	}
	else
	{
		dbHdr->GetRFC822Author(authorname, sizeof (authorname), m_messageDB->GetDB());
		dbHdr->GetAuthor(author, sizeof (author), m_messageDB->GetDB());
		num = MSG_ParseRFC822Addresses(author, &name, &address);
		if (num < 0)
			return eOUT_OF_MEMORY;

		err = msg_AddNameAndAddressToAB (context, authorname, address, lastOneToAdd);
	}
	delete dbHdr;

	XP_FREEIF (name);
	XP_FREEIF (address);

	return err;
}

MsgERR MessageDBView::AddAllToABByIndex(MWContext* context, MSG_ViewIndex index, XP_Bool lastOneToAdd)
{
	MsgERR err = eSUCCESS;
	XPStringObj prettyname;	
	XPStringObj mailbox;
	int32 numRecipients = 0;
	int32 numCCList = 0;
	char name [kMaxFullNameLength];
	char address [kMaxEmailAddressLength];

	if (!IsValidIndex(index))
		return eInvalidIndex;

	DBMessageHdr *dbHdr = m_messageDB->GetDBHdrForKey(m_idArray[index]);

	numRecipients = dbHdr->GetNumRecipients();
	numCCList = dbHdr->GetNumCCRecipients();

	err = AddSenderToABByIndex (context, index, (lastOneToAdd && numRecipients == 0 && numCCList == 0), FALSE);

	if (err != eSUCCESS) {
		delete dbHdr;
		return eUNKNOWN;
	}

	for (int32 i = 0; i < numRecipients && err == eSUCCESS; i++)
	{
		dbHdr->GetNameOfRecipient(prettyname, i, m_messageDB->GetDB());
		XP_STRNCPY_SAFE(name, prettyname, kMaxFullNameLength);
		dbHdr->GetMailboxOfRecipient(mailbox, i, m_messageDB->GetDB());
		XP_STRNCPY_SAFE(address, mailbox, kMaxEmailAddressLength);
		err = msg_AddNameAndAddressToAB (context, name, address, (lastOneToAdd && 
			( i == numRecipients - 1) && (numCCList == 0)));
	}

	for (int32 j = 0; j < numCCList && err == eSUCCESS; j++)
	{
		dbHdr->GetNameOfCCRecipient(prettyname, j, m_messageDB->GetDB());
		XP_STRNCPY_SAFE(name, prettyname, kMaxFullNameLength);
		dbHdr->GetMailboxOfCCRecipient(mailbox, j, m_messageDB->GetDB());
		XP_STRNCPY_SAFE(address, mailbox, kMaxEmailAddressLength);
		err = msg_AddNameAndAddressToAB (context, name, address, (lastOneToAdd && (j == numCCList - 1)));
	}

	delete dbHdr;

	return err;
}

MsgERR MessageDBView::MarkMarkedByIndex(MSG_ViewIndex index, XP_Bool mark)
{
	MsgERR err;

	if (!IsValidIndex(index))
		return eInvalidIndex;

	if (mark)
		OrExtraFlag(index, kMsgMarked);
	else
		AndExtraFlag(index, ~kMsgMarked);

	err = m_messageDB->MarkMarked(m_idArray[index], mark, &m_changeListener);
	NoteChange(index, 1, MSG_NotifyChanged);
	return err;
}

// caller must RemoveReference thread!
MSG_ViewIndex	MessageDBView::GetThreadFromMsgIndex(MSG_ViewIndex index, 
													 DBThreadMessageHdr **threadHdr)
{
	MessageKey			msgKey = GetAt(index);
	MsgViewIndex		threadIndex;

	XP_ASSERT(threadHdr);
	*threadHdr = m_messageDB->GetDBThreadHdrForMsgID(msgKey);

	if (*threadHdr == NULL)
	{
//		XP_ASSERT(FALSE);
		return MSG_VIEWINDEXNONE;
	}
	if (msgKey != (*threadHdr)->m_threadKey)
		threadIndex = GetIndexOfFirstDisplayedKeyInThread(*threadHdr);
	else
		threadIndex = index;
	return threadIndex;
}

// ignore handling.
MsgERR MessageDBView::ToggleThreadIgnored(DBThreadMessageHdr *thread, MSG_ViewIndex threadIndex)

{
	MsgERR		err;
	if (!IsValidIndex(threadIndex))
		return eInvalidIndex;
	err = SetThreadIgnored(thread, threadIndex, !((thread->GetFlags() & kIgnored) != 0));
	return err;
}

MsgERR MessageDBView::ToggleThreadWatched(DBThreadMessageHdr *thread, MSG_ViewIndex index)

{
	MsgERR		err;
	if (!IsValidIndex(index))
		return eInvalidIndex;
	err = SetThreadWatched(thread, index, !((thread->GetFlags() & kWatched) != 0));
	return err;
}

MsgERR MessageDBView::ToggleIgnored( MsgViewIndex* indices,	int32 numIndices, XP_Bool *resultToggleState)
{
	MsgERR		err;
	DBThreadMessageHdr *thread = NULL;

	if (numIndices == 1)
	{
		MsgViewIndex	threadIndex = GetThreadFromMsgIndex(*indices, &thread);
		if (thread)
		{
			err = ToggleThreadIgnored(thread, threadIndex);
			if (resultToggleState)
				*resultToggleState = (thread->GetFlags() & kIgnored) ? TRUE : FALSE;
			delete thread;
		}
	}
	else
	{
		if (numIndices > 1)
			XP_QSORT (indices, numIndices, sizeof(MSG_ViewIndex), MSG_Pane::CompareViewIndices);
		for (int curIndex = numIndices - 1; curIndex >= 0; curIndex--)
		{
			MsgViewIndex	threadIndex = GetThreadFromMsgIndex(*indices, &thread);
		}
	}
	return eSUCCESS;
}
MsgERR MessageDBView::ToggleWatched( MsgViewIndex* indices,	int32 numIndices)
{
	MsgERR		err;
	DBThreadMessageHdr *thread = NULL;

	if (numIndices == 1)
	{
		MsgViewIndex	threadIndex = GetThreadFromMsgIndex(*indices, &thread);
		if (threadIndex != MSG_VIEWINDEXNONE)
		{
			err = ToggleThreadWatched(thread, threadIndex);
			delete thread;
		}
	}
	else
	{
		if (numIndices > 1)
			XP_QSORT (indices, numIndices, sizeof(MSG_ViewIndex), MSG_Pane::CompareViewIndices);
		for (int curIndex = numIndices - 1; curIndex >= 0; curIndex--)
		{
		MsgViewIndex	threadIndex = GetThreadFromMsgIndex(*indices, &thread);
		}
	}
	return eSUCCESS;
}

MsgERR MessageDBView::SetThreadIgnored(DBThreadMessageHdr *thread, MSG_ViewIndex threadIndex, XP_Bool ignored)
{
	MsgERR err;

	if (!IsValidIndex(threadIndex))
		return eInvalidIndex;
	err = m_messageDB->MarkThreadIgnored(thread, m_idArray[threadIndex], ignored, &m_changeListener);
	NoteChange(threadIndex, 1, MSG_NotifyChanged);
	if (ignored)
	{
		MarkThreadRead(thread, threadIndex, TRUE);
	}
	return err;
}
MsgERR MessageDBView::SetThreadWatched(DBThreadMessageHdr *thread, MSG_ViewIndex index, XP_Bool watched)
{
	MsgERR err;

	if (!IsValidIndex(index))
		return eInvalidIndex;
	err = m_messageDB->MarkThreadWatched(thread, m_idArray[index], watched, &m_changeListener);
	NoteChange(index, 1, MSG_NotifyChanged);
	return err;
}

MsgERR MessageDBView::SetKeyByIndex(MSG_ViewIndex index, MessageKey id)
{
	m_idArray.SetAt(index, id);
	return eSUCCESS;
}

// This method just removes the specified line from the view. It does
// NOT delete it from the database.
MsgERR MessageDBView::RemoveByIndex(MSG_ViewIndex index)
{
	if (!IsValidIndex(index))
		return eInvalidIndex;

	m_idArray.RemoveAt(index);
	m_flags.RemoveAt(index);
	m_levels.RemoveAt(index);
	NoteChange(index, -1, MSG_NotifyInsertOrDelete);
	return eSUCCESS;
}

MsgERR MessageDBView::DeleteMessagesByIndex(MSG_ViewIndex *indices, int32 numIndices, XP_Bool removeFromDB)
{
	MsgERR	err = eSUCCESS;
	IDArray	messageKeys;

	MSG_ViewIndex *tmpIndices = new MSG_ViewIndex [numIndices];
	if (!tmpIndices)
		return eOUT_OF_MEMORY;
	memcpy (tmpIndices, indices, numIndices * sizeof(MSG_ViewIndex));
	XP_QSORT (tmpIndices, numIndices, sizeof(MSG_ViewIndex), MSG_Pane::CompareViewIndices);

	if (removeFromDB)
		messageKeys.SetSize(numIndices);

	for (int32 i = numIndices - 1; i >= 0 && err == eSUCCESS; i--)
	{
		MSG_ViewIndex	viewIndex = tmpIndices[i];
		if (IsValidIndex(viewIndex))
		{
			if (removeFromDB)
				messageKeys.SetAt(i, m_idArray[viewIndex]);

			err = RemoveByIndex(viewIndex);
			OnHeaderAddedOrDeleted();
		}
	}
	if (removeFromDB)
	{
		if (err == eSUCCESS)
			err = m_messageDB->DeleteMessages(messageKeys, &m_changeListener);
	}
	delete [] tmpIndices;
	return err;
}
// Delete the message from the database, and remove it from the view.
// If there's an error deleting the message from the db, it will
// not be removed from the view. We could return an error and still
// have deleted the message from the db, if there's an error
// deleting it from the view (this better not happen...) 
MsgERR MessageDBView::DeleteMsgByIndex(MSG_ViewIndex index, XP_Bool removeFromDB)
{
	MsgERR err = eSUCCESS;
	if (!IsValidIndex(index))
		return eInvalidIndex;
		
	// cache the key before it is removed from the m_idArray
	MessageKey keyOfDeletedMessage = m_idArray[index];
	
	if (err == eSUCCESS)
	{
		err = RemoveByIndex(index);
		OnHeaderAddedOrDeleted();
	}
	if (removeFromDB)
		err = m_messageDB->DeleteMessage(keyOfDeletedMessage, &m_changeListener);
	return err;
}

MsgERR MessageDBView::InsertByIndex(MSG_ViewIndex index,
									MessageKey id)
{
	if (!IsValidIndex(index))
		return eInvalidIndex;
	m_idArray.InsertAt(index, id, 1);
	m_flags.InsertAt(index, 0, 1);
	m_levels.InsertAt(index, 0, 1);
	return eSUCCESS;
}

// view modifications methods by ID
MsgERR MessageDBView::SetReadByID(MessageKey  id, XP_Bool read, MSG_ViewIndex* pIndex) 
{
	MSG_ViewIndex viewIndex = (MSG_ViewIndex) FindViewIndex(id);
	if (pIndex != NULL)
		*pIndex = viewIndex;

	return SetReadByIndex(viewIndex, read);
}

// This should insert a new id after the passed in id, and returns the index
// at which it inserted it. Only write if needed.
MsgERR MessageDBView::InsertByID(MessageKey /*id*/,
								 MessageKey /*newId*/,
								 MSG_ViewIndex* /*pIndex*/)
{
	return eNYI;
}

void MessageDBView::NoteChange(MSG_ViewIndex firstlineChanged, int numChanged, 
							   MSG_NOTIFY_CODE changeType)
{
	NotifyViewChangeAll(firstlineChanged, numChanged, changeType, &m_changeListener);
}

void MessageDBView::NoteStartChange(MSG_ViewIndex firstlineChanged, int numChanged, 
						   MSG_NOTIFY_CODE changeType)
{
	NotifyViewStartChangeAll(firstlineChanged, numChanged, changeType, &m_changeListener);
}

void MessageDBView::NoteEndChange(MSG_ViewIndex firstlineChanged, int numChanged, 
						   MSG_NOTIFY_CODE changeType)
{
	NotifyViewEndChangeAll(firstlineChanged, numChanged, changeType, &m_changeListener);
}


XP_Bool MessageDBView::IsValidIndex(MSG_ViewIndex index)
{
	return (index < (MSG_ViewIndex) m_idArray.GetSize());
}

MsgERR MessageDBView::OrExtraFlag(MSG_ViewIndex index, char orflag)
{
	char	flag;
	if (!IsValidIndex(index))
		return eInvalidIndex;
	flag = m_flags[index];
	flag |= orflag;
	m_flags[index] = flag;
	OnExtraFlagChanged(index, flag);
	return eSUCCESS;
}

MsgERR MessageDBView::AndExtraFlag(MSG_ViewIndex index, char andflag)
{
	char	flag;
	if (!IsValidIndex(index))
		return eInvalidIndex;
	flag = m_flags[index];
	flag &= andflag;
	m_flags[index] = flag;
	OnExtraFlagChanged(index, flag);
	return eSUCCESS;
}

MsgERR MessageDBView::SetExtraFlag(MSG_ViewIndex index, char extraflag)
{
	if (!IsValidIndex(index))
		return eInvalidIndex;
	m_flags[index] = extraflag;
	OnExtraFlagChanged(index, extraflag);
	return eSUCCESS;
}

MsgERR MessageDBView::GetExtraFlag(MSG_ViewIndex index, 
										   char *extraFlag)
{
	if (!IsValidIndex(index))
		return eInvalidIndex;
	*extraFlag = m_flags[index];
	return eSUCCESS;
}


void MessageDBView::SetExtraFlagsFromDBFlags(uint32 messageFlags, MSG_ViewIndex index)
{
	XP_ASSERT(IsValidIndex(index));
	char	flags = m_flags[index];
	char	origFlags = flags;
	char	saveFlags;

	// save extra flags not stored in db, and restore them below.
	// make a mask of the state of the extra flags
	saveFlags = flags & (kHasChildren | kIsThread | kElided);
	CopyDBFlagsToExtraFlags(messageFlags, &flags);
	
	// no matter what CopyDBFlagsToExtraFlags did
	// clear the three extra flags
	flags &= ~(kHasChildren | kIsThread | kElided);
	
		// use the mask to restore the state of the three flags
	flags |= saveFlags;
	m_flags.SetAt(index, flags);

	if (m_sortType == SortByThread && (flags & kIsRead != origFlags & kIsRead))	// to update unread counts on thread headers.
	{
		MSG_ViewIndex threadIndex = ThreadIndexOfMsg(m_idArray[index], index, NULL, NULL);
		if (threadIndex != index)
			NoteChange(threadIndex, 1, MSG_NotifyChanged);
	}


}

// Copy	flags from extra array byte into DBMessageHdr flags. These flags may not have
// the same bit positions, although they do now. The message flags are currently 32 bits, and
// the extra flags just 8. Also, this routine takes a pointer to messageFlags so that it
// can just change the bits it knows about. 
// These are the DB flags, not the MSG_FLAG flags.
void	MessageDBView::CopyExtraFlagsToDBFlags(char flags, uint32 *messageFlags)
{
	if (flags & kElided)
		*messageFlags |= kElided;
	else
		*messageFlags &= ~kElided;

	if (flags & kExpired)
		*messageFlags |= kExpired;
	else
		*messageFlags &= ~kExpired;

	if (flags & kIsRead)
		*messageFlags |= kIsRead;
	else
		*messageFlags &= ~kIsRead;
	if (flags & kHasChildren)
		*messageFlags |= kHasChildren;
	else
		*messageFlags &= ~kHasChildren;
	if (flags & kIsThread)
		*messageFlags |= kIsThread;
	else
		*messageFlags &= ~kIsThread;
	if (flags & kMsgMarked)
		*messageFlags |= kMsgMarked;
	else
		*messageFlags &= ~kMsgMarked;
}

void MessageDBView::CopyExtraFlagsToPublicFlags(char flags, uint32 *messageFlags)
{
	uint32	extraFlags = 0;

	// Then turn off the possible bits in the result.
	*messageFlags &= ~m_publicEquivOfExtraFlags;
	// Now, convert the flags that are on, and turn the 32 bit public 
	// equivalent on in the result.
	CopyExtraFlagsToDBFlags(flags, &extraFlags);
	MessageDB::ConvertDBFlagsToPublicFlags(&extraFlags);
	*messageFlags |= extraFlags;
}

void MessageDBView::CopyDBFlagsToExtraFlags(uint32 messageFlags, char *extraFlags)
{
	if (messageFlags & kIsRead)
		*extraFlags |= kIsRead;
	else
		*extraFlags &= ~kIsRead;

	if (messageFlags & kExpired)
		*extraFlags |= kExpired;
	else
		*extraFlags &= ~kExpired;

	if (messageFlags & kHasChildren)
		*extraFlags |= kHasChildren;
	else
		*extraFlags &= ~kHasChildren;

	if (messageFlags & kElided)
		*extraFlags |= kElided;
	else
		*extraFlags &= ~kElided;
	if (messageFlags & kIsThread)
		*extraFlags |= kIsThread;
	else
		*extraFlags &= ~kIsThread;
	if (messageFlags & kMsgMarked)
		*extraFlags |= kMsgMarked;
	else
		*extraFlags &= ~kMsgMarked;
	if (messageFlags & kOffline)
		*extraFlags |= kOffline;
	else
		*extraFlags &= ~kOffline;
}

/* Cache of open view objects allows us to enforce one open view per DB */

XP_List *MessageDBView::m_viewCache = NULL;

MessageDBView *MessageDBView::CacheLookup (MessageDB *pDB, ViewType type)
{
	if (m_viewCache)
	{
		XP_List *walkCache = m_viewCache;

		MessageDBView *walkView = 0;
		do
		{
			walkView = (MessageDBView*) XP_ListNextObject(walkCache);
			if (walkView && walkView->GetDB() == pDB)
			{
				// dbShowingIgnored doesn't matter if pDB is NULL, so FALSE is OK.
				XP_Bool dbShowingIgnored = (pDB) 
					? (pDB->GetDBFolderInfo()->GetFlags() & MSG_FOLDER_PREF_SHOWIGNORED)
					: FALSE;
				if (( walkView->m_viewType == type && !!walkView->GetShowingIgnored() == !!dbShowingIgnored) || type == ViewAny || !pDB)
					return walkView;
			}
		} while (walkView);
	}
	return NULL;
}

void MessageDBView::CacheAdd ()
{
	// allocate list when needed
	if (!m_viewCache)
		m_viewCache = XP_ListNew();

	// can't already exist in cache
	XP_ASSERT (!CacheLookup(m_messageDB, m_viewType)); 

	XP_ListAddObject (m_viewCache, this);
}

void MessageDBView::CacheRemove ()
{
	// must already exist in cache
	XP_ASSERT (CacheLookup (m_messageDB, m_viewType)); 

	XP_ListRemoveObject (m_viewCache, this);

	// delete list when empty
	if (XP_ListCount (m_viewCache) == 0)
	{
		XP_ListDestroy (m_viewCache);
		m_viewCache = NULL;	// isn't this required? dmb 
	}
}

int32 MessageDBView::AddReference ()
{
	m_messageDB->AddUseCount ();
	return ++m_refCount;
}

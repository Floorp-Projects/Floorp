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
#include "xp.h"
#include "msgdb.h"
#include "thrdbvw.h"
#include "errcode.h"
#include "msghdr.h"
#include "dberror.h"
#include "newsdb.h"
#include "vwerror.h"
#include "thrhead.h"
#include "newshdr.h"
#include "grpinfo.h"

ThreadDBView::ThreadDBView(ViewType viewType)
{
	m_viewType = viewType;
	m_havePrevView = FALSE;
	m_headerIndex = 0;
	m_viewFlags = kOutlineDisplay;
	SetInitialSortState();
}

ThreadDBView::~ThreadDBView()
{
}

MsgERR ThreadDBView::Open(MessageDB *messageDB, ViewType viewType, uint32* pCount, XP_Bool runInForeground /* = TRUE */)
{
	MsgERR err;
	if ((err = MessageDBView::Open(messageDB, viewType, pCount, runInForeground)) != eSUCCESS)
		return err;

	if (pCount)
		*pCount = 0;
	return Init(pCount, runInForeground);
}

// Initialize id array and flag array in thread order.
MsgERR ThreadDBView::Init(uint32 *pCount, XP_Bool runInForeground /* = TRUE */)
{
	MsgERR err;
	SortOrder sortOrder;
	SortType sortType;

	m_idArray.RemoveAll();
	m_flags.RemoveAll();
	m_levels.RemoveAll();
	m_prevIdArray.RemoveAll();
	m_prevFlags.RemoveAll();
	m_prevLevels.RemoveAll();
	m_havePrevView = FALSE;
	m_sortType = SortByThread;	// this sorts by thread, which we will resort.
	sortType = m_sortType;
	MsgERR getSortInfoERR = m_messageDB->GetSortInfo(&sortType, &sortOrder);

	if (!runInForeground)
		return eBuildViewInBackground;

	// list all the ids into m_idArray.
	MessageKey startMsg = 0; 
	do
	{
		const int kIdChunkSize = 200;
		int			numListed = 0;
		MessageKey	idArray[kIdChunkSize];
		int32		flagArray[kIdChunkSize];
		char		levelArray[kIdChunkSize];

		err = m_messageDB->ListThreadIds(&startMsg, m_viewType == ViewOnlyNewHeaders, idArray, flagArray, levelArray, kIdChunkSize, &numListed, this, NULL);
		if (err == eSUCCESS)
		{
			int32 numAdded = AddKeys(idArray, flagArray, levelArray, sortType, numListed);
			if (pCount)
				*pCount += numAdded;
		}

	} while (err == eSUCCESS && startMsg != kIdNone);
	if (err == eCorruptDB)
	{
		m_messageDB->SetSummaryValid(FALSE); // get mail db and do this.
		err = eEXCEPTION;
	}
	if (getSortInfoERR == eSUCCESS)
	{
		InitSort(sortType, sortOrder);
	}

	return err;
}

int32 ThreadDBView::AddKeys(MessageKey *pOutput, int32 *pFlags, char *pLevels, SortType sortType, int numListed)
{
	int	numAdded = 0;
	for (int i = 0; i < numListed; i++)
	{
		int32 threadFlag = pFlags[i];
		char flag = 0;
		CopyDBFlagsToExtraFlags(threadFlag, 
									&flag);

		// skip ignored threads.
		if ((threadFlag & kIgnored) && !(m_viewFlags & kShowIgnored))
			continue;
		// by default, make threads collapsed, unless we're in only viewing new msgs

		if (flag & kHasChildren)
			flag |= kElided;
		// should this be persistent? Doesn't seem to need to be.
		flag |= kIsThread;
		m_idArray.Add(pOutput[i]);
		m_flags.Add(flag);
		m_levels.Add(pLevels[i]);
		numAdded++;
		if ((/*m_viewFlags & kUnreadOnly || */(sortType != SortByThread)) && flag & kElided)
		{
			ExpandByIndex(m_idArray.GetSize() - 1, NULL);
		}
	}
	return numAdded;
}

MsgERR	ThreadDBView::InitSort(SortType sortType, SortOrder sortOrder)
{
	if (sortType == SortByThread)
	{
		SortInternal(SortById, sortOrder); // sort top level threads by id.
		m_sortType = SortByThread;
		m_messageDB->SetSortInfo(m_sortType, sortOrder);
	}
	if ((m_viewFlags & kUnreadOnly) && m_sortType == SortByThread)
		ExpandAll();
	m_sortValid = TRUE;
	Sort(sortType, sortOrder);
	if (sortType != SortByThread)	// forget prev view, since it has everything expanded.
		ClearPrevIdArray();
	return eSUCCESS;
}


// override so we can forget any previous id array, which
// is made invalid by adding a header. (Same true for deleting... ### dmb)
MsgERR	ThreadDBView::AddHdr(DBMessageHdr *msgHdr)
{
	if (m_viewFlags & kUnreadOnly)
		msgHdr->AndFlags(~kElided);
	else
		msgHdr->OrFlags(kElided);	// by default, new headers are collapsed.
	return MessageDBView::AddHdr(msgHdr);
}

// This function merely parses the XOver header into a DBMessageHdr
// and stores that in m_headers. From there, it will be threaded
// and added to the database in a separate pass.
MsgERR	ThreadDBView::AddHdrFromServerLine(char *line, MessageKey *msgId)
{
	MsgERR	err = eSUCCESS;

	XP_ASSERT(msgId != NULL);

	NewsMessageHdr *newsMsgHdr = new NewsMessageHdr;
	// this will stick 0's in various places in line, but that's
	// the way 2.0 worked, so I guess it's OK.
	if (!DBMessageHdr::ParseLine(line, newsMsgHdr, m_messageDB->GetDB()))	
		err = eXOverParseError;

	if (err != eSUCCESS)
	{
		delete newsMsgHdr;
		return err;
	}

	*msgId = newsMsgHdr->GetMessageKey();
	if (m_messageDB->KeyToAddExists(*msgId))
	{
		delete newsMsgHdr;
		return eSUCCESS;
	}

	m_newHeaders.SetAtGrow(m_headerIndex++, newsMsgHdr);
	// this should be tuneable, but let's try every 200 objects
	if (m_headerIndex > 200)
		AddNewMessages();
	return eSUCCESS;
}

MsgERR ThreadDBView::AddNewMessages()
{
	MsgERR err = eSUCCESS;
	XP_Bool		isNewThread;
	NewsGroupDB	*newsDB = (NewsGroupDB *) m_messageDB;

	// go through the new headers adding them to the db
	// The idea here is that m_headers is just the new headers
	for (int i = 0; i < m_newHeaders.GetSize(); i++)
	{

		DBMessageHdr *dbMsgHdr = (DBMessageHdr *) m_newHeaders[i];
		if ((dbMsgHdr->GetFlags() & kHasChildren) && m_sortType == SortByThread)
			dbMsgHdr->OrFlags(kElided);

		err = newsDB->AddHdrToDB(dbMsgHdr, &isNewThread, TRUE);
		if (err == eCorruptDB)
		{
			newsDB->SetSummaryValid(FALSE);
			break;
		}
		delete dbMsgHdr;
		// Only add header to view if it's a new thread
//		if (err == eSUCCESS)	// notify param to AddHdrToDB should take care of this
//		{
//			if (m_sortType != SortByThread)
//			{
//				AddHdr(dbMsgHdr);
//			}
//			else if (isNewThread)
//			{
//				AddHdr(dbMsgHdr);
//			}
//		}
	}
	newsDB->Commit();
	m_headerIndex = 0;
	m_newHeaders.RemoveAll();
	return err;

}


void ThreadDBView::SetInitialSortState(void)
{
	m_sortOrder = SortTypeAscending; 
	m_sortType = SortByThread;
}

MsgERR	ThreadDBView::ExpandAll()
{
	MsgERR err = eSUCCESS;
	// go through expanding in place 
	for (uint32 i = 0; i < m_idArray.GetSize(); i++)
	{
		uint32	numExpanded;
		BYTE	flags = m_flags[i];
		if (flags & kHasChildren && (flags & kElided))
		{
			err = ExpandByIndex(i, &numExpanded);
			i += numExpanded;
			if (err != eSUCCESS)
				return err;
		}
	}
	return err;
}

MsgERR	ThreadDBView::Sort(SortType sortType, SortOrder sortOrder)
{
	// if the client wants us to forget our cached id arrays, they
	// should build a new view. If this isn't good enough, we
	// need a method to do that.
	if (sortType != m_sortType || !m_sortValid)
	{
		if (sortType == SortByThread)
		{
			m_sortType = sortType;
			if ( m_havePrevView)
			{
				// restore saved id array and flags array
				m_idArray.RemoveAll();
				m_idArray.InsertAt(0, &m_prevIdArray);
				m_flags.RemoveAll();
				m_flags.InsertAt(0, &m_prevFlags);
				m_levels.RemoveAll();
				m_levels.InsertAt(0, &m_prevLevels);
				m_messageDB->SetSortInfo(sortType, sortOrder);
				m_sortValid = TRUE;
				return eSUCCESS;
			}
			else
			{
				// set sort info in anticipation of what Init will do.
				m_messageDB->SetSortInfo(sortType, SortTypeAscending);
				Init(NULL);	// build up thread list.
				if (sortOrder != SortTypeAscending)
					Sort(sortType, sortOrder);
				return eSUCCESS;
			}
		}
		else if (sortType  != SortByThread && m_sortType == SortByThread /* && !m_havePrevView*/)
		{
		// going from SortByThread to non-thread sort - must build new id array and flags array
			m_prevIdArray.RemoveAll();
			m_prevIdArray.InsertAt(0, &m_idArray);
			m_prevFlags.RemoveAll();
			m_prevFlags.InsertAt(0, &m_flags);
			m_prevLevels.RemoveAll();
			m_prevLevels.InsertAt(0, &m_levels);
			ExpandAll();
//			m_idArray.RemoveAll();
//			m_flags.RemoveAll();
			m_havePrevView = TRUE;
		}
	}
	return SortInternal(sortType, sortOrder);
}

void ThreadDBView::OnHeaderAddedOrDeleted()
{
	ClearPrevIdArray();
}

void	ThreadDBView::OnExtraFlagChanged(MSG_ViewIndex index, char extraFlag)
{
	if (IsValidIndex(index) && m_havePrevView)
	{
		MessageKey keyChanged = m_idArray[index];
		MSG_ViewIndex prevViewIndex = m_prevIdArray.FindIndex(keyChanged);
		if (prevViewIndex != MSG_VIEWINDEXNONE)
		{
			char prevFlag = m_prevFlags.GetAt(prevViewIndex);
			// don't want to change the elided bit, or has children or is thread
			if (prevFlag & kElided)
				extraFlag |= kElided;
			else
				extraFlag &= ~kElided;
			if (prevFlag & kIsThread)
				extraFlag |= kIsThread;
			else
				extraFlag &= ~kIsThread;
			if (prevFlag & kHasChildren)
				extraFlag |= kHasChildren;
			else
				extraFlag &= ~kHasChildren;
			m_prevFlags.SetAt(prevViewIndex, extraFlag);	// will this be right?
		}
	}
}

void ThreadDBView::ClearPrevIdArray()
{
	m_prevIdArray.RemoveAll();
	m_prevFlags.RemoveAll();
	m_havePrevView = FALSE;
}

MSG_ViewIndex
ThreadDBView::GetInsertInfoForNewHdr(MessageKey newKey,
									 MSG_ViewIndex startThreadIndex,
									 int8 *pLevel)
{
	DBThreadMessageHdr *threadHdr = m_messageDB->GetDBThreadHdrForMsgID(newKey);
	MSG_ViewIndex threadIndex = 0;
	int i;

	if (m_viewFlags & kUnreadOnly)
	{
		uint8 levelToAdd;
		XPByteArray levelStack;
		for (i = 0; i < threadHdr->GetNumChildren(); i++)
		{
			DBMessageHdr *msgHdr = threadHdr->GetChildHdrAt(i);
			if (msgHdr != NULL)
			{
				// if the current header's level is <= to the top of the level stack,
				// pop off the top of the stack.
				// unreadonly - The level stack needs to work across calls
				// to this routine, in the case that we have more than 200 unread
				// messages in a thread.
				while (levelStack.GetSize() > 1 && 
							msgHdr->GetLevel()  <= levelStack.GetAt(levelStack.GetSize() - 1))
				{
					levelStack.RemoveAt(levelStack.GetSize() - 1);
				}

				if (msgHdr->GetMessageKey() == newKey)
				{
					if (levelStack.GetSize() == 0)
						levelToAdd = 0;
					else
						levelToAdd = levelStack.GetAt(levelStack.GetSize() - 1) + 1;
					delete msgHdr;

					if (pLevel)
						*pLevel = levelToAdd;
					break;
				}
				if ((startThreadIndex + threadIndex) < m_idArray.GetSize() 
					&& msgHdr->GetMessageKey()  == m_idArray[startThreadIndex + threadIndex])
				{
					levelStack.Add(m_levels[startThreadIndex + threadIndex]);
					threadIndex++;
				}
				delete msgHdr;
			}
		}
	}
	else
	{
		for (i = 0; i < threadHdr->GetNumChildren(); i++)
		{
			MessageKey msgKey = threadHdr->GetChildAt(i);
			if (msgKey == newKey)
			{
				if (pLevel)
				{
					DBMessageHdr  *msgHdr = threadHdr->GetChildHdrAt(i);
					*pLevel = msgHdr->GetLevel();
					delete msgHdr;
				}
				break;
			}
			if (msgKey == m_idArray[startThreadIndex + threadIndex])
				threadIndex++;
		}
	}
	return threadIndex + startThreadIndex;
}

MsgERR ThreadDBView::OnNewHeader(MessageKey newKey, XP_Bool ensureListed)
{
	MsgERR	err = eSUCCESS;
	// views can override this behaviour, which is to append to view.
	// This is the mail behaviour, but threaded views might want
	// to insert in order...
	DBMessageHdr *msgHdr = m_messageDB->GetDBHdrForKey(newKey);
	if (msgHdr != NULL)
	{
		if (m_viewType == ViewOnlyNewHeaders && !ensureListed && (msgHdr->GetFlags() & kIsRead))
			return eSUCCESS;
		// Currently, we only add the header in a threaded view if it's a thread.
		// We used to check if this was the first header in the thread, but that's
		// a bit harder in the unreadOnly view. But we'll catch it below.
		if (m_sortType != SortByThread)// || msgHdr->GetMessageKey() == m_messageDB->GetKeyOfFirstMsgInThread(msgHdr->GetMessageKey()))
			err = AddHdr(msgHdr);
		else	// need to find the thread we added this to so we can change the hasnew flag
				// added message to existing thread, but not to view
		{		// Fix flags on thread header.
			int32 threadCount;
			uint32 threadFlags;
			MSG_ViewIndex threadIndex = ThreadIndexOfMsg(newKey, kViewIndexNone, &threadCount, &threadFlags);
			if (threadIndex != MSG_VIEWINDEXNONE)
			{
				// check if this is now the new thread hdr
				char	flags = m_flags[threadIndex];
				// if we have a collapsed thread which just got a new
				// top of thread, change the id array.
				if ((flags & kElided) && msgHdr->GetLevel() == 0 
					&& (!(m_viewFlags & kUnreadOnly) || !(msgHdr->GetFlags() & kIsRead)))
				{
					m_idArray.SetAt(threadIndex, msgHdr->GetMessageKey());
					NoteChange(threadIndex, 1, MSG_NotifyChanged);
				}
				if (! (flags & kHasChildren))
				{
					flags |= kHasChildren | kIsThread;
					if (!(m_viewFlags & kUnreadOnly))
						flags |= kElided;
					m_flags[threadIndex] = flags;
					NoteChange(threadIndex, 1, MSG_NotifyChanged);
				}
				if (! (flags & kElided))	// thread is expanded
				{								// insert child into thread
					int8 level;					// levels of other hdrs may have changed!
					char	newFlags = 0;

					MSG_ViewIndex insertIndex = GetInsertInfoForNewHdr(newKey, threadIndex, &level);
					// this header is the new top of the thread. try collapsing the existing thread,
					// removing it, installing this header as top, and expanding it.
					CopyDBFlagsToExtraFlags(msgHdr->GetFlags(), &newFlags);
					if (level == 0)	
					{
						CollapseByIndex(threadIndex, NULL);
						// call base class, so child won't get promoted.
						MessageDBView::RemoveByIndex(threadIndex);	
						newFlags |= kIsThread | kHasChildren | kElided;
					}
					m_idArray.InsertAt(insertIndex, newKey);
					m_flags.InsertAt(insertIndex, newFlags, 1);
					m_levels.InsertAt(insertIndex, level);
					NoteChange(threadIndex, 1, MSG_NotifyChanged);
					NoteChange(insertIndex, 1, MSG_NotifyInsertOrDelete);
					if (level == 0)
						ExpandByIndex(threadIndex, NULL);
				}
			}
			else 
			{
				DBThreadMessageHdr *threadHdr = m_messageDB->GetDBThreadHdrForMsgID(newKey);
				if (threadHdr)
				{
					switch (m_viewType)
					{
					case ViewOnlyThreadsWithNew:
						{
							DBMessageHdr *parentHdr = 	GetFirstMessageHdrToDisplayInThread(threadHdr);
							if (parentHdr && (ensureListed || !(msgHdr->GetFlags() & kIsRead)))
							{
								parentHdr->OrFlags(kHasChildren | kIsThread);
								if (!(m_viewFlags & kUnreadOnly))
									parentHdr->OrFlags(kElided);
								AddHdr(parentHdr);
							}
							delete parentHdr;
						}
						break;
					case  ViewWatchedThreadsWithNew:
						if (threadHdr->GetFlags() & kWatched)
						{
							DBMessageHdr *parentHdr = 	GetFirstMessageHdrToDisplayInThread(threadHdr);
							if (parentHdr && (ensureListed || !(msgHdr->GetFlags() & kIsRead)))
							{
								parentHdr->OrFlags(kElided | kHasChildren | kIsThread);
								AddHdr(parentHdr);
							}
							delete parentHdr;
						}
						break;
					case ViewOnlyNewHeaders:	
					case ViewAllThreads:
						if (!(threadHdr->GetFlags() & kIgnored))
							AddHdr(msgHdr);
						break;
					default:
						AddHdr(msgHdr);

					}
					delete threadHdr;
				}
			}
		}
		delete msgHdr;
	}
	else
		err = eID_NOT_FOUND;
	return err;
}

// This method just removes the specified line from the view. It does
// NOT delete it from the database.
MsgERR ThreadDBView::RemoveByIndex(MSG_ViewIndex index)
{
	MsgERR err = eSUCCESS;
	int32	flags;

	if (!IsValidIndex(index))
		return eInvalidIndex;

	flags = m_flags[index];

	if (m_sortType != SortByThread)
		return MessageDBView::RemoveByIndex(index);

	if ((flags & kIsThread) && !(flags & kElided) && (flags & kHasChildren))
	{
		// fix flags on thread header...Newly promoted message 
		// should have flags set correctly
		DBThreadMessageHdr *threadHdr = m_messageDB->GetDBThreadHdrForMsgID(m_idArray[index]);
		if (threadHdr)
		{
			MessageDBView::RemoveByIndex(index);
			DBThreadMessageHdr *nextThreadHdr = (index < GetSize()) 
				? m_messageDB->GetDBThreadHdrForMsgID(m_idArray[index]) : 0;
			// make sure id of next message is really in the same thread
			// it might have been deleted from the view but not the db yet.
			if (threadHdr == nextThreadHdr && threadHdr->GetNumChildren() > 1)
			{
				// unreadOnly
				DBMessageHdr *msgHdr = threadHdr->GetChildHdrAt(1);
				if (msgHdr != NULL)
				{
					char flag = 0;
					CopyDBFlagsToExtraFlags(msgHdr->GetFlags(), &flag);
					m_flags.SetAtGrow(index, (uint8) flags);
					m_levels.SetAtGrow(index, 0);
					delete msgHdr;
				}
			}
			if (nextThreadHdr)
				delete nextThreadHdr;
			delete threadHdr;
		}
		return err;
	}
	else if (!(flags & kIsThread))
	{
		return MessageDBView::RemoveByIndex(index);
	}
	// deleting collapsed thread header is special case. Child will be promoted,
	// so just tell FE that line changed, not that it was deleted
	DBThreadMessageHdr *threadHdr = m_messageDB->GetDBThreadHdrForMsgID(m_idArray[index]);
	if (threadHdr && threadHdr->GetNumChildren() > 1)
	{
		// change the id array and flags array to reflect the child header.
		// If we're not deleting the header, we want the second header,
		// Otherwise, the first one (which just got promoted).
		DBMessageHdr *msgHdr = threadHdr->GetChildHdrAt(1);
		if (msgHdr != NULL)
		{
			m_idArray.SetAt(index, msgHdr->GetMessageKey());

			char flag = 0;
			CopyDBFlagsToExtraFlags(msgHdr->GetFlags(), &flag);
//			if (msgHdr->GetArticleNum() == msgHdr->GetThreadId())
				flag |= kIsThread;

			if (threadHdr->GetNumChildren() == 2)	// if only hdr in thread (with one about to be deleted)
													// adjust flags.
			{
				flag &=  ~kHasChildren;
				flag &= ~kElided;
			}
			else
			{
				flag |= kHasChildren;
				flag |= kElided;
			}
			m_flags[index] = flag;
			delete msgHdr;
		}
		else
			XP_ASSERT(FALSE);	
		NoteChange(index, 0, MSG_NotifyInsertOrDelete);	// hack to tell fe that the key has changed
	}
	else
		err = MessageDBView::RemoveByIndex(index);

	delete threadHdr;
	return err;
}


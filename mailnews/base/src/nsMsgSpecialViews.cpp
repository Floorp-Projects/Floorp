/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#include "msgCore.h"
#include "nsMsgSpecialViews.h"
#include "nsIMsgThread.h"

nsMsgThreadsWithUnreadDBView::nsMsgThreadsWithUnreadDBView()
{
}

nsMsgThreadsWithUnreadDBView::~nsMsgThreadsWithUnreadDBView()
{
}

NS_IMETHODIMP nsMsgThreadsWithUnreadDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{
  return nsMsgThreadedDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
}

NS_IMETHODIMP nsMsgThreadsWithUnreadDBView::GetViewType(nsMsgViewTypeValue *aViewType)
{
    NS_ENSURE_ARG_POINTER(aViewType);
    *aViewType = nsMsgViewType::eShowThreadsWithUnread;
    return NS_OK;
}

PRBool nsMsgThreadsWithUnreadDBView::WantsThisThread(nsIMsgThread *threadHdr)
{
	if (threadHdr)
  {
    PRUint32 numNewChildren;

    threadHdr->GetNumUnreadChildren(&numNewChildren);
    if (numNewChildren > 0) 
      return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult nsMsgThreadsWithUnreadDBView::AddMsgToThreadNotInView(nsIMsgThread *threadHdr, nsIMsgDBHdr *msgHdr, PRBool ensureListed)
{
  nsresult rv = NS_OK;

  nsCOMPtr <nsIMsgDBHdr> parentHdr;
  PRUint32 msgFlags;
  PRUint32 newFlags;
  msgHdr->GetFlags(&msgFlags);
  GetFirstMessageHdrToDisplayInThread(threadHdr, getter_AddRefs(parentHdr));
	if (parentHdr && (ensureListed || !(msgFlags & MSG_FLAG_READ)))
	{
		parentHdr->OrFlags(MSG_VIEW_FLAG_HASCHILDREN | MSG_VIEW_FLAG_ISTHREAD, &newFlags);
		if (!(m_viewFlags & nsMsgViewFlagsType::kUnreadOnly))
			parentHdr->OrFlags(MSG_FLAG_ELIDED, &newFlags);
		rv = AddHdr(parentHdr);
	}
  return rv;
}

NS_IMETHODIMP nsMsgWatchedThreadsWithUnreadDBView::GetViewType(nsMsgViewTypeValue *aViewType)
{
    NS_ENSURE_ARG_POINTER(aViewType);
    *aViewType = nsMsgViewType::eShowWatchedThreadsWithUnread;
    return NS_OK;
}

PRBool nsMsgWatchedThreadsWithUnreadDBView::WantsThisThread(nsIMsgThread *threadHdr)
{
	if (threadHdr)
  {
    PRUint32 numNewChildren;
    PRUint32 threadFlags;

    threadHdr->GetNumUnreadChildren(&numNewChildren);
    threadHdr->GetFlags(&threadFlags);
    if (numNewChildren > 0 && (threadFlags & MSG_FLAG_WATCHED) != 0) 
      return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult nsMsgWatchedThreadsWithUnreadDBView::AddMsgToThreadNotInView(nsIMsgThread *threadHdr, nsIMsgDBHdr *msgHdr, PRBool ensureListed)
{
  nsresult rv = NS_OK;
  PRUint32 threadFlags;
  PRUint32 msgFlags, newFlags;
  msgHdr->GetFlags(&msgFlags);
  threadHdr->GetFlags(&threadFlags);
  if (threadFlags & MSG_FLAG_WATCHED)
  {
    nsCOMPtr <nsIMsgDBHdr> parentHdr;
    GetFirstMessageHdrToDisplayInThread(threadHdr, getter_AddRefs(parentHdr));
    if (parentHdr && (ensureListed || !(msgFlags & MSG_FLAG_READ)))
    {
      parentHdr->OrFlags(MSG_FLAG_ELIDED | MSG_VIEW_FLAG_HASCHILDREN | MSG_VIEW_FLAG_ISTHREAD, &newFlags);
      rv = AddHdr(parentHdr);
    }
  }
  return rv;
}
#ifdef WE_DO_CACHELESS_VIEWS
// This view will initially be used for cacheless IMAP.
CachelessView::CachelessView(ViewType viewType)
{
	m_viewType = viewType;
	m_viewFlags = kThreadedDisplay;
	SetInitialSortState();
	m_folder = NULL;
	m_master = NULL;
	m_sizeInitialized = FALSE;
#ifdef DEBUG_bienvenu
	SetViewSize(1);	// start off with a size until we figure out how to hook up IMAP response to view.
#endif
}

CachelessView::~CachelessView()
{
}

MsgERR CachelessView::Open(MessageDB *messageDB, ViewType viewType, /*WORD viewFlags,*/ uint32* pCount, XP_Bool runInForeground)
{
	MsgERR err;
	if ((err = MessageDBView::Open(messageDB, viewType, pCount, runInForeground)) != eSUCCESS)
		return err;

	if (pCount)
		*pCount = 0;
	return Init(pCount, runInForeground);
}

MsgERR CachelessView::SetViewSize(int32 setSize)
{
	m_idArray.RemoveAll();
	// Initialize whole view as MSG_MESSAGEKEYNONE
	m_idArray.InsertAt(0, MSG_MESSAGEKEYNONE, setSize); 
	m_flags.InsertAt(0, 0, setSize);
	m_levels.InsertAt(0, 0, setSize);
	return eSUCCESS;
}

void CachelessView::ClearPendingIds()
{
	for (MSG_ViewIndex viewIndex = m_curStartSeq - 1; viewIndex < m_curEndSeq; viewIndex++)
	{
		if (GetAt(viewIndex) == kIdPending)
			SetKeyByIndex(viewIndex, kIdNone);
	}
}

/*static*/ void CachelessView::ExitFunction (URL_Struct * /* URL_s */, int /* status */, MWContext *context)
{
	// need to remove kIdPending from view
	MSG_Pane *pane = context->imapURLPane;
	if (pane)
	{
		MessageDBView *view = pane->GetMsgView();
		if (view && view->GetViewType() == 	ViewCacheless)
		{
			CachelessView *cachelessView = (CachelessView *) view;
			cachelessView->ClearPendingIds();
		}
	}
}

MsgERR	CachelessView::ListShortMsgHdrByIndex(MSG_ViewIndex startIndex, int numToList, MSG_MessageLine *pOutput, int *pNumListed)
{
	MsgERR err = eSUCCESS;

	XP_BZERO(pOutput, sizeof(*pOutput) * numToList);
	int i;
	for (i = 0; i < numToList && err == eSUCCESS; i++)
	{
		(pOutput + i)->messageKey = MSG_MESSAGEKEYNONE;
		if (i + startIndex < m_idArray.GetSize())
		{
			MessageKey keyToList = m_idArray.GetAt(i + startIndex);
			if (keyToList != kIdNone && keyToList != kIdPending)
			{
				err = m_messageDB->GetShortMessageHdr(m_idArray[i + startIndex], pOutput + i);
				if (err == eSUCCESS)
				{
					char	extraFlag = m_flags[i + startIndex];

					CopyExtraFlagsToPublicFlags(extraFlag, &((pOutput + i)->flags));
					(pOutput + i)->level = 0;
				}
			}
			else if (keyToList == kIdNone)
			{
				if (m_folder->GetType() == FOLDER_IMAPMAIL)
				{
					XPPtrArray	panes;
					MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) m_folder;
					m_curStartSeq = startIndex + 1; // imap sequence #'s seem to be 1 relative
					m_curEndSeq = m_curStartSeq + MAX(10, numToList);
					// What's a good chunk size to list? Try 10 for now, or numToList.
					if (m_curStartSeq + 1 < m_idArray.GetSize())
					{
						MessageKey  nextKeyToList = m_idArray.GetAt(m_curStartSeq + 1);
						if (nextKeyToList != kIdNone && nextKeyToList != kIdPending && m_curStartSeq > 0)
						{
							nextKeyToList = m_idArray.GetAt(startIndex - 1);
							if (nextKeyToList == kIdNone)
							{
								MSG_ViewIndex tempStart = m_curStartSeq;
								m_curStartSeq = m_curStartSeq -  MIN(10, m_curStartSeq - 1);
								m_curEndSeq = tempStart;
							}
						}
					}
					if (!m_sizeInitialized && imapFolder->GetMailboxSpec()->number_of_messages > 0)
					{
						SetViewSize(imapFolder->GetMailboxSpec()->number_of_messages);
						m_sizeInitialized = TRUE;
					}
					m_master->FindPanesReferringToFolder (m_folder, &panes);
					MSG_Pane *pane = (panes.GetSize() > 0) ? (MSG_Pane *) panes.GetAt(0) : (MSG_Pane *)NULL;
					if (m_curEndSeq > GetSize())
						m_curEndSeq = GetSize();
					if (pane)
					{
						char *imapFolderURL = imapFolder->SetupHeaderFetchUrl(pane, GetDB(), m_curStartSeq, m_curEndSeq);
						MSG_UrlQueue::AddUrlToPane(imapFolderURL, CachelessView::ExitFunction, pane, NET_SUPER_RELOAD);
					    FREEIF(imapFolderURL);
					}
				}
				
			}
		}
	}
	if (pNumListed != NULL)
		*pNumListed = i;

	return err;
}

MsgERR		CachelessView::AddNewMessages() 
{
	return eSUCCESS;

}

MsgERR	CachelessView::AddHdr(nsIMsgDBHdr *msgHdr)
{
	char	flags = 0;
#ifdef DEBUG_bienvenu
	XP_ASSERT((int) m_idArray.GetSize() == m_flags.GetSize() && (int) m_idArray.GetSize() == m_levels.GetSize());
#endif
	CopyDBFlagsToExtraFlags(msgHdr->GetFlags(), &flags);
	if (msgHdr->GetArticleNum() == msgHdr->GetThreadId())
		flags |= kIsThread;
	// if unreadonly, level is 0 because we must be the only msg in the thread.
	char levelToAdd = (m_viewFlags & kUnreadOnly) ? 0 : msgHdr->GetLevel();

	m_idArray.Add(msgHdr->getID());
	m_flags.Add(flags);
	m_levels.Add(levelToAdd);
	NoteChange(m_idArray.GetSize() - 1, 1, MSG_NotifyInsertOrDelete);

	OnHeaderAddedOrDeleted();
	return eSUCCESS;
}


// for news, xover line, potentially, for IMAP, imap line...
MsgERR		CachelessView::AddHdrFromServerLine(char * /*line*/, MessageKey * /*msgId*/)
{
	return eSUCCESS;
}

void CachelessView::SetInitialSortState(void)
{
	m_sortOrder = SortTypeAscending; 
	m_sortType = SortById;
}

MsgERR CachelessView::Init(uint32 * /*pCount*/, XP_Bool /* runInForeground */ /* = TRUE */)
{
	MsgERR ret = eSUCCESS; 
	m_sortType = SortById;	

	MailDB	*mailDB = NULL;

	if (GetDB())
		mailDB = GetDB()->GetMailDB();

	if (mailDB)
	{
		m_master = mailDB->GetMaster();
		if (m_master)
		{
			ENeoString	mailBoxName;

			mailDB->m_neoFolderInfo->GetMailboxName(mailBoxName);
			m_folder = m_master->FindImapMailFolder(mailBoxName);
		}
	}
	return ret;
}
#endif // WE_DO_CACHELESS_VIEW

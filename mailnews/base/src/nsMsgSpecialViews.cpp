/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    PRUint32 orFlags = MSG_VIEW_FLAG_ISTHREAD;
    PRUint32 numChildren;
    threadHdr->GetNumChildren(&numChildren);
    if (numChildren > 1)
      orFlags |= MSG_VIEW_FLAG_HASCHILDREN;

    parentHdr->OrFlags(orFlags, &newFlags);
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
      PRUint32 orFlags = MSG_VIEW_FLAG_ISTHREAD | MSG_FLAG_ELIDED;
      PRUint32 numChildren;
      threadHdr->GetNumChildren(&numChildren);
      if (numChildren > 1)
        orFlags |= MSG_VIEW_FLAG_HASCHILDREN;
      parentHdr->OrFlags(orFlags, &newFlags);
      rv = AddHdr(parentHdr);
    }
  }
  return rv;
}

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
#include "nsMsgThreadedDBView.h"
#include "nsIMsgHdr.h"
#include "nsIMsgThread.h"

nsMsgThreadedDBView::nsMsgThreadedDBView()
{
  /* member initializers and constructor code */
	m_havePrevView = PR_FALSE;
}

nsMsgThreadedDBView::~nsMsgThreadedDBView()
{
  /* destructor code */
}

NS_IMETHODIMP nsMsgThreadedDBView::Open(nsIMsgDatabase *msgDB, nsMsgViewSortType *viewType, PRInt32 *pCount)
{
	nsresult rv;;
	rv = nsMsgDBView::Open(msgDB, viewType, pCount);
  NS_ENSURE_SUCCESS(rv, rv);

	if (pCount)
		*pCount = 0;
	return Init(pCount);
}

NS_IMETHODIMP nsMsgThreadedDBView::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgThreadedDBView::Init(PRInt32 *pCount)
{
#ifndef DEBUG_seth
	nsresult rv;
	nsMsgViewSortOrder sortOrder;
	nsMsgViewSortTypeValue sortType;

	m_keys.RemoveAll();
	m_flags.RemoveAll();
//	m_levels.RemoveAll(); // ### need to add this method to nsByteArray
	m_prevIdArray.RemoveAll();
	m_prevFlags.RemoveAll();
//	m_prevLevels.RemoveAll();
	m_havePrevView = PR_FALSE;
    m_sortType = nsMsgViewSortType::byThread;	// this sorts by thread, which we will resort.
	sortType = m_sortType;
	nsresult getSortrv = NS_OK; // ### TODO m_db->GetSortInfo(&sortType, &sortOrder);


	// list all the ids into m_idArray.
	nsMsgKey startMsg = 0; 
	do
	{
		const int kIdChunkSize = 200;
		PRInt32			numListed = 0;
		nsMsgKey	idArray[kIdChunkSize];
		PRInt32		flagArray[kIdChunkSize];
		char		levelArray[kIdChunkSize];

    rv = ListThreadIds(&startMsg, m_viewType == nsMsgDBViewType::onlyUnreadHeaders, idArray, flagArray, 
                    levelArray, kIdChunkSize, &numListed, nsnull);
		if (NS_SUCCEEDED(rv))
		{
			PRInt32 numAdded = AddKeys(idArray, flagArray, levelArray, sortType, numListed);
			if (pCount)
				*pCount += numAdded;
		}

	} while (NS_SUCCEEDED(rv) && startMsg != nsMsgKey_None);

	if (NS_SUCCEEDED(getSortrv))
	{
		InitSort(sortType, sortOrder);
	}

	return rv;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP nsMsgThreadedDBView::AddKeys(nsMsgKey *pKeys, PRInt32 *pFlags, const char *pLevels, nsMsgViewSortType *sortType, PRInt32 numKeysToAdd)

{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgThreadedDBView::Sort(nsMsgViewSortType *sortType, nsMsgViewSortOrder *sortOrder)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



// list the ids of the top-level thread ids starting at id == startMsg. This actually returns
// the ids of the first message in each thread.
nsresult nsMsgThreadedDBView::ListThreadIds(nsMsgKey *startMsg, PRBool unreadOnly, nsMsgKey *pOutput, PRInt32 *pFlags, char *pLevels, 
									 PRInt32 numToList, PRInt32 *pNumListed, PRInt32 *pTotalHeaders)
{
#ifndef DEBUG_seth
	nsresult rv = NS_OK;
	// N.B..don't ret before assigning numListed to *pNumListed
	PRInt32				numListed = 0;

	if (*startMsg > 0)
	{
		NS_ASSERTION(m_threadEnumerator != nsnull, "where's our iterator?");	// for now, we'll just have to rely on the caller leaving
									// the iterator in the right place.
	}
	else
	{
    PRUint32 viewType;
    if (m_viewType == nsMsgDBViewType::allThreads)
      viewType = nsMsgViewType::eShowAll;
    else
      viewType = nsMsgViewType::eShowUnread;

    rv = m_db->EnumerateThreads(viewType, getter_AddRefs(m_threadEnumerator));
    NS_ENSURE_SUCCESS(rv, rv);
	}

	PRBool hasMore = PR_FALSE;

	while (NS_SUCCEEDED(rv = m_threadEnumerator->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
	{
    nsCOMPtr <nsISupports> supports;
    rv = m_threadEnumerator->GetNext(getter_AddRefs(supports));
    nsCOMPtr <nsIMsgThread> threadHdr = do_QueryInterface(supports);
    NS_ENSURE_SUCCESS(rv,rv);

		PRInt32	threadCount;
		PRInt32	threadsRemoved = 0;
		for (PRInt32 i = 0; i < numToList && threadHdr != nsnull; i++)
		{
    	nsCOMPtr <nsIMsgDBHdr> msgHdr;
      PRInt32 numChildren;
      threadHdr->GetNumChildren(&numChildren);
			if (numChildren != 0)	// not empty thread
			{
				if (pTotalHeaders)
					*pTotalHeaders += numChildren;
//	###	do this on branch		if (unreadOnly)
//					msgHdr = threadHdr->GetFirstUnreadChild(this);
//				else
					rv = threadHdr->GetChildAt(0, getter_AddRefs(msgHdr));
				PRUint32 threadFlags;
        threadHdr->GetFlags(&threadFlags);

				if (msgHdr != NULL && WantsThisThread(threadHdr))
				{
          nsMsgKey msgKey;
          msgHdr->GetMessageKey(&msgKey);
					pOutput[numListed] = msgKey;
					pLevels[numListed] = 0;
					// DMB TODO - This will do for now...Until we decide how to
					// handle thread flags vs. message flags, if we do decide
					// to make them different.
					msgHdr->OrFlags(threadFlags & (kWatched | kIgnored));
					PRBool	isRead = FALSE;

					// make sure DB agrees with newsrc, if we're news.
					IsRead(msgKey, &isRead);
					MarkHdrRead(msgHdr, isRead, NULL);
					// try adding in kIsThread flag for unreadonly view.
					pFlags[numListed] = msgHdr->m_flags | kIsThread | threadFlags;
					if (NS_SUCCEEDED(GetThreadCount(threadHdr, &threadCount)) && threadCount > 1)
						pFlags[numListed] |= kHasChildren;

					numListed++;
				}
			}
			else if (threadsRemoved < 10 && !(threadHdr->GetFlags() & (kWatched | kIgnored)))
			{
				// ### remove thread.
				threadsRemoved++;	// don't want to remove all empty threads first time
									// around as it will choke preformance for upgrade.
#ifdef DEBUG_bienvenu
				printf("removing empty non-ignored non-watched thread\n");
#endif
			}
			threadHdr = nextThreadHdr;
		}
	}	

	if (threadHdr != NULL)
	{
		*startMsg = threadHdr->fID;
	}
	else
	{
		*startMsg = nsMsgKey_None;
		m_threadEnumerator = nsnull;
	}
	*pNumListed = numListed;
	return rv;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}


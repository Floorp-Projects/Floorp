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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"
#include "nsMsgThread.h"
#include "nsMsgDatabase.h"
#include "nsCOMPtr.h"

NS_IMPL_ISUPPORTS(nsMsgThread, NS_GET_IID(nsMsgThread))

nsMsgThread::nsMsgThread()
{
	Init();
}
nsMsgThread::nsMsgThread(nsMsgDatabase *db, nsIMdbTable *table)
{
	Init();
	m_mdbTable = table;
	m_mdbDB = db;
	if (db)
		db->AddRef();

	if (table && db)
	{
		table->GetMetaRow(db->GetEnv(), nil, nil, &m_metaRow);
		InitCachedValues();
	}
}

void nsMsgThread::Init()
{
	m_threadKey = nsMsgKey_None; 
	m_threadRootKey = nsMsgKey_None;
	m_numChildren = 0;		
	m_numUnreadChildren = 0;	
	m_flags = 0;
	m_mdbTable = nsnull;
	m_mdbDB = nsnull;
	m_metaRow = nsnull;
	m_cachedValuesInitialized = PR_FALSE;
	NS_INIT_REFCNT();
}


nsMsgThread::~nsMsgThread()
{
	if (m_mdbTable)
		m_mdbTable->Release();
	if (m_mdbDB)
		m_mdbDB->Release();
	if (m_metaRow)
		m_metaRow->Release();
}

nsresult nsMsgThread::InitCachedValues()
{
	nsresult err = NS_OK;

	if (!m_mdbDB || !m_metaRow)
	    return NS_ERROR_NULL_POINTER;

	if (!m_cachedValuesInitialized)
	{
		err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadFlagsColumnToken, &m_flags);
	    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, &m_numChildren);
		err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadIdColumnToken, &m_threadKey);
	    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, &m_numUnreadChildren);
		err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadRootKeyColumnToken, &m_threadRootKey, nsMsgKey_None);
		
		if (NS_SUCCEEDED(err))
			m_cachedValuesInitialized = PR_TRUE;
	}
	return err;
}

NS_IMETHODIMP		nsMsgThread::SetThreadKey(nsMsgKey threadKey)
{
	nsresult ret = NS_OK;

	m_threadKey = threadKey;
	// by definition, the initial thread key is also the thread root key.
	SetThreadRootKey(threadKey);
	ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadIdColumnToken, threadKey);
	// gotta set column in meta row here.
	return ret;
}

NS_IMETHODIMP	nsMsgThread::GetThreadKey(nsMsgKey *result)
{
	if (result)
	{
		nsresult res = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadIdColumnToken, &m_threadKey);
		*result = m_threadKey;
		return res;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgThread::GetFlags(PRUint32 *result)
{
	if (result)
	{
		nsresult res = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadFlagsColumnToken, &m_flags);
		*result = m_flags;
		return res;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgThread::SetFlags(PRUint32 flags)
{
	nsresult ret = NS_OK;

	m_flags = flags;
	ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadFlagsColumnToken, m_flags);
	return ret;
}

NS_IMETHODIMP nsMsgThread::SetSubject(const char *subject)
{
	return m_mdbDB->CharPtrToRowCellColumn(m_metaRow, m_mdbDB->m_threadSubjectColumnToken, subject);
}

NS_IMETHODIMP nsMsgThread::GetSubject(char **result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;
	nsresult ret = m_mdbDB->RowCellColumnToCharPtr(m_metaRow, m_mdbDB->m_threadSubjectColumnToken, result);
	return ret;
}

NS_IMETHODIMP nsMsgThread::GetNumChildren(PRUint32 *result)
{
	if (result)
	{
		*result = m_numChildren;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsMsgThread::GetNumUnreadChildren (PRUint32 *result)
{
	if (result)
	{
		*result = m_numUnreadChildren;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgThread::AddChild(nsIMsgDBHdr *child, nsIMsgDBHdr *inReplyTo, PRBool threadInThread, 
										nsIDBChangeAnnouncer *announcer)
{
	nsresult ret = NS_OK;
    nsMsgHdr* hdr = NS_STATIC_CAST(nsMsgHdr*, child);          // closed system, cast ok
	PRUint32 newHdrFlags = 0;
	nsMsgKey newHdrKey = 0;
	PRBool parentKeyNeedsSetting = PR_TRUE;
	
	nsIMdbRow *hdrRow = hdr->GetMDBRow();
	hdr->GetFlags(&newHdrFlags);
	hdr->GetMessageKey(&newHdrKey);


	PRUint32 numChildren;
	PRUint32 childIndex = 0;

	// get the num children before we add the new header.
	GetNumChildren(&numChildren);

	if (m_mdbTable)
	{
		m_mdbTable->AddRow(m_mdbDB->GetEnv(), hdrRow);
		ChangeChildCount(1);
		if (! (newHdrFlags & MSG_FLAG_READ))
			ChangeUnreadChildCount(1);
	}
	if (inReplyTo)
	{
		nsMsgKey parentKey;
		inReplyTo->GetMessageKey(&parentKey);
		child->SetThreadParent(parentKey);
		parentKeyNeedsSetting = PR_FALSE;
	}
	// check if this header is a parent of one of the messages in this thread

	nsCOMPtr <nsIMsgDBHdr> curHdr;
	for (childIndex = 0; childIndex < numChildren; childIndex++)
	{
		nsMsgKey msgKey;

		ret = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
		if (NS_SUCCEEDED(ret) && curHdr)
		{
			if (hdr->IsParentOf(curHdr))
			{
				nsMsgKey oldThreadParent;
				curHdr->GetThreadParent(&oldThreadParent);
				curHdr->GetMessageKey(&msgKey);
				curHdr->SetThreadParent(newHdrKey);
				if (msgKey == newHdrKey)
					parentKeyNeedsSetting = PR_FALSE;

				// OK, this is a reparenting - need to send notification
				if (announcer)
					announcer->NotifyParentChangedAll(msgKey, oldThreadParent, newHdrKey, nsnull);
#ifdef DEBUG_bienvenu1
				if (newHdrKey != m_threadKey)
					printf("adding second level child\n");
#endif
				// If this hdr was the root, then the new hdr is the root.
				if (msgKey == m_threadRootKey)
				{
					SetThreadRootKey(newHdrKey);
					parentKeyNeedsSetting = PR_FALSE;
				}
			}
		}
	}
	// If this header is not a reply to a header in the thread, and isn't a parent
	// check to see if it starts with Re: - if not, and the first header does start
	// with re, should we make this header the top level header?
	// If it's date is less (or it's ID?), then yes.
	if (numChildren > 0 && !(newHdrFlags & MSG_FLAG_HAS_RE) && !inReplyTo)
	{
		PRTime newHdrDate;
		PRTime topLevelHdrDate;

		nsCOMPtr <nsIMsgDBHdr> topLevelHdr;
		ret = GetRootHdr(nsnull, getter_AddRefs(topLevelHdr));
		if (NS_SUCCEEDED(ret) && topLevelHdr)
		{
			child->GetDate(&newHdrDate);
			topLevelHdr->GetDate(&topLevelHdrDate);
			if (LL_CMP(newHdrDate, <, topLevelHdrDate))
			{
				mdb_pos outPos;
				m_mdbTable->MoveRow(m_mdbDB->GetEnv(), hdrRow, -1, 0, &outPos);
				topLevelHdr->SetThreadParent(newHdrKey);
				parentKeyNeedsSetting = PR_FALSE;
				SetThreadRootKey(newHdrKey);
				child->SetThreadParent(nsMsgKey_None);
				// argh, here we'd need to adjust all the headers that listed 
				// the demoted header as their thread parent, but only because
				// of subject threading. Adjust them to point to the new parent,
				// that is.
				ReparentNonReferenceChildrenOf(topLevelHdr, newHdrKey, announcer);
			}
		}
	}
	// OK, check to see if we added this header, and didn't parent it.

	if (numChildren > 0 && parentKeyNeedsSetting)
		child->SetThreadParent(m_threadRootKey);

	return ret;
}

nsresult	nsMsgThread::ReparentNonReferenceChildrenOf(nsIMsgDBHdr *topLevelHdr, nsMsgKey newParentKey,
														nsIDBChangeAnnouncer *announcer)
{
	nsCOMPtr <nsIMsgDBHdr> curHdr;
	PRUint32 numChildren;
	PRUint32 childIndex = 0;

	GetNumChildren(&numChildren);
	for (childIndex = 0; childIndex < numChildren; childIndex++)
	{
		nsMsgKey msgKey;

		topLevelHdr->GetMessageKey(&msgKey);
		nsresult ret = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
		if (NS_SUCCEEDED(ret) && curHdr)
		{
			nsMsgKey oldThreadParent;
			nsIMsgDBHdr *curMsgHdr = curHdr;
	        nsMsgHdr* topLevelMsgHdr = NS_STATIC_CAST(nsMsgHdr*, curMsgHdr);      // closed system, cast ok
			curHdr->GetThreadParent(&oldThreadParent);
			if (oldThreadParent == msgKey && !topLevelMsgHdr->IsParentOf(curHdr))
			{
				curHdr->GetThreadParent(&oldThreadParent);
				curHdr->GetMessageKey(&msgKey);
				curHdr->SetThreadParent(newParentKey);
				// OK, this is a reparenting - need to send notification
				if (announcer)
					announcer->NotifyParentChangedAll(msgKey, oldThreadParent, newParentKey, nsnull);
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgThread::GetChildAt(PRInt32 aIndex, nsIMsgDBHdr **result)
{
	nsresult ret = NS_OK;
	mdbOid oid;
	nsIMdbRow *hdrRow = nsnull;

	ret = m_mdbTable->PosToOid( m_mdbDB->GetEnv(), aIndex, &oid);
	if (NS_SUCCEEDED(ret))
	{
		//do I have to release hdrRow?
		ret = m_mdbTable->PosToRow(m_mdbDB->GetEnv(), aIndex, &hdrRow); 
		if(NS_SUCCEEDED(ret) && hdrRow)
		{
			ret = m_mdbDB->CreateMsgHdr(hdrRow,  oid.mOid_Id , result);
		}
	}

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetChild(nsMsgKey msgKey, nsIMsgDBHdr **result)
{
	nsresult ret = NS_OK;
	mdb_bool	hasOid;
	mdbOid		rowObjectId;


	if (!result || !m_mdbTable)
		return NS_ERROR_NULL_POINTER;

	*result = NULL;
	rowObjectId.mOid_Id = msgKey;
	rowObjectId.mOid_Scope = m_mdbDB->m_hdrRowScopeToken;
	ret = m_mdbTable->HasOid(m_mdbDB->GetEnv(), &rowObjectId, &hasOid);
	if (NS_SUCCEEDED(ret) && hasOid && m_mdbDB && m_mdbDB->m_mdbStore)
	{
		nsIMdbRow *hdrRow = nsnull;
		ret = m_mdbDB->m_mdbStore->GetRow(m_mdbDB->GetEnv(), &rowObjectId,  &hdrRow);
		if (ret == NS_OK && hdrRow && m_mdbDB)
		{
			ret = m_mdbDB->CreateMsgHdr(hdrRow,  msgKey, result);
		}
	}
	return ret;
}


NS_IMETHODIMP nsMsgThread::GetChildHdrAt(PRInt32 aIndex, nsIMsgDBHdr **result)
{
	nsresult ret = NS_OK;
	nsIMdbRow* resultRow;
	mdb_pos pos = aIndex - 1;

	if (!result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	// mork doesn't seem to handle this correctly, so deal with going off
	// the end here.
	if (aIndex > (PRInt32) m_numChildren)
		return NS_OK;

	nsIMdbTableRowCursor *rowCursor;
	ret = m_mdbTable->GetTableRowCursor(m_mdbDB->GetEnv(), pos, &rowCursor);

	ret = rowCursor->NextRow(m_mdbDB->GetEnv(), &resultRow, &pos);
	NS_RELEASE(rowCursor);
	if (NS_FAILED(ret) || !resultRow) 
		return ret;

	//Get key from row
	mdbOid outOid;
	nsMsgKey key=0;
	if (resultRow->GetOid(m_mdbDB->GetEnv(), &outOid) == NS_OK)
		key = outOid.mOid_Id;

	ret = m_mdbDB->CreateMsgHdr(resultRow, key, result);
	if (NS_FAILED(ret)) 
		return ret;
	return ret;
}


NS_IMETHODIMP nsMsgThread::RemoveChildAt(PRInt32 aIndex)
{
	nsresult ret = NS_OK;

	return ret;
}


nsresult nsMsgThread::RemoveChild(nsMsgKey msgKey)
{
	nsresult ret = NS_OK;
	mdbOid		rowObjectId;
	rowObjectId.mOid_Id = msgKey;
	rowObjectId.mOid_Scope = m_mdbDB->m_hdrRowScopeToken;
	ret = m_mdbTable->CutOid(m_mdbDB->GetEnv(), &rowObjectId);
#if 0 // this seems to cause problems
  if (m_numChildren == 0 && m_metaRow && m_mdbDB)
    m_metaRow->CutAllColumns(m_mdbDB->GetEnv());
#endif
	return ret;
}

NS_IMETHODIMP nsMsgThread::RemoveChildHdr(nsIMsgDBHdr *child, nsIDBChangeAnnouncer *announcer)
{
	PRUint32 flags;
	nsMsgKey key;
	nsMsgKey threadParent;

	if (!child)
		return NS_ERROR_NULL_POINTER;

	child->GetFlags(&flags);
	child->GetMessageKey(&key);

	child->GetThreadParent(&threadParent);
	ReparentChildrenOf(key, threadParent, announcer);

	if (!(flags & MSG_FLAG_READ))
		ChangeUnreadChildCount(-1);
	ChangeChildCount(-1);
	return RemoveChild(key);
}

nsresult nsMsgThread::ReparentChildrenOf(nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeAnnouncer *announcer)
{
	nsresult rv = NS_OK;

	PRUint32 numChildren;
	PRUint32 childIndex = 0;

 	GetNumChildren(&numChildren);

	nsCOMPtr <nsIMsgDBHdr> curHdr;
	if (numChildren > 0)
	{
		for (childIndex = 0; childIndex < numChildren; childIndex++)
		{
			rv = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
			if (NS_SUCCEEDED(rv) && curHdr)
			{
				nsMsgKey threadParent;

				curHdr->GetThreadParent(&threadParent);
				if (threadParent == oldParent)
				{
					nsMsgKey curKey;

					curHdr->SetThreadParent(newParent);
					curHdr->GetMessageKey(&curKey);
					if (announcer)
						announcer->NotifyParentChangedAll(curKey, oldParent, newParent, nsnull);

					// if the old parent was the root of the thread, then only the first child gets 
					// promoted to root, and other children become children of the new root.
					if (newParent == nsMsgKey_None)
          {
            SetThreadRootKey(curKey);
						newParent = curKey;
          }
				}
			}
		}
	}
	return rv;
}

NS_IMETHODIMP nsMsgThread::MarkChildRead(PRBool bRead)
{
	nsresult ret = NS_OK;

	if(bRead)
		ChangeUnreadChildCount(-1);
	else
		ChangeUnreadChildCount(1);

	return ret;
}

PRBool nsMsgThread::TryReferenceThreading(nsIMsgDBHdr *newHeader)
{
	// start at end of references to find immediate parent in thread
	PRBool addedChild = PR_FALSE;
#if 0
	PRBool done = PR_FALSE;

	for (int32 refIndex = newHeader->GetNumReferences() - 1; !done && refIndex >= 0; refIndex--)
	{
		nsCOMPtr <nsIMsgDBHdr>  refHdr;
		nsCString referenceStr;

		newHeader->GetStringReference(refIndex, referenceStr);
		refHdr = m_mdbDB->GetMsgHdrForMessageID(referenceStr);
		if (refHdr)
		{
			// position iterator at child pos.
			childIterator.reset();
			while (PR_TRUE)
			{
				MessageKey curMsgId = childIterator.currentID();
				if (curMsgId == 0)
					break;
				if (curMsgId == refHdr->fID)
					break;
				childIterator.nextID();
			}
			// new header is a reply to header at child index. Its level
			// is the childIndex header + 1. We need
			// to put it in the m_children array with the other children of
			// the document we are a reply to, in date order.
			newHeader->SetLevel(refHdr->GetLevel() + 1);
			while (childIterator.currentID() != 0 && !done)
			{
				MessageKey msgId = childIterator.currentID();
				if (msgId != 0)
				{
					nsIMsgDBHdr *childHdr = (nsIMsgDBHdr *) childIterator.currentObject();
					if (childHdr != NULL)
					{
						// If the current header is at a equal or higher level, we've reached the end
						// of the current level, so insert here.
						// If the current header is at the desired level, and is later than
						// the new header, put the new header before the current header.
						// Or if we're at the end, insert here.
						if ((childHdr != refHdr && childHdr->GetLevel() <= refHdr->GetLevel())
							|| (childHdr->GetLevel() == newHeader->GetLevel() 
								&& newHeader->GetDate() < childHdr->GetDate())
							|| childIterator.nextID() == 0) // evil side effect - advance cursor
						{
							messageDB->AddNeoHdr(newHeader);
							// it seems if we get to the end, addHere sticks object at
							// beginning of parts list. That's not what we want.
							if (childIterator.currentID() != 0)
								childIterator.addHere(newHeader, TRUE /* insert before cursor */);
							else
								childIterator.addObject(newHeader);
							newHeader->unrefer();	// adding a newHeader to a part mgr adds a reference
							done = PR_TRUE;
							addedChild = PR_TRUE;
						}
					}
				}
			}
			refHdr->unrefer();
			break;
		}
	}
#endif
	return addedChild;
}


class nsMsgThreadEnumerator : public nsISimpleEnumerator {
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods:
    NS_DECL_NSISIMPLEENUMERATOR

    // nsMsgThreadEnumerator methods:
    typedef nsresult (*nsMsgThreadEnumeratorFilter)(nsIMsgDBHdr* hdr, void* closure);

    nsMsgThreadEnumerator(nsMsgThread *thread, nsMsgKey startKey,
                      nsMsgThreadEnumeratorFilter filter, void* closure);
	PRInt32 MsgKeyFirstChildIndex(nsMsgKey inMsgKey);
    virtual ~nsMsgThreadEnumerator();

protected:

	nsresult					Prefetch();

	nsIMdbTableRowCursor*       mRowCursor;
    nsCOMPtr <nsIMsgDBHdr>      mResultHdr;
	nsMsgThread*				mThread;
	nsMsgKey					mThreadParentKey;
	nsMsgKey					mFirstMsgKey;
	PRInt32						mChildIndex;
    PRBool                      mDone;
	PRBool						mNeedToPrefetch;
  PRBool            mFoundChildren;
    nsMsgThreadEnumeratorFilter     mFilter;
    void*                       mClosure;
};

nsMsgThreadEnumerator::nsMsgThreadEnumerator(nsMsgThread *thread, nsMsgKey startKey,
                                     nsMsgThreadEnumeratorFilter filter, void* closure)
    : mRowCursor(nsnull), mDone(PR_FALSE),
      mFilter(filter), mClosure(closure), mFoundChildren(PR_FALSE)
{
    NS_INIT_REFCNT();
	mThreadParentKey = startKey;
	mChildIndex = 0;
	mThread = thread;
	mNeedToPrefetch = PR_TRUE;
	mFirstMsgKey = nsMsgKey_None;

	nsresult rv = mThread->GetRootHdr(nsnull, getter_AddRefs(mResultHdr));

	if (NS_SUCCEEDED(rv) && mResultHdr)
		mResultHdr->GetMessageKey(&mFirstMsgKey);

	PRUint32 numChildren;
	mThread->GetNumChildren(&numChildren);

	if (mThreadParentKey != nsMsgKey_None)
	{
		nsMsgKey msgKey = nsMsgKey_None;
		PRUint32 childIndex = 0;


		for (childIndex = 0; childIndex < numChildren; childIndex++)
		{
			rv = mThread->GetChildHdrAt(childIndex, getter_AddRefs(mResultHdr));
			if (NS_SUCCEEDED(rv) && mResultHdr)
			{
				mResultHdr->GetMessageKey(&msgKey);

				if (msgKey == startKey)
				{
					mChildIndex = MsgKeyFirstChildIndex(msgKey);
					mDone = (mChildIndex < 0);
					break;
				}

				if (mDone)
					break;

			}
			else
				NS_ASSERTION(PR_FALSE, "couldn't get child from thread");
		}
	}

#ifdef DEBUG_bienvenu1
		nsCOMPtr <nsIMsgDBHdr> child;
		for (PRUint32 childIndex = 0; childIndex < numChildren; childIndex++)
		{
			rv = mThread->GetChildHdrAt(childIndex, getter_AddRefs(child));
			if (NS_SUCCEEDED(rv) && child)
			{
				nsMsgKey threadParent;
				nsMsgKey msgKey;
				// we're only doing one level of threading, so check if caller is
				// asking for children of the first message in the thread or not.
				// if not, we will tell him there are no children.
				child->GetMessageKey(&msgKey);
				child->GetThreadParent(&threadParent);

				printf("index = %ld key = %ld parent = %lx\n", childIndex, msgKey, threadParent);
			}
		}
#endif
    NS_ADDREF(thread);
}

nsMsgThreadEnumerator::~nsMsgThreadEnumerator()
{
    NS_RELEASE(mThread);
}

NS_IMPL_ISUPPORTS(nsMsgThreadEnumerator, NS_GET_IID(nsISimpleEnumerator))


PRInt32 nsMsgThreadEnumerator::MsgKeyFirstChildIndex(nsMsgKey inMsgKey)
{
//	if (msgKey != mThreadParentKey)
//		mDone = PR_TRUE;
	// look through rest of thread looking for a child of this message.
	// If the inMsgKey is the first message in the thread, then all children
	// without parents are considered to be children of inMsgKey.
	// Otherwise, only true children qualify.
	PRUint32 numChildren;
	nsCOMPtr <nsIMsgDBHdr> curHdr;
	PRInt32 firstChildIndex = -1;

	mThread->GetNumChildren(&numChildren);

	// if this is the first message in the thread, just check if there's more than
	// one message in the thread.
	if (inMsgKey == mThread->m_threadRootKey)
		return (numChildren > 1) ? 1 : -1;

	for (PRUint32 curChildIndex = 0; curChildIndex < numChildren; curChildIndex++)
	{
		nsresult rv = mThread->GetChildHdrAt(curChildIndex, getter_AddRefs(curHdr));
		if (NS_SUCCEEDED(rv) && curHdr)
		{
			nsMsgKey parentKey;

			curHdr->GetThreadParent(&parentKey);
			if (parentKey == inMsgKey)
			{
				firstChildIndex = curChildIndex;
				break;
			}
		}
	}
#ifdef DEBUG_bienvenu1
	printf("first child index of %ld = %ld\n", inMsgKey, firstChildIndex);
#endif
	return firstChildIndex;
}

NS_IMETHODIMP nsMsgThreadEnumerator::GetNext(nsISupports **aItem)
{
	if (!aItem)
		return NS_ERROR_NULL_POINTER;
	nsresult rv = NS_OK;

	if (mNeedToPrefetch)
		rv = Prefetch();

    if (NS_SUCCEEDED(rv) && mResultHdr) 
	{
        *aItem = mResultHdr;
        NS_ADDREF(*aItem);
		mNeedToPrefetch = PR_TRUE;
    }
	return rv;
}

nsresult nsMsgThreadEnumerator::Prefetch()
{
	nsresult rv=NS_OK;          // XXX or should this default to an error?
	mResultHdr = nsnull;
	if (mThreadParentKey == nsMsgKey_None)
	{
		rv = mThread->GetRootHdr(&mChildIndex, getter_AddRefs(mResultHdr));
		NS_ASSERTION(NS_SUCCEEDED(rv) && mResultHdr, "better be able to get root hdr");
		mChildIndex = 0; // since root can be anywhere, set mChildIndex to 0.
	}
	else if (!mDone)
	{
		PRUint32 numChildren;
		mThread->GetNumChildren(&numChildren);

		while (mChildIndex < (PRInt32) numChildren)
		{
			rv  = mThread->GetChildHdrAt(mChildIndex++, getter_AddRefs(mResultHdr));
			if (NS_SUCCEEDED(rv) && mResultHdr)
			{
				nsMsgKey parentKey;
				nsMsgKey curKey;

				mResultHdr->GetThreadParent(&parentKey);
				mResultHdr->GetMessageKey(&curKey);
				// if the parent is the same as the msg we're enumerating over,
				// or the parentKey isn't set, and we're iterating over the top
				// level message in the thread, then leave mResultHdr set to cur msg.
				if (parentKey == mThreadParentKey || 
						((parentKey == 0 || parentKey == nsMsgKey_None) 
							&& mThreadParentKey == mFirstMsgKey && curKey != mThreadParentKey))
					break;
				mResultHdr = nsnull;
			}
			else
				NS_ASSERTION(PR_FALSE, "better be able to get child");
		}
    if (!mResultHdr && mThreadParentKey == mFirstMsgKey && !mFoundChildren && numChildren > 1)
    {
      mThread->ReparentMsgsWithInvalidParent(numChildren, mThreadParentKey);
    }
	}
	if (!mResultHdr) 
	{
        mDone = PR_TRUE;
		return NS_ERROR_FAILURE;
    }
    if (NS_FAILED(rv)) 
	{
        mDone = PR_TRUE;
        return rv;
    }
	else
		mNeedToPrefetch = PR_FALSE;
  mFoundChildren = PR_TRUE;

#ifdef DEBUG_bienvenu1
	nsMsgKey debugMsgKey;
	mResultHdr->GetMessageKey(&debugMsgKey);
	printf("next for %ld = %ld\n", mThreadParentKey, debugMsgKey);
#endif

    return rv;
}

NS_IMETHODIMP nsMsgThreadEnumerator::HasMoreElements(PRBool *aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;
	if (mNeedToPrefetch)
		Prefetch();
	*aResult = !mDone;
    return NS_OK;
}


NS_IMETHODIMP nsMsgThread::EnumerateMessages(nsMsgKey parentKey, nsISimpleEnumerator* *result)
{
	nsresult ret = NS_OK;
	nsMsgThreadEnumerator* e = new nsMsgThreadEnumerator(this, parentKey, nsnull, nsnull);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;

	return ret;
}

nsresult nsMsgThread::ReparentMsgsWithInvalidParent(PRUint32 numChildren, nsMsgKey threadParentKey)
{
  nsresult ret = NS_OK;
  // run through looking for messages that don't have a correct parent, 
  // i.e., a parent that's in the thread!
  for (PRInt32 childIndex = 0; childIndex < (PRInt32) numChildren; childIndex++)
	{
    nsCOMPtr <nsIMsgDBHdr> curChild;
		ret  = GetChildHdrAt(childIndex, getter_AddRefs(curChild));
		if (NS_SUCCEEDED(ret) && curChild)
		{
			nsMsgKey parentKey;
      nsCOMPtr <nsIMsgDBHdr> parent;

			curChild->GetThreadParent(&parentKey);

      if (parentKey != nsMsgKey_None)
      {
        GetChild(parentKey, getter_AddRefs(parent));
        if (!parent)
          curChild->SetThreadParent(threadParentKey);
      }
    }
  }
  return ret;
}

NS_IMETHODIMP nsMsgThread::GetRootHdr(PRInt32 *resultIndex, nsIMsgDBHdr **result)
{
	nsresult ret;
	if (!result)
		return NS_ERROR_NULL_POINTER;

  *result = nsnull;

	if (m_threadRootKey != nsMsgKey_None)
	{
		ret = GetChildHdrForKey(m_threadRootKey, result, resultIndex);
		if (NS_SUCCEEDED(ret) && *result)
			return ret;
    else
    {
      printf("need to reset thread root key\n");
		  PRUint32 numChildren;
      nsMsgKey threadParentKey = nsMsgKey_None;
		  GetNumChildren(&numChildren);

      for (PRInt32 childIndex = 0; childIndex < (PRInt32) numChildren; childIndex++)
		  {
        nsCOMPtr <nsIMsgDBHdr> curChild;
			  ret  = GetChildHdrAt(childIndex, getter_AddRefs(curChild));
			  if (NS_SUCCEEDED(ret) && curChild)
			  {
				  nsMsgKey parentKey;

				  curChild->GetThreadParent(&parentKey);
          if (parentKey == nsMsgKey_None)
          {
            NS_ASSERTION(!(*result), "two top level msgs, not good");
  				  curChild->GetMessageKey(&threadParentKey);
            SetThreadRootKey(threadParentKey);
            if (resultIndex)
              *resultIndex = childIndex;
            *result = curChild;
            NS_ADDREF(*result);
            ReparentMsgsWithInvalidParent(numChildren, threadParentKey);
//            return NS_OK;
          }
        }
      }
      if (*result)
      {
        return NS_OK;
      }
    }
		// if we can't get the thread root key, we'll just get the first hdr.
		// there's a bug where sometimes we weren't resetting the thread root key 
		// when removing the thread root key.
	}
	if (resultIndex)
		*resultIndex = 0;
	return GetChildHdrAt(0, result);
}

nsresult nsMsgThread::ChangeChildCount(PRInt32 delta)
{
	nsresult ret = NS_OK;
	PRUint32 childCount = 0;
	m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, childCount);

	NS_ASSERTION(childCount != 0 || delta > 0, "child count gone negative");
	childCount += delta;

	NS_ASSERTION((PRInt32) childCount >= 0, "child count gone to 0 or below");
	if ((PRInt32) childCount < 0)	// force child count to >= 0
		childCount = 0;

	ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, childCount);
	m_numChildren = childCount;
	return ret;
}

nsresult nsMsgThread::ChangeUnreadChildCount(PRInt32 delta)
{
	nsresult ret = NS_OK;
	PRUint32 childCount = 0;
	m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, childCount);
	childCount += delta;

	ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, childCount);
	m_numUnreadChildren = childCount;
	return ret;
}

nsresult nsMsgThread::SetThreadRootKey(nsMsgKey threadRootKey)
{
	nsresult ret = NS_OK;
	m_threadRootKey = threadRootKey;
	ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadRootKeyColumnToken, threadRootKey);
	return ret;
}

nsresult nsMsgThread::GetChildHdrForKey(nsMsgKey desiredKey, nsIMsgDBHdr **result, PRInt32 *resultIndex)
{
	PRUint32 numChildren;
	PRUint32 childIndex = 0;
	nsresult rv = NS_OK;        // XXX or should this default to an error?

	if (!result)
		return NS_ERROR_NULL_POINTER;

	GetNumChildren(&numChildren);

	if ((PRInt32) numChildren < 0)
		numChildren = 0;

	for (childIndex = 0; childIndex < numChildren; childIndex++)
	{
		rv = GetChildHdrAt(childIndex, result);
		if (NS_SUCCEEDED(rv) && *result)
		{
			nsMsgKey msgKey;
			// we're only doing one level of threading, so check if caller is
			// asking for children of the first message in the thread or not.
			// if not, we will tell him there are no children.
			(*result)->GetMessageKey(&msgKey);

			if (msgKey == desiredKey)
				break;
			NS_RELEASE(*result);
		}
	}
	if (resultIndex)
		*resultIndex = childIndex;

	return rv;
}


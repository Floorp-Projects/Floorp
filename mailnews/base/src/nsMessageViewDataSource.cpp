/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsMessageViewDataSource.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsMsgRDFUtils.h"
#include "nsMsgUtils.h"


#include "plstr.h"

#define VIEW_SHOW_ALL 0x1
#define VIEW_SHOW_READ 0x2
#define VIEW_SHOW_UNREAD 0x4
#define VIEW_SHOW_WATCHED 0x8

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);


nsIRDFResource* nsMessageViewDataSource::kNC_MessageChild = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Subject = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Sender = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Date = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Status = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Total = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Unread = nsnull;

NS_IMPL_ADDREF(nsMessageViewDataSource)

NS_IMETHODIMP_(nsrefcnt)
nsMessageViewDataSource::Release()
{
    // We need a special implementation of Release(). The composite
    // datasource holds a reference to mDataSource, and mDataSource
    // holds a reference _back_ to the composite datasource by way of
    // the "observer".
    NS_PRECONDITION(PRInt32(mRefCnt) > 0, "duplicate release");
    --mRefCnt;

    // When the number of references is one, we know that all that
    // remains is the circular references from mDataSource back to
    // us. Release it.
    if (mRefCnt == 1 && mDataSource) {
        mDataSource->RemoveObserver(this);
        return 0;
    }
    else if (mRefCnt == 0) {
        delete this;
        return 0;
    }
    else {
        return mRefCnt;
    }
}

NS_IMETHODIMP
nsMessageViewDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(nsCOMTypeInfo<nsIRDFCompositeDataSource>::GetIID()) ||
    iid.Equals(nsCOMTypeInfo<nsIRDFDataSource>::GetIID()) ||
	iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIRDFCompositeDataSource*, this);
		NS_ADDREF_THIS();
	}
	else if(iid.Equals(nsCOMTypeInfo<nsIMessageView>::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIMessageView*, this);
		NS_ADDREF_THIS();
	}
	else if(iid.Equals(nsCOMTypeInfo<nsIMsgWindowData>::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIMsgWindowData*, this);
		NS_ADDREF_THIS();
	}

	if(*result)
	{
	    return NS_OK;
	}

  return NS_NOINTERFACE;
}

nsMessageViewDataSource::nsMessageViewDataSource(void)
{
	NS_INIT_REFCNT();
	mObservers = nsnull;
	mShowStatus = VIEW_SHOW_ALL;
	mInitialized = PR_FALSE;
	mShowThreads = PR_FALSE;
}

nsMessageViewDataSource::~nsMessageViewDataSource (void)
{
	mRDFService->UnregisterDataSource(this);

	nsrefcnt refcnt;
	NS_RELEASE2(kNC_MessageChild, refcnt);
	NS_RELEASE2(kNC_Subject, refcnt);
	NS_RELEASE2(kNC_Date, refcnt);
	NS_RELEASE2(kNC_Sender, refcnt);
	NS_RELEASE2(kNC_Status, refcnt);
	NS_RELEASE2(kNC_Total, refcnt);
	NS_RELEASE2(kNC_Unread, refcnt);
	nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); // XXX probably need shutdown listener here
	mRDFService = nsnull;

}


nsresult
nsMessageViewDataSource::Init()
{
	if (mInitialized)
		return NS_ERROR_ALREADY_INITIALIZED;

  nsresult rv;
	rv = nsServiceManager::GetService(kRDFServiceCID,
                               nsCOMTypeInfo<nsIRDFService>::GetIID(),
                               (nsISupports**) &mRDFService); // XXX probably need shutdown listener here
  if (NS_FAILED(rv)) return rv;
  
	mRDFService->RegisterDataSource(this, PR_FALSE);

	if (! kNC_MessageChild) {
		mRDFService->GetResource(NC_RDF_MESSAGECHILD,   &kNC_MessageChild);
		mRDFService->GetResource(NC_RDF_SUBJECT,		&kNC_Subject);
		mRDFService->GetResource(NC_RDF_DATE,			&kNC_Date);
		mRDFService->GetResource(NC_RDF_SENDER,			&kNC_Sender);
		mRDFService->GetResource(NC_RDF_STATUS		,   &kNC_Status);
		mRDFService->GetResource(NC_RDF_TOTALMESSAGES		,   &kNC_Total);
		mRDFService->GetResource(NC_RDF_TOTALUNREADMESSAGES		,   &kNC_Unread);
	}
	mInitialized = PR_TRUE;

	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetURI(char* *uri)
{
	if ((*uri = nsXPIDLCString::Copy("rdf:mail-messageview")) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetSource(nsIRDFResource* property,
					   nsIRDFNode* target,
					   PRBool tv,
					   nsIRDFResource** source /* out */)
{
	if(mDataSource)
		return mDataSource->GetSource(property, target, tv, source);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetTarget(nsIRDFResource* source,
					   nsIRDFResource* property,
					   PRBool tv,
					   nsIRDFNode** target)
{
	nsresult rv;

	//First see if we handle this
	nsCOMPtr<nsIMessage> message(do_QueryInterface(source));
	if(message)
	{
		rv = createMessageNode(message, property, target);
		if(NS_SUCCEEDED(rv) && rv != NS_RDF_NO_VALUE)
			return rv;
	}

	if(mDataSource)
		return mDataSource->GetTarget(source, property, tv, target);
	else
		return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsMessageViewDataSource::GetSources(nsIRDFResource* property,
						nsIRDFNode* target,
						PRBool tv,
						nsISimpleEnumerator** sources)
{
	if(mDataSource)
		return mDataSource->GetSources(property, target, tv, sources);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetTargets(nsIRDFResource* source,
						nsIRDFResource* property,    
						PRBool tv,
						nsISimpleEnumerator** targets)
{
	nsresult rv;
	if(!targets)
		return NS_ERROR_NULL_POINTER;

	*targets=nsnull;

	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMessage> message;
	
	folder = do_QueryInterface(source, &rv);
	if (NS_SUCCEEDED(rv))
	{
		if (property == kNC_MessageChild)
		{

			if(mShowThreads)
			{
				nsCOMPtr<nsISimpleEnumerator> threads;
				rv = folder->GetThreads(getter_AddRefs(threads));
				if (NS_FAILED(rv)) return rv;
				nsMessageViewThreadEnumerator * threadEnumerator = 
					new nsMessageViewThreadEnumerator(threads, folder);
				if(!threadEnumerator)
					return NS_ERROR_OUT_OF_MEMORY;
				NS_ADDREF(threadEnumerator);
				*targets = threadEnumerator;
				rv = NS_OK;
			}
			else
			{
				nsCOMPtr<nsISimpleEnumerator> messages;
				rv = folder->GetMessages(getter_AddRefs(messages));
				if (NS_SUCCEEDED(rv))
				{
					nsMessageViewMessageEnumerator * messageEnumerator = 
						new nsMessageViewMessageEnumerator(messages, mShowStatus);
					if(!messageEnumerator)
						return NS_ERROR_OUT_OF_MEMORY;
					NS_ADDREF(messageEnumerator);
					*targets = messageEnumerator;
					rv = NS_OK;
				}
			}
		}
	}
	else if (mShowThreads && NS_SUCCEEDED(source->QueryInterface(nsCOMTypeInfo<nsIMessage>::GetIID(), getter_AddRefs(message))))
	{
		if(property == kNC_MessageChild)
		{
			nsCOMPtr<nsIMsgFolder> msgfolder;
			rv = message->GetMsgFolder(getter_AddRefs(msgfolder));
			if(NS_SUCCEEDED(rv))
			{
				nsCOMPtr<nsIMsgThread> thread;
				rv = msgfolder->GetThreadForMessage(message, getter_AddRefs(thread));
				if(NS_SUCCEEDED(rv))
				{
					nsCOMPtr<nsISimpleEnumerator> messages;
					nsMsgKey msgKey;
					message->GetMessageKey(&msgKey);
					thread->EnumerateMessages(msgKey, getter_AddRefs(messages));
					nsCOMPtr<nsMessageFromMsgHdrEnumerator> converter;
					NS_NewMessageFromMsgHdrEnumerator(messages, msgfolder, getter_AddRefs(converter));
					nsMessageViewMessageEnumerator * messageEnumerator = 
						new nsMessageViewMessageEnumerator(converter, mShowStatus);
					if(!messageEnumerator)
						return NS_ERROR_OUT_OF_MEMORY;
					NS_ADDREF(messageEnumerator);
					*targets = messageEnumerator;
					rv = NS_OK;

				}
			}

		}
	}

	if(*targets)
		return rv;

	if(mDataSource)
		return mDataSource->GetTargets(source, property, tv, targets);
	else
		return rv;
}

NS_IMETHODIMP nsMessageViewDataSource::Assert(nsIRDFResource* source,
					nsIRDFResource* property, 
					nsIRDFNode* target,
					PRBool tv)
{
	if(mDataSource)
		return mDataSource->Assert(source, property, target, tv);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::Unassert(nsIRDFResource* source,
					  nsIRDFResource* property,
					  nsIRDFNode* target)
{
	if(mDataSource)
		return mDataSource->Unassert(source, property, target);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::Change(nsIRDFResource* aSource,
                                              nsIRDFResource* aProperty,
                                              nsIRDFNode* aOldTarget,
                                              nsIRDFNode* aNewTarget)
{
	if (mDataSource)
		return mDataSource->Change(aSource, aProperty, aOldTarget, aNewTarget);
	else
    return NS_OK;
}


NS_IMETHODIMP nsMessageViewDataSource::Move(nsIRDFResource* aOldSource,
                                            nsIRDFResource* aNewSource,
                                            nsIRDFResource* aProperty,
                                            nsIRDFNode* aTarget)
{
	if (mDataSource)
		return mDataSource->Move(aOldSource, aNewSource, aProperty, aTarget);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::HasAssertion(nsIRDFResource* source,
						  nsIRDFResource* property,
						  nsIRDFNode* target,
						  PRBool tv,
						  PRBool* hasAssertion)
{
	if(mDataSource)
		return mDataSource->HasAssertion(source, property, target, tv, hasAssertion);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::AddObserver(nsIRDFObserver* n)
{
	if (! mObservers) {
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
    if (NS_FAILED(rv)) return rv;
	}
	mObservers->AppendElement(n);
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::RemoveObserver(nsIRDFObserver* n)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(n);
  return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::ArcLabelsIn(nsIRDFNode* node,
						 nsISimpleEnumerator** labels)
{
	if(mDataSource)
		return mDataSource->ArcLabelsIn(node, labels);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::ArcLabelsOut(nsIRDFResource* source,
						  nsISimpleEnumerator** labels)
{
	
	nsCOMPtr<nsIMessage> message;
	if(mShowThreads && NS_SUCCEEDED(source->QueryInterface(nsCOMTypeInfo<nsIMessage>::GetIID(), getter_AddRefs(message))))
	{
		nsresult rv;
		nsCOMPtr<nsISupportsArray> arcs;
		NS_NewISupportsArray(getter_AddRefs(arcs));
		if (arcs == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;

	    arcs->AppendElement(kNC_Subject);
		arcs->AppendElement(kNC_Sender);
		arcs->AppendElement(kNC_Date);
		arcs->AppendElement(kNC_Status);
		arcs->AppendElement(kNC_Total);
		arcs->AppendElement(kNC_Unread);

		nsCOMPtr<nsIMsgFolder> folder;
		rv = message->GetMsgFolder(getter_AddRefs(folder));
		if(NS_SUCCEEDED(rv) && folder)
		{
			nsCOMPtr<nsIMsgThread> thread;
			rv =folder->GetThreadForMessage(message, getter_AddRefs(thread));
			if(thread && NS_SUCCEEDED(rv))
			{

				nsCOMPtr<nsISimpleEnumerator> messages;
				nsMsgKey msgKey;
				message->GetMessageKey(&msgKey);
				thread->EnumerateMessages(msgKey, getter_AddRefs(messages));
				nsCOMPtr<nsMessageFromMsgHdrEnumerator> converter;
				NS_NewMessageFromMsgHdrEnumerator(messages, folder, getter_AddRefs(converter));
				nsMessageViewMessageEnumerator * messageEnumerator = 
					new nsMessageViewMessageEnumerator(converter, VIEW_SHOW_ALL);
				if(!messageEnumerator)
					return NS_ERROR_OUT_OF_MEMORY;
				NS_ADDREF(messageEnumerator);
				PRBool hasMore = PR_FALSE;

				if(NS_SUCCEEDED(messageEnumerator->HasMoreElements(&hasMore)) && hasMore)
					arcs->AppendElement(kNC_MessageChild);
				NS_IF_RELEASE(messageEnumerator);
			}
		}
		nsArrayEnumerator* cursor =
			new nsArrayEnumerator(arcs);
		if (cursor == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		NS_ADDREF(cursor);
		*labels = cursor;
		return NS_OK;
	}
	if(mDataSource)
		return mDataSource->ArcLabelsOut(source, labels);
	else
		return NS_OK;
} 

NS_IMETHODIMP nsMessageViewDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	if(mDataSource)
		return mDataSource->GetAllResources(aCursor);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetAllCommands(nsIRDFResource* source,
							nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	if(mDataSource)
		return mDataSource->GetAllCommands(source, commands);
	else
		return NS_OK;
}
NS_IMETHODIMP nsMessageViewDataSource::GetAllCmds(nsIRDFResource* source,
							nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	if(mDataSource)
		return mDataSource->GetAllCmds(source, commands);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
							  nsIRDFResource*   aCommand,
							  nsISupportsArray/*<nsIRDFResource>*/* aArguments,
							  PRBool* aResult)
{
	if(mDataSource)
		return mDataSource->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
					   nsIRDFResource*   aCommand,
					   nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	if(mDataSource)
		return mDataSource->DoCommand(aSources, aCommand, aArguments);
	else
		return NS_OK;
}

//We're only going to allow one datasource at a time.
NS_IMETHODIMP nsMessageViewDataSource::AddDataSource(nsIRDFDataSource* source)
{
	if(mDataSource)
		RemoveDataSource(mDataSource);
	
	mDataSource = dont_QueryInterface(source);
	
	mDataSource->AddObserver(this);

	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::RemoveDataSource(nsIRDFDataSource* source)
{
	mDataSource->RemoveObserver(this);
	mDataSource =  null_nsCOMPtr();
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::OnAssert(nsIRDFResource* subject,
						nsIRDFResource* predicate,
						nsIRDFNode* object)
{
	if (mObservers) {
    PRUint32 count;
    nsresult rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = count - 1; i >= 0; --i) {
        nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
        obs->OnAssert(subject, predicate, object);
        NS_RELEASE(obs);
    }
    }
    return NS_OK;

}

NS_IMETHODIMP nsMessageViewDataSource::OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object)
{
    if (mObservers) {
        PRUint32 count;
        nsresult rv = mObservers->Count(&count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(subject, predicate, object);
            NS_RELEASE(obs);
        }
    }
    return NS_OK;
}


NS_IMETHODIMP nsMessageViewDataSource::OnChange(nsIRDFResource* aSource,
                                                nsIRDFResource* aProperty,
                                                nsIRDFNode* aOldTarget,
                                                nsIRDFNode* aNewTarget)
{
  if (mObservers) {
    PRUint32 count;
    nsresult rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnChange(aSource, aProperty, aOldTarget, aNewTarget);
      NS_RELEASE(obs);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsMessageViewDataSource::OnMove(nsIRDFResource* aOldSource,
                                              nsIRDFResource* aNewSource,
                                              nsIRDFResource* aProperty,
                                              nsIRDFNode* aTarget)
{
  if (mObservers) {
    PRUint32 count;
    nsresult rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnMove(aOldSource, aNewSource, aProperty, aTarget);
      NS_RELEASE(obs);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsMessageViewDataSource::SetShowAll()
{
	mShowStatus = VIEW_SHOW_ALL;
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetShowUnread()
{
	mShowStatus = VIEW_SHOW_UNREAD;
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetShowRead()
{
	mShowStatus = VIEW_SHOW_READ;
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetShowWatched()
{
	mShowStatus = VIEW_SHOW_WATCHED;
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetShowThreads(PRBool showThreads)
{
	mShowThreads = showThreads;
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetStatusFeedback(nsIMsgStatusFeedback * *aStatusFeedback)
{
	if(!aStatusFeedback)
		return NS_ERROR_NULL_POINTER;

	*aStatusFeedback = mStatusFeedback;
	NS_IF_ADDREF(*aStatusFeedback);
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetStatusFeedback(nsIMsgStatusFeedback * aStatusFeedback)
{
	mStatusFeedback = aStatusFeedback;
	return NS_OK;
}


NS_IMETHODIMP nsMessageViewDataSource::GetTransactionManager(nsITransactionManager * *aTransactionManager)
{
	if(!aTransactionManager)
		return NS_ERROR_NULL_POINTER;

	*aTransactionManager = mTransactionManager;
	NS_IF_ADDREF(*aTransactionManager);
	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::SetTransactionManager(nsITransactionManager * aTransactionManager)
{
	mTransactionManager = aTransactionManager;
	return NS_OK;
}
nsresult
nsMessageViewDataSource::createMessageNode(nsIMessage *message,
                                         nsIRDFResource *property,
                                         nsIRDFNode **target)
{
    if (mShowThreads && ( kNC_Total == property))
      return createTotalNode(message, target);
	else if (mShowThreads && (kNC_Unread == property))
      return createUnreadNode(message, target);
    else
      return NS_RDF_NO_VALUE;
}

nsresult nsMessageViewDataSource::createTotalNode(nsIMessage *message, nsIRDFNode **target)
{
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMsgThread> thread;
	nsresult rv;
	nsString emptyString("");

	rv = GetMessageFolderAndThread(message, getter_AddRefs(folder), getter_AddRefs(thread));
	if(NS_SUCCEEDED(rv) && thread)
	{
		if(IsThreadsFirstMessage(thread, message))
		{
			PRUint32 numChildren;
			rv = thread->GetNumChildren(&numChildren);
			if(NS_SUCCEEDED(rv))
			{
				if(numChildren > 1)
					rv = createNode(numChildren, target);
				else
					rv = createNode(emptyString, target);
			}
		}
		else
		{
			rv = createNode(emptyString, target);
		}

	}

	if(NS_SUCCEEDED(rv))
		return rv;
	else 
		return NS_RDF_NO_VALUE;
}

nsresult nsMessageViewDataSource::createUnreadNode(nsIMessage *message, nsIRDFNode **target)
{
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMsgThread> thread;
	nsresult rv;
	nsString emptyString("");

	rv = GetMessageFolderAndThread(message, getter_AddRefs(folder), getter_AddRefs(thread));
	if(NS_SUCCEEDED(rv) && thread)
	{
		if(IsThreadsFirstMessage(thread, message))
		{
			PRUint32 numUnread;
			rv = thread->GetNumUnreadChildren(&numUnread);
			if(NS_SUCCEEDED(rv))
			{
				if(numUnread > 0)
					rv = createNode(numUnread, target);
				else
					rv = createNode(emptyString, target);
			}
		}
		else
		{
			rv = createNode(emptyString, target);
		}
	}

	if(NS_SUCCEEDED(rv))
		return rv;
	else 
		return NS_RDF_NO_VALUE;
}

nsresult nsMessageViewDataSource::GetMessageFolderAndThread(nsIMessage *message,
															nsIMsgFolder **folder,
															nsIMsgThread **thread)
{
	nsresult rv;
	rv = message->GetMsgFolder(folder);
	if(NS_SUCCEEDED(rv))
	{
		rv = (*folder)->GetThreadForMessage(message, thread);
	}
	return rv;
}

PRBool nsMessageViewDataSource::IsThreadsFirstMessage(nsIMsgThread *thread, nsIMessage *message)
{
	nsCOMPtr<nsIMsgDBHdr> firstHdr;
	nsresult rv;

	rv = thread->GetRootHdr(nsnull, getter_AddRefs(firstHdr));
	if(NS_FAILED(rv))
		return PR_FALSE;

	nsMsgKey messageKey, firstHdrKey;

	rv = message->GetMessageKey(&messageKey);

	if(NS_FAILED(rv))
		return PR_FALSE;

	rv = firstHdr->GetMessageKey(&firstHdrKey);

	if(NS_FAILED(rv))
		return PR_FALSE;

	return messageKey == firstHdrKey;

}
//////////////////////////   nsMessageViewMessageEnumerator //////////////////


NS_IMPL_ISUPPORTS(nsMessageViewMessageEnumerator, nsCOMTypeInfo<nsISimpleEnumerator>::GetIID())

nsMessageViewMessageEnumerator::nsMessageViewMessageEnumerator(nsISimpleEnumerator *srcEnumerator,
															   PRUint32 showStatus)
{
	NS_INIT_REFCNT();	

	mSrcEnumerator = dont_QueryInterface(srcEnumerator);

	mShowStatus = showStatus;
}
nsMessageViewMessageEnumerator::~nsMessageViewMessageEnumerator()
{
	//member variables are nsCOMPtr's.
}


/** Next will advance the list. will return failed if already at end
*/
NS_IMETHODIMP nsMessageViewMessageEnumerator::GetNext(nsISupports **aItem)
{
	if (!aItem)
		return NS_ERROR_NULL_POINTER;
	nsresult rv = SetAtNextItem();
	if (NS_SUCCEEDED(rv) && mCurMsg)
	{
		*aItem = mCurMsg;
		NS_ADDREF(*aItem);
	}
	NS_ASSERTION(NS_SUCCEEDED(rv),"getnext shouldn't fail");
	return rv;
}

/** GetNext will return the next item it will fail if the list is empty
*  @param aItem return value
*/
/** return if the collection is at the end.  that is the beginning following a call to Prev
*  and it is the end of the list following a call to next
*  @param aItem return value
*/
NS_IMETHODIMP nsMessageViewMessageEnumerator::HasMoreElements(PRBool *aResult)
{
	return mSrcEnumerator->HasMoreElements(aResult);
}

//This function sets mSrcEnumerator at the next item that fits
//the criteria for this enumerator.  If there are no more items
//returns NS_ERROR_FAILURE
nsresult nsMessageViewMessageEnumerator::SetAtNextItem()
{
	nsresult rv;

	nsCOMPtr<nsISupports> currentItem;
	nsCOMPtr<nsIMessage> message;

	PRBool hasMore = PR_FALSE;
	while(NS_SUCCEEDED(mSrcEnumerator->HasMoreElements(&hasMore)) && hasMore)
	{
		rv = mSrcEnumerator->GetNext(getter_AddRefs(currentItem));
		PRBool successful = PR_FALSE;
		if(NS_FAILED(rv))
			break;

		message = do_QueryInterface(currentItem, &rv);
		if(NS_SUCCEEDED(rv))
		{
			PRBool meetsCriteria;
			rv = MeetsCriteria(message, &meetsCriteria);
			if(NS_SUCCEEDED(rv) & meetsCriteria)
				successful = PR_TRUE;
		}
		if(successful) 
		{
			mCurMsg = do_QueryInterface(currentItem, &rv);
			return rv;
		}

	}
	return NS_ERROR_FAILURE;
}

nsresult nsMessageViewMessageEnumerator::MeetsCriteria(nsIMessage *message, PRBool *meetsCriteria)
{

	if(!meetsCriteria)
		return NS_ERROR_NULL_POINTER;

	*meetsCriteria = PR_FALSE;

	if(mShowStatus == VIEW_SHOW_ALL)
	{
		*meetsCriteria = PR_TRUE;
	}
	else
	{
		PRUint32 flags;
		message->GetFlags(&flags);

		if(mShowStatus == VIEW_SHOW_READ)
			*meetsCriteria = flags & MSG_FLAG_READ;
		else if(mShowStatus == VIEW_SHOW_UNREAD)
			*meetsCriteria = !(flags & MSG_FLAG_READ);
		else if(mShowStatus == VIEW_SHOW_WATCHED)
			*meetsCriteria = flags & MSG_FLAG_WATCHED;
	}
	return NS_OK;
}

//////////////////////////   nsMessageViewThreadEnumerator //////////////////


NS_IMPL_ISUPPORTS(nsMessageViewThreadEnumerator, nsCOMTypeInfo<nsISimpleEnumerator>::GetIID())

nsMessageViewThreadEnumerator::nsMessageViewThreadEnumerator(nsISimpleEnumerator *threads,
															 nsIMsgFolder *srcFolder)
{
	NS_INIT_REFCNT();
	
	mThreads = do_QueryInterface(threads);
	mFolder = do_QueryInterface(srcFolder);
	mNeedToPrefetch = PR_TRUE;
}

nsMessageViewThreadEnumerator::~nsMessageViewThreadEnumerator()
{
	//member variables are nsCOMPtr's
}

/** Next will advance the list. will return failed if already at end
*/
NS_IMETHODIMP nsMessageViewThreadEnumerator::GetNext(nsISupports **aItem)
{
	nsresult rv = NS_OK;

	if(!mMessages)
		return NS_ERROR_FAILURE;

	if (mNeedToPrefetch)
		rv = Prefetch();
	if (NS_SUCCEEDED(rv) && mMessages)
		rv = mMessages->GetNext(aItem);
//	NS_ASSERTION(NS_SUCCEEDED(rv),"getnext shouldn't fail");
	mNeedToPrefetch = PR_TRUE;
	return rv;
}

nsresult nsMessageViewThreadEnumerator::Prefetch()
{
	nsresult rv = NS_OK;

	//then check to see if the there are no more messages in last thread
	//Get the next thread
	rv = mThreads->GetNext(getter_AddRefs(mCurThread));
	if(NS_SUCCEEDED(rv))
	{
		rv = GetMessagesForCurrentThread();
		mNeedToPrefetch = PR_FALSE;
	}
	else
		mMessages = nsnull;
	return rv;
}

/** return if the collection is at the end.  that is the beginning following a call to Prev
*  and it is the end of the list following a call to next
*  @param aItem return value
*/
NS_IMETHODIMP nsMessageViewThreadEnumerator::HasMoreElements(PRBool *aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;
	nsresult rv = NS_OK;

	if (mNeedToPrefetch)
		Prefetch();
	//First check to see if there are no more threads
	if (mMessages)
		rv = mMessages->HasMoreElements(aResult);
	else
		*aResult = nsnull;
	return rv;
}

nsresult nsMessageViewThreadEnumerator::GetMessagesForCurrentThread()
{
	nsCOMPtr<nsIMsgThread> thread;
	nsresult rv = NS_OK;
	if(mCurThread)
	{
		thread = do_QueryInterface(mCurThread, &rv);
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsISimpleEnumerator> msgHdrs;
			rv = thread->EnumerateMessages(nsMsgKey_None, getter_AddRefs(msgHdrs));
			nsMessageFromMsgHdrEnumerator *messages;
			NS_NewMessageFromMsgHdrEnumerator(msgHdrs, mFolder, &messages);
			mMessages = do_QueryInterface(messages, &rv);
			NS_IF_RELEASE(messages);
		}
	}
	else
		mMessages = nsnull;

	return rv;
}

nsresult
NS_NewMessageViewDataSource(const nsIID& iid, void **result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nsMessageViewDataSource* datasource = new nsMessageViewDataSource();
    if (! datasource)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = datasource->Init();
    if (NS_FAILED(rv)) {
        delete datasource;
        return rv;
    }

	return datasource->QueryInterface(iid, result);
}

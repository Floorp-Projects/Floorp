/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsMessageViewDataSource.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsMsgRDFUtils.h"
#include "nsMsgUtils.h"


#include "plstr.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);


nsIRDFResource* nsMessageViewDataSource::kNC_MessageChild = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Subject = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Sender = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Date = nsnull;
nsIRDFResource* nsMessageViewDataSource::kNC_Status = nsnull;

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
  if (iid.Equals(NS_GET_IID(nsIRDFCompositeDataSource)) ||
    iid.Equals(NS_GET_IID(nsIRDFDataSource)) ||
	iid.Equals(NS_GET_IID(nsISupports)))
	{
		*result = NS_STATIC_CAST(nsIRDFCompositeDataSource*, this);
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
	mViewType = nsIMessageView::eShowAll;
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
                               NS_GET_IID(nsIRDFService),
                               (nsISupports**) &mRDFService); // XXX probably need shutdown listener here
  if (NS_FAILED(rv)) return rv;
  
	mRDFService->RegisterDataSource(this, PR_FALSE);

	if (! kNC_MessageChild) {
		mRDFService->GetResource(NC_RDF_MESSAGECHILD,   &kNC_MessageChild);
		mRDFService->GetResource(NC_RDF_SUBJECT,		&kNC_Subject);
		mRDFService->GetResource(NC_RDF_DATE,			&kNC_Date);
		mRDFService->GetResource(NC_RDF_SENDER,			&kNC_Sender);
		mRDFService->GetResource(NC_RDF_STATUS		,   &kNC_Status);
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
	if(mDataSource)
		return mDataSource->GetTargets(source, property, tv, targets);
	else
		return NS_RDF_NO_VALUE;
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

nsresult
nsMessageViewDataSource::GetMessageEnumerator(nsIMessage* message, nsISimpleEnumerator* *result)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder;
  rv = message->GetMsgFolder(getter_AddRefs(folder));
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(folder, "GetMsgFolder returned NS_OK, but no folder");

  nsCOMPtr<nsIMsgThread> thread;
  rv = folder->GetThreadForMessage(message, getter_AddRefs(thread));
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(folder, "GetThreadForMessage returned NS_OK, but no thread");

  nsMsgKey msgKey;
  rv = message->GetMessageKey(&msgKey);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISimpleEnumerator> messages;
  rv = thread->EnumerateMessages(msgKey, getter_AddRefs(messages));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsMessageFromMsgHdrEnumerator> converter;
  rv = NS_NewMessageFromMsgHdrEnumerator(messages, folder, getter_AddRefs(converter));
  if (NS_FAILED(rv)) return rv;

  nsMessageViewMessageEnumerator* messageEnumerator = 
    new nsMessageViewMessageEnumerator(converter, nsIMessageView::eShowAll);
  if (!messageEnumerator)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(messageEnumerator);
  *result = messageEnumerator;
  return NS_OK;
}

NS_IMETHODIMP 
nsMessageViewDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsMessageViewDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  nsresult rv;
	nsCOMPtr<nsIMessage> message;
	if(mShowThreads && NS_SUCCEEDED(aSource->QueryInterface(NS_GET_IID(nsIMessage), getter_AddRefs(message))))
	{
    if (aArc == kNC_Subject ||
        aArc == kNC_Sender ||
        aArc == kNC_Date ||
        aArc == kNC_Status) {
      *result = PR_TRUE;
      return NS_OK;
    }
    else if (aArc == kNC_MessageChild) {
      nsCOMPtr<nsISimpleEnumerator> messageEnumerator;
      rv = GetMessageEnumerator(message, getter_AddRefs(messageEnumerator));
      if (NS_SUCCEEDED(rv)) {
				PRBool hasMore = PR_FALSE;
        if (NS_SUCCEEDED(messageEnumerator->HasMoreElements(&hasMore)) && hasMore) {
          *result = PR_TRUE;
          return NS_OK;
        }
      }
    }
  }
  if (mDataSource)
    return mDataSource->HasArcOut(aSource, aArc, result);
  else
    *result = PR_FALSE;
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
	if(mShowThreads && NS_SUCCEEDED(source->QueryInterface(NS_GET_IID(nsIMessage), getter_AddRefs(message))))
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

    nsCOMPtr<nsISimpleEnumerator> messageEnumerator;
    rv = GetMessageEnumerator(message, getter_AddRefs(messageEnumerator));
    if (NS_SUCCEEDED(rv)) {
      PRBool hasMore = PR_FALSE;
      if (NS_SUCCEEDED(messageEnumerator->HasMoreElements(&hasMore)) && hasMore) {
        arcs->AppendElement(kNC_MessageChild);
      }
    }

		return NS_NewArrayEnumerator(labels, arcs);
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

NS_IMETHODIMP
nsMessageViewDataSource::GetAllowNegativeAssertions(PRBool *aAllowNegativeAssertions)
{
//	*aAllowNegativeAssertions = mAllowNegativeAssertions;
	*aAllowNegativeAssertions = PR_TRUE;	// XXX fix build bustage
	return(NS_OK);
}

NS_IMETHODIMP
nsMessageViewDataSource::SetAllowNegativeAssertions(PRBool aAllowNegativeAssertions)
{
//	mAllowNegativeAssertions = aAllowNegativeAssertions;
	return(NS_OK);
}

NS_IMETHODIMP
nsMessageViewDataSource::GetCoalesceDuplicateArcs(PRBool *aCoalesceDuplicateArcs)
{
//	*aCoalesceDuplicateArcs = mCoalesceDuplicateArcs;
	*aCoalesceDuplicateArcs = PR_TRUE;	// XXX fix build bustage
	return(NS_OK);
}

NS_IMETHODIMP
nsMessageViewDataSource::SetCoalesceDuplicateArcs(PRBool aCoalesceDuplicateArcs)
{
//	mCoalesceDuplicateArcs = aCoalesceDuplicateArcs;
	return(NS_OK);
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

NS_IMETHODIMP nsMessageViewDataSource::GetDataSources(nsISimpleEnumerator** _result)
{
  return NS_NewSingletonEnumerator(_result, mDataSource);
}

NS_IMETHODIMP nsMessageViewDataSource::OnAssert(nsIRDFDataSource* aDataSource,
                                                nsIRDFResource* subject,
                                                nsIRDFResource* predicate,
                                                nsIRDFNode* object)
{
	if (mObservers) {
    PRUint32 count;
    nsresult rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = count - 1; i >= 0; --i) {
        nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
        obs->OnAssert(this, subject, predicate, object);
        NS_RELEASE(obs);
    }
    }
    return NS_OK;

}

NS_IMETHODIMP nsMessageViewDataSource::OnUnassert(nsIRDFDataSource* aDataSource,
                                                  nsIRDFResource* subject,
                                                  nsIRDFResource* predicate,
                                                  nsIRDFNode* object)
{
    if (mObservers) {
        PRUint32 count;
        nsresult rv = mObservers->Count(&count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(this, subject, predicate, object);
            NS_RELEASE(obs);
        }
    }
    return NS_OK;
}


NS_IMETHODIMP nsMessageViewDataSource::OnChange(nsIRDFDataSource* aDataSource,
                                                nsIRDFResource* aSource,
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
      obs->OnChange(this, aSource, aProperty, aOldTarget, aNewTarget);
      NS_RELEASE(obs);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsMessageViewDataSource::OnMove(nsIRDFDataSource* aDataSource,
                                              nsIRDFResource* aOldSource,
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
      obs->OnMove(aDataSource, aOldSource, aNewSource, aProperty, aTarget);
      NS_RELEASE(obs);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::BeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
  return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::EndUpdateBatch(nsIRDFDataSource* aDataSource)
{
  return NS_OK;
}


nsresult
nsMessageViewDataSource::createMessageNode(nsIMessage *message,
                                         nsIRDFResource *property,
                                         nsIRDFNode **target)
{
      return NS_RDF_NO_VALUE;
}


//////////////////////////   nsMessageViewMessageEnumerator //////////////////


NS_IMPL_ISUPPORTS1(nsMessageViewMessageEnumerator, nsISimpleEnumerator)

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

	if(!mCurMsg)
		return NS_ERROR_FAILURE;

	*aItem = mCurMsg;
	NS_ADDREF(*aItem);

	return NS_OK;
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
	if(!aResult)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = SetAtNextItem();
	*aResult = NS_SUCCEEDED(rv);
	return NS_OK;
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

	if(mShowStatus == nsIMessageView::eShowAll)
	{
		*meetsCriteria = PR_TRUE;
	}
	else
	{
		PRUint32 flags;
		message->GetFlags(&flags);

		if(mShowStatus == nsIMessageView::eShowRead)
			*meetsCriteria = flags & MSG_FLAG_READ;
		else if(mShowStatus == nsIMessageView::eShowUnread)
			*meetsCriteria = !(flags & MSG_FLAG_READ);
		else if(mShowStatus == nsIMessageView::eShowWatched)
			*meetsCriteria = flags & MSG_FLAG_WATCHED;
	}
	return NS_OK;
}

//////////////////////////   nsMessageViewThreadEnumerator //////////////////


NS_IMPL_ISUPPORTS1(nsMessageViewThreadEnumerator, nsISimpleEnumerator)

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


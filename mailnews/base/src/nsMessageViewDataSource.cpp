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
#include "nsRDFCursorUtils.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"


#include "plstr.h"

#define VIEW_SHOW_ALL 0x1
#define VIEW_SHOW_READ 0x2
#define VIEW_SHOW_UNREAD 0x4
#define VIEW_SHOW_WATCHED 0x8

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);


nsIRDFResource* nsMessageViewDataSource::kNC_MessageChild;

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, MessageChild);

//This really needs to be some place else so that all datasources can use it.
static PRBool
peq(nsIRDFResource* r1, nsIRDFResource* r2)
{
  PRBool result;
  if (NS_SUCCEEDED(r1->EqualsResource(r2, &result)) && result) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

NS_IMPL_ADDREF(nsMessageViewDataSource)
NS_IMPL_RELEASE(nsMessageViewDataSource)

NS_IMETHODIMP
nsMessageViewDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(nsIRDFCompositeDataSource::GetIID()) ||
    iid.Equals(nsIRDFDataSource::GetIID()) ||
      iid.Equals(kISupportsIID))
  {
    *result = NS_STATIC_CAST(nsIRDFCompositeDataSource*, this);
    AddRef();
    return NS_OK;
  }
	else if(iid.Equals(nsIMessageView::GetIID()))
	{
    *result = NS_STATIC_CAST(nsIMessageView*, this);
    AddRef();
    return NS_OK;
	}
  return NS_NOINTERFACE;
}

nsMessageViewDataSource::nsMessageViewDataSource(void)
{
	NS_INIT_REFCNT();
	mDataSource = nsnull;
	mURI = nsnull;
	mObservers = nsnull;
	mShowStatus = VIEW_SHOW_ALL;
	mInitialized = PR_FALSE;
	mShowThreads = PR_TRUE;

	nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
											nsIRDFService::GetIID(),
                                            (nsISupports**) &mRDFService); // XXX probably need shutdown listener here

}

nsMessageViewDataSource::~nsMessageViewDataSource (void)
{
	mRDFService->UnregisterDataSource(this);

	RemoveDataSource(mDataSource);
	if(mURI)
		PL_strfree(mURI);
	if (mObservers) {
	  for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
		  nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
		  NS_RELEASE(obs);
	  }
	  delete mObservers;
	}
	nsrefcnt refcnt;
	NS_RELEASE2(kNC_MessageChild, refcnt);
	nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); // XXX probably need shutdown listener here
	mRDFService = nsnull;

}


NS_IMETHODIMP nsMessageViewDataSource::Init(const char* uri)
{
	if (mInitialized)
		return NS_ERROR_ALREADY_INITIALIZED;

	if ((mURI = PL_strdup(uri)) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	mRDFService->RegisterDataSource(this, PR_FALSE);

	if (! kNC_MessageChild) {
		mRDFService->GetResource(kURINC_MessageChild,   &kNC_MessageChild);
	}
	mInitialized = PR_TRUE;

	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetURI(char* *uri)
{
	if ((*uri = nsXPIDLCString::Copy(mURI)) == nsnull)
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
	if(mDataSource)
		return mDataSource->GetTarget(source, property, tv, target);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetSources(nsIRDFResource* property,
						nsIRDFNode* target,
						PRBool tv,
						nsIRDFAssertionCursor** sources)
{
	if(mDataSource)
		return mDataSource->GetSources(property, target, tv, sources);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::GetTargets(nsIRDFResource* source,
						nsIRDFResource* property,    
						PRBool tv,
						nsIRDFAssertionCursor** targets)
{
	nsresult rv = NS_ERROR_FAILURE;

	nsIMsgFolder* folder;
	if (NS_SUCCEEDED(source->QueryInterface(nsIMsgFolder::GetIID(), (void**)&folder)))
	{
		if (peq(kNC_MessageChild, property))
		{

			if(mShowThreads)
			{
				nsIEnumerator *threads = nsnull;
				rv = folder->GetThreads(&threads);
				if (NS_FAILED(rv)) return rv;
				nsMessageViewThreadEnumerator * threadEnumerator = 
					new nsMessageViewThreadEnumerator(threads);
				if(!threadEnumerator)
					return NS_ERROR_OUT_OF_MEMORY;
				nsRDFEnumeratorAssertionCursor* cursor =
					new nsRDFEnumeratorAssertionCursor(this, source, kNC_MessageChild, threadEnumerator);
				NS_IF_RELEASE(threads);
				if (cursor == nsnull)
					return NS_ERROR_OUT_OF_MEMORY;
				NS_ADDREF(cursor);
				*targets = cursor;
				rv = NS_OK;
			}
			else
			{
				nsIEnumerator *messages = nsnull;
				rv = folder->GetMessages(&messages);
				if (NS_FAILED(rv)) return rv;
				nsMessageViewMessageEnumerator * messageEnumerator = 
					new nsMessageViewMessageEnumerator(messages, mShowStatus);
				if(!messageEnumerator)
					return NS_ERROR_OUT_OF_MEMORY;
				nsRDFEnumeratorAssertionCursor* cursor =
					new nsRDFEnumeratorAssertionCursor(this, source, kNC_MessageChild, messageEnumerator);
				NS_IF_RELEASE(messages);
				if (cursor == nsnull)
					return NS_ERROR_OUT_OF_MEMORY;
				NS_ADDREF(cursor);
				*targets = cursor;
				rv = NS_OK;
			}
		}
		NS_IF_RELEASE(folder);
		if(NS_SUCCEEDED(rv))
			return rv;
	}
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
		if ((mObservers = new nsVoidArray()) == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
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
						 nsIRDFArcsInCursor** labels)
{
	if(mDataSource)
		return mDataSource->ArcLabelsIn(node, labels);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::ArcLabelsOut(nsIRDFResource* source,
						  nsIRDFArcsOutCursor** labels)
{
	if(mDataSource)
		return mDataSource->ArcLabelsOut(source, labels);
	else
		return NS_OK;
} 

NS_IMETHODIMP nsMessageViewDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
{
	if(mDataSource)
		return mDataSource->GetAllResources(aCursor);
	else
		return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::Flush()
{
	if(mDataSource)
		return mDataSource->Flush();
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

	mDataSource = source;
	NS_ADDREF(mDataSource);

	mDataSource->AddObserver(this);

	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::RemoveDataSource(nsIRDFDataSource* source)
{
	mDataSource->RemoveObserver(this);
	NS_RELEASE(mDataSource);
	mDataSource = nsnull;

	return NS_OK;
}

NS_IMETHODIMP nsMessageViewDataSource::OnAssert(nsIRDFResource* subject,
						nsIRDFResource* predicate,
						nsIRDFNode* object)
{
	if (mObservers) {
    for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
        nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
        obs->OnAssert(subject, predicate, object);
    }
    }
    return NS_OK;

}

NS_IMETHODIMP nsMessageViewDataSource::OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object)
{
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(subject, predicate, object);
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

//////////////////////////   nsMessageViewMessageEnumerator //////////////////


NS_IMPL_ISUPPORTS(nsMessageViewMessageEnumerator, nsIEnumerator::GetIID())

nsMessageViewMessageEnumerator::nsMessageViewMessageEnumerator(nsIEnumerator *srcEnumerator,
															   PRUint32 showStatus)
{
	NS_INIT_REFCNT();	
	mSrcEnumerator = srcEnumerator;
	NS_ADDREF(mSrcEnumerator);

	mShowStatus = showStatus;
}
nsMessageViewMessageEnumerator::~nsMessageViewMessageEnumerator()
{
	NS_IF_RELEASE(mSrcEnumerator);
}

/** First will reset the list. will return NS_FAILED if no items
*/
NS_IMETHODIMP nsMessageViewMessageEnumerator::First(void)
{
	nsresult rv = mSrcEnumerator->First();
	if(NS_SUCCEEDED(rv))
	{
		//See if the first item meets the criteria.  If not, find next item.
		nsISupports *currentItem;
		nsIMessage *message;
		rv = CurrentItem(&currentItem);
		if(NS_SUCCEEDED(rv))
		{
			if(NS_SUCCEEDED(rv = currentItem->QueryInterface(nsIMessage::GetIID(), (void**)&message)))
			{
				PRBool meetsCriteria;
				rv = MeetsCriteria(message, &meetsCriteria);
				if(NS_SUCCEEDED(rv) && !meetsCriteria)
					rv = Next();
				NS_IF_RELEASE(message);
			}
			NS_IF_RELEASE(currentItem);
		}

	}
	return rv;
}

/** Next will advance the list. will return failed if already at end
*/
NS_IMETHODIMP nsMessageViewMessageEnumerator::Next(void)
{
	nsresult rv = SetAtNextItem();
	return rv;
}

/** CurrentItem will return the CurrentItem item it will fail if the list is empty
*  @param aItem return value
*/
NS_IMETHODIMP nsMessageViewMessageEnumerator::CurrentItem(nsISupports **aItem)
{
	nsresult rv = mSrcEnumerator->CurrentItem(aItem);
	return rv;
}

/** return if the collection is at the end.  that is the beginning following a call to Prev
*  and it is the end of the list following a call to next
*  @param aItem return value
*/
NS_IMETHODIMP nsMessageViewMessageEnumerator::IsDone()
{
	nsresult rv = mSrcEnumerator->IsDone();
	return rv;
}

//This function sets mSrcEnumerator at the next item that fits
//the criteria for this enumerator.  If there are no more items
//returns NS_ERROR_FAILURE
nsresult nsMessageViewMessageEnumerator::SetAtNextItem()
{
	nsresult rv;

	while(mSrcEnumerator->IsDone() != NS_OK)
	{
		mSrcEnumerator->Next();
		nsISupports *currentItem;
		nsIMessage *message;
		PRBool successful = PR_FALSE;
		rv = mSrcEnumerator->CurrentItem(&currentItem);
		if(NS_FAILED(rv))
			break;

		if(NS_SUCCEEDED(currentItem->QueryInterface(nsIMessage::GetIID(), (void**)&message)))
		{
			PRBool meetsCriteria;
			rv = MeetsCriteria(message, &meetsCriteria);
			if(NS_SUCCEEDED(rv) & meetsCriteria)
				successful = PR_TRUE;
			NS_IF_RELEASE(message);
		}
		NS_IF_RELEASE(currentItem);
		if(successful) return NS_OK;

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


NS_IMPL_ISUPPORTS(nsMessageViewThreadEnumerator, nsIEnumerator::GetIID())

nsMessageViewThreadEnumerator::nsMessageViewThreadEnumerator(nsIEnumerator *threads)
{
	NS_INIT_REFCNT();	
	mThreads = threads;
	NS_ADDREF(mThreads);
	mMessages = nsnull;

}

nsMessageViewThreadEnumerator::~nsMessageViewThreadEnumerator()
{
	NS_IF_RELEASE(mThreads);
	NS_IF_RELEASE(mMessages);
}

/** First will reset the list. will return NS_FAILED if no items
*/
NS_IMETHODIMP nsMessageViewThreadEnumerator::First(void)
{
	//Get the first thread and first message for that thread.
	nsresult rv = mThreads->First();
	if(NS_SUCCEEDED(rv))
	{
		rv = GetMessagesForCurrentThread();
	}
	return rv;
}

/** Next will advance the list. will return failed if already at end
*/
NS_IMETHODIMP nsMessageViewThreadEnumerator::Next(void)
{
	nsresult rv;

	if(!mMessages)
		return NS_ERROR_FAILURE;

	//Get the next thread
	rv = mThreads->Next();
	if(NS_SUCCEEDED(rv))
		rv = GetMessagesForCurrentThread();
	return rv;
}

/** CurrentItem will return the CurrentItem item it will fail if the list is empty
*  @param aItem return value
*/
NS_IMETHODIMP nsMessageViewThreadEnumerator::CurrentItem(nsISupports **aItem)
{
	if(!mMessages)
		return NS_ERROR_FAILURE;
	//return the current message
	nsresult rv = mMessages->CurrentItem(aItem);
	return rv;
}

/** return if the collection is at the end.  that is the beginning following a call to Prev
*  and it is the end of the list following a call to next
*  @param aItem return value
*/
NS_IMETHODIMP nsMessageViewThreadEnumerator::IsDone()
{
	//First check to see if there are no more threads
	nsresult rv = mThreads->IsDone();
	//then check to see if the there are no more messages in last thread
	if(rv == NS_OK && mMessages)
		rv = mMessages->IsDone();
	return rv;
}

nsresult nsMessageViewThreadEnumerator::GetMessagesForCurrentThread()
{
	nsISupports *currentItem;
	nsIMsgThread *thread;
	nsresult rv = mThreads->CurrentItem(&currentItem);
	if(NS_SUCCEEDED(rv))
	{
		if(NS_SUCCEEDED(rv = currentItem->QueryInterface(nsIMsgThread::GetIID(), (void**)&thread)))
		{
			NS_IF_RELEASE(mMessages);
			thread->EnumerateMessages(nsMsgKey_None, &mMessages);
			rv = mMessages->First();
			NS_IF_RELEASE(thread);
		}
		NS_IF_RELEASE(currentItem);
	}

	return rv;
}
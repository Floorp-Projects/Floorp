/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsAbRDFDataSource.h"
#include "nsAbBaseCID.h"
#include "nsIAbDirectory.h"
#include "nsIAddrBookSession.h"
#include "nsIAbCard.h"
#include "nsAbUtils.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"
#include "nsIServiceManager.h"
#include "nsIThread.h"
#include "nsIEventQueueService.h"
#include "nsIProxyObjectManager.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsAutoLock.h"

#include "prprf.h"	 
#include "prlog.h"	 

// this is used for notification of observers using nsVoidArray
typedef struct _nsAbRDFNotification {
  nsIRDFDataSource *datasource;
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *object;
} nsAbRDFNotification;
                                                


static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////
// Utilities


nsresult nsAbRDFDataSource::createNode(nsString& str, nsIRDFNode **node)
{
	*node = nsnull;
	nsresult rv; 
    nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv)); 
	NS_ENSURE_SUCCESS(rv, rv); // always check this before proceeding 
	nsCOMPtr<nsIRDFLiteral> value;
	rv = rdf->GetLiteral(str.get(), getter_AddRefs(value));
	if (NS_SUCCEEDED(rv)) 
	{
		*node = value;
		NS_IF_ADDREF(*node);
	}
	return rv;
}

nsresult nsAbRDFDataSource::createNode(PRUint32 value, nsIRDFNode **node)
{
	nsresult rv;
	nsAutoString str;
	str.AppendInt((PRInt32)value);
	rv = createNode(str, node);
	return rv;
}

PRBool nsAbRDFDataSource::changeEnumFunc(nsISupports *aElement, void *aData)
{
  nsAbRDFNotification* note = (nsAbRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnChange(note->datasource,
                     note->subject,
                     note->property,
                     nsnull, note->object);
  return PR_TRUE;
}

PRBool nsAbRDFDataSource::assertEnumFunc(nsISupports *aElement, void *aData)
{
  nsAbRDFNotification *note = (nsAbRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;
  
  observer->OnAssert(note->datasource,
                     note->subject,
                     note->property,
                     note->object);
  return PR_TRUE;
}

PRBool nsAbRDFDataSource::unassertEnumFunc(nsISupports *aElement, void *aData)
{
  nsAbRDFNotification* note = (nsAbRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnUnassert(note->datasource,
                       note->subject,
                       note->property,
                       note->object);
  return PR_TRUE;
}

nsresult nsAbRDFDataSource::CreateProxyObserver (nsIRDFObserver* observer,
	nsIRDFObserver** proxyObserver)
{
	nsresult rv;

	nsCOMPtr<nsIEventQueueService> eventQSvc = do_GetService (NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS(rv, rv);

	// Get the UI event queue
	nsCOMPtr<nsIEventQueue> uiQueue;
	rv = eventQSvc->GetSpecialEventQueue (
			nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
			getter_AddRefs (uiQueue));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIProxyObjectManager> proxyMgr = 
		do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS(rv, rv);

	// Proxy the observer on the UI queue
	/*
	 * TODO
	 * Currenly using PROXY_ASYNC, however
	 * this can flood the event queue if
	 * rate of events on the observer is
	 * greater that the time to process the
	 * events.
	 * This causes the UI to pause.
	 */
	rv = proxyMgr->GetProxyForObject (uiQueue,
		NS_GET_IID(nsIRDFObserver),
		observer,
		PROXY_ASYNC | PROXY_ALWAYS,
		(void** )proxyObserver);

	return rv;
}

nsresult nsAbRDFDataSource::CreateProxyObservers ()
{
	nsresult rv = NS_OK;

	PRUint32 nObservers;
	mObservers->Count (&nObservers);

	if (!mProxyObservers)
	{
		rv = NS_NewISupportsArray(getter_AddRefs(mProxyObservers));
		NS_ENSURE_SUCCESS(rv, rv);
	}

	PRUint32 nProxyObservers;
	mProxyObservers->Count (&nProxyObservers);

	/*
	 * For all the outstanding observers that
	 * have not been proxied
	 */
	for (PRUint32 i = nProxyObservers; i < nObservers; i++)
	{
		nsCOMPtr<nsISupports> supports;
		rv = mObservers->GetElementAt (i, getter_AddRefs (supports));
		NS_ENSURE_SUCCESS(rv, rv);

		nsCOMPtr<nsIRDFObserver> observer (do_QueryInterface (supports, &rv));
		NS_ENSURE_SUCCESS(rv, rv);
		
		// Create the proxy
		nsCOMPtr<nsIRDFObserver> proxyObserver;
		rv = CreateProxyObserver (observer, getter_AddRefs (proxyObserver));
		NS_ENSURE_SUCCESS(rv, rv);

		mProxyObservers->AppendElement(proxyObserver);
	}

	return rv;
}

nsresult nsAbRDFDataSource::NotifyObservers(nsIRDFResource *subject,
	nsIRDFResource *property,
	nsIRDFNode *object,
	PRBool assert,
	PRBool change)
{
	NS_ASSERTION(!(change && assert),
                 "Can't change and assert at the same time!\n");

	if(!mLock)
	{
		NS_ERROR("Error in AutoLock resource in nsAbRDFDataSource::NotifyObservers()");
		return NS_ERROR_OUT_OF_MEMORY;
	}

	nsresult rv;

	nsAutoLock lockGuard (mLock);

	if (!mObservers)
		return NS_OK;


	// Get the current thread
	nsCOMPtr<nsIThread> currentThread;
	rv = nsIThread::GetCurrent (getter_AddRefs(currentThread));
	NS_ENSURE_SUCCESS (rv, rv);

	// Get the main thread, which is the UI thread
	/*
	 * TODO
	 * Is the main thread always guaranteed to be
	 * the UI thread?
	 *
	 * Note that this also binds the data source
	 * to the UI which is supposedly the only
	 * place where it is used, but what about
	 * RDF datasources that are not UI specific
	 * but are used in the UI?
	 */
	nsCOMPtr<nsIThread> uiThread;
	rv = nsIThread::GetMainThread (getter_AddRefs(uiThread));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsISupportsArray> observers;
	if (currentThread == uiThread)
	{
		/*
		 * Since this is the UI Thread use the
		 * observers list directly for performance
		 */
		observers = mObservers;
	}
	else
	{
		/*
		 * This is a different thread to the UI
		 * thread need to use proxies to the
		 * observers
		 *
		 * Create the proxies
		 */
		rv = CreateProxyObservers ();
		NS_ENSURE_SUCCESS (rv, rv);

		observers = mProxyObservers;
	}

	nsAbRDFNotification note = { this, subject, property, object };
	if (change)
		observers->EnumerateForwards(changeEnumFunc, &note);
	else if (assert)
		observers->EnumerateForwards(assertEnumFunc, &note);
	else
		observers->EnumerateForwards(unassertEnumFunc, &note);

	return NS_OK;
}

nsresult nsAbRDFDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
	nsIRDFResource *propertyResource,
	const PRUnichar *oldValue, 
	const PRUnichar *newValue)
{
	nsCOMPtr<nsIRDFNode> newValueNode;
	nsString newValueStr(newValue);
	createNode(newValueStr, getter_AddRefs(newValueNode));
	NotifyObservers(resource, propertyResource, newValueNode, PR_FALSE, PR_TRUE);
	return NS_OK;
}


nsAbRDFDataSource::nsAbRDFDataSource():
  mObservers(nsnull),
  mProxyObservers(nsnull),
  mInitialized(PR_FALSE),
  mRDFService(nsnull),
  mLock(nsnull)
{
	NS_INIT_REFCNT();

	mLock = PR_NewLock ();
}

nsAbRDFDataSource::~nsAbRDFDataSource (void)
{
	if (mRDFService)
	{
		mRDFService->UnregisterDataSource(this);
		nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); 
		mRDFService = nsnull;
	}

	if(mLock)
		PR_DestroyLock (mLock);
}

nsresult nsAbRDFDataSource::Init()
{

	nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
		NS_GET_IID(nsIRDFService),
		(nsISupports**) &mRDFService); 
	NS_ENSURE_SUCCESS(rv, rv);

	mRDFService->RegisterDataSource(this, PR_FALSE);
	
	return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbRDFDataSource, nsIRDFDataSource)

 // nsIRDFDataSource methods
NS_IMETHODIMP nsAbRDFDataSource::GetURI(char* *uri)
{
    NS_NOTREACHED("should be implemented by a subclass");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsAbRDFDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               PRBool tv,
                                               nsIRDFResource** source /* out */)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::GetTarget(nsIRDFResource* source,
                                               nsIRDFResource* property,
                                               PRBool tv,
                                               nsIRDFNode** target)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP nsAbRDFDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                PRBool tv,
                                                nsISimpleEnumerator** sources)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,    
                                                PRBool tv,
                                                nsISimpleEnumerator** targets)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::Assert(nsIRDFResource* source,
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::Change(nsIRDFResource *aSource,
                                              nsIRDFResource *aProperty,
                                              nsIRDFNode *aOldTarget,
                                              nsIRDFNode *aNewTarget)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::Move(nsIRDFResource *aOldSource,
                                            nsIRDFResource *aNewSource,
                                            nsIRDFResource *aProperty,
                                            nsIRDFNode *aTarget)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP nsAbRDFDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
    *hasAssertion = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP nsAbRDFDataSource::AddObserver(nsIRDFObserver* observer)
{
	if(!mLock)
	{
		NS_ERROR("Error in AutoLock resource in nsAbRDFDataSource::AddObservers()");
		return NS_ERROR_OUT_OF_MEMORY;
	}

	nsresult rv;

	// Lock the whole method
	nsAutoLock lockGuard (mLock);

	if (!mObservers)
	{
		rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
		NS_ENSURE_SUCCESS(rv, rv);
	}

	// Do not add if already present
	PRInt32 i;
	mObservers->GetIndexOf (observer, &i);
	if (i >= 0)
		return NS_OK;

	mObservers->AppendElement(observer);

	/*
	 * If the proxy observers has been created
	 * then do the work here to avoid unecessary
	 * delay when performing the notify from a
	 * different thread
	 */
	if (mProxyObservers)
	{
		nsCOMPtr<nsIRDFObserver> proxyObserver;
		rv = CreateProxyObserver (observer,
			getter_AddRefs(proxyObserver));
		NS_ENSURE_SUCCESS(rv, rv);

		mProxyObservers->AppendElement (proxyObserver);
	}

	return NS_OK;
}

NS_IMETHODIMP nsAbRDFDataSource::RemoveObserver(nsIRDFObserver* observer)
{
	if(!mLock)
	{
		NS_ERROR("Error in AutoLock resource in nsAbRDFDataSource::RemoveObservers()");
		return NS_ERROR_OUT_OF_MEMORY;
	}

	// Lock the whole method
	nsAutoLock lockGuard (mLock);

	if (!mObservers)
		return NS_OK;

	PRInt32 i;
	mObservers->GetIndexOf (observer, &i);
	if (i >= 0)
	{
		mObservers->RemoveElementAt(i);

		if (mProxyObservers)
			mProxyObservers->RemoveElementAt(i);
	}

	return NS_OK;
}

NS_IMETHODIMP 
nsAbRDFDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsAbRDFDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsAbRDFDataSource::ArcLabelsIn(nsIRDFNode* node,
                                                nsISimpleEnumerator** labels)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                 nsISimpleEnumerator** labels)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
nsAbRDFDataSource::GetAllCommands(nsIRDFResource* source,
                                     nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
nsAbRDFDataSource::GetAllCmds(nsIRDFResource* source,
                                      nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
nsAbRDFDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                        PRBool* aResult)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbRDFDataSource::DoCommand
(nsISupportsArray * aSources, nsIRDFResource* aCommand, nsISupportsArray * aArguments)
{
    return NS_RDF_NO_VALUE;
}


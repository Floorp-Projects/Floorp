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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsAbRDFDataSource.h"
#include "nsAbBaseCID.h"
#include "nsAbDirectory.h"
#include "nsIAddrBookSession.h"
#include "nsIAbCard.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"
#include "nsIServiceManager.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

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
    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv); 
    if (NS_FAILED(rv)) return rv;   // always check this before proceeding 
	nsCOMPtr<nsIRDFLiteral> value;
	rv = rdf->GetLiteral(str.GetUnicode(), getter_AddRefs(value));
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

nsresult nsAbRDFDataSource::NotifyObservers(nsIRDFResource *subject,
                                            nsIRDFResource *property,
                                            nsIRDFNode *object,
                                            PRBool assert,
											PRBool change)
{
    NS_ASSERTION(!(change && assert),
                 "Can't change and assert at the same time!\n");

	if (mObservers)
	{
		nsAbRDFNotification note = { this, subject, property, object };
		if (change)
			mObservers->EnumerateForwards(changeEnumFunc, &note);
		else if (assert)
			mObservers->EnumerateForwards(assertEnumFunc, &note);
		else
			mObservers->EnumerateForwards(unassertEnumFunc, &note);
	}
	return NS_OK;
}

nsresult nsAbRDFDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
												nsIRDFResource *propertyResource,
												const PRUnichar *oldValue, 
												const PRUnichar *newValue)
{
	nsCOMPtr<nsIRDFNode> newValueNode;
	nsString newValueStr = newValue;
	createNode(newValueStr, getter_AddRefs(newValueNode));
	NotifyObservers(resource, propertyResource, newValueNode, PR_FALSE, PR_TRUE);
	return NS_OK;
}


nsAbRDFDataSource::nsAbRDFDataSource():
  mObservers(nsnull),
  mInitialized(PR_FALSE),
  mRDFService(nsnull)
{
	NS_INIT_REFCNT();
}

nsAbRDFDataSource::~nsAbRDFDataSource (void)
{
	if (mRDFService)
	{
		mRDFService->UnregisterDataSource(this);
		nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); 
		mRDFService = nsnull;
	}
}

nsresult nsAbRDFDataSource::Init()
{

	nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
											 NS_GET_IID(nsIRDFService),
											 (nsISupports**) &mRDFService); 
	if (NS_FAILED(rv)) return rv;

	mRDFService->RegisterDataSource(this, PR_FALSE);
	
	return NS_OK;
}

NS_IMPL_ISUPPORTS(nsAbRDFDataSource, NS_GET_IID(nsIRDFDataSource));

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

NS_IMETHODIMP nsAbRDFDataSource::AddObserver(nsIRDFObserver* n)
{
  if (! mObservers) {
    nsresult rv;
    rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
    if (NS_FAILED(rv)) return rv;
  }
  mObservers->AppendElement(n);
  return NS_OK;
}

NS_IMETHODIMP nsAbRDFDataSource::RemoveObserver(nsIRDFObserver* n)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(n);
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


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
 * Alec Flett <alecf@netscape.com>
 */

#include "nsMsgSearchDataSource.h"
#include "nsIRDFService.h"
#include "nsMsgRDFUtils.h"

typedef struct _notifyStruct {
  nsIRDFResource *source;
  nsIRDFResource *property;
  nsIRDFResource *target;
} notifyStruct;

nsCOMPtr<nsIRDFResource> nsMsgSearchDataSource::kNC_MessageChild;
nsrefcnt nsMsgSearchDataSource::gInstanceCount = 0;


nsMsgSearchDataSource::nsMsgSearchDataSource()
{
  NS_INIT_REFCNT();

}

nsresult
nsMsgSearchDataSource::Init()
{
  if (gInstanceCount++ == 0) {

    getRDFService()->GetResource(NC_RDF_MESSAGECHILD, getter_AddRefs(kNC_MessageChild));
  }

  return NS_OK;
}

nsMsgSearchDataSource::~nsMsgSearchDataSource()
{
  if (--gInstanceCount == 0) {
    kNC_MessageChild = nsnull;
  }
}

NS_IMPL_ISUPPORTS2(nsMsgSearchDataSource,
                   nsIRDFDataSource,
                   nsIMsgSearchHitNotify)

NS_IMETHODIMP
nsMsgSearchDataSource::OnSearchHit(nsIMsgHdr* aMessage)
{
  nsresult rv;
  nsCOMPtr<nsIRDFResource> messageResource = do_QueryInterface(aMessage, &rv);

  notifyObserversAssert(mSearchRoot, kNC_MessageChild, messageResource);
  return NS_OK;
}

nsresult
nsMsgSearchDataSource::notifyObserversAssert(nsIRDFResource *aSource,
                                             nsIRDFResource *aProperty,
                                             nsIRDFResource *aTarget)
{
  // no observers to notify, that's ok
  if (!mObservers) return NS_OK;

  notifyStruct notifyMessage;
  notifyMessage.source = aSource;
  notifyMessage.property = aProperty;
  notifyMessage.target = aTarget;
  
  mObservers->EnumerateForwards(notifyAssert, (void *)&notifyMessage);
  
  return NS_OK;
}

PRBool
nsMsgSearchDataSource::notifyAssert(nsISupports* aElement, void *aData)
{
  nsIRDFObserver *observer = (nsIRDFObserver*)aElement;
  notifyStruct *notifyMessage = (notifyStruct *)aData;

  observer->OnAssert(notifyMessage->source,
                     notifyMessage->property,
                     notifyMessage->target);
  return PR_TRUE;
}


/* readonly attribute string URI; */
NS_IMETHODIMP
nsMsgSearchDataSource::GetURI(char * *aURI)
{
  *aURI = nsCRT::strdup("NC:msgsearch");
  return NS_OK;
}

/* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgSearchDataSource::GetSource(nsIRDFResource *aProperty,
                                 nsIRDFNode *aTarget,
                                 PRBool aTruthValue, nsIRDFResource **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgSearchDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgSearchDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **aResult)
{
  // hopefully the message datasource will answer all these questions for us
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgSearchDataSource::GetTargets(nsIRDFResource *aSource,
                                  nsIRDFResource *aProperty,
                                  PRBool aTruthValue,
                                  nsISimpleEnumerator **aResult)
{
  // decode the search results?
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgSearchDataSource::Assert(nsIRDFResource *aSource,
                              nsIRDFResource *aProperty,
                              nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP
nsMsgSearchDataSource::Unassert(nsIRDFResource *aSource,
                                nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Change (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aOldTarget, in nsIRDFNode aNewTarget); */
NS_IMETHODIMP
nsMsgSearchDataSource::Change(nsIRDFResource *aSource,
                              nsIRDFResource *aProperty,
                              nsIRDFNode *aOldTarget, nsIRDFNode *aNewTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Move (in nsIRDFResource aOldSource, in nsIRDFResource aNewSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP
nsMsgSearchDataSource::Move(nsIRDFResource *aOldSource,
                            nsIRDFResource *aNewSource,
                            nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgSearchDataSource::HasAssertion(nsIRDFResource *aSource,
                                    nsIRDFResource *aProperty,
                                    nsIRDFNode *aTarget,
                                    PRBool aTruthValue,
                                    PRBool *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void AddObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsMsgSearchDataSource::AddObserver(nsIRDFObserver *aObserver)
{
  nsresult rv;
  if (!mObservers) {
    rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mObservers->AppendElement(aObserver);
  
  return NS_OK;
}

/* void RemoveObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsMsgSearchDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
  NS_ENSURE_TRUE(mObservers, NS_ERROR_NOT_INITIALIZED);

  return mObservers->RemoveElement(aObserver);
}

/* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
NS_IMETHODIMP
nsMsgSearchDataSource::ArcLabelsIn(nsIRDFNode *aNode,
                                   nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgSearchDataSource::ArcLabelsOut(nsIRDFResource *aSource,
                                    nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator GetAllResources (); */
NS_IMETHODIMP
nsMsgSearchDataSource::GetAllResources(nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgSearchDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP
nsMsgSearchDataSource::IsCommandEnabled(nsISupportsArray *aSources,
                                        nsIRDFResource *aCommand,
                                        nsISupportsArray *aArguments, PRBool *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP
nsMsgSearchDataSource::DoCommand(nsISupportsArray *aSources,
                                 nsIRDFResource *aCommand,
                                 nsISupportsArray *aArguments)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator GetAllCmds (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgSearchDataSource::GetAllCmds(nsIRDFResource *aSource,
                                  nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

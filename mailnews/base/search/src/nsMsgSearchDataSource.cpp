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

#include "nsIMessage.h"
#include "nsIMsgHdr.h"
#include "nsIMsgSearchSession.h"

nsCOMPtr<nsIRDFResource> nsMsgSearchDataSource::kNC_MessageChild;
nsrefcnt nsMsgSearchDataSource::gInstanceCount = 0;
PRUint32 nsMsgSearchDataSource::gCurrentURINum = 0;


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

    mURINum = gCurrentURINum++;
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
                   nsIMsgSearchNotify)

    NS_IMETHODIMP
nsMsgSearchDataSource::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *folder)
{
    nsresult rv;

    nsCOMPtr<nsIMessage> message;
    folder->CreateMessageFromMsgDBHdr(aMsgHdr, getter_AddRefs(message));
    
    // this probably wont work. Need to convert nsMsgDBHdr -> nsMessage
    // probably through a URI or something
    nsCOMPtr<nsIRDFResource> messageResource =
      do_QueryInterface(message, &rv);

    // should probably try to cache this with an in-memory datasource
    // or something
    NotifyObservers(mSearchRoot, kNC_MessageChild, messageResource, PR_TRUE, PR_FALSE);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchDataSource::SetSearchSession(nsIMsgSearchSession* aSession)
{
    mSearchSession = aSession;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchDataSource::GetSearchSession(nsIMsgSearchSession** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    (*aResult) = mSearchSession;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}


/* readonly attribute string URI; */
NS_IMETHODIMP
nsMsgSearchDataSource::GetURI(char * *aURI)
{
    nsCAutoString uri("mailsearch:#");
    uri.AppendInt(mURINum);
    
    *aURI = uri.ToNewCString();
    return NS_OK;
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


/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgSearchDataSource::ArcLabelsOut(nsIRDFResource *aSource,
                                    nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


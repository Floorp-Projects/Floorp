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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
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

#include "nsMsgFilterDelegateFactory.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsTextFormatter.h"

#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsRDFCID.h"

#include "nsIMsgFolder.h"
#include "nsIMsgFilter.h"
#include "nsIMsgFilterList.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

// unicode "%s" format string
static const PRUnichar unicodeFormatter[] = {
    (PRUnichar)'%',
    (PRUnichar)'s',
    (PRUnichar)0,
};


NS_IMPL_ISUPPORTS1(nsMsgFilterDelegateFactory, nsIRDFDelegateFactory)

nsMsgFilterDelegateFactory::nsMsgFilterDelegateFactory()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsMsgFilterDelegateFactory::~nsMsgFilterDelegateFactory()
{
  /* destructor code */
}

/* void CreateDelegate (in nsIRDFResource aOuter, in string aKey, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult aResult); */
NS_IMETHODIMP nsMsgFilterDelegateFactory::CreateDelegate(nsIRDFResource *aOuter, const char *aKey, const nsIID & aIID, void * *aResult)
{
    nsresult rv;
    *aResult = nsnull;
    
    // if it's a folder, then we return a filter list..
    // otherwise make sure it's in the form
    // mailbox://userid@server/foldername;filterName=foo
    

    const char* uri;
    aOuter->GetValueConst(&uri);

    // if it has ';filter' then it's a filter, otherwise we'll assume
    // that it's a folder.
    nsCAutoString uriStr(uri);

    nsCOMPtr<nsISupports> resultSupports;
    
    if (uriStr.Find("filterName=") != -1) {
        nsCOMPtr<nsIMsgFilter> filter;
        rv = getFilterDelegate(aOuter, getter_AddRefs(filter));
        if (NS_SUCCEEDED(rv))
            resultSupports = filter;
        
    }
    else {
        // probably a folder, get the filter list
        nsCOMPtr<nsIMsgFilterList> filterList;
        rv = getFilterListDelegate(aOuter, getter_AddRefs(filterList));
        if (NS_SUCCEEDED(rv))
            resultSupports = filterList;
        
    }

    if (resultSupports)
        return resultSupports->QueryInterface(aIID, aResult);

    return NS_ERROR_FAILURE;
}

nsresult
nsMsgFilterDelegateFactory::getFilterListDelegate(nsIRDFResource *aOuter,
                                                  nsIMsgFilterList **aResult)
{
    nsresult rv;
    
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(aOuter, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIMsgFilterList> filterList;
    rv = folder->GetFilterList(getter_AddRefs(filterList));
    NS_ENSURE_SUCCESS(rv, rv);
    
    *aResult = filterList;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

nsresult
nsMsgFilterDelegateFactory::getFilterDelegate(nsIRDFResource *aOuter,
                                              nsIMsgFilter **aResult)
{
    nsresult rv;
    // now try to find ";filterName="
    nsXPIDLCString uri;
    rv = aOuter->GetValue(getter_Copies(uri));
    if (NS_FAILED(rv)) return rv;
    
    PRInt32 seperatorPosition = 0;
    const char *filterTag = uri;
    while (filterTag && *filterTag != ';') {
        seperatorPosition++;
        filterTag++;
    }
    
    // if no ;, and it's not a folder,
    // I don't know what the heck it is.
    if (!filterTag)
        return NS_ERROR_FAILURE;

    const char *filterName = getFilterName(filterTag);

    nsCOMPtr<nsIMsgFilterList> filterList;
    rv = getFilterList(uri, seperatorPosition, getter_AddRefs(filterList));

        // now that we have the filter list and index, retrieve the filter.

    // XXX convert from UTF8
    nsAutoString filterString;
    PRUnichar *unicodeString =
        nsTextFormatter::smprintf(unicodeFormatter, filterName);
    NS_ENSURE_TRUE(unicodeString, NS_ERROR_OUT_OF_MEMORY);
    
    filterString.Assign(unicodeString);

    nsTextFormatter::smprintf_free(unicodeString);
    
    nsCOMPtr<nsIMsgFilter> filter;
    rv = filterList->GetFilterNamed(filterString.get(), getter_AddRefs(filter));
    if (NS_FAILED(rv)) return rv;

    *aResult = filter;
    NS_IF_ADDREF(*aResult);
    
    return NS_OK;
}
    

const char *
nsMsgFilterDelegateFactory::getFilterName(const char *filterTag)
{
    
    if (nsCRT::strncmp(filterTag, MSGFILTER_TAG, MSGFILTER_TAG_LENGTH) != 0)
        return nsnull;
    
    const char *filterNameStr = filterTag + MSGFILTER_TAG_LENGTH;

    return filterNameStr;
}

nsresult
nsMsgFilterDelegateFactory::getFilterList(const char *aUri,
                                          PRInt32 aTagPosition,
                                          nsIMsgFilterList** aResult)
{
    nsresult rv;
    // now we actually need the filter list, so we truncate the string
    // and use getdelegate to get the filter delegate
    nsCAutoString folderUri((const char*)aUri);
    folderUri.Truncate(aTagPosition);
    
    // convert URI to resource
    nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFResource> filterListResource;
    rdf->GetResource(folderUri.get(), getter_AddRefs(filterListResource));
    NS_ENSURE_SUCCESS(rv, rv);

    return filterListResource->GetDelegate("filter",
                                           NS_GET_IID(nsIMsgFilterList),
                                           (void **)aResult);
}


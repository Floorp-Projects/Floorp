/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMsgMailViewList.h"
#include "nsReadableUtils.h"
#include "nsISupportsArray.h"
#include "nsIFileChannel.h"
#include "nsIMsgFilterService.h"
#include "nsIFileSpec.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"

#include "nsAppDirectoryServiceDefs.h"

nsMsgMailView::nsMsgMailView()
{
    NS_NewISupportsArray(getter_AddRefs(mViewSearchTerms));
}

NS_IMPL_ADDREF(nsMsgMailView)
NS_IMPL_RELEASE(nsMsgMailView)
NS_IMPL_QUERY_INTERFACE1(nsMsgMailView, nsIMsgMailView)

nsMsgMailView::~nsMsgMailView()
{
    if (mViewSearchTerms)
        mViewSearchTerms->Clear();
}

NS_IMETHODIMP nsMsgMailView::GetMailViewName(PRUnichar ** aMailViewName)
{
    *aMailViewName = ToNewUnicode(mName); 
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailView::SetMailViewName(const PRUnichar * aMailViewName)
{
    mName = aMailViewName;
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailView::GetSearchTerms(nsISupportsArray ** aSearchTerms)
{
    NS_ENSURE_ARG_POINTER(aSearchTerms);
    NS_IF_ADDREF(*aSearchTerms = mViewSearchTerms);
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailView::SetSearchTerms(nsISupportsArray * aSearchTerms)
{
    mViewSearchTerms = aSearchTerms;
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailView::AppendTerm(nsIMsgSearchTerm * aTerm)
{
    NS_ENSURE_TRUE(aTerm, NS_ERROR_NULL_POINTER);
    
    return mViewSearchTerms->AppendElement(NS_STATIC_CAST(nsISupports*,aTerm));
}

NS_IMETHODIMP nsMsgMailView::CreateTerm(nsIMsgSearchTerm **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsCOMPtr<nsIMsgSearchTerm> searchTerm = do_CreateInstance("@mozilla.org/messenger/searchTerm;1");
    NS_IF_ADDREF(*aResult = searchTerm);
    return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// nsMsgMailViewList implementation
/////////////////////////////////////////////////////////////////////////////
nsMsgMailViewList::nsMsgMailViewList()
{
    LoadMailViews();
}

NS_IMPL_ADDREF(nsMsgMailViewList)
NS_IMPL_RELEASE(nsMsgMailViewList)
NS_IMPL_QUERY_INTERFACE1(nsMsgMailViewList, nsIMsgMailViewList)

nsMsgMailViewList::~nsMsgMailViewList()
{

}

NS_IMETHODIMP nsMsgMailViewList::GetMailViewCount(PRUint32 * aCount)
{
    if (m_mailViews)
       m_mailViews->Count(aCount);
    else
        *aCount = 0;
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailViewList::GetMailViewAt(PRUint32 aMailViewIndex, nsIMsgMailView ** aMailView)
{
    NS_ENSURE_ARG_POINTER(aMailView);
    NS_ENSURE_TRUE(m_mailViews, NS_ERROR_FAILURE);
    
    PRUint32 mailViewCount;
    m_mailViews->Count(&mailViewCount);
    NS_ENSURE_TRUE(mailViewCount >= aMailViewIndex, NS_ERROR_FAILURE);

    return m_mailViews->QueryElementAt(aMailViewIndex, NS_GET_IID(nsIMsgMailView),
                                      (void **)aMailView);
}

NS_IMETHODIMP nsMsgMailViewList::AddMailView(nsIMsgMailView * aMailView)
{
    NS_ENSURE_ARG_POINTER(aMailView);
    NS_ENSURE_TRUE(m_mailViews, NS_ERROR_FAILURE);

    m_mailViews->AppendElement(NS_STATIC_CAST(nsISupports*, aMailView));
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailViewList::RemoveMailView(nsIMsgMailView * aMailView)
{
    m_mailViews->RemoveElement(NS_STATIC_CAST(nsISupports*, aMailView));
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailViewList::CreateMailView(nsIMsgMailView ** aMailView)
{
    NS_ENSURE_ARG_POINTER(aMailView);

    nsMsgMailView * mailView = new nsMsgMailView;
    NS_ENSURE_TRUE(mailView, NS_ERROR_OUT_OF_MEMORY);

    NS_IF_ADDREF(*aMailView = mailView);    
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailViewList::Save()
{
    // brute force...remove all the old filters in our filter list, then we'll re-add our current
    // list
    nsCOMPtr<nsIMsgFilter> msgFilter;
    PRUint32 numFilters;
    mFilterList->GetFilterCount(&numFilters);
    while (numFilters)
    {
        mFilterList->RemoveFilterAt(numFilters - 1);
        numFilters--;
    }

    // now convert our mail view list into a filter list and save it
    ConvertMailViewListToFilterList();

    // now save the filters to our file
    return mFilterList->SaveToDefaultFile();
}

nsresult nsMsgMailViewList::ConvertMailViewListToFilterList()
{
  PRUint32 mailViewCount = 0;
  m_mailViews->Count(&mailViewCount);
  nsCOMPtr<nsIMsgMailView> mailView;
  nsCOMPtr<nsIMsgFilter> newMailFilter;
  nsXPIDLString mailViewName;
  for (PRUint32 index = 0; index < mailViewCount; index++)
  {
      GetMailViewAt(index, getter_AddRefs(mailView));
      if (!mailView)
          continue;
      mailView->GetMailViewName(getter_Copies(mailViewName));
      mFilterList->CreateFilter(mailViewName, getter_AddRefs(newMailFilter));
      if (!newMailFilter)
          continue;

      nsCOMPtr<nsISupportsArray> searchTerms;
      mailView->GetSearchTerms(getter_AddRefs(searchTerms));
      newMailFilter->SetSearchTerms(searchTerms);
      mFilterList->InsertFilterAt(index, newMailFilter);
  }

  return NS_OK;
}

nsresult nsMsgMailViewList::LoadMailViews()
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = file->AppendNative(nsDependentCString("mailViews.dat"));

    // if the file doesn't exist, we should try to get it from the defaults directory and copy it over
    PRBool exists = PR_FALSE;
    file->Exists(&exists);
    if (!exists)
    {
        nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIFile> defaultMessagesFile;
        rv = mailSession->GetDataFilesDir("messenger", getter_AddRefs(defaultMessagesFile));
        rv = defaultMessagesFile->AppendNative(nsDependentCString("mailViews.dat"));

        nsCOMPtr<nsIFileSpec> defaultMailViewSpec;
        rv = NS_NewFileSpecFromIFile(defaultMessagesFile, getter_AddRefs(defaultMailViewSpec));
        
        // get the profile directory
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(defaultMessagesFile));
        
        // convert to spec
        nsCOMPtr<nsIFileSpec> profileDirSpec;
        rv = NS_NewFileSpecFromIFile(defaultMessagesFile, getter_AddRefs(profileDirSpec));
        // now copy the file over to the profile directory
        defaultMailViewSpec->CopyToDir(profileDirSpec);
    }

    nsCOMPtr<nsIFileSpec> mailViewSpec;
    rv = NS_NewFileSpecFromIFile(file, getter_AddRefs(mailViewSpec));
    if (NS_FAILED(rv)) return rv;

    // this is kind of a hack but I think it will be an effective hack. The filter service already knows how to 
    // take a nsIFileSpec and parse the contents into filters which are very similar to mail views. Intead of
    // re-writing all of that dirty parsing code, let's just re-use it then convert the results into a data strcuture
    // we wish to give to our consumers. 
      
    nsCOMPtr<nsIMsgFilterService> filterService = do_GetService(NS_MSGFILTERSERVICE_CONTRACTID, &rv);
    nsCOMPtr<nsIMsgFilterList> mfilterList;
      
    rv = filterService->OpenFilterList(mailViewSpec, NULL, NULL, getter_AddRefs(mFilterList));
    NS_ENSURE_SUCCESS(rv, rv);

    // now convert the filter list into our mail view objects, stripping out just the info we need
    ConvertFilterListToMailView(mFilterList, getter_AddRefs(m_mailViews));
    return rv;
}

nsresult nsMsgMailViewList::ConvertFilterListToMailView(nsIMsgFilterList * aFilterList, nsISupportsArray ** aMailViewList)
{
    nsresult rv = NS_OK;
    NS_ENSURE_ARG_POINTER(aFilterList);
    NS_ENSURE_ARG_POINTER(aMailViewList);

    nsCOMPtr<nsISupportsArray> mailViewList;
    NS_NewISupportsArray(getter_AddRefs(mailViewList));

    // iterate over each filter in the list
    nsCOMPtr<nsIMsgFilter> msgFilter;
    PRUint32 numFilters;
    aFilterList->GetFilterCount(&numFilters);
    for (PRUint32 index = 0; index < numFilters; index++)
    {
        aFilterList->GetFilterAt(index, getter_AddRefs(msgFilter));
        if (!msgFilter)
            continue;
        // create a new nsIMsgMailView for this item
        nsCOMPtr<nsIMsgMailView> newMailView; 
        rv = CreateMailView(getter_AddRefs(newMailView));
        NS_ENSURE_SUCCESS(rv, rv);
        nsXPIDLString filterName;
        msgFilter->GetFilterName(getter_Copies(filterName));
        newMailView->SetMailViewName(filterName);

        nsCOMPtr<nsISupportsArray> filterSearchTerms;
        msgFilter->GetSearchTerms(getter_AddRefs(filterSearchTerms));
        newMailView->SetSearchTerms(filterSearchTerms);

        // now append this new mail view to our global list view
        mailViewList->AppendElement(NS_STATIC_CAST(nsISupports*, newMailView));
    }

    NS_IF_ADDREF(*aMailViewList = mailViewList);

    return rv;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsAbMDBDirectory.h" 
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "nsAddrDatabase.h"
#include "nsIAbMDBCard.h"
#include "nsIAbListener.h"
#include "nsIAddrBookSession.h"
#include "nsIAddressBook.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsAbDirectoryQuery.h"
#include "nsIAbDirectoryQueryProxy.h"
#include "nsAbQueryStringToExpression.h"
#include "nsAbUtils.h"
#include "nsArrayEnumerator.h"
#include "nsAbMDBCardProperty.h"

#include "mdb.h"
#include "prprf.h"

// XXX todo
// fix this -1,0,1 crap, use an enum or #define

nsAbMDBDirectory::nsAbMDBDirectory(void):
     nsRDFResource(),
     mInitialized(PR_FALSE),
     mIsMailingList(-1),
     mIsQueryURI(PR_FALSE),
     mPerformingQuery(PR_FALSE)
{
}

nsAbMDBDirectory::~nsAbMDBDirectory(void)
{
  if (mDatabase) {
    mDatabase->RemoveListener(this);
  }
}

NS_IMPL_ISUPPORTS_INHERITED4(nsAbMDBDirectory, nsRDFResource,
                             nsIAbDirectory,
                             nsIAbMDBDirectory,
                             nsIAbDirectorySearch,
                             nsIAddrDBListener)

////////////////////////////////////////////////////////////////////////////////

nsresult nsAbMDBDirectory::RemoveCardFromAddressList(nsIAbCard* card)
{
  nsresult rv = NS_OK;
  PRUint32 listTotal;
  PRInt32 i, j;
  rv = m_AddressList->Count(&listTotal);
  NS_ENSURE_SUCCESS(rv,rv);

  for (i = listTotal - 1; i >= 0; i--)
  {            
    nsCOMPtr<nsIAbDirectory> listDir(do_QueryElementAt(m_AddressList, i, &rv));
    if (listDir)
    {
      nsCOMPtr <nsISupportsArray> pAddressLists;
      listDir->GetAddressLists(getter_AddRefs(pAddressLists));
      if (pAddressLists)
      {  
        PRUint32 total;
        rv = pAddressLists->Count(&total);
        for (j = total - 1; j >= 0; j--)
        {
          nsCOMPtr<nsIAbCard> cardInList(do_QueryElementAt(pAddressLists, j, &rv));
          PRBool equals;
          nsresult rv = cardInList->Equals(card, &equals);  // should we checking email?
          if (NS_SUCCEEDED(rv) && equals) {
            pAddressLists->RemoveElementAt(j);
        }
      }
    }
  }
  }
  return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::ModifyDirectory(nsIAbDirectory *directory, nsIAbDirectoryProperties *aProperties)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsAbMDBDirectory::DeleteDirectory(nsIAbDirectory *directory)
{
  nsresult rv = NS_OK;
  
  if (!directory)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAbMDBDirectory> dbdirectory(do_QueryInterface(directory, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString uri;
  rv = dbdirectory->GetDirUri(getter_Copies(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAddrDatabase> database;
  nsCOMPtr<nsIAddressBook> addressBook = do_GetService(NS_ADDRESSBOOK_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    rv = addressBook->GetAbDatabaseFromURI(uri.get(), getter_AddRefs(database));        

    if (NS_SUCCEEDED(rv))
      rv = database->DeleteMailList(directory, PR_TRUE);

    if (NS_SUCCEEDED(rv))
      database->Commit(nsAddrDBCommitType::kLargeCommit);

    if (m_AddressList)
      m_AddressList->RemoveElement(directory);
    rv = mSubDirectories.RemoveObject(directory);

    NotifyItemDeleted(directory);
  }
  

  return rv;
}

nsresult nsAbMDBDirectory::NotifyItemChanged(nsISupports *item)
{
  nsresult rv;
  nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = abSession->NotifyItemPropertyChanged(item, nsnull, nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

nsresult nsAbMDBDirectory::NotifyPropertyChanged(nsIAbDirectory *list, const char *property, const PRUnichar* oldValue, const PRUnichar* newValue)
{
  nsresult rv;
  nsCOMPtr<nsISupports> supports = do_QueryInterface(list, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = abSession->NotifyItemPropertyChanged(supports, property, oldValue, newValue);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

nsresult nsAbMDBDirectory::NotifyItemAdded(nsISupports *item)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
  if(NS_SUCCEEDED(rv))
    abSession->NotifyDirectoryItemAdded(this, item);
  return NS_OK;
}

nsresult nsAbMDBDirectory::NotifyItemDeleted(nsISupports *item)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
  if(NS_SUCCEEDED(rv))
    abSession->NotifyDirectoryItemDeleted(this, item);

  return NS_OK;
}


// nsIRDFResource methods

NS_IMETHODIMP nsAbMDBDirectory::Init(const char* aURI)
{
  nsresult rv;

  rv = nsRDFResource::Init(aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mURINoQuery = aURI;

  nsCOMPtr<nsIURI> uri = do_CreateInstance (NS_STANDARDURL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uri->SetSpec(nsDependentCString(aURI));
  NS_ENSURE_SUCCESS(rv, rv);

  mIsValidURI = PR_TRUE;

    nsCOMPtr<nsIURL> url = do_QueryInterface(uri);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString queryString;
  rv = url->GetQuery (queryString);

  nsCAutoString path;
  rv = url->GetPath (path);
  mPath = path;

  if (!queryString.IsEmpty())
  {
    mPath.Truncate(path.Length() - queryString.Length() - 1);

    mURINoQuery.Truncate(mURINoQuery.Length() - queryString.Length() - 1);

    mQueryString = queryString;

    mIsQueryURI = PR_TRUE;
  }
  else
    mIsQueryURI = PR_FALSE;

  return rv;
}


// nsIAbMDBDirectory methods

NS_IMETHODIMP nsAbMDBDirectory::ClearDatabase()
{       
  if (mIsQueryURI)
    return NS_ERROR_NOT_IMPLEMENTED;

  if (mDatabase)
  {
    mDatabase->RemoveListener(this);
    mDatabase = nsnull; 
  }
  return NS_OK; 
}

NS_IMETHODIMP nsAbMDBDirectory::RemoveElementsFromAddressList()
{
  if (mIsQueryURI)
    return NS_ERROR_NOT_IMPLEMENTED;

  if (m_AddressList)
  {
    PRUint32 count;
    nsresult rv;
    rv = m_AddressList->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    PRInt32 i;
    for (i = count - 1; i >= 0; i--)
      m_AddressList->RemoveElementAt(i);
  }
  m_AddressList = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::RemoveEmailAddressAt(PRUint32 aIndex)
{
  if (mIsQueryURI)
    return NS_ERROR_NOT_IMPLEMENTED;

  if (m_AddressList)
  {
    return m_AddressList->RemoveElementAt(aIndex);
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbMDBDirectory::AddDirectory(const char *uriName, nsIAbDirectory **childDir)
{
  if (mIsQueryURI)
    return NS_ERROR_NOT_IMPLEMENTED;

  if (!childDir || !uriName)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIRDFResource> res;
  rv = rdf->GetResource(nsDependentCString(uriName), getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  mSubDirectories.AppendObject(directory);
  NS_IF_ADDREF(*childDir = directory);
  return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::GetDirUri(char **uri)
{
  NS_ASSERTION(uri, "Null out param");
  NS_ASSERTION(!mURI.IsEmpty(), "Not initialized?");

  *uri = ToNewCString(mURI);

  if (!*uri)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}



// nsIAbDirectory methods

NS_IMETHODIMP nsAbMDBDirectory::GetChildNodes(nsISimpleEnumerator* *aResult)
{
  if (mIsQueryURI)
  {
    nsCOMArray<nsIAbDirectory> children;
    return NS_NewArrayEnumerator(aResult, children);
  }

  mInitialized = PR_TRUE;
  return NS_NewArrayEnumerator(aResult, mSubDirectories);
}

PR_STATIC_CALLBACK(PRBool) enumerateSearchCache(nsHashKey *aKey, void *aData, void* closure)
{
        nsISupportsArray* array = (nsISupportsArray* )closure;
  nsIAbCard* card = (nsIAbCard* )aData;

  array->AppendElement (card);
        return PR_TRUE;
}

NS_IMETHODIMP nsAbMDBDirectory::GetChildCards(nsIEnumerator* *result)
{
  if (mIsQueryURI)
  {
    nsresult rv;
    rv = StartSearch();
    NS_ENSURE_SUCCESS(rv, rv);

    // TODO
    // Search is synchronous so need to return
    // results after search is complete
    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    mSearchCache.Enumerate(enumerateSearchCache, (void*)array);
    return array->Enumerate(result);
  }

  NS_ASSERTION(!mURI.IsEmpty(), "Not Initialized?");
  if (mIsMailingList == -1)
  {
    /* directory URIs are of the form
     * moz-abmdbdirectory://foo
     * mailing list URIs are of the form
     * moz-abmdbdirectory://foo/bar
     */
    NS_ENSURE_TRUE(mURI.Length() > kMDBDirectoryRootLen, NS_ERROR_UNEXPECTED);
    if (strchr(mURI.get() + kMDBDirectoryRootLen, '/'))
      mIsMailingList = 1;
    else
      mIsMailingList = 0;
  }

  nsresult rv = GetAbDatabase();

  if (NS_SUCCEEDED(rv) && mDatabase)
  {
    if (mIsMailingList == 0)
      rv = mDatabase->EnumerateCards(this, result);
    else if (mIsMailingList == 1)
      rv = mDatabase->EnumerateListAddresses(this, result);
  }

  return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::DeleteCards(nsISupportsArray *cards)
{
  nsresult rv = NS_OK;

  if (mIsQueryURI) {
    // if this is a query, delete the cards from the directory (without the query)
    // before we do the delete, make this directory (which represents the search)
    // a listener on the database, so that it will get notified when the cards are deleted
    // after delete, remove this query as a listener.
    nsCOMPtr<nsIAddressBook> addressBook = do_GetService(NS_ADDRESSBOOK_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIAddrDatabase> database;
    rv = addressBook->GetAbDatabaseFromURI(mURINoQuery.get(), getter_AddRefs(database));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = database->AddListener(this);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFResource> resource;
    rv = gRDFService->GetResource(mURINoQuery, getter_AddRefs(resource));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIAbDirectory> directory = do_QueryInterface(resource, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = directory->DeleteCards(cards);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = database->RemoveListener(this);
    NS_ENSURE_SUCCESS(rv, rv);
    return rv;
  }

  if (!mDatabase)
    rv = GetAbDatabase();

  if (NS_SUCCEEDED(rv) && mDatabase)
  {
    PRUint32 cardCount;
    PRUint32 i;
    rv = cards->Count(&cardCount);
    NS_ENSURE_SUCCESS(rv, rv);
    for (i = 0; i < cardCount; i++)
    {
      nsCOMPtr<nsIAbCard> card;
      nsCOMPtr<nsIAbMDBCard> dbcard;
      card = do_QueryElementAt(cards, i, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      dbcard = do_QueryInterface(card, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      if (card)
      {
        if (IsMailingList())
        {
          mDatabase->DeleteCardFromMailList(this, card, PR_TRUE);

          PRUint32 cardTotal;
          PRInt32 i;
          rv = m_AddressList->Count(&cardTotal);
          for (i = cardTotal - 1; i >= 0; i--)
          {            
            nsCOMPtr<nsIAbMDBCard> dbarrayCard(do_QueryElementAt(m_AddressList, i, &rv));
            if (dbarrayCard)
            {
              PRUint32 tableID, rowID, cardTableID, cardRowID; 
              dbarrayCard->GetDbTableID(&tableID);
              dbarrayCard->GetDbRowID(&rowID);
              dbcard->GetDbTableID(&cardTableID);
              dbcard->GetDbRowID(&cardRowID);
              if (tableID == cardTableID && rowID == cardRowID)
                m_AddressList->RemoveElementAt(i);
            }
          }
        }
        else
        {
          mDatabase->DeleteCard(card, PR_TRUE);
          PRBool bIsMailList = PR_FALSE;
          card->GetIsMailList(&bIsMailList);
          if (bIsMailList)
          {
            //to do, get mailing list dir side uri and notify rdf to remove it
            PRUint32 rowID;
            dbcard->GetDbRowID(&rowID);
            char *listUri = PR_smprintf("%s/MailList%ld", mURI.get(), rowID);
            if (listUri)
            {
              nsresult rv = NS_OK;
              nsCOMPtr<nsIRDFService> rdfService = 
                       do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);

              if(NS_SUCCEEDED(rv))
                {
                nsCOMPtr<nsIRDFResource> listResource;
                rv = rdfService->GetResource(nsDependentCString(listUri),
                                             getter_AddRefs(listResource));
                nsCOMPtr<nsIAbDirectory> listDir = do_QueryInterface(listResource, &rv);
                if(NS_SUCCEEDED(rv))
                  {
                  if (m_AddressList)
                    m_AddressList->RemoveElement(listDir);
                  rv = mSubDirectories.RemoveObject(listDir);

                  if (listDir)
                    NotifyItemDeleted(listDir);
                  PR_smprintf_free(listUri);
                  }
                else 
                  {
                  PR_smprintf_free(listUri);
                  return rv;
                  }
                }
              else
                {
                PR_smprintf_free(listUri);
                return rv;
                }
            }
          }
          else
          { 
            rv = RemoveCardFromAddressList(card);
            NS_ENSURE_SUCCESS(rv,rv);
          }
        }
      }
    }
    mDatabase->Commit(nsAddrDBCommitType::kLargeCommit);
  }
  return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::HasCard(nsIAbCard *cards, PRBool *hasCard)
{
  if(!hasCard)
    return NS_ERROR_NULL_POINTER;

  if (mIsQueryURI)
  {
    nsVoidKey key (NS_STATIC_CAST(void*, cards));
    *hasCard = mSearchCache.Exists (&key);
    return NS_OK;
  }

  nsresult rv = NS_OK;
  if (!mDatabase)
    rv = GetAbDatabase();

  if(NS_SUCCEEDED(rv) && mDatabase)
  {
    if(NS_SUCCEEDED(rv))
      rv = mDatabase->ContainsCard(cards, hasCard);

  }
  return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::HasDirectory(nsIAbDirectory *dir, PRBool *hasDir)
{
  if (!hasDir)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIAbMDBDirectory> dbdir(do_QueryInterface(dir, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool bIsMailingList  = PR_FALSE;
  dir->GetIsMailList(&bIsMailingList);
  if (bIsMailingList)
  {
    nsXPIDLCString uri;
    rv = dbdir->GetDirUri(getter_Copies(uri));
        NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAddrDatabase> database;
      nsCOMPtr<nsIAddressBook> addressBook = do_GetService(NS_ADDRESSBOOK_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      rv = addressBook->GetAbDatabaseFromURI(uri.get(), getter_AddRefs(database));        
    }
    if(NS_SUCCEEDED(rv) && database)
    {
      if(NS_SUCCEEDED(rv))
        rv = database->ContainsMailList(dir, hasDir);
    }
  }

  return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::CreateNewDirectory(nsIAbDirectoryProperties *aProperties)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbMDBDirectory::CreateDirectoryByURI(const PRUnichar *dirName, const char *uri, PRBool migrating)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbMDBDirectory::AddMailListWithKey(nsIAbDirectory *list, PRUint32 *key)
{
  return(InternalAddMailList(list, key));
}

NS_IMETHODIMP nsAbMDBDirectory::AddMailList(nsIAbDirectory *list)
{
  return(InternalAddMailList(list, nsnull));
}

nsresult nsAbMDBDirectory::InternalAddMailList(nsIAbDirectory *list, PRUint32 *key)
{
  if (mIsQueryURI)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv = NS_OK;
  if (!mDatabase)
    rv = GetAbDatabase();

  if (NS_FAILED(rv) || !mDatabase)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAbMDBDirectory> dblist(do_QueryInterface(list, &rv));
  if (NS_FAILED(rv))
  {
    // XXX fix this.
    nsAbMDBDirProperty* dblistproperty = new nsAbMDBDirProperty ();
    if (!dblistproperty)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dblistproperty);
    nsCOMPtr<nsIAbDirectory> newlist = getter_AddRefs(NS_STATIC_CAST(nsIAbDirectory*, dblistproperty));
    newlist->CopyMailList(list);
    list = newlist;
    dblist = do_QueryInterface(list, &rv);
  }

  if (!key)
    mDatabase->CreateMailListAndAddToDB(list, PR_TRUE);
  else
    mDatabase->CreateMailListAndAddToDBWithKey(list, PR_TRUE, key);
  mDatabase->Commit(nsAddrDBCommitType::kLargeCommit);

  PRUint32 dbRowID;
  dblist->GetDbRowID(&dbRowID);

  nsCAutoString listUri;
  listUri = mURI + NS_LITERAL_CSTRING("/MailList");
  listUri.AppendInt(dbRowID);

  nsCOMPtr<nsIAbDirectory> newList;
  rv = AddDirectory(listUri.get(), getter_AddRefs(newList));
  nsCOMPtr<nsIAbMDBDirectory> dbnewList(do_QueryInterface(newList));
  if (NS_SUCCEEDED(rv) && newList)
  {
    nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(newList, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = mDatabase->AddListener(listener);
    NS_ENSURE_SUCCESS(rv, rv);

    dbnewList->CopyDBMailList (dblist);
    AddMailListToDirectory(newList);
    NotifyItemAdded(newList);
  }

  return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::AddCard(nsIAbCard* card, nsIAbCard **addedCard)
{
  if (mIsQueryURI)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv = NS_OK;
  if (!mDatabase)
    rv = GetAbDatabase();

  if (NS_FAILED(rv) || !mDatabase)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAbCard> newCard;
  nsCOMPtr<nsIAbMDBCard> dbcard;

  dbcard = do_QueryInterface(card, &rv);
  if (NS_FAILED(rv) || !dbcard) {
    dbcard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

    newCard = do_QueryInterface(dbcard, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
  
    rv = newCard->Copy(card);
  NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    newCard = card;
  }

  dbcard->SetAbDatabase (mDatabase);
  if (mIsMailingList == 1)
    mDatabase->CreateNewListCardAndAddToDB(this, m_dbRowID, newCard, PR_TRUE /* notify */);
  else
    mDatabase->CreateNewCardAndAddToDB(newCard, PR_TRUE);
  mDatabase->Commit(nsAddrDBCommitType::kLargeCommit);

  NS_IF_ADDREF(*addedCard = newCard);
  return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::DropCard(nsIAbCard* aCard, PRBool needToCopyCard)
{
  NS_ENSURE_ARG_POINTER(aCard);

  if (mIsQueryURI)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv = NS_OK;

  // Don't add the card if it's not a normal one (ie, they can't be added as members).
  PRBool isNormal;
  rv = aCard->GetIsANormalCard(&isNormal);
  if (!isNormal)
    return NS_OK;

  NS_ASSERTION(!mURI.IsEmpty(), "Not initialized?");
  if (mIsMailingList == -1)
  {
    /* directory URIs are of the form
     * moz-abmdbdirectory://foo
     * mailing list URIs are of the form
     * moz-abmdbdirectory://foo/bar
     */
    NS_ENSURE_TRUE(mURI.Length() > kMDBDirectoryRootLen, NS_ERROR_UNEXPECTED);
    if (strchr(mURI.get() + kMDBDirectoryRootLen, '/'))
      mIsMailingList = 1;
    else
      mIsMailingList = 0;
  }
  if (!mDatabase)
    rv = GetAbDatabase();

  if (NS_FAILED(rv) || !mDatabase)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAbCard> newCard;
  nsCOMPtr<nsIAbMDBCard> dbcard;

  if (needToCopyCard) {
    dbcard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

    newCard = do_QueryInterface(dbcard, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
  
    rv = newCard->Copy(aCard);
  NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    dbcard = do_QueryInterface(aCard, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    newCard = aCard;
  }  

  dbcard->SetAbDatabase(mDatabase);

  if (mIsMailingList == 1) {
    if (needToCopyCard) {
      // first, add the card to the directory that contains the mailing list.
      mDatabase->CreateNewCardAndAddToDB(newCard, PR_TRUE /* notify */);
    }
    // since we didn't copy the card, we don't have to notify that it was inserted
    mDatabase->CreateNewListCardAndAddToDB(this, m_dbRowID, newCard, PR_FALSE /* notify */);
  }
  else {
    mDatabase->CreateNewCardAndAddToDB(newCard, PR_TRUE /* notify */);
  }
  mDatabase->Commit(nsAddrDBCommitType::kLargeCommit);
  return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::EditMailListToDatabase(const char *uri, nsIAbCard *listCard)
{
  if (mIsQueryURI)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv = NS_OK;

  nsCOMPtr<nsIAddrDatabase>  listDatabase;  

  nsCOMPtr<nsIAddressBook> addressBook = do_GetService(NS_ADDRESSBOOK_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = addressBook->GetAbDatabaseFromURI(uri, getter_AddRefs(listDatabase));

  if (listDatabase)
  {
    listDatabase->EditMailList(this, listCard, PR_TRUE);
    listDatabase->Commit(nsAddrDBCommitType::kLargeCommit);
    listDatabase = nsnull;

    return NS_OK;

  }
  else
    return NS_ERROR_FAILURE;
}

// nsIAddrDBListener methods

NS_IMETHODIMP nsAbMDBDirectory::OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::OnCardEntryChange
(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator)
{
  NS_ENSURE_ARG_POINTER(card);
  nsCOMPtr<nsISupports> cardSupports(do_QueryInterface(card));
  nsresult rv;

  switch (abCode) {
  case AB_NotifyInserted:
    rv = NotifyItemAdded(cardSupports);
    break;
  case AB_NotifyDeleted:
    rv = NotifyItemDeleted(cardSupports);
    break;
  case AB_NotifyPropertyChanged:
    rv = NotifyItemChanged(cardSupports);
    break;
  default:
    rv = NS_ERROR_UNEXPECTED;
    break;
  }
    
          NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::OnListEntryChange
(PRUint32 abCode, nsIAbDirectory *list, nsIAddrDBListener *instigator)
{
  nsresult rv = NS_OK;
  
  if (abCode == AB_NotifyPropertyChanged && list)
  {
    PRBool bIsMailList = PR_FALSE;
    rv = list->GetIsMailList(&bIsMailList);
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIAbMDBDirectory> dblist(do_QueryInterface(list, &rv));
    NS_ENSURE_SUCCESS(rv,rv);

    if (bIsMailList) {
        nsXPIDLString pListName;
      rv = list->GetDirName(getter_Copies(pListName));
      NS_ENSURE_SUCCESS(rv,rv);

      rv = NotifyPropertyChanged(list, "DirName", nsnull, pListName.get());
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator)
{
  if (mDatabase)
      mDatabase->RemoveListener(this);
  return NS_OK;
}

// nsIAbDirectorySearch methods

NS_IMETHODIMP nsAbMDBDirectory::StartSearch()
{
  if (!mIsQueryURI)
    return NS_ERROR_FAILURE;

  nsresult rv;

  mPerformingQuery = PR_TRUE;
  mSearchCache.Reset();

  nsCOMPtr<nsIAbDirectoryQueryArguments> arguments = do_CreateInstance(NS_ABDIRECTORYQUERYARGUMENTS_CONTRACTID,&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAbBooleanExpression> expression;
  rv = nsAbQueryStringToExpression::Convert(mQueryString.get(),
    getter_AddRefs(expression));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = arguments->SetExpression(expression);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the return properties to
  // return nsIAbCard interfaces
  nsCStringArray properties;
  properties.AppendCString(nsCAutoString("card:nsIAbCard"));
  CharPtrArrayGuard returnProperties(PR_FALSE);
  rv = CStringArrayToCharPtrArray::Convert(properties,returnProperties.GetSizeAddr(),
          returnProperties.GetArrayAddr(), PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = arguments->SetReturnProperties(returnProperties.GetSize(), returnProperties.GetArray());
  NS_ENSURE_SUCCESS(rv, rv);

  // don't search the subdirectories 
  // if the current directory is a mailing list, it won't have any subdirectories
  // if the current directory is a addressbook, searching both it
  // and the subdirectories (the mailing lists), will yield duplicate results
  // because every entry in a mailing list will be an entry in the parent addressbook
  rv = arguments->SetQuerySubDirectories(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the the query listener
  nsCOMPtr<nsIAbDirectoryQueryResultListener> queryListener;
  nsAbDirSearchListener* _queryListener =
    new nsAbDirSearchListener (this);
  queryListener = _queryListener;

  // Get the directory without the query
  nsCOMPtr<nsIRDFResource> resource;
  rv = gRDFService->GetResource (mURINoQuery, getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Initiate the proxy query with the no query directory
  nsCOMPtr<nsIAbDirectoryQueryProxy> queryProxy = 
      do_CreateInstance(NS_ABDIRECTORYQUERYPROXY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = queryProxy->Initiate(directory);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = queryProxy->DoQuery(arguments, queryListener, -1, 0, &mContext);
  return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::StopSearch()
{
  if (!mIsQueryURI)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


// nsAbDirSearchListenerContext methods

nsresult nsAbMDBDirectory::OnSearchFinished (PRInt32 result)
{
  mPerformingQuery = PR_FALSE;
  return NS_OK;
}

nsresult nsAbMDBDirectory::OnSearchFoundCard (nsIAbCard* card)
{
  nsVoidKey key (NS_STATIC_CAST(void*, card));
  mSearchCache.Put (&key, card);

  // TODO
  // Search is synchronous so asserting on the
  // datasource will not work since the getChildCards
  // method will not have returned with results.
  // NotifyItemAdded (card);
  return NS_OK;
}

nsresult nsAbMDBDirectory::GetAbDatabase()
{
  NS_ASSERTION(!mURI.IsEmpty(), "Not initialized?");

  if (!mDatabase) {
    nsresult rv;

    nsCOMPtr<nsIAddressBook> addressBook = do_GetService(NS_ADDRESSBOOK_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = addressBook->GetAbDatabaseFromURI(mURI.get(), getter_AddRefs(mDatabase));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mDatabase->AddListener(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mDatabase)
    return NS_ERROR_NULL_POINTER;
  return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::HasCardForEmailAddress(const char * aEmailAddress, PRBool * aCardExists)
{
  nsresult rv = NS_OK;
  *aCardExists = PR_FALSE;

  if (!mDatabase)
    rv = GetAbDatabase();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAbCard> card; 
  mDatabase->GetCardFromAttribute(this, kLowerPriEmailColumn /* see #196777 */, aEmailAddress, PR_TRUE /* caseInsensitive, see bug #191798 */, getter_AddRefs(card));
  if (card)
    *aCardExists = PR_TRUE;
  else 
  {
    // fix for bug #187239
    // didn't find it as the primary email?  try again, with k2ndEmailColumn ("Additional Email")
    // 
    // TODO bug #198731
    // unlike the kPriEmailColumn, we don't have kLower2ndEmailColumn
    // so we will still suffer from bug #196777 for "additional emails"
    mDatabase->GetCardFromAttribute(this, k2ndEmailColumn, aEmailAddress, PR_TRUE /* caseInsensitive, see bug #191798 */, getter_AddRefs(card));
    if (card)
      *aCardExists = PR_TRUE;
  }
  return NS_OK;
}

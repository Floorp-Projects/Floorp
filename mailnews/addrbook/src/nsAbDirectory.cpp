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

#include "msgCore.h"    // precompiled header...

#include "nsAbDirectory.h"	 
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "nsAbCard.h"
	 
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kAbCardCID, NS_ABCARDRESOURCE_CID);

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsABDirectory::nsABDirectory(void)
  :  nsRDFResource(), mListeners(nsnull),
     mInitialized(PR_FALSE), mCardInitialized(PR_FALSE),
     mCsid(0), mDepth(0), mPrefFlags(0)
{
//  NS_INIT_REFCNT(); done by superclass

	NS_NewISupportsArray(getter_AddRefs(mSubDirectories));
	NS_NewISupportsArray(getter_AddRefs(mSubCards));

	//The rdf:addressdirectory datasource is going to be a listener to all nsIAbDirectory, so add
	//it as a listener
    nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
    if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFDataSource> datasource;
		rv = rdfService->GetDataSource("rdf:addressdirectory", getter_AddRefs(datasource));
		if(NS_SUCCEEDED(rv))
		{   /*
			nsCOMPtr<nsIAbListener> directoryListener(do_QueryInterface(datasource, &rv));
			if(NS_SUCCEEDED(rv))
			{
				AddAddrBookListener(directoryListener);
			}*/

		}
	} 
}

nsABDirectory::~nsABDirectory(void)
{
	if(mSubDirectories)
	{
		PRUint32 count;
		nsresult rv = mSubDirectories->Count(&count);
		NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
		int i;
		for (i = count - 1; i >= 0; i--)
			mSubDirectories->RemoveElementAt(i);
	}

	if(mSubCards)
	{
		PRUint32 count;
		nsresult rv = mSubCards->Count(&count);
		NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");

		int i;
		for (i = count - 1; i >= 0; i--)
			mSubCards->RemoveElementAt(i);
	}

	if (mListeners) 
	{
		PRInt32 i;
		for (i = mListeners->Count() - 1; i >= 0; --i) 
			mListeners->RemoveElementAt(i);
		delete mListeners;
	}

}

NS_IMPL_ISUPPORTS_INHERITED(nsABDirectory, nsRDFResource, nsIAbDirectory)

////////////////////////////////////////////////////////////////////////////////

typedef PRBool
(*nsArrayFilter)(nsISupports* element, void* data);

#if 0
static nsresult
nsFilterBy(nsISupportsArray* array, nsArrayFilter filter, void* data,
           nsISupportsArray* *result)
{
  nsCOMPtr<nsISupportsArray> f;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(f));
  if (NS_FAILED(rv)) return rv;
  PRUint32 count;
  rv = array->Count(&count);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  PRUint32 i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> element = getter_AddRefs(array->ElementAt(i));
    if (filter(element, data)) {
      rv = f->AppendElement(element);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  *result = f;
  return NS_OK;
}

#endif

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsABDirectory::AddUnique(nsISupports* element)
{
  // XXX fix this
  return mSubDirectories->AppendElement(element);
}

NS_IMETHODIMP
nsABDirectory::ReplaceElement(nsISupports* element, nsISupports* newElement)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsABDirectory::GetChildNodes(nsIEnumerator* *result)
{
  if (!mInitialized) 
  {
    if (!PL_strcmp(mURI, "abdirectory:/") && GetDirList())
	{
		int count = GetDirList()->Count();
		int i;
		for (i = 0; i < count; i++)
		{
			DIR_Server *server = (DIR_Server *)GetDirList()->ElementAt(i);
			nsCOMPtr<nsIAbDirectory> childDir;
			nsAutoString currentDirStr = "Pab";
			currentDirStr.Append(server->position, 10);
			AddSubDirectory(currentDirStr, getter_AddRefs(childDir));
		}
	}
    mInitialized = PR_TRUE;
  }
  return mSubDirectories->Enumerate(result);
}

nsresult nsABDirectory::AddSubDirectory(nsAutoString name, nsIAbDirectory **childDir)
{
	if(!childDir)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);

	if(NS_FAILED(rv))
		return rv;
	
	nsAutoString uri;
	uri.Append(mURI);
	uri.Append("/");

	uri.Append(name);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriStr, getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;
	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;        
	delete[] uriStr;

	mSubDirectories->AppendElement(directory);
	*childDir = directory;
	NS_ADDREF(*childDir);
	 
    (void)nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

	return rv;
}

NS_IMETHODIMP nsABDirectory::GetChildCards(nsIEnumerator* *result)
{
	if (!mCardInitialized) 
	{
		if (!PL_strcmp(mURI, "abdirectory://Pab1") ||
			!PL_strcmp(mURI, "abdirectory://Pab2") ||
			!PL_strcmp(mURI, "abdirectory://Pab3"))
		{
			int j;
			for (j = 0; j < 2; j++)
			{   
				nsAutoString currentCardStr;
				if (!PL_strcmp(mURI, "abdirectory://Pab1"))
                    currentCardStr.Append("abcard://Pab1/Card");	
				if (!PL_strcmp(mURI, "abdirectory://Pab2"))
                    currentCardStr.Append("abcard://Pab2/Card");	
				if (!PL_strcmp(mURI, "abdirectory://Pab3"))
                    currentCardStr.Append("abcard://Pab3/Card");	
				nsCOMPtr<nsIAbCard> childCard;
				if (j == 0) currentCardStr.Append('1');
				if (j == 1) currentCardStr.Append('2');
				AddChildCards(currentCardStr, getter_AddRefs(childCard));
			}
		}
		mCardInitialized = PR_TRUE;
	}
//	return mSubCards->Enumerate(result);
	return mSubDirectories->Enumerate(result);
}

nsresult nsABDirectory::AddChildCards(nsAutoString name, nsIAbCard **childCard)
{
	if(!childCard)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);

	if(NS_FAILED(rv))
		return rv;

	nsAutoString uri(name);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriStr, getter_AddRefs(res));
	if (NS_FAILED(rv))
	{
		delete[] uriStr;
		return rv;
	}
	nsCOMPtr<nsIAbCard> personCard(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
	{
		rv = nsComponentManager::CreateInstance(kAbCardCID, nsnull, nsIAbCard::GetIID(), getter_AddRefs(personCard));
		if (NS_FAILED(rv) || !personCard)
		{
			delete[] uriStr;
			return rv;
		}
	}
	delete[] uriStr;

//	mSubCards->AppendElement(personCard);
	mSubDirectories->AppendElement(personCard);
	*childCard = personCard;
	NS_ADDREF(*childCard);

	return rv;
}


NS_IMETHODIMP nsABDirectory::CreateCardFromDirectory(nsIAbCard* *result)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectory::AddAddrBookListener(nsIAbListener * listener)
{
  if (! mListeners)
	{
		mListeners = new nsVoidArray();
		if(!mListeners)
			return NS_ERROR_OUT_OF_MEMORY;
  }
  mListeners->AppendElement(listener);
  return NS_OK;
}

NS_IMETHODIMP nsABDirectory::RemoveAddrBookListener(nsIAbListener * listener)
{
  if (! mListeners)
    return NS_OK;
  mListeners->RemoveElement(listener);
  return NS_OK;

}


////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsABDirectory::GetName(char **name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	if (!PL_strcmp(mURI, "abdirectory://Pab1/Card1"))
		SetName("Person1");
	if (!PL_strcmp(mURI, "abdirectory://Pab1/Card2"))
		SetName("Person2");
	if (!PL_strcmp(mURI, "abdirectory://Pab2/Card1"))
		SetName("Person3");
	if (!PL_strcmp(mURI, "abdirectory://Pab2/Card2"))
		SetName("Person4");
	if (!PL_strcmp(mURI, "abdirectory://Pab3/Card1"))
		SetName("Person5");
	if (!PL_strcmp(mURI, "abdirectory://Pab3/Card2"))
		SetName("Person6");
	else if (GetDirList())
	{
		int count = GetDirList()->Count();
		int i;
		for (i = 0; i < count; i++)
		{
			DIR_Server *server = (DIR_Server *)GetDirList()->ElementAt(i);
			nsCOMPtr<nsIAbDirectory> childDir;
			nsAutoString currentDirStr = "abdirectory://Pab";
			currentDirStr.Append(server->position, 10);
			char* dirUrl = currentDirStr.ToNewCString();
			if (dirUrl == nsnull) 
				return NS_ERROR_OUT_OF_MEMORY;
			if (!PL_strcmp(mURI, dirUrl))
				SetName(server->description);
			delete[] dirUrl;
		}
	}
	*name = mDirName.ToNewCString();
	return NS_OK;
}

NS_IMETHODIMP nsABDirectory::SetName(char * name)
{
	mDirName = name;
	return NS_OK;
}

NS_IMETHODIMP nsABDirectory::GetParent(nsIAbBase* *parent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectory::SetParent(nsIAbBase *parent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectory::GetChildNamed(const char* name, nsISupports* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectory::ContainsChildNamed(const char *name, PRBool* containsChild)
{
	nsCOMPtr<nsISupports> child;
	
	if(containsChild)
	{
		*containsChild = PR_FALSE;
		if(NS_SUCCEEDED(GetChildNamed(name, getter_AddRefs(child))))
		{
			*containsChild = child != nsnull;
		}
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsABDirectory::FindParentOf(nsIAbDirectory * aDirectory, nsIAbDirectory ** aParent)
{
	if(!aParent)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	*aParent = nsnull;

	PRUint32 i, j, count;
	rv = mSubDirectories->Count(&count);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
	nsCOMPtr<nsISupports> supports;
	nsCOMPtr<nsIAbDirectory> child;

	for (i = 0; i < count && *aParent == NULL; i++)
	{
		supports = getter_AddRefs(mSubDirectories->ElementAt(i));
		child = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv) && child)
		{
			if (aDirectory == child.get())
			{
				*aParent = this;
				NS_ADDREF(*aParent);
				return NS_OK;
			}
		}
	}

	for (j = 0; j < count && *aParent == NULL; j++)
	{
/*
		supports = getter_AddRefs(mSubDirectories->ElementAt(j));
		child = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv) && child)
		{
			rv = child->FindParentOf(aDirectory, aParent);
			if(NS_SUCCEEDED(rv))
				return rv;
		}
*/
	}

	return rv;

}

NS_IMETHODIMP nsABDirectory::IsParentOf(nsIAbDirectory *child, PRBool deep, PRBool *isParent)
{
	if(!isParent)
		return NS_ERROR_NULL_POINTER;
	
	nsresult rv = NS_OK;

	PRUint32 i, count;
	rv = mSubDirectories->Count(&count);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");

	for (i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubDirectories->ElementAt(i));
		nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(supports, &rv));
		if(NS_SUCCEEDED(rv))
		{
			if (directory.get() == child )
				*isParent = PR_TRUE;
			else if(deep)
			{;
//				directory->IsParentOf(child, deep, isParent);
			}
		}
		if(*isParent)
			return NS_OK;
    }
	*isParent = PR_FALSE;
	return rv;
}


#ifdef HAVE_DB
NS_IMETHOD GetTotalPersonsInDB(PRUint32 *totalPersons) const;					// How many messages in database.
#endif
	


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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDirectoryDataSource.h"
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
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *object;
} nsAbRDFNotification;
                                                
static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kAbDirectoryDataSourceCID, NS_ABDIRECTORYDATASOURCE_CID);
static NS_DEFINE_CID(kAbDirectoryCID, NS_ABDIRECTORYRESOURCE_CID); 
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);

nsIRDFResource* nsAbDirectoryDataSource::kNC_Child = nsnull;
nsIRDFResource* nsAbDirectoryDataSource::kNC_DirName = nsnull;

nsIRDFResource* nsAbDirectoryDataSource::kNC_MailingList = nsnull;
nsIRDFResource* nsAbDirectoryDataSource::kNC_CardChild = nsnull;

// commands
nsIRDFResource* nsAbDirectoryDataSource::kNC_Delete = nsnull;
nsIRDFResource* nsAbDirectoryDataSource::kNC_NewDirectory = nsnull;

#define NC_RDF_CHILD				"http://home.netscape.com/NC-rdf#child"
#define NC_RDF_DIRNAME			    "http://home.netscape.com/NC-rdf#DirName"
#define NC_RDF_MAILINGLIST			"http://home.netscape.com/NC-rdf#MailingList"
#define NC_RDF_CARDCHILD			"http://home.netscape.com/NC-rdf#CardChild"

//Directory Commands
#define NC_RDF_DELETE				"http://home.netscape.com/NC-rdf#Delete"
#define NC_RDF_NEWDIRECTORY			"http://home.netscape.com/NC-rdf#NewDirectory"

////////////////////////////////////////////////////////////////////////

nsAbDirectoryDataSource::nsAbDirectoryDataSource():
  mInitialized(PR_FALSE),
  mRDFService(nsnull)
{

}

nsAbDirectoryDataSource::~nsAbDirectoryDataSource (void)
{

	if (mRDFService)
	{
		mRDFService->UnregisterDataSource(this);
		nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); 
		mRDFService = nsnull;
	}
	
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->RemoveAddressBookListener(this);

	nsrefcnt refcnt;
	NS_RELEASE2(kNC_Child, refcnt);
	NS_RELEASE2(kNC_DirName, refcnt);
	NS_RELEASE2(kNC_MailingList, refcnt);
	NS_RELEASE2(kNC_CardChild, refcnt);

	NS_RELEASE2(kNC_Delete, refcnt);
	NS_RELEASE2(kNC_NewDirectory, refcnt);

	/* free all directories */
	DIR_ShutDown();
}

nsresult
nsAbDirectoryDataSource::Init()
{
	if (mInitialized)
		return NS_ERROR_ALREADY_INITIALIZED;

	nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
											 nsCOMTypeInfo<nsIRDFService>::GetIID(),
											 (nsISupports**) &mRDFService); 
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if (NS_SUCCEEDED(rv))
		abSession->AddAddressBookListener(this);

	mRDFService->RegisterDataSource(this, PR_FALSE);

	if (!kNC_Child)
	{
		mRDFService->GetResource(NC_RDF_CHILD, &kNC_Child);
		mRDFService->GetResource(NC_RDF_DIRNAME, &kNC_DirName);
		mRDFService->GetResource(NC_RDF_MAILINGLIST, &kNC_MailingList);
		mRDFService->GetResource(NC_RDF_CARDCHILD, &kNC_CardChild);

		mRDFService->GetResource(NC_RDF_DELETE, &kNC_Delete);
		mRDFService->GetResource(NC_RDF_NEWDIRECTORY, &kNC_NewDirectory);
	}

	DIR_GetDirServers();

	mInitialized = PR_TRUE;
	return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(nsAbDirectoryDataSource, nsAbRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsAbDirectoryDataSource, nsAbRDFDataSource)

NS_IMETHODIMP nsAbDirectoryDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if(iid.Equals(nsCOMTypeInfo<nsIAbListener>::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIAbListener*, this);
		NS_ADDREF(this);
		return NS_OK;
	}
	else
		return nsAbRDFDataSource::QueryInterface(iid, result);
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsAbDirectoryDataSource::GetURI(char* *uri)
{
  if ((*uri = nsXPIDLCString::Copy("rdf:addressdirectory")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsAbDirectoryDataSource::GetTarget(nsIRDFResource* source,
                                               nsIRDFResource* property,
                                               PRBool tv,
                                               nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  // we only have positive assertions in the mail data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv) && directory) {
    rv = createDirectoryNode(directory, property, target);
  }
  else
	  return NS_RDF_NO_VALUE;
  return rv;
}


NS_IMETHODIMP nsAbDirectoryDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,    
                                                PRBool tv,
                                                nsISimpleEnumerator** targets)
{
  nsresult rv = NS_RDF_NO_VALUE;

  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv) && directory)
  {
    if ((kNC_Child == property))
    {
      nsCOMPtr<nsIEnumerator> subDirectories;

      rv = directory->GetChildNodes(getter_AddRefs(subDirectories));
      if (NS_FAILED(rv)) return rv;
      nsAdapterEnumerator* cursor =
        new nsAdapterEnumerator(subDirectories);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
    else if((kNC_DirName == property)) 
   { 
      nsSingletonEnumerator* cursor =
        new nsSingletonEnumerator(property);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
    else if((kNC_CardChild == property))
    { 
      nsCOMPtr<nsIEnumerator> cardChild;

      rv = directory->GetChildCards(getter_AddRefs(cardChild));
      if (NS_SUCCEEDED(rv) && cardChild)
	  {
		  nsAdapterEnumerator* cursor =
			new nsAdapterEnumerator(cardChild);
		  if (cursor == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		  NS_ADDREF(cursor);
		  *targets = cursor;
		  rv = NS_OK;
	  }
    }
    else if((kNC_MailingList == property))
    { 
      nsCOMPtr<nsIEnumerator> mailingList;

      rv = directory->GetMailingList(getter_AddRefs(mailingList));
      if (NS_FAILED(rv)) return rv;
      nsAdapterEnumerator* cursor =
        new nsAdapterEnumerator(mailingList);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
  }
  else {
	  //create empty cursor
	  nsCOMPtr<nsISupportsArray> assertions;
      NS_NewISupportsArray(getter_AddRefs(assertions));
	  nsArrayEnumerator* cursor = 
		  new nsArrayEnumerator(assertions);
	  if(cursor == nsnull)
		  return NS_ERROR_OUT_OF_MEMORY;
	  NS_ADDREF(cursor);
	  *targets = cursor;
	  rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsAbDirectoryDataSource::Assert(nsIRDFResource* source,
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv)
{
	nsresult rv;
	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(source, &rv));
	//We don't handle tv = PR_FALSE at the moment.
	if(NS_SUCCEEDED(rv) && tv)
		return DoDirectoryAssert(directory, property, target);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbDirectoryDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
	nsresult rv;
	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(source, &rv));
	if(NS_SUCCEEDED(rv))
		return DoDirectoryHasAssertion(directory, property, target, tv, hasAssertion);
	else
		*hasAssertion = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsAbDirectoryDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                 nsISimpleEnumerator** labels)
{
  nsCOMPtr<nsISupportsArray> arcs;
  nsresult rv = NS_RDF_NO_VALUE;

  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    fflush(stdout);
   rv = getDirectoryArcLabelsOut(directory, getter_AddRefs(arcs));
  }
  else {
    // how to return an empty cursor?
    // for now return a 0-length nsISupportsArray
    NS_NewISupportsArray(getter_AddRefs(arcs));
  }

  nsArrayEnumerator* cursor =
    new nsArrayEnumerator(arcs);
  
  if (cursor == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(cursor);
  *labels = cursor;
  
  return NS_OK;
}

nsresult
nsAbDirectoryDataSource::getDirectoryArcLabelsOut(nsIAbDirectory *directory,
                                             nsISupportsArray **arcs)
{
	nsresult rv;
	rv = NS_NewISupportsArray(arcs);
	if(NS_FAILED(rv))
		return rv;
	
	(*arcs)->AppendElement(kNC_DirName);
	(*arcs)->AppendElement(kNC_Child);
	(*arcs)->AppendElement(kNC_CardChild);
	return NS_OK;
}

NS_IMETHODIMP
nsAbDirectoryDataSource::GetAllCommands(nsIRDFResource* source,
                                      nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> cmds;

  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewISupportsArray(getter_AddRefs(cmds));
    if (NS_FAILED(rv)) return rv;
    cmds->AppendElement(kNC_Delete);
    cmds->AppendElement(kNC_NewDirectory);
  }

  if (cmds != nsnull)
    return cmds->Enumerate(commands);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAbDirectoryDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                        PRBool* aResult)
{
  nsresult rv;
  nsCOMPtr<nsIAbDirectory> directory;

  PRUint32 i, cnt;
  rv = aSources->Count(&cnt);
  for (i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> source = getter_AddRefs(aSources->ElementAt(i));
		directory = do_QueryInterface(source, &rv);
    if (NS_SUCCEEDED(rv)) {
      // we don't care about the arguments -- directory commands are always enabled
      if (!((aCommand == kNC_Delete) ||
		    (aCommand == kNC_NewDirectory))) {
        *aResult = PR_FALSE;
        return NS_OK;
      }
    }
  }
  *aResult = PR_TRUE;
  return NS_OK; // succeeded for all sources
}

NS_IMETHODIMP
nsAbDirectoryDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	PRUint32 i, cnt;
	nsresult rv = aSources->Count(&cnt);
	if (NS_FAILED(rv)) return rv;

	for (i = 0; i < cnt; i++) 
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(aSources->ElementAt(i));
		nsCOMPtr<nsIAbDirectory> directory = do_QueryInterface(supports, &rv);
		if (NS_SUCCEEDED(rv)) 
		{
			if ((aCommand == kNC_Delete))  
				rv = DoDeleteFromDirectory(directory, aArguments);
			else if((aCommand == kNC_NewDirectory)) 
				rv = DoNewDirectory(directory, aArguments);
		}
	}
	//for the moment return NS_OK, because failure stops entire DoCommand process.
	return NS_OK;
}

NS_IMETHODIMP nsAbDirectoryDataSource::OnItemAdded(nsISupports *parentDirectory, nsISupports *item)
{
	nsresult rv;
	nsCOMPtr<nsIAbCard> card;
	nsCOMPtr<nsIAbDirectory> directory;
	nsCOMPtr<nsIRDFResource> parentResource;

	if(NS_SUCCEEDED(parentDirectory->QueryInterface(nsCOMTypeInfo<nsIRDFResource>::GetIID(), getter_AddRefs(parentResource))))
	{ 
		//If we are adding a card
		if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIAbCard>::GetIID(), getter_AddRefs(card))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if (NS_SUCCEEDED(rv))
			{
				//Notify directories that a message was added.
				NotifyObservers(parentResource, kNC_CardChild, itemNode, PR_TRUE);
			}
		}
		//If we are adding a directory
		else if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIAbDirectory>::GetIID(), getter_AddRefs(directory))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify a directory was added.
				NotifyObservers(parentResource, kNC_Child, itemNode, PR_TRUE);
			}
		}
	}

	return NS_OK;
}

NS_IMETHODIMP nsAbDirectoryDataSource::OnItemRemoved(nsISupports *parentDirectory, nsISupports *item)
{
	nsresult rv;
	nsCOMPtr<nsIAbCard> card;
	nsCOMPtr<nsIAbDirectory> directory;
	nsCOMPtr<nsIRDFResource> parentResource;

	if(NS_SUCCEEDED(parentDirectory->QueryInterface(nsCOMTypeInfo<nsIRDFResource>::GetIID(), getter_AddRefs(parentResource))))
	{
		//If we are removing a card
		if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIAbCard>::GetIID(), getter_AddRefs(card))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify directories that a card was deleted.
				NotifyObservers(parentResource, kNC_CardChild, itemNode, PR_FALSE);
			}
		}
		//If we are removing a directory
		else if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIAbDirectory>::GetIID(), getter_AddRefs(directory))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify a directory was deleted.
				NotifyObservers(parentResource, kNC_Child, itemNode, PR_FALSE);
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbDirectoryDataSource::OnItemPropertyChanged(nsISupports *item, const char *property,
														   const char *oldValue, const char *newValue)

{
	nsresult rv;
	nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item, &rv));

	if(NS_SUCCEEDED(rv))
	{
		if(PL_strcmp("DirName", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_DirName, oldValue, newValue);
		}
	}
	return NS_OK;
}

nsresult nsAbDirectoryDataSource::createDirectoryNode(nsIAbDirectory* directory,
                                                 nsIRDFResource* property,
                                                 nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;
  
  if ((kNC_DirName == property))
	rv = createDirectoryNameNode(directory, target);
//  else if ((kNC_Child == property))
//    rv = createDirectoryChildNode(directory,target);
//  else if ((kNC_CardChild == property))
//    rv = createCardChildNode(directory,target);
  
  return rv;
}


nsresult nsAbDirectoryDataSource::createDirectoryNameNode(nsIAbDirectory *directory,
                                                     nsIRDFNode **target)
{
  char *name;
  nsresult rv = directory->GetDirName(&name);
  if (NS_FAILED(rv)) return rv;
  nsString nameString(name);
  createNode(nameString, target);
  delete[] name;
  return NS_OK;
}

nsresult
nsAbDirectoryDataSource::createDirectoryChildNode(nsIAbDirectory *directory,
                                             nsIRDFNode **target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  return NS_FAILED(rv) ? NS_RDF_NO_VALUE : rv;
}

nsresult
nsAbDirectoryDataSource::createCardChildNode(nsIAbDirectory *directory,
                                             nsIRDFNode **target)
{
  char *name;
  nsresult rv = directory->GetDirName(&name);
  if (NS_FAILED(rv)) return rv;
  nsString nameString(name);
  createNode(nameString, target);
  delete[] name;
  return NS_OK;
}

nsresult nsAbDirectoryDataSource::DoDeleteFromDirectory(nsIAbDirectory *directory, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	PRUint32 itemCount;
	rv = arguments->Count(&itemCount);
	if (NS_FAILED(rv)) return rv;
	
	nsCOMPtr<nsISupportsArray> cardArray, dirArray;
	NS_NewISupportsArray(getter_AddRefs(cardArray));
	NS_NewISupportsArray(getter_AddRefs(dirArray));

	//Split up deleted items into different type arrays to be passed to the folder
	//for deletion.
	PRUint32 item;
	for(item = 0; item < itemCount; item++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(arguments->ElementAt(item));
		nsCOMPtr<nsIAbCard> deletedCard(do_QueryInterface(supports));
		nsCOMPtr<nsIAbDirectory> deletedDir(do_QueryInterface(supports));
		if (deletedCard)
		{
			cardArray->AppendElement(supports);
		}
		else if(deletedDir)
		{
			dirArray->AppendElement(supports);
		}
	}
	PRUint32 cnt;
	rv = cardArray->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	if (cnt > 0)
		rv = directory->DeleteCards(cardArray);

	rv = dirArray->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	if (cnt > 0)
		rv = directory->DeleteDirectories(dirArray);

	return rv;
}


nsresult nsAbDirectoryDataSource::DoNewDirectory(nsIAbDirectory *directory, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsISupports> elem = getter_AddRefs(arguments->ElementAt(0));
	nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(elem, &rv);
	if(NS_SUCCEEDED(rv))
	{
		PRUnichar *name;
		literal->GetValue(&name);
		nsString tempStr = name;
		nsAutoCString nameStr(tempStr);

		rv = directory->CreateNewDirectory(nameStr, nsnull);
	}
	return rv;
}


nsresult nsAbDirectoryDataSource::DoDirectoryAssert(nsIAbDirectory *directory, nsIRDFResource *property, nsIRDFNode *target)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}


nsresult nsAbDirectoryDataSource::DoDirectoryHasAssertion(nsIAbDirectory *directory, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion)
{
	nsresult rv = NS_OK;
	if (!hasAssertion)
		return NS_ERROR_NULL_POINTER;

	//We're not keeping track of negative assertions on directory.
	if (!tv)
	{
		*hasAssertion = PR_FALSE;
		return NS_OK;
	}

	if ((kNC_CardChild == property))
	{
		nsCOMPtr<nsIAbCard> card(do_QueryInterface(target, &rv));
		if(NS_SUCCEEDED(rv))
			rv = directory->HasCard(card, hasAssertion);
	}
	else if ((kNC_Child == property))
	{
		nsCOMPtr<nsIAbDirectory> newDirectory(do_QueryInterface(target, &rv));
		if(NS_SUCCEEDED(rv))
			rv = directory->HasDirectory(newDirectory, hasAssertion);
	}
	else 
		*hasAssertion = PR_FALSE;

	return rv;

}

nsresult NS_NewAbDirectoryDataSource(const nsIID& iid, void **result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nsAbDirectoryDataSource* datasource = new nsAbDirectoryDataSource();
    if (! datasource)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = datasource->Init();
    if (NS_FAILED(rv)) {
        delete datasource;
        return rv;
    }

	return datasource->QueryInterface(iid, result);
}

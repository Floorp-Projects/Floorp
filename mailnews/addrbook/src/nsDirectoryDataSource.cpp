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

nsIRDFResource* nsABDirectoryDataSource::kNC_Child;
nsIRDFResource* nsABDirectoryDataSource::kNC_DirName;

nsIRDFResource* nsABDirectoryDataSource::kNC_DirChild;
nsIRDFResource* nsABDirectoryDataSource::kNC_CardChild;

// commands
nsIRDFResource* nsABDirectoryDataSource::kNC_Delete;
nsIRDFResource* nsABDirectoryDataSource::kNC_NewDirectory;

#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, DirName);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, DirChild);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, CardChild);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Delete);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, NewDirectory);

////////////////////////////////////////////////////////////////////////
// Utilities


void nsABDirectoryDataSource::createNode(nsString& str, nsIRDFNode **node)
{
	*node = nsnull;
    nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv); 
    if (NS_FAILED(rv)) return;   // always check this before proceeding 
	nsIRDFLiteral * value;
	if (NS_SUCCEEDED(rdf->GetLiteral(str.GetUnicode(), &value))) 
	{
		*node = value;
	}
}

void nsABDirectoryDataSource::createNode(PRUint32 value, nsIRDFNode **node)
{
	char *valueStr = PR_smprintf("%d", value);
	nsString str(valueStr);
	createNode(str, node);
	PR_smprintf_free(valueStr);
}

nsABDirectoryDataSource::nsABDirectoryDataSource():
  mObservers(nsnull),
  mInitialized(PR_FALSE),
  mRDFService(nsnull)
{
	NS_INIT_REFCNT();

  // XXX This call should be moved to a NS_NewMsgFooDataSource()
  // method that the factory calls, so that failure to construct
  // will return an error code instead of returning a partially
  // initialized object.
  nsresult rv = Init();
	PR_ASSERT(NS_SUCCEEDED(rv));
}

nsABDirectoryDataSource::~nsABDirectoryDataSource (void)
{
	mRDFService->UnregisterDataSource(this);

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->RemoveAddressBookListener(this);

	nsrefcnt refcnt;
	NS_RELEASE2(kNC_Child, refcnt);
	NS_RELEASE2(kNC_DirName, refcnt);
	NS_RELEASE2(kNC_DirChild, refcnt);
	NS_RELEASE2(kNC_CardChild, refcnt);

	NS_RELEASE2(kNC_Delete, refcnt);
	NS_RELEASE2(kNC_NewDirectory, refcnt);
	if (mRDFService)
	{
		nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); 
		mRDFService = nsnull;
	}
	
	/* free all directories */
	DIR_ShutDown();
}

nsresult
nsABDirectoryDataSource::Init()
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
		mRDFService->GetResource(kURINC_child,		&kNC_Child);
	if (!kNC_DirName)
		mRDFService->GetResource(kURINC_DirName,	&kNC_DirName);
	if (!kNC_DirChild)
		mRDFService->GetResource(kURINC_DirChild,	&kNC_DirChild);
	if (!kNC_CardChild)
		mRDFService->GetResource(kURINC_CardChild,	&kNC_CardChild);

	if (!kNC_Delete)
		mRDFService->GetResource(kURINC_Delete, &kNC_Delete);
	if (!kNC_NewDirectory)
		mRDFService->GetResource(kURINC_NewDirectory, &kNC_NewDirectory);
	
	DIR_GetDirServers();

	mInitialized = PR_TRUE;
	return NS_OK;
}


NS_IMPL_ADDREF(nsABDirectoryDataSource)
NS_IMPL_RELEASE(nsABDirectoryDataSource)

NS_IMETHODIMP
nsABDirectoryDataSource::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if (iid.Equals(nsCOMTypeInfo<nsIRDFDataSource>::GetIID()) ||
		iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIRDFDataSource*, this);
		AddRef();
		return NS_OK;
	}
	else if(iid.Equals(nsCOMTypeInfo<nsIAbListener>::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIAbListener*, this);
		AddRef();
		return NS_OK;
	}
	return NS_NOINTERFACE;
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsABDirectoryDataSource::GetURI(char* *uri)
{
  if ((*uri = nsXPIDLCString::Copy("rdf:addressdirectory")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsABDirectoryDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               PRBool tv,
                                               nsIRDFResource** source /* out */)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectoryDataSource::GetTarget(nsIRDFResource* source,
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


NS_IMETHODIMP nsABDirectoryDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                PRBool tv,
                                                nsISimpleEnumerator** sources)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectoryDataSource::GetTargets(nsIRDFResource* source,
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
      if (NS_FAILED(rv)) return rv;
      nsAdapterEnumerator* cursor =
        new nsAdapterEnumerator(cardChild);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
    else if((kNC_DirChild == property))
    { 
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

NS_IMETHODIMP nsABDirectoryDataSource::Assert(nsIRDFResource* source,
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

NS_IMETHODIMP nsABDirectoryDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
  return NS_RDF_ASSERTION_REJECTED;//NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectoryDataSource::Change(nsIRDFResource *aSource,
                                              nsIRDFResource *aProperty,
                                              nsIRDFNode *aOldTarget,
                                              nsIRDFNode *aNewTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectoryDataSource::Move(nsIRDFResource *aOldSource,
                                            nsIRDFResource *aNewSource,
                                            nsIRDFResource *aProperty,
                                            nsIRDFNode *aTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsABDirectoryDataSource::HasAssertion(nsIRDFResource* source,
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

NS_IMETHODIMP nsABDirectoryDataSource::AddObserver(nsIRDFObserver* n)
{
  if (! mObservers) {
    nsresult rv;
    rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
    if (NS_FAILED(rv)) return rv;
  }
  mObservers->AppendElement(n);
  return NS_OK;
}

NS_IMETHODIMP nsABDirectoryDataSource::RemoveObserver(nsIRDFObserver* n)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(n);
  return NS_OK;
}

PRBool
nsABDirectoryDataSource::assertEnumFunc(nsISupports *aElement, void *aData)
{
  nsAbRDFNotification *note = (nsAbRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;
  
  observer->OnAssert(note->subject,
                     note->property,
                     note->object);
  return PR_TRUE;
}

PRBool
nsABDirectoryDataSource::unassertEnumFunc(nsISupports *aElement, void *aData)
{
  nsAbRDFNotification* note = (nsAbRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnUnassert(note->subject,
                     note->property,
                     note->object);
  return PR_TRUE;
}

nsresult nsABDirectoryDataSource::NotifyObservers(nsIRDFResource *subject,
                                                nsIRDFResource *property,
                                                nsIRDFNode *object,
                                                PRBool assert)
{
	if(mObservers)
	{
		nsAbRDFNotification note = { subject, property, object };
		if (assert)
			mObservers->EnumerateForwards(assertEnumFunc, &note);
		else
			mObservers->EnumerateForwards(unassertEnumFunc, &note);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABDirectoryDataSource::ArcLabelsIn(nsIRDFNode* node,
                                                nsISimpleEnumerator** labels)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectoryDataSource::ArcLabelsOut(nsIRDFResource* source,
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
nsABDirectoryDataSource::getDirectoryArcLabelsOut(nsIAbDirectory *directory,
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
nsABDirectoryDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsABDirectoryDataSource::GetAllCommands(nsIRDFResource* source,
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
nsABDirectoryDataSource::GetAllCmds(nsIRDFResource* source,
                                      nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsABDirectoryDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
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
nsABDirectoryDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
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

NS_IMETHODIMP nsABDirectoryDataSource::OnItemAdded(nsIAbBase *parentDirectory, nsISupports *item)
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
				//Notify folders that a message was added.
				NotifyObservers(parentResource, kNC_Child, itemNode, PR_TRUE);
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsABDirectoryDataSource::OnItemRemoved(nsIAbBase *parentDirectory, nsISupports *item)
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
				//Notify folders that a message was deleted.
				NotifyObservers(parentResource, kNC_CardChild, itemNode, PR_FALSE);
			}
		}
		//If we are removing a directory
		else if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIAbDirectory>::GetIID(), getter_AddRefs(directory))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify folders that a message was deleted.
				NotifyObservers(parentResource, kNC_Child, itemNode, PR_FALSE);
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsABDirectoryDataSource::OnItemPropertyChanged(nsISupports *item, const char *property,
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

nsresult nsABDirectoryDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
													  nsIRDFResource *propertyResource,
													  const char *oldValue, const char *newValue)
{
	nsCOMPtr<nsIRDFNode> newValueNode;
	nsString newValueStr = newValue;
	createNode(newValueStr, getter_AddRefs(newValueNode));
	NotifyObservers(resource, propertyResource, newValueNode, PR_TRUE);

	if (oldValue)
	{
		nsCOMPtr<nsIRDFNode> oldValueNode;
		nsString oldValueStr = oldValue;
		createNode(oldValueStr, getter_AddRefs(oldValueNode));
		NotifyObservers(resource, propertyResource, oldValueNode, PR_FALSE);
	}
	return NS_OK;
}

nsresult nsABDirectoryDataSource::createDirectoryNode(nsIAbDirectory* directory,
                                                 nsIRDFResource* property,
                                                 nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;
  
  if ((kNC_DirName == property))
	rv = createDirectoryNameNode(directory, target);
  else if ((kNC_Child == property))
    rv = createDirectoryChildNode(directory,target);
  else if ((kNC_CardChild == property))
    rv = createCardChildNode(directory,target);
  
  return rv;
}


nsresult nsABDirectoryDataSource::createDirectoryNameNode(nsIAbDirectory *directory,
                                                     nsIRDFNode **target)
{
  char *name;
  nsresult rv = directory->GetName(&name);
  if (NS_FAILED(rv)) return rv;
  nsString nameString(name);
  createNode(nameString, target);
  delete[] name;
  return NS_OK;
}

nsresult
nsABDirectoryDataSource::createDirectoryChildNode(nsIAbDirectory *directory,
                                             nsIRDFNode **target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  return NS_FAILED(rv) ? NS_RDF_NO_VALUE : rv;
}

nsresult
nsABDirectoryDataSource::createCardChildNode(nsIAbDirectory *directory,
                                             nsIRDFNode **target)
{
  char *name;
  nsresult rv = directory->GetName(&name);
  if (NS_FAILED(rv)) return rv;
  nsString nameString(name);
  createNode(nameString, target);
  delete[] name;
  return NS_OK;
}

nsresult nsABDirectoryDataSource::DoDeleteFromDirectory(nsIAbDirectory *directory, nsISupportsArray *arguments)
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
	for(PRUint32 item = 0; item < itemCount; item++)
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

	return rv;
}

nsresult nsABDirectoryDataSource::DoNewDirectory(nsIAbDirectory *directory, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsISupports> elem = getter_AddRefs(arguments->ElementAt(0)); 
	nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(elem, &rv));
	if(NS_SUCCEEDED(rv))
	{
		PRUnichar *name;
		literal->GetValue(&name);
		nsString tempStr = name;
		nsAutoCString nameStr(tempStr);

//		rv = directory->CreateSubDirectory(nameStr);
	}
	return rv;
}

nsresult nsABDirectoryDataSource::DoDirectoryAssert(nsIAbDirectory *directory, nsIRDFResource *property, nsIRDFNode *target)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}


nsresult nsABDirectoryDataSource::DoDirectoryHasAssertion(nsIAbDirectory *directory, nsIRDFResource *property, nsIRDFNode *target,
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

	if((kNC_CardChild == property))
	{
		nsCOMPtr<nsIAbCard> card(do_QueryInterface(target, &rv));
		if(NS_SUCCEEDED(rv))
			rv = directory->HasCard(card, hasAssertion);
	}
	else 
		*hasAssertion = PR_FALSE;

	return rv;

}


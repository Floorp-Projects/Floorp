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

#include "msgCore.h"    // precompiled header...

#include "nsDirectoryDataSource.h"
#include "nsAbBaseCID.h"
#include "nsAbDirectory.h"

#include "nsIMsgDatabase.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"


// this is used for notification of observers using nsVoidArray
typedef struct _nsAbRDFNotification {
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *object;
} nsAbRDFNotification;
                                                


static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kAbDirectoryDataSourceCID, NS_ABDIRECTORYDATASOURCE_CID);

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

static PRBool
peq(nsIRDFResource* r1, nsIRDFResource* r2)
{
	PRBool result;
	if (NS_SUCCEEDED(r1->EqualsResource(r2, &result)) && result) 
		return PR_TRUE;
	else 
		return PR_FALSE;
}

#if 0
static PRBool
peqSort(nsIRDFResource* r1, nsIRDFResource* r2, PRBool *isSort)
{
	if(!isSort)
		return PR_FALSE;

	char *r1Str, *r2Str;
	nsString r1nsStr, r2nsStr, r1nsSortStr;

	r1->GetValue(&r1Str);
	r2->GetValue(&r2Str);

	r1nsStr = r1Str;
	r2nsStr = r2Str;
	r1nsSortStr = r1Str;

	delete[] r1Str;
	delete[] r2Str;

	//probably need to not assume this will always come directly after property.
	r1nsSortStr +="?sort=true";

	if(r1nsStr == r2nsStr)
	{
		*isSort = PR_FALSE;
		return PR_TRUE;
	}
	else if(r1nsSortStr == r2nsStr)
	{
		*isSort = PR_TRUE;
		return PR_TRUE;
	}
	else
	{
		//In case the resources are equal but the values are different.  I'm not sure if this
		//could happen but it is feasible given interface.
		*isSort = PR_FALSE;
		return(peq(r1, r2));
	}
}
#endif

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
  mURI(nsnull),
  mObservers(nsnull),
  mInitialized(PR_FALSE),
  mRDFService(nsnull)
{
	NS_INIT_REFCNT();

	nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
											 nsIRDFService::GetIID(),
											 (nsISupports**) &mRDFService); 

	PR_ASSERT(NS_SUCCEEDED(rv));
}

nsABDirectoryDataSource::~nsABDirectoryDataSource (void)
{
	mRDFService->UnregisterDataSource(this);

	PL_strfree(mURI);
	if (mObservers) 
	{
		PRInt32 i;
		for (i = mObservers->Count() - 1; i >= 0; --i) 
		{
			nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
			NS_RELEASE(obs);
		}
		delete mObservers;
	}
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


NS_IMPL_ADDREF(nsABDirectoryDataSource)
NS_IMPL_RELEASE(nsABDirectoryDataSource)

NS_IMETHODIMP
nsABDirectoryDataSource::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if (iid.Equals(nsIRDFDataSource::GetIID()) ||
	    iid.Equals(kISupportsIID))
	{
		*result = NS_STATIC_CAST(nsIRDFDataSource*, this);
		AddRef();
		return NS_OK;
	}
	else if(iid.Equals(nsIAbListener::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIAbListener*, this);
		AddRef();
		return NS_OK;
	}
	return NS_NOINTERFACE;
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsABDirectoryDataSource::Init(const char* uri)
{
	if (mInitialized)
		return NS_ERROR_ALREADY_INITIALIZED;

	if ((mURI = PL_strdup(uri)) == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

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

NS_IMETHODIMP nsABDirectoryDataSource::GetURI(char* *uri)
{
  if ((*uri = nsXPIDLCString::Copy(mURI)) == nsnull)
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
    if (peq(kNC_Child, property))
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
    else if(peq(kNC_DirName, property)) 
   { 
      nsSingletonEnumerator* cursor =
        new nsSingletonEnumerator(property);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
    else if(peq(kNC_CardChild, property))
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
    else if(peq(kNC_DirChild, property))
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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectoryDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsABDirectoryDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
  *hasAssertion = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsABDirectoryDataSource::AddObserver(nsIRDFObserver* n)
{
  if (! mObservers) {
    if ((mObservers = new nsVoidArray()) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
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
nsABDirectoryDataSource::assertEnumFunc(void *aElement, void *aData)
{
  nsAbRDFNotification *note = (nsAbRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;
  
  observer->OnAssert(note->subject,
                     note->property,
                     note->object);
  return PR_TRUE;
}

PRBool
nsABDirectoryDataSource::unassertEnumFunc(void *aElement, void *aData)
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
	
	nsCOMPtr<nsIEnumerator>  subDirectory;
	if (NS_SUCCEEDED(directory->GetChildNodes(getter_AddRefs(subDirectory))))
	{
		if(NS_OK == subDirectory->First())
		{
			nsCOMPtr<nsISupports> firstDir;
			rv = subDirectory->CurrentItem(getter_AddRefs(firstDir));
			if (NS_SUCCEEDED(rv))
				(*arcs)->AppendElement(kNC_Child);
		}
	}

	nsCOMPtr<nsIEnumerator> cards;
	if(NS_SUCCEEDED(directory->GetChildCards(getter_AddRefs(cards)))) 
	{
		if(NS_OK == cards->First())
		{
			nsCOMPtr<nsISupports> firstCard;
			rv = cards->CurrentItem(getter_AddRefs(firstCard));
			if (NS_SUCCEEDED(rv))
				(*arcs)->AppendElement(kNC_CardChild);
		}
	}
	return NS_OK;
}

NS_IMETHODIMP
nsABDirectoryDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectoryDataSource::Flush()
{
  PR_ASSERT(0);
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
      if (!(peq(aCommand, kNC_Delete) ||
		    peq(aCommand, kNC_NewDirectory))) {
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
  nsresult rv = NS_OK;

  // XXX need to handle batching of command applied to all sources

  PRUint32 i, cnt;
  rv = aSources->Count(&cnt);
  for (i = 0; i < cnt; i++) {
		nsCOMPtr<nsISupports> supports = getter_AddRefs(aSources->ElementAt(i));
    nsCOMPtr<nsIAbDirectory> directory = do_QueryInterface(supports, &rv);
    if (NS_SUCCEEDED(rv)) {
      if (peq(aCommand, kNC_Delete)) {
		rv = DoDeleteFromDirectory(directory, aArguments);
      }
	  else if(peq(aCommand, kNC_NewDirectory)) {
		rv = DoNewDirectory(directory, aArguments);
	  }

    }
  }
  return rv;
}

NS_IMETHODIMP nsABDirectoryDataSource::OnItemAdded(nsIAbBase *parentDirectory, nsISupports *item)
{
	nsresult rv;
	nsCOMPtr<nsIAbDirectory> directory;
	nsCOMPtr<nsIRDFResource> parentResource;

	if(NS_SUCCEEDED(parentDirectory->QueryInterface(nsIRDFResource::GetIID(), getter_AddRefs(parentResource))))
	{
		//If we are adding a directory
		if(NS_SUCCEEDED(item->QueryInterface(nsIAbDirectory::GetIID(), getter_AddRefs(directory))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify directories that a message was added.
				NotifyObservers(parentResource, kNC_Child, itemNode, PR_TRUE);
			}
		}
	}
  return NS_OK;
}

NS_IMETHODIMP nsABDirectoryDataSource::OnItemRemoved(nsIAbBase *parentDirectory, nsISupports *item)
{
/*	
	nsresult rv;
	nsCOMPtr<nsIAbCard> card;
	nsCOMPtr<nsIRDFResource> parentResource;

	if(NS_SUCCEEDED(parentDirectory->QueryInterface(nsIRDFResource::GetIID(), getter_AddRefs(parentResource))))
	{
		//If we are adding a card
		if(NS_SUCCEEDED(item->QueryInterface(nsIAbCard::GetIID(), getter_AddRefs(card))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify directories that a card was deleted.
				NotifyObservers(parentResource, kNC_CardChild, itemNode, PR_FALSE);
			}
		}
	}*/
  return NS_OK;
}

NS_IMETHODIMP nsABDirectoryDataSource::OnItemPropertyChanged(nsISupports *item, const char *property,
														   const char *oldValue, const char *newValue)

{
	/*
	nsresult rv;
	nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item, &rv));

	if(NS_SUCCEEDED(rv))
	{
		if(PL_strcmp("DirName", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_DirName, oldValue, newValue);
		}
	}*/

	return NS_OK;
}

nsresult nsABDirectoryDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
													  nsIRDFResource *propertyResource,
													  const char *oldValue, const char *newValue)
{
	nsCOMPtr<nsIRDFNode> oldValueNode;
	nsCOMPtr<nsIRDFNode> newValueNode;
	nsString oldValueStr = oldValue;
	nsString newValueStr = newValue;
	createNode(oldValueStr, getter_AddRefs(oldValueNode));
	createNode(newValueStr, getter_AddRefs(newValueNode));
	NotifyObservers(resource, propertyResource, oldValueNode, PR_FALSE);
	NotifyObservers(resource, propertyResource, newValueNode, PR_TRUE);
	return NS_OK;
}

nsresult nsABDirectoryDataSource::createDirectoryNode(nsIAbDirectory* directory,
                                                 nsIRDFResource* property,
                                                 nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;
  
  if (peq(kNC_DirName, property))
	rv = createDirectoryNameNode(directory, target);
  else if (peq(kNC_Child, property))
    rv = createDirectoryChildNode(directory,target);
  else if (peq(kNC_CardChild, property))
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
	return rv;
}

nsresult nsABDirectoryDataSource::DoNewDirectory(nsIAbDirectory *directory, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(arguments->ElementAt(0), &rv));
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

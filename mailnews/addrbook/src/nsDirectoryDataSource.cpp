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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Seth Spitzer <sspitzer@netscape.com>
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

#include "nsDirectoryDataSource.h"
#include "nsAbBaseCID.h"
#include "nsIAbDirectory.h"
#include "nsIAddrBookSession.h"
#include "nsIAbCard.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"
#include "nsAdapterEnumerator.h"
#include "nsIObserverService.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsMsgRDFUtils.h"
#include "nsILocaleService.h"
#include "nsCollationCID.h"
#include "prmem.h"
                
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
                                
#define NC_RDF_DIRNAME			    "http://home.netscape.com/NC-rdf#DirName"
#define NC_RDF_CARDCHILD			"http://home.netscape.com/NC-rdf#CardChild"
#define NC_RDF_DIRURI				"http://home.netscape.com/NC-rdf#DirUri"
#define NC_RDF_ISMAILLIST			"http://home.netscape.com/NC-rdf#IsMailList"
#define NC_RDF_ISREMOTE				"http://home.netscape.com/NC-rdf#IsRemote"
#define NC_RDF_ISWRITEABLE			"http://home.netscape.com/NC-rdf#IsWriteable"
#define NC_RDF_DIRTREENAMESORT  "http://home.netscape.com/NC-rdf#DirTreeNameSort"

//Directory Commands
#define NC_RDF_MODIFY				"http://home.netscape.com/NC-rdf#Modify"
#define NC_RDF_DELETECARDS			"http://home.netscape.com/NC-rdf#DeleteCards"

////////////////////////////////////////////////////////////////////////

nsAbDirectoryDataSource::nsAbDirectoryDataSource()
{
}

nsAbDirectoryDataSource::~nsAbDirectoryDataSource()
{
}

nsresult nsAbDirectoryDataSource::Cleanup()
{
  nsresult rv;
  nsCOMPtr <nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = rdf->UnregisterDataSource(this);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = abSession->RemoveAddressBookListener(this);
  NS_ENSURE_SUCCESS(rv,rv);
  
  return NS_OK;
}

NS_IMETHODIMP
nsAbDirectoryDataSource::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  if (!strcmp(aTopic,"profile-do-change")) {
    /* the nsDirPrefs code caches all the directories that it got 
     * from the first profiles prefs.js
     * When we profile switch, we need to force it to shut down.
     * we'll re-load all the directories from the second profiles prefs.js 
     * that happens in nsAbBSDirectory::GetChildNodes()
     * when we call DIR_GetDirectories()
     */
    DIR_ShutDown();
    return NS_OK;
  }
  else if (!strcmp(aTopic,NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    return Cleanup();
  }
  return NS_OK;
}

nsresult
nsAbDirectoryDataSource::Init()
{
  nsresult rv;
  nsCOMPtr<nsIAddrBookSession> abSession = 
    do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv,rv);
  
  // this listener cares about all events
  rv = abSession->AddAddressBookListener(this, nsIAbListener::all);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = rdf->RegisterDataSource(this, PR_FALSE);
  NS_ENSURE_SUCCESS(rv,rv);
 
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CHILD),
                        getter_AddRefs(kNC_Child));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DIRNAME),
                        getter_AddRefs(kNC_DirName));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CARDCHILD),
                        getter_AddRefs(kNC_CardChild));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DIRURI),
                        getter_AddRefs(kNC_DirUri));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISMAILLIST),
                        getter_AddRefs(kNC_IsMailList));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISREMOTE),
                        getter_AddRefs(kNC_IsRemote));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISSECURE),
                        getter_AddRefs(kNC_IsSecure));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISWRITEABLE),
                        getter_AddRefs(kNC_IsWriteable));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DIRTREENAMESORT), getter_AddRefs(kNC_DirTreeNameSort));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_MODIFY), getter_AddRefs(kNC_Modify));  
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DELETE),
                        getter_AddRefs(kNC_Delete));  
  NS_ENSURE_SUCCESS(rv,rv);
  rv = rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DELETECARDS),
                        getter_AddRefs(kNC_DeleteCards));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = createNode(NS_LITERAL_STRING("true").get(), getter_AddRefs(kTrueLiteral));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = createNode(NS_LITERAL_STRING("false").get(), getter_AddRefs(kFalseLiteral));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // since the observer (this) supports weak ref, 
  // and we call AddObserver() with PR_TRUE for ownsWeak
  // we don't need to remove our observer from the from the observer service
  rv = observerService->AddObserver(this, "profile-do-change", PR_TRUE);
  NS_ENSURE_SUCCESS(rv,rv);
  rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv,rv);
  
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED3(nsAbDirectoryDataSource, nsAbRDFDataSource, nsIAbListener, nsIObserver, nsISupportsWeakReference)

 // nsIRDFDataSource methods
NS_IMETHODIMP nsAbDirectoryDataSource::GetURI(char* *uri)
{
  if ((*uri = nsCRT::strdup("rdf:addressdirectory")) == nsnull)
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
  if (NS_SUCCEEDED(rv) && directory)
    rv = createDirectoryNode(directory, property, target);
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
  NS_ENSURE_ARG_POINTER(targets);
  
  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv) && directory)
  {
    if ((kNC_Child == property))
    {
      return directory->GetChildNodes(targets);
    }
    else if((kNC_DirName == property) ||
      (kNC_DirUri == property) ||
      (kNC_IsMailList == property) ||
      (kNC_IsRemote == property) ||
      (kNC_IsSecure == property) ||
      (kNC_IsWriteable == property) ||
      (kNC_DirTreeNameSort == property)) 
    { 
      nsSingletonEnumerator* cursor =
        new nsSingletonEnumerator(property);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      return NS_OK;
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
        return NS_OK;
      }
    }
  }
  return NS_NewEmptyEnumerator(targets);
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

NS_IMETHODIMP 
nsAbDirectoryDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  nsresult rv;
  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(aSource, &rv));
  if (NS_SUCCEEDED(rv)) {
    *result = (aArc == kNC_DirName ||
               aArc == kNC_Child ||
               aArc == kNC_CardChild ||
               aArc == kNC_DirUri ||
               aArc == kNC_IsMailList ||
               aArc == kNC_IsRemote ||
               aArc == kNC_IsSecure ||
               aArc == kNC_IsWriteable ||
               aArc == kNC_DirTreeNameSort);
  }
  else {
    *result = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsAbDirectoryDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                 nsISimpleEnumerator** labels)
{
  nsCOMPtr<nsISupportsArray> arcs;
  nsresult rv = NS_RDF_NO_VALUE;

  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
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
  NS_ENSURE_SUCCESS(rv, rv);
  
  (*arcs)->AppendElement(kNC_DirName);
  (*arcs)->AppendElement(kNC_Child);
  (*arcs)->AppendElement(kNC_CardChild);
  (*arcs)->AppendElement(kNC_DirUri);
  (*arcs)->AppendElement(kNC_IsMailList);
  (*arcs)->AppendElement(kNC_IsRemote);
  (*arcs)->AppendElement(kNC_IsSecure);
  (*arcs)->AppendElement(kNC_IsWriteable);
  (*arcs)->AppendElement(kNC_DirTreeNameSort);
  return NS_OK;
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
		directory = do_QueryElementAt(aSources, i, &rv);
    if (NS_SUCCEEDED(rv)) {
      // we don't care about the arguments -- directory commands are always enabled
      if (!((aCommand == kNC_Delete) || (aCommand == kNC_DeleteCards) 
            ||(aCommand == kNC_Modify))) {
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
	NS_ENSURE_SUCCESS(rv, rv);

	if (aCommand == kNC_Modify) {
       rv = DoModifyDirectory(aSources,aArguments);
  }
  else
  {
	if ((aCommand == kNC_Delete))  
		rv = DoDeleteFromDirectory(aSources, aArguments);
  else {
    for (i = 0; i < cnt; i++) {
      nsCOMPtr<nsIAbDirectory> directory = do_QueryElementAt(aSources, i, &rv);
      if (NS_SUCCEEDED(rv)) {
        NS_ASSERTION(aCommand == kNC_DeleteCards, "unknown command");
        if ((aCommand == kNC_DeleteCards))  
          rv = DoDeleteCardsFromDirectory(directory, aArguments);
      }
    }
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

	if(NS_SUCCEEDED(parentDirectory->QueryInterface(NS_GET_IID(nsIRDFResource), getter_AddRefs(parentResource))))
	{ 
		//If we are adding a card
		if(NS_SUCCEEDED(item->QueryInterface(NS_GET_IID(nsIAbCard), getter_AddRefs(card))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if (NS_SUCCEEDED(rv))
			{
				//Notify directories that a message was added.
				NotifyObservers(parentResource, kNC_CardChild, itemNode, PR_TRUE, PR_FALSE);
			}
		}
		//If we are adding a directory
		else if(NS_SUCCEEDED(item->QueryInterface(NS_GET_IID(nsIAbDirectory), getter_AddRefs(directory))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify a directory was added.
				NotifyObservers(parentResource, kNC_Child, itemNode, PR_TRUE, PR_FALSE);
			}
		}
	}

	return NS_OK;
}

nsresult nsAbDirectoryDataSource::DoModifyDirectory(nsISupportsArray *parentDir, nsISupportsArray *arguments)
{
  PRUint32 itemCount;
  // Parent dir count should be 1.
  nsresult rv = parentDir->Count(&itemCount);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(itemCount == 1, "DoModifyDirectory() must have parent directory count = 1.");
  if (itemCount != 1)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAbDirectory> parent = do_QueryElementAt(parentDir, 0, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Argument count should be 2. 1st one is nsIAbDirectory and 2nd is nsIAbDirectoryProperties.
  nsCOMPtr<nsISupportsArray> resourceArray = do_QueryElementAt(arguments, 0, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = resourceArray->Count(&itemCount);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(itemCount == 2, "DoModifyDirectory() must have resource argument count = 2.");
  if (itemCount != 2)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAbDirectory> modifiedDir = do_QueryElementAt(resourceArray, 0, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIAbDirectoryProperties> properties = do_QueryElementAt(resourceArray, 1, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  if (modifiedDir && properties)
    rv = parent->ModifyDirectory(modifiedDir, properties);
  return rv;
}

NS_IMETHODIMP nsAbDirectoryDataSource::OnItemRemoved(nsISupports *parentDirectory, nsISupports *item)
{
	nsresult rv;
	nsCOMPtr<nsIAbCard> card;
	nsCOMPtr<nsIAbDirectory> directory;
	nsCOMPtr<nsIRDFResource> parentResource;

	if(NS_SUCCEEDED(parentDirectory->QueryInterface(NS_GET_IID(nsIRDFResource), getter_AddRefs(parentResource))))
	{
		//If we are removing a card
		if(NS_SUCCEEDED(item->QueryInterface(NS_GET_IID(nsIAbCard), getter_AddRefs(card))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify directories that a card was deleted.
				NotifyObservers(parentResource, kNC_CardChild, itemNode, PR_FALSE, PR_FALSE);
			}
		}
		//If we are removing a directory
		else if(NS_SUCCEEDED(item->QueryInterface(NS_GET_IID(nsIAbDirectory), getter_AddRefs(directory))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify a directory was deleted.
				NotifyObservers(parentResource, kNC_Child, itemNode, PR_FALSE, PR_FALSE);
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbDirectoryDataSource::OnItemPropertyChanged(nsISupports *item, const char *property,
														   const PRUnichar *oldValue, const PRUnichar *newValue)

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
  else if ((kNC_DirUri == property))
	  rv = createDirectoryUriNode(directory, target);
  else if ((kNC_Child == property))
	  rv = createDirectoryChildNode(directory, target);
  else if ((kNC_IsMailList == property))
	  rv = createDirectoryIsMailListNode(directory, target);
  else if ((kNC_IsRemote == property))
	  rv = createDirectoryIsRemoteNode(directory, target);
  else if ((kNC_IsSecure == property))
	  rv = createDirectoryIsSecureNode(directory, target);
  else if ((kNC_IsWriteable == property))
	  rv = createDirectoryIsWriteableNode(directory, target);
  else if ((kNC_DirTreeNameSort == property))
    rv = createDirectoryTreeNameSortNode(directory, target);
  return rv;
}


nsresult nsAbDirectoryDataSource::createDirectoryNameNode(nsIAbDirectory *directory,
                                                     nsIRDFNode **target)
{
	nsresult rv = NS_OK;

        nsXPIDLString name;
	rv = directory->GetDirName(getter_Copies(name));
	NS_ENSURE_SUCCESS(rv, rv);
	rv = createNode(name.get(), target);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

nsresult nsAbDirectoryDataSource::createDirectoryUriNode(nsIAbDirectory *directory,
                                                     nsIRDFNode **target)
{
  nsCOMPtr<nsIRDFResource> source(do_QueryInterface(directory));

  nsXPIDLCString uri;
  nsresult rv = source->GetValue(getter_Copies(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString nameString; nameString.AssignWithConversion(uri);
  rv = createNode(nameString.get(), target);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

nsresult
nsAbDirectoryDataSource::createDirectoryChildNode(nsIAbDirectory *directory,
                                             nsIRDFNode **target)
{
	nsCOMPtr<nsISupportsArray> pAddressLists;
	directory->GetAddressLists(getter_AddRefs(pAddressLists));

	if (pAddressLists)
	{
		PRUint32 total = 0;
		pAddressLists->Count(&total);

		if (total)
		{
			PRBool isMailList = PR_FALSE;
			directory->GetIsMailList(&isMailList);
			if (!isMailList)
      {
        // fetch the last element 
        nsCOMPtr<nsIRDFResource> mailList = do_QueryElementAt(pAddressLists, total - 1);
        NS_IF_ADDREF(*target = mailList);
      } 
    } // if total
  } // if pAddressLists
	
  return (*target ? NS_OK : NS_RDF_NO_VALUE);
}

nsresult
nsAbDirectoryDataSource::createDirectoryIsRemoteNode(nsIAbDirectory* directory,
                                                     nsIRDFNode **target)
{
  PRBool isRemote;
  nsresult rv = directory->GetIsRemote(&isRemote);
  NS_ENSURE_SUCCESS(rv, rv);
  
  NS_IF_ADDREF(*target = (isRemote ? kTrueLiteral : kFalseLiteral));
  return NS_OK;
}

nsresult
nsAbDirectoryDataSource::createDirectoryIsSecureNode(nsIAbDirectory* directory,
                                                     nsIRDFNode **target)
{
  PRBool IsSecure;
  nsresult rv = directory->GetIsSecure(&IsSecure);
  NS_ENSURE_SUCCESS(rv, rv);
  
  NS_IF_ADDREF(*target = (IsSecure ? kTrueLiteral : kFalseLiteral));
  return NS_OK;
}

nsresult
nsAbDirectoryDataSource::createDirectoryIsWriteableNode(nsIAbDirectory* directory,
                                                        nsIRDFNode **target)
{
  PRBool isWriteable;
  nsresult rv = directory->GetOperations(&isWriteable);
  NS_ENSURE_SUCCESS(rv, rv);
  
  NS_IF_ADDREF(*target = ((isWriteable & nsIAbDirectory::opWrite) ? kTrueLiteral : kFalseLiteral));
  return NS_OK;
}

nsresult
nsAbDirectoryDataSource::createDirectoryIsMailListNode(nsIAbDirectory* directory,
                                                       nsIRDFNode **target)
{
  nsresult rv;
  PRBool isMailList;
  rv = directory->GetIsMailList(&isMailList);
  NS_ENSURE_SUCCESS(rv, rv);
  
  NS_IF_ADDREF(*target = (isMailList ? kTrueLiteral : kFalseLiteral));
  return NS_OK;
}
 
nsresult
nsAbDirectoryDataSource::createDirectoryTreeNameSortNode(nsIAbDirectory* directory, nsIRDFNode **target)
{
  nsXPIDLString name;
  nsresult rv = directory->GetDirName(getter_Copies(name));
	NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(directory);
  const char *uri = nsnull;
  rv = resource->GetValueConst(&uri);
  NS_ENSURE_SUCCESS(rv,rv);

  // Get directory type.
  nsCOMPtr <nsIAbDirectoryProperties> properties;
  rv = directory->GetDirectoryProperties(getter_AddRefs(properties));
  NS_ENSURE_SUCCESS(rv,rv);
  PRUint32 dirType;
  rv = properties->GetDirType(&dirType);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 position;
  rv = properties->GetPosition(&position);
  // Get isMailList
  PRBool isMailList = PR_FALSE;
  directory->GetIsMailList(&isMailList);

  /* sort addressbooks in this order - Personal Addressbook, Collected Addresses, MDB, LDAP -
   * by prefixing address book names with numbers and using the xul sort service.
   *
   *  0Personal Address Book
   *  1Collected Address Book
   *  2MAB1
   *    5MAB1LIST1
   *    5MAB1LIST2
   *  2MAB2
   *    5MAB2LIST1
   *    5MAB2LIST2
   *  3LDAP1
   *  3LDAP2
   *  4MAPI1
   *  4MAPI2
   */

  nsAutoString sortString;
  // top level sort will be by position. Sort by type under that...
  sortString.Append((PRUnichar) (position + 'a'));
  if (isMailList)
    sortString.AppendInt(5);    // mailing lists
  else if (dirType == PABDirectory)
  {
    if (strcmp(uri, kPersonalAddressbookUri) == 0)
      sortString.AppendInt(0);  // Personal addrbook
    else if (strcmp(uri, kCollectedAddressbookUri) == 0)
      sortString.AppendInt(1);  // Collected addrbook
    else
      sortString.AppendInt(2);  // Normal addrbook 
  }
  else if (dirType == LDAPDirectory)
    sortString.AppendInt(3);    // LDAP addrbook
  else if (dirType == MAPIDirectory)
    sortString.AppendInt(4);    // MAPI addrbook
  else
    sortString.AppendInt(6);    // everything else comes last

  sortString += name;
  PRUint8 *sortKey=nsnull;
  PRUint32 sortKeyLength;
  rv = CreateCollationKey(sortString, &sortKey, &sortKeyLength);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr <nsIRDFService> rdfService = do_GetService (NS_RDF_CONTRACTID "/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  createBlobNode(sortKey, sortKeyLength, target, rdfService);
  NS_ENSURE_SUCCESS(rv, rv);
  PR_Free(sortKey);

  return NS_OK;

}

nsresult nsAbDirectoryDataSource::CreateCollationKey(const nsString &aSource,  PRUint8 **aKey, PRUint32 *aLength)
{
  NS_ENSURE_ARG_POINTER(aKey);
  NS_ENSURE_ARG_POINTER(aLength);

  nsresult rv;
  if (!mCollationKeyGenerator)
  {
    nsCOMPtr<nsILocaleService> localeSvc = do_GetService(NS_LOCALESERVICE_CONTRACTID,&rv); 
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocale> locale; 
    rv = localeSvc->GetApplicationLocale(getter_AddRefs(locale));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr <nsICollationFactory> factory = do_CreateInstance(kCollationFactoryCID, &rv); 
    NS_ENSURE_SUCCESS(rv, rv);

    rv = factory->CreateCollation(locale, getter_AddRefs(mCollationKeyGenerator));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return mCollationKeyGenerator->AllocateRawSortKey(nsICollation::kCollationCaseInSensitive, aSource, aKey, aLength);
}

nsresult nsAbDirectoryDataSource::DoDeleteFromDirectory(nsISupportsArray *parentDirs, nsISupportsArray *delDirs)
{
	PRUint32 item, itemCount;
	nsresult rv = parentDirs->Count(&itemCount);
	NS_ENSURE_SUCCESS(rv, rv);

	for (item = 0; item < itemCount; item++) 
	{
		nsCOMPtr<nsIAbDirectory> parent = do_QueryElementAt(parentDirs, item, &rv);
		if (NS_SUCCEEDED(rv)) 
		{
			nsCOMPtr<nsIAbDirectory> deletedDir(do_QueryElementAt(delDirs, item));
			if(deletedDir)
			{
				rv = parent->DeleteDirectory(deletedDir);
			}
		}
	}
	return rv;
}

nsresult nsAbDirectoryDataSource::DoDeleteCardsFromDirectory(nsIAbDirectory *directory, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	PRUint32 itemCount;
	rv = arguments->Count(&itemCount);
	NS_ENSURE_SUCCESS(rv, rv);
	
	nsCOMPtr<nsISupportsArray> cardArray;
	NS_NewISupportsArray(getter_AddRefs(cardArray));

	//Split up deleted items into different type arrays to be passed to the folder
	//for deletion.
	PRUint32 item;
	for(item = 0; item < itemCount; item++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(arguments->ElementAt(item));
		nsCOMPtr<nsIAbCard> deletedCard(do_QueryInterface(supports));
		if (deletedCard)
		{
			cardArray->AppendElement(supports);
		}
	}
	PRUint32 cnt;
	rv = cardArray->Count(&cnt);
	NS_ENSURE_SUCCESS(rv, rv);
	if (cnt > 0)
		rv = directory->DeleteCards(cardArray);
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
	else if ((kNC_IsMailList == property) || (kNC_IsRemote == property) ||
			(kNC_IsSecure == property) || (kNC_IsWriteable == property))
	{
		nsCOMPtr<nsIRDFResource> dirResource(do_QueryInterface(directory, &rv));
		NS_ENSURE_SUCCESS(rv, rv);
		rv = GetTargetHasAssertion(this, dirResource, property, tv, target, hasAssertion);
	}
	else 
		*hasAssertion = PR_FALSE;

	return rv;

}

nsresult nsAbDirectoryDataSource::GetTargetHasAssertion(nsIRDFDataSource *dataSource, nsIRDFResource* dirResource,
							   nsIRDFResource *property,PRBool tv, nsIRDFNode *target,PRBool* hasAssertion)
{
	nsresult rv;
	if(!hasAssertion)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIRDFNode> currentTarget;

	rv = dataSource->GetTarget(dirResource, property,tv, getter_AddRefs(currentTarget));
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFLiteral> value1(do_QueryInterface(target));
		nsCOMPtr<nsIRDFLiteral> value2(do_QueryInterface(currentTarget));
		if(value1 && value2)
			//If the two values are equal then it has this assertion
			*hasAssertion = (value1 == value2);
	}
	else
		rv = NS_NOINTERFACE;

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



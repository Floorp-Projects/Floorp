/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIAddressBook.h"
#include "nsAddressBook.h"
#include "nsAbBaseCID.h"
#include "nsDirPrefs.h"
#include "nsIAddrBookSession.h"
#include "nsAbRDFResource.h"

#include "prmem.h"
#include "prprf.h"	 

#include "nsCOMPtr.h"
#include "nsIDOMXULElement.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIDOMWindow.h"
#include "nsIContentViewer.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);


const char *kDirectoryDataSourceRoot = "abdirectory://";
const char *kCardDataSourceRoot = "abcard://";



static nsresult ConvertDOMListToResourceArray(nsIDOMNodeList *nodeList, nsISupportsArray **resourceArray)
{
	nsresult rv = NS_OK;
	PRUint32 listLength;
	nsIDOMNode *node;
	nsIDOMXULElement *xulElement;
	nsIRDFResource *resource;

	if(!resourceArray)
		return NS_ERROR_NULL_POINTER;

	if(NS_FAILED(rv = nodeList->GetLength(&listLength)))
		return rv;

	if(NS_FAILED(NS_NewISupportsArray(resourceArray)))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	for(PRUint32 i = 0; i < listLength; i++)
	{
		if(NS_FAILED(nodeList->Item(i, &node)))
			return rv;

		if(NS_SUCCEEDED(rv = node->QueryInterface(nsCOMTypeInfo<nsIDOMXULElement>::GetIID(), (void**)&xulElement)))
		{
			if(NS_SUCCEEDED(rv = xulElement->GetResource(&resource)))
			{
				(*resourceArray)->AppendElement(resource);
				NS_RELEASE(resource);
			}
			NS_RELEASE(xulElement);
		}
		NS_RELEASE(node);
		
	}

	return rv;
}

//
// nsAddressBook
//
nsAddressBook::nsAddressBook()
{
	NS_INIT_REFCNT();
}

nsAddressBook::~nsAddressBook()
{
}

NS_IMPL_ISUPPORTS(nsAddressBook, nsIAddressBook::GetIID());

//
// nsIAddressBook
//

NS_IMETHODIMP nsAddressBook::DeleteCards
(nsIDOMXULElement *tree, nsIDOMXULElement *srcDirectory, nsIDOMNodeList *nodeList)
{
	nsresult rv;

	if(!tree || !srcDirectory || !nodeList)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIRDFCompositeDataSource> database;
	nsCOMPtr<nsISupportsArray> resourceArray, dirArray;
	nsCOMPtr<nsIRDFResource> resource;

	rv = srcDirectory->GetResource(getter_AddRefs(resource));

	if(NS_FAILED(rv))
		return rv;

	rv = tree->GetDatabase(getter_AddRefs(database));
	if(NS_FAILED(rv))
		return rv;

	rv = ConvertDOMListToResourceArray(nodeList, getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return rv;

	rv = NS_NewISupportsArray(getter_AddRefs(dirArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	dirArray->AppendElement(resource);
	
	rv = DoCommand(database, NC_RDF_DELETE, dirArray, resourceArray);

	return rv;
}

NS_IMETHODIMP nsAddressBook::NewAddressBook
(nsIRDFCompositeDataSource* db, nsIDOMXULElement *srcDirectory, const char *name)
{
	if(!db || !srcDirectory || !name)
		return NS_ERROR_NULL_POINTER;

//	DIR_Server * server = nsnull;
//	nsresult rv = DIR_AddNewAddressBook(name, &server);
	nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsISupportsArray> nameArray, dirArray;

	rv = NS_NewISupportsArray(getter_AddRefs(dirArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;
	nsCOMPtr<nsIRDFResource> parentResource;
	char *parentUri = PR_smprintf("%s", kDirectoryDataSourceRoot);
	rv = rdfService->GetResource(parentUri, getter_AddRefs(parentResource));
	nsCOMPtr<nsIAbDirectory> parentDir = do_QueryInterface(parentResource);
	if (!parentDir)
		return NS_ERROR_NULL_POINTER;
	if (parentUri)
		PR_smprintf_free(parentUri);

	dirArray->AppendElement(parentResource);

	rv = NS_NewISupportsArray(getter_AddRefs(nameArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;
	/*
	nsCOMPtr<nsIRDFResource> itemResource;
	char *uri = PR_smprintf("%s%s", kDirectoryDataSourceRoot, server->fileName);
	rv = rdfService->GetResource(uri, getter_AddRefs(itemResource));
	if (uri)
		PR_smprintf_free(uri);
	nsCOMPtr<nsIAbDirectory> newDir = do_QueryInterface(itemResource);
	if (newDir)
	{
		newDir->SetDirName(server->description);
		newDir->SetServer(server);
	}
	else
		return NS_ERROR_NULL_POINTER;
*/
	nsString nameStr = name;
	nsCOMPtr<nsIRDFLiteral> nameLiteral;

	rdfService->GetLiteral(nameStr.GetUnicode(), getter_AddRefs(nameLiteral));
	nameArray->AppendElement(nameLiteral);
//	resourceArray->AppendElement(itemResource);


	DoCommand(db, "http://home.netscape.com/NC-rdf#NewDirectory", dirArray, nameArray);
	return rv;
}

NS_IMETHODIMP nsAddressBook::DeleteAddressBook
(nsIDOMXULElement *tree, nsIDOMXULElement *srcDirectory, nsIDOMNodeList *nodeList)
{
	nsresult rv;

	if(!tree || !srcDirectory || !nodeList)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIRDFCompositeDataSource> database;
	nsCOMPtr<nsISupportsArray> resourceArray, dirArray;
	nsCOMPtr<nsIRDFResource> resource;

	rv = srcDirectory->GetResource(getter_AddRefs(resource));

	if(NS_FAILED(rv))
		return rv;

	rv = tree->GetDatabase(getter_AddRefs(database));
	if(NS_FAILED(rv))
		return rv;

	rv = ConvertDOMListToResourceArray(nodeList, getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return rv;

	rv = NS_NewISupportsArray(getter_AddRefs(dirArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	dirArray->AppendElement(resource);
	
	rv = DoCommand(database, NC_RDF_DELETE, dirArray, resourceArray);

	return rv;
}

nsresult nsAddressBook::DoCommand(nsIRDFCompositeDataSource* db, char *command,
						nsISupportsArray *srcArray, nsISupportsArray *argumentArray)
{

	nsresult rv;

    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> commandResource;
	rv = rdfService->GetResource(command, getter_AddRefs(commandResource));
	if(NS_SUCCEEDED(rv))
	{
		rv = db->DoCommand(srcArray, commandResource, argumentArray);
	}

	return rv;

}

nsresult nsAddressBook::PrintCard()
{
#ifdef DEBUG_seth
  printf("nsAddressBook::PrintCard()\n");
#endif

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContentViewer> viewer;

  if (!mWebShell) {
#ifdef DEBUG_seth
        printf("can't print, there is no webshell\n");
#endif
        return rv;
  }

  mWebShell->GetContentViewer(getter_AddRefs(viewer));

  if (viewer) {
    rv = viewer->Print();
  }
#ifdef DEBUG_seth
  else {
        printf("failed to get the viewer for printing\n");
  }
#endif

  return rv;  
}

nsresult nsAddressBook::PrintAddressbook()
{
#ifdef DEBUG_seth
	printf("nsAddressBook::PrintAddressbook()\n");
#endif
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsAddressBook::SetWebShellWindow(nsIDOMWindow *aWin)
{
   NS_PRECONDITION(aWin != nsnull, "null ptr");
   if (!aWin)
       return NS_ERROR_NULL_POINTER;
 
   nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
   if (!globalObj) {
     return NS_ERROR_FAILURE;
   }
 
   nsCOMPtr<nsIWebShell> webShell;
   globalObj->GetWebShell(getter_AddRefs(webShell));
   if (!webShell)
     return NS_ERROR_NOT_INITIALIZED;
 
   mWebShell = webShell;

   return NS_OK;
}

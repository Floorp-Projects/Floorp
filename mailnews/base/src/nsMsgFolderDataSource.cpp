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

#include "nsMsgFolderDataSource.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgFolder.h"
#include "nsMsgRDFUtils.h"
#include "nsIMessage.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgCopyService.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgMailSessionCID,		NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID,		NS_MSGCOPYSERVICE_CID);

nsIRDFResource* nsMsgFolderDataSource::kNC_Child = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_MessageChild = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Folder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Name= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_SpecialFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalUnreadMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Charset = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_BiffState = nsnull;

// commands
nsIRDFResource* nsMsgFolderDataSource::kNC_Delete= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NewFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_GetNewMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Copy= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Move= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_MarkAllMessagesRead= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Compact= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Rename= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_EmptyTrash= nsnull;


nsMsgFolderDataSource::nsMsgFolderDataSource():
  mInitialized(PR_FALSE),
  mRDFService(nsnull)
{

}

nsMsgFolderDataSource::~nsMsgFolderDataSource (void)
{
	nsresult rv;
  mRDFService->UnregisterDataSource(this);

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->RemoveFolderListener(this);

  nsrefcnt refcnt;
  NS_RELEASE2(kNC_Child, refcnt);
  NS_RELEASE2(kNC_MessageChild, refcnt);
  NS_RELEASE2(kNC_Folder, refcnt);
  NS_RELEASE2(kNC_Name, refcnt);
  NS_RELEASE2(kNC_SpecialFolder, refcnt);
  NS_RELEASE2(kNC_TotalMessages, refcnt);
  NS_RELEASE2(kNC_TotalUnreadMessages, refcnt);
  NS_RELEASE2(kNC_Charset, refcnt);
  NS_RELEASE2(kNC_BiffState, refcnt);

  NS_RELEASE2(kNC_Delete, refcnt);
  NS_RELEASE2(kNC_NewFolder, refcnt);
  NS_RELEASE2(kNC_GetNewMessages, refcnt);
  NS_RELEASE2(kNC_Copy, refcnt);
  NS_RELEASE2(kNC_Move, refcnt);
  NS_RELEASE2(kNC_MarkAllMessagesRead, refcnt);
  NS_RELEASE2(kNC_Compact, refcnt);
  NS_RELEASE2(kNC_Rename, refcnt);
  NS_RELEASE2(kNC_EmptyTrash, refcnt);

  nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); // XXX probably need shutdown listener here
  mRDFService = nsnull;
}

nsresult nsMsgFolderDataSource::Init()
{
  if (mInitialized)
      return NS_ERROR_ALREADY_INITIALIZED;

  nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                             nsCOMTypeInfo<nsIRDFService>::GetIID(),
                                             (nsISupports**) &mRDFService); // XXX probably need shutdown listener here

  PR_ASSERT(NS_SUCCEEDED(rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->AddFolderListener(this);

  mRDFService->RegisterDataSource(this, PR_FALSE);

  if (! kNC_Child) {
    mRDFService->GetResource(NC_RDF_CHILD,   &kNC_Child);
    mRDFService->GetResource(NC_RDF_MESSAGECHILD,   &kNC_MessageChild);
    mRDFService->GetResource(NC_RDF_FOLDER,  &kNC_Folder);
    mRDFService->GetResource(NC_RDF_NAME,    &kNC_Name);
    mRDFService->GetResource(NC_RDF_SPECIALFOLDER, &kNC_SpecialFolder);
    mRDFService->GetResource(NC_RDF_TOTALMESSAGES, &kNC_TotalMessages);
    mRDFService->GetResource(NC_RDF_TOTALUNREADMESSAGES, &kNC_TotalUnreadMessages);
    mRDFService->GetResource(NC_RDF_CHARSET, &kNC_Charset);
    mRDFService->GetResource(NC_RDF_BIFFSTATE, &kNC_BiffState);
    
	mRDFService->GetResource(NC_RDF_DELETE, &kNC_Delete);
    mRDFService->GetResource(NC_RDF_NEWFOLDER, &kNC_NewFolder);
    mRDFService->GetResource(NC_RDF_GETNEWMESSAGES, &kNC_GetNewMessages);
    mRDFService->GetResource(NC_RDF_COPY, &kNC_Copy);
    mRDFService->GetResource(NC_RDF_MOVE, &kNC_Move);
    mRDFService->GetResource(NC_RDF_MARKALLMESSAGESREAD,
                             &kNC_MarkAllMessagesRead);
    mRDFService->GetResource(NC_RDF_COMPACT, &kNC_Compact);
    mRDFService->GetResource(NC_RDF_RENAME, &kNC_Rename);
    mRDFService->GetResource(NC_RDF_EMPTYTRASH, &kNC_EmptyTrash);
  }
  mInitialized = PR_TRUE;
  return NS_OK;
}



NS_IMPL_ADDREF_INHERITED(nsMsgFolderDataSource, nsMsgRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsMsgFolderDataSource, nsMsgRDFDataSource)

NS_IMETHODIMP
nsMsgFolderDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if(iid.Equals(nsCOMTypeInfo<nsIFolderListener>::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIFolderListener*, this);
		NS_ADDREF(this);
		return NS_OK;
	}
	else
		return nsMsgRDFDataSource::QueryInterface(iid, result);
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsMsgFolderDataSource::GetURI(char* *uri)
{
  if ((*uri = nsXPIDLCString::Copy("rdf:mailnewsfolders")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               PRBool tv,
                                               nsIRDFResource** source /* out */)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetTarget(nsIRDFResource* source,
                                               nsIRDFResource* property,
                                               PRBool tv,
                                               nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  // we only have positive assertions in the mail data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source));
  if (folder) {
    rv = createFolderNode(folder, property, target);
#if 0
    nsXPIDLCString srcval;
    nsXPIDLCString propval;
    nsXPIDLCString targetval;
    source->GetValue(getter_Copies(srcval));
    property->GetValue(getter_Copies(propval));
    //    (*target)->GetValue(getter_Copies(targetval));

    printf("nsMsgFolderDataSource::GetTarget(%s, %s, %s, (%s))\n",
           (const char*)srcval,
           (const char*)propval, tv ? "TRUE" : "FALSE",
           (const char*)"");
#endif
    
  }
  else
	  return NS_RDF_NO_VALUE;
  return rv;
}


NS_IMETHODIMP nsMsgFolderDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                PRBool tv,
                                                nsISimpleEnumerator** sources)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,    
                                                PRBool tv,
                                                nsISimpleEnumerator** targets)
{
  nsresult rv = NS_RDF_NO_VALUE;
  if(!targets)
	  return NS_ERROR_NULL_POINTER;

#if 0
  nsXPIDLCString srcval;
  nsXPIDLCString propval;
  nsXPIDLCString targetval;
  source->GetValue(getter_Copies(srcval));
  property->GetValue(getter_Copies(propval));
  //    (*target)->GetValue(getter_Copies(targetval));
  
  printf("nsMsgFolderDataSource::GetTargets(%s, %s, %s, (%s))\n",
         (const char*)srcval,
         (const char*)propval, tv ? "TRUE" : "FALSE",
         (const char*)"");
#endif
  *targets = nsnull;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv))
  {
    if ((kNC_Child == property))
    {
      nsCOMPtr<nsIEnumerator> subFolders;

      rv = folder->GetSubFolders(getter_AddRefs(subFolders));
      if (NS_SUCCEEDED(rv))
	  {
		  nsAdapterEnumerator* cursor =
			new nsAdapterEnumerator(subFolders);
		  if (cursor == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		  NS_ADDREF(cursor);
		  *targets = cursor;
		  rv = NS_OK;
	  }
    }
    else if ((kNC_MessageChild == property))
    {
      nsCOMPtr<nsIEnumerator> messages;

      rv = folder->GetMessages(getter_AddRefs(messages));
      if (NS_SUCCEEDED(rv))
	  {
		  nsAdapterEnumerator* cursor =
			new nsAdapterEnumerator(messages);
		  if (cursor == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		  NS_ADDREF(cursor);
		  *targets = cursor;
		  rv = NS_OK;
	  }
    }
    else if((kNC_Name == property) || (kNC_SpecialFolder == property))
    {
      nsSingletonEnumerator* cursor =
        new nsSingletonEnumerator(property);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
  }
  if(!*targets)
  {
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

NS_IMETHODIMP nsMsgFolderDataSource::Assert(nsIRDFResource* source,
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv)
{
	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
	//We don't handle tv = PR_FALSE at the moment.
	if(NS_SUCCEEDED(rv) && tv)
		return DoFolderAssert(folder, property, target);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgFolderDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMsgFolderDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
	if(NS_SUCCEEDED(rv))
		return DoFolderHasAssertion(folder, property, target, tv, hasAssertion);
	else
		*hasAssertion = PR_FALSE;
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolderDataSource::ArcLabelsIn(nsIRDFNode* node,
                                                 nsISimpleEnumerator** labels)
{
	return nsMsgRDFDataSource::ArcLabelsIn(node, labels);
}

NS_IMETHODIMP nsMsgFolderDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                  nsISimpleEnumerator** labels)
{
  nsCOMPtr<nsISupportsArray> arcs;
  nsresult rv = NS_RDF_NO_VALUE;
  
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    fflush(stdout);
    rv = getFolderArcLabelsOut(folder, getter_AddRefs(arcs));
#if 0
    nsXPIDLCString srcval;
    source->GetValue(getter_Copies(srcval));
    printf("nsMsgFolderDataSource::ArcLabelsOut(%s)\n", (const char*)srcval);
#endif
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
nsMsgFolderDataSource::getFolderArcLabelsOut(nsIMsgFolder *folder,
                                             nsISupportsArray **arcs)
{
	nsresult rv;
  rv = NS_NewISupportsArray(arcs);
	if(NS_FAILED(rv))
		return rv;
  
  (*arcs)->AppendElement(kNC_Name);
  (*arcs)->AppendElement(kNC_SpecialFolder);
  (*arcs)->AppendElement(kNC_TotalMessages);
  (*arcs)->AppendElement(kNC_TotalUnreadMessages);
  (*arcs)->AppendElement(kNC_Charset);
  (*arcs)->AppendElement(kNC_BiffState);
  (*arcs)->AppendElement(kNC_Child);
  (*arcs)->AppendElement(kNC_MessageChild);
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllCommands(nsIRDFResource* source,
                                      nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  nsresult rv;

  nsCOMPtr<nsISupportsArray> cmds;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewISupportsArray(getter_AddRefs(cmds));
    if (NS_FAILED(rv)) return rv;
    cmds->AppendElement(kNC_Delete);
    cmds->AppendElement(kNC_NewFolder);
    cmds->AppendElement(kNC_GetNewMessages);
    cmds->AppendElement(kNC_Copy);
    cmds->AppendElement(kNC_Move);
    cmds->AppendElement(kNC_MarkAllMessagesRead);
    cmds->AppendElement(kNC_Compact);
    cmds->AppendElement(kNC_Rename);
    cmds->AppendElement(kNC_EmptyTrash);
  }

  if (cmds != nsnull)
    return cmds->Enumerate(commands);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllCmds(nsIRDFResource* source,
                                      nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolderDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                        PRBool* aResult)
{
	nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder;

  PRUint32 cnt;
  rv = aSources->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> source = getter_AddRefs(aSources->ElementAt(i));
		folder = do_QueryInterface(source, &rv);
    if (NS_SUCCEEDED(rv)) {
      // we don't care about the arguments -- folder commands are always enabled
      if (!((aCommand == kNC_Delete) ||
            (aCommand == kNC_NewFolder) ||
            (aCommand == kNC_Copy) ||
            (aCommand == kNC_Move) ||
            (aCommand == kNC_GetNewMessages) ||
            (aCommand == kNC_MarkAllMessagesRead) ||
            (aCommand == kNC_Compact) || 
            (aCommand == kNC_Rename) ||
            (aCommand == kNC_EmptyTrash) )) 
      {
        *aResult = PR_FALSE;
        return NS_OK;
      }
    }
  }
  *aResult = PR_TRUE;
  return NS_OK; // succeeded for all sources
}

NS_IMETHODIMP
nsMsgFolderDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsITransactionManager> transactionManager;
  nsCOMPtr<nsISupports> supports;
  // XXX need to handle batching of command applied to all sources

  PRUint32 cnt = 0;
  PRUint32 i = 0;

  rv = GetTransactionManager(aSources, getter_AddRefs(transactionManager));
  if (NS_FAILED(rv)) return rv;
  rv = aSources->Count(&cnt);
  if (NS_FAILED(rv)) return rv;

  for ( ; i < cnt; i++) {
    supports  = getter_AddRefs(aSources->ElementAt(i));
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
    if (NS_SUCCEEDED(rv)) 
    {
      if ((aCommand == kNC_Delete))
      {
        rv = DoDeleteFromFolder(folder, aArguments, transactionManager);
      }
      else if((aCommand == kNC_NewFolder)) 
      {
        rv = DoNewFolder(folder, aArguments);
      }
      else if((aCommand == kNC_GetNewMessages))
      {
        rv = folder->GetNewMessages();
      }
      else if((aCommand == kNC_Copy))
      {
        rv = DoCopyToFolder(folder, aArguments, transactionManager, PR_FALSE);
      }
      else if((aCommand == kNC_Move))
      {
        rv = DoCopyToFolder(folder, aArguments, transactionManager, PR_TRUE);
      }
      else if((aCommand == kNC_MarkAllMessagesRead))
      {
        rv = folder->MarkAllMessagesRead();
      }
      else if ((aCommand == kNC_Compact))
      {
        rv = folder->Compact();
      }
      else if ((aCommand == kNC_EmptyTrash))
      {
          rv = folder->EmptyTrash();
      }
    }
  }
  //for the moment return NS_OK, because failure stops entire DoCommand process.
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::OnItemAdded(nsIFolder *parentFolder, nsISupports *item)
{
	nsresult rv;
	nsCOMPtr<nsIMessage> message;
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIRDFResource> parentResource;

	if(NS_SUCCEEDED(parentFolder->QueryInterface(nsCOMTypeInfo<nsIRDFResource>::GetIID(), getter_AddRefs(parentResource))))
	{
		//If we are adding a message
		if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIMessage>::GetIID(), getter_AddRefs(message))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify folders that a message was added.
				NotifyObservers(parentResource, kNC_MessageChild, itemNode, PR_TRUE);
			}
		}
		//If we are adding a folder
		else if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIMsgFolder>::GetIID(), getter_AddRefs(folder))))
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

NS_IMETHODIMP nsMsgFolderDataSource::OnItemRemoved(nsIFolder *parentFolder, nsISupports *item)
{
	nsresult rv;
	nsCOMPtr<nsIMessage> message;
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIRDFResource> parentResource;

	if(NS_SUCCEEDED(parentFolder->QueryInterface(nsCOMTypeInfo<nsIRDFResource>::GetIID(), getter_AddRefs(parentResource))))
	{
		//If we are removing a message
		if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIMessage>::GetIID(), getter_AddRefs(message))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify folders that a message was deleted.
				NotifyObservers(parentResource, kNC_MessageChild, itemNode, PR_FALSE);
			}
		}
		//If we are removing a folder
		else if(NS_SUCCEEDED(item->QueryInterface(nsCOMTypeInfo<nsIMsgFolder>::GetIID(), getter_AddRefs(folder))))
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

NS_IMETHODIMP nsMsgFolderDataSource::OnItemPropertyChanged(nsISupports *item, const char *property,
														   const char *oldValue, const char *newValue)

{
	nsresult rv;
	nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item, &rv));

	if(NS_SUCCEEDED(rv))
	{
		if(PL_strcmp("TotalMessages", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_TotalMessages, oldValue, newValue);
		}
		else if(PL_strcmp("TotalUnreadMessages", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_TotalUnreadMessages, oldValue, newValue);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::OnItemPropertyFlagChanged(nsISupports *item, const char *property,
									   PRUint32 oldFlag, PRUint32 newFlag)
{
	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(item));
	if(folder)
	{
		nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item));
		if(resource)
		{
			if(PL_strcmp("BiffState", property) == 0)
			{
				nsCAutoString oldBiffStateStr, newBiffStateStr;

				rv = GetBiffStateString(oldFlag, oldBiffStateStr);
				if(NS_FAILED(rv))
					return rv;
				rv = GetBiffStateString(newFlag, newBiffStateStr);
				if(NS_FAILED(rv))
					return rv;
				NotifyPropertyChanged(resource, kNC_BiffState, oldBiffStateStr, newBiffStateStr);
			}
		}

	}
	return NS_OK;

}

nsresult nsMsgFolderDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
													  nsIRDFResource *propertyResource,
													  const char *oldValue, const char *newValue)
{
	nsCOMPtr<nsIRDFNode> oldValueNode, newValueNode;
	nsString oldValueStr = oldValue;
	nsString newValueStr = newValue;
	createNode(oldValueStr,getter_AddRefs(oldValueNode));
	createNode(newValueStr, getter_AddRefs(newValueNode));
	NotifyObservers(resource, propertyResource, oldValueNode, PR_FALSE);
	NotifyObservers(resource, propertyResource, newValueNode, PR_TRUE);
	return NS_OK;
}

nsresult nsMsgFolderDataSource::createFolderNode(nsIMsgFolder* folder,
                                                 nsIRDFResource* property,
                                                 nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;
  
  if ((kNC_Name == property))
		rv = createFolderNameNode(folder, target);
  else if ((kNC_SpecialFolder == property))
		rv = createFolderSpecialNode(folder,target);
	else if ((kNC_TotalMessages == property))
		rv = createTotalMessagesNode(folder, target);
	else if ((kNC_TotalUnreadMessages == property))
		rv = createUnreadMessagesNode(folder, target);
	else if ((kNC_Charset == property))
		rv = createCharsetNode(folder, target);
	else if ((kNC_BiffState == property))
		rv = createBiffStateNode(folder, target);
	else if ((kNC_Child == property))
		rv = createFolderChildNode(folder, target);
	else if ((kNC_MessageChild == property))
		rv = createFolderMessageNode(folder, target);
 
  return rv;
}


nsresult nsMsgFolderDataSource::createFolderNameNode(nsIMsgFolder *folder,
                                                     nsIRDFNode **target)
{
  PRUnichar *name;
  nsresult rv = folder->GetName(&name);
  if (NS_FAILED(rv)) return rv;
  nsString nameString(name);
  createNode(nameString, target);
  delete[] name;
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderSpecialNode(nsIMsgFolder *folder,
                                               nsIRDFNode **target)
{
  PRUint32 flags;
  nsresult rv = folder->GetFlags(&flags);
  if(NS_FAILED(rv)) return rv;
  
  nsString specialFolderString;
  
  if(flags & MSG_FOLDER_FLAG_INBOX)
    specialFolderString = "Inbox";
  else if(flags & MSG_FOLDER_FLAG_TRASH)
    specialFolderString = "Trash";
  else if(flags & MSG_FOLDER_FLAG_QUEUE)
    specialFolderString = "Unsent Messages";
  else if(flags & MSG_FOLDER_FLAG_SENTMAIL)
    specialFolderString = "Sent";
  else if(flags & MSG_FOLDER_FLAG_DRAFTS)
    specialFolderString = "Drafts";
  else if(flags & MSG_FOLDER_FLAG_TEMPLATES)
    specialFolderString = "Templates";
  else
    specialFolderString = "none";
  
  createNode(specialFolderString, target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createTotalMessagesNode(nsIMsgFolder *folder,
											   nsIRDFNode **target)
{
	nsresult rv;
	PRInt32 totalMessages;
	rv = folder->GetTotalMessages(PR_FALSE, &totalMessages);
	if(NS_SUCCEEDED(rv))
	{
		if(totalMessages >= 0)
			rv = createNode(totalMessages, target);
		else if(totalMessages == -1)
		{
			nsString unknownMessages("???");
			createNode(unknownMessages, target);
		}
		else if(totalMessages == -2)
		{
			nsString unknownMessages("");
			createNode(unknownMessages, target);
		}
	}
	return rv;
}

nsresult
nsMsgFolderDataSource::createCharsetNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
	PRUnichar *charset;
	nsString charsetStr;
	nsresult rv = folder->GetCharset(&charset);
	//We always need to return a value
	if(NS_SUCCEEDED(rv))
		charsetStr = charset;
	else
		charsetStr ="";
	createNode(charsetStr, target);
	return NS_OK;

}

nsresult
nsMsgFolderDataSource::createBiffStateNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 biffState;
	rv = folder->GetBiffState(&biffState);
	if(NS_FAILED(rv)) return rv;

	nsCAutoString biffString;
	GetBiffStateString(biffState, biffString);
	nsString uniStr = biffString;
	createNode(uniStr, target);
	return NS_OK;
}

nsresult
nsMsgFolderDataSource::GetBiffStateString(PRUint32 biffState, nsCAutoString& biffStateStr)
{
	if(biffState == nsMsgBiffState_NewMail)
		biffStateStr = "NewMail";
	else if(biffState == nsMsgBiffState_NoMail)
		biffStateStr = "NoMail";
	else 
		biffStateStr = "UnknownMail";

	return NS_OK;
}

nsresult 
nsMsgFolderDataSource::createUnreadMessagesNode(nsIMsgFolder *folder,
												nsIRDFNode **target)
{
	nsresult rv;
	PRInt32 totalUnreadMessages;
	rv = folder->GetNumUnread(PR_FALSE, &totalUnreadMessages);
	if(NS_SUCCEEDED(rv))
	{
		if(totalUnreadMessages >=0)
			rv = createNode(totalUnreadMessages, target);
		else if(totalUnreadMessages == -1)
		{
			nsString unknownMessages("???");
			createNode(unknownMessages, target);
		}
		else if(totalUnreadMessages == -2)
		{
			nsString unknownMessages("");
			createNode(unknownMessages, target);
		}
	}

	return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderChildNode(nsIMsgFolder *folder,
                                             nsIRDFNode **target)
{
  nsCOMPtr<nsIEnumerator> subFolders;
  nsresult rv = folder->GetSubFolders(getter_AddRefs(subFolders));
  if (NS_FAILED(rv))
    return NS_RDF_NO_VALUE;
  
  rv = subFolders->First();
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISupports> firstFolder;
    rv = subFolders->CurrentItem(getter_AddRefs(firstFolder));
    if (NS_SUCCEEDED(rv)) {
      firstFolder->QueryInterface(nsCOMTypeInfo<nsIRDFResource>::GetIID(), (void**)target);
    }
  }
  return NS_FAILED(rv) ? NS_RDF_NO_VALUE : rv;
}


nsresult
nsMsgFolderDataSource::createFolderMessageNode(nsIMsgFolder *folder,
                                               nsIRDFNode **target)
{
  nsCOMPtr<nsIEnumerator> messages;
  nsresult rv = folder->GetMessages(getter_AddRefs(messages));
  if (NS_SUCCEEDED(rv) && rv != NS_RDF_CURSOR_EMPTY) {
	rv = messages->First();
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsISupports> firstMessage;
      rv = messages->CurrentItem(getter_AddRefs(firstMessage));
      if (NS_SUCCEEDED(rv)) {
		rv = firstMessage->QueryInterface(nsCOMTypeInfo<nsIRDFNode>::GetIID(), (void**)target);
      }
    }
  }
  return rv == NS_OK ? NS_OK : NS_RDF_NO_VALUE;
}

nsresult nsMsgFolderDataSource::DoCopyToFolder(nsIMsgFolder *dstFolder, nsISupportsArray *arguments,
											   nsITransactionManager *txnMgr, PRBool isMove)
{
	nsresult rv;
	PRUint32 itemCount;
	rv = arguments->Count(&itemCount);
	if (NS_FAILED(rv)) return rv;
	
	//need source folder and at least one item to copy
	if(itemCount < 2)
		return NS_ERROR_FAILURE;


	nsCOMPtr<nsISupports> srcFolderSupports = getter_AddRefs(arguments->ElementAt(0));
	nsCOMPtr<nsIMsgFolder> srcFolder(do_QueryInterface(srcFolderSupports));
	if(!srcFolder)
		return NS_ERROR_FAILURE;

    arguments->RemoveElementAt(0);
    itemCount--;

	nsCOMPtr<nsISupportsArray> messageArray;
	NS_NewISupportsArray(getter_AddRefs(messageArray));

	for(PRUint32 i = 0; i < itemCount; i++)
	{

		nsCOMPtr<nsISupports> supports = getter_AddRefs(arguments->ElementAt(i));
		nsCOMPtr<nsIMessage> message(do_QueryInterface(supports));
		if (message)
		{
			messageArray->AppendElement(supports);
		}

	}
	//Call copyservice with dstFolder, srcFolder, messages, isMove, and txnManager
	NS_WITH_SERVICE(nsIMsgCopyService, copyService, kMsgCopyServiceCID, &rv); 
	if(NS_SUCCEEDED(rv))
	{
		copyService->CopyMessages(srcFolder, messageArray, dstFolder, isMove, 
                              nsnull, txnMgr);

	}
	return NS_OK;
}

nsresult nsMsgFolderDataSource::DoDeleteFromFolder(
    nsIMsgFolder *folder, nsISupportsArray *arguments, 
    nsITransactionManager* txnMgr)
{
	nsresult rv = NS_OK;
	PRUint32 itemCount;
  rv = arguments->Count(&itemCount);
  if (NS_FAILED(rv)) return rv;
	
	nsCOMPtr<nsISupportsArray> messageArray, folderArray;
	NS_NewISupportsArray(getter_AddRefs(messageArray));
	NS_NewISupportsArray(getter_AddRefs(folderArray));

	//Split up deleted items into different type arrays to be passed to the folder
	//for deletion.
	for(PRUint32 item = 0; item < itemCount; item++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(arguments->ElementAt(item));
		nsCOMPtr<nsIMessage> deletedMessage(do_QueryInterface(supports));
		nsCOMPtr<nsIMsgFolder> deletedFolder(do_QueryInterface(supports));
		if (deletedMessage)
		{
			messageArray->AppendElement(supports);
		}
		else if(deletedFolder)
		{
			folderArray->AppendElement(supports);
		}
	}
	PRUint32 cnt;
	rv = messageArray->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	if (cnt > 0)
		rv = folder->DeleteMessages(messageArray, txnMgr, PR_FALSE);

	rv = folderArray->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	if (cnt > 0)
		rv = folder->DeleteSubFolders(folderArray);

	return rv;
}

nsresult nsMsgFolderDataSource::DoNewFolder(nsIMsgFolder *folder, nsISupportsArray *arguments)
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

		rv = folder->CreateSubfolder(nameStr);
	}
	return rv;
}

nsresult nsMsgFolderDataSource::DoFolderAssert(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target)
{
	nsresult rv = NS_ERROR_FAILURE;

	if((kNC_Charset == property))
	{
		nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(target));
		if(literal)
		{
			PRUnichar *value;
			rv = literal->GetValue(&value);
			if(NS_SUCCEEDED(rv))
			{
				rv = folder->SetCharset(value);
				delete[] value;
			}
		}
		else
			rv = NS_ERROR_FAILURE;
	}

	return rv;

}


nsresult nsMsgFolderDataSource::DoFolderHasAssertion(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion)
{
	nsresult rv = NS_OK;
	if(!hasAssertion)
		return NS_ERROR_NULL_POINTER;

	//We're not keeping track of negative assertions on folders.
	if(!tv)
	{
		*hasAssertion = PR_FALSE;
		return NS_OK;
	}

	if((kNC_Child == property))
	{
		nsCOMPtr<nsIFolder> childFolder(do_QueryInterface(target, &rv));
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIFolder> folderasFolder(do_QueryInterface(folder));
			nsCOMPtr<nsIFolder> childsParent;
			rv = childFolder->GetParent(getter_AddRefs(childsParent));
			*hasAssertion = (NS_SUCCEEDED(rv) && childsParent && folderasFolder
							&& (childsParent.get() == folderasFolder.get()));
		}
	}
	else if((kNC_MessageChild == property))
	{
		nsCOMPtr<nsIMessage> message(do_QueryInterface(target, &rv));
		if(NS_SUCCEEDED(rv))
			rv = folder->HasMessage(message, hasAssertion);
	}
	else if ((kNC_Name == property) || (kNC_SpecialFolder == property) || (kNC_TotalMessages == property)
		|| (kNC_TotalUnreadMessages == property) || (kNC_Charset == property) || (kNC_BiffState == property))
	{
		nsCOMPtr<nsIRDFResource> folderResource(do_QueryInterface(folder, &rv));

		if(NS_FAILED(rv))
			return rv;

		rv = GetTargetHasAssertion(this, folderResource, property, tv, target, hasAssertion);
	}
	else 
		*hasAssertion = PR_FALSE;

	return rv;


}

nsresult
NS_NewMsgFolderDataSource(const nsIID& iid, void **result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nsMsgFolderDataSource* datasource = new nsMsgFolderDataSource();
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

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


#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgMailSessionCID,					NS_MSGMAILSESSION_CID);
// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsIRDFResource* nsMsgFolderDataSource::kNC_Child = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_MessageChild = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Folder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Name= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_SpecialFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalUnreadMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Charset = nsnull;

// commands
nsIRDFResource* nsMsgFolderDataSource::kNC_Delete= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NewFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_GetNewMessages= nsnull;



nsMsgFolderDataSource::nsMsgFolderDataSource():
  mInitialized(PR_FALSE),
  mRDFService(nsnull)
{
  NS_INIT_REFCNT();

  nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                             nsIRDFService::GetIID(),
                                             (nsISupports**) &mRDFService); // XXX probably need shutdown listener here

  PR_ASSERT(NS_SUCCEEDED(rv));

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->AddFolderListener(this);

}

nsMsgFolderDataSource::~nsMsgFolderDataSource (void)
{
	nsresult rv;
  mRDFService->UnregisterDataSource(this);

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->RemoveFolderListener(this);

  PL_strfree(mURI);


  nsrefcnt refcnt;
  NS_RELEASE2(kNC_Child, refcnt);
  NS_RELEASE2(kNC_MessageChild, refcnt);
  NS_RELEASE2(kNC_Folder, refcnt);
  NS_RELEASE2(kNC_Name, refcnt);
  NS_RELEASE2(kNC_SpecialFolder, refcnt);
  NS_RELEASE2(kNC_TotalMessages, refcnt);
  NS_RELEASE2(kNC_TotalUnreadMessages, refcnt);
  NS_RELEASE2(kNC_Charset, refcnt);

  NS_RELEASE2(kNC_Delete, refcnt);
  NS_RELEASE2(kNC_NewFolder, refcnt);
  NS_RELEASE2(kNC_GetNewMessages, refcnt);

  nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); // XXX probably need shutdown listener here
  mRDFService = nsnull;
}


NS_IMPL_ADDREF(nsMsgFolderDataSource)
NS_IMPL_RELEASE(nsMsgFolderDataSource)

NS_IMETHODIMP
nsMsgFolderDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if(iid.Equals(nsIFolderListener::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIFolderListener*, this);
		NS_ADDREF(this);
		return NS_OK;
	}
	else
		return nsMsgRDFDataSource::QueryInterface(iid, result);
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsMsgFolderDataSource::Init(const char* uri)
{
  if (mInitialized)
      return NS_ERROR_ALREADY_INITIALIZED;

  if ((mURI = PL_strdup(uri)) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

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
    
	mRDFService->GetResource(NC_RDF_DELETE, &kNC_Delete);
    mRDFService->GetResource(NC_RDF_NEWFOLDER, &kNC_NewFolder);
    mRDFService->GetResource(NC_RDF_GETNEWMESSAGES, &kNC_GetNewMessages);
  }
  mInitialized = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetURI(char* *uri)
{
  if ((*uri = nsXPIDLCString::Copy(mURI)) == nsnull)
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

  *targets = nsnull;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv))
  {
    if (peq(kNC_Child, property))
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
    else if (peq(kNC_MessageChild, property))
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
    else if(peq(kNC_Name, property) || peq(kNC_SpecialFolder, property))
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
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
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

NS_IMETHODIMP nsMsgFolderDataSource::Flush()
{
  PR_ASSERT(0);
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
  }

  if (cmds != nsnull)
    return cmds->Enumerate(commands);
  return NS_ERROR_FAILURE;
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
      if (!(peq(aCommand, kNC_Delete) ||
		    peq(aCommand, kNC_NewFolder) ||
			peq(aCommand, kNC_GetNewMessages))) {
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

  PRUint32 cnt;
  PRUint32 i = 0;

  rv = aSources->Count(&cnt);
  if (NS_FAILED(rv)) return rv;

  if (cnt > 1)
  {
    supports = getter_AddRefs(aSources->ElementAt(0));
    transactionManager = do_QueryInterface(supports, &rv);
    if (NS_SUCCEEDED(rv) && transactionManager)
        i = 1;
  }

  for ( ; i < cnt; i++) {
    supports  = getter_AddRefs(aSources->ElementAt(i));
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
    if (NS_SUCCEEDED(rv)) {
      if (peq(aCommand, kNC_Delete))
	  {
		rv = DoDeleteFromFolder(folder, aArguments, transactionManager);
      }
	  else if(peq(aCommand, kNC_NewFolder)) 
	  {
		rv = DoNewFolder(folder, aArguments);
	  }
	  else if(peq(aCommand, kNC_GetNewMessages))
	  {
		rv = folder->GetNewMessages();
	  }

    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgFolderDataSource::OnItemAdded(nsIFolder *parentFolder, nsISupports *item)
{
	nsresult rv;
	nsCOMPtr<nsIMessage> message;
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIRDFResource> parentResource;

	if(NS_SUCCEEDED(parentFolder->QueryInterface(nsIRDFResource::GetIID(), getter_AddRefs(parentResource))))
	{
		//If we are adding a message
		if(NS_SUCCEEDED(item->QueryInterface(nsIMessage::GetIID(), getter_AddRefs(message))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify folders that a message was added.
				NotifyObservers(parentResource, kNC_MessageChild, itemNode, PR_TRUE);
			}
		}
		//If we are adding a folder
		else if(NS_SUCCEEDED(item->QueryInterface(nsIMsgFolder::GetIID(), getter_AddRefs(folder))))
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

	if(NS_SUCCEEDED(parentFolder->QueryInterface(nsIRDFResource::GetIID(), getter_AddRefs(parentResource))))
	{
		//If we are removing a message
		if(NS_SUCCEEDED(item->QueryInterface(nsIMessage::GetIID(), getter_AddRefs(message))))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify folders that a message was deleted.
				NotifyObservers(parentResource, kNC_MessageChild, itemNode, PR_FALSE);
			}
		}
		//If we are removing a folder
		else if(NS_SUCCEEDED(item->QueryInterface(nsIMsgFolder::GetIID(), getter_AddRefs(folder))))
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
  
  if (peq(kNC_Name, property))
		rv = createFolderNameNode(folder, target);
  else if (peq(kNC_SpecialFolder, property))
		rv = createFolderSpecialNode(folder,target);
	else if (peq(kNC_TotalMessages, property))
		rv = createTotalMessagesNode(folder, target);
	else if (peq(kNC_TotalUnreadMessages, property))
		rv = createUnreadMessagesNode(folder, target);
	else if (peq(kNC_Charset, property))
		rv = createCharsetNode(folder, target);
	else if (peq(kNC_Child, property))
		rv = createFolderChildNode(folder, target);
	else if (peq(kNC_MessageChild, property))
		rv = createFolderMessageNode(folder, target);
 
  return rv;
}


nsresult nsMsgFolderDataSource::createFolderNameNode(nsIMsgFolder *folder,
                                                     nsIRDFNode **target)
{
  char *name;
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
      firstFolder->QueryInterface(nsIRDFResource::GetIID(), (void**)target);
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
    if (NS_SUCCEEDED(messages->First())) {
      nsCOMPtr<nsISupports> firstMessage;
      rv = messages->CurrentItem(getter_AddRefs(firstMessage));
      if (NS_SUCCEEDED(rv)) {
		rv = firstMessage->QueryInterface(nsIRDFNode::GetIID(), (void**)target);
      }
    }
  }
  return rv == NS_OK ? NS_OK : NS_RDF_NO_VALUE;
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
			messageArray->AppendElement(deletedMessage);
		}
		else if(deletedFolder)
		{
			folderArray->AppendElement(deletedFolder);
		}
	}
	PRUint32 cnt;
  rv = messageArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (cnt > 0)
		rv = folder->DeleteMessages(messageArray, txnMgr);

	rv = folderArray->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	if (cnt > 0)
		rv = folder->DeleteSubFolders(folderArray);

	return rv;
}

nsresult nsMsgFolderDataSource::DoNewFolder(nsIMsgFolder *folder, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(arguments->ElementAt(0), &rv));
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

	if(peq(kNC_Charset, property))
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

    if (peq(kNC_Name, property) || peq(kNC_SpecialFolder, property) || peq(kNC_TotalMessages, property)
		|| peq(kNC_TotalUnreadMessages, property) || peq(kNC_Charset, property))
	{
		nsCOMPtr<nsIRDFResource> folderResource(do_QueryInterface(folder, &rv));

		if(NS_FAILED(rv))
			return rv;

		rv = GetTargetHasAssertion(this, folderResource, property, tv, target, hasAssertion);
	}
	else if(peq(kNC_Child, property))
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
	else if(peq(kNC_MessageChild, property))
	{
		nsCOMPtr<nsIMessage> message(do_QueryInterface(target, &rv));
		if(NS_SUCCEEDED(rv))
			rv = folder->HasMessage(message, hasAssertion);
	}
	else
		*hasAssertion = PR_FALSE;

	return rv;


}

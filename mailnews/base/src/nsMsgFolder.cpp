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

#include "nsMsgFolder.h"	 
#include "nsISupports.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceFactory.h"

/* use these macros to define a class IID for our component. */
static NS_DEFINE_IID(kIMsgFolderIID, NS_IMSGFOLDER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID, NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFResourceFactoryIID,  NS_IRDFRESOURCEFACTORY_IID);

nsMsgFolder::nsMsgFolder(const char* uri)
{
	NS_INIT_REFCNT();
    mURI = PL_strdup(uri);

	mFlags = 0;
	mName = nsnull;			

	mDepth = 0;
	mNumUnreadMessages = -1;
	mNumTotalMessages = 0;

#ifdef HAVE_MASTER
  mMaster = NULL;
#endif

#ifdef HAVE_SEMAPHORE
  mSemaphoreHolder = NULL;
#endif

  mPrefFlags = 0;
	mCsid = 0;

#ifdef HAVE_DB
	mLastMessageLoaded	= MSG_MESSAGEKEYNONE;
	mNumPendingUnreadMessages = 0;
	mNumPendingTotalMessages  = 0;
#endif
	NS_NewISupportsArray(&mSubFolders);

#ifdef HAVE_CACHE
	mIsCachable = TRUE;
#endif
}

nsMsgFolder::~nsMsgFolder()
{
	if(mSubFolders)
	{
		PRInt32 count = mSubFolders->Count();

		for (int i = count - 1; i >= 0; i--)
			mSubFolders->RemoveElementAt(i);

		NS_RELEASE(mSubFolders);
	}
    
    PR_FREEIF(mName);

}

NS_IMPL_ADDREF(nsMsgFolder)
NS_IMPL_RELEASE(nsMsgFolder)

NS_IMETHODIMP
nsMsgFolder::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
  if(iid.Equals(kIMsgFolderIID) ||
		iid.Equals(kISupportsIID)) {
		*result = NS_STATIC_CAST(nsIMsgFolder*, this);
	}
	else if(iid.Equals(kIRDFResourceIID) ||
    iid.Equals(kIRDFNodeIID)) {
    *result = NS_STATIC_CAST(nsIRDFResource*, this);
	}

  if(*result != nsnull)
	{
		AddRef();
		return NS_OK;
	}

	return NS_NOINTERFACE;
}

NS_IMETHODIMP nsMsgFolder::EqualsNode(nsIRDFNode* node, PRBool* result) const
{
	nsresult rv;
	nsIRDFResource* resource;
  if (NS_SUCCEEDED(node->QueryInterface(kIRDFResourceIID, (void**) &resource)))
	{
		rv = EqualsResource(resource, result);
		NS_RELEASE(resource);
	}
  else {
		*result = PR_FALSE;
		rv = NS_OK;\
  }
	return rv;
}

NS_IMETHODIMP nsMsgFolder::GetValue(const char* *uri) const
{
	if (!uri)
		return NS_ERROR_NULL_POINTER;
	*uri = mURI;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::EqualsResource(const nsIRDFResource* resource, PRBool* result) const
{
	if (!resource || !result)  return NS_ERROR_NULL_POINTER;

	const char *uri;
	if(NS_SUCCEEDED(resource->GetValue(&uri)))
	{
		return EqualsString(uri, result);
	}

	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgFolder::EqualsString(const char* uri, PRBool* result) const
{
  if (!uri || !result)
		return NS_ERROR_NULL_POINTER;
  *result = (PL_strcmp(uri, mURI) == 0);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetType(FolderType *type)
{
	if(type)
	{
		*type = FOLDER_UNKNOWN;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

#ifdef HAVE_DB
// this class doesn't have a url
NS_IMETHODIMP nsMsgFolder::BuildUrl (MessageDB *db, MessageKey key, char ** url)
{
	if(*url)
	{
		*url = NULL;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}
#endif

#ifdef HAVE_MASTER
NS_IMETHODIMP nsMsgFolder::SetMaster(MSG_Master *master)
{
	mMaster = master;
	return NS_OK;
}
#endif

#ifdef DOES_FOLDEROPERATIONS
NS_IMETHODIMP nsMsgFolder::StartAsyncCopyMessagesInto (MSG_FolderInfo *dstFolder,
                                             MSG_Pane* sourcePane, 
																						 MessageDB *sourceDB,
                                             IDArray *srcArray,
                                             int32 srcCount,
                                             MWContext *currentContext,
                                             MSG_UrlQueue *urlQueue,
                                             XP_Bool deleteAfterCopy,
                                             MessageKey nextKey = MSG_MESSAGEKEYNONE)
{
		// General note: If either the source or destination folder is an IMAP folder then we add the copy info struct
	// to the end of the current context's chain of copy info structs then fire off an IMAP URL.
	// However, local folders don't work this way! We must add the copy info struct to the URL queue where it will be fired
	// at its leisure. 

	MessageCopyInfo *copyInfo = (MessageCopyInfo *) XP_ALLOC(sizeof(MessageCopyInfo));
  
  if (copyInfo)
  {
		XP_BZERO (copyInfo, sizeof(MessageCopyInfo));
		copyInfo->srcFolder = this;
		copyInfo->dstFolder = dstFolder;
		copyInfo->nextCopyInfo = NULL;
		copyInfo->dstIMAPfolderUpdated=FALSE;
		copyInfo->offlineFolderPositionOfMostRecentMessage = 0;
		copyInfo->srcDB = sourceDB;
		copyInfo->srcArray  = srcArray;
		copyInfo->srcCount  = srcCount;
  
		copyInfo->moveState.ismove     = deleteAfterCopy;
		copyInfo->moveState.sourcePane = sourcePane;
		copyInfo->moveState.nextKeyToLoad = nextKeyToLoad;
		copyInfo->moveState.urlForNextKeyLoad = NULL;
		copyInfo->moveState.moveCompleted = FALSE;
		copyInfo->moveState.finalDownLoadMessageSize = 0;
		copyInfo->moveState.imap_connection = 0;
		copyInfo->moveState.haveUploadedMessageSize = FALSE;
      
    MsgERR openErr = eSUCCESS;
    XP_Bool wasCreated;
    if (dstFolder->GetType() == FOLDER_MAIL)
			openErr = MailDB::Open (dstFolder->GetMailFolderInfo()->GetPathname(), FALSE, &copyInfo->moveState.destDB, FALSE);
    else if (dstFolder->GetType() == FOLDER_IMAPMAIL && !IsNews())
      openErr = ImapMailDB::Open (dstFolder->GetMailFolderInfo()->GetPathname(), FALSE, &copyInfo->moveState.destDB, 
                                    sourcePane->GetMaster(), &wasCreated);

                  
    if (!dstFolder->GetMailFolderInfo() || (openErr != eSUCCESS))
        copyInfo->moveState.destDB = NULL;
      
      // let the front end know that we are starting a long update
    sourcePane->StartingUpdate(MSG_NotifyNone, 0, 0);
      
    if ((this->GetType() == FOLDER_IMAPMAIL) || (dstFolder->GetType() == FOLDER_IMAPMAIL))
    {
			// add this copyinfo struct to the end
			if (currentContext->msgCopyInfo != NULL)
			{
        MessageCopyInfo *endingNode = currentContext->msgCopyInfo;
        while (endingNode->nextCopyInfo != NULL)
        	endingNode = endingNode->nextCopyInfo;
        endingNode->nextCopyInfo = copyInfo;
			}
			else
		   	currentContext->msgCopyInfo = copyInfo;
	    // BeginCopyMessages will fire an IMAP url.  The IMAP
      // module will call FinishCopyMessages so that the whole
      // shebang is handled as one IMAP url.  Previously the copy
      // happened with a mailbox url and IMAP url running together
      // in the same context.  This worked on mac only.
      MsgERR copyErr = BeginCopyingMessages(dstFolder, sourceDB, srcArray,urlQueue,srcCount,copyInfo);
      if (0 != copyErr)
      {
      CleanupCopyMessagesInto(&currentContext->msgCopyInfo);
      
      if (/* !NET_IsOffline() && */((int32) copyErr < -1) )
        FE_Alert (sourcePane->GetContext(), XP_GetString(copyErr));
      }
		}
    else
    {
			// okay, add this URL to our URL queue.
			URL_Struct *url_struct = NET_CreateURLStruct("mailbox:copymessages", NET_DONT_RELOAD);
			if (url_struct) 
			{
				MSG_UrlQueue::AddLocalMsgCopyUrlToPane(copyInfo, url_struct, PostMessageCopyUrlExitFunc, sourcePane, FALSE);
			}
		}
  }
	return NS_OK;
}

    
NS_IMETHODIMP nsMsgFolder::BeginCopyingMessages (MSG_FolderInfo *dstFolder, 
																		  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      MSG_UrlQueue *urlQueue,
                                      int32 srcCount,
                                      MessageCopyInfo *copyInfo)
{

	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::FinishCopyingMessages (MWContext *context,
                                      MSG_FolderInfo * srcFolder, 
                                      MSG_FolderInfo *dstFolder, 
																			MessageDB *sourceDB,
                                      IDArray **ppSrcArray, 
                                      int32 srcCount,
                                      msg_move_state *state)
{
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::CleanupCopyMessagesInto (MessageCopyInfo **info)
{
	if (!info || !*info)
	return;

	MSG_Pane *sourcePane = (*info)->moveState.sourcePane;

	XP_Bool searchPane = sourcePane ? sourcePane->GetPaneType() == MSG_SEARCHPANE : FALSE;
	
  if ((*info)->moveState.destDB != NULL)
  {
    (*info)->moveState.destDB->SafeClose((*info)->moveState.destDB);
    (*info)->moveState.destDB = NULL;
  }
  if ((*info)->dstFolder->TestSemaphore(this))
		(*info)->dstFolder->ReleaseSemaphore(this);

	// if we were a search pane, and an error occurred, close the view on this action..
	if (sourcePane != NULL && searchPane)
		((MSG_SearchPane *) sourcePane)->CloseView((*info)->srcFolder);
		
    
  // tell the fe that we are finished with
  // out backend driven update.  They can
  // now do things like load the next message.
  
  // now that an imap copy message is at most 2 urls, we can end the
  // the update here.  Now this is this only ending update and resource
  // cleanup for message copying  
  sourcePane->EndingUpdate(MSG_NotifyNone, 0, 0);
	
	// tell the FE that we're done copying so they can re-enable 
	// selection if they've decided to disable it during the copy
	
	// I don't think we want to do this if we are a search pane but i haven't been able to 
	// justify why yet!!

	if (!searchPane)
		FE_PaneChanged(sourcePane, TRUE, MSG_PaneNotifyCopyFinished, 0);

  // EndingUpdate may have caused an interruption of this context and cleaning up the 
  // url queue may have deleted this  MessageCopyInfo already
  if (*info)
  {
		MessageCopyInfo *deleteMe = *info;
		*info = deleteMe->nextCopyInfo;        // but nextCopyInfo == NULL. this causes the fault later on 
		XP_FREE(deleteMe);
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SaveMessages(IDArray *, const char *fileName, 
                                MSG_Pane *pane, MessageDB *msgDB,
								  int (*doneCB)(void *, int status) = NULL, void *state = NULL,
								  XP_Bool addMozillaStatus = TRUE)
{
  DownloadArticlesToFolder::SaveMessages(array, fileName, pane, this, msgDB, doneCB, state, addMozillaStatus);
	return NS_OK;
}
#endif

NS_IMETHODIMP nsMsgFolder::GetPrettyName(char ** name)
{
	if(name)
	{
		*name = mName;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::GetName(char **name)
{
	if(name)
	{
		*name = mName;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;

}

NS_IMETHODIMP nsMsgFolder::SetName(const char *name)
{
	PR_FREEIF(mName);
	mName = PL_strdup(name);
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetPrettiestName(char **name)
{
	nsresult result;
	if(name)
	{
		if(NS_SUCCEEDED(result = GetPrettyName(name)))
		{
			if(*name)
			{
				return NS_OK;
			}
			else
			{
				return GetName(name);
			}
		}
		else
			return result;
	}
	else return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::GetNameFromPathName(const char *pathName, char **folderName)
{
	if(!folderName)
		return NS_ERROR_NULL_POINTER;

	char* ptr = PL_strrchr(pathName, '/');
	if (ptr) 
		*folderName = ptr + 1;
	else
		*folderName =(char*)pathName;

	return NS_OK;

}


NS_IMETHODIMP nsMsgFolder::HasSubFolders(PRBool *hasSubFolders)
{
	if(hasSubFolders)
	{
		*hasSubFolders = mSubFolders->Count() > 0;
		return NS_OK;
	}
	else return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::GetNumSubFolders(PRInt32 *numSubFolders)
{
	if(numSubFolders)
	{
		*numSubFolders = mSubFolders->Count();
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;

}

NS_IMETHODIMP nsMsgFolder::GetNumSubFoldersToDisplay(PRInt32 *numSubFolders)
{
	if(numSubFolders)
	{
		return GetNumSubFolders(numSubFolders);
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::GetSubFolder(int which, nsIMsgFolder **aFolder)
{
  PR_ASSERT(which >= 0 && which < mSubFolders->Count());
	if(aFolder)
	{
		*aFolder = nsnull;
		nsISupports *folder = mSubFolders->ElementAt(which);
		if(folder)
		{
			folder->QueryInterface(kIMsgFolderIID, (void**)aFolder);

			NS_RELEASE(folder);
		}


		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::GetSubFolders (nsISupportsArray ** subFolders)
{
	if(subFolders)
	{
		NS_ADDREF(mSubFolders);
		*subFolders = mSubFolders;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::AddSubFolder(const nsIMsgFolder *folder)
{
	nsISupports * supports;

	if(NS_SUCCEEDED(((nsISupports*)folder)->QueryInterface(kISupportsIID, (void**)&supports)))
	{
		mSubFolders->AppendElement(supports);
		NS_RELEASE(supports);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::AddSubfolderIfUnique(const nsIMsgFolder *newSubfolder)
{
	return NS_OK;
}
NS_IMETHODIMP nsMsgFolder::ReplaceSubfolder(const nsIMsgFolder *oldFolder, const nsIMsgFolder *newFolder)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::RemoveSubFolder (const nsIMsgFolder *which)
{
	nsISupports * supports;

	if(NS_SUCCEEDED(((nsISupports*)which)->QueryInterface(kISupportsIID, (void**)&supports)))
	{
		mSubFolders->RemoveElement(supports, 0);
		//make sure it's really been removed and no others exist.
		PR_ASSERT(mSubFolders->IndexOf(supports, 0) == -1);
		NS_RELEASE(supports);
	}


#ifdef HAVE_PANE
    if (m_subFolders->GetSize() == 0)
    {
		// Our last child was deleted, so reset our hierarchy bits and tell the panes that this 
		// folderInfo changed, which eventually redraws the expand/collapse widget in the folder pane
        m_flags &= ~MSG_FOLDER_FLAG_DIRECTORY;
        m_flags &= ~MSG_FOLDER_FLAG_ELIDED;
#ifdef HAVE_MASTER
		m_master->BroadcastFolderChanged (this);
#endif
    }
#endif
	return NS_OK;
}

#ifdef HAVE_PANE
NS_IMETHODIMP nsMsgFolder::GetVisibleSubFolders (nsISupportsArray ** visibleSubFolders)
{
	// The folder pane uses this routine to work around the fact
	// that unsubscribed newsgroups are children of the news host.
	// We can't count those when computing folder pane view indexes.

	for (int i = 0; i < m_subFolders->GetSize(); i++)
	{
		MSG_FolderInfo *f = m_subFolders->GetAt(i);
		if (f && f->CanBeInFolderPane())
			subFolders.Add (f);
	}

	return NS_OK;
}

#endif

#ifdef HAVE_ADMINURL
NS_IMETHODIMP nsMsgFolder::GetAdminUrl(MWContext *context, MSG_AdminURLType type)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::HaveAdminUrl(MSG_AdminURLType type, PRBool *haveAdminUrl)
{
	if(haveAdminUrl)
	{
		*haveAdminUrl = PR_FALSE;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}
#endif

NS_IMETHODIMP nsMsgFolder::GetDeleteIsMoveToTrash(PRBool *deleteIsMoveToTrash)
{
	if(deleteIsMoveToTrash)
	{
		*deleteIsMoveToTrash = PR_FALSE;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::GetShowDeletedMessages(PRBool *showDeletedMessages)
{
	if(showDeletedMessages)
	{
		*showDeletedMessages = PR_FALSE;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsMsgFolder::OnCloseFolder ()
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::Delete ()
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::PropagateDelete (nsIMsgFolder **folder, PRBool deleteStorage)
{
	nsresult status = NS_OK;

	nsIMsgFolder *child = nsnull;

	// first, find the folder we're looking to delete
	for (int i = 0; i < mSubFolders->Count() && *folder; i++)
	{
		nsISupports *supports = mSubFolders->ElementAt(i);
		if(supports)
		{
			if(NS_SUCCEEDED(status = supports->QueryInterface(kIMsgFolderIID, (void**)&child)))
			{
				if (*folder == child)
				{
					// maybe delete disk storage for it, and its subfolders
					status = child->RecursiveDelete(deleteStorage);	

					if (status == NS_OK) 
					{
#ifdef HAVE_MASTER
						PR_ASSERT(mMaster);
						// Send out a broadcast message that this folder is going away.
						// Many important things happen on this broadcast.
						mMaster->BroadcastFolderDeleted (child);
#endif
						RemoveSubFolder(child);
						NS_RELEASE(*folder);	// stop looking since will set *folder to nsnull
					}
				}
				else
				{
					PRInt32 folderDepth, childDepth;

					if(NS_SUCCEEDED((*folder)->GetDepth(&folderDepth)) &&
					   NS_SUCCEEDED(child->GetDepth(&childDepth)) &&
						folderDepth > childDepth)
					{ 
						status = child->PropagateDelete (folder, deleteStorage);
					}
				}
				NS_RELEASE(child);
			}
			NS_RELEASE(supports);
		}
	}
	
	return status;
}

NS_IMETHODIMP nsMsgFolder::RecursiveDelete (PRBool deleteStorage)
{
	// If deleteStorage is TRUE, recursively deletes disk storage for this folder
	// and all its subfolders.
	// Regardless of deleteStorage, always unlinks them from the children lists and
	// frees memory for the subfolders but NOT for _this_

	nsresult status = NS_OK;
	
	while (mSubFolders->Count() > 0)
	{
		nsISupports *supports = mSubFolders->ElementAt(0);
		nsIMsgFolder *child = nsnull;

		if(NS_SUCCEEDED(status = supports->QueryInterface(kIMsgFolderIID, (void**)&child)))
		{
			status = child->RecursiveDelete(deleteStorage);  // recur

#ifdef HAVE_MASTER
			// Send out a broadcast message that this folder is going away.
			// Many important things happen on this broadcast.
			mMaster->BroadcastFolderDeleted (child);
#endif
			RemoveSubFolder(child);  // unlink it from this's child list
			NS_RELEASE(child);
		}
		NS_RELEASE(supports);  // free memory
	}

	// now delete the disk storage for _this_
	if (deleteStorage && (status == NS_OK))
		status = Delete();

	return status;
}

NS_IMETHODIMP nsMsgFolder::CreateSubfolder (const char *, nsIMsgFolder**, PRInt32*)
{
	return NS_OK;

}


NS_IMETHODIMP nsMsgFolder::Rename (const char *name)
{
    nsresult status = NS_OK;
	status = SetName(name);
	//After doing a SetName we need to make sure that broadcasting this message causes a
	//new sort to happen.
#ifdef HAVE_MASTER
	if (m_master)
		m_master->BroadcastFolderChanged(this);
#endif
	return status;

}

NS_IMETHODIMP nsMsgFolder::Adopt (const nsIMsgFolder *srcFolder, PRInt32* outPos)
{
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::ContainsChildNamed (const char *name, PRBool* containsChild)
{
	nsIMsgFolder *child;
	
	if(containsChild)
	{
		*containsChild = PR_FALSE;
		if(NS_SUCCEEDED(FindChildNamed(name, &child)))
		{
			*containsChild = child != nsnull;
			if(child)
				NS_RELEASE(child);
		}
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::FindChildNamed (const char *name, nsIMsgFolder ** aChild)
{
	if(!aChild)
		return NS_ERROR_NULL_POINTER;

	// will return nsnull if we can't find it
	*aChild = nsnull;

    nsIMsgFolder *folder = nsnull;

	PRInt32 count = mSubFolders->Count();

    for (int i = 0; i < count; i++)
    {
		nsISupports *supports;
        supports = mSubFolders->ElementAt(i);
		if(folder)
			NS_RELEASE(folder);
		if(NS_SUCCEEDED(supports->QueryInterface(kISupportsIID, (void**)&folder)))
		{
			FolderType type;
			char *folderName;


			GetType(&type);
			folder->GetName(&folderName);
			if (type == FOLDER_IMAPMAIL ||
				type == FOLDER_IMAPSERVERCONTAINER)
			{
				// IMAP INBOX is case insensitive
				if (type == FOLDER_IMAPSERVERCONTAINER &&
					!PL_strcasecmp(folderName, "INBOX"))
				{
					NS_RELEASE(supports);
					continue;
				}

				// For IMAP, folder names are case sensitive
				if (!PL_strcmp(folderName, name))
				{
					*aChild = folder;
					return NS_OK;
				}
			}
			else
			{
				// case-insensitive compare is probably LCD across OS filesystems
				if (!PL_strcasecmp(folderName, name))
				{
					*aChild = folder;
					return NS_OK;
				}
			}
		}
		NS_RELEASE(supports);
    }

	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::FindParentOf (const nsIMsgFolder * aFolder, nsIMsgFolder ** aParent)
{
	if(!aParent)
		return NS_ERROR_NULL_POINTER;

	*aParent = nsnull;

	PRInt32 count = mSubFolders->Count();

	for (int i = 0; i < count && *aParent == NULL; i++)
  {
		nsISupports *supports = mSubFolders->ElementAt(i);
		nsIMsgFolder *child = nsnull;
		if(NS_SUCCEEDED(supports->QueryInterface(kIMsgFolderIID, (void**)&child)))
		{
			if (aFolder == child)
			{
				*aParent = this;
				NS_ADDREF(*aParent);
			}

			NS_RELEASE(child);
		}
		NS_RELEASE(supports);
	}

  for (int j = 0; j < count && *aParent == NULL; j++)
  {

   	nsISupports *supports = mSubFolders->ElementAt(j);
		nsIMsgFolder *child = nsnull;
		if(NS_SUCCEEDED(supports->QueryInterface(kIMsgFolderIID, (void**)&child)))
		{
			child->FindParentOf(aFolder, aParent);
			NS_RELEASE(child);
		}
		NS_RELEASE(supports);

	}

	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::IsParentOf (const nsIMsgFolder *child, PRBool deep, PRBool *isParent)
{
	if(!isParent)
		return NS_ERROR_NULL_POINTER;

	PRInt32 count = mSubFolders->Count();

  for (int i = 0; i < count; i++)
  {
		nsISupports *supports = mSubFolders->ElementAt(i);
		nsIMsgFolder *folder;
		if(NS_SUCCEEDED(supports->QueryInterface(kIMsgFolderIID, (void**)&folder)))
		{
			if (folder == child )
				*isParent = PR_TRUE;
			else if(deep)
			{
				folder->IsParentOf(child, deep, isParent);
			}
		}
		NS_RELEASE(supports);
		if(*isParent)
			return NS_OK;
    }
	*isParent = PR_FALSE;
	return NS_OK;

}


NS_IMETHODIMP nsMsgFolder::GenerateUniqueSubfolderName(const char *prefix, const nsIMsgFolder *otherFolder,
													char **name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	//we don't support this.
	*name = nsnull;
	return NS_OK;

}


NS_IMETHODIMP nsMsgFolder::GetDepth(PRInt32 *depth)
{
	if(!depth)
		return NS_ERROR_NULL_POINTER;
	*depth = mDepth;
	return NS_OK;
	
}

NS_IMETHODIMP nsMsgFolder::SetDepth(PRInt32 depth)
{
	mDepth = depth;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::UpdateSummaryTotals()
{
	//We don't support this
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SummaryChanged()
{
	UpdateSummaryTotals();
#ifdef HAVE_MASTER
    if (mMaster)
        mMaster->BroadcastFolderChanged(this);
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetNumUnread(PRBool deep, PRInt32 *numUnread)
{
	if(!numUnread)
		return NS_ERROR_NULL_POINTER;

	PRInt32 total = mNumUnreadMessages;
  if (deep)
  {
		nsIMsgFolder *folder;
		PRInt32 count = mSubFolders->Count();

    for (int i = 0; i < count; i++)
    {
			nsISupports *supports = mSubFolders->ElementAt(i);

			if(NS_SUCCEEDED(supports->QueryInterface(kIMsgFolderIID, (void**)&folder)))
			{
				if (folder)
				{
					PRInt32 num;
					folder->GetNumUnread(deep, &num);
					if (num >= 0) // it's legal for counts to be negative if we don't know
						total += num;
				}
				NS_RELEASE(folder);
			}
			NS_RELEASE(supports);
		}
	}
  *numUnread = total;
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::GetTotalMessages(PRBool deep, PRInt32 *totalMessages)
{
	if(!totalMessages)
		return NS_ERROR_NULL_POINTER;

	PRInt32 total = mNumTotalMessages;
  if (deep)
  {
    nsIMsgFolder *folder;
		PRInt32 count = mSubFolders->Count();

    for (int i = 0; i < count; i++)
    {
			nsISupports *supports = mSubFolders->ElementAt(i);

			if(NS_SUCCEEDED(supports->QueryInterface(kIMsgFolderIID, (void**)&folder)))
			{
				if (folder)
				{
					PRInt32 num;
					folder->GetTotalMessages (deep, &num);
          if (num >= 0) // it's legal for counts to be negative if we don't know
						total += num;
					NS_RELEASE(folder);
				}
			}
			NS_RELEASE(supports);
		}
	}
  *totalMessages = total;
  return NS_OK;
}

#ifdef HAVE_DB
	NS_IMETHOD GetTotalMessagesInDB(PRInt32 *totalMessages) const;					// How many messages in database.
#endif
	
#ifdef HAVE_PANE
	virtual void	MarkAllRead(MSG_Pane *pane, XP_Bool deep);
#endif

#ifdef HAVE_DB	
	// These functions are used for tricking the front end into thinking that we have more 
	// messages than are really in the DB.  This is usually after and IMAP message copy where
	// we don't want to do an expensive select until the user actually opens that folder
	// These functions are called when MSG_Master::GetFolderLineById is populating a MSG_FolderLine
	// struct used by the FE
	int32			GetNumPendingUnread(PRBool deep = FALSE) const;
	int32			GetNumPendingTotalMessages(PRBool deep = FALSE) const;
	
	void			ChangeNumPendingUnread(int32 delta);
	void			ChangeNumPendingTotalMessages(int32 delta);


NS_IMETHODIMP nsMsgFolder::SetFolderPrefFlags(PRInt32 flags)
{

}

NS_IMETHODIMP nsMsgFolder::GetFolderPrefFlags(PRInt32 *flags)
{

}

NS_IMETHODIMP nsMsgFolder::SetFolderCSID(PRInt16 csid)
{

}

NS_IMETHODIMP nsMsgFolder::GetFolderCSID(PRInt16 *csid)
{

}

NS_IMETHODIMP nsMsgFolder::SetLastMessageLoaded(MessageKey lastMessageLoaded)
{

}

NS_IMETHODIMP nsMsgFolder::GetLastMessageLoaded()
{

}

#endif

NS_IMETHODIMP nsMsgFolder::SetFlag (PRInt32 which)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ClearFlag (PRInt32 which)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFlag(PRInt32 flag, PRBool *_retval)
{
	return NS_OK;
}

#ifdef HAVE_FLAGS
NS_IMETHODIMP nsMsgFolder::GetFlags(PRUint32 *flags) const
{

}

NS_IMETHODIMP nsMsgFolder::ToggleFlag (PRUint32 which)
{

}

NS_IMETHODIMP nsMsgFolder::OnFlagChange (PRUint32 which)
{

}

NS_IMETHODIMP nsMsgFolder::SetFlagInAllFolderPanes(PRUInt32 which)
{

}

NS_IMETHODIMP nsMsgFolder::TestFlag (PRUint32 which, PRBool *result)
{
	
}

NS_IMETHODIMP nsMsgFolder::GetFoldersWithFlag(PRUint32 flags, nsIMsgFolder** result,
                           PRInt32 resultsize, PRInt32 *numFolders)
{

}



NS_IMETHODIMP nsMsgFolder::GetExpansionArray (nsISupportsArray ** expansionArray )
{

}
#endif

#ifdef HAVE_NET
NS_IMETHODIMP nsMsgFolder::EscapeMessageId(const char *messageId, const char **escapeMessageID)
{

}
#endif

NS_IMETHODIMP nsMsgFolder::GetExpungedBytesCount(PRInt32 *count)
{
	if(!count)
		return NS_ERROR_NULL_POINTER;

	*count = 0;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetDeletable (PRBool *deletable)
{

	if(!deletable)
		return NS_ERROR_NULL_POINTER;

	*deletable = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetCanCreateChildren (PRBool *canCreateChildren)
{
	if(!canCreateChildren)
		return NS_ERROR_NULL_POINTER;

	*canCreateChildren = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetCanBeRenamed (PRBool *canBeRenamed)
{
	if(!canBeRenamed)
		return NS_ERROR_NULL_POINTER;

	*canBeRenamed = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetRequiresCleanup(PRBool *requiredCleanup)
{
	if(!requiredCleanup)
		return NS_ERROR_NULL_POINTER;

	*requiredCleanup = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ClearRequiresCleanup()
{
	return NS_OK;
}

#ifdef HAVE_PANE
NS_IMETHODIMP nsMsgFolder::CanBeInFolderPane (PRBool *canBeInFolderPane)
{

}
#endif

NS_IMETHODIMP nsMsgFolder::GetKnowsSearchNntpExtension(PRBool *knowsExtension)
{
	if(!knowsExtension)
		return NS_ERROR_NULL_POINTER;

	*knowsExtension = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetAllowsPosting(PRBool *allowsPosting)
{
	if(!allowsPosting)
		return NS_ERROR_NULL_POINTER;

	*allowsPosting = PR_TRUE;
	return NS_OK;
}

#ifdef HAVE_FLAGS
NS_IMETHODIMP nsMsgFolder::DisplayRecipients(PRBool *displayRecipients)
{

}
#endif

#ifdef HAVE_SEMAPHORE
NS_IMETHODIMP nsMsgFolder::AcquireSemaphore (void *semHolder)
{

}

NS_IMETHODIMP nsMsgFolder::ReleaseSemaphore (void *semHolder)
{

}

NS_IMETHODIMP nsMsgFolder::TestSemaphore (void *semHolder, PRBool *result)
{

}

NS_IMETHODIMP nsMsgFolder::IsLocked (PRBool *isLocked)
{ 
	*isLocked =  mSemaphoreHolder != NULL;
 }
#endif

#ifdef HAVE_PANE
    MWContext *GetFolderPaneContext();
#endif

#ifdef HAVE_MASTER
    MSG_Master  *GetMaster() {return m_master;}
#endif

#ifdef HAVE_CACHE
NS_IMETHODIMP nsMsgFolder::WriteToCache (XP_File)
{

}

NS_IMETHODIMP nsMsgFolder::ReadFromCache (char *)
{

}

NS_IMETHODIMP nsMsgFolder::IsCachable (PRBool *isCachable)
{

}

NS_IMETHODIMP nsMsgFolder::SkipCacheTokens (char **ppBuf, int numTokens)
{

}
#endif

NS_IMETHODIMP nsMsgFolder::GetRelativePathName (char **pathName)
{
	if(!pathName)
		return NS_ERROR_NULL_POINTER;

	*pathName = nsnull;
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::GetSizeOnDisk(PRInt32 *size)
{
	if(!size)
		return NS_ERROR_NULL_POINTER;

	*size = 0;
	return NS_OK;

}

#ifdef HAVE_NET
NS_IMETHODIMP nsMsgFolder::ShouldPerformOperationOffline(PRBool *performOffline)
{

}
#endif


#ifdef DOES_FOLDEROPERATIONS
NS_IMETHODIMP nsMsgFolder::DownloadToTempFileAndUpload(MessageCopyInfo *copyInfo, IDArray &keysToSave, MSG_FolderInfo *dstFolder, MessageDB *sourceDB)
{

}

NS_IMETHODIMP nsMsgFolder::UpdateMoveCopyStatus(MWContext *context, XP_Bool isMove, int32 curMsgCount, int32 totMessages)
{

}

#endif

NS_IMETHODIMP nsMsgFolder::RememberPassword(const char *password)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetRememberedPassword(char ** password)
{

	if(!password)
		return NS_ERROR_NULL_POINTER;

	*password = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *needsAuthenticate)
{
	if(!needsAuthenticate)
		return NS_ERROR_NULL_POINTER;

	*needsAuthenticate = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetUserName(char **userName)
{
	if(!userName)
		return NS_ERROR_NULL_POINTER;

	*userName = "";
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetHostName(char **hostName)
{
	if(!hostName)
		return NS_ERROR_NULL_POINTER;

	*hostName = "";
	return NS_OK;
}


/**
 * This class creates resources for message folder URIs. It should be
 * registered for the "mailnewsfolder:" prefix.
 */
class nsMsgFolderResourceFactoryImpl : public nsIRDFResourceFactory
{
public:
  nsMsgFolderResourceFactoryImpl(void);
  virtual ~nsMsgFolderResourceFactoryImpl(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateResource(const char* aURI, nsIRDFResource** aResult);
};

nsMsgFolderResourceFactoryImpl::nsMsgFolderResourceFactoryImpl(void)
{
  NS_INIT_REFCNT();
}

nsMsgFolderResourceFactoryImpl::~nsMsgFolderResourceFactoryImpl(void)
{
}

NS_IMPL_ISUPPORTS(nsMsgFolderResourceFactoryImpl, kIRDFResourceFactoryIID);

NS_IMETHODIMP
nsMsgFolderResourceFactoryImpl::CreateResource(const char* aURI, nsIRDFResource** aResult)
{
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsMsgFolder *folder = new nsMsgFolder(aURI);
  if (! folder)
    return NS_ERROR_OUT_OF_MEMORY;

	folder->QueryInterface(kIRDFResourceIID, (void**)aResult);
    return NS_OK;
}

nsresult
NS_NewRDFMsgFolderResourceFactory(nsIRDFResourceFactory** aResult)
{
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsMsgFolderResourceFactoryImpl* factory =
  new nsMsgFolderResourceFactoryImpl();

  if (! factory)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(factory);
  *aResult = factory;
  return NS_OK;
}

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
#include "nsMsgFolderFlags.h"
#include "prprf.h"

/* use these macros to define a class IID for our component. */
static NS_DEFINE_IID(kIMsgFolderIID, NS_IMSGFOLDER_IID);
static NS_DEFINE_IID(kIMsgMailFolderIID, NS_IMSGMAILFOLDER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsMsgFolder::nsMsgFolder()
{
	NS_INIT_REFCNT();

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

  if(*result != nsnull)
	{
		AddRef();
		return NS_OK;
	}

	return NS_NOINTERFACE;
}

NS_IMETHODIMP nsMsgFolder::GetType(FolderType *type)
{
	if(!type)
		return NS_ERROR_NULL_POINTER;

	*type = FOLDER_UNKNOWN;

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::BuildFolderURL(char **url)
{
	if(*url)
	{
		*url = NULL;
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

NS_IMETHODIMP nsMsgFolder::SetFlag(PRInt32 flag)
{
	// OnFlagChange can be expensive, so don't call it if we don't need to
	PRBool flagSet;
	nsresult rv;

	if(!NS_SUCCEEDED(rv = GetFlag(flag, &flagSet)))
		return rv;

	if (!flagSet)
	{
		mFlags |= flag;
		OnFlagChange(flag);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ClearFlag(PRInt32 flag)
{
	// OnFlagChange can be expensive, so don't call it if we don't need to
	PRBool flagSet;
	nsresult rv;

	if(!NS_SUCCEEDED(rv = GetFlag(flag, &flagSet)))
		return rv;

	if (!flagSet)
	{
		mFlags &= ~flag;
		OnFlagChange (flag);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFlag(PRInt32 flag, PRBool *_retval)
{
	*_retval = ((mFlags & flag) != 0);
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ToggleFlag(PRInt32 flag)
{
  mFlags ^= flag;
	OnFlagChange (flag);

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::OnFlagChange(PRInt32 flag)
{
	//Still need to implement
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFlags(PRInt32 *_retval)
{
	*_retval = mFlags;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFoldersWithFlag(PRInt32 flags, nsIMsgFolder **result,
																							PRInt32 resultsize,	PRInt32 *numFolders)
{
	int num = 0;
	if ((flags & mFlags) == flags) {
		if (result && (num < resultsize)) {
			result[num] = this;
		}
		num++;
  }

	nsIMsgFolder *folder = nsnull;
	for (int i=0; i < mSubFolders->Count(); i++) {
		nsISupports *supports = mSubFolders->ElementAt(i);
		if(NS_SUCCEEDED(supports->QueryInterface(kIMsgFolderIID, (void**)&folder)))
		{
			// CAREFUL! if NULL ise passed in for result then the caller
			// still wants the full count!  Otherwise, the result should be at most the
			// number that the caller asked for.
			int numSubFolders;

			if (!result)
			{
				folder->GetFoldersWithFlag(flags, NULL, 0, &numSubFolders);
				num += numSubFolders;
			}
			else if (num < resultsize)
			{
				folder->GetFoldersWithFlag(flags, result + num, resultsize - num, &numSubFolders);
				num += numSubFolders;
			}
			else
			{
				NS_RELEASE(folder);
				NS_RELEASE(supports);
				break;
			}
			NS_RELEASE(folder);
		}
		NS_RELEASE(supports);
	}

  *numFolders = num;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetExpansionArray(const nsISupportsArray *expansionArray)
{
  // the application of flags in GetExpansionArray is subtly different
  // than in GetFoldersWithFlag 

	for (int i = 0; i < mSubFolders->Count(); i++)
	{
		nsISupports *supports = mSubFolders->ElementAt(i);
		nsIMsgFolder *folder = nsnull;

		if(NS_SUCCEEDED(supports->QueryInterface(kIMsgFolderIID, (void**)&folder)))
		{
			((nsISupportsArray*)expansionArray)->InsertElementAt(folder, expansionArray->Count());
			PRInt32 flags;
			folder->GetFlags(&flags);
			if (!(flags & MSG_FOLDER_FLAG_ELIDED))
				folder->GetExpansionArray(expansionArray);
		}
  }

	return NS_OK;
}

#ifdef HAVE_PANE
NS_IMETHODIMP nsMsgFolder::SetFlagInAllFolderPanes(PRUInt32 which)
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

NS_IMETHODIMP nsMsgFolder::DisplayRecipients(PRBool *displayRecipients)
{
	nsresult rv;

	*displayRecipients = PR_FALSE;

	if (mFlags & MSG_FOLDER_FLAG_SENTMAIL && !(mFlags & MSG_FOLDER_FLAG_INBOX))
		*displayRecipients = PR_TRUE;
	else if (mFlags & MSG_FOLDER_FLAG_QUEUE)
		*displayRecipients = PR_TRUE;
	else 
	{
		// Only mail folders can be FCC folders
		if (mFlags & MSG_FOLDER_FLAG_MAIL || mFlags & MSG_FOLDER_FLAG_IMAPBOX) 
		{
			// There's one FCC folder for sent mail, and one for sent news
			nsIMsgFolder *fccFolders[2]; 
			int numFccFolders = 0;
#ifdef HAVE_MASTER
			m_master->GetFolderTree()->GetFoldersWithFlag (MSG_FOLDER_FLAG_SENTMAIL, fccFolders, 2, &numFccFolders);
#endif
			for (int i = 0; i < numFccFolders; i++)
			{
				PRBool isParent;
				if(NS_SUCCEEDED(rv = fccFolders[i]->IsParentOf(this, PR_TRUE, &isParent)))
				{
					if (isParent)
						*displayRecipients = PR_TRUE;
				}
				NS_RELEASE(fccFolders[i]);
			}
		}
	}
	return NS_OK;
}

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


nsMsgMailFolder::nsMsgMailFolder()
{
	mHaveReadNameFromDB = PR_FALSE;
	mPathName = nsnull;
}

nsMsgMailFolder::~nsMsgMailFolder()
{

}

NS_IMPL_ADDREF(nsMsgMailFolder)
NS_IMPL_RELEASE(nsMsgMailFolder)

NS_IMETHODIMP
nsMsgMailFolder::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
  if(iid.Equals(kIMsgFolderIID) ||
		iid.Equals(kISupportsIID)) {
		*result = NS_STATIC_CAST(nsIMsgFolder*, this);
	}
	else if(iid.Equals(kIMsgMailFolderIID))
	{
		*result = NS_STATIC_CAST(nsIMsgMailFolder*, this);
	}

  if(*result != nsnull)
	{
		AddRef();
		return NS_OK;
	}

	return NS_NOINTERFACE;
}

NS_IMETHODIMP nsMsgMailFolder::GetType(FolderType *type)
{
	if(!type)
		return NS_ERROR_NULL_POINTER;

	*type = FOLDER_MAIL;

	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::BuildFolderURL(char **url)
{
	const char *urlScheme = "mailbox:";

	if(!url)
		return NS_ERROR_NULL_POINTER;

	*url = PR_smprintf("%s%s", urlScheme, mPathName);
	return NS_OK;

}

NS_IMETHODIMP nsMsgMailFolder::CreateSubfolder(const char *leafNameFromUser, nsIMsgFolder **outFolder, PRInt32 *outPos)
{
#ifdef HAVE_PORT
	MsgERR status = 0;
	*ppOutFolder = NULL;
	*pOutPos = 0;
	XP_StatStruct stat;
    
    
	// Only create a .sbd pathname if we're not in the root folder. The root folder
	// e.g. c:\netscape\mail has to behave differently than subfolders.
	if (m_depth > 1)
	{
		// Look around in our directory to get a subdirectory, creating it 
    // if necessary
        XP_BZERO (&stat, sizeof(stat));
        if (0 == XP_Stat (m_pathName, &stat, xpMailSubdirectory))
        {
            if (!S_ISDIR(stat.st_mode))
                status = MK_COULD_NOT_CREATE_DIRECTORY; // a file .sbd already exists
        }
        else {
            status = XP_MakeDirectory (m_pathName, xpMailSubdirectory);
			if (status == -1)
				status = MK_COULD_NOT_CREATE_DIRECTORY;
		}
    }
    
	char *leafNameForDisk = CreatePlatformLeafNameForDisk(leafNameFromUser,m_master, this);
	if (!leafNameForDisk)
		status = MK_OUT_OF_MEMORY;

    if (0 == status) //ok so far
    {
        // Now that we have a suitable parent directory created/identified, 
        // we can create the new mail folder inside the parent dir. Again,

        char *newFolderPath = (char*) XP_ALLOC(XP_STRLEN(m_pathName) + XP_STRLEN(leafNameForDisk) + XP_STRLEN(".sbd/") + 1);
        if (newFolderPath)
        {
            XP_STRCPY (newFolderPath, m_pathName);
            if (m_depth == 1)
                XP_STRCAT (newFolderPath, "/");
            else
                XP_STRCAT (newFolderPath, ".sbd/");
            XP_STRCAT (newFolderPath, leafNameForDisk);

	        if (0 != XP_Stat (newFolderPath, &stat, xpMailFolder))
			{
				XP_File file = XP_FileOpen(newFolderPath, xpMailFolder, XP_FILE_WRITE_BIN);
				if (file)
				{
 					// Create an empty database for this mail folder, set its name from the user  
  					MailDB *unusedDb = NULL;
 					MailDB::Open(newFolderPath, TRUE, &unusedDb, TRUE);
  					if (unusedDb)
  					{
 						//need to set the folder name
 					
						MSG_FolderInfoMail *newFolder = BuildFolderTree (newFolderPath, m_depth + 1, m_subFolders, m_master);
 						if (newFolder)
 						{
 							// so we don't show ??? in totals
 							newFolder->SummaryChanged();
 							*ppOutFolder = newFolder;
 							*pOutPos = m_subFolders->FindIndex (0, newFolder);
 						}
 						else
 							status = MK_OUT_OF_MEMORY;
 						unusedDb->SetFolderInfoValid(newFolderPath,0,0);
  						unusedDb->Close();
  					}
					else
					{
						XP_FileClose(file);
						file = NULL;
						XP_FileRemove (newFolderPath, xpMailFolder);
						status = MK_MSG_CANT_CREATE_FOLDER;
					}
					if (file)
					{
						XP_FileClose(file);
						file = NULL;
					}
				}
				else
					status = MK_MSG_CANT_CREATE_FOLDER;
			}
			else
				status = MK_MSG_FOLDER_ALREADY_EXISTS;
			FREEIF(newFolderPath);
        }
        else
            status = MK_OUT_OF_MEMORY;
    }
    FREEIF(leafNameForDisk);
    return status;
#endif //HAVE_PORT
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::RemoveSubFolder (const nsIMsgFolder *which)
{
	// Let the base class do list management
	nsMsgFolder::RemoveSubFolder (which);

	// Derived class is responsible for managing the subdirectory
#ifdef HAVE_PORT
	if (0 == m_subFolders->GetSize())
		XP_RemoveDirectory (m_pathName, xpMailSubdirectory);
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::Delete ()
{
#ifdef HAVE_PORT
	    MessageDB   *db;
    // remove the summary file
    MsgERR status = CloseDatabase (m_pathName, &db);
    if (0 == status)
    {
        if (db != NULL)
            db->Close();    // decrement ref count, so it will leave cache
        XP_FileRemove (m_pathName, xpMailFolderSummary);
    }

    if ((0 == status) && (GetType() == FOLDER_MAIL))
	{
        // remove the mail folder file
        status = XP_FileRemove (m_pathName, xpMailFolder);

		// if the delete seems to have failed, but the file doesn't
		// exist, that's not really an error condition, is it now?
		if (status)
		{
			XP_StatStruct fileStat;
	        if (0 == XP_Stat(m_pathName, &fileStat, xpMailFolder))
				status = 0;
		}
	}


    if (0 != status)
        status = MK_UNABLE_TO_DELETE_FILE;
    return status;  
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::Rename (const char *newName)
{
#ifdef HAVE_PORT
	    // change the leaf name (stored separately)
    MsgERR status = MSG_FolderInfo::Rename (newUserLeafName);
    if (status == 0)
    {
    	char *baseDir = XP_STRDUP(m_pathName);
    	if (baseDir)
    	{
	        char *base_slash = XP_STRRCHR (baseDir, '/'); 
	        if (base_slash)
	            *base_slash = '\0';
        }

    	char *leafNameForDisk = CreatePlatformLeafNameForDisk(newUserLeafName,m_master, baseDir);
    	if (!leafNameForDisk)
    		status = MK_OUT_OF_MEMORY;

        if (0 == status)
		{
		        // calculate the new path name
	        char *newPath = (char*) XP_ALLOC(XP_STRLEN(m_pathName) + XP_STRLEN(leafNameForDisk) + 1);
	        XP_STRCPY (newPath, m_pathName);
	        char *slash = XP_STRRCHR (newPath, '/'); 
	        if (slash)
	            XP_STRCPY (slash + 1, leafNameForDisk);

	        // rename the mail summary file, if there is one
	        MessageDB *db = NULL;
	        status = CloseDatabase (m_pathName, &db);
	        
			XP_StatStruct fileStat;
	        if (!XP_Stat(m_pathName, &fileStat, xpMailFolderSummary))
				status = XP_FileRename(m_pathName, xpMailFolderSummary, newPath, xpMailFolderSummary);
			if (0 == status)
			{
				if (db)
				{
					if (ReopenDatabase (db, newPath) == 0)
					{	
						//need to set mailbox name
					}
				}
				else
				{
					MailDB *mailDb = NULL;
					MailDB::Open(newPath, TRUE, &mailDb, TRUE);
					if (mailDb)
					{
						//need to set mailbox name
						mailDb->Close();
					}
				}
			}

			// rename the mail folder file, if its local
			if ((status == 0) && (GetType() == FOLDER_MAIL))
				status = XP_FileRename (m_pathName, xpMailFolder, newPath, xpMailFolder);

			if (status == 0)
			{
				// rename the subdirectory if there is one
				if (m_subFolders->GetSize() > 0)
				status = XP_FileRename (m_pathName, xpMailSubdirectory, newPath, xpMailSubdirectory);

				// tell all our children about the new pathname
				if (status == 0)
				{
					int startingAt = XP_STRLEN (newPath) - XP_STRLEN (leafNameForDisk) + 1; // add one for trailing '/'
					status = PropagateRename (leafNameForDisk, startingAt);
				}
			}
		}
    	FREEIF(baseDir);
    }
    return status;
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::Adopt(const nsIMsgFolder *srcFolder, PRInt32 *outPos)
{
#ifdef HAVE_PORT
		MsgERR err = eSUCCESS;
	XP_ASSERT (srcFolder->GetType() == GetType());	// we can only adopt the same type of folder
	MSG_FolderInfoMail *mailFolder = (MSG_FolderInfoMail*) srcFolder;

	if (srcFolder == this)
		return MK_MSG_CANT_COPY_TO_SAME_FOLDER;

	if (ContainsChildNamed(mailFolder->GetName()))
		return MK_MSG_FOLDER_ALREADY_EXISTS;	
	
	// If we aren't already a directory, create the directory and set the flag bits
	if (0 == m_subFolders->GetSize())
	{
		XP_Dir dir = XP_OpenDir (m_pathName, xpMailSubdirectory);
		if (dir)
			XP_CloseDir (dir);
		else
		{
			XP_MakeDirectory (m_pathName, xpMailSubdirectory);
			dir = XP_OpenDir (m_pathName, xpMailSubdirectory);
			if (dir)
				XP_CloseDir (dir);
			else
				err = MK_COULD_NOT_CREATE_DIRECTORY;
		}
		if (eSUCCESS == err)
		{
			m_flags |= MSG_FOLDER_FLAG_DIRECTORY;
			m_flags |= MSG_FOLDER_FLAG_ELIDED;
		}
	}

	// Recurse the tree to adopt srcFolder's children
	err = mailFolder->PropagateAdopt (m_pathName, m_depth);

	// Add the folder to our tree in the right sorted position
	if (eSUCCESS == err)
	{
		XP_ASSERT(m_subFolders->FindIndex(0, srcFolder) == -1);
		*pOutPos = m_subFolders->Add (srcFolder);
	}

	return err;
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetName(char** name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	if (!mHaveReadNameFromDB)
	{
		if (mDepth == 1) 
		{
			SetName("Local Mail");
			mHaveReadNameFromDB = TRUE;
		}
		else
		{
			//Need to read the name from the database
		}
	}
	*name = mName;


	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetPrettyName(char ** prettyName)
{
	if (mDepth == 1) {
	// Depth == 1 means we are on the mail server level
	// override the name here to say "Local Mail"
		FolderType type;
		GetType(&type);
		if (type == FOLDER_MAIL)
			*prettyName = PL_strdup("Local Mail");
		else
			return nsMsgFolder::GetPrettyName(prettyName);
	}
	else
		return nsMsgFolder::GetPrettyName(prettyName);

	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GenerateUniqueSubfolderName(const char *prefix, 
																													 const nsIMsgFolder *otherFolder,
																													 char** name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	/* only try 256 times */
	for (int count = 0; (count < 256); count++)
	{
		PRInt32 prefixSize = PL_strlen(prefix);

		//allocate string big enough for prefix, 256, and '\0'
		char *uniqueName = (char*)PR_MALLOC(prefixSize + 4);
		PR_snprintf(uniqueName, prefixSize + 4, "%s%d",prefix,count);
		PRBool containsChild;
		PRBool otherContainsChild = PR_FALSE;

		ContainsChildNamed(uniqueName, &containsChild);
		if(otherFolder)
		{
			((nsIMsgFolder*)otherFolder)->ContainsChildNamed(uniqueName, &otherContainsChild);
		}

		if (!containsChild && !otherContainsChild)
		{
			*name = uniqueName;
			return NS_OK;
		}
		else
			PR_FREEIF(uniqueName);
	}
	*name = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::UpdateSummaryTotals()
{
	//We need to read this info from the database

	// If we asked, but didn't get any, stop asking
	if (mNumUnreadMessages == -1)
		mNumUnreadMessages = -2;

	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetExpungedBytesCount(PRInt32 *count)
{
	if(!count)
		return NS_ERROR_NULL_POINTER;

	*count = mExpungedBytes;

	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetDeletable (PRBool *deletable)
{
	if(!deletable)
		return NS_ERROR_NULL_POINTER;

	// These are specified in the "Mail/News Windows" UI spec

	if (mFlags & MSG_FOLDER_FLAG_TRASH)
	{
		PRBool moveToTrash;
		GetDeleteIsMoveToTrash(&moveToTrash);
		if(moveToTrash)
			*deletable = PR_TRUE;	// allow delete of trash if we don't use trash
	}
	else if (mDepth == 1)
		*deletable = PR_FALSE;
	else if (mFlags & MSG_FOLDER_FLAG_INBOX || 
		mFlags & MSG_FOLDER_FLAG_DRAFTS || 
		mFlags & MSG_FOLDER_FLAG_TRASH ||
		mFlags & MSG_FOLDER_FLAG_TEMPLATES)
		*deletable = PR_FALSE;
	else *deletable =  PR_TRUE;

	return NS_OK;
}
 
NS_IMETHODIMP nsMsgMailFolder::GetCanCreateChildren (PRBool *canCreateChildren)
{	
	if(!canCreateChildren)
		return NS_ERROR_NULL_POINTER;

	*canCreateChildren = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetCanBeRenamed (PRBool *canBeRenamed)
{
	if(!canBeRenamed)
		return NS_ERROR_NULL_POINTER;

	  // The root mail folder can't be renamed
	if (mDepth < 2)
		*canBeRenamed = PR_FALSE;

  // Here's a weird case necessitated because we don't have a separate
  // preference for any folder name except the FCC folder (Sent). Others
  // are known by name, and as such, can't be renamed. I guess.
  else if (mFlags & MSG_FOLDER_FLAG_TRASH ||
      mFlags & MSG_FOLDER_FLAG_DRAFTS ||
      mFlags & MSG_FOLDER_FLAG_QUEUE ||
      mFlags & MSG_FOLDER_FLAG_INBOX ||
			mFlags & MSG_FOLDER_FLAG_TEMPLATES)
      *canBeRenamed = PR_FALSE;
  else 
		*canBeRenamed = PR_TRUE;

	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetRequiresCleanup(PRBool *requiresCleanup)
{
#ifdef HAVE_PORT
	if (m_expungedBytes > 0)
	{
		int32 purgeThreshhold = m_master->GetPrefs()->GetPurgeThreshhold();
		XP_Bool purgePrompt = m_master->GetPrefs()->GetPurgeThreshholdEnabled();;
		return (purgePrompt && m_expungedBytes / 1000L > purgeThreshhold);
	}
	return FALSE;
#endif
	return NS_OK;
}



NS_IMETHODIMP nsMsgMailFolder::GetRelativePathName (char **pathName)
{
	if(!pathName)
		return NS_ERROR_NULL_POINTER;
	*pathName = mPathName;
	return NS_OK;
}


NS_IMETHODIMP nsMsgMailFolder::GetSizeOnDisk(PRInt32 size)
{
#ifdef HAVE_PORT
	int32 ret = 0;
	XP_StatStruct st;

	if (!XP_Stat(GetPathname(), &st, xpMailFolder))
	ret += st.st_size;

	if (!XP_Stat(GetPathname(), &st, xpMailFolderSummary))
	ret += st.st_size;

	return ret;
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetUserName(char** userName)
{
#ifdef HAVE_PORT
	return NET_GetPopUsername();
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetHostName(char** hostName)
{
#ifdef HAVE_PORT
	XP_Bool serverIsIMAP = m_master->GetPrefs()->GetMailServerIsIMAP4();
	if (serverIsIMAP)
	{
		MSG_IMAPHost *defaultIMAPHost = m_master->GetIMAPHostTable()->GetDefaultHost();
		return (defaultIMAPHost) ? defaultIMAPHost->GetHostName() : 0;
	}
	else
		return m_master->GetPrefs()->GetPopHost();
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate)
{
#ifdef HAVE_PORT
		XP_Bool ret = FALSE;
	if (m_master->IsCachePasswordProtected() && !m_master->IsUserAuthenticated() && !m_master->AreLocalFoldersAuthenticated())
	{
		char *savedPassword = GetRememberedPassword();
		if (savedPassword && XP_STRLEN(savedPassword))
			ret = TRUE;
		FREEIF(savedPassword);
	}
	return ret;
#endif

	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::RememberPassword(const char *password)
{
#ifdef HAVE_DB
		MailDB *mailDb = NULL;
	MailDB::Open(m_pathName, TRUE, &mailDb);
	if (mailDb)
	{
		mailDb->SetCachedPassword(password);
		mailDb->Close();
	}
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetRememberedPassword(char ** password)
{
#ifdef HAVE_PORT
		XP_Bool serverIsIMAP = m_master->GetPrefs()->GetMailServerIsIMAP4();
	char *savedPassword = NULL; 
	if (serverIsIMAP)
	{
		MSG_IMAPHost *defaultIMAPHost = m_master->GetIMAPHostTable()->GetDefaultHost();
		if (defaultIMAPHost)
		{
			MSG_FolderInfo *hostFolderInfo = defaultIMAPHost->GetHostFolderInfo();
			MSG_FolderInfo *defaultHostIMAPInbox = NULL;
			if (hostFolderInfo->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &defaultHostIMAPInbox, 1) == 1 
				&& defaultHostIMAPInbox != NULL)
			{
				savedPassword = defaultHostIMAPInbox->GetRememberedPassword();
			}
		}
	}
	else
	{
		MSG_FolderInfo *offlineInbox = NULL;
		if (m_flags & MSG_FOLDER_FLAG_INBOX)
		{
			char *retPassword = NULL;
			MailDB *mailDb = NULL;
			MailDB::Open(m_pathName, FALSE, &mailDb, FALSE);
			if (mailDb)
			{
				mailDb->GetCachedPassword(cachedPassword);
				retPassword = XP_STRDUP(cachedPassword);
				mailDb->Close();

			}
			return retPassword;
		}
		if (m_master->GetLocalMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &offlineInbox, 1) && offlineInbox)
			savedPassword = offlineInbox->GetRememberedPassword();
	}
	return savedPassword;
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::GetPathName(char * *aPathName)
{
	if(!aPathName)
		return NS_ERROR_NULL_POINTER;

	if(mPathName)
		*aPathName = PL_strdup(mPathName);
	else
		*aPathName = nsnull;

	return NS_OK;
}

NS_IMETHODIMP nsMsgMailFolder::SetPathName(char * aPathName)
{
	if(mPathName)
		PR_FREEIF(mPathName);

	if(aPathName)
		mPathName = PL_strdup(aPathName);
	else
		mPathName = nsnull;

	return NS_OK;
}

nsresult
NS_NewMsgMailFolder(nsIMsgFolder** aResult)
{
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsMsgMailFolder* folder =
		new nsMsgMailFolder();

  if (! folder)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(folder);
  *aResult = folder;
  return NS_OK;
}
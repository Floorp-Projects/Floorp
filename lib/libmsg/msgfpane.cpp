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
#include "msg.h"
#include "errcode.h"
#include "xp_str.h"
#include "xpgetstr.h"

#include "msgfpane.h"
#include "msgimap.h"
#include "newshost.h"
#include "imaphost.h"

#include "msgtpane.h"
#include "msgmpane.h"

#include "msgprefs.h"
#include "msgdbvw.h"
#include "mailhdr.h"
#include "maildb.h"
#include "msgmast.h"

#include "msg_filt.h"
#include "pmsgfilt.h"
#include "pmsgsrch.h"
#include "prsembst.h"
#include "grpinfo.h"
#include "msgrulet.h"
#include "dwordarr.h"
#include "newshost.h"
#include "msgundac.h"
#include "msgurlq.h"
#include "hosttbl.h"
#include "xp_qsort.h"
#include "prefapi.h"
extern "C"
{
extern int MK_OUT_OF_MEMORY;
extern int MK_MSG_OFFER_COMPRESS;
extern int MK_MSG_OPEN_FOLDER;
extern int MK_MSG_OPEN_FOLDER2;
extern int MK_MSG_NEW_FOLDER;
extern int MK_MSG_COMPRESS_FOLDER;
extern int MK_MSG_COMPRESS_ALL_FOLDER;
extern int MK_MSG_OPEN_NEWS_HOST_ETC;
extern int MK_MSG_ADD_NEWS_GROUP;
extern int MK_MSG_EMPTY_TRASH_FOLDER;
extern int MK_MSG_UNDO;
extern int MK_MSG_REDO;
extern int MK_MSG_DELETE_FOLDER;
extern int MK_MSG_RMV_NEWS_HOST;
extern int MK_MSG_SUBSCRIBE;
extern int MK_MSG_UNSUBSCRIBE;
extern int MK_MSG_NEW_NEWS_MESSAGE;
extern int MK_MSG_MARK_ALL_READ;
extern int MK_MSG_CANT_OPEN;
extern int MK_MSG_ENTER_FOLDERNAME;
extern int MK_MSG_RENAME_FOLDER;
extern int MK_MSG_DELETE_FOLDER_MESSAGES;
extern int MK_MSG_FOLDER_ALREADY_EXISTS;
extern int MK_MSG_CAN_ONLY_DELETE_MAIL_FOLDERS;
extern int XP_NEWS_PROMPT_ADD_NEWSGROUP;
extern int MK_MSG_UNSUBSCRIBE_GROUP;
extern int MK_MSG_UNSUBSCRIBE_PROFILE_GROUP;
extern int MK_MSG_PANES_OPEN_ON_FOLDER;
extern int MK_MSG_ILLEGAL_CHARS_IN_NAME;
extern int MK_MSG_NEW_NEWSGROUP;
extern int MK_MSG_CANT_MOVE_FOLDER_TO_CHILD;
extern int MK_MSG_NEW_GROUPS_DETECTED;
extern int MK_MSG_REMOVE_HOST_CONFIRM;
extern int MK_MSG_NO_POP_HOST;
extern int MK_MSG_RENAME_FOLDER_CAPTION;
extern int MK_MSG_NEW_FOLDER_CAPTION;
extern int MK_MSG_MANAGE_MAIL_ACCOUNT;
extern int MK_MSG_UNABLE_MANAGE_MAIL_ACCOUNT;
extern int MK_POP3_NO_MESSAGES;
extern int MK_MSG_MODERATE_NEWSGROUP;
extern int MK_MSG_CANT_MOVE_INBOX;
extern int MK_MSG_CONFIRM_MOVE_MAGIC_FOLDER;
extern int MK_MSG_UPDATE_MSG_COUNTS;
extern int MK_MSG_MOVE_TARGET_NO_INFERIORS;
extern int MK_MSG_PARENT_TARGET_NO_INFERIORS;
extern int MK_MSG_TRASH_NO_INFERIORS;
extern int MK_MSG_TRACK_FOLDER_MOVE;
extern int MK_MSG_TRACK_FOLDER_DEL;
extern int MK_MSG_NO_MAIL_TO_NEWS;
extern int MK_MSG_NO_NEWS_TO_MAIL;
extern int MK_MSG_UNIQUE_TRASH_RENAME_FAILED;
extern int MK_MSG_NEEDED_UNIQUE_TRASH_NAME;
extern int MK_MSG_CANT_MOVE_FOLDER;
extern int MK_MSG_CONFIRM_MOVE_FOLDER_TO_TRASH;
extern int MK_MSG_REMOVE_IMAP_HOST_CONFIRM;
}


#ifdef XP_OS2
#define qsort(a,b,c,d) qsort(a,b,c,(int (_Optlink*) (const void*,const void*)) d)
#endif


MSG_FolderPane::MSG_FolderPane(MWContext* context, MSG_Master* master)
: MSG_LinedPane(context, master)
{
}

MsgERR MSG_FolderPane::Init()
{
    FEStart();
    MSG_FolderInfo* folderTree = m_master->GetFolderTree();

    int32 num = folderTree->GetNumSubFolders();
    if (num != 0) 
    {
        for (int32 i = 0; i < num; i++) 
        {
            // Add the top level folder
            MSG_FolderInfo *folder = folderTree->GetSubFolder((MSG_ViewIndex) i);
			if (folder->CanBeInFolderPane ())
			{
				int index = m_folderView.Add (folder);
				m_flagsArray.Add(folder->GetFlags() & 0xFF);

				// Add any children of the folder which might be expanded
				if (!(folder->GetFlags() & MSG_FOLDER_FLAG_ELIDED) && !(folder->GetType() == FOLDER_CATEGORYCONTAINER))
				{
					MSG_FolderArray array;
					GetExpansionArray (folder, array);
					if (array.GetSize() != 0)
					{
						InsertFlagsArrayAt(++index, array);
						m_folderView.InsertAt (index, &array);
					}
				}
			}
        }
    }

    FEEnd();
    // for fun, ### dmb - remove when we figure out how notifications should work
    FlushUpdates(); 
    return eSUCCESS;
}

MSG_FolderPane::~MSG_FolderPane() 
{
    if (m_scanTimer)
        FE_ClearTimeout (m_scanTimer);
}


void
MSG_FolderPane::RedisplayAll()
{
    StartingUpdate(MSG_NotifyAll, 0, 0);
    m_folderView.RemoveAll();
	m_flagsArray.RemoveAll();
    Init();
    EndingUpdate(MSG_NotifyAll, 0, 0);
}

MSG_PaneType MSG_FolderPane::GetPaneType() 
{
    return MSG_FOLDERPANE;
}

void MSG_FolderPane::NotifyPrefsChange(NotifyCode) 
{
    // ###tw  Write me!
}

void MSG_FolderPane::InsertFlagsArrayAt(MSG_ViewIndex index, MSG_FolderArray &folders)
{
	for (int32 arrayIndex = 0; arrayIndex < folders.GetSize(); arrayIndex++)
	{
		MSG_FolderInfo *arrayFolder = folders.GetAt(arrayIndex);
		m_flagsArray.InsertAt(index + arrayIndex, arrayFolder->GetFlags() & 0xFF);
	}
}


XP_Bool
MSG_FolderPane::ScanFolder_1()
{
    // ###tw To heck with it.  This should search through all the folders, and
    // find one that does not have an accurate count of unread messages in
    // memory.  If it finds one, then this should attempt to quickly get that
    // number from the database, and then return TRUE.  If every folder either
    // has an accurate count or we have already determined an accurate count
    // can't be gotten quickly, then this routine should return FALSE.
    for (int32 i=0 ; i<m_folderView.GetSize() ; i++) 
    {
        MSG_FolderInfo *folder = m_folderView.GetAt((MSG_ViewIndex) i);
        if (folder->GetNumUnread() == -1) 
		{
            if (folder->GetType() == FOLDER_MAIL || folder->IsNews()) 
			{
                folder->UpdateSummaryTotals();
                GetMaster()->BroadcastFolderChanged(folder);
                return TRUE;
            }
        }
    }
    return FALSE;
}

void 
MSG_FolderPane::ScanFolder_s(void* closure)
{
    ((MSG_FolderPane*) closure)->ScanFolder();
}

void 
MSG_FolderPane::ScanFolder()
{
    m_scanTimer = NULL;
    if (ScanFolder_1()) {
        if (m_scanTimer == NULL) {
            m_scanTimer = FE_SetTimeout((TimeoutCallbackFunction)MSG_FolderPane::ScanFolder_s, this, 1);
        }
    } else {
        if (m_wantCompress && !m_offeredCompress) {
            char* buf;
            if (NET_AreThereActiveConnectionsForWindow(m_context)) {
                /* Looks like we're busy doing something, probably incorporating
                new mail.  Don't interrupt that; we can wait until we're idle
                to ask our silly compression question.  Just try again in a
                half-second. */
                m_scanTimer = FE_SetTimeout((TimeoutCallbackFunction) MSG_FolderPane::ScanFolder_s, this, 500);
                return;
            }
            buf = PR_smprintf(XP_GetString(MK_MSG_OFFER_COMPRESS),
            /*###tw msg_addup_wasted_bytes(f->folders)*/ 0 / 1024);
            if (buf) {
                m_offeredCompress = TRUE; /* Set it now, before FE_Confirm gets called,
                as timers and things can get called from
                FE_Confirm, and we could end up back here
                again. */
                if (FE_Confirm(m_context, buf)) {
                    msg_InterruptContext(m_context, FALSE);
                    CompressAllFolders();
                }
                XP_FREE(buf);
            }
        }
        m_offeredCompress = TRUE;
    }
}


void
MSG_FolderPane::FlushUpdates()
{
    // ###tw  This stuff probably needs to get moved somewhere else, and this
    // FlushUpdates() method should probably go away...
    if (m_scanTimer == NULL) {
        /* What a disgusting way to do a background job.  ### */
        m_scanTimer = FE_SetTimeout((TimeoutCallbackFunction) MSG_FolderPane::ScanFolder_s, this, 1);
    }
}



XP_Bool MSG_FolderPane::GetFolderLineByIndex(MSG_ViewIndex line, int32 numlines, MSG_FolderLine* data)
{
    if (line == MSG_VIEWINDEXNONE || line + numlines > (MSG_ViewIndex) m_folderView.GetSize() || numlines < 1) 
        return FALSE;
   
    for (int i=0 ; i<numlines ; i++) 
	{
        MSG_FolderInfo *folder = m_folderView.GetAt(line + i);
        XP_Bool result = m_master->GetFolderLineById(folder, data + i);
		int8	flags = m_flagsArray.GetAt(line + i);
		if (flags & MSG_FOLDER_FLAG_ELIDED)
			data->flags |= MSG_FOLDER_FLAG_ELIDED;
		else
			data->flags &= ~MSG_FOLDER_FLAG_ELIDED;
        XP_ASSERT(result);
        if (!result) 
            return FALSE;
    }
    return TRUE;
}

int MSG_FolderPane::GetFolderLevelByIndex(MSG_ViewIndex line)
{
    if (line == MSG_VIEWINDEXNONE || line >= (MSG_ViewIndex) m_folderView.GetSize()) 
        return 0;
    
    MSG_FolderLine data[1];
    MSG_FolderInfo *folder = m_folderView.GetAt(line);
    XP_Bool result = m_master->GetFolderLineById(folder, data);
    XP_ASSERT(result);
    if (!result) 
        return 0;
    return data[0].level;
}


int MSG_FolderPane::GetFolderMaxDepth()
{
    int result = 0;
    for (MSG_ViewIndex i=0 ; i < (MSG_ViewIndex) m_folderView.GetSize() ; i++) 
	{
        uint8 depth = m_folderView.GetAt(i)->GetDepth();
        if (result < depth)
            result = depth;
    }
    return result;
}



char*
MSG_FolderPane::GetFolderName(MSG_ViewIndex line) 
{
    XP_ASSERT(int(line) >= 0 && line < (MSG_ViewIndex) m_folderView.GetSize());
    XP_ASSERT ( m_folderView.GetAt(line)->IsMail() );

    if (int(line) >= 0 && line < (MSG_ViewIndex)  m_folderView.GetSize()) 
	{
        MSG_FolderInfoMail* f = m_folderView.GetAt(line)->GetMailFolderInfo();
        if (f && (f->GetType() == FOLDER_MAIL) || (f->GetType() == FOLDER_IMAPMAIL)) 
            return XP_STRDUP(f->GetPathname());
    }
    return NULL;
}


void MSG_FolderPane::MakeFolderVisible (MSG_FolderInfo *folder)
{
	MSG_ViewIndex idx = GetFolderIndex (folder);
	if (idx == MSG_VIEWINDEXNONE)
	{
		MSG_FolderInfo *parent = m_master->GetFolderTree()->FindParentOf (folder);
		if (parent)
		{
			MakeFolderVisible (parent);
			idx = GetFolderIndex (folder);
		}
	}

	if (idx != MSG_VIEWINDEXNONE && m_flagsArray.GetAt(idx) & MSG_FOLDER_FLAG_ELIDED)
	{
		int32 unused;
		ToggleExpansion (idx, &unused);
	}
}


MSG_ViewIndex MSG_FolderPane::GetFolderIndex(MSG_FolderInfo *folder, XP_Bool expand /*= FALSE*/)
{
	MSG_ViewIndex idx = (MSG_ViewIndex) m_folderView.FindIndex (0, folder);
	if (idx == MSG_VIEWINDEXNONE && expand)
	{
		MakeFolderVisible (folder);
		idx = (MSG_ViewIndex) m_folderView.FindIndex (0, folder);
	}
	return idx;
}


MSG_FolderInfo *MSG_FolderPane::GetFolderInfo(MSG_ViewIndex index)
{
    XP_ASSERT(index != MSG_VIEWINDEXNONE);
    XP_ASSERT(index < (MSG_ViewIndex) m_folderView.GetSize());
    if (index == MSG_VIEWINDEXNONE || index >= (MSG_ViewIndex) m_folderView.GetSize()) 
        return NULL;

    return m_folderView.GetAt(index);
}

/* becoming obsolete with switch to new Subscribe MSG_Host* APIs */
MSG_NewsHost *MSG_FolderPane::GetNewsHostFromIndex (MSG_ViewIndex index)
{
	MSG_NewsHost *host = NULL;

	MSG_FolderInfo *folder = GetFolderInfo (index);
	msg_HostTable *table = m_master->GetHostTable();
	if (folder && table)
		host = table->FindNewsHost (folder);

	return host;
}

MSG_Host *MSG_FolderPane::GetHostFromIndex (MSG_ViewIndex index)
{
	MSG_FolderInfo *folder = GetFolderInfo (index);
	if (folder)
	{
		MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
		if (newsFolder) 
			return newsFolder->GetHost();
		else if (FOLDER_CONTAINERONLY == folder->GetType())
			return ((MSG_FolderInfoContainer*) folder)->GetHost();
		else if (FOLDER_IMAPSERVERCONTAINER == folder->GetType())
			return ((MSG_IMAPFolderInfoContainer*) folder)->GetIMAPHost();
		else {
			MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
			if (imapFolder)
			{
				return imapFolder->GetIMAPHost();
			}
		}
	}
	return NULL;
}


void MSG_FolderPane::OnFolderChanged(MSG_FolderInfo *folderInfo)
{
    MSG_ViewIndex index = GetFolderIndex(folderInfo);

    if (index != MSG_VIEWINDEXNONE)
    {
        StartingUpdate (MSG_NotifyChanged, index, 1);   
        EndingUpdate (MSG_NotifyChanged, index, 1);     
    }

	MSG_LinedPane::OnFolderChanged (folderInfo);
}


void
MSG_FolderPane::OpenFolderCB_s(MWContext *context, char *file_name,
                               void *closure)
{
    XP_ASSERT(((MSG_FolderPane*) closure)->m_context == context);
    ((MSG_FolderPane*) closure)->OpenFolderCB(file_name);
}


void
MSG_FolderPane::OpenFolderCB(char* file_name) 
{
    if (!file_name) 
        return;

    MSG_FolderInfo* folder = FindMailFolder(file_name, TRUE);
    if (folder) {
        // ###tw  Uh, do something to let the FE's know about it?  Maybe not...
    }
}

MsgERR
MSG_FolderPane::OpenFolder ()
{
    FE_PromptForFileName (m_context, XP_GetString(MK_MSG_OPEN_FOLDER), 0, /* default_path */
                        TRUE, FALSE, (ReadFileNameCallbackFunction) MSG_FolderPane::OpenFolderCB_s, this);
    return 0;
}

// The IMAP libnet module is still discovering folders after this
// pane was created.  So we need a way to add folders as they come in.
void MSG_FolderPane::OnFolderAdded(MSG_FolderInfo *addedFolder, MSG_Pane *instigator)
{
    MSG_FolderInfo *tree = m_master->GetFolderTree();
    XP_ASSERT(tree);
    MSG_FolderInfo *parentFolder = tree->FindParentOf(addedFolder);
    if (parentFolder)
    {
        // at this point the parent folder already contains the child.
		// find the index of the child within the parent's list
        MSG_FolderArray subFolders;
		parentFolder->GetVisibleSubFolders(subFolders);
        int32 newPosition = subFolders.FindIndex (0, addedFolder);

		// Add the folder to the folder pane's view
		AddFolderToView(addedFolder, parentFolder, newPosition);

		// Tell the FE that a new folder was created that they should select
		// notification for imap folders happens in create url exit function
		if (!(addedFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX))
			FE_PaneChanged (this, FALSE, MSG_PaneNotifySelectNewFolder, MSG_VIEWINDEXNONE);
    }
    else
    {
        // this is likely the container? 
        // this was being hit when a UofW server was serving up /u/kmcentee XP_ASSERT(FALSE);
    }
    
	MSG_LinedPane::OnFolderAdded (addedFolder, instigator);
}

void	MSG_FolderPane::PostProcessRemoteCopyAction()
{
    if (m_RemoteCopyState)
    {
    	MsgERR err = m_RemoteCopyState->DoNextAction();
    	if (err)
    	{
    		delete m_RemoteCopyState;
    		m_RemoteCopyState = NULL;
    	}
    }
}


void MSG_FolderPane::AddFoldersToView (MSG_FolderArray &folders, MSG_FolderInfo *parent, int32 newFolderIdx)
{
	// This is a hack to allow creating folders inside the folderPane c-tor. Yuck.
	if (m_folderView.GetSize() == 0)
		return;

	if (parent == NULL)
		parent = GetMaster()->GetFolderTree();

	// If the parent is not already visible, expand any folders we need to so the 
	// newly added children will be visible.
	MSG_ViewIndex parentIdx = GetFolderIndex (parent, TRUE /*expand*/);
		
	// If this is the first child for this parent, the FE needs to
	// redraw the parent line to get the expand/collapse widget
	if ((parent->GetNumSubFolders() == 1) && (parentIdx != MSG_VIEWINDEXNONE))
	{
		StartingUpdate (MSG_NotifyChanged, parentIdx, 1);
		parent->SetFlag (MSG_FOLDER_FLAG_DIRECTORY);
		// tell the view
		ClearFlagForFolder(parent, MSG_FOLDER_FLAG_ELIDED);	// make sure elided bit isn't set.
		EndingUpdate (MSG_NotifyChanged, parentIdx, 1);
	}

	if (parentIdx != MSG_VIEWINDEXNONE && !(m_flagsArray.GetAt(parentIdx) & MSG_FOLDER_FLAG_ELIDED))
	{
		// The new parent may have expanded folders in the folder pane between the parent
		// index and the position of the new child. Therefore, we need to adjust [the index 
		// of the new child relative to its parent] into [an index into the folder pane's view].
		int32 expansionOffset = 0;
		MSG_FolderArray visibleSubFolders;
		parent->GetVisibleSubFolders(visibleSubFolders);

		for (int i = 0; i < newFolderIdx; i++)
		{
			MSG_FolderInfo *folder = visibleSubFolders.GetAt(i);
			uint32 flags = folder->GetFlags();
			// ### dmb - should this use the flag array? 
			if ((flags & MSG_FOLDER_FLAG_DIRECTORY) && !(m_flagsArray.GetAt(parentIdx + i + expansionOffset + 1) & MSG_FOLDER_FLAG_ELIDED))
			{
				MSG_FolderArray array;
				GetExpansionArray (folder, array);
				expansionOffset += array.GetSize();
			}
		}
		expansionOffset += newFolderIdx;
		// Now that we know where the new folder goes in the folder pane, tell
		// the FE to redraw at that index
		if (MSG_VIEWINDEXNONE == GetFolderIndex (folders.GetAt(0)))
		{
			MSG_ViewIndex newIdx = expansionOffset;
			if (parentIdx != MSG_VIEWINDEXNONE)
				newIdx += parentIdx + 1;
			StartingUpdate (MSG_NotifyInsertOrDelete, newIdx, folders.GetSize());
			InsertFlagsArrayAt(newIdx, folders);
			m_folderView.InsertAt (newIdx, &folders);
			EndingUpdate (MSG_NotifyInsertOrDelete, newIdx, folders.GetSize());

		}
	}
	else if (parentIdx != MSG_VIEWINDEXNONE)
	{
		int32 unused;
		ToggleExpansion(parentIdx, &unused);				
	}
	else if (parentIdx == MSG_VIEWINDEXNONE)
	{
		// The new parent may have expanded folders in the folder pane between the parent
		// index and the position of the new child. Therefore, we need to adjust [the index 
		// of the new child relative to its parent] into [an index into the folder pane's view].
		int32 expansionOffset = 0;
		MSG_FolderArray visibleSubFolders;
		parent->GetVisibleSubFolders(visibleSubFolders);

		for (int i = 0; i < newFolderIdx; i++)
		{
			MSG_FolderInfo *folder = visibleSubFolders.GetAt(i);
			uint32 flags = folder->GetFlags();
			// ### dmb - should this use the flag array? 
			if ((flags & MSG_FOLDER_FLAG_DIRECTORY) && !(m_flagsArray.GetAt(parentIdx + i + expansionOffset + 1) & MSG_FOLDER_FLAG_ELIDED))
			{
				MSG_FolderArray array;
				GetExpansionArray (folder, array);
				expansionOffset += array.GetSize();
			}
		}
		expansionOffset += newFolderIdx;
		// this case is for when the root object is the parent...
		StartingUpdate (MSG_NotifyInsertOrDelete, expansionOffset, folders.GetSize());
		InsertFlagsArrayAt(expansionOffset, folders);
		m_folderView.InsertAt (expansionOffset, &folders);
		EndingUpdate (MSG_NotifyInsertOrDelete, expansionOffset, folders.GetSize());

		// add all the expanded children
		for (int index = 0; index < folders.GetSize(); index++)
		{
			MSG_FolderInfo *folder = folders.GetAt(index);
			if (!(folder->GetFlags() & MSG_FOLDER_FLAG_ELIDED))
			{
				MSG_FolderArray array;
				GetExpansionArray (folder, array);
				if (array.GetSize() != 0)
				{
					StartingUpdate (MSG_NotifyInsertOrDelete, ++expansionOffset, array.GetSize());
					InsertFlagsArrayAt(expansionOffset, array);
					m_folderView.InsertAt (expansionOffset, &array);
					EndingUpdate (MSG_NotifyInsertOrDelete, expansionOffset, array.GetSize());
					expansionOffset += folders.GetSize();
				}
			}
		}
	}
}

void MSG_FolderPane::SetFlagForFolder (MSG_FolderInfo *folder, uint32 which)
{
	MSG_ViewIndex viewIndex = GetFolderIndex (folder);
	if (viewIndex != MSG_VIEWINDEXNONE)
	{
		m_flagsArray.SetAt(viewIndex, m_flagsArray.GetAt(viewIndex) | which);
	}
}

void MSG_FolderPane::ClearFlagForFolder(MSG_FolderInfo *folder, uint32 flag)
{
	MSG_ViewIndex viewIndex = GetFolderIndex (folder);
	if (viewIndex != MSG_VIEWINDEXNONE)
	{
		m_flagsArray.SetAt(viewIndex, m_flagsArray.GetAt(viewIndex) & ~flag);
	}
}

void MSG_FolderPane::AddFolderToView (MSG_FolderInfo *folder, MSG_FolderInfo *parent, int32 newPos)
{
	// Just a little convenience routine since not all folder operations have an array
	MSG_FolderArray array;
	array.Add(folder);
	AddFoldersToView (array, parent, newPos);
}


MsgERR MSG_FolderPane::NewFolderWithUI (MSG_ViewIndex *indices, int32 numIndices)
{
    MsgERR status = 0;

	XP_ASSERT (numIndices == 1);	
	MSG_FolderInfo *folder = GetFolderInfo (indices[0]);
	
	if (folder && (folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOINFERIORS))
	{
		FE_Alert(GetContext(), XP_GetString(MK_MSG_PARENT_TARGET_NO_INFERIORS));
		// leave status == 0, since we already annoyed the user
	}
	else if (folder)
	{
		char *name = GetValidNewFolderName (folder, MK_MSG_NEW_FOLDER_CAPTION, folder);

		// If the user didn't click cancel, (name non-null) create the folder
		if (name)
		{
			status = GetMaster()->CreateMailFolder (this, folder, name);
			XP_FREE (name);
		}
	}
	return status;
}



	// remove the folders from the visible folder pane. leave disk storage intact
MsgERR MSG_FolderPane::RemoveFolderFromView(MSG_FolderInfo *doomedFolder)
{
    MSG_FolderArray children;
    GetExpansionArray (doomedFolder, children);
    MSG_ViewIndex doomedIndex = GetFolderIndex(doomedFolder);
	
	MSG_FolderInfo *parentOfDoomed = m_master->GetFolderTree()->FindParentOf (doomedFolder);

	return RemoveIndexedFolderFromView(doomedIndex, children, (doomedIndex == MSG_VIEWINDEXNONE) ? 0 : m_flagsArray.GetAt(doomedIndex), parentOfDoomed);
}

	// remove the indexed folders from the visible folder pane. 
MsgERR MSG_FolderPane::RemoveIndexedFolderFromView(MSG_ViewIndex doomedIndex, MSG_FolderArray &children, uint32 doomedFlags, MSG_FolderInfo *parentOfDoomed)
{
    if (MSG_VIEWINDEXNONE != doomedIndex)
    {
        // total number of changing lines is 'folder' plus any visible children
        // since we're deleting, the count is negative
        int32 countVector = -1; // default for collapsed
        if (!(doomedFlags & MSG_FOLDER_FLAG_ELIDED))
            countVector = -(children.GetSize() + 1);
        StartingUpdate (MSG_NotifyInsertOrDelete, doomedIndex, countVector);
        // remove children, if any and parent expanded, from view
		if (!(doomedFlags & MSG_FOLDER_FLAG_ELIDED))
		{
			m_folderView.RemoveAt (doomedIndex + 1, &children); 
			m_flagsArray.RemoveAt(doomedIndex + 1, children.GetSize());
		}
        // remove 'folder' from view
        m_folderView.RemoveAt (doomedIndex);                
		m_flagsArray.RemoveAt(doomedIndex);
        EndingUpdate (MSG_NotifyInsertOrDelete, doomedIndex, countVector);
    }

    return 0;
}


	// delete the folders and associated disk storage, calls RemoveFolderFromView
MsgERR MSG_FolderPane::DeleteFolder (MSG_FolderInfoMail *folder, XP_Bool performPreflight)
{
    MsgERR status = 0;
    if (performPreflight)	// preflight is not performed to delete non-existant imap folders.
    	status = PreflightDeleteFolder (folder, TRUE /*getUserConfirmation*/);
    	
    if (status != 0)
        return 0; // ###phil not nice to drop this error, but we don't want another alert box.

    if (folder->GetType() == FOLDER_MAIL)
	{
		MSG_FolderInfo *tree = m_master->GetFolderTree();
		XP_ASSERT (tree);
		if (tree)
		{
			// I'm leaving the FindParent split out in case we put in parent pointers,
			// which would make this an O(1) operation
			MSG_FolderInfo *parent = tree->FindParentOf (folder); 
			status = parent->PropagateDelete ((MSG_FolderInfo**) &folder);
		}
	}
    else if (folder->GetType() == FOLDER_IMAPMAIL) 
	{
		char *deleteMailboxURL = CreateImapMailboxDeleteUrl(((MSG_IMAPFolderInfoMail *)folder)->GetHostName(),
																((MSG_IMAPFolderInfoMail *)folder)->GetOnlineName(),
																((MSG_IMAPFolderInfoMail *)folder)->GetOnlineHierarchySeparator());
		if (deleteMailboxURL)
		{
			MSG_UrlQueue *q = MSG_UrlQueue::AddUrlToPane(deleteMailboxURL, NULL, this, NET_SUPER_RELOAD);
			XP_ASSERT(q);
			if (q)
				q->AddInterruptCallback (SafeToSelectInterruptFunction);

			XP_FREE(deleteMailboxURL);
		}
		else
			status = MK_OUT_OF_MEMORY;
	}
    else
	{
		XP_ASSERT(FALSE); // should have been caught by command status
		status = eUNKNOWN;
	}

    
    return status;
}


MsgERR MSG_FolderPane::DeleteVirtualNewsgroup (MSG_FolderInfo *folder)
{
	MSG_FolderInfoNews *virtualGroup = folder->GetNewsFolderInfo();

	XP_ASSERT(virtualGroup);
	XP_ASSERT(virtualGroup->GetFlags() & MSG_FOLDER_FLAG_PROFILE_GROUP);

	// Can't use BuildUrl because a group name in the URL will open a new window
	char *profileUrl = PR_smprintf("%s/dummy?profile/PROFILE DEL %s",
								   virtualGroup->GetHost()->GetURLBase(), virtualGroup->GetName());
	if (profileUrl)
	{
		URL_Struct* urlStruct = NET_CreateURLStruct (profileUrl, NET_DONT_RELOAD);
		if (urlStruct)
			GetURL (urlStruct, FALSE);
		XP_FREE(profileUrl);
	}
	return eSUCCESS;
}


void MSG_FolderPane::OnFolderDeleted(MSG_FolderInfo *folder)
{
	// Here we catch the broadcast message that a folder has been deleted. 
	// In the context of the folder pane, this means we remove the folder
	// from the folder view, since that view element has a stale pointer
	RemoveFolderFromView (folder);

	// Call our parent in case they need to respond to this notification
	//
	// I think it makes sense to send the folderDeleted notification even if
	// the folder isn't in view. This is different from a list notification
	MSG_LinedPane::OnFolderDeleted (folder);
}


MsgERR MSG_FolderPane::TrashFolders (const MSG_ViewIndex *indices, int32 numIndices, XP_Bool noTrash /*=FALSE*/)
{
	MSG_FolderArray srcFolders;
	MsgERR status = MakeMoveFoldersArray (indices, numIndices, &srcFolders);
	if (eSUCCESS == status)
		status = TrashFolders (srcFolders, noTrash);
	return status;
}


MsgERR MSG_FolderPane::TrashFolders (MSG_FolderArray &srcFolders, XP_Bool noTrash /*=FALSE*/)
{
    MSG_FolderInfo *folder = NULL;
    MsgERR status = -1;	// assume error, because there are so many error paths.
	XP_Bool userCancelled = FALSE;

	// If the prefs say not to confirm, pretend the user already has confirmed
    XP_Bool trashFoldersConfirmed = !(GetPrefs()->GetConfirmMoveFoldersToTrash()); 

    for (int i = 0; (i < srcFolders.GetSize()) && (!userCancelled); i++)
    {
        folder = srcFolders.GetAt(i);
        XP_ASSERT (folder);
		int32 flags = folder->GetFlags();

		if ((flags & MSG_FOLDER_FLAG_NEWS_HOST) || folder->GetType() == FOLDER_IMAPSERVERCONTAINER)
		{
			MSG_Host* host;
			int		confirmStrID;
			
			if (flags & MSG_FOLDER_FLAG_NEWS_HOST)
			{
				host = ((MSG_NewsFolderInfoContainer*) folder)->GetHost();
				confirmStrID = MK_MSG_REMOVE_HOST_CONFIRM;
			}
			else
			{
				host = ((MSG_IMAPFolderInfoContainer*) folder)->GetIMAPHost();
				confirmStrID = MK_MSG_REMOVE_IMAP_HOST_CONFIRM;
			}
			char* tmp = PR_smprintf(XP_GetString(confirmStrID),	host->getFullUIName());
			if (tmp) 
			{
				XP_Bool doit = FE_Confirm(GetContext(), tmp);
				XP_FREE(tmp);
				// I don't think we need to preflight this - we need to just do it.
				if (doit /* && PreflightDeleteFolder(folder) == 0 */) 
				{
					status = host->RemoveHost();
#if !defined(XP_WIN32) && !defined(XP_WIN16) //bug# 112472
					RedisplayAll();
#endif
				}
			}
			break;				// Only allow one newshost to be deleted at
								// a time (since it is a background operation).
		}
				

        if (folder->GetType() != FOLDER_MAIL && folder->GetType() != FOLDER_IMAPMAIL && !folder->IsNews()) 
        {
			// Shouldn't have been able to get here, because the menu item should have been disabled.  Right?
			XP_ASSERT(0);		
            FE_Alert (GetContext(), XP_GetString(MK_MSG_CAN_ONLY_DELETE_MAIL_FOLDERS));
            continue;
        }

        if (folder->IsNews())
        {
			XP_Bool isVirtualGroup = folder->TestFlag(MSG_FOLDER_FLAG_PROFILE_GROUP);
            char *vgPrompt = "";
            if (isVirtualGroup)
                vgPrompt = PR_smprintf (XP_GetString(MK_MSG_UNSUBSCRIBE_PROFILE_GROUP), folder->GetName());
            char *unsubPrompt = PR_smprintf (XP_GetString (MK_MSG_UNSUBSCRIBE_GROUP), folder->GetPrettiestName());
            
            if (vgPrompt && unsubPrompt)
            {
                char *wholePrompt = PR_smprintf ("%s%s",vgPrompt, unsubPrompt);
                if (wholePrompt)
                {
                    if (!FE_Confirm (GetContext(), wholePrompt))
                    {
                        XP_FREE(wholePrompt);
                        if (isVirtualGroup)
                            XP_FREE(vgPrompt);
                        XP_FREE(unsubPrompt);
                        continue;
                    }
					else if (isVirtualGroup)
						status = DeleteVirtualNewsgroup (folder);
                    XP_FREE(wholePrompt);
                }
                else
                    continue; // out of memory
                if (isVirtualGroup)
                    XP_FREE(vgPrompt);
                XP_FREE(unsubPrompt);
            }
            else
                continue; // out of memory
        }

        MSG_FolderInfoMail *mailFolder = folder->GetMailFolderInfo();
        if ((mailFolder) && (folder->GetType() == FOLDER_MAIL))
        {
            // Find the Trash folder
            MSG_FolderInfoMail *tree = m_master->GetLocalMailFolderTree();
            XP_ASSERT(tree);
            MSG_FolderInfo *trashFolder = NULL;
            tree->GetFoldersWithFlag (MSG_FOLDER_FLAG_TRASH, &trashFolder, 1);

            // If the folder already lives in the Trash, really delete it,
            // otherwise move it to the Trash folder.
			if (trashFolder != NULL) {
				if (noTrash || trashFolder->IsParentOf(mailFolder))
					status = DeleteFolder (mailFolder);
				else
				{
					// first see if the trash has a folder with the same name
					XP_Bool needUnique = trashFolder->ContainsChildNamed(mailFolder->GetName());
					const char *uniqueName = NULL;
					char *oldName = XP_STRDUP(mailFolder->GetName());
					status = 0;
					if (needUnique)
					{
						// if so, first move the folder to a unique name
			            MSG_FolderInfo *parentOfCurrent = m_master->GetFolderTree()->FindParentOf (mailFolder);
						uniqueName = trashFolder->GenerateUniqueSubfolderName(mailFolder->GetName(), parentOfCurrent);
						
						if (uniqueName)
							status = RenameFolder(mailFolder,uniqueName);
						if (!uniqueName || ((int32) status < 0))
						{
							// if the unique rename fails, simply alert the user and don't trash it
							char *templateString = XP_GetString(MK_MSG_UNIQUE_TRASH_RENAME_FAILED);
							char *alertString = NULL;
							if (templateString) alertString = PR_smprintf(templateString,mailFolder->GetName());
							if (alertString) FE_Alert(GetContext(),alertString);
							FREEIF(alertString);
						}

					}
					// move it to the trash.
					if ((int32) status >= 0)
					{
						MSG_FolderArray tempArray;
						tempArray.Add (folder);
						if (!trashFoldersConfirmed)
						{
							if (FE_Confirm (GetContext(), XP_GetString(MK_MSG_CONFIRM_MOVE_FOLDER_TO_TRASH)))
								status = MoveFolders (tempArray, trashFolder);
							else 
								userCancelled = TRUE;

							trashFoldersConfirmed = TRUE;
						}
						else
							status = MoveFolders (tempArray, trashFolder);
					}
					if (needUnique && (status == 0))
					{
						char *templateString = XP_GetString(MK_MSG_NEEDED_UNIQUE_TRASH_NAME);
						char *alertString = NULL;
						if (templateString) alertString = PR_smprintf(templateString,oldName,uniqueName);
						if (alertString) FE_Alert(GetContext(),alertString);
						FREEIF(alertString);
					}
					FREEIF(oldName);
				}
			}
			else 
				FE_Alert(GetContext(),"Could not locate Trash folder!");
        }
        else if ((mailFolder) && (folder->GetType() == FOLDER_IMAPMAIL) && !folder->DeleteIsMoveToTrash())
        {
			status = DeleteFolder (mailFolder);
        }
        else if ((mailFolder) && (folder->GetType() == FOLDER_IMAPMAIL)) 
		{
			MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
            // Find the IMAP Trash folder
            MSG_FolderInfo *trashFolder = MSG_GetTrashFolderForHost(imapFolder->GetIMAPHost());

            // If the folder already lives in the Trash, really delete it,
            // otherwise move it to the Trash folder.
            if (trashFolder != NULL) {
				if (noTrash || trashFolder->IsParentOf(mailFolder))
					status = DeleteFolder (mailFolder);
				else if (trashFolder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOINFERIORS)
				{
					if (FE_Confirm(GetContext(), XP_GetString(MK_MSG_TRASH_NO_INFERIORS)) )
						status = DeleteFolder (mailFolder);
				}
				else
				{
					// first see if the trash has a folder with the same name
					XP_Bool needUnique = trashFolder->ContainsChildNamed(mailFolder->GetName());
					if (needUnique)
					{
						// if the unique rename fails, simply alert the user and don't trash it
						char *templateString = XP_GetString(MK_MSG_UNIQUE_TRASH_RENAME_FAILED);
						char *alertString = NULL;
						if (templateString) alertString = PR_smprintf(templateString,mailFolder->GetName());
						if (alertString) FE_Alert(GetContext(),alertString);
						FREEIF(alertString);
					}
					else 
					{
						// With the three pane UI, it's sometimes hard to tell which pane has focus, and 
						// sometimes users can delete a folder when they think they're deleting a message.
						MSG_FolderArray tempArray;
						tempArray.Add (folder);
						if (!trashFoldersConfirmed)
						{
							if (FE_Confirm (GetContext(), XP_GetString(MK_MSG_CONFIRM_MOVE_FOLDER_TO_TRASH)))
								status = MoveFolders (tempArray, trashFolder);
							else
								userCancelled = TRUE;
							trashFoldersConfirmed = TRUE; // only ask once
						}
						else
							status = MoveFolders (tempArray, trashFolder);
					}
				}
			}
			else FE_Alert(GetContext(),"Could not locate Trash folder!");
		}
        else if (folder->IsNews())
        {
            MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
			if (newsFolder)
			{
				newsFolder->GetHost()->RemoveGroup (newsFolder);
				status = 0;	// lovely remove group is void
			}
        }
        else
        {
            XP_ASSERT(FALSE);
            continue;
        }
    }

	// Must add this URL after all the IMAP delete URLs (if any) to tell the FE when 
	// it's safe to select a folder (which runs a URL which might interrupt the delete)
	MSG_UrlQueue::AddUrlToPane ("mailbox:?null", SafeToSelectExitFunction, this);

    return status;
}


/*static*/ void MSG_FolderPane::RuleTrackCB (void *cookie, XPPtrArray& rules, 
											XPDWordArray &actions, XP_Bool isDelete)
{
	MWContext *context = ((MSG_FolderPane*) cookie)->GetContext();
	MSG_RuleTrackAction action = dontChange;

	// Ask once what the user wants to do. Asking for every rule is pretty annoying
	if (!isDelete)
	{
		if (FE_Confirm (context, XP_GetString(MK_MSG_TRACK_FOLDER_MOVE)))
			action = trackChange;
		else
			action = dontChange;
	}
	else
	{
		if (FE_Confirm (context, XP_GetString(MK_MSG_TRACK_FOLDER_DEL)))
			action = disableRule;
		else
			action = dontChange;
	}

	// Set the user's choice into the array to apply to each affected rule
    for (int i = 0; i < rules.GetSize(); i++)
		actions.SetAtGrow (i, action);
}


MsgERR MSG_FolderPane::MakeMoveFoldersArray (const MSG_ViewIndex *indices, 
											 int32 numIndices, 
											 MSG_FolderArray *outArray)
{
	// When we move a group of folders, constrain the list of source folders
	// such that we don't try to move a folder whose parent is also in the 
	// list. This allows us to preserve hierarchy as much as possible, and also 
	// to reduce the number of errors we need to handle (e.g. uh-oh, this 
	// folder's gone; I wonder if I moved it already)

	// First, put 'em all in a temporary list
	int i;
	MSG_FolderArray tmpArray;
	for (i = 0; i < numIndices; i++)
		tmpArray.Add(GetFolderInfo(indices[i]));

	// Now sort the list to get the shallowest folders first. Doing this 
	// assures that one pass through the folders will catch all the 
	// parent-child relationships.
	tmpArray.QuickSort (MSG_FolderInfo::CompareFolderDepth);

	// Add elements to the array if they aren't a child of another array element
	for (i = 0; i < tmpArray.GetSize(); i++)
	{
		MSG_FolderInfo *folder = tmpArray.GetAt(i);
		XP_Bool alreadyHaveParent = FALSE;

		for (int j = 0; j < outArray->GetSize(); j++)
		{
			alreadyHaveParent = outArray->GetAt(j)->IsParentOf(folder);
			if (alreadyHaveParent)
				break;
		}

		if (!alreadyHaveParent)
			outArray->Add(folder);
	}

	return eSUCCESS;
}


int MSG_FolderPane::PreflightMoveFolder (MSG_FolderInfo *srcFolder, MSG_FolderInfo *srcParent, MSG_FolderInfo *destFolder)
{
	// can't move categories around.
//	if (srcFolder->IsCategory() && srcFolder->GetType() != FOLDER_CATEGORYCONTAINER)
//		return -1;

    if (srcParent == destFolder || srcFolder == destFolder)
    {
        // Moving folder into same hierarchy level it's already in -- do nothing.
        return -1;
    }
	// can't move catgories, but can move category containers
	if (srcFolder->GetNewsFolderInfo() && srcFolder->GetNewsFolderInfo()->IsCategory() && srcFolder->GetType() != FOLDER_CATEGORYCONTAINER)
	{
		return -1;
	}
	// can't drop onto categories.
	if (destFolder->GetNewsFolderInfo() && destFolder->GetNewsFolderInfo()->IsCategory())
		return -1;

	if (srcFolder->GetFlags() & MSG_FOLDER_FLAG_NEWS_HOST)
	{	
		if (destFolder->TestFlag (MSG_FOLDER_FLAG_TRASH))
			return 0; // dragging news host to trash ==> remove host
		return -1; // can't move news hosts currently
	}
    else if (srcFolder->GetType() == FOLDER_IMAPSERVERCONTAINER)
    {
    	// we were not catching this because the srcFolder and destFolder were of different types
		return (destFolder->GetType() == FOLDER_IMAPSERVERCONTAINER) ? 0 : -1;
    }
    else if (destFolder->GetType() != srcFolder->GetType())
    {
		// Allow unsubscribe via move to Trash
		if (srcFolder->IsNews() && destFolder->GetFlags() & MSG_FOLDER_FLAG_TRASH)
			return 0;
				    
	    if (srcFolder->IsMail())
	    {
	    	if (destFolder->IsNews() || destFolder->GetType() == FOLDER_CONTAINERONLY)
		    	return MK_MSG_NO_MAIL_TO_NEWS;
	    	else
	    		return 0;
	    }
	    else if (destFolder->IsMail())
	    {
	    	if (srcFolder->IsNews() || srcFolder->GetType() == FOLDER_CONTAINERONLY)
		    	return MK_MSG_NO_NEWS_TO_MAIL;
	    	else
	    		return 0;
	    }
		else if (destFolder->GetType()  == FOLDER_IMAPSERVERCONTAINER)
		{
	    	if (srcFolder->IsNews() || srcFolder->GetType() == FOLDER_CONTAINERONLY)
		    	return MK_MSG_NO_NEWS_TO_MAIL;
		}
	    
    }
    else if (destFolder->IsNews())
    {
		if (srcFolder->IsNews())
		{
			MSG_FolderInfoNews *srcGroup = srcFolder->GetNewsFolderInfo();
			MSG_FolderInfoNews *destGroup = destFolder->GetNewsFolderInfo();
			// Newsgroups can only be reordered if they live on the same host
			if (srcGroup->GetHost() != destGroup->GetHost()) 
				return -1; 
		}
		else
			return 0;
    }

	// Don't allow a folder to be moved into one of its own children
	if (srcFolder->IsParentOf (destFolder))
		return MK_MSG_CANT_MOVE_FOLDER_TO_CHILD;
	
	// don't allow a destFolder where the MSG_FOLDER_PREF_IMAPNOINFERIORS pref flag is set
	if (destFolder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOINFERIORS)
		return MK_MSG_MOVE_TARGET_NO_INFERIORS;
	
	
	// Don't allow the user to move their imap inbox
	uint32 flags = srcFolder->GetFlags();
	if ( (flags & MSG_FOLDER_FLAG_INBOX) && (flags & MSG_FOLDER_FLAG_IMAPBOX) )
		return MK_MSG_CANT_MOVE_INBOX;

	return 0;
}

int MSG_FolderPane::PreflightMoveFolderWithUI (MSG_FolderInfo *srcFolder, MSG_FolderInfo *srcParent, MSG_FolderInfo *destFolder)
{
	int err = PreflightMoveFolder (srcFolder, srcParent, destFolder);
	if (0 == err)
	{
		// Since these folders are special based on their name and location,
		// changing the location makes them not special. Make sure the user
		// really wants to do this.
		if (srcFolder->GetDepth() == 2)
		{
			int32 flags = srcFolder->GetFlags();
			if (flags & MSG_FOLDER_FLAG_INBOX || flags & MSG_FOLDER_FLAG_DRAFTS || 
				flags & MSG_FOLDER_FLAG_QUEUE || flags & MSG_FOLDER_FLAG_TRASH ||
				flags & MSG_FOLDER_FLAG_TEMPLATES)
			{
				char *prompt = PR_smprintf (XP_GetString(MK_MSG_CONFIRM_MOVE_MAGIC_FOLDER), 
					srcFolder->GetName(), srcFolder->GetName());
				if (prompt)
				{
					XP_Bool allowMove = FE_Confirm(GetContext(), prompt);
					XP_FREE(prompt);
					if (!allowMove)
						err = -1;
				}
				else
					err = MK_OUT_OF_MEMORY;
			}
		}
	}
	else
	{
		if (err != -1) // -1 is a generic failure -- no specific error string
			FE_Alert (GetContext(), XP_GetString (err));
	}

	return err; 
}


XP_Bool MSG_FolderPane::OneFolderInArrayIsIMAP(MSG_FolderArray &theFolders)
{
	for (int32 i = 0; i < theFolders.GetSize(); i++)
	{
		MSG_FolderInfo *srcFolder = theFolders.GetAt(i);
		if (srcFolder->GetType() == FOLDER_IMAPMAIL)
			return TRUE;
	}
	
	return FALSE;
}


MSG_DragEffect MSG_FolderPane::DragFoldersStatus (
	const MSG_ViewIndex *indices, int32 numIndices,
	MSG_FolderInfo *destFolder, MSG_DragEffect request)
{
	if ((request & MSG_Require_Move) != MSG_Require_Move)
		return MSG_Drag_Not_Allowed;

	// IMAP ACLs - check destination folder
	MSG_IMAPFolderInfoMail *imapFolder = destFolder->GetIMAPFolderInfoMail();
	if (imapFolder)
	{
		if (!imapFolder->GetCanDropFolderIntoThisFolder())
			return MSG_Drag_Not_Allowed;
	}

	for (int i = 0; i < numIndices; i++)
	{
		MSG_FolderInfo *src = GetFolderInfo (indices[i]);
		MSG_FolderInfo *srcParent = GetMaster()->GetFolderTree()->FindParentOf (src);
		if (0 != PreflightMoveFolder (src, srcParent, destFolder))
			return MSG_Drag_Not_Allowed;
	}
	return MSG_Require_Move;
}


MsgERR MSG_FolderPane::MoveFolders (const MSG_ViewIndex *indices, int32 numIndices, MSG_FolderInfo *dest, XP_Bool needExitFunc /*= FALSE*/)
{
	MSG_FolderArray srcFolders;
	MsgERR status = MakeMoveFoldersArray (indices, numIndices, &srcFolders);
	if (eSUCCESS == status)
		status = MoveFolders (srcFolders, dest, needExitFunc);
	return status;
}


MsgERR MSG_FolderPane::MoveFolders (MSG_FolderArray &srcFolders, MSG_FolderInfo *destFolder, XP_Bool needExitFunc /* = FALSE*/)
{
    MsgERR err = 0;
    
	for (int32 i = 0; i < srcFolders.GetSize(); i++)
	{
		MSG_FolderInfo *srcFolder = srcFolders.GetAt(i);
		MSG_FolderInfo *srcParent = GetMaster()->GetFolderTree()->FindParentOf (srcFolder);

		if (0 == PreflightMoveFolderWithUI (srcFolder, srcParent, destFolder))
		{
			// Deleting a newsgroup and dragging it to the trash are synonymous
			if ((srcFolder->IsNews() || srcFolder->GetType() == FOLDER_CONTAINERONLY) && destFolder->GetFlags() & MSG_FOLDER_FLAG_TRASH)
			{
				MSG_ViewIndex srcIdx = m_folderView.FindIndex (0, srcFolder);
				if (srcIdx != MSG_VIEWINDEXNONE)
					err = TrashFolders (&srcIdx, 1);
			}
			else
			{

				// If we're moving into an IMAP folder, start the URL here
				if ( ((destFolder->GetType() == FOLDER_IMAPMAIL) || (destFolder == GetMaster()->GetImapMailFolderTree())) &&
					 (srcFolder->GetType() == FOLDER_IMAPMAIL)  )
				{
					const char *destinationName = "";	// covers promote to root
					char destinationHierarchySeparator = 0;	// covers promote to root
					if (destFolder->GetType() == FOLDER_IMAPMAIL)
					{
						destinationName = ((MSG_IMAPFolderInfoMail *)destFolder)->GetOnlineName();
						destinationHierarchySeparator = ((MSG_IMAPFolderInfoMail *)destFolder)->GetOnlineHierarchySeparator();
					}
					
					// the rename on the server has to happen first imap.h
					char *renameMailboxURL = CreateImapMailboxMoveFolderHierarchyUrl
															(((MSG_IMAPFolderInfoMail *) srcFolder)->GetHostName(),
															((MSG_IMAPFolderInfoMail *) srcFolder)->GetOnlineName(),
															((MSG_IMAPFolderInfoMail *) srcFolder)->GetOnlineHierarchySeparator(),
															destinationName,destinationHierarchySeparator);
					if (renameMailboxURL)
					{
						MSG_UrlQueue *q = MSG_UrlQueue::AddUrlToPane(renameMailboxURL, NULL, this, NET_SUPER_RELOAD);
						XP_ASSERT(q);
						if (q)
							q->AddInterruptCallback (SafeToSelectInterruptFunction);

						XP_FREE(renameMailboxURL);
					}
				}
				else if (srcFolder->GetType() != destFolder->GetType() && srcFolder->IsMail() && (destFolder->IsMail() || destFolder->GetType() == FOLDER_IMAPSERVERCONTAINER))
				{
					// this must be a move on/offline of a mail folder
					m_RemoteCopyState = new MSG_RemoteFolderCopyState(srcFolder, destFolder, this);
					if (m_RemoteCopyState)
					{
						err = m_RemoteCopyState->DoNextAction();
						if (err)
						{
							delete m_RemoteCopyState;
							m_RemoteCopyState = NULL;
						}
					}
				}
				else
				{
					// moving local folders around
    				err = PerformLegalFolderMove(srcFolder, destFolder); // stop on error?
				}
			}
		}
	}

	if (needExitFunc)
	{
		// Must add this URL after all the IMAP delete URLs (if any) to tell the FE when 
		// it's safe to select a folder (which runs a URL which might interrupt the delete)
		MSG_UrlQueue::AddUrlToPane ("mailbox:?null", SafeToSelectExitFunction, this);
	}

	return err;
}


MsgERR MSG_FolderPane::PerformLegalFolderMove(MSG_FolderInfo *srcFolder, MSG_FolderInfo *newParent)
{
    MsgERR err = 0;
    int32 movedPos = 0;
    MSG_FolderInfo *originalParent = GetMaster()->GetFolderTree()->FindParentOf (srcFolder);
    XP_ASSERT(originalParent);

    // Rule changes get written when 'tracker' gets destructed
    MSG_RuleTracker tracker (GetMaster(), RuleTrackCB, this);
	tracker.WatchMoveFolders(&srcFolder, 1);
    if (originalParent)
    {
 		originalParent->RemoveSubFolder (srcFolder);
	   	// If the srcFolder is an IMAP folder then reset its stored online name
    	// This has to happen after MSG_RuleTracker decides if srcFolder was
    	// a filter target and before MSG_RuleTracker gets destructed and writes
    	// the new filter move destination.
    	// Special case newParent being the imap container
    	if (srcFolder->GetType() == FOLDER_IMAPMAIL)
    	{
    		MSG_IMAPFolderInfoMail *srcImapFolder = (MSG_IMAPFolderInfoMail *) srcFolder;
			MSG_IMAPHost *srcHost = srcImapFolder->GetIMAPHost();
			XP_ASSERT(srcHost);
    		if (newParent->GetType() == FOLDER_IMAPMAIL)
    			srcImapFolder->ParentRenamed( ((MSG_IMAPFolderInfoMail *) newParent)->GetOnlineName());
    		else if (newParent->GetType() == FOLDER_IMAPSERVERCONTAINER)
				srcImapFolder->ParentRenamed(srcHost->GetRootNamespacePrefix());
    		else
    			XP_ASSERT(FALSE);	// should be impossible
    	}
    	
        // Move items around in the folder pane to reflect the new ancestry
        // since either the source or the destination might be expanded. 

//		MSG_FolderPane *eachPane;
//		for (eachPane = (MSG_FolderPane *) m_master->FindFirstPaneOfType(MSG_FOLDERPANE); eachPane; 
//				eachPane =  (MSG_FolderPane *) m_master->FindNextPaneOfType(eachPane->GetNextPane(), MSG_FOLDERPANE))
//		{
//			eachPane->UpdateFolderPaneAfterFolderMove(srcFolder, originalParent, newParent, movedPos);
//		}
        // Begin Undo hook
        // **** use StartBatch, EndBatch when moving multiple folders enabled
        UndoManager *undoManager = GetUndoManager();

        if (undoManager && undoManager->GetState () == UndoIdle)
        {
            MoveFolderUndoAction *undoAction = 
                new MoveFolderUndoAction(this, originalParent, srcFolder, newParent);

            if (undoAction)
                undoManager->AddUndoAction(undoAction);
        }
        // End Undo hook
    }
	if (err == eSUCCESS)
	{
		// Mail folder and summary get moved here, unless moving imap server
		m_master->BroadcastFolderDeleted(srcFolder);
		if (srcFolder->GetType() != FOLDER_IMAPSERVERCONTAINER)
		{
			err = newParent->Adopt (srcFolder, &movedPos); 
		}
		else
		{
			movedPos = originalParent->GetSubFolders()->FindIndex (0, newParent);
			if (newParent->GetIMAPFolderInfoContainer())
			{
				MSG_IMAPHostTable *imapHostTable = m_master->GetIMAPHostTable();
				if (imapHostTable)
					imapHostTable->ReorderIMAPHost(srcFolder->GetIMAPFolderInfoContainer()->GetIMAPHost(), newParent->GetIMAPFolderInfoContainer()->GetIMAPHost());
				// put the src folder after the drop target.
				originalParent->GetSubFolders()->InsertAt(movedPos + 1, srcFolder);
			}
			else
				XP_ASSERT(FALSE);
		}
		m_master->BroadcastFolderAdded(srcFolder, NULL);
	}
    else
    {
        char *errorString = XP_GetString (err);
        if (errorString && *errorString)
			FE_Alert (GetContext(), errorString);
    }
	return err;
}

// Undo helper function
MsgERR MSG_FolderPane::UndoMoveFolder (MSG_FolderInfo *srcParent,
                                       MSG_FolderInfo *srcFolder, 
                                       MSG_FolderInfo *destFolder)
{
    MSG_ViewIndex viewIndex;
    MsgERR err = eUNKNOWN;
    int32 movedPos = 0;
    {
        // Don't remove the enclosing bracket....
        MSG_RuleTracker tracker (GetMaster(), RuleTrackCB, this);
        tracker.WatchMoveFolders(&srcFolder, 1);

        // Mail folder and summary get moved here
        err = destFolder->Adopt (srcFolder, &movedPos); 

        // Rule changes get written when 'tracker' gets destructed
    }
    
    if (eSUCCESS == err)
    {
        viewIndex = GetFolderIndex (srcFolder);
        // Move items around in the folder pane to reflect the new ancestry
        // since either the source or the destination might be expanded. 

        MSG_FolderArray itemsToMove;
        GetExpansionArray(srcFolder, itemsToMove); // add the expanded children
        itemsToMove.InsertAt(0, srcFolder); // we will want to move the item itself.

        // Remove srcFolder and any open children of srcFolder
        int32 countVector = 0 - itemsToMove.GetSize();
        if (MSG_VIEWINDEXNONE != viewIndex) {
            StartingUpdate (MSG_NotifyInsertOrDelete, viewIndex, countVector);
			m_folderView.RemoveAt (viewIndex, &itemsToMove);  
			m_flagsArray.RemoveAt(viewIndex, itemsToMove.GetSize());
		}
        srcParent->RemoveSubFolder (srcFolder);
        if (MSG_VIEWINDEXNONE != viewIndex)
            EndingUpdate (MSG_NotifyInsertOrDelete, viewIndex, countVector);

        // If we've just moved out the only child of this parent, tell the FE that 
        // the state of the parent has changed so they redraw the expand/collapse widget
        if (srcParent && srcParent->GetNumSubFolders() == 0)
        {
            int srcParentIdx = m_folderView.FindIndex (0, srcParent);
            if (srcParentIdx != -1)
            {
                StartingUpdate (MSG_NotifyChanged, srcParentIdx, 1);
                EndingUpdate (MSG_NotifyChanged, srcParentIdx, 1);
            }
        }

        // Add srcFolder and its children to the new parent if the parent is expanded
        uint32 destFlags = destFolder->GetFlags();
        int32 destFolderIdx = m_folderView.FindIndex (0, destFolder);
		XP_Bool destElided = m_flagsArray.GetAt(destFolderIdx) & MSG_FOLDER_FLAG_ELIDED;
        if ((destFlags & MSG_FOLDER_FLAG_DIRECTORY) && !destElided)
        {
            XP_ASSERT (destFolderIdx >= 0);
            AddFoldersToView (itemsToMove, destFolder, movedPos);
        }

        // If we've just added the first child of the destination, tell the FE to
        // redraw the dest folder line to get a new expand/collapse widget
        if (destFolder->GetNumSubFolders() == 1 && destFolderIdx != -1) 
        {
            StartingUpdate (MSG_NotifyChanged, destFolderIdx, 1);
            EndingUpdate (MSG_NotifyChanged, destFolderIdx, 1);
        }
    }
    return err;
}


MsgERR MSG_FolderPane::RenameOnlineFolder (MSG_FolderInfo *folder, const char *newName)
{
	char *renameMailboxURL = CreateImapMailboxRenameLeafUrl(((MSG_IMAPFolderInfoMail *)folder)->GetHostName(),
															((MSG_IMAPFolderInfoMail *)folder)->GetOnlineName(),
															((MSG_IMAPFolderInfoMail *)folder)->GetOnlineHierarchySeparator(),
															newName);  // name is assumed to be leafname 
																	// and doesn't affect hierarchy
	if (renameMailboxURL)
	{
		MSG_UrlQueue::AddUrlToPane (renameMailboxURL, NULL, this, NET_SUPER_RELOAD);
	    XP_FREE(renameMailboxURL);
	}

	return eSUCCESS;
}


MsgERR MSG_FolderPane::RenameOfflineFolder (MSG_FolderInfo *folder, const char *newName)
{
    // Begin Undo hook
    UndoManager *undoManager = GetUndoManager();
    if (undoManager && undoManager->GetState() == UndoIdle)
    {
        UndoAction *undoAction = NULL;

		if (folder->GetType() == FOLDER_MAIL) {
            undoAction = new RenameFolderUndoAction(this, folder, folder->GetName(), newName);
		}
		else if (folder->GetType() == FOLDER_IMAPMAIL) {
			undoAction = new IMAPRenameFolderUndoAction(this, folder, 
					 folder->GetName(), newName);  // name is assumed to be leafname 
		}
        
        if (undoAction)
            undoManager->AddUndoAction (undoAction);
    }
    // End Undo hook
    MsgERR returnErr = eFAILURE;
    MSG_RuleTracker tracker (GetMaster(), RuleTrackCB, this);
	tracker.WatchMoveFolders(&folder, 1);

    // I moved the StartingUpdate/EndingUpdate code here
    // because I call this function when I am notified that
    // the imap netlib module has renamed a folder
    // (i.e. the end of an asynch operation - km
    returnErr = folder->Rename (newName);
	if (returnErr == 0)
		// Force UI to update and for various lists to resort.
		RedisplayAll();

    return returnErr;
}


char *MSG_FolderPane::GetValidNewFolderName (MSG_FolderInfo *parent, int captionId, MSG_FolderInfo *folder)
{
    // Loop until we get a unique name, or else the user clicks cancel
    char *name = NULL;
    XP_Bool validName = FALSE;
    while (!validName)
    {
        name = FE_PromptWithCaption (GetContext(), XP_GetString(captionId), XP_GetString(MK_MSG_ENTER_FOLDERNAME), folder->GetPrettiestName());
        if (name)
        {
            if (parent->ContainsChildNamed(name))
            {
                FE_Alert (GetContext(), XP_GetString(MK_MSG_FOLDER_ALREADY_EXISTS));
                XP_FREE(name);
            }
            else
                validName = TRUE;
        }
        else
            validName = TRUE; 
    }
    return name;
}


MsgERR MSG_FolderPane::PreflightRename(MSG_FolderInfo *folder, int depth)
{
    MSG_FolderArray *subFolders = folder->GetSubFolders();
    for (int i = 0; i < subFolders->GetSize(); i++)
    {
        MsgERR err = PreflightRename (subFolders->GetAt(i), depth + 1);
        if (err)
            return err;
    }

	if (depth > 0)	// only need to check children - we handle the top-level case
	{
		// Prevent the user from really deleting any folder which has a thread pane open
		XPPtrArray panes;
		GetMaster()->FindPanesReferringToFolder (folder, &panes);
		if (panes.GetSize() > 0)
		{
			char *prompt = PR_smprintf (XP_GetString (MK_MSG_PANES_OPEN_ON_FOLDER), folder->GetName());
			if (prompt)
			{
				FE_Alert (GetContext(), prompt);
				XP_FREE(prompt);
			}
			return (MsgERR) -1;
		}
	}
	return eSUCCESS;
}

MsgERR MSG_FolderPane::RenameFolder(MSG_FolderInfo *folder, const char *newName)
{
    MsgERR status = 0;
	char *name = NULL;
	XP_Bool mustFreeName = FALSE;

    XP_ASSERT(folder);
	if (!folder)
		return eUNKNOWN;

	// if this folder has sub-folders, preflight the rename
    XPPtrArray *subFolders = folder->GetSubFolders();
	if (subFolders && subFolders->GetSize() > 0)
	{
		MsgERR err = PreflightRename(folder, 0);
		if (err != eSUCCESS)
			return err;
	}
	// If we didn't get a new name on input, ask for one from the user
	if (!newName)
	{
		name = GetValidNewFolderName (GetMaster()->GetFolderTree()->FindParentOf(folder), MK_MSG_RENAME_FOLDER_CAPTION, folder);
		mustFreeName = TRUE;
	}
	else
	{
		name = (char*) newName;
		MSG_FolderInfo *parent = GetMaster()->GetFolderTree()->FindParentOf(folder);
		if (parent && parent->ContainsChildNamed(name))
        {
			// we don't want to free it, since we didn't dup it.
            name = NULL;
			status = eFAILURE;
        }
        else if (XP_FileNameContainsBadChars(name))
        {
            name = NULL;
			status = eFAILURE;
        }
	}

    if (name && (XP_STRLEN(name) > 0) && XP_STRCMP(folder->GetName(), name)) // don't do anything unless names are different
    {
        if (folder->GetType() == FOLDER_MAIL)
            status = RenameOfflineFolder (folder, name);
        else if (folder->GetType() == FOLDER_IMAPMAIL)
			RenameOnlineFolder (folder, name);
        else
		{
			XP_ASSERT(FALSE); // should have been caught by command status
			status = eUNKNOWN;
		}
		if (mustFreeName)
			XP_FREE(name);
    }


    return status;
}

void MSG_FolderPane::GetExpansionArray(MSG_FolderInfo *folder, MSG_FolderArray &array)
{
	MSG_FolderArray *subFolders = folder->GetSubFolders();
    for (int i = 0; i < subFolders->GetSize(); i++)
    {
        MSG_FolderInfo *subFolder = subFolders->GetAt(i);
		if (subFolder->CanBeInFolderPane())
		{
			array.InsertAt(array.GetSize(), subFolder);
			// now, this is not right, unless we're expanding...We can look for this in
			// the view.
			MSG_ViewIndex subFolderIndex = GetFolderIndex(subFolder);
			int32 flags = (subFolderIndex == MSG_VIEWINDEXNONE) ? subFolder->GetFlags() : m_flagsArray.GetAt(subFolderIndex);
			if (!(flags & MSG_FOLDER_FLAG_ELIDED))
				GetExpansionArray(subFolder, array);
		}
    }
}

void MSG_FolderPane::OnToggleExpansion(MSG_FolderInfo* toggledinfo,
									   MSG_ViewIndex line, int32 countVector)
{
	if (countVector == 0) return; // Can this actually happen?
    // this is pretty horrible, but somewhere we need to update counts on
	// expanded folder.
    if (countVector > 0) {
        ScanFolder();
		MSG_FolderInfoNews* newsinfo =
			(MSG_FolderInfoNews*) GetFolderInfo(line);
        if (newsinfo && newsinfo->IsNews()) {
            // OK, we just opened up a newshost.  Go ask that host
            // for all the latest counts.
            MSG_NewsHost* host = newsinfo->GetHost();
			host->SetEverExpanded(TRUE);
            host->SetWantNewTotals(TRUE);
            UpdateNewsCounts(host);
        }
		else if (toggledinfo->GetType() == FOLDER_IMAPMAIL)
		{
			MSG_IMAPFolderInfoMail *imapInfo = toggledinfo->GetIMAPFolderInfoMail();
			if (imapInfo)
			{
				XP_Bool usingSubscription = TRUE;
				MSG_IMAPHost *imapHost = imapInfo->GetIMAPHost();
				if (imapHost)
				{
					usingSubscription = imapHost->GetIsHostUsingSubscription();
				}
				if (!usingSubscription)
				{
					// we need to run a child discovery URL, if we're doing LIST instead of LSUB
					char *url = CreateImapChildDiscoveryUrl(imapInfo->GetHostName(),
						imapInfo->GetOnlineName(), imapInfo->GetOnlineHierarchySeparator());
					if (url)
					{
						URL_Struct *url_s = NET_CreateURLStruct(url, NET_SUPER_RELOAD);
						if (url_s)
						{
							MSG_UrlQueue::AddUrlToPane(url_s, NULL, this);
						}
						XP_FREE(url);
					}
				}
			}
		}
        else if (toggledinfo->GetType() == FOLDER_IMAPSERVERCONTAINER) {
			RefreshIMAPHostFolders(toggledinfo, TRUE);
        }
    } else {
		if ((toggledinfo->GetType() == FOLDER_CONTAINERONLY) ||
		    (toggledinfo->GetType() == FOLDER_IMAPMAIL) ||
		    (toggledinfo->GetType() == FOLDER_IMAPSERVERCONTAINER)) {
			// OK, we just collapsed a newshost or an imap folder.  Stop any background process
			// we may have had going (which probably was us getting counts
			// or new newsgroups on that newshost).
			InterruptContext(FALSE);
		}
	}
}


char *MSG_FolderPane::RefreshIMAPHostFolders(MSG_FolderInfo* hostinfo, XP_Bool runURL)
{
	// Expanding an imap server, update folder list.
	// First, clear the verified bits so that we can discover
	// folders going away, also.
	XPPtrArray *subFolders = hostinfo->GetSubFolders();
	for (int i = 0; i < subFolders->GetSize(); i++)
	{
		MSG_IMAPFolderInfoMail *childinfo = ((MSG_FolderInfo*)subFolders->GetAt(i))->GetIMAPFolderInfoMail();
		if (childinfo)
		{
			childinfo->SetHierarchyIsOnlineVerified(FALSE);
		}
	}

	// Now re-discover the folders
    char * url = CreateImapAllMailboxDiscoveryUrl(hostinfo->GetName());
    if (!runURL)
		return url;
	if (url)
    {
	    URL_Struct *url_struct = NET_CreateURLStruct(url, NET_SUPER_RELOAD);
	    if (url_struct)
		{
			MSG_UrlQueue::AddUrlToPane(url_struct, NULL, this);
		}
	    XP_FREE(url);
	}
	return 0;
}

void MSG_FolderPane::RefreshUpdatedIMAPHosts()
{
    for (MSG_ViewIndex currentIndex = 0; currentIndex < (MSG_ViewIndex) GetNumLines(); currentIndex++)
	{
		MSG_FolderInfo *finfo = GetFolderInfo(currentIndex);
		if (finfo && finfo->GetType() == FOLDER_IMAPSERVERCONTAINER)
		{
			MSG_IMAPFolderInfoContainer *imapinfo = (MSG_IMAPFolderInfoContainer *)finfo;
			if (imapinfo->GetHostNeedsFolderUpdate())
			{
				imapinfo->SetHostNeedsFolderUpdate(FALSE);
				char *url = RefreshIMAPHostFolders(imapinfo, FALSE);
				if (url)
				{
					MSG_UrlQueue::AddUrlToPane(url, NULL, this);
					XP_FREE(url);
				}
			}
		}
	}
}


void MSG_FolderPane::ToggleExpansion(MSG_ViewIndex line, int32* numChanged)
{
    // Build the array of what will change
    MSG_FolderInfo *folder = GetFolderInfo (line);
    MSG_FolderArray array;

    GetExpansionArray(folder, array);

    // Tell the FE to redraw the expand/collapse widget of the parent folder
    StartingUpdate (MSG_NotifyChanged, line, 1);
	uint8 flag = m_flagsArray.GetAt(line);
	if (flag & MSG_FOLDER_FLAG_ELIDED)
		flag &= ~MSG_FOLDER_FLAG_ELIDED;
	else
		flag |= MSG_FOLDER_FLAG_ELIDED;
	m_flagsArray.SetAt(line, flag);
    folder->ToggleFlag (MSG_FOLDER_FLAG_ELIDED);
    EndingUpdate (MSG_NotifyChanged, line, 1);

    // Update the folder pane's array of what's visible
    line++; // all the interesting stuff happens *after* the sel line

    // Compute how many lines to tell the FE about, and in which direction.
    // Negative direction if the folder was just collapsed.
    int32 countVector = array.GetSize();
    if (flag & MSG_FOLDER_FLAG_ELIDED)
        countVector = 0 - countVector;

    StartingUpdate (MSG_NotifyInsertOrDelete, line, countVector);
    if (flag & MSG_FOLDER_FLAG_ELIDED)
	{
        m_folderView.RemoveAt (line, &array);
		m_flagsArray.RemoveAt(line, array.GetSize());
	}
    else
	{
        m_folderView.InsertAt (line, &array);
		InsertFlagsArrayAt(line, array);
	}
    EndingUpdate (MSG_NotifyInsertOrDelete, line, countVector);

	OnToggleExpansion(folder, line, countVector);

    if (numChanged)
        *numChanged = countVector;
}

int32 MSG_FolderPane::ExpansionDelta(MSG_ViewIndex line)
{
    // Build the array of what will change
    MSG_FolderInfo *folder = GetFolderInfo (line);
    MSG_FolderArray array;
    GetExpansionArray(folder, array);

    // If the selected item is collapsed, we will be adding to the view, so
    // the delta is a positive number. If the sel item is expanded, we will 
    // be shrinking the view, so the expansionDelta should be negative
    int32 vector = array.GetSize();
    return m_flagsArray.GetAt(line) & MSG_FOLDER_FLAG_ELIDED ? vector : -vector;
}


int32 MSG_FolderPane::GetNumLines()
{
    return m_folderView.GetSize();
}

MsgERR MSG_FolderPane::UpdateMessageCounts(MSG_ViewIndex *indices, int32 numIndices)
{
	MSG_NewsHost *host = NULL;
	for (MSG_ViewIndex index = 0; index < numIndices; index++)
	{
		MSG_FolderInfo* folderInfo = GetFolderInfo(indices[index]);
		if (folderInfo)
		{
			if (folderInfo->GetType() == FOLDER_CONTAINERONLY)
			{
				MSG_FolderInfoContainer *newsContainer = (MSG_FolderInfoContainer *) folderInfo;
				host = newsContainer->GetHost();
				if (host)
				{
					host->SetWantNewTotals(TRUE);
					break;
				}
			}
			MSG_FolderInfoNews *newsFolder = folderInfo->GetNewsFolderInfo(); 
			if (newsFolder)
			{
				newsFolder->SetWantNewTotals(TRUE);
				host = newsFolder->GetHost();
			}
			// queue up an imap status url to update the counts. Could make this a method on
			// the imapfolderinfomail, but we'd need to pass in a pane.
			MSG_IMAPFolderInfoMail *imapFolder = folderInfo->GetIMAPFolderInfoMail();
			if (imapFolder)
			{
				char *url = CreateIMAPStatusFolderURL(imapFolder->GetHostName(), imapFolder->GetOnlineName(), imapFolder->GetOnlineHierarchySeparator());
				if (url)
				{
					MSG_UrlQueue::AddUrlToPane(url, NULL, this);
					XP_FREE(url);
				}
			}
		}
    }
	if (host)
		UpdateNewsCounts(host);
	return eSUCCESS;
}

/*static*/ void MSG_FolderPane::SafeToSelectInterruptFunction (MSG_UrlQueue * /*queue*/, URL_Struct *URL_s, int status, MWContext *context)
{
	// This is the interrupt function for the URL queue we use for IMAP move/delete folder URLs
	// If the user interrupts such a URL, we need to tell the FE it's ok to select the folder
	// so we don't get caught holding the "not ok to select folder" state forever.
	SafeToSelectExitFunction (URL_s, status, context);
}

/*static*/ void MSG_FolderPane::SafeToSelectExitFunction (URL_Struct *URL_s, int /*status*/, MWContext* /*context*/)
{
	XP_ASSERT(URL_s->msg_pane);
	if (URL_s->msg_pane)
	{
		// Tell the FE that we're done, and they can run another URL if they want. 
		//
		// NB: If you add IMAP code which uses this callback to avoid getting interrupted, make
		// sure that the local folder code also sends the same notification, so the operation looks
		// the same to the FE whether it's IMAP or local.
		FE_PaneChanged (URL_s->msg_pane, FALSE, MSG_PaneNotifySafeToSelectFolder, 0);        
	}

}

MsgERR MSG_FolderPane::CompressFolder(MSG_ViewIndex *indices, int32 numIndices)
{
    MsgERR status = eSUCCESS;

	for (int i = 0; i < numIndices; i++)
	{
		MSG_FolderInfoMail *folder = NULL;

		folder = (MSG_FolderInfoMail*) GetFolderInfo (indices[i]);
		XP_ASSERT (folder);

		status = CompressOneMailFolder(folder);
	}
	return status;
}


MsgERR MSG_FolderPane::AddNewsGroup()
{
    // ###tw   Write me! 

    // This is just a place holder for alpha news... ###tw
    char *groupURL = FE_Prompt(GetContext(), XP_GetString(XP_NEWS_PROMPT_ADD_NEWSGROUP), "");
    if (!groupURL) return 0;        /* User cancelled. */

    MSG_FolderInfo *newsFolder = m_master->AddNewsGroup(groupURL); 

    return newsFolder ? 0 : eUNKNOWN;
}

MsgERR MSG_FolderPane::Undo()
{
    UndoManager *undoManager = GetUndoManager();
    
    if (undoManager)
    {
        UndoStatus status;
        
        status = undoManager->Undo();
        switch (status) {
        case UndoComplete:
        case UndoInProgress:
            return eSUCCESS;
        default:
            return eUNKNOWN;
        }
    }

    return 0;
}


MsgERR MSG_FolderPane::Redo()
{
    UndoManager *undoManager = GetUndoManager();
    
    if (undoManager)
    {
        UndoStatus status;
        
        status = undoManager->Redo();
        switch (status) {
        case UndoComplete:
        case UndoInProgress:
            return eSUCCESS;
        default:
            return eUNKNOWN;
        }
    }

    return 0;
}

MsgERR MSG_FolderPane::MarkAllMessagesRead(MSG_ViewIndex* indices, int32 numIndices)
{
    MSG_FolderInfo *folder;
	for (MSG_ViewIndex i = 0; i < numIndices; i++)
	{
		folder = GetFolderInfo(indices[i]);
		if (folder)	// do a deep mark read for category containers.
			folder->MarkAllRead(GetContext(), folder->GetType() == FOLDER_CATEGORYCONTAINER);
	}
    return 0;
}


XP_Bool MSG_FolderPane::CanUndo()
{
    UndoManager *undoManager = GetUndoManager();

    if (undoManager)
        return undoManager->CanUndo();

    return FALSE;
}


XP_Bool MSG_FolderPane::CanRedo()
{
    UndoManager *undoManager = GetUndoManager();

    if (undoManager)
        return undoManager->CanRedo();

    return FALSE;
}


MsgERR MSG_FolderPane::DoCommand(MSG_CommandType command, 
                                 MSG_ViewIndex* indices, 
                                 int32 numIndices)
{
    MsgERR status = 0;

# define MSG_ASSERT_MAIL() \
    XP_ASSERT(IndicesAreMail(indices,numIndices)); \
    if (!IndicesAreMail(indices,numIndices)) break
# define MSG_ASSERT_NEWS() \
    XP_ASSERT(IndicesAreNews(indices,numIndices)); \
    if (!IndicesAreNews(indices,numIndices)) break
# define MSG_ASSERT_NOT_NEWS() \
    XP_ASSERT(!IndicesAreNews(indices,numIndices)); \
    if (IndicesAreNews(indices,numIndices)) break
# define MSG_ASSERT_MAIL_OR_NEWS() ; // No other possibilities right now,
                                     // so nothing to assert!

    InterruptContext(FALSE);

    //###tw  DisableUpdates();

    switch (command) {

    /* FILE MENU
       =========
    */
	case MSG_GetNewMail:
		status = GetNewMail(this, (numIndices) ? GetFolderInfo(indices[0]) : 0);
		break;
    case MSG_OpenFolder:
        MSG_ASSERT_MAIL();
        status = OpenFolder();
        break;
    case MSG_NewFolder:
        MSG_ASSERT_NOT_NEWS();  // IMAP container ok
        status = NewFolderWithUI(indices, numIndices);
        break;
    case MSG_CompressFolder:
        MSG_ASSERT_MAIL();
        status = CompressFolder(indices, numIndices);
        break;
    case MSG_AddNewsGroup:
//      MSG_ASSERT_NEWS();  should just use default news server
        status = AddNewsGroup();
        break;
	case MSG_UpdateMessageCount:
		status = UpdateMessageCounts(indices, numIndices);
		break;
    case MSG_Print:
        XP_ASSERT(0);
        status = eUNKNOWN;
        break;
    case MSG_NewNewsgroup:
		{
			MSG_FolderInfo *folder = GetFolderInfo(indices[0]);
			XP_ASSERT(NewNewsgroupStatus(folder));
			status = NewNewsgroup (folder, FOLDER_CATEGORYCONTAINER == folder->GetType());
		}
        break;

    /* EDIT MENU
       =========
    */
    case MSG_Undo:
        MSG_ASSERT_MAIL_OR_NEWS();
        status = Undo();
        break;
    case MSG_Redo:
        MSG_ASSERT_MAIL_OR_NEWS();
        status = Redo();
        break;
    case MSG_Unsubscribe:
		// should be MSG_Delete but winfe is defeating me at the moment
    case MSG_DeleteFolder:
	case MSG_DeleteNoTrash:
        MSG_ASSERT_MAIL_OR_NEWS();
        status = TrashFolders(indices, numIndices, (command == MSG_DeleteNoTrash));
        break;
	case MSG_ManageMailAccount:
        MSG_ASSERT_MAIL();
		status = ManageMailAccount((numIndices) ? GetFolderInfo(indices[0]) : 0);
		break;
	case MSG_ModerateNewsgroup:
		status = ModerateNewsgroup(GetFolderInfo(indices[0]));
		break;

    /* VIEW/SORT MENUS
       ===============
    */

    /* MESSAGE MENU
       ============
    */
    case MSG_PostNew:
        MSG_ASSERT_NEWS();
        status = ComposeNewsMessage(GetFolderInfo (indices[0]));
        break;

    /* GO MENU
       =======
    */
    case MSG_MarkAllRead:
        MSG_ASSERT_MAIL_OR_NEWS();
        status = MarkAllMessagesRead(indices, numIndices);
        break;

    /* FOLDER MENU
       ===========
    */
    case MSG_DoRenameFolder:
        MSG_ASSERT_MAIL();
        status = RenameFolder (GetFolderInfo (indices[0]));
        break;

    /* OPTIONS MENU
       ============
    */

	case MSG_EmptyTrash:
		status = EmptyTrash((numIndices) ? GetFolderInfo(indices[0]) : 0);
		break;
    default:
        status = MSG_LinedPane::DoCommand(command, indices, numIndices);
        break;
  }


# undef MSG_ASSERT_MAIL
# undef MSG_ASSERT_NEWS
# undef MSG_ASSERT_MAIL_OR_NEWS

    //###tw  EnableUpdates();

    return status;
}


MsgERR
MSG_FolderPane::GetCommandStatus(MSG_CommandType command,
								 const MSG_ViewIndex* indices,
								 int32 numindices,
								 XP_Bool *selectable_pP,
								 MSG_COMMAND_CHECK_STATE *selected_pP,
								 const char **display_stringP,
								 XP_Bool *plural_pP)
{
	const char *display_string = 0;
	XP_Bool plural_p = FALSE;
	XP_Bool selectable_p = TRUE;
	XP_Bool selected_p = FALSE;
	XP_Bool selected_used_p = FALSE;
	XP_Bool selected_newsgroups_p = FALSE;
	XP_Bool folder_selected_p;
	XP_Bool news_p = IndicesAreNews (indices, numindices);
	XP_Bool mail_p = IndicesAreMail (indices, numindices);

	//###phil not if it's a container XP_ASSERT(news_p || mail_p && !(news_p && mail_p)); // Amazing paranoia.

	folder_selected_p = (numindices > 0);
	if (news_p && folder_selected_p) 
	{
		selected_newsgroups_p = TRUE; // ###tw  Should figure out if anything
									  // selected is actually a newsgroup.
	}

	switch (command) {

	/* FILE MENU
	   =========
	*/
	case MSG_OpenFolder:
		display_string = XP_GetString(MK_MSG_OPEN_FOLDER2);
		selectable_p = mail_p;
		break;
	case MSG_NewFolder:
		display_string = XP_GetString(MK_MSG_NEW_FOLDER);
		selectable_p = numindices == 1 && GetFolderInfo(indices[0])->CanCreateChildren();
		break;
	case MSG_CompressFolder:
		display_string = XP_GetString(MK_MSG_COMPRESS_FOLDER);
		// multiple compresses are done with a url queue
		selectable_p = mail_p && folder_selected_p;
		break;
	case MSG_AddNewsGroup:
		display_string = XP_GetString(MK_MSG_ADD_NEWS_GROUP);
		// selectable_p = TRUE; /*news_p needs to work for news container.*/;		/* ###tw && context->msgframe->data.news.current_host != NULL; */
		selectable_p = GetMaster()->GetHostTable() != NULL;
		break;
	case MSG_Unsubscribe:
		selectable_p = news_p && numindices >= 1;	// is last check redundant?
		display_string = XP_GetString(MK_MSG_UNSUBSCRIBE);
		break;
	case MSG_NewNewsgroup:
		display_string = XP_GetString(MK_MSG_NEW_NEWSGROUP);
		selectable_p = numindices > 0 && NewNewsgroupStatus (GetFolderInfo(indices[0]));
		break;

	/* EDIT MENU
	   =========
	*/
	case MSG_Undo:
		display_string = XP_GetString(MK_MSG_UNDO);
		selectable_p = CanUndo();
		break;
	case MSG_Redo:
		display_string = XP_GetString(MK_MSG_REDO);
		selectable_p = CanRedo();
		break;
	case MSG_DeleteFolder:	// should be MSG_Delete
		{
		XP_Bool allSelectionsAreDeletable = TRUE;
		for (int i = 0; i < numindices; i++)
		{
			MSG_FolderInfo *f = m_folderView.GetAt(indices[i]);
			if (!f->IsDeletable() ||
				((f->GetFlags() & MSG_FOLDER_FLAG_NEWS_HOST) && numindices>1))
			{
				allSelectionsAreDeletable = FALSE;
				break;
			}
		}

		display_string = XP_GetString(MK_MSG_DELETE_FOLDER);
		selectable_p = folder_selected_p && allSelectionsAreDeletable;
		break;
		}

	case MSG_ManageMailAccount:
		display_string = XP_GetString(MK_MSG_MANAGE_MAIL_ACCOUNT);
		selectable_p = mail_p && numindices > 0 && GetFolderInfo(indices[0])->HaveAdminUrl(MSG_AdminServer);
		break;
	case MSG_ModerateNewsgroup:
		display_string = XP_GetString (MK_MSG_MODERATE_NEWSGROUP);
		selectable_p = (numindices > 0 && ModerateNewsgroupStatus(GetFolderInfo(indices[0])));
		break;

	case MSG_UpdateMessageCount:
		display_string = XP_GetString(MK_MSG_UPDATE_MSG_COUNTS);
		selectable_p = news_p || AnyIndicesAreIMAPMail (indices, numindices);
		break;
	/* VIEW/SORT MENUS
	   ===============
	*/

	/* MESSAGE MENU
	   ============
	*/
	case MSG_PostNew:
	display_string = XP_GetString(MK_MSG_NEW_NEWS_MESSAGE);
	// we're currently only supporting posting to one newsgroup at a time.
	selectable_p = news_p && numindices == 1 && GetFolderInfo(indices[0])->AllowsPosting();
	break;

	/* GO MENU
	   =======
	*/
	case MSG_MarkAllRead:
		display_string = XP_GetString(MK_MSG_MARK_ALL_READ);
		selectable_p = news_p && folder_selected_p && selected_newsgroups_p;
		break;

	/* FOLDER MENU 
	   ===========
	*/
	case MSG_DoRenameFolder:
		display_string = XP_GetString (MK_MSG_RENAME_FOLDER);
		selectable_p = mail_p && folder_selected_p && numindices == 1;
		if (selectable_p && !GetFolderInfo(indices[0])->CanBeRenamed ())
			selectable_p = FALSE;
		break;

	/* OPTIONS MENU
	   ============
	*/

	default:
		return MSG_Pane::GetCommandStatus(command, indices, numindices,
									  selectable_pP, selected_pP,
									  display_stringP, plural_pP);
  }

    if (selectable_pP)
        *selectable_pP = selectable_p;
    if (selected_pP)
    {
        if (selected_used_p)
        {
            if (selected_p)
                *selected_pP = MSG_Checked;
            else
                *selected_pP = MSG_Unchecked;
        }
        else
        {
            *selected_pP = MSG_NotUsed;
        }
    }
    if (display_stringP)
        *display_stringP = display_string;
    if (plural_pP)
        *plural_pP = plural_p;

    return 0;
}


XP_Bool MSG_FolderPane::IndicesAreNews (const MSG_ViewIndex *indices, int32 numIndices)
{
    for (int i = 0; i < numIndices; i++)
    {
        MSG_ViewIndex index = indices[i];
        if (!IsValidIndex(index))
            return FALSE;
        MSG_FolderInfo *folder = m_folderView.GetAt (index);
        if (!folder->IsNews())
            return FALSE;
    }

    return TRUE;
}


XP_Bool MSG_FolderPane::IndicesAreMail (const MSG_ViewIndex *indices, int32 numIndices)
{
    for (int i = 0; i < numIndices; i++)
    {
        MSG_ViewIndex index = indices[i];
        if (!IsValidIndex(index))
            return FALSE;
        MSG_FolderInfo *folder = m_folderView.GetAt(index);
        if (!folder->IsMail())
            return FALSE;
    }
    return TRUE;
}


XP_Bool MSG_FolderPane::AnyIndicesAreIMAPMail (const MSG_ViewIndex *indices, int32 numIndices)
{
    for (int i = 0; i < numIndices; i++)
    {
        MSG_ViewIndex index = indices[i];
        if (!IsValidIndex(index))
            continue;
        MSG_FolderInfo *folder = m_folderView.GetAt(index);
        if (folder->IsMail() && folder->GetType() == FOLDER_IMAPMAIL)
            return TRUE;
    }
    return FALSE;
}


MsgERR MSG_FolderPane::CheckForNew(MSG_NewsHost* host)
{
	XP_ASSERT(host);
	if (!host) return eUNKNOWN;
	m_numNewGroups = 0;
	return MSG_Pane::CheckForNew(host);
}


void MSG_FolderPane::CheckForNewDone(URL_Struct* url_struct, int status,
									 MWContext* context)
{
	MSG_NewsHost* host = m_hostCheckingForNew;
	XP_ASSERT(host);
	if (!host) return;
	MSG_Pane::CheckForNewDone(url_struct, status, context);
}



void MSG_FolderPane::UpdateNewsCountsDone(int status)
{
	if (status < 0) return;
	// See if we have a newshost opened that we haven't checked for new groups
	// on yet.  If so, do it.
	msg_HostTable* table = m_master->GetHostTable();
	XP_ASSERT(table);
	for (int32 i=0 ; i<table->getNumHosts() ; i++) {
		MSG_NewsHost* host = table->getHost(i);
		XP_ASSERT(host);
		if (!host) continue;
		if (host->GetEverExpanded() && !host->GetCheckedForNew() &&
			   host->getLastUpdate() > 0) {
			host->SetCheckedForNew(TRUE);
			CheckForNew(host);
			return;
		}
	}
}


MSG_RemoteFolderCopyState::MSG_RemoteFolderCopyState(MSG_FolderInfo *sourceTree, MSG_FolderInfo *destinationTree, MSG_FolderPane *urlPane)
{
	m_sourceTree = sourceTree;
	m_destinationTree = destinationTree;
	
	m_currentSourceNode = m_currentDestinationParent = NULL;
	
	m_urlPane = urlPane;
	m_currentTask = kMakingMailbox;
	m_copySourceDB = NULL;
}

MSG_RemoteFolderCopyState::~MSG_RemoteFolderCopyState()
{
	if (m_copySourceDB)
		m_copySourceDB->Close();
}

void MSG_RemoteFolderCopyState::SetNextSourceFromParent(MSG_FolderInfo *parent, MSG_FolderInfo *child)
{
	if (parent && !parent->IsParentOf(m_sourceTree))	// prevent looking at the parent of m_sourceTree
	{
		XPPtrArray *children = parent->GetSubFolders();
		int childIndex = children->FindIndex(0, child);

		if (++childIndex < parent->GetNumSubFolders())
		{
			m_currentSourceNode = parent->GetSubFolder(childIndex);
		}
		else if (parent != m_sourceTree)
		{
			m_currentDestinationParent = m_destinationTree->FindParentOf(m_currentDestinationParent);
			SetNextSourceFromParent(m_sourceTree->FindParentOf(parent), parent);
		}
		else
			m_currentSourceNode = NULL;	// we are finished
	}
	else
		m_currentSourceNode = NULL;	// we are finished
}

void MSG_RemoteFolderCopyState::SetNextSourceNode()
{
	if (m_currentSourceNode->HasSubFolders())
	{
		// imap mailbox names are stored unescaped
		char *unescapedName = XP_STRDUP(m_currentSourceNode->GetName());
		if (unescapedName)
		{
			m_currentDestinationParent = m_currentDestinationParent->FindChildNamed(NET_UnEscape(unescapedName));
			if (m_currentDestinationParent)
				m_currentSourceNode = m_currentSourceNode->GetSubFolder(0);
			else
				m_currentSourceNode = NULL;	// will stop this copy operation
			XP_FREE(unescapedName);
		}
		else
			m_currentSourceNode = NULL;	// will stop this copy operation
	}
	else
	{
		// we have no children, kick up to my parent
		//m_currentDestinationParent = m_currentDestinationParent->FindParentOf(m_currentDestinationParent);
		SetNextSourceFromParent(m_sourceTree->FindParentOf(m_currentSourceNode), m_currentSourceNode);
	}
}


MsgERR MSG_RemoteFolderCopyState::MakeCurrentMailbox()
{
	MsgERR returnCode = m_urlPane->GetMaster()->CreateMailFolder(m_urlPane, m_currentDestinationParent, m_currentSourceNode->GetName());
	if ( (returnCode == 0) && (m_sourceTree->GetType() == FOLDER_IMAPMAIL))
		returnCode = DoNextAction();
	
	return returnCode;
}
	

/* static */ void MSG_FolderPane::RemoteFolderCopySelectCompleteExitFunction(URL_Struct *URL_s, int /*status*/, MWContext* /*window_id*/)
{
	if (URL_s->msg_pane)
	{
		MSG_Pane *urlPane = (URL_s->msg_pane->GetParentPane() ? URL_s->msg_pane->GetParentPane() : URL_s->msg_pane);
		if (urlPane->GetPaneType() == MSG_FOLDERPANE)
		{
			MSG_FolderPane *folderPane = (MSG_FolderPane *) urlPane;
			if (folderPane->m_RemoteCopyState)
				folderPane->m_RemoteCopyState->DoNextAction();
		}
	}
}

MsgERR MSG_RemoteFolderCopyState::DoUpdateCurrentMailbox()
{
	MsgERR returnCode = eSUCCESS;
	XP_ASSERT(m_currentSourceNode);	
	MSG_IMAPFolderInfoMail *srcMailFolder = m_currentSourceNode->GetIMAPFolderInfoMail();
	if (srcMailFolder)
	{
		srcMailFolder->StartUpdateOfNewlySelectedFolder(m_urlPane, FALSE, NULL, NULL, FALSE, FALSE,
			MSG_FolderPane::RemoteFolderCopySelectCompleteExitFunction);
															  
	}
	else
	{
		// must be local folder, no need to select
		DoNextAction();
	}
	return returnCode;
}

MsgERR MSG_RemoteFolderCopyState::DoCurrentCopy()
{
	MsgERR returnCode = eFAILURE;
	XP_ASSERT(m_currentSourceNode);
	
	// imap mailbox names are stored unescaped
	char *unescapedName = XP_STRDUP(m_currentSourceNode->GetName());
	if (unescapedName)
	{
		MSG_FolderInfo *copyDestination = m_currentDestinationParent->FindChildNamed(NET_UnEscape(unescapedName));
		if (copyDestination)
		{
			if (m_currentSourceNode->GetTotalMessages())
			{
				MSG_FolderInfoMail *srcMailFolder = m_currentSourceNode->GetMailFolderInfo();
				if (srcMailFolder)
				{
					DBFolderInfo *grpInfo = NULL;
					if (srcMailFolder->GetDBFolderInfoAndDB(&grpInfo, &m_copySourceDB) == 0)
					{
						IDArray *allMsgs = new IDArray;
						if (allMsgs)
						{
							m_copySourceDB->ListAllIds(*allMsgs);
							srcMailFolder->StartAsyncCopyMessagesInto(copyDestination, 
																		m_urlPane,
																		m_copySourceDB,
																		allMsgs,
																		allMsgs->GetSize(),
																		m_urlPane->GetContext(),
																		NULL, FALSE);
							returnCode = 0;
						}
					}
				}
			}
			else
				returnCode = DoNextAction();	// nothing to copy here
		}
		XP_FREE(unescapedName);
	}
	return returnCode;
}



MsgERR MSG_RemoteFolderCopyState::DoNextAction()
{
	MsgERR returnCode = 0;
	
	if (!m_currentSourceNode)
	{
		// this is the first call for this copy!
		m_currentTask = kMakingMailbox;
		m_currentSourceNode = m_sourceTree;
		m_currentDestinationParent = m_destinationTree;
	}
	else
	{
		if (m_currentTask == kCopyingMessages)
		{
			if (m_copySourceDB)
			{
				// we finished a copy, close its source DB
				m_copySourceDB->Close();
				m_copySourceDB = NULL;
			}
			m_currentTask = kMakingMailbox;
			SetNextSourceNode();
		}
		else if (m_currentTask == kMakingMailbox)
			m_currentTask = kUpdatingMailbox;
		else
			m_currentTask = kCopyingMessages;
	}
	
	if (m_currentSourceNode)
	{
		if (m_currentTask == kMakingMailbox)
			returnCode = MakeCurrentMailbox();
		else if (m_currentTask == kUpdatingMailbox)
			returnCode = DoUpdateCurrentMailbox();
		else
			returnCode = DoCurrentCopy();
	}
	else
		returnCode = eFAILURE;	// finito
	
	return returnCode;
}



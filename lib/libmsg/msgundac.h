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
/*  msgundac.h --- internal defs for msg undo actions
 */

#ifndef MsgUndoActions_H
#define MsgUndoActions_H

#include "msgundmg.h"
#include "msgfinfo.h"
#include "chngntfy.h"
#include "mailhdr.h"


class MarkMessageUndoAction : public UndoAction
{	
public:
	MarkMessageUndoAction (MSG_Pane *pane,
						   MSG_CommandType command, 
						   MSG_ViewIndex *indices,
						   int32 numIndices,
						   MSG_FolderInfo *folder);
    MarkMessageUndoAction (MSG_Pane *pane,
						   MSG_CommandType command,
						   MSG_FolderInfo *folder);
	virtual ~MarkMessageUndoAction();

	virtual UndoStatus DoUndo();
	virtual XP_Bool HasFolder(MSG_FolderInfo *folder);
    void AddKey(MessageKey key);
	
protected:
	// helper
	MSG_CommandType GetUndoMarkCommand();

protected:
	MSG_Pane *m_pane;
	MSG_CommandType m_command;
    IDArray m_keyArray;
	MSG_FolderInfo* m_folder;
};

class OfflineIMAPUndoAction;

class UndoIMAPChangeListener : public ChangeListener {
public:
	UndoIMAPChangeListener(OfflineIMAPUndoAction *action);
	virtual ~UndoIMAPChangeListener();

	virtual void OnAnnouncerGoingAway(ChangeAnnouncer * /*instigator*/) ;

protected:
  OfflineIMAPUndoAction *m_undoAction;
};


class OfflineIMAPUndoAction : public UndoAction
{	
public:
	friend class UndoIMAPChangeListener;
	OfflineIMAPUndoAction (MSG_Pane *pane, MSG_FolderInfo* folder,
							MessageKey dbKey, int32 opType,
							MSG_FolderInfo *srcFolder, MSG_FolderInfo *dstFolder,
							imapMessageFlagsType flags, MailMessageHdr *hdr, XP_Bool addFlag = FALSE);
	virtual ~OfflineIMAPUndoAction();
	virtual XP_Bool HasFolder(MSG_FolderInfo *folder);
	virtual UndoStatus DoUndo();

protected:
	MSG_Pane *m_pane;
	MessageKey m_dbKey;
	ImapMailDB *m_db;
	MSG_IMAPFolderInfoMail *m_folder;
	MSG_IMAPFolderInfoMail *m_srcFolder;
	MSG_IMAPFolderInfoMail *m_dstFolder;
	int32 m_opType;
	imapMessageFlagsType m_flags;
	MailMessageHdr *m_header;
	XP_Bool m_addFlags;
	UndoIMAPChangeListener *m_changeListener;
};


class UndoMarkChangeListener : public ChangeListener {
public:
  UndoMarkChangeListener(MSG_Pane *pane,
						 MSG_FolderInfo *folderInfo,
						 MSG_CommandType command);
  virtual ~UndoMarkChangeListener();

  virtual void OnKeyChange(MessageKey keyChanged, int32 flags, 
						   ChangeListener *instigator);

protected:
  MSG_Pane *m_pane;
  MSG_FolderInfo *m_folderInfo;
  MSG_CommandType m_command;
  MarkMessageUndoAction *m_undoAction;
};


class MoveCopyMessagesUndoAction : public UndoAction
{
public:
	MoveCopyMessagesUndoAction(MSG_FolderInfo *srcFolder,
							   MSG_FolderInfo *dstFolder,
							   XP_Bool isMove,
							   MSG_Pane *pane,
							   MessageKey prevKeyToLoad,
							   MessageKey nextKeyToLoad);
	virtual ~MoveCopyMessagesUndoAction();

	virtual UndoStatus DoUndo();
	virtual XP_Bool HasFolder(MSG_FolderInfo *folder);
	virtual UndoStatus UndoPreExit();

	// helper methods
	void AddDstKey(MessageKey key);
	void AddSrcKey(MessageKey key);
	UndoStatus ReCreateMailDB(MSG_FolderInfo *folder);

protected:
	MSG_Pane *m_pane;
	MSG_FolderInfo *m_srcFolder;
	MSG_FolderInfo *m_dstFolder;
	XP_Bool m_isMove;
	MessageKey m_prevKeyToLoad;
	MessageKey m_nextKeyToLoad;
    IDArray m_srcArray;
    IDArray m_dstArray;
	MailDB *m_mailDB;
};

class MoveFolderUndoAction : public UndoAction
{
public:
	MoveFolderUndoAction(MSG_Pane *pane,
						 MSG_FolderInfo *srcParent,
						 MSG_FolderInfo *srcFolder,
						 MSG_FolderInfo *dstFolder);
	virtual ~MoveFolderUndoAction();

	virtual UndoStatus DoUndo();
	virtual XP_Bool HasFolder(MSG_FolderInfo *folder);

protected:
	MSG_Pane *m_pane;
	MSG_FolderInfo *m_srcParent;
	MSG_FolderInfo *m_srcFolder;
	MSG_FolderInfo *m_dstFolder;
};

class RenameFolderUndoAction : public UndoAction
{
public:
	RenameFolderUndoAction (MSG_Pane *pane,
							MSG_FolderInfo *folder,
							const char *oldName,
							const char *newName);
	virtual ~RenameFolderUndoAction();

	virtual UndoStatus DoUndo();
	virtual XP_Bool HasFolder(MSG_FolderInfo *folder);

protected:
	MSG_Pane *m_pane;
	MSG_FolderInfo *m_folder;
	char *m_oldName;
	char *m_newName;
};


#if (1)

class IMAPRenameFolderUndoAction : public UndoAction
{
public:
	IMAPRenameFolderUndoAction (MSG_Pane *pane,
								MSG_FolderInfo *folder,
								const char *oldName,
								const char *newName);
	virtual ~IMAPRenameFolderUndoAction();

	virtual UndoStatus DoUndo();
	virtual XP_Bool HasFolder(MSG_FolderInfo *folder);
	virtual UndoStatus UndoPreExit();

protected:
	MSG_Pane *m_pane;
	MSG_FolderInfo *m_folder;
	char *m_oldName;
	char *m_newName;
};

class IMAPMoveCopyMessagesUndoAction : public UndoAction
{
public:
	IMAPMoveCopyMessagesUndoAction(MSG_Pane *pane,
							   MSG_FolderInfo *srcFolder,
							   MSG_FolderInfo *dstFolder,
							   XP_Bool isMove,
							   MessageKey prevKeyToLoad,
							   MessageKey nextKeyToLoad);
	virtual ~IMAPMoveCopyMessagesUndoAction();

	virtual UndoStatus DoUndo();
	virtual XP_Bool HasFolder(MSG_FolderInfo *folder);
	virtual UndoStatus UndoPreExit();

	// helper functions
	void AddKey(MessageKey key);

protected:
    IDArray m_keyArray;
	MessageKey m_prevKeyToLoad;
	MessageKey m_nextKeyToLoad;
	MSG_Pane *m_pane;
	MSG_FolderInfo *m_srcFolder;
	MSG_FolderInfo *m_dstFolder;
    XP_Bool m_isMove;
};

#endif


#endif



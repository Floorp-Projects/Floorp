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

#ifndef _MsgTPane_H_
#define _MsgTPane_H_

#include "msglpane.h"
#include "msg.h"
#include "chngntfy.h"

class MSG_FolderInfo;
class MSG_FolderPane;
class MSG_MessagePane;
class MessageDB;
class MessageDBView;
class MSG_ThreadPane;

#include "msgdbvw.h"

class ThreadPaneListener : public PaneListener
{
public:
	ThreadPaneListener(MSG_Pane *pane);
	virtual ~ThreadPaneListener();
protected:
};

class MSG_ThreadPane : public MSG_LinedPane 
{
	friend class ThreadPaneListener;
public:

	MSG_ThreadPane(MWContext* context, MSG_Master* master);
	virtual ~MSG_ThreadPane();

	virtual MSG_PaneType GetPaneType();

	virtual void StartingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num);
	virtual void EndingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num);
	virtual MessageDBView *GetMsgView() {return m_view;}
	virtual void SetMsgView(MessageDBView *view) {m_view = view;}

	ViewType GetViewTypeFromCommand(MSG_CommandType command);
	MsgERR					SwitchViewType(MSG_CommandType command);

	virtual MsgERR DoCommand(MSG_CommandType command,
						   MSG_ViewIndex* indices, int32 numindices);

	virtual MsgERR GetCommandStatus(MSG_CommandType command,
								  const MSG_ViewIndex* indices, int32 numindices,
								  XP_Bool *selectable_p,
								  MSG_COMMAND_CHECK_STATE *selected_p,
								  const char **display_string,
								  XP_Bool *plural_p);

	virtual MsgERR DataNavigate(MSG_MotionType motion,
						MessageKey startKey, MessageKey *resultKey, 
						MessageKey *resultThreadKey);

	virtual void NotifyPrefsChange(NotifyCode code);

	MsgERR	LoadFolder(MSG_FolderInfo* folder);
	MsgERR	LoadFolder(const char *url);
	MsgERR	ReloadFolder();

	XP_Bool GetThreadLineByIndex(MSG_ViewIndex line, int32 numlines,
							   MSG_MessageLine* data);
	int GetThreadLevelByIndex( MSG_ViewIndex line );
	XP_Bool GetThreadLineByKey(MessageKey key, MSG_MessageLine* data);

	void ToggleRead(int line);

	MSG_ViewIndex GetMessageIndex(MessageKey key, XP_Bool expand = FALSE);
	virtual MessageKey GetMessageKey(MSG_ViewIndex index);     //mscott - made virtual for search as view. Search pane has different method

	virtual void ToggleExpansion(MSG_ViewIndex line, int32* numchanged);
	virtual int32 ExpansionDelta(MSG_ViewIndex line);
	virtual int32 GetNumLines();

	char *BuildUrlForKey(MessageKey key);

	virtual MSG_FolderInfo* GetFolder();
	virtual void SetFolder(MSG_FolderInfo *info);

	// Drafts
	MsgERR	OpenDraft (MSG_FolderInfo *folder, MessageKey key);
	static void OpenDraftExit(URL_Struct *url , int status, MWContext *context);

	MsgERR ResetView(ViewType typeOfView);
	virtual PaneListener *GetListener();
	void	ClearNewBits(XP_Bool notify);

	// IMAP Subscription Upgrade
	// if reallyLoad, loads the folder, otherwise clears its state
	MsgERR	FinishLoadingIMAPUpgradeFolder(XP_Bool reallyLoad);
	MSG_FolderInfo *GetIMAPUpgradeFolder() { return m_imapUpgradeBeginFolder; }
	void	SetIMAPUpgradeFolder(MSG_FolderInfo *f) { m_imapUpgradeBeginFolder = f; }

protected:
	MsgERR ComposeMessage(MSG_CommandType command, MSG_ViewIndex* indices,
						  int32 numIndices);
	MsgERR ForwardMessages(MSG_ViewIndex* indices, int32 numIndices);

	static SortType		GetSortTypeFromCommand(MSG_CommandType command);
	static void ViewChange(MessageDBView *dbView,
					MSG_ViewIndex line_number,
					MessageKey msgKey,
					int num_changed,
					MSG_NOTIFY_CODE code,
					void *closure);

	ThreadPaneListener m_threadPaneListener;
	MSG_FolderInfo* m_folder;
    MessageDBView* m_view;

	MSG_FolderInfo *m_imapUpgradeBeginFolder;
	
};


#endif /* _MsgTPane_H_ */

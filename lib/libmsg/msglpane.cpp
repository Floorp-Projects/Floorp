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

#include "msglpane.h"


struct MSG_LinedPaneStack {
  int fecount;
  MSG_NOTIFY_CODE code;
  MSG_ViewIndex where;
  int32 num;
};


MSG_LinedPane::MSG_LinedPane(MWContext* context, MSG_Master* master) 
: MSG_Pane(context, master) {
}

MSG_LinedPane::~MSG_LinedPane() {
#ifdef DEBUG
  delete [] m_stack;
#endif
}


XP_Bool MSG_LinedPane::IsLinePane() {
  return TRUE;
}



void MSG_LinedPane::StartingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
								   int32 num) {
#ifdef DEBUG
  if (m_numstack <= m_maxstack) {
	m_maxstack = m_numstack + 5;
	MSG_LinedPaneStack* tmp = m_stack;
	m_stack = new MSG_LinedPaneStack[m_maxstack];
	for (int i=0 ; i<m_numstack ; i++) {
	  m_stack[i] = tmp[i];
	}
	delete [] tmp;
  }
  m_stack[m_numstack].fecount = m_fecount;
  m_stack[m_numstack].code = code;
  m_stack[m_numstack].where = where;
  m_stack[m_numstack].num = num;
  XP_ASSERT(m_numstack == 0 || m_stack[m_numstack-1].fecount == m_fecount);
#endif
  m_numstack++;
  if (m_disablecount == 0) {
    FE_ListChangeStarting(this, m_fecount == 0, code, where, num);
  }
}


void MSG_LinedPane::EndingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
								 int32 num) {
  m_numstack--;
#ifdef DEBUG
  XP_ASSERT(m_numstack >= 0);
  XP_ASSERT(m_stack[m_numstack].fecount == m_fecount);
  XP_ASSERT(m_stack[m_numstack].code == code);
  XP_ASSERT(m_stack[m_numstack].where == where);
  XP_ASSERT(m_stack[m_numstack].num == num);
#endif
  if (m_disablecount == 0) {
    FE_ListChangeFinished(this, m_fecount == 0, code, where, num);
  }
}
  
void MSG_LinedPane::OnFolderChanged(MSG_FolderInfo *folderInfo) 
{
	// I believe all FE's support this now...
	FE_PaneChanged(this, FALSE, MSG_PaneNotifyFolderInfoChanged, (uint32) folderInfo);
}


void MSG_LinedPane::DisableUpdate()
{
	m_disablecount++;
}

void MSG_LinedPane::EnableUpdate()
{
	m_disablecount--;
	XP_ASSERT(m_disablecount >= 0);
}

XP_Bool MSG_LinedPane::IsValidIndex(MSG_ViewIndex index)
{
	return (XP_Bool) ((index != MSG_VIEWINDEXNONE) && index < (MSG_ViewIndex) GetNumLines());
}

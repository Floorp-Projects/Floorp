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
/* 
   MNListView.cpp - Mail/News panes that are displayed using an outliner.
   Created: Chris Toshok <toshok@netscape.com>, 25-Aug-96.
 */



#include "MNListView.h"
#include "MozillaApp.h"

#include "xpgetstr.h"

const char* XFE_MNListView::changeFocus = "XFE_MNListView::changeFocus";

extern int XFE_INBOX_DOESNT_EXIST;

extern "C" void fe_showMailFilterDlg(Widget toplevel, MWContext *context, MSG_Pane *pane);

XFE_MNListView::XFE_MNListView(XFE_Component *toplevel_component,
			       XFE_View *parent_view, MWContext *context, MSG_Pane *p) 
  : XFE_MNView(toplevel_component, parent_view, context, p)
{
  m_outliner = NULL;
}

XFE_MNListView::~XFE_MNListView()
{
  /* nothing to do here either. */
}

XFE_Outliner *
XFE_MNListView::getOutliner()
{
  return m_outliner;
}

Boolean
XFE_MNListView::handlesCommand(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
#define IS_CMD(command) cmd == (command)

  if (IS_CMD(xfeCmdSelectAll)
      || cmd == xfeCmdEditMailFilterRules
      )
    {
      return True;
    }
  else
    {
      return XFE_MNView::handlesCommand(cmd, calldata);
    }
#undef IS_CMD
}

Boolean 
XFE_MNListView::isCommandSelected(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
  MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
  XP_Bool selectable = False;

  if (handlesCommand(cmd, calldata))
  {
  	msg_cmd = commandToMsgCmd(cmd);

  	if (msg_cmd != (MSG_CommandType)~0)
  	{

	    if ( MSG_GetToggleStatus(m_pane, msg_cmd, NULL, 0 ) == MSG_Checked) 
            		selectable = True;
            return (Boolean)selectable;
  	}
  }
  return XFE_MNView::isCommandSelected(cmd, calldata);
}
Boolean
XFE_MNListView::isCommandEnabled(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
  MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
  const int *selected;
  int count = 0;
  XP_Bool selectable = False;

  m_outliner->getSelection(&selected, &count);
  
  if (msg_cmd != (MSG_CommandType)~0)
    {

		if (IS_CMD(xfeCmdAddNewsgroup) || IS_CMD(xfeCmdInviteToNewsgroup)) {
			return !XP_IsContextBusy(m_contextData);
		}

		if (IS_CMD(xfeCmdGetNewMessages)) {
			int num_inboxes = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
													 MSG_FOLDER_FLAG_INBOX,
													 NULL, 0);

			/* 5.0 will support more than one imap server. 
			   There is an inbox per server. Therefore, we
			   should not constrain the num_inboxes to be just 1. 
			   Otherwise, we will see some button being
			   disabled for the wrong reason */  
			if (num_inboxes > 0) 
				MSG_CommandStatus(m_pane, msg_cmd, 
								  /* just need some integer pointer here...*/
								  (MSG_ViewIndex*)&count,
								  0, &selectable, NULL, NULL, NULL);
		}/* else */
		else if (count == 0 || 
				 (m_pane && MSG_GetNumLines(m_pane) == 0))
			{
				MSG_CommandStatus(m_pane, msg_cmd, 
								  /* just need some integer pointer here...*/
								  (MSG_ViewIndex*)&count,
								  0, &selectable, NULL, NULL, NULL);
			}
		else
			{
				MSG_CommandStatus(m_pane, msg_cmd,
								  (MSG_ViewIndex*)selected, count, 
								  &selectable, NULL, NULL, NULL);
			}
		return selectable;
      
    }
  else if (IS_CMD(xfeCmdEditMailFilterRules)
	   ||IS_CMD(xfeCmdSearch))
    {
      /* Get thread view context
       */
      /* Get mozilla app first
       */
      XFE_MozillaApp *mozilla = theMozillaApp();

      /* then FRAME_MAILNEWS_THREAD frame
       */
      XP_List *threadList = mozilla->getFrameList(FRAME_MAILNEWS_THREAD);
      int count; 
      Boolean enabled = True;
      if (threadList && 
	  (count=XP_ListCount(threadList))) {
	MWContext *context;
	for (int i=0; i < count; i++) {
	  XFE_Frame *frame = (XFE_Frame *) XP_ListGetObjectNum(threadList,
							       i+1);
	  if (frame && (context=frame->getContext())) {
	    if (XP_IsContextBusy(context)) {
	      /* As long as one thread window is reading messages
	       */
	      enabled = False;
	      break;
	    }/* if */
	  }/* if */
	  
	}/* for i */
      }/* if */
      return enabled;
    }
  else if (IS_CMD(xfeCmdSelectAll))
    {
      return True; // not sure I feel comfortable with such extremes
    }
  else
    {
      return XFE_View::isCommandEnabled(cmd, calldata, info);
    }
#undef IS_CMD
}

void
XFE_MNListView::doCommand(CommandType cmd, void *calldata, 
						  XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
  if (IS_CMD(xfeCmdSelectAll))
    {
      m_outliner->selectAllItems();
    }
  else if (IS_CMD(xfeCmdEditMailFilterRules))
    {
        fe_showMailFilterDlg(getToplevel()->getBaseWidget(),m_contextData,m_pane);
    }
  else
    {
      XFE_MNView::doCommand(cmd, calldata, info);
    }
#undef IS_CMD  
}

char*
XFE_MNListView::commandToString(CommandType cmd, void *calldata, XFE_CommandInfo *info)
{
#define IS_CMD(command) cmd == (command)
	MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
	MSG_MotionType msg_nav = commandToMsgNav(cmd);
	const int *selected;
	int count;
	const char *display_string = NULL;
	
	if (!m_outliner)
		return NULL;
	
	m_outliner->getSelection(&selected, &count);
	
	if (msg_cmd != (MSG_CommandType)~0)
		{
			if (IS_CMD(xfeCmdGetNewMessages)) {
				int num_inboxes = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
														 MSG_FOLDER_FLAG_INBOX,
														 NULL, 0);
				if (num_inboxes > 0 || 
					(MSG_CommandStatus(m_pane, msg_cmd, 
									   (MSG_ViewIndex*)selected, 
									   count, NULL, NULL, 
									   (const char **)&display_string, NULL) < 0)) {
					return NULL;
				}/* if */
			}/* else */
			else if (MSG_CommandStatus(m_pane, msg_cmd, 
									   (MSG_ViewIndex*)selected, 
									   count, NULL, NULL, 
									   (const char **)&display_string, NULL) < 0)
				return NULL;
			else
				{
					if ( (cmd == xfeCmdComposeMessageHTML ) ||
						 ( cmd == xfeCmdComposeMessagePlain)  ||
						 ( cmd == xfeCmdComposeArticleHTML)  ||
						 ( cmd == xfeCmdComposeArticlePlain) )
						{
							return NULL;
						}
				}
		}
	else if (msg_nav != (MSG_MotionType)~0)
		{
			if (count < 1
				|| MSG_NavigateStatus(m_pane, msg_nav, 
									  (MSG_ViewIndex)selected[0],
									  NULL, (const char **)&display_string) < 0)
				return NULL;
		}


	if (display_string)
	   if ( IS_CMD(xfeCmdAddNewsgroup) || IS_CMD(xfeCmdRenameFolder) )
		return  NULL;
	   else
		return  (char*)display_string;
	else
		return XFE_MNView::commandToString(cmd, calldata, info);
#undef IS_CMD  
}


void
XFE_MNListView::setInFocus(XP_Bool infocus)
{
    m_outliner->setInFocus(infocus);
}

XP_Bool
XFE_MNListView::isFocus()
{
   return m_outliner->isFocus();
}

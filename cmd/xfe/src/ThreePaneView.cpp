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
   ThreePaneView.cpp -- class definition for ThreePaneView
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#include "ThreePaneView.h"
#include "ThreadFrame.h"
#include "prefs.h"
#include "prefapi.h"

#include <Xfe/Pane.h>

const char* XFE_ThreePaneView::ShowFolder = "XFE_ThreePaneView::ShowFolder";
#define THREEPANEVIEW_SHOW_PREF "mail.threadpane.3pane"

// Minimum and maximum sizes for the paned window panes:
#define PANE_MIN 50
#define PANE_MAX 6000


XFE_ThreePaneView::XFE_ThreePaneView(XFE_Component *toplevel_component,
				       Widget parent, XFE_View *parent_view,
				       MWContext *context)
  : XFE_View(toplevel_component, parent_view, context)
{
  Widget	hpane;

  hpane = XtVaCreateWidget("hpane",
						   xfePaneWidgetClass,
						   parent,
						   XmNorientation,			XmHORIZONTAL,
						   XmNsashPosition,			200,
						   XmNsashThickness,		10,
						   XmNsashShadowThickness,	1,
						   XmNpaneSashType,			XmPANE_SASH_LIVE,
						   NULL);

  m_focusview = NULL;
  m_folderview = new XFE_FolderView(toplevel_component, hpane, this,
				    context);
  m_threadview = new XFE_ThreadView(toplevel_component, hpane, this,
				    context);

  // add our subviews to the list of subviews for command dispatching and
  // deletion.
  addView(m_folderview);
  addView(m_threadview);

  m_folderview->registerInterest(XFE_FolderView::folderSelected,
				 this,
				 (XFE_FunctionNotification)userClickedFolder_cb);
  getToplevel()->registerInterest(XFE_MNBanner::twoPaneView,
				this,
				(XFE_FunctionNotification)twoPaneView_cb);

  getToplevel()->registerInterest(XFE_MNListView::changeFocus,
				this,
				(XFE_FunctionNotification)changeFocus_cb);
  XP_Bool show_folder;
  PREF_GetBoolPref(THREEPANEVIEW_SHOW_PREF, &show_folder);


  if (show_folder)
  {
  	m_folderview->show();
  }

  m_threadview->show();

  
  /* when show 3 pane, the folder view is the focus by default */
  if ( show_folder )
  {
    m_threadview->setInFocus(False);
    m_folderview->setInFocus(True); 

    /* need to remember to set m_focusview to remember which view 
    ** is in-focus.
    */
    m_focusview= m_folderview; 
  }
  else
  {
    /* when show 2 pane, the thread view is the focus by default */
    m_threadview->setInFocus(True);
    m_folderview->setInFocus(False); 
    m_focusview= m_threadview; 
  }


  setBaseWidget(hpane);
}

XFE_ThreePaneView::~XFE_ThreePaneView()
{

    /* unregister all interests */
    m_folderview->unregisterInterest(XFE_FolderView::folderSelected,
                                 this,
                                 (XFE_FunctionNotification)userClickedFolder_cb);
    getToplevel()->unregisterInterest(XFE_MNBanner::twoPaneView,
                                this,
                                (XFE_FunctionNotification)twoPaneView_cb);

    if ( m_folderview->isShown() )
     {
      PREF_SetBoolPref(THREEPANEVIEW_SHOW_PREF, (XP_Bool)True);
     }
    else
     {
      PREF_SetBoolPref(THREEPANEVIEW_SHOW_PREF, (XP_Bool)False);
     }

    /* XFE_View destroy m_folderview and m_threadview for us */
}

void
XFE_ThreePaneView::selectFolder(MSG_FolderInfo *info)
{
  m_folderview->selectFolder(info);
}

XFE_View*
XFE_ThreePaneView::getThreadView()
{
	return m_threadview;
}

XFE_View*
XFE_ThreePaneView::getFolderView()
{
	return m_folderview;
}

XFE_CALLBACK_DEFN(XFE_ThreePaneView,userClickedFolder)(XFE_NotificationCenter *,
                                 void *, void* calldata)
{
  MSG_FolderInfo *info = (MSG_FolderInfo*)calldata;

  ((XFE_ThreadFrame*)getToplevel())->loadFolder(info);
}

XFE_CALLBACK_DEFN(XFE_ThreePaneView,twoPaneView)(XFE_NotificationCenter *,
                                 void *, void*)
{
	if ( m_folderview->isShown() )
	{
	    m_folderview->hide();
	    if (m_focusview == m_folderview) 
	    {
	    	m_folderview->setInFocus(False);
		m_focusview = m_threadview;
	    }
	    m_focusview->setInFocus(True);
	}
	else m_folderview->show();
}

XFE_CALLBACK_DEFN(XFE_ThreePaneView,changeFocus)(XFE_NotificationCenter *,
                                 void *, void* calldata)
{
	XFE_MNListView *infocus_view = (XFE_MNListView*)calldata;
	
	XP_ASSERT( calldata != NULL);

	if (m_focusview != infocus_view)
	{
	    if (m_focusview )
		m_focusview->setInFocus(False);
	    infocus_view->setInFocus(True);
	    m_focusview=infocus_view;
	    getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
	}
}

Boolean
XFE_ThreePaneView::isCommandEnabled(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{

  XP_ASSERT( m_focusview != NULL);

  return m_focusview->isCommandEnabled(cmd,calldata,info);
}

void
XFE_ThreePaneView::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
  XP_ASSERT( m_focusview != NULL);

  if (IS_CMD(xfeCmdMommy) && m_focusview != m_threadview && m_threadview)
  {
	m_threadview->doCommand(cmd,calldata,info);
  }
  else
   	m_focusview->doCommand(cmd,calldata,info);
}

Boolean
XFE_ThreePaneView::handlesCommand(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
  XP_ASSERT( m_focusview != NULL);
  if (IS_CMD(xfeCmdMommy) && m_focusview != m_threadview && m_threadview)
  {
	return m_threadview->handlesCommand(cmd,calldata,info);
  }

  return m_focusview->handlesCommand(cmd,calldata,info);
}

Boolean
XFE_ThreePaneView::isCommandSelected(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
   XP_ASSERT( m_focusview != NULL);
   return m_focusview->isCommandSelected(cmd,calldata,info);
}

char*
XFE_ThreePaneView::commandToString(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
   XP_ASSERT( m_focusview != NULL);
   return m_focusview->commandToString(cmd,calldata,info);
}

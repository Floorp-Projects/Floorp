/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* 
   CategoryView.cpp -- presents view of news categories.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
   */

#include "MozillaApp.h"
#include "CategoryView.h"
#include "xpassert.h"

#include "xpgetstr.h"
extern int XFE_CATEGORY_OUTLINER_COLUMN_NAME;

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#define OUTLINER_GEOMETRY_PREF "mail.categorypane.outliner_geometry"

const int XFE_CategoryView::OUTLINER_COLUMN_NAME = 0;

const char *XFE_CategoryView::categorySelected = "XFE_CategoryView::categorySelected";

XFE_CategoryView::XFE_CategoryView(XFE_Component *toplevel_component,
								   Widget parent,
								   XFE_View *parent_view, MWContext *context,
								   MSG_Pane *p) 
	: XFE_MNListView(toplevel_component, parent_view, context, p)
{
	int num_columns = 1;
	static int default_column_widths[] = { 20 };
	
	// create our view if we don't have one already.
	if (!p)
		setPane(MSG_CreateCategoryPane(m_contextData,
									   XFE_MNView::m_master));
	
	// create our outliner.
	m_outliner = new XFE_Outliner("categoryList",
								  this,
								  getToplevel(),
								  parent,
								  FALSE, // constantSize
								  TRUE,  // hasHeadings
								  num_columns,
								  num_columns,
								  default_column_widths,
								  OUTLINER_GEOMETRY_PREF);
	
	m_outliner->setPipeColumn( OUTLINER_COLUMN_NAME );
	m_outliner->setMultiSelectAllowed( True );
	
	setBaseWidget(m_outliner->getBaseWidget());
}

XFE_CategoryView::~XFE_CategoryView()
{
D(printf ("In XFE_CategoryView::~XFE_CategoryView()\n");)

	delete m_outliner;
	
	destroyPane();
}

void
XFE_CategoryView::loadFolder(MSG_FolderInfo *folderinfo)
{
	m_folderInfo = folderinfo;

	MSG_LoadNewsGroup(m_pane, m_folderInfo);

	XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::MNChromeNeedsUpdating);
}

void
XFE_CategoryView::selectCategory(MSG_FolderInfo *category)
{
	MSG_ViewIndex index;

	index = MSG_GetFolderIndexForInfo(m_pane, category, True);

	if (index != MSG_VIEWINDEXNONE)
		m_outliner->selectItemExclusive(index);
}

Boolean
XFE_CategoryView::isCommandEnabled(CommandType cmd,
								   void *calldata, XFE_CommandInfo*)
{
	return XFE_MNListView::isCommandEnabled(cmd, calldata);
}

Boolean
XFE_CategoryView::handlesCommand(CommandType cmd,
								 void *calldata, XFE_CommandInfo*)
{
	if (cmd == xfeCmdDeleteCategory
		|| cmd == xfeCmdKillCategory
		|| cmd == xfeCmdMarkCategoryRead
		|| cmd == xfeCmdMarkCategoryUnread
		|| cmd == xfeCmdSelectCategory)
		{
			return True;
		}
	else
		{
			return XFE_MNListView::handlesCommand(cmd, calldata);
		}
}

void
XFE_CategoryView::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
	MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
	const int *selected;
	int count;
	
	m_outliner->getSelection(&selected, &count);
	
	if ((msg_cmd == (MSG_CommandType)~0) ||
	    (msg_cmd == MSG_PostNew) ||
	    (msg_cmd == MSG_MailNew) )
		{
			XFE_MNListView::doCommand(cmd, calldata, info);
		}
	else
		{
			MSG_Command(m_pane, msg_cmd, (MSG_ViewIndex*)selected, count);
		}
	
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void *
XFE_CategoryView::ConvFromIndex(int index)
{
	return MSG_GetFolderInfo(m_pane, index);
}

int
XFE_CategoryView::ConvToIndex(void *item)
{
	MSG_ViewIndex index = MSG_GetFolderIndex(m_pane, (MSG_FolderInfo*)item);
	
	if (index == MSG_VIEWINDEXNONE)
		return -1;
	else
		return index;
}


char *
XFE_CategoryView::getColumnName(int column)
{
	// there is only one column in the category view.
	XP_ASSERT(column == OUTLINER_COLUMN_NAME);
	
	return "Category";
}

char *
XFE_CategoryView::getColumnHeaderText(int column)
{
	// there is only one column in the category view.
	XP_ASSERT(column == OUTLINER_COLUMN_NAME);

	return XP_GetString(XFE_CATEGORY_OUTLINER_COLUMN_NAME);
}

fe_icon *
XFE_CategoryView::getColumnHeaderIcon(int)
{
	return 0;
}

EOutlinerTextStyle
XFE_CategoryView::getColumnHeaderStyle(int)
{
	return OUTLINER_Default;
}

void *
XFE_CategoryView::aquireLineData(int line)
{
  m_ancestorInfo = NULL;

  if (!MSG_GetFolderLineByIndex(m_pane, line, 1, &m_categoryLine))
    return NULL;

  if ( m_categoryLine.level > 0 ) 
    {
      m_ancestorInfo = new OutlinerAncestorInfo[ m_categoryLine.level ];
    }
  else
    {
      m_ancestorInfo = new OutlinerAncestorInfo[ 1 ];
    }
      
  // ripped straight from the winfe
  int i = m_categoryLine.level - 1;
  int idx = line + 1;
  int total_lines = m_outliner->getTotalLines();
  while ( i > 0 ) {
    int level;
    if ( idx < total_lines ) {
      level = MSG_GetFolderLevelByIndex( m_pane, idx );
      if ( (level - 1) == i ) {
	m_ancestorInfo[i].has_prev = TRUE;
	m_ancestorInfo[i].has_next = TRUE;
	i--;
	idx++;
      } else if ( (level - 1) < i ) {
	m_ancestorInfo[i].has_prev = FALSE;
	m_ancestorInfo[i].has_next = FALSE;
	i--;
      } else {
	idx++;
      }
    } else {
      m_ancestorInfo[i].has_prev = FALSE;
      m_ancestorInfo[i].has_next = FALSE;
      i--;
    }
  }

  m_ancestorInfo[0].has_prev = FALSE;
  m_ancestorInfo[0].has_next = FALSE;
  
  return &m_categoryLine;
}

void 
XFE_CategoryView::getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded,
							  int *depth, OutlinerAncestorInfo **ancestor)
{
  XP_Bool is_line_expandable;
  XP_Bool is_line_expanded;

  is_line_expandable = m_categoryLine.numChildren > 0;

  if (is_line_expandable)
    {
      is_line_expanded = !(m_categoryLine.flags & MSG_FOLDER_FLAG_ELIDED);
    }
  else
    {
      is_line_expanded = FALSE;
    }

  if (ancestor)
    *ancestor = m_ancestorInfo;

  if (depth)
    *depth = m_categoryLine.level - 1;

  if (expandable)
    *expandable =  is_line_expandable;

  if (is_expanded)
    *is_expanded = is_line_expanded;
}

EOutlinerTextStyle
XFE_CategoryView::getColumnStyle(int)
{
  return (m_categoryLine.unseen > 0) ? OUTLINER_Bold : OUTLINER_Default;
}

char *
XFE_CategoryView::getColumnText(int)
{
	if (m_categoryLine.prettyName)
		return (char*)m_categoryLine.prettyName;
	else
		return (char*)m_categoryLine.name;
}

fe_icon *
XFE_CategoryView::getColumnIcon(int)
{
	return &newsgroupIcon;
}

void
XFE_CategoryView::releaseLineData()
{
  delete [] m_ancestorInfo;
  m_ancestorInfo = NULL;
}

void
XFE_CategoryView::Buttonfunc(const OutlineButtonFuncData *data)
{
  MSG_FolderLine line;

  if (!MSG_GetFolderLineByIndex(m_pane, data->row, 1, &line)) return;

  m_outliner->selectItemExclusive(data->row);
  
  notifyInterested(categorySelected, MSG_GetFolderInfo(m_pane, data->row));

  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_CategoryView::Flippyfunc(const OutlineFlippyFuncData *data)
{
  if (MSG_ExpansionDelta(m_pane, data->row) != 0)
    MSG_ToggleExpansion(m_pane, data->row, NULL);
  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

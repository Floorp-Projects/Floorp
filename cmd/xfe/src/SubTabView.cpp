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
   SubTabView.cpp -- 4.x subscribe ui tabs.
   Created: Chris Toshok <toshok@netscape.com>, 18-Oct-1996.
   */



#include "SubTabView.h"
#include "Outliner.h"
#include "Command.h"

#include "xfe.h"
#include "msgcom.h"
#include "xp_mem.h"
#include "xpassert.h"

#include <Xm/DrawnB.h>
#include <Xm/Form.h>
#include <XmL/Folder.h>
#include "DtWidgets/ComboBox.h"

#include "xpgetstr.h"
extern int XFE_SUB_OUTLINER_COLUMN_NAME;
extern int XFE_SUB_OUTLINER_COLUMN_POSTINGS;

const int XFE_SubTabView::OUTLINER_COLUMN_NAME = 0;
const int XFE_SubTabView::OUTLINER_COLUMN_SUBSCRIBE = 1;
const int XFE_SubTabView::OUTLINER_COLUMN_POSTINGS = 2;

fe_icon XFE_SubTabView::subscribedIcon = { 0 };

int XFE_SubTabView::m_numhosts = -1;
MSG_Host **XFE_SubTabView::m_hosts = NULL;

XFE_SubTabView::XFE_SubTabView(XFE_Component *toplevel_component, Widget parent,
							   XFE_View *parent_view, MWContext *context,
							   int tab_string_id, MSG_Pane *p)
	: XFE_MNListView(toplevel_component, parent_view, context, p)
{
	XmString xmstr;
	xmstr = XmStringCreate(XP_GetString( tab_string_id ), XmFONTLIST_DEFAULT_TAG);
	m_form = XmLFolderAddTabForm(parent, xmstr);
	XmStringFree(xmstr);

	setBaseWidget(m_form);

	m_ancestorInfo = NULL;
}

XFE_SubTabView::~XFE_SubTabView()
{
	if (m_hosts) delete [] m_hosts;

	m_hosts = NULL;
	m_numhosts = -1;
}

void
XFE_SubTabView::init_outliner_icons(Widget outliner)
{
	{
		Pixel bg_pixel;
    
		XtVaGetValues(outliner, XmNbackground, &bg_pixel, 0);
    
		if (!subscribedIcon.pixmap)
			fe_NewMakeIcon(getToplevel()->getBaseWidget(),
						   getToplevel()->getFGPixel(),
						   bg_pixel,
						   &subscribedIcon,
						   NULL, 
						   MN_Check.width, MN_Check.height,
						   MN_Check.mono_bits, MN_Check.color_bits, MN_Check.mask_bits, FALSE);

		// update this to the right icon once hagan gives it to us.
		if (!msgReadIcon.pixmap)
			fe_NewMakeIcon(getToplevel()->getBaseWidget(),
						   getToplevel()->getFGPixel(),
						   bg_pixel,
						   &msgReadIcon,
						   NULL, 
						   MN_DotRead.width, MN_DotRead.height,
						   MN_DotRead.mono_bits, MN_DotRead.color_bits, MN_DotRead.mask_bits, FALSE);
			
		if (!newsgroupIcon.pixmap)
			fe_NewMakeIcon(getToplevel()->getBaseWidget(),
						   getToplevel()->getFGPixel(),
						   bg_pixel,
						   &newsgroupIcon,
						   NULL, 
						   MN_Newsgroup.width, MN_Newsgroup.height,
						   MN_Newsgroup.mono_bits, MN_Newsgroup.color_bits, MN_Newsgroup.mask_bits, FALSE);
	}
}

Boolean
XFE_SubTabView::isCommandEnabled(CommandType command, void */*calldata*/, XFE_CommandInfo*)
{
	MWContext *c = MSG_GetContext(m_pane);

	if (command == xfeCmdStopLoading)
		{
			return fe_IsContextStoppable(c);
		}
	else
		{
			XP_Bool selectable = False;
			MSG_CommandType msg_cmd = commandToMsgCmd(command);
      
			if (msg_cmd != (MSG_CommandType)~0)
				{
					const int *selected;
					int count = 0;

					m_outliner->getSelection(&selected, &count);
	  
					if (MSG_CommandStatus(m_pane, msg_cmd, (MSG_ViewIndex*)selected,
										  count, &selectable, NULL, NULL, NULL) < 0)
						selectable = FALSE;
				}

      	  
			return selectable;
		}
}

Boolean
XFE_SubTabView::handlesCommand(CommandType command, void */*calldata*/, XFE_CommandInfo*)
{
	if (command == xfeCmdToggleSubscribe
		|| command == xfeCmdStopLoading)
		{
			return True;
		}
	else
		{
			return False; // we don't want XFE_MNListView to get caught up in this
		}
}

void
XFE_SubTabView::doCommand(CommandType command, void */*calldata*/, XFE_CommandInfo*)
{
	const int *selected;
	int count;
  
	m_outliner->getSelection(&selected, &count);

	if (command == xfeCmdToggleSubscribe)
		{
			MSG_Command(m_pane, MSG_ToggleSubscribed, (MSG_ViewIndex*)selected, count);
		}
	else if (command == xfeCmdStopLoading)
		{
			XP_InterruptContext(m_contextData);
		}
	else
		{
			MSG_CommandType msg_cmd = commandToMsgCmd(command);

			if (msg_cmd != (MSG_CommandType)~0)
				MSG_Command(m_pane, msg_cmd, (MSG_ViewIndex*)selected, count);
		}

	updateButtons();
}

void *
XFE_SubTabView::ConvFromIndex(int /*index*/)
{
	return 0;
}

int
XFE_SubTabView::ConvToIndex(void */*item*/)
{
	return 0;
}

char *
XFE_SubTabView::getColumnName(int column)
{
	switch (column)
		{
		case OUTLINER_COLUMN_NAME:	return "Name";
		case OUTLINER_COLUMN_SUBSCRIBE:	return "Sub";
		case OUTLINER_COLUMN_POSTINGS:	return "Postings";
		default: XP_ASSERT(0); return 0;
		}
}

char *
XFE_SubTabView::getColumnHeaderText(int column)
{
	switch (column)
		{
		case OUTLINER_COLUMN_NAME:
			return XP_GetString(XFE_SUB_OUTLINER_COLUMN_NAME);
		case OUTLINER_COLUMN_SUBSCRIBE:
			return 0;
		case OUTLINER_COLUMN_POSTINGS:
			return XP_GetString(XFE_SUB_OUTLINER_COLUMN_POSTINGS);
		default:
			XP_ASSERT(0);
			return 0;
		}
}

fe_icon *
XFE_SubTabView::getColumnHeaderIcon(int column)
{
	if (column == OUTLINER_COLUMN_SUBSCRIBE)
		return &subscribedIcon;
	else
		return 0;
}

EOutlinerTextStyle
XFE_SubTabView::getColumnHeaderStyle(int /*column*/)
{
	return OUTLINER_Default;
}

void *
XFE_SubTabView::acquireLineData(int line)
{
	if (!MSG_GetGroupNameLineByIndex(m_pane, line, 1, &m_groupLine))
		return NULL;

	m_ancestorInfo = new OutlinerAncestorInfo[m_groupLine.level + 1];

	int i = m_groupLine.level;
	int idx = line + 1;
	while ( i > 0 ) 
		{
			if ( idx < m_outliner->getTotalLines())
				{
					MSG_GroupNameLine groupLine;
					MSG_GetGroupNameLineByIndex( m_pane, idx, 1, &groupLine );
					if ( groupLine.level == i ) 
						{
							m_ancestorInfo[i].has_prev = TRUE;
							m_ancestorInfo[i].has_next = TRUE;
							i--;
							idx++;
						} 
					else if ( groupLine.level < i ) 
						{
							m_ancestorInfo[i].has_prev = FALSE;
							m_ancestorInfo[i].has_next = FALSE;
							i--;
						}
					else
						{
							idx++;
						}
				}
			else
				{
					m_ancestorInfo[i].has_prev = FALSE;
					m_ancestorInfo[i].has_next = FALSE;
					i--;
				}
		}
		m_ancestorInfo[i].has_prev = FALSE;
		m_ancestorInfo[i].has_next = FALSE;

	return &m_groupLine;
}

void
XFE_SubTabView::getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth,
							OutlinerAncestorInfo **ancestor)
{
	XP_Bool is_line_expandable;
	XP_Bool is_line_expanded;

	is_line_expandable = (m_groupLine.flags & MSG_GROUPNAME_FLAG_HASCHILDREN) != 0;

	if (is_line_expandable)
		{
			is_line_expanded = (m_groupLine.flags & MSG_GROUPNAME_FLAG_ELIDED) == 0;
		}
	else
		{
			is_line_expanded = FALSE;
		}

	if ( ancestor )
		*ancestor = m_ancestorInfo;

	if (depth)
		*depth = m_groupLine.level;

	if (expandable)
		*expandable = is_line_expandable;

	if (is_expanded)
		*is_expanded = is_line_expanded;
}

EOutlinerTextStyle
XFE_SubTabView::getColumnStyle(int /*column*/)
{
	if (m_groupLine.total > 0)
		return OUTLINER_Bold;
	else
		return OUTLINER_Default;
}

char *
XFE_SubTabView::getColumnText(int column)
{
	static char postings_buf[256];

	switch (column)
		{
		case OUTLINER_COLUMN_NAME:
			return m_groupLine.name;
		case OUTLINER_COLUMN_SUBSCRIBE:
			return 0;
		case OUTLINER_COLUMN_POSTINGS:
			if (m_groupLine.total > 0)
				{
					sprintf(postings_buf, "%d", m_groupLine.total);
					return postings_buf;
				}
			else
				{
					return 0;
				}
		default:
			XP_ASSERT(0);
			return 0;
		}
}

fe_icon *
XFE_SubTabView::getColumnIcon(int column)
{
	switch (column)
		{
		case OUTLINER_COLUMN_NAME:
			return &newsgroupIcon;
		case OUTLINER_COLUMN_SUBSCRIBE:
			if (!(m_groupLine.flags & MSG_GROUPNAME_FLAG_HASCHILDREN))
				if (m_groupLine.flags & MSG_GROUPNAME_FLAG_SUBSCRIBED)
					return &subscribedIcon;
				else
					return &msgReadIcon; // XXX 
			else
				return 0;
		case OUTLINER_COLUMN_POSTINGS:
			return 0;
		default:
			XP_ASSERT(0);
			return 0;
		}
}

void
XFE_SubTabView::releaseLineData()
{
	if (m_ancestorInfo)
		{
			delete [] m_ancestorInfo;
			m_ancestorInfo = NULL;
		}
}

void
XFE_SubTabView::Buttonfunc(const OutlineButtonFuncData *data)
{
	if (data->row == -1)
		return; // no sorting in these views.

	if (data->clicks == 2)
		{
			m_outliner->selectItemExclusive(data->row);

			if (m_groupLine.flags & MSG_GROUPNAME_FLAG_HASCHILDREN)
				{
					MSG_ToggleExpansion(m_pane, (MSG_ViewIndex)data->row, NULL);
				}
			else
				MSG_Command(m_pane, MSG_ToggleSubscribed, (MSG_ViewIndex*)&data->row, 1);      
		}
	else
		{
			if (data->ctrl)
				{
					m_outliner->toggleSelected(data->row);
				}
			else if (data->shift)
				{
					// select the range.
					const int *selected;
					int count;
					
					m_outliner->getSelection(&selected, &count);
					
					if (count == 0) /* there wasn't anything selected yet. */
						{
							m_outliner->selectItemExclusive(data->row);
						}
					else if (count == 1) /* there was only one, so we select the range from
											that item to the new one. */
						{
							m_outliner->selectRangeByIndices(selected[0], data->row);
						}
					else /* we had a range of items selected, so let's do something really
							nice with them. */
						{
							m_outliner->trimOrExpandSelection(data->row);
						}
				}
			else
				{
					// handle the columns that don't actually move the selection here
					if (data->column == OUTLINER_COLUMN_SUBSCRIBE)
						{
							if (!(m_groupLine.flags & MSG_GROUPNAME_FLAG_HASCHILDREN))
								MSG_Command(m_pane, MSG_ToggleSubscribed, (MSG_ViewIndex*)&data->row, 1);
						}
					else
						{
							// we've selected a newsgroup
							m_outliner->selectItemExclusive(data->row);
						}
				}
		}
	
	updateButtons();
}

void
XFE_SubTabView::Flippyfunc(const OutlineFlippyFuncData *data)
{
	int delta;
	XP_Bool selection_needs_bubbling = False;

	delta = MSG_ExpansionDelta(m_pane, data->row);

	if (delta == 0)
		return;

	/* we're not selected and we're being collapsed.
	   check if any of our children are selected, and if
	   so we select this row. */
	if (!m_outliner->isSelected(data->row)
		&& delta < 0)
		{
			int num_children = -1 * delta;
			int i;
			
			for (i = data->row + 1; i < data->row + num_children; i ++)
				if (m_outliner->isSelected(i))
					{
						selection_needs_bubbling = True;
					}
		}

	MSG_ToggleExpansion(m_pane, data->row, NULL);

	if (selection_needs_bubbling)
		m_outliner->selectItem(data->row);

	updateButtons();
}

void
XFE_SubTabView::updateButtons()
{
}

void
XFE_SubTabView::initializeServerCombo(Widget parent)
{
	Colormap cmap;
	Cardinal depth;
	Visual *v;

	XtVaGetValues(getToplevel()->getBaseWidget(),
				  XmNvisual, &v,
				  XmNcolormap, &cmap,
				  XmNdepth, &depth,
				  NULL);

	m_serverCombo = XtVaCreateManagedWidget("serverCombo",
											dtComboBoxWidgetClass,
											parent,
											XmNtype, XmDROP_DOWN_COMBO_BOX,
											XmNarrowType, XmMOTIF,
											XmNvisibleItemCount, 5,
											XmNvisual, v,
											XmNcolormap, cmap,
											XmNdepth, depth,
											NULL);

	XtAddCallback(m_serverCombo, XmNselectionCallback, server_select_cb, this);
}

void
XFE_SubTabView::serverSelected()
{
	// nothing to do here.  subclasses will do stuff, though.
}

void
XFE_SubTabView::server_select(int pos)
{
	XP_ASSERT(pos < m_numhosts && m_hosts);

	if (m_hosts [ pos ] != MSG_SubscribeGetHost(m_pane))
		{
			MSG_SubscribeSetHost(m_pane, m_hosts[ pos ]);
      
			serverSelected();
		}
}

void
XFE_SubTabView::server_select_cb(Widget, XtPointer clientData, XtPointer cd)
{
	DtComboBoxCallbackStruct *cb = (DtComboBoxCallbackStruct *)cd;
	XFE_SubTabView *obj = (XFE_SubTabView *)clientData;

	obj->server_select(cb->item_position);
}

void
XFE_SubTabView::defaultFocus()
{
	// nothing.
}

int
XFE_SubTabView::getButtonsMaxWidth()
{
  return 0;
}

void
XFE_SubTabView::setButtonsWidth(int /*width*/)
{
  // nothing.
}

void
XFE_SubTabView::syncServerList()
{
	if (m_hosts) delete [] m_hosts;

    // This needs to be updated to handle IMAP as well!
	m_numhosts = MSG_GetSubscribingHosts(XFE_MNView::getMaster(),
                                         NULL, 0);
  
  
	m_hosts = new MSG_Host* [ m_numhosts ];

	MSG_GetSubscribingHosts(XFE_MNView::getMaster(), m_hosts, m_numhosts);
}

// this method is called when a tab is activated.
void
XFE_SubTabView::syncServerCombo()
{
	int i;

	XP_ASSERT(m_hosts);

	DtComboBoxDeleteAllItems(m_serverCombo);

	for (i = 0; i < m_numhosts; i ++)
		{
			XmString str;
      
			str = XmStringCreate((char*)MSG_GetHostUIName(m_hosts[i]), XmFONTLIST_DEFAULT_TAG);
			DtComboBoxAddItem(m_serverCombo, str, 0, True);
			if (m_hosts[i] == MSG_SubscribeGetHost(m_pane))
				{
					DtComboBoxSelectItem(m_serverCombo, str);
					//					DtComboBoxSetItem(m_serverCombo, str);
				}
			XmStringFree(str);
		}
}

void
XFE_SubTabView::button_callback(Widget w, 
								XtPointer clientData,
								XtPointer /*calldata*/)
{
	XFE_SubTabView *obj = (XFE_SubTabView*)clientData;
  
	obj->doCommand(Command::intern(XtName(w)));
}

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
   FolderDropdown.cpp --- a combo box for selecting folders/newsgroups
   Created: Chris Toshok <toshok@netscape.com>, 6-Mar-97
 */



#include "FolderFrame.h"
#include "FolderDropdown.h"
#include "MozillaApp.h"

#undef DEBUG_dora
#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#ifdef DEBUG_dora
#define DD(x) x
#else
#define DD(x)
#endif


const char * XFE_FolderDropdown::folderSelected = "XFE_FolderDropdown::folderSelected";

XFE_FolderDropdown::XFE_FolderDropdown(XFE_Component *toplevel_component,
									   Widget parent,
									   XP_Bool allowServerSelection,
									   XP_Bool showNewsgroups,
									   XP_Bool boldWithNew)
	: XFE_Component(toplevel_component)
{
	m_popupServer = True;

	Widget combo;
	Colormap cmap;
	Cardinal depth;
	Visual *v;

	XtVaGetValues(getToplevel()->getBaseWidget(),
				  XmNvisual, &v,
				  XmNcolormap, &cmap,
				  XmNdepth, &depth,
				  NULL);

	m_allowServerSelection = allowServerSelection;
	m_showNewsgroups = showNewsgroups;
	m_boldWithNew = boldWithNew;
	m_foldersHaveChanged = FALSE;

	combo = XtVaCreateWidget("folderDropdown",
							 dtComboBoxWidgetClass,
							 parent,
							 XmNshadowThickness, 1,
							 XmNmarginWidth, 0,
							 XmNmarginHeight, 0,
							 XmNarrowType, XmMOTIF,
							 XmNtype, XmDROP_DOWN_LIST_BOX,
							 XmNcolumns, 30,
							 XmNvisibleItemCount, 20,
							 XmNvisual, v,
							 XmNcolormap, cmap,
							 XmNdepth, depth,
							 NULL);

	m_lastSelectedPosition = 0;
	m_numNewsHosts = 0;
	m_numinfos = 0;
	m_infos = 0;

	XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::foldersHaveChanged,
											   this,
											   (XFE_FunctionNotification)rebuildFolderDropdown_cb);
	XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::newsgroupsHaveChanged,
											   this,
											   (XFE_FunctionNotification)rebuildFolderDropdown_cb);
	if (m_boldWithNew)
		XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::folderChromeNeedsUpdating,
												   this,
												   (XFE_FunctionNotification)boldFolderInfo_cb);

	XtAddCallback(combo, XmNselectionCallback, folderSelect_cb, this);
	XtAddCallback(combo, XmNmenuPostCallback, folderMenuPost_cb, this);

	setBaseWidget(combo);
	installDestroyHandler();

	resyncDropdown();
}


XFE_FolderDropdown::~XFE_FolderDropdown()
{
	if (m_infos)
		delete [] m_infos;

	XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::foldersHaveChanged,
												 this,
												 (XFE_FunctionNotification)rebuildFolderDropdown_cb);
	XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::newsgroupsHaveChanged,
												 this,
												 (XFE_FunctionNotification)rebuildFolderDropdown_cb);
	if (m_boldWithNew)
		XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::folderChromeNeedsUpdating,
													 this,
													 (XFE_FunctionNotification)boldFolderInfo_cb);
}

void 
XFE_FolderDropdown::setPopupServer(XP_Bool p)
{
	m_popupServer = p;
}

MSG_FolderInfo *
XFE_FolderDropdown::getSelectedFolder()
{
	int info;

	if (m_infos == NULL) return NULL;

	XtVaGetValues(m_widget,
				  XmNselectedPosition, &info,
				  NULL);

	if (info < 0 || info > m_numinfos)
		return NULL;
	else
		return m_infos[info];
}

void
XFE_FolderDropdown::selectFolder(MSG_FolderInfo *info, Boolean notify)
{
	int i;

	if (m_foldersHaveChanged)
	  {
		m_foldersHaveChanged = FALSE;
		resyncDropdown();
	  }

	for (i = 0; i < m_numinfos; i ++)
		{
			if (info == m_infos[i])
				{
					if ( notify )
						folderSelect(i);
					else
					{
					m_lastSelectedPosition = i;
					}

					XtVaSetValues(m_widget,
								  XmNselectedPosition, i,
								  0);
					return;
				}
		}

	/* we might really want to know if the folder isn't there in the list,
	   but this actually can happen frequently if the user deletes folders */
	//XP_ASSERT(0);
}

void
XFE_FolderDropdown::buildList(MSG_FolderInfo *info, int *position)
{


	int num_children = MSG_GetFolderChildren(fe_getMNMaster(),
											 info,
											 NULL, 0);

#ifdef DEBUG_dora
	// For debugging P0 bug on XP only.
	MSG_FolderLine line;

	printf("buildList %x\n", info);
	printf("num_children %d\n", num_children);

	if ( info )
 	{
	MSG_GetFolderLineById(fe_getMNMaster(), info, &line);

 	if ( line.name)
        	printf("folder name=%s\n", line.name);
	else printf("no folder name...\n");
	}

#endif
	if (num_children == 0) 
		{
			return;
		}
	else
		{
			MSG_FolderInfo **tmp;
			int i;
			
			tmp = new MSG_FolderInfo* [num_children];

			/* first we handle local mail stuff. */
			MSG_GetFolderChildren(fe_getMNMaster(),
								  info,
								  tmp,
								  num_children);
			
			for (i = 0; i < num_children; i ++)
				{
					if (*position >= m_numinfos) return;

					m_infos[*position] = tmp[i];
					*position += 1;

					XP_ASSERT(tmp[i]);
					buildList(tmp[i], position);
				}
			
			delete [] tmp;
		}
}

void
XFE_FolderDropdown::syncFolderList()
{
	MSG_Master *master = fe_getMNMaster();

	int num_infos;

	if (m_infos) delete [] m_infos;

	/* this should be the number of MSG_FolderInfo's for mail,
	   including servers. */
	num_infos = MSG_GetFoldersWithFlag(master,
									   MSG_FOLDER_FLAG_MAIL,
									   NULL, 0);

	if (m_showNewsgroups)
		{
			num_infos += MSG_GetFoldersWithFlag(master,
												MSG_FOLDER_FLAG_NEWSGROUP,
												NULL, 0);

			m_numNewsHosts = MSG_GetFoldersWithFlag(master,
													MSG_FOLDER_FLAG_NEWS_HOST,
													NULL, 0);
			num_infos += m_numNewsHosts;
		}

	m_numinfos = num_infos;
	m_infos = new MSG_FolderInfo* [num_infos];

	int pos = 0;

	/* build the default mail tree */
	buildList(NULL, &pos);

	/* now we build each newshost */
	if (m_numNewsHosts > 0)
	{
		MSG_FolderInfo **news_hosts;
		int i;

		news_hosts = new MSG_FolderInfo* [m_numNewsHosts];

		MSG_GetFoldersWithFlag(master,
							   MSG_FOLDER_FLAG_NEWS_HOST,
							   news_hosts, m_numNewsHosts);

		for (i = 0; i < m_numNewsHosts; i++)
			buildList(news_hosts[i], &pos);

		delete [] news_hosts;
	}
	DD(printf("\nold m_numinfos = %d new numinfos %d\n", 
		m_numinfos, pos);)
	
	m_numinfos = pos;
}

void
XFE_FolderDropdown::syncCombo()
{
	int i;
	XmStringTable xmstrings;
	XmStringTable old_xmstrings = NULL;

	DtComboBoxDeleteAllItems(m_widget);

	m_lastSelectedPosition = -1;

	D(printf ("In syncCombo().  There are %d entries\n", m_numinfos);)

	xmstrings = (XmString*)XtCalloc(m_numinfos, sizeof(XmString));
	
	for (i = 0; i < m_numinfos; i ++)
		{
			MSG_FolderLine line;
			
			if ( !m_infos[i] ) 
				{  
					// This block is just workaround for temp fix on the P0 crash
					// bug when trying to bring up ThreadWindow.
					
					// This is to prevent a P0 crash in XP code.
					// We found that the all (subscribed & non-subscribed) news
					// group was parsed when we tried to genterate a
					// Combobox of all subscribed newsgroup.
					
					// MSG_GetFolderChildren() should not try to parse
					// unsubscribed newsgroup....
					// David B. will investigate this further

					DD(printf("[XFE_FolderDropdown Warning:]! Hitting NULL FolderInfo\n");)

					for (i = 0; i < m_numinfos; i ++)
						{
							if (xmstrings[i])
								XmStringFree(xmstrings[i]);
						}
					XtFree((char*)xmstrings);

					return;
				}

			MSG_GetFolderLineById(fe_getMNMaster(), m_infos[i], &line);
			
			xmstrings[i] = makeListItemFromLine(&line);
		}

	XtVaGetValues(m_widget, XmNitems, &old_xmstrings, NULL);

	if (old_xmstrings)
		XtFree((char*)old_xmstrings);

	XtVaSetValues(m_widget, XmNitems, xmstrings, XmNitemCount, m_numinfos, NULL);
}

void
XFE_FolderDropdown::resyncDropdown()
{
	MSG_FolderInfo *saved_info = getSelectedFolder();

	syncFolderList();
	syncCombo();

	if (saved_info)
		selectFolder(saved_info);
}

XFE_CALLBACK_DEFN(XFE_FolderDropdown, rebuildFolderDropdown)(XFE_NotificationCenter *,
															 void *,
															 void *)
{
	resyncDropdown();

	m_foldersHaveChanged = TRUE;
}

XFE_CALLBACK_DEFN(XFE_FolderDropdown, boldFolderInfo)(XFE_NotificationCenter *,
													  void *,
													  void *cd)
{
	/* This is wasteful and duplicates much of syncCombo above, but only for
	   one folder info */
	MSG_FolderInfo *info = (MSG_FolderInfo*)cd;
	MSG_FolderLine line;

	int index;
	Widget list;
	Boolean isSelected;

	if (m_foldersHaveChanged)
	  {
		m_foldersHaveChanged = FALSE;
		resyncDropdown();
	  }

	for (index = 0; index < m_numinfos; index ++)
		{
			if (info == m_infos[ index ])
				{
					break;
				}
		}

	/* not found */
	if (index == m_numinfos) return;
	
	if (!MSG_GetFolderLineById(fe_getMNMaster(), info, &line))
		return;

	XmString both = makeListItemFromLine(&line);	
			
	XtVaGetValues(m_widget, XmNlist, &list, NULL);

	isSelected = XmListPosSelected(list, index + 1);

	XmListReplaceItemsPos(list, &both, 1, index + 1);

	if (isSelected)
		XmListSelectPos(list, index + 1, False);

	XmStringFree(both);
}

void
XFE_FolderDropdown::folderSelect(int row)
{
	MSG_FolderLine line;

	XP_ASSERT(row >= 0 && row < m_numinfos);

	if (!MSG_GetFolderLineById(fe_getMNMaster(),
							   m_infos[row],
							   &line))
		return;

	if (line.level == 1) // it's a server '	
		{
			if (m_popupServer)
				fe_showFoldersWithSelected(XtParent(getToplevel()->getBaseWidget()),
										   NULL,
										   NULL,
										   m_infos[row]);
			if (!m_allowServerSelection)
				/* do not allow select; revert it!
				 */
				row = m_lastSelectedPosition;
	

			if (row == -1)
				/* We shall loop through our list to 
				 * retrieve the first non-server entry instead of hard-wire it...
				 */
				row = 0;

			XtVaSetValues(m_widget,
						  XmNselectedPosition, row,
						  NULL);
		}

	m_lastSelectedPosition = row;

	notifyInterested(XFE_FolderDropdown::folderSelected,
					 (void*)m_infos[row]);
}

void
XFE_FolderDropdown::folderMenuPost()
{
	if (m_foldersHaveChanged)
	  {
		m_foldersHaveChanged = FALSE;
		resyncDropdown();
	  }
}

void
XFE_FolderDropdown::folderSelect_cb(Widget, XtPointer clientData, XtPointer cd)
{
	DtComboBoxCallbackStruct *cbs = (DtComboBoxCallbackStruct*)cd;
	XFE_FolderDropdown *obj = (XFE_FolderDropdown*)clientData;

	obj->folderSelect(cbs->item_position);
}

void
XFE_FolderDropdown::folderMenuPost_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_FolderDropdown *obj = (XFE_FolderDropdown*)clientData;

  obj->folderMenuPost();
}

XmString
XFE_FolderDropdown::makeListItemFromLine(MSG_FolderLine* line)
{
	char buf[500];
	memset(buf, 32, line->level - 1);
	
	buf[line->level - 1] = 0;

	XmString space;	
	space = XmStringCreate(buf, "BOLD");
	
	if (line->prettyName)
		PR_snprintf(buf, sizeof(buf), "%s", line->prettyName);
	else
		PR_snprintf(buf, sizeof(buf), "%s", line->name);
	
	XmString xmstr;
	if (m_boldWithNew && line->unseen > 0)
		xmstr = XmStringCreate(buf, "BOLD");
	else
		xmstr = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	
	XmString both;
	both = XmStringConcat(space, xmstr);
	
	XmStringFree(space);
	XmStringFree(xmstr);

	return 	both;		
}

void
XFE_FolderDropdown::selectFolder(char *name)
{
	if (name && XP_STRLEN(name)) {
		const char *this_folder_name;
		for (int i=0; i < m_numinfos; i++) {
			MSG_FolderLine line;

			MSG_GetFolderLineById(fe_getMNMaster(), m_infos[i], &line);
			this_folder_name = MSG_GetFolderNameFromID(m_infos[i]);
			if (this_folder_name && (XP_STRCMP(this_folder_name,name) == 0)) {

				XP_ASSERT(line.prettyName || line.name);
				XmString both = makeListItemFromLine(&line);
				DtComboBoxSelectItem(m_widget, both);
				XmStringFree(both);
				m_lastSelectedPosition = i;
				break;
			}/* if */
		}/* for i*/
	}/* if */
}



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
   ABDirListView.h -- class definition for ABDirListView
   Created: Tao Cheng <tao@netscape.com>, 14-oct-97
 */

#ifndef _XFE_ABDIRLISTVIEW_H
#define _XFE_ABDIRLISTVIEW_H

#include "MNListView.h"
#include "addrbk.h"

class XFE_ABDirListView : public XFE_MNListView
{
public:
	XFE_ABDirListView(XFE_Component *toplevel_component,
					  Widget         parent, 
					  XFE_View      *parent_view, 
					  MWContext     *context,
					  XP_List       *directories);
	virtual ~XFE_ABDirListView();

	void paneChanged(XP_Bool asynchronous,
					 MSG_PANE_CHANGED_NOTIFY_CODE notify_code,
					 int32 value);


	/* used by toplevel to see which view can handle a command.  Returns true
	 * if we can handle it. 
	 */
	virtual Boolean isCommandEnabled(CommandType command, 
									 void *calldata = NULL,
									 XFE_CommandInfo* i = NULL);

	virtual Boolean isCommandSelected(CommandType command, 
									  void *calldata = NULL,
									  XFE_CommandInfo* i = NULL);

	virtual Boolean handlesCommand(CommandType command, 
								   void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

	virtual void doCommand(CommandType command, 
						   void *calldata = NULL,
						   XFE_CommandInfo* i = NULL);

    // 
	void    expandCollapse(XP_Bool expand);
	//
#if defined(USE_ABCOM)
	void    selectContainer(AB_ContainerInfo *containerInfo);

	static int ShowPropertySheetForDirFunc(DIR_Server *server, 
										   MWContext  *context,
										   MSG_Pane   *pane,
										   XP_Bool     newDirectory);
#endif /* USE_ABCOM */
	void    selectLine(int line);
	void    selectDir(DIR_Server* dir);
	//
	void    setDirServers(XP_List *dirs);

	// columns for the Outliner
    enum {OUTLINER_COLUMN_NAME = 0,
		  OUTLINER_COLUMN_LAST
	};

	// The Outlinable interface
	void    *ConvFromIndex(int index);
	int      ConvToIndex(void *item);

	//
	char	*getColumnName(int column);
	char    *getColumnHeaderText(int column);
	fe_icon *getColumnHeaderIcon(int column);
	EOutlinerTextStyle 	getColumnHeaderStyle(int column);
	EOutlinerTextStyle 	getColumnStyle(int column);
	char    *getColumnText(int column);
	fe_icon *getColumnIcon(int column);
	
	//
	fe_icon *treeInfoToIcon(int depth, int flags, XP_Bool expanded);
	void     getTreeInfo(XP_Bool *expandable, 
						 XP_Bool *is_expanded, 
						 int *depth, 
						 OutlinerAncestorInfo **ancestor);
	//
	void     Buttonfunc(const OutlineButtonFuncData *data);
	//
	virtual void clickHeader(const OutlineButtonFuncData *);
	virtual void clickBody(const OutlineButtonFuncData *){};
	virtual void clickShiftBody(const OutlineButtonFuncData *){};
	virtual void doubleClickBody(const OutlineButtonFuncData *);
	
	void     Flippyfunc(const OutlineFlippyFuncData *data);
	//
	void     releaseLineData();
	void    *acquireLineData(int line);	

	//
    void     propertiesCB();

	//
	static const char *dirCollapse; // toggle 2 pane ui
	static const char *dirSelect; // select dir

#if defined(USE_MOTIF_DND)
	/* motif drag and drop interface. 
	 */
	fe_icon_data        *GetDragIconData(int row, int column);
	static fe_icon_data *getDragIconData(void *this_ptr,
										 int row, int column);
	
	void                 GetDragTargets(int row, int column, 
										Atom **targets, int *num_targets);
	static void          getDragTargets(void *this_ptr,
										int row, int col,
										Atom **targets, int *num_targets);

	char                *DragConvert(Atom atom);
	static char         *dragConvert(void *this_ptr,
									 Atom atom);
  
	int                  ProcessTargets(int row, int col,
										Atom *targets,
										const char **data,
										int numItems);
	static int           processTargets(void *this_ptr,
										int row, int col,
										Atom *targets,
										const char **data,
										int numItems);

	/* no instance method needed for getDropTargets 
	 */
	static void          getDropTargets(void *this_ptr,
										Atom **targets, int *num_targets);

#else
	// external entry
	static void dirListDropCallback(Widget, 
									void *, 
									fe_dnd_Event, 
									fe_dnd_Source *, 
									XEvent *);
#endif
	static void propertyCallback(DIR_Server *dir,
								 void       *callData);

protected:
#if !defined(USE_MOTIF_DND)
	// internal drop callback for addressbook drops
#if defined(USE_ABCOM)
	void        dirListDropCB(fe_dnd_Source*, XEvent *event);
#else
	void        dirListDropCB(fe_dnd_Source*);
#endif /* USE_ABCOM */
#endif

private:
	static fe_icon m_openParentIcon;
	static fe_icon m_closedParentIcon;
	static fe_icon m_pabIcon;
	static fe_icon m_ldapDirIcon;
	static fe_icon m_mListIcon;

	/* temp assume no list entry yet
	 */
	DIR_Server   *m_dir;
	DIR_Server   *m_dirLine;
	XP_List      *m_directories;
	XP_List      *m_deleted_directories;
	int           m_nDirs;

	OutlinerAncestorInfo *m_ancestorInfo;

	void propertyCB(DIR_Server *dir);

#if defined(USE_ABCOM)
	AB_ContainerInfo *m_containerLine;
	AB_ContainerInfo *m_activeContainer;
	int32             m_numLines;
#endif /* USE_ABCOM */

};
#endif /* _XFE_ABDIRLISTVIEW_H */

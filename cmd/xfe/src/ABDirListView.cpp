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

#include "ABDirPropertyDlg.h"
#include "ABDirListView.h"

#if defined(USE_MOTIF_DND)

#include "OutlinerDrop.h"

#endif /* USE_MOTIF_DND */

#include "xpgetstr.h"

const char *XFE_ABDirListView::dirCollapse = 
                              "XFE_ABDirListView::dirCollapse";
const char *XFE_ABDirListView::dirSelect = 
                              "XFE_ABDirListView::dirSelect";

// icons
fe_icon XFE_ABDirListView::m_openParentIcon = { 0 };
fe_icon XFE_ABDirListView::m_closedParentIcon = { 0 };
fe_icon XFE_ABDirListView::m_pabIcon = { 0 };
fe_icon XFE_ABDirListView::m_ldapDirIcon = { 0 };
fe_icon XFE_ABDirListView::m_mListIcon = { 0 };

extern int XFE_AB_HEADER_NAME;
extern int XFE_AB2PANE_DIR_HEADER;

#if defined(DEBUG_tao)
#define D(x) printf x
#else
#define D(x)
#endif


#define AB_DIR_OUTLINER_GEOMETRY_PREF "ab_dir.outliner_geometry"

XFE_ABDirListView::XFE_ABDirListView(XFE_Component *toplevel_component,
									 Widget         parent, 
									 XFE_View      *parent_view, 
									 MWContext     *context,
									 XP_List       *directories):
	XFE_MNListView(toplevel_component, 
				   parent_view,
				   context, 
				   (MSG_Pane *)NULL),
#if !defined(USE_ABCOM)
	m_directories(directories)
#else
	m_containerLine(NULL),
	m_activeContainer(NULL)
#endif /* USE_ABCOM */
{
	/* initialize 
	 */
	m_dir = 0;
	m_dirLine = 0;
	m_ancestorInfo = 0;
	m_deleted_directories = NULL;

	/* For outliner
	 */
	int num_columns = OUTLINER_COLUMN_LAST;
	static int column_widths[] = {23};
	m_outliner = new XFE_Outliner("dirList",
								  this,
								  toplevel_component,
								  parent,
								  False, // constantSize
								  True,  // hasHeadings
								  num_columns,
								  num_columns,// num_visible 
								  column_widths,
								  AB_DIR_OUTLINER_GEOMETRY_PREF);
	m_outliner->setHideColumnsAllowed(False);
	m_outliner->setPipeColumn(OUTLINER_COLUMN_NAME);

	/* BEGIN_3P: XmLGrid
	 */
	XtVaSetValues(m_outliner->getBaseWidget(),
				  XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				  // XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
				  XmNvisibleRows, 15,
				  NULL);
	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNcellDefaults, True,
				  XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
				  NULL);
	/* END_3P: XmLGrid
	 */

#if defined(USE_ABCOM)
	// todo: check return val
	int error = AB_CreateContainerPane(&m_pane,
									   context,
									   fe_getMNMaster());


	error = 
		AB_SetShowPropertySheetForDirFunc(m_pane, 
		   &XFE_ABDirListView::ShowPropertySheetForDirFunc);

	error = AB_InitializeContainerPane(m_pane);
	m_nDirs = MSG_GetNumLines(m_pane);
#if defined(DEBUG_tao)
	printf("\n MSG_GetNumLines, m_nDirs=%d\n", m_nDirs);
#endif

#else
	/* XFE_Outliner constructor does not allocate any content row
	 * XFE_Outliner::change(int first, int length, int newnumrows)
	 */
	if (directories) {
		m_nDirs = XP_ListCount(directories);
	}/* if */
#endif /* USE_ABCOM */

	m_outliner->change(0, m_nDirs, m_nDirs);
	m_outliner->show();
	if (m_nDirs)
		m_outliner->selectItemExclusive(0);
	setBaseWidget(m_outliner->getBaseWidget());
    XtVaSetValues(getBaseWidget(),
                  XmNpaneMinimum, 1,
                  XmNpaneMaximum, 10000,
                  NULL);
	/* initialize the icons if they haven't already been
	 */
	Pixel bg_pixel;
	XtVaGetValues(m_outliner->getBaseWidget(), XmNbackground, &bg_pixel, 0);

	if (!m_openParentIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
					   bg_pixel,
					   &m_openParentIcon,
					   NULL, 
					   oparent.width, 
					   oparent.height,
					   oparent.mono_bits, 
					   oparent.color_bits, 
					   oparent.mask_bits, 
					   FALSE);
  
	if (!m_closedParentIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
					   bg_pixel,
					   &m_closedParentIcon,
					   NULL, 
					   cparent.width, 
					   cparent.height,
					   cparent.mono_bits, 
					   cparent.color_bits, 
					   cparent.mask_bits, 
					   FALSE);
  

	if (!m_pabIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
					   bg_pixel,
					   &m_pabIcon,
					   NULL, 
					   MNC_AddressSmall.width, 
					   MNC_AddressSmall.height,
					   MNC_AddressSmall.mono_bits, 
					   MNC_AddressSmall.color_bits, 
					   MNC_AddressSmall.mask_bits, 
					   FALSE);
  

	if (!m_ldapDirIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
					   bg_pixel,
					   &m_ldapDirIcon,
					   NULL, 
					   MN_FolderServer.width, 
					   MN_FolderServer.height,
					   MN_FolderServer.mono_bits, 
					   MN_FolderServer.color_bits, 
					   MN_FolderServer.mask_bits, 
					   FALSE);
  

	if (!m_mListIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
					   bg_pixel,
					   &m_mListIcon,
					   NULL, 
					   MN_People.width, 
					   MN_People.height,
					   MN_People.mono_bits, 
					   MN_People.color_bits, 
					   MN_People.mask_bits, 
					   FALSE);
  

}

XFE_ABDirListView::~XFE_ABDirListView()
{
}

#if defined(USE_ABCOM)
int
XFE_ABDirListView::ShowPropertySheetForDirFunc(DIR_Server *server, 
											   MWContext  *context, 
											   MSG_Pane   *pane,
											   XP_Bool     newDirectory)
{
#if defined(DEBUG_tao)
	printf("\n XFE_ABDirListView::ShowPropertySheetForDirFunc, newDirectory=%d\n",
		   newDirectory);
#endif
	XFE_ABDirPropertyDlg* Dlg = 
		new XFE_ABDirPropertyDlg(CONTEXT_WIDGET(context),
								 "abDirProperties", 
								 True, 
								 context);
	Dlg->setDlgValues(server);
	Dlg->setPane(pane);
	Dlg->show();
	return 1;
}
#endif /* USE_ABCOM */

void 
XFE_ABDirListView::paneChanged(XP_Bool asynchronous,
							   MSG_PANE_CHANGED_NOTIFY_CODE notify_code,
							   int32 value)
{
#if defined(DEBUG_tao)
	printf("\nXFE_ABDirListView::paneChanged, asynchronous=%d, notify_code=%d, value=0x%x", asynchronous, notify_code, value);
#endif
}

Boolean 
XFE_ABDirListView::isCommandEnabled(CommandType command, 
									void * /* calldata */,
									XFE_CommandInfo* /* i */)
{
	AB_CommandType abCmd = (AB_CommandType)~0;
	if (command == xfeCmdABNewPAB)
		abCmd = AB_NewAddressBook;
	else if (command == xfeCmdABNewLDAPDirectory)
		abCmd = AB_NewLDAPDirectory;
	else if (command == xfeCmdABDelete)
		abCmd = AB_DeleteCmd;
	else if (command == xfeCmdABProperties)
		abCmd = AB_PropertiesCmd;
	else
		return FALSE;

	int count = 0;
	const int *indices = 0;
	m_outliner->getSelection(&indices, &count);

	XP_Bool selectable_p = False;
#if defined(USE_ABCOM)
	int error = 
		AB_CommandStatusAB2(m_pane, 
							abCmd,
							(MSG_ViewIndex *) indices,
							(int32) count,
							&selectable_p,
							NULL,
							NULL,
							NULL);
#endif /* USE_ABCOM */
	return selectable_p;
}

Boolean 
XFE_ABDirListView::isCommandSelected(CommandType /* command */, 
									 void * /* calldata */,
									 XFE_CommandInfo* /* i */)
{
	return FALSE;
}

Boolean 
XFE_ABDirListView::handlesCommand(CommandType command, 
								  void * /* calldata */,
								  XFE_CommandInfo* /* i */)
{
	if (command == xfeCmdABNewPAB ||
		command == xfeCmdABNewLDAPDirectory ||
		command == xfeCmdABDelete ||
		command == xfeCmdABProperties)
		return True;
	else
		return FALSE;
}

void 
XFE_ABDirListView::doCommand(CommandType command, 
							 void * /* calldata */,
							 XFE_CommandInfo* /* i */)
{
	AB_CommandType abCmd = (AB_CommandType)~0;
	if (command == xfeCmdABNewPAB)
		abCmd = AB_NewAddressBook;
	else if (command == xfeCmdABNewLDAPDirectory)
		abCmd = AB_NewLDAPDirectory;
	else if (command == xfeCmdABDelete)
		abCmd = AB_DeleteCmd;
	else if (command == xfeCmdABProperties)
		abCmd = AB_PropertiesCmd;
	else
		return;

	int count = 0;
	const int *indices = 0;
	m_outliner->getSelection(&indices, &count);

#if defined(USE_ABCOM)	
	int error = 
		AB_CommandAB2(m_pane, 
					  abCmd,
					  (MSG_ViewIndex *) indices,
					  (int32) count);
#endif /* USE_ABCOM */
	return;
}

void 
XFE_ABDirListView::setDirServers(XP_List *dirs)
{
	m_directories = dirs;
	if (m_directories) {
		m_nDirs = XP_ListCount(m_directories);		
	}/* if */
	else {
		m_nDirs = 0;
		m_dir = 0;
		m_dirLine = 0;
	}/* else */

	/* update UI
	 */
	m_outliner->change(0, m_nDirs, m_nDirs);
}

// The Outlinable interface
void* XFE_ABDirListView::ConvFromIndex(int /* index */)
{
	return 0;
}

int XFE_ABDirListView::ConvToIndex(void */* item */)
{
	return -1;
}

//
char *XFE_ABDirListView::getColumnName(int /* column */)
{
	return XP_GetString(XFE_AB_HEADER_NAME);
}

char *XFE_ABDirListView::getColumnHeaderText(int column)
{
  char *tmp = 0;
  switch (column) {
  case OUTLINER_COLUMN_NAME:
    tmp = XP_GetString(XFE_AB2PANE_DIR_HEADER);
    break;
  }/* switch () */

  return tmp;
}


fe_icon *XFE_ABDirListView::getColumnHeaderIcon(int /* column */)
{
	fe_icon *myIcon = 0;

#if 0
	switch (column) {
	case OUTLINER_COLUMN_FLPPY:
		myIcon = &m_openParentIcon;
		break;

	case OUTLINER_COLUMN_TYPE:
		myIcon = &m_pabIcon;
		break;
  case OUTLINER_COLUMN_NAME:
		myIcon = &m_pabIcon;
		break;
		
	}/* switch () */
#endif
	return myIcon;
}

EOutlinerTextStyle 
XFE_ABDirListView::getColumnHeaderStyle(int /* column */)
{
	return OUTLINER_Default;
}


EOutlinerTextStyle 
XFE_ABDirListView::getColumnStyle(int /* column */)
{
	return OUTLINER_Default;
}

//
fe_icon*
XFE_ABDirListView::treeInfoToIcon(int /* depth */, int /* flags */, 
								  XP_Bool /* expanded */)
{
#if 0
	if (depth < 1) {
		if (flags & MSG_FOLDER_FLAG_NEWS_HOST)
			return secure ? &newsServerSecureIcon : &newsServerIcon;
		else if (flags & MSG_FOLDER_FLAG_IMAPBOX)
			return &mailServerIcon;
		else
			return &mailLocalIcon;
	}/* if */

	if (flags & MSG_FOLDER_FLAG_NEWSGROUP)
		return &newsgroupIcon;
	else if (flags & MSG_FOLDER_FLAG_INBOX)
		return expanded ? &inboxOpenIcon : &inboxIcon;
	else if (flags & MSG_FOLDER_FLAG_DRAFTS)
		return expanded ? &draftsOpenIcon : &draftsIcon;
	else if (flags & MSG_FOLDER_FLAG_TRASH)
		return &trashIcon;
	else if (flags & MSG_FOLDER_FLAG_QUEUE)
		return expanded ? &outboxOpenIcon : &outboxIcon;
	else if (flags & MSG_FOLDER_FLAG_IMAPBOX)
		return expanded ? &folderServerOpenIcon : &folderServerIcon;
	else
		return expanded ? &folderOpenIcon : &folderIcon;
#else
	return NULL;
#endif
}

void 
XFE_ABDirListView::getTreeInfo(XP_Bool *expandable, 
							   XP_Bool *is_expanded, 
							   int *depth, 
							   OutlinerAncestorInfo **ancestor)
{
	XP_Bool is_line_expandable = False;
	XP_Bool is_line_expanded = False;
	
#if defined(USE_ABCOM)
	AB_ContainerAttribValue *value = NULL;
	int error = AB_GetContainerAttribute(m_containerLine,
										 attribNumChildren,
										 &value);
	XP_ASSERT(value && value->attrib == attribNumChildren);

	/* is_line_expandable 
	 */
	if (value->u.number)
		is_line_expandable = value->u.number?True:False;
	AB_FreeContainerAttribValue(value);

	/* depth
	 */
	if (depth) {
		error = AB_GetContainerAttribute(m_containerLine,
										 attribDepth,
										 &value);
	
		XP_ASSERT(value && value->attrib == attribDepth);
		*depth = value->u.number;
		AB_FreeContainerAttribValue(value);
	}/* if */

	/* is_line_expanded ?
	 */
	if (is_line_expandable) {
		int32 delta = MSG_ExpansionDelta(m_pane, value->u.number);
		if (delta < 0)
			is_line_expanded = True;
	}/* if */

#else
	is_line_expandable = 
		(m_dirLine->dirType == PABDirectory)?
		True:False;
	if (is_line_expandable) {
		is_line_expanded = True; // hardwired
    }
	else {
		is_line_expanded = False;
    }

	if (depth)
		*depth = (m_dirLine->dirType == PABDirectory)?0:0;
#endif
	if (ancestor)
		*ancestor = m_ancestorInfo;

	if (expandable)
		*expandable =  is_line_expandable;

	if (is_expanded)
		*is_expanded = is_line_expanded;
}

char*
XFE_ABDirListView::getColumnText(int column)
{
  char *tmp = 0;
  switch (column) {
  case OUTLINER_COLUMN_NAME:
#if defined(USE_ABCOM)
	  if (m_containerLine) {
		  AB_ContainerAttribValue *value = NULL;
		  int error = AB_GetContainerAttribute(m_containerLine,
											   attribName,
											   &value);
		  XP_ASSERT(value && value->attrib == attribName);
		  if (value->u.string)
			  tmp = XP_STRDUP(value->u.string); //todo need to free it
		  AB_FreeContainerAttribValue(value);
	  }/* if */
#else
	  if (m_dirLine &&
		  m_dirLine->description) {
		  tmp = XP_STRDUP(m_dirLine->description);
	  }/* if */
#endif /* USE_ABCOM */
	  break;
  }/* switch () */

  return tmp;

}

fe_icon*
XFE_ABDirListView::getColumnIcon(int column)
{
	fe_icon *myIcon = NULL;
#if defined(USE_ABCOM)	
	if (!m_containerLine)
		return myIcon;

	switch (column) {
	case OUTLINER_COLUMN_NAME:
		AB_ContainerAttribValue *value = NULL;
		int error = AB_GetContainerAttribute(m_containerLine,
											 attribContainerType,
											 &value);
		XP_ASSERT(value && value->attrib == attribContainerType);
		AB_ContainerType dirType = value->u.containerType; // attribContainerType
		AB_FreeContainerAttribValue(value);

		if (dirType == AB_PABContainer)
			myIcon = &m_pabIcon;
		else if (dirType == AB_LDAPContainer)
			myIcon = &m_ldapDirIcon;
		else if (dirType == AB_MListContainer)
			myIcon = &m_mListIcon;
		break;
	}/* switch */
#else
	if (!m_dirLine)
		return myIcon;

	switch (column) {
	case OUTLINER_COLUMN_NAME:
		if (m_dirLine->dirType == PABDirectory)
			myIcon = &m_pabIcon;
		else if (m_dirLine->dirType == LDAPDirectory)
			myIcon = &m_ldapDirIcon;
		else
			myIcon = &m_mListIcon;
		break;
	}/* switch */
#endif /* USE_ABCOM */
	return myIcon;	
}

//
void XFE_ABDirListView::clickHeader(const OutlineButtonFuncData *data)
{

  int column = data->column; 
  switch (column) {
  case OUTLINER_COLUMN_NAME:  
  default:
	  break;
  }/* switch() */
}

void
XFE_ABDirListView::propertiesCB()
{
  int count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
  if (count > 0 && indices) {
	  DIR_Server *dir = 
		  (DIR_Server *) XP_ListGetObjectNum(m_directories, 
											 indices[0]+1);
	  fe_showABDirPropertyDlg(getToplevel()->getBaseWidget(),
							  m_contextData,
							  dir,
							  &(XFE_ABDirListView::propertyCallback),
							  this);
  }/* if */
}

void XFE_ABDirListView::propertyCallback(DIR_Server *dir, void *callData)
{
	XFE_ABDirListView *obj = (XFE_ABDirListView *) callData;
	obj->propertyCB(dir);
}

void XFE_ABDirListView::propertyCB(DIR_Server *dir)
{	
	int which = XP_ListGetNumFromObject(m_directories, dir);
#if defined(DEBUG_tao)
	printf("\nXFE_ABDirListView::propertyCB=%d\n", which);
#endif
	if (!which) {
		/* new
		 */
		const int *selected;
		int count;		
		m_outliner->getSelection(&selected, &count);
		int pos = 0;
		if (m_nDirs > 0 && count && selected) {
			// Insert dir at position
			pos = selected[0];
			DIR_Server *prev_dir;
			prev_dir = (DIR_Server*)XP_ListGetObjectNum(m_directories,
														pos+1);
			XP_ListInsertObjectAfter(m_directories, prev_dir, dir);
			pos = pos+1;
		}
		else {
			XP_ListAddObjectToEnd(m_directories, dir);
			pos = m_nDirs+1;
		}
			
		// Repaint 
		m_nDirs = XP_ListCount(m_directories);
		m_outliner->change(0, m_nDirs, m_nDirs);
		// Set selection
		m_outliner->selectItemExclusive(pos);
		notifyInterested(XFE_ABDirListView::dirSelect, (void *) dir);
	}/* if */
	else
		m_outliner->invalidateLine(which);
		
	DIR_SaveServerPreferences(m_directories);
	if (m_deleted_directories) {
		DIR_CleanUpServerPreferences(m_deleted_directories);
		m_deleted_directories = NULL;
	}/* if */

	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void XFE_ABDirListView::doubleClickBody(const OutlineButtonFuncData *data)
{
#if defined(USE_ABCOM)
	int count = 0;
	const int *indices = 0;
	m_outliner->getSelection(&indices, &count);

	int error = 
		AB_CommandAB2(m_pane, 
					  AB_PropertiesCmd,
					  (MSG_ViewIndex *) indices,
					  (int32) count);
#else
	/* get dir
	 */
	DIR_Server *dir = 
		(DIR_Server *) XP_ListGetObjectNum(m_directories, 
										   data->row+1);

	fe_showABDirPropertyDlg(getToplevel()->getBaseWidget(),
							m_contextData,
							dir,
							&(XFE_ABDirListView::propertyCallback),
							this);
#endif /* USE_ABCOM */
}


void XFE_ABDirListView::Buttonfunc(const OutlineButtonFuncData *data)
{
  int row = data->row, 
      clicks = data->clicks;

  // focus
  notifyInterested(XFE_MNListView::changeFocus, this);

  if (row < 0) {
	  clickHeader(data);
	  return;
  } 
  else {
	  /* content row 
	   */
	  if (clicks == 2) {
		  m_outliner->selectItemExclusive(data->row);
		  doubleClickBody(data);
	  }/* clicks == 2 */
	  else if (clicks == 1) {
		  if (data->ctrl) {
				  m_outliner->toggleSelected(data->row);
		  }
		  else if (data->shift) {
			  // select the range.
			  const int *selected;
			  int count;
  
			  m_outliner->getSelection(&selected, &count);
			  
			  if (count == 0) { /* there wasn't anything selected yet. */
				  m_outliner->selectItemExclusive(data->row);
			  }/* if count == 0 */
			  else if (count == 1) {
				  /* there was only one, so we select the range from
					 that item to the new one. */
				  m_outliner->selectRangeByIndices(selected[0], data->row);
			  }/* count == 1 */
			  else {
				  /* we had a range of items selected, 
				   * so let's do something really
				   * nice with them. */
				  m_outliner->trimOrExpandSelection(data->row);
			  }/* else */
		  }/* if */
		  else {
			  m_outliner->selectItemExclusive(data->row);
			  selectLine(data->row);
		  }/* else */

		  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
	  }/* clicks == 1 */
  }/* else */

}

void XFE_ABDirListView::Flippyfunc(const OutlineFlippyFuncData */* data */)
{
}
	
//
void XFE_ABDirListView::releaseLineData()
{
	delete [] m_ancestorInfo;
	m_ancestorInfo = NULL;	
}

void *XFE_ABDirListView::acquireLineData(int line)
{
#if defined(USE_ABCOM)
	if (line < 0 ||
		!m_outliner ||
		line >= m_outliner->getTotalLines())
		return 0;
	
	AB_ContainerAttribValue *value = NULL;
	int error = AB_GetContainerAttributeForPane(m_pane,
										 line,
										 attribContainerInfo,
										 &value);
	XP_ASSERT(value && value->attrib == attribContainerInfo && value->u.container);
	m_containerLine = value->u.container;
	AB_FreeContainerAttribValue(value);

	/* ancestor
	 */
	/* depth
	 */
	error = AB_GetContainerAttribute(m_containerLine,
									 attribDepth,
									 &value);
	
	XP_ASSERT(value && value->attrib == attribDepth);
	int32 depth = value->u.number;
#if defined(DEBUG_tao_)
	printf("\nXFE_ABDirListView::acquireLineData, depth=%d\n", depth);
#endif
	AB_FreeContainerAttribValue(value);
	if (depth) 
		m_ancestorInfo = new OutlinerAncestorInfo[depth];
	else
		m_ancestorInfo = new OutlinerAncestorInfo[1];
      
	// ripped straight from the winfe
	int i = depth - 1;
	int idx = line + 1;
	int total_lines = m_outliner->getTotalLines();
	while (i >  0) {
		if ( idx < total_lines ) {
			/* depth
			 */
			error = AB_GetContainerAttribute(m_containerLine,
											 attribDepth,
											 &value);
			
			XP_ASSERT(value && value->attrib == attribDepth);
			int32 level = value->u.number;
			AB_FreeContainerAttribValue(value);

			if ( (level - 1) == i ) {
				m_ancestorInfo[i].has_prev = TRUE;
				m_ancestorInfo[i].has_next = TRUE;
				i--;
				idx++;
			}/* if */ 
			else if ( (level - 1) < i ) {
				m_ancestorInfo[i].has_prev = FALSE;
				m_ancestorInfo[i].has_next = FALSE;
				i--;
			}/* else if */
			else {
				idx++;
			}/* else */
		}/* if */ 
		else {
			m_ancestorInfo[i].has_prev = FALSE;
			m_ancestorInfo[i].has_next = FALSE;
			i--;
		}/* else */
	}/* while */

	m_ancestorInfo[0].has_prev = FALSE;
	m_ancestorInfo[0].has_next = FALSE;
	return m_containerLine;
#else
	if (line < 0 ||
		line >= m_nDirs ||
		m_directories == 0) {
		m_dirLine = 0;
		return 0;
	}/* if */
	else {
		m_dirLine = (DIR_Server *) XP_ListGetObjectNum(m_directories, 
													   line+1);
	}/* else */

	/* ancestor
	 */
	m_ancestorInfo = NULL;

	int level = (m_dirLine->dirType == PABDirectory)?1:0;

	if (level > 0) {
		m_ancestorInfo = new OutlinerAncestorInfo[level];
    }
	else {
		m_ancestorInfo = new OutlinerAncestorInfo[ 1 ];
    }
      
	// ripped straight from the winfe
	int i = level - 1;
	int idx = line + 1;
	int total_lines = m_outliner->getTotalLines();
	while ( i >  0 ) {
		if ( idx < total_lines ) {
			int level = (m_dirLine->dirType == PABDirectory)?1:2;
			//MSG_GetFolderLevelByIndex(m_pane, idx);
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

	return m_dirLine;
#endif /* !USE_ABCOM */
}

#define COLLAPSE_IS_HIDE 1
//
void XFE_ABDirListView::expandCollapse(XP_Bool expand)
{
	if (expand)
		show();
	else
		hide();
}

#if defined(USE_ABCOM)
//
void XFE_ABDirListView::selectContainer(AB_ContainerInfo *containerInfo)
{
	if (containerInfo == m_activeContainer)
		return;

	MSG_ViewIndex index = AB_GetIndexForContainer(m_pane, containerInfo);
	if (index != MSG_VIEWINDEXNONE) {
		m_outliner->scroll2Item((int) index);

		selectLine((int) index);
	}/* if */
}
#endif /* USE_ABCOM */

void XFE_ABDirListView::selectLine(int line)
{
#if defined(USE_ABCOM)
	AB_ContainerAttribValue *value = NULL;
	int error = AB_GetContainerAttributeForPane(m_pane,
												line,
												attribContainerInfo,
												&value);
	XP_ASSERT(value && value->attrib == attribContainerInfo && value->u.container);

	// set active one
	m_activeContainer = value->u.container;

	notifyInterested(XFE_ABDirListView::dirSelect, 
					 (void *) value->u.container);
	AB_FreeContainerAttribValue(value);
#else
	DIR_Server *dir = 
		(DIR_Server *) XP_ListGetObjectNum(m_directories, 
										   line+1);
	selectDir(dir);
#endif /* USE_ABCOM */
}

void XFE_ABDirListView::selectDir(DIR_Server* dir)
{
	notifyInterested(XFE_ABDirListView::dirSelect, (void *) dir);
}

#if defined(USE_MOTIF_DND)

fe_icon_data*
XFE_ABDirListView::GetDragIconData(int row, int column)
{
	D(("XFE_ABDirListView::GetDragIconData()\n"));
	/* TODO: get line data
	 * determine entry type
	 * return MNC_AddressSmall, or MN_FolderServer
	 */
	fe_icon_data *icon_data = 0;
	if (row < 0) {
#if defined(DEBUG_tao)
		printf("\n XFE_ABDirListView::GetDragIconData (row,col)=(%d,%d)\n",
			   row, column);
#endif
	}/* if */
	else {
		DIR_Server *dirLine = (DIR_Server *) XP_ListGetObjectNum(m_directories, 
																 row+1);
		if (!dirLine)
			return icon_data;

		if (dirLine->dirType == PABDirectory)
			icon_data = &MNC_AddressSmall; /* shall call make/initialize icons */
		else
			icon_data = &MN_FolderServer;
	}/* else */
	return icon_data;
}

void
XFE_ABDirListView::GetDragTargets(int    row, int column,
								  Atom **targets,
								  int   *num_targets)
{
	D(("XFE_ABDirListView::GetDragTargets(row=%d, col=%d)\n", row, column));
	
	XP_ASSERT(row > -1);
	if (row == -1) {
		*targets = NULL;
		*num_targets = 0;
	}/* if */
	else {
		if (!m_outliner->isSelected(row))
			m_outliner->selectItemExclusive(row);
		
		*num_targets = 2;
		
		*targets = new Atom[ *num_targets ];

		(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV;
		(*targets)[1] = XFE_OutlinerDrop::_XA_NETSCAPE_PAB;

	}/* else */
}

void 
XFE_ABDirListView::getDropTargets(void */*this_ptr*/,
								  Atom **targets,
								  int *num_targets)
{
	D(("XFE_ABDirListView::getDropTargets()\n"));
	*num_targets = 2;
	*targets = new Atom[ *num_targets ];

	(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV;
	(*targets)[1] = XFE_OutlinerDrop::_XA_NETSCAPE_PAB;
}

char *
XFE_ABDirListView::DragConvert(Atom atom)
{
	/* pack data
	 */
	if (atom == XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV)	{
#if defined(DEBUG_tao)
		printf("\nXFE_ABDirListView::DragConvert:_XA_NETSCAPE_DIRSERV\n");
#endif		
		
		uint32 count = 0;
		const int *indices = 0;

		m_outliner->getSelection(&indices, (int *) &count);

		char tmp[32];
		sprintf(tmp, "%d", count);

		int len = XP_STRLEN(tmp);
		char *buf = (char *) XtCalloc(len, sizeof(char));
		buf = XP_STRCAT(buf, tmp);		
		for (int i=0; i < count; i++) {
			sprintf(tmp, "%d", indices[i]);
			len += XP_STRLEN(tmp)+1;
			buf = XtRealloc(buf, len);
			buf = XP_STRCAT(buf, " ");		
			buf = XP_STRCAT(buf, tmp);					
		}/* for i */
#if defined(DEBUG_tao)
		printf("\nXFE_ABDirListView::DragConvert:_XA_NETSCAPE_DIRSERV=%x\n", buf);
#endif
		return buf;
	}/* if */
	else if (atom == XFE_OutlinerDrop::_XA_NETSCAPE_PAB)	{
#if defined(DEBUG_tao)
		printf("\nXFE_ABDirListView::DragConvert:_XA_NETSCAPE_PAB\n");
#endif		
		uint32 count = 0;
		const int *indices = 0;

		m_outliner->getSelection(&indices, (int *) &count);

		char tmp[32];
		sprintf(tmp, "%d", count);

		int len = XP_STRLEN(tmp);
		char *buf = (char *) XtCalloc(len, sizeof(char));
		buf = XP_STRCAT(buf, tmp);		
		for (int i=0; i < count; i++) {
			sprintf(tmp, "%d", indices[i]);
			len += XP_STRLEN(tmp)+1;
			buf = XtRealloc(buf, len);
			buf = XP_STRCAT(buf, " ");		
			buf = XP_STRCAT(buf, tmp);					
		}/* for i */
#if defined(DEBUG_tao)
		printf("\nXFE_ABDirListView::DragConvert:_XA_NETSCAPE_PAB=%x\n", buf);
#endif
		return buf;
	}/* else if */
	return (char *) NULL;
}

int
XFE_ABDirListView::ProcessTargets(int row, int col,
								  Atom *targets,
								  const char **data,
								  int numItems)
{
	int i;

	D(("XFE_ABDirListView::ProcessTargets(row=%d, col=%d, numItems=%d)\n", row, col, numItems));
	
	for (i=0; i < numItems; i++) {
		if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
			continue;
		
		D(("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(m_widget),targets[i]),data[i]));
		if (targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV) {
#if defined(DEBUG_tao)
			printf("\nXFE_ABDirListView::ProcessTargets:_XA_NETSCAPE_DIRSERV\n");
#endif		
			/* decode
			 */			
			char *pStr = (char *) XP_STRDUP(data[i]);
			int   len = XP_STRLEN(pStr);

			uint32 pCount = 0;
			char tmp[32];
			sscanf(data[i], "%d", &pCount);
			int *indices = (int *) XP_CALLOC(pCount, sizeof(int));
			
			char *tok = 0,
			     *last = 0;
			int   count = 0;
			char *sep = " ";

			while (((tok=XP_STRTOK_R(count?nil:pStr, sep, &last)) != NULL)&&
				   XP_STRLEN(tok) &&
				   count < len) {
				int index = atoi(tok);
				if (!count)
					XP_ASSERT(index == pCount);
				else
					indices[count-1] = 	index;
				count++;
			}/* while */
			return TRUE;
		}/* if */
		else if (targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_PAB) {
			D(("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(m_widget),targets[i]),data[i]));
#if defined(DEBUG_tao)
			printf("\nXFE_ABDirListView::ProcessTargets:_XA_NETSCAPE_PAB\n");
#endif
			return TRUE;
		}/* elss if */	
	}/* for i */
	return FALSE;
}

/* external entries
 */
fe_icon_data*
XFE_ABDirListView::getDragIconData(void *this_ptr,
								   int   row,
								   int   column)
{
	XFE_ABDirListView *view = (XFE_ABDirListView*)this_ptr;	
	return view->GetDragIconData(row, column);
}

void
XFE_ABDirListView::getDragTargets(void  *this_ptr,
								  int    row, int column,
								  Atom **targets, int *num_targets)
{
	XFE_ABDirListView *view = (XFE_ABDirListView*)this_ptr;	
	view->GetDragTargets(row, column, targets, num_targets);
}

char *
XFE_ABDirListView::dragConvert(void *this_ptr,
							   Atom atom)
{
  XFE_ABDirListView *view = (XFE_ABDirListView*) this_ptr;
  
  return view->DragConvert(atom);
}

int
XFE_ABDirListView::processTargets(void *this_ptr,
								  int row, int col,
								  Atom *targets,
								  const char **data,
								  int numItems)
{
	XFE_ABDirListView *view = (XFE_ABDirListView*)this_ptr;
	
	return view->ProcessTargets(row, col, targets, data, numItems);
}

#else /* USE_MOTIF_DND */

// Address field drop site handler
void
XFE_ABDirListView::dirListDropCallback(Widget, 
									   void          *cd,
									   fe_dnd_Event   type,
									   fe_dnd_Source *source,
									   XEvent        */* event */) 
{
    XFE_ABDirListView *ad = (XFE_ABDirListView *)cd;
    
    if (type == FE_DND_DROP && 
		ad && 
		source)
#if defined(USE_ABCOM)
        ad->dirListDropCB(source, event);
#else
        ad->dirListDropCB(source);
#endif
}

#if defined(USE_ABCOM)
void
XFE_ABDirListView::dirListDropCB(fe_dnd_Source *source, XEvent *event)
{
#if defined(DEBUG_tao)
	printf("\nXFE_ABDirListView::dirListDropCB:srcType=%d\n",
		   source->type);
#endif
	XP_ASSERT(source && event && m_outliner);

	unsigned int state = event->xbutton.state;
    XP_Bool shift = ((state & ShiftMask) != 0),
		    ctrl = ((state & ControlMask) != 0);


	/* onto which 
	 */
	int row = -1;
	int x, y;

	m_outliner->translateFromRootCoords(event->xbutton.x_root, 
										event->xbutton.y_root, 
										&x, &y);
	
	row = m_outliner->XYToRow(x, y);
#if defined(DEBUG_tao)
	printf("\nXFE_ABDirListView::entryListDropCallback,shift=%d, ctrl=%d, row=%d",
		   shift, ctrl, row);
#endif
	if (row < 0 ||
		row >= m_outliner->getTotalLines())
		XP_ASSERT(0);

	AB_ContainerInfo *containerInfo = 
		AB_GetContainerForIndex(m_pane, (MSG_ViewIndex) row);

    switch (source->type) {
    case FE_DND_ADDRESSBOOK:
	case FE_DND_BOOKS_DIRECTORIES: {
		XFE_MNListView* listView = (XFE_MNListView *) source->closure;
		XFE_Outliner *outliner = listView->getOutliner();
		const int *indices = NULL;
		int32 numIndices = 0;
		outliner->getSelection(&indices, &numIndices);
		MSG_Pane *srcPane = listView->getPane();
		AB_DragEffect effect = 
			AB_DragEntriesIntoContainerStatus(srcPane,
											  (const MSG_ViewIndex *) indices,
											  (int32) numIndices,
											  containerInfo,
											  AB_Default_Drag); 
		int error = 0;
		if (effect == AB_Drag_Not_Allowed)
			return;
		else
			error = AB_DragEntriesIntoContainer(srcPane,
												(const MSG_ViewIndex *) indices,
												(int32) numIndices,
												containerInfo,
												effect);
	}
	
	break;
		
    default:
        break;
    }/* switch */
}

#else
void
XFE_ABDirListView::dirListDropCB(fe_dnd_Source *)
{
}
#endif 
#endif /* USE_MOTIF_DND */

#if defined(USE_ABCOM)

#endif /* USE_ABCOM */

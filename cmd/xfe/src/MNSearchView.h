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
   MNSearchView.h -- class definitions for mail/news search view.
   Created: Dora Hsu <dora@netscape.com>, 15-Dec-96.
 */



#ifndef _xfe_mnsearchview_h
#define _xfe_mnsearchview_h

#include "MNListView.h"
#include "Outliner.h"
#include "Command.h"
#include "msg_srch.h"
#include "Menu.h"       // for struct MenuSpec

class XFE_MsgFrame;
class XFE_Dashboard;
class XFE_FolderDropdown;
class XFE_PopupMenu;
class XFE_Button;



typedef 
struct XFE_SearchElement
{
  Widget form;       /* container */
  Widget label;	     /* label */
  Widget attr_opt;   /* attribute option */
  Widget attr_pop;   /* attribute option */
  Widget op_opt;   /* operator option */
  Widget op_pop;   /* operator option */
  Widget value;      /* value option */

  Dimension width;    /* width  of the row */
  Dimension height;   /* height of the row */

  MSG_ScopeAttribute scope;
  MSG_SearchAttribute attr;
  MSG_SearchOperator  op;
  MSG_SearchValueWidget type;
  int32 value_option;  /* for the MSG value */
  char *textValue;

} XFE_SearchElement;

typedef struct XFE_SearchRules
{
  /* Top Level Layout */
  Widget searchLabel;
  XFE_FolderDropdown *scopeOpt;
  Widget scopeOptW;
  Widget whereLabel;
  Widget searchCommand;
  Widget more_btn;
  Widget less_btn;

  Widget commandGroup;

  Widget frame;
  Widget form;

  MSG_ScopeAttribute scope;

} XFE_SearchRules;

typedef struct XFE_SearchResult
{
  Widget container;
  Boolean expand;
} XFE_SearchResult;

class XFE_MNSearchView: public XFE_MNListView
{
public:

	XFE_MNSearchView(XFE_Component *toplevel_component,
					 Widget parent,
					 XFE_Frame * parent_frame,
					 XFE_MNView *mn_parentView,
					 MWContext *context, MSG_Pane *p, 
					 MSG_FolderInfo *selectedFolder = NULL, 
					 Boolean AddGotoBtn = False);

  virtual ~XFE_MNSearchView();

  static const char* CloseSearch;
  static const char* STOP;
  static const char* SEARCH;
  static const int kStop;
  static const int kSearch;

  static const int MAILSEARCH_OUTLINER_COLUMN_SUBJECT;
  static const int MAILSEARCH_OUTLINER_COLUMN_SENDER;
  static const int MAILSEARCH_OUTLINER_COLUMN_DATE;
  static const int MAILSEARCH_OUTLINER_COLUMN_PRIORITY;
  static const int MAILSEARCH_OUTLINER_COLUMN_LOCATION;

  static const char *scopeChanged;

  virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

  virtual void doViewLayout();
  virtual void doLayout();

  /* Outlinable interface methods */
  virtual void *ConvFromIndex(int index);
  virtual int ConvToIndex(void *item);

  virtual char *getColumnName(int column);

  virtual char *getColumnHeaderText(int column);
  virtual fe_icon *getColumnHeaderIcon(int column);
  virtual EOutlinerTextStyle getColumnHeaderStyle(int column);
 
  virtual void *acquireLineData(int line);
 
  virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth
,
                           OutlinerAncestorInfo **ancestor);
  virtual EOutlinerTextStyle getColumnStyle(int column);
  virtual char *getColumnText(int column);
  virtual fe_icon *getColumnIcon(int column);
 
  virtual void releaseLineData();
 
  virtual void Buttonfunc(const OutlineButtonFuncData *data);
  virtual void Flippyfunc(const OutlineFlippyFuncData *data);
  virtual void createWidgets(Widget formParent);

  virtual void toggleActionButtonState(Boolean on);

  static void  startCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  newCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  closeCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  miscCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  moreCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  lessCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  expandCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  gotoFolderCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  deleteMsgsCallback(Widget w, XtPointer clientData, XtPointer callData);
  static void  exposeOutlinerHandler(Widget, XtPointer, XEvent*, Boolean*);
  
  void  stopSearch();
  void  setFolderOption(MSG_FolderInfo *folderInfo);
  void  gotoFolder(Widget w);
  void  deleteMsgs();


  XFE_CALLBACK_DECL(searchFinished)
  XFE_CALLBACK_DECL(searchStart)
protected:
  virtual void 	createCommandButtons();
  virtual void  addDefaultFolders();
  virtual void  buildHeaderOption();
  virtual void  prepSearchScope();
  virtual void  buildResultTable();
  virtual void  clickHeader(const OutlineButtonFuncData *data);
  virtual void  singleClick(const OutlineButtonFuncData *data);
  virtual void  doubleClick(const OutlineButtonFuncData *data);
  virtual void  miscCmd();
  virtual void  handleClose();
  virtual void  buildResult();

  void  createResultArea();
  
  // Protected data member to be shared with child class

  Visual*   m_visual;
  Colormap  m_cmap;
  Cardinal  m_depth;
  Widget    m_content;
  Widget    m_header;
  Widget    m_displayArea;
  Boolean   m_allocated;
  Boolean   m_hasRightLabel;
  Widget    m_searchBtn; 		/* Search Btn */
  Widget    m_newSearchBtn; 	/* New Search Btn */
  Widget    m_miscBtn; 		/* will be save btn or help btn */
  Widget    m_closeBtn; 		/* Close Btn */
  Boolean   m_hasResult;
  XP_Bool   m_booleanAnd;       /* search all vs search any */

  int 		m_count ;
  /* Top Level Layout */
  XFE_SearchRules   m_rules;
  XFE_SearchResult m_result;
  MSG_ResultElement *m_resultLine;
  MSG_Pane 	*m_searchpane;
  XFE_MsgFrame	*m_msgFrame;
  Widget    m_ruleContent;
  Boolean	m_searchAll;
  MSG_FolderInfo *m_folderInfo;
  XFE_Dashboard *m_dashboard;
  DIR_Server 	*m_dir;
  XFE_Frame * m_parentFrame;

private:

  void initialize();
  void 	createMoreLess(Widget);
  void  createDisplayArea();
  void  startSearch(Widget w);
  void  resetSearch();


  void  createMiniDashboard(Widget dashboardParent);
  int   getNumFolderOptions();

  void  updateUI(Boolean allowSearch);
  void  updateResultCount(int count);
  void  setResultSensitive(Boolean sens);
  
  void 	addRules(char *title, 
		MSG_ScopeAttribute  curScope, 
		uint16 curAttr);

  void  expand();
  void  doExpand(Boolean doit);
  void  moreRules();
  void  lessRules();
  void  setAllBooleans(Boolean andP);
  void  setOneBoolean(int i);
  void  newSearch();
  void  turnOnStop(Widget w, Boolean turnOn);
  void  addFolderOption();
  char*	getNameFromScope(MSG_ScopeAttribute scope_num);
  Boolean isOnStop(Widget w);
  Boolean isMailFolder(MSG_FolderInfo* folderInfo);
  XFE_CALLBACK_DECL(folderSelected)
  void  selectFolder(MSG_FolderInfo *);
  static void andOrCB(Widget w, XtPointer, XtPointer);

  Widget m_gotoBtn;
  XFE_Button* m_fileBtn;
  Widget m_deleteBtn;
  Boolean m_addGotoBtn;
  XFE_PopupMenu *m_popup;

  static MenuSpec popup_spec[];
};

#endif


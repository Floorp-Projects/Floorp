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
   ABListSearchView.h -- view of user's mailfilters.
   Created: Tao Cheng <tao@netscape.com>, 17-dec-96
 */

#ifndef _ABLISTSEARCHVIEW_H_
#define _ABLISTSEARCHVIEW_H_

#include "rosetta.h"
#include "MNListView.h"
#include "ABSearchView.h"
#include "PopupMenu.h"

#include "Button.h"
#include "addrbk.h"

typedef enum {AB_BOOK = 0,
      AB_PICKER
} eABViewMode;

class XFE_ABListSearchView: public XFE_MNListView 
{
public:
  XFE_ABListSearchView(XFE_Component *toplevel_component, 
					   Widget         parent,
					   XFE_View      *parent_view, 
					   MWContext     *context,
					   XP_List       *directories);
  virtual ~XFE_ABListSearchView();

  //
  Widget makeFilterBox(Widget parent, XP_Bool stopBtn);

  // user/list functions
  void undo();
  void redo();
  void abDelete();
  void abDeleteAllEntries();
  void newUser();
  void newList();

  // Get and pack selection
  ABAddrMsgCBProcStruc* getSelections();

  //
  virtual void listChangeFinished(XP_Bool asynchronous,
								  MSG_NOTIFY_CODE notify, 
								  MSG_ViewIndex where, int32 num);

  virtual void paneChanged(XP_Bool asynchronous, 
						   MSG_PANE_CHANGED_NOTIFY_CODE notify_code, 
						   int32 value);

  void dirChanged(int32 value);

  //
  void changeEntryCount();
  XFE_Outliner *getOutliner() { return m_outliner;}

  //
  void    expandCollapse(XP_Bool expand);
  //
  void    selectLine(int line);
  void    selectDir(DIR_Server* dir);
#if defined(USE_ABCOM)
  void    selectContainer(AB_ContainerInfo *containerInfo);
  void    setContainerPane(MSG_Pane *pane);

  static AB_EntryType getType(AB_ContainerInfo *container, ABID id);
  static AB_EntryType getType(MSG_Pane *abPane, MSG_ViewIndex index);

  static int ShowPropertySheetForEntryFunc(MSG_Pane  *pane,
										   MWContext *context);
#endif /* USE_ABCOM */

	//

  DIR_Server *getDir() { return m_dir;}
  ABook      *getAddrBook() { return m_AddrBook;}
  ABPane     *getABPane() { return m_abPane;}

  virtual void stopSearch();
  XP_Bool      isSearching() { return m_searchingDir;}

  // Sort
#if defined(USE_ABCOM)
  void           setSortType(AB_AttribID id);
  AB_AttribID    getSortType();
#else
  void           setSortType(AB_CommandType type);
  ABID           getSortType();
#endif /* USE_ABCOM */
  XP_Bool        isAscending();
  void           setAscending(XP_Bool as);


  /* this method is used by the toplevel to sensitize menu/toolbar items. 
   */
  virtual Boolean isCommandEnabled(CommandType cmd, 
								   void *calldata = NULL,
								   XFE_CommandInfo* m = NULL);

  /* used by toplevel to see which view can handle a command.  Returns true
     if we can handle it. */
  virtual Boolean handlesCommand(CommandType cmd, 
								 void *calldata = NULL, 
								 XFE_CommandInfo* i = NULL);

  /* this method is used by the toplevel to dispatch a command. */
  virtual void doCommand(CommandType cmd, 
						 void *calldata = NULL,
						 XFE_CommandInfo* i = NULL);

  // The Outlinable interface.
  virtual void    *ConvFromIndex(int index);
  virtual int      ConvToIndex(void *item);

  // columns for the Outliner
  enum {
	OUTLINER_COLUMN_TYPE = 0,
	OUTLINER_COLUMN_NAME,
	OUTLINER_COLUMN_EMAIL,
	OUTLINER_COLUMN_COMPANY,
	OUTLINER_COLUMN_PHONE,
	OUTLINER_COLUMN_NICKNAME,
	OUTLINER_COLUMN_LOCALITY,
	OUTLINER_COLUMN_LAST
  };

  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */);
  virtual char *getCellDocString(int /* row */, int /* column */);

  //
  // The Outlinable interface.
  //
  virtual EOutlinerTextStyle 
                   getColumnHeaderStyle(int column);
  virtual char    *getColumnName(int column);
  virtual char    *getColumnHeaderText(int column);
  virtual char    *getColumnText(int column);
  virtual fe_icon *getColumnIcon(int column);

  //
  virtual fe_icon *getColumnHeaderIcon(int column);
  virtual EOutlinerTextStyle 
                   getColumnStyle(int column);

  //
  virtual void     getTreeInfo(XP_Bool *expandable, 
			       XP_Bool *is_expanded, 
			       int *depth, 
			       OutlinerAncestorInfo **ancestor);
  //
  virtual void     Buttonfunc(const OutlineButtonFuncData *data);

  virtual void     Flippyfunc(const OutlineFlippyFuncData *data);
  //
  virtual void     releaseLineData();
  virtual void    *acquireLineData(int line);

  //
  // Sub-Dialog Windows
  void propertiesCB();
  void editProperty();
  void popupUserPropertyWindow(ABID entry, XP_Bool newuser,
			       XP_Bool modal);
  void popupListPropertyWindow(ABID entry, XP_Bool newuser,
			       XP_Bool modal);


  // callbacks
  static void resizeCallback(Widget, XtPointer, XtPointer);

  static void typeDownTimerCallback(XtPointer closure, XtIntervalId *);
  static void typeActivateCallback(Widget, XtPointer, XtPointer);
  static void typeDownCallback(Widget, XtPointer, XtPointer);
  static void comboSelCallback(Widget, XtPointer, XtPointer);
  static void searchCallback(Widget, XtPointer, XtPointer);

  static void expandCallback(Widget, XtPointer, XtPointer);
  //
  static void searchDlgCB(ABSearchInfo_t *clientData, void *callData);
  //
  // callback to be notified when allconnectionsComplete
  //
  virtual void allConnectionsComplete(MWContext  *context);

  void unRegisterInterested();
  XFE_CALLBACK_DECL(allConnectionsComplete)

  void setLdapDisabled(XP_Bool b);
  //
  static const char *dirExpand; // toggle 2 pane ui
  static const char *dirsChanged; // 
  static const char *dirSelect; // select dir

  // icons for the outliner
  static fe_icon m_personIcon;
  static fe_icon m_listIcon;
  HG82719

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
  static void entryListDropCallback(Widget, 
									void*, fe_dnd_Event, fe_dnd_Source*, 
									XEvent*);
#endif

protected:
#if !defined(USE_MOTIF_DND)
  // internal drop callback for addressbook drops
#if defined(USE_ABCOM)
  void        entryListDropCB(fe_dnd_Source*, XEvent *event);
#else
  void        entryListDropCB(fe_dnd_Source*);
#endif /* USE_ABCOM */

#endif
  void        resizeCB(Widget, XtPointer);

  void    startSearch(ABSearchInfo_t *clientData);
  int     addToAddressBook();
  void    idToPerson(DIR_Server *pABDir, ABID id, PersonEntry* pPerson);

  XP_Bool m_ldapDisabled;

  //
  virtual void clickHeader(const OutlineButtonFuncData *);
  virtual void clickBody(const OutlineButtonFuncData *){};
  virtual void clickShiftBody(const OutlineButtonFuncData *){};
  virtual void doubleClickBody(const OutlineButtonFuncData *){};

  // callbacks
  virtual void typeDownCB(Widget w, XtPointer callData);
  virtual void comboSelCB(Widget w, XtPointer callData);
  virtual void searchCB(Widget w, XtPointer callData);
  virtual void searchDlg(Widget w, XtPointer callData);

  // UI
  void refreshCombo();
  void layout();

  Widget        m_filterSearchBtn;
  Widget        m_filterStopBtn;
  Widget        m_filterDirCombo;

  Widget        m_filterBoxForm;	
  Widget        m_collapsedForm;
  Widget        m_filterPrompt;
  //
  XP_Bool       m_searchingDir;
  char         *m_searchStr;

  XP_List      *m_directories;
  DIR_Server   *m_dir;

#if defined(USE_ABCOM)
  AB_ContainerInfo *m_containerInfo;
  MSG_Pane         *m_abContainerPane;
  uint32           *m_pageSize;

  int               m_numAttribs;
#endif /* USE_ABCOM */

  ABook        *m_AddrBook; 
  ABPane       *m_abPane;

  ABID          m_entryID; /* entryID for current line */
  MSG_ViewIndex m_dataIndex;

#if defined(USE_ABCOM)
	AB_AttribID m_sortType;
#else
  unsigned long m_sortType;
#endif /* USE_ABCOM */
  XP_Bool       m_ascending;

private:
  // no need to reinvent the wheel if existing one rolls smoothly
  Widget        m_expandBtn;
	
  ABSearchInfo_t *m_searchInfo;

  /* trigger search when time elapse
   */
  XtIntervalId    m_typeDownTimer;	
  MSG_ViewIndex   m_typeDownIndex;
  XFE_PopupMenu *m_popup;
  static MenuSpec view_popup_spec[];
};

#endif

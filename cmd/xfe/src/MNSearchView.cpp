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
   MNSearchView.h -- user's mail search view.
   Created: Dora Hsu <dora@netscape.com>, 15-Dec-96.
*/


#include "MozillaApp.h"
#include "MNSearchView.h"
#include "AdvSearchDialog.h"
#include "SearchRuleView.h"
#include "MsgFrame.h"
#include "MsgView.h"
#include "ViewGlue.h"
#include "FolderDropdown.h"
#include "FolderMenu.h"
#include "Dashboard.h"
#include "xfe2_extern.h"
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/SelectioB.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/DrawnB.h>
#include "Xfe/Xfe.h"

#include "libi18n.h"
#include "intl_csi.h"
#include "felocale.h"

#ifdef DEBUG_dora
#define DD(x) x
#else
#define DD(x)
#endif

#define OUTLINER_GEOMETRY_PREF "mail.searchpane.outliner_geometry"

// Maximum number of rules can be created
#define MAX_RULES_ALLOWED 5
extern "C" Widget
fe_make_option_menu(Widget toplevel, 
	Widget parent, char* widgetName, Widget *popup);
extern "C" void
fe_get_option_size ( Widget optionMenu, 
                        Dimension *retWidth, Dimension *retHeight );

#define TOTAL_DEFAULT_FOLDER_OPTIONS	3

const char* XFE_MNSearchView::CloseSearch	= "XFE_MNSearchView::CloseSearch";
const char* XFE_MNSearchView::STOP	= "Stop";
const char* XFE_MNSearchView::SEARCH	= "Search";

const int XFE_MNSearchView::kStop	= 0;
const int XFE_MNSearchView::kSearch	= 1;

// For Column Header to use
const int XFE_MNSearchView::MAILSEARCH_OUTLINER_COLUMN_SUBJECT= 0;
const int XFE_MNSearchView::MAILSEARCH_OUTLINER_COLUMN_SENDER= 1;
const int XFE_MNSearchView::MAILSEARCH_OUTLINER_COLUMN_DATE= 2;
const int XFE_MNSearchView::MAILSEARCH_OUTLINER_COLUMN_PRIORITY= 3;
const int XFE_MNSearchView::MAILSEARCH_OUTLINER_COLUMN_LOCATION= 4;
const char *XFE_MNSearchView::scopeChanged = "XFE_MNSearchView::scopeChanged";

#include "xpgetstr.h"

extern int XFE_SEARCH_LOCATION;
extern int XFE_SEARCH_SUBJECT;
extern int XFE_SEARCH_SENDER;
extern int XFE_SEARCH_DATE;
extern int XFE_SEARCH_ALLMAILFOLDERS;
extern int XFE_SEARCH_AllNEWSGROUPS;
extern int XFE_SEARCH_LDAPDIRECTORY;
extern int XFE_AB_STOP;
extern int XFE_AB_SEARCH;
extern int XFE_PRI_PRIORITY;
extern int XFE_SEARCH_NO_MATCHES;
extern int XFE_VIRTUALNEWSGROUP;
extern int XFE_VIRTUALNEWSGROUPDESC;

MenuSpec XFE_MNSearchView::popup_spec[] = {
  { "fileSubmenu", DYNA_CASCADEBUTTON, NULL, NULL, False,
    (void*)xfeCmdMoveMessage, XFE_FolderMenu::generate },
  { "copySubmenu", DYNA_CASCADEBUTTON, NULL, NULL, False,
    (void*)xfeCmdCopyMessage, XFE_FolderMenu::generate },
  { xfeCmdDeleteMessage,	PUSHBUTTON },
  { xfeCmdSaveMessagesAs,	PUSHBUTTON },
  { NULL }
};

XFE_MNSearchView::XFE_MNSearchView(XFE_Component *toplevel_component,
								   Widget parent,
								   XFE_Frame * parent_frame,
								   XFE_MNView *mn_parentView,
								   MWContext *context, MSG_Pane *p, 
								   MSG_FolderInfo *selectedFolder,
								   Boolean AddGotoBtn)
  : XFE_MNListView(toplevel_component, mn_parentView, context, p)
{
	m_parentFrame = parent_frame;

  // Initialize private member now...
  initialize();
  m_addGotoBtn = AddGotoBtn;

  if ( selectedFolder)
  {
    if( !isMailFolder(selectedFolder) )
	m_rules.scope = scopeNewsgroup;

    m_folderInfo = selectedFolder;
  }

  m_hasRightLabel = False;

  {
  /* Get Visual, Colormap, and Depth */
  XtVaGetValues(getToplevel()->getBaseWidget(),
                 XmNvisual, &m_visual,
                 XmNcolormap, &m_cmap,
                 XmNdepth, &m_depth,
                 NULL);
  }
  {
     Widget w;

     w = XtVaCreateWidget("searchForm", xmFormWidgetClass, parent,
			XmNautoUnmanage, False, 
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			XmNtopAttachment, XmATTACH_FORM,
			NULL);
     setBaseWidget(w);
  }

  if (!p) // Create SearchPane here
  {
	m_pane = MSG_CreateSearchPane(m_contextData, fe_getMNMaster());
	setPane(m_pane);
  }
  getToplevel()->registerInterest(
		XFE_Frame::allConnectionsCompleteCallback, this,
                (XFE_FunctionNotification)searchFinished_cb);

  getToplevel()->registerInterest(
		XFE_SearchRuleView::activateSearch, this,
		(XFE_FunctionNotification)searchStart_cb);
}


XFE_MNSearchView::~XFE_MNSearchView()
{
  getToplevel()->unregisterInterest(
		XFE_Frame::allConnectionsCompleteCallback, this,
                (XFE_FunctionNotification)searchFinished_cb);

  getToplevel()->unregisterInterest(
		XFE_SearchRuleView::activateSearch, this,
		(XFE_FunctionNotification)searchStart_cb);

  if (m_popup)
    delete m_popup;

  destroyPane();
}

void
XFE_MNSearchView::initialize()
{
  // Initialize private data member here
  m_searchpane = NULL;
  m_count = 0;
  m_visual = NULL;
  m_depth = 0;
  m_content = NULL;
  m_header = NULL;
  m_displayArea = NULL;
  m_popup = NULL;
  m_ruleContent = NULL;
  m_allocated = False;
  m_hasRightLabel = False;
  m_searchBtn = NULL;
  m_newSearchBtn = NULL;
  m_miscBtn = NULL;
  m_closeBtn = NULL;
  m_folderInfo = NULL;
  m_hasResult = False;
  m_searchAll = False;
  m_gotoBtn = NULL;
  m_fileBtn = NULL;
  m_deleteBtn = NULL;
  m_addGotoBtn = False;

  // XFE_SearchResult struct
  m_rules.searchLabel = NULL;
  m_rules.scopeOpt = NULL;
  m_rules.whereLabel = NULL;
  m_rules.searchCommand = NULL;
  m_rules.more_btn = NULL;
  m_rules.less_btn = NULL;
  m_rules.commandGroup = NULL;
  m_rules.frame = NULL;
  m_rules.form = NULL;
  m_rules.scope = (MSG_ScopeAttribute)scopeMailFolder;
  m_dir = NULL;

  // Search Result

  m_result.container = NULL;
  m_result.expand = False;

  // bstell: initialize doc_csid
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_contextData);
  INTL_SetCSIDocCSID(c, fe_LocaleCharSetID);
  INTL_SetCSIWinCSID(c, INTL_DocToWinCharSetID(INTL_GetCSIDocCSID(c)));
}

Boolean
XFE_MNSearchView::handlesCommand(CommandType cmd, void *calldata,
                                 XFE_CommandInfo*)
{
#define IS_CMD(command) cmd == (command)
  if (IS_CMD(xfeCmdMoveMessage)
      || IS_CMD(xfeCmdCopyMessage)
      || IS_CMD(xfeCmdDeleteMessage)
      || IS_CMD(xfeCmdSaveMessagesAs)
      || IS_CMD(xfeCmdShowPopup)
      )
      return TRUE;
  else
    {
      return XFE_MNListView::handlesCommand(cmd, calldata);
    }
#undef IS_CMD
}

Boolean
XFE_MNSearchView::isCommandEnabled(CommandType cmd, void *calldata,
                                   XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)

    if (IS_CMD(xfeCmdShowPopup))
        return (m_outliner != 0);

    const int *selected;
    int count;
    m_outliner->getSelection(&selected, &count);

    if (IS_CMD(xfeCmdMoveMessage)
        || IS_CMD(xfeCmdCopyMessage)
        || IS_CMD(xfeCmdDeleteMessage)
        || IS_CMD(xfeCmdSaveMessagesAs)
        )
        return (count > 0);

    else
        return XFE_MNListView::isCommandEnabled(cmd, calldata, info);
}

void
XFE_MNSearchView::doCommand(CommandType cmd,
                            void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
	if (IS_CMD(xfeCmdShowPopup) && m_outliner != 0)
    {
		XEvent *event = info->event;
		int x, y, clickrow;
		Widget w = XtWindowToWidget(event->xany.display, event->xany.window);
		
		if (m_popup)
			delete m_popup;

		if (w == NULL) w = m_widget;
		
		m_popup = new XFE_PopupMenu("popup",(XFE_Frame*)m_toplevel, // XXXXXXX
						XfeAncestorFindApplicationShell(w));
		
        m_popup->addMenuSpec(popup_spec);

		m_outliner->translateFromRootCoords(event->xbutton.x_root,
											event->xbutton.y_root,
											&x, &y);
      
		clickrow = m_outliner->XYToRow(x, y);
		
		if (clickrow != -1) /* if it was actually in the outliner's content rows. */
        {
            if (! m_outliner->isSelected(clickrow))
                m_outliner->selectItemExclusive(clickrow);
        }

		m_popup->position(event);
		m_popup->show();
    }

    else if (IS_CMD(xfeCmdMoveMessage) || IS_CMD(xfeCmdCopyMessage))
    {
        MSG_FolderInfo *info = (MSG_FolderInfo*)calldata;
		  
        if (info)
        {
            const int *selected;
            int count;
            m_outliner->getSelection(&selected, &count);
            if (IS_CMD(xfeCmdCopyMessage))
                MSG_CopyMessagesIntoFolder(m_pane, (MSG_ViewIndex*)selected,
                                           count, info);
            else
                MSG_MoveMessagesIntoFolder(m_pane, (MSG_ViewIndex*)selected,
                                           count, info);
        }
    }
    else if (IS_CMD(xfeCmdDeleteMessage))
        deleteMsgs();

    else
        XFE_MNListView::doCommand(cmd, calldata, info);
}

void
XFE_MNSearchView::addRules(char *title, 
			MSG_ScopeAttribute  curScope, 
			uint16 curAttr)
{
	XFE_SearchRuleView *ruleView;
	void* folder_data = (void*)m_folderInfo;

   	if (curScope == scopeLdapDirectory)
	   folder_data = (void*)m_dir;
	ruleView = new XFE_SearchRuleView(getToplevel(),
					m_ruleContent,
					this,
					m_contextData,
					getPane(),
					title,
				        folder_data,
					curScope,
					curAttr);
	
	addView(ruleView);
}

void
XFE_MNSearchView::createCommandButtons()
{

  Arg av[20];
  int ac = 0;

  ac = 0;
  XtSetArg (av[ac], XmNnumColumns, 1); ac++;
  XtSetArg (av[ac], XmNorientation, XmVERTICAL); ac++;
  XtSetArg (av[ac], XmNadjustLast, False); ac++;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_NONE); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg (av [ac], XmNresizable, False); ac++;
  m_rules.searchCommand = XmCreateRowColumn(m_content, "searchRC", av, ac);

  ac = 0;
  m_searchBtn = XmCreatePushButton(m_rules.searchCommand, "SearchBtn", av, ac);
  m_miscBtn = XmCreatePushButton(m_rules.searchCommand, "MiscBtn", av, ac);
  m_closeBtn = XmCreatePushButton(m_rules.searchCommand, "CloseBtn", av, ac);

  XtVaSetValues( m_content,
                XmNdefaultButton, NULL,
                NULL);

  XtAddCallback (m_searchBtn, XmNactivateCallback, startCallback, this);
  XtAddCallback(m_miscBtn, XmNactivateCallback, miscCallback, this);
  XtAddCallback (m_closeBtn, XmNactivateCallback, closeCallback, this);

  XtManageChild(m_searchBtn);
  XtManageChild(m_miscBtn);
  XtManageChild(m_closeBtn);
  XtManageChild(m_rules.searchCommand);

  // we have not start search, so don't turn on the stop button yet...
  XtVaSetValues(m_searchBtn, XmNuserData, kSearch, 0 );
}

//fe_search_get_name_from_scope(MSG_ScopeAttribute scope_num)
char * 
XFE_MNSearchView::getNameFromScope(MSG_ScopeAttribute scope_num)
{
    char *str = NULL;

    switch (scope_num)
    {
        case scopeMailFolder:
        {
                str = (char*)malloc(120);
                strcpy(str,XP_GetString(XFE_SEARCH_ALLMAILFOLDERS));
                break;
        }
        case scopeNewsgroup:
        {
                str = (char*)malloc(120);
                strcpy(str,XP_GetString(XFE_SEARCH_AllNEWSGROUPS));
                break;
        }
        case scopeLdapDirectory:
        {
                str = (char*)malloc(120);
                strcpy(str,XP_GetString(XFE_SEARCH_LDAPDIRECTORY));
                break;
        }
		default : // Do we handle scopeAllSearchableGroups, 
				// scopeOfflineNewsgroup ???
		break;
    }

    return str;
}

int
XFE_MNSearchView::getNumFolderOptions()
{
 return TOTAL_DEFAULT_FOLDER_OPTIONS;
}

void
XFE_MNSearchView::addDefaultFolders()
{
}

Boolean
XFE_MNSearchView::isMailFolder(MSG_FolderInfo *folderInfo)
{
  MSG_FolderLine folderLine;
  MSG_GetFolderLineById(XFE_MNView::getMaster(), folderInfo, &folderLine);

DD(
  if (folderLine.name) 
	printf("select Folder with name....%s\n", folderLine.name);
  else printf("select Folder has no name...\n");
  if (folderLine.prettyName)
	printf("select Folder with pretty name (%s)\n", folderLine.prettyName);
  else printf("select Folder has no pretty name...\n");
)
 
  if (folderLine.flags & 
	  (MSG_FOLDER_FLAG_NEWSGROUP | MSG_FOLDER_FLAG_NEWS_HOST))
	return False;
  return True;
}

void
XFE_MNSearchView::setFolderOption(MSG_FolderInfo *folderInfo)
{
   if ( m_folderInfo == folderInfo ) return;
   m_rules.scopeOpt->selectFolder(folderInfo, True);
}

void 
XFE_MNSearchView::addFolderOption()
{
}

void
XFE_MNSearchView::buildHeaderOption()
{
  int ac;
  Arg av[10];
  ac = 0;
  Dimension width, height;
 
  m_rules.searchLabel = XmCreateLabelGadget(m_header, 
				"searchFolderLabel", av, ac);

  m_rules.scopeOpt = new XFE_FolderDropdown (getToplevel(),
					m_header,
					TRUE, /* server selection */
					TRUE ); /* show news */
  m_rules.scopeOpt->setPopupServer(False);
  m_rules.scopeOpt->registerInterest(XFE_FolderDropdown::folderSelected,
                                     this,
                                     (XFE_FunctionNotification)folderSelected_cb);

  if ( m_folderInfo ) /* set the m_rules.scope right here... */
        m_rules.scopeOpt->selectFolder(m_folderInfo, False);

  XtVaGetValues(m_rules.scopeOpt->getBaseWidget(),
                XmNwidth, &width,
                XmNheight, &height, 0);

  ac = 0;
  m_rules.whereLabel = XmCreateLabelGadget(m_header, "where", av, ac);
  XtVaSetValues(m_rules.searchLabel, XmNheight, height, 0 );
  XtVaSetValues(m_rules.whereLabel, XmNheight, height, 0 );


}

XFE_CALLBACK_DEFN(XFE_MNSearchView, folderSelected)(XFE_NotificationCenter*,

                           void *, void *calldata)
{
        MSG_FolderInfo *info = (MSG_FolderInfo*)calldata;

        selectFolder(info);
}


void
XFE_MNSearchView::selectFolder(MSG_FolderInfo *folderInfo)
{

	if ( m_folderInfo == folderInfo) return;

	if ( isMailFolder(folderInfo) )
	{
             m_rules.scope = scopeMailFolder;
	     getToplevel()->notifyInterested(
		XFE_MNSearchView::scopeChanged, (void*)folderInfo);
	}
	else {
       		m_rules.scope = scopeNewsgroup;
	     	getToplevel()->notifyInterested(
			XFE_MNSearchView::scopeChanged, (void*)folderInfo);

	}
	m_folderInfo = folderInfo;
}

void
XFE_MNSearchView::andOrCB(Widget w, XtPointer clientData, XtPointer)
{
    XFE_MNSearchView* view = (XFE_MNSearchView*)clientData;
    view->setAllBooleans((strcmp("matchAny", XtName(w)) != 0));
}

// change all the the2 "ands" to "ors" or vice versa
void
XFE_MNSearchView::setAllBooleans(Boolean andP)
{
    m_booleanAnd = andP;
    if (m_numsubviews <= 1)
        return;
    for (int i=1; i < m_numsubviews; ++i)
        setOneBoolean(i);
}

void
XFE_MNSearchView::setOneBoolean(int which)
{
    extern int XFE_SEARCH_BOOL_AND_THE, XFE_SEARCH_BOOL_OR_THE;
    char* newlabel;
    if (m_booleanAnd)
        newlabel = XP_GetString(XFE_SEARCH_BOOL_AND_THE);
    else
        newlabel = XP_GetString(XFE_SEARCH_BOOL_OR_THE);
    XFE_SearchRuleView* curView = (XFE_SearchRuleView*)(m_subviews[which]);
    curView->changeLabel(newlabel);
}

void
XFE_MNSearchView::createMoreLess(Widget parent)
{
  Dimension width;

  Widget form = XtVaCreateManagedWidget("_form", xmFormWidgetClass, parent, 0);

  Widget popup;
  Widget option = fe_make_option_menu(getToplevel()->getBaseWidget(),
                                      form, "optionAndOr", &popup);
  XtVaSetValues(option,
                XmNtopAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_FORM,
                0);

  Widget btn = XmCreatePushButtonGadget(popup, "matchAll", 0, 0);
  XtAddCallback(btn, XmNactivateCallback, andOrCB, this);
  XtManageChild(btn);
  btn = XmCreatePushButtonGadget(popup, "matchAny", 0, 0);
  XtAddCallback(btn, XmNactivateCallback, andOrCB, this);
  XtManageChild(btn);

  XtManageChild(option);

  XtVaCreateManagedWidget("where", xmLabelGadgetClass, form,
                          XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                          XmNbottomWidget, option,
                          XmNleftAttachment, XmATTACH_WIDGET,
                          XmNleftWidget, option,
                          0);

  m_ruleContent = XtVaCreateWidget("ruleContent", xmFormWidgetClass, form,
			XmNresizePolicy, XmRESIZE_ANY,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, option,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNrightAttachment, XmATTACH_FORM,
                                   XmNbottomAttachment, XmATTACH_FORM,
			0);

  m_rules.commandGroup = XmCreateForm(m_ruleContent, "groupRC", NULL, 0 );
  XtVaSetValues(m_rules.commandGroup,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_FORM,
                0);

  m_rules.more_btn = XmCreatePushButton(m_rules.commandGroup, "more", NULL, 0);
  m_rules.less_btn = XmCreatePushButton(m_rules.commandGroup, "fewer", NULL,0);

  XtVaGetValues(m_rules.searchLabel, XmNwidth, &width, 0 );
  XtVaSetValues(m_rules.more_btn,
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_FORM,
		XmNleftOffset, width,
                XmNrightAttachment, XmATTACH_NONE,
                0);
 
  XtVaSetValues(m_rules.less_btn,
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, m_rules.more_btn,
                XmNrightAttachment, XmATTACH_NONE,
                0);
 
  XtManageChild(m_rules.more_btn);

  m_newSearchBtn=XmCreatePushButton(m_rules.commandGroup, "NewBtn", NULL, 0);
  XtVaSetValues(m_newSearchBtn,
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_NONE,
                XmNrightAttachment, XmATTACH_FORM,
                0);
  XtManageChild(m_newSearchBtn);
  XtAddCallback (m_newSearchBtn, XmNactivateCallback, newCallback, this);

  Dimension height1;
  XtVaGetValues(m_rules.less_btn, XmNheight, &height1, 0);

  XtAddCallback(m_rules.more_btn, XmNactivateCallback, moreCallback, this);
  XtAddCallback(m_rules.less_btn, XmNactivateCallback, lessCallback, this);
}

void
XFE_MNSearchView::doViewLayout()
{
  XFE_SearchRuleView *view;
  int i;
  Dimension lwidth;

  for ( i = 0; i < m_numsubviews; i++ )
  {
	view = (XFE_SearchRuleView*)m_subviews[i];
	view->hide();
	if ( i == 0 )
        {
  	  XtVaGetValues(m_rules.searchLabel, XmNwidth, &lwidth, 0);
	  XtVaSetValues(view->getBaseWidget(), 
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, m_header, 0);
	  view->setLabelWidth(view->getLeadingWidth());
        }
	else
	  XtVaSetValues(view->getBaseWidget(), XmNtopWidget, 
			(m_subviews[i-1])->getBaseWidget(), 0);
		
	XtVaSetValues(view->getBaseWidget(), 
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_rules.searchCommand,
		XmNleftAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_NONE,
		0);
	view->show();
  }
}

void
XFE_MNSearchView::doLayout()
{
  XtVaSetValues(m_rules.searchLabel, 			/* Search for Header*/
                XmNalignment, XmALIGNMENT_END,
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_NONE,
                0);
 
  XtVaSetValues(m_rules.scopeOpt->getBaseWidget(),	/* Search for Scope */
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNbottomWidget, m_rules.searchLabel,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, m_rules.searchLabel,
                XmNrightAttachment, XmATTACH_NONE,
                0);
 
  XtVaSetValues(m_rules.whereLabel,			/* Search for Where */
                XmNalignment, XmALIGNMENT_BEGINNING,
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNbottomWidget, m_rules.searchLabel,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, m_rules.scopeOpt->getBaseWidget(),
                XmNrightAttachment, XmATTACH_FORM,
                0);
  doViewLayout();
 
 
  XtVaSetValues(m_rules.commandGroup,	/* More/Less */
				XmNnoResize, True,
                XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, m_subviews[0]->getBaseWidget(),
                XmNbottomAttachment, XmATTACH_NONE,
				XmNbottomOffset, 0,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_WIDGET,
                XmNrightWidget, m_rules.searchCommand,
                0);



  XtManageChild(m_rules.searchLabel);
  m_rules.scopeOpt->show();
  XtManageChild(m_rules.commandGroup);

  XtManageChild (m_ruleContent);
  XtManageChild (m_content);

  // Now everything's managed; make sure the initial and/or is right:
  setAllBooleans(m_booleanAnd);
}

void
XFE_MNSearchView::createDisplayArea()
{
  Cardinal ac;
  Arg av[6];

  // Create Display area to display all the rules and results
  ac = 0;
  m_content = XtVaCreateWidget("searchContent", xmFormWidgetClass,
			m_displayArea,
                	XmNtopAttachment, XmATTACH_FORM,
                	XmNbottomAttachment, XmATTACH_NONE,
                	XmNleftAttachment, XmATTACH_FORM,
                	XmNrightAttachment, XmATTACH_FORM,
			0);

  /* Right side buttons group - Search, NewSearch, SaveAs, Close */
  createCommandButtons();

  /* Scope Header Info  */

  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg (av [ac], XmNrightWidget, m_rules.searchCommand); ac++;
  m_header = XmCreateForm(m_content, "searchHeader", av, ac );

  buildHeaderOption();

  XtManageChild(m_header); /* Scope Header */

  Widget frame = XtVaCreateManagedWidget("moreLessFrame", xmFrameWidgetClass,
                                         m_content,
                                  XmNtopAttachment, XmATTACH_WIDGET,
                                  XmNtopWidget, m_header,
                                  XmNbottomAttachment, XmATTACH_NONE,
                                  XmNleftAttachment, XmATTACH_FORM,
                                  XmNrightAttachment, XmATTACH_WIDGET,
                                  XmNrightWidget, m_rules.searchCommand,
                                  0);

  /* Rule Control - More/Less  */
  createMoreLess(frame);

  /* Add Initial Rules */ 
  addRules("A_the1", m_rules.scope, 0); 


  /* Start to Layout stuffs */
  doLayout();
}

void
XFE_MNSearchView::createMiniDashboard(Widget dashboardParent)
{
  Arg av[20];
  int ac;

  ac = 0;
  Widget separator = XmCreateSeparatorGadget(dashboardParent, 
			"separator", av, ac);
  XtManageChild(separator);


  XP_ASSERT( m_parentFrame != NULL );
  XP_ASSERT( m_parentFrame->isAlive() );

  m_dashboard = new XFE_Dashboard(getToplevel(),	// top level
								  dashboardParent,	// parent
								  m_parentFrame,	// parent frame
								  False);			// have task bar

  m_dashboard->setShowStatusBar(True);
  m_dashboard->setShowProgressBar(True);
  
  /* Set up attachments */
  XtVaSetValues (separator,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_NONE,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
				 0);

  XtVaSetValues (m_dashboard->getBaseWidget(),
                 XmNtopAttachment, XmATTACH_WIDGET,
				 XmNtopWidget, separator,
                 XmNbottomAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 0);
  m_dashboard->show();
}

void
XFE_MNSearchView::createWidgets(Widget formParent)
{
  Cardinal ac;
  Arg      av[20];
  

  /* Build content */
  ac = 0;
  XtSetArg(av[ac], XmNautoUnmanage, False); ac++;
  m_displayArea= XmCreateForm (formParent, 
				"searchDisplayArea", av, ac);
  createDisplayArea();
 
  /* Build dashbord */
  ac = 0;
  Widget dashboard = XmCreateForm(formParent, 
				"searchDashboard", av, ac);
  createMiniDashboard(dashboard);
  XtManageChild(dashboard);
  CONTEXT_DATA(m_contextData)->dashboard = dashboard;

  XtVaSetValues (dashboard,
                 XmNtopAttachment, XmATTACH_NONE,
                 XmNbottomAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 0);

  XtVaSetValues(m_displayArea,
		XmNtopAttachment,XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_WIDGET,
		XmNbottomWidget, dashboard,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		0);

  XtManageChild(m_displayArea);
  // Create layouts
  XtManageChild (getBaseWidget());
}


XFE_CALLBACK_DEFN(XFE_MNSearchView, searchStart)(
		XFE_NotificationCenter */*obj*/,
                void */*clientData*/,
                void */* callData */)
{
  startSearch(m_searchBtn);
}
//fe_search_start_cb(Widget w, XtPointer clientData, XtPointer callData)
void
XFE_MNSearchView::startCallback(Widget w, XtPointer clientData, XtPointer)
{
  XFE_MNSearchView *obj = (XFE_MNSearchView*)clientData;
  obj->startSearch(w);
}

//fe_search_result_reset(fe_search_data *fep)
void
XFE_MNSearchView::resetSearch()
{
  if (m_outliner)
   m_outliner->deselectAllItems();
  if ( m_outliner )
     m_outliner->change(0, 0, 0);

  if (m_gotoBtn) /* Turn off the m_gotoBtn */
  {
	XtSetSensitive(m_gotoBtn, False);
	XtSetSensitive(m_deleteBtn, False);
    m_fileBtn->setSensitive(False);
  }

  toggleActionButtonState(False);

  getToplevel()->notifyInterested(XFE_View::statusNeedsUpdating, (void*)"");
}

//fe_search_createResultArea(MWContext *context)
void
XFE_MNSearchView::createResultArea()
{
   XtVaSetValues(m_content, XmNresizePolicy, XmRESIZE_ANY, 0);
   //XtVaSetValues(getBaseWidget(), XmNresizePolicy, XmRESIZE_NONE, 0);

   XtVaSetValues(m_content, XmNbottomAttachment, XmATTACH_NONE, 0);

   XtVaSetValues(m_content, XmNresizePolicy, XmRESIZE_NONE, 0);
   XtVaSetValues(getBaseWidget(), XmNresizePolicy, XmRESIZE_NONE, 0);
   XtVaSetValues(getToplevel()->getBaseWidget(), 
			XmNresizePolicy, XmRESIZE_ANY, 0);
   doExpand(True);
}

//fe_search_updateUI(MWContext *context, Boolean allowSearch)
void
XFE_MNSearchView::updateUI(Boolean allowSearch)
{

  XtVaSetValues(m_rules.more_btn, XmNsensitive, allowSearch, 0);
  XtVaSetValues(m_rules.less_btn, XmNsensitive, allowSearch, 0);
  if (m_gotoBtn )
  {
   if ( !allowSearch ) /* disable gotoBtn so that user will not interrupt
			  current search. This is to be consistent with
			  "more btn" */
   {
      XtVaSetValues(m_gotoBtn, XmNsensitive, allowSearch, 0);
      XtVaSetValues(m_deleteBtn, XmNsensitive, allowSearch, 0);
      m_fileBtn->setSensitive(allowSearch);
   }
   else 
   {
       /* when search is done, don't enable this gotobtn until
          there is something selected in the search result */ 

       int count;
       const int *selected;

       if (m_outliner)
       {
      	  m_outliner->getSelection( &selected, &count);
		  if ((count == 1) && 
			  MSG_GoToFolderStatus (getPane(), (MSG_ViewIndex *)selected, count))
          {
             XtSetSensitive(m_gotoBtn, True);
             XtSetSensitive(m_deleteBtn, True);
             m_fileBtn->setSensitive(True);
          }
          else if (count > 1)
          {
             XtSetSensitive(m_gotoBtn, False);
             XtSetSensitive(m_deleteBtn, True);
             m_fileBtn->setSensitive(True);
          }
          else
          {
             XtSetSensitive(m_gotoBtn, False);
             XtSetSensitive(m_deleteBtn, False);
             m_fileBtn->setSensitive(False);
          }
       }
       else
       {
         XtSetSensitive(m_gotoBtn, False);
         XtSetSensitive(m_deleteBtn, False);
         m_fileBtn->setSensitive(False);
       }
    }
  }

  if ( m_hasResult )
  	XtVaSetValues(m_newSearchBtn, XmNsensitive, allowSearch, 0);

}

//fe_search_click_onStop(Widget w, MWContext *context)
Boolean
XFE_MNSearchView::isOnStop(Widget w)
{
  int data;

  XtVaGetValues(w, 
		XmNuserData, &data,  0);

 if ( data == kStop )
 {
    return True;
 }
 return False;

}



//fe_search_turnStopOn(MWContext* context, Widget w, Boolean turnOn)
void
XFE_MNSearchView::turnOnStop(Widget w, Boolean turnOn)
{
  XmString xmStr;
  char *string;

  if ( turnOn)
  {
    string = XP_STRDUP(XP_GetString(XFE_AB_STOP));
    xmStr = XmStringCreateSimple(string);
    XtVaSetValues(w, XmNlabelString, xmStr, 
				     XmNuserData, kStop,  0);
    updateUI(False);
    XP_FREE(string);
  }
  else
  {
    string = XP_STRDUP(XP_GetString(XFE_AB_SEARCH));
    xmStr = XmStringCreateSimple(string);
    XtVaSetValues(w, XmNlabelString, xmStr,
				XmNuserData, kSearch,  0);
    updateUI(True);
    XP_FREE(string);
  }
}

void
XFE_MNSearchView::stopSearch()
{
	turnOnStop(m_searchBtn, False);

	if (m_outliner) {
		int count = m_outliner->getTotalLines();
		if (!count) {
			char tmp[128];
			XP_SAFE_SPRINTF(tmp, sizeof(tmp),
							"%s",
							XP_GetString(XFE_SEARCH_NO_MATCHES));

			XFE_Progress(m_contextData, tmp);
		}/* if */
	}/* if */
}

void
XFE_MNSearchView::prepSearchScope()
{
   if (m_searchAll )
   {
   	MSG_AddAllScopes( getPane(), fe_getMNMaster(), m_rules.scope);
   }
   else
   {
   	MSG_AddScopeTerm(getPane(), m_rules.scope, m_folderInfo);
   }
}
		

void
XFE_MNSearchView::startSearch(Widget w)
{
  XFE_SearchRuleView *subView;
  int i;
  int count = 0;
  MSG_SearchValue value;
  MSG_SearchAttribute attr;
  MSG_SearchOperator op;

  if ( isOnStop (w) )
  {
 	/* We should stop the search action */
	XP_InterruptContext(m_contextData);
	turnOnStop(w, False);
	return;
  }

  resetSearch();

  if ( m_allocated )
  {
     MSG_SearchFree(getPane());
     m_allocated = 0;
     m_hasResult = False;
  }

  count = m_numsubviews;

  if ( count > 0 ) {
    MSG_SearchAlloc(getPane());
    m_allocated = 1;
    m_hasResult = True;
  }

  for (i = count-1; i >= 0 ; i-- )
  {
	subView = (XFE_SearchRuleView*)m_subviews[i];

    if ( subView ) 
    {
       char customHdr[160];
	   if (subView->getSearchTerm(&attr, &op, &value, customHdr) )
          { 
              MSG_AddSearchTerm(getPane(), attr, op, &value, 
                                m_booleanAnd, customHdr); 
          } 
/*
#else 
           MSG_AddSearchTerm(getPane(), attr, op, &value); 
#endif
*/
	   else 
	   {
		turnOnStop(w, False);
		return;
	   }
     }
   }
  /* Create the result area now... */
  createResultArea();
  prepSearchScope();

   if ( MSG_Search(getPane()) == SearchError_Success )
   {
   		turnOnStop(w, True);
   }
}

//fe_search_new_cb(Widget w, XtPointer clientData, XtPointer callData)
void
XFE_MNSearchView::newCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_MNSearchView* obj = (XFE_MNSearchView*)clientData;
  obj->newSearch();
}

void
XFE_MNSearchView::newSearch()
{
  int i, count ;
  XFE_SearchRuleView *subView;

  if (m_allocated )
  {
    MSG_SearchFree(getPane());
    m_allocated = 0;
    m_hasResult = False;
  }

  count = m_numsubviews;

  // DORA rule.ruleList is obselete now it's view
  resetSearch();
  doExpand(False);
  for (i = count-1; i > 0 ; i-- )
  {
        lessRules();
  }
  subView = (XFE_SearchRuleView*)m_subviews[0];
  subView->resetSearchValue();

}

//fe_search_close_cb(Widget w, XtPointer clientData, XtPointer callData)
void
XFE_MNSearchView::closeCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_MNSearchView* obj = (XFE_MNSearchView*)clientData;

  obj->handleClose();
}

void
XFE_MNSearchView::handleClose()
{
  /* hide the frame for now*/
  getToplevel()->notifyInterested(XFE_MNSearchView::CloseSearch);
  //XtDestroyWidget(obj->getToplevel()->getBaseWidget());
}



//fe_search_save_as_cb(Widget w, XtPointer clientData, XtPointer callData)
void
XFE_MNSearchView::miscCallback(Widget /*w*/, 
			XtPointer clientData, XtPointer /*callData*/)
{
  XFE_MNSearchView* obj = (XFE_MNSearchView*)clientData;
  obj->miscCmd();
}

void
XFE_MNSearchView::miscCmd()
{
    // Bring up the Advanced Search dialog
    XFE_AdvSearchDialog* advDialog = new XFE_AdvSearchDialog
        (getToplevel()->getBaseWidget(),
         "advancedSearch",
         ViewGlue_getFrame(m_contextData));
    advDialog->post();
    //delete advDialog;   //this causes a core dump
}


//fe_search_more_rule_cb(Widget w, XtPointer clientData, XtPointer callData)
void
XFE_MNSearchView::moreCallback(
			Widget , XtPointer clientData, XtPointer )
{
   XFE_MNSearchView* obj = (XFE_MNSearchView*)clientData;
   obj->moreRules();
}

void
XFE_MNSearchView::moreRules()
{
   XFE_SearchRuleView *newView = NULL;
   XFE_SearchRuleView *curView = NULL;
   Dimension width, height;
   Dimension dheight = 0;
   Dimension minHeight = 0;
   Dimension delta = 0;
   Dimension mh, space;
 
   // Get the minimum height for the m_content by getting the
   // m_ruleContent height. We use this to control the content geometry

   XtVaGetValues(XtParent(m_ruleContent), XmNheight, &minHeight,
	XmNmarginHeight, &mh, XmNverticalSpacing, &space, 0 );

   XtVaSetValues(m_content, XmNresizePolicy, XmRESIZE_NONE, 0);
   // Get margin height of m_content so that we can use m_content's content
   // and m_content's margin height to calculate 
   // the exact height of m_content
   XtVaGetValues (m_content, XmNwidth, &width, 
		XmNheight, &height, 
		XmNmarginHeight, &mh,  // Need to get margin height
		0 );

   XtVaGetValues(getToplevel()->getBaseWidget(), XmNheight, &dheight, 0 );


   void* folder_data = (void*)m_folderInfo;

   if ( m_rules.scope == scopeLdapDirectory)
      folder_data = (void*)m_dir;

   if ( m_numsubviews== 0 )
   {
	// Add new view
	 newView = new XFE_SearchRuleView(getToplevel(),
					m_ruleContent,
					this,
					m_contextData,
					getPane(),
					"A_the2",
					folder_data,
					m_rules.scope,
					0);
   }
   else 
   {
	// Append a new view
    curView = (XFE_SearchRuleView*)(m_subviews[m_numsubviews-1]);
    XP_ASSERT(curView!= NULL);
    newView = new XFE_SearchRuleView(getToplevel(),
			m_ruleContent,
			this,
			m_contextData,
			getPane(),
			"A_the2",
			folder_data,
			m_rules.scope,
			(curView->getAttributeIndex()+1));
   }

   // We only use a single margin height because the
   // attachments of m_content's children has bottomAttachment set to NONE.
   // Therefore, marginHeight at the bottom is not used.

     delta = newView->getHeight()+mh;

   addView(newView);
   if ( curView )
   {
	Dimension lwidth = newView->getLeadingWidth();
 
   	XtVaSetValues(newView->getBaseWidget(),
                XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, curView->getBaseWidget(),
                0);


   	if (!m_hasRightLabel)
   	{
		curView->adjustLeadingWidth(lwidth);
     		m_hasRightLabel = True;
   	}
        newView->setLabelWidth(lwidth);
  }
  else
  {
   Dimension lbl_width;
   XtVaGetValues(m_rules.searchLabel, XmNwidth, &lbl_width, 0);
   newView->setLabelWidth(lbl_width);
 
   XtVaSetValues(newView->getBaseWidget(),
                XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, m_header, 0);
   }
   XtVaSetValues(newView->getBaseWidget(),
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_WIDGET, 
	        XmNrightWidget, m_rules.searchCommand, 0);

   XtVaSetValues(m_rules.commandGroup, 
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, newView->getBaseWidget(),
                 XmNbottomAttachment, XmATTACH_FORM,
                 0);

  XtVaSetValues (m_content,XmNwidth, width, 0);

  XtVaSetValues (m_content, XmNresizePolicy, XmRESIZE_NONE,
		XmNheight, height+delta, 0);

  XtVaSetValues (getBaseWidget(), XmNwidth, width, 
		XmNheight, dheight + delta, 
		XmNresizePolicy, XmRESIZE_ANY, 0); 

  XtVaSetValues (getToplevel()->getBaseWidget(), XmNwidth, width, 
		XmNheight, dheight + delta, 
		XmNresizePolicy, XmRESIZE_ANY, 0); 
  newView->show();
  XtManageChild(m_rules.less_btn);

  // To prevent more rules to show up after 5th rules
  // Same as windows

  if ( m_numsubviews >= MAX_RULES_ALLOWED )
  {
	XtSetSensitive(m_rules.more_btn, False);
  }

  // Make sure the label ("and" vs. "or") is the same as the rest:
  setOneBoolean(m_numsubviews-1);
}

//fe_search_less_rule_cb(Widget w, XtPointer clientData, XtPointer callData)
void
XFE_MNSearchView::lessCallback( Widget, XtPointer clientData, XtPointer)
{
   XFE_MNSearchView* obj = (XFE_MNSearchView*)clientData;
   obj->lessRules();
}

void
XFE_MNSearchView::lessRules()
{
   XFE_SearchRuleView *newView;
   XFE_SearchRuleView *curView;
   //fe_SearchListElement *newlist;
   //fe_SearchListElement *curlist;
   Dimension width = 0, height = 0, dwidth, dheight = 0, curHeight = 0;
   Dimension minHeight = 0;
   Dimension mh= 0;
 
   // When subtract rules, we use the searchCommand to control
   // the min Height  of the m_content
   XtVaGetValues(m_rules.searchCommand, XmNheight, &minHeight, 0);

   XtVaSetValues (m_content, XmNresizePolicy, XmRESIZE_NONE, 0);
   XtVaGetValues (m_content, XmNmarginHeight, &mh, 
		XmNheight, &height, XmNwidth, &width, 0);
   XtVaGetValues (getToplevel()->getBaseWidget(), XmNwidth, &dwidth, 0);
   XtVaGetValues (getToplevel()->getBaseWidget(), XmNheight, &dheight, 0 );

   // Min height of the m_content is made of the height of searchCommand
   // height plus one margin height. The bottom attachment of searchCommand is
   // NONE. Therefore, we only use one margin height to calculate the height

   minHeight = minHeight+mh;

   if ( m_numsubviews> 1 )
   {
	
     curView = (XFE_SearchRuleView*)(m_subviews[m_numsubviews-1]);
     m_subviews[m_numsubviews-1] = NULL;
     m_numsubviews--;
     newView = (XFE_SearchRuleView*)(m_subviews[m_numsubviews-1]);
     XtVaSetValues(m_rules.commandGroup, 
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, newView->getBaseWidget(), 0);

     curHeight = curView->getHeight();
     curView->hide();
     delete curView;
   }

   if ( m_numsubviews== 1 )
   {
   	XtUnmanageChild(m_rules.less_btn);

	// To maintain at min height for m_content
        if ( height - curHeight < minHeight)
          curHeight = height-minHeight;
   }

   XtVaSetValues(m_content, XmNwidth, width, 0 ); 

   XtVaSetValues(m_content, 
		XmNresizePolicy, XmRESIZE_NONE,
		XmNheight, height-curHeight, 
		0 );

   XtVaSetValues(getBaseWidget(), 
	XmNwidth, dwidth, 
	XmNheight, dheight - curHeight, 
	XmNresizePolicy, XmRESIZE_ANY, 
	0 );

   XtVaSetValues(getToplevel()->getBaseWidget(), 
	XmNwidth, dwidth, 
	XmNheight, dheight - curHeight, 
	XmNresizePolicy, XmRESIZE_ANY, 
	0 );

  // To prevent more rules to show up after 5th rules
  // Same as windows

  if ( m_numsubviews < MAX_RULES_ALLOWED )
  {
	XtSetSensitive(m_rules.more_btn, True);
  }
}


// The Outlinable interface.
void *
XFE_MNSearchView::ConvFromIndex(int /*index*/) 
{
	return NULL;
}

int
XFE_MNSearchView::ConvToIndex(void */*item*/)
{
	return 0;
}


char *
XFE_MNSearchView::getColumnName(int column)
{
	switch (column)
		{
		case MAILSEARCH_OUTLINER_COLUMN_LOCATION:
			return XP_GetString(XFE_SEARCH_LOCATION); // XXX i18n
		case MAILSEARCH_OUTLINER_COLUMN_SUBJECT:
			return XP_GetString(XFE_SEARCH_SUBJECT); // XXX i18n
		case MAILSEARCH_OUTLINER_COLUMN_SENDER:
			return XP_GetString(XFE_SEARCH_SENDER); // XXX i18n
		case MAILSEARCH_OUTLINER_COLUMN_DATE:
			return XP_GetString(XFE_SEARCH_DATE); // XXX i18n
		case MAILSEARCH_OUTLINER_COLUMN_PRIORITY:
			return XP_GetString(XFE_PRI_PRIORITY); // XXX i18n
		default:
			XP_ASSERT(0);
			return 0;
		}
}

char *
XFE_MNSearchView::getColumnHeaderText(int column) 
{
	switch (column)
		{
		case MAILSEARCH_OUTLINER_COLUMN_LOCATION:
			return XP_GetString(XFE_SEARCH_LOCATION); // XXX i18n
		case MAILSEARCH_OUTLINER_COLUMN_SUBJECT:
			return XP_GetString(XFE_SEARCH_SUBJECT); // XXX i18n
		case MAILSEARCH_OUTLINER_COLUMN_SENDER:
			return XP_GetString(XFE_SEARCH_SENDER); // XXX i18n
		case MAILSEARCH_OUTLINER_COLUMN_DATE:
			return XP_GetString(XFE_SEARCH_DATE); // XXX i18n
		case MAILSEARCH_OUTLINER_COLUMN_PRIORITY:
			return XP_GetString(XFE_PRI_PRIORITY); // XXX i18n
		default:
			XP_ASSERT(0);
			return 0;
    }
}


fe_icon *
XFE_MNSearchView::getColumnHeaderIcon(int) 
{
	return NULL;
}

EOutlinerTextStyle 
XFE_MNSearchView::getColumnHeaderStyle(int) 
{
	return OUTLINER_Bold;
}

void *
XFE_MNSearchView::acquireLineData(int line) 
{
   m_resultLine = NULL;
   int count = m_outliner->getTotalLines();

   if (line >= 0 &&
	   line < count) {
	   MSG_GetResultElement(getPane(), line, &m_resultLine);
   }/* if */

   return m_resultLine;
}


void 
XFE_MNSearchView::getTreeInfo(XP_Bool */*expandable*/, 
			XP_Bool */*is_expanded*/, int */*depth*/,
                        OutlinerAncestorInfo **/*ancestor*/)
{
	// Do Nothing
}

EOutlinerTextStyle
XFE_MNSearchView::getColumnStyle(int /*column*/)
{
  return OUTLINER_Default;
}

char *
XFE_MNSearchView::getColumnText(int column)
{
 MSG_SearchValue *result;
 char *data = NULL;

	// Fill in the text for this particular column
  if (!m_resultLine )  return NULL;
  switch (column)
  {
    case MAILSEARCH_OUTLINER_COLUMN_SUBJECT:
  	MSG_GetResultAttribute(m_resultLine, attribSubject, &result);
	INTL_DECODE_MIME_PART_II(data, result->u.string, fe_LocaleCharSetID, FALSE);
	break;
    case MAILSEARCH_OUTLINER_COLUMN_SENDER:
  	MSG_GetResultAttribute(m_resultLine, attribSender, &result);
	INTL_DECODE_MIME_PART_II(data, result->u.string, fe_LocaleCharSetID, FALSE);
	break;
    case MAILSEARCH_OUTLINER_COLUMN_DATE:
  	MSG_GetResultAttribute(m_resultLine, attribDate, &result);
	data = XP_STRDUP(MSG_FormatDate(getPane(), result->u.date));
	break;
    case MAILSEARCH_OUTLINER_COLUMN_PRIORITY:
  	MSG_GetResultAttribute(m_resultLine, attribPriority, &result);
	data = priorityToString(result->u.priority);
	break;
    case MAILSEARCH_OUTLINER_COLUMN_LOCATION:
  	MSG_GetResultAttribute(m_resultLine, attribLocation, &result);
	data = XP_STRDUP(result->u.string);
	break;
  }
  MSG_DestroySearchValue(result);
  return data;
}

fe_icon *
XFE_MNSearchView::getColumnIcon(int column)
{
   // Fill in the text for this particular column
   if ( column == MAILSEARCH_OUTLINER_COLUMN_SUBJECT)
   {
	return 0; // Suppose to return mail icon
   }

   return 0;
}

void
XFE_MNSearchView::clickHeader(const OutlineButtonFuncData *data)
{
  int column = data->column;
  int invalid = True;

  switch (column)
    {
    case MAILSEARCH_OUTLINER_COLUMN_LOCATION:
	MSG_SortResultList(getPane(), attribLocation, False);
	break;
    case MAILSEARCH_OUTLINER_COLUMN_SUBJECT:
	MSG_SortResultList(getPane(), attribSubject, False);
	break;
    case MAILSEARCH_OUTLINER_COLUMN_SENDER:
	MSG_SortResultList(getPane(), attribSender, False);
	break;
    case MAILSEARCH_OUTLINER_COLUMN_DATE:
	MSG_SortResultList(getPane(), attribDate, False);
	break;
    case MAILSEARCH_OUTLINER_COLUMN_PRIORITY:
	MSG_SortResultList(getPane(), attribPriority, False);
	break;
    default:
      invalid = False;
    }/* switch() */
  if (invalid)
    m_outliner->invalidate();
}

void 
XFE_MNSearchView::singleClick(const OutlineButtonFuncData *data)
{
	m_outliner->selectItemExclusive(data->row);
	toggleActionButtonState(True);
}

void 
XFE_MNSearchView::doubleClick(const OutlineButtonFuncData *data)
{
   MSG_ViewIndex index;
   MSG_ResultElement *elem = NULL;

   m_outliner->selectItemExclusive(data->row);

   toggleActionButtonState(True);

   index = (MSG_ViewIndex)(data->row);
   MSG_GetResultElement(getPane(), index, &elem );

   XP_ASSERT(elem!=NULL);


   MWContextType cxType = MSG_GetResultElementType(elem);

   if ( cxType == MWContextMail
	|| cxType == MWContextMailMsg
	|| cxType == MWContextNews
	|| cxType == MWContextNewsMsg )
   {
   	XP_List *msg_frame_list = 
		XFE_MozillaApp::theApp()->getFrameList( FRAME_MAILNEWS_MSG );
	m_msgFrame = NULL;
        if (!XP_ListIsEmpty(msg_frame_list))
        {
          m_msgFrame = (XFE_MsgFrame*)XP_ListTopObject( msg_frame_list );
        }

	if (!m_msgFrame)
	{
	 m_msgFrame = new XFE_MsgFrame(XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()),
								   ViewGlue_getFrame(m_contextData), NULL);
        }
        m_searchpane = ((XFE_MsgView*)(m_msgFrame->getView()))->getPane();
	m_msgFrame->show();
        MSG_OpenResultElement(elem, m_searchpane);

        // Tell the message frame to unmanage half its buttons:
        m_msgFrame->setButtonsByContext(cxType);
    }

}

void 
XFE_MNSearchView::Buttonfunc(const OutlineButtonFuncData *data)
{
	int row = data->row, 
		clicks = data->clicks;

	if (row < 0) {
		clickHeader(data);
		return;
	} 
	else {
		/* content row 
		 */
		if (clicks == 2) {
			m_outliner->selectItemExclusive(data->row);
			doubleClick(data);
		}/* clicks == 2 */
		else if (clicks == 1) {
			if (data->ctrl)
				{
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
					/* we had a range of items selected, so let's do something really
					   nice with them. */
					m_outliner->trimOrExpandSelection(data->row);
				}/* else */
			}/* if */
			else {
				m_outliner->selectItemExclusive(data->row);
			}/* else */

			Boolean finishSearch = True;

			/* this "go to btn" should keep consistent state with "more btn" */
			if ( m_rules.more_btn ) 
			  XtVaGetValues(m_rules.more_btn, XmNsensitive, &finishSearch, 0);

			if ( finishSearch && m_gotoBtn )  /* if search is done, take care of the gotobtn */
			{
			/* Find out how many rows are selected: If only one is selected,
			   we turn on the button m_gotoBtn, otherwise, we don't allow
			   the button m_gotoBtn to be sensitive */

			  int count;
			  const int *selected;
  			  m_outliner->getSelection( &selected, &count);

			  if ((count == 1) &&
				  MSG_GoToFolderStatus (getPane(), 
										(MSG_ViewIndex *)selected, count))
              {
				  XtSetSensitive(m_gotoBtn, True);
				  XtSetSensitive(m_deleteBtn, True);
                  m_fileBtn->setSensitive(True);
              }
              else if (count > 1)
              {
				  XtSetSensitive(m_gotoBtn, False);
				  XtSetSensitive(m_deleteBtn, True);
                  m_fileBtn->setSensitive(True);
              }
			  else
              {
				  XtSetSensitive(m_gotoBtn, False);
				  XtSetSensitive(m_deleteBtn, False);
                  m_fileBtn->setSensitive(False);
              }

			}
			else if ( finishSearch)
			{
			  int count;
			  const int *selected;
  			  m_outliner->getSelection( &selected, &count);
			  if ( count >= 1 )
			     toggleActionButtonState(True);
			  else 
			     toggleActionButtonState(False);

			}
			getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
		}/* clicks == 1 */
	}/* else */
}

void 
XFE_MNSearchView::Flippyfunc(const OutlineFlippyFuncData */*data*/)
{
	// Do nothing
}

void 
XFE_MNSearchView::releaseLineData() 
{
	// Do nothing
}
//fe_search_expand_cb(Widget w, XtPointer clientData, XtPointer callData)
void
XFE_MNSearchView::expandCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_MNSearchView *obj = (XFE_MNSearchView*)clientData;

  obj->expand();
}

void 
XFE_MNSearchView::expand()
{
  if ( m_result.expand )
  {
     doExpand(False);
  }
  else
  {
     doExpand(True);
  }
}

//fe_search_do_expand(MWContext *context, Boolean doExpand )
void
XFE_MNSearchView::doExpand(Boolean doit)
{
  Dimension width = 0, height = 0;
  Dimension cwidth = 0, cheight = 0;
  Widget shell = getToplevel()->getBaseWidget();

  if ( doit )
  {
        XtVaGetValues(shell, XmNwidth, &width, XmNheight, &height, 0);
        if ( !m_result.container ) 
            buildResult();
        XtVaSetValues(m_result.container, XmNresizePolicy, XmRESIZE_ANY, 0 );
        XtVaGetValues(m_content, XmNwidth, &cwidth, 0 );

        XtVaSetValues(m_result.container, XmNwidth, cwidth, 0);
	XtVaSetValues(shell, XmNwidth, cwidth,  0);
	XtVaSetValues(getBaseWidget(), XmNwidth, cwidth,  0);

	if ( !XtIsManaged(m_result.container) )
 	{
          XtVaGetValues(m_result.container, XmNheight, &cheight, 0);
          XtManageChild(m_result.container);
        }

          XtVaSetValues(m_content, 
		XmNresizePolicy, XmRESIZE_ANY,
		0);
          XtVaSetValues(m_result.container, 
		XmNresizePolicy, XmRESIZE_ANY,
		0);
          XtVaSetValues(getBaseWidget(), 
		XmNresizePolicy, XmRESIZE_ANY,
		0);
          XtVaSetValues(shell, 
		XmNresizePolicy, XmRESIZE_ANY,
		0);

        XtVaSetValues(m_result.container, XmNwidth, cwidth, 0);
	XtVaSetValues(getBaseWidget(),  XmNheight, cheight + height, 0);
	XtVaSetValues(shell,  XmNheight, cheight + height, 0);
        m_result.expand = True;
   }
   else
   {
     XtVaGetValues(shell, XmNwidth, &width, XmNheight, &height, 0);
     XtVaSetValues(shell, XmNresizePolicy, XmRESIZE_NONE, 0 );
     XtVaSetValues(m_content, XmNresizePolicy, XmRESIZE_NONE, 0 );
     XtVaSetValues(m_content, XmNwidth, width, 0 );
     XtVaSetValues(shell, XmNwidth, width, 0 );
     if ( m_result.container && XtIsManaged(m_result.container))
     {
        XtVaSetValues(m_result.container, XmNwidth, width, 0);
        XtVaGetValues(m_result.container, XmNwidth, 
			&cwidth, XmNheight, &cheight, 0);
     	XtUnmanageChild(m_result.container);
     XtVaSetValues(m_result.container, XmNwidth, width, 0);
     }
     XtVaSetValues(m_content, XmNwidth, width, 0 );
     XtVaSetValues(shell, XmNwidth, width, 0 );

     if ( m_result.expand )
     {
        XtVaSetValues(shell, XmNheight, height-cheight, 0);
     }
     XtVaSetValues(shell, XmNresizePolicy, XmRESIZE_ANY, 0);

     m_result.expand = False;
   }
}

void
XFE_MNSearchView::buildResultTable()
{
  int num_columns =  5;
  static int column_widths[] = {20, 10, 9, 10, 10};
  m_outliner = new XFE_Outliner("addressList",
                                this,
                                getToplevel(),
                                m_result.container,
                                False, // constantSize
                                True,  // hasHeadings
                                num_columns,
                                num_columns,
                                column_widths,
								OUTLINER_GEOMETRY_PREF);

  m_outliner->setMultiSelectAllowed(True);

}

//fe_search_build_result (MWContext *context)
void
XFE_MNSearchView::buildResult()
{
  Arg	av[30];
  Cardinal ac;

  XtVaSetValues(m_content, XmNbottomAttachment, XmATTACH_NONE, 0 );
  
  /* Result */
  ac = 0;
  XtSetArg (av [ac], XmNheight, 250); ac++;
  m_result.container = XmCreateForm(m_displayArea, 
		"searchResult", av, ac);

  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg (av [ac], XmNtopWidget, m_content); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNresizePolicy, XmRESIZE_NONE); ac++;
  XtSetArg (av [ac], XmNfractionBase, 3); ac++;
  XtSetValues(m_result.container, av, ac);
  //XtManageChild(m_result.container);

 /* Create Result Frame */

 /* For outliner 
  */

  buildResultTable();

  XtVaSetValues(m_outliner->getBaseWidget(),
                XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
                XmNvisibleRows, 15,
                NULL);
  XtVaSetValues(m_outliner->getBaseWidget(),
                XmNcellDefaults, True,
                XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
                NULL);

  /* For the "Go to Message Folder"
  */
  if ( m_addGotoBtn )
  {
  ac = 0;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ++ac;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_NONE); ++ac;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_POSITION); ++ac;
  XtSetArg(av[ac], XmNleftPosition, 0); ++ac;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_POSITION); ++ac;
  XtSetArg(av[ac], XmNrightPosition, 1); ++ac;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ++ac;
  XtSetArg(av[ac], XmNleftOffset, 15); ++ac;
  XtSetArg(av[ac], XmNrightOffset, 8); ++ac;
  m_gotoBtn = XmCreatePushButton(m_result.container, "goToMessageFolder", av, ac);
  XtSetSensitive(m_gotoBtn, False);
  XtAddCallback(m_gotoBtn, XmNactivateCallback, gotoFolderCallback, this);
  XtManageChild(m_gotoBtn);

  // Egregious hack: XFE_Buttons don't appear outdented like PushButtons,
  // so kludge that by surrounding the XFE_Button with an outdented frame:
  Widget fileFrame
       = XtVaCreateManagedWidget("_fileFrame", xmFrameWidgetClass,
                                   m_result.container,
                                 XmNshadowType, XmSHADOW_OUT,
                                 XmNbottomAttachment, XmATTACH_FORM,
                                 XmNtopAttachment, XmATTACH_NONE,
                                 XmNleftAttachment, XmATTACH_POSITION,
                                 XmNleftPosition, 1,
                                 XmNrightAttachment, XmATTACH_POSITION,
                                 XmNrightPosition, 2,
                                 XmNbottomAttachment, XmATTACH_FORM,
                                 XmNleftOffset, 8,
                                 XmNrightOffset, 8,
                                 0);
  m_fileBtn = new XFE_Button(ViewGlue_getFrame(m_contextData),
                             fileFrame,
                             "fileMsg",
                             XFE_FolderMenu::generate,
                             (void*)xfeCmdMoveMessage,
                             0);
  m_fileBtn->setSensitive(False);
  m_fileBtn->show();
  // Also want to set the button's background color to the same as the
  // background color for m_gotoBtn, but only after they're both realized.
  
  ac = 0;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ++ac;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_NONE); ++ac;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_POSITION); ++ac;
  XtSetArg(av[ac], XmNleftPosition, 2); ++ac;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_POSITION); ++ac;
  XtSetArg(av[ac], XmNrightPosition, 3); ++ac;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ++ac;
  XtSetArg(av[ac], XmNleftOffset, 8); ++ac;
  XtSetArg(av[ac], XmNrightOffset, 15); ++ac;
  m_deleteBtn = XmCreatePushButton(m_result.container, "deleteMsg", av, ac);
  XtSetSensitive(m_deleteBtn, False);
  XtAddCallback(m_deleteBtn, XmNactivateCallback, deleteMsgsCallback, this);
  XtManageChild(m_deleteBtn);
  

     XtVaSetValues(m_outliner->getBaseWidget(),

		  XmNtopAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_WIDGET,
		  XmNbottomWidget, m_gotoBtn,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  0);

  }
  else /* No gotoBtn, need to attach to form */
  {
     XtVaSetValues(m_outliner->getBaseWidget(),
		  XmNtopAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  0);
  }

  // put a realize callback on the outliner so that we can call
  // hackTranslations() on it to get the popup menus working:
  XtAddEventHandler(m_outliner->getBaseWidget(), ExposureMask, False,
                    exposeOutlinerHandler, (XtPointer)this);

  m_outliner->show();
} 

XFE_CALLBACK_DEFN(XFE_MNSearchView, searchFinished)(
		XFE_NotificationCenter */*obj*/,
                void */*clientData*/,
                void */* callData */)
{
  stopSearch();
}
void
XFE_MNSearchView::gotoFolderCallback(Widget w, XtPointer clientData, XtPointer)
{
  XFE_MNSearchView *obj = (XFE_MNSearchView*)clientData;
  obj->gotoFolder(w);
}

#define MULTI_ENTRIES 0
void
XFE_MNSearchView::gotoFolder(Widget /*w*/)
{
   
  int count, 
	  i = 0;
  const int *selected;
  m_outliner->getSelection( &selected, &count);
  /* support for opening a search result in its thread pane context 
   */
  if ((count == 1) &&
	  MSG_GoToFolderStatus (getPane(), (MSG_ViewIndex *)selected, count))
  {
#if MULTI_ENTRIES
	  for (i=0; i < count; i++) {
#endif
	  MSG_ResultElement *elem = NULL;
	  MSG_GetResultElement(getPane(), selected[i], &elem);
	  
	  if ( !elem ) 
#if MULTI_ENTRIES
		  continue;
#else
		  return;
#endif
	  
	  MSG_SearchValue *value;
	  MSG_GetResultAttribute(elem, attribMessageKey, &value);
	  MessageKey key = value->u.key;
	  MSG_GetResultAttribute(elem, attribFolderInfo, &value);
	  MSG_FolderInfo *folderInfo = value->u.folder;
	  
	  fe_showMessages(XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()),
					  ViewGlue_getFrame(m_contextData), NULL, folderInfo, 
					  fe_globalPrefs.reuse_thread_window, FALSE, key);
#if MULTI_ENTRIES
	  }/* for i */
#endif
  }
}

void
XFE_MNSearchView::deleteMsgsCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_MNSearchView *obj = (XFE_MNSearchView*)clientData;
  obj->deleteMsgs();
}

void
XFE_MNSearchView::deleteMsgs()
{
    int count;
    const int *selected;
    m_outliner->getSelection(&selected, &count);

    if (count <= 0)
        return;

    MSG_Command(getPane(), MSG_DeleteMessage,
                (MSG_ViewIndex *)selected, count);
}

// The outliners are created too late to have hackTranslations()
// called on them, so we install the search translations manually:
/*static*/ void
XFE_MNSearchView::exposeOutlinerHandler(Widget w,
                                        XtPointer, XEvent*, Boolean*)
{
    XtOverrideTranslations(w, fe_globalData.mnsearch_global_translations);
}

void 
XFE_MNSearchView::toggleActionButtonState(Boolean)
{
  // Do nothing for Mail/News search.. just a place holder for now
}


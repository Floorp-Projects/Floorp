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
   LdapSearchView.h -- user's mail search view.
   Created: Dora Hsu <dora@netscape.com>, 15-Dec-96.
*/

#include "MozillaApp.h"
#include "LdapSearchView.h"
#include "MsgFrame.h"
#include "MsgView.h"
#include "BrowserFrame.h"
#include "ViewGlue.h"

#include "ComposeView.h"
#include "ComposeFolderView.h"
#include "addrbk.h"
#include "icondata.h"

#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/SelectioB.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/DrawnB.h>

// For Column Header to use

const char * XFE_LdapSearchView::CloseLdap = "XFE_LdapSearchView::closeLdap";
const int XFE_LdapSearchView::SEARCH_OUTLINER_COLUMN_NAME= 0;
const int XFE_LdapSearchView::SEARCH_OUTLINER_COLUMN_EMAIL= 1;
const int XFE_LdapSearchView::SEARCH_OUTLINER_COLUMN_COMPANY= 2;
const int XFE_LdapSearchView::SEARCH_OUTLINER_COLUMN_PHONE= 3;
const int XFE_LdapSearchView::SEARCH_OUTLINER_COLUMN_LOCALITY= 4;

#define OUTLINER_GEOMETRY_PREF "mail.ldapsearchpane.outliner_geometry"

static fe_icon m_personIcon = { 0 };

extern "C" Widget
fe_make_option_menu(Widget toplevel,
        Widget parent, char* widgetName, Widget *popup);

extern "C" void
fe_get_option_size ( Widget optionMenu,
                        Dimension *retWidth, Dimension *retHeight );

XFE_LdapSearchView::XFE_LdapSearchView(XFE_Component *toplevel_component,
									   Widget parent,
									   XFE_Frame * parent_frame,
									   XFE_MNView *mn_parentView,
									   MWContext *context, MSG_Pane *p)
	: XFE_MNSearchView(toplevel_component, 
					   parent, 
					   parent_frame,
					   mn_parentView, 
					   context, 
					   p, 
					   NULL, 
					   False)
{
  initialize();
}

XFE_LdapSearchView::~XFE_LdapSearchView()
{
}

void
XFE_LdapSearchView::initialize()
{
  // LDAP Specific
  m_rules.scope = (MSG_ScopeAttribute)scopeLdapDirectory;
  m_toAddrBook  = NULL;
  m_toCompose  = NULL;
  m_browserFrame = NULL;
  m_directories = FE_GetDirServers();
  int nDirs = XP_ListCount(m_directories);
  if ( nDirs )
  	m_dir = (DIR_Server *) XP_ListGetObjectNum(m_directories, 1);
  else m_dir = NULL;
}


void
XFE_LdapSearchView::createWidgets (Widget formParent)
{
   XFE_MNSearchView::createWidgets(formParent);
   createResultArea();
}


// Override
void
XFE_LdapSearchView::changeScope ()
{
  // Should do nothing because the scope should always be LdapDirectory 
  m_dir = getDirServer();
  getToplevel()->notifyInterested( XFE_MNSearchView::scopeChanged, m_dir);
}

//fe_search_folder_option_cb
void
XFE_LdapSearchView::folderOptionCallback(Widget /*w*/,
                                        XtPointer clientData, XtPointer)
{
   XFE_LdapSearchView* obj = (XFE_LdapSearchView*)clientData;
   obj->changeScope();
}


// Override
void
XFE_LdapSearchView::addDefaultFolders()
{
  int i;
  Widget popupW;
  Cardinal ac = 0;
  Arg av[10];
  XP_List    *directories = FE_GetDirServers();
  int nDirs = XP_ListCount(directories);

  XtVaGetValues(m_rules.scopeOptW, XmNsubMenuId, &popupW, 0);
  /* Hopefully: Get Number of Scope Names from MSG */ 
  for ( i = 0; i < nDirs; i++)
  {
    Widget btn;
    XmString xmStr;

    DIR_Server *dir = (DIR_Server*)XP_ListGetObjectNum(directories,i+1);

    if ( dir && dir->dirType == LDAPDirectory )
    {

       xmStr = XmStringCreateLtoR(dir->description,
                                XmSTRING_DEFAULT_CHARSET);

       ac = 0;
       XtSetArg(av[ac], XmNuserData, i); ac++;
       XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
       btn = XmCreatePushButtonGadget(popupW, "ldapDirectory", av, ac);

       XtAddCallback(btn, XmNactivateCallback, 
			XFE_LdapSearchView::folderOptionCallback, 
			this);

       XtManageChild(btn);
       XmStringFree(xmStr);
    }
   }
}

// Override
void
XFE_LdapSearchView::buildHeaderOption()
{
  Cardinal ac;
  Arg av[20];
  Widget popupW;
  Dimension width, height;

  ac = 0;
  m_rules.searchLabel = XmCreateLabelGadget(m_header,
                                "ldapSearchFolderLabel", av, ac);
  m_rules.scopeOptW = fe_make_option_menu(getToplevel()->getBaseWidget(),
                        m_header, "ldapSearchScopeOpt", &popupW);

  // Add folders...
  addDefaultFolders();
  fe_get_option_size ( m_rules.scopeOptW, &width, &height);

  ac = 0;
  m_rules.whereLabel = XmCreateLabelGadget(m_header, "where", av, ac);
  XtVaSetValues(m_rules.searchLabel, XmNheight, height, 0 );
  XtVaSetValues(m_rules.whereLabel, XmNheight, height, 0 );
}

// Override


DIR_Server* 
XFE_LdapSearchView::getDirServer()
{
  Widget btn;
  int folderNum;

  XtVaGetValues(m_rules.scopeOptW, XmNmenuHistory, &btn, 0 );
  XtVaGetValues(btn, XmNuserData, &folderNum, 0);

  // Ldap search start ...
  if ( m_numsubviews <= 0 ) return 0; // This should never happen

  int nDirs = XP_ListCount(m_directories);
  DIR_Server *dir = (DIR_Server *) XP_ListGetObjectNum(m_directories,
			folderNum+1);
  m_dir = dir;
  return dir;
}

void
XFE_LdapSearchView::prepSearchScope()
{

  DIR_Server *dir = getDirServer();
  XP_ASSERT(dir!= NULL);
  MSG_AddLdapScope( getPane(), dir);
}


//fe_search_save_as_cb(Widget w, XtPointer clientData, XtPointer callData)
void
XFE_LdapSearchView::miscCmd()
{
}

char *
XFE_LdapSearchView::getColumnHeaderText(int column) 
{
	/* Server ?
	 */
	DIR_Server *pServer = getDirServer();
	XP_ASSERT(pServer != NULL);
	/* attrib
	 */
	MSG_SearchAttribute  attrib;
	switch (column) {
    case SEARCH_OUTLINER_COLUMN_NAME:
		attrib = attribCommonName;
		break;

    case SEARCH_OUTLINER_COLUMN_EMAIL:
		attrib = attrib822Address;
		break;

	case SEARCH_OUTLINER_COLUMN_COMPANY:
		attrib = attribOrganization;
		break;
		
    case SEARCH_OUTLINER_COLUMN_PHONE:
		attrib = attribPhoneNumber;
		break;
    case SEARCH_OUTLINER_COLUMN_LOCALITY:
		attrib = attribLocality;
		break;
		
	default:
		XP_ASSERT(0);
		return 0;
    }/* switch */
	DIR_AttributeId id;
	MSG_SearchAttribToDirAttrib(attrib, &id);
	const char *text = DIR_GetAttributeName(pServer, id);
	static char header[128];
	header[0] = '\0';
	XP_STRCPY(header, text);
	return header;
}

char *
XFE_LdapSearchView::getColumnText(int column)
{
 MSG_SearchValue *result;
 MSG_SearchError err = SearchError_NullPointer; 
	// Fill in the text for this particular column
  if (!m_resultLine )  return NULL;
  switch (column)
  {
    case SEARCH_OUTLINER_COLUMN_NAME:
  	err = MSG_GetResultAttribute(m_resultLine, attribCommonName, &result);
	break;
    case SEARCH_OUTLINER_COLUMN_EMAIL:
  	err = MSG_GetResultAttribute(m_resultLine, attrib822Address, &result);
	break;
    case SEARCH_OUTLINER_COLUMN_COMPANY:
  	err = MSG_GetResultAttribute(m_resultLine, attribOrganization, &result);
	break;
    case SEARCH_OUTLINER_COLUMN_PHONE:
  	err = MSG_GetResultAttribute(m_resultLine, attribPhoneNumber, &result);
	break;
    case SEARCH_OUTLINER_COLUMN_LOCALITY:
  	err = MSG_GetResultAttribute(m_resultLine, attribLocality, &result);
	break;
    default:
#ifdef DEBUG
	  printf("XFE_LdapSearchView::getColumnText()Invalid column type.\n");
#endif
	break;
  }

  return XP_STRDUP(((err == SearchError_Success) && result && result->u.string?
	  result->u.string:""));

}

fe_icon *
XFE_LdapSearchView::getColumnIcon(int column)
{
   // Fill in the text for this particular column
   if ( column == SEARCH_OUTLINER_COLUMN_NAME)
   {
	return 0; // Suppose to return people icon
   }

   return 0;
}

void 
XFE_LdapSearchView::doubleClick(const OutlineButtonFuncData *data)
{
   MSG_ViewIndex index;
   MSG_ResultElement *elem = NULL;
   // Do nothing
   if (data->clicks == 2 )
   {
	m_outliner->selectItemExclusive(data->row);
	toggleActionButtonState(True);
	index = (MSG_ViewIndex)(data->row);
        MSG_GetResultElement(getPane(), index, &elem );

	// Always create a new frame for each result
	m_browserFrame = new XFE_BrowserFrame(
			XtParent(getToplevel()->getBaseWidget()), 
			   ViewGlue_getFrame(m_contextData), NULL);
        m_searchpane = ((XFE_MsgView*)(m_browserFrame->getView()))->getPane();
        MSG_OpenResultElement(elem, m_browserFrame->getContext());
	m_browserFrame->show();
   }
}

void
XFE_LdapSearchView::buildResultTable()
{
  int num_columns =  5;
  int visible_num_columns =  4;
  static int column_widths[] = {10, 20, 20, 10, 10};
  m_outliner = new XFE_Outliner("addressList",
                                this,
                                getToplevel(),
                                m_result.container,
                                False, // constantSize
                                True,  // hasHeadings
                                num_columns,
                                visible_num_columns,
                                column_widths,
								OUTLINER_GEOMETRY_PREF);

  Pixel bg;
  XtVaGetValues(m_outliner->getBaseWidget(), XmNbackground, &bg, 0);
  if (!m_personIcon.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
		     bg,
		     &m_personIcon,
		     NULL, 
		     MN_Person.width, 
		     MN_Person.height,
		     MN_Person.mono_bits, 
		     MN_Person.color_bits, 
		     MN_Person.mask_bits, 
		     FALSE);
  m_outliner->setMultiSelectAllowed(True);

#if !defined(USE_MOTIF_DND)
  m_outliner->setDragType(FE_DND_LDAP_ENTRY, &m_personIcon, this);
#endif /* USE_MOTIF_DND */

  m_outliner->setHideColumnsAllowed(True);

}

void
XFE_LdapSearchView::buildResult()
{
  Widget ldapCommands;
  Arg argv[10];
  Cardinal ac = 0;

    XFE_MNSearchView::buildResult();

    ac = 0;
    XtSetArg(argv[ac], XmNorientation, XmHORIZONTAL); ac++;
    ldapCommands = XmCreateRowColumn(m_result.container, "ldapCommands", argv, ac);

    XtVaSetValues(ldapCommands, 
                  XmNtopAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  0);

    ac = 0;
    m_toCompose =  XmCreatePushButtonGadget(ldapCommands, "toCompose", argv,ac);
    m_toAddrBook =  XmCreatePushButtonGadget(ldapCommands, "toAddrBook", argv,ac);
    XtSetSensitive(m_toAddrBook, False);
    XtSetSensitive(m_toCompose,False);

    XtManageChild(m_toAddrBook);
    XtManageChild(m_toCompose);

	XtAddCallback(m_toAddrBook, XmNactivateCallback, 
				  XFE_LdapSearchView::toAddrBookCallback, 
				  this);
	XtAddCallback(m_toCompose, XmNactivateCallback, 
				  XFE_LdapSearchView::toComposeCallback, 
				  this);

    Dimension h, mh;

    XtVaGetValues(m_toAddrBook, XmNheight, &h, 0 );

    XtVaGetValues(ldapCommands, XmNmarginHeight, &mh, 0 );

    h = h + mh *2;

    XtVaSetValues(m_outliner->getBaseWidget(),
                  XmNbottomOffset, h,
                  0);
    XtManageChild(ldapCommands);
}

void
XFE_LdapSearchView::clickHeader(const OutlineButtonFuncData */*data*/)
{
}

void
XFE_LdapSearchView::handleClose()
{
  /* hide the frame for now*/
  getToplevel()->notifyInterested(XFE_LdapSearchView::CloseLdap);
  //XtDestroyWidget(obj->getToplevel()->getBaseWidget());
}

void 
XFE_LdapSearchView::toAddrBookCallback(Widget w, 
									   XtPointer clientData, 
									   XtPointer callData)
{
	XFE_LdapSearchView *obj = (XFE_LdapSearchView *) clientData;
	obj->toAddrBookCB(w, callData);
}/* XFE_LdapSearchView::toAddrBookCallback() */

void 
XFE_LdapSearchView::toAddrBookCB(Widget /* w */, XtPointer /* callData */)
{
	/* need to check if selections are in personal address book
	 * already 
	 */
  int count = 0; // = XmLGridGetSelectedRowCount(w);
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
  if (!count)
    return;

  if (SearchError_Success !=
	  MSG_AddLdapResultsToAddressBook(getPane(), 
									  (MSG_ViewIndex *) indices, 
									  count))
	  XP_ASSERT(0);

}/* XFE_LdapSearchView::toAddrBookCB() */

void 
XFE_LdapSearchView::toComposeCallback(Widget w, 
									  XtPointer clientData, 
									  XtPointer callData)
{
	XFE_LdapSearchView *obj = (XFE_LdapSearchView *) clientData;
	obj->toComposeCB(w, callData);
}/* XFE_LdapSearchView::toComposeCallback() */

void 
XFE_LdapSearchView::toComposeCB(Widget /* w */, XtPointer /* callData */)
{

	/* Get all selections; pack them and ..
	 *  AddrBookFrame::
	 */
  /* Need to mimic ...
   */
  int count = 0; // = XmLGridGetSelectedRowCount(w);
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
  if (!count)
    return;

  if (SearchError_Success != 
	  MSG_ComposeFromLdapResults(getPane(), 
								 (MSG_ViewIndex *) indices, 
								 count))
	  XP_ASSERT(0);
}/* XFE_LdapSearchView::toComposeCB() */

void
XFE_LdapSearchView::doLayout()
{
  XtVaSetValues(m_rules.searchLabel,                    /* Search for Header*/
                XmNalignment, XmALIGNMENT_END,
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_NONE,
                0);

  XtVaSetValues(m_rules.scopeOptW,      /* Search for Scope */
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNbottomWidget, m_rules.searchLabel,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, m_rules.searchLabel,
                XmNrightAttachment, XmATTACH_NONE,
                0);

  XtVaSetValues(m_rules.whereLabel,                     /* Search for Where */
                XmNalignment, XmALIGNMENT_BEGINNING,
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNbottomWidget, m_rules.searchLabel,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, m_rules.scopeOptW,
                XmNrightAttachment, XmATTACH_FORM,
                0);
  doViewLayout();

  XtVaSetValues(m_rules.commandGroup,   /* More/Less */
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
  XtManageChild(m_rules.scopeOptW);
  XtManageChild(m_rules.whereLabel);
  XtManageChild(m_rules.commandGroup);

  XtManageChild (m_ruleContent);
  XtManageChild (m_content);
}

/* Backend is not calling FE_PaneChanged on Ldap Directory change right
   now, therefore, this method is not in use. However, the backend should
   really call FE_PaneChanged when directory order is changed in the prefs.
   That notification is hooked up for address book not for ldap search.
   Has filed a bug to phil */
void
XFE_LdapSearchView::paneChanged(XP_Bool /*asynchronous*/,
                                MSG_PANE_CHANGED_NOTIFY_CODE /* notify_code */,
                                int32 /*value*/)
{

        /* Shall we free existing list ?
         */
        m_directories = FE_GetDirServers();
        int nDirs = XP_ListCount(m_directories);
        XP_Bool found = False;
        for (int i=0; i < nDirs; i++) {
                DIR_Server *dir = 
                        (DIR_Server *) XP_ListGetObjectNum(m_directories,i+1);
                if (dir == m_dir ||
                        (dir && m_dir &&
                         (dir->dirType == m_dir->dirType))) {
                        if ((dir->serverName==NULL && m_dir->serverName==NULL) ||
                                (dir->serverName && m_dir->serverName &&
                                 !XP_STRCMP(dir->serverName, m_dir->serverName))) {
                                found = True;
                                break;
                        }/* if */
                }/* if */
        }/* for i*/
        if (!found) {
                /* m_dir got deleted
                 */
		m_dir = NULL;
		if ( !m_dir && nDirs )
		{
		/* there are still some directories installed, pick the
		   first one for best guess */
		m_dir = (DIR_Server *) XP_ListGetObjectNum(m_directories, 1 );
		}
	}
	if (!m_dir) /* non-directory left.... close search dialog*/
	  handleClose();
}

/*
 * Append selected search results to an address folder view.
 * This is called as result of dragging from the LDAP search view
 * to a compose window address area.
 */
void
XFE_LdapSearchView::addSelectedToAddressPane(XFE_AddressFolderView* addr, SEND_STATUS fieldStatus)
{
  int count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);

  if (count==0)
    return;

  /* pack selected
   */
  ABAddrMsgCBProcStruc *pairs =
      (ABAddrMsgCBProcStruc *) XP_CALLOC(1,sizeof(ABAddrMsgCBProcStruc));
  pairs->m_pairs = (StatusID_t **) XP_CALLOC(count, sizeof(StatusID_t*));

  MSG_ResultElement *resultLine = NULL;
  for (int i=0; i < count; i++) {
	  if (SearchError_Success != 
		  MSG_GetResultElement(getPane(), indices[i], &resultLine)) 
		  continue;

	  StatusID_t *pair;
	  pair = (StatusID_t *) XP_CALLOC(1, sizeof(StatusID_t));
	  pair->status = fieldStatus;
	  pair->type = ABTypePerson;

	  // email
	  MSG_SearchValue *result;
	  if ((SearchError_Success == MSG_GetResultAttribute(resultLine,attrib822Address,&result))&&
		  result &&
		  result->u.string) {
		  pair->emailAddr = XP_STRDUP(result->u.string);
		  MSG_DestroySearchValue(result);
	  }

	  // fullname
	  MSG_GetResultAttribute(resultLine, attribCommonName, &result);
	  
	  // assemble
	  if (strlen(pair->emailAddr) && 
		  result && 
		  result->u.string) {
		  char tmp[AB_MAX_STRLEN];
		  sprintf(tmp, "%s <%s>", result->u.string, pair->emailAddr);
		  pair->dplyStr = XP_STRDUP(tmp);
		  MSG_DestroySearchValue(result);
	  }
	  else
		  pair->dplyStr = XP_STRDUP(result->u.string);
      
	  pairs->m_pairs[pairs->m_count] = pair;
	  (pairs->m_count)++;
  }
  if (pairs && pairs->m_count) {
      addr->addrMsgCB(pairs);
  }
}

void 
XFE_LdapSearchView::toggleActionButtonState(Boolean on)
{
  XtSetSensitive(m_toAddrBook, on);
  XtSetSensitive(m_toCompose, on);
}

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

#include "rosetta.h"
#include "Frame.h"
#include "ViewGlue.h"
#include "ABListSearchView.h"
#include "AB2PaneView.h"
#include "ABNameFolderDlg.h"
#include "ABMListDlg.h"

#if defined(USE_MOTIF_DND)
 
#include "OutlinerDrop.h"

#endif /* USE_MOTIF_DND */

#include "ABSearchDlg.h"
#include "MNView.h"
#include "Xfe/Xfe.h"

#include <Xm/ArrowB.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/TextF.h>

extern "C" {
#include "xpassert.h"
#include "xfe.h"
#include "DtWidgets/ComboBox.h"
#include "felocale.h"

XP_List* FE_GetDirServers();
ABook*   fe_GetABook(MWContext *context);
};

#include "libi18n.h"
#include "intl_csi.h"

#include "xpgetstr.h"

#include "prefapi.h"

const char *XFE_ABListSearchView::dirExpand = 
                                  "XFE_ABListSearchView::dirExpand";
const char *XFE_ABListSearchView::dirsChanged = 
                                  "XFE_ABListSearchView::dirsChanged";
const char *XFE_ABListSearchView::dirSelect = 
                                  "XFE_ABListSearchView::dirSelect";

// icons
fe_icon XFE_ABListSearchView::m_personIcon = { 0 };
fe_icon XFE_ABListSearchView::m_listIcon = { 0 };
HG28190

extern int XFE_AB_HEADER_NAME;
extern int XFE_AB_HEADER_CERTIFICATE;
extern int XFE_AB_HEADER_EMAIL;
extern int XFE_AB_HEADER_NICKNAME;
extern int XFE_AB_HEADER_EMAIL;
extern int XFE_AB_HEADER_COMPANY;
extern int XFE_AB_HEADER_PHONE;
extern int XFE_AB_HEADER_LOCALITY;

extern int XFE_AB_SEARCH_DLG;
extern int XFE_AB_SEARCH;
extern int XFE_AB_STOP;
extern int XFE_SEARCH_NO_MATCHES;
extern int XFE_ADDR_ENTRY_ALREADY_EXISTS;
extern int MK_ADDR_ENTRY_ALREADY_EXISTS;

#ifndef AB_MAX_STRLEN
#define AB_MAX_STRLEN 1024
#endif

#if defined(DEBUG_tao)
#define D(x) printf x
#else
#define D(x)
#endif

MenuSpec XFE_ABListSearchView::view_popup_spec[] = {
  { xfeCmdComposeMessage,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdABDeleteEntry,PUSHBUTTON },
#if defined(USE_ABCOM)
  { xfeCmdABDeleteAllEntries,PUSHBUTTON },
#endif /* USE_ABCOM */
  MENU_SEPARATOR,
  { xfeCmdViewProperties,	PUSHBUTTON },
  { NULL }
};

static char a_line[AB_MAX_STRLEN];

XFE_ABListSearchView::XFE_ABListSearchView(XFE_Component *toplevel_component,
										   Widget         /* parent */,
										   XFE_View      *parent_view, 
										   MWContext     *context,
										   XP_List       *directories):
	XFE_MNListView(toplevel_component, parent_view, context, (MSG_Pane *)NULL),
	m_searchingDir(False),
	m_searchStr(NULL),
	m_searchInfo(NULL),
	m_popup(0)
{

  /* Initialize
   */
#if defined(USE_ABCOM)
  m_containerInfo = 0;
  m_abContainerPane = 0;

  // TODO: what is the default??
  m_sortType = AB_attribFullName;
  m_pageSize = 0;

  m_numAttribs = 0;
  //  m_attribIDs = 0;
#else
  m_sortType = AB_SortByFullNameCmd;
#endif /* USE_ABCOM */
  m_dataIndex = MSG_VIEWINDEXNONE;
  m_typeDownIndex = MSG_VIEWINDEXNONE;

  m_expandBtn = 0;

  m_ldapDisabled = False;
  m_filterDirCombo = 0;
  m_filterBoxForm = 0;
  m_filterSearchBtn = 0;
  m_filterStopBtn = 0;

  //
  m_typeDownTimer = 0;

  // bstell: initialize doc_csid
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_contextData);
  INTL_SetCSIDocCSID(c, fe_LocaleCharSetID);
  INTL_SetCSIWinCSID(c, INTL_DocToWinCharSetID(INTL_GetCSIDocCSID(c)));

  /* Establish connection wih backend first
   *
   */
  ((MWContext*)m_contextData)->type = MWContextAddressBook;
  /* Allocate frontend data */
  fe_addrbk_data* d = CONTEXT_DATA(m_contextData)->abdata 
                    = XP_NEW_ZAP(fe_addrbk_data);

  /* setting stuff to null, don't remove these 
   */
  d->editlist = NULL;
  d->edituser = NULL;
  d->findshell = NULL;

  /* AddressBookPane
   */
  m_abPane = NULL;
  m_dir = NULL;

#if defined(USE_ABCOM)
  /* create the pane
   */
  int error = AB_CreateABPane((MSG_Pane **) &m_abPane,
							  (MWContext *) m_contextData, /* or ABfeContext */
							  fe_getMNMaster());

  error = 
	  AB_SetShowPropertySheetForEntryFunc((MSG_Pane *) m_abPane,
		 &XFE_ABListSearchView::ShowPropertySheetForEntryFunc);

  m_pane = (MSG_Pane *) m_abPane;
#else
  m_AddrBook = fe_GetABook(0);
  m_directories = directories;

  int nDirs = XP_ListCount(m_directories);
  XP_ASSERT(nDirs);
  m_dir = (DIR_Server *) XP_ListGetObjectNum(m_directories,1);
  XP_ASSERT(m_dir);

  AB_InitAddressBookPane(&m_abPane,    /* returned sortPane */
						 m_dir,        /* DIR_Server */
						 m_AddrBook,   /* Address Book */
						 (MWContext *) m_contextData, /* or ABfeContext */
						 fe_getMNMaster(), 
						 ABFullName /*AB_SortByFullNameCmd */, 
						 True);
#endif /* USE_ABCOM */

  /* Tao_04dec96: use XFE_MNListView
   * connect pane and view
   * MSG_SetFEData((MSG_Pane *)m_abPane, this);
   */
  setPane((MSG_Pane *)m_abPane);

#if !defined(GLUE_COMPO_CONTEXT)
  registerInterest(XFE_View::allConnectionsCompleteCallback,
						  this,
						  (XFE_FunctionNotification)allConnectionsComplete_cb);
 /* Tao_17dec96
   * register interest in getting allconnectionsComplete
   */
  MWContext *top = XP_GetNonGridContext (context);
  XFE_Frame *f = ViewGlue_getFrame(top);
  if (f)
	  f->registerInterest(XFE_Frame::allConnectionsCompleteCallback,
						  this,
						  (XFE_FunctionNotification)allConnectionsComplete_cb);
#endif /* GLUE_COMPO_CONTEXT */
}

XFE_ABListSearchView::~XFE_ABListSearchView()
{
	if (m_abPane) {
#if defined(USE_ABCOM)
		MSG_SetFEData(m_pane, 0);
		AB_ClosePane(m_pane);
#else
	    AB_CloseAddressBookPane(&m_abPane);
#endif /* USE_ABCOM */
	}/* if */

	/* unregister 
	 */
 	if (m_outliner)
		XtRemoveCallback(m_outliner->getBaseWidget(), XmNresizeCallback, 
						 XFE_ABListSearchView::resizeCallback, 
						 (XtPointer) this);

#if !defined(GLUE_COMPO_CONTEXT)
	/* Tao_17dec96
	 * register interest ingetting allconnectionsComplete
	 */
	MWContext *top = XP_GetNonGridContext (m_contextData);
	XFE_Frame *f = ViewGlue_getFrame(top);
	if (f)
		f->unregisterInterest(XFE_Frame::allConnectionsCompleteCallback,
							  this,
							  (XFE_FunctionNotification)allConnectionsComplete_cb);
#endif /* GLUE_COMPO_CONTEXT */

	//
	if (m_popup)
		delete m_popup;
}

#if defined(USE_ABCOM)
int 
XFE_ABListSearchView::ShowPropertySheetForEntryFunc(MSG_Pane *pane,
													MWContext *context)
{
#if defined(DEBUG_tao)
	printf("\nXFE_ABListSearchView::ShowPropertySheetForEntryFunc,pane=0x%x,context=0x%d\n",
		   pane,context);
#endif
	MSG_PaneType type = MSG_GetPaneType(pane);
	if (type == AB_PERSONENTRYPANE) {
		return fe_ShowPropertySheetForEntry(pane, context);
	}/* if */
	else if (type == AB_MAILINGLISTPANE) {
		return fe_ShowPropertySheetForMList(pane, context);
	}/* else */
	return FALSE;
}

AB_EntryType 
XFE_ABListSearchView::getType(AB_ContainerInfo *container, ABID id)
{
	XP_ASSERT(container && id != AB_ABIDUNKNOWN);
	
	AB_AttributeValue *valueArray = NULL;
	int error = AB_GetEntryAttribute(container,
									 id,
									 AB_attribEntryType, 
									 &valueArray);
	XP_ASSERT(valueArray && valueArray->attrib == AB_attribEntryType);
	AB_EntryType type = AB_Person;
	if (valueArray)
		type = valueArray->u.entryType;
	AB_FreeEntryAttributeValue(valueArray);
	return type;
}

AB_EntryType 
XFE_ABListSearchView::getType(MSG_Pane *abPane, MSG_ViewIndex index)
{
	XP_ASSERT(abPane && index != MSG_VIEWINDEXNONE);
	
	AB_AttributeValue *valueArray = NULL;
	int error =  AB_GetEntryAttributeForPane(abPane,
											 index,
											 AB_attribEntryType, 
											 &valueArray);
	XP_ASSERT(valueArray && valueArray->attrib == AB_attribEntryType);
	AB_EntryType type = AB_Person;
	if (valueArray)
		type = valueArray->u.entryType;
	AB_FreeEntryAttributeValue(valueArray);
	return type;
}
#endif /* USE_ABCOM */

void XFE_ABListSearchView::setLdapDisabled(XP_Bool b)
{
	m_ldapDisabled = b;
}

void XFE_ABListSearchView::idToPerson(DIR_Server *pDir,
									  ABID entry, PersonEntry* pPerson)
{
	if (pDir && 
		entry != MSG_VIEWINDEXNONE && 
		pPerson) {
		DIR_Server *dir = pDir;
		ABook      *aBook = m_AddrBook;
		
		char        a_line[AB_MAX_STRLEN];		
		pPerson->WinCSID = m_contextData->fe.data->xfe_doc_csid;

		a_line[0] = '\0';
		if (AB_GetNickname(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pNickName = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetGivenName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pGivenName = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetMiddleName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pMiddleName = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetFamilyName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pFamilyName = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetCompanyName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pCompanyName = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetLocality(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pLocality = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetRegion(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pRegion = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetEmailAddress(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pEmailAddress = XP_STRDUP(a_line);
	  
		a_line[0] = '\0';
		if (AB_GetInfo(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pInfo = XP_STRDUP(a_line);
		
		AB_GetHTMLMail(dir, aBook, entry, &(pPerson->HTMLmail));
		
		a_line[0] = '\0';
		if (AB_GetTitle(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pTitle = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetStreetAddress(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pAddress = XP_STRDUP(a_line);
	 
		a_line[0] = '\0';
		if (AB_GetPOAddress(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pPOAddress = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetZipCode(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pZipCode = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetCountry(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pCountry = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetWorkPhone(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pWorkPhone = XP_STRDUP(a_line);
		
		a_line[0] = '\0';
		if (AB_GetFaxPhone(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pFaxPhone = XP_STRDUP(a_line);
	 
		a_line[0] = '\0';
		if (AB_GetHomePhone(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pHomePhone = XP_STRDUP(a_line);
#if 0
		a_line[0] = '\0';
		if (AB_GetDistName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pDistName = XP_STRDUP(a_line);
#endif
		HG87291
		
		a_line[0] = '\0';
		if (AB_GetCoolAddress(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
			pPerson->pCoolAddress = XP_STRDUP(a_line);
		short use;
		AB_GetUseServer(dir, aBook, entry, &use);
		pPerson->UseServer = use;
	}/* if */
}

Boolean  
XFE_ABListSearchView::isCommandEnabled(CommandType cmd, 
					    void */* calldata = NULL */, XFE_CommandInfo*)
{
  uint32 count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, (int *) &count);
  
  AB_CommandType abCmd = (AB_CommandType)~0;

  if (cmd == xfeCmdComposeMessage)
    abCmd = AB_NewMessageCmd;
  else if (cmd == xfeCmdAddToAddressBook)
    abCmd = AB_AddUserCmd;
  else if (cmd == xfeCmdABNewList)
    abCmd = AB_AddMailingListCmd;

  else if (cmd == xfeCmdABNewPAB)
    abCmd = AB_NewAddressBook;
  else if (cmd == xfeCmdABNewLDAPDirectory)
    abCmd = AB_NewLDAPDirectory;

  else if (cmd == xfeCmdImport)
    abCmd = AB_ImportCmd;
  else if (cmd == xfeCmdSaveAs)
    abCmd = AB_SaveCmd;
  else if (cmd == xfeCmdClose)
    abCmd = AB_CloseCmd;

  else if (cmd == xfeCmdUndo)
    abCmd = AB_UndoCmd;
  else if (cmd == xfeCmdRedo)
    abCmd = AB_RedoCmd;
  else if (cmd == xfeCmdABDeleteEntry)
    abCmd = AB_DeleteCmd;
  else if (cmd == xfeCmdABDeleteAllEntries)
    abCmd = AB_DeleteAllCmd;
  else if (cmd == xfeCmdABSearchEntry)
    abCmd = AB_LDAPSearchCmd; 
  else if (cmd == xfeCmdViewProperties
		   || cmd == xfeCmdABEditEntry)
    abCmd = AB_PropertiesCmd;
  else if (cmd == xfeCmdABByType)
    abCmd = AB_SortByTypeCmd;
  else if (cmd == xfeCmdABByName)
    abCmd = AB_SortByFullNameCmd;
  else if (cmd == xfeCmdABByEmailAddress)
    abCmd = AB_SortByEmailAddress;
  else if (cmd == xfeCmdABByCompany)
    abCmd = AB_SortByCompanyName;
  else if (cmd == xfeCmdABByLocality)
    abCmd = AB_SortByLocality;
  else if (cmd == xfeCmdABByNickName)
    abCmd = AB_SortByNickname;
  else if (cmd == xfeCmdSortAscending)
    abCmd = AB_SortAscending;
  else if (cmd == xfeCmdSortDescending)
    abCmd = AB_SortDescending;

  else if (cmd == xfeCmdABCall)
    abCmd = AB_CallCmd;

  if (cmd == xfeCmdABCall &&
	  !fe_IsConferenceInstalled())
	  return FALSE;

  MSG_COMMAND_CHECK_STATE sState = MSG_NotUsed;
  XP_Bool enable = FALSE, 
	      plural = FALSE;
#if defined(DEBUG_tao_)
  printf("\nXFE_ABListSearchView::isCommandEnabled:%s\n", Command::getString(cmd));
#endif
  if (abCmd != ((AB_CommandType)~0))
#if defined(USE_ABCOM)
	  AB_CommandStatusAB2(m_pane, abCmd, 
						  (MSG_ViewIndex *)indices, (int32) count, 
						  &enable, &sState, NULL, &plural);
#else
	  AB_CommandStatus(m_abPane, abCmd, (MSG_ViewIndex *)indices, count, 
					   &enable, &sState, NULL, &plural);
#endif /* USE_ABCOM */
  return enable;
}/* XFE_ABListSearchView::isCommandEnabled() */

/* used by toplevel to see which view can handle a command.  Returns true
 * if we can handle it. 
 */
Boolean 
XFE_ABListSearchView::handlesCommand(CommandType cmd, 
									 void * /* calldata */, 
									 XFE_CommandInfo* /* i */)
{
#if defined(DEBUG_tao_)
  printf("\nXFE_ABListSearchView::handlesCommand:%s\n", Command::getString(cmd));
#endif
	// handle view specific command
  if (IS_AB_PANE_CMD(cmd)
	  || IS_2_PANE_CMD(cmd)

	  || cmd == xfeCmdDisplayHTMLDomainsDialog
	  || cmd == xfeCmdABEditEntry
	  || cmd == xfeCmdABDeleteEntry
	  || cmd == xfeCmdABCall
	  || cmd == xfeCmdABvCard
	  || cmd == xfeCmdSelectAll)
	  return True;
  return False;
	
}

/* this method is used by the toplevel to dispatch a command. */
void 
XFE_ABListSearchView::doCommand(CommandType cmd, 
								void * /* calldata */,
								XFE_CommandInfo* info)
{
	if (cmd == xfeCmdShowPopup) {
		// Finish up the popup
		int x, y, clickrow;
		XEvent *event = info->event;

		m_outliner->translateFromRootCoords(event->xbutton.x_root,
											event->xbutton.y_root,
											&x, &y);
		clickrow = m_outliner->XYToRow(x, y);
		if (clickrow != -1) {
			/* if it was actually in the outliner's content rows. 
			 */
			if (m_popup)
				delete m_popup;

			m_popup = 
				new XFE_PopupMenu("popup",(XFE_Frame *) getToplevel(), 
								  XfeAncestorFindApplicationShell(getToplevel()->
																  getBaseWidget()));
			m_popup->addMenuSpec(view_popup_spec);
			m_popup->position (event);
			m_popup->show();
		}/* if */
	}/* if */
	else if (cmd == xfeCmdSelectAll)
      m_outliner->selectAllItems();
#if defined(USE_ABCOM)
	else {
		AB_CommandType abCmd = (AB_CommandType)~0;

		if (cmd == xfeCmdComposeMessage)
			abCmd = AB_NewMessageCmd;
		else if (cmd == xfeCmdAddToAddressBook)
			abCmd = AB_AddUserCmd;
		else if (cmd == xfeCmdABNewList)
			abCmd = AB_AddMailingListCmd;
		
		else if (cmd == xfeCmdABNewPAB)
			abCmd = AB_NewAddressBook;
		else if (cmd == xfeCmdABNewLDAPDirectory)
			abCmd = AB_NewLDAPDirectory;
		
		else if (cmd == xfeCmdImport)
			abCmd = AB_ImportCmd;
		else if (cmd == xfeCmdSaveAs)
			abCmd = AB_SaveCmd;
		else if (cmd == xfeCmdClose)
			abCmd = AB_CloseCmd;
		
		else if (cmd == xfeCmdUndo)
			abCmd = AB_UndoCmd;
		else if (cmd == xfeCmdRedo)
			abCmd = AB_RedoCmd;
		else if (cmd == xfeCmdABDeleteEntry)
			abCmd = AB_DeleteCmd;
		else if (cmd == xfeCmdABDeleteAllEntries)
			abCmd = AB_DeleteAllCmd;
		else if (cmd == xfeCmdABSearchEntry)
			abCmd = AB_LDAPSearchCmd; 
		else if (cmd == xfeCmdViewProperties
				 || cmd == xfeCmdABEditEntry)
			abCmd = AB_PropertiesCmd;
		else if (cmd == xfeCmdABByType)
			abCmd = AB_SortByTypeCmd;
		else if (cmd == xfeCmdABByName)
			abCmd = AB_SortByFullNameCmd;
		else if (cmd == xfeCmdABByEmailAddress)
			abCmd = AB_SortByEmailAddress;
		else if (cmd == xfeCmdABByCompany)
			abCmd = AB_SortByCompanyName;
		else if (cmd == xfeCmdABByLocality)
			abCmd = AB_SortByLocality;
		else if (cmd == xfeCmdABByNickName)
			abCmd = AB_SortByNickname;
		else if (cmd == xfeCmdSortAscending)
			abCmd = AB_SortAscending;
		else if (cmd == xfeCmdSortDescending)
			abCmd = AB_SortDescending;

		else if (cmd == xfeCmdABCall)
			abCmd = AB_CallCmd;
		
		int32 count = 0;
		const int *indices = 0;

		m_outliner->getSelection(&indices, (int *) &count);
		int error = AB_CommandAB2(m_pane,
								  abCmd,
								  (MSG_ViewIndex *) indices,
								  count);

	}/* else */
#endif
}

/*
 * Callbacks for outside world
 */
void
XFE_ABListSearchView::newUser()
{
#if defined(USE_ABCOM)
	int error = AB_CommandAB2(m_pane, AB_AddUserCmd, 
							  (MSG_ViewIndex *) NULL, 0);
#else
	  popupUserPropertyWindow(MSG_VIEWINDEXNONE, True, True);
#endif /* USE_ABCOM */
}

void 
XFE_ABListSearchView::newList()
{
#if defined(USE_ABCOM)
	int error = AB_CommandAB2(m_pane, AB_AddMailingListCmd, 
							  (MSG_ViewIndex *) NULL, 0);
#else
  popupListPropertyWindow(MSG_VIEWINDEXNONE, True, False);
#endif /* USE_ABCOM */
}

void 
XFE_ABListSearchView::abDelete()
{
  /* check which is selected
   */
  uint32 count = 0;
  const int *indices = 0;


#if !defined(USE_ABCOM)
  AB_GetEntryCount (m_dir, m_AddrBook, 
					&count, (ABID) ABTypeAll, 0);
#endif /* USE_ABCOM */
  count = 0;
  m_outliner->getSelection(&indices, (int *) &count);
  if (count > 0 && indices) {
	  int first = indices[0];
	  /* int AB_Command (ABPane* pane, AB_CommandType command,
	   * MSG_ViewIndex* indices, int32 numindices);
	   */
#if defined(USE_ABCOM)
	  AB_CommandAB2(m_pane, 
					AB_DeleteCmd, 
					(MSG_ViewIndex *)indices, count);
	  count = MSG_GetNumLines(m_pane);
#else
	  AB_Command((ABPane *) m_abPane, 
				 AB_DeleteCmd, 
				 (MSG_ViewIndex *)indices, count);
	  
	  /* Tao_04dec96: use XFE_MNListView ???
	   */
	  AB_GetEntryCount (m_dir, m_AddrBook, 
						&count, (ABID) ABTypeAll, 0);
#endif /* USE_ABCOM */
#if 0
	  m_outliner->change(0, count, count);
#endif
	  if (count) {
		
		  int pos = (first <= (count-1))?first:(count-1);
		  m_outliner->selectItemExclusive(pos);
		  m_outliner->makeVisible(pos);
	  }/* if */
	  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
  }/* if */
}

void
XFE_ABListSearchView::abDeleteAllEntries()
{
  /* check which is selected
   */
  uint32 count = 0;
  const int *indices = 0;

  m_outliner->getSelection(&indices, (int *) &count);
  if (count > 0 && indices) {
	  int first = indices[0];
	  /* int AB_Command (ABPane* pane, AB_CommandType command,
	   * MSG_ViewIndex* indices, int32 numindices);
	   */
#if defined(USE_ABCOM)
	  AB_CommandAB2(m_pane, 
					AB_DeleteAllCmd, 
					(MSG_ViewIndex *)indices, count);
	  count = MSG_GetNumLines(m_pane);
#endif /* USE_ABCOM */
	  if (count) {
		
		  int pos = (first <= (count-1))?first:(count-1);
		  m_outliner->selectItemExclusive(pos);
		  m_outliner->makeVisible(pos);
	  }/* if */
	  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
  }/* if */
}

void 
XFE_ABListSearchView::undo()
{
  /* check which is selected
   */
  uint32 count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, (int *) &count);

    /* int AB_Command (ABPane* pane, AB_CommandType command,
     * MSG_ViewIndex* indices, int32 numindices);
     */
#if defined(USE_ABCOM)
  AB_CommandAB2(m_pane, 
				AB_UndoCmd, 
				(MSG_ViewIndex *)indices, count);
#else
    AB_Command((ABPane *) m_abPane, 
	       AB_UndoCmd, 
	       (MSG_ViewIndex *)indices, count);
#endif /* USE_ABCOM */

#if 0
    /* Tao_04dec96: use XFE_MNListView ???
     */
    AB_GetEntryCount (m_dir, m_AddrBook, 
		      &count, (ABID) ABTypeAll, 0);
    m_outliner->change(0, count, count);
#endif
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);

}

void 
XFE_ABListSearchView::redo()
{
  /* check which is selected
   */
  uint32 count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, (int *) &count);

    /* int AB_Command (ABPane* pane, AB_CommandType command,
     * MSG_ViewIndex* indices, int32 numindices);
     */
#if defined(USE_ABCOM)
  AB_CommandAB2(m_pane, 
				AB_RedoCmd, 
				(MSG_ViewIndex *)indices, count);
#else
    AB_Command((ABPane *) m_abPane, 
	       AB_RedoCmd, 
	       (MSG_ViewIndex *)indices, count);
#endif /* USE_ABCOM */
#if 0
    /* Tao_04dec96: use XFE_MNListView ???
     */
    AB_GetEntryCount (m_dir, m_AddrBook, 
		      &count, (ABID) ABTypeAll, 0);
    m_outliner->change(0, count, count);
#endif
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);

}

void
XFE_ABListSearchView::propertiesCB()
{
	editProperty();
}

void 
XFE_ABListSearchView::popupUserPropertyWindow(ABID entry, XP_Bool newuser,
											  XP_Bool /* modal */)
{
#if defined(USE_ABCOM)
	if (newuser)
		editProperty();
	else
		newUser();
#else
  if (m_dir->dirType == LDAPDirectory) {
	  uint32 count = 0;
	  const int *indices = 0;
	  m_outliner->getSelection(&indices, (int *) &count);
	  AB_Command (m_abPane, AB_PropertiesCmd, (MSG_ViewIndex *)indices, count);
  }/* if */
  else {
	  fe_showABCardPropertyDlg(getToplevel()->getBaseWidget(),
							   m_contextData,
							   entry,
							   newuser);
	  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
  }/* else */
#endif /* USE_ABCOM */
}/* XFE_ABListSearchView::popupUserPropertyWindow() */

void
XFE_ABListSearchView::popupListPropertyWindow(ABID entry, XP_Bool newuser,
											  XP_Bool modal)
{
	XFE_ABMListDlg* listDlg = 
		new XFE_ABMListDlg(this, 
						   getToplevel()->getBaseWidget(), 
						   "abMListProperties", 
						   modal, m_contextData);
	listDlg->setDlgValues(entry, newuser);
	listDlg->show();
}/* XFE_ABListSearchView::popupListPropertyWindow() */

void 
XFE_ABListSearchView::editProperty()
{
  /* check which is selected
   */
  int count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
  if (count > 0 && indices) {
#if defined(USE_ABCOM)
	  int error = AB_CommandAB2(m_pane, AB_PropertiesCmd, 
								(MSG_ViewIndex *)indices, (int32) count);
#else
    /* Take the first one 
     */
    ABID type;
    ABID entry;

    entry = AB_GetEntryIDAt((AddressPane *) m_abPane, (uint32) indices[0]);
    
    if (entry == MSG_VIEWINDEXNONE) 
      return;
    
    AB_GetType(m_dir, m_AddrBook, entry, &type);
    
    /* Select
    */
    if (type == ABTypePerson)
      popupUserPropertyWindow(entry, False, True);
    else if (type == ABTypeList)
      popupListPropertyWindow(entry, False, True);
#endif /* USE_ABCOM */
  }/* if */
}


int XFE_ABListSearchView::addToAddressBook() 
{

	if (m_dir->dirType != LDAPDirectory)
		return -1;

	uint32 count = 0;
	const int *indices = 0;
	m_outliner->getSelection(&indices, (int *) &count);
	DIR_Server *pABDir = NULL;
	DIR_GetPersonalAddressBook(m_directories, &pABDir);

	if (!pABDir)
		return -1;

	int errorID = -1;

    ABID entry;
	PersonEntry pPerson;
	if (count == 1) {
		/* pop up property sheet
		 */
		if (MSG_VIEWINDEXNONE == 
			(entry=AB_GetEntryIDAt((AddressPane *) m_abPane, 
								   (uint32) indices[0])))
			return -1;

		pPerson.Initialize();

		idToPerson(m_dir, entry, &pPerson);
		if (TRUE == 
			(errorID=FE_ShowPropertySheetFor(m_contextData, 
											 MSG_VIEWINDEXNONE, &pPerson))) {

		}/* if */
		else if (errorID == FALSE) {

		}/* else */
		else if (errorID == -1) {
		}/* else if */

		pPerson.CleanUp();			

	}/* if */
	else {
		for (int i=0; i < count; i++){
			if (MSG_VIEWINDEXNONE == 
				(entry=AB_GetEntryIDAt((AddressPane *) m_abPane, 
									   (uint32) indices[i])))
				continue;

			pPerson.Initialize();
			idToPerson(m_dir, entry, &pPerson);

			ABID newEntry;
			errorID = AB_AddUser(pABDir, m_AddrBook, 
								 &pPerson, &newEntry);
			if (errorID != 0 ||
				newEntry == MSG_VIEWINDEXNONE) {
				if (MK_ADDR_ENTRY_ALREADY_EXISTS == errorID)
					fe_Alert_2(getBaseWidget(),
							   XP_GetString(XFE_ADDR_ENTRY_ALREADY_EXISTS));
				else {
					char tmp[128];
					XP_SAFE_SPRINTF(tmp, sizeof(tmp),
									"%s",
									XP_GetString(errorID));
					fe_Alert_2(getBaseWidget(), tmp);
				}/* else */
			}/* if */
			pPerson.CleanUp();			
		}/* for i */
	}/* else */
	return TRUE;
}

void 
XFE_ABListSearchView::listChangeFinished(XP_Bool          /* asynchronous */,
										 MSG_NOTIFY_CODE  notify, 
										 MSG_ViewIndex    where,
										 int32            num)
{
	switch (notify) {
	case MSG_NotifyLDAPTotalContentChanged:
		{
#if defined(DEBUG_tao)
	printf("\nXFE_ABListSearchView::MSG_NotifyLDAPTotalContentChanged, where=%d, num=%d", where, num);
#endif
			m_outliner->change(0, num, num);
			m_outliner->scroll2Item((int) where);
		}
		break;

	case MSG_NotifyInsertOrDelete:
		if (m_searchingDir)
#if defined(USE_ABCOM)
			{
				int error = AB_LDAPSearchResultsAB2(m_pane,
													where, num);
#if 1
				//fe_UpdateGraph(m_contextData, True);
#else
				notifyInterested(XFE_Component::progressBarCylonTick);
#endif
			}
#else
		AB_LDAPSearchResults((ABPane*)m_pane, where, num);
#endif /* USE_ABCOM */
		break;

	}/* notify */
}

void 
XFE_ABListSearchView::paneChanged(XP_Bool asynchronous,
								  MSG_PANE_CHANGED_NOTIFY_CODE notify_code,
								  int32 value)
{
#if defined(DEBUG_tao)
	printf("\nXFE_ABListSearchView::paneChanged, asynchronous=%d, notify_code=%d, value=0x%x", asynchronous, notify_code, value);
#endif
	// remove the timer
	if (m_typeDownTimer) {
		XtRemoveTimeOut(m_typeDownTimer);
		m_typeDownTimer = 0;
	}/* if */

	switch (notify_code) {
	case MSG_PaneDirectoriesChanged:
		dirChanged(value);
		break;

	case MSG_PaneChanged: 
		break;

	case MSG_PaneClose: 
		break;

	case MSG_PaneNotifyStartSearching:
		m_searchingDir = True;
#if 1
		//fe_UpdateGraph(m_contextData, True);
#else
		notifyInterested(XFE_Component::progressBarCylonStart);
#endif 
		break;

	case MSG_PaneNotifyStopSearching:
#if 1
		//fe_StopProgressGraph(m_contextData);
#else
		notifyInterested(XFE_Component::progressBarCylonStop);
#endif 
		m_searchingDir = False;
		break;

	case MSG_PaneNotifyTypeDownCompleted:
		{
#if defined(DEBUG_tao)
			printf("\nMSG_PaneNotifyTypeDownCompleted\n");
#endif 
			m_typeDownIndex = (MSG_ViewIndex) value;

			/* Steal from SubAllView.cpp
			 */
			if (m_typeDownIndex == MSG_VIEWINDEXNONE) {
				m_outliner->deselectAllItems();
			}/* if */
			else {
				m_outliner->selectItemExclusive(m_typeDownIndex);
				m_outliner->makeVisible(m_typeDownIndex);
			}/* else */
			getToplevel()->
				notifyInterested(XFE_View::chromeNeedsUpdating);
		}
		break;

	}/* notify_code */
}

void
XFE_ABListSearchView::dirChanged(int32 /* value */)
{

#if defined(USE_ABCOM)
	// rewrite with new APIs
	XP_ASSERT(0);

#else
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
		if (nDirs) {
			/* lucky me!!
			 * assign the first one
			 */
			m_dir = (DIR_Server *) XP_ListGetObjectNum(m_directories, 1);
		}/* if */
	}/* if */

	/* And the combo list
	 */
	refreshCombo();

	/* notify parent
	 */
	notifyInterested(XFE_ABListSearchView::dirsChanged, m_directories);
#endif 
}

void
XFE_ABListSearchView::refreshCombo()
{
	// remove the timer
	if (m_typeDownTimer) {
		XtRemoveTimeOut(m_typeDownTimer);
		m_typeDownTimer = 0;
	}/* if */

#if defined(USE_ABCOM)
	uint32 nDirs = 0;
	XFE_AB2PaneView *parV = (XFE_AB2PaneView *) getParent();
	const AB_ContainerInfo **ctrArray = parV->getRootContainers(nDirs);

	XmString xmstr = 0;
	for (int i=0; i < nDirs; i++) {
		const DIR_Server *dir = 
			AB_GetDirServerForContainer((AB_ContainerInfo *) ctrArray[i]);
		
		if (dir) {
			if (m_ldapDisabled && 
				dir->dirType == LDAPDirectory)
				continue;
			
			xmstr = XmStringCreateLtoR(dir->description, 
									   XmSTRING_DEFAULT_CHARSET);
			DtComboBoxAddItem(m_filterDirCombo, xmstr, 0, True );
			// 
			XmStringFree(xmstr);
		}/* if */
	}/* for i*/

	// set the current dir
	// use containerInfo
	if (m_dir) {
		if (m_dir->description) {
			xmstr = XmStringCreateLtoR(m_dir->description, 
									   XmSTRING_DEFAULT_CHARSET);
			DtComboBoxSelectItem(m_filterDirCombo, xmstr);
			XmStringFree(xmstr);
		}/* if */

		// TODO: convert to abcom
		AB_ChangeDirectory(m_abPane, m_dir);
	}/* if */


#else
	if (m_directories) {
		DtComboBoxDeleteAllItems(m_filterDirCombo);
		int nDirs = XP_ListCount(m_directories);
		XmString xmstr;
		for (int i=0; i < nDirs; i++) {
			DIR_Server *dir = 
				(DIR_Server *) XP_ListGetObjectNum(m_directories,i+1);
			if (dir) {
				if (m_ldapDisabled && 
					dir->dirType == LDAPDirectory)
					continue;
				xmstr = XmStringCreateLtoR(dir->description, 
										   XmSTRING_DEFAULT_CHARSET);
				DtComboBoxAddItem(m_filterDirCombo, xmstr, 0, True );
				XmStringFree(xmstr);
			}/* if */
		}/* for i*/

		// set the current dir
		if (m_dir) {
			if (m_dir->description) {
				xmstr = XmStringCreateLtoR(m_dir->description, 
										   XmSTRING_DEFAULT_CHARSET);
				DtComboBoxSelectItem(m_filterDirCombo, xmstr);
				XmStringFree(xmstr);
			}/* if */
			AB_ChangeDirectory(m_abPane, m_dir);
		}/* if */
	}/* if */
#endif
}

Widget
XFE_ABListSearchView::makeFilterBox(Widget parent, XP_Bool stopBtn)
{
  /* Create simple search area
   */

  /* child widgets
   */
  Widget filterPrompt;
  Widget filterTypeIn;

  /* The form
   */
  m_filterBoxForm = XtVaCreateManagedWidget("filterBoxForm",
											xmFormWidgetClass,
											parent,
											NULL);

  // create the arrow button stuff

  m_collapsedForm = XtVaCreateManagedWidget("collapsdForm",
										   xmFormWidgetClass,
										   m_filterBoxForm,
										   // XmNallowResize, FALSE,
										   // XmNskipAdjust, TRUE,
										   NULL);

  m_expandBtn = XtVaCreateManagedWidget("expandBtn",
										xmArrowButtonWidgetClass,
										m_collapsedForm,
										XmNleftAttachment, XmATTACH_FORM,
										XmNtopAttachment, XmATTACH_FORM,
										XmNbottomAttachment, XmATTACH_FORM,
										XmNrightAttachment, XmATTACH_NONE,
										XmNarrowDirection, XmARROW_DOWN,
										XmNshadowThickness, 0,
										NULL);

  XtAddCallback(m_expandBtn, 
				XmNactivateCallback, expandCallback, this);

  Arg av [20];
  int ac = 0;

#if 0
  XP_Bool ldapDisabled = False;
  PREF_GetBoolPref("mail.addr_book.ldap.disabled",&ldapDisabled);
#if 0
  XP_Bool locked = False;
  locked = PREF_PrefIsLocked("mail.addr_book.ldap.disabled");
  if (ldapDisabled || locked)
	  ldapDisabled = True;
#endif

#else
  /* Combo Box
   */
  /* Get visual, colormap, and depth first???
   */
  Visual   *v = 0;
  Colormap  cmap = 0;
  Cardinal  depth = 0;
  XtVaGetValues (getToplevel()->getBaseWidget(), 
				 XmNvisual, &v, 
				 XmNcolormap, &cmap,
                 XmNdepth, &depth, 
				 NULL);

  /* Create a combobox for storing directories 
   */
  ac = 0;

  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNmoveSelectedItemUp, False); ac++;
  XtSetArg (av[ac], XmNtype, XmDROP_DOWN_LIST_BOX); ac++;
  XtSetArg (av[ac], XmNvisibleItemCount, 8); ac++; 

  m_filterDirCombo = DtCreateComboBox(m_collapsedForm, 
				      "filterDirCombo", av,ac);
  XtVaSetValues(m_filterDirCombo,
				XmNleftAttachment, XmATTACH_WIDGET,
				XmNleftWidget, m_expandBtn,
				XmNtopAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNbottomAttachment, XmATTACH_FORM,
				NULL);
  XtManageChild(m_filterDirCombo);

  // Add fake items to comboBox
  XP_Bool ldapDisabled = False;
  PREF_GetBoolPref("mail.addr_book.ldap.disabled",&ldapDisabled);
#if 0
  XP_Bool locked = False;
  locked = PREF_PrefIsLocked("mail.addr_book.ldap.disabled");
  if (ldapDisabled || locked)
	  ldapDisabled = True;
#endif

  XmString xmstr;
#if defined(USE_ABCOM)

	uint32 nDirs = 0;
	XFE_AB2PaneView *parV = (XFE_AB2PaneView *) getParent();
	const AB_ContainerInfo **ctrArray = parV->getRootContainers(nDirs);

	for (int i=0; i < nDirs; i++) {
		const DIR_Server *dir = 
			AB_GetDirServerForContainer((AB_ContainerInfo *) ctrArray[i]);

		if (dir) {
			if (ldapDisabled && 
				dir->dirType == LDAPDirectory)
				continue;
			
			xmstr = XmStringCreateLtoR(dir->description, 
									   XmSTRING_DEFAULT_CHARSET);
			DtComboBoxAddItem(m_filterDirCombo, xmstr, 0, True );
			// 
			XmStringFree(xmstr);
		}/* if */
	}/* for i*/
#else
  DIR_Server *dirTop = NULL;
  int nDirs = XP_ListCount(m_directories);
  for (int i=0; i < nDirs; i++) {
    DIR_Server *dir = (DIR_Server *) XP_ListGetObjectNum(m_directories,i+1);
    if (dir) {
		if (ldapDisabled && 
			dir->dirType == LDAPDirectory)
			continue;

     xmstr = XmStringCreateLtoR(dir->description, 
				XmSTRING_DEFAULT_CHARSET);
     DtComboBoxAddItem(m_filterDirCombo, xmstr, 0, True );
	 // 
	 if (!dirTop)
		 dirTop = dir;
     XmStringFree(xmstr);
    }/* if */
  }/* for i*/

  // search button sensitivity
  XP_Bool searchOn = True;
  if (dirTop) {
	  if (dirTop->dirType == PABDirectory) {
		  searchOn = False;
     }/* if */
      else {
		  searchOn = True;
      }/* else */
  }/* if */
#endif /* USE_ABCOM */

#endif /* !0 */
  /* Prompt
   */
  filterPrompt = XtVaCreateManagedWidget("filterPrompt",
										 xmLabelWidgetClass,
										 m_filterBoxForm,
										 XmNtopAttachment, XmATTACH_FORM,
										 XmNtopOffset, 4,
										 XmNleftAttachment, XmATTACH_FORM,
										 XmNleftOffset, 2,
										 XmNrightAttachment, XmATTACH_NONE,
										 XmNbottomAttachment, XmATTACH_NONE,
										 NULL);
  m_filterPrompt = filterPrompt;


  XtVaSetValues(m_collapsedForm,
				XmNleftAttachment, XmATTACH_FORM,
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, filterPrompt,
				XmNtopOffset, 4,
				XmNrightAttachment, XmATTACH_NONE,
				XmNbottomAttachment, XmATTACH_FORM,
				NULL);
  filterTypeIn = XtVaCreateManagedWidget("filterTypeIn",
										 xmTextFieldWidgetClass,
										 m_filterBoxForm,
										 XmNtopAttachment, XmATTACH_WIDGET,
										 XmNtopWidget, filterPrompt,
										 XmNtopOffset, 4,
										 XmNleftAttachment, XmATTACH_WIDGET,
										 XmNleftWidget, m_collapsedForm,
										 XmNleftOffset, 3,
										 XmNrightAttachment, XmATTACH_NONE,
										 XmNbottomAttachment, XmATTACH_FORM,
										 //XmNbottomOffset, 3,
										 NULL);

  /* stop button
   */
  if (stopBtn) {
	  ac = 0;
	  XtSetArg(av[ac], XmNwidth, 100); ac++;
#if !defined(USE_ABCOM)
	  XtSetArg(av[ac], XtNsensitive, False), ac++;
#endif
	  XtSetArg(av[ac], XmNrecomputeSize, False), ac++;
	  m_filterStopBtn = XmCreatePushButton(m_filterBoxForm, 
											 "filterStopBtn", 
											 av, ac);
	  if (!ldapDisabled)
		  XtManageChild(m_filterStopBtn);
	  
	  XtVaSetValues(m_filterStopBtn,
					XmNleftAttachment, XmATTACH_NONE,
					XmNtopAttachment, XmATTACH_NONE,
					XmNrightAttachment, XmATTACH_FORM,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNrightOffset, 4,
					XmNbottomOffset, 4,
					NULL);
  }/* if */

  /* search button
   */
  ac = 0;
  XtSetArg(av[ac], XmNwidth, 100); ac++;
#if !defined(USE_ABCOM)
  XtSetArg(av[ac], XtNsensitive, searchOn), ac++;
#endif
  XtSetArg(av[ac], XmNrecomputeSize, False), ac++;
  m_filterSearchBtn = XmCreatePushButton(m_filterBoxForm, 
					 "filterSearchBtn", 
					 av, ac);
  if (!ldapDisabled)
	  XtManageChild(m_filterSearchBtn);

  if (stopBtn)
	  XtVaSetValues(m_filterSearchBtn,
					XmNleftAttachment, XmATTACH_NONE,
					XmNtopAttachment, XmATTACH_NONE,
					XmNrightAttachment, XmATTACH_WIDGET,
					XmNrightWidget, m_filterStopBtn,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNrightOffset, 3,
					XmNbottomOffset, 4,
					NULL);
  else
	  XtVaSetValues(m_filterSearchBtn,
					XmNleftAttachment, XmATTACH_NONE,
					XmNtopAttachment, XmATTACH_NONE,
					XmNrightAttachment, XmATTACH_FORM,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNrightOffset, 4,
					XmNbottomOffset, 4,
					NULL);
  if (stopBtn)
	  XtVaSetValues(filterTypeIn,
					XmNrightAttachment, XmATTACH_WIDGET,
					XmNrightWidget, m_filterSearchBtn,
					XmNrightOffset, 2,
					NULL);
  

  /* m_filterBoxForm
   */
  XtVaSetValues(m_filterBoxForm,
				XmNleftAttachment, XmATTACH_FORM,
				XmNtopAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNbottomAttachment, XmATTACH_NONE,
				NULL);

  /* manage this form
   */
  XtManageChild(m_filterBoxForm);

  /* Provide callbacks as for entries outside world
   */
  XtAddCallback(m_filterDirCombo, 
		XmNselectionCallback, 
		XFE_ABListSearchView::comboSelCallback, 
		this);
  XtAddCallback(filterTypeIn, 
		XmNvalueChangedCallback, 
		XFE_ABListSearchView::typeDownCallback, 
		this);
  XtAddCallback(filterTypeIn, 
		XmNactivateCallback, 
		XFE_ABListSearchView::typeActivateCallback, 
		this);

  XtAddCallback(m_filterSearchBtn, 
		XmNactivateCallback,
		XFE_ABListSearchView::searchCallback, 
		this);

  if (stopBtn)
	  XtAddCallback(m_filterStopBtn, 
					XmNactivateCallback,
					XFE_ABListSearchView::searchCallback, 
					this);

  return m_filterBoxForm;
}

void XFE_ABListSearchView::layout()
{

  XtVaSetValues(m_outliner->getBaseWidget(),
				XmNleftAttachment, XmATTACH_FORM,
				XmNtopAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNbottomAttachment, XmATTACH_FORM,
				NULL);

}

XFE_CALLBACK_DEFN(XFE_ABListSearchView, allConnectionsComplete)(XFE_NotificationCenter */*obj*/, 
								void */*clientData*/, 
								void *callData)
{
	MWContext *c = (MWContext *) callData;
	if (c == m_contextData)
		stopSearch();
}

void
XFE_ABListSearchView::allConnectionsComplete(MWContext  *context)
{
#if defined(DEBUG_tao)
	printf("\n**XFE_ABListSearchView::allConnectionsComplete\n");
#endif
	XFE_View::allConnectionsComplete(context);
	if (context == m_contextData)
		stopSearch();
}

void
XFE_ABListSearchView::searchCallback(Widget w, 
									 XtPointer clientData, 
									 XtPointer callData)
{
  XFE_ABListSearchView *obj = (XFE_ABListSearchView *) clientData;

  if (!obj->isSearching())
	  obj->searchDlg(w, callData);
  else
	  obj->searchCB(w, callData);

}
void
XFE_ABListSearchView::searchDlg(Widget /* w */, 
								XtPointer /* callData */)
{
	if (!m_searchInfo) {
		m_searchInfo = 
			(ABSearchInfo_t *) XP_CALLOC(1, sizeof(ABSearchInfo_t));
		m_searchInfo->m_mode = AB_SEARCH_BASIC;
		m_searchInfo->m_obj = this;
		m_searchInfo->m_cbProc = &(XFE_ABListSearchView::searchDlgCB);
		m_searchInfo->m_logicOp = True; // and
	}/* if */
	m_searchInfo->m_dir = m_dir;

	fe_showABSearchDlg(getToplevel()->getBaseWidget(),
					   m_contextData,
					   m_searchInfo);
}

void
XFE_ABListSearchView::expandCallback(Widget /* w */, 
									 XtPointer clientData, 
									 XtPointer /* callData */)
{
  XFE_ABListSearchView *obj = (XFE_ABListSearchView *) clientData;
  obj->notifyInterested(XFE_ABListSearchView::dirExpand);
}

void
XFE_ABListSearchView::searchCB(Widget /* w */, 
			   XtPointer /* callData */)
{
	// remove the timer
	if (m_typeDownTimer) {
		XtRemoveTimeOut(m_typeDownTimer);
		m_typeDownTimer = 0;
	}/* if */

    if (m_dir && m_dir->dirType == LDAPDirectory) {
		if (!m_searchingDir) {
			if (m_searchStr &&
				XP_STRLEN(m_searchStr)) {
#if defined(USE_ABCOM)
				int error = AB_SearchDirectoryAB2(m_pane,
												  m_searchStr);
#else
				AB_SearchDirectory(m_abPane, m_searchStr);
#endif /* USE_ABCOM */
				m_searchingDir = True;
				
				if (m_filterStopBtn) {
					XtSetSensitive(m_filterSearchBtn, False);
					XtSetSensitive(m_filterStopBtn, True);
				}/* if */
				else {
#if 1					
					XtSetSensitive(m_filterSearchBtn, False);
					getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
#else
					/* change label to Stop
					 */
					fe_SetString(w, XmNlabelString, 
								 XP_GetString(XFE_AB_STOP));
#endif
				}/* else */
			}/* if */
		}/* if */
		else
			stopSearch();
	}/* if */
}/* XFE_AddrSearchView::searchCB() */

void XFE_ABListSearchView::stopSearch()
{
  /* Stop 
   */
#if defined(USE_ABCOM)
  int error = AB_FinishSearchAB2(m_pane);
#else
  AB_FinishSearch(m_abPane, m_contextData);
#endif /* USE_ABCOM */
  m_searchingDir = False;
  
  if (m_filterStopBtn) {
	  XtSetSensitive(m_filterSearchBtn, True);
	  XtSetSensitive(m_filterStopBtn, False);
  }/* if */
  else {
#if 1
	  XtSetSensitive(m_filterSearchBtn, True);
	  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
#else
	  /* change label to Search
	   */
	  fe_SetString(m_filterSearchBtn, 
				   XmNlabelString, XP_GetString(XFE_AB_SEARCH_DLG));
#endif
  }/* else */
  
  uint32 count = 0;
#if defined(USE_ABCOM)
  count = MSG_GetNumLines(m_pane);
#else
  AB_GetEntryCount (m_dir, m_AddrBook, 
		    &count, (ABID) ABTypeAll, 0);
#endif

#if defined(DEBUG_tao)
    printf("\n XFE_ABListSearchView::stopSearch=%d,count=%d", 
		 m_outliner->getTotalLines(), count);
#endif

#if 0
	/* become annoying in tyepdown
	 */
  if (!count) {
	  char tmp[128];
	  XP_SAFE_SPRINTF(tmp, sizeof(tmp),
					  "%s",
					  XP_GetString(XFE_SEARCH_NO_MATCHES));
	  fe_Alert_2(getBaseWidget(), tmp);
  }/* if */
#endif /* 0 */

  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);

}/* */

XP_Bool XFE_ABListSearchView::isAscending()
{
#if defined(USE_ABCOM)
	return True; //AB_GetPaneSortedAscendingAB2(m_pane);
#else
	return AB_GetPaneSortedAscending(m_abPane);
#endif /* USE_ABCOM */
}

void XFE_ABListSearchView::setAscending(XP_Bool as)
{
  /* If need to re-sort
   */
  if (as != isAscending() || as != m_ascending) {
    /* Flip
     */

#if defined(USE_ABCOM)
	  setSortType(m_sortType); 
#else
	  setSortType((AB_CommandType)m_sortType); 
#endif /* USE_ABCOM */
  }/* if */
}

#if defined(USE_ABCOM)
void           
XFE_ABListSearchView::setSortType(AB_AttribID id)
{
	int error = AB_SortByAttribute(m_pane, id, m_ascending);

	// we shall not need to do so; but, just in case.
    m_outliner->invalidate();
}

AB_AttribID
XFE_ABListSearchView::getSortType()
{
	AB_AttribID attribID = AB_attribUnknown;
	int error = AB_GetPaneSortedByAB2(m_pane,
									  &attribID);
	return attribID;
}

#else
ABID XFE_ABListSearchView::getSortType()
{
  return AB_GetPaneSortedBy(m_abPane);
}

void XFE_ABListSearchView::setSortType(AB_CommandType type)
{
   switch (type) {
   case AB_SortByNickname:
   case AB_SortByCompanyName:
   case AB_SortByLocality:
   case AB_SortByTypeCmd:
   case AB_SortByFullNameCmd:
   case AB_SortByEmailAddress:
    AB_Command(m_abPane, type, NULL, 0);
    /* Flip m_ascending if same type is called
     */
    if (m_sortType == type)
      m_ascending = m_ascending?False:True;
    m_sortType = type;
    m_outliner->invalidate();
    break;


   default:
     XP_ASSERT(0);
     break;
  }/* switch */

}
#endif /* USE_ABCOM */


/* Methods for the outlinable interface.
 */
// Converts between an index and some non positional data type.
// Used to maintain selection when the list is reordered.
void *
XFE_ABListSearchView::ConvFromIndex(int /*index*/)
{
  return 0;
}

// Converts between an index and some non positional data type.
// Used to maintain selection when the list is reordered.
int
XFE_ABListSearchView::ConvToIndex(void */*item*/)
{
  return 0;
}

/* Methods for the outlinable interface.
 */
char *
XFE_ABListSearchView::getCellTipString(int row, int column)
{
	char *tmp = 0;
	if (row < 0) {
		/* header
		 */
		tmp = getColumnHeaderText(column);
	}/* if */
	else {
		ABID orgID = m_entryID;
		MSG_ViewIndex orgIndex = m_dataIndex;
		
		/* content 
		 */
		m_dataIndex = row;
#if defined(USE_ABCOM)
#if 0
		int error = AB_GetABIDForIndex(m_pane,
									   (MSG_ViewIndex) row,
									   &m_entryID);
#endif
		
#else
		m_entryID = AB_GetEntryIDAt((AddressPane *) m_abPane, (uint32) row);
#endif /* USE_ABCOM */
		tmp = getColumnText(column);
		
		/* reset
		 */
		m_dataIndex = orgIndex;
		m_entryID = orgID;
	}/* else */

	if (tmp && 
		(!m_outliner->isColTextFit(tmp, row, column)))
		return tmp;

	return NULL;
}

char *
XFE_ABListSearchView::getCellDocString(int /* row */, int /* column */)
{
	return NULL;
}

char*
XFE_ABListSearchView::getColumnName(int column)
{
  switch (column) {
    case OUTLINER_COLUMN_TYPE:	
		return XP_GetString(XFE_AB_HEADER_NAME);
    case OUTLINER_COLUMN_NAME:
		return XP_GetString(XFE_AB_HEADER_NAME);
    case OUTLINER_COLUMN_EMAIL:	
		return XP_GetString(XFE_AB_HEADER_EMAIL);
    case OUTLINER_COLUMN_PHONE:	
		return XP_GetString(XFE_AB_HEADER_PHONE);
    case OUTLINER_COLUMN_NICKNAME:	
		return XP_GetString(XFE_AB_HEADER_NICKNAME);
    case OUTLINER_COLUMN_COMPANY:	
		return XP_GetString(XFE_AB_HEADER_COMPANY);
    case OUTLINER_COLUMN_LOCALITY:	
		return XP_GetString(XFE_AB_HEADER_LOCALITY);
    default:			
		XP_ASSERT(0); 
		return 0;
    }
}

EOutlinerTextStyle 
XFE_ABListSearchView::getColumnHeaderStyle(int column)
{
#if defined(USE_ABCOM)
	AB_AttribID sortType = AB_attribUnknown;
	if (column >= 0 &&
		column < m_numAttribs) {
		AB_ColumnInfo *cInfo = AB_GetColumnInfo(m_containerInfo, 
												(AB_ColumnID) column);
		if (cInfo) {
			sortType = cInfo->sortable?cInfo->attribID:AB_attribUnknown;
			AB_FreeColumnInfo(cInfo);
		}/* if */
	}/* if */
#else
	ABID sortType = 0;
	switch (column) {
	case OUTLINER_COLUMN_TYPE:
		sortType = ABTypeEntry;
		break;
		
	case OUTLINER_COLUMN_NAME:
		sortType = ABFullName;
		break;
		
	case OUTLINER_COLUMN_EMAIL:
		sortType = ABEmailAddress;
		break;
		
	case OUTLINER_COLUMN_NICKNAME:
		sortType = ABNickname;
	  break;
	  
	case OUTLINER_COLUMN_COMPANY:
		sortType = ABCompany;
		break;
		
	case OUTLINER_COLUMN_LOCALITY:
		sortType = ABLocality;
		break;
	}/* switch */
#endif /* USE_ABCOM */

	if (sortType == getSortType())
		return OUTLINER_Bold;
	else
		return OUTLINER_Default;
}

/* Returns the text and/or icon to display at the top of the column.
 */
char*
XFE_ABListSearchView::getColumnHeaderText(int column)
{

#if defined(USE_ABCOM)
	a_line[0] = '\0';
	if (column >= 0 &&
		column < m_numAttribs) {
		AB_ColumnInfo *cInfo = AB_GetColumnInfo(m_containerInfo, 
												(AB_ColumnID) column);
		if (cInfo) {
			XP_SAFE_SPRINTF(a_line, sizeof(a_line),
							"%s",
							cInfo->displayString?cInfo->displayString:"");
			AB_FreeColumnInfo(cInfo);
		}/* if */
	}/* if */
	return a_line;
#else
  char *tmp = 0;
  switch (column) {
  case OUTLINER_COLUMN_TYPE:
    tmp = XP_STRDUP(" ");
    break;

  case OUTLINER_COLUMN_NAME:
    tmp = XP_GetString(XFE_AB_HEADER_NAME);
    break;

  case OUTLINER_COLUMN_NICKNAME:
    tmp = XP_GetString(XFE_AB_HEADER_NICKNAME);
    break;

  case OUTLINER_COLUMN_EMAIL:
    tmp = XP_GetString(XFE_AB_HEADER_EMAIL);
    break;

  case OUTLINER_COLUMN_PHONE:
    tmp = XP_GetString(XFE_AB_HEADER_PHONE);
    break;

  case OUTLINER_COLUMN_COMPANY:
    tmp = XP_GetString(XFE_AB_HEADER_COMPANY);
    break;

  case OUTLINER_COLUMN_LOCALITY:
    tmp = XP_GetString(XFE_AB_HEADER_LOCALITY);
    break;

  }/* switch () */

  return tmp;
#endif /* USE_ABCOM */
}

char*
XFE_ABListSearchView::getColumnText(int column)
{
  char *loc;

  a_line[0] = '\0';
#if defined(USE_ABCOM)
  AB_AttributeValue *value = 0;
  AB_AttribID attrib = AB_attribEntryType;
  XP_Bool isPhone = False;
#if 1
	if (column >= 0 &&
		column < m_numAttribs) {
		AB_ColumnInfo *cInfo = AB_GetColumnInfo(m_containerInfo,  
												(AB_ColumnID) column);
		if (cInfo) {
			attrib = cInfo->attribID;
			AB_FreeColumnInfo(cInfo);
		}/* if */
	}/* if */
#else
  switch (column) {
  case OUTLINER_COLUMN_TYPE:
	  break;

  case OUTLINER_COLUMN_NAME:
	  attrib = AB_attribDisplayName; // shall be AB_attribDisplayName;
	  break;

  case OUTLINER_COLUMN_NICKNAME:
	  attrib = AB_attribNickName;
	  break;
	  
  case OUTLINER_COLUMN_EMAIL:
	  attrib = AB_attribEmailAddress;
	  break;
	  
  case OUTLINER_COLUMN_PHONE:
	  isPhone = True;
	  attrib = AB_attribWorkPhone;
	  break;

  case OUTLINER_COLUMN_COMPANY:
	  attrib = AB_attribCompanyName;
	  break;
	  
  case OUTLINER_COLUMN_LOCALITY:
 	  attrib = AB_attribLocality;
	  break;
	  
  }/* switch () */
#endif 
  if (attrib != AB_attribEntryType) {
#if 1
	  int error = AB_GetEntryAttributeForPane(m_pane,
											  m_dataIndex, 
											  attrib,
											  &value);

	  if (isPhone && 
		  EMPTY_STRVAL(value)) {
		  error = AB_GetEntryAttributeForPane(m_pane,
											  m_dataIndex, 
											  AB_attribHomePhone, 
											  &value);
	  }/* if */
#else
	  int error = AB_GetEntryAttribute(m_containerInfo, m_entryID, 
									   attrib, &value);

	  if (isPhone && 
		  EMPTY_STRVAL(value)) {
		  error = AB_GetEntryAttribute(m_containerInfo, m_entryID, 
									   AB_attribHomePhone, &value);
	  }/* if */
#endif
	  XP_SAFE_SPRINTF(a_line, sizeof(a_line),
					  "%s",
					  EMPTY_STRVAL(value)?"":value->u.string);

	  AB_FreeEntryAttributeValue(value);
  }/* if */
#else
  switch (column) {
  case OUTLINER_COLUMN_TYPE:
    break;

  case OUTLINER_COLUMN_NAME:
    AB_GetFullName(m_dir, m_AddrBook, m_entryID, a_line);
	INTL_CONVERT_BUF_TO_LOCALE(a_line, loc);
    break;

  case OUTLINER_COLUMN_NICKNAME:
    AB_GetNickname(m_dir, m_AddrBook, m_entryID, a_line);
	INTL_CONVERT_BUF_TO_LOCALE(a_line, loc);
    break;

  case OUTLINER_COLUMN_EMAIL:
    AB_GetEmailAddress(m_dir, m_AddrBook, m_entryID, a_line);
    break;

  case OUTLINER_COLUMN_PHONE:
    AB_GetWorkPhone(m_dir, m_AddrBook, m_entryID, a_line);
	if (!a_line || !XP_STRLEN(a_line)) {
		a_line[0] = '\0';
		AB_GetHomePhone(m_dir, m_AddrBook, m_entryID, a_line);
	}/* if */
    break;

  case OUTLINER_COLUMN_COMPANY:
    AB_GetCompanyName(m_dir, m_AddrBook, m_entryID, a_line);
	INTL_CONVERT_BUF_TO_LOCALE(a_line, loc);
    break;

  case OUTLINER_COLUMN_LOCALITY:
    AB_GetLocality(m_dir, m_AddrBook, m_entryID, a_line);
	INTL_CONVERT_BUF_TO_LOCALE(a_line, loc);
    break;

  }/* switch () */
#endif /* !USE_ABCOM */
  return a_line;
}

fe_icon*
XFE_ABListSearchView::getColumnIcon(int column)
{
  fe_icon *myIcon = 0;
  switch (column) {
  case OUTLINER_COLUMN_TYPE:
    {
#if defined(USE_ABCOM)
		AB_EntryType type = getType(m_pane, m_dataIndex);
		if (type == AB_Person)
			myIcon = &m_personIcon; /* shall call make/initialize icons */
		else if (type == AB_MailingList)
			myIcon = &m_listIcon;
#else
      ABID type;
      AB_GetType(m_dir, m_AddrBook, m_entryID, &type);
      if (type == ABTypePerson)
	myIcon = &m_personIcon; /* shall call make/initialize icons */
      else if (type == ABTypeList)
	myIcon = &m_listIcon;
#endif /* USE_ABCOM */
    }
    break;
  }/* switch () */
  return myIcon;
}

/* This method acquires one line of data: entryID is set for getColumnText
 */
void*
XFE_ABListSearchView::acquireLineData(int line)
{
#if defined(USE_ABCOM)
	int error = AB_GetABIDForIndex(m_pane,
								   (MSG_ViewIndex) line,
								   &m_entryID);

	m_dataIndex = (error == AB_SUCCESS)?line:MSG_VIEWINDEXNONE;

#else
  m_entryID = AB_GetEntryIDAt((AddressPane *) m_abPane, (uint32) line);
#endif /* USE_ABCOM */

  if (m_entryID == MSG_VIEWINDEXNONE)
    return 0;
  else
    return (void*)m_entryID;
}

// Returns the text and/or icon to display at the top of the column.
fe_icon*
XFE_ABListSearchView::getColumnHeaderIcon(int /*column*/)
{
  return 0;
}

// Returns the text and/or icon to display at the top of the column.
/*
 * The following 4 requests deal with the currently acquired line.
 */
EOutlinerTextStyle 
XFE_ABListSearchView::getColumnStyle(int /*column*/)
{
  /* To be refined
   */
  return OUTLINER_Default;
}

//
void
XFE_ABListSearchView::getTreeInfo(XP_Bool */*expandable*/,
				   XP_Bool */*is_expanded*/, 
				   int *depth, 
				   OutlinerAncestorInfo **/*ancestor*/)
{
  depth = 0;
}

void 
XFE_ABListSearchView::clickHeader(const OutlineButtonFuncData *data)
{
  int column = data->column; 
  int invalid = True;
#if defined(USE_ABCOM)
#if 1
	if (column >= 0 &&
		column < m_numAttribs) {
		AB_ColumnInfo *cInfo = AB_GetColumnInfo(m_containerInfo,  
												(AB_ColumnID) column);
		if (cInfo) {
			setSortType(cInfo->attribID);
			AB_FreeColumnInfo(cInfo);
		}/* if */
	}/* if */
#else
  switch (column) {
    case OUTLINER_COLUMN_TYPE:
      setSortType(AB_attribEntryType);
      break;

    case OUTLINER_COLUMN_NAME:
      setSortType(AB_attribFullName);
      break;

    case OUTLINER_COLUMN_NICKNAME:
      setSortType(AB_attribNickName);
      break;

    case OUTLINER_COLUMN_EMAIL:
      setSortType(AB_attribEmailAddress);
      break;

    case OUTLINER_COLUMN_COMPANY:
      setSortType(AB_attribCompanyName);
      break;

    case OUTLINER_COLUMN_LOCALITY:
      setSortType(AB_attribLocality);
      break;
    default:
      invalid = False;
    }/* switch() */
#endif
#else
  switch (column) {
    case OUTLINER_COLUMN_TYPE:
      setSortType(AB_SortByTypeCmd);
      break;

    case OUTLINER_COLUMN_NAME:
      setSortType(AB_SortByFullNameCmd);
      break;

    case OUTLINER_COLUMN_NICKNAME:
      setSortType(AB_SortByNickname);
      break;

    case OUTLINER_COLUMN_EMAIL:
      setSortType(AB_SortByEmailAddress);
      break;

    case OUTLINER_COLUMN_COMPANY:
      setSortType(AB_SortByCompanyName);
      break;

    case OUTLINER_COLUMN_LOCALITY:
      setSortType(AB_SortByLocality);
      break;
    default:
      invalid = False;
    }/* switch() */
#endif /* USE_ABCOM */

  if (invalid)
    m_outliner->invalidate();
}

//
void 
XFE_ABListSearchView::Buttonfunc(const OutlineButtonFuncData* data)
{
  int row = data->row, 
      clicks = data->clicks;

  // focus
  notifyInterested(XFE_MNListView::changeFocus, this);

  if (row < 0) {
	  // focus
	  notifyInterested(XFE_View::chromeNeedsUpdating, this);

	  clickHeader(data);
	  return;
  } 
  else {
	  /* content row 
	   */
	  ABID entry;
	  
#if defined(USE_ABCOM)
	  int error = AB_GetABIDForIndex(m_pane,
									 (MSG_ViewIndex) row,
									 &entry);
#else
	  entry = AB_GetEntryIDAt((AddressPane *) m_abPane, (uint32) row);
#endif /* USE_ABCOM */

	  if (entry == MSG_VIEWINDEXNONE) 
		  return;
	  
	  if (clicks == 2) {
		  m_outliner->selectItemExclusive(data->row);
		  doubleClickBody(data);
	  }/* clicks == 2 */
	  else if (clicks == 1) {
		  if (data->ctrl)
			  {
				  m_outliner->toggleSelected(data->row);
			  }
		  else if (data->shift) {
			  // select the range.
			  const int *selected = 0;
			  int count = 0;
  
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
		  }/* else */

		  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
	  }/* clicks == 1 */
  }/* else */
}

void 
XFE_ABListSearchView::Flippyfunc(const OutlineFlippyFuncData */*data*/)
{
}

/* Tells the Outlinable object that the line data is no
 * longer needed, and it can free any storage associated with it.
 */ 
void
XFE_ABListSearchView::releaseLineData()
{
}

/*
 * Callbacks for outside world
 */
void XFE_ABListSearchView::comboSelCallback(Widget w, 
				   XtPointer clientData, 
				   XtPointer callData)
{
  XFE_ABListSearchView *obj = (XFE_ABListSearchView *) clientData;
  obj->comboSelCB(w, callData);
}


void
XFE_ABListSearchView::comboSelCB(Widget /* w */, XtPointer callData)
{
  DtComboBoxCallbackStruct *cbData = (DtComboBoxCallbackStruct *)callData;
  if (cbData->reason == XmCR_SELECT ) {
	  char* text = 0;
	  XmString str = cbData->item_or_text;
	  
	  XmStringGetLtoR(str, XmSTRING_DEFAULT_CHARSET, &text);
	  if (*text ) {
		  /* get the right dir server 
		   */
#if defined(USE_ABCOM)
		  uint32 nDirs = 0;
		  XFE_AB2PaneView *parV = (XFE_AB2PaneView *) getParent();
		  const AB_ContainerInfo **ctrArray = parV->getRootContainers(nDirs);
		  for (int i=0; i < nDirs; i++) {
			  const DIR_Server *dir = 
				  AB_GetDirServerForContainer((AB_ContainerInfo *) ctrArray[i]);
			  if (dir &&
				  dir->description && 
				  !XP_STRCMP(dir->description, text)) {
				  selectContainer((AB_ContainerInfo *) ctrArray[i]);
				  notifyInterested(XFE_ABListSearchView::dirSelect,
								   (void *) ctrArray[i]);
				  break;
			  }/* if */
		  }/* for i*/
		  
#else
		  int count = XP_ListCount(m_directories);
		  for (int i=0; i < count; i++) {
			  DIR_Server *dir = 
				  (DIR_Server *) XP_ListGetObjectNum(m_directories, i+1);
			  if (dir &&
				  dir->description && 
				  !XP_STRCMP(dir->description, text)) {
				  selectDir(dir);
				  break;
			  }/* if */
		  }/* for i */
#endif /* !USE_ABCOM */
	  }/* if */	
	  XtFree(text);	  
  }/* if */
}

void
XFE_ABListSearchView::typeActivateCallback(Widget /* w */, 
				   XtPointer clientData, 
				   XtPointer /* callData */)
{
  XFE_ABListSearchView *obj = (XFE_ABListSearchView *) clientData;
  if (obj->m_filterSearchBtn)
	  obj->searchCB(obj->m_filterSearchBtn, NULL);
}

void
XFE_ABListSearchView::typeDownTimerCallback(XtPointer closure, XtIntervalId *)
{
	XFE_ABListSearchView *obj = (XFE_ABListSearchView *) closure;

	if (obj->m_filterSearchBtn)
		obj->searchCB(obj->m_filterSearchBtn, NULL);
}

void
XFE_ABListSearchView::typeDownCallback(Widget w, 
				   XtPointer clientData, 
				   XtPointer callData)
{
  XFE_ABListSearchView *obj = (XFE_ABListSearchView *) clientData;
  obj->typeDownCB(w, callData);
}

void
XFE_ABListSearchView::typeDownCB(Widget w, 
			     XtPointer /* callData */)
{
  // unregister any timer if there is one
  if (m_typeDownTimer) {
	  XtRemoveTimeOut(m_typeDownTimer);
	  m_typeDownTimer = 0;
  }/* if */

  if (w) {
	  const char *str;
	  str = fe_GetTextField(w);
	  
	  m_searchStr = XP_STRDUP(str);
  }/* if w */

  if (!m_dir)
	  return;

  if (m_dir->dirType == LDAPDirectory) {
	  /* for PAB
	   */
	  m_typeDownIndex = MSG_VIEWINDEXNONE;

	  /* stop any search
	   */
	  stopSearch();

	  /* add a timer
	   */
	  unsigned long interval = 900;
	  PREF_GetIntPref("ldap_1.autoCompleteInterval", (int32 *) &interval);
#if defined(DEBUG_tao)
	  printf("\nldap_1.autoCompleteInterval=%d\n", interval);
#endif
	  m_typeDownTimer = XtAppAddTimeOut(fe_XtAppContext, interval,
										typeDownTimerCallback, this);

  }/* if */
  else if (m_dir->dirType == PABDirectory) {
    /* Do  type down 
     */
#if defined(USE_ABCOM)
	if (m_searchStr && !XP_STRLEN(m_searchStr))
		m_typeDownIndex = MSG_VIEWINDEXNONE;
		
	int error = AB_TypedownSearch(m_pane,
								  m_searchStr,
								  m_typeDownIndex);
#else
	MSG_ViewIndex startIndex = 0;
    int i;

    startIndex = 0;
	int count = 0;
	const int *indices = 0;
	m_outliner->getSelection(&indices, &count);
    if (count != 0)
		startIndex = *indices;
    /* Get the first matching row
     */
    MSG_ViewIndex index = 0;
    i = AB_GetIndexMatchingTypedown(m_abPane, 
									&index, m_searchStr, startIndex);
    /* Steal from SubAllView.cpp
     */
    if (index == MSG_VIEWINDEXNONE) {
      m_outliner->deselectAllItems();
    }/* if */
    else {
      m_outliner->selectItemExclusive(index);
      m_outliner->makeVisible(index);
    }/* else */
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
#endif /* USE_ABCOM */
  }/* if */
}/* XFE_ABListSearchView::typeDownCB() */

void XFE_ABListSearchView::changeEntryCount()
{
  uint32 count = 0;
#if defined(USE_ABCOM)
  count = MSG_GetNumLines(m_pane);
#if defined(DEBUG_tao)
  printf("\nXFE_ABListSearchView::changeEntryCount=%d\n", count);
#endif

#else
  AB_GetEntryCount(m_dir, m_AddrBook, 
				   &count, (ABID) ABTypeAll, 0);
#endif
  m_outliner->change(0, count, count);
}/* XFE_ABListSearchView::changeEntryCount() */

void XFE_ABListSearchView::unRegisterInterested()
{
  MWContext *top = XP_GetNonGridContext (m_contextData);
  XFE_Frame *f = ViewGlue_getFrame(top);
  f->unregisterInterest(XFE_Frame::allConnectionsCompleteCallback,
						this,
						(XFE_FunctionNotification)allConnectionsComplete_cb);
}

//
void XFE_ABListSearchView::expandCollapse(XP_Bool expand)
{
	if (m_expandBtn)
		XtVaSetValues(m_expandBtn, 
					  XmNarrowDirection, expand?XmARROW_DOWN:XmARROW_RIGHT,
					  NULL);
}

//
#if defined(USE_ABCOM)
void XFE_ABListSearchView::setContainerPane(MSG_Pane *pane)
{
	m_abContainerPane = pane;

	if (!m_containerInfo) {
	}/* if */
}

void XFE_ABListSearchView::selectContainer(AB_ContainerInfo *containerInfo)
{
	if (containerInfo == m_containerInfo)
		return;

	// remove the timer
	if (m_typeDownTimer) {
		XtRemoveTimeOut(m_typeDownTimer);
		m_typeDownTimer = 0;
	}/* if */

	/* refresh chrome as well
	 */
	int error = -1;
	if (m_containerInfo) {
		error = AB_ChangeABContainer(m_pane,
									 containerInfo);
	}/* if */
	else {
		error = AB_InitializeABPane(m_pane,
									containerInfo);
		// add callback here
		XP_ASSERT(m_outliner);
		if (m_outliner) {
			int32 nRows = 0;
			int   first = 0, last = 0;
			nRows = m_outliner->visibleRows(first, last);
			if (nRows < 10)
				nRows = 10;
			int error = AB_SetFEPageSizeForPane(m_pane, nRows); 
			XtAddCallback(m_outliner->getBaseWidget(), XmNresizeCallback, 
						  XFE_ABListSearchView::resizeCallback, 
						  (XtPointer) this);
		}/* m_outliner */
	}/* else */
		
	m_containerInfo = containerInfo;

	// get info on the fly
	int num_columns = AB_GetNumColumnsForContainer(m_containerInfo);
	AB_AttribID *attribIDs = (AB_AttribID *) XP_CALLOC(num_columns, 
													   sizeof(AB_AttribID));
	m_numAttribs = num_columns;
	error = AB_GetColumnAttribIDs(m_containerInfo,
								  attribIDs,
								  &m_numAttribs);
#if defined(DEBUG_tao_)
	AB_ColumnInfo *cInfo = 0;
	for (AB_ColumnID i=AB_ColumnID0; i < num_columns; i++) {
		cInfo = AB_GetColumnInfo(m_containerInfo, i);
		if (cInfo) {
			printf("\nID=%d, attribID=%d, displayString=%s, sortable=%d\n",
				   i, cInfo->attribID, cInfo->displayString, cInfo->sortable);
			AB_FreeColumnInfo(cInfo);
		}/* if */
	}/* for i */
#endif
	// TODO: take out this once BE support Notification
	changeEntryCount();

	/* TODO: remove refernce to dir
	 */
	/* dir
	 */
	m_dir = AB_GetDirServerForContainer(containerInfo);

	if (m_dir) {
		/* search button
		 */
		if (m_dir->dirType == PABDirectory) {
			XtSetSensitive(m_filterSearchBtn, False);
		}/* if */
		else {
			XtSetSensitive(m_filterSearchBtn, True);
		}/* else */
		if(m_filterStopBtn)
			XtSetSensitive(m_filterStopBtn, False);

		if (m_dir->description) {
			XmString xmstr = XmStringCreateLtoR(m_dir->description, 
												XmSTRING_DEFAULT_CHARSET);
			DtComboBoxSelectItem(m_filterDirCombo, xmstr);
			XmStringFree(xmstr);
		}/* if */
	}/* if */
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);  

	/* restart a new search when necessary
	 */
	m_typeDownIndex = MSG_VIEWINDEXNONE;
	typeDownCB(NULL, NULL);
}
#endif /* USE_ABCOM */

void XFE_ABListSearchView::selectLine(int /* line */)
{
	/* shall deal with a line in ABPane
	 */
}

void XFE_ABListSearchView::selectDir(DIR_Server* dir)
{
	if (dir == m_dir)
		return;

	if (!m_dir) {
		XP_ASSERT(m_outliner);
		if (m_outliner)
			XtAddCallback(m_outliner->getBaseWidget(), XmNresizeCallback, 
						  XFE_ABListSearchView::resizeCallback, 
						  (XtPointer) this);
	}/* if */

	// remove the timer
	if (m_typeDownTimer) {
		XtRemoveTimeOut(m_typeDownTimer);
		m_typeDownTimer = 0;
	}/* if */

	m_dir = dir;
	/* refresh chrome as well
	 */
	AB_ChangeDirectory(m_abPane, m_dir);
	if (m_dir->dirType == PABDirectory) {
		XtSetSensitive(m_filterSearchBtn, False);
	}/* if */
	else {
		XtSetSensitive(m_filterSearchBtn, True);
	}/* else */

	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating); 
 
	/* restart a new search when necessary
	 */
	m_typeDownIndex = MSG_VIEWINDEXNONE;
	typeDownCB(NULL, NULL);
}

void
XFE_ABListSearchView::searchDlgCB(ABSearchInfo_t *clientData, 
								  void */* callData */)
{
	XFE_ABListSearchView *view = (XFE_ABListSearchView *) clientData->m_obj;
	view->startSearch(clientData);
}

void
XFE_ABListSearchView::startSearch(ABSearchInfo_t *info)
{

	MSG_SearchFree((MSG_Pane *) m_abPane);
	MSG_SearchAlloc((MSG_Pane *) m_abPane);
	MSG_AddLdapScope((MSG_Pane *) m_abPane, info->m_dir);

	uint16             nRules = info->m_nRules;
	ABSearchUIParam_t *params = info->m_params;

	for (int i=0; i < nRules; i++) {
#if defined(DEBUG_tao)
		printf("\n(i,attribute,op,val)=(%d,%d,%d,%s)\n",
			   i,
			   params[i].m_attribNval.attribute,
			   params[i].m_op,
			   params[i].m_attribNval.u.string?
			   params[i].m_attribNval.u.string:"nil");
#endif
		if (!params[i].m_attribNval.u.string ||
			!XP_STRLEN(params[i].m_attribNval.u.string))
			continue;

		MSG_SearchError err =
			/* add a criterion line to the search 
			 */
#ifdef FE_IMPLEMENTS_BOOLEAN_OR
			MSG_AddSearchTerm(m_pane,
							  params[i].m_attribNval.attribute,
							  params[i].m_op,
							  &(params[i].m_attribNval),
							  info->m_logicOp,
							  NULL);
#else
		    MSG_AddSearchTerm(m_pane,
							  params[i].m_attribNval.attribute,
							  params[i].m_op,
							  &(params[i].m_attribNval));
#endif
		
	}/* for i */

	m_searchingDir = True;

#if 0
	/* Don't do this!
	 */
	/* change label to Stop
	 */
	fe_SetString(m_filterSearchBtn, 
				 XmNlabelString, XP_GetString(XFE_AB_STOP));
#endif

#if defined(USE_ABCOM)
	AB_SearchDirectoryAB2(m_pane, NULL);	
#else
	AB_SearchDirectory(m_abPane, NULL);	
#endif /* USE_ABCOM */
}

ABAddrMsgCBProcStruc* XFE_ABListSearchView::getSelections()
{
	int count = 0;
	const int *indices = 0;
	m_outliner->getSelection(&indices, &count);
	if (!count)
		return NULL;

	/* pack selected
	 */
	ABAddrMsgCBProcStruc *pairs = 
		(ABAddrMsgCBProcStruc *) XP_CALLOC(1, sizeof(ABAddrMsgCBProcStruc));
	pairs->m_pairs = (StatusID_t **) XP_CALLOC(count, sizeof(StatusID_t*)); 
	for (int i=0; i < count; i++) {
#if defined(USE_ABCOM)

		StatusID_t *pair;
		pair = (StatusID_t *) XP_CALLOC(1, sizeof(StatusID_t));
		pair->status = ::TO;
		pair->dir = 0;
		pair->id = MSG_VIEWINDEXNONE;

		// type, fulladdress
		uint16 numItems = 2;
		AB_AttribID *attribs = (AB_AttribID *) XP_CALLOC(numItems, 
													 sizeof(AB_AttribID));
		attribs[0] = AB_attribEntryType;
		attribs[1] = AB_attribFullAddress;
		AB_AttributeValue *values = NULL;

		int error = 
			AB_GetEntryAttributesForPane(m_pane,
										 (MSG_ViewIndex) indices[i],
										 attribs,
										 &values,
										 &numItems);
		XP_ASSERT(values);
		for (int i=0; i < numItems; i++) {
			switch (values[i].attrib) {
			case AB_attribEntryType:
				pair->type = values[i].u.entryType;
				break;

			case AB_attribFullAddress:
				pair->dplyStr = 
					!EMPTY_STRVAL(&(values[i]))?XP_STRDUP(values[i].u.string)
					:NULL;
				break;
			default:
				XP_ASSERT(0);
				break;
			}/* switch */
		}/* for i */
		XP_FREEIF(attribs);
#else
		ABID entry;
		entry = AB_GetEntryIDAt((AddressPane *) m_abPane, (uint32) indices[i]);
		if (entry == MSG_VIEWINDEXNONE) 
			continue;

		StatusID_t *pair;
		pair = (StatusID_t *) XP_CALLOC(1, sizeof(StatusID_t));
		pair->status = ::TO;
		pair->dir = m_dir;
		pair->id = entry;
		AB_GetType(m_dir, m_AddrBook, entry, &(pair->type));
		
		//email
		char a_line[AB_MAX_STRLEN];
		a_line[0] = '\0';
		AB_GetEmailAddress(m_dir, m_AddrBook, entry, a_line);
		pair->emailAddr = XP_STRDUP(a_line);

		// fullname
		a_line[0] = '\0';
		AB_GetFullName(m_dir, m_AddrBook, entry, a_line);
		
		// assemble
		pair->dplyStr = NULL;
		AB_GetExpandedName(m_dir, m_AddrBook, entry, &(pair->dplyStr));
		if (!pair->dplyStr) {
			// fullname
			a_line[0] = '\0';
			AB_GetFullName(m_dir, m_AddrBook, entry, a_line);
			pair->dplyStr = XP_STRDUP(a_line);
		}/* if */
#endif /* USE_ABCOM */

		pairs->m_pairs[pairs->m_count] = pair;
		(pairs->m_count)++;
	}/* for i */
	return pairs;
}/* XFE_ABListSearchView::getSelections() */

void 
XFE_ABListSearchView::resizeCallback(Widget w, 
									 XtPointer clientData, 
									 XtPointer callData)
{
	XFE_ABListSearchView *obj = (XFE_ABListSearchView *) clientData;
	obj->resizeCB(w, callData);
}

void
XFE_ABListSearchView::resizeCB(Widget /* w */, XtPointer callData)
{
	XmLGridCallbackStruct *cData = (XmLGridCallbackStruct *) callData;

	switch (cData->reason) {
	case XmCR_RESIZE_GRID:
		{
			int32 nRows = 0;
			int   first = 0, last = 0;
			nRows = m_outliner->visibleRows(first, last);
#if defined(DEBUG_tao)
			printf("\nXFE_ABListSearchView::resizeCB: nRows=%d, first=%d, last=%d\n", nRows, first, last);
#endif /* DEBUG_tao */
			if (nRows < 10)
				nRows = 10;
#if defined(USE_ABCOM)
			int error = AB_SetFEPageSizeForPane(m_pane, nRows); 
#endif /* USE_ABCOM */
		}
		break;

	}/* cData->reason */
}

#if defined(USE_MOTIF_DND)

fe_icon_data*
XFE_ABListSearchView::GetDragIconData(int row, int column)
{
	D(("XFE_ABListSearchView::GetDragIconData()\n"));
	/* TODO: get line data
	 * determine entry type
	 * return person/MN_Person, or, list/MN_People
	 */
	fe_icon_data *icon_data = 0;
	if (row < 0) {
#if defined(DEBUG_tao)
		printf("\n XFE_ABListSearchView::GetDragIconData (row,col)=(%d,%d)\n",
			   row, column);
#endif
		icon_data = &MN_Person; /* shall call make/initialize icons */
	}/* if */
	else {
		ABID entryID = AB_GetEntryIDAt((AddressPane *) m_abPane, (uint32) row);

		if (entryID == MSG_VIEWINDEXNONE)
			return icon_data;

		ABID type;
		AB_GetType(m_dir, m_AddrBook, entryID, &type);
		if (type == ABTypePerson)
			icon_data = &MN_Person; /* shall call make/initialize icons */
		else if (type == ABTypeList)
			icon_data = &MN_People;
	}/* else */
	return icon_data;
}

void
XFE_ABListSearchView::GetDragTargets(int    row, int column,
									 Atom **targets,
									 int   *num_targets)
{
	D(("XFE_ABListSearchView::GetDragTargets(row=%d, col=%d)\n",row,column));
	
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
		
		(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_PAB;
		(*targets)[1] = XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV;
	}/* else */
}

void 
XFE_ABListSearchView::getDropTargets(void */*this_ptr*/,
									 Atom **targets,
									 int  *num_targets)
{
	D(("XFE_ABListSearchView::getDropTargets()\n"));
	*num_targets = 2;
	*targets = new Atom[ *num_targets ];

	(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_PAB;
	(*targets)[1] = XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV;
}

char *
XFE_ABListSearchView::DragConvert(Atom atom)
{
	/* pack data
	 */
	if (atom == XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV) {
#if defined(DEBUG_tao)
		printf("\nXFE_ABListSearchView::DragConvert:_XA_NETSCAPE_DIRSERV\n");
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
		printf("\nXFE_ABListSearchView::DragConvert:_XA_NETSCAPE_DIRSERV=%x\n", buf);
#endif
		return buf;
	}/* if */
	else if (atom == XFE_OutlinerDrop::_XA_NETSCAPE_PAB)	{
#if defined(DEBUG_tao)
		printf("\nXFE_ABListSearchView::DragConvert:_XA_NETSCAPE_PAB\n");
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
		printf("\nXFE_ABListSearchView::DragConvert:_XA_NETSCAPE_PAB=%x\n", buf);
#endif
		return buf;
	}/* else if */
	return (char *) NULL;
}

int
XFE_ABListSearchView::ProcessTargets(int row, int col,
									 Atom *targets,
									 const char **data,
									 int numItems)
{
	int i;

	D(("XFE_ABListSearchView::ProcessTargets(row=%d, col=%d, numItems=%d)\n", row, col, numItems));
	
	for (i=0; i < numItems; i++) {
		if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
			continue;
		
		D(("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(m_widget),targets[i]),data[i]));
		if (targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_PAB) {
#if defined(DEBUG_tao)
			printf("\nXFE_ABListSearchView::ProcessTargets:_XA_NETSCAPE_PAB\n");
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
		else if (targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_DIRSERV) {
#if defined(DEBUG_tao)
			printf("\nXFE_ABListSearchView::ProcessTargets:_XA_NETSCAPE_DIRSERV\n");
#endif		
			return TRUE;
		}/* else if */
	}/* for i */
	return FALSE;
}

/* external entries
 */
fe_icon_data*
XFE_ABListSearchView::getDragIconData(void *this_ptr,
									  int   row,
									  int   column)
{
	XFE_ABListSearchView *view = (XFE_ABListSearchView*)this_ptr;	
	return view->GetDragIconData(row, column);
}

void
XFE_ABListSearchView::getDragTargets(void  *this_ptr,
									 int    row, int column,
									 Atom **targets, int *num_targets)
{
	XFE_ABListSearchView *view = (XFE_ABListSearchView*)this_ptr;	
	view->GetDragTargets(row, column, targets, num_targets);
}

char *
XFE_ABListSearchView::dragConvert(void *this_ptr,
								  Atom atom)
{
  XFE_ABListSearchView *view = (XFE_ABListSearchView*) this_ptr;
  
  return view->DragConvert(atom);
}

int
XFE_ABListSearchView::processTargets(void *this_ptr,
									 int row, int col,
									 Atom *targets,
									 const char **data,
									 int numItems)
{
	XFE_ABListSearchView *view = (XFE_ABListSearchView*)this_ptr;
	
	return view->ProcessTargets(row, col, targets, data, numItems);
}

#else /* USE_MOTIF_DND */

// Address field drop site handler
void
XFE_ABListSearchView::entryListDropCallback(Widget, 
											void* cd,
											fe_dnd_Event type,
											fe_dnd_Source *source,
#if defined(USE_ABCOM)
											XEvent* event) 
#else
											XEvent* /* event */) 
#endif /* USE_ABCOM */
{
    XFE_ABListSearchView *ad = (XFE_ABListSearchView *)cd;
    if (type == FE_DND_DROP && 
		ad && 
		source)
#if defined(USE_ABCOM)
        ad->entryListDropCB(source, event);
#else
        ad->entryListDropCB(source);
#endif /* USE_ABCOM */
}

#if defined(USE_ABCOM)
void
XFE_ABListSearchView::entryListDropCB(fe_dnd_Source *source, XEvent *event)
{
#if defined(DEBUG_tao)
	printf("\nXFE_ABListSearchView::entryListDropCB:srcType=%d\n",
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
	if (row < 0 ||
		row >= m_outliner->getTotalLines())
		XP_ASSERT(0);

#if defined(DEBUG_tao)
	printf("\nXFE_ABListSearchView::entryListDropCallback,shift=%d, ctrl=%d, row=%d",
		   shift, ctrl, row);
#endif
	AB_ContainerInfo *containerInfo = 
		AB_GetContainerForIndex(m_pane, (MSG_ViewIndex) row);

    switch (source->type) {
    case FE_DND_ADDRESSBOOK:
	case FE_DND_BOOKS_DIRECTORIES: {
		XFE_MNListView* listView = (XFE_MNListView *) source->closure;
		XFE_Outliner *outliner = listView->getOutliner();
		const int *indices = NULL;
		int32 numIndices = 0;
		outliner->getSelection(&indices, (int *) &numIndices);
		MSG_Pane *srcPane = listView->getPane();
		AB_DragEffect effect = 
			AB_DragEntriesIntoContainerStatus(srcPane,
											  (const MSG_ViewIndex *) indices,
											  (int32) numIndices,
											  m_containerInfo,
											  AB_Default_Drag); 
		int error = 0;
		if (effect == AB_Drag_Not_Allowed)
			return;
		else
			error = 
				AB_DragEntriesIntoContainer(srcPane,
											(const MSG_ViewIndex *) indices,
											(int32) numIndices,
											containerInfo?containerInfo
											             :m_containerInfo,
											effect);
	}
	
	break;
		
    default:
        break;
    }/* switch */
}

#else
void
XFE_ABListSearchView::entryListDropCB(fe_dnd_Source *source)
{
    switch (source->type) {

    case FE_DND_ADDRESSBOOK: {
#if defined(DEBUG_tao)
		printf("\nXFE_ABListSearchView::entryListDropCB:FE_DND_ADDRESSBOOK\n");
#endif
		break;
	}

	case FE_DND_BOOKS_DIRECTORIES: {
#if defined(DEBUG_tao)
		printf("\nXFE_ABListSearchView::entryListDropCB:FE_DND_BOOKS_DIRECTORIES\n");
#endif
		
		break;
	}
    default:
        break;
    }
}

#endif

#endif /* !USE_MOTIF_DND */

#if defined(USE_ABCOM)
extern "C" int 
FE_ShowPropertySheeetForAB2(MSG_Pane *pane,
							AB_EntryType entryType)
{
	if (entryType == AB_MailingList) {
		
	}/* if */
	else if (entryType == AB_Person) {
		
	}/* else */
	return FALSE;
}
#endif /* USE_ABCOM */

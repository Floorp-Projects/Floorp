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
   ABNameFolderDlg.cpp -- class definition for ABNameFolderDlg
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#include "ABNameFolderDlg.h"
#include "PropertySheetView.h"

#include "ABNameGenTab.h"
#include "ABNameConTab.h"

#if 0
/* out for 4.x and 5.0
 */
#include "ABNameSecuTab.h"
#endif

#include "ABNameCTalkTab.h"

#include "AddrBookView.h"

extern "C" {
XP_List* FE_GetDirServers();
ABook*   fe_GetABook(MWContext *context);
};

#include "xpgetstr.h"

extern int XFE_AB_NAME_CARD_FOR;
extern int XFE_AB_NAME_NEW_CARD;
extern int XFE_ADDR_ENTRY_ALREADY_EXISTS;
extern int MK_ADDR_ENTRY_ALREADY_EXISTS;
//
// This is the dialog it self
//
XFE_ABNameFolderDlg::XFE_ABNameFolderDlg(Widget    parent,
										 char     *name,
										 Boolean   modal,
										 MWContext *context):
	XFE_PropertySheetDialog((XFE_View *)0, parent, name,
						context,
						TRUE,    /* ok */
						TRUE,    /* cancel */
						TRUE,    /* help */
						FALSE,   /* apply */
						FALSE,   /* separator */
						modal)
{
  m_newUser = True;
  m_entry = MSG_VIEWINDEXNONE;

  //
  m_personEntry.Initialize();

  /*  protected: m_view == folder view
   */
  XFE_PropertySheetView* folderView = (XFE_PropertySheetView *) m_view;

  /* Add tabs
   */
  /* Gen
   */
  folderView->addTab(new XFE_ABNameGenTabView((XFE_Component *)this, 
					      folderView));
  folderView->addTab(new XFE_ABNameConTabView((XFE_Component *)this, 
					      folderView));
#if 0
  /* out for 4.x and 5.0
   */
  folderView->addTab(new XFE_ABNameSecuTabView((XFE_Component *)this, 
					       folderView));
#endif
  folderView->addTab(new XFE_ABNameCTalkTabView((XFE_Component *)this, 
						folderView));
}

#if defined(USE_ABCOM)
XFE_ABNameFolderDlg::XFE_ABNameFolderDlg(MSG_Pane *personPane,
										 MWContext *context):
	XFE_PropertySheetDialog((XFE_View *)0, 
						CONTEXT_WIDGET(context), 
						"abCardProperties",
						context,
						TRUE,    /* ok */
						TRUE,    /* cancel */
						TRUE,    /* help */
						FALSE,   /* apply */
						FALSE,   /* separator */
						TRUE)

{
	m_newUser = True;
	m_entry = MSG_VIEWINDEXNONE;
	
	/*  protected: m_view == folder view
	 */
	XFE_PropertySheetView* folderView = (XFE_PropertySheetView *) m_view;

	/* Add tabs
	 */
	/* Gen
	 */
	folderView->addTab(new XFE_ABNameGenTabView((XFE_Component *)this, 
												folderView));
	folderView->addTab(new XFE_ABNameConTabView((XFE_Component *)this, 
												folderView));
	folderView->addTab(new XFE_ABNameCTalkTabView((XFE_Component *)this, 
												  folderView));

	setDlgValues(personPane);
}
#endif /* USE_ABCOM */

XFE_ABNameFolderDlg::~XFE_ABNameFolderDlg()
{
#if defined(USE_ABCOM)
  int error = AB_ClosePane(((XFE_MNView *) m_view)->getPane());
#else
  m_personEntry.CleanUp();

#endif /* USE_ABCOM */
}

#if defined(USE_ABCOM)
void 
XFE_ABNameFolderDlg::setDlgValues(MSG_Pane *pane)
{
	/* super class' */
	((XFE_MNView *) m_view)->setPane(pane);
	m_entry = AB_GetABIDForPerson(pane);
	m_newUser = m_entry?False:True;

	XFE_PropertySheetDialog::setDlgValues();
}
#endif /* USE_ABCOM */

void XFE_ABNameFolderDlg::setDlgValues(ABID entry, PersonEntry* pPerson,
									   Boolean newUser)
{
  m_newUser = newUser;
  m_entry = entry;

  /* copy person data
   */
  if (pPerson) {
	  if (pPerson->pNickName)
		  m_personEntry.pNickName = XP_STRDUP(pPerson->pNickName);

	  if (pPerson->pGivenName)
		  m_personEntry.pGivenName = XP_STRDUP(pPerson->pGivenName);

	  if (pPerson->pMiddleName)
		  m_personEntry.pMiddleName = XP_STRDUP(pPerson->pMiddleName);
	  
	  if (pPerson->pFamilyName)
		  m_personEntry.pFamilyName = XP_STRDUP(pPerson->pFamilyName);
	  
	  if (pPerson->pCompanyName)
		  m_personEntry.pCompanyName = XP_STRDUP(pPerson->pCompanyName);
	  
	  if (pPerson->pLocality)
		  m_personEntry.pLocality = XP_STRDUP(pPerson->pLocality);

	  if (pPerson->pRegion)
		  m_personEntry.pRegion = XP_STRDUP(pPerson->pRegion);

	  if (pPerson->pEmailAddress)
		  m_personEntry.pEmailAddress = XP_STRDUP(pPerson->pEmailAddress);

	  if (pPerson->pInfo)
		  m_personEntry.pInfo = XP_STRDUP(pPerson->pInfo);

	  m_personEntry.HTMLmail = pPerson->HTMLmail;

	  if (pPerson->pTitle)
		  m_personEntry.pTitle = XP_STRDUP(pPerson->pTitle);

	  if (pPerson->pAddress)
		  m_personEntry.pAddress = XP_STRDUP(pPerson->pAddress);

	  if (pPerson->pPOAddress)
		  m_personEntry.pPOAddress = XP_STRDUP(pPerson->pPOAddress);
	  
	  if (pPerson->pZipCode)
		  m_personEntry.pZipCode = XP_STRDUP(pPerson->pZipCode);

	  if (pPerson->pCountry)
		  m_personEntry.pCountry = XP_STRDUP(pPerson->pCountry);

	  if (pPerson->pWorkPhone)
		  m_personEntry.pWorkPhone = XP_STRDUP(pPerson->pWorkPhone);

	  if (pPerson->pFaxPhone)
		  m_personEntry.pFaxPhone = XP_STRDUP(pPerson->pFaxPhone);

	  if (pPerson->pHomePhone)
		  m_personEntry.pHomePhone = XP_STRDUP(pPerson->pHomePhone);

	  m_personEntry.Security = pPerson->Security;

	  if (pPerson->pCoolAddress)
		  m_personEntry.pCoolAddress = XP_STRDUP(pPerson->pCoolAddress);

	  m_personEntry.UseServer = pPerson->UseServer;
  }/* if */

  if (!m_newUser && m_entry != MSG_VIEWINDEXNONE) {

	  DIR_Server *dir = getABDir();
	  ABook      *aBook = fe_GetABook(0);
	  char        a_line[AB_MAX_STRLEN];
	  a_line[0] = '\0';
	  if (AB_GetFullName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE) {
		  char tmp[AB_MAX_STRLEN];
		  XP_SAFE_SPRINTF(tmp, sizeof(tmp),
						  XP_GetString(XFE_AB_NAME_CARD_FOR),
						  a_line);
		  setCardName(tmp);
	  }/* if AB_GetFullName */
  }/* if !m_newUser && m_entry != MSG_VIEWINDEXNONE */
  else {
	  setCardName(XP_GetString(XFE_AB_NAME_NEW_CARD));
  }/* else */
  
  XFE_PropertySheetDialog::setDlgValues();
}

void XFE_ABNameFolderDlg::setDlgValues(ABID entry, Boolean newUser)
{
  m_newUser = newUser;
  m_entry = entry;

  if (!m_newUser && m_entry != MSG_VIEWINDEXNONE) {
	  DIR_Server *dir = getABDir();
	  ABook      *aBook = fe_GetABook(0);

	  char        a_line[AB_MAX_STRLEN];

	  a_line[0] = '\0';
	  if (AB_GetNickname(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pNickName = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetGivenName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pGivenName = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetMiddleName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pMiddleName = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetFamilyName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pFamilyName = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetCompanyName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pCompanyName = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetLocality(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pLocality = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetRegion(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pRegion = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetEmailAddress(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pEmailAddress = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetInfo(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pInfo = XP_STRDUP(a_line);
	  
	  AB_GetHTMLMail(dir, aBook, entry, &(m_personEntry.HTMLmail));
	  
	  a_line[0] = '\0';
	  if (AB_GetTitle(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pTitle = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetStreetAddress(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pAddress = XP_STRDUP(a_line);

	  a_line[0] = '\0';
	  if (AB_GetPOAddress(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pPOAddress = XP_STRDUP(a_line);

	  a_line[0] = '\0';
	  if (AB_GetZipCode(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pZipCode = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetCountry(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pCountry = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetWorkPhone(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pWorkPhone = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetFaxPhone(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pFaxPhone = XP_STRDUP(a_line);
	  
	  a_line[0] = '\0';
	  if (AB_GetHomePhone(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pHomePhone = XP_STRDUP(a_line);
#if 0
	  a_line[0] = '\0';
	  if (AB_GetDistName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pDistName = XP_STRDUP(a_line);
#endif
	  AB_GetSecurity(dir, aBook, entry, &m_personEntry.Security);
	  
	  a_line[0] = '\0';
	  if (AB_GetCoolAddress(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE)
		  m_personEntry.pCoolAddress = XP_STRDUP(a_line);
	  short use;
	  AB_GetUseServer(dir, aBook, entry, &use);
	  m_personEntry.UseServer = use;
	  a_line[0] = '\0';
	  if (AB_GetFullName(dir, aBook, entry, a_line) != MSG_VIEWINDEXNONE) {
		  char tmp[AB_MAX_STRLEN];
		  XP_SAFE_SPRINTF(tmp, sizeof(tmp),
						  XP_GetString(XFE_AB_NAME_CARD_FOR),
						  a_line);
		  setCardName(tmp);
	  }/* if AB_GetFullName */
  }/* if !m_newUser && m_entry != MSG_VIEWINDEXNONE */
  else {
	  m_personEntry.Initialize();
	  setCardName(XP_GetString(XFE_AB_NAME_NEW_CARD));
  }/* else */
  
  XFE_PropertySheetDialog::setDlgValues();
}

void XFE_ABNameFolderDlg::apply()
{
  getDlgValues();

#if defined(USE_ABCOM)

  MSG_Pane *pane = ((XFE_MNView *) m_view)->getPane();
  uint16 numItems = 1;
  AB_AttributeValue *values = 
	  (AB_AttributeValue *) XP_CALLOC(numItems, 
									  sizeof(AB_AttributeValue));
  values[0].attrib = AB_attribWinCSID;
  values[0].u.shortValue = m_context->fe.data->xfe_doc_csid;

  m_okToDestroy = TRUE;
  int error = AB_SetPersonEntryAttributes(pane, 
										  values, 
										  numItems);
  AB_FreeEntryAttributeValues(values, numItems);

  if (error != AB_SUCCESS)
	  m_okToDestroy = False;

  error = AB_CommitChanges(pane);
  if (error != AB_SUCCESS)
	  m_okToDestroy = False;

#else
  DIR_Server *dir = getABDir();
  ABook      *aBook = fe_GetABook(0);

  // assign win_csid
  m_personEntry.WinCSID = m_context->fe.data->xfe_doc_csid;
  int errorID;
  m_okToDestroy = TRUE;
  if (m_newUser) {
	  /* eEntryAlreadyExists ; MK_ADDR_ENTRY_ALREADY_EXISTS
	   */
	  int errorID = AB_AddUser(dir, aBook, &m_personEntry, &m_entry);
	  if (errorID != 0 ||
		  m_entry == MSG_VIEWINDEXNONE) {
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
		  m_okToDestroy = FALSE;
	  }/* if */
  }/* if */
  else {
	  errorID = AB_ModifyUser(dir, aBook, m_entry, &m_personEntry);
	  if (errorID != 0 ||
		  m_entry == MSG_VIEWINDEXNONE) {
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
		  m_okToDestroy = FALSE;
	  }/* if */
  }/* else */
#endif /* USE_ABCOM */
}

char* XFE_ABNameFolderDlg::getFullname()
{
	char a_line[AB_MAX_STRLEN];

	a_line[0] = '\0';

	static char *firstName = 0;
	static char *lastName = 0;
	static XP_Bool changed = False;

#if defined(USE_ABCOM)
	uint16       numItems = 3;
	AB_AttribID *attribs = (AB_AttribID *) XP_CALLOC(numItems, 
													  sizeof(AB_AttribID));
	attribs[0] = AB_attribGivenName;
	attribs[1] = AB_attribFamilyName;
	attribs[2] = AB_attribFullName;

	AB_AttributeValue *values = NULL;
	int error =
		AB_GetPersonEntryAttributes(((XFE_MNView *) m_view)->getPane(), 
									attribs,
									&values, 
									&numItems);
	char *tmp0 = values[0].u.string,
		 *tmp1 = values[1].u.string,
		 *tmp2 = values[2].u.string;

	if (!changed &&	(firstName && tmp0 && XP_STRCMP(firstName, tmp0)) ||
		(lastName && tmp1 && XP_STRCMP(lastName, tmp1)))
		changed = True;

	if (tmp2)
		XP_SAFE_SPRINTF(a_line, sizeof(a_line),
						"%s",
						tmp2);
	if ((m_entry == MSG_VIEWINDEXNONE) ||
		changed ||
		(tmp2 == NULL)) {
		if (tmp0 || tmp1) {
			XP_SAFE_SPRINTF(a_line, sizeof(a_line),
							"%s %s",
							tmp0?
							tmp0:"", 
							tmp1?
							tmp1:"");
		}/* if */
	}/* if FullName */
	firstName = tmp0;
	lastName = tmp1;

	// free
	XP_FREEIF(attribs);
	AB_FreeEntryAttributeValues(values, numItems);
#else
	DIR_Server *dir = getABDir();
	ABook      *aBook = fe_GetABook(0);


	if (!changed &&
		(firstName && XP_STRCMP(firstName, m_personEntry.pGivenName)) ||
		(lastName && XP_STRCMP(lastName, m_personEntry.pFamilyName)))
		changed = True;

	if ((m_entry == MSG_VIEWINDEXNONE) ||
		changed ||
		(AB_GetFullName(dir, aBook, m_entry, a_line) == MSG_VIEWINDEXNONE)) {
		if (m_personEntry.pGivenName || m_personEntry.pFamilyName) {
			XP_SAFE_SPRINTF(a_line, sizeof(a_line),
							"%s %s",
							m_personEntry.pGivenName?
							m_personEntry.pGivenName:"", 
							m_personEntry.pFamilyName?
							m_personEntry.pFamilyName:"");
		}/* if */
	}/* if AB_GetFullName */
	firstName = m_personEntry.pGivenName;
	lastName = m_personEntry.pFamilyName;
#endif /* USE_ABCOM */
	return XP_STRLEN(a_line)?XP_STRDUP(a_line):0;
}

void XFE_ABNameFolderDlg::setCardName(char *name)
{
	if (name)
		XtVaSetValues(XtParent(m_chrome), XmNtitle, name, NULL);
}

DIR_Server* XFE_ABNameFolderDlg::getABDir()
{
  XP_List    *directories = FE_GetDirServers();
  
  DIR_Server *pabDir = NULL;
  DIR_GetPersonalAddressBook(directories, &pabDir);
  return pabDir;
}/* XFE_ABNameFolderDlg::getABDir() */

extern "C" void fe_showABCardPropertyDlg(Widget parent,
										 MWContext *context,
										 ABID entry = MSG_VIEWINDEXNONE, 
										 XP_Bool newuser = True)
{
	XFE_ABNameFolderDlg* nameDlg = 
		new XFE_ABNameFolderDlg(parent,
								"abCardProperties", 
								True, 
								context);
	nameDlg->setDlgValues(entry, newuser);

	nameDlg->show();
}

extern "C" int FE_ShowPropertySheetFor(MWContext* context, 
									   ABID entryID, PersonEntry* pPerson)
{
	// need a context
	XP_ASSERT(context);

	// Get ABook
	ABook *addr_book = fe_GetABook(0);

	// Get ABDir
	DIR_Server *dir;
	DIR_GetPersonalAddressBook(FE_GetDirServers(), &dir);

	int result = -1;
	if (dir) {
		XFE_ABNameFolderDlg* nameDlg = 
			new XFE_ABNameFolderDlg(CONTEXT_WIDGET(context),
								    "abCardProperties", 
									True, 
									context);
		if (entryID != MSG_MESSAGEIDNONE) {
			nameDlg->setDlgValues(entryID, pPerson, False);
		}/* if */
		else {
			/* new user 
			 */
			nameDlg->setDlgValues(entryID, pPerson, True);
		}/* else */
	
		XFE_PropertySheetDialog::ANS_t ans = XFE_PropertySheetDialog::eWAITING;
		nameDlg->setClientData(&ans);
		nameDlg->show();

		while (ans == XFE_PropertySheetDialog::eWAITING)
			fe_EventLoop ();

#if defined(DEBUG_tao)
		printf("\n THE ANSWER is %d\n", ans);
#endif
		if (ans == XFE_PropertySheetDialog::eCANCEL)
			result = FALSE;
		else if (ans == XFE_PropertySheetDialog::eAPPLY)
			result = TRUE;
		else
			result = -1;
	}/* if dir */
	return result;
}

#if defined(USE_ABCOM)
extern "C" int 
fe_ShowPropertySheetForEntry(MSG_Pane *pane, MWContext *context)
{
	XFE_ABNameFolderDlg* nameDlg = 
		new XFE_ABNameFolderDlg(pane, context);
	nameDlg->show();
}
#endif /* USE_ABCOM */

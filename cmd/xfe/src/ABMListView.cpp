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
/* */
/*
 * ABMListView.cpp --
 * Superclass for all mail news views (folder, thread, and message.)
 * Created: Tao Cheng <tao@netscape.com>, Nov/27/1996.
 */

#include "ABMListView.h"

#include "AddrBookView.h"

// icons
fe_icon XFE_ABMListView::m_personIcon = { 0 };
fe_icon XFE_ABMListView::m_listIcon = { 0 };

#include "felocale.h"
#include "xpgetstr.h"

extern int XFE_ADDR_ADD_PERSON_TO_ABOOK;
extern int MK_ADDR_ADD_PERSON_TO_ABOOK;
extern int XFE_AB_NAME_CONTACT_TAB;
extern int XFE_AB_NAME_GENERAL_TAB;
extern int MK_ADDR_LAST_FIRST_SEP;
extern int XFE_DND_NO_EMAILADDRESS;

#define OUTLINER_GEOMETRY_PREF "addressbook.listpane.outliner_geometry"

#define MULTI_SELECT_ON   1

//
static void TableTraverse(
	Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam)

{
   XtPointer client;
   XFE_ABMListView *obj ;

   XtVaGetValues( w, XmNuserData, &client, 0);

   obj = (XFE_ABMListView *) client;
   obj->tableTraverse(w, event, params, nparam);
}

// Customized Traversal Actions

static XtActionsRec actions[] =
{ {"TableTraverse",     TableTraverse     },};

extern XtAppContext fe_XtAppContext;
//
XFE_ABMListView::XFE_ABMListView(XFE_Component *toplevel_component, 
				 Widget         parent,
				 DIR_Server    *dir, 
				 ABook         *pABook,
				 XFE_View      *parent_view,
				 MWContext     *context,
				 MSG_Master    *master): 
  XFE_MNView(toplevel_component, parent_view, context,
			 (MSG_Pane *)NULL),

  m_master(master),
  m_dir(dir),
  m_mListPane(NULL),
  m_AddrBook(pABook),
  m_listID(MSG_VIEWINDEXNONE),
  m_entryID(MSG_VIEWINDEXNONE),
  m_memberID(MSG_VIEWINDEXNONE),
  m_count(0),
  m_textF(0)
{
  /* initialize m_directroies
   */
  m_directories = FE_GetDirServers();

  m_preStr[0] = '\0';

  // Add actions
  XtAppAddActions( fe_XtAppContext, actions,  XtNumber(actions));

  /* For outliner
   */
  int num_columns = 2;
  static int column_widths[] = {6, 50};

  m_outliner = new XFE_Outliner("mailingList",
								this,
								toplevel_component,
								parent,
								False, // constantSize
								False,  //hasHeadings
								num_columns,
                                num_columns,
								column_widths,
								OUTLINER_GEOMETRY_PREF);
  /* BEGIN_3P: XmLGrid
   */
  XtVaSetValues(m_outliner->getBaseWidget(),
                XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
				XmNvisibleRows, 10,
				NULL);
  XtVaSetValues(m_outliner->getBaseWidget(),
				XmNcellDefaults, True,
                XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
				NULL);
  // editable
  XtVaSetValues(m_outliner->getBaseWidget(),
				XmNcellDefaults, True,
				XmNcolumn, 1,
                XmNcellEditable, True,
				NULL);
  /* END_3P: XmLGrid
   */

  // create drop site for internal drop
  fe_dnd_CreateDrop(m_outliner->getBaseWidget(),AddressDropCb,this);

  
  /* XFE_Outliner constructor does not allocate any content row
   * XFE_Outliner::change(int first, int length, int newnumrows)
   */
  m_outliner->show();
  setBaseWidget(m_outliner->getBaseWidget());
  // initialize the icons if they haven't already been
  {
    Pixel bg_pixel;
    
    XtVaGetValues(m_outliner->getBaseWidget(), XmNbackground, &bg_pixel, 0);
    if (!m_personIcon.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     /* umm. fix me
		      */
		     BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
		     bg_pixel,
		     &m_personIcon,
		     NULL, 
		     MN_Person.width, 
		     MN_Person.height,
		     MN_Person.mono_bits, 
		     MN_Person.color_bits, 
		     MN_Person.mask_bits, 
		     FALSE);

    if (!m_listIcon.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     /* umm. fix me
		      */
		     BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
		     bg_pixel,
		     &m_listIcon,
		     NULL, 
		     MN_People.width, 
		     MN_People.height,
		     MN_People.mono_bits, 
		     MN_People.color_bits, 
		     MN_People.mask_bits, 
		     FALSE);

  }
}

XFE_ABMListView::~XFE_ABMListView()
{
  /* close the pane
   */
	if (m_mListPane)
		AB_CloseMailingListPane(&m_mListPane);

}

ABID XFE_ABMListView::setValues(ABID listID)
{
  m_listID = listID;

  /* Initialize mailing list pane
   */
  m_mListPane = NULL;
  AB_InitMailingListPane(&m_mListPane,
			 &m_listID,
			 m_dir,
			 m_AddrBook,
			 m_contextData,
			 m_master,
			 ABFullName,
			 True);

  /* Tao_04dec96: use XFE_MNListView
   * connect pane and view
   * MSG_SetFEData((MSG_Pane *)m_abPane, this);
   * 
   */
  setPane((MSG_Pane *)m_mListPane);
  /* Get # of existing entry
   */
  AB_GetEntryCountInMailingList(m_mListPane, &m_count);

  m_outliner->change(0, m_count, m_count);
  return  m_listID;
}

/* Methods for the outlinable interface.
 */
char *XFE_ABMListView::getCellTipString(int row, int column)
{
	char *tmp = 0;
	static char tipstr[128];
	tipstr[0] = '\0';

	if (row < 0) {
		/* header
		 */
		tmp = getColumnHeaderText(column);
	}/* if */
	else {
		ABID orgID = m_entryID;

		/* content 
		 */
		m_entryID = AB_GetEntryIDAt((AddressPane *) m_mListPane, 
									(uint32) row);
		tmp = getColumnText(column);

		/* reset
		 */
		m_entryID = orgID;
	}/* else */
	if (tmp && 
		(!m_outliner->isColTextFit(tmp, row, column)))
		XP_STRCPY(tipstr,tmp);
	return tipstr;
}

char *XFE_ABMListView::getCellDocString(int /* row */, int /* column */)
{
	return NULL;
}

// Converts between an index and some non positional data type.
// Used to maintain selection when the list is reordered.
void *
XFE_ABMListView::ConvFromIndex(int /*index*/)
{
  return 0;
}

// Converts between an index and some non positional data type.
// Used to maintain selection when the list is reordered.
int
XFE_ABMListView::ConvToIndex(void */*item*/)
{
  return 0;
}

/* This method acquires one line of data: entryID is set for getColumnText
 */
void*
XFE_ABMListView::acquireLineData(int line)
{
  m_entryID = AB_GetEntryIDAt((AddressPane *) m_mListPane, (uint32) line);

  static char tmp[8];
  tmp[0] = '\0';
  return tmp;
}

/* The Outlinable interface.
 */

char*
XFE_ABMListView::getColumnName(int column)
{
  switch (column) {
  case XFE_AddrBookView::OUTLINER_COLUMN_TYPE:
	  return XP_GetString(XFE_AB_NAME_CONTACT_TAB);

  case XFE_AddrBookView::OUTLINER_COLUMN_NAME:
	  return XP_GetString(XFE_AB_NAME_GENERAL_TAB);
  }/* switch */

  return "unDefined";
}

/* Returns the text and/or icon to display at the top of the column.
 */
char*
XFE_ABMListView::getColumnHeaderText(int /* column */)
{
  return 0;
}

// Returns the text and/or icon to display at the top of the column.
fe_icon*
XFE_ABMListView::getColumnHeaderIcon(int /*column*/)
{
  return 0;
}

// Returns the text and/or icon to display at the top of the column.
EOutlinerTextStyle 
XFE_ABMListView::getColumnHeaderStyle(int /*column*/)
{
  return OUTLINER_Default;
}

/*
 * The following 4 requests deal with the currently acquired line.
 */
EOutlinerTextStyle 
XFE_ABMListView::getColumnStyle(int /*column*/)
{
  /* To be refined
   */
  return OUTLINER_Default;
}

char*
XFE_ABMListView::getColumnText(int column)
{
  static char a_line[AB_MAX_STRLEN];
  a_line[0] = '\0';
  switch (column) {
  case XFE_AddrBookView::OUTLINER_COLUMN_TYPE:
    break;

  case XFE_AddrBookView::OUTLINER_COLUMN_NAME:
    {
		char *expandedName = NULL;
		AB_GetExpandedName(m_dir, m_AddrBook, m_entryID, &expandedName);
		if (expandedName)
			XP_STRCPY(a_line, expandedName);
    }
    break;

  }/* switch () */
  return a_line;
}

fe_icon*
XFE_ABMListView::getColumnIcon(int column)
{
  fe_icon *myIcon = 0;
  switch (column) {
  case OUTLINER_COLUMN_TYPE:
    {
      ABID type;
      AB_GetType(m_dir, m_AddrBook, m_entryID, &type);
      if (type == ABTypePerson)
	myIcon = &m_personIcon; /* shall call make/initialize icons */
      else if (type == ABTypeList)
	myIcon = &m_listIcon;
    }
    break;
  }/* switch () */
  return myIcon;
}

//
void
XFE_ABMListView::getTreeInfo(XP_Bool */*expandable*/,
				   XP_Bool */*is_expanded*/, 
				   int *depth, 
				   OutlinerAncestorInfo **/*ancestor*/)
{
  depth = 0;
}
 
//
void 
XFE_ABMListView::Buttonfunc(const OutlineButtonFuncData *data)
{
  int row = data->row,
      clicks = data->clicks;

  if (row < 0) {
    return;
  } 
  else {
    /* content row 
     */
    ABID type;
    ABID entry;

    entry = AB_GetEntryIDAt((AddressPane *) m_mListPane, (uint32) row);
    
    if (entry == MSG_VIEWINDEXNONE) 
      return;
    
    AB_GetType(m_dir, m_AddrBook, entry, &type);
    
    if (clicks == 2) {
		m_outliner->selectItemExclusive(data->row);
    }/* clicks == 2 */
    else if (clicks == 1) {
#if MULTI_SELECT_ON
		if (data->shift) {
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
				 * nice with them. 
				 */
	  
				m_outliner->trimOrExpandSelection(data->row);
			}/* else */
		}/* if */
		else {
			m_outliner->selectItemExclusive(data->row);
		}/* else */
#else
		m_outliner->selectItemExclusive(data->row);
#endif
		getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
    }/* clicks == 1 */
  }/* else */

}/* XFE_ABMListView::Buttonfunc() */

void 
XFE_ABMListView::Flippyfunc(const OutlineFlippyFuncData */*data*/)
{
}

/* Tells the Outlinable object that the line data is no
 * longer needed, and it can free any storage associated with it.
 */ 
void
XFE_ABMListView::releaseLineData()
{
}

/* return the outliner associated with this outlinable.
 *
 */
XFE_Outliner*
XFE_ABMListView::getOutliner()
{
  return m_outliner;
}


/*
 * Callbacks for outside world
 */
void XFE_ABMListView::entryTTYActivateCallback(Widget w, 
					       XtPointer clientData, 
					       XtPointer callData)
{
  XFE_ABMListView *obj = (XFE_ABMListView *) clientData;
  obj->entryTTYActivateCB(w, callData);
}

void
XFE_ABMListView::entryTTYActivateCB(Widget w, XtPointer /* callData */)
{
  const char *str;
  str = fe_GetTextField(w);

  if (!str || XP_STRLEN(str) == 0)
	  return;

  /* if the name does not match/get expanded:
   * we break down every token
   */
  if (m_memberID == MSG_VIEWINDEXNONE) {
	  int len = XP_STRLEN(str);
	  char *pStr = (char *) XP_CALLOC(len+1, sizeof(char));

	  XP_SAFE_SPRINTF(pStr, len+1,
					  "%s\0",
					  str);

	  char *tok = 0,
		   *last = 0;
	  int   count = 0;
	  char *sep = XP_GetString(MK_ADDR_LAST_FIRST_SEP);

	  while (((tok=XP_STRTOK_R(count?nil:pStr, sep, &last)) != NULL)&&
			 XP_STRLEN(tok) &&
			 count < len) {

		  /* new one; no email address is attached
		   */
		  PersonEntry *person = new PersonEntry;
		  person->Initialize();
		  person->WinCSID = m_contextData->fe.data->xfe_doc_csid;
		  person->pGivenName = XP_STRDUP(tok);
		  person->pEmailAddress = XP_STRDUP(tok);

		  ABID entryID;
		  AB_AddPersonToMailingListAt(m_mListPane, 
									  person, 
									  m_count++, 
									  &entryID);
		  delete person;

		  count++;
	  }/* while */
  }/* if */
  else
    AB_AddIDToMailingListAt(m_mListPane, m_memberID, m_count++);

  //AB_GetEntryCountInMailingList(m_mListPane, &m_count);
  /* backend does not update list view
   */
  m_outliner->change(0, m_count, m_count);

  /* reset 
   */
  m_memberID = MSG_VIEWINDEXNONE;
  XP_FREE((void *)str);
  fe_SetTextFieldAndCallBack(w, "");
}

void XFE_ABMListView::entryTTYValChgCallback(Widget w, 
					       XtPointer clientData, 
					       XtPointer callData)
{
  XFE_ABMListView *obj = (XFE_ABMListView *) clientData;
  obj->entryTTYValChgCB(w, callData);
}

void
XFE_ABMListView::entryTTYValChgCB(Widget w, XtPointer /* callData */)
{

  m_textF = w;

  const char *str;
  str = fe_GetTextField(w);

  int curPos = XmTextFieldGetCursorPosition(w);

  if (m_preStr && XP_STRCMP(str, (const char*)m_preStr) == 0)
    return;

  ABID entryID;
  ABID field;
  XP_Bool setV = False;
#if 1
  DIR_Server* pab  = NULL;
  DIR_GetComposeNameCompletionAddressBook(m_directories, &pab);
  if (pab && 
	  AB_GetIDForNameCompletion(m_AddrBook, 
								pab,
								&entryID, 
								&field, 
								str) != MSG_VIEWINDEXNONE) {
#else
  if (AB_GetIDForNameCompletion(m_AddrBook, 
				 m_dir,
				 &entryID, 
				 &field, 
				 str) != MSG_VIEWINDEXNONE) {
#endif
	  char a_line[AB_MAX_STRLEN];
	  a_line[0] = '\0';

	  if (field == ABNickname) {
		  AB_GetNickname(m_dir, m_AddrBook, entryID, a_line);
	  }/* ABNickname */
	  else if (field == ABFullName) {
		  AB_GetFullName(m_dir, m_AddrBook, entryID, a_line);
	  }/* else if ABFullName */

	  int len = strlen(a_line);
	  if (len) {
		XP_STRCPY(m_preStr, a_line);

		/* shal be a valid id
		 */
		m_memberID = entryID;
		setV = True;
	  }/* if len */
	  else {
		  m_memberID = MSG_VIEWINDEXNONE;
		  XP_STRCPY(m_preStr, str);
	  }/* else */

  }/* if */
  else {
#if defined(DEBUG_tao)
	  printf("\n  entryTTYValChgCB:set m_memberID = MSG_VIEWINDEXNONE");
#endif
	  m_memberID = MSG_VIEWINDEXNONE;
	  strcpy(m_preStr, str);
  }/* else */
  if (setV) {
	  int lastPos = m_preStr?XP_STRLEN(m_preStr):0;

	  fe_SetTextFieldAndCallBack(w, m_preStr);
#if defined(DEBUG_tao)
	  printf("\n *** str=%s, curPos=%d, lastPos=%d, m_preStr=%s",
			 str?str:"", curPos, lastPos, m_preStr?m_preStr:"");
#endif
	  /* set selection
	   */
	  if (curPos > 0 && lastPos > curPos) {
		  Time time = XtLastTimestampProcessed(XtDisplay(w));

		  XmTextFieldSetSelection(w, curPos, lastPos, time);
		  /* set cursor position
		   */
	  }/* if */
	  XmTextFieldSetCursorPosition(w, curPos);
  }/* if */
}

void XFE_ABMListView::remove()
{
  /* check which is selected
   */
  uint32 num = 0; // = XmLGridGetSelectedRowCount(w);
  const int *indices = 0;
  m_outliner->getSelection(&indices, (int *) &num);
  if (num > 0 && indices) {
	  int curInd =  indices[0];
#if MULTI_SELECT_ON
	  /* indices is an internal buffer and
	   * may be modified in the process of deletion 
	   */
	  int *buf = (int *) XP_CALLOC(num, sizeof(int));
	  buf = (int *) memcpy(buf, indices, num*sizeof(int));

	  for (int i=num-1; i >=0; i--) {
		  AB_RemoveIDFromMailingListAt(m_mListPane, buf[i]);
	  }/* for i */
	  XP_FREEIF(buf);
#else
    AB_RemoveIDFromMailingListAt(m_mListPane, indices[0]);
#endif

	AB_GetEntryCountInMailingList(m_mListPane, &m_count);

	/* select next
	 */
	if (m_count) {
		if (curInd >= m_count)
			curInd = m_count-1;
		if(m_outliner && curInd >=0) {
			m_outliner->selectItemExclusive(curInd);
			m_outliner->makeVisible(curInd);
		}/* if */
	}/* if m_count */
  }/* if num > 0 && indices */
}/* remove() */

int XFE_ABMListView::apply(char *pNickName, char *pFullName, char *pInfo)
{
	// TODO: Add your specialized code here and/or call the base class
	if (m_textF)
		entryTTYActivateCB(m_textF, NULL);

	MailingListEntry  list;
	ABID              tempID = MSG_MESSAGEIDNONE;
	int               errorID = 0;
	MSG_ViewIndex     index = 0;
	uint32            count = 0;
	uint              i = 0;
  
	// UpdateData();
	list.Initialize();

    // assign win_csid
    list.WinCSID = m_contextData->fe.data->xfe_doc_csid;

	// get the values from the dialog box
	if (pNickName)
		list.pNickName = XP_STRDUP(pNickName);
	if (pFullName)
		list.pFullName = XP_STRDUP(pFullName);
	if (pInfo)
		list.pInfo = XP_STRDUP(pInfo);

	// get the count that is now in the list in the pane
	AB_GetEntryCountInMailingList(m_mListPane, &count);
	
	// run through all the entries to see if there are
	// going to be any errors
	do {
		if ((errorID =
			 AB_ModifyMailingListAndEntriesWithChecks(m_mListPane, &list,
													  &index, i)) != 0) {
			// if the error was that this entry isn't in the database 
			// then we are going to prompt them
			// do you want to add them 
			// defined in allxpstr.h
			if (MK_ADDR_ADD_PERSON_TO_ABOOK == errorID) {
				// bunch of formatting stuff for the dialog
				char * fullname = NULL;
				// set the selection to the one that's in error
				m_outliner->selectItemExclusive(index);
				m_outliner->makeVisible(index);
				tempID = AB_GetEntryIDAt((AddressPane*) m_mListPane,
											 index);
				AB_GetExpandedName (m_dir, m_AddrBook, tempID, &fullname);
				char* tmp = 
					PR_smprintf(XP_GetString(XFE_ADDR_ADD_PERSON_TO_ABOOK),
								fullname);
				if (tmp && fullname) {
					// if they dont respond yes then return because
					// i dont know what to do
					// Boolean fe_Confirm_2 (Widget parent, const char *message);
					if (!fe_Confirm_2(getBaseWidget(), tmp)) {
						XP_FREEIF (tmp);
						XP_FREEIF (fullname);
						return errorID;
					}/* if */
					else
						// if they did respond yes then reset the error
						// update the count of where we are in the list
						errorID = 0;

					i = index;
					// if we prompt them on the last one
					// then add it there might be a better way to write this
					// do loop
					if (i == count - 1) {
						errorID =
							AB_ModifyMailingListAndEntriesWithChecks(m_mListPane,
																	 &list,
																	 &index,
																	 MSG_VIEWINDEXNONE);
					}/* if */
				}/* if tmp && fullname */
				XP_FREEIF (tmp);
				XP_FREEIF (fullname);
			}/* if MK_ADDR_ADD_PERSON_TO_ABOOK */
			else {
				m_outliner->selectItemExclusive(index);
				m_outliner->makeVisible(index);
				break;
			}/* else */
		}/* if errorID != 0 */
		i++;
	} while (i < count && errorID == 0); 
	
	return errorID;
}

void 
XFE_ABMListView::tableTraverse(Widget /*w*/,
							   XEvent *  event,
							   String *  params,
							   Cardinal* nparam)

{
	XrmQuark q;
	static int quarksValid = 0;
	static XrmQuark qBACKSPACE;
	
	if (*nparam != 1)
		return;

    if (!quarksValid) {
		qBACKSPACE = XrmStringToQuark("BACKSPACE");
		quarksValid = 1;
    }/* if */

    q = XrmStringToQuark(params[0] );

	if ( q == qBACKSPACE )
		doBackSpace(event);
}

void
XFE_ABMListView::doBackSpace(XEvent *event)
{
  XmTextPosition cpos;
  XmTextPosition pos;

  XtVaGetValues(m_textF, XmNcursorPosition, &cpos, 0);
  pos = XmTextFieldGetLastPosition(m_textF);

  if (pos > 0 && m_textF) { 
	  // Hack text widget action
	  XtCallActionProc(m_textF, "delete-previous-character",
					   event, NULL, 0 );
  } /* pos > 0 */
}


// Address field drop site handler
void
XFE_ABMListView::AddressDropCb(Widget,void* cd,
							   fe_dnd_Event type,fe_dnd_Source *source,
							   XEvent*) {
    XFE_ABMListView *ad=(XFE_ABMListView*)cd;
    
    if (type==FE_DND_DROP && ad && source)
        ad->addressDropCb(source);
}

void
XFE_ABMListView::addressDropCb(fe_dnd_Source *source)
{
    switch (source->type) {

    case FE_DND_ADDRESSBOOK:
        {
        // add addressbook drop to address panel
        XFE_ABListSearchView* addrView=( XFE_ABListSearchView*)source->closure;
        if (addrView)
            processAddressBookDrop(addrView->getOutliner(),
                                   addrView->getDir(),
                                   addrView->getAddrBook(),
                                   (AddressPane*)addrView->getABPane());
        break;
        }
	case FE_DND_BOOKS_DIRECTORIES:
		{
#if defined(DEBUG_tao)
			printf("\nXFE_ABMListView::addressDropCb:FE_DND_BOOKS_DIRECTORIES\n");
#endif
		
			break;
		}
    default:
        break;
    }
}

void
XFE_ABMListView::processAddressBookDrop(XFE_Outliner *outliner,
                                        DIR_Server *abDir,
                                        ABook *abBook,
                                        AddressPane *abPane)
{
    if (!m_mListPane ||
        abBook != m_AddrBook ||
        abDir != m_dir)
        return;

    // add dropped entries to end of mailing list
    const int *selectedList=NULL;
    int numSelected=0;
    if (outliner->getSelection(&selectedList, &numSelected)) {
        int i;
		XP_Bool noEmails = False;
		for (i=0; i<numSelected; i++) {
            ABID entry = AB_GetEntryIDAt(abPane,(uint32)selectedList[i]);
    
            if (entry == MSG_VIEWINDEXNONE) 
                continue;

			/* check if email address exist
			 */
			char a_line[AB_MAX_STRLEN];
			a_line[0] = '\0';
			AB_GetEmailAddress(m_dir, m_AddrBook, entry, a_line);
			if (!a_line ||
				!XP_STRLEN(a_line)) {
				ABID type;
				AB_GetType(m_dir, m_AddrBook, entry, &type);
				if (type == ABTypePerson) {
					noEmails = True;
					continue;
				}/* if */
			}/* if */
			
            uint32 count=0;
            AB_GetEntryCountInMailingList(m_mListPane,&count);
            AB_AddIDToMailingListAt(m_mListPane,entry,count);
        }
		if (noEmails) {
		  // Prompt the user to enter the email address
		  char tmp[1024];
		  XP_SAFE_SPRINTF(tmp, sizeof(tmp),
						  "%s",
						  XP_GetString(XFE_DND_NO_EMAILADDRESS));
		  FE_Alert(m_contextData, tmp);
		}/* if */

    }
}

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
   AddressFolderView.cpp -- presents view for addressing and attachment .
   Created: Dora Hsu <dora@netscape.com>, 23-Sept-96.
   */


#include "rosetta.h"
#include "AddressFolderView.h"
#include "AddressOutliner.h"
#include "ABListSearchView.h"
#include "ComposeFolderView.h"
#include "ComposeAttachFolderView.h"
#include "LdapSearchView.h"
#include "xfe.h"
#include "icondata.h"
#include "msgcom.h"
#include "xp_mem.h"
#include "IconGroup.h"
#include "Button.h"

#include <Xm/Xm.h>
#include <XmL/Grid.h>
#include "ComposeView.h"
#include <xpgetstr.h> /* for XP_GetString() */

extern int XFE_MNC_ADDRESS_TO;
extern int XFE_MNC_ADDRESS_CC;
extern int XFE_MNC_ADDRESS_BCC;
extern int XFE_MNC_ADDRESS_NEWSGROUP;
extern int XFE_MNC_ADDRESS_REPLY;
extern int XFE_MNC_ADDRESS_FOLLOWUP;

#ifdef DEBUG_dora
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

#define OUTLINER_GEOMETRY_PREF "mail.composition.addresspane.outliner_geometry"

static MSG_HEADER_SET standard_header_set[] = {
    MSG_TO_HEADER_MASK,
    MSG_CC_HEADER_MASK,
    MSG_BCC_HEADER_MASK,
    MSG_REPLY_TO_HEADER_MASK,
    MSG_NEWSGROUPS_HEADER_MASK,
    MSG_FOLLOWUP_TO_HEADER_MASK
    };

#define TOTAL_HEADERS (sizeof(standard_header_set)/sizeof(MSG_HEADER_SET))

extern "C" char *fe_StringTrim (char *string);

extern "C" char* xfe_ExpandForNameCompletion(char *pString,
                                             ABook *pAddrBook,
                                             DIR_Server *pDirServer);
extern "C" char * xfe_ExpandName(char *pString, int* iconType, short* xxx,
                                 ABook *pAddrBook, DIR_Server *pDirServer);

extern "C"
{
XP_List* FE_GetDirServers();
ABook*   fe_GetABook(MWContext *context);
};


const char *XFE_AddressFolderView::tabPrev = "XFE_AddressFolderView::tabPrev";
const char *XFE_AddressFolderView::tabNext = "XFE_AddressFolderView::tabNext";

const char *XFE_AddressFolderView::TO = NULL;
const char *XFE_AddressFolderView::CC = NULL;
const char *XFE_AddressFolderView::BCC = NULL; 
const char *XFE_AddressFolderView::NEWSGROUP = NULL;
const char *XFE_AddressFolderView::REPLY = NULL;
const char *XFE_AddressFolderView::FOLLOWUP =  NULL;

const int XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_BTN = 0;
const int XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE = 1;
const int XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_ICON = 2;
const int XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT= 3;

#ifdef HAVE_ADDRESS_SECURITY_COLUMN
HG98269
#endif

const int XFE_AddressFolderView::ADDRESS_ICON_PERSON = 0;
const int XFE_AddressFolderView::ADDRESS_ICON_LIST = 1;
const int XFE_AddressFolderView::ADDRESS_ICON_NEWS = 2;
 

fe_icon XFE_AddressFolderView::arrowIcon = { 0 };
fe_icon XFE_AddressFolderView::personIcon = { 0 };
fe_icon XFE_AddressFolderView::listIcon = { 0 };
fe_icon XFE_AddressFolderView::newsIcon = { 0 };

HG91001


typedef enum {
  AddressFolder_Add,
  AddressFolder_Remove,
  AddressFolder_Select,
  AddressFolder_None
} EAddressFolderActionState;


extern "C" Widget
fe_MailComposeAddress_CreateManaged(MWContext* context, Widget parent);


extern XtAppContext fe_XtAppContext;

// Constructor
XFE_AddressFolderView::XFE_AddressFolderView(
			XFE_Component *toplevel_component, 
			XFE_View *parent_view,
			MSG_Pane *p,
			MWContext *context) 
  : XFE_MNListView(toplevel_component, parent_view, context, p)
{
  setParent(parent_view);

  // Initialize necessary data
  m_fieldData = NULL;
  m_fieldList = XP_ListNew();

  m_clearAddressee = True;

  // get the list of directory servers & address books
  XP_List *pDirectories=FE_GetDirServers();
  XP_ASSERT(pDirectories);
  
  DIR_GetComposeNameCompletionAddressBook(pDirectories, &m_pCompleteServer);

  m_pAddrBook = fe_GetABook(0);

  setupAddressHeadings();
}


Boolean
XFE_AddressFolderView::isCommandEnabled(CommandType, void*, XFE_CommandInfo*)
{
  return False;
}

Boolean
XFE_AddressFolderView::handlesCommand(CommandType command,
									  void*, XFE_CommandInfo*)
{
  if (command == xfeCmdGetNewMessages
      || command == xfeCmdAddNewsgroup
      || command == xfeCmdDelete
      HG92179
      || command == xfeCmdStopLoading)
    return True;

  return False;
}

void XFE_AddressFolderView::purgeAll()
{
	if (!m_fieldList)
		return;

	int count = (int) XP_ListCount(m_fieldList);
	if (count == 0)
		return;

	for (int i=count-1; i >=0; i--)
		removeDataAt(i);
}

ABAddrMsgCBProcStruc* XFE_AddressFolderView::exportAddressees()
{
	/* flush the buf
	 */
	updateHeaderInfo();

	if (!m_fieldList)
		return NULL;

    int count = (int) XP_ListCount(m_fieldList);
	if (count == 0)
		return NULL;

	ABAddrMsgCBProcStruc *package = 
		(ABAddrMsgCBProcStruc *) XP_CALLOC(1, sizeof(ABAddrMsgCBProcStruc));
	package->m_pairs = (StatusID_t **) XP_CALLOC(count, sizeof(StatusID_t*)); 

	XFE_AddressFieldData *fieldData = NULL;
	StatusID_t *pair = NULL;
	for (int i=0; i < count; i++) {
		fieldData = (XFE_AddressFieldData*) XP_ListGetObjectNum(m_fieldList, i+1);
		if (!fieldData ||
			!fieldData->receipient ||
			!XP_STRLEN(fieldData->receipient))
			continue;

		// calloc
		pair = (StatusID_t *) XP_CALLOC(1, sizeof(StatusID_t));

		// dplyStr
		pair->dplyStr = XP_STRDUP(fieldData->receipient);

		// status
		if (XP_STRCMP(fieldData->type, XFE_AddressFolderView::CC) == 0)
			pair->status = ::CC;
		else if (XP_STRCMP(fieldData->type, XFE_AddressFolderView::BCC) == 0)
			pair->status = ::BCC;
		else
			pair->status = ::TO;

		// type
		if (fieldData->icon == ADDRESS_ICON_PERSON)
			pair->type = ABTypePerson;
		else if (fieldData->icon == ADDRESS_ICON_LIST)
			pair->type = ABTypeList;
		else
			pair->type = ABTypePerson;

		package->m_pairs[package->m_count++] = pair;
	}/* for i */
	return package;
}

void XFE_AddressFolderView::addrMsgCallback(ABAddrMsgCBProcStruc* clientData, 
					    void*                 callData)
{
  XP_ASSERT(clientData && callData);
  XFE_AddressFolderView *obj = (XFE_AddressFolderView *) callData;
  obj->addrMsgCB(clientData);
}/* XFE_AddressFolderView::addrMsgCallback */

void XFE_AddressFolderView::addrMsgCB(ABAddrMsgCBProcStruc* clientData)
{

	if (m_clearAddressee)
		/* clean the list
		 */
		purgeAll();

  /* let me append these entries at the end for the moment
   */
  int npairs = clientData->m_count;
  int count = 0;
  char *firstR = 0;
  for (int i=0; i < npairs; i++) {
    count = (int) XP_ListCount(m_fieldList);
    StatusID_t *addressee = clientData->m_pairs[i];

    /* m_fieldData is the newly created entry
     */
    if (count == 0 || !m_fieldData) {
      /* brand new
       * Add new record
       */ 
      m_fieldData = new XFE_AddressFieldData;
      initializeData(m_fieldData);
	  m_fieldData->type = MSG_HeaderMaskToString(MSG_TO_HEADER_MASK);
	  m_fieldData->receipient = NULL;
      XP_ListAddObjectToEnd(m_fieldList, m_fieldData);
	  count++;
    }/* else */

    /* type: to, cc, bcc
     */
    switch(addressee->status) {
    case ::TO:
      m_fieldData->type = MSG_HeaderMaskToString(MSG_TO_HEADER_MASK);
      break;

    case ::CC:
      m_fieldData->type = MSG_HeaderMaskToString(MSG_CC_HEADER_MASK);
      break;

    case ::BCC:
      m_fieldData->type = MSG_HeaderMaskToString(MSG_BCC_HEADER_MASK);
      break;
    }/*switch */

    /* receipient
     */
    m_fieldData->receipient = XP_STRDUP(addressee->dplyStr?
										addressee->dplyStr:
										"");
	if (!firstR)
		firstR = m_fieldData->receipient;
    /* type: person, list
     */
    switch(addressee->type) {
    case ABTypePerson:
      m_fieldData->icon = ADDRESS_ICON_PERSON;
      break;

    case ABTypeList:
      m_fieldData->icon = ADDRESS_ICON_LIST;
      break;
    }/* switch */

    /* add new record
     */
    if (count)
		insertNewDataAfter(count-1);
    else {
		printf("\n HOW DO I GET INTO THIS??\n");
		/* brand new
		 * Add new record
		 */ 
		m_fieldData = new XFE_AddressFieldData;
		initializeData(m_fieldData);
		XP_ListAddObjectToEnd(m_fieldList, m_fieldData);
    }/* else */

    XP_FREE((StatusID_t *)addressee);
  }/* for i */
  XP_FREE((StatusID_t **)clientData->m_pairs);
  XP_FREE((ABAddrMsgCBProcStruc *)clientData);
  
  count = (int) XP_ListCount(m_fieldList);
  m_outliner->change(0, count, count);
	/* move focus to last row
	 * void 
	 * XFE_AddressOutliner::PlaceText(int row, int col, 
	 *                                Boolean doUpdate)
	 */
	((XFE_AddressOutliner *) m_outliner)->fillText(firstR);
	((XFE_AddressOutliner *) m_outliner)->
		PlaceText(count-1, ADDRESS_OUTLINER_COLUMN_RECEIPIENT, 
				  False);
}/* XFE_AddressFolderView::addrMsgCB */

XFE_CALLBACK_DEFN(XFE_AddressFolderView, openAddrBk)(XFE_NotificationCenter */*obj*/,
                                          void */*clientData*/,
                                          void */*callData*/)
{

      fe_showAddrMSGDlg(getToplevel()->getBaseWidget(), 
						XFE_AddressFolderView::addrMsgCallback, this, 
						m_contextData);
      return;
}

void
XFE_AddressFolderView::doCommand(CommandType command, void *, XFE_CommandInfo*)
{
	if (command == xfeCmdAddresseePicker)
		fe_showAddrMSGDlg(getToplevel()->getBaseWidget(), 
						  XFE_AddressFolderView::addrMsgCallback, this, 
						  m_contextData);

}

// Outlinable Interface starts here 

void*
XFE_AddressFolderView::ConvFromIndex(int)
{
  return NULL;
}

int
XFE_AddressFolderView::ConvToIndex(void*)
{
  return 0;
}

char *
XFE_AddressFolderView::getColumnName(int column)
{
  switch (column)
    {
    case ADDRESS_OUTLINER_COLUMN_BTN:
      return "Btn";
    case ADDRESS_OUTLINER_COLUMN_TYPE:
      return "Type";
    case ADDRESS_OUTLINER_COLUMN_ICON:
      return "Icon";
#ifdef HAVE_ADDRESS_SECURITY_COLUMN
    HG92769
#endif
    case ADDRESS_OUTLINER_COLUMN_RECEIPIENT:
      return "Receipient";
    default:
      XP_ASSERT(0);
      return 0;
    }
}

char *
XFE_AddressFolderView::getColumnHeaderText(int column)
{
  switch (column)
    {
    case ADDRESS_OUTLINER_COLUMN_BTN:
      return "";
    case ADDRESS_OUTLINER_COLUMN_TYPE:
      return "Type";
    case ADDRESS_OUTLINER_COLUMN_ICON:
      return "";
#ifdef HAVE_ADDRESS_SECURITY_COLUMN
    HG98268
#endif
    case ADDRESS_OUTLINER_COLUMN_RECEIPIENT:
      return "Receipient";
    default:
      XP_ASSERT(0);
      return 0;
    }
}

fe_icon*
XFE_AddressFolderView::getColumnHeaderIcon(int /*column*/)
{
  return NULL;
}

EOutlinerTextStyle 
XFE_AddressFolderView::getColumnHeaderStyle(int /*column*/)
{
   return OUTLINER_Bold;
}



// line starts with 0 to mean the first row
Boolean
XFE_AddressFolderView::removeDataAt(int line)
{

  XFE_AddressFieldData *data;
  Boolean removed = False;
  int count = (int) XP_ListCount(m_fieldList);

  if ( count == 0 ) 
	  return False;

  if (line > (count-2))
	  return False;

  data = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
  if (!data)
	  return False;

  if ( count  == 1 ) 
  {

     delete data->receipient;
     initializeData(data);
     m_fieldData = data;
  }
  else
  {
    removed = (Boolean)XP_ListRemoveObject(m_fieldList, data);
    delete data->receipient;
    delete data;
    data = NULL;

    if ( line + 1  <= (count-1) )
      m_fieldData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
    else  if ( line > 0 )
      m_fieldData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line);
    else 
      XP_ASSERT(0);
  }
  return removed;

}

// line starts with 0 to mean the first row
void
XFE_AddressFolderView::getDataAt(int line, int column, char **textstr)
{
  XFE_AddressFieldData *data;
  
  *textstr = NULL;
  if ( XP_ListCount(m_fieldList) == 0 ) *textstr = NULL;
  else
  {
     data = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
     if ( !data) return;
     switch(column)
     {
	case ADDRESS_OUTLINER_COLUMN_BTN:
	break;
	case ADDRESS_OUTLINER_COLUMN_TYPE:
          if (data->type )
	  {
	    *textstr = new char[strlen(data->type)+1];
	    strcpy(*textstr, data->type);
          }

	break;
	case ADDRESS_OUTLINER_COLUMN_RECEIPIENT:
          if ( data->receipient )
          {
	    *textstr = new char[strlen(data->receipient)+1];
	    strcpy(*textstr, data->receipient);
	  }
	break;
        default:
	break;
  }
  }
}


Boolean
XFE_AddressFolderView::hasDataAt(int line)
{
   Boolean hasData = True;
   XFE_AddressFieldData *tmp_fieldData;

   tmp_fieldData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
   
   if (!tmp_fieldData ) hasData = False;
   
   return hasData;
}

void
XFE_AddressFolderView::insertNewDataAfter(int line)
{
    XFE_AddressFieldData *tmp_fieldData;

    tmp_fieldData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);

    XP_ASSERT(tmp_fieldData);

    m_fieldData = new XFE_AddressFieldData;
    initializeData(m_fieldData);

    XP_ASSERT(tmp_fieldData->type);
    m_fieldData->type = tmp_fieldData->type;

    XP_ListInsertObjectAfter( m_fieldList, tmp_fieldData, m_fieldData);
}

int
XFE_AddressFolderView::getHeader(int line)
{
  XFE_AddressFieldData *m_tmpData;
  MSG_HEADER_SET mask;
  int i;
  
  if ( XP_ListCount(m_fieldList) == 0 ) XP_ASSERT(0);

  m_tmpData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);

  if (!m_tmpData) XP_ASSERT(0);

  mask = MSG_HeaderStringToMask(m_tmpData->type);

  for ( i = 0; i < (int)TOTAL_HEADERS; i++ )
    if ( standard_header_set[i] == mask ) break;

  return (int)i;
}

void 
XFE_AddressFolderView::setHeader(int line, MSG_HEADER_SET header)
{
  Boolean brandnew = False;

  if ( XP_ListCount(m_fieldList) == 0 )
  {
	// Add new record
    m_fieldData = new XFE_AddressFieldData;
    initializeData(m_fieldData);
    brandnew = True;
  }
  else 
  {
     m_fieldData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
     if ( !m_fieldData ) return;
  }
  
  m_fieldData->type = MSG_HeaderMaskToString(header);

  if ( header == MSG_NEWSGROUPS_HEADER_MASK )
   {
      m_fieldData->icon = XFE_AddressFolderView::ADDRESS_ICON_NEWS;
   }
   else if ( m_fieldData->icon != XFE_AddressFolderView::ADDRESS_ICON_LIST)
   {
       m_fieldData->icon = XFE_AddressFolderView::ADDRESS_ICON_PERSON;
   }

  if ( brandnew ){
     XP_ListAddObjectToEnd(m_fieldList, m_fieldData);
  }
}

const char *
XFE_AddressFolderView::setData(int line, char *recipient)
{
  Boolean brandnew = False;
  char *expandedStr = NULL;
  int iconType = 0;
  

  if ( XP_ListCount(m_fieldList) == 0 )
  {
	// Add new record
    m_fieldData = new XFE_AddressFieldData;
    initializeData(m_fieldData);
    brandnew = True;
  }
  else 
  {
     m_fieldData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
     if ( !m_fieldData ) return 0;
  }
  if ( m_fieldData->receipient )
    delete [] m_fieldData->receipient;

  m_fieldData->receipient = NULL;

  if ( recipient && strlen(recipient))
  {
     HG82688

     if ( strcmp(m_fieldData->type, XFE_AddressFolderView::NEWSGROUP) ) // If it's newsgroup, then, don't change it's icon
        m_fieldData->icon = iconType;

     if ( expandedStr )
     {
       m_fieldData->receipient = new char[strlen(expandedStr)+1];
       strcpy(m_fieldData->receipient, expandedStr);
     }
     else
     {
       m_fieldData->receipient = new char[strlen(recipient)+1];
       strcpy(m_fieldData->receipient, recipient);
     }

     // Created by XP_ALLOC
     XP_FREE(expandedStr);
  }

  // Reminder: Don't need to free the recipient here because
  // Caller to setRecipient is in charge of freeing since
  // some time recipient might be dynamically set.
  
  // May need to break up to add several uid rows
  if ( brandnew ){
     XP_ListAddObjectToEnd(m_fieldList, m_fieldData);
  }
  return m_fieldData->type;
}


// We no longer free the textstr here
int
XFE_AddressFolderView::setReceipient(int line, char *textstr)
{
  // Special case: if it's a null string, don't do any matching
  if (textstr == 0 || ( XP_STRLEN(textstr) == 0))
  {
      setData(line, textstr);
      return 1;
  }

  // Try completion first; MSG_ParseRFC822Addresses() may get the count
  // wrong, especially when completing firstname_lastname w/spaces.
#if defined(DEBUG_tao)
  printf("\nXFE_AddressFolderView::setReceipient,%d=%s\n", line, textstr);
#endif
  char* completestr = xfe_ExpandForNameCompletion(textstr, m_pAddrBook,
                                                  m_pCompleteServer);
  if (completestr)
  {
      setData(line, completestr);
      XP_FREE(completestr);
      return 1;
  }

  // RFC822 parsing:
  char* pNames = NULL;
  char* pAddresses = NULL;
  int count = MSG_ParseRFC822Addresses(textstr, &pNames, &pAddresses);
  if (count > 0)
  {
      const char *msgtype;
      int newline = line;
      char* pn = pNames;
      char* pa = pAddresses;

      for (int i=0; i<count; ++i)
      {
          MSG_HEADER_SET header;
          char* fullname = MSG_MakeFullAddress(pn, pa);
          XP_ASSERT(fullname);
          while (*pn != '\0')
              ++pn;
          ++pn;
          while (*pa != '\0')
              ++pa;
          ++pa;
          if (i > 0)
          {
              insertNewDataAfter(newline++);
              (void)setHeader(newline, header);
          }
          msgtype = setData(newline, fullname);
          if (i == 0)
              header = MSG_HeaderStringToMask(msgtype);
          XP_FREE(fullname);
      }

      if (pNames)
          XP_FREE(pNames);
      if (pAddresses)
          XP_FREE(pAddresses);
  }

  return count;
}


void
XFE_AddressFolderView::updateReceipient(int line)
{
  XFE_AddressFieldData *pAddress = 0;
  MSG_HEADER_SET msgtype = 0;
  char *newvalue = 0;

  if ( XP_ListCount(m_fieldList) == 0 )	return;

  pAddress = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);

  if ( !pAddress ) return;

  if ( pAddress->receipient)
     msgtype = MSG_HeaderStringToMask(pAddress->type);

   newvalue = MSG_UpdateHeaderContents(getPane(), 
			msgtype, pAddress->receipient);
  
  if (newvalue) {
    delete pAddress->receipient;
    pAddress->receipient = new char[strlen(newvalue)+1];
    strcpy(pAddress->receipient, newvalue);
    MSG_SetCompHeader(getPane(), msgtype, newvalue);
    XP_FREE(newvalue);
  }
}


void
XFE_AddressFolderView:: getTreeInfo(XP_Bool * /*expandable*/, 
			XP_Bool */*is_expanded*/, int */*depth*/,
                           OutlinerAncestorInfo **/*ancestor*/)
{
// Do nothing, there is no tree here
}

void *
XFE_AddressFolderView::acquireLineData(int line) 
{
  int count;

  count = XP_ListCount(m_fieldList);

  if ( count == 0 && line == 0 )
  {
    // New Table, create a TO field by default
    m_fieldData = new XFE_AddressFieldData;
    initializeData(m_fieldData);
    m_fieldData->type = MSG_HeaderMaskToString(MSG_TO_HEADER_MASK);
    m_fieldData->receipient = NULL;
    XP_ListAddObjectToEnd(m_fieldList, m_fieldData);
  }
  else if ( count-1 >= line )
  {
     m_fieldData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
  } else if ( line > count-1 )
  {
     XFE_AddressFieldData *tmp_fieldData;
     tmp_fieldData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, count);

     m_fieldData = new XFE_AddressFieldData;
    initializeData(m_fieldData);
     XP_ASSERT(tmp_fieldData->type);
     // copy previous header to empty new line:
     if (!XP_STRCMP(tmp_fieldData->type, BCC)
         || !XP_STRCMP(tmp_fieldData->type, REPLY))
         m_fieldData->type = MSG_HeaderMaskToString(MSG_TO_HEADER_MASK);
     else
         m_fieldData->type = tmp_fieldData->type;
     XP_ListAddObjectToEnd(m_fieldList, m_fieldData);

  }
  return m_fieldData;
}


EOutlinerTextStyle 
XFE_AddressFolderView::getColumnStyle(int /*column*/)
{
  return OUTLINER_Default;
}

// Caller has to free the return value
char *
XFE_AddressFolderView::getColumnText(int column)
{
   char* data = NULL;

    if (!m_fieldData) return NULL;
    switch (column) 
    {
	case ADDRESS_OUTLINER_COLUMN_BTN:
	case ADDRESS_OUTLINER_COLUMN_ICON:
	break;
	case ADDRESS_OUTLINER_COLUMN_TYPE:
	   data = new char[strlen(m_fieldData->type)+1];
	   strcpy(data, m_fieldData->type);
	break;
	case ADDRESS_OUTLINER_COLUMN_RECEIPIENT:
           if ( m_fieldData->receipient )
	   {
	   data = new char[strlen(m_fieldData->receipient)+1];
	   strcpy(data, m_fieldData->receipient);
           }
	break;
	default:
	break;
    }
    return data;
}

fe_icon *
XFE_AddressFolderView::getColumnIcon(int column)
{

  switch(column)
  {
	case ADDRESS_OUTLINER_COLUMN_BTN:
	  return &arrowIcon;
	case ADDRESS_OUTLINER_COLUMN_ICON:
	{
           if ( m_fieldData->icon == ADDRESS_ICON_PERSON )
	      return &personIcon;
	   else if ( m_fieldData->icon == ADDRESS_ICON_LIST)
	      return &listIcon;
           else if ( m_fieldData->icon == ADDRESS_ICON_NEWS)
	      return &newsIcon;
	   else if ( m_fieldData->icon == 8 )
	      return &newsIcon;
	}
	break;
#ifdef HAVE_ADDRESS_SECURITY_COLUMN
	HG32179
#endif
  }
  return 0;
}

void
XFE_AddressFolderView::releaseLineData()
{
// Not needed to free anything here.
// We only make a reference to an element in the array. We don't want to
// set anything free.

}

void
XFE_AddressFolderView::Buttonfunc(const OutlineButtonFuncData */*data*/)
{
}

void
XFE_AddressFolderView::Flippyfunc(const OutlineFlippyFuncData * /*data*/)
{
}

void 
XFE_AddressFolderView::setupAddressHeadings()
{
XFE_AddressFolderView::TO = XP_GetString(XFE_MNC_ADDRESS_TO);
XFE_AddressFolderView::CC = XP_GetString(XFE_MNC_ADDRESS_CC);
XFE_AddressFolderView::BCC = XP_GetString(XFE_MNC_ADDRESS_BCC); 
XFE_AddressFolderView::NEWSGROUP = XP_GetString(XFE_MNC_ADDRESS_NEWSGROUP);
XFE_AddressFolderView::REPLY = XP_GetString(XFE_MNC_ADDRESS_REPLY);
XFE_AddressFolderView::FOLLOWUP =  XP_GetString(XFE_MNC_ADDRESS_FOLLOWUP);
}

void
XFE_AddressFolderView::setupIcons()
{
  Widget base = getToplevel()->getBaseWidget();
  // Initialize Icons if they have not been
  if (!personIcon.pixmap )
  {
	Pixel bg_pixel;

	XtVaGetValues(base,  XmNbackground, &bg_pixel, 0);

	// AddressBook
	IconGroup_createAllIcons(&MNC_Address_group,
                            getToplevel()->getBaseWidget(),
                            BlackPixelOfScreen(XtScreen(base)), // hack. :(
                            bg_pixel);
 
	// Arrow
        fe_NewMakeIcon(getToplevel()->getBaseWidget(),
                     BlackPixelOfScreen(XtScreen(base)), // umm. fix me
                     bg_pixel,
                     &arrowIcon,
                     NULL,
                     HFolderO.width, HFolderO.height,
                     HFolderO.mono_bits, HFolderO.color_bits, HFolderO.mask_bits, FALSE);


	// Person 
        fe_NewMakeIcon(getToplevel()->getBaseWidget(),
                     BlackPixelOfScreen(XtScreen(base)), // umm. fix me
                     bg_pixel,
                     &personIcon,
                     NULL,
                     MN_Person.width, MN_Person.height,
                     MN_Person.mono_bits, MN_Person.color_bits, 
		     MN_Person.mask_bits, FALSE);

	// List 
        fe_NewMakeIcon(getToplevel()->getBaseWidget(),
                     BlackPixelOfScreen(XtScreen(base)), // umm. fix me
                     bg_pixel,
                     &listIcon,
                     NULL,
                     MN_People.width, MN_People.height,
                     MN_People.mono_bits, MN_People.color_bits, 
		     MN_People.mask_bits, FALSE);



	// News 
        fe_NewMakeIcon(getToplevel()->getBaseWidget(),
                     BlackPixelOfScreen(XtScreen(base)), // umm. fix me
                     bg_pixel,
                     &newsIcon,
                     NULL,
                     MN_Newsgroup.width, MN_Newsgroup.height,
                     MN_Newsgroup.mono_bits, MN_Newsgroup.color_bits, 
		     MN_Newsgroup.mask_bits, FALSE);


	HG82687
  }
}

void
XFE_AddressFolderView::createWidgets(Widget parent_widget)
{
  Widget formW;
  //static const char *addressBook = "addressBook";

#ifdef HAVE_ADDRESS_SECURITY_COLUMN
  static int column_widths[] = {4,15,4,4,25}; // outliner column size
  int dbTableNumColumns = 5;
#else
  static int column_widths[] = {0,15,0,25}; // outliner column size
  int dbTableNumColumns = 4;
#endif

  Widget gridW;

  formW = XtVaCreateManagedWidget("addressFolderBaseWidget",
		xmFormWidgetClass, parent_widget, 0);
  setBaseWidget(formW);

  setupIcons();

  getParent()->getToplevel()->registerInterest(
	XFE_ComposeView::tabNext, this,
        (XFE_FunctionNotification)tabToButton_cb);
   
  getParent()->getToplevel()->registerInterest(
	XFE_AddressOutliner::tabPrev, this,
        (XFE_FunctionNotification)tabToButton_cb);

   // Initialize the necessary info on the AddressFolderView
   initialize();

   // Create Table

	m_outliner = new XFE_AddressOutliner( "grid",
		this,
		this,
		formW,
		False,
		dbTableNumColumns,
		column_widths,
		OUTLINER_GEOMETRY_PREF);

	// Arrow Button
	m_outliner->setColumnResizable(ADDRESS_OUTLINER_COLUMN_BTN, False);
	m_outliner->setColumnWidth(ADDRESS_OUTLINER_COLUMN_BTN, 
							   HFolderO.width + 2 /* for the outliner's shadow */);

	// Followup To:  (etc..)
	m_outliner->setColumnResizable(ADDRESS_OUTLINER_COLUMN_TYPE, False);

	// AB Person icon
	m_outliner->setColumnResizable(ADDRESS_OUTLINER_COLUMN_ICON, False);
	m_outliner->setColumnWidth(ADDRESS_OUTLINER_COLUMN_ICON, 
							   MN_Person.width + 2 /* for the outliner's shadow */);

#ifdef HAVE_ADDRESS_SECURITY_COLUMN
	// Security thing, didn't make 4.0
	HG78277
#endif

	gridW= ( (XFE_AddressOutliner*)m_outliner)->getBaseWidget();
	XtVaSetValues( gridW,
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM,
		XmNtraversalOn, True,
		XmNhighlightOnEnter, True,
		0);
	((XFE_AddressOutliner*)m_outliner)->show();

        // create drop site for internal drop
        fe_dnd_CreateDrop(m_outliner->getBaseWidget(),AddressDropCb,this);

}

void
XFE_AddressFolderView::btnActivateCallback(Widget w, XtPointer clientData, XtPointer callData)
{
  XFE_AddressFolderView *obj = (XFE_AddressFolderView*)clientData;
 
  obj->btnActivate(w, callData);
}

void
XFE_AddressFolderView::btnActivate(Widget w, XtPointer)
{
  EAddressFolderActionState  state;

  XtVaGetValues(w, XmNuserData, &state, 0);

  switch (state)
  {
	case AddressFolder_Add:
	  XDEBUG(printf("add in XFE_AddressFolderView::btnActivate\n");)
	  break;
	case AddressFolder_Remove:
	  XDEBUG(printf("remove in XFE_AddressFolderView::btnActivate\n");)
	  break;
	case AddressFolder_Select:
	  XDEBUG(printf("select in XFE_AddressFolderView::btnActivate\n");)
	  break;
	default:
	  XDEBUG(printf("not handled in XFE_AddressFolderView::btnActivate\n");)
	  break;
  }
}

XFE_CALLBACK_DEFN(XFE_AddressFolderView,tabToButton)(XFE_NotificationCenter*, void*, void*)
{
  XmProcessTraversal(m_addBtnW, XmTRAVERSE_CURRENT);

}

//------------ NEW code -------------------------

// Inserting initial addressing field value
void
XFE_AddressFolderView::initialize()
{
   MSG_Pane *pane = getPane(); // Get it from MNView

   MSG_HeaderEntry * entry;
   int nCount;

   nCount = MSG_RetrieveStandardHeaders(pane,&entry);

   if (nCount > 0)
   {
      int i;
      for (i=0; i<nCount; i++)
      {
          XFE_AddressFieldData *data = new XFE_AddressFieldData;
          initializeData(data);

          data->type = MSG_HeaderMaskToString(entry[i].header_type);
	  if ( entry[i].header_value )
	  data->receipient = (char*)XP_CALLOC(strlen(entry[i].header_value)+1,
				sizeof(char));
	  XP_STRCPY(data->receipient, entry[i].header_value);
          data->icon = ADDRESS_ICON_PERSON;

	  XP_ListAddObjectToEnd(m_fieldList, data);
          XP_FREE(entry[i].header_value);
      }

      if (entry) // Need to check for free
                XP_FREE(entry);
   }
}

int
XFE_AddressFolderView::getTotalData()
{
   int count = 0;
  
   if ( m_fieldList )
   	count = XP_ListCount(m_fieldList);
   return count;
}


void XFE_AddressFolderView::updateHeaderInfo ( void )
{
    // Make sure the address outliner has gotten the last address typed in:
    ((XFE_AddressOutliner*)m_outliner)->updateAddresses();

    MSG_Pane *pComposePane = getPane();
    int i, j;

    if (m_fieldList)
    {
        MSG_HeaderEntry * entry = NULL;
        XFE_AddressFieldData *address;

        MSG_ClearComposeHeaders(pComposePane);

        int count = XP_ListCount(m_fieldList);

        if (count)
            entry = (MSG_HeaderEntry*)XP_ALLOC(sizeof(MSG_HeaderEntry)*count);

        if (entry == NULL)
            return;

	i = 0; 
	j = i;
	while ( j < count )
        {
            if( (address = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, j+1) ))
            {
		entry[i].header_value = NULL;
		if ( address->receipient )
                  entry[i].header_value = XP_STRDUP(address->receipient);
                if (!strcmp(address->type,TO))
                    entry[i].header_type = MSG_TO_HEADER_MASK;
                else if(!strcmp(address->type,REPLY))
                    entry[i].header_type = MSG_REPLY_TO_HEADER_MASK;
                else if(!strcmp(address->type,CC))
                    entry[i].header_type = MSG_CC_HEADER_MASK;
                else if(!strcmp(address->type,BCC))
                    entry[i].header_type = MSG_BCC_HEADER_MASK;
                else if(!strcmp(address->type,NEWSGROUP))
                    entry[i].header_type = MSG_NEWSGROUPS_HEADER_MASK;
                else if(!strcmp(address->type,FOLLOWUP))
                    entry[i].header_type = MSG_FOLLOWUP_TO_HEADER_MASK;
		if ( entry[i].header_value ) i++;
            }
	    j++;
        }

        MSG_HeaderEntry * list;
        count = MSG_CompressHeaderEntries(entry,i,&list);

        if ((count != -1)&&(list!=NULL))
        {
            for (i=0; i<count; i++)
            {
                XP_ASSERT(list[i].header_value);
                MSG_SetCompHeader(pComposePane,list[i].header_type,list[i].header_value);
                XP_FREE(list[i].header_value);
            }
	    XP_FREE(list);
        }
        for (i=0; i<count; i++)
        {
		XP_FREE(entry[i].header_value);
	}
	XP_FREE(entry);
    }

}

// This method should be called from Activate Callback
char *
XFE_AddressFolderView::changedItem(char *pString, int*  iconType,
                                   short* xxx)
{
#if defined(DEBUG_tao)
	printf("\nXFE_AddressFolderView::changedItem,%s\n",pString?pString:"");
#endif 
   return  xfe_ExpandName(pString, iconType, xxx,
                          m_pAddrBook, m_pCompleteServer);
}


//----------- Address Book stuff here ------------

extern "C" char * xfe_ExpandName(char * pString, int* iconID, short* xxx,
                                 ABook *pAddrBook, DIR_Server *pDirServer)
{
#if !defined(MOZ_NEWADDR)
        ABID entryID;
        ABID field;
        char * fullname;
        ABID type;

#if defined(DEBUG_tao)
		printf("\n  xfe_ExpandName");
#endif

        XP_ASSERT(pAddrBook);
        XP_ASSERT(pDirServer);
		AB_GetIDForNameCompletion(pAddrBook,
								  pDirServer,
								  &entryID,
								  &field,
								  pString);
        if (entryID != (unsigned long)-1)
        {
	   AB_GetType(pDirServer, pAddrBook, entryID, &type);

	   if ( type == ABTypePerson )
	   {
	      *iconID = XFE_AddressFolderView::ADDRESS_ICON_PERSON;
	   }
	   else if ( type == ABTypeList)
	   {
	       *iconID = XFE_AddressFolderView::ADDRESS_ICON_LIST;
	   }
		
       HG91268

          AB_GetExpandedName(
                        pDirServer,
                        pAddrBook, entryID, &fullname);

          if (fullname) return fullname;  
       }
#endif
       return NULL; 
}



//------------ Begin of MSG code -------------------------
int MSG_TotalHeaderCount()
{
	return TOTAL_HEADERS;
}

MSG_HEADER_SET 
MSG_HeaderValue( int pos )
{
  if( pos >= (int) TOTAL_HEADERS) XP_ASSERT(0);

  return standard_header_set[pos];
}

MSG_HEADER_SET 
MSG_HeaderStringToMask(const char * strheader)
{
   MSG_HEADER_SET header = 0;
   
   if ( !strcmp(strheader, XFE_AddressFolderView::TO) ) 
		header = MSG_TO_HEADER_MASK; 
   else if ( !strcmp(strheader, XFE_AddressFolderView::CC) ) 
		header = MSG_CC_HEADER_MASK; 
   else if ( !strcmp(strheader, XFE_AddressFolderView::BCC) ) 
		header = MSG_BCC_HEADER_MASK; 
   else if ( !strcmp(strheader, XFE_AddressFolderView::REPLY) ) 
		header = MSG_REPLY_TO_HEADER_MASK; 
   else if ( !strcmp(strheader, XFE_AddressFolderView::NEWSGROUP) ) 
		header = MSG_NEWSGROUPS_HEADER_MASK; 
   else if ( !strcmp(strheader, XFE_AddressFolderView::FOLLOWUP) ) 
		header = MSG_FOLLOWUP_TO_HEADER_MASK; 

   return header;
}

const char * MSG_HeaderMaskToString(MSG_HEADER_SET header)
{
    const char *retval = NULL;
    switch (header)
    {
        case MSG_TO_HEADER_MASK:
            retval = XFE_AddressFolderView::TO;
            break;
        case MSG_CC_HEADER_MASK:
            retval = XFE_AddressFolderView::CC;
            break;
        case MSG_BCC_HEADER_MASK:
            retval = XFE_AddressFolderView::BCC;
            break;
        case MSG_REPLY_TO_HEADER_MASK:
            retval = XFE_AddressFolderView::REPLY;
            break;
        case MSG_NEWSGROUPS_HEADER_MASK:
            retval = XFE_AddressFolderView::NEWSGROUP;
            break;
        case MSG_FOLLOWUP_TO_HEADER_MASK:
            retval = XFE_AddressFolderView::FOLLOWUP;
            break;
        default:
            XP_ASSERT(0);
    }
    return retval;
}

void
XFE_AddressFolderView::initializeData(XFE_AddressFieldData *data)
{

  data->type = NULL;
  data->receipient = NULL;
  data->icon = ADDRESS_ICON_PERSON;
  data->currStr = NULL;
  data->insertPos = 0;
  data->security = 0;
}


void
XFE_AddressFolderView::setCurrentString(int line, const char *newStr)
{
  char *tmpStr;
  XFE_AddressFieldData *data;
  
  if ( XP_ListCount(m_fieldList) == 0 ) return;
  else
  {
     data = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
     if (!data) return;
  }
 
  if ( newStr && strlen(newStr) > 0 )
  {
	if ( data->currStr)
	tmpStr = (char*)XP_CALLOC(strlen(data->currStr)+strlen(newStr)+1, sizeof(char));
	else 
	  tmpStr = (char*)XP_CALLOC(strlen(newStr)+1, sizeof(char));
        if ( data->currStr )
        {
	   // delete old value
	   delete data->currStr;
	   data->currStr = NULL;
        }
	   strcpy(tmpStr, newStr);
	   data->currStr = tmpStr;
  }
  else 
  {
	delete data->currStr;
	data->currStr = NULL;
  }
}

void
XFE_AddressFolderView::getCurrentString(int line, char **textstr)
{
  XFE_AddressFieldData *data;
  
  *textstr = NULL;
  if ( XP_ListCount(m_fieldList) == 0 ) *textstr = NULL;
  else
  {
     data = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, line+1);
     if (!data) return;
     if ( data->currStr && strlen(data->currStr))
     {
	*textstr = (char*)XP_CALLOC(strlen(data->currStr)+1, sizeof(char));
	strcpy(*textstr, data->currStr);
     }
  }
}

XFE_AddressFolderView::~XFE_AddressFolderView()
{
}

// Address field drop site handler
void
XFE_AddressFolderView::AddressDropCb(Widget,void* cd,
                                     fe_dnd_Event type,fe_dnd_Source *source,
                                     XEvent*) {
    XFE_AddressFolderView *ad=(XFE_AddressFolderView*)cd;
    
    if (type==FE_DND_DROP && ad && source)
        ad->addressDropCb(source);
}

void
XFE_AddressFolderView::addressDropCb(fe_dnd_Source *source)
{
    XDEBUG(printf("XFE_AddressFolderView::addressDropCb()\n"));
    
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
    case FE_DND_LDAP_ENTRY:
        {
        if (source->closure)
            processLDAPDrop(source);
        break;
        }
    default:
        {
#if !defined(USE_MOTIF_DND)
        // pass non-addressbook drops to attachment panel
        XFE_ComposeFolderView* parent=(XFE_ComposeFolderView*)getParent();
        if (parent) {
            XFE_ComposeAttachFolderView* attachView=parent->getAttachFolderView();
            if (attachView)
                attachView->attachDropCb(source);
        }
#endif /* !defined(USE_MOTIF_DND) */
        }
        break;
    }
}

void
XFE_AddressFolderView::processAddressBookDrop(XFE_Outliner *outliner,
                                              DIR_Server *abDir,
                                              ABook *abBook,
                                              AddressPane *abPane)
{
    // Make sure insert happens at end of list - drag-over will have moved
    // text field position around under mouse.
    SEND_STATUS fieldStatus=::TO;
    int fieldCount=XP_ListCount(m_fieldList);
    if (fieldCount>0) {
        ((XFE_AddressOutliner*)m_outliner)->PlaceText(fieldCount-1,
                                                      ADDRESS_OUTLINER_COLUMN_RECEIPIENT, 
                                                      True);
        
        // Set field status to match that of last field in list - allows drop
        // of several TO's, then switch to drop of several CC's etc.
        XFE_AddressFieldData *tmpData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, fieldCount);
        if (tmpData) {
            if (tmpData->type==TO) fieldStatus=::TO;
            if (tmpData->type==CC) fieldStatus=::CC;
            if (tmpData->type==BCC) fieldStatus=::BCC;
        }
    }

    const int *selectedList=NULL;
    int numSelected=0;
    if (outliner->getSelection(&selectedList, &numSelected)) {
        StatusID_t **itemList=(StatusID_t**)XP_ALLOC(numSelected*sizeof(StatusID_t*));
        int numItems=0;

        int i;
        for (i=0; i<numSelected; i++) {
            ABID type;
            char *expName = NULL;
#if defined(USE_ABCOM)
		// type, fulladdress
		uint16 numItem = 2;
		AB_AttribID *attribs = (AB_AttribID *) XP_CALLOC(numItem, 
														 sizeof(AB_AttribID));
		attribs[0] = AB_attribEntryType;
		attribs[1] = AB_attribFullAddress;
		AB_AttributeValue *values = NULL;

		int error = 
			AB_GetEntryAttributesForPane((MSG_Pane *) abPane,
										 (MSG_ViewIndex) selectedList[i],
										 attribs,
										 &values,
										 &numItem);
		XP_ASSERT(values);
		for (int ii=0; ii < numItem; ii++) {
			switch (values[ii].attrib) {
			case AB_attribEntryType:
				type = values[ii].u.entryType;
				break;

			case AB_attribFullAddress:
				expName = 
					!EMPTY_STRVAL(&(values[ii]))?XP_STRDUP(values[ii].u.string)
					:NULL;
				break;
			default:
				XP_ASSERT(0);
				break;
			}/* switch */
		}/* for i */
		XP_FREEIF(attribs);
#else
            ABID entry = AB_GetEntryIDAt(abPane,(uint32)selectedList[i]);
    
            if (entry == MSG_VIEWINDEXNONE) 
                continue;

            // entry type
            AB_GetType(abDir, abBook, entry, &type);

            //email
            char email[AB_MAX_STRLEN];
            email[0] = '\0';
            AB_GetEmailAddress(abDir,abBook,entry,email);

            // name
            AB_GetExpandedName(abDir,abBook,entry,&expName);
            if (!expName) {
                char fullName[AB_MAX_STRLEN];
                fullName[0] = '\0';
                AB_GetFullName(abDir,abBook,entry,fullName);
                expName=XP_STRDUP(fullName);
            }
#endif /* USE_ABCOM */

            // add to address list via AddressFolder Address Picker callback

            StatusID_t *item=(StatusID_t*)XP_ALLOC(sizeof(StatusID_t));

            item->type=type;
            item->status=fieldStatus;
#if !defined(USE_ABCOM)
            item->dir=abDir;
            item->id=entry;
            item->emailAddr=XP_STRDUP(email);
#endif /* USE_ABCOM */
            item->dplyStr=expName;
            
            itemList[numItems++]=item;
        }
        
        if (numItems>0) {
            ABAddrMsgCBProcStruc *cbStruct=(ABAddrMsgCBProcStruc*)XP_ALLOC(sizeof(ABAddrMsgCBProcStruc));
            cbStruct->m_pairs=itemList;
            cbStruct->m_count=numItems;

            m_clearAddressee = False;
            addrMsgCB(cbStruct);    // Add to address panel. This call frees the item list
            m_clearAddressee = True;
            
        }
    }
}

void
XFE_AddressFolderView::processLDAPDrop(fe_dnd_Source* source)
{
    XFE_LdapSearchView *ldapView=(XFE_LdapSearchView *)source->closure;

    // Make sure insert happens at end of list - drag-over will have moved
    // text field position around under mouse.
    SEND_STATUS fieldStatus=::TO;
    int fieldCount=XP_ListCount(m_fieldList);
#ifdef DEBUG_sgidev
    printf("XFE_AddressFolderView::processLDAPDrop(%d)\n",fieldCount);
#endif    
    if (fieldCount>0) {
        ((XFE_AddressOutliner*)m_outliner)->PlaceText(fieldCount-1,
                                                      ADDRESS_OUTLINER_COLUMN_RECEIPIENT, 
                                                      True);
        // Set field status to match that of last field in list - allows drop
        // of several TO's, then switch to drop of several CC's etc.
        XFE_AddressFieldData *tmpData = (XFE_AddressFieldData*)XP_ListGetObjectNum(m_fieldList, fieldCount);
        if (tmpData) {
            if (tmpData->type==TO) fieldStatus=::TO;
            if (tmpData->type==CC) fieldStatus=::CC;
            if (tmpData->type==BCC) fieldStatus=::BCC;
        }
    }

    m_clearAddressee = False;
    ldapView->addSelectedToAddressPane(this,fieldStatus);
    m_clearAddressee = True;
}


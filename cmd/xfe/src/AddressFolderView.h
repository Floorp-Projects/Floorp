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
   AddressAddressFolderView.h -- class definition for AddressAddressFolderView
   Created: Dora Hsu<dora@netscape.com>, 23-Sept-96.
 */



#ifndef _xfe_addressfolderview_h
#define _xfe_addressfolderview_h

#include "rosetta.h"
#include "MNListView.h"
#include "Addressable.h"
#include "Outliner.h"
#include "Command.h"

#include "addrbk.h"

typedef struct XFE_AddressFieldData {
  const char *type;
  char *receipient;
  int  icon;
  short security;
  char *currStr;
  int insertPos;
} XFE_AddressFieldData;

class XFE_AddressFolderView : public XFE_MNListView, public MAddressable
{
public:
  XFE_AddressFolderView(
		XFE_Component *toplevel_component,
		XFE_View *parent_view, 
		MSG_Pane *p = NULL,
		MWContext *context = NULL);

  virtual ~XFE_AddressFolderView();
  virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType command, void *calldata = NULL,
						           XFE_CommandInfo* i = NULL);


  void initialize();
  void createWidgets(Widget);
  void updateHeaderInfo();
  /* Outlinable interface methods */
 // Converts between an index and some non positional data type.
  // Used to maintain selection when the list is reordered.
  virtual void *ConvFromIndex(int index);
  virtual int ConvToIndex(void *item);

  virtual char *getColumnName(int column);

  // Returns the text and/or icon to display at the top of the column.
  virtual char *getColumnHeaderText(int column);
  virtual fe_icon *getColumnHeaderIcon(int column);
  virtual EOutlinerTextStyle getColumnHeaderStyle(int column);

  // Tells the Outlinable object that it should get the data
  // for a line.  This line will be the focus of all requests
  // by the Outliner.  The return value is not interrogated, and
  // is only checked for NULL-ness.  return NULL if there isn't
  // any information for this line, the line is invalid, etc, etc.
  virtual void *acquireLineData(int line);

  //
  // The following 4 requests deal with the currently acquired line.
  //
  virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth,
                           OutlinerAncestorInfo **ancestor);
  virtual EOutlinerTextStyle getColumnStyle(int column);
  virtual char *getColumnText(int column);
  virtual fe_icon *getColumnIcon(int column);

  // Tells the Outlinable object that the line data is no
  // longer needed, and it can free any storage associated with
  // it.
  virtual void releaseLineData();

  virtual void Buttonfunc(const OutlineButtonFuncData *data);
  virtual void Flippyfunc(const OutlineFlippyFuncData *data);
  Boolean removeDataAt(int line);

  // For address book button
  XFE_CALLBACK_DECL(openAddrBk)

  void setHeader(int line, MSG_HEADER_SET header);
  int getHeader(int line);
  int setReceipient(int line,  char *pAddressStr);
  void updateReceipient(int line);

  // Might want to delete these two lines
  void setCurrentString(int line, const char *newStr);
  void getCurrentString(int line, char**textstr);

  void getDataAt(int line, int column, char **textstr);
  void insertNewDataAfter(int line);
  Boolean hasDataAt(int line);

  char *changedItem(char *pString, int* iconType, short* xxx);
 
  // Return actual data count from the receipient data list
  int getTotalData();

  static const char* tabPrev;
  static const char* tabNext;

  static const int ADDRESS_OUTLINER_COLUMN_BTN;
  static const int ADDRESS_OUTLINER_COLUMN_TYPE;
  static const int ADDRESS_OUTLINER_COLUMN_ICON;
#ifdef HAVE_ADDRESS_SECURITY_COLUMN
  HG21817
#endif
  static const int ADDRESS_OUTLINER_COLUMN_RECEIPIENT;

  static const char *TO;
  static const char *CC;
  static const char *BCC;
  static const char *REPLY;
  static const char *NEWSGROUP;
  static const char *FOLLOWUP;

  static const int ADDRESS_ICON_PERSON;
  static const int ADDRESS_ICON_NEWS;
  static const int ADDRESS_ICON_LIST;

  /* Addressee Picker
   */
  ABAddrMsgCBProcStruc* exportAddressees();
  static void addrMsgCallback(ABAddrMsgCBProcStruc* clientData, 
			void*                 callData);
  void addrMsgCB(ABAddrMsgCBProcStruc* clientData);
  void purgeAll();

  // internal drop callback for addressbook and address search window drops
  static void AddressDropCb(Widget,void*,fe_dnd_Event,fe_dnd_Source*,XEvent*);
  void addressDropCb(fe_dnd_Source*);
  void processAddressBookDrop(XFE_Outliner*,DIR_Server *abDir,ABook *abBook,
                              AddressPane*);
  void processLDAPDrop(fe_dnd_Source*);
private:

  ABook *m_pAddrBook;
  DIR_Server *m_pCompleteServer;

  XP_Bool m_clearAddressee;

  // icons for the outliner
  static fe_icon arrowIcon;
  static fe_icon personIcon;
  static fe_icon listIcon;
  static fe_icon newsIcon;

  HG81761
  const char *setData(int line,  char *receipient);

  // These member data may not need to be here ??
  Widget m_addBtnW;
  
  XFE_AddressFieldData *m_fieldData;
  XP_List 	*m_fieldList;
  XFE_Outliner *m_outliner;

  void setupIcons();
  void setupAddressHeadings();
  void btnActivate(Widget w, XtPointer);
  XFE_CALLBACK_DECL(tabToButton)
  void initializeData(XFE_AddressFieldData *data);
  static void btnActivateCallback(Widget w, XtPointer clientData, 
					XtPointer callData);

};

////////////////////////////////////////////////////////////
// backend candidates

const char * MSG_HeaderMaskToString(MSG_HEADER_SET header);
MSG_HEADER_SET  MSG_HeaderStringToMask(const char *strheader);
////////////////////////////////////////////////////////////
// end backend candidates

#endif /* _xfe_folderview_h */

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
   OptionFolderView.cpp -- presents view for option panel.
   Created: Dora Hsu <dora@netscape.com>, 23-Sept-96.
   */


#include "rosetta.h"
#include "OptionFolderView.h"
#include "ComposeView.h"
#include <Xm/Xm.h>
#include <Xm/ToggleBG.h>
#include <Xm/Form.h>
#include <xpgetstr.h> /* for XP_GetString() */

extern int XFE_MNC_OPTION_HIGHEST;
extern int XFE_MNC_OPTION_HIGH;
extern int XFE_MNC_OPTION_NORMAL;
extern int XFE_MNC_OPTION_LOW;
extern int XFE_MNC_OPTION_LOWEST;


static fe_OptionMenuItemDescription fe_msg_encoding_menu_items[] =
{
    // name	 	label				data
    { "8-bit",		"8-bit", 			(void*)MSG_8bit},
    { "MIME-compliant",	"MIME-compliant",		(void*)MSG_MimeCompliant},
    { NULL }
};

static fe_OptionMenuItemDescription fe_attach_encoding_menu_items[] =
{
    // name	 	label			data
    { "MIME",		"MIME (base 64)", 	(void*)MSG_Mime},
    { "UUENCODE",	"UUENCODE",		(void*)MSG_UUencode},
    { NULL }
};


#include "xpgetstr.h"

extern int XFE_MAIL_PRIORITY_LOWEST;
extern int XFE_MAIL_PRIORITY_LOW;
extern int XFE_MAIL_PRIORITY_NORMAL;
extern int XFE_MAIL_PRIORITY_HIGH;
extern int XFE_MAIL_PRIORITY_HIGHEST;

static
Widget
fe_OptionMenuCreate(Widget toplevel,  Widget parent, char *name,
			fe_OptionMenuItemDescription *list,
			XtCallbackProc option_proc, XtPointer option_clientdata)
{
  unsigned i;
  Widget   popup_menu;
  Widget   buttons[16];
  Widget   option_menu;
  Cardinal argc;
  Arg      argv[8];
  Visual*  v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;

  XtVaGetValues(toplevel, XtNvisual, &v, XtNcolormap, &cmap,
	XtNdepth, &depth, 0);

  argc = 0;
  XtSetArg (argv[argc], XmNvisual, v); argc++;
  XtSetArg (argv[argc], XmNdepth, depth); argc++;
  XtSetArg (argv[argc], XmNcolormap, cmap); argc++;

  popup_menu = XmCreatePulldownMenu(parent, name, argv, argc);

  for (i = 0; list[i].name; i++) {
      argc = 0;
      XtSetArg(argv[argc], XmNuserData, (XtPointer)list[i].data); argc++;
      buttons[i] = XmCreatePushButtonGadget(popup_menu, list[i].name, argv, argc);
      
	XtAddCallback(
			buttons[i],
			XmNactivateCallback,
			option_proc, option_clientdata);
  }

  XtManageChildren(buttons, i);

  argc = 0;
  XtSetArg(argv[argc], XmNsubMenuId, popup_menu); argc++;
  option_menu = fe_CreateOptionMenu( parent, name, argv, argc);

  return option_menu;
}

Boolean
XFE_OptionFolderView::isPrioritySelected(MSG_PRIORITY priority)
{
	if ( m_selectedPriority == priority )
		return True;
	return False;
}

void
XFE_OptionFolderView::selectPriority(MSG_PRIORITY priority)
{
  MSG_Pane *comppane = (MSG_Pane*)getPane(); 
  char *pPriority = priorityToString(priority);
  int index = 0;

  MSG_SetCompHeader(comppane,MSG_PRIORITY_HEADER_MASK, pPriority);
  if ( priority == MSG_HighestPriority )
	index = 0;
  else if ( priority == MSG_HighPriority)
	index = 1;
  else if ( priority == MSG_NormalPriority)
	index = 2;
  else if ( priority == MSG_LowPriority)
	index = 3;
  else if ( priority == MSG_LowestPriority)
	index = 4;
	
  m_selectedPriority = priority;

  (void)fe_OptionMenuSetHistory(m_optionW, (int)index);
}

void
XFE_OptionFolderView::selectPriorityCallback(
			Widget w, XtPointer client, XtPointer)
{
  
	CommandType command;


  XFE_OptionFolderView *obj = (XFE_OptionFolderView*)client;
  MSG_PRIORITY priority = MSG_PriorityNotSet;

  XtVaGetValues(w, XmNuserData, &priority,  0);
  obj->selectPriority(priority);
  if ( priority == MSG_HighestPriority )
	command = xfeCmdSetPriorityHighest;
  else if ( priority == MSG_HighPriority )
        command = xfeCmdSetPriorityHigh;
  else if ( priority == MSG_NormalPriority )
        command = xfeCmdSetPriorityNormal;
  else if ( priority == MSG_LowPriority )
        command = xfeCmdSetPriorityLow;
  else if ( priority == MSG_LowestPriority )
        command = xfeCmdSetPriorityLowest;
  else
	  return;

  obj->getToplevel()->notifyInterested(XFE_View::commandNeedsUpdating,
			(void*)command );
}

void
XFE_OptionFolderView::selectTextEncoding(MSG_TEXT_ENCODING encoding)
{

  if ( encoding == MSG_8bit )
     MIME_ConformToStandard(False);
  else
     MIME_ConformToStandard(True);
}

void
XFE_OptionFolderView::selectTextEncodingCallback(
			Widget w, XtPointer client, XtPointer)
{
  XFE_OptionFolderView *obj = (XFE_OptionFolderView*)client;
  MSG_TEXT_ENCODING encoding = MSG_TextEncodingNotSet;

  XtVaGetValues(w, XmNuserData, &encoding,  0);
  obj->selectTextEncoding(encoding);
}

void
XFE_OptionFolderView::selectBinaryEncoding(MSG_BINARY_ENCODING encoding)
{

  if ( encoding == MSG_Mime )
       MSG_SetCompBoolHeader(getPane(), 
		MSG_UUENCODE_BINARY_BOOL_HEADER_MASK, False);
  else if ( encoding == MSG_UUencode )
       MSG_SetCompBoolHeader(getPane(), 
		MSG_UUENCODE_BINARY_BOOL_HEADER_MASK, True);
  else 
	XP_ASSERT(0); // Should never reach here.... because we only have two states
}

void
XFE_OptionFolderView::selectBinaryEncodingCallback(
			Widget w, XtPointer client, XtPointer)
{
  XFE_OptionFolderView *obj = (XFE_OptionFolderView*)client;
  MSG_BINARY_ENCODING encoding = MSG_BinaryEncodingNotSet;

  XtVaGetValues(w, XmNuserData, &encoding,  0);
  obj->selectBinaryEncoding(encoding);
}

//Destructor
XFE_OptionFolderView::~XFE_OptionFolderView()
{
}

//Constructor
XFE_OptionFolderView::XFE_OptionFolderView(
		XFE_Component *toplevel_component,
		XFE_View *parent_view,
		MSG_Pane *p,
		MWContext *context)
	:XFE_MNView(toplevel_component, parent_view, context, p)
{
	setParent(parent_view);
	// Initialize Data
	m_bReturnReceipt = False;
	m_bEncrypted = False;
	m_bSigned = False;
	m_selectedPriority = MSG_NormalPriority;

	parent_view->getToplevel()->
		registerInterest(XFE_ComposeView::updateSecurityOption,
			this,
			(XFE_FunctionNotification)updateSecurityOption_cb);
}

XFE_CALLBACK_DEFN(XFE_OptionFolderView,updateSecurityOption)(XFE_NotificationCenter *, void *, void*)
{
  Boolean set = False;
  Boolean notify = False;

  
  HG20192
  if ( notify )
    getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating,
			(void*)NULL);
}

void
XFE_OptionFolderView::createWidgets(Widget parent_widget)
{
    Arg av[10];
    int ac = 0;

	static fe_OptionMenuItemDescription fe_priority_menu_items[] =
	{
		// name	 		        label           data
		{ "setPriorityHighest",	(char*)NULL, 	(void*)MSG_HighestPriority},
		{ "setPriorityHigh",	(char*)NULL, 	(void*)MSG_HighPriority},
		{ "setPriorityNormal", 	(char*)NULL, 	(void*)MSG_NormalPriority},
		{ "setPriorityLow",  	(char*)NULL, 	(void*)MSG_LowPriority},
		{ "setPriorityLowest", 	(char*)NULL, 	(void*)MSG_LowestPriority},
		{ NULL }
	};

	// HP-UX 9.03 won't let us assign these values inside the initializer list.
	fe_priority_menu_items[0].label = XP_GetString(XFE_MNC_OPTION_HIGHEST);
	fe_priority_menu_items[1].label = XP_GetString(XFE_MNC_OPTION_HIGH);
	fe_priority_menu_items[2].label = XP_GetString(XFE_MNC_OPTION_NORMAL);
	fe_priority_menu_items[3].label = XP_GetString(XFE_MNC_OPTION_LOW);
	fe_priority_menu_items[4].label = XP_GetString(XFE_MNC_OPTION_LOWEST);

    Widget form = XmCreateForm(parent_widget, "optionForm", NULL, 0);
    XtVaSetValues(form, XmNfractionBase, 3, 0);

    setBaseWidget(form);

    Widget RC0 = XmCreateRowColumn(form, "RC0", NULL, 0);
    XtVaSetValues(RC0, XmNorientation, XmVERTICAL, 0);

    Widget RC1 = XmCreateRowColumn(form, "RC1", NULL, 0);
    XtVaSetValues(RC1, XmNorientation, XmVERTICAL, 0);

    int num = 5;
	for (int i=0; i < num; i++) {
		fe_priority_menu_items[i].label = 
			XP_GetString(XFE_MAIL_PRIORITY_LOWEST+num-1-i);
	}/* for i */
    m_optionW = fe_OptionMenuCreate(getToplevel()->getBaseWidget(),
                        form,
                        "PriorityOption",
                        fe_priority_menu_items,
                        &XFE_OptionFolderView::selectPriorityCallback,
                        (void*) this);

    m_textEncodingOptionW = fe_OptionMenuCreate(getToplevel()->getBaseWidget(),
                        RC1,
                        "TextEncodingOption",
                        fe_msg_encoding_menu_items,
                        &XFE_OptionFolderView::selectTextEncodingCallback,
                        (void*) this);
    m_binaryEncodingOptionW = fe_OptionMenuCreate(getToplevel()->getBaseWidget(),
                        RC1,
                        "BinaryEncodingOption",
                        fe_attach_encoding_menu_items,
                        &XFE_OptionFolderView::selectBinaryEncodingCallback,
                        (void*) this);

    m_returnReceiptW = XmCreateToggleButtonGadget( RC0,
		"returnReceipt", av, ac );		

    XtManageChild(m_textEncodingOptionW);
    XtManageChild(m_binaryEncodingOptionW);
    XtManageChild(m_returnReceiptW);
    XtAddCallback(m_returnReceiptW, XmNvalueChangedCallback, 
		returnReceiptChangeCallback, (XtPointer)this);

    HG20198
    // Do Some Attachments


    XtVaSetValues( RC0,
	XmNalignment, XmALIGNMENT_BEGINNING,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_NONE,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_NONE,
	NULL);

    XtManageChild(RC0);

    XtVaSetValues( RC1,
	XmNalignment, XmALIGNMENT_BEGINNING,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, RC0,
        XmNleftOffset, 5,
        XmNrightAttachment, XmATTACH_NONE,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_NONE,
	NULL);
    XtManageChild(RC1);

    XtVaSetValues( m_optionW,
	XmNalignment, XmALIGNMENT_BEGINNING,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, RC1,
        XmNleftOffset, 5,
        XmNrightAttachment, XmATTACH_NONE,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_NONE,
	NULL);
    XtManageChild(m_optionW);

    // Text Encoding .... 
    // Initially, get the default setting from the preferences
    // question - do we do this for each option tab click ???? 
    if (fe_globalPrefs.qp_p)
    {
	int index = 0;
	// do quotable print... encoding
	 index = (int) MSG_MimeCompliant - 1;
         (void)fe_OptionMenuSetHistory(m_textEncodingOptionW, (int)index);
         MIME_ConformToStandard(True);
    }
    else 
    {
	int index = 0;
	// do 8bit... encoding
	 index = (int) MSG_8bit - 1;
         (void)fe_OptionMenuSetHistory(m_textEncodingOptionW, (int)index);
         MIME_ConformToStandard(False);
    }

    updateAllOptions();

    XtManageChild(form);
}
 
void
XFE_OptionFolderView::updateAllOptions()
{
    // Will decide the state of these buttons

    // Return Receipt
    if ( MSG_GetCompBoolHeader(getPane(), MSG_RETURN_RECEIPT_BOOL_HEADER_MASK) )
    {
    	XtVaSetValues (m_returnReceiptW, XmNset, True, 0);
    }
    else 
    	XtVaSetValues (m_returnReceiptW, XmNset, False, 0);

    HG35220


   // Binary encoding...
   int index = 0;
   if ( MSG_GetCompBoolHeader(getPane(), MSG_UUENCODE_BINARY_BOOL_HEADER_MASK) )
    {
	 index = (int) MSG_UUencode - 1;
         (void)fe_OptionMenuSetHistory(m_binaryEncodingOptionW, (int)index);
    }
  else
    {
	 index = (int) MSG_Mime - 1;
         (void)fe_OptionMenuSetHistory(m_binaryEncodingOptionW, (int)index);
    }
}

void
XFE_OptionFolderView::handleReturnReceipt(Boolean set)
{
  if (set)
    MSG_SetCompBoolHeader(getPane(), MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, True);
  else
    MSG_SetCompBoolHeader(getPane(), MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, False);
}

void
XFE_OptionFolderView::returnReceiptChangeCallback( Widget w, XtPointer client, XtPointer)
{
	XFE_OptionFolderView *obj = (XFE_OptionFolderView*)client;
	Boolean set = False;

	XtVaGetValues(w, XmNset, &set, 0 );
        obj->handleReturnReceipt(set);
}

void
XFE_OptionFolderView::handleEncrypted(Boolean set)
{
  Boolean notify = False;
  HG82811

  if (notify) 
  {
    getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating,
			(void*)NULL);
  } 

}


void
XFE_OptionFolderView::encryptedChangeCallback( Widget w, XtPointer client, XtPointer)
{
	XFE_OptionFolderView *obj = (XFE_OptionFolderView*)client;
	Boolean set = False;

	XtVaGetValues(w, XmNset, &set, 0 );
        obj->handleEncrypted(set);
}

void
XFE_OptionFolderView::handleSigned(Boolean set)
{
  Boolean notify = False;
  HG97291
  if (notify)
  {
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating,
			(void*)NULL);
  }

}

void
XFE_OptionFolderView::signedChangeCallback( Widget w, XtPointer client, XtPointer)
{
	XFE_OptionFolderView *obj = (XFE_OptionFolderView*)client;
	Boolean set = False;

	XtVaGetValues(w, XmNset, &set, 0 );
        obj->handleSigned(set);
}


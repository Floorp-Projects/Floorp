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
   ComposeFolderView.cpp -- presents view for addressing and attachment .
   Created: Dora Hsu <dora@netscape.com>, 23-Sept-96.
   */



#include "ComposeView.h"
#include "ComposeFolderView.h"
#include "AddressFolderView.h"
#include "ComposeAttachFolderView.h"
#include "AddressOutliner.h"
#include "xfe.h"
#include "IconGroup.h"
#include "xp_mem.h"
#include <xpgetstr.h> /* for XP_GetString() */

#include <Xm/Xm.h>
#include <Xm/Frame.h>
#include <XmL/XmL.h>
#include <Xm/DrawnB.h>
#include <Xm/ArrowB.h>
#include <Xm/LabelG.h>
#include <XmL/Folder.h>
#include <Xfe/Button.h>

extern int XFE_MNC_ADDRESS;
extern int XFE_MNC_ATTACHMENT;
extern int XFE_MNC_OPTION;

#ifdef DEBUG_dora
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

extern "C" {
  Widget fe_EditorCreateComposeToolbar(MWContext*, Widget, char*);
}

extern "C" 
Widget fe_MailComposeWin_Create(MWContext* context, Widget parent);

extern "C" Widget
fe_MailComposeAddress_CreateManaged(MWContext* context, Widget parent);

extern "C" Widget
fe_MailComposeAttach_CreateManaged(MWContext* context, Widget parent);

extern "C" Widget 
makeOptionMenu(MWContext *context, Widget parent);


Boolean
XFE_ComposeFolderView::isPrioritySelected(MSG_PRIORITY priority)
{
        return m_optionFolderViewAlias->isPrioritySelected(priority);
}

void
XFE_ComposeFolderView::selectPriority(MSG_PRIORITY priority)
{
  m_optionFolderViewAlias->selectPriority(priority);
}


/* Constructor */
XFE_ComposeFolderView::XFE_ComposeFolderView(
			XFE_Component *toplevel_component,
			XFE_View *parent_view,
			MSG_Pane *p,
			MWContext *context) 
  : XFE_MNView(toplevel_component, parent_view, context, p)
{
  //XP_ASSERT(p==NULL);

  setParent(parent_view);
  /* Create Widgets */

  // Initialize necessary data
  m_addressFormW = 0;
  m_attachFormW = 0;
  m_optionFormW = 0;
  m_frameW = 0;
  m_optionW = 0;

 // m_expanded = True;
  m_tabNumber = 0;
  m_selectedPriority = MSG_NormalPriority;
  m_attachFolderViewAlias = NULL;
  m_addressFolderViewAlias = NULL;
}

XP_Bool
XFE_ComposeFolderView::isCommandSelected(CommandType command,
										 void* calldata,
										 XFE_CommandInfo* info)
{
  if ((command == xfeCmdViewAddresses)
	  || (command == xfeCmdViewOptions)
	  || (command == xfeCmdViewAttachments)) {

	  int tab_number;

	  XtVaGetValues(m_tabGroupW, XmNactiveTab, &tab_number, 0);

	  if (command == xfeCmdViewAddresses)
		  return (tab_number == 0);
	  else if (command == xfeCmdViewAttachments)
		  return (tab_number == 1);
	  else // if (command == xfeCmdViewOptions)
		  return (tab_number == 2);

  } else {
	  return XFE_MNView::isCommandSelected(command, calldata, info);
  }
}

Boolean
XFE_ComposeFolderView::isCommandEnabled(CommandType command,
										void *, XFE_CommandInfo* )
{
  if ((command == xfeCmdViewAddresses)
	||  (command == xfeCmdViewOptions)
        || (command == xfeCmdViewAttachments))
     return True;
  else if (m_addressFolderViewAlias &&
      m_addressFolderViewAlias->isCommandEnabled(command))
    return True;
  else if ( m_attachFolderViewAlias &&
     m_attachFolderViewAlias->isCommandEnabled(command))
    return True;

  return False;
}

Boolean
XFE_ComposeFolderView::handlesCommand(CommandType command,
									  void *, XFE_CommandInfo*)
{
XDEBUG(	printf ("in XFE_ComposeFolderView::handlesCommand(%s)\n", Command::getString(command));)
  if ((command == xfeCmdViewAddresses)
	|| (command == xfeCmdViewOptions)
        || (command == xfeCmdViewAttachments))
     return True;
  else if (m_addressFolderViewAlias && 
	m_addressFolderViewAlias->handlesCommand(command))
    return True;
  else if ( m_attachFolderViewAlias &&
  	m_attachFolderViewAlias->handlesCommand(command))
    return True;

XDEBUG(	printf ("leaving XFE_ComposeFolderView::handlesCommand(%s), Command::getString(command)\n");)
  return False;
}

void
XFE_ComposeFolderView::doCommand(CommandType command,
								 void *, XFE_CommandInfo* )
{
  if (command == xfeCmdViewAddresses)
    {
      XDEBUG(	printf ("XFE_ComposeFolderView::xfeCmdViewAddresses()\n");)
      XmLFolderSetActiveTab(m_tabGroupW, 0, True);
    }
  else if (command == xfeCmdViewAttachments)
    {
      XDEBUG(	printf ("XFE_ComposeFolderView::xfeCmdViewAttachments()\n");)
      XmLFolderSetActiveTab(m_tabGroupW, 1, True);
    }
  else if (command == xfeCmdViewOptions)
    {
      XDEBUG(	printf ("XFE_ComposeFolderView::xfeCmdViewOptions()\n");)
      XmLFolderSetActiveTab(m_tabGroupW, 2, True);
    }
  else if ( command == xfeCmdAttachFile)
    {
       if (m_attachFolderViewAlias)
	   m_attachFolderViewAlias->doCommand(command);
    }
  else if ( command == xfeCmdAttachWebPage )
    {
       if (m_attachFolderViewAlias)
	   m_attachFolderViewAlias->doCommand(command);
    }
  else if ( command == xfeCmdDeleteAttachment )
    {
       if (m_attachFolderViewAlias)
	   m_attachFolderViewAlias->doCommand(command);
    }
  else if ( command == xfeCmdDelete )
    {
       if (m_attachFolderViewAlias)
	   m_attachFolderViewAlias->doCommand(command);
    }
  else if ( command == xfeCmdAddresseePicker )
    {
		if (m_addressFolderViewAlias)
			m_addressFolderViewAlias->doCommand(command);
    }

XDEBUG(	printf ("leaving XFE_ComposeFolderView::doCommand()\n");)
}

// called when any compose folder tab is pressed
void
XFE_ComposeFolderView::folderActivate(int tabPosition)
{
    // ASSUMPTION: Tab #1 is 'Attachments'
    // Allow Attach folder to track its state
    if (m_attachFolderViewAlias) {
        m_attachFolderViewAlias->folderVisible(tabPosition==1 ? TRUE : FALSE);
    }
    else if ( tabPosition == 2 ) // ASSUMPTION: Tab #2 is 'Options'
    {
	if (m_optionFolderViewAlias)
	    m_optionFolderViewAlias->updateAllOptions();
    }
}


void
XFE_ComposeFolderView::createWidgets(Widget parent_widget, XP_Bool /*usePlainText*/)
{

   XmString str;
   Widget formBaseW;

  XDEBUG( printf("enter XFE_ComposeFolderView::createWidgets()\n");)
   
   Visual *v =0;
   Colormap cmap = 0;
   Cardinal depth = 0;

   XtVaGetValues(getToplevel()->getBaseWidget(),
	XmNvisual, &v,
	XmNcolormap, &cmap,
	XmNdepth, &depth, 0);

   formBaseW = XtVaCreateManagedWidget("addressBaseForm",
				xmFormWidgetClass, parent_widget, 
				XmNcolormap, cmap,
				XmNdepth, depth,
				XmNvisual, v, 0);

   setBaseWidget(formBaseW);
   setupIcons(); //- don't do icon for now
   

   {

   m_frameW = XtVaCreateManagedWidget("newAddressFrame",
			xmFrameWidgetClass, formBaseW, 
			XmNshadowType, XmSHADOW_IN, 
			XmNtopAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM, 
				XmNcolormap, cmap,
				XmNdepth, depth,
				XmNvisual, v, 0);

   m_tabGroupW =  XtVaCreateWidget("newComposeFolder",
                          xmlFolderWidgetClass,
                          m_frameW, 
			XmNcolormap, cmap,
			XmNtabPlacement, XmFOLDER_LEFT,
			XmNtabWidgetClass, xfeButtonWidgetClass,
                                XmNdepth, depth,
                                XmNvisual, v, 0);
   XtAddCallback(m_tabGroupW,XmNactivateCallback,folderActivateCallback,(XtPointer)this);

   XtManageChild(m_tabGroupW);

   Pixel bg;
   XtVaGetValues(m_tabGroupW, XmNbackground, &bg, 0);

   str = XmStringCreateSimple(XP_GetString(XFE_MNC_ADDRESS));
   Widget m_addressTab = XmLFolderAddTabFromClass(m_tabGroupW, str);
   XtVaSetValues(m_addressTab, XmNbuttonLayout, XmBUTTON_PIXMAP_ONLY,
		XmNpixmap,  MNC_AddressSmall_group.pixmap_icon.pixmap,
	 NULL);
   XmStringFree(str);

   m_addressFormW = XtVaCreateManagedWidget("form_addr",
                                 xmFormWidgetClass, m_tabGroupW,
                                 XmNbackground, bg ,
                                 NULL);

   XtVaSetValues(m_addressTab, XmNtabManagedWidget, m_addressFormW, NULL);

   str = XmStringCreateSimple(XP_GetString(XFE_MNC_ATTACHMENT));
   //m_attachFormW = XmLFolderAddTabForm(m_tabGroupW, str);
   Widget m_attachTab = XmLFolderAddTabFromClass(m_tabGroupW, str);
   XtVaSetValues(m_attachTab, XmNbuttonLayout, XmBUTTON_PIXMAP_ONLY,
		XmNpixmap,  MNC_AttachSmall_group.pixmap_icon.pixmap, NULL);
   XmStringFree(str);
   m_attachFormW = XtVaCreateManagedWidget("form_attach",
                                 xmFormWidgetClass, m_tabGroupW,
                                 XmNbackground, bg,
                                 NULL);

   XtVaSetValues(m_attachTab, XmNtabManagedWidget, m_attachFormW, NULL);


   str = XmStringCreateSimple(XP_GetString(XFE_MNC_OPTION));
   //m_optionFormW = XmLFolderAddTabForm(m_tabGroupW, str);
   Widget m_optionTab = XmLFolderAddTabFromClass(m_tabGroupW, str);
   XtVaSetValues(m_optionTab, XmNbuttonLayout, XmBUTTON_PIXMAP_ONLY,
		XmNpixmap,  MNC_Options_group.pixmap_icon.pixmap, NULL);
   XmStringFree(str);
   m_optionFormW = XtVaCreateManagedWidget("form_attach",
                   	xmFormWidgetClass, m_tabGroupW,
                        XmNbackground, bg,
                        NULL);

   XtVaSetValues(m_optionTab, XmNtabManagedWidget, m_optionFormW, NULL);

   { // Addressing Folder
     XFE_AddressFolderView *addressFolderView = 
         new XFE_AddressFolderView( getToplevel(), (XFE_View*)this, getPane(), getContext());

     addView(addressFolderView);

     m_addressFolderViewAlias = addressFolderView;

     addressFolderView->createWidgets(m_addressFormW);
     Widget addressContent;

     addressContent = addressFolderView->getBaseWidget();
     XtVaSetValues(addressContent, XmNtopAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM, NULL);
     addressFolderView->show();
   }
   {// Attachment Folder
     XFE_ComposeAttachFolderView *attachFolderView = 
         new XFE_ComposeAttachFolderView(getToplevel(),(XFE_View*)this,getPane(), getContext());
     addView(attachFolderView);
     m_attachFolderViewAlias = attachFolderView;

     attachFolderView->createWidgets(m_attachFormW);
     Widget attachContent;

     attachContent = attachFolderView->getBaseWidget();
     XtVaSetValues(attachContent, XmNtopAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM, NULL);
     attachFolderView->show();
   }

   { // Option Folder
     XFE_OptionFolderView *optionFolderView = 
         new XFE_OptionFolderView( getToplevel(), (XFE_View*)this, getPane(), getContext());

     addView(optionFolderView);

     m_optionFolderViewAlias = optionFolderView;

     optionFolderView->createWidgets(m_optionFormW);
     Widget optionContent;

     optionContent = optionFolderView->getBaseWidget();
     XtVaSetValues(optionContent, XmNtopAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM, NULL);
     optionFolderView->show();
   }

   } // End of Address Frame
  XDEBUG( printf("leave XFE_ComposeFolderView::createWidgets()\n");)
}

void
XFE_ComposeFolderView::folderActivateCallback(Widget, XtPointer clientData, XtPointer callData)
{
  XFE_ComposeFolderView *obj = (XFE_ComposeFolderView*)clientData;
  XmLFolderCallbackStruct *fcb=(XmLFolderCallbackStruct*)callData;

  if (obj && fcb)
      obj->folderActivate(fcb->pos);
}


//-------------NEW code ---------------------------------------

#ifndef EDITOR
Widget
fe_OptionMenuSetHistory(Widget menu, unsigned index)
{
    Arg        args[4];
    Cardinal   n;
    Widget     cascade;
    Widget     popup_menu;
    WidgetList children;
    Cardinal   nchildren;

    /*
     *   Update the label, and set the position of the popup.
     */
    cascade = XmOptionButtonGadget(menu);

    /*
     *    Get the popup menu from the cascade.
     */
    n = 0;
    XtSetArg(args[n], XmNsubMenuId, &popup_menu); n++;
    XtGetValues(cascade, args, n);

    /*
     *    Get the children of the popup.
     */
    n = 0;
    XtSetArg(args[n], XmNchildren, &children); n++;
    XtSetArg(args[n], XmNnumChildren, &nchildren); n++;
    XtGetValues(popup_menu, args, n);
    if (index < nchildren) {
        /*
         *    Finally, set the Nth button as history.
         */
        n = 0;
        XtSetArg(args[n], XmNmenuHistory, children[index]); n++;
        /* NOTE: set it on the top level menu (strange) */
        XtSetValues(menu, args, n);

        return children[index];
    }
    return NULL;
}

#endif

void
XFE_ComposeFolderView::updateHeaderInfo(void)
{

  
  if ( m_addressFolderViewAlias )
	m_addressFolderViewAlias->updateHeaderInfo();

}

void
XFE_ComposeFolderView::setupIcons()
{
  Widget base = getToplevel()->getBaseWidget();

  Pixel bg_pixel;

  XtVaGetValues(base,  XmNbackground, &bg_pixel, 0);

  IconGroup_createAllIcons(&MNC_AddressSmall_group,
                            getToplevel()->getBaseWidget(),
                            BlackPixelOfScreen(XtScreen(base)), // hack. :(
                            bg_pixel);

  IconGroup_createAllIcons(&MNC_AttachSmall_group,
                            getToplevel()->getBaseWidget(),
                            BlackPixelOfScreen(XtScreen(base)), // hack. :(
                            bg_pixel);

  IconGroup_createAllIcons(&MNC_Options_group,
                            getToplevel()->getBaseWidget(),
                            BlackPixelOfScreen(XtScreen(base)), // hack. :(
                            bg_pixel);
}

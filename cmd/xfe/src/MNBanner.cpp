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
   MNBanner.cpp -- implementation for the MNBanner funky little pseudo title bar proxy icon thingy.
   Created: Chris Toshok <toshok@netscape.com>, 15-Oct-96.
 */



#include "MNBanner.h"
#include "Logo.h"

#include <Xm/Frame.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/ArrowB.h>

#include <Xfe/ToolItem.h>
#include "ThreePaneView.h"

const char *XFE_MNBanner::twoPaneView = "XFE_MNBanner::twoPaneView";
XFE_MNBanner::XFE_MNBanner(XFE_Frame *parent_frame,
						   XFE_Toolbox * parent_toolbox,
						   char *title, char *subtitle,
						   char *info) 
	: XFE_ToolboxItem(parent_frame,parent_toolbox)
{
  Widget tool_item;

  m_parentFrame = parent_frame;

  m_isFolderShown = True;
  // Create the base widget - a tool item
  tool_item = XtVaCreateWidget("bannerItem",
							   xfeToolItemWidgetClass,
							   parent_toolbox->getBaseWidget(),
							   XmNuserData,				this,
							   NULL);
	
  m_form = XtVaCreateManagedWidget("banner",
								   xmFormWidgetClass,
								   tool_item,
								   NULL);
  
  // no proxy icon yet -- you must call setProxyIcon.
  
  m_titlelabel = XtVaCreateManagedWidget("title",
										 xmLabelWidgetClass,
										 m_form,
										 XmNleftAttachment, XmATTACH_FORM,
										 XmNrightAttachment, XmATTACH_NONE,
										 XmNtopAttachment, XmATTACH_FORM,
										 XmNbottomAttachment, XmATTACH_FORM,
										 NULL);

  m_subtitlelabel = XtVaCreateManagedWidget("subtitle",
											xmLabelWidgetClass,
											m_form,
											XmNleftAttachment, XmATTACH_WIDGET,
											XmNleftWidget, m_titlelabel,
											XmNrightAttachment, XmATTACH_NONE,
											XmNtopAttachment, XmATTACH_FORM,
											XmNbottomAttachment, XmATTACH_FORM,
											NULL);

  m_infolabel = XtVaCreateManagedWidget("info",
										xmLabelWidgetClass,
										m_form,
										XmNalignment, XmALIGNMENT_END,
										XmNleftAttachment, XmATTACH_WIDGET,
										XmNleftWidget, m_subtitlelabel,
										XmNrightAttachment, XmATTACH_FORM,
										XmNtopAttachment, XmATTACH_FORM,
										XmNbottomAttachment, XmATTACH_FORM,
										NULL);

  // Register banner widgets for dragging
  addDragWidget(m_form);
  addDragWidget(m_titlelabel);
  addDragWidget(m_subtitlelabel);
  addDragWidget(m_infolabel);

  // Create the logo
  m_logo = new XFE_Logo(m_parentFrame,tool_item,"logo");

  // It will always be small for this component
  m_logo->setSize(XFE_ANIMATION_SMALL);
										
  setBaseWidget(tool_item);

  m_proxyicon = NULL;
  m_mommyButton = NULL;
  m_titleComponent = NULL;
  m_arrowButton = NULL;

  setTitle(title);
  setSubtitle(subtitle);
  setInfo(info);
}

void
XFE_MNBanner::setMommyIcon(IconGroup *icons)
{
	if (!m_mommyButton) {
		m_mommyButton = new XFE_Button(m_parentFrame,
									   m_form, 
									   "mommy", 
									   icons);
		// 
		m_mommyButton->setToplevel(m_parentFrame);

	}/* if */
	else
		m_mommyButton->setPixmap(icons); // XXX 

	XtVaSetValues(m_mommyButton->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(m_infolabel,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_subtitlelabel,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_mommyButton->getBaseWidget(),
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	m_mommyButton->show();

	m_mommyButton->registerInterest(XFE_Button::doCommandCallback,
									this,
									(XFE_FunctionNotification)doMommyCommand_cb);
}

XFE_CALLBACK_DEFN(XFE_MNBanner, doMommyCommand)(XFE_NotificationCenter *,
												void *,
												void *)
{
	m_parentFrame->doCommand(xfeCmdMommy);
}

void
XFE_MNBanner::showFolderBtn()
{
  getToplevel()->notifyInterested(XFE_MNBanner::twoPaneView);
}

void
showFolderBtnCB(Widget w, XtPointer clientData, XtPointer)
{
  XFE_MNBanner *obj = (XFE_MNBanner*)clientData;
  unsigned char arrowDir;

  XtVaGetValues(w, XmNarrowDirection, &arrowDir, NULL);
  if ( arrowDir == XmARROW_DOWN) 
	XtVaSetValues(w, XmNarrowDirection, XmARROW_RIGHT, NULL);
  else
	XtVaSetValues(w, XmNarrowDirection, XmARROW_DOWN, NULL);

  obj->showFolderBtn();
}

void 
XFE_MNBanner::setProxyIcon(fe_icon *icon)
{
	Widget title_widget;


	if (!m_proxyicon)
		m_proxyicon = new XFE_ProxyIcon(m_toplevel, m_form, "mnProxy", icon);
	else
		m_proxyicon->setIcon(icon);
	
#if 0
	XtVaSetValues(m_proxyicon->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
#endif
	XtVaSetValues(m_proxyicon->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_arrowButton,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
	
	if (m_titleComponent)
		title_widget = m_titleComponent->getBaseWidget();
	else
		title_widget = m_titlelabel;
	
	XtVaSetValues(title_widget,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_proxyicon->getBaseWidget(),
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
	
	m_proxyicon->show();
}

void
XFE_MNBanner::setTitle(char *title)
{
  XmString xmstr;

  if (m_titleComponent) // we can't possibly support this in an intelligent way.
	  return;

  if (title == NULL)
	  title = " ";

  xmstr = XmStringCreate(title, XmFONTLIST_DEFAULT_TAG);

  XtVaSetValues(m_titlelabel,
				XmNlabelString, xmstr,
				NULL);

  XmStringFree(xmstr);
}

void
XFE_MNBanner::setTitleComponent(XFE_Component *title_component)
{
	XtUnmanageChild(m_titlelabel);

	m_titleComponent = title_component;

	XtVaSetValues(m_titleComponent->getBaseWidget(),
				  XmNleftAttachment, m_proxyicon ? XmATTACH_WIDGET : XmATTACH_FORM,
				  XmNleftWidget, m_proxyicon ? m_proxyicon->getBaseWidget() : NULL,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_subtitlelabel,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_titleComponent->getBaseWidget(),
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	m_titleComponent->show();
}

void
XFE_MNBanner::setSubtitle(char *subtitle)
{
  XmString xmstr;

  if (subtitle == NULL)
	  subtitle = " ";

  xmstr = XmStringCreate(subtitle, XmFONTLIST_DEFAULT_TAG);

  XtVaSetValues(m_subtitlelabel,
		XmNlabelString, xmstr,
		NULL);

  XmStringFree(xmstr);
}

void
XFE_MNBanner::setInfo(char *info)
{
	XmString xmstr;
	XmString old_info;
	XP_Bool needsChange = TRUE;
	char *oldinfoStr = NULL;

	if (info == NULL)
		info = " ";

	XtVaGetValues (m_infolabel, 
				   XmNlabelString, &old_info,
				   NULL);

	if (XmStringGetLtoR(old_info, XmFONTLIST_DEFAULT_TAG, &oldinfoStr))
		if (oldinfoStr && !strcmp(oldinfoStr, info))
			{
				needsChange = FALSE;
				if (oldinfoStr) XtFree(oldinfoStr);
			}
	
	if (needsChange)
		{
			xmstr = XmStringCreate(info, XmFONTLIST_DEFAULT_TAG);
			
			XtVaSetValues(m_infolabel,
						  XmNlabelString, xmstr,
						  NULL);
			
			XmStringFree(xmstr);
		}
}

Widget
XFE_MNBanner::getComponentParent()
{
	return m_form;
}

XFE_ProxyIcon *
XFE_MNBanner::getProxyIcon()
{
  return m_proxyicon;
}

char *
XFE_MNBanner::getTitle()
{
  XP_ASSERT(0);
  return 0;
}

char *
XFE_MNBanner::getSubtitle()
{
  XP_ASSERT(0);
  return 0;
}

char *
XFE_MNBanner::getInfo()
{
	XP_ASSERT(0);
	return 0;
}

XFE_Button *
XFE_MNBanner::getMommyButton()
{
	return m_mommyButton;
}

XFE_CALLBACK_DEFN(XFE_MNBanner,showFolder)(XFE_NotificationCenter *,
                                 void *, void* cd)
{
  unsigned char arrowDir;
  int32 showFolder = (int32)cd;

  XtVaGetValues(m_arrowButton, XmNarrowDirection, &arrowDir, NULL);

  if ( (arrowDir == XmARROW_DOWN)  && !showFolder)
	XtVaSetValues(m_arrowButton, XmNarrowDirection, XmARROW_RIGHT, NULL);
  else if ( (arrowDir == XmARROW_RIGHT) && showFolder )
	XtVaSetValues(m_arrowButton, XmNarrowDirection, XmARROW_DOWN, NULL);
}


void
XFE_MNBanner::setShowFolder(XP_Bool show)
{
  unsigned char arrowDir;

  m_isFolderShown = show;
  if (!m_arrowButton )
  {
		m_arrowButton =   XtVaCreateManagedWidget("arrowb",
                                     xmArrowButtonWidgetClass,
                                     m_form,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNtopAttachment, XmATTACH_FORM,
                                     XmNbottomAttachment, XmATTACH_FORM,
                                     XmNrightAttachment, XmATTACH_NONE,
                                     XmNarrowDirection, (show? XmARROW_DOWN : XmARROW_RIGHT),
                                     XmNshadowThickness, 0,
                                     NULL);
		XtAddCallback(m_arrowButton, XmNactivateCallback, showFolderBtnCB, this );

		getToplevel()->registerInterest(XFE_ThreePaneView::ShowFolder, this,
			(XFE_FunctionNotification)showFolder_cb);

  }
  else 
  {
     XtVaGetValues(m_arrowButton, XmNarrowDirection, &arrowDir, NULL);

     if ( (arrowDir == XmARROW_DOWN)  && !show)
	XtVaSetValues(m_arrowButton, XmNarrowDirection, XmARROW_RIGHT, NULL);
     else if ( (arrowDir == XmARROW_RIGHT) && show)
	XtVaSetValues(m_arrowButton, XmNarrowDirection, XmARROW_DOWN, NULL);
   }
}

XP_Bool
XFE_MNBanner::isFolderShown()
{
   return m_isFolderShown;
}

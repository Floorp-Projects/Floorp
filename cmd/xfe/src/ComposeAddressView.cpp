/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   ComposeAddressView.cpp -- presents view for addressing and attachment .
   Created: Dora Hsu <dora@netscape.com>, 23-Sept-96.
   */



#include "ComposeAddressView.h"
#include "xfe.h"
#include "msgcom.h"
#include "xp_mem.h"

#include <Xm/Frame.h>


#ifdef DEBUG_dora
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

extern "C" 
Widget fe_MailComposeWin_Create(MWContext* context, Widget parent);


XFE_ComposeAddressView::XFE_ComposeAddressView(XFE_View *parent_view,
			XFE_Component *toplevel_component,
			MSG_Pane *p) : XFE_MNView(toplevel_component, p)
{

  Widget w;
  setParent(parent_view);
  /* Create Widgets */
  w= fe_MailComposeWin_Create((MWContext*)(parent_view->getContext()), 
			parent_view->getBaseWidget());
  setBaseWidget(w);
}

void
XFE_ComposeAddressView::createWidgets(Widget)
{
  XDEBUG( printf("enter XFE_ComposeAddressView::createWidgets()\n");)
  XDEBUG( printf("leave XFE_ComposeAddressView::createWidgets()\n");)
}

Boolean
XFE_ComposeAddressView::isCommandEnabled(CommandType /* command */, void */*calldata*/)
{
XDEBUG(	printf ("in XFE_ComposeAddressView::isCommandEnabled()\n");)

XDEBUG(	printf ("leaving XFE_ComposeAddressView::isCommandEnabled()\n");)
  return False;
}

Boolean
XFE_ComposeAddressView::handlesCommand(CommandType command, void */*calldata*/)
{
XDEBUG(	printf ("in XFE_ComposeAddressView::handlesCommand(%s)\n", Command::getString(command));)
  if (command == xfeCmdGetNewMessages
      || command == xfeCmdAddNewsgroup
      || command == xfeCmdDelete
      || command == xfeCmdViewSecurity
      || command == xfeCmdStopLoading)
    return True;

XDEBUG(	printf ("leaving XFE_ComposeAddressView::handlesCommand(%s), Command::getString(command)\n");)
  return False;
}

void
XFE_ComposeAddressView::doCommand(CommandType command, void *,
								  XFE_CommandInfo*)
{
XDEBUG(	printf ("in XFE_ComposeAddressView::doCommand()\n");)
XDEBUG(	printf ("Do Command: %s \n", Command::getString(command));)

#if 0
  if (command == xfeCmdNewMailMessage)
    {
      XDEBUG(	printf ("XFE_ComposeAddressView::newMailMessage()\n");)
    }
  else if (command == xfeCmdOpenDraft)
    {
      XDEBUG(	printf ("MSG_FolderPane::openDraft()\n");)
    }
  else if (command == xfeCmdAddNewsgroup)
    {
      XDEBUG(	printf ("MSG_FolderPane::addNewsgroup()\n");)
    }
  else if (command == xfeCmdDelete)
    {
      XDEBUG(	printf ("MSG_FolderPane::deleteFolder()\n");)
    }
  else if (command == xfeCmdViewSecurity)
    {
      XDEBUG(	printf ("MSG_FolderPane::viewSecurity()\n");)
    }
  else if (command == xfeCmdStopLoading)
    {
      XDEBUG(	printf ("MSG_FolderPane::stopLoading()\n");)
    }
#endif

XDEBUG(	printf ("leaving XFE_ComposeAddressView::doCommand()\n");)
}

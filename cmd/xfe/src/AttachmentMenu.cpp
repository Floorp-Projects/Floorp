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
   AttachmentMenu.cpp -- class for doing the dynamic attachment menus.
   Created: Chris Toshok <toshok@netscape.com>, 3-Jan-1997.
 */



#include "AttachmentMenu.h"
#include "MsgView.h"
#include "felocale.h"

XFE_AttachmentMenu::XFE_AttachmentMenu(Widget w, XFE_Frame *frame)
{
  m_cascade = w;
  m_parentFrame = frame;

  XtAddCallback(m_cascade, XmNdestroyCallback, destroy_cb, this);
  
  XtVaGetValues(m_cascade,
		XmNsubMenuId, &m_submenu,
		NULL);

  XtVaGetValues(m_submenu,
		XmNnumChildren, &m_firstslot,
		NULL);

  XP_ASSERT(m_submenu);

  frame->registerInterest(XFE_MsgView::messageHasChanged,
			  this,
			  (XFE_FunctionNotification)attachmentsHaveChanged_cb);

  m_attachmentData = NULL;

  // make sure we initially install an update callback
  attachmentsHaveChanged(NULL, NULL, NULL);
}

XFE_AttachmentMenu::~XFE_AttachmentMenu()
{
  m_parentFrame->unregisterInterest(XFE_MsgView::messageHasChanged,
				    this,
				    (XFE_FunctionNotification)attachmentsHaveChanged_cb);
}

void
XFE_AttachmentMenu::generate(Widget cascade, XtPointer, XFE_Frame *frame)
{
  XFE_AttachmentMenu *obj;

  obj = new XFE_AttachmentMenu(cascade, frame);
}

void
XFE_AttachmentMenu::activate_cb(Widget widget, XtPointer cd, XtPointer)
{
  XFE_AttachmentMenu *obj = (XFE_AttachmentMenu*)cd;
  MSG_AttachmentData *data;

  XtVaGetValues(widget, XmNuserData, &data, 0);
  
  XP_ASSERT(data);

  if (obj->m_parentFrame->handlesCommand(xfeCmdOpenUrl, (void*)data->url))
    obj->m_parentFrame->doCommand(xfeCmdOpenUrl, (void*)data->url);
}

void
XFE_AttachmentMenu::add_attachment_menu_items(Widget menu)
{
  const MSG_AttachmentData *data;
  Arg av [20];
  int ac;
  XmString xmname;

  if (!m_attachmentData)
    return;

  for (data = m_attachmentData;
       data != NULL;
       data ++)
    {
      char buf[1000];
      char *name, *description;
      Widget button;

      name = data->real_name;
      if (name == NULL) name = "";
      
      description = data->description;
      if (description == NULL) description = "";

      PR_snprintf(buf, sizeof(buf), "%s (%s)", name, description);

      xmname = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);

      ac = 0;
      XtSetArg (av[ac], XmNlabelString, xmname); ac++;
      XtSetArg (av[ac], XmNuserData, data); ac++;      

      button = XmCreatePushButtonGadget(menu, "openAttachment", av, ac);
      
      XtAddCallback(button, XmNactivateCallback, activate_cb, this);
      XtManageChild(button);

      XmStringFree(xmname);
    }
}

void
XFE_AttachmentMenu::update()
{
  Widget *kids;
  int nkids;

  XtVaGetValues (m_submenu, XmNchildren, &kids, XmNnumChildren, &nkids, 0);

  XtUnrealizeWidget(m_submenu);

  if (nkids) 
    {
      kids = &(kids[m_firstslot]);
      nkids -= m_firstslot;

      XtUnmanageChildren (kids, nkids);
      fe_DestroyWidgetTree(kids, nkids);
    }

  add_attachment_menu_items(m_submenu);

  XtRealizeWidget(m_submenu);

  XtRemoveCallback(m_cascade, XmNcascadingCallback, update_cb, this);
}

void
XFE_AttachmentMenu::update_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_AttachmentMenu *obj = (XFE_AttachmentMenu*)clientData;

  obj->update();
}

void
XFE_AttachmentMenu::destroy_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_AttachmentMenu *obj = (XFE_AttachmentMenu*)clientData;

  delete obj;
}

XFE_CALLBACK_DEFN(XFE_AttachmentMenu, attachmentsHaveChanged)(XFE_NotificationCenter *,
							      void *,
							      void */*calldata*/)
{
#if 0
  if (calldata)
    m_attachmentData = MSG_GetAttachmentList((MSG_Pane*)calldata);

  // This may seem stupid, but it keeps us from having more than
  // one reference to this particular callback without having
  // to worry about other cascadingCallbacks.

  // remove it if it's already there
  XtRemoveCallback(m_cascade, XmNcascadingCallback, update_cb, this);

  // and then add it back.
  XtAddCallback(m_cascade, XmNcascadingCallback, update_cb, this);
#endif
}

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
   Dialog.cpp -- implementation for Dialogs that can contain views.
   Created: Chris Toshok <toshok@netscape.com>, 16-Oct-96.
 */



#include "ViewDialog.h"
#include "xpassert.h"
#include "structs.h"
#include "xfe.h"

XFE_ViewDialog::XFE_ViewDialog(XFE_View *view, Widget parent,
							   char *name, 
							   MWContext *context,
							   Boolean ok, Boolean cancel,
							   Boolean help, Boolean apply,
							   Boolean separator, Boolean modal,
							   Widget chrome_widget)
  : XFE_Dialog(parent, name, ok, cancel, help, apply, separator, modal, chrome_widget),
  m_okToDestroy(TRUE)
{
  m_view = view;
  m_context = context;

  if (m_view)
    m_view->show();

  if (cancel)
    XtAddCallback(m_chrome, XmNcancelCallback, cancel_cb, this);

  if (ok)
    XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);

  if (help)
    XtAddCallback(m_chrome, XmNhelpCallback, help_cb, this);

  if (apply)
    XtAddCallback(m_chrome, XmNapplyCallback, apply_cb, this);
}

XFE_ViewDialog::~XFE_ViewDialog()
{
  if (m_view)
    delete m_view;
}

void
XFE_ViewDialog::setView(XFE_View *view)
{
  m_view = view;
  if (m_view)
    m_view->show();
}

void
XFE_ViewDialog::cancel()
{
  CommandType cancel_cmd = xfeCmdDialogCancel;
  XP_ASSERT(m_view);
 
  if (!m_view->handlesCommand(cancel_cmd))
    {
      XBell(XtDisplay(m_widget), 100);
      return;
    }
 
  if (m_view->isCommandEnabled(cancel_cmd))
    {
      m_view->doCommand(cancel_cmd);
    }
}

void
XFE_ViewDialog::help()
{
  CommandType help_cmd = xfeCmdDialogHelp;
  XP_ASSERT(m_view);

  if (!m_view->handlesCommand(help_cmd))
    {
      XBell(XtDisplay(m_widget), 100);
      return;
    }

  if (m_view->isCommandEnabled(help_cmd))
    {
      m_view->doCommand(help_cmd);
    }
}

void
XFE_ViewDialog::apply()
{
  CommandType apply_cmd = xfeCmdDialogApply;
  XP_ASSERT(m_view);

  if (!m_view->handlesCommand(apply_cmd))
    {
      XBell(XtDisplay(m_widget), 100);
      return;
    }

  if (m_view->isCommandEnabled(apply_cmd))
    {
      m_view->doCommand(apply_cmd);
    }
}

void
XFE_ViewDialog::ok()
{
  CommandType ok_cmd = xfeCmdDialogOk;

  XP_ASSERT(m_view);

  if (!m_view->handlesCommand(ok_cmd))
    {
      XBell(XtDisplay(m_widget), 100);
      return;
    }

  if (m_view->isCommandEnabled(ok_cmd))
    {
      m_view->doCommand(ok_cmd);
    }
}

void
XFE_ViewDialog::cancel_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_ViewDialog *obj = (XFE_ViewDialog*)clientData;

  obj->cancel();

  XtDestroyWidget(obj->getBaseWidget());
}

void
XFE_ViewDialog::help_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_ViewDialog *obj = (XFE_ViewDialog*)clientData;

  obj->help();
}

void
XFE_ViewDialog::apply_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_ViewDialog *obj = (XFE_ViewDialog*)clientData;

  obj->apply();
}

void
XFE_ViewDialog::ok_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_ViewDialog *obj = (XFE_ViewDialog*)clientData;

  obj->ok();

  if (obj->m_okToDestroy)
	  XtDestroyWidget(obj->getBaseWidget());
}

Pixel
XFE_ViewDialog::getFGPixel()
{
  return CONTEXT_DATA(m_context)->fg_pixel;
}

Pixel
XFE_ViewDialog::getBGPixel()
{
  return CONTEXT_DATA(m_context)->default_bg_pixel;
}

Pixel
XFE_ViewDialog::getTopShadowPixel()
{
  return CONTEXT_DATA(m_context)->top_shadow_pixel;
}

Pixel
XFE_ViewDialog::getBottomShadowPixel()
{
  return CONTEXT_DATA(m_context)->bottom_shadow_pixel;
}

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
   Dialog.h -- class definitions for FE dialog windows that can contain views.
   Created: Chris Toshok <toshok@netscape.com>, 16-Oct-96.
 */

#ifndef _xfe_viewdialog_h
#define _xfe_viewdialog_h

#include "xp_core.h"

#include "Dialog.h"
#include "View.h"

class XFE_ViewDialog : public XFE_Dialog

{
public:
  XFE_ViewDialog(XFE_View *view,
				 Widget parent,
				 char *name,
				 MWContext *context,
				 Boolean ok = TRUE,
				 Boolean  cancel = TRUE,
				 Boolean  help = TRUE,  
				 Boolean  apply = TRUE, 
				 Boolean  separator = TRUE,
				 Boolean  modal = TRUE,
				 Widget chrome_widget = NULL);

  virtual ~XFE_ViewDialog();

  virtual Pixel getFGPixel();
  virtual Pixel getBGPixel();
  virtual Pixel getTopShadowPixel();
  virtual Pixel getBottomShadowPixel();

protected:

  void setView(XFE_View *m_view);

  virtual void cancel();
  virtual void help();
  virtual void apply();
  virtual void ok();

  static void cancel_cb(Widget, XtPointer, XtPointer);
  static void help_cb(Widget, XtPointer, XtPointer);
  static void apply_cb(Widget, XtPointer, XtPointer);
  static void ok_cb(Widget, XtPointer, XtPointer);

  XFE_View *m_view;
  MWContext *m_context;

  Boolean m_okToDestroy;
};

#endif /* _xfe_viewdialog_h */

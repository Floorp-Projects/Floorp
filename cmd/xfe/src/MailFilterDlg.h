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
   MailFilterDlg.h -- class definition for XFE_MailFilterDlg
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */



#ifndef _MAILFILTERDLG_H_
#define _MAILFILTERDLG_H_

#include "ViewDialog.h"

//
class XFE_MailFilterDlg: public XFE_ViewDialog 
{

public:

  XFE_MailFilterDlg(Widget           parent,
		      char            *name,
		      Boolean          modal,
		      MWContext *context);

  virtual ~XFE_MailFilterDlg();

  // Notification callback
  XFE_CALLBACK_DECL(listChanged)


protected:
  virtual void cancel();
  virtual void ok();

private:

};

extern "C" void fe_showMailFilterDlg(Widget toplevel, MWContext *context);

#endif /* _MAILFILTERDLG_H_ */

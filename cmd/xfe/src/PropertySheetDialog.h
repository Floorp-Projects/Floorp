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
   PropertySheetDialog.h -- class definition for PropertySheetDialog
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */



#ifndef _xfe_xmlfolderdialog_h
#define _xfe_xmlfolderdialog_h

#include "ViewDialog.h"

// This is a general wrapper of dialog with a property sheet widget
class XFE_PropertySheetDialog: public XFE_ViewDialog {
public:
  XFE_PropertySheetDialog(XFE_View *view, /* the parent view */
		      Widget    parent,
		      char     *name,
		      MWContext *context,
		      Boolean   ok = TRUE,
		      Boolean   cancel = TRUE,
		      Boolean   help = TRUE,  
		      Boolean   apply = TRUE, 
		      Boolean   separator = TRUE,
		      Boolean   modal = TRUE);

  virtual ~XFE_PropertySheetDialog();

  virtual void setDlgValues();
  virtual void getDlgValues();

  typedef enum {
	  eWAITING = 0, eCANCEL, eAPPLY, eOTHERS
  } ANS_t;

  // 
  void    setClientData(ANS_t *data) {m_clientData = data;}

protected:
  virtual void cancel();
  virtual void apply();
  virtual void ok();

  ANS_t *m_clientData;

private:


}; /* XFE_PropertySheetDialog */

#endif /* _xfe_xmlfolderdialog_h */

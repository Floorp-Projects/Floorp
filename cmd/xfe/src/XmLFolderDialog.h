/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* -*- Mode: C++; tab-width: 4 -*-
   XmLFolderDialog.h -- class definition for XmLFolderDialog
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#ifndef _xfe_xmlfolderdialog_h
#define _xfe_xmlfolderdialog_h

#include "ViewDialog.h"

// This is a general wrapper of dialog with XmLFloder widget
class XFE_XmLFolderDialog: public XFE_ViewDialog {
public:
  XFE_XmLFolderDialog(XFE_View *view, /* the parent view */
		      Widget    parent,
		      char     *name,
		      MWContext *context,
		      Boolean   ok = TRUE,
		      Boolean   cancel = TRUE,
		      Boolean   help = TRUE,  
		      Boolean   apply = TRUE, 
		      Boolean   separator = TRUE,
		      Boolean   modal = TRUE);

  virtual ~XFE_XmLFolderDialog();

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


}; /* XFE_XmLFolderDialog */

#endif /* _xfe_xmlfolderdialog_h */

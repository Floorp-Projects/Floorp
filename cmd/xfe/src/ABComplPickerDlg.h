/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* -*- Mode: C++; tab-width: 4 -*-
   Created: Tao Cheng <tao@netscape.com>, 16-mar-98
 */

#ifndef _ABCOMPLPICKERDLG_H_
#define _ABCOMPLPICKERDLG_H_

#if defined(GLUE_COMPO_CONTEXT)
#include "ViewDashBDlg.h"
#else
#include "ViewDialog.h"
#endif /* GLUE_COMPO_CONTEXT */

typedef void (*NCPickerExitFunc)(void *clientData, void *callData);
//
#if defined(GLUE_COMPO_CONTEXT)
class XFE_ABComplPickerDlg: public XFE_ViewDashBDlg
#else
class XFE_ABComplPickerDlg: public XFE_ViewDialog 
#endif /* GLUE_COMPO_CONTEXT */
{

public:

  XFE_ABComplPickerDlg(Widget     parent,
					   char      *name,
					   Boolean    modal,
					   MWContext *context,
					   MSG_Pane  *pane,
					   MWContext *pickerContext,
					   NCPickerExitFunc func,
					   void      *callData);

  virtual ~XFE_ABComplPickerDlg();

  void selectItem(int);

  static MWContext* cloneCntxtNcreatePane(MWContext  *context, 
										  MSG_Pane  **pane);

  XFE_CALLBACK_DECL(confirmed)

protected:
  virtual void cancel();
  virtual void ok();

  NCPickerExitFunc  m_func;
  void             *m_cData;
private:

  MWContext *m_pickerContext;
};

#endif /* _ABCOMPLPICKERDLG_H_ */

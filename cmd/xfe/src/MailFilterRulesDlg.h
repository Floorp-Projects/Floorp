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
/* 
   MailFilterRulesDlg.h -- class definition for XFE_MailFilterDlg
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */



#ifndef _MAILFILTERRULESDLG_H_
#define _MAILFILTERRULESDLG_H_

#include "ViewDialog.h"
#include "MailFilterRulesView.h"

//
class XFE_MailFilterRulesDlg: public XFE_ViewDialog 
{

public:

  XFE_MailFilterRulesDlg(XFE_Component *subscriber,
						 Widget     parent,
						 char      *name,
						 Boolean    modal,
						 MWContext *context,
						 XP_Bool    isNew);

	virtual ~XFE_MailFilterRulesDlg();

	void setDlgValues(MSG_FilterList *list, MSG_FilterIndex at) {
		((XFE_MailFilterRulesView *)m_view)->setDlgValues(list, at);
	}

protected:
  virtual void cancel();
  virtual void ok();

private:
	XFE_Component *m_subscriber;
};

#endif /* _MAILFILTERRULESDLG_H_ */

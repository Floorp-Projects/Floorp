/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   AddrBookView.h -- class definition for XFE_AddrBookView
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
   Revised: Tao Cheng <tao@netscape.com>, 01-nov-96
 */

#ifndef _xfe_addrbookview_h
#define _xfe_addrbookview_h

#include "ABListSearchView.h"

#include "addrbook.h"

class XFE_AddrBookFrame;
class XFE_ABNameFolderDlg;
class XFE_ABMListDlg;


class XFE_AddrBookView : public XFE_ABListSearchView
{
public:
  XFE_AddrBookView(XFE_Component *toplevel_component, 
				   Widget         parent, 
				   XFE_View      *parent_view, 
				   MWContext     *context,
				   XP_List       *directories);
  virtual ~XFE_AddrBookView();

  void abCall();
  void abVCard();

  // tooltips and doc string
  virtual char *getDocString(CommandType cmd);

  virtual void     doubleClickBody(const OutlineButtonFuncData *data);

protected:
  /* Address Book specific
   */
  virtual Boolean isCommandSelected(CommandType cmd, 
									void *calldata = NULL,
									XFE_CommandInfo* i = NULL);
private:
};
#endif /* _xfe_addrbookview_h */

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
   EditorBar.h
   Created: Richard Hess <rhess@netscape.com>, 10-Dec-96.
   */



#ifndef _xfe_editorbar_h
#define _xfe_editorbar_h

#include "structs.h"
#include "Component.h"

class XFE_EditorBar : public XFE_Component
{
public:
  XFE_EditorBar(XFE_Component *toplevel_component,
	     Widget parent, MWContext *context);

  virtual ~XFE_EditorBar();

private:
  Widget m_eToolbar;
};

#endif /* _xfe_editorbar_h */

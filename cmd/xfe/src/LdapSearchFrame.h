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
   LdapSearchFrame.h -- class definitions for mail/news search frames.
   Created: Dora Hsu <dora@netscape.com>, 15-Dec-96.
*/

#ifndef _xfe_mailsearchframe_h
#define _xfe_mailsearchframe_h

#include "Frame.h"
#include "xp_core.h"
#include <Xm/Xm.h>

class XFE_LdapSearchFrame : public XFE_Frame
{
public:
  XFE_LdapSearchFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
  virtual ~XFE_LdapSearchFrame();
  // For close btn
  XFE_CALLBACK_DECL(close)
};

extern "C" MWContext* fe_showLdapSearch(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
#endif /* _xfe_mailsearchframe_h */

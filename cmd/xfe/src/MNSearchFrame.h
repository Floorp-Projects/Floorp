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
   MNSearchFrame.h -- class definitions for mail/news search frames.
   Created: Dora Hsu <dora@netscape.com>, 15-Dec-96.
*/



#ifndef _xfe_mnsearchframe_h
#define _xfe_mnsearchframe_h

#include "Frame.h"
#include "xp_core.h"
#include <Xm/Xm.h>
#include "MNView.h"

class XFE_MNSearchFrame : public XFE_Frame
{
public:
  XFE_MNSearchFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, XFE_MNView *mnview, 
		MSG_FolderInfo* folderInfo = NULL);
  virtual ~XFE_MNSearchFrame();

  void	setFolderOption(MSG_FolderInfo *folderInfo);
  // For close btn
  XFE_CALLBACK_DECL(close)
};

extern "C" MWContext* fe_showMNSearch(Widget toplevel, XFE_Frame *parent_frame,
									  Chrome *chromespec, XFE_MNView *mnview, 
									  MSG_FolderInfo* folderInfo);

#endif /* _xfe_mailsearchframe_h */

/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   ViewGlue.h -- glue to go from views to MWContexts, and back again.
   Created: Chris Toshok <toshok@netscape.com>, 25-Sep-96
   */



#ifndef _xfe_viewglue_h
#define _xfe_viewglue_h

#include "Frame.h"

/* This should go away once MWContexts go away, but we're still stuck in that
   world... */

extern void ViewGlue_addMapping(XFE_Frame *frame, void *context);
extern XFE_Frame *ViewGlue_getFrame(void *context);

#endif /* _xfe_viewglue_h */

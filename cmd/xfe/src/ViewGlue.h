/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   ViewGlue.h -- glue to go from views to MWContext, and back again.
   Created: Chris Toshok <toshok@netscape.com>, 25-Sep-96
   */



#ifndef _xfe_viewglue_h
#define _xfe_viewglue_h

#include "Frame.h"

/* This should go away once MWContexts go away, but we're still stuck in that
   world... */

extern void ViewGlue_addMapping(XFE_Frame *frame, void *context);
extern XFE_Frame *ViewGlue_getFrame(void *context);

/* for XFE_Component
 */
extern void ViewGlue_addMappingForCompo(XFE_Component *compo, void *context);
extern XFE_Component *ViewGlue_getCompo(void *context);

#endif /* _xfe_viewglue_h */

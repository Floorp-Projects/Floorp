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


/* xp_thrmo.h --- Status message text for the thermometer. */

#ifndef _XP_THRMO_
#define _XP_THRMO_

#include "xp_core.h"

XP_BEGIN_PROTOS
extern const char *
XP_ProgressText (unsigned long total_bytes,
		 unsigned long bytes_received,
		 unsigned long start_time_secs,
		 unsigned long now_secs);
XP_END_PROTOS

#endif /* _XP_THRMO_ */

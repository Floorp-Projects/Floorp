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

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*   stubsec.c --- stub fe handling of FE security related stuff. */

#include "structs.h"
#include "ntypes.h"
#include "xpassert.h"
#include "proto.h"
#include "fe_proto.h"

Bool
FE_SecurityDialog(MWContext* context,
		  int message,
		  XP_Bool* prefs_toggle)
{
}

void
FE_SecurityOptionsChanged(MWContext *context)
{
}

void
FE_SetPasswordEnabled(MWContext *context, PRBool usePW)
{
}

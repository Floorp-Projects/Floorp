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

/* 
   stubps.c --- stub functions for fe
                specific ps printing stuff.
*/

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"

/* Get the dimensions of an icon in pixels for the PostScript front end. */
extern void
FE_GetPSIconDimensions(int icon_number,
		       int *width,
		       int *height)
{
}

/* Fill in the bits of an icon for the PostScript front end. */
extern XP_Bool
FE_GetPSIconData(int icon_number,
		 IL_Pixmap *image,
		 IL_Pixmap *mask)
{
}

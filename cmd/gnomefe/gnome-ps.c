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
   gnomeps.c --- gnome functions for fe
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
  printf("FE_GetPSIconDimensions (empty)\n");
}

/* Fill in the bits of an icon for the PostScript front end. */
extern XP_Bool
FE_GetPSIconData(int icon_number,
		 IL_Pixmap *image,
		 IL_Pixmap *mask)
{
  printf("FE_GetPSIconData (empty)\n");
}

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
  xp_rgb.c --- parsing color names to RGB triplets.
  Created: John Giannandrea <jg@netscape.com>, 28-Sep-95
*/

#include "xp.h"

XP_BEGIN_PROTOS

/* Looks up the passed name via a caseless compare in a static table
   of color names.  If found it sets the passed pointers to contain
   the red, green, and blue values for that color.  On success the return
   code is 0.  Returns 1 if no match is found for the color.
 */
extern intn XP_ColorNameToRGB(char *name, uint8 *r, uint8 *g, uint8 *b);

XP_END_PROTOS

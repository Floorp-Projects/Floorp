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

#include "xp.h"
#include "net.h"
#include "pa_parse.h"
#include "layout.h"
#include "laylayer.h"
#include "layers.h"
#include "laytrav.h"

extern int MK_OUT_OF_MEMORY;

#ifdef PROFILE
#pragma profile on
#endif

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */

#ifdef XP_WIN16
#define SIZE_LIMIT		32000
#endif /* XP_WIN16 */

/*	Right now we are just using the line list and not worrying about how to jump out of cells.
 *	Instead of using bina's lo_JumpCellWall(), I think we should store a ptr to the parent cell
 *	in each layout element when we create it.  Would make life much easier and faster.
 */

LO_Element * lo_tv_GetFirstLayoutElement( lo_DocState *state )
{
	LO_Element **	line_array;
	
	XP_LOCK_BLOCK ( line_array, LO_Element **, state->line_array );
	return line_array[0];
}

LO_Element * lo_tv_GetNextLayoutElement( lo_DocState *state, LO_Element *curr_ele, Bool skipContainers )
{
	LO_Element *next;

	next = curr_ele->lo_any.next;
	if (skipContainers == FALSE && curr_ele->lo_any.type == LO_CELL) {
		next = curr_ele->lo_cell.cell_list;
	}

	return next;
}

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

#ifndef _Laytrav_h_
#define _Laytrav_h_

#include "xp_core.h"

/*	I wanted to define the following API to traverse the line list.
 *	What do you guys think?  This might come in handy later when
 *  we want to traverse the different line lists in the layer tree.
 *  The logic for figuring out the next layer to go to will all be in one
 *  place: laytrav.c.  
 */

/* API for traversing layout element list by visiting the nodes of the layer tree. */

/* You want to be able to go through only visible layers */
/* You want the option to drill down into containers or not. */

LO_Element * lo_tv_GetFirstLayoutElement( lo_DocState *state );
LO_Element * lo_tv_GetNextLayoutElement( lo_DocState *state, LO_Element *curr_ele, Bool skipContainers );
LO_Element * lo_tv_GetPrevLayoutElement( lo_DocState *state, LO_Element *curr_ele );
LO_Element * lo_tv_FindNextElementOfType( lo_DocState *state, LO_Element *curr_ele, uint32 ele_type );
LO_Element * lo_tv_FindPrevElementOfType( lo_DocState *state, LO_Element *curr_ele, uint32 ele_type );

Bool lo_tv_IsAContainer( LO_Element *curr_ele );

#endif

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   layrelay.h --- reflow of layout elements.
   Created: Nisheeth Ranjan <nisheeth@netscape.com>, 7-Nov-1997.
 */
#ifndef _Layrelay_h_
#define _Layrelay_h_

#include "xp_core.h"

/* Relayout State */
typedef struct _RelayoutState {	
	MWContext *context;
	lo_TopState * top_state;
	lo_DocState * doc_state;
} lo_RelayoutState;

/* Fitting functions */

/* State management functions */
extern lo_RelayoutState * lo_rl_CreateRelayoutState( void );
extern void lo_rl_DestroyRelayoutState(lo_RelayoutState *relay_state );
extern lo_RelayoutState *lo_rl_InitRelayoutState(MWContext *context,
						 lo_RelayoutState *blank_state,
						 int32 newWidth, int32 newHeight,
						 int32 margin_width, int32 margin_height );
/* This function actually lives in laygrid.c */
extern void lo_RelayoutGridDocumentOnResize(MWContext *context,
					    lo_TopState *top_state,
					    int32 width, int32 height);

#endif

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
/* 
   xploele.h --- an API for accessing LO_Element data.
   Created: Chris Toshok <toshok@netscape.com>, 6-Nov-1997.
 */


#ifndef _xp_loele_h
#define _xp_loele_h

#include "xp_mem.h"
#include "xp_core.h"
#include "ntypes.h"
#include "lo_ele.h"

XP_BEGIN_PROTOS

void*		XPLO_GetElementFEData(LO_Element *element);
void		XPLO_SetElementFEData(LO_Element *element, void* fe_data);

int16		XPLO_GetElementType(LO_Element *element);
int32		XPLO_GetElementID(LO_Element *element);
int16		XPLO_GetElementXOffset(LO_Element *element);
void		XPLO_GetElementRect(LO_Element *element,
								int32 *x,
								int32 *y,
								int32 *width,
								int32 *height);
int32		XPLO_GetElementLineHeight(LO_Element *element);

LO_Element* XPLO_GetPrevElement(LO_Element *element);
LO_Element* XPLO_GetNextElement(LO_Element *element);

ED_Element* XPLO_GetEditElement(LO_Element *element);
int32		XPLO_GetEditOffset(LO_Element *element);

LO_AnchorData*	XPLO_GetAnchorData(LO_Element *element);

XP_END_PROTOS

#endif /* _xp_loele_h */

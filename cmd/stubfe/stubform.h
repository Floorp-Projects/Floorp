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

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"

XP_BEGIN_PROTOS

extern void STUBFE_GetFormElementInfo (MWContext * context, LO_FormElementStruct * form_element);
extern void STUBFE_GetFormElementValue (MWContext * context, LO_FormElementStruct * form_element,
					XP_Bool hide);
extern void STUBFE_ResetFormElement (MWContext * context, LO_FormElementStruct * form_element);
extern void STUBFE_SetFormElementToggle (MWContext * context, LO_FormElementStruct * form_element,
					 XP_Bool toggle);

XP_END_PROTOS

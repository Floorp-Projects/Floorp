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
/**********************************************************************
 e_kit.h
 By Daniel Malmer
**********************************************************************/

#ifndef __e_kit_h
#define __e_kit_h

#ifdef	__cplusplus
extern "C" {
#endif

#include <X11/Intrinsic.h>

struct MWContext_;

extern void ekit_Initialize(Widget toplevel);

extern char* ekit_AnimationFile(void);
extern char* ekit_LogoUrl(void);
extern char* ekit_HomePage(void);
extern char* ekit_UserAgent(void);
extern char* ekit_AutoconfigUrl(void);

extern char* ekit_AboutData(void);
extern Boolean ekit_Enabled(void);
extern void ekit_LoadCustomUrl(char* prefix, struct MWContext_* context);
extern void ekit_LoadCustomAnimation(void);
extern Boolean ekit_CustomAnimation(void);

#ifdef	__cplusplus
}
#endif

#endif

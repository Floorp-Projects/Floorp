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

#ifndef _CL_TYPES_H
#define _CL_TYPES_H

/* Compositor types */
typedef struct CL_Compositor CL_Compositor;
typedef struct CL_Layer CL_Layer;
typedef struct CL_Event CL_Event;
typedef struct CL_Drawable CL_Drawable;

/* This is an opaque data type that corresponds to a
   platform-specific region data type.               */
typedef void *FE_Region;
typedef struct _XP_Rect XP_Rect;

typedef enum 
{
    CL_OFFSCREEN_DISABLED,
    CL_OFFSCREEN_ENABLED,
    CL_OFFSCREEN_AUTO
} CL_OffscreenMode;

#endif /* _CL_TYPES_H */

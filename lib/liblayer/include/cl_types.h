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

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

/* 
   stubimg.c --- stub functions for fe
                 specific images stuff.
*/

#define JMC_INIT_IMGCB_ID

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"

#include "libimg.h"
#include "il_util.h"
#include "prtypes.h"

JMC_PUBLIC_API(void)
_IMGCB_init(struct IMGCB* self, JMCException* *exception)
{
}

JMC_PUBLIC_API(void*)
_IMGCB_getBackwardCompatibleInterface(struct IMGCB* self,
				      const JMCInterfaceID* iid,
				      JMCException* *exception)
{
}

JMC_PUBLIC_API(void)
_IMGCB_NewPixmap(IMGCB* img_cb, jint op, void *dpy_cx, jint width, jint height,
		 IL_Pixmap *image, IL_Pixmap *mask) 
{
}

JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap,
		    jint x_offset, jint y_offset, jint width, jint height)
{
}

JMC_PUBLIC_API(void)
_IMGCB_ControlPixmapBits(IMGCB* img_cb, jint op, void* dpy_cx,
                         IL_Pixmap* pixmap, IL_PixmapControl message)
{
}

JMC_PUBLIC_API(void)
_IMGCB_DestroyPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* pixmap)
{
}

JMC_PUBLIC_API(void)
_IMGCB_DisplayPixmap(IMGCB* img_cb, jint op, void* dpy_cx, IL_Pixmap* image, 
                     IL_Pixmap* mask, jint x, jint y, jint x_offset,
                     jint y_offset, jint width, jint height)
{
}

JMC_PUBLIC_API(void)
_IMGCB_GetIconDimensions(IMGCB* img_cb, jint op, void* dpy_cx, int* width,
                         int* height, jint icon_number)
{
}
JMC_PUBLIC_API(void)
_IMGCB_DisplayIcon(IMGCB* img_cb, jint op, void* dpy_cx, jint x, jint y,
                   jint icon_number)
{
}

/* Mocha image group observer callback. */
void
FE_MochaImageGroupObserver(XP_Observable observable, XP_ObservableMsg message,
                           void *message_data, void *closure)
{   
}   

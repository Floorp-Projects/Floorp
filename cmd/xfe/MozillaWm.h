/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * MozillaWm.h
 *
 * Mozilla Window Manager interface
 *
 * X11 window manager conventions do not support the window manager
 * functionality required by Mozilla JavaScript 1.2 features for NetCaster.
 */

#ifndef _MOZILLA_WM_H
#define _MOZILLA_WM_H

#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IMPORTANT - remove this #define when we get a window manager which supports API */
#define MOZILLA_WM_I_AM_A_WINDOW_MANAGER


/* Window Manager-only API */
#ifdef MOZILLA_WM_I_AM_A_WINDOW_MANAGER
/* set version of Mozilla WM API supported by window manager. */
void MozillaWmSetWmVersion(Screen*,long);
#endif

/* general client API */

/* get version of Mozilla WM API supported by window manager for client's screen */
long MozillaWmGetWmVersion(Screen*);

/* get/set Mozilla WM hint flags on application shell window */
void MozillaWmSetHints(Screen*,Window,unsigned long);
void MozillaWmAddHints(Screen*,Window,unsigned long);
void MozillaWmRemoveHints(Screen*,Window,unsigned long);
unsigned long MozillaWmGetHints(Screen*,Window);

/* hint masks */

#define MOZILLA_WM_NONE				(0)
#define MOZILLA_WM_ALWAYS_BOTTOM	(1 << 0)
#define MOZILLA_WM_ALWAYS_TOP		(1 << 1)
#define MOZILLA_WM_ZORDER_LOCK		(1 << 2)

#define MOZILLA_WM_VERSION_INVALID -1

#ifdef __cplusplus
}
#endif

#endif /* _MOZILLA_WM_H */


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

/* MozillaWm.c
 *
 * Mozilla Window Manager interface
 *
 * X11 window manager conventions do not support the window manager
 * functionality required by Mozilla JavaScript 1.2 features for NetCaster.
 */

#include "MozillaWm.h"

#define _MOZILLA_WM_VERSION "_MOZILLA_WM_VERSION"
#define _MOZILLA_WM_HINTS "_MOZILLA_WM_HINTS"

/* Window Manager-only API */
#ifdef MOZILLA_WM_I_AM_A_WINDOW_MANAGER
/* set version of Mozilla WM API supported by window manager. */
void
MozillaWmSetWmVersion(Screen *screen,long wmVersion)
{
    Display *display=XDisplayOfScreen(screen);
    Window root=XRootWindowOfScreen(screen);
    Atom XA_MOZILLA_WM_VERSION=XInternAtom(display,_MOZILLA_WM_VERSION,False);
    
    XChangeProperty(display,
                    root,
                    XA_MOZILLA_WM_VERSION,
                    XA_MOZILLA_WM_VERSION,
                    32,
                    PropModeReplace,
                    (unsigned char *) &wmVersion,
                    1);
}
#endif

/* general client API */

/* get version of Mozilla WM API supported by window manager for client's screen */
long
MozillaWmGetWmVersion(Screen *screen)
{
    Display *display=XDisplayOfScreen(screen);
    Window root=XRootWindowOfScreen(screen);
    Atom XA_MOZILLA_WM_VERSION=XInternAtom(display,_MOZILLA_WM_VERSION,False);
    int retValue;
    unsigned long outlength;
    unsigned char *outpointer=NULL;
    Atom type;
    int format;
    unsigned long bytes_left;

    long wmVersion=MOZILLA_WM_VERSION_INVALID;/* default to invalid version */

    retValue=XGetWindowProperty(display,
                                root,
                                XA_MOZILLA_WM_VERSION,
                                0,
                                10000000,
                                False,
                                AnyPropertyType,
                                &type,
                                &format,
                                &outlength,
                                &bytes_left,
                                &outpointer);
    if (retValue == 0 &&
        outpointer != NULL &&
        outlength > 0 &&
        format == 32) {
        /* extract version number from property data */
        wmVersion=((long*)outpointer)[0];
    }
    if (outpointer != NULL)
        XFree((char*)outpointer);

    return wmVersion;
}


/* set Mozilla WM hint flags on shell window */
void MozillaWmSetHints(Screen *screen, Window window, unsigned long hints)
{
    Display *display=XDisplayOfScreen(screen);
    Atom XA_MOZILLA_WM_HINTS=XInternAtom(display,_MOZILLA_WM_HINTS,False);

    XChangeProperty(display,
                    window,
                    XA_MOZILLA_WM_HINTS,
                    XA_MOZILLA_WM_HINTS,
                    32,
                    PropModeReplace,
                    (unsigned char *) &hints,
                    1);

}

/* add Mozilla WM hint flags to shell window */
void MozillaWmAddHints(Screen *screen, Window window, unsigned long hints)
{
    unsigned long wmHints=MozillaWmGetHints(screen,window);
    wmHints |= hints;
    MozillaWmSetHints(screen,window,wmHints);
}

/* remove Mozilla WM hint flags from shell window */
void MozillaWmRemoveHints(Screen *screen, Window window, unsigned long hints)
{
    unsigned long wmHints=MozillaWmGetHints(screen,window);
    wmHints &= ~hints;
    MozillaWmSetHints(screen,window,wmHints);
}

/* get Mozilla WM hint flags from shell window */
unsigned long MozillaWmGetHints(Screen *screen, Window window)
{
    Display *display=XDisplayOfScreen(screen);
    Atom XA_MOZILLA_WM_HINTS=XInternAtom(display,_MOZILLA_WM_HINTS,False);
    int retValue;
    unsigned long outlength;
    unsigned char *outpointer=NULL;
    Atom type;
    int format;
    unsigned long bytes_left;

    unsigned long wmHints=MOZILLA_WM_NONE; /* default to no flags */

    retValue=XGetWindowProperty(display,
                                window,
                                XA_MOZILLA_WM_HINTS,
                                0,
                                10000000,
                                False,
                                AnyPropertyType,
                                &type,
                                &format,
                                &outlength,
                                &bytes_left,
                                &outpointer);
    if (retValue == 0 &&
        outpointer != NULL &&
        outlength > 0 &&
        format == 32) {
        /* extract flags from property data */
        wmHints=((long*)outpointer)[0];
    }
    if (outpointer != NULL)
        XFree((char*)outpointer);

    return wmHints;
}


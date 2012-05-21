/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X11Util.h"

namespace mozilla {

bool
XVisualIDToInfo(Display* aDisplay, VisualID aVisualID,
                Visual** aVisual, unsigned int* aDepth)
{
    if (aVisualID == None) {
        *aVisual = NULL;
        *aDepth = 0;
        return true;
    }

    const Screen* screen = DefaultScreenOfDisplay(aDisplay);

    for (int d = 0; d < screen->ndepths; d++) {
        Depth *d_info = &screen->depths[d];
        for (int v = 0; v < d_info->nvisuals; v++) {
            Visual* visual = &d_info->visuals[v];
            if (visual->visualid == aVisualID) {
                *aVisual = visual;
                *aDepth = d_info->depth;
                return true;
            }
        }
    }

    NS_ERROR("VisualID not on Screen.");
    return false;
}

ScopedXErrorHandler::ErrorEvent* ScopedXErrorHandler::sXErrorPtr;

int
ScopedXErrorHandler::ErrorHandler(Display *, XErrorEvent *ev)
{
    // only record the error if no error was previously recorded.
    // this means that in case of multiple errors, it's the first error that we report.
    if (!sXErrorPtr->mError.error_code)
      sXErrorPtr->mError = *ev;
    return 0;
}

ScopedXErrorHandler::ScopedXErrorHandler()
{
    // let sXErrorPtr point to this object's mXError object, but don't reset this mXError object!
    // think of the case of nested ScopedXErrorHandler's.
    mOldXErrorPtr = sXErrorPtr;
    sXErrorPtr = &mXError;
    mOldErrorHandler = XSetErrorHandler(ErrorHandler);
}

ScopedXErrorHandler::~ScopedXErrorHandler()
{
    sXErrorPtr = mOldXErrorPtr;
    XSetErrorHandler(mOldErrorHandler);
}

bool
ScopedXErrorHandler::SyncAndGetError(Display *dpy, XErrorEvent *ev)
{
    XSync(dpy, False);
    return GetError(ev);
}

bool
ScopedXErrorHandler::GetError(XErrorEvent *ev)
{
    bool retval = mXError.mError.error_code != 0;
    if (ev)
        *ev = mXError.mError;
    mXError = ErrorEvent(); // reset
    return retval;
}

} // namespace mozilla

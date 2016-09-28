/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X11Util.h"
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "MainThreadUtils.h"            // for NS_IsMainThread

namespace mozilla {

void
FindVisualAndDepth(Display* aDisplay, VisualID aVisualID,
                   Visual** aVisual, int* aDepth)
{
    const Screen* screen = DefaultScreenOfDisplay(aDisplay);

    for (int d = 0; d < screen->ndepths; d++) {
        Depth *d_info = &screen->depths[d];
        for (int v = 0; v < d_info->nvisuals; v++) {
            Visual* visual = &d_info->visuals[v];
            if (visual->visualid == aVisualID) {
                *aVisual = visual;
                *aDepth = d_info->depth;
                return;
            }
        }
    }

    NS_ASSERTION(aVisualID == X11None, "VisualID not on Screen.");
    *aVisual = nullptr;
    *aDepth = 0;
    return;
}

void
FinishX(Display* aDisplay)
{
  unsigned long lastRequest = NextRequest(aDisplay) - 1;
  if (lastRequest == LastKnownRequestProcessed(aDisplay))
    return;

  XSync(aDisplay, False);
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

ScopedXErrorHandler::ScopedXErrorHandler(bool aAllowOffMainThread)
{
    if (!aAllowOffMainThread) {
      // Off main thread usage is not safe in general, but OMTC GL layers uses this
      // with the main thread blocked, which makes it safe.
      NS_WARNING_ASSERTION(
        NS_IsMainThread(),
        "ScopedXErrorHandler being called off main thread, may cause issues");
    }
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
    FinishX(dpy);

    bool retval = mXError.mError.error_code != 0;
    if (ev)
        *ev = mXError.mError;
    mXError = ErrorEvent(); // reset
    return retval;
}

} // namespace mozilla

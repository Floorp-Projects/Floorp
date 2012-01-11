/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

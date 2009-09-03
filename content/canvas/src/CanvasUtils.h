/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef _CANVASUTILS_H_
#define _CANVASUTILS_H_

#include "prtypes.h"

class nsICanvasElement;
class nsIPrincipal;

namespace mozilla {

class CanvasUtils {
public:
    // Check that the rectangle [x,y,w,h] is a subrectangle of [0,0,realWidth,realHeight]

    static PRBool CheckSaneSubrectSize(PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h,
                                       PRInt32 realWidth, PRInt32 realHeight)
    {
        if (w <= 0 || h <= 0 || x < 0 || y < 0)
            return PR_FALSE;

        if (x >= realWidth  || w > (realWidth - x) ||
            y >= realHeight || h > (realHeight - y))
            return PR_FALSE;

        return PR_TRUE;
    }

    // Flag aCanvasElement as write-only if drawing an image with aPrincipal
    // onto it would make it such.

    static void DoDrawImageSecurityCheck(nsICanvasElement *aCanvasElement,
                                         nsIPrincipal *aPrincipal,
                                         PRBool forceWriteOnly);

    static void LogMessage (const nsCString& errorString);
    static void LogMessagef (const char *fmt, ...);

private:
    // this can't be instantiated
    CanvasUtils() { }
};

}

#endif /* _CANVASUTILS_H_ */

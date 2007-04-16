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
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
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

#ifndef NSGLPBUFFER_H_
#define NSGLPBUFFER_H_

#include "gfxASurface.h"
#include "gfxImageSurface.h"

#include "glew.h"

#ifdef XP_WIN
#include <windows.h>
#include "wglew.h"
#endif

class nsCanvasRenderingContextGLPrivate;

class nsGLPbuffer {
public:
    nsGLPbuffer();
    ~nsGLPbuffer();

    PRBool Init(nsCanvasRenderingContextGLPrivate *priv);
    PRBool Resize(PRInt32 width, PRInt32 height);

    void Destroy();

    void MakeContextCurrent();

    void SwapBuffers();
    gfxImageSurface* ThebesSurface();

    inline GLEWContext *glewGetContext() {
        return &mGlewContext;
    }

#ifdef XP_WIN
    inline WGLEWContext *wglewGetContext() {
        return &mWGlewContext;
    }
#endif

protected:
    static void *sCurrentContextToken;

    nsCanvasRenderingContextGLPrivate *mPriv;

    PRInt32 mWidth, mHeight;

    GLEWContext mGlewContext;

    nsRefPtr<gfxImageSurface> mThebesSurface;

#ifdef XP_WIN
    // this is the crap that we need to get the gl entry points
    HWND mGlewWindow;
    HDC mGlewDC;
    HGLRC mGlewWglContext;

    // and this is the actual stuff that we need to render
    HPBUFFERARB mPbuffer;
    HDC mPbufferDC;
    HGLRC mPbufferContext;

    WGLEWContext mWGlewContext;
#endif

};

#endif /* NSGLPBUFFER_H_ */

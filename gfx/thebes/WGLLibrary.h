/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "GLContext.h"

namespace mozilla {
namespace gl {

class WGLLibrary
{
public:
    WGLLibrary() : mInitialized(PR_FALSE), mOGLLibrary(nsnull) {}

    typedef HGLRC (GLAPIENTRY * PFNWGLCREATECONTEXTPROC) (HDC);
    PFNWGLCREATECONTEXTPROC fCreateContext;
    typedef BOOL (GLAPIENTRY * PFNWGLDELETECONTEXTPROC) (HGLRC);
    PFNWGLDELETECONTEXTPROC fDeleteContext;
    typedef BOOL (GLAPIENTRY * PFNWGLMAKECURRENTPROC) (HDC, HGLRC);
    PFNWGLMAKECURRENTPROC fMakeCurrent;
    typedef PROC (GLAPIENTRY * PFNWGLGETPROCADDRESSPROC) (LPCSTR);
    PFNWGLGETPROCADDRESSPROC fGetProcAddress;
    typedef HGLRC (GLAPIENTRY * PFNWGLGETCURRENTCONTEXT) (void);
    PFNWGLGETCURRENTCONTEXT fGetCurrentContext;
    typedef HDC (GLAPIENTRY * PFNWGLGETCURRENTDC) (void);
    PFNWGLGETCURRENTDC fGetCurrentDC;
    typedef BOOL (GLAPIENTRY * PFNWGLSHARELISTS) (HGLRC oldContext, HGLRC newContext);
    PFNWGLSHARELISTS fShareLists;

    typedef HANDLE (WINAPI * PFNWGLCREATEPBUFFERPROC) (HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int* piAttribList);
    PFNWGLCREATEPBUFFERPROC fCreatePbuffer;
    typedef BOOL (WINAPI * PFNWGLDESTROYPBUFFERPROC) (HANDLE hPbuffer);
    PFNWGLDESTROYPBUFFERPROC fDestroyPbuffer;
    typedef HDC (WINAPI * PFNWGLGETPBUFFERDCPROC) (HANDLE hPbuffer);
    PFNWGLGETPBUFFERDCPROC fGetPbufferDC;

    typedef BOOL (WINAPI * PFNWGLBINDTEXIMAGEPROC) (HANDLE hPbuffer, int iBuffer);
    PFNWGLBINDTEXIMAGEPROC fBindTexImage;
    typedef BOOL (WINAPI * PFNWGLRELEASETEXIMAGEPROC) (HANDLE hPbuffer, int iBuffer);
    PFNWGLRELEASETEXIMAGEPROC fReleaseTexImage;

    typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATPROC) (HDC hdc, const int* piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
    PFNWGLCHOOSEPIXELFORMATPROC fChoosePixelFormat;
    typedef BOOL (WINAPI * PFNWGLGETPIXELFORMATATTRIBIVPROC) (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int* piAttributes, int *piValues);
    PFNWGLGETPIXELFORMATATTRIBIVPROC fGetPixelFormatAttribiv;

    bool EnsureInitialized();

private:
    bool mInitialized;
    PRLibrary *mOGLLibrary;
};

// a global WGLLibrary instance
extern WGLLibrary sWGLLibrary;

} /* namespace gl */
} /* namespace mozilla */


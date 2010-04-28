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

#include "GLContextProvider.h"
#include "nsDebug.h"
#include "nsIWidget.h"

namespace mozilla {
namespace gl {

GLContextProvider sGLContextProvider;

static class WGLLibrary
{
public:
    WGLLibrary() : mInitialized(PR_FALSE) {}

    typedef HGLRC (GLAPIENTRY * PFNWGLCREATECONTEXTPROC) (HDC);
    PFNWGLCREATECONTEXTPROC fCreateContext;
    typedef BOOL (GLAPIENTRY * PFNWGLDELETECONTEXTPROC) (HGLRC);
    PFNWGLDELETECONTEXTPROC fDeleteContext;
    typedef BOOL (GLAPIENTRY * PFNWGLMAKECURRENTPROC) (HDC, HGLRC);
    PFNWGLMAKECURRENTPROC fMakeCurrent;
    typedef PROC (GLAPIENTRY * PFNWGLGETPROCADDRESSPROC) (LPCSTR);
    PFNWGLGETPROCADDRESSPROC fGetProcAddress;

    PRBool EnsureInitialized()
    {
        if (mInitialized) {
            return PR_TRUE;
        }
        if (!mOGLLibrary) {
            mOGLLibrary = PR_LoadLibrary("Opengl32.dll");
            if (!mOGLLibrary) {
                NS_WARNING("Couldn't load OpenGL DLL.");
                return PR_FALSE;
            }
        }
        fCreateContext = (PFNWGLCREATECONTEXTPROC)
          PR_FindFunctionSymbol(mOGLLibrary, "wglCreateContext");
        fDeleteContext = (PFNWGLDELETECONTEXTPROC)
          PR_FindFunctionSymbol(mOGLLibrary, "wglDeleteContext");
        fMakeCurrent = (PFNWGLMAKECURRENTPROC)
          PR_FindFunctionSymbol(mOGLLibrary, "wglMakeCurrent");
        fGetProcAddress = (PFNWGLGETPROCADDRESSPROC)
          PR_FindFunctionSymbol(mOGLLibrary, "wglGetProcAddress");
        if (!fCreateContext || !fDeleteContext ||
            !fMakeCurrent || !fGetProcAddress) {
            return PR_FALSE;
        }
        mInitialized = PR_TRUE;
        return PR_TRUE;
    }

private:
    PRBool mInitialized;
    PRLibrary *mOGLLibrary;
} sWGLLibrary;

class GLContextWGL : public GLContext
{
public:
    GLContextWGL(HDC aDC, HGLRC aContext)
        : mContext(aContext), mDC(aDC) {}

    ~GLContextWGL()
    {
        sWGLLibrary.fDeleteContext(mContext);
    }

    PRBool Init()
    {
        MakeCurrent();
        SetupLookupFunction();
        return InitWithPrefix("gl", PR_TRUE);
    }

    PRBool MakeCurrent()
    {
        BOOL succeeded = sWGLLibrary.fMakeCurrent(mDC, mContext);
        NS_ASSERTION(succeeded, "Failed to make GL context current!");
        return succeeded;
    }

    PRBool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sWGLLibrary.fGetProcAddress;
        return PR_TRUE;
    }

private:
    HGLRC mContext;
    HDC mDC;
};

already_AddRefed<GLContext>
GLContextProvider::CreateForWindow(nsIWidget *aWidget)
{
    if (!sWGLLibrary.EnsureInitialized()) {
        return nsnull;
    }
    /**
       * We need to make sure we call SetPixelFormat -after- calling 
       * EnsureInitialized, otherwise it can load/unload the dll and 
       * wglCreateContext will fail.
       */

    HDC dc = (HDC)aWidget->GetNativeData(NS_NATIVE_GRAPHIC);

    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int iFormat = ChoosePixelFormat(dc, &pfd);

    SetPixelFormat(dc, iFormat, &pfd);
    HGLRC context = sWGLLibrary.fCreateContext(dc);
    if (!context) {
        return nsnull;
    }

    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(dc, context);
    glContext->Init();

    return glContext.forget().get();
}

already_AddRefed<GLContext>
GLContextProvider::CreatePbuffer(const gfxSize &)
{
    return nsnull;
}

} /* namespace gl */
} /* namespace mozilla */

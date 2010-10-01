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
 *   Benoit Jacob <bjacob@mozilla.com>
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

#include "GLContextProvider.h"
#include "GLContext.h"
#include "nsDebug.h"
#include "nsString.h"
#include "nsIWidget.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIConsoleService.h"
#include "nsIPrefService.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"

// from GL/osmesa.h. We don't include that file so as to avoid having a build-time dependency on OSMesa.
#define OSMESA_RGBA     GL_RGBA
#define OSMESA_BGRA     0x1
#define OSMESA_ARGB     0x2
#define OSMESA_RGB      GL_RGB
#define OSMESA_BGR      0x4
#define OSMESA_RGB_565  0x5
#define OSMESA_Y_UP     0x11

namespace mozilla {
namespace gl {

static void LogMessage(const char *msg)
{
  nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (console) {
    console->LogStringMessage(NS_ConvertUTF8toUTF16(nsDependentCString(msg)).get());
    fprintf(stderr, "%s\n", msg);
  }
}

typedef void* PrivateOSMesaContext;

class OSMesaLibrary
{
public:
    OSMesaLibrary() : mInitialized(PR_FALSE), mOSMesaLibrary(nsnull) {}

    typedef PrivateOSMesaContext (GLAPIENTRY * PFNOSMESACREATECONTEXTEXT) (GLenum, GLint, GLint, GLint, PrivateOSMesaContext);
    typedef void (GLAPIENTRY * PFNOSMESADESTROYCONTEXT) (PrivateOSMesaContext);
    typedef bool (GLAPIENTRY * PFNOSMESAMAKECURRENT) (PrivateOSMesaContext, void *, GLenum, GLsizei, GLsizei);
    typedef PrivateOSMesaContext (GLAPIENTRY * PFNOSMESAGETCURRENTCONTEXT) (void);
    typedef void (GLAPIENTRY * PFNOSMESAPIXELSTORE) (GLint, GLint);
    typedef PRFuncPtr (GLAPIENTRY * PFNOSMESAGETPROCADDRESS) (const char*);

    PFNOSMESACREATECONTEXTEXT fCreateContextExt;
    PFNOSMESADESTROYCONTEXT fDestroyContext;
    PFNOSMESAMAKECURRENT fMakeCurrent;
    PFNOSMESAGETCURRENTCONTEXT fGetCurrentContext;
    PFNOSMESAPIXELSTORE fPixelStore;
    PFNOSMESAGETPROCADDRESS fGetProcAddress;

    PRBool EnsureInitialized();

private:
    PRBool mInitialized;
    PRLibrary *mOSMesaLibrary;
};

OSMesaLibrary sOSMesaLibrary;

PRBool
OSMesaLibrary::EnsureInitialized()
{
    if (mInitialized)
        return PR_TRUE;

    nsresult rv;

    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch("webgl.", getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCString osmesalib;

    rv = prefBranch->GetCharPref("osmesalib", getter_Copies(osmesalib));

    if (NS_FAILED(rv) ||
        osmesalib.Length() == 0)
    {
        return PR_FALSE;
    }

    mOSMesaLibrary = PR_LoadLibrary(osmesalib.get());

    if (!mOSMesaLibrary) {
        LogMessage("Couldn't open OSMesa lib for software rendering -- webgl.osmesalib path is incorrect, or not a valid shared library");
        return PR_FALSE;
    }

    LibrarySymbolLoader::SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &fCreateContextExt, { "OSMesaCreateContextExt", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "OSMesaMakeCurrent", NULL } },
        { (PRFuncPtr*) &fPixelStore, { "OSMesaPixelStore", NULL } },
        { (PRFuncPtr*) &fDestroyContext, { "OSMesaDestroyContext", NULL } },
        { (PRFuncPtr*) &fGetCurrentContext, { "OSMesaGetCurrentContext", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "OSMesaMakeCurrent", NULL } },
        { (PRFuncPtr*) &fGetProcAddress, { "OSMesaGetProcAddress", NULL } },
        { NULL, { NULL } }
    };

    if (!LibrarySymbolLoader::LoadSymbols(mOSMesaLibrary, &symbols[0])) {
        LogMessage("Couldn't find required entry points in OSMesa libary");
        return PR_FALSE;
    }

    mInitialized = PR_TRUE;
    return PR_TRUE;
}

class GLContextOSMesa : public GLContext
{
public:
    GLContextOSMesa(const ContextFormat& aFormat)
        : GLContext(aFormat, PR_TRUE, nsnull),
          mThebesSurface(nsnull),
          mContext(nsnull)
    {
    }

    ~GLContextOSMesa()
    {
        MarkDestroyed();

        if (mContext)
            sOSMesaLibrary.fDestroyContext(mContext);
    }

    GLContextType GetContextType() {
        return ContextTypeOSMesa;
    }

    PRBool Init(const gfxIntSize &aSize)
    {
        int osmesa_format = -1;
        int gfxasurface_imageformat = -1;
        PRBool format_accepted = PR_FALSE;

        if (mCreationFormat.red > 0 &&
            mCreationFormat.green > 0 &&
            mCreationFormat.blue > 0 &&
            mCreationFormat.red <= 8 &&
            mCreationFormat.green <= 8 &&
            mCreationFormat.blue <= 8)
        {
            if (mCreationFormat.alpha == 0) {
                // we can't use OSMESA_BGR because it is packed 24 bits per pixel.
                // So we use OSMESA_BGRA and have to use ImageFormatRGB24
                // to make sure that the dummy alpha channel is ignored.
                osmesa_format = OSMESA_BGRA;
                gfxasurface_imageformat = gfxASurface::ImageFormatRGB24;
                format_accepted = PR_TRUE;
            } else if (mCreationFormat.alpha <= 8) {
                osmesa_format = OSMESA_BGRA;
                gfxasurface_imageformat = gfxASurface::ImageFormatARGB32;
                format_accepted = PR_TRUE;
            }
        }
        if (!format_accepted) {
            NS_WARNING("Pixel format not supported with OSMesa.");
            return PR_FALSE;
        }

        mThebesSurface = new gfxImageSurface(aSize, gfxASurface::gfxImageFormat(gfxasurface_imageformat));
        if (mThebesSurface->CairoStatus() != 0) {
            NS_WARNING("image surface failed");
            return PR_FALSE;
        }

        mContext = sOSMesaLibrary.fCreateContextExt(osmesa_format, mCreationFormat.depth, mCreationFormat.stencil, 0, NULL);
        if (!mContext) {
            NS_WARNING("OSMesaCreateContextExt failed!");
            return PR_FALSE;
        }

        if (!MakeCurrent()) return PR_FALSE;
        if (!SetupLookupFunction()) return PR_FALSE;

        // OSMesa's different from the other GL providers, it renders to an image surface, not to a pbuffer
        sOSMesaLibrary.fPixelStore(OSMESA_Y_UP, 0);

        return InitWithPrefix("gl", PR_TRUE);
    }

    PRBool MakeCurrent(PRBool aForce = PR_FALSE)
    {
        PRBool succeeded
          = sOSMesaLibrary.fMakeCurrent(mContext, mThebesSurface->Data(),
                                        LOCAL_GL_UNSIGNED_BYTE,
                                        mThebesSurface->Width(),
                                        mThebesSurface->Height());
        NS_ASSERTION(succeeded, "Failed to make OSMesa context current!");

        return succeeded;
    }

    PRBool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sOSMesaLibrary.fGetProcAddress;
        return PR_TRUE;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch (aType) {
        case NativeImageSurface:
            return mThebesSurface.get();
        default:
            return nsnull;
        }
    }

private:
    nsRefPtr<gfxImageSurface> mThebesSurface;
    PrivateOSMesaContext mContext;
};

already_AddRefed<GLContext>
GLContextProviderOSMesa::CreateForWindow(nsIWidget *aWidget)
{
    return nsnull;
}

already_AddRefed<GLContext>
GLContextProviderOSMesa::CreateOffscreen(const gfxIntSize& aSize,
                                         const ContextFormat& aFormat)
{
    if (!sOSMesaLibrary.EnsureInitialized()) {
        return nsnull;
    }

    nsRefPtr<GLContextOSMesa> glContext = new GLContextOSMesa(aFormat);

    if (!glContext->Init(aSize))
    {
        return nsnull;
    }

    return glContext.forget();
}

GLContext *
GLContextProviderOSMesa::GetGlobalContext()
{
    return nsnull;
}

void
GLContextProviderOSMesa::Shutdown()
{
}

} /* namespace gl */
} /* namespace mozilla */

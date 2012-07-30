/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContext.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsString.h"
#include "nsIWidget.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIConsoleService.h"
#include "mozilla/Preferences.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"

#include "gfxCrashReporterUtils.h"

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
    OSMesaLibrary() : mInitialized(false), mOSMesaLibrary(nullptr) {}

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

    bool EnsureInitialized();

private:
    bool mInitialized;
    PRLibrary *mOSMesaLibrary;
};

OSMesaLibrary sOSMesaLibrary;

bool
OSMesaLibrary::EnsureInitialized()
{
    if (mInitialized)
        return true;

    nsAdoptingCString osmesalib = Preferences::GetCString("webgl.osmesalib");
    if (osmesalib.IsEmpty()) {
        return false;
    }

    mOSMesaLibrary = PR_LoadLibrary(osmesalib.get());

    if (!mOSMesaLibrary) {
        LogMessage("Couldn't open OSMesa lib for software rendering -- webgl.osmesalib path is incorrect, or not a valid shared library");
        return false;
    }

    GLLibraryLoader::SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &fCreateContextExt, { "OSMesaCreateContextExt", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "OSMesaMakeCurrent", NULL } },
        { (PRFuncPtr*) &fPixelStore, { "OSMesaPixelStore", NULL } },
        { (PRFuncPtr*) &fDestroyContext, { "OSMesaDestroyContext", NULL } },
        { (PRFuncPtr*) &fGetCurrentContext, { "OSMesaGetCurrentContext", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "OSMesaMakeCurrent", NULL } },
        { (PRFuncPtr*) &fGetProcAddress, { "OSMesaGetProcAddress", NULL } },
        { NULL, { NULL } }
    };

    if (!GLLibraryLoader::LoadSymbols(mOSMesaLibrary, &symbols[0])) {
        LogMessage("Couldn't find required entry points in OSMesa libary");
        return false;
    }

    mInitialized = true;
    return true;
}

class GLContextOSMesa : public GLContext
{
public:
    GLContextOSMesa(const ContextFormat& aFormat)
        : GLContext(aFormat, true, nullptr),
          mThebesSurface(nullptr),
          mContext(nullptr)
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

    bool Init(const gfxIntSize &aSize)
    {
        int osmesa_format = -1;
        int gfxasurface_imageformat = -1;
        bool format_accepted = false;

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
                format_accepted = true;
            } else if (mCreationFormat.alpha <= 8) {
                osmesa_format = OSMESA_BGRA;
                gfxasurface_imageformat = gfxASurface::ImageFormatARGB32;
                format_accepted = true;
            }
        }
        if (!format_accepted) {
            NS_WARNING("Pixel format not supported with OSMesa.");
            return false;
        }

        mThebesSurface = new gfxImageSurface(aSize, gfxASurface::gfxImageFormat(gfxasurface_imageformat));
        if (mThebesSurface->CairoStatus() != 0) {
            NS_WARNING("image surface failed");
            return false;
        }

        mContext = sOSMesaLibrary.fCreateContextExt(osmesa_format, mCreationFormat.depth, mCreationFormat.stencil, 0, NULL);
        if (!mContext) {
            NS_WARNING("OSMesaCreateContextExt failed!");
            return false;
        }

        if (!MakeCurrent()) return false;
        if (!SetupLookupFunction()) return false;

        // OSMesa's different from the other GL providers, it renders to an image surface, not to a pbuffer
        sOSMesaLibrary.fPixelStore(OSMESA_Y_UP, 0);

        return InitWithPrefix("gl", true);
    }

    bool MakeCurrentImpl(bool aForce = false)
    {
        bool succeeded
          = sOSMesaLibrary.fMakeCurrent(mContext, mThebesSurface->Data(),
                                        LOCAL_GL_UNSIGNED_BYTE,
                                        mThebesSurface->Width(),
                                        mThebesSurface->Height());
        NS_ASSERTION(succeeded, "Failed to make OSMesa context current!");

        return succeeded;
    }

    bool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sOSMesaLibrary.fGetProcAddress;
        return true;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch (aType) {
        case NativeImageSurface:
            return mThebesSurface.get();
        default:
            return nullptr;
        }
    }

    bool SupportsRobustness()
    {
        return false;
    }

private:
    nsRefPtr<gfxImageSurface> mThebesSurface;
    PrivateOSMesaContext mContext;
};

already_AddRefed<GLContext>
GLContextProviderOSMesa::CreateForWindow(nsIWidget *aWidget)
{
    return nullptr;
}

already_AddRefed<GLContext>
GLContextProviderOSMesa::CreateOffscreen(const gfxIntSize& aSize,
                                         const ContextFormat& aFormat,
                                         const ContextFlags)
{
    if (!sOSMesaLibrary.EnsureInitialized()) {
        return nullptr;
    }

    ContextFormat actualFormat(aFormat);
    actualFormat.samples = 0;

    nsRefPtr<GLContextOSMesa> glContext = new GLContextOSMesa(actualFormat);

    if (!glContext->Init(aSize))
    {
        return nullptr;
    }

    return glContext.forget();
}

GLContext *
GLContextProviderOSMesa::GetGlobalContext(const ContextFlags)
{
    return nullptr;
}

void
GLContextProviderOSMesa::Shutdown()
{
}

} /* namespace gl */
} /* namespace mozilla */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#define GET_NATIVE_WINDOW(aWidget) GDK_WINDOW_XID((GdkWindow*) aWidget->GetNativeData(NS_NATIVE_WINDOW))
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "X11UndefineNone.h"

#include "mozilla/MathAlgorithms.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/widget/X11CompositorWidget.h"
#include "mozilla/Unused.h"

#include "prenv.h"
#include "GLContextProvider.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "GLXLibrary.h"
#include "gfxXlibSurface.h"
#include "gfxContext.h"
#include "gfxEnv.h"
#include "gfxPlatform.h"
#include "GLContextGLX.h"
#include "gfxUtils.h"
#include "gfx2DGlue.h"
#include "GLScreenBuffer.h"
#include "gfxPrefs.h"

#include "gfxCrashReporterUtils.h"

#ifdef MOZ_WIDGET_GTK
#include "gfxPlatformGtk.h"
#endif

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

GLXLibrary sGLXLibrary;

// Check that we have at least version aMajor.aMinor .
bool
GLXLibrary::GLXVersionCheck(int aMajor, int aMinor)
{
    return aMajor < mGLXMajorVersion ||
           (aMajor == mGLXMajorVersion && aMinor <= mGLXMinorVersion);
}

static inline bool
HasExtension(const char* aExtensions, const char* aRequiredExtension)
{
    return GLContext::ListHasExtension(
        reinterpret_cast<const GLubyte*>(aExtensions), aRequiredExtension);
}

bool
GLXLibrary::EnsureInitialized()
{
    if (mInitialized) {
        return true;
    }

    // Don't repeatedly try to initialize.
    if (mTriedInitializing) {
        return false;
    }
    mTriedInitializing = true;

    // Force enabling s3 texture compression. (Bug 774134)
    PR_SetEnv("force_s3tc_enable=true");

    if (!mOGLLibrary) {
        const char* libGLfilename = nullptr;
        bool forceFeatureReport = false;

        // see e.g. bug 608526: it is intrinsically interesting to know whether we have dynamically linked to libGL.so.1
        // because at least the NVIDIA implementation requires an executable stack, which causes mprotect calls,
        // which trigger glibc bug http://sourceware.org/bugzilla/show_bug.cgi?id=12225
#ifdef __OpenBSD__
        libGLfilename = "libGL.so";
#else
        libGLfilename = "libGL.so.1";
#endif

        ScopedGfxFeatureReporter reporter(libGLfilename, forceFeatureReport);
        mOGLLibrary = PR_LoadLibrary(libGLfilename);
        if (!mOGLLibrary) {
            NS_WARNING("Couldn't load OpenGL shared library.");
            return false;
        }
        reporter.SetSuccessful();
    }

    if (gfxEnv::GlxDebug()) {
        mDebug = true;
    }

    GLLibraryLoader::SymLoadStruct symbols[] = {
        /* functions that were in GLX 1.0 */
        { (PRFuncPtr*) &xDestroyContextInternal, { "glXDestroyContext", nullptr } },
        { (PRFuncPtr*) &xMakeCurrentInternal, { "glXMakeCurrent", nullptr } },
        { (PRFuncPtr*) &xSwapBuffersInternal, { "glXSwapBuffers", nullptr } },
        { (PRFuncPtr*) &xQueryVersionInternal, { "glXQueryVersion", nullptr } },
        { (PRFuncPtr*) &xGetCurrentContextInternal, { "glXGetCurrentContext", nullptr } },
        { (PRFuncPtr*) &xWaitGLInternal, { "glXWaitGL", nullptr } },
        { (PRFuncPtr*) &xWaitXInternal, { "glXWaitX", nullptr } },
        /* functions introduced in GLX 1.1 */
        { (PRFuncPtr*) &xQueryExtensionsStringInternal, { "glXQueryExtensionsString", nullptr } },
        { (PRFuncPtr*) &xGetClientStringInternal, { "glXGetClientString", nullptr } },
        { (PRFuncPtr*) &xQueryServerStringInternal, { "glXQueryServerString", nullptr } },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct symbols13[] = {
        /* functions introduced in GLX 1.3 */
        { (PRFuncPtr*) &xChooseFBConfigInternal, { "glXChooseFBConfig", nullptr } },
        { (PRFuncPtr*) &xGetFBConfigAttribInternal, { "glXGetFBConfigAttrib", nullptr } },
        // WARNING: xGetFBConfigs not set in symbols13_ext
        { (PRFuncPtr*) &xGetFBConfigsInternal, { "glXGetFBConfigs", nullptr } },
        // WARNING: symbols13_ext sets xCreateGLXPixmapWithConfig instead
        { (PRFuncPtr*) &xCreatePixmapInternal, { "glXCreatePixmap", nullptr } },
        { (PRFuncPtr*) &xDestroyPixmapInternal, { "glXDestroyPixmap", nullptr } },
        { (PRFuncPtr*) &xCreateNewContextInternal, { "glXCreateNewContext", nullptr } },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct symbols13_ext[] = {
        /* extension equivalents for functions introduced in GLX 1.3 */
        // GLX_SGIX_fbconfig extension
        { (PRFuncPtr*) &xChooseFBConfigInternal, { "glXChooseFBConfigSGIX", nullptr } },
        { (PRFuncPtr*) &xGetFBConfigAttribInternal, { "glXGetFBConfigAttribSGIX", nullptr } },
        // WARNING: no xGetFBConfigs equivalent in extensions
        // WARNING: different from symbols13:
        { (PRFuncPtr*) &xCreateGLXPixmapWithConfigInternal, { "glXCreateGLXPixmapWithConfigSGIX", nullptr } },
        { (PRFuncPtr*) &xDestroyPixmapInternal, { "glXDestroyGLXPixmap", nullptr } }, // not from ext
        { (PRFuncPtr*) &xCreateNewContextInternal, { "glXCreateContextWithConfigSGIX", nullptr } },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct symbols14[] = {
        /* functions introduced in GLX 1.4 */
        { (PRFuncPtr*) &xGetProcAddressInternal, { "glXGetProcAddress", nullptr } },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct symbols14_ext[] = {
        /* extension equivalents for functions introduced in GLX 1.4 */
        // GLX_ARB_get_proc_address extension
        { (PRFuncPtr*) &xGetProcAddressInternal, { "glXGetProcAddressARB", nullptr } },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct symbols_texturefrompixmap[] = {
        { (PRFuncPtr*) &xBindTexImageInternal, { "glXBindTexImageEXT", nullptr } },
        { (PRFuncPtr*) &xReleaseTexImageInternal, { "glXReleaseTexImageEXT", nullptr } },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct symbols_createcontext[] = {
        { (PRFuncPtr*) &xCreateContextAttribsInternal, { "glXCreateContextAttribsARB", nullptr } },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct symbols_videosync[] = {
      { (PRFuncPtr*) &xGetVideoSyncInternal, { "glXGetVideoSyncSGI", nullptr } },
      { (PRFuncPtr*) &xWaitVideoSyncInternal, { "glXWaitVideoSyncSGI", nullptr } },
      { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct symbols_swapcontrol[] = {
      { (PRFuncPtr*) &xSwapIntervalInternal, { "glXSwapIntervalEXT", nullptr } },
      { nullptr, { nullptr } }
    };

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &symbols[0])) {
        NS_WARNING("Couldn't find required entry point in OpenGL shared library");
        return false;
    }

    Display* display = DefaultXDisplay();
    int screen = DefaultScreen(display);

    if (!xQueryVersion(display, &mGLXMajorVersion, &mGLXMinorVersion)) {
        mGLXMajorVersion = 0;
        mGLXMinorVersion = 0;
        return false;
    }

    if (!GLXVersionCheck(1, 1))
        // Not possible to query for extensions.
        return false;

    const char* clientVendor = xGetClientString(display, LOCAL_GLX_VENDOR);
    const char* serverVendor = xQueryServerString(display, screen, LOCAL_GLX_VENDOR);
    const char* extensionsStr = xQueryExtensionsString(display, screen);

    GLLibraryLoader::SymLoadStruct* sym13;
    if (!GLXVersionCheck(1, 3)) {
        // Even if we don't have 1.3, we might have equivalent extensions
        // (as on the Intel X server).
        if (!HasExtension(extensionsStr, "GLX_SGIX_fbconfig")) {
            return false;
        }
        sym13 = symbols13_ext;
    } else {
        sym13 = symbols13;
    }
    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, sym13)) {
        NS_WARNING("Couldn't find required entry point in OpenGL shared library");
        return false;
    }

    GLLibraryLoader::SymLoadStruct* sym14;
    if (!GLXVersionCheck(1, 4)) {
        // Even if we don't have 1.4, we might have equivalent extensions
        // (as on the Intel X server).
        if (!HasExtension(extensionsStr, "GLX_ARB_get_proc_address")) {
            return false;
        }
        sym14 = symbols14_ext;
    } else {
        sym14 = symbols14;
    }
    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, sym14)) {
        NS_WARNING("Couldn't find required entry point in OpenGL shared library");
        return false;
    }

    if (HasExtension(extensionsStr, "GLX_EXT_texture_from_pixmap") &&
        GLLibraryLoader::LoadSymbols(mOGLLibrary, symbols_texturefrompixmap,
                                         (GLLibraryLoader::PlatformLookupFunction)&xGetProcAddress))
    {
        mUseTextureFromPixmap = gfxPrefs::UseGLXTextureFromPixmap();
    } else {
        mUseTextureFromPixmap = false;
        NS_WARNING("Texture from pixmap disabled");
    }

    if (HasExtension(extensionsStr, "GLX_ARB_create_context") &&
        HasExtension(extensionsStr, "GLX_ARB_create_context_profile") &&
        GLLibraryLoader::LoadSymbols(mOGLLibrary, symbols_createcontext,
                                     (GLLibraryLoader::PlatformLookupFunction)&xGetProcAddress))
    {
        mHasCreateContextAttribs = true;
    }

    if (HasExtension(extensionsStr, "GLX_ARB_create_context_robustness"))
    {
        mHasRobustness = true;
    }

    if (HasExtension(extensionsStr, "GLX_SGI_video_sync") &&
        GLLibraryLoader::LoadSymbols(mOGLLibrary, symbols_videosync,
                                     (GLLibraryLoader::PlatformLookupFunction)&xGetProcAddress))
    {
        mHasVideoSync = true;
    }

    if (!(HasExtension(extensionsStr, "GLX_EXT_swap_control") &&
          GLLibraryLoader::LoadSymbols(mOGLLibrary, symbols_swapcontrol)))
    {
        NS_WARNING("GLX_swap_control unsupported, ASAP mode may still block on buffer swaps.");
    }

    mIsATI = serverVendor && DoesStringMatch(serverVendor, "ATI");
    mIsNVIDIA = serverVendor && DoesStringMatch(serverVendor, "NVIDIA Corporation");
    mClientIsMesa = clientVendor && DoesStringMatch(clientVendor, "Mesa");

    mInitialized = true;

    return true;
}

bool
GLXLibrary::SupportsTextureFromPixmap(gfxASurface* aSurface)
{
    if (!EnsureInitialized()) {
        return false;
    }

    if (aSurface->GetType() != gfxSurfaceType::Xlib || !mUseTextureFromPixmap) {
        return false;
    }

    return true;
}

bool
GLXLibrary::SupportsVideoSync()
{
    if (!EnsureInitialized()) {
        return false;
    }

    return mHasVideoSync;
}

GLXPixmap
GLXLibrary::CreatePixmap(gfxASurface* aSurface)
{
    if (!SupportsTextureFromPixmap(aSurface)) {
        return X11None;
    }

    gfxXlibSurface* xs = static_cast<gfxXlibSurface*>(aSurface);
    const XRenderPictFormat* format = xs->XRenderFormat();
    if (!format || format->type != PictTypeDirect) {
        return X11None;
    }
    const XRenderDirectFormat& direct = format->direct;
    int alphaSize = FloorLog2(direct.alphaMask + 1);
    NS_ASSERTION((1 << alphaSize) - 1 == direct.alphaMask,
                 "Unexpected render format with non-adjacent alpha bits");

    int attribs[] = { LOCAL_GLX_DOUBLEBUFFER, False,
                      LOCAL_GLX_DRAWABLE_TYPE, LOCAL_GLX_PIXMAP_BIT,
                      LOCAL_GLX_ALPHA_SIZE, alphaSize,
                      (alphaSize ? LOCAL_GLX_BIND_TO_TEXTURE_RGBA_EXT
                       : LOCAL_GLX_BIND_TO_TEXTURE_RGB_EXT), True,
                      LOCAL_GLX_RENDER_TYPE, LOCAL_GLX_RGBA_BIT,
                      X11None };

    int numConfigs = 0;
    Display* display = xs->XDisplay();
    int xscreen = DefaultScreen(display);

    ScopedXFree<GLXFBConfig> cfgs(xChooseFBConfig(display,
                                                  xscreen,
                                                  attribs,
                                                  &numConfigs));

    // Find an fbconfig that matches the pixel format used on the Pixmap.
    int matchIndex = -1;
    unsigned long redMask =
        static_cast<unsigned long>(direct.redMask) << direct.red;
    unsigned long greenMask =
        static_cast<unsigned long>(direct.greenMask) << direct.green;
    unsigned long blueMask =
        static_cast<unsigned long>(direct.blueMask) << direct.blue;
    // This is true if the Pixmap has bits for alpha or unused bits.
    bool haveNonColorBits =
        ~(redMask | greenMask | blueMask) != -1UL << format->depth;

    for (int i = 0; i < numConfigs; i++) {
        int id = X11None;
        sGLXLibrary.xGetFBConfigAttrib(display, cfgs[i], LOCAL_GLX_VISUAL_ID, &id);
        Visual* visual;
        int depth;
        FindVisualAndDepth(display, id, &visual, &depth);
        if (!visual ||
            visual->c_class != TrueColor ||
            visual->red_mask != redMask ||
            visual->green_mask != greenMask ||
            visual->blue_mask != blueMask ) {
            continue;
        }

        // Historically Xlib Visuals did not try to represent an alpha channel
        // and there was no means to use an alpha channel on a Pixmap.  The
        // Xlib Visual from the fbconfig was not intended to have any
        // information about alpha bits.
        //
        // Since then, RENDER has added formats for 32 bit depth Pixmaps.
        // Some of these formats have bits for alpha and some have unused
        // bits.
        //
        // Then the Composite extension added a 32 bit depth Visual intended
        // for Windows with an alpha channel, so bits not in the visual color
        // masks were expected to be treated as alpha bits.
        //
        // Usually GLX counts only color bits in the Visual depth, but the
        // depth of Composite's ARGB Visual includes alpha bits.  However,
        // bits not in the color masks are not necessarily alpha bits because
        // sometimes (NVIDIA) 32 bit Visuals are added for fbconfigs with 32
        // bit BUFFER_SIZE but zero alpha bits and 24 color bits (NVIDIA
        // again).
        //
        // This checks that the depth matches in one of the two ways.
        // NVIDIA now forces format->depth == depth so only the first way
        // is checked for NVIDIA
        if (depth != format->depth &&
            (mIsNVIDIA || depth != format->depth - alphaSize) ) {
            continue;
        }

        // If all bits of the Pixmap are color bits and the Pixmap depth
        // matches the depth of the fbconfig visual, then we can assume that
        // the driver will do whatever is necessary to ensure that any
        // GLXPixmap alpha bits are treated as set.  We can skip the
        // ALPHA_SIZE check in this situation.  We need to skip this check for
        // situations (ATI) where there are no fbconfigs without alpha bits.
        //
        // glXChooseFBConfig should prefer configs with smaller
        // LOCAL_GLX_BUFFER_SIZE, so we should still get zero alpha bits if
        // available, except perhaps with NVIDIA drivers where buffer size is
        // not the specified sum of the component sizes.
        if (haveNonColorBits) {
            // There are bits in the Pixmap format that haven't been matched
            // against the fbconfig visual.  These bits could either represent
            // alpha or be unused, so just check that the number of alpha bits
            // matches.
            int size = 0;
            sGLXLibrary.xGetFBConfigAttrib(display, cfgs[i],
                                           LOCAL_GLX_ALPHA_SIZE, &size);
            if (size != alphaSize) {
                continue;
            }
        }

        matchIndex = i;
        break;
    }
    if (matchIndex == -1) {
        // GLX can't handle A8 surfaces, so this is not really unexpected. The
        // caller should deal with this situation.
        NS_WARNING_ASSERTION(
          format->depth == 8,
          "[GLX] Couldn't find a FBConfig matching Pixmap format");
        return X11None;
    }

    int pixmapAttribs[] = { LOCAL_GLX_TEXTURE_TARGET_EXT, LOCAL_GLX_TEXTURE_2D_EXT,
                            LOCAL_GLX_TEXTURE_FORMAT_EXT,
                            (alphaSize ? LOCAL_GLX_TEXTURE_FORMAT_RGBA_EXT
                             : LOCAL_GLX_TEXTURE_FORMAT_RGB_EXT),
                            X11None};

    GLXPixmap glxpixmap = xCreatePixmap(display,
                                        cfgs[matchIndex],
                                        xs->XDrawable(),
                                        pixmapAttribs);

    return glxpixmap;
}

void
GLXLibrary::DestroyPixmap(Display* aDisplay, GLXPixmap aPixmap)
{
    if (!mUseTextureFromPixmap) {
        return;
    }

    xDestroyPixmap(aDisplay, aPixmap);
}

void
GLXLibrary::BindTexImage(Display* aDisplay, GLXPixmap aPixmap)
{
    if (!mUseTextureFromPixmap) {
        return;
    }

    // Make sure all X drawing to the surface has finished before binding to a texture.
    if (mClientIsMesa) {
        // Using XSync instead of Mesa's glXWaitX, because its glxWaitX is a
        // noop when direct rendering unless the current drawable is a
        // single-buffer window.
        FinishX(aDisplay);
    } else {
        xWaitX();
    }
    xBindTexImage(aDisplay, aPixmap, LOCAL_GLX_FRONT_LEFT_EXT, nullptr);
}

void
GLXLibrary::ReleaseTexImage(Display* aDisplay, GLXPixmap aPixmap)
{
    if (!mUseTextureFromPixmap) {
        return;
    }

    xReleaseTexImage(aDisplay, aPixmap, LOCAL_GLX_FRONT_LEFT_EXT);
}

void
GLXLibrary::UpdateTexImage(Display* aDisplay, GLXPixmap aPixmap)
{
    // NVIDIA drivers don't require a rebind of the pixmap in order
    // to display an updated image, and it's faster not to do it.
    if (mIsNVIDIA) {
        xWaitX();
        return;
    }

    ReleaseTexImage(aDisplay, aPixmap);
    BindTexImage(aDisplay, aPixmap);
}

#ifdef DEBUG

static int (*sOldErrorHandler)(Display*, XErrorEvent*);
ScopedXErrorHandler::ErrorEvent sErrorEvent;
static int GLXErrorHandler(Display* display, XErrorEvent* ev)
{
    if (!sErrorEvent.mError.error_code) {
        sErrorEvent.mError = *ev;
    }
    return 0;
}

void
GLXLibrary::BeforeGLXCall()
{
    if (mDebug) {
        sOldErrorHandler = XSetErrorHandler(GLXErrorHandler);
    }
}

void
GLXLibrary::AfterGLXCall()
{
    if (mDebug) {
        FinishX(DefaultXDisplay());
        if (sErrorEvent.mError.error_code) {
            char buffer[2048];
            XGetErrorText(DefaultXDisplay(), sErrorEvent.mError.error_code, buffer, sizeof(buffer));
            printf_stderr("X ERROR: %s (%i) - Request: %i.%i, Serial: %lu",
                          buffer,
                          sErrorEvent.mError.error_code,
                          sErrorEvent.mError.request_code,
                          sErrorEvent.mError.minor_code,
                          sErrorEvent.mError.serial);
            NS_ABORT();
        }
        XSetErrorHandler(sOldErrorHandler);
    }
}

#define BEFORE_GLX_CALL do {           \
    sGLXLibrary.BeforeGLXCall();       \
} while (0)

#define AFTER_GLX_CALL do {            \
    sGLXLibrary.AfterGLXCall();        \
} while (0)

#else

#define BEFORE_GLX_CALL do { } while(0)
#define AFTER_GLX_CALL do { } while(0)

#endif

void
GLXLibrary::xDestroyContext(Display* display, GLXContext context)
{
    BEFORE_GLX_CALL;
    xDestroyContextInternal(display, context);
    AFTER_GLX_CALL;
}

Bool
GLXLibrary::xMakeCurrent(Display* display,
                         GLXDrawable drawable,
                         GLXContext context)
{
    BEFORE_GLX_CALL;
    Bool result = xMakeCurrentInternal(display, drawable, context);
    AFTER_GLX_CALL;
    return result;
}

GLXContext
GLXLibrary::xGetCurrentContext()
{
    BEFORE_GLX_CALL;
    GLXContext result = xGetCurrentContextInternal();
    AFTER_GLX_CALL;
    return result;
}

/* static */ void*
GLXLibrary::xGetProcAddress(const char* procName)
{
    BEFORE_GLX_CALL;
    void* result = sGLXLibrary.xGetProcAddressInternal(procName);
    AFTER_GLX_CALL;
    return result;
}

GLXFBConfig*
GLXLibrary::xChooseFBConfig(Display* display,
                            int screen,
                            const int* attrib_list,
                            int* nelements)
{
    BEFORE_GLX_CALL;
    GLXFBConfig* result = xChooseFBConfigInternal(display, screen, attrib_list, nelements);
    AFTER_GLX_CALL;
    return result;
}

GLXFBConfig*
GLXLibrary::xGetFBConfigs(Display* display,
                          int screen,
                          int* nelements)
{
    BEFORE_GLX_CALL;
    GLXFBConfig* result = xGetFBConfigsInternal(display, screen, nelements);
    AFTER_GLX_CALL;
    return result;
}

GLXContext
GLXLibrary::xCreateNewContext(Display* display,
                              GLXFBConfig config,
                              int render_type,
                              GLXContext share_list,
                              Bool direct)
{
    BEFORE_GLX_CALL;
    GLXContext result = xCreateNewContextInternal(display, config,
                                                  render_type,
                                                  share_list, direct);
    AFTER_GLX_CALL;
    return result;
}

int
GLXLibrary::xGetFBConfigAttrib(Display* display,
                               GLXFBConfig config,
                               int attribute,
                               int* value)
{
    BEFORE_GLX_CALL;
    int result = xGetFBConfigAttribInternal(display, config,
                                            attribute, value);
    AFTER_GLX_CALL;
    return result;
}

void
GLXLibrary::xSwapBuffers(Display* display, GLXDrawable drawable)
{
    BEFORE_GLX_CALL;
    xSwapBuffersInternal(display, drawable);
    AFTER_GLX_CALL;
}

const char*
GLXLibrary::xQueryExtensionsString(Display* display,
                                   int screen)
{
    BEFORE_GLX_CALL;
    const char* result = xQueryExtensionsStringInternal(display, screen);
    AFTER_GLX_CALL;
    return result;
}

const char*
GLXLibrary::xGetClientString(Display* display,
                             int screen)
{
    BEFORE_GLX_CALL;
    const char* result = xGetClientStringInternal(display, screen);
    AFTER_GLX_CALL;
    return result;
}

const char*
GLXLibrary::xQueryServerString(Display* display,
                               int screen, int name)
{
    BEFORE_GLX_CALL;
    const char* result = xQueryServerStringInternal(display, screen, name);
    AFTER_GLX_CALL;
    return result;
}

GLXPixmap
GLXLibrary::xCreatePixmap(Display* display,
                          GLXFBConfig config,
                          Pixmap pixmap,
                          const int* attrib_list)
{
    BEFORE_GLX_CALL;
    GLXPixmap result = xCreatePixmapInternal(display, config,
                                             pixmap, attrib_list);
    AFTER_GLX_CALL;
    return result;
}

GLXPixmap
GLXLibrary::xCreateGLXPixmapWithConfig(Display* display,
                                       GLXFBConfig config,
                                       Pixmap pixmap)
{
    BEFORE_GLX_CALL;
    GLXPixmap result = xCreateGLXPixmapWithConfigInternal(display, config, pixmap);
    AFTER_GLX_CALL;
    return result;
}

void
GLXLibrary::xDestroyPixmap(Display* display, GLXPixmap pixmap)
{
    BEFORE_GLX_CALL;
    xDestroyPixmapInternal(display, pixmap);
    AFTER_GLX_CALL;
}

Bool
GLXLibrary::xQueryVersion(Display* display,
                          int* major,
                          int* minor)
{
    BEFORE_GLX_CALL;
    Bool result = xQueryVersionInternal(display, major, minor);
    AFTER_GLX_CALL;
    return result;
}

void
GLXLibrary::xBindTexImage(Display* display,
                          GLXDrawable drawable,
                          int buffer,
                          const int* attrib_list)
{
    BEFORE_GLX_CALL;
    xBindTexImageInternal(display, drawable, buffer, attrib_list);
    AFTER_GLX_CALL;
}

void
GLXLibrary::xReleaseTexImage(Display* display,
                             GLXDrawable drawable,
                             int buffer)
{
    BEFORE_GLX_CALL;
    xReleaseTexImageInternal(display, drawable, buffer);
    AFTER_GLX_CALL;
}

void
GLXLibrary::xWaitGL()
{
    BEFORE_GLX_CALL;
    xWaitGLInternal();
    AFTER_GLX_CALL;
}

void
GLXLibrary::xWaitX()
{
    BEFORE_GLX_CALL;
    xWaitXInternal();
    AFTER_GLX_CALL;
}

GLXContext
GLXLibrary::xCreateContextAttribs(Display* display,
                                  GLXFBConfig config,
                                  GLXContext share_list,
                                  Bool direct,
                                  const int* attrib_list)
{
    BEFORE_GLX_CALL;
    GLXContext result = xCreateContextAttribsInternal(display,
                                                      config,
                                                      share_list,
                                                      direct,
                                                      attrib_list);
    AFTER_GLX_CALL;
    return result;
}

int
GLXLibrary::xGetVideoSync(unsigned int* count)
{
    BEFORE_GLX_CALL;
    int result = xGetVideoSyncInternal(count);
    AFTER_GLX_CALL;
    return result;
}

int
GLXLibrary::xWaitVideoSync(int divisor, int remainder, unsigned int* count)
{
    BEFORE_GLX_CALL;
    int result = xWaitVideoSyncInternal(divisor, remainder, count);
    AFTER_GLX_CALL;
    return result;
}

void
GLXLibrary::xSwapInterval(Display* display, GLXDrawable drawable, int interval)
{
    BEFORE_GLX_CALL;
    xSwapIntervalInternal(display, drawable, interval);
    AFTER_GLX_CALL;
}

already_AddRefed<GLContextGLX>
GLContextGLX::CreateGLContext(CreateContextFlags flags, const SurfaceCaps& caps,
                              GLContextGLX* shareContext, bool isOffscreen,
                              Display* display, GLXDrawable drawable, GLXFBConfig cfg,
                              bool deleteDrawable, gfxXlibSurface* pixmap,
                              ContextProfile profile)
{
    GLXLibrary& glx = sGLXLibrary;

    int db = 0;
    int err = glx.xGetFBConfigAttrib(display, cfg,
                                      LOCAL_GLX_DOUBLEBUFFER, &db);
    if (LOCAL_GLX_BAD_ATTRIBUTE != err) {
        if (ShouldSpew()) {
            printf("[GLX] FBConfig is %sdouble-buffered\n", db ? "" : "not ");
        }
    }

    GLXContext context;
    RefPtr<GLContextGLX> glContext;
    bool error;

    OffMainThreadScopedXErrorHandler xErrorHandler;

    do {
        error = false;

        GLXContext glxContext = shareContext ? shareContext->mContext : nullptr;
        if (glx.HasCreateContextAttribs()) {
            AutoTArray<int, 11> attrib_list;
            if (glx.HasRobustness()) {
                int robust_attribs[] = {
                    LOCAL_GL_CONTEXT_FLAGS_ARB, LOCAL_GL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
                    LOCAL_GL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_GL_LOSE_CONTEXT_ON_RESET_ARB,
                };
                attrib_list.AppendElements(robust_attribs, MOZ_ARRAY_LENGTH(robust_attribs));
            }
            if (profile == ContextProfile::OpenGLCore) {
                int core_attribs[] = {
                    LOCAL_GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                    LOCAL_GLX_CONTEXT_MINOR_VERSION_ARB, 2,
                    LOCAL_GLX_CONTEXT_FLAGS_ARB, LOCAL_GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                };
                attrib_list.AppendElements(core_attribs, MOZ_ARRAY_LENGTH(core_attribs));
            };
            attrib_list.AppendElement(0);

            context = glx.xCreateContextAttribs(
                display,
                cfg,
                glxContext,
                True,
                attrib_list.Elements());
        } else {
            context = glx.xCreateNewContext(
                display,
                cfg,
                LOCAL_GLX_RGBA_TYPE,
                glxContext,
                True);
        }

        if (context) {
            glContext = new GLContextGLX(flags, caps, shareContext, isOffscreen, display,
                                         drawable, context, deleteDrawable, db, pixmap,
                                         profile);
            if (!glContext->Init())
                error = true;
        } else {
            error = true;
        }

        error |= xErrorHandler.SyncAndGetError(display);

        if (error) {
            if (shareContext) {
                shareContext = nullptr;
                continue;
            }

            NS_WARNING("Failed to create GLXContext!");
            glContext = nullptr; // note: this must be done while the graceful X error handler is set,
                                // because glxMakeCurrent can give a GLXBadDrawable error
        }

        return glContext.forget();
    } while (true);
}

GLContextGLX::~GLContextGLX()
{
    MarkDestroyed();

    // Wrapped context should not destroy glxContext/Surface
    if (!mOwnsContext) {
        return;
    }

    // see bug 659842 comment 76
#ifdef DEBUG
    bool success =
#endif
    mGLX->xMakeCurrent(mDisplay, X11None, nullptr);
    MOZ_ASSERT(success,
               "glXMakeCurrent failed to release GL context before we call "
               "glXDestroyContext!");

    mGLX->xDestroyContext(mDisplay, mContext);

    if (mDeleteDrawable) {
        mGLX->xDestroyPixmap(mDisplay, mDrawable);
    }
}


bool
GLContextGLX::Init()
{
    SetupLookupFunction();
    if (!InitWithPrefix("gl", true)) {
        return false;
    }

    // EXT_framebuffer_object is not supported on Core contexts
    // so we'll also check for ARB_framebuffer_object
    if (!IsExtensionSupported(EXT_framebuffer_object) && !IsSupported(GLFeature::framebuffer_object))
        return false;

    return true;
}

bool
GLContextGLX::MakeCurrentImpl(bool aForce)
{
    bool succeeded = true;

    // With the ATI FGLRX driver, glxMakeCurrent is very slow even when the context doesn't change.
    // (This is not the case with other drivers such as NVIDIA).
    // So avoid calling it more than necessary. Since GLX documentation says that:
    //     "glXGetCurrentContext returns client-side information.
    //      It does not make a round trip to the server."
    // I assume that it's not worth using our own TLS slot here.
    if (aForce || mGLX->xGetCurrentContext() != mContext) {
        if (mGLX->IsMesa()) {
          // Read into the event queue to ensure that Mesa receives a
          // DRI2InvalidateBuffers event before drawing. See bug 1280653.
          Unused << XPending(mDisplay);
        }

        succeeded = mGLX->xMakeCurrent(mDisplay, mDrawable, mContext);
        NS_ASSERTION(succeeded, "Failed to make GL context current!");

        if (!IsOffscreen() && mGLX->SupportsSwapControl()) {
            // Many GLX implementations default to blocking until the next
            // VBlank when calling glXSwapBuffers. We want to run unthrottled
            // in ASAP mode. See bug 1280744.
            const bool isASAP = (gfxPrefs::LayoutFrameRate() == 0);
            mGLX->xSwapInterval(mDisplay, mDrawable, isASAP ? 0 : 1);
        }
    }

    return succeeded;
}

bool
GLContextGLX::IsCurrent() {
    return mGLX->xGetCurrentContext() == mContext;
}

bool
GLContextGLX::SetupLookupFunction()
{
    mLookupFunc = (PlatformLookupFunction)&GLXLibrary::xGetProcAddress;
    return true;
}

bool
GLContextGLX::IsDoubleBuffered() const
{
    return mDoubleBuffered;
}

bool
GLContextGLX::SupportsRobustness() const
{
    return mGLX->HasRobustness();
}

bool
GLContextGLX::SwapBuffers()
{
    if (!mDoubleBuffered)
        return false;
    mGLX->xSwapBuffers(mDisplay, mDrawable);
    return true;
}

bool
GLContextGLX::OverrideDrawable(GLXDrawable drawable)
{
    if (Screen())
        Screen()->AssureBlitted();
    Bool result = mGLX->xMakeCurrent(mDisplay, drawable, mContext);
    return result;
}

bool
GLContextGLX::RestoreDrawable()
{
    return mGLX->xMakeCurrent(mDisplay, mDrawable, mContext);
}

GLContextGLX::GLContextGLX(
                  CreateContextFlags flags,
                  const SurfaceCaps& caps,
                  GLContext* shareContext,
                  bool isOffscreen,
                  Display* aDisplay,
                  GLXDrawable aDrawable,
                  GLXContext aContext,
                  bool aDeleteDrawable,
                  bool aDoubleBuffered,
                  gfxXlibSurface* aPixmap,
                  ContextProfile profile)
    : GLContext(flags, caps, shareContext, isOffscreen),
      mContext(aContext),
      mDisplay(aDisplay),
      mDrawable(aDrawable),
      mDeleteDrawable(aDeleteDrawable),
      mDoubleBuffered(aDoubleBuffered),
      mGLX(&sGLXLibrary),
      mPixmap(aPixmap),
      mOwnsContext(true)
{
    MOZ_ASSERT(mGLX);
    // See 899855
    SetProfileVersion(profile, 200);
}


static GLContextGLX*
GetGlobalContextGLX()
{
    return static_cast<GLContextGLX*>(GLContextProviderGLX::GetGlobalContext());
}

static bool
AreCompatibleVisuals(Visual* one, Visual* two)
{
    if (one->c_class != two->c_class) {
        return false;
    }

    if (one->red_mask != two->red_mask ||
        one->green_mask != two->green_mask ||
        one->blue_mask != two->blue_mask) {
        return false;
    }

    if (one->bits_per_rgb != two->bits_per_rgb) {
        return false;
    }

    return true;
}

static StaticRefPtr<GLContext> gGlobalContext;

already_AddRefed<GLContext>
GLContextProviderGLX::CreateWrappingExisting(void* aContext, void* aSurface)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nullptr;
    }

    if (aContext && aSurface) {
        SurfaceCaps caps = SurfaceCaps::Any();
        RefPtr<GLContextGLX> glContext =
            new GLContextGLX(CreateContextFlags::NONE, caps,
                             nullptr, // SharedContext
                             false, // Offscreen
                             (Display*)DefaultXDisplay(), // Display
                             (GLXDrawable)aSurface, (GLXContext)aContext,
                             false, // aDeleteDrawable,
                             true,
                             (gfxXlibSurface*)nullptr,
                             ContextProfile::OpenGLCompatibility);

        glContext->mOwnsContext = false;
        gGlobalContext = glContext;

        return glContext.forget();
    }

    return nullptr;
}

already_AddRefed<GLContext>
CreateForWidget(Display* aXDisplay, Window aXWindow, bool aForceAccelerated)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nullptr;
    }

    // Currently, we take whatever Visual the window already has, and
    // try to create an fbconfig for that visual.  This isn't
    // necessarily what we want in the long run; an fbconfig may not
    // be available for the existing visual, or if it is, the GL
    // performance might be suboptimal.  But using the existing visual
    // is a relatively safe intermediate step.

    if (!aXDisplay) {
        NS_ERROR("X Display required for GLX Context provider");
        return nullptr;
    }

    int xscreen = DefaultScreen(aXDisplay);

    ScopedXFree<GLXFBConfig> cfgs;
    GLXFBConfig config;
    int visid;
    if (!GLContextGLX::FindFBConfigForWindow(aXDisplay, xscreen, aXWindow, &cfgs,
                                             &config, &visid))
    {
        return nullptr;
    }

    SurfaceCaps caps = SurfaceCaps::Any();
    GLContextGLX* shareContext = GetGlobalContextGLX();
    RefPtr<GLContextGLX> gl = GLContextGLX::CreateGLContext(CreateContextFlags::NONE,
                                                            caps, shareContext, false,
                                                            aXDisplay, aXWindow, config,
                                                            false);
    return gl.forget();
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateForCompositorWidget(CompositorWidget* aCompositorWidget, bool aForceAccelerated)
{
    X11CompositorWidget* compWidget = aCompositorWidget->AsX11();
    MOZ_ASSERT(compWidget);

    return CreateForWidget(compWidget->XDisplay(),
                           compWidget->XWindow(),
                           aForceAccelerated);
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateForWindow(nsIWidget* aWidget, bool aForceAccelerated)
{
    Display* display = (Display*)aWidget->GetNativeData(NS_NATIVE_COMPOSITOR_DISPLAY);
    Window window = GET_NATIVE_WINDOW(aWidget);

    return CreateForWidget(display,
                           window,
                           aForceAccelerated);
}

static bool
ChooseConfig(GLXLibrary* glx, Display* display, int screen, const SurfaceCaps& minCaps,
             ScopedXFree<GLXFBConfig>* const out_scopedConfigArr,
             GLXFBConfig* const out_config, int* const out_visid)
{
    ScopedXFree<GLXFBConfig>& scopedConfigArr = *out_scopedConfigArr;

    if (minCaps.antialias)
        return false;

    int attribs[] = {
        LOCAL_GLX_DRAWABLE_TYPE, LOCAL_GLX_PIXMAP_BIT,
        LOCAL_GLX_X_RENDERABLE, True,
        LOCAL_GLX_RED_SIZE, 8,
        LOCAL_GLX_GREEN_SIZE, 8,
        LOCAL_GLX_BLUE_SIZE, 8,
        LOCAL_GLX_ALPHA_SIZE, minCaps.alpha ? 8 : 0,
        LOCAL_GLX_DEPTH_SIZE, minCaps.depth ? 16 : 0,
        LOCAL_GLX_STENCIL_SIZE, minCaps.stencil ? 8 : 0,
        0
    };

    int numConfigs = 0;
    scopedConfigArr = glx->xChooseFBConfig(display, screen, attribs, &numConfigs);
    if (!scopedConfigArr || !numConfigs)
        return false;

    // Issues with glxChooseFBConfig selection and sorting:
    // * ALPHA_SIZE is sorted as 'largest total RGBA bits first'. If we don't request
    //   alpha bits, we'll probably get RGBA anyways, since 32 is more than 24.
    // * DEPTH_SIZE is sorted largest first, including for `0` inputs.
    // * STENCIL_SIZE is smallest first, but it might return `8` even though we ask for
    //   `0`.

    // For now, we don't care about these. We *will* care when we do XPixmap sharing.

    for (int i = 0; i < numConfigs; ++i) {
        GLXFBConfig curConfig = scopedConfigArr[i];

        int visid;
        if (glx->xGetFBConfigAttrib(display, curConfig, LOCAL_GLX_VISUAL_ID, &visid)
            != Success)
        {
            continue;
        }

        if (!visid)
            continue;

        *out_config = curConfig;
        *out_visid = visid;
        return true;
    }

    return false;
}

bool
GLContextGLX::FindFBConfigForWindow(Display* display, int screen, Window window,
                                    ScopedXFree<GLXFBConfig>* const out_scopedConfigArr,
                                    GLXFBConfig* const out_config, int* const out_visid)
{
    ScopedXFree<GLXFBConfig>& cfgs = *out_scopedConfigArr;
    int numConfigs;
    if (sGLXLibrary.IsATI() ||
        !sGLXLibrary.GLXVersionCheck(1, 3)) {
        const int attribs[] = {
            LOCAL_GLX_DOUBLEBUFFER, False,
            0
        };
        cfgs = sGLXLibrary.xChooseFBConfig(display,
                                           screen,
                                           attribs,
                                           &numConfigs);
    } else {
        cfgs = sGLXLibrary.xGetFBConfigs(display,
                                         screen,
                                         &numConfigs);
    }

    if (!cfgs) {
        NS_WARNING("[GLX] glXGetFBConfigs() failed");
        return false;
    }
    NS_ASSERTION(numConfigs > 0, "No FBConfigs found!");

    // XXX the visual ID is almost certainly the LOCAL_GLX_FBCONFIG_ID, so
    // we could probably do this first and replace the glXGetFBConfigs
    // with glXChooseConfigs.  Docs are sparklingly clear as always.
    XWindowAttributes windowAttrs;
    if (!XGetWindowAttributes(display, window, &windowAttrs)) {
        NS_WARNING("[GLX] XGetWindowAttributes() failed");
        return false;
    }
    const VisualID windowVisualID = XVisualIDFromVisual(windowAttrs.visual);
#ifdef DEBUG
    printf("[GLX] window %lx has VisualID 0x%lx\n", window, windowVisualID);
#endif

    for (int i = 0; i < numConfigs; i++) {
        int visid = X11None;
        sGLXLibrary.xGetFBConfigAttrib(display, cfgs[i], LOCAL_GLX_VISUAL_ID, &visid);
        if (!visid) {
            continue;
        }
        if (sGLXLibrary.IsATI()) {
            int depth;
            Visual* visual;
            FindVisualAndDepth(display, visid, &visual, &depth);
            if (depth == windowAttrs.depth &&
                AreCompatibleVisuals(windowAttrs.visual, visual)) {
                *out_config = cfgs[i];
                *out_visid = visid;
                return true;
            }
        } else {
            if (windowVisualID == static_cast<VisualID>(visid)) {
                *out_config = cfgs[i];
                *out_visid = visid;
                return true;
            }
        }
    }

    NS_WARNING("[GLX] Couldn't find a FBConfig matching window visual");
    return false;
}

static already_AddRefed<GLContextGLX>
CreateOffscreenPixmapContext(CreateContextFlags flags, const IntSize& size,
                             const SurfaceCaps& minCaps, nsACString* const out_failureId,
                             ContextProfile profile = ContextProfile::OpenGLCompatibility)
{
    GLXLibrary* glx = &sGLXLibrary;
    if (!glx->EnsureInitialized())
        return nullptr;

    Display* display = DefaultXDisplay();
    int screen = DefaultScreen(display);

    ScopedXFree<GLXFBConfig> scopedConfigArr;
    GLXFBConfig config;
    int visid;
    if (!ChooseConfig(glx, display, screen, minCaps, &scopedConfigArr, &config, &visid)) {
        NS_WARNING("Failed to find a compatible config.");
        return nullptr;
    }

    Visual* visual;
    int depth;
    FindVisualAndDepth(display, visid, &visual, &depth);

    OffMainThreadScopedXErrorHandler xErrorHandler;
    bool error = false;

    Drawable drawable;
    GLXPixmap pixmap = 0;

    gfx::IntSize dummySize(16, 16);
    RefPtr<gfxXlibSurface> surface = gfxXlibSurface::Create(DefaultScreenOfDisplay(display),
                                                            visual,
                                                            dummySize);
    if (surface->CairoStatus() != 0) {
        mozilla::Unused << xErrorHandler.SyncAndGetError(display);
        return nullptr;
    }

    // Handle slightly different signature between glXCreatePixmap and
    // its pre-GLX-1.3 extension equivalent (though given the ABI, we
    // might not need to).
    drawable = surface->XDrawable();
    if (glx->GLXVersionCheck(1, 3)) {
        pixmap = glx->xCreatePixmap(display, config, drawable, nullptr);
    } else {
        pixmap = glx->xCreateGLXPixmapWithConfig(display, config, drawable);
    }

    if (pixmap == 0) {
        error = true;
    }

    bool serverError = xErrorHandler.SyncAndGetError(display);
    if (error || serverError)
        return nullptr;

    GLContextGLX* shareContext = GetGlobalContextGLX();
    return GLContextGLX::CreateGLContext(flags, minCaps, shareContext, true, display,
                                         pixmap, config, true, surface, profile);
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderGLX::CreateHeadless(CreateContextFlags flags,
                                     nsACString* const out_failureId)
{
    IntSize dummySize = IntSize(16, 16);
    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    return CreateOffscreenPixmapContext(flags, dummySize, dummyCaps, out_failureId);
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderGLX::CreateOffscreen(const IntSize& size,
                                      const SurfaceCaps& minCaps,
                                      CreateContextFlags flags,
                                      nsACString* const out_failureId)
{
    SurfaceCaps minBackbufferCaps = minCaps;
    if (minCaps.antialias) {
        minBackbufferCaps.antialias = false;
        minBackbufferCaps.depth = false;
        minBackbufferCaps.stencil = false;
    }

    ContextProfile profile = ContextProfile::OpenGLCore;
    if (flags & CreateContextFlags::REQUIRE_COMPAT_PROFILE) {
        profile = ContextProfile::OpenGLCompatibility;
    }

    RefPtr<GLContext> gl;
    gl = CreateOffscreenPixmapContext(flags, size, minBackbufferCaps, out_failureId,
                                      profile);
    if (!gl)
        return nullptr;

    if (!gl->InitOffscreen(size, minCaps)) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_GLX_INIT");
        return nullptr;
    }

    return gl.forget();
}

/*static*/ GLContext*
GLContextProviderGLX::GetGlobalContext()
{
    // TODO: get GLX context sharing to work well with multiple threads
    if (gfxEnv::DisableContextSharingGlx())
        return nullptr;

    static bool triedToCreateContext = false;
    if (!triedToCreateContext) {
        triedToCreateContext = true;

        MOZ_RELEASE_ASSERT(!gGlobalContext, "GFX: Global GL context already initialized.");
        nsCString discardFailureId;
        RefPtr<GLContext> temp = CreateHeadless(CreateContextFlags::NONE, &discardFailureId);
        gGlobalContext = temp;
    }

    return gGlobalContext;
}

/*static*/ void
GLContextProviderGLX::Shutdown()
{
    gGlobalContext = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */


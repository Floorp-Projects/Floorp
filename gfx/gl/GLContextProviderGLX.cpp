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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Woodrow <mwoodrow@mozilla.com>
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

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#define GET_NATIVE_WINDOW(aWidget) GDK_WINDOW_XID((GdkWindow *) aWidget->GetNativeData(NS_NATIVE_WINDOW))
#elif defined(MOZ_WIDGET_QT)
#include <QWidget>
#include <QX11Info>
#define GET_NATIVE_WINDOW(aWidget) static_cast<QWidget*>(aWidget->GetNativeData(NS_NATIVE_SHELLWIDGET))->handle()
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "mozilla/X11Util.h"

#include "prenv.h"
#include "GLContextProvider.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "GLXLibrary.h"
#include "gfxXlibSurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "GLContext.h"
#include "gfxUtils.h"

#include "gfxCrashReporterUtils.h"

namespace mozilla {
namespace gl {

static bool gIsATI = false;
static bool gIsChromium = false;
static int gGLXMajorVersion = 0, gGLXMinorVersion = 0;

// Check that we have at least version aMajor.aMinor .
static inline bool
GLXVersionCheck(int aMajor, int aMinor)
{
    return aMajor < gGLXMajorVersion ||
           (aMajor == gGLXMajorVersion && aMinor <= gGLXMinorVersion);
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

    if (!mOGLLibrary) {
        // see e.g. bug 608526: it is intrinsically interesting to know whether we have dynamically linked to libGL.so.1
        // because at least the NVIDIA implementation requires an executable stack, which causes mprotect calls,
        // which trigger glibc bug http://sourceware.org/bugzilla/show_bug.cgi?id=12225
#ifdef __OpenBSD__
        const char *libGLfilename = "libGL.so";
#else
        const char *libGLfilename = "libGL.so.1";
#endif
        ScopedGfxFeatureReporter reporter(libGLfilename);
        mOGLLibrary = PR_LoadLibrary(libGLfilename);
        if (!mOGLLibrary) {
            NS_WARNING("Couldn't load OpenGL shared library.");
            return false;
        }
        reporter.SetSuccessful();
    }

    if (PR_GetEnv("MOZ_GLX_DEBUG")) {
        mDebug = true;
    }

    GLLibraryLoader::SymLoadStruct symbols[] = {
        /* functions that were in GLX 1.0 */
        { (PRFuncPtr*) &xDestroyContextInternal, { "glXDestroyContext", NULL } },
        { (PRFuncPtr*) &xMakeCurrentInternal, { "glXMakeCurrent", NULL } },
        { (PRFuncPtr*) &xSwapBuffersInternal, { "glXSwapBuffers", NULL } },
        { (PRFuncPtr*) &xQueryVersionInternal, { "glXQueryVersion", NULL } },
        { (PRFuncPtr*) &xGetCurrentContextInternal, { "glXGetCurrentContext", NULL } },
        { (PRFuncPtr*) &xWaitGLInternal, { "glXWaitGL", NULL } },
        { (PRFuncPtr*) &xWaitXInternal, { "glXWaitX", NULL } },
        /* functions introduced in GLX 1.1 */
        { (PRFuncPtr*) &xQueryExtensionsStringInternal, { "glXQueryExtensionsString", NULL } },
        { (PRFuncPtr*) &xGetClientStringInternal, { "glXGetClientString", NULL } },
        { (PRFuncPtr*) &xQueryServerStringInternal, { "glXQueryServerString", NULL } },
        { NULL, { NULL } }
    };

    GLLibraryLoader::SymLoadStruct symbols13[] = {
        /* functions introduced in GLX 1.3 */
        { (PRFuncPtr*) &xChooseFBConfigInternal, { "glXChooseFBConfig", NULL } },
        { (PRFuncPtr*) &xGetFBConfigAttribInternal, { "glXGetFBConfigAttrib", NULL } },
        // WARNING: xGetFBConfigs not set in symbols13_ext
        { (PRFuncPtr*) &xGetFBConfigsInternal, { "glXGetFBConfigs", NULL } },
        { (PRFuncPtr*) &xGetVisualFromFBConfigInternal, { "glXGetVisualFromFBConfig", NULL } },
        // WARNING: symbols13_ext sets xCreateGLXPixmapWithConfig instead
        { (PRFuncPtr*) &xCreatePixmapInternal, { "glXCreatePixmap", NULL } },
        { (PRFuncPtr*) &xDestroyPixmapInternal, { "glXDestroyPixmap", NULL } },
        { (PRFuncPtr*) &xCreateNewContextInternal, { "glXCreateNewContext", NULL } },
        { NULL, { NULL } }
    };

    GLLibraryLoader::SymLoadStruct symbols13_ext[] = {
        /* extension equivalents for functions introduced in GLX 1.3 */
        // GLX_SGIX_fbconfig extension
        { (PRFuncPtr*) &xChooseFBConfigInternal, { "glXChooseFBConfigSGIX", NULL } },
        { (PRFuncPtr*) &xGetFBConfigAttribInternal, { "glXGetFBConfigAttribSGIX", NULL } },
        // WARNING: no xGetFBConfigs equivalent in extensions
        { (PRFuncPtr*) &xGetVisualFromFBConfigInternal, { "glXGetVisualFromFBConfig", NULL } },
        // WARNING: different from symbols13:
        { (PRFuncPtr*) &xCreateGLXPixmapWithConfigInternal, { "glXCreateGLXPixmapWithConfigSGIX", NULL } },
        { (PRFuncPtr*) &xDestroyPixmapInternal, { "glXDestroyGLXPixmap", NULL } }, // not from ext
        { (PRFuncPtr*) &xCreateNewContextInternal, { "glXCreateContextWithConfigSGIX", NULL } },
        { NULL, { NULL } }
    };

    GLLibraryLoader::SymLoadStruct symbols14[] = {
        /* functions introduced in GLX 1.4 */
        { (PRFuncPtr*) &xGetProcAddressInternal, { "glXGetProcAddress", NULL } },
        { NULL, { NULL } }
    };

    GLLibraryLoader::SymLoadStruct symbols14_ext[] = {
        /* extension equivalents for functions introduced in GLX 1.4 */
        // GLX_ARB_get_proc_address extension
        { (PRFuncPtr*) &xGetProcAddressInternal, { "glXGetProcAddressARB", NULL } },
        { NULL, { NULL } }
    };

    GLLibraryLoader::SymLoadStruct symbols_texturefrompixmap[] = {
        { (PRFuncPtr*) &xBindTexImageInternal, { "glXBindTexImageEXT", NULL } },
        { (PRFuncPtr*) &xReleaseTexImageInternal, { "glXReleaseTexImageEXT", NULL } },
        { NULL, { NULL } }
    };

    GLLibraryLoader::SymLoadStruct symbols_robustness[] = {
        { (PRFuncPtr*) &xCreateContextAttribsInternal, { "glXCreateContextAttribsARB", NULL } },
        { NULL, { NULL } }
    };

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &symbols[0])) {
        NS_WARNING("Couldn't find required entry point in OpenGL shared library");
        return false;
    }

    Display *display = DefaultXDisplay();

    int screen = DefaultScreen(display);
    const char *serverVendor = NULL;
    const char *serverVersionStr = NULL;
    const char *extensionsStr = NULL;

    if (!xQueryVersion(display, &gGLXMajorVersion, &gGLXMinorVersion)) {
        gGLXMajorVersion = 0;
        gGLXMinorVersion = 0;
        return false;
    }

    serverVendor = xQueryServerString(display, screen, GLX_VENDOR);
    serverVersionStr = xQueryServerString(display, screen, GLX_VERSION);

    if (!GLXVersionCheck(1, 1))
        // Not possible to query for extensions.
        return false;

    extensionsStr = xQueryExtensionsString(display, screen);

    GLLibraryLoader::SymLoadStruct *sym13;
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

    GLLibraryLoader::SymLoadStruct *sym14;
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
        mHasTextureFromPixmap = true;
    } else {
        NS_WARNING("Texture from pixmap disabled");
    }

    if (HasExtension(extensionsStr, "GLX_ARB_create_context_robustness") &&
        GLLibraryLoader::LoadSymbols(mOGLLibrary, symbols_robustness)) {
        mHasRobustness = true;
    }

    gIsATI = serverVendor && DoesStringMatch(serverVendor, "ATI");
    gIsChromium = (serverVendor &&
                   DoesStringMatch(serverVendor, "Chromium")) ||
        (serverVersionStr &&
         DoesStringMatch(serverVersionStr, "Chromium"));

    mInitialized = true;
    return true;
}

bool
GLXLibrary::SupportsTextureFromPixmap(gfxASurface* aSurface)
{
    if (!EnsureInitialized()) {
        return false;
    }
    
    if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib || !mHasTextureFromPixmap) {
        return false;
    }

    return true;
}

GLXPixmap 
GLXLibrary::CreatePixmap(gfxASurface* aSurface)
{
    if (!SupportsTextureFromPixmap(aSurface)) {
        return 0;
    }

    int attribs[] = { GLX_DOUBLEBUFFER, False,
                      GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
                      GLX_BIND_TO_TEXTURE_RGBA_EXT, True,
                      None };

    int numFormats;
    Display *display = DefaultXDisplay();
    int xscreen = DefaultScreen(display);

    ScopedXFree<GLXFBConfig> cfg(xChooseFBConfig(display,
                                                 xscreen,
                                                 attribs,
                                                 &numFormats));
    if (!cfg) {
        return 0;
    }
    NS_ABORT_IF_FALSE(numFormats > 0,
                 "glXChooseFBConfig() failed to match our requested format and violated its spec (!)");

    gfxXlibSurface *xs = static_cast<gfxXlibSurface*>(aSurface);

    int pixmapAttribs[] = { GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
                            GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
                            None};

    GLXPixmap glxpixmap = xCreatePixmap(display,
                                        cfg[0],
                                        xs->XDrawable(),
                                        pixmapAttribs);

    return glxpixmap;
}

void
GLXLibrary::DestroyPixmap(GLXPixmap aPixmap)
{
    if (!mHasTextureFromPixmap) {
        return;
    }

    Display *display = DefaultXDisplay();
    xDestroyPixmap(display, aPixmap);
}

void
GLXLibrary::BindTexImage(GLXPixmap aPixmap)
{    
    if (!mHasTextureFromPixmap) {
        return;
    }

    Display *display = DefaultXDisplay();
    // Make sure all X drawing to the surface has finished before binding to a texture.
    xWaitX();
    xBindTexImage(display, aPixmap, GLX_FRONT_LEFT_EXT, NULL);
}

void
GLXLibrary::ReleaseTexImage(GLXPixmap aPixmap)
{
    if (!mHasTextureFromPixmap) {
        return;
    }

    Display *display = DefaultXDisplay();
    xReleaseTexImage(display, aPixmap, GLX_FRONT_LEFT_EXT);
}

#ifdef DEBUG

static int (*sOldErrorHandler)(Display *, XErrorEvent *);
ScopedXErrorHandler::ErrorEvent sErrorEvent;
static int GLXErrorHandler(Display *display, XErrorEvent *ev)
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
        XSync(DefaultXDisplay(), False);
        if (sErrorEvent.mError.error_code) {
            char buffer[2048];
            XGetErrorText(DefaultXDisplay(), sErrorEvent.mError.error_code, buffer, sizeof(buffer));
            printf_stderr("X ERROR: %s (%i) - Request: %i.%i, Serial: %i",
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

#define BEFORE_GLX_CALL do {                     \
    sGLXLibrary.BeforeGLXCall();                 \
} while (0)
    
#define AFTER_GLX_CALL do {                      \
    sGLXLibrary.AfterGLXCall();                  \
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
GLXLibrary::xGetProcAddress(const char *procName)
{
    BEFORE_GLX_CALL;
    void* result = sGLXLibrary.xGetProcAddressInternal(procName);
    AFTER_GLX_CALL;
    return result;
}

GLXFBConfig*
GLXLibrary::xChooseFBConfig(Display* display, 
                            int screen, 
                            const int *attrib_list, 
                            int *nelements)
{
    BEFORE_GLX_CALL;
    GLXFBConfig* result = xChooseFBConfigInternal(display, screen, attrib_list, nelements);
    AFTER_GLX_CALL;
    return result;
}

GLXFBConfig* 
GLXLibrary::xGetFBConfigs(Display* display, 
                          int screen, 
                          int *nelements)
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

XVisualInfo*
GLXLibrary::xGetVisualFromFBConfig(Display* display, 
                                   GLXFBConfig config)
{
    BEFORE_GLX_CALL;
    XVisualInfo* result = xGetVisualFromFBConfigInternal(display, config);
    AFTER_GLX_CALL;
    return result;
}

int
GLXLibrary::xGetFBConfigAttrib(Display *display,
                               GLXFBConfig config,
                               int attribute,
                               int *value)
{
    BEFORE_GLX_CALL;
    int result = xGetFBConfigAttribInternal(display, config,
                                            attribute, value);
    AFTER_GLX_CALL;
    return result;
}

void
GLXLibrary::xSwapBuffers(Display *display, GLXDrawable drawable)
{
    BEFORE_GLX_CALL;
    xSwapBuffersInternal(display, drawable);
    AFTER_GLX_CALL;
}

const char *
GLXLibrary::xQueryExtensionsString(Display *display,
                                   int screen)
{
    BEFORE_GLX_CALL;
    const char *result = xQueryExtensionsStringInternal(display, screen);
    AFTER_GLX_CALL;
    return result;
}

const char *
GLXLibrary::xGetClientString(Display *display,
                             int screen)
{
    BEFORE_GLX_CALL;
    const char *result = xGetClientStringInternal(display, screen);
    AFTER_GLX_CALL;
    return result;
}

const char *
GLXLibrary::xQueryServerString(Display *display,
                               int screen, int name)
{
    BEFORE_GLX_CALL;
    const char *result = xQueryServerStringInternal(display, screen, name);
    AFTER_GLX_CALL;
    return result;
}

GLXPixmap
GLXLibrary::xCreatePixmap(Display *display, 
                          GLXFBConfig config,
                          Pixmap pixmap,
                          const int *attrib_list)
{
    BEFORE_GLX_CALL;
    GLXPixmap result = xCreatePixmapInternal(display, config,
                                             pixmap, attrib_list);
    AFTER_GLX_CALL;
    return result;
}

GLXPixmap
GLXLibrary::xCreateGLXPixmapWithConfig(Display *display,
                                       GLXFBConfig config,
                                       Pixmap pixmap)
{
    BEFORE_GLX_CALL;
    GLXPixmap result = xCreateGLXPixmapWithConfigInternal(display, config, pixmap);
    AFTER_GLX_CALL;
    return result;
}

void
GLXLibrary::xDestroyPixmap(Display *display, GLXPixmap pixmap)
{
    BEFORE_GLX_CALL;
    xDestroyPixmapInternal(display, pixmap);
    AFTER_GLX_CALL;
}

GLXContext
GLXLibrary::xCreateContext(Display *display,
                           XVisualInfo *vis,
                           GLXContext shareList,
                           Bool direct)
{
    BEFORE_GLX_CALL;
    GLXContext result = xCreateContextInternal(display, vis, shareList, direct);
    AFTER_GLX_CALL;
    return result;
}

Bool
GLXLibrary::xQueryVersion(Display *display,
                          int *major,
                          int *minor)
{
    BEFORE_GLX_CALL;
    Bool result = xQueryVersionInternal(display, major, minor);
    AFTER_GLX_CALL;
    return result;
}

void
GLXLibrary::xBindTexImage(Display *display,
                          GLXDrawable drawable,
                          int buffer,
                          const int *attrib_list)
{
    BEFORE_GLX_CALL;
    xBindTexImageInternal(display, drawable, buffer, attrib_list);
    AFTER_GLX_CALL;
}

void
GLXLibrary::xReleaseTexImage(Display *display,
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

GLXLibrary sGLXLibrary;

class GLContextGLX : public GLContext
{
public:
    static already_AddRefed<GLContextGLX>
    CreateGLContext(const ContextFormat& format,
                    Display *display,
                    GLXDrawable drawable,
                    GLXFBConfig cfg,
                    XVisualInfo *vinfo,
                    GLContextGLX *shareContext,
                    bool deleteDrawable,
                    gfxXlibSurface *pixmap = nsnull)
    {
        int db = 0, err;
        err = sGLXLibrary.xGetFBConfigAttrib(display, cfg,
                                             GLX_DOUBLEBUFFER, &db);
        if (GLX_BAD_ATTRIBUTE != err) {
#ifdef DEBUG
            printf("[GLX] FBConfig is %sdouble-buffered\n", db ? "" : "not ");
#endif
        }

        GLXContext context;
        nsRefPtr<GLContextGLX> glContext;
        bool error;

        ScopedXErrorHandler xErrorHandler;

TRY_AGAIN_NO_SHARING:

        error = false;

        if (sGLXLibrary.HasRobustness()) {
            int attrib_list[] = {
                LOCAL_GL_CONTEXT_FLAGS_ARB, LOCAL_GL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
                LOCAL_GL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_GL_LOSE_CONTEXT_ON_RESET_ARB,
                0,
            };

            context = sGLXLibrary.xCreateContextAttribs(display,
                                                        cfg,
                                                        shareContext ? shareContext->mContext : NULL,
                                                        True,
                                                        attrib_list);
        } else {
            context = sGLXLibrary.xCreateNewContext(display,
                                                    cfg,
                                                    GLX_RGBA_TYPE,
                                                    shareContext ? shareContext->mContext : NULL,
                                                    True);
        }

        if (context) {
            glContext = new GLContextGLX(format,
                                        shareContext,
                                        display,
                                        drawable,
                                        context,
                                        deleteDrawable,
                                        db,
                                        pixmap);
            if (!glContext->Init())
                error = true;
        } else {
            error = true;
        }

        error |= xErrorHandler.SyncAndGetError(display);

        if (error) {
            if (shareContext) {
                shareContext = nsnull;
                goto TRY_AGAIN_NO_SHARING;
            }

            NS_WARNING("Failed to create GLXContext!");
            glContext = nsnull; // note: this must be done while the graceful X error handler is set,
                                // because glxMakeCurrent can give a GLXBadDrawable error
        }

        return glContext.forget();
    }

    ~GLContextGLX()
    {
        MarkDestroyed();

        // see bug 659842 comment 76
#ifdef DEBUG
        bool success =
#endif
        sGLXLibrary.xMakeCurrent(mDisplay, None, nsnull);
        NS_ABORT_IF_FALSE(success,
            "glXMakeCurrent failed to release GL context before we call glXDestroyContext!");

        sGLXLibrary.xDestroyContext(mDisplay, mContext);

        if (mDeleteDrawable) {
            sGLXLibrary.xDestroyPixmap(mDisplay, mDrawable);
        }
    }

    GLContextType GetContextType() {
        return ContextTypeGLX;
    }

    bool Init()
    {
        MakeCurrent();
        SetupLookupFunction();
        if (!InitWithPrefix("gl", true)) {
            return false;
        }

        if (!IsExtensionSupported("GL_EXT_framebuffer_object"))
            return false;

        InitFramebuffers();

        return true;
    }

    bool MakeCurrentImpl(bool aForce = false)
    {
        bool succeeded = true;

        // With the ATI FGLRX driver, glxMakeCurrent is very slow even when the context doesn't change.
        // (This is not the case with other drivers such as NVIDIA).
        // So avoid calling it more than necessary. Since GLX documentation says that:
        //     "glXGetCurrentContext returns client-side information.
        //      It does not make a round trip to the server."
        // I assume that it's not worth using our own TLS slot here.
        if (aForce || sGLXLibrary.xGetCurrentContext() != mContext) {
            succeeded = sGLXLibrary.xMakeCurrent(mDisplay, mDrawable, mContext);
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
        }

        return succeeded;
    }

    bool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)&GLXLibrary::xGetProcAddress;
        return true;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch(aType) {
        case NativeGLContext:
            return mContext;
 
        case NativeThebesSurface:
            return mPixmap;

        default:
            return nsnull;
        }
    }

    bool IsDoubleBuffered()
    {
        return mDoubleBuffered;
    }

    bool SupportsRobustness()
    {
        return sGLXLibrary.HasRobustness();
    }

    bool SwapBuffers()
    {
        if (!mDoubleBuffered)
            return false;
        sGLXLibrary.xSwapBuffers(mDisplay, mDrawable);
        sGLXLibrary.xWaitGL();
        return true;
    }

    bool TextureImageSupportsGetBackingSurface()
    {
        return sGLXLibrary.HasTextureFromPixmap();
    }

    virtual already_AddRefed<TextureImage>
    CreateTextureImage(const nsIntSize& aSize,
                       TextureImage::ContentType aContentType,
                       GLenum aWrapMode,
                       bool aUseNearestFilter = false);

private:
    friend class GLContextProviderGLX;

    GLContextGLX(const ContextFormat& aFormat,
                 GLContext *aShareContext,
                 Display *aDisplay,
                 GLXDrawable aDrawable,
                 GLXContext aContext,
                 bool aDeleteDrawable,
                 bool aDoubleBuffered,
                 gfxXlibSurface *aPixmap)
        : GLContext(aFormat, aDeleteDrawable ? true : false, aShareContext),
          mContext(aContext),
          mDisplay(aDisplay),
          mDrawable(aDrawable),
          mDeleteDrawable(aDeleteDrawable),
          mDoubleBuffered(aDoubleBuffered),
          mPixmap(aPixmap)
    { }

    GLXContext mContext;
    Display *mDisplay;
    GLXDrawable mDrawable;
    bool mDeleteDrawable;
    bool mDoubleBuffered;

    nsRefPtr<gfxXlibSurface> mPixmap;
};

class TextureImageGLX : public TextureImage
{
    friend already_AddRefed<TextureImage>
    GLContextGLX::CreateTextureImage(const nsIntSize&,
                                     ContentType,
                                     GLenum,
                                     bool);

public:
    virtual ~TextureImageGLX()
    {
        mGLContext->MakeCurrent();
        mGLContext->fDeleteTextures(1, &mTexture);
        sGLXLibrary.DestroyPixmap(mPixmap);
    }

    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion)
    {
        mInUpdate = true;
        return mUpdateSurface;
    }

    virtual void EndUpdate()
    {
        mInUpdate = false;
    }


    virtual bool DirectUpdate(gfxASurface* aSurface, const nsIntRegion& aRegion, const nsIntPoint& aFrom)
    {
        nsRefPtr<gfxContext> ctx = new gfxContext(mUpdateSurface);
        gfxUtils::ClipToRegion(ctx, aRegion);
        ctx->SetSource(aSurface, aFrom);
        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx->Paint();
        return true;
    }

    virtual void BindTexture(GLenum aTextureUnit)
    {
        mGLContext->fActiveTexture(aTextureUnit);
        mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
        sGLXLibrary.BindTexImage(mPixmap);
        mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
    }

    virtual void ReleaseTexture()
    {
        sGLXLibrary.ReleaseTexImage(mPixmap);
    }

    virtual already_AddRefed<gfxASurface> GetBackingSurface()
    {
        nsRefPtr<gfxASurface> copy = mUpdateSurface;
        return copy.forget();
    }

    virtual bool InUpdate() const { return mInUpdate; }

    virtual GLuint GetTextureID() {
        return mTexture;
    };

private:
   TextureImageGLX(GLuint aTexture,
                   const nsIntSize& aSize,
                   GLenum aWrapMode,
                   ContentType aContentType,
                   GLContext* aContext,
                   gfxASurface* aSurface,
                   GLXPixmap aPixmap)
        : TextureImage(aSize, aWrapMode, aContentType)
        , mGLContext(aContext)
        , mUpdateSurface(aSurface)
        , mPixmap(aPixmap)
        , mInUpdate(false)
        , mTexture(aTexture)
    {
        if (aSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
            mShaderType = gl::RGBALayerProgramType;
        } else {
            mShaderType = gl::RGBXLayerProgramType;
        }
    }

    GLContext* mGLContext;
    nsRefPtr<gfxASurface> mUpdateSurface;
    GLXPixmap mPixmap;
    bool mInUpdate;
    GLuint mTexture;

    virtual void ApplyFilter()
    {
        mGLContext->ApplyFilterToBoundTexture(mFilter);
    }
};

already_AddRefed<TextureImage>
GLContextGLX::CreateTextureImage(const nsIntSize& aSize,
                                 TextureImage::ContentType aContentType,
                                 GLenum aWrapMode,
                                 bool aUseNearestFilter)
{
    if (!TextureImageSupportsGetBackingSurface()) {
        return GLContext::CreateTextureImage(aSize, 
                                             aContentType, 
                                             aWrapMode, 
                                             aUseNearestFilter);
    }

    Display *display = DefaultXDisplay();
    int xscreen = DefaultScreen(display);
    gfxASurface::gfxImageFormat imageFormat = gfxASurface::FormatFromContent(aContentType);

    XRenderPictFormat* xrenderFormat =
        gfxXlibSurface::FindRenderFormat(display, imageFormat);
    NS_ASSERTION(xrenderFormat, "Could not find a render format for our display!");


    nsRefPtr<gfxXlibSurface> surface =
        gfxXlibSurface::Create(ScreenOfDisplay(display, xscreen),
                               xrenderFormat,
                               gfxIntSize(aSize.width, aSize.height));
    NS_ASSERTION(surface, "Failed to create xlib surface!");

    if (aContentType == gfxASurface::CONTENT_COLOR_ALPHA) {
        nsRefPtr<gfxContext> ctx = new gfxContext(surface);
        ctx->SetOperator(gfxContext::OPERATOR_CLEAR);
        ctx->Paint();
    }

    MakeCurrent();
    GLXPixmap pixmap = sGLXLibrary.CreatePixmap(surface);
    NS_ASSERTION(pixmap, "Failed to create pixmap!");

    GLuint texture;
    fGenTextures(1, &texture);

    fActiveTexture(LOCAL_GL_TEXTURE0);
    fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

    nsRefPtr<TextureImageGLX> teximage =
        new TextureImageGLX(texture, aSize, aWrapMode, aContentType, this, surface, pixmap);

    GLint texfilter = aUseNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, aWrapMode);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, aWrapMode);

    return teximage.forget();
}

static GLContextGLX *
GetGlobalContextGLX()
{
    return static_cast<GLContextGLX*>(GLContextProviderGLX::GetGlobalContext());
}

static bool
AreCompatibleVisuals(XVisualInfo *one, XVisualInfo *two)
{
    if (one->c_class != two->c_class) {
        return false;
    }

    if (one->depth != two->depth) {
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

already_AddRefed<GLContext>
GLContextProviderGLX::CreateForWindow(nsIWidget *aWidget)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nsnull;
    }

    // Currently, we take whatever Visual the window already has, and
    // try to create an fbconfig for that visual.  This isn't
    // necessarily what we want in the long run; an fbconfig may not
    // be available for the existing visual, or if it is, the GL
    // performance might be suboptimal.  But using the existing visual
    // is a relatively safe intermediate step.

    Display *display = (Display*)aWidget->GetNativeData(NS_NATIVE_DISPLAY); 
    int xscreen = DefaultScreen(display);
    Window window = GET_NATIVE_WINDOW(aWidget);

    int numConfigs;
    ScopedXFree<GLXFBConfig> cfgs;
    if (gIsATI || !GLXVersionCheck(1, 3)) {
        const int attribs[] = {
            GLX_DOUBLEBUFFER, False,
            0
        };
        cfgs = sGLXLibrary.xChooseFBConfig(display,
                                           xscreen,
                                           attribs,
                                           &numConfigs);
    } else {
        cfgs = sGLXLibrary.xGetFBConfigs(display,
                                         xscreen,
                                         &numConfigs);
    }

    if (!cfgs) {
        NS_WARNING("[GLX] glXGetFBConfigs() failed");
        return nsnull;
    }
    NS_ASSERTION(numConfigs > 0, "No FBConfigs found!");

    // XXX the visual ID is almost certainly the GLX_FBCONFIG_ID, so
    // we could probably do this first and replace the glXGetFBConfigs
    // with glXChooseConfigs.  Docs are sparklingly clear as always.
    XWindowAttributes widgetAttrs;
    if (!XGetWindowAttributes(display, window, &widgetAttrs)) {
        NS_WARNING("[GLX] XGetWindowAttributes() failed");
        return nsnull;
    }
    const VisualID widgetVisualID = XVisualIDFromVisual(widgetAttrs.visual);
#ifdef DEBUG
    printf("[GLX] widget has VisualID 0x%lx\n", widgetVisualID);
#endif

    ScopedXFree<XVisualInfo> vi;
    if (gIsATI) {
        XVisualInfo vinfo_template;
        int nvisuals;
        vinfo_template.visual   = widgetAttrs.visual;
        vinfo_template.visualid = XVisualIDFromVisual(vinfo_template.visual);
        vinfo_template.depth    = widgetAttrs.depth;
        vinfo_template.screen   = xscreen;
        vi = XGetVisualInfo(display, VisualIDMask|VisualDepthMask|VisualScreenMask,
                            &vinfo_template, &nvisuals);
        NS_ASSERTION(vi && nvisuals == 1, "Could not locate unique matching XVisualInfo for Visual");
    }

    int matchIndex = -1;
    ScopedXFree<XVisualInfo> vinfo;

    for (int i = 0; i < numConfigs; i++) {
        vinfo = sGLXLibrary.xGetVisualFromFBConfig(display, cfgs[i]);
        if (!vinfo) {
            continue;
        }
        if (gIsATI) {
            if (AreCompatibleVisuals(vi, vinfo)) {
                matchIndex = i;
                break;
            }
        } else {
            if (widgetVisualID == vinfo->visualid) {
                matchIndex = i;
                break;
            }
        }
    }

    if (matchIndex == -1) {
        NS_WARNING("[GLX] Couldn't find a FBConfig matching widget visual");
        return nsnull;
    }

    GLContextGLX *shareContext = GetGlobalContextGLX();

    nsRefPtr<GLContextGLX> glContext = GLContextGLX::CreateGLContext(ContextFormat(ContextFormat::BasicRGB24),
                                                                     display,
                                                                     window,
                                                                     cfgs[matchIndex],
                                                                     vinfo,
                                                                     shareContext,
                                                                     false);

    return glContext.forget();
}

static already_AddRefed<GLContextGLX>
CreateOffscreenPixmapContext(const gfxIntSize& aSize,
                             const ContextFormat& aFormat,
                             bool aShare)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nsnull;
    }

    Display *display = DefaultXDisplay();
    int xscreen = DefaultScreen(display);

    int attribs[] = {
        GLX_DOUBLEBUFFER, False,
        GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
        GLX_X_RENDERABLE, True,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 0,
        GLX_DEPTH_SIZE, 0,
        0
    };
    int numConfigs = 0;

    ScopedXFree<GLXFBConfig> cfgs;
    cfgs = sGLXLibrary.xChooseFBConfig(display,
                                       xscreen,
                                       attribs,
                                       &numConfigs);
    if (!cfgs) {
        return nsnull;
    }

    NS_ASSERTION(numConfigs > 0,
                 "glXChooseFBConfig() failed to match our requested format and violated its spec (!)");

    ScopedXFree<XVisualInfo> vinfo;
    int chosenIndex = 0;

    for (int i = 0; i < numConfigs; ++i) {
        int dtype, visid;

        if (sGLXLibrary.xGetFBConfigAttrib(display, cfgs[i], GLX_DRAWABLE_TYPE, &dtype) != Success
            || !(dtype & GLX_PIXMAP_BIT))
        {
            continue;
        }
        if (sGLXLibrary.xGetFBConfigAttrib(display, cfgs[i], GLX_VISUAL_ID, &visid) != Success
            || visid == 0)
        {
            continue;
        }

        vinfo = sGLXLibrary.xGetVisualFromFBConfig(display, cfgs[i]);

        if (vinfo) {
            chosenIndex = i;
            break;
        }
    }

    if (!vinfo) {
        NS_WARNING("glXChooseFBConfig() didn't give us any configs with visuals!");
        return nsnull;
    }

    ScopedXErrorHandler xErrorHandler;
    GLXPixmap glxpixmap = 0;
    bool error = false;

    nsRefPtr<gfxXlibSurface> xsurface = gfxXlibSurface::Create(DefaultScreenOfDisplay(display),
                                                               vinfo->visual,
                                                               gfxIntSize(16, 16));
    if (xsurface->CairoStatus() != 0) {
        error = true;
        goto DONE_CREATING_PIXMAP;
    }

    // Handle slightly different signature between glXCreatePixmap and
    // its pre-GLX-1.3 extension equivalent (though given the ABI, we
    // might not need to).
    if (GLXVersionCheck(1, 3)) {
        glxpixmap = sGLXLibrary.xCreatePixmap(display,
                                              cfgs[chosenIndex],
                                              xsurface->XDrawable(),
                                              NULL);
    } else {
        glxpixmap = sGLXLibrary.xCreateGLXPixmapWithConfig(display,
                                                           cfgs[chosenIndex],
                                                           xsurface->
                                                             XDrawable());
    }
    if (glxpixmap == 0) {
        error = true;
    }

DONE_CREATING_PIXMAP:

    nsRefPtr<GLContextGLX> glContext;
    bool serverError = xErrorHandler.SyncAndGetError(display);

    if (!error && // earlier recorded error
        !serverError)
    {
        glContext = GLContextGLX::CreateGLContext(
                        aFormat,
                        display,
                        glxpixmap,
                        cfgs[chosenIndex],
                        vinfo,
                        aShare ? GetGlobalContextGLX() : nsnull,
                        true,
                        xsurface);
    }

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateOffscreen(const gfxIntSize& aSize,
                                      const ContextFormat& aFormat,
                                      const ContextFlags)
{
    nsRefPtr<GLContextGLX> glContext =
        CreateOffscreenPixmapContext(aSize, aFormat, true);

    if (!glContext) {
        return nsnull;
    }

    if (!glContext->GetSharedContext()) {
        // no point in returning anything if sharing failed, we can't
        // render from this
        return nsnull;
    }

    if (!glContext->ResizeOffscreenFBO(aSize, true)) {
        // we weren't able to create the initial
        // offscreen FBO, so this is dead
        return nsnull;
    }

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateForNativePixmapSurface(gfxASurface *aSurface)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nsnull;
    }

    if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib) {
        NS_WARNING("GLContextProviderGLX::CreateForNativePixmapSurface called with non-Xlib surface");
        return nsnull;
    }

    nsAutoTArray<int, 20> attribs;

#define A1_(_x)  do { attribs.AppendElement(_x); } while(0)
#define A2_(_x,_y)  do {                                                \
        attribs.AppendElement(_x);                                      \
        attribs.AppendElement(_y);                                      \
    } while(0)

    A2_(GLX_DOUBLEBUFFER, False);
    A2_(GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT);
    A1_(0);

    int numFormats;
    Display *display = DefaultXDisplay();
    int xscreen = DefaultScreen(display);

    ScopedXFree<GLXFBConfig> cfg(sGLXLibrary.xChooseFBConfig(display,
                                                             xscreen,
                                                             attribs.Elements(),
                                                             &numFormats));
    if (!cfg) {
        return nsnull;
    }
    NS_ASSERTION(numFormats > 0,
                 "glXChooseFBConfig() failed to match our requested format and violated its spec (!)");

    gfxXlibSurface *xs = static_cast<gfxXlibSurface*>(aSurface);

    GLXPixmap glxpixmap = sGLXLibrary.xCreatePixmap(display,
                                                    cfg[0],
                                                    xs->XDrawable(),
                                                    NULL);

    nsRefPtr<GLContextGLX> glContext = GLContextGLX::CreateGLContext(ContextFormat(ContextFormat::BasicRGB24),
                                                                     display,
                                                                     glxpixmap,
                                                                     cfg[0],
                                                                     NULL,
                                                                     NULL,
                                                                     false,
                                                                     xs);

    return glContext.forget();
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext *
GLContextProviderGLX::GetGlobalContext()
{
    static bool triedToCreateContext = false;
    if (!triedToCreateContext && !gGlobalContext) {
        triedToCreateContext = true;
        gGlobalContext = CreateOffscreenPixmapContext(gfxIntSize(1, 1),
                                                      ContextFormat(ContextFormat::BasicRGB24),
                                                      false);
        if (gGlobalContext)
            gGlobalContext->SetIsGlobalSharedContext(true);
    }

    return gGlobalContext;
}

void
GLContextProviderGLX::Shutdown()
{
    gGlobalContext = nsnull;
}

} /* namespace gl */
} /* namespace mozilla */


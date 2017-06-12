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
#include "mozilla/layers/CompositorOptions.h"
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

#define SYMBOL(X) { (PRFuncPtr*)&mSymbols.f##X, { "glX" #X, nullptr } }
#define END_OF_SYMBOLS { nullptr, { nullptr } }

    const GLLibraryLoader::SymLoadStruct symbols[] = {
        /* functions that were in GLX 1.0 */
        SYMBOL(DestroyContext),
        SYMBOL(MakeCurrent),
        SYMBOL(SwapBuffers),
        SYMBOL(QueryVersion),
        SYMBOL(GetCurrentContext),
        SYMBOL(WaitGL),
        SYMBOL(WaitX),

        /* functions introduced in GLX 1.1 */
        SYMBOL(QueryExtensionsString),
        SYMBOL(GetClientString),
        SYMBOL(QueryServerString),

        /* functions introduced in GLX 1.3 */
        SYMBOL(ChooseFBConfig),
        SYMBOL(GetFBConfigAttrib),
        SYMBOL(GetFBConfigs),
        SYMBOL(CreatePixmap),
        SYMBOL(DestroyPixmap),
        SYMBOL(CreateNewContext),

        // Core in GLX 1.4, ARB extension before.
        { (PRFuncPtr*)&mSymbols.fGetProcAddress, { "glXGetProcAddress",
                                                   "glXGetProcAddressARB",
                                                   nullptr } },
        END_OF_SYMBOLS
    };
    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, symbols)) {
        NS_WARNING("Couldn't load required GLX symbols.");
        return false;
    }

    Display* display = DefaultXDisplay();
    int screen = DefaultScreen(display);

    {
        int major, minor;
        if (!fQueryVersion(display, &major, &minor) ||
            major != 1 || minor < 3)
        {
            NS_ERROR("GLX version older than 1.3. (released in 1998)");
            return false;
        }
    }

    const GLLibraryLoader::SymLoadStruct symbols_texturefrompixmap[] = {
        SYMBOL(BindTexImageEXT),
        SYMBOL(ReleaseTexImageEXT),
        END_OF_SYMBOLS
    };

    const GLLibraryLoader::SymLoadStruct symbols_createcontext[] = {
        SYMBOL(CreateContextAttribsARB),
        END_OF_SYMBOLS
    };

    const GLLibraryLoader::SymLoadStruct symbols_videosync[] = {
        SYMBOL(GetVideoSyncSGI),
        SYMBOL(WaitVideoSyncSGI),
        END_OF_SYMBOLS
    };

    const GLLibraryLoader::SymLoadStruct symbols_swapcontrol[] = {
        SYMBOL(SwapIntervalEXT),
        END_OF_SYMBOLS
    };

    const auto lookupFunction =
        (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress;

    const auto fnLoadSymbols = [&](const GLLibraryLoader::SymLoadStruct* symbols) {
        if (GLLibraryLoader::LoadSymbols(mOGLLibrary, symbols, lookupFunction))
            return true;

        GLLibraryLoader::ClearSymbols(symbols);
        return false;
    };

    const char* clientVendor = fGetClientString(display, LOCAL_GLX_VENDOR);
    const char* serverVendor = fQueryServerString(display, screen, LOCAL_GLX_VENDOR);
    const char* extensionsStr = fQueryExtensionsString(display, screen);

    if (HasExtension(extensionsStr, "GLX_EXT_texture_from_pixmap") &&
        fnLoadSymbols(symbols_texturefrompixmap))
    {
        mUseTextureFromPixmap = gfxPrefs::UseGLXTextureFromPixmap();
    } else {
        mUseTextureFromPixmap = false;
        NS_WARNING("Texture from pixmap disabled");
    }

    if (HasExtension(extensionsStr, "GLX_ARB_create_context") &&
        HasExtension(extensionsStr, "GLX_ARB_create_context_profile") &&
        fnLoadSymbols(symbols_createcontext))
    {
        mHasCreateContextAttribs = true;
    }

    if (HasExtension(extensionsStr, "GLX_ARB_create_context_robustness")) {
        mHasRobustness = true;
    }

    if (HasExtension(extensionsStr, "GLX_SGI_video_sync") &&
        fnLoadSymbols(symbols_videosync))
    {
        mHasVideoSync = true;
    }

    if (!HasExtension(extensionsStr, "GLX_EXT_swap_control") ||
        !fnLoadSymbols(symbols_swapcontrol))
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

    ScopedXFree<GLXFBConfig> cfgs(fChooseFBConfig(display,
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
        sGLXLibrary.fGetFBConfigAttrib(display, cfgs[i], LOCAL_GLX_VISUAL_ID, &id);
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
            sGLXLibrary.fGetFBConfigAttrib(display, cfgs[i],
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

    GLXPixmap glxpixmap = fCreatePixmap(display,
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

    fDestroyPixmap(aDisplay, aPixmap);
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
        fWaitX();
    }
    fBindTexImage(aDisplay, aPixmap, LOCAL_GLX_FRONT_LEFT_EXT, nullptr);
}

void
GLXLibrary::ReleaseTexImage(Display* aDisplay, GLXPixmap aPixmap)
{
    if (!mUseTextureFromPixmap) {
        return;
    }

    fReleaseTexImage(aDisplay, aPixmap, LOCAL_GLX_FRONT_LEFT_EXT);
}

void
GLXLibrary::UpdateTexImage(Display* aDisplay, GLXPixmap aPixmap)
{
    // NVIDIA drivers don't require a rebind of the pixmap in order
    // to display an updated image, and it's faster not to do it.
    if (mIsNVIDIA) {
        fWaitX();
        return;
    }

    ReleaseTexImage(aDisplay, aPixmap);
    BindTexImage(aDisplay, aPixmap);
}

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
GLXLibrary::BeforeGLXCall() const
{
    if (mDebug) {
        sOldErrorHandler = XSetErrorHandler(GLXErrorHandler);
    }
}

void
GLXLibrary::AfterGLXCall() const
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

already_AddRefed<GLContextGLX>
GLContextGLX::CreateGLContext(CreateContextFlags flags, const SurfaceCaps& caps,
                              bool isOffscreen, Display* display, GLXDrawable drawable,
                              GLXFBConfig cfg, bool deleteDrawable,
                              gfxXlibSurface* pixmap)
{
    GLXLibrary& glx = sGLXLibrary;

    int db = 0;
    int err = glx.fGetFBConfigAttrib(display, cfg,
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

        if (glx.HasCreateContextAttribs()) {
            AutoTArray<int, 11> attrib_list;
            if (glx.HasRobustness()) {
                const int robust_attribs[] = {
                    LOCAL_GLX_CONTEXT_FLAGS_ARB,
                    LOCAL_GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB,
                    LOCAL_GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB,
                    LOCAL_GLX_LOSE_CONTEXT_ON_RESET_ARB,
                };
                attrib_list.AppendElements(robust_attribs, MOZ_ARRAY_LENGTH(robust_attribs));
            }
            if (!(flags & CreateContextFlags::REQUIRE_COMPAT_PROFILE)) {
                int core_attribs[] = {
                    LOCAL_GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                    LOCAL_GLX_CONTEXT_MINOR_VERSION_ARB, 2,
                    LOCAL_GLX_CONTEXT_PROFILE_MASK_ARB, LOCAL_GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                };
                attrib_list.AppendElements(core_attribs, MOZ_ARRAY_LENGTH(core_attribs));
            };
            attrib_list.AppendElement(0);

            context = glx.fCreateContextAttribs(
                display,
                cfg,
                nullptr,
                True,
                attrib_list.Elements());
        } else {
            context = glx.fCreateNewContext(
                display,
                cfg,
                LOCAL_GLX_RGBA_TYPE,
                nullptr,
                True);
        }

        if (context) {
            glContext = new GLContextGLX(flags, caps, isOffscreen, display, drawable,
                                         context, deleteDrawable, db, pixmap);
            if (!glContext->Init())
                error = true;
        } else {
            error = true;
        }

        error |= xErrorHandler.SyncAndGetError(display);

        if (error) {
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
    mGLX->fMakeCurrent(mDisplay, X11None, nullptr);
    MOZ_ASSERT(success,
               "glXMakeCurrent failed to release GL context before we call "
               "glXDestroyContext!");

    mGLX->fDestroyContext(mDisplay, mContext);

    if (mDeleteDrawable) {
        mGLX->fDestroyPixmap(mDisplay, mDrawable);
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
    if (aForce || mGLX->fGetCurrentContext() != mContext) {
        if (mGLX->IsMesa()) {
          // Read into the event queue to ensure that Mesa receives a
          // DRI2InvalidateBuffers event before drawing. See bug 1280653.
          Unused << XPending(mDisplay);
        }

        succeeded = mGLX->fMakeCurrent(mDisplay, mDrawable, mContext);
        NS_ASSERTION(succeeded, "Failed to make GL context current!");

        if (!IsOffscreen() && mGLX->SupportsSwapControl()) {
            // Many GLX implementations default to blocking until the next
            // VBlank when calling glXSwapBuffers. We want to run unthrottled
            // in ASAP mode. See bug 1280744.
            const bool isASAP = (gfxPrefs::LayoutFrameRate() == 0);
            mGLX->fSwapInterval(mDisplay, mDrawable, isASAP ? 0 : 1);
        }
    }

    return succeeded;
}

bool
GLContextGLX::IsCurrent() {
    return mGLX->fGetCurrentContext() == mContext;
}

bool
GLContextGLX::SetupLookupFunction()
{
    mLookupFunc = (PlatformLookupFunction)sGLXLibrary.GetGetProcAddress();
    return true;
}

bool
GLContextGLX::IsDoubleBuffered() const
{
    return mDoubleBuffered;
}

bool
GLContextGLX::SwapBuffers()
{
    if (!mDoubleBuffered)
        return false;
    mGLX->fSwapBuffers(mDisplay, mDrawable);
    return true;
}

void
GLContextGLX::GetWSIInfo(nsCString* const out) const
{
    Display* display = DefaultXDisplay();
    int screen = DefaultScreen(display);

    int majorVersion, minorVersion;
    sGLXLibrary.fQueryVersion(display, &majorVersion, &minorVersion);

    out->Append(nsPrintfCString("GLX %u.%u", majorVersion, minorVersion));

    out->AppendLiteral("\nGLX_VENDOR(client): ");
    out->Append(sGLXLibrary.fGetClientString(display, LOCAL_GLX_VENDOR));

    out->AppendLiteral("\nGLX_VENDOR(server): ");
    out->Append(sGLXLibrary.fQueryServerString(display, screen, LOCAL_GLX_VENDOR));

    out->AppendLiteral("\nExtensions: ");
    out->Append(sGLXLibrary.fQueryExtensionsString(display, screen));
}

bool
GLContextGLX::OverrideDrawable(GLXDrawable drawable)
{
    if (Screen())
        Screen()->AssureBlitted();
    Bool result = mGLX->fMakeCurrent(mDisplay, drawable, mContext);
    return result;
}

bool
GLContextGLX::RestoreDrawable()
{
    return mGLX->fMakeCurrent(mDisplay, mDrawable, mContext);
}

GLContextGLX::GLContextGLX(
                  CreateContextFlags flags,
                  const SurfaceCaps& caps,
                  bool isOffscreen,
                  Display* aDisplay,
                  GLXDrawable aDrawable,
                  GLXContext aContext,
                  bool aDeleteDrawable,
                  bool aDoubleBuffered,
                  gfxXlibSurface* aPixmap)
    : GLContext(flags, caps, nullptr, isOffscreen),
      mContext(aContext),
      mDisplay(aDisplay),
      mDrawable(aDrawable),
      mDeleteDrawable(aDeleteDrawable),
      mDoubleBuffered(aDoubleBuffered),
      mGLX(&sGLXLibrary),
      mPixmap(aPixmap),
      mOwnsContext(true)
{
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
                             false, // Offscreen
                             (Display*)DefaultXDisplay(), // Display
                             (GLXDrawable)aSurface, (GLXContext)aContext,
                             false, // aDeleteDrawable,
                             true,
                             (gfxXlibSurface*)nullptr);

        glContext->mOwnsContext = false;
        return glContext.forget();
    }

    return nullptr;
}

already_AddRefed<GLContext>
CreateForWidget(Display* aXDisplay, Window aXWindow,
                bool aWebRender,
                bool aForceAccelerated)
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
                                             &config, &visid, aWebRender))
    {
        return nullptr;
    }

    CreateContextFlags flags;
    if (aWebRender) {
        flags = CreateContextFlags::NONE; // WR needs GL3.2+
    } else {
        flags = CreateContextFlags::REQUIRE_COMPAT_PROFILE;
    }
    return GLContextGLX::CreateGLContext(flags, SurfaceCaps::Any(), false, aXDisplay,
                                         aXWindow, config, false, nullptr);
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateForCompositorWidget(CompositorWidget* aCompositorWidget, bool aForceAccelerated)
{
    X11CompositorWidget* compWidget = aCompositorWidget->AsX11();
    MOZ_ASSERT(compWidget);

    return CreateForWidget(compWidget->XDisplay(),
                           compWidget->XWindow(),
                           compWidget->GetCompositorOptions().UseWebRender(),
                           aForceAccelerated);
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateForWindow(nsIWidget* aWidget,
                                      bool aWebRender,
                                      bool aForceAccelerated)
{
    Display* display = (Display*)aWidget->GetNativeData(NS_NATIVE_COMPOSITOR_DISPLAY);
    Window window = GET_NATIVE_WINDOW(aWidget);

    return CreateForWidget(display,
                           window,
                           aWebRender,
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
    scopedConfigArr = glx->fChooseFBConfig(display, screen, attribs, &numConfigs);
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
        if (glx->fGetFBConfigAttrib(display, curConfig, LOCAL_GLX_VISUAL_ID, &visid)
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
                                    GLXFBConfig* const out_config, int* const out_visid,
                                    bool aWebRender)
{
    ScopedXFree<GLXFBConfig>& cfgs = *out_scopedConfigArr;
    int numConfigs;
    const int webrenderAttribs[] = {
        LOCAL_GLX_DEPTH_SIZE, 24,
        LOCAL_GLX_DOUBLEBUFFER, True,
        0
    };

    if (aWebRender) {
      cfgs = sGLXLibrary.fChooseFBConfig(display,
                                         screen,
                                         webrenderAttribs,
                                         &numConfigs);
    } else {
      cfgs = sGLXLibrary.fGetFBConfigs(display,
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

    if (aWebRender) {
        for (int i = 0; i < numConfigs; i++) {
            int visid = X11None;
            sGLXLibrary.fGetFBConfigAttrib(display, cfgs[i], LOCAL_GLX_VISUAL_ID, &visid);
            if (!visid) {
                continue;
            }

            int depth;
            Visual* visual;
            FindVisualAndDepth(display, visid, &visual, &depth);
            if (depth == windowAttrs.depth &&
                AreCompatibleVisuals(windowAttrs.visual, visual)) {
                *out_config = cfgs[i];
                *out_visid = visid;
                return true;
            }
        }
    } else {
        for (int i = 0; i < numConfigs; i++) {
            int visid = X11None;
            sGLXLibrary.fGetFBConfigAttrib(display, cfgs[i], LOCAL_GLX_VISUAL_ID, &visid);
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
    }

    NS_WARNING("[GLX] Couldn't find a FBConfig matching window visual");
    return false;
}

static already_AddRefed<GLContextGLX>
CreateOffscreenPixmapContext(CreateContextFlags flags, const IntSize& size,
                             const SurfaceCaps& minCaps, nsACString* const out_failureId)
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
    const auto drawable = surface->XDrawable();
    const auto pixmap = glx->fCreatePixmap(display, config, drawable, nullptr);
    if (pixmap == 0) {
        error = true;
    }

    bool serverError = xErrorHandler.SyncAndGetError(display);
    if (error || serverError)
        return nullptr;

    return GLContextGLX::CreateGLContext(flags, minCaps, true, display, pixmap, config,
                                         true, surface);
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

    RefPtr<GLContext> gl;
    gl = CreateOffscreenPixmapContext(flags, size, minBackbufferCaps, out_failureId);
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
    // Context sharing not supported.
    return nullptr;
}

/*static*/ void
GLContextProviderGLX::Shutdown()
{
}

} /* namespace gl */
} /* namespace mozilla */


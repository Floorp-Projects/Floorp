/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"
// please add new includes below Qt, otherwise it break Qt build due malloc wrapper conflicts

#if defined(XP_UNIX)

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdkx.h>
// we're using default display for now
#define GET_NATIVE_WINDOW(aWidget) (EGLNativeWindowType)GDK_WINDOW_XID((GdkWindow *) aWidget->GetNativeData(NS_NATIVE_WINDOW))
#elif defined(MOZ_WIDGET_QT)
#include <QtOpenGL/QGLContext>
#define GLdouble_defined 1
// we're using default display for now
#define GET_NATIVE_WINDOW(aWidget) (EGLNativeWindowType)static_cast<QWidget*>(aWidget->GetNativeData(NS_NATIVE_SHELLWIDGET))->winId()
#elif defined(MOZ_WIDGET_GONK)
#define GET_NATIVE_WINDOW(aWidget) ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_WINDOW))
#endif

#if defined(MOZ_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "mozilla/X11Util.h"
#include "gfxXlibSurface.h"
#endif

#if defined(ANDROID)
/* from widget */
#if defined(MOZ_WIDGET_ANDROID)
#include "AndroidBridge.h"
#include "nsSurfaceTexture.h"
#endif
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)

# if defined(MOZ_WIDGET_GONK)
#  include "cutils/properties.h"
#  include <ui/GraphicBuffer.h>

using namespace android;

# define EGL_NATIVE_BUFFER_ANDROID 0x3140

# endif

#endif

#define GLES2_LIB "libGLESv2.so"
#define GLES2_LIB2 "libGLESv2.so.2"

#elif defined(XP_WIN)

#include "nsIFile.h"

#define GLES2_LIB "libGLESv2.dll"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include <windows.h>

// a little helper
class AutoDestroyHWND {
public:
    AutoDestroyHWND(HWND aWnd = NULL)
        : mWnd(aWnd)
    {
    }

    ~AutoDestroyHWND() {
        if (mWnd) {
            ::DestroyWindow(mWnd);
        }
    }

    operator HWND() {
        return mWnd;
    }

    HWND forget() {
        HWND w = mWnd;
        mWnd = NULL;
        return w;
    }

    HWND operator=(HWND aWnd) {
        if (mWnd && mWnd != aWnd) {
            ::DestroyWindow(mWnd);
        }
        mWnd = aWnd;
        return mWnd;
    }

    HWND mWnd;
};

#else

#error "Platform not recognized"

#endif

#include "mozilla/Preferences.h"
#include "gfxUtils.h"
#include "gfxFailure.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "GLContextProvider.h"
#include "GLLibraryEGL.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"

#include "nsIWidget.h"

#include "gfxCrashReporterUtils.h"


#if defined(MOZ_PLATFORM_MAEMO) || defined(MOZ_WIDGET_GONK)
static bool gUseBackingSurface = true;
#else
static bool gUseBackingSurface = false;
#endif

#ifdef MOZ_WIDGET_GONK
extern nsIntRect gScreenBounds;
#endif

namespace mozilla {
namespace gl {

static GLLibraryEGL sEGLLibrary;

#define ADD_ATTR_2(_array, _k, _v) do {         \
    (_array).AppendElement(_k);                 \
    (_array).AppendElement(_v);                 \
} while (0)

#define ADD_ATTR_1(_array, _k) do {             \
    (_array).AppendElement(_k);                 \
} while (0)

#ifndef MOZ_JAVA_COMPOSITOR
static EGLSurface
CreateSurfaceForWindow(nsIWidget *aWidget, EGLConfig config);
#endif

static bool
CreateConfig(EGLConfig* aConfig);
#ifdef MOZ_X11

#ifdef MOZ_EGL_XRENDER_COMPOSITE
static EGLSurface
CreateBasicEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig);
#endif

static EGLConfig
CreateEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig = nsnull);
#endif

static EGLint gContextAttribs[] = {
    LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
    LOCAL_EGL_NONE
};

static EGLint gContextAttribsRobustness[] = {
    LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
    //LOCAL_EGL_CONTEXT_ROBUST_ACCESS_EXT, LOCAL_EGL_TRUE,
    LOCAL_EGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_EXT, LOCAL_EGL_LOSE_CONTEXT_ON_RESET_EXT,
    LOCAL_EGL_NONE
};

static int
next_power_of_two(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

static bool
is_power_of_two(int v)
{
    NS_ASSERTION(v >= 0, "bad value");

    if (v == 0)
        return true;

    return (v & (v-1)) == 0;
}

class GLContextEGL : public GLContext
{
    friend class TextureImageEGL;

    static already_AddRefed<GLContextEGL>
    CreateGLContext(const ContextFormat& format,
                    EGLSurface surface,
                    EGLConfig config,
                    GLContextEGL *shareContext,
                    bool aIsOffscreen = false)
    {
        EGLContext context;

        context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                             config,
                                             shareContext ? shareContext->mContext : EGL_NO_CONTEXT,
                                             sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                                         : gContextAttribs);
        if (!context) {
            if (shareContext) {
                shareContext = nsnull;
                context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                                     config,
                                                     EGL_NO_CONTEXT,
                                                     sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                                                 : gContextAttribs);
                if (!context) {
                    NS_WARNING("Failed to create EGLContext!");
                    return nsnull;
                }
            }
        }

        nsRefPtr<GLContextEGL> glContext =
            new GLContextEGL(format, shareContext, config,
                             surface, context, aIsOffscreen);

        if (!glContext->Init())
            return nsnull;

        return glContext.forget();
    }

public:
    GLContextEGL(const ContextFormat& aFormat,
                 GLContext *aShareContext,
                 EGLConfig aConfig,
                 EGLSurface aSurface,
                 EGLContext aContext,
                 bool aIsOffscreen = false)
        : GLContext(aFormat, aIsOffscreen, aShareContext)
        , mConfig(aConfig) 
        , mSurface(aSurface), mContext(aContext)
        , mPlatformContext(nsnull)
        , mThebesSurface(nsnull)
        , mBound(false)
        , mIsPBuffer(false)
        , mIsDoubleBuffered(false)
        , mCanBindToTexture(false)
        , mShareWithEGLImage(false)
    {
        // any EGL contexts will always be GLESv2
        SetIsGLES2(true);

#ifdef DEBUG
        printf_stderr("Initializing context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif
    }

    ~GLContextEGL()
    {
        MarkDestroyed();

        // If mGLWidget is non-null, then we've been given it by the GL context provider,
        // and it's managed by the widget implementation. In this case, We can't destroy
        // our contexts.
        if (mPlatformContext)
            return;

#ifdef DEBUG
        printf_stderr("Destroying context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif

        sEGLLibrary.fDestroyContext(EGL_DISPLAY(), mContext);
        if (mSurface && !mPlatformContext) {
            sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
        }
    }

    GLContextType GetContextType() {
        return ContextTypeEGL;
    }

    bool Init()
    {
#if defined(ANDROID)
        // We can't use LoadApitraceLibrary here because the GLContext
        // expects its own handle to the GL library
        if (!OpenLibrary(APITRACE_LIB))
#endif
            if (!OpenLibrary(GLES2_LIB)) {
#if defined(XP_UNIX)
                if (!OpenLibrary(GLES2_LIB2)) {
                    NS_WARNING("Couldn't load GLES2 LIB.");
                    return false;
                }
#endif
            }

#ifdef MOZ_WIDGET_GONK
        char propValue[PROPERTY_VALUE_MAX];
        property_get("ro.build.version.sdk", propValue, "0");
        if (atoi(propValue) < 15)
            gUseBackingSurface = false;
#endif

        bool current = MakeCurrent();
        if (!current) {
            gfx::LogFailure(NS_LITERAL_CSTRING(
                "Couldn't get device attachments for device."));
            return false;
        }

        SetupLookupFunction();

        bool ok = InitWithPrefix("gl", true);

        PR_STATIC_ASSERT(sizeof(GLint) >= sizeof(int32_t));
        mMaxTextureImageSize = PR_INT32_MAX;

        mShareWithEGLImage = sEGLLibrary.HasKHRImageBase() &&
                             sEGLLibrary.HasKHRImageTexture2D() &&
                             IsExtensionSupported(OES_EGL_image);

        if (ok)
            InitFramebuffers();

        return ok;
    }

    bool IsDoubleBuffered() {
        return mIsDoubleBuffered;
    }

    void SetIsDoubleBuffered(bool aIsDB) {
        mIsDoubleBuffered = aIsDB;
    }

    bool SupportsRobustness()
    {
        return sEGLLibrary.HasRobustness();
    }

    virtual bool IsANGLE()
    {
        return sEGLLibrary.IsANGLE();
    }

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    gfxASurface* GetOffscreenPixmapSurface()
    {
      return mThebesSurface;
    }
    
    virtual bool WaitNative() {
      return sEGLLibrary.fWaitNative(LOCAL_EGL_CORE_NATIVE_ENGINE);
    }
#endif

    bool BindTexImage()
    {
        if (!mSurface)
            return false;

        if (mBound && !ReleaseTexImage())
            return false;

        EGLBoolean success = sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
            (EGLSurface)mSurface, LOCAL_EGL_BACK_BUFFER);
        if (success == LOCAL_EGL_FALSE)
            return false;

        mBound = true;
        return true;
    }

    bool ReleaseTexImage()
    {
        if (!mBound)
            return true;

        if (!mSurface)
            return false;

        EGLBoolean success;
        success = sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                               (EGLSurface)mSurface,
                                               LOCAL_EGL_BACK_BUFFER);
        if (success == LOCAL_EGL_FALSE)
            return false;

        mBound = false;
        return true;
    }

    bool BindExternalBuffer(GLuint texture, void* buffer)
    {
#if defined(MOZ_WIDGET_GONK)
        EGLint attrs[] = {
            LOCAL_EGL_IMAGE_PRESERVED, LOCAL_EGL_TRUE,
            LOCAL_EGL_NONE, LOCAL_EGL_NONE
        };
        EGLImage image = sEGLLibrary.fCreateImage(EGL_DISPLAY(),
                                                  EGL_NO_CONTEXT,
                                                  EGL_NATIVE_BUFFER_ANDROID,
                                                  buffer, attrs);
        fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_EXTERNAL, image);
        fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, texture);
        sEGLLibrary.fDestroyImage(EGL_DISPLAY(), image);
        return true;
#else
        return false;
#endif
    }

    bool UnbindExternalBuffer(GLuint texture)
    {
#if defined(MOZ_WIDGET_GONK)
        fActiveTexture(LOCAL_GL_TEXTURE0);
        fBindTexture(LOCAL_GL_TEXTURE_2D, texture);
        fTexImage2D(LOCAL_GL_TEXTURE_2D, 0,
                    LOCAL_GL_RGBA,
                    1, 1, 0,
                    LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                    nsnull);
        return true;
#else
        return false;
#endif
    }

#ifdef MOZ_WIDGET_GONK
    virtual already_AddRefed<TextureImage>
    CreateDirectTextureImage(GraphicBuffer* aBuffer, GLenum aWrapMode) MOZ_OVERRIDE;
#endif

    bool MakeCurrentImpl(bool aForce = false) {
        bool succeeded = true;

        // Assume that EGL has the same problem as WGL does,
        // where MakeCurrent with an already-current context is
        // still expensive.
#ifndef MOZ_WIDGET_QT
        if (!mSurface) {
            // We need to be able to bind NO_SURFACE when we don't
            // have access to a surface. We won't be drawing to the screen
            // but we will be able to do things like resource releases.
            succeeded = sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                                 EGL_NO_SURFACE, EGL_NO_SURFACE,
                                                 EGL_NO_CONTEXT);
            if (!succeeded && sEGLLibrary.fGetError() == LOCAL_EGL_CONTEXT_LOST) {
                mContextLost = true;
                NS_WARNING("EGL context has been lost.");
            }
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
            return succeeded;
        }
#endif
        if (aForce || sEGLLibrary.fGetCurrentContext() != mContext) {
#ifdef MOZ_WIDGET_QT
            // Shared Qt GL context need to be informed about context switch
            if (mSharedContext) {
                QGLContext* qglCtx = static_cast<QGLContext*>(static_cast<GLContextEGL*>(mSharedContext.get())->mPlatformContext);
                if (qglCtx) {
                    qglCtx->doneCurrent();
                }
            }
#endif
            succeeded = sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                                 mSurface, mSurface,
                                                 mContext);
            if (!succeeded && sEGLLibrary.fGetError() == LOCAL_EGL_CONTEXT_LOST) {
                mContextLost = true;
                NS_WARNING("EGL context has been lost.");
            }
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
        }

        return succeeded;
    }

#ifdef MOZ_WIDGET_QT
    virtual bool
    RenewSurface() {
        /* We don't support renewing on QT because we don't create the surface ourselves */
        return false;
    }
#else
    virtual bool
    RenewSurface() {
        sEGLLibrary.fMakeCurrent(EGL_DISPLAY(), EGL_NO_SURFACE, EGL_NO_SURFACE,
                                 EGL_NO_CONTEXT);
        if (!mSurface) {
#ifdef MOZ_JAVA_COMPOSITOR
            mSurface = mozilla::AndroidBridge::Bridge()->ProvideEGLSurface();
#else
            EGLConfig config;
            CreateConfig(&config);
            mSurface = CreateSurfaceForWindow(NULL, config);
#endif
        }
        return sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                        mSurface, mSurface,
                                        mContext);
    }
#endif

    virtual void
    ReleaseSurface() {
        if (mSurface && !mPlatformContext) {
            sEGLLibrary.fMakeCurrent(EGL_DISPLAY(), EGL_NO_SURFACE, EGL_NO_SURFACE,
                                     EGL_NO_CONTEXT);
            sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
            mSurface = NULL;
        }
    }

    bool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sEGLLibrary.mSymbols.fGetProcAddress;
        return true;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch (aType) {
        case NativeGLContext:
            return mContext;

        default:
            return nsnull;
        }
    }

    bool SwapBuffers()
    {
        if (mSurface && !mPlatformContext) {
            return sEGLLibrary.fSwapBuffers(EGL_DISPLAY(), mSurface);
        } else {
            return false;
        }
    }
    // GLContext interface - returns Tiled Texture Image in our case
    virtual already_AddRefed<TextureImage>
    CreateTextureImage(const nsIntSize& aSize,
                       TextureImage::ContentType aContentType,
                       GLenum aWrapMode,
                       TextureImage::Flags aFlags = TextureImage::NoFlags);

    // a function to generate Tiles for Tiled Texture Image
    virtual already_AddRefed<TextureImage>
    TileGenFunc(const nsIntSize& aSize,
                TextureImage::ContentType aContentType,
                TextureImage::Flags aFlags = TextureImage::NoFlags);
    // hold a reference to the given surface
    // for the lifetime of this context.
    void HoldSurface(gfxASurface *aSurf) {
        mThebesSurface = aSurf;
    }

    void SetPlatformContext(void *context) {
        mPlatformContext = context;
    }

    EGLContext Context() {
        return mContext;
    }

    bool BindTex2DOffscreen(GLContext *aOffscreen);
    void UnbindTex2DOffscreen(GLContext *aOffscreen);
    bool ResizeOffscreen(const gfxIntSize& aNewSize);
    void BindOffscreenFramebuffer();

    static already_AddRefed<GLContextEGL>
    CreateEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                    const ContextFormat& aFormat,
                                    bool aShare);

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    static already_AddRefed<GLContextEGL>
    CreateBasicEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                         const ContextFormat& aFormat);

    bool ResizeOffscreenPixmapSurface(const gfxIntSize& aNewSize);
#endif

    static already_AddRefed<GLContextEGL>
    CreateEGLPBufferOffscreenContext(const gfxIntSize& aSize,
                                     const ContextFormat& aFormat,
                                     bool bufferUnused = false);

    void SetOffscreenSize(const gfxIntSize &aRequestedSize,
                          const gfxIntSize &aActualSize)
    {
        mOffscreenSize = aRequestedSize;
        mOffscreenActualSize = aActualSize;
    }

    void *GetD3DShareHandle() {
        if (!sEGLLibrary.HasANGLESurfaceD3DTexture2DShareHandle()) {
            return nsnull;
        }

        void *h = nsnull;

#ifndef EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE
#define EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE 0x3200
#endif

        if (!sEGLLibrary.fQuerySurfacePointerANGLE(EGL_DISPLAY(), mSurface,
                                                   EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, (void**) &h))
        {
            return nsnull;
        }

        return h;
    }

    virtual bool HasLockSurface() {
        return sEGLLibrary.HasKHRLockSurface();
    }

    virtual SharedTextureHandle CreateSharedHandle(TextureImage::TextureShareType aType);
    virtual SharedTextureHandle CreateSharedHandle(TextureImage::TextureShareType aType,
                                                   void* aBuffer,
                                                   SharedTextureBufferType aBufferType);
    virtual void UpdateSharedHandle(TextureImage::TextureShareType aType,
                                    SharedTextureHandle aSharedHandle);
    virtual void ReleaseSharedHandle(TextureImage::TextureShareType aType,
                                     SharedTextureHandle aSharedHandle);
    virtual bool GetSharedHandleDetails(TextureImage::TextureShareType aType,
                                        SharedTextureHandle aSharedHandle,
                                        SharedHandleDetails& aDetails);
    virtual bool AttachSharedHandle(TextureImage::TextureShareType aType,
                                    SharedTextureHandle aSharedHandle);
protected:
    friend class GLContextProviderEGL;

    EGLConfig  mConfig;
    EGLSurface mSurface;
    EGLContext mContext;
    void *mPlatformContext;
    nsRefPtr<gfxASurface> mThebesSurface;
    bool mBound;

    bool mIsPBuffer;
    bool mIsDoubleBuffered;
    bool mCanBindToTexture;
    bool mShareWithEGLImage;

    static EGLSurface CreatePBufferSurfaceTryingPowerOfTwo(EGLConfig config,
                                                           EGLenum bindToTextureFormat,
                                                           gfxIntSize& pbsize)
    {
        nsTArray<EGLint> pbattrs(16);
        EGLSurface surface = nsnull;

    TRY_AGAIN_POWER_OF_TWO:
        pbattrs.Clear();
        pbattrs.AppendElement(LOCAL_EGL_WIDTH); pbattrs.AppendElement(pbsize.width);
        pbattrs.AppendElement(LOCAL_EGL_HEIGHT); pbattrs.AppendElement(pbsize.height);

        if (bindToTextureFormat != LOCAL_EGL_NONE) {
            pbattrs.AppendElement(LOCAL_EGL_TEXTURE_TARGET);
            pbattrs.AppendElement(LOCAL_EGL_TEXTURE_2D);

            pbattrs.AppendElement(LOCAL_EGL_TEXTURE_FORMAT);
            pbattrs.AppendElement(bindToTextureFormat);
        }

        pbattrs.AppendElement(LOCAL_EGL_NONE);

        surface = sEGLLibrary.fCreatePbufferSurface(EGL_DISPLAY(), config, &pbattrs[0]);
        if (!surface) {
            if (!is_power_of_two(pbsize.width) ||
                !is_power_of_two(pbsize.height))
            {
                if (!is_power_of_two(pbsize.width))
                    pbsize.width = next_power_of_two(pbsize.width);
                if (!is_power_of_two(pbsize.height))
                    pbsize.height = next_power_of_two(pbsize.height);

                NS_WARNING("Failed to create pbuffer, trying power of two dims");
                goto TRY_AGAIN_POWER_OF_TWO;
            }

            NS_WARNING("Failed to create pbuffer surface");
            return nsnull;
        }

        return surface;
    }
};

typedef enum {
    Image
#ifdef MOZ_WIDGET_ANDROID
    , SurfaceTexture
#endif
} SharedHandleType;

class SharedTextureHandleWrapper
{
public:
    SharedTextureHandleWrapper(SharedHandleType aHandleType) : mHandleType(aHandleType)
    {
    }

    virtual ~SharedTextureHandleWrapper()
    {
    }

    SharedHandleType Type() { return mHandleType; }

    SharedHandleType mHandleType;
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureWrapper: public SharedTextureHandleWrapper
{
public:
    SurfaceTextureWrapper(nsSurfaceTexture* aSurfaceTexture) :
        SharedTextureHandleWrapper(SharedHandleType::SurfaceTexture)
        , mSurfaceTexture(aSurfaceTexture)
    {
    }

    virtual ~SurfaceTextureWrapper() {
        mSurfaceTexture = nsnull;
    }

    nsSurfaceTexture* SurfaceTexture() { return mSurfaceTexture; }

    nsRefPtr<nsSurfaceTexture> mSurfaceTexture;
};

#endif // MOZ_WIDGET_ANDROID

class EGLTextureWrapper : public SharedTextureHandleWrapper
{
public:
    EGLTextureWrapper(GLContext* aContext, GLuint aTexture, bool aOwnsTexture) :
        SharedTextureHandleWrapper(SharedHandleType::Image)
        , mContext(aContext)
        , mTexture(aTexture)
        , mEGLImage(nsnull)
        , mSyncObject(nsnull)
        , mOwnsTexture(aOwnsTexture)
    {
    }

    bool CreateEGLImage() {
        MOZ_ASSERT(!mEGLImage && mTexture && sEGLLibrary.HasKHRImageBase());
        static const EGLint eglAttributes[] = {
            LOCAL_EGL_NONE
        };
        GLContextEGL* ctx = static_cast<GLContextEGL*>(mContext.get());
        mEGLImage = sEGLLibrary.fCreateImage(EGL_DISPLAY(), ctx->Context(), LOCAL_EGL_GL_TEXTURE_2D,
                                             (EGLClientBuffer)mTexture, eglAttributes);
        if (!mEGLImage) {
#ifdef DEBUG
            printf_stderr("Could not create EGL images: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
#endif
            return false;
        }
        return true;
    }

    virtual ~EGLTextureWrapper() {
        if (mEGLImage) {
            sEGLLibrary.fDestroyImage(EGL_DISPLAY(), mEGLImage);
            mEGLImage = nsnull;
        }
    }

    GLuint GetTextureID() {
        return mTexture;
    }

    GLContext* GetContext() {
        return mContext.get();
    }

    const EGLImage GetEGLImage() {
        return mEGLImage;
    }

    bool MakeSync() {
        MOZ_ASSERT(mSyncObject == nsnull);

        if (sEGLLibrary.IsExtensionSupported(GLLibraryEGL::KHR_fence_sync)) {
            mSyncObject = sEGLLibrary.fCreateSync(EGL_DISPLAY(), LOCAL_EGL_SYNC_FENCE, nsnull);
            // We need to flush to make sure the sync object enters the command stream;
            // we can't use EGL_SYNC_FLUSH_COMMANDS_BIT at wait time, because the wait
            // happens on a different thread/context.
            mContext->fFlush();
        }

        if (mSyncObject == EGL_NO_SYNC) {
            // we failed to create one, so just do a finish
            mContext->fFinish();
        }

        return true;
    }

    bool WaitSync() {
        if (!mSyncObject) {
            // if we have no sync object, then we did a Finish() earlier
            return true;
        }

        // wait at most 1 second; this should really be never/rarely hit
        const uint64_t ns_per_ms = 1000 * 1000;
        EGLTime timeout = 1000 * ns_per_ms;

        EGLint result = sEGLLibrary.fClientWaitSync(EGL_DISPLAY(), mSyncObject, 0, timeout);
        sEGLLibrary.fDestroySync(EGL_DISPLAY(), mSyncObject);
        mSyncObject = nsnull;

        return result == LOCAL_EGL_CONDITION_SATISFIED;
    }

    bool OwnsTexture() {
        return mOwnsTexture;
    }

private:
    nsRefPtr<GLContext> mContext;
    GLuint mTexture;
    EGLImage mEGLImage;
    EGLSync mSyncObject;
    bool mOwnsTexture;
};

void
GLContextEGL::UpdateSharedHandle(TextureImage::TextureShareType aType,
                                 SharedTextureHandle aSharedHandle)
{
    if (aType != TextureImage::ThreadShared) {
        NS_ERROR("Implementation not available for this sharing type");
        return;
    }

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(aSharedHandle);

    NS_ASSERTION(wrapper->Type() == SharedHandleType::Image, "Expected EGLImage shared handle");
    NS_ASSERTION(mShareWithEGLImage, "EGLImage not supported or disabled in runtime");

    EGLTextureWrapper* wrap = reinterpret_cast<EGLTextureWrapper*>(wrapper);
    // We need to copy the current GLContext drawing buffer to the texture
    // exported by the EGLImage.  Need to save both the read FBO and the texture
    // binding, because we're going to munge them to do this.
    GLuint prevRead = GetUserBoundReadFBO();
    GLint oldtex = -1;
    BindUserReadFBO(0);
    fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &oldtex);
    MOZ_ASSERT(oldtex != -1);
    fBindTexture(LOCAL_GL_TEXTURE_2D, wrap->GetTextureID());

    // CopyTexSubImage2D, is ~2x slower than simple FBO render to texture with draw quads,
    // but render with draw quads require complex and hard to maintain context save/restore code
    fCopyTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0, 0, 0,
                       0, 0, mOffscreenActualSize.width,
                       mOffscreenActualSize.height);

    fBindTexture(LOCAL_GL_TEXTURE_2D, oldtex);
    BindUserReadFBO(prevRead);

    // Make sure our copy is finished, so that we can be ready to draw
    // in different thread GLContext.  If we have KHR_fence_sync, then
    // we insert a sync object, otherwise we have to do a GuaranteeResolve.
    wrap->MakeSync();
}

SharedTextureHandle
GLContextEGL::CreateSharedHandle(TextureImage::TextureShareType aType)
{
    if (aType != TextureImage::ThreadShared)
        return 0;

    if (!mShareWithEGLImage)
        return 0;

    MakeCurrent();
    GLuint texture = 0;
    ContextFormat fmt = ActualFormat();
    CreateTextureForOffscreen(ChooseGLFormats(fmt, GLContext::ForceRGBA), mOffscreenSize, texture);
    // texture ownership moved to EGLTextureWrapper after  this point
    // and texture will be deleted in EGLTextureWrapper dtor
    EGLTextureWrapper* tex = new EGLTextureWrapper(this, texture, true);
    if (!tex->CreateEGLImage()) {
        NS_ERROR("EGLImage creation for EGLTextureWrapper failed");
        ReleaseSharedHandle(aType, (SharedTextureHandle)tex);
        return 0;
    }
    // Raw pointer shared across threads
    return (SharedTextureHandle)tex;
}

SharedTextureHandle
GLContextEGL::CreateSharedHandle(TextureImage::TextureShareType aType,
                                 void* aBuffer,
                                 SharedTextureBufferType aBufferType)
{
    // Both EGLImage and SurfaceTexture only support ThreadShared currently, but
    // it's possible to make SurfaceTexture work across processes. We should do that.
    if (aType != TextureImage::ThreadShared)
        return 0;

    switch (aBufferType) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedTextureBufferType::SurfaceTexture:
        if (!IsExtensionSupported(GLContext::OES_EGL_image_external)) {
            NS_WARNING("Missing GL_OES_EGL_image_external");
            return 0;
        }

        return (SharedTextureHandle) new SurfaceTextureWrapper(reinterpret_cast<nsSurfaceTexture*>(aBuffer));
#endif
    case SharedTextureBufferType::TextureID: {
        if (!mShareWithEGLImage)
            return 0;

        GLuint texture = (GLuint)aBuffer;
        EGLTextureWrapper* tex = new EGLTextureWrapper(this, texture, false);
        if (!tex->CreateEGLImage()) {
            NS_ERROR("EGLImage creation for EGLTextureWrapper failed");
            delete tex;
            return 0;
        }

        return (SharedTextureHandle)tex;
    }
    default:
        NS_ERROR("Unknown shared texture buffer type");
        return 0;
    }
}

void GLContextEGL::ReleaseSharedHandle(TextureImage::TextureShareType aType,
                                       SharedTextureHandle aSharedHandle)
{
    if (aType != TextureImage::ThreadShared) {
        NS_ERROR("Implementation not available for this sharing type");
        return;
    }

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(aSharedHandle);

    switch (wrapper->Type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedHandleType::SurfaceTexture:
        delete wrapper;
        break;
#endif
    
    case SharedHandleType::Image: {
        NS_ASSERTION(mShareWithEGLImage, "EGLImage not supported or disabled in runtime");

        EGLTextureWrapper* wrap = (EGLTextureWrapper*)aSharedHandle;
        GLContext *ctx = wrap->GetContext();
        if (ctx->IsDestroyed() || !ctx->IsOwningThreadCurrent()) {
            ctx = ctx->GetSharedContext();
        }
        // If we have a context, then we need to delete the texture;
        // if we don't have a context (either real or shared),
        // then they went away when the contex was deleted, because it
        // was the only one that had access to it.
        if (wrap->OwnsTexture() && ctx && !ctx->IsDestroyed() && ctx->MakeCurrent()) {
            GLuint texture = wrap->GetTextureID();
            ctx->fDeleteTextures(1, &texture);
        }
        delete wrap;
        break;
    }

    default:
        NS_ERROR("Unknown shared handle type");
    }
}

bool GLContextEGL::GetSharedHandleDetails(TextureImage::TextureShareType aType,
                                          SharedTextureHandle aSharedHandle,
                                          SharedHandleDetails& aDetails)
{
    if (aType != TextureImage::ThreadShared)
        return false;

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(aSharedHandle);

    switch (wrapper->Type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedHandleType::SurfaceTexture: {
        SurfaceTextureWrapper* surfaceWrapper = reinterpret_cast<SurfaceTextureWrapper*>(wrapper);

        aDetails.mTarget = LOCAL_GL_TEXTURE_EXTERNAL;
        aDetails.mProgramType = RGBALayerExternalProgramType;
        surfaceWrapper->SurfaceTexture()->GetTransformMatrix(aDetails.mTextureTransform);
        break;
    }
#endif

    case SharedHandleType::Image:
        aDetails.mTarget = LOCAL_GL_TEXTURE_2D;
        aDetails.mProgramType = RGBALayerProgramType;
        break;

    default:
        NS_ERROR("Unknown shared handle type");
        return false;
    }

    return true;
}

bool GLContextEGL::AttachSharedHandle(TextureImage::TextureShareType aType,
                                      SharedTextureHandle aSharedHandle)
{
    if (aType != TextureImage::ThreadShared)
        return false;

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(aSharedHandle);

    switch (wrapper->Type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedHandleType::SurfaceTexture: {
#ifndef DEBUG
        /**
         * NOTE: SurfaceTexture spams us if there are any existing GL errors, so we'll clear
         * them here in order to avoid that.
         */
        GetAndClearError();
#endif
        SurfaceTextureWrapper* surfaceTextureWrapper = reinterpret_cast<SurfaceTextureWrapper*>(wrapper);

        // FIXME: SurfaceTexture provides a transform matrix which is supposed to
        // be applied to the texture coordinates. We should return that here
        // so we can render correctly. Bug 775083
        surfaceTextureWrapper->SurfaceTexture()->UpdateTexImage();
        break;
    }
#endif // MOZ_WIDGET_ANDROID
    
    case SharedHandleType::Image: {
        NS_ASSERTION(mShareWithEGLImage, "EGLImage not supported or disabled in runtime");

        EGLTextureWrapper* wrap = (EGLTextureWrapper*)aSharedHandle;
        fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, wrap->GetEGLImage());
        break;
    }

    default:
        NS_ERROR("Unknown shared handle type");
        return false;
    }

    return true;
}

bool
GLContextEGL::BindTex2DOffscreen(GLContext *aOffscreen)
{
    if (aOffscreen->GetContextType() != ContextTypeEGL) {
        NS_WARNING("non-EGL context");
        return false;
    }

    GLContextEGL *offs = static_cast<GLContextEGL*>(aOffscreen);

    if (offs->mCanBindToTexture) {
        bool ok = sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
                                              offs->mSurface,
                                              LOCAL_EGL_BACK_BUFFER);
        return ok;
    }

    if (offs->mOffscreenTexture) {
        if (offs->GetSharedContext() != GLContextProviderEGL::GetGlobalContext())
        {
            NS_WARNING("offscreen FBO context can only be bound with context sharing!");
            return false;
        }

        fBindTexture(LOCAL_GL_TEXTURE_2D, offs->mOffscreenTexture);
        return true;
    }

    NS_WARNING("don't know how to bind this!");

    return false;
}

void
GLContextEGL::UnbindTex2DOffscreen(GLContext *aOffscreen)
{
    NS_ASSERTION(aOffscreen->GetContextType() == ContextTypeEGL, "wrong type");

    GLContextEGL *offs = static_cast<GLContextEGL*>(aOffscreen);

    if (offs->mCanBindToTexture) {
        sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                     offs->mSurface,
                                     LOCAL_EGL_BACK_BUFFER);
    }
}

bool
GLContextEGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    if (!IsOffscreenSizeAllowed(aNewSize))
        return false;

    if (mIsPBuffer) {
        gfxIntSize pbsize(aNewSize);

        EGLSurface surface =
            CreatePBufferSurfaceTryingPowerOfTwo(mConfig,
                                                 mCanBindToTexture
                                                 ? (mCreationFormat.minAlpha
                                                    ? LOCAL_EGL_TEXTURE_RGBA
                                                    : LOCAL_EGL_TEXTURE_RGB)
                                                 : LOCAL_EGL_NONE,
                                                 pbsize);
        if (!surface) {
            NS_WARNING("Failed to resize pbuffer");
            return false;
        }

        if (!ResizeOffscreenFBOs(pbsize, false))
            return false;

        SetOffscreenSize(aNewSize, pbsize);

        if (mSurface && !mPlatformContext) {
            sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
        }

        mSurface = surface;

        MakeCurrent(true);
        ClearSafely();

        return true;
    }

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    if (ResizeOffscreenPixmapSurface(aNewSize)) {
        if (ResizeOffscreenFBOs(aNewSize, true))
            return true;
    }
#endif

    return ResizeOffscreenFBOs(aNewSize, true);
}


static GLContextEGL *
GetGlobalContextEGL()
{
    return static_cast<GLContextEGL*>(GLContextProviderEGL::GetGlobalContext());
}

static GLenum
GLFormatForImage(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
    case gfxASurface::ImageFormatRGB24:
        // Thebes only supports RGBX, not packed RGB.
        return LOCAL_GL_RGBA;
    case gfxASurface::ImageFormatRGB16_565:
        return LOCAL_GL_RGB;
    case gfxASurface::ImageFormatA8:
        return LOCAL_GL_LUMINANCE;
    default:
        NS_WARNING("Unknown GL format for Image format");
    }
    return 0;
}

#ifdef MOZ_WIDGET_GONK
static PixelFormat
PixelFormatForImage(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
        return PIXEL_FORMAT_RGBA_8888;
    case gfxASurface::ImageFormatRGB24:
        return PIXEL_FORMAT_RGBX_8888;
    case gfxASurface::ImageFormatRGB16_565:
        return PIXEL_FORMAT_RGB_565;
    case gfxASurface::ImageFormatA8:
        return PIXEL_FORMAT_L_8;
    default:
        MOZ_NOT_REACHED("Unknown gralloc pixel format for Image format");
    }
    return 0;
}

static gfxASurface::gfxContentType
ContentTypeForPixelFormat(PixelFormat aFormat)
{
    switch (aFormat) {
    case PIXEL_FORMAT_L_8:
        return gfxASurface::CONTENT_ALPHA;
    case PIXEL_FORMAT_RGBA_8888:
        return gfxASurface::CONTENT_COLOR_ALPHA;
    case PIXEL_FORMAT_RGBX_8888:
    case PIXEL_FORMAT_RGB_565:
        return gfxASurface::CONTENT_COLOR;
    default:
        MOZ_NOT_REACHED("Unknown content type for gralloc pixel format");
    }
    return gfxASurface::CONTENT_COLOR;
}
#endif

static GLenum
GLTypeForImage(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
    case gfxASurface::ImageFormatRGB24:
    case gfxASurface::ImageFormatA8:
        return LOCAL_GL_UNSIGNED_BYTE;
    case gfxASurface::ImageFormatRGB16_565:
        return LOCAL_GL_UNSIGNED_SHORT_5_6_5;
    default:
        NS_WARNING("Unknown GL format for Image format");
    }
    return 0;
}

class TextureImageEGL
    : public TextureImage
{
public:
    TextureImageEGL(GLuint aTexture,
                    const nsIntSize& aSize,
                    GLenum aWrapMode,
                    ContentType aContentType,
                    GLContext* aContext,
                    Flags aFlags = TextureImage::NoFlags,
                    TextureState aTextureState = Created)
        : TextureImage(aSize, aWrapMode, aContentType, aFlags)
        , mGLContext(aContext)
        , mUpdateFormat(gfxASurface::ImageFormatUnknown)
        , mEGLImage(nsnull)
        , mTexture(aTexture)
        , mSurface(nsnull)
        , mConfig(nsnull)
        , mTextureState(aTextureState)
        , mBound(false)
        , mIsLocked(false)
    {
        mUpdateFormat = gfxPlatform::GetPlatform()->OptimalFormatForContent(GetContentType());

        if (gUseBackingSurface) {
#ifdef MOZ_WIDGET_GONK
            switch (mUpdateFormat) {
            case gfxASurface::ImageFormatARGB32:
                mShaderType = BGRALayerProgramType;
                break;
            case gfxASurface::ImageFormatRGB24:
                mUpdateFormat = gfxASurface::ImageFormatARGB32;
                mShaderType = BGRXLayerProgramType;
                break;
            case gfxASurface::ImageFormatRGB16_565:
                mShaderType = RGBXLayerProgramType;
                break;
            case gfxASurface::ImageFormatA8:
                mShaderType = RGBALayerProgramType;
                break;
            default:
                MOZ_NOT_REACHED("Unknown update format");
            }
#else
            if (mUpdateFormat != gfxASurface::ImageFormatARGB32) {
                mShaderType = RGBXLayerProgramType;
            } else {
                mShaderType = RGBALayerProgramType;
            }
#endif
            Resize(aSize);
        } else {
            if (mUpdateFormat == gfxASurface::ImageFormatRGB16_565) {
                mShaderType = RGBXLayerProgramType;
            } else if (mUpdateFormat == gfxASurface::ImageFormatRGB24) {
                // RGB24 means really RGBX for Thebes, which means we have to
                // use the right shader and ignore the uninitialized alpha
                // value.
                mShaderType = BGRXLayerProgramType;
            } else {
                mShaderType = BGRALayerProgramType;
            }
        }
    }

    virtual ~TextureImageEGL()
    {
        GLContext *ctx = mGLContext;
        if (ctx->IsDestroyed() || !ctx->IsOwningThreadCurrent()) {
            ctx = ctx->GetSharedContext();
        }

        // If we have a context, then we need to delete the texture;
        // if we don't have a context (either real or shared),
        // then they went away when the contex was deleted, because it
        // was the only one that had access to it.
        if (ctx && !ctx->IsDestroyed()) {
            ctx->MakeCurrent();
            ctx->fDeleteTextures(1, &mTexture);
            ReleaseTexImage();
            DestroyEGLSurface();
        }
    }

    bool UsingDirectTexture()
    {
#ifdef MOZ_WIDGET_GONK
        if (mGraphicBuffer != nsnull)
            return true;
#endif
        return !!mBackingSurface;
    }

    virtual void GetUpdateRegion(nsIntRegion& aForRegion)
    {
        if (mTextureState != Valid) {
            // if the texture hasn't been initialized yet, force the
            // client to paint everything
            aForRegion = nsIntRect(nsIntPoint(0, 0), mSize);
        }

        if (UsingDirectTexture()) {
            return;
        }

        // We can only draw a rectangle, not subregions due to
        // the way that our texture upload functions work.  If
        // needed, we /could/ do multiple texture uploads if we have
        // non-overlapping rects, but that's a tradeoff.
        aForRegion = nsIntRegion(aForRegion.GetBounds());
    }

    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion)
    {
        NS_ASSERTION(!mUpdateSurface, "BeginUpdate() without EndUpdate()?");

        // determine the region the client will need to repaint
        GetUpdateRegion(aRegion);
        mUpdateRect = aRegion.GetBounds();

        //printf_stderr("BeginUpdate with updateRect [%d %d %d %d]\n", mUpdateRect.x, mUpdateRect.y, mUpdateRect.width, mUpdateRect.height);
        if (!nsIntRect(nsIntPoint(0, 0), mSize).Contains(mUpdateRect)) {
            NS_ERROR("update outside of image");
            return NULL;
        }

#ifdef MOZ_WIDGET_GONK
        if (mGraphicBuffer != nsnull) {
            mUpdateSurface = GetLockSurface();

            return mUpdateSurface;
        }
#endif

        if (mBackingSurface) {
            if (sEGLLibrary.HasKHRLockSurface()) {
                mUpdateSurface = GetLockSurface();
            } else {
                mUpdateSurface = mBackingSurface;
            }

            return mUpdateSurface;
        }

        //printf_stderr("creating image surface %dx%d format %d\n", mUpdateRect.width, mUpdateRect.height, mUpdateFormat);

        mUpdateSurface =
            new gfxImageSurface(gfxIntSize(mUpdateRect.width, mUpdateRect.height),
                                mUpdateFormat);

        mUpdateSurface->SetDeviceOffset(gfxPoint(-mUpdateRect.x, -mUpdateRect.y));

        return mUpdateSurface;
    }

    virtual void EndUpdate()
    {
        NS_ASSERTION(!!mUpdateSurface, "EndUpdate() without BeginUpdate()?");

        if (mIsLocked) {
            UnlockSurface();
            mTextureState = Valid;
            mUpdateSurface = nsnull;
            return;
        }

        if (mBackingSurface && mUpdateSurface == mBackingSurface) {
#ifdef MOZ_X11
            if (mBackingSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
                XSync(DefaultXDisplay(), False);
            }
#endif

            mBackingSurface->SetDeviceOffset(gfxPoint(0, 0));
            mTextureState = Valid;
            mUpdateSurface = nsnull;
            return;
        }

        //printf_stderr("EndUpdate: slow path");

        // This is the slower path -- we didn't have any way to set up
        // a fast mapping between our cairo target surface and the GL
        // texture, so we have to upload data.

        // Undo the device offset that BeginUpdate set; doesn't much
        // matter for us here, but important if we ever do anything
        // directly with the surface.
        mUpdateSurface->SetDeviceOffset(gfxPoint(0, 0));

        nsRefPtr<gfxImageSurface> uploadImage = nsnull;
        gfxIntSize updateSize(mUpdateRect.width, mUpdateRect.height);

        NS_ASSERTION(mUpdateSurface->GetType() == gfxASurface::SurfaceTypeImage &&
                     mUpdateSurface->GetSize() == updateSize,
                     "Upload image isn't an image surface when one is expected, or is wrong size!");

        uploadImage = static_cast<gfxImageSurface*>(mUpdateSurface.get());

        if (!uploadImage) {
            return;
        }

        mGLContext->MakeCurrent();
        mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

        if (mTextureState != Valid) {
            NS_ASSERTION(mUpdateRect.x == 0 && mUpdateRect.y == 0 &&
                         mUpdateRect.Size() == mSize,
                         "Bad initial update on non-created texture!");

            mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    mUpdateRect.width,
                                    mUpdateRect.height,
                                    0,
                                    GLFormatForImage(uploadImage->Format()),
                                    GLTypeForImage(uploadImage->Format()),
                                    uploadImage->Data());
        } else {
            mGLContext->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                                       0,
                                       mUpdateRect.x,
                                       mUpdateRect.y,
                                       mUpdateRect.width,
                                       mUpdateRect.height,
                                       GLFormatForImage(uploadImage->Format()),
                                       GLTypeForImage(uploadImage->Format()),
                                       uploadImage->Data());
        }

        mUpdateSurface = nsnull;
        mTextureState = Valid;
        return;         // mTexture is bound
    }

    virtual bool DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom /* = nsIntPoint(0, 0) */)
    {
        nsIntRect bounds = aRegion.GetBounds();

        nsIntRegion region;
        if (mTextureState != Valid) {
            bounds = nsIntRect(0, 0, mSize.width, mSize.height);
            region = nsIntRegion(bounds);
        } else {
            region = aRegion;
        }

        if ((mBackingSurface && sEGLLibrary.HasKHRLockSurface())
#ifdef MOZ_WIDGET_GONK
            || (mGraphicBuffer != nsnull)
#endif
            ) {
            mUpdateSurface = GetLockSurface();
            if (mUpdateSurface) {
                nsRefPtr<gfxContext> ctx = new gfxContext(mUpdateSurface);
                gfxUtils::ClipToRegion(ctx, aRegion);
                ctx->SetSource(aSurf, gfxPoint(-aFrom.x, -aFrom.y));
                ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
                ctx->Paint();
                mUpdateSurface = nsnull;
                UnlockSurface();
            }
        } else {
            mShaderType =
              mGLContext->UploadSurfaceToTexture(aSurf,
                                                 region,
                                                 mTexture,
                                                 mTextureState == Created,
                                                 bounds.TopLeft() + aFrom,
                                                 false);
        }

        mTextureState = Valid;
        return true;
    }

    virtual void BindTexture(GLenum aTextureUnit)
    {
        // Ensure the texture is allocated before it is used.
        if (mTextureState == Created) {
            Resize(mSize);
        }

#ifdef MOZ_WIDGET_GONK
        if (UsingDirectTexture()) {
            mGLContext->fActiveTexture(aTextureUnit);
            mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
            mGLContext->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mEGLImage);
            if (sEGLLibrary.fGetError() != LOCAL_EGL_SUCCESS) {
               LOG("Could not set image target texture. ERROR (0x%04x)", sEGLLibrary.fGetError());
            }
        } else
#endif
        {
            mGLContext->fActiveTexture(aTextureUnit);
            mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
            mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
        }
    }

    virtual GLuint GetTextureID() 
    {
        // Ensure the texture is allocated before it is used.
        if (mTextureState == Created) {
            Resize(mSize);
        }
        return mTexture;
    };

    virtual bool InUpdate() const { return !!mUpdateSurface; }

    virtual void Resize(const nsIntSize& aSize)
    {
        NS_ASSERTION(!mUpdateSurface, "Resize() while in update?");

        if (mSize == aSize && mTextureState != Created)
            return;

        mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    
        // Try to generate a backin surface first if we have the ability
        if (gUseBackingSurface) {
            CreateBackingSurface(gfxIntSize(aSize.width, aSize.height));
        }

        if (!UsingDirectTexture()) {
            // If we don't have a backing surface or failed to obtain one,
            // use the GL Texture failsafe
            mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    aSize.width,
                                    aSize.height,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    GLTypeForImage(mUpdateFormat),
                                    NULL);
        }

        mTextureState = Allocated;
        mSize = aSize;
    }

    bool BindTexImage()
    {
        if (mBound && !ReleaseTexImage())
            return false;

        EGLBoolean success =
            sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
                                      (EGLSurface)mSurface,
                                      LOCAL_EGL_BACK_BUFFER);

        if (success == LOCAL_EGL_FALSE)
            return false;

        mBound = true;
        return true;
    }

    bool ReleaseTexImage()
    {
        if (!mBound)
            return true;

        EGLBoolean success =
            sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                         (EGLSurface)mSurface,
                                         LOCAL_EGL_BACK_BUFFER);

        if (success == LOCAL_EGL_FALSE)
            return false;

        mBound = false;
        return true;
    }

    virtual already_AddRefed<gfxImageSurface> GetLockSurface()
    {
        if (mIsLocked) {
            NS_WARNING("Can't lock surface twice");
            return nsnull;
        }

#ifdef MOZ_WIDGET_GONK
        if (mGraphicBuffer != nsnull) {
            // Unset the EGLImage target so that we don't get clashing locks
            mGLContext->MakeCurrent(true);
            mGLContext->UnbindExternalBuffer(mTexture);

            void *vaddr;
            if (mGraphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN |
                                     GraphicBuffer::USAGE_SW_WRITE_OFTEN,
                                     &vaddr) != OK) {
                LOG("Could not lock GraphicBuffer");
                return false;
            }

            nsRefPtr<gfxImageSurface> surface =
                new gfxImageSurface(reinterpret_cast<unsigned char *>(vaddr),
                                    gfxIntSize(mSize.width, mSize.height),
                                    mGraphicBuffer->getStride() * gfxUtils::ImageFormatToDepth(mUpdateFormat) / 8,
                                    mUpdateFormat);

            mIsLocked = true;

            return surface.forget();
        }
#endif

        if (!sEGLLibrary.HasKHRLockSurface()) {
            NS_WARNING("GetLockSurface called, but no EGL_KHR_lock_surface extension!");
            return nsnull;
        }

        if (!CreateEGLSurface(mBackingSurface)) {
            NS_WARNING("Failed to create EGL surface");
            return nsnull;
        }

        static EGLint lock_attribs[] = {
            LOCAL_EGL_MAP_PRESERVE_PIXELS_KHR, LOCAL_EGL_TRUE,
            LOCAL_EGL_LOCK_USAGE_HINT_KHR, LOCAL_EGL_READ_SURFACE_BIT_KHR | LOCAL_EGL_WRITE_SURFACE_BIT_KHR,
            LOCAL_EGL_NONE
        };

        sEGLLibrary.fLockSurface(EGL_DISPLAY(), mSurface, lock_attribs);

        mIsLocked = true;

        unsigned char *data = nsnull;
        int pitch = 0;
        int pixsize = 0;

        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_POINTER_KHR, (EGLint*)&data);
        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_PITCH_KHR, &pitch);
        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_PIXEL_SIZE_KHR, &pixsize);

        nsRefPtr<gfxImageSurface> sharedImage =
            new gfxImageSurface(data,
                                mBackingSurface->GetSize(),
                                pitch,
                                mUpdateFormat);

        return sharedImage.forget();
    }

    virtual void UnlockSurface()
    {
        if (!mIsLocked) {
            NS_WARNING("UnlockSurface called, surface not locked!");
            return;
        }

        mIsLocked = false;

#ifdef MOZ_WIDGET_GONK
        if (mGraphicBuffer != nsnull) {
            mGraphicBuffer->unlock();

            return;
        }
#endif

        sEGLLibrary.fUnlockSurface(EGL_DISPLAY(), mSurface);
    }

    virtual already_AddRefed<gfxASurface> GetBackingSurface()
    {
        nsRefPtr<gfxASurface> copy = mBackingSurface;
        return copy.forget();
    }

    virtual bool CreateEGLSurface(gfxASurface* aSurface)
    {
#ifdef MOZ_X11
        if (!aSurface) {
            NS_WARNING("no surface");
            return false;
        }

        if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib) {
            NS_WARNING("wrong surface type, must be xlib");
            return false;
        }

        if (mSurface) {
            return true;
        }

        EGLSurface surface = CreateEGLSurfaceForXSurface(aSurface, &mConfig);

        if (!surface) {
            NS_WARNING("couldn't find X config for surface");
            return false;
        }

        mSurface = surface;
        return true;
#else
        return false;
#endif
    }

    virtual void DestroyEGLSurface(void)
    {
#ifdef MOZ_WIDGET_GONK
        mGraphicBuffer.clear();

        if (mEGLImage) {
            sEGLLibrary.fDestroyImage(EGL_DISPLAY(), mEGLImage);
            mEGLImage = nsnull;
        }
#endif

        if (!mSurface)
            return;

        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
        mSurface = nsnull;
    }

    virtual bool CreateBackingSurface(const gfxIntSize& aSize)
    {
        ReleaseTexImage();
        DestroyEGLSurface();
        mBackingSurface = nsnull;

#ifdef MOZ_X11
        Display* dpy = DefaultXDisplay();
        XRenderPictFormat* renderFMT =
            gfxXlibSurface::FindRenderFormat(dpy, mUpdateFormat);

        nsRefPtr<gfxXlibSurface> xsurface =
            gfxXlibSurface::Create(DefaultScreenOfDisplay(dpy),
                                   renderFMT,
                                   gfxIntSize(aSize.width, aSize.height));

        XSync(dpy, False);
        mConfig = nsnull;

        if (sEGLLibrary.HasKHRImagePixmap() &&
            mGLContext->IsExtensionSupported(GLContext::OES_EGL_image))
        {
            mEGLImage =
                sEGLLibrary.fCreateImage(EGL_DISPLAY(),
                                         EGL_NO_CONTEXT,
                                         LOCAL_EGL_NATIVE_PIXMAP,
                                         (EGLClientBuffer)xsurface->XDrawable(),
                                         nsnull);

            if (!mEGLImage) {
                printf_stderr("couldn't create EGL image: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return false;
            }
            mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
            mGLContext->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mEGLImage);
            sEGLLibrary.fDestroyImage(EGL_DISPLAY(), mEGLImage);
            mEGLImage = nsnull;
        } else {
            if (!CreateEGLSurface(xsurface)) {
                printf_stderr("ProviderEGL Failed create EGL surface: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return false;
            }

            if (!BindTexImage()) {
                printf_stderr("ProviderEGL Failed to bind teximage: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return false;
            }
        }

        mBackingSurface = xsurface;

        return mBackingSurface != nsnull;
#endif

#ifdef MOZ_WIDGET_GONK
        if (gUseBackingSurface) {
            mGLContext->MakeCurrent(true);
            PixelFormat format = PixelFormatForImage(mUpdateFormat);
            uint32_t usage = GraphicBuffer::USAGE_HW_TEXTURE |
                             GraphicBuffer::USAGE_SW_READ_OFTEN |
                             GraphicBuffer::USAGE_SW_WRITE_OFTEN;
            mGraphicBuffer = new GraphicBuffer(aSize.width, aSize.height, format, usage);
            if (mGraphicBuffer->initCheck() == OK) {
                const int eglImageAttributes[] = { LOCAL_EGL_IMAGE_PRESERVED, LOCAL_EGL_TRUE,
                                                   LOCAL_EGL_NONE, LOCAL_EGL_NONE };
                mEGLImage = sEGLLibrary.fCreateImage(EGL_DISPLAY(),
                                                        EGL_NO_CONTEXT,
                                                        EGL_NATIVE_BUFFER_ANDROID,
                                                        (EGLClientBuffer) mGraphicBuffer->getNativeBuffer(),
                                                        eglImageAttributes);
                if (!mEGLImage) {
                    mGraphicBuffer = nsnull;
                    LOG("Could not create EGL images: ERROR (0x%04x)", sEGLLibrary.fGetError());
                    return false;
                }

                return true;
            }

            mGraphicBuffer = nsnull;
            LOG("GraphicBufferAllocator::alloc failed");
            return false;
        }
#endif
        return mBackingSurface != nsnull;
    }

protected:
    typedef gfxASurface::gfxImageFormat ImageFormat;

    GLContext* mGLContext;

    nsIntRect mUpdateRect;
    ImageFormat mUpdateFormat;
    bool mUsingDirectTexture;
    nsRefPtr<gfxASurface> mBackingSurface;
    nsRefPtr<gfxASurface> mUpdateSurface;
#ifdef MOZ_WIDGET_GONK
    sp<GraphicBuffer> mGraphicBuffer;
#endif
    EGLImage mEGLImage;
    GLuint mTexture;
    EGLSurface mSurface;
    EGLConfig mConfig;
    TextureState mTextureState;

    bool mBound;
    bool mIsLocked;

    virtual void ApplyFilter()
    {
        mGLContext->ApplyFilterToBoundTexture(mFilter);
    }
};

#ifdef MOZ_WIDGET_GONK

class DirectTextureImageEGL
    : public TextureImageEGL
{
public:
    DirectTextureImageEGL(GLuint aTexture,
                          sp<GraphicBuffer> aGraphicBuffer,
                          GLenum aWrapMode,
                          GLContext* aContext)
        : TextureImageEGL(aTexture,
                          nsIntSize(aGraphicBuffer->getWidth(), aGraphicBuffer->getHeight()),
                          aWrapMode,
                          ContentTypeForPixelFormat(aGraphicBuffer->getPixelFormat()),
                          aContext,
                          ForceSingleTile,
                          Valid)
    {
        mGraphicBuffer = aGraphicBuffer;

        const int eglImageAttributes[] =
            { LOCAL_EGL_IMAGE_PRESERVED, LOCAL_EGL_TRUE,
              LOCAL_EGL_NONE, LOCAL_EGL_NONE };

        mEGLImage = sEGLLibrary.fCreateImage(EGL_DISPLAY(),
                                             EGL_NO_CONTEXT,
                                             EGL_NATIVE_BUFFER_ANDROID,
                                             mGraphicBuffer->getNativeBuffer(),
                                             eglImageAttributes);
        if (!mEGLImage) {
            LOG("Could not create EGL images: ERROR (0x%04x)", sEGLLibrary.fGetError());
        }
    }
};

#endif  // MOZ_WIDGET_GONK

already_AddRefed<TextureImage>
GLContextEGL::CreateTextureImage(const nsIntSize& aSize,
                                 TextureImage::ContentType aContentType,
                                 GLenum aWrapMode,
                                 TextureImage::Flags aFlags)
{
    nsRefPtr<TextureImage> t = new gl::TiledTextureImage(this, aSize, aContentType, aFlags);
    return t.forget();
}

#ifdef MOZ_WIDGET_GONK
already_AddRefed<TextureImage>
GLContextEGL::CreateDirectTextureImage(GraphicBuffer* aBuffer,
                                       GLenum aWrapMode)
{
    MakeCurrent();

    GLuint texture;
    fGenTextures(1, &texture);

    nsRefPtr<TextureImage> texImage(
        new DirectTextureImageEGL(texture, aBuffer, aWrapMode, this));
    texImage->BindTexture(LOCAL_GL_TEXTURE0);

    GLint texfilter = LOCAL_GL_LINEAR;
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, aWrapMode);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, aWrapMode);

    return texImage.forget();
}
#endif  // MOZ_WIDGET_GONK

already_AddRefed<TextureImage>
GLContextEGL::TileGenFunc(const nsIntSize& aSize,
                                 TextureImage::ContentType aContentType,
                                 TextureImage::Flags aFlags)
{
  MakeCurrent();

  GLuint texture;
  fGenTextures(1, &texture);

  nsRefPtr<TextureImageEGL> teximage =
      new TextureImageEGL(texture, aSize, LOCAL_GL_CLAMP_TO_EDGE, aContentType, this, aFlags);
  
  teximage->BindTexture(LOCAL_GL_TEXTURE0);

  GLint texfilter = aFlags & TextureImage::UseNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  return teximage.forget();
}

inline static ContextFormat
DepthToGLFormat(int aDepth)
{
    switch (aDepth) {
        case 32:
            return ContextFormat::BasicRGBA32;
        case 24:
            return ContextFormat::BasicRGB24;
        case 16:
            return ContextFormat::BasicRGB16_565;
        default:
            break;
    }
    return ContextFormat::BasicRGBA32;
}

static nsRefPtr<GLContext> gGlobalContext;

static const EGLint kEGLConfigAttribsRGB16[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_RED_SIZE,        5,
    LOCAL_EGL_GREEN_SIZE,      6,
    LOCAL_EGL_BLUE_SIZE,       5,
    LOCAL_EGL_ALPHA_SIZE,      0,
    LOCAL_EGL_NONE
};

static const EGLint kEGLConfigAttribsRGB24[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_RED_SIZE,        8,
    LOCAL_EGL_GREEN_SIZE,      8,
    LOCAL_EGL_BLUE_SIZE,       8,
    LOCAL_EGL_NONE
};

static const EGLint kEGLConfigAttribsRGBA32[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_RED_SIZE,        8,
    LOCAL_EGL_GREEN_SIZE,      8,
    LOCAL_EGL_BLUE_SIZE,       8,
    LOCAL_EGL_ALPHA_SIZE,      8,
    LOCAL_EGL_NONE
};

static bool
CreateConfig(EGLConfig* aConfig, PRInt32 depth)
{
    EGLConfig configs[64];
    const EGLint* attribs;
    EGLint ncfg = ArrayLength(configs);

    switch (depth) {
        case 16:
            attribs = kEGLConfigAttribsRGB16;
            break;
        case 24:
            attribs = kEGLConfigAttribsRGB24;
            break;
        case 32:
            attribs = kEGLConfigAttribsRGBA32;
            break;
        default:
            NS_ERROR("Unknown pixel depth");
            return false;
    }

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(), attribs,
                                   configs, ncfg, &ncfg) ||
        ncfg < 1) {
        return false;
    }

    for (int j = 0; j < ncfg; ++j) {
        EGLConfig config = configs[j];
        EGLint r, g, b, a;

        if (sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), config,
                                         LOCAL_EGL_RED_SIZE, &r) &&
            sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), config,
                                         LOCAL_EGL_GREEN_SIZE, &g) &&
            sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), config,
                                         LOCAL_EGL_BLUE_SIZE, &b) &&
            sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), config,
                                         LOCAL_EGL_ALPHA_SIZE, &a) &&
            ((depth == 16 && r == 5 && g == 6 && b == 5) ||
             (depth == 24 && r == 8 && g == 8 && b == 8) ||
             (depth == 32 && r == 8 && g == 8 && b == 8 && a == 8)))
        {
            *aConfig = config;
            return true;
        }
    }
    return false;
}

// Return true if a suitable EGLConfig was found and pass it out
// through aConfig.  Return false otherwise.
//
// NB: It's entirely legal for the returned EGLConfig to be valid yet
// have the value null.
static bool
CreateConfig(EGLConfig* aConfig)
{
    PRInt32 depth = gfxPlatform::GetPlatform()->GetScreenDepth();
    if (!CreateConfig(aConfig, depth)) {
#ifdef MOZ_WIDGET_ANDROID
        // Bug 736005
        // Android doesn't always support 16 bit so also try 24 bit
        if (depth == 16) {
            return CreateConfig(aConfig, 24);
        }
#endif
        return false;
    } else {
        return true;
    }
}

// When MOZ_JAVA_COMPOSITOR is defined,
// use mozilla::AndroidBridge::Bridge()->ProvideEGLSurface() instead.
#ifndef MOZ_JAVA_COMPOSITOR
static EGLSurface
CreateSurfaceForWindow(nsIWidget *aWidget, EGLConfig config)
{
    EGLSurface surface;

#ifdef DEBUG
    sEGLLibrary.DumpEGLConfig(config);
#endif

#if defined(MOZ_WIDGET_ANDROID)

    // On Android, we have to ask Java to make the eglCreateWindowSurface
    // call for us.  See GLHelpers.java for a description of why.
    //
    // We also only have one true "window", so we just use it directly and ignore
    // what was passed in.
    AndroidGeckoSurfaceView& sview = mozilla::AndroidBridge::Bridge()->SurfaceView();
    if (sview.isNull()) {
        printf_stderr("got null surface\n");
        return NULL;
    }

    surface = mozilla::AndroidBridge::Bridge()->
        CallEglCreateWindowSurface(EGL_DISPLAY(), config, sview);
#else
    surface = sEGLLibrary.fCreateWindowSurface(EGL_DISPLAY(), config, GET_NATIVE_WINDOW(aWidget), 0);
#endif

#ifdef MOZ_WIDGET_GONK
    gScreenBounds.x = 0;
    gScreenBounds.y = 0;
    sEGLLibrary.fQuerySurface(EGL_DISPLAY(), surface, LOCAL_EGL_WIDTH, &gScreenBounds.width);
    sEGLLibrary.fQuerySurface(EGL_DISPLAY(), surface, LOCAL_EGL_HEIGHT, &gScreenBounds.height);
#endif

    return surface;
}
#endif

already_AddRefed<GLContext>
GLContextProviderEGL::CreateForWindow(nsIWidget *aWidget)
{
    EGLConfig config;

    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    bool doubleBuffered = true;

    void* currentContext = sEGLLibrary.fGetCurrentContext();
    if (aWidget->HasGLContext() && currentContext) {
        PRInt32 depth = gfxPlatform::GetPlatform()->GetScreenDepth();
        void* platformContext = currentContext;
#ifdef MOZ_WIDGET_QT
        QGLContext* context = const_cast<QGLContext*>(QGLContext::currentContext());
        if (context && context->device()) {
            depth = context->device()->depth();
        }
        doubleBuffered = context->format().doubleBuffer();
        platformContext = context;
#endif
        nsRefPtr<GLContextEGL> glContext =
            new GLContextEGL(ContextFormat(DepthToGLFormat(depth)),
                             gGlobalContext,
                             NULL,
                             sEGLLibrary.fGetCurrentSurface(LOCAL_EGL_DRAW), // just use same surface for read and draw
                             currentContext,
                             false);

        if (!glContext->Init())
            return nsnull;

        glContext->MakeCurrent();
        sEGLLibrary.LoadConfigSensitiveSymbols();

        glContext->SetIsDoubleBuffered(doubleBuffered);

        glContext->SetPlatformContext(platformContext);
        if (!gGlobalContext) {
            gGlobalContext = glContext;
        }

        return glContext.forget();
    }

    if (!CreateConfig(&config)) {
        printf_stderr("Failed to create EGL config!\n");
        return nsnull;
    }

#ifdef MOZ_JAVA_COMPOSITOR
    mozilla::AndroidBridge::Bridge()->RegisterCompositor();
    EGLSurface surface = mozilla::AndroidBridge::Bridge()->ProvideEGLSurface();
#else
    EGLSurface surface = CreateSurfaceForWindow(aWidget, config);
#endif

    if (!surface) {
        return nsnull;
    }

    if (!sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API)) {
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    GLContextEGL *shareContext = GetGlobalContextEGL();

    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(ContextFormat(ContextFormat::BasicRGB24),
                                      surface,
                                      config,
                                      shareContext,
                                      false);

    if (!glContext) {
        return nsnull;
    }

    glContext->MakeCurrent();
    sEGLLibrary.LoadConfigSensitiveSymbols();
    glContext->SetIsDoubleBuffered(doubleBuffered);

    return glContext.forget();
}

static void
FillPBufferAttribs_Minimal(nsTArray<EGLint>& aAttrs)
{
    aAttrs.Clear();

#define A1(_x)      do { aAttrs.AppendElement(_x); } while (0)
#define A2(_x,_y)   do { A1(_x); A1(_y); } while (0)

    A2(LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT);

    A2(LOCAL_EGL_SURFACE_TYPE, LOCAL_EGL_PBUFFER_BIT);

    A1(LOCAL_EGL_NONE);
#undef A1
#undef A2
}

static void
FillPBufferAttribs(nsTArray<EGLint>& aAttrs,
                   const ContextFormat& aFormat,
                   bool aCanBindToTexture,
                   int aColorBitsOverride,
                   int aDepthBitsOverride)
{
    aAttrs.Clear();

#define A1(_x)      do { aAttrs.AppendElement(_x); } while (0)
#define A2(_x,_y)   do { A1(_x); A1(_y); } while (0)

    A2(LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT);

    A2(LOCAL_EGL_SURFACE_TYPE, LOCAL_EGL_PBUFFER_BIT);

    if (aColorBitsOverride == -1) {
        A2(LOCAL_EGL_RED_SIZE, aFormat.red);
        A2(LOCAL_EGL_GREEN_SIZE, aFormat.green);
        A2(LOCAL_EGL_BLUE_SIZE, aFormat.blue);
    } else {
        A2(LOCAL_EGL_RED_SIZE, aColorBitsOverride);
        A2(LOCAL_EGL_GREEN_SIZE, aColorBitsOverride);
        A2(LOCAL_EGL_BLUE_SIZE, aColorBitsOverride);
    }

    A2(LOCAL_EGL_ALPHA_SIZE, aFormat.alpha);

    if (aDepthBitsOverride == -1) {
        A2(LOCAL_EGL_DEPTH_SIZE, aFormat.minDepth);
    } else {
        A2(LOCAL_EGL_DEPTH_SIZE, aDepthBitsOverride);
    }

    A2(LOCAL_EGL_STENCIL_SIZE, aFormat.minStencil);

    if (aCanBindToTexture) {
        A2(aFormat.minAlpha ? LOCAL_EGL_BIND_TO_TEXTURE_RGBA : LOCAL_EGL_BIND_TO_TEXTURE_RGB,
           LOCAL_EGL_TRUE);
    }

    A1(LOCAL_EGL_NONE);
#undef A1
#undef A2
}

already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPBufferOffscreenContext(const gfxIntSize& aSize,
                                               const ContextFormat& aFormat,
                                               bool bufferUnused)
{
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    bool configCanBindToTexture = true;

    EGLConfig configs[64];
    int numConfigs = sizeof(configs)/sizeof(EGLConfig);
    int foundConfigs = 0;

    // if we're running under ANGLE, we can't set BIND_TO_TEXTURE --
    // it's not supported, and we have dx interop pbuffers anyway
    if (sEGLLibrary.IsANGLE() || bufferUnused)
        configCanBindToTexture = false;

    nsTArray<EGLint> attribs(32);
    int attribAttempt = 0;

    int tryDepthSize = (aFormat.depth > 0) ? 24 : 0;

TRY_ATTRIBS_AGAIN:
    if (bufferUnused) {
        FillPBufferAttribs_Minimal(attribs);
    } else {
        switch (attribAttempt) {
        case 0:
            FillPBufferAttribs(attribs, aFormat, configCanBindToTexture, 8, tryDepthSize);
            break;
        case 1:
            FillPBufferAttribs(attribs, aFormat, configCanBindToTexture, -1, tryDepthSize);
            break;
        case 2:
            FillPBufferAttribs(attribs, aFormat, configCanBindToTexture, -1, -1);
            break;
        }
    }

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                   &attribs[0],
                                   configs, numConfigs,
                                   &foundConfigs)
        || foundConfigs == 0)
    {
        if (bufferUnused) {
            NS_WARNING("No EGL Config for minimal PBuffer!");
            return nsnull;
        }

        if (attribAttempt < 3) {
            attribAttempt++;
            goto TRY_ATTRIBS_AGAIN;
        }

        if (configCanBindToTexture) {
            NS_WARNING("No pbuffer EGL configs that can bind to texture, trying without");
            configCanBindToTexture = false;
            attribAttempt = 0;
            goto TRY_ATTRIBS_AGAIN;
        }

        // no configs? no pbuffers!
        NS_WARNING("Failed to select acceptable config for PBuffer creation!");
        return nsnull;
    }

    // XXX do some smarter matching here, perhaps instead of the more complex
    // minimum overrides above
    config = configs[0];
#ifdef DEBUG
    sEGLLibrary.DumpEGLConfig(config);
#endif

    gfxIntSize pbsize(aSize);
    surface = GLContextEGL::CreatePBufferSurfaceTryingPowerOfTwo(config,
                                                                 configCanBindToTexture
                                                                 ? (aFormat.minAlpha
                                                                    ? LOCAL_EGL_TEXTURE_RGBA
                                                                    : LOCAL_EGL_TEXTURE_RGB)
                                                                 : LOCAL_EGL_NONE,
                                                                 pbsize);
    if (!surface) {
        NS_WARNING("Failed to create PBuffer for context!");
        return nsnull;
    }

    sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API);

    GLContextEGL* shareContext = GetGlobalContextEGL();
    context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                         config,
                                         shareContext ? shareContext->mContext
                                                      : EGL_NO_CONTEXT,
                                         sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                                     : gContextAttribs);
    if (!context) { 
        NS_WARNING("Failed to create GLContext from PBuffer");
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    nsRefPtr<GLContextEGL> glContext = new GLContextEGL(aFormat, shareContext,
                                                        config, surface, context,
                                                        true);

    if (!glContext->Init()) {
        NS_WARNING("Failed to initialize GLContext!");
        return nsnull;
    }

    glContext->mCanBindToTexture = configCanBindToTexture;

    if (!bufferUnused) {  // We *are* using the buffer
      glContext->SetOffscreenSize(aSize, pbsize);
      glContext->mIsPBuffer = true;
    }

    return glContext.forget();
}

#ifdef MOZ_X11
EGLSurface
CreateEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig)
{
    gfxXlibSurface* xsurface = static_cast<gfxXlibSurface*>(aSurface);
    bool opaque =
        aSurface->GetContentType() == gfxASurface::CONTENT_COLOR;

    static EGLint pixmap_config_rgb[] = {
        LOCAL_EGL_TEXTURE_TARGET,       LOCAL_EGL_TEXTURE_2D,
        LOCAL_EGL_TEXTURE_FORMAT,       LOCAL_EGL_TEXTURE_RGB,
        LOCAL_EGL_NONE
    };

    static EGLint pixmap_config_rgba[] = {
        LOCAL_EGL_TEXTURE_TARGET,       LOCAL_EGL_TEXTURE_2D,
        LOCAL_EGL_TEXTURE_FORMAT,       LOCAL_EGL_TEXTURE_RGBA,
        LOCAL_EGL_NONE
    };

    EGLSurface surface = nsnull;
    if (aConfig && *aConfig) {
        if (opaque)
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), *aConfig,
                                                       (EGLNativePixmapType)xsurface->XDrawable(),
                                                       pixmap_config_rgb);
        else
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), *aConfig,
                                                       (EGLNativePixmapType)xsurface->XDrawable(),
                                                       pixmap_config_rgba);

        if (surface != EGL_NO_SURFACE)
            return surface;
    }

    EGLConfig configs[32];
    int numConfigs = 32;

    static EGLint pixmap_config[] = {
        LOCAL_EGL_SURFACE_TYPE,         LOCAL_EGL_PIXMAP_BIT,
        LOCAL_EGL_RENDERABLE_TYPE,      LOCAL_EGL_OPENGL_ES2_BIT,
        LOCAL_EGL_DEPTH_SIZE,           0,
        LOCAL_EGL_BIND_TO_TEXTURE_RGB,  LOCAL_EGL_TRUE,
        LOCAL_EGL_NONE
    };

    static EGLint pixmap_lock_config[] = {
        LOCAL_EGL_SURFACE_TYPE,         LOCAL_EGL_PIXMAP_BIT | LOCAL_EGL_LOCK_SURFACE_BIT_KHR,
        LOCAL_EGL_RENDERABLE_TYPE,      LOCAL_EGL_OPENGL_ES2_BIT,
        LOCAL_EGL_DEPTH_SIZE,           0,
        LOCAL_EGL_BIND_TO_TEXTURE_RGB,  LOCAL_EGL_TRUE,
        LOCAL_EGL_NONE
    };

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                   sEGLLibrary.HasKHRLockSurface() ?
                                       pixmap_lock_config : pixmap_config,
                                   configs, numConfigs, &numConfigs))
        return nsnull;

    if (numConfigs == 0)
        return nsnull;

    int i = 0;
    for (i = 0; i < numConfigs; ++i) {
        if (opaque)
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), configs[i],
                                                       (EGLNativePixmapType)xsurface->XDrawable(),
                                                       pixmap_config_rgb);
        else
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), configs[i],
                                                       (EGLNativePixmapType)xsurface->XDrawable(),
                                                       pixmap_config_rgba);

        if (surface != EGL_NO_SURFACE)
            break;
    }

    if (!surface) {
        return nsnull;
    }

    if (aConfig)
        *aConfig = configs[i];

    return surface;
}
#endif

already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                              const ContextFormat& aFormat,
                                              bool aShare)
{
    gfxASurface *thebesSurface = nsnull;
    EGLNativePixmapType pixmap = 0;

#ifdef MOZ_X11
    nsRefPtr<gfxXlibSurface> xsurface =
        gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()),
                               gfxXlibSurface::FindRenderFormat(DefaultXDisplay(),
                                                                gfxASurface::ImageFormatRGB24),
                               aSize);

    // XSync required after gfxXlibSurface::Create, otherwise EGL will fail with BadDrawable error
    XSync(DefaultXDisplay(), False);
    if (xsurface->CairoStatus() != 0)
        return nsnull;

    thebesSurface = xsurface;
    pixmap = (EGLNativePixmapType)xsurface->XDrawable();
#endif

    if (!pixmap) {
        return nsnull;
    }

    EGLSurface surface = 0;
    EGLConfig config = 0;

#ifdef MOZ_X11
    surface = CreateEGLSurfaceForXSurface(thebesSurface, &config);
#endif
    if (!config) {
        return nsnull;
    }

    GLContextEGL *shareContext = aShare ? GetGlobalContextEGL() : nsnull;

    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(aFormat,
                                      surface,
                                      config,
                                      shareContext,
                                      true);

    glContext->HoldSurface(thebesSurface);

    return glContext.forget();
}

// Under EGL, if we're under X11, then we have to create a Pixmap
// because Maemo's EGL implementation doesn't support pbuffers at all
// for some reason.  On Android, pbuffers are supported fine, though
// often without the ability to texture from them directly.
already_AddRefed<GLContext>
GLContextProviderEGL::CreateOffscreen(const gfxIntSize& aSize,
                                      const ContextFormat& aFormat,
                                      const ContextFlags aFlags)
{
    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

#if !defined(MOZ_X11)
    bool usePBuffers = false; // Generally, prefer FBOs to PBuffers

    if (sEGLLibrary.IsANGLE())
      usePBuffers = true; // For d3d share handle, we need an EGL surface

    gfxIntSize pbufferSize = usePBuffers ? aSize : gfxIntSize(16, 16);
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateEGLPBufferOffscreenContext(pbufferSize, aFormat, !usePBuffers);

    if (!glContext)
        return nsnull;

    gfxIntSize fboSize = usePBuffers ? glContext->OffscreenActualSize() : aSize;
    if (!(aFlags & GLContext::ContextFlagsGlobal) && !glContext->ResizeOffscreenFBOs(fboSize, !usePBuffers))
        return nsnull;

    return glContext.forget();
#elif defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateBasicEGLPixmapOffscreenContext(aSize, aFormat);

    if (!glContext)
        return nsnull;

    if (!(aFlags & GLContext::ContextFlagsGlobal) && !glContext->ResizeOffscreenFBOs(glContext->OffscreenActualSize(), true))
        return nsnull;

    return glContext.forget();
#elif defined(MOZ_X11)
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateEGLPixmapOffscreenContext(gfxIntSize(16, 16), aFormat, true);

    if (!glContext) {
        return nsnull;
    }

    if (!(aFlags & GLContext::ContextFlagsGlobal) && !glContext->ResizeOffscreenFBOs(aSize, true)) {
        // we weren't able to create the initial
        // offscreen FBO, so this is dead
        return nsnull;
    }
    return glContext.forget();
#else
    return nsnull;
#endif
}

already_AddRefed<GLContext>
GLContextProviderEGL::CreateForNativePixmapSurface(gfxASurface* aSurface)
{
    if (!sEGLLibrary.EnsureInitialized())
        return nsnull;

#ifdef MOZ_X11
    EGLSurface surface = nsnull;
    EGLConfig config = nsnull;

    if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib) {
        // Not implemented
        return nsnull;
    }

    surface = CreateEGLSurfaceForXSurface(aSurface, &config);
    if (!config) {
        return nsnull;
    }

    GLContextEGL *shareContext = GetGlobalContextEGL();
    gfxXlibSurface* xsurface = static_cast<gfxXlibSurface*>(aSurface);

    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(DepthToGLFormat(xsurface->XRenderFormat()->depth),
                                      surface, config, shareContext, false);

    glContext->HoldSurface(aSurface);

    return glContext.forget().get();
#else
    // Not implemented
    return nsnull;
#endif
}

GLContext *
GLContextProviderEGL::GetGlobalContext(const ContextFlags)
{
// Don't want a global context on Android as 1) share groups across 2 threads fail on many Tegra drivers (bug 759225)
// and 2) some mobile devices have a very strict limit on global number of GL contexts (bug 754257)
#ifdef MOZ_JAVA_COMPOSITOR
    return nsnull;
#endif

// Don't want a global context on Windows as we don't use it for WebGL surface sharing and it makes
// context creation fail after a context loss (bug 764578)
#ifdef XP_WIN
    return nsnull;
#endif


    static bool triedToCreateContext = false;
    if (!triedToCreateContext && !gGlobalContext) {
        triedToCreateContext = true;
        // Don't assign directly to gGlobalContext here, because
        // CreateOffscreen can call us re-entrantly.
        nsRefPtr<GLContext> ctx =
            GLContextProviderEGL::CreateOffscreen(gfxIntSize(16, 16),
                                                  ContextFormat(ContextFormat::BasicRGB24),
                                                  GLContext::ContextFlagsGlobal);
        gGlobalContext = ctx;
        if (gGlobalContext)
            gGlobalContext->SetIsGlobalSharedContext(true);
    }

    return gGlobalContext;
}

void
GLContextProviderEGL::Shutdown()
{
    gGlobalContext = nsnull;
}

//------------------------------------------------------------------------------
// The following methods exist to support an accelerated WebGL XRender composite
// path for BasicLayers. This is a potentially temporary change that can be
// removed when performance of GL layers is superior on mobile linux platforms.
//------------------------------------------------------------------------------
#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)

EGLSurface
CreateBasicEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig)
{
  gfxXlibSurface* xsurface = static_cast<gfxXlibSurface*>(aSurface);

  bool opaque =
    aSurface->GetContentType() == gfxASurface::CONTENT_COLOR;

  EGLSurface surface = nsnull;
  if (aConfig && *aConfig) {
    surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), *aConfig,
                                               xsurface->XDrawable(),
                                               0);

    if (surface != EGL_NO_SURFACE)
      return surface;
  }

  EGLConfig configs[32];
  int numConfigs = 32;

  static EGLint pixmap_config[] = {
      LOCAL_EGL_SURFACE_TYPE,         LOCAL_EGL_PIXMAP_BIT,
      LOCAL_EGL_RENDERABLE_TYPE,      LOCAL_EGL_OPENGL_ES2_BIT,
      0x30E2, 0x30E3,
      LOCAL_EGL_DEPTH_SIZE,           16,
      LOCAL_EGL_NONE
  };

  if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                 pixmap_config,
                                 configs, numConfigs, &numConfigs))
      return nsnull;

  if (numConfigs == 0)
      return nsnull;

  int i = 0;
  for (i = 0; i < numConfigs; ++i) {
    surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), configs[i],
                                               xsurface->XDrawable(),
                                               0);

    if (surface != EGL_NO_SURFACE)
      break;
  }

  if (!surface) {
    return nsnull;
  }

  if (aConfig)
  {
    *aConfig = configs[i];
  }

  return surface;
}

already_AddRefed<GLContextEGL>
GLContextEGL::CreateBasicEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                              const ContextFormat& aFormat)
{
  gfxASurface *thebesSurface = nsnull;
  EGLNativePixmapType pixmap = 0;

  XRenderPictFormat* format = gfxXlibSurface::FindRenderFormat(DefaultXDisplay(), gfxASurface::ImageFormatARGB32);

  nsRefPtr<gfxXlibSurface> xsurface =
    gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()), format, aSize);

  // XSync required after gfxXlibSurface::Create, otherwise EGL will fail with BadDrawable error
  XSync(DefaultXDisplay(), False);
  if (xsurface->CairoStatus() != 0)
  {
    return nsnull;
  }

  thebesSurface = xsurface;

  pixmap = xsurface->XDrawable();

  if (!pixmap) {
    return nsnull;
  }

  EGLSurface surface = 0;
  EGLConfig config = 0;

  surface = CreateBasicEGLSurfaceForXSurface(xsurface, &config);

  if (!config) {
    return nsnull;
  }

  EGLContext context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                                  config,
                                                  EGL_NO_CONTEXT,
                                                  sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                                              : gContextAttribs);
  if (!context) {
    sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
    return nsnull;
  }

  nsRefPtr<GLContextEGL> glContext = new GLContextEGL(aFormat, nsnull,
                                                      config, surface, context,
                                                      true);

  if (!glContext->Init())
  {
    return nsnull;
  }

  glContext->HoldSurface(thebesSurface);

  return glContext.forget();
}

bool GLContextEGL::ResizeOffscreenPixmapSurface(const gfxIntSize& aNewSize)
{
  gfxASurface *thebesSurface = nsnull;
  EGLNativePixmapType pixmap = 0;

  XRenderPictFormat* format = gfxXlibSurface::FindRenderFormat(DefaultXDisplay(), gfxASurface::ImageFormatARGB32);

  nsRefPtr<gfxXlibSurface> xsurface =
    gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()),
                           format,
                           aNewSize);

  // XSync required after gfxXlibSurface::Create, otherwise EGL will fail with BadDrawable error
  XSync(DefaultXDisplay(), False);
  if (xsurface->CairoStatus() != 0)
    return nsnull;

  thebesSurface = xsurface;

  pixmap = xsurface->XDrawable();

  if (!pixmap) {
    return nsnull;
  }

  EGLSurface surface = 0;
  EGLConfig config = 0;
  surface = CreateBasicEGLSurfaceForXSurface(xsurface, &config);
  if (!surface) {
    NS_WARNING("Failed to resize pbuffer");
    return nsnull;
  }

  sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);

  mSurface = surface;
  HoldSurface(thebesSurface);
  SetOffscreenSize(aNewSize, aNewSize);
  MakeCurrent(true);

  return true;
}

#endif

} /* namespace gl */
} /* namespace mozilla */


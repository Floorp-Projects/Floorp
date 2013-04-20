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
#include "HwcComposer2D.h"
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

#define EGL_DISPLAY()        sEGLLibrary.Display()

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

#ifndef MOZ_ANDROID_OMTC
static EGLSurface
CreateSurfaceForWindow(nsIWidget *aWidget, EGLConfig config);
#endif

static bool
CreateConfig(EGLConfig* aConfig);
#ifdef MOZ_X11

static EGLConfig
CreateEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig = nullptr);
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
    CreateGLContext(const SurfaceCaps& caps,
                    GLContextEGL *shareContext,
                    bool isOffscreen,
                    EGLConfig config,
                    EGLSurface surface)
    {
        if (sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API) == LOCAL_EGL_FALSE) {
            NS_WARNING("Failed to bind API to GLES!");
            return nullptr;
        }

        EGLContext eglShareContext = shareContext ? shareContext->mContext
                                                  : EGL_NO_CONTEXT;
        EGLint* attribs = sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                      : gContextAttribs;

        EGLContext context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                                        config,
                                                        eglShareContext,
                                                        attribs);
        if (!context && shareContext) {
            shareContext = nullptr;
            context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                                 config,
                                                 EGL_NO_CONTEXT,
                                                 attribs);
        }
        if (!context) {
            NS_WARNING("Failed to create EGLContext!");
            return nullptr;
        }

        nsRefPtr<GLContextEGL> glContext = new GLContextEGL(caps,
                                                            shareContext,
                                                            isOffscreen,
                                                            config,
                                                            surface,
                                                            context);

        if (!glContext->Init())
            return nullptr;

        return glContext.forget();
    }

public:
    GLContextEGL(const SurfaceCaps& caps,
                 GLContext* shareContext,
                 bool isOffscreen,
                 EGLConfig config,
                 EGLSurface surface,
                 EGLContext context)
        : GLContext(caps, shareContext, isOffscreen)
        , mConfig(config)
        , mSurface(surface)
        , mCurSurface(surface)
        , mContext(context)
        , mPlatformContext(nullptr)
        , mThebesSurface(nullptr)
        , mBound(false)
        , mIsPBuffer(false)
        , mIsDoubleBuffered(false)
        , mCanBindToTexture(false)
        , mShareWithEGLImage(false)
        , mTemporaryEGLImageTexture(0)
    {
        // any EGL contexts will always be GLESv2
        SetIsGLES2(true);

#ifdef DEBUG
        printf_stderr("Initializing context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif
#ifdef MOZ_WIDGET_GONK
        if (!mIsOffscreen) {
            mHwc = HwcComposer2D::GetInstance();
            MOZ_ASSERT(!mHwc->Initialized());

            if (mHwc->Init(EGL_DISPLAY(), mSurface)) {
                NS_WARNING("HWComposer initialization failed!");
                mHwc = nullptr;
            }
        }
#endif
    }

    ~GLContextEGL()
    {
        if (MakeCurrent()) {
            if (mTemporaryEGLImageTexture != 0) {
                fDeleteTextures(1, &mTemporaryEGLImageTexture);
                mTemporaryEGLImageTexture = 0;
            }
        }

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
        if (!InitWithPrefix("gl", true))
            return false;

        PR_STATIC_ASSERT(sizeof(GLint) >= sizeof(int32_t));
        mMaxTextureImageSize = INT32_MAX;

        mShareWithEGLImage = sEGLLibrary.HasKHRImageBase() &&
                             sEGLLibrary.HasKHRImageTexture2D() &&
                             IsExtensionSupported(OES_EGL_image);

        return true;
    }

    bool IsDoubleBuffered() {
        return mIsDoubleBuffered;
    }

    void SetIsDoubleBuffered(bool aIsDB) {
        mIsDoubleBuffered = aIsDB;
    }

    virtual EGLContext GetEGLContext() {
        return mContext;
    }

    virtual GLLibraryEGL* GetLibraryEGL() {
        return &sEGLLibrary;
    }


    bool SupportsRobustness()
    {
        return sEGLLibrary.HasRobustness();
    }

    virtual bool IsANGLE()
    {
        return sEGLLibrary.IsANGLE();
    }

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

#ifdef MOZ_WIDGET_GONK
    EGLImage CreateEGLImageForNativeBuffer(void* buffer) MOZ_OVERRIDE
    {
        EGLint attrs[] = {
            LOCAL_EGL_IMAGE_PRESERVED, LOCAL_EGL_TRUE,
            LOCAL_EGL_NONE, LOCAL_EGL_NONE
        };
        return sEGLLibrary.fCreateImage(EGL_DISPLAY(),
                                        EGL_NO_CONTEXT,
                                        LOCAL_EGL_NATIVE_BUFFER_ANDROID,
                                        buffer, attrs);
    }

    void DestroyEGLImage(EGLImage image) MOZ_OVERRIDE
    {
        sEGLLibrary.fDestroyImage(EGL_DISPLAY(), image);
    }

    EGLImage GetNullEGLImage() MOZ_OVERRIDE
    {
        if (!mNullGraphicBuffer.get()) {
            mNullGraphicBuffer
              = new android::GraphicBuffer(
                  1, 1,
                  PIXEL_FORMAT_RGB_565,
                  GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_NEVER);
            EGLint attrs[] = {
                LOCAL_EGL_NONE, LOCAL_EGL_NONE
            };
            mNullEGLImage = sEGLLibrary.fCreateImage(EGL_DISPLAY(),
                                                     EGL_NO_CONTEXT,
                                                     LOCAL_EGL_NATIVE_BUFFER_ANDROID,
                                                     mNullGraphicBuffer->getNativeBuffer(),
                                                     attrs);
        }
        return mNullEGLImage;
    }
#endif

#ifdef MOZ_WIDGET_GONK
    virtual already_AddRefed<TextureImage>
    CreateDirectTextureImage(GraphicBuffer* aBuffer, GLenum aWrapMode) MOZ_OVERRIDE;
#endif

    virtual void MakeCurrent_EGLSurface(void* surf) {
        EGLSurface eglSurface = (EGLSurface)surf;
        if (!eglSurface)
            eglSurface = mSurface;

        if (eglSurface == mCurSurface)
            return;

        // Else, surface changed...
        mCurSurface = eglSurface;
        MakeCurrent(true);
    }

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
                                                 mCurSurface, mCurSurface,
                                                 mContext);
            
            int eglError = sEGLLibrary.fGetError();
            if (!succeeded) {
                if (eglError == LOCAL_EGL_CONTEXT_LOST) {
                    mContextLost = true;
                    NS_WARNING("EGL context has been lost.");
                } else {
                    NS_WARNING("Failed to make GL context current!");
#ifdef DEBUG
                    printf_stderr("EGL Error: 0x%04x\n", eglError);
#endif
                }
            }
        }

        return succeeded;
    }

    virtual bool IsCurrent() {
        return sEGLLibrary.fGetCurrentContext() == mContext;
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
#ifdef MOZ_ANDROID_OMTC
            mSurface = mozilla::AndroidBridge::Bridge()->ProvideEGLSurface();
            if (!mSurface) {
                return false;
            }
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
            return nullptr;
        }
    }

    bool SwapBuffers()
    {
        if (mSurface && !mPlatformContext) {
#ifdef MOZ_WIDGET_GONK
            if (mHwc)
                return !mHwc->swapBuffers((hwc_display_t)EGL_DISPLAY(),
                                          (hwc_surface_t)mSurface);
            else
#endif
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
    CreateEGLPixmapOffscreenContext(const gfxIntSize& size);

    static already_AddRefed<GLContextEGL>
    CreateEGLPBufferOffscreenContext(const gfxIntSize& size);

    virtual SharedTextureHandle CreateSharedHandle(SharedTextureShareType shareType,
                                                   void* buffer,
                                                   SharedTextureBufferType bufferType);
    virtual void UpdateSharedHandle(SharedTextureShareType shareType,
                                    SharedTextureHandle sharedHandle);
    virtual void ReleaseSharedHandle(SharedTextureShareType shareType,
                                     SharedTextureHandle sharedHandle);
    virtual bool GetSharedHandleDetails(SharedTextureShareType shareType,
                                        SharedTextureHandle sharedHandle,
                                        SharedHandleDetails& details);
    virtual bool AttachSharedHandle(SharedTextureShareType shareType,
                                    SharedTextureHandle sharedHandle);

protected:
    friend class GLContextProviderEGL;

    EGLConfig  mConfig;
    EGLSurface mSurface;
    EGLSurface mCurSurface;
    EGLContext mContext;
    void *mPlatformContext;
    nsRefPtr<gfxASurface> mThebesSurface;
    bool mBound;

    bool mIsPBuffer;
    bool mIsDoubleBuffered;
    bool mCanBindToTexture;
    bool mShareWithEGLImage;
#ifdef MOZ_WIDGET_GONK
    nsRefPtr<HwcComposer2D> mHwc;

    /* mNullEGLImage and mNullGraphicBuffer are a hack to unattach a gralloc buffer
     * from a texture, which we don't know how to do otherwise (at least in the
     * TEXTURE_EXTERNAL case --- in the TEXTURE_2D case we could also use TexImage2D).
     *
     * So mNullGraphicBuffer will be initialized once to be a 1x1 gralloc buffer,
     * and mNullEGLImage will be initialized once to be an EGLImage wrapping it.
     *
     * This happens in GetNullEGLImage().
     */
    EGLImage mNullEGLImage;
    android::sp<android::GraphicBuffer> mNullGraphicBuffer;
#endif

    // A dummy texture ID that can be used when we need a texture object whose
    // images we're going to define with EGLImageTargetTexture2D.
    GLuint mTemporaryEGLImageTexture;

    static EGLSurface CreatePBufferSurfaceTryingPowerOfTwo(EGLConfig config,
                                                           EGLenum bindToTextureFormat,
                                                           gfxIntSize& pbsize)
    {
        nsTArray<EGLint> pbattrs(16);
        EGLSurface surface = nullptr;

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
            return nullptr;
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
        mSurfaceTexture = nullptr;
    }

    nsSurfaceTexture* SurfaceTexture() { return mSurfaceTexture; }

    nsRefPtr<nsSurfaceTexture> mSurfaceTexture;
};

#endif // MOZ_WIDGET_ANDROID

class EGLTextureWrapper : public SharedTextureHandleWrapper
{
public:
    EGLTextureWrapper() :
        SharedTextureHandleWrapper(SharedHandleType::Image)
        , mEGLImage(nullptr)
        , mSyncObject(nullptr)
    {
    }

    // Args are the active GL context, and a texture in that GL
    // context for which to create an EGLImage.  After the EGLImage
    // is created, the texture is unused by EGLTextureWrapper.
    bool CreateEGLImage(GLContextEGL *ctx, GLuint texture) {
        MOZ_ASSERT(!mEGLImage && texture && sEGLLibrary.HasKHRImageBase());
        static const EGLint eglAttributes[] = {
            LOCAL_EGL_NONE
        };
        EGLContext eglContext = (EGLContext)ctx->GetEGLContext();
        mEGLImage = sEGLLibrary.fCreateImage(EGL_DISPLAY(), eglContext, LOCAL_EGL_GL_TEXTURE_2D,
                                             (EGLClientBuffer)texture, eglAttributes);
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
            mEGLImage = nullptr;
        }
    }

    const EGLImage GetEGLImage() {
        return mEGLImage;
    }

    // Insert a sync point on the given context, which should be the current active
    // context.
    bool MakeSync(GLContext *ctx) {
        MOZ_ASSERT(mSyncObject == nullptr);

        if (sEGLLibrary.IsExtensionSupported(GLLibraryEGL::KHR_fence_sync)) {
            mSyncObject = sEGLLibrary.fCreateSync(EGL_DISPLAY(), LOCAL_EGL_SYNC_FENCE, nullptr);
            // We need to flush to make sure the sync object enters the command stream;
            // we can't use EGL_SYNC_FLUSH_COMMANDS_BIT at wait time, because the wait
            // happens on a different thread/context.
            ctx->fFlush();
        }

        if (mSyncObject == EGL_NO_SYNC) {
            // we failed to create one, so just do a finish
            ctx->fFinish();
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
        mSyncObject = nullptr;

        return result == LOCAL_EGL_CONDITION_SATISFIED;
    }

private:
    EGLImage mEGLImage;
    EGLSync mSyncObject;
};

void
GLContextEGL::UpdateSharedHandle(SharedTextureShareType shareType,
                                 SharedTextureHandle sharedHandle)
{
    if (shareType != SameProcess) {
        NS_ERROR("Implementation not available for this sharing type");
        return;
    }

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(sharedHandle);

    NS_ASSERTION(wrapper->Type() == SharedHandleType::Image, "Expected EGLImage shared handle");
    NS_ASSERTION(mShareWithEGLImage, "EGLImage not supported or disabled in runtime");

    EGLTextureWrapper* wrap = reinterpret_cast<EGLTextureWrapper*>(wrapper);
    // We need to copy the current GLContext drawing buffer to the texture
    // exported by the EGLImage.  Need to save both the read FBO and the texture
    // binding, because we're going to munge them to do this.
    ScopedBindTexture autoTex(this, mTemporaryEGLImageTexture);
    fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, wrap->GetEGLImage());

    // CopyTexSubImage2D, is ~2x slower than simple FBO render to texture with
    // draw quads, but if we want that, we need to assure that our default
    // framebuffer is texture-backed.
    gfxIntSize size = OffscreenSize();
    BlitFramebufferToTexture(0, mTemporaryEGLImageTexture, size, size);

    // Make sure our copy is finished, so that we can be ready to draw
    // in different thread GLContext.  If we have KHR_fence_sync, then
    // we insert a sync object, otherwise we have to do a GuaranteeResolve.
    wrap->MakeSync(this);
}

SharedTextureHandle
GLContextEGL::CreateSharedHandle(SharedTextureShareType shareType,
                                 void* buffer,
                                 SharedTextureBufferType bufferType)
{
    // Both EGLImage and SurfaceTexture only support same-process currently, but
    // it's possible to make SurfaceTexture work across processes. We should do that.
    if (shareType != SameProcess)
        return 0;

    switch (bufferType) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedTextureBufferType::SurfaceTexture:
        if (!IsExtensionSupported(GLContext::OES_EGL_image_external)) {
            NS_WARNING("Missing GL_OES_EGL_image_external");
            return 0;
        }

        return (SharedTextureHandle) new SurfaceTextureWrapper(reinterpret_cast<nsSurfaceTexture*>(buffer));
#endif
    case SharedTextureBufferType::TextureID: {
        if (!mShareWithEGLImage)
            return 0;

        GLuint texture = (uintptr_t)buffer;
        EGLTextureWrapper* tex = new EGLTextureWrapper();
        if (!tex->CreateEGLImage(this, texture)) {
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

void GLContextEGL::ReleaseSharedHandle(SharedTextureShareType shareType,
                                       SharedTextureHandle sharedHandle)
{
    if (shareType != SameProcess) {
        NS_ERROR("Implementation not available for this sharing type");
        return;
    }

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(sharedHandle);

    switch (wrapper->Type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedHandleType::SurfaceTexture:
        delete wrapper;
        break;
#endif
    
    case SharedHandleType::Image: {
        NS_ASSERTION(mShareWithEGLImage, "EGLImage not supported or disabled in runtime");

        EGLTextureWrapper* wrap = (EGLTextureWrapper*)sharedHandle;
        delete wrap;
        break;
    }

    default:
        NS_ERROR("Unknown shared handle type");
        return;
    }
}

bool GLContextEGL::GetSharedHandleDetails(SharedTextureShareType shareType,
                                          SharedTextureHandle sharedHandle,
                                          SharedHandleDetails& details)
{
    if (shareType != SameProcess)
        return false;

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(sharedHandle);

    switch (wrapper->Type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedHandleType::SurfaceTexture: {
        SurfaceTextureWrapper* surfaceWrapper = reinterpret_cast<SurfaceTextureWrapper*>(wrapper);

        details.mTarget = LOCAL_GL_TEXTURE_EXTERNAL;
        details.mProgramType = RGBALayerExternalProgramType;
        surfaceWrapper->SurfaceTexture()->GetTransformMatrix(details.mTextureTransform);
        break;
    }
#endif

    case SharedHandleType::Image:
        details.mTarget = LOCAL_GL_TEXTURE_2D;
        details.mProgramType = RGBALayerProgramType;
        break;

    default:
        NS_ERROR("Unknown shared handle type");
        return false;
    }

    return true;
}

bool GLContextEGL::AttachSharedHandle(SharedTextureShareType shareType,
                                      SharedTextureHandle sharedHandle)
{
    if (shareType != SameProcess)
        return false;

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(sharedHandle);

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

        EGLTextureWrapper* wrap = (EGLTextureWrapper*)sharedHandle;
        wrap->WaitSync();
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
GLContextEGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
	return ResizeScreenBuffer(aNewSize);
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
        , mEGLImage(nullptr)
        , mTexture(aTexture)
        , mSurface(nullptr)
        , mConfig(nullptr)
        , mTextureState(aTextureState)
        , mBound(false)
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
        if (mGraphicBuffer != nullptr)
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

        if (mBackingSurface) {
            mUpdateSurface = mBackingSurface;
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

        if (mBackingSurface && mUpdateSurface == mBackingSurface) {
#ifdef MOZ_X11
            if (mBackingSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
                FinishX(DefaultXDisplay());
            }
#endif

            mBackingSurface->SetDeviceOffset(gfxPoint(0, 0));
            mTextureState = Valid;
            mUpdateSurface = nullptr;
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

        nsRefPtr<gfxImageSurface> uploadImage = nullptr;
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

        mUpdateSurface = nullptr;
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

        mShaderType =
          mGLContext->UploadSurfaceToTexture(aSurf,
                                              region,
                                              mTexture,
                                              mTextureState == Created,
                                              bounds.TopLeft() + aFrom,
                                              false);

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
            mEGLImage = nullptr;
        }
#endif

        if (!mSurface)
            return;

        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
        mSurface = nullptr;
    }

    virtual bool CreateBackingSurface(const gfxIntSize& aSize)
    {
        ReleaseTexImage();
        DestroyEGLSurface();
        mBackingSurface = nullptr;

#ifdef MOZ_X11
        Display* dpy = DefaultXDisplay();
        XRenderPictFormat* renderFMT =
            gfxXlibSurface::FindRenderFormat(dpy, mUpdateFormat);

        nsRefPtr<gfxXlibSurface> xsurface =
            gfxXlibSurface::Create(DefaultScreenOfDisplay(dpy),
                                   renderFMT,
                                   gfxIntSize(aSize.width, aSize.height));

        XSync(dpy, False);
        mConfig = nullptr;

        if (sEGLLibrary.HasKHRImagePixmap() &&
            mGLContext->IsExtensionSupported(GLContext::OES_EGL_image))
        {
            mEGLImage =
                sEGLLibrary.fCreateImage(EGL_DISPLAY(),
                                         EGL_NO_CONTEXT,
                                         LOCAL_EGL_NATIVE_PIXMAP,
                                         (EGLClientBuffer)xsurface->XDrawable(),
                                         nullptr);

            if (!mEGLImage) {
                printf_stderr("couldn't create EGL image: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return false;
            }
            mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
            mGLContext->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mEGLImage);
            sEGLLibrary.fDestroyImage(EGL_DISPLAY(), mEGLImage);
            mEGLImage = nullptr;
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

        return mBackingSurface != nullptr;
#endif

#ifdef MOZ_WIDGET_GONK
        if (gUseBackingSurface && aSize.width >= 64) {
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
                                                        LOCAL_EGL_NATIVE_BUFFER_ANDROID,
                                                        (EGLClientBuffer) mGraphicBuffer->getNativeBuffer(),
                                                        eglImageAttributes);
                if (!mEGLImage) {
                    mGraphicBuffer = nullptr;
                    LOG("Could not create EGL images: ERROR (0x%04x)", sEGLLibrary.fGetError());
                    return false;
                }

                return true;
            }

            mGraphicBuffer = nullptr;
            LOG("GraphicBufferAllocator::alloc failed");
            return false;
        }
#endif
        return mBackingSurface != nullptr;
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
                                             LOCAL_EGL_NATIVE_BUFFER_ANDROID,
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

static nsRefPtr<GLContext> gGlobalContext;

static const EGLint kEGLConfigAttribsOffscreenPBuffer[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_PBUFFER_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_NONE
};

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
    LOCAL_EGL_ALPHA_SIZE,      0,
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
CreateConfig(EGLConfig* aConfig, int32_t depth)
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
    int32_t depth = gfxPlatform::GetPlatform()->GetScreenDepth();
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

// When MOZ_ANDROID_OMTC is defined,
// use mozilla::AndroidBridge::Bridge()->ProvideEGLSurface() instead.
#ifndef MOZ_ANDROID_OMTC
static EGLSurface
CreateSurfaceForWindow(nsIWidget *aWidget, EGLConfig config)
{
    EGLSurface surface;

#ifdef DEBUG
    sEGLLibrary.DumpEGLConfig(config);
#endif

#if !defined(MOZ_WIDGET_ANDROID)
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
    if (!sEGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    bool doubleBuffered = true;

    bool hasNativeContext = aWidget->HasGLContext();
    EGLContext eglContext = sEGLLibrary.fGetCurrentContext();
    if (hasNativeContext && eglContext) {
        void* platformContext = eglContext;
        SurfaceCaps caps = SurfaceCaps::Any();
#ifdef MOZ_WIDGET_QT
        int depth = gfxPlatform::GetPlatform()->GetScreenDepth();
        QGLContext* context = const_cast<QGLContext*>(QGLContext::currentContext());
        if (context && context->device()) {
            depth = context->device()->depth();
        }
        const QGLFormat& format = context->format();
        doubleBuffered = format.doubleBuffer();
        platformContext = context;
        caps.bpp16 = depth == 16 ? true : false;
        caps.alpha = format.rgba();
        caps.depth = format.depth();
        caps.stencil = format.stencil();
#endif
        EGLConfig config = EGL_NO_CONFIG;
        EGLSurface surface = sEGLLibrary.fGetCurrentSurface(LOCAL_EGL_DRAW);
        nsRefPtr<GLContextEGL> glContext =
            new GLContextEGL(caps,
                             gGlobalContext, false,
                             config, surface, eglContext);

        if (!glContext->Init())
            return nullptr;

        glContext->MakeCurrent();
        glContext->SetIsDoubleBuffered(doubleBuffered);
        glContext->SetPlatformContext(platformContext);

        if (!gGlobalContext) {
            gGlobalContext = glContext;
        }

        return glContext.forget();
    }

    EGLConfig config;
    if (!CreateConfig(&config)) {
        printf_stderr("Failed to create EGL config!\n");
        return nullptr;
    }

#ifdef MOZ_ANDROID_OMTC
    mozilla::AndroidBridge::Bridge()->RegisterCompositor();
    EGLSurface surface = mozilla::AndroidBridge::Bridge()->ProvideEGLSurface();
#else
    EGLSurface surface = CreateSurfaceForWindow(aWidget, config);
#endif

    if (!surface) {
        printf_stderr("Failed to create EGLSurface!\n");
        return nullptr;
    }

    GLContextEGL* shareContext = GetGlobalContextEGL();
    SurfaceCaps caps = SurfaceCaps::Any();
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(caps,
                                      shareContext, false,
                                      config, surface);

    if (!glContext) {
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nullptr;
    }

    glContext->MakeCurrent();
    glContext->SetIsDoubleBuffered(doubleBuffered);

    return glContext.forget();
}

already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPBufferOffscreenContext(const gfxIntSize& size)
{
    EGLConfig config;
    EGLSurface surface;

    const EGLint numConfigs = 1; // We only need one.
    EGLConfig configs[numConfigs];
    EGLint foundConfigs = 0;
    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                   kEGLConfigAttribsOffscreenPBuffer,
                                   configs, numConfigs,
                                   &foundConfigs)
        || foundConfigs == 0)
    {
        NS_WARNING("No EGL Config for minimal PBuffer!");
        return nullptr;
    }

    // We absolutely don't care, so just pick the first one.
    config = configs[0];
    if (GLContext::DebugMode())
        sEGLLibrary.DumpEGLConfig(config);

    gfxIntSize pbSize(size);
    surface = GLContextEGL::CreatePBufferSurfaceTryingPowerOfTwo(config,
                                                                 LOCAL_EGL_NONE,
                                                                 pbSize);
    if (!surface) {
        NS_WARNING("Failed to create PBuffer for context!");
        return nullptr;
    }

    GLContextEGL* shareContext = GetGlobalContextEGL();
    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(dummyCaps,
                                      shareContext, true,
                                      config, surface);
    if (!glContext) {
        NS_WARNING("Failed to create GLContext from PBuffer");
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nullptr;
    }

    if (!glContext->Init()) {
        NS_WARNING("Failed to initialize GLContext!");
        // GLContextEGL::dtor will destroy |surface| for us.
        return nullptr;
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

    EGLSurface surface = nullptr;
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

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                   pixmap_config,
                                   configs, numConfigs, &numConfigs)
        || numConfigs == 0)
    {
        NS_WARNING("No EGL Config for pixmap!");
        return nullptr;
    }

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
        NS_WARNING("Failed to CreatePixmapSurface!");
        return nullptr;
    }

    if (aConfig)
        *aConfig = configs[i];

    return surface;
}
#endif

already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPixmapOffscreenContext(const gfxIntSize& size)
{
    gfxASurface *thebesSurface = nullptr;
    EGLNativePixmapType pixmap = 0;

#ifdef MOZ_X11
    nsRefPtr<gfxXlibSurface> xsurface =
        gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()),
                               gfxXlibSurface::FindRenderFormat(DefaultXDisplay(),
                                                                gfxASurface::ImageFormatRGB24),
                               size);

    // XSync required after gfxXlibSurface::Create, otherwise EGL will fail with BadDrawable error
    XSync(DefaultXDisplay(), False);
    if (xsurface->CairoStatus() != 0)
        return nullptr;

    thebesSurface = xsurface;
    pixmap = (EGLNativePixmapType)xsurface->XDrawable();
#endif

    if (!pixmap) {
        return nullptr;
    }

    EGLSurface surface = 0;
    EGLConfig config = 0;

#ifdef MOZ_X11
    surface = CreateEGLSurfaceForXSurface(thebesSurface, &config);
#endif
    if (!config) {
        return nullptr;
    }
    MOZ_ASSERT(surface);

    GLContextEGL* shareContext = GetGlobalContextEGL();
    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(dummyCaps,
                                      shareContext, true,
                                      config, surface);
    if (!glContext) {
        NS_WARNING("Failed to create GLContext from XSurface");
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nullptr;
    }

    if (!glContext->Init()) {
        NS_WARNING("Failed to initialize GLContext!");
        // GLContextEGL::dtor will destroy |surface| for us.
        return nullptr;
    }

    glContext->HoldSurface(thebesSurface);

    return glContext.forget();
}

// Under EGL, if we're under X11, then we have to create a Pixmap
// because Maemo's EGL implementation doesn't support pbuffers at all
// for some reason.  On Android, pbuffers are supported fine, though
// often without the ability to texture from them directly.
already_AddRefed<GLContext>
GLContextProviderEGL::CreateOffscreen(const gfxIntSize& size,
                                      const SurfaceCaps& caps,
                                      ContextFlags flags)
{
    if (!sEGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    gfxIntSize dummySize = gfxIntSize(16, 16);
    nsRefPtr<GLContextEGL> glContext;
#if defined(MOZ_X11)
    glContext = GLContextEGL::CreateEGLPixmapOffscreenContext(dummySize);
#else
    glContext = GLContextEGL::CreateEGLPBufferOffscreenContext(dummySize);
#endif

    if (!glContext)
        return nullptr;

    if (flags & GLContext::ContextFlagsGlobal)
        return glContext.forget();

    if (!glContext->InitOffscreen(size, caps))
        return nullptr;

    return glContext.forget();
}

SharedTextureHandle
GLContextProviderEGL::CreateSharedHandle(GLContext::SharedTextureShareType shareType,
                                         void* buffer,
                                         GLContext::SharedTextureBufferType bufferType)
{
  return 0;
}

// Don't want a global context on Android as 1) share groups across 2 threads fail on many Tegra drivers (bug 759225)
// and 2) some mobile devices have a very strict limit on global number of GL contexts (bug 754257)
// and 3) each EGL context eats 750k on B2G (bug 813783)
GLContext *
GLContextProviderEGL::GetGlobalContext(const ContextFlags)
{
    return nullptr;
}

void
GLContextProviderEGL::Shutdown()
{
    gGlobalContext = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */


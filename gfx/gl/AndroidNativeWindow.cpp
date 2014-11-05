#ifdef MOZ_WIDGET_ANDROID

#include "AndroidNativeWindow.h"
#include "prlink.h"

// #define ANDROID_NATIVE_WINDOW_DEBUG

#if defined(ANDROID_NATIVE_WINDOW_DEBUG) || defined(DEBUG)
#define ALOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AndroidNativeWindow" , ## args)
#else
#define ALOG(args...) ((void)0)
#endif

using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla;

class NativeWindowLibrary
{
public:

  NativeWindowLibrary()
    : fANativeWindow_fromSurface(nullptr)
    , fANativeWindow_release(nullptr)
    , fANativeWindow_setBuffersGeometry(nullptr)
    , fANativeWindow_lock(nullptr)
    , fANativeWindow_unlockAndPost(nullptr)
    , fANativeWindow_getFormat(nullptr)
    , fANativeWindow_getWidth(nullptr)
    , fANativeWindow_getHeight(nullptr)
  {
    PRLibrary* lib = PR_LoadLibrary("libandroid.so");

    fANativeWindow_fromSurface = (pfnANativeWindow_fromSurface)PR_FindSymbol(lib, "ANativeWindow_fromSurface");
    fANativeWindow_release = (pfnANativeWindow_release)PR_FindSymbol(lib, "ANativeWindow_release");
    fANativeWindow_setBuffersGeometry = (pfnANativeWindow_setBuffersGeometry)PR_FindSymbol(lib, "ANativeWindow_setBuffersGeometry");
    fANativeWindow_lock = (pfnANativeWindow_lock)PR_FindSymbol(lib, "ANativeWindow_lock");
    fANativeWindow_unlockAndPost = (pfnANativeWindow_unlockAndPost)PR_FindSymbol(lib, "ANativeWindow_unlockAndPost");
    fANativeWindow_getFormat = (pfnANativeWindow_getFormat)PR_FindSymbol(lib, "ANativeWindow_getFormat");
    fANativeWindow_getWidth = (pfnANativeWindow_getWidth)PR_FindSymbol(lib, "ANativeWindow_getWidth");
    fANativeWindow_getHeight = (pfnANativeWindow_getHeight)PR_FindSymbol(lib, "ANativeWindow_getHeight");
  }

  void* ANativeWindow_fromSurface(JNIEnv* aEnv, jobject aSurface) {
    ALOG("%s: env=%p, surface=%p\n", __PRETTY_FUNCTION__, aEnv, aSurface);
    if (!Initialized()) {
      return nullptr;
    }

    return fANativeWindow_fromSurface(aEnv, aSurface);
  }

  void ANativeWindow_release(void* aWindow) {
    ALOG("%s: window=%p\n", __PRETTY_FUNCTION__, aWindow);
    if (!Initialized()) {
      return;
    }

    fANativeWindow_release(aWindow);
  }

  bool ANativeWindow_setBuffersGeometry(void* aWindow, int32_t aWidth, int32_t aHeight, int32_t aFormat) {
    ALOG("%s: window=%p, width=%d, height=%d, format=%d\n", __PRETTY_FUNCTION__, aWindow, aWidth, aHeight, aFormat);
    if (!Initialized()) {
      return nullptr;
    }

    return fANativeWindow_setBuffersGeometry(aWindow, aWidth, aHeight, (int32_t)aFormat) == 0;
  }

  bool ANativeWindow_lock(void* aWindow, void* out_buffer, void*in_out_dirtyBounds) {
    ALOG("%s: window=%p, out_buffer=%p, in_out_dirtyBounds=%p\n", __PRETTY_FUNCTION__,
         aWindow, out_buffer, in_out_dirtyBounds);
    if (!Initialized()) {
      return false;
    }

    return fANativeWindow_lock(aWindow, out_buffer, in_out_dirtyBounds) == 0;
  }

  bool ANativeWindow_unlockAndPost(void* aWindow) {
    ALOG("%s: window=%p\n", __PRETTY_FUNCTION__, aWindow);
    if (!Initialized()) {
      return false;
    }

    return fANativeWindow_unlockAndPost(aWindow) == 0;
  }

  AndroidWindowFormat ANativeWindow_getFormat(void* aWindow) {
    ALOG("%s: window=%p\n", __PRETTY_FUNCTION__, aWindow);
    if (!Initialized()) {
      return AndroidWindowFormat::Unknown;
    }

    return (AndroidWindowFormat)fANativeWindow_getFormat(aWindow);
  }

  int32_t ANativeWindow_getWidth(void* aWindow) {
    ALOG("%s: window=%p\n", __PRETTY_FUNCTION__, aWindow);
    if (!Initialized()) {
      return -1;
    }

    return fANativeWindow_getWidth(aWindow);
  }

  int32_t ANativeWindow_getHeight(void* aWindow) {
    ALOG("%s: window=%p\n", __PRETTY_FUNCTION__, aWindow);
    if (!Initialized()) {
      return -1;
    }

    return fANativeWindow_getHeight(aWindow);
  }

  bool Initialized() {
    return fANativeWindow_fromSurface && fANativeWindow_release && fANativeWindow_setBuffersGeometry
      && fANativeWindow_lock && fANativeWindow_unlockAndPost && fANativeWindow_getFormat && fANativeWindow_getWidth
      && fANativeWindow_getHeight;
  }

private:

  typedef void* (*pfnANativeWindow_fromSurface)(JNIEnv* env, jobject surface);
  pfnANativeWindow_fromSurface fANativeWindow_fromSurface;

  typedef void (*pfnANativeWindow_release)(void* window);
  pfnANativeWindow_release fANativeWindow_release;

  typedef int32_t (*pfnANativeWindow_setBuffersGeometry)(void* window, int32_t width, int32_t height, int32_t format);
  pfnANativeWindow_setBuffersGeometry fANativeWindow_setBuffersGeometry;

  typedef int32_t (*pfnANativeWindow_lock)(void *window, void *out_buffer, void *in_out_dirtyBounds);
  pfnANativeWindow_lock fANativeWindow_lock;

  typedef int32_t (*pfnANativeWindow_unlockAndPost)(void *window);
  pfnANativeWindow_unlockAndPost fANativeWindow_unlockAndPost;

  typedef AndroidWindowFormat (*pfnANativeWindow_getFormat)(void* window);
  pfnANativeWindow_getFormat fANativeWindow_getFormat;

  typedef int32_t (*pfnANativeWindow_getWidth)(void* window);
  pfnANativeWindow_getWidth fANativeWindow_getWidth;

  typedef int32_t (*pfnANativeWindow_getHeight)(void* window);
  pfnANativeWindow_getHeight fANativeWindow_getHeight;
};

static NativeWindowLibrary* sLibrary = nullptr;

static bool
EnsureInit()
{
  static bool initialized = false;
  if (!initialized) {
    if (!sLibrary) {
      sLibrary = new NativeWindowLibrary();
    }
    initialized = sLibrary->Initialized();
  }

  return initialized;
}


namespace mozilla {

/* static */ AndroidNativeWindow*
AndroidNativeWindow::CreateFromSurface(JNIEnv* aEnv, jobject aSurface)
{
  if (!EnsureInit()) {
    ALOG("Not initialized");
    return nullptr;
  }

  void* window = sLibrary->ANativeWindow_fromSurface(aEnv, aSurface);
  if (!window) {
    ALOG("Failed to create window from surface");
    return nullptr;
  }

  return new AndroidNativeWindow(window);
}

AndroidNativeWindow::~AndroidNativeWindow()
{
  if (EnsureInit() && mWindow) {
    sLibrary->ANativeWindow_release(mWindow);
    mWindow = nullptr;
  }
}

IntSize
AndroidNativeWindow::Size()
{
  MOZ_ASSERT(mWindow);
  if (!EnsureInit()) {
    return IntSize(0, 0);
  }

  return IntSize(sLibrary->ANativeWindow_getWidth(mWindow), sLibrary->ANativeWindow_getHeight(mWindow));
}

AndroidWindowFormat
AndroidNativeWindow::Format()
{
  MOZ_ASSERT(mWindow);
  if (!EnsureInit()) {
    return AndroidWindowFormat::Unknown;
  }

  return sLibrary->ANativeWindow_getFormat(mWindow);
}

bool
AndroidNativeWindow::SetBuffersGeometry(int32_t aWidth, int32_t aHeight, AndroidWindowFormat aFormat)
{
  MOZ_ASSERT(mWindow);
  if (!EnsureInit())
    return false;

  return sLibrary->ANativeWindow_setBuffersGeometry(mWindow, aWidth, aHeight, (int32_t)aFormat);
}

bool
AndroidNativeWindow::Lock(void** out_bits,int32_t* out_width, int32_t* out_height,
                          int32_t* out_stride, AndroidWindowFormat* out_format)
{
  /* Copied from native_window.h in Android NDK (platform-9) */
  typedef struct ANativeWindow_Buffer {
      // The number of pixels that are show horizontally.
      int32_t width;

      // The number of pixels that are shown vertically.
      int32_t height;

      // The number of *pixels* that a line in the buffer takes in
      // memory.  This may be >= width.
      int32_t stride;

      // The format of the buffer.  One of WINDOW_FORMAT_*
      int32_t format;

      // The actual bits.
      void* bits;

      // Do not touch.
      uint32_t reserved[6];
  } ANativeWindow_Buffer; 


  ANativeWindow_Buffer buffer;

  if (!sLibrary->ANativeWindow_lock(mWindow, &buffer, nullptr)) {
    ALOG("Failed to lock");
    return false;
  }

  *out_bits = buffer.bits;
  *out_width = buffer.width;
  *out_height = buffer.height;
  *out_stride = buffer.stride;
  *out_format = (AndroidWindowFormat)buffer.format;
  return true;
}

bool
AndroidNativeWindow::UnlockAndPost()
{
  if (!EnsureInit()) {
    ALOG("Not initialized");
    return false;
  }

  return sLibrary->ANativeWindow_unlockAndPost(mWindow);
}

}

#endif // MOZ_WIDGET_ANDROID
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLLIBRARYEGL_H_
#define GLLIBRARYEGL_H_

#if defined(MOZ_X11)
#  include "mozilla/X11Util.h"
#endif

#include "GLTypes.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "nsISupports.h"
#include "prlink.h"

#include <bitset>
#include <memory>
#include <unordered_map>

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/ProfilerLabels.h"
#endif

#if defined(MOZ_X11)
#  define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)mozilla::DefaultXDisplay())
#else
#  define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
#endif

struct ID3D11Device;

extern "C" {
struct AHardwareBuffer;
}

namespace angle {
class Platform;
}

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}

namespace gl {

class SymbolLoader;

PRLibrary* LoadApitraceLibrary();

void BeforeEGLCall(const char* funcName);
void AfterEGLCall(const char* funcName);

class EglDisplay;
/**
 * Known GL extensions that can be queried by
 * IsExtensionSupported.  The results of this are cached, and as
 * such it's safe to use this even in performance critical code.
 * If you add to this array, remember to add to the string names
 * in GLLibraryEGL.cpp.
 */
enum class EGLLibExtension {
  ANDROID_get_native_client_buffer,
  ANGLE_device_creation,
  ANGLE_device_creation_d3d11,
  ANGLE_platform_angle,
  ANGLE_platform_angle_d3d,
  EXT_device_query,
  Max
};

/**
 * Known GL extensions that can be queried by
 * IsExtensionSupported.  The results of this are cached, and as
 * such it's safe to use this even in performance critical code.
 * If you add to this array, remember to add to the string names
 * in GLLibraryEGL.cpp.
 */
enum class EGLExtension {
  KHR_image_base,
  KHR_image_pixmap,
  KHR_gl_texture_2D_image,
  ANGLE_surface_d3d_texture_2d_share_handle,
  EXT_create_context_robustness,
  KHR_image,
  KHR_fence_sync,
  KHR_wait_sync,
  ANDROID_native_fence_sync,
  EGL_ANDROID_image_crop,
  ANGLE_d3d_share_handle_client_buffer,
  KHR_create_context,
  KHR_stream,
  KHR_stream_consumer_gltexture,
  NV_stream_consumer_gltexture_yuv,
  ANGLE_stream_producer_d3d_texture,
  KHR_surfaceless_context,
  KHR_create_context_no_error,
  MOZ_create_context_provoking_vertex_dont_care,
  EXT_swap_buffers_with_damage,
  KHR_swap_buffers_with_damage,
  EXT_buffer_age,
  KHR_partial_update,
  NV_robustness_video_memory_purge,
  Max
};

// -

class GLLibraryEGL final {
  friend class EglDisplay;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GLLibraryEGL)

 private:
  PRLibrary* mEGLLibrary = nullptr;
  PRLibrary* mGLLibrary = nullptr;
  bool mIsANGLE = false;
  std::bitset<UnderlyingValue(EGLLibExtension::Max)> mAvailableExtensions;
  std::weak_ptr<EglDisplay> mDefaultDisplay;
  std::unordered_map<EGLDisplay, std::weak_ptr<EglDisplay>> mActiveDisplays;

 public:
  static RefPtr<GLLibraryEGL> Create(nsACString* const out_failureId);

 private:
  ~GLLibraryEGL() = default;

  bool Init(nsACString* const out_failureId);
  void InitLibExtensions();

 public:
  Maybe<SymbolLoader> GetSymbolLoader() const;

  std::shared_ptr<EglDisplay> CreateDisplay(bool forceAccel,
                                            nsACString* const out_failureId);
  std::shared_ptr<EglDisplay> CreateDisplay(ID3D11Device*);
  std::shared_ptr<EglDisplay> DefaultDisplay(nsACString* const out_failureId);

  bool IsExtensionSupported(EGLLibExtension aKnownExtension) const {
    return mAvailableExtensions[UnderlyingValue(aKnownExtension)];
  }

  void MarkExtensionUnsupported(EGLLibExtension aKnownExtension) {
    mAvailableExtensions[UnderlyingValue(aKnownExtension)] = false;
  }

  bool IsANGLE() const { return mIsANGLE; }

  // -
  // PFN wrappers

#ifdef MOZ_WIDGET_ANDROID
#  define PROFILE_CALL AUTO_PROFILER_LABEL(__func__, GRAPHICS);
#else
#  define PROFILE_CALL
#endif

#ifndef MOZ_FUNCTION_NAME
#  ifdef __GNUC__
#    define MOZ_FUNCTION_NAME __PRETTY_FUNCTION__
#  elif defined(_MSC_VER)
#    define MOZ_FUNCTION_NAME __FUNCTION__
#  else
#    define MOZ_FUNCTION_NAME \
      __func__  // defined in C99, supported in various C++ compilers. Just raw
                // function name.
#  endif
#endif

#ifdef DEBUG
#  define BEFORE_CALL BeforeEGLCall(MOZ_FUNCTION_NAME);
#  define AFTER_CALL AfterEGLCall(MOZ_FUNCTION_NAME);
#else
#  define BEFORE_CALL
#  define AFTER_CALL
#endif

#define WRAP(X)                \
  PROFILE_CALL                 \
  BEFORE_CALL                  \
  const auto ret = mSymbols.X; \
  AFTER_CALL                   \
  return ret

 public:
  EGLDisplay fGetDisplay(void* display_id) const {
    WRAP(fGetDisplay(display_id));
  }

  EGLDisplay fGetPlatformDisplay(EGLenum platform, void* native_display,
                                 const EGLAttrib* attrib_list) const {
    WRAP(fGetPlatformDisplay(platform, native_display, attrib_list));
  }

  EGLSurface fGetCurrentSurface(EGLint id) const {
    WRAP(fGetCurrentSurface(id));
  }

  EGLContext fGetCurrentContext() const { WRAP(fGetCurrentContext()); }

  EGLBoolean fBindAPI(EGLenum api) const { WRAP(fBindAPI(api)); }

  EGLint fGetError() const { WRAP(fGetError()); }

  EGLBoolean fWaitNative(EGLint engine) const { WRAP(fWaitNative(engine)); }

  EGLCastToRelevantPtr fGetProcAddress(const char* procname) const {
    WRAP(fGetProcAddress(procname));
  }

  // ANGLE_device_creation
  EGLDeviceEXT fCreateDeviceANGLE(EGLint device_type, void* native_device,
                                  const EGLAttrib* attrib_list) const {
    WRAP(fCreateDeviceANGLE(device_type, native_device, attrib_list));
  }

  EGLBoolean fReleaseDeviceANGLE(EGLDeviceEXT device) {
    WRAP(fReleaseDeviceANGLE(device));
  }

  // ANDROID_get_native_client_buffer
  EGLClientBuffer fGetNativeClientBufferANDROID(
      const struct AHardwareBuffer* buffer) {
    WRAP(fGetNativeClientBufferANDROID(buffer));
  }

 private:
  EGLBoolean fTerminate(EGLDisplay display) const { WRAP(fTerminate(display)); }

  EGLBoolean fMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                          EGLContext ctx) const {
    WRAP(fMakeCurrent(dpy, draw, read, ctx));
  }

  EGLBoolean fDestroyContext(EGLDisplay dpy, EGLContext ctx) const {
    WRAP(fDestroyContext(dpy, ctx));
  }

  EGLContext fCreateContext(EGLDisplay dpy, EGLConfig config,
                            EGLContext share_context,
                            const EGLint* attrib_list) const {
    WRAP(fCreateContext(dpy, config, share_context, attrib_list));
  }

  EGLBoolean fDestroySurface(EGLDisplay dpy, EGLSurface surface) const {
    WRAP(fDestroySurface(dpy, surface));
  }

 public:
  EGLSurface fCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                  EGLNativeWindowType win,
                                  const EGLint* attrib_list) const {
    WRAP(fCreateWindowSurface(dpy, config, win, attrib_list));
  }

 private:
  EGLSurface fCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
                                   const EGLint* attrib_list) const {
    WRAP(fCreatePbufferSurface(dpy, config, attrib_list));
  }

  EGLSurface fCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype,
                                            EGLClientBuffer buffer,
                                            EGLConfig config,
                                            const EGLint* attrib_list) const {
    WRAP(fCreatePbufferFromClientBuffer(dpy, buftype, buffer, config,
                                        attrib_list));
  }

  EGLSurface fCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
                                  EGLNativePixmapType pixmap,
                                  const EGLint* attrib_list) const {
    WRAP(fCreatePixmapSurface(dpy, config, pixmap, attrib_list));
  }

  EGLBoolean fInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) const {
    WRAP(fInitialize(dpy, major, minor));
  }

  EGLBoolean fChooseConfig(EGLDisplay dpy, const EGLint* attrib_list,
                           EGLConfig* configs, EGLint config_size,
                           EGLint* num_config) const {
    WRAP(fChooseConfig(dpy, attrib_list, configs, config_size, num_config));
  }

  EGLBoolean fGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                              EGLint attribute, EGLint* value) const {
    WRAP(fGetConfigAttrib(dpy, config, attribute, value));
  }

  EGLBoolean fGetConfigs(EGLDisplay dpy, EGLConfig* configs, EGLint config_size,
                         EGLint* num_config) const {
    WRAP(fGetConfigs(dpy, configs, config_size, num_config));
  }

  EGLBoolean fSwapBuffers(EGLDisplay dpy, EGLSurface surface) const {
    WRAP(fSwapBuffers(dpy, surface));
  }

  EGLBoolean fCopyBuffers(EGLDisplay dpy, EGLSurface surface,
                          EGLNativePixmapType target) const {
    WRAP(fCopyBuffers(dpy, surface, target));
  }

 public:
  const GLubyte* fQueryString(EGLDisplay dpy, EGLint name) const {
    WRAP(fQueryString(dpy, name));
  }

 private:
  EGLBoolean fQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute,
                           EGLint* value) const {
    WRAP(fQueryContext(dpy, ctx, attribute, value));
  }

  EGLBoolean fBindTexImage(EGLDisplay dpy, EGLSurface surface,
                           EGLint buffer) const {
    WRAP(fBindTexImage(dpy, surface, buffer));
  }

  EGLBoolean fReleaseTexImage(EGLDisplay dpy, EGLSurface surface,
                              EGLint buffer) const {
    WRAP(fReleaseTexImage(dpy, surface, buffer));
  }

  EGLBoolean fSwapInterval(EGLDisplay dpy, EGLint interval) const {
    WRAP(fSwapInterval(dpy, interval));
  }

  EGLImage fCreateImage(EGLDisplay dpy, EGLContext ctx, EGLenum target,
                        EGLClientBuffer buffer,
                        const EGLint* attrib_list) const {
    WRAP(fCreateImageKHR(dpy, ctx, target, buffer, attrib_list));
  }

  EGLBoolean fDestroyImage(EGLDisplay dpy, EGLImage image) const {
    WRAP(fDestroyImageKHR(dpy, image));
  }

  EGLBoolean fQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute,
                           EGLint* value) const {
    WRAP(fQuerySurface(dpy, surface, attribute, value));
  }

  EGLBoolean fQuerySurfacePointerANGLE(EGLDisplay dpy, EGLSurface surface,
                                       EGLint attribute, void** value) const {
    WRAP(fQuerySurfacePointerANGLE(dpy, surface, attribute, value));
  }

  EGLSync fCreateSync(EGLDisplay dpy, EGLenum type,
                      const EGLint* attrib_list) const {
    WRAP(fCreateSyncKHR(dpy, type, attrib_list));
  }

  EGLBoolean fDestroySync(EGLDisplay dpy, EGLSync sync) const {
    WRAP(fDestroySyncKHR(dpy, sync));
  }

  EGLint fClientWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags,
                         EGLTime timeout) const {
    WRAP(fClientWaitSyncKHR(dpy, sync, flags, timeout));
  }

  EGLBoolean fGetSyncAttrib(EGLDisplay dpy, EGLSync sync, EGLint attribute,
                            EGLint* value) const {
    WRAP(fGetSyncAttribKHR(dpy, sync, attribute, value));
  }

  EGLint fWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags) const {
    WRAP(fWaitSyncKHR(dpy, sync, flags));
  }

  EGLint fDupNativeFenceFDANDROID(EGLDisplay dpy, EGLSync sync) const {
    WRAP(fDupNativeFenceFDANDROID(dpy, sync));
  }

  // KHR_stream
  EGLStreamKHR fCreateStreamKHR(EGLDisplay dpy,
                                const EGLint* attrib_list) const {
    WRAP(fCreateStreamKHR(dpy, attrib_list));
  }

  EGLBoolean fDestroyStreamKHR(EGLDisplay dpy, EGLStreamKHR stream) const {
    WRAP(fDestroyStreamKHR(dpy, stream));
  }

  EGLBoolean fQueryStreamKHR(EGLDisplay dpy, EGLStreamKHR stream,
                             EGLenum attribute, EGLint* value) const {
    WRAP(fQueryStreamKHR(dpy, stream, attribute, value));
  }

  // KHR_stream_consumer_gltexture
  EGLBoolean fStreamConsumerGLTextureExternalKHR(EGLDisplay dpy,
                                                 EGLStreamKHR stream) const {
    WRAP(fStreamConsumerGLTextureExternalKHR(dpy, stream));
  }

  EGLBoolean fStreamConsumerAcquireKHR(EGLDisplay dpy,
                                       EGLStreamKHR stream) const {
    WRAP(fStreamConsumerAcquireKHR(dpy, stream));
  }

  EGLBoolean fStreamConsumerReleaseKHR(EGLDisplay dpy,
                                       EGLStreamKHR stream) const {
    WRAP(fStreamConsumerReleaseKHR(dpy, stream));
  }

  // EXT_device_query
  EGLBoolean fQueryDisplayAttribEXT(EGLDisplay dpy, EGLint attribute,
                                    EGLAttrib* value) const {
    WRAP(fQueryDisplayAttribEXT(dpy, attribute, value));
  }

 public:
  EGLBoolean fQueryDeviceAttribEXT(EGLDeviceEXT device, EGLint attribute,
                                   EGLAttrib* value) const {
    WRAP(fQueryDeviceAttribEXT(device, attribute, value));
  }

 private:
  // NV_stream_consumer_gltexture_yuv
  EGLBoolean fStreamConsumerGLTextureExternalAttribsNV(
      EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib* attrib_list) const {
    WRAP(fStreamConsumerGLTextureExternalAttribsNV(dpy, stream, attrib_list));
  }

  // ANGLE_stream_producer_d3d_texture
  EGLBoolean fCreateStreamProducerD3DTextureANGLE(
      EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib* attrib_list) const {
    WRAP(fCreateStreamProducerD3DTextureANGLE(dpy, stream, attrib_list));
  }

  EGLBoolean fStreamPostD3DTextureANGLE(EGLDisplay dpy, EGLStreamKHR stream,
                                        void* texture,
                                        const EGLAttrib* attrib_list) const {
    WRAP(fStreamPostD3DTextureANGLE(dpy, stream, texture, attrib_list));
  }

  // EGL_EXT_swap_buffers_with_damage / EGL_KHR_swap_buffers_with_damage
  EGLBoolean fSwapBuffersWithDamage(EGLDisplay dpy, EGLSurface surface,
                                    const EGLint* rects, EGLint n_rects) {
    WRAP(fSwapBuffersWithDamage(dpy, surface, rects, n_rects));
  }

  // EGL_KHR_partial_update
  EGLBoolean fSetDamageRegion(EGLDisplay dpy, EGLSurface surface,
                              const EGLint* rects, EGLint n_rects) {
    WRAP(fSetDamageRegion(dpy, surface, rects, n_rects));
  }

#undef WRAP
#undef PROFILE_CALL
#undef BEFORE_CALL
#undef AFTER_CALL
#undef MOZ_FUNCTION_NAME

  ////

 private:
  struct {
    EGLCastToRelevantPtr(GLAPIENTRY* fGetProcAddress)(const char* procname);
    EGLDisplay(GLAPIENTRY* fGetDisplay)(void* display_id);
    EGLDisplay(GLAPIENTRY* fGetPlatformDisplay)(EGLenum platform,
                                                void* native_display,
                                                const EGLAttrib* attrib_list);
    EGLBoolean(GLAPIENTRY* fTerminate)(EGLDisplay dpy);
    EGLSurface(GLAPIENTRY* fGetCurrentSurface)(EGLint);
    EGLContext(GLAPIENTRY* fGetCurrentContext)(void);
    EGLBoolean(GLAPIENTRY* fMakeCurrent)(EGLDisplay dpy, EGLSurface draw,
                                         EGLSurface read, EGLContext ctx);
    EGLBoolean(GLAPIENTRY* fDestroyContext)(EGLDisplay dpy, EGLContext ctx);
    EGLContext(GLAPIENTRY* fCreateContext)(EGLDisplay dpy, EGLConfig config,
                                           EGLContext share_context,
                                           const EGLint* attrib_list);
    EGLBoolean(GLAPIENTRY* fDestroySurface)(EGLDisplay dpy, EGLSurface surface);
    EGLSurface(GLAPIENTRY* fCreateWindowSurface)(EGLDisplay dpy,
                                                 EGLConfig config,
                                                 EGLNativeWindowType win,
                                                 const EGLint* attrib_list);
    EGLSurface(GLAPIENTRY* fCreatePbufferSurface)(EGLDisplay dpy,
                                                  EGLConfig config,
                                                  const EGLint* attrib_list);
    EGLSurface(GLAPIENTRY* fCreatePbufferFromClientBuffer)(
        EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
        EGLConfig config, const EGLint* attrib_list);
    EGLSurface(GLAPIENTRY* fCreatePixmapSurface)(EGLDisplay dpy,
                                                 EGLConfig config,
                                                 EGLNativePixmapType pixmap,
                                                 const EGLint* attrib_list);
    EGLBoolean(GLAPIENTRY* fBindAPI)(EGLenum api);
    EGLBoolean(GLAPIENTRY* fInitialize)(EGLDisplay dpy, EGLint* major,
                                        EGLint* minor);
    EGLBoolean(GLAPIENTRY* fChooseConfig)(EGLDisplay dpy,
                                          const EGLint* attrib_list,
                                          EGLConfig* configs,
                                          EGLint config_size,
                                          EGLint* num_config);
    EGLint(GLAPIENTRY* fGetError)(void);
    EGLBoolean(GLAPIENTRY* fGetConfigAttrib)(EGLDisplay dpy, EGLConfig config,
                                             EGLint attribute, EGLint* value);
    EGLBoolean(GLAPIENTRY* fGetConfigs)(EGLDisplay dpy, EGLConfig* configs,
                                        EGLint config_size, EGLint* num_config);
    EGLBoolean(GLAPIENTRY* fWaitNative)(EGLint engine);
    EGLBoolean(GLAPIENTRY* fSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
    EGLBoolean(GLAPIENTRY* fCopyBuffers)(EGLDisplay dpy, EGLSurface surface,
                                         EGLNativePixmapType target);
    const GLubyte*(GLAPIENTRY* fQueryString)(EGLDisplay, EGLint name);
    EGLBoolean(GLAPIENTRY* fQueryContext)(EGLDisplay dpy, EGLContext ctx,
                                          EGLint attribute, EGLint* value);
    EGLBoolean(GLAPIENTRY* fBindTexImage)(EGLDisplay, EGLSurface surface,
                                          EGLint buffer);
    EGLBoolean(GLAPIENTRY* fReleaseTexImage)(EGLDisplay, EGLSurface surface,
                                             EGLint buffer);
    EGLBoolean(GLAPIENTRY* fSwapInterval)(EGLDisplay dpy, EGLint interval);
    EGLImage(GLAPIENTRY* fCreateImageKHR)(EGLDisplay dpy, EGLContext ctx,
                                          EGLenum target,
                                          EGLClientBuffer buffer,
                                          const EGLint* attrib_list);
    EGLBoolean(GLAPIENTRY* fDestroyImageKHR)(EGLDisplay dpy, EGLImage image);
    EGLBoolean(GLAPIENTRY* fQuerySurface)(EGLDisplay dpy, EGLSurface surface,
                                          EGLint attribute, EGLint* value);
    EGLBoolean(GLAPIENTRY* fQuerySurfacePointerANGLE)(EGLDisplay dpy,
                                                      EGLSurface surface,
                                                      EGLint attribute,
                                                      void** value);
    EGLSync(GLAPIENTRY* fCreateSyncKHR)(EGLDisplay dpy, EGLenum type,
                                        const EGLint* attrib_list);
    EGLBoolean(GLAPIENTRY* fDestroySyncKHR)(EGLDisplay dpy, EGLSync sync);
    EGLint(GLAPIENTRY* fClientWaitSyncKHR)(EGLDisplay dpy, EGLSync sync,
                                           EGLint flags, EGLTime timeout);
    EGLBoolean(GLAPIENTRY* fGetSyncAttribKHR)(EGLDisplay dpy, EGLSync sync,
                                              EGLint attribute, EGLint* value);
    EGLint(GLAPIENTRY* fWaitSyncKHR)(EGLDisplay dpy, EGLSync sync,
                                     EGLint flags);
    EGLint(GLAPIENTRY* fDupNativeFenceFDANDROID)(EGLDisplay dpy, EGLSync sync);
    // KHR_stream
    EGLStreamKHR(GLAPIENTRY* fCreateStreamKHR)(EGLDisplay dpy,
                                               const EGLint* attrib_list);
    EGLBoolean(GLAPIENTRY* fDestroyStreamKHR)(EGLDisplay dpy,
                                              EGLStreamKHR stream);
    EGLBoolean(GLAPIENTRY* fQueryStreamKHR)(EGLDisplay dpy, EGLStreamKHR stream,
                                            EGLenum attribute, EGLint* value);
    // KHR_stream_consumer_gltexture
    EGLBoolean(GLAPIENTRY* fStreamConsumerGLTextureExternalKHR)(
        EGLDisplay dpy, EGLStreamKHR stream);
    EGLBoolean(GLAPIENTRY* fStreamConsumerAcquireKHR)(EGLDisplay dpy,
                                                      EGLStreamKHR stream);
    EGLBoolean(GLAPIENTRY* fStreamConsumerReleaseKHR)(EGLDisplay dpy,
                                                      EGLStreamKHR stream);
    // EXT_device_query
    EGLBoolean(GLAPIENTRY* fQueryDisplayAttribEXT)(EGLDisplay dpy,
                                                   EGLint attribute,
                                                   EGLAttrib* value);
    EGLBoolean(GLAPIENTRY* fQueryDeviceAttribEXT)(EGLDeviceEXT device,
                                                  EGLint attribute,
                                                  EGLAttrib* value);
    // NV_stream_consumer_gltexture_yuv
    EGLBoolean(GLAPIENTRY* fStreamConsumerGLTextureExternalAttribsNV)(
        EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib* attrib_list);
    // ANGLE_stream_producer_d3d_texture
    EGLBoolean(GLAPIENTRY* fCreateStreamProducerD3DTextureANGLE)(
        EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib* attrib_list);
    EGLBoolean(GLAPIENTRY* fStreamPostD3DTextureANGLE)(
        EGLDisplay dpy, EGLStreamKHR stream, void* texture,
        const EGLAttrib* attrib_list);
    // ANGLE_device_creation
    EGLDeviceEXT(GLAPIENTRY* fCreateDeviceANGLE)(EGLint device_type,
                                                 void* native_device,
                                                 const EGLAttrib* attrib_list);
    EGLBoolean(GLAPIENTRY* fReleaseDeviceANGLE)(EGLDeviceEXT device);
    // EGL_EXT_swap_buffers_with_damage / EGL_KHR_swap_buffers_with_damage
    EGLBoolean(GLAPIENTRY* fSwapBuffersWithDamage)(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   const EGLint* rects,
                                                   EGLint n_rects);
    // EGL_KHR_partial_update
    EGLBoolean(GLAPIENTRY* fSetDamageRegion)(EGLDisplay dpy, EGLSurface surface,
                                             const EGLint* rects,
                                             EGLint n_rects);
    EGLClientBuffer(GLAPIENTRY* fGetNativeClientBufferANDROID)(
        const struct AHardwareBuffer* buffer);
  } mSymbols = {};
};

class EglDisplay final {
 public:
  const RefPtr<GLLibraryEGL> mLib;
  const EGLDisplay mDisplay;
  const bool mIsWARP;

 private:
  std::bitset<UnderlyingValue(EGLExtension::Max)> mAvailableExtensions;

  struct PrivateUseOnly final {};

 public:
  static std::shared_ptr<EglDisplay> Create(GLLibraryEGL&, EGLDisplay,
                                            bool isWarp);

  // Only `public` for make_shared.
  EglDisplay(const PrivateUseOnly&, GLLibraryEGL&, EGLDisplay, bool isWarp);

 public:
  ~EglDisplay();

  bool IsExtensionSupported(EGLExtension aKnownExtension) const {
    return mAvailableExtensions[UnderlyingValue(aKnownExtension)];
  }

  void MarkExtensionUnsupported(EGLExtension aKnownExtension) {
    mAvailableExtensions[UnderlyingValue(aKnownExtension)] = false;
  }

  void DumpEGLConfig(EGLConfig) const;
  void DumpEGLConfigs() const;

  void Shutdown();

  // -

  bool HasKHRImageBase() const {
    return IsExtensionSupported(EGLExtension::KHR_image) ||
           IsExtensionSupported(EGLExtension::KHR_image_base);
  }

  bool HasKHRImagePixmap() const {
    return IsExtensionSupported(EGLExtension::KHR_image) ||
           IsExtensionSupported(EGLExtension::KHR_image_pixmap);
  }

  // -

  EGLBoolean fTerminate() { return mLib->fTerminate(mDisplay); }

  EGLBoolean fMakeCurrent(EGLSurface draw, EGLSurface read,
                          EGLContext ctx) const {
    return mLib->fMakeCurrent(mDisplay, draw, read, ctx);
  }

  EGLBoolean fDestroyContext(EGLContext ctx) const {
    return mLib->fDestroyContext(mDisplay, ctx);
  }

  EGLContext fCreateContext(EGLConfig config, EGLContext share_context,
                            const EGLint* attrib_list) const {
    return mLib->fCreateContext(mDisplay, config, share_context, attrib_list);
  }

  EGLBoolean fDestroySurface(EGLSurface surface) const {
    return mLib->fDestroySurface(mDisplay, surface);
  }

  EGLSurface fCreateWindowSurface(EGLConfig config, EGLNativeWindowType win,
                                  const EGLint* attrib_list) const {
    return mLib->fCreateWindowSurface(mDisplay, config, win, attrib_list);
  }

  EGLSurface fCreatePbufferSurface(EGLConfig config,
                                   const EGLint* attrib_list) const {
    return mLib->fCreatePbufferSurface(mDisplay, config, attrib_list);
  }

  EGLSurface fCreatePbufferFromClientBuffer(EGLenum buftype,
                                            EGLClientBuffer buffer,
                                            EGLConfig config,
                                            const EGLint* attrib_list) const {
    return mLib->fCreatePbufferFromClientBuffer(mDisplay, buftype, buffer,
                                                config, attrib_list);
  }

  EGLBoolean fChooseConfig(const EGLint* attrib_list, EGLConfig* configs,
                           EGLint config_size, EGLint* num_config) const {
    return mLib->fChooseConfig(mDisplay, attrib_list, configs, config_size,
                               num_config);
  }

  EGLBoolean fGetConfigAttrib(EGLConfig config, EGLint attribute,
                              EGLint* value) const {
    return mLib->fGetConfigAttrib(mDisplay, config, attribute, value);
  }

  EGLBoolean fGetConfigs(EGLConfig* configs, EGLint config_size,
                         EGLint* num_config) const {
    return mLib->fGetConfigs(mDisplay, configs, config_size, num_config);
  }

  EGLBoolean fSwapBuffers(EGLSurface surface) const {
    return mLib->fSwapBuffers(mDisplay, surface);
  }

  EGLBoolean fBindTexImage(EGLSurface surface, EGLint buffer) const {
    return mLib->fBindTexImage(mDisplay, surface, buffer);
  }

  EGLBoolean fReleaseTexImage(EGLSurface surface, EGLint buffer) const {
    return mLib->fReleaseTexImage(mDisplay, surface, buffer);
  }

  EGLBoolean fSwapInterval(EGLint interval) const {
    return mLib->fSwapInterval(mDisplay, interval);
  }

  EGLImage fCreateImage(EGLContext ctx, EGLenum target, EGLClientBuffer buffer,
                        const EGLint* attribList) const {
    MOZ_ASSERT(HasKHRImageBase());
    return mLib->fCreateImage(mDisplay, ctx, target, buffer, attribList);
  }

  EGLBoolean fDestroyImage(EGLImage image) const {
    MOZ_ASSERT(HasKHRImageBase());
    return mLib->fDestroyImage(mDisplay, image);
  }

  EGLBoolean fQuerySurface(EGLSurface surface, EGLint attribute,
                           EGLint* value) const {
    return mLib->fQuerySurface(mDisplay, surface, attribute, value);
  }

  EGLBoolean fQuerySurfacePointerANGLE(EGLSurface surface, EGLint attribute,
                                       void** value) const {
    MOZ_ASSERT(IsExtensionSupported(
        EGLExtension::ANGLE_surface_d3d_texture_2d_share_handle));
    return mLib->fQuerySurfacePointerANGLE(mDisplay, surface, attribute, value);
  }

  EGLSync fCreateSync(EGLenum type, const EGLint* attrib_list) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_fence_sync));
    return mLib->fCreateSync(mDisplay, type, attrib_list);
  }

  EGLBoolean fDestroySync(EGLSync sync) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_fence_sync));
    return mLib->fDestroySync(mDisplay, sync);
  }

  EGLint fClientWaitSync(EGLSync sync, EGLint flags, EGLTime timeout) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_fence_sync));
    return mLib->fClientWaitSync(mDisplay, sync, flags, timeout);
  }

  EGLBoolean fGetSyncAttrib(EGLSync sync, EGLint attribute,
                            EGLint* value) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_fence_sync));
    return mLib->fGetSyncAttrib(mDisplay, sync, attribute, value);
  }

  EGLint fWaitSync(EGLSync sync, EGLint flags) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_wait_sync));
    return mLib->fWaitSync(mDisplay, sync, flags);
  }

  EGLint fDupNativeFenceFDANDROID(EGLSync sync) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::ANDROID_native_fence_sync));
    return mLib->fDupNativeFenceFDANDROID(mDisplay, sync);
  }

  // EXT_device_query
  EGLBoolean fQueryDisplayAttribEXT(EGLint attribute, EGLAttrib* value) const {
    MOZ_ASSERT(mLib->IsExtensionSupported(EGLLibExtension::EXT_device_query));
    return mLib->fQueryDisplayAttribEXT(mDisplay, attribute, value);
  }

  // KHR_stream
  EGLStreamKHR fCreateStreamKHR(const EGLint* attrib_list) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_stream));
    return mLib->fCreateStreamKHR(mDisplay, attrib_list);
  }

  EGLBoolean fDestroyStreamKHR(EGLStreamKHR stream) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_stream));
    return mLib->fDestroyStreamKHR(mDisplay, stream);
  }

  EGLBoolean fQueryStreamKHR(EGLStreamKHR stream, EGLenum attribute,
                             EGLint* value) const {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_stream));
    return mLib->fQueryStreamKHR(mDisplay, stream, attribute, value);
  }

  // KHR_stream_consumer_gltexture
  EGLBoolean fStreamConsumerGLTextureExternalKHR(EGLStreamKHR stream) const {
    MOZ_ASSERT(
        IsExtensionSupported(EGLExtension::KHR_stream_consumer_gltexture));
    return mLib->fStreamConsumerGLTextureExternalKHR(mDisplay, stream);
  }

  EGLBoolean fStreamConsumerAcquireKHR(EGLStreamKHR stream) const {
    MOZ_ASSERT(
        IsExtensionSupported(EGLExtension::KHR_stream_consumer_gltexture));
    return mLib->fStreamConsumerAcquireKHR(mDisplay, stream);
  }

  EGLBoolean fStreamConsumerReleaseKHR(EGLStreamKHR stream) const {
    MOZ_ASSERT(
        IsExtensionSupported(EGLExtension::KHR_stream_consumer_gltexture));
    return mLib->fStreamConsumerReleaseKHR(mDisplay, stream);
  }

  // NV_stream_consumer_gltexture_yuv
  EGLBoolean fStreamConsumerGLTextureExternalAttribsNV(
      EGLStreamKHR stream, const EGLAttrib* attrib_list) const {
    MOZ_ASSERT(
        IsExtensionSupported(EGLExtension::NV_stream_consumer_gltexture_yuv));
    return mLib->fStreamConsumerGLTextureExternalAttribsNV(mDisplay, stream,
                                                           attrib_list);
  }

  // ANGLE_stream_producer_d3d_texture
  EGLBoolean fCreateStreamProducerD3DTextureANGLE(
      EGLStreamKHR stream, const EGLAttrib* attrib_list) const {
    MOZ_ASSERT(
        IsExtensionSupported(EGLExtension::ANGLE_stream_producer_d3d_texture));
    return mLib->fCreateStreamProducerD3DTextureANGLE(mDisplay, stream,
                                                      attrib_list);
  }

  EGLBoolean fStreamPostD3DTextureANGLE(EGLStreamKHR stream, void* texture,
                                        const EGLAttrib* attrib_list) const {
    MOZ_ASSERT(
        IsExtensionSupported(EGLExtension::ANGLE_stream_producer_d3d_texture));
    return mLib->fStreamPostD3DTextureANGLE(mDisplay, stream, texture,
                                            attrib_list);
  }

  // EGL_EXT_swap_buffers_with_damage / EGL_KHR_swap_buffers_with_damage
  EGLBoolean fSwapBuffersWithDamage(EGLSurface surface, const EGLint* rects,
                                    EGLint n_rects) {
    MOZ_ASSERT(
        IsExtensionSupported(EGLExtension::EXT_swap_buffers_with_damage) ||
        IsExtensionSupported(EGLExtension::KHR_swap_buffers_with_damage));
    return mLib->fSwapBuffersWithDamage(mDisplay, surface, rects, n_rects);
  }

  // EGL_KHR_partial_update
  EGLBoolean fSetDamageRegion(EGLSurface surface, const EGLint* rects,
                              EGLint n_rects) {
    MOZ_ASSERT(IsExtensionSupported(EGLExtension::KHR_partial_update));
    return mLib->fSetDamageRegion(mDisplay, surface, rects, n_rects);
  }
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLLIBRARYEGL_H_ */

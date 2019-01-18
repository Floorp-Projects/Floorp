/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(LOCALGL_H_)
#  define LOCALGL_H_

#  include "GLTypes.h"
#  include "GLConsts.h"

namespace mozilla {
namespace gl {
class GLContext;
bool CheckContextLost(const GLContext* gl);
}  // namespace gl
}  // namespace mozilla

#  define MOZ_GL_ASSERT(glContext, expr) \
    MOZ_ASSERT((expr) || mozilla::gl::CheckContextLost(glContext))

// -

// clang-format off

// TODO: use official constant names instead of followed ones.

// IMG_texture_compression_pvrtc
#define LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1            0x8C00
#define LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1            0x8C01
#define LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1           0x8C02
#define LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1           0x8C03

// OES_EGL_image_external
#define LOCAL_GL_TEXTURE_EXTERNAL                       0x8D65
#define LOCAL_GL_TEXTURE_BINDING_EXTERNAL               0x8D67

// AMD_compressed_ATC_texture
#define LOCAL_GL_ATC_RGB                                0x8C92
#define LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA                0x8C93
#define LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA            0x87EE

// EGL_ANDROID_image_crop
#define LOCAL_EGL_IMAGE_CROP_LEFT_ANDROID               0x3148
#define LOCAL_EGL_IMAGE_CROP_TOP_ANDROID                0x3149
#define LOCAL_EGL_IMAGE_CROP_RIGHT_ANDROID              0x314A
#define LOCAL_EGL_IMAGE_CROP_BOTTOM_ANDROID             0x314B

// EGL_ANGLE_platform_angle
#define LOCAL_EGL_PLATFORM_ANGLE_ANGLE                      0x3202
#define LOCAL_EGL_PLATFORM_ANGLE_TYPE_ANGLE                 0x3203
#define LOCAL_EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE    0x3204
#define LOCAL_EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE    0x3205
#define LOCAL_EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE         0x3206

// EGL_ANGLE_keyed_mutex
#define LOCAL_EGL_DXGI_KEYED_MUTEX_ANGLE                     0x33A2

// EGL_ANGLE_stream_producer_d3d_texture
#define LOCAL_EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE           0x33AB

// EGL_ANGLE_platform_angle_d3d
#define LOCAL_EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE              0x3207
#define LOCAL_EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE             0x3208
#define LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE            0x3209
#define LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE   0x320A
#define LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE       0x320B
#define LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_REFERENCE_ANGLE  0x320C
#define LOCAL_EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE  0x320F

// EGL_ANGLE_d3d_texture_client_buffer
#define LOCAL_EGL_D3D_TEXTURE_ANGLE                          0x33A3

// EGL_ANGLE_flexible_surface_compatibility
#define LOCAL_EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6

// EGL_ANGLE_experimental_present_path
#define LOCAL_EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE             0x33A4
#define LOCAL_EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE        0x33A9
#define LOCAL_EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE        0x33AA

// EGL_ANGLE_direct3d_display
#define LOCAL_EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE         ((EGLNativeDisplayType)-2)
#define LOCAL_EGL_D3D11_ONLY_DISPLAY_ANGLE              ((EGLNativeDisplayType)-3)

// WGL_NV_DX_interop
#define LOCAL_WGL_ACCESS_READ_ONLY                      0x0000
#define LOCAL_WGL_ACCESS_READ_WRITE                     0x0001
#define LOCAL_WGL_ACCESS_WRITE_DISCARD                  0x0002

// Others
#define LOCAL_EGL_PRESERVED_RESOURCES                   0x3030
#define LOCAL_EGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_EXT 0x3138
#define LOCAL_GL_CONTEXT_FLAGS_ARB                      0x2094
#define LOCAL_GL_CONTEXT_CORE_PROFILE_BIT_ARB           0x00000001
#define LOCAL_GL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB  0x00000002
#define LOCAL_GL_CONTEXT_ROBUST_ACCESS_BIT_ARB          0x00000004

// clang-format on

#endif

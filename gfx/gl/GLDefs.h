/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(LOCALGL_H_)
#define LOCALGL_H_

#include "GLTypes.h"
#include "GLConsts.h"

// TODO: use official constant names instead of followed ones.
// IMG_texture_compression_pvrtc
#define LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1            0x8C00
#define LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1            0x8C01
#define LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1           0x8C02
#define LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1           0x8C03

// OES_EGL_image_external
#define LOCAL_GL_TEXTURE_EXTERNAL                       0x8D65
#define LOCAL_GL_TEXTURE_BINDING_EXTERNAL               0x8D67

// EGL_KHR_fence_sync
#define LOCAL_EGL_SYNC_FENCE                            0x30F9
#define LOCAL_EGL_SYNC_TYPE                             0x30F7
#define LOCAL_EGL_SYNC_STATUS                           0x30F1
#define LOCAL_EGL_SYNC_CONDITION                        0x30F8
#define LOCAL_EGL_SIGNALED                              0x30F2
#define LOCAL_EGL_UNSIGNALED                            0x30F3
#define LOCAL_EGL_SYNC_PRIOR_COMMANDS_COMPLETE          0x30F0
#define LOCAL_EGL_SYNC_FLUSH_COMMANDS_BIT               0x0001
#define LOCAL_EGL_FOREVER                               0xFFFFFFFFFFFFFFFFull
#define LOCAL_EGL_TIMEOUT_EXPIRED                       0x30F5
#define LOCAL_EGL_CONDITION_SATISFIED                   0x30F6
#define LOCAL_EGL_SUCCESS                               0x3000

// EGL_KHR_gl_texture_2D_image
#define LOCAL_EGL_GL_TEXTURE_2D                         0x30B1

// EGL_KHR_image_base (not supplied by EGL_KHR_image!)
#define LOCAL_EGL_IMAGE_PRESERVED                       0x30D2

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

// EGL_ANGLE_platform_angle_d3d
#define LOCAL_EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE              0x3207
#define LOCAL_EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE             0x3208
#define LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE            0x3209
#define LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE   0x320A
#define LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE       0x320B
#define LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_REFERENCE_ANGLE  0x320C
#define LOCAL_EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE  0x320F

// EGL_ANGLE_direct3d_display
#define LOCAL_EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE         ((EGLNativeDisplayType)-2)
#define LOCAL_EGL_D3D11_ONLY_DISPLAY_ANGLE              ((EGLNativeDisplayType)-3)

// WGL_NV_DX_interop
#define LOCAL_WGL_ACCESS_READ_ONLY                      0x0000
#define LOCAL_WGL_ACCESS_READ_WRITE                     0x0001
#define LOCAL_WGL_ACCESS_WRITE_DISCARD                  0x0002

// GL_KHR_robustness
#define LOCAL_GL_CONTEXT_ROBUST_ACCESS                  0x90F3

// Others
#define LOCAL_EGL_PRESERVED_RESOURCES                   0x3030
#define LOCAL_EGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_EXT 0x3138
#define LOCAL_GL_RESET_NOTIFICATION_STRATEGY            0x8256
#define LOCAL_GL_LOSE_CONTEXT_ON_RESET                  0x8252
#define LOCAL_GL_NO_RESET_NOTIFICATION                  0x8261
#define LOCAL_GL_CONTEXT_LOST                           0x9242
#define LOCAL_GL_CONTEXT_FLAGS_ARB                      0x2094
#define LOCAL_GL_CONTEXT_CORE_PROFILE_BIT_ARB           0x00000001
#define LOCAL_GL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB  0x00000002
#define LOCAL_GL_CONTEXT_ROBUST_ACCESS_BIT_ARB          0x00000004

#endif

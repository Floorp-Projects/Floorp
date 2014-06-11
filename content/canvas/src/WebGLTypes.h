/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLTYPES_H_
#define WEBGLTYPES_H_

#include "mozilla/TypedEnum.h"

// Most WebIDL typedefs are identical to their OpenGL counterparts.
#include "GLTypes.h"

// Manual reflection of WebIDL typedefs that are different from their
// OpenGL counterparts.
typedef int64_t WebGLsizeiptr;
typedef int64_t WebGLintptr;
typedef bool WebGLboolean;

namespace mozilla {

/*
 * WebGLContextFakeBlackStatus and WebGLTextureFakeBlackStatus are enums to
 * track what needs to use a dummy 1x1 black texture, which we refer to as a
 * 'fake black' texture.
 *
 * There are generally two things that can cause us to use such 'fake black'
 * textures:
 *
 *   (1) OpenGL ES rules on sampling incomplete textures specify that they
 *       must be sampled as RGBA(0, 0, 0, 1) (opaque black). We have to implement these rules
 *       ourselves, if only because we do not always run on OpenGL ES, and also
 *       because this is dangerously close to the kind of case where we don't
 *       want to trust the driver with corner cases of texture memory accesses.
 *
 *   (2) OpenGL has cases where a renderbuffer, or a texture image, can contain
 *       uninitialized image data. See below the comment about WebGLImageDataStatus.
 *       WebGL must never have access to uninitialized image data. The WebGL 1 spec,
 *       section 4.1 'Resource Restrictions', specifies that in any such case, the
 *       uninitialized image data must be exposed to WebGL as if it were filled
 *       with zero bytes, which means it's either opaque or transparent black
 *       depending on whether the image format has alpha.
 *
 * Why are there _two_ separate enums there, WebGLContextFakeBlackStatus
 * and WebGLTextureFakeBlackStatus? That's because each texture must know the precise
 * reason why it needs to be faked (incomplete texture vs. uninitialized image data),
 * whereas the WebGL context can only know whether _any_ faking is currently needed at all.
 */
MOZ_BEGIN_ENUM_CLASS(WebGLContextFakeBlackStatus, int)
  Unknown,
  NotNeeded,
  Needed
MOZ_END_ENUM_CLASS(WebGLContextFakeBlackStatus)

MOZ_BEGIN_ENUM_CLASS(WebGLTextureFakeBlackStatus, int)
  Unknown,
  NotNeeded,
  IncompleteTexture,
  UninitializedImageData
MOZ_END_ENUM_CLASS(WebGLTextureFakeBlackStatus)

/*
 * Implementing WebGL (or OpenGL ES 2.0) on top of desktop OpenGL requires
 * emulating the vertex attrib 0 array when it's not enabled. Indeed,
 * OpenGL ES 2.0 allows drawing without vertex attrib 0 array enabled, but
 * desktop OpenGL does not allow that.
 */
MOZ_BEGIN_ENUM_CLASS(WebGLVertexAttrib0Status, int)
    Default, // default status - no emulation needed
    EmulatedUninitializedArray, // need an artificial attrib 0 array, but contents may be left uninitialized
    EmulatedInitializedArray // need an artificial attrib 0 array, and contents must be initialized
MOZ_END_ENUM_CLASS(WebGLVertexAttrib0Status)

/*
 * Enum to track the status of image data (renderbuffer or texture image) presence
 * and initialization.
 *
 * - NoImageData is the initial state before any image data is allocated.
 * - InitializedImageData is the state after image data is allocated and initialized.
 * - UninitializedImageData is an intermediate state where data is allocated but not
 *   initialized. It is the state that renderbuffers are in after a renderbufferStorage call,
 *   and it is the state that texture images are in after a texImage2D call with null data.
 */
MOZ_BEGIN_ENUM_CLASS(WebGLImageDataStatus, int)
    NoImageData,
    UninitializedImageData,
    InitializedImageData
MOZ_END_ENUM_CLASS(WebGLImageDataStatus)

/*
 * The formats that may participate, either as source or destination formats,
 * in WebGL texture conversions. This includes:
 *  - all the formats accepted by WebGL.texImage2D, e.g. RGBA4444
 *  - additional formats provided by extensions, e.g. RGB32F
 *  - additional source formats, depending on browser details, used when uploading
 *    textures from DOM elements. See gfxImageSurface::Format().
 */
MOZ_BEGIN_ENUM_CLASS(WebGLTexelFormat, int)
    // returned by SurfaceFromElementResultToImageSurface to indicate absence of image data
    None,
    // dummy error code returned by GetWebGLTexelFormat in error cases,
    // after assertion failure (so this never happens in debug builds)
    BadFormat,
    // dummy pseudo-format meaning "use the other format".
    // For example, if SrcFormat=Auto and DstFormat=RGB8, then the source
    // is implicitly treated as being RGB8 itself.
    Auto,
    // 1-channel formats
    R8,
    A8,
    D16, // WEBGL_depth_texture
    D32, // WEBGL_depth_texture
    R16F, // OES_texture_half_float
    A16F, // OES_texture_half_float
    R32F, // OES_texture_float
    A32F, // OES_texture_float
    // 2-channel formats
    RA8,
    RA16F, // OES_texture_half_float
    RA32F, // OES_texture_float
    D24S8, // WEBGL_depth_texture
    // 3-channel formats
    RGB8,
    BGRX8, // used for DOM elements. Source format only.
    RGB565,
    RGB16F, // OES_texture_half_float
    RGB32F, // OES_texture_float
    // 4-channel formats
    RGBA8,
    BGRA8, // used for DOM elements
    RGBA5551,
    RGBA4444,
    RGBA16F, // OES_texture_half_float
    RGBA32F // OES_texture_float
MOZ_END_ENUM_CLASS(WebGLTexelFormat)

MOZ_BEGIN_ENUM_CLASS(WebGLTexImageFunc, int)
    TexImage,
    TexSubImage,
    CopyTexImage,
    CopyTexSubImage,
    CompTexImage,
    CompTexSubImage,
MOZ_END_ENUM_CLASS(WebGLTexImageFunc)

// Please keep extensions in alphabetic order.
MOZ_BEGIN_ENUM_CLASS(WebGLExtensionID, uint8_t)
    ANGLE_instanced_arrays,
    EXT_blend_minmax,
    EXT_color_buffer_half_float,
    EXT_frag_depth,
    EXT_sRGB,
    EXT_texture_filter_anisotropic,
    OES_element_index_uint,
    OES_standard_derivatives,
    OES_texture_float,
    OES_texture_float_linear,
    OES_texture_half_float,
    OES_texture_half_float_linear,
    OES_vertex_array_object,
    WEBGL_color_buffer_float,
    WEBGL_compressed_texture_atc,
    WEBGL_compressed_texture_etc1,
    WEBGL_compressed_texture_pvrtc,
    WEBGL_compressed_texture_s3tc,
    WEBGL_debug_renderer_info,
    WEBGL_debug_shaders,
    WEBGL_depth_texture,
    WEBGL_draw_buffers,
    WEBGL_lose_context,
    Max,
    Unknown
MOZ_END_ENUM_CLASS(WebGLExtensionID)

} // namespace mozilla

#endif

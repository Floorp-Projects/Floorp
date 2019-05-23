/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLTYPES_H_
#define WEBGLTYPES_H_

#include <limits>

// Most WebIDL typedefs are identical to their OpenGL counterparts.
#include "GLTypes.h"
#include "mozilla/Casting.h"

// Manual reflection of WebIDL typedefs that are different from their
// OpenGL counterparts.
typedef int64_t WebGLsizeiptr;
typedef int64_t WebGLintptr;
typedef bool WebGLboolean;

// -

namespace mozilla {
namespace gl {
class GLContext;  // This is going to be needed a lot.
}  // namespace gl

// -
// Prevent implicit conversions into calloc and malloc. (mozilla namespace
// only!)

template <typename DestT>
class ForbidNarrowing final {
  DestT mVal;

 public:
  template <typename SrcT>
  MOZ_IMPLICIT ForbidNarrowing(SrcT val) : mVal(val) {
    static_assert(
        std::numeric_limits<SrcT>::min() >= std::numeric_limits<DestT>::min(),
        "SrcT must be narrower than DestT.");
    static_assert(
        std::numeric_limits<SrcT>::max() <= std::numeric_limits<DestT>::max(),
        "SrcT must be narrower than DestT.");
  }

  explicit operator DestT() const { return mVal; }
};

inline void* malloc(const ForbidNarrowing<size_t> s) {
  return ::malloc(size_t(s));
}

inline void* calloc(const ForbidNarrowing<size_t> n,
                    const ForbidNarrowing<size_t> size) {
  return ::calloc(size_t(n), size_t(size));
}

// -

namespace detail {

template <typename From>
class AutoAssertCastT final {
  const From mVal;

 public:
  explicit AutoAssertCastT(const From val) : mVal(val) {}

  template <typename To>
  operator To() const {
    return AssertedCast<To>(mVal);
  }
};

}  // namespace detail

template <typename From>
inline auto AutoAssertCast(const From val) {
  return detail::AutoAssertCastT<From>(val);
}

/*
 * Implementing WebGL (or OpenGL ES 2.0) on top of desktop OpenGL requires
 * emulating the vertex attrib 0 array when it's not enabled. Indeed,
 * OpenGL ES 2.0 allows drawing without vertex attrib 0 array enabled, but
 * desktop OpenGL does not allow that.
 */
enum class WebGLVertexAttrib0Status : uint8_t {
  Default,                     // default status - no emulation needed
  EmulatedUninitializedArray,  // need an artificial attrib 0 array, but
                               // contents may be left uninitialized
  EmulatedInitializedArray  // need an artificial attrib 0 array, and contents
                            // must be initialized
};

/*
 * The formats that may participate, either as source or destination formats,
 * in WebGL texture conversions. This includes:
 *  - all the formats accepted by WebGL.texImage2D, e.g. RGBA4444
 *  - additional formats provided by extensions, e.g. RGB32F
 *  - additional source formats, depending on browser details, used when
 * uploading textures from DOM elements. See gfxImageSurface::Format().
 */
enum class WebGLTexelFormat : uint8_t {
  // returned by SurfaceFromElementResultToImageSurface to indicate absence of
  // image data
  None,
  // common value for formats for which format conversions are not supported
  FormatNotSupportingAnyConversion,
  // dummy pseudo-format meaning "use the other format".
  // For example, if SrcFormat=Auto and DstFormat=RGB8, then the source
  // is implicitly treated as being RGB8 itself.
  Auto,
  // 1-channel formats
  A8,
  A16F,  // OES_texture_half_float
  A32F,  // OES_texture_float
  R8,
  R16F,  // OES_texture_half_float
  R32F,  // OES_texture_float
  // 2-channel formats
  RA8,
  RA16F,  // OES_texture_half_float
  RA32F,  // OES_texture_float
  RG8,
  RG16F,
  RG32F,
  // 3-channel formats
  RGB8,
  RGB565,
  RGB11F11F10F,
  RGB16F,  // OES_texture_half_float
  RGB32F,  // OES_texture_float
  // 4-channel formats
  RGBA8,
  RGBA5551,
  RGBA4444,
  RGBA16F,  // OES_texture_half_float
  RGBA32F,  // OES_texture_float
  // DOM element source only formats.
  RGBX8,
  BGRX8,
  BGRA8
};

enum class WebGLTexImageFunc : uint8_t {
  TexImage,
  TexSubImage,
  CopyTexImage,
  CopyTexSubImage,
  CompTexImage,
  CompTexSubImage,
};

enum class WebGLTexDimensions : uint8_t { Tex2D, Tex3D };

// Please keep extensions in alphabetic order.
enum class WebGLExtensionID : uint8_t {
  ANGLE_instanced_arrays,
  EXT_blend_minmax,
  EXT_color_buffer_float,
  EXT_color_buffer_half_float,
  EXT_disjoint_timer_query,
  EXT_float_blend,
  EXT_frag_depth,
  EXT_shader_texture_lod,
  EXT_sRGB,
  EXT_texture_compression_bptc,
  EXT_texture_compression_rgtc,
  EXT_texture_filter_anisotropic,
  MOZ_debug,
  OES_element_index_uint,
  OES_fbo_render_mipmap,
  OES_standard_derivatives,
  OES_texture_float,
  OES_texture_float_linear,
  OES_texture_half_float,
  OES_texture_half_float_linear,
  OES_vertex_array_object,
  WEBGL_color_buffer_float,
  WEBGL_compressed_texture_astc,
  WEBGL_compressed_texture_etc,
  WEBGL_compressed_texture_etc1,
  WEBGL_compressed_texture_pvrtc,
  WEBGL_compressed_texture_s3tc,
  WEBGL_compressed_texture_s3tc_srgb,
  WEBGL_debug_renderer_info,
  WEBGL_debug_shaders,
  WEBGL_depth_texture,
  WEBGL_draw_buffers,
  WEBGL_lose_context,
  Max
};

class UniqueBuffer {
  // Like UniquePtr<>, but for void* and malloc/calloc/free.
  void* mBuffer;

 public:
  UniqueBuffer() : mBuffer(nullptr) {}

  MOZ_IMPLICIT UniqueBuffer(void* buffer) : mBuffer(buffer) {}

  ~UniqueBuffer() { free(mBuffer); }

  UniqueBuffer(UniqueBuffer&& other) {
    this->mBuffer = other.mBuffer;
    other.mBuffer = nullptr;
  }

  UniqueBuffer& operator=(UniqueBuffer&& other) {
    free(this->mBuffer);
    this->mBuffer = other.mBuffer;
    other.mBuffer = nullptr;
    return *this;
  }

  UniqueBuffer& operator=(void* newBuffer) {
    free(this->mBuffer);
    this->mBuffer = newBuffer;
    return *this;
  }

  explicit operator bool() const { return bool(mBuffer); }

  void* get() const { return mBuffer; }

  UniqueBuffer(const UniqueBuffer& other) =
      delete;  // construct using std::move()!
  void operator=(const UniqueBuffer& other) =
      delete;  // assign using std::move()!
};

namespace webgl {
struct FormatUsageInfo;

struct SampleableInfo final {
  const char* incompleteReason = nullptr;
  uint32_t levels = 0;
  const webgl::FormatUsageInfo* usage = nullptr;
  bool isDepthTexCompare = false;

  bool IsComplete() const { return bool(levels); }
};

enum class AttribBaseType : uint8_t {
  Int,
  UInt,
  Float,    // Also includes NormU?Int
  Boolean,  // Can convert from anything.
};
const char* ToString(AttribBaseType);

}  // namespace webgl

}  // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLTYPES_H_
#define WEBGLTYPES_H_

#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "GLDefs.h"
#include "ImageContainer.h"
#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Range.h"
#include "mozilla/RefCounted.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BuildConstants.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "gfxTypes.h"

#include "nsTArray.h"
#include "nsString.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/ipc/SharedMemoryBasic.h"

// Manual reflection of WebIDL typedefs that are different from their
// OpenGL counterparts.
using WebGLsizeiptr = int64_t;
using WebGLintptr = int64_t;
using WebGLboolean = bool;

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

namespace webgl {
template <typename T>
struct QueueParamTraits;
class TexUnpackBytes;
class TexUnpackImage;
class TexUnpackSurface;
}  // namespace webgl

class ClientWebGLContext;
struct WebGLTexPboOffset;
class WebGLTexture;
class WebGLBuffer;
class WebGLFramebuffer;
class WebGLProgram;
class WebGLQuery;
class WebGLRenderbuffer;
class WebGLSampler;
class WebGLShader;
class WebGLSync;
class WebGLTexture;
class WebGLTransformFeedback;
class WebGLVertexArray;

// -

class VRefCounted : public RefCounted<VRefCounted> {
 public:
  virtual ~VRefCounted() = default;

#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
  virtual const char* typeName() const = 0;
  virtual size_t typeSize() const = 0;
#endif
};

// -

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
  EXT_texture_norm16,
  MOZ_debug,
  OES_draw_buffers_indexed,
  OES_element_index_uint,
  OES_fbo_render_mipmap,
  OES_standard_derivatives,
  OES_texture_float,
  OES_texture_float_linear,
  OES_texture_half_float,
  OES_texture_half_float_linear,
  OES_vertex_array_object,
  OVR_multiview2,
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
  WEBGL_explicit_present,
  WEBGL_lose_context,
  Max
};

class UniqueBuffer final {
  // Like UniquePtr<>, but for void* and malloc/calloc/free.
  void* mBuffer = nullptr;

 public:
  static inline UniqueBuffer Take(void* buffer) {
    UniqueBuffer ret;
    ret.mBuffer = buffer;
    return ret;
  }

  UniqueBuffer() = default;

  ~UniqueBuffer() { reset(); }

  UniqueBuffer(UniqueBuffer&& rhs) { *this = std::move(rhs); }

  UniqueBuffer& operator=(UniqueBuffer&& rhs) {
    reset();
    this->mBuffer = rhs.mBuffer;
    rhs.mBuffer = nullptr;
    return *this;
  }

  explicit operator bool() const { return bool(mBuffer); }

  void* get() const { return mBuffer; }

  void reset() {
    // Believe it or not, when `free` unconditional, it was showing up
    // in profiles, nearly 20% of time spent in MethodDispatcther<UniformData>
    // on Aquarium.
    if (mBuffer) {
      free(mBuffer);
      mBuffer = nullptr;
    }
  }
};

namespace webgl {
struct FormatUsageInfo;

static constexpr GLenum kErrorPerfWarning = 0x10001;

struct SampleableInfo final {
  const char* incompleteReason = nullptr;
  uint32_t levels = 0;
  const webgl::FormatUsageInfo* usage = nullptr;
  bool isDepthTexCompare = false;

  bool IsComplete() const { return bool(levels); }
};

enum class AttribBaseType : uint8_t {
  Boolean,  // Can convert from anything.
  Float,    // Also includes NormU?Int
  Int,
  Uint,
};
webgl::AttribBaseType ToAttribBaseType(GLenum);
const char* ToString(AttribBaseType);

enum class UniformBaseType : uint8_t {
  Float,
  Int,
  Uint,
};
const char* ToString(UniformBaseType);

using ObjectId = uint64_t;

enum class BufferKind : uint8_t {
  Undefined,
  Index,
  NonIndex,
};

}  // namespace webgl

// -

struct FloatOrInt final  // For TexParameter[fi] and friends.
{
  bool isFloat = false;
  GLfloat f = 0;
  GLint i = 0;

  explicit FloatOrInt(GLint x = 0) : isFloat(false), f(x), i(x) {}

  explicit FloatOrInt(GLfloat x) : isFloat(true), f(x), i(roundf(x)) {}
};

struct WebGLContextOptions {
  bool alpha = true;
  bool depth = true;
  bool stencil = false;
  bool premultipliedAlpha = true;
  bool antialias = true;
  bool preserveDrawingBuffer = false;
  bool failIfMajorPerformanceCaveat = false;
  bool xrCompatible = false;
  dom::WebGLPowerPreference powerPreference =
      dom::WebGLPowerPreference::Default;
  dom::PredefinedColorSpace colorSpace = dom::PredefinedColorSpace::Srgb;
  bool ignoreColorSpace = true;  // Our legacy behavior.
  bool shouldResistFingerprinting = true;
  bool enableDebugRendererInfo = false;

  WebGLContextOptions();
  WebGLContextOptions(const WebGLContextOptions&) = default;

  bool operator==(const WebGLContextOptions&) const;
  bool operator!=(const WebGLContextOptions& rhs) const {
    return !(*this == rhs);
  }
};

namespace gfx {

inline ColorSpace2 ToColorSpace2(const dom::PredefinedColorSpace cs) {
  switch (cs) {
    case dom::PredefinedColorSpace::Srgb:
      return ColorSpace2::SRGB;
    case dom::PredefinedColorSpace::Display_p3:
      return ColorSpace2::DISPLAY_P3;
    case dom::PredefinedColorSpace::EndGuard_:
      break;
  }
  MOZ_CRASH("Exhaustive switch");
}

}  // namespace gfx

// -

template <typename _T>
struct avec2 {
  using T = _T;

  T x = T();
  T y = T();

  template <typename U, typename V>
  static Maybe<avec2> From(const U _x, const V _y) {
    const auto x = CheckedInt<T>(_x);
    const auto y = CheckedInt<T>(_y);
    if (!x.isValid() || !y.isValid()) return {};
    return Some(avec2(x.value(), y.value()));
  }

  template <typename U>
  static auto From(const U& val) {
    return From(val.x, val.y);
  }
  template <typename U>
  static auto FromSize(const U& val) {
    return From(val.width, val.height);
  }

  avec2() = default;
  avec2(const T _x, const T _y) : x(_x), y(_y) {}

  bool operator==(const avec2& rhs) const { return x == rhs.x && y == rhs.y; }
  bool operator!=(const avec2& rhs) const { return !(*this == rhs); }

#define _(OP)                                 \
  avec2 operator OP(const avec2& rhs) const { \
    return {x OP rhs.x, y OP rhs.y};          \
  }                                           \
  avec2 operator OP(const T rhs) const { return {x OP rhs, y OP rhs}; }

  _(+)
  _(-)
  _(*)
  _(/)

#undef _

  avec2 Clamp(const avec2& min, const avec2& max) const {
    return {mozilla::Clamp(x, min.x, max.x), mozilla::Clamp(y, min.y, max.y)};
  }

  // mozilla::Clamp doesn't work on floats, so be clear that this is a min+max
  // helper.
  avec2 ClampMinMax(const avec2& min, const avec2& max) const {
    const auto ClampScalar = [](const T v, const T min, const T max) {
      return std::max(min, std::min(v, max));
    };
    return {ClampScalar(x, min.x, max.x), ClampScalar(y, min.y, max.y)};
  }

  template <typename U>
  U StaticCast() const {
    return {static_cast<typename U::T>(x), static_cast<typename U::T>(y)};
  }
};

template <typename T>
avec2<T> MinExtents(const avec2<T>& a, const avec2<T>& b) {
  return {std::min(a.x, b.x), std::min(a.y, b.y)};
}

template <typename T>
avec2<T> MaxExtents(const avec2<T>& a, const avec2<T>& b) {
  return {std::max(a.x, b.x), std::max(a.y, b.y)};
}

// -

template <typename _T>
struct avec3 {
  using T = _T;

  T x = T();
  T y = T();
  T z = T();

  template <typename U, typename V>
  static Maybe<avec3> From(const U _x, const V _y, const V _z) {
    const auto x = CheckedInt<T>(_x);
    const auto y = CheckedInt<T>(_y);
    const auto z = CheckedInt<T>(_z);
    if (!x.isValid() || !y.isValid() || !z.isValid()) return {};
    return Some(avec3(x.value(), y.value(), z.value()));
  }

  template <typename U>
  static auto From(const U& val) {
    return From(val.x, val.y, val.z);
  }

  avec3() = default;
  avec3(const T _x, const T _y, const T _z) : x(_x), y(_y), z(_z) {}

  bool operator==(const avec3& rhs) const {
    return x == rhs.x && y == rhs.y && z == rhs.z;
  }
  bool operator!=(const avec3& rhs) const { return !(*this == rhs); }
};

using ivec2 = avec2<int32_t>;
using ivec3 = avec3<int32_t>;
using uvec2 = avec2<uint32_t>;
using uvec3 = avec3<uint32_t>;

inline ivec2 AsVec(const gfx::IntSize& s) { return {s.width, s.height}; }

// -

namespace webgl {

struct PackingInfo final {
  GLenum format = 0;
  GLenum type = 0;

  bool operator<(const PackingInfo& x) const {
    if (format != x.format) return format < x.format;

    return type < x.type;
  }

  bool operator==(const PackingInfo& x) const {
    return (format == x.format && type == x.type);
  }
};

struct DriverUnpackInfo final {
  GLenum internalFormat = 0;
  GLenum unpackFormat = 0;
  GLenum unpackType = 0;

  PackingInfo ToPacking() const { return {unpackFormat, unpackType}; }
};

// -

template <typename E>
class EnumMask {
 public:
  uint64_t mBits = 0;

 private:
  struct BitRef final {
    EnumMask& bits;
    const uint64_t mask;

    explicit operator bool() const { return bits.mBits & mask; }

    auto& operator=(const bool val) {
      if (val) {
        bits.mBits |= mask;
      } else {
        bits.mBits &= ~mask;
      }
      return *this;
    }
  };

  uint64_t Mask(const E i) const {
    return uint64_t{1} << static_cast<uint64_t>(i);
  }

 public:
  BitRef operator[](const E i) { return {*this, Mask(i)}; }
  bool operator[](const E i) const { return mBits & Mask(i); }
};

class ExtensionBits : public EnumMask<WebGLExtensionID> {};

// -

enum class ContextLossReason : uint8_t {
  None,
  Manual,
  Guilty,
};

inline bool ReadContextLossReason(const uint8_t val,
                                  ContextLossReason* const out) {
  if (val > static_cast<uint8_t>(ContextLossReason::Guilty)) {
    return false;
  }
  *out = static_cast<ContextLossReason>(val);
  return true;
}

// -

struct InitContextDesc final {
  bool isWebgl2 = false;
  bool resistFingerprinting = false;
  uvec2 size = {};
  WebGLContextOptions options;
  uint32_t principalKey = 0;
};

constexpr uint32_t kMaxTransformFeedbackSeparateAttribs = 4;

struct Limits final {
  ExtensionBits supportedExtensions;

  // WebGL 1
  uint32_t maxTexUnits = 0;
  uint32_t maxTex2dSize = 0;
  uint32_t maxTexCubeSize = 0;
  uint32_t maxVertexAttribs = 0;
  uint32_t maxViewportDim = 0;
  std::array<float, 2> pointSizeRange = {{1, 1}};
  std::array<float, 2> lineWidthRange = {{1, 1}};

  // WebGL 2
  uint32_t maxTexArrayLayers = 0;
  uint32_t maxTex3dSize = 0;
  uint32_t maxUniformBufferBindings = 0;
  uint32_t uniformBufferOffsetAlignment = 0;

  // Exts
  bool astcHdr = false;
  uint32_t maxColorDrawBuffers = 1;
  uint64_t queryCounterBitsTimeElapsed = 0;
  uint64_t queryCounterBitsTimestamp = 0;
  uint32_t maxMultiviewLayers = 0;
};

struct InitContextResult final {
  std::string error;
  WebGLContextOptions options;
  webgl::Limits limits;
  EnumMask<layers::SurfaceDescriptor::Type> uploadableSdTypes;
};

// -

struct ErrorInfo final {
  GLenum type;
  std::string info;
};

struct ShaderPrecisionFormat final {
  GLint rangeMin = 0;
  GLint rangeMax = 0;
  GLint precision = 0;
};

// -

enum class LossStatus {
  Ready,

  Lost,
  LostForever,
  LostManually,
};

// -

struct CompileResult final {
  bool pending = true;
  nsCString log;
  nsCString translatedSource;
  bool success = false;
};

// -

struct OpaqueFramebufferOptions final {
  bool depthStencil = true;
  bool antialias = true;
  uint32_t width = 0;
  uint32_t height = 0;
};

// -

struct SwapChainOptions final {
  bool bgra = false;
};

// -

struct ActiveInfo {
  GLenum elemType = 0;     // `type`
  uint32_t elemCount = 0;  // `size`
  std::string name;
};

struct ActiveAttribInfo final : public ActiveInfo {
  int32_t location = -1;
  AttribBaseType baseType = AttribBaseType::Float;
};

struct ActiveUniformInfo final : public ActiveInfo {
  std::unordered_map<uint32_t, uint32_t>
      locByIndex;  // Uniform array locations are sparse.
  int32_t block_index = -1;
  int32_t block_offset = -1;  // In block, offset.
  int32_t block_arrayStride = -1;
  int32_t block_matrixStride = -1;
  bool block_isRowMajor = false;
};

struct ActiveUniformBlockInfo final {
  std::string name;
  // BLOCK_BINDING is dynamic state
  uint32_t dataSize = 0;
  std::vector<uint32_t> activeUniformIndices;
  bool referencedByVertexShader = false;
  bool referencedByFragmentShader = false;
};

struct LinkActiveInfo final {
  std::vector<ActiveAttribInfo> activeAttribs;
  std::vector<ActiveUniformInfo> activeUniforms;
  std::vector<ActiveUniformBlockInfo> activeUniformBlocks;
  std::vector<ActiveInfo> activeTfVaryings;
};

struct LinkResult final {
  bool pending = true;
  nsCString log;
  bool success = false;
  LinkActiveInfo active;
  GLenum tfBufferMode = 0;
};

// -

/// 4x32-bit primitives, with a type tag.
struct TypedQuad final {
  alignas(alignof(float)) uint8_t data[4 * sizeof(float)] = {};
  webgl::AttribBaseType type = webgl::AttribBaseType::Float;
};

/// [1-16]x32-bit primitives, with a type tag.
struct GetUniformData final {
  alignas(alignof(float)) uint8_t data[4 * 4 * sizeof(float)] = {};
  GLenum type = 0;
};

struct FrontBufferSnapshotIpc final {
  uvec2 surfSize = {};
  Maybe<mozilla::ipc::Shmem> shmem = {};
};

struct ReadPixelsResult {
  gfx::IntRect subrect = {};
  size_t byteStride = 0;
};

struct ReadPixelsResultIpc final : public ReadPixelsResult {
  mozilla::ipc::Shmem shmem = {};
};

struct VertAttribPointerDesc final {
  bool intFunc = false;
  uint8_t channels = 4;
  bool normalized = false;
  uint8_t byteStrideOrZero = 0;
  GLenum type = LOCAL_GL_FLOAT;
  uint64_t byteOffset = 0;
};

struct VertAttribPointerCalculated final {
  uint8_t byteSize = 4 * 4;
  uint8_t byteStride = 4 * 4;  // at-most 255
  webgl::AttribBaseType baseType = webgl::AttribBaseType::Float;
};

}  // namespace webgl

/**
 * Represents a block of memory that it may or may not own.  The
 * inner data type must be trivially copyable by memcpy.
 */
template <typename T = uint8_t>
class RawBuffer final {
  const T* mBegin = nullptr;
  size_t mLen = 0;
  UniqueBuffer mOwned;

 public:
  using ElementType = T;

  /**
   * If aTakeData is true, RawBuffer will delete[] the memory when destroyed.
   */
  explicit RawBuffer(const Range<const T>& data, UniqueBuffer&& owned = {})
      : mBegin(data.begin().get()),
        mLen(data.length()),
        mOwned(std::move(owned)) {}

  explicit RawBuffer(const size_t len) : mLen(len) {}

  ~RawBuffer() = default;

  Range<const T> Data() const { return {mBegin, mLen}; }
  const auto& begin() const { return mBegin; }
  const auto& size() const { return mLen; }

  void Shrink(const size_t newLen) {
    if (mLen <= newLen) return;
    mLen = newLen;
  }

  RawBuffer() = default;

  RawBuffer(const RawBuffer&) = delete;
  RawBuffer& operator=(const RawBuffer&) = delete;

  RawBuffer(RawBuffer&&) = default;
  RawBuffer& operator=(RawBuffer&&) = default;
};

// -

struct CopyableRange final : public Range<const uint8_t> {};

// -

// clang-format off

#define FOREACH_ID(X) \
  X(FuncScopeIdError) \
  X(compressedTexImage2D) \
  X(compressedTexImage3D) \
  X(compressedTexSubImage2D) \
  X(compressedTexSubImage3D) \
  X(copyTexSubImage2D) \
  X(copyTexSubImage3D) \
  X(drawArrays) \
  X(drawArraysInstanced) \
  X(drawElements) \
  X(drawElementsInstanced) \
  X(drawRangeElements) \
  X(renderbufferStorage) \
  X(renderbufferStorageMultisample) \
  X(texImage2D) \
  X(texImage3D) \
  X(TexStorage2D) \
  X(TexStorage3D) \
  X(texSubImage2D) \
  X(texSubImage3D) \
  X(vertexAttrib1f) \
  X(vertexAttrib1fv) \
  X(vertexAttrib2f) \
  X(vertexAttrib2fv) \
  X(vertexAttrib3f) \
  X(vertexAttrib3fv) \
  X(vertexAttrib4f) \
  X(vertexAttrib4fv) \
  X(vertexAttribI4i) \
  X(vertexAttribI4iv) \
  X(vertexAttribI4ui) \
  X(vertexAttribI4uiv) \
  X(vertexAttribIPointer) \
  X(vertexAttribPointer)

// clang-format on

enum class FuncScopeId {
#define _(X) X,
  FOREACH_ID(_)
#undef _
};

static constexpr const char* const FUNCSCOPE_NAME_BY_ID[] = {
#define _(X) #X,
    FOREACH_ID(_)
#undef _
};

#undef FOREACH_ID

inline auto GetFuncScopeName(const FuncScopeId id) {
  return FUNCSCOPE_NAME_BY_ID[static_cast<size_t>(id)];
}

// -

template <typename C, typename K>
inline auto MaybeFind(C& container, const K& key)
    -> decltype(&(container.find(key)->second)) {
  const auto itr = container.find(key);
  if (itr == container.end()) return nullptr;
  return &(itr->second);
}

template <typename C, typename K>
inline typename C::mapped_type Find(
    const C& container, const K& key,
    const typename C::mapped_type notFound = {}) {
  const auto itr = container.find(key);
  if (itr == container.end()) return notFound;
  return itr->second;
}

// -

template <typename T, typename U>
inline Maybe<T> MaybeAs(const U val) {
  const auto checked = CheckedInt<T>(val);
  if (!checked.isValid()) return {};
  return Some(checked.value());
}

// -

inline GLenum ImageToTexTarget(const GLenum imageTarget) {
  switch (imageTarget) {
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      return LOCAL_GL_TEXTURE_CUBE_MAP;
    default:
      return imageTarget;
  }
}

inline bool IsTexTarget3D(const GLenum texTarget) {
  switch (texTarget) {
    case LOCAL_GL_TEXTURE_2D_ARRAY:
    case LOCAL_GL_TEXTURE_3D:
      return true;

    default:
      return false;
  }
}

// -

namespace dom {
class Element;
class ImageBitmap;
class ImageData;
class OffscreenCanvas;
}  // namespace dom

struct TexImageSource {
  const dom::ArrayBufferView* mView = nullptr;
  GLuint mViewElemOffset = 0;
  GLuint mViewElemLengthOverride = 0;

  const WebGLintptr* mPboOffset = nullptr;

  const dom::ImageBitmap* mImageBitmap = nullptr;
  const dom::ImageData* mImageData = nullptr;

  const dom::OffscreenCanvas* mOffscreenCanvas = nullptr;

  const dom::Element* mDomElem = nullptr;
  ErrorResult* mOut_error = nullptr;
};

namespace webgl {

template <class DerivedT>
struct DeriveNotEq {
  bool operator!=(const DerivedT& rhs) const {
    const auto self = reinterpret_cast<const DerivedT*>(this);
    return !(*self == rhs);
  }
};

struct PixelPackingState : public DeriveNotEq<PixelPackingState> {
  uint32_t alignmentInTypeElems = 4;  // ALIGNMENT isn't naive byte alignment!
  uint32_t rowLength = 0;
  uint32_t imageHeight = 0;
  uint32_t skipPixels = 0;
  uint32_t skipRows = 0;
  uint32_t skipImages = 0;

  // C++20's default comparison operators can't come soon enough!
  bool operator==(const PixelPackingState& rhs) const {
    return alignmentInTypeElems == rhs.alignmentInTypeElems &&
           rowLength == rhs.rowLength && imageHeight == rhs.imageHeight &&
           skipPixels == rhs.skipPixels && skipRows == rhs.skipRows &&
           skipImages == rhs.skipImages;
  }

  static void AssertDefaultUnpack(gl::GLContext& gl, const bool isWebgl2) {
    PixelPackingState{}.AssertCurrentUnpack(gl, isWebgl2);
  }

  void ApplyUnpack(gl::GLContext&, bool isWebgl2,
                   const uvec3& uploadSize) const;
  bool AssertCurrentUnpack(gl::GLContext&, bool isWebgl2) const;
};

struct PixelUnpackStateWebgl final : public PixelPackingState {
  GLenum colorspaceConversion =
      dom::WebGLRenderingContext_Binding::BROWSER_DEFAULT_WEBGL;
  bool flipY = false;
  bool premultiplyAlpha = false;
  bool requireFastPath = false;
};

struct ExplicitPixelPackingState final {
  struct Metrics final {
    uvec3 usedSize = {};
    size_t bytesPerPixel = 0;

    // (srcStrideAndRowOverride.x, otherwise ROW_LENGTH != 0, otherwise size.x)
    // ...aligned to ALIGNMENT.
    size_t bytesPerRowStride = 0;

    // structuredSrcSize.y, otherwise IMAGE_HEIGHT*(SKIP_IMAGES+size.z)
    size_t totalRows = 0;

    // This ensures that no one else needs to do CheckedInt math.
    size_t totalBytesUsed = 0;
    size_t totalBytesStrided = 0;
  };

  // It's so important that these aren't modified once evaluated.
  const PixelPackingState state;
  const Metrics metrics;

  static Result<ExplicitPixelPackingState, std::string> ForUseWith(
      const PixelPackingState&, GLenum target, const uvec3& subrectSize,
      const webgl::PackingInfo&, const Maybe<size_t> bytesPerRowStrideOverride);
};

struct ReadPixelsDesc final {
  ivec2 srcOffset;
  uvec2 size;
  PackingInfo pi = {LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE};
  PixelPackingState packState;
};

}  // namespace webgl

namespace webgl {

struct TexUnpackBlobDesc final {
  GLenum imageTarget = LOCAL_GL_TEXTURE_2D;
  uvec3 size;
  gfxAlphaType srcAlphaType = gfxAlphaType::NonPremult;

  Maybe<RawBuffer<>> cpuData;
  Maybe<uint64_t> pboOffset;

  Maybe<uvec2> structuredSrcSize;
  RefPtr<layers::Image> image;
  Maybe<layers::SurfaceDescriptor> sd;
  RefPtr<gfx::DataSourceSurface> dataSurf;

  webgl::PixelUnpackStateWebgl unpacking;
  bool applyUnpackTransforms = true;

  // -

  auto ExplicitUnpacking(const webgl::PackingInfo& pi,
                         const Maybe<size_t> bytesPerRowStrideOverride) const {
    return ExplicitPixelPackingState::ForUseWith(this->unpacking,
                                                 this->imageTarget, this->size,
                                                 pi, bytesPerRowStrideOverride);
  }

  void Shrink(const webgl::PackingInfo&);
};

}  // namespace webgl

// ---------------------------------------
// MakeRange

template <typename T, size_t N>
inline Range<const T> MakeRange(T (&arr)[N]) {
  return {arr, N};
}

template <typename T>
inline Range<const T> MakeRange(const dom::Sequence<T>& seq) {
  return {seq.Elements(), seq.Length()};
}

template <typename T>
inline Range<const T> MakeRange(const RawBuffer<T>& from) {
  return from.Data();
}

// abv = ArrayBufferView
template <typename T>
inline auto MakeRangeAbv(const T& abv)
    -> Range<const typename T::element_type> {
  abv.ComputeState();
  return {abv.Data(), abv.Length()};
}

// -

constexpr auto kUniversalAlignment = alignof(std::max_align_t);

template <typename T>
inline size_t AlignmentOffset(const size_t alignment, const T posOrPtr) {
  MOZ_ASSERT(alignment);
  const auto begin = reinterpret_cast<uintptr_t>(posOrPtr);
  const auto wholeMultiples = (begin + (alignment - 1)) / alignment;
  const auto aligned = wholeMultiples * alignment;
  return aligned - begin;
}

template <typename T>
inline size_t ByteSize(const Range<T>& range) {
  return range.length() * sizeof(T);
}

Maybe<Range<const uint8_t>> GetRangeFromView(const dom::ArrayBufferView& view,
                                             GLuint elemOffset,
                                             GLuint elemCountOverride);

// -

template <typename T>
RawBuffer<T> RawBufferView(const Range<T>& range) {
  return RawBuffer<T>{range};
}

// -

Maybe<webgl::ErrorInfo> CheckBindBufferRange(
    const GLenum target, const GLuint index, const bool isBuffer,
    const uint64_t offset, const uint64_t size, const webgl::Limits& limits);

Maybe<webgl::ErrorInfo> CheckFramebufferAttach(const GLenum bindImageTarget,
                                               const GLenum curTexTarget,
                                               const uint32_t mipLevel,
                                               const uint32_t zLayerBase,
                                               const uint32_t zLayerCount,
                                               const webgl::Limits& limits);

Result<webgl::VertAttribPointerCalculated, webgl::ErrorInfo>
CheckVertexAttribPointer(bool isWebgl2, const webgl::VertAttribPointerDesc&);

uint8_t ElemTypeComponents(GLenum elemType);

inline std::string ToString(const nsACString& text) {
  return {text.BeginReading(), text.Length()};
}

inline void Memcpy(const RangedPtr<uint8_t>& destBytes,
                   const RangedPtr<const uint8_t>& srcBytes,
                   const size_t byteSize) {
  // Trigger range asserts
  (void)(srcBytes + byteSize);
  (void)(destBytes + byteSize);

  memcpy(destBytes.get(), srcBytes.get(), byteSize);
}

// -

namespace webgl {

// In theory, this number can be unbounded based on the driver. However, no
// driver appears to expose more than 8. We might as well stop there too, for
// now.
// (http://opengl.gpuinfo.org/gl_stats_caps_single.php?listreportsbycap=GL_MAX_COLOR_ATTACHMENTS)
inline constexpr size_t kMaxDrawBuffers = 8;
}  // namespace webgl

}  // namespace mozilla

#endif

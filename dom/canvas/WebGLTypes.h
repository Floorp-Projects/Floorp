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
#include "gfxTypes.h"

#include "nsTArray.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/ipc/SharedMemoryBasic.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLStrongTypes.h"

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

namespace ipc {
template <typename T>
struct PcqParamTraits;
}

namespace webgl {
class TexUnpackBytes;
class TexUnpackImage;
class TexUnpackSurface;
}  // namespace webgl

class ClientWebGLContext;
struct WebGLTexPboOffset;
class WebGLTexture;
class WebGLUniformLocation;
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
class WebGLUniformLocation;
class WebGLVertexArray;
class WebGLVertexArrayObject;
template <typename T>
class WebGLRefPtr;

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

// ---

/**
 * ID type used by most WebGL classes.   The Id() is unique for each Client
 * object of a given type.  The IDs start at 1 -- ID 0 is reserved for
 * null objects.  This class is subclassed by ClientWebGL... classes in the
 * client and is used directly as a key in an object ID map in the host.
 */
template <typename HostType>
class WebGLId {
 public:
  using IdType = uint64_t;

  WebGLId() : mId(0){};
  WebGLId(IdType aId) : mId(aId){};
  WebGLId(const WebGLId<HostType>* aPtr) { mId = aPtr ? aPtr->mId : 0; }
  WebGLId(const HostType* aPtr) { mId = aPtr ? aPtr->Id() : 0; }

  IdType Id() const { return mId; }

  operator bool() const { return mId != 0; }

  bool operator<(const WebGLId<HostType>& o) const { return mId < o.mId; }
  bool operator!=(const WebGLId<HostType>& o) const { return mId != o.mId; }

  static const WebGLId<HostType> Invalid() { return WebGLId<HostType>(); }

 protected:
  friend struct DefaultHasher<WebGLId<HostType>>;
  IdType mId;
};

template <typename HostType>
struct DefaultHasher<WebGLId<HostType>> {
  using Key = WebGLId<HostType>;
  using Lookup = Key;

  static HashNumber hash(const Lookup& aLookup) {
    // This discards the high 32-bits of 64-bit integers!
    return aLookup.Id();
  }

  static bool match(const Key& aKey, const Lookup& aLookup) {
    // Use builtin or overloaded operator==.
    return aKey.Id() == aLookup.Id();
  }

  static void rekey(Key& aKey, const Key& aNewKey) { aKey.mId = aNewKey.Id(); }
};

// ---

struct FloatOrInt final  // For TexParameter[fi] and friends.
{
  const bool isFloat;
  const GLfloat f;
  const GLint i;

  explicit FloatOrInt(GLint x = 0) : isFloat(false), f(x), i(x) {}

  explicit FloatOrInt(GLfloat x) : isFloat(true), f(x), i(roundf(x)) {}

  FloatOrInt& operator=(const FloatOrInt& x) {
    memcpy(this, &x, sizeof(x));
    return *this;
  }
};

struct WebGLPixelStore {
  uint32_t mUnpackImageHeight = 0;
  uint32_t mUnpackSkipImages = 0;
  uint32_t mUnpackRowLength = 0;
  uint32_t mUnpackSkipRows = 0;
  uint32_t mUnpackSkipPixels = 0;
  uint32_t mUnpackAlignment = 0;
  uint32_t mPackRowLength = 0;
  uint32_t mPackSkipRows = 0;
  uint32_t mPackSkipPixels = 0;
  uint32_t mPackAlignment = 0;
  GLenum mColorspaceConversion = 0;
  bool mFlipY = false;
  bool mPremultiplyAlpha = false;
  bool mRequireFastPath = false;
};

struct WebGLTexImageData {
  TexImageTarget mTarget;
  int32_t mRowLength;
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mDepth;
  gfxAlphaType mSrcAlphaType;
};

struct WebGLTexPboOffset {
  TexImageTarget mTarget;
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mDepth;
  WebGLsizeiptr mPboOffset;
  bool mHasExpectedImageSize;
  GLsizei mExpectedImageSize;
};

using WebGLTexUnpackVariant =
    Variant<UniquePtr<webgl::TexUnpackBytes>,
            UniquePtr<webgl::TexUnpackSurface>,
            UniquePtr<webgl::TexUnpackImage>, WebGLTexPboOffset>;

using MaybeWebGLTexUnpackVariant = Maybe<WebGLTexUnpackVariant>;

// TODO: Make this into a bit-vector instead.
struct ExtensionSets {
  nsTArray<WebGLExtensionID> mNonSystem;
  nsTArray<WebGLExtensionID> mSystem;
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
  bool shouldResistFingerprinting = true;
  bool enableDebugRendererInfo = false;

  WebGLContextOptions();
  bool operator==(const WebGLContextOptions&) const;
};

// return value for the SetDimensions message
struct SetDimensionsData {
  WebGLContextOptions mOptions;
  bool mOptionsFrozen;
  bool mResetLayer;
  bool mMaybeLostOldContext;
  nsresult mResult;
  WebGLPixelStore mPixelStore;
};

// return value for the InitializeCanvasRenderer message
struct ICRData {
  gfx::IntSize size;
  bool hasAlpha;
  bool supportsAlpha;
  bool isPremultAlpha;
};

using Int32Array2 = Array<int32_t, 2>;
using Int32Array4 = Array<int32_t, 4>;
using Uint32Array4 = Array<uint32_t, 4>;
using Float32Array2 = Array<float, 2>;
using Float32Array4 = Array<float, 4>;
using BoolArray4 = Array<bool, 4>;

// First is vertex shader, second is fragment shader.
using MaybeAttachedShaders = Maybe<Array<WebGLId<WebGLShader>, 2>>;

using WebGLVariant =
    Variant<int32_t, uint32_t, int64_t, uint64_t, bool, float, double,
            nsCString, nsString, WebGLId<WebGLBuffer>,
            WebGLId<WebGLFramebuffer>, WebGLId<WebGLProgram>,
            WebGLId<WebGLQuery>, WebGLId<WebGLRenderbuffer>,
            WebGLId<WebGLSampler>, WebGLId<WebGLShader>, WebGLId<WebGLSync>,
            WebGLId<WebGLTexture>, WebGLId<WebGLTransformFeedback>,
            WebGLId<WebGLUniformLocation>, WebGLId<WebGLVertexArray>,
            WebGLShaderPrecisionFormat, Int32Array2, Int32Array4, Uint32Array4,
            Float32Array2, Float32Array4, BoolArray4, nsTArray<uint32_t>,
            nsTArray<int32_t>, nsTArray<bool>, nsTArray<float>>;

using MaybeWebGLVariant = Maybe<WebGLVariant>;

template <typename T>
class AsSomeVariantT {
  Maybe<T> mMaybeObj;

 public:
  AsSomeVariantT(Maybe<T>&& aObj) : mMaybeObj(std::move(aObj)) {}

  template <typename... As>
  operator Maybe<Variant<As...>>() {
    if (mMaybeObj.isNothing()) {
      return Nothing();
    }
    return Some(Variant<As...>(std::forward<T>(mMaybeObj.ref())));
  }
};

template <typename FullType,
          typename T = typename RemoveReference<FullType>::Type>
AsSomeVariantT<T> AsSomeVariant(FullType&& aObj) {
  return AsSomeVariantT<T>(Some(std::forward<T>(aObj)));
}

// Stack-based arrays can't be moved
template <typename T, size_t N>
AsSomeVariantT<Array<T, N>> AsSomeVariant(const Array<T, N>& aObj) {
  return AsSomeVariantT<Array<T, N>>(Some(aObj));
}

template <typename T>
AsSomeVariantT<WebGLId<T>> AsSomeVariant(WebGLId<T>* aObj) {
  return AsSomeVariantT<WebGLId<T>>(aObj ? Some(*aObj) : Nothing());
}

template <typename T>
AsSomeVariantT<WebGLId<T>> AsSomeVariant(T* aObj) {
  return AsSomeVariant(static_cast<WebGLId<T>*>(aObj));
}

template <typename T>
AsSomeVariantT<WebGLId<T>> AsSomeVariant(WebGLRefPtr<T> aObj) {
  return AsSomeVariant(static_cast<WebGLId<T>*>(aObj.get()));
}

template <typename T>
AsSomeVariantT<T> AsSomeVariant(const Maybe<T>& aObj) {
  return AsSomeVariantT<T>(aObj);
}

template <typename T>
AsSomeVariantT<T> AsSomeVariant(Maybe<T>&& aObj) {
  return AsSomeVariantT<T>(std::move(aObj));
}

/**
 * Represents a block of memory that it may or may not own.  The
 * inner data type must be trivially copyable by memcpy.  A RawBuffer
 * may be backed by local memory or shared memory.
 */
template <typename T = uint8_t, typename nonCV = typename RemoveCV<T>::Type,
          typename EnableIf<std::is_trivially_assignable<nonCV&, nonCV>::value,
                            int>::Type = 0>
class RawBuffer {
  // The SharedMemoryBasic that owns mData, if any.
  RefPtr<mozilla::ipc::SharedMemoryBasic> mSmem;
  // Pointer to the raw memory block
  T* mData = nullptr;
  // Length is the number of elements of size T in the array
  size_t mLength = 0;
  // true if we should delete[] the mData on destruction
  bool mOwnsData = false;

  friend mozilla::ipc::PcqParamTraits<RawBuffer>;

 public:
  using ElementType = T;

  /**
   * If aTakeData is true, RawBuffer will delete[] the memory when destroyed.
   */
  RawBuffer(size_t len, T* data, bool aTakeData = false)
      : mData(data), mLength(len), mOwnsData(aTakeData) {
    MOZ_ASSERT(mData && mLength);
  }

  RawBuffer(size_t len, RefPtr<mozilla::ipc::SharedMemoryBasic>& aSmem)
      : mSmem(aSmem), mData(aSmem->memory()), mLength(len), mOwnsData(false) {
    MOZ_ASSERT(mData && mLength);
  }

  ~RawBuffer() {
    // If we have a SharedMemoryBasic then it must own mData.
    MOZ_ASSERT((!mSmem) || (!mOwnsData));
    if (mOwnsData) {
      delete[] mData;
    }
    if (mSmem) {
      mSmem->CloseHandle();
    }
  }

  uint32_t Length() const { return mLength; }

  T* Data() { return mData; }
  const T* Data() const { return mData; }

  T& operator[](size_t idx) {
    MOZ_ASSERT(mData && (idx < mLength));
    return mData[idx];
  }
  const T& operator[](size_t idx) const {
    MOZ_ASSERT(mData && (idx < mLength));
    return mData[idx];
  }

  operator bool() const { return mData && mLength; }

  RawBuffer() {}
  RawBuffer(const RawBuffer&) = delete;
  RawBuffer& operator=(const RawBuffer&) = delete;

  RawBuffer(RawBuffer&& o)
      : mSmem(o.mSmem),
        mData(o.mData),
        mLength(o.mLength),
        mOwnsData(o.mOwnsData) {
    o.mSmem = nullptr;
    o.mData = nullptr;
    o.mLength = 0;
    o.mOwnsData = false;
  }

  RawBuffer& operator=(RawBuffer&& o) {
    mSmem = o.mSmem;
    mData = o.mData;
    mLength = o.mLength;
    mOwnsData = o.mOwnsData;
    o.mSmem = nullptr;
    o.mData = nullptr;
    o.mLength = 0;
    o.mOwnsData = false;
    return *this;
  }
};

}  // namespace mozilla

#endif

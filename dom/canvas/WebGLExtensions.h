/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_EXTENSIONS_H_
#define WEBGL_EXTENSIONS_H_

#include "mozilla/AlreadyAddRefed.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "WebGLObjectModel.h"
#include "WebGLTypes.h"
#include "WebGLFormats.h"

namespace mozilla {
class ErrorResult;

namespace dom {
template <typename>
struct Nullable;
template <typename>
class Sequence;
}  // namespace dom

namespace webgl {
class FormatUsageAuthority;
}  // namespace webgl

class WebGLContext;
class WebGLQuery;
class WebGLShader;
class WebGLTexture;
class WebGLVertexArray;

class WebGLExtensionBase : public WebGLContextBoundObject<WebGLExtensionBase> {
 public:
  explicit WebGLExtensionBase(WebGLContext* webgl);

  void MarkLost();

  void SetExplicit() {
    mIsExplicit = true;
    OnSetExplicit();
  }

  bool IsExplicit() { return mIsExplicit; }

  NS_INLINE_DECL_REFCOUNTING(WebGLExtensionBase)

 protected:
  virtual ~WebGLExtensionBase();

  virtual void OnMarkLost() {}
  virtual void OnSetExplicit() {}

  bool mIsLost = false;
  bool mIsExplicit = false;
};

////

class WebGLExtensionCompressedTextureASTC : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureASTC(WebGLContext* webgl);
  virtual ~WebGLExtensionCompressedTextureASTC();

  Maybe<nsTArray<nsString>> GetSupportedProfiles() const;

  static bool IsSupported(const WebGLContext* webgl);
};

class WebGLExtensionCompressedTextureBPTC final : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureBPTC(WebGLContext* webgl);

  static bool IsSupported(const WebGLContext* webgl);
};

class WebGLExtensionCompressedTextureES3 : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureES3(WebGLContext*);
  virtual ~WebGLExtensionCompressedTextureES3();
};

class WebGLExtensionCompressedTextureETC1 : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureETC1(WebGLContext*);
  virtual ~WebGLExtensionCompressedTextureETC1();
};

class WebGLExtensionCompressedTexturePVRTC : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTexturePVRTC(WebGLContext*);
  virtual ~WebGLExtensionCompressedTexturePVRTC();
};

class WebGLExtensionCompressedTextureRGTC final : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureRGTC(WebGLContext* webgl);

  static bool IsSupported(const WebGLContext* webgl);
};

class WebGLExtensionCompressedTextureS3TC : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureS3TC(WebGLContext*);
  virtual ~WebGLExtensionCompressedTextureS3TC();

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionCompressedTextureS3TC_SRGB : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureS3TC_SRGB(WebGLContext*);
  virtual ~WebGLExtensionCompressedTextureS3TC_SRGB();

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionDebugRendererInfo : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDebugRendererInfo(WebGLContext*);
  virtual ~WebGLExtensionDebugRendererInfo();
};

class WebGLExtensionDebugShaders : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDebugShaders(WebGLContext*);
  virtual ~WebGLExtensionDebugShaders();

  nsString GetTranslatedShaderSource(const WebGLShader& shader) const;
};

class WebGLExtensionDepthTexture : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDepthTexture(WebGLContext*);
  virtual ~WebGLExtensionDepthTexture();

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionElementIndexUint : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionElementIndexUint(WebGLContext*);
  virtual ~WebGLExtensionElementIndexUint();
};

class WebGLExtensionExplicitPresent : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionExplicitPresent(WebGLContext*);

  static bool IsSupported(const WebGLContext*);

  void Present() const;
};

class WebGLExtensionEXTColorBufferFloat : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionEXTColorBufferFloat(WebGLContext*);
  virtual ~WebGLExtensionEXTColorBufferFloat() {}

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionFBORenderMipmap : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionFBORenderMipmap(WebGLContext* webgl);
  virtual ~WebGLExtensionFBORenderMipmap();

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionFloatBlend : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionFloatBlend(WebGLContext* webgl);
  virtual ~WebGLExtensionFloatBlend();

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionFragDepth : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionFragDepth(WebGLContext*);
  virtual ~WebGLExtensionFragDepth();

  static bool IsSupported(const WebGLContext* context);
};

class WebGLExtensionLoseContext : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionLoseContext(WebGLContext*);
  virtual ~WebGLExtensionLoseContext();

  void LoseContext();
  void RestoreContext();
};

class WebGLExtensionSRGB : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionSRGB(WebGLContext*);
  virtual ~WebGLExtensionSRGB();

  static bool IsSupported(const WebGLContext* context);
};

class WebGLExtensionStandardDerivatives : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionStandardDerivatives(WebGLContext*);
  virtual ~WebGLExtensionStandardDerivatives();
};

class WebGLExtensionShaderTextureLod : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionShaderTextureLod(WebGLContext*);
  virtual ~WebGLExtensionShaderTextureLod();

  static bool IsSupported(const WebGLContext* context);
};

class WebGLExtensionTextureFilterAnisotropic : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionTextureFilterAnisotropic(WebGLContext*);
  virtual ~WebGLExtensionTextureFilterAnisotropic();
};

class WebGLExtensionTextureFloat : public WebGLExtensionBase {
 public:
  static void InitWebGLFormats(webgl::FormatUsageAuthority* authority);

  explicit WebGLExtensionTextureFloat(WebGLContext*);
  virtual ~WebGLExtensionTextureFloat();

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionTextureFloatLinear : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionTextureFloatLinear(WebGLContext*);
  virtual ~WebGLExtensionTextureFloatLinear();
};

class WebGLExtensionTextureHalfFloat : public WebGLExtensionBase {
 public:
  static void InitWebGLFormats(webgl::FormatUsageAuthority* authority);

  explicit WebGLExtensionTextureHalfFloat(WebGLContext*);
  virtual ~WebGLExtensionTextureHalfFloat();

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionTextureHalfFloatLinear : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionTextureHalfFloatLinear(WebGLContext*);
  virtual ~WebGLExtensionTextureHalfFloatLinear();
};

class WebGLExtensionColorBufferFloat : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionColorBufferFloat(WebGLContext*);
  virtual ~WebGLExtensionColorBufferFloat();

  static bool IsSupported(const WebGLContext*);

  void SetRenderable(const webgl::FormatRenderableState);
  void OnSetExplicit() override;
};

class WebGLExtensionColorBufferHalfFloat : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionColorBufferHalfFloat(WebGLContext*);
  virtual ~WebGLExtensionColorBufferHalfFloat();

  static bool IsSupported(const WebGLContext*);

  void SetRenderable(const webgl::FormatRenderableState);
  void OnSetExplicit() override;
};

class WebGLExtensionDrawBuffers : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDrawBuffers(WebGLContext*);
  virtual ~WebGLExtensionDrawBuffers();

  void DrawBuffersWEBGL(const nsTArray<GLenum>& buffers);

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionVertexArray : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionVertexArray(WebGLContext* webgl);
  virtual ~WebGLExtensionVertexArray();

  already_AddRefed<WebGLVertexArray> CreateVertexArrayOES();
  void DeleteVertexArrayOES(WebGLVertexArray* array);
  bool IsVertexArrayOES(const WebGLVertexArray* array);
  void BindVertexArrayOES(WebGLVertexArray* array);
};

class WebGLExtensionInstancedArrays : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionInstancedArrays(WebGLContext* webgl);
  virtual ~WebGLExtensionInstancedArrays();

  void DrawArraysInstancedANGLE(GLenum mode, GLint first, GLsizei count,
                                GLsizei primcount);
  void DrawElementsInstancedANGLE(GLenum mode, GLsizei count, GLenum type,
                                  WebGLintptr offset, GLsizei primcount);
  void VertexAttribDivisorANGLE(GLuint index, GLuint divisor);

  static bool IsSupported(const WebGLContext* webgl);
};

class WebGLExtensionBlendMinMax : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionBlendMinMax(WebGLContext* webgl);
  virtual ~WebGLExtensionBlendMinMax();

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionDisjointTimerQuery : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDisjointTimerQuery(WebGLContext* webgl);
  virtual ~WebGLExtensionDisjointTimerQuery();

  already_AddRefed<WebGLQuery> CreateQueryEXT() const;
  void DeleteQueryEXT(WebGLQuery* query) const;
  bool IsQueryEXT(const WebGLQuery* query) const;
  void BeginQueryEXT(GLenum target, WebGLQuery& query) const;
  void EndQueryEXT(GLenum target) const;
  void QueryCounterEXT(WebGLQuery& query, GLenum target) const;
  MaybeWebGLVariant GetQueryEXT(GLenum target, GLenum pname) const;
  MaybeWebGLVariant GetQueryObjectEXT(const WebGLQuery& query,
                                      GLenum pname) const;

  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionMOZDebug final : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionMOZDebug(WebGLContext* webgl);
  virtual ~WebGLExtensionMOZDebug();

  MaybeWebGLVariant GetParameter(GLenum pname) const;
};

template <WebGLExtensionID ext>
struct WebGLExtensionClassMap;

#define DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(_extensionID, _extensionClass) \
  template <>                                                                 \
  struct WebGLExtensionClassMap<WebGLExtensionID::_extensionID> {             \
    using Type = _extensionClass;                                             \
  };

DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(ANGLE_instanced_arrays,
                                       WebGLExtensionInstancedArrays)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_blend_minmax,
                                       WebGLExtensionBlendMinMax)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_color_buffer_float,
                                       WebGLExtensionColorBufferFloat)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_color_buffer_half_float,
                                       WebGLExtensionColorBufferHalfFloat)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_texture_compression_bptc,
                                       WebGLExtensionCompressedTextureBPTC)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_texture_compression_rgtc,
                                       WebGLExtensionCompressedTextureRGTC)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_frag_depth, WebGLExtensionFragDepth)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_sRGB, WebGLExtensionSRGB)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_shader_texture_lod,
                                       WebGLExtensionShaderTextureLod)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_texture_filter_anisotropic,
                                       WebGLExtensionTextureFilterAnisotropic)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(EXT_disjoint_timer_query,
                                       WebGLExtensionDisjointTimerQuery)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(MOZ_debug, WebGLExtensionMOZDebug)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(OES_element_index_uint,
                                       WebGLExtensionElementIndexUint)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(OES_standard_derivatives,
                                       WebGLExtensionStandardDerivatives)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(OES_texture_float,
                                       WebGLExtensionTextureFloat)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(OES_texture_float_linear,
                                       WebGLExtensionTextureFloatLinear)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(OES_texture_half_float,
                                       WebGLExtensionTextureHalfFloat)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(OES_texture_half_float_linear,
                                       WebGLExtensionTextureHalfFloatLinear)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(OES_vertex_array_object,
                                       WebGLExtensionVertexArray)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_color_buffer_float,
                                       WebGLExtensionEXTColorBufferFloat)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_compressed_texture_astc,
                                       WebGLExtensionCompressedTextureASTC)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_compressed_texture_etc,
                                       WebGLExtensionCompressedTextureES3)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_compressed_texture_etc1,
                                       WebGLExtensionCompressedTextureETC1)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_compressed_texture_pvrtc,
                                       WebGLExtensionCompressedTexturePVRTC)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_compressed_texture_s3tc,
                                       WebGLExtensionCompressedTextureS3TC)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_compressed_texture_s3tc_srgb,
                                       WebGLExtensionCompressedTextureS3TC_SRGB)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_debug_renderer_info,
                                       WebGLExtensionDebugRendererInfo)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_debug_shaders,
                                       WebGLExtensionDebugShaders)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_depth_texture,
                                       WebGLExtensionDepthTexture)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_draw_buffers,
                                       WebGLExtensionDrawBuffers)
DEFINE_WEBGL_EXTENSION_CLASS_MAP_ENTRY(WEBGL_lose_context,
                                       WebGLExtensionLoseContext)

class WebGLExtensionMultiview : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionMultiview(WebGLContext*);
  virtual ~WebGLExtensionMultiview();
  static bool IsSupported(const WebGLContext*);

  void FramebufferTextureMultiviewOVR(GLenum target, GLenum attachment,
                                      WebGLTexture* texture, GLint level,
                                      GLint baseViewIndex,
                                      GLsizei numViews) const;
};

}  // namespace mozilla

#endif  // WEBGL_EXTENSIONS_H_

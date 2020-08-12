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

class WebGLExtensionBase {
 protected:
  WebGLContext* const mContext;

 private:
  bool mIsExplicit = false;

 protected:
  explicit WebGLExtensionBase(WebGLContext* context) : mContext(context) {}

 public:
  virtual ~WebGLExtensionBase() = default;

 private:
  virtual void OnSetExplicit() {}

 public:
  void SetExplicit() {
    mIsExplicit = true;
    OnSetExplicit();
  }

  bool IsExplicit() const { return mIsExplicit; }
};

////

class WebGLExtensionCompressedTextureASTC : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureASTC(WebGLContext* webgl);
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
};

class WebGLExtensionCompressedTextureETC1 : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureETC1(WebGLContext*);
};

class WebGLExtensionCompressedTexturePVRTC : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTexturePVRTC(WebGLContext*);
};

class WebGLExtensionCompressedTextureRGTC final : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureRGTC(WebGLContext* webgl);
  static bool IsSupported(const WebGLContext* webgl);
};

class WebGLExtensionCompressedTextureS3TC : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureS3TC(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionCompressedTextureS3TC_SRGB : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionCompressedTextureS3TC_SRGB(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionDebugRendererInfo : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDebugRendererInfo(WebGLContext* webgl)
      : WebGLExtensionBase(webgl) {}
};

class WebGLExtensionDebugShaders : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDebugShaders(WebGLContext* webgl)
      : WebGLExtensionBase(webgl) {}
};

class WebGLExtensionDepthTexture : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDepthTexture(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionElementIndexUint : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionElementIndexUint(WebGLContext* webgl)
      : WebGLExtensionBase(webgl) {}
};

class WebGLExtensionExplicitPresent : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionExplicitPresent(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionEXTColorBufferFloat : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionEXTColorBufferFloat(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionFBORenderMipmap : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionFBORenderMipmap(WebGLContext* webgl);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionFloatBlend : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionFloatBlend(WebGLContext* webgl);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionFragDepth : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionFragDepth(WebGLContext*);
  static bool IsSupported(const WebGLContext* context);
};

class WebGLExtensionLoseContext : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionLoseContext(WebGLContext* webgl)
      : WebGLExtensionBase(webgl) {}
};

class WebGLExtensionMultiview : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionMultiview(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionSRGB : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionSRGB(WebGLContext*);
  static bool IsSupported(const WebGLContext* context);
};

class WebGLExtensionStandardDerivatives : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionStandardDerivatives(WebGLContext* webgl)
      : WebGLExtensionBase(webgl) {}
};

class WebGLExtensionShaderTextureLod : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionShaderTextureLod(WebGLContext*);
  static bool IsSupported(const WebGLContext* context);
};

class WebGLExtensionTextureFilterAnisotropic : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionTextureFilterAnisotropic(WebGLContext* webgl)
      : WebGLExtensionBase(webgl) {}
};

class WebGLExtensionTextureFloat : public WebGLExtensionBase {
 public:
  static void InitWebGLFormats(webgl::FormatUsageAuthority* authority);

  explicit WebGLExtensionTextureFloat(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionTextureFloatLinear : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionTextureFloatLinear(WebGLContext*);
};

class WebGLExtensionTextureHalfFloat : public WebGLExtensionBase {
 public:
  static void InitWebGLFormats(webgl::FormatUsageAuthority* authority);

  explicit WebGLExtensionTextureHalfFloat(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionTextureHalfFloatLinear : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionTextureHalfFloatLinear(WebGLContext*);
};

class WebGLExtensionTextureNorm16 : public WebGLExtensionBase {
 public:
  static bool IsSupported(const WebGLContext*);
  explicit WebGLExtensionTextureNorm16(WebGLContext*);
};

class WebGLExtensionColorBufferFloat : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionColorBufferFloat(WebGLContext*);
  static bool IsSupported(const WebGLContext*);

  void SetRenderable(const webgl::FormatRenderableState);
  void OnSetExplicit() override;
};

class WebGLExtensionColorBufferHalfFloat : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionColorBufferHalfFloat(WebGLContext*);
  static bool IsSupported(const WebGLContext*);

  void SetRenderable(const webgl::FormatRenderableState);
  void OnSetExplicit() override;
};

class WebGLExtensionDrawBuffers : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDrawBuffers(WebGLContext*);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionVertexArray : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionVertexArray(WebGLContext* webgl)
      : WebGLExtensionBase(webgl) {}
};

class WebGLExtensionInstancedArrays : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionInstancedArrays(WebGLContext* webgl);
  static bool IsSupported(const WebGLContext* webgl);
};

class WebGLExtensionBlendMinMax : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionBlendMinMax(WebGLContext* webgl);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionDisjointTimerQuery : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionDisjointTimerQuery(WebGLContext* webgl);
  static bool IsSupported(const WebGLContext*);
};

class WebGLExtensionMOZDebug final : public WebGLExtensionBase {
 public:
  explicit WebGLExtensionMOZDebug(WebGLContext* webgl)
      : WebGLExtensionBase(webgl) {}
};

}  // namespace mozilla

#endif  // WEBGL_EXTENSIONS_H_

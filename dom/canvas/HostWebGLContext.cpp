/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HostWebGLContext.h"

#include "CompositableHost.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/TextureClientSharedSurface.h"

#include "TexUnpackBlob.h"
#include "WebGL1Context.h"
#include "WebGL2Context.h"
#include "WebGLBuffer.h"
#include "WebGLCrossProcessCommandQueue.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLSync.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"
#include "WebGLUniformLocation.h"
#include "WebGLQuery.h"

namespace mozilla {

LazyLogModule gWebGLBridgeLog("webglbridge");

#define DEFINE_OBJECT_ID_MAP_FUNCS(_WebGLType)                                 \
  WebGLId<WebGL##_WebGLType> HostWebGLContext::Insert(                         \
      RefPtr<WebGL##_WebGLType>&& aObj, const WebGLId<WebGL##_WebGLType>& aId) \
      const {                                                                  \
    return m##_WebGLType##Map.Insert(std::move(aObj), aId);                    \
  }                                                                            \
  WebGL##_WebGLType* HostWebGLContext::Find(                                   \
      const WebGLId<WebGL##_WebGLType>& aId) const {                           \
    return m##_WebGLType##Map.Find(aId);                                       \
  }                                                                            \
  void HostWebGLContext::Remove(const WebGLId<WebGL##_WebGLType>& aId) const { \
    return m##_WebGLType##Map.Remove(aId);                                     \
  }

DEFINE_OBJECT_ID_MAP_FUNCS(Framebuffer);
DEFINE_OBJECT_ID_MAP_FUNCS(Program);
DEFINE_OBJECT_ID_MAP_FUNCS(Query);
DEFINE_OBJECT_ID_MAP_FUNCS(Renderbuffer);
DEFINE_OBJECT_ID_MAP_FUNCS(Sampler);
DEFINE_OBJECT_ID_MAP_FUNCS(Shader);
DEFINE_OBJECT_ID_MAP_FUNCS(Sync);
DEFINE_OBJECT_ID_MAP_FUNCS(TransformFeedback);
DEFINE_OBJECT_ID_MAP_FUNCS(VertexArray);
DEFINE_OBJECT_ID_MAP_FUNCS(Buffer);
DEFINE_OBJECT_ID_MAP_FUNCS(Texture);
DEFINE_OBJECT_ID_MAP_FUNCS(UniformLocation);

/* static */ WebGLContext* HostWebGLContext::MakeWebGLContext(
    WebGLVersion aVersion) {
  switch (aVersion) {
    case WEBGL1:
      return WebGL1Context::Create();
    case WEBGL2:
      return WebGL2Context::Create();
    default:
      MOZ_ASSERT_UNREACHABLE("Illegal WebGLVersion");
      return nullptr;
  }
}

HostWebGLContext::HostWebGLContext(
    WebGLVersion aVersion, RefPtr<WebGLContext> aContext,
    UniquePtr<HostWebGLCommandSink>&& aCommandSink,
    UniquePtr<HostWebGLErrorSource>&& aErrorSource)
    : WebGLContextEndpoint(aVersion),
      mCommandSink(std::move(aCommandSink)),
      mErrorSource(std::move(aErrorSource)),
      mContext(aContext),
      mClientContext(nullptr) {
  mContext->SetHost(this);
  if (mCommandSink) {
    mCommandSink->SetHostContext(this);
  }
}

HostWebGLContext::~HostWebGLContext() {
  if (mContext) {
    mContext->SetHost(nullptr);
  }
}

/* static */ UniquePtr<HostWebGLContext> HostWebGLContext::Create(
    WebGLVersion aVersion, UniquePtr<HostWebGLCommandSink>&& aCommandSink,
    UniquePtr<HostWebGLErrorSource>&& aErrorSource) {
  WebGLContext* context = MakeWebGLContext(aVersion);
  if (!context) {
    return nullptr;
  }
  return WrapUnique(new HostWebGLContext(
      aVersion, context, std::move(aCommandSink), std::move(aErrorSource)));
}

CommandResult HostWebGLContext::RunCommandsForDuration(TimeDuration aDuration) {
  return mCommandSink->ProcessUpToDuration(aDuration);
}

void HostWebGLContext::SetCompositableHost(
    RefPtr<CompositableHost>& aCompositableHost) {
  mContext->SetCompositableHost(aCompositableHost);
}

void HostWebGLContext::Present() { mContext->Present(); }

void HostWebGLContext::CreateFramebuffer(const WebGLId<WebGLFramebuffer>& aId) {
  Insert(mContext->CreateFramebuffer(), aId);
}

void HostWebGLContext::CreateProgram(const WebGLId<WebGLProgram>& aId) {
  Insert(mContext->CreateProgram(), aId);
}

void HostWebGLContext::CreateRenderbuffer(
    const WebGLId<WebGLRenderbuffer>& aId) {
  Insert(mContext->CreateRenderbuffer(), aId);
}

void HostWebGLContext::CreateShader(GLenum aType,
                                    const WebGLId<WebGLShader>& aId) {
  Insert(mContext->CreateShader(aType), aId);
}

WebGLId<WebGLUniformLocation> HostWebGLContext::GetUniformLocation(
    const WebGLId<WebGLProgram>& progId, const nsString& name) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return Insert(
      RefPtr<WebGLUniformLocation>(mContext->GetUniformLocation(*prog, name)));
}

WebGLId<WebGLBuffer> HostWebGLContext::CreateBuffer() {
  return Insert(RefPtr<WebGLBuffer>(mContext->CreateBuffer()));
}

WebGLId<WebGLTexture> HostWebGLContext::CreateTexture() {
  return Insert(RefPtr<WebGLTexture>(mContext->CreateTexture()));
}

void HostWebGLContext::CreateSampler(const WebGLId<WebGLSampler>& aId) {
  Insert(GetWebGL2Context()->CreateSampler(), aId);
}

WebGLId<WebGLSync> HostWebGLContext::FenceSync(const WebGLId<WebGLSync>& aId,
                                               GLenum condition,
                                               GLbitfield flags) {
  return Insert(GetWebGL2Context()->FenceSync(condition, flags), aId);
}

void HostWebGLContext::CreateTransformFeedback(
    const WebGLId<WebGLTransformFeedback>& aId) {
  Insert(GetWebGL2Context()->CreateTransformFeedback(), aId);
}

void HostWebGLContext::CreateVertexArray(const WebGLId<WebGLVertexArray>& aId,
                                         bool aFromExtension) {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::OES_vertex_array_object>();
    MOZ_RELEASE_ASSERT(ext);
    Insert(ext->CreateVertexArrayOES(), aId);
    return;
  }

  Insert(mContext->CreateVertexArray(), aId);
}

void HostWebGLContext::CreateQuery(const WebGLId<WebGLQuery>& aId,
                                   bool aFromExtension) const {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::EXT_disjoint_timer_query>();
    MOZ_RELEASE_ASSERT(ext);
    Insert(ext->CreateQueryEXT(), aId);
    return;
  }

  Insert(const_cast<WebGL2Context*>(GetWebGL2Context())->CreateQuery(), aId);
}

// ------------------------- Composition -------------------------
Maybe<ICRData> HostWebGLContext::InitializeCanvasRenderer(
    layers::LayersBackend backend) {
  return mContext->InitializeCanvasRenderer(backend);
}

void HostWebGLContext::SetContextOptions(const WebGLContextOptions& options) {
  mContext->SetOptions(options);
}

SetDimensionsData HostWebGLContext::SetDimensions(int32_t signedWidth,
                                                  int32_t signedHeight) {
  return mContext->SetDimensions(signedWidth, signedHeight);
}

gfx::IntSize HostWebGLContext::DrawingBufferSize() {
  return mContext->DrawingBufferSize();
}

void HostWebGLContext::OnMemoryPressure() {
  return mContext->OnMemoryPressure();
}

void HostWebGLContext::AllowContextRestore() {
  mContext->AllowContextRestore();
}

void HostWebGLContext::DidRefresh() { mContext->DidRefresh(); }

// ------------------------- GL State -------------------------
bool HostWebGLContext::IsContextLost() const {
  return mContext->IsContextLost();
}

void HostWebGLContext::Disable(GLenum cap) { mContext->Disable(cap); }

void HostWebGLContext::Enable(GLenum cap) { mContext->Enable(cap); }

bool HostWebGLContext::IsEnabled(GLenum cap) {
  return mContext->IsEnabled(cap);
}

MaybeWebGLVariant HostWebGLContext::GetParameter(GLenum pname) {
  return mContext->GetParameter(pname);
}

void HostWebGLContext::AttachShader(const WebGLId<WebGLProgram>& progId,
                                    const WebGLId<WebGLShader>& shaderId) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  RefPtr<WebGLShader> shader = MustFind(shaderId);
  mContext->AttachShader(*prog, *shader);
}

void HostWebGLContext::BindAttribLocation(const WebGLId<WebGLProgram>& progId,
                                          GLuint location,
                                          const nsString& name) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  mContext->BindAttribLocation(*prog, location, name);
}

void HostWebGLContext::BindFramebuffer(GLenum target,
                                       const WebGLId<WebGLFramebuffer>& fb) {
  mContext->BindFramebuffer(target, Find(fb));
}

void HostWebGLContext::BindRenderbuffer(GLenum target,
                                        const WebGLId<WebGLRenderbuffer>& fb) {
  mContext->BindRenderbuffer(target, Find(fb));
}

void HostWebGLContext::BlendColor(GLclampf r, GLclampf g, GLclampf b,
                                  GLclampf a) {
  mContext->BlendColor(r, g, b, a);
}

void HostWebGLContext::BlendEquation(GLenum mode) {
  mContext->BlendEquation(mode);
}

void HostWebGLContext::BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  mContext->BlendEquationSeparate(modeRGB, modeAlpha);
}

void HostWebGLContext::BlendFunc(GLenum sfactor, GLenum dfactor) {
  mContext->BlendFunc(sfactor, dfactor);
}

void HostWebGLContext::BlendFuncSeparate(GLenum srcRGB, GLenum dstRGB,
                                         GLenum srcAlpha, GLenum dstAlpha) {
  mContext->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

GLenum HostWebGLContext::CheckFramebufferStatus(GLenum target) {
  return mContext->CheckFramebufferStatus(target);
}

void HostWebGLContext::Clear(GLbitfield mask) { mContext->Clear(mask); }

void HostWebGLContext::ClearColor(GLclampf r, GLclampf g, GLclampf b,
                                  GLclampf a) {
  mContext->ClearColor(r, g, b, a);
}

void HostWebGLContext::ClearDepth(GLclampf v) { mContext->ClearDepth(v); }

void HostWebGLContext::ClearStencil(GLint v) { mContext->ClearStencil(v); }

void HostWebGLContext::ColorMask(WebGLboolean r, WebGLboolean g, WebGLboolean b,
                                 WebGLboolean a) {
  mContext->ColorMask(r, g, b, a);
}

void HostWebGLContext::CompileShader(const WebGLId<WebGLShader>& shaderId) {
  RefPtr<WebGLShader> shader = MustFind(shaderId);
  mContext->CompileShader(*shader);
}

void HostWebGLContext::CullFace(GLenum face) { mContext->CullFace(face); }

void HostWebGLContext::DeleteFramebuffer(const WebGLId<WebGLFramebuffer>& fb) {
  mContext->DeleteFramebuffer(Find(fb));
}

void HostWebGLContext::DeleteProgram(const WebGLId<WebGLProgram>& prog) {
  mContext->DeleteProgram(Find(prog));
}

void HostWebGLContext::DeleteRenderbuffer(
    const WebGLId<WebGLRenderbuffer>& rb) {
  mContext->DeleteRenderbuffer(Find(rb));
}

void HostWebGLContext::DeleteShader(const WebGLId<WebGLShader>& shader) {
  mContext->DeleteShader(Find(shader));
}

void HostWebGLContext::DepthFunc(GLenum func) { mContext->DepthFunc(func); }

void HostWebGLContext::DepthMask(WebGLboolean b) { mContext->DepthMask(b); }

void HostWebGLContext::DepthRange(GLclampf zNear, GLclampf zFar) {
  mContext->DepthRange(zNear, zFar);
}

void HostWebGLContext::DetachShader(const WebGLId<WebGLProgram>& progId,
                                    const WebGLId<WebGLShader>& shaderId) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  RefPtr<WebGLShader> shader = MustFind(shaderId);
  mContext->DetachShader(*prog, *shader);
}

void HostWebGLContext::Flush() { mContext->Flush(); }

void HostWebGLContext::Finish() { mContext->Finish(); }

void HostWebGLContext::FramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum rbTarget,
    const WebGLId<WebGLRenderbuffer>& rb) {
  mContext->FramebufferRenderbuffer(target, attachment, rbTarget, Find(rb));
}

void HostWebGLContext::FramebufferTexture2D(GLenum target, GLenum attachment,
                                            GLenum texImageTarget,
                                            const WebGLId<WebGLTexture>& tex,
                                            GLint level) {
  mContext->FramebufferTexture2D(target, attachment, texImageTarget, Find(tex),
                                 level);
}

void HostWebGLContext::FrontFace(GLenum mode) { mContext->FrontFace(mode); }

Maybe<WebGLActiveInfo> HostWebGLContext::GetActiveAttrib(
    const WebGLId<WebGLProgram>& progId, GLuint index) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return mContext->GetActiveAttrib(*prog, index);
}

Maybe<WebGLActiveInfo> HostWebGLContext::GetActiveUniform(
    const WebGLId<WebGLProgram>& progId, GLuint index) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return mContext->GetActiveUniform(*prog, index);
}

MaybeAttachedShaders HostWebGLContext::GetAttachedShaders(
    const WebGLId<WebGLProgram>& progId) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return mContext->GetAttachedShaders(*prog);
}

GLint HostWebGLContext::GetAttribLocation(const WebGLId<WebGLProgram>& progId,
                                          const nsString& name) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return mContext->GetAttribLocation(*prog, name);
}

MaybeWebGLVariant HostWebGLContext::GetBufferParameter(GLenum target,
                                                       GLenum pname) {
  return mContext->GetBufferParameter(target, pname);
}

GLenum HostWebGLContext::GetError() { return mContext->GetError(); }

MaybeWebGLVariant HostWebGLContext::GetFramebufferAttachmentParameter(
    GLenum target, GLenum attachment, GLenum pname) {
  return mContext->GetFramebufferAttachmentParameter(target, attachment, pname);
}

MaybeWebGLVariant HostWebGLContext::GetProgramParameter(
    const WebGLId<WebGLProgram>& progId, GLenum pname) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return mContext->GetProgramParameter(*prog, pname);
}

nsString HostWebGLContext::GetProgramInfoLog(
    const WebGLId<WebGLProgram>& progId) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return mContext->GetProgramInfoLog(*prog);
}

MaybeWebGLVariant HostWebGLContext::GetRenderbufferParameter(GLenum target,
                                                             GLenum pname) {
  return mContext->GetRenderbufferParameter(target, pname);
}

MaybeWebGLVariant HostWebGLContext::GetShaderParameter(
    const WebGLId<WebGLShader>& shaderId, GLenum pname) {
  RefPtr<WebGLShader> shader = MustFind(shaderId);
  return mContext->GetShaderParameter(*shader, pname);
}

MaybeWebGLVariant HostWebGLContext::GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype) {
  return AsSomeVariant(
      mContext->GetShaderPrecisionFormat(shadertype, precisiontype));
}

nsString HostWebGLContext::GetShaderInfoLog(
    const WebGLId<WebGLShader>& shaderId) {
  RefPtr<WebGLShader> shader = MustFind(shaderId);
  return mContext->GetShaderInfoLog(*shader);
}

nsString HostWebGLContext::GetShaderSource(
    const WebGLId<WebGLShader>& shaderId) {
  RefPtr<WebGLShader> shader = MustFind(shaderId);
  return mContext->GetShaderSource(*shader);
}

MaybeWebGLVariant HostWebGLContext::GetUniform(
    const WebGLId<WebGLProgram>& progId,
    const WebGLId<WebGLUniformLocation>& locId) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  RefPtr<WebGLUniformLocation> loc = MustFind(locId);
  return mContext->GetUniform(*prog, *loc);
}

void HostWebGLContext::Hint(GLenum target, GLenum mode) {
  mContext->Hint(target, mode);
}

void HostWebGLContext::LineWidth(GLfloat width) { mContext->LineWidth(width); }

void HostWebGLContext::LinkProgram(const WebGLId<WebGLProgram>& progId) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  mContext->LinkProgram(*prog);
}

WebGLPixelStore HostWebGLContext::PixelStorei(GLenum pname, GLint param) {
  return mContext->PixelStorei(pname, param);
}

void HostWebGLContext::PolygonOffset(GLfloat factor, GLfloat units) {
  mContext->PolygonOffset(factor, units);
}

void HostWebGLContext::SampleCoverage(GLclampf value, WebGLboolean invert) {
  mContext->SampleCoverage(value, invert);
}

void HostWebGLContext::Scissor(GLint x, GLint y, GLsizei width,
                               GLsizei height) {
  mContext->Scissor(x, y, width, height);
}

void HostWebGLContext::ShaderSource(const WebGLId<WebGLShader>& shaderId,
                                    const nsString& source) {
  RefPtr<WebGLShader> shader = MustFind(shaderId);
  mContext->ShaderSource(*shader, source);
}

void HostWebGLContext::StencilFunc(GLenum func, GLint ref, GLuint mask) {
  mContext->StencilFunc(func, ref, mask);
}

void HostWebGLContext::StencilFuncSeparate(GLenum face, GLenum func, GLint ref,
                                           GLuint mask) {
  mContext->StencilFuncSeparate(face, func, ref, mask);
}

void HostWebGLContext::StencilMask(GLuint mask) { mContext->StencilMask(mask); }

void HostWebGLContext::StencilMaskSeparate(GLenum face, GLuint mask) {
  mContext->StencilMaskSeparate(face, mask);
}

void HostWebGLContext::StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
  mContext->StencilOp(sfail, dpfail, dppass);
}

void HostWebGLContext::StencilOpSeparate(GLenum face, GLenum sfail,
                                         GLenum dpfail, GLenum dppass) {
  mContext->StencilOpSeparate(face, sfail, dpfail, dppass);
}

void HostWebGLContext::Viewport(GLint x, GLint y, GLsizei width,
                                GLsizei height) {
  mContext->Viewport(x, y, width, height);
}

// ------------------------- Buffer Objects -------------------------
void HostWebGLContext::BindBuffer(GLenum target,
                                  const WebGLId<WebGLBuffer>& buffer) {
  mContext->BindBuffer(target, Find(buffer));
}

void HostWebGLContext::BindBufferBase(GLenum target, GLuint index,
                                      const WebGLId<WebGLBuffer>& buffer) {
  mContext->BindBufferBase(target, index, Find(buffer));
}

void HostWebGLContext::BindBufferRange(GLenum target, GLuint index,
                                       const WebGLId<WebGLBuffer>& buffer,
                                       WebGLintptr offset, WebGLsizeiptr size) {
  mContext->BindBufferRange(target, index, Find(buffer), offset, size);
}

void HostWebGLContext::DeleteBuffer(const WebGLId<WebGLBuffer>& buf) {
  mContext->DeleteBuffer(Find(buf));
}

void HostWebGLContext::CopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                                         GLintptr readOffset,
                                         GLintptr writeOffset,
                                         GLsizeiptr size) {
  GetWebGL2Context()->CopyBufferSubData(readTarget, writeTarget, readOffset,
                                        writeOffset, size);
}

Maybe<UniquePtr<RawBuffer<>>> HostWebGLContext::GetBufferSubData(
    GLenum target, GLintptr srcByteOffset, size_t byteLen) {
  return GetWebGL2Context()->GetBufferSubData(target, srcByteOffset, byteLen);
}

void HostWebGLContext::BufferData(GLenum target, const RawBuffer<>& data,
                                  GLenum usage) {
  mContext->BufferDataImpl(target, data.Length(), data.Data(), usage);
}

void HostWebGLContext::BufferSubData(GLenum target, WebGLsizeiptr dstByteOffset,
                                     const RawBuffer<>& srcData) {
  mContext->BufferSubDataImpl(target, dstByteOffset, srcData.Length(),
                              srcData.Data());
}

// -------------------------- Framebuffer Objects --------------------------
void HostWebGLContext::BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1,
                                       GLint srcY1, GLint dstX0, GLint dstY0,
                                       GLint dstX1, GLint dstY1,
                                       GLbitfield mask, GLenum filter) {
  GetWebGL2Context()->BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                      dstX1, dstY1, mask, filter);
}

void HostWebGLContext::FramebufferTextureLayer(
    GLenum target, GLenum attachment, const WebGLId<WebGLTexture>& textureId,
    GLint level, GLint layer) {
  GetWebGL2Context()->FramebufferTextureLayer(target, attachment,
                                              Find(textureId), level, layer);
}

void HostWebGLContext::InvalidateFramebuffer(
    GLenum target, const nsTArray<GLenum>& attachments) {
  GetWebGL2Context()->InvalidateFramebuffer(target, attachments);
}

void HostWebGLContext::InvalidateSubFramebuffer(
    GLenum target, const nsTArray<GLenum>& attachments, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  GetWebGL2Context()->InvalidateSubFramebuffer(target, attachments, x, y, width,
                                               height);
}

void HostWebGLContext::ReadBuffer(GLenum mode) {
  GetWebGL2Context()->ReadBuffer(mode);
}

// ----------------------- Renderbuffer objects -----------------------
Maybe<nsTArray<int32_t>> HostWebGLContext::GetInternalformatParameter(
    GLenum target, GLenum internalformat, GLenum pname) {
  return GetWebGL2Context()->GetInternalformatParameter(target, internalformat,
                                                        pname);
}

void HostWebGLContext::RenderbufferStorage_base(GLenum target, GLsizei samples,
                                                GLenum internalFormat,
                                                GLsizei width, GLsizei height,
                                                FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  mContext->RenderbufferStorage_base(target, samples, internalFormat, width,
                                     height);
}

// --------------------------- Texture objects ---------------------------
void HostWebGLContext::ActiveTexture(GLenum texUnit) {
  return mContext->ActiveTexture(texUnit);
}

void HostWebGLContext::BindTexture(GLenum texTarget,
                                   const WebGLId<WebGLTexture>& tex) {
  return mContext->BindTexture(texTarget, Find(tex));
}

void HostWebGLContext::DeleteTexture(const WebGLId<WebGLTexture>& tex) {
  mContext->DeleteTexture(Find(tex));
}

void HostWebGLContext::GenerateMipmap(GLenum texTarget) {
  mContext->GenerateMipmap(texTarget);
}

void HostWebGLContext::CopyTexImage2D(GLenum target, GLint level,
                                      GLenum internalFormat, GLint x, GLint y,
                                      uint32_t width, uint32_t height,
                                      uint32_t depth) {
  mContext->CopyTexImage2D(target, level, internalFormat, x, y, width, height,
                           depth);
}

void HostWebGLContext::TexStorage(uint8_t funcDims, GLenum target,
                                  GLsizei levels, GLenum internalFormat,
                                  GLsizei width, GLsizei height, GLsizei depth,
                                  FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  GetWebGL2Context()->TexStorage(funcDims, target, levels, internalFormat,
                                 width, height, depth);
}

template <typename TexUnpackType>
struct ToTexUnpackTypeMatcher {
  template <typename T, typename mozilla::EnableIf<
                            mozilla::IsConvertible<T*, TexUnpackType*>::value,
                            int>::Type = 0>
  UniquePtr<TexUnpackType> operator()(UniquePtr<T>& x) {
    return std::move(x);
  }
  template <typename T, typename mozilla::EnableIf<
                            !mozilla::IsConvertible<T*, TexUnpackType*>::value,
                            char>::Type = 0>
  UniquePtr<TexUnpackType> operator()(UniquePtr<T>& x) {
    MOZ_ASSERT_UNREACHABLE(
        "Attempted to read TexUnpackBlob as something it was not");
    return nullptr;
  }
  UniquePtr<TexUnpackType> operator()(WebGLTexPboOffset& aPbo) {
    UniquePtr<webgl::TexUnpackBytes> bytes = mContext->ToTexUnpackBytes(aPbo);
    return operator()(bytes);
  }
  WebGLContext* mContext;
};

template <typename TexUnpackType>
UniquePtr<TexUnpackType> AsTexUnpackType(WebGLContext* aContext,
                                         MaybeWebGLTexUnpackVariant&& src) {
  if (!src) {
    return nullptr;
  }
  if ((!src.ref().is<WebGLTexPboOffset>()) &&
      (!aContext->ValidateNullPixelUnpackBuffer())) {
    return nullptr;
  }

  return src.ref().match(ToTexUnpackTypeMatcher<TexUnpackType>{aContext});
}

void HostWebGLContext::TexImage(uint8_t funcDims, GLenum target, GLint level,
                                GLenum internalFormat, GLsizei width,
                                GLsizei height, GLsizei depth, GLint border,
                                GLenum unpackFormat, GLenum unpackType,
                                MaybeWebGLTexUnpackVariant&& src,
                                FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  mContext->TexImage(
      funcDims, target, level, internalFormat, width, height, depth, border,
      unpackFormat, unpackType,
      AsTexUnpackType<webgl::TexUnpackBlob>(mContext, std::move(src)));
}

void HostWebGLContext::TexSubImage(uint8_t funcDims, GLenum target, GLint level,
                                   GLint xOffset, GLint yOffset, GLint zOffset,
                                   GLsizei width, GLsizei height, GLsizei depth,
                                   GLenum unpackFormat, GLenum unpackType,
                                   MaybeWebGLTexUnpackVariant&& src,
                                   FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  mContext->TexSubImage(
      funcDims, target, level, xOffset, yOffset, zOffset, width, height, depth,
      unpackFormat, unpackType,
      AsTexUnpackType<webgl::TexUnpackBlob>(mContext, std::move(src)));
}

void HostWebGLContext::CompressedTexImage(
    uint8_t funcDims, GLenum target, GLint level, GLenum internalFormat,
    GLsizei width, GLsizei height, GLsizei depth, GLint border,
    MaybeWebGLTexUnpackVariant&& src, const Maybe<GLsizei>& expectedImageSize,
    FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  mContext->CompressedTexImage(
      funcDims, target, level, internalFormat, width, height, depth, border,
      AsTexUnpackType<webgl::TexUnpackBytes>(mContext, std::move(src)),
      expectedImageSize);
}

void HostWebGLContext::CompressedTexSubImage(
    uint8_t funcDims, GLenum target, GLint level, GLint xOffset, GLint yOffset,
    GLint zOffset, GLsizei width, GLsizei height, GLsizei depth,
    GLenum unpackFormat, MaybeWebGLTexUnpackVariant&& src,
    const Maybe<GLsizei>& expectedImageSize, FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  mContext->CompressedTexSubImage(
      funcDims, target, level, xOffset, yOffset, zOffset, width, height, depth,
      unpackFormat,
      AsTexUnpackType<webgl::TexUnpackBytes>(mContext, std::move(src)),
      expectedImageSize);
}

void HostWebGLContext::CopyTexSubImage(uint8_t funcDims, GLenum target,
                                       GLint level, GLint xOffset,
                                       GLint yOffset, GLint zOffset, GLint x,
                                       GLint y, uint32_t width, uint32_t height,
                                       uint32_t depth, FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  mContext->CopyTexSubImage(funcDims, target, level, xOffset, yOffset, zOffset,
                            x, y, width, height, depth);
}

MaybeWebGLVariant HostWebGLContext::GetTexParameter(GLenum texTarget,
                                                    GLenum pname) {
  return mContext->GetTexParameter(texTarget, pname);
}

void HostWebGLContext::TexParameter_base(GLenum texTarget, GLenum pname,
                                         const FloatOrInt& param) {
  mContext->TexParameter_base(texTarget, pname, param);
}

// ------------------- Programs and shaders --------------------------------
void HostWebGLContext::UseProgram(const WebGLId<WebGLProgram>& prog) {
  mContext->UseProgram(Find(prog));
}

void HostWebGLContext::ValidateProgram(const WebGLId<WebGLProgram>& progId) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  mContext->ValidateProgram(*prog);
}

GLint HostWebGLContext::GetFragDataLocation(const WebGLId<WebGLProgram>& progId,
                                            const nsString& name) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return GetWebGL2Context()->GetFragDataLocation(*prog, name);
}

// ------------------------ Uniforms and attributes ------------------------
void HostWebGLContext::UniformNfv(const nsCString& funcName, uint8_t N,
                                  const WebGLId<WebGLUniformLocation>& loc,
                                  const RawBuffer<const float>& arr,
                                  GLuint elemOffset, GLuint elemCountOverride) {
  mContext->UniformNfv(funcName.BeginReading(), N, Find(loc), arr, elemOffset,
                       elemCountOverride);
}

void HostWebGLContext::UniformNiv(const nsCString& funcName, uint8_t N,
                                  const WebGLId<WebGLUniformLocation>& loc,
                                  const RawBuffer<const int32_t>& arr,
                                  GLuint elemOffset, GLuint elemCountOverride) {
  mContext->UniformNiv(funcName.BeginReading(), N, Find(loc), arr, elemOffset,
                       elemCountOverride);
}

void HostWebGLContext::UniformNuiv(const nsCString& funcName, uint8_t N,
                                   const WebGLId<WebGLUniformLocation>& loc,
                                   const RawBuffer<const uint32_t>& arr,
                                   GLuint elemOffset,
                                   GLuint elemCountOverride) {
  mContext->UniformNuiv(funcName.BeginReading(), N, Find(loc), arr, elemOffset,
                        elemCountOverride);
}

void HostWebGLContext::UniformMatrixAxBfv(
    const nsCString& funcName, uint8_t A, uint8_t B,
    const WebGLId<WebGLUniformLocation>& loc, bool transpose,
    const RawBuffer<const float>& arr, GLuint elemOffset,
    GLuint elemCountOverride) {
  mContext->UniformMatrixAxBfv(funcName.BeginReading(), A, B, Find(loc),
                               transpose, arr, elemOffset, elemCountOverride);
}

void HostWebGLContext::UniformFVec(const WebGLId<WebGLUniformLocation>& aLoc,
                                   const nsTArray<float>& vec) {
  switch (vec.Length()) {
    case 1:
      mContext->Uniform1f(Find(aLoc), vec[0]);
      break;
    case 2:
      mContext->Uniform2f(Find(aLoc), vec[0], vec[1]);
      break;
    case 3:
      mContext->Uniform3f(Find(aLoc), vec[0], vec[1], vec[2]);
      break;
    case 4:
      mContext->Uniform4f(Find(aLoc), vec[0], vec[1], vec[2], vec[3]);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Illegal number of parameters to UniformFVec");
  }
}

void HostWebGLContext::UniformIVec(const WebGLId<WebGLUniformLocation>& aLoc,
                                   const nsTArray<int32_t>& vec) {
  switch (vec.Length()) {
    case 1:
      mContext->Uniform1i(Find(aLoc), vec[0]);
      break;
    case 2:
      mContext->Uniform2i(Find(aLoc), vec[0], vec[1]);
      break;
    case 3:
      mContext->Uniform3i(Find(aLoc), vec[0], vec[1], vec[2]);
      break;
    case 4:
      mContext->Uniform4i(Find(aLoc), vec[0], vec[1], vec[2], vec[3]);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Illegal number of parameters to UniformIVec");
  }
}

void HostWebGLContext::UniformUIVec(const WebGLId<WebGLUniformLocation>& aLoc,
                                    const nsTArray<uint32_t>& vec) {
  switch (vec.Length()) {
    case 1:
      mContext->Uniform1ui(Find(aLoc), vec[0]);
      break;
    case 2:
      mContext->Uniform2ui(Find(aLoc), vec[0], vec[1]);
      break;
    case 3:
      mContext->Uniform3ui(Find(aLoc), vec[0], vec[1], vec[2]);
      break;
    case 4:
      mContext->Uniform4ui(Find(aLoc), vec[0], vec[1], vec[2], vec[3]);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Illegal number of parameters to UniformUIVec");
  }
}

void HostWebGLContext::VertexAttrib4f(GLuint index, GLfloat x, GLfloat y,
                                      GLfloat z, GLfloat w,
                                      FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  mContext->VertexAttrib4f(index, x, y, z, w);
}

void HostWebGLContext::VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z,
                                       GLint w, FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  GetWebGL2Context()->VertexAttribI4i(index, x, y, z, w);
}

void HostWebGLContext::VertexAttribI4ui(GLuint index, GLuint x, GLuint y,
                                        GLuint z, GLuint w,
                                        FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  GetWebGL2Context()->VertexAttribI4ui(index, x, y, z, w);
}

void HostWebGLContext::VertexAttribDivisor(GLuint index, GLuint divisor,
                                           bool aFromExtension) {
  GetWebGL2Context()->VertexAttribDivisor(index, divisor);
}

MaybeWebGLVariant HostWebGLContext::GetIndexedParameter(GLenum target,
                                                        GLuint index) {
  return GetWebGL2Context()->GetIndexedParameter(target, index);
}

MaybeWebGLVariant HostWebGLContext::GetUniformIndices(
    const WebGLId<WebGLProgram>& progId,
    const nsTArray<nsString>& uniformNames) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return GetWebGL2Context()->GetUniformIndices(*prog, uniformNames);
}

MaybeWebGLVariant HostWebGLContext::GetActiveUniforms(
    const WebGLId<WebGLProgram>& progId, const nsTArray<GLuint>& uniformIndices,
    GLenum pname) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return GetWebGL2Context()->GetActiveUniforms(*prog, uniformIndices, pname);
}

GLuint HostWebGLContext::GetUniformBlockIndex(
    const WebGLId<WebGLProgram>& progId, const nsString& uniformBlockName) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return GetWebGL2Context()->GetUniformBlockIndex(*prog, uniformBlockName);
}

MaybeWebGLVariant HostWebGLContext::GetActiveUniformBlockParameter(
    const WebGLId<WebGLProgram>& progId, GLuint uniformBlockIndex,
    GLenum pname) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return GetWebGL2Context()->GetActiveUniformBlockParameter(
      *prog, uniformBlockIndex, pname);
}

nsString HostWebGLContext::GetActiveUniformBlockName(
    const WebGLId<WebGLProgram>& progId, GLuint uniformBlockIndex) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return GetWebGL2Context()->GetActiveUniformBlockName(*prog,
                                                       uniformBlockIndex);
}

void HostWebGLContext::UniformBlockBinding(const WebGLId<WebGLProgram>& progId,
                                           GLuint uniformBlockIndex,
                                           GLuint uniformBlockBinding) {
  RefPtr<WebGLProgram> prog = MustFind(progId);
  return GetWebGL2Context()->UniformBlockBinding(*prog, uniformBlockIndex,
                                                 uniformBlockBinding);
}

void HostWebGLContext::EnableVertexAttribArray(GLuint index) {
  mContext->EnableVertexAttribArray(index);
}

void HostWebGLContext::DisableVertexAttribArray(GLuint index) {
  mContext->DisableVertexAttribArray(index);
}

MaybeWebGLVariant HostWebGLContext::GetVertexAttrib(GLuint index,
                                                    GLenum pname) {
  return mContext->GetVertexAttrib(index, pname);
}

WebGLsizeiptr HostWebGLContext::GetVertexAttribOffset(GLuint index,
                                                      GLenum pname) {
  return mContext->GetVertexAttribOffset(index, pname);
}

void HostWebGLContext::VertexAttribAnyPointer(bool isFuncInt, GLuint index,
                                              GLint size, GLenum type,
                                              bool normalized, GLsizei stride,
                                              WebGLintptr byteOffset,
                                              FuncScopeId aFuncId) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  mContext->VertexAttribAnyPointer(isFuncInt, index, size, type, normalized,
                                   stride, byteOffset);
}

// --------------------------- Buffer Operations --------------------------
void HostWebGLContext::ClearBufferfv(GLenum buffer, GLint drawBuffer,
                                     const RawBuffer<const float>& src,
                                     GLuint srcElemOffset) {
  GetWebGL2Context()->ClearBufferfv(buffer, drawBuffer, src, srcElemOffset);
}

void HostWebGLContext::ClearBufferiv(GLenum buffer, GLint drawBuffer,
                                     const RawBuffer<const int32_t>& src,
                                     GLuint srcElemOffset) {
  GetWebGL2Context()->ClearBufferiv(buffer, drawBuffer, src, srcElemOffset);
}

void HostWebGLContext::ClearBufferuiv(GLenum buffer, GLint drawBuffer,
                                      const RawBuffer<const uint32_t>& src,
                                      GLuint srcElemOffset) {
  GetWebGL2Context()->ClearBufferuiv(buffer, drawBuffer, src, srcElemOffset);
}

void HostWebGLContext::ClearBufferfi(GLenum buffer, GLint drawBuffer,
                                     GLfloat depth, GLint stencil) {
  GetWebGL2Context()->ClearBufferfi(buffer, drawBuffer, depth, stencil);
}

// ------------------------------ Readback -------------------------------
void HostWebGLContext::ReadPixels1(GLint x, GLint y, GLsizei width,
                                   GLsizei height, GLenum format, GLenum type,
                                   WebGLsizeiptr offset) {
  mContext->ReadPixels(x, y, width, height, format, type, offset);
}

Maybe<UniquePtr<RawBuffer<>>> HostWebGLContext::ReadPixels2(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    size_t byteLen) {
  return mContext->ReadPixels(x, y, width, height, format, type, byteLen);
}

// ----------------------------- Sampler -----------------------------------
void HostWebGLContext::DeleteSampler(const WebGLId<WebGLSampler>& aId) {
  GetWebGL2Context()->DeleteSampler(Find(aId));
}

void HostWebGLContext::BindSampler(GLuint unit,
                                   const WebGLId<WebGLSampler>& sampler) {
  GetWebGL2Context()->BindSampler(unit, Find(sampler));
}

void HostWebGLContext::SamplerParameteri(const WebGLId<WebGLSampler>& samplerId,
                                         GLenum pname, GLint param) {
  RefPtr<WebGLSampler> sampler = MustFind(samplerId);
  GetWebGL2Context()->SamplerParameteri(*sampler, pname, param);
}

void HostWebGLContext::SamplerParameterf(const WebGLId<WebGLSampler>& samplerId,
                                         GLenum pname, GLfloat param) {
  RefPtr<WebGLSampler> sampler = MustFind(samplerId);
  GetWebGL2Context()->SamplerParameterf(*sampler, pname, param);
}

MaybeWebGLVariant HostWebGLContext::GetSamplerParameter(
    const WebGLId<WebGLSampler>& samplerId, GLenum pname) {
  RefPtr<WebGLSampler> sampler = MustFind(samplerId);
  return GetWebGL2Context()->GetSamplerParameter(*sampler, pname);
}

// ------------------------------- GL Sync ---------------------------------
void HostWebGLContext::DeleteSync(const WebGLId<WebGLSync>& sync) {
  GetWebGL2Context()->DeleteSync(Find(sync));
}

GLenum HostWebGLContext::ClientWaitSync(const WebGLId<WebGLSync>& sync,
                                        GLbitfield flags, GLuint64 timeout) {
  return GetWebGL2Context()->ClientWaitSync(*MustFind(sync), flags, timeout);
}

void HostWebGLContext::WaitSync(const WebGLId<WebGLSync>& sync,
                                GLbitfield flags, GLint64 timeout) {
  GetWebGL2Context()->WaitSync(*MustFind(sync), flags, timeout);
}

MaybeWebGLVariant HostWebGLContext::GetSyncParameter(
    const WebGLId<WebGLSync>& sync, GLenum pname) {
  return GetWebGL2Context()->GetSyncParameter(*MustFind(sync), pname);
}

// -------------------------- Transform Feedback ---------------------------
void HostWebGLContext::DeleteTransformFeedback(
    const WebGLId<WebGLTransformFeedback>& tf) {
  GetWebGL2Context()->DeleteTransformFeedback(Find(tf));
}

void HostWebGLContext::BindTransformFeedback(
    GLenum target, const WebGLId<WebGLTransformFeedback>& tf) {
  GetWebGL2Context()->BindTransformFeedback(target, Find(tf));
}

void HostWebGLContext::BeginTransformFeedback(GLenum primitiveMode) {
  GetWebGL2Context()->BeginTransformFeedback(primitiveMode);
}

void HostWebGLContext::EndTransformFeedback() {
  GetWebGL2Context()->EndTransformFeedback();
}

void HostWebGLContext::PauseTransformFeedback() {
  GetWebGL2Context()->PauseTransformFeedback();
}

void HostWebGLContext::ResumeTransformFeedback() {
  GetWebGL2Context()->ResumeTransformFeedback();
}

void HostWebGLContext::TransformFeedbackVaryings(
    const WebGLId<WebGLProgram>& prog, const nsTArray<nsString>& varyings,
    GLenum bufferMode) {
  GetWebGL2Context()->TransformFeedbackVaryings(*MustFind(prog), varyings,
                                                bufferMode);
}

Maybe<WebGLActiveInfo> HostWebGLContext::GetTransformFeedbackVarying(
    const WebGLId<WebGLProgram>& prog, GLuint index) {
  return GetWebGL2Context()->GetTransformFeedbackVarying(*MustFind(prog),
                                                         index);
}

// ------------------------------ WebGL Debug
// ------------------------------------
void HostWebGLContext::EnqueueError(GLenum aGLError, const nsCString& aMsg) {
  mContext->GenerateError(aGLError, aMsg.BeginReading());
}

void HostWebGLContext::EnqueueWarning(const nsCString& aMsg) {
  mContext->GenerateWarning(aMsg.BeginReading());
}

void HostWebGLContext::ReportOOMAndLoseContext() {
  mContext->ErrorOutOfMemory("Ran out of memory in WebGL IPC.");
  LoseContext(false);
}

// -------------------------------------------------------------------------
// Host-side extension methods.
// -------------------------------------------------------------------------

// Misc. Extensions
void HostWebGLContext::EnableExtension(dom::CallerType callerType,
                                       WebGLExtensionID ext) {
  if (ext >= WebGLExtensionID::Max) {
    MOZ_ASSERT_UNREACHABLE("Illegal extension ID");
    return;
  }
  mContext->EnableExtension(ext, callerType);
}

const Maybe<ExtensionSets> HostWebGLContext::GetSupportedExtensions() {
  return mContext->GetSupportedExtensions();
}

void HostWebGLContext::DrawBuffers(const nsTArray<GLenum>& buffers,
                                   bool aFromExtension) {
  if (aFromExtension) {
    auto* ext = mContext->GetExtension<WebGLExtensionID::WEBGL_draw_buffers>();
    MOZ_RELEASE_ASSERT(ext);
    return ext->DrawBuffersWEBGL(buffers);
  }

  return GetWebGL2Context()->DrawBuffers(buffers);
}

Maybe<nsTArray<nsString>> HostWebGLContext::GetASTCExtensionSupportedProfiles()
    const {
  auto* ext =
      mContext->GetExtension<WebGLExtensionID::WEBGL_compressed_texture_astc>();
  MOZ_RELEASE_ASSERT(ext);
  return ext->GetSupportedProfiles();
}

nsString HostWebGLContext::GetTranslatedShaderSource(
    const WebGLId<WebGLShader>& shader) const {
  auto* ext = mContext->GetExtension<WebGLExtensionID::WEBGL_debug_shaders>();
  MOZ_RELEASE_ASSERT(ext);
  return ext->GetTranslatedShaderSource(*MustFind(shader));
}

void HostWebGLContext::LoseContext(bool isSimulated) {
  isSimulated ? mContext->LoseContext() : mContext->ForceLoseContext();
}

void HostWebGLContext::RestoreContext() { mContext->RestoreContext(); }

MaybeWebGLVariant HostWebGLContext::MOZDebugGetParameter(GLenum pname) const {
  auto* ext = mContext->GetExtension<WebGLExtensionID::MOZ_debug>();
  MOZ_RELEASE_ASSERT(ext);
  return ext->GetParameter(pname);
}

// VertexArrayObjectEXT
void HostWebGLContext::BindVertexArray(const WebGLId<WebGLVertexArray>& array,
                                       bool aFromExtension) {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::OES_vertex_array_object>();
    MOZ_RELEASE_ASSERT(ext);
    return ext->BindVertexArrayOES(Find(array));
  }

  GetWebGL2Context()->BindVertexArray(Find(array));
}

void HostWebGLContext::DeleteVertexArray(const WebGLId<WebGLVertexArray>& array,
                                         bool aFromExtension) {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::OES_vertex_array_object>();
    MOZ_RELEASE_ASSERT(ext);
    ext->DeleteVertexArrayOES(Find(array));
  } else {
    mContext->DeleteVertexArray(Find(array));
  }
}

// InstancedElementsEXT
void HostWebGLContext::DrawArraysInstanced(GLenum mode, GLint first,
                                           GLsizei count, GLsizei primcount,
                                           bool aFromExtension) {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::ANGLE_instanced_arrays>();
    MOZ_RELEASE_ASSERT(ext);
    return ext->DrawArraysInstancedANGLE(mode, first, count, primcount);
  }

  mContext->DrawArraysInstanced(mode, first, count, primcount);
}

void HostWebGLContext::DrawElementsInstanced(GLenum mode, GLsizei count,
                                             GLenum type, WebGLintptr offset,
                                             GLsizei primcount,
                                             FuncScopeId aFuncId,
                                             bool aFromExtension) {
  const WebGLContext::FuncScope scope(*mContext, GetFuncScopeName(aFuncId));
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::ANGLE_instanced_arrays>();
    MOZ_RELEASE_ASSERT(ext);
    return ext->DrawElementsInstancedANGLE(mode, count, type, offset,
                                           primcount);
  }

  mContext->DrawElementsInstanced(mode, count, type, offset, primcount);
}

void HostWebGLContext::DrawRangeElements(GLenum mode, GLuint start, GLuint end,
                                         GLsizei count, GLenum type,
                                         WebGLintptr byteOffset) {
  GetWebGL2Context()->DrawRangeElements(mode, start, end, count, type,
                                        byteOffset);
}

// GLQueryEXT
void HostWebGLContext::DeleteQuery(const WebGLId<WebGLQuery>& query,
                                   bool aFromExtension) const {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::EXT_disjoint_timer_query>();
    MOZ_RELEASE_ASSERT(ext);
    ext->DeleteQueryEXT(Find(query));
  } else {
    const_cast<WebGL2Context*>(GetWebGL2Context())->DeleteQuery(Find(query));
  }
}

void HostWebGLContext::BeginQuery(GLenum target,
                                  const WebGLId<WebGLQuery>& query,
                                  bool aFromExtension) const {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::EXT_disjoint_timer_query>();
    MOZ_RELEASE_ASSERT(ext);
    return ext->BeginQueryEXT(target, *MustFind(query));
  }

  const_cast<WebGL2Context*>(GetWebGL2Context())
      ->BeginQuery(target, *MustFind(query));
}

void HostWebGLContext::EndQuery(GLenum target, bool aFromExtension) const {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::EXT_disjoint_timer_query>();
    MOZ_RELEASE_ASSERT(ext);
    return ext->EndQueryEXT(target);
  }

  const_cast<WebGL2Context*>(GetWebGL2Context())->EndQuery(target);
}

void HostWebGLContext::QueryCounter(const WebGLId<WebGLQuery>& query,
                                    GLenum target) const {
  auto* ext =
      mContext->GetExtension<WebGLExtensionID::EXT_disjoint_timer_query>();
  MOZ_RELEASE_ASSERT(ext);
  return ext->QueryCounterEXT(*MustFind(query), target);
}

MaybeWebGLVariant HostWebGLContext::GetQuery(GLenum target, GLenum pname,
                                             bool aFromExtension) const {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::EXT_disjoint_timer_query>();
    MOZ_RELEASE_ASSERT(ext);
    return ext->GetQueryEXT(target, pname);
  }

  return const_cast<WebGL2Context*>(GetWebGL2Context())
      ->GetQuery(target, pname);
}

MaybeWebGLVariant HostWebGLContext::GetQueryParameter(
    const WebGLId<WebGLQuery>& query, GLenum pname, bool aFromExtension) const {
  if (aFromExtension) {
    auto* ext =
        mContext->GetExtension<WebGLExtensionID::EXT_disjoint_timer_query>();
    MOZ_RELEASE_ASSERT(ext);
    return ext->GetQueryObjectEXT(*MustFind(query), pname);
  }

  return const_cast<WebGL2Context*>(GetWebGL2Context())
      ->GetQueryParameter(*MustFind(query), pname);
}

void HostWebGLContext::PostWarning(const nsCString& aWarningMsg) {
  if (mClientContext) {
    mClientContext->PostWarning(aWarningMsg);
    return;
  }
  mErrorSource->RunCommand(WebGLErrorCommand::Warning, aWarningMsg);
}

void HostWebGLContext::PostContextCreationError(const nsCString& aMsg) {
  if (mClientContext) {
    mClientContext->PostContextCreationError(aMsg);
    return;
  }
  mErrorSource->RunCommand(WebGLErrorCommand::CreationError, aMsg);
}

void HostWebGLContext::OnLostContext() {
  if (mClientContext) {
    mClientContext->OnLostContext();
    return;
  }
  mErrorSource->RunCommand(WebGLErrorCommand::OnLostContext);
}

void HostWebGLContext::OnRestoredContext() {
  if (mClientContext) {
    mClientContext->OnRestoredContext();
    return;
  }
  mErrorSource->RunCommand(WebGLErrorCommand::OnRestoredContext);
}

already_AddRefed<layers::SharedSurfaceTextureClient>
HostWebGLContext::GetVRFrame() {
  return mContext->GetVRFrame();
}

}  // namespace mozilla

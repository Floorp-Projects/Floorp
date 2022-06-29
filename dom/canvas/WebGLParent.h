/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLPARENT_H_
#define WEBGLPARENT_H_

#include "mozilla/GfxMessageUtils.h"
#include "mozilla/dom/PWebGLParent.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {

class HostWebGLContext;
class WebGLChild;

namespace layers {
class RemoteTextureOwnerClient;
class SharedSurfaceTextureClient;
class SurfaceDescriptor;
}  // namespace layers

namespace gl {
class SharedSurface;
}  // namespace gl

namespace dom {

class WebGLParent : public PWebGLParent, public SupportsWeakPtr {
  friend PWebGLParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGLParent, override);

  mozilla::ipc::IPCResult RecvInitialize(const webgl::InitContextDesc&,
                                         webgl::InitContextResult* out);

  WebGLParent();  // For IPDL

  using IPCResult = mozilla::ipc::IPCResult;

  IPCResult RecvDispatchCommands(mozilla::ipc::Shmem&&, uint64_t);
  IPCResult RecvTexImage(uint32_t level, uint32_t respecFormat,
                         const uvec3& offset, const webgl::PackingInfo&,
                         webgl::TexUnpackBlobDesc&&);

  IPCResult RecvGetBufferSubData(GLenum target, uint64_t srcByteOffset,
                                 uint64_t byteSize, mozilla::ipc::Shmem* ret);
  IPCResult GetFrontBufferSnapshot(webgl::FrontBufferSnapshotIpc* ret,
                                   IProtocol* aProtocol);
  IPCResult RecvGetFrontBufferSnapshot(webgl::FrontBufferSnapshotIpc* ret);
  IPCResult RecvReadPixels(const webgl::ReadPixelsDesc&, uint64_t byteSize,
                           webgl::ReadPixelsResultIpc* ret);

  // -

  using ObjectId = webgl::ObjectId;

  IPCResult RecvCheckFramebufferStatus(GLenum target, GLenum* ret);
  IPCResult RecvClientWaitSync(ObjectId id, GLbitfield flags, GLuint64 timeout,
                               GLenum* ret);
  IPCResult RecvCreateOpaqueFramebuffer(ObjectId id,
                                        const OpaqueFramebufferOptions&,
                                        bool* ret);
  IPCResult RecvDrawingBufferSize(uvec2* ret);
  IPCResult RecvFinish();
  IPCResult RecvGetBufferParameter(GLenum target, GLenum pname,
                                   Maybe<double>* ret);
  IPCResult RecvGetCompileResult(ObjectId id, webgl::CompileResult* ret);
  IPCResult RecvGetError(GLenum* ret);
  IPCResult RecvGetFragDataLocation(ObjectId id, const std::string& name,
                                    GLint* ret);
  IPCResult RecvGetFramebufferAttachmentParameter(ObjectId id,
                                                  GLenum attachment,
                                                  GLenum pname,
                                                  Maybe<double>* ret);
  IPCResult RecvGetFrontBuffer(ObjectId fb, bool vr,
                               Maybe<layers::SurfaceDescriptor>* ret);
  IPCResult RecvPresentFrontBufferToCompositor(
      uint64_t fb, layers::RemoteTextureId textureId,
      layers::RemoteTextureOwnerId ownerId);
  IPCResult RecvGetIndexedParameter(GLenum target, GLuint index,
                                    Maybe<double>* ret);
  IPCResult RecvGetInternalformatParameter(GLenum target, GLuint format,
                                           GLuint pname,
                                           Maybe<std::vector<int32_t>>* ret);
  IPCResult RecvGetLinkResult(ObjectId id, webgl::LinkResult* ret);
  IPCResult RecvGetNumber(GLenum pname, Maybe<double>* ret);
  IPCResult RecvGetQueryParameter(ObjectId id, GLenum pname,
                                  Maybe<double>* ret);
  IPCResult RecvGetRenderbufferParameter(ObjectId id, GLenum pname,
                                         Maybe<double>* ret);
  IPCResult RecvGetSamplerParameter(ObjectId id, GLenum pname,
                                    Maybe<double>* ret);
  IPCResult RecvGetShaderPrecisionFormat(
      GLenum shaderType, GLenum precisionType,
      Maybe<webgl::ShaderPrecisionFormat>* ret);
  IPCResult RecvGetString(GLenum pname, Maybe<std::string>* ret);
  IPCResult RecvGetTexParameter(ObjectId id, GLenum pname, Maybe<double>* ret);
  IPCResult RecvGetUniform(ObjectId id, uint32_t loc,
                           webgl::GetUniformData* ret);
  IPCResult RecvGetVertexAttrib(GLuint index, GLenum pname, Maybe<double>* ret);
  IPCResult RecvIsEnabled(GLenum cap, bool* ret);
  IPCResult RecvOnMemoryPressure();
  IPCResult RecvValidateProgram(ObjectId id, bool* ret);

  // -

 private:
  ~WebGLParent();

  mozilla::ipc::IPCResult Recv__delete__() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  UniquePtr<HostWebGLContext> mHost;

  // Runnable that repeatedly processes our WebGL command queue
  RefPtr<Runnable> mRunCommandsRunnable;

  RefPtr<layers::RemoteTextureOwnerClient> mRemoteTextureOwner;
};

}  // namespace dom
}  // namespace mozilla

#endif  // WEBGLPARENT_H_

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLParent.h"

#include "WebGLChild.h"
#include "mozilla/dom/WebGLCrossProcessCommandQueue.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "HostWebGLContext.h"
#include "WebGLMethodDispatcher.h"

namespace mozilla {

namespace dom {

mozilla::ipc::IPCResult WebGLParent::RecvInitialize(
    const webgl::InitContextDesc& desc,
    UniquePtr<HostWebGLCommandSinkP>&& aSinkP,
    UniquePtr<HostWebGLCommandSinkI>&& aSinkI,
    webgl::InitContextResult* const out) {
  auto remotingData = Some(HostWebGLContext::RemotingData{
      *this, {},  // std::move(commandSink),
  });

  mHost = HostWebGLContext::Create(
      {
          {},
          std::move(remotingData),
      },
      desc, out);

  if (!mHost) {
    return IPC_FAIL(this, "Failed to create HostWebGLContext");
  }

  return IPC_OK();
}

WebGLParent::WebGLParent() : PcqActor(this) {}
WebGLParent::~WebGLParent() = default;

// -

using IPCResult = mozilla::ipc::IPCResult;

IPCResult WebGLParent::RecvDispatchCommands(Shmem&& shmem,
                                            const uint64_t cmdsByteSize) {
  MOZ_ASSERT(cmdsByteSize);
  const auto shmemBytes = ByteRange(shmem);
  const auto byteSize = std::min<uint64_t>(shmemBytes.length(), cmdsByteSize);
  const auto cmdsBytes =
      Range<const uint8_t>{shmemBytes.begin(), shmemBytes.begin() + byteSize};
  auto view = webgl::RangeConsumerView{cmdsBytes};

  while (true) {
    size_t id = 0;
    const auto status = view.ReadParam(&id);
    if (status != QueueStatus::kSuccess) break;

    WebGLMethodDispatcher<0>::DispatchCommand(*mHost, id, view);
  }

  return IPC_OK();
}

// -

mozilla::ipc::IPCResult WebGLParent::Recv__delete__() {
  mHost = nullptr;
  return IPC_OK();
}

void WebGLParent::ActorDestroy(ActorDestroyReason aWhy) { mHost = nullptr; }

// -

IPCResult WebGLParent::RecvGetFrontBufferSnapshot(
    webgl::FrontBufferSnapshotIpc* const ret) {
  *ret = {};

  const auto surfSize = mHost->GetFrontBufferSize();
  const auto byteSize = 4 * surfSize.x * surfSize.y;

  Shmem shmem;
  if (!PWebGLParent::AllocShmem(
          byteSize, mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC,
          &shmem)) {
    return IPC_FAIL(this, "Failed to allocate shmem for result");
  }

  auto shmemBytes = ByteRange(shmem);
  if (!mHost->FrontBufferSnapshotInto(shmemBytes)) return IPC_OK();
  *ret = {surfSize, Some(std::move(shmem))};
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetBufferSubData(const GLenum target,
                                            const uint64_t srcByteOffset,
                                            const uint64_t byteSize,
                                            Shmem* const ret) {
  Shmem shmem;
  if (!PWebGLParent::AllocShmem(
          byteSize, mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC,
          &shmem)) {
    MOZ_ASSERT(false);
    return IPC_FAIL(this, "Failed to allocate shmem for result");
  }

  const auto range = ByteRange(shmem);
  memset(range.begin().get(), 0,
         range.length());  // TODO: This is usually overkill.

  if (mHost->GetBufferSubData(target, srcByteOffset, range)) {
    *ret = std::move(shmem);
  }
  return IPC_OK();
}

IPCResult WebGLParent::RecvReadPixels(const webgl::ReadPixelsDesc& desc,
                                      const uint64_t byteCount,
                                      webgl::ReadPixelsResultIpc* const ret) {
  *ret = {};

  Shmem shmem;
  if (!PWebGLParent::AllocShmem(
          byteCount, mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC,
          &shmem)) {
    MOZ_ASSERT(false);
    return IPC_FAIL(this, "Failed to allocate shmem for result");
  }

  auto range = ByteRange(shmem);
  memset(range.begin().get(), 0,
         range.length());  // TODO: This is usually overkill.

  const auto res = mHost->ReadPixelsInto(desc, range);
  *ret = {res, std::move(shmem)};
  return IPC_OK();
}

// -

IPCResult WebGLParent::RecvCheckFramebufferStatus(GLenum target,
                                                  GLenum* const ret) {
  *ret = mHost->CheckFramebufferStatus(target);
  return IPC_OK();
}

IPCResult WebGLParent::RecvClientWaitSync(ObjectId id, GLbitfield flags,
                                          GLuint64 timeout, GLenum* const ret) {
  *ret = mHost->ClientWaitSync(id, flags, timeout);
  return IPC_OK();
}

IPCResult WebGLParent::RecvCreateOpaqueFramebuffer(
    const ObjectId id, const OpaqueFramebufferOptions& options,
    bool* const ret) {
  *ret = mHost->CreateOpaqueFramebuffer(id, options);
  return IPC_OK();
}

IPCResult WebGLParent::RecvDrawingBufferSize(uvec2* const ret) {
  *ret = mHost->DrawingBufferSize();
  return IPC_OK();
}

IPCResult WebGLParent::RecvFinish() {
  mHost->Finish();
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetBufferParameter(GLenum target, GLenum pname,
                                              Maybe<double>* const ret) {
  *ret = mHost->GetBufferParameter(target, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetCompileResult(ObjectId id,
                                            webgl::CompileResult* const ret) {
  *ret = mHost->GetCompileResult(id);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetError(GLenum* const ret) {
  *ret = mHost->GetError();
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetFragDataLocation(ObjectId id,
                                               const std::string& name,
                                               GLint* const ret) {
  *ret = mHost->GetFragDataLocation(id, name);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetFramebufferAttachmentParameter(
    ObjectId id, GLenum attachment, GLenum pname, Maybe<double>* const ret) {
  *ret = mHost->GetFramebufferAttachmentParameter(id, attachment, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetFrontBuffer(
    ObjectId fb, const bool vr, Maybe<layers::SurfaceDescriptor>* const ret) {
  *ret = mHost->GetFrontBuffer(fb, vr);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetIndexedParameter(GLenum target, GLuint index,
                                               Maybe<double>* const ret) {
  *ret = mHost->GetIndexedParameter(target, index);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetInternalformatParameter(
    const GLenum target, const GLuint format, const GLuint pname,
    Maybe<std::vector<int32_t>>* const ret) {
  *ret = mHost->GetInternalformatParameter(target, format, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetLinkResult(ObjectId id,
                                         webgl::LinkResult* const ret) {
  *ret = mHost->GetLinkResult(id);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetNumber(GLenum pname, Maybe<double>* const ret) {
  *ret = mHost->GetNumber(pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetQueryParameter(ObjectId id, GLenum pname,
                                             Maybe<double>* const ret) {
  *ret = mHost->GetQueryParameter(id, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetRenderbufferParameter(ObjectId id, GLenum pname,
                                                    Maybe<double>* const ret) {
  *ret = mHost->GetRenderbufferParameter(id, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetSamplerParameter(ObjectId id, GLenum pname,
                                               Maybe<double>* const ret) {
  *ret = mHost->GetSamplerParameter(id, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetShaderPrecisionFormat(
    GLenum shaderType, GLenum precisionType,
    Maybe<webgl::ShaderPrecisionFormat>* const ret) {
  *ret = mHost->GetShaderPrecisionFormat(shaderType, precisionType);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetString(GLenum pname,
                                     Maybe<std::string>* const ret) {
  *ret = mHost->GetString(pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetTexParameter(ObjectId id, GLenum pname,
                                           Maybe<double>* const ret) {
  *ret = mHost->GetTexParameter(id, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetUniform(ObjectId id, uint32_t loc,
                                      webgl::GetUniformData* const ret) {
  *ret = mHost->GetUniform(id, loc);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetVertexAttrib(GLuint index, GLenum pname,
                                           Maybe<double>* const ret) {
  *ret = mHost->GetVertexAttrib(index, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvIsEnabled(GLenum cap, bool* const ret) {
  *ret = mHost->IsEnabled(cap);
  return IPC_OK();
}

IPCResult WebGLParent::RecvOnMemoryPressure() {
  mHost->OnMemoryPressure();
  return IPC_OK();
}

IPCResult WebGLParent::RecvValidateProgram(ObjectId id, bool* const ret) {
  *ret = mHost->ValidateProgram(id);
  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla

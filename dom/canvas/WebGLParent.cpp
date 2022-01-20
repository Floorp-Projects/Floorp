/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLParent.h"

#include "WebGLChild.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "ImageContainer.h"
#include "HostWebGLContext.h"
#include "WebGLMethodDispatcher.h"

namespace mozilla::dom {

mozilla::ipc::IPCResult WebGLParent::RecvInitialize(
    const webgl::InitContextDesc& desc, webgl::InitContextResult* const out) {
  mHost = HostWebGLContext::Create({nullptr, this}, desc, out);

  if (!mHost && !out->error.size()) {
    return IPC_FAIL(this, "Abnormally failed to create HostWebGLContext.");
  }

  return IPC_OK();
}

WebGLParent::WebGLParent() = default;
WebGLParent::~WebGLParent() = default;

// -

using IPCResult = mozilla::ipc::IPCResult;

IPCResult WebGLParent::RecvDispatchCommands(Shmem&& rawShmem,
                                            const uint64_t cmdsByteSize) {
  AUTO_PROFILER_LABEL("WebGLParent::RecvDispatchCommands", GRAPHICS);
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  auto shmem = webgl::RaiiShmem(this, std::move(rawShmem));

  const auto& gl = mHost->mContext->GL();
  const gl::GLContext::TlsScope tlsIsCurrent(gl);

  MOZ_ASSERT(cmdsByteSize);
  const auto shmemBytes = shmem.ByteRange();
  const auto byteSize = std::min<uint64_t>(shmemBytes.length(), cmdsByteSize);
  const auto cmdsBytes =
      Range<const uint8_t>{shmemBytes.begin(), shmemBytes.begin() + byteSize};
  auto view = webgl::RangeConsumerView{cmdsBytes};

  if (kIsDebug) {
    const auto initialOffset =
        AlignmentOffset(kUniversalAlignment, cmdsBytes.begin().get());
    MOZ_ALWAYS_TRUE(!initialOffset);
  }

  while (true) {
    view.AlignTo(kUniversalAlignment);
    size_t id = 0;
    if (!view.ReadParam(&id)) break;

    const auto ok = WebGLMethodDispatcher<0>::DispatchCommand(*mHost, id, view);
    if (!ok) {
      const nsPrintfCString cstr(
          "DispatchCommand(id: %i) failed. Please file a bug!", int(id));
      const auto str = ToString(cstr);
      gfxCriticalError() << str;
      mHost->JsWarning(str);
      mHost->OnContextLoss(webgl::ContextLossReason::None);
      break;
    }
  }

  return IPC_OK();
}

IPCResult WebGLParent::RecvTexImage(const uint32_t level,
                                    const uint32_t respecFormat,
                                    const uvec3& offset,
                                    const webgl::PackingInfo& pi,
                                    webgl::TexUnpackBlobDesc&& desc) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  mHost->TexImage(level, respecFormat, offset, pi, desc);
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
  return GetFrontBufferSnapshot(ret, this);
}

IPCResult WebGLParent::GetFrontBufferSnapshot(
    webgl::FrontBufferSnapshotIpc* const ret, IProtocol* aProtocol) {
  AUTO_PROFILER_LABEL("WebGLParent::GetFrontBufferSnapshot", GRAPHICS);
  *ret = {};
  if (!mHost) {
    return IPC_FAIL(aProtocol, "HostWebGLContext is not initialized.");
  }

  const auto maybeSize = mHost->FrontBufferSnapshotInto({});
  if (maybeSize) {
    const auto& surfSize = *maybeSize;
    const auto byteSize = 4 * surfSize.x * surfSize.y;

    auto shmem = webgl::RaiiShmem::Alloc(
        aProtocol, byteSize,
        mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC);
    if (!shmem) {
      NS_WARNING("Failed to alloc shmem for RecvGetFrontBufferSnapshot.");
      return IPC_FAIL(aProtocol, "Failed to allocate shmem for result");
    }
    const auto range = shmem.ByteRange();
    *ret = {surfSize, Some(shmem.Extract())};

    if (!mHost->FrontBufferSnapshotInto(Some(range))) {
      gfxCriticalNote << "WebGLParent::RecvGetFrontBufferSnapshot: "
                         "FrontBufferSnapshotInto(some) failed after "
                         "FrontBufferSnapshotInto(none)";
      // Zero means failure, as we still need to send any shmem we alloc.
      ret->surfSize = {0, 0};
    }
  }
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetBufferSubData(const GLenum target,
                                            const uint64_t srcByteOffset,
                                            const uint64_t byteSize,
                                            Shmem* const ret) {
  AUTO_PROFILER_LABEL("WebGLParent::RecvGetBufferSubData", GRAPHICS);
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  const auto allocSize = 1 + byteSize;
  auto shmem = webgl::RaiiShmem::Alloc(
      this, allocSize,
      mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC);
  if (!shmem) {
    NS_WARNING("Failed to alloc shmem for RecvGetBufferSubData.");
    return IPC_FAIL(this, "Failed to allocate shmem for result");
  }

  const auto shmemRange = shmem.ByteRange();
  const auto dataRange =
      Range<uint8_t>{shmemRange.begin() + 1, shmemRange.end()};

  // We need to always send the shmem:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1463831#c2
  const auto ok = mHost->GetBufferSubData(target, srcByteOffset, dataRange);
  *(shmemRange.begin().get()) = ok;
  *ret = shmem.Extract();
  return IPC_OK();
}

IPCResult WebGLParent::RecvReadPixels(const webgl::ReadPixelsDesc& desc,
                                      const uint64_t byteSize,
                                      webgl::ReadPixelsResultIpc* const ret) {
  AUTO_PROFILER_LABEL("WebGLParent::RecvReadPixels", GRAPHICS);
  *ret = {};
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  const auto allocSize = std::max<uint64_t>(1, byteSize);
  auto shmem = webgl::RaiiShmem::Alloc(
      this, allocSize,
      mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC);
  if (!shmem) {
    NS_WARNING("Failed to alloc shmem for RecvReadPixels.");
    return IPC_FAIL(this, "Failed to allocate shmem for result");
  }

  const auto range = shmem.ByteRange();

  const auto res = mHost->ReadPixelsInto(desc, range);
  *ret = {res, shmem.Extract()};
  return IPC_OK();
}

// -

IPCResult WebGLParent::RecvCheckFramebufferStatus(GLenum target,
                                                  GLenum* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->CheckFramebufferStatus(target);
  return IPC_OK();
}

IPCResult WebGLParent::RecvClientWaitSync(ObjectId id, GLbitfield flags,
                                          GLuint64 timeout, GLenum* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->ClientWaitSync(id, flags, timeout);
  return IPC_OK();
}

IPCResult WebGLParent::RecvCreateOpaqueFramebuffer(
    const ObjectId id, const OpaqueFramebufferOptions& options,
    bool* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->CreateOpaqueFramebuffer(id, options);
  return IPC_OK();
}

IPCResult WebGLParent::RecvDrawingBufferSize(uvec2* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->DrawingBufferSize();
  return IPC_OK();
}

IPCResult WebGLParent::RecvFinish() {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  mHost->Finish();
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetBufferParameter(GLenum target, GLenum pname,
                                              Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetBufferParameter(target, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetCompileResult(ObjectId id,
                                            webgl::CompileResult* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetCompileResult(id);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetError(GLenum* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetError();
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetFragDataLocation(ObjectId id,
                                               const std::string& name,
                                               GLint* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetFragDataLocation(id, name);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetFramebufferAttachmentParameter(
    ObjectId id, GLenum attachment, GLenum pname, Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetFramebufferAttachmentParameter(id, attachment, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetFrontBuffer(
    ObjectId fb, const bool vr, Maybe<layers::SurfaceDescriptor>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetFrontBuffer(fb, vr);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetIndexedParameter(GLenum target, GLuint index,
                                               Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetIndexedParameter(target, index);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetInternalformatParameter(
    const GLenum target, const GLuint format, const GLuint pname,
    Maybe<std::vector<int32_t>>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetInternalformatParameter(target, format, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetLinkResult(ObjectId id,
                                         webgl::LinkResult* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetLinkResult(id);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetNumber(GLenum pname, Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetNumber(pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetQueryParameter(ObjectId id, GLenum pname,
                                             Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetQueryParameter(id, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetRenderbufferParameter(ObjectId id, GLenum pname,
                                                    Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetRenderbufferParameter(id, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetSamplerParameter(ObjectId id, GLenum pname,
                                               Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetSamplerParameter(id, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetShaderPrecisionFormat(
    GLenum shaderType, GLenum precisionType,
    Maybe<webgl::ShaderPrecisionFormat>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetShaderPrecisionFormat(shaderType, precisionType);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetString(GLenum pname,
                                     Maybe<std::string>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetString(pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetTexParameter(ObjectId id, GLenum pname,
                                           Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetTexParameter(id, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetUniform(ObjectId id, uint32_t loc,
                                      webgl::GetUniformData* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetUniform(id, loc);
  return IPC_OK();
}

IPCResult WebGLParent::RecvGetVertexAttrib(GLuint index, GLenum pname,
                                           Maybe<double>* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->GetVertexAttrib(index, pname);
  return IPC_OK();
}

IPCResult WebGLParent::RecvIsEnabled(GLenum cap, bool* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->IsEnabled(cap);
  return IPC_OK();
}

IPCResult WebGLParent::RecvOnMemoryPressure() {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  mHost->OnMemoryPressure();
  return IPC_OK();
}

IPCResult WebGLParent::RecvValidateProgram(ObjectId id, bool* const ret) {
  if (!mHost) {
    return IPC_FAIL(this, "HostWebGLContext is not initialized.");
  }

  *ret = mHost->ValidateProgram(id);
  return IPC_OK();
}

}  // namespace mozilla::dom

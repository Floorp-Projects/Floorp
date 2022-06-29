/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HostWebGLContext.h"

#include "CompositableHost.h"
#include "mozilla/layers/LayersSurfaces.h"

#include "MozFramebuffer.h"
#include "TexUnpackBlob.h"
#include "WebGL2Context.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLFramebuffer.h"
#include "WebGLMemoryTracker.h"
#include "WebGLParent.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLSync.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"
#include "WebGLQuery.h"

#include "mozilla/StaticMutex.h"

namespace mozilla {

// -

static StaticMutex sContextSetLock MOZ_UNANNOTATED;

static std::unordered_set<HostWebGLContext*>& DeferredStaticContextSet() {
  static std::unordered_set<HostWebGLContext*> sContextSet;
  return sContextSet;
}

LockedOutstandingContexts::LockedOutstandingContexts()
    : contexts(DeferredStaticContextSet()) {
  sContextSetLock.Lock();
}

LockedOutstandingContexts::~LockedOutstandingContexts() {
  sContextSetLock.Unlock();
}

// -

/*static*/
UniquePtr<HostWebGLContext> HostWebGLContext::Create(
    const OwnerData& ownerData, const webgl::InitContextDesc& desc,
    webgl::InitContextResult* const out) {
  auto host = WrapUnique(new HostWebGLContext(ownerData));
  auto webgl = WebGLContext::Create(*host, desc, out);
  if (!webgl) return nullptr;
  return host;
}

HostWebGLContext::HostWebGLContext(const OwnerData& ownerData)
    : mOwnerData(ownerData) {
  StaticMutexAutoLock lock(sContextSetLock);
  auto& contexts = DeferredStaticContextSet();
  (void)contexts.insert(this);
}

HostWebGLContext::~HostWebGLContext() {
  StaticMutexAutoLock lock(sContextSetLock);
  auto& contexts = DeferredStaticContextSet();
  (void)contexts.erase(this);
}

// -

void HostWebGLContext::OnContextLoss(const webgl::ContextLossReason reason) {
  if (mOwnerData.inProcess) {
    mOwnerData.inProcess->OnContextLoss(reason);
  } else {
    (void)mOwnerData.outOfProcess->SendOnContextLoss(reason);
  }
}

void HostWebGLContext::JsWarning(const std::string& text) const {
  if (mOwnerData.inProcess) {
    mOwnerData.inProcess->JsWarning(text);
    return;
  }
  (void)mOwnerData.outOfProcess->SendJsWarning(text);
}

gl::SwapChain* HostWebGLContext::GetSwapChain(const ObjectId xrFb,
                                              const bool webvr) const {
  return mContext->GetSwapChain(AutoResolve(xrFb), webvr);
}

Maybe<layers::SurfaceDescriptor> HostWebGLContext::GetFrontBuffer(
    const ObjectId xrFb, const bool webvr) const {
  return mContext->GetFrontBuffer(AutoResolve(xrFb), webvr);
}

//////////////////////////////////////////////
// Creation

void HostWebGLContext::CreateBuffer(const ObjectId id) {
  auto& slot = mBufferMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = mContext->CreateBuffer();
}

void HostWebGLContext::CreateFramebuffer(const ObjectId id) {
  auto& slot = mFramebufferMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = mContext->CreateFramebuffer();
}

bool HostWebGLContext::CreateOpaqueFramebuffer(
    const ObjectId id, const webgl::OpaqueFramebufferOptions& options) {
  auto& slot = mFramebufferMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return false;
  }
  slot = mContext->CreateOpaqueFramebuffer(options);
  return slot;
}

void HostWebGLContext::CreateProgram(const ObjectId id) {
  auto& slot = mProgramMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = mContext->CreateProgram();
}

void HostWebGLContext::CreateQuery(const ObjectId id) {
  auto& slot = mQueryMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = mContext->CreateQuery();
}

void HostWebGLContext::CreateRenderbuffer(const ObjectId id) {
  auto& slot = mRenderbufferMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = mContext->CreateRenderbuffer();
}

void HostWebGLContext::CreateSampler(const ObjectId id) {
  auto& slot = mSamplerMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = GetWebGL2Context()->CreateSampler();
}

void HostWebGLContext::CreateShader(const ObjectId id, GLenum type) {
  auto& slot = mShaderMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = mContext->CreateShader(type);
}

void HostWebGLContext::CreateSync(const ObjectId id) {
  auto& slot = mSyncMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = GetWebGL2Context()->FenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void HostWebGLContext::CreateTexture(const ObjectId id) {
  auto& slot = mTextureMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = mContext->CreateTexture();
}

void HostWebGLContext::CreateTransformFeedback(const ObjectId id) {
  auto& slot = mTransformFeedbackMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = GetWebGL2Context()->CreateTransformFeedback();
}

void HostWebGLContext::CreateVertexArray(const ObjectId id) {
  auto& slot = mVertexArrayMap[id];
  if (slot) {
    MOZ_ASSERT(false, "duplicate ID");
    return;
  }
  slot = mContext->CreateVertexArray();
}

////////////////////////

#define _(X) \
  void HostWebGLContext::Delete##X(const ObjectId id) { m##X##Map.erase(id); }

_(Buffer)
_(Framebuffer)
_(Program)
_(Query)
_(Renderbuffer)
_(Sampler)
_(Shader)
_(Sync)
_(Texture)
_(TransformFeedback)
_(VertexArray)

#undef _

}  // namespace mozilla

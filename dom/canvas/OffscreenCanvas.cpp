/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvas.h"

#include "mozilla/dom/OffscreenCanvasBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/Telemetry.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasUtils.h"
#include "GLScreenBuffer.h"
#include "WebGL1Context.h"
#include "WebGL2Context.h"

namespace mozilla {
namespace dom {

OffscreenCanvasCloneData::OffscreenCanvasCloneData(layers::AsyncCanvasRenderer* aRenderer,
                                                   uint32_t aWidth, uint32_t aHeight,
                                                   layers::LayersBackend aCompositorBackend,
                                                   bool aNeutered, bool aIsWriteOnly)
  : mRenderer(aRenderer)
  , mWidth(aWidth)
  , mHeight(aHeight)
  , mCompositorBackendType(aCompositorBackend)
  , mNeutered(aNeutered)
  , mIsWriteOnly(aIsWriteOnly)
{
}

OffscreenCanvasCloneData::~OffscreenCanvasCloneData()
{
}

OffscreenCanvas::OffscreenCanvas(uint32_t aWidth,
                                 uint32_t aHeight,
                                 layers::LayersBackend aCompositorBackend,
                                 layers::AsyncCanvasRenderer* aRenderer)
  : mAttrDirty(false)
  , mNeutered(false)
  , mIsWriteOnly(false)
  , mWidth(aWidth)
  , mHeight(aHeight)
  , mCompositorBackendType(aCompositorBackend)
  , mCanvasClient(nullptr)
  , mCanvasRenderer(aRenderer)
{}

OffscreenCanvas::~OffscreenCanvas()
{
  ClearResources();
}

OffscreenCanvas*
OffscreenCanvas::GetParentObject() const
{
  return nullptr;
}

JSObject*
OffscreenCanvas::WrapObject(JSContext* aCx,
                            JS::Handle<JSObject*> aGivenProto)
{
  return OffscreenCanvasBinding::Wrap(aCx, this, aGivenProto);
}

void
OffscreenCanvas::ClearResources()
{
  if (mCanvasClient) {
    mCanvasClient->Clear();
    ImageBridgeChild::DispatchReleaseCanvasClient(mCanvasClient);
    mCanvasClient = nullptr;

    if (mCanvasRenderer) {
      nsCOMPtr<nsIThread> activeThread = mCanvasRenderer->GetActiveThread();
      MOZ_RELEASE_ASSERT(activeThread);
      MOZ_RELEASE_ASSERT(activeThread == NS_GetCurrentThread());
      mCanvasRenderer->SetCanvasClient(nullptr);
      mCanvasRenderer->mContext = nullptr;
      mCanvasRenderer->mGLContext = nullptr;
      mCanvasRenderer->ResetActiveThread();
    }
  }
}

already_AddRefed<nsISupports>
OffscreenCanvas::GetContext(JSContext* aCx,
                            const nsAString& aContextId,
                            JS::Handle<JS::Value> aContextOptions,
                            ErrorResult& aRv)
{
  if (mNeutered) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // We only support WebGL in workers for now
  CanvasContextType contextType;
  if (!CanvasUtils::GetCanvasContextType(aContextId, &contextType)) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  if (!(contextType == CanvasContextType::WebGL1 ||
        contextType == CanvasContextType::WebGL2))
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  already_AddRefed<nsISupports> result =
    CanvasRenderingContextHelper::GetContext(aCx,
                                             aContextId,
                                             aContextOptions,
                                             aRv);

  if (!mCurrentContext) {
    return nullptr;
  }

  if (mCanvasRenderer) {
    WebGLContext* webGL = static_cast<WebGLContext*>(mCurrentContext.get());
    gl::GLContext* gl = webGL->GL();
    mCanvasRenderer->mContext = mCurrentContext;
    mCanvasRenderer->SetActiveThread();
    mCanvasRenderer->mGLContext = gl;
    mCanvasRenderer->SetIsAlphaPremultiplied(webGL->IsPremultAlpha() || !gl->Caps().alpha);

    if (ImageBridgeChild::IsCreated()) {
      TextureFlags flags = TextureFlags::ORIGIN_BOTTOM_LEFT;
      mCanvasClient = ImageBridgeChild::GetSingleton()->
        CreateCanvasClient(CanvasClient::CanvasClientTypeShSurf, flags).take();
      mCanvasRenderer->SetCanvasClient(mCanvasClient);

      gl::GLScreenBuffer* screen = gl->Screen();
      gl::SurfaceCaps caps = screen->mCaps;
      auto forwarder = mCanvasClient->GetForwarder();

      UniquePtr<gl::SurfaceFactory> factory =
        gl::GLScreenBuffer::CreateFactory(gl, caps, forwarder, flags);

      if (factory)
        screen->Morph(Move(factory));
    }
  }

  return result;
}

already_AddRefed<nsICanvasRenderingContextInternal>
OffscreenCanvas::CreateContext(CanvasContextType aContextType)
{
  RefPtr<nsICanvasRenderingContextInternal> ret =
    CanvasRenderingContextHelper::CreateContext(aContextType);

  ret->SetOffscreenCanvas(this);
  return ret.forget();
}

void
OffscreenCanvas::CommitFrameToCompositor()
{
  // The attributes has changed, we have to notify main
  // thread to change canvas size.
  if (mAttrDirty) {
    if (mCanvasRenderer) {
      mCanvasRenderer->SetWidth(mWidth);
      mCanvasRenderer->SetHeight(mHeight);
      mCanvasRenderer->NotifyElementAboutAttributesChanged();
    }
    mAttrDirty = false;
  }

  if (mCurrentContext) {
    static_cast<WebGLContext*>(mCurrentContext.get())->PresentScreenBuffer();
  }

  if (mCanvasRenderer && mCanvasRenderer->mGLContext) {
    mCanvasRenderer->NotifyElementAboutInvalidation();
    ImageBridgeChild::GetSingleton()->
      UpdateAsyncCanvasRenderer(mCanvasRenderer);
  }
}

OffscreenCanvasCloneData*
OffscreenCanvas::ToCloneData()
{
  return new OffscreenCanvasCloneData(mCanvasRenderer, mWidth, mHeight,
                                      mCompositorBackendType, mNeutered, mIsWriteOnly);
}

/* static */ already_AddRefed<OffscreenCanvas>
OffscreenCanvas::CreateFromCloneData(OffscreenCanvasCloneData* aData)
{
  MOZ_ASSERT(aData);
  RefPtr<OffscreenCanvas> wc =
    new OffscreenCanvas(aData->mWidth, aData->mHeight,
                        aData->mCompositorBackendType, aData->mRenderer);
  if (aData->mNeutered) {
    wc->SetNeutered();
  }
  return wc.forget();
}

/* static */ bool
OffscreenCanvas::PrefEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("gfx.offscreencanvas.enabled");
  } else {
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    MOZ_ASSERT(workerPrivate);
    return workerPrivate->OffscreenCanvasEnabled();
  }
}

/* static */ bool
OffscreenCanvas::PrefEnabledOnWorkerThread(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return true;
  }

  return PrefEnabled(aCx, aObj);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(OffscreenCanvas, DOMEventTargetHelper, mCurrentContext)

NS_IMPL_ADDREF_INHERITED(OffscreenCanvas, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(OffscreenCanvas, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(OffscreenCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

} // namespace dom
} // namespace mozilla

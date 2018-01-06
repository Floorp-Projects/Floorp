/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvas.h"

#include "mozilla/dom/OffscreenCanvasBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
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

OffscreenCanvas::OffscreenCanvas(nsIGlobalObject* aGlobal,
                                 uint32_t aWidth,
                                 uint32_t aHeight,
                                 layers::LayersBackend aCompositorBackend,
                                 layers::AsyncCanvasRenderer* aRenderer)
  : DOMEventTargetHelper(aGlobal)
  , mAttrDirty(false)
  , mNeutered(false)
  , mIsWriteOnly(false)
  , mWidth(aWidth)
  , mHeight(aHeight)
  , mCompositorBackendType(aCompositorBackend)
  , mCanvasRenderer(aRenderer)
{}

OffscreenCanvas::~OffscreenCanvas()
{
  ClearResources();
}

JSObject*
OffscreenCanvas::WrapObject(JSContext* aCx,
                            JS::Handle<JSObject*> aGivenProto)
{
  return OffscreenCanvasBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<OffscreenCanvas>
OffscreenCanvas::Constructor(const GlobalObject& aGlobal,
                             uint32_t aWidth,
                             uint32_t aHeight,
                             ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<OffscreenCanvas> offscreenCanvas =
    new OffscreenCanvas(global, aWidth, aHeight,
                        layers::LayersBackend::LAYERS_NONE, nullptr);
  return offscreenCanvas.forget();
}

void
OffscreenCanvas::ClearResources()
{
  if (mCanvasClient) {
    mCanvasClient->Clear();

    if (mCanvasRenderer) {
      nsCOMPtr<nsISerialEventTarget> activeTarget = mCanvasRenderer->GetActiveEventTarget();
      MOZ_RELEASE_ASSERT(activeTarget, "GFX: failed to get active event target.");
      bool current;
      activeTarget->IsOnCurrentThread(&current);
      MOZ_RELEASE_ASSERT(current, "GFX: active thread is not current thread.");
      mCanvasRenderer->SetCanvasClient(nullptr);
      mCanvasRenderer->mContext = nullptr;
      mCanvasRenderer->mGLContext = nullptr;
      mCanvasRenderer->ResetActiveEventTarget();
    }

    mCanvasClient = nullptr;
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
        contextType == CanvasContextType::WebGL2 ||
        contextType == CanvasContextType::ImageBitmap))
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
    if (contextType == CanvasContextType::WebGL1 ||
        contextType == CanvasContextType::WebGL2) {
      WebGLContext* webGL = static_cast<WebGLContext*>(mCurrentContext.get());
      gl::GLContext* gl = webGL->GL();
      mCanvasRenderer->mContext = mCurrentContext;
      mCanvasRenderer->SetActiveEventTarget();
      mCanvasRenderer->mGLContext = gl;
      mCanvasRenderer->SetIsAlphaPremultiplied(webGL->IsPremultAlpha() || !gl->Caps().alpha);

      if (RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton()) {
        TextureFlags flags = TextureFlags::ORIGIN_BOTTOM_LEFT;
        mCanvasClient = imageBridge->CreateCanvasClient(CanvasClient::CanvasClientTypeShSurf, flags);
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
  if (!mCanvasRenderer) {
    // This offscreen canvas doesn't associate to any HTML canvas element.
    // So, just bail out.
    return;
  }

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

already_AddRefed<ImageBitmap>
OffscreenCanvas::TransferToImageBitmap(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> globalObject = GetGlobalObject();
  RefPtr<ImageBitmap> result = ImageBitmap::CreateFromOffscreenCanvas(globalObject, *this, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // TODO: Clear the content?
  return result.forget();
}

already_AddRefed<Promise>
OffscreenCanvas::ToBlob(JSContext* aCx,
                        const nsAString& aType,
                        JS::Handle<JS::Value> aParams,
                        ErrorResult& aRv)
{
  // do a trust check if this is a write-only canvas
  if (mIsWriteOnly) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = GetGlobalObject();

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Encoder callback when encoding is complete.
  class EncodeCallback : public EncodeCompleteCallback
  {
  public:
    EncodeCallback(nsIGlobalObject* aGlobal, Promise* aPromise)
      : mGlobal(aGlobal)
      , mPromise(aPromise) {}

    // This is called on main thread.
    nsresult ReceiveBlob(already_AddRefed<Blob> aBlob)
    {
      RefPtr<Blob> blob = aBlob;

      if (mPromise) {
        RefPtr<Blob> newBlob = Blob::Create(mGlobal, blob->Impl());
        mPromise->MaybeResolve(newBlob);
      }

      mGlobal = nullptr;
      mPromise = nullptr;

      return NS_OK;
    }

    nsCOMPtr<nsIGlobalObject> mGlobal;
    RefPtr<Promise> mPromise;
  };

  RefPtr<EncodeCompleteCallback> callback =
    new EncodeCallback(global, promise);

  // TODO: Can we obtain the context and document here somehow
  // so that we can decide when usePlaceholder should be true/false?
  // See https://trac.torproject.org/18599
  // For now, we always return a placeholder if fingerprinting resistance is on.
  bool usePlaceholder = nsContentUtils::ShouldResistFingerprinting();
  CanvasRenderingContextHelper::ToBlob(aCx, global, callback, aType, aParams,
                                       usePlaceholder, aRv);

  return promise.forget();
}

already_AddRefed<gfx::SourceSurface>
OffscreenCanvas::GetSurfaceSnapshot(gfxAlphaType* const aOutAlphaType)
{
  if (!mCurrentContext) {
    return nullptr;
  }

  return mCurrentContext->GetSurfaceSnapshot(aOutAlphaType);
}

nsCOMPtr<nsIGlobalObject>
OffscreenCanvas::GetGlobalObject()
{
  if (NS_IsMainThread()) {
    return GetParentObject();
  }

  dom::workers::WorkerPrivate* workerPrivate =
    dom::workers::GetCurrentThreadWorkerPrivate();
  return workerPrivate->GlobalScope();
}

/* static */ already_AddRefed<OffscreenCanvas>
OffscreenCanvas::CreateFromCloneData(nsIGlobalObject* aGlobal, OffscreenCanvasCloneData* aData)
{
  MOZ_ASSERT(aData);
  RefPtr<OffscreenCanvas> wc =
    new OffscreenCanvas(aGlobal, aData->mWidth, aData->mHeight,
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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(OffscreenCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

} // namespace dom
} // namespace mozilla

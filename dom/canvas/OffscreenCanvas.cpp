/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvas.h"

#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/OffscreenCanvasBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/layers/CanvasRenderer.h"
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/Telemetry.h"
#include "mozilla/webgpu/CanvasContext.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasUtils.h"
#include "ClientWebGLContext.h"
#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "nsContentUtils.h"

namespace mozilla::dom {

OffscreenCanvasCloneData::OffscreenCanvasCloneData(
    layers::CanvasRenderer* aRenderer, uint32_t aWidth, uint32_t aHeight,
    layers::LayersBackend aCompositorBackend, bool aNeutered, bool aIsWriteOnly)
    : mRenderer(aRenderer),
      mWidth(aWidth),
      mHeight(aHeight),
      mCompositorBackendType(aCompositorBackend),
      mNeutered(aNeutered),
      mIsWriteOnly(aIsWriteOnly) {}

OffscreenCanvasCloneData::~OffscreenCanvasCloneData() = default;

OffscreenCanvas::OffscreenCanvas(nsIGlobalObject* aGlobal, uint32_t aWidth,
                                 uint32_t aHeight,
                                 layers::LayersBackend aCompositorBackend,
                                 layers::CanvasRenderer* aRenderer)
    : DOMEventTargetHelper(aGlobal),
      mAttrDirty(false),
      mNeutered(false),
      mIsWriteOnly(false),
      mWidth(aWidth),
      mHeight(aHeight),
      mCompositorBackendType(aCompositorBackend),
      mCanvasRenderer(aRenderer) {}

OffscreenCanvas::~OffscreenCanvas() { ClearResources(); }

JSObject* OffscreenCanvas::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return OffscreenCanvas_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<OffscreenCanvas> OffscreenCanvas::Constructor(
    const GlobalObject& aGlobal, uint32_t aWidth, uint32_t aHeight) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<OffscreenCanvas> offscreenCanvas = new OffscreenCanvas(
      global, aWidth, aHeight, layers::LayersBackend::LAYERS_NONE, nullptr);
  return offscreenCanvas.forget();
}

void OffscreenCanvas::ClearResources() {
  if (mCanvasClient) {
    mCanvasClient->Clear();

    MOZ_CRASH("todo");
    // if (mCanvasRenderer) {
    //  nsCOMPtr<nsISerialEventTarget> activeTarget =
    //      mCanvasRenderer->GetActiveEventTarget();
    //  MOZ_RELEASE_ASSERT(activeTarget,
    //                     "GFX: failed to get active event target.");
    //  bool current;
    //  activeTarget->IsOnCurrentThread(&current);
    //  MOZ_RELEASE_ASSERT(current, "GFX: active thread is not current
    //  thread."); mCanvasRenderer->SetCanvasClient(nullptr);
    //  mCanvasRenderer->mContext = nullptr;
    //  mCanvasRenderer->mGLContext = nullptr;
    //  mCanvasRenderer->ResetActiveEventTarget();
    //}

    mCanvasClient = nullptr;
  }
}

void OffscreenCanvas::GetContext(
    JSContext* aCx, const OffscreenRenderingContextId& aContextId,
    JS::Handle<JS::Value> aContextOptions,
    Nullable<OwningOffscreenRenderingContext>& aResult, ErrorResult& aRv) {
  if (mNeutered) {
    aResult.SetNull();
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  CanvasContextType contextType;
  switch (aContextId) {
    case OffscreenRenderingContextId::Bitmaprenderer:
      contextType = CanvasContextType::ImageBitmap;
      break;
    case OffscreenRenderingContextId::Webgl:
      contextType = CanvasContextType::WebGL1;
      break;
    case OffscreenRenderingContextId::Webgl2:
      contextType = CanvasContextType::WebGL2;
      break;
    case OffscreenRenderingContextId::Webgpu:
      contextType = CanvasContextType::WebGPU;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled canvas type!");
      aResult.SetNull();
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
      return;
  }

  RefPtr<nsISupports> result = CanvasRenderingContextHelper::GetOrCreateContext(
      aCx, contextType, aContextOptions, aRv);
  if (!result) {
    aResult.SetNull();
    return;
  }

  MOZ_ASSERT(mCurrentContext);

  if (mCanvasRenderer) {
    // mCanvasRenderer->SetContextType(contextType);
    if (contextType == CanvasContextType::WebGL1 ||
        contextType == CanvasContextType::WebGL2) {
      MOZ_ASSERT_UNREACHABLE("WebGL OffscreenCanvas not yet supported.");
      aResult.SetNull();
      return;
    }
    if (contextType == CanvasContextType::WebGPU) {
      MOZ_ASSERT_UNREACHABLE("WebGPU OffscreenCanvas not yet supported.");
      aResult.SetNull();
      return;
    }
  }

  switch (mCurrentContextType) {
    case CanvasContextType::ImageBitmap:
      aResult.SetValue().SetAsImageBitmapRenderingContext() =
          *static_cast<ImageBitmapRenderingContext*>(mCurrentContext.get());
      break;
    case CanvasContextType::WebGL1:
    case CanvasContextType::WebGL2:
      aResult.SetValue().SetAsWebGLRenderingContext() =
          *static_cast<ClientWebGLContext*>(mCurrentContext.get());
      break;
    case CanvasContextType::WebGPU:
      aResult.SetValue().SetAsGPUCanvasContext() =
          *static_cast<webgpu::CanvasContext*>(mCurrentContext.get());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled canvas type!");
      aResult.SetNull();
      break;
  }
}

layers::ImageContainer* OffscreenCanvas::GetImageContainer() {
  if (!mCanvasRenderer) {
    return nullptr;
  }
  // return mCanvasRenderer->GetImageContainer();
  MOZ_CRASH("todo");
}

already_AddRefed<nsICanvasRenderingContextInternal>
OffscreenCanvas::CreateContext(CanvasContextType aContextType) {
  RefPtr<nsICanvasRenderingContextInternal> ret =
      CanvasRenderingContextHelper::CreateContext(aContextType);

  ret->SetOffscreenCanvas(this);
  return ret.forget();
}

void OffscreenCanvas::CommitFrameToCompositor() {
  if (!mCanvasRenderer) {
    // This offscreen canvas doesn't associate to any HTML canvas element.
    // So, just bail out.
    return;
  }
  MOZ_CRASH("todo");

  // The attributes has changed, we have to notify main
  // thread to change canvas size.
  if (mAttrDirty) {
    MOZ_CRASH("todo");
    // if (mCanvasRenderer) {
    //  mCanvasRenderer->SetWidth(mWidth);
    //  mCanvasRenderer->SetHeight(mHeight);
    //  mCanvasRenderer->NotifyElementAboutAttributesChanged();
    //}
    mAttrDirty = false;
  }

  // CanvasContextType contentType = mCanvasRenderer->GetContextType();
  // if (mCurrentContext && (contentType == CanvasContextType::WebGL1 ||
  //                        contentType == CanvasContextType::WebGL2)) {
  //  MOZ_ASSERT_UNREACHABLE("WebGL OffscreenCanvas not yet supported.");
  //  return;
  //}
  // if (mCurrentContext && (contentType == CanvasContextType::WebGPU)) {
  //  MOZ_ASSERT_UNREACHABLE("WebGPU OffscreenCanvas not yet supported.");
  //  return;
  //}

  // if (mCanvasRenderer && mCanvasRenderer->mGLContext) {
  //  mCanvasRenderer->NotifyElementAboutInvalidation();
  //  ImageBridgeChild::GetSingleton()->UpdateAsyncCanvasRenderer(
  //      mCanvasRenderer);
  //}
}

OffscreenCanvasCloneData* OffscreenCanvas::ToCloneData() {
  return new OffscreenCanvasCloneData(mCanvasRenderer, mWidth, mHeight,
                                      mCompositorBackendType, mNeutered,
                                      mIsWriteOnly);
}

already_AddRefed<ImageBitmap> OffscreenCanvas::TransferToImageBitmap(
    ErrorResult& aRv) {
  RefPtr<ImageBitmap> result =
      ImageBitmap::CreateFromOffscreenCanvas(GetOwnerGlobal(), *this, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // TODO: Clear the content?
  return result.forget();
}

already_AddRefed<EncodeCompleteCallback>
OffscreenCanvas::CreateEncodeCompleteCallback(
    nsCOMPtr<nsIGlobalObject>&& aGlobal, Promise* aPromise) {
  // Encoder callback when encoding is complete.
  class EncodeCallback : public EncodeCompleteCallback {
   public:
    EncodeCallback(nsCOMPtr<nsIGlobalObject>&& aGlobal, Promise* aPromise)
        : mGlobal(std::move(aGlobal)), mPromise(aPromise) {}

    // This is called on main thread.
    nsresult ReceiveBlobImpl(already_AddRefed<BlobImpl> aBlobImpl) override {
      RefPtr<BlobImpl> blobImpl = aBlobImpl;

      if (mPromise) {
        if (NS_WARN_IF(!blobImpl)) {
          mPromise->MaybeReject(NS_ERROR_FAILURE);
        } else {
          RefPtr<Blob> blob = Blob::Create(mGlobal, blobImpl);
          if (NS_WARN_IF(!blob)) {
            mPromise->MaybeReject(NS_ERROR_FAILURE);
          } else {
            mPromise->MaybeResolve(blob);
          }
        }
      }

      mGlobal = nullptr;
      mPromise = nullptr;

      return NS_OK;
    }

    nsCOMPtr<nsIGlobalObject> mGlobal;
    RefPtr<Promise> mPromise;
  };

  return MakeAndAddRef<EncodeCallback>(std::move(aGlobal), aPromise);
}

already_AddRefed<Promise> OffscreenCanvas::ConvertToBlob(
    const ImageEncodeOptions& aOptions, ErrorResult& aRv) {
  // do a trust check if this is a write-only canvas
  if (mIsWriteOnly) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsAutoString type;
  nsContentUtils::ASCIIToLower(aOptions.mType, type);

  nsAutoString encodeOptions;
  if (aOptions.mQuality.WasPassed()) {
    encodeOptions.AppendLiteral("quality=");
    encodeOptions.AppendInt(NS_lround(aOptions.mQuality.Value() * 100.0));
  }

  RefPtr<EncodeCompleteCallback> callback =
      CreateEncodeCompleteCallback(std::move(global), promise);
  bool usePlaceholder = ShouldResistFingerprinting();
  CanvasRenderingContextHelper::ToBlob(callback, type, encodeOptions,
                                       /* aUsingCustomOptions */ false,
                                       usePlaceholder, aRv);

  return promise.forget();
}

already_AddRefed<Promise> OffscreenCanvas::ToBlob(JSContext* aCx,
                                                  const nsAString& aType,
                                                  JS::Handle<JS::Value> aParams,
                                                  ErrorResult& aRv) {
  // do a trust check if this is a write-only canvas
  if (mIsWriteOnly) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<EncodeCompleteCallback> callback =
      CreateEncodeCompleteCallback(std::move(global), promise);
  bool usePlaceholder = ShouldResistFingerprinting();
  CanvasRenderingContextHelper::ToBlob(aCx, callback, aType, aParams,
                                       usePlaceholder, aRv);

  return promise.forget();
}

already_AddRefed<gfx::SourceSurface> OffscreenCanvas::GetSurfaceSnapshot(
    gfxAlphaType* const aOutAlphaType) {
  if (!mCurrentContext) {
    return nullptr;
  }

  return mCurrentContext->GetSurfaceSnapshot(aOutAlphaType);
}

bool OffscreenCanvas::ShouldResistFingerprinting() const {
  return nsContentUtils::ShouldResistFingerprinting(GetOwnerGlobal());
}

/* static */
already_AddRefed<OffscreenCanvas> OffscreenCanvas::CreateFromCloneData(
    nsIGlobalObject* aGlobal, OffscreenCanvasCloneData* aData) {
  MOZ_ASSERT(aData);
  RefPtr<OffscreenCanvas> wc =
      new OffscreenCanvas(aGlobal, aData->mWidth, aData->mHeight,
                          aData->mCompositorBackendType, aData->mRenderer);
  if (aData->mNeutered) {
    wc->SetNeutered();
  }
  return wc.forget();
}

/* static */
bool OffscreenCanvas::PrefEnabledOnWorkerThread(JSContext* aCx,
                                                JSObject* aObj) {
  if (NS_IsMainThread()) {
    return true;
  }

  return StaticPrefs::gfx_offscreencanvas_enabled();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(OffscreenCanvas, DOMEventTargetHelper,
                                   mCurrentContext)

NS_IMPL_ADDREF_INHERITED(OffscreenCanvas, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(OffscreenCanvas, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(OffscreenCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

}  // namespace mozilla::dom

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvas.h"

#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/OffscreenCanvasBinding.h"
#include "mozilla/dom/OffscreenCanvasDisplayHelper.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
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
#include "WebGLChild.h"

namespace mozilla::dom {

OffscreenCanvasCloneData::OffscreenCanvasCloneData(
    OffscreenCanvasDisplayHelper* aDisplay, uint32_t aWidth, uint32_t aHeight,
    layers::LayersBackend aCompositorBackend, layers::TextureType aTextureType,
    bool aNeutered, bool aIsWriteOnly)
    : mDisplay(aDisplay),
      mWidth(aWidth),
      mHeight(aHeight),
      mCompositorBackendType(aCompositorBackend),
      mTextureType(aTextureType),
      mNeutered(aNeutered),
      mIsWriteOnly(aIsWriteOnly) {}

OffscreenCanvasCloneData::~OffscreenCanvasCloneData() = default;

OffscreenCanvas::OffscreenCanvas(nsIGlobalObject* aGlobal, uint32_t aWidth,
                                 uint32_t aHeight,
                                 layers::LayersBackend aCompositorBackend,
                                 layers::TextureType aTextureType,
                                 OffscreenCanvasDisplayHelper* aDisplay)
    : DOMEventTargetHelper(aGlobal),
      mAttrDirty(false),
      mNeutered(false),
      mIsWriteOnly(false),
      mWidth(aWidth),
      mHeight(aHeight),
      mCompositorBackendType(aCompositorBackend),
      mTextureType(aTextureType),
      mDisplay(aDisplay) {}

OffscreenCanvas::~OffscreenCanvas() = default;

JSObject* OffscreenCanvas::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return OffscreenCanvas_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<OffscreenCanvas> OffscreenCanvas::Constructor(
    const GlobalObject& aGlobal, uint32_t aWidth, uint32_t aHeight) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<OffscreenCanvas> offscreenCanvas = new OffscreenCanvas(
      global, aWidth, aHeight, layers::LayersBackend::LAYERS_NONE,
      layers::TextureType::Unknown, nullptr);
  return offscreenCanvas.forget();
}

void OffscreenCanvas::SetWidth(uint32_t aWidth, ErrorResult& aRv) {
  if (mNeutered) {
    aRv.ThrowInvalidStateError(
        "Cannot set width of placeholder canvas transferred to worker.");
    return;
  }

  if (mWidth != aWidth) {
    mWidth = aWidth;
    CanvasAttrChanged();
  }
}

void OffscreenCanvas::SetHeight(uint32_t aHeight, ErrorResult& aRv) {
  if (mNeutered) {
    aRv.ThrowInvalidStateError(
        "Cannot set height of placeholder canvas transferred to worker.");
    return;
  }

  if (mHeight != aHeight) {
    mHeight = aHeight;
    CanvasAttrChanged();
  }
}

void OffscreenCanvas::UpdateNeuteredSize(uint32_t aWidth, uint32_t aHeight) {
  MOZ_ASSERT(mNeutered);
  MOZ_ASSERT(!mCurrentContext);
  mWidth = aWidth;
  mHeight = aHeight;
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

  int32_t childId = 0;

  MOZ_ASSERT(mCurrentContext);
  switch (mCurrentContextType) {
    case CanvasContextType::ImageBitmap:
      aResult.SetValue().SetAsImageBitmapRenderingContext() =
          *static_cast<ImageBitmapRenderingContext*>(mCurrentContext.get());
      break;
    case CanvasContextType::WebGL1:
    case CanvasContextType::WebGL2: {
      auto* webgl = static_cast<ClientWebGLContext*>(mCurrentContext.get());
      WebGLChild* webglChild = webgl->GetChild();
      if (webglChild) {
        childId = webglChild->Id();
      }
      aResult.SetValue().SetAsWebGLRenderingContext() = *webgl;
      break;
    }
    case CanvasContextType::WebGPU:
      aResult.SetValue().SetAsGPUCanvasContext() =
          *static_cast<webgpu::CanvasContext*>(mCurrentContext.get());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled canvas type!");
      aResult.SetNull();
      break;
  }

  if (mDisplay) {
    mDisplay->UpdateContext(mCurrentContextType, childId);
  }
}

already_AddRefed<nsICanvasRenderingContextInternal>
OffscreenCanvas::CreateContext(CanvasContextType aContextType) {
  RefPtr<nsICanvasRenderingContextInternal> ret =
      CanvasRenderingContextHelper::CreateContext(aContextType);

  ret->SetOffscreenCanvas(this);
  return ret.forget();
}

void OffscreenCanvas::UpdateDisplayData(
    const OffscreenCanvasDisplayData& aData) {
  if (!mDisplay) {
    return;
  }

  mPendingUpdate = Some(aData);
  QueueCommitToCompositor();
}

void OffscreenCanvas::QueueCommitToCompositor() {
  if (!mDisplay || !mCurrentContext || mPendingCommit) {
    // If we already have a commit pending, or we have no bound display/context,
    // just bail out.
    return;
  }

  mPendingCommit = NS_NewCancelableRunnableFunction(
      "OffscreenCanvas::QueueCommitToCompositor",
      [self = RefPtr{this}] { self->DequeueCommitToCompositor(); });
  NS_DispatchToCurrentThread(mPendingCommit);
}

void OffscreenCanvas::DequeueCommitToCompositor() {
  MOZ_ASSERT(mPendingCommit);
  mPendingCommit = nullptr;
  Maybe<OffscreenCanvasDisplayData> update = std::move(mPendingUpdate);
  mDisplay->CommitFrameToCompositor(mCurrentContext, mTextureType, update);
}

void OffscreenCanvas::CommitFrameToCompositor() {
  if (!mDisplay || !mCurrentContext) {
    // This offscreen canvas doesn't associate to any HTML canvas element.
    // So, just bail out.
    return;
  }

  if (mPendingCommit) {
    // We got an explicit commit while waiting for an implicit.
    mPendingCommit->Cancel();
    mPendingCommit = nullptr;
  }

  Maybe<OffscreenCanvasDisplayData> update = std::move(mPendingUpdate);
  mDisplay->CommitFrameToCompositor(mCurrentContext, mTextureType, update);
}

OffscreenCanvasCloneData* OffscreenCanvas::ToCloneData() {
  return new OffscreenCanvasCloneData(mDisplay, mWidth, mHeight,
                                      mCompositorBackendType, mTextureType,
                                      mNeutered, mIsWriteOnly);
}

already_AddRefed<ImageBitmap> OffscreenCanvas::TransferToImageBitmap(
    ErrorResult& aRv) {
  RefPtr<ImageBitmap> result =
      ImageBitmap::CreateFromOffscreenCanvas(GetOwnerGlobal(), *this, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (result && mCurrentContext) {
    // FIXME(aosmond): The spec is unclear about the state of the canvas after
    // clearing. Does it expect to preserve the WebGL state, other than the
    // buffer state? Once we have clarity, we should ensure we clear the WebGL
    // canvas as desired.
    mCurrentContext->Reset();
    mCurrentContext->SetDimensions(result->Width(), result->Height());
  }
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
  RefPtr<OffscreenCanvas> wc = new OffscreenCanvas(
      aGlobal, aData->mWidth, aData->mHeight, aData->mCompositorBackendType,
      aData->mTextureType, aData->mDisplay);
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

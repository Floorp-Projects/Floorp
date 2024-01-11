/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvas.h"

#include "mozilla/Atomics.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/OffscreenCanvasBinding.h"
#include "mozilla/dom/OffscreenCanvasDisplayHelper.h"
#include "mozilla/dom/OffscreenCanvasRenderingContext2D.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
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
#include "nsProxyRelease.h"
#include "WebGLChild.h"

namespace mozilla::dom {

OffscreenCanvasCloneData::OffscreenCanvasCloneData(
    OffscreenCanvasDisplayHelper* aDisplay, uint32_t aWidth, uint32_t aHeight,
    layers::LayersBackend aCompositorBackend, layers::TextureType aTextureType,
    bool aNeutered, bool aIsWriteOnly, nsIPrincipal* aExpandedReader)
    : mDisplay(aDisplay),
      mWidth(aWidth),
      mHeight(aHeight),
      mCompositorBackendType(aCompositorBackend),
      mTextureType(aTextureType),
      mNeutered(aNeutered),
      mIsWriteOnly(aIsWriteOnly),
      mExpandedReader(aExpandedReader) {}

OffscreenCanvasCloneData::~OffscreenCanvasCloneData() {
  NS_ReleaseOnMainThread("OffscreenCanvasCloneData::mExpandedReader",
                         mExpandedReader.forget());
}

OffscreenCanvas::OffscreenCanvas(nsIGlobalObject* aGlobal, uint32_t aWidth,
                                 uint32_t aHeight)
    : DOMEventTargetHelper(aGlobal), mWidth(aWidth), mHeight(aHeight) {}

OffscreenCanvas::OffscreenCanvas(
    nsIGlobalObject* aGlobal, uint32_t aWidth, uint32_t aHeight,
    layers::LayersBackend aCompositorBackend, layers::TextureType aTextureType,
    already_AddRefed<OffscreenCanvasDisplayHelper> aDisplay)
    : DOMEventTargetHelper(aGlobal),
      mWidth(aWidth),
      mHeight(aHeight),
      mCompositorBackendType(aCompositorBackend),
      mTextureType(aTextureType),
      mDisplay(aDisplay) {}

OffscreenCanvas::~OffscreenCanvas() {
  Destroy();
  NS_ReleaseOnMainThread("OffscreenCanvas::mExpandedReader",
                         mExpandedReader.forget());
}

void OffscreenCanvas::Destroy() {
  if (mDisplay) {
    mDisplay->DestroyCanvas();
  }
}

JSObject* OffscreenCanvas::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return OffscreenCanvas_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<OffscreenCanvas> OffscreenCanvas::Constructor(
    const GlobalObject& aGlobal, uint32_t aWidth, uint32_t aHeight,
    ErrorResult& aRv) {
  // CanvasRenderingContextHelper::GetWidthHeight wants us to return
  // an nsIntSize, so make sure that that will work.
  if (!CheckedInt<int32_t>(aWidth).isValid()) {
    aRv.ThrowRangeError(
        nsPrintfCString("OffscreenCanvas width %u is out of range: must be no "
                        "greater than 2147483647.",
                        aWidth));
    return nullptr;
  }
  if (!CheckedInt<int32_t>(aHeight).isValid()) {
    aRv.ThrowRangeError(
        nsPrintfCString("OffscreenCanvas height %u is out of range: must be no "
                        "greater than 2147483647.",
                        aHeight));
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<OffscreenCanvas> offscreenCanvas =
      new OffscreenCanvas(global, aWidth, aHeight);
  return offscreenCanvas.forget();
}

void OffscreenCanvas::SetWidth(uint32_t aWidth, ErrorResult& aRv) {
  if (mNeutered) {
    aRv.ThrowInvalidStateError("Cannot set width of detached OffscreenCanvas.");
    return;
  }

  // CanvasRenderingContextHelper::GetWidthHeight wants us to return
  // an nsIntSize, so make sure that that will work.
  if (!CheckedInt<int32_t>(aWidth).isValid()) {
    aRv.ThrowRangeError(
        nsPrintfCString("OffscreenCanvas width %u is out of range: must be no "
                        "greater than 2147483647.",
                        aWidth));
    return;
  }

  mWidth = aWidth;
  CanvasAttrChanged();
}

void OffscreenCanvas::SetHeight(uint32_t aHeight, ErrorResult& aRv) {
  if (mNeutered) {
    aRv.ThrowInvalidStateError(
        "Cannot set height of detached OffscreenCanvas.");
    return;
  }

  // CanvasRenderingContextHelper::GetWidthHeight wants us to return
  // an nsIntSize, so make sure that that will work.
  if (!CheckedInt<int32_t>(aHeight).isValid()) {
    aRv.ThrowRangeError(
        nsPrintfCString("OffscreenCanvas height %u is out of range: must be no "
                        "greater than 2147483647.",
                        aHeight));
    return;
  }

  mHeight = aHeight;
  CanvasAttrChanged();
}

void OffscreenCanvas::SetSize(const nsIntSize& aSize, ErrorResult& aRv) {
  if (mNeutered) {
    aRv.ThrowInvalidStateError(
        "Cannot set dimensions of detached OffscreenCanvas.");
    return;
  }

  if (NS_WARN_IF(aSize.IsEmpty())) {
    aRv.ThrowRangeError("OffscreenCanvas size is empty, must be non-empty.");
    return;
  }

  mWidth = aSize.width;
  mHeight = aSize.height;
  CanvasAttrChanged();
}

void OffscreenCanvas::GetContext(
    JSContext* aCx, const OffscreenRenderingContextId& aContextId,
    JS::Handle<JS::Value> aContextOptions,
    Nullable<OwningOffscreenRenderingContext>& aResult, ErrorResult& aRv) {
  if (mNeutered) {
    aResult.SetNull();
    aRv.ThrowInvalidStateError(
        "Cannot create context for detached OffscreenCanvas.");
    return;
  }

  CanvasContextType contextType;
  switch (aContextId) {
    case OffscreenRenderingContextId::_2d:
      contextType = CanvasContextType::OffscreenCanvas2D;
      break;
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

  // If we are on a worker, we need to give our OffscreenCanvasDisplayHelper
  // object access to a worker ref so we can dispatch properly during painting
  // if we need to flush our contents to its ImageContainer for display.
  RefPtr<ThreadSafeWorkerRef> workerRef;
  if (mDisplay) {
    if (WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate()) {
      RefPtr<StrongWorkerRef> strongRef = StrongWorkerRef::Create(
          workerPrivate, "OffscreenCanvas::GetContext",
          [display = mDisplay]() { display->DestroyCanvas(); });
      if (NS_WARN_IF(!strongRef)) {
        aResult.SetNull();
        aRv.ThrowUnknownError("Worker shutting down");
        return;
      }

      workerRef = new ThreadSafeWorkerRef(strongRef);
    } else {
      MOZ_ASSERT(NS_IsMainThread());
    }
  }

  RefPtr<nsISupports> result = CanvasRenderingContextHelper::GetOrCreateContext(
      aCx, contextType, aContextOptions, aRv);
  if (!result) {
    aResult.SetNull();
    return;
  }

  Maybe<int32_t> childId;

  MOZ_ASSERT(mCurrentContext);
  switch (mCurrentContextType) {
    case CanvasContextType::OffscreenCanvas2D:
      aResult.SetValue().SetAsOffscreenCanvasRenderingContext2D() =
          *static_cast<OffscreenCanvasRenderingContext2D*>(
              mCurrentContext.get());
      break;
    case CanvasContextType::ImageBitmap:
      aResult.SetValue().SetAsImageBitmapRenderingContext() =
          *static_cast<ImageBitmapRenderingContext*>(mCurrentContext.get());
      break;
    case CanvasContextType::WebGL1:
    case CanvasContextType::WebGL2: {
      auto* webgl = static_cast<ClientWebGLContext*>(mCurrentContext.get());
      WebGLChild* webglChild = webgl->GetChild();
      if (webglChild) {
        childId.emplace(webglChild->Id());
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
    mDisplay->UpdateContext(this, std::move(workerRef), mCurrentContextType,
                            childId);
  }
}

already_AddRefed<nsICanvasRenderingContextInternal>
OffscreenCanvas::CreateContext(CanvasContextType aContextType) {
  RefPtr<nsICanvasRenderingContextInternal> ret =
      CanvasRenderingContextHelper::CreateContext(aContextType);
  if (NS_WARN_IF(!ret)) {
    return nullptr;
  }

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

UniquePtr<OffscreenCanvasCloneData> OffscreenCanvas::ToCloneData(
    JSContext* aCx) {
  if (NS_WARN_IF(mNeutered)) {
    ErrorResult rv;
    rv.ThrowDataCloneError(
        "Cannot clone OffscreenCanvas that is already transferred.");
    MOZ_ALWAYS_TRUE(rv.MaybeSetPendingException(aCx));
    return nullptr;
  }

  if (NS_WARN_IF(mCurrentContext)) {
    ErrorResult rv;
    rv.ThrowInvalidStateError("Cannot clone canvas with context.");
    MOZ_ALWAYS_TRUE(rv.MaybeSetPendingException(aCx));
    return nullptr;
  }

  auto cloneData = MakeUnique<OffscreenCanvasCloneData>(
      mDisplay, mWidth, mHeight, mCompositorBackendType, mTextureType,
      mNeutered, mIsWriteOnly, mExpandedReader);
  SetNeutered();
  return cloneData;
}

already_AddRefed<ImageBitmap> OffscreenCanvas::TransferToImageBitmap(
    ErrorResult& aRv) {
  if (mNeutered) {
    aRv.ThrowInvalidStateError(
        "Cannot get bitmap from detached OffscreenCanvas.");
    return nullptr;
  }

  if (!mCurrentContext) {
    aRv.ThrowInvalidStateError(
        "Cannot get bitmap from canvas without a context.");
    return nullptr;
  }

  RefPtr<ImageBitmap> result =
      ImageBitmap::CreateFromOffscreenCanvas(GetOwnerGlobal(), *this, aRv);
  if (!result) {
    return nullptr;
  }

  if (mCurrentContext) {
    mCurrentContext->ResetBitmap();
  }
  return result.forget();
}

already_AddRefed<EncodeCompleteCallback>
OffscreenCanvas::CreateEncodeCompleteCallback(Promise* aPromise) {
  // Encoder callback when encoding is complete.
  class EncodeCallback : public EncodeCompleteCallback {
   public:
    explicit EncodeCallback(Promise* aPromise)
        : mPromise(aPromise), mCanceled(false) {}

    void MaybeInitWorkerRef() {
      WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
      if (wp) {
        mWorkerRef = WeakWorkerRef::Create(
            wp, [self = RefPtr{this}]() { self->Cancel(); });
        if (!mWorkerRef) {
          Cancel();
        }
      }
    }

    nsresult ReceiveBlobImpl(already_AddRefed<BlobImpl> aBlobImpl) override {
      RefPtr<BlobImpl> blobImpl = aBlobImpl;
      mWorkerRef = nullptr;

      if (mPromise) {
        RefPtr<nsIGlobalObject> global = mPromise->GetGlobalObject();
        if (NS_WARN_IF(!global) || NS_WARN_IF(!blobImpl)) {
          mPromise->MaybeReject(NS_ERROR_FAILURE);
        } else {
          RefPtr<Blob> blob = Blob::Create(global, blobImpl);
          if (NS_WARN_IF(!blob)) {
            mPromise->MaybeReject(NS_ERROR_FAILURE);
          } else {
            mPromise->MaybeResolve(blob);
          }
        }
      }

      mPromise = nullptr;

      return NS_OK;
    }

    bool CanBeDeletedOnAnyThread() override { return mCanceled; }

    void Cancel() {
      mPromise = nullptr;
      mWorkerRef = nullptr;
      mCanceled = true;
    }

    RefPtr<Promise> mPromise;
    RefPtr<WeakWorkerRef> mWorkerRef;
    Atomic<bool> mCanceled;
  };

  RefPtr<EncodeCallback> p = MakeAndAddRef<EncodeCallback>(aPromise);
  p->MaybeInitWorkerRef();
  return p.forget();
}

already_AddRefed<Promise> OffscreenCanvas::ConvertToBlob(
    const ImageEncodeOptions& aOptions, ErrorResult& aRv) {
  // do a trust check if this is a write-only canvas
  if (mIsWriteOnly) {
    aRv.ThrowSecurityError("Cannot get blob from write-only canvas.");
    return nullptr;
  }

  if (mNeutered) {
    aRv.ThrowInvalidStateError(
        "Cannot get blob from detached OffscreenCanvas.");
    return nullptr;
  }

  if (mWidth == 0 || mHeight == 0) {
    aRv.ThrowIndexSizeError("Cannot get blob from empty canvas.");
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

  // Only image/jpeg and image/webp support the quality parameter.
  if (aOptions.mQuality.WasPassed() &&
      (type.EqualsLiteral("image/jpeg") || type.EqualsLiteral("image/webp"))) {
    encodeOptions.AppendLiteral("quality=");
    encodeOptions.AppendInt(NS_lround(aOptions.mQuality.Value() * 100.0));
  }

  RefPtr<EncodeCompleteCallback> callback =
      CreateEncodeCompleteCallback(promise);
  bool usePlaceholder =
      ShouldResistFingerprinting(RFPTarget::CanvasImageExtractionPrompt);
  CanvasRenderingContextHelper::ToBlob(callback, type, encodeOptions,
                                       /* aUsingCustomOptions */ false,
                                       usePlaceholder, aRv);
  if (aRv.Failed()) {
    promise->MaybeReject(std::move(aRv));
  }

  return promise.forget();
}

already_AddRefed<Promise> OffscreenCanvas::ToBlob(JSContext* aCx,
                                                  const nsAString& aType,
                                                  JS::Handle<JS::Value> aParams,
                                                  ErrorResult& aRv) {
  // do a trust check if this is a write-only canvas
  if (mIsWriteOnly) {
    aRv.ThrowSecurityError("Cannot get blob from write-only canvas.");
    return nullptr;
  }

  if (mNeutered) {
    aRv.ThrowInvalidStateError(
        "Cannot get blob from detached OffscreenCanvas.");
    return nullptr;
  }

  if (mWidth == 0 || mHeight == 0) {
    aRv.ThrowIndexSizeError("Cannot get blob from empty canvas.");
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<EncodeCompleteCallback> callback =
      CreateEncodeCompleteCallback(promise);
  bool usePlaceholder =
      ShouldResistFingerprinting(RFPTarget::CanvasImageExtractionPrompt);
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

void OffscreenCanvas::SetWriteOnly(RefPtr<nsIPrincipal>&& aExpandedReader) {
  NS_ReleaseOnMainThread("OffscreenCanvas::mExpandedReader",
                         mExpandedReader.forget());
  mExpandedReader = std::move(aExpandedReader);
  mIsWriteOnly = true;
}

bool OffscreenCanvas::CallerCanRead(nsIPrincipal& aPrincipal) const {
  if (!mIsWriteOnly) {
    return true;
  }

  // If mExpandedReader is set, this canvas was tainted only by
  // mExpandedReader's resources. So allow reading if the subject
  // principal subsumes mExpandedReader.
  if (mExpandedReader && aPrincipal.Subsumes(mExpandedReader)) {
    return true;
  }

  return nsContentUtils::PrincipalHasPermission(aPrincipal,
                                                nsGkAtoms::all_urlsPermission);
}

bool OffscreenCanvas::ShouldResistFingerprinting(RFPTarget aTarget) const {
  return nsContentUtils::ShouldResistFingerprinting(GetOwnerGlobal(), aTarget);
}

/* static */
already_AddRefed<OffscreenCanvas> OffscreenCanvas::CreateFromCloneData(
    nsIGlobalObject* aGlobal, OffscreenCanvasCloneData* aData) {
  MOZ_ASSERT(aData);
  RefPtr<OffscreenCanvas> wc = new OffscreenCanvas(
      aGlobal, aData->mWidth, aData->mHeight, aData->mCompositorBackendType,
      aData->mTextureType, aData->mDisplay.forget());
  if (aData->mNeutered) {
    wc->SetNeutered();
  }
  if (aData->mIsWriteOnly) {
    wc->SetWriteOnly(std::move(aData->mExpandedReader));
  }
  return wc.forget();
}

/* static */
bool OffscreenCanvas::PrefEnabledOnWorkerThread(JSContext* aCx,
                                                JSObject* aObj) {
  return NS_IsMainThread() || StaticPrefs::gfx_offscreencanvas_enabled();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(OffscreenCanvas, DOMEventTargetHelper,
                                   mCurrentContext)

NS_IMPL_ADDREF_INHERITED(OffscreenCanvas, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(OffscreenCanvas, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(OffscreenCanvas)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, EventTarget)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

}  // namespace mozilla::dom

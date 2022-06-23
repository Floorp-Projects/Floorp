/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_OFFSCREENCANVAS_H_
#define MOZILLA_DOM_OFFSCREENCANVAS_H_

#include "gfxTypes.h"
#include "mozilla/dom/CanvasRenderingContextHelper.h"
#include "mozilla/dom/ImageEncoder.h"
#include "mozilla/dom/OffscreenCanvasDisplayHelper.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"

struct JSContext;

namespace mozilla {
class CancelableRunnable;
class ErrorResult;

namespace gfx {
class SourceSurface;
}

namespace dom {
enum class OffscreenRenderingContextId : uint8_t;
class Blob;
class EncodeCompleteCallback;
class OffscreenCanvasDisplayHelper;
class ImageBitmap;
struct ImageEncodeOptions;

using OwningOffscreenRenderingContext = class
    OwningOffscreenCanvasRenderingContext2DOrImageBitmapRenderingContextOrWebGLRenderingContextOrWebGL2RenderingContextOrGPUCanvasContext;

// This is helper class for transferring OffscreenCanvas to worker thread.
// Because OffscreenCanvas is not thread-safe. So we cannot pass Offscreen-
// Canvas to worker thread directly. Thus, we create this helper class and
// store necessary data in it then pass it to worker thread.
struct OffscreenCanvasCloneData final {
  OffscreenCanvasCloneData(OffscreenCanvasDisplayHelper* aDisplay,
                           uint32_t aWidth, uint32_t aHeight,
                           layers::LayersBackend aCompositorBackend,
                           layers::TextureType aTextureType, bool aNeutered,
                           bool aIsWriteOnly);
  ~OffscreenCanvasCloneData();

  RefPtr<OffscreenCanvasDisplayHelper> mDisplay;
  uint32_t mWidth;
  uint32_t mHeight;
  layers::LayersBackend mCompositorBackendType;
  layers::TextureType mTextureType;
  bool mNeutered;
  bool mIsWriteOnly;
};

class OffscreenCanvas final : public DOMEventTargetHelper,
                              public CanvasRenderingContextHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(OffscreenCanvas,
                                           DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(contextlost);
  IMPL_EVENT_HANDLER(contextrestored);

  OffscreenCanvas(nsIGlobalObject* aGlobal, uint32_t aWidth, uint32_t aHeight,
                  layers::LayersBackend aCompositorBackend,
                  layers::TextureType aTextureType,
                  OffscreenCanvasDisplayHelper* aDisplay);

  nsIGlobalObject* GetParentObject() const { return GetOwnerGlobal(); }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<OffscreenCanvas> Constructor(
      const GlobalObject& aGlobal, uint32_t aWidth, uint32_t aHeight);

  void ClearResources();

  uint32_t Width() const { return mWidth; }
  uint32_t Height() const { return mHeight; }
  void SetWidth(uint32_t aWidth, ErrorResult& aRv);
  void SetHeight(uint32_t aHeight, ErrorResult& aRv);

  void GetContext(JSContext* aCx, const OffscreenRenderingContextId& aContextId,
                  JS::Handle<JS::Value> aContextOptions,
                  Nullable<OwningOffscreenRenderingContext>& aResult,
                  ErrorResult& aRv);

  already_AddRefed<ImageBitmap> TransferToImageBitmap(ErrorResult& aRv);

  already_AddRefed<Promise> ConvertToBlob(const ImageEncodeOptions& aOptions,
                                          ErrorResult& aRv);

  already_AddRefed<Promise> ToBlob(JSContext* aCx, const nsAString& aType,
                                   JS::Handle<JS::Value> aParams,
                                   ErrorResult& aRv);

  nsICanvasRenderingContextInternal* GetContext() const {
    return mCurrentContext;
  }

  CanvasContextType GetContextType() const { return mCurrentContextType; }

  already_AddRefed<gfx::SourceSurface> GetSurfaceSnapshot(
      gfxAlphaType* aOutAlphaType = nullptr);

  static already_AddRefed<OffscreenCanvas> CreateFromCloneData(
      nsIGlobalObject* aGlobal, OffscreenCanvasCloneData* aData);

  // Return true on main-thread, and return gfx.offscreencanvas.enabled
  // on worker thread.
  static bool PrefEnabledOnWorkerThread(JSContext* aCx, JSObject* aObj);

  OffscreenCanvasCloneData* ToCloneData();

  void UpdateDisplayData(const OffscreenCanvasDisplayData& aData);

  void CommitFrameToCompositor();
  void DequeueCommitToCompositor();
  void QueueCommitToCompositor();

  virtual bool GetOpaqueAttr() override { return false; }

  virtual nsIntSize GetWidthHeight() override {
    return nsIntSize(mWidth, mHeight);
  }

  virtual already_AddRefed<nsICanvasRenderingContextInternal> CreateContext(
      CanvasContextType aContextType) override;

  void SetNeutered() {
    mWidth = 0;
    mHeight = 0;
    mNeutered = true;
  }

  bool MayNeuter() const { return !mNeutered && !mCurrentContext; }

  void SetWriteOnly() { mIsWriteOnly = true; }

  bool IsWriteOnly() const { return mIsWriteOnly; }

  layers::LayersBackend GetCompositorBackendType() const {
    return mCompositorBackendType;
  }

  bool ShouldResistFingerprinting() const;

  bool IsTransferredFromElement() const { return !!mDisplay; }

 private:
  ~OffscreenCanvas();

  already_AddRefed<EncodeCompleteCallback> CreateEncodeCompleteCallback(
      Promise* aPromise);

  void CanvasAttrChanged() {
    ErrorResult dummy;
    UpdateContext(nullptr, JS::NullHandleValue, dummy);
  }

  bool mNeutered;
  bool mIsWriteOnly;

  uint32_t mWidth;
  uint32_t mHeight;

  layers::LayersBackend mCompositorBackendType;
  layers::TextureType mTextureType;

  RefPtr<OffscreenCanvasDisplayHelper> mDisplay;
  RefPtr<CancelableRunnable> mPendingCommit;
  Maybe<OffscreenCanvasDisplayData> mPendingUpdate;
};

}  // namespace dom
}  // namespace mozilla

#endif  // MOZILLA_DOM_OFFSCREENCANVAS_H_

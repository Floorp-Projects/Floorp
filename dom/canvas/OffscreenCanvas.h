/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_OFFSCREENCANVAS_H_
#define MOZILLA_DOM_OFFSCREENCANVAS_H_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/RefPtr.h"
#include "CanvasRenderingContextHelper.h"
#include "nsCycleCollectionParticipant.h"

struct JSContext;

namespace mozilla {

class ErrorResult;

namespace layers {
class AsyncCanvasRenderer;
class CanvasClient;
} // namespace layers

namespace dom {
class Blob;
class ImageBitmap;

// This is helper class for transferring OffscreenCanvas to worker thread.
// Because OffscreenCanvas is not thread-safe. So we cannot pass Offscreen-
// Canvas to worker thread directly. Thus, we create this helper class and
// store necessary data in it then pass it to worker thread.
struct OffscreenCanvasCloneData final
{
  OffscreenCanvasCloneData(layers::AsyncCanvasRenderer* aRenderer,
                           uint32_t aWidth, uint32_t aHeight,
                           layers::LayersBackend aCompositorBackend,
                           bool aNeutered, bool aIsWriteOnly);
  ~OffscreenCanvasCloneData();

  RefPtr<layers::AsyncCanvasRenderer> mRenderer;
  uint32_t mWidth;
  uint32_t mHeight;
  layers::LayersBackend mCompositorBackendType;
  bool mNeutered;
  bool mIsWriteOnly;
};

class OffscreenCanvas final : public DOMEventTargetHelper
                            , public CanvasRenderingContextHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(OffscreenCanvas, DOMEventTargetHelper)

  OffscreenCanvas(nsIGlobalObject* aGlobal,
                  uint32_t aWidth,
                  uint32_t aHeight,
                  layers::LayersBackend aCompositorBackend,
                  layers::AsyncCanvasRenderer* aRenderer);

  nsCOMPtr<nsIGlobalObject> GetParentObject() const { return GetOwnerGlobal(); }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<OffscreenCanvas>
  Constructor(const GlobalObject& aGlobal,
              uint32_t aWidth,
              uint32_t aHeight,
              ErrorResult& aRv);

  void ClearResources();

  uint32_t Width() const
  {
    return mWidth;
  }

  uint32_t Height() const
  {
    return mHeight;
  }

  void SetWidth(uint32_t aWidth, ErrorResult& aRv)
  {
    if (mNeutered) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    if (mWidth != aWidth) {
      mWidth = aWidth;
      CanvasAttrChanged();
    }
  }

  void SetHeight(uint32_t aHeight, ErrorResult& aRv)
  {
    if (mNeutered) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    if (mHeight != aHeight) {
      mHeight = aHeight;
      CanvasAttrChanged();
    }
  }

  already_AddRefed<ImageBitmap>
  TransferToImageBitmap();

  already_AddRefed<Promise>
  ToBlob(JSContext* aCx,
         const nsAString& aType,
         JS::Handle<JS::Value> aParams,
         ErrorResult& aRv);

  nsICanvasRenderingContextInternal* GetContext() const
  {
    return mCurrentContext;
  }

  already_AddRefed<gfx::SourceSurface> GetSurfaceSnapshot(bool* aPremultAlpha = nullptr);

  static already_AddRefed<OffscreenCanvas>
  CreateFromCloneData(nsIGlobalObject* aGlobal, OffscreenCanvasCloneData* aData);

  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  // Return true on main-thread, and return gfx.offscreencanvas.enabled
  // on worker thread.
  static bool PrefEnabledOnWorkerThread(JSContext* aCx, JSObject* aObj);

  OffscreenCanvasCloneData* ToCloneData();

  void CommitFrameToCompositor();

  virtual bool GetOpaqueAttr() override
  {
    return false;
  }

  virtual nsIntSize GetWidthHeight() override
  {
    return nsIntSize(mWidth, mHeight);
  }

  virtual already_AddRefed<nsICanvasRenderingContextInternal>
  CreateContext(CanvasContextType aContextType) override;

  virtual already_AddRefed<nsISupports>
  GetContext(JSContext* aCx,
             const nsAString& aContextId,
             JS::Handle<JS::Value> aContextOptions,
             ErrorResult& aRv) override;

  void SetNeutered()
  {
    mNeutered = true;
  }

  bool IsNeutered() const
  {
    return mNeutered;
  }

  void SetWriteOnly()
  {
    mIsWriteOnly = true;
  }

  bool IsWriteOnly() const
  {
    return mIsWriteOnly;
  }

  layers::LayersBackend GetCompositorBackendType() const
  {
    return mCompositorBackendType;
  }

private:
  ~OffscreenCanvas();

  nsCOMPtr<nsIGlobalObject> GetGlobalObject();

  void CanvasAttrChanged()
  {
    mAttrDirty = true;
    ErrorResult dummy;
    UpdateContext(nullptr, JS::NullHandleValue, dummy);
  }

  bool mAttrDirty;
  bool mNeutered;
  bool mIsWriteOnly;

  uint32_t mWidth;
  uint32_t mHeight;

  layers::LayersBackend mCompositorBackendType;

  RefPtr<layers::CanvasClient> mCanvasClient;
  RefPtr<layers::AsyncCanvasRenderer> mCanvasRenderer;
};

} // namespace dom
} // namespace mozilla

#endif // MOZILLA_DOM_OFFSCREENCANVAS_H_

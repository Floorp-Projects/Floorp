/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerManagerD3D9.h"

#include "ThebesLayerD3D9.h"
#include "ContainerLayerD3D9.h"
#include "ImageLayerD3D9.h"
#include "ColorLayerD3D9.h"
#include "CanvasLayerD3D9.h"
#include "ReadbackLayerD3D9.h"
#include "gfxWindowsPlatform.h"
#include "nsIGfxInfo.h"
#include "nsServiceManagerUtils.h"
#include "gfxFailure.h"
#include "mozilla/Preferences.h"

#include "gfxCrashReporterUtils.h"

namespace mozilla {
namespace layers {

DeviceManagerD3D9 *LayerManagerD3D9::mDefaultDeviceManager = nullptr;

LayerManagerD3D9::LayerManagerD3D9(nsIWidget *aWidget)
  : mWidget(aWidget)
  , mDeviceResetCount(0)
{
  mCurrentCallbackInfo.Callback = nullptr;
  mCurrentCallbackInfo.CallbackData = nullptr;
}

LayerManagerD3D9::~LayerManagerD3D9()
{
  Destroy();
}

bool
LayerManagerD3D9::Initialize(bool force)
{
  ScopedGfxFeatureReporter reporter("D3D9 Layers", force);

  /* XXX: this preference and blacklist code should move out of the layer manager */
  bool forceAccelerate = gfxPlatform::GetPrefLayersAccelerationForceEnabled();

  nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
  if (gfxInfo) {
    int32_t status;
    if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, &status))) {
      if (status != nsIGfxInfo::FEATURE_NO_INFO && !forceAccelerate)
      {
        NS_WARNING("Direct3D 9-accelerated layers are not supported on this system.");
        return false;
      }
    }
  }

  if (!mDefaultDeviceManager) {
    mDeviceManager = new DeviceManagerD3D9;

    if (!mDeviceManager->Init()) {
      mDeviceManager = nullptr;
      return false;
    }

    mDefaultDeviceManager = mDeviceManager;
  } else {
    mDeviceManager = mDefaultDeviceManager;
  }

  mSwapChain = mDeviceManager->
    CreateSwapChain((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW));

  if (!mSwapChain) {
    return false;
  }

  reporter.SetSuccessful();
  return true;
}

void
LayerManagerD3D9::SetClippingRegion(const nsIntRegion &aClippingRegion)
{
  mClippingRegion = aClippingRegion;
}

void
LayerManagerD3D9::Destroy()
{
  if (!IsDestroyed()) {
    if (mRoot) {
      static_cast<LayerD3D9*>(mRoot->ImplData())->LayerManagerDestroyed();
    }
    /* Important to release this first since it also holds a reference to the
     * device manager
     */
    mSwapChain = nullptr;
    mDeviceManager = nullptr;
  }
  LayerManager::Destroy();
}

void
LayerManagerD3D9::BeginTransaction()
{
  mInTransaction = true;
}

void
LayerManagerD3D9::BeginTransactionWithTarget(gfxContext *aTarget)
{
  mInTransaction = true;
  mTarget = aTarget;
}

void
LayerManagerD3D9::EndConstruction()
{
}

bool
LayerManagerD3D9::EndEmptyTransaction(EndTransactionFlags aFlags)
{
  mInTransaction = false;

  // If the device reset count from our last EndTransaction doesn't match
  // the current device reset count, the device must have been reset one or
  // more times since our last transaction. In that case, an empty transaction
  // is not possible, because layers may need to be rerendered.
  if (!mRoot || mDeviceResetCount != mDeviceManager->GetDeviceResetCount())
    return false;

  EndTransaction(nullptr, nullptr, aFlags);
  return true;
}

void
LayerManagerD3D9::EndTransaction(DrawThebesLayerCallback aCallback,
                                 void* aCallbackData,
                                 EndTransactionFlags aFlags)
{
  mInTransaction = false;

  mDeviceResetCount = mDeviceManager->GetDeviceResetCount();

  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    mCurrentCallbackInfo.Callback = aCallback;
    mCurrentCallbackInfo.CallbackData = aCallbackData;

    if (aFlags & END_NO_COMPOSITE) {
      // Apply pending tree updates before recomputing effective
      // properties.
      mRoot->ApplyPendingUpdatesToSubtree();
    }

    // The results of our drawing always go directly into a pixel buffer,
    // so we don't need to pass any global transform here.
    mRoot->ComputeEffectiveTransforms(gfx3DMatrix());

    SetCompositingDisabled(aFlags & END_NO_COMPOSITE);
    Render();
    /* Clean this out for sanity */
    mCurrentCallbackInfo.Callback = nullptr;
    mCurrentCallbackInfo.CallbackData = nullptr;
  }

  // Clear mTarget, next transaction could have no target
  mTarget = nullptr;
}

void
LayerManagerD3D9::SetRoot(Layer *aLayer)
{
  mRoot = aLayer;
}

already_AddRefed<ThebesLayer>
LayerManagerD3D9::CreateThebesLayer()
{
  nsRefPtr<ThebesLayer> layer = new ThebesLayerD3D9(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
LayerManagerD3D9::CreateContainerLayer()
{
  nsRefPtr<ContainerLayer> layer = new ContainerLayerD3D9(this);
  return layer.forget();
}

already_AddRefed<ImageLayer>
LayerManagerD3D9::CreateImageLayer()
{
  nsRefPtr<ImageLayer> layer = new ImageLayerD3D9(this);
  return layer.forget();
}

already_AddRefed<ColorLayer>
LayerManagerD3D9::CreateColorLayer()
{
  nsRefPtr<ColorLayer> layer = new ColorLayerD3D9(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
LayerManagerD3D9::CreateCanvasLayer()
{
  nsRefPtr<CanvasLayer> layer = new CanvasLayerD3D9(this);
  return layer.forget();
}

already_AddRefed<ReadbackLayer>
LayerManagerD3D9::CreateReadbackLayer()
{
  nsRefPtr<ReadbackLayer> layer = new ReadbackLayerD3D9(this);
  return layer.forget();
}

void ReleaseTexture(void *texture)
{
  static_cast<IDirect3DTexture9*>(texture)->Release();
}

void
LayerManagerD3D9::ReportFailure(const nsACString &aMsg, HRESULT aCode)
{
  // We could choose to abort here when hr == E_OUTOFMEMORY.
  nsCString msg;
  msg.Append(aMsg);
  msg.AppendLiteral(" Error code: ");
  msg.AppendInt(uint32_t(aCode));
  NS_WARNING(msg.BeginReading());

  gfx::LogFailure(msg);
}

void
LayerManagerD3D9::Render()
{
  if (!mSwapChain->PrepareForRendering()) {
    return;
  }
  deviceManager()->SetupRenderState();

  SetupPipeline();

  if (CompositingDisabled()) {
    static_cast<LayerD3D9*>(mRoot->ImplData())->RenderLayer();
    return;
  }

  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  device()->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 0, 0);

  device()->BeginScene();

  const nsIntRect *clipRect = mRoot->GetClipRect();
  RECT r;
  if (clipRect) {
    r.left = (LONG)clipRect->x;
    r.top = (LONG)clipRect->y;
    r.right = (LONG)(clipRect->x + clipRect->width);
    r.bottom = (LONG)(clipRect->y + clipRect->height);
  } else {
    r.left = r.top = 0;
    r.right = rect.width;
    r.bottom = rect.height;
  }
  device()->SetScissorRect(&r);

  static_cast<LayerD3D9*>(mRoot->ImplData())->RenderLayer();

  device()->EndScene();

  if (!mTarget) {
    const nsIntRect *r;
    for (nsIntRegionRectIterator iter(mClippingRegion);
         (r = iter.Next()) != nullptr;) {
      mSwapChain->Present(*r);
    }
    LayerManager::PostPresent();
  } else {
    PaintToTarget();
  }
}

void
LayerManagerD3D9::SetupPipeline()
{
  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  gfx3DMatrix viewMatrix;
  /*
   * Matrix to transform to viewport space ( <-1.0, 1.0> topleft,
   * <1.0, -1.0> bottomright)
   */
  viewMatrix._11 = 2.0f / rect.width;
  viewMatrix._22 = -2.0f / rect.height;
  viewMatrix._33 = 0.0f;
  viewMatrix._41 = -1.0f;
  viewMatrix._42 = 1.0f;

  HRESULT hr = device()->SetVertexShaderConstantF(CBmProjection,
                                                  &viewMatrix._11, 4);

  if (FAILED(hr)) {
    NS_WARNING("Failed to set projection shader constant!");
  }

  hr = device()->SetVertexShaderConstantF(CBvTextureCoords,
                                          ShaderConstantRect(0, 0, 1.0f, 1.0f),
                                          1);

  if (FAILED(hr)) {
    NS_WARNING("Failed to set texCoords shader constant!");
  }

  float offset[] = { 0, 0, 0, 0 };
  hr = device()->SetVertexShaderConstantF(CBvRenderTargetOffset, offset, 1);

  if (FAILED(hr)) {
    NS_WARNING("Failed to set RenderTargetOffset shader constant!");
  }
}

void
LayerManagerD3D9::PaintToTarget()
{
  nsRefPtr<IDirect3DSurface9> backBuff;
  nsRefPtr<IDirect3DSurface9> destSurf;
  device()->GetRenderTarget(0, getter_AddRefs(backBuff));

  D3DSURFACE_DESC desc;
  backBuff->GetDesc(&desc);

  device()->CreateOffscreenPlainSurface(desc.Width, desc.Height,
                                       D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                       getter_AddRefs(destSurf), nullptr);

  device()->GetRenderTargetData(backBuff, destSurf);

  D3DLOCKED_RECT rect;
  destSurf->LockRect(&rect, nullptr, D3DLOCK_READONLY);

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface((unsigned char*)rect.pBits,
                        gfxIntSize(desc.Width, desc.Height),
                        rect.Pitch,
                        gfxASurface::ImageFormatARGB32);

  mTarget->SetSource(imageSurface);
  mTarget->SetOperator(gfxContext::OPERATOR_OVER);
  mTarget->Paint();
  destSurf->UnlockRect();
}

LayerD3D9::LayerD3D9(LayerManagerD3D9 *aManager)
  : mD3DManager(aManager)
{
}

} /* namespace layers */
} /* namespace mozilla */

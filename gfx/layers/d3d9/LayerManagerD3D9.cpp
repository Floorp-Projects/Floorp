/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

DeviceManagerD3D9 *LayerManagerD3D9::mDefaultDeviceManager = nsnull;

LayerManagerD3D9::LayerManagerD3D9(nsIWidget *aWidget)
  : mWidget(aWidget)
  , mDeviceResetCount(0)
{
  mCurrentCallbackInfo.Callback = NULL;
  mCurrentCallbackInfo.CallbackData = NULL;
}

LayerManagerD3D9::~LayerManagerD3D9()
{
  Destroy();
}

bool
LayerManagerD3D9::Initialize()
{
  ScopedGfxFeatureReporter reporter("D3D9 Layers");

  /* XXX: this preference and blacklist code should move out of the layer manager */
  bool forceAccelerate =
    Preferences::GetBool("layers.acceleration.force-enabled", false);

  nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
  if (gfxInfo) {
    PRInt32 status;
    if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, &status))) {
      if (status != nsIGfxInfo::FEATURE_NO_INFO && !forceAccelerate)
      {
        NS_WARNING("Direct3D 9-accelerated layers are not supported on this system.");
        return PR_FALSE;
      }
    }
  }

  if (!mDefaultDeviceManager) {
    mDeviceManager = new DeviceManagerD3D9;

    if (!mDeviceManager->Init()) {
      mDeviceManager = nsnull;
      return PR_FALSE;
    }

    mDefaultDeviceManager = mDeviceManager;
  } else {
    mDeviceManager = mDefaultDeviceManager;
  }

  mSwapChain = mDeviceManager->
    CreateSwapChain((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW));

  if (!mSwapChain) {
    return PR_FALSE;
  }

  reporter.SetSuccessful();
  return PR_TRUE;
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
    mSwapChain = nsnull;
    mDeviceManager = nsnull;
  }
  LayerManager::Destroy();
}

void
LayerManagerD3D9::BeginTransaction()
{
}

void
LayerManagerD3D9::BeginTransactionWithTarget(gfxContext *aTarget)
{
  mTarget = aTarget;
}

void
LayerManagerD3D9::EndConstruction()
{
}

bool
LayerManagerD3D9::EndEmptyTransaction()
{
  // If the device reset count from our last EndTransaction doesn't match
  // the current device reset count, the device must have been reset one or
  // more times since our last transaction. In that case, an empty transaction
  // is not possible, because layers may need to be rerendered.
  if (!mRoot || mDeviceResetCount != mDeviceManager->GetDeviceResetCount())
    return false;

  EndTransaction(nsnull, nsnull);
  return true;
}

void
LayerManagerD3D9::EndTransaction(DrawThebesLayerCallback aCallback,
                                 void* aCallbackData,
                                 EndTransactionFlags aFlags)
{
  mDeviceResetCount = mDeviceManager->GetDeviceResetCount();

  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    mCurrentCallbackInfo.Callback = aCallback;
    mCurrentCallbackInfo.CallbackData = aCallbackData;

    // The results of our drawing always go directly into a pixel buffer,
    // so we don't need to pass any global transform here.
    mRoot->ComputeEffectiveTransforms(gfx3DMatrix());

    Render();
    /* Clean this out for sanity */
    mCurrentCallbackInfo.Callback = NULL;
    mCurrentCallbackInfo.CallbackData = NULL;
  }

  // Clear mTarget, next transaction could have no target
  mTarget = NULL;
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

already_AddRefed<ImageContainer>
LayerManagerD3D9::CreateImageContainer()
{
  nsRefPtr<ImageContainer> container = new ImageContainerD3D9(device());
  return container.forget();
}

already_AddRefed<ShadowThebesLayer>
LayerManagerD3D9::CreateShadowThebesLayer()
{
  if (LayerManagerD3D9::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowThebesLayerD3D9>(new ShadowThebesLayerD3D9(this)).forget();
}

already_AddRefed<ShadowContainerLayer>
LayerManagerD3D9::CreateShadowContainerLayer()
{
  if (LayerManagerD3D9::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowContainerLayerD3D9>(new ShadowContainerLayerD3D9(this)).forget();
}

already_AddRefed<ShadowImageLayer>
LayerManagerD3D9::CreateShadowImageLayer()
{
  if (LayerManagerD3D9::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowImageLayerD3D9>(new ShadowImageLayerD3D9(this)).forget();
}

already_AddRefed<ShadowColorLayer>
LayerManagerD3D9::CreateShadowColorLayer()
{
  if (LayerManagerD3D9::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowColorLayerD3D9>(new ShadowColorLayerD3D9(this)).forget();
}

already_AddRefed<ShadowCanvasLayer>
LayerManagerD3D9::CreateShadowCanvasLayer()
{
  if (LayerManagerD3D9::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowCanvasLayerD3D9>(new ShadowCanvasLayerD3D9(this)).forget();
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
  msg.AppendInt(PRUint32(aCode));
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
  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  device()->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 0, 0);

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
         (r = iter.Next()) != nsnull;) {
      mSwapChain->Present(*r);
    }
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
                                       getter_AddRefs(destSurf), NULL);

  device()->GetRenderTargetData(backBuff, destSurf);

  D3DLOCKED_RECT rect;
  destSurf->LockRect(&rect, NULL, D3DLOCK_READONLY);

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

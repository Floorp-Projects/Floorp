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

#include "LayerManagerD3D9Shaders.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace layers {

struct vertex {
  float x, y;
};

IDirect3D9 *LayerManagerD3D9::mD3D9 = NULL;

typedef IDirect3D9* (WINAPI*Direct3DCreate9Func)(
  UINT SDKVersion
);


LayerManagerD3D9::LayerManagerD3D9(nsIWidget *aWidget)
{
    mWidget = aWidget;
    mCurrentCallbackInfo.Callback = NULL;
    mCurrentCallbackInfo.CallbackData = NULL;
}

LayerManagerD3D9::~LayerManagerD3D9()
{
}

#define HAS_CAP(a, b) (((a) & (b)) == (b))
#define LACKS_CAP(a, b) !(((a) & (b)) == (b))

PRBool
LayerManagerD3D9::Initialize()
{
  if (!mD3D9) {
    Direct3DCreate9Func d3d9create = (Direct3DCreate9Func)
      GetProcAddress(LoadLibraryW(L"d3d9.dll"), "Direct3DCreate9");
    if (!d3d9create) {
      return PR_FALSE;
    }

    mD3D9 = d3d9create(D3D_SDK_VERSION);
    if (!mD3D9) {
      return PR_FALSE;
    }
  }

  D3DPRESENT_PARAMETERS pp;
  memset(&pp, 0, sizeof(D3DPRESENT_PARAMETERS));

  pp.BackBufferFormat = D3DFMT_A8R8G8B8;
  pp.SwapEffect = D3DSWAPEFFECT_COPY;
  pp.Windowed = TRUE;
  pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
  pp.hDeviceWindow = (HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW);

  HRESULT hr = mD3D9->CreateDevice(D3DADAPTER_DEFAULT,
                                   D3DDEVTYPE_HAL,
                                   NULL,
                                   D3DCREATE_FPU_PRESERVE |
                                   D3DCREATE_MULTITHREADED |
                                   D3DCREATE_MIXED_VERTEXPROCESSING,
                                   &pp,
                                   getter_AddRefs(mDevice));

  if (FAILED(hr)) {
    return PR_FALSE;
  }

  if (!VerifyCaps()) {
    return PR_FALSE;
  }

  hr = mDevice->CreateVertexShader((DWORD*)LayerQuadVS,
                                   getter_AddRefs(mLayerVS));

  if (FAILED(hr)) {
    return PR_FALSE;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBShaderPS,
                                  getter_AddRefs(mRGBPS));

  if (FAILED(hr)) {
    return PR_FALSE;
  }

  hr = mDevice->CreatePixelShader((DWORD*)YCbCrShaderPS,
                                  getter_AddRefs(mYCbCrPS));

  if (FAILED(hr)) {
    return PR_FALSE;
  }

  hr = mDevice->CreatePixelShader((DWORD*)SolidColorShaderPS,
                                  getter_AddRefs(mSolidColorPS));

  if (FAILED(hr)) {
    return PR_FALSE;
  }

  hr = mDevice->CreateVertexBuffer(sizeof(vertex) * 4,
                                   0,
                                   0,
                                   D3DPOOL_MANAGED,
                                   getter_AddRefs(mVB),
                                   NULL);

  if (FAILED(hr)) {
    return PR_FALSE;
  }

  vertex *vertices;
  hr = mVB->Lock(0, 0, (void**)&vertices, 0);
  if (FAILED(hr)) {
    return PR_FALSE;
  }
  
  vertices[0].x = vertices[0].y = 0;
  vertices[1].x = 1; vertices[1].y = 0;
  vertices[2].x = 0; vertices[2].y = 1;
  vertices[3].x = 1; vertices[3].y = 1;

  mVB->Unlock();

  hr = mDevice->SetStreamSource(0, mVB, 0, sizeof(vertex));
  if (FAILED(hr)) {
    return PR_FALSE;
  }

  D3DVERTEXELEMENT9 elements[] = {
    { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
      D3DDECLUSAGE_POSITION, 0 },
    D3DDECL_END()
  };
  
  mDevice->CreateVertexDeclaration(elements, getter_AddRefs(mVD));

  SetupRenderState();

  nsCOMPtr<nsIConsoleService> 
    console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  D3DADAPTER_IDENTIFIER9 identifier;
  mD3D9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier);

  if (console) {
    nsString msg;
    msg +=
      NS_LITERAL_STRING("Direct3D 9 LayerManager Initialized Succesfully.\nDriver: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)identifier.Driver));
    msg += NS_LITERAL_STRING("\nDescription: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)identifier.Description));
    msg += NS_LITERAL_STRING("\nVersion: ");
    msg += NS_ConvertUTF8toUTF16(
      nsPrintfCString("%d.%d.%d.%d",
                      HIWORD(identifier.DriverVersion.HighPart),
                      LOWORD(identifier.DriverVersion.HighPart),
                      HIWORD(identifier.DriverVersion.LowPart),
                      LOWORD(identifier.DriverVersion.LowPart)));
    console->LogStringMessage(msg.get());
  }

  return PR_TRUE;
}

void
LayerManagerD3D9::SetClippingRegion(const nsIntRegion &aClippingRegion)
{
  mClippingRegion = aClippingRegion;
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

void
LayerManagerD3D9::EndTransaction(DrawThebesLayerCallback aCallback,
                                 void* aCallbackData)
{
  mCurrentCallbackInfo.Callback = aCallback;
  mCurrentCallbackInfo.CallbackData = aCallbackData;
  Render();
  /* Clean this out for sanity */
  mCurrentCallbackInfo.Callback = NULL;
  mCurrentCallbackInfo.CallbackData = NULL;
}

void
LayerManagerD3D9::SetRoot(Layer *aLayer)
{
  mRootLayer = static_cast<LayerD3D9*>(aLayer->ImplData());
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

already_AddRefed<ImageContainer>
LayerManagerD3D9::CreateImageContainer()
{
  nsRefPtr<ImageContainer> container = new ImageContainerD3D9(this);
  return container.forget();
}

void
LayerManagerD3D9::SetShaderMode(ShaderMode aMode)
{
  switch (aMode) {
    case RGBLAYER:
      mDevice->SetVertexShader(mLayerVS);
      mDevice->SetPixelShader(mRGBPS);
      break;
    case YCBCRLAYER:
      mDevice->SetVertexShader(mLayerVS);
      mDevice->SetPixelShader(mYCbCrPS);
      break;
    case SOLIDCOLORLAYER:
      mDevice->SetVertexShader(mLayerVS);
      mDevice->SetPixelShader(mSolidColorPS);
      break;
  }
}

void
LayerManagerD3D9::Render()
{
  if (!SetupBackBuffer()) {
    return;
  }
  SetupPipeline();
  nsIntRect rect;
  mWidget->GetBounds(rect);

  mDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0xffffffff, 0, 0);

  mDevice->BeginScene();
  
  if (mRootLayer) {
    const nsIntRect *clipRect = mRootLayer->GetLayer()->GetClipRect();
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
    mDevice->SetScissorRect(&r);

    mRootLayer->RenderLayer();
  }

  mDevice->EndScene();

  if (!mTarget) {
    const nsIntRect *r;
    for (nsIntRegionRectIterator iter(mClippingRegion);
         (r = iter.Next()) != nsnull;) {
      RECT rect;
      rect.left = r->x;
      rect.top = r->y;
      rect.right = r->XMost();
      rect.bottom = r->YMost();

      mDevice->Present(&rect, &rect, NULL, NULL);
    }
  } else {
    PaintToTarget();
  }
}

void
LayerManagerD3D9::SetupPipeline()
{
  nsIntRect rect;
  mWidget->GetBounds(rect);

  float viewMatrix[4][4];
  /*
   * Matrix to transform to viewport space ( <-1.0, 1.0> topleft, 
   * <1.0, -1.0> bottomright)
   */
  memset(&viewMatrix, 0, sizeof(viewMatrix));
  viewMatrix[0][0] = 2.0f / rect.width;
  viewMatrix[1][1] = -2.0f / rect.height;
  viewMatrix[2][2] = 1.0f;
  viewMatrix[3][0] = -1.0f;
  viewMatrix[3][1] = 1.0f;
  viewMatrix[3][3] = 1.0f;
  
  HRESULT hr = mDevice->SetVertexShaderConstantF(8, &viewMatrix[0][0], 4);

  if (FAILED(hr)) {
    NS_WARNING("Failed to set projection shader constant!");
  }
}

PRBool
LayerManagerD3D9::SetupBackBuffer()
{
  nsRefPtr<IDirect3DSurface9> backBuffer;
  mDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
			 getter_AddRefs(backBuffer));

  D3DSURFACE_DESC desc;
  nsIntRect rect;
  mWidget->GetBounds(rect);
  backBuffer->GetDesc(&desc);

  HRESULT hr = mDevice->TestCooperativeLevel();

  /* The device is lost or something else is wrong, failure */
  if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET) {
    return PR_FALSE;
  }

  /*
   * If the backbuffer is the right size, and the device is not lost, we can
   * safely render without doing anything.
   */
  if ((desc.Width == rect.width && desc.Height == rect.height) &&
      SUCCEEDED(hr)) {
    return PR_TRUE;
  }

  /*
   * Our device is lost or our backbuffer needs resizing, start by clearing
   * out all D3DPOOL_DEFAULT surfaces.
   */
  for(unsigned int i = 0; i < mThebesLayers.Length(); i++) {
    mThebesLayers[i]->CleanResources();
  }

  backBuffer = NULL;

  D3DPRESENT_PARAMETERS pp;
  memset(&pp, 0, sizeof(D3DPRESENT_PARAMETERS));

  pp.BackBufferFormat = D3DFMT_A8R8G8B8;
  pp.SwapEffect = D3DSWAPEFFECT_COPY;
  pp.Windowed = TRUE;
  pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
  pp.hDeviceWindow = (HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW);

  hr = mDevice->Reset(&pp);
  if (FAILED(hr)) {
    return PR_FALSE;
  }

  SetupRenderState();

  return PR_TRUE;
}

void
LayerManagerD3D9::PaintToTarget()
{
  nsRefPtr<IDirect3DSurface9> backBuff;
  nsRefPtr<IDirect3DSurface9> destSurf;
  mDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
                         getter_AddRefs(backBuff));

  D3DSURFACE_DESC desc;
  backBuff->GetDesc(&desc);

  mDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height,
                                       D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                       getter_AddRefs(destSurf), NULL);

  mDevice->GetRenderTargetData(backBuff, destSurf);

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

void
LayerManagerD3D9::SetupRenderState()
{
  mDevice->SetStreamSource(0, mVB, 0, sizeof(vertex));
  mDevice->SetVertexDeclaration(mVD);
  mDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  mDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  mDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
  mDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  mDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  mDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  mDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

PRBool
LayerManagerD3D9::VerifyCaps()
{
  D3DCAPS9 caps;
  HRESULT hr = mDevice->GetDeviceCaps(&caps);

  if (FAILED(hr)) {
    return PR_FALSE;
  }

  if (LACKS_CAP(caps.DevCaps, D3DDEVCAPS_TEXTUREVIDEOMEMORY)) {
    return PR_FALSE;
  }

  if (LACKS_CAP(caps.PrimitiveMiscCaps, D3DPMISCCAPS_CULLNONE)) {
    return PR_FALSE;
  }

  if (LACKS_CAP(caps.SrcBlendCaps, D3DPBLENDCAPS_ONE) ||
      LACKS_CAP(caps.SrcBlendCaps, D3DBLEND_SRCALPHA) ||
      LACKS_CAP(caps.DestBlendCaps, D3DPBLENDCAPS_INVSRCALPHA)) {
    return PR_FALSE;
  }

  if (LACKS_CAP(caps.RasterCaps, D3DPRASTERCAPS_SCISSORTEST)) {
    return PR_FALSE;
  }

  if (LACKS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_ALPHA) ||
      HAS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_SQUAREONLY) ||
      (HAS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_POW2) &&
       LACKS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_NONPOW2CONDITIONAL))) {
    return PR_FALSE;
  }

  if (LACKS_CAP(caps.TextureFilterCaps, D3DPTFILTERCAPS_MAGFLINEAR) ||
      LACKS_CAP(caps.TextureFilterCaps, D3DPTFILTERCAPS_MINFLINEAR)) {
    return PR_FALSE;
  }

  if (LACKS_CAP(caps.TextureAddressCaps, D3DPTADDRESSCAPS_CLAMP)) {
    return PR_FALSE;
  }

  if (caps.MaxTextureHeight < 4096 ||
      caps.MaxTextureWidth < 4096) {
    return PR_FALSE;
  }

  if ((caps.PixelShaderVersion & 0xffff) < 0x200 ||
      (caps.VertexShaderVersion & 0xffff) < 0x200) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

LayerD3D9::LayerD3D9(LayerManagerD3D9 *aManager)
  : mD3DManager(aManager)
  , mNextSibling(NULL)
{
}

LayerD3D9*
LayerD3D9::GetNextSibling()
{
  return mNextSibling;
}

void
LayerD3D9::SetNextSibling(LayerD3D9 *aNextSibling)
{
  mNextSibling = aNextSibling;
}

} /* namespace layers */
} /* namespace mozilla */

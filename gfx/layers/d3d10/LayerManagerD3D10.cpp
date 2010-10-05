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

#include "LayerManagerD3D10.h"
#include "LayerManagerD3D10Effect.h"
#include "gfxWindowsPlatform.h"
#include "gfxD2DSurface.h"
#include "cairo-win32.h"
#include "dxgi.h"

#include "ContainerLayerD3D10.h"
#include "ThebesLayerD3D10.h"
#include "ColorLayerD3D10.h"
#include "CanvasLayerD3D10.h"
#include "ImageLayerD3D10.h"

namespace mozilla {
namespace layers {

typedef HRESULT (WINAPI*D3D10CreateEffectFromMemoryFunc)(
    void *pData,
    SIZE_T DataLength,
    UINT FXFlags,
    ID3D10Device *pDevice, 
    ID3D10EffectPool *pEffectPool,
    ID3D10Effect **ppEffect
);

struct Vertex
{
    float position[2];
};

// {17F88CCB-1F49-4c08-8002-ADA7BD44856D}
static const GUID sEffect = 
{ 0x17f88ccb, 0x1f49, 0x4c08, { 0x80, 0x2, 0xad, 0xa7, 0xbd, 0x44, 0x85, 0x6d } };
// {19599D91-912C-4C2F-A8C5-299DE85FBD34}
static const GUID sInputLayout = 
{ 0x19599d91, 0x912c, 0x4c2f, { 0xa8, 0xc5, 0x29, 0x9d, 0xe8, 0x5f, 0xbd, 0x34 } };
// {293157D2-09C7-4680-AE27-C28E370E418B}
static const GUID sVertexBuffer = 
{ 0x293157d2, 0x9c7, 0x4680, { 0xae, 0x27, 0xc2, 0x8e, 0x37, 0xe, 0x41, 0x8b } };

cairo_user_data_key_t gKeyD3D10Texture;

LayerManagerD3D10::LayerManagerD3D10(nsIWidget *aWidget)
  : mWidget(aWidget)
{
}

LayerManagerD3D10::~LayerManagerD3D10()
{
}

bool
LayerManagerD3D10::Initialize()
{
  HRESULT hr;

  cairo_device_t *device = gfxWindowsPlatform::GetPlatform()->GetD2DDevice();
  if (!device) {
    return false;
  }

  mDevice = cairo_d2d_device_get_device(device);

  UINT size = 4;
  if (FAILED(mDevice->GetPrivateData(sEffect, &size, mEffect.StartAssignment()))) {
    D3D10CreateEffectFromMemoryFunc createEffect = (D3D10CreateEffectFromMemoryFunc)
	GetProcAddress(LoadLibraryA("d3d10_1.dll"), "D3D10CreateEffectFromMemory");

    if (!createEffect) {
      return false;
    }

    hr = createEffect((void*)g_main,
                      sizeof(g_main),
                      D3D10_EFFECT_SINGLE_THREADED,
                      mDevice,
                      NULL,
                      getter_AddRefs(mEffect));
    
    if (FAILED(hr)) {
      return false;
    }

    mDevice->SetPrivateDataInterface(sEffect, mEffect);
  }

  size = 4;
  if (FAILED(mDevice->GetPrivateData(sInputLayout, &size, mInputLayout.StartAssignment()))) {
    D3D10_INPUT_ELEMENT_DESC layout[] =
    {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC passDesc;
    mEffect->GetTechniqueByName("RenderRGBLayerPremul")->GetPassByIndex(0)->
      GetDesc(&passDesc);

    hr = mDevice->CreateInputLayout(layout,
                                    sizeof(layout) / sizeof(D3D10_INPUT_ELEMENT_DESC),
                                    passDesc.pIAInputSignature,
                                    passDesc.IAInputSignatureSize,
                                    getter_AddRefs(mInputLayout));
    
    if (FAILED(hr)) {
      return false;
    }

    mDevice->SetPrivateDataInterface(sInputLayout, mInputLayout);
  }

  size = 4;
  if (FAILED(mDevice->GetPrivateData(sVertexBuffer, &size, mVertexBuffer.StartAssignment()))) {
    Vertex vertices[] = { {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0} };
    CD3D10_BUFFER_DESC bufferDesc(sizeof(vertices), D3D10_BIND_VERTEX_BUFFER);
    D3D10_SUBRESOURCE_DATA data;
    data.pSysMem = (void*)vertices;

    hr = mDevice->CreateBuffer(&bufferDesc, &data, getter_AddRefs(mVertexBuffer));

    if (FAILED(hr)) {
      return false;
    }

    mDevice->SetPrivateDataInterface(sVertexBuffer, mVertexBuffer);
  }

  nsRefPtr<IDXGIDevice> dxgiDevice;
  nsRefPtr<IDXGIAdapter> dxgiAdapter;
  nsRefPtr<IDXGIFactory> dxgiFactory;

  mDevice->QueryInterface(dxgiDevice.StartAssignment());
  dxgiDevice->GetAdapter(getter_AddRefs(dxgiAdapter));

  dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.StartAssignment()));

  DXGI_SWAP_CHAIN_DESC swapDesc;
  ::ZeroMemory(&swapDesc, sizeof(swapDesc));

  swapDesc.BufferDesc.Width = 0;
  swapDesc.BufferDesc.Height = 0;
  swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  swapDesc.BufferDesc.RefreshRate.Numerator = 60;
  swapDesc.BufferDesc.RefreshRate.Denominator = 1;
  swapDesc.SampleDesc.Count = 1;
  swapDesc.SampleDesc.Quality = 0;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.BufferCount = 1;
  swapDesc.OutputWindow = (HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW);
  swapDesc.Windowed = TRUE;

  /**
   * Create a swap chain, this swap chain will contain the backbuffer for
   * the window we draw to. The front buffer is the full screen front
   * buffer.
   */
  hr = dxgiFactory->CreateSwapChain(dxgiDevice, &swapDesc, getter_AddRefs(mSwapChain));

  if (FAILED(hr)) {
    return false;
  }

  return true;
}

void
LayerManagerD3D10::SetRoot(Layer *aRoot)
{
  mRoot = aRoot;
}

void
LayerManagerD3D10::BeginTransaction()
{
}

void
LayerManagerD3D10::BeginTransactionWithTarget(gfxContext* aTarget)
{
  mTarget = aTarget;
}

void
LayerManagerD3D10::EndTransaction(DrawThebesLayerCallback aCallback,
                                  void* aCallbackData)
{
  mCurrentCallbackInfo.Callback = aCallback;
  mCurrentCallbackInfo.CallbackData = aCallbackData;
  Render();
  mCurrentCallbackInfo.Callback = nsnull;
  mCurrentCallbackInfo.CallbackData = nsnull;
  mTarget = nsnull;
}

already_AddRefed<ThebesLayer>
LayerManagerD3D10::CreateThebesLayer()
{
  nsRefPtr<ThebesLayer> layer = new ThebesLayerD3D10(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
LayerManagerD3D10::CreateContainerLayer()
{
  nsRefPtr<ContainerLayer> layer = new ContainerLayerD3D10(this);
  return layer.forget();
}

already_AddRefed<ImageLayer>
LayerManagerD3D10::CreateImageLayer()
{
  nsRefPtr<ImageLayer> layer = new ImageLayerD3D10(this);
  return layer.forget();
}

already_AddRefed<ColorLayer>
LayerManagerD3D10::CreateColorLayer()
{
  nsRefPtr<ColorLayer> layer = new ColorLayerD3D10(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
LayerManagerD3D10::CreateCanvasLayer()
{
  nsRefPtr<CanvasLayer> layer = new CanvasLayerD3D10(this);
  return layer.forget();
}

already_AddRefed<ImageContainer>
LayerManagerD3D10::CreateImageContainer()
{
  nsRefPtr<ImageContainer> layer = new ImageContainerD3D10(this);
  return layer.forget();
}

static void ReleaseTexture(void *texture)
{
  static_cast<ID3D10Texture2D*>(texture)->Release();
}

already_AddRefed<gfxASurface>
LayerManagerD3D10::CreateOptimalSurface(const gfxIntSize &aSize,
                                   gfxASurface::gfxImageFormat aFormat)
{
  if ((aFormat != gfxASurface::ImageFormatRGB24 &&
       aFormat != gfxASurface::ImageFormatARGB32)) {
    return LayerManager::CreateOptimalSurface(aSize, aFormat);
  }

  nsRefPtr<ID3D10Texture2D> texture;
  
  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aSize.width, aSize.height, 1, 1);
  desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
  desc.MiscFlags = D3D10_RESOURCE_MISC_GDI_COMPATIBLE;
  
  HRESULT hr = device()->CreateTexture2D(&desc, NULL, getter_AddRefs(texture));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create new texture for CreateOptimalSurface!");
    return LayerManager::CreateOptimalSurface(aSize, aFormat);
  }

  nsRefPtr<gfxD2DSurface> surface =
    new gfxD2DSurface(texture, aFormat == gfxASurface::ImageFormatRGB24 ?
      gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA);

  if (!surface || surface->CairoStatus()) {
    return LayerManager::CreateOptimalSurface(aSize, aFormat);
  }

  surface->SetData(&gKeyD3D10Texture,
                   texture.forget().get(),
                   ReleaseTexture);

  return surface.forget();
}

void
LayerManagerD3D10::SetViewport(const nsIntSize &aViewport)
{
  mViewport = aViewport;

  D3D10_VIEWPORT viewport;
  viewport.MaxDepth = 1.0f;
  viewport.MinDepth = 0;
  viewport.Width = aViewport.width;
  viewport.Height = aViewport.height;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;

  mDevice->RSSetViewports(1, &viewport);

  gfx3DMatrix projection;
  /*
   * Matrix to transform to viewport space ( <-1.0, 1.0> topleft,
   * <1.0, -1.0> bottomright)
   */
  projection._11 = 2.0f / aViewport.width;
  projection._22 = -2.0f / aViewport.height;
  projection._33 = 1.0f;
  projection._41 = -1.0f;
  projection._42 = 1.0f;
  projection._44 = 1.0f;

  HRESULT hr = mEffect->GetVariableByName("mProjection")->
    SetRawValue(&projection._11, 0, 64);

  if (FAILED(hr)) {
    NS_WARNING("Failed to set projection matrix.");
  }
}

void
LayerManagerD3D10::SetupPipeline()
{
  VerifyBufferSize();
  UpdateRenderTarget();

  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  HRESULT hr;

  hr = mEffect->GetVariableByName("vTextureCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));

  if (FAILED(hr)) {
    NS_WARNING("Failed to set Texture Coordinates.");
    return;
  }

  ID3D10RenderTargetView *view = mRTView;
  mDevice->OMSetRenderTargets(1, &view, NULL);
  mDevice->IASetInputLayout(mInputLayout);

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  ID3D10Buffer *buffer = mVertexBuffer;
  mDevice->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
  mDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  SetViewport(nsIntSize(rect.width, rect.height));
}

void
LayerManagerD3D10::UpdateRenderTarget()
{
  if (mRTView) {
    return;
  }

  HRESULT hr;

  nsRefPtr<ID3D10Texture2D> backBuf;
  
  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)backBuf.StartAssignment());
  if (FAILED(hr)) {
    return;
  }
  
  mDevice->CreateRenderTargetView(backBuf, NULL, getter_AddRefs(mRTView));
}

void
LayerManagerD3D10::VerifyBufferSize()
{
  DXGI_SWAP_CHAIN_DESC swapDesc;
  mSwapChain->GetDesc(&swapDesc);

  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  if (swapDesc.BufferDesc.Width == rect.width &&
      swapDesc.BufferDesc.Height == rect.height) {
    return;
  }

  mRTView = nsnull;
  mSwapChain->ResizeBuffers(1, rect.width, rect.height,
                            DXGI_FORMAT_B8G8R8A8_UNORM, 0);

}

void
LayerManagerD3D10::Render()
{
  if (mRoot) {
    static_cast<LayerD3D10*>(mRoot->ImplData())->Validate();
  }

  SetupPipeline();

  float black[] = { 0, 0, 0, 0 };
  device()->ClearRenderTargetView(mRTView, black);

  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  if (mRoot) {
    const nsIntRect *clipRect = mRoot->GetClipRect();
    D3D10_RECT r;
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
    device()->RSSetScissorRects(1, &r);

    static_cast<LayerD3D10*>(mRoot->ImplData())->RenderLayer(1, gfx3DMatrix());
  }

  if (mTarget) {
    PaintToTarget();
  } else {
    mSwapChain->Present(0, 0);
  }
}

void
LayerManagerD3D10::PaintToTarget()
{
  nsRefPtr<ID3D10Texture2D> backBuf;
  
  mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)backBuf.StartAssignment());

  D3D10_TEXTURE2D_DESC bbDesc;
  backBuf->GetDesc(&bbDesc);

  CD3D10_TEXTURE2D_DESC softDesc(bbDesc.Format, bbDesc.Width, bbDesc.Height);
  softDesc.MipLevels = 1;
  softDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
  softDesc.Usage = D3D10_USAGE_STAGING;
  softDesc.BindFlags = 0;

  nsRefPtr<ID3D10Texture2D> readTexture;

  device()->CreateTexture2D(&softDesc, NULL, getter_AddRefs(readTexture));

  device()->CopyResource(readTexture, backBuf);

  D3D10_MAPPED_TEXTURE2D map;
  readTexture->Map(0, D3D10_MAP_READ, 0, &map);

  nsRefPtr<gfxImageSurface> tmpSurface =
    new gfxImageSurface((unsigned char*)map.pData,
                        gfxIntSize(bbDesc.Width, bbDesc.Height),
                        map.RowPitch,
                        gfxASurface::ImageFormatARGB32);

  mTarget->SetSource(tmpSurface);
  mTarget->SetOperator(gfxContext::OPERATOR_SOURCE);
  mTarget->Paint();
}

LayerD3D10::LayerD3D10(LayerManagerD3D10 *aManager)
  : mD3DManager(aManager)
{
}

}
}

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MLGDeviceD3D11.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/StackArray.h"
#include "mozilla/layers/DiagnosticsD3D11.h"
#include "mozilla/layers/LayerMLGPU.h"
#include "mozilla/layers/MemoryReportingMLGPU.h"
#include "mozilla/layers/ShaderDefinitionsMLGPU.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/widget/WinCompositorWidget.h"
#include "MLGShaders.h"
#include "TextureD3D11.h"
#include "gfxPrefs.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace mozilla::widget;
using namespace mozilla::layers::mlg;
using namespace std;

// Defined in CompositorD3D11.cpp.
bool CanUsePartialPresents(ID3D11Device* aDevice);

static D3D11_BOX RectToBox(const gfx::IntRect& aRect);

MLGRenderTargetD3D11::MLGRenderTargetD3D11(const gfx::IntSize& aSize, MLGRenderTargetFlags aFlags)
 : MLGRenderTarget(aFlags),
   mSize(aSize)
{
}

MLGRenderTargetD3D11::~MLGRenderTargetD3D11()
{
  if (mDepthBuffer) {
    sRenderTargetUsage -= mSize.width * mSize.height * 1;
  }
  ForgetTexture();
}

bool
MLGRenderTargetD3D11::Initialize(ID3D11Device* aDevice)
{
  D3D11_TEXTURE2D_DESC desc;
  ::ZeroMemory(&desc, sizeof(desc));
  desc.Width = mSize.width;
  desc.Height = mSize.height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.SampleDesc.Count = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr = aDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  if (FAILED(hr) || !texture) {
    gfxCriticalNote << "Failed to create render target texture: " << hexa(hr);
    return false;
  }

  return Initialize(aDevice, texture);
}

bool
MLGRenderTargetD3D11::Initialize(ID3D11Device* aDevice, ID3D11Texture2D* aTexture)
{
  if (!UpdateTexture(aTexture)) {
    return false;
  }
  if ((mFlags & MLGRenderTargetFlags::ZBuffer) && !CreateDepthBuffer(aDevice)) {
    return false;
  }
  return true;
}

bool
MLGRenderTargetD3D11::UpdateTexture(ID3D11Texture2D* aTexture)
{
  // Save the view first, in case we can re-use it.
  RefPtr<ID3D11RenderTargetView> view = mRTView.forget();

  ForgetTexture();

  if (!aTexture) {
    return true;
  }

#ifdef DEBUG
  D3D11_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);
  MOZ_ASSERT(desc.Width == mSize.width && desc.Height == mSize.height);
#endif

  RefPtr<ID3D11Device> device;
  aTexture->GetDevice(getter_AddRefs(device));

  if (view) {
    // Check that the view matches the backing texture.
    RefPtr<ID3D11Resource> resource;
    view->GetResource(getter_AddRefs(resource));
    if (resource != aTexture) {
      view = nullptr;
    }
  }

  // If we couldn't re-use a view from before, make one now.
  if (!view) {
    HRESULT hr = device->CreateRenderTargetView(aTexture, nullptr, getter_AddRefs(view));
    if (FAILED(hr) || !view) {
      gfxCriticalNote << "Failed to create render target view: " << hexa(hr);
      return false;
    }
  }

  mTexture = aTexture;
  mRTView = view.forget();
  sRenderTargetUsage += mSize.width * mSize.height * 4;
  return true;
}

void
MLGRenderTargetD3D11::ForgetTexture()
{
  if (mTexture) {
    sRenderTargetUsage -= mSize.width * mSize.height * 4;
    mTexture = nullptr;
  }
  mRTView = nullptr;
  mTextureSource = nullptr;
}

bool
MLGRenderTargetD3D11::CreateDepthBuffer(ID3D11Device* aDevice)
{
  D3D11_TEXTURE2D_DESC desc;
  ::ZeroMemory(&desc, sizeof(desc));
  desc.Width = mSize.width;
  desc.Height = mSize.height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_D32_FLOAT;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

  RefPtr<ID3D11Texture2D> buffer;
  HRESULT hr = aDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(buffer));
  if (FAILED(hr) || !buffer) {
    gfxCriticalNote << "Could not create depth-stencil buffer: " << hexa(hr);
    return false;
  }

  D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
  ::ZeroMemory(&viewDesc, sizeof(viewDesc));
  viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
  viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

  RefPtr<ID3D11DepthStencilView> dsv;
  hr = aDevice->CreateDepthStencilView(buffer, &viewDesc, getter_AddRefs(dsv));
  if (FAILED(hr) || !dsv) {
    gfxCriticalNote << "Could not create depth-stencil view: " << hexa(hr);
    return false;
  }

  mDepthBuffer = buffer;
  mDepthStencilView = dsv;
  sRenderTargetUsage += mSize.width * mSize.height * 1;
  return true;
}

ID3D11DepthStencilView*
MLGRenderTargetD3D11::GetDSV()
{
  return mDepthStencilView;
}

ID3D11RenderTargetView*
MLGRenderTargetD3D11::GetRenderTargetView()
{
  return mRTView;
}

IntSize
MLGRenderTargetD3D11::GetSize() const
{
  return mSize;
}

MLGTexture*
MLGRenderTargetD3D11::GetTexture()
{
  if (!mTextureSource) {
    mTextureSource = new MLGTextureD3D11(mTexture);
  }
  return mTextureSource;
}

MLGSwapChainD3D11::MLGSwapChainD3D11(MLGDeviceD3D11* aParent, ID3D11Device* aDevice)
 : mParent(aParent),
   mDevice(aDevice),
   mWidget(nullptr),
   mCanUsePartialPresents(CanUsePartialPresents(aDevice))
{
}

MLGSwapChainD3D11::~MLGSwapChainD3D11()
{
}

void
MLGSwapChainD3D11::Destroy()
{
  if (mRT == mParent->GetRenderTarget()) {
    mParent->SetRenderTarget(nullptr);
  }
  mWidget = nullptr;
  mRT = nullptr;
  mSwapChain = nullptr;
  mSwapChain1 = nullptr;
}

RefPtr<MLGSwapChainD3D11>
MLGSwapChainD3D11::Create(MLGDeviceD3D11* aParent, ID3D11Device* aDevice, CompositorWidget* aWidget)
{
  RefPtr<MLGSwapChainD3D11> swapChain = new MLGSwapChainD3D11(aParent, aDevice);
  if (!swapChain->Initialize(aWidget)) {
    return nullptr;
  }
  return swapChain.forget();
}

bool
MLGSwapChainD3D11::Initialize(CompositorWidget* aWidget)
{
  HWND hwnd = aWidget->AsWindows()->GetHwnd();

  RefPtr<IDXGIDevice> dxgiDevice;
  mDevice->QueryInterface(dxgiDevice.StartAssignment());

  RefPtr<IDXGIFactory> dxgiFactory;
  {
    RefPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(getter_AddRefs(adapter));

    adapter->GetParent(IID_PPV_ARGS(dxgiFactory.StartAssignment()));
  }

  RefPtr<IDXGIFactory2> dxgiFactory2;
  if (gfxPrefs::Direct3D11UseDoubleBuffering() &&
      SUCCEEDED(dxgiFactory->QueryInterface(dxgiFactory2.StartAssignment())) &&
      dxgiFactory2 &&
      IsWin10OrLater())
  {
    // DXGI_SCALING_NONE is not available on Windows 7 with the Platform Update:
    // This looks awful for things like the awesome bar and browser window
    // resizing, so we don't use a flip buffer chain here. (Note when using
    // EFFECT_SEQUENTIAL Windows doesn't stretch the surface when resizing).
    //
    // We choose not to run this on platforms earlier than Windows 10 because
    // it appears sometimes this breaks our ability to test ASAP compositing,
    // which breaks Talos.
    DXGI_SWAP_CHAIN_DESC1 desc;
    ::ZeroMemory(&desc, sizeof(desc));
    desc.Width = 0;
    desc.Height = 0;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.Flags = 0;

    HRESULT hr = dxgiFactory2->CreateSwapChainForHwnd(
      mDevice,
      hwnd,
      &desc,
      nullptr,
      nullptr,
      getter_AddRefs(mSwapChain1));
    if (SUCCEEDED(hr) && mSwapChain1) {
      DXGI_RGBA color = { 1.0f, 1.0f, 1.0f, 1.0f };
      mSwapChain1->SetBackgroundColor(&color);
      mSwapChain = mSwapChain1;
      mIsDoubleBuffered = true;
    }
  }

  if (!mSwapChain) {
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
    swapDesc.OutputWindow = hwnd;
    swapDesc.Windowed = TRUE;
    swapDesc.Flags = 0;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

    HRESULT hr = dxgiFactory->CreateSwapChain(dxgiDevice, &swapDesc, getter_AddRefs(mSwapChain));
    if (FAILED(hr)) {
      gfxCriticalNote << "Could not create swap chain: " << hexa(hr);
      return false;
    }

    // Try to get an IDXGISwapChain1 if we can, for partial presents.
    mSwapChain->QueryInterface(mSwapChain1.StartAssignment());
  }

  // We need this because we don't want DXGI to respond to Alt+Enter.
  dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
  mWidget = aWidget;
  return true;
}

RefPtr<MLGRenderTarget>
MLGSwapChainD3D11::AcquireBackBuffer()
{
  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), getter_AddRefs(texture));
  if (hr == DXGI_ERROR_INVALID_CALL && mDevice->GetDeviceRemovedReason() != S_OK) {
    // This can happen on some drivers when there's a TDR.
    mParent->HandleDeviceReset("SwapChain::GetBuffer");
    return nullptr;
  }
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to acquire swap chain's backbuffer: " << hexa(hr);
    return nullptr;
  }

  if (!mRT) {
    MLGRenderTargetFlags flags = MLGRenderTargetFlags::Default;
    if (gfxPrefs::AdvancedLayersEnableDepthBuffer()) {
      flags |= MLGRenderTargetFlags::ZBuffer;
    }

    mRT = new MLGRenderTargetD3D11(mSize, flags);
    if (!mRT->Initialize(mDevice, nullptr)) {
      return nullptr;
    }
  }

  if (!mRT->UpdateTexture(texture)) {
    return nullptr;
  }

  if (mIsDoubleBuffered) {
    UpdateBackBufferContents(texture);
  }
  return mRT;
}

void
MLGSwapChainD3D11::UpdateBackBufferContents(ID3D11Texture2D* aBack)
{
  MOZ_ASSERT(mIsDoubleBuffered);

  // The front region contains the newly invalid region for this frame. The
  // back region contains that, plus the region that was only drawn into the
  // back buffer on the previous frame. Thus by subtracting the two, we can
  // find the region that needs to be copied from the front buffer to the
  // back. We do this so we don't have to re-render those pixels.
  nsIntRegion frontValid;
  frontValid.Sub(mBackBufferInvalid, mFrontBufferInvalid);
  if (frontValid.IsEmpty()) {
    return;
  }

  RefPtr<ID3D11Texture2D> front;
  HRESULT hr = mSwapChain->GetBuffer(1, __uuidof(ID3D11Texture2D), getter_AddRefs(front));
  if (FAILED(hr) || !front) {
    return;
  }

  RefPtr<ID3D11DeviceContext> context;
  mDevice->GetImmediateContext(getter_AddRefs(context));

  for (auto iter = frontValid.RectIter(); !iter.Done(); iter.Next()) {
    const IntRect& rect = iter.Get();
    D3D11_BOX box = RectToBox(rect);
    context->CopySubresourceRegion(aBack, 0, rect.x, rect.y, 0, front, 0, &box);
  }

  // The back and front buffers are now in sync.
  mBackBufferInvalid = mFrontBufferInvalid;
  MOZ_ASSERT(!mBackBufferInvalid.IsEmpty());
}

bool
MLGSwapChainD3D11::ResizeBuffers(const IntSize& aSize)
{
  // We have to clear all references to the old backbuffer before resizing.
  mRT = nullptr;

  // Clear the size before re-allocating. If allocation fails we want to try
  // again, because we had to sacrifice our original backbuffer to try
  // resizing.
  mSize = IntSize(0, 0);

  HRESULT hr = mSwapChain->ResizeBuffers(0, aSize.width, aSize.height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
  if (hr == DXGI_ERROR_DEVICE_REMOVED) {
    mParent->HandleDeviceReset("ResizeBuffers");
    return false;
  }
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to resize swap chain buffers: " << hexa(hr);
    return false;
  }

  mSize = aSize;
  mBackBufferInvalid = IntRect(IntPoint(0, 0), mSize);
  mFrontBufferInvalid = IntRect(IntPoint(0, 0), mSize);
  return true;
}

IntSize
MLGSwapChainD3D11::GetSize() const
{
  return mSize;
}

void
MLGSwapChainD3D11::Present()
{
  MOZ_ASSERT(!mBackBufferInvalid.IsEmpty());
  MOZ_ASSERT(mBackBufferInvalid.GetNumRects() > 0);

  // See bug 1260611 comment #28 for why we do this.
  mParent->InsertPresentWaitQuery();

  HRESULT hr;
  if (mCanUsePartialPresents && mSwapChain1) {
    StackArray<RECT, 4> rects(mBackBufferInvalid.GetNumRects());
    size_t i = 0;
    for (auto iter = mBackBufferInvalid.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& rect = iter.Get();
      rects[i].left = rect.x;
      rects[i].top = rect.y;
      rects[i].bottom = rect.YMost();
      rects[i].right = rect.XMost();
      i++;
    }

    DXGI_PRESENT_PARAMETERS params;
    PodZero(&params);
    params.DirtyRectsCount = mBackBufferInvalid.GetNumRects();
    params.pDirtyRects = rects.data();
    hr = mSwapChain1->Present1(0, 0, &params);
  } else {
    hr = mSwapChain->Present(0, 0);
  }

  if (hr == DXGI_ERROR_DEVICE_REMOVED) {
    mParent->HandleDeviceReset("Present");
  }

  if (FAILED(hr)) {
    gfxCriticalNote << "D3D11 swap chain failed to present: " << hexa(hr);
  }

  if (mIsDoubleBuffered) {
    // Both the front and back buffer invalid regions are in sync, but now the
    // presented buffer (the front buffer) is clean, so we clear its invalid
    // region. The back buffer that will be used next frame however is now
    // dirty.
    MOZ_ASSERT(mFrontBufferInvalid.GetBounds() == mBackBufferInvalid.GetBounds());
    mFrontBufferInvalid.SetEmpty();
  } else {
    mBackBufferInvalid.SetEmpty();
  }
  mLastPresentSize = mSize;

  // Note: this waits on the query we inserted in the previous frame,
  // not the one we just inserted now. Example:
  //   Insert query #1
  //   Present #1
  //   (first frame, no wait)
  //   Insert query #2
  //   Present #2
  //   Wait for query #1.
  //   Insert query #3
  //   Present #3
  //   Wait for query #2.
  //
  // This ensures we're done reading textures before swapping buffers.
  mParent->WaitForPreviousPresentQuery();
}

void
MLGSwapChainD3D11::ForcePresent()
{
  DXGI_SWAP_CHAIN_DESC desc;
  mSwapChain->GetDesc(&desc);

  LayoutDeviceIntSize size = mWidget->GetClientSize();

  if (desc.BufferDesc.Width != size.width || desc.BufferDesc.Height != size.height) {
    return;
  }

  mSwapChain->Present(0, 0);
  if (mIsDoubleBuffered) {
    // Make sure we present the old front buffer since we know it is completely
    // valid. This non-vsynced present should be pretty much 'free' for a flip
    // chain.
    mSwapChain->Present(0, 0);
  }

  mLastPresentSize = mSize;
}

void
MLGSwapChainD3D11::CopyBackbuffer(gfx::DrawTarget* aTarget, const gfx::IntRect& aBounds)
{
  RefPtr<ID3D11DeviceContext> context;
  mDevice->GetImmediateContext(getter_AddRefs(context));

  RefPtr<ID3D11Texture2D> backbuffer;
  HRESULT hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuffer.StartAssignment());
  if (FAILED(hr)) {
    gfxWarning() << "Failed to acquire swapchain backbuffer: " << hexa(hr);
    return;
  }

  D3D11_TEXTURE2D_DESC bbDesc;
  backbuffer->GetDesc(&bbDesc);

  CD3D11_TEXTURE2D_DESC tempDesc(bbDesc.Format, bbDesc.Width, bbDesc.Height);
  tempDesc.MipLevels = 1;
  tempDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  tempDesc.Usage = D3D11_USAGE_STAGING;
  tempDesc.BindFlags = 0;

  RefPtr<ID3D11Texture2D> temp;
  hr = mDevice->CreateTexture2D(&tempDesc, nullptr, getter_AddRefs(temp));
  if (FAILED(hr)) {
    gfxWarning() << "Failed to create a temporary texture for PresentAndCopy: " << hexa(hr);
    return;
  }

  context->CopyResource(temp, backbuffer);

  D3D11_MAPPED_SUBRESOURCE map;
  hr = context->Map(temp, 0, D3D11_MAP_READ, 0, &map);
  if (FAILED(hr)) {
    gfxWarning() << "Failed to map temporary texture for PresentAndCopy: " << hexa(hr);
    return;
  }

  RefPtr<DataSourceSurface> source = Factory::CreateWrappingDataSourceSurface(
    (uint8_t*)map.pData,
    map.RowPitch,
    IntSize(bbDesc.Width, bbDesc.Height),
    SurfaceFormat::B8G8R8A8);

  aTarget->CopySurface(
    source,
    IntRect(0, 0, bbDesc.Width, bbDesc.Height),
    IntPoint(-aBounds.x, -aBounds.y));
  aTarget->Flush();

  context->Unmap(temp, 0);
}

RefPtr<MLGBufferD3D11>
MLGBufferD3D11::Create(ID3D11Device* aDevice,
                       MLGBufferType aType,
                       uint32_t aSize,
                       MLGUsage aUsage,
                       const void* aInitialData)
{
  D3D11_BUFFER_DESC desc;
  desc.ByteWidth = aSize;
  desc.MiscFlags = 0;
  desc.StructureByteStride = 0;

  switch (aUsage) {
    case MLGUsage::Dynamic:
      desc.Usage = D3D11_USAGE_DYNAMIC;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      break;
    case MLGUsage::Immutable:
      desc.Usage = D3D11_USAGE_IMMUTABLE;
      desc.CPUAccessFlags = 0;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown buffer usage type");
      return nullptr;
  }

  switch (aType) {
    case MLGBufferType::Vertex:
      desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      break;
    case MLGBufferType::Constant:
      desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown buffer type");
      return nullptr;
  }

  D3D11_SUBRESOURCE_DATA data;
  data.pSysMem = aInitialData;
  data.SysMemPitch = aSize;
  data.SysMemSlicePitch = 0;

  RefPtr<ID3D11Buffer> buffer;
  aDevice->CreateBuffer(&desc, aInitialData ? &data : nullptr, getter_AddRefs(buffer));

  return new MLGBufferD3D11(buffer, aType, aSize);
}

MLGBufferD3D11::MLGBufferD3D11(ID3D11Buffer* aBuffer, MLGBufferType aType, size_t aSize)
 : mBuffer(aBuffer),
   mType(aType),
   mSize(aSize)
{
  switch (mType) {
  case MLGBufferType::Vertex:
    mlg::sVertexBufferUsage += mSize;
    break;
  case MLGBufferType::Constant:
    mlg::sConstantBufferUsage += mSize;
    break;
  }
}

MLGBufferD3D11::~MLGBufferD3D11()
{
  switch (mType) {
  case MLGBufferType::Vertex:
    mlg::sVertexBufferUsage -= mSize;
    break;
  case MLGBufferType::Constant:
    mlg::sConstantBufferUsage -= mSize;
    break;
  }
}

MLGTextureD3D11::MLGTextureD3D11(ID3D11Texture2D* aTexture)
 : mTexture(aTexture)
{
  D3D11_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);

  mSize.width = desc.Width;
  mSize.height = desc.Height;
}

/* static */ RefPtr<MLGTextureD3D11>
MLGTextureD3D11::Create(ID3D11Device* aDevice,
                        const gfx::IntSize& aSize,
                        gfx::SurfaceFormat aFormat,
                        MLGUsage aUsage,
                        MLGTextureFlags aFlags)
{
  D3D11_TEXTURE2D_DESC desc;
  ::ZeroMemory(&desc, sizeof(desc));
  desc.Width = aSize.width;
  desc.Height = aSize.height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.SampleDesc.Count = 1;

  switch (aFormat) {
  case SurfaceFormat::B8G8R8A8:
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("Unsupported surface format");
    return nullptr;
  }

  switch (aUsage) {
  case MLGUsage::Immutable:
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    break;
  case MLGUsage::Default:
    desc.Usage = D3D11_USAGE_DEFAULT;
    break;
  case MLGUsage::Dynamic:
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    break;
  case MLGUsage::Staging:
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("Unsupported usage type");
    break;
  }

  if (aFlags & MLGTextureFlags::ShaderResource) {
    desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
  }
  if (aFlags & MLGTextureFlags::RenderTarget) {
    desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
  }

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr = aDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  if (FAILED(hr) || !texture) {
    gfxCriticalNote << "Failed to create 2D texture: " << hexa(hr);
    return nullptr;
  }

  ReportTextureMemoryUsage(texture, aSize.width * aSize.height * 4);

  return new MLGTextureD3D11(texture);
}

ID3D11ShaderResourceView*
MLGTextureD3D11::GetShaderResourceView()
{
  if (!mView) {
    RefPtr<ID3D11Device> device;
    mTexture->GetDevice(getter_AddRefs(device));

    HRESULT hr = device->CreateShaderResourceView(mTexture, nullptr, getter_AddRefs(mView));
    if (FAILED(hr) || !mView) {
      gfxWarning() << "Could not create shader resource view: " << hexa(hr);
      return nullptr;
    }
  }
  return mView;
}

MLGDeviceD3D11::MLGDeviceD3D11(ID3D11Device* aDevice)
 : mDevice(aDevice),
   mScissored(false)
{
}

MLGDeviceD3D11::~MLGDeviceD3D11()
{
  // Caller should have unlocked all textures after presenting.
  MOZ_ASSERT(mLockedTextures.IsEmpty());
  MOZ_ASSERT(mLockAttemptedTextures.IsEmpty());
}

bool
MLGDeviceD3D11::Initialize()
{
  if (!mDevice) {
    return Fail("FEATURE_FAILURE_NO_DEVICE");
  }

  if (mDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0) {
    return Fail("FEATURE_FAILURE_NEED_LEVEL_10_0");
  }

  mDevice->GetImmediateContext(getter_AddRefs(mCtx));
  if (!mCtx) {
    return Fail("FEATURE_FAILURE_NO_CONTEXT");
  }

  mCtx->QueryInterface((ID3D11DeviceContext1**)getter_AddRefs(mCtx1));

  if (mCtx1) {
    // Windows 7 can have Direct3D 11.1 if the platform update is installed,
    // but according to some NVIDIA presentations it is known to be buggy.
    // It's not clear whether that only refers to command list emulation,
    // or whether it just has performance penalties. To be safe we only use
    // it on Windows 8 or higher.
    //
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/display/directx-feature-improvements-in-windows-8#buffers
    D3D11_FEATURE_DATA_D3D11_OPTIONS options;
    HRESULT hr = mDevice->CheckFeatureSupport(
      D3D11_FEATURE_D3D11_OPTIONS,
      &options,
      sizeof(options));
    if (SUCCEEDED(hr)) {
      if (IsWin8OrLater()) {
        mCanUseConstantBufferOffsetBinding = (options.ConstantBufferOffsetting != FALSE);
      }
      mCanUseClearView = (options.ClearView != FALSE);
    }
  }

  // Get capabilities.
  switch (mDevice->GetFeatureLevel()) {
  case D3D_FEATURE_LEVEL_11_1:
  case D3D_FEATURE_LEVEL_11_0:
    mMaxConstantBufferBindSize = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    break;
  case D3D_FEATURE_LEVEL_10_1:
  case D3D_FEATURE_LEVEL_10_0:
    mMaxConstantBufferBindSize = D3D10_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("Unknown feature level");
  }

  mDiagnostics = MakeUnique<DiagnosticsD3D11>(mDevice, mCtx);

  {
    struct Vertex2D { float x; float y; };
    Vertex2D vertices[] = { { 0, 0 }, { 1.0f, 0 }, { 0, 1.0f }, { 1.0f, 1.0f } };
    mUnitQuadVB = CreateBuffer(
      MLGBufferType::Vertex,
      sizeof(Vertex2D) * 4,
      MLGUsage::Immutable,
      &vertices);
    if (!mUnitQuadVB) {
      return Fail("FEATURE_FAILURE_UNIT_QUAD_BUFFER");
    }
  }

  {
    struct Vertex3D { float x; float y; float z; };
    Vertex3D vertices[3] = {
      { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }
    };
    mUnitTriangleVB = CreateBuffer(
      MLGBufferType::Vertex,
      sizeof(Vertex3D) * 3,
      MLGUsage::Immutable,
      &vertices);
    if (!mUnitTriangleVB) {
      return Fail("FEATURE_FAILURE_UNIT_TRIANGLE_BUFFER");
    }
  }

  // Define pixel shaders.
#define LAZY_PS(cxxName, enumName) mLazyPixelShaders[PixelShaderID::enumName] = &s##cxxName;
  LAZY_PS(TexturedVertexRGB, TexturedVertexRGB);
  LAZY_PS(TexturedVertexRGBA, TexturedVertexRGBA);
  LAZY_PS(TexturedQuadRGB, TexturedQuadRGB);
  LAZY_PS(TexturedQuadRGBA, TexturedQuadRGBA);
  LAZY_PS(ColoredQuadPS, ColoredQuad);
  LAZY_PS(ColoredVertexPS, ColoredVertex);
  LAZY_PS(ComponentAlphaQuadPS, ComponentAlphaQuad);
  LAZY_PS(ComponentAlphaVertexPS, ComponentAlphaVertex);
  LAZY_PS(TexturedVertexIMC4, TexturedVertexIMC4);
  LAZY_PS(TexturedVertexNV12, TexturedVertexNV12);
  LAZY_PS(TexturedQuadIMC4, TexturedQuadIMC4);
  LAZY_PS(TexturedQuadNV12, TexturedQuadNV12);
  LAZY_PS(BlendMultiplyPS, BlendMultiply);
  LAZY_PS(BlendScreenPS, BlendScreen);
  LAZY_PS(BlendOverlayPS, BlendOverlay);
  LAZY_PS(BlendDarkenPS, BlendDarken);
  LAZY_PS(BlendLightenPS, BlendLighten);
  LAZY_PS(BlendColorDodgePS, BlendColorDodge);
  LAZY_PS(BlendColorBurnPS, BlendColorBurn);
  LAZY_PS(BlendHardLightPS, BlendHardLight);
  LAZY_PS(BlendSoftLightPS, BlendSoftLight);
  LAZY_PS(BlendDifferencePS, BlendDifference);
  LAZY_PS(BlendExclusionPS, BlendExclusion);
  LAZY_PS(BlendHuePS, BlendHue);
  LAZY_PS(BlendSaturationPS, BlendSaturation);
  LAZY_PS(BlendColorPS, BlendColor);
  LAZY_PS(BlendLuminosityPS, BlendLuminosity);
  LAZY_PS(ClearPS, Clear);
  LAZY_PS(MaskCombinerPS, MaskCombiner);
  LAZY_PS(DiagnosticTextPS, DiagnosticText);
#undef LAZY_PS

  // Define vertex shaders.
#define LAZY_VS(cxxName, enumName) mLazyVertexShaders[VertexShaderID::enumName] = &s##cxxName;
  LAZY_VS(TexturedQuadVS, TexturedQuad);
  LAZY_VS(TexturedVertexVS, TexturedVertex);
  LAZY_VS(BlendVertexVS, BlendVertex);
  LAZY_VS(ColoredQuadVS, ColoredQuad);
  LAZY_VS(ColoredVertexVS, ColoredVertex);
  LAZY_VS(ClearVS, Clear);
  LAZY_VS(MaskCombinerVS, MaskCombiner);
  LAZY_VS(DiagnosticTextVS, DiagnosticText);
#undef LAZY_VS

  // Force critical shaders to initialize early.
  if (!InitPixelShader(PixelShaderID::TexturedQuadRGB) ||
      !InitPixelShader(PixelShaderID::TexturedQuadRGBA) ||
      !InitPixelShader(PixelShaderID::ColoredQuad) ||
      !InitPixelShader(PixelShaderID::ComponentAlphaQuad) ||
      !InitPixelShader(PixelShaderID::Clear) ||
      !InitVertexShader(VertexShaderID::TexturedQuad) ||
      !InitVertexShader(VertexShaderID::ColoredQuad) ||
      !InitVertexShader(VertexShaderID::Clear))
  {
    return Fail("FEATURE_FAILURE_CRITICAL_SHADER_FAILURE");
  }

  // Common unit quad layout: vPos, vRect, vLayerIndex, vDepth
# define BASE_UNIT_QUAD_LAYOUT \
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, \
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, \
      { "TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, \
      { "TEXCOORD", 2, DXGI_FORMAT_R32_SINT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }

  // Common unit triangle layout: vUnitPos, vPos1-3, vLayerIndex, vDepth
# define BASE_UNIT_TRIANGLE_LAYOUT \
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, \
      { "POSITION", 1, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, \
      { "POSITION", 2, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, \
      { "POSITION", 3, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, \
      { "TEXCOORD", 0, DXGI_FORMAT_R32_UINT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, \
      { "TEXCOORD", 1, DXGI_FORMAT_R32_SINT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }

  // Initialize input layouts.
  {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
      BASE_UNIT_QUAD_LAYOUT,
      // vTexRect
      { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
    };
    if (!InitInputLayout(inputDesc, MOZ_ARRAY_LENGTH(inputDesc), sTexturedQuadVS, VertexShaderID::TexturedQuad)) {
      return Fail("FEATURE_FAILURE_UNIT_QUAD_TEXTURED_LAYOUT");
    }
  }
  {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
      BASE_UNIT_QUAD_LAYOUT,
      // vColor
      { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
    };
    if (!InitInputLayout(inputDesc, MOZ_ARRAY_LENGTH(inputDesc), sColoredQuadVS, VertexShaderID::ColoredQuad)) {
      return Fail("FEATURE_FAILURE_UNIT_QUAD_COLORED_LAYOUT");
    }
  }
  {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
      BASE_UNIT_TRIANGLE_LAYOUT,
      // vTexCoord1, vTexCoord2, vTexCoord3
      { "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
      { "TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
      { "TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
    };
    if (!InitInputLayout(inputDesc, MOZ_ARRAY_LENGTH(inputDesc), sTexturedVertexVS, VertexShaderID::TexturedVertex)) {
      return Fail("FEATURE_FAILURE_TEXTURED_INPUT_LAYOUT");
    }
    // Propagate the input layout to other vertex shaders that use the same.
    mInputLayouts[VertexShaderID::BlendVertex] = mInputLayouts[VertexShaderID::TexturedVertex];
  }
  {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
      BASE_UNIT_TRIANGLE_LAYOUT,
      { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
    };
    if (!InitInputLayout(inputDesc, MOZ_ARRAY_LENGTH(inputDesc), sColoredVertexVS, VertexShaderID::ColoredVertex)) {
      return Fail("FEATURE_FAILURE_COLORED_INPUT_LAYOUT");
    }
  }

# undef BASE_UNIT_QUAD_LAYOUT
# undef BASE_UNIT_TRIANGLE_LAYOUT

  // Ancillary shaders that are not used for batching.
  {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
      // vPos
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      // vRect
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_SINT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
      // vDepth
      { "TEXCOORD", 1, DXGI_FORMAT_R32_SINT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    if (!InitInputLayout(inputDesc, MOZ_ARRAY_LENGTH(inputDesc), sClearVS, VertexShaderID::Clear)) {
      return Fail("FEATURE_FAILURE_CLEAR_INPUT_LAYOUT");
    }
  }
  {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
      // vPos
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      // vTexCoords
      { "POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
    };
    if (!InitInputLayout(inputDesc, MOZ_ARRAY_LENGTH(inputDesc), sMaskCombinerVS, VertexShaderID::MaskCombiner)) {
      return Fail("FEATURE_FAILURE_MASK_COMBINER_INPUT_LAYOUT");
    }
  }
  {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
      // vPos
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      // vRect
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
      // vTexCoords
      { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    if (!InitInputLayout(inputDesc, MOZ_ARRAY_LENGTH(inputDesc), sDiagnosticTextVS, VertexShaderID::DiagnosticText)) {
      return Fail("FEATURE_FAILURE_DIAGNOSTIC_INPUT_LAYOUT");
    }
  }

  if (!InitRasterizerStates() ||
      !InitDepthStencilState() ||
      !InitBlendStates() ||
      !InitSamplerStates() ||
      !InitSyncObject())
  {
    return false;
  }

  mCtx->RSSetState(mRasterizerStateNoScissor);

  return MLGDevice::Initialize();
}

bool
MLGDeviceD3D11::InitPixelShader(PixelShaderID aShaderID)
{
  const ShaderBytes* code = mLazyPixelShaders[aShaderID];
  HRESULT hr = mDevice->CreatePixelShader(
    code->mData,
    code->mLength,
    nullptr,
    getter_AddRefs(mPixelShaders[aShaderID]));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create pixel shader " << hexa(unsigned(aShaderID)) << ": " << hexa(hr);
    return false;
  }
  return true;
}

bool
MLGDeviceD3D11::InitRasterizerStates()
{
  {
    CD3D11_RASTERIZER_DESC desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
    desc.CullMode = D3D11_CULL_NONE;
    desc.FillMode = D3D11_FILL_SOLID;
    desc.ScissorEnable = TRUE;
    HRESULT hr = mDevice->CreateRasterizerState(&desc, getter_AddRefs(mRasterizerStateScissor));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_SCISSOR_RASTERIZER",
                  "Could not create scissor rasterizer (%x)", hr);
    }
  }
  {
    CD3D11_RASTERIZER_DESC desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
    desc.CullMode = D3D11_CULL_NONE;
    desc.FillMode = D3D11_FILL_SOLID;
    HRESULT hr = mDevice->CreateRasterizerState(&desc, getter_AddRefs(mRasterizerStateNoScissor));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_DEFAULT_RASTERIZER",
                  "Could not create default rasterizer (%x)", hr);
    }
  }
  return true;
}

bool
MLGDeviceD3D11::InitSamplerStates()
{
  {
    CD3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
    HRESULT hr = mDevice->CreateSamplerState(&desc, getter_AddRefs(mSamplerStates[SamplerMode::LinearClamp]));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_LINEAR_CLAMP_SAMPLER",
                  "Could not create linear clamp sampler (%x)", hr);
    }
  }
  {
    CD3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
    desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    memset(desc.BorderColor, 0, sizeof(desc.BorderColor));
    HRESULT hr = mDevice->CreateSamplerState(&desc, getter_AddRefs(mSamplerStates[SamplerMode::LinearClampToZero]));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_LINEAR_CLAMP_ZERO_SAMPLER",
                  "Could not create linear clamp to zero sampler (%x)", hr);
    }
  }

  {
    CD3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    HRESULT hr = mDevice->CreateSamplerState(&desc, getter_AddRefs(mSamplerStates[SamplerMode::Point]));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_POINT_SAMPLER",
                  "Could not create point sampler (%x)", hr);
    }
  }
  return true;
}

bool
MLGDeviceD3D11::InitBlendStates()
{
  {
    CD3D11_BLEND_DESC desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
    HRESULT hr = mDevice->CreateBlendState(&desc, getter_AddRefs(mBlendStates[MLGBlendState::Copy]));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_COPY_BLEND_STATE",
                  "Could not create copy blend state (%x)", hr);
    }
  }

  {
    CD3D11_BLEND_DESC desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    HRESULT hr = mDevice->CreateBlendState(&desc, getter_AddRefs(mBlendStates[MLGBlendState::Over]));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_OVER_BLEND_STATE",
                  "Could not create over blend state (%x)", hr);
    }
  }

  {
    CD3D11_BLEND_DESC desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    HRESULT hr = mDevice->CreateBlendState(&desc, getter_AddRefs(mBlendStates[MLGBlendState::OverAndPremultiply]));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_OVER_BLEND_STATE",
                  "Could not create over blend state (%x)", hr);
    }
  }

  {
    CD3D11_BLEND_DESC desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MIN;
    HRESULT hr = mDevice->CreateBlendState(&desc, getter_AddRefs(mBlendStates[MLGBlendState::Min]));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_MIN_BLEND_STATE",
                  "Could not create min blend state (%x)", hr);
    }
  }

  {
    CD3D11_BLEND_DESC desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_COLOR;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    HRESULT hr = mDevice->CreateBlendState(&desc, getter_AddRefs(mBlendStates[MLGBlendState::ComponentAlpha]));
    if (FAILED(hr)) {
      return Fail("FEATURE_FAILURE_COMPONENT_ALPHA_BLEND_STATE",
                  "Could not create component alpha blend state (%x)", hr);
    }
  }
  return true;
}

bool
MLGDeviceD3D11::InitDepthStencilState()
{
  D3D11_DEPTH_STENCIL_DESC desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);

  HRESULT hr = mDevice->CreateDepthStencilState(
    &desc, getter_AddRefs(mDepthStencilStates[MLGDepthTestMode::Write]));
  if (FAILED(hr)) {
    return Fail("FEATURE_FAILURE_WRITE_DEPTH_STATE",
                "Could not create write depth stencil state (%x)", hr);
  }

  desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  hr = mDevice->CreateDepthStencilState(
    &desc, getter_AddRefs(mDepthStencilStates[MLGDepthTestMode::ReadOnly]));
  if (FAILED(hr)) {
    return Fail("FEATURE_FAILURE_READ_DEPTH_STATE",
                "Could not create read depth stencil state (%x)", hr);
  }

  desc.DepthEnable = FALSE;
  hr = mDevice->CreateDepthStencilState(
    &desc, getter_AddRefs(mDepthStencilStates[MLGDepthTestMode::Disabled]));
  if (FAILED(hr)) {
    return Fail("FEATURE_FAILURE_DISABLED_DEPTH_STATE",
                "Could not create disabled depth stencil state (%x)", hr);
  }

  desc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
  desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
  hr = mDevice->CreateDepthStencilState(
    &desc, getter_AddRefs(mDepthStencilStates[MLGDepthTestMode::AlwaysWrite]));
  if (FAILED(hr)) {
    return Fail("FEATURE_FAILURE_WRITE_DEPTH_STATE",
                "Could not create always-write depth stencil state (%x)", hr);
  }

  return true;
}

bool
MLGDeviceD3D11::InitVertexShader(VertexShaderID aShaderID)
{
  const ShaderBytes* code = mLazyVertexShaders[aShaderID];
  HRESULT hr = mDevice->CreateVertexShader(
    code->mData,
    code->mLength,
    nullptr,
    getter_AddRefs(mVertexShaders[aShaderID]));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create vertex shader " << hexa(unsigned(aShaderID)) << ": " << hexa(hr);
    return false;
  }
  return true;
}

bool
MLGDeviceD3D11::InitInputLayout(D3D11_INPUT_ELEMENT_DESC* aDesc,
                                size_t aNumElements,
                                const ShaderBytes& aCode,
                                VertexShaderID aShaderID)
{
  HRESULT hr = mDevice->CreateInputLayout(
    aDesc,
    aNumElements,
    aCode.mData,
    aCode.mLength,
    getter_AddRefs(mInputLayouts[aShaderID]));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create input layout for shader "
      << hexa(unsigned(aShaderID))
      << ": " << hexa(hr);
    return false;
  }
  return true;
}

TextureFactoryIdentifier
MLGDeviceD3D11::GetTextureFactoryIdentifier() const
{
  TextureFactoryIdentifier ident(
    GetLayersBackend(),
    XRE_GetProcessType(),
    GetMaxTextureSize());
  ident.mSyncHandle = mSyncHandle;
  return ident;
}

inline uint32_t GetMaxTextureSizeForFeatureLevel1(D3D_FEATURE_LEVEL aFeatureLevel)
{
  int32_t maxTextureSize;
  switch (aFeatureLevel) {
  case D3D_FEATURE_LEVEL_11_1:
  case D3D_FEATURE_LEVEL_11_0:
    maxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    break;
  case D3D_FEATURE_LEVEL_10_1:
  case D3D_FEATURE_LEVEL_10_0:
    maxTextureSize = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    break;
  case D3D_FEATURE_LEVEL_9_3:
    maxTextureSize = D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    break;
  default:
    maxTextureSize = D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  }
  return maxTextureSize;
}

LayersBackend
MLGDeviceD3D11::GetLayersBackend() const
{
  return LayersBackend::LAYERS_D3D11;
}

int32_t
MLGDeviceD3D11::GetMaxTextureSize() const
{
  return GetMaxTextureSizeForFeatureLevel1(mDevice->GetFeatureLevel());
}

RefPtr<MLGSwapChain>
MLGDeviceD3D11::CreateSwapChainForWidget(widget::CompositorWidget* aWidget)
{
  return MLGSwapChainD3D11::Create(this, mDevice, aWidget);
}

RefPtr<DataTextureSource>
MLGDeviceD3D11::CreateDataTextureSource(TextureFlags aFlags)
{
  return new DataTextureSourceD3D11(mDevice, gfx::SurfaceFormat::UNKNOWN, aFlags);
}

static inline D3D11_MAP
ToD3D11Map(MLGMapType aType)
{
  switch (aType) {
  case MLGMapType::READ:
    return D3D11_MAP_READ;
  case MLGMapType::READ_WRITE:
    return D3D11_MAP_READ_WRITE;
  case MLGMapType::WRITE:
    return D3D11_MAP_WRITE;
  case MLGMapType::WRITE_DISCARD:
    return D3D11_MAP_WRITE_DISCARD;
  }
  return D3D11_MAP_WRITE;
}

bool
MLGDeviceD3D11::Map(MLGResource* aResource, MLGMapType aType, MLGMappedResource* aMap)
{
  ID3D11Resource* resource = aResource->AsResourceD3D11()->GetResource();

  D3D11_MAPPED_SUBRESOURCE map;
  HRESULT hr = mCtx->Map(resource, 0, ToD3D11Map(aType), 0, &map);

  if (FAILED(hr)) {
    gfxWarning() << "Could not map MLG resource: " << hexa(hr);
    return false;
  }

  aMap->mData = reinterpret_cast<uint8_t*>(map.pData);
  aMap->mStride = map.RowPitch;
  return true;
}

void
MLGDeviceD3D11::Unmap(MLGResource* aResource)
{
  ID3D11Resource* resource = aResource->AsResourceD3D11()->GetResource();
  mCtx->Unmap(resource, 0);
}

void
MLGDeviceD3D11::UpdatePartialResource(MLGResource* aResource,
                                      const gfx::IntRect* aRect,
                                      void* aData, uint32_t aStride)
{
  D3D11_BOX box;
  if (aRect) {
    box = RectToBox(*aRect);
  }

  ID3D11Resource* resource = aResource->AsResourceD3D11()->GetResource();
  mCtx->UpdateSubresource(resource, 0, aRect ? &box : nullptr, aData, aStride, 0);
}

void
MLGDeviceD3D11::SetRenderTarget(MLGRenderTarget* aRT)
{
  ID3D11RenderTargetView* rtv = nullptr;
  ID3D11DepthStencilView* dsv = nullptr;

  if (aRT) {
    MLGRenderTargetD3D11* rt = aRT->AsD3D11();
    rtv = rt->GetRenderTargetView();
    dsv = rt->GetDSV();
  }

  mCtx->OMSetRenderTargets(1, &rtv, dsv);
  mCurrentRT = aRT;
}

MLGRenderTarget*
MLGDeviceD3D11::GetRenderTarget()
{
  return mCurrentRT;
}

void
MLGDeviceD3D11::SetViewport(const gfx::IntRect& aViewport)
{
  D3D11_VIEWPORT vp;
  vp.MaxDepth = 1.0f;
  vp.MinDepth = 0.0f;
  vp.TopLeftX = aViewport.x;
  vp.TopLeftY = aViewport.y;
  vp.Width = aViewport.width;
  vp.Height = aViewport.height;
  mCtx->RSSetViewports(1, &vp);
}

static inline D3D11_RECT
ToD3D11Rect(const gfx::IntRect& aRect)
{
  D3D11_RECT rect;
  rect.left = aRect.x;
  rect.top = aRect.y;
  rect.right = aRect.XMost();
  rect.bottom = aRect.YMost();
  return rect;
}

void
MLGDeviceD3D11::SetScissorRect(const Maybe<gfx::IntRect>& aScissorRect)
{
  if (!aScissorRect) {
    if (mScissored) {
      mCtx->RSSetState(mRasterizerStateNoScissor);
      mScissored = false;
    }
    return;
  }
  D3D11_RECT rect = ToD3D11Rect(aScissorRect.value());
  mCtx->RSSetScissorRects(1, &rect);
  if (!mScissored) {
    mScissored = true;
    mCtx->RSSetState(mRasterizerStateScissor);
  }
}

void
MLGDeviceD3D11::SetVertexShader(VertexShaderID aShader)
{
  if (!mVertexShaders[aShader]) {
    InitVertexShader(aShader);
    MOZ_ASSERT(mInputLayouts[aShader]);
  }
  if (mCurrentVertexShader != mVertexShaders[aShader]) {
    mCtx->VSSetShader(mVertexShaders[aShader], nullptr, 0);
    mCurrentVertexShader = mVertexShaders[aShader];
  }
  if (mCurrentInputLayout != mInputLayouts[aShader]) {
    mCtx->IASetInputLayout(mInputLayouts[aShader]);
    mCurrentInputLayout = mInputLayouts[aShader];
  }
}

void
MLGDeviceD3D11::SetPixelShader(PixelShaderID aShader)
{
  if (!mPixelShaders[aShader]) {
    InitPixelShader(aShader);
  }
  if (mCurrentPixelShader != mPixelShaders[aShader]) {
    mCtx->PSSetShader(mPixelShaders[aShader], nullptr, 0);
    mCurrentPixelShader = mPixelShaders[aShader];
  }
}

void
MLGDeviceD3D11::SetSamplerMode(uint32_t aIndex, SamplerMode aMode)
{
  ID3D11SamplerState* sampler = mSamplerStates[aMode];
  mCtx->PSSetSamplers(aIndex, 1, &sampler);
}

void
MLGDeviceD3D11::SetBlendState(MLGBlendState aState)
{
  if (mCurrentBlendState != mBlendStates[aState]) {
    FLOAT blendFactor[4] = { 0, 0, 0, 0 };
    mCtx->OMSetBlendState(mBlendStates[aState], blendFactor, 0xFFFFFFFF);
    mCurrentBlendState = mBlendStates[aState];
  }
}

void
MLGDeviceD3D11::SetVertexBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aStride, uint32_t aOffset)
{
  ID3D11Buffer* buffer = aBuffer ? aBuffer->AsD3D11()->GetBuffer() : nullptr;
  mCtx->IASetVertexBuffers(aSlot, 1, &buffer, &aStride, &aOffset);
}

void
MLGDeviceD3D11::SetVSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer)
{
  MOZ_ASSERT(aSlot < kMaxVertexShaderConstantBuffers);

  ID3D11Buffer* buffer = aBuffer ? aBuffer->AsD3D11()->GetBuffer() : nullptr;
  mCtx->VSSetConstantBuffers(aSlot, 1, &buffer);
}

void
MLGDeviceD3D11::SetPSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer)
{
  MOZ_ASSERT(aSlot < kMaxPixelShaderConstantBuffers);

  ID3D11Buffer* buffer = aBuffer ? aBuffer->AsD3D11()->GetBuffer() : nullptr;
  mCtx->PSSetConstantBuffers(aSlot, 1, &buffer);
}

void
MLGDeviceD3D11::SetVSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aFirstConstant, uint32_t aNumConstants)
{
  MOZ_ASSERT(aSlot < kMaxVertexShaderConstantBuffers);
  MOZ_ASSERT(mCanUseConstantBufferOffsetBinding);
  MOZ_ASSERT(mCtx1);
  MOZ_ASSERT(aFirstConstant % 16 == 0);

  ID3D11Buffer* buffer = aBuffer ? aBuffer->AsD3D11()->GetBuffer() : nullptr;
  mCtx1->VSSetConstantBuffers1(aSlot, 1, &buffer, &aFirstConstant, &aNumConstants);
}

void
MLGDeviceD3D11::SetPSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aFirstConstant, uint32_t aNumConstants)
{
  MOZ_ASSERT(aSlot < kMaxPixelShaderConstantBuffers);
  MOZ_ASSERT(mCanUseConstantBufferOffsetBinding);
  MOZ_ASSERT(mCtx1);
  MOZ_ASSERT(aFirstConstant % 16 == 0);

  ID3D11Buffer* buffer = aBuffer ? aBuffer->AsD3D11()->GetBuffer() : nullptr;
  mCtx1->PSSetConstantBuffers1(aSlot, 1, &buffer, &aFirstConstant, &aNumConstants);
}

void
MLGDeviceD3D11::SetPrimitiveTopology(MLGPrimitiveTopology aTopology)
{
  D3D11_PRIMITIVE_TOPOLOGY topology;
  switch (aTopology) {
  case MLGPrimitiveTopology::TriangleStrip:
    topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    break;
  case MLGPrimitiveTopology::TriangleList:
    topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    break;
  case MLGPrimitiveTopology::UnitQuad:
    SetVertexBuffer(0, mUnitQuadVB, sizeof(float) * 2, 0);
    topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    break;
  case MLGPrimitiveTopology::UnitTriangle:
    SetVertexBuffer(0, mUnitTriangleVB, sizeof(float) * 3, 0);
    topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("Unknown topology");
    break;
  }

  mCtx->IASetPrimitiveTopology(topology);
}

RefPtr<MLGBuffer>
MLGDeviceD3D11::CreateBuffer(MLGBufferType aType,
                             uint32_t aSize,
                             MLGUsage aUsage,
                             const void* aInitialData)
{
  return MLGBufferD3D11::Create(mDevice, aType, aSize, aUsage, aInitialData);
}

RefPtr<MLGRenderTarget>
MLGDeviceD3D11::CreateRenderTarget(const gfx::IntSize& aSize, MLGRenderTargetFlags aFlags)
{
  RefPtr<MLGRenderTargetD3D11> rt = new MLGRenderTargetD3D11(aSize, aFlags);
  if (!rt->Initialize(mDevice)) {
    return nullptr;
  }
  return rt;
}

void
MLGDeviceD3D11::Clear(MLGRenderTarget* aRT, const gfx::Color& aColor)
{
  MLGRenderTargetD3D11* rt = aRT->AsD3D11();
  FLOAT rgba[4] = { aColor.r, aColor.g, aColor.b, aColor.a };
  mCtx->ClearRenderTargetView(rt->GetRenderTargetView(), rgba);
  if (ID3D11DepthStencilView* dsv = rt->GetDSV()) {
    mCtx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0, 0);
  }
}

void
MLGDeviceD3D11::ClearDepthBuffer(MLGRenderTarget* aRT)
{
  MLGRenderTargetD3D11* rt = aRT->AsD3D11();
  if (ID3D11DepthStencilView* dsv = rt->GetDSV()) {
    mCtx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0, 0);
  }
}

void
MLGDeviceD3D11::ClearView(MLGRenderTarget* aRT,
                          const Color& aColor,
                          const IntRect* aRects,
                          size_t aNumRects)
{
  MOZ_ASSERT(mCanUseClearView);
  MOZ_ASSERT(mCtx1);

  MLGRenderTargetD3D11* rt = aRT->AsD3D11();
  FLOAT rgba[4] = { aColor.r, aColor.g, aColor.b, aColor.a };

  StackArray<D3D11_RECT, 8> rects(aNumRects);
  for (size_t i = 0; i < aNumRects; i++) {
    rects[i] = ToD3D11Rect(aRects[i]);
  }

  // Batch ClearView calls since too many will crash NVIDIA drivers.
  size_t remaining = aNumRects;
  size_t cursor = 0;
  while (remaining > 0) {
    size_t amount = std::min(remaining, kMaxClearViewRects);
    mCtx1->ClearView(rt->GetRenderTargetView(), rgba, rects.data() + cursor, amount);

    remaining -= amount;
    cursor += amount;
  }
}

void
MLGDeviceD3D11::Draw(uint32_t aVertexCount, uint32_t aOffset)
{
  mCtx->Draw(aVertexCount, aOffset);
}

void
MLGDeviceD3D11::DrawInstanced(uint32_t aVertexCountPerInstance, uint32_t aInstanceCount,
                              uint32_t aVertexOffset, uint32_t aInstanceOffset)
{
  mCtx->DrawInstanced(aVertexCountPerInstance, aInstanceCount, aVertexOffset, aInstanceOffset);
}

void
MLGDeviceD3D11::SetPSTextures(uint32_t aSlot, uint32_t aNumTextures, TextureSource* const* aTextures)
{
  StackArray<ID3D11ShaderResourceView*, 2> textures(aNumTextures);

  for (size_t i = 0; i < aNumTextures; i++) {
    if (!aTextures[i]) {
      gfxWarning() << "Null TextureRef in SetPSTextures";
      continue;
    }

    ID3D11ShaderResourceView* view = nullptr;
    if (TextureSourceD3D11* source = aTextures[i]->AsSourceD3D11()) {
      ID3D11Texture2D* texture = source->GetD3D11Texture();
      if (!texture) {
        gfxWarning() << "No D3D11 texture present in SetPSTextures";
        continue;
      }
      MaybeLockTexture(texture);

      view = source->GetShaderResourceView();
    } else {
      gfxWarning() << "Unknown texture type in SetPSTextures";
      continue;
    }

    if (!view) {
      gfxWarning() << "Failed to get shader resource view for texture";
      continue;
    }
    textures[i] = view;
  }

  mCtx->PSSetShaderResources(aSlot, aNumTextures, textures.data());
}

void
MLGDeviceD3D11::SetPSTexture(uint32_t aSlot, MLGTexture* aTexture)
{
  RefPtr<ID3D11ShaderResourceView> view;
  if (aTexture) {
    MLGTextureD3D11* texture = aTexture->AsD3D11();
    view = texture->GetShaderResourceView();
  }

  ID3D11ShaderResourceView* viewPtr = view.get();
  mCtx->PSSetShaderResources(aSlot, 1, &viewPtr);
}

void
MLGDeviceD3D11::MaybeLockTexture(ID3D11Texture2D* aTexture)
{
  RefPtr<IDXGIKeyedMutex> mutex;
  HRESULT hr = aTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(mutex));
  if (FAILED(hr) || !mutex) {
    return;
  }

  hr = mutex->AcquireSync(0, 10000);

  if (hr == WAIT_TIMEOUT) {
    gfxDevCrash(LogReason::D3DLockTimeout) << "D3D lock mutex timeout";
    mLockAttemptedTextures.PutEntry(mutex);
  } else if (hr == WAIT_ABANDONED) {
    gfxCriticalNote << "GFX: D3D11 lock mutex abandoned";
    mLockAttemptedTextures.PutEntry(mutex);
  } else if (FAILED(hr)) {
    gfxCriticalNote << "D3D11 lock mutex failed: " << hexa(hr);
    mLockAttemptedTextures.PutEntry(mutex);
  } else {
    mLockedTextures.PutEntry(mutex);
  }
}

void
MLGDeviceD3D11::SetPSTexturesNV12(uint32_t aSlot, TextureSource* aTexture)
{
  MOZ_ASSERT(aTexture->GetFormat() == SurfaceFormat::NV12);

  TextureSourceD3D11* source = aTexture->AsSourceD3D11();
  if (!source) {
    gfxWarning() << "Unknown texture type in SetPSCompoundTexture";
    return;
  }

  ID3D11Texture2D* texture = source->GetD3D11Texture();
  if (!texture) {
    gfxWarning() << "TextureSourceD3D11 does not have an ID3D11Texture";
    return;
  }

  MaybeLockTexture(texture);

  RefPtr<ID3D11ShaderResourceView> views[2];
  D3D11_SHADER_RESOURCE_VIEW_DESC desc =
    CD3D11_SHADER_RESOURCE_VIEW_DESC(
      D3D11_SRV_DIMENSION_TEXTURE2D,
      DXGI_FORMAT_R8_UNORM);

  HRESULT hr = mDevice->CreateShaderResourceView(
    texture,
    &desc,
    getter_AddRefs(views[0]));
  if (FAILED(hr) || !views[0]) {
    gfxWarning() << "Could not bind an SRV for Y plane of NV12 texture: " << hexa(hr);
    return;
  }

  desc.Format = DXGI_FORMAT_R8G8_UNORM;
  hr = mDevice->CreateShaderResourceView(
    texture,
    &desc,
    getter_AddRefs(views[1]));
  if (FAILED(hr) || !views[0]) {
    gfxWarning() << "Could not bind an SRV for CbCr plane of NV12 texture: " << hexa(hr);
    return;
  }

  ID3D11ShaderResourceView* bind[2] = {
    views[0],
    views[1]
  };
  mCtx->PSSetShaderResources(aSlot, 2, bind);
}

bool
MLGDeviceD3D11::InitSyncObject()
{
  CD3D11_TEXTURE2D_DESC desc(
    DXGI_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, 1,
    D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  if (FAILED(hr) || !texture) {
    return Fail("FEATURE_FAILURE_SYNC_OBJECT",
                "Could not create a sync texture: %x", hr);
  }

  hr = texture->QueryInterface((IDXGIResource**)getter_AddRefs(mSyncTexture));
  if (FAILED(hr) || !texture) {
    return Fail("FEATURE_FAILURE_QI_SYNC_OBJECT",
                "Could not QI sync texture: %x", hr);
  }

  hr = mSyncTexture->GetSharedHandle(&mSyncHandle);
  if (FAILED(hr) || !mSyncHandle) {
    NS_DispatchToMainThread(NS_NewRunnableFunction("layers::MLGDeviceD3D11::InitSyncObject",
                                                   [] () -> void {
      Accumulate(Telemetry::D3D11_SYNC_HANDLE_FAILURE, 1);
    }));
    return Fail("FEATURE_FAILURE_GET_SHARED_HANDLE",
                "Could not get sync texture shared handle: %x", hr);
  }
  return true;
}

void
MLGDeviceD3D11::StartDiagnostics(uint32_t aInvalidPixels)
{
  mDiagnostics->Start(aInvalidPixels);
}

void
MLGDeviceD3D11::EndDiagnostics()
{
  mDiagnostics->End();
}

void
MLGDeviceD3D11::GetDiagnostics(GPUStats* aStats)
{
  mDiagnostics->Query(aStats);
}

bool
MLGDeviceD3D11::Synchronize()
{
  RefPtr<IDXGIKeyedMutex> mutex;
  mSyncTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));

  {
    HRESULT hr;
    AutoTextureLock lock(mutex, hr, 10000);
    if (hr == WAIT_TIMEOUT) {
      hr = mDevice->GetDeviceRemovedReason();
      if (hr == S_OK) {
        // There is no driver-removed event. Crash with this timeout.
        MOZ_CRASH("GFX: D3D11 normal status timeout");
      }

      // Since the timeout is related to the driver-removed, clear the
      // render-bounding size to skip this frame.
      HandleDeviceReset("SyncObject");
      return false;
    }
    if (hr == WAIT_ABANDONED) {
      gfxCriticalNote << "GFX: AL_D3D11 abandoned sync";
    }
  }

  return true;
}

void
MLGDeviceD3D11::UnlockAllTextures()
{
  for (auto iter = mLockedTextures.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<IDXGIKeyedMutex> mutex = iter.Get()->GetKey();
    mutex->ReleaseSync(0);
  }
  mLockedTextures.Clear();
  mLockAttemptedTextures.Clear();
}

void
MLGDeviceD3D11::SetDepthTestMode(MLGDepthTestMode aMode)
{
  mCtx->OMSetDepthStencilState(mDepthStencilStates[aMode], 0xffffffff);
}

void
MLGDeviceD3D11::InsertPresentWaitQuery()
{
  CD3D11_QUERY_DESC desc(D3D11_QUERY_EVENT);
  HRESULT hr = mDevice->CreateQuery(&desc, getter_AddRefs(mNextWaitForPresentQuery));
  if (FAILED(hr) || !mNextWaitForPresentQuery) {
    gfxWarning() << "Could not create D3D11_QUERY_EVENT: " << hexa(hr);
    return;
  }

  mCtx->End(mNextWaitForPresentQuery);
}

void
MLGDeviceD3D11::WaitForPreviousPresentQuery()
{
  if (mWaitForPresentQuery) {
    BOOL result;
    WaitForGPUQuery(mDevice, mCtx, mWaitForPresentQuery, &result);
  }
  mWaitForPresentQuery = mNextWaitForPresentQuery.forget();
}

void
MLGDeviceD3D11::Flush()
{
  mCtx->Flush();
}

void
MLGDeviceD3D11::EndFrame()
{
  // On our Windows 8 x64 machines, we have observed a driver bug related to
  // XXSetConstantBuffers1. It appears binding the same buffer to multiple
  // slots, and potentially leaving slots bound for many frames (as can
  // happen if we bind a high slot, like for blending), can consistently
  // cause shaders to read wrong values much later. It is possible there is
  // a driver bug related to aliasing and partial binding.
  //
  // Configuration: GeForce GT 610 (0x104a), Driver 9.18.13.3523, 3-4-2014,
  // on Windows 8 x64.
  //
  // To alleviate this we unbind all buffers at the end of the frame.
  static ID3D11Buffer* nullBuffers[6] = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  };
  MOZ_ASSERT(MOZ_ARRAY_LENGTH(nullBuffers) >= kMaxVertexShaderConstantBuffers);
  MOZ_ASSERT(MOZ_ARRAY_LENGTH(nullBuffers) >= kMaxPixelShaderConstantBuffers);

  mCtx->VSSetConstantBuffers(0, kMaxVertexShaderConstantBuffers, nullBuffers);
  mCtx->VSSetConstantBuffers(0, kMaxPixelShaderConstantBuffers, nullBuffers);

  MLGDevice::EndFrame();
}

void
MLGDeviceD3D11::HandleDeviceReset(const char* aWhere)
{
  if (!IsValid()) {
    return;
  }

  Fail(NS_LITERAL_CSTRING("FEATURE_FAILURE_DEVICE_RESET"), nullptr);

  gfxCriticalNote << "GFX: D3D11 detected a device reset in " << aWhere;
  if (XRE_IsGPUProcess()) {
    GPUParent::GetSingleton()->NotifyDeviceReset();
  }

  UnmapSharedBuffers();
  mIsValid = false;
}

RefPtr<MLGTexture>
MLGDeviceD3D11::CreateTexture(const gfx::IntSize& aSize,
                              gfx::SurfaceFormat aFormat,
                              MLGUsage aUsage,
                              MLGTextureFlags aFlags)
{
  return MLGTextureD3D11::Create(mDevice, aSize, aFormat, aUsage, aFlags);
}

RefPtr<MLGTexture>
MLGDeviceD3D11::CreateTexture(TextureSource* aSource)
{
  TextureSourceD3D11* source = aSource->AsSourceD3D11();
  if (!source) {
    gfxWarning() << "Attempted to wrap a non-D3D11 texture";
    return nullptr;
  }
  if (!source->GetD3D11Texture()) {
    return nullptr;
  }
  return new MLGTextureD3D11(source->GetD3D11Texture());
}

void
MLGDeviceD3D11::CopyTexture(MLGTexture* aDest,
                            const gfx::IntPoint& aTarget,
                            MLGTexture* aSource,
                            const gfx::IntRect& aRect)
{
  MLGTextureD3D11* dest = aDest->AsD3D11();
  MLGTextureD3D11* source = aSource->AsD3D11();

  D3D11_BOX box = RectToBox(aRect);
  mCtx->CopySubresourceRegion(
    dest->GetTexture(), 0,
    aTarget.x, aTarget.y, 0,
    source->GetTexture(), 0,
    &box);
}

static D3D11_BOX
RectToBox(const gfx::IntRect& aRect)
{
  D3D11_BOX box;
  box.front = 0;
  box.back = 1;
  box.left = aRect.x;
  box.top = aRect.y;
  box.right = aRect.XMost();
  box.bottom = aRect.YMost();
  return box;
}

} // namespace layers
} // namespace mozilla

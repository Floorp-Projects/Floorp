/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "LayerManagerD3D10.h"
#include "LayerManagerD3D10Effect.h"
#include "gfxWindowsPlatform.h"
#include "gfxD2DSurface.h"
#include "gfxFailure.h"
#include "cairo-win32.h"
#include "dxgi.h"

#include "ContainerLayerD3D10.h"
#include "ThebesLayerD3D10.h"
#include "ColorLayerD3D10.h"
#include "CanvasLayerD3D10.h"
#include "ReadbackLayerD3D10.h"
#include "ImageLayerD3D10.h"
#include "mozilla/layers/PLayerChild.h"

#include "../d3d9/Nv3DVUtils.h"

#include "gfxCrashReporterUtils.h"

using namespace std;
using namespace mozilla::gfx;

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

// {592BF306-0EED-4F76-9D03-A0846450F472}
static const GUID sDeviceAttachments = 
{ 0x592bf306, 0xeed, 0x4f76, { 0x9d, 0x3, 0xa0, 0x84, 0x64, 0x50, 0xf4, 0x72 } };
// {716AEDB1-C9C3-4B4D-8332-6F65D44AF6A8}
static const GUID sLayerManagerCount = 
{ 0x716aedb1, 0xc9c3, 0x4b4d, { 0x83, 0x32, 0x6f, 0x65, 0xd4, 0x4a, 0xf6, 0xa8 } };

cairo_user_data_key_t gKeyD3D10Texture;

LayerManagerD3D10::LayerManagerD3D10(nsIWidget *aWidget)
  : mWidget(aWidget)
{
}

struct DeviceAttachments
{
  nsRefPtr<ID3D10Effect> mEffect;
  nsRefPtr<ID3D10InputLayout> mInputLayout;
  nsRefPtr<ID3D10Buffer> mVertexBuffer;
  nsRefPtr<ReadbackManagerD3D10> mReadbackManager;
};

LayerManagerD3D10::~LayerManagerD3D10()
{
  if (mDevice) {
    int referenceCount = 0;
    UINT size = sizeof(referenceCount);
    HRESULT hr = mDevice->GetPrivateData(sLayerManagerCount, &size, &referenceCount);
    NS_ASSERTION(SUCCEEDED(hr), "Reference count not found on device.");
    referenceCount--;
    mDevice->SetPrivateData(sLayerManagerCount, sizeof(referenceCount), &referenceCount);

    if (!referenceCount) {
      DeviceAttachments *attachments;
      size = sizeof(attachments);
      mDevice->GetPrivateData(sDeviceAttachments, &size, &attachments);
      // No LayerManagers left for this device. Clear out interfaces stored which
      // hold a reference to the device.
      mDevice->SetPrivateData(sDeviceAttachments, 0, NULL);

      delete attachments;
    }
  }

  Destroy();
}

bool
LayerManagerD3D10::Initialize(bool force)
{
  ScopedGfxFeatureReporter reporter("D3D10 Layers", force);

  HRESULT hr;

  /* Create an Nv3DVUtils instance */
  if (!mNv3DVUtils) {
    mNv3DVUtils = new Nv3DVUtils();
    if (!mNv3DVUtils) {
      NS_WARNING("Could not create a new instance of Nv3DVUtils.\n");
    }
  }

  /* Initialize the Nv3DVUtils object */
  if (mNv3DVUtils) {
    mNv3DVUtils->Initialize();
  }

  mDevice = gfxWindowsPlatform::GetPlatform()->GetD3D10Device();
  if (!mDevice) {
      return false;
  }

  /*
   * Do some post device creation setup
   */
  if (mNv3DVUtils) {
    IUnknown* devUnknown = NULL;
    if (mDevice) {
      mDevice->QueryInterface(IID_IUnknown, (void **)&devUnknown);
    }
    mNv3DVUtils->SetDeviceInfo(devUnknown);
  }

  int referenceCount = 0;
  UINT size = sizeof(referenceCount);
  // If this isn't there yet it'll fail, count will remain 0, which is correct.
  mDevice->GetPrivateData(sLayerManagerCount, &size, &referenceCount);
  referenceCount++;
  mDevice->SetPrivateData(sLayerManagerCount, sizeof(referenceCount), &referenceCount);

  DeviceAttachments *attachments;
  size = sizeof(DeviceAttachments*);
  if (FAILED(mDevice->GetPrivateData(sDeviceAttachments, &size, &attachments))) {
    attachments = new DeviceAttachments;
    mDevice->SetPrivateData(sDeviceAttachments, sizeof(attachments), &attachments);

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

    attachments->mEffect = mEffect;
  
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

    attachments->mInputLayout = mInputLayout;
  
    Vertex vertices[] = { {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0} };
    CD3D10_BUFFER_DESC bufferDesc(sizeof(vertices), D3D10_BIND_VERTEX_BUFFER);
    D3D10_SUBRESOURCE_DATA data;
    data.pSysMem = (void*)vertices;

    hr = mDevice->CreateBuffer(&bufferDesc, &data, getter_AddRefs(mVertexBuffer));

    if (FAILED(hr)) {
      return false;
    }

    attachments->mVertexBuffer = mVertexBuffer;
  } else {
    mEffect = attachments->mEffect;
    mVertexBuffer = attachments->mVertexBuffer;
    mInputLayout = attachments->mInputLayout;
  }

  if (HasShadowManager()) {
    reporter.SetSuccessful();
    return true;
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
  // We don't really need this flag, however it seems on some NVidia hardware
  // smaller area windows do not present properly without this flag. This flag
  // should have no negative consequences by itself. See bug 613790. This flag
  // is broken on optimus devices. As a temporary solution we don't set it
  // there, the only way of reliably detecting we're on optimus is looking for
  // the DLL. See Bug 623807.
  if (gfxWindowsPlatform::IsOptimus()) {
    swapDesc.Flags = 0;
  } else {
    swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;
  }
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

  // We need this because we don't want DXGI to respond to Alt+Enter.
  dxgiFactory->MakeWindowAssociation(swapDesc.OutputWindow, DXGI_MWA_NO_WINDOW_CHANGES);

  reporter.SetSuccessful();
  return true;
}

void
LayerManagerD3D10::Destroy()
{
  if (!IsDestroyed()) {
    if (mRoot) {
      static_cast<LayerD3D10*>(mRoot->ImplData())->LayerManagerDestroyed();
    }
    mRootForShadowTree = nsnull;
    // XXX need to be careful here about surface destruction
    // racing with share-to-chrome message
  }
  LayerManager::Destroy();
}

void
LayerManagerD3D10::SetRoot(Layer *aRoot)
{
  mRoot = aRoot;
}

void
LayerManagerD3D10::BeginTransaction()
{
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif
}

void
LayerManagerD3D10::BeginTransactionWithTarget(gfxContext* aTarget)
{
  mTarget = aTarget;
}

bool
LayerManagerD3D10::EndEmptyTransaction()
{
  if (!mRoot)
    return false;

  EndTransaction(nsnull, nsnull);
  return true;
}

void
LayerManagerD3D10::EndTransaction(DrawThebesLayerCallback aCallback,
                                  void* aCallbackData,
                                  EndTransactionFlags aFlags)
{
  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    mCurrentCallbackInfo.Callback = aCallback;
    mCurrentCallbackInfo.CallbackData = aCallbackData;

    // The results of our drawing always go directly into a pixel buffer,
    // so we don't need to pass any global transform here.
    mRoot->ComputeEffectiveTransforms(gfx3DMatrix());

#ifdef MOZ_LAYERS_HAVE_LOG
    MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
    Log();
#endif

    Render();
    mCurrentCallbackInfo.Callback = nsnull;
    mCurrentCallbackInfo.CallbackData = nsnull;
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif

  mTarget = nsnull;
}

already_AddRefed<ThebesLayer>
LayerManagerD3D10::CreateThebesLayer()
{
  nsRefPtr<ThebesLayer> layer = new ThebesLayerD3D10(this);
  return layer.forget();
}
 
already_AddRefed<ShadowThebesLayer>
LayerManagerD3D10::CreateShadowThebesLayer()
{
  nsRefPtr<ShadowThebesLayerD3D10> layer = new ShadowThebesLayerD3D10(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
LayerManagerD3D10::CreateContainerLayer()
{
  nsRefPtr<ContainerLayer> layer = new ContainerLayerD3D10(this);
  return layer.forget();
}

already_AddRefed<ShadowContainerLayer>
LayerManagerD3D10::CreateShadowContainerLayer()
{
  nsRefPtr<ShadowContainerLayer> layer = new ShadowContainerLayerD3D10(this);
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

already_AddRefed<ReadbackLayer>
LayerManagerD3D10::CreateReadbackLayer()
{
  nsRefPtr<ReadbackLayer> layer = new ReadbackLayerD3D10(this);
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


already_AddRefed<gfxASurface>
LayerManagerD3D10::CreateOptimalMaskSurface(const gfxIntSize &aSize)
{
  return CreateOptimalSurface(aSize, gfxASurface::ImageFormatARGB32);
}


TemporaryRef<DrawTarget>
LayerManagerD3D10::CreateDrawTarget(const IntSize &aSize,
                                    SurfaceFormat aFormat)
{
  if ((aFormat != FORMAT_B8G8R8A8 &&
       aFormat != FORMAT_B8G8R8X8)) {
    return LayerManager::CreateDrawTarget(aSize, aFormat);
  }

  nsRefPtr<ID3D10Texture2D> texture;
  
  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aSize.width, aSize.height, 1, 1);
  desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
  
  HRESULT hr = device()->CreateTexture2D(&desc, NULL, getter_AddRefs(texture));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create new texture for CreateOptimalSurface!");
    return LayerManager::CreateDrawTarget(aSize, aFormat);
  }

  RefPtr<DrawTarget> surface =
    Factory::CreateDrawTargetForD3D10Texture(texture, aFormat);

  if (!surface) {
    return LayerManager::CreateDrawTarget(aSize, aFormat);
  }
  
  return surface;
}

ReadbackManagerD3D10*
LayerManagerD3D10::readbackManager()
{
  EnsureReadbackManager();
  return mReadbackManager;
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
  projection._33 = 0.0f;
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
LayerManagerD3D10::SetupInputAssembler()
{
  mDevice->IASetInputLayout(mInputLayout);

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  ID3D10Buffer *buffer = mVertexBuffer;
  mDevice->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
  mDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
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

  SetupInputAssembler();

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
  
  if (mSwapChain) {
    hr = mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)backBuf.StartAssignment());
    if (FAILED(hr)) {
      return;
    }
  } else {
    backBuf = mBackBuffer;
  }
  
  mDevice->CreateRenderTargetView(backBuf, NULL, getter_AddRefs(mRTView));
}

void
LayerManagerD3D10::VerifyBufferSize()
{
  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  if (mSwapChain) {
    DXGI_SWAP_CHAIN_DESC swapDesc;
    mSwapChain->GetDesc(&swapDesc);

    if (swapDesc.BufferDesc.Width == rect.width &&
        swapDesc.BufferDesc.Height == rect.height) {
      return;
    }

    mRTView = nsnull;
    if (gfxWindowsPlatform::IsOptimus()) {
      mSwapChain->ResizeBuffers(1, rect.width, rect.height,
                                DXGI_FORMAT_B8G8R8A8_UNORM,
                                0);
    } else {
      mSwapChain->ResizeBuffers(1, rect.width, rect.height,
                                DXGI_FORMAT_B8G8R8A8_UNORM,
                                DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE);
    }
  } else {
    D3D10_TEXTURE2D_DESC oldDesc;    
    if (mBackBuffer) {
        mBackBuffer->GetDesc(&oldDesc);
    } else {
        oldDesc.Width = oldDesc.Height = 0;
    }
    if (oldDesc.Width == rect.width &&
        oldDesc.Height == rect.height) {
      return;
    }

    CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                               rect.width, rect.height, 1, 1);
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D10_RESOURCE_MISC_SHARED
                     // FIXME/bug 662109: synchronize using KeyedMutex
                     /*D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX*/;
    HRESULT hr = device()->CreateTexture2D(&desc, nsnull, getter_AddRefs(mBackBuffer));
    if (FAILED(hr)) {
      ReportFailure(NS_LITERAL_CSTRING("LayerManagerD3D10::VerifyBufferSize(): Failed to create shared texture"),
                    hr);
      NS_RUNTIMEABORT("Failed to create back buffer");
    }

    // XXX resize texture?
    mRTView = nsnull;
  }
}

void
LayerManagerD3D10::EnsureReadbackManager()
{
  if (mReadbackManager) {
    return;
  }

  DeviceAttachments *attachments;
  UINT size = sizeof(DeviceAttachments*);
  if (FAILED(mDevice->GetPrivateData(sDeviceAttachments, &size, &attachments))) {
    // Strange! This shouldn't happen ... return a readback manager for this
    // layer manager only.
    mReadbackManager = new ReadbackManagerD3D10();
    gfx::LogFailure(NS_LITERAL_CSTRING("Couldn't get device attachments for device."));
    return;
  }

  if (attachments->mReadbackManager) {
    mReadbackManager = attachments->mReadbackManager;
    return;
  }

  mReadbackManager = new ReadbackManagerD3D10();
  attachments->mReadbackManager = mReadbackManager;
}

void
LayerManagerD3D10::Render()
{
  static_cast<LayerD3D10*>(mRoot->ImplData())->Validate();

  SetupPipeline();

  float black[] = { 0, 0, 0, 0 };
  device()->ClearRenderTargetView(mRTView, black);

  nsIntRect rect;
  mWidget->GetClientBounds(rect);

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

  static_cast<LayerD3D10*>(mRoot->ImplData())->RenderLayer();

  if (mTarget) {
    PaintToTarget();
  } else if (mBackBuffer) {
    ShadowLayerForwarder::BeginTransaction();
    
    nsIntRect contentRect = nsIntRect(0, 0, rect.width, rect.height);
    if (!mRootForShadowTree) {
        mRootForShadowTree = new DummyRoot(this);
        mRootForShadowTree->SetShadow(ConstructShadowFor(mRootForShadowTree));
        CreatedContainerLayer(mRootForShadowTree);
        ShadowLayerForwarder::SetRoot(mRootForShadowTree);
    }

    nsRefPtr<WindowLayer> windowLayer =
        static_cast<WindowLayer*>(mRootForShadowTree->GetFirstChild());
    if (!windowLayer) {
        windowLayer = new WindowLayer(this);
        windowLayer->SetShadow(ConstructShadowFor(windowLayer));
        CreatedThebesLayer(windowLayer);
        mRootForShadowTree->InsertAfter(windowLayer, nsnull);
        ShadowLayerForwarder::InsertAfter(mRootForShadowTree, windowLayer);
    }

    if (!mRootForShadowTree->GetVisibleRegion().IsEqual(contentRect)) {
        mRootForShadowTree->SetVisibleRegion(contentRect);
        windowLayer->SetVisibleRegion(contentRect);

        ShadowLayerForwarder::Mutated(mRootForShadowTree);
        ShadowLayerForwarder::Mutated(windowLayer);
    }

    FrameMetrics m;
    if (ContainerLayer* cl = mRoot->AsContainerLayer()) {
        m = cl->GetFrameMetrics();
    } else {
        m.mScrollId = FrameMetrics::ROOT_SCROLL_ID;
    }
    if (m != mRootForShadowTree->GetFrameMetrics()) {
        mRootForShadowTree->SetFrameMetrics(m);
        ShadowLayerForwarder::Mutated(mRootForShadowTree);
    }

    SurfaceDescriptorD3D10 sd;
    GetDescriptor(mBackBuffer, &sd);
    ShadowLayerForwarder::PaintedThebesBuffer(windowLayer,
                                              contentRect,
                                              contentRect, nsIntPoint(),
                                              sd);

    // A source in the graphics pipeline can't also be a target.  So
    // unbind here to avoid racing with the chrome process sourcing
    // the back texture.
    mDevice->OMSetRenderTargets(0, NULL, NULL);

    // XXX revisit this Flush() in bug 662109.  It's not clear it's
    // needed.
    mDevice->Flush();

    mRTView = NULL;

    AutoInfallibleTArray<EditReply, 10> replies;
    ShadowLayerForwarder::EndTransaction(&replies);
    // We expect only 1 reply, but might get none if the parent
    // process crashed

    swap(mBackBuffer, mRemoteFrontBuffer);
  } else {
    mSwapChain->Present(0, 0);
  }
  LayerManager::PostPresent();
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

  HRESULT hr = device()->CreateTexture2D(&softDesc, NULL, getter_AddRefs(readTexture));
  if (FAILED(hr)) {
    ReportFailure(NS_LITERAL_CSTRING("LayerManagerD3D10::PaintToTarget(): Failed to create texture"),
                  hr);
    return;
  }

  device()->CopyResource(readTexture, backBuf);

  D3D10_MAPPED_TEXTURE2D map;
  readTexture->Map(0, D3D10_MAP_READ, 0, &map);

  nsRefPtr<gfxImageSurface> tmpSurface =
    new gfxImageSurface((unsigned char*)map.pData,
                        gfxIntSize(bbDesc.Width, bbDesc.Height),
                        map.RowPitch,
                        gfxASurface::ImageFormatARGB32);

  mTarget->SetSource(tmpSurface);
  mTarget->SetOperator(gfxContext::OPERATOR_OVER);
  mTarget->Paint();

  readTexture->Unmap(0);
}

void
LayerManagerD3D10::ReportFailure(const nsACString &aMsg, HRESULT aCode)
{
  // We could choose to abort here when hr == E_OUTOFMEMORY.
  nsCString msg;
  msg.Append(aMsg);
  msg.AppendLiteral(" Error code: ");
  msg.AppendInt(PRUint32(aCode));
  NS_WARNING(msg.BeginReading());

  gfx::LogFailure(msg);
}

LayerD3D10::LayerD3D10(LayerManagerD3D10 *aManager)
  : mD3DManager(aManager)
{
}

ID3D10EffectTechnique*
LayerD3D10::SelectShader(PRUint8 aFlags)
{
  switch (aFlags) {
  case (SHADER_RGBA | SHADER_NON_PREMUL | SHADER_LINEAR | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderRGBALayerNonPremulMask");
  case (SHADER_RGBA | SHADER_NON_PREMUL | SHADER_LINEAR | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderRGBALayerNonPremul");
  case (SHADER_RGBA | SHADER_NON_PREMUL | SHADER_POINT | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderRGBALayerNonPremulPoint");
  case (SHADER_RGBA | SHADER_NON_PREMUL | SHADER_POINT | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderRGBALayerNonPremulPointMask");
  case (SHADER_RGBA | SHADER_PREMUL | SHADER_LINEAR | SHADER_MASK_3D):
    return effect()->GetTechniqueByName("RenderRGBALayerPremulMask3D");
  case (SHADER_RGBA | SHADER_PREMUL | SHADER_LINEAR | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderRGBALayerPremulMask");
  case (SHADER_RGBA | SHADER_PREMUL | SHADER_LINEAR | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderRGBALayerPremul");
  case (SHADER_RGBA | SHADER_PREMUL | SHADER_POINT | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderRGBALayerPremulPointMask");
  case (SHADER_RGBA | SHADER_PREMUL | SHADER_POINT | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderRGBALayerPremulPoint");
  case (SHADER_RGB | SHADER_PREMUL | SHADER_POINT | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderRGBLayerPremulPointMask");
  case (SHADER_RGB | SHADER_PREMUL | SHADER_POINT | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderRGBLayerPremulPoint");
  case (SHADER_RGB | SHADER_PREMUL | SHADER_LINEAR | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderRGBLayerPremulMask");
  case (SHADER_RGB | SHADER_PREMUL | SHADER_LINEAR | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderRGBLayerPremul");
  case (SHADER_SOLID | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderSolidColorLayerMask");
  case (SHADER_SOLID | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderSolidColorLayer");
  case (SHADER_COMPONENT_ALPHA | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderComponentAlphaLayerMask");
  case (SHADER_COMPONENT_ALPHA | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderComponentAlphaLayer");
  case (SHADER_YCBCR | SHADER_MASK):
    return effect()->GetTechniqueByName("RenderYCbCrLayerMask");
  case (SHADER_YCBCR | SHADER_NO_MASK):
    return effect()->GetTechniqueByName("RenderYCbCrLayer");
  default:
    NS_ERROR("Invalid shader.");
    return nsnull;
  }
}

PRUint8
LayerD3D10::LoadMaskTexture()
{
  if (Layer* maskLayer = GetLayer()->GetMaskLayer()) {
    gfxIntSize size;
    nsRefPtr<ID3D10ShaderResourceView> maskSRV =
      static_cast<LayerD3D10*>(maskLayer->ImplData())->GetAsTexture(&size);
  
    if (!maskSRV) {
      return SHADER_NO_MASK;
    }

    gfxMatrix maskTransform;
    bool maskIs2D = maskLayer->GetEffectiveTransform().CanDraw2D(&maskTransform);
    NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
    gfxRect bounds = gfxRect(gfxPoint(), size);
    bounds = maskTransform.TransformBounds(bounds);

    effect()->GetVariableByName("vMaskQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)bounds.x,
        (float)bounds.y,
        (float)bounds.width,
        (float)bounds.height)
      );

    effect()->GetVariableByName("tMask")->AsShaderResource()->SetResource(maskSRV);
    return SHADER_MASK;
  }

  return SHADER_NO_MASK; 
}

WindowLayer::WindowLayer(LayerManagerD3D10* aManager)
  : ThebesLayer(aManager, nsnull)
{
 }

WindowLayer::~WindowLayer()
{
  PLayerChild::Send__delete__(GetShadow());
}

DummyRoot::DummyRoot(LayerManagerD3D10* aManager)
  : ContainerLayer(aManager, nsnull)
{
}

DummyRoot::~DummyRoot()
{
  RemoveChild(nsnull);
  PLayerChild::Send__delete__(GetShadow());
}

void
DummyRoot::InsertAfter(Layer* aLayer, Layer* aNull)
{
  NS_ABORT_IF_FALSE(!mFirstChild && !aNull,
                    "Expect to append one child, once");
  mFirstChild = nsRefPtr<Layer>(aLayer).forget().get();
}

void
DummyRoot::RemoveChild(Layer* aNull)
{
  NS_ABORT_IF_FALSE(!aNull, "Unused argument should be null");
  NS_IF_RELEASE(mFirstChild);
}


}
}

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DEVICEMANAGERD3D9_H
#define GFX_DEVICEMANAGERD3D9_H

#include "gfxTypes.h"
#include "nsAutoPtr.h"
#include "d3d9.h"
#include "nsTArray.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/RefPtr.h"

struct nsIntRect;

namespace mozilla {
namespace layers {

class DeviceManagerD3D9;
class LayerD3D9;
class Nv3DVUtils;
class Layer;
class TextureSourceD3D9;

// Shader Constant locations
const int CBmLayerTransform = 0;
const int CBmProjection = 4;
const int CBvRenderTargetOffset = 8;
const int CBvTextureCoords = 9;
const int CBvLayerQuad = 10;
// we don't use opacity with solid color shaders
const int CBfLayerOpacity = 0;
const int CBvColor = 0;

enum DeviceManagerState {
  // The device and swap chain are OK.
  DeviceOK,
  // The device or swap chain are in a bad state, and we should not render.
  DeviceFail,
  // The device is lost and cannot be reset, the user should forget the
  // current device manager and create a new one.
  DeviceMustRecreate,
};


/**
 * This structure is used to pass rectangles to our shader constant. We can use
 * this for passing rectangular areas to SetVertexShaderConstant. In the format
 * of a 4 component float(x,y,width,height). Our vertex shader can then use
 * this to construct rectangular positions from the 0,0-1,1 quad that we source
 * it with.
 */
struct ShaderConstantRect
{
  float mX, mY, mWidth, mHeight;

  // Provide all the commonly used argument types to prevent all the local
  // casts in the code.
  ShaderConstantRect(float aX, float aY, float aWidth, float aHeight)
    : mX(aX), mY(aY), mWidth(aWidth), mHeight(aHeight)
  { }

  ShaderConstantRect(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight)
    : mX((float)aX), mY((float)aY)
    , mWidth((float)aWidth), mHeight((float)aHeight)
  { }

  ShaderConstantRect(int32_t aX, int32_t aY, float aWidth, float aHeight)
    : mX((float)aX), mY((float)aY), mWidth(aWidth), mHeight(aHeight)
  { }

  // For easy passing to SetVertexShaderConstantF.
  operator float* () { return &mX; }
};

/**
 * SwapChain class, this class manages the swap chain belonging to a
 * LayerManagerD3D9.
 */
class SwapChainD3D9
{
  NS_INLINE_DECL_REFCOUNTING(SwapChainD3D9)
public:
  ~SwapChainD3D9();

  /**
   * This function will prepare the device this swap chain belongs to for
   * rendering to this swap chain. Only after calling this function can the
   * swap chain be drawn to, and only until this function is called on another
   * swap chain belonging to this device will the device draw to it. Passed in
   * is the size of the swap chain. If the window size differs from the size
   * during the last call to this function the swap chain will resize. Note that
   * in no case does this function guarantee the backbuffer to still have its
   * old content.
   */
  DeviceManagerState PrepareForRendering();

  already_AddRefed<IDirect3DSurface9> GetBackBuffer();

  /**
   * This function will present the selected rectangle of the swap chain to
   * its associated window.
   */
  void Present(const nsIntRect &aRect);
  void Present();

private:
  friend class DeviceManagerD3D9;

  SwapChainD3D9(DeviceManagerD3D9 *aDeviceManager);
  
  bool Init(HWND hWnd);

  /**
   * This causes us to release our swap chain, clearing out our resource usage
   * so the master device may reset.
   */
  void Reset();

  nsRefPtr<IDirect3DSwapChain9> mSwapChain;
  nsRefPtr<DeviceManagerD3D9> mDeviceManager;
  HWND mWnd;
};

/**
 * Device manager, this class is used by the layer managers to share the D3D9
 * device and create swap chains for the individual windows the layer managers
 * belong to.
 */
class DeviceManagerD3D9 MOZ_FINAL
{
public:
  DeviceManagerD3D9();
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DeviceManagerD3D9)

  /**
   * Initialises the device manager, the underlying device, and everything else
   * the manager needs.
   * Returns true if initialisation succeeds, false otherwise.
   * Note that if initisalisation fails, you cannot try again - you must throw
   * away the DeviceManagerD3D9 and create a new one.
   */
  bool Init();

  /**
   * Sets up the render state for the device for layer rendering.
   */
  void SetupRenderState();

  /**
   * Create a swap chain setup to work with the specified window.
   */
  already_AddRefed<SwapChainD3D9> CreateSwapChain(HWND hWnd);

  IDirect3DDevice9 *device() { return mDevice; }

  bool IsD3D9Ex() { return mDeviceEx; }

  bool HasDynamicTextures() { return mHasDynamicTextures; }

  enum ShaderMode {
    RGBLAYER,
    RGBALAYER,
    COMPONENTLAYERPASS1,
    COMPONENTLAYERPASS2,
    YCBCRLAYER,
    SOLIDCOLORLAYER
  };

  void SetShaderMode(ShaderMode aMode, Layer* aMask, bool aIs2D);
  // returns the register to be used for the mask texture, if appropriate
  uint32_t SetShaderMode(ShaderMode aMode, MaskType aMaskType);

  /** 
   * Return pointer to the Nv3DVUtils instance 
   */ 
  Nv3DVUtils *GetNv3DVUtils()  { return mNv3DVUtils; }

  /**
   * Returns true if this device was removed.
   */
  bool DeviceWasRemoved() { return mDeviceWasRemoved; }

  uint32_t GetDeviceResetCount() { return mDeviceResetCount; }

  /**
   * We keep a list of all layers here that may have hardware resource allocated
   * so we can clean their resources on reset.
   */
  nsTArray<LayerD3D9*> mLayersWithResources;

  int32_t GetMaxTextureSize() { return mMaxTextureSize; }

  // Removes aHost from our list of texture hosts if it is the head.
  void RemoveTextureListHead(TextureSourceD3D9* aHost);

  /**
   * Creates a texture using our device.
   * If needed, we keep a record of the new texture, so the texture can be
   * released. In this case, aTextureHostIDirect3DTexture9 must be non-null.
   */
  TemporaryRef<IDirect3DTexture9> CreateTexture(const gfx::IntSize &aSize,
                                                _D3DFORMAT aFormat,
                                                D3DPOOL aPool,
                                                TextureSourceD3D9* aTextureHostIDirect3DTexture9);
#ifdef DEBUG
  // Looks for aFind in the list of texture hosts.
  // O(n) so only use for assertions.
  bool IsInTextureHostList(TextureSourceD3D9* aFind);
#endif

  /**
   * This function verifies the device is ready for rendering, internally this
   * will test the cooperative level of the device and reset the device if
   * needed. If this returns false subsequent rendering calls may return errors.
   */
  DeviceManagerState VerifyReadyForRendering();

  static uint32_t sMaskQuadRegister;

private:
  friend class SwapChainD3D9;

  ~DeviceManagerD3D9();
  void DestroyDevice();

  /**
   * This will fill our vertex buffer with the data of our quad, it may be
   * called when the vertex buffer is recreated.
   */
  bool CreateVertexBuffer();

  /**
   * Release all textures created by this device manager.
   */
  void ReleaseTextureResources();
  /**
   * Add aHost to our list of texture hosts.
   */
  void RegisterTextureHost(TextureSourceD3D9* aHost);

  /* Array used to store all swap chains for device resets */
  nsTArray<SwapChainD3D9*> mSwapChains;

  /* The D3D device we use */
  nsRefPtr<IDirect3DDevice9> mDevice;

  /* The D3D9Ex device - only valid on Vista+ with WDDM */
  nsRefPtr<IDirect3DDevice9Ex> mDeviceEx;

  /* An instance of the D3D9 object */
  nsRefPtr<IDirect3D9> mD3D9;

  /* An instance of the D3D9Ex object - only valid on Vista+ with WDDM */
  nsRefPtr<IDirect3D9Ex> mD3D9Ex;

  /* Vertex shader used for layer quads */
  nsRefPtr<IDirect3DVertexShader9> mLayerVS;

  /* Pixel shader used for RGB textures */
  nsRefPtr<IDirect3DPixelShader9> mRGBPS;

  /* Pixel shader used for RGBA textures */
  nsRefPtr<IDirect3DPixelShader9> mRGBAPS;

  /* Pixel shader used for component alpha textures (pass 1) */
  nsRefPtr<IDirect3DPixelShader9> mComponentPass1PS;

  /* Pixel shader used for component alpha textures (pass 2) */
  nsRefPtr<IDirect3DPixelShader9> mComponentPass2PS;

  /* Pixel shader used for RGB textures */
  nsRefPtr<IDirect3DPixelShader9> mYCbCrPS;

  /* Pixel shader used for solid colors */
  nsRefPtr<IDirect3DPixelShader9> mSolidColorPS;

  /* As above, but using a mask layer */
  nsRefPtr<IDirect3DVertexShader9> mLayerVSMask;
  nsRefPtr<IDirect3DVertexShader9> mLayerVSMask3D;
  nsRefPtr<IDirect3DPixelShader9> mRGBPSMask;
  nsRefPtr<IDirect3DPixelShader9> mRGBAPSMask;
  nsRefPtr<IDirect3DPixelShader9> mRGBAPSMask3D;
  nsRefPtr<IDirect3DPixelShader9> mComponentPass1PSMask;
  nsRefPtr<IDirect3DPixelShader9> mComponentPass2PSMask;
  nsRefPtr<IDirect3DPixelShader9> mYCbCrPSMask;
  nsRefPtr<IDirect3DPixelShader9> mSolidColorPSMask;

  /* Vertex buffer containing our basic vertex structure */
  nsRefPtr<IDirect3DVertexBuffer9> mVB;

  /* Our vertex declaration */
  nsRefPtr<IDirect3DVertexDeclaration9> mVD;

  /* We maintain a doubly linked list of all d3d9 texture hosts which host
   * d3d9 textures created by this device manager.
   * Texture hosts must remove themselves when they disappear (i.e., we
   * expect all hosts in the list to be valid).
   * The list is cleared when we release the textures.
   */
  TextureSourceD3D9* mTextureHostList;

  /* Our focus window - this is really a dummy window we can associate our
   * device with.
   */
  HWND mFocusWnd;

  /* we use this to help track if our device temporarily or permanently lost */
  HMONITOR mDeviceMonitor;

  uint32_t mDeviceResetCount;

  uint32_t mMaxTextureSize;

  /**
   * Wrap (repeat) or clamp textures. We prefer the former so we can do buffer
   * rotation, but some older hardware doesn't support it.
   */
  D3DTEXTUREADDRESS mTextureAddressingMode;

  /* If this device supports dynamic textures */
  bool mHasDynamicTextures;

  /* If this device was removed */
  bool mDeviceWasRemoved;

  /* Nv3DVUtils instance */ 
  nsAutoPtr<Nv3DVUtils> mNv3DVUtils; 

  /**
   * Verifies all required device capabilities are present.
   */
  bool VerifyCaps();
};

} /* namespace layers */
} /* namespace mozilla */

#endif /* GFX_DEVICEMANAGERD3D9_H */

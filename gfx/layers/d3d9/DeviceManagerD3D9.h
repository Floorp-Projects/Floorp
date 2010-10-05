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

#ifndef GFX_DEVICEMANAGERD3D9_H
#define GFX_DEVICEMANAGERD3D9_H

#include "gfxTypes.h"
#include "nsRect.h"
#include "nsAutoPtr.h"
#include "d3d9.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

class DeviceManagerD3D9;
class LayerD3D9;
class Nv3DVUtils;

// Shader Constant locations
const int CBmLayerTransform = 0;
const int CBmProjection = 4;
const int CBvRenderTargetOffset = 8;
const int CBvTextureCoords = 9;
const int CBvLayerQuad = 10;
const int CBfLayerOpacity = 0;

/**
 * SwapChain class, this class manages the swap chain belonging to a
 * LayerManagerD3D9.
 */
class THEBES_API SwapChainD3D9
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
  bool PrepareForRendering();

  /**
   * This function will present the selected rectangle of the swap chain to
   * its associated window.
   */
  void Present(const nsIntRect &aRect);

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
class THEBES_API DeviceManagerD3D9
{
public:
  DeviceManagerD3D9();
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
protected:
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

public:
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
    YCBCRLAYER,
    SOLIDCOLORLAYER
  };

  void SetShaderMode(ShaderMode aMode);

  /** 
   * Return pointer to the Nv3DVUtils instance 
   */ 
  Nv3DVUtils *GetNv3DVUtils()  { return mNv3DVUtils; }

  /**
   * Returns true if this device was removed.
   */
  bool DeviceWasRemoved() { return mDeviceWasRemoved; }

  /**
   * We keep a list of all layers here that may have hardware resource allocated
   * so we can clean their resources on reset.
   */
  nsTArray<LayerD3D9*> mLayersWithResources;
private:
  friend class SwapChainD3D9;

  ~DeviceManagerD3D9();

  /**
   * This function verifies the device is ready for rendering, internally this
   * will test the cooperative level of the device and reset the device if
   * needed. If this returns false subsequent rendering calls may return errors.
   */
  bool VerifyReadyForRendering();

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

  /* Pixel shader used for RGB textures */
  nsRefPtr<IDirect3DPixelShader9> mYCbCrPS;

  /* Pixel shader used for solid colors */
  nsRefPtr<IDirect3DPixelShader9> mSolidColorPS;

  /* Vertex buffer containing our basic vertex structure */
  nsRefPtr<IDirect3DVertexBuffer9> mVB;

  /* Our vertex declaration */
  nsRefPtr<IDirect3DVertexDeclaration9> mVD;

  /* Our focus window - this is really a dummy window we can associate our
   * device with.
   */
  HWND mFocusWnd;

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

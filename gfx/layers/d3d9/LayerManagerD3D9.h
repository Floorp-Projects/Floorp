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

#ifndef GFX_LAYERMANAGERD3D9_H
#define GFX_LAYERMANAGERD3D9_H

#include "Layers.h"

#include "mozilla/layers/ShadowLayers.h"

#include <windows.h>
#include <d3d9.h>

#include "gfxContext.h"
#include "nsIWidget.h"

#include "DeviceManagerD3D9.h"

namespace mozilla {
namespace layers {

class LayerD3D9;
class ThebesLayerD3D9;

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

  ShaderConstantRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
    : mX((float)aX), mY((float)aY)
    , mWidth((float)aWidth), mHeight((float)aHeight)
  { }

  ShaderConstantRect(PRInt32 aX, PRInt32 aY, float aWidth, float aHeight)
    : mX((float)aX), mY((float)aY), mWidth(aWidth), mHeight(aHeight)
  { }

  // For easy passing to SetVertexShaderConstantF.
  operator float* () { return &mX; }
};

/*
 * This is the LayerManager used for Direct3D 9. For now this will render on
 * the main thread.
 */
class THEBES_API LayerManagerD3D9 : public ShadowLayerManager {
public:
  LayerManagerD3D9(nsIWidget *aWidget);
  virtual ~LayerManagerD3D9();

  /*
   * Initializes the layer manager, this is when the layer manager will
   * actually access the device and attempt to create the swap chain used
   * to draw to the window. If this method fails the device cannot be used.
   * This function is not threadsafe.
   *
   * \return True is initialization was succesful, false when it was not.
   */
  PRBool Initialize();

  /*
   * Sets the clipping region for this layer manager. This is important on
   * windows because using OGL we no longer have GDI's native clipping. Therefor
   * widget must tell us what part of the screen is being invalidated,
   * and we should clip to this.
   *
   * \param aClippingRegion Region to clip to. Setting an empty region
   * will disable clipping.
   */
  void SetClippingRegion(const nsIntRegion& aClippingRegion);

  /*
   * LayerManager implementation.
   */
  virtual void Destroy();

  virtual ShadowLayerManager* AsShadowManager()
  { return this; }

  virtual void BeginTransaction();

  virtual void BeginTransactionWithTarget(gfxContext* aTarget);

  void EndConstruction();

  virtual bool EndEmptyTransaction();

  struct CallbackInfo {
    DrawThebesLayerCallback Callback;
    void *CallbackData;
  };

  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData);

  const CallbackInfo &GetCallbackInfo() { return mCurrentCallbackInfo; }

  void SetRoot(Layer* aLayer);

  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize)
  {
    if (!mDeviceManager)
      return false;
    PRInt32 maxSize = mDeviceManager->GetMaxTextureSize();
    return aSize <= gfxIntSize(maxSize, maxSize);
  }

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();

  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();

  virtual already_AddRefed<ImageLayer> CreateImageLayer();

  virtual already_AddRefed<ColorLayer> CreateColorLayer();

  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();

  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer();

  virtual already_AddRefed<ImageContainer> CreateImageContainer();

  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer();
  virtual already_AddRefed<ShadowContainerLayer> CreateShadowContainerLayer();
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer();
  virtual already_AddRefed<ShadowColorLayer> CreateShadowColorLayer();
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer();

  virtual LayersBackend GetBackendType() { return LAYERS_D3D9; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("Direct3D 9"); }
  bool DeviceWasRemoved() { return deviceManager()->DeviceWasRemoved(); }

  /*
   * Helper methods.
   */
  void SetClippingEnabled(PRBool aEnabled);

  void SetShaderMode(DeviceManagerD3D9::ShaderMode aMode)
    { mDeviceManager->SetShaderMode(aMode); }

  IDirect3DDevice9 *device() const { return mDeviceManager->device(); }
  DeviceManagerD3D9 *deviceManager() const { return mDeviceManager; }

  /** 
   * Return pointer to the Nv3DVUtils instance. Re-direct to mDeviceManager.
   */ 
  Nv3DVUtils *GetNv3DVUtils()  { return mDeviceManager ? mDeviceManager->GetNv3DVUtils() : NULL; } 

  static void OnDeviceManagerDestroy(DeviceManagerD3D9 *aDeviceManager) {
    if(aDeviceManager == mDefaultDeviceManager)
      mDefaultDeviceManager = nsnull;
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "D3D9"; }
#endif // MOZ_LAYERS_HAVE_LOG

  void ReportFailure(const nsACString &aMsg, HRESULT aCode);

private:
  /* Default device manager instance */
  static DeviceManagerD3D9 *mDefaultDeviceManager;

  /* Device manager instance for this layer manager */
  nsRefPtr<DeviceManagerD3D9> mDeviceManager;

  /* Swap chain associated with this layer manager */
  nsRefPtr<SwapChainD3D9> mSwapChain;

  /* Widget associated with this layer manager */
  nsIWidget *mWidget;

  /*
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

  /* Callback info for current transaction */
  CallbackInfo mCurrentCallbackInfo;

  /*
   * Region we're clipping our current drawing to.
   */
  nsIntRegion mClippingRegion;

  /*
   * Device reset count at last paint. Whenever this changes, we need to
   * do a full layer tree update.
   */
  PRUint32 mDeviceResetCount;

  /*
   * Render the current layer tree to the active target.
   */
  void Render();

  /*
   * Setup the pipeline.
   */
  void SetupPipeline();

  /*
   * Copies the content of our backbuffer to the set transaction target.
   */
  void PaintToTarget();

};

/*
 * General information and tree management for OGL layers.
 */
class LayerD3D9
{
public:
  LayerD3D9(LayerManagerD3D9 *aManager);

  virtual LayerD3D9 *GetFirstChildD3D9() { return nsnull; }

  void SetFirstChild(LayerD3D9 *aParent);

  virtual Layer* GetLayer() = 0;

  virtual void RenderLayer() = 0;

  /**
  /* This function may be used on device resets to clear all VRAM resources
   * that a layer might be using.
   */
  virtual void CleanResources() {}

  IDirect3DDevice9 *device() const { return mD3DManager->device(); }

  /* Called by the layer manager when it's destroyed */
  virtual void LayerManagerDestroyed() {}

  void ReportFailure(const nsACString &aMsg, HRESULT aCode) {
    return mD3DManager->ReportFailure(aMsg, aCode);
  }

  void SetShaderTransformAndOpacity()
  {
    Layer* layer = GetLayer();
    const gfx3DMatrix& transform = layer->GetEffectiveTransform();
    device()->SetVertexShaderConstantF(CBmLayerTransform, &transform._11, 4);

    float opacity[4];
    /*
     * We always upload a 4 component float, but the shader will use only the
     * first component since it's declared as a 'float'.
     */
    opacity[0] = layer->GetEffectiveOpacity();
    device()->SetPixelShaderConstantF(CBfLayerOpacity, opacity, 1);
  }

protected:
  LayerManagerD3D9 *mD3DManager;
};

/*
 * RAII helper for locking D3D9 textures.
 */
class LockTextureRectD3D9 
{
public:
  LockTextureRectD3D9(IDirect3DTexture9* aTexture) 
    : mTexture(aTexture)
  {
    mLockResult = mTexture->LockRect(0, &mR, NULL, 0);
  }

  ~LockTextureRectD3D9()
  {
    mTexture->UnlockRect(0);
  }

  bool HasLock() {
    return SUCCEEDED(mLockResult);
  }

  D3DLOCKED_RECT GetLockRect() 
  {
    return mR;
  }
private:
  LockTextureRectD3D9 (const LockTextureRectD3D9&);
  LockTextureRectD3D9& operator= (const LockTextureRectD3D9&);

  IDirect3DTexture9* mTexture;
  D3DLOCKED_RECT mR;
  HRESULT mLockResult;
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERMANAGERD3D9_H */

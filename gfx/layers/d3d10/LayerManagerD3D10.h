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

#ifndef GFX_LAYERMANAGERD3D10_H
#define GFX_LAYERMANAGERD3D10_H

#include "Layers.h"

#include <windows.h>
#include <d3d10_1.h>

#include "gfxContext.h"
#include "nsIWidget.h"

namespace mozilla {
namespace layers {

/**
 * This structure is used to pass rectangles to our shader constant. We can use
 * this for passing rectangular areas to SetVertexShaderConstant. In the format
 * of a 4 component float(x,y,width,height). Our vertex shader can then use
 * this to construct rectangular positions from the 0,0-1,1 quad that we source
 * it with.
 */
struct ShaderConstantRectD3D10
{
  float mX, mY, mWidth, mHeight;
  ShaderConstantRectD3D10(float aX, float aY, float aWidth, float aHeight)
    : mX(aX), mY(aY), mWidth(aWidth), mHeight(aHeight)
  { }

  // For easy passing to SetVertexShaderConstantF.
  operator float* () { return &mX; }
};

extern cairo_user_data_key_t gKeyD3D10Texture;

/*
 * This is the LayerManager used for Direct3D 9. For now this will render on
 * the main thread.
 */
class THEBES_API LayerManagerD3D10 : public LayerManager {
public:
  LayerManagerD3D10(nsIWidget *aWidget);
  virtual ~LayerManagerD3D10();

  /*
   * Initializes the layer manager, this is when the layer manager will
   * actually access the device and attempt to create the swap chain used
   * to draw to the window. If this method fails the device cannot be used.
   * This function is not threadsafe.
   *
   * \return True is initialization was succesful, false when it was not.
   */
  bool Initialize();

  /*
   * LayerManager implementation.
   */
  virtual void Destroy();

  virtual void SetRoot(Layer *aLayer);

  void BeginTransaction();

  void BeginTransactionWithTarget(gfxContext* aTarget);

  struct CallbackInfo {
    DrawThebesLayerCallback Callback;
    void *CallbackData;
  };

  void EndTransaction(DrawThebesLayerCallback aCallback,
                      void* aCallbackData);

  const CallbackInfo &GetCallbackInfo() { return mCurrentCallbackInfo; }

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();

  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();

  virtual already_AddRefed<ImageLayer> CreateImageLayer();

  virtual already_AddRefed<ColorLayer> CreateColorLayer();

  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();

  virtual already_AddRefed<ImageContainer> CreateImageContainer();

  virtual already_AddRefed<gfxASurface>
    CreateOptimalSurface(const gfxIntSize &aSize,
                         gfxASurface::gfxImageFormat imageFormat);

  virtual LayersBackend GetBackendType() { return LAYERS_D3D10; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("Direct3D 10"); }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "D3D9"; }
#endif // MOZ_LAYERS_HAVE_LOG

  // Public helpers

  ID3D10Device1 *device() const { return mDevice; }

  ID3D10Effect *effect() const { return mEffect; }

  void SetViewport(const nsIntSize &aViewport);
  const nsIntSize &GetViewport() { return mViewport; }

private:
  void SetupPipeline();
  void UpdateRenderTarget();
  void VerifyBufferSize();

  void Render();

  nsRefPtr<ID3D10Device1> mDevice;

  nsRefPtr<ID3D10Effect> mEffect;
  nsRefPtr<ID3D10InputLayout> mInputLayout;
  nsRefPtr<ID3D10Buffer> mVertexBuffer;

  nsRefPtr<ID3D10RenderTargetView> mRTView;

  nsRefPtr<IDXGISwapChain> mSwapChain;

  nsIWidget *mWidget;

  CallbackInfo mCurrentCallbackInfo;

  nsIntSize mViewport;

  /*
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

  /*
   * Copies the content of our backbuffer to the set transaction target.
   */
  void PaintToTarget();
};

/*
 * General information and tree management for OGL layers.
 */
class LayerD3D10
{
public:
  LayerD3D10(LayerManagerD3D10 *aManager);

  virtual LayerD3D10 *GetFirstChildD3D10() { return nsnull; }

  void SetFirstChild(LayerD3D10 *aParent);

  virtual Layer* GetLayer() = 0;

  /**
   * This will render a child layer to whatever render target is currently
   * active. aOpacity and aTransform will pass any 'carried' transformations
   * and/or opacity from the parent. This allows the parent to avoid
   * rendering to intermediate surfaces when possible.
   */
  virtual void RenderLayer(float aOpacity, const gfx3DMatrix &aTransform) = 0;
  virtual void Validate() {}

  ID3D10Device1 *device() const { return mD3DManager->device(); }
  ID3D10Effect *effect() const { return mD3DManager->effect(); }

  /* Called by the layer manager when it's destroyed */
  virtual void LayerManagerDestroyed() {}
protected:
  LayerManagerD3D10 *mD3DManager;
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERMANAGERD3D9_H */

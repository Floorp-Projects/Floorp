/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"
#include "Layers.h"

#include <windows.h>
#include <d3d10_1.h>

#include "gfxContext.h"
#include "nsIWidget.h"

#include "ReadbackManagerD3D10.h"

namespace mozilla {
namespace layers {

class DummyRoot;
class Nv3DVUtils;

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
 * This is the LayerManager used for Direct3D 10. For now this will
 * render on the main thread.
 *
 * For the time being, LayerManagerD3D10 both forwards layers
 * transactions and receives forwarded transactions.  In the Azure
 * future, it will only be a ShadowLayerManager.
 */
class THEBES_API LayerManagerD3D10 : public ShadowLayerManager,
                                     public ShadowLayerForwarder {
public:
  typedef LayerManager::LayersBackend LayersBackend;

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
  bool Initialize(bool force = false);

  /*
   * LayerManager implementation.
   */
  virtual void Destroy();

  virtual ShadowLayerForwarder* AsShadowForwarder()
  { return this; }

  virtual ShadowLayerManager* AsShadowManager()
  { return this; }

  virtual void SetRoot(Layer *aLayer);

  virtual void BeginTransaction();

  virtual void BeginTransactionWithTarget(gfxContext* aTarget);

  virtual bool EndEmptyTransaction();

  struct CallbackInfo {
    DrawThebesLayerCallback Callback;
    void *CallbackData;
  };

  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);

  const CallbackInfo &GetCallbackInfo() { return mCurrentCallbackInfo; }

  // D3D10 guarantees textures can be at least this size
  enum {
    MAX_TEXTURE_SIZE = 8192
  };
  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize)
  {
    return aSize <= gfxIntSize(MAX_TEXTURE_SIZE, MAX_TEXTURE_SIZE);
  }

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer();

  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ShadowContainerLayer> CreateShadowContainerLayer();

  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer()
  { return nsnull; }

  virtual already_AddRefed<ColorLayer> CreateColorLayer();
  virtual already_AddRefed<ShadowColorLayer> CreateShadowColorLayer()
  { return nsnull; }

  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer()
  { return nsnull; }

  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer();

  virtual already_AddRefed<gfxASurface>
    CreateOptimalSurface(const gfxIntSize &aSize,
                         gfxASurface::gfxImageFormat imageFormat);

  virtual TemporaryRef<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize &aSize,
                     mozilla::gfx::SurfaceFormat aFormat);

  virtual LayersBackend GetBackendType() { return LAYERS_D3D10; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("Direct3D 10"); }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "D3D10"; }
#endif // MOZ_LAYERS_HAVE_LOG

  // Public helpers

  ID3D10Device1 *device() const { return mDevice; }

  ID3D10Effect *effect() const { return mEffect; }

  ReadbackManagerD3D10 *readbackManager();

  void SetViewport(const nsIntSize &aViewport);
  const nsIntSize &GetViewport() { return mViewport; }

  /**
   * Return pointer to the Nv3DVUtils instance
   */
  Nv3DVUtils *GetNv3DVUtils()  { return mNv3DVUtils; }

  static void ReportFailure(const nsACString &aMsg, HRESULT aCode);

private:
  void SetupPipeline();
  void UpdateRenderTarget();
  void VerifyBufferSize();
  void EnsureReadbackManager();

  void Render();

  nsRefPtr<ID3D10Device1> mDevice;

  nsRefPtr<ID3D10Effect> mEffect;
  nsRefPtr<ID3D10InputLayout> mInputLayout;
  nsRefPtr<ID3D10Buffer> mVertexBuffer;
  nsRefPtr<ReadbackManagerD3D10> mReadbackManager;

  nsRefPtr<ID3D10RenderTargetView> mRTView;

  nsRefPtr<IDXGISwapChain> mSwapChain;

  nsIWidget *mWidget;

  CallbackInfo mCurrentCallbackInfo;

  nsIntSize mViewport;

  /* Nv3DVUtils instance */ 
  nsAutoPtr<Nv3DVUtils> mNv3DVUtils; 

  /*
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

  /*
   * We use a double-buffered "window surface" to display our content
   * in the compositor process, if we're remote.  The textures act
   * like the backing store for an OS window --- we render the layer
   * tree into the back texture and send it to the compositor, then
   * swap back/front textures.  This means, obviously, that we've lost
   * all layer tree information after rendering.
   *
   * The remote front buffer is the texture currently being displayed
   * by chrome.  We keep a reference to it to simplify resource
   * management; if we didn't, then there can be periods during IPC
   * transport when neither process holds a "real" ref.  That's
   * solvable but not worth the complexity.
   */
  nsRefPtr<ID3D10Texture2D> mBackBuffer;
  nsRefPtr<ID3D10Texture2D> mRemoteFrontBuffer;
  /*
   * If we're remote content, this is the root of the shadowable tree
   * we send to the compositor.
   */
  nsRefPtr<DummyRoot> mRootForShadowTree;

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
   * active.
   */
  virtual void RenderLayer() = 0;
  virtual void Validate() {}

  ID3D10Device1 *device() const { return mD3DManager->device(); }
  ID3D10Effect *effect() const { return mD3DManager->effect(); }

  /* Called by the layer manager when it's destroyed */
  virtual void LayerManagerDestroyed() {}

  /**
   * Return pointer to the Nv3DVUtils instance. Calls equivalent method in LayerManager.
   */
  Nv3DVUtils *GetNv3DVUtils()  { return mD3DManager->GetNv3DVUtils(); }


  void SetEffectTransformAndOpacity()
  {
    Layer* layer = GetLayer();
    const gfx3DMatrix& transform = layer->GetEffectiveTransform();
    void* raw = &const_cast<gfx3DMatrix&>(transform)._11;
    effect()->GetVariableByName("mLayerTransform")->SetRawValue(raw, 0, 64);
    effect()->GetVariableByName("fLayerOpacity")->AsScalar()->SetFloat(layer->GetEffectiveOpacity());
  }

protected:
  LayerManagerD3D10 *mD3DManager;
};

/**
 * WindowLayer is a simple, special kinds of shadowable layer into
 * which layer trees are rendered.  It represents something like an OS
 * window.  It exists only to allow sharing textures with the
 * compositor while reusing existing shadow-layer machinery.
 *
 * WindowLayer being implemented as a thebes layer isn't an important
 * detail; other layer types could have been used.
 */
class WindowLayer : public ThebesLayer, public ShadowableLayer {
public:
  WindowLayer(LayerManagerD3D10* aManager);
  virtual ~WindowLayer();

  void InvalidateRegion(const nsIntRegion&) {}
  Layer* AsLayer() { return this; }

  void SetShadow(PLayerChild* aChild) { mShadow = aChild; }
};

/**
 * DummyRoot is the root of the shadowable layer tree created by
 * remote content.  It exists only to contain WindowLayers.  It always
 * has exactly one child WindowLayer.
 */
class DummyRoot : public ContainerLayer, public ShadowableLayer {
public:
  DummyRoot(LayerManagerD3D10* aManager);
  virtual ~DummyRoot();

  void ComputeEffectiveTransforms(const gfx3DMatrix&) {}
  void InsertAfter(Layer*, Layer*);
  void RemoveChild(Layer*);
  Layer* AsLayer() { return this; }

  void SetShadow(PLayerChild* aChild) { mShadow = aChild; }
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERMANAGERD3D9_H */

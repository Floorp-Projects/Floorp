/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  virtual PRInt32 GetMaxTextureSize() const
  {
    return MAX_TEXTURE_SIZE;
  }

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer();

  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ShadowContainerLayer> CreateShadowContainerLayer();

  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer()
  { return nullptr; }

  virtual already_AddRefed<ColorLayer> CreateColorLayer();
  virtual already_AddRefed<ShadowColorLayer> CreateShadowColorLayer()
  { return nullptr; }

  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer()
  { return nullptr; }

  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer();

  virtual already_AddRefed<gfxASurface>
    CreateOptimalSurface(const gfxIntSize &aSize,
                         gfxASurface::gfxImageFormat imageFormat);

  virtual already_AddRefed<gfxASurface>
    CreateOptimalMaskSurface(const gfxIntSize &aSize);

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
  IDXGISwapChain *SwapChain() const
  {
    return mSwapChain;
  }
  ReadbackManagerD3D10 *readbackManager();

  void SetupInputAssembler();
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

  virtual LayerD3D10 *GetFirstChildD3D10() { return nullptr; }

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

  /*
   * Returns a shader resource view of a texture containing the contents of this
   * layer. Will try to return an existing texture if possible, or a temporary
   * one if not. It is the callee's responsibility to release the shader
   * resource view. Will return null if a texture could not be constructed.
   * The texture will not be transformed, i.e., it will be in the same coord
   * space as this.
   * Any layer that can be used as a mask layer should override this method.
   * If aSize is non-null, it will contain the size of the texture.
   */
  virtual already_AddRefed<ID3D10ShaderResourceView> GetAsTexture(gfxIntSize* aSize)
  {
    return nullptr;
  }

  void SetEffectTransformAndOpacity()
  {
    Layer* layer = GetLayer();
    const gfx3DMatrix& transform = layer->GetEffectiveTransform();
    void* raw = &const_cast<gfx3DMatrix&>(transform)._11;
    effect()->GetVariableByName("mLayerTransform")->SetRawValue(raw, 0, 64);
    effect()->GetVariableByName("fLayerOpacity")->AsScalar()->SetFloat(layer->GetEffectiveOpacity());
  }

protected:
  /*
   * Finds a texture for this layer's mask layer (if it has one) and sets it
   * as an input to the shaders.
   * Returns SHADER_MASK if a texture is loaded, SHADER_NO_MASK if there was no 
   * mask layer, or a texture for the mask layer could not be loaded.
   */
  PRUint8 LoadMaskTexture();

  /**
   * Select a shader technique using a combination of the following flags.
   * Not all combinations of flags are supported, and might cause an error,
   * check the fx file to see which shaders exist. In particular, aFlags should
   * include any combination of the 0x20 bit = 0 flags OR one of the 0x20 bit = 1
   * flags. Mask flags can be used in either case.
   */
  ID3D10EffectTechnique* SelectShader(PRUint8 aFlags);
  const static PRUint8 SHADER_NO_MASK = 0;
  const static PRUint8 SHADER_MASK = 0x1;
  const static PRUint8 SHADER_MASK_3D = 0x2;
  // 0x20 bit = 0
  const static PRUint8 SHADER_RGB = 0;
  const static PRUint8 SHADER_RGBA = 0x4;
  const static PRUint8 SHADER_NON_PREMUL = 0;
  const static PRUint8 SHADER_PREMUL = 0x8;
  const static PRUint8 SHADER_LINEAR = 0;
  const static PRUint8 SHADER_POINT = 0x10;
  // 0x20 bit = 1
  const static PRUint8 SHADER_YCBCR = 0x20;
  const static PRUint8 SHADER_COMPONENT_ALPHA = 0x24;
  const static PRUint8 SHADER_SOLID = 0x28;

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

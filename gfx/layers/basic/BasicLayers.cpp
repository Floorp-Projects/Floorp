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
 *   Robert O'Callahan <robert@ocallahan.org>
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

#include "BasicLayers.h"
#include "ImageLayers.h"

#include "nsTArray.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPattern.h"
#include "gfxUtils.h"

#include "GLContext.h"

namespace mozilla {
namespace layers {

class BasicContainerLayer;

/**
 * This is the ImplData for all Basic layers. It also exposes methods
 * private to the Basic implementation that are common to all Basic layer types.
 * In particular, there is an internal Paint() method that we can use
 * to paint the contents of non-Thebes layers.
 *
 * The class hierarchy for Basic layers is like this:
 *                                 BasicImplData
 * Layer                            |   |   |
 *  |                               |   |   |
 *  +-> ContainerLayer              |   |   |
 *  |    |                          |   |   |
 *  |    +-> BasicContainerLayer <--+   |   |
 *  |                                   |   |
 *  +-> ThebesLayer                     |   |
 *  |    |                              |   |
 *  |    +-> BasicThebesLayer <---------+   |
 *  |                                       |
 *  +-> ImageLayer                          |
 *       |                                  |
 *       +-> BasicImageLayer <--------------+
 */
class BasicImplData {
public:
  BasicImplData()
  {
    MOZ_COUNT_CTOR(BasicImplData);
  }
  ~BasicImplData()
  {
    MOZ_COUNT_DTOR(BasicImplData);
  }

  /**
   * Layers that paint themselves, such as ImageLayers, should paint
   * in response to this method call. aContext will already have been
   * set up to account for all the properties of the layer (transform,
   * opacity, etc).
   */
  virtual void Paint(gfxContext* aContext,
                     LayerManager::DrawThebesLayerCallback aCallback,
                     void* aCallbackData) {}
};

static BasicImplData*
ToData(Layer* aLayer)
{
  return static_cast<BasicImplData*>(aLayer->ImplData());
}

class BasicContainerLayer : public ContainerLayer, BasicImplData {
public:
  BasicContainerLayer(BasicLayerManager* aManager) :
    ContainerLayer(aManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicContainerLayer);
  }
  virtual ~BasicContainerLayer();

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerLayer::SetVisibleRegion(aRegion);
  }
  virtual void InsertAfter(Layer* aChild, Layer* aAfter);
  virtual void RemoveChild(Layer* aChild);

protected:
  void RemoveChildInternal(Layer* aChild);

  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

BasicContainerLayer::~BasicContainerLayer()
{
  while (mFirstChild) {
    RemoveChildInternal(mFirstChild);
  }

  MOZ_COUNT_DTOR(BasicContainerLayer);
}

void
BasicContainerLayer::InsertAfter(Layer* aChild, Layer* aAfter)
{
  NS_ASSERTION(BasicManager()->InConstruction(),
               "Can only set properties in construction phase");
  NS_ASSERTION(aChild->Manager() == Manager(),
               "Child has wrong manager");
  NS_ASSERTION(!aChild->GetParent(),
               "aChild already in the tree");
  NS_ASSERTION(!aChild->GetNextSibling() && !aChild->GetPrevSibling(),
               "aChild already has siblings?");
  NS_ASSERTION(!aAfter ||
               (aAfter->Manager() == Manager() &&
                aAfter->GetParent() == this),
               "aAfter is not our child");

  NS_ADDREF(aChild);

  aChild->SetParent(this);
  if (!aAfter) {
    aChild->SetNextSibling(mFirstChild);
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(aChild);
    }
    mFirstChild = aChild;
    return;
  }

  Layer* next = aAfter->GetNextSibling();
  aChild->SetNextSibling(next);
  aChild->SetPrevSibling(aAfter);
  if (next) {
    next->SetPrevSibling(aChild);
  }
  aAfter->SetNextSibling(aChild);
}

void
BasicContainerLayer::RemoveChild(Layer* aChild)
{
  NS_ASSERTION(BasicManager()->InConstruction(),
               "Can only set properties in construction phase");
  RemoveChildInternal(aChild);
}

void
BasicContainerLayer::RemoveChildInternal(Layer* aChild)
{
  NS_ASSERTION(aChild->Manager() == Manager(),
               "Child has wrong manager");
  NS_ASSERTION(aChild->GetParent() == this,
               "aChild not our child");

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  }

  aChild->SetNextSibling(nsnull);
  aChild->SetPrevSibling(nsnull);
  aChild->SetParent(nsnull);

  NS_RELEASE(aChild);
}

/**
 * BasicThebesLayer does not currently retain any buffers. This
 * makes the implementation fairly simple. During a transaction the
 * valid region is just the visible region, and between transactions
 * the valid region is empty.
 */
class BasicThebesLayer : public ThebesLayer, BasicImplData {
public:
  BasicThebesLayer(BasicLayerManager* aLayerManager) :
    ThebesLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicThebesLayer);
  }
  virtual ~BasicThebesLayer()
  {
    MOZ_COUNT_DTOR(BasicThebesLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ThebesLayer::SetVisibleRegion(aRegion);
  }
  virtual void InvalidateRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
  }

  virtual void Paint(gfxContext* aContext,
                     LayerManager::DrawThebesLayerCallback aCallback,
                     void* aCallbackData);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

void
BasicThebesLayer::Paint(gfxContext* aContext,
                        LayerManager::DrawThebesLayerCallback aCallback,
                        void* aCallbackData)
{
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");
  gfxContext* target = BasicManager()->GetTarget();
  NS_ASSERTION(target, "We shouldn't be called if there's no target");

  aCallback(this, target, mVisibleRegion, aCallbackData);
}

class BasicImageLayer : public ImageLayer, BasicImplData {
public:
  BasicImageLayer(BasicLayerManager* aLayerManager) :
    ImageLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicImageLayer);
  }
  virtual ~BasicImageLayer()
  {
    MOZ_COUNT_DTOR(BasicImageLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mVisibleRegion = aRegion;
  }

  virtual void Paint(gfxContext* aContext,
                     LayerManager::DrawThebesLayerCallback aCallback,
                     void* aCallbackData);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

void
BasicImageLayer::Paint(gfxContext* aContext,
                       LayerManager::DrawThebesLayerCallback aCallback,
                       void* aCallbackData)
{
  if (!mContainer)
    return;

  gfxIntSize size;
  nsRefPtr<gfxASurface> surface = mContainer->GetCurrentAsSurface(&size);
  if (!surface) {
    return;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);
  if (!pat) {
    return;
  }

  pat->SetFilter(mFilter);

  // Set PAD mode so that when the video is being scaled, we do not sample
  // outside the bounds of the video image.
  gfxPattern::GraphicsExtend extend = gfxPattern::EXTEND_PAD;

  // PAD is slow with X11 and Quartz surfaces, so prefer speed over correctness
  // and use NONE.
  nsRefPtr<gfxASurface> target = aContext->CurrentSurface();
  gfxASurface::gfxSurfaceType type = target->GetType();
  if (type == gfxASurface::SurfaceTypeXlib ||
      type == gfxASurface::SurfaceTypeXcb ||
      type == gfxASurface::SurfaceTypeQuartz) {
    extend = gfxPattern::EXTEND_NONE;
  }

  pat->SetExtend(extend);

  /* Draw RGB surface onto frame */
  aContext->NewPath();
  aContext->PixelSnappedRectangleAndSetPattern(
      gfxRect(0, 0, size.width, size.height), pat);
  aContext->Fill();
}

class BasicColorLayer : public ColorLayer, BasicImplData {
public:
  BasicColorLayer(BasicLayerManager* aLayerManager) :
    ColorLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicColorLayer);
  }
  virtual ~BasicColorLayer()
  {
    MOZ_COUNT_DTOR(BasicColorLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mVisibleRegion = aRegion;
  }

  virtual void Paint(gfxContext* aContext,
                     LayerManager::DrawThebesLayerCallback aCallback,
                     void* aCallbackData);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

void
BasicColorLayer::Paint(gfxContext* aContext,
                       LayerManager::DrawThebesLayerCallback aCallback,
                       void* aCallbackData)
{
  aContext->SetColor(mColor);
  aContext->Paint();
}

class BasicCanvasLayer : public CanvasLayer,
                         BasicImplData
{
public:
  BasicCanvasLayer(BasicLayerManager* aLayerManager) :
    CanvasLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicCanvasLayer);
  }
  virtual ~BasicCanvasLayer()
  {
    MOZ_COUNT_DTOR(BasicCanvasLayer);
  }

  virtual void Initialize(const Data& aData);
  virtual void Updated(const nsIntRect& aRect);
  virtual void Paint(gfxContext* aContext,
                     LayerManager::DrawThebesLayerCallback aCallback,
                     void* aCallbackData);

protected:
  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<mozilla::gl::GLContext> mGLContext;

  nsIntRect mBounds;
  nsIntRect mUpdatedRect;

  PRPackedBool mGLBufferIsPremultiplied;
  PRPackedBool mNeedsYFlip;
};

void
BasicCanvasLayer::Initialize(const Data& aData)
{
  NS_ASSERTION(mSurface == nsnull, "BasicCanvasLayer::Initialize called twice!");

  mUpdatedRect.Empty();

  if (aData.mSurface) {
    mSurface = aData.mSurface;
    NS_ASSERTION(aData.mGLContext == nsnull,
                 "CanvasLayer can't have both surface and GLContext");
    mNeedsYFlip = PR_FALSE;
  } else if (aData.mGLContext) {
    mGLContext = aData.mGLContext;
    mGLBufferIsPremultiplied = aData.mGLBufferIsPremultiplied;
    mNeedsYFlip = PR_TRUE;
  } else {
    NS_ERROR("CanvasLayer created without mSurface or mGLContext?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
}

void
BasicCanvasLayer::Updated(const nsIntRect& aRect)
{
  NS_ASSERTION(mUpdatedRect.IsEmpty(),
               "CanvasLayer::Updated called more than once in a transaction!");

  mUpdatedRect.UnionRect(mUpdatedRect, aRect);

  if (mGLContext) {
    nsRefPtr<gfxImageSurface> isurf =
      new gfxImageSurface(gfxIntSize(mBounds.width, mBounds.height),
                          IsOpaqueContent()
                            ? gfxASurface::ImageFormatRGB24
                            : gfxASurface::ImageFormatARGB32);
    if (!isurf || isurf->CairoStatus() != 0) {
      return;
    }

    NS_ASSERTION(isurf->Stride() == mBounds.width * 4, "gfxImageSurface stride isn't what we expect!");

    // We need to read from the GLContext
    mGLContext->MakeCurrent();

    // We have to flush to ensure that any buffered GL operations are
    // in the framebuffer before we read.
    mGLContext->fFlush();

    // For simplicity, we read the entire framebuffer for now -- in
    // the future we should use mUpdatedRect, though with WebGL we don't
    // have an easy way to generate one.
#ifndef USE_GLES2
    mGLContext->fReadPixels(0, 0, mBounds.width, mBounds.height,
                            LOCAL_GL_BGRA, LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV,
                            isurf->Data());
#else
    mGLContext->fReadPixels(0, 0, mBounds.width, mBounds.height,
                            LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                            isurf->Data());
#endif

    // If the underlying GLContext doesn't have a framebuffer into which
    // premultiplied values were written, we have to do this ourselves here.
    // Note that this is a WebGL attribute; GL itself has no knowledge of
    // premultiplied or unpremultiplied alpha.
    if (!mGLBufferIsPremultiplied)
      gfxUtils::PremultiplyImageSurface(isurf);

    // stick our surface into mSurface, so that the Paint() path is the same
    mSurface = isurf;
  }

  // sanity
  NS_ASSERTION(mUpdatedRect.IsEmpty() || mBounds.Contains(mUpdatedRect),
               "CanvasLayer: Updated rect bigger than bounds!");
}

void
BasicCanvasLayer::Paint(gfxContext* aContext,
                        LayerManager::DrawThebesLayerCallback aCallback,
                        void* aCallbackData)
{
  nsRefPtr<gfxPattern> pat = new gfxPattern(mSurface);

  pat->SetFilter(mFilter);
  pat->SetExtend(gfxPattern::EXTEND_PAD);

  gfxRect r(0, 0, mBounds.width, mBounds.height);
  gfxMatrix m;
  if (mNeedsYFlip) {
    m = aContext->CurrentMatrix();
    aContext->Translate(gfxPoint(0.0, mBounds.height));
    aContext->Scale(1.0, -1.0);
  }

  aContext->NewPath();
  aContext->PixelSnappedRectangleAndSetPattern(r, pat);
  aContext->Fill();

  if (mNeedsYFlip) {
    aContext->SetMatrix(m);
  }

  mUpdatedRect.Empty();
}

BasicLayerManager::BasicLayerManager(gfxContext* aContext) :
  mDefaultTarget(aContext)
#ifdef DEBUG
  , mPhase(PHASE_NONE)
#endif
{
  MOZ_COUNT_CTOR(BasicLayerManager);
}

BasicLayerManager::~BasicLayerManager()
{
  NS_ASSERTION(mPhase == PHASE_NONE, "Died during transaction?");
  MOZ_COUNT_DTOR(BasicLayerManager);
}

void
BasicLayerManager::SetDefaultTarget(gfxContext* aContext)
{
  NS_ASSERTION(mPhase == PHASE_NONE,
               "Must set default target outside transaction");
  mDefaultTarget = aContext;
}

void
BasicLayerManager::BeginTransaction()
{
  NS_ASSERTION(mPhase == PHASE_NONE, "Nested transactions not allowed");
#ifdef DEBUG
  mPhase = PHASE_CONSTRUCTION;
#endif
  mTarget = mDefaultTarget;
}

void
BasicLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
  NS_ASSERTION(mPhase == PHASE_NONE, "Nested transactions not allowed");
#ifdef DEBUG
  mPhase = PHASE_CONSTRUCTION;
#endif
  mTarget = aTarget;
}

void
BasicLayerManager::EndTransaction(DrawThebesLayerCallback aCallback,
                                  void* aCallbackData)
{
  NS_ASSERTION(mRoot, "Root not set");
  NS_ASSERTION(mPhase == PHASE_CONSTRUCTION, "Should be in construction phase");
#ifdef DEBUG
  mPhase = PHASE_DRAWING;
#endif

  if (mTarget) {
    PaintLayer(mRoot, aCallback, aCallbackData);
    mTarget = nsnull;
  }

#ifdef DEBUG
  mPhase = PHASE_NONE;
#endif
  // No retained layers supported for now
  mRoot = nsnull;
}

void
BasicLayerManager::SetRoot(Layer* aLayer)
{
  NS_ASSERTION(aLayer, "Root can't be null");
  NS_ASSERTION(aLayer->Manager() == this, "Wrong manager");
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  mRoot = aLayer;
}

// Returns true if painting aLayer requires a PushGroup
static PRBool
NeedsGroup(Layer* aLayer)
{
  return aLayer->GetOpacity() != 1.0;
}

// Returns true if we need to save the state of the gfxContext when
// we start painting aLayer (and restore the state when we've finished
// painting aLayer)
static PRBool
NeedsState(Layer* aLayer)
{
  return aLayer->GetClipRect() != nsnull ||
         !aLayer->GetTransform().IsIdentity();
}

// Returns true if it's OK to save the contents of aLayer in an
// opaque surface (a surface without an alpha channel).
// If we can use a surface without an alpha channel, we should, because
// it will often make painting of antialiased text faster and higher
// quality.
static PRBool
UseOpaqueSurface(Layer* aLayer)
{
  // If the visible content in the layer is opaque, there is no need
  // for an alpha channel.
  if (aLayer->IsOpaqueContent())
    return PR_TRUE;
  // Also, if this layer is the bottommost layer in a container which
  // doesn't need an alpha channel, we can use an opaque surface for this
  // layer too. Any transparent areas must be covered by something else
  // in the container.
  BasicContainerLayer* parent =
    static_cast<BasicContainerLayer*>(aLayer->GetParent());
  return parent && parent->GetFirstChild() == aLayer &&
         UseOpaqueSurface(parent);
}

void
BasicLayerManager::PaintLayer(Layer* aLayer,
                              DrawThebesLayerCallback aCallback,
                              void* aCallbackData)
{
  PRBool needsGroup = NeedsGroup(aLayer);
  PRBool needsSaveRestore = needsGroup || NeedsState(aLayer);

 if (needsSaveRestore) {
    mTarget->Save();

    if (aLayer->GetClipRect()) {
      const nsIntRect& r = *aLayer->GetClipRect();
      mTarget->NewPath();
      mTarget->Rectangle(gfxRect(r.x, r.y, r.width, r.height), PR_TRUE);
      mTarget->Clip();
    }

    gfxMatrix transform;
    // XXX we need to add some kind of 3D transform support, possibly
    // using pixman?
    NS_ASSERTION(aLayer->GetTransform().Is2D(),
                 "Only 2D transforms supported currently");
    aLayer->GetTransform().Is2D(&transform);
    mTarget->Multiply(transform);

    if (needsGroup) {
      // If we need to call PushGroup, we should clip to the smallest possible
      // area first to minimize the size of the temporary surface.
      nsIntRect bbox = aLayer->GetVisibleRegion().GetBounds();
      gfxRect deviceRect =
        mTarget->UserToDevice(gfxRect(bbox.x, bbox.y, bbox.width, bbox.height));
      deviceRect.RoundOut();

      gfxMatrix currentMatrix = mTarget->CurrentMatrix();
      mTarget->IdentityMatrix();
      mTarget->NewPath();
      mTarget->Rectangle(deviceRect);
      mTarget->Clip();
      mTarget->SetMatrix(currentMatrix);

      gfxASurface::gfxContentType type = UseOpaqueSurface(aLayer)
          ? gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA;
      mTarget->PushGroup(type);
    }
  }

  ToData(aLayer)->Paint(mTarget, aCallback, aCallbackData);
  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    PaintLayer(child, aCallback, aCallbackData);
  }

  if (needsSaveRestore) {
    if (needsGroup) {
      mTarget->PopGroupToSource();
      mTarget->Paint(aLayer->GetOpacity());
    }

    mTarget->Restore();
  }
}

already_AddRefed<ThebesLayer>
BasicLayerManager::CreateThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ThebesLayer> layer = new BasicThebesLayer(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
BasicLayerManager::CreateContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ContainerLayer> layer = new BasicContainerLayer(this);
  return layer.forget();
}

already_AddRefed<ImageLayer>
BasicLayerManager::CreateImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ImageLayer> layer = new BasicImageLayer(this);
  return layer.forget();
}

already_AddRefed<ColorLayer>
BasicLayerManager::CreateColorLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ColorLayer> layer = new BasicColorLayer(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
BasicLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<CanvasLayer> layer = new BasicCanvasLayer(this);
  return layer.forget();
}

}
}

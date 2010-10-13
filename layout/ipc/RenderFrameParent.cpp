/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

#include "mozilla/layers/ShadowLayersParent.h"

#include "BasicLayers.h"
#include "LayerManagerOGL.h"
#include "RenderFrameParent.h"

#include "gfx3DMatrix.h"
#include "nsFrameLoader.h"
#include "nsViewportFrame.h"

typedef nsFrameLoader::ViewportConfig ViewportConfig;
using namespace mozilla::layers;

namespace mozilla {
namespace layout {

static void
AssertInTopLevelChromeDoc(ContainerLayer* aContainer,
                          nsIFrame* aContainedFrame)
{
  NS_ASSERTION(
    (aContainer->Manager()->GetBackendType() != LayerManager::LAYERS_BASIC) ||
    (aContainedFrame->GetNearestWidget() ==
     static_cast<BasicLayerManager*>(aContainer->Manager())->GetRetainerWidget()),
    "Expected frame to be in top-level chrome document");
}

static void
AssertValidContainerOfShadowTree(ContainerLayer* aContainer,
                                 Layer* aShadowRoot)
{
  NS_ABORT_IF_FALSE(
    !aContainer || (aShadowRoot &&
                    aShadowRoot == aContainer->GetFirstChild() &&
                    nsnull == aShadowRoot->GetNextSibling()),
    "container of shadow tree may only be null or have 1 child that is the shadow root");
}

// Compute the transform of the shadow tree contained by
// |aContainerFrame| to widget space.  We transform because the
// subprocess layer manager renders to a different top-left than where
// the shadow tree is drawn here and because a scale can be set on the
// shadow tree.
static void
ComputeShadowTreeTransform(nsIFrame* aContainerFrame,
                           const FrameMetrics& aMetrics,
                           const ViewportConfig& aConfig,
                           nsDisplayListBuilder* aBuilder,
                           nsIntPoint* aShadowTranslation,
                           float* aShadowXScale,
                           float* aShadowYScale)
{
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();
  // Offset to the content rect in case we have borders or padding
  nsPoint frameOffset =
    (aBuilder->ToReferenceFrame(aContainerFrame->GetParent()) +
     aContainerFrame->GetContentRect().TopLeft());
  *aShadowTranslation = frameOffset.ToNearestPixels(auPerDevPixel);

  // |aMetrics.mViewportScrollOffset| was the content document's
  // scroll offset when it was painted (the document pixel at CSS
  // viewport (0,0)).  |aConfig.mScrollOffset| is what our user
  // expects, or wants, the content-document scroll offset to be.  So
  // we set a compensating translation that moves the content document
  // pixels to where the user wants them to be.
  nsIntPoint scrollCompensation =
    (aConfig.mScrollOffset.ToNearestPixels(auPerDevPixel));
  scrollCompensation.x -= aMetrics.mViewportScrollOffset.x * aConfig.mXScale;
  scrollCompensation.y -= aMetrics.mViewportScrollOffset.y * aConfig.mYScale;
  *aShadowTranslation -= scrollCompensation;

  *aShadowXScale = aConfig.mXScale;
  *aShadowYScale = aConfig.mYScale;
}

static void
UpdateShadowSubtree(Layer* aSubtreeRoot)
{
  ShadowLayer* shadow = aSubtreeRoot->AsShadowLayer();

  shadow->SetShadowClipRect(aSubtreeRoot->GetClipRect());
  shadow->SetShadowTransform(aSubtreeRoot->GetTransform());
  shadow->SetShadowVisibleRegion(aSubtreeRoot->GetVisibleRegion());

  for (Layer* child = aSubtreeRoot->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    UpdateShadowSubtree(child);
  }
}

static void
TransformShadowTreeTo(ContainerLayer* aRoot,
                      const nsIntRect& aVisibleRect,
                      const nsIntPoint& aTranslation,
                      float aXScale, float aYScale)
{
  UpdateShadowSubtree(aRoot);

  ShadowLayer* shadow = aRoot->AsShadowLayer();
  NS_ABORT_IF_FALSE(aRoot->GetTransform() == shadow->GetShadowTransform(),
                    "transforms should be the same now");
  NS_ABORT_IF_FALSE(aRoot->GetTransform().Is2D(),
                    "only 2D transforms expected currently");
  gfxMatrix shadowTransform;
  shadow->GetShadowTransform().Is2D(&shadowTransform);
  // Pre-multiply this transform into the shadow's transform, so that
  // it occurs before any transform set by the child
  shadowTransform.Translate(gfxPoint(aTranslation.x, aTranslation.y));
  shadowTransform.Scale(aXScale, aYScale);
  shadow->SetShadowTransform(gfx3DMatrix::From2D(shadowTransform));
}

static Layer*
ShadowRootOf(ContainerLayer* aContainer)
{
  NS_ABORT_IF_FALSE(aContainer, "need a non-null container");

  Layer* shadowRoot = aContainer->GetFirstChild();
  NS_ABORT_IF_FALSE(!shadowRoot || nsnull == shadowRoot->GetNextSibling(),
                    "shadow root container may only have 0 or 1 children");
  return shadowRoot;
}

// Return true iff |aManager| is a "temporary layer manager".  They're
// used for small software rendering tasks, like drawWindow.  That's
// currently implemented by a BasicLayerManager without a backing
// widget, and hence in non-retained mode.
static PRBool
IsTempLayerManager(LayerManager* aManager)
{
  return (LayerManager::LAYERS_BASIC == aManager->GetBackendType() &&
          !static_cast<BasicLayerManager*>(aManager)->IsRetained());
}

RenderFrameParent::RenderFrameParent(nsFrameLoader* aFrameLoader)
  : mFrameLoader(aFrameLoader)
{}

RenderFrameParent::~RenderFrameParent()
{}

void
RenderFrameParent::ShadowLayersUpdated()
{
  mFrameLoader->SetCurrentRemoteFrame(this);

  nsIFrame* docFrame = mFrameLoader->GetPrimaryFrameOfOwningContent();
  if (!docFrame) {
    // Bad, but nothing we can do about it (XXX/cjones: or is there?
    // maybe bug 589337?).  When the new frame is created, we'll
    // probably still be the current render frame and will get to draw
    // our content then.  Or, we're shutting down and this update goes
    // to /dev/null.
    return;
  }

  // FIXME/cjones: we should collect the rects/regions updated for
  // Painted*Layer() calls and pass that region to here, then only
  // invalidate that rect
  //
  // We pass INVALIDATE_NO_THEBES_LAYERS here because we're
  // invalidating the <browser> on behalf of its counterpart in the
  // content process.  Not only do we not need to invalidate the
  // shadow layers, things would just break if we did --- we have no
  // way to repaint shadow layers from this process.
  nsRect rect = nsRect(nsPoint(0, 0), docFrame->GetRect().Size());
  docFrame->InvalidateWithFlags(rect, nsIFrame::INVALIDATE_NO_THEBES_LAYERS);
}

already_AddRefed<Layer>
RenderFrameParent::BuildLayer(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame,
                              LayerManager* aManager,
                              const nsIntRect& aVisibleRect)
{
  NS_ABORT_IF_FALSE(aFrame,
                    "makes no sense to have a shadow tree without a frame");
  NS_ABORT_IF_FALSE(!mContainer ||
                    IsTempLayerManager(aManager) ||
                    mContainer->Manager() == aManager,
                    "retaining manager changed out from under us ... HELP!");

  if (mContainer && mContainer->Manager() != aManager) {
    // This can happen if aManager is a "temporary" manager, or if the
    // widget's layer manager changed out from under us.  We need to
    // FIXME handle the former case somehow, probably with an API to
    // draw a manager's subtree.  The latter is bad bad bad, but the
    // the NS_ABORT_IF_FALSE() above will flag it.  Returning NULL
    // here will just cause the shadow subtree not to be rendered.
    return nsnull;
  }

  Layer* containerShadowRoot = mContainer ? ShadowRootOf(mContainer) : nsnull;
  ContainerLayer* shadowRoot = GetRootLayer();
  NS_ABORT_IF_FALSE(!shadowRoot || shadowRoot->Manager() == aManager,
                    "retaining manager changed out from under us ... HELP!");

  if (mContainer && shadowRoot != containerShadowRoot) {
    // Shadow root changed.  Remove it from the container, if it
    // existed.
    if (containerShadowRoot) {
      mContainer->RemoveChild(containerShadowRoot);
    }
  }

  if (!shadowRoot) {
    // No shadow layer tree at the moment.
    mContainer = nsnull;
  } else if (shadowRoot != containerShadowRoot) {
    // Wrap the shadow layer tree in mContainer.
    if (!mContainer) {
      mContainer = aManager->CreateContainerLayer();
    }
    NS_ABORT_IF_FALSE(!mContainer->GetFirstChild(),
                      "container of shadow tree shouldn't have a 'root' here");

    mContainer->InsertAfter(shadowRoot, nsnull);
  }

  if (mContainer) {
    AssertInTopLevelChromeDoc(mContainer, aFrame);
    nsIntPoint shadowTranslation;
    float shadowXScale, shadowYScale;
    ComputeShadowTreeTransform(aFrame,
                               shadowRoot->GetFrameMetrics(),
                               mFrameLoader->GetViewportConfig(),
                               aBuilder,
                               &shadowTranslation,
                               &shadowXScale, &shadowYScale);
    TransformShadowTreeTo(shadowRoot, aVisibleRect,
                          shadowTranslation, shadowXScale, shadowYScale);
    mContainer->SetClipRect(nsnull);
  }

  AssertValidContainerOfShadowTree(mContainer, shadowRoot);
  return nsRefPtr<Layer>(mContainer).forget();
}

void
RenderFrameParent::ActorDestroy(ActorDestroyReason why)
{
  if (mFrameLoader->GetCurrentRemoteFrame() == this) {
    // XXX this might cause some weird issues ... we'll just not
    // redraw the part of the window covered by this until the "next"
    // remote frame has a layer-tree transaction.  For
    // why==NormalShutdown, we'll definitely want to do something
    // better, especially as nothing guarantees another Update() from
    // the "next" remote layer tree.
    mFrameLoader->SetCurrentRemoteFrame(nsnull);
  }
  mFrameLoader = nsnull;
}

PLayersParent*
RenderFrameParent::AllocPLayers()
{
  LayerManager* lm = GetLayerManager();
  switch (lm->GetBackendType()) {
  case LayerManager::LAYERS_BASIC: {
    BasicShadowLayerManager* bslm = static_cast<BasicShadowLayerManager*>(lm);
    return new ShadowLayersParent(bslm);
  }
  case LayerManager::LAYERS_OPENGL: {
    LayerManagerOGL* lmo = static_cast<LayerManagerOGL*>(lm);
    return new ShadowLayersParent(lmo);
  }
  default: {
    NS_WARNING("shadow layers no sprechen D3D backend yet");
    return nsnull;
  }
  }
}

bool
RenderFrameParent::DeallocPLayers(PLayersParent* aLayers)
{
  delete aLayers;
  return true;
}

LayerManager*
RenderFrameParent::GetLayerManager() const
{
  nsIDocument* doc = mFrameLoader->GetOwnerDoc();
  return doc->GetShell()->GetLayerManager();
}

ShadowLayersParent*
RenderFrameParent::GetShadowLayers() const
{
  const nsTArray<PLayersParent*>& shadowParents = ManagedPLayersParent();
  NS_ABORT_IF_FALSE(shadowParents.Length() <= 1,
                    "can only support at most 1 ShadowLayersParent");
  return (shadowParents.Length() == 1) ?
    static_cast<ShadowLayersParent*>(shadowParents[0]) : nsnull;
}

ContainerLayer*
RenderFrameParent::GetRootLayer() const
{
  ShadowLayersParent* shadowLayers = GetShadowLayers();
  return shadowLayers ? shadowLayers->GetRoot() : nsnull;
}

}  // namespace layout
}  // namespace mozilla

already_AddRefed<Layer>
nsDisplayRemote::BuildLayer(nsDisplayListBuilder* aBuilder,
                            LayerManager* aManager)
{
  PRInt32 appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntRect visibleRect = GetVisibleRect().ToNearestPixels(appUnitsPerDevPixel);
  nsRefPtr<Layer> layer = mRemoteFrame->BuildLayer(aBuilder, mFrame, aManager, visibleRect);
  return layer.forget();
}

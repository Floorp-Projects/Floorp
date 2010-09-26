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

// Update the translation from |aContainedFrame| space to widget
// space.  We translate because the subprocess layer manager thinks
// it's rendering to top-left=<0, 0> (which is good!).
static void
SetTransformFor(ContainerLayer* aContainer, nsIFrame* aContainedFrame,
                const FrameMetrics& aMetrics, const ViewportConfig& aConfig,
                nsDisplayListBuilder* aBuilder)
{
  NS_ABORT_IF_FALSE(aContainer && aContainedFrame, "args must be nonnull");
  AssertInTopLevelChromeDoc(aContainer, aContainedFrame);

  nscoord auPerDevPixel = aContainedFrame->PresContext()->AppUnitsPerDevPixel();
  // Offset to the content rect in case we have borders or padding
  nsPoint frameOffset =
    (aBuilder->ToReferenceFrame(aContainedFrame->GetParent()) +
     aContainedFrame->GetContentRect().TopLeft());
  nsIntPoint translation = frameOffset.ToNearestPixels(auPerDevPixel);

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
  translation -= scrollCompensation;

  gfxMatrix transform;
  transform.Translate(gfxPoint(translation.x, translation.y));
  transform.Scale(aConfig.mXScale, aConfig.mYScale);
  aContainer->SetTransform(gfx3DMatrix::From2D(transform));
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
                              LayerManager* aManager)
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
    SetTransformFor(mContainer, aFrame,
                    shadowRoot->GetFrameMetrics(),
                    mFrameLoader->GetViewportConfig(),
                    aBuilder);
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
  if (LayerManager::LAYERS_BASIC != lm->GetBackendType()) {
    NS_WARNING("shadow layers no sprechen GL backend yet");
    return nsnull;
  }    

  BasicShadowLayerManager* bslm = static_cast<BasicShadowLayerManager*>(lm);
  return new ShadowLayersParent(bslm);
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
  nsRefPtr<Layer> layer = mRemoteFrame->BuildLayer(aBuilder, mFrame, aManager);
  return layer.forget();
}

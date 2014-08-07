/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGClipPathFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "nsGkAtoms.h"
#include "nsRenderingContext.h"
#include "mozilla/dom/SVGClipPathElement.h"
#include "nsSVGEffects.h"
#include "nsSVGUtils.h"

using namespace mozilla::dom;

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGClipPathFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGClipPathFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGClipPathFrame)

nsresult
nsSVGClipPathFrame::ClipPaint(nsRenderingContext* aContext,
                              nsIFrame* aParent,
                              const gfxMatrix &aMatrix)
{
  // If the flag is set when we get here, it means this clipPath frame
  // has already been used painting the current clip, and the document
  // has a clip reference loop.
  if (mInUse) {
    NS_WARNING("Clip loop detected!");
    return NS_OK;
  }
  AutoClipPathReferencer clipRef(this);

  mClipParent = aParent;
  if (mClipParentMatrix) {
    *mClipParentMatrix = aMatrix;
  } else {
    mClipParentMatrix = new gfxMatrix(aMatrix);
  }

  gfxContext *gfx = aContext->ThebesContext();

  nsISVGChildFrame *singleClipPathChild = nullptr;

  if (IsTrivial(&singleClipPathChild)) {
    // Notify our child that it's painting as part of a clipPath, and that
    // we only require it to draw its path (it should skip filling, etc.):
    SVGAutoRenderState mode(aContext, SVGAutoRenderState::CLIP);

    if (!singleClipPathChild) {
      // We have no children - the spec says clip away everything:
      gfx->Rectangle(gfxRect());
    } else {
      singleClipPathChild->NotifySVGChanged(
                             nsISVGChildFrame::TRANSFORM_CHANGED);
      singleClipPathChild->PaintSVG(aContext, nullptr);
    }
    gfx->Clip();
    gfx->NewPath();
    return NS_OK;
  }

  // Seems like this is a non-trivial clipPath, so we need to use a clip mask.

  // Notify our children that they're painting into a clip mask:
  SVGAutoRenderState mode(aContext, SVGAutoRenderState::CLIP_MASK);

  // Check if this clipPath is itself clipped by another clipPath:
  nsSVGClipPathFrame *clipPathFrame =
    nsSVGEffects::GetEffectProperties(this).GetClipPathFrame(nullptr);
  bool referencedClipIsTrivial;
  if (clipPathFrame) {
    referencedClipIsTrivial = clipPathFrame->IsTrivial();
    gfx->Save();
    if (referencedClipIsTrivial) {
      clipPathFrame->ClipPaint(aContext, aParent, aMatrix);
    } else {
      gfx->PushGroup(gfxContentType::ALPHA);
    }
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      // The CTM of each frame referencing us can be different.
      SVGFrame->NotifySVGChanged(nsISVGChildFrame::TRANSFORM_CHANGED);

      bool isOK = true;
      nsSVGClipPathFrame *clipPathFrame =
        nsSVGEffects::GetEffectProperties(kid).GetClipPathFrame(&isOK);
      if (!isOK) {
        continue;
      }

      bool isTrivial;

      if (clipPathFrame) {
        isTrivial = clipPathFrame->IsTrivial();
        gfx->Save();
        if (isTrivial) {
          clipPathFrame->ClipPaint(aContext, aParent, aMatrix);
        } else {
          gfx->PushGroup(gfxContentType::ALPHA);
        }
      }

      SVGFrame->PaintSVG(aContext, nullptr);

      if (clipPathFrame) {
        if (!isTrivial) {
          gfx->PopGroupToSource();

          nsRefPtr<gfxPattern> clipMaskSurface;
          gfx->PushGroup(gfxContentType::ALPHA);

          clipPathFrame->ClipPaint(aContext, aParent, aMatrix);
          clipMaskSurface = gfx->PopGroup();

          if (clipMaskSurface) {
            gfx->Mask(clipMaskSurface);
          }
        }
        gfx->Restore();
      }
    }
  }

  if (clipPathFrame) {
    if (!referencedClipIsTrivial) {
      gfx->PopGroupToSource();

      nsRefPtr<gfxPattern> clipMaskSurface;
      gfx->PushGroup(gfxContentType::ALPHA);

      clipPathFrame->ClipPaint(aContext, aParent, aMatrix);
      clipMaskSurface = gfx->PopGroup();

      if (clipMaskSurface) {
        gfx->Mask(clipMaskSurface);
      }
    }
    gfx->Restore();
  }

  return NS_OK;
}

bool
nsSVGClipPathFrame::PointIsInsideClipPath(nsIFrame* aClippedFrame,
                                          const gfxPoint &aPoint)
{
  // If the flag is set when we get here, it means this clipPath frame
  // has already been used in hit testing against the current clip,
  // and the document has a clip reference loop.
  if (mInUse) {
    NS_WARNING("Clip loop detected!");
    return false;
  }
  AutoClipPathReferencer clipRef(this);

  gfxMatrix matrix = GetClipPathTransform(aClippedFrame);
  if (!matrix.Invert()) {
    return false;
  }
  gfxPoint point = matrix.Transform(aPoint);

  // clipPath elements can themselves be clipped by a different clip path. In
  // that case the other clip path further clips away the element that is being
  // clipped by the original clipPath. If this clipPath is being clipped by a
  // different clip path we need to check if it prevents the original element
  // from recieving events at aPoint:
  nsSVGClipPathFrame *clipPathFrame =
    nsSVGEffects::GetEffectProperties(this).GetClipPathFrame(nullptr);
  if (clipPathFrame &&
      !clipPathFrame->PointIsInsideClipPath(aClippedFrame, aPoint)) {
    return false;
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      gfxPoint pointForChild = point;
      gfxMatrix m = static_cast<nsSVGElement*>(kid->GetContent())->
        PrependLocalTransformsTo(gfxMatrix(), nsSVGElement::eUserSpaceToParent);
      if (!m.IsIdentity()) {
        if (!m.Invert()) {
          return false;
        }
        pointForChild = m.Transform(point);
      }
      if (SVGFrame->GetFrameForPoint(pointForChild)) {
        return true;
      }
    }
  }
  return false;
}

bool
nsSVGClipPathFrame::IsTrivial(nsISVGChildFrame **aSingleChild)
{
  // If the clip path is clipped then it's non-trivial
  if (nsSVGEffects::GetEffectProperties(this).GetClipPathFrame(nullptr))
    return false;

  if (aSingleChild) {
    *aSingleChild = nullptr;
  }

  nsISVGChildFrame *foundChild = nullptr;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame *svgChild = do_QueryFrame(kid);
    if (svgChild) {
      // We consider a non-trivial clipPath to be one containing
      // either more than one svg child and/or a svg container
      if (foundChild || svgChild->IsDisplayContainer())
        return false;

      // or where the child is itself clipped
      if (nsSVGEffects::GetEffectProperties(kid).GetClipPathFrame(nullptr))
        return false;

      foundChild = svgChild;
    }
  }
  if (aSingleChild) {
    *aSingleChild = foundChild;
  }
  return true;
}

bool
nsSVGClipPathFrame::IsValid()
{
  if (mInUse) {
    NS_WARNING("Clip loop detected!");
    return false;
  }
  AutoClipPathReferencer clipRef(this);

  bool isOK = true;
  nsSVGEffects::GetEffectProperties(this).GetClipPathFrame(&isOK);
  if (!isOK) {
    return false;
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {

    nsIAtom *type = kid->GetType();

    if (type == nsGkAtoms::svgUseFrame) {
      for (nsIFrame* grandKid = kid->GetFirstPrincipalChild(); grandKid;
           grandKid = grandKid->GetNextSibling()) {

        nsIAtom *type = grandKid->GetType();

        if (type != nsGkAtoms::svgPathGeometryFrame &&
            type != nsGkAtoms::svgTextFrame) {
          return false;
        }
      }
      continue;
    }
    if (type != nsGkAtoms::svgPathGeometryFrame &&
        type != nsGkAtoms::svgTextFrame) {
      return false;
    }
  }
  return true;
}

nsresult
nsSVGClipPathFrame::AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::transform) {
      nsSVGEffects::InvalidateDirectRenderingObservers(this);
      nsSVGUtils::NotifyChildrenOfSVGChange(this,
                                            nsISVGChildFrame::TRANSFORM_CHANGED);
    }
    if (aAttribute == nsGkAtoms::clipPathUnits) {
      nsSVGEffects::InvalidateRenderingObservers(this);
    }
  }

  return nsSVGClipPathFrameBase::AttributeChanged(aNameSpaceID,
                                                  aAttribute, aModType);
}

void
nsSVGClipPathFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVG(nsGkAtoms::clipPath),
               "Content is not an SVG clipPath!");

  AddStateBits(NS_STATE_SVG_CLIPPATH_CHILD);
  nsSVGClipPathFrameBase::Init(aContent, aParent, aPrevInFlow);
}

nsIAtom *
nsSVGClipPathFrame::GetType() const
{
  return nsGkAtoms::svgClipPathFrame;
}

gfxMatrix
nsSVGClipPathFrame::GetCanvasTM(uint32_t aFor, nsIFrame* aTransformRoot)
{
  SVGClipPathElement *content = static_cast<SVGClipPathElement*>(mContent);

  gfxMatrix tm =
    content->PrependLocalTransformsTo(mClipParentMatrix ?
                                      *mClipParentMatrix : gfxMatrix());

  return nsSVGUtils::AdjustMatrixForUnits(tm,
                                          &content->mEnumAttributes[SVGClipPathElement::CLIPPATHUNITS],
                                          mClipParent);
}

gfxMatrix
nsSVGClipPathFrame::GetClipPathTransform(nsIFrame* aClippedFrame)
{
  SVGClipPathElement *content = static_cast<SVGClipPathElement*>(mContent);

  gfxMatrix tm = content->PrependLocalTransformsTo(gfxMatrix());

  nsSVGEnum* clipPathUnits =
    &content->mEnumAttributes[SVGClipPathElement::CLIPPATHUNITS];

  return nsSVGUtils::AdjustMatrixForUnits(tm, clipPathUnits, aClippedFrame);
}

SVGBBox
nsSVGClipPathFrame::GetBBoxForClipPathFrame(const SVGBBox &aBBox, 
                                            const gfxMatrix &aMatrix)
{
  nsIContent* node = GetContent()->GetFirstChild();
  SVGBBox unionBBox, tmpBBox;
  for (; node; node = node->GetNextSibling()) {
    nsIFrame *frame = 
      static_cast<nsSVGElement*>(node)->GetPrimaryFrame();
    if (frame) {
      nsISVGChildFrame *svg = do_QueryFrame(frame);
      if (svg) {
        tmpBBox = svg->GetBBoxContribution(mozilla::gfx::ToMatrix(aMatrix), 
                                         nsSVGUtils::eBBoxIncludeFill);
        nsSVGEffects::EffectProperties effectProperties =
                              nsSVGEffects::GetEffectProperties(frame);
        bool isOK = true;
        nsSVGClipPathFrame *clipPathFrame = 
                              effectProperties.GetClipPathFrame(&isOK);
        if (clipPathFrame && isOK) {
          tmpBBox = clipPathFrame->GetBBoxForClipPathFrame(tmpBBox, aMatrix);
        } 
        tmpBBox.Intersect(aBBox);
        unionBBox.UnionEdges(tmpBBox);
      }
    }
  }
  nsSVGEffects::EffectProperties props = 
    nsSVGEffects::GetEffectProperties(this);    
  if (props.mClipPath) {
    bool isOK = true;
    nsSVGClipPathFrame *clipPathFrame = props.GetClipPathFrame(&isOK);
    if (clipPathFrame && isOK) {
      tmpBBox = clipPathFrame->GetBBoxForClipPathFrame(aBBox, aMatrix);                                                       
      unionBBox.Intersect(tmpBBox);
    } else if (!isOK) {
      unionBBox = SVGBBox();
    }
  }
  return unionBBox;
}

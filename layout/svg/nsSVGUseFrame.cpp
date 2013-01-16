/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "nsIAnonymousContentCreator.h"
#include "nsSVGGFrame.h"
#include "mozilla/dom/SVGUseElement.h"
#include "nsContentList.h"

typedef nsSVGGFrame nsSVGUseFrameBase;

using namespace mozilla::dom;

class nsSVGUseFrame : public nsSVGUseFrameBase,
                      public nsIAnonymousContentCreator
{
  friend nsIFrame*
  NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

protected:
  nsSVGUseFrame(nsStyleContext* aContext) :
    nsSVGUseFrameBase(aContext),
    mHasValidDimensions(true)
  {}

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  
  // nsIFrame interface:
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD  AttributeChanged(int32_t         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               int32_t         aModType);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgUseFrame
   */
  virtual nsIAtom* GetType() const;

  virtual bool IsLeaf() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGUse"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  virtual void ReflowSVG();
  virtual void NotifySVGChanged(uint32_t aFlags);

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        uint32_t aFilter);

private:
  bool mHasValidDimensions;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGUseFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGUseFrame)

nsIAtom *
nsSVGUseFrame::GetType() const
{
  return nsGkAtoms::svgUseFrame;
}

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(nsSVGUseFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGUseFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGUseFrame::Init(nsIContent* aContent,
                    nsIFrame* aParent,
                    nsIFrame* aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVG(nsGkAtoms::use),
               "Content is not an SVG use!");

  mHasValidDimensions =
    static_cast<SVGUseElement*>(aContent)->HasValidDimensions();

  return nsSVGUseFrameBase::Init(aContent, aParent, aPrevInFlow);
}

NS_IMETHODIMP
nsSVGUseFrame::AttributeChanged(int32_t         aNameSpaceID,
                                nsIAtom*        aAttribute,
                                int32_t         aModType)
{
  SVGUseElement *useElement = static_cast<SVGUseElement*>(mContent);

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::x ||
        aAttribute == nsGkAtoms::y) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nullptr;
      nsSVGUtils::InvalidateBounds(this, false);
      nsSVGUtils::ScheduleReflowSVG(this);
      nsSVGUtils::NotifyChildrenOfSVGChange(this, TRANSFORM_CHANGED);
    } else if (aAttribute == nsGkAtoms::width ||
               aAttribute == nsGkAtoms::height) {
      bool invalidate = false;
      if (mHasValidDimensions != useElement->HasValidDimensions()) {
        mHasValidDimensions = !mHasValidDimensions;
        invalidate = true;
      }
      if (useElement->OurWidthAndHeightAreUsed()) {
        invalidate = true;
        useElement->SyncWidthOrHeight(aAttribute);
      }
      if (invalidate) {
        nsSVGUtils::InvalidateBounds(this, false);
        nsSVGUtils::ScheduleReflowSVG(this);
      }
    }
  } else if (aNameSpaceID == kNameSpaceID_XLink &&
             aAttribute == nsGkAtoms::href) {
    // we're changing our nature, clear out the clone information
    nsSVGUtils::InvalidateBounds(this, false);
    nsSVGUtils::ScheduleReflowSVG(this);
    useElement->mOriginal = nullptr;
    useElement->UnlinkSource();
    useElement->TriggerReclone();
  }

  return nsSVGUseFrameBase::AttributeChanged(aNameSpaceID,
                                             aAttribute, aModType);
}

void
nsSVGUseFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsRefPtr<SVGUseElement> use = static_cast<SVGUseElement*>(mContent);
  nsSVGUseFrameBase::DestroyFrom(aDestructRoot);
  use->DestroyAnonymousContent();
}

bool
nsSVGUseFrame::IsLeaf() const
{
  return true;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

void
nsSVGUseFrame::ReflowSVG()
{
  // We only handle x/y offset here, since any width/height that is in force is
  // handled by the nsSVGOuterSVGFrame for the anonymous <svg> that will be
  // created for that purpose.
  float x, y;
  static_cast<SVGUseElement*>(mContent)->
    GetAnimatedLengthValues(&x, &y, nullptr);
  mRect.MoveTo(nsLayoutUtils::RoundGfxRectToAppRect(
                 gfxRect(x, y, 0.0, 0.0),
                 PresContext()->AppUnitsPerCSSPixel()).TopLeft());
  nsSVGUseFrameBase::ReflowSVG();
}

void
nsSVGUseFrame::NotifySVGChanged(uint32_t aFlags)
{
  if (aFlags & COORD_CONTEXT_CHANGED &&
      !(aFlags & TRANSFORM_CHANGED)) {
    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y'
    SVGUseElement *use = static_cast<SVGUseElement*>(mContent);
    if (use->mLengthAttributes[SVGUseElement::X].IsPercentage() ||
        use->mLengthAttributes[SVGUseElement::Y].IsPercentage()) {
      aFlags |= TRANSFORM_CHANGED;
      // Ancestor changes can't affect how we render from the perspective of
      // any rendering observers that we may have, so we don't need to
      // invalidate them. We also don't need to invalidate ourself, since our
      // changed ancestor will have invalidated its entire area, which includes
      // our area.
      // For perf reasons we call this before calling NotifySVGChanged() below.
      nsSVGUtils::ScheduleReflowSVG(this);
    }
  }

  // We don't remove the TRANSFORM_CHANGED flag here if we have a viewBox or
  // non-percentage width/height, since if they're set then they are cloned to
  // an anonymous child <svg>, and its nsSVGInnerSVGFrame will do that.

  nsSVGUseFrameBase::NotifySVGChanged(aFlags);
}

//----------------------------------------------------------------------
// nsIAnonymousContentCreator methods:

nsresult
nsSVGUseFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  SVGUseElement *use = static_cast<SVGUseElement*>(mContent);

  nsIContent* clone = use->CreateAnonymousContent();
  if (!clone)
    return NS_ERROR_FAILURE;
  if (!aElements.AppendElement(clone))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

void
nsSVGUseFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        uint32_t aFilter)
{
  SVGUseElement *use = static_cast<SVGUseElement*>(mContent);
  nsIContent* clone = use->GetAnonymousContent();
  aElements.MaybeAppendElement(clone);
}

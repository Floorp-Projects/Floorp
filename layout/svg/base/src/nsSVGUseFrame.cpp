/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "nsIAnonymousContentCreator.h"
#include "nsIDOMSVGUseElement.h"
#include "nsSVGGFrame.h"
#include "nsSVGUseElement.h"

typedef nsSVGGFrame nsSVGUseFrameBase;

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

  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

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
  virtual void UpdateBounds();
  virtual void NotifySVGChanged(PRUint32 aFlags);

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        PRUint32 aFilter);

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
#ifdef DEBUG
  nsCOMPtr<nsIDOMSVGUseElement> use = do_QueryInterface(aContent);
  NS_ASSERTION(use, "Content is not an SVG use!");
#endif /* DEBUG */

  mHasValidDimensions =
    static_cast<nsSVGUseElement*>(aContent)->HasValidDimensions();

  return nsSVGUseFrameBase::Init(aContent, aParent, aPrevInFlow);
}

NS_IMETHODIMP
nsSVGUseFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                nsIAtom*        aAttribute,
                                PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::x ||
        aAttribute == nsGkAtoms::y) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nsnull;
    
      nsSVGUtils::NotifyChildrenOfSVGChange(this, TRANSFORM_CHANGED);
    } else if (aAttribute == nsGkAtoms::width ||
               aAttribute == nsGkAtoms::height) {
      static_cast<nsSVGUseElement*>(mContent)->SyncWidthOrHeight(aAttribute);

      if (mHasValidDimensions != 
          static_cast<nsSVGUseElement*>(mContent)->HasValidDimensions()) {

        mHasValidDimensions = !mHasValidDimensions;
        nsSVGUtils::InvalidateAndScheduleBoundsUpdate(this);
      }
    }
  } else if (aNameSpaceID == kNameSpaceID_XLink &&
             aAttribute == nsGkAtoms::href) {
    // we're changing our nature, clear out the clone information
    nsSVGUseElement *use = static_cast<nsSVGUseElement*>(mContent);
    use->mOriginal = nsnull;
    use->UnlinkSource();
    use->TriggerReclone();
  }

  return nsSVGUseFrameBase::AttributeChanged(aNameSpaceID,
                                             aAttribute, aModType);
}

void
nsSVGUseFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsRefPtr<nsSVGUseElement> use = static_cast<nsSVGUseElement*>(mContent);
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
nsSVGUseFrame::UpdateBounds()
{
  // We only handle x/y offset here, since any width/height that is in force is
  // handled by the nsSVGOuterSVGFrame for the anonymous <svg> that will be
  // created for that purpose.
  float x, y;
  static_cast<nsSVGUseElement*>(mContent)->
    GetAnimatedLengthValues(&x, &y, nsnull);
  mRect.MoveTo(nsLayoutUtils::RoundGfxRectToAppRect(
                 gfxRect(x, y, 0.0, 0.0),
                 PresContext()->AppUnitsPerCSSPixel()).TopLeft());
  nsSVGUseFrameBase::UpdateBounds();
}

void
nsSVGUseFrame::NotifySVGChanged(PRUint32 aFlags)
{
  if (aFlags & COORD_CONTEXT_CHANGED &&
      !(aFlags & TRANSFORM_CHANGED)) {
    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y'
    nsSVGUseElement *use = static_cast<nsSVGUseElement*>(mContent);
    if (use->mLengthAttributes[nsSVGUseElement::X].IsPercentage() ||
        use->mLengthAttributes[nsSVGUseElement::Y].IsPercentage()) {
      aFlags |= TRANSFORM_CHANGED;
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
  nsSVGUseElement *use = static_cast<nsSVGUseElement*>(mContent);

  nsIContent* clone = use->CreateAnonymousContent();
  if (!clone)
    return NS_ERROR_FAILURE;
  if (!aElements.AppendElement(clone))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

void
nsSVGUseFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        PRUint32 aFilter)
{
  nsSVGUseElement *use = static_cast<nsSVGUseElement*>(mContent);
  nsIContent* clone = use->GetAnonymousContent();
  aElements.MaybeAppendElement(clone);
}

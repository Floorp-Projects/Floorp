/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
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

// Keep in (case-insensitive) order:
#include "nsFrame.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGStopElement.h"
#include "nsStyleContext.h"
#include "nsSVGEffects.h"

// This is a very simple frame whose only purpose is to capture style change
// events and propagate them to the parent.  Most of the heavy lifting is done
// within the nsSVGGradientFrame, which is the parent for this frame

typedef nsFrame  nsSVGStopFrameBase;

class nsSVGStopFrame : public nsSVGStopFrameBase
{
  friend nsIFrame*
  NS_NewSVGStopFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGStopFrame(nsStyleContext* aContext) : nsSVGStopFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame interface:
#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgStopFrame
   */
  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsSVGStopFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGStop"), aResult);
  }
#endif
};

//----------------------------------------------------------------------
// Implementation

NS_IMPL_FRAMEARENA_HELPERS(nsSVGStopFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

#ifdef DEBUG
NS_IMETHODIMP
nsSVGStopFrame::Init(nsIContent* aContent,
                     nsIFrame* aParent,
                     nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGStopElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "Content doesn't support nsIDOMSVGStopElement");

  return nsSVGStopFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

/* virtual */ void
nsSVGStopFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsSVGStopFrameBase::DidSetStyleContext(aOldStyleContext);
  nsSVGEffects::InvalidateRenderingObservers(this);
}

nsIAtom *
nsSVGStopFrame::GetType() const
{
  return nsGkAtoms::svgStopFrame;
}

NS_IMETHODIMP
nsSVGStopFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::offset) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  }

  return nsSVGStopFrameBase::AttributeChanged(aNameSpaceID,
                                              aAttribute, aModType);
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* NS_NewSVGStopFrame(nsIPresShell*   aPresShell,
                             nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGStopFrame(aContext);
}

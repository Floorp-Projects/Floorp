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
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_SVGOUTERSVGFRAME_H__
#define __NS_SVGOUTERSVGFRAME_H__

#include "nsSVGContainerFrame.h"
#include "nsISVGSVGFrame.h"
#include "nsISVGEnum.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGNumber.h"

////////////////////////////////////////////////////////////////////////
// nsSVGOuterSVGFrame class

typedef nsSVGDisplayContainerFrame nsSVGOuterSVGFrameBase;

class nsSVGOuterSVGFrame : public nsSVGOuterSVGFrameBase,
                           public nsISVGSVGFrame
{
  friend nsIFrame*
  NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
protected:
  nsSVGOuterSVGFrame(nsStyleContext* aContext);
  NS_IMETHOD InitSVG();

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return 1; }
  NS_IMETHOD_(nsrefcnt) Release() { return 1; }

public:
  // nsIFrame:
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  DidReflow(nsPresContext*   aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus aStatus);

  NS_IMETHOD  InsertFrames(nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  nsIFrame* GetFrameForPoint(const nsPoint& aPoint);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgOuterSVGFrame
   */
  virtual nsIAtom* GetType() const;

  void Paint(nsIRenderingContext& aRenderingContext,
             const nsRect& aDirtyRect, nsPoint aPt);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGOuterSVG"), aResult);
  }
#endif

  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  // nsSVGOuterSVGFrame methods:

  /* Invalidate takes a nsRect in screen pixel coordinates */
  nsresult InvalidateRect(nsRect aRect);
  PRBool IsRedrawSuspended();

  // nsISVGSVGFrame interface:
  NS_IMETHOD SuspendRedraw();
  NS_IMETHOD UnsuspendRedraw();
  NS_IMETHOD NotifyViewportChange();

  // nsSVGContainerFrame methods:
  virtual already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();

protected:
  // implementation helpers:
  void InitiateReflow();

  void CalculateAvailableSpace(nsRect *maxRect, nsRect *preferredRect,
                               nsPresContext* aPresContext,
                               const nsHTMLReflowState& aReflowState);

  PRUint32 mRedrawSuspendCount;
  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;

  // zoom and pan
  nsCOMPtr<nsISVGEnum>      mZoomAndPan;
  nsCOMPtr<nsIDOMSVGPoint>  mCurrentTranslate;
  nsCOMPtr<nsIDOMSVGNumber> mCurrentScale;

  PRPackedBool mNeedsReflow;
  PRPackedBool mViewportInitialized;
};

#endif

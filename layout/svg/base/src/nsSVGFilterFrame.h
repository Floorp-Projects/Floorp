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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __NS_SVGFILTERFRAME_H__
#define __NS_SVGFILTERFRAME_H__

#include "nsFrame.h"
#include "nsQueryFrame.h"
#include "nsRect.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class nsIAtom;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsRenderingContext;
class nsStyleContext;
class nsSVGFilterPaintCallback;
class nsSVGFilterElement;
class nsSVGIntegerPair;
class nsSVGLength2;

typedef nsSVGContainerFrame nsSVGFilterFrameBase;

class nsSVGFilterFrame : public nsSVGFilterFrameBase
{
  friend nsIFrame*
  NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGFilterFrame(nsStyleContext* aContext)
    : nsSVGFilterFrameBase(aContext),
      mLoopFlag(false),
      mNoHRefURI(false)
  {
    AddStateBits(NS_STATE_SVG_NONDISPLAY_CHILD);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  nsresult FilterPaint(nsRenderingContext *aContext,
                       nsIFrame *aTarget, nsSVGFilterPaintCallback *aPaintCallback,
                       const nsIntRect* aDirtyRect);

  /**
   * Returns the area that could change when the given rect of the source changes.
   * The rectangles are relative to the origin of the outer svg, if aTarget is SVG,
   * relative to aTarget itself otherwise, in device pixels.
   */
  nsIntRect GetInvalidationBBox(nsIFrame *aTarget, const nsIntRect& aRect);

  /**
   * Returns the area in device pixels that is needed from the source when
   * the given area needs to be repainted.
   * The rectangles are relative to the origin of the outer svg, if aTarget is SVG,
   * relative to aTarget itself otherwise, in device pixels.
   */
  nsIntRect GetSourceForInvalidArea(nsIFrame *aTarget, const nsIntRect& aRect);

  /**
   * Returns the bounding box of the post-filter area of aTarget.
   * The rectangles are relative to the origin of the outer svg, if aTarget is SVG,
   * relative to aTarget itself otherwise, in device pixels.
   * @param aSourceBBox overrides the normal bbox for the source, if non-null
   */
  nsIntRect GetFilterBBox(nsIFrame *aTarget, const nsIntRect *aSourceBBox);

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFilterFrame
   */
  virtual nsIAtom* GetType() const;

private:
  // Parse our xlink:href and set up our nsSVGPaintingProperty if we
  // reference another filter and we don't have a property. Return
  // the referenced filter's frame if available, null otherwise.
  class AutoFilterReferencer;
  friend class nsAutoFilterInstance;
  nsSVGFilterFrame* GetReferencedFilter();
  nsSVGFilterFrame* GetReferencedFilterIfNotInUse();

  // Accessors to lookup filter attributes
  PRUint16 GetEnumValue(PRUint32 aIndex, nsIContent *aDefault);
  PRUint16 GetEnumValue(PRUint32 aIndex)
  {
    return GetEnumValue(aIndex, mContent);
  }
  const nsSVGIntegerPair *GetIntegerPairValue(PRUint32 aIndex, nsIContent *aDefault);
  const nsSVGIntegerPair *GetIntegerPairValue(PRUint32 aIndex)
  {
    return GetIntegerPairValue(aIndex, mContent);
  }
  const nsSVGLength2 *GetLengthValue(PRUint32 aIndex, nsIContent *aDefault);
  const nsSVGLength2 *GetLengthValue(PRUint32 aIndex)
  {
    return GetLengthValue(aIndex, mContent);
  }
  const nsSVGFilterElement *GetFilterContent(nsIContent *aDefault);
  const nsSVGFilterElement *GetFilterContent()
  {
    return GetFilterContent(mContent);
  }

  // This flag is used to detect loops in xlink:href processing
  bool                              mLoopFlag;
  bool                              mNoHRefURI;
};

#endif

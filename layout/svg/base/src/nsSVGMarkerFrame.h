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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef __NS_SVGMARKERFRAME_H__
#define __NS_SVGMARKERFRAME_H__

#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class nsIAtom;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsRenderingContext;
class nsStyleContext;
class nsSVGPathGeometryFrame;
class nsSVGSVGElement;

struct nsSVGMark;

typedef nsSVGContainerFrame nsSVGMarkerFrameBase;

class nsSVGMarkerFrame : public nsSVGMarkerFrameBase
{
  friend nsIFrame*
  NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGMarkerFrame(nsStyleContext* aContext)
    : nsSVGMarkerFrameBase(aContext)
    , mMarkedFrame(nsnull)
    , mInUse(false)
    , mInUse2(false)
  {
    AddStateBits(NS_STATE_SVG_NONDISPLAY_CHILD);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame interface:
#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);
  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgMarkerFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGMarker"), aResult);
  }
#endif

  // nsSVGMarkerFrame methods:
  nsresult PaintMark(nsRenderingContext *aContext,
                     nsSVGPathGeometryFrame *aMarkedFrame,
                     nsSVGMark *aMark,
                     float aStrokeWidth);

  SVGBBox GetMarkBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                  PRUint32 aFlags,
                                  nsSVGPathGeometryFrame *aMarkedFrame,
                                  const nsSVGMark *aMark,
                                  float aStrokeWidth);

private:
  // stuff needed for callback
  nsSVGPathGeometryFrame *mMarkedFrame;
  float mStrokeWidth, mX, mY, mAutoAngle;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM();

  // A helper class to allow us to paint markers safely. The helper
  // automatically sets and clears the mInUse flag on the marker frame (to
  // prevent nasty reference loops) as well as the reference to the marked
  // frame and its coordinate context. It's easy to mess this up
  // and break things, so this helper makes the code far more robust.
  class AutoMarkerReferencer
  {
  public:
    AutoMarkerReferencer(nsSVGMarkerFrame *aFrame,
                         nsSVGPathGeometryFrame *aMarkedFrame);
    ~AutoMarkerReferencer();
  private:
    nsSVGMarkerFrame *mFrame;
  };

  // nsSVGMarkerFrame methods:
  void SetParentCoordCtxProvider(nsSVGSVGElement *aContext);

  // recursion prevention flag
  bool mInUse;

  // second recursion prevention flag, for GetCanvasTM()
  bool mInUse2;
};

#endif

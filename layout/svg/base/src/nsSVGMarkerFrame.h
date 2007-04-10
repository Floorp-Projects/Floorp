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

#include "nsSVGContainerFrame.h"

class gfxContext;
class nsSVGPathGeometryFrame;
class nsIURI;
class nsIContent;
struct nsSVGMark;

typedef nsSVGContainerFrame nsSVGMarkerFrameBase;

class nsSVGMarkerFrame : public nsSVGMarkerFrameBase
{
protected:
  friend nsIFrame*
  NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

  NS_IMETHOD InitSVG();

public:
  nsSVGMarkerFrame(nsStyleContext* aContext) : nsSVGMarkerFrameBase(aContext) {}

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
  nsresult PaintMark(nsSVGRenderState *aContext,
                     nsSVGPathGeometryFrame *aMarkedFrame,
                     nsSVGMark *aMark,
                     float aStrokeWidth);

  nsRect RegionMark(nsSVGPathGeometryFrame *aMarkedFrame,
                    const nsSVGMark *aMark, float aStrokeWidth);

private:
  // stuff needed for callback
  nsSVGPathGeometryFrame *mMarkedFrame;
  float mStrokeWidth, mX, mY, mAngle;

  // nsSVGContainerFrame methods:
  virtual already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();

  // VC6 does not allow the inner class to access protected members
  // of the outer class
  class AutoMarkerReferencer;
  friend class AutoMarkerReferencer;

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
  PRPackedBool mInUse;

  // second recursion prevention flag, for GetCanvasTM()
  PRPackedBool mInUse2;
};

nsIContent *
NS_GetSVGMarkerElement(nsIURI *aURI, nsIContent *aContent);

#endif

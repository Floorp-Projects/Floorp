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
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#ifndef __NS_SVGPATHGEOMETRYFRAME_H__
#define __NS_SVGPATHGEOMETRYFRAME_H__

#include "nsFrame.h"
#include "nsISVGChildFrame.h"
#include "nsSVGUtils.h"
#include "nsGkAtoms.h"
#include "nsSVGGeometryFrame.h"

class nsRenderingContext;
class nsSVGMarkerFrame;
class nsSVGMarkerProperty;

typedef nsSVGGeometryFrame nsSVGPathGeometryFrameBase;

class nsSVGPathGeometryFrame : public nsSVGPathGeometryFrameBase,
                               public nsISVGChildFrame
{
  friend nsIFrame*
  NS_NewSVGPathGeometryFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGPathGeometryFrame(nsStyleContext* aContext) :
    nsSVGPathGeometryFrameBase(aContext) {}

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgPathGeometryFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPathGeometry"), aResult);
  }
#endif

  // nsSVGGeometryFrame methods
  gfxMatrix GetCanvasTM();

protected:
  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsRenderingContext *aContext,
                      const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint);
  NS_IMETHOD_(nsRect) GetCoveredRegion();
  NS_IMETHOD UpdateCoveredRegion();
  NS_IMETHOD InitialUpdate();
  virtual void NotifySVGChanged(PRUint32 aFlags);
  virtual gfxRect GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      PRUint32 aFlags);
  NS_IMETHOD_(bool) IsDisplayContainer() { return false; }
  NS_IMETHOD_(bool) HasValidCoveredRect() {
    return !(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD);
  }

protected:
  void GeneratePath(gfxContext *aContext,
                    const gfxMatrix *aOverrideTransform = nsnull);

private:
  void Render(nsRenderingContext *aContext);

  struct MarkerProperties {
    nsSVGMarkerProperty* mMarkerStart;
    nsSVGMarkerProperty* mMarkerMid;
    nsSVGMarkerProperty* mMarkerEnd;

    bool MarkersExist() const {
      return mMarkerStart || mMarkerMid || mMarkerEnd;
    }

    nsSVGMarkerFrame *GetMarkerStartFrame();
    nsSVGMarkerFrame *GetMarkerMidFrame();
    nsSVGMarkerFrame *GetMarkerEndFrame();
  };

  /**
   * @param aFrame should be the first continuation
   */
  static MarkerProperties GetMarkerProperties(nsSVGPathGeometryFrame *aFrame);
};

#endif // __NS_SVGPATHGEOMETRYFRAME_H__

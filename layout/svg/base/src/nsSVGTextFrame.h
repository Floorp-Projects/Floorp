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

#ifndef NS_SVGTEXTFRAME_H
#define NS_SVGTEXTFRAME_H

#include "nsSVGTextContainerFrame.h"
#include "gfxRect.h"
#include "gfxMatrix.h"

class nsRenderingContext;

typedef nsSVGTextContainerFrame nsSVGTextFrameBase;

class nsSVGTextFrame : public nsSVGTextFrameBase
{
  friend nsIFrame*
  NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGTextFrame(nsStyleContext* aContext)
    : nsSVGTextFrameBase(aContext),
      mPositioningDirty(true) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgTextFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGText"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  virtual void NotifySVGChanged(PRUint32 aFlags);
  // Override these four to ensure that UpdateGlyphPositioning is called
  // to bring glyph positions up to date
  NS_IMETHOD PaintSVG(nsRenderingContext* aContext,
                      const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint & aPoint);
  virtual void UpdateBounds();
  virtual gfxRect GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      PRUint32 aFlags);
  
  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM();
  
  // nsSVGTextContainerFrame methods:
  virtual PRUint32 GetNumberOfChars();
  virtual float GetComputedTextLength();
  virtual float GetSubStringLength(PRUint32 charnum, PRUint32 nchars);
  virtual PRInt32 GetCharNumAtPosition(nsIDOMSVGPoint *point);

  NS_IMETHOD GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval);
  NS_IMETHOD GetRotationOfChar(PRUint32 charnum, float *_retval);

  // nsSVGTextFrame
  void NotifyGlyphMetricsChange();

private:
  /**
   * @param aForceGlobalTransform passed down to nsSVGGlyphFrames to
   * control whether they should use the global transform even when
   * NS_STATE_NONDISPLAY_CHILD
   */
  void UpdateGlyphPositioning(bool aForceGlobalTransform);

  void SetWhitespaceHandling(nsSVGGlyphFrame *aFrame);

  nsAutoPtr<gfxMatrix> mCanvasTM;

  bool mPositioningDirty;
};

#endif

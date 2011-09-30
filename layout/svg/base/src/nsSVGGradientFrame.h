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

#ifndef __NS_SVGGRADIENTFRAME_H__
#define __NS_SVGGRADIENTFRAME_H__

#include "nsSVGPaintServerFrame.h"
#include "nsWeakReference.h"
#include "nsSVGElement.h"
#include "gfxPattern.h"

class nsIDOMSVGStopElement;

typedef nsSVGPaintServerFrame nsSVGGradientFrameBase;

/**
 * Gradients can refer to other gradients. We create an nsSVGPaintingProperty
 * with property type nsGkAtoms::href to track the referenced gradient.
 */
class nsSVGGradientFrame : public nsSVGGradientFrameBase
{
protected:
  nsSVGGradientFrame(nsStyleContext* aContext);

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsSVGPaintServerFrame methods:
  virtual already_AddRefed<gfxPattern>
    GetPaintServerPattern(nsIFrame *aSource,
                          float aGraphicOpacity,
                          const gfxRect *aOverrideBounds);

  // nsIFrame interface:
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGradient"), aResult);
  }
#endif // DEBUG

private:

  // Parse our xlink:href and set up our nsSVGPaintingProperty if we
  // reference another gradient and we don't have a property. Return
  // the referenced gradient's frame if available, null otherwise.
  nsSVGGradientFrame* GetReferencedGradient();

  // Helpers to look at our gradient and then along its reference chain (if any)
  // to find the first gradient with the specified attribute.
  // Returns aDefault if no content with that attribute is found
  nsSVGGradientElement* GetGradientWithAttr(nsIAtom *aAttrName, nsIContent *aDefault);

  // Some attributes are only valid on one type of gradient, and we *must* get
  // the right type or we won't have the data structures we require.
  // Returns aDefault if no content with that attribute is found
  nsSVGGradientElement* GetGradientWithAttr(nsIAtom *aAttrName, nsIAtom *aGradType,
                                            nsIContent *aDefault);

  // Optionally get a stop frame (returns stop index/count)
  PRInt32 GetStopFrame(PRInt32 aIndex, nsIFrame * *aStopFrame);

  PRUint16 GetSpreadMethod();
  PRUint32 GetStopCount();
  void GetStopInformation(PRInt32 aIndex,
                          float *aOffset, nscolor *aColor, float *aStopOpacity);

  // Will be singular for gradientUnits="objectBoundingBox" with an empty bbox.
  gfxMatrix GetGradientTransform(nsIFrame *aSource, const gfxRect *aOverrideBounds);

protected:
  virtual already_AddRefed<gfxPattern> CreateGradient() = 0;

  // Use these inline methods instead of GetGradientWithAttr(..., aGradType)
  nsSVGLinearGradientElement* GetLinearGradientWithAttr(nsIAtom *aAttrName, nsIContent *aDefault)
  {
    return static_cast<nsSVGLinearGradientElement*>(
            GetGradientWithAttr(aAttrName, nsGkAtoms::svgLinearGradientFrame, aDefault));
  }
  nsSVGRadialGradientElement* GetRadialGradientWithAttr(nsIAtom *aAttrName, nsIContent *aDefault)
  {
    return static_cast<nsSVGRadialGradientElement*>(
            GetGradientWithAttr(aAttrName, nsGkAtoms::svgRadialGradientFrame, aDefault));
  }

  // Get the value of our gradientUnits attribute
  PRUint16 GetGradientUnits();

  // The frame our gradient is (currently) being applied to
  nsIFrame*                              mSource;

private:
  // Flag to mark this frame as "in use" during recursive calls along our
  // gradient's reference chain so we can detect reference loops. See:
  // http://www.w3.org/TR/SVG11/pservers.html#LinearGradientElementHrefAttribute
  bool                                   mLoopFlag;
  // Gradients often don't reference other gradients, so here we cache
  // the fact that that isn't happening.
  bool                                   mNoHRefURI;
};


// -------------------------------------------------------------------------
// Linear Gradients
// -------------------------------------------------------------------------

typedef nsSVGGradientFrame nsSVGLinearGradientFrameBase;

class nsSVGLinearGradientFrame : public nsSVGLinearGradientFrameBase
{
  friend nsIFrame* NS_NewSVGLinearGradientFrame(nsIPresShell* aPresShell,
                                                nsStyleContext* aContext);
protected:
  nsSVGLinearGradientFrame(nsStyleContext* aContext) :
    nsSVGLinearGradientFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame interface:
#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  virtual nsIAtom* GetType() const;  // frame type: nsGkAtoms::svgLinearGradientFrame

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGLinearGradient"), aResult);
  }
#endif // DEBUG

protected:
  float GradientLookupAttribute(nsIAtom *aAtomName, PRUint16 aEnumName);
  virtual already_AddRefed<gfxPattern> CreateGradient();
};

// -------------------------------------------------------------------------
// Radial Gradients
// -------------------------------------------------------------------------

typedef nsSVGGradientFrame nsSVGRadialGradientFrameBase;

class nsSVGRadialGradientFrame : public nsSVGRadialGradientFrameBase
{
  friend nsIFrame* NS_NewSVGRadialGradientFrame(nsIPresShell* aPresShell,
                                                nsStyleContext* aContext);
protected:
  nsSVGRadialGradientFrame(nsStyleContext* aContext) :
    nsSVGRadialGradientFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame interface:
#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  virtual nsIAtom* GetType() const;  // frame type: nsGkAtoms::svgRadialGradientFrame

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGRadialGradient"), aResult);
  }
#endif // DEBUG

protected:
  float GradientLookupAttribute(nsIAtom *aAtomName, PRUint16 aEnumName,
                                nsSVGRadialGradientElement *aElement = nsnull);
  virtual already_AddRefed<gfxPattern> CreateGradient();
};

#endif // __NS_SVGGRADIENTFRAME_H__


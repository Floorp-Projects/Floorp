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
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsSVGElement.h"
#include "cairo.h"

class nsIDOMSVGStopElement;

typedef nsSVGPaintServerFrame  nsSVGGradientFrameBase;

class nsSVGGradientFrame : public nsSVGGradientFrameBase,
                           public nsISVGValueObserver
{
public:
  // nsSVGPaintServerFrame methods:
  virtual PRBool SetupPaintServer(gfxContext *aContext,
                                  nsSVGGeometryFrame *aSource,
                                  float aOpacity,
                                  void **aClosure);
  virtual void CleanupPaintServer(gfxContext *aContext, void *aClosure);

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return 1; }
  NS_IMETHOD_(nsrefcnt) Release() { return 1; }

public:
  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable, 
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable, 
                                    nsISVGValue::modificationType aModType);

  // nsIFrame interface:
  NS_IMETHOD DidSetStyleContext();
  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  virtual nsIAtom* GetType() const;  // frame type: nsGkAtoms::svgGradientFrame

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  // nsIFrameDebug interface:
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGradient"), aResult);
  }
#endif // DEBUG

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(gfxContext* aContext)
  {
    return NS_OK;  // override - our frames don't directly render
  }
  
private:

  // Helper methods to aid gradient implementation
  // ---------------------------------------------
  // The SVG specification allows gradient elements to reference another
  // gradient element to "inherit" its attributes or gradient stops. Reference
  // chains of arbitrary length are allowed, and loop checking is essential!
  // Use the following helpers to safely get attributes and stops.

  // Parse our xlink:href and set mNextGrad if we reference another gradient.
  void GetRefedGradientFromHref();

  // Helpers to look at our gradient and then along its reference chain (if any)
  // to find the first gradient with the specified attribute.
  nsIContent* GetGradientWithAttr(nsIAtom *aAttrName);

  // Some attributes are only valid on one type of gradient, and we *must* get
  // the right type or we won't have the data structures we require.
  nsIContent* GetGradientWithAttr(nsIAtom *aAttrName, nsIAtom *aGradType);

  // Optionally get a stop frame (returns stop index/count)
  PRInt32 GetStopFrame(PRInt32 aIndex, nsIFrame * *aStopFrame);

  PRUint16 GetSpreadMethod();
  PRUint32 GetStopCount();
  void GetStopInformation(PRInt32 aIndex,
                          float *aOffset, nscolor *aColor, float *aOpacity);
  nsresult GetGradientTransform(nsIDOMSVGMatrix **retval,
                                nsSVGGeometryFrame *aSource);

protected:

  virtual cairo_pattern_t *CreateGradient() = 0;

  // Use these inline methods instead of GetGradientWithAttr(..., aGradType)
  nsIContent* GetLinearGradientWithAttr(nsIAtom *aAttrName)
  {
    return GetGradientWithAttr(aAttrName, nsGkAtoms::svgLinearGradientFrame);
  }
  nsIContent* GetRadialGradientWithAttr(nsIAtom *aAttrName)
  {
    return GetGradientWithAttr(aAttrName, nsGkAtoms::svgRadialGradientFrame);
  }

  // We must loop check notifications too: see bug 330387 comment 18 + testcase
  // and comment 19. The mLoopFlag check is in Will/DidModifySVGObservable.
  void WillModify(modificationType aModType = mod_other)
  {
    mLoopFlag = PR_TRUE;
    nsSVGValue::WillModify(aModType);
    mLoopFlag = PR_FALSE;
  }
  void DidModify(modificationType aModType = mod_other)
  {
    mLoopFlag = PR_TRUE;
    nsSVGValue::DidModify(aModType);
    mLoopFlag = PR_FALSE;
  }

  // Get the value of our gradientUnits attribute
  PRUint16 GetGradientUnits();

  nsSVGGradientFrame(nsStyleContext* aContext,
                     nsIDOMSVGURIReference *aRef);

  virtual ~nsSVGGradientFrame();

  // The graphic element our gradient is (currently) being applied to
  nsRefPtr<nsSVGElement>                 mSourceContent;

private:

  // href of the other gradient we reference (if any)
  nsCOMPtr<nsIDOMSVGAnimatedString>      mHref;

  // Frame of the gradient we reference (if any). Do NOT use this directly.
  // Use Get[Xxx]GradientWithAttr instead to ensure proper loop checking.
  nsSVGGradientFrame                    *mNextGrad;

  // Flag to mark this frame as "in use" during recursive calls along our
  // gradient's reference chain so we can detect reference loops. See:
  // http://www.w3.org/TR/SVG11/pservers.html#LinearGradientElementHrefAttribute
  PRPackedBool                           mLoopFlag;

  // Ideally we'd set mNextGrad by implementing Init(), but the frame of the
  // gradient we reference isn't available at that stage. Our only option is to
  // set mNextGrad lazily in GetGradientWithAttr, and to make that efficient
  // we need this flag. Our class size is the same since it just fills padding.
  PRPackedBool                           mInitialized;
};


// -------------------------------------------------------------------------
// Linear Gradients
// -------------------------------------------------------------------------

typedef nsSVGGradientFrame nsSVGLinearGradientFrameBase;

class nsSVGLinearGradientFrame : public nsSVGLinearGradientFrameBase
{
public:
  friend nsIFrame* NS_NewSVGLinearGradientFrame(nsIPresShell* aPresShell, 
                                                nsIContent*   aContent,
                                                nsStyleContext* aContext);

  // nsIFrame interface:
  virtual nsIAtom* GetType() const;  // frame type: nsGkAtoms::svgLinearGradientFrame

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  // nsIFrameDebug interface:
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGLinearGradient"), aResult);
  }
#endif // DEBUG

protected:
  nsSVGLinearGradientFrame(nsStyleContext* aContext,
                           nsIDOMSVGURIReference *aRef) :
    nsSVGLinearGradientFrameBase(aContext, aRef) {}

  float GradientLookupAttribute(nsIAtom *aAtomName, PRUint16 aEnumName);
  virtual cairo_pattern_t *CreateGradient();
};

// -------------------------------------------------------------------------
// Radial Gradients
// -------------------------------------------------------------------------

typedef nsSVGGradientFrame nsSVGRadialGradientFrameBase;

class nsSVGRadialGradientFrame : public nsSVGRadialGradientFrameBase
{
public:
  friend nsIFrame* NS_NewSVGRadialGradientFrame(nsIPresShell* aPresShell, 
                                                nsIContent*   aContent,
                                                nsStyleContext* aContext);

  // nsIFrame interface:
  virtual nsIAtom* GetType() const;  // frame type: nsGkAtoms::svgRadialGradientFrame

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  // nsIFrameDebug interface:
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGRadialGradient"), aResult);
  }
#endif // DEBUG

protected:
  nsSVGRadialGradientFrame(nsStyleContext* aContext,
                           nsIDOMSVGURIReference *aRef) :
    nsSVGRadialGradientFrameBase(aContext, aRef) {}

  float GradientLookupAttribute(nsIAtom *aAtomName, PRUint16 aEnumName,
                                nsIContent *aElement = nsnull);
  virtual cairo_pattern_t *CreateGradient();
};

#endif // __NS_SVGGRADIENTFRAME_H__


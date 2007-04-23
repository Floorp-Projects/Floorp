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
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef __NS_SVGPATTERNFRAME_H__
#define __NS_SVGPATTERNFRAME_H__

#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGPaintServerFrame.h"

class nsIDOMSVGAnimatedPreserveAspectRatio;
class nsIFrame;
class nsSVGLength2;
class nsSVGElement;
class gfxContext;

typedef nsSVGPaintServerFrame  nsSVGPatternFrameBase;

class nsSVGPatternFrame : public nsSVGPatternFrameBase,
                          public nsISVGValueObserver
{
public:
  friend nsIFrame* NS_NewSVGPatternFrame(nsIPresShell* aPresShell, 
                                         nsIContent*   aContent,
                                         nsStyleContext* aContext);

  nsSVGPatternFrame(nsStyleContext* aContext) : nsSVGPatternFrameBase(aContext) {}

  nsresult PaintPattern(cairo_surface_t **surface,
                        nsIDOMSVGMatrix **patternMatrix,
                        nsSVGGeometryFrame *aSource);

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
  
  // nsSVGContainerFrame methods:
  virtual already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();

  // nsIFrame interface:
  NS_IMETHOD DidSetStyleContext();

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgPatternFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  // nsIFrameDebug interface:
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPattern"), aResult);
  }
#endif // DEBUG

protected:
  nsSVGPatternFrame(nsStyleContext* aContext,
                    nsIDOMSVGURIReference *aRef);

  virtual ~nsSVGPatternFrame();

  // Internal methods for handling referenced patterns
  PRBool checkURITarget(nsIAtom *);
  PRBool checkURITarget();
  //
  nsSVGLength2 *GetX();
  nsSVGLength2 *GetY();
  nsSVGLength2 *GetWidth();
  nsSVGLength2 *GetHeight();

  PRUint16 GetPatternUnits();
  PRUint16 GetPatternContentUnits();
  nsresult GetPatternTransform(nsIDOMSVGMatrix **retval);

  NS_IMETHOD GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio 
                                                     **aPreserveAspectRatio);
  NS_IMETHOD GetPatternFirstChild(nsIFrame **kid);
  NS_IMETHOD GetViewBox(nsIDOMSVGRect * *aMatrix);
  nsresult   GetPatternRect(nsIDOMSVGRect **patternRect, nsIDOMSVGRect *bbox, 
                            nsSVGElement *content);
  nsresult   GetPatternMatrix(nsIDOMSVGMatrix **aCTM, 
                              nsIDOMSVGRect *bbox,
                              nsIDOMSVGRect *callerBBox,
                              nsIDOMSVGMatrix *callerCTM);
  nsresult   ConstructCTM(nsIDOMSVGMatrix **ctm, nsIDOMSVGRect *callerBBox);
  nsresult   GetCallerGeometry(nsIDOMSVGMatrix **aCTM, 
                               nsIDOMSVGRect **aBBox,
                               nsSVGElement **aContent, 
                               nsSVGGeometryFrame *aSource);

private:
  // this is a *temporary* reference to the frame of the element currently
  // referencing our pattern.  This must be temporary because different
  // referencing frames will all reference this one frame
  nsSVGGeometryFrame                     *mSource;
  nsCOMPtr<nsIDOMSVGMatrix>               mCTM;

protected:
  nsSVGPatternFrame                      *mNextPattern;
  nsCOMPtr<nsIDOMSVGAnimatedString> 	  mHref;
  PRPackedBool                            mLoopFlag;
};

#endif


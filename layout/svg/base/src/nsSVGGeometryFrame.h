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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef __NS_SVGGEOMETRYFRAME_H__
#define __NS_SVGGEOMETRYFRAME_H__

#include "nsFrame.h"
#include "nsWeakReference.h"
#include "nsISVGValueObserver.h"

class nsSVGPaintServerFrame;
class gfxContext;

typedef nsFrame nsSVGGeometryFrameBase;

/* nsSVGGeometryFrame is a base class for SVG objects that directly
 * have geometry (circle, ellipse, line, polyline, polygon, path, and
 * glyph frames).  It knows how to convert the style information into
 * cairo context information and stores the fill/stroke paint
 * servers. */

class nsSVGGeometryFrame : public nsSVGGeometryFrameBase,
                           public nsISVGValueObserver
{
public:
  nsSVGGeometryFrame(nsStyleContext *aContext);
  virtual void Destroy();

  // nsIFrame interface:
  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame* aParent,
                  nsIFrame* aPrevInFlow);
  NS_IMETHOD DidSetStyleContext();

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable,
                                    nsISVGValue::modificationType aModType);

  // nsSVGGeometryFrame methods:
  NS_IMETHOD GetCanvasTM(nsIDOMSVGMatrix * *aCanvasTM) = 0;
  PRUint16 GetClipRule();
  PRBool IsClipChild(); 

  float GetStrokeWidth();

  // Check if this geometry needs to be filled or stroked.  These also
  // have the side effect of looking up the paint server if that is
  // the indicated type and storing it in a property, so need to be
  // called before SetupCairo{Fill,Stroke}.
  PRBool HasFill();
  PRBool HasStroke();

  // Setup/Cleanup a cairo context for filling a path
  nsresult SetupCairoFill(gfxContext *aContext, void **aClosure);
  void CleanupCairoFill(gfxContext *aContext, void *aClosure);

  // Set up a cairo context for measuring a stroked path
  void SetupCairoStrokeGeometry(gfxContext *aContext);

  // Set up a cairo context for hit testing a stroked path
  void SetupCairoStrokeHitGeometry(gfxContext *aContext);

  // Setup/Cleanup a cairo context for stroking path
  nsresult SetupCairoStroke(gfxContext *aContext, void **aClosure);
  void CleanupCairoStroke(gfxContext *aContext, void *aClosure);

protected:
  virtual nsresult UpdateGraphic(PRBool suppressInvalidation = PR_FALSE) = 0;

  nsSVGPaintServerFrame *GetPaintServer(const nsStyleSVGPaint *aPaint);

  NS_IMETHOD InitSVG();

private:
  nsresult GetStrokeDashArray(double **arr, PRUint32 *count);
  float GetStrokeDashoffset();
  void RemovePaintServerProperties();

  // Returns opacity that should be used in rendering this primitive.
  // In the general case the return value is just the passed opacity.
  // If we can avoid the expense of a specified group opacity, we
  // multiply the passed opacity by the value of the 'opacity'
  // property, and elsewhere pretend the 'opacity' property has a
  // value of 1.
  float MaybeOptimizeOpacity(float aOpacity);
};

#endif // __NS_SVGGEOMETRYFRAME_H__

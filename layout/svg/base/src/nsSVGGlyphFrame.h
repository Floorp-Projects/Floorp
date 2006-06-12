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

#ifndef __NS_SVGGLYPHFRAME_H__
#define __NS_SVGGLYPHFRAME_H__

#include "nsSVGGeometryFrame.h"
#include "nsISVGGlyphGeometrySource.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsISVGChildFrame.h"

class nsISVGTextFrame;

typedef nsSVGGeometryFrame nsSVGGlyphFrameBase;

class nsSVGGlyphFrame : public nsSVGGlyphFrameBase,
                        public nsISVGGlyphGeometrySource, // : nsISVGGlyphMetricsSource 
                        public nsISVGGlyphFragmentLeaf, // : nsISVGGlyphFragmentNode
                        public nsISVGChildFrame
{
protected:
  friend nsIFrame*
  NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                      nsIFrame* parentFrame, nsStyleContext* aContext);
  nsSVGGlyphFrame(nsStyleContext* aContext);

public:
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return 1; }
  NS_IMETHOD_(nsrefcnt) Release() { return 1; }

  // nsIFrame interface:
  NS_IMETHOD
  Init(nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD  CharacterDataChanged(nsPresContext*  aPresContext,
                                   nsIContent*     aChild,
                                   PRBool          aAppend);

  NS_IMETHOD  DidSetStyleContext();

  NS_IMETHOD  SetSelected(nsPresContext* aPresContext,
                          nsIDOMRange*    aRange,
                          PRBool          aSelected,
                          nsSpread        aSpread);
  NS_IMETHOD  GetSelected(PRBool *aSelected) const;
  NS_IMETHOD  IsSelectable(PRBool* aIsSelectable, PRUint8* aSelectStyle) const;

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgGlyphFrame
   */
  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGlyph"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsISVGRendererCanvas* canvas);
  NS_IMETHOD GetFrameForPointSVG(float x, float y, nsIFrame** hit);
  NS_IMETHOD_(nsRect) GetCoveredRegion();
  NS_IMETHOD UpdateCoveredRegion();
  NS_IMETHOD InitialUpdate();
  NS_IMETHOD NotifyCanvasTMChanged(PRBool suppressInvalidation);
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD SetMatrixPropagation(PRBool aPropagate) { return NS_OK; }
  NS_IMETHOD SetOverrideCTM(nsIDOMSVGMatrix *aCTM) { return NS_ERROR_FAILURE; }
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsISVGGeometrySource interface: 
  NS_IMETHOD GetCanvasTM(nsIDOMSVGMatrix * *aCTM);
  virtual nsresult UpdateGraphic(PRBool suppressInvalidation = PR_FALSE);

  // nsISVGGlyphMetricsSource interface:
  NS_DECL_NSISVGGLYPHMETRICSSOURCE

  // nsISVGGlyphGeometrySource interface:
  NS_DECL_NSISVGGLYPHGEOMETRYSOURCE

  // nsISVGGlyphFragmentLeaf interface:
  NS_IMETHOD_(void) SetGlyphPosition(float x, float y);
  NS_IMETHOD GetGlyphMetrics(nsISVGRendererGlyphMetrics** metrics);
  NS_IMETHOD_(PRBool) IsStartOfChunk(); // == is new absolutely positioned chunk.
  NS_IMETHOD_(void) GetAdjustedPosition(/* inout */ float &x, /* inout */ float &y);

  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetX();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetY();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDx();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDy();
  NS_IMETHOD_(PRUint16) GetTextAnchor();
  NS_IMETHOD_(PRBool) IsAbsolutelyPositioned();

  // nsISVGGlyphFragmentNode interface:
  NS_IMETHOD_(PRUint32) GetNumberOfChars();
  NS_IMETHOD_(float) GetComputedTextLength();
  NS_IMETHOD_(float) GetSubStringLength(PRUint32 charnum, PRUint32 fragmentChars);
  NS_IMETHOD_(PRInt32) GetCharNumAtPosition(nsIDOMSVGPoint *point);
  NS_IMETHOD_(nsISVGGlyphFragmentLeaf *) GetFirstGlyphFragment();
  NS_IMETHOD_(nsISVGGlyphFragmentLeaf *) GetNextGlyphFragment();
  NS_IMETHOD_(PRUint32) BuildGlyphFragmentTree(PRUint32 charNum, PRBool lastBranch);
  NS_IMETHOD_(void) NotifyMetricsSuspended();
  NS_IMETHOD_(void) NotifyMetricsUnsuspended();
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeSuspended();
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeUnsuspended();
  
protected:
  void UpdateGeometry(PRBool bRedraw, PRBool suppressInvalidation);
  void UpdateMetrics();
  void UpdateFragmentTree();
  nsISVGTextFrame *GetTextFrame();
  
  nsString mCharacterData;
  nsCOMPtr<nsISVGRendererGlyphGeometry> mGeometry;
  nsCOMPtr<nsISVGRendererGlyphMetrics> mMetrics;
  float mX, mY;
  PRPackedBool mFragmentTreeDirty;
};

#endif

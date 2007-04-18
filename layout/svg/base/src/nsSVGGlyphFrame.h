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
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsISVGChildFrame.h"
#include "gfxContext.h"
#include "gfxFont.h"

struct nsSVGCharacterPosition;
class nsSVGTextFrame;
class nsSVGGlyphFrame;

typedef nsSVGGeometryFrame nsSVGGlyphFrameBase;

class nsSVGGlyphFrame : public nsSVGGlyphFrameBase,
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
   * @see nsGkAtoms::svgGlyphFrame
   */
  virtual nsIAtom* GetType() const;

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    // Set the frame state bit for text frames to mark them as replaced.
    // XXX kipp: temporary

    return nsSVGGlyphFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGlyph"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsSVGRenderState *aContext, nsRect *aDirtyRect);
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
  NS_IMETHOD_(PRBool) IsDisplayContainer() { return PR_FALSE; }
  NS_IMETHOD_(PRBool) HasValidCoveredRect() { return PR_TRUE; }

  // nsISVGGeometrySource interface: 
  NS_IMETHOD GetCanvasTM(nsIDOMSVGMatrix * *aCTM);
  virtual nsresult UpdateGraphic(PRBool suppressInvalidation = PR_FALSE);

  // nsISVGGlyphFragmentLeaf interface:
  NS_IMETHOD GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval);
  NS_IMETHOD GetRotationOfChar(PRUint32 charnum, float *_retval);
  NS_IMETHOD_(float) GetBaselineOffset(PRUint16 baselineIdentifier);
  NS_IMETHOD_(float) GetAdvance();

  NS_IMETHOD_(void) SetGlyphPosition(float x, float y);
  NS_IMETHOD_(nsSVGTextPathFrame*) FindTextPathParent();
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
  NS_IMETHOD_(void) SetWhitespaceHandling(PRUint8 aWhitespaceHandling);

protected:
  struct nsSVGCharacterPosition {
    gfxPoint pos;
    gfxFloat angle;
    PRBool draw;
  };

  // VC6 does not allow the inner class to access protected members
  // of the outer class
  class nsSVGAutoGlyphHelperContext;
  friend class nsSVGAutoGlyphHelperContext;

  // A helper class to deal with gfxTextRuns and temporary thebes
  // contexts.  It destroys them when it goes out of scope.
  class nsSVGAutoGlyphHelperContext
  {
  public:
    nsSVGAutoGlyphHelperContext(nsSVGGlyphFrame *aSource,
                                const nsString &aText)
    {
      Init(aSource, aText);
    }

    nsSVGAutoGlyphHelperContext(nsSVGGlyphFrame *aSource,
                                const nsString &aText,
                                nsSVGCharacterPosition **cp);

    gfxContext *GetContext() { return mCT; }
    gfxTextRun *GetTextRun() { return mTextRun; }

  private:
    void Init(nsSVGGlyphFrame *aSource, const nsString &aText);

    nsRefPtr<gfxContext> mCT;
    nsAutoPtr<gfxTextRun> mTextRun;
  };

  gfxTextRun *GetTextRun(gfxContext *aCtx,
                         const nsString &aText);

  PRBool GetCharacterData(nsAString & aCharacterData);
  nsresult GetCharacterPosition(gfxContext *aContext,
                                const nsString &aText,
                                nsSVGCharacterPosition **aCharacterPosition);

  enum FillOrStroke { FILL, STROKE};

  void LoopCharacters(gfxContext *aCtx, const nsString &aText,
                      const nsSVGCharacterPosition *aCP,
                      FillOrStroke aFillOrStroke);

  void UpdateGeometry(PRBool bRedraw, PRBool suppressInvalidation);
  void UpdateMetrics();
  PRBool ContainsPoint(float x, float y);
  nsresult GetGlobalTransform(gfxContext *aContext);
  nsresult GetHighlight(PRUint32 *charnum, PRUint32 *nchars,
                        nscolor *foreground, nscolor *background);

  nsRefPtr<gfxFontGroup> mFontGroup;
  nsAutoPtr<gfxFontStyle> mFontStyle;
  gfxPoint mPosition;
  PRUint8 mWhitespaceHandling;
};

#endif

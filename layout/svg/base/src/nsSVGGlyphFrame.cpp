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

#include "nsFrame.h"
#include "nsISVGRendererGlyphGeometry.h"
#include "nsISVGRendererGlyphMetrics.h"
#include "nsISVGRenderer.h"
#include "nsISVGGlyphGeometrySource.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsITextContent.h"
#include "nsISVGChildFrame.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGTextFrame.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGContainerFrame.h"
#include "nsISVGTextContainerFrame.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prdtoa.h"
#include "nsIDOMSVGRect.h"
#include "nsILookAndFeel.h"
#include "nsTextFragment.h"

typedef nsFrame nsSVGGlyphFrameBase;

class nsSVGGlyphFrame : public nsSVGGlyphFrameBase,
                        public nsISVGGlyphGeometrySource, // : nsISVGGlyphMetricsSource : nsISVGGeometrySource
                        public nsISVGGlyphFragmentLeaf, // : nsISVGGlyphFragmentNode
                        public nsISVGChildFrame
{
protected:
  friend nsresult
  NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                      nsIFrame* parentFrame, nsIFrame** aNewFrame);
  nsSVGGlyphFrame();
  virtual ~nsSVGGlyphFrame();

public:
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsIFrame interface:
  NS_IMETHOD
  Init(nsPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsStyleContext*  aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD  CharacterDataChanged(nsPresContext* aPresContext,
                                   nsIContent*     aChild,
                                   PRBool          aAppend);

  NS_IMETHOD  DidSetStyleContext(nsPresContext* aPresContext);

  NS_IMETHOD  SetSelected(nsPresContext* aPresContext,
                          nsIDOMRange*    aRange,
                          PRBool          aSelected,
                          nsSpread        aSpread);
  NS_IMETHOD  GetSelected(PRBool *aSelected) const;
  NS_IMETHOD  IsSelectable(PRBool* aIsSelectable, PRUint8* aSelectStyle);

  // nsISVGChildFrame interface:
  NS_IMETHOD Paint(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips);
  NS_IMETHOD GetFrameForPoint(float x, float y, nsIFrame** hit);
  NS_IMETHOD_(already_AddRefed<nsISVGRendererRegion>) GetCoveredRegion();
  NS_IMETHOD InitialUpdate();
  NS_IMETHOD NotifyCanvasTMChanged();
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsISVGGeometrySource interface: 
  NS_DECL_NSISVGGEOMETRYSOURCE

  // nsISVGGlyphMetricsSource interface:
  NS_DECL_NSISVGGLYPHMETRICSSOURCE

  // nsISVGGlyphGeometrySource interface:
  NS_DECL_NSISVGGLYPHGEOMETRYSOURCE

  // nsISVGGlyphFragmentLeaf interface:
  NS_IMETHOD_(void) SetGlyphPosition(float x, float y);
  NS_IMETHOD_(float) GetGlyphPositionX();
  NS_IMETHOD_(float) GetGlyphPositionY();  
  NS_IMETHOD GetGlyphMetrics(nsISVGRendererGlyphMetrics** metrics);
  NS_IMETHOD_(PRBool) IsStartOfChunk(); // == is new absolutely positioned chunk.
  NS_IMETHOD_(void) GetAdjustedPosition(/* inout */ float &x, /* inout */ float &y);
  NS_IMETHOD_(PRUint32) GetNumberOfChars();
  NS_IMETHOD_(PRUint32) GetCharNumberOffset();

  // nsISVGGlyphFragmentNode interface:
  NS_IMETHOD_(nsISVGGlyphFragmentLeaf *) GetFirstGlyphFragment();
  NS_IMETHOD_(nsISVGGlyphFragmentLeaf *) GetNextGlyphFragment();
  NS_IMETHOD_(PRUint32) BuildGlyphFragmentTree(PRUint32 charNum, PRBool lastBranch);
  NS_IMETHOD_(void) NotifyMetricsSuspended();
  NS_IMETHOD_(void) NotifyMetricsUnsuspended();
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeSuspended();
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeUnsuspended();
  
protected:
  void UpdateGeometry(PRUint32 flags, PRBool bRedraw=PR_TRUE);
  void UpdateMetrics(PRUint32 flags);
  void UpdateFragmentTree();
  nsISVGOuterSVGFrame *GetOuterSVGFrame();
  nsISVGTextFrame *GetTextFrame();
  
  nsCOMPtr<nsISVGRendererGlyphGeometry> mGeometry;
  nsCOMPtr<nsISVGRendererGlyphMetrics> mMetrics;
  float mX, mY;
  PRUint32 mCharOffset;
  PRUint32 mGeometryUpdateFlags;
  PRUint32 mMetricsUpdateFlags;
  PRBool mFragmentTreeDirty;
  nsString mCharacterData;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parentFrame,
                    nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

#ifdef DEBUG
  NS_ASSERTION(parentFrame, "null parent");
  nsISVGTextContainerFrame *text_container;
  parentFrame->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame), (void**)&text_container);
  NS_ASSERTION(text_container, "trying to construct an SVGGlyphFrame for an invalid container");
  
  nsCOMPtr<nsITextContent> tc = do_QueryInterface(aContent);
  NS_ASSERTION(tc, "trying to construct an SVGGlyphFrame for wrong content element");
#endif
  
  nsSVGGlyphFrame* it = new (aPresShell) nsSVGGlyphFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsSVGGlyphFrame::nsSVGGlyphFrame()
    : mGeometryUpdateFlags(0), mMetricsUpdateFlags(0),
      mCharOffset(0), mFragmentTreeDirty(PR_FALSE)
{
}

nsSVGGlyphFrame::~nsSVGGlyphFrame()
{

}


//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGlyphFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGGeometrySource)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphMetricsSource)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphGeometrySource)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentLeaf)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentNode)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGlyphFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::Init(nsPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsStyleContext*  aContext,
                      nsIFrame*        aPrevInFlow)
{
//  rv = nsSVGGlyphFrameBase::Init(aPresContext, aContent, aParent,
//                                 aContext, aPrevInFlow);

  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  // construct our glyphmetrics & glyphgeometry objects:
  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("No outerSVGFrame");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));

  renderer->CreateGlyphMetrics(this, getter_AddRefs(mMetrics));
  if (!mMetrics) return NS_ERROR_FAILURE;
  
  renderer->CreateGlyphGeometry(this, getter_AddRefs(mGeometry));
  if (!mGeometry) return NS_ERROR_FAILURE;
  
  
  SetStyleContext(aPresContext, aContext);
    
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::CharacterDataChanged(nsPresContext* aPresContext,
                                      nsIContent*     aChild,
                                      PRBool          aAppend)
{
#ifdef DEBUG
//  printf("** nsSVGGlyphFrame::CharacterDataChanged\n");
#endif
  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("No outerSVGFrame");
    return NS_ERROR_FAILURE;
  }
  
  outerSVGFrame->SuspendRedraw();
  UpdateFragmentTree();
  UpdateMetrics(nsISVGGeometrySource::UPDATEMASK_ALL);
  UpdateGeometry(nsISVGGeometrySource::UPDATEMASK_ALL);
  outerSVGFrame->UnsuspendRedraw();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::DidSetStyleContext(nsPresContext* aPresContext)
{
  return CharacterDataChanged(aPresContext, NULL, PR_FALSE);
}

NS_IMETHODIMP
nsSVGGlyphFrame::SetSelected(nsPresContext* aPresContext,
                             nsIDOMRange*    aRange,
                             PRBool          aSelected,
                             nsSpread        aSpread)
{
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::SetSelected()\n", this);
#endif
//  return nsSVGGlyphFrameBase::SetSelected(aPresContext, aRange, aSelected, aSpread);

  // check whether style allows selection
  PRBool  selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
    return NS_OK;
  
  if ( aSelected ){
    mState |=  NS_FRAME_SELECTED_CONTENT;
  }
  else
    mState &= ~NS_FRAME_SELECTED_CONTENT;

  UpdateGeometry(nsISVGGlyphGeometrySource::UPDATEMASK_HIGHLIGHT |
                 nsISVGGlyphGeometrySource::UPDATEMASK_HAS_HIGHLIGHT,
                 PR_FALSE);  

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetSelected(PRBool *aSelected) const
{
  nsresult rv = nsSVGGlyphFrameBase::GetSelected(aSelected);
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::GetSelected()=%d\n", this, *aSelected);
#endif
  return rv;
}

NS_IMETHODIMP
nsSVGGlyphFrame::IsSelectable(PRBool* aIsSelectable,
                              PRUint8* aSelectStyle)
{
  nsresult rv = nsSVGGlyphFrameBase::IsSelectable(aIsSelectable, aSelectStyle);
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::IsSelectable()=(%d,%d)\n", this, *aIsSelectable, aSelectStyle);
#endif
  return rv;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::Paint(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips)
{
#ifdef DEBUG
  //printf("nsSVGGlyphFrame(%p)::Paint\n", this);
#endif
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  mGeometry->Render(canvas);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetFrameForPoint(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
  //printf("nsSVGGlyphFrame(%p)::GetFrameForPoint\n", this);
#endif
  // test for hit:
  *hit = nsnull;
  PRBool isHit;
  mGeometry->ContainsPoint(x, y, &isHit);
  if (isHit) 
    *hit = this;
  
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGGlyphFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *region = nsnull;
  mGeometry->GetCoveredRegion(&region);
  return region;
}

NS_IMETHODIMP
nsSVGGlyphFrame::InitialUpdate()
{
  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("No outerSVGFrame");
    return NS_ERROR_FAILURE;
  }
  
  outerSVGFrame->SuspendRedraw();
  UpdateFragmentTree();
  UpdateMetrics(nsISVGGeometrySource::UPDATEMASK_ALL);
  UpdateGeometry(nsISVGGeometrySource::UPDATEMASK_ALL);
  outerSVGFrame->UnsuspendRedraw();
  
  return NS_OK;
}  

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyCanvasTMChanged()
{
  UpdateGeometry(nsISVGGeometrySource::UPDATEMASK_CANVAS_TM);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyRedrawSuspended()
{
  // XXX should we cache the fact that redraw is suspended?
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyRedrawUnsuspended()
{
  NS_ASSERTION(!mMetricsUpdateFlags, "dirty metrics in nsSVGGlyphFrame::NotifyRedrawUnsuspended");
  NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::NotifyRedrawUnsuspended");
    
  if (mGeometryUpdateFlags != 0) {
    nsCOMPtr<nsISVGRendererRegion> dirty_region;
    mGeometry->Update(mGeometryUpdateFlags, getter_AddRefs(dirty_region));
    if (dirty_region) {
      nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
      if (outerSVGFrame)
        outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);
    }
    mGeometryUpdateFlags = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsresult rv = mMetrics->GetBoundingBox(_retval);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  // offset the bounds by the position of this glyph fragment:
  float x,y;
  (*_retval)->GetX(&x);
  (*_retval)->GetY(&y);
  (*_retval)->SetX(x+mX);
  (*_retval)->SetY(y+mY);
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGGeometrySource methods:

/* [noscript] readonly attribute nsPresContext presContext; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetPresContext(nsPresContext * *aPresContext)
{
  // XXX gcc 3.2.2 requires the explicit 'nsSVGGlyphFrameBase::' qualification
  *aPresContext = nsSVGGlyphFrameBase::GetPresContext();
  NS_ADDREF(*aPresContext);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGMatrix canvasTM; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetCanvasTM(nsIDOMSVGMatrix * *aCTM)
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
  *aCTM = parentTM.get();
  NS_ADDREF(*aCTM);
  return NS_OK;
}

/* readonly attribute float strokeOpacity; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeOpacity(float *aStrokeOpacity)
{
  *aStrokeOpacity = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeOpacity;
  return NS_OK;
}

/* readonly attribute float strokeWidth; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeWidth(float *aStrokeWidth)
{
  *aStrokeWidth = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeWidth;
  return NS_OK;
}

/* void getStrokeDashArray ([array, size_is (count)] out float arr, out unsigned long count); */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeDashArray(float **arr, PRUint32 *count)
{
  *arr = nsnull;
  *count = 0;
  
  const nsString &dasharrayString = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeDasharray;
  if (dasharrayString.Length() == 0) return NS_OK;

  // XXX parsing of the dasharray string should be done elsewhere

  char *str = ToNewCString(dasharrayString);

  // array elements are separated by commas. count them to get our max
  // no of elems.

  int i=0;
  char* cp = str;
  while (*cp) {
    if (*cp == ',')
      ++i;
    ++cp;
  }
  ++i;

  // now get the elements
  
  *arr = (float*) nsMemory::Alloc(i * sizeof(float));

  cp = str;
  char *elem;
  while ((elem = nsCRT::strtok(cp, "',", &cp))) {
    char *end;
    (*arr)[(*count)++] = (float) PR_strtod(elem, &end);
#ifdef DEBUG
    //printf("[%f]",(*arr)[(*count)-1]);
#endif
  }
  
  nsMemory::Free(str);

  return NS_OK;
}

/* readonly attribute float strokeDashoffset; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeDashoffset(float *aStrokeDashoffset)
{
  *aStrokeDashoffset = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeDashoffset;
  return NS_OK;
}

/* readonly attribute unsigned short strokeLinecap; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeLinecap(PRUint16 *aStrokeLinecap)
{
  *aStrokeLinecap = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeLinecap;
  return NS_OK;
}

/* readonly attribute unsigned short strokeLinejoin; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeLinejoin(PRUint16 *aStrokeLinejoin)
{
  *aStrokeLinejoin = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeLinejoin;
  return NS_OK;
}

/* readonly attribute float strokeMiterlimit; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeMiterlimit(float *aStrokeMiterlimit)
{
  *aStrokeMiterlimit = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeMiterlimit; 
  return NS_OK;
}

/* readonly attribute float fillOpacity; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillOpacity(float *aFillOpacity)
{
  *aFillOpacity = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFillOpacity;
  return NS_OK;
}

/* readonly attribute unsigned short fillRule; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillRule(PRUint16 *aFillRule)
{
  *aFillRule = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFillRule;
  return NS_OK;
}

/* readonly attribute unsigned short strokePaintType; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokePaintType(PRUint16 *aStrokePaintType)
{
  *aStrokePaintType = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStroke.mType;
  return NS_OK;
}

/* [noscript] readonly attribute nscolor strokePaint; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokePaint(nscolor *aStrokePaint)
{
  *aStrokePaint = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStroke.mColor;
  return NS_OK;
}

/* readonly attribute unsigned short fillPaintType; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillPaintType(PRUint16 *aFillPaintType)
{
  *aFillPaintType = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFill.mType;
  return NS_OK;
}

/* [noscript] readonly attribute nscolor fillPaint; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillPaint(nscolor *aFillPaint)
{
  *aFillPaint = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFill.mColor;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGGlyphMetricsSource methods:

/* [noscript] readonly attribute nsFont font; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFont(nsFont *aFont)
{
  *aFont = ((const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font))->mFont;

  // XXX eventually we will have to treat decorations separately from
  // fonts, because they can have a different color than the current
  // glyph.
  
  NS_ASSERTION(mParent, "no parent");
  nsStyleContext *parentContext = mParent->GetStyleContext();
  NS_ASSERTION(parentContext, "no style context on parent");
  
  PRUint8 styleDecorations =
    ((const nsStyleTextReset*)parentContext->GetStyleData(eStyleStruct_TextReset))->mTextDecoration;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_UNDERLINE)
    aFont->decorations |= NS_FONT_DECORATION_UNDERLINE;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_OVERLINE)
    aFont->decorations |= NS_FONT_DECORATION_OVERLINE;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_LINE_THROUGH)
    aFont->decorations |= NS_FONT_DECORATION_LINE_THROUGH;    
  
  return NS_OK;
}

/* readonly attribute DOMString characterData; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetCharacterData(nsAString & aCharacterData)
{
  aCharacterData = mCharacterData;
  return NS_OK;
}

/* readonly attribute unsigned short textRendering; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetTextRendering(PRUint16 *aTextRendering)
{
  *aTextRendering = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mTextRendering;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGGlyphGeometrySource methods:

/* readonly attribute nsISVGRendererGlyphMetrics metrics; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetMetrics(nsISVGRendererGlyphMetrics * *aMetrics)
{
  *aMetrics = mMetrics;
  NS_ADDREF(*aMetrics);
  return NS_OK;
}

/* readonly attribute float x; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}

/* readonly attribute float y; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}

/* readonly attribute boolean hasHighlight; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetHasHighlight(PRBool *aHasHighlight)
{
  *aHasHighlight = (mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;

  return NS_OK;
}


// Utilities for converting from indices in the uncompressed content
// element strings to compressed frame string and back:
int CompressIndex(int index, const nsTextFragment*fragment)
{
  int ci=0;
  if (fragment->Is2b()) {
    const PRUnichar *data=fragment->Get2b();
    while(*data && index) {
      if (XP_IS_SPACE_W(*data)){
        do {
          ++data;
          --index;
        }while(XP_IS_SPACE_W(*data) && index);
      }
      else {
        ++data;
        --index;
      }
      ++ci;
    }
  }
  else {
    const char *data=fragment->Get1b();
    while(*data && index) {
      if (XP_IS_SPACE_W(*data)){
        do {
          ++data;
          --index;
        }while(XP_IS_SPACE_W(*data) && index);
      }
      else {
        ++data;
        --index;
      }
      ++ci;
    }
  }
    
  return ci;
}

int UncompressIndex(int index, PRBool bRightAffinity, const nsTextFragment*fragment)
{
  // XXX
  return index;
}

/* [noscript] void getHighlight (out unsigned long charnum, out unsigned long nchars, out nscolor foreground, out nscolor background); */
NS_IMETHODIMP
nsSVGGlyphFrame::GetHighlight(PRUint32 *charnum, PRUint32 *nchars, nscolor *foreground, nscolor *background)
{
  *foreground = NS_RGB(255,255,255);
  *background = NS_RGB(0,0,0); 
  *charnum=0;
  *nchars=0;

    PRBool hasHighlight;
  GetHasHighlight(&hasHighlight);

  if (!hasHighlight) {
    NS_ERROR("nsSVGGlyphFrame::GetHighlight() called by renderer when there is no highlight");
    return NS_ERROR_FAILURE;
  }

  // XXX gcc 3.2.2 requires the explicit 'nsSVGGlyphFrameBase::' qualification
  nsPresContext *presContext = nsSVGGlyphFrameBase::GetPresContext();

  nsCOMPtr<nsITextContent> tc = do_QueryInterface(mContent);
  NS_ASSERTION(tc, "no textcontent interface");

  // The selection ranges are relative to the uncompressed text in
  // the content element. We'll need the text fragment:
  const nsTextFragment *fragment = tc->Text();
  
  // get the selection details 
  SelectionDetails *details = nsnull;
  {
    nsCOMPtr<nsIFrameSelection> frameSelection;
    {
      nsCOMPtr<nsISelectionController> controller;
      GetSelectionController(presContext, getter_AddRefs(controller));
      
      if (!controller) {
        NS_ERROR("no selection controller");
        return NS_ERROR_FAILURE;
      }
      frameSelection = do_QueryInterface(controller);
    }
    if (!frameSelection) {
      frameSelection = presContext->PresShell()->FrameSelection();
    }
    if (!frameSelection) {
      NS_ERROR("no frameselection interface");
      return NS_ERROR_FAILURE;
    }

    frameSelection->LookUpSelection(mContent, 0, fragment->GetLength(),
                                    &details, PR_FALSE);
  }

#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  {
    SelectionDetails *dp = details;
    printf("nsSVGGlyphFrame(%p)::GetHighlight() [\n", this);
    while (dp) {
      printf("selection detail: %d(%d)->%d(%d) type %d\n",
             dp->mStart, CompressIndex(dp->mStart, fragment),
             dp->mEnd, CompressIndex(dp->mEnd, fragment),
             dp->mType);
      dp = dp->mNext;
    }
    printf("]\n");
      
  }
#endif
  
  if (details) {
    NS_ASSERTION(details->mNext==nsnull, "can't do multiple selection ranges");

    *charnum=CompressIndex(details->mStart, fragment);
    *nchars=CompressIndex(details->mEnd, fragment)-*charnum;  
    
    nsILookAndFeel *look = presContext->LookAndFeel();

    look->GetColor(nsILookAndFeel::eColor_TextSelectBackground, *background);
    look->GetColor(nsILookAndFeel::eColor_TextSelectForeground, *foreground);

    SelectionDetails *dp = details;
    while ((dp=details->mNext) != nsnull) {
      delete details;
      details = dp;
    }
    delete details;
  }
  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGGlyphFragmentLeaf interface:

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::SetGlyphPosition(float x, float y)
{
  mX = x;
  mY = y;
  UpdateGeometry(nsISVGGlyphGeometrySource::UPDATEMASK_X |
                 nsISVGGlyphGeometrySource::UPDATEMASK_Y);
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetGlyphPositionX()
{
  return mX;
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetGlyphPositionY()
{
  return mY;
}


NS_IMETHODIMP
nsSVGGlyphFrame::GetGlyphMetrics(nsISVGRendererGlyphMetrics** metrics)
{
  *metrics = mMetrics;
  NS_ADDREF(*metrics);
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsSVGGlyphFrame::IsStartOfChunk()
{
  // this fragment is a chunk if it has a corresponding absolute
  // position adjustment in an ancestors' x or y array. (At the moment
  // we don't map the full arrays, but only the first elements.)

  return PR_FALSE;
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::GetAdjustedPosition(/* inout */ float &x, /* inout */ float &y)
{
}

NS_IMETHODIMP_(PRUint32)
nsSVGGlyphFrame::GetNumberOfChars()
{
  return mCharacterData.Length();
}

NS_IMETHODIMP_(PRUint32)
nsSVGGlyphFrame::GetCharNumberOffset()
{
  return mCharOffset;
}

//----------------------------------------------------------------------
// nsISVGGlyphFragmentNode interface:

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGGlyphFrame::GetFirstGlyphFragment()
{
  return this;
}

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGGlyphFrame::GetNextGlyphFragment()
{
  nsIFrame* sibling = mNextSibling;
  while (sibling) {
    nsISVGGlyphFragmentNode *node = nsnull;
    sibling->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
    if (node)
      return node->GetFirstGlyphFragment();
    sibling = sibling->GetNextSibling();
  }

  // no more siblings. go back up the tree.
  
  NS_ASSERTION(mParent, "null parent");
  nsISVGGlyphFragmentNode *node = nsnull;
  mParent->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
  return node ? node->GetNextGlyphFragment() : nsnull;
}

NS_IMETHODIMP_(PRUint32)
nsSVGGlyphFrame::BuildGlyphFragmentTree(PRUint32 charNum, PRBool lastBranch)
{
  // XXX actually we should be building a new fragment for each chunk here...


  mCharOffset = charNum;
  nsCOMPtr<nsITextContent> tc = do_QueryInterface(mContent);

  if (tc->TextLength() == 0) {
#ifdef DEBUG
    printf("Glyph frame with zero length text\n");
#endif
    mCharacterData.AssignLiteral("");
    return charNum;
  }

  mCharacterData.Truncate();
  tc->AppendTextTo(mCharacterData);
  mCharacterData.CompressWhitespace(charNum==0, lastBranch);

  return charNum+mCharacterData.Length();
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::NotifyMetricsSuspended()
{
  // do nothing
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::NotifyMetricsUnsuspended()
{
  NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::NotifyMetricsUnsuspended");

  if (mMetricsUpdateFlags != 0) {
    PRBool metricsDirty;
    mMetrics->Update(mMetricsUpdateFlags, &metricsDirty);
    if (metricsDirty) {
      mGeometryUpdateFlags |= nsISVGGlyphGeometrySource::UPDATEMASK_METRICS;
      nsISVGTextFrame* text_frame = GetTextFrame();
      NS_ASSERTION(text_frame, "null text frame");
      if (text_frame)
        text_frame->NotifyGlyphMetricsChange(this);
    }
    mMetricsUpdateFlags = 0;
  }   
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::NotifyGlyphFragmentTreeSuspended()
{
  // do nothing
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::NotifyGlyphFragmentTreeUnsuspended()
{
  if (mFragmentTreeDirty) {
    nsISVGTextFrame* text_frame = GetTextFrame();
    NS_ASSERTION(text_frame, "null text frame");
    if (text_frame)
      text_frame->NotifyGlyphFragmentTreeChange(this);
    mFragmentTreeDirty = PR_FALSE;
  }
}



//----------------------------------------------------------------------
//

void nsSVGGlyphFrame::UpdateGeometry(PRUint32 flags, PRBool bRedraw)
{
  mGeometryUpdateFlags |= flags;
  
  nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }
  
  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (!suspended) {
    NS_ASSERTION(!mMetricsUpdateFlags, "dirty metrics in nsSVGGlyphFrame::UpdateGeometry");
    NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::UpdateGeometry");
    nsCOMPtr<nsISVGRendererRegion> dirty_region;
    mGeometry->Update(mGeometryUpdateFlags, getter_AddRefs(dirty_region));
    if (dirty_region)
      outerSVGFrame->InvalidateRegion(dirty_region, bRedraw);
    mGeometryUpdateFlags = 0;
  }  
}

void nsSVGGlyphFrame::UpdateMetrics(PRUint32 flags)
{
  mMetricsUpdateFlags |= flags;

  nsISVGTextFrame* text_frame = GetTextFrame();
  if (!text_frame) {
    NS_ERROR("null text_frame");
    return;
  }
  
  PRBool suspended = text_frame->IsMetricsSuspended();
  if (!suspended) {
    NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::UpdateMetrics");
    PRBool metricsDirty;
    mMetrics->Update(mMetricsUpdateFlags, &metricsDirty);
    if (metricsDirty) {
      mGeometryUpdateFlags |= nsISVGGlyphGeometrySource::UPDATEMASK_METRICS;
      text_frame->NotifyGlyphMetricsChange(this);
    }
    mMetricsUpdateFlags = 0;
  }
}

void nsSVGGlyphFrame::UpdateFragmentTree()
{
  mFragmentTreeDirty = PR_TRUE;
    
  nsISVGTextFrame* text_frame = GetTextFrame();
  if (!text_frame) {
    NS_ERROR("null text_frame");
    return;
  }
  
  PRBool suspended = text_frame->IsGlyphFragmentTreeSuspended();
  if (!suspended) {
    text_frame->NotifyGlyphFragmentTreeChange(this);
    mFragmentTreeDirty = PR_FALSE;
  }
}

nsISVGOuterSVGFrame *
nsSVGGlyphFrame::GetOuterSVGFrame()
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetOuterSVGFrame();  
}

nsISVGTextFrame *
nsSVGGlyphFrame::GetTextFrame()
{
  NS_ASSERTION(mParent, "null parent");

  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetTextFrame();
}

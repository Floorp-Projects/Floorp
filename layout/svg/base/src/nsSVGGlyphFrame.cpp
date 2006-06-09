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

#include "nsISVGRendererGlyphGeometry.h"
#include "nsISVGRendererGlyphMetrics.h"
#include "nsISVGRenderer.h"
#include "nsITextContent.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGTextFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsISVGTextContainerFrame.h"
#include "nsISVGValueUtils.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prdtoa.h"
#include "nsIDOMSVGRect.h"
#include "nsILookAndFeel.h"
#include "nsTextFragment.h"
#include "nsSVGRect.h"
#include "nsSVGPoint.h"
#include "nsSVGAtoms.h"
#include "nsIViewManager.h"
#include "nsINameSpaceManager.h"
#include "nsContainerFrame.h"
#include "nsLayoutAtoms.h"
#include "nsSVGUtils.h"
#include "nsISVGPathFlatten.h"
#include "nsSVGGlyphFrame.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parentFrame, nsStyleContext* aContext)
{
#ifdef DEBUG
  NS_ASSERTION(parentFrame, "null parent");
  nsISVGTextContainerFrame *text_container;
  parentFrame->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame), (void**)&text_container);
  NS_ASSERTION(text_container, "trying to construct an SVGGlyphFrame for an invalid container");
  
  nsCOMPtr<nsITextContent> tc = do_QueryInterface(aContent);
  NS_ASSERTION(tc, "trying to construct an SVGGlyphFrame for wrong content element");
#endif

  return new (aPresShell) nsSVGGlyphFrame(aContext);
}

nsSVGGlyphFrame::nsSVGGlyphFrame(nsStyleContext* aContext)
    : nsSVGGlyphFrameBase(aContext),
      mCharOffset(0),
      mFragmentTreeDirty(PR_FALSE)
{
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGlyphFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphMetricsSource)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphGeometrySource)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentLeaf)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentNode)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGlyphFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        aPrevInFlow)
{
//  rv = nsSVGGlyphFrameBase::Init(aPresContext, aContent, aParent, aPrevInFlow);

  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  if (mContent) {
    mContent->SetMayHaveFrame(PR_TRUE);
  }
  
  // construct our glyphmetrics & glyphgeometry objects:
  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("No outerSVGFrame");
    DidSetStyleContext();
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  if (renderer) {
    renderer->CreateGlyphMetrics(this, getter_AddRefs(mMetrics));
    renderer->CreateGlyphGeometry(getter_AddRefs(mGeometry));
  }

  nsSVGGlyphFrameBase::InitSVG();
  DidSetStyleContext();

  if (!renderer || !mMetrics || !mGeometry)
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::CharacterDataChanged(nsPresContext*  aPresContext,
                                      nsIContent*     aChild,
                                      PRBool          aAppend)
{
	return UpdateGraphic();
}

nsresult
nsSVGGlyphFrame::UpdateGraphic(PRBool suppressInvalidation)
{
  if (GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)
    return NS_OK;

#ifdef DEBUG
//  printf("** nsSVGGlyphFrame::Update\n");
#endif
  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("No outerSVGFrame");
    return NS_ERROR_FAILURE;
  }
  
  outerSVGFrame->SuspendRedraw();
  UpdateFragmentTree();
  UpdateMetrics();
  UpdateGeometry(PR_TRUE, PR_FALSE);
  outerSVGFrame->UnsuspendRedraw();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::DidSetStyleContext()
{
  nsSVGGlyphFrameBase::DidSetStyleContext();

  return CharacterDataChanged(nsnull, nsnull, PR_FALSE);
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

  UpdateGeometry(PR_FALSE, PR_FALSE);

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
                              PRUint8* aSelectStyle) const
{
  nsresult rv = nsSVGGlyphFrameBase::IsSelectable(aIsSelectable, aSelectStyle);
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::IsSelectable()=(%d,%d)\n", this, *aIsSelectable, aSelectStyle);
#endif
  return rv;
}

nsIAtom *
nsSVGGlyphFrame::GetType() const
{
  return nsLayoutAtoms::svgGlyphFrame;
}

PRBool
nsSVGGlyphFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~nsIFrame::eSVG);
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::PaintSVG(nsISVGRendererCanvas* canvas)
{
#ifdef DEBUG
  //printf("nsSVGGlyphFrame(%p)::Paint\n", this);
#endif
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  mGeometry->Render(this, canvas);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
  //printf("nsSVGGlyphFrame(%p)::GetFrameForPoint\n", this);
#endif
  // test for hit:
  *hit = nsnull;

  if (!mRect.Contains(x, y))
    return NS_OK;

  PRBool events = PR_FALSE;
  switch (GetStyleSVG()->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
      if (GetStyleVisibility()->IsVisible() &&
          (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None ||
           GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None))
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      if (GetStyleVisibility()->IsVisible())
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_PAINTED:
      if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None ||
          GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_FILL:
    case NS_STYLE_POINTER_EVENTS_STROKE:
    case NS_STYLE_POINTER_EVENTS_ALL:
      events = PR_TRUE;
      break;
    default:
      NS_ERROR("not reached");
      break;
  }

  if (!events)
    return NS_OK;

  PRBool isHit;
  mGeometry->ContainsPoint(this, x, y, &isHit);
  if (isHit) 
    *hit = this;
  
  return NS_OK;
}

NS_IMETHODIMP_(nsRect)
nsSVGGlyphFrame::GetCoveredRegion()
{
  return mRect;
}

NS_IMETHODIMP
nsSVGGlyphFrame::UpdateCoveredRegion()
{
  mGeometry->GetCoveredRegion(this, &mRect);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::InitialUpdate()
{
  return UpdateGraphic();
}  

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  UpdateGeometry(PR_TRUE, suppressInvalidation);
  
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
  NS_ASSERTION(!(GetStateBits() & NS_STATE_SVG_METRICS_DIRTY),
               "dirty metrics in nsSVGGlyphFrame::NotifyRedrawUnsuspended");
  NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::NotifyRedrawUnsuspended");
    
  if (GetStateBits() & NS_STATE_SVG_DIRTY)
    UpdateGeometry(PR_TRUE, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  if (mGeometry)
    return mGeometry->GetBoundingBox(this, _retval);
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsSVGGeometryFrame methods:

/* readonly attribute nsIDOMSVGMatrix canvasTM; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetCanvasTM(nsIDOMSVGMatrix * *aCTM)
{
  NS_ASSERTION(mParent, "null parent");
  
  nsSVGContainerFrame *containerFrame = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                       mParent);
  nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();

  *aCTM = parentTM.get();
  NS_ADDREF(*aCTM);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGGlyphMetricsSource methods:

/* [noscript] readonly attribute nsFont font; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFont(nsFont *aFont)
{
  *aFont = GetStyleFont()->mFont;

  // XXX eventually we will have to treat decorations separately from
  // fonts, because they can have a different color than the current
  // glyph.
  
  NS_ASSERTION(mParent, "no parent");
  nsStyleContext *parentContext = mParent->GetStyleContext();
  NS_ASSERTION(parentContext, "no style context on parent");
  
  PRUint8 styleDecorations =
    parentContext->GetStyleTextReset()->mTextDecoration;
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

static void
FindPoint(nsSVGPathData *data,
          float aX, float aY, float aAdvance,
          nsSVGCharacterPosition *aCP)
{
  float x, y, length = 0;
  float midpoint = aX + aAdvance/2;
  for (PRUint32 i = 0; i < data->count; i++) {
    if (data->type[i] == NS_SVGPATHFLATTEN_LINE) {
      float dx = data->x[i] - x;
      float dy = data->y[i] - y;
      float sublength = sqrt(dx*dx + dy*dy);
      
      if (length + sublength > midpoint) {
        float ratio = (aX - length)/sublength;
        aCP->x = x * (1.0f - ratio) + data->x[i] * ratio;
        aCP->y = y * (1.0f - ratio) + data->y[i] * ratio;

        float dx = data->x[i] - x;
        float dy = data->y[i] - y;
        aCP->angle = atan2(dy, dx);

        float normalization = 1.0/sqrt(dx*dx+dy*dy);
        aCP->x += - aY * dy * normalization;
        aCP->y +=   aY * dx * normalization;
        return;
      }
      length += sublength;
    }
    x = data->x[i];
    y = data->y[i];
  }
}

/* readonly attribute nsSVGCharacterPostion characterPosition; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetCharacterPosition(nsSVGCharacterPosition **aCharacterPosition)
{
  *aCharacterPosition = nsnull;
  nsISVGPathFlatten *textPath = nsnull;

  /* check if we're the child of a textPath */
  for (nsIFrame *frame = this; frame != nsnull; frame = frame->GetParent())
    if (frame->GetType() == nsLayoutAtoms::svgTextPathFrame) {
      frame->QueryInterface(NS_GET_IID(nsISVGPathFlatten), (void **)&textPath);
      break;
    }

  /* we're an ordinary fragment - return */
  /* XXX: we might want to use this for individual x/y/dx/dy adjustment */
  if (!textPath)
    return NS_OK;

  nsSVGPathData *data;
  textPath->GetFlattenedPath(&data);

  /* textPath frame, but invalid target */
  if (!data)
    return NS_ERROR_FAILURE;

  float length = data->Length();
  PRUint32 strLength = mCharacterData.Length();

  nsSVGCharacterPosition *cp = new nsSVGCharacterPosition[strLength];

  for (PRUint32 k = 0; k < strLength; k++)
      cp[k].draw = PR_FALSE;

  float x = mX;
  for (PRUint32 i = 0; i < strLength; i++) {
    float advance;
    mMetrics->GetAdvanceOfChar(i, &advance);

    /* have we run off the end of the path? */
    if (x + advance/2 > length)
      break;

    /* check that we've advanced to the start of the path */
    if (x + advance/2 >= 0.0f) {
      cp[i].draw = PR_TRUE;

      // add y (normal)
      // add rotation
      // move point back along tangent
      FindPoint(data, x, mY, advance, &(cp[i]));
    }
    x += advance;
  }

  *aCharacterPosition = cp;

  delete data;

  return NS_OK;
}

/* readonly attribute unsigned short textRendering; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetTextRendering(PRUint16 *aTextRendering)
{
  *aTextRendering = GetStyleSVG()->mTextRendering;
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

  nsPresContext *presContext = GetPresContext();

  nsCOMPtr<nsITextContent> tc = do_QueryInterface(mContent);
  NS_ASSERTION(tc, "no textcontent interface");

  // The selection ranges are relative to the uncompressed text in
  // the content element. We'll need the text fragment:
  const nsTextFragment *fragment = tc->Text();
  
  // get the selection details 
  SelectionDetails *details = nsnull;
  {
    nsCOMPtr<nsFrameSelection> frameSelection;
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

    details = frameSelection->LookUpSelection(
      mContent, 0, fragment->GetLength(), PR_FALSE
      );
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
  UpdateGeometry(PR_TRUE, PR_FALSE);
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
  NS_IF_ADDREF(*metrics);
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

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetX()
{
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                          (void**)&containerFrame);
  if (containerFrame)
    return containerFrame->GetX();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetY()
{
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                          (void**)&containerFrame);
  if (containerFrame)
    return containerFrame->GetY();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetDx()
{
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                          (void**)&containerFrame);
  if (containerFrame)
    return containerFrame->GetDx();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetDy()
{
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                          (void**)&containerFrame);
  if (containerFrame)
    return containerFrame->GetDy();
  return nsnull;
}

NS_IMETHODIMP_(PRUint16)
nsSVGGlyphFrame::GetTextAnchor()
{
  return GetStyleSVG()->mTextAnchor;
}

NS_IMETHODIMP_(PRBool)
nsSVGGlyphFrame::IsAbsolutelyPositioned()
{
  nsIFrame *lastFrame = this;

  for (nsIFrame *frame = this->GetParent();
       frame != nsnull;
       lastFrame = frame, frame = frame->GetParent()) {

    /* need to be the first child if we are absolutely positioned */
    if (!frame ||
        frame->GetFirstChild(nsnull) != lastFrame)
      break;

    // textPath is always absolutely positioned for our purposes
    if (frame->GetType() == nsLayoutAtoms::svgTextPathFrame)
      return PR_TRUE;
        
    if (frame &&
        (frame->GetContent()->HasAttr(kNameSpaceID_None, nsSVGAtoms::x) ||
         frame->GetContent()->HasAttr(kNameSpaceID_None, nsSVGAtoms::y)))
        return PR_TRUE;

    if (frame->GetType() == nsLayoutAtoms::svgTextFrame)
      break;
  }

  return PR_FALSE;
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

  if (GetStateBits() & NS_STATE_SVG_METRICS_DIRTY) {
    PRBool metricsDirty = PR_FALSE;
    if (mMetrics)
      mMetrics->Update(&metricsDirty);
    if (metricsDirty) {
      AddStateBits(NS_STATE_SVG_DIRTY);
      nsISVGTextFrame* text_frame = GetTextFrame();
      NS_ASSERTION(text_frame, "null text frame");
      if (text_frame)
        text_frame->NotifyGlyphMetricsChange(this);
    }
    RemoveStateBits(NS_STATE_SVG_METRICS_DIRTY);
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

void nsSVGGlyphFrame::UpdateGeometry(PRBool bRedraw,
                                     PRBool suppressInvalidation)
{
  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }
  
  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (suspended) {
    AddStateBits(NS_STATE_SVG_DIRTY);
  } else {
    NS_ASSERTION(!(GetStateBits() & NS_STATE_SVG_METRICS_DIRTY),
                 "dirty metrics in nsSVGGlyphFrame::UpdateGeometry");
    NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::UpdateGeometry");
    RemoveStateBits(NS_STATE_SVG_DIRTY);

    if (suppressInvalidation)
      return;

    outerSVGFrame->InvalidateRect(mRect);
    mGeometry->GetCoveredRegion(this, &mRect);

    nsRect filterRect;
    filterRect = nsSVGUtils::FindFilterInvalidation(this);
    if (!filterRect.IsEmpty()) {
      outerSVGFrame->InvalidateRect(filterRect);
    } else {
      outerSVGFrame->InvalidateRect(mRect);
    }
  }  
}

void nsSVGGlyphFrame::UpdateMetrics()
{
  nsISVGTextFrame* text_frame = GetTextFrame();
  if (!text_frame) {
    NS_ERROR("null text_frame");
    return;
  }
  
  PRBool suspended = text_frame->IsMetricsSuspended();
  if (suspended) {
    AddStateBits(NS_STATE_SVG_METRICS_DIRTY);
  } else {
    NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::UpdateMetrics");
    PRBool metricsDirty;
    mMetrics->Update(&metricsDirty);
    if (metricsDirty) {
      AddStateBits(NS_STATE_SVG_DIRTY);
      text_frame->NotifyGlyphMetricsChange(this);
    }
    RemoveStateBits(NS_STATE_SVG_METRICS_DIRTY);
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

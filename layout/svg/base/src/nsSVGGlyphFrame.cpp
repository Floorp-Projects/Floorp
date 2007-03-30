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

#include "nsSVGOuterSVGFrame.h"
#include "nsSVGTextFrame.h"
#include "nsILookAndFeel.h"
#include "nsTextFragment.h"
#include "nsSVGUtils.h"
#include "nsIDOMSVGLengthList.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGPoint.h"
#include "nsSVGGlyphFrame.h"
#include "nsSVGTextPathFrame.h"
#include "nsSVGPathElement.h"
#include "nsSVGPoint.h"
#include "nsSVGRect.h"
#include "nsDOMError.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "cairo.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parentFrame, nsStyleContext* aContext)
{
  NS_ASSERTION(parentFrame, "null parent");
  nsISVGTextContentMetrics *metrics;
  CallQueryInterface(parentFrame, &metrics);
  NS_ASSERTION(metrics, "trying to construct an SVGGlyphFrame for an invalid container");
  
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "trying to construct an SVGGlyphFrame for wrong content element");

  return new (aPresShell) nsSVGGlyphFrame(aContext);
}

nsSVGGlyphFrame::nsSVGGlyphFrame(nsStyleContext* aContext)
    : nsSVGGlyphFrameBase(aContext),
      mWhitespaceHandling(COMPRESS_WHITESPACE)
{
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGlyphFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentLeaf)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentNode)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGlyphFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

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

  nsSVGTextContainerFrame *containerFrame =
    NS_STATIC_CAST(nsSVGTextContainerFrame *, mParent);
  if (containerFrame)
    containerFrame->UpdateGraphic();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::DidSetStyleContext()
{
  nsSVGGlyphFrameBase::DidSetStyleContext();

  return UpdateGraphic();
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
  return nsGkAtoms::svgGlyphFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

void
nsSVGGlyphFrame::LoopCharacters(cairo_t *aCtx, const nsAString &aText,
                                const nsSVGCharacterPosition *aCP,
                                void (*aFunc)(cairo_t *cr, const char *utf8))
{
  if (!aCP) {
    aFunc(aCtx, NS_ConvertUTF16toUTF8(aText).get());
  } else {
    for (PRUint32 i = 0; i < aText.Length(); i++) {
      /* character actually on the path? */
      if (aCP[i].draw == PR_FALSE)
        continue;
      cairo_matrix_t matrix;
      cairo_get_matrix(aCtx, &matrix);
      cairo_move_to(aCtx, aCP[i].x, aCP[i].y);
      cairo_rotate(aCtx, aCP[i].angle);
      aFunc(aCtx, NS_ConvertUTF16toUTF8(Substring(aText, i, 1)).get());
      cairo_set_matrix(aCtx, &matrix);
    }
  }
}

NS_IMETHODIMP
nsSVGGlyphFrame::PaintSVG(nsSVGRenderState *aContext, nsRect *aDirtyRect)
{
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  nsAutoString text;
  if (!GetCharacterData(text)) {
    return NS_OK;
  }

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  gfxContext *gfx = aContext->GetGfxContext();
  cairo_t *ctx = gfx->GetCairo();

  SelectFont(gfx);

  nsresult rv = GetCharacterPosition(gfx, text, getter_Transfers(cp));

  cairo_matrix_t matrix;

  PRUint16 renderMode = aContext->GetRenderMode();

  if (renderMode == nsSVGRenderState::NORMAL) {
    /* save/pop the state so we don't screw up the xform */
    cairo_save(ctx);
  }
  else {
    cairo_get_matrix(ctx, &matrix);
  }

  rv = GetGlobalTransform(gfx);
  if (NS_FAILED(rv)) {
    if (renderMode == nsSVGRenderState::NORMAL)
      cairo_restore(ctx);
    return rv;
  }

  if (!cp)
    cairo_move_to(ctx, mX, mY);

  if (renderMode != nsSVGRenderState::NORMAL) {
    if (GetClipRule() == NS_STYLE_FILL_RULE_EVENODD)
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_EVEN_ODD);
    else
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_WINDING);

    if (renderMode == nsSVGRenderState::CLIP_MASK) {
      cairo_set_antialias(ctx, CAIRO_ANTIALIAS_NONE);
      cairo_set_source_rgba(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
      LoopCharacters(ctx, text, cp, cairo_show_text);
    } else {
      LoopCharacters(ctx, text, cp, cairo_text_path);
    }

    cairo_set_matrix(ctx, &matrix);

    return NS_OK;
  }

  void *closure;
  if (HasFill() && NS_SUCCEEDED(SetupCairoFill(gfx, &closure))) {
    LoopCharacters(ctx, text, cp, cairo_show_text);
    CleanupCairoFill(gfx, closure);
  }

  if (HasStroke() && NS_SUCCEEDED(SetupCairoStroke(gfx, &closure))) {
    cairo_new_path(ctx);
    if (!cp)
      cairo_move_to(ctx, mX, mY);
    LoopCharacters(ctx, text, cp, cairo_text_path);
    cairo_stroke(ctx);
    CleanupCairoStroke(gfx, closure);
    cairo_new_path(ctx);
  }

  cairo_restore(ctx);

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

  if (!mRect.Contains(nscoord(x), nscoord(y)))
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

  PRBool isHit = ContainsPoint(x, y);
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
  mRect.Empty();

  PRBool hasFill = HasFill();
  PRBool hasStroke = HasStroke();

  if (!hasFill && !hasStroke) {
    return NS_OK;
  }

  nsAutoString text;
  if (!GetCharacterData(text)) {
    return NS_OK;
  }

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));

  nsresult rv = GetGlobalTransform(ctx);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!cp) {
    cairo_move_to(ctx, mX, mY);

    if (hasStroke) {
      cairo_text_path(ctx, NS_ConvertUTF16toUTF8(text).get());
    } else {
      cairo_text_extents_t extent;
      cairo_text_extents(ctx,
                         NS_ConvertUTF16toUTF8(text).get(),
                         &extent);
      cairo_rectangle(ctx, mX + extent.x_bearing, mY + extent.y_bearing,
                      extent.width, extent.height);
    }
  } else {
    cairo_matrix_t matrix;
    for (PRUint32 i=0; i<text.Length(); i++) {
      /* character actually on the path? */
      if (cp[i].draw == PR_FALSE)
        continue;
      cairo_get_matrix(ctx, &matrix);
      cairo_move_to(ctx, cp[i].x, cp[i].y);
      cairo_rotate(ctx, cp[i].angle);
      if (hasStroke) {
        cairo_text_path(ctx, NS_ConvertUTF16toUTF8(Substring(text, i, 1)).get());
      } else {
        cairo_text_extents_t extent;
        cairo_text_extents(ctx,
                           NS_ConvertUTF16toUTF8(Substring(text, i, 1)).get(),
                           &extent);
        cairo_rel_move_to(ctx, extent.x_bearing, extent.y_bearing);
        cairo_rel_line_to(ctx, extent.width, 0);
        cairo_rel_line_to(ctx, 0, extent.height);
        cairo_rel_line_to(ctx, -extent.width, 0);
        cairo_close_path(ctx);
      }
      cairo_set_matrix(ctx, &matrix);
    }
  }

  double xmin, ymin, xmax, ymax;

  if (hasStroke) {
    SetupCairoStrokeGeometry(ctx);
    cairo_stroke_extents(ctx, &xmin, &ymin, &xmax, &ymax);
    nsSVGUtils::UserToDeviceBBox(ctx, &xmin, &ymin, &xmax, &ymax);
  } else {
    cairo_identity_matrix(ctx);
    cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);
  }

  mRect = nsSVGUtils::ToBoundingPixelRect(xmin, ymin, xmax, ymax);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::InitialUpdate()
{
  nsresult rv = UpdateGraphic();

  NS_ASSERTION(!(mState & NS_FRAME_IN_REFLOW),
               "We don't actually participate in reflow");
  
  // Do unset the various reflow bits, though.
  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);
  
  return rv;
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
  if (GetStateBits() & NS_STATE_SVG_DIRTY)
    UpdateGeometry(PR_TRUE, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  if (!GetCharacterData(text)) {
    return NS_OK;
  }

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));

  nsresult rv = GetGlobalTransform(ctx);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!cp)
    cairo_move_to(ctx, mX, mY);

  LoopCharacters(ctx, text, cp, cairo_text_path);

  cairo_identity_matrix(ctx);

  double xmin, ymin, xmax, ymax;

  cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);

  return NS_NewSVGRect(_retval, xmin, ymin, xmax - xmin, ymax - ymin);
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
// nsSVGGlyphFrame methods:

PRBool
nsSVGGlyphFrame::GetCharacterData(nsAString & aCharacterData)
{
  nsAutoString characterData;
  mContent->AppendTextTo(characterData);

  if (mWhitespaceHandling & COMPRESS_WHITESPACE) {
    PRBool trimLeadingWhitespace, trimTrailingWhitespace;
    trimLeadingWhitespace = ((mWhitespaceHandling & TRIM_LEADING_WHITESPACE) != 0);
    trimTrailingWhitespace = ((mWhitespaceHandling & TRIM_TRAILING_WHITESPACE) != 0);
    characterData.CompressWhitespace(trimLeadingWhitespace, 
                                     trimTrailingWhitespace);
  } else {
    nsAString::iterator start, end;
    characterData.BeginWriting(start);
    characterData.EndWriting(end);
    while (start != end) {
      if (NS_IsAsciiWhitespace(*start))
        *start = ' ';
      ++start;
    }
  }
  aCharacterData = characterData;

  return !characterData.IsEmpty();
}

nsresult
nsSVGGlyphFrame::GetCharacterPosition(gfxContext *aContext,
                                      const nsAString &aText,
                                      nsSVGCharacterPosition **aCharacterPosition)
{
  cairo_t *ctx = aContext->GetCairo();

  *aCharacterPosition = nsnull;

  NS_ASSERTION(!aText.IsEmpty(), "no text");

  nsSVGTextPathFrame *textPath = FindTextPathParent();

  /* we're an ordinary fragment - return */
  /* XXX: we might want to use this for individual x/y/dx/dy adjustment */
  if (!textPath)
    return NS_OK;

  nsAutoPtr<nsSVGFlattenedPath> data(textPath->GetFlattenedPath());

  /* textPath frame, but invalid target */
  if (!data)
    return NS_ERROR_FAILURE;

  float length = data->GetLength();
  PRUint32 strLength = aText.Length();

  nsSVGCharacterPosition *cp = new nsSVGCharacterPosition[strLength];

  for (PRUint32 k = 0; k < strLength; k++)
    cp[k].draw = PR_FALSE;

  float x = mX;
  for (PRUint32 i = 0; i < strLength; i++) {

    cairo_text_extents_t extent;

    cairo_text_extents(ctx,
                       NS_ConvertUTF16toUTF8(Substring(aText, i, 1)).get(),
                       &extent);
    float advance = extent.x_advance;

    /* have we run off the end of the path? */
    if (x + advance / 2 > length)
      break;

    /* check that we've advanced to the start of the path */
    if (x + advance / 2 >= 0.0f) {
      cp[i].draw = PR_TRUE;

      // add y (normal)
      // add rotation
      // move point back along tangent
      data->FindPoint(advance, x, mY,
                      &(cp[i].x),
                      &(cp[i].y),
                      &(cp[i].angle));
    }
    x += advance;
  }

  *aCharacterPosition = cp;

  return NS_OK;
}

//----------------------------------------------------------------------

// Utilities for converting from indices in the uncompressed content
// element strings to compressed frame string and back:
static int
CompressIndex(int index, const nsTextFragment*fragment)
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

static int
UncompressIndex(int index, PRBool bRightAffinity, const nsTextFragment*fragment)
{
  // XXX
  return index;
}

nsresult
nsSVGGlyphFrame::GetHighlight(PRUint32 *charnum, PRUint32 *nchars,
                              nscolor *foreground, nscolor *background)
{
  *foreground = NS_RGB(255,255,255);
  *background = NS_RGB(0,0,0); 
  *charnum=0;
  *nchars=0;

  PRBool hasHighlight =
    (mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;

  if (!hasHighlight) {
    NS_ERROR("nsSVGGlyphFrame::GetHighlight() called by renderer when there is no highlight");
    return NS_ERROR_FAILURE;
  }

  nsPresContext *presContext = PresContext();

  // The selection ranges are relative to the uncompressed text in
  // the content element. We'll need the text fragment:
  const nsTextFragment *fragment = mContent->GetText();
  NS_ASSERTION(fragment, "no text");
  
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

NS_IMETHODIMP
nsSVGGlyphFrame::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));

  float x, y;

  if (cp) {
    if (cp[charnum].draw == PR_FALSE) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    x = cp[charnum].x;
    y = cp[charnum].y;

  } else {
    x = mX;
    y = mY;

    if (charnum > 0) {
      cairo_text_extents_t extent;

      cairo_text_extents(ctx,
                         NS_ConvertUTF16toUTF8(Substring(text,
                                                         0,
                                                         charnum)).get(),
                         &extent);

      x += extent.x_advance;
      y += extent.y_advance;
    }
  }

  return NS_NewSVGPoint(_retval, x, y);
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;
  
  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));

  cairo_text_extents_t extent;

  if (cp) {
    if (cp[charnum].draw == PR_FALSE) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    cairo_text_extents(ctx, 
                       NS_ConvertUTF16toUTF8(Substring(text, charnum, 1)).get(),
                       &extent);

    float s = sin(cp[charnum].angle);
    float c = cos(cp[charnum].angle);

    return NS_NewSVGPoint(_retval, 
             cp[charnum].x + extent.x_advance * c - extent.y_advance * s,
             cp[charnum].y + extent.y_advance * c + extent.x_advance * s);
  }

  cairo_text_extents(ctx, 
                     NS_ConvertUTF16toUTF8(Substring(text, 0, charnum + 1)).get(),
                     &extent);

  return NS_NewSVGPoint(_retval, mX + extent.x_advance, mY + extent.y_advance);
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));

  cairo_text_extents_t extent;
  cairo_text_extents(ctx,
                     NS_ConvertUTF16toUTF8(Substring(text, charnum, 1)).get(),
                     &extent);

  if (cp) {
    if (cp[charnum].draw == PR_FALSE) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    cairo_matrix_t matrix;
    cairo_get_matrix(ctx, &matrix);
    cairo_move_to(ctx, cp[charnum].x, cp[charnum].y);
    cairo_rotate(ctx, cp[charnum].angle);

    cairo_rel_move_to(ctx, extent.x_bearing, extent.y_bearing);
    cairo_rel_line_to(ctx, extent.width, 0);
    cairo_rel_line_to(ctx, 0, extent.height);
    cairo_rel_line_to(ctx, -extent.width, 0);
    cairo_close_path(ctx);
    cairo_identity_matrix(ctx);

    double xmin, ymin, xmax, ymax;

    cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);

    cairo_set_matrix(ctx, &matrix);

    return NS_NewSVGRect(_retval, xmin, ymin, xmax - xmin, ymax - ymin);
  }

  float x = mX;
  float y = mY;

  x += extent.x_bearing;
  y += extent.y_bearing;

  cairo_text_extents_t precedingExtent;

  if (charnum > 0) {
    // add the space taken up by the text which comes before charnum
    // to the position of the charnum character
    cairo_text_extents(ctx,
                       NS_ConvertUTF16toUTF8(Substring(text,
                                                       0,
                                                       charnum)).get(),
                       &precedingExtent);

    x += precedingExtent.x_advance;
    y += precedingExtent.y_advance;
  }

  return NS_NewSVGRect(_retval, x, y, extent.width, extent.height);
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  const double radPerDeg = M_PI/180.0;

  nsAutoString text;
  GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));

  if (cp) {
    if (cp[charnum].draw == PR_FALSE) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    *_retval = cp[charnum].angle / radPerDeg;
  } else {
    *_retval = 0.0;
  }
  return NS_OK;
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetBaselineOffset(PRUint16 baselineIdentifier)
{
  float _retval;
  cairo_font_extents_t extents;

  nsSVGAutoGlyphHelperContext ctx(this);

  cairo_font_extents(ctx, &extents);

  switch (baselineIdentifier) {
  case BASELINE_HANGING:
    // not really right, but the best we can do with the information provided
    // FALLTHROUGH
  case BASELINE_TEXT_BEFORE_EDGE:
    _retval = -extents.ascent;
    break;
  case BASELINE_TEXT_AFTER_EDGE:
    _retval = extents.descent;
    break;
  case BASELINE_CENTRAL:
  case BASELINE_MIDDLE:
    _retval = - (extents.ascent - extents.descent) / 2.0;
    break;
  case BASELINE_ALPHABETIC:
  default:
    _retval = 0.0;
    break;
  }
  
  return _retval;
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetAdvance()
{
  nsAutoString text;
  if (!GetCharacterData(text)) {
    return 0.0f;
  }

  nsSVGAutoGlyphHelperContext ctx(this);

  cairo_text_extents_t extents;
  cairo_text_extents(ctx, 
                     NS_ConvertUTF16toUTF8(text).get(),
                     &extents);
  
  return extents.x_advance;
}

NS_IMETHODIMP_(nsSVGTextPathFrame*) 
nsSVGGlyphFrame::FindTextPathParent()
{
  /* check if we're the child of a textPath */
  for (nsIFrame *frame = GetParent();
       frame != nsnull;
       frame = frame->GetParent()) {
    nsIAtom* type = frame->GetType();
    if (type == nsGkAtoms::svgTextPathFrame) {
      return NS_STATIC_CAST(nsSVGTextPathFrame*, frame);
    } else if (type == nsGkAtoms::svgTextFrame)
      return nsnull;
  }
  return nsnull;
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

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetX()
{
  nsSVGTextContainerFrame *containerFrame;
  containerFrame = NS_STATIC_CAST(nsSVGTextContainerFrame *, mParent);
  if (containerFrame)
    return containerFrame->GetX();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetY()
{
  nsSVGTextContainerFrame *containerFrame;
  containerFrame = NS_STATIC_CAST(nsSVGTextContainerFrame *, mParent);
  if (containerFrame)
    return containerFrame->GetY();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetDx()
{
  nsSVGTextContainerFrame *containerFrame;
  containerFrame = NS_STATIC_CAST(nsSVGTextContainerFrame *, mParent);
  if (containerFrame)
    return containerFrame->GetDx();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetDy()
{
  nsSVGTextContainerFrame *containerFrame;
  containerFrame = NS_STATIC_CAST(nsSVGTextContainerFrame *, mParent);
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

  for (nsIFrame *frame = GetParent();
       frame != nsnull;
       lastFrame = frame, frame = frame->GetParent()) {

    /* need to be the first child if we are absolutely positioned */
    if (!frame ||
        frame->GetFirstChild(nsnull) != lastFrame)
      break;

    // textPath is always absolutely positioned for our purposes
    if (frame->GetType() == nsGkAtoms::svgTextPathFrame)
      return PR_TRUE;
        
    if (frame &&
        (frame->GetContent()->HasAttr(kNameSpaceID_None, nsGkAtoms::x) ||
         frame->GetContent()->HasAttr(kNameSpaceID_None, nsGkAtoms::y)))
        return PR_TRUE;

    if (frame->GetType() == nsGkAtoms::svgTextFrame)
      break;
  }

  return PR_FALSE;
}


//----------------------------------------------------------------------
// nsISVGGlyphFragmentNode interface:

NS_IMETHODIMP_(PRUint32)
nsSVGGlyphFrame::GetNumberOfChars()
{
  if (mWhitespaceHandling == PRESERVE_WHITESPACE)
    return mContent->TextLength();

  nsAutoString text;
  GetCharacterData(text);
  return text.Length();
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetComputedTextLength()
{
  nsAutoString text;
  if (!GetCharacterData(text)) {
    return 0.0f;
  }

  nsSVGAutoGlyphHelperContext ctx(this);

  cairo_text_extents_t extent;
  cairo_text_extents(ctx,
                     NS_ConvertUTF16toUTF8(text).get(),
                     &extent);

  return fabs(extent.x_advance) + fabs(extent.y_advance);
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetSubStringLength(PRUint32 charnum, PRUint32 fragmentChars)
{
  nsAutoString text;
  GetCharacterData(text);

  nsSVGAutoGlyphHelperContext ctx(this);

  cairo_text_extents_t extent;
  cairo_text_extents(ctx,
                     NS_ConvertUTF16toUTF8(Substring(text, charnum, fragmentChars)).get(),
                     &extent);

  return fabs(extent.x_advance) + fabs(extent.y_advance);
}

NS_IMETHODIMP_(PRInt32)
nsSVGGlyphFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point)
{
  float xPos, yPos;
  point->GetX(&xPos);
  point->GetY(&yPos);

  nsAutoString text;
  GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));

  float x, y;
  if (!cp) {
    x = mX;
    y = mY;
  }

  for (PRUint32 charnum = 0; charnum < text.Length(); charnum++) {
    /* character actually on the path? */
    if (cp && cp[charnum].draw == PR_FALSE)
      continue;

    cairo_matrix_t matrix;
    cairo_get_matrix(ctx, &matrix);
    cairo_new_path(ctx);

    if (cp) {
      cairo_move_to(ctx, cp[charnum].x, cp[charnum].y);
      cairo_rotate(ctx, cp[charnum].angle);
    } else {
      if (charnum > 0) {
        cairo_text_extents_t extent;

        cairo_text_extents(ctx,
                           NS_ConvertUTF16toUTF8(Substring(text,
                                                           0,
                                                           charnum)).get(),
                           &extent);
        cairo_move_to(ctx, x + extent.x_advance, y + extent.y_advance);
      } else {
        cairo_move_to(ctx, x, y);
      }
    }
    cairo_text_extents_t extent;
    cairo_text_extents(ctx,
                       NS_ConvertUTF16toUTF8(Substring(text, charnum, 1)).get(),
                       &extent);

    cairo_rel_move_to(ctx, extent.x_bearing, extent.y_bearing);
    cairo_rel_line_to(ctx, extent.width, 0);
    cairo_rel_line_to(ctx, 0, extent.height);
    cairo_rel_line_to(ctx, -extent.width, 0);
    cairo_close_path(ctx);

    cairo_identity_matrix(ctx);
    if (cairo_in_fill(ctx, xPos, yPos)) {
      return charnum;
    }

    cairo_set_matrix(ctx, &matrix);
  }
  return -1;
}

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
    CallQueryInterface(sibling, &node);
    if (node)
      return node->GetFirstGlyphFragment();
    sibling = sibling->GetNextSibling();
  }

  // no more siblings. go back up the tree.
  
  NS_ASSERTION(mParent, "null parent");
  nsISVGGlyphFragmentNode *node = nsnull;
  CallQueryInterface(mParent, &node);
  return node ? node->GetNextGlyphFragment() : nsnull;
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::SetWhitespaceHandling(PRUint8 aWhitespaceHandling)
{
  mWhitespaceHandling = aWhitespaceHandling;
}

//----------------------------------------------------------------------
//

void nsSVGGlyphFrame::SelectFont(gfxContext *aContext)
{
  cairo_t *ctx = aContext->GetCairo();

  const nsStyleFont* fontData = GetStyleFont();
  nsFont font = fontData->mFont;

  // XXX eventually we will have to treat decorations separately from
  // fonts, because they can have a different color than the current
  // glyph.
  
  NS_ASSERTION(mParent, "no parent");
  nsStyleContext *parentContext = mParent->GetStyleContext();
  NS_ASSERTION(parentContext, "no style context on parent");
  
  PRUint8 styleDecorations =
    parentContext->GetStyleTextReset()->mTextDecoration;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_UNDERLINE)
    font.decorations |= NS_FONT_DECORATION_UNDERLINE;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_OVERLINE)
    font.decorations |= NS_FONT_DECORATION_OVERLINE;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_LINE_THROUGH)
    font.decorations |= NS_FONT_DECORATION_LINE_THROUGH;    
  
  cairo_font_slant_t slant;
  cairo_font_weight_t weight = CAIRO_FONT_WEIGHT_NORMAL;

  switch (font.style) {
  case NS_FONT_STYLE_NORMAL:
    slant = CAIRO_FONT_SLANT_NORMAL;
    break;
  case NS_FONT_STYLE_ITALIC:
    slant = CAIRO_FONT_SLANT_ITALIC;
    break;
  case NS_FONT_STYLE_OBLIQUE:
    slant = CAIRO_FONT_SLANT_OBLIQUE;
    break;
  }

  if (font.weight % 100 == 0) {
    if (font.weight >= 600)
      weight = CAIRO_FONT_WEIGHT_BOLD;
  } else if (font.weight % 100 < 50) {
    weight = CAIRO_FONT_WEIGHT_BOLD;
  }

  nsAutoString family;
  font.GetFirstFamily(family);
  cairo_select_font_face(ctx,
                         NS_ConvertUTF16toUTF8(family).get(),
                         slant,
                         weight);

  // Since SVG has its own scaling, we really don't want
  // fonts in SVG to respond to the browser's "TextZoom"
  // (Ctrl++,Ctrl+-)
  nsPresContext *presContext = PresContext();
  float textZoom = presContext->TextZoom();

  cairo_set_font_size(ctx, presContext->AppUnitsToDevPixels(fontData->mSize) / textZoom);
}

void nsSVGGlyphFrame::UpdateGeometry(PRBool bRedraw,
                                     PRBool suppressInvalidation)
{
  if (GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)
    return;

  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }
  
  if (outerSVGFrame->IsRedrawSuspended()) {
    AddStateBits(NS_STATE_SVG_DIRTY);
  } else {
    RemoveStateBits(NS_STATE_SVG_DIRTY);

    if (suppressInvalidation)
      return;

    outerSVGFrame->InvalidateRect(mRect);
    UpdateCoveredRegion();

    nsRect filterRect;
    filterRect = nsSVGUtils::FindFilterInvalidation(this);
    if (!filterRect.IsEmpty()) {
      outerSVGFrame->InvalidateRect(filterRect);
    } else {
      outerSVGFrame->InvalidateRect(mRect);
    }
  }  
}

PRBool
nsSVGGlyphFrame::ContainsPoint(float x, float y)
{
  nsAutoString text;
  if (!GetCharacterData(text)) {
    return PR_FALSE;
  }

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));

  nsresult rv = GetGlobalTransform(ctx);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  float xx = 0, yy = 0;
  if (!cp) {
    xx = mX;
    yy = mY;
  }

  cairo_matrix_t matrix;

  for (PRUint32 i = 0; i < text.Length(); i++) {
    /* character actually on the path? */
    if (cp && cp[i].draw == PR_FALSE)
      continue;

    cairo_get_matrix(ctx, &matrix);

    if (cp) {
      cairo_move_to(ctx, cp[i].x, cp[i].y);
      cairo_rotate(ctx, cp[i].angle);
    } else {
      cairo_move_to(ctx, xx, yy);
    }

    cairo_text_extents_t extent;
    cairo_text_extents(ctx,
                       NS_ConvertUTF16toUTF8(Substring(text, i, 1)).get(),
                       &extent);
    cairo_rel_move_to(ctx, extent.x_bearing, extent.y_bearing);
    cairo_rel_line_to(ctx, extent.width, 0);
    cairo_rel_line_to(ctx, 0, extent.height);
    cairo_rel_line_to(ctx, -extent.width, 0);
    cairo_close_path(ctx);

    cairo_set_matrix(ctx, &matrix);

    if (!cp) {
      xx += extent.x_advance;
      yy += extent.y_advance;
    }
  }

  cairo_identity_matrix(ctx);
  return cairo_in_fill(ctx, x, y);
}

nsresult
nsSVGGlyphFrame::GetGlobalTransform(gfxContext *aContext)
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm;
  GetCanvasTM(getter_AddRefs(ctm));
  NS_ASSERTION(ctm, "graphic source didn't specify a ctm");

  gfxMatrix matrix = nsSVGUtils::ConvertSVGMatrixToThebes(ctm);

  if (matrix.IsSingular()) {
    aContext->IdentityMatrix();
    return NS_ERROR_FAILURE;
  }

  aContext->Multiply(matrix);

  return NS_OK;
}

//----------------------------------------------------------------------
// helper class

nsSVGGlyphFrame::nsSVGAutoGlyphHelperContext::nsSVGAutoGlyphHelperContext(
    nsSVGGlyphFrame *aSource,
    const nsAString &aText,
    nsSVGCharacterPosition **cp)
{
  Init(aSource);

  nsresult rv = aSource->GetCharacterPosition(mCT, aText, cp);
  if NS_FAILED(rv) {
    NS_WARNING("failed to get character position data");
  }
}

void nsSVGGlyphFrame::nsSVGAutoGlyphHelperContext::Init(nsSVGGlyphFrame *aSource)
{
  mCT = new gfxContext(nsSVGUtils::GetThebesComputationalSurface());
  aSource->SelectFont(mCT);
}

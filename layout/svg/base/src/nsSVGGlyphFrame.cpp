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
#include "gfxPlatform.h"

// XXX: This initial straightforward conversion from accessing cairo
// directly to Thebes doesn't handle clusters.  Pretty much all code
// that measures or draws single characters (textPath code and some
// DOM accessors) will need to be reworked.

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

  const nsStyleFont* fontData = GetStyleFont();
  nsFont font = fontData->mFont;

  // Since SVG has its own scaling, we really don't want
  // fonts in SVG to respond to the browser's "TextZoom"
  // (Ctrl++,Ctrl+-)
  nsPresContext *presContext = PresContext();
  float textZoom = presContext->TextZoom();
  double size = presContext->AppUnitsToDevPixels(fontData->mSize) / textZoom;

  nsCAutoString langGroup;
  nsIAtom *langGroupAtom = presContext->GetLangGroup();
  if (langGroupAtom) {
    const char* lg;
    langGroupAtom->GetUTF8String(&lg);
    langGroup.Assign(lg);
  }

  // XXX decorations are ignored by gfxFontStyle - still need to implement
  mFontStyle = new gfxFontStyle(font.style, font.variant,
                                font.weight, font.decorations,
                                size, langGroup, font.sizeAdjust,
                                font.systemFont, font.familyNameQuirks);

  if (mFontStyle) {
    mFontGroup =
      gfxPlatform::GetPlatform()->CreateFontGroup(font.name, mFontStyle);
  }

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
nsSVGGlyphFrame::LoopCharacters(gfxContext *aCtx, const nsString &aText,
                                const nsSVGCharacterPosition *aCP,
                                FillOrStroke aFillOrStroke)
{
  nsAutoPtr<gfxTextRun> textRun(GetTextRun(aCtx, aText));

  if (!textRun)
    return;

  if (!aCP) {
    if (aFillOrStroke == STROKE) {
      textRun->DrawToPath(aCtx, mPosition, 0, aText.Length(), nsnull, nsnull);
    } else {
      textRun->Draw(aCtx, mPosition, 0, aText.Length(),
                    nsnull, nsnull, nsnull);
    }
  } else {
    for (PRUint32 i = 0; i < aText.Length(); i++) {
      /* character actually on the path? */
      if (aCP[i].draw == PR_FALSE)
        continue;

      gfxMatrix matrix = aCtx->CurrentMatrix();

      gfxMatrix rot;
      rot.Rotate(aCP[i].angle);
      aCtx->Multiply(rot);

      rot.Invert();
      gfxPoint pt = rot.Transform(aCP[i].pos);

      if (aFillOrStroke == STROKE) {
        textRun->DrawToPath(aCtx, pt, i, 1, nsnull, nsnull);
      } else {
        textRun->Draw(aCtx, pt, i, 1, nsnull, nsnull, nsnull);
      }
      aCtx->SetMatrix(matrix);
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

  nsresult rv = GetCharacterPosition(gfx, text, getter_Transfers(cp));

  gfxMatrix matrix;

  PRUint16 renderMode = aContext->GetRenderMode();

  if (renderMode == nsSVGRenderState::NORMAL) {
    /* save/pop the state so we don't screw up the xform */
    gfx->Save();
  }
  else {
    matrix = gfx->CurrentMatrix();
  }

  rv = GetGlobalTransform(gfx);
  if (NS_FAILED(rv)) {
    if (renderMode == nsSVGRenderState::NORMAL)
      gfx->Restore();
    return rv;
  }

  if (renderMode != nsSVGRenderState::NORMAL) {
    if (GetClipRule() == NS_STYLE_FILL_RULE_EVENODD)
      gfx->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
    else
      gfx->SetFillRule(gfxContext::FILL_RULE_WINDING);

    if (renderMode == nsSVGRenderState::CLIP_MASK) {
      gfx->SetAntialiasMode(gfxContext::MODE_ALIASED);
      gfx->SetColor(gfxRGBA(1.0f, 1.0f, 1.0f, 1.0f));
      LoopCharacters(gfx, text, cp, FILL);
    } else {
      LoopCharacters(gfx, text, cp, STROKE);
    }

    gfx->SetMatrix(matrix);

    return NS_OK;
  }

  void *closure;
  if (HasFill() && NS_SUCCEEDED(SetupCairoFill(gfx, &closure))) {
    LoopCharacters(gfx, text, cp, FILL);
    CleanupCairoFill(gfx, closure);
  }

  if (HasStroke() && NS_SUCCEEDED(SetupCairoStroke(gfx, &closure))) {
    gfx->NewPath();
    LoopCharacters(gfx, text, cp, STROKE);
    gfx->Stroke();
    CleanupCairoStroke(gfx, closure);
    gfx->NewPath();
  }

  gfx->Restore();

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

  gfxContext *gfx = ctx.GetContext();
  gfxTextRun *textRun = ctx.GetTextRun();
  if (!gfx || !textRun)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = GetGlobalTransform(gfx);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!cp) {
    if (hasStroke) {
      textRun->DrawToPath(gfx, mPosition, 0, text.Length(), nsnull, nsnull);
    } else {
      gfxTextRun::Metrics metrics =
        textRun->MeasureText(0, text.Length(), PR_FALSE, nsnull);
      gfx->Rectangle(metrics.mBoundingBox + mPosition);
    }
  } else {
    for (PRUint32 i=0; i<text.Length(); i++) {
      /* character actually on the path? */
      if (cp[i].draw == PR_FALSE)
        continue;

      gfxMatrix matrix = gfx->CurrentMatrix();

      if (hasStroke) {
        gfxMatrix rot;
        rot.Rotate(cp[i].angle);
        gfx->Multiply(rot);

        rot.Invert();
        gfxPoint pt = rot.Transform(cp[i].pos);

        textRun->DrawToPath(gfx, pt, i, 1, nsnull, nsnull);
      } else {
        gfx->MoveTo(cp[i].pos);
        gfx->Rotate(cp[i].angle);

        gfxTextRun::Metrics metrics =
          textRun->MeasureText(i, 1, PR_FALSE, nsnull);

        gfx->Rectangle(metrics.mBoundingBox + gfx->CurrentPoint());
      }
      gfx->SetMatrix(matrix);
    }
  }

  gfxRect extent;

  if (hasStroke) {
    SetupCairoStrokeGeometry(gfx);
    extent = gfx->GetUserStrokeExtent();
    extent = gfx->UserToDevice(extent);
  } else {
    gfx->IdentityMatrix();
    extent = gfx->GetUserFillExtent();
  }

  mRect = nsSVGUtils::ToBoundingPixelRect(extent);

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

  gfxContext *gfx = ctx.GetContext();
  if (!gfx)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = GetGlobalTransform(gfx);
  NS_ENSURE_SUCCESS(rv, rv);

  LoopCharacters(gfx, text, cp, STROKE);
  gfx->IdentityMatrix();
  gfxRect rect = gfx->GetUserFillExtent();

  return NS_NewSVGRect(_retval, rect);
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
                                      const nsString &aText,
                                      nsSVGCharacterPosition **aCharacterPosition)
{
  *aCharacterPosition = nsnull;

  NS_ASSERTION(!aText.IsEmpty(), "no text");

  nsSVGTextPathFrame *textPath = FindTextPathParent();

  /* we're an ordinary fragment - return */
  /* XXX: we might want to use this for individual x/y/dx/dy adjustment */
  if (!textPath)
    return NS_OK;

  nsRefPtr<gfxFlattenedPath> data = textPath->GetFlattenedPath();

  /* textPath frame, but invalid target */
  if (!data)
    return NS_ERROR_FAILURE;

  float length = data->GetLength();
  PRUint32 strLength = aText.Length();

  nsAutoPtr<gfxTextRun> textRun(GetTextRun(aContext, aText));
  if (!textRun)
    return NS_ERROR_OUT_OF_MEMORY;

  nsSVGCharacterPosition *cp = new nsSVGCharacterPosition[strLength];

  for (PRUint32 k = 0; k < strLength; k++)
    cp[k].draw = PR_FALSE;

  float x = mPosition.x;
  for (PRUint32 i = 0; i < strLength; i++) {
    float halfAdvance = textRun->GetAdvanceWidth(i, 1, nsnull) / 2.0;

    /* have we run off the end of the path? */
    if (x + halfAdvance > length)
      break;

    /* check that we've advanced to the start of the path */
    if (x + halfAdvance >= 0.0f) {
      cp[i].draw = PR_TRUE;

      // add y (normal)
      // add rotation
      // move point back along tangent
      gfxPoint pt = data->FindPoint(gfxPoint(x + halfAdvance, mPosition.y),
                                    &(cp[i].angle));
      cp[i].pos =
        pt - gfxPoint(cos(cp[i].angle), sin(cp[i].angle)) * halfAdvance;
    }
    x += 2 * halfAdvance;
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
  mPosition.MoveTo(x, y);
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

  gfxPoint pt;

  if (cp) {
    if (cp[charnum].draw == PR_FALSE) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    pt = cp[charnum].pos;
  } else {
    pt = mPosition;

    if (charnum > 0) {
      gfxTextRun *textRun = ctx.GetTextRun();
      if (!textRun)
        return NS_ERROR_OUT_OF_MEMORY;
      pt.x += textRun->GetAdvanceWidth(0, charnum, nsnull);
    }
  }

  return NS_NewSVGPoint(_retval, pt.x, pt.y);
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));
  gfxTextRun *textRun = ctx.GetTextRun();
  if (!textRun)
    return NS_ERROR_OUT_OF_MEMORY;

  if (cp) {
    if (cp[charnum].draw == PR_FALSE) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    float advance = textRun->GetAdvanceWidth(charnum, 1, nsnull);

    return NS_NewSVGPoint(_retval,
                          cp[charnum].pos.x + advance * cos(cp[charnum].angle),
                          cp[charnum].pos.y + advance * sin(cp[charnum].angle));
  }

  return NS_NewSVGPoint(_retval,
                        mPosition.x + textRun->GetAdvanceWidth(0,
                                                               charnum + 1,
                                                               nsnull),
                        mPosition.y);
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(this, text, getter_Transfers(cp));
  gfxTextRun *textRun = ctx.GetTextRun();
  if (!textRun)
    return NS_ERROR_OUT_OF_MEMORY;

  gfxTextRun::Metrics metrics =
    textRun->MeasureText(charnum, 1, PR_FALSE, nsnull);

  if (cp) {
    if (cp[charnum].draw == PR_FALSE) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    gfxContext *gfx = ctx.GetContext();
    if (!gfx)
      return NS_ERROR_OUT_OF_MEMORY;

    gfxMatrix matrix = gfx->CurrentMatrix();

    gfx->MoveTo(cp[charnum].pos);
    gfx->Rotate(cp[charnum].angle);

    gfx->Rectangle(metrics.mBoundingBox + gfx->CurrentPoint());

    gfx->IdentityMatrix();

    gfxRect rect = gfx->GetUserFillExtent();

    gfx->SetMatrix(matrix);

    return NS_NewSVGRect(_retval, rect);
  }

  gfxPoint pt = mPosition;

  if (charnum > 0) {
    // add the space taken up by the text which comes before charnum
    // to the position of the charnum character
    pt.x += textRun->GetAdvanceWidth(0, charnum, nsnull);
  }

  return NS_NewSVGRect(_retval, metrics.mBoundingBox + pt);
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

  nsAutoString text;
  GetCharacterData(text);

  nsSVGAutoGlyphHelperContext ctx(this, text);
  gfxTextRun *textRun = ctx.GetTextRun();
  if (!textRun)
    return 0.0f;

  gfxTextRun::Metrics metrics =
    textRun->MeasureText(0, text.Length(), PR_FALSE, nsnull);

  switch (baselineIdentifier) {
  case BASELINE_HANGING:
    // not really right, but the best we can do with the information provided
    // FALLTHROUGH
  case BASELINE_TEXT_BEFORE_EDGE:
    _retval = -metrics.mAscent;
    break;
  case BASELINE_TEXT_AFTER_EDGE:
    _retval = metrics.mDescent;
    break;
  case BASELINE_CENTRAL:
  case BASELINE_MIDDLE:
    _retval = - (metrics.mAscent - metrics.mDescent) / 2.0;
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

  nsSVGAutoGlyphHelperContext ctx(this, text);
  gfxTextRun *textRun = ctx.GetTextRun();
  if (!textRun)
    return 0.0f;

  return textRun->GetAdvanceWidth(0, text.Length(), nsnull);
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
  return GetAdvance();
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetSubStringLength(PRUint32 charnum, PRUint32 fragmentChars)
{
  nsAutoString text;
  GetCharacterData(text);

  nsSVGAutoGlyphHelperContext ctx(this, text);
  gfxTextRun *textRun = ctx.GetTextRun();
  if (!textRun)
    return 0.0f;

  return textRun->GetAdvanceWidth(charnum, fragmentChars, nsnull);
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

  gfxPoint pt;
  if (!cp) {
    pt = mPosition;
  }

  gfxContext *gfx = ctx.GetContext();
  gfxTextRun *textRun = ctx.GetTextRun();
  if (!gfx || !textRun)
    return NS_ERROR_OUT_OF_MEMORY;

  for (PRUint32 charnum = 0; charnum < text.Length(); charnum++) {
    /* character actually on the path? */
    if (cp && cp[charnum].draw == PR_FALSE)
      continue;

    gfxMatrix matrix = gfx->CurrentMatrix();
    gfx->NewPath();

    if (cp) {
      gfx->MoveTo(cp[charnum].pos);
      gfx->Rotate(cp[charnum].angle);
    } else {
      if (charnum > 0) {
        gfx->MoveTo(pt + gfxPoint(textRun->GetAdvanceWidth(0, charnum, nsnull),
                                  0));
      } else {
        gfx->MoveTo(pt);
      }
    }

    gfxTextRun::Metrics metrics =
      textRun->MeasureText(charnum, 1, PR_FALSE, nsnull);

    gfx->Rectangle(metrics.mBoundingBox + gfx->CurrentPoint());

    gfx->IdentityMatrix();
    if (gfx->PointInFill(gfxPoint(xPos, yPos))) {
      gfx->SetMatrix(matrix);
      return charnum;
    }

    gfx->SetMatrix(matrix);
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
  gfxContext *gfx = ctx.GetContext();
  gfxTextRun *textRun = ctx.GetTextRun();
  if (!gfx || !textRun)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = GetGlobalTransform(gfx);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  gfxPoint pt;
  if (!cp) {
    pt = mPosition;
  }

  for (PRUint32 i = 0; i < text.Length(); i++) {
    /* character actually on the path? */
    if (cp && cp[i].draw == PR_FALSE)
      continue;

    gfxMatrix matrix = gfx->CurrentMatrix();

    if (cp) {
      gfx->MoveTo(cp[i].pos);
      gfx->Rotate(cp[i].angle);
    } else {
      gfx->MoveTo(pt);
    }

    gfxTextRun::Metrics metrics =
      textRun->MeasureText(i, 1, PR_FALSE, nsnull);

    gfx->Rectangle(metrics.mBoundingBox + gfx->CurrentPoint());

    gfx->SetMatrix(matrix);

    if (!cp) {
      pt.x += metrics.mAdvanceWidth;
    }
  }

  gfx->IdentityMatrix();
  return gfx->PointInFill(gfxPoint(x, y));
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

gfxTextRun *
nsSVGGlyphFrame::GetTextRun(gfxContext *aCtx, const nsString &aText)
{
  // XXX: should really pass in GetPresContext()->AppUnitsPerDevPixel()
  // instead of "1" and do the appropriate unit conversions when sending
  // coordinates into thebes and pulling metrics out.
  //
  // References:
  //   https://bugzilla.mozilla.org/show_bug.cgi?id=375141
  //   http://weblogs.mozillazine.org/roc/archives/2007/03/text_text_text.html

  gfxTextRunFactory::Parameters params =
    { aCtx, nsnull, nsnull,
      nsnull, nsnull, nsnull,
      1, // see note above
      0 };

  if (!mFontGroup)
    return nsnull;

  return mFontGroup->MakeTextRun(aText.get(), aText.Length(), &params);
}

//----------------------------------------------------------------------
// helper class

nsSVGGlyphFrame::nsSVGAutoGlyphHelperContext::nsSVGAutoGlyphHelperContext(
    nsSVGGlyphFrame *aSource,
    const nsString &aText,
    nsSVGCharacterPosition **cp)
{
  Init(aSource, aText);

  nsresult rv = aSource->GetCharacterPosition(mCT, aText, cp);
  if NS_FAILED(rv) {
    NS_WARNING("failed to get character position data");
  }
}

void
nsSVGGlyphFrame::nsSVGAutoGlyphHelperContext::Init(nsSVGGlyphFrame *aSource,
                                                   const nsString &aText)
{
  mCT = new gfxContext(nsSVGUtils::GetThebesComputationalSurface());
  mTextRun = aSource->GetTextRun(mCT, aText);
}

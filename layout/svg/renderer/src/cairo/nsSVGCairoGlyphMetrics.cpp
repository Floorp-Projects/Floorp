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
 * The Original Code is the Mozilla SVG Cairo Renderer project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Parts of this file contain code derived from the following files(s)
 * of the Mozilla SVG project (these parts are Copyright (C) by their
 * respective copyright-holders):
 *    layout/svg/renderer/src/gdiplus/nsSVGGDIPlusGlyphMetrics.cpp
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

#include "nsCOMPtr.h"
#include "nsISVGGlyphMetricsSource.h"
#include "nsPromiseFlatString.h"
#include "nsFont.h"
#include "nsPresContext.h"
#include "nsISVGCairoGlyphMetrics.h"
#include "nsSVGCairoGlyphMetrics.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGTypeCIDs.h"
#include "nsIComponentManager.h"
#include <cairo.h>

/**
 * \addtogroup gdiplus_renderer Cairo Rendering Engine
 * @{
 */

/**
 * \addtogroup gdiplus_renderer Cairo Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 *  Cairo glyph metrics implementation
 */
class nsSVGCairoGlyphMetrics : public nsISVGCairoGlyphMetrics
{
protected:
  friend nsresult NS_NewSVGCairoGlyphMetrics(nsISVGRendererGlyphMetrics **result,
                                             nsISVGGlyphMetricsSource *src);
  nsSVGCairoGlyphMetrics(nsISVGGlyphMetricsSource *src);
  ~nsSVGCairoGlyphMetrics();
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphMetrics interface:
  NS_DECL_NSISVGRENDERERGLYPHMETRICS

  NS_IMETHOD_(void) GetSubBoundingBox(PRUint32 charoffset, PRUint32 count,
                                      nsIDOMSVGRect * *aBoundingBox);

  NS_IMETHOD_(cairo_font_t*)GetFont() { return mFont; }

private:
  cairo_t *mCT;
  cairo_font_t *mFont;
  cairo_text_extents_t mExtents;
  nsCOMPtr<nsISVGGlyphMetricsSource> mSource;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGCairoGlyphMetrics::nsSVGCairoGlyphMetrics(nsISVGGlyphMetricsSource *src)
  : mFont(NULL), mSource(src)
{
  mCT = cairo_create();
}

nsSVGCairoGlyphMetrics::~nsSVGCairoGlyphMetrics()
{
  // don't delete mFont because the cairo_destroy takes it down
  cairo_destroy(mCT);
}

nsresult
NS_NewSVGCairoGlyphMetrics(nsISVGRendererGlyphMetrics **result,
                           nsISVGGlyphMetricsSource *src)
{
  *result = new nsSVGCairoGlyphMetrics(src);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*result);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGCairoGlyphMetrics)
NS_IMPL_RELEASE(nsSVGCairoGlyphMetrics)

NS_INTERFACE_MAP_BEGIN(nsSVGCairoGlyphMetrics)
NS_INTERFACE_MAP_ENTRY(nsISVGRendererGlyphMetrics)
NS_INTERFACE_MAP_ENTRY(nsISVGCairoGlyphMetrics)
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererGlyphMetrics methods:

/** Implements float getBaselineOffset(in unsigned short baselineIdentifier); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetBaselineOffset(PRUint16 baselineIdentifier, float *_retval)
{
  switch (baselineIdentifier) {
  case BASELINE_TEXT_BEFORE_EDGE:
    *_retval = mExtents.y_bearing;
    break;
  case BASELINE_TEXT_AFTER_EDGE:
    *_retval = (float)(PRUint16)(mExtents.y_bearing + mExtents.height + 0.5);
    break;
  case BASELINE_CENTRAL:
  case BASELINE_MIDDLE:
    *_retval = (float)(PRUint16)(mExtents.y_bearing + mExtents.height/2.0 + 0.5);
    break;
  case BASELINE_ALPHABETIC:
  default:
  {
    // xxxx
    *_retval = 0.0;
#if 0
    FontFamily family;
    GetFont()->GetFamily(&family);
    INT style = GetFont()->GetStyle();
    // alternatively to rounding here, we could set the
    // pixeloffsetmode to 'PixelOffsetModeHalf' on painting
    *_retval = (float)(UINT16)(GetBoundingRect()->Y
                               + GetFont()->GetSize()
                               *family.GetCellAscent(style)/family.GetEmHeight(style)
                               + 0.5);
#endif
  }
  break;
  }
  
  return NS_OK;
}


/** Implements readonly attribute float #advance; */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetAdvance(float *aAdvance)
{
  *aAdvance = mExtents.x_advance;
  return NS_OK;
}


// XXX
NS_IMETHODIMP_(void)
nsSVGCairoGlyphMetrics::GetSubBoundingBox(PRUint32 charoffset, PRUint32 count,
                                          nsIDOMSVGRect * *aBoundingBox)
{
  *aBoundingBox = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);

  NS_ASSERTION(rect, "could not create rect");
  if (!rect) {
    *aBoundingBox = NULL;
    return;
  }

  cairo_text_extents_t extents;
  nsAutoString text;
  mSource->GetCharacterData(text);
  cairo_text_extents(mCT,
                     (unsigned char *)NS_ConvertUCS2toUTF8(Substring(text,
                                                                     charoffset,
                                                                     count)).get(),
                     &extents);

  
  rect->SetX(extents.x_bearing);
  rect->SetY(extents.y_bearing);
  rect->SetWidth(extents.width);
  rect->SetHeight(extents.height);

  *aBoundingBox = rect;
  NS_ADDREF(*aBoundingBox);
}


/** Implements readonly attribute nsIDOMSVGRect #boundingBox; */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetBoundingBox(nsIDOMSVGRect * *aBoundingBox)
{
  *aBoundingBox = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);

  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;
  
  rect->SetX(mExtents.x_bearing);
  rect->SetY(mExtents.y_bearing);
  rect->SetWidth(mExtents.width);
  rect->SetHeight(mExtents.height);

  *aBoundingBox = rect;
  NS_ADDREF(*aBoundingBox);
  
  return NS_OK;
}

/** Implements nsIDOMSVGRect getExtentOfChar(in unsigned long charnum); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  cairo_glyph_t glyph;
  cairo_text_extents_t extent;
  glyph.x = glyph.y = 0;

  nsAutoString text;
  mSource->GetCharacterData(text);
  glyph.index = text[charnum];

  cairo_set_font(mCT, mFont);
  cairo_glyph_extents(mCT, &glyph, 1, &extent);

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);

  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;
  
  rect->SetX(extent.x_bearing);
  rect->SetY(extent.y_bearing);
  rect->SetWidth(extent.width);
  rect->SetHeight(extent.height);

  *_retval = rect;
  NS_ADDREF(*_retval);
  
  return NS_OK;
}

/** Implements boolean update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::Update(PRUint32 updatemask, PRBool *_retval)
{
  *_retval = PR_FALSE;
  
  if (updatemask & nsISVGGlyphMetricsSource::UPDATEMASK_CHARACTER_DATA) {
    *_retval = PR_TRUE;
  }

  if (updatemask & nsISVGGlyphMetricsSource::UPDATEMASK_FONT) {
    if (mFont) {
      // don't delete mFont because we're just pointing at the ctx copy
      mFont = NULL;
    }
    *_retval = PR_TRUE;
  }

  if (!mFont) {
    nsFont font;
    mSource->GetFont(&font);
      
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

    nsString family;
    font.GetFirstFamily(family);
    char *f = ToNewCString(family);
    cairo_select_font(mCT, f, slant, weight);
    free(f);
    mFont = cairo_current_font(mCT);

    nsCOMPtr<nsPresContext> presContext;
    mSource->GetPresContext(getter_AddRefs(presContext));
    float pxPerTwips;
    pxPerTwips = presContext->TwipsToPixels();
    cairo_scale_font(mCT, font.size*pxPerTwips);
 
    nsAutoString text;
    mSource->GetCharacterData(text);
    cairo_text_extents(mCT, 
                       (unsigned char*)NS_ConvertUCS2toUTF8(text).get(),
                       &mExtents);
  }
  
  return NS_OK;
}

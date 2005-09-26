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

extern cairo_surface_t *gSVGCairoDummySurface;

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

  NS_IMETHOD_(void) SelectFont(cairo_t *ctx);

private:
  cairo_t *mCT;
  cairo_text_extents_t mExtents;
  nsCOMPtr<nsISVGGlyphMetricsSource> mSource;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGCairoGlyphMetrics::nsSVGCairoGlyphMetrics(nsISVGGlyphMetricsSource *src)
  : mSource(src)
{
  memset(&mExtents, 0, sizeof(cairo_text_extents_t));
  mCT = cairo_create(gSVGCairoDummySurface);
}

nsSVGCairoGlyphMetrics::~nsSVGCairoGlyphMetrics()
{
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
  cairo_font_extents_t extents;

  SelectFont(mCT);
  cairo_font_extents(mCT, &extents);

  switch (baselineIdentifier) {
  case BASELINE_HANGING:
    // not really right, but the best we can do with the information provided
    // FALLTHROUGH
  case BASELINE_TEXT_BEFORE_EDGE:
    *_retval = -extents.ascent;
    break;
  case BASELINE_TEXT_AFTER_EDGE:
    *_retval = extents.descent;
    break;
  case BASELINE_CENTRAL:
  case BASELINE_MIDDLE:
    *_retval = - (extents.ascent - extents.descent) / 2.0;
    break;
  case BASELINE_ALPHABETIC:
  default:
    *_retval = 0.0;
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


/** Implements nsIDOMSVGRect getExtentOfChar(in unsigned long charnum); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  cairo_text_extents_t extent;

  nsAutoString text;
  mSource->GetCharacterData(text);

  SelectFont(mCT);
  cairo_text_extents(mCT,
                     NS_ConvertUCS2toUTF8(Substring(text, charnum, 1)).get(),
                     &extent);

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

NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetAdvanceOfChar(PRUint32 charnum, float *advance)
{
  cairo_text_extents_t extent;

  nsAutoString text;
  mSource->GetCharacterData(text);

  SelectFont(mCT);
  cairo_text_extents(mCT,
                     NS_ConvertUCS2toUTF8(Substring(text, charnum, 1)).get(),
                     &extent);

  *advance = extent.x_advance;
  
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
    *_retval = PR_TRUE;
  }

  SelectFont(mCT);

  nsAutoString text;
  mSource->GetCharacterData(text);
  cairo_text_extents(mCT, 
                     NS_ConvertUCS2toUTF8(text).get(),
                     &mExtents);
  
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSVGCairoGlyphMetrics::SelectFont(cairo_t *ctx)
{
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
    cairo_select_font_face(ctx, f, slant, weight);
    nsMemory::Free(f);

    nsCOMPtr<nsPresContext> presContext;
    mSource->GetPresContext(getter_AddRefs(presContext));
    float pxPerTwips;
    pxPerTwips = presContext->TwipsToPixels();
    cairo_set_font_size(ctx, font.size*pxPerTwips);
}

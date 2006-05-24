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
#include "nsAutoPtr.h"
#include "nsISVGGlyphMetricsSource.h"
#include "nsPromiseFlatString.h"
#include "nsFont.h"
#include "nsPresContext.h"
#include "nsISVGCairoGlyphMetrics.h"
#include "nsSVGCairoGlyphMetrics.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGPoint.h"
#include "nsSVGRect.h"
#include "nsSVGUtils.h"
#include "nsDOMError.h"
#include "nsIFrame.h"
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
  nsISVGGlyphMetricsSource *mSource;
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


/** Implements float getComputedTextLength(in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetComputedTextLength(float *_retval)
{
  nsAutoString text;
  mSource->GetCharacterData(text);

  SelectFont(mCT);

  cairo_text_extents_t extent;
  cairo_text_extents(mCT,
                     NS_ConvertUTF16toUTF8(text).get(),
                     &extent);

  *_retval = fabs(extent.x_advance) + fabs(extent.y_advance);

  return NS_OK;
}

/** Implements float getSubStringLength(in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)
{
  nsAutoString text;
  mSource->GetCharacterData(text);

  SelectFont(mCT);

  cairo_text_extents_t extent;
  cairo_text_extents(mCT,
                     NS_ConvertUTF16toUTF8(Substring(text, charnum, nchars)).get(),
                     &extent);

  *_retval = fabs(extent.x_advance) + fabs(extent.y_advance);

  return NS_OK;
}

/** Implements nsIDOMSVGRect getStartPositionOfChar(in unsigned long charnum); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  mSource->GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  if (NS_FAILED(mSource->GetCharacterPosition(getter_Transfers(cp))))
    return NS_ERROR_FAILURE;

  SelectFont(mCT);

  float x, y;

  if (cp) {
    if (cp[charnum].draw == PR_FALSE)
      return NS_ERROR_DOM_INDEX_SIZE_ERR;

    x = cp[charnum].x;
    y = cp[charnum].y;

  } else {
    if (charnum > 0) {
      cairo_text_extents_t extent;

      cairo_text_extents(mCT,
                         NS_ConvertUTF16toUTF8(Substring(text,
                                                         0,
                                                         charnum)).get(),
                         &extent);

      x = extent.x_advance;
      y = extent.y_advance;
    } else {
      x = y = 0.0f;
    }
  }

  return NS_NewSVGPoint(_retval, x, y);
}

/** Implements nsIDOMSVGRect getEndPositionOfChar(in unsigned long charnum); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  mSource->GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;
  
  if (NS_FAILED(mSource->GetCharacterPosition(getter_Transfers(cp))))
    return NS_ERROR_FAILURE;

  SelectFont(mCT);

  if (cp) {
    if (cp[charnum].draw == PR_FALSE)
      return NS_ERROR_DOM_INDEX_SIZE_ERR;

    cairo_text_extents_t extent;

    cairo_text_extents(mCT, 
                       NS_ConvertUTF16toUTF8(Substring(text, charnum, 1)).get(),
                       &extent);

    float s = sin(cp[charnum].angle);
    float c = cos(cp[charnum].angle);

    return NS_NewSVGPoint(_retval, 
             cp[charnum].x + extent.x_advance * c - extent.y_advance * s,
             cp[charnum].y + extent.y_advance * c + extent.x_advance * s);
  }
  
  cairo_text_extents_t extent;
  
  cairo_text_extents(mCT, 
                     NS_ConvertUTF16toUTF8(Substring(text, 0, charnum + 1)).get(),
                     &extent);

  return NS_NewSVGPoint(_retval, extent.x_advance, extent.y_advance);
}

/** Implements nsIDOMSVGRect getExtentOfChar(in unsigned long charnum); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsAutoString text;
  mSource->GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  if (NS_FAILED(mSource->GetCharacterPosition(getter_Transfers(cp))))
    return NS_ERROR_FAILURE;

  SelectFont(mCT);

  cairo_text_extents_t extent;
  cairo_text_extents(mCT,
                     NS_ConvertUTF16toUTF8(Substring(text, charnum, 1)).get(),
                     &extent);

  if (cp) {
    if (cp[charnum].draw == PR_FALSE)
      return NS_ERROR_DOM_INDEX_SIZE_ERR;

    cairo_matrix_t matrix;
    cairo_get_matrix(mCT, &matrix);
    cairo_new_path(mCT);
    cairo_move_to(mCT, cp[charnum].x, cp[charnum].y);
    cairo_rotate(mCT, cp[charnum].angle);

    cairo_rel_move_to(mCT, extent.x_bearing, extent.y_bearing);
    cairo_rel_line_to(mCT, extent.width, 0);
    cairo_rel_line_to(mCT, 0, extent.height);
    cairo_rel_line_to(mCT, -extent.width, 0);
    cairo_close_path(mCT);

    double xmin, ymin, xmax, ymax;

    cairo_fill_extents(mCT, &xmin, &ymin, &xmax, &ymax);
    cairo_user_to_device(mCT, &xmin, &ymin);
    cairo_user_to_device(mCT, &xmax, &ymax);

    cairo_set_matrix(mCT, &matrix);

    return NS_NewSVGRect(_retval, xmin, ymin, xmax - xmin, ymax - ymin);
  }

  cairo_text_extents_t precedingExtent;

  if (charnum > 0) {
    cairo_text_extents(mCT,
                       NS_ConvertUTF16toUTF8(Substring(text,
                                                       0,
                                                       charnum)).get(),
                       &precedingExtent);
  } else {
    precedingExtent.x_advance = 0.0;
    precedingExtent.y_advance = 0.0;
  }

  // add the space taken up by the text which comes before charnum
  // to the position of the charnum character
  return NS_NewSVGRect(_retval,
                       precedingExtent.x_advance + extent.x_bearing,
                       precedingExtent.y_advance + extent.y_bearing,
                       extent.width,
                       extent.height);
}

/** Implements float getRotationOfChar(in unsigned long charnum); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  const double radPerDeg = M_PI/180.0;

  nsAutoString text;
  mSource->GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  if (NS_FAILED(mSource->GetCharacterPosition(getter_Transfers(cp))))
    return NS_ERROR_FAILURE;

  if (cp) {
    if (cp[charnum].draw == PR_FALSE)
      return NS_ERROR_DOM_INDEX_SIZE_ERR;

    *_retval = cp[charnum].angle / radPerDeg;
  } else {
    *_retval = 0.0;
  }
  return NS_OK;
}

/** Implements long GetCharNumAtPosition(in nsIDOMSVGPoint point); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)
{
  float x,y;
  point->GetX(&x);
  point->GetY(&y);

  nsAutoString text;
  mSource->GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  if (NS_FAILED(mSource->GetCharacterPosition(getter_Transfers(cp))))
    return NS_ERROR_FAILURE;

  for (PRUint32 charnum = 0; charnum < text.Length(); charnum++) {
    /* character actually on the path? */
    if (cp && cp[charnum].draw == PR_FALSE)
      continue;

    cairo_matrix_t matrix;
    cairo_get_matrix(mCT, &matrix);
    cairo_new_path(mCT);

    if (cp) {
      cairo_move_to(mCT, cp[charnum].x, cp[charnum].y);
      cairo_rotate(mCT, cp[charnum].angle);
    } else {
      if (charnum > 0) {
        cairo_text_extents_t extent;

        cairo_text_extents(mCT,
                           NS_ConvertUTF16toUTF8(Substring(text,
                                                           0,
                                                           charnum)).get(),
                           &extent);
        cairo_move_to(mCT, extent.x_advance, extent.y_advance);
      } else {
        cairo_move_to(mCT, 0.0, 0.0);
      }
    }
    cairo_text_extents_t extent;
    cairo_text_extents(mCT,
                       NS_ConvertUTF16toUTF8(Substring(text, charnum, 1)).get(),
                       &extent);

    cairo_rel_move_to(mCT, extent.x_bearing, extent.y_bearing);
    cairo_rel_line_to(mCT, extent.width, 0);
    cairo_rel_line_to(mCT, 0, extent.height);
    cairo_rel_line_to(mCT, -extent.width, 0);
    cairo_close_path(mCT);

    cairo_identity_matrix(mCT);
    if (cairo_in_fill(mCT, x, y))
      *_retval = charnum;

    cairo_set_matrix(mCT, &matrix);
  }
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
                     NS_ConvertUTF16toUTF8(Substring(text, charnum, 1)).get(),
                     &extent);

  *advance = extent.x_advance;
  
  return NS_OK;
}

/** Implements boolean update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGCairoGlyphMetrics::Update(PRBool *_retval)
{
  *_retval = PR_TRUE;
  
  nsAutoString text;
  mSource->GetCharacterData(text);
  if (text.IsEmpty()) {
    memset(&mExtents, 0, sizeof(cairo_text_extents_t));
    return NS_OK;
  }

  SelectFont(mCT);
  cairo_text_extents(mCT, 
                     NS_ConvertUTF16toUTF8(text).get(),
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

    nsIFrame *frame;
    CallQueryInterface(mSource, &frame);
    nsPresContext *presContext = frame->GetPresContext();

    float pxPerTwips;
    pxPerTwips = presContext->TwipsToPixels();
    // Since SVG has its own scaling, we really don't want
    // fonts in SVG to respond to the browser's "TextZoom"
    // (Ctrl++,Ctrl+-)
    float textZoom = presContext->TextZoom();
    cairo_set_font_size(ctx, font.size*pxPerTwips/textZoom);
}

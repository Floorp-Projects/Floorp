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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Leon Sha <leon.sha@sun.com>
 *   Alex Fritze <alex@croczilla.com>
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
#include "nsISVGRendererGlyphGeometry.h"
#include "nsIDOMSVGMatrix.h"
#include "nsISVGGlyphGeometrySource.h"
#include "nsPromiseFlatString.h"
#include "nsPresContext.h"
#include "nsMemory.h"

#include "nsSVGLibartFreetype.h"
#include "nsIServiceManager.h"
#include "nsISVGLibartGlyphMetricsFT.h"
#include "libart-incs.h"
#include "nsISVGLibartCanvas.h"

#include "nsPresContext.h"
#include "nsIDeviceContext.h"
#include "nsIComponentManager.h"

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Libart freetype-based glyph geometry implementation
 */
class nsSVGLibartGlyphGeometryFT : public nsISVGRendererGlyphGeometry
{
protected:
  friend nsresult NS_NewSVGLibartGlyphGeometryFT(nsISVGRendererGlyphGeometry **result,
                                                 nsISVGGlyphGeometrySource *src);

  nsSVGLibartGlyphGeometryFT();
  ~nsSVGLibartGlyphGeometryFT();
  nsresult Init(nsISVGGlyphGeometrySource* src);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphGeometry interface:
  NS_DECL_NSISVGRENDERERGLYPHGEOMETRY

protected:
  void PaintFill(nsISVGLibartCanvas* canvas, nsISVGLibartGlyphMetricsFT* metrics);
  nsCOMPtr<nsISVGGlyphGeometrySource> mSource;


  
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartGlyphGeometryFT::nsSVGLibartGlyphGeometryFT()
{
}

nsSVGLibartGlyphGeometryFT::~nsSVGLibartGlyphGeometryFT()
{
}

nsresult
nsSVGLibartGlyphGeometryFT::Init(nsISVGGlyphGeometrySource* src)
{
  mSource = src;
  return NS_OK;
}

nsresult
NS_NewSVGLibartGlyphGeometryFT(nsISVGRendererGlyphGeometry **result,
                               nsISVGGlyphGeometrySource *src)
{
  *result = nsnull;
  
  nsSVGLibartGlyphGeometryFT* gg = new nsSVGLibartGlyphGeometryFT();
  if (!gg) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(gg);

  nsresult rv = gg->Init(src);

  if (NS_FAILED(rv)) {
    NS_RELEASE(gg);
    return rv;
  }
  
  *result = gg;
  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLibartGlyphGeometryFT)
NS_IMPL_RELEASE(nsSVGLibartGlyphGeometryFT)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartGlyphGeometryFT)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererGlyphGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsISVGRendererGlyphGeometry methods:

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGLibartGlyphGeometryFT::Render(nsISVGRendererCanvas *canvas)
{
  nsCOMPtr<nsISVGLibartCanvas> libartCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(libartCanvas, "wrong svg render context for geometry!");
  if (!libartCanvas) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsISVGLibartGlyphMetricsFT> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    mSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics) return NS_ERROR_FAILURE;
  }

  PRBool hasFill = PR_FALSE;
  {
    PRUint16 filltype;
    mSource->GetFillPaintType(&filltype);
    if (filltype == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR)
      hasFill = PR_TRUE;
  }

  PRBool hasStroke = PR_FALSE;
  {
    PRUint16 stroketype;
    mSource->GetStrokePaintType(&stroketype);
    if (stroketype == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR)
      hasStroke = PR_TRUE;
  }

  if (hasFill)
    PaintFill(libartCanvas, metrics);

//  XXX
//  if (hasStroke)
//    PaintStroke(libartCanvas, metrics);
  
  return NS_OK;
}


/** Implements nsISVGRendererRegion update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGLibartGlyphGeometryFT::Update(PRUint32 updatemask, nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;      
  return NS_OK;
}

/** Implements nsISVGRendererRegion getCoveredRegion(); */
NS_IMETHODIMP
nsSVGLibartGlyphGeometryFT::GetCoveredRegion(nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGLibartGlyphGeometryFT::ContainsPoint(float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

void
nsSVGLibartGlyphGeometryFT::PaintFill(nsISVGLibartCanvas* canvas,
                                      nsISVGLibartGlyphMetricsFT* metrics)
{
  FT_Matrix xform;
  FT_Vector delta;
  {
    float x,y;
    mSource->GetX(&x);
    mSource->GetY(&y);
    
    nsCOMPtr<nsIDOMSVGMatrix> ctm;
    mSource->GetCTM(getter_AddRefs(ctm));
    NS_ASSERTION(ctm, "graphic source didn't specify a ctm");

    // negations of B,C,F are to transform matrix from y-down to y-up

    float a,b,c,d,e,f;
    ctm->GetA(&a);
    xform.xx = (FT_Fixed)(a*0x10000L); // convert to 16.16 fixed
    ctm->GetB(&b);
    xform.yx = (FT_Fixed)(-b*0x10000L);
    ctm->GetC(&c);
    xform.xy = (FT_Fixed)(-c*0x10000L);
    ctm->GetD(&d);
    xform.yy = (FT_Fixed)(d*0x10000L);
    ctm->GetE(&e);
    delta.x = (FT_Pos)((a*x+c*y+e)*64); // convert to 26.6 fixed
    ctm->GetF(&f);
    delta.y = (FT_Pos)(-(b*x+d*y+f)*64);
  }
  
  float opacity;
  mSource->GetFillOpacity(&opacity);

  ArtColor fill_color;
  {
    nscolor rgb;
    mSource->GetFillPaint(&rgb);
    canvas->GetArtColor(rgb, fill_color);
  }
  
  PRUint32 glyph_count = metrics->GetGlyphCount();
  
  for (PRUint32 i=0; i<glyph_count; ++i) {
    FT_Glyph glyph;
    nsSVGLibartFreetype::ft2->GlyphCopy(metrics->GetGlyphAt(i), &glyph);

    nsSVGLibartFreetype::ft2->GlyphTransform(glyph, &xform, &delta);
    
    if (NS_SUCCEEDED(nsSVGLibartFreetype::ft2->GlyphToBitmap(&glyph,
                                                             FT_RENDER_MODE_NORMAL,
                                                             nsnull, // translation
                                                             1 // destroy glyph copy
                                                             ))) {
      FT_BitmapGlyph bitmap = (FT_BitmapGlyph)glyph;
      
      ArtRender* render = canvas->NewRender(bitmap->left,
                                            -bitmap->top,
                                            bitmap->left+bitmap->bitmap.width,
                                            -bitmap->top+bitmap->bitmap.rows);
      if (render) {
        art_render_image_solid(render, fill_color);
        art_render_mask_solid(render, (int)(0x10000*opacity));
        
        art_render_mask(render,
                        bitmap->left,
                        -bitmap->top,
                        bitmap->left+bitmap->bitmap.width,
                        -bitmap->top+bitmap->bitmap.rows,
                        bitmap->bitmap.buffer,
                        bitmap->bitmap.pitch);
        canvas->InvokeRender(render);
      }
    }
    nsSVGLibartFreetype::ft2->DoneGlyph(glyph);
  }
}

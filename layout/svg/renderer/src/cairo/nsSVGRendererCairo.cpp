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
 *    layout/svg/renderer/src/libart/nsSVGRendererLibart.cpp
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

#include <cairo.h>

#include "nsCOMPtr.h"
#include "nsISVGRenderer.h"
#include "nsSVGCairoPathGeometry.h"
#include "nsSVGCairoGlyphGeometry.h"
#include "nsSVGCairoGlyphMetrics.h"
#include "nsSVGCairoCanvas.h"
#include "nsSVGCairoRegion.h"

void NS_InitSVGCairoGlyphMetricsGlobals();
void NS_FreeSVGCairoGlyphMetricsGlobals();

/**
 * \addtogroup cairo_renderer Cairo Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Cairo renderer factory
 */
class nsSVGRendererCairo : public nsISVGRenderer
{
protected:
  friend nsresult NS_NewSVGRendererCairo(nsISVGRenderer** aResult);
  friend void NS_InitSVGRendererCairoGlobals();
  friend void NS_FreeSVGRendererCairoGlobals();
  
  nsSVGRendererCairo();

public:
  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsISVGRenderer interface
  NS_DECL_NSISVGRENDERER
private:

};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGRendererCairo::nsSVGRendererCairo()
{
}

nsresult
NS_NewSVGRendererCairo(nsISVGRenderer** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsSVGRendererCairo* result = new nsSVGRendererCairo();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}

void NS_InitSVGRendererCairoGlobals()
{
}

void NS_FreeSVGRendererCairoGlobals()
{
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS1(nsSVGRendererCairo, nsISVGRenderer)

//----------------------------------------------------------------------
// nsISVGRenderer methods

/** Implements nsISVGRendererPathGeometry createPathGeometry(in nsISVGPathGeometrySource src); */
NS_IMETHODIMP
nsSVGRendererCairo::CreatePathGeometry(nsISVGPathGeometrySource *src,
                                       nsISVGRendererPathGeometry **_retval)
{
  return NS_NewSVGCairoPathGeometry(_retval, src);
}

/** Implements nsISVGRendererGlyphMetrics createGlyphMetrics(in nsISVGGlyphMetricsSource src); */
NS_IMETHODIMP
nsSVGRendererCairo::CreateGlyphMetrics(nsISVGGlyphMetricsSource *src,
                                       nsISVGRendererGlyphMetrics **_retval)
{
  return NS_NewSVGCairoGlyphMetrics(_retval, src);
}

/** Implements nsISVGRendererGlyphGeometry createGlyphGeometry(in nsISVGGlyphGeometrySource src); */
NS_IMETHODIMP
nsSVGRendererCairo::CreateGlyphGeometry(nsISVGGlyphGeometrySource *src,
                                        nsISVGRendererGlyphGeometry **_retval)
{
  return NS_NewSVGCairoGlyphGeometry(_retval, src);
}

/** Implements [noscript] nsISVGRendererCanvas createCanvas(in nsIRenderingContext ctx,
    in nsPresContext presContext, const in nsRectRef dirtyRect); */
NS_IMETHODIMP
nsSVGRendererCairo::CreateCanvas(nsIRenderingContext *ctx,
                                 nsPresContext *presContext,
                                 const nsRect & dirtyRect,
                                 nsISVGRendererCanvas **_retval)
{
  return NS_NewSVGCairoCanvas(_retval, ctx, presContext, dirtyRect);
}

/** Implements nsISVGRendererRegion createRectRegion(in float x, in float y, in float width, in float height); */
NS_IMETHODIMP
nsSVGRendererCairo::CreateRectRegion(float x, float y, float width, float height,
                                     nsISVGRendererRegion **_retval)
{
  return NS_NewSVGCairoRectRegion(_retval, x, y, width, height);
}

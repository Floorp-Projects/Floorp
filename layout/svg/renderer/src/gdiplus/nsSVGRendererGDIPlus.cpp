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

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include <Gdiplus.h>
using namespace Gdiplus;

#include "nsCOMPtr.h"
#include "nsISVGRenderer.h"
#include "nsSVGGDIPlusPathGeometry.h"
#include "nsSVGGDIPlusGlyphGeometry.h"
#include "nsSVGGDIPlusGlyphMetrics.h"
#include "nsSVGGDIPlusCanvas.h"
#include "nsSVGGDIPlusRegion.h"

void NS_InitSVGGDIPlusGlyphMetricsGlobals();
void NS_FreeSVGGDIPlusGlyphMetricsGlobals();

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * GDI+ renderer factory
 */
class nsSVGRendererGDIPlus : public nsISVGRenderer
{
protected:
  friend nsresult NS_NewSVGRendererGDIPlus(nsISVGRenderer** aResult);
  friend void NS_InitSVGRendererGDIPlusGlobals();
  friend void NS_FreeSVGRendererGDIPlusGlobals();
  
  nsSVGRendererGDIPlus();

public:
  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsISVGRenderer interface
  NS_DECL_NSISVGRENDERER
private:
  static PRBool sGdiplusStarted;
  static ULONG_PTR sGdiplusToken;
  static GdiplusStartupOutput sGdiplusStartupOutput;

};

/** @} */

//----------------------------------------------------------------------
// implementation:

PRBool nsSVGRendererGDIPlus::sGdiplusStarted = PR_FALSE;
ULONG_PTR nsSVGRendererGDIPlus::sGdiplusToken;
GdiplusStartupOutput nsSVGRendererGDIPlus::sGdiplusStartupOutput;


nsSVGRendererGDIPlus::nsSVGRendererGDIPlus()
{
  if (!sGdiplusStarted) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&sGdiplusToken, &gdiplusStartupInput,&sGdiplusStartupOutput);
    sGdiplusStarted = PR_TRUE;
  }
}

nsresult
NS_NewSVGRendererGDIPlus(nsISVGRenderer** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsSVGRendererGDIPlus* result = new nsSVGRendererGDIPlus();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}

void NS_InitSVGRendererGDIPlusGlobals()
{
  NS_InitSVGGDIPlusGlyphMetricsGlobals();
  
  // initialize gdi+ lazily in nsSVGRendererGDIPlus CTOR
}

void NS_FreeSVGRendererGDIPlusGlobals()
{
  NS_FreeSVGGDIPlusGlyphMetricsGlobals();
  
  if (nsSVGRendererGDIPlus::sGdiplusStarted) {
    GdiplusShutdown(nsSVGRendererGDIPlus::sGdiplusToken);
  }
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS1(nsSVGRendererGDIPlus, nsISVGRenderer);

//----------------------------------------------------------------------
// nsISVGRenderer methods

/** Implements nsISVGRendererPathGeometry createPathGeometry(in nsISVGPathGeometrySource src); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreatePathGeometry(nsISVGPathGeometrySource *src,
                                         nsISVGRendererPathGeometry **_retval)
{
  return NS_NewSVGGDIPlusPathGeometry(_retval, src);
}

/** Implements nsISVGRendererGlyphMetrics createGlyphMetrics(in nsISVGGlyphMetricsSource src); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateGlyphMetrics(nsISVGGlyphMetricsSource *src,
                                         nsISVGRendererGlyphMetrics **_retval)
{
  return NS_NewSVGGDIPlusGlyphMetrics(_retval, src);
}

/** Implements nsISVGRendererGlyphGeometry createGlyphGeometry(in nsISVGGlyphGeometrySource src); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateGlyphGeometry(nsISVGGlyphGeometrySource *src,
                                          nsISVGRendererGlyphGeometry **_retval)
{
  return NS_NewSVGGDIPlusGlyphGeometry(_retval, src);
}

/** Implements [noscript] nsISVGRendererCanvas createCanvas(in nsIRenderingContext ctx,
   in nsPresContext presContext, const in nsRectRef dirtyRect); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateCanvas(nsIRenderingContext *ctx,
                                   nsPresContext *presContext,
                                   const nsRect & dirtyRect,
                                   nsISVGRendererCanvas **_retval)
{
  return NS_NewSVGGDIPlusCanvas(_retval, ctx, presContext, dirtyRect);
}

/** Implements nsISVGRendererRegion createRectRegion(in float x, in float y, in float width, in float height); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateRectRegion(float x, float y, float width, float height,
                                       nsISVGRendererRegion **_retval)
{
  return NS_NewSVGGDIPlusRectRegion(_retval, x, y, width, height);
}


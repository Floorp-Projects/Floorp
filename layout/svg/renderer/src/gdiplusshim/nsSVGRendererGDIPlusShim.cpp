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
#include "nsSVGRendererGDIPlus.h"

extern "C" {
  static Status (WINAPI *pGdiplusStartup)(OUT ULONG_PTR *, const GdiplusStartupInput *, OUT GdiplusStartupOutput *) = nsnull;
  static VOID (WINAPI *pGdiplusShutdown)(ULONG_PTR) = nsnull;
}
static nsSVGRendererGDIPlusFunctions *rendererFunctions = nsnull;

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
  
public:
  nsSVGRendererGDIPlus();

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
    pGdiplusStartup(&sGdiplusToken, &gdiplusStartupInput,&sGdiplusStartupOutput);
    sGdiplusStarted = PR_TRUE;
  }
}

static PRBool
TryLoadLibraries()
{
  static PRBool attemptedLoad = PR_FALSE, successfulLoad = PR_FALSE;

  if (attemptedLoad)
    return successfulLoad;
  else
    attemptedLoad = PR_TRUE;

  HMODULE gdiplus, gkgdiplus;

  if ((gdiplus = LoadLibrary("gdiplus.dll")) == NULL)
    return PR_FALSE;
  pGdiplusStartup =
    (Status (WINAPI *)(OUT ULONG_PTR *, const GdiplusStartupInput *, OUT GdiplusStartupOutput *))GetProcAddress(gdiplus, "GdiplusStartup");
  pGdiplusShutdown =
     (VOID (WINAPI *)(ULONG_PTR))GetProcAddress(gdiplus, "GdiplusShutdown");

  if (!pGdiplusStartup || !pGdiplusShutdown)
    return PR_FALSE;
  
  if ((gkgdiplus = LoadLibrary("gksvggdiplus.dll")) == NULL)
    return PR_FALSE;

  nsSVGRendererGDIPlusFunctions *(*getFuncs)() = nsnull;

  getFuncs = (nsSVGRendererGDIPlusFunctions *(*)())GetProcAddress(gkgdiplus, "NS_GetSVGRendererGDIPlusFunctions");

  if (!getFuncs)
    return PR_FALSE;
  rendererFunctions = getFuncs();
  
  successfulLoad = PR_TRUE;
  return PR_TRUE;
}  

nsresult
NS_NewSVGRendererGDIPlus(nsISVGRenderer** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  if (!TryLoadLibraries())
    return NS_ERROR_FAILURE;

  nsSVGRendererGDIPlus* result = new nsSVGRendererGDIPlus();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}

void
NS_InitSVGRendererGDIPlusGlobals()
{
  if (!TryLoadLibraries())
    return;

  rendererFunctions->NS_InitSVGGDIPlusGlyphMetricsGlobals();
  
  // initialize gdi+ lazily in nsSVGRendererGDIPlus CTOR
}

void
NS_FreeSVGRendererGDIPlusGlobals()
{
  if (!TryLoadLibraries())
    return;

  rendererFunctions->NS_FreeSVGGDIPlusGlyphMetricsGlobals();
  
  if (nsSVGRendererGDIPlus::sGdiplusStarted) {
    pGdiplusShutdown(nsSVGRendererGDIPlus::sGdiplusToken);
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
  return rendererFunctions->NS_NewSVGGDIPlusPathGeometry(_retval, src);
}

/** Implements nsISVGRendererGlyphMetrics createGlyphMetrics(in nsISVGGlyphMetricsSource src); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateGlyphMetrics(nsISVGGlyphMetricsSource *src,
                                         nsISVGRendererGlyphMetrics **_retval)
{
  return rendererFunctions->NS_NewSVGGDIPlusGlyphMetrics(_retval, src);
}

/** Implements nsISVGRendererGlyphGeometry createGlyphGeometry(in nsISVGGlyphGeometrySource src); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateGlyphGeometry(nsISVGGlyphGeometrySource *src,
                                          nsISVGRendererGlyphGeometry **_retval)
{
  return rendererFunctions->NS_NewSVGGDIPlusGlyphGeometry(_retval, src);
}

/** Implements [noscript] nsISVGRendererCanvas createCanvas(in nsIRenderingContext ctx,
   in nsPresContext presContext, const in nsRectRef dirtyRect); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateCanvas(nsIRenderingContext *ctx,
                                   nsPresContext *presContext,
                                   const nsRect & dirtyRect,
                                   nsISVGRendererCanvas **_retval)
{
  return rendererFunctions->NS_NewSVGGDIPlusCanvas(_retval, ctx, presContext, dirtyRect);
}

/** Implements nsISVGRendererRegion createRectRegion(in float x, in float y, in float width, in float height); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateRectRegion(float x, float y, float width, float height,
                                       nsISVGRendererRegion **_retval)
{
  return rendererFunctions->NS_NewSVGGDIPlusRectRegion(_retval, x, y, width, height);
}

/** Implements nsISVGRendererSurface createSurface(in float width, in float height); */
NS_IMETHODIMP
nsSVGRendererGDIPlus::CreateSurface(PRUint32 width, PRUint32 height,
                                    nsISVGRendererSurface **_retval)
{
  return rendererFunctions->NS_NewSVGGDIPlusSurface(_retval, width, height);
}

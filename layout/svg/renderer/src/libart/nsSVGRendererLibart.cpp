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
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
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
#include "nsISVGRenderer.h"
#include "nsSVGLibartPathGeometry.h"
#include "nsSVGLibartCanvas.h"
#include "nsSVGLibartRegion.h"
#include "nsSVGLibartGlyphGeometryDefault.h"
#include "nsSVGLibartGlyphMetricsDefault.h"
#ifdef MOZ_ENABLE_FREETYPE2
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsSVGLibartFreetype.h"
#include "nsSVGLibartGlyphGeometryFT.h"
#include "nsSVGLibartGlyphMetricsFT.h"
#include "nsIPromptService.h"
#endif

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Libart-based renderer implementation
 */
class nsSVGRendererLibart : public nsISVGRenderer
{
protected:
  friend nsresult NS_NewSVGRendererLibart(nsISVGRenderer** aResult);

  nsSVGRendererLibart();
  virtual ~nsSVGRendererLibart();

public:
  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsISVGRenderer interface
  NS_DECL_NSISVGRENDERER

  static PRBool sGlobalsInited;
#ifdef MOZ_ENABLE_FREETYPE2
  static PRBool sUseFreetype;
#endif
};

/** @} */

//----------------------------------------------------------------------
// implementation

PRBool nsSVGRendererLibart::sGlobalsInited = PR_FALSE;

#ifdef MOZ_ENABLE_FREETYPE2
PRBool nsSVGRendererLibart::sUseFreetype = PR_FALSE;
#endif

nsSVGRendererLibart::nsSVGRendererLibart()
{
}

nsSVGRendererLibart::~nsSVGRendererLibart()
{
}

nsresult
NS_NewSVGRendererLibart(nsISVGRenderer** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsSVGRendererLibart* result = new nsSVGRendererLibart();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;

  if (!nsSVGRendererLibart::sGlobalsInited) {
#ifdef MOZ_ENABLE_FREETYPE2
    static NS_NAMED_LITERAL_CSTRING(prompt_service, "@mozilla.org/embedcomp/prompt-service;1");

    if (!nsSVGLibartFreetype::Init()) {
      nsCOMPtr<nsIPromptService> prompter(do_GetService(prompt_service.get()));
      NS_NAMED_LITERAL_STRING(title, "Freetype error");
      NS_NAMED_LITERAL_STRING(msg, "The Libart/Freetype SVG rendering engine could not initialize the freetype library. Please go to http://www.mozilla.org/projects/fonts/unix/enabling_truetype.html and follow steps 2-7.");
      prompter->Alert(nsnull, title.get(), msg.get());
    }
    else if (!nsSVGLibartFreetype::HasSuitableFonts()) {
      nsCOMPtr<nsIPromptService> prompter(do_GetService(prompt_service.get()));
      NS_NAMED_LITERAL_STRING(title, "Font Configuration Error");
      NS_NAMED_LITERAL_STRING(msg, "The Libart/Freetype SVG rendering engine can't find any truetype fonts on your system. Please go to http://www.mozilla.org/projects/fonts/unix/enabling_truetype.html and follow steps 2-7.");
      prompter->Alert(nsnull, title.get(), msg.get());
      nsSVGLibartFreetype::Shutdown();
    }
    else {
      nsSVGRendererLibart::sUseFreetype = PR_TRUE;
      NS_InitSVGLibartGlyphMetricsFTGlobals();
    }
#endif    
    nsSVGRendererLibart::sGlobalsInited = PR_TRUE;
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS1(nsSVGRendererLibart, nsISVGRenderer)

//----------------------------------------------------------------------
// nsISVGRenderer methods

/** Implements nsISVGRendererPathGeometry createPathGeometry(in nsISVGPathGeometrySource src); */
NS_IMETHODIMP
nsSVGRendererLibart::CreatePathGeometry(nsISVGPathGeometrySource *src,
                                        nsISVGRendererPathGeometry **_retval)
{
  return NS_NewSVGLibartPathGeometry(_retval, src);
}

/** Implements nsISVGRendererGlyphMetrics createGlyphMetrics(in nsISVGGlyphMetricsSource src); */
NS_IMETHODIMP
nsSVGRendererLibart::CreateGlyphMetrics(nsISVGGlyphMetricsSource *src,
                                        nsISVGRendererGlyphMetrics **_retval)
{
#ifdef MOZ_ENABLE_FREETYPE2
  if (sUseFreetype)
    return NS_NewSVGLibartGlyphMetricsFT(_retval, src);
#endif
  return NS_NewSVGLibartGlyphMetricsDefault(_retval, src);
}

/** Implements nsISVGRendererGlyphGeometry createGlyphGeometry(in nsISVGGlyphGeometrySource src); */
NS_IMETHODIMP
nsSVGRendererLibart::CreateGlyphGeometry(nsISVGGlyphGeometrySource *src,
                                         nsISVGRendererGlyphGeometry **_retval)
{
#ifdef MOZ_ENABLE_FREETYPE2
  if (sUseFreetype)
    return NS_NewSVGLibartGlyphGeometryFT(_retval, src);
#endif
  return NS_NewSVGLibartGlyphGeometryDefault(_retval, src);
}

/** Implements [noscript] nsISVGRendererCanvas createCanvas(in nsIRenderingContext ctx,
   in nsPresContext presContext, const in nsRectRef dirtyRect); */
NS_IMETHODIMP
nsSVGRendererLibart::CreateCanvas(nsIRenderingContext *ctx,
                                  nsPresContext *presContext,
                                  const nsRect & dirtyRect,
                                   nsISVGRendererCanvas **_retval)
{
  return NS_NewSVGLibartCanvas(_retval, ctx, presContext, dirtyRect);
}

/** Implements nsISVGRendererRegion createRectRegion(in float x, in float y, in float width, in float height); */
NS_IMETHODIMP
nsSVGRendererLibart::CreateRectRegion(float x, float y, float width, float height,
                                      nsISVGRendererRegion **_retval)
{
  return NS_NewSVGLibartRectRegion(_retval, x, y, width, height);
}


////////////////////////////////////////////////////////////////////////
// module initialisation

void NS_InitSVGRendererLibartGlobals()
{
  // Initialization of freetype (if compiled-in) is done lazily in
  // NS_NewSVGRendererLibart(). The main reason is that we might want
  // to show error messages using the prompt service, and that's
  // probably not a good idea in *this* function, at a time when
  // layout is still not fully initialized.
}

void NS_FreeSVGRendererLibartGlobals()
{
  if (!nsSVGRendererLibart::sGlobalsInited) return;
  
#ifdef MOZ_ENABLE_FREETYPE2
  if (nsSVGRendererLibart::sUseFreetype) {
    NS_FreeSVGLibartGlyphMetricsFTGlobals();  
    nsSVGLibartFreetype::Shutdown();
  }
#endif
}

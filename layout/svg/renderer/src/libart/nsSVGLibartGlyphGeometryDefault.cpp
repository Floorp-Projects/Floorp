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


#include "nsCOMPtr.h"
#include "nsISVGRendererGlyphGeometry.h"
#include "nsIDOMSVGMatrix.h"
#include "nsISVGGlyphGeometrySource.h"
#include "nsPromiseFlatString.h"
#include "nsPresContext.h"
#include "nsMemory.h"

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 *  Libart glyph geometry implementation 
 */
class nsSVGLibartGlyphGeometry : public nsISVGRendererGlyphGeometry
{
protected:
  friend nsresult NS_NewSVGLibartGlyphGeometryDefault(nsISVGRendererGlyphGeometry **result,
                                                      nsISVGGlyphGeometrySource *src);

  nsSVGLibartGlyphGeometry();
  ~nsSVGLibartGlyphGeometry();
  nsresult Init(nsISVGGlyphGeometrySource* src);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphGeometry interface:
  NS_DECL_NSISVGRENDERERGLYPHGEOMETRY
  
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartGlyphGeometry::nsSVGLibartGlyphGeometry()
{
}

nsSVGLibartGlyphGeometry::~nsSVGLibartGlyphGeometry()
{
}

nsresult
nsSVGLibartGlyphGeometry::Init(nsISVGGlyphGeometrySource* src)
{
  return NS_OK;
}


nsresult
NS_NewSVGLibartGlyphGeometryDefault(nsISVGRendererGlyphGeometry **result,
                                    nsISVGGlyphGeometrySource *src)
{
  *result = nsnull;
  
  nsSVGLibartGlyphGeometry* gg = new nsSVGLibartGlyphGeometry();
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

NS_IMPL_ADDREF(nsSVGLibartGlyphGeometry)
NS_IMPL_RELEASE(nsSVGLibartGlyphGeometry)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartGlyphGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererGlyphGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsISVGRendererGlyphGeometry methods:

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGLibartGlyphGeometry::Render(nsISVGRendererCanvas *canvas)
{
  return NS_OK;
}

/** Implements nsISVGRendererRegion update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGLibartGlyphGeometry::Update(PRUint32 updatemask, nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;      
  return NS_OK;
}

/** Implements nsISVGRendererRegion getCoveredRegion(); */
NS_IMETHODIMP
nsSVGLibartGlyphGeometry::GetCoveredRegion(nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGLibartGlyphGeometry::ContainsPoint(float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}



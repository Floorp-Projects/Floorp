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
#include "nsISVGGlyphMetricsSource.h"
#include "nsISVGRendererGlyphMetrics.h"
#include "nsPromiseFlatString.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nsPresContext.h"
#include "float.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGTypeCIDs.h"
#include "nsIComponentManager.h"

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
//////////////////////////////////////////////////////////////////////
/**
 *  Default Libart glyph metrics implementation.
 */
class nsSVGLibartGlyphMetrics : public nsISVGRendererGlyphMetrics
{
protected:
  friend nsresult NS_NewSVGLibartGlyphMetricsDefault(nsISVGRendererGlyphMetrics **result,
                                                     nsISVGGlyphMetricsSource *src);
  
  nsSVGLibartGlyphMetrics(nsISVGGlyphMetricsSource *src);
  ~nsSVGLibartGlyphMetrics();
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphMetrics interface:
  NS_DECL_NSISVGRENDERERGLYPHMETRICS

private:

};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartGlyphMetrics::nsSVGLibartGlyphMetrics(nsISVGGlyphMetricsSource *src)
{
}

nsSVGLibartGlyphMetrics::~nsSVGLibartGlyphMetrics()
{
}

nsresult
NS_NewSVGLibartGlyphMetricsDefault(nsISVGRendererGlyphMetrics **result,
                                   nsISVGGlyphMetricsSource *src)
{
  *result = new nsSVGLibartGlyphMetrics(src);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*result);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLibartGlyphMetrics)
NS_IMPL_RELEASE(nsSVGLibartGlyphMetrics)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartGlyphMetrics)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererGlyphMetrics)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererGlyphMetrics methods:

/** Implements float getBaselineOffset(in unsigned short baselineIdentifier); */
NS_IMETHODIMP
nsSVGLibartGlyphMetrics::GetBaselineOffset(PRUint16 baselineIdentifier, float *_retval)
{
  *_retval = 0.0f;
  
  switch (baselineIdentifier) {
    case BASELINE_TEXT_BEFORE_EDGE:
      break;
    case BASELINE_TEXT_AFTER_EDGE:
      break;
    case BASELINE_CENTRAL:
    case BASELINE_MIDDLE:
      break;
    case BASELINE_ALPHABETIC:
    default:
    break;
  }
  
  return NS_OK;
}


/** Implements readonly attribute float #advance; */
NS_IMETHODIMP
nsSVGLibartGlyphMetrics::GetAdvance(float *aAdvance)
{
  *aAdvance = 0.0f;
  return NS_OK;
}

/** Implements readonly attribute nsIDOMSVGRect #boundingBox; */
NS_IMETHODIMP
nsSVGLibartGlyphMetrics::GetBoundingBox(nsIDOMSVGRect * *aBoundingBox)
{
  *aBoundingBox = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);

  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;
  
  rect->SetX(0);
  rect->SetY(0);
  rect->SetWidth(0);
  rect->SetHeight(0);

  *aBoundingBox = rect;
  NS_ADDREF(*aBoundingBox);
  
  return NS_OK;
}

/** Implements [noscript] nsIDOMSVGRect getExtentOfChar(in unsigned long charnum); */
NS_IMETHODIMP
nsSVGLibartGlyphMetrics::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);

  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;
  
  rect->SetX(0);
  rect->SetY(0);
  rect->SetWidth(0);
  rect->SetHeight(0);

  *_retval = rect;
  NS_ADDREF(*_retval);
  
  return NS_OK;
}

/** Implements boolean update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGLibartGlyphMetrics::Update(PRUint32 updatemask, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}


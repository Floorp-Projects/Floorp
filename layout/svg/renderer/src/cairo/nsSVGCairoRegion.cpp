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
 * The Initial Developer of the Original Code is Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
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
#include "nsISVGRendererRegion.h"
#include "nsISVGRectangleSink.h"
#include "nsSVGCairoRegion.h"

class nsSVGCairoRectRegion : public nsISVGRendererRegion
{
protected:
  friend nsresult NS_NewSVGCairoRectRegion(nsISVGRendererRegion** result,
                                           float x, float y,
                                           float width, float height);
  nsSVGCairoRectRegion(float x, float y, float w, float h);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererRegion interface:
  NS_DECL_NSISVGRENDERERREGION

private:
  float mX, mY, mWidth, mHeight;
};

//----------------------------------------------------------------------
// implementation:

nsresult
NS_NewSVGCairoRectRegion(nsISVGRendererRegion** result, float x, float y,
                         float width, float height)
{
  *result = new nsSVGCairoRectRegion(x, y, width, height);
  
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*result);
  return NS_OK;
}

nsSVGCairoRectRegion::nsSVGCairoRectRegion(float x, float y, float w, float h) :
    mX(x), mY(y), mWidth(w), mHeight(h)
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGCairoRectRegion)
NS_IMPL_RELEASE(nsSVGCairoRectRegion)

NS_INTERFACE_MAP_BEGIN(nsSVGCairoRectRegion)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererRegion)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererRegion methods:

/** Implements nsISVGRendererRegion combine(in nsISVGRendererRegion other); */
NS_IMETHODIMP
nsSVGCairoRectRegion::Combine(nsISVGRendererRegion *other,
                              nsISVGRendererRegion **_retval)
{
  nsSVGCairoRectRegion *_other = static_cast<nsSVGCairoRectRegion*>(other); // ok, i know i'm being bad

  float x1 = PR_MIN(mX, _other->mX);
  float y1 = PR_MIN(mY, _other->mY);
  float x2 = PR_MAX(mX+mWidth, _other->mX+_other->mWidth);
  float y2 = PR_MAX(mY+mHeight, _other->mY+_other->mHeight);
  
  return NS_NewSVGCairoRectRegion(_retval, x1, y1, x2-x1, y2-y1);
}

/** Implements void getRectangleScans(in nsISVGRectangleSink sink); */
NS_IMETHODIMP
nsSVGCairoRectRegion::GetRectangleScans(nsISVGRectangleSink *sink)
{
  sink->SinkRectangle(mX, mY, mWidth, mHeight);
  return NS_OK;
}

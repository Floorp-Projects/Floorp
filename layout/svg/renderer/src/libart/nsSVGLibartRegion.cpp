/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsCOMPtr.h"
#include "nsISVGLibartRegion.h"
#include "nsSVGLibartRegion.h"
#include "nsISVGRectangleSink.h"
#include <math.h>

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Libart region implementation
 */
class nsSVGLibartRegion : public nsISVGLibartRegion // : nsISVGRendererRegion
{
protected:
  friend nsresult NS_NewSVGLibartRectRegion(nsISVGRendererRegion** result,
                                            float x, float y,
                                            float width, float height);
  friend nsresult NS_NewSVGLibartSVPRegion(nsISVGRendererRegion** result,
                                           ArtSVP *path);
  nsSVGLibartRegion(ArtIRect *rect);
  nsSVGLibartRegion(ArtSVP* path);
  nsSVGLibartRegion(ArtUta* uta); // takes ownership
  
  ~nsSVGLibartRegion();

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGLibartRegion interface:
  NS_IMETHOD_(ArtUta*) GetUta();
  
  // nsISVGRendererRegion interface:
  NS_DECL_NSISVGRENDERERREGION
  
private:
  ArtUta* mUta;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartRegion::nsSVGLibartRegion(ArtIRect *rect)
{
  mUta = art_uta_from_irect(rect);
}

nsSVGLibartRegion::nsSVGLibartRegion(ArtSVP* path)
{
  mUta = path ? art_uta_from_svp(path) : nsnull;  
}

nsSVGLibartRegion::nsSVGLibartRegion(ArtUta* uta)
    : mUta(uta)
{
}

nsSVGLibartRegion::~nsSVGLibartRegion()
{
  if (mUta)
    art_uta_free(mUta);
}

nsresult
NS_NewSVGLibartRectRegion(nsISVGRendererRegion** result,
                          float x, float y,
                          float width, float height)
{
  if (width<=0.0f || height<=0.0f) {
    *result = new nsSVGLibartRegion((ArtUta*)nsnull);
  }
  else {
    ArtIRect irect;
    irect.x0 = (int)x; // floor(x)
    irect.y0 = (int)y; // floor(y)
    irect.x1 = (int)ceil(x+width);
    irect.y1 = (int)ceil(y+height);
    NS_ASSERTION(irect.x0!=irect.x1 && irect.y0!=irect.y1, "empty region");
    *result = new nsSVGLibartRegion(&irect);
  }
  
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*result);
  return NS_OK;
}

nsresult
NS_NewSVGLibartSVPRegion(nsISVGRendererRegion** result,
                         ArtSVP* path)
{
  *result = new nsSVGLibartRegion(path);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);
  return NS_OK;  
}


//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLibartRegion)
NS_IMPL_RELEASE(nsSVGLibartRegion)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartRegion)
  NS_INTERFACE_MAP_ENTRY(nsISVGLibartRegion)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererRegion)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGLibartRegion interface:

NS_IMETHODIMP_(ArtUta*)
nsSVGLibartRegion::GetUta()
{
  return mUta;
}

//----------------------------------------------------------------------
// nsISVGRendererRegion methods:

/** Implements nsISVGRendererRegion combine(in nsISVGRendererRegion other); */
NS_IMETHODIMP
nsSVGLibartRegion::Combine(nsISVGRendererRegion *other,
                           nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsISVGLibartRegion> other2 = do_QueryInterface(other);
  if (!other2) {
    NS_WARNING("Union operation on incompatible regions");
    return NS_ERROR_FAILURE;
  }

  nsISVGLibartRegion *regions[2];
  int count = 0;

  if (mUta)
    regions[count++]=this;

  if (other2->GetUta())
    regions[count++]=other2;

  switch (count) {
    case 0:
      break;
    case 1:
      *_retval = regions[0];
      NS_ADDREF(*_retval);
      break;
    case 2:
      *_retval = new nsSVGLibartRegion(art_uta_union(regions[0]->GetUta(),
                                                     regions[1]->GetUta()));
      NS_IF_ADDREF(*_retval);
      break;
  }
  
  return NS_OK;
}

/** Implements void getRectangleScans(in nsISVGRectangleSink sink); */
NS_IMETHODIMP
nsSVGLibartRegion::GetRectangleScans(nsISVGRectangleSink *sink)
{
  if (!mUta) return NS_OK;
  
  int nRects=0;
  ArtIRect* rectList = art_rect_list_from_uta(mUta, 200, 200, &nRects);
    for (int i=0; i<nRects; ++i) {
      int x,y,w,h;
      x = PR_MIN(rectList[i].x0,rectList[i].x1)-2;
      y = PR_MIN(rectList[i].y0,rectList[i].y1)-2;
      w = PR_ABS(rectList[i].x0-rectList[i].x1)+4;
      h = PR_ABS(rectList[i].y0-rectList[i].y1)+4;

      sink->SinkRectangle(x,y,w,h);
    }
        
    art_free(rectList);
  
  return NS_OK;
}

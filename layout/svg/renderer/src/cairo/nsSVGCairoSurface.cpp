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

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsSVGCairoSurface.h"
#include "nsISVGCairoSurface.h"
#include <cairo.h>

/**
 * \addtogroup cairo_renderer Cairo Rendering Engine
 * @{
 */
//////////////////////////////////////////////////////////////////////
/**
 * Cairo surface implementation
 */
class nsSVGCairoSurface : public nsISVGCairoSurface
{
public:
  nsSVGCairoSurface();
  ~nsSVGCairoSurface();
  nsresult Init(PRUint32 width, PRUint32 height);
    
  // nsISupports interface:
  NS_DECL_ISUPPORTS
    
  // nsISVGRendererSurface interface:
  NS_DECL_NSISVGRENDERERSURFACE

  // nsISVGCairoSurface interface:
  NS_IMETHOD_(cairo_surface_t*) GetSurface() { return mSurface; }
    
private:
  char *mData;
  cairo_surface_t *mSurface;
  PRUint32 mWidth, mHeight;
};


/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGCairoSurface::nsSVGCairoSurface() : mData(nsnull), mSurface(nsnull)
{
}

nsSVGCairoSurface::~nsSVGCairoSurface()
{
  if (mSurface) {
    cairo_surface_destroy(mSurface);
    mSurface = nsnull;
  }
  if (mData) {
    delete [] mData;
    mData = nsnull;
  }
}

nsresult
nsSVGCairoSurface::Init(PRUint32 width, PRUint32 height)
{
  mWidth = width;
  mHeight = height;

  mData = new char[4*width*height];

  if (!mData)
    return NS_ERROR_OUT_OF_MEMORY;

  memset(mData, 0, 4*width*height);
  mSurface = cairo_surface_create_for_image(mData, CAIRO_FORMAT_ARGB32,
                                            mWidth, mHeight, 4*mWidth);
  if (!mSurface)
    return NS_ERROR_FAILURE;

  cairo_surface_set_filter(mSurface, CAIRO_FILTER_BEST);

  return NS_OK;
}

nsresult
NS_NewSVGCairoSurface(nsISVGRendererSurface **result,
                      PRUint32 width, PRUint32 height)
{
  nsSVGCairoSurface* surf = new nsSVGCairoSurface();
  if (!surf)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(surf);

  nsresult rv = surf->Init(width, height);

  if (NS_FAILED(rv)) {
    NS_RELEASE(surf);
    return rv;
  }
  
  *result = surf;
  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGCairoSurface)
NS_IMPL_RELEASE(nsSVGCairoSurface)

NS_INTERFACE_MAP_BEGIN(nsSVGCairoSurface)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererSurface)
  NS_INTERFACE_MAP_ENTRY(nsISVGCairoSurface)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsSVGCairoSurface::GetWidth(PRUint32 *aWidth)
{
  *aWidth = mWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoSurface::GetHeight(PRUint32 *aHeight)
{
  *aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoSurface::GetData(PRUint8 **aData, PRUint32 *length, PRInt32 *stride)
{
  *aData = (PRUint8*)mData;
  *length = 4*mWidth*mHeight;
  *stride = 4*mWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoSurface::Lock()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoSurface::Unlock()
{
  return NS_OK;
}


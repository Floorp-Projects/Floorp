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
#include "nsISVGGDIPlusRegion.h"
#include "nsSVGGDIPlusRegion.h"
#include "nsISVGRectangleSink.h"
#include "nsPresContext.h"
#include "nsDeviceContextWin.h"

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 *  GDI+ region implementation
 */
class nsSVGGDIPlusRegion : public nsISVGGDIPlusRegion
{
protected:
  friend nsresult NS_NewSVGGDIPlusRectRegion(nsISVGRendererRegion** result,
                                             float x, float y,
                                             float width, float height);
  friend nsresult NS_NewSVGGDIPlusPathRegion(nsISVGRendererRegion** result,
                                             const GraphicsPath* path);
  friend nsresult NS_NewSVGGDIPlusClonedRegion(nsISVGRendererRegion** result,
                                               const Region* region,
                                               nsPresContext *presContext);
  nsSVGGDIPlusRegion(RectF& rect);
  nsSVGGDIPlusRegion(const GraphicsPath* path);
  nsSVGGDIPlusRegion(const nsSVGGDIPlusRegion& other);
  nsSVGGDIPlusRegion(const Region* other
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
                     , const Graphics* graphics
#endif
                     );
  
  ~nsSVGGDIPlusRegion();

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGGDIPlusRegion interface:
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
  NS_IMETHOD_(const RectF*) GetRect()const;
#else
  NS_IMETHOD_(const Region*) GetRegion()const;
#endif
  
  // nsISVGRendererRegion interface:
  NS_DECL_NSISVGRENDERERREGION
  
private:
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
  RectF mRect;
#else
  Region mRegion;
#endif
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGGDIPlusRegion::nsSVGGDIPlusRegion(RectF& rect)
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
    : mRect(rect.X, rect.Y, rect.Width, rect.Height)
#else
    : mRegion(rect)
#endif
{
}

nsSVGGDIPlusRegion::nsSVGGDIPlusRegion(const GraphicsPath* path)
#ifndef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
    : mRegion(path)
#endif
{
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
  path->GetBounds(&mRect);
#endif
}

nsSVGGDIPlusRegion::nsSVGGDIPlusRegion(const nsSVGGDIPlusRegion& other)
{
  
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
  mRect.X = other.GetRect()->X;
  mRect.Y = other.GetRect()->Y;
  mRect.Width = other.GetRect()->Width;
  mRect.Height = other.GetRect()->Height;
#else
  mRegion.MakeEmpty();
  mRegion.Union(other.GetRegion());
#endif
}

nsSVGGDIPlusRegion::nsSVGGDIPlusRegion(const Region* other
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
                                       , const Graphics* graphics
#endif
                                      )
{
  
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
  other->GetBounds(&mRect, graphics);
#else
  mRegion.MakeEmpty();
  mRegion.Union(other);
#endif
}


nsSVGGDIPlusRegion::~nsSVGGDIPlusRegion()
{
}

nsresult
NS_NewSVGGDIPlusRectRegion(nsISVGRendererRegion** result,
                           float x, float y,
                           float width, float height)
{
  *result = new nsSVGGDIPlusRegion(RectF(x, y, width, height));
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);
  return NS_OK;
}

nsresult
NS_NewSVGGDIPlusPathRegion(nsISVGRendererRegion** result,
                           const GraphicsPath* path)
{
  *result = new nsSVGGDIPlusRegion(path);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);
  return NS_OK;  
}

nsresult NS_NewSVGGDIPlusClonedRegion(nsISVGRendererRegion** result,
                                      const Region* region,
                                      nsPresContext *presContext)
{
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
  HWND win;
  HDC devicehandle;
  {
    nsIDeviceContext* devicecontext = presContext->DeviceContext();
    // this better be what we think it is:
    win = (HWND)((nsDeviceContextWin *)devicecontext)->mWidget;
    devicehandle = ::GetDC(win);
  }

  {
    // wrap this in a block so that the Graphics-object goes out of
    // scope before we release the HDC.
    
    Graphics graphics(devicehandle);
    
    *result = new nsSVGGDIPlusRegion(region, &graphics);  
  }
  
  ::ReleaseDC(win, devicehandle);
#else
    *result = new nsSVGGDIPlusRegion(region);
#endif
  
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);
  return NS_OK;  
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGGDIPlusRegion)
NS_IMPL_RELEASE(nsSVGGDIPlusRegion)

NS_INTERFACE_MAP_BEGIN(nsSVGGDIPlusRegion)
  NS_INTERFACE_MAP_ENTRY(nsISVGGDIPlusRegion)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererRegion)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGGDIPlusRegion methods:

#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
NS_IMETHODIMP_(const RectF*)
nsSVGGDIPlusRegion::GetRect()const
{
  return &mRect;
}
#else
NS_IMETHODIMP_(const Region*)
nsSVGGDIPlusRegion::GetRegion()const
{
  return &mRegion;
}
#endif

//----------------------------------------------------------------------
// nsISVGRendererRegion methods:

/** Implements nsISVGRendererRegion combine(in nsISVGRendererRegion other); */
NS_IMETHODIMP
nsSVGGDIPlusRegion::Combine(nsISVGRendererRegion *other, nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  nsSVGGDIPlusRegion *union_region = new nsSVGGDIPlusRegion(*this);
  NS_ADDREF(union_region);
  *_retval = union_region;

  if (!other) return NS_OK;
  
  nsCOMPtr<nsISVGGDIPlusRegion> other2 = do_QueryInterface(other);
  if (!other2) {
    NS_WARNING("Union operation on incompatible regions");
    return NS_OK;
  }
  
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
  RectF::Union(union_region->mRect, mRect, *(other2->GetRect()));
#else
  union_region->mRegion.Union(other2->GetRegion());
#endif
  
  return NS_OK;
}

/** Implements void getRectangleScans(in nsISVGRectangleSink sink); */
NS_IMETHODIMP
nsSVGGDIPlusRegion::GetRectangleScans(nsISVGRectangleSink *sink)
{
#ifdef NS_SVG_GDIPLUS_RENDERER_USE_RECT_REGIONS
  sink->SinkRectangle(mRect.X-1, mRect.Y-1, mRect.Width+2, mRect.Height+2);
#else
  Matrix matrix;
  INT count = mRegion.GetRegionScansCount(&matrix);
  if (count == 0) return NS_OK;
  
  RectF* rects = (RectF*)malloc(count*sizeof(RectF));
  mRegion.GetRegionScans(&matrix, rects, &count);

  for (INT i=0; i<count; ++i) 
    sink->SinkRectangle(rects[i].X-1, rects[i].Y-1, rects[i].Width+2, rects[i].Height+2);

  free(rects);
#endif
  
  return NS_OK;
}

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
 * Crocodile Clips Ltd.
 * Portions created by the Initial Developer are Copyright (C) 2001
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
#include "nsSVGLibartPathGeometry.h"
#include "nsISVGRendererPathGeometry.h"
#include "nsISVGLibartCanvas.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGLibartRegion.h"
#include "nsISVGRendererRegion.h"
#include "nsSVGLibartBPathBuilder.h"
#include "nsSVGLibartGradient.h"
#include "nsISVGRendererPathBuilder.h"
#include "nsISVGPathGeometrySource.h"
#include "nsISVGGradient.h"
#include "nsSVGFill.h"
#include "nsSVGStroke.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "prdtoa.h"
#include "nsString.h"

// comment from art_vpath_path.c: The Adobe PostScript reference
// manual defines flatness as the maximum deviation between any
// point on the vpath approximation and the corresponding point on the
// "true" curve, and we follow this definition here. A value of 0.25
// should ensure high quality for aa rendering.
#define SVG_BEZIER_FLATNESS 0.5

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Libart path geometry implementation
 */
class nsSVGLibartPathGeometry : public nsISVGRendererPathGeometry
{
protected:
  friend nsresult NS_NewSVGLibartPathGeometry(nsISVGRendererPathGeometry **result,
                                              nsISVGPathGeometrySource *src);

  nsSVGLibartPathGeometry();
  ~nsSVGLibartPathGeometry();
  nsresult Init(nsISVGPathGeometrySource* src);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS
  
  // nsISVGRendererPathGeometry interface:
  NS_DECL_NSISVGRENDERERPATHGEOMETRY
  
protected:
  void ClearPath() { if (mVPath) { art_free(mVPath); mVPath=nsnull; } }
  void ClearFill() { mFill.Clear(); }
  void ClearStroke() { mStroke.Clear(); }
  void ClearCoveredRegion() { mCoveredRegion = nsnull; }
  ArtVpath *GetPath();
  ArtSVP *GetFill();
  ArtSVP *GetStroke();
  ArtSVP *GetGradient();
  
  private:
  nsCOMPtr<nsISVGPathGeometrySource> mSource;
  nsCOMPtr<nsISVGRendererRegion> mCoveredRegion;

  ArtVpath* mVPath;
  nsSVGFill mFill;
  nsSVGStroke mStroke;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartPathGeometry::nsSVGLibartPathGeometry()
    : mVPath(nsnull)
{
}

nsSVGLibartPathGeometry::~nsSVGLibartPathGeometry()
{
  ClearPath();
}

nsresult nsSVGLibartPathGeometry::Init(nsISVGPathGeometrySource* src)
{
  mSource = src;
  return NS_OK;
}


nsresult
NS_NewSVGLibartPathGeometry(nsISVGRendererPathGeometry **result,
                            nsISVGPathGeometrySource *src)
{
  nsSVGLibartPathGeometry* pg = new nsSVGLibartPathGeometry();
  if (!pg) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(pg);

  nsresult rv = pg->Init(src);

  if (NS_FAILED(rv)) {
    NS_RELEASE(pg);
    return rv;
  }
  
  *result = pg;
  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLibartPathGeometry)
NS_IMPL_RELEASE(nsSVGLibartPathGeometry)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartPathGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererPathGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------

ArtVpath*
nsSVGLibartPathGeometry::GetPath()
{
  if (mVPath) return mVPath;

  // 1. construct a bezier path:
  ArtBpath*bpath = nsnull;
  
  nsCOMPtr<nsISVGRendererPathBuilder> builder;
  NS_NewSVGLibartBPathBuilder(getter_AddRefs(builder), &bpath);
  mSource->ConstructPath(builder);
  builder->EndPath();

  // 2. transform the bpath into global coords:
  double matrix[6];
  {
    nsCOMPtr<nsIDOMSVGMatrix> ctm;
    mSource->GetCanvasTM(getter_AddRefs(ctm));
    NS_ASSERTION(ctm, "graphic source didn't have a ctm");
    
    float val;
    ctm->GetA(&val);
    matrix[0] = val;
    
    ctm->GetB(&val);
    matrix[1] = val;
    
    ctm->GetC(&val);  
    matrix[2] = val;  
    
    ctm->GetD(&val);  
    matrix[3] = val;  
    
    ctm->GetE(&val);
    matrix[4] = val;
    
    ctm->GetF(&val);
    matrix[5] = val;
  }

  if ( bpath &&
       ( matrix[0] != 1.0 || matrix[2] != 0.0 || matrix[4] != 0.0 ||
         matrix[1] != 0.0 || matrix[3] != 1.0 || matrix[5] != 0.0 ))
  {
    ArtBpath* temp = bpath;
    bpath = art_bpath_affine_transform(bpath, matrix);
    art_free(temp);
  }

  // 3. convert the bpath into a vpath:
  if (bpath)
    mVPath = art_bez_path_to_vec(bpath, SVG_BEZIER_FLATNESS);

  return mVPath;
}

ArtSVP *
nsSVGLibartPathGeometry::GetFill()
{
  if (!mFill.IsEmpty() || !GetPath()) return mFill.GetSvp();
  
  mFill.Build(GetPath(), mSource);
  
  return mFill.GetSvp();
}

ArtSVP *
nsSVGLibartPathGeometry::GetStroke()
{
  if (!mStroke.IsEmpty() || !GetPath()) return mStroke.GetSvp();

  mStroke.Build(GetPath(), mSource);
  
  return mStroke.GetSvp();
}

//----------------------------------------------------------------------
// nsISVGRendererPathGeometry methods:

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGLibartPathGeometry::Render(nsISVGRendererCanvas *canvas)
{
  nsCOMPtr<nsISVGLibartCanvas> libartCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(libartCanvas, "wrong svg render context for geometry!");
  if (!libartCanvas) return NS_ERROR_FAILURE;

  PRUint16 type;
  
  // paint fill:
  mSource->GetFillPaintType(&type);

  if (type == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR && GetFill()) {
    nscolor rgb;
    mSource->GetFillPaint(&rgb);
    float opacity;
    mSource->GetFillOpacity(&opacity);

    ArtColor col;
    libartCanvas->GetArtColor(rgb, col);
    
    ArtRender* render = libartCanvas->NewRender();
    NS_ASSERTION(render, "could not create render");

#ifdef DEBUG_scooter
    printf("nsSVGLibartPathGeometry::Render - opacity=%f\n",opacity);
#endif

    art_render_mask_solid(render, (int)(0x10000 * opacity));
    art_render_svp(render, GetFill());
    art_render_image_solid(render, col);
    libartCanvas->InvokeRender(render);
  } else if (type == nsISVGGeometrySource::PAINT_TYPE_SERVER && GetFill() ) {
    // Handle gradients
    nsCOMPtr<nsISVGGradient> aGrad;
    mSource->GetFillGradient(getter_AddRefs(aGrad));
    float opacity;
    mSource->GetFillOpacity(&opacity);
    
    ArtRender* render = libartCanvas->NewRender();
    NS_ASSERTION(render, "could not create render");

    art_render_mask_solid(render, (int)(0x10000 * opacity));
    art_render_svp(render, GetFill());

    // Now, get the appropriate gradient fill
    nsCOMPtr<nsISVGRendererRegion> region;
    GetCoveredRegion(getter_AddRefs(region));
    nsCOMPtr<nsISVGLibartRegion> aLibartRegion = do_QueryInterface(region);
    nsCOMPtr<nsIDOMSVGMatrix> ctm;
    mSource->GetCanvasTM(getter_AddRefs(ctm));
    LibartGradient(render, ctm, aGrad, aLibartRegion);

    // And draw it
    libartCanvas->InvokeRender(render);
  }

  // paint stroke:
  mSource->GetStrokePaintType(&type);
  if (type == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR && GetStroke()) {
    nscolor rgb;
    mSource->GetStrokePaint(&rgb);
    float opacity;
    mSource->GetStrokeOpacity(&opacity);

    ArtColor col;
    libartCanvas->GetArtColor(rgb, col);
    
    ArtRender* render = libartCanvas->NewRender();
    NS_ASSERTION(render, "could not create render");

    art_render_mask_solid(render, (int)(0x10000 * opacity));
    art_render_svp(render, GetStroke());
    art_render_image_solid(render, col);
    libartCanvas->InvokeRender(render);
  } else if (type == nsISVGGeometrySource::PAINT_TYPE_SERVER && GetStroke()) {
    // Handle gradients
    nsCOMPtr<nsISVGGradient> aGrad;
    mSource->GetStrokeGradient(getter_AddRefs(aGrad));
    float opacity;
    mSource->GetStrokeOpacity(&opacity);
    
    ArtRender* render = libartCanvas->NewRender();
    NS_ASSERTION(render, "could not create render");

    art_render_mask_solid(render, (int)(0x10000 * opacity));
    art_render_svp(render, GetStroke());

    // Now, get the appropriate gradient fill
    nsCOMPtr<nsISVGRendererRegion> region;
    GetCoveredRegion(getter_AddRefs(region));
    nsCOMPtr<nsISVGLibartRegion> aLibartRegion = do_QueryInterface(region);
    nsCOMPtr<nsIDOMSVGMatrix> ctm;
    mSource->GetCanvasTM(getter_AddRefs(ctm));
    LibartGradient(render, ctm, aGrad, aLibartRegion);

    // And draw it
    libartCanvas->InvokeRender(render);
  }
  
  return NS_OK;
}

/** Implements nsISVGRendererRegion update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGLibartPathGeometry::Update(PRUint32 updatemask, nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  const unsigned long pathmask =
    nsISVGPathGeometrySource::UPDATEMASK_PATH |
    nsISVGGeometrySource::UPDATEMASK_CANVAS_TM;

  const unsigned long fillmask = 
    pathmask |
    nsISVGGeometrySource::UPDATEMASK_FILL_RULE;

  const unsigned long strokemask =
    pathmask |
    nsISVGGeometrySource::UPDATEMASK_STROKE_WIDTH       |
    nsISVGGeometrySource::UPDATEMASK_STROKE_LINECAP     |
    nsISVGGeometrySource::UPDATEMASK_STROKE_LINEJOIN    |
    nsISVGGeometrySource::UPDATEMASK_STROKE_MITERLIMIT  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_DASH_ARRAY  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_DASHOFFSET;

  const unsigned long coveredregionmask =
    fillmask                                            |
    strokemask                                          |
    nsISVGGeometrySource::UPDATEMASK_FILL_PAINT_TYPE    |
    nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT_TYPE;
  
  nsCOMPtr<nsISVGRendererRegion> before;
  // only obtain the 'before' region if we have built a path before:
  if (!mFill.IsEmpty() || !mStroke.IsEmpty())
    GetCoveredRegion(getter_AddRefs(before));

  if ((updatemask & pathmask)!=0){
    ClearPath();
  }
  if ((updatemask & fillmask)!=0)
    ClearFill();
  if ((updatemask & strokemask)!=0)
    ClearStroke();
  if ((updatemask & coveredregionmask)!=0) {
    ClearCoveredRegion();
    nsCOMPtr<nsISVGRendererRegion> after;
    GetCoveredRegion(getter_AddRefs(after));
    if (after)
      after->Combine(before, _retval);
  }
  else if (updatemask != nsISVGGeometrySource::UPDATEMASK_NOTHING) {
    *_retval = before;
    NS_IF_ADDREF(*_retval);
  }

  return NS_OK;
}

/** Implements nsISVGRendererRegion getCoveredRegion(); */
NS_IMETHODIMP
nsSVGLibartPathGeometry::GetCoveredRegion(nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;
  
  if (mCoveredRegion) {
    *_retval = mCoveredRegion;
    NS_ADDREF(*_retval);
    return NS_OK;
  }
  
  PRUint16 type;  
  mSource->GetFillPaintType(&type);
  bool hasCoveredFill = (type!=nsISVGGeometrySource::PAINT_TYPE_NONE) && GetFill();
  
  mSource->GetStrokePaintType(&type);
  bool hasCoveredStroke = (type!=nsISVGGeometrySource::PAINT_TYPE_NONE) && GetStroke();

  if (!hasCoveredFill && !hasCoveredStroke) return NS_OK;

  if (hasCoveredFill) {
    nsCOMPtr<nsISVGRendererRegion> reg1;
    NS_NewSVGLibartSVPRegion(getter_AddRefs(reg1), GetFill());
    if (hasCoveredStroke) {
      nsCOMPtr<nsISVGRendererRegion> reg2;
      NS_NewSVGLibartSVPRegion(getter_AddRefs(reg2), GetStroke());
      reg1->Combine(reg2, _retval);
    }
    else {
      *_retval = reg1;
      NS_ADDREF(*_retval);
    }
  } // covered stroke only
  else
    NS_NewSVGLibartSVPRegion(_retval, GetStroke());

  mCoveredRegion = *_retval;
  return NS_OK;
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGLibartPathGeometry::ContainsPoint(float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;

  PRUint16 mask;
  mSource->GetHittestMask(&mask);

  if (mask & nsISVGPathGeometrySource::HITTEST_MASK_FILL &&
      GetFill() &&
      mFill.Contains(x,y)) {
    *_retval = PR_TRUE;
    return NS_OK;
  }
  if (mask & nsISVGPathGeometrySource::HITTEST_MASK_STROKE &&
      GetStroke() &&
      mStroke.Contains(x,y)) {
    *_retval = PR_TRUE;
  }
  
  return NS_OK;
}

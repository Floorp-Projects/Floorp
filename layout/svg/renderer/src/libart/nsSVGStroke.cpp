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

#include "nsSVGStroke.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsISVGPathGeometrySource.h"
#include "prdtoa.h"
#include "nsIDOMSVGMatrix.h"
#include <math.h>

#define SVG_STROKE_FLATNESS 0.5

void
nsSVGStroke::Build(ArtVpath* path, nsISVGPathGeometrySource* source)
{
  if (mSvp)
    art_svp_free(mSvp);

  float width;
  source->GetStrokeWidth(&width);

  // XXX since we construct the stroke from a pre-transformed path, we
  // adjust the stroke width according to the expansion part of
  // transformation. This is not true anamorphic scaling, but the best
  // we can do given the circumstances...
  float expansion; 
  {
    double matrix[6];
    nsCOMPtr<nsIDOMSVGMatrix> ctm;
    source->GetCanvasTM(getter_AddRefs(ctm));
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
    
    expansion = sqrt((float)fabs(matrix[0]*matrix[3]-matrix[2]*matrix[1]));
  }

  width*=expansion;
  if (width==0.0) return;
  
  PRUint16 strokelinecap;
  source->GetStrokeLinecap(&strokelinecap);  
  ArtPathStrokeCapType captype;
  switch(strokelinecap) {
    case nsISVGGeometrySource::STROKE_LINECAP_BUTT:
      captype = ART_PATH_STROKE_CAP_BUTT;
      break;
    case nsISVGGeometrySource::STROKE_LINECAP_ROUND:
      captype = ART_PATH_STROKE_CAP_ROUND;
      break;
    case nsISVGGeometrySource::STROKE_LINECAP_SQUARE:
      captype = ART_PATH_STROKE_CAP_SQUARE;
      break;
    default:
      NS_ERROR("not reached");
  }

  PRUint16 strokelinejoin;
  source->GetStrokeLinejoin(&strokelinejoin);

  ArtPathStrokeJoinType jointype;
  switch(strokelinejoin) {
    case nsISVGGeometrySource::STROKE_LINEJOIN_MITER:
      jointype = ART_PATH_STROKE_JOIN_MITER;
      break;
    case nsISVGGeometrySource::STROKE_LINEJOIN_ROUND:
      jointype = ART_PATH_STROKE_JOIN_ROUND;
      break;
    case nsISVGGeometrySource::STROKE_LINEJOIN_BEVEL:
      jointype = ART_PATH_STROKE_JOIN_BEVEL;
      break;
    default:
      NS_ERROR("not reached");
  }

  float *dashArray;
  PRUint32 dashCount;
  source->GetStrokeDashArray(&dashArray, &dashCount);
  if (dashCount>0) {
    ArtVpathDash dashes;

    float offset;
    source->GetStrokeDashoffset(&offset);
    dashes.offset = offset;

    dashes.n_dash = dashCount;
    dashes.dash = new double[dashCount];
    while (dashCount--)
      dashes.dash[dashCount] = dashArray[dashCount];

    nsMemory::Free(dashArray);

    // create a dashed vpath:

    ArtVpathArrayIterator src_iter;
//      ArtVpathClipFilter clip_filter;
    ArtVpathDashFilter dash_filter;
      
    art_vpath_array_iterator_init((ArtVpath*)path, &src_iter);
//      art_vpath_clip_filter_init((ArtVpathIterator*)&src_iter, canvasSpecs->GetClipRect(), &clip_filter);
//      art_vpath_dash_filter_init((ArtVpathIterator*)&clip_filter, &dash, &dash_filter);
    art_vpath_dash_filter_init((ArtVpathIterator*)&src_iter, &dashes, &dash_filter);
    
    path = art_vpath_new_vpath_array((ArtVpathIterator*)&dash_filter);
    
    // clean up
    delete[] dashes.dash;
  }
  else
  {
    // no dash pattern - just clip & contract with moveto_open
      ArtVpathArrayIterator src_iter;
//      ArtVpathClipFilter clip_filter;
//      ArtVpathContractFilter contract_filter;
      
      art_vpath_array_iterator_init((ArtVpath*)path, &src_iter);
//      art_vpath_clip_filter_init((ArtVpathIterator*)&src_iter, canvasSpecs->GetClipRect(), &clip_filter);
//      art_vpath_contract_filter_init((ArtVpathIterator*)&clip_filter,
//                                     ART_LINETO_CLIPPED,
//                                     ART_MOVETO_OPEN,
//                                     &contract_filter);
      
//      path = art_vpath_new_vpath_array((ArtVpathIterator*)&contract_filter);
      path = art_vpath_new_vpath_array((ArtVpathIterator*)&src_iter);
  }    

  float miterlimit;
  source->GetStrokeMiterlimit(&miterlimit);

  mSvp = art_svp_vpath_stroke(path,
                              jointype,
                              captype,
                              width,
                              miterlimit,
                              SVG_STROKE_FLATNESS);
  art_free(path);
}


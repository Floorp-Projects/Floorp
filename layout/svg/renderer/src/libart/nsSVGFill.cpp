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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *    Bradley Baetz <bbaetz@cs.mcgill.ca>
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

#include <math.h>

#include "nsSVGFill.h"
#include "nsISVGPathGeometrySource.h"

// static helper functions 

#define EPSILON 1e-6

static PRBool ContainsOpenPath(ArtVpath* src)
{
  int pos;
  for(pos=0;src[pos].code != ART_END;++pos) {
    if (src[pos].code==ART_MOVETO_OPEN)
      return PR_TRUE;
  }
  return PR_FALSE;
}

// Closes an open path, returning a new one
static ArtVpath* CloseOpenPath(ArtVpath* src)
{
  // We need to insert something extra for each open subpath
  // So we realloc stuff as needed
  int currSize = 0;
  int maxSize = 4;
  ArtVpath *ret = art_new(ArtVpath, maxSize);

  PRBool isOpenSubpath = PR_FALSE;
  double startX=0;
  double startY=0;

  int srcPos;
  for (srcPos=0;src[srcPos].code != ART_END;++srcPos) {
    if (currSize==maxSize)
      art_expand(ret, ArtVpath, maxSize);
    if (src[srcPos].code == ART_MOVETO_OPEN)
      ret[currSize].code = ART_MOVETO; // close it
    else
      ret[currSize].code = src[srcPos].code;
    ret[currSize].x = src[srcPos].x;
    ret[currSize].y = src[srcPos].y;
    ++currSize;

    // OK, it was open
    if (src[srcPos].code == ART_MOVETO_OPEN) {
      startX = src[srcPos].x;
      startY = src[srcPos].y;
      isOpenSubpath = PR_TRUE;
    } else if (src[srcPos+1].code != ART_LINETO) {
      if (isOpenSubpath &&
          ((fabs(startX - src[srcPos].x) > EPSILON) ||
           (fabs(startY - src[srcPos].y) > EPSILON))) {
        // The next one isn't a line, so lets close this
        if (currSize == maxSize)
          art_expand(ret, ArtVpath, maxSize);
        ret[currSize].code = ART_LINETO;
        ret[currSize].x = startX;
        ret[currSize].y = startY;
        ++currSize;
      }
      isOpenSubpath = PR_FALSE;
    }
  }
  if (currSize == maxSize)
    art_expand(ret, ArtVpath, maxSize);
  ret[currSize].code = ART_END;
  ret[currSize].x = 0;
  ret[currSize].y = 0;
  return ret;
}

// nsSVGFill members
  
void
nsSVGFill::Build(ArtVpath* path, nsISVGPathGeometrySource* source)
{
  if (mSvp) {
    art_svp_free(mSvp);
    mSvp = nsnull;
  }

  NS_ASSERTION(path, "null vpath");
  
  // clip
  ArtVpathArrayIterator src_iter;
//  ArtVpathPolyRectClipFilter clip_filter;
  
  art_vpath_array_iterator_init((ArtVpath*)path, &src_iter);
//  art_vpath_poly_rect_clip_filter_init((ArtVpathIterator*)&src_iter,
//                                       canvasSpecs->GetClipRect(),
//                                       &clip_filter);
  
//  path = art_vpath_new_vpath_array((ArtVpathIterator*)&clip_filter);
  path = art_vpath_new_vpath_array((ArtVpathIterator*)&src_iter);

  
  if (ContainsOpenPath(path)) {
    ArtVpath* tmp_path = path;
    path = CloseOpenPath(tmp_path);
    art_free(tmp_path);
  }
  
  ArtVpath* perturbedVP = art_vpath_perturb(path);
  art_free(path);
  
  ArtSVP* svp = art_svp_from_vpath(perturbedVP);
  art_free(perturbedVP);
  
  ArtSVP* uncrossedSVP = art_svp_uncross(svp);
  art_svp_free(svp);
  
  PRUint16 fillrule;
  source->GetFillRule(&fillrule);
  ArtWindRule wind;
  switch (fillrule) {
    case nsISVGGeometrySource::FILL_RULE_NONZERO:
      wind = ART_WIND_RULE_NONZERO;
      break;
    case nsISVGGeometrySource::FILL_RULE_EVENODD:
      wind = ART_WIND_RULE_ODDEVEN;
      break;
    default:
      NS_ERROR("not reached");
  }
  
  mSvp = art_svp_rewind_uncrossed (uncrossedSVP, wind);
  art_svp_free (uncrossedSVP);

}


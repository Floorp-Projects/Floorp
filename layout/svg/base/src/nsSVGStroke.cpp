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

#include "nsSVGStroke.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "prdtoa.h"
#include "nsMemory.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

void
nsSVGStroke::Build(ArtVpath* path, const nsSVGStrokeStyle& style)
{
  if (mSvp)
    art_svp_free(mSvp);

  ArtPathStrokeCapType captype;
  switch(style.linecap) {
    case NS_STYLE_STROKE_LINECAP_BUTT:
      captype = ART_PATH_STROKE_CAP_BUTT;
      break;
    case NS_STYLE_STROKE_LINECAP_ROUND:
      captype = ART_PATH_STROKE_CAP_ROUND;
      break;
    case NS_STYLE_STROKE_LINECAP_SQUARE:
      captype = ART_PATH_STROKE_CAP_SQUARE;
      break;
    default:
      NS_ERROR("not reached");
  }

  ArtPathStrokeJoinType jointype;
  switch(style.linejoin) {
    case NS_STYLE_STROKE_LINEJOIN_MITER:
      jointype = ART_PATH_STROKE_JOIN_MITER;
      break;
    case NS_STYLE_STROKE_LINEJOIN_ROUND:
      jointype = ART_PATH_STROKE_JOIN_ROUND;
      break;
    case NS_STYLE_STROKE_LINEJOIN_BEVEL:
      jointype = ART_PATH_STROKE_JOIN_BEVEL;
      break;
    default:
      NS_ERROR("not reached");
  }

  if (style.dasharray.Length() > 0) {
    ArtVpathDash dash;
    dash.offset = style.dashoffset;

    // XXX parsing of the dasharray string should be done elsewhere

    char* str;
    {
      nsAutoString temp(style.dasharray);
      str = ToNewCString(temp);
    }

    // array elements are separated by commas. count them to get our
    // max no of elems. 
    dash.n_dash = 0;
    char* cp = str;
    while (*cp) {
      if (*cp == ',')
        ++dash.n_dash;
      ++cp;
    }
    ++dash.n_dash;

    if (dash.n_dash > 0) {
      // get the elements
      dash.dash = new double[dash.n_dash];
      cp = str;
      char *elem;
      int count = 0;
      while ((elem = nsCRT::strtok(cp, "',", &cp))) {
        char *end;
        dash.dash[count++] = PR_strtod(elem, &end);
#ifdef DEBUG
        printf("[%f]",dash.dash[count-1]);
#endif
      }
      dash.n_dash = count;
      nsMemory::Free(str);

      // create a dashed vpath:
      path = art_vpath_dash(path, &dash);

      // clean up
      delete[] dash.dash;
    }
  }
    
  
  mSvp = art_svp_vpath_stroke(path,
                              jointype,
                              captype,
                              style.width,
                              style.miterlimit,
                              getFlatness());
}

double nsSVGStroke::getFlatness()
{
  double flatness = 0.5;
  
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefBranch) return flatness;

  nsXPIDLCString valuestr;
  if (NS_SUCCEEDED(prefBranch->GetCharPref("svg.stroke_flatness", getter_Copies(valuestr)))) {
    flatness = PR_strtod(valuestr, nsnull);
  }
  return flatness;
}

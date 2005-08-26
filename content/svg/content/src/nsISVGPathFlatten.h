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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef __NS_ISVGPATHFLATTEN_H__
#define __NS_ISVGPATHFLATTEN_H__

#include "nsISupports.h"
#include <stdlib.h>
#include <math.h>

////////////////////////////////////////////////////////////////////////
// nsISVGPathFlatten

#define NS_ISVGPATHFLATTEN_IID \
{ 0xefbe2079, 0x1d7b, 0x492c, { 0x90, 0x10, 0xa7, 0xf3, 0xe5, 0x8b, 0x35, 0xab } }

#define NS_SVGPATHFLATTEN_LINE 0
#define NS_SVGPATHFLATTEN_MOVE 1

class nsSVGPathData
{
private:
  PRUint32 arraysize;

public:
  PRUint32 count;
  float *x;
  float *y;
  PRUint8 *type;
  
  nsSVGPathData() : arraysize(0), count(0), x(nsnull), y(nsnull), type(nsnull) {}
  ~nsSVGPathData() {
    if (x) free(x);
    if (y) free(y);
    if (type) free(type);
  }

  void AddPoint(float aX, float aY, PRUint8 aType) {
    if (count + 1 > arraysize) {
      if (!arraysize)
        arraysize = 16;
      x = (float *) realloc(x, 2*arraysize*sizeof(float));
      y = (float *) realloc(y, 2*arraysize*sizeof(float));
      type = (PRUint8 *) realloc(type, 2*arraysize*sizeof(PRUint8));
      arraysize *= 2;
    }
    x[count] = aX;
    y[count] = aY;
    type[count] = aType;
    count++;
  }

  float Length() {
    float xx, yy, length = 0;
    for (PRUint32 i = 0; i < count; i++) {
      if (type[i] == NS_SVGPATHFLATTEN_LINE) {
        float dx = x[i] - xx;
        float dy = y[i] - yy;
        length += sqrt(dx*dx + dy*dy);
      }
      xx = x[i];
      yy = y[i];
    }
    return length;
  }
};

class nsISVGPathFlatten : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISVGPATHFLATTEN_IID; return iid; }
  
  NS_IMETHOD GetFlattenedPath(nsSVGPathData **_retval, nsIFrame *parent = nsnull)=0;
};

#endif // __NS_ISVGPATHFLATTEN_H__

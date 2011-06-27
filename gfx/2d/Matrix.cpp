/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "Matrix.h"
#include <math.h>

namespace mozilla {
namespace gfx {

Matrix
Matrix::Rotation(Float aAngle)
{
  Matrix newMatrix;

  Float s = sin(aAngle);
  Float c = cos(aAngle);

  newMatrix._11 = c;
  newMatrix._12 = s;
  newMatrix._21 = -s;
  newMatrix._22 = c;
  
  return newMatrix;
}

Rect
Matrix::TransformBounds(const Rect &aRect) const
{
  int i;
  Point quad[4];
  Float min_x, max_x;
  Float min_y, max_y;

  quad[0] = *this * aRect.TopLeft();
  quad[1] = *this * aRect.TopRight();
  quad[2] = *this * aRect.BottomLeft();
  quad[3] = *this * aRect.BottomRight();

  min_x = max_x = quad[0].x;
  min_y = max_y = quad[0].y;

  for (i = 1; i < 4; i++) {
    if (quad[i].x < min_x)
      min_x = quad[i].x;
    if (quad[i].x > max_x)
      max_x = quad[i].x;

    if (quad[i].y < min_y)
      min_y = quad[i].y;
    if (quad[i].y > max_y)
      max_y = quad[i].y;
  }

  return Rect(min_x, min_y, max_x - min_x, max_y - min_y);
}

}
}

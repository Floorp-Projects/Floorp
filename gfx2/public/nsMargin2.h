/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef NSMARGIN_H
#define NSMARGIN_H

#include "gfxtypes.h"

struct nsMargin2 {
  gfx_coord left, top, right, bottom;
  
  // Constructors
  nsMargin2() {}
  nsMargin2(const nsMargin2& aMargin) {*this = aMargin;}
  nsMargin2(gfx_coord aLeft,  gfx_coord aTop, gfx_coord aRight, gfx_coord aBottom) {
    left = aLeft; top = aTop;
    right = aRight; bottom = aBottom;
  }

  void SizeTo(gfx_coord aLeft,  gfx_coord aTop,
              gfx_coord aRight, gfx_coord aBottom) {
    left = aLeft; top = aTop;
    right = aRight; bottom = aBottom;
  }
  void SizeBy(gfx_coord aLeft, gfx_coord  aTop,
              gfx_coord aRight, gfx_coord aBottom) {
    left += aLeft; top += aTop;
    right += aRight; bottom += aBottom;
  }

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  PRBool operator==(const nsMargin2& aMargin) const {
    return (PRBool) ((left == aMargin.left) && (top == aMargin.top) &&
                     (right == aMargin.right) && (bottom == aMargin.bottom));
  }
  PRBool operator!=(const nsMargin2& aMargin) const {
    return (PRBool) ((left != aMargin.left) || (top != aMargin.top) ||
                     (right != aMargin.right) || (bottom != aMargin.bottom));
  }
  nsMargin2 operator+(const nsMargin2& aMargin) const {
    return nsMargin2(left + aMargin.left, top + aMargin.top,
                    right + aMargin.right, bottom + aMargin.bottom);
  }
  nsMargin2 operator-(const nsMargin2& aMargin) const {
    return nsMargin2(left - aMargin.left, top - aMargin.top,
                    right - aMargin.right, bottom - aMargin.bottom);
  }
  nsMargin2& operator+=(const nsMargin2& aMargin) {
    left += aMargin.left;
    top += aMargin.top;
    right += aMargin.right;
    bottom += aMargin.bottom;
    return *this;
  }
  nsMargin2& operator-=(const nsMargin2& aMargin) {
    left -= aMargin.left;
    top -= aMargin.top;
    right -= aMargin.right;
    bottom -= aMargin.bottom;
    return *this;
  }
};

#endif /* NSMARGIN_H */

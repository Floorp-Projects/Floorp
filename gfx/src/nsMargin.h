/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef NSMARGIN_H
#define NSMARGIN_H

#include "nsCoord.h"

struct nsMargin {
  nscoord left, top, right, bottom;

  // Constructors
  nsMargin() {}
  nsMargin(const nsMargin& aMargin) {*this = aMargin;}
  nsMargin(nscoord aLeft,  nscoord aTop,
           nscoord aRight, nscoord aBottom) {left = aLeft; top = aTop;
                                             right = aRight; bottom = aBottom;}

  void SizeTo(nscoord aLeft,  nscoord aTop,
              nscoord aRight, nscoord aBottom) {left = aLeft; top = aTop;
                                                right = aRight; bottom = aBottom;}
  void SizeBy(nscoord aLeft, nscoord  aTop,
              nscoord aRight, nscoord aBottom) {left += aLeft; top += aTop;
                                                right += aRight; bottom += aBottom;}

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  PRBool operator==(const nsMargin& aMargin) const {
    return (PRBool) ((left == aMargin.left) && (top == aMargin.top) &&
                     (right == aMargin.right) && (bottom == aMargin.bottom));
  }
  PRBool operator!=(const nsMargin& aMargin) const {
    return (PRBool) ((left != aMargin.left) || (top != aMargin.top) ||
                     (right != aMargin.right) || (bottom != aMargin.bottom));
  }
  nsMargin operator+(const nsMargin& aMargin) const {
    return nsMargin(left + aMargin.left, top + aMargin.top,
                    right + aMargin.right, bottom + aMargin.bottom);
  }
  nsMargin operator-(const nsMargin& aMargin) const {
    return nsMargin(left - aMargin.left, top - aMargin.top,
                    right - aMargin.right, bottom - aMargin.bottom);
  }
  nsMargin& operator+=(const nsMargin& aMargin) {left += aMargin.left;
                                                 top += aMargin.top;
                                                 right += aMargin.right;
                                                 bottom += aMargin.bottom;
                                                 return *this;}
  nsMargin& operator-=(const nsMargin& aMargin) {left -= aMargin.left;
                                                 top -= aMargin.top;
                                                 right -= aMargin.right;
                                                 bottom -= aMargin.bottom;
                                                 return *this;}
};

#endif /* NSMARGIN_H */

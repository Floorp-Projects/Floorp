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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef NSMARGIN_H
#define NSMARGIN_H

#include "nsCoord.h"
#include "nsPoint.h"
#include "gfxCore.h"

struct nsMargin {
  nscoord top, right, bottom, left;

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

  nscoord LeftRight() const { return left + right; }
  nscoord TopBottom() const { return top + bottom; }

  nsPoint TopLeft() const { return nsPoint(left, top); }

#if (NS_SIDE_TOP == 0) && (NS_SIDE_RIGHT == 1) && (NS_SIDE_BOTTOM == 2) && (NS_SIDE_LEFT == 3)
  nscoord& side(PRUint8 aSide) {
    NS_PRECONDITION(aSide <= NS_SIDE_LEFT, "Out of range side");
    return *(&top + aSide);
  }    

  nscoord side(PRUint8 aSide) const {
    NS_PRECONDITION(aSide <= NS_SIDE_LEFT, "Out of range side");
    return *(&top + aSide);
  }    
#else
#error "Somebody changed the side constants."
#endif

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

  PRBool IsZero() { return !left && !top && !right && !bottom; }
};

struct nsIntMargin {
  PRInt32 top, right, bottom, left;

  // Constructors
  nsIntMargin() {}
  nsIntMargin(const nsIntMargin& aMargin) {*this = aMargin;}
  nsIntMargin(PRInt32 aLeft,  PRInt32 aTop,
              PRInt32 aRight, PRInt32 aBottom) {left = aLeft; top = aTop;
                                                right = aRight; bottom = aBottom;}
  void SizeTo(PRInt32 aLeft,  PRInt32 aTop,
              PRInt32 aRight, PRInt32 aBottom) {left = aLeft; top = aTop;
                                                right = aRight; bottom = aBottom;}

  PRInt32 LeftRight() const { return left + right; }
  PRInt32 TopBottom() const { return top + bottom; }

  nsPoint TopLeft() const { return nsPoint(left, top); }

  PRInt32& side(PRUint8 aSide) {
    NS_PRECONDITION(aSide <= NS_SIDE_LEFT, "Out of range side");
    return *(&top + aSide);
  }

  PRInt32 side(PRUint8 aSide) const {
    NS_PRECONDITION(aSide <= NS_SIDE_LEFT, "Out of range side");
    return *(&top + aSide);
  }

  PRBool operator!=(const nsIntMargin& aMargin) const {
    return (PRBool) ((left != aMargin.left) || (top != aMargin.top) ||
                     (right != aMargin.right) || (bottom != aMargin.bottom));
  }
  nsIntMargin operator+(const nsIntMargin& aMargin) const {
    return nsIntMargin(left + aMargin.left, top + aMargin.top,
                    right + aMargin.right, bottom + aMargin.bottom);
  }

  PRBool IsZero() { return !left && !top && !right && !bottom; }
};

#endif /* NSMARGIN_H */

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

#ifndef NSSIZE_H
#define NSSIZE_H

#include "nsCoord.h"

// Maximum allowable size
#define NS_MAXSIZE nscoord_MAX

struct nsSize {
  nscoord width, height;

  // Constructors
  nsSize() {}
  nsSize(const nsSize& aSize) {width = aSize.width; height = aSize.height;}
  nsSize(nscoord aWidth, nscoord aHeight) {width = aWidth; height = aHeight;}

  void SizeTo(nscoord aWidth, nscoord aHeight) {width = aWidth; height = aHeight;}
  void SizeBy(nscoord aDeltaWidth, nscoord aDeltaHeight) {width += aDeltaWidth;
                                                          height += aDeltaHeight;}

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  PRBool  operator==(const nsSize& aSize) const {
    return (PRBool) ((width == aSize.width) && (height == aSize.height));
  }
  PRBool  operator!=(const nsSize& aSize) const {
    return (PRBool) ((width != aSize.width) || (height != aSize.height));
  }
  nsSize operator+(const nsSize& aSize) const {
    return nsSize(width + aSize.width, height + aSize.height);
  }
  nsSize& operator+=(const nsSize& aSize) {width += aSize.width;
                                           height += aSize.height;
                                           return *this;}
};

struct nsIntSize {
  PRInt32 width, height;

  nsIntSize() {}
  nsIntSize(const nsIntSize& aSize) {width = aSize.width; height = aSize.height;}
  nsIntSize(PRInt32 aWidth, PRInt32 aHeight) {width = aWidth; height = aHeight;}

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  PRBool  operator==(const nsIntSize& aSize) const {
    return (PRBool) ((width == aSize.width) && (height == aSize.height));
  }
  PRBool  operator!=(const nsIntSize& aSize) const {
    return (PRBool) ((width != aSize.width) || (height != aSize.height));
  }

  void SizeTo(PRInt32 aWidth, PRInt32 aHeight) {width = aWidth; height = aHeight;}
};

#endif /* NSSIZE_H */

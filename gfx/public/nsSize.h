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

#ifndef NSSIZE_H
#define NSSIZE_H

#include "nsCoord.h"

// Maximum allowable size
#define NS_MAXSIZE  nscoord(1 << 30)

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
  nsSize operator-(const nsSize& aSize) const {
    return nsSize(width - aSize.width, height - aSize.height);
  }
  nsSize& operator+=(const nsSize& aSize) {width += aSize.width;
                                           height += aSize.height;
                                           return *this;}
  nsSize& operator-=(const nsSize& aSize) {width -= aSize.width;
                                           height -= aSize.height;
                                           return *this;}
};

#endif /* NSSIZE_H */

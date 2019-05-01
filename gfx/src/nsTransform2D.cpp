/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTransform2D.h"

void nsTransform2D ::TransformCoord(nscoord* ptX, nscoord* ptY) const {
  *ptX = NSToCoordRound(*ptX * m00 + m20);
  *ptY = NSToCoordRound(*ptY * m11 + m21);
}

void nsTransform2D ::TransformCoord(nscoord* aX, nscoord* aY, nscoord* aWidth,
                                    nscoord* aHeight) const {
  nscoord x2 = *aX + *aWidth;
  nscoord y2 = *aY + *aHeight;
  TransformCoord(aX, aY);
  TransformCoord(&x2, &y2);
  *aWidth = x2 - *aX;
  *aHeight = y2 - *aY;
}

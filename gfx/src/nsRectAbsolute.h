/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSRECTABSOLUTE_H
#define NSRECTABSOLUTE_H

#include "mozilla/gfx/RectAbsolute.h"
#include "nsCoord.h"
#include "nsRect.h"

struct nsRectAbsolute :
  public mozilla::gfx::BaseRectAbsolute<nscoord, nsRectAbsolute, nsRect> {
  typedef mozilla::gfx::BaseRectAbsolute<nscoord, nsRectAbsolute, nsRect> Super;

  nsRectAbsolute() : Super() {}
  nsRectAbsolute(nscoord aX1, nscoord aY1, nscoord aX2, nscoord aY2) :
      Super(aX1, aY1, aX2, aY2) {}
};

#endif /* NSRECTABSOLUTE_H */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSCOORDBOX_H
#define NSCOORDBOX_H

#include "mozilla/gfx/Box.h"
#include "nsCoord.h"
#include "nsRect.h"

// Would like to call this nsBox, but can't because nsBox is a frame type.
struct nsCoordBox :
  public mozilla::gfx::BaseBox<nscoord, nsCoordBox, nsRect> {
  typedef mozilla::gfx::BaseBox<nscoord, nsCoordBox, nsRect> Super;

  nsCoordBox() : Super() {}
  nsCoordBox(nscoord aX1, nscoord aY1, nscoord aX2, nscoord aY2) :
      Super(aX1, aY1, aX2, aY2) {}
};

#endif /* NSCOORDBOX_H */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSMARGIN_H
#define NSMARGIN_H

#include "nsCoord.h"
#include "mozilla/gfx/BaseMargin.h"
#include "mozilla/gfx/Rect.h"

struct nsMargin : public mozilla::gfx::BaseMargin<nscoord, nsMargin> {
  typedef mozilla::gfx::BaseMargin<nscoord, nsMargin> Super;

  // Constructors
  nsMargin() = default;
  nsMargin(const nsMargin& aMargin) = default;
  nsMargin(nscoord aTop, nscoord aRight, nscoord aBottom, nscoord aLeft)
      : Super(aTop, aRight, aBottom, aLeft) {}
};

typedef mozilla::gfx::IntMargin nsIntMargin;

#endif /* NSMARGIN_H */

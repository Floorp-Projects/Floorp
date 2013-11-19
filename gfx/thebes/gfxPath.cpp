/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPath.h"
#include "mozilla/gfx/2D.h"

#include "cairo.h"

using namespace mozilla::gfx;

gfxPath::gfxPath(cairo_path_t* aPath)
  : mPath(aPath)
{
}

gfxPath::gfxPath(Path* aPath)
  : mPath(nullptr)
  , mMoz2DPath(aPath)
{
}

gfxPath::~gfxPath()
{
    cairo_path_destroy(mPath);
}

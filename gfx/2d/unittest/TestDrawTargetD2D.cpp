/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestDrawTargetD2D.h"

using namespace mozilla::gfx;
TestDrawTargetD2D::TestDrawTargetD2D()
{
  ::D3D10CreateDevice1(NULL,
                       D3D10_DRIVER_TYPE_HARDWARE,
                       NULL,
                       D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                       D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                       D3D10_FEATURE_LEVEL_10_0,
                       D3D10_1_SDK_VERSION,
                       byRef(mDevice));

  Factory::SetDirect3D10Device(mDevice);

  mDT = Factory::CreateDrawTarget(BACKEND_DIRECT2D, IntSize(DT_WIDTH, DT_HEIGHT), FORMAT_B8G8R8A8);
}

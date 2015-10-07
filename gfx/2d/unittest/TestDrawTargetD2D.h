/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "TestDrawTargetBase.h"

#include <d3d10_1.h>

class TestDrawTargetD2D : public TestDrawTargetBase
{
public:
  TestDrawTargetD2D();

private:
  mozilla::RefPtr<ID3D10Device1> mDevice;
};

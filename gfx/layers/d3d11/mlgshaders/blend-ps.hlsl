/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common.hlsl"
#include "common-ps.hlsl"
#include "blend-common.hlsl"
#include "../BlendingHelpers.hlslh"

Texture2D simpleTex : register(ps, t0);
Texture2D tBackdrop : register(ps, t1);

#include "blend-ps-generated.hlslh"

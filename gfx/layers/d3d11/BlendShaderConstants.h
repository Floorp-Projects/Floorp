/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LAYERS_D3D11_BLENDSHADERCONSTANTS_H_
#define MOZILLA_GFX_LAYERS_D3D11_BLENDSHADERCONSTANTS_H_

// These constants are shared between CompositorD3D11 and the blend pixel
// shader.
#define PS_LAYER_RGB 0
#define PS_LAYER_RGBA 1
#define PS_LAYER_YCBCR 2
#define PS_LAYER_COLOR 3
#define PS_LAYER_NV12 4

#endif  // MOZILLA_GFX_LAYERS_D3D11_BLENDSHADERCONSTANTS_H_

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorBench_h
#define mozilla_layers_CompositorBench_h

#include "mozilla/gfx/Rect.h"           // for Rect

namespace mozilla {
namespace layers {

class Compositor;

// Uncomment this line to rebuild with compositor bench.
// #define MOZ_COMPOSITOR_BENCH

#ifdef MOZ_COMPOSITOR_BENCH
void CompositorBench(Compositor* aCompositor, const gfx::Rect& aScreenRect);
#else
static inline void CompositorBench(Compositor* aCompositor, const gfx::Rect& aScreenRect) {}
#endif

}
}

#endif



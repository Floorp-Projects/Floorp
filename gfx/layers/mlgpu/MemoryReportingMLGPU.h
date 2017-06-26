/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_MemoryReportingMLGPU_h
#define mozilla_gfx_layers_mlgpu_MemoryReportingMLGPU_h

#include "mozilla/Atomics.h"

namespace mozilla {
namespace layers {
namespace mlg {

void InitializeMemoryReporters();

extern mozilla::Atomic<size_t> sConstantBufferUsage;
extern mozilla::Atomic<size_t> sVertexBufferUsage;
extern mozilla::Atomic<size_t> sRenderTargetUsage;

} // namespace mlg
} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_MemoryReportingMLGPU_h

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryReportingMLGPU.h"
#include "nsIMemoryReporter.h"

namespace mozilla {
namespace layers {
namespace mlg {

mozilla::Atomic<size_t> sConstantBufferUsage;
mozilla::Atomic<size_t> sVertexBufferUsage;
mozilla::Atomic<size_t> sRenderTargetUsage;

class MemoryReportingMLGPU final : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData,
                            bool aAnonymize) override
  {
    if (sConstantBufferUsage) {
      MOZ_COLLECT_REPORT(
        "mlgpu-constant-buffers", KIND_OTHER, UNITS_BYTES,
        sConstantBufferUsage,
        "Advanced Layers shader constant buffers.");
    }
    if (sVertexBufferUsage) {
      MOZ_COLLECT_REPORT(
        "mlgpu-vertex-buffers", KIND_OTHER, UNITS_BYTES,
        sVertexBufferUsage,
        "Advanced Layers shader vertex buffers.");
    }
    if (sRenderTargetUsage) {
      MOZ_COLLECT_REPORT(
        "mlgpu-render-targets", KIND_OTHER, UNITS_BYTES,
        sRenderTargetUsage,
        "Advanced Layers render target textures and depth buffers.");
    }
    return NS_OK;
  }

private:
  ~MemoryReportingMLGPU() {}
};

NS_IMPL_ISUPPORTS(MemoryReportingMLGPU, nsIMemoryReporter);

void
InitializeMemoryReporters()
{
  RegisterStrongMemoryReporter(new MemoryReportingMLGPU());
}

} // namespace mlg
} // namespace layers
} // namespace mozilla

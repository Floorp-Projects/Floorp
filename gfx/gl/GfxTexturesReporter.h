/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXTEXTURESREPORTER_H_
#define GFXTEXTURESREPORTER_H_

#include "mozilla/Atomics.h"
#include "nsIMemoryReporter.h"
#include "GLTypes.h"

namespace mozilla {
namespace gl {

class GfxTexturesReporter final : public nsIMemoryReporter
{
    ~GfxTexturesReporter() {}

public:
    NS_DECL_ISUPPORTS

    GfxTexturesReporter()
    {
#ifdef DEBUG
        // There must be only one instance of this class, due to |sAmount|
        // being static.  Assert this.
        static bool hasRun = false;
        MOZ_ASSERT(!hasRun);
        hasRun = true;
#endif
    }

    enum MemoryUse {
        // when memory being allocated is reported to a memory reporter
        MemoryAllocated,
        // when memory being freed is reported to a memory reporter
        MemoryFreed
    };

    // When memory is used/freed for tile textures, call this method to update
    // the value reported by this memory reporter.
    static void UpdateAmount(MemoryUse action, size_t amount);

    static void UpdateWasteAmount(size_t delta) {
      sTileWasteAmount += delta;
    }

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize) override
    {
        MOZ_COLLECT_REPORT(
            "gfx-tiles-waste", KIND_OTHER, UNITS_BYTES,
            int64_t(sTileWasteAmount),
            "Memory lost due to tiles extending past content boundaries");

        MOZ_COLLECT_REPORT(
            "gfx-textures", KIND_OTHER, UNITS_BYTES,
            int64_t(sAmount),
            "Memory used for storing GL textures.");

        MOZ_COLLECT_REPORT(
            "gfx-textures-peak", KIND_OTHER, UNITS_BYTES,
            int64_t(sPeakAmount),
            "Peak memory used for storing GL textures.");

        return NS_OK;
    }

private:
    static Atomic<size_t> sAmount;
    static Atomic<size_t> sPeakAmount;
    // Count the amount of memory lost to tile waste
    static Atomic<size_t> sTileWasteAmount;
};

class GfxTextureWasteTracker {
public:
  GfxTextureWasteTracker()
    : mBytes(0)
  {
    MOZ_COUNT_CTOR(GfxTextureWasteTracker);
  }

  void Update(int32_t aPixelArea, int32_t aBytesPerPixel) {
    GfxTexturesReporter::UpdateWasteAmount(-mBytes);
    mBytes = aPixelArea * aBytesPerPixel;
    GfxTexturesReporter::UpdateWasteAmount(mBytes);
  }

  ~GfxTextureWasteTracker() {
    GfxTexturesReporter::UpdateWasteAmount(-mBytes);
    MOZ_COUNT_DTOR(GfxTextureWasteTracker);
  }
private:
  GfxTextureWasteTracker(const GfxTextureWasteTracker& aRef);

  int32_t mBytes;
};

} // namespace gl
} // namespace mozilla

#endif // GFXTEXTURESREPORTER_H_

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXTEXTURESREPORTER_H_
#define GFXTEXTURESREPORTER_H_

#include "nsIMemoryReporter.h"
#include "GLTypes.h"

namespace mozilla {
namespace gl {

class GfxTexturesReporter MOZ_FINAL : public nsIMemoryReporter
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
    static void UpdateAmount(MemoryUse action, GLenum format, GLenum type,
                             int32_t tileWidth, int32_t tileHeight);

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize)
    {
        return MOZ_COLLECT_REPORT(
            "gfx-textures", KIND_OTHER, UNITS_BYTES, sAmount,
            "Memory used for storing GL textures.");
    }

private:
    static int64_t sAmount;
};

}
}

#endif // GFXTEXTURESREPORTER_H_

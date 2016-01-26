/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxTexturesReporter.h"

using namespace mozilla;
using namespace mozilla::gl;

NS_IMPL_ISUPPORTS(GfxTexturesReporter, nsIMemoryReporter)

Atomic<size_t> GfxTexturesReporter::sAmount(0);
Atomic<size_t> GfxTexturesReporter::sTileWasteAmount(0);

/* static */ void
GfxTexturesReporter::UpdateAmount(MemoryUse action, size_t amount)
{
    if (action == MemoryFreed) {
        MOZ_RELEASE_ASSERT(amount <= sAmount);
        sAmount -= amount;
    } else {
        sAmount += amount;
    }
}

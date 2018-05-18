/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SkMemoryReporter.h"

#include "skia/include/core/SkGraphics.h"

namespace mozilla {
namespace gfx {

NS_IMETHODIMP
SkMemoryReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                 nsISupports* aData,
                                 bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/skia-font-cache", KIND_HEAP, UNITS_BYTES,
    SkGraphics::GetFontCacheUsed(),
    "Memory used in the skia font cache.");
  return NS_OK;
}

NS_IMPL_ISUPPORTS(SkMemoryReporter, nsIMemoryReporter);

} // namespace gfx
} // namespace mozilla

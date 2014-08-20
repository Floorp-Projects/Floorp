/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceUtils.h"

#include "prlog.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsPrintfCString.h"

namespace mozilla {

#if defined(PR_LOGGING)
nsCString
DumpTimeRanges(dom::TimeRanges* aRanges)
{
  nsCString dump;

  dump = "[";

  for (uint32_t i = 0; i < aRanges->Length(); ++i) {
    if (i > 0) {
      dump += ", ";
    }
    ErrorResult dummy;
    dump += nsPrintfCString("(%f, %f)", aRanges->Start(i, dummy), aRanges->End(i, dummy));
  }

  dump += "]";

  return dump;
}
#endif

} // namespace mozilla

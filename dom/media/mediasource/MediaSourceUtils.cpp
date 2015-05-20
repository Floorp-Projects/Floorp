/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceUtils.h"

#include "mozilla/Logging.h"
#include "nsPrintfCString.h"

namespace mozilla {

nsCString
DumpTimeRanges(const media::TimeIntervals& aRanges)
{
  nsCString dump;

  dump = "[";

  for (uint32_t i = 0; i < aRanges.Length(); ++i) {
    if (i > 0) {
      dump += ", ";
    }
    dump += nsPrintfCString("(%f, %f)",
                            aRanges.Start(i).ToSeconds(),
                            aRanges.End(i).ToSeconds());
  }

  dump += "]";

  return dump;
}

} // namespace mozilla

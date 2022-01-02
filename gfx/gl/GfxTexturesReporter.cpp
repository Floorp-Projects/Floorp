/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <sstream>
#include "nsExceptionHandler.h"
#include "GfxTexturesReporter.h"
#include "mozilla/StaticPrefs_gfx.h"

using namespace mozilla::gl;

NS_IMPL_ISUPPORTS(GfxTexturesReporter, nsIMemoryReporter)

mozilla::Atomic<size_t> GfxTexturesReporter::sAmount(0);
mozilla::Atomic<size_t> GfxTexturesReporter::sPeakAmount(0);
mozilla::Atomic<size_t> GfxTexturesReporter::sTileWasteAmount(0);

static std::string FormatBytes(size_t amount) {
  std::stringstream stream;
  int depth = 0;
  double val = amount;
  while (val > 1024) {
    val /= 1024;
    depth++;
  }

  const char* unit;
  switch (depth) {
    case 0:
      unit = "bytes";
      break;
    case 1:
      unit = "KB";
      break;
    case 2:
      unit = "MB";
      break;
    case 3:
      unit = "GB";
      break;
    default:
      unit = "";
      break;
  }

  stream << val << " " << unit;
  return stream.str();
}

/* static */
void GfxTexturesReporter::UpdateAmount(MemoryUse action, size_t amount) {
  if (action == MemoryFreed) {
    MOZ_RELEASE_ASSERT(
        amount <= sAmount,
        "GFX: Current texture usage greater than update amount.");
    sAmount -= amount;

    if (StaticPrefs::gfx_logging_texture_usage_enabled_AtStartup()) {
      printf_stderr("Current texture usage: %s\n",
                    FormatBytes(sAmount).c_str());
    }
  } else {
    sAmount += amount;
    if (sAmount > sPeakAmount) {
      sPeakAmount.exchange(sAmount);
      if (StaticPrefs::gfx_logging_peak_texture_usage_enabled_AtStartup()) {
        printf_stderr("Peak texture usage: %s\n",
                      FormatBytes(sPeakAmount).c_str());
      }
    }
  }

  CrashReporter::AnnotateTexturesSize(sAmount);
}

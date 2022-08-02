/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BrowserDefines_h
#define mozilla_BrowserDefines_h

#include <cstddef>
#include "mozilla/CmdLineAndEnvUtils.h"

namespace mozilla {
namespace browser {
constexpr static const char* kRequiredArguments[] = {"url", "private-window"};
#ifdef XP_WIN
#  define ATTEMPTING_DEELEVATION_FLAG "attempting-deelevation"
constexpr static const char* kOptionalArguments[] = {
    ATTEMPTING_DEELEVATION_FLAG};
#else
constexpr static auto kOptionalArguments = nullptr;
#endif
}  // namespace browser

template <typename CharT>
inline void EnsureBrowserCommandlineSafe(int aArgc, CharT** aArgv) {
  mozilla::EnsureCommandlineSafe(aArgc, aArgv, browser::kRequiredArguments,
                                 browser::kOptionalArguments);
}
}  // namespace mozilla

#endif

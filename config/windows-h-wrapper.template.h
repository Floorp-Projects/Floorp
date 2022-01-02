/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_windows_h
#define mozilla_windows_h

// Include the "real" windows.h header.
//
// Also turn off deprecation warnings, as we may be wrapping deprecated fns.

#pragma GCC system_header
#include_next <windows.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Check if the header should be disabled
#if defined(MOZ_DISABLE_WINDOWS_WRAPPER)
#define MOZ_WINDOWS_WRAPPER_DISABLED_REASON "explicitly disabled"

#elif !defined(__cplusplus)
#define MOZ_WINDOWS_WRAPPER_DISABLED_REASON "non-C++ source file"

#else
// We're allowed to wrap in the current context. Define `MOZ_WRAPPED_WINDOWS_H`
// to note that fact, and perform the wrapping.
#define MOZ_WRAPPED_WINDOWS_H
extern "C++" {

${decls}

} // extern "C++"

#undef GetCurrentTime // Use GetTickCount() instead.

#endif // enabled

#pragma GCC diagnostic pop

#endif // !defined(mozilla_windows_h)

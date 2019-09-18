/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_windows_h
#define mozilla_windows_h

// Include the "real" windows.h header. On clang/gcc, this can be done with the
// `include_next` feature, however MSVC requires a direct include path.
//
// Also turn off deprecation warnings, as we may be wrapping deprecated fns.

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC system_header
#  include_next <windows.h>

#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#else
#  include <${header_path}>

#  pragma warning(push)
#  pragma warning(disable: 4996 4995)
#endif // defined(__GNUC__) || defined(__clang__)

// Check if the header should be disabled
#if defined(MOZ_DISABLE_WINDOWS_WRAPPER)
#define MOZ_WINDOWS_WRAPPER_DISABLED_REASON "explicitly disabled"

#elif !defined(__cplusplus)
#define MOZ_WINDOWS_WRAPPER_DISABLED_REASON "non-C++ source file"

#elif !defined(__GNUC__) && !defined(__clang__) && !defined(_DLL)
#define MOZ_WINDOWS_WRAPPER_DISABLED_REASON "non-dynamic RTL"

#else
// We're allowed to wrap in the current context. Define `MOZ_WRAPPED_WINDOWS_H`
// to note that fact, and perform the wrapping.
#define MOZ_WRAPPED_WINDOWS_H
extern "C++" {

${decls}

} // extern "C++"

#undef GetCurrentTime // Use GetTickCount() instead.

#endif // enabled

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#else
#  pragma warning(pop)
#endif // defined(__GNUC__) || defined(__clang__)

#endif // !defined(mozilla_windows_h)

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_AllOfDcomp_h
#define mozilla_gfx_AllOfDcomp_h

// Getting everything that we need in dcomp.h defined means messing with some
// defines.

#if (_WIN32_WINNT < _WIN32_WINNT_WIN10)

#  define XSTR(x) STR(x)
#  define STR(x) #x
// clang-format off

#  pragma message "IDCompositionFilterEffect in dcomp.h requires _WIN32_WINNT >= _WIN32_WINNT_WIN10."
// Pedantically, it actually requires _WIN32_WINNT_WINTHRESHOLD, but that's the
// same as _WIN32_WINNT_WIN10.

#  pragma message "Forcing NTDDI_VERSION " XSTR(NTDDI_VERSION) " -> " XSTR(NTDDI_WIN10)
#  undef NTDDI_VERSION
#  define NTDDI_VERSION NTDDI_WIN10

#  pragma message "Forcing _WIN32_WINNT " XSTR(_WIN32_WINNT) " -> " XSTR(_WIN32_WINNT_WIN10)
#  undef _WIN32_WINNT
#  define _WIN32_WINNT _WIN32_WINNT_WIN10

// clang-format on
#  undef STR
#  undef XSTR

#endif

// -

#include <dcomp.h>

#endif  // mozilla_gfx_AllOfDcomp_h

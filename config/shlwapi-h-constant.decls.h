/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains a series of C-style declarations for constants defined in
 * shlwapi.h using #define. Adding a new constant should be a simple as adding
 * its name (and optionally type) to this file.
 *
 * This file is processed by make-windows-h-wrapper.py to generate a wrapper for
 * the header which removes the defines usually implementing these constants.
 *
 * Wrappers defined in this file will be declared as `constexpr` values,
 * and will have their value derived from the windows.h define.
 *
 * NOTE: This is *NOT* a real C header, but rather an input to the avove script.
 * Only basic declarations in the form found here are allowed.
 */

// XXX(glandium): We don't have any here, because they don't look like they
// could cause problems.

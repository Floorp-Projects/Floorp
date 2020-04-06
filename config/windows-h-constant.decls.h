/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains a series of C-style declarations for constants defined in
 * windows.h using #define. Adding a new constant should be a simple as adding
 * its name (and optionally type) to this file.
 *
 * This file is processed by generate-windows-h-wrapper.py to generate a wrapper
 * for the header which removes the defines usually implementing these constants.
 *
 * Wrappers defined in this file will be declared as `constexpr` values,
 * and will have their value derived from the windows.h define.
 *
 * NOTE: This is *NOT* a real C header, but rather an input to the avove script.
 * Only basic declarations in the form found here are allowed.
 */

// XXX(nika): There are a lot of these (>30k)!
// This is just a set of ones I saw in a quick scan which looked problematic.

auto CREATE_NEW;
auto CREATE_ALWAYS;
auto OPEN_EXISTING;
auto OPEN_ALWAYS;
auto TRUNCATE_EXISTING;
auto INVALID_FILE_SIZE;
auto INVALID_SET_FILE_POINTER;
auto INVALID_FILE_ATTRIBUTES;

auto ANSI_NULL;
auto UNICODE_NULL;

auto MINCHAR;
auto MAXCHAR;
auto MINSHORT;
auto MAXSHORT;
auto MINLONG;
auto MAXLONG;
auto MAXBYTE;
auto MAXWORD;
auto MAXDWORD;

auto ERROR;

auto DELETE;
auto READ_CONTROL;
auto WRITE_DAC;
auto WRITE_OWNER;
auto SYNCHRONIZE;

auto MAXIMUM_ALLOWED;
auto GENERIC_READ;
auto GENERIC_WRITE;
auto GENERIC_EXECUTE;
auto GENERIC_ALL;

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains a series of C-style function prototypes for A/W-suffixed
 * Win32 APIs defined by shlwapi.h.
 *
 * This file is processed by make-windows-h-wrapper.py to generate a wrapper for
 * the header which removes the defines usually implementing these aliases.
 *
 * Wrappers defined in this file will have the 'stdcall' calling convention,
 * will be defined as 'inline', and will only be defined if the corresponding
 * #define directive has not been #undef-ed.
 *
 * NOTE: This is *NOT* a real C header, but rather an input to the avove script.
 * Only basic declarations in the form found here are allowed.
 */

// XXX(glandium): there are a ton more, but this is the one that is causing
// immediate problems.
HRESULT GetAcceptLanguages(LPTSTR, DWORD*);

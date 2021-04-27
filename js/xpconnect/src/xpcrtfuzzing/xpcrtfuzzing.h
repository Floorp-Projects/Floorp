/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// xpcrtfuzzing.h - Functionality for XPC runtime fuzzing

#ifndef shell_xpcrtfuzzing_h
#define shell_xpcrtfuzzing_h

#include "mozilla/dom/ScriptSettings.h"  // mozilla::dom::AutoJSAPI

// This is the entry point of the XPC runtime fuzzing code from the XPC shell
int FuzzXPCRuntimeStart(mozilla::dom::AutoJSAPI* jsapi, int* argc,
                        char*** argv);

// These are the traditional libFuzzer-style functions for initialization
// and fuzzing iteration.
int FuzzXPCRuntimeInit(/* int* argc, char*** argv */);
int FuzzXPCRuntimeFuzz(const uint8_t* buf, size_t size);

#endif /* shell_xpcrtfuzzing_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// jsrtfuzzing.h - Functionality for JS runtime fuzzing

#ifndef shell_jsrtfuzzing_h
#define shell_jsrtfuzzing_h

#include "vm/JSContext.h"

namespace js {
namespace shell {

// This is the entry point of the JS runtime fuzzing code from the JS shell
int FuzzJSRuntimeStart(JSContext* cx, int* argc, char*** argv);

// These are the traditional libFuzzer-style functions for initialization
// and fuzzing iteration.
int FuzzJSRuntimeInit(int* argc, char*** argv);
int FuzzJSRuntimeFuzz(const uint8_t* buf, size_t size);

}  // namespace shell
}  // namespace js

#endif /* shell_jsrtfuzzing_h */

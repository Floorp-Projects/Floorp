/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// OSObject.h - os object for exposing posix system calls in the JS shell

#ifndef shell_OSObject_h
#define shell_OSObject_h

#include "js/TypeDecls.h"
#include "js/Utility.h"

class JSLinearString;

namespace js {
namespace shell {

#ifdef XP_WIN
constexpr char PathSeparator = '\\';
#else
constexpr char PathSeparator = '/';
#endif

struct RCFile;

/* Define an os object on the given global object. */
bool DefineOS(JSContext* cx, JS::HandleObject global, bool fuzzingSafe,
              RCFile** shellOut, RCFile** shellErr);

enum PathResolutionMode { RootRelative, ScriptRelative };

bool IsAbsolutePath(JSLinearString* filename);

JSString* ResolvePath(JSContext* cx, JS::HandleString filenameStr,
                      PathResolutionMode resolveMode);

JSObject* FileAsTypedArray(JSContext* cx, JS::HandleString pathnameStr);

JS::UniqueChars GetCWD();

}  // namespace shell
}  // namespace js

#endif /* shell_OSObject_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is the interface that the regexp engine exposes to SpiderMonkey. */

#ifndef regexp_RegExpAPI_h
#define regexp_RegExpAPI_h

#include "frontend/TokenStream.h"
#include "vm/JSContext.h"
#include "vm/RegExpShared.h"

namespace js {
namespace irregexp {

Isolate* CreateIsolate(JSContext* cx);

bool CheckPatternSyntax(JSContext* cx, frontend::TokenStreamAnyChars& ts,
                        const mozilla::Range<const char16_t> chars,
                        JS::RegExpFlags flags);
bool CheckPatternSyntax(JSContext* cx, frontend::TokenStreamAnyChars& ts,
                        HandleAtom pattern, JS::RegExpFlags flags);

bool CompilePattern(JSContext* cx, MutableHandleRegExpShared re,
                    HandleLinearString input);

}  // namespace irregexp
}  // namespace js

#endif /* regexp_RegExpAPI_h */

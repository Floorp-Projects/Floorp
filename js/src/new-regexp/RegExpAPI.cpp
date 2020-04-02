/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "new-regexp/RegExpAPI.h"

#include "new-regexp/regexp-parser.h"
#include "new-regexp/regexp-shim.h"
#include "new-regexp/regexp.h"

namespace js {
namespace irregexp {

using frontend::TokenStream;
using frontend::TokenStreamAnyChars;

using v8::internal::FlatStringReader;
using v8::internal::RegExpCompileData;
using v8::internal::RegExpParser;
using v8::internal::Zone;

Isolate* CreateIsolate(JSContext* cx) {
  auto isolate = MakeUnique<Isolate>(cx);
  if (!isolate || !isolate->init()) {
    return nullptr;
  }
  return isolate.release();
}

static bool CheckPatternSyntaxImpl(JSContext* cx, FlatStringReader* pattern,
                                   JS::RegExpFlags flags,
                                   RegExpCompileData* result) {
  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  Zone zone(allocScope.alloc());

  v8::internal::HandleScope handleScope(cx->isolate);
  return RegExpParser::ParseRegExp(cx->isolate, &zone, pattern, flags, result);
}

bool CheckPatternSyntax(JSContext* cx, TokenStreamAnyChars& ts,
                        const mozilla::Range<const char16_t> chars,
                        JS::RegExpFlags flags) {
  FlatStringReader reader(chars.begin().get(), chars.length());
  RegExpCompileData result;
  if (!CheckPatternSyntaxImpl(cx, &reader, flags, &result)) {
    // TODO: Report syntax error
    return false;
  }
  return true;
}

bool CheckPatternSyntax(JSContext* cx, TokenStreamAnyChars& ts,
                        HandleAtom pattern, JS::RegExpFlags flags) {
  FlatStringReader reader(pattern);
  RegExpCompileData result;
  if (!CheckPatternSyntaxImpl(cx, &reader, flags, &result)) {
    // TODO: Report syntax error
    return false;
  }
  return true;
}

}  // namespace irregexp
}  // namespace js

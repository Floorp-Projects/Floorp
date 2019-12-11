/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "new-regexp/regexp-shim.h"

namespace v8 {
namespace internal {

void PrintF(const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  vprintf(format, arguments);
  va_end(arguments);
}

void PrintF(FILE* out, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  vfprintf(out, format, arguments);
  va_end(arguments);
}

// Origin:
// https://github.com/v8/v8/blob/855591a54d160303349a5f0a32fab15825c708d1/src/utils/ostreams.cc#L120-L169
// (This is a hand-simplified version.)
// Writes the given character to the output escaping everything outside
// of printable ASCII range.
std::ostream& operator<<(std::ostream& os, const AsUC16& c) {
  uint16_t v = c.value;
  bool isPrint = 0x20 < v && v <= 0x7e;
  char buf[10];
  const char* format = isPrint ? "%c" : (v <= 0xFF) ? "\\x%02x" : "\\u%04x";
  snprintf(buf, sizeof(buf), format, v);
  return os << buf;
}
std::ostream& operator<<(std::ostream& os, const AsUC32& c) {
  int32_t v = c.value;
  if (v <= String::kMaxUtf16CodeUnit) {
    return os << AsUC16(v);
  }
  char buf[13];
  snprintf(buf, sizeof(buf), "\\u{%06x}", v);
  return os << buf;
}

DisallowJavascriptExecution::DisallowJavascriptExecution(Isolate* isolate)
    : nojs_(isolate->cx()) {}

// TODO: Map flags to jitoptions
bool FLAG_correctness_fuzzer_suppressions = false;
bool FLAG_enable_regexp_unaligned_accesses = false;
bool FLAG_harmony_regexp_sequence = false;
bool FLAG_regexp_interpret_all = false;
bool FLAG_regexp_mode_modifiers = false;
bool FLAG_regexp_optimization = false;
bool FLAG_regexp_peephole_optimization = false;
bool FLAG_regexp_possessive_quantifier = false;
bool FLAG_regexp_tier_up = false;
bool FLAG_trace_regexp_assembler = false;
bool FLAG_trace_regexp_bytecodes = false;
bool FLAG_trace_regexp_parser = false;
bool FLAG_trace_regexp_peephole_optimization = false;

}  // namespace internal
}  // namespace v8

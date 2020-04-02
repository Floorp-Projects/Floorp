/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "new-regexp/RegExpAPI.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"

#include "new-regexp/regexp-parser.h"
#include "new-regexp/regexp-shim.h"
#include "new-regexp/regexp.h"
#include "util/StringBuffer.h"

namespace js {
namespace irregexp {

using mozilla::AssertedCast;
using mozilla::PointerRangeSize;

using frontend::TokenStream;
using frontend::TokenStreamAnyChars;

using v8::internal::FlatStringReader;
using v8::internal::RegExpCompileData;
using v8::internal::RegExpError;
using v8::internal::RegExpParser;
using v8::internal::Zone;

static uint32_t ErrorNumber(RegExpError err) {
  switch (err) {
    case RegExpError::kNone:
      return JSMSG_NOT_AN_ERROR;
    case RegExpError::kStackOverflow:
      return JSMSG_OVER_RECURSED;
    case RegExpError::kAnalysisStackOverflow:
      return JSMSG_OVER_RECURSED;
    case RegExpError::kTooLarge:
      return JSMSG_TOO_MANY_PARENS;
    case RegExpError::kUnterminatedGroup:
      return JSMSG_MISSING_PAREN;
    case RegExpError::kUnmatchedParen:
      return JSMSG_UNMATCHED_RIGHT_PAREN;
    case RegExpError::kEscapeAtEndOfPattern:
      return JSMSG_ESCAPE_AT_END_OF_REGEXP;
    case RegExpError::kInvalidPropertyName:
      return JSMSG_INVALID_PROPERTY_NAME;
    case RegExpError::kInvalidEscape:
      return JSMSG_INVALID_IDENTITY_ESCAPE;
    case RegExpError::kInvalidDecimalEscape:
      return JSMSG_INVALID_DECIMAL_ESCAPE;
    case RegExpError::kInvalidUnicodeEscape:
      return JSMSG_INVALID_UNICODE_ESCAPE;
    case RegExpError::kNothingToRepeat:
      return JSMSG_NOTHING_TO_REPEAT;
    case RegExpError::kLoneQuantifierBrackets:
      // Note: V8 reports the same error for both ']' and '}'.
      return JSMSG_RAW_BRACKET_IN_REGEXP;
    case RegExpError::kRangeOutOfOrder:
      return JSMSG_NUMBERS_OUT_OF_ORDER;
    case RegExpError::kIncompleteQuantifier:
      return JSMSG_INCOMPLETE_QUANTIFIER;
    case RegExpError::kInvalidQuantifier:
      return JSMSG_INVALID_QUANTIFIER;
    case RegExpError::kInvalidGroup:
      return JSMSG_INVALID_GROUP;
    case RegExpError::kMultipleFlagDashes:
    case RegExpError::kRepeatedFlag:
    case RegExpError::kInvalidFlagGroup:
      // V8 contains experimental support for turning regexp flags on
      // and off in the middle of a regular expression. Unless it
      // becomes standardized, SM does not support this feature.
      MOZ_CRASH("Mode modifiers not supported");
    case RegExpError::kTooManyCaptures:
      return JSMSG_TOO_MANY_PARENS;
    case RegExpError::kInvalidCaptureGroupName:
      return JSMSG_INVALID_CAPTURE_NAME;
    case RegExpError::kDuplicateCaptureGroupName:
      return JSMSG_DUPLICATE_CAPTURE_NAME;
    case RegExpError::kInvalidNamedReference:
      return JSMSG_INVALID_NAMED_REF;
    case RegExpError::kInvalidNamedCaptureReference:
      return JSMSG_INVALID_NAMED_CAPTURE_REF;
    case RegExpError::kInvalidClassEscape:
      return JSMSG_RANGE_WITH_CLASS_ESCAPE;
    case RegExpError::kInvalidClassPropertyName:
      return JSMSG_INVALID_CLASS_PROPERTY_NAME;
    case RegExpError::kInvalidCharacterClass:
      return JSMSG_RANGE_WITH_CLASS_ESCAPE;
    case RegExpError::kUnterminatedCharacterClass:
      return JSMSG_UNTERM_CLASS;
    case RegExpError::kOutOfOrderCharacterClass:
      return JSMSG_BAD_CLASS_RANGE;
    case RegExpError::NumErrors:
      MOZ_CRASH("Unreachable");
  }
  MOZ_CRASH("Unreachable");
}

Isolate* CreateIsolate(JSContext* cx) {
  auto isolate = MakeUnique<Isolate>(cx);
  if (!isolate || !isolate->init()) {
    return nullptr;
  }
  return isolate.release();
}

static size_t ComputeColumn(const Latin1Char* begin, const Latin1Char* end) {
  return PointerRangeSize(begin, end);
}

static size_t ComputeColumn(const char16_t* begin, const char16_t* end) {
  return unicode::CountCodePoints(begin, end);
}

// This function is varargs purely so it can call ReportCompileErrorLatin1.
// We never call it with additional arguments.
template <typename CharT>
static void ReportSyntaxError(TokenStreamAnyChars& ts,
                              RegExpCompileData& result, CharT* start,
                              size_t length,
                              ...) {
  gc::AutoSuppressGC suppressGC(ts.context());
  uint32_t errorNumber = ErrorNumber(result.error);

  uint32_t offset = std::max(result.error_pos, 0);
  MOZ_ASSERT(offset <= length);

  ErrorMetadata err;

  // Ordinarily this indicates whether line-of-context information can be
  // added, but we entirely ignore that here because we create a
  // a line of context based on the expression source.
  uint32_t location = ts.currentToken().pos.begin;
  if (ts.fillExceptingContext(&err, location)) {
    // Line breaks are not significant in pattern text in the same way as
    // in source text, so act as though pattern text is a single line, then
    // compute a column based on "code point" count (treating a lone
    // surrogate as a "code point" in UTF-16).  Gak.
    err.lineNumber = 1;
    err.columnNumber =
        AssertedCast<uint32_t>(ComputeColumn(start, start + offset));
  }

  // For most error reporting, the line of context derives from the token
  // stream.  So when location information doesn't come from the token
  // stream, we can't give a line of context.  But here the "line of context"
  // can be (and is) derived from the pattern text, so we can provide it no
  // matter if the location is derived from the caller.

  const CharT* windowStart =
      (offset > ErrorMetadata::lineOfContextRadius)
          ? start + (offset - ErrorMetadata::lineOfContextRadius)
          : start;

  const CharT* windowEnd =
      (length - offset > ErrorMetadata::lineOfContextRadius)
          ? start + offset + ErrorMetadata::lineOfContextRadius
          : start + length;

  size_t windowLength = PointerRangeSize(windowStart, windowEnd);
  MOZ_ASSERT(windowLength <= ErrorMetadata::lineOfContextRadius * 2);

  // Create the windowed string, not including the potential line
  // terminator.
  StringBuffer windowBuf(ts.context());
  if (!windowBuf.append(windowStart, windowEnd)) return;

  // The line of context must be null-terminated, and StringBuffer doesn't
  // make that happen unless we force it to.
  if (!windowBuf.append('\0')) return;

  err.lineOfContext.reset(windowBuf.stealChars());
  if (!err.lineOfContext) return;

  err.lineLength = windowLength;
  err.tokenOffset = offset - (windowStart - start);

  va_list args;
  va_start(args, length);
  ReportCompileErrorLatin1(ts.context(), std::move(err), nullptr, errorNumber,
                           &args);
  va_end(args);
}

static void ReportSyntaxError(TokenStreamAnyChars& ts,
                              RegExpCompileData& result, HandleAtom pattern) {
  JS::AutoCheckCannotGC nogc_;
  if (pattern->hasLatin1Chars()) {
    ReportSyntaxError(ts, result, pattern->latin1Chars(nogc_),
                      pattern->length());
  } else {
    ReportSyntaxError(ts, result, pattern->twoByteChars(nogc_),
                      pattern->length());
  }
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
    ReportSyntaxError(ts, result, chars.begin().get(), chars.length());
    return false;
  }
  return true;
}

bool CheckPatternSyntax(JSContext* cx, TokenStreamAnyChars& ts,
                        HandleAtom pattern, JS::RegExpFlags flags) {
  FlatStringReader reader(pattern);
  RegExpCompileData result;
  if (!CheckPatternSyntaxImpl(cx, &reader, flags, &result)) {
    ReportSyntaxError(ts, result, pattern);
    return false;
  }
  return true;
}

}  // namespace irregexp
}  // namespace js

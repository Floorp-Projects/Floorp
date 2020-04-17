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

#include "jit/JitCommon.h"
#include "new-regexp/regexp-bytecode-generator.h"
#include "new-regexp/regexp-compiler.h"
#include "new-regexp/regexp-interpreter.h"
#include "new-regexp/regexp-macro-assembler-arch.h"
#include "new-regexp/regexp-parser.h"
#include "new-regexp/regexp-shim.h"
#include "new-regexp/regexp.h"
#include "util/StringBuffer.h"
#include "vm/MatchPairs.h"
#include "vm/RegExpShared.h"

namespace js {
namespace irregexp {

using mozilla::AssertedCast;
using mozilla::Maybe;
using mozilla::PointerRangeSize;

using frontend::TokenStream;
using frontend::TokenStreamAnyChars;

using v8::internal::FlatStringReader;
using v8::internal::HandleScope;
using v8::internal::InputOutputData;
using v8::internal::IrregexpInterpreter;
using v8::internal::NativeRegExpMacroAssembler;
using v8::internal::RegExpBytecodeGenerator;
using v8::internal::RegExpCompileData;
using v8::internal::RegExpCompiler;
using v8::internal::RegExpError;
using v8::internal::RegExpMacroAssembler;
using v8::internal::RegExpNode;
using v8::internal::RegExpParser;
using v8::internal::SMRegExpMacroAssembler;
using v8::internal::Zone;

using V8HandleString = v8::internal::Handle<v8::internal::String>;
using V8HandleRegExp = v8::internal::Handle<v8::internal::JSRegExp>;

using namespace v8::internal::regexp_compiler_constants;

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
                              size_t length, ...) {
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

  HandleScope handleScope(cx->isolate);
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

// A regexp is a good candidate for Boyer-Moore if it has at least 3
// times as many characters as it has unique characters. Note that
// table lookups in irregexp are done modulo tableSize (128).
template <typename CharT>
static bool HasFewDifferentCharacters(const CharT* chars, size_t length) {
  const uint32_t tableSize =
      v8::internal::NativeRegExpMacroAssembler::kTableSize;
  bool character_found[tableSize];
  uint32_t different = 0;
  memset(&character_found[0], 0, sizeof(character_found));
  for (uint32_t i = 0; i < length; i++) {
    uint32_t ch = chars[i] % tableSize;
    if (!character_found[ch]) {
      character_found[ch] = true;
      different++;
      // We declare a regexp low-alphabet if it has at least 3 times as many
      // characters as it has different characters.
      if (different * 3 > length) {
        return false;
      }
    }
  }
  return true;
}

// Identifies the sort of pattern where Boyer-Moore is faster than string search
static bool UseBoyerMoore(HandleAtom pattern, JS::AutoAssertNoGC& nogc) {
  size_t length =
      std::min(size_t(kMaxLookaheadForBoyerMoore), pattern->length());
  if (length <= kPatternTooShortForBoyerMoore) {
    return false;
  }

  if (pattern->hasLatin1Chars()) {
    return HasFewDifferentCharacters(pattern->latin1Chars(nogc), length);
  }
  MOZ_ASSERT(pattern->hasTwoByteChars());
  return HasFewDifferentCharacters(pattern->twoByteChars(nogc), length);
}

// Sample character frequency information for use in Boyer-Moore.
static void SampleCharacters(HandleLinearString input,
                             RegExpCompiler& compiler) {
  static const int kSampleSize = 128;
  int chars_sampled = 0;

  FlatStringReader sample_subject(input);
  int length = sample_subject.length();

  int half_way = (length - kSampleSize) / 2;
  for (int i = std::max(0, half_way); i < length && chars_sampled < kSampleSize;
       i++, chars_sampled++) {
    compiler.frequency_collator()->CountCharacter(sample_subject.Get(i));
  }
}

static RegExpNode* WrapBody(MutableHandleRegExpShared re,
                            RegExpCompiler& compiler, RegExpCompileData& data,
                            Zone* zone, bool isLatin1) {
  using v8::internal::ChoiceNode;
  using v8::internal::EndNode;
  using v8::internal::GuardedAlternative;
  using v8::internal::RegExpCapture;
  using v8::internal::RegExpCharacterClass;
  using v8::internal::RegExpQuantifier;
  using v8::internal::RegExpTree;
  using v8::internal::TextNode;

  RegExpNode* captured_body =
      RegExpCapture::ToNode(data.tree, 0, &compiler, compiler.accept());
  RegExpNode* node = captured_body;

  if (!data.tree->IsAnchoredAtStart() && !re->sticky()) {
    // Add a .*? at the beginning, outside the body capture, unless
    // this expression is anchored at the beginning or sticky.
    JS::RegExpFlags default_flags;
    RegExpNode* loop_node = RegExpQuantifier::ToNode(
        0, RegExpTree::kInfinity, false,
        new (zone) RegExpCharacterClass('*', default_flags), &compiler,
        captured_body, data.contains_anchor);

    if (data.contains_anchor) {
      // Unroll loop once, to take care of the case that might start
      // at the start of input.
      ChoiceNode* first_step_node = new (zone) ChoiceNode(2, zone);
      first_step_node->AddAlternative(GuardedAlternative(captured_body));
      first_step_node->AddAlternative(GuardedAlternative(new (zone) TextNode(
          new (zone) RegExpCharacterClass('*', default_flags), false,
          loop_node)));
      node = first_step_node;
    } else {
      node = loop_node;
    }
  }
  if (isLatin1) {
    node = node->FilterOneByte(RegExpCompiler::kMaxRecursion);
    // Do it again to propagate the new nodes to places where they were not
    // put because they had not been calculated yet.
    if (node != nullptr) {
      node = node->FilterOneByte(RegExpCompiler::kMaxRecursion);
    }
  } else if (re->unicode() && (re->global() || re->sticky())) {
    node = RegExpCompiler::OptionallyStepBackToLeadSurrogate(&compiler, node,
                                                             re->getFlags());
  }
  if (node == nullptr) node = new (zone) EndNode(EndNode::BACKTRACK, zone);
  return node;
}

bool CompilePattern(JSContext* cx, MutableHandleRegExpShared re,
                    HandleLinearString input) {
  RootedAtom pattern(cx, re->getSource());
  JS::RegExpFlags flags = re->getFlags();
  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  Zone zone(allocScope.alloc());

  RegExpCompileData data;
  {
    FlatStringReader patternBytes(pattern);
    if (!RegExpParser::ParseRegExp(cx->isolate, &zone, &patternBytes, flags,
                                   &data)) {
      MOZ_ASSERT(data.error == RegExpError::kStackOverflow);
      JS::CompileOptions options(cx);
      TokenStream dummyTokenStream(cx, options, nullptr, 0, nullptr);
      ReportSyntaxError(dummyTokenStream, data, pattern);
      return false;
    }
  }

  if (re->kind() == RegExpShared::Kind::Unparsed) {
    // This is the first time we have compiled this regexp.
    // First, check to see if we should use simple string search
    // with an atom.
    if (!flags.ignoreCase() && !flags.sticky()) {
      RootedAtom searchAtom(cx);
      if (data.simple) {
        // The parse-tree is a single atom that is equal to the pattern.
        searchAtom = re->getSource();
      } else if (data.tree->IsAtom() && data.capture_count == 0) {
        // The parse-tree is a single atom that is not equal to the pattern.
        v8::internal::RegExpAtom* atom = data.tree->AsAtom();
        const char16_t* twoByteChars = atom->data().begin();
        searchAtom = AtomizeChars(cx, twoByteChars, atom->length());
        if (!searchAtom) {
          return false;
        }
      }
      JS::AutoAssertNoGC nogc(cx);
      if (searchAtom && !UseBoyerMoore(searchAtom, nogc)) {
        re->useAtomMatch(searchAtom);
        return true;
      }
    }
    // Add one to account for the whole-match capture
    re->useRegExpMatch(data.capture_count + 1);
  }

  MOZ_ASSERT(re->kind() == RegExpShared::Kind::RegExp);

  HandleScope handleScope(cx->isolate);
  RegExpCompiler compiler(cx->isolate, &zone, data.capture_count,
                          input->hasLatin1Chars());

  bool isLatin1 = input->hasLatin1Chars();

  SampleCharacters(input, compiler);
  data.node = WrapBody(re, compiler, data, &zone, isLatin1);
  data.error = AnalyzeRegExp(cx->isolate, isLatin1, data.node);
  if (data.error != RegExpError::kNone) {
    MOZ_ASSERT(data.error == RegExpError::kAnalysisStackOverflow);
    JS_ReportErrorASCII(cx, "Stack overflow");
    return false;
  }

  bool useNativeCode = re->markedForTierUp();

  MOZ_ASSERT_IF(useNativeCode, IsNativeRegExpEnabled());

  Maybe<jit::JitContext> jctx;
  Maybe<js::jit::StackMacroAssembler> stack_masm;
  UniquePtr<RegExpMacroAssembler> masm;
  if (useNativeCode) {
    NativeRegExpMacroAssembler::Mode mode =
        isLatin1 ? NativeRegExpMacroAssembler::LATIN1
                 : NativeRegExpMacroAssembler::UC16;
    // If we are compiling native code, we need a macroassembler,
    // which needs a jit context.
    jctx.emplace(cx, nullptr);
    stack_masm.emplace();
    uint32_t num_capture_registers = re->pairCount() * 2;
    masm = MakeUnique<SMRegExpMacroAssembler>(cx, stack_masm.ref(), &zone, mode,
                                              num_capture_registers);
  } else {
    masm = MakeUnique<RegExpBytecodeGenerator>(cx->isolate, &zone);
  }
  if (!masm) {
    ReportOutOfMemory(cx);
    return false;
  }

  bool largePattern =
      pattern->length() > v8::internal::RegExp::kRegExpTooLargeToOptimize;
  masm->set_slow_safe(largePattern);
  if (compiler.optimize()) {
    compiler.set_optimize(!largePattern);
  }

  // When matching a regexp with known maximum length that is anchored
  // at the end, we may be able to skip the beginning of long input
  // strings. This decision is made here because it depends on
  // information in the AST that isn't replicated in the Node
  // structure used inside the compiler.
  bool is_start_anchored = data.tree->IsAnchoredAtStart();
  bool is_end_anchored = data.tree->IsAnchoredAtEnd();
  int max_length = data.tree->max_match();
  static const int kMaxBacksearchLimit = 1024;
  if (is_end_anchored && !is_start_anchored && !re->sticky() &&
      max_length < kMaxBacksearchLimit) {
    masm->SetCurrentPositionFromEnd(max_length);
  }

  if (re->global()) {
    RegExpMacroAssembler::GlobalMode mode = RegExpMacroAssembler::GLOBAL;
    if (data.tree->min_match() > 0) {
      mode = RegExpMacroAssembler::GLOBAL_NO_ZERO_LENGTH_CHECK;
    } else if (re->unicode()) {
      mode = RegExpMacroAssembler::GLOBAL_UNICODE;
    }
    masm->set_global_mode(mode);
  }

  // Compile the regexp
  V8HandleString wrappedPattern(v8::internal::String(pattern), cx->isolate);
  RegExpCompiler::CompilationResult result = compiler.Assemble(
      cx->isolate, masm.get(), data.node, data.capture_count, wrappedPattern);
  if (JS::Value(result.code).isUndefined()) {
    // SMRegExpMacroAssembler::GetCode returns undefined on OOM.
    MOZ_ASSERT(useNativeCode);
    ReportOutOfMemory(cx);
    return false;
  }
  if (!result.Succeeded()) {
    MOZ_ASSERT(result.error == RegExpError::kTooLarge);
    JS_ReportErrorASCII(cx, "regexp too big");
    return false;
  }

  re->updateMaxRegisters(result.num_registers);
  if (useNativeCode) {
    // Transfer ownership of the tables from the macroassembler to the
    // RegExpShared.
    SMRegExpMacroAssembler::TableVector& tables =
        static_cast<SMRegExpMacroAssembler*>(masm.get())->tables();
    for (uint32_t i = 0; i < tables.length(); i++) {
      if (!re->addTable(std::move(tables[i]))) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
    re->setJitCode(v8::internal::Code::cast(result.code).inner(), isLatin1);
  } else {
    // Transfer ownership of the bytecode from the HandleScope to the
    // RegExpShared.
    ByteArray bytecode =
        v8::internal::ByteArray::cast(result.code).takeOwnership(cx->isolate);
    uint32_t length = bytecode->length;
    re->setByteCode(bytecode.release(), isLatin1);
    js::AddCellMemory(re, length, MemoryUse::RegExpSharedBytecode);
  }

  return true;
}

template <typename CharT>
RegExpRunStatus ExecuteRaw(jit::JitCode* code, const CharT* chars,
                           size_t length, size_t startIndex,
                           VectorMatchPairs* matches) {
  InputOutputData data(chars, chars + length, startIndex, matches);

  static_assert(RegExpRunStatus_Error ==
                v8::internal::RegExp::kInternalRegExpException);
  static_assert(RegExpRunStatus_Success ==
                v8::internal::RegExp::kInternalRegExpSuccess);
  static_assert(RegExpRunStatus_Success_NotFound ==
                v8::internal::RegExp::kInternalRegExpFailure);

  typedef int (*RegExpCodeSignature)(InputOutputData*);
  auto function = reinterpret_cast<RegExpCodeSignature>(code->raw());
  {
    JS::AutoSuppressGCAnalysis nogc;
    return (RegExpRunStatus) CALL_GENERATED_1(function, &data);
  }
}

RegExpRunStatus Interpret(JSContext* cx, MutableHandleRegExpShared re,
                          HandleLinearString input, size_t startIndex,
                          VectorMatchPairs* matches) {
  HandleScope handleScope(cx->isolate);
  V8HandleRegExp wrappedRegExp(v8::internal::JSRegExp(re), cx->isolate);
  V8HandleString wrappedInput(v8::internal::String(input), cx->isolate);

  uint32_t numRegisters = re->getMaxRegisters();

  // Allocate memory for registers. They will be initialized by the
  // interpreter. (See IrregexpInterpreter::MatchInternal.)
  Vector<int32_t, 8, SystemAllocPolicy> registers;
  if (!registers.growByUninitialized(numRegisters)) {
    ReportOutOfMemory(cx);
    return RegExpRunStatus_Error;
  }

  static_assert(RegExpRunStatus_Error ==
                v8::internal::RegExp::kInternalRegExpException);
  static_assert(RegExpRunStatus_Success ==
                v8::internal::RegExp::kInternalRegExpSuccess);
  static_assert(RegExpRunStatus_Success_NotFound ==
                v8::internal::RegExp::kInternalRegExpFailure);

  RegExpRunStatus status =
      (RegExpRunStatus)IrregexpInterpreter::MatchForCallFromRuntime(
          cx->isolate, wrappedRegExp, wrappedInput, registers.begin(),
          numRegisters, startIndex);

  MOZ_ASSERT(status == RegExpRunStatus_Error ||
             status == RegExpRunStatus_Success ||
             status == RegExpRunStatus_Success_NotFound);

  // Copy results out of registers
  if (status == RegExpRunStatus_Success) {
    uint32_t length = re->pairCount() * 2;
    MOZ_ASSERT(length <= registers.length());
    for (uint32_t i = 0; i < length; i++) {
      matches->pairsRaw()[i] = registers[i];
    }
  }

  return status;
}

RegExpRunStatus Execute(JSContext* cx, MutableHandleRegExpShared re,
                        HandleLinearString input, size_t startIndex,
                        VectorMatchPairs* matches) {
  bool latin1 = input->hasLatin1Chars();
  jit::JitCode* jitCode = re->getJitCode(latin1);
  bool isCompiled = !!jitCode;

  if (isCompiled) {
    JS::AutoCheckCannotGC nogc;
    if (latin1) {
      return ExecuteRaw(jitCode, input->latin1Chars(nogc), input->length(),
                        startIndex, matches);
    }
    return ExecuteRaw(jitCode, input->twoByteChars(nogc), input->length(),
                      startIndex, matches);
  }

  return Interpret(cx, re, input, startIndex, matches);
}

}  // namespace irregexp
}  // namespace js

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"  // MOZ_RELEASE_ASSERT
#include "mozilla/RefPtr.h"      // RefPtr
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include <algorithm>  // std::all_of, std::equal, std::move, std::transform
#include <iterator>   // std::size
#include <memory>     // std::uninitialized_fill_n
#include <stddef.h>   // size_t
#include <stdint.h>   // uint32_t

#include "jsapi.h"  // JS_EnsureLinearString, JS_GC, JS_Get{Latin1,TwoByte}LinearStringChars, JS_GetStringLength, JS_ValueToFunction
#include "jstypes.h"  // JS_PUBLIC_API

#include "gc/GC.h"                        // js::gc::FinishGC
#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/CompileOptions.h"  // JS::CompileOptions, JS::InstantiateOptions
#include "js/Conversions.h"     // JS::ToString
#include "js/experimental/JSStencil.h"  // JS::Stencil, JS::InstantiateGlobalStencil
#include "js/MemoryFunctions.h"         // JS_malloc
#include "js/RootingAPI.h"              // JS::MutableHandle, JS::Rooted
#include "js/SourceText.h"              // JS::SourceOwnership, JS::SourceText
#include "js/String.h"  // JS::GetLatin1LinearStringChars, JS::GetTwoByteLinearStringChars, JS::StringHasLatin1Chars
#include "js/UniquePtr.h"  // js::UniquePtr
#include "js/Utility.h"    // JS::FreePolicy
#include "js/Value.h"      // JS::NullValue, JS::ObjectValue, JS::Value
#include "jsapi-tests/tests.h"
#include "util/Text.h"         // js_strlen
#include "vm/Compression.h"    // js::Compressor::CHUNK_SIZE
#include "vm/HelperThreads.h"  // js::RunPendingSourceCompressions
#include "vm/JSFunction.h"     // JSFunction::getOrCreateScript
#include "vm/JSScript.h"  // JSScript, js::ScriptSource::MinimumCompressibleLength, js::SynchronouslyCompressSource
#include "vm/Monitor.h"   // js::Monitor, js::AutoLockMonitor

using mozilla::Utf8Unit;

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSString;

template <typename Unit>
using Source = js::UniquePtr<Unit[], JS::FreePolicy>;

constexpr size_t ChunkSize = js::Compressor::CHUNK_SIZE;
constexpr size_t MinimumCompressibleLength =
    js::ScriptSource::MinimumCompressibleLength;

// Don't use ' ' to spread stuff across lines.
constexpr char FillerWhitespace = '\n';

template <typename Unit>
static Source<Unit> MakeSourceAllWhitespace(JSContext* cx, size_t len) {
  static_assert(ChunkSize % sizeof(Unit) == 0,
                "chunk size presumed to be a multiple of char size");

  Source<Unit> source(
      reinterpret_cast<Unit*>(JS_malloc(cx, len * sizeof(Unit))));
  if (source) {
    std::uninitialized_fill_n(source.get(), len, FillerWhitespace);
  }
  return source;
}

template <typename Unit>
static JSFunction* EvaluateChars(JSContext* cx, Source<Unit> chars, size_t len,
                                 char functionName, const char* func) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(func, 1);

  // Evaluate the provided source text, containing a function named
  // |functionName|.
  JS::SourceText<Unit> sourceText;
  if (!sourceText.init(cx, std::move(chars), len)) {
    return nullptr;
  }

  {
    JS::Rooted<JS::Value> dummy(cx);
    if (!JS::Evaluate(cx, options, sourceText, &dummy)) {
      return nullptr;
    }
  }

  // Evaluate the name of that function.
  JS::Rooted<JS::Value> rval(cx);
  const char16_t name[] = {char16_t(functionName)};
  JS::SourceText<char16_t> srcbuf;
  if (!srcbuf.init(cx, name, std::size(name), JS::SourceOwnership::Borrowed)) {
    return nullptr;
  }
  if (!JS::Evaluate(cx, options, srcbuf, &rval)) {
    return nullptr;
  }

  // Return the function.
  MOZ_RELEASE_ASSERT(rval.isObject());
  return JS_ValueToFunction(cx, rval);
}

static void CompressSourceSync(JS::Handle<JSFunction*> fun, JSContext* cx) {
  JS::Rooted<JSScript*> script(cx, JSFunction::getOrCreateScript(cx, fun));
  MOZ_RELEASE_ASSERT(script);
  MOZ_RELEASE_ASSERT(script->scriptSource()->hasSourceText());

  MOZ_RELEASE_ASSERT(js::SynchronouslyCompressSource(cx, script));

  MOZ_RELEASE_ASSERT(script->scriptSource()->hasCompressedSource());
}

static constexpr char FunctionStart[] = "function @() {";
constexpr size_t FunctionStartLength = js_strlen(FunctionStart);
constexpr size_t FunctionNameOffset = 9;

static_assert(FunctionStart[FunctionNameOffset] == '@',
              "offset must correctly point at the function name location");

static constexpr char FunctionEnd[] = "return 42; }";
constexpr size_t FunctionEndLength = js_strlen(FunctionEnd);

template <typename Unit>
static void WriteFunctionOfSizeAtOffset(Source<Unit>& source,
                                        size_t usableSourceLen,
                                        char functionName,
                                        size_t functionLength, size_t offset) {
  MOZ_RELEASE_ASSERT(functionLength >= MinimumCompressibleLength,
                     "function must be a certain size to be compressed");
  MOZ_RELEASE_ASSERT(offset <= usableSourceLen,
                     "offset must not exceed usable source");
  MOZ_RELEASE_ASSERT(functionLength <= usableSourceLen,
                     "function must fit in usable source");
  MOZ_RELEASE_ASSERT(offset <= usableSourceLen - functionLength,
                     "function must not extend past usable source");

  // Assigning |char| to |char16_t| is permitted, but we deliberately require a
  // cast to assign |char| to |Utf8Unit|.  |std::copy_n| would handle the first
  // case, but the required transformation for UTF-8 demands |std::transform|.
  auto TransformToUnit = [](char c) { return Unit(c); };

  // Fill in the function start.
  std::transform(FunctionStart, FunctionStart + FunctionStartLength,
                 &source[offset], TransformToUnit);
  source[offset + FunctionNameOffset] = Unit(functionName);

  // Fill in the function end.
  std::transform(FunctionEnd, FunctionEnd + FunctionEndLength,
                 &source[offset + functionLength - FunctionEndLength],
                 TransformToUnit);
}

static JSString* DecompressSource(JSContext* cx, JS::Handle<JSFunction*> fun) {
  JS::Rooted<JS::Value> fval(cx, JS::ObjectValue(*JS_GetFunctionObject(fun)));
  return JS::ToString(cx, fval);
}

static bool IsExpectedFunctionString(JS::Handle<JSString*> str,
                                     char functionName, JSContext* cx) {
  JSLinearString* lstr = JS_EnsureLinearString(cx, str);
  MOZ_RELEASE_ASSERT(lstr);

  size_t len = JS_GetStringLength(str);
  if (len < FunctionStartLength || len < FunctionEndLength) {
    return false;
  }

  JS::AutoAssertNoGC nogc(cx);

  auto CheckContents = [functionName, len](const auto* chars) {
    // Check the function in parts:
    //
    //   * "function "
    //   * "A"
    //   * "() {"
    //   * "\n...\n"
    //   * "return 42; }"
    return std::equal(chars, chars + FunctionNameOffset, FunctionStart) &&
           chars[FunctionNameOffset] == functionName &&
           std::equal(chars + FunctionNameOffset + 1,
                      chars + FunctionStartLength,
                      FunctionStart + FunctionNameOffset + 1) &&
           std::all_of(chars + FunctionStartLength,
                       chars + len - FunctionEndLength,
                       [](auto c) { return c == FillerWhitespace; }) &&
           std::equal(chars + len - FunctionEndLength, chars + len,
                      FunctionEnd);
  };

  bool hasExpectedContents;
  if (JS::StringHasLatin1Chars(str)) {
    const JS::Latin1Char* chars = JS::GetLatin1LinearStringChars(nogc, lstr);
    hasExpectedContents = CheckContents(chars);
  } else {
    const char16_t* chars = JS::GetTwoByteLinearStringChars(nogc, lstr);
    hasExpectedContents = CheckContents(chars);
  }

  return hasExpectedContents;
}

BEGIN_TEST(testScriptSourceCompression_inOneChunk) {
  CHECK(run<char16_t>());
  CHECK(run<Utf8Unit>());
  return true;
}

template <typename Unit>
bool run() {
  constexpr size_t len = MinimumCompressibleLength + 55;
  auto source = MakeSourceAllWhitespace<Unit>(cx, len);
  CHECK(source);

  // Write out a 'b' or 'c' function that is long enough to be compressed,
  // that starts after source start and ends before source end.
  constexpr char FunctionName = 'a' + sizeof(Unit);
  WriteFunctionOfSizeAtOffset(source, len, FunctionName,
                              MinimumCompressibleLength,
                              len - MinimumCompressibleLength);

  JS::Rooted<JSFunction*> fun(cx);
  fun = EvaluateChars(cx, std::move(source), len, FunctionName, __FUNCTION__);
  CHECK(fun);

  CompressSourceSync(fun, cx);

  JS::Rooted<JSString*> str(cx, DecompressSource(cx, fun));
  CHECK(str);
  CHECK(IsExpectedFunctionString(str, FunctionName, cx));

  return true;
}
END_TEST(testScriptSourceCompression_inOneChunk)

BEGIN_TEST(testScriptSourceCompression_endsAtBoundaryInOneChunk) {
  CHECK(run<char16_t>());
  CHECK(run<Utf8Unit>());
  return true;
}

template <typename Unit>
bool run() {
  constexpr size_t len = ChunkSize / sizeof(Unit);
  auto source = MakeSourceAllWhitespace<Unit>(cx, len);
  CHECK(source);

  // Write out a 'd' or 'e' function that is long enough to be compressed,
  // that (for no particular reason) starts after source start and ends
  // before usable source end.
  constexpr char FunctionName = 'c' + sizeof(Unit);
  WriteFunctionOfSizeAtOffset(source, len, FunctionName,
                              MinimumCompressibleLength,
                              len - MinimumCompressibleLength);

  JS::Rooted<JSFunction*> fun(cx);
  fun = EvaluateChars(cx, std::move(source), len, FunctionName, __FUNCTION__);
  CHECK(fun);

  CompressSourceSync(fun, cx);

  JS::Rooted<JSString*> str(cx, DecompressSource(cx, fun));
  CHECK(str);
  CHECK(IsExpectedFunctionString(str, FunctionName, cx));

  return true;
}
END_TEST(testScriptSourceCompression_endsAtBoundaryInOneChunk)

BEGIN_TEST(testScriptSourceCompression_isExactChunk) {
  CHECK(run<char16_t>());
  CHECK(run<Utf8Unit>());
  return true;
}

template <typename Unit>
bool run() {
  constexpr size_t len = ChunkSize / sizeof(Unit);
  auto source = MakeSourceAllWhitespace<Unit>(cx, len);
  CHECK(source);

  // Write out a 'f' or 'g' function that occupies the entire source (and
  // entire chunk, too).
  constexpr char FunctionName = 'e' + sizeof(Unit);
  WriteFunctionOfSizeAtOffset(source, len, FunctionName, len, 0);

  JS::Rooted<JSFunction*> fun(cx);
  fun = EvaluateChars(cx, std::move(source), len, FunctionName, __FUNCTION__);
  CHECK(fun);

  CompressSourceSync(fun, cx);

  JS::Rooted<JSString*> str(cx, DecompressSource(cx, fun));
  CHECK(str);
  CHECK(IsExpectedFunctionString(str, FunctionName, cx));

  return true;
}
END_TEST(testScriptSourceCompression_isExactChunk)

BEGIN_TEST(testScriptSourceCompression_crossesChunkBoundary) {
  CHECK(run<char16_t>());
  CHECK(run<Utf8Unit>());
  return true;
}

template <typename Unit>
bool run() {
  constexpr size_t len = ChunkSize / sizeof(Unit) + 293;
  auto source = MakeSourceAllWhitespace<Unit>(cx, len);
  CHECK(source);

  // This function crosses a chunk boundary but does not end at one.
  constexpr size_t FunctionSize = 177 + ChunkSize / sizeof(Unit);

  // Write out a 'h' or 'i' function.
  constexpr char FunctionName = 'g' + sizeof(Unit);
  WriteFunctionOfSizeAtOffset(source, len, FunctionName, FunctionSize, 37);

  JS::Rooted<JSFunction*> fun(cx);
  fun = EvaluateChars(cx, std::move(source), len, FunctionName, __FUNCTION__);
  CHECK(fun);

  CompressSourceSync(fun, cx);

  JS::Rooted<JSString*> str(cx, DecompressSource(cx, fun));
  CHECK(str);
  CHECK(IsExpectedFunctionString(str, FunctionName, cx));

  return true;
}
END_TEST(testScriptSourceCompression_crossesChunkBoundary)

BEGIN_TEST(testScriptSourceCompression_crossesChunkBoundary_endsAtBoundary) {
  CHECK(run<char16_t>());
  CHECK(run<Utf8Unit>());
  return true;
}

template <typename Unit>
bool run() {
  // Exactly two chunks.
  constexpr size_t len = (2 * ChunkSize) / sizeof(Unit);
  auto source = MakeSourceAllWhitespace<Unit>(cx, len);
  CHECK(source);

  // This function crosses a chunk boundary, and it ends exactly at the end
  // of both the second chunk and the full source.
  constexpr size_t FunctionSize = 1 + ChunkSize / sizeof(Unit);

  // Write out a 'j' or 'k' function.
  constexpr char FunctionName = 'i' + sizeof(Unit);
  WriteFunctionOfSizeAtOffset(source, len, FunctionName, FunctionSize,
                              len - FunctionSize);

  JS::Rooted<JSFunction*> fun(cx);
  fun = EvaluateChars(cx, std::move(source), len, FunctionName, __FUNCTION__);
  CHECK(fun);

  CompressSourceSync(fun, cx);

  JS::Rooted<JSString*> str(cx, DecompressSource(cx, fun));
  CHECK(str);
  CHECK(IsExpectedFunctionString(str, FunctionName, cx));

  return true;
}
END_TEST(testScriptSourceCompression_crossesChunkBoundary_endsAtBoundary)

BEGIN_TEST(testScriptSourceCompression_containsWholeChunk) {
  CHECK(run<char16_t>());
  CHECK(run<Utf8Unit>());
  return true;
}

template <typename Unit>
bool run() {
  constexpr size_t len = (2 * ChunkSize) / sizeof(Unit) + 17;
  auto source = MakeSourceAllWhitespace<Unit>(cx, len);
  CHECK(source);

  // This function crosses two chunk boundaries and begins/ends in the middle
  // of chunk boundaries.
  constexpr size_t FunctionSize = 2 + ChunkSize / sizeof(Unit);

  // Write out a 'l' or 'm' function.
  constexpr char FunctionName = 'k' + sizeof(Unit);
  WriteFunctionOfSizeAtOffset(source, len, FunctionName, FunctionSize,
                              ChunkSize / sizeof(Unit) - 1);

  JS::Rooted<JSFunction*> fun(cx);
  fun = EvaluateChars(cx, std::move(source), len, FunctionName, __FUNCTION__);
  CHECK(fun);

  CompressSourceSync(fun, cx);

  JS::Rooted<JSString*> str(cx, DecompressSource(cx, fun));
  CHECK(str);
  CHECK(IsExpectedFunctionString(str, FunctionName, cx));

  return true;
}
END_TEST(testScriptSourceCompression_containsWholeChunk)

BEGIN_TEST(testScriptSourceCompression_containsWholeChunk_endsAtBoundary) {
  CHECK(run<char16_t>());
  CHECK(run<Utf8Unit>());
  return true;
}

template <typename Unit>
bool run() {
  // Exactly three chunks.
  constexpr size_t len = (3 * ChunkSize) / sizeof(Unit);
  auto source = MakeSourceAllWhitespace<Unit>(cx, len);
  CHECK(source);

  // This function crosses two chunk boundaries and ends at a chunk boundary.
  constexpr size_t FunctionSize = 1 + (2 * ChunkSize) / sizeof(Unit);

  // Write out a 'n' or 'o' function.
  constexpr char FunctionName = 'm' + sizeof(Unit);
  WriteFunctionOfSizeAtOffset(source, len, FunctionName, FunctionSize,
                              ChunkSize / sizeof(Unit) - 1);

  JS::Rooted<JSFunction*> fun(cx);
  fun = EvaluateChars(cx, std::move(source), len, FunctionName, __FUNCTION__);
  CHECK(fun);

  CompressSourceSync(fun, cx);

  JS::Rooted<JSString*> str(cx, DecompressSource(cx, fun));
  CHECK(str);
  CHECK(IsExpectedFunctionString(str, FunctionName, cx));

  return true;
}
END_TEST(testScriptSourceCompression_containsWholeChunk_endsAtBoundary)

BEGIN_TEST(testScriptSourceCompression_spansMultipleMiddleChunks) {
  CHECK(run<char16_t>());
  CHECK(run<Utf8Unit>());
  return true;
}

template <typename Unit>
bool run() {
  // Four chunks.
  constexpr size_t len = (4 * ChunkSize) / sizeof(Unit);
  auto source = MakeSourceAllWhitespace<Unit>(cx, len);
  CHECK(source);

  // This function spans the two middle chunks and further extends one
  // character to each side.
  constexpr size_t FunctionSize = 2 + (2 * ChunkSize) / sizeof(Unit);

  // Write out a 'p' or 'q' function.
  constexpr char FunctionName = 'o' + sizeof(Unit);
  WriteFunctionOfSizeAtOffset(source, len, FunctionName, FunctionSize,
                              ChunkSize / sizeof(Unit) - 1);

  JS::Rooted<JSFunction*> fun(cx);
  fun = EvaluateChars(cx, std::move(source), len, FunctionName, __FUNCTION__);
  CHECK(fun);

  CompressSourceSync(fun, cx);

  JS::Rooted<JSString*> str(cx, DecompressSource(cx, fun));
  CHECK(str);
  CHECK(IsExpectedFunctionString(str, FunctionName, cx));

  return true;
}
END_TEST(testScriptSourceCompression_spansMultipleMiddleChunks)

BEGIN_TEST(testScriptSourceCompression_automatic) {
  constexpr size_t len = MinimumCompressibleLength + 55;
  auto chars = MakeSourceAllWhitespace<char16_t>(cx, len);
  CHECK(chars);

  JS::SourceText<char16_t> source;
  CHECK(source.init(cx, std::move(chars), len));

  JS::CompileOptions options(cx);
  JS::Rooted<JSScript*> script(cx, JS::Compile(cx, options, source));
  CHECK(script);

  // Check that source compression was triggered by the compile. If the
  // off-thread source compression system is globally disabled, the source will
  // remain uncompressed.
  js::RunPendingSourceCompressions(cx->runtime());
  bool expected = js::IsOffThreadSourceCompressionEnabled();
  CHECK(script->scriptSource()->hasCompressedSource() == expected);

  return true;
}
END_TEST(testScriptSourceCompression_automatic)

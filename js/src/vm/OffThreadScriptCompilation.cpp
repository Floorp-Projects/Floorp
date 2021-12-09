/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/OffThreadScriptCompilation.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Range.h"       // mozilla::Range
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit
#include "mozilla/Vector.h"      // mozilla::Vector

#include <stddef.h>  // size_t

#include "jspubtd.h"  // js::CurrentThreadCanAccessRuntime
#include "jstypes.h"  // JS_PUBLIC_API

#include "js/CompileOptions.h"  // JS::ReadOnlyCompileOptions
#include "js/experimental/JSStencil.h"  // JS::CompileToStencilOffThread, JS::FinishOffThreadCompileToStencil
#include "js/SourceText.h"         // JS::SourceText
#include "vm/HelperThreadState.h"  // js::StartOffThreadParseScript
#include "vm/JSContext.h"          // JSContext
#include "vm/Runtime.h"            // js::CanUseExtraThreads

using namespace js;

using mozilla::Utf8Unit;

using JS::ReadOnlyCompileOptions;

enum class OffThread { Compile, Decode };

static bool CanDoOffThread(JSContext* cx, const ReadOnlyCompileOptions& options,
                           size_t length) {
  static const size_t TINY_LENGTH = 5 * 1000;

  // These are heuristics which the caller may choose to ignore (e.g., for
  // testing purposes).
  if (!options.forceAsync) {
    // Compiling off the main thread inolves significant overheads.
    // Don't bother if the script is tiny.
    if (length < TINY_LENGTH) {
      return false;
    }
  }

  return cx->runtime()->canUseParallelParsing() && CanUseExtraThreads();
}

JS_PUBLIC_API bool JS::CanCompileOffThread(
    JSContext* cx, const ReadOnlyCompileOptions& options, size_t length) {
  return CanDoOffThread(cx, options, length);
}

JS_PUBLIC_API JS::OffThreadToken* JS::CompileOffThread(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, OffThreadCompileCallback callback,
    void* callbackData) {
  MOZ_ASSERT(CanCompileOffThread(cx, options, srcBuf.length()));
  return StartOffThreadParseScript(cx, options, srcBuf, callback, callbackData);
}

JS_PUBLIC_API JS::OffThreadToken* JS::CompileOffThread(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Utf8Unit>& srcBuf, OffThreadCompileCallback callback,
    void* callbackData) {
  MOZ_ASSERT(CanCompileOffThread(cx, options, srcBuf.length()));
  return StartOffThreadParseScript(cx, options, srcBuf, callback, callbackData);
}

JS_PUBLIC_API JSScript* JS::FinishOffThreadScript(JSContext* cx,
                                                  JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  return HelperThreadState().finishScriptParseTask(cx, token);
}

JS_PUBLIC_API JS::OffThreadToken* JS::CompileToStencilOffThread(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, OffThreadCompileCallback callback,
    void* callbackData) {
  MOZ_ASSERT(CanCompileOffThread(cx, options, srcBuf.length()));
  return StartOffThreadCompileToStencil(cx, options, srcBuf, callback,
                                        callbackData);
}

JS_PUBLIC_API JS::OffThreadToken* JS::CompileToStencilOffThread(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Utf8Unit>& srcBuf, OffThreadCompileCallback callback,
    void* callbackData) {
  MOZ_ASSERT(CanCompileOffThread(cx, options, srcBuf.length()));
  return StartOffThreadCompileToStencil(cx, options, srcBuf, callback,
                                        callbackData);
}

JS_PUBLIC_API already_AddRefed<JS::Stencil> JS::FinishOffThreadCompileToStencil(
    JSContext* cx, JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  RefPtr<JS::Stencil> stencil =
      HelperThreadState().finishCompileToStencilTask(cx, token);
  return stencil.forget();
}

JS_PUBLIC_API JSScript* JS::FinishOffThreadScriptAndStartIncrementalEncoding(
    JSContext* cx, JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  return HelperThreadState().finishScriptParseTask(cx, token,
                                                   StartEncoding::Yes);
}

JS_PUBLIC_API void JS::CancelOffThreadScript(JSContext* cx,
                                             JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  HelperThreadState().cancelParseTask(cx->runtime(), ParseTaskKind::Script,
                                      token);
}

JS_PUBLIC_API void JS::CancelOffThreadCompileToStencil(
    JSContext* cx, JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  HelperThreadState().cancelParseTask(cx->runtime(),
                                      ParseTaskKind::ScriptStencil, token);
}

JS_PUBLIC_API JS::OffThreadToken* JS::CompileOffThreadModule(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, OffThreadCompileCallback callback,
    void* callbackData) {
  MOZ_ASSERT(CanCompileOffThread(cx, options, srcBuf.length()));
  return StartOffThreadParseModule(cx, options, srcBuf, callback, callbackData);
}

JS_PUBLIC_API JS::OffThreadToken* JS::CompileOffThreadModule(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Utf8Unit>& srcBuf, OffThreadCompileCallback callback,
    void* callbackData) {
  MOZ_ASSERT(CanCompileOffThread(cx, options, srcBuf.length()));
  return StartOffThreadParseModule(cx, options, srcBuf, callback, callbackData);
}

JS_PUBLIC_API JSObject* JS::FinishOffThreadModule(JSContext* cx,
                                                  JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  return HelperThreadState().finishModuleParseTask(cx, token);
}

JS_PUBLIC_API void JS::CancelOffThreadModule(JSContext* cx,
                                             JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  HelperThreadState().cancelParseTask(cx->runtime(), ParseTaskKind::Module,
                                      token);
}

JS_PUBLIC_API bool JS::CanDecodeOffThread(JSContext* cx,
                                          const ReadOnlyCompileOptions& options,
                                          size_t length) {
  return CanDoOffThread(cx, options, length);
}

// TODO: Once off-thread instantiation is removed, use JS::DecodeOptions here
//       and split the instantiation off from the off-thread API.
//       Until then, options here is used for both decode and instantiation.
JS_PUBLIC_API JS::OffThreadToken* JS::DecodeOffThreadScript(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    mozilla::Vector<uint8_t>& buffer /* TranscodeBuffer& */, size_t cursor,
    OffThreadCompileCallback callback, void* callbackData) {
  JS::TranscodeRange range(buffer.begin() + cursor, buffer.length() - cursor);
  MOZ_ASSERT(CanDecodeOffThread(cx, options, range.length()));
  return StartOffThreadDecodeScript(cx, options, range, callback, callbackData);
}

JS_PUBLIC_API JS::OffThreadToken* JS::DecodeOffThreadScript(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    const mozilla::Range<uint8_t>& range /* TranscodeRange& */,
    OffThreadCompileCallback callback, void* callbackData) {
  MOZ_ASSERT(CanDecodeOffThread(cx, options, range.length()));
  return StartOffThreadDecodeScript(cx, options, range, callback, callbackData);
}

JS_PUBLIC_API JSScript* JS::FinishOffThreadScriptDecoder(
    JSContext* cx, JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  return HelperThreadState().finishScriptDecodeTask(cx, token);
}

JS_PUBLIC_API void JS::CancelOffThreadScriptDecoder(JSContext* cx,
                                                    JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  HelperThreadState().cancelParseTask(cx->runtime(),
                                      ParseTaskKind::ScriptDecode, token);
}

JS_PUBLIC_API JS::OffThreadToken* JS::DecodeMultiOffThreadStencils(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    TranscodeSources& sources, OffThreadCompileCallback callback,
    void* callbackData) {
#ifdef DEBUG
  size_t length = 0;
  for (auto& source : sources) {
    length += source.range.length();
  }
  MOZ_ASSERT(CanCompileOffThread(cx, options, length));
#endif
  return StartOffThreadDecodeMultiStencils(cx, options, sources, callback,
                                           callbackData);
}

JS_PUBLIC_API bool JS::FinishMultiOffThreadStencilDecoder(
    JSContext* cx, JS::OffThreadToken* token,
    mozilla::Vector<RefPtr<JS::Stencil>>* stencils) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  return HelperThreadState().finishMultiStencilsDecodeTask(cx, token, stencils);
}

JS_PUBLIC_API void JS::CancelMultiOffThreadScriptsDecoder(
    JSContext* cx, JS::OffThreadToken* token) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  HelperThreadState().cancelParseTask(
      cx->runtime(), ParseTaskKind::MultiStencilsDecode, token);
}

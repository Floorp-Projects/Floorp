/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Structures and functions for transcoding compiled scripts and functions to
 * and from memory.
 */

#ifndef js_Transcoding_h
#define js_Transcoding_h

#include "mozilla/Range.h"   // mozilla::Range
#include "mozilla/Vector.h"  // mozilla::Vector

#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint32_t

#include "js/RootingAPI.h"  // JS::Handle, JS::MutableHandle

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSObject;
class JS_PUBLIC_API JSScript;

namespace JS {

class ReadOnlyCompileOptions;

using TranscodeBuffer = mozilla::Vector<uint8_t>;
using TranscodeRange = mozilla::Range<uint8_t>;

struct TranscodeSource final {
  TranscodeSource(const TranscodeRange& range_, const char* file, uint32_t line)
      : range(range_), filename(file), lineno(line) {}

  const TranscodeRange range;
  const char* filename;
  const uint32_t lineno;
};

using TranscodeSources = mozilla::Vector<TranscodeSource>;

enum TranscodeResult : uint8_t {
  // Successful encoding / decoding.
  TranscodeResult_Ok = 0,

  // A warning message, is set to the message out-param.
  TranscodeResult_Failure = 0x10,
  TranscodeResult_Failure_BadBuildId = TranscodeResult_Failure | 0x1,
  TranscodeResult_Failure_RunOnceNotSupported = TranscodeResult_Failure | 0x2,
  TranscodeResult_Failure_AsmJSNotSupported = TranscodeResult_Failure | 0x3,
  TranscodeResult_Failure_BadDecode = TranscodeResult_Failure | 0x4,
  TranscodeResult_Failure_WrongCompileOption = TranscodeResult_Failure | 0x5,
  TranscodeResult_Failure_NotInterpretedFun = TranscodeResult_Failure | 0x6,

  // There is a pending exception on the context.
  TranscodeResult_Throw = 0x20
};

// Encode JSScript into the buffer.
extern JS_PUBLIC_API TranscodeResult EncodeScript(JSContext* cx,
                                                  TranscodeBuffer& buffer,
                                                  Handle<JSScript*> script);

// Decode JSScript from the buffer.
extern JS_PUBLIC_API TranscodeResult
DecodeScript(JSContext* cx, const ReadOnlyCompileOptions& options,
             TranscodeBuffer& buffer, MutableHandle<JSScript*> scriptp,
             size_t cursorIndex = 0);

extern JS_PUBLIC_API TranscodeResult
DecodeScript(JSContext* cx, const ReadOnlyCompileOptions& options,
             const TranscodeRange& range, MutableHandle<JSScript*> scriptp);

// If js::UseOffThreadParseGlobal is true, decode JSScript from the buffer.
//
// If js::UseOffThreadParseGlobal is false, decode CompilationStencil from the
// buffer and instantiate JSScript from it.
//
// options.useOffThreadParseGlobal should match JS::SetUseOffThreadParseGlobal.
extern JS_PUBLIC_API TranscodeResult DecodeScriptMaybeStencil(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    TranscodeBuffer& buffer, MutableHandle<JSScript*> scriptp,
    size_t cursorIndex = 0);

// If js::UseOffThreadParseGlobal is true, decode JSScript from the buffer.
//
// If js::UseOffThreadParseGlobal is false, decode CompilationStencil from the
// buffer and instantiate JSScript from it.
//
// And then register an encoder on its script source, such that all functions
// can be encoded as they are parsed. This strategy is used to avoid blocking
// the main thread in a non-interruptible way.
//
// See also JS::FinishIncrementalEncoding.
//
// options.useOffThreadParseGlobal should match JS::SetUseOffThreadParseGlobal.
extern JS_PUBLIC_API TranscodeResult DecodeScriptAndStartIncrementalEncoding(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    TranscodeBuffer& buffer, MutableHandle<JSScript*> scriptp,
    size_t cursorIndex = 0);

// Finish incremental encoding started by one of:
//   * JS::CompileAndStartIncrementalEncoding
//   * JS::FinishOffThreadScriptAndStartIncrementalEncoding
//   * JS::DecodeScriptAndStartIncrementalEncoding
//
// The |script| argument of |FinishIncrementalEncoding| should be the top-level
// script returned from one of the above.
//
// The |buffer| argument of |FinishIncrementalEncoding| is used for appending
// the encoded bytecode into the buffer. If any of these functions failed, the
// content of |buffer| would be undefined.
//
// If js::UseOffThreadParseGlobal is true, |buffer| contains encoded JSScript.
//
// If js::UseOffThreadParseGlobal is false, |buffer| contains encoded
// CompilationStencil.
extern JS_PUBLIC_API bool FinishIncrementalEncoding(JSContext* cx,
                                                    Handle<JSScript*> script,
                                                    TranscodeBuffer& buffer);

}  // namespace JS

#endif /* js_Transcoding_h */

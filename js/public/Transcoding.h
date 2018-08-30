/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Structures and functions for transcoding compiled scripts and functions to
 * and from memory.
 */

#ifndef js_Transcoding_h
#define js_Transcoding_h

#include "mozilla/Range.h" // mozilla::Range
#include "mozilla/Vector.h" // mozilla::Vector

#include <stddef.h> // size_t
#include <stdint.h> // uint8_t, uint32_t

#include "js/RootingAPI.h" // JS::Handle, JS::MutableHandle

struct JSContext;
class JSFunction;
class JSObject;
class JSScript;

namespace JS {

using TranscodeBuffer = mozilla::Vector<uint8_t>;
using TranscodeRange = mozilla::Range<uint8_t>;

struct TranscodeSource final
{
    TranscodeSource(const TranscodeRange& range_, const char* file, uint32_t line)
        : range(range_), filename(file), lineno(line)
    {}

    const TranscodeRange range;
    const char* filename;
    const uint32_t lineno;
};

using TranscodeSources = mozilla::Vector<TranscodeSource>;

enum TranscodeResult : uint8_t
{
    // Successful encoding / decoding.
    TranscodeResult_Ok = 0,

    // A warning message, is set to the message out-param.
    TranscodeResult_Failure = 0x10,
    TranscodeResult_Failure_BadBuildId =          TranscodeResult_Failure | 0x1,
    TranscodeResult_Failure_RunOnceNotSupported = TranscodeResult_Failure | 0x2,
    TranscodeResult_Failure_AsmJSNotSupported =   TranscodeResult_Failure | 0x3,
    TranscodeResult_Failure_BadDecode =           TranscodeResult_Failure | 0x4,
    TranscodeResult_Failure_WrongCompileOption =  TranscodeResult_Failure | 0x5,
    TranscodeResult_Failure_NotInterpretedFun =   TranscodeResult_Failure | 0x6,

    // There is a pending exception on the context.
    TranscodeResult_Throw = 0x20
};

extern JS_PUBLIC_API(TranscodeResult)
EncodeScript(JSContext* cx, TranscodeBuffer& buffer, Handle<JSScript*> script);

extern JS_PUBLIC_API(TranscodeResult)
EncodeInterpretedFunction(JSContext* cx, TranscodeBuffer& buffer, Handle<JSObject*> funobj);

extern JS_PUBLIC_API(TranscodeResult)
DecodeScript(JSContext* cx, TranscodeBuffer& buffer, MutableHandle<JSScript*> scriptp,
             size_t cursorIndex = 0);

extern JS_PUBLIC_API(TranscodeResult)
DecodeScript(JSContext* cx, const TranscodeRange& range, MutableHandle<JSScript*> scriptp);

extern JS_PUBLIC_API(TranscodeResult)
DecodeInterpretedFunction(JSContext* cx, TranscodeBuffer& buffer, MutableHandle<JSFunction*> funp,
                          size_t cursorIndex = 0);

// Register an encoder on the given script source, such that all functions can
// be encoded as they are parsed. This strategy is used to avoid blocking the
// main thread in a non-interruptible way.
//
// The |script| argument of |StartIncrementalEncoding| and
// |FinishIncrementalEncoding| should be the top-level script returned either as
// an out-param of any of the |Compile| functions, or the result of
// |FinishOffThreadScript|.
//
// The |buffer| argument of |FinishIncrementalEncoding| is used for appending
// the encoded bytecode into the buffer. If any of these functions failed, the
// content of |buffer| would be undefined.
extern JS_PUBLIC_API(bool)
StartIncrementalEncoding(JSContext* cx, Handle<JSScript*> script);

extern JS_PUBLIC_API(bool)
FinishIncrementalEncoding(JSContext* cx, Handle<JSScript*> script, TranscodeBuffer& buffer);

} // namespace JS

#endif /* js_Transcoding_h */

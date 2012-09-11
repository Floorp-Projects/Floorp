/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef YarrJIT_h
#define YarrJIT_h

#include "assembler/wtf/Platform.h"

#if ENABLE_YARR_JIT

#include "assembler/assembler/MacroAssembler.h"
#include "YarrPattern.h"

#if WTF_CPU_X86 && !WTF_COMPILER_MSVC && !WTF_COMPILER_SUNCC
#define YARR_CALL __attribute__ ((regparm (3)))
#else
#define YARR_CALL
#endif

#if JS_TRACE_LOGGING
#include "TraceLogging.h"
#endif

namespace JSC {

class JSGlobalData;
class ExecutablePool;

namespace Yarr {

class YarrCodeBlock {
    typedef int (*YarrJITCode)(const UChar* input, unsigned start, unsigned length, int* output) YARR_CALL;

public:
    YarrCodeBlock()
        : m_needFallBack(false)
    {
    }

    ~YarrCodeBlock()
    {
    }

    void setFallBack(bool fallback) { m_needFallBack = fallback; }
    bool isFallBack() { return m_needFallBack; }
    void set(MacroAssembler::CodeRef ref) { m_ref = ref; }

    int execute(const UChar* input, unsigned start, unsigned length, int* output)
    {
#if JS_TRACE_LOGGING
        js::AutoTraceLog logger(js::TraceLogging::defaultLogger(),
                                js::TraceLogging::YARR_YIT_START,
                                js::TraceLogging::YARR_YIT_STOP);
#endif

        return JS_EXTENSION((reinterpret_cast<YarrJITCode>(m_ref.m_code.executableAddress()))(input, start, length, output));
    }

#if ENABLE_REGEXP_TRACING
    void *getAddr() { return m_ref.m_code.executableAddress(); }
#endif

    void release() { m_ref.release(); }

private:
    MacroAssembler::CodeRef m_ref;
    bool m_needFallBack;
};

void jitCompile(YarrPattern&, JSGlobalData*, YarrCodeBlock& jitObject);
int execute(YarrCodeBlock& jitObject, const UChar* input, unsigned start, unsigned length, int* output);

} } // namespace JSC::Yarr

#endif

#endif // YarrJIT_h

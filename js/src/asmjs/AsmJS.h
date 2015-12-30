/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef asmjs_asmjs_h
#define asmjs_asmjs_h

#include "vm/NativeObject.h"

namespace js {

class AsmJSModule;
class ExclusiveContext;
namespace frontend {
    template <typename ParseHandler> class Parser;
    template <typename ParseHandler> struct ParseContext;
    class FullParseHandler;
    class ParseNode;
}
typedef frontend::Parser<frontend::FullParseHandler> AsmJSParser;
typedef frontend::ParseContext<frontend::FullParseHandler> AsmJSParseContext;

// An AsmJSModuleObject is an internal implementation object (i.e., not exposed
// directly to user script) which traces and owns an AsmJSModule. The
// AsmJSModuleObject is referenced by the extended slots of the content-visible
// module and export JSFunctions.

class AsmJSModuleObject : public NativeObject
{
    static const unsigned MODULE_SLOT = 0;

  public:
    static const unsigned RESERVED_SLOTS = 1;

    bool hasModule() const;
    void setModule(AsmJSModule* module);
    AsmJSModule& module() const;

    void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* code, size_t* data);

    static const Class class_;
};

typedef Handle<AsmJSModuleObject*> HandleAsmJSModule;

// This function takes over parsing of a function starting with "use asm". The
// return value indicates whether an error was reported which the caller should
// propagate. If no error was reported, the function may still fail to validate
// as asm.js. In this case, the parser.tokenStream has been advanced an
// indeterminate amount and the entire function should be reparsed from the
// beginning.

extern bool
CompileAsmJS(ExclusiveContext* cx, AsmJSParser& parser, frontend::ParseNode* stmtList,
             bool* validated);

// asm.js module/export queries:

extern bool
IsAsmJSModuleNative(JSNative native);

extern bool
IsAsmJSModule(JSFunction* fun);

extern bool
IsAsmJSFunction(JSFunction* fun);

// asm.js testing natives:

extern bool
IsAsmJSCompilationAvailable(JSContext* cx, unsigned argc, JS::Value* vp);

extern bool
IsAsmJSModule(JSContext* cx, unsigned argc, JS::Value* vp);

extern bool
IsAsmJSModuleLoadedFromCache(JSContext* cx, unsigned argc, Value* vp);

extern bool
IsAsmJSFunction(JSContext* cx, unsigned argc, JS::Value* vp);

// asm.js toString/toSource support:

extern JSString*
AsmJSFunctionToString(JSContext* cx, HandleFunction fun);

extern JSString*
AsmJSModuleToString(JSContext* cx, HandleFunction fun, bool addParenToLambda);

// asm.js heap:

extern bool
IsValidAsmJSHeapLength(uint32_t length);

extern uint32_t
RoundUpToNextValidAsmJSHeapLength(uint32_t length);

extern bool
OnDetachAsmJSArrayBuffer(JSContext* cx, Handle<ArrayBufferObject*> buffer);

// The assumed page size; dynamically checked in CompileAsmJS.
#ifdef _MIPS_ARCH_LOONGSON3A
static const size_t AsmJSPageSize = 16384;
#else
static const size_t AsmJSPageSize = 4096;
#endif

#if defined(ASMJS_MAY_USE_SIGNAL_HANDLERS_FOR_OOB)
extern const size_t AsmJSMappedSize;
#endif

} // namespace js

#endif // asmjs_asmjs_h

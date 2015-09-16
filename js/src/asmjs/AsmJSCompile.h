/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
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

#ifndef jit_AsmJSCompile_h
#define jit_AsmJSCompile_h

#include "jit/CompileWrappers.h"

namespace js {

class AsmFunction;
class LifoAlloc;
class ModuleCompiler;
class ModuleCompileResults;

namespace jit {
    class LIRGraph;
    class MIRGenerator;
}

struct ModuleCompileInputs
{
    jit::CompileCompartment* compartment;
    jit::CompileRuntime* runtime;
    bool usesSignalHandlersForOOB;

    ModuleCompileInputs(jit::CompileCompartment* compartment,
                        jit::CompileRuntime* runtime,
                        bool usesSignalHandlersForOOB)
      : compartment(compartment),
        runtime(runtime),
        usesSignalHandlersForOOB(usesSignalHandlersForOOB)
    {}
};

class MOZ_RAII AsmModuleCompilerScope
{
    ModuleCompiler* m_;

    AsmModuleCompilerScope(const AsmModuleCompilerScope&) = delete;
    AsmModuleCompilerScope(const AsmModuleCompilerScope&&) = delete;
    AsmModuleCompilerScope& operator=(const AsmModuleCompilerScope&&) = delete;

  public:
    AsmModuleCompilerScope()
      : m_(nullptr)
    {}

    void setModule(ModuleCompiler* m) {
        MOZ_ASSERT(m);
        m_ = m;
    }

    ModuleCompiler& module() const {
        MOZ_ASSERT(m_);
        return *m_;
    }

    ~AsmModuleCompilerScope();
};

bool CreateAsmModuleCompiler(ModuleCompileInputs mci, AsmModuleCompilerScope* scope);
bool GenerateAsmFunctionMIR(ModuleCompiler& m, LifoAlloc& lifo, AsmFunction& func, jit::MIRGenerator** mir);
bool GenerateAsmFunctionCode(ModuleCompiler& m, AsmFunction& func, jit::MIRGenerator& mir, jit::LIRGraph& lir);
void FinishAsmModuleCompilation(ModuleCompiler& m, ScopedJSDeletePtr<ModuleCompileResults>* results);

} // namespace js

#endif // jit_AsmJSCompile_h

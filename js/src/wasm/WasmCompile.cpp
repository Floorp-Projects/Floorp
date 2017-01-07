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

#include "wasm/WasmCompile.h"

#include "jsprf.h"

#include "wasm/WasmBinaryIterator.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

static bool
DecodeFunctionBody(Decoder& d, ModuleGenerator& mg, uint32_t funcIndex)
{
    uint32_t bodySize;
    if (!d.readVarU32(&bodySize))
        return d.fail("expected number of function body bytes");

    const size_t offsetInModule = d.currentOffset();

    // Skip over the function body; we'll validate it later.
    const uint8_t* bodyBegin;
    if (!d.readBytes(bodySize, &bodyBegin))
        return d.fail("function body length too big");

    FunctionGenerator fg;
    if (!mg.startFuncDef(offsetInModule, &fg))
        return false;

    if (!fg.bytes().resize(bodySize))
        return false;

    memcpy(fg.bytes().begin(), bodyBegin, bodySize);

    return mg.finishFuncDef(funcIndex, &fg);
}

static bool
DecodeCodeSection(Decoder& d, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Code, &mg.mutableEnv(), &sectionStart, &sectionSize, "code"))
        return false;

    if (!mg.startFuncDefs())
        return false;

    if (sectionStart == Decoder::NotStarted) {
        if (mg.env().numFuncDefs() != 0)
            return d.fail("expected function bodies");

        return mg.finishFuncDefs();
    }

    uint32_t numFuncDefs;
    if (!d.readVarU32(&numFuncDefs))
        return d.fail("expected function body count");

    if (numFuncDefs != mg.env().numFuncDefs())
        return d.fail("function body count does not match function signature count");

    for (uint32_t funcDefIndex = 0; funcDefIndex < numFuncDefs; funcDefIndex++) {
        if (!DecodeFunctionBody(d, mg, mg.env().numFuncImports() + funcDefIndex))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "code"))
        return false;

    return mg.finishFuncDefs();
}

bool
CompileArgs::initFromContext(ExclusiveContext* cx, ScriptedCaller&& scriptedCaller)
{
    alwaysBaseline = cx->options().wasmAlwaysBaseline();
    this->scriptedCaller = Move(scriptedCaller);
    return assumptions.initBuildIdFromContext(cx);
}

SharedModule
wasm::Compile(const ShareableBytes& bytecode, const CompileArgs& args, UniqueChars* error)
{
    MOZ_RELEASE_ASSERT(wasm::HaveSignalHandlers());

    Decoder d(bytecode.begin(), bytecode.end(), error);

    auto env = js::MakeUnique<ModuleEnvironment>();
    if (!env)
        return nullptr;

    if (!DecodeModuleEnvironment(d, env.get()))
        return nullptr;

    ModuleGenerator mg(error);
    if (!mg.init(Move(env), args))
        return nullptr;

    if (!DecodeCodeSection(d, mg))
        return nullptr;

    if (!DecodeModuleTail(d, &mg.mutableEnv()))
        return nullptr;

    MOZ_ASSERT(!*error, "unreported error in decoding");

    return mg.finish(bytecode);
}

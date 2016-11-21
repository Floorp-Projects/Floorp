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

    if (d.bytesRemain() < bodySize)
        return d.fail("function body length too big");

    const uint8_t* bodyBegin = d.currentPosition();
    const size_t offsetInModule = d.currentOffset();

    FunctionGenerator fg;
    if (!mg.startFuncDef(offsetInModule, &fg))
        return false;

    if (!ValidateFunctionBody(mg.env(), funcIndex, d))
        return false;

    if (d.currentPosition() != bodyBegin + bodySize)
        return d.fail("function body length mismatch");

    if (!fg.bytes().resize(bodySize))
        return false;

    memcpy(fg.bytes().begin(), bodyBegin, bodySize);

    return mg.finishFuncDef(funcIndex, &fg);
}

// Section decoding.
static bool
DecodeCodeSection(Decoder& d, ModuleGenerator& mg)
{
    if (!mg.startFuncDefs())
        return false;

    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Code, &sectionStart, &sectionSize, "code"))
        return false;

    if (sectionStart == Decoder::NotStarted) {
        if (mg.numFuncDefs() != 0)
            return d.fail("expected function bodies");

        return mg.finishFuncDefs();
    }

    uint32_t numFuncDefs;
    if (!d.readVarU32(&numFuncDefs))
        return d.fail("expected function body count");

    if (numFuncDefs != mg.numFuncDefs())
        return d.fail("function body count does not match function signature count");

    for (uint32_t funcDefIndex = 0; funcDefIndex < numFuncDefs; funcDefIndex++) {
        if (!DecodeFunctionBody(d, mg, mg.numFuncImports() + funcDefIndex))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "code"))
        return false;

    return mg.finishFuncDefs();
}

static void
MaybeDecodeNameSectionBody(Decoder& d, NameInBytecodeVector* pfuncNames)
{
    // For simplicity, ignore all failures, even OOM. Failure will simply result
    // in the names section not being included for this module.

    uint32_t numFuncNames;
    if (!d.readVarU32(&numFuncNames))
        return;

    if (numFuncNames > MaxFuncs)
        return;

    // Use a local vector (and not pfuncNames) since it could result in a
    // partially initialized result in case of failure in the middle.
    NameInBytecodeVector funcNames;
    if (!funcNames.resize(numFuncNames))
        return;

    for (uint32_t i = 0; i < numFuncNames; i++) {
        uint32_t numBytes;
        if (!d.readVarU32(&numBytes))
            return;

        NameInBytecode name;
        name.offset = d.currentOffset();
        name.length = numBytes;
        funcNames[i] = name;

        if (!d.readBytes(numBytes))
            return;

        // Skip local names for a function.
        uint32_t numLocals;
        if (!d.readVarU32(&numLocals))
            return;
        for (uint32_t j = 0; j < numLocals; j++) {
            uint32_t numBytes;
            if (!d.readVarU32(&numBytes))
                return;
            if (!d.readBytes(numBytes))
                return;
        }
    }

    *pfuncNames = Move(funcNames);
}

static bool
DecodeNameSection(Decoder& d, NameInBytecodeVector* funcNames)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startUserDefinedSection(NameSectionName, &sectionStart, &sectionSize))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    // Once started, user-defined sections do not report validation errors.

    MaybeDecodeNameSectionBody(d, funcNames);

    d.finishUserDefinedSection(sectionStart, sectionSize);
    return true;
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

    ModuleGenerator mg;
    if (!mg.init(Move(env), args))
        return nullptr;

    if (!DecodeCodeSection(d, mg))
        return nullptr;

    DataSegmentVector dataSegments;
    if (!DecodeDataSection(d, mg.env(), &dataSegments))
        return nullptr;

    NameInBytecodeVector funcNames;
    if (!DecodeNameSection(d, &funcNames))
        return nullptr;

    if (!DecodeUnknownSections(d))
        return nullptr;

    MOZ_ASSERT(!*error, "unreported error in decoding");

    return mg.finish(bytecode,
                     Move(dataSegments),
                     Move(funcNames));
}

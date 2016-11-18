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

#include "wasm/WasmBinaryFormat.h"
#include "wasm/WasmBinaryIterator.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmSignalHandlers.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::IsNaN;

namespace {

struct ValidatingPolicy : OpIterPolicy
{
    // Validation is what we're all about here.
    static const bool Validate = true;
};

typedef OpIter<ValidatingPolicy> ValidatingOpIter;

class FunctionDecoder
{
    const ModuleGenerator& mg_;
    const ValTypeVector& locals_;
    ValidatingOpIter iter_;

  public:
    FunctionDecoder(const ModuleGenerator& mg, const ValTypeVector& locals, Decoder& d)
      : mg_(mg), locals_(locals), iter_(d)
    {}
    const ModuleGenerator& mg() const { return mg_; }
    ValidatingOpIter& iter() { return iter_; }
    const ValTypeVector& locals() const { return locals_; }

    bool checkHasMemory() {
        if (!mg().usesMemory())
            return iter().fail("can't touch memory without memory");
        return true;
    }
};

} // end anonymous namespace

static bool
DecodeCallArgs(FunctionDecoder& f, const Sig& sig)
{
    const ValTypeVector& args = sig.args();
    uint32_t numArgs = args.length();
    for (size_t i = 0; i < numArgs; ++i) {
        ValType argType = args[i];
        if (!f.iter().readCallArg(argType, numArgs, i, nullptr))
            return false;
    }

    return f.iter().readCallArgsEnd(numArgs);
}

static bool
DecodeCallReturn(FunctionDecoder& f, const Sig& sig)
{
    return f.iter().readCallReturn(sig.ret());
}

static bool
DecodeCall(FunctionDecoder& f)
{
    uint32_t funcIndex;
    if (!f.iter().readCall(&funcIndex))
        return false;

    if (funcIndex >= f.mg().numFuncs())
        return f.iter().fail("callee index out of range");

    if (!f.iter().inReachableCode())
        return true;

    const Sig* sig = &f.mg().funcSig(funcIndex);

    return DecodeCallArgs(f, *sig) &&
           DecodeCallReturn(f, *sig);
}

static bool
DecodeCallIndirect(FunctionDecoder& f)
{
    if (!f.mg().numTables())
        return f.iter().fail("can't call_indirect without a table");

    uint32_t sigIndex;
    if (!f.iter().readCallIndirect(&sigIndex, nullptr))
        return false;

    if (sigIndex >= f.mg().numSigs())
        return f.iter().fail("signature index out of range");

    if (!f.iter().inReachableCode())
        return true;

    const Sig& sig = f.mg().sig(sigIndex);
    if (!DecodeCallArgs(f, sig))
        return false;

    return DecodeCallReturn(f, sig);
}

static bool
DecodeBrTable(FunctionDecoder& f)
{
    uint32_t tableLength;
    ExprType type = ExprType::Limit;
    if (!f.iter().readBrTable(&tableLength, &type, nullptr, nullptr))
        return false;

    uint32_t depth;
    for (size_t i = 0, e = tableLength; i < e; ++i) {
        if (!f.iter().readBrTableEntry(&type, nullptr, &depth))
            return false;
    }

    // Read the default label.
    return f.iter().readBrTableDefault(&type, nullptr, &depth);
}

static bool
DecodeFunctionBodyExprs(FunctionDecoder& f)
{
#define CHECK(c) if (!(c)) return false; break

    while (true) {
        uint16_t op;
        if (!f.iter().readOp(&op))
            return false;

        switch (op) {
          case uint16_t(Op::End):
            if (!f.iter().readEnd(nullptr, nullptr, nullptr))
                return false;
            if (f.iter().controlStackEmpty())
                return true;
            break;
          case uint16_t(Op::Nop):
            CHECK(f.iter().readNop());
          case uint16_t(Op::Drop):
            CHECK(f.iter().readDrop());
          case uint16_t(Op::Call):
            CHECK(DecodeCall(f));
          case uint16_t(Op::CallIndirect):
            CHECK(DecodeCallIndirect(f));
          case uint16_t(Op::I32Const):
            CHECK(f.iter().readI32Const(nullptr));
          case uint16_t(Op::I64Const):
            CHECK(f.iter().readI64Const(nullptr));
          case uint16_t(Op::F32Const):
            CHECK(f.iter().readF32Const(nullptr));
          case uint16_t(Op::F64Const):
            CHECK(f.iter().readF64Const(nullptr));
          case uint16_t(Op::GetLocal):
            CHECK(f.iter().readGetLocal(f.locals(), nullptr));
          case uint16_t(Op::SetLocal):
            CHECK(f.iter().readSetLocal(f.locals(), nullptr, nullptr));
          case uint16_t(Op::TeeLocal):
            CHECK(f.iter().readTeeLocal(f.locals(), nullptr, nullptr));
          case uint16_t(Op::GetGlobal):
            CHECK(f.iter().readGetGlobal(f.mg().globals(), nullptr));
          case uint16_t(Op::SetGlobal):
            CHECK(f.iter().readSetGlobal(f.mg().globals(), nullptr, nullptr));
          case uint16_t(Op::Select):
            CHECK(f.iter().readSelect(nullptr, nullptr, nullptr, nullptr));
          case uint16_t(Op::Block):
            CHECK(f.iter().readBlock());
          case uint16_t(Op::Loop):
            CHECK(f.iter().readLoop());
          case uint16_t(Op::If):
            CHECK(f.iter().readIf(nullptr));
          case uint16_t(Op::Else):
            CHECK(f.iter().readElse(nullptr, nullptr));
          case uint16_t(Op::I32Clz):
          case uint16_t(Op::I32Ctz):
          case uint16_t(Op::I32Popcnt):
            CHECK(f.iter().readUnary(ValType::I32, nullptr));
          case uint16_t(Op::I64Clz):
          case uint16_t(Op::I64Ctz):
          case uint16_t(Op::I64Popcnt):
            CHECK(f.iter().readUnary(ValType::I64, nullptr));
          case uint16_t(Op::F32Abs):
          case uint16_t(Op::F32Neg):
          case uint16_t(Op::F32Ceil):
          case uint16_t(Op::F32Floor):
          case uint16_t(Op::F32Sqrt):
          case uint16_t(Op::F32Trunc):
          case uint16_t(Op::F32Nearest):
            CHECK(f.iter().readUnary(ValType::F32, nullptr));
          case uint16_t(Op::F64Abs):
          case uint16_t(Op::F64Neg):
          case uint16_t(Op::F64Ceil):
          case uint16_t(Op::F64Floor):
          case uint16_t(Op::F64Sqrt):
          case uint16_t(Op::F64Trunc):
          case uint16_t(Op::F64Nearest):
            CHECK(f.iter().readUnary(ValType::F64, nullptr));
          case uint16_t(Op::I32Add):
          case uint16_t(Op::I32Sub):
          case uint16_t(Op::I32Mul):
          case uint16_t(Op::I32DivS):
          case uint16_t(Op::I32DivU):
          case uint16_t(Op::I32RemS):
          case uint16_t(Op::I32RemU):
          case uint16_t(Op::I32And):
          case uint16_t(Op::I32Or):
          case uint16_t(Op::I32Xor):
          case uint16_t(Op::I32Shl):
          case uint16_t(Op::I32ShrS):
          case uint16_t(Op::I32ShrU):
          case uint16_t(Op::I32Rotl):
          case uint16_t(Op::I32Rotr):
            CHECK(f.iter().readBinary(ValType::I32, nullptr, nullptr));
          case uint16_t(Op::I64Add):
          case uint16_t(Op::I64Sub):
          case uint16_t(Op::I64Mul):
          case uint16_t(Op::I64DivS):
          case uint16_t(Op::I64DivU):
          case uint16_t(Op::I64RemS):
          case uint16_t(Op::I64RemU):
          case uint16_t(Op::I64And):
          case uint16_t(Op::I64Or):
          case uint16_t(Op::I64Xor):
          case uint16_t(Op::I64Shl):
          case uint16_t(Op::I64ShrS):
          case uint16_t(Op::I64ShrU):
          case uint16_t(Op::I64Rotl):
          case uint16_t(Op::I64Rotr):
            CHECK(f.iter().readBinary(ValType::I64, nullptr, nullptr));
          case uint16_t(Op::F32Add):
          case uint16_t(Op::F32Sub):
          case uint16_t(Op::F32Mul):
          case uint16_t(Op::F32Div):
          case uint16_t(Op::F32Min):
          case uint16_t(Op::F32Max):
          case uint16_t(Op::F32CopySign):
            CHECK(f.iter().readBinary(ValType::F32, nullptr, nullptr));
          case uint16_t(Op::F64Add):
          case uint16_t(Op::F64Sub):
          case uint16_t(Op::F64Mul):
          case uint16_t(Op::F64Div):
          case uint16_t(Op::F64Min):
          case uint16_t(Op::F64Max):
          case uint16_t(Op::F64CopySign):
            CHECK(f.iter().readBinary(ValType::F64, nullptr, nullptr));
          case uint16_t(Op::I32Eq):
          case uint16_t(Op::I32Ne):
          case uint16_t(Op::I32LtS):
          case uint16_t(Op::I32LtU):
          case uint16_t(Op::I32LeS):
          case uint16_t(Op::I32LeU):
          case uint16_t(Op::I32GtS):
          case uint16_t(Op::I32GtU):
          case uint16_t(Op::I32GeS):
          case uint16_t(Op::I32GeU):
            CHECK(f.iter().readComparison(ValType::I32, nullptr, nullptr));
          case uint16_t(Op::I64Eq):
          case uint16_t(Op::I64Ne):
          case uint16_t(Op::I64LtS):
          case uint16_t(Op::I64LtU):
          case uint16_t(Op::I64LeS):
          case uint16_t(Op::I64LeU):
          case uint16_t(Op::I64GtS):
          case uint16_t(Op::I64GtU):
          case uint16_t(Op::I64GeS):
          case uint16_t(Op::I64GeU):
            CHECK(f.iter().readComparison(ValType::I64, nullptr, nullptr));
          case uint16_t(Op::F32Eq):
          case uint16_t(Op::F32Ne):
          case uint16_t(Op::F32Lt):
          case uint16_t(Op::F32Le):
          case uint16_t(Op::F32Gt):
          case uint16_t(Op::F32Ge):
            CHECK(f.iter().readComparison(ValType::F32, nullptr, nullptr));
          case uint16_t(Op::F64Eq):
          case uint16_t(Op::F64Ne):
          case uint16_t(Op::F64Lt):
          case uint16_t(Op::F64Le):
          case uint16_t(Op::F64Gt):
          case uint16_t(Op::F64Ge):
            CHECK(f.iter().readComparison(ValType::F64, nullptr, nullptr));
          case uint16_t(Op::I32Eqz):
            CHECK(f.iter().readConversion(ValType::I32, ValType::I32, nullptr));
          case uint16_t(Op::I64Eqz):
          case uint16_t(Op::I32WrapI64):
            CHECK(f.iter().readConversion(ValType::I64, ValType::I32, nullptr));
          case uint16_t(Op::I32TruncSF32):
          case uint16_t(Op::I32TruncUF32):
          case uint16_t(Op::I32ReinterpretF32):
            CHECK(f.iter().readConversion(ValType::F32, ValType::I32, nullptr));
          case uint16_t(Op::I32TruncSF64):
          case uint16_t(Op::I32TruncUF64):
            CHECK(f.iter().readConversion(ValType::F64, ValType::I32, nullptr));
          case uint16_t(Op::I64ExtendSI32):
          case uint16_t(Op::I64ExtendUI32):
            CHECK(f.iter().readConversion(ValType::I32, ValType::I64, nullptr));
          case uint16_t(Op::I64TruncSF32):
          case uint16_t(Op::I64TruncUF32):
            CHECK(f.iter().readConversion(ValType::F32, ValType::I64, nullptr));
          case uint16_t(Op::I64TruncSF64):
          case uint16_t(Op::I64TruncUF64):
          case uint16_t(Op::I64ReinterpretF64):
            CHECK(f.iter().readConversion(ValType::F64, ValType::I64, nullptr));
          case uint16_t(Op::F32ConvertSI32):
          case uint16_t(Op::F32ConvertUI32):
          case uint16_t(Op::F32ReinterpretI32):
            CHECK(f.iter().readConversion(ValType::I32, ValType::F32, nullptr));
          case uint16_t(Op::F32ConvertSI64):
          case uint16_t(Op::F32ConvertUI64):
            CHECK(f.iter().readConversion(ValType::I64, ValType::F32, nullptr));
          case uint16_t(Op::F32DemoteF64):
            CHECK(f.iter().readConversion(ValType::F64, ValType::F32, nullptr));
          case uint16_t(Op::F64ConvertSI32):
          case uint16_t(Op::F64ConvertUI32):
            CHECK(f.iter().readConversion(ValType::I32, ValType::F64, nullptr));
          case uint16_t(Op::F64ConvertSI64):
          case uint16_t(Op::F64ConvertUI64):
          case uint16_t(Op::F64ReinterpretI64):
            CHECK(f.iter().readConversion(ValType::I64, ValType::F64, nullptr));
          case uint16_t(Op::F64PromoteF32):
            CHECK(f.iter().readConversion(ValType::F32, ValType::F64, nullptr));
          case uint16_t(Op::I32Load8S):
          case uint16_t(Op::I32Load8U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 1, nullptr));
          case uint16_t(Op::I32Load16S):
          case uint16_t(Op::I32Load16U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 2, nullptr));
          case uint16_t(Op::I32Load):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 4, nullptr));
          case uint16_t(Op::I64Load8S):
          case uint16_t(Op::I64Load8U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 1, nullptr));
          case uint16_t(Op::I64Load16S):
          case uint16_t(Op::I64Load16U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 2, nullptr));
          case uint16_t(Op::I64Load32S):
          case uint16_t(Op::I64Load32U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 4, nullptr));
          case uint16_t(Op::I64Load):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 8, nullptr));
          case uint16_t(Op::F32Load):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::F32, 4, nullptr));
          case uint16_t(Op::F64Load):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::F64, 8, nullptr));
          case uint16_t(Op::I32Store8):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 1, nullptr, nullptr));
          case uint16_t(Op::I32Store16):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 2, nullptr, nullptr));
          case uint16_t(Op::I32Store):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 4, nullptr, nullptr));
          case uint16_t(Op::I64Store8):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 1, nullptr, nullptr));
          case uint16_t(Op::I64Store16):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 2, nullptr, nullptr));
          case uint16_t(Op::I64Store32):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 4, nullptr, nullptr));
          case uint16_t(Op::I64Store):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 8, nullptr, nullptr));
          case uint16_t(Op::F32Store):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::F32, 4, nullptr, nullptr));
          case uint16_t(Op::F64Store):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::F64, 8, nullptr, nullptr));
          case uint16_t(Op::GrowMemory):
            CHECK(f.checkHasMemory() && f.iter().readGrowMemory(nullptr));
          case uint16_t(Op::CurrentMemory):
            CHECK(f.checkHasMemory() && f.iter().readCurrentMemory());
          case uint16_t(Op::Br):
            CHECK(f.iter().readBr(nullptr, nullptr, nullptr));
          case uint16_t(Op::BrIf):
            CHECK(f.iter().readBrIf(nullptr, nullptr, nullptr, nullptr));
          case uint16_t(Op::BrTable):
            CHECK(DecodeBrTable(f));
          case uint16_t(Op::Return):
            CHECK(f.iter().readReturn(nullptr));
          case uint16_t(Op::Unreachable):
            CHECK(f.iter().readUnreachable());
          default:
            return f.iter().unrecognizedOpcode(op);
        }
    }

    MOZ_CRASH("unreachable");

#undef CHECK
}

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

    ValTypeVector locals;
    const Sig& sig = mg.funcSig(funcIndex);
    if (!locals.appendAll(sig.args()))
        return false;

    if (!DecodeLocalEntries(d, ModuleKind::Wasm, &locals))
        return false;

    FunctionDecoder f(mg, locals, d);

    if (!f.iter().readFunctionStart(sig.ret()))
        return false;

    if (!DecodeFunctionBodyExprs(f))
        return false;

    if (!f.iter().readFunctionEnd())
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
DecodeImportSection(Decoder& d, ModuleGeneratorData* init, ImportVector* imports)
{
    Maybe<Limits> memory;
    if (!DecodeImportSection(d, init->sigs, &init->funcSigs, &init->globals, &init->tables, &memory,
                             imports))
        return false;

    // The global data offsets will be filled in by ModuleGenerator::init.
    if (!init->funcImportGlobalDataOffsets.resize(init->funcSigs.length()))
        return false;

    if (memory) {
        init->memoryUsage = MemoryUsage::Unshared;
        init->minMemoryLength = memory->initial;
        init->maxMemoryLength = memory->maximum;
    }

    return true;
}

static bool
DecodeMemorySection(Decoder& d, ModuleGeneratorData* init)
{
    bool present;
    Limits memory;
    if (!DecodeMemorySection(d, UsesMemory(init->memoryUsage), &memory, &present))
        return false;

    if (present) {
        init->memoryUsage = MemoryUsage::Unshared;
        init->minMemoryLength = memory.initial;
        init->maxMemoryLength = memory.maximum;
    }

    return true;
}

static bool
DecodeExportSection(Decoder& d, ModuleGenerator& mg)
{
    ExportVector exports;
    if (!DecodeExportSection(d, mg.numFuncs(), mg.numTables(), mg.usesMemory(), mg.globals(),
                             &exports))
        return false;

    return mg.setExports(Move(exports));
}

static bool
DecodeStartSection(Decoder& d, ModuleGenerator& mg)
{
    Maybe<uint32_t> startFuncIndex;
    if (!DecodeStartSection(d, mg.funcSigs(), &startFuncIndex))
        return false;

    return !startFuncIndex || mg.setStartFunction(*startFuncIndex);
}

static bool
DecodeElemSection(Decoder& d, ModuleGenerator& mg)
{
    ElemSegmentVector elems;
    if (!DecodeElemSection(d, mg.tables(), mg.globals(), mg.numFuncs(), &elems))
        return false;

    mg.setElemSegments(Move(elems));
    return true;
}

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

static bool
DecodeDataSection(Decoder& d, ModuleGenerator& mg)
{
    DataSegmentVector dataSegments;
    if (!DecodeDataSection(d, mg.usesMemory(), mg.minMemoryLength(), mg.globals(), &dataSegments))
        return false;

    mg.setDataSegments(Move(dataSegments));
    return true;
}

static void
MaybeDecodeNameSectionBody(Decoder& d, ModuleGenerator& mg)
{
    // For simplicity, ignore all failures, even OOM. Failure will simply result
    // in the names section not being included for this module.

    uint32_t numFuncNames;
    if (!d.readVarU32(&numFuncNames))
        return;

    if (numFuncNames > MaxFuncs)
        return;

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

    mg.setFuncNames(Move(funcNames));
}

static bool
DecodeNameSection(Decoder& d, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startUserDefinedSection(NameSectionName, &sectionStart, &sectionSize))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    // Once started, user-defined sections do not report validation errors.

    MaybeDecodeNameSectionBody(d, mg);

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

    auto init = js::MakeUnique<ModuleGeneratorData>();
    if (!init)
        return nullptr;

    if (!DecodePreamble(d))
        return nullptr;

    if (!DecodeTypeSection(d, &init->sigs))
        return nullptr;

    ImportVector imports;
    if (!::DecodeImportSection(d, init.get(), &imports))
        return nullptr;

    if (!DecodeFunctionSection(d, init->sigs, &init->funcSigs))
        return nullptr;

    if (!DecodeTableSection(d, &init->tables))
        return nullptr;

    if (!::DecodeMemorySection(d, init.get()))
        return nullptr;

    if (!DecodeGlobalSection(d, &init->globals))
        return nullptr;

    ModuleGenerator mg(Move(imports));
    if (!mg.init(Move(init), args))
        return nullptr;

    if (!::DecodeExportSection(d, mg))
        return nullptr;

    if (!::DecodeStartSection(d, mg))
        return nullptr;

    if (!::DecodeElemSection(d, mg))
        return nullptr;

    if (!DecodeCodeSection(d, mg))
        return nullptr;

    if (!::DecodeDataSection(d, mg))
        return nullptr;

    if (!DecodeNameSection(d, mg))
        return nullptr;

    if (!DecodeUnknownSections(d))
        return nullptr;

    MOZ_ASSERT(!*error, "unreported error in decoding");

    return mg.finish(bytecode);
}

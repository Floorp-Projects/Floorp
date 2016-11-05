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

struct ValidatingPolicy : ExprIterPolicy
{
    // Validation is what we're all about here.
    static const bool Validate = true;
};

typedef ExprIter<ValidatingPolicy> ValidatingExprIter;

class FunctionDecoder
{
    const ModuleGenerator& mg_;
    const ValTypeVector& locals_;
    ValidatingExprIter iter_;

  public:
    FunctionDecoder(const ModuleGenerator& mg, const ValTypeVector& locals, Decoder& d)
      : mg_(mg), locals_(locals), iter_(d)
    {}
    const ModuleGenerator& mg() const { return mg_; }
    ValidatingExprIter& iter() { return iter_; }
    const ValTypeVector& locals() const { return locals_; }

    bool checkHasMemory() {
        if (!mg().usesMemory())
            return iter().fail("can't touch memory without memory");
        return true;
    }
};

} // end anonymous namespace

static bool
CheckValType(Decoder& d, ValType type)
{
    switch (type) {
      case ValType::I32:
      case ValType::F32:
      case ValType::F64:
      case ValType::I64:
        return true;
      default:
        // Note: it's important not to remove this default since readValType()
        // can return ValType values for which there is no enumerator.
        break;
    }

    return d.fail("bad type");
}

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
    uint32_t calleeIndex;
    if (!f.iter().readCall(&calleeIndex))
        return false;

    if (calleeIndex >= f.mg().numFuncs())
        return f.iter().fail("callee index out of range");

    if (!f.iter().inReachableCode())
        return true;

    const Sig* sig = &f.mg().funcSig(calleeIndex);

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
        Expr expr;
        if (!f.iter().readExpr(&expr))
            return false;

        switch (expr) {
          case Expr::End:
            if (!f.iter().readEnd(nullptr, nullptr, nullptr))
                return false;
            if (f.iter().controlStackEmpty())
                return true;
            break;
          case Expr::Nop:
            CHECK(f.iter().readNop());
          case Expr::Drop:
            CHECK(f.iter().readDrop());
          case Expr::Call:
            CHECK(DecodeCall(f));
          case Expr::CallIndirect:
            CHECK(DecodeCallIndirect(f));
          case Expr::I32Const:
            CHECK(f.iter().readI32Const(nullptr));
          case Expr::I64Const:
            CHECK(f.iter().readI64Const(nullptr));
          case Expr::F32Const:
            CHECK(f.iter().readF32Const(nullptr));
          case Expr::F64Const:
            CHECK(f.iter().readF64Const(nullptr));
          case Expr::GetLocal:
            CHECK(f.iter().readGetLocal(f.locals(), nullptr));
          case Expr::SetLocal:
            CHECK(f.iter().readSetLocal(f.locals(), nullptr, nullptr));
          case Expr::TeeLocal:
            CHECK(f.iter().readTeeLocal(f.locals(), nullptr, nullptr));
          case Expr::GetGlobal:
            CHECK(f.iter().readGetGlobal(f.mg().globals(), nullptr));
          case Expr::SetGlobal:
            CHECK(f.iter().readSetGlobal(f.mg().globals(), nullptr, nullptr));
          case Expr::Select:
            CHECK(f.iter().readSelect(nullptr, nullptr, nullptr, nullptr));
          case Expr::Block:
            CHECK(f.iter().readBlock());
          case Expr::Loop:
            CHECK(f.iter().readLoop());
          case Expr::If:
            CHECK(f.iter().readIf(nullptr));
          case Expr::Else:
            CHECK(f.iter().readElse(nullptr, nullptr));
          case Expr::I32Clz:
          case Expr::I32Ctz:
          case Expr::I32Popcnt:
            CHECK(f.iter().readUnary(ValType::I32, nullptr));
          case Expr::I64Clz:
          case Expr::I64Ctz:
          case Expr::I64Popcnt:
            CHECK(f.iter().readUnary(ValType::I64, nullptr));
          case Expr::F32Abs:
          case Expr::F32Neg:
          case Expr::F32Ceil:
          case Expr::F32Floor:
          case Expr::F32Sqrt:
          case Expr::F32Trunc:
          case Expr::F32Nearest:
            CHECK(f.iter().readUnary(ValType::F32, nullptr));
          case Expr::F64Abs:
          case Expr::F64Neg:
          case Expr::F64Ceil:
          case Expr::F64Floor:
          case Expr::F64Sqrt:
          case Expr::F64Trunc:
          case Expr::F64Nearest:
            CHECK(f.iter().readUnary(ValType::F64, nullptr));
          case Expr::I32Add:
          case Expr::I32Sub:
          case Expr::I32Mul:
          case Expr::I32DivS:
          case Expr::I32DivU:
          case Expr::I32RemS:
          case Expr::I32RemU:
          case Expr::I32And:
          case Expr::I32Or:
          case Expr::I32Xor:
          case Expr::I32Shl:
          case Expr::I32ShrS:
          case Expr::I32ShrU:
          case Expr::I32Rotl:
          case Expr::I32Rotr:
            CHECK(f.iter().readBinary(ValType::I32, nullptr, nullptr));
          case Expr::I64Add:
          case Expr::I64Sub:
          case Expr::I64Mul:
          case Expr::I64DivS:
          case Expr::I64DivU:
          case Expr::I64RemS:
          case Expr::I64RemU:
          case Expr::I64And:
          case Expr::I64Or:
          case Expr::I64Xor:
          case Expr::I64Shl:
          case Expr::I64ShrS:
          case Expr::I64ShrU:
          case Expr::I64Rotl:
          case Expr::I64Rotr:
            CHECK(f.iter().readBinary(ValType::I64, nullptr, nullptr));
          case Expr::F32Add:
          case Expr::F32Sub:
          case Expr::F32Mul:
          case Expr::F32Div:
          case Expr::F32Min:
          case Expr::F32Max:
          case Expr::F32CopySign:
            CHECK(f.iter().readBinary(ValType::F32, nullptr, nullptr));
          case Expr::F64Add:
          case Expr::F64Sub:
          case Expr::F64Mul:
          case Expr::F64Div:
          case Expr::F64Min:
          case Expr::F64Max:
          case Expr::F64CopySign:
            CHECK(f.iter().readBinary(ValType::F64, nullptr, nullptr));
          case Expr::I32Eq:
          case Expr::I32Ne:
          case Expr::I32LtS:
          case Expr::I32LtU:
          case Expr::I32LeS:
          case Expr::I32LeU:
          case Expr::I32GtS:
          case Expr::I32GtU:
          case Expr::I32GeS:
          case Expr::I32GeU:
            CHECK(f.iter().readComparison(ValType::I32, nullptr, nullptr));
          case Expr::I64Eq:
          case Expr::I64Ne:
          case Expr::I64LtS:
          case Expr::I64LtU:
          case Expr::I64LeS:
          case Expr::I64LeU:
          case Expr::I64GtS:
          case Expr::I64GtU:
          case Expr::I64GeS:
          case Expr::I64GeU:
            CHECK(f.iter().readComparison(ValType::I64, nullptr, nullptr));
          case Expr::F32Eq:
          case Expr::F32Ne:
          case Expr::F32Lt:
          case Expr::F32Le:
          case Expr::F32Gt:
          case Expr::F32Ge:
            CHECK(f.iter().readComparison(ValType::F32, nullptr, nullptr));
          case Expr::F64Eq:
          case Expr::F64Ne:
          case Expr::F64Lt:
          case Expr::F64Le:
          case Expr::F64Gt:
          case Expr::F64Ge:
            CHECK(f.iter().readComparison(ValType::F64, nullptr, nullptr));
          case Expr::I32Eqz:
            CHECK(f.iter().readConversion(ValType::I32, ValType::I32, nullptr));
          case Expr::I64Eqz:
          case Expr::I32WrapI64:
            CHECK(f.iter().readConversion(ValType::I64, ValType::I32, nullptr));
          case Expr::I32TruncSF32:
          case Expr::I32TruncUF32:
          case Expr::I32ReinterpretF32:
            CHECK(f.iter().readConversion(ValType::F32, ValType::I32, nullptr));
          case Expr::I32TruncSF64:
          case Expr::I32TruncUF64:
            CHECK(f.iter().readConversion(ValType::F64, ValType::I32, nullptr));
          case Expr::I64ExtendSI32:
          case Expr::I64ExtendUI32:
            CHECK(f.iter().readConversion(ValType::I32, ValType::I64, nullptr));
          case Expr::I64TruncSF32:
          case Expr::I64TruncUF32:
            CHECK(f.iter().readConversion(ValType::F32, ValType::I64, nullptr));
          case Expr::I64TruncSF64:
          case Expr::I64TruncUF64:
          case Expr::I64ReinterpretF64:
            CHECK(f.iter().readConversion(ValType::F64, ValType::I64, nullptr));
          case Expr::F32ConvertSI32:
          case Expr::F32ConvertUI32:
          case Expr::F32ReinterpretI32:
            CHECK(f.iter().readConversion(ValType::I32, ValType::F32, nullptr));
          case Expr::F32ConvertSI64:
          case Expr::F32ConvertUI64:
            CHECK(f.iter().readConversion(ValType::I64, ValType::F32, nullptr));
          case Expr::F32DemoteF64:
            CHECK(f.iter().readConversion(ValType::F64, ValType::F32, nullptr));
          case Expr::F64ConvertSI32:
          case Expr::F64ConvertUI32:
            CHECK(f.iter().readConversion(ValType::I32, ValType::F64, nullptr));
          case Expr::F64ConvertSI64:
          case Expr::F64ConvertUI64:
          case Expr::F64ReinterpretI64:
            CHECK(f.iter().readConversion(ValType::I64, ValType::F64, nullptr));
          case Expr::F64PromoteF32:
            CHECK(f.iter().readConversion(ValType::F32, ValType::F64, nullptr));
          case Expr::I32Load8S:
          case Expr::I32Load8U:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 1, nullptr));
          case Expr::I32Load16S:
          case Expr::I32Load16U:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 2, nullptr));
          case Expr::I32Load:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 4, nullptr));
          case Expr::I64Load8S:
          case Expr::I64Load8U:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 1, nullptr));
          case Expr::I64Load16S:
          case Expr::I64Load16U:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 2, nullptr));
          case Expr::I64Load32S:
          case Expr::I64Load32U:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 4, nullptr));
          case Expr::I64Load:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 8, nullptr));
          case Expr::F32Load:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::F32, 4, nullptr));
          case Expr::F64Load:
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::F64, 8, nullptr));
          case Expr::I32Store8:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 1, nullptr, nullptr));
          case Expr::I32Store16:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 2, nullptr, nullptr));
          case Expr::I32Store:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 4, nullptr, nullptr));
          case Expr::I64Store8:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 1, nullptr, nullptr));
          case Expr::I64Store16:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 2, nullptr, nullptr));
          case Expr::I64Store32:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 4, nullptr, nullptr));
          case Expr::I64Store:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 8, nullptr, nullptr));
          case Expr::F32Store:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::F32, 4, nullptr, nullptr));
          case Expr::F64Store:
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::F64, 8, nullptr, nullptr));
          case Expr::GrowMemory:
            CHECK(f.checkHasMemory() && f.iter().readGrowMemory(nullptr));
          case Expr::CurrentMemory:
            CHECK(f.checkHasMemory() && f.iter().readCurrentMemory());
          case Expr::Br:
            CHECK(f.iter().readBr(nullptr, nullptr, nullptr));
          case Expr::BrIf:
            CHECK(f.iter().readBrIf(nullptr, nullptr, nullptr, nullptr));
          case Expr::BrTable:
            CHECK(DecodeBrTable(f));
          case Expr::Return:
            CHECK(f.iter().readReturn(nullptr));
          case Expr::Unreachable:
            CHECK(f.iter().readUnreachable());
          default:
            return f.iter().unrecognizedOpcode(expr);
        }
    }

    MOZ_CRASH("unreachable");

#undef CHECK
}

static bool
DecodeTypeSection(Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Type, &sectionStart, &sectionSize, "type"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSigs;
    if (!d.readVarU32(&numSigs))
        return d.fail("expected number of signatures");

    if (numSigs > MaxSigs)
        return d.fail("too many signatures");

    if (!init->sigs.resize(numSigs))
        return false;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t form;
        if (!d.readVarU32(&form) || form != uint32_t(TypeCode::Func))
            return d.fail("expected function form");

        uint32_t numArgs;
        if (!d.readVarU32(&numArgs))
            return d.fail("bad number of function args");

        if (numArgs > MaxArgsPerFunc)
            return d.fail("too many arguments in signature");

        ValTypeVector args;
        if (!args.resize(numArgs))
            return false;

        for (uint32_t i = 0; i < numArgs; i++) {
            if (!d.readValType(&args[i]))
                return d.fail("bad value type");

            if (!CheckValType(d, args[i]))
                return false;
        }

        uint32_t numRets;
        if (!d.readVarU32(&numRets))
            return d.fail("bad number of function returns");

        if (numRets > 1)
            return d.fail("too many returns in signature");

        ExprType result = ExprType::Void;

        if (numRets == 1) {
            ValType type;
            if (!d.readValType(&type))
                return d.fail("bad expression type");

            if (!CheckValType(d, type))
                return false;

            result = ToExprType(type);
        }

        init->sigs[sigIndex] = Sig(Move(args), result);
    }

    if (!d.finishSection(sectionStart, sectionSize, "type"))
        return false;

    return true;
}

static bool
DecodeSignatureIndex(Decoder& d, const ModuleGeneratorData& init, const SigWithId** sig)
{
    uint32_t sigIndex;
    if (!d.readVarU32(&sigIndex))
        return d.fail("expected signature index");

    if (sigIndex >= init.sigs.length())
        return d.fail("signature index out of range");

    *sig = &init.sigs[sigIndex];
    return true;
}

static bool
DecodeFunctionSection(Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Function, &sectionStart, &sectionSize, "function"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numDefs;
    if (!d.readVarU32(&numDefs))
        return d.fail("expected number of function definitions");

    if (numDefs > MaxFuncs)
        return d.fail("too many functions");

    if (!init->funcDefSigs.resize(numDefs))
        return false;

    for (uint32_t i = 0; i < numDefs; i++) {
        if (!DecodeSignatureIndex(d, *init, &init->funcDefSigs[i]))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "function"))
        return false;

    return true;
}

static UniqueChars
DecodeName(Decoder& d)
{
    uint32_t numBytes;
    if (!d.readVarU32(&numBytes))
        return nullptr;

    const uint8_t* bytes;
    if (!d.readBytes(numBytes, &bytes))
        return nullptr;

    UniqueChars name(js_pod_malloc<char>(numBytes + 1));
    if (!name)
        return nullptr;

    memcpy(name.get(), bytes, numBytes);
    name[numBytes] = '\0';

    return name;
}

static bool
DecodeMemoryLimits(Decoder& d, ModuleGeneratorData* init)
{
    Limits memory;
    if (!DecodeMemoryLimits(d, UsesMemory(init->memoryUsage), &memory))
        return false;

    init->memoryUsage = MemoryUsage::Unshared;
    init->minMemoryLength = memory.initial;
    init->maxMemoryLength = memory.maximum;
    return true;
}

static bool
DecodeResizableTable(Decoder& d, ModuleGeneratorData* init)
{
    uint32_t elementType;
    if (!d.readVarU32(&elementType))
        return d.fail("expected table element type");

    if (elementType != uint32_t(TypeCode::AnyFunc))
        return d.fail("expected 'anyfunc' element type");

    Limits limits;
    if (!DecodeLimits(d, &limits))
        return false;

    if (!init->tables.empty())
        return d.fail("already have default table");

    return init->tables.emplaceBack(TableKind::AnyFunction, limits);
}

static bool
GlobalIsJSCompatible(Decoder& d, ValType type, bool isMutable)
{
    switch (type) {
      case ValType::I32:
      case ValType::F32:
      case ValType::F64:
        break;
      case ValType::I64:
        if (!JitOptions.wasmTestMode)
            return d.fail("can't import/export an Int64 global to JS");
        break;
      default:
        return d.fail("unexpected variable type in global import/export");
    }

    if (isMutable)
        return d.fail("can't import/export mutable globals in the MVP");

    return true;
}

static bool
DecodeImport(Decoder& d, ModuleGeneratorData* init, ImportVector* imports)
{
    UniqueChars moduleName = DecodeName(d);
    if (!moduleName)
        return d.fail("expected valid import module name");

    UniqueChars funcName = DecodeName(d);
    if (!funcName)
        return d.fail("expected valid import func name");

    uint32_t importKind;
    if (!d.readVarU32(&importKind))
        return d.fail("failed to read import kind");

    switch (DefinitionKind(importKind)) {
      case DefinitionKind::Function: {
        const SigWithId* sig = nullptr;
        if (!DecodeSignatureIndex(d, *init, &sig))
            return false;
        if (!init->funcImports.emplaceBack(sig))
            return false;
        break;
      }
      case DefinitionKind::Table: {
        if (!DecodeResizableTable(d, init))
            return false;
        break;
      }
      case DefinitionKind::Memory: {
        if (!DecodeMemoryLimits(d, init))
            return false;
        break;
      }
      case DefinitionKind::Global: {
        ValType type;
        bool isMutable;
        if (!DecodeGlobalType(d, &type, &isMutable))
            return false;
        if (!GlobalIsJSCompatible(d, type, isMutable))
            return false;
        if (!init->globals.append(GlobalDesc(type, isMutable, init->globals.length())))
            return false;
        break;
      }
      default:
        return d.fail("unsupported import kind");
    }

    return imports->emplaceBack(Move(moduleName), Move(funcName), DefinitionKind(importKind));
}

static bool
DecodeImportSection(Decoder& d, ModuleGeneratorData* init, ImportVector* imports)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Import, &sectionStart, &sectionSize, "import"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numImports;
    if (!d.readVarU32(&numImports))
        return d.fail("failed to read number of imports");

    if (numImports > MaxImports)
        return d.fail("too many imports");

    for (uint32_t i = 0; i < numImports; i++) {
        if (!DecodeImport(d, init, imports))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "import"))
        return false;

    return true;
}

static bool
DecodeTableSection(Decoder& d, ModuleGeneratorData* init, Uint32Vector* oldElems)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Table, &sectionStart, &sectionSize, "table"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numTables;
    if (!d.readVarU32(&numTables))
        return d.fail("failed to read number of tables");

    if (numTables != 1)
        return d.fail("the number of tables must be exactly one");

    if (!DecodeResizableTable(d, init))
        return false;

    if (!d.finishSection(sectionStart, sectionSize, "table"))
        return false;

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
DecodeGlobalSection(Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Global, &sectionStart, &sectionSize, "global"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numGlobals;
    if (!d.readVarU32(&numGlobals))
        return d.fail("expected number of globals");

    if (numGlobals > MaxGlobals)
        return d.fail("too many globals");

    for (uint32_t i = 0; i < numGlobals; i++) {
        ValType type;
        bool isMutable;
        if (!DecodeGlobalType(d, &type, &isMutable))
            return false;

        InitExpr initializer;
        if (!DecodeInitializerExpression(d, init->globals, type, &initializer))
            return false;

        if (!init->globals.append(GlobalDesc(initializer, isMutable)))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "global"))
        return false;

    return true;
}

typedef HashSet<const char*, CStringHasher, SystemAllocPolicy> CStringSet;

static UniqueChars
DecodeExportName(Decoder& d, CStringSet* dupSet)
{
    UniqueChars exportName = DecodeName(d);
    if (!exportName) {
        d.fail("expected valid export name");
        return nullptr;
    }

    CStringSet::AddPtr p = dupSet->lookupForAdd(exportName.get());
    if (p) {
        d.fail("duplicate export");
        return nullptr;
    }

    if (!dupSet->add(p, exportName.get()))
        return nullptr;

    return Move(exportName);
}

static bool
DecodeExport(Decoder& d, ModuleGenerator& mg, CStringSet* dupSet)
{
    UniqueChars fieldName = DecodeExportName(d, dupSet);
    if (!fieldName)
        return false;

    uint32_t exportKind;
    if (!d.readVarU32(&exportKind))
        return d.fail("failed to read export kind");

    switch (DefinitionKind(exportKind)) {
      case DefinitionKind::Function: {
        uint32_t funcIndex;
        if (!d.readVarU32(&funcIndex))
            return d.fail("expected export internal index");

        if (funcIndex >= mg.numFuncs())
            return d.fail("exported function index out of bounds");

        return mg.addFuncDefExport(Move(fieldName), funcIndex);
      }
      case DefinitionKind::Table: {
        uint32_t tableIndex;
        if (!d.readVarU32(&tableIndex))
            return d.fail("expected table index");

        if (tableIndex >= mg.tables().length())
            return d.fail("exported table index out of bounds");

        return mg.addTableExport(Move(fieldName));
      }
      case DefinitionKind::Memory: {
        uint32_t memoryIndex;
        if (!d.readVarU32(&memoryIndex))
            return d.fail("expected memory index");

        if (memoryIndex > 0 || !mg.usesMemory())
            return d.fail("exported memory index out of bounds");

        return mg.addMemoryExport(Move(fieldName));
      }
      case DefinitionKind::Global: {
        uint32_t globalIndex;
        if (!d.readVarU32(&globalIndex))
            return d.fail("expected global index");

        if (globalIndex >= mg.globals().length())
            return d.fail("exported global index out of bounds");

        const GlobalDesc& global = mg.globals()[globalIndex];
        if (!GlobalIsJSCompatible(d, global.type(), global.isMutable()))
            return false;

        return mg.addGlobalExport(Move(fieldName), globalIndex);
      }
      default:
        return d.fail("unexpected export kind");
    }

    MOZ_CRASH("unreachable");
}

static bool
DecodeExportSection(Decoder& d, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Export, &sectionStart, &sectionSize, "export"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    CStringSet dupSet;
    if (!dupSet.init())
        return false;

    uint32_t numExports;
    if (!d.readVarU32(&numExports))
        return d.fail("failed to read number of exports");

    if (numExports > MaxExports)
        return d.fail("too many exports");

    for (uint32_t i = 0; i < numExports; i++) {
        if (!DecodeExport(d, mg, &dupSet))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "export"))
        return false;

    return true;
}

static bool
DecodeFunctionBody(Decoder& d, ModuleGenerator& mg, uint32_t funcDefIndex)
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
    const Sig& sig = mg.funcDefSig(funcDefIndex);
    if (!locals.appendAll(sig.args()))
        return false;

    if (!DecodeLocalEntries(d, &locals))
        return d.fail("failed decoding local entries");

    for (ValType type : locals) {
        if (!CheckValType(d, type))
            return false;
    }

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

    return mg.finishFuncDef(funcDefIndex, &fg);
}

static bool
DecodeStartSection(Decoder& d, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Start, &sectionStart, &sectionSize, "start"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t funcIndex;
    if (!d.readVarU32(&funcIndex))
        return d.fail("failed to read start func index");

    if (funcIndex >= mg.numFuncs())
        return d.fail("unknown start function");

    const Sig& sig = mg.funcSig(funcIndex);
    if (!IsVoid(sig.ret()))
        return d.fail("start function must not return anything");

    if (sig.args().length())
        return d.fail("start function must be nullary");

    if (!mg.setStartFunction(funcIndex))
        return false;

    if (!d.finishSection(sectionStart, sectionSize, "start"))
        return false;

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

    for (uint32_t i = 0; i < numFuncDefs; i++) {
        if (!DecodeFunctionBody(d, mg, i))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "code"))
        return false;

    return mg.finishFuncDefs();
}

static bool
DecodeElemSection(Decoder& d, Uint32Vector&& oldElems, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Elem, &sectionStart, &sectionSize, "elem"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSegments;
    if (!d.readVarU32(&numSegments))
        return d.fail("failed to read number of elem segments");

    if (numSegments > MaxElemSegments)
        return d.fail("too many elem segments");

    for (uint32_t i = 0; i < numSegments; i++) {
        uint32_t tableIndex;
        if (!d.readVarU32(&tableIndex))
            return d.fail("expected table index");

        MOZ_ASSERT(mg.tables().length() <= 1);
        if (tableIndex >= mg.tables().length())
            return d.fail("table index out of range");

        InitExpr offset;
        if (!DecodeInitializerExpression(d, mg.globals(), ValType::I32, &offset))
            return false;

        uint32_t numElems;
        if (!d.readVarU32(&numElems))
            return d.fail("expected segment size");

        Uint32Vector elemFuncIndices;
        if (!elemFuncIndices.resize(numElems))
            return false;

        for (uint32_t i = 0; i < numElems; i++) {
            if (!d.readVarU32(&elemFuncIndices[i]))
                return d.fail("failed to read element function index");
            if (elemFuncIndices[i] >= mg.numFuncs())
                return d.fail("table element out of range");
        }

        if (!mg.addElemSegment(offset, Move(elemFuncIndices)))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "elem"))
        return false;

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
DecodeDataSection(Decoder& d, ModuleGenerator& mg)
{
    DataSegmentVector dataSegments;
    if (!DecodeDataSection(d, mg.usesMemory(), mg.minMemoryLength(), mg.globals(), &dataSegments))
        return false;

    mg.setDataSegments(Move(dataSegments));
    return true;
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

    if (!DecodeTypeSection(d, init.get()))
        return nullptr;

    ImportVector imports;
    if (!DecodeImportSection(d, init.get(), &imports))
        return nullptr;

    if (!DecodeFunctionSection(d, init.get()))
        return nullptr;

    Uint32Vector oldElems;
    if (!DecodeTableSection(d, init.get(), &oldElems))
        return nullptr;

    if (!::DecodeMemorySection(d, init.get()))
        return nullptr;

    if (!DecodeGlobalSection(d, init.get()))
        return nullptr;

    ModuleGenerator mg(Move(imports));
    if (!mg.init(Move(init), args))
        return nullptr;

    if (!DecodeExportSection(d, mg))
        return nullptr;

    if (!DecodeStartSection(d, mg))
        return nullptr;

    if (!DecodeElemSection(d, Move(oldElems), mg))
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

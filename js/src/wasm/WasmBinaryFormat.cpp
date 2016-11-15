/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2016 Mozilla Foundation
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

#include "wasm/WasmBinaryFormat.h"

#include "mozilla/CheckedInt.h"

#include "jsprf.h"

#include "jit/JitOptions.h"

using namespace js;
using namespace js::wasm;

using mozilla::CheckedInt;

bool
wasm::DecodePreamble(Decoder& d)
{
    uint32_t u32;
    if (!d.readFixedU32(&u32) || u32 != MagicNumber)
        return d.fail("failed to match magic number");

    if (!d.readFixedU32(&u32) || u32 != EncodingVersion)
        return d.fail("binary version 0x%" PRIx32 " does not match expected version 0x%" PRIx32,
                      u32, EncodingVersion);

    return true;
}

static bool
DecodeValType(Decoder& d, ModuleKind kind, ValType* type)
{
    uint8_t unchecked;
    if (!d.readValType(&unchecked))
        return false;

    switch (unchecked) {
      case uint8_t(ValType::I32):
      case uint8_t(ValType::F32):
      case uint8_t(ValType::F64):
      case uint8_t(ValType::I64):
        *type = ValType(unchecked);
        return true;
      case uint8_t(ValType::I8x16):
      case uint8_t(ValType::I16x8):
      case uint8_t(ValType::I32x4):
      case uint8_t(ValType::F32x4):
      case uint8_t(ValType::B8x16):
      case uint8_t(ValType::B16x8):
      case uint8_t(ValType::B32x4):
        if (kind != ModuleKind::AsmJS)
            return d.fail("bad type");
        *type = ValType(unchecked);
        return true;
      default:
        break;
    }
    return d.fail("bad type");
}

bool
wasm::DecodeTypeSection(Decoder& d, SigWithIdVector* sigs)
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

    if (!sigs->resize(numSigs))
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
            if (!DecodeValType(d, ModuleKind::Wasm, &args[i]))
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
            if (!DecodeValType(d, ModuleKind::Wasm, &type))
                return false;

            result = ToExprType(type);
        }

        (*sigs)[sigIndex] = Sig(Move(args), result);
    }

    if (!d.finishSection(sectionStart, sectionSize, "type"))
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
DecodeSignatureIndex(Decoder& d, const SigWithIdVector& sigs, uint32_t* sigIndex)
{
    if (!d.readVarU32(sigIndex))
        return d.fail("expected signature index");

    if (*sigIndex >= sigs.length())
        return d.fail("signature index out of range");

    return true;
}

static bool
DecodeLimits(Decoder& d, Limits* limits)
{
    uint32_t flags;
    if (!d.readVarU32(&flags))
        return d.fail("expected flags");

    if (flags & ~uint32_t(0x1))
        return d.fail("unexpected bits set in flags: %" PRIu32, (flags & ~uint32_t(0x1)));

    if (!d.readVarU32(&limits->initial))
        return d.fail("expected initial length");

    if (flags & 0x1) {
        uint32_t maximum;
        if (!d.readVarU32(&maximum))
            return d.fail("expected maximum length");

        if (limits->initial > maximum) {
            return d.fail("memory size minimum must not be greater than maximum; "
                          "maximum length %" PRIu32 " is less than initial length %" PRIu32,
                          maximum, limits->initial);
        }

        limits->maximum.emplace(maximum);
    }

    return true;
}

static bool
DecodeTableLimits(Decoder& d, TableDescVector* tables)
{
    uint32_t elementType;
    if (!d.readVarU32(&elementType))
        return d.fail("expected table element type");

    if (elementType != uint32_t(TypeCode::AnyFunc))
        return d.fail("expected 'anyfunc' element type");

    Limits limits;
    if (!DecodeLimits(d, &limits))
        return false;

    if (limits.initial > MaxTableElems)
        return d.fail("too many table elements");

    if (tables->length())
        return d.fail("already have default table");

    return tables->emplaceBack(TableKind::AnyFunction, limits);
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
        if (!jit::JitOptions.wasmTestMode)
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
DecodeGlobalType(Decoder& d, ValType* type, bool* isMutable)
{
    if (!DecodeValType(d, ModuleKind::Wasm, type))
        return false;

    uint32_t flags;
    if (!d.readVarU32(&flags))
        return d.fail("expected global flags");

    if (flags & ~uint32_t(GlobalFlags::AllowedMask))
        return d.fail("unexpected bits set in global flags");

    *isMutable = flags & uint32_t(GlobalFlags::IsMutable);
    return true;
}

static bool
DecodeMemoryLimits(Decoder& d, bool hasMemory, Limits* memory)
{
    if (hasMemory)
        return d.fail("already have default memory");

    if (!DecodeLimits(d, memory))
        return false;

    CheckedInt<uint32_t> initialBytes = memory->initial;
    initialBytes *= PageSize;
    if (!initialBytes.isValid() || initialBytes.value() > uint32_t(INT32_MAX))
        return d.fail("initial memory size too big");

    memory->initial = initialBytes.value();

    if (memory->maximum) {
        CheckedInt<uint32_t> maximumBytes = *memory->maximum;
        maximumBytes *= PageSize;
        if (!maximumBytes.isValid())
            return d.fail("maximum memory size too big");

        memory->maximum = Some(maximumBytes.value());
    }

    return true;
}

static bool
DecodeImport(Decoder& d, const SigWithIdVector& sigs, SigWithIdPtrVector* funcSigs,
             GlobalDescVector* globals, TableDescVector* tables, Maybe<Limits>* memory,
             ImportVector* imports)
{
    UniqueChars moduleName = DecodeName(d);
    if (!moduleName)
        return d.fail("expected valid import module name");

    UniqueChars funcName = DecodeName(d);
    if (!funcName)
        return d.fail("expected valid import func name");

    uint32_t rawImportKind;
    if (!d.readVarU32(&rawImportKind))
        return d.fail("failed to read import kind");

    DefinitionKind importKind = DefinitionKind(rawImportKind);

    switch (importKind) {
      case DefinitionKind::Function: {
        uint32_t sigIndex;
        if (!DecodeSignatureIndex(d, sigs, &sigIndex))
            return false;
        if (!funcSigs->append(&sigs[sigIndex]))
            return false;
        break;
      }
      case DefinitionKind::Table: {
        if (!DecodeTableLimits(d, tables))
            return false;
        break;
      }
      case DefinitionKind::Memory: {
        Limits limits;
        if (!DecodeMemoryLimits(d, !!*memory, &limits))
            return false;
        memory->emplace(limits);
        break;
      }
      case DefinitionKind::Global: {
        ValType type;
        bool isMutable;
        if (!DecodeGlobalType(d, &type, &isMutable))
            return false;
        if (!GlobalIsJSCompatible(d, type, isMutable))
            return false;
        if (!globals->append(GlobalDesc(type, isMutable, globals->length())))
            return false;
        break;
      }
      default:
        return d.fail("unsupported import kind");
    }

    return imports->emplaceBack(Move(moduleName), Move(funcName), importKind);
}

bool
wasm::DecodeImportSection(Decoder& d, const SigWithIdVector& sigs, SigWithIdPtrVector* funcSigs,
                          GlobalDescVector* globals, TableDescVector* tables, Maybe<Limits>* memory,
                          ImportVector* imports)
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
        if (!DecodeImport(d, sigs, funcSigs, globals, tables, memory, imports))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "import"))
        return false;

    return true;
}

bool
wasm::DecodeFunctionSection(Decoder& d, const SigWithIdVector& sigs, SigWithIdPtrVector* funcSigs)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Function, &sectionStart, &sectionSize, "function"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numDefs;
    if (!d.readVarU32(&numDefs))
        return d.fail("expected number of function definitions");

    uint32_t numFuncs = funcSigs->length() + numDefs;
    if (numFuncs > MaxFuncs)
        return d.fail("too many functions");

    if (!funcSigs->reserve(numFuncs))
        return false;

    for (uint32_t i = 0; i < numDefs; i++) {
        uint32_t sigIndex;
        if (!DecodeSignatureIndex(d, sigs, &sigIndex))
            return false;
        funcSigs->infallibleAppend(&sigs[sigIndex]);
    }

    if (!d.finishSection(sectionStart, sectionSize, "function"))
        return false;

    return true;
}

bool
wasm::DecodeTableSection(Decoder& d, TableDescVector* tables)
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

    if (!DecodeTableLimits(d, tables))
        return false;

    if (!d.finishSection(sectionStart, sectionSize, "table"))
        return false;

    return true;
}

bool
wasm::DecodeMemorySection(Decoder& d, bool hasMemory, Limits* memory, bool *present)
{
    *present = false;

    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Memory, &sectionStart, &sectionSize, "memory"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    *present = true;

    uint32_t numMemories;
    if (!d.readVarU32(&numMemories))
        return d.fail("failed to read number of memories");

    if (numMemories != 1)
        return d.fail("the number of memories must be exactly one");

    if (!DecodeMemoryLimits(d, hasMemory, memory))
        return false;

    if (!d.finishSection(sectionStart, sectionSize, "memory"))
        return false;

    return true;
}

bool
wasm::DecodeGlobalSection(Decoder& d, GlobalDescVector* globals)
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
        if (!DecodeInitializerExpression(d, *globals, type, &initializer))
            return false;

        if (!globals->append(GlobalDesc(initializer, isMutable)))
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
DecodeExport(Decoder& d, size_t numFuncs, size_t numTables, bool usesMemory,
             const GlobalDescVector& globals, ExportVector* exports, CStringSet* dupSet)
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
            return d.fail("expected function index");

        if (funcIndex >= numFuncs)
            return d.fail("exported function index out of bounds");

        return exports->emplaceBack(Move(fieldName), funcIndex, DefinitionKind::Function);
      }
      case DefinitionKind::Table: {
        uint32_t tableIndex;
        if (!d.readVarU32(&tableIndex))
            return d.fail("expected table index");

        if (tableIndex >= numTables)
            return d.fail("exported table index out of bounds");

        return exports->emplaceBack(Move(fieldName), DefinitionKind::Table);
      }
      case DefinitionKind::Memory: {
        uint32_t memoryIndex;
        if (!d.readVarU32(&memoryIndex))
            return d.fail("expected memory index");

        if (memoryIndex > 0 || !usesMemory)
            return d.fail("exported memory index out of bounds");

        return exports->emplaceBack(Move(fieldName), DefinitionKind::Memory);
      }
      case DefinitionKind::Global: {
        uint32_t globalIndex;
        if (!d.readVarU32(&globalIndex))
            return d.fail("expected global index");

        if (globalIndex >= globals.length())
            return d.fail("exported global index out of bounds");

        const GlobalDesc& global = globals[globalIndex];
        if (!GlobalIsJSCompatible(d, global.type(), global.isMutable()))
            return false;

        return exports->emplaceBack(Move(fieldName), globalIndex, DefinitionKind::Global);
      }
      default:
        return d.fail("unexpected export kind");
    }

    MOZ_CRASH("unreachable");
}

bool
wasm::DecodeExportSection(Decoder& d, size_t numFuncs, size_t numTables, bool usesMemory,
                          const GlobalDescVector& globals, ExportVector* exports)
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
        if (!DecodeExport(d, numFuncs, numTables, usesMemory, globals, exports, &dupSet))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "export"))
        return false;

    return true;
}

bool
wasm::DecodeStartSection(Decoder& d, const SigWithIdPtrVector& funcSigs,
                         Maybe<uint32_t>* startFuncIndex)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Start, &sectionStart, &sectionSize, "start"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t funcIndex;
    if (!d.readVarU32(&funcIndex))
        return d.fail("failed to read start func index");

    if (funcIndex >= funcSigs.length())
        return d.fail("unknown start function");

    const Sig& sig = *funcSigs[funcIndex];
    if (!IsVoid(sig.ret()))
        return d.fail("start function must not return anything");

    if (sig.args().length())
        return d.fail("start function must be nullary");

    *startFuncIndex = Some(funcIndex);

    if (!d.finishSection(sectionStart, sectionSize, "start"))
        return false;

    return true;
}

bool
wasm::EncodeLocalEntries(Encoder& e, const ValTypeVector& locals)
{
    uint32_t numLocalEntries = 0;
    ValType prev = ValType(TypeCode::Limit);
    for (ValType t : locals) {
        if (t != prev) {
            numLocalEntries++;
            prev = t;
        }
    }

    if (!e.writeVarU32(numLocalEntries))
        return false;

    if (numLocalEntries) {
        prev = locals[0];
        uint32_t count = 1;
        for (uint32_t i = 1; i < locals.length(); i++, count++) {
            if (prev != locals[i]) {
                if (!e.writeVarU32(count))
                    return false;
                if (!e.writeValType(prev))
                    return false;
                prev = locals[i];
                count = 0;
            }
        }
        if (!e.writeVarU32(count))
            return false;
        if (!e.writeValType(prev))
            return false;
    }

    return true;
}

bool
wasm::DecodeLocalEntries(Decoder& d, ModuleKind kind, ValTypeVector* locals)
{
    uint32_t numLocalEntries;
    if (!d.readVarU32(&numLocalEntries))
        return d.fail("failed to read number of local entries");

    for (uint32_t i = 0; i < numLocalEntries; i++) {
        uint32_t count;
        if (!d.readVarU32(&count))
            return d.fail("failed to read local entry count");

        if (MaxLocals - locals->length() < count)
            return d.fail("too many locals");

        ValType type;
        if (!DecodeValType(d, kind, &type))
            return false;

        if (!locals->appendN(type, count))
            return false;
    }

    return true;
}

bool
wasm::DecodeInitializerExpression(Decoder& d, const GlobalDescVector& globals, ValType expected,
                                  InitExpr* init)
{
    uint16_t op;
    if (!d.readOp(&op))
        return d.fail("failed to read initializer type");

    switch (op) {
      case uint16_t(Op::I32Const): {
        int32_t i32;
        if (!d.readVarS32(&i32))
            return d.fail("failed to read initializer i32 expression");
        *init = InitExpr(Val(uint32_t(i32)));
        break;
      }
      case uint16_t(Op::I64Const): {
        int64_t i64;
        if (!d.readVarS64(&i64))
            return d.fail("failed to read initializer i64 expression");
        *init = InitExpr(Val(uint64_t(i64)));
        break;
      }
      case uint16_t(Op::F32Const): {
        RawF32 f32;
        if (!d.readFixedF32(&f32))
            return d.fail("failed to read initializer f32 expression");
        *init = InitExpr(Val(f32));
        break;
      }
      case uint16_t(Op::F64Const): {
        RawF64 f64;
        if (!d.readFixedF64(&f64))
            return d.fail("failed to read initializer f64 expression");
        *init = InitExpr(Val(f64));
        break;
      }
      case uint16_t(Op::GetGlobal): {
        uint32_t i;
        if (!d.readVarU32(&i))
            return d.fail("failed to read get_global index in initializer expression");
        if (i >= globals.length())
            return d.fail("global index out of range in initializer expression");
        if (!globals[i].isImport() || globals[i].isMutable())
            return d.fail("initializer expression must reference a global immutable import");
        *init = InitExpr(i, globals[i].type());
        break;
      }
      default: {
        return d.fail("unexpected initializer expression");
      }
    }

    if (expected != init->type())
        return d.fail("type mismatch: initializer type and expected type don't match");

    uint16_t end;
    if (!d.readOp(&end) || end != uint16_t(Op::End))
        return d.fail("failed to read end of initializer expression");

    return true;
}

bool
wasm::DecodeDataSection(Decoder& d, bool usesMemory, uint32_t minMemoryByteLength,
                        const GlobalDescVector& globals, DataSegmentVector* segments)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Data, &sectionStart, &sectionSize, "data"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    if (!usesMemory)
        return d.fail("data section requires a memory section");

    uint32_t numSegments;
    if (!d.readVarU32(&numSegments))
        return d.fail("failed to read number of data segments");

    if (numSegments > MaxDataSegments)
        return d.fail("too many data segments");

    for (uint32_t i = 0; i < numSegments; i++) {
        uint32_t linearMemoryIndex;
        if (!d.readVarU32(&linearMemoryIndex))
            return d.fail("expected linear memory index");

        if (linearMemoryIndex != 0)
            return d.fail("linear memory index must currently be 0");

        DataSegment seg;
        if (!DecodeInitializerExpression(d, globals, ValType::I32, &seg.offset))
            return false;

        if (!d.readVarU32(&seg.length))
            return d.fail("expected segment size");

        seg.bytecodeOffset = d.currentOffset();

        if (!d.readBytes(seg.length))
            return d.fail("data segment shorter than declared");

        if (!segments->append(seg))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "data"))
        return false;

    return true;
}

bool
wasm::DecodeUnknownSections(Decoder& d)
{
    while (!d.done()) {
        if (!d.skipUserDefinedSection())
            return false;
    }

    return true;
}

bool
Decoder::fail(const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    UniqueChars str(JS_vsmprintf(msg, ap));
    va_end(ap);
    if (!str)
        return false;

    return fail(Move(str));
}

bool
Decoder::fail(UniqueChars msg)
{
    MOZ_ASSERT(error_);
    UniqueChars strWithOffset(JS_smprintf("at offset %" PRIuSIZE ": %s", currentOffset(), msg.get()));
    if (!strWithOffset)
        return false;

    *error_ = Move(strWithOffset);
    return false;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2018 Mozilla Foundation
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

#include "wasm/WasmCraneliftCompile.h"

#include "js/Printf.h"

#include "wasm/cranelift/baldrapi.h"
#include "wasm/cranelift/clifapi.h"
#include "wasm/WasmGenerator.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

static inline SymbolicAddress
ToSymbolicAddress(BD_SymbolicAddress bd)
{
    switch (bd) {
      case BD_SymbolicAddress::GrowMemory:    return SymbolicAddress::GrowMemory;
      case BD_SymbolicAddress::CurrentMemory: return SymbolicAddress::CurrentMemory;
      case BD_SymbolicAddress::FloorF32:      return SymbolicAddress::FloorF;
      case BD_SymbolicAddress::FloorF64:      return SymbolicAddress::FloorD;
      case BD_SymbolicAddress::CeilF32:       return SymbolicAddress::CeilF;
      case BD_SymbolicAddress::CeilF64:       return SymbolicAddress::CeilD;
      case BD_SymbolicAddress::NearestF32:    return SymbolicAddress::NearbyIntF;
      case BD_SymbolicAddress::NearestF64:    return SymbolicAddress::NearbyIntD;
      case BD_SymbolicAddress::TruncF32:      return SymbolicAddress::TruncF;
      case BD_SymbolicAddress::TruncF64:      return SymbolicAddress::TruncD;
      case BD_SymbolicAddress::Limit:         break;
    }
    MOZ_CRASH("unknown baldrdash symbolic address");
}

static bool
GenerateCraneliftCode(WasmMacroAssembler& masm, const CraneliftCompiledFunc& func,
                      const FuncTypeIdDesc& funcTypeId, uint32_t lineOrBytecode,
                      FuncOffsets* offsets)
{
    wasm::GenerateFunctionPrologue(masm, funcTypeId, mozilla::Nothing(), offsets);

    // Omit the check when framePushed is small and we know there's no
    // recursion.
    if (func.framePushed < MAX_UNCHECKED_LEAF_FRAME_SIZE && !func.containsCalls)
        masm.reserveStack(func.framePushed);
    else
        masm.wasmReserveStackChecked(func.framePushed, BytecodeOffset(lineOrBytecode));
    MOZ_ASSERT(masm.framePushed() == func.framePushed);

    uint32_t funcBase = masm.currentOffset();
    if (!masm.appendRawCode(func.code, func.codeSize))
        return false;

    wasm::GenerateFunctionEpilogue(masm, func.framePushed, offsets);

    masm.flush();
    if (masm.oom())
        return false;
    offsets->end = masm.currentOffset();

    for (size_t i = 0; i < func.numMetadata; i++) {
        const CraneliftMetadataEntry& metadata = func.metadatas[i];

        CheckedInt<size_t> offset = funcBase;
        offset += metadata.offset;
        if (!offset.isValid())
            return false;

#ifdef DEBUG
        CheckedInt<uint32_t> checkedBytecodeOffset = lineOrBytecode;
        checkedBytecodeOffset += metadata.srcLoc;
        MOZ_ASSERT(checkedBytecodeOffset.isValid());
#endif
        uint32_t bytecodeOffset = lineOrBytecode + metadata.srcLoc;

        switch (metadata.which) {
          case CraneliftMetadataEntry::Which::DirectCall: {
            CallSiteDesc desc(bytecodeOffset, CallSiteDesc::Func);
            masm.append(desc, CodeOffset(offset.value()), metadata.extra);
            break;
          }
          case CraneliftMetadataEntry::Which::IndirectCall: {
            CallSiteDesc desc(bytecodeOffset, CallSiteDesc::Dynamic);
            masm.append(desc, CodeOffset(offset.value()));
            break;
          }
          case CraneliftMetadataEntry::Which::Trap: {
            Trap trap = (Trap)metadata.extra;
            BytecodeOffset trapOffset(bytecodeOffset);
            masm.append(trap, wasm::TrapSite(offset.value(), trapOffset));
            break;
          }
          case CraneliftMetadataEntry::Which::MemoryAccess: {
            BytecodeOffset trapOffset(bytecodeOffset);
            masm.appendOutOfBoundsTrap(trapOffset, offset.value());
            break;
          }
          case CraneliftMetadataEntry::Which::SymbolicAccess: {
            SymbolicAddress sym = ToSymbolicAddress(BD_SymbolicAddress(metadata.extra));
            masm.append(SymbolicAccess(CodeOffset(offset.value()), sym));
            break;
          }
          default: {
            MOZ_CRASH("unknown cranelift metadata kind");
          }
        }
    }

    return true;
}

// In Rust, a BatchCompiler variable has a lifetime constrained by those of its
// associated StaticEnvironment and ModuleEnvironment. This RAII class ties
// them together, as well as makes sure that the compiler is properly destroyed
// when it exits scope.

class AutoCranelift
{
    CraneliftStaticEnvironment staticEnv_;
    CraneliftModuleEnvironment env_;
    CraneliftCompiler* compiler_;
  public:
    explicit AutoCranelift(const ModuleEnvironment& env)
      : env_(env),
        compiler_(nullptr)
    {}
    bool init() {
        compiler_ = cranelift_compiler_create(&staticEnv_, &env_);
        return !!compiler_;
    }
    ~AutoCranelift() {
        if (compiler_)
            cranelift_compiler_destroy(compiler_);
    }
    operator CraneliftCompiler*() { return compiler_; }
};

CraneliftFuncCompileInput::CraneliftFuncCompileInput(const FuncCompileInput& func)
  : bytecode(func.begin),
    bytecodeSize(func.end - func.begin),
    index(func.index)
{}

#ifndef WASM_HUGE_MEMORY
static_assert(offsetof(TlsData, boundsCheckLimit) == sizeof(size_t),
              "fix make_heap() in wasm2clif.rs");
#endif

CraneliftStaticEnvironment::CraneliftStaticEnvironment()
  :
#ifdef JS_CODEGEN_X64
    hasSse2(Assembler::HasSSE2()),
    hasSse3(Assembler::HasSSE3()),
    hasSse41(Assembler::HasSSE41()),
    hasSse42(Assembler::HasSSE42()),
    hasPopcnt(Assembler::HasPOPCNT()),
    hasAvx(Assembler::HasAVX()),
#else
    hasSse2(false),
    hasSse3(false),
    hasSse41(false),
    hasSse42(false),
    hasPopcnt(false),
    hasAvx(false),
#endif
    hasBmi1(false), // TODO implement feature detection for bmi1
    hasBmi2(false), // TODO implement feature detection for bmi2
    hasLzcnt(false), // TODO implement feature detection for lzcnt
    staticMemoryBound(
#ifdef WASM_HUGE_MEMORY
        // In the huge memory configuration, we always reserve the full 4 GB index
        // space for a heap.
        IndexRange
#else
        // Otherwise, heap bounds are stored in the `boundsCheckLimit` field of
        // TlsData.
        0
#endif
    ),
    memoryGuardSize(OffsetGuardLimit),
    instanceTlsOffset(offsetof(TlsData, instance)),
    interruptTlsOffset(offsetof(TlsData, interrupt)),
    cxTlsOffset(offsetof(TlsData, cx)),
    realmCxOffset(JSContext::offsetOfRealm()),
    realmTlsOffset(offsetof(TlsData, realm)),
    realmFuncImportTlsOffset(offsetof(FuncImportTls, realm))
{
}

// Most of BaldrMonkey's data structures refer to a "global offset" which is a
// byte offset into the `globalArea` field of the  `TlsData` struct.
//
// Cranelift represents global variables with their byte offset from the "VM
// context pointer" which is the `WasmTlsReg` pointing to the `TlsData` struct.
//
// This function translates between the two.

static size_t
globalToTlsOffset(size_t globalOffset)
{
    return offsetof(wasm::TlsData, globalArea) + globalOffset;
}

CraneliftModuleEnvironment::CraneliftModuleEnvironment(const ModuleEnvironment& env)
  : env(env),
    min_memory_length(env.minMemoryLength)
{
}

TypeCode
env_unpack(BD_ValType valType)
{
    return TypeCode(UnpackTypeCodeType(PackedTypeCode(valType.packed)));
}

const FuncTypeWithId*
env_function_signature(const CraneliftModuleEnvironment* env, size_t funcIndex)
{
    return env->env.funcTypes[funcIndex];
}

size_t
env_func_import_tls_offset(const CraneliftModuleEnvironment* env, size_t funcIndex)
{
    return globalToTlsOffset(env->env.funcImportGlobalDataOffsets[funcIndex]);
}

bool
env_func_is_import(const CraneliftModuleEnvironment* env, size_t funcIndex)
{
    return env->env.funcIsImport(funcIndex);
}

const FuncTypeWithId*
env_signature(const CraneliftModuleEnvironment* env, size_t funcTypeIndex)
{
    return &env->env.types[funcTypeIndex].funcType();
}

const TableDesc*
env_table(const CraneliftModuleEnvironment* env, size_t tableIndex)
{
    return &env->env.tables[tableIndex];
}

const GlobalDesc*
env_global(const CraneliftModuleEnvironment* env, size_t globalIndex)
{
    return &env->env.globals[globalIndex];
}

bool
wasm::CraneliftCompileFunctions(const ModuleEnvironment& env,
                                LifoAlloc& lifo,
                                const FuncCompileInputVector& inputs,
                                CompiledCode* code,
                                UniqueChars* error)
{
    MOZ_ASSERT(env.tier() == Tier::Optimized);
    MOZ_ASSERT(env.optimizedBackend() == OptimizedBackend::Cranelift);

    AutoCranelift compiler(env);
    if (!compiler.init())
        return false;

    TempAllocator alloc(&lifo);
    JitContext jitContext(&alloc);
    WasmMacroAssembler masm(alloc);
    MOZ_ASSERT(IsCompilingWasm());

    // Swap in already-allocated empty vectors to avoid malloc/free.
    MOZ_ASSERT(code->empty());
    if (!code->swap(masm))
        return false;

    for (const FuncCompileInput& func : inputs) {
        CraneliftFuncCompileInput clifInput(func);

        CraneliftCompiledFunc clifFunc;
        if (!cranelift_compile_function(compiler, &clifInput, &clifFunc)) {
            *error = JS_smprintf("Cranelift error in clifFunc #%u", clifInput.index);
            return false;
        }

        uint32_t lineOrBytecode = func.lineOrBytecode;
        const FuncTypeIdDesc& funcTypeId = env.funcTypes[clifInput.index]->id;

        FuncOffsets offsets;
        if (!GenerateCraneliftCode(masm, clifFunc, funcTypeId, lineOrBytecode, &offsets))
            return false;

        if (!code->codeRanges.emplaceBack(func.index, func.lineOrBytecode, offsets))
            return false;
    }

    masm.finish();
    if (masm.oom())
        return false;

    return code->swap(masm);
}

////////////////////////////////////////////////////////////////////////////////
//
// Callbacks from Rust to C++.

// Offsets assumed by the `make_heap()` function.
static_assert(offsetof(wasm::TlsData, memoryBase) == 0, "memory base moved");

// The translate_call() function in wasm2clif.rs depends on these offsets.
static_assert(offsetof(wasm::FuncImportTls, code) == 0, "Import code field moved");
static_assert(offsetof(wasm::FuncImportTls, tls) == sizeof(void*), "Import tls moved");

// Global

bool
global_isConstant(const GlobalDesc* global)
{
    return global->isConstant();
}

bool
global_isIndirect(const GlobalDesc* global)
{
    return global->isIndirect();
}

BD_ConstantValue
global_constantValue(const GlobalDesc* global)
{
    Val value(global->constantValue());
    BD_ConstantValue v;
    v.t = TypeCode(value.type().code());
    switch (v.t) {
      case TypeCode::I32:
        v.u.i32 = value.i32();
        break;
      case TypeCode::I64:
        v.u.i64 = value.i64();
        break;
      case TypeCode::F32:
        v.u.f32 = value.f32();
        break;
      case TypeCode::F64:
        v.u.f64 = value.f64();
        break;
      default:
        MOZ_CRASH("Bad type");
    }
    return v;
}

#ifdef DEBUG
static bool
IsCraneliftCompatible(TypeCode type)
{
    switch (type) {
      case TypeCode::I32:
      case TypeCode::I64:
      case TypeCode::F32:
      case TypeCode::F64:
        return true;
      default:
        return false;
    }
}
#endif

TypeCode
global_type(const GlobalDesc* global)
{
    TypeCode type = TypeCode(global->type().code());
    MOZ_ASSERT(IsCraneliftCompatible(type));
    return type;
}

size_t
global_tlsOffset(const GlobalDesc* global)
{
    return globalToTlsOffset(global->offset());
}

// TableDesc

size_t
table_tlsOffset(const TableDesc* table)
{
    return globalToTlsOffset(table->globalDataOffset);
}

bool
table_isExternal(const TableDesc* table)
{
    return table->external;
}

// Sig

size_t
funcType_numArgs(const FuncTypeWithId* funcType)
{
    return funcType->args().length();
}

const BD_ValType*
funcType_args(const FuncTypeWithId* funcType)
{
#ifdef DEBUG
    for (ValType valType : funcType->args())
        MOZ_ASSERT(IsCraneliftCompatible(TypeCode(valType.code())));
#endif
    static_assert(sizeof(BD_ValType) == sizeof(ValType), "update BD_ValType");
    return (const BD_ValType*)&funcType->args()[0];
}

TypeCode
funcType_retType(const FuncTypeWithId* funcType)
{
    return TypeCode(funcType->ret().code());
}

FuncTypeIdDescKind
funcType_idKind(const FuncTypeWithId* funcType)
{
    return funcType->id.kind();
}

size_t
funcType_idImmediate(const FuncTypeWithId* funcType)
{
    return funcType->id.immediate();
}

size_t
funcType_idTlsOffset(const FuncTypeWithId* funcType)
{
    return globalToTlsOffset(funcType->id.globalDataOffset());
}

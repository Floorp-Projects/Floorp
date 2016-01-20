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

#include "asmjs/AsmJS.h"

#include "mozilla/Attributes.h"
#include "mozilla/Compression.h"
#include "mozilla/MathAlgorithms.h"

#include "jsmath.h"
#include "jsprf.h"
#include "jsstr.h"
#include "jsutil.h"

#include "jswrapper.h"

#include "asmjs/WasmGenerator.h"
#include "asmjs/WasmSerialize.h"
#include "builtin/SIMD.h"
#include "frontend/Parser.h"
#include "jit/AtomicOperations.h"
#include "jit/MIR.h"
#include "js/Class.h"
#include "js/MemoryMetrics.h"
#include "vm/StringBuffer.h"
#include "vm/Time.h"
#include "vm/TypedArrayObject.h"

#include "jsobjinlines.h"

#include "frontend/ParseNode-inl.h"
#include "frontend/Parser-inl.h"
#include "vm/ArrayBufferObject-inl.h"

using namespace js;
using namespace js::frontend;
using namespace js::jit;
using namespace js::wasm;

using mozilla::Compression::LZ4;
using mozilla::HashGeneric;
using mozilla::IsNaN;
using mozilla::IsNegativeZero;
using mozilla::Maybe;
using mozilla::Move;
using mozilla::PodCopy;
using mozilla::PodEqual;
using mozilla::PodZero;
using mozilla::PositiveInfinity;
using JS::AsmJSOption;
using JS::GenericNaN;

/*****************************************************************************/
// asm.js module object

typedef PropertyName* PropertyNamePtr;

static void
TraceNameField(JSTracer* trc, const PropertyNamePtr* name, const char* label)
{
    if (*name)
        TraceManuallyBarrieredEdge(trc, const_cast<PropertyNamePtr*>(name), label);
}

// The asm.js spec recognizes this set of builtin Math functions.
enum AsmJSMathBuiltinFunction
{
    AsmJSMathBuiltin_sin, AsmJSMathBuiltin_cos, AsmJSMathBuiltin_tan,
    AsmJSMathBuiltin_asin, AsmJSMathBuiltin_acos, AsmJSMathBuiltin_atan,
    AsmJSMathBuiltin_ceil, AsmJSMathBuiltin_floor, AsmJSMathBuiltin_exp,
    AsmJSMathBuiltin_log, AsmJSMathBuiltin_pow, AsmJSMathBuiltin_sqrt,
    AsmJSMathBuiltin_abs, AsmJSMathBuiltin_atan2, AsmJSMathBuiltin_imul,
    AsmJSMathBuiltin_fround, AsmJSMathBuiltin_min, AsmJSMathBuiltin_max,
    AsmJSMathBuiltin_clz32
};

// The asm.js spec will recognize this set of builtin Atomics functions.
enum AsmJSAtomicsBuiltinFunction
{
    AsmJSAtomicsBuiltin_compareExchange,
    AsmJSAtomicsBuiltin_exchange,
    AsmJSAtomicsBuiltin_load,
    AsmJSAtomicsBuiltin_store,
    AsmJSAtomicsBuiltin_fence,
    AsmJSAtomicsBuiltin_add,
    AsmJSAtomicsBuiltin_sub,
    AsmJSAtomicsBuiltin_and,
    AsmJSAtomicsBuiltin_or,
    AsmJSAtomicsBuiltin_xor,
    AsmJSAtomicsBuiltin_isLockFree
};

// Set of known global object SIMD's attributes, i.e. types
enum AsmJSSimdType
{
    AsmJSSimdType_int32x4,
    AsmJSSimdType_float32x4,
    AsmJSSimdType_bool32x4
};

static inline bool
IsSignedIntSimdType(AsmJSSimdType type)
{
    switch (type) {
      case AsmJSSimdType_int32x4:
        return true;
      case AsmJSSimdType_float32x4:
      case AsmJSSimdType_bool32x4:
        return false;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unknown SIMD type");
}

// Set of known operations, for a given SIMD type (int32x4, float32x4,...)
enum AsmJSSimdOperation
{
#define ASMJSSIMDOPERATION(op) AsmJSSimdOperation_##op,
    FORALL_SIMD_ASMJS_OP(ASMJSSIMDOPERATION)
#undef ASMJSSIMDOPERATION
};

// An AsmJSGlobal represents a JS global variable in the asm.js module function.
class AsmJSGlobal
{
  public:
    enum Which { Variable, FFI, ArrayView, ArrayViewCtor, MathBuiltinFunction,
                 AtomicsBuiltinFunction, Constant, SimdCtor, SimdOperation };
    enum VarInitKind { InitConstant, InitImport };
    enum ConstantKind { GlobalConstant, MathConstant };

  private:
    struct CacheablePod {
        Which which_;
        union {
            struct {
                uint32_t globalDataOffset_;
                VarInitKind initKind_;
                union {
                    ValType importType_;
                    Val val_;
                } u;
            } var;
            uint32_t ffiIndex_;
            Scalar::Type viewType_;
            AsmJSMathBuiltinFunction mathBuiltinFunc_;
            AsmJSAtomicsBuiltinFunction atomicsBuiltinFunc_;
            AsmJSSimdType simdCtorType_;
            struct {
                AsmJSSimdType type_;
                AsmJSSimdOperation which_;
            } simdOp;
            struct {
                ConstantKind kind_;
                double value_;
            } constant;
        } u;
    } pod;
    PropertyName* name_;

    friend class ModuleValidator;

  public:
    AsmJSGlobal() {}
    AsmJSGlobal(Which which, PropertyName* name) {
        mozilla::PodZero(&pod);  // zero padding for Valgrind
        pod.which_ = which;
        name_ = name;
        MOZ_ASSERT_IF(name_, name_->isTenured());
    }
    void trace(JSTracer* trc) const {
        TraceNameField(trc, &name_, "asm.js global name");
    }
    Which which() const {
        return pod.which_;
    }
    uint32_t varGlobalDataOffset() const {
        MOZ_ASSERT(pod.which_ == Variable);
        return pod.u.var.globalDataOffset_;
    }
    VarInitKind varInitKind() const {
        MOZ_ASSERT(pod.which_ == Variable);
        return pod.u.var.initKind_;
    }
    Val varInitVal() const {
        MOZ_ASSERT(pod.which_ == Variable);
        MOZ_ASSERT(pod.u.var.initKind_ == InitConstant);
        return pod.u.var.u.val_;
    }
    ValType varInitImportType() const {
        MOZ_ASSERT(pod.which_ == Variable);
        MOZ_ASSERT(pod.u.var.initKind_ == InitImport);
        return pod.u.var.u.importType_;
    }
    PropertyName* varImportField() const {
        MOZ_ASSERT(pod.which_ == Variable);
        MOZ_ASSERT(pod.u.var.initKind_ == InitImport);
        return name_;
    }
    PropertyName* ffiField() const {
        MOZ_ASSERT(pod.which_ == FFI);
        return name_;
    }
    uint32_t ffiIndex() const {
        MOZ_ASSERT(pod.which_ == FFI);
        return pod.u.ffiIndex_;
    }
    // When a view is created from an imported constructor:
    //   var I32 = stdlib.Int32Array;
    //   var i32 = new I32(buffer);
    // the second import has nothing to validate and thus has a null field.
    PropertyName* maybeViewName() const {
        MOZ_ASSERT(pod.which_ == ArrayView || pod.which_ == ArrayViewCtor);
        return name_;
    }
    Scalar::Type viewType() const {
        MOZ_ASSERT(pod.which_ == ArrayView || pod.which_ == ArrayViewCtor);
        return pod.u.viewType_;
    }
    PropertyName* mathName() const {
        MOZ_ASSERT(pod.which_ == MathBuiltinFunction);
        return name_;
    }
    PropertyName* atomicsName() const {
        MOZ_ASSERT(pod.which_ == AtomicsBuiltinFunction);
        return name_;
    }
    AsmJSMathBuiltinFunction mathBuiltinFunction() const {
        MOZ_ASSERT(pod.which_ == MathBuiltinFunction);
        return pod.u.mathBuiltinFunc_;
    }
    AsmJSAtomicsBuiltinFunction atomicsBuiltinFunction() const {
        MOZ_ASSERT(pod.which_ == AtomicsBuiltinFunction);
        return pod.u.atomicsBuiltinFunc_;
    }
    AsmJSSimdType simdCtorType() const {
        MOZ_ASSERT(pod.which_ == SimdCtor);
        return pod.u.simdCtorType_;
    }
    PropertyName* simdCtorName() const {
        MOZ_ASSERT(pod.which_ == SimdCtor);
        return name_;
    }
    PropertyName* simdOperationName() const {
        MOZ_ASSERT(pod.which_ == SimdOperation);
        return name_;
    }
    AsmJSSimdOperation simdOperation() const {
        MOZ_ASSERT(pod.which_ == SimdOperation);
        return pod.u.simdOp.which_;
    }
    AsmJSSimdType simdOperationType() const {
        MOZ_ASSERT(pod.which_ == SimdOperation);
        return pod.u.simdOp.type_;
    }
    PropertyName* constantName() const {
        MOZ_ASSERT(pod.which_ == Constant);
        return name_;
    }
    ConstantKind constantKind() const {
        MOZ_ASSERT(pod.which_ == Constant);
        return pod.u.constant.kind_;
    }
    double constantValue() const {
        MOZ_ASSERT(pod.which_ == Constant);
        return pod.u.constant.value_;
    }

    WASM_DECLARE_SERIALIZABLE(AsmJSGlobal);
};

typedef Vector<AsmJSGlobal, 0, SystemAllocPolicy> AsmJSGlobalVector;

// An AsmJSImport is slightly different than an asm.js FFI function: a single
// asm.js FFI function can be called with many different signatures. When
// compiled to wasm, each unique FFI function paired with signature generates a
// wasm import.
class AsmJSImport
{
    uint32_t ffiIndex_;
  public:
    AsmJSImport() = default;
    explicit AsmJSImport(uint32_t ffiIndex) : ffiIndex_(ffiIndex) {}
    uint32_t ffiIndex() const { return ffiIndex_; }
};

typedef Vector<AsmJSImport, 0, SystemAllocPolicy> AsmJSImportVector;

// An AsmJSExport logically extends Export with the extra information needed for
// an asm.js exported function, viz., the offsets in module's source chars in
// case the function is toString()ed.
class AsmJSExport
{
    // All fields are treated as cacheable POD:
    uint32_t startOffsetInModule_;  // Store module-start-relative offsets
    uint32_t endOffsetInModule_;    // so preserved by serialization.

  public:
    AsmJSExport() { PodZero(this); }
    AsmJSExport(uint32_t startOffsetInModule, uint32_t endOffsetInModule)
      : startOffsetInModule_(startOffsetInModule),
        endOffsetInModule_(endOffsetInModule)
    {}
    uint32_t startOffsetInModule() const {
        return startOffsetInModule_;
    }
    uint32_t endOffsetInModule() const {
        return endOffsetInModule_;
    }
};

typedef Vector<AsmJSExport, 0, SystemAllocPolicy> AsmJSExportVector;

// Holds the trivially-memcpy()able, serializable portion of AsmJSModuleData.
struct AsmJSModuleCacheablePod
{
    uint32_t                minHeapLength;
    uint32_t                numFFIs;
    uint32_t                srcLength;
    uint32_t                srcLengthWithRightBrace;
};

// Holds the immutable guts of an AsmJSModule. This struct is mutably built up
// by ModuleValidator and then handed over to the AsmJSModule constructor in
// finish().
struct AsmJSModuleData : AsmJSModuleCacheablePod
{
    AsmJSGlobalVector       globals;
    AsmJSImportVector       imports;
    AsmJSExportVector       exports;
    ExportMap               exportMap;
    PropertyName*           globalArgumentName;
    PropertyName*           importArgumentName;
    PropertyName*           bufferArgumentName;

    // These values are not serialized since they are relative to the
    // containing script which can be different between serialization and
    // deserialization contexts. Thus, they must be set explicitly using the
    // ambient Parser/ScriptSource after deserialization. Cloning, however,
    // preserves the same exact parsing context and can copy these values.
    uint32_t                srcStart;
    uint32_t                srcBodyStart;
    bool                    strict;
    ScriptSourceHolder      scriptSource;

    AsmJSModuleData()
      : globalArgumentName(nullptr),
        importArgumentName(nullptr),
        bufferArgumentName(nullptr),
        srcStart(0),
        srcBodyStart(0),
        strict(false)
    {
        PodZero(&pod());
    }

    AsmJSModuleCacheablePod& pod() { return *this; }
    const AsmJSModuleCacheablePod& pod() const { return *this; }

    void trace(JSTracer* trc) const {
        for (const AsmJSGlobal& global : globals)
            global.trace(trc);
        TraceNameField(trc, &globalArgumentName, "asm.js global argument name");
        TraceNameField(trc, &importArgumentName, "asm.js import argument name");
        TraceNameField(trc, &bufferArgumentName, "asm.js buffer argument name");
    }

    WASM_DECLARE_SERIALIZABLE(AsmJSModuleData)
};

typedef UniquePtr<AsmJSModuleData> UniqueAsmJSModuleData;

// An AsmJSModule is-a Module with the extra persistent state necessary to
// represent a compiled asm.js module.
class js::AsmJSModule final : public Module
{
    typedef UniquePtr<const AsmJSModuleData> UniqueConstAsmJSModuleData;
    typedef UniquePtr<const StaticLinkData> UniqueConstStaticLinkData;

    const UniqueConstStaticLinkData link_;
    const UniqueConstAsmJSModuleData module_;

  public:
    AsmJSModule(UniqueModuleData base,
                UniqueStaticLinkData link,
                UniqueAsmJSModuleData module)
      : Module(Move(base)),
        link_(Move(link)),
        module_(Move(module))
    {}

    virtual void trace(JSTracer* trc) override {
        Module::trace(trc);
        module_->trace(trc);
    }
    virtual void addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code, size_t* data) override {
        Module::addSizeOfMisc(mallocSizeOf, code, data);
        *data += mallocSizeOf(link_.get()) + link_->sizeOfExcludingThis(mallocSizeOf);
        *data += mallocSizeOf(module_.get()) + module_->sizeOfExcludingThis(mallocSizeOf);
    }

    uint32_t minHeapLength() const { return module_->minHeapLength; }
    uint32_t numFFIs() const { return module_->numFFIs; }
    bool strict() const { return module_->strict; }
    ScriptSource* scriptSource() const { return module_->scriptSource.get(); }
    const AsmJSGlobalVector& asmJSGlobals() const { return module_->globals; }
    const AsmJSImportVector& asmJSImports() const { return module_->imports; }
    const AsmJSExportVector& asmJSExports() const { return module_->exports; }
    const ExportMap& exportMap() const { return module_->exportMap; }
    PropertyName* globalArgumentName() const { return module_->globalArgumentName; }
    PropertyName* importArgumentName() const { return module_->importArgumentName; }
    PropertyName* bufferArgumentName() const { return module_->bufferArgumentName; }

    // srcStart() refers to the offset in the ScriptSource to the beginning of
    // the asm.js module function. If the function has been created with the
    // Function constructor, this will be the first character in the function
    // source. Otherwise, it will be the opening parenthesis of the arguments
    // list.
    uint32_t srcStart() const {
        return module_->srcStart;
    }
    uint32_t srcEndBeforeCurly() const {
        return module_->srcStart + module_->srcLength;
    }
    uint32_t srcEndAfterCurly() const {
        return module_->srcStart + module_->srcLengthWithRightBrace;
    }

    // srcBodyStart() refers to the offset in the ScriptSource to the end
    // of the 'use asm' string-literal token.
    uint32_t srcBodyStart() const {
        return module_->srcBodyStart;
    }

    bool staticallyLink(ExclusiveContext* cx) {
        return Module::staticallyLink(cx, *link_);
    }

    // Clone this AsmJSModule into a new AsmJSModule that isn't statically or
    // dynamically linked for cases where a single asm.js module function is
    // linked several times.
    bool clone(JSContext* cx, MutableHandle<WasmModuleObject*> moduleObj) const;

    // These are the top-level serialization functions used by caching to
    // cache an AsmJSModule.
    size_t serializedSize() const;
    uint8_t* serialize(uint8_t* cursor) const;
    static const uint8_t*
    deserialize(ExclusiveContext* cx, const uint8_t* cursor, AsmJSParser& parser,
                MutableHandle<WasmModuleObject*> moduleObj);
};

/*****************************************************************************/
// ParseNode utilities

static inline ParseNode*
NextNode(ParseNode* pn)
{
    return pn->pn_next;
}

static inline ParseNode*
UnaryKid(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_UNARY));
    return pn->pn_kid;
}

static inline ParseNode*
BinaryRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_BINARY));
    return pn->pn_right;
}

static inline ParseNode*
BinaryLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_BINARY));
    return pn->pn_left;
}

static inline ParseNode*
ReturnExpr(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_RETURN));
    return UnaryKid(pn);
}

static inline ParseNode*
TernaryKid1(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid1;
}

static inline ParseNode*
TernaryKid2(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid2;
}

static inline ParseNode*
TernaryKid3(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid3;
}

static inline ParseNode*
ListHead(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_LIST));
    return pn->pn_head;
}

static inline unsigned
ListLength(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_LIST));
    return pn->pn_count;
}

static inline ParseNode*
CallCallee(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_CALL));
    return ListHead(pn);
}

static inline unsigned
CallArgListLength(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_CALL));
    MOZ_ASSERT(ListLength(pn) >= 1);
    return ListLength(pn) - 1;
}

static inline ParseNode*
CallArgList(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_CALL));
    return NextNode(ListHead(pn));
}

static inline ParseNode*
VarListHead(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_VAR) || pn->isKind(PNK_CONST));
    return ListHead(pn);
}

static inline bool
IsDefaultCase(ParseNode* pn)
{
    return pn->as<CaseClause>().isDefault();
}

static inline ParseNode*
CaseExpr(ParseNode* pn)
{
    return pn->as<CaseClause>().caseExpression();
}

static inline ParseNode*
CaseBody(ParseNode* pn)
{
    return pn->as<CaseClause>().statementList();
}

static inline ParseNode*
BinaryOpLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isBinaryOperation());
    MOZ_ASSERT(pn->isArity(PN_LIST));
    MOZ_ASSERT(pn->pn_count == 2);
    return ListHead(pn);
}

static inline ParseNode*
BinaryOpRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isBinaryOperation());
    MOZ_ASSERT(pn->isArity(PN_LIST));
    MOZ_ASSERT(pn->pn_count == 2);
    return NextNode(ListHead(pn));
}

static inline ParseNode*
BitwiseLeft(ParseNode* pn)
{
    return BinaryOpLeft(pn);
}

static inline ParseNode*
BitwiseRight(ParseNode* pn)
{
    return BinaryOpRight(pn);
}

static inline ParseNode*
MultiplyLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_STAR));
    return BinaryOpLeft(pn);
}

static inline ParseNode*
MultiplyRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_STAR));
    return BinaryOpRight(pn);
}

static inline ParseNode*
AddSubLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_ADD) || pn->isKind(PNK_SUB));
    return BinaryOpLeft(pn);
}

static inline ParseNode*
AddSubRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_ADD) || pn->isKind(PNK_SUB));
    return BinaryOpRight(pn);
}

static inline ParseNode*
DivOrModLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_DIV) || pn->isKind(PNK_MOD));
    return BinaryOpLeft(pn);
}

static inline ParseNode*
DivOrModRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_DIV) || pn->isKind(PNK_MOD));
    return BinaryOpRight(pn);
}

static inline ParseNode*
ComparisonLeft(ParseNode* pn)
{
    return BinaryOpLeft(pn);
}

static inline ParseNode*
ComparisonRight(ParseNode* pn)
{
    return BinaryOpRight(pn);
}

static inline ParseNode*
AndOrLeft(ParseNode* pn)
{
    return BinaryOpLeft(pn);
}

static inline ParseNode*
AndOrRight(ParseNode* pn)
{
    return BinaryOpRight(pn);
}

static inline ParseNode*
RelationalLeft(ParseNode* pn)
{
    return BinaryOpLeft(pn);
}

static inline ParseNode*
RelationalRight(ParseNode* pn)
{
    return BinaryOpRight(pn);
}

static inline bool
IsExpressionStatement(ParseNode* pn)
{
    return pn->isKind(PNK_SEMI);
}

static inline ParseNode*
ExpressionStatementExpr(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_SEMI));
    return UnaryKid(pn);
}

static inline PropertyName*
LoopControlMaybeLabel(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_BREAK) || pn->isKind(PNK_CONTINUE));
    MOZ_ASSERT(pn->isArity(PN_NULLARY));
    return pn->as<LoopControlStatement>().label();
}

static inline PropertyName*
LabeledStatementLabel(ParseNode* pn)
{
    return pn->as<LabeledStatement>().label();
}

static inline ParseNode*
LabeledStatementStatement(ParseNode* pn)
{
    return pn->as<LabeledStatement>().statement();
}

static double
NumberNodeValue(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_NUMBER));
    return pn->pn_dval;
}

static bool
NumberNodeHasFrac(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_NUMBER));
    return pn->pn_u.number.decimalPoint == HasDecimal;
}

static ParseNode*
DotBase(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_DOT));
    MOZ_ASSERT(pn->isArity(PN_NAME));
    return pn->expr();
}

static PropertyName*
DotMember(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_DOT));
    MOZ_ASSERT(pn->isArity(PN_NAME));
    return pn->pn_atom->asPropertyName();
}

static ParseNode*
ElemBase(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_ELEM));
    return BinaryLeft(pn);
}

static ParseNode*
ElemIndex(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_ELEM));
    return BinaryRight(pn);
}

static inline JSFunction*
FunctionObject(ParseNode* fn)
{
    MOZ_ASSERT(fn->isKind(PNK_FUNCTION));
    MOZ_ASSERT(fn->isArity(PN_CODE));
    return fn->pn_funbox->function();
}

static inline PropertyName*
FunctionName(ParseNode* fn)
{
    if (JSAtom* atom = FunctionObject(fn)->atom())
        return atom->asPropertyName();
    return nullptr;
}

static inline ParseNode*
FunctionStatementList(ParseNode* fn)
{
    MOZ_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
    ParseNode* last = fn->pn_body->last();
    MOZ_ASSERT(last->isKind(PNK_STATEMENTLIST));
    return last;
}

static inline bool
IsNormalObjectField(ExclusiveContext* cx, ParseNode* pn)
{
    return pn->isKind(PNK_COLON) &&
           pn->getOp() == JSOP_INITPROP &&
           BinaryLeft(pn)->isKind(PNK_OBJECT_PROPERTY_NAME);
}

static inline PropertyName*
ObjectNormalFieldName(ExclusiveContext* cx, ParseNode* pn)
{
    MOZ_ASSERT(IsNormalObjectField(cx, pn));
    MOZ_ASSERT(BinaryLeft(pn)->isKind(PNK_OBJECT_PROPERTY_NAME));
    return BinaryLeft(pn)->pn_atom->asPropertyName();
}

static inline ParseNode*
ObjectNormalFieldInitializer(ExclusiveContext* cx, ParseNode* pn)
{
    MOZ_ASSERT(IsNormalObjectField(cx, pn));
    return BinaryRight(pn);
}

static inline bool
IsDefinition(ParseNode* pn)
{
    return pn->isKind(PNK_NAME) && pn->isDefn();
}

static inline ParseNode*
MaybeDefinitionInitializer(ParseNode* pn)
{
    MOZ_ASSERT(IsDefinition(pn));
    return pn->expr();
}

static inline bool
IsUseOfName(ParseNode* pn, PropertyName* name)
{
    return pn->isKind(PNK_NAME) && pn->name() == name;
}

static inline bool
IsIgnoredDirectiveName(ExclusiveContext* cx, JSAtom* atom)
{
    return atom != cx->names().useStrict;
}

static inline bool
IsIgnoredDirective(ExclusiveContext* cx, ParseNode* pn)
{
    return pn->isKind(PNK_SEMI) &&
           UnaryKid(pn) &&
           UnaryKid(pn)->isKind(PNK_STRING) &&
           IsIgnoredDirectiveName(cx, UnaryKid(pn)->pn_atom);
}

static inline bool
IsEmptyStatement(ParseNode* pn)
{
    return pn->isKind(PNK_SEMI) && !UnaryKid(pn);
}

static inline ParseNode*
SkipEmptyStatements(ParseNode* pn)
{
    while (pn && IsEmptyStatement(pn))
        pn = pn->pn_next;
    return pn;
}

static inline ParseNode*
NextNonEmptyStatement(ParseNode* pn)
{
    return SkipEmptyStatements(pn->pn_next);
}

static bool
GetToken(AsmJSParser& parser, TokenKind* tkp)
{
    TokenStream& ts = parser.tokenStream;
    TokenKind tk;
    while (true) {
        if (!ts.getToken(&tk, TokenStream::Operand))
            return false;
        if (tk != TOK_SEMI)
            break;
    }
    *tkp = tk;
    return true;
}

static bool
PeekToken(AsmJSParser& parser, TokenKind* tkp)
{
    TokenStream& ts = parser.tokenStream;
    TokenKind tk;
    while (true) {
        if (!ts.peekToken(&tk, TokenStream::Operand))
            return false;
        if (tk != TOK_SEMI)
            break;
        ts.consumeKnownToken(TOK_SEMI, TokenStream::Operand);
    }
    *tkp = tk;
    return true;
}

static bool
ParseVarOrConstStatement(AsmJSParser& parser, ParseNode** var)
{
    TokenKind tk;
    if (!PeekToken(parser, &tk))
        return false;
    if (tk != TOK_VAR && tk != TOK_CONST) {
        *var = nullptr;
        return true;
    }

    *var = parser.statement(YieldIsName);
    if (!*var)
        return false;

    MOZ_ASSERT((*var)->isKind(PNK_VAR) || (*var)->isKind(PNK_CONST));
    return true;
}

/*****************************************************************************/

// Represents the type and value of an asm.js numeric literal.
//
// A literal is a double iff the literal contains a decimal point (even if the
// fractional part is 0). Otherwise, integers may be classified:
//  fixnum: [0, 2^31)
//  negative int: [-2^31, 0)
//  big unsigned: [2^31, 2^32)
//  out of range: otherwise
// Lastly, a literal may be a float literal which is any double or integer
// literal coerced with Math.fround.
class NumLit
{
  public:
    enum Which {
        Fixnum,
        NegativeInt,
        BigUnsigned,
        Double,
        Float,
        Int32x4,
        Float32x4,
        Bool32x4,
        OutOfRangeInt = -1
    };

  private:
    Which which_;
    union {
        Value scalar_;
        SimdConstant simd_;
    } u;

  public:
    NumLit() = default;

    NumLit(Which w, Value v) : which_(w) {
        u.scalar_ = v;
        MOZ_ASSERT(!isSimd());
    }

    NumLit(Which w, SimdConstant c) : which_(w) {
        u.simd_ = c;
        MOZ_ASSERT(isSimd());
    }

    Which which() const {
        return which_;
    }

    int32_t toInt32() const {
        MOZ_ASSERT(which_ == Fixnum || which_ == NegativeInt || which_ == BigUnsigned);
        return u.scalar_.toInt32();
    }

    uint32_t toUint32() const {
        return (uint32_t)toInt32();
    }

    double toDouble() const {
        MOZ_ASSERT(which_ == Double);
        return u.scalar_.toDouble();
    }

    float toFloat() const {
        MOZ_ASSERT(which_ == Float);
        return float(u.scalar_.toDouble());
    }

    Value scalarValue() const {
        MOZ_ASSERT(which_ != OutOfRangeInt);
        return u.scalar_;
    }

    bool isSimd() const {
        return which_ == Int32x4 || which_ == Float32x4 || which_ == Bool32x4;
    }

    const SimdConstant& simdValue() const {
        MOZ_ASSERT(isSimd());
        return u.simd_;
    }

    bool valid() const {
        return which_ != OutOfRangeInt;
    }

    ValType type() const {
        switch (which_) {
          case NumLit::Fixnum:
          case NumLit::NegativeInt:
          case NumLit::BigUnsigned:
            return ValType::I32;
          case NumLit::Double:
            return ValType::F64;
          case NumLit::Float:
            return ValType::F32;
          case NumLit::Int32x4:
            return ValType::I32x4;
          case NumLit::Float32x4:
            return ValType::F32x4;
          case NumLit::Bool32x4:
            return ValType::B32x4;
          case NumLit::OutOfRangeInt:;
        }
        MOZ_CRASH("bad literal");
    }

    Val value() const {
        switch (which_) {
          case NumLit::Fixnum:
          case NumLit::NegativeInt:
          case NumLit::BigUnsigned:
            return Val(toUint32());
          case NumLit::Float:
            return Val(toFloat());
          case NumLit::Double:
            return Val(toDouble());
          case NumLit::Int32x4:
            return Val(simdValue().asInt32x4());
          case NumLit::Float32x4:
            return Val(simdValue().asFloat32x4());
          case NumLit::Bool32x4:
            return Val(simdValue().asInt32x4(), ValType::B32x4);
          case NumLit::OutOfRangeInt:;
        }
        MOZ_CRASH("bad literal");
    }
};

// Respresents the type of a general asm.js expression.
class Type
{
  public:
    enum Which {
        Fixnum = NumLit::Fixnum,
        Signed = NumLit::NegativeInt,
        Unsigned = NumLit::BigUnsigned,
        DoubleLit = NumLit::Double,
        Float = NumLit::Float,
        Int32x4 = NumLit::Int32x4,
        Float32x4 = NumLit::Float32x4,
        Bool32x4 = NumLit::Bool32x4,
        Double,
        MaybeDouble,
        MaybeFloat,
        Floatish,
        Int,
        Intish,
        Void
    };

  private:
    Which which_;

  public:
    Type() = default;
    MOZ_IMPLICIT Type(Which w) : which_(w) {}
    MOZ_IMPLICIT Type(AsmJSSimdType type) {
        switch (type) {
          case AsmJSSimdType_int32x4:
            which_ = Int32x4;
            return;
          case AsmJSSimdType_float32x4:
            which_ = Float32x4;
            return;
          case AsmJSSimdType_bool32x4:
            which_ = Bool32x4;
            return;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("bad AsmJSSimdType");
    }

    static Type var(ValType t) {
        switch (t) {
          case ValType::I32:   return Int;
          case ValType::I64:   MOZ_CRASH("no int64 in asm.js");
          case ValType::F32:   return Float;
          case ValType::F64:   return Double;
          case ValType::I32x4: return Int32x4;
          case ValType::F32x4: return Float32x4;
          case ValType::B32x4: return Bool32x4;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("bad type");
    }

    static Type ret(ExprType t) {
        switch (t) {
          case ExprType::Void:   return Type::Void;
          case ExprType::I32:    return Signed;
          case ExprType::I64:    MOZ_CRASH("no int64 in asm.js");
          case ExprType::F32:    return Float;
          case ExprType::F64:    return Double;
          case ExprType::I32x4:  return Int32x4;
          case ExprType::F32x4:  return Float32x4;
          case ExprType::B32x4:  return Bool32x4;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("bad type");
    }

    static Type lit(const NumLit& lit) {
        MOZ_ASSERT(lit.valid());
        Which which = Type::Which(lit.which());
        MOZ_ASSERT(which >= Fixnum && which <= Bool32x4);
        Type t;
        t.which_ = which;
        return t;
    }

    Which which() const { return which_; }

    bool operator==(Type rhs) const { return which_ == rhs.which_; }
    bool operator!=(Type rhs) const { return which_ != rhs.which_; }

    bool operator<=(Type rhs) const {
        switch (rhs.which_) {
          case Signed:      return isSigned();
          case Unsigned:    return isUnsigned();
          case DoubleLit:   return isDoubleLit();
          case Double:      return isDouble();
          case Float:       return isFloat();
          case Int32x4:     return isInt32x4();
          case Float32x4:   return isFloat32x4();
          case Bool32x4:    return isBool32x4();
          case MaybeDouble: return isMaybeDouble();
          case MaybeFloat:  return isMaybeFloat();
          case Floatish:    return isFloatish();
          case Int:         return isInt();
          case Intish:      return isIntish();
          case Fixnum:      return isFixnum();
          case Void:        return isVoid();
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected rhs type");
    }

    bool operator<=(ValType rhs) const {
        switch (rhs) {
          case ValType::I32:    return isInt();
          case ValType::I64:    MOZ_CRASH("no int64 in asm.js");
          case ValType::F32:    return isFloat();
          case ValType::F64:    return isDouble();
          case ValType::I32x4:  return isInt32x4();
          case ValType::F32x4:  return isFloat32x4();
          case ValType::B32x4:  return isBool32x4();
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected rhs type");
    }

    bool isFixnum() const {
        return which_ == Fixnum;
    }

    bool isSigned() const {
        return which_ == Signed || which_ == Fixnum;
    }

    bool isUnsigned() const {
        return which_ == Unsigned || which_ == Fixnum;
    }

    bool isInt() const {
        return isSigned() || isUnsigned() || which_ == Int;
    }

    bool isIntish() const {
        return isInt() || which_ == Intish;
    }

    bool isDoubleLit() const {
        return which_ == DoubleLit;
    }

    bool isDouble() const {
        return isDoubleLit() || which_ == Double;
    }

    bool isMaybeDouble() const {
        return isDouble() || which_ == MaybeDouble;
    }

    bool isFloat() const {
        return which_ == Float;
    }

    bool isMaybeFloat() const {
        return isFloat() || which_ == MaybeFloat;
    }

    bool isFloatish() const {
        return isMaybeFloat() || which_ == Floatish;
    }

    bool isVoid() const {
        return which_ == Void;
    }

    bool isExtern() const {
        return isDouble() || isSigned();
    }

    bool isInt32x4() const {
        return which_ == Int32x4;
    }

    bool isFloat32x4() const {
        return which_ == Float32x4;
    }

    bool isBool32x4() const {
        return which_ == Bool32x4;
    }

    bool isSimd() const {
        return isInt32x4() || isFloat32x4() || isBool32x4();
    }

    bool isVarType() const {
        return isInt() || isFloat() || isDouble() || isSimd();
    }

    ValType checkedValueType() const {
        MOZ_ASSERT(isVarType());
        if (isInt())
            return ValType::I32;
        else if (isFloat())
            return ValType::F32;
        else if (isDouble())
            return ValType::F64;
        else if (isInt32x4())
            return ValType::I32x4;
        else if (isBool32x4())
            return ValType::B32x4;
        MOZ_ASSERT(isFloat32x4());
        return ValType::F32x4;
    }

    MIRType toMIRType() const {
        switch (which_) {
          case Double:
          case DoubleLit:
          case MaybeDouble:
            return MIRType_Double;
          case Float:
          case Floatish:
          case MaybeFloat:
            return MIRType_Float32;
          case Fixnum:
          case Int:
          case Signed:
          case Unsigned:
          case Intish:
            return MIRType_Int32;
          case Int32x4:
            return MIRType_Int32x4;
          case Float32x4:
            return MIRType_Float32x4;
          case Bool32x4:
            return MIRType_Bool32x4;
          case Void:
            return MIRType_None;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Invalid Type");
    }

    AsmJSSimdType simdType() const {
        MOZ_ASSERT(isSimd());
        switch (which_) {
          case Int32x4:
            return AsmJSSimdType_int32x4;
          case Float32x4:
            return AsmJSSimdType_float32x4;
          case Bool32x4:
            return AsmJSSimdType_bool32x4;
          // Scalar types
          case Double:
          case DoubleLit:
          case MaybeDouble:
          case Float:
          case MaybeFloat:
          case Floatish:
          case Fixnum:
          case Int:
          case Signed:
          case Unsigned:
          case Intish:
          case Void:
            break;
        }
        MOZ_CRASH("not a SIMD Type");
    }

    const char* toChars() const {
        switch (which_) {
          case Double:      return "double";
          case DoubleLit:   return "doublelit";
          case MaybeDouble: return "double?";
          case Float:       return "float";
          case Floatish:    return "floatish";
          case MaybeFloat:  return "float?";
          case Fixnum:      return "fixnum";
          case Int:         return "int";
          case Signed:      return "signed";
          case Unsigned:    return "unsigned";
          case Intish:      return "intish";
          case Int32x4:     return "int32x4";
          case Float32x4:   return "float32x4";
          case Bool32x4:    return "bool32x4";
          case Void:        return "void";
        }
        MOZ_CRASH("Invalid Type");
    }
};

static const unsigned VALIDATION_LIFO_DEFAULT_CHUNK_SIZE = 4 * 1024;

// The ModuleValidator encapsulates the entire validation of an asm.js module.
// Its lifetime goes from the validation of the top components of an asm.js
// module (all the globals), the emission of bytecode for all the functions in
// the module and the validation of function's pointer tables. It also finishes
// the compilation of all the module's stubs.
//
// Rooting note: ModuleValidator is a stack class that contains unrooted
// PropertyName (JSAtom) pointers.  This is safe because it cannot be
// constructed without a TokenStream reference.  TokenStream is itself a stack
// class that cannot be constructed without an AutoKeepAtoms being live on the
// stack, which prevents collection of atoms.
//
// ModuleValidator is marked as rooted in the rooting analysis.  Don't add
// non-JSAtom pointers, or this will break!
class MOZ_STACK_CLASS ModuleValidator
{
  public:
    class Func
    {
        PropertyName* name_;
        uint32_t firstUse_;
        uint32_t index_;
        uint32_t srcBegin_;
        uint32_t srcEnd_;
        bool defined_;

      public:
        Func(PropertyName* name, uint32_t firstUse, uint32_t index)
          : name_(name), firstUse_(firstUse), index_(index),
            srcBegin_(0), srcEnd_(0), defined_(false)
        {}

        PropertyName* name() const { return name_; }
        uint32_t firstUse() const { return firstUse_; }
        bool defined() const { return defined_; }
        uint32_t index() const { return index_; }

        void define(ParseNode* fn) {
            MOZ_ASSERT(!defined_);
            defined_ = true;
            srcBegin_ = fn->pn_pos.begin;
            srcEnd_ = fn->pn_pos.end;
        }

        uint32_t srcBegin() const { MOZ_ASSERT(defined_); return srcBegin_; }
        uint32_t srcEnd() const { MOZ_ASSERT(defined_); return srcEnd_; }
    };

    typedef Vector<const Func*> ConstFuncVector;
    typedef Vector<Func*> FuncVector;

    class FuncPtrTable
    {
        uint32_t sigIndex_;
        PropertyName* name_;
        uint32_t firstUse_;
        uint32_t mask_;
        bool defined_;

        FuncPtrTable(FuncPtrTable&& rhs) = delete;

      public:
        FuncPtrTable(uint32_t sigIndex, PropertyName* name, uint32_t firstUse, uint32_t mask)
          : sigIndex_(sigIndex), name_(name), firstUse_(firstUse), mask_(mask), defined_(false)
        {}

        uint32_t sigIndex() const { return sigIndex_; }
        PropertyName* name() const { return name_; }
        uint32_t firstUse() const { return firstUse_; }
        unsigned mask() const { return mask_; }
        bool defined() const { return defined_; }
        void define() { MOZ_ASSERT(!defined_); defined_ = true; }
    };

    typedef Vector<FuncPtrTable*> FuncPtrTableVector;

    class Global
    {
      public:
        enum Which {
            Variable,
            ConstantLiteral,
            ConstantImport,
            Function,
            FuncPtrTable,
            FFI,
            ArrayView,
            ArrayViewCtor,
            MathBuiltinFunction,
            AtomicsBuiltinFunction,
            SimdCtor,
            SimdOperation
        };

      private:
        Which which_;
        union {
            struct {
                Type::Which type_;
                uint32_t globalDataOffset_;
                NumLit literalValue_;
            } varOrConst;
            uint32_t funcIndex_;
            uint32_t funcPtrTableIndex_;
            uint32_t ffiIndex_;
            struct {
                Scalar::Type viewType_;
            } viewInfo;
            AsmJSMathBuiltinFunction mathBuiltinFunc_;
            AsmJSAtomicsBuiltinFunction atomicsBuiltinFunc_;
            AsmJSSimdType simdCtorType_;
            struct {
                AsmJSSimdType type_;
                AsmJSSimdOperation which_;
            } simdOp;
        } u;

        friend class ModuleValidator;
        friend class js::LifoAlloc;

        explicit Global(Which which) : which_(which) {}

      public:
        Which which() const {
            return which_;
        }
        Type varOrConstType() const {
            MOZ_ASSERT(which_ == Variable || which_ == ConstantLiteral || which_ == ConstantImport);
            return u.varOrConst.type_;
        }
        uint32_t varOrConstGlobalDataOffset() const {
            MOZ_ASSERT(which_ == Variable || which_ == ConstantImport);
            return u.varOrConst.globalDataOffset_;
        }
        bool isConst() const {
            return which_ == ConstantLiteral || which_ == ConstantImport;
        }
        NumLit constLiteralValue() const {
            MOZ_ASSERT(which_ == ConstantLiteral);
            return u.varOrConst.literalValue_;
        }
        uint32_t funcIndex() const {
            MOZ_ASSERT(which_ == Function);
            return u.funcIndex_;
        }
        uint32_t funcPtrTableIndex() const {
            MOZ_ASSERT(which_ == FuncPtrTable);
            return u.funcPtrTableIndex_;
        }
        unsigned ffiIndex() const {
            MOZ_ASSERT(which_ == FFI);
            return u.ffiIndex_;
        }
        bool isAnyArrayView() const {
            return which_ == ArrayView || which_ == ArrayViewCtor;
        }
        Scalar::Type viewType() const {
            MOZ_ASSERT(isAnyArrayView());
            return u.viewInfo.viewType_;
        }
        bool isMathFunction() const {
            return which_ == MathBuiltinFunction;
        }
        AsmJSMathBuiltinFunction mathBuiltinFunction() const {
            MOZ_ASSERT(which_ == MathBuiltinFunction);
            return u.mathBuiltinFunc_;
        }
        bool isAtomicsFunction() const {
            return which_ == AtomicsBuiltinFunction;
        }
        AsmJSAtomicsBuiltinFunction atomicsBuiltinFunction() const {
            MOZ_ASSERT(which_ == AtomicsBuiltinFunction);
            return u.atomicsBuiltinFunc_;
        }
        bool isSimdCtor() const {
            return which_ == SimdCtor;
        }
        AsmJSSimdType simdCtorType() const {
            MOZ_ASSERT(which_ == SimdCtor);
            return u.simdCtorType_;
        }
        bool isSimdOperation() const {
            return which_ == SimdOperation;
        }
        AsmJSSimdOperation simdOperation() const {
            MOZ_ASSERT(which_ == SimdOperation);
            return u.simdOp.which_;
        }
        AsmJSSimdType simdOperationType() const {
            MOZ_ASSERT(which_ == SimdOperation);
            return u.simdOp.type_;
        }
    };

    struct MathBuiltin
    {
        enum Kind { Function, Constant };
        Kind kind;

        union {
            double cst;
            AsmJSMathBuiltinFunction func;
        } u;

        MathBuiltin() : kind(Kind(-1)) {}
        explicit MathBuiltin(double cst) : kind(Constant) {
            u.cst = cst;
        }
        explicit MathBuiltin(AsmJSMathBuiltinFunction func) : kind(Function) {
            u.func = func;
        }
    };

    struct ArrayView
    {
        ArrayView(PropertyName* name, Scalar::Type type)
          : name(name), type(type)
        {}

        PropertyName* name;
        Scalar::Type type;
    };

  private:
    struct SigHashPolicy
    {
        typedef const Sig& Lookup;
        static HashNumber hash(Lookup sig) { return sig.hash(); }
        static bool match(const Sig* lhs, Lookup rhs) { return *lhs == rhs; }
    };
    typedef HashMap<const DeclaredSig*, uint32_t, SigHashPolicy> SigMap;
    class NamedSig
    {
        PropertyName* name_;
        const DeclaredSig* sig_;

      public:
        NamedSig(PropertyName* name, const DeclaredSig& sig)
          : name_(name), sig_(&sig)
        {}
        PropertyName* name() const {
            return name_;
        }
        const Sig& sig() const {
            return *sig_;
        }

        // Implement HashPolicy:
        struct Lookup {
            PropertyName* name;
            const Sig& sig;
            Lookup(PropertyName* name, const Sig& sig) : name(name), sig(sig) {}
        };
        static HashNumber hash(Lookup l) {
            return HashGeneric(l.name, l.sig.hash());
        }
        static bool match(NamedSig lhs, Lookup rhs) {
            return lhs.name_ == rhs.name && *lhs.sig_ == rhs.sig;
        }
    };
    typedef HashMap<NamedSig, uint32_t, NamedSig> ImportMap;
    typedef HashMap<PropertyName*, Global*> GlobalMap;
    typedef HashMap<PropertyName*, MathBuiltin> MathNameMap;
    typedef HashMap<PropertyName*, AsmJSAtomicsBuiltinFunction> AtomicsNameMap;
    typedef HashMap<PropertyName*, AsmJSSimdOperation> SimdOperationNameMap;
    typedef Vector<ArrayView> ArrayViewVector;

    ExclusiveContext*     cx_;
    AsmJSParser&          parser_;
    ParseNode*            moduleFunctionNode_;
    PropertyName*         moduleFunctionName_;
    MathNameMap           standardLibraryMathNames_;
    AtomicsNameMap        standardLibraryAtomicsNames_;
    SimdOperationNameMap  standardLibrarySimdOpNames_;

    // Validation-internal state:
    LifoAlloc             validationLifo_;
    FuncVector            functions_;
    FuncPtrTableVector    funcPtrTables_;
    GlobalMap             globalMap_;
    SigMap                sigMap_;
    ImportMap             importMap_;
    ArrayViewVector       arrayViews_;
    bool                  atomicsPresent_;

    // State used to build the AsmJSModule in finish():
    ModuleGenerator       mg_;
    UniqueAsmJSModuleData module_;

    // Error reporting:
    UniqueChars           errorString_;
    uint32_t              errorOffset_;
    bool                  errorOverRecursed_;

    // Helpers:
    bool addStandardLibraryMathName(const char* name, AsmJSMathBuiltinFunction func) {
        JSAtom* atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        MathBuiltin builtin(func);
        return standardLibraryMathNames_.putNew(atom->asPropertyName(), builtin);
    }
    bool addStandardLibraryMathName(const char* name, double cst) {
        JSAtom* atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        MathBuiltin builtin(cst);
        return standardLibraryMathNames_.putNew(atom->asPropertyName(), builtin);
    }
    bool addStandardLibraryAtomicsName(const char* name, AsmJSAtomicsBuiltinFunction func) {
        JSAtom* atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        return standardLibraryAtomicsNames_.putNew(atom->asPropertyName(), func);
    }
    bool addStandardLibrarySimdOpName(const char* name, AsmJSSimdOperation op) {
        JSAtom* atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        return standardLibrarySimdOpNames_.putNew(atom->asPropertyName(), op);
    }
    bool declareSig(Sig&& sig, uint32_t* sigIndex) {
        SigMap::AddPtr p = sigMap_.lookupForAdd(sig);
        if (p) {
            *sigIndex = p->value();
            MOZ_ASSERT(mg_.sig(*sigIndex) == sig);
            return true;
        }

        *sigIndex = sigMap_.count();
        if (*sigIndex >= MaxSigs)
            return failCurrentOffset("too many unique signatures");

        mg_.initSig(*sigIndex, Move(sig));
        return sigMap_.add(p, &mg_.sig(*sigIndex), *sigIndex);
    }

    // ModuleGeneratorData limits:
    static const unsigned MaxSigs    =   4 * 1024;
    static const unsigned MaxFuncs   = 512 * 1024;
    static const unsigned MaxImports =   4 * 1024;

  public:
    ModuleValidator(ExclusiveContext* cx, AsmJSParser& parser, ParseNode* moduleFunctionNode)
      : cx_(cx),
        parser_(parser),
        moduleFunctionNode_(moduleFunctionNode),
        moduleFunctionName_(FunctionName(moduleFunctionNode)),
        standardLibraryMathNames_(cx),
        standardLibraryAtomicsNames_(cx),
        standardLibrarySimdOpNames_(cx),
        validationLifo_(VALIDATION_LIFO_DEFAULT_CHUNK_SIZE),
        functions_(cx),
        funcPtrTables_(cx),
        globalMap_(cx),
        sigMap_(cx),
        importMap_(cx),
        arrayViews_(cx),
        atomicsPresent_(false),
        mg_(cx),
        errorString_(nullptr),
        errorOffset_(UINT32_MAX),
        errorOverRecursed_(false)
    {}

    ~ModuleValidator() {
        if (errorString_) {
            MOZ_ASSERT(errorOffset_ != UINT32_MAX);
            tokenStream().reportAsmJSError(errorOffset_,
                                           JSMSG_USE_ASM_TYPE_FAIL,
                                           errorString_.get());
        }
        if (errorOverRecursed_)
            ReportOverRecursed(cx_);
    }

    bool init() {
        module_ = cx_->make_unique<AsmJSModuleData>();
        if (!module_)
            return false;

        module_->minHeapLength = RoundUpToNextValidAsmJSHeapLength(0);
        module_->srcStart = moduleFunctionNode_->pn_body->pn_pos.begin;
        module_->srcBodyStart = parser_.tokenStream.currentToken().pos.end;
        module_->strict = parser_.pc->sc->strict() && !parser_.pc->sc->hasExplicitUseStrict();
        module_->scriptSource.reset(parser_.ss);

        if (!globalMap_.init() || !sigMap_.init() || !importMap_.init())
            return false;

        if (!standardLibraryMathNames_.init() ||
            !addStandardLibraryMathName("sin", AsmJSMathBuiltin_sin) ||
            !addStandardLibraryMathName("cos", AsmJSMathBuiltin_cos) ||
            !addStandardLibraryMathName("tan", AsmJSMathBuiltin_tan) ||
            !addStandardLibraryMathName("asin", AsmJSMathBuiltin_asin) ||
            !addStandardLibraryMathName("acos", AsmJSMathBuiltin_acos) ||
            !addStandardLibraryMathName("atan", AsmJSMathBuiltin_atan) ||
            !addStandardLibraryMathName("ceil", AsmJSMathBuiltin_ceil) ||
            !addStandardLibraryMathName("floor", AsmJSMathBuiltin_floor) ||
            !addStandardLibraryMathName("exp", AsmJSMathBuiltin_exp) ||
            !addStandardLibraryMathName("log", AsmJSMathBuiltin_log) ||
            !addStandardLibraryMathName("pow", AsmJSMathBuiltin_pow) ||
            !addStandardLibraryMathName("sqrt", AsmJSMathBuiltin_sqrt) ||
            !addStandardLibraryMathName("abs", AsmJSMathBuiltin_abs) ||
            !addStandardLibraryMathName("atan2", AsmJSMathBuiltin_atan2) ||
            !addStandardLibraryMathName("imul", AsmJSMathBuiltin_imul) ||
            !addStandardLibraryMathName("clz32", AsmJSMathBuiltin_clz32) ||
            !addStandardLibraryMathName("fround", AsmJSMathBuiltin_fround) ||
            !addStandardLibraryMathName("min", AsmJSMathBuiltin_min) ||
            !addStandardLibraryMathName("max", AsmJSMathBuiltin_max) ||
            !addStandardLibraryMathName("E", M_E) ||
            !addStandardLibraryMathName("LN10", M_LN10) ||
            !addStandardLibraryMathName("LN2", M_LN2) ||
            !addStandardLibraryMathName("LOG2E", M_LOG2E) ||
            !addStandardLibraryMathName("LOG10E", M_LOG10E) ||
            !addStandardLibraryMathName("PI", M_PI) ||
            !addStandardLibraryMathName("SQRT1_2", M_SQRT1_2) ||
            !addStandardLibraryMathName("SQRT2", M_SQRT2))
        {
            return false;
        }

        if (!standardLibraryAtomicsNames_.init() ||
            !addStandardLibraryAtomicsName("compareExchange", AsmJSAtomicsBuiltin_compareExchange) ||
            !addStandardLibraryAtomicsName("exchange", AsmJSAtomicsBuiltin_exchange) ||
            !addStandardLibraryAtomicsName("load", AsmJSAtomicsBuiltin_load) ||
            !addStandardLibraryAtomicsName("store", AsmJSAtomicsBuiltin_store) ||
            !addStandardLibraryAtomicsName("fence", AsmJSAtomicsBuiltin_fence) ||
            !addStandardLibraryAtomicsName("add", AsmJSAtomicsBuiltin_add) ||
            !addStandardLibraryAtomicsName("sub", AsmJSAtomicsBuiltin_sub) ||
            !addStandardLibraryAtomicsName("and", AsmJSAtomicsBuiltin_and) ||
            !addStandardLibraryAtomicsName("or", AsmJSAtomicsBuiltin_or) ||
            !addStandardLibraryAtomicsName("xor", AsmJSAtomicsBuiltin_xor) ||
            !addStandardLibraryAtomicsName("isLockFree", AsmJSAtomicsBuiltin_isLockFree))
        {
            return false;
        }

#define ADDSTDLIBSIMDOPNAME(op) || !addStandardLibrarySimdOpName(#op, AsmJSSimdOperation_##op)
        if (!standardLibrarySimdOpNames_.init()
            FORALL_SIMD_ASMJS_OP(ADDSTDLIBSIMDOPNAME))
        {
            return false;
        }
#undef ADDSTDLIBSIMDOPNAME

        UniqueModuleGeneratorData genData = MakeUnique<ModuleGeneratorData>();
        if (!genData ||
            !genData->sigs.resize(MaxSigs) ||
            !genData->funcSigs.resize(MaxFuncs) ||
            !genData->imports.resize(MaxImports))
        {
            return false;
        }

        return mg_.init(Move(genData), ModuleKind::AsmJS);
    }

    ExclusiveContext* cx() const             { return cx_; }
    PropertyName* moduleFunctionName() const { return moduleFunctionName_; }
    PropertyName* globalArgumentName() const { return module_->globalArgumentName; }
    PropertyName* importArgumentName() const { return module_->importArgumentName; }
    PropertyName* bufferArgumentName() const { return module_->bufferArgumentName; }
    ModuleGenerator& mg()                    { return mg_; }
    AsmJSParser& parser() const              { return parser_; }
    TokenStream& tokenStream() const         { return parser_.tokenStream; }
    bool supportsSimd() const                { return cx_->jitSupportsSimd(); }
    bool atomicsPresent() const              { return atomicsPresent_; }
    uint32_t minHeapLength() const           { return module_->minHeapLength; }

    void initModuleFunctionName(PropertyName* name) {
        MOZ_ASSERT(!moduleFunctionName_);
        moduleFunctionName_ = name;
    }
    void initGlobalArgumentName(PropertyName* n) {
        MOZ_ASSERT(n->isTenured());
        module_->globalArgumentName = n;
    }
    void initImportArgumentName(PropertyName* n) {
        MOZ_ASSERT(n->isTenured());
        module_->importArgumentName = n;
    }
    void initBufferArgumentName(PropertyName* n) {
        MOZ_ASSERT(n->isTenured());
        module_->bufferArgumentName = n;
    }
    bool addGlobalVarInit(PropertyName* var, const NumLit& lit, bool isConst) {
        uint32_t globalDataOffset;
        if (!mg_.allocateGlobalVar(lit.type(), &globalDataOffset))
            return false;

        Global::Which which = isConst ? Global::ConstantLiteral : Global::Variable;
        Global* global = validationLifo_.new_<Global>(which);
        if (!global)
            return false;
        global->u.varOrConst.globalDataOffset_ = globalDataOffset;
        global->u.varOrConst.type_ = (isConst ? Type::lit(lit) : Type::var(lit.type())).which();
        if (isConst)
            global->u.varOrConst.literalValue_ = lit;
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::Variable, nullptr);
        g.pod.u.var.initKind_ = AsmJSGlobal::InitConstant;
        g.pod.u.var.u.val_ = lit.value();
        g.pod.u.var.globalDataOffset_ = globalDataOffset;
        return module_->globals.append(g);
    }
    bool addGlobalVarImport(PropertyName* var, PropertyName* field, ValType type, bool isConst) {
        uint32_t globalDataOffset;
        if (!mg_.allocateGlobalVar(type, &globalDataOffset))
            return false;

        Global::Which which = isConst ? Global::ConstantImport : Global::Variable;
        Global* global = validationLifo_.new_<Global>(which);
        if (!global)
            return false;
        global->u.varOrConst.globalDataOffset_ = globalDataOffset;
        global->u.varOrConst.type_ = Type::var(type).which();
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::Variable, field);
        g.pod.u.var.initKind_ = AsmJSGlobal::InitImport;
        g.pod.u.var.u.importType_ = type;
        g.pod.u.var.globalDataOffset_ = globalDataOffset;
        return module_->globals.append(g);
    }
    bool addArrayView(PropertyName* var, Scalar::Type vt, PropertyName* maybeField) {
        if (!arrayViews_.append(ArrayView(var, vt)))
            return false;

        Global* global = validationLifo_.new_<Global>(Global::ArrayView);
        if (!global)
            return false;
        global->u.viewInfo.viewType_ = vt;
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::ArrayView, maybeField);
        g.pod.u.viewType_ = vt;
        return module_->globals.append(g);
    }
    bool addMathBuiltinFunction(PropertyName* var, AsmJSMathBuiltinFunction func,
                                PropertyName* field)
    {
        Global* global = validationLifo_.new_<Global>(Global::MathBuiltinFunction);
        if (!global)
            return false;
        global->u.mathBuiltinFunc_ = func;
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::MathBuiltinFunction, field);
        g.pod.u.mathBuiltinFunc_ = func;
        return module_->globals.append(g);
    }
  private:
    bool addGlobalDoubleConstant(PropertyName* var, double constant) {
        Global* global = validationLifo_.new_<Global>(Global::ConstantLiteral);
        if (!global)
            return false;
        global->u.varOrConst.type_ = Type::Double;
        global->u.varOrConst.literalValue_ = NumLit(NumLit::Double, DoubleValue(constant));
        return globalMap_.putNew(var, global);
    }
  public:
    bool addMathBuiltinConstant(PropertyName* var, double constant, PropertyName* field) {
        if (!addGlobalDoubleConstant(var, constant))
            return false;

        AsmJSGlobal g(AsmJSGlobal::Constant, field);
        g.pod.u.constant.value_ = constant;
        g.pod.u.constant.kind_ = AsmJSGlobal::MathConstant;
        return module_->globals.append(g);
    }
    bool addGlobalConstant(PropertyName* var, double constant, PropertyName* field) {
        if (!addGlobalDoubleConstant(var, constant))
            return false;

        AsmJSGlobal g(AsmJSGlobal::Constant, field);
        g.pod.u.constant.value_ = constant;
        g.pod.u.constant.kind_ = AsmJSGlobal::GlobalConstant;
        return module_->globals.append(g);
    }
    bool addAtomicsBuiltinFunction(PropertyName* var, AsmJSAtomicsBuiltinFunction func,
                                   PropertyName* field)
    {
        atomicsPresent_ = true;

        Global* global = validationLifo_.new_<Global>(Global::AtomicsBuiltinFunction);
        if (!global)
            return false;
        global->u.atomicsBuiltinFunc_ = func;
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::AtomicsBuiltinFunction, field);
        g.pod.u.atomicsBuiltinFunc_ = func;
        return module_->globals.append(g);
    }
    bool addSimdCtor(PropertyName* var, AsmJSSimdType type, PropertyName* field) {
        Global* global = validationLifo_.new_<Global>(Global::SimdCtor);
        if (!global)
            return false;
        global->u.simdCtorType_ = type;
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::SimdCtor, field);
        g.pod.u.simdCtorType_ = type;
        return module_->globals.append(g);
    }
    bool addSimdOperation(PropertyName* var, AsmJSSimdType type, AsmJSSimdOperation op,
                          PropertyName* opName)
    {
        Global* global = validationLifo_.new_<Global>(Global::SimdOperation);
        if (!global)
            return false;
        global->u.simdOp.type_ = type;
        global->u.simdOp.which_ = op;
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::SimdOperation, opName);
        g.pod.u.simdOp.type_ = type;
        g.pod.u.simdOp.which_ = op;
        return module_->globals.append(g);
    }
    bool addArrayViewCtor(PropertyName* var, Scalar::Type vt, PropertyName* field) {
        Global* global = validationLifo_.new_<Global>(Global::ArrayViewCtor);
        if (!global)
            return false;
        global->u.viewInfo.viewType_ = vt;
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::ArrayViewCtor, field);
        g.pod.u.viewType_ = vt;
        return module_->globals.append(g);
    }
    bool addFFI(PropertyName* var, PropertyName* field) {
        if (module_->numFFIs == UINT32_MAX)
            return false;
        uint32_t ffiIndex = module_->numFFIs++;

        Global* global = validationLifo_.new_<Global>(Global::FFI);
        if (!global)
            return false;
        global->u.ffiIndex_ = ffiIndex;
        if (!globalMap_.putNew(var, global))
            return false;

        AsmJSGlobal g(AsmJSGlobal::FFI, field);
        g.pod.u.ffiIndex_ = ffiIndex;
        return module_->globals.append(g);
    }
    bool addExportField(ParseNode* pn, const Func& func, PropertyName* maybeFieldName) {
        // Record the field name of this export.
        CacheableChars fieldName;
        if (maybeFieldName)
            fieldName = StringToNewUTF8CharsZ(cx_, *maybeFieldName);
        else
            fieldName = DuplicateString("");
        if (!fieldName || !module_->exportMap.fieldNames.append(Move(fieldName)))
            return false;

        // Declare which function is exported which gives us an index into the
        // module ExportVector.
        uint32_t exportIndex;
        if (!mg_.declareExport(func.index(), &exportIndex))
            return false;

        // Add a mapping from the given field to the Export's index.
        if (!module_->exportMap.fieldsToExports.append(exportIndex))
            return false;

        // The exported function might have already been exported in which case
        // the index will refer into the range of AsmJSExports.
        MOZ_ASSERT(exportIndex <= module_->exports.length());
        if (exportIndex < module_->exports.length())
            return true;

        // If this is a new export, record the src info for later toString.
        CacheableChars exportName = StringToNewUTF8CharsZ(cx_, *func.name());
        return exportName &&
               module_->exportMap.exportNames.emplaceBack(Move(exportName)) &&
               module_->exports.emplaceBack(func.srcBegin() - module_->srcStart,
                                            func.srcEnd() - module_->srcStart);
    }
    bool addFunction(PropertyName* name, uint32_t firstUse, Sig&& sig, Func** func) {
        uint32_t sigIndex;
        if (!declareSig(Move(sig), &sigIndex))
            return false;
        uint32_t funcIndex = numFunctions();
        if (funcIndex >= MaxFuncs)
            return failCurrentOffset("too many functions");
        if (!mg_.initFuncSig(funcIndex, sigIndex))
            return false;
        Global* global = validationLifo_.new_<Global>(Global::Function);
        if (!global)
            return false;
        global->u.funcIndex_ = funcIndex;
        if (!globalMap_.putNew(name, global))
            return false;
        *func = validationLifo_.new_<Func>(name, firstUse, funcIndex);
        return *func && functions_.append(*func);
    }
    bool declareFuncPtrTable(Sig&& sig, PropertyName* name, uint32_t firstUse, uint32_t mask,
                             uint32_t* index)
    {
        if (!mg_.declareFuncPtrTable(/* numElems = */ mask + 1, index))
            return false;
        MOZ_ASSERT(*index == numFuncPtrTables());
        Global* global = validationLifo_.new_<Global>(Global::FuncPtrTable);
        if (!global)
            return false;
        global->u.funcPtrTableIndex_ = *index;
        if (!globalMap_.putNew(name, global))
            return false;
        uint32_t sigIndex;
        if (!declareSig(Move(sig), &sigIndex))
            return false;
        FuncPtrTable* t = validationLifo_.new_<FuncPtrTable>(sigIndex, name, firstUse, mask);
        return t && funcPtrTables_.append(t);
    }
    bool defineFuncPtrTable(uint32_t funcPtrTableIndex, const Vector<uint32_t>& elems) {
        FuncPtrTable& table = *funcPtrTables_[funcPtrTableIndex];
        if (table.defined())
            return false;
        table.define();
        mg_.defineFuncPtrTable(funcPtrTableIndex, elems);
        return true;
    }
    bool declareImport(PropertyName* name, Sig&& sig, unsigned ffiIndex, uint32_t* importIndex) {
        ImportMap::AddPtr p = importMap_.lookupForAdd(NamedSig::Lookup(name, sig));
        if (p) {
            *importIndex = p->value();
            return true;
        }
        *importIndex = module_->imports.length();
        if (*importIndex >= MaxImports)
            return failCurrentOffset("too many imports");
        if (!module_->imports.emplaceBack(ffiIndex))
            return false;
        uint32_t sigIndex;
        if (!declareSig(Move(sig), &sigIndex))
            return false;
        uint32_t globalDataOffset;
        if (!mg_.allocateGlobalBytes(Module::SizeOfImportExit, sizeof(void*), &globalDataOffset))
            return false;
        if (!mg_.initImport(*importIndex, sigIndex, globalDataOffset))
            return false;
        return importMap_.add(p, NamedSig(name, mg_.sig(sigIndex)), *importIndex);
    }

    bool tryConstantAccess(uint64_t start, uint64_t width) {
        MOZ_ASSERT(UINT64_MAX - start > width);
        uint64_t len = start + width;
        if (len > uint64_t(INT32_MAX) + 1)
            return false;
        len = RoundUpToNextValidAsmJSHeapLength(len);
        if (len > module_->minHeapLength)
            module_->minHeapLength = len;
        return true;
    }

    // Error handling.
    bool hasAlreadyFailed() const {
        return !!errorString_;
    }

    bool failOffset(uint32_t offset, const char* str) {
        MOZ_ASSERT(!hasAlreadyFailed());
        MOZ_ASSERT(errorOffset_ == UINT32_MAX);
        MOZ_ASSERT(str);
        errorOffset_ = offset;
        errorString_ = DuplicateString(str);
        return false;
    }

    bool failCurrentOffset(const char* str) {
        return failOffset(tokenStream().currentToken().pos.begin, str);
    }

    bool fail(ParseNode* pn, const char* str) {
        return failOffset(pn->pn_pos.begin, str);
    }

    bool failfVAOffset(uint32_t offset, const char* fmt, va_list ap) {
        MOZ_ASSERT(!hasAlreadyFailed());
        MOZ_ASSERT(errorOffset_ == UINT32_MAX);
        MOZ_ASSERT(fmt);
        errorOffset_ = offset;
        errorString_.reset(JS_vsmprintf(fmt, ap));
        return false;
    }

    bool failfOffset(uint32_t offset, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        failfVAOffset(offset, fmt, ap);
        va_end(ap);
        return false;
    }

    bool failf(ParseNode* pn, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        failfVAOffset(pn->pn_pos.begin, fmt, ap);
        va_end(ap);
        return false;
    }

    bool failNameOffset(uint32_t offset, const char* fmt, PropertyName* name) {
        // This function is invoked without the caller properly rooting its locals.
        gc::AutoSuppressGC suppress(cx_);
        JSAutoByteString bytes;
        if (AtomToPrintableString(cx_, name, &bytes))
            failfOffset(offset, fmt, bytes.ptr());
        return false;
    }

    bool failName(ParseNode* pn, const char* fmt, PropertyName* name) {
        return failNameOffset(pn->pn_pos.begin, fmt, name);
    }

    bool failOverRecursed() {
        errorOverRecursed_ = true;
        return false;
    }

    unsigned numArrayViews() const {
        return arrayViews_.length();
    }
    const ArrayView& arrayView(unsigned i) const {
        return arrayViews_[i];
    }
    unsigned numFunctions() const {
        return functions_.length();
    }
    Func& function(unsigned i) const {
        return *functions_[i];
    }
    unsigned numFuncPtrTables() const {
        return funcPtrTables_.length();
    }
    FuncPtrTable& funcPtrTable(unsigned i) const {
        return *funcPtrTables_[i];
    }

    const Global* lookupGlobal(PropertyName* name) const {
        if (GlobalMap::Ptr p = globalMap_.lookup(name))
            return p->value();
        return nullptr;
    }

    Func* lookupFunction(PropertyName* name) {
        if (GlobalMap::Ptr p = globalMap_.lookup(name)) {
            Global* value = p->value();
            if (value->which() == Global::Function)
                return functions_[value->funcIndex()];
        }
        return nullptr;
    }

    bool lookupStandardLibraryMathName(PropertyName* name, MathBuiltin* mathBuiltin) const {
        if (MathNameMap::Ptr p = standardLibraryMathNames_.lookup(name)) {
            *mathBuiltin = p->value();
            return true;
        }
        return false;
    }
    bool lookupStandardLibraryAtomicsName(PropertyName* name, AsmJSAtomicsBuiltinFunction* atomicsBuiltin) const {
        if (AtomicsNameMap::Ptr p = standardLibraryAtomicsNames_.lookup(name)) {
            *atomicsBuiltin = p->value();
            return true;
        }
        return false;
    }
    bool lookupStandardSimdOpName(PropertyName* name, AsmJSSimdOperation* op) const {
        if (SimdOperationNameMap::Ptr p = standardLibrarySimdOpNames_.lookup(name)) {
            *op = p->value();
            return true;
        }
        return false;
    }

    bool startFunctionBodies() {
        return true;
    }
    bool finishFunctionBodies() {
        return mg_.finishFuncDefs();
    }
    bool finish(MutableHandle<WasmModuleObject*> moduleObj, SlowFunctionVector* slowFuncs) {
        HeapUsage heap = arrayViews_.empty()
                              ? HeapUsage::None
                              : atomicsPresent_
                                ? HeapUsage::Shared
                                : HeapUsage::Unshared;

        auto muted = MutedErrorsBool(parser_.ss->mutedErrors());

        CacheableChars filename;
        if (parser_.ss->filename()) {
            filename = DuplicateString(parser_.ss->filename());
            if (!filename)
                return false;
        }

        CacheableTwoByteChars displayURL;
        if (parser_.ss->hasDisplayURL()) {
            displayURL = DuplicateString(parser_.ss->displayURL());
            if (!displayURL)
                return false;
        }

        uint32_t endBeforeCurly = tokenStream().currentToken().pos.end;
        module_->srcLength = endBeforeCurly - module_->srcStart;

        TokenPos pos;
        JS_ALWAYS_TRUE(tokenStream().peekTokenPos(&pos, TokenStream::Operand));
        uint32_t endAfterCurly = pos.end;
        module_->srcLengthWithRightBrace = endAfterCurly - module_->srcStart;

        UniqueModuleData base;
        UniqueStaticLinkData link;
        if (!mg_.finish(heap, muted, Move(filename), Move(displayURL), &base, &link, slowFuncs))
            return false;

        moduleObj.set(WasmModuleObject::create(cx_));
        if (!moduleObj)
            return false;

        return moduleObj->init(cx_->new_<AsmJSModule>(Move(base), Move(link), Move(module_)));
    }
};

/*****************************************************************************/
// Numeric literal utilities

static bool
IsNumericNonFloatLiteral(ParseNode* pn)
{
    // Note: '-' is never rolled into the number; numbers are always positive
    // and negations must be applied manually.
    return pn->isKind(PNK_NUMBER) ||
           (pn->isKind(PNK_NEG) && UnaryKid(pn)->isKind(PNK_NUMBER));
}

static bool
IsCallToGlobal(ModuleValidator& m, ParseNode* pn, const ModuleValidator::Global** global)
{
    if (!pn->isKind(PNK_CALL))
        return false;

    ParseNode* callee = CallCallee(pn);
    if (!callee->isKind(PNK_NAME))
        return false;

    *global = m.lookupGlobal(callee->name());
    return !!*global;
}

static bool
IsCoercionCall(ModuleValidator& m, ParseNode* pn, ValType* coerceTo, ParseNode** coercedExpr)
{
    const ModuleValidator::Global* global;
    if (!IsCallToGlobal(m, pn, &global))
        return false;

    if (CallArgListLength(pn) != 1)
        return false;

    if (coercedExpr)
        *coercedExpr = CallArgList(pn);

    if (global->isMathFunction() && global->mathBuiltinFunction() == AsmJSMathBuiltin_fround) {
        *coerceTo = ValType::F32;
        return true;
    }

    if (global->isSimdOperation() && global->simdOperation() == AsmJSSimdOperation_check) {
        switch (global->simdOperationType()) {
          case AsmJSSimdType_int32x4:
            *coerceTo = ValType::I32x4;
            return true;
          case AsmJSSimdType_float32x4:
            *coerceTo = ValType::F32x4;
            return true;
          case AsmJSSimdType_bool32x4:
            *coerceTo = ValType::B32x4;
            return true;
        }
    }

    return false;
}

static bool
IsFloatLiteral(ModuleValidator& m, ParseNode* pn)
{
    ParseNode* coercedExpr;
    ValType coerceTo;
    if (!IsCoercionCall(m, pn, &coerceTo, &coercedExpr))
        return false;
    // Don't fold into || to avoid clang/memcheck bug (bug 1077031).
    if (coerceTo != ValType::F32)
        return false;
    return IsNumericNonFloatLiteral(coercedExpr);
}

static unsigned
SimdTypeToLength(AsmJSSimdType type)
{
    switch (type) {
      case AsmJSSimdType_int32x4:
      case AsmJSSimdType_float32x4:
      case AsmJSSimdType_bool32x4:
        return 4;
    }
    MOZ_CRASH("unexpected SIMD type");
}

static bool
IsSimdTuple(ModuleValidator& m, ParseNode* pn, AsmJSSimdType* type)
{
    const ModuleValidator::Global* global;
    if (!IsCallToGlobal(m, pn, &global))
        return false;

    if (!global->isSimdCtor())
        return false;

    if (CallArgListLength(pn) != SimdTypeToLength(global->simdCtorType()))
        return false;

    *type = global->simdCtorType();
    return true;
}

static bool
IsNumericLiteral(ModuleValidator& m, ParseNode* pn);

static NumLit
ExtractNumericLiteral(ModuleValidator& m, ParseNode* pn);

static inline bool
IsLiteralInt(ModuleValidator& m, ParseNode* pn, uint32_t* u32);

static bool
IsSimdLiteral(ModuleValidator& m, ParseNode* pn)
{
    AsmJSSimdType type;
    if (!IsSimdTuple(m, pn, &type))
        return false;

    ParseNode* arg = CallArgList(pn);
    unsigned length = SimdTypeToLength(type);
    for (unsigned i = 0; i < length; i++) {
        if (!IsNumericLiteral(m, arg))
            return false;

        uint32_t _;
        switch (type) {
          case AsmJSSimdType_int32x4:
          case AsmJSSimdType_bool32x4:
            if (!IsLiteralInt(m, arg, &_))
                return false;
            MOZ_FALLTHROUGH;
          case AsmJSSimdType_float32x4:
            if (!IsNumericNonFloatLiteral(arg))
                return false;
        }

        arg = NextNode(arg);
    }

    MOZ_ASSERT(arg == nullptr);
    return true;
}

static bool
IsNumericLiteral(ModuleValidator& m, ParseNode* pn)
{
    return IsNumericNonFloatLiteral(pn) ||
           IsFloatLiteral(m, pn) ||
           IsSimdLiteral(m, pn);
}

// The JS grammar treats -42 as -(42) (i.e., with separate grammar
// productions) for the unary - and literal 42). However, the asm.js spec
// recognizes -42 (modulo parens, so -(42) and -((42))) as a single literal
// so fold the two potential parse nodes into a single double value.
static double
ExtractNumericNonFloatValue(ParseNode* pn, ParseNode** out = nullptr)
{
    MOZ_ASSERT(IsNumericNonFloatLiteral(pn));

    if (pn->isKind(PNK_NEG)) {
        pn = UnaryKid(pn);
        if (out)
            *out = pn;
        return -NumberNodeValue(pn);
    }

    return NumberNodeValue(pn);
}

static NumLit
ExtractSimdValue(ModuleValidator& m, ParseNode* pn)
{
    MOZ_ASSERT(IsSimdLiteral(m, pn));

    AsmJSSimdType type = AsmJSSimdType_int32x4;
    JS_ALWAYS_TRUE(IsSimdTuple(m, pn, &type));

    ParseNode* arg = CallArgList(pn);
    switch (type) {
      case AsmJSSimdType_int32x4: {
        MOZ_ASSERT(SimdTypeToLength(type) == 4);
        int32_t val[4];
        for (size_t i = 0; i < 4; i++, arg = NextNode(arg)) {
            uint32_t u32;
            JS_ALWAYS_TRUE(IsLiteralInt(m, arg, &u32));
            val[i] = int32_t(u32);
        }
        MOZ_ASSERT(arg== nullptr);
        return NumLit(NumLit::Int32x4, SimdConstant::CreateX4(val));
      }
      case AsmJSSimdType_float32x4: {
        MOZ_ASSERT(SimdTypeToLength(type) == 4);
        float val[4];
        for (size_t i = 0; i < 4; i++, arg = NextNode(arg))
            val[i] = float(ExtractNumericNonFloatValue(arg));
        MOZ_ASSERT(arg == nullptr);
        return NumLit(NumLit::Float32x4, SimdConstant::CreateX4(val));
      }
      case AsmJSSimdType_bool32x4: {
        MOZ_ASSERT(SimdTypeToLength(type) == 4);
        int32_t val[4];
        for (size_t i = 0; i < 4; i++, arg = NextNode(arg)) {
            uint32_t u32;
            JS_ALWAYS_TRUE(IsLiteralInt(m, arg, &u32));
            val[i] = u32 ? -1 : 0;
        }
        MOZ_ASSERT(arg == nullptr);
        return NumLit(NumLit::Bool32x4, SimdConstant::CreateX4(val));
      }
    }

    MOZ_CRASH("Unexpected SIMD type.");
}

static NumLit
ExtractNumericLiteral(ModuleValidator& m, ParseNode* pn)
{
    MOZ_ASSERT(IsNumericLiteral(m, pn));

    if (pn->isKind(PNK_CALL)) {
        // Float literals are explicitly coerced and thus the coerced literal may be
        // any valid (non-float) numeric literal.
        if (CallArgListLength(pn) == 1) {
            pn = CallArgList(pn);
            double d = ExtractNumericNonFloatValue(pn);
            return NumLit(NumLit::Float, DoubleValue(d));
        }

        MOZ_ASSERT(CallArgListLength(pn) == 4);
        return ExtractSimdValue(m, pn);
    }

    double d = ExtractNumericNonFloatValue(pn, &pn);

    // The asm.js spec syntactically distinguishes any literal containing a
    // decimal point or the literal -0 as having double type.
    if (NumberNodeHasFrac(pn) || IsNegativeZero(d))
        return NumLit(NumLit::Double, DoubleValue(d));

    // The syntactic checks above rule out these double values.
    MOZ_ASSERT(!IsNegativeZero(d));
    MOZ_ASSERT(!IsNaN(d));

    // Although doubles can only *precisely* represent 53-bit integers, they
    // can *imprecisely* represent integers much bigger than an int64_t.
    // Furthermore, d may be inf or -inf. In both cases, casting to an int64_t
    // is undefined, so test against the integer bounds using doubles.
    if (d < double(INT32_MIN) || d > double(UINT32_MAX))
        return NumLit(NumLit::OutOfRangeInt, UndefinedValue());

    // With the above syntactic and range limitations, d is definitely an
    // integer in the range [INT32_MIN, UINT32_MAX] range.
    int64_t i64 = int64_t(d);
    if (i64 >= 0) {
        if (i64 <= INT32_MAX)
            return NumLit(NumLit::Fixnum, Int32Value(i64));
        MOZ_ASSERT(i64 <= UINT32_MAX);
        return NumLit(NumLit::BigUnsigned, Int32Value(uint32_t(i64)));
    }
    MOZ_ASSERT(i64 >= INT32_MIN);
    return NumLit(NumLit::NegativeInt, Int32Value(i64));
}

static inline bool
IsLiteralInt(NumLit lit, uint32_t* u32)
{
    switch (lit.which()) {
      case NumLit::Fixnum:
      case NumLit::BigUnsigned:
      case NumLit::NegativeInt:
        *u32 = lit.toUint32();
        return true;
      case NumLit::Double:
      case NumLit::Float:
      case NumLit::OutOfRangeInt:
      case NumLit::Int32x4:
      case NumLit::Float32x4:
      case NumLit::Bool32x4:
        return false;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Bad literal type");
}

static inline bool
IsLiteralInt(ModuleValidator& m, ParseNode* pn, uint32_t* u32)
{
    return IsNumericLiteral(m, pn) &&
           IsLiteralInt(ExtractNumericLiteral(m, pn), u32);
}

/*****************************************************************************/

namespace {

// Encapsulates the building of an asm bytecode function from an asm.js function
// source code, packing the asm.js code into the asm bytecode form that can
// be decoded and compiled with a FunctionCompiler.
class MOZ_STACK_CLASS FunctionValidator
{
  public:
    struct Local
    {
        ValType type;
        unsigned slot;
        Local(ValType t, unsigned slot) : type(t), slot(slot) {}
    };

  private:
    typedef HashMap<PropertyName*, Local> LocalMap;
    typedef HashMap<PropertyName*, uint32_t> LabelMap;

    ModuleValidator&  m_;
    ParseNode*        fn_;

    FunctionGenerator fg_;
    Maybe<Encoder>    encoder_;

    LocalMap          locals_;
    LabelMap          labels_;

    bool              hasAlreadyReturned_;
    ExprType          ret_;

  public:
    FunctionValidator(ModuleValidator& m, ParseNode* fn)
      : m_(m),
        fn_(fn),
        locals_(m.cx()),
        labels_(m.cx()),
        hasAlreadyReturned_(false)
    {}

    ModuleValidator& m() const        { return m_; }
    ExclusiveContext* cx() const      { return m_.cx(); }
    ParseNode* fn() const             { return fn_; }

    bool init(PropertyName* name, unsigned line, unsigned column) {
        if (!locals_.init() || !labels_.init())
            return false;

        if (!m_.mg().startFuncDef(name, line, column, &fg_))
            return false;

        encoder_.emplace(fg_.bytecode());
        return true;
    }

    bool finish(uint32_t funcIndex, unsigned generateTime) {
        return m_.mg().finishFuncDef(funcIndex, generateTime, &fg_);
    }

    bool fail(ParseNode* pn, const char* str) {
        return m_.fail(pn, str);
    }

    bool failf(ParseNode* pn, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        m_.failfVAOffset(pn->pn_pos.begin, fmt, ap);
        va_end(ap);
        return false;
    }

    bool failName(ParseNode* pn, const char* fmt, PropertyName* name) {
        return m_.failName(pn, fmt, name);
    }

    /***************************************************** Local scope setup */

    bool addFormal(ParseNode* pn, PropertyName* name, ValType type) {
        LocalMap::AddPtr p = locals_.lookupForAdd(name);
        if (p)
            return failName(pn, "duplicate local name '%s' not allowed", name);
        return locals_.add(p, name, Local(type, locals_.count()));
    }

    bool addVariable(ParseNode* pn, PropertyName* name, ValType type) {
        return addFormal(pn, name, type) &&
               fg_.addVariable(type);
    }

    /****************************** For consistency of returns in a function */

    bool hasAlreadyReturned() const {
        return hasAlreadyReturned_;
    }

    ExprType returnedType() const {
        return ret_;
    }

    void setReturnedType(ExprType ret) {
        ret_ = ret;
        hasAlreadyReturned_ = true;
    }

    /**************************************************************** Labels */

    uint32_t lookupLabel(PropertyName* label) const {
        if (auto p = labels_.lookup(label))
            return p->value();
        return -1;
    }

    bool addLabel(PropertyName* label, uint32_t* id) {
        *id = labels_.count();
        return labels_.putNew(label, *id);
    }

    void removeLabel(PropertyName* label) {
        auto p = labels_.lookup(label);
        MOZ_ASSERT(!!p);
        labels_.remove(p);
    }

    /*************************************************** Read-only interface */

    const Local* lookupLocal(PropertyName* name) const {
        if (auto p = locals_.lookup(name))
            return &p->value();
        return nullptr;
    }

    const ModuleValidator::Global* lookupGlobal(PropertyName* name) const {
        if (locals_.has(name))
            return nullptr;
        return m_.lookupGlobal(name);
    }

    size_t numLocals() const { return locals_.count(); }

    /************************************************* Packing interface */

    Encoder& encoder() { return *encoder_; }

    bool noteLineCol(ParseNode* pn) {
        uint32_t line, column;
        m().tokenStream().srcCoords.lineNumAndColumnIndex(pn->pn_pos.begin, &line, &column);
        return fg_.addSourceCoords(encoder().bytecodeOffset(), line, column);
    }

    MOZ_WARN_UNUSED_RESULT
    bool writeOp(Expr op) {
        return encoder().writeExpr(op);
    }

    MOZ_WARN_UNUSED_RESULT
    bool writeDebugCheckPoint() {
#ifdef DEBUG
        return writeOp(Expr::DebugCheckPoint);
#endif
        return true;
    }

    MOZ_WARN_UNUSED_RESULT
    bool writeU8(uint8_t u8) {
        return encoder().writeU8(u8);
    }
    MOZ_WARN_UNUSED_RESULT
    bool writeVarU32(uint32_t u32) {
        return encoder().writeVarU32(u32);
    }
    MOZ_WARN_UNUSED_RESULT
    bool writeU32(uint32_t u32) {
        return encoder().writeU32(u32);
    }
    MOZ_WARN_UNUSED_RESULT
    bool writeI32(int32_t i32) {
        return encoder().writeI32(i32);
    }
    MOZ_WARN_UNUSED_RESULT
    bool writeInt32Lit(int32_t i32) {
        return writeOp(Expr::I32Const) && encoder().writeI32(i32);
    }

    MOZ_WARN_UNUSED_RESULT
    bool writeLit(NumLit lit) {
        switch (lit.which()) {
          case NumLit::Fixnum:
          case NumLit::NegativeInt:
          case NumLit::BigUnsigned:
            return writeInt32Lit(lit.toInt32());
          case NumLit::Float:
            return writeOp(Expr::F32Const) && encoder().writeF32(lit.toFloat());
          case NumLit::Double:
            return writeOp(Expr::F64Const) && encoder().writeF64(lit.toDouble());
          case NumLit::Int32x4:
            return writeOp(Expr::I32X4Const) && encoder().writeI32X4(lit.simdValue().asInt32x4());
          case NumLit::Float32x4:
            return writeOp(Expr::F32X4Const) && encoder().writeF32X4(lit.simdValue().asFloat32x4());
          case NumLit::Bool32x4:
            // Boolean vectors use the Int32x4 memory representation.
            return writeOp(Expr::B32X4Const) && encoder().writeI32X4(lit.simdValue().asInt32x4());
          case NumLit::OutOfRangeInt:
            break;
        }
        MOZ_CRASH("unexpected literal type");
    }

    void patchOp(size_t pos, Expr stmt) {
        encoder().patchExpr(pos, stmt);
    }
    void patchU8(size_t pos, uint8_t u8) {
        encoder().patchU8(pos, u8);
    }
    template<class T>
    void patch32(size_t pos, T val) {
        static_assert(sizeof(T) == sizeof(uint32_t), "patch32 is used for 4-bytes long ops");
        encoder().patch32(pos, val);
    }

    MOZ_WARN_UNUSED_RESULT
    bool tempU8(size_t* offset) {
        return encoder().writeU8(uint8_t(Expr::Unreachable), offset);
    }
    MOZ_WARN_UNUSED_RESULT
    bool tempOp(size_t* offset) {
        return encoder().writeExpr(Expr::Unreachable, offset);
    }
    MOZ_WARN_UNUSED_RESULT
    bool temp32(size_t* offset) {
        if (!encoder().writeU8(uint8_t(Expr::Unreachable), offset))
            return false;
        for (size_t i = 1; i < 4; i++) {
            if (!encoder().writeU8(uint8_t(Expr::Unreachable)))
                return false;
        }
        return true;
    }
};

} /* anonymous namespace */

/*****************************************************************************/
// asm.js type-checking and code-generation algorithm

static bool
CheckIdentifier(ModuleValidator& m, ParseNode* usepn, PropertyName* name)
{
    if (name == m.cx()->names().arguments || name == m.cx()->names().eval)
        return m.failName(usepn, "'%s' is not an allowed identifier", name);
    return true;
}

static bool
CheckModuleLevelName(ModuleValidator& m, ParseNode* usepn, PropertyName* name)
{
    if (!CheckIdentifier(m, usepn, name))
        return false;

    if (name == m.moduleFunctionName() ||
        name == m.globalArgumentName() ||
        name == m.importArgumentName() ||
        name == m.bufferArgumentName() ||
        m.lookupGlobal(name))
    {
        return m.failName(usepn, "duplicate name '%s' not allowed", name);
    }

    return true;
}

static bool
CheckFunctionHead(ModuleValidator& m, ParseNode* fn)
{
    JSFunction* fun = FunctionObject(fn);
    if (fun->hasRest())
        return m.fail(fn, "rest args not allowed");
    if (fun->isExprBody())
        return m.fail(fn, "expression closures not allowed");
    if (fn->pn_funbox->hasDestructuringArgs)
        return m.fail(fn, "destructuring args not allowed");
    return true;
}

static bool
CheckArgument(ModuleValidator& m, ParseNode* arg, PropertyName** name)
{
    if (!IsDefinition(arg))
        return m.fail(arg, "duplicate argument name not allowed");

    if (arg->isKind(PNK_ASSIGN))
        return m.fail(arg, "default arguments not allowed");

    if (!CheckIdentifier(m, arg, arg->name()))
        return false;

    *name = arg->name();
    return true;
}

static bool
CheckModuleArgument(ModuleValidator& m, ParseNode* arg, PropertyName** name)
{
    if (!CheckArgument(m, arg, name))
        return false;

    if (!CheckModuleLevelName(m, arg, *name))
        return false;

    return true;
}

static bool
CheckModuleArguments(ModuleValidator& m, ParseNode* fn)
{
    unsigned numFormals;
    ParseNode* arg1 = FunctionArgsList(fn, &numFormals);
    ParseNode* arg2 = arg1 ? NextNode(arg1) : nullptr;
    ParseNode* arg3 = arg2 ? NextNode(arg2) : nullptr;

    if (numFormals > 3)
        return m.fail(fn, "asm.js modules takes at most 3 argument");

    PropertyName* arg1Name = nullptr;
    if (arg1 && !CheckModuleArgument(m, arg1, &arg1Name))
        return false;
    m.initGlobalArgumentName(arg1Name);

    PropertyName* arg2Name = nullptr;
    if (arg2 && !CheckModuleArgument(m, arg2, &arg2Name))
        return false;
    m.initImportArgumentName(arg2Name);

    PropertyName* arg3Name = nullptr;
    if (arg3 && !CheckModuleArgument(m, arg3, &arg3Name))
        return false;
    m.initBufferArgumentName(arg3Name);

    return true;
}

static bool
CheckPrecedingStatements(ModuleValidator& m, ParseNode* stmtList)
{
    MOZ_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));

    ParseNode* stmt = ListHead(stmtList);
    for (unsigned i = 0, n = ListLength(stmtList); i < n; i++) {
        if (!IsIgnoredDirective(m.cx(), stmt))
            return m.fail(stmt, "invalid asm.js statement");
    }

    return true;
}

static bool
CheckGlobalVariableInitConstant(ModuleValidator& m, PropertyName* varName, ParseNode* initNode,
                                bool isConst)
{
    NumLit lit = ExtractNumericLiteral(m, initNode);
    if (!lit.valid())
        return m.fail(initNode, "global initializer is out of representable integer range");

    return m.addGlobalVarInit(varName, lit, isConst);
}

static bool
CheckTypeAnnotation(ModuleValidator& m, ParseNode* coercionNode, ValType* coerceTo,
                    ParseNode** coercedExpr = nullptr)
{
    switch (coercionNode->getKind()) {
      case PNK_BITOR: {
        ParseNode* rhs = BitwiseRight(coercionNode);
        uint32_t i;
        if (!IsLiteralInt(m, rhs, &i) || i != 0)
            return m.fail(rhs, "must use |0 for argument/return coercion");
        *coerceTo = ValType::I32;
        if (coercedExpr)
            *coercedExpr = BitwiseLeft(coercionNode);
        return true;
      }
      case PNK_POS: {
        *coerceTo = ValType::F64;
        if (coercedExpr)
            *coercedExpr = UnaryKid(coercionNode);
        return true;
      }
      case PNK_CALL: {
        if (IsCoercionCall(m, coercionNode, coerceTo, coercedExpr))
            return true;
        break;
      }
      default:;
    }

    return m.fail(coercionNode, "must be of the form +x, x|0, fround(x), or a SIMD check(x)");
}

static bool
CheckGlobalVariableImportExpr(ModuleValidator& m, PropertyName* varName, ValType coerceTo,
                              ParseNode* coercedExpr, bool isConst)
{
    if (!coercedExpr->isKind(PNK_DOT))
        return m.failName(coercedExpr, "invalid import expression for global '%s'", varName);

    ParseNode* base = DotBase(coercedExpr);
    PropertyName* field = DotMember(coercedExpr);

    PropertyName* importName = m.importArgumentName();
    if (!importName)
        return m.fail(coercedExpr, "cannot import without an asm.js foreign parameter");
    if (!IsUseOfName(base, importName))
        return m.failName(coercedExpr, "base of import expression must be '%s'", importName);

    return m.addGlobalVarImport(varName, field, coerceTo, isConst);
}

static bool
CheckGlobalVariableInitImport(ModuleValidator& m, PropertyName* varName, ParseNode* initNode,
                              bool isConst)
{
    ValType coerceTo;
    ParseNode* coercedExpr;
    if (!CheckTypeAnnotation(m, initNode, &coerceTo, &coercedExpr))
        return false;
    return CheckGlobalVariableImportExpr(m, varName, coerceTo, coercedExpr, isConst);
}

static bool
IsArrayViewCtorName(ModuleValidator& m, PropertyName* name, Scalar::Type* type)
{
    JSAtomState& names = m.cx()->names();
    if (name == names.Int8Array) {
        *type = Scalar::Int8;
    } else if (name == names.Uint8Array) {
        *type = Scalar::Uint8;
    } else if (name == names.Int16Array) {
        *type = Scalar::Int16;
    } else if (name == names.Uint16Array) {
        *type = Scalar::Uint16;
    } else if (name == names.Int32Array) {
        *type = Scalar::Int32;
    } else if (name == names.Uint32Array) {
        *type = Scalar::Uint32;
    } else if (name == names.Float32Array) {
        *type = Scalar::Float32;
    } else if (name == names.Float64Array) {
        *type = Scalar::Float64;
    } else {
        return false;
    }
    return true;
}

static bool
CheckNewArrayViewArgs(ModuleValidator& m, ParseNode* ctorExpr, PropertyName* bufferName)
{
    ParseNode* bufArg = NextNode(ctorExpr);
    if (!bufArg || NextNode(bufArg) != nullptr)
        return m.fail(ctorExpr, "array view constructor takes exactly one argument");

    if (!IsUseOfName(bufArg, bufferName))
        return m.failName(bufArg, "argument to array view constructor must be '%s'", bufferName);

    return true;
}

static bool
CheckNewArrayView(ModuleValidator& m, PropertyName* varName, ParseNode* newExpr)
{
    PropertyName* globalName = m.globalArgumentName();
    if (!globalName)
        return m.fail(newExpr, "cannot create array view without an asm.js global parameter");

    PropertyName* bufferName = m.bufferArgumentName();
    if (!bufferName)
        return m.fail(newExpr, "cannot create array view without an asm.js heap parameter");

    ParseNode* ctorExpr = ListHead(newExpr);

    PropertyName* field;
    Scalar::Type type;
    if (ctorExpr->isKind(PNK_DOT)) {
        ParseNode* base = DotBase(ctorExpr);

        if (!IsUseOfName(base, globalName))
            return m.failName(base, "expecting '%s.*Array", globalName);

        field = DotMember(ctorExpr);
        if (!IsArrayViewCtorName(m, field, &type))
            return m.fail(ctorExpr, "could not match typed array name");
    } else {
        if (!ctorExpr->isKind(PNK_NAME))
            return m.fail(ctorExpr, "expecting name of imported array view constructor");

        PropertyName* globalName = ctorExpr->name();
        const ModuleValidator::Global* global = m.lookupGlobal(globalName);
        if (!global)
            return m.failName(ctorExpr, "%s not found in module global scope", globalName);

        if (global->which() != ModuleValidator::Global::ArrayViewCtor)
            return m.failName(ctorExpr, "%s must be an imported array view constructor", globalName);

        field = nullptr;
        type = global->viewType();
    }

    if (!CheckNewArrayViewArgs(m, ctorExpr, bufferName))
        return false;

    return m.addArrayView(varName, type, field);
}

static bool
IsSimdTypeName(ModuleValidator& m, PropertyName* name, AsmJSSimdType* type)
{
    if (name == m.cx()->names().Int32x4) {
        *type = AsmJSSimdType_int32x4;
        return true;
    }
    if (name == m.cx()->names().Float32x4) {
        *type = AsmJSSimdType_float32x4;
        return true;
    }
    if (name == m.cx()->names().Bool32x4) {
        *type = AsmJSSimdType_bool32x4;
        return true;
    }
    return false;
}

static bool
IsSimdValidOperationType(AsmJSSimdType type, AsmJSSimdOperation op)
{
#define CASE(op) case AsmJSSimdOperation_##op:
    switch(type) {
      case AsmJSSimdType_int32x4:
        switch (op) {
          FORALL_INT32X4_ASMJS_OP(CASE) return true;
          default: return false;
        }
        break;
      case AsmJSSimdType_float32x4:
        switch (op) {
          FORALL_FLOAT32X4_ASMJS_OP(CASE) return true;
          default: return false;
        }
        break;
      case AsmJSSimdType_bool32x4:
        switch (op) {
          FORALL_BOOL_SIMD_OP(CASE) return true;
          default: return false;
        }
        break;
    }
#undef CASE
    MOZ_CRASH("Unhandles SIMD type");
}

static bool
CheckGlobalMathImport(ModuleValidator& m, ParseNode* initNode, PropertyName* varName,
                      PropertyName* field)
{
    // Math builtin, with the form glob.Math.[[builtin]]
    ModuleValidator::MathBuiltin mathBuiltin;
    if (!m.lookupStandardLibraryMathName(field, &mathBuiltin))
        return m.failName(initNode, "'%s' is not a standard Math builtin", field);

    switch (mathBuiltin.kind) {
      case ModuleValidator::MathBuiltin::Function:
        return m.addMathBuiltinFunction(varName, mathBuiltin.u.func, field);
      case ModuleValidator::MathBuiltin::Constant:
        return m.addMathBuiltinConstant(varName, mathBuiltin.u.cst, field);
      default:
        break;
    }
    MOZ_CRASH("unexpected or uninitialized math builtin type");
}

static bool
CheckGlobalAtomicsImport(ModuleValidator& m, ParseNode* initNode, PropertyName* varName,
                         PropertyName* field)
{
    // Atomics builtin, with the form glob.Atomics.[[builtin]]
    AsmJSAtomicsBuiltinFunction func;
    if (!m.lookupStandardLibraryAtomicsName(field, &func))
        return m.failName(initNode, "'%s' is not a standard Atomics builtin", field);

    return m.addAtomicsBuiltinFunction(varName, func, field);
}

static bool
CheckGlobalSimdImport(ModuleValidator& m, ParseNode* initNode, PropertyName* varName,
                      PropertyName* field)
{
    if (!m.supportsSimd())
        return m.fail(initNode, "SIMD is not supported on this platform");

    // SIMD constructor, with the form glob.SIMD.[[type]]
    AsmJSSimdType simdType;
    if (!IsSimdTypeName(m, field, &simdType))
        return m.failName(initNode, "'%s' is not a standard SIMD type", field);
    return m.addSimdCtor(varName, simdType, field);
}

static bool
CheckGlobalSimdOperationImport(ModuleValidator& m, const ModuleValidator::Global* global,
                               ParseNode* initNode, PropertyName* varName, PropertyName* opName)
{
    AsmJSSimdType simdType = global->simdCtorType();
    AsmJSSimdOperation simdOp;
    if (!m.lookupStandardSimdOpName(opName, &simdOp))
        return m.failName(initNode, "'%s' is not a standard SIMD operation", opName);
    if (!IsSimdValidOperationType(simdType, simdOp))
        return m.failName(initNode, "'%s' is not an operation supported by the SIMD type", opName);
    return m.addSimdOperation(varName, simdType, simdOp, opName);
}

static bool
CheckGlobalDotImport(ModuleValidator& m, PropertyName* varName, ParseNode* initNode)
{
    ParseNode* base = DotBase(initNode);
    PropertyName* field = DotMember(initNode);

    if (base->isKind(PNK_DOT)) {
        ParseNode* global = DotBase(base);
        PropertyName* mathOrAtomicsOrSimd = DotMember(base);

        PropertyName* globalName = m.globalArgumentName();
        if (!globalName)
            return m.fail(base, "import statement requires the module have a stdlib parameter");

        if (!IsUseOfName(global, globalName)) {
            if (global->isKind(PNK_DOT)) {
                return m.failName(base, "imports can have at most two dot accesses "
                                        "(e.g. %s.Math.sin)", globalName);
            }
            return m.failName(base, "expecting %s.*", globalName);
        }

        if (mathOrAtomicsOrSimd == m.cx()->names().Math)
            return CheckGlobalMathImport(m, initNode, varName, field);
        if (mathOrAtomicsOrSimd == m.cx()->names().Atomics)
            return CheckGlobalAtomicsImport(m, initNode, varName, field);
        if (mathOrAtomicsOrSimd == m.cx()->names().SIMD)
            return CheckGlobalSimdImport(m, initNode, varName, field);
        return m.failName(base, "expecting %s.{Math|SIMD}", globalName);
    }

    if (!base->isKind(PNK_NAME))
        return m.fail(base, "expected name of variable or parameter");

    if (base->name() == m.globalArgumentName()) {
        if (field == m.cx()->names().NaN)
            return m.addGlobalConstant(varName, GenericNaN(), field);
        if (field == m.cx()->names().Infinity)
            return m.addGlobalConstant(varName, PositiveInfinity<double>(), field);

        Scalar::Type type;
        if (IsArrayViewCtorName(m, field, &type))
            return m.addArrayViewCtor(varName, type, field);

        return m.failName(initNode, "'%s' is not a standard constant or typed array name", field);
    }

    if (base->name() == m.importArgumentName())
        return m.addFFI(varName, field);

    const ModuleValidator::Global* global = m.lookupGlobal(base->name());
    if (!global)
        return m.failName(initNode, "%s not found in module global scope", base->name());

    if (!global->isSimdCtor())
        return m.failName(base, "expecting SIMD constructor name, got %s", field);

    return CheckGlobalSimdOperationImport(m, global, initNode, varName, field);
}

static bool
CheckModuleGlobal(ModuleValidator& m, ParseNode* var, bool isConst)
{
    if (!IsDefinition(var))
        return m.fail(var, "import variable names must be unique");

    if (!CheckModuleLevelName(m, var, var->name()))
        return false;

    ParseNode* initNode = MaybeDefinitionInitializer(var);
    if (!initNode)
        return m.fail(var, "module import needs initializer");

    if (IsNumericLiteral(m, initNode))
        return CheckGlobalVariableInitConstant(m, var->name(), initNode, isConst);

    if (initNode->isKind(PNK_BITOR) || initNode->isKind(PNK_POS) || initNode->isKind(PNK_CALL))
        return CheckGlobalVariableInitImport(m, var->name(), initNode, isConst);

    if (initNode->isKind(PNK_NEW))
        return CheckNewArrayView(m, var->name(), initNode);

    if (initNode->isKind(PNK_DOT))
        return CheckGlobalDotImport(m, var->name(), initNode);

    return m.fail(initNode, "unsupported import expression");
}

static bool
CheckModuleProcessingDirectives(ModuleValidator& m)
{
    TokenStream& ts = m.parser().tokenStream;
    while (true) {
        bool matched;
        if (!ts.matchToken(&matched, TOK_STRING, TokenStream::Operand))
            return false;
        if (!matched)
            return true;

        if (!IsIgnoredDirectiveName(m.cx(), ts.currentToken().atom()))
            return m.failCurrentOffset("unsupported processing directive");

        TokenKind tt;
        if (!ts.getToken(&tt))
            return false;
        if (tt != TOK_SEMI)
            return m.failCurrentOffset("expected semicolon after string literal");
    }
}

static bool
CheckModuleGlobals(ModuleValidator& m)
{
    while (true) {
        ParseNode* varStmt;
        if (!ParseVarOrConstStatement(m.parser(), &varStmt))
            return false;
        if (!varStmt)
            break;
        for (ParseNode* var = VarListHead(varStmt); var; var = NextNode(var)) {
            if (!CheckModuleGlobal(m, var, varStmt->isKind(PNK_CONST)))
                return false;
        }
    }

    return true;
}

static bool
ArgFail(FunctionValidator& f, PropertyName* argName, ParseNode* stmt)
{
    return f.failName(stmt, "expecting argument type declaration for '%s' of the "
                      "form 'arg = arg|0' or 'arg = +arg' or 'arg = fround(arg)'", argName);
}

static bool
CheckArgumentType(FunctionValidator& f, ParseNode* stmt, PropertyName* name, ValType* type)
{
    if (!stmt || !IsExpressionStatement(stmt))
        return ArgFail(f, name, stmt ? stmt : f.fn());

    ParseNode* initNode = ExpressionStatementExpr(stmt);
    if (!initNode || !initNode->isKind(PNK_ASSIGN))
        return ArgFail(f, name, stmt);

    ParseNode* argNode = BinaryLeft(initNode);
    ParseNode* coercionNode = BinaryRight(initNode);

    if (!IsUseOfName(argNode, name))
        return ArgFail(f, name, stmt);

    ParseNode* coercedExpr;
    if (!CheckTypeAnnotation(f.m(), coercionNode, type, &coercedExpr))
        return false;

    if (!IsUseOfName(coercedExpr, name))
        return ArgFail(f, name, stmt);

    return true;
}

static bool
CheckProcessingDirectives(ModuleValidator& m, ParseNode** stmtIter)
{
    ParseNode* stmt = *stmtIter;

    while (stmt && IsIgnoredDirective(m.cx(), stmt))
        stmt = NextNode(stmt);

    *stmtIter = stmt;
    return true;
}

static bool
CheckArguments(FunctionValidator& f, ParseNode** stmtIter, ValTypeVector* argTypes)
{
    ParseNode* stmt = *stmtIter;

    unsigned numFormals;
    ParseNode* argpn = FunctionArgsList(f.fn(), &numFormals);

    for (unsigned i = 0; i < numFormals; i++, argpn = NextNode(argpn), stmt = NextNode(stmt)) {
        PropertyName* name;
        if (!CheckArgument(f.m(), argpn, &name))
            return false;

        ValType type;
        if (!CheckArgumentType(f, stmt, name, &type))
            return false;

        if (!argTypes->append(type))
            return false;

        if (!f.addFormal(argpn, name, type))
            return false;
    }

    *stmtIter = stmt;
    return true;
}

static bool
IsLiteralOrConst(FunctionValidator& f, ParseNode* pn, NumLit* lit)
{
    if (pn->isKind(PNK_NAME)) {
        const ModuleValidator::Global* global = f.lookupGlobal(pn->name());
        if (!global || global->which() != ModuleValidator::Global::ConstantLiteral)
            return false;

        *lit = global->constLiteralValue();
        return true;
    }

    if (!IsNumericLiteral(f.m(), pn))
        return false;

    *lit = ExtractNumericLiteral(f.m(), pn);
    return true;
}

static bool
CheckFinalReturn(FunctionValidator& f, ParseNode* lastNonEmptyStmt)
{
    if (!f.hasAlreadyReturned()) {
        f.setReturnedType(ExprType::Void);
        return f.writeOp(Expr::Return);
    }

    if (!lastNonEmptyStmt->isKind(PNK_RETURN)) {
        if (!IsVoid(f.returnedType()))
            return f.fail(lastNonEmptyStmt, "void incompatible with previous return type");

        return f.writeOp(Expr::Return);
    }

    return true;
}

static bool
SetLocal(FunctionValidator& f, Expr exprStmt, NumLit lit)
{
    return f.writeOp(exprStmt) &&
           f.writeOp(Expr::SetLocal) &&
           f.writeVarU32(f.numLocals()) &&
           f.writeLit(lit);
}

static bool
CheckVariable(FunctionValidator& f, ParseNode* var)
{
    if (!IsDefinition(var))
        return f.fail(var, "local variable names must not restate argument names");

    PropertyName* name = var->name();

    if (!CheckIdentifier(f.m(), var, name))
        return false;

    ParseNode* initNode = MaybeDefinitionInitializer(var);
    if (!initNode)
        return f.failName(var, "var '%s' needs explicit type declaration via an initial value", name);

    NumLit lit;
    if (!IsLiteralOrConst(f, initNode, &lit))
        return f.failName(var, "var '%s' initializer must be literal or const literal", name);

    if (!lit.valid())
        return f.failName(var, "var '%s' initializer out of range", name);

    switch (lit.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
      case NumLit::BigUnsigned:
        if (lit.toInt32() != 0 && !SetLocal(f, Expr::I32Expr, lit))
            return false;
        break;
      case NumLit::Double:
        if ((lit.toDouble() != 0.0 || IsNegativeZero(lit.toDouble())) &&
            !SetLocal(f, Expr::F64Expr, lit))
            return false;
        break;
      case NumLit::Float:
        if ((lit.toFloat() != 0.f || !IsNegativeZero(lit.toFloat())) &&
            !SetLocal(f, Expr::F32Expr, lit))
            return false;
        break;
      case NumLit::Int32x4:
        if (lit.simdValue() != SimdConstant::SplatX4(0) &&
            !SetLocal(f, Expr::I32X4Expr, lit))
            return false;
        break;
      case NumLit::Float32x4:
        if (lit.simdValue() != SimdConstant::SplatX4(0.f) &&
            !SetLocal(f, Expr::F32X4Expr, lit))
            return false;
        break;
      case NumLit::Bool32x4:
        if (lit.simdValue() != SimdConstant::SplatX4(0) &&
            !SetLocal(f, Expr::B32X4Expr, lit))
            return false;
        break;
      case NumLit::OutOfRangeInt:
        MOZ_CRASH("can't be here because of valid() check above");
    }

    return f.addVariable(var, name, lit.type());
}

static bool
CheckVariables(FunctionValidator& f, ParseNode** stmtIter)
{
    ParseNode* stmt = *stmtIter;

    for (; stmt && stmt->isKind(PNK_VAR); stmt = NextNonEmptyStatement(stmt)) {
        for (ParseNode* var = VarListHead(stmt); var; var = NextNode(var)) {
            if (!CheckVariable(f, var))
                return false;
        }
    }

    *stmtIter = stmt;
    return true;
}

static bool
CheckExpr(FunctionValidator& f, ParseNode* expr, Type* type);

static bool
CheckNumericLiteral(FunctionValidator& f, ParseNode* num, Type* type)
{
    NumLit lit = ExtractNumericLiteral(f.m(), num);
    if (!lit.valid())
        return f.fail(num, "numeric literal out of representable integer range");
    *type = Type::lit(lit);
    return f.writeLit(lit);
}

static bool
CheckVarRef(FunctionValidator& f, ParseNode* varRef, Type* type)
{
    PropertyName* name = varRef->name();

    if (const FunctionValidator::Local* local = f.lookupLocal(name)) {
        if (!f.writeOp(Expr::GetLocal))
            return false;
        MOZ_ASSERT(local->type != ValType::I64, "no int64 in asm.js");
        if (!f.writeVarU32(local->slot))
            return false;
        *type = Type::var(local->type);
        return true;
    }

    if (const ModuleValidator::Global* global = f.lookupGlobal(name)) {
        switch (global->which()) {
          case ModuleValidator::Global::ConstantLiteral:
            *type = global->varOrConstType();
            return f.writeLit(global->constLiteralValue());
          case ModuleValidator::Global::ConstantImport:
          case ModuleValidator::Global::Variable: {
            *type = global->varOrConstType();
            return f.writeOp(Expr::LoadGlobal) &&
                   f.writeVarU32(global->varOrConstGlobalDataOffset()) &&
                   f.writeU8(uint8_t(global->isConst()));
          }
          case ModuleValidator::Global::Function:
          case ModuleValidator::Global::FFI:
          case ModuleValidator::Global::MathBuiltinFunction:
          case ModuleValidator::Global::AtomicsBuiltinFunction:
          case ModuleValidator::Global::FuncPtrTable:
          case ModuleValidator::Global::ArrayView:
          case ModuleValidator::Global::ArrayViewCtor:
          case ModuleValidator::Global::SimdCtor:
          case ModuleValidator::Global::SimdOperation:
            break;
        }
        return f.failName(varRef, "'%s' may not be accessed by ordinary expressions", name);
    }

    return f.failName(varRef, "'%s' not found in local or asm.js module scope", name);
}

static inline bool
IsLiteralOrConstInt(FunctionValidator& f, ParseNode* pn, uint32_t* u32)
{
    NumLit lit;
    if (!IsLiteralOrConst(f, pn, &lit))
        return false;

    return IsLiteralInt(lit, u32);
}

static bool
FoldMaskedArrayIndex(FunctionValidator& f, ParseNode** indexExpr, int32_t* mask,
                     NeedsBoundsCheck* needsBoundsCheck)
{
    MOZ_ASSERT((*indexExpr)->isKind(PNK_BITAND));

    ParseNode* indexNode = BitwiseLeft(*indexExpr);
    ParseNode* maskNode = BitwiseRight(*indexExpr);

    uint32_t mask2;
    if (IsLiteralOrConstInt(f, maskNode, &mask2)) {
        // Flag the access to skip the bounds check if the mask ensures that an
        // 'out of bounds' access can not occur based on the current heap length
        // constraint. The unsigned maximum of a masked index is the mask
        // itself, so check that the mask is not negative and compare the mask
        // to the known minimum heap length.
        if (int32_t(mask2) >= 0 && mask2 < f.m().minHeapLength())
            *needsBoundsCheck = NO_BOUNDS_CHECK;
        *mask &= mask2;
        *indexExpr = indexNode;
        return true;
    }

    return false;
}

static const int32_t NoMask = -1;

static bool
CheckArrayAccess(FunctionValidator& f, ParseNode* viewName, ParseNode* indexExpr,
                 Scalar::Type* viewType, NeedsBoundsCheck* needsBoundsCheck, int32_t* mask)
{
    *needsBoundsCheck = NEEDS_BOUNDS_CHECK;

    if (!viewName->isKind(PNK_NAME))
        return f.fail(viewName, "base of array access must be a typed array view name");

    const ModuleValidator::Global* global = f.lookupGlobal(viewName->name());
    if (!global || !global->isAnyArrayView())
        return f.fail(viewName, "base of array access must be a typed array view name");

    *viewType = global->viewType();

    uint32_t index;
    if (IsLiteralOrConstInt(f, indexExpr, &index)) {
        uint64_t byteOffset = uint64_t(index) << TypedArrayShift(*viewType);
        if (!f.m().tryConstantAccess(byteOffset, TypedArrayElemSize(*viewType)))
            return f.fail(indexExpr, "constant index out of range");

        *mask = NoMask;
        *needsBoundsCheck = NO_BOUNDS_CHECK;
        return f.writeInt32Lit(byteOffset);
    }

    // Mask off the low bits to account for the clearing effect of a right shift
    // followed by the left shift implicit in the array access. E.g., H32[i>>2]
    // loses the low two bits.
    *mask = ~(TypedArrayElemSize(*viewType) - 1);

    if (indexExpr->isKind(PNK_RSH)) {
        ParseNode* shiftAmountNode = BitwiseRight(indexExpr);

        uint32_t shift;
        if (!IsLiteralInt(f.m(), shiftAmountNode, &shift))
            return f.failf(shiftAmountNode, "shift amount must be constant");

        unsigned requiredShift = TypedArrayShift(*viewType);
        if (shift != requiredShift)
            return f.failf(shiftAmountNode, "shift amount must be %u", requiredShift);

        ParseNode* pointerNode = BitwiseLeft(indexExpr);

        if (pointerNode->isKind(PNK_BITAND))
            FoldMaskedArrayIndex(f, &pointerNode, mask, needsBoundsCheck);

        Type pointerType;
        if (!CheckExpr(f, pointerNode, &pointerType))
            return false;

        if (!pointerType.isIntish())
            return f.failf(pointerNode, "%s is not a subtype of int", pointerType.toChars());
    } else {
        // For legacy compatibility, accept Int8/Uint8 accesses with no shift.
        if (TypedArrayShift(*viewType) != 0)
            return f.fail(indexExpr, "index expression isn't shifted; must be an Int8/Uint8 access");

        MOZ_ASSERT(*mask == NoMask);
        bool folded = false;

        ParseNode* pointerNode = indexExpr;

        if (pointerNode->isKind(PNK_BITAND))
            folded = FoldMaskedArrayIndex(f, &pointerNode, mask, needsBoundsCheck);

        Type pointerType;
        if (!CheckExpr(f, pointerNode, &pointerType))
            return false;

        if (folded) {
            if (!pointerType.isIntish())
                return f.failf(pointerNode, "%s is not a subtype of intish", pointerType.toChars());
        } else {
            if (!pointerType.isInt())
                return f.failf(pointerNode, "%s is not a subtype of int", pointerType.toChars());
        }
    }

    return true;
}

static bool
CheckAndPrepareArrayAccess(FunctionValidator& f, ParseNode* viewName, ParseNode* indexExpr,
                           Scalar::Type* viewType, NeedsBoundsCheck* needsBoundsCheck, int32_t* mask)
{
    size_t prepareAt;
    if (!f.tempOp(&prepareAt))
        return false;

    if (!CheckArrayAccess(f, viewName, indexExpr, viewType, needsBoundsCheck, mask))
        return false;

    // Don't generate the mask op if there is no need for it which could happen for
    // a shift of zero or a SIMD access.
    if (*mask != NoMask) {
        f.patchOp(prepareAt, Expr::I32And);
        return f.writeInt32Lit(*mask);
    }

    f.patchOp(prepareAt, Expr::Id);
    return true;
}

static bool
CheckLoadArray(FunctionValidator& f, ParseNode* elem, Type* type)
{
    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;

    size_t opcodeAt;
    size_t needsBoundsCheckAt;
    if (!f.tempOp(&opcodeAt) || !f.tempU8(&needsBoundsCheckAt))
        return false;

    if (!CheckAndPrepareArrayAccess(f, ElemBase(elem), ElemIndex(elem), &viewType, &needsBoundsCheck, &mask))
        return false;

    switch (viewType) {
      case Scalar::Int8:    f.patchOp(opcodeAt, Expr::I32LoadMem8S);  break;
      case Scalar::Uint8:   f.patchOp(opcodeAt, Expr::I32LoadMem8U);  break;
      case Scalar::Int16:   f.patchOp(opcodeAt, Expr::I32LoadMem16S); break;
      case Scalar::Uint16:  f.patchOp(opcodeAt, Expr::I32LoadMem16U); break;
      case Scalar::Uint32:
      case Scalar::Int32:   f.patchOp(opcodeAt, Expr::I32LoadMem);    break;
      case Scalar::Float32: f.patchOp(opcodeAt, Expr::F32LoadMem);    break;
      case Scalar::Float64: f.patchOp(opcodeAt, Expr::F64LoadMem);    break;
      default: MOZ_CRASH("unexpected scalar type");
    }

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));

    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Int16:
      case Scalar::Int32:
      case Scalar::Uint8:
      case Scalar::Uint16:
      case Scalar::Uint32:
        *type = Type::Intish;
        break;
      case Scalar::Float32:
        *type = Type::MaybeFloat;
        break;
      case Scalar::Float64:
        *type = Type::MaybeDouble;
        break;
      default: MOZ_CRASH("Unexpected array type");
    }

    return true;
}

static bool
CheckStoreArray(FunctionValidator& f, ParseNode* lhs, ParseNode* rhs, Type* type)
{
    size_t opcodeAt;
    size_t needsBoundsCheckAt;
    if (!f.tempOp(&opcodeAt) || !f.tempU8(&needsBoundsCheckAt))
        return false;

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckAndPrepareArrayAccess(f, ElemBase(lhs), ElemIndex(lhs), &viewType, &needsBoundsCheck, &mask))
        return false;

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Int16:
      case Scalar::Int32:
      case Scalar::Uint8:
      case Scalar::Uint16:
      case Scalar::Uint32:
        if (!rhsType.isIntish())
            return f.failf(lhs, "%s is not a subtype of intish", rhsType.toChars());
        break;
      case Scalar::Float32:
        if (!rhsType.isMaybeDouble() && !rhsType.isFloatish())
            return f.failf(lhs, "%s is not a subtype of double? or floatish", rhsType.toChars());
        break;
      case Scalar::Float64:
        if (!rhsType.isMaybeFloat() && !rhsType.isMaybeDouble())
            return f.failf(lhs, "%s is not a subtype of float? or double?", rhsType.toChars());
        break;
      default:
        MOZ_CRASH("Unexpected view type");
    }

    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Uint8:
        f.patchOp(opcodeAt, Expr::I32StoreMem8);
        break;
      case Scalar::Int16:
      case Scalar::Uint16:
        f.patchOp(opcodeAt, Expr::I32StoreMem16);
        break;
      case Scalar::Int32:
      case Scalar::Uint32:
        f.patchOp(opcodeAt, Expr::I32StoreMem);
        break;
      case Scalar::Float32:
        if (rhsType.isFloatish())
            f.patchOp(opcodeAt, Expr::F32StoreMem);
        else
            f.patchOp(opcodeAt, Expr::F64StoreMemF32);
        break;
      case Scalar::Float64:
        if (rhsType.isFloatish())
            f.patchOp(opcodeAt, Expr::F32StoreMemF64);
        else
            f.patchOp(opcodeAt, Expr::F64StoreMem);
        break;
      default: MOZ_CRASH("unexpected scalar type");
    }

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));

    *type = rhsType;
    return true;
}

static bool
CheckAssignName(FunctionValidator& f, ParseNode* lhs, ParseNode* rhs, Type* type)
{
    RootedPropertyName name(f.cx(), lhs->name());

    if (const FunctionValidator::Local* lhsVar = f.lookupLocal(name)) {
        if (!f.writeOp(Expr::SetLocal) || !f.writeVarU32(lhsVar->slot))
            return false;

        Type rhsType;
        if (!CheckExpr(f, rhs, &rhsType))
            return false;

        if (!(rhsType <= lhsVar->type)) {
            return f.failf(lhs, "%s is not a subtype of %s",
                           rhsType.toChars(), Type::var(lhsVar->type).toChars());
        }
        MOZ_ASSERT(lhsVar->type != ValType::I64, "no int64 in asm.js");
        *type = rhsType;
        return true;
    }

    if (const ModuleValidator::Global* global = f.lookupGlobal(name)) {
        if (global->which() != ModuleValidator::Global::Variable)
            return f.failName(lhs, "'%s' is not a mutable variable", name);

        if (!f.writeOp(Expr::StoreGlobal) || !f.writeVarU32(global->varOrConstGlobalDataOffset()))
            return false;

        Type rhsType;
        if (!CheckExpr(f, rhs, &rhsType))
            return false;

        Type globType = global->varOrConstType();
        if (!(rhsType <= globType))
            return f.failf(lhs, "%s is not a subtype of %s", rhsType.toChars(), globType.toChars());
        *type = rhsType;
        return true;
    }

    return f.failName(lhs, "'%s' not found in local or asm.js module scope", name);
}

static bool
CheckAssign(FunctionValidator& f, ParseNode* assign, Type* type)
{
    MOZ_ASSERT(assign->isKind(PNK_ASSIGN));

    ParseNode* lhs = BinaryLeft(assign);
    ParseNode* rhs = BinaryRight(assign);

    if (lhs->getKind() == PNK_ELEM)
        return CheckStoreArray(f, lhs, rhs, type);

    if (lhs->getKind() == PNK_NAME)
        return CheckAssignName(f, lhs, rhs, type);

    return f.fail(assign, "left-hand side of assignment must be a variable or array access");
}

static bool
CheckMathIMul(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 2)
        return f.fail(call, "Math.imul must be passed 2 arguments");

    ParseNode* lhs = CallArgList(call);
    ParseNode* rhs = NextNode(lhs);

    if (!f.writeOp(Expr::I32Mul))
        return false;

    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (!lhsType.isIntish())
        return f.failf(lhs, "%s is not a subtype of intish", lhsType.toChars());
    if (!rhsType.isIntish())
        return f.failf(rhs, "%s is not a subtype of intish", rhsType.toChars());

    *type = Type::Signed;
    return true;
}

static bool
CheckMathClz32(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Math.clz32 must be passed 1 argument");

    if (!f.writeOp(Expr::I32Clz))
        return false;

    ParseNode* arg = CallArgList(call);

    Type argType;
    if (!CheckExpr(f, arg, &argType))
        return false;

    if (!argType.isIntish())
        return f.failf(arg, "%s is not a subtype of intish", argType.toChars());

    *type = Type::Fixnum;
    return true;
}

static bool
CheckMathAbs(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Math.abs must be passed 1 argument");

    ParseNode* arg = CallArgList(call);

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type argType;
    if (!CheckExpr(f, arg, &argType))
        return false;

    if (argType.isSigned()) {
        f.patchOp(opcodeAt, Expr::I32Abs);
        *type = Type::Unsigned;
        return true;
    }

    if (argType.isMaybeDouble()) {
        f.patchOp(opcodeAt, Expr::F64Abs);
        *type = Type::Double;
        return true;
    }

    if (argType.isMaybeFloat()) {
        f.patchOp(opcodeAt, Expr::F32Abs);
        *type = Type::Floatish;
        return true;
    }

    return f.failf(call, "%s is not a subtype of signed, float? or double?", argType.toChars());
}

static bool
CheckMathSqrt(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Math.sqrt must be passed 1 argument");

    ParseNode* arg = CallArgList(call);

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type argType;
    if (!CheckExpr(f, arg, &argType))
        return false;

    if (argType.isMaybeDouble()) {
        f.patchOp(opcodeAt, Expr::F64Sqrt);
        *type = Type::Double;
        return true;
    }

    if (argType.isMaybeFloat()) {
        f.patchOp(opcodeAt, Expr::F32Sqrt);
        *type = Type::Floatish;
        return true;
    }

    return f.failf(call, "%s is neither a subtype of double? nor float?", argType.toChars());
}

static bool
CheckMathMinMax(FunctionValidator& f, ParseNode* callNode, bool isMax, Type* type)
{
    if (CallArgListLength(callNode) < 2)
        return f.fail(callNode, "Math.min/max must be passed at least 2 arguments");

    size_t opcodeAt;
    size_t numArgsAt;
    if (!f.tempOp(&opcodeAt) || !f.tempU8(&numArgsAt))
        return false;

    ParseNode* firstArg = CallArgList(callNode);
    Type firstType;
    if (!CheckExpr(f, firstArg, &firstType))
        return false;

    if (firstType.isMaybeDouble()) {
        *type = Type::Double;
        firstType = Type::MaybeDouble;
        f.patchOp(opcodeAt, isMax ? Expr::F64Max : Expr::F64Min);
    } else if (firstType.isMaybeFloat()) {
        *type = Type::Float;
        firstType = Type::MaybeFloat;
        f.patchOp(opcodeAt, isMax ? Expr::F32Max : Expr::F32Min);
    } else if (firstType.isSigned()) {
        *type = Type::Signed;
        firstType = Type::Signed;
        f.patchOp(opcodeAt, isMax ? Expr::I32Max : Expr::I32Min);
    } else {
        return f.failf(firstArg, "%s is not a subtype of double?, float? or signed",
                       firstType.toChars());
    }

    unsigned numArgs = CallArgListLength(callNode);
    f.patchU8(numArgsAt, numArgs);

    ParseNode* nextArg = NextNode(firstArg);
    for (unsigned i = 1; i < numArgs; i++, nextArg = NextNode(nextArg)) {
        Type nextType;
        if (!CheckExpr(f, nextArg, &nextType))
            return false;
        if (!(nextType <= firstType))
            return f.failf(nextArg, "%s is not a subtype of %s", nextType.toChars(), firstType.toChars());
    }

    return true;
}

static bool
CheckSharedArrayAtomicAccess(FunctionValidator& f, ParseNode* viewName, ParseNode* indexExpr,
                             Scalar::Type* viewType, NeedsBoundsCheck* needsBoundsCheck,
                             int32_t* mask)
{
    if (!CheckAndPrepareArrayAccess(f, viewName, indexExpr, viewType, needsBoundsCheck, mask))
        return false;

    // The global will be sane, CheckArrayAccess checks it.
    const ModuleValidator::Global* global = f.lookupGlobal(viewName->name());
    if (global->which() != ModuleValidator::Global::ArrayView)
        return f.fail(viewName, "base of array access must be a typed array view");

    MOZ_ASSERT(f.m().atomicsPresent());

    switch (*viewType) {
      case Scalar::Int8:
      case Scalar::Int16:
      case Scalar::Int32:
      case Scalar::Uint8:
      case Scalar::Uint16:
      case Scalar::Uint32:
        return true;
      default:
        return f.failf(viewName, "not an integer array");
    }

    return true;
}

static bool
CheckAtomicsFence(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 0)
        return f.fail(call, "Atomics.fence must be passed 0 arguments");

    *type = Type::Void;
    return f.writeOp(Expr::AtomicsFence);
}

static bool
WriteAtomicOperator(FunctionValidator& f, Expr opcode, size_t* needsBoundsCheckAt, size_t* viewTypeAt)
{
    return f.writeOp(opcode) &&
           f.tempU8(needsBoundsCheckAt) &&
           f.tempU8(viewTypeAt);
}

static bool
CheckAtomicsLoad(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 2)
        return f.fail(call, "Atomics.load must be passed 2 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);

    size_t needsBoundsCheckAt;
    size_t viewTypeAt;
    if (!WriteAtomicOperator(f, Expr::I32AtomicsLoad, &needsBoundsCheckAt, &viewTypeAt))
        return false;

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsStore(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 3)
        return f.fail(call, "Atomics.store must be passed 3 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);
    ParseNode* valueArg = NextNode(indexArg);

    size_t needsBoundsCheckAt;
    size_t viewTypeAt;
    if (!WriteAtomicOperator(f, Expr::I32AtomicsStore, &needsBoundsCheckAt, &viewTypeAt))
        return false;

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    Type rhsType;
    if (!CheckExpr(f, valueArg, &rhsType))
        return false;

    if (!rhsType.isIntish())
        return f.failf(arrayArg, "%s is not a subtype of intish", rhsType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = rhsType;
    return true;
}

static bool
CheckAtomicsBinop(FunctionValidator& f, ParseNode* call, Type* type, AtomicOp op)
{
    if (CallArgListLength(call) != 3)
        return f.fail(call, "Atomics binary operator must be passed 3 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);
    ParseNode* valueArg = NextNode(indexArg);

    size_t needsBoundsCheckAt;
    size_t viewTypeAt;
    if (!WriteAtomicOperator(f, Expr::I32AtomicsBinOp, &needsBoundsCheckAt, &viewTypeAt))
        return false;
    if (!f.writeU8(uint8_t(op)))
        return false;

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    Type valueArgType;
    if (!CheckExpr(f, valueArg, &valueArgType))
        return false;

    if (!valueArgType.isIntish())
        return f.failf(valueArg, "%s is not a subtype of intish", valueArgType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsIsLockFree(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Atomics.isLockFree must be passed 1 argument");

    ParseNode* sizeArg = CallArgList(call);

    uint32_t size;
    if (!IsLiteralInt(f.m(), sizeArg, &size))
        return f.fail(sizeArg, "Atomics.isLockFree requires an integer literal argument");

    *type = Type::Int;
    return f.writeInt32Lit(AtomicOperations::isLockfree(size));
}

static bool
CheckAtomicsCompareExchange(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 4)
        return f.fail(call, "Atomics.compareExchange must be passed 4 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);
    ParseNode* oldValueArg = NextNode(indexArg);
    ParseNode* newValueArg = NextNode(oldValueArg);

    size_t needsBoundsCheckAt;
    size_t viewTypeAt;
    if (!WriteAtomicOperator(f, Expr::I32AtomicsCompareExchange, &needsBoundsCheckAt, &viewTypeAt))
        return false;

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    Type oldValueArgType;
    if (!CheckExpr(f, oldValueArg, &oldValueArgType))
        return false;

    Type newValueArgType;
    if (!CheckExpr(f, newValueArg, &newValueArgType))
        return false;

    if (!oldValueArgType.isIntish())
        return f.failf(oldValueArg, "%s is not a subtype of intish", oldValueArgType.toChars());

    if (!newValueArgType.isIntish())
        return f.failf(newValueArg, "%s is not a subtype of intish", newValueArgType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsExchange(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 3)
        return f.fail(call, "Atomics.exchange must be passed 3 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);
    ParseNode* valueArg = NextNode(indexArg);

    size_t needsBoundsCheckAt;
    size_t viewTypeAt;
    if (!WriteAtomicOperator(f, Expr::I32AtomicsExchange, &needsBoundsCheckAt, &viewTypeAt))
        return false;

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    Type valueArgType;
    if (!CheckExpr(f, valueArg, &valueArgType))
        return false;

    if (!valueArgType.isIntish())
        return f.failf(arrayArg, "%s is not a subtype of intish", valueArgType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsBuiltinCall(FunctionValidator& f, ParseNode* callNode, AsmJSAtomicsBuiltinFunction func,
                        Type* type)
{
    switch (func) {
      case AsmJSAtomicsBuiltin_compareExchange:
        return CheckAtomicsCompareExchange(f, callNode, type);
      case AsmJSAtomicsBuiltin_exchange:
        return CheckAtomicsExchange(f, callNode, type);
      case AsmJSAtomicsBuiltin_load:
        return CheckAtomicsLoad(f, callNode, type);
      case AsmJSAtomicsBuiltin_store:
        return CheckAtomicsStore(f, callNode, type);
      case AsmJSAtomicsBuiltin_fence:
        return CheckAtomicsFence(f, callNode, type);
      case AsmJSAtomicsBuiltin_add:
        return CheckAtomicsBinop(f, callNode, type, AtomicFetchAddOp);
      case AsmJSAtomicsBuiltin_sub:
        return CheckAtomicsBinop(f, callNode, type, AtomicFetchSubOp);
      case AsmJSAtomicsBuiltin_and:
        return CheckAtomicsBinop(f, callNode, type, AtomicFetchAndOp);
      case AsmJSAtomicsBuiltin_or:
        return CheckAtomicsBinop(f, callNode, type, AtomicFetchOrOp);
      case AsmJSAtomicsBuiltin_xor:
        return CheckAtomicsBinop(f, callNode, type, AtomicFetchXorOp);
      case AsmJSAtomicsBuiltin_isLockFree:
        return CheckAtomicsIsLockFree(f, callNode, type);
      default:
        MOZ_CRASH("unexpected atomicsBuiltin function");
    }
}

typedef bool (*CheckArgType)(FunctionValidator& f, ParseNode* argNode, Type type);

template <CheckArgType checkArg>
static bool
CheckCallArgs(FunctionValidator& f, ParseNode* callNode, ValTypeVector* args)
{
    ParseNode* argNode = CallArgList(callNode);
    for (unsigned i = 0; i < CallArgListLength(callNode); i++, argNode = NextNode(argNode)) {
        Type type;
        if (!CheckExpr(f, argNode, &type))
            return false;

        if (!checkArg(f, argNode, type))
            return false;

        if (!args->append(type.checkedValueType()))
            return false;
    }
    return true;
}

static bool
CheckSignatureAgainstExisting(ModuleValidator& m, ParseNode* usepn, const Sig& sig, const Sig& existing)
{
    if (sig.args().length() != existing.args().length()) {
        return m.failf(usepn, "incompatible number of arguments (%u here vs. %u before)",
                       sig.args().length(), existing.args().length());
    }

    for (unsigned i = 0; i < sig.args().length(); i++) {
        if (sig.arg(i) != existing.arg(i)) {
            return m.failf(usepn, "incompatible type for argument %u: (%s here vs. %s before)",
                           i, Type::var(sig.arg(i)).toChars(), Type::var(existing.arg(i)).toChars());
        }
    }

    if (sig.ret() != existing.ret()) {
        return m.failf(usepn, "%s incompatible with previous return of type %s",
                       Type::ret(sig.ret()).toChars(), Type::ret(existing.ret()).toChars());
    }

    MOZ_ASSERT(sig == existing);
    return true;
}

static bool
CheckFunctionSignature(ModuleValidator& m, ParseNode* usepn, Sig&& sig, PropertyName* name,
                       ModuleValidator::Func** func)
{
    ModuleValidator::Func* existing = m.lookupFunction(name);
    if (!existing) {
        if (!CheckModuleLevelName(m, usepn, name))
            return false;
        return m.addFunction(name, usepn->pn_pos.begin, Move(sig), func);
    }

    if (!CheckSignatureAgainstExisting(m, usepn, sig, m.mg().funcSig(existing->index())))
        return false;

    *func = existing;
    return true;
}

static bool
CheckIsVarType(FunctionValidator& f, ParseNode* argNode, Type type)
{
    if (!type.isVarType())
        return f.failf(argNode, "%s is not a subtype of int, float or double", type.toChars());
    return true;
}

static bool
CheckInternalCall(FunctionValidator& f, ParseNode* callNode, PropertyName* calleeName,
                  ExprType ret, Type* type)
{
    if (!f.writeOp(Expr::CallInternal))
        return false;

    // Function's index, to find out the function's entry
    size_t funcIndexAt;
    if (!f.temp32(&funcIndexAt))
        return false;

    if (!f.noteLineCol(callNode))
        return false;

    ValTypeVector args;
    if (!CheckCallArgs<CheckIsVarType>(f, callNode, &args))
        return false;

    ModuleValidator::Func* callee;
    if (!CheckFunctionSignature(f.m(), callNode, Sig(Move(args), ret), calleeName, &callee))
        return false;

    f.patch32(funcIndexAt, callee->index());
    *type = Type::ret(ret);
    return true;
}

static bool
CheckFuncPtrTableAgainstExisting(ModuleValidator& m, ParseNode* usepn, PropertyName* name,
                                 Sig&& sig, unsigned mask, uint32_t* funcPtrTableIndex)
{
    if (const ModuleValidator::Global* existing = m.lookupGlobal(name)) {
        if (existing->which() != ModuleValidator::Global::FuncPtrTable)
            return m.failName(usepn, "'%s' is not a function-pointer table", name);

        ModuleValidator::FuncPtrTable& table = m.funcPtrTable(existing->funcPtrTableIndex());
        if (mask != table.mask())
            return m.failf(usepn, "mask does not match previous value (%u)", table.mask());

        if (!CheckSignatureAgainstExisting(m, usepn, sig, m.mg().sig(table.sigIndex())))
            return false;

        *funcPtrTableIndex = existing->funcPtrTableIndex();
        return true;
    }

    if (!CheckModuleLevelName(m, usepn, name))
        return false;

    if (!m.declareFuncPtrTable(Move(sig), name, usepn->pn_pos.begin, mask, funcPtrTableIndex))
        return m.fail(usepn, "table too big");

    return true;
}

static bool
CheckFuncPtrCall(FunctionValidator& f, ParseNode* callNode, ExprType ret, Type* type)
{
    ParseNode* callee = CallCallee(callNode);
    ParseNode* tableNode = ElemBase(callee);
    ParseNode* indexExpr = ElemIndex(callee);

    if (!tableNode->isKind(PNK_NAME))
        return f.fail(tableNode, "expecting name of function-pointer array");

    PropertyName* name = tableNode->name();
    if (const ModuleValidator::Global* existing = f.lookupGlobal(name)) {
        if (existing->which() != ModuleValidator::Global::FuncPtrTable)
            return f.failName(tableNode, "'%s' is not the name of a function-pointer array", name);
    }

    if (!indexExpr->isKind(PNK_BITAND))
        return f.fail(indexExpr, "function-pointer table index expression needs & mask");

    ParseNode* indexNode = BitwiseLeft(indexExpr);
    ParseNode* maskNode = BitwiseRight(indexExpr);

    uint32_t mask;
    if (!IsLiteralInt(f.m(), maskNode, &mask) || mask == UINT32_MAX || !IsPowerOfTwo(mask + 1))
        return f.fail(maskNode, "function-pointer table index mask value must be a power of two minus 1");

    // Opcode
    if (!f.writeOp(Expr::CallIndirect))
        return false;

    // Table's mask
    if (!f.writeU32(mask))
        return false;

    // Global data offset
    size_t globalDataOffsetAt;
    if (!f.temp32(&globalDataOffsetAt))
        return false;

    // Call signature
    size_t sigIndexAt;
    if (!f.temp32(&sigIndexAt))
        return false;

    if (!f.noteLineCol(callNode))
        return false;

    Type indexType;
    if (!CheckExpr(f, indexNode, &indexType))
        return false;

    if (!indexType.isIntish())
        return f.failf(indexNode, "%s is not a subtype of intish", indexType.toChars());

    ValTypeVector args;
    if (!CheckCallArgs<CheckIsVarType>(f, callNode, &args))
        return false;

    Sig sig(Move(args), ret);

    uint32_t tableIndex;
    if (!CheckFuncPtrTableAgainstExisting(f.m(), tableNode, name, Move(sig), mask, &tableIndex))
        return false;

    f.patch32(globalDataOffsetAt, f.m().mg().funcPtrTableGlobalDataOffset(tableIndex));
    f.patch32(sigIndexAt, f.m().funcPtrTable(tableIndex).sigIndex());

    *type = Type::ret(ret);
    return true;
}

static bool
CheckIsExternType(FunctionValidator& f, ParseNode* argNode, Type type)
{
    if (!type.isExtern())
        return f.failf(argNode, "%s is not a subtype of extern", type.toChars());
    return true;
}

static bool
CheckFFICall(FunctionValidator& f, ParseNode* callNode, unsigned ffiIndex, ExprType ret,
             Type* type)
{
    PropertyName* calleeName = CallCallee(callNode)->name();

    if (ret == ExprType::F32)
        return f.fail(callNode, "FFI calls can't return float");
    if (IsSimdType(ret))
        return f.fail(callNode, "FFI calls can't return SIMD values");

    // Opcode
    if (!f.writeOp(Expr::CallImport))
        return false;

    // Import index
    size_t importIndexAt;
    if (!f.temp32(&importIndexAt))
        return false;

    if (!f.noteLineCol(callNode))
        return false;

    ValTypeVector args;
    if (!CheckCallArgs<CheckIsExternType>(f, callNode, &args))
        return false;

    uint32_t importIndex;
    if (!f.m().declareImport(calleeName, Sig(Move(args), ret), ffiIndex, &importIndex))
        return false;

    f.patch32(importIndexAt, importIndex);

    *type = Type::ret(ret);
    return true;
}

static bool
CheckFloatCoercionArg(FunctionValidator& f, ParseNode* inputNode, Type inputType,
                      size_t opcodeAt)
{
    if (inputType.isMaybeDouble()) {
        f.patchOp(opcodeAt, Expr::F32FromF64);
        return true;
    }
    if (inputType.isSigned()) {
        f.patchOp(opcodeAt, Expr::F32FromS32);
        return true;
    }
    if (inputType.isUnsigned()) {
        f.patchOp(opcodeAt, Expr::F32FromU32);
        return true;
    }
    if (inputType.isFloatish()) {
        f.patchOp(opcodeAt, Expr::Id);
        return true;
    }

    return f.failf(inputNode, "%s is not a subtype of signed, unsigned, double? or floatish",
                   inputType.toChars());
}

static bool
CheckCoercedCall(FunctionValidator& f, ParseNode* call, ExprType ret, Type* type);

static bool
CheckCoercionArg(FunctionValidator& f, ParseNode* arg, ValType expected, Type* type)
{
    ExprType ret = ToExprType(expected);
    if (arg->isKind(PNK_CALL))
        return CheckCoercedCall(f, arg, ret, type);

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type argType;
    if (!CheckExpr(f, arg, &argType))
        return false;

    switch (expected) {
      case ValType::F32:
        if (!CheckFloatCoercionArg(f, arg, argType, opcodeAt))
            return false;
        break;
      case ValType::I64:
        MOZ_CRASH("no int64 in asm.js");
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B32x4:
        if (!(argType <= expected))
            return f.fail(arg, "argument to SIMD coercion isn't from the correct SIMD type");
        f.patchOp(opcodeAt, Expr::Id);
        break;
      case ValType::I32:
      case ValType::F64:
        MOZ_CRASH("not call coercions");
    }

    *type = Type::ret(ret);
    return true;
}

static bool
CheckMathFRound(FunctionValidator& f, ParseNode* callNode, Type* type)
{
    if (CallArgListLength(callNode) != 1)
        return f.fail(callNode, "Math.fround must be passed 1 argument");

    ParseNode* argNode = CallArgList(callNode);
    Type argType;
    if (!CheckCoercionArg(f, argNode, ValType::F32, &argType))
        return false;

    MOZ_ASSERT(argType == Type::Float);
    *type = Type::Float;
    return true;
}

static bool
CheckMathBuiltinCall(FunctionValidator& f, ParseNode* callNode, AsmJSMathBuiltinFunction func,
                     Type* type)
{
    unsigned arity = 0;
    Expr f32;
    Expr f64;
    switch (func) {
      case AsmJSMathBuiltin_imul:   return CheckMathIMul(f, callNode, type);
      case AsmJSMathBuiltin_clz32:  return CheckMathClz32(f, callNode, type);
      case AsmJSMathBuiltin_abs:    return CheckMathAbs(f, callNode, type);
      case AsmJSMathBuiltin_sqrt:   return CheckMathSqrt(f, callNode, type);
      case AsmJSMathBuiltin_fround: return CheckMathFRound(f, callNode, type);
      case AsmJSMathBuiltin_min:    return CheckMathMinMax(f, callNode, /* isMax = */ false, type);
      case AsmJSMathBuiltin_max:    return CheckMathMinMax(f, callNode, /* isMax = */ true, type);
      case AsmJSMathBuiltin_ceil:   arity = 1; f64 = Expr::F64Ceil;  f32 = Expr::F32Ceil;     break;
      case AsmJSMathBuiltin_floor:  arity = 1; f64 = Expr::F64Floor; f32 = Expr::F32Floor;    break;
      case AsmJSMathBuiltin_sin:    arity = 1; f64 = Expr::F64Sin;   f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_cos:    arity = 1; f64 = Expr::F64Cos;   f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_tan:    arity = 1; f64 = Expr::F64Tan;   f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_asin:   arity = 1; f64 = Expr::F64Asin;  f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_acos:   arity = 1; f64 = Expr::F64Acos;  f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_atan:   arity = 1; f64 = Expr::F64Atan;  f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_exp:    arity = 1; f64 = Expr::F64Exp;   f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_log:    arity = 1; f64 = Expr::F64Log;   f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_pow:    arity = 2; f64 = Expr::F64Pow;   f32 = Expr::Unreachable; break;
      case AsmJSMathBuiltin_atan2:  arity = 2; f64 = Expr::F64Atan2; f32 = Expr::Unreachable; break;
      default: MOZ_CRASH("unexpected mathBuiltin function");
    }

    unsigned actualArity = CallArgListLength(callNode);
    if (actualArity != arity)
        return f.failf(callNode, "call passed %u arguments, expected %u", actualArity, arity);

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    if (!f.noteLineCol(callNode))
        return false;

    Type firstType;
    ParseNode* argNode = CallArgList(callNode);
    if (!CheckExpr(f, argNode, &firstType))
        return false;

    if (!firstType.isMaybeFloat() && !firstType.isMaybeDouble())
        return f.fail(argNode, "arguments to math call should be a subtype of double? or float?");

    bool opIsDouble = firstType.isMaybeDouble();
    if (!opIsDouble && f32 == Expr::Unreachable)
        return f.fail(callNode, "math builtin cannot be used as float");

    if (opIsDouble)
        f.patchOp(opcodeAt, f64);
    else
        f.patchOp(opcodeAt, f32);

    if (arity == 2) {
        Type secondType;
        argNode = NextNode(argNode);
        if (!CheckExpr(f, argNode, &secondType))
            return false;

        if (firstType.isMaybeDouble() && !secondType.isMaybeDouble())
            return f.fail(argNode, "both arguments to math builtin call should be the same type");
        if (firstType.isMaybeFloat() && !secondType.isMaybeFloat())
            return f.fail(argNode, "both arguments to math builtin call should be the same type");
    }

    *type = opIsDouble ? Type::Double : Type::Floatish;
    return true;
}

namespace {
// Include CheckSimdCallArgs in unnamed namespace to avoid MSVC name lookup bug.

template<class CheckArgOp>
static bool
CheckSimdCallArgs(FunctionValidator& f, ParseNode* call, unsigned expectedArity,
                  const CheckArgOp& checkArg)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != expectedArity)
        return f.failf(call, "expected %u arguments to SIMD call, got %u", expectedArity, numArgs);

    ParseNode* arg = CallArgList(call);
    for (size_t i = 0; i < numArgs; i++, arg = NextNode(arg)) {
        MOZ_ASSERT(!!arg);
        Type argType;
        if (!CheckExpr(f, arg, &argType))
            return false;
        if (!checkArg(f, arg, i, argType))
            return false;
    }

    return true;
}

template<class CheckArgOp>
static bool
CheckSimdCallArgsPatchable(FunctionValidator& f, ParseNode* call, unsigned expectedArity,
                           const CheckArgOp& checkArg)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != expectedArity)
        return f.failf(call, "expected %u arguments to SIMD call, got %u", expectedArity, numArgs);

    ParseNode* arg = CallArgList(call);
    for (size_t i = 0; i < numArgs; i++, arg = NextNode(arg)) {
        MOZ_ASSERT(!!arg);
        Type argType;
        size_t patchAt;
        if (!f.tempOp(&patchAt))
            return false;
        if (!CheckExpr(f, arg, &argType))
            return false;
        if (!checkArg(f, arg, i, argType, patchAt))
            return false;
    }

    return true;
}


class CheckArgIsSubtypeOf
{
    Type formalType_;

  public:
    explicit CheckArgIsSubtypeOf(AsmJSSimdType t) : formalType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType) const
    {
        if (!(actualType <= formalType_)) {
            return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                           formalType_.toChars());
        }
        return true;
    }
};

static inline Type
SimdToCoercedScalarType(AsmJSSimdType t)
{
    switch (t) {
      case AsmJSSimdType_int32x4:
      case AsmJSSimdType_bool32x4:
        return Type::Intish;
      case AsmJSSimdType_float32x4:
        return Type::Floatish;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected SIMD type");
}

class CheckSimdScalarArgs
{
    AsmJSSimdType simdType_;
    Type formalType_;

  public:
    explicit CheckSimdScalarArgs(AsmJSSimdType simdType)
      : simdType_(simdType), formalType_(SimdToCoercedScalarType(simdType))
    {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType,
                    size_t patchAt) const
    {
        if (!(actualType <= formalType_)) {
            // As a special case, accept doublelit arguments to float32x4 ops by
            // re-emitting them as float32 constants.
            if (simdType_ != AsmJSSimdType_float32x4 || !actualType.isDoubleLit()) {
                return f.failf(arg, "%s is not a subtype of %s%s",
                               actualType.toChars(), formalType_.toChars(),
                               simdType_ == AsmJSSimdType_float32x4 ? " or doublelit" : "");
            }

            // We emitted a double literal and actually want a float32.
            MOZ_ASSERT(patchAt != size_t(-1));
            f.patchOp(patchAt, Expr::F32FromF64);
            return true;
        }

        if (patchAt == size_t(-1))
            return true;

        f.patchOp(patchAt, Expr::Id);
        return true;
    }
};

class CheckSimdSelectArgs
{
    Type formalType_;

  public:
    explicit CheckSimdSelectArgs(AsmJSSimdType t) : formalType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType) const
    {
        if (argIndex == 0) {
            // First argument of select is a bool32x4 mask.
            if (!(actualType <= Type::Bool32x4))
                return f.failf(arg, "%s is not a subtype of Bool32x4", actualType.toChars());
            return true;
        }

        if (!(actualType <= formalType_)) {
            return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                           formalType_.toChars());
        }
        return true;
    }
};

class CheckSimdVectorScalarArgs
{
    AsmJSSimdType formalSimdType_;

  public:
    explicit CheckSimdVectorScalarArgs(AsmJSSimdType t) : formalSimdType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType,
                    size_t patchAt = -1) const
    {
        MOZ_ASSERT(argIndex < 2);
        if (argIndex == 0) {
            // First argument is the vector
            if (!(actualType <= Type(formalSimdType_))) {
                return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                               Type(formalSimdType_).toChars());
            }

            if (patchAt == size_t(-1))
                return true;

            f.patchOp(patchAt, Expr::Id);
            return true;
        }

        // Second argument is the scalar
        return CheckSimdScalarArgs(formalSimdType_)(f, arg, argIndex, actualType, patchAt);
    }
};

class CheckSimdExtractLaneArgs
{
    AsmJSSimdType formalSimdType_;

  public:
    explicit CheckSimdExtractLaneArgs(AsmJSSimdType t) : formalSimdType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType) const
    {
        MOZ_ASSERT(argIndex < 2);
        if (argIndex == 0) {
            // First argument is the vector
            if (!(actualType <= Type(formalSimdType_))) {
                return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                               Type(formalSimdType_).toChars());
            }
            return true;
        }

        uint32_t laneIndex;
        // Second argument is the lane < vector length
        if (!IsLiteralOrConstInt(f, arg, &laneIndex))
            return f.failf(arg, "lane selector should be a constant integer literal");
        if (laneIndex >= SimdTypeToLength(formalSimdType_))
            return f.failf(arg, "lane selector should be in bounds");
        return true;
    }
};

class CheckSimdReplaceLaneArgs
{
    AsmJSSimdType formalSimdType_;

  public:
    explicit CheckSimdReplaceLaneArgs(AsmJSSimdType t) : formalSimdType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType,
                    size_t patchAt) const
    {
        MOZ_ASSERT(argIndex < 3);
        uint32_t u32;
        switch (argIndex) {
          case 0:
            // First argument is the vector
            if (!(actualType <= Type(formalSimdType_))) {
                return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                               Type(formalSimdType_).toChars());
            }
            f.patchOp(patchAt, Expr::Id);
            return true;
          case 1:
            // Second argument is the lane (< vector length).
            if (!IsLiteralOrConstInt(f, arg, &u32))
                return f.failf(arg, "lane selector should be a constant integer literal");
            if (u32 >= SimdTypeToLength(formalSimdType_))
                return f.failf(arg, "lane selector should be in bounds");
            f.patchOp(patchAt, Expr::Id);
            return true;
          case 2:
            // Third argument is the scalar
            return CheckSimdScalarArgs(formalSimdType_)(f, arg, argIndex, actualType, patchAt);
        }
        return false;
    }
};

} // namespace

static bool
SwitchPackOp(FunctionValidator& f, AsmJSSimdType type, Expr i32x4, Expr f32x4, Expr b32x4)
{
    switch (type) {
      case AsmJSSimdType_int32x4:   return f.writeOp(i32x4);
      case AsmJSSimdType_float32x4: return f.writeOp(f32x4);
      case AsmJSSimdType_bool32x4:  return f.writeOp(b32x4);
    }
    MOZ_CRASH("unexpected simd type");
}

static bool
CheckSimdUnary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
               MSimdUnaryArith::Operation op, Type* type)
{
    if (!SwitchPackOp(f, opType, Expr::I32X4Unary, Expr::F32X4Unary, Expr::B32X4Unary))
        return false;
    if (!f.writeU8(uint8_t(op)))
        return false;
    if (!CheckSimdCallArgs(f, call, 1, CheckArgIsSubtypeOf(opType)))
        return false;
    *type = opType;
    return true;
}

template<class OpKind>
inline bool
CheckSimdBinaryGuts(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, OpKind op,
                    Type* type)
{
    if (!f.writeU8(uint8_t(op)))
        return false;
    if (!CheckSimdCallArgs(f, call, 2, CheckArgIsSubtypeOf(opType)))
        return false;
    *type = opType;
    return true;
}

static bool
CheckSimdBinary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                MSimdBinaryArith::Operation op, Type* type)
{
    return SwitchPackOp(f, opType, Expr::I32X4Binary, Expr::F32X4Binary, Expr::B32X4Binary) &&
           CheckSimdBinaryGuts(f, call, opType, op, type);
}

static bool
CheckSimdBinary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                MSimdBinaryBitwise::Operation op, Type* type)
{
    return SwitchPackOp(f, opType, Expr::I32X4BinaryBitwise, Expr::Unreachable, Expr::B32X4BinaryBitwise) &&
           CheckSimdBinaryGuts(f, call, opType, op, type);
}

static bool
CheckSimdBinary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                MSimdBinaryComp::Operation op, Type* type)
{
    switch (opType) {
      case AsmJSSimdType_int32x4:
        if (!f.writeOp(Expr::B32X4BinaryCompI32X4))
            return false;
        break;
      case AsmJSSimdType_float32x4:
        if (!f.writeOp(Expr::B32X4BinaryCompF32X4))
            return false;
        break;
      case AsmJSSimdType_bool32x4:
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Can't compare boolean vectors");
    }
    if (!f.writeU8(uint8_t(op)))
        return false;
    if (!CheckSimdCallArgs(f, call, 2, CheckArgIsSubtypeOf(opType)))
        return false;
    *type = Type::Bool32x4;
    return true;
}

static bool
CheckSimdBinary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                MSimdShift::Operation op, Type* type)
{
    if (!f.writeOp(Expr::I32X4BinaryShift) || !f.writeU8(uint8_t(op)))
        return false;
    if (!CheckSimdCallArgs(f, call, 2, CheckSimdVectorScalarArgs(opType)))
        return false;
    *type = Type::Int32x4;
    return true;
}

static bool
CheckSimdExtractLane(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    switch (opType) {
      case AsmJSSimdType_int32x4:
        if (!f.writeOp(Expr::I32I32X4ExtractLane))
            return false;
        *type = Type::Signed;
        break;
      case AsmJSSimdType_float32x4:
        if (!f.writeOp(Expr::F32F32X4ExtractLane))
            return false;
        *type = Type::Float;
        break;
      case AsmJSSimdType_bool32x4:
        if (!f.writeOp(Expr::I32B32X4ExtractLane))
            return false;
        *type = Type::Int;
        break;
    }
    return CheckSimdCallArgs(f, call, 2, CheckSimdExtractLaneArgs(opType));
}

static bool
CheckSimdReplaceLane(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    if (!SwitchPackOp(f, opType, Expr::I32X4ReplaceLane, Expr::F32X4ReplaceLane, Expr::B32X4ReplaceLane))
        return false;
    if (!CheckSimdCallArgsPatchable(f, call, 3, CheckSimdReplaceLaneArgs(opType)))
        return false;
    *type = opType;
    return true;
}

typedef bool IsBitCast;

namespace {
// Include CheckSimdCast in unnamed namespace to avoid MSVC name lookup bug (due to the use of Type).

static bool
CheckSimdCast(FunctionValidator& f, ParseNode* call, AsmJSSimdType fromType, AsmJSSimdType toType,
              bool bitcast, Type* type)
{
    if (!SwitchPackOp(f, toType,
                      bitcast ? Expr::I32X4FromF32X4Bits : Expr::I32X4FromF32X4,
                      bitcast ? Expr::F32X4FromI32X4Bits : Expr::F32X4FromI32X4,
                      Expr::Unreachable))
    {
        return false;
    }
    if (!CheckSimdCallArgs(f, call, 1, CheckArgIsSubtypeOf(fromType)))
        return false;
    *type = toType;
    return true;
}

} // namespace

static bool
CheckSimdShuffleSelectors(FunctionValidator& f, ParseNode* lane, int32_t lanes[4], uint32_t maxLane)
{
    for (unsigned i = 0; i < 4; i++, lane = NextNode(lane)) {
        uint32_t u32;
        if (!IsLiteralInt(f.m(), lane, &u32))
            return f.failf(lane, "lane selector should be a constant integer literal");
        if (u32 >= maxLane)
            return f.failf(lane, "lane selector should be less than %u", maxLane);
        lanes[i] = int32_t(u32);
    }
    return true;
}

static bool
CheckSimdSwizzle(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != 5)
        return f.failf(call, "expected 5 arguments to SIMD swizzle, got %u", numArgs);

    if (!SwitchPackOp(f, opType, Expr::I32X4Swizzle, Expr::F32X4Swizzle, Expr::Unreachable))
        return false;

    Type retType = opType;
    ParseNode* vec = CallArgList(call);
    Type vecType;
    if (!CheckExpr(f, vec, &vecType))
        return false;
    if (!(vecType <= retType))
        return f.failf(vec, "%s is not a subtype of %s", vecType.toChars(), retType.toChars());

    int32_t lanes[4];
    if (!CheckSimdShuffleSelectors(f, NextNode(vec), lanes, 4))
        return false;

    for (unsigned i = 0; i < 4; i++) {
        if (!f.writeU8(uint8_t(lanes[i])))
            return false;
    }

    *type = retType;
    return true;
}

static bool
CheckSimdShuffle(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != 6)
        return f.failf(call, "expected 6 arguments to SIMD shuffle, got %u", numArgs);

    if (!SwitchPackOp(f, opType, Expr::I32X4Shuffle, Expr::F32X4Shuffle, Expr::Unreachable))
        return false;

    Type retType = opType;
    ParseNode* arg = CallArgList(call);
    for (unsigned i = 0; i < 2; i++, arg = NextNode(arg)) {
        Type type;
        if (!CheckExpr(f, arg, &type))
            return false;
        if (!(type <= retType))
            return f.failf(arg, "%s is not a subtype of %s", type.toChars(), retType.toChars());
    }

    int32_t lanes[4];
    if (!CheckSimdShuffleSelectors(f, arg, lanes, 8))
        return false;

    for (unsigned i = 0; i < 4; i++) {
        if (!f.writeU8(uint8_t(lanes[i])))
            return false;
    }

    *type = retType;
    return true;
}

static bool
CheckSimdLoadStoreArgs(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                       Scalar::Type* viewType, NeedsBoundsCheck* needsBoundsCheck)
{
    ParseNode* view = CallArgList(call);
    if (!view->isKind(PNK_NAME))
        return f.fail(view, "expected Uint8Array view as SIMD.*.load/store first argument");

    const ModuleValidator::Global* global = f.lookupGlobal(view->name());
    if (!global ||
        global->which() != ModuleValidator::Global::ArrayView ||
        global->viewType() != Scalar::Uint8)
    {
        return f.fail(view, "expected Uint8Array view as SIMD.*.load/store first argument");
    }

    *needsBoundsCheck = NEEDS_BOUNDS_CHECK;

    switch (opType) {
      case AsmJSSimdType_int32x4:   *viewType = Scalar::Int32x4;   break;
      case AsmJSSimdType_float32x4: *viewType = Scalar::Float32x4; break;
      case AsmJSSimdType_bool32x4:  MOZ_CRASH("Cannot load/store boolean SIMD type");
    }

    ParseNode* indexExpr = NextNode(view);
    uint32_t indexLit;
    if (IsLiteralOrConstInt(f, indexExpr, &indexLit)) {
        if (!f.m().tryConstantAccess(indexLit, Simd128DataSize))
            return f.fail(indexExpr, "constant index out of range");

        *needsBoundsCheck = NO_BOUNDS_CHECK;
        return f.writeInt32Lit(indexLit);
    }

    Type indexType;
    if (!CheckExpr(f, indexExpr, &indexType))
        return false;

    if (!indexType.isIntish())
        return f.failf(indexExpr, "%s is not a subtype of intish", indexType.toChars());

    return true;
}

static bool
CheckSimdLoad(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
              unsigned numElems, Type* type)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != 2)
        return f.failf(call, "expected 2 arguments to SIMD load, got %u", numArgs);

    if (!SwitchPackOp(f, opType, Expr::I32X4Load, Expr::F32X4Load, Expr::Unreachable))
        return false;

    size_t viewTypeAt;
    size_t needsBoundsCheckAt;
    if (!f.tempU8(&viewTypeAt) || !f.tempU8(&needsBoundsCheckAt) || !f.writeU8(numElems))
        return false;

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    if (!CheckSimdLoadStoreArgs(f, call, opType, &viewType, &needsBoundsCheck))
        return false;

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = opType;
    return true;
}

static bool
CheckSimdStore(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
               unsigned numElems, Type* type)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != 3)
        return f.failf(call, "expected 3 arguments to SIMD store, got %u", numArgs);

    if (!SwitchPackOp(f, opType, Expr::I32X4Store, Expr::F32X4Store, Expr::Unreachable))
        return false;

    size_t viewTypeAt;
    size_t needsBoundsCheckAt;
    if (!f.tempU8(&viewTypeAt) || !f.tempU8(&needsBoundsCheckAt) || !f.writeU8(numElems))
        return false;

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    if (!CheckSimdLoadStoreArgs(f, call, opType, &viewType, &needsBoundsCheck))
        return false;

    Type retType = opType;
    ParseNode* vecExpr = NextNode(NextNode(CallArgList(call)));
    Type vecType;
    if (!CheckExpr(f, vecExpr, &vecType))
        return false;
    if (!(vecType <= retType))
        return f.failf(vecExpr, "%s is not a subtype of %s", vecType.toChars(), retType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = vecType;
    return true;
}

static bool
CheckSimdSelect(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    if (!SwitchPackOp(f, opType, Expr::I32X4Select, Expr::F32X4Select, Expr::Unreachable))
        return false;
    if (!CheckSimdCallArgs(f, call, 3, CheckSimdSelectArgs(opType)))
        return false;
    *type = opType;
    return true;
}

static bool
CheckSimdAllTrue(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    switch (opType) {
      case AsmJSSimdType_bool32x4:
        if (!f.writeOp(Expr::I32B32X4AllTrue))
            return false;
        break;
      case AsmJSSimdType_int32x4:
      case AsmJSSimdType_float32x4:
        MOZ_CRASH("allTrue is only defined on bool SIMD types");
    }
    if (!CheckSimdCallArgs(f, call, 1, CheckArgIsSubtypeOf(opType)))
        return false;
    *type = Type::Int;
    return true;
}

static bool
CheckSimdAnyTrue(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    switch (opType) {
      case AsmJSSimdType_bool32x4:
        if (!f.writeOp(Expr::I32B32X4AnyTrue))
            return false;
        break;
      case AsmJSSimdType_int32x4:
      case AsmJSSimdType_float32x4:
        MOZ_CRASH("anyTrue is only defined on bool SIMD types");
    }
    if (!CheckSimdCallArgs(f, call, 1, CheckArgIsSubtypeOf(opType)))
        return false;
    *type = Type::Int;
    return true;
}

static bool
CheckSimdCheck(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    ValType coerceTo;
    ParseNode* argNode;
    if (!IsCoercionCall(f.m(), call, &coerceTo, &argNode))
        return f.failf(call, "expected 1 argument in call to check");
    return CheckCoercionArg(f, argNode, coerceTo, type);
}

static bool
CheckSimdSplat(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    if (!SwitchPackOp(f, opType, Expr::I32X4Splat, Expr::F32X4Splat, Expr::B32X4Splat))
        return false;
    if (!CheckSimdCallArgsPatchable(f, call, 1, CheckSimdScalarArgs(opType)))
        return false;
    *type = opType;
    return true;
}

static bool
CheckSimdOperationCall(FunctionValidator& f, ParseNode* call, const ModuleValidator::Global* global,
                       Type* type)
{
    MOZ_ASSERT(global->isSimdOperation());

    AsmJSSimdType opType = global->simdOperationType();

    switch (global->simdOperation()) {
      case AsmJSSimdOperation_check:
        return CheckSimdCheck(f, call, opType, type);

#define OP_CHECK_CASE_LIST_(OP)                                                         \
      case AsmJSSimdOperation_##OP:                                                     \
        return CheckSimdBinary(f, call, opType, MSimdBinaryArith::Op_##OP, type);
      FOREACH_NUMERIC_SIMD_BINOP(OP_CHECK_CASE_LIST_)
      FOREACH_FLOAT_SIMD_BINOP(OP_CHECK_CASE_LIST_)
#undef OP_CHECK_CASE_LIST_

      case AsmJSSimdOperation_lessThan:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::lessThan, type);
      case AsmJSSimdOperation_lessThanOrEqual:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::lessThanOrEqual, type);
      case AsmJSSimdOperation_equal:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::equal, type);
      case AsmJSSimdOperation_notEqual:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::notEqual, type);
      case AsmJSSimdOperation_greaterThan:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::greaterThan, type);
      case AsmJSSimdOperation_greaterThanOrEqual:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::greaterThanOrEqual, type);

      case AsmJSSimdOperation_and:
        return CheckSimdBinary(f, call, opType, MSimdBinaryBitwise::and_, type);
      case AsmJSSimdOperation_or:
        return CheckSimdBinary(f, call, opType, MSimdBinaryBitwise::or_, type);
      case AsmJSSimdOperation_xor:
        return CheckSimdBinary(f, call, opType, MSimdBinaryBitwise::xor_, type);

      case AsmJSSimdOperation_extractLane:
        return CheckSimdExtractLane(f, call, opType, type);
      case AsmJSSimdOperation_replaceLane:
        return CheckSimdReplaceLane(f, call, opType, type);

      case AsmJSSimdOperation_fromInt32x4:
        return CheckSimdCast(f, call, AsmJSSimdType_int32x4, opType, IsBitCast(false), type);
      case AsmJSSimdOperation_fromFloat32x4:
        return CheckSimdCast(f, call, AsmJSSimdType_float32x4, opType, IsBitCast(false), type);
      case AsmJSSimdOperation_fromInt32x4Bits:
        return CheckSimdCast(f, call, AsmJSSimdType_int32x4, opType, IsBitCast(true), type);
      case AsmJSSimdOperation_fromFloat32x4Bits:
        return CheckSimdCast(f, call, AsmJSSimdType_float32x4, opType, IsBitCast(true), type);

      case AsmJSSimdOperation_shiftLeftByScalar:
        return CheckSimdBinary(f, call, opType, MSimdShift::lsh, type);
      case AsmJSSimdOperation_shiftRightByScalar:
        return CheckSimdBinary(f, call, opType,
                               IsSignedIntSimdType(opType) ? MSimdShift::rsh : MSimdShift::ursh,
                               type);
      case AsmJSSimdOperation_shiftRightArithmeticByScalar:
        return CheckSimdBinary(f, call, opType, MSimdShift::rsh, type);
      case AsmJSSimdOperation_shiftRightLogicalByScalar:
        return CheckSimdBinary(f, call, opType, MSimdShift::ursh, type);

      case AsmJSSimdOperation_abs:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::abs, type);
      case AsmJSSimdOperation_neg:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::neg, type);
      case AsmJSSimdOperation_not:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::not_, type);
      case AsmJSSimdOperation_sqrt:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::sqrt, type);
      case AsmJSSimdOperation_reciprocalApproximation:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::reciprocalApproximation, type);
      case AsmJSSimdOperation_reciprocalSqrtApproximation:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::reciprocalSqrtApproximation, type);

      case AsmJSSimdOperation_swizzle:
        return CheckSimdSwizzle(f, call, opType, type);
      case AsmJSSimdOperation_shuffle:
        return CheckSimdShuffle(f, call, opType, type);

      case AsmJSSimdOperation_load:
        return CheckSimdLoad(f, call, opType, 4, type);
      case AsmJSSimdOperation_load1:
        return CheckSimdLoad(f, call, opType, 1, type);
      case AsmJSSimdOperation_load2:
        return CheckSimdLoad(f, call, opType, 2, type);
      case AsmJSSimdOperation_load3:
        return CheckSimdLoad(f, call, opType, 3, type);
      case AsmJSSimdOperation_store:
        return CheckSimdStore(f, call, opType, 4, type);
      case AsmJSSimdOperation_store1:
        return CheckSimdStore(f, call, opType, 1, type);
      case AsmJSSimdOperation_store2:
        return CheckSimdStore(f, call, opType, 2, type);
      case AsmJSSimdOperation_store3:
        return CheckSimdStore(f, call, opType, 3, type);

      case AsmJSSimdOperation_select:
        return CheckSimdSelect(f, call, opType, type);

      case AsmJSSimdOperation_splat:
        return CheckSimdSplat(f, call, opType, type);

      case AsmJSSimdOperation_allTrue:
        return CheckSimdAllTrue(f, call, opType, type);
      case AsmJSSimdOperation_anyTrue:
        return CheckSimdAnyTrue(f, call, opType, type);
    }
    MOZ_CRASH("unexpected simd operation in CheckSimdOperationCall");
}

static bool
CheckSimdCtorCall(FunctionValidator& f, ParseNode* call, const ModuleValidator::Global* global,
                  Type* type)
{
    MOZ_ASSERT(call->isKind(PNK_CALL));

    AsmJSSimdType simdType = global->simdCtorType();
    if (!SwitchPackOp(f, simdType, Expr::I32X4Ctor, Expr::F32X4Ctor, Expr::B32X4Ctor))
        return false;

    unsigned length = SimdTypeToLength(simdType);
    if (!CheckSimdCallArgsPatchable(f, call, length, CheckSimdScalarArgs(simdType)))
        return false;

    *type = simdType;
    return true;
}

static bool
CheckUncoercedCall(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_CALL));

    const ModuleValidator::Global* global;
    if (IsCallToGlobal(f.m(), expr, &global)) {
        if (global->isMathFunction())
            return CheckMathBuiltinCall(f, expr, global->mathBuiltinFunction(), type);
        if (global->isAtomicsFunction())
            return CheckAtomicsBuiltinCall(f, expr, global->atomicsBuiltinFunction(), type);
        if (global->isSimdCtor())
            return CheckSimdCtorCall(f, expr, global, type);
        if (global->isSimdOperation())
            return CheckSimdOperationCall(f, expr, global, type);
    }

    return f.fail(expr, "all function calls must either be calls to standard lib math functions, "
                        "standard atomic functions, standard SIMD constructors or operations, "
                        "ignored (via f(); or comma-expression), coerced to signed (via f()|0), "
                        "coerced to float (via fround(f())) or coerced to double (via +f())");
}

static bool
CoerceResult(FunctionValidator& f, ParseNode* expr, ExprType expected, Type actual, size_t patchAt,
             Type* type)
{
    // At this point, the bytecode resembles this:
    //      | patchAt | the thing we wanted to coerce | current position |>
    switch (expected) {
      case ExprType::Void:
        if (actual.isIntish())
            f.patchOp(patchAt, Expr::I32Expr);
        else if (actual.isFloatish())
            f.patchOp(patchAt, Expr::F32Expr);
        else if (actual.isMaybeDouble())
            f.patchOp(patchAt, Expr::F64Expr);
        else if (actual.isInt32x4())
            f.patchOp(patchAt, Expr::I32X4Expr);
        else if (actual.isFloat32x4())
            f.patchOp(patchAt, Expr::F32X4Expr);
        else if (actual.isBool32x4())
            f.patchOp(patchAt, Expr::B32X4Expr);
        else if (actual.isVoid())
            f.patchOp(patchAt, Expr::Id);
        else
            MOZ_CRASH("unhandled return type");
        break;
      case ExprType::I32:
        if (!actual.isIntish())
            return f.failf(expr, "%s is not a subtype of intish", actual.toChars());
        f.patchOp(patchAt, Expr::Id);
        break;
      case ExprType::I64:
        MOZ_CRASH("no int64 in asm.js");
      case ExprType::F32:
        if (!CheckFloatCoercionArg(f, expr, actual, patchAt))
            return false;
        break;
      case ExprType::F64:
        if (actual.isMaybeDouble())
            f.patchOp(patchAt, Expr::Id);
        else if (actual.isMaybeFloat())
            f.patchOp(patchAt, Expr::F64FromF32);
        else if (actual.isSigned())
            f.patchOp(patchAt, Expr::F64FromS32);
        else if (actual.isUnsigned())
            f.patchOp(patchAt, Expr::F64FromU32);
        else
            return f.failf(expr, "%s is not a subtype of double?, float?, signed or unsigned", actual.toChars());
        break;
      case ExprType::I32x4:
        if (!actual.isInt32x4())
            return f.failf(expr, "%s is not a subtype of int32x4", actual.toChars());
        f.patchOp(patchAt, Expr::Id);
        break;
      case ExprType::F32x4:
        if (!actual.isFloat32x4())
            return f.failf(expr, "%s is not a subtype of float32x4", actual.toChars());
        f.patchOp(patchAt, Expr::Id);
        break;
      case ExprType::B32x4:
        if (!actual.isBool32x4())
            return f.failf(expr, "%s is not a subtype of bool32x4", actual.toChars());
        f.patchOp(patchAt, Expr::Id);
        break;
    }

    *type = Type::ret(expected);
    return true;
}

static bool
CheckCoercedMathBuiltinCall(FunctionValidator& f, ParseNode* callNode, AsmJSMathBuiltinFunction func,
                            ExprType ret, Type* type)
{
    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;
    Type actual;
    if (!CheckMathBuiltinCall(f, callNode, func, &actual))
        return false;
    return CoerceResult(f, callNode, ret, actual, opcodeAt, type);
}

static bool
CheckCoercedSimdCall(FunctionValidator& f, ParseNode* call, const ModuleValidator::Global* global,
                     ExprType ret, Type* type)
{
    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type actual;
    if (global->isSimdCtor()) {
        if (!CheckSimdCtorCall(f, call, global, &actual))
            return false;
        MOZ_ASSERT(actual.isSimd());
    } else {
        MOZ_ASSERT(global->isSimdOperation());
        if (!CheckSimdOperationCall(f, call, global, &actual))
            return false;
    }

    return CoerceResult(f, call, ret, actual, opcodeAt, type);
}

static bool
CheckCoercedAtomicsBuiltinCall(FunctionValidator& f, ParseNode* callNode,
                               AsmJSAtomicsBuiltinFunction func, ExprType ret, Type* type)
{
    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;
    Type actual;
    if (!CheckAtomicsBuiltinCall(f, callNode, func, &actual))
        return false;
    return CoerceResult(f, callNode, ret, actual, opcodeAt, type);
}

static bool
CheckCoercedCall(FunctionValidator& f, ParseNode* call, ExprType ret, Type* type)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    if (IsNumericLiteral(f.m(), call)) {
        size_t coerceOp;
        if (!f.tempOp(&coerceOp))
            return false;
        NumLit lit = ExtractNumericLiteral(f.m(), call);
        if (!f.writeLit(lit))
            return false;
        return CoerceResult(f, call, ret, Type::lit(lit), coerceOp, type);
    }

    ParseNode* callee = CallCallee(call);

    if (callee->isKind(PNK_ELEM))
        return CheckFuncPtrCall(f, call, ret, type);

    if (!callee->isKind(PNK_NAME))
        return f.fail(callee, "unexpected callee expression type");

    PropertyName* calleeName = callee->name();

    if (const ModuleValidator::Global* global = f.lookupGlobal(calleeName)) {
        switch (global->which()) {
          case ModuleValidator::Global::FFI:
            return CheckFFICall(f, call, global->ffiIndex(), ret, type);
          case ModuleValidator::Global::MathBuiltinFunction:
            return CheckCoercedMathBuiltinCall(f, call, global->mathBuiltinFunction(), ret, type);
          case ModuleValidator::Global::AtomicsBuiltinFunction:
            return CheckCoercedAtomicsBuiltinCall(f, call, global->atomicsBuiltinFunction(), ret, type);
          case ModuleValidator::Global::ConstantLiteral:
          case ModuleValidator::Global::ConstantImport:
          case ModuleValidator::Global::Variable:
          case ModuleValidator::Global::FuncPtrTable:
          case ModuleValidator::Global::ArrayView:
          case ModuleValidator::Global::ArrayViewCtor:
            return f.failName(callee, "'%s' is not callable function", callee->name());
          case ModuleValidator::Global::SimdCtor:
          case ModuleValidator::Global::SimdOperation:
            return CheckCoercedSimdCall(f, call, global, ret, type);
          case ModuleValidator::Global::Function:
            break;
        }
    }

    return CheckInternalCall(f, call, calleeName, ret, type);
}

static bool
CheckPos(FunctionValidator& f, ParseNode* pos, Type* type)
{
    MOZ_ASSERT(pos->isKind(PNK_POS));
    ParseNode* operand = UnaryKid(pos);

    if (operand->isKind(PNK_CALL))
        return CheckCoercedCall(f, operand, ExprType::F64, type);

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type actual;
    if (!CheckExpr(f, operand, &actual))
        return false;

    return CoerceResult(f, operand, ExprType::F64, actual, opcodeAt, type);
}

static bool
CheckNot(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_NOT));
    ParseNode* operand = UnaryKid(expr);

    if (!f.writeOp(Expr::I32Not))
        return false;

    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    if (!operandType.isInt())
        return f.failf(operand, "%s is not a subtype of int", operandType.toChars());

    *type = Type::Int;
    return true;
}

static bool
CheckNeg(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_NEG));
    ParseNode* operand = UnaryKid(expr);

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    if (operandType.isInt()) {
        f.patchOp(opcodeAt, Expr::I32Neg);
        *type = Type::Intish;
        return true;
    }

    if (operandType.isMaybeDouble()) {
        f.patchOp(opcodeAt, Expr::F64Neg);
        *type = Type::Double;
        return true;
    }

    if (operandType.isMaybeFloat()) {
        f.patchOp(opcodeAt, Expr::F32Neg);
        *type = Type::Floatish;
        return true;
    }

    return f.failf(operand, "%s is not a subtype of int, float? or double?", operandType.toChars());
}

static bool
CheckCoerceToInt(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_BITNOT));
    ParseNode* operand = UnaryKid(expr);

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    if (operandType.isMaybeDouble() || operandType.isMaybeFloat()) {
        Expr opcode = operandType.isMaybeDouble() ? Expr::I32SConvertF64 : Expr::I32SConvertF32;
        f.patchOp(opcodeAt, opcode);
        *type = Type::Signed;
        return true;
    }

    if (!operandType.isIntish())
        return f.failf(operand, "%s is not a subtype of double?, float? or intish", operandType.toChars());

    f.patchOp(opcodeAt, Expr::Id);
    *type = Type::Signed;
    return true;
}

static bool
CheckBitNot(FunctionValidator& f, ParseNode* neg, Type* type)
{
    MOZ_ASSERT(neg->isKind(PNK_BITNOT));
    ParseNode* operand = UnaryKid(neg);

    if (operand->isKind(PNK_BITNOT))
        return CheckCoerceToInt(f, operand, type);

    if (!f.writeOp(Expr::I32BitNot))
        return false;

    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    if (!operandType.isIntish())
        return f.failf(operand, "%s is not a subtype of intish", operandType.toChars());

    *type = Type::Signed;
    return true;
}

static bool
CheckAsExprStatement(FunctionValidator& f, ParseNode* exprStmt);

static bool
CheckComma(FunctionValidator& f, ParseNode* comma, Type* type)
{
    MOZ_ASSERT(comma->isKind(PNK_COMMA));
    ParseNode* operands = ListHead(comma);

    if (!f.writeOp(Expr::Block) || !f.writeU32(ListLength(comma)))
        return false;

    ParseNode* pn = operands;
    for (; NextNode(pn); pn = NextNode(pn)) {
        if (!CheckAsExprStatement(f, pn))
            return false;
    }

    return CheckExpr(f, pn, type) &&
           f.writeDebugCheckPoint();
}

static bool
CheckConditional(FunctionValidator& f, ParseNode* ternary, Type* type)
{
    MOZ_ASSERT(ternary->isKind(PNK_CONDITIONAL));

    if (!f.writeOp(Expr::Ternary))
        return false;

    ParseNode* cond = TernaryKid1(ternary);
    ParseNode* thenExpr = TernaryKid2(ternary);
    ParseNode* elseExpr = TernaryKid3(ternary);

    Type condType;
    if (!CheckExpr(f, cond, &condType))
        return false;

    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    Type thenType;
    if (!CheckExpr(f, thenExpr, &thenType))
        return false;

    Type elseType;
    if (!CheckExpr(f, elseExpr, &elseType))
        return false;

    if (thenType.isInt() && elseType.isInt()) {
        *type = Type::Int;
    } else if (thenType.isDouble() && elseType.isDouble()) {
        *type = Type::Double;
    } else if (thenType.isFloat() && elseType.isFloat()) {
        *type = Type::Float;
    } else if (elseType.isInt32x4() && thenType.isInt32x4()) {
        *type = Type::Int32x4;
    } else if (elseType.isFloat32x4() && thenType.isFloat32x4()) {
        *type = Type::Float32x4;
    } else if (elseType.isBool32x4() && thenType.isBool32x4()) {
        *type = Type::Bool32x4;
    } else {
        return f.failf(ternary, "then/else branches of conditional must both produce int, float, "
                       "double or SIMD types, current types are %s and %s",
                       thenType.toChars(), elseType.toChars());
    }

    return true;
}

static bool
IsValidIntMultiplyConstant(ModuleValidator& m, ParseNode* expr)
{
    if (!IsNumericLiteral(m, expr))
        return false;

    NumLit lit = ExtractNumericLiteral(m, expr);
    switch (lit.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
        if (abs(lit.toInt32()) < (1<<20))
            return true;
        return false;
      case NumLit::BigUnsigned:
      case NumLit::Double:
      case NumLit::Float:
      case NumLit::OutOfRangeInt:
      case NumLit::Int32x4:
      case NumLit::Float32x4:
      case NumLit::Bool32x4:
        return false;
    }

    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Bad literal");
}

static bool
CheckMultiply(FunctionValidator& f, ParseNode* star, Type* type)
{
    MOZ_ASSERT(star->isKind(PNK_STAR));
    ParseNode* lhs = MultiplyLeft(star);
    ParseNode* rhs = MultiplyRight(star);

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (lhsType.isInt() && rhsType.isInt()) {
        if (!IsValidIntMultiplyConstant(f.m(), lhs) && !IsValidIntMultiplyConstant(f.m(), rhs))
            return f.fail(star, "one arg to int multiply must be a small (-2^20, 2^20) int literal");
        f.patchOp(opcodeAt, Expr::I32Mul);
        *type = Type::Intish;
        return true;
    }

    if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        f.patchOp(opcodeAt, Expr::F64Mul);
        *type = Type::Double;
        return true;
    }

    if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        f.patchOp(opcodeAt, Expr::F32Mul);
        *type = Type::Floatish;
        return true;
    }

    return f.fail(star, "multiply operands must be both int, both double? or both float?");
}

static bool
CheckAddOrSub(FunctionValidator& f, ParseNode* expr, Type* type, unsigned* numAddOrSubOut = nullptr)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    MOZ_ASSERT(expr->isKind(PNK_ADD) || expr->isKind(PNK_SUB));
    ParseNode* lhs = AddSubLeft(expr);
    ParseNode* rhs = AddSubRight(expr);

    Type lhsType, rhsType;
    unsigned lhsNumAddOrSub, rhsNumAddOrSub;

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    if (lhs->isKind(PNK_ADD) || lhs->isKind(PNK_SUB)) {
        if (!CheckAddOrSub(f, lhs, &lhsType, &lhsNumAddOrSub))
            return false;
        if (lhsType == Type::Intish)
            lhsType = Type::Int;
    } else {
        if (!CheckExpr(f, lhs, &lhsType))
            return false;
        lhsNumAddOrSub = 0;
    }

    if (rhs->isKind(PNK_ADD) || rhs->isKind(PNK_SUB)) {
        if (!CheckAddOrSub(f, rhs, &rhsType, &rhsNumAddOrSub))
            return false;
        if (rhsType == Type::Intish)
            rhsType = Type::Int;
    } else {
        if (!CheckExpr(f, rhs, &rhsType))
            return false;
        rhsNumAddOrSub = 0;
    }

    unsigned numAddOrSub = lhsNumAddOrSub + rhsNumAddOrSub + 1;
    if (numAddOrSub > (1<<20))
        return f.fail(expr, "too many + or - without intervening coercion");

    if (lhsType.isInt() && rhsType.isInt()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_ADD) ? Expr::I32Add : Expr::I32Sub);
        *type = Type::Intish;
    } else if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_ADD) ? Expr::F64Add : Expr::F64Sub);
        *type = Type::Double;
    } else if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_ADD) ? Expr::F32Add : Expr::F32Sub);
        *type = Type::Floatish;
    } else {
        return f.failf(expr, "operands to + or - must both be int, float? or double?, got %s and %s",
                       lhsType.toChars(), rhsType.toChars());
    }

    if (numAddOrSubOut)
        *numAddOrSubOut = numAddOrSub;
    return true;
}

static bool
CheckDivOrMod(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_DIV) || expr->isKind(PNK_MOD));

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    ParseNode* lhs = DivOrModLeft(expr);
    ParseNode* rhs = DivOrModRight(expr);

    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_DIV) ? Expr::F64Div : Expr::F64Mod);
        *type = Type::Double;
        return true;
    }

    if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        if (expr->isKind(PNK_DIV))
            f.patchOp(opcodeAt, Expr::F32Div);
        else
            return f.fail(expr, "modulo cannot receive float arguments");
        *type = Type::Floatish;
        return true;
    }

    if (lhsType.isSigned() && rhsType.isSigned()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_DIV) ? Expr::I32DivS : Expr::I32RemS);
        *type = Type::Intish;
        return true;
    }

    if (lhsType.isUnsigned() && rhsType.isUnsigned()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_DIV) ? Expr::I32DivU : Expr::I32RemU);
        *type = Type::Intish;
        return true;
    }

    return f.failf(expr, "arguments to / or %% must both be double?, float?, signed, or unsigned; "
                   "%s and %s are given", lhsType.toChars(), rhsType.toChars());
}

static bool
CheckComparison(FunctionValidator& f, ParseNode* comp, Type* type)
{
    MOZ_ASSERT(comp->isKind(PNK_LT) || comp->isKind(PNK_LE) || comp->isKind(PNK_GT) ||
               comp->isKind(PNK_GE) || comp->isKind(PNK_EQ) || comp->isKind(PNK_NE));

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    ParseNode* lhs = ComparisonLeft(comp);
    ParseNode* rhs = ComparisonRight(comp);

    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (!(lhsType.isSigned() && rhsType.isSigned()) &&
        !(lhsType.isUnsigned() && rhsType.isUnsigned()) &&
        !(lhsType.isDouble() && rhsType.isDouble()) &&
        !(lhsType.isFloat() && rhsType.isFloat()))
    {
        return f.failf(comp, "arguments to a comparison must both be signed, unsigned, floats or doubles; "
                       "%s and %s are given", lhsType.toChars(), rhsType.toChars());
    }

    Expr stmt;
    if (lhsType.isSigned() && rhsType.isSigned()) {
        switch (comp->getOp()) {
          case JSOP_EQ: stmt = Expr::I32Eq;  break;
          case JSOP_NE: stmt = Expr::I32Ne;  break;
          case JSOP_LT: stmt = Expr::I32LtS; break;
          case JSOP_LE: stmt = Expr::I32LeS; break;
          case JSOP_GT: stmt = Expr::I32GtS; break;
          case JSOP_GE: stmt = Expr::I32GeS; break;
          default: MOZ_CRASH("unexpected comparison op");
        }
    } else if (lhsType.isUnsigned() && rhsType.isUnsigned()) {
        switch (comp->getOp()) {
          case JSOP_EQ: stmt = Expr::I32Eq;  break;
          case JSOP_NE: stmt = Expr::I32Ne;  break;
          case JSOP_LT: stmt = Expr::I32LtU; break;
          case JSOP_LE: stmt = Expr::I32LeU; break;
          case JSOP_GT: stmt = Expr::I32GtU; break;
          case JSOP_GE: stmt = Expr::I32GeU; break;
          default: MOZ_CRASH("unexpected comparison op");
        }
    } else if (lhsType.isDouble()) {
        switch (comp->getOp()) {
          case JSOP_EQ: stmt = Expr::F64Eq; break;
          case JSOP_NE: stmt = Expr::F64Ne; break;
          case JSOP_LT: stmt = Expr::F64Lt; break;
          case JSOP_LE: stmt = Expr::F64Le; break;
          case JSOP_GT: stmt = Expr::F64Gt; break;
          case JSOP_GE: stmt = Expr::F64Ge; break;
          default: MOZ_CRASH("unexpected comparison op");
        }
    } else if (lhsType.isFloat()) {
        switch (comp->getOp()) {
          case JSOP_EQ: stmt = Expr::F32Eq; break;
          case JSOP_NE: stmt = Expr::F32Ne; break;
          case JSOP_LT: stmt = Expr::F32Lt; break;
          case JSOP_LE: stmt = Expr::F32Le; break;
          case JSOP_GT: stmt = Expr::F32Gt; break;
          case JSOP_GE: stmt = Expr::F32Ge; break;
          default: MOZ_CRASH("unexpected comparison op");
        }
    } else {
        MOZ_CRASH("unexpected type");
    }

    f.patchOp(opcodeAt, stmt);
    *type = Type::Int;
    return true;
}

static bool
CheckBitwise(FunctionValidator& f, ParseNode* bitwise, Type* type)
{
    ParseNode* lhs = BitwiseLeft(bitwise);
    ParseNode* rhs = BitwiseRight(bitwise);

    int32_t identityElement;
    bool onlyOnRight;
    switch (bitwise->getKind()) {
      case PNK_BITOR:  identityElement = 0;  onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_BITAND: identityElement = -1; onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_BITXOR: identityElement = 0;  onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_LSH:    identityElement = 0;  onlyOnRight = true;  *type = Type::Signed;   break;
      case PNK_RSH:    identityElement = 0;  onlyOnRight = true;  *type = Type::Signed;   break;
      case PNK_URSH:   identityElement = 0;  onlyOnRight = true;  *type = Type::Unsigned; break;
      default: MOZ_CRASH("not a bitwise op");
    }

    uint32_t i;
    if (!onlyOnRight && IsLiteralInt(f.m(), lhs, &i) && i == uint32_t(identityElement)) {
        Type rhsType;
        if (!CheckExpr(f, rhs, &rhsType))
            return false;
        if (!rhsType.isIntish())
            return f.failf(bitwise, "%s is not a subtype of intish", rhsType.toChars());
        return true;
    }

    if (IsLiteralInt(f.m(), rhs, &i) && i == uint32_t(identityElement)) {
        if (bitwise->isKind(PNK_BITOR) && lhs->isKind(PNK_CALL))
            return CheckCoercedCall(f, lhs, ExprType::I32, type);

        Type lhsType;
        if (!CheckExpr(f, lhs, &lhsType))
            return false;
        if (!lhsType.isIntish())
            return f.failf(bitwise, "%s is not a subtype of intish", lhsType.toChars());
        return true;
    }

    switch (bitwise->getKind()) {
      case PNK_BITOR:  if (!f.writeOp(Expr::I32Ior))  return false; break;
      case PNK_BITAND: if (!f.writeOp(Expr::I32And))  return false; break;
      case PNK_BITXOR: if (!f.writeOp(Expr::I32Xor))  return false; break;
      case PNK_LSH:    if (!f.writeOp(Expr::I32Shl))  return false; break;
      case PNK_RSH:    if (!f.writeOp(Expr::I32ShrS)) return false; break;
      case PNK_URSH:   if (!f.writeOp(Expr::I32ShrU)) return false; break;
      default: MOZ_CRASH("not a bitwise op");
    }

    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (!lhsType.isIntish())
        return f.failf(lhs, "%s is not a subtype of intish", lhsType.toChars());
    if (!rhsType.isIntish())
        return f.failf(rhs, "%s is not a subtype of intish", rhsType.toChars());

    return true;
}

static bool
CheckExpr(FunctionValidator& f, ParseNode* expr, Type* type)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    if (IsNumericLiteral(f.m(), expr))
        return CheckNumericLiteral(f, expr, type);

    switch (expr->getKind()) {
      case PNK_NAME:        return CheckVarRef(f, expr, type);
      case PNK_ELEM:        return CheckLoadArray(f, expr, type);
      case PNK_ASSIGN:      return CheckAssign(f, expr, type);
      case PNK_POS:         return CheckPos(f, expr, type);
      case PNK_NOT:         return CheckNot(f, expr, type);
      case PNK_NEG:         return CheckNeg(f, expr, type);
      case PNK_BITNOT:      return CheckBitNot(f, expr, type);
      case PNK_COMMA:       return CheckComma(f, expr, type);
      case PNK_CONDITIONAL: return CheckConditional(f, expr, type);
      case PNK_STAR:        return CheckMultiply(f, expr, type);
      case PNK_CALL:        return CheckUncoercedCall(f, expr, type);

      case PNK_ADD:
      case PNK_SUB:         return CheckAddOrSub(f, expr, type);

      case PNK_DIV:
      case PNK_MOD:         return CheckDivOrMod(f, expr, type);

      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_EQ:
      case PNK_NE:          return CheckComparison(f, expr, type);

      case PNK_BITOR:
      case PNK_BITAND:
      case PNK_BITXOR:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:        return CheckBitwise(f, expr, type);

      default:;
    }

    return f.fail(expr, "unsupported expression");
}

static bool
CheckStatement(FunctionValidator& f, ParseNode* stmt);

static bool
CheckAsExprStatement(FunctionValidator& f, ParseNode* expr)
{
    if (expr->isKind(PNK_CALL)) {
        Type _;
        return CheckCoercedCall(f, expr, ExprType::Void, &_);
    }

    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    Type type;
    if (!CheckExpr(f, expr, &type))
        return false;

    if (type.isIntish())
        f.patchOp(opcodeAt, Expr::I32Expr);
    else if (type.isFloatish())
        f.patchOp(opcodeAt, Expr::F32Expr);
    else if (type.isMaybeDouble())
        f.patchOp(opcodeAt, Expr::F64Expr);
    else if (type.isInt32x4())
        f.patchOp(opcodeAt, Expr::I32X4Expr);
    else if (type.isFloat32x4())
        f.patchOp(opcodeAt, Expr::F32X4Expr);
    else if (type.isBool32x4())
        f.patchOp(opcodeAt, Expr::B32X4Expr);
    else
        MOZ_CRASH("unexpected or unimplemented expression statement");

    return true;
}

static bool
CheckExprStatement(FunctionValidator& f, ParseNode* exprStmt)
{
    MOZ_ASSERT(exprStmt->isKind(PNK_SEMI));
    ParseNode* expr = UnaryKid(exprStmt);
    if (!expr)
        return f.writeOp(Expr::Nop);
    return CheckAsExprStatement(f, expr);
}

enum class InterruptCheckPosition {
    Head,
    Loop
};

static bool
MaybeAddInterruptCheck(FunctionValidator& f, InterruptCheckPosition pos, ParseNode* pn)
{
    if (f.m().mg().args().useSignalHandlersForInterrupt)
        return true;

    switch (pos) {
      case InterruptCheckPosition::Head:
        if (!f.writeOp(Expr::InterruptCheckHead))
            return false;
        break;
      case InterruptCheckPosition::Loop:
        if (!f.writeOp(Expr::InterruptCheckLoop))
            return false;
        break;
    }

    unsigned lineno = 0, column = 0;
    f.m().tokenStream().srcCoords.lineNumAndColumnIndex(pn->pn_pos.begin, &lineno, &column);
    return f.writeU32(lineno) &&
           f.writeU32(column);
}

static bool
CheckWhile(FunctionValidator& f, ParseNode* whileStmt)
{
    MOZ_ASSERT(whileStmt->isKind(PNK_WHILE));
    ParseNode* cond = BinaryLeft(whileStmt);
    ParseNode* body = BinaryRight(whileStmt);

    if (!f.writeOp(Expr::While))
        return false;

    Type condType;
    if (!CheckExpr(f, cond, &condType))
        return false;
    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    return MaybeAddInterruptCheck(f, InterruptCheckPosition::Loop, whileStmt) &&
           CheckStatement(f, body);
}

static bool
CheckFor(FunctionValidator& f, ParseNode* forStmt)
{
    MOZ_ASSERT(forStmt->isKind(PNK_FOR));
    ParseNode* forHead = BinaryLeft(forStmt);
    ParseNode* body = BinaryRight(forStmt);

    if (!forHead->isKind(PNK_FORHEAD))
        return f.fail(forHead, "unsupported for-loop statement");

    ParseNode* maybeInit = TernaryKid1(forHead);
    ParseNode* maybeCond = TernaryKid2(forHead);
    ParseNode* maybeInc = TernaryKid3(forHead);

    Expr stmt = maybeInit ? (maybeInc ? Expr::ForInitInc   : Expr::ForInitNoInc)
                          : (maybeInc ? Expr::ForNoInitInc : Expr::ForNoInitNoInc);
    if (!f.writeOp(stmt))
        return false;

    if (maybeInit && !CheckAsExprStatement(f, maybeInit))
        return false;

    if (maybeCond) {
        Type condType;
        if (!CheckExpr(f, maybeCond, &condType))
            return false;
        if (!condType.isInt())
            return f.failf(maybeCond, "%s is not a subtype of int", condType.toChars());
    } else if (!f.writeInt32Lit(1)) {
        return false;
    }

    if (!MaybeAddInterruptCheck(f, InterruptCheckPosition::Loop, forStmt))
        return false;

    if (!CheckStatement(f, body))
        return false;

    if (maybeInc && !CheckAsExprStatement(f, maybeInc))
        return false;

    return f.writeDebugCheckPoint();
}

static bool
CheckDoWhile(FunctionValidator& f, ParseNode* whileStmt)
{
    MOZ_ASSERT(whileStmt->isKind(PNK_DOWHILE));
    ParseNode* body = BinaryLeft(whileStmt);
    ParseNode* cond = BinaryRight(whileStmt);

    if (!f.writeOp(Expr::DoWhile))
        return false;

    if (!MaybeAddInterruptCheck(f, InterruptCheckPosition::Loop, cond))
        return false;

    if (!CheckStatement(f, body))
        return false;

    Type condType;
    if (!CheckExpr(f, cond, &condType))
        return false;
    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    return true;
}

static bool
CheckLabel(FunctionValidator& f, ParseNode* labeledStmt)
{
    MOZ_ASSERT(labeledStmt->isKind(PNK_LABEL));
    PropertyName* label = LabeledStatementLabel(labeledStmt);
    ParseNode* stmt = LabeledStatementStatement(labeledStmt);

    if (!f.writeOp(Expr::Label))
        return false;

    uint32_t labelId;
    if (!f.addLabel(label, &labelId))
        return false;

    if (!f.writeU32(labelId))
        return false;

    if (!CheckStatement(f, stmt))
        return false;

    f.removeLabel(label);
    return true;
}

static bool
CheckIf(FunctionValidator& f, ParseNode* ifStmt)
{
  recurse:
    size_t opcodeAt;
    if (!f.tempOp(&opcodeAt))
        return false;

    MOZ_ASSERT(ifStmt->isKind(PNK_IF));
    ParseNode* cond = TernaryKid1(ifStmt);
    ParseNode* thenStmt = TernaryKid2(ifStmt);
    ParseNode* elseStmt = TernaryKid3(ifStmt);

    Type condType;
    if (!CheckExpr(f, cond, &condType))
        return false;
    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    if (!CheckStatement(f, thenStmt))
        return false;

    if (!elseStmt) {
        f.patchOp(opcodeAt, Expr::If);
    } else {
        f.patchOp(opcodeAt, Expr::IfElse);

        if (elseStmt->isKind(PNK_IF)) {
            ifStmt = elseStmt;
            goto recurse;
        }

        if (!CheckStatement(f, elseStmt))
            return false;
    }

    return true;
}

static bool
CheckCaseExpr(FunctionValidator& f, ParseNode* caseExpr, int32_t* value)
{
    if (!IsNumericLiteral(f.m(), caseExpr))
        return f.fail(caseExpr, "switch case expression must be an integer literal");

    NumLit lit = ExtractNumericLiteral(f.m(), caseExpr);
    switch (lit.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
        *value = lit.toInt32();
        break;
      case NumLit::OutOfRangeInt:
      case NumLit::BigUnsigned:
        return f.fail(caseExpr, "switch case expression out of integer range");
      case NumLit::Double:
      case NumLit::Float:
      case NumLit::Int32x4:
      case NumLit::Float32x4:
      case NumLit::Bool32x4:
        return f.fail(caseExpr, "switch case expression must be an integer literal");
    }

    return true;
}

static bool
CheckDefaultAtEnd(FunctionValidator& f, ParseNode* stmt)
{
    for (; stmt; stmt = NextNode(stmt)) {
        if (IsDefaultCase(stmt) && NextNode(stmt) != nullptr)
            return f.fail(stmt, "default label must be at the end");
    }

    return true;
}

static bool
CheckSwitchRange(FunctionValidator& f, ParseNode* stmt, int32_t* low, int32_t* high,
                 int32_t* tableLength)
{
    if (IsDefaultCase(stmt)) {
        *low = 0;
        *high = -1;
        *tableLength = 0;
        return true;
    }

    int32_t i = 0;
    if (!CheckCaseExpr(f, CaseExpr(stmt), &i))
        return false;

    *low = *high = i;

    ParseNode* initialStmt = stmt;
    for (stmt = NextNode(stmt); stmt && !IsDefaultCase(stmt); stmt = NextNode(stmt)) {
        int32_t i = 0;
        if (!CheckCaseExpr(f, CaseExpr(stmt), &i))
            return false;

        *low = Min(*low, i);
        *high = Max(*high, i);
    }

    int64_t i64 = (int64_t(*high) - int64_t(*low)) + 1;
    if (i64 > 4*1024*1024)
        return f.fail(initialStmt, "all switch statements generate tables; this table would be too big");

    *tableLength = int32_t(i64);
    return true;
}

void
PatchSwitch(FunctionValidator& f,
            size_t hasDefaultAt, bool hasDefault,
            size_t lowAt, int32_t low,
            size_t highAt, int32_t high,
            size_t numCasesAt, uint32_t numCases)
{
    f.patchU8(hasDefaultAt, uint8_t(hasDefault));
    f.patch32(lowAt, low);
    f.patch32(highAt, high);
    f.patch32(numCasesAt, numCases);
}

static bool
CheckSwitch(FunctionValidator& f, ParseNode* switchStmt)
{
    MOZ_ASSERT(switchStmt->isKind(PNK_SWITCH));

    if (!f.writeOp(Expr::TableSwitch))
        return false;

    // Has default
    size_t hasDefaultAt;
    if (!f.tempU8(&hasDefaultAt))
        return false;

    // Low / High / Num cases
    size_t lowAt;
    if (!f.temp32(&lowAt))
        return false;
    size_t highAt;
    if (!f.temp32(&highAt))
        return false;
    size_t numCasesAt;
    if (!f.temp32(&numCasesAt))
        return false;

    ParseNode* switchExpr = BinaryLeft(switchStmt);
    ParseNode* switchBody = BinaryRight(switchStmt);

    if (!switchBody->isKind(PNK_STATEMENTLIST))
        return f.fail(switchBody, "switch body may not contain 'let' declarations");

    Type exprType;
    if (!CheckExpr(f, switchExpr, &exprType))
        return false;

    if (!exprType.isSigned())
        return f.failf(switchExpr, "%s is not a subtype of signed", exprType.toChars());

    ParseNode* stmt = ListHead(switchBody);

    if (!CheckDefaultAtEnd(f, stmt))
        return false;

    if (!stmt) {
        PatchSwitch(f, hasDefaultAt, false, lowAt, 0, highAt, 0, numCasesAt, 0);
        return true;
    }

    int32_t low = 0, high = 0, tableLength = 0;
    if (!CheckSwitchRange(f, stmt, &low, &high, &tableLength))
        return false;

    Vector<bool, 8> cases(f.cx());
    if (!cases.resize(tableLength))
        return false;

    uint32_t numCases = 0;
    for (; stmt && !IsDefaultCase(stmt); stmt = NextNode(stmt)) {
        int32_t caseValue = ExtractNumericLiteral(f.m(), CaseExpr(stmt)).toInt32();
        unsigned caseIndex = caseValue - low;

        if (cases[caseIndex])
            return f.fail(stmt, "no duplicate case labels");

        cases[caseIndex] = true;
        numCases += 1;
        if (!f.writeI32(caseValue))
            return false;

        if (!CheckStatement(f, CaseBody(stmt)))
            return false;
    }

    bool hasDefault = false;
    if (stmt && IsDefaultCase(stmt)) {
        hasDefault = true;
        if (!CheckStatement(f, CaseBody(stmt)))
            return false;
    }

    PatchSwitch(f, hasDefaultAt, hasDefault, lowAt, low, highAt, high, numCasesAt, numCases);
    return true;
}

static bool
CheckReturnType(FunctionValidator& f, ParseNode* usepn, ExprType ret)
{
    if (!f.hasAlreadyReturned()) {
        f.setReturnedType(ret);
        return true;
    }

    if (f.returnedType() != ret) {
        return f.failf(usepn, "%s incompatible with previous return of type %s",
                       Type::ret(ret).toChars(), Type::ret(f.returnedType()).toChars());
    }

    return true;
}

static bool
CheckReturn(FunctionValidator& f, ParseNode* returnStmt)
{
    ParseNode* expr = ReturnExpr(returnStmt);

    if (!f.writeOp(Expr::Return))
        return false;

    if (!expr)
        return CheckReturnType(f, returnStmt, ExprType::Void);

    Type type;
    if (!CheckExpr(f, expr, &type))
        return false;

    ExprType ret;
    if (type.isSigned())
        ret = ExprType::I32;
    else if (type.isFloat())
        ret = ExprType::F32;
    else if (type.isDouble())
        ret = ExprType::F64;
    else if (type.isInt32x4())
        ret = ExprType::I32x4;
    else if (type.isFloat32x4())
        ret = ExprType::F32x4;
    else if (type.isBool32x4())
        ret = ExprType::B32x4;
    else if (type.isVoid())
        ret = ExprType::Void;
    else
        return f.failf(expr, "%s is not a valid return type", type.toChars());

    return CheckReturnType(f, expr, ret);
}

static bool
CheckStatementList(FunctionValidator& f, ParseNode* stmtList)
{
    MOZ_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));

    if (!f.writeOp(Expr::Block) || !f.writeU32(ListLength(stmtList)))
        return false;

    for (ParseNode* stmt = ListHead(stmtList); stmt; stmt = NextNode(stmt)) {
        if (!CheckStatement(f, stmt))
            return false;
    }

    return f.writeDebugCheckPoint();
}

static bool
CheckBreakOrContinue(FunctionValidator& f, PropertyName* maybeLabel,
                     Expr withoutLabel, Expr withLabel)
{
    if (!maybeLabel)
        return f.writeOp(withoutLabel);

    if (!f.writeOp(withLabel))
        return false;

    uint32_t labelId = f.lookupLabel(maybeLabel);
    MOZ_ASSERT(labelId != uint32_t(-1));

    return f.writeU32(labelId);
}

static bool
CheckStatement(FunctionValidator& f, ParseNode* stmt)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    switch (stmt->getKind()) {
      case PNK_SEMI:          return CheckExprStatement(f, stmt);
      case PNK_WHILE:         return CheckWhile(f, stmt);
      case PNK_FOR:           return CheckFor(f, stmt);
      case PNK_DOWHILE:       return CheckDoWhile(f, stmt);
      case PNK_LABEL:         return CheckLabel(f, stmt);
      case PNK_IF:            return CheckIf(f, stmt);
      case PNK_SWITCH:        return CheckSwitch(f, stmt);
      case PNK_RETURN:        return CheckReturn(f, stmt);
      case PNK_STATEMENTLIST: return CheckStatementList(f, stmt);
      case PNK_BREAK:         return CheckBreakOrContinue(f, LoopControlMaybeLabel(stmt),
                                                          Expr::Break, Expr::BreakLabel);
      case PNK_CONTINUE:      return CheckBreakOrContinue(f, LoopControlMaybeLabel(stmt),
                                                          Expr::Continue, Expr::ContinueLabel);
      default:;
    }

    return f.fail(stmt, "unexpected statement kind");
}

static bool
ParseFunction(ModuleValidator& m, ParseNode** fnOut, unsigned* line, unsigned* column)
{
    TokenStream& tokenStream = m.tokenStream();

    tokenStream.consumeKnownToken(TOK_FUNCTION, TokenStream::Operand);
    tokenStream.srcCoords.lineNumAndColumnIndex(tokenStream.currentToken().pos.end, line, column);

    RootedPropertyName name(m.cx());

    TokenKind tk;
    if (!tokenStream.getToken(&tk, TokenStream::Operand))
        return false;
    if (tk == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tk == TOK_YIELD) {
        if (!m.parser().checkYieldNameValidity())
            return false;
        name = m.cx()->names().yield;
    } else {
        return false;  // The regular parser will throw a SyntaxError, no need to m.fail.
    }

    ParseNode* fn = m.parser().handler.newFunctionDefinition();
    if (!fn)
        return false;

    // This flows into FunctionBox, so must be tenured.
    RootedFunction fun(m.cx(),
                       NewScriptedFunction(m.cx(), 0, JSFunction::INTERPRETED,
                                           name, /* proto = */ nullptr, gc::AllocKind::FUNCTION,
                                           TenuredObject));
    if (!fun)
        return false;

    AsmJSParseContext* outerpc = m.parser().pc;

    Directives directives(outerpc);
    FunctionBox* funbox = m.parser().newFunctionBox(fn, fun, outerpc, directives, NotGenerator);
    if (!funbox)
        return false;

    Directives newDirectives = directives;
    AsmJSParseContext funpc(&m.parser(), outerpc, fn, funbox, &newDirectives);
    if (!funpc.init(m.parser()))
        return false;

    if (!m.parser().functionArgsAndBodyGeneric(InAllowed, YieldIsName, fn, fun, Statement)) {
        if (tokenStream.hadError() || directives == newDirectives)
            return false;

        return m.fail(fn, "encountered new directive in function");
    }

    MOZ_ASSERT(!tokenStream.hadError());
    MOZ_ASSERT(directives == newDirectives);

    fn->pn_blockid = outerpc->blockid();

    *fnOut = fn;
    return true;
}

static bool
CheckFunction(ModuleValidator& m)
{
    // asm.js modules can be quite large when represented as parse trees so pop
    // the backing LifoAlloc after parsing/compiling each function.
    AsmJSParser::Mark mark = m.parser().mark();

    int64_t before = PRMJ_Now();

    ParseNode* fn = nullptr;
    unsigned line = 0, column = 0;
    if (!ParseFunction(m, &fn, &line, &column))
        return false;

    if (!CheckFunctionHead(m, fn))
        return false;

    FunctionValidator f(m, fn);
    if (!f.init(FunctionName(fn), line, column))
        return m.fail(fn, "internal compiler failure (probably out of memory)");

    ParseNode* stmtIter = ListHead(FunctionStatementList(fn));

    if (!CheckProcessingDirectives(m, &stmtIter))
        return false;

    ValTypeVector args;
    if (!CheckArguments(f, &stmtIter, &args))
        return false;

    if (!MaybeAddInterruptCheck(f, InterruptCheckPosition::Head, fn))
        return false;

    if (!CheckVariables(f, &stmtIter))
        return false;

    ParseNode* lastNonEmptyStmt = nullptr;
    for (; stmtIter; stmtIter = NextNode(stmtIter)) {
        if (!CheckStatement(f, stmtIter))
            return false;
        if (!IsEmptyStatement(stmtIter))
            lastNonEmptyStmt = stmtIter;
    }

    if (!CheckFinalReturn(f, lastNonEmptyStmt))
        return false;

    ModuleValidator::Func* func = nullptr;
    if (!CheckFunctionSignature(m, fn, Sig(Move(args), f.returnedType()), FunctionName(fn), &func))
        return false;

    if (func->defined())
        return m.failName(fn, "function '%s' already defined", FunctionName(fn));

    func->define(fn);

    if (!f.finish(func->index(), (PRMJ_Now() - before) / PRMJ_USEC_PER_MSEC))
        return m.fail(fn, "internal compiler failure (probably out of memory)");

    // Release the parser's lifo memory only after the last use of a parse node.
    m.parser().release(mark);
    return true;
}

static bool
CheckAllFunctionsDefined(ModuleValidator& m)
{
    for (unsigned i = 0; i < m.numFunctions(); i++) {
        ModuleValidator::Func& f = m.function(i);
        if (!f.defined())
            return m.failNameOffset(f.firstUse(), "missing definition of function %s", f.name());
    }

    return true;
}

static bool
CheckFunctions(ModuleValidator& m)
{
    while (true) {
        TokenKind tk;
        if (!PeekToken(m.parser(), &tk))
            return false;

        if (tk != TOK_FUNCTION)
            break;

        if (!CheckFunction(m))
            return false;
    }

    return CheckAllFunctionsDefined(m);
}

static bool
CheckFuncPtrTable(ModuleValidator& m, ParseNode* var)
{
    if (!IsDefinition(var))
        return m.fail(var, "function-pointer table name must be unique");

    ParseNode* arrayLiteral = MaybeDefinitionInitializer(var);
    if (!arrayLiteral || !arrayLiteral->isKind(PNK_ARRAY))
        return m.fail(var, "function-pointer table's initializer must be an array literal");

    unsigned length = ListLength(arrayLiteral);

    if (!IsPowerOfTwo(length))
        return m.failf(arrayLiteral, "function-pointer table length must be a power of 2 (is %u)", length);

    unsigned mask = length - 1;

    Vector<uint32_t> elemFuncIndices(m.cx());
    const Sig* sig = nullptr;
    for (ParseNode* elem = ListHead(arrayLiteral); elem; elem = NextNode(elem)) {
        if (!elem->isKind(PNK_NAME))
            return m.fail(elem, "function-pointer table's elements must be names of functions");

        PropertyName* funcName = elem->name();
        const ModuleValidator::Func* func = m.lookupFunction(funcName);
        if (!func)
            return m.fail(elem, "function-pointer table's elements must be names of functions");

        const Sig& funcSig = m.mg().funcSig(func->index());
        if (sig) {
            if (*sig != funcSig)
                return m.fail(elem, "all functions in table must have same signature");
        } else {
            sig = &funcSig;
        }

        if (!elemFuncIndices.append(func->index()))
            return false;
    }

    Sig copy;
    if (!copy.clone(*sig))
        return false;

    uint32_t tableIndex;
    if (!CheckFuncPtrTableAgainstExisting(m, var, var->name(), Move(copy), mask, &tableIndex))
        return false;

    if (!m.defineFuncPtrTable(tableIndex, elemFuncIndices))
        return m.fail(var, "duplicate function-pointer definition");

    return true;
}

static bool
CheckFuncPtrTables(ModuleValidator& m)
{
    while (true) {
        ParseNode* varStmt;
        if (!ParseVarOrConstStatement(m.parser(), &varStmt))
            return false;
        if (!varStmt)
            break;
        for (ParseNode* var = VarListHead(varStmt); var; var = NextNode(var)) {
            if (!CheckFuncPtrTable(m, var))
                return false;
        }
    }

    for (unsigned i = 0; i < m.numFuncPtrTables(); i++) {
        ModuleValidator::FuncPtrTable& funcPtrTable = m.funcPtrTable(i);
        if (!funcPtrTable.defined()) {
            return m.failNameOffset(funcPtrTable.firstUse(),
                                    "function-pointer table %s wasn't defined",
                                    funcPtrTable.name());
        }
    }

    return true;
}

static bool
CheckModuleExportFunction(ModuleValidator& m, ParseNode* pn, PropertyName* maybeFieldName = nullptr)
{
    if (!pn->isKind(PNK_NAME))
        return m.fail(pn, "expected name of exported function");

    PropertyName* funcName = pn->name();
    const ModuleValidator::Global* global = m.lookupGlobal(funcName);
    if (!global)
        return m.failName(pn, "exported function name '%s' not found", funcName);

    if (global->which() != ModuleValidator::Global::Function)
        return m.failName(pn, "'%s' is not a function", funcName);

    return m.addExportField(pn, m.function(global->funcIndex()), maybeFieldName);
}

static bool
CheckModuleExportObject(ModuleValidator& m, ParseNode* object)
{
    MOZ_ASSERT(object->isKind(PNK_OBJECT));

    for (ParseNode* pn = ListHead(object); pn; pn = NextNode(pn)) {
        if (!IsNormalObjectField(m.cx(), pn))
            return m.fail(pn, "only normal object properties may be used in the export object literal");

        PropertyName* fieldName = ObjectNormalFieldName(m.cx(), pn);

        ParseNode* initNode = ObjectNormalFieldInitializer(m.cx(), pn);
        if (!initNode->isKind(PNK_NAME))
            return m.fail(initNode, "initializer of exported object literal must be name of function");

        if (!CheckModuleExportFunction(m, initNode, fieldName))
            return false;
    }

    return true;
}

static bool
CheckModuleReturn(ModuleValidator& m)
{
    TokenKind tk;
    if (!GetToken(m.parser(), &tk))
        return false;
    TokenStream& ts = m.parser().tokenStream;
    if (tk != TOK_RETURN) {
        return m.failCurrentOffset((tk == TOK_RC || tk == TOK_EOF)
                                   ? "expecting return statement"
                                   : "invalid asm.js. statement");
    }
    ts.ungetToken();

    ParseNode* returnStmt = m.parser().statement(YieldIsName);
    if (!returnStmt)
        return false;

    ParseNode* returnExpr = ReturnExpr(returnStmt);
    if (!returnExpr)
        return m.fail(returnStmt, "export statement must return something");

    if (returnExpr->isKind(PNK_OBJECT)) {
        if (!CheckModuleExportObject(m, returnExpr))
            return false;
    } else {
        if (!CheckModuleExportFunction(m, returnExpr))
            return false;
    }

    // Function statements are not added to the lexical scope in ParseContext
    // (since cx->tempLifoAlloc is marked/released after each function
    // statement) and thus all the identifiers in the return statement will be
    // mistaken as free variables and added to lexdeps. Clear these now.
    m.parser().pc->lexdeps->clear();
    return true;
}

static bool
CheckModuleEnd(ModuleValidator &m)
{
    TokenKind tk;
    if (!GetToken(m.parser(), &tk))
        return false;

    if (tk != TOK_EOF && tk != TOK_RC)
        return m.failCurrentOffset("top-level export (return) must be the last statement");

    m.parser().tokenStream.ungetToken();
    return true;
}

static bool
CheckModule(ExclusiveContext* cx, AsmJSParser& parser, ParseNode* stmtList,
            MutableHandle<WasmModuleObject*> moduleObj, unsigned* time,
            SlowFunctionVector* slowFuncs)
{
    int64_t before = PRMJ_Now();

    ParseNode* moduleFunctionNode = parser.pc->maybeFunction;
    MOZ_ASSERT(moduleFunctionNode);

    ModuleValidator m(cx, parser, moduleFunctionNode);
    if (!m.init())
        return false;

    if (!CheckFunctionHead(m, moduleFunctionNode))
        return false;

    if (!CheckModuleArguments(m, moduleFunctionNode))
        return false;

    if (!CheckPrecedingStatements(m, stmtList))
        return false;

    if (!CheckModuleProcessingDirectives(m))
        return false;

    if (!CheckModuleGlobals(m))
        return false;

    if (!m.startFunctionBodies())
        return false;

    if (!CheckFunctions(m))
        return false;

    if (!m.finishFunctionBodies())
        return false;

    if (!CheckFuncPtrTables(m))
        return false;

    if (!CheckModuleReturn(m))
        return false;

    if (!CheckModuleEnd(m))
        return false;

    if (!m.finish(moduleObj, slowFuncs))
        return false;

    *time = (PRMJ_Now() - before) / PRMJ_USEC_PER_MSEC;
    return true;
}

/*****************************************************************************/
// Link-time validation

static bool
LinkFail(JSContext* cx, const char* str)
{
    JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING, GetErrorMessage,
                                 nullptr, JSMSG_USE_ASM_LINK_FAIL, str);
    return false;
}

static bool
GetDataProperty(JSContext* cx, HandleValue objVal, HandlePropertyName field, MutableHandleValue v)
{
    if (!objVal.isObject())
        return LinkFail(cx, "accessing property of non-object");

    RootedObject obj(cx, &objVal.toObject());
    if (IsScriptedProxy(obj))
        return LinkFail(cx, "accessing property of a Proxy");

    Rooted<PropertyDescriptor> desc(cx);
    RootedId id(cx, NameToId(field));
    if (!GetPropertyDescriptor(cx, obj, id, &desc))
        return false;

    if (!desc.object())
        return LinkFail(cx, "property not present on object");

    if (!desc.isDataDescriptor())
        return LinkFail(cx, "property is not a data property");

    v.set(desc.value());
    return true;
}

static bool
HasPureCoercion(JSContext* cx, HandleValue v)
{
    if (IsVectorObject<Int32x4>(v) || IsVectorObject<Float32x4>(v) || IsVectorObject<Bool32x4>(v))
        return true;

    // Ideally, we'd reject all non-SIMD non-primitives, but Emscripten has a
    // bug that generates code that passes functions for some imports. To avoid
    // breaking all the code that contains this bug, we make an exception for
    // functions that don't have user-defined valueOf or toString, for their
    // coercions are not observable and coercion via ToNumber/ToInt32
    // definitely produces NaN/0. We should remove this special case later once
    // most apps have been built with newer Emscripten.
    jsid toString = NameToId(cx->names().toString);
    if (v.toObject().is<JSFunction>() &&
        HasObjectValueOf(&v.toObject(), cx) &&
        ClassMethodIsNative(cx, &v.toObject().as<JSFunction>(), &JSFunction::class_, toString, fun_toString))
    {
        return true;
    }

    return false;
}

static bool
ValidateGlobalVariable(JSContext* cx, const AsmJSGlobal& global, uint8_t* globalData,
                       HandleValue importVal)
{
    void* datum = globalData + global.varGlobalDataOffset();

    switch (global.varInitKind()) {
      case AsmJSGlobal::InitConstant: {
        Val v = global.varInitVal();
        switch (v.type()) {
          case ValType::I32:
            *(int32_t*)datum = v.i32();
            break;
          case ValType::I64:
            MOZ_CRASH("int64");
          case ValType::F32:
            *(float*)datum = v.f32();
            break;
          case ValType::F64:
            *(double*)datum = v.f64();
            break;
          case ValType::I32x4:
          case ValType::B32x4:
            // Bool32x4 uses the same data layout as Int32x4.
            memcpy(datum, v.i32x4(), Simd128DataSize);
            break;
          case ValType::F32x4:
            memcpy(datum, v.f32x4(), Simd128DataSize);
            break;
        }
        break;
      }

      case AsmJSGlobal::InitImport: {
        RootedPropertyName field(cx, global.varImportField());
        RootedValue v(cx);
        if (!GetDataProperty(cx, importVal, field, &v))
            return false;

        if (!v.isPrimitive() && !HasPureCoercion(cx, v))
            return LinkFail(cx, "Imported values must be primitives");

        switch (global.varInitImportType()) {
          case ValType::I32:
            if (!ToInt32(cx, v, (int32_t*)datum))
                return false;
            break;
          case ValType::I64:
            MOZ_CRASH("int64");
          case ValType::F32:
            if (!RoundFloat32(cx, v, (float*)datum))
                return false;
            break;
          case ValType::F64:
            if (!ToNumber(cx, v, (double*)datum))
                return false;
            break;
          case ValType::I32x4: {
            SimdConstant simdConstant;
            if (!ToSimdConstant<Int32x4>(cx, v, &simdConstant))
                return false;
            memcpy(datum, simdConstant.asInt32x4(), Simd128DataSize);
            break;
          }
          case ValType::F32x4: {
            SimdConstant simdConstant;
            if (!ToSimdConstant<Float32x4>(cx, v, &simdConstant))
                return false;
            memcpy(datum, simdConstant.asFloat32x4(), Simd128DataSize);
            break;
          }
          case ValType::B32x4: {
            SimdConstant simdConstant;
            if (!ToSimdConstant<Bool32x4>(cx, v, &simdConstant))
                return false;
            // Bool32x4 uses the same data layout as Int32x4.
            memcpy(datum, simdConstant.asInt32x4(), Simd128DataSize);
            break;
          }
        }
        break;
      }
    }

    return true;
}

static bool
ValidateFFI(JSContext* cx, const AsmJSGlobal& global, HandleValue importVal,
            MutableHandle<FunctionVector> ffis)
{
    RootedPropertyName field(cx, global.ffiField());
    RootedValue v(cx);
    if (!GetDataProperty(cx, importVal, field, &v))
        return false;

    if (!v.isObject() || !v.toObject().is<JSFunction>())
        return LinkFail(cx, "FFI imports must be functions");

    ffis[global.ffiIndex()].set(&v.toObject().as<JSFunction>());
    return true;
}

static bool
ValidateArrayView(JSContext* cx, const AsmJSGlobal& global, HandleValue globalVal)
{
    RootedPropertyName field(cx, global.maybeViewName());
    if (!field)
        return true;

    RootedValue v(cx);
    if (!GetDataProperty(cx, globalVal, field, &v))
        return false;

    bool tac = IsTypedArrayConstructor(v, global.viewType());
    if (!tac)
        return LinkFail(cx, "bad typed array constructor");

    return true;
}

static bool
ValidateMathBuiltinFunction(JSContext* cx, const AsmJSGlobal& global, HandleValue globalVal)
{
    RootedValue v(cx);
    if (!GetDataProperty(cx, globalVal, cx->names().Math, &v))
        return false;

    RootedPropertyName field(cx, global.mathName());
    if (!GetDataProperty(cx, v, field, &v))
        return false;

    Native native = nullptr;
    switch (global.mathBuiltinFunction()) {
      case AsmJSMathBuiltin_sin: native = math_sin; break;
      case AsmJSMathBuiltin_cos: native = math_cos; break;
      case AsmJSMathBuiltin_tan: native = math_tan; break;
      case AsmJSMathBuiltin_asin: native = math_asin; break;
      case AsmJSMathBuiltin_acos: native = math_acos; break;
      case AsmJSMathBuiltin_atan: native = math_atan; break;
      case AsmJSMathBuiltin_ceil: native = math_ceil; break;
      case AsmJSMathBuiltin_floor: native = math_floor; break;
      case AsmJSMathBuiltin_exp: native = math_exp; break;
      case AsmJSMathBuiltin_log: native = math_log; break;
      case AsmJSMathBuiltin_pow: native = math_pow; break;
      case AsmJSMathBuiltin_sqrt: native = math_sqrt; break;
      case AsmJSMathBuiltin_min: native = math_min; break;
      case AsmJSMathBuiltin_max: native = math_max; break;
      case AsmJSMathBuiltin_abs: native = math_abs; break;
      case AsmJSMathBuiltin_atan2: native = math_atan2; break;
      case AsmJSMathBuiltin_imul: native = math_imul; break;
      case AsmJSMathBuiltin_clz32: native = math_clz32; break;
      case AsmJSMathBuiltin_fround: native = math_fround; break;
    }

    if (!IsNativeFunction(v, native))
        return LinkFail(cx, "bad Math.* builtin function");

    return true;
}

static PropertyName*
SimdTypeToName(JSContext* cx, AsmJSSimdType type)
{
    switch (type) {
      case AsmJSSimdType_int32x4:   return cx->names().Int32x4;
      case AsmJSSimdType_float32x4: return cx->names().Float32x4;
      case AsmJSSimdType_bool32x4:  return cx->names().Bool32x4;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected SIMD type");
}

static SimdTypeDescr::Type
AsmJSSimdTypeToTypeDescrType(AsmJSSimdType type)
{
    switch (type) {
      case AsmJSSimdType_int32x4: return Int32x4::type;
      case AsmJSSimdType_float32x4: return Float32x4::type;
      case AsmJSSimdType_bool32x4: return Bool32x4::type;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected AsmJSSimdType");
}

static bool
ValidateSimdType(JSContext* cx, const AsmJSGlobal& global, HandleValue globalVal,
                 MutableHandleValue out)
{
    RootedValue v(cx);
    if (!GetDataProperty(cx, globalVal, cx->names().SIMD, &v))
        return false;

    AsmJSSimdType type;
    if (global.which() == AsmJSGlobal::SimdCtor)
        type = global.simdCtorType();
    else
        type = global.simdOperationType();

    RootedPropertyName simdTypeName(cx, SimdTypeToName(cx, type));
    if (!GetDataProperty(cx, v, simdTypeName, &v))
        return false;

    if (!v.isObject())
        return LinkFail(cx, "bad SIMD type");

    RootedObject simdDesc(cx, &v.toObject());
    if (!simdDesc->is<SimdTypeDescr>())
        return LinkFail(cx, "bad SIMD type");

    if (AsmJSSimdTypeToTypeDescrType(type) != simdDesc->as<SimdTypeDescr>().type())
        return LinkFail(cx, "bad SIMD type");

    out.set(v);
    return true;
}

static bool
ValidateSimdType(JSContext* cx, const AsmJSGlobal& global, HandleValue globalVal)
{
    RootedValue _(cx);
    return ValidateSimdType(cx, global, globalVal, &_);
}

static bool
ValidateSimdOperation(JSContext* cx, const AsmJSGlobal& global, HandleValue globalVal)
{
    // SIMD operations are loaded from the SIMD type, so the type must have been
    // validated before the operation.
    RootedValue v(cx);
    JS_ALWAYS_TRUE(ValidateSimdType(cx, global, globalVal, &v));

    RootedPropertyName opName(cx, global.simdOperationName());
    if (!GetDataProperty(cx, v, opName, &v))
        return false;

    Native native = nullptr;
    switch (global.simdOperationType()) {
#define SET_NATIVE_INT32X4(op) case AsmJSSimdOperation_##op: native = simd_int32x4_##op; break;
#define SET_NATIVE_FLOAT32X4(op) case AsmJSSimdOperation_##op: native = simd_float32x4_##op; break;
#define SET_NATIVE_BOOL32X4(op) case AsmJSSimdOperation_##op: native = simd_bool32x4_##op; break;
#define FALLTHROUGH(op) case AsmJSSimdOperation_##op:
      case AsmJSSimdType_int32x4:
        switch (global.simdOperation()) {
          FORALL_INT32X4_ASMJS_OP(SET_NATIVE_INT32X4)
          default:
            MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("shouldn't have been validated in the first "
                                                    "place");
        }
        break;
      case AsmJSSimdType_float32x4:
        switch (global.simdOperation()) {
          FORALL_FLOAT32X4_ASMJS_OP(SET_NATIVE_FLOAT32X4)
          default:
             MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("shouldn't have been validated in the first "
                                                     "place");
        }
        break;
      case AsmJSSimdType_bool32x4:
        switch (global.simdOperation()) {
          FORALL_BOOL_SIMD_OP(SET_NATIVE_BOOL32X4)
          default:
             MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("shouldn't have been validated in the first "
                                                     "place");
        }
        break;
#undef FALLTHROUGH
#undef SET_NATIVE_FLOAT32X4
#undef SET_NATIVE_INT32X4
#undef SET_NATIVE_BOOL32X4
#undef SET_NATIVE
    }
    if (!native || !IsNativeFunction(v, native))
        return LinkFail(cx, "bad SIMD.type.* operation");
    return true;
}

static bool
ValidateAtomicsBuiltinFunction(JSContext* cx, const AsmJSGlobal& global, HandleValue globalVal)
{
    RootedValue v(cx);
    if (!GetDataProperty(cx, globalVal, cx->names().Atomics, &v))
        return false;
    RootedPropertyName field(cx, global.atomicsName());
    if (!GetDataProperty(cx, v, field, &v))
        return false;

    Native native = nullptr;
    switch (global.atomicsBuiltinFunction()) {
      case AsmJSAtomicsBuiltin_compareExchange: native = atomics_compareExchange; break;
      case AsmJSAtomicsBuiltin_exchange: native = atomics_exchange; break;
      case AsmJSAtomicsBuiltin_load: native = atomics_load; break;
      case AsmJSAtomicsBuiltin_store: native = atomics_store; break;
      case AsmJSAtomicsBuiltin_fence: native = atomics_fence; break;
      case AsmJSAtomicsBuiltin_add: native = atomics_add; break;
      case AsmJSAtomicsBuiltin_sub: native = atomics_sub; break;
      case AsmJSAtomicsBuiltin_and: native = atomics_and; break;
      case AsmJSAtomicsBuiltin_or: native = atomics_or; break;
      case AsmJSAtomicsBuiltin_xor: native = atomics_xor; break;
      case AsmJSAtomicsBuiltin_isLockFree: native = atomics_isLockFree; break;
    }

    if (!IsNativeFunction(v, native))
        return LinkFail(cx, "bad Atomics.* builtin function");

    return true;
}

static bool
ValidateConstant(JSContext* cx, const AsmJSGlobal& global, HandleValue globalVal)
{
    RootedPropertyName field(cx, global.constantName());
    RootedValue v(cx, globalVal);

    if (global.constantKind() == AsmJSGlobal::MathConstant) {
        if (!GetDataProperty(cx, v, cx->names().Math, &v))
            return false;
    }

    if (!GetDataProperty(cx, v, field, &v))
        return false;

    if (!v.isNumber())
        return LinkFail(cx, "math / global constant value needs to be a number");

    // NaN != NaN
    if (IsNaN(global.constantValue())) {
        if (!IsNaN(v.toNumber()))
            return LinkFail(cx, "global constant value needs to be NaN");
    } else {
        if (v.toNumber() != global.constantValue())
            return LinkFail(cx, "global constant value mismatch");
    }

    return true;
}

static bool
CheckBuffer(JSContext* cx, AsmJSModule& module, HandleValue bufferVal,
            MutableHandle<ArrayBufferObjectMaybeShared*> buffer)
{
    if (module.hasSharedHeap() && !IsSharedArrayBuffer(bufferVal))
        return LinkFail(cx, "shared views can only be constructed onto SharedArrayBuffer");

    if (!module.hasSharedHeap() && !IsArrayBuffer(bufferVal))
        return LinkFail(cx, "unshared views can only be constructed onto ArrayBuffer");

    buffer.set(&AsAnyArrayBuffer(bufferVal));
    uint32_t heapLength = buffer->byteLength();

    if (!IsValidAsmJSHeapLength(heapLength)) {
        UniqueChars msg(
            JS_smprintf("ArrayBuffer byteLength 0x%x is not a valid heap length. The next "
                        "valid length is 0x%x",
                        heapLength,
                        RoundUpToNextValidAsmJSHeapLength(heapLength)));
        return LinkFail(cx, msg.get());
    }

    // This check is sufficient without considering the size of the loaded datum because heap
    // loads and stores start on an aligned boundary and the heap byteLength has larger alignment.
    MOZ_ASSERT((module.minHeapLength() - 1) <= INT32_MAX);
    if (heapLength < module.minHeapLength()) {
        UniqueChars msg(
            JS_smprintf("ArrayBuffer byteLength of 0x%x is less than 0x%x (the size implied "
                        "by const heap accesses).",
                        heapLength,
                        module.minHeapLength()));
        return LinkFail(cx, msg.get());
    }

    // Shell builtins may have disabled signal handlers since the module we're
    // cloning was compiled. LookupAsmJSModuleInCache checks for signal handlers
    // as well for the caching case.
    if (module.compileArgs() != CompileArgs(cx))
        return LinkFail(cx, "Signals have been toggled since compilation");

    if (buffer->is<ArrayBufferObject>()) {
        Rooted<ArrayBufferObject*> abheap(cx, &buffer->as<ArrayBufferObject>());
        bool useSignalHandlers = module.compileArgs().useSignalHandlersForOOB;
        if (!ArrayBufferObject::prepareForAsmJS(cx, abheap, useSignalHandlers))
            return LinkFail(cx, "Unable to prepare ArrayBuffer for asm.js use");
    }

    return true;
}

static bool
DynamicallyLinkModule(JSContext* cx, const CallArgs& args, AsmJSModule& module)
{
    HandleValue globalVal = args.get(0);
    HandleValue importVal = args.get(1);
    HandleValue bufferVal = args.get(2);

    Rooted<ArrayBufferObjectMaybeShared*> buffer(cx);
    if (module.usesHeap() && !CheckBuffer(cx, module, bufferVal, &buffer))
        return false;

    Rooted<FunctionVector> ffis(cx, FunctionVector(cx));
    if (!ffis.resize(module.numFFIs()))
        return false;

    for (const AsmJSGlobal& global : module.asmJSGlobals()) {
        switch (global.which()) {
          case AsmJSGlobal::Variable:
            if (!ValidateGlobalVariable(cx, global, module.globalData(), importVal))
                return false;
            break;
          case AsmJSGlobal::FFI:
            if (!ValidateFFI(cx, global, importVal, &ffis))
                return false;
            break;
          case AsmJSGlobal::ArrayView:
          case AsmJSGlobal::ArrayViewCtor:
            if (!ValidateArrayView(cx, global, globalVal))
                return false;
            break;
          case AsmJSGlobal::MathBuiltinFunction:
            if (!ValidateMathBuiltinFunction(cx, global, globalVal))
                return false;
            break;
          case AsmJSGlobal::AtomicsBuiltinFunction:
            if (!ValidateAtomicsBuiltinFunction(cx, global, globalVal))
                return false;
            break;
          case AsmJSGlobal::Constant:
            if (!ValidateConstant(cx, global, globalVal))
                return false;
            break;
          case AsmJSGlobal::SimdCtor:
            if (!ValidateSimdType(cx, global, globalVal))
                return false;
            break;
          case AsmJSGlobal::SimdOperation:
            if (!ValidateSimdOperation(cx, global, globalVal))
                return false;
            break;
        }
    }

    Rooted<FunctionVector> imports(cx, FunctionVector(cx));
    for (const AsmJSImport& import : module.asmJSImports()) {
        if (!imports.append(ffis[import.ffiIndex()]))
            return false;
    }

    return module.dynamicallyLink(cx, buffer, imports);
}

static bool
HandleDynamicLinkFailure(JSContext* cx, const CallArgs& args, AsmJSModule& module,
                         HandlePropertyName name)
{
    if (cx->isExceptionPending())
        return false;

    ScriptSource* source = module.scriptSource();

    // Source discarding is allowed to affect JS semantics because it is never
    // enabled for normal JS content.
    bool haveSource = source->hasSourceData();
    if (!haveSource && !JSScript::loadSource(cx, source, &haveSource))
        return false;
    if (!haveSource) {
        JS_ReportError(cx, "asm.js link failure with source discarding enabled");
        return false;
    }

    uint32_t begin = module.srcBodyStart();  // starts right after 'use asm'
    uint32_t end = module.srcEndBeforeCurly();
    Rooted<JSFlatString*> src(cx, source->substringDontDeflate(cx, begin, end));
    if (!src)
        return false;

    RootedFunction fun(cx, NewScriptedFunction(cx, 0, JSFunction::INTERPRETED_NORMAL,
                                               name, /* proto = */ nullptr, gc::AllocKind::FUNCTION,
                                               TenuredObject));
    if (!fun)
        return false;

    Rooted<PropertyNameVector> formals(cx, PropertyNameVector(cx));
    if (!formals.reserve(3))
        return false;

    if (module.globalArgumentName())
        formals.infallibleAppend(module.globalArgumentName());
    if (module.importArgumentName())
        formals.infallibleAppend(module.importArgumentName());
    if (module.bufferArgumentName())
        formals.infallibleAppend(module.bufferArgumentName());

    CompileOptions options(cx);
    options.setMutedErrors(source->mutedErrors())
           .setFile(source->filename())
           .setNoScriptRval(false);

    // The exported function inherits an implicit strict context if the module
    // also inherited it somehow.
    if (module.strict())
        options.strictOption = true;

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, src))
        return false;

    const char16_t* chars = stableChars.twoByteRange().start().get();
    SourceBufferHolder::Ownership ownership = stableChars.maybeGiveOwnershipToCaller()
                                              ? SourceBufferHolder::GiveOwnership
                                              : SourceBufferHolder::NoOwnership;
    SourceBufferHolder srcBuf(chars, end - begin, ownership);
    if (!frontend::CompileFunctionBody(cx, &fun, options, formals, srcBuf))
        return false;

    // Call the function we just recompiled.
    args.setCallee(ObjectValue(*fun));
    return Invoke(cx, args, args.isConstructing() ? CONSTRUCT : NO_CONSTRUCT);
}

static WasmModuleObject*
AsmJSModuleToModuleObject(JSFunction* fun)
{
    MOZ_ASSERT(IsAsmJSModule(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_MODULE_SLOT);
    return &v.toObject().as<WasmModuleObject>();
}

// Implements the semantics of an asm.js module function that has been successfully validated.
static bool
LinkAsmJS(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // The LinkAsmJS builtin (created by NewAsmJSModuleFunction) is an extended
    // function and stores its module in an extended slot.
    RootedFunction fun(cx, &args.callee().as<JSFunction>());
    Rooted<WasmModuleObject*> moduleObj(cx, AsmJSModuleToModuleObject(fun));
    AsmJSModule* module = &moduleObj->module().asAsmJS();

    // When a module is linked, it is dynamically specialized to the given
    // arguments (buffer, ffis). Thus, if the module is linked again (it is just
    // a function so it can be called multiple times), we need to clone a new
    // module.
    if (module->dynamicallyLinked()) {
        if (!module->clone(cx, &moduleObj))
            return false;

        module = &moduleObj->module().asAsmJS();

        if (!module->staticallyLink(cx))
            return false;
    }

    // Link the module by performing the link-time validation checks in the
    // asm.js spec and then patching the generated module to associate it with
    // the given heap (ArrayBuffer) and a new global data segment (the closure
    // state shared by the inner asm.js functions).
    if (!DynamicallyLinkModule(cx, args, *module)) {
        // Linking failed, so reparse the entire asm.js module from scratch to
        // get normal interpreted bytecode which we can simply Invoke. Very slow.
        RootedPropertyName name(cx, fun->name());
        return HandleDynamicLinkFailure(cx, args, *module, name);
    }

    // Link-time validation succeed!

    RootedObject exportObj(cx);
    if (!module->createExportObject(cx, moduleObj, module->exportMap(), &exportObj))
        return false;

    args.rval().set(ObjectValue(*exportObj));
    return true;
}

static JSFunction*
NewModuleFunction(ExclusiveContext* cx, JSFunction* origFun, HandleObject moduleObj)
{
    RootedPropertyName name(cx, origFun->name());

    JSFunction::Flags flags = origFun->isLambda() ? JSFunction::ASMJS_LAMBDA_CTOR
                                                  : JSFunction::ASMJS_CTOR;
    JSFunction* moduleFun =
        NewNativeConstructor(cx, LinkAsmJS, origFun->nargs(), name,
                             gc::AllocKind::FUNCTION_EXTENDED, TenuredObject,
                             flags);
    if (!moduleFun)
        return nullptr;

    moduleFun->setExtendedSlot(FunctionExtended::WASM_MODULE_SLOT, ObjectValue(*moduleObj));

    MOZ_ASSERT(IsAsmJSModule(moduleFun));
    return moduleFun;
}

/*****************************************************************************/
// Caching and cloning

uint8_t*
AsmJSGlobal::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    cursor = SerializeName(cursor, name_);
    return cursor;
}

size_t
AsmJSGlobal::serializedSize() const
{
    return sizeof(pod) +
           SerializedNameSize(name_);
}

const uint8_t*
AsmJSGlobal::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &pod, sizeof(pod))) &&
    (cursor = DeserializeName(cx, cursor, &name_));
    return cursor;
}

bool
AsmJSGlobal::clone(JSContext* cx, AsmJSGlobal* out) const
{
    *out = *this;
    return true;
}

size_t
AsmJSModuleData::serializedSize() const
{
    return sizeof(pod()) +
           SerializedVectorSize(globals) +
           SerializedPodVectorSize(imports) +
           SerializedPodVectorSize(exports) +
           exportMap.serializedSize() +
           SerializedNameSize(globalArgumentName) +
           SerializedNameSize(importArgumentName) +
           SerializedNameSize(bufferArgumentName);
}

uint8_t*
AsmJSModuleData::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
    cursor = SerializeVector(cursor, globals);
    cursor = SerializePodVector(cursor, imports);
    cursor = SerializePodVector(cursor, exports);
    cursor = exportMap.serialize(cursor);
    cursor = SerializeName(cursor, globalArgumentName);
    cursor = SerializeName(cursor, importArgumentName);
    cursor = SerializeName(cursor, bufferArgumentName);
    return cursor;
}

const uint8_t*
AsmJSModuleData::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &pod(), sizeof(pod()))) &&
    (cursor = DeserializeVector(cx, cursor, &globals)) &&
    (cursor = DeserializePodVector(cx, cursor, &imports)) &&
    (cursor = DeserializePodVector(cx, cursor, &exports)) &&
    (cursor = exportMap.deserialize(cx, cursor)) &&
    (cursor = DeserializeName(cx, cursor, &globalArgumentName)) &&
    (cursor = DeserializeName(cx, cursor, &importArgumentName)) &&
    (cursor = DeserializeName(cx, cursor, &bufferArgumentName));
    return cursor;
}

bool
AsmJSModuleData::clone(JSContext* cx, AsmJSModuleData* out) const
{
    out->pod() = pod();
    out->globalArgumentName = globalArgumentName;
    out->importArgumentName = importArgumentName;
    out->bufferArgumentName = bufferArgumentName;
    out->srcStart = srcStart;
    out->srcBodyStart = srcBodyStart;
    out->strict = strict;
    out->scriptSource.reset(scriptSource.get());
    return CloneVector(cx, globals, &out->globals) &&
           ClonePodVector(cx, imports, &out->imports) &&
           ClonePodVector(cx, exports, &out->exports) &&
           exportMap.clone(cx, &out->exportMap);
}

size_t
AsmJSModuleData::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return globals.sizeOfExcludingThis(mallocSizeOf) +
           imports.sizeOfExcludingThis(mallocSizeOf) +
           exports.sizeOfExcludingThis(mallocSizeOf) +
           exportMap.sizeOfExcludingThis(mallocSizeOf);
}

size_t
AsmJSModule::serializedSize() const
{
    return base().serializedSize() +
           link_->serializedSize() +
           module_->serializedSize();
}

uint8_t*
AsmJSModule::serialize(uint8_t* cursor) const
{
    cursor = base().serialize(cursor);
    cursor = link_->serialize(cursor);
    cursor = module_->serialize(cursor);
    return cursor;
}

/* static */ const uint8_t*
AsmJSModule::deserialize(ExclusiveContext* cx, const uint8_t* cursor, AsmJSParser& parser,
                         MutableHandle<WasmModuleObject*> moduleObj)
{
    moduleObj.set(WasmModuleObject::create(cx));
    if (!moduleObj)
        return nullptr;

    // Deserialization GC-allocates a bunch of atoms and stores them in unrooted
    // Vectors so, for simplicity, inhibit GC of the atoms zone.
    AutoKeepAtoms aka(cx->perThreadData);

    UniqueModuleData base = cx->make_unique<ModuleData>();
    if (!base)
        return nullptr;
    cursor = base->deserialize(cx, cursor);
    if (!cursor)
        return nullptr;

    MOZ_ASSERT(!base->loadedFromCache);
    base->loadedFromCache = true;

    UniqueStaticLinkData link = cx->make_unique<StaticLinkData>();
    if (!link)
        return nullptr;
    cursor = link->deserialize(cx, cursor);
    if (!cursor)
        return nullptr;

    UniqueAsmJSModuleData module = cx->make_unique<AsmJSModuleData>();
    if (!module)
        return nullptr;
    cursor = module->deserialize(cx, cursor);
    if (!cursor)
        return nullptr;

    // See AsmJSModuleData comment as well as ModuleValidator::init().
    module->srcStart = parser.pc->maybeFunction->pn_body->pn_pos.begin;
    module->srcBodyStart = parser.tokenStream.currentToken().pos.end;
    module->strict = parser.pc->sc->strict() && !parser.pc->sc->hasExplicitUseStrict();
    module->scriptSource.reset(parser.ss);

    if (!moduleObj->init(cx->new_<AsmJSModule>(Move(base), Move(link), Move(module))))
        return nullptr;

    return cursor;
}

bool
AsmJSModule::clone(JSContext* cx, MutableHandle<WasmModuleObject*> moduleObj) const
{
    moduleObj.set(WasmModuleObject::create(cx));
    if (!moduleObj)
        return false;

    // Prevent any GC that may move the temporarily-unrooted atoms being cloned.
    AutoKeepAtoms aka(cx->perThreadData);

    UniqueModuleData base = cx->make_unique<ModuleData>();
    if (!base || !this->base().clone(cx, base.get()))
        return false;

    UniqueStaticLinkData link = cx->make_unique<StaticLinkData>();
    if (!link || !link_->clone(cx, link.get()))
        return false;

    UniqueAsmJSModuleData module = cx->make_unique<AsmJSModuleData>();
    if (!module || !module_->clone(cx, module.get()))
        return false;

    if (!moduleObj->init(cx->new_<AsmJSModule>(Move(base), Move(link), Move(module))))
        return false;

    return Module::clone(cx, *link_, &moduleObj->module());
}

namespace {

struct PropertyNameWrapper
{
    PropertyName* name;

    PropertyNameWrapper()
      : name(nullptr)
    {}
    explicit PropertyNameWrapper(PropertyName* name)
      : name(name)
    {}
    size_t serializedSize() const {
        return SerializedNameSize(name);
    }
    uint8_t* serialize(uint8_t* cursor) const {
        return SerializeName(cursor, name);
    }
    const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor) {
        return DeserializeName(cx, cursor, &name);
    }
};

class ModuleChars
{
  protected:
    uint32_t isFunCtor_;
    Vector<PropertyNameWrapper, 0, SystemAllocPolicy> funCtorArgs_;

  public:
    static uint32_t beginOffset(AsmJSParser& parser) {
        return parser.pc->maybeFunction->pn_pos.begin;
    }

    static uint32_t endOffset(AsmJSParser& parser) {
        TokenPos pos(0, 0);  // initialize to silence GCC warning
        MOZ_ALWAYS_TRUE(parser.tokenStream.peekTokenPos(&pos, TokenStream::Operand));
        return pos.end;
    }
};

class ModuleCharsForStore : ModuleChars
{
    uint32_t uncompressedSize_;
    uint32_t compressedSize_;
    Vector<char, 0, SystemAllocPolicy> compressedBuffer_;

  public:
    bool init(AsmJSParser& parser) {
        MOZ_ASSERT(beginOffset(parser) < endOffset(parser));

        uncompressedSize_ = (endOffset(parser) - beginOffset(parser)) * sizeof(char16_t);
        size_t maxCompressedSize = LZ4::maxCompressedSize(uncompressedSize_);
        if (maxCompressedSize < uncompressedSize_)
            return false;

        if (!compressedBuffer_.resize(maxCompressedSize))
            return false;

        const char16_t* chars = parser.tokenStream.rawCharPtrAt(beginOffset(parser));
        const char* source = reinterpret_cast<const char*>(chars);
        size_t compressedSize = LZ4::compress(source, uncompressedSize_, compressedBuffer_.begin());
        if (!compressedSize || compressedSize > UINT32_MAX)
            return false;

        compressedSize_ = compressedSize;

        // For a function statement or named function expression:
        //   function f(x,y,z) { abc }
        // the range [beginOffset, endOffset) captures the source:
        //   f(x,y,z) { abc }
        // An unnamed function expression captures the same thing, sans 'f'.
        // Since asm.js modules do not contain any free variables, equality of
        // [beginOffset, endOffset) is sufficient to guarantee identical code
        // generation, modulo MachineId.
        //
        // For functions created with 'new Function', function arguments are
        // not present in the source so we must manually explicitly serialize
        // and match the formals as a Vector of PropertyName.
        isFunCtor_ = parser.pc->isFunctionConstructorBody();
        if (isFunCtor_) {
            unsigned numArgs;
            ParseNode* arg = FunctionArgsList(parser.pc->maybeFunction, &numArgs);
            for (unsigned i = 0; i < numArgs; i++, arg = arg->pn_next) {
                if (!funCtorArgs_.append(arg->name()))
                    return false;
            }
        }

        return true;
    }

    size_t serializedSize() const {
        return sizeof(uint32_t) +
               sizeof(uint32_t) +
               compressedSize_ +
               sizeof(uint32_t) +
               (isFunCtor_ ? SerializedVectorSize(funCtorArgs_) : 0);
    }

    uint8_t* serialize(uint8_t* cursor) const {
        cursor = WriteScalar<uint32_t>(cursor, uncompressedSize_);
        cursor = WriteScalar<uint32_t>(cursor, compressedSize_);
        cursor = WriteBytes(cursor, compressedBuffer_.begin(), compressedSize_);
        cursor = WriteScalar<uint32_t>(cursor, isFunCtor_);
        if (isFunCtor_)
            cursor = SerializeVector(cursor, funCtorArgs_);
        return cursor;
    }
};

class ModuleCharsForLookup : ModuleChars
{
    Vector<char16_t, 0, SystemAllocPolicy> chars_;

  public:
    const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor) {
        uint32_t uncompressedSize;
        cursor = ReadScalar<uint32_t>(cursor, &uncompressedSize);

        uint32_t compressedSize;
        cursor = ReadScalar<uint32_t>(cursor, &compressedSize);

        if (!chars_.resize(uncompressedSize / sizeof(char16_t)))
            return nullptr;

        const char* source = reinterpret_cast<const char*>(cursor);
        char* dest = reinterpret_cast<char*>(chars_.begin());
        if (!LZ4::decompress(source, dest, uncompressedSize))
            return nullptr;

        cursor += compressedSize;

        cursor = ReadScalar<uint32_t>(cursor, &isFunCtor_);
        if (isFunCtor_)
            cursor = DeserializeVector(cx, cursor, &funCtorArgs_);

        return cursor;
    }

    bool match(AsmJSParser& parser) const {
        const char16_t* parseBegin = parser.tokenStream.rawCharPtrAt(beginOffset(parser));
        const char16_t* parseLimit = parser.tokenStream.rawLimit();
        MOZ_ASSERT(parseLimit >= parseBegin);
        if (uint32_t(parseLimit - parseBegin) < chars_.length())
            return false;
        if (!PodEqual(chars_.begin(), parseBegin, chars_.length()))
            return false;
        if (isFunCtor_ != parser.pc->isFunctionConstructorBody())
            return false;
        if (isFunCtor_) {
            // For function statements, the closing } is included as the last
            // character of the matched source. For Function constructor,
            // parsing terminates with EOF which we must explicitly check. This
            // prevents
            //   new Function('"use asm"; function f() {} return f')
            // from incorrectly matching
            //   new Function('"use asm"; function f() {} return ff')
            if (parseBegin + chars_.length() != parseLimit)
                return false;
            unsigned numArgs;
            ParseNode* arg = FunctionArgsList(parser.pc->maybeFunction, &numArgs);
            if (funCtorArgs_.length() != numArgs)
                return false;
            for (unsigned i = 0; i < funCtorArgs_.length(); i++, arg = arg->pn_next) {
                if (funCtorArgs_[i].name != arg->name())
                    return false;
            }
        }
        return true;
    }
};

} // unnamed namespace

static JS::AsmJSCacheResult
StoreAsmJSModuleInCache(AsmJSParser& parser, const AsmJSModule& module, ExclusiveContext* cx)
{
    MachineId machineId;
    if (!machineId.extractCurrentState(cx))
        return JS::AsmJSCache_InternalError;

    ModuleCharsForStore moduleChars;
    if (!moduleChars.init(parser))
        return JS::AsmJSCache_InternalError;

    size_t serializedSize = machineId.serializedSize() +
                            moduleChars.serializedSize() +
                            module.serializedSize();

    JS::OpenAsmJSCacheEntryForWriteOp open = cx->asmJSCacheOps().openEntryForWrite;
    if (!open)
        return JS::AsmJSCache_Disabled_Internal;

    const char16_t* begin = parser.tokenStream.rawCharPtrAt(ModuleChars::beginOffset(parser));
    const char16_t* end = parser.tokenStream.rawCharPtrAt(ModuleChars::endOffset(parser));
    bool installed = parser.options().installedFile;

    ScopedCacheEntryOpenedForWrite entry(cx, serializedSize);
    JS::AsmJSCacheResult openResult =
        open(cx->global(), installed, begin, end, serializedSize, &entry.memory, &entry.handle);
    if (openResult != JS::AsmJSCache_Success)
        return openResult;

    uint8_t* cursor = entry.memory;
    cursor = machineId.serialize(cursor);
    cursor = moduleChars.serialize(cursor);
    cursor = module.serialize(cursor);

    MOZ_ASSERT(cursor == entry.memory + serializedSize);
    return JS::AsmJSCache_Success;
}

static bool
LookupAsmJSModuleInCache(ExclusiveContext* cx, AsmJSParser& parser, bool* loadedFromCache,
                         MutableHandle<WasmModuleObject*> moduleObj,
                         UniqueChars* compilationTimeReport)
{
    int64_t usecBefore = PRMJ_Now();

    *loadedFromCache = false;

    MachineId machineId;
    if (!machineId.extractCurrentState(cx))
        return true;

    JS::OpenAsmJSCacheEntryForReadOp open = cx->asmJSCacheOps().openEntryForRead;
    if (!open)
        return true;

    const char16_t* begin = parser.tokenStream.rawCharPtrAt(ModuleChars::beginOffset(parser));
    const char16_t* limit = parser.tokenStream.rawLimit();

    ScopedCacheEntryOpenedForRead entry(cx);
    if (!open(cx->global(), begin, limit, &entry.serializedSize, &entry.memory, &entry.handle))
        return true;

    const uint8_t* cursor = entry.memory;

    MachineId cachedMachineId;
    cursor = cachedMachineId.deserialize(cx, cursor);
    if (!cursor)
        return false;
    if (machineId != cachedMachineId)
        return true;

    ModuleCharsForLookup moduleChars;
    cursor = moduleChars.deserialize(cx, cursor);
    if (!moduleChars.match(parser))
        return true;

    cursor = AsmJSModule::deserialize(cx, cursor, parser, moduleObj);
    if (!cursor)
        return false;

    bool atEnd = cursor == entry.memory + entry.serializedSize;
    MOZ_ASSERT(atEnd, "Corrupt cache file");
    if (!atEnd)
        return true;

    AsmJSModule& module = moduleObj->module().asAsmJS();

    if (module.compileArgs() != CompileArgs(cx))
        return true;

    if (!module.staticallyLink(cx))
        return false;

    if (!parser.tokenStream.advance(module.srcEndBeforeCurly()))
        return false;

    *loadedFromCache = true;

    int64_t usecAfter = PRMJ_Now();
    int ms = (usecAfter - usecBefore) / PRMJ_USEC_PER_MSEC;
    *compilationTimeReport = UniqueChars(JS_smprintf("loaded from cache in %dms", ms));
    return true;
}

/*****************************************************************************/
// Top-level js::CompileAsmJS

static bool
NoExceptionPending(ExclusiveContext* cx)
{
    return !cx->isJSContext() || !cx->asJSContext()->isExceptionPending();
}

static bool
Warn(AsmJSParser& parser, int errorNumber, const char* str)
{
    ParseReportKind reportKind = parser.options().throwOnAsmJSValidationFailureOption &&
                                 errorNumber == JSMSG_USE_ASM_TYPE_FAIL
                                 ? ParseError
                                 : ParseWarning;
    parser.reportNoOffset(reportKind, /* strict = */ false, errorNumber, str ? str : "");
    return false;
}

static bool
EstablishPreconditions(ExclusiveContext* cx, AsmJSParser& parser)
{
#if defined(JS_CODEGEN_NONE) || defined(JS_CODEGEN_ARM64)
    return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by lack of a JIT compiler");
#endif

    if (!cx->jitSupportsFloatingPoint())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by lack of floating point support");

    if (cx->gcSystemPageSize() != AsmJSPageSize)
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by non 4KiB system page size");

    switch (parser.options().asmJSOption) {
      case AsmJSOption::Disabled:
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by javascript.options.asmjs in about:config");
      case AsmJSOption::DisabledByDebugger:
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by debugger");
      case AsmJSOption::Enabled:
        break;
    }

    if (parser.pc->isGenerator())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by generator context");

    if (parser.pc->isArrowFunction())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by arrow function context");

    // Class constructors are also methods
    if (parser.pc->isMethod())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by class constructor or method context");

    return true;
}

static UniqueChars
BuildConsoleMessage(ExclusiveContext* cx, AsmJSModule& module, unsigned time,
                    const SlowFunctionVector& slowFuncs, JS::AsmJSCacheResult cacheResult)
{
#ifndef JS_MORE_DETERMINISTIC
    UniqueChars slowText;
    if (!slowFuncs.empty()) {
        slowText.reset(JS_smprintf("; %d functions compiled slowly: ", slowFuncs.length()));
        if (!slowText)
            return nullptr;

        for (unsigned i = 0; i < slowFuncs.length(); i++) {
            const SlowFunction& func = slowFuncs[i];
            JSAutoByteString name;
            if (!AtomToPrintableString(cx, func.name, &name))
                return nullptr;

            slowText.reset(JS_smprintf("%s%s:%u:%u (%ums)%s", slowText.get(),
                                       name.ptr(), func.line, func.column, func.ms,
                                       i+1 < slowFuncs.length() ? ", " : ""));
            if (!slowText)
                return nullptr;
        }
    }

    const char* cacheString = "";
    switch (cacheResult) {
      case JS::AsmJSCache_Success:
        cacheString = "stored in cache";
        break;
      case JS::AsmJSCache_ModuleTooSmall:
        cacheString = "not stored in cache (too small to benefit)";
        break;
      case JS::AsmJSCache_SynchronousScript:
        cacheString = "unable to cache asm.js in synchronous scripts; try loading "
                      "asm.js via <script async> or createElement('script')";
        break;
      case JS::AsmJSCache_QuotaExceeded:
        cacheString = "not enough temporary storage quota to store in cache";
        break;
      case JS::AsmJSCache_StorageInitFailure:
        cacheString = "storage initialization failed (consider filing a bug)";
        break;
      case JS::AsmJSCache_Disabled_Internal:
        cacheString = "caching disabled by internal configuration (consider filing a bug)";
        break;
      case JS::AsmJSCache_Disabled_ShellFlags:
        cacheString = "caching disabled by missing command-line arguments";
        break;
      case JS::AsmJSCache_Disabled_JitInspector:
        cacheString = "caching disabled by active JIT inspector";
        break;
      case JS::AsmJSCache_InternalError:
        cacheString = "unable to store in cache due to internal error (consider filing a bug)";
        break;
      case JS::AsmJSCache_LIMIT:
        MOZ_CRASH("bad AsmJSCacheResult");
        break;
    }

    return UniqueChars(JS_smprintf("total compilation time %dms; %s%s",
                                   time, cacheString, slowText ? slowText.get() : ""));
#else
    return DuplicateString("");
#endif
}

bool
js::CompileAsmJS(ExclusiveContext* cx, AsmJSParser& parser, ParseNode* stmtList, bool* validated)
{
    *validated = false;

    // Various conditions disable asm.js optimizations.
    if (!EstablishPreconditions(cx, parser))
        return NoExceptionPending(cx);


    // Before spending any time parsing the module, try to look it up in the
    // embedding's cache using the chars about to be parsed as the key.
    Rooted<WasmModuleObject*> moduleObj(cx);
    bool loadedFromCache;
    UniqueChars message;
    if (!LookupAsmJSModuleInCache(cx, parser, &loadedFromCache, &moduleObj, &message))
        return false;

    // If not present in the cache, parse, validate and generate code in a
    // single linear pass over the chars of the asm.js module.
    if (!loadedFromCache) {
        // "Checking" parses, validates and compiles, producing a fully compiled
        // WasmModuleObject as result.
        unsigned time;
        SlowFunctionVector slowFuncs(cx);
        if (!CheckModule(cx, parser, stmtList, &moduleObj, &time, &slowFuncs))
            return NoExceptionPending(cx);

        // Try to store the AsmJSModule in the embedding's cache. The
        // AsmJSModule must be stored before static linking since static linking
        // specializes the AsmJSModule to the current process's address space
        // and therefore must be executed after a cache hit.
        AsmJSModule& module = moduleObj->module().asAsmJS();
        JS::AsmJSCacheResult cacheResult = StoreAsmJSModuleInCache(parser, module, cx);
        if (!module.staticallyLink(cx))
            return false;

        message = BuildConsoleMessage(cx, module, time, slowFuncs, cacheResult);
        if (!message)
            return NoExceptionPending(cx);
    }

    // The module function dynamically links the AsmJSModule when called and
    // generates a set of functions wrapping all the exports.
    FunctionBox* funbox = parser.pc->maybeFunction->pn_funbox;
    RootedFunction moduleFun(cx, NewModuleFunction(cx, funbox->function(), moduleObj));
    if (!moduleFun)
        return false;

    // Finished! Clobber the default function created by the parser with the new
    // asm.js module function. Special cases in the bytecode emitter avoid
    // generating bytecode for asm.js functions, allowing this asm.js module
    // function to be the finished result.
    MOZ_ASSERT(funbox->function()->isInterpreted());
    funbox->object = moduleFun;

    // Success! Write to the console with a "warning" message.
    *validated = true;
    Warn(parser, JSMSG_USE_ASM_TYPE_OK, message.get());
    return NoExceptionPending(cx);
}

/*****************************************************************************/
// asm.js testing functions

bool
js::IsAsmJSModuleNative(Native native)
{
    return native == LinkAsmJS;
}

bool
js::IsAsmJSModule(JSFunction* fun)
{
    return fun->maybeNative() == LinkAsmJS;
}

bool
js::IsAsmJSFunction(JSFunction* fun)
{
    if (IsExportedFunction(fun))
        return ExportedFunctionToModuleObject(fun)->module().isAsmJS();
    return false;
}

bool
js::IsAsmJSCompilationAvailable(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // See EstablishPreconditions.
#if defined(JS_CODEGEN_NONE) || defined(JS_CODEGEN_ARM64)
    bool available = false;
#else
    bool available = cx->jitSupportsFloatingPoint() &&
                     cx->gcSystemPageSize() == AsmJSPageSize &&
                     cx->runtime()->options().asmJS();
#endif

    args.rval().set(BooleanValue(available));
    return true;
}

static JSFunction*
MaybeWrappedNativeFunction(const Value& v)
{
    if (!v.isObject())
        return nullptr;

    JSObject* obj = CheckedUnwrap(&v.toObject());
    if (!obj)
        return nullptr;

    if (!obj->is<JSFunction>())
        return nullptr;

    return &obj->as<JSFunction>();
}

bool
js::IsAsmJSModule(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool rval = false;
    if (JSFunction* fun = MaybeWrappedNativeFunction(args.get(0)))
        rval = IsAsmJSModule(fun);

    args.rval().set(BooleanValue(rval));
    return true;
}

bool
js::IsAsmJSFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool rval = false;
    if (JSFunction* fun = MaybeWrappedNativeFunction(args.get(0)))
        rval = IsAsmJSFunction(fun);

    args.rval().set(BooleanValue(rval));
    return true;
}

bool
js::IsAsmJSModuleLoadedFromCache(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSFunction* fun = MaybeWrappedNativeFunction(args.get(0));
    if (!fun || !IsAsmJSModule(fun)) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_USE_ASM_TYPE_FAIL,
                             "argument passed to isAsmJSModuleLoadedFromCache is not a "
                             "validated asm.js module");
        return false;
    }

    bool loadedFromCache = AsmJSModuleToModuleObject(fun)->module().loadedFromCache();

    args.rval().set(BooleanValue(loadedFromCache));
    return true;
}

/*****************************************************************************/
// asm.js toString/toSource support

static bool
AppendUseStrictSource(JSContext* cx, HandleFunction fun, Handle<JSFlatString*> src, StringBuffer& out)
{
    // We need to add "use strict" in the body right after the opening
    // brace.
    size_t bodyStart = 0, bodyEnd;

    // No need to test for functions created with the Function ctor as
    // these don't implicitly inherit the "use strict" context. Strict mode is
    // enabled for functions created with the Function ctor only if they begin with
    // the "use strict" directive, but these functions won't validate as asm.js
    // modules.

    if (!FindBody(cx, fun, src, &bodyStart, &bodyEnd))
        return false;

    return out.appendSubstring(src, 0, bodyStart) &&
           out.append("\n\"use strict\";\n") &&
           out.appendSubstring(src, bodyStart, src->length() - bodyStart);
}

JSString*
js::AsmJSModuleToString(JSContext* cx, HandleFunction fun, bool addParenToLambda)
{
    MOZ_ASSERT(IsAsmJSModule(fun));

    AsmJSModule& module = AsmJSModuleToModuleObject(fun)->module().asAsmJS();
    uint32_t begin = module.srcStart();
    uint32_t end = module.srcEndAfterCurly();
    ScriptSource* source = module.scriptSource();
    StringBuffer out(cx);

    if (addParenToLambda && fun->isLambda() && !out.append("("))
        return nullptr;

    if (!out.append("function "))
        return nullptr;

    if (fun->atom() && !out.append(fun->atom()))
        return nullptr;

    bool haveSource = source->hasSourceData();
    if (!haveSource && !JSScript::loadSource(cx, source, &haveSource))
        return nullptr;

    if (!haveSource) {
        if (!out.append("() {\n    [sourceless code]\n}"))
            return nullptr;
    } else {
        // Whether the function has been created with a Function ctor
        bool funCtor = begin == 0 && end == source->length() && source->argumentsNotIncluded();
        if (funCtor) {
            // Functions created with the function constructor don't have arguments in their source.
            if (!out.append("("))
                return nullptr;

            if (PropertyName* argName = module.globalArgumentName()) {
                if (!out.append(argName))
                    return nullptr;
            }
            if (PropertyName* argName = module.importArgumentName()) {
                if (!out.append(", ") || !out.append(argName))
                    return nullptr;
            }
            if (PropertyName* argName = module.bufferArgumentName()) {
                if (!out.append(", ") || !out.append(argName))
                    return nullptr;
            }

            if (!out.append(") {\n"))
                return nullptr;
        }

        Rooted<JSFlatString*> src(cx, source->substring(cx, begin, end));
        if (!src)
            return nullptr;

        if (module.strict()) {
            if (!AppendUseStrictSource(cx, fun, src, out))
                return nullptr;
        } else {
            if (!out.append(src))
                return nullptr;
        }

        if (funCtor && !out.append("\n}"))
            return nullptr;
    }

    if (addParenToLambda && fun->isLambda() && !out.append(")"))
        return nullptr;

    return out.finishString();
}

JSString*
js::AsmJSFunctionToString(JSContext* cx, HandleFunction fun)
{
    MOZ_ASSERT(IsAsmJSFunction(fun));

    AsmJSModule& module = ExportedFunctionToModuleObject(fun)->module().asAsmJS();
    const AsmJSExport& f = module.asmJSExports()[ExportedFunctionToIndex(fun)];
    uint32_t begin = module.srcStart() + f.startOffsetInModule();
    uint32_t end = module.srcStart() + f.endOffsetInModule();

    ScriptSource* source = module.scriptSource();
    StringBuffer out(cx);

    if (!out.append("function "))
        return nullptr;

    bool haveSource = source->hasSourceData();
    if (!haveSource && !JSScript::loadSource(cx, source, &haveSource))
        return nullptr;

    if (!haveSource) {
        // asm.js functions can't be anonymous
        MOZ_ASSERT(fun->atom());
        if (!out.append(fun->atom()))
            return nullptr;
        if (!out.append("() {\n    [sourceless code]\n}"))
            return nullptr;
    } else {
        // asm.js functions cannot have been created with a Function constructor
        // as they belong within a module.
        MOZ_ASSERT(!(begin == 0 && end == source->length() && source->argumentsNotIncluded()));

        if (module.strict()) {
            // AppendUseStrictSource expects its input to start right after the
            // function name, so split the source chars from the src into two parts:
            // the function name and the rest (arguments + body).

            // asm.js functions can't be anonymous
            MOZ_ASSERT(fun->atom());
            if (!out.append(fun->atom()))
                return nullptr;

            size_t nameEnd = begin + fun->atom()->length();
            Rooted<JSFlatString*> src(cx, source->substring(cx, nameEnd, end));
            if (!src || !AppendUseStrictSource(cx, fun, src, out))
                return nullptr;
        } else {
            Rooted<JSFlatString*> src(cx, source->substring(cx, begin, end));
            if (!src)
                return nullptr;
            if (!out.append(src))
                return nullptr;
        }
    }

    return out.finishString();
}

/*****************************************************************************/
// asm.js heap

static const size_t MinHeapLength = 64 * 1024;
static_assert(MinHeapLength % AsmJSPageSize == 0, "Invalid page size");

#if defined(ASMJS_MAY_USE_SIGNAL_HANDLERS_FOR_OOB)

// Targets define AsmJSImmediateRange to be the size of an address immediate,
// and AsmJSCheckedImmediateRange, to be the size of an address immediate that
// can be supported by signal-handler OOB handling.
static_assert(AsmJSCheckedImmediateRange <= AsmJSImmediateRange,
              "AsmJSImmediateRange should be the size of an unconstrained "
              "address immediate");

// To support the use of signal handlers for catching Out Of Bounds accesses,
// the internal ArrayBuffer data array is inflated to 4GiB (only the
// byteLength portion of which is accessible) so that out-of-bounds accesses
// (made using a uint32 index) are guaranteed to raise a SIGSEGV.
// Then, an additional extent is added to permit folding of immediate
// values into addresses. And finally, unaligned accesses and mask optimizations
// might also try to access a few bytes after this limit, so just inflate it by
// AsmJSPageSize.
const size_t js::AsmJSMappedSize = 4 * 1024ULL * 1024ULL * 1024ULL +
                                   AsmJSImmediateRange +
                                   AsmJSPageSize;

#endif // ASMJS_MAY_USE_SIGNAL_HANDLERS_FOR_OOB

// From the asm.js spec Linking section:
//  the heap object's byteLength must be either
//    2^n for n in [12, 24)
//  or
//    2^24 * n for n >= 1.

bool
js::IsValidAsmJSHeapLength(uint32_t length)
{
    bool valid = length >= MinHeapLength &&
                 (IsPowerOfTwo(length) ||
                  (length & 0x00ffffff) == 0);

    MOZ_ASSERT_IF(valid, length % AsmJSPageSize == 0);
    MOZ_ASSERT_IF(valid, length == RoundUpToNextValidAsmJSHeapLength(length));

    return valid;
}

uint32_t
js::RoundUpToNextValidAsmJSHeapLength(uint32_t length)
{
    if (length <= MinHeapLength)
        return MinHeapLength;

    if (length <= 16 * 1024 * 1024)
        return mozilla::RoundUpPow2(length);

    MOZ_ASSERT(length <= 0xff000000);
    return (length + 0x00ffffff) & ~0x00ffffff;
}

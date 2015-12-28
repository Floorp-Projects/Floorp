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

#ifndef asmjs_AsmJSModule_h
#define asmjs_AsmJSModule_h

#include "mozilla/EnumeratedArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/Move.h"
#include "mozilla/PodOperations.h"

#include "asmjs/AsmJSValidate.h"
#include "asmjs/WasmModule.h"
#include "builtin/SIMD.h"
#include "gc/Tracer.h"
#include "vm/TypedArrayObject.h"

namespace js {

class AsmJSModuleObject;
typedef Handle<AsmJSModuleObject*> HandleAsmJSModule;

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

// An AsmJSModule extends (via containment) a wasm::Module with the extra persistent state
// necessary to represent a compiled asm.js module.
class AsmJSModule
{
  public:
    class Global
    {
      public:
        enum Which { Variable, FFI, ArrayView, ArrayViewCtor, MathBuiltinFunction,
                     AtomicsBuiltinFunction, Constant, SimdCtor, SimdOperation, ByteLength };
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
                        wasm::ValType importType_;
                        wasm::Val val_;
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

        friend class AsmJSModule;

        Global(Which which, PropertyName* name) {
            mozilla::PodZero(&pod);  // zero padding for Valgrind
            pod.which_ = which;
            name_ = name;
            MOZ_ASSERT_IF(name_, name_->isTenured());
        }

        void trace(JSTracer* trc) {
            if (name_)
                TraceManuallyBarrieredEdge(trc, &name_, "asm.js global name");
        }

      public:
        Global() {}
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
        wasm::Val varInitVal() const {
            MOZ_ASSERT(pod.which_ == Variable);
            MOZ_ASSERT(pod.u.var.initKind_ == InitConstant);
            return pod.u.var.u.val_;
        }
        wasm::ValType varInitImportType() const {
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

        WASM_DECLARE_SERIALIZABLE(Global);
    };

    typedef Vector<Global, 0, SystemAllocPolicy> GlobalVector;

    class Import
    {
        uint32_t ffiIndex_;
      public:
        Import() = default;
        explicit Import(uint32_t ffiIndex) : ffiIndex_(ffiIndex) {}
        uint32_t ffiIndex() const { return ffiIndex_; }
    };

    typedef Vector<Import, 0, SystemAllocPolicy> ImportVector;

    class Export
    {
        PropertyName* name_;
        PropertyName* maybeFieldName_;
        struct CacheablePod {
            uint32_t wasmIndex_;
            uint32_t startOffsetInModule_;  // Store module-start-relative offsets
            uint32_t endOffsetInModule_;    // so preserved by serialization.
        } pod;

      public:
        Export() {}
        Export(PropertyName* name, PropertyName* maybeFieldName, uint32_t wasmIndex,
               uint32_t startOffsetInModule, uint32_t endOffsetInModule)
          : name_(name),
            maybeFieldName_(maybeFieldName)
        {
            MOZ_ASSERT(name_->isTenured());
            MOZ_ASSERT_IF(maybeFieldName_, maybeFieldName_->isTenured());
            pod.wasmIndex_ = wasmIndex;
            pod.startOffsetInModule_ = startOffsetInModule;
            pod.endOffsetInModule_ = endOffsetInModule;
        }

        void trace(JSTracer* trc) {
            TraceManuallyBarrieredEdge(trc, &name_, "asm.js export name");
            if (maybeFieldName_)
                TraceManuallyBarrieredEdge(trc, &maybeFieldName_, "asm.js export field");
        }

        PropertyName* name() const {
            return name_;
        }
        PropertyName* maybeFieldName() const {
            return maybeFieldName_;
        }
        uint32_t startOffsetInModule() const {
            return pod.startOffsetInModule_;
        }
        uint32_t endOffsetInModule() const {
            return pod.endOffsetInModule_;
        }
        static const uint32_t ChangeHeap = UINT32_MAX;
        bool isChangeHeap() const {
            return pod.wasmIndex_ == ChangeHeap;
        }
        uint32_t wasmIndex() const {
            MOZ_ASSERT(!isChangeHeap());
            return pod.wasmIndex_;
        }

        WASM_DECLARE_SERIALIZABLE(Export)
    };

    typedef Vector<Export, 0, SystemAllocPolicy> ExportVector;

    typedef JS::UniquePtr<wasm::Module, JS::DeletePolicy<wasm::Module>> UniqueWasmModule;

  private:
    UniqueWasmModule            wasm_;
    wasm::UniqueStaticLinkData  linkData_;
    struct CacheablePod {
        uint32_t                minHeapLength_;
        uint32_t                maxHeapLength_;
        uint32_t                heapLengthMask_;
        uint32_t                numFFIs_;
        uint32_t                srcLength_;
        uint32_t                srcLengthWithRightBrace_;
        bool                    strict_;
        bool                    hasArrayView_;
        bool                    isSharedView_;
        bool                    hasFixedMinHeapLength_;
    } pod;
    const ScriptSourceHolder    scriptSource_;
    const uint32_t              srcStart_;
    const uint32_t              srcBodyStart_;
    GlobalVector                globals_;
    ImportVector                imports_;
    ExportVector                exports_;
    PropertyName*               globalArgumentName_;
    PropertyName*               importArgumentName_;
    PropertyName*               bufferArgumentName_;

  public:
    explicit AsmJSModule(ScriptSource* scriptSource, uint32_t srcStart, uint32_t srcBodyStart,
                         bool strict);
    void trace(JSTracer* trc);

    /*************************************************************************/
    // These functions may be used as soon as the module is constructed:

    ScriptSource* scriptSource() const {
        return scriptSource_.get();
    }
    bool strict() const {
        return pod.strict_;
    }

    // srcStart() refers to the offset in the ScriptSource to the beginning of
    // the asm.js module function. If the function has been created with the
    // Function constructor, this will be the first character in the function
    // source. Otherwise, it will be the opening parenthesis of the arguments
    // list.
    uint32_t srcStart() const {
        return srcStart_;
    }

    // srcBodyStart() refers to the offset in the ScriptSource to the end
    // of the 'use asm' string-literal token.
    uint32_t srcBodyStart() const {
        return srcBodyStart_;
    }

    // While these functions may be accessed at any time, their values will
    // change as the module is compiled.
    uint32_t minHeapLength() const {
        return pod.minHeapLength_;
    }
    uint32_t maxHeapLength() const {
        return pod.maxHeapLength_;
    }
    uint32_t heapLengthMask() const {
        MOZ_ASSERT(pod.hasFixedMinHeapLength_);
        return pod.heapLengthMask_;
    }

    void initGlobalArgumentName(PropertyName* n) {
        MOZ_ASSERT(!isFinished());
        MOZ_ASSERT_IF(n, n->isTenured());
        globalArgumentName_ = n;
    }
    void initImportArgumentName(PropertyName* n) {
        MOZ_ASSERT(!isFinished());
        MOZ_ASSERT_IF(n, n->isTenured());
        importArgumentName_ = n;
    }
    void initBufferArgumentName(PropertyName* n) {
        MOZ_ASSERT(!isFinished());
        MOZ_ASSERT_IF(n, n->isTenured());
        bufferArgumentName_ = n;
    }
    PropertyName* globalArgumentName() const {
        return globalArgumentName_;
    }
    PropertyName* importArgumentName() const {
        return importArgumentName_;
    }
    PropertyName* bufferArgumentName() const {
        return bufferArgumentName_;
    }

    bool addGlobalVarInit(const wasm::Val& v, uint32_t globalDataOffset) {
        MOZ_ASSERT(!isFinished());
        Global g(Global::Variable, nullptr);
        g.pod.u.var.initKind_ = Global::InitConstant;
        g.pod.u.var.u.val_ = v;
        g.pod.u.var.globalDataOffset_ = globalDataOffset;
        return globals_.append(g);
    }
    bool addGlobalVarImport(PropertyName* name, wasm::ValType importType, uint32_t globalDataOffset) {
        MOZ_ASSERT(!isFinished());
        Global g(Global::Variable, name);
        g.pod.u.var.initKind_ = Global::InitImport;
        g.pod.u.var.u.importType_ = importType;
        g.pod.u.var.globalDataOffset_ = globalDataOffset;
        return globals_.append(g);
    }
    bool addFFI(PropertyName* field, uint32_t* ffiIndex) {
        MOZ_ASSERT(!isFinished());
        if (pod.numFFIs_ == UINT32_MAX)
            return false;
        Global g(Global::FFI, field);
        g.pod.u.ffiIndex_ = *ffiIndex = pod.numFFIs_++;
        return globals_.append(g);
    }
    bool addArrayView(Scalar::Type vt, PropertyName* maybeField) {
        MOZ_ASSERT(!isFinished());
        pod.hasArrayView_ = true;
        pod.isSharedView_ = false;
        Global g(Global::ArrayView, maybeField);
        g.pod.u.viewType_ = vt;
        return globals_.append(g);
    }
    bool addArrayViewCtor(Scalar::Type vt, PropertyName* field) {
        MOZ_ASSERT(!isFinished());
        MOZ_ASSERT(field);
        pod.isSharedView_ = false;
        Global g(Global::ArrayViewCtor, field);
        g.pod.u.viewType_ = vt;
        return globals_.append(g);
    }
    bool addByteLength() {
        MOZ_ASSERT(!isFinished());
        Global g(Global::ByteLength, nullptr);
        return globals_.append(g);
    }
    bool addMathBuiltinFunction(AsmJSMathBuiltinFunction func, PropertyName* field) {
        MOZ_ASSERT(!isFinished());
        Global g(Global::MathBuiltinFunction, field);
        g.pod.u.mathBuiltinFunc_ = func;
        return globals_.append(g);
    }
    bool addMathBuiltinConstant(double value, PropertyName* field) {
        MOZ_ASSERT(!isFinished());
        Global g(Global::Constant, field);
        g.pod.u.constant.value_ = value;
        g.pod.u.constant.kind_ = Global::MathConstant;
        return globals_.append(g);
    }
    bool addAtomicsBuiltinFunction(AsmJSAtomicsBuiltinFunction func, PropertyName* field) {
        MOZ_ASSERT(!isFinished());
        Global g(Global::AtomicsBuiltinFunction, field);
        g.pod.u.atomicsBuiltinFunc_ = func;
        return globals_.append(g);
    }
    bool addSimdCtor(AsmJSSimdType type, PropertyName* field) {
        MOZ_ASSERT(!isFinished());
        Global g(Global::SimdCtor, field);
        g.pod.u.simdCtorType_ = type;
        return globals_.append(g);
    }
    bool addSimdOperation(AsmJSSimdType type, AsmJSSimdOperation op, PropertyName* field) {
        MOZ_ASSERT(!isFinished());
        Global g(Global::SimdOperation, field);
        g.pod.u.simdOp.type_ = type;
        g.pod.u.simdOp.which_ = op;
        return globals_.append(g);
    }
    bool addGlobalConstant(double value, PropertyName* name) {
        MOZ_ASSERT(!isFinished());
        Global g(Global::Constant, name);
        g.pod.u.constant.value_ = value;
        g.pod.u.constant.kind_ = Global::GlobalConstant;
        return globals_.append(g);
    }
    bool addImport(uint32_t ffiIndex, uint32_t importIndex) {
        MOZ_ASSERT(imports_.length() == importIndex);
        return imports_.emplaceBack(ffiIndex);
    }
    bool addExport(PropertyName* name, PropertyName* maybeFieldName, uint32_t wasmIndex,
                   uint32_t funcSrcBegin, uint32_t funcSrcEnd)
    {
        // NB: funcSrcBegin/funcSrcEnd are given relative to the ScriptSource
        // (the entire file) and ExportedFunctions store offsets relative to
        // the beginning of the module (so that they are caching-invariant).
        MOZ_ASSERT(!isFinished());
        MOZ_ASSERT(srcStart_ < funcSrcBegin);
        MOZ_ASSERT(funcSrcBegin < funcSrcEnd);
        return exports_.emplaceBack(name, maybeFieldName, wasmIndex,
                                    funcSrcBegin - srcStart_, funcSrcEnd - srcStart_);
    }
    bool addChangeHeap(uint32_t mask, uint32_t min, uint32_t max) {
        MOZ_ASSERT(!isFinished());
        MOZ_ASSERT(!pod.hasFixedMinHeapLength_);
        MOZ_ASSERT(IsValidAsmJSHeapLength(mask + 1));
        MOZ_ASSERT(min >= RoundUpToNextValidAsmJSHeapLength(0));
        MOZ_ASSERT(max <= pod.maxHeapLength_);
        MOZ_ASSERT(min <= max);
        pod.heapLengthMask_ = mask;
        pod.minHeapLength_ = min;
        pod.maxHeapLength_ = max;
        pod.hasFixedMinHeapLength_ = true;
        return true;
    }

    const GlobalVector& globals() const {
        return globals_;
    }
    const ImportVector& imports() const {
        return imports_;
    }
    const ExportVector& exports() const {
        return exports_;
    }

    void setViewsAreShared() {
        if (pod.hasArrayView_)
            pod.isSharedView_ = true;
    }
    bool hasArrayView() const {
        return pod.hasArrayView_;
    }
    bool isSharedView() const {
        return pod.isSharedView_;
    }
    bool tryRequireHeapLengthToBeAtLeast(uint32_t len) {
        MOZ_ASSERT(!isFinished());
        if (pod.hasFixedMinHeapLength_ && len > pod.minHeapLength_)
            return false;
        if (len > pod.maxHeapLength_)
            return false;
        len = RoundUpToNextValidAsmJSHeapLength(len);
        if (len > pod.minHeapLength_)
            pod.minHeapLength_ = len;
        return true;
    }

    /*************************************************************************/
    // A module isFinished() when compilation completes. After being finished,
    // a module must be statically and dynamically linked before execution.

    bool isFinished() const {
        return !!wasm_;
    }
    void finish(wasm::Module* wasm, wasm::UniqueStaticLinkData linkData,
                uint32_t endBeforeCurly, uint32_t endAfterCurly);

    /*************************************************************************/
    // These accessor functions can only be used after finish():

    wasm::Module& wasm() const {
        MOZ_ASSERT(isFinished());
        return *wasm_;
    }
    uint32_t numFFIs() const {
        MOZ_ASSERT(isFinished());
        return pod.numFFIs_;
    }
    uint32_t srcEndBeforeCurly() const {
        MOZ_ASSERT(isFinished());
        return srcStart_ + pod.srcLength_;
    }
    uint32_t srcEndAfterCurly() const {
        MOZ_ASSERT(isFinished());
        return srcStart_ + pod.srcLengthWithRightBrace_;
    }
    bool staticallyLink(ExclusiveContext* cx) {
        return wasm_->staticallyLink(cx, *linkData_);
    }

    // See WASM_DECLARE_SERIALIZABLE.
    size_t serializedSize() const;
    uint8_t* serialize(uint8_t* cursor) const;
    const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor);
    bool clone(JSContext* cx, HandleAsmJSModule moduleObj) const;
    void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* asmJSModuleCode,
                       size_t* asmJSModuleData);
};

// Store the just-parsed module in the cache using AsmJSCacheOps.
extern JS::AsmJSCacheResult
StoreAsmJSModuleInCache(AsmJSParser& parser,
                        const AsmJSModule& module,
                        ExclusiveContext* cx);

// Attempt to load the asm.js module that is about to be parsed from the cache
// using AsmJSCacheOps. The return value indicates whether an error was
// reported. The loadedFromCache outparam indicates whether the module was
// successfully loaded and stored in moduleObj.extern bool
extern bool
LookupAsmJSModuleInCache(ExclusiveContext* cx, AsmJSParser& parser, HandleAsmJSModule moduleObj,
                         bool* loadedFromCache, UniqueChars* compilationTimeReport);

// This function must be called for every detached ArrayBuffer.
extern bool
OnDetachAsmJSArrayBuffer(JSContext* cx, Handle<ArrayBufferObject*> buffer);

// An AsmJSModuleObject is an internal implementation object (i.e., not exposed
// directly to user script) which manages the lifetime of an AsmJSModule. A
// JSObject is necessary since we want LinkAsmJS/CallAsmJS JSFunctions to be
// able to point to their module via their extended slots.
class AsmJSModuleObject : public NativeObject
{
    static const unsigned MODULE_SLOT = 0;

  public:
    static const unsigned RESERVED_SLOTS = 1;

    static AsmJSModuleObject* create(ExclusiveContext* cx);

    bool hasModule() const;
    void setModule(AsmJSModule* module);
    AsmJSModule& module() const;

    void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* asmJSModuleCode,
                       size_t* asmJSModuleData) {
        module().addSizeOfMisc(mallocSizeOf, asmJSModuleCode, asmJSModuleData);
    }

    static const Class class_;
};

} // namespace js

#endif /* asmjs_AsmJSModule_h */

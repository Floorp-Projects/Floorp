/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_asmjsmodule_h__)
#define jsion_asmjsmodule_h__

#include "gc/Marking.h"
#include "ion/RegisterSets.h"

#include "jstypedarrayinlines.h"

namespace js {

// The basis of the asm.js type system is the EcmaScript-defined coercions
// ToInt32 and ToNumber.
enum AsmJSCoercion
{
    AsmJS_ToInt32,
    AsmJS_ToNumber
};

// The asm.js spec recognizes this set of builtin Math functions.
enum AsmJSMathBuiltin
{
    AsmJSMathBuiltin_sin, AsmJSMathBuiltin_cos, AsmJSMathBuiltin_tan,
    AsmJSMathBuiltin_asin, AsmJSMathBuiltin_acos, AsmJSMathBuiltin_atan,
    AsmJSMathBuiltin_ceil, AsmJSMathBuiltin_floor, AsmJSMathBuiltin_exp,
    AsmJSMathBuiltin_log, AsmJSMathBuiltin_pow, AsmJSMathBuiltin_sqrt,
    AsmJSMathBuiltin_abs, AsmJSMathBuiltin_atan2, AsmJSMathBuiltin_imul
};

// An asm.js module represents the collection of functions nested inside a
// single outer "use asm" function. For example, this asm.js module:
//   function() { "use asm"; function f() {} function g() {} return f }
// contains the functions 'f' and 'g'.
//
// An asm.js module contains both the jit-code produced by compiling all the
// functions in the module as well all the data required to perform the
// link-time validation step in the asm.js spec.
//
// NB: this means that AsmJSModule must be GC-safe.
class AsmJSModule
{
  public:
    class Global
    {
      public:
        enum Which { Variable, FFI, ArrayView, MathBuiltin, Constant };
        enum VarInitKind { InitConstant, InitImport };

      private:
        Which which_;
        union {
            struct {
                uint32_t index_;
                VarInitKind initKind_;
                union {
                    Value constant_;
                    AsmJSCoercion coercion_;
                } init;
            } var;
            uint32_t ffiIndex_;
            ArrayBufferView::ViewType viewType_;
            AsmJSMathBuiltin mathBuiltin_;
            double constantValue_;
        } u;
        HeapPtrPropertyName name_;

        friend class AsmJSModule;
        Global(Which which) : which_(which) {}

        void trace(JSTracer *trc) {
            if (name_)
                MarkString(trc, &name_, "asm.js global name");
        }

      public:
        Which which() const {
            return which_;
        }
        uint32_t varIndex() const {
            JS_ASSERT(which_ == Variable);
            return u.var.index_;
        }
        VarInitKind varInitKind() const {
            JS_ASSERT(which_ == Variable);
            return u.var.initKind_;
        }
        const Value &varInitConstant() const {
            JS_ASSERT(which_ == Variable);
            JS_ASSERT(u.var.initKind_ == InitConstant);
            return u.var.init.constant_;
        }
        AsmJSCoercion varImportCoercion() const {
            JS_ASSERT(which_ == Variable);
            JS_ASSERT(u.var.initKind_ == InitImport);
            return u.var.init.coercion_;
        }
        PropertyName *varImportField() const {
            JS_ASSERT(which_ == Variable);
            JS_ASSERT(u.var.initKind_ == InitImport);
            return name_;
        }
        PropertyName *ffiField() const {
            JS_ASSERT(which_ == FFI);
            return name_;
        }
        uint32_t ffiIndex() const {
            JS_ASSERT(which_ == FFI);
            return u.ffiIndex_;
        }
        PropertyName *viewName() const {
            JS_ASSERT(which_ == ArrayView);
            return name_;
        }
        ArrayBufferView::ViewType viewType() const {
            JS_ASSERT(which_ == ArrayView);
            return u.viewType_;
        }
        PropertyName *mathName() const {
            JS_ASSERT(which_ == MathBuiltin);
            return name_;
        }
        AsmJSMathBuiltin mathBuiltin() const {
            JS_ASSERT(which_ == MathBuiltin);
            return u.mathBuiltin_;
        }
        PropertyName *constantName() const {
            JS_ASSERT(which_ == Constant);
            return name_;
        }
        double constantValue() const {
            JS_ASSERT(which_ == Constant);
            return u.constantValue_;
        }
    };

    class Exit
    {
        unsigned ffiIndex_;
        union {
            unsigned codeOffset_;
            uint8_t *code_;
        } u;

      public:
        Exit(unsigned ffiIndex)
          : ffiIndex_(ffiIndex)
        {
          u.codeOffset_ = 0;
        }
        unsigned ffiIndex() const {
            return ffiIndex_;
        }
        void initCodeOffset(unsigned off) {
            JS_ASSERT(!u.codeOffset_);
            u.codeOffset_ = off;
        }
        void patch(uint8_t *baseAddress) {
            u.code_ = baseAddress + u.codeOffset_;
        }
        uint8_t *code() const {
            return u.code_;
        }
    };

    typedef int32_t (*CodePtr)(uint64_t *args);

    typedef Vector<AsmJSCoercion, 0, SystemAllocPolicy> ArgCoercionVector;

    enum ReturnType { Return_Int32, Return_Double, Return_Void };

    class ExportedFunction
    {
      public:

      private:

        HeapPtrFunction fun_;
        HeapPtrPropertyName maybeFieldName_;
        ArgCoercionVector argCoercions_;
        ReturnType returnType_;
        bool hasCodePtr_;
        union {
            unsigned codeOffset_;
            CodePtr code_;
        } u;

        friend class AsmJSModule;

        ExportedFunction(JSFunction *fun,
                         PropertyName *maybeFieldName,
                         MoveRef<ArgCoercionVector> argCoercions,
                         ReturnType returnType)
          : fun_(fun),
            maybeFieldName_(maybeFieldName),
            argCoercions_(argCoercions),
            returnType_(returnType),
            hasCodePtr_(false)
        {
            u.codeOffset_ = 0;
        }

        void trace(JSTracer *trc) {
            MarkObject(trc, &fun_, "asm.js export name");
            if (maybeFieldName_)
                MarkString(trc, &maybeFieldName_, "asm.js export field");
        }

      public:
        ExportedFunction(MoveRef<ExportedFunction> rhs)
          : fun_(rhs->fun_),
            maybeFieldName_(rhs->maybeFieldName_),
            argCoercions_(Move(rhs->argCoercions_)),
            returnType_(rhs->returnType_),
            hasCodePtr_(rhs->hasCodePtr_),
            u(rhs->u)
        {}

        void initCodeOffset(unsigned off) {
            JS_ASSERT(!hasCodePtr_); 
            JS_ASSERT(!u.codeOffset_);
            u.codeOffset_ = off;
        }
        void patch(uint8_t *baseAddress) {
            JS_ASSERT(!hasCodePtr_);
            JS_ASSERT(u.codeOffset_);
            hasCodePtr_ = true;
            u.code_ = JS_DATA_TO_FUNC_PTR(CodePtr, baseAddress + u.codeOffset_);
        }

        PropertyName *name() const {
            return fun_->name();
        }
        JSFunction *unclonedFunObj() const {
            return fun_;
        }
        PropertyName *maybeFieldName() const {
            return maybeFieldName_;
        }
        unsigned numArgs() const {
            return argCoercions_.length();
        }
        AsmJSCoercion argCoercion(unsigned i) const {
            return argCoercions_[i];
        }
        ReturnType returnType() const {
            return returnType_;
        }
        CodePtr code() const {
            JS_ASSERT(hasCodePtr_);
            return u.code_;
        }
    };

  private:
    typedef Vector<ExportedFunction, 0, SystemAllocPolicy> ExportedFunctionVector;
    typedef Vector<Global, 0, SystemAllocPolicy> GlobalVector;
    typedef Vector<Exit, 0, SystemAllocPolicy> ExitVector;
    typedef Vector<ion::AsmJSHeapAccess, 0, SystemAllocPolicy> HeapAccessVector;

    GlobalVector                          globals_;
    ExitVector                            exits_;
    ExportedFunctionVector                exports_;
    HeapAccessVector                      heapAccesses_;
    uint32_t                              numGlobalVars_;
    uint32_t                              numFFIs_;
    uint32_t                              numFuncPtrTableElems_;
    bool                                  hasArrayView_;

    ScopedReleasePtr<JSC::ExecutablePool> codePool_;
    uint8_t *                             code_;
    uint8_t *                             operationCallbackExit_;
    size_t                                functionBytes_;
    size_t                                codeBytes_;
    size_t                                totalBytes_;

    bool                                  linked_;
    HeapPtr<ArrayBufferObject>            maybeHeap_;

    uint8_t *globalData() const {
        JS_ASSERT(code_);
        return code_ + codeBytes_;
    }

  public:
    AsmJSModule()
      : numGlobalVars_(0),
        numFFIs_(0),
        numFuncPtrTableElems_(0),
        hasArrayView_(false),
        code_(NULL),
        operationCallbackExit_(NULL),
        functionBytes_(0),
        codeBytes_(0),
        totalBytes_(0),
        linked_(false)
    {}

    void trace(JSTracer *trc) {
        for (unsigned i = 0; i < globals_.length(); i++)
            globals_[i].trace(trc);
        for (unsigned i = 0; i < exports_.length(); i++)
            exports_[i].trace(trc);
        for (unsigned i = 0; i < exits_.length(); i++) {
            if (exitIndexToGlobalDatum(i).fun)
                MarkObject(trc, &exitIndexToGlobalDatum(i).fun, "asm.js imported function");
        }
        if (maybeHeap_)
            MarkObject(trc, &maybeHeap_, "asm.js heap");
    }

    bool addGlobalVarInitConstant(const Value &v, uint32_t *globalIndex) {
        if (numGlobalVars_ == UINT32_MAX)
            return false;
        Global g(Global::Variable);
        g.u.var.initKind_ = Global::InitConstant;
        g.u.var.init.constant_ = v;
        g.u.var.index_ = *globalIndex = numGlobalVars_++;
        return globals_.append(g);
    }
    bool addGlobalVarImport(PropertyName *fieldName, AsmJSCoercion coercion, uint32_t *globalIndex) {
        Global g(Global::Variable);
        g.u.var.initKind_ = Global::InitImport;
        g.u.var.init.coercion_ = coercion;
        g.u.var.index_ = *globalIndex = numGlobalVars_++;
        g.name_ = fieldName;
        return globals_.append(g);
    }
    bool incrementNumFuncPtrTableElems(uint32_t numElems) {
        if (UINT32_MAX - numFuncPtrTableElems_ < numElems)
            return false;
        numFuncPtrTableElems_ += numElems;
        return true;
    }
    bool addFFI(PropertyName *field, uint32_t *ffiIndex) {
        if (numFFIs_ == UINT32_MAX)
            return false;
        Global g(Global::FFI);
        g.u.ffiIndex_ = *ffiIndex = numFFIs_++;
        g.name_ = field;
        return globals_.append(g);
    }
    bool addArrayView(ArrayBufferView::ViewType vt, PropertyName *field) {
        hasArrayView_ = true;
        Global g(Global::ArrayView);
        g.u.viewType_ = vt;
        g.name_ = field;
        return globals_.append(g);
    }
    bool addMathBuiltin(AsmJSMathBuiltin mathBuiltin, PropertyName *field) {
        Global g(Global::MathBuiltin);
        g.u.mathBuiltin_ = mathBuiltin;
        g.name_ = field;
        return globals_.append(g);
    }
    bool addGlobalConstant(double value, PropertyName *fieldName) {
        Global g(Global::Constant);
        g.u.constantValue_ = value;
        g.name_ = fieldName;
        return globals_.append(g);
    }
    bool addExit(unsigned ffiIndex, unsigned *exitIndex) {
        *exitIndex = unsigned(exits_.length());
        return exits_.append(Exit(ffiIndex));
    }

    bool addExportedFunction(RawFunction fun, PropertyName *maybeFieldName,
                             MoveRef<ArgCoercionVector> argCoercions, ReturnType returnType)
    {
        ExportedFunction func(fun, maybeFieldName, argCoercions, returnType);
        return exports_.append(Move(func));
    }
    unsigned numExportedFunctions() const {
        return exports_.length();
    }
    const ExportedFunction &exportedFunction(unsigned i) const {
        return exports_[i];
    }
    ExportedFunction &exportedFunction(unsigned i) {
        return exports_[i];
    }
    bool hasArrayView() const {
        return hasArrayView_;
    }
    unsigned numFFIs() const {
        return numFFIs_;
    }
    unsigned numGlobalVars() const {
        return numGlobalVars_;
    }
    unsigned numGlobals() const {
        return globals_.length();
    }
    Global global(unsigned i) const {
        return globals_[i];
    }
    unsigned numFuncPtrTableElems() const {
        return numFuncPtrTableElems_;
    }
    unsigned numExits() const {
        return exits_.length();
    }
    Exit &exit(unsigned i) {
        return exits_[i];
    }
    const Exit &exit(unsigned i) const {
        return exits_[i];
    }

    // An Exit holds bookkeeping information about an exit; the ExitDatum
    // struct overlays the actual runtime data stored in the global data
    // section.
    struct ExitDatum
    {
        uint8_t *exit;
        HeapPtrFunction fun;
    };

    // Global data section
    //
    // The global data section is placed after the executable code (i.e., at
    // offset codeBytes_) in the module's linear allocation. The global data
    // are laid out in this order:
    //   0. a pointer/descriptor for the heap that was linked to the module
    //   1. global variable state (elements are sizeof(uint64_t))
    //   2. function-pointer table elements (elements are sizeof(void*))
    //   3. exits (elements are sizeof(ExitDatum))
    //
    // NB: The list of exits is extended while emitting function bodies and
    // thus exits must be at the end of the list to avoid invalidating indices.
    size_t globalDataBytes() const {
        return sizeof(void*) +
               numGlobalVars_ * sizeof(uint64_t) +
               numFuncPtrTableElems_ * sizeof(void*) +
               exits_.length() * sizeof(ExitDatum);
    }
    unsigned heapOffset() const {
        return 0;
    }
    uint8_t *&heapDatum() const {
        return *(uint8_t**)(globalData() + heapOffset());
    }
    unsigned globalVarIndexToGlobalDataOffset(unsigned i) const {
        JS_ASSERT(i < numGlobalVars_);
        return sizeof(void*) +
               i * sizeof(uint64_t);
    }
    void *globalVarIndexToGlobalDatum(unsigned i) const {
        return (void *)(globalData() + globalVarIndexToGlobalDataOffset(i));
    }
    unsigned funcPtrIndexToGlobalDataOffset(unsigned i) const {
        return sizeof(void*) +
               numGlobalVars_ * sizeof(uint64_t) +
               i * sizeof(void*);
    }
    void *&funcPtrIndexToGlobalDatum(unsigned i) const {
        return *(void **)(globalData() + funcPtrIndexToGlobalDataOffset(i));
    }
    unsigned exitIndexToGlobalDataOffset(unsigned exitIndex) const {
        JS_ASSERT(exitIndex < exits_.length());
        return sizeof(void*) +
               numGlobalVars_ * sizeof(uint64_t) +
               numFuncPtrTableElems_ * sizeof(void*) +
               exitIndex * sizeof(ExitDatum);
    }
    ExitDatum &exitIndexToGlobalDatum(unsigned exitIndex) const {
        return *(ExitDatum *)(globalData() + exitIndexToGlobalDataOffset(exitIndex));
    }

    void setFunctionBytes(size_t functionBytes) {
        JS_ASSERT(functionBytes % gc::PageSize == 0);
        functionBytes_ = functionBytes;
    }
    size_t functionBytes() const {
        JS_ASSERT(functionBytes_);
        JS_ASSERT(functionBytes_ % gc::PageSize == 0);
        return functionBytes_;
    }
    bool containsPC(void *pc) const {
        uint8_t *code = functionCode();
        return pc >= code && pc < (code + functionBytes());
    }

    bool addHeapAccesses(const ion::AsmJSHeapAccessVector &accesses) {
        if (!heapAccesses_.reserve(heapAccesses_.length() + accesses.length()))
            return false;
        for (size_t i = 0; i < accesses.length(); i++)
            heapAccesses_.infallibleAppend(accesses[i]);
        return true;
    }
    unsigned numHeapAccesses() const {
        return heapAccesses_.length();
    }
    ion::AsmJSHeapAccess &heapAccess(unsigned i) {
        return heapAccesses_[i];
    }
    const ion::AsmJSHeapAccess &heapAccess(unsigned i) const {
        return heapAccesses_[i];
    }

    void takeOwnership(JSC::ExecutablePool *pool, uint8_t *code, size_t codeBytes, size_t totalBytes) {
        JS_ASSERT(uintptr_t(code) % gc::PageSize == 0);
        codePool_ = pool;
        code_ = code;
        codeBytes_ = codeBytes;
        totalBytes_ = totalBytes;
    }
    uint8_t *functionCode() const {
        JS_ASSERT(code_);
        JS_ASSERT(uintptr_t(code_) % gc::PageSize == 0);
        return code_;
    }

    void setOperationCallbackExit(uint8_t *ptr) {
        operationCallbackExit_ = ptr;
    }
    uint8_t *operationCallbackExit() const {
        return operationCallbackExit_;
    }

    void setIsLinked(Handle<ArrayBufferObject*> maybeHeap) {
        JS_ASSERT(!linked_);
        linked_ = true;
        maybeHeap_ = maybeHeap;
        heapDatum() = maybeHeap_ ? maybeHeap_->dataPointer() : NULL;
    }
    bool isLinked() const {
        return linked_;
    }
    uint8_t *maybeHeap() const {
        JS_ASSERT(linked_);
        return heapDatum();
    }
    size_t heapLength() const {
        JS_ASSERT(linked_);
        return maybeHeap_ ? maybeHeap_->byteLength() : 0;
    }
};

// The AsmJSModule C++ object is held by a JSObject that takes care of calling
// 'trace' and the destructor on finalization.
extern AsmJSModule &
AsmJSModuleObjectToModule(JSObject *obj);

}  // namespace js

#endif  // jsion_asmjsmodule_h__


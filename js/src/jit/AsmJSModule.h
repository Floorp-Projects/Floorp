/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AsmJSModule_h
#define jit_AsmJSModule_h

#ifdef JS_ION

#include "mozilla/Move.h"
#include "mozilla/PodOperations.h"

#include "jsscript.h"

#include "gc/Marking.h"
#include "jit/AsmJS.h"
#include "jit/IonMacroAssembler.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/RegisterSets.h"
#include "vm/TypedArrayObject.h"

namespace js {

namespace frontend { class TokenStream; }

// These EcmaScript-defined coercions form the basis of the asm.js type system.
enum AsmJSCoercion
{
    AsmJS_ToInt32,
    AsmJS_ToNumber,
    AsmJS_FRound
};

// The asm.js spec recognizes this set of builtin Math functions.
enum AsmJSMathBuiltinFunction
{
    AsmJSMathBuiltin_sin, AsmJSMathBuiltin_cos, AsmJSMathBuiltin_tan,
    AsmJSMathBuiltin_asin, AsmJSMathBuiltin_acos, AsmJSMathBuiltin_atan,
    AsmJSMathBuiltin_ceil, AsmJSMathBuiltin_floor, AsmJSMathBuiltin_exp,
    AsmJSMathBuiltin_log, AsmJSMathBuiltin_pow, AsmJSMathBuiltin_sqrt,
    AsmJSMathBuiltin_abs, AsmJSMathBuiltin_atan2, AsmJSMathBuiltin_imul,
    AsmJSMathBuiltin_fround, AsmJSMathBuiltin_min, AsmJSMathBuiltin_max
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
        enum Which { Variable, FFI, ArrayView, MathBuiltinFunction, Constant };
        enum VarInitKind { InitConstant, InitImport };
        enum ConstantKind { GlobalConstant, MathConstant };

      private:
        struct Pod {
            Which which_;
            union {
                struct {
                    uint32_t index_;
                    VarInitKind initKind_;
                    AsmJSCoercion coercion_;
                    union {
                        Value constant_; // will only contain int32/double
                    } init;
                } var;
                uint32_t ffiIndex_;
                ArrayBufferView::ViewType viewType_;
                AsmJSMathBuiltinFunction mathBuiltinFunc_;
                struct {
                    ConstantKind kind_;
                    double value_;
                } constant;
            } u;
        } pod;
        PropertyName *name_;

        friend class AsmJSModule;

        Global(Which which, PropertyName *name) {
            pod.which_ = which;
            name_ = name;
            JS_ASSERT_IF(name_, name_->isTenured());
        }

        void trace(JSTracer *trc) {
            if (name_)
                MarkStringUnbarriered(trc, &name_, "asm.js global name");
            JS_ASSERT_IF(pod.which_ == Variable && pod.u.var.initKind_ == InitConstant,
                         !pod.u.var.init.constant_.isMarkable());
        }

      public:
        Global() {}
        Which which() const {
            return pod.which_;
        }
        uint32_t varIndex() const {
            JS_ASSERT(pod.which_ == Variable);
            return pod.u.var.index_;
        }
        VarInitKind varInitKind() const {
            JS_ASSERT(pod.which_ == Variable);
            return pod.u.var.initKind_;
        }
        const Value &varInitConstant() const {
            JS_ASSERT(pod.which_ == Variable);
            JS_ASSERT(pod.u.var.initKind_ == InitConstant);
            return pod.u.var.init.constant_;
        }
        AsmJSCoercion varInitCoercion() const {
            JS_ASSERT(pod.which_ == Variable);
            return pod.u.var.coercion_;
        }
        PropertyName *varImportField() const {
            JS_ASSERT(pod.which_ == Variable);
            JS_ASSERT(pod.u.var.initKind_ == InitImport);
            return name_;
        }
        PropertyName *ffiField() const {
            JS_ASSERT(pod.which_ == FFI);
            return name_;
        }
        uint32_t ffiIndex() const {
            JS_ASSERT(pod.which_ == FFI);
            return pod.u.ffiIndex_;
        }
        PropertyName *viewName() const {
            JS_ASSERT(pod.which_ == ArrayView);
            return name_;
        }
        ArrayBufferView::ViewType viewType() const {
            JS_ASSERT(pod.which_ == ArrayView);
            return pod.u.viewType_;
        }
        PropertyName *mathName() const {
            JS_ASSERT(pod.which_ == MathBuiltinFunction);
            return name_;
        }
        AsmJSMathBuiltinFunction mathBuiltinFunction() const {
            JS_ASSERT(pod.which_ == MathBuiltinFunction);
            return pod.u.mathBuiltinFunc_;
        }
        PropertyName *constantName() const {
            JS_ASSERT(pod.which_ == Constant);
            return name_;
        }
        ConstantKind constantKind() const {
            JS_ASSERT(pod.which_ == Constant);
            return pod.u.constant.kind_;
        }
        double constantValue() const {
            JS_ASSERT(pod.which_ == Constant);
            return pod.u.constant.value_;
        }

        size_t serializedSize() const;
        uint8_t *serialize(uint8_t *cursor) const;
        const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor);
        bool clone(ExclusiveContext *cx, Global *out) const;
    };

    class Exit
    {
        unsigned ffiIndex_;
        unsigned globalDataOffset_;
        unsigned interpCodeOffset_;
        unsigned ionCodeOffset_;

        friend class AsmJSModule;

      public:
        Exit() {}
        Exit(unsigned ffiIndex, unsigned globalDataOffset)
          : ffiIndex_(ffiIndex), globalDataOffset_(globalDataOffset),
            interpCodeOffset_(0), ionCodeOffset_(0)
        {}
        unsigned ffiIndex() const {
            return ffiIndex_;
        }
        unsigned globalDataOffset() const {
            return globalDataOffset_;
        }
        void initInterpOffset(unsigned off) {
            JS_ASSERT(!interpCodeOffset_);
            interpCodeOffset_ = off;
        }
        void initIonOffset(unsigned off) {
            JS_ASSERT(!ionCodeOffset_);
            ionCodeOffset_ = off;
        }
        void updateOffsets(jit::MacroAssembler &masm) {
            interpCodeOffset_ = masm.actualOffset(interpCodeOffset_);
            ionCodeOffset_ = masm.actualOffset(ionCodeOffset_);
        }

        size_t serializedSize() const;
        uint8_t *serialize(uint8_t *cursor) const;
        const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor);
        bool clone(ExclusiveContext *cx, Exit *out) const;
    };
    typedef int32_t (*CodePtr)(uint64_t *args, uint8_t *global);

    // An Exit holds bookkeeping information about an exit; the ExitDatum
    // struct overlays the actual runtime data stored in the global data
    // section.
    struct ExitDatum
    {
        uint8_t *exit;
        HeapPtrFunction fun;
    };

    typedef Vector<AsmJSCoercion, 0, SystemAllocPolicy> ArgCoercionVector;

    enum ReturnType { Return_Int32, Return_Double, Return_Void };

    class ExportedFunction
    {
        PropertyName *name_;
        PropertyName *maybeFieldName_;
        ArgCoercionVector argCoercions_;
        struct Pod {
            ReturnType returnType_;
            uint32_t codeOffset_;
            // These two fields are offsets to the beginning of the ScriptSource
            // of the module, and thus invariant under serialization (unlike
            // absolute offsets into ScriptSource).
            uint32_t startOffsetInModule_;
            uint32_t endOffsetInModule_;
        } pod;

        friend class AsmJSModule;

        ExportedFunction(PropertyName *name,
                         uint32_t startOffsetInModule, uint32_t endOffsetInModule,
                         PropertyName *maybeFieldName,
                         ArgCoercionVector &&argCoercions,
                         ReturnType returnType)
        {
            name_ = name;
            maybeFieldName_ = maybeFieldName;
            argCoercions_ = mozilla::Move(argCoercions);
            pod.returnType_ = returnType;
            pod.codeOffset_ = UINT32_MAX;
            pod.startOffsetInModule_ = startOffsetInModule;
            pod.endOffsetInModule_ = endOffsetInModule;
            JS_ASSERT_IF(maybeFieldName_, name_->isTenured());
        }

        void trace(JSTracer *trc) {
            MarkStringUnbarriered(trc, &name_, "asm.js export name");
            if (maybeFieldName_)
                MarkStringUnbarriered(trc, &maybeFieldName_, "asm.js export field");
        }

      public:
        ExportedFunction() {}
        ExportedFunction(ExportedFunction &&rhs) {
            name_ = rhs.name_;
            maybeFieldName_ = rhs.maybeFieldName_;
            argCoercions_ = mozilla::Move(rhs.argCoercions_);
            pod = rhs.pod;
        }

        void initCodeOffset(unsigned off) {
            JS_ASSERT(pod.codeOffset_ == UINT32_MAX);
            pod.codeOffset_ = off;
        }
        void updateCodeOffset(jit::MacroAssembler &masm) {
            pod.codeOffset_ = masm.actualOffset(pod.codeOffset_);
        }


        PropertyName *name() const {
            return name_;
        }
        uint32_t startOffsetInModule() const {
            return pod.startOffsetInModule_;
        }
        uint32_t endOffsetInModule() const {
            return pod.endOffsetInModule_;
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
            return pod.returnType_;
        }

        size_t serializedSize() const;
        uint8_t *serialize(uint8_t *cursor) const;
        const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor);
        bool clone(ExclusiveContext *cx, ExportedFunction *out) const;
    };

    class CodeRange
    {
      public:
        enum Kind { Entry, Function };

      private:
        Kind kind_;
        uint32_t begin_;
        uint32_t end_;
        uint32_t functionNameIndex_;

        friend class AsmJSModule;
        CodeRange(Kind k, uint32_t begin, uint32_t end, uint32_t functionNameIndex)
          : kind_(k), begin_(begin), end_(end), functionNameIndex_(functionNameIndex)
        {}

      public:
        CodeRange() {}
        Kind kind() const { return kind_; }
        uint32_t begin() const { return begin_; }
        uint32_t end() const { return end_; }
        PropertyName *functionName(const AsmJSModule &module) const {
            JS_ASSERT(kind_ == Function);
            return module.functionNames_[functionNameIndex_].name();
        }
    };

    class Name
    {
        PropertyName *name_;
      public:
        Name() : name_(nullptr) {}
        MOZ_IMPLICIT Name(PropertyName *name) : name_(name) {}
        PropertyName *name() const { return name_; }
        PropertyName *&name() { return name_; }
        size_t serializedSize() const;
        uint8_t *serialize(uint8_t *cursor) const;
        const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor);
        bool clone(ExclusiveContext *cx, Name *out) const;
    };

#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
    // Function information to add to the VTune JIT profiler following linking.
    struct ProfiledFunction
    {
        PropertyName *name;
        struct Pod {
            unsigned startCodeOffset;
            unsigned endCodeOffset;
            unsigned lineno;
            unsigned columnIndex;
        } pod;

        explicit ProfiledFunction()
          : name(nullptr)
        { }

        ProfiledFunction(PropertyName *name, unsigned start, unsigned end,
                         unsigned line = 0, unsigned column = 0)
          : name(name)
        {
            JS_ASSERT(name->isTenured());

            pod.startCodeOffset = start;
            pod.endCodeOffset = end;
            pod.lineno = line;
            pod.columnIndex = column;
        }

        void trace(JSTracer *trc) {
            if (name)
                MarkStringUnbarriered(trc, &name, "asm.js profiled function name");
        }

        size_t serializedSize() const;
        uint8_t *serialize(uint8_t *cursor) const;
        const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor);
    };
#endif

#if defined(JS_ION_PERF)
    struct ProfiledBlocksFunction : public ProfiledFunction
    {
        unsigned endInlineCodeOffset;
        jit::BasicBlocksVector blocks;

        ProfiledBlocksFunction(PropertyName *name, unsigned start, unsigned endInline, unsigned end,
                               jit::BasicBlocksVector &blocksVector)
          : ProfiledFunction(name, start, end), endInlineCodeOffset(endInline),
            blocks(mozilla::Move(blocksVector))
        {
            JS_ASSERT(name->isTenured());
        }

        ProfiledBlocksFunction(ProfiledBlocksFunction &&copy)
          : ProfiledFunction(copy.name, copy.pod.startCodeOffset, copy.pod.endCodeOffset),
            endInlineCodeOffset(copy.endInlineCodeOffset), blocks(mozilla::Move(copy.blocks))
        { }
    };
#endif

    struct RelativeLink
    {
        enum Kind
        {
            RawPointer,
            CodeLabel,
            InstructionImmediate
        };

        RelativeLink()
        { }

        RelativeLink(Kind kind)
        {
#if defined(JS_CODEGEN_MIPS)
            kind_ = kind;
#elif defined(JS_CODEGEN_ARM)
            // On ARM, CodeLabels are only used to label raw pointers, so in
            // all cases on ARM, a RelativePatch means patching a raw pointer.
            JS_ASSERT(kind == CodeLabel || kind == RawPointer);
#endif
            // On X64 and X86, all RelativePatch-es are patched as raw pointers.
        }

        bool isRawPointerPatch() {
#if defined(JS_CODEGEN_MIPS)
            return kind_ == RawPointer;
#else
            return true;
#endif
        }

#ifdef JS_CODEGEN_MIPS
        Kind kind_;
#endif
        uint32_t patchAtOffset;
        uint32_t targetOffset;
    };

    typedef Vector<RelativeLink, 0, SystemAllocPolicy> RelativeLinkVector;

    struct AbsoluteLink
    {
        jit::CodeOffsetLabel patchAt;
        jit::AsmJSImmKind target;
    };

    typedef Vector<AbsoluteLink, 0, SystemAllocPolicy> AbsoluteLinkVector;

    // Static-link data is used to patch a module either after it has been
    // compiled or deserialized with various absolute addresses (of code or
    // data in the process) or relative addresses (of code or data in the same
    // AsmJSModule).
    struct StaticLinkData
    {
        uint32_t interruptExitOffset;
        RelativeLinkVector relativeLinks;
        AbsoluteLinkVector absoluteLinks;

        size_t serializedSize() const;
        uint8_t *serialize(uint8_t *cursor) const;
        const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor);
        bool clone(ExclusiveContext *cx, StaticLinkData *out) const;

        size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
    };

  private:
    struct Pod {
        size_t                            funcPtrTableAndExitBytes_;
        size_t                            functionBytes_; // just the function bodies, no stubs
        size_t                            codeBytes_;     // function bodies and stubs
        size_t                            totalBytes_;    // function bodies, stubs, and global data
        uint32_t                          minHeapLength_;
        uint32_t                          numGlobalVars_;
        uint32_t                          numFFIs_;
        uint32_t                          srcLength_;
        uint32_t                          srcLengthWithRightBrace_;
        bool                              strict_;
        bool                              hasArrayView_;
    } pod;

    // These two fields need to be kept out pod as they depend on the position
    // of the module within the ScriptSource and thus aren't invariant with
    // respect to caching.
    const uint32_t                        srcStart_;
    const uint32_t                        srcBodyStart_;

    Vector<Global,                 0, SystemAllocPolicy> globals_;
    Vector<Exit,                   0, SystemAllocPolicy> exits_;
    Vector<ExportedFunction,       0, SystemAllocPolicy> exports_;
    Vector<jit::CallSite,          0, SystemAllocPolicy> callSites_;
    Vector<CodeRange,              0, SystemAllocPolicy> codeRanges_;
    Vector<Name,                   0, SystemAllocPolicy> functionNames_;
    Vector<jit::AsmJSHeapAccess,   0, SystemAllocPolicy> heapAccesses_;
    Vector<jit::IonScriptCounts*,  0, SystemAllocPolicy> functionCounts_;
#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
    Vector<ProfiledFunction,       0, SystemAllocPolicy> profiledFunctions_;
#endif
#if defined(JS_ION_PERF)
    Vector<ProfiledBlocksFunction, 0, SystemAllocPolicy> perfProfiledBlocksFunctions_;
#endif

    ScriptSource *                        scriptSource_;
    PropertyName *                        globalArgumentName_;
    PropertyName *                        importArgumentName_;
    PropertyName *                        bufferArgumentName_;
    uint8_t *                             code_;
    uint8_t *                             interruptExit_;
    StaticLinkData                        staticLinkData_;
    HeapPtrArrayBufferObject              maybeHeap_;
    bool                                  dynamicallyLinked_;
    bool                                  loadedFromCache_;

    // This field is accessed concurrently when requesting an interrupt.
    // Access must be synchronized via the runtime's interrupt lock.
    mutable bool                          codeIsProtected_;

  public:
    explicit AsmJSModule(ScriptSource *scriptSource, uint32_t srcStart, uint32_t srcBodyStart,
                         bool strict);
    void trace(JSTracer *trc);
    ~AsmJSModule();

    // An AsmJSModule transitions monotonically through these states:
    bool isFinishedWithModulePrologue() const { return pod.funcPtrTableAndExitBytes_ != SIZE_MAX; }
    bool isFinishedWithFunctionBodies() const { return pod.functionBytes_ != UINT32_MAX; }
    bool isFinished() const { return !!code_; }
    bool isStaticallyLinked() const { return !!interruptExit_; }
    bool isDynamicallyLinked() const { return dynamicallyLinked_; }

    /*************************************************************************/
    // These functions may be used as soon as the module is constructed:

    ScriptSource *scriptSource() const {
        JS_ASSERT(scriptSource_);
        return scriptSource_;
    }
    bool strict() const {
        return pod.strict_;
    }
    bool loadedFromCache() const {
        return loadedFromCache_;
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
    unsigned numFunctionCounts() const {
        return functionCounts_.length();
    }
    jit::IonScriptCounts *functionCounts(unsigned i) {
        return functionCounts_[i];
    }

    // about:memory reporting
    void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t *asmJSModuleCode,
                       size_t *asmJSModuleData);

    /*************************************************************************/
    // These functions build the global scope of the module while parsing the
    // module prologue (before the function bodies):

    void initGlobalArgumentName(PropertyName *n) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        JS_ASSERT_IF(n, n->isTenured());
        globalArgumentName_ = n;
    }
    void initImportArgumentName(PropertyName *n) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        JS_ASSERT_IF(n, n->isTenured());
        importArgumentName_ = n;
    }
    void initBufferArgumentName(PropertyName *n) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        JS_ASSERT_IF(n, n->isTenured());
        bufferArgumentName_ = n;
    }
    PropertyName *globalArgumentName() const {
        return globalArgumentName_;
    }
    PropertyName *importArgumentName() const {
        return importArgumentName_;
    }
    PropertyName *bufferArgumentName() const {
        return bufferArgumentName_;
    }
    bool addGlobalVarInit(const Value &v, AsmJSCoercion coercion, uint32_t *globalIndex) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        if (pod.numGlobalVars_ == UINT32_MAX)
            return false;
        Global g(Global::Variable, nullptr);
        g.pod.u.var.initKind_ = Global::InitConstant;
        g.pod.u.var.init.constant_ = v;
        g.pod.u.var.coercion_ = coercion;
        g.pod.u.var.index_ = *globalIndex = pod.numGlobalVars_++;
        return globals_.append(g);
    }
    bool addGlobalVarImport(PropertyName *name, AsmJSCoercion coercion, uint32_t *globalIndex) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        Global g(Global::Variable, name);
        g.pod.u.var.initKind_ = Global::InitImport;
        g.pod.u.var.coercion_ = coercion;
        g.pod.u.var.index_ = *globalIndex = pod.numGlobalVars_++;
        return globals_.append(g);
    }
    bool addFFI(PropertyName *field, uint32_t *ffiIndex) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        if (pod.numFFIs_ == UINT32_MAX)
            return false;
        Global g(Global::FFI, field);
        g.pod.u.ffiIndex_ = *ffiIndex = pod.numFFIs_++;
        return globals_.append(g);
    }
    bool addArrayView(ArrayBufferView::ViewType vt, PropertyName *field) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        pod.hasArrayView_ = true;
        Global g(Global::ArrayView, field);
        g.pod.u.viewType_ = vt;
        return globals_.append(g);
    }
    bool addMathBuiltinFunction(AsmJSMathBuiltinFunction func, PropertyName *field) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        Global g(Global::MathBuiltinFunction, field);
        g.pod.u.mathBuiltinFunc_ = func;
        return globals_.append(g);
    }
    bool addMathBuiltinConstant(double value, PropertyName *field) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        Global g(Global::Constant, field);
        g.pod.u.constant.value_ = value;
        g.pod.u.constant.kind_ = Global::MathConstant;
        return globals_.append(g);
    }
    bool addGlobalConstant(double value, PropertyName *name) {
        JS_ASSERT(!isFinishedWithModulePrologue());
        Global g(Global::Constant, name);
        g.pod.u.constant.value_ = value;
        g.pod.u.constant.kind_ = Global::GlobalConstant;
        return globals_.append(g);
    }
    unsigned numGlobals() const {
        return globals_.length();
    }
    Global &global(unsigned i) {
        return globals_[i];
    }

    /*************************************************************************/

    void startFunctionBodies() {
        JS_ASSERT(!isFinishedWithModulePrologue());
        pod.funcPtrTableAndExitBytes_ = 0;
        JS_ASSERT(isFinishedWithModulePrologue());
    }

    /*************************************************************************/
    // These functions are called while parsing/compiling function bodies:

    void requireHeapLengthToBeAtLeast(uint32_t len) {
        JS_ASSERT(isFinishedWithModulePrologue() && !isFinishedWithFunctionBodies());
        if (len > pod.minHeapLength_)
            pod.minHeapLength_ = len;
    }
    bool addFunctionCodeRange(PropertyName *name, uint32_t begin, uint32_t end) {
        JS_ASSERT(isFinishedWithModulePrologue() && !isFinishedWithFunctionBodies());
        JS_ASSERT(name->isTenured());
        if (functionNames_.length() >= UINT32_MAX)
            return false;
        CodeRange codeRange(CodeRange::Function, begin, end, functionNames_.length());
        return functionNames_.append(name) && codeRanges_.append(codeRange);
    }
    bool addEntryCodeRange(unsigned exportIndex, uint32_t end) {
        uint32_t begin = exports_[exportIndex].pod.codeOffset_;
        CodeRange codeRange(CodeRange::Entry, begin, end, UINT32_MAX);
        return codeRanges_.append(codeRange);
    }
    bool addExit(unsigned ffiIndex, unsigned *exitIndex) {
        JS_ASSERT(isFinishedWithModulePrologue() && !isFinishedWithFunctionBodies());
        if (SIZE_MAX - pod.funcPtrTableAndExitBytes_ < sizeof(ExitDatum))
            return false;
        uint32_t globalDataOffset = globalDataBytes();
        JS_STATIC_ASSERT(sizeof(ExitDatum) % sizeof(void*) == 0);
        pod.funcPtrTableAndExitBytes_ += sizeof(ExitDatum);
        *exitIndex = unsigned(exits_.length());
        return exits_.append(Exit(ffiIndex, globalDataOffset));
    }
    unsigned numExits() const {
        JS_ASSERT(isFinishedWithModulePrologue());
        return exits_.length();
    }
    Exit &exit(unsigned i) {
        JS_ASSERT(isFinishedWithModulePrologue());
        return exits_[i];
    }
    const Exit &exit(unsigned i) const {
        JS_ASSERT(isFinishedWithModulePrologue());
        return exits_[i];
    }
    bool addFuncPtrTable(unsigned numElems, uint32_t *globalDataOffset) {
        JS_ASSERT(isFinishedWithModulePrologue() && !isFinished());
        JS_ASSERT(IsPowerOfTwo(numElems));
        if (SIZE_MAX - pod.funcPtrTableAndExitBytes_ < numElems * sizeof(void*))
            return false;
        *globalDataOffset = globalDataBytes();
        pod.funcPtrTableAndExitBytes_ += numElems * sizeof(void*);
        return true;
    }
    bool addFunctionCounts(jit::IonScriptCounts *counts) {
        JS_ASSERT(isFinishedWithModulePrologue() && !isFinishedWithFunctionBodies());
        return functionCounts_.append(counts);
    }
#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
    bool addProfiledFunction(PropertyName *name, unsigned codeStart, unsigned codeEnd,
                             unsigned line, unsigned column)
    {
        JS_ASSERT(isFinishedWithModulePrologue() && !isFinishedWithFunctionBodies());
        ProfiledFunction func(name, codeStart, codeEnd, line, column);
        return profiledFunctions_.append(func);
    }
    unsigned numProfiledFunctions() const {
        JS_ASSERT(isFinishedWithModulePrologue());
        return profiledFunctions_.length();
    }
    ProfiledFunction &profiledFunction(unsigned i) {
        JS_ASSERT(isFinishedWithModulePrologue());
        return profiledFunctions_[i];
    }
#endif
#ifdef JS_ION_PERF
    bool addProfiledBlocks(PropertyName *name, unsigned codeBegin, unsigned inlineEnd,
                           unsigned codeEnd, jit::BasicBlocksVector &basicBlocks)
    {
        JS_ASSERT(isFinishedWithModulePrologue() && !isFinishedWithFunctionBodies());
        ProfiledBlocksFunction func(name, codeBegin, inlineEnd, codeEnd, basicBlocks);
        return perfProfiledBlocksFunctions_.append(mozilla::Move(func));
    }
    unsigned numPerfBlocksFunctions() const {
        JS_ASSERT(isFinishedWithModulePrologue());
        return perfProfiledBlocksFunctions_.length();
    }
    ProfiledBlocksFunction &perfProfiledBlocksFunction(unsigned i) {
        JS_ASSERT(isFinishedWithModulePrologue());
        return perfProfiledBlocksFunctions_[i];
    }
#endif

    /*************************************************************************/

    // This function is called after compiling the function bodies (before
    // compiling entries/exits) to record the extent of compiled function code.
    void finishFunctionBodies(size_t functionBytes) {
        JS_ASSERT(isFinishedWithModulePrologue() && !isFinishedWithFunctionBodies());
        pod.functionBytes_ = functionBytes;
        JS_ASSERT(isFinishedWithFunctionBodies());
    }

    /*************************************************************************/
    // Exported functions are added after finishFunctionBodies() and before
    // finish(). The list of exported functions can be accessed any time after
    // the exported functions have been added.

    bool addExportedFunction(PropertyName *name,
                             uint32_t srcStart,
                             uint32_t srcEnd,
                             PropertyName *maybeFieldName,
                             ArgCoercionVector &&argCoercions,
                             ReturnType returnType)
    {
        JS_ASSERT(isFinishedWithFunctionBodies() && !isFinished());
        ExportedFunction func(name, srcStart, srcEnd, maybeFieldName,
                              mozilla::Move(argCoercions), returnType);
        if (exports_.length() >= UINT32_MAX)
            return false;
        return exports_.append(mozilla::Move(func));
    }
    unsigned numExportedFunctions() const {
        JS_ASSERT(isFinishedWithFunctionBodies());
        return exports_.length();
    }
    const ExportedFunction &exportedFunction(unsigned i) const {
        JS_ASSERT(isFinishedWithFunctionBodies());
        return exports_[i];
    }
    ExportedFunction &exportedFunction(unsigned i) {
        JS_ASSERT(isFinishedWithFunctionBodies());
        return exports_[i];
    }

    /*************************************************************************/

    // finish() is called once the entire module has been parsed (via
    // tokenStream) and all function and entry/exit trampolines have been
    // generated (via masm). After this function, the module must still be
    // statically and dynamically linked before code can be run.
    bool finish(ExclusiveContext *cx,
                frontend::TokenStream &tokenStream,
                jit::MacroAssembler &masm,
                const jit::Label &interruptLabel);

    /*************************************************************************/
    // These accessor functions can be used after finish():

    bool hasArrayView() const {
        JS_ASSERT(isFinished());
        return pod.hasArrayView_;
    }
    unsigned numFFIs() const {
        JS_ASSERT(isFinished());
        return pod.numFFIs_;
    }
    uint32_t srcEndBeforeCurly() const {
        JS_ASSERT(isFinished());
        return srcStart_ + pod.srcLength_;
    }
    uint32_t srcEndAfterCurly() const {
        JS_ASSERT(isFinished());
        return srcStart_ + pod.srcLengthWithRightBrace_;
    }
    uint8_t *codeBase() const {
        JS_ASSERT(isFinished());
        JS_ASSERT(uintptr_t(code_) % AsmJSPageSize == 0);
        return code_;
    }
    size_t functionBytes() const {
        JS_ASSERT(isFinished());
        return pod.functionBytes_;
    }
    size_t codeBytes() const {
        JS_ASSERT(isFinished());
        return pod.codeBytes_;
    }
    bool containsPC(void *pc) const {
        JS_ASSERT(isFinished());
        return pc >= code_ && pc < (code_ + functionBytes());
    }
    uint8_t *interpExitTrampoline(const Exit &exit) const {
        JS_ASSERT(isFinished());
        JS_ASSERT(exit.interpCodeOffset_);
        return code_ + exit.interpCodeOffset_;
    }
    uint8_t *ionExitTrampoline(const Exit &exit) const {
        JS_ASSERT(isFinished());
        JS_ASSERT(exit.ionCodeOffset_);
        return code_ + exit.ionCodeOffset_;
    }

    // Lookup a callsite by the return pc (from the callee to the caller).
    // Return null if no callsite was found.
    const jit::CallSite *lookupCallSite(void *returnAddress) const;

    // Lookup the name the code range containing the given pc. Return null if no
    // code range was found.
    const CodeRange *lookupCodeRange(void *pc) const;

    // Lookup a heap access site by the pc which performs the access. Return
    // null if no heap access was found.
    const jit::AsmJSHeapAccess *lookupHeapAccess(void *pc) const;

    // The global data section is placed after the executable code (i.e., at
    // offset codeBytes_) in the module's linear allocation. The global data
    // are laid out in this order:
    //   0. a pointer (padded up to 8 bytes to ensure double-alignment of
    //      globals) for the heap that was linked to the module.
    //   1. global variable state (elements are sizeof(uint64_t))
    //   2. interleaved function-pointer tables and exits. These are allocated
    //      while type checking function bodies (as exits and uses of
    //      function-pointer tables are encountered).
    size_t offsetOfGlobalData() const {
        JS_ASSERT(isFinished());
        return pod.codeBytes_;
    }
    uint8_t *globalData() const {
        JS_ASSERT(isFinished());
        return code_ + offsetOfGlobalData();
    }
    size_t globalDataBytes() const {
        return sizeof(uint64_t) +
               pod.numGlobalVars_ * sizeof(uint64_t) +
               pod.funcPtrTableAndExitBytes_;
    }
    static unsigned heapGlobalDataOffset() {
        return 0;
    }
    uint8_t *&heapDatum() const {
        JS_ASSERT(isFinished());
        return *(uint8_t**)(globalData() + heapGlobalDataOffset());
    }
    unsigned globalVarIndexToGlobalDataOffset(unsigned i) const {
        JS_ASSERT(isFinishedWithModulePrologue());
        JS_ASSERT(i < pod.numGlobalVars_);
        return sizeof(uint64_t) +
               i * sizeof(uint64_t);
    }
    void *globalVarIndexToGlobalDatum(unsigned i) const {
        JS_ASSERT(isFinished());
        return (void *)(globalData() + globalVarIndexToGlobalDataOffset(i));
    }
    uint8_t **globalDataOffsetToFuncPtrTable(unsigned globalDataOffset) const {
        JS_ASSERT(isFinished());
        JS_ASSERT(globalDataOffset < globalDataBytes());
        return (uint8_t **)(globalData() + globalDataOffset);
    }
    unsigned exitIndexToGlobalDataOffset(unsigned exitIndex) const {
        JS_ASSERT(isFinishedWithModulePrologue());
        return exits_[exitIndex].globalDataOffset();
    }
    ExitDatum &exitIndexToGlobalDatum(unsigned exitIndex) const {
        JS_ASSERT(isFinished());
        return *(ExitDatum *)(globalData() + exitIndexToGlobalDataOffset(exitIndex));
    }
    void detachIonCompilation(size_t exitIndex) const {
        JS_ASSERT(isFinished());
        exitIndexToGlobalDatum(exitIndex).exit = interpExitTrampoline(exit(exitIndex));
    }

    /*************************************************************************/
    // These functions are called after finish() but before staticallyLink():

    bool addRelativeLink(RelativeLink link) {
        JS_ASSERT(isFinished() && !isStaticallyLinked());
        return staticLinkData_.relativeLinks.append(link);
    }

    // A module is serialized after it is finished but before it is statically
    // linked. (Technically, it could be serialized after static linking, but it
    // would still need to be statically linked on deserialization.)
    size_t serializedSize() const;
    uint8_t *serialize(uint8_t *cursor) const;
    const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor);

    // Additionally, this function is called to flush the i-cache after
    // deserialization and cloning (but still before static linking, to prevent
    // a bunch of expensive micro-flushes).
    void setAutoFlushICacheRange();

    /*************************************************************************/

    // After a module isFinished compiling or deserializing, it is "statically
    // linked" which specializes the code to its current address (this allows
    // code to be relocated between serialization and deserialization).
    void staticallyLink(ExclusiveContext *cx);

    // After a module is statically linked, it is "dynamically linked" which
    // specializes it to a particular set of arguments. In particular, this
    // binds the code to a particular heap (via initHeap) and set of global
    // variables. A given asm.js module cannot be dynamically linked more than
    // once so, if JS tries, the module is cloned.
    void setIsDynamicallyLinked() {
        JS_ASSERT(!isDynamicallyLinked());
        dynamicallyLinked_ = true;
        JS_ASSERT(isDynamicallyLinked());
    }
    void initHeap(Handle<ArrayBufferObject*> heap, JSContext *cx);
    bool clone(JSContext *cx, ScopedJSDeletePtr<AsmJSModule> *moduleOut) const;
    void restoreToInitialState(ArrayBufferObject *maybePrevBuffer, ExclusiveContext *cx);

    /*************************************************************************/
    // Functions that can be called after dynamic linking succeeds:

    CodePtr entryTrampoline(const ExportedFunction &func) const {
        JS_ASSERT(isDynamicallyLinked());
        return JS_DATA_TO_FUNC_PTR(CodePtr, code_ + func.pod.codeOffset_);
    }
    uint8_t *interruptExit() const {
        JS_ASSERT(isDynamicallyLinked());
        return interruptExit_;
    }
    uint8_t *maybeHeap() const {
        JS_ASSERT(isDynamicallyLinked());
        return heapDatum();
    }
    ArrayBufferObject *maybeHeapBufferObject() const {
        JS_ASSERT(isDynamicallyLinked());
        return maybeHeap_;
    }
    size_t heapLength() const {
        JS_ASSERT(isDynamicallyLinked());
        return maybeHeap_ ? maybeHeap_->byteLength() : 0;
    }

    // Additionally, these functions may only be called while holding the
    // runtime's interrupt lock.
    void protectCode(JSRuntime *rt) const;
    void unprotectCode(JSRuntime *rt) const;
    bool codeIsProtected(JSRuntime *rt) const;
};

// Store the just-parsed module in the cache using AsmJSCacheOps.
extern bool
StoreAsmJSModuleInCache(AsmJSParser &parser,
                        const AsmJSModule &module,
                        ExclusiveContext *cx);

// Attempt to load the asm.js module that is about to be parsed from the cache
// using AsmJSCacheOps. On cache hit, *module will be non-null. Note: the
// return value indicates whether or not an error was encountered, not whether
// there was a cache hit.
extern bool
LookupAsmJSModuleInCache(ExclusiveContext *cx,
                         AsmJSParser &parser,
                         ScopedJSDeletePtr<AsmJSModule> *module,
                         ScopedJSFreePtr<char> *compilationTimeReport);

// An AsmJSModuleObject is an internal implementation object (i.e., not exposed
// directly to user script) which manages the lifetime of an AsmJSModule. A
// JSObject is necessary since we want LinkAsmJS/CallAsmJS JSFunctions to be
// able to point to their module via their extended slots.
class AsmJSModuleObject : public JSObject
{
    static const unsigned MODULE_SLOT = 0;

  public:
    static const unsigned RESERVED_SLOTS = 1;

    // On success, return an AsmJSModuleClass JSObject that has taken ownership
    // (and release()ed) the given module.
    static AsmJSModuleObject *create(ExclusiveContext *cx, ScopedJSDeletePtr<AsmJSModule> *module);

    AsmJSModule &module() const;

    void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t *asmJSModuleCode,
                       size_t *asmJSModuleData) {
        module().addSizeOfMisc(mallocSizeOf, asmJSModuleCode, asmJSModuleData);
    }

    static const Class class_;
};

}  // namespace js

#endif  // JS_ION

#endif /* jit_AsmJSModule_h */

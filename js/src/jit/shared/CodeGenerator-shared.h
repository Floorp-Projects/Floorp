/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_CodeGenerator_shared_h
#define jit_shared_CodeGenerator_shared_h

#include "mozilla/Alignment.h"

#include "jit/IonFrames.h"
#include "jit/IonMacroAssembler.h"
#include "jit/LIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/Safepoints.h"
#include "jit/Snapshots.h"
#include "jit/VMFunctions.h"
#include "vm/ForkJoin.h"

namespace js {
namespace jit {

class OutOfLineCode;
class CodeGenerator;
class MacroAssembler;
class IonCache;

template <class ArgSeq, class StoreOutputTo>
class OutOfLineCallVM;

class OutOfLineTruncateSlow;

struct PatchableBackedgeInfo
{
    CodeOffsetJump backedge;
    Label *loopHeader;
    Label *interruptCheck;

    PatchableBackedgeInfo(CodeOffsetJump backedge, Label *loopHeader, Label *interruptCheck)
      : backedge(backedge), loopHeader(loopHeader), interruptCheck(interruptCheck)
    {}
};

struct ReciprocalMulConstants {
    int32_t multiplier;
    int32_t shiftAmount;
};

class CodeGeneratorShared : public LInstructionVisitor
{
    js::Vector<OutOfLineCode *, 0, SystemAllocPolicy> outOfLineCode_;
    OutOfLineCode *oolIns;

    MacroAssembler &ensureMasm(MacroAssembler *masm);
    mozilla::Maybe<MacroAssembler> maybeMasm_;

  public:
    MacroAssembler &masm;

  protected:
    MIRGenerator *gen;
    LIRGraph &graph;
    LBlock *current;
    SnapshotWriter snapshots_;
    RecoverWriter recovers_;
    JitCode *deoptTable_;
#ifdef DEBUG
    uint32_t pushedArgs_;
#endif
    uint32_t lastOsiPointOffset_;
    SafepointWriter safepoints_;
    Label invalidate_;
    CodeOffsetLabel invalidateEpilogueData_;

    js::Vector<SafepointIndex, 0, SystemAllocPolicy> safepointIndices_;
    js::Vector<OsiIndex, 0, SystemAllocPolicy> osiIndices_;

    // Mapping from bailout table ID to an offset in the snapshot buffer.
    js::Vector<SnapshotOffset, 0, SystemAllocPolicy> bailouts_;

    // Allocated data space needed at runtime.
    js::Vector<uint8_t, 0, SystemAllocPolicy> runtimeData_;

    // Vector of information about generated polymorphic inline caches.
    js::Vector<uint32_t, 0, SystemAllocPolicy> cacheList_;

    // List of stack slots that have been pushed as arguments to an MCall.
    js::Vector<uint32_t, 0, SystemAllocPolicy> pushedArgumentSlots_;

    // Patchable backedges generated for loops.
    Vector<PatchableBackedgeInfo, 0, SystemAllocPolicy> patchableBackedges_;

#ifdef JS_TRACE_LOGGING
    js::Vector<CodeOffsetLabel, 0, SystemAllocPolicy> patchableTraceLoggers_;
    js::Vector<CodeOffsetLabel, 0, SystemAllocPolicy> patchableTLScripts_;
#endif

  public:
    struct NativeToBytecode {
        CodeOffsetLabel nativeOffset;
        InlineScriptTree *tree;
        jsbytecode *pc;
    };

  protected:
    js::Vector<NativeToBytecode, 0, SystemAllocPolicy> nativeToBytecodeList_;
    uint8_t *nativeToBytecodeMap_;
    uint32_t nativeToBytecodeMapSize_;
    uint32_t nativeToBytecodeTableOffset_;
    uint32_t nativeToBytecodeNumRegions_;

    JSScript **nativeToBytecodeScriptList_;
    uint32_t nativeToBytecodeScriptListLength_;

    // When profiling is enabled, this is the instrumentation manager which
    // maintains state of what script is currently being generated (for inline
    // scripts) and when instrumentation needs to be emitted or skipped.
    IonInstrumentation sps_;

    bool isNativeToBytecodeMapEnabled() {
        return gen->isNativeToBytecodeMapEnabled();
    }

  protected:
    // The offset of the first instruction of the OSR entry block from the
    // beginning of the code buffer.
    size_t osrEntryOffset_;

    TempAllocator &alloc() const {
        return graph.mir().alloc();
    }

    inline void setOsrEntryOffset(size_t offset) {
        JS_ASSERT(osrEntryOffset_ == 0);
        osrEntryOffset_ = offset;
    }
    inline size_t getOsrEntryOffset() const {
        return osrEntryOffset_;
    }

    // The offset of the first instruction of the body.
    // This skips the arguments type checks.
    size_t skipArgCheckEntryOffset_;

    inline void setSkipArgCheckEntryOffset(size_t offset) {
        JS_ASSERT(skipArgCheckEntryOffset_ == 0);
        skipArgCheckEntryOffset_ = offset;
    }
    inline size_t getSkipArgCheckEntryOffset() const {
        return skipArgCheckEntryOffset_;
    }

    typedef js::Vector<SafepointIndex, 8, SystemAllocPolicy> SafepointIndices;

    bool markArgumentSlots(LSafepoint *safepoint);
    void dropArguments(unsigned argc);

  protected:
#ifdef CHECK_OSIPOINT_REGISTERS
    // See js_JitOptions.checkOsiPointRegisters. We set this here to avoid
    // races when enableOsiPointRegisterChecks is called while we're generating
    // code off-thread.
    bool checkOsiPointRegisters;
#endif

    // The initial size of the frame in bytes. These are bytes beyond the
    // constant header present for every Ion frame, used for pre-determined
    // spills.
    int32_t frameDepth_;

    // In some cases, we force stack alignment to platform boundaries, see
    // also CodeGeneratorShared constructor. This value records the adjustment
    // we've done.
    int32_t frameInitialAdjustment_;

    // Frame class this frame's size falls into (see IonFrame.h).
    FrameSizeClass frameClass_;

    // For arguments to the current function.
    inline int32_t ArgToStackOffset(int32_t slot) const {
        return masm.framePushed() +
               (gen->compilingAsmJS() ? sizeof(AsmJSFrame) : sizeof(IonJSFrameLayout)) +
               slot;
    }

    // For the callee of the current function.
    inline int32_t CalleeStackOffset() const {
        return masm.framePushed() + IonJSFrameLayout::offsetOfCalleeToken();
    }

    inline int32_t SlotToStackOffset(int32_t slot) const {
        JS_ASSERT(slot > 0 && slot <= int32_t(graph.localSlotCount()));
        int32_t offset = masm.framePushed() - frameInitialAdjustment_ - slot;
        JS_ASSERT(offset >= 0);
        return offset;
    }
    inline int32_t StackOffsetToSlot(int32_t offset) const {
        // See: SlotToStackOffset. This is used to convert pushed arguments
        // to a slot index that safepoints can use.
        //
        // offset = framePushed - frameInitialAdjustment - slot
        // offset + slot = framePushed - frameInitialAdjustment
        // slot = framePushed - frameInitialAdjustement - offset
        return masm.framePushed() - frameInitialAdjustment_ - offset;
    }

    // For argument construction for calls. Argslots are Value-sized.
    inline int32_t StackOffsetOfPassedArg(int32_t slot) const {
        // A slot of 0 is permitted only to calculate %esp offset for calls.
        JS_ASSERT(slot >= 0 && slot <= int32_t(graph.argumentSlotCount()));
        int32_t offset = masm.framePushed() -
                       graph.paddedLocalSlotsSize() -
                       (slot * sizeof(Value));

        // Passed arguments go below A function's local stack storage.
        // When arguments are being pushed, there is nothing important on the stack.
        // Therefore, It is safe to push the arguments down arbitrarily.  Pushing
        // by sizeof(Value) is desirable since everything on the stack is a Value.
        // Note that paddedLocalSlotCount() aligns to at least a Value boundary
        // specifically to support this.
        JS_ASSERT(offset >= 0);
        JS_ASSERT(offset % sizeof(Value) == 0);
        return offset;
    }

    inline int32_t ToStackOffset(const LAllocation *a) const {
        if (a->isArgument())
            return ArgToStackOffset(a->toArgument()->index());
        return SlotToStackOffset(a->toStackSlot()->slot());
    }

    uint32_t frameSize() const {
        return frameClass_ == FrameSizeClass::None() ? frameDepth_ : frameClass_.frameSize();
    }

  protected:
    // Ensure the cache is an IonCache while expecting the size of the derived
    // class. We only need the cache list at GC time. Everyone else can just take
    // runtimeData offsets.
    size_t allocateCache(const IonCache &, size_t size) {
        size_t dataOffset = allocateData(size);
        masm.propagateOOM(cacheList_.append(dataOffset));
        return dataOffset;
    }

#ifdef CHECK_OSIPOINT_REGISTERS
    void resetOsiPointRegs(LSafepoint *safepoint);
    bool shouldVerifyOsiPointRegs(LSafepoint *safepoint);
    void verifyOsiPointRegs(LSafepoint *safepoint);
#endif

    bool addNativeToBytecodeEntry(const BytecodeSite &site);
    void dumpNativeToBytecodeEntries();
    void dumpNativeToBytecodeEntry(uint32_t idx);

  public:
    MIRGenerator &mirGen() const {
        return *gen;
    }

    // When appending to runtimeData_, the vector might realloc, leaving pointers
    // int the origianl vector stale and unusable. DataPtr acts like a pointer,
    // but allows safety in the face of potentially realloc'ing vector appends.
    friend class DataPtr;
    template <typename T>
    class DataPtr
    {
        CodeGeneratorShared *cg_;
        size_t index_;

        T *lookup() {
            return reinterpret_cast<T *>(&cg_->runtimeData_[index_]);
        }
      public:
        DataPtr(CodeGeneratorShared *cg, size_t index)
          : cg_(cg), index_(index) { }

        T * operator ->() {
            return lookup();
        }
        T * operator *() {
            return lookup();
        }
    };

  protected:

    size_t allocateData(size_t size) {
        JS_ASSERT(size % sizeof(void *) == 0);
        size_t dataOffset = runtimeData_.length();
        masm.propagateOOM(runtimeData_.appendN(0, size));
        return dataOffset;
    }

    template <typename T>
    inline size_t allocateCache(const T &cache) {
        size_t index = allocateCache(cache, sizeof(mozilla::AlignedStorage2<T>));
        if (masm.oom())
            return SIZE_MAX;
        // Use the copy constructor on the allocated space.
        JS_ASSERT(index == cacheList_.back());
        new (&runtimeData_[index]) T(cache);
        return index;
    }

  protected:
    // Encodes an LSnapshot into the compressed snapshot buffer, returning
    // false on failure.
    bool encode(LRecoverInfo *recover);
    bool encode(LSnapshot *snapshot);
    bool encodeAllocation(LSnapshot *snapshot, MDefinition *def, uint32_t *startIndex);

    // Attempts to assign a BailoutId to a snapshot, if one isn't already set.
    // If the bailout table is full, this returns false, which is not a fatal
    // error (the code generator may use a slower bailout mechanism).
    bool assignBailoutId(LSnapshot *snapshot);

    // Encode all encountered safepoints in CG-order, and resolve |indices| for
    // safepoint offsets.
    void encodeSafepoints();

    // Fixup offsets of native-to-bytecode map.
    bool createNativeToBytecodeScriptList(JSContext *cx);
    bool generateCompactNativeToBytecodeMap(JSContext *cx, JitCode *code);
    void verifyCompactNativeToBytecodeMap(JitCode *code);

    // Mark the safepoint on |ins| as corresponding to the current assembler location.
    // The location should be just after a call.
    bool markSafepoint(LInstruction *ins);
    bool markSafepointAt(uint32_t offset, LInstruction *ins);

    // Mark the OSI point |ins| as corresponding to the current
    // assembler location inside the |osiIndices_|. Return the assembler
    // location for the OSI point return location within
    // |returnPointOffset|.
    bool markOsiPoint(LOsiPoint *ins, uint32_t *returnPointOffset);

    // Ensure that there is enough room between the last OSI point and the
    // current instruction, such that:
    //  (1) Invalidation will not overwrite the current instruction, and
    //  (2) Overwriting the current instruction will not overwrite
    //      an invalidation marker.
    void ensureOsiSpace();

    OutOfLineCode *oolTruncateDouble(FloatRegister src, Register dest, MInstruction *mir);
    bool emitTruncateDouble(FloatRegister src, Register dest, MInstruction *mir);
    bool emitTruncateFloat32(FloatRegister src, Register dest, MInstruction *mir);

    void emitPreBarrier(Register base, const LAllocation *index, MIRType type);
    void emitPreBarrier(Address address, MIRType type);

    // We don't emit code for trivial blocks, so if we want to branch to the
    // given block, and it's trivial, return the ultimate block we should
    // actually branch directly to.
    MBasicBlock *skipTrivialBlocks(MBasicBlock *block) {
        while (block->lir()->isTrivial()) {
            JS_ASSERT(block->lir()->rbegin()->numSuccessors() == 1);
            block = block->lir()->rbegin()->getSuccessor(0);
        }
        return block;
    }

    // Test whether the given block can be reached via fallthrough from the
    // current block.
    inline bool isNextBlock(LBlock *block) {
        uint32_t target = skipTrivialBlocks(block->mir())->id();
        uint32_t i = current->mir()->id() + 1;
        if (target < i)
            return false;
        // Trivial blocks can be crossed via fallthrough.
        for (; i != target; ++i) {
            if (!graph.getBlock(i)->isTrivial())
                return false;
        }
        return true;
    }

  public:
    // Save and restore all volatile registers to/from the stack, excluding the
    // specified register(s), before a function call made using callWithABI and
    // after storing the function call's return value to an output register.
    // (The only registers that don't need to be saved/restored are 1) the
    // temporary register used to store the return value of the function call,
    // if there is one [otherwise that stored value would be overwritten]; and
    // 2) temporary registers whose values aren't needed in the rest of the LIR
    // instruction [this is purely an optimization].  All other volatiles must
    // be saved and restored in case future LIR instructions need those values.)
    void saveVolatile(Register output) {
        RegisterSet regs = RegisterSet::Volatile();
        regs.takeUnchecked(output);
        masm.PushRegsInMask(regs);
    }
    void restoreVolatile(Register output) {
        RegisterSet regs = RegisterSet::Volatile();
        regs.takeUnchecked(output);
        masm.PopRegsInMask(regs);
    }
    void saveVolatile(FloatRegister output) {
        RegisterSet regs = RegisterSet::Volatile();
        regs.takeUnchecked(output);
        masm.PushRegsInMask(regs);
    }
    void restoreVolatile(FloatRegister output) {
        RegisterSet regs = RegisterSet::Volatile();
        regs.takeUnchecked(output);
        masm.PopRegsInMask(regs);
    }
    void saveVolatile(RegisterSet temps) {
        masm.PushRegsInMask(RegisterSet::VolatileNot(temps));
    }
    void restoreVolatile(RegisterSet temps) {
        masm.PopRegsInMask(RegisterSet::VolatileNot(temps));
    }
    void saveVolatile() {
        masm.PushRegsInMask(RegisterSet::Volatile());
    }
    void restoreVolatile() {
        masm.PopRegsInMask(RegisterSet::Volatile());
    }

    // These functions have to be called before and after any callVM and before
    // any modifications of the stack.  Modification of the stack made after
    // these calls should update the framePushed variable, needed by the exit
    // frame produced by callVM.
    inline void saveLive(LInstruction *ins);
    inline void restoreLive(LInstruction *ins);
    inline void restoreLiveIgnore(LInstruction *ins, RegisterSet reg);

    // Save/restore all registers that are both live and volatile.
    inline void saveLiveVolatile(LInstruction *ins);
    inline void restoreLiveVolatile(LInstruction *ins);

    template <typename T>
    void pushArg(const T &t) {
        masm.Push(t);
#ifdef DEBUG
        pushedArgs_++;
#endif
    }

    void storeResultTo(Register reg) {
        masm.storeCallResult(reg);
    }

    void storeFloatResultTo(FloatRegister reg) {
        masm.storeCallFloatResult(reg);
    }

    template <typename T>
    void storeResultValueTo(const T &t) {
        masm.storeCallResultValue(t);
    }

    bool callVM(const VMFunction &f, LInstruction *ins, const Register *dynStack = nullptr);

    template <class ArgSeq, class StoreOutputTo>
    inline OutOfLineCode *oolCallVM(const VMFunction &fun, LInstruction *ins, const ArgSeq &args,
                                    const StoreOutputTo &out);

    bool callVM(const VMFunctionsModal &f, LInstruction *ins, const Register *dynStack = nullptr) {
        return callVM(f[gen->info().executionMode()], ins, dynStack);
    }

    template <class ArgSeq, class StoreOutputTo>
    inline OutOfLineCode *oolCallVM(const VMFunctionsModal &f, LInstruction *ins,
                                    const ArgSeq &args, const StoreOutputTo &out)
    {
        return oolCallVM(f[gen->info().executionMode()], ins, args, out);
    }

    bool addCache(LInstruction *lir, size_t cacheIndex);
    size_t addCacheLocations(const CacheLocationList &locs, size_t *numLocs);
    ReciprocalMulConstants computeDivisionConstants(int d);

  protected:
    bool addOutOfLineCode(OutOfLineCode *code, const MInstruction *mir);
    bool addOutOfLineCode(OutOfLineCode *code, const BytecodeSite &site);
    bool hasOutOfLineCode() { return !outOfLineCode_.empty(); }
    bool generateOutOfLineCode();

    Label *labelForBackedgeWithImplicitCheck(MBasicBlock *mir);

    // Generate a jump to the start of the specified block, adding information
    // if this is a loop backedge. Use this in place of jumping directly to
    // mir->lir()->label(), or use getJumpLabelForBranch() if a label to use
    // directly is needed.
    void jumpToBlock(MBasicBlock *mir);

// This function is not used for MIPS. MIPS has branchToBlock.
#ifndef JS_CODEGEN_MIPS
    void jumpToBlock(MBasicBlock *mir, Assembler::Condition cond);
#endif

  private:
    void generateInvalidateEpilogue();

    void setupSimdAlignment(unsigned fixup);

  public:
    CodeGeneratorShared(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm);

  public:
    template <class ArgSeq, class StoreOutputTo>
    bool visitOutOfLineCallVM(OutOfLineCallVM<ArgSeq, StoreOutputTo> *ool);

    bool visitOutOfLineTruncateSlow(OutOfLineTruncateSlow *ool);

    bool omitOverRecursedCheck() const;

#ifdef JS_TRACE_LOGGING
  protected:
    bool emitTracelogScript(bool isStart);
    bool emitTracelogTree(bool isStart, uint32_t textId);

  public:
    bool emitTracelogScriptStart() {
        return emitTracelogScript(/* isStart =*/ true);
    }
    bool emitTracelogScriptStop() {
        return emitTracelogScript(/* isStart =*/ false);
    }
    bool emitTracelogStartEvent(uint32_t textId) {
        return emitTracelogTree(/* isStart =*/ true, textId);
    }
    bool emitTracelogStopEvent(uint32_t textId) {
        return emitTracelogTree(/* isStart =*/ false, textId);
    }
#endif
};

// An out-of-line path is generated at the end of the function.
class OutOfLineCode : public TempObject
{
    Label entry_;
    Label rejoin_;
    uint32_t framePushed_;
    BytecodeSite site_;

  public:
    OutOfLineCode()
      : framePushed_(0),
        site_()
    { }

    virtual bool generate(CodeGeneratorShared *codegen) = 0;

    Label *entry() {
        return &entry_;
    }
    virtual void bind(MacroAssembler *masm) {
        masm->bind(entry());
    }
    Label *rejoin() {
        return &rejoin_;
    }
    void setFramePushed(uint32_t framePushed) {
        framePushed_ = framePushed;
    }
    uint32_t framePushed() const {
        return framePushed_;
    }
    void setBytecodeSite(const BytecodeSite &site) {
        site_ = site;
    }
    const BytecodeSite &bytecodeSite() const {
        return site_;
    }
    jsbytecode *pc() const {
        return site_.pc();
    }
    JSScript *script() const {
        return site_.script();
    }
};

// For OOL paths that want a specific-typed code generator.
template <typename T>
class OutOfLineCodeBase : public OutOfLineCode
{
  public:
    virtual bool generate(CodeGeneratorShared *codegen) {
        return accept(static_cast<T *>(codegen));
    }

  public:
    virtual bool accept(T *codegen) = 0;
};

// ArgSeq store arguments for OutOfLineCallVM.
//
// OutOfLineCallVM are created with "oolCallVM" function. The third argument of
// this function is an instance of a class which provides a "generate" function
// to call the "pushArg" needed by the VMFunction call.  The list of argument
// can be created by using the ArgList function which create an empty list of
// arguments.  Arguments are added to this list by using the comma operator.
// The type of the argument list is returned by the comma operator, and due to
// templates arguments, it is quite painful to write by hand.  It is recommended
// to use it directly as argument of a template function which would get its
// arguments infered by the compiler (such as oolCallVM).  The list of arguments
// must be written in the same order as if you were calling the function in C++.
//
// Example:
//   (ArgList(), ToRegister(lir->lhs()), ToRegister(lir->rhs()))

template <class SeqType, typename LastType>
class ArgSeq : public SeqType
{
  private:
    typedef ArgSeq<SeqType, LastType> ThisType;
    LastType last_;

  public:
    ArgSeq(const SeqType &seq, const LastType &last)
      : SeqType(seq),
        last_(last)
    { }

    template <typename NextType>
    inline ArgSeq<ThisType, NextType>
    operator, (const NextType &last) const {
        return ArgSeq<ThisType, NextType>(*this, last);
    }

    inline void generate(CodeGeneratorShared *codegen) const {
        codegen->pushArg(last_);
        this->SeqType::generate(codegen);
    }
};

// Mark the end of an argument list.
template <>
class ArgSeq<void, void>
{
  private:
    typedef ArgSeq<void, void> ThisType;

  public:
    ArgSeq() { }
    ArgSeq(const ThisType &) { }

    template <typename NextType>
    inline ArgSeq<ThisType, NextType>
    operator, (const NextType &last) const {
        return ArgSeq<ThisType, NextType>(*this, last);
    }

    inline void generate(CodeGeneratorShared *codegen) const {
    }
};

inline ArgSeq<void, void>
ArgList()
{
    return ArgSeq<void, void>();
}

// Store wrappers, to generate the right move of data after the VM call.

struct StoreNothing
{
    inline void generate(CodeGeneratorShared *codegen) const {
    }
    inline RegisterSet clobbered() const {
        return RegisterSet(); // No register gets clobbered
    }
};

class StoreRegisterTo
{
  private:
    Register out_;

  public:
    explicit StoreRegisterTo(Register out)
      : out_(out)
    { }

    inline void generate(CodeGeneratorShared *codegen) const {
        codegen->storeResultTo(out_);
    }
    inline RegisterSet clobbered() const {
        RegisterSet set = RegisterSet();
        set.add(out_);
        return set;
    }
};

class StoreFloatRegisterTo
{
  private:
    FloatRegister out_;

  public:
    explicit StoreFloatRegisterTo(FloatRegister out)
      : out_(out)
    { }

    inline void generate(CodeGeneratorShared *codegen) const {
        codegen->storeFloatResultTo(out_);
    }
    inline RegisterSet clobbered() const {
        RegisterSet set = RegisterSet();
        set.add(out_);
        return set;
    }
};

template <typename Output>
class StoreValueTo_
{
  private:
    Output out_;

  public:
    explicit StoreValueTo_(const Output &out)
      : out_(out)
    { }

    inline void generate(CodeGeneratorShared *codegen) const {
        codegen->storeResultValueTo(out_);
    }
    inline RegisterSet clobbered() const {
        RegisterSet set = RegisterSet();
        set.add(out_);
        return set;
    }
};

template <typename Output>
StoreValueTo_<Output> StoreValueTo(const Output &out)
{
    return StoreValueTo_<Output>(out);
}

template <class ArgSeq, class StoreOutputTo>
class OutOfLineCallVM : public OutOfLineCodeBase<CodeGeneratorShared>
{
  private:
    LInstruction *lir_;
    const VMFunction &fun_;
    ArgSeq args_;
    StoreOutputTo out_;

  public:
    OutOfLineCallVM(LInstruction *lir, const VMFunction &fun, const ArgSeq &args,
                    const StoreOutputTo &out)
      : lir_(lir),
        fun_(fun),
        args_(args),
        out_(out)
    { }

    bool accept(CodeGeneratorShared *codegen) {
        return codegen->visitOutOfLineCallVM(this);
    }

    LInstruction *lir() const { return lir_; }
    const VMFunction &function() const { return fun_; }
    const ArgSeq &args() const { return args_; }
    const StoreOutputTo &out() const { return out_; }
};

template <class ArgSeq, class StoreOutputTo>
inline OutOfLineCode *
CodeGeneratorShared::oolCallVM(const VMFunction &fun, LInstruction *lir, const ArgSeq &args,
                               const StoreOutputTo &out)
{
    JS_ASSERT(lir->mirRaw());
    JS_ASSERT(lir->mirRaw()->isInstruction());

    OutOfLineCode *ool = new(alloc()) OutOfLineCallVM<ArgSeq, StoreOutputTo>(lir, fun, args, out);
    if (!addOutOfLineCode(ool, lir->mirRaw()->toInstruction()))
        return nullptr;
    return ool;
}

template <class ArgSeq, class StoreOutputTo>
bool
CodeGeneratorShared::visitOutOfLineCallVM(OutOfLineCallVM<ArgSeq, StoreOutputTo> *ool)
{
    LInstruction *lir = ool->lir();

    saveLive(lir);
    ool->args().generate(this);
    if (!callVM(ool->function(), lir))
        return false;
    ool->out().generate(this);
    restoreLiveIgnore(lir, ool->out().clobbered());
    masm.jump(ool->rejoin());
    return true;
}

} // namespace jit
} // namespace js

#endif /* jit_shared_CodeGenerator_shared_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MacroAssembler_h
#define jit_MacroAssembler_h

#include "mozilla/MacroForEach.h"
#include "mozilla/MathAlgorithms.h"

#include "jscompartment.h"

#if defined(JS_CODEGEN_X86)
# include "jit/x86/MacroAssembler-x86.h"
#elif defined(JS_CODEGEN_X64)
# include "jit/x64/MacroAssembler-x64.h"
#elif defined(JS_CODEGEN_ARM)
# include "jit/arm/MacroAssembler-arm.h"
#elif defined(JS_CODEGEN_ARM64)
# include "jit/arm64/MacroAssembler-arm64.h"
#elif defined(JS_CODEGEN_MIPS32)
# include "jit/mips32/MacroAssembler-mips32.h"
#elif defined(JS_CODEGEN_MIPS64)
# include "jit/mips64/MacroAssembler-mips64.h"
#elif defined(JS_CODEGEN_NONE)
# include "jit/none/MacroAssembler-none.h"
#else
# error "Unknown architecture!"
#endif
#include "jit/AtomicOp.h"
#include "jit/IonInstrumentation.h"
#include "jit/JitCompartment.h"
#include "jit/VMFunctions.h"
#include "vm/ProxyObject.h"
#include "vm/Shape.h"
#include "vm/TypedArrayObject.h"
#include "vm/UnboxedObject.h"

using mozilla::FloatingPoint;

// * How to read/write MacroAssembler method declarations:
//
// The following macros are made to avoid #ifdef around each method declarations
// of the Macro Assembler, and they are also used as an hint on the location of
// the implementations of each method.  For example, the following declaration
//
//   void Pop(FloatRegister t) DEFINED_ON(x86_shared, arm);
//
// suggests the MacroAssembler::Pop(FloatRegister) method is implemented in
// x86-shared/MacroAssembler-x86-shared.h, and also in arm/MacroAssembler-arm.h.
//
// - If there is no annotation, then there is only one generic definition in
//   MacroAssembler.cpp.
//
// - If the declaration is "inline", then the method definition(s) would be in
//   the "-inl.h" variant of the same file(s).
//
// The script check_macroassembler_style.py (check-masm target of the Makefile)
// is used to verify that method definitions are matching the annotation added
// to the method declarations.  If there is any difference, then you either
// forgot to define the method in one of the macro assembler, or you forgot to
// update the annotation of the macro assembler declaration.
//
// Some convenient short-cuts are used to avoid repeating the same list of
// architectures on each method declaration, such as PER_ARCH and
// PER_SHARED_ARCH.

# define ALL_ARCH mips32, mips64, arm, arm64, x86, x64
# define ALL_SHARED_ARCH arm, arm64, x86_shared, mips_shared

// * How this macro works:
//
// DEFINED_ON is a macro which check if, for the current architecture, the
// method is defined on the macro assembler or not.
//
// For each architecture, we have a macro named DEFINED_ON_arch.  This macro is
// empty if this is not the current architecture.  Otherwise it must be either
// set to "define" or "crash" (only use for the none target so-far).
//
// The DEFINED_ON macro maps the list of architecture names given as argument to
// a list of macro names.  For example,
//
//   DEFINED_ON(arm, x86_shared)
//
// is expanded to
//
//   DEFINED_ON_none DEFINED_ON_arm DEFINED_ON_x86_shared
//
// which are later expanded on ARM, x86, x64 by DEFINED_ON_EXPAND_ARCH_RESULTS
// to
//
//   define
//
// or if the JIT is disabled or set to no architecture to
//
//   crash
//
// or to nothing, if the current architecture is not listed in the list of
// arguments of DEFINED_ON.  Note, only one of the DEFINED_ON_arch macro
// contributes to the non-empty result, which is the macro of the current
// architecture if it is listed in the arguments of DEFINED_ON.
//
// This result is appended to DEFINED_ON_RESULT_ before expanding the macro,
// which result is either no annotation, a MOZ_CRASH(), or a "= delete"
// annotation on the method declaration.

# define DEFINED_ON_x86
# define DEFINED_ON_x64
# define DEFINED_ON_x86_shared
# define DEFINED_ON_arm
# define DEFINED_ON_arm64
# define DEFINED_ON_mips32
# define DEFINED_ON_mips64
# define DEFINED_ON_mips_shared
# define DEFINED_ON_none

// Specialize for each architecture.
#if defined(JS_CODEGEN_X86)
# undef DEFINED_ON_x86
# define DEFINED_ON_x86 define
# undef DEFINED_ON_x86_shared
# define DEFINED_ON_x86_shared define
#elif defined(JS_CODEGEN_X64)
# undef DEFINED_ON_x64
# define DEFINED_ON_x64 define
# undef DEFINED_ON_x86_shared
# define DEFINED_ON_x86_shared define
#elif defined(JS_CODEGEN_ARM)
# undef DEFINED_ON_arm
# define DEFINED_ON_arm define
#elif defined(JS_CODEGEN_ARM64)
# undef DEFINED_ON_arm64
# define DEFINED_ON_arm64 define
#elif defined(JS_CODEGEN_MIPS32)
# undef DEFINED_ON_mips32
# define DEFINED_ON_mips32 define
# undef DEFINED_ON_mips_shared
# define DEFINED_ON_mips_shared define
#elif defined(JS_CODEGEN_MIPS64)
# undef DEFINED_ON_mips64
# define DEFINED_ON_mips64 define
# undef DEFINED_ON_mips_shared
# define DEFINED_ON_mips_shared define
#elif defined(JS_CODEGEN_NONE)
# undef DEFINED_ON_none
# define DEFINED_ON_none crash
#else
# error "Unknown architecture!"
#endif

# define DEFINED_ON_RESULT_crash   { MOZ_CRASH(); }
# define DEFINED_ON_RESULT_define
# define DEFINED_ON_RESULT_        = delete

# define DEFINED_ON_DISPATCH_RESULT_2(Macro, Result) \
    Macro ## Result
# define DEFINED_ON_DISPATCH_RESULT(...)     \
    DEFINED_ON_DISPATCH_RESULT_2(DEFINED_ON_RESULT_, __VA_ARGS__)

// We need to let the evaluation of MOZ_FOR_EACH terminates.
# define DEFINED_ON_EXPAND_ARCH_RESULTS_3(ParenResult)  \
    DEFINED_ON_DISPATCH_RESULT ParenResult
# define DEFINED_ON_EXPAND_ARCH_RESULTS_2(ParenResult)  \
    DEFINED_ON_EXPAND_ARCH_RESULTS_3 (ParenResult)
# define DEFINED_ON_EXPAND_ARCH_RESULTS(ParenResult)    \
    DEFINED_ON_EXPAND_ARCH_RESULTS_2 (ParenResult)

# define DEFINED_ON_FWDARCH(Arch) DEFINED_ON_ ## Arch
# define DEFINED_ON_MAP_ON_ARCHS(ArchList)              \
    DEFINED_ON_EXPAND_ARCH_RESULTS(                     \
      (MOZ_FOR_EACH(DEFINED_ON_FWDARCH, (), ArchList)))

# define DEFINED_ON(...)                                \
    DEFINED_ON_MAP_ON_ARCHS((none, __VA_ARGS__))

# define PER_ARCH DEFINED_ON(ALL_ARCH)
# define PER_SHARED_ARCH DEFINED_ON(ALL_SHARED_ARCH)


#if MOZ_LITTLE_ENDIAN
#define IMM32_16ADJ(X) X << 16
#else
#define IMM32_16ADJ(X) X
#endif

namespace js {
namespace jit {

// Defined in JitFrames.h
enum ExitFrameTokenValues;

// The public entrypoint for emitting assembly. Note that a MacroAssembler can
// use cx->lifoAlloc, so take care not to interleave masm use with other
// lifoAlloc use if one will be destroyed before the other.
class MacroAssembler : public MacroAssemblerSpecific
{
    MacroAssembler* thisFromCtor() {
        return this;
    }

  public:
    class AutoRooter : public JS::AutoGCRooter
    {
        MacroAssembler* masm_;

      public:
        AutoRooter(JSContext* cx, MacroAssembler* masm)
          : JS::AutoGCRooter(cx, IONMASM),
            masm_(masm)
        { }

        MacroAssembler* masm() const {
            return masm_;
        }
    };

    /*
     * Base class for creating a branch.
     */
    class Branch
    {
        bool init_;
        Condition cond_;
        Label* jump_;
        Register reg_;

      public:
        Branch()
          : init_(false),
            cond_(Equal),
            jump_(nullptr),
            reg_(Register::FromCode(0))      // Quell compiler warnings.
        { }

        Branch(Condition cond, Register reg, Label* jump)
          : init_(true),
            cond_(cond),
            jump_(jump),
            reg_(reg)
        { }

        bool isInitialized() const {
            return init_;
        }

        Condition cond() const {
            return cond_;
        }

        Label* jump() const {
            return jump_;
        }

        Register reg() const {
            return reg_;
        }

        void invertCondition() {
            cond_ = InvertCondition(cond_);
        }

        void relink(Label* jump) {
            jump_ = jump;
        }

        virtual void emit(MacroAssembler& masm) = 0;
    };

    /*
     * Creates a branch based on a specific TypeSet::Type.
     * Note: emits number test (int/double) for TypeSet::DoubleType()
     */
    class BranchType : public Branch
    {
        TypeSet::Type type_;

      public:
        BranchType()
          : Branch(),
            type_(TypeSet::UnknownType())
        { }

        BranchType(Condition cond, Register reg, TypeSet::Type type, Label* jump)
          : Branch(cond, reg, jump),
            type_(type)
        { }

        void emit(MacroAssembler& masm);
    };

    /*
     * Creates a branch based on a GCPtr.
     */
    class BranchGCPtr : public Branch
    {
        ImmGCPtr ptr_;

      public:
        BranchGCPtr()
          : Branch(),
            ptr_(ImmGCPtr(nullptr))
        { }

        BranchGCPtr(Condition cond, Register reg, ImmGCPtr ptr, Label* jump)
          : Branch(cond, reg, jump),
            ptr_(ptr)
        { }

        void emit(MacroAssembler& masm);
    };

    mozilla::Maybe<AutoRooter> autoRooter_;
    mozilla::Maybe<JitContext> jitContext_;
    mozilla::Maybe<AutoJitContextAlloc> alloc_;

  private:
    // Labels for handling exceptions and failures.
    NonAssertingLabel failureLabel_;

  public:
    MacroAssembler()
      : framePushed_(0),
#ifdef DEBUG
        inCall_(false),
#endif
        emitProfilingInstrumentation_(false)
    {
        JitContext* jcx = GetJitContext();
        JSContext* cx = jcx->cx;
        if (cx)
            constructRoot(cx);

        if (!jcx->temp) {
            MOZ_ASSERT(cx);
            alloc_.emplace(cx);
        }

        moveResolver_.setAllocator(*jcx->temp);

#if defined(JS_CODEGEN_ARM)
        initWithAllocator();
        m_buffer.id = jcx->getNextAssemblerId();
#elif defined(JS_CODEGEN_ARM64)
        initWithAllocator();
        armbuffer_.id = jcx->getNextAssemblerId();
#endif
    }

    // This constructor should only be used when there is no JitContext active
    // (for example, Trampoline-$(ARCH).cpp and IonCaches.cpp).
    explicit MacroAssembler(JSContext* cx, IonScript* ion = nullptr,
                            JSScript* script = nullptr, jsbytecode* pc = nullptr);

    // wasm compilation handles its own JitContext-pushing
    struct WasmToken {};
    explicit MacroAssembler(WasmToken, TempAllocator& alloc)
      : framePushed_(0),
#ifdef DEBUG
        inCall_(false),
#endif
        emitProfilingInstrumentation_(false)
    {
        moveResolver_.setAllocator(alloc);

#if defined(JS_CODEGEN_ARM)
        initWithAllocator();
        m_buffer.id = 0;
#elif defined(JS_CODEGEN_ARM64)
        initWithAllocator();
        armbuffer_.id = 0;
#endif
    }

    void constructRoot(JSContext* cx) {
        autoRooter_.emplace(cx, this);
    }

    MoveResolver& moveResolver() {
        return moveResolver_;
    }

    size_t instructionsSize() const {
        return size();
    }

    //{{{ check_macroassembler_style
  public:
    // ===============================================================
    // MacroAssembler high-level usage.

    // Flushes the assembly buffer, on platforms that need it.
    void flush() PER_SHARED_ARCH;

    // Add a comment that is visible in the pretty printed assembly code.
    void comment(const char* msg) PER_SHARED_ARCH;

    // ===============================================================
    // Frame manipulation functions.

    inline uint32_t framePushed() const;
    inline void setFramePushed(uint32_t framePushed);
    inline void adjustFrame(int32_t value);

    // Adjust the frame, to account for implicit modification of the stack
    // pointer, such that callee can remove arguments on the behalf of the
    // caller.
    inline void implicitPop(uint32_t bytes);

  private:
    // This field is used to statically (at compilation time) emulate a frame
    // pointer by keeping track of stack manipulations.
    //
    // It is maintained by all stack manipulation functions below.
    uint32_t framePushed_;

  public:
    // ===============================================================
    // Stack manipulation functions.

    void PushRegsInMask(LiveRegisterSet set)
                            DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);
    void PushRegsInMask(LiveGeneralRegisterSet set);

    void PopRegsInMask(LiveRegisterSet set);
    void PopRegsInMask(LiveGeneralRegisterSet set);
    void PopRegsInMaskIgnore(LiveRegisterSet set, LiveRegisterSet ignore)
                                 DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    void Push(const Operand op) DEFINED_ON(x86_shared);
    void Push(Register reg) PER_SHARED_ARCH;
    void Push(Register reg1, Register reg2, Register reg3, Register reg4) DEFINED_ON(arm64);
    void Push(const Imm32 imm) PER_SHARED_ARCH;
    void Push(const ImmWord imm) PER_SHARED_ARCH;
    void Push(const ImmPtr imm) PER_SHARED_ARCH;
    void Push(const ImmGCPtr ptr) PER_SHARED_ARCH;
    void Push(FloatRegister reg) PER_SHARED_ARCH;
    void Push(jsid id, Register scratchReg);
    void Push(TypedOrValueRegister v);
    void Push(const ConstantOrRegister& v);
    void Push(const ValueOperand& val);
    void Push(const Value& val);
    void Push(JSValueType type, Register reg);
    void PushValue(const Address& addr);
    void PushEmptyRooted(VMFunction::RootType rootType);
    inline CodeOffset PushWithPatch(ImmWord word);
    inline CodeOffset PushWithPatch(ImmPtr imm);

    void Pop(const Operand op) DEFINED_ON(x86_shared);
    void Pop(Register reg) PER_SHARED_ARCH;
    void Pop(FloatRegister t) PER_SHARED_ARCH;
    void Pop(const ValueOperand& val) PER_SHARED_ARCH;
    void popRooted(VMFunction::RootType rootType, Register cellReg, const ValueOperand& valueReg);

    // Move the stack pointer based on the requested amount.
    void adjustStack(int amount);
    void freeStack(uint32_t amount);

    // Warning: This method does not update the framePushed() counter.
    void freeStack(Register amount);

  private:
    // ===============================================================
    // Register allocation fields.
#ifdef DEBUG
    friend AutoRegisterScope;
    friend AutoFloatRegisterScope;
    // Used to track register scopes for debug builds.
    // Manipulated by the AutoGenericRegisterScope class.
    AllocatableRegisterSet debugTrackedRegisters_;
#endif // DEBUG

  public:
    // ===============================================================
    // Simple call functions.

    CodeOffset call(Register reg) PER_SHARED_ARCH;
    CodeOffset call(Label* label) PER_SHARED_ARCH;
    void call(const Address& addr) DEFINED_ON(x86_shared);
    void call(ImmWord imm) PER_SHARED_ARCH;
    // Call a target native function, which is neither traceable nor movable.
    void call(ImmPtr imm) PER_SHARED_ARCH;
    void call(wasm::SymbolicAddress imm) PER_SHARED_ARCH;
    // Call a target JitCode, which must be traceable, and may be movable.
    void call(JitCode* c) PER_SHARED_ARCH;

    inline void call(const wasm::CallSiteDesc& desc, const Register reg);
    inline void call(const wasm::CallSiteDesc& desc, uint32_t funcDefIndex);
    inline void call(const wasm::CallSiteDesc& desc, wasm::Trap trap);

    CodeOffset callWithPatch() PER_SHARED_ARCH;
    void patchCall(uint32_t callerOffset, uint32_t calleeOffset) PER_SHARED_ARCH;

    // Push the return address and make a call. On platforms where this function
    // is not defined, push the link register (pushReturnAddress) at the entry
    // point of the callee.
    void callAndPushReturnAddress(Register reg) DEFINED_ON(x86_shared);
    void callAndPushReturnAddress(Label* label) DEFINED_ON(x86_shared);

    void pushReturnAddress() DEFINED_ON(mips_shared, arm, arm64);
    void popReturnAddress() DEFINED_ON(mips_shared, arm, arm64);

  public:
    // ===============================================================
    // Patchable near/far jumps.

    // "Far jumps" provide the ability to jump to any uint32_t offset from any
    // other uint32_t offset without using a constant pool (thus returning a
    // simple CodeOffset instead of a CodeOffsetJump).
    CodeOffset farJumpWithPatch() PER_SHARED_ARCH;
    void patchFarJump(CodeOffset farJump, uint32_t targetOffset) PER_SHARED_ARCH;
    static void repatchFarJump(uint8_t* code, uint32_t farJumpOffset, uint32_t targetOffset) PER_SHARED_ARCH;

    // Emit a nop that can be patched to and from a nop and a jump with an int8
    // relative displacement.
    CodeOffset nopPatchableToNearJump() PER_SHARED_ARCH;
    static void patchNopToNearJump(uint8_t* jump, uint8_t* target) PER_SHARED_ARCH;
    static void patchNearJumpToNop(uint8_t* jump) PER_SHARED_ARCH;

  public:
    // ===============================================================
    // ABI function calls.

    // Setup a call to C/C++ code, given the assumption that the framePushed
    // accruately define the state of the stack, and that the top of the stack
    // was properly aligned. Note that this only supports cdecl.
    void setupAlignedABICall(); // CRASH_ON(arm64)

    // Setup an ABI call for when the alignment is not known. This may need a
    // scratch register.
    void setupUnalignedABICall(Register scratch) PER_ARCH;

    // Arguments must be assigned to a C/C++ call in order. They are moved
    // in parallel immediately before performing the call. This process may
    // temporarily use more stack, in which case esp-relative addresses will be
    // automatically adjusted. It is extremely important that esp-relative
    // addresses are computed *after* setupABICall(). Furthermore, no
    // operations should be emitted while setting arguments.
    void passABIArg(const MoveOperand& from, MoveOp::Type type);
    inline void passABIArg(Register reg);
    inline void passABIArg(FloatRegister reg, MoveOp::Type type);

    template <typename T>
    inline void callWithABI(const T& fun, MoveOp::Type result = MoveOp::GENERAL);

  private:
    // Reinitialize the variables which have to be cleared before making a call
    // with callWithABI.
    void setupABICall();

    // Reserve the stack and resolve the arguments move.
    void callWithABIPre(uint32_t* stackAdjust, bool callFromWasm = false) PER_ARCH;

    // Emits a call to a C/C++ function, resolving all argument moves.
    void callWithABINoProfiler(void* fun, MoveOp::Type result);
    void callWithABINoProfiler(wasm::SymbolicAddress imm, MoveOp::Type result);
    void callWithABINoProfiler(Register fun, MoveOp::Type result) PER_ARCH;
    void callWithABINoProfiler(const Address& fun, MoveOp::Type result) PER_ARCH;

    // Restore the stack to its state before the setup function call.
    void callWithABIPost(uint32_t stackAdjust, MoveOp::Type result) PER_ARCH;

    // Create the signature to be able to decode the arguments of a native
    // function, when calling a function within the simulator.
    inline void appendSignatureType(MoveOp::Type type);
    inline ABIFunctionType signature() const;

    // Private variables used to handle moves between registers given as
    // arguments to passABIArg and the list of ABI registers expected for the
    // signature of the function.
    MoveResolver moveResolver_;

    // Architecture specific implementation which specify how registers & stack
    // offsets are used for calling a function.
    ABIArgGenerator abiArgs_;

#ifdef DEBUG
    // Flag use to assert that we use ABI function in the right context.
    bool inCall_;
#endif

    // If set by setupUnalignedABICall then callWithABI will pop the stack
    // register which is on the stack.
    bool dynamicAlignment_;

#ifdef JS_SIMULATOR
    // The signature is used to accumulate all types of arguments which are used
    // by the caller. This is used by the simulators to decode the arguments
    // properly, and cast the function pointer to the right type.
    uint32_t signature_;
#endif

  public:
    // ===============================================================
    // Jit Frames.
    //
    // These functions are used to build the content of the Jit frames.  See
    // CommonFrameLayout class, and all its derivatives. The content should be
    // pushed in the opposite order as the fields of the structures, such that
    // the structures can be used to interpret the content of the stack.

    // Call the Jit function, and push the return address (or let the callee
    // push the return address).
    //
    // These functions return the offset of the return address, in order to use
    // the return address to index the safepoints, which are used to list all
    // live registers.
    inline uint32_t callJitNoProfiler(Register callee);
    inline uint32_t callJit(Register callee);
    inline uint32_t callJit(JitCode* code);

    // The frame descriptor is the second field of all Jit frames, pushed before
    // calling the Jit function.  It is a composite value defined in JitFrames.h
    inline void makeFrameDescriptor(Register frameSizeReg, FrameType type, uint32_t headerSize);

    // Push the frame descriptor, based on the statically known framePushed.
    inline void pushStaticFrameDescriptor(FrameType type, uint32_t headerSize);

    // Push the callee token of a JSFunction which pointer is stored in the
    // |callee| register. The callee token is packed with a |constructing| flag
    // which correspond to the fact that the JS function is called with "new" or
    // not.
    inline void PushCalleeToken(Register callee, bool constructing);

    // Unpack a callee token located at the |token| address, and return the
    // JSFunction pointer in the |dest| register.
    inline void loadFunctionFromCalleeToken(Address token, Register dest);

    // This function emulates a call by pushing an exit frame on the stack,
    // except that the fake-function is inlined within the body of the caller.
    //
    // This function assumes that the current frame is an IonJS frame.
    //
    // This function returns the offset of the /fake/ return address, in order to use
    // the return address to index the safepoints, which are used to list all
    // live registers.
    //
    // This function should be balanced with a call to adjustStack, to pop the
    // exit frame and emulate the return statement of the inlined function.
    inline uint32_t buildFakeExitFrame(Register scratch);

  private:
    // This function is used by buildFakeExitFrame to push a fake return address
    // on the stack. This fake return address should never be used for resuming
    // any execution, and can even be an invalid pointer into the instruction
    // stream, as long as it does not alias any other.
    uint32_t pushFakeReturnAddress(Register scratch) PER_SHARED_ARCH;

  public:
    // ===============================================================
    // Exit frame footer.
    //
    // When calling outside the Jit we push an exit frame. To mark the stack
    // correctly, we have to push additional information, called the Exit frame
    // footer, which is used to identify how the stack is marked.
    //
    // See JitFrames.h, and MarkJitExitFrame in JitFrames.cpp.

    // If the current piece of code might be garbage collected, then the exit
    // frame footer must contain a pointer to the current JitCode, such that the
    // garbage collector can keep the code alive as long this code is on the
    // stack. This function pushes a placeholder which is replaced when the code
    // is linked.
    inline void PushStubCode();

    // Return true if the code contains a self-reference which needs to be
    // patched when the code is linked.
    inline bool hasSelfReference() const;

    // Push stub code and the VMFunction pointer.
    inline void enterExitFrame(const VMFunction* f = nullptr);

    // Push an exit frame token to identify which fake exit frame this footer
    // corresponds to.
    inline void enterFakeExitFrame(enum ExitFrameTokenValues token);

    // Push an exit frame token for a native call.
    inline void enterFakeExitFrameForNative(bool isConstructing);

    // Pop ExitFrame footer in addition to the extra frame.
    inline void leaveExitFrame(size_t extraFrame = 0);

  private:
    // Save the top of the stack into PerThreadData::jitTop of the main thread,
    // which should be the location of the latest exit frame.
    void linkExitFrame();

    // Patch the value of PushStubCode with the pointer to the finalized code.
    void linkSelfReference(JitCode* code);

    // If the JitCode that created this assembler needs to transition into the VM,
    // we want to store the JitCode on the stack in order to mark it during a GC.
    // This is a reference to a patch location where the JitCode* will be written.
    CodeOffset selfReferencePatch_;

  public:
    // ===============================================================
    // Move instructions

    inline void move64(Imm64 imm, Register64 dest) PER_ARCH;
    inline void move64(Register64 src, Register64 dest) PER_ARCH;

    inline void moveFloat32ToGPR(FloatRegister src, Register dest) PER_SHARED_ARCH;
    inline void moveGPRToFloat32(Register src, FloatRegister dest) PER_SHARED_ARCH;

    inline void move8SignExtend(Register src, Register dest) PER_SHARED_ARCH;
    inline void move16SignExtend(Register src, Register dest) PER_SHARED_ARCH;

    // ===============================================================
    // Logical instructions

    inline void not32(Register reg) PER_SHARED_ARCH;

    inline void and32(Register src, Register dest) PER_SHARED_ARCH;
    inline void and32(Imm32 imm, Register dest) PER_SHARED_ARCH;
    inline void and32(Imm32 imm, Register src, Register dest) DEFINED_ON(arm64);
    inline void and32(Imm32 imm, const Address& dest) PER_SHARED_ARCH;
    inline void and32(const Address& src, Register dest) PER_SHARED_ARCH;

    inline void andPtr(Register src, Register dest) PER_ARCH;
    inline void andPtr(Imm32 imm, Register dest) PER_ARCH;

    inline void and64(Imm64 imm, Register64 dest) PER_ARCH;
    inline void or64(Imm64 imm, Register64 dest) PER_ARCH;
    inline void xor64(Imm64 imm, Register64 dest) PER_ARCH;

    inline void or32(Register src, Register dest) PER_SHARED_ARCH;
    inline void or32(Imm32 imm, Register dest) PER_SHARED_ARCH;
    inline void or32(Imm32 imm, const Address& dest) PER_SHARED_ARCH;

    inline void orPtr(Register src, Register dest) PER_ARCH;
    inline void orPtr(Imm32 imm, Register dest) PER_ARCH;

    inline void and64(Register64 src, Register64 dest) PER_ARCH;
    inline void or64(Register64 src, Register64 dest) PER_ARCH;
    inline void xor64(Register64 src, Register64 dest) PER_ARCH;

    inline void xor32(Register src, Register dest) PER_SHARED_ARCH;
    inline void xor32(Imm32 imm, Register dest) PER_SHARED_ARCH;

    inline void xorPtr(Register src, Register dest) PER_ARCH;
    inline void xorPtr(Imm32 imm, Register dest) PER_ARCH;

    inline void and64(const Operand& src, Register64 dest) DEFINED_ON(x64, mips64);
    inline void or64(const Operand& src, Register64 dest) DEFINED_ON(x64, mips64);
    inline void xor64(const Operand& src, Register64 dest) DEFINED_ON(x64, mips64);

    // ===============================================================
    // Arithmetic functions

    inline void add32(Register src, Register dest) PER_SHARED_ARCH;
    inline void add32(Imm32 imm, Register dest) PER_SHARED_ARCH;
    inline void add32(Imm32 imm, const Address& dest) PER_SHARED_ARCH;
    inline void add32(Imm32 imm, const AbsoluteAddress& dest) DEFINED_ON(x86_shared);

    inline void addPtr(Register src, Register dest) PER_ARCH;
    inline void addPtr(Register src1, Register src2, Register dest) DEFINED_ON(arm64);
    inline void addPtr(Imm32 imm, Register dest) PER_ARCH;
    inline void addPtr(Imm32 imm, Register src, Register dest) DEFINED_ON(arm64);
    inline void addPtr(ImmWord imm, Register dest) PER_ARCH;
    inline void addPtr(ImmPtr imm, Register dest);
    inline void addPtr(Imm32 imm, const Address& dest) DEFINED_ON(mips_shared, arm, arm64, x86, x64);
    inline void addPtr(Imm32 imm, const AbsoluteAddress& dest) DEFINED_ON(x86, x64);
    inline void addPtr(const Address& src, Register dest) DEFINED_ON(mips_shared, arm, arm64, x86, x64);

    inline void add64(Register64 src, Register64 dest) PER_ARCH;
    inline void add64(Imm32 imm, Register64 dest) PER_ARCH;
    inline void add64(Imm64 imm, Register64 dest) DEFINED_ON(x86, x64, arm, mips32, mips64);
    inline void add64(const Operand& src, Register64 dest) DEFINED_ON(x64, mips64);

    inline void addFloat32(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;

    inline void addDouble(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;
    inline void addConstantDouble(double d, FloatRegister dest) DEFINED_ON(x86);

    inline void sub32(const Address& src, Register dest) PER_SHARED_ARCH;
    inline void sub32(Register src, Register dest) PER_SHARED_ARCH;
    inline void sub32(Imm32 imm, Register dest) PER_SHARED_ARCH;

    inline void subPtr(Register src, Register dest) PER_ARCH;
    inline void subPtr(Register src, const Address& dest) DEFINED_ON(mips_shared, arm, arm64, x86, x64);
    inline void subPtr(Imm32 imm, Register dest) PER_ARCH;
    inline void subPtr(ImmWord imm, Register dest) DEFINED_ON(x64);
    inline void subPtr(const Address& addr, Register dest) DEFINED_ON(mips_shared, arm, arm64, x86, x64);

    inline void sub64(Register64 src, Register64 dest) PER_ARCH;
    inline void sub64(Imm64 imm, Register64 dest) DEFINED_ON(x86, x64, arm, mips32, mips64);
    inline void sub64(const Operand& src, Register64 dest) DEFINED_ON(x64, mips64);

    inline void subFloat32(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;

    inline void subDouble(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;

    // On x86-shared, srcDest must be eax and edx will be clobbered.
    inline void mul32(Register rhs, Register srcDest) PER_SHARED_ARCH;

    inline void mul32(Register src1, Register src2, Register dest, Label* onOver, Label* onZero) DEFINED_ON(arm64);

    inline void mul64(const Operand& src, const Register64& dest) DEFINED_ON(x64);
    inline void mul64(const Operand& src, const Register64& dest, const Register temp)
        DEFINED_ON(x64, mips64);
    inline void mul64(Imm64 imm, const Register64& dest) PER_ARCH;
    inline void mul64(Imm64 imm, const Register64& dest, const Register temp)
        DEFINED_ON(x86, x64, arm, mips32, mips64);
    inline void mul64(const Register64& src, const Register64& dest, const Register temp)
        PER_ARCH;

    inline void mulBy3(Register src, Register dest) PER_ARCH;

    inline void mulFloat32(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;
    inline void mulDouble(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;

    inline void mulDoublePtr(ImmPtr imm, Register temp, FloatRegister dest) DEFINED_ON(mips_shared, arm, arm64, x86, x64);

    // Perform an integer division, returning the integer part rounded toward zero.
    // rhs must not be zero, and the division must not overflow.
    //
    // On x86_shared, srcDest must be eax and edx will be clobbered.
    // On ARM, the chip must have hardware division instructions.
    inline void quotient32(Register rhs, Register srcDest, bool isUnsigned) PER_SHARED_ARCH;

    // Perform an integer division, returning the remainder part.
    // rhs must not be zero, and the division must not overflow.
    //
    // On x86_shared, srcDest must be eax and edx will be clobbered.
    // On ARM, the chip must have hardware division instructions.
    inline void remainder32(Register rhs, Register srcDest, bool isUnsigned) PER_SHARED_ARCH;

    inline void divFloat32(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;
    inline void divDouble(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;

    inline void inc32(RegisterOrInt32Constant* key);
    inline void inc64(AbsoluteAddress dest) PER_ARCH;

    inline void dec32(RegisterOrInt32Constant* key);

    inline void neg32(Register reg) PER_SHARED_ARCH;
    inline void neg64(Register64 reg) DEFINED_ON(x86, x64, arm, mips32, mips64);

    inline void negateFloat(FloatRegister reg) PER_SHARED_ARCH;

    inline void negateDouble(FloatRegister reg) PER_SHARED_ARCH;

    inline void absFloat32(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;
    inline void absDouble(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;

    inline void sqrtFloat32(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;
    inline void sqrtDouble(FloatRegister src, FloatRegister dest) PER_SHARED_ARCH;

    // srcDest = {min,max}{Float32,Double}(srcDest, other)
    // For min and max, handle NaN specially if handleNaN is true.

    inline void minFloat32(FloatRegister other, FloatRegister srcDest, bool handleNaN) PER_SHARED_ARCH;
    inline void minDouble(FloatRegister other, FloatRegister srcDest, bool handleNaN) PER_SHARED_ARCH;

    inline void maxFloat32(FloatRegister other, FloatRegister srcDest, bool handleNaN) PER_SHARED_ARCH;
    inline void maxDouble(FloatRegister other, FloatRegister srcDest, bool handleNaN) PER_SHARED_ARCH;

    // ===============================================================
    // Shift functions

    // For shift-by-register there may be platform-specific
    // variations, for example, x86 will perform the shift mod 32 but
    // ARM will perform the shift mod 256.
    //
    // For shift-by-immediate the platform assembler may restrict the
    // immediate, for example, the ARM assembler requires the count
    // for 32-bit shifts to be in the range [0,31].

    inline void lshift32(Imm32 shift, Register srcDest) PER_SHARED_ARCH;
    inline void rshift32(Imm32 shift, Register srcDest) PER_SHARED_ARCH;
    inline void rshift32Arithmetic(Imm32 shift, Register srcDest) PER_SHARED_ARCH;

    inline void lshiftPtr(Imm32 imm, Register dest) PER_ARCH;
    inline void rshiftPtr(Imm32 imm, Register dest) PER_ARCH;
    inline void rshiftPtr(Imm32 imm, Register src, Register dest) DEFINED_ON(arm64);
    inline void rshiftPtrArithmetic(Imm32 imm, Register dest) PER_ARCH;

    inline void lshift64(Imm32 imm, Register64 dest) PER_ARCH;
    inline void rshift64(Imm32 imm, Register64 dest) PER_ARCH;
    inline void rshift64Arithmetic(Imm32 imm, Register64 dest) PER_ARCH;

    // On x86_shared these have the constraint that shift must be in CL.
    inline void lshift32(Register shift, Register srcDest) PER_SHARED_ARCH;
    inline void rshift32(Register shift, Register srcDest) PER_SHARED_ARCH;
    inline void rshift32Arithmetic(Register shift, Register srcDest) PER_SHARED_ARCH;

    inline void lshift64(Register shift, Register64 srcDest) PER_ARCH;
    inline void rshift64(Register shift, Register64 srcDest) PER_ARCH;
    inline void rshift64Arithmetic(Register shift, Register64 srcDest) PER_ARCH;

    // ===============================================================
    // Rotation functions
    // Note: - on x86 and x64 the count register must be in CL.
    //       - on x64 the temp register should be InvalidReg.

    inline void rotateLeft(Imm32 count, Register input, Register dest) PER_SHARED_ARCH;
    inline void rotateLeft(Register count, Register input, Register dest) PER_SHARED_ARCH;
    inline void rotateLeft64(Imm32 count, Register64 input, Register64 dest) DEFINED_ON(x64);
    inline void rotateLeft64(Register count, Register64 input, Register64 dest) DEFINED_ON(x64);
    inline void rotateLeft64(Imm32 count, Register64 input, Register64 dest, Register temp)
        DEFINED_ON(x86, x64, arm, mips32, mips64);
    inline void rotateLeft64(Register count, Register64 input, Register64 dest, Register temp)
        PER_ARCH;

    inline void rotateRight(Imm32 count, Register input, Register dest) PER_SHARED_ARCH;
    inline void rotateRight(Register count, Register input, Register dest) PER_SHARED_ARCH;
    inline void rotateRight64(Imm32 count, Register64 input, Register64 dest) DEFINED_ON(x64);
    inline void rotateRight64(Register count, Register64 input, Register64 dest) DEFINED_ON(x64);
    inline void rotateRight64(Imm32 count, Register64 input, Register64 dest, Register temp)
        DEFINED_ON(x86, x64, arm, mips32, mips64);
    inline void rotateRight64(Register count, Register64 input, Register64 dest, Register temp)
        PER_ARCH;

    // ===============================================================
    // Bit counting functions

    // knownNotZero may be true only if the src is known not to be zero.
    inline void clz32(Register src, Register dest, bool knownNotZero) PER_SHARED_ARCH;
    inline void ctz32(Register src, Register dest, bool knownNotZero) PER_SHARED_ARCH;

    inline void clz64(Register64 src, Register dest) PER_ARCH;
    inline void ctz64(Register64 src, Register dest) PER_ARCH;

    // On x86_shared, temp may be Invalid only if the chip has the POPCNT instruction.
    // On ARM, temp may never be Invalid.
    inline void popcnt32(Register src, Register dest, Register temp) PER_SHARED_ARCH;

    // temp may be invalid only if the chip has the POPCNT instruction.
    inline void popcnt64(Register64 src, Register64 dest, Register temp) PER_ARCH;

    // ===============================================================
    // Condition functions

    template <typename T1, typename T2>
    inline void cmp32Set(Condition cond, T1 lhs, T2 rhs, Register dest)
        DEFINED_ON(x86_shared, arm, arm64, mips32, mips64);

    template <typename T1, typename T2>
    inline void cmpPtrSet(Condition cond, T1 lhs, T2 rhs, Register dest)
        PER_ARCH;

    // ===============================================================
    // Branch functions

    template <class L>
    inline void branch32(Condition cond, Register lhs, Register rhs, L label) PER_SHARED_ARCH;
    template <class L>
    inline void branch32(Condition cond, Register lhs, Imm32 rhs, L label) PER_SHARED_ARCH;
    inline void branch32(Condition cond, Register length, const RegisterOrInt32Constant& key,
                         Label* label);

    inline void branch32(Condition cond, const Address& lhs, Register rhs, Label* label) PER_SHARED_ARCH;
    inline void branch32(Condition cond, const Address& lhs, Imm32 rhs, Label* label) PER_SHARED_ARCH;
    inline void branch32(Condition cond, const Address& length, const RegisterOrInt32Constant& key,
                         Label* label);

    inline void branch32(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);
    inline void branch32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);

    inline void branch32(Condition cond, const BaseIndex& lhs, Register rhs, Label* label)
        DEFINED_ON(x86_shared);
    inline void branch32(Condition cond, const BaseIndex& lhs, Imm32 rhs, Label* label) PER_SHARED_ARCH;

    inline void branch32(Condition cond, const Operand& lhs, Register rhs, Label* label) DEFINED_ON(x86_shared);
    inline void branch32(Condition cond, const Operand& lhs, Imm32 rhs, Label* label) DEFINED_ON(x86_shared);

    inline void branch32(Condition cond, wasm::SymbolicAddress lhs, Imm32 rhs, Label* label)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);

    // The supported condition are Equal, NotEqual, LessThan(orEqual), GreaterThan(orEqual),
    // Below(orEqual) and Above(orEqual).
    // When a fail label is not defined it will fall through to next instruction,
    // else jump to the fail label.
    inline void branch64(Condition cond, Register64 lhs, Imm64 val, Label* success,
                         Label* fail = nullptr) PER_ARCH;
    inline void branch64(Condition cond, Register64 lhs, Register64 rhs, Label* success,
                         Label* fail = nullptr) PER_ARCH;
    // On x86 and x64 NotEqual and Equal conditions are allowed for the branch64 variants
    // with Address as lhs. On others only the NotEqual condition.
    inline void branch64(Condition cond, const Address& lhs, Imm64 val, Label* label) PER_ARCH;

    // Compare the value at |lhs| with the value at |rhs|.  The scratch
    // register *must not* be the base of |lhs| or |rhs|.
    inline void branch64(Condition cond, const Address& lhs, const Address& rhs, Register scratch,
                         Label* label) PER_ARCH;

    inline void branchPtr(Condition cond, Register lhs, Register rhs, Label* label) PER_SHARED_ARCH;
    inline void branchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label) PER_SHARED_ARCH;
    inline void branchPtr(Condition cond, Register lhs, ImmPtr rhs, Label* label) PER_SHARED_ARCH;
    inline void branchPtr(Condition cond, Register lhs, ImmGCPtr rhs, Label* label) PER_SHARED_ARCH;
    inline void branchPtr(Condition cond, Register lhs, ImmWord rhs, Label* label) PER_SHARED_ARCH;

    template <class L>
    inline void branchPtr(Condition cond, const Address& lhs, Register rhs, L label) PER_SHARED_ARCH;
    inline void branchPtr(Condition cond, const Address& lhs, ImmPtr rhs, Label* label) PER_SHARED_ARCH;
    inline void branchPtr(Condition cond, const Address& lhs, ImmGCPtr rhs, Label* label) PER_SHARED_ARCH;
    inline void branchPtr(Condition cond, const Address& lhs, ImmWord rhs, Label* label) PER_SHARED_ARCH;

    inline void branchPtr(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);
    inline void branchPtr(Condition cond, const AbsoluteAddress& lhs, ImmWord rhs, Label* label)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);

    inline void branchPtr(Condition cond, wasm::SymbolicAddress lhs, Register rhs, Label* label)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);

    template <typename T>
    inline CodeOffsetJump branchPtrWithPatch(Condition cond, Register lhs, T rhs, RepatchLabel* label) PER_SHARED_ARCH;
    template <typename T>
    inline CodeOffsetJump branchPtrWithPatch(Condition cond, Address lhs, T rhs, RepatchLabel* label) PER_SHARED_ARCH;

    void branchPtrInNurseryChunk(Condition cond, Register ptr, Register temp, Label* label)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);
    void branchPtrInNurseryChunk(Condition cond, const Address& address, Register temp, Label* label)
        DEFINED_ON(x86);
    void branchValueIsNurseryObject(Condition cond, const Address& address, Register temp, Label* label) PER_ARCH;
    void branchValueIsNurseryObject(Condition cond, ValueOperand value, Register temp, Label* label) PER_ARCH;

    // This function compares a Value (lhs) which is having a private pointer
    // boxed inside a js::Value, with a raw pointer (rhs).
    inline void branchPrivatePtr(Condition cond, const Address& lhs, Register rhs, Label* label) PER_ARCH;

    inline void branchFloat(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                            Label* label) PER_SHARED_ARCH;

    // Truncate a double/float32 to int32 and when it doesn't fit an int32 it will jump to
    // the failure label. This particular variant is allowed to return the value module 2**32,
    // which isn't implemented on all architectures.
    // E.g. the x64 variants will do this only in the int64_t range.
    inline void branchTruncateFloat32MaybeModUint32(FloatRegister src, Register dest, Label* fail)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);
    inline void branchTruncateDoubleMaybeModUint32(FloatRegister src, Register dest, Label* fail)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);

    // Truncate a double/float32 to intptr and when it doesn't fit jump to the failure label.
    inline void branchTruncateFloat32ToPtr(FloatRegister src, Register dest, Label* fail)
        DEFINED_ON(x86, x64);
    inline void branchTruncateDoubleToPtr(FloatRegister src, Register dest, Label* fail)
        DEFINED_ON(x86, x64);

    // Truncate a double/float32 to int32 and when it doesn't fit jump to the failure label.
    inline void branchTruncateFloat32ToInt32(FloatRegister src, Register dest, Label* fail)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);
    inline void branchTruncateDoubleToInt32(FloatRegister src, Register dest, Label* fail)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);

    inline void branchDouble(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                             Label* label) PER_SHARED_ARCH;

    inline void branchDoubleNotInInt64Range(Address src, Register temp, Label* fail);
    inline void branchDoubleNotInUInt64Range(Address src, Register temp, Label* fail);
    inline void branchFloat32NotInInt64Range(Address src, Register temp, Label* fail);
    inline void branchFloat32NotInUInt64Range(Address src, Register temp, Label* fail);

    template <typename T, typename L>
    inline void branchAdd32(Condition cond, T src, Register dest, L label) PER_SHARED_ARCH;
    template <typename T>
    inline void branchSub32(Condition cond, T src, Register dest, Label* label) PER_SHARED_ARCH;

    inline void decBranchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label) PER_SHARED_ARCH;

    template <class L>
    inline void branchTest32(Condition cond, Register lhs, Register rhs, L label) PER_SHARED_ARCH;
    template <class L>
    inline void branchTest32(Condition cond, Register lhs, Imm32 rhs, L label) PER_SHARED_ARCH;
    inline void branchTest32(Condition cond, const Address& lhs, Imm32 rhh, Label* label) PER_SHARED_ARCH;
    inline void branchTest32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
        DEFINED_ON(arm, arm64, mips_shared, x86, x64);

    template <class L>
    inline void branchTestPtr(Condition cond, Register lhs, Register rhs, L label) PER_SHARED_ARCH;
    inline void branchTestPtr(Condition cond, Register lhs, Imm32 rhs, Label* label) PER_SHARED_ARCH;
    inline void branchTestPtr(Condition cond, const Address& lhs, Imm32 rhs, Label* label) PER_SHARED_ARCH;

    template <class L>
    inline void branchTest64(Condition cond, Register64 lhs, Register64 rhs, Register temp,
                             L label) PER_ARCH;

    // Branches to |label| if |reg| is false. |reg| should be a C++ bool.
    template <class L>
    inline void branchIfFalseBool(Register reg, L label);

    // Branches to |label| if |reg| is true. |reg| should be a C++ bool.
    inline void branchIfTrueBool(Register reg, Label* label);

    inline void branchIfRope(Register str, Label* label);

    inline void branchLatin1String(Register string, Label* label);
    inline void branchTwoByteString(Register string, Label* label);

    inline void branchIfFunctionHasNoScript(Register fun, Label* label);
    inline void branchIfInterpreted(Register fun, Label* label);

    inline void branchFunctionKind(Condition cond, JSFunction::FunctionKind kind, Register fun,
                                   Register scratch, Label* label);

    void branchIfNotInterpretedConstructor(Register fun, Register scratch, Label* label);

    inline void branchTestObjClass(Condition cond, Register obj, Register scratch, const js::Class* clasp,
                                   Label* label);
    inline void branchTestObjShape(Condition cond, Register obj, const Shape* shape, Label* label);
    inline void branchTestObjShape(Condition cond, Register obj, Register shape, Label* label);
    inline void branchTestObjGroup(Condition cond, Register obj, ObjectGroup* group, Label* label);
    inline void branchTestObjGroup(Condition cond, Register obj, Register group, Label* label);

    inline void branchTestObjectTruthy(bool truthy, Register objReg, Register scratch,
                                       Label* slowCheck, Label* checked);

    inline void branchTestClassIsProxy(bool proxy, Register clasp, Label* label);

    inline void branchTestObjectIsProxy(bool proxy, Register object, Register scratch, Label* label);

    inline void branchTestProxyHandlerFamily(Condition cond, Register proxy, Register scratch,
                                             const void* handlerp, Label* label);

    template <typename Value>
    inline void branchTestMIRType(Condition cond, const Value& val, MIRType type, Label* label);

    // Emit type case branch on tag matching if the type tag in the definition
    // might actually be that type.
    void maybeBranchTestType(MIRType type, MDefinition* maybeDef, Register tag, Label* label);

    inline void branchTestNeedsIncrementalBarrier(Condition cond, Label* label);

    // Perform a type-test on a tag of a Value (32bits boxing), or the tagged
    // value (64bits boxing).
    inline void branchTestUndefined(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestInt32(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestDouble(Condition cond, Register tag, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);
    inline void branchTestNumber(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestBoolean(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestString(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestSymbol(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestNull(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestObject(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestPrimitive(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;
    inline void branchTestMagic(Condition cond, Register tag, Label* label) PER_SHARED_ARCH;

    // Perform a type-test on a Value, addressed by Address or BaseIndex, or
    // loaded into ValueOperand.
    // BaseIndex and ValueOperand variants clobber the ScratchReg on x64.
    // All Variants clobber the ScratchReg on arm64.
    inline void branchTestUndefined(Condition cond, const Address& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestUndefined(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestUndefined(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestInt32(Condition cond, const Address& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestInt32(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestInt32(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestDouble(Condition cond, const Address& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestDouble(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestDouble(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestNumber(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestBoolean(Condition cond, const Address& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestBoolean(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestBoolean(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestString(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestString(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestSymbol(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestSymbol(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestNull(Condition cond, const Address& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestNull(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestNull(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    // Clobbers the ScratchReg on x64.
    inline void branchTestObject(Condition cond, const Address& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestObject(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestObject(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestGCThing(Condition cond, const Address& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestGCThing(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;

    inline void branchTestPrimitive(Condition cond, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestMagic(Condition cond, const Address& address, Label* label) PER_SHARED_ARCH;
    inline void branchTestMagic(Condition cond, const BaseIndex& address, Label* label) PER_SHARED_ARCH;
    template <class L>
    inline void branchTestMagic(Condition cond, const ValueOperand& value, L label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

    inline void branchTestMagic(Condition cond, const Address& valaddr, JSWhyMagic why, Label* label) PER_ARCH;

    inline void branchTestMagicValue(Condition cond, const ValueOperand& val, JSWhyMagic why,
                                     Label* label);

    void branchTestValue(Condition cond, const ValueOperand& lhs,
                         const Value& rhs, Label* label) PER_ARCH;

    // Checks if given Value is evaluated to true or false in a condition.
    // The type of the value should match the type of the method.
    inline void branchTestInt32Truthy(bool truthy, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);
    inline void branchTestDoubleTruthy(bool truthy, FloatRegister reg, Label* label) PER_SHARED_ARCH;
    inline void branchTestBooleanTruthy(bool truthy, const ValueOperand& value, Label* label) PER_ARCH;
    inline void branchTestStringTruthy(bool truthy, const ValueOperand& value, Label* label)
        DEFINED_ON(arm, arm64, mips32, mips64, x86_shared);

  private:

    // Implementation for branch* methods.
    template <typename T>
    inline void branch32Impl(Condition cond, const T& length, const RegisterOrInt32Constant& key,
                             Label* label);

    template <typename T, typename S, typename L>
    inline void branchPtrImpl(Condition cond, const T& lhs, const S& rhs, L label)
        DEFINED_ON(x86_shared);

    void branchPtrInNurseryChunkImpl(Condition cond, Register ptr, Label* label)
        DEFINED_ON(x86);
    template <typename T>
    void branchValueIsNurseryObjectImpl(Condition cond, const T& value, Register temp, Label* label)
        DEFINED_ON(arm64, mips64, x64);

    template <typename T>
    inline void branchTestUndefinedImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestInt32Impl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestDoubleImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestNumberImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestBooleanImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestStringImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestSymbolImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestNullImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestObjectImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestGCThingImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T>
    inline void branchTestPrimitiveImpl(Condition cond, const T& t, Label* label)
        DEFINED_ON(arm, arm64, x86_shared);
    template <typename T, class L>
    inline void branchTestMagicImpl(Condition cond, const T& t, L label)
        DEFINED_ON(arm, arm64, x86_shared);

  public:
    // ========================================================================
    // Canonicalization primitives.
    inline void canonicalizeDouble(FloatRegister reg);
    inline void canonicalizeDoubleIfDeterministic(FloatRegister reg);

    inline void canonicalizeFloat(FloatRegister reg);
    inline void canonicalizeFloatIfDeterministic(FloatRegister reg);

    inline void canonicalizeFloat32x4(FloatRegister reg, FloatRegister scratch)
        DEFINED_ON(x86_shared);

  public:
    // ========================================================================
    // Memory access primitives.
    inline void storeUncanonicalizedDouble(FloatRegister src, const Address& dest)
        DEFINED_ON(x86_shared, arm, arm64, mips32, mips64);
    inline void storeUncanonicalizedDouble(FloatRegister src, const BaseIndex& dest)
        DEFINED_ON(x86_shared, arm, arm64, mips32, mips64);
    inline void storeUncanonicalizedDouble(FloatRegister src, const Operand& dest)
        DEFINED_ON(x86_shared);

    template<class T>
    inline void storeDouble(FloatRegister src, const T& dest);

    inline void storeUncanonicalizedFloat32(FloatRegister src, const Address& dest)
        DEFINED_ON(x86_shared, arm, arm64, mips32, mips64);
    inline void storeUncanonicalizedFloat32(FloatRegister src, const BaseIndex& dest)
        DEFINED_ON(x86_shared, arm, arm64, mips32, mips64);
    inline void storeUncanonicalizedFloat32(FloatRegister src, const Operand& dest)
        DEFINED_ON(x86_shared);

    template<class T>
    inline void storeFloat32(FloatRegister src, const T& dest);

    inline void storeFloat32x3(FloatRegister src, const Address& dest) PER_SHARED_ARCH;
    inline void storeFloat32x3(FloatRegister src, const BaseIndex& dest) PER_SHARED_ARCH;

    template <typename T>
    void storeUnboxedValue(const ConstantOrRegister& value, MIRType valueType, const T& dest,
                           MIRType slotType) PER_ARCH;

    inline void memoryBarrier(MemoryBarrierBits barrier) PER_SHARED_ARCH;

  public:
    // ========================================================================
    // Truncate floating point.

    // Undefined behaviour when truncation is outside Int64 range.
    // Needs a temp register if SSE3 is not present.
    inline void truncateFloat32ToInt64(Address src, Address dest, Register temp)
        DEFINED_ON(x86_shared);
    inline void truncateFloat32ToUInt64(Address src, Address dest, Register temp,
                                        FloatRegister floatTemp)
        DEFINED_ON(x86, x64);
    inline void truncateDoubleToInt64(Address src, Address dest, Register temp)
        DEFINED_ON(x86_shared);
    inline void truncateDoubleToUInt64(Address src, Address dest, Register temp,
                                       FloatRegister floatTemp)
        DEFINED_ON(x86, x64);

  public:
    // ========================================================================
    // wasm support

    // Emit a bounds check against the (dynamically-patched) wasm bounds check
    // limit, jumping to 'label' if 'cond' holds.
    template <class L>
    inline void wasmBoundsCheck(Condition cond, Register index, L label) PER_ARCH;

    // Called after compilation completes to patch the given limit into the
    // given instruction's immediate.
    static inline void wasmPatchBoundsCheck(uint8_t* patchAt, uint32_t limit) PER_ARCH;

    // On x86, each instruction adds its own wasm::MemoryAccess's to the
    // wasm::MemoryAccessVector (there can be multiple when i64 is involved).
    // On x64, only some asm.js accesses need a wasm::MemoryAccess so the caller
    // is responsible for doing this instead.
    void wasmLoad(const wasm::MemoryAccessDesc& access, Operand srcAddr, AnyRegister out) DEFINED_ON(x86, x64);
    void wasmLoadI64(const wasm::MemoryAccessDesc& access, Operand srcAddr, Register64 out) DEFINED_ON(x86, x64);
    void wasmStore(const wasm::MemoryAccessDesc& access, AnyRegister value, Operand dstAddr) DEFINED_ON(x86, x64);
    void wasmStoreI64(const wasm::MemoryAccessDesc& access, Register64 value, Operand dstAddr) DEFINED_ON(x86);

    // wasm specific methods, used in both the wasm baseline compiler and ion.
    void wasmTruncateDoubleToUInt32(FloatRegister input, Register output, Label* oolEntry) DEFINED_ON(x86, x64, arm);
    void wasmTruncateDoubleToInt32(FloatRegister input, Register output, Label* oolEntry) DEFINED_ON(x86_shared, arm);
    void outOfLineWasmTruncateDoubleToInt32(FloatRegister input, bool isUnsigned, wasm::TrapOffset off, Label* rejoin) DEFINED_ON(x86_shared);

    void wasmTruncateFloat32ToUInt32(FloatRegister input, Register output, Label* oolEntry) DEFINED_ON(x86, x64, arm);
    void wasmTruncateFloat32ToInt32(FloatRegister input, Register output, Label* oolEntry) DEFINED_ON(x86_shared, arm);
    void outOfLineWasmTruncateFloat32ToInt32(FloatRegister input, bool isUnsigned, wasm::TrapOffset off, Label* rejoin) DEFINED_ON(x86_shared);

    void outOfLineWasmTruncateDoubleToInt64(FloatRegister input, bool isUnsigned, wasm::TrapOffset off, Label* rejoin) DEFINED_ON(x86_shared);
    void outOfLineWasmTruncateFloat32ToInt64(FloatRegister input, bool isUnsigned, wasm::TrapOffset off, Label* rejoin) DEFINED_ON(x86_shared);

    // This function takes care of loading the callee's TLS and pinned regs but
    // it is the caller's responsibility to save/restore TLS or pinned regs.
    void wasmCallImport(const wasm::CallSiteDesc& desc, const wasm::CalleeDesc& callee);

    // WasmTableCallIndexReg must contain the index of the indirect call.
    void wasmCallIndirect(const wasm::CallSiteDesc& desc, const wasm::CalleeDesc& callee);

    // This function takes care of loading the pointer to the current instance
    // as the implicit first argument. It preserves TLS and pinned registers.
    // (TLS & pinned regs are non-volatile registers in the system ABI).
    void wasmCallBuiltinInstanceMethod(const ABIArg& instanceArg,
                                       wasm::SymbolicAddress builtin);

    // Emit the out-of-line trap code to which trapping jumps/branches are
    // bound. This should be called once per function after all other codegen,
    // including "normal" OutOfLineCode.
    void wasmEmitTrapOutOfLineCode();

  public:
    // ========================================================================
    // Clamping functions.

    inline void clampIntToUint8(Register reg) PER_SHARED_ARCH;

    //}}} check_macroassembler_style
  public:

    // Emits a test of a value against all types in a TypeSet. A scratch
    // register is required.
    template <typename Source>
    void guardTypeSet(const Source& address, const TypeSet* types, BarrierKind kind, Register scratch, Label* miss);

    void guardObjectType(Register obj, const TypeSet* types, Register scratch, Label* miss);

    template <typename TypeSet>
    void guardTypeSetMightBeIncomplete(TypeSet* types, Register obj, Register scratch, Label* label);

    void loadObjShape(Register objReg, Register dest) {
        loadPtr(Address(objReg, ShapedObject::offsetOfShape()), dest);
    }
    void loadObjGroup(Register objReg, Register dest) {
        loadPtr(Address(objReg, JSObject::offsetOfGroup()), dest);
    }
    void loadBaseShape(Register objReg, Register dest) {
        loadObjShape(objReg, dest);
        loadPtr(Address(dest, Shape::offsetOfBase()), dest);
    }
    void loadObjClass(Register objReg, Register dest) {
        loadObjGroup(objReg, dest);
        loadPtr(Address(dest, ObjectGroup::offsetOfClasp()), dest);
    }

    void loadObjPrivate(Register obj, uint32_t nfixed, Register dest) {
        loadPtr(Address(obj, NativeObject::getPrivateDataOffset(nfixed)), dest);
    }

    void loadObjProto(Register obj, Register dest) {
        loadPtr(Address(obj, JSObject::offsetOfGroup()), dest);
        loadPtr(Address(dest, ObjectGroup::offsetOfProto()), dest);
    }

    void loadStringLength(Register str, Register dest) {
        load32(Address(str, JSString::offsetOfLength()), dest);
    }

    void loadStringChars(Register str, Register dest);
    void loadStringChar(Register str, Register index, Register output);

    void loadJSContext(Register dest) {
        movePtr(ImmPtr(GetJitContext()->runtime->getJSContext()), dest);
    }
    void loadJitActivation(Register dest) {
        loadPtr(AbsoluteAddress(GetJitContext()->runtime->addressOfActivation()), dest);
    }
    void loadWasmActivationFromTls(Register dest) {
        loadPtr(Address(WasmTlsReg, offsetof(wasm::TlsData, cx)), dest);
        loadPtr(Address(dest, JSContext::offsetOfWasmActivation()), dest);
    }
    void loadWasmActivationFromSymbolicAddress(Register dest) {
        movePtr(wasm::SymbolicAddress::Context, dest);
        loadPtr(Address(dest, JSContext::offsetOfWasmActivation()), dest);
    }

    template<typename T>
    void loadTypedOrValue(const T& src, TypedOrValueRegister dest) {
        if (dest.hasValue())
            loadValue(src, dest.valueReg());
        else
            loadUnboxedValue(src, dest.type(), dest.typedReg());
    }

    template<typename T>
    void loadElementTypedOrValue(const T& src, TypedOrValueRegister dest, bool holeCheck,
                                 Label* hole) {
        if (dest.hasValue()) {
            loadValue(src, dest.valueReg());
            if (holeCheck)
                branchTestMagic(Assembler::Equal, dest.valueReg(), hole);
        } else {
            if (holeCheck)
                branchTestMagic(Assembler::Equal, src, hole);
            loadUnboxedValue(src, dest.type(), dest.typedReg());
        }
    }

    template <typename T>
    void storeTypedOrValue(TypedOrValueRegister src, const T& dest) {
        if (src.hasValue()) {
            storeValue(src.valueReg(), dest);
        } else if (IsFloatingPointType(src.type())) {
            FloatRegister reg = src.typedReg().fpu();
            if (src.type() == MIRType::Float32) {
                convertFloat32ToDouble(reg, ScratchDoubleReg);
                reg = ScratchDoubleReg;
            }
            storeDouble(reg, dest);
        } else {
            storeValue(ValueTypeFromMIRType(src.type()), src.typedReg().gpr(), dest);
        }
    }

    template <typename T>
    inline void storeObjectOrNull(Register src, const T& dest);

    template <typename T>
    void storeConstantOrRegister(const ConstantOrRegister& src, const T& dest) {
        if (src.constant())
            storeValue(src.value(), dest);
        else
            storeTypedOrValue(src.reg(), dest);
    }

    void storeCallResult(Register reg) {
        if (reg != ReturnReg)
            mov(ReturnReg, reg);
    }

    void storeCallFloatResult(FloatRegister reg) {
        if (reg != ReturnDoubleReg)
            moveDouble(ReturnDoubleReg, reg);
    }

    inline void storeCallResultValue(AnyRegister dest);

    void storeCallResultValue(ValueOperand dest) {
#if defined(JS_NUNBOX32)
        // reshuffle the return registers used for a call result to store into
        // dest, using ReturnReg as a scratch register if necessary. This must
        // only be called after returning from a call, at a point when the
        // return register is not live. XXX would be better to allow wrappers
        // to store the return value to different places.
        if (dest.typeReg() == JSReturnReg_Data) {
            if (dest.payloadReg() == JSReturnReg_Type) {
                // swap the two registers.
                mov(JSReturnReg_Type, ReturnReg);
                mov(JSReturnReg_Data, JSReturnReg_Type);
                mov(ReturnReg, JSReturnReg_Data);
            } else {
                mov(JSReturnReg_Data, dest.payloadReg());
                mov(JSReturnReg_Type, dest.typeReg());
            }
        } else {
            mov(JSReturnReg_Type, dest.typeReg());
            mov(JSReturnReg_Data, dest.payloadReg());
        }
#elif defined(JS_PUNBOX64)
        if (dest.valueReg() != JSReturnReg)
            mov(JSReturnReg, dest.valueReg());
#else
#error "Bad architecture"
#endif
    }

    inline void storeCallResultValue(TypedOrValueRegister dest);

    template <typename T>
    Register extractString(const T& source, Register scratch) {
        return extractObject(source, scratch);
    }
    using MacroAssemblerSpecific::store32;
    void store32(const RegisterOrInt32Constant& key, const Address& dest) {
        if (key.isRegister())
            store32(key.reg(), dest);
        else
            store32(Imm32(key.constant()), dest);
    }

    template <typename T>
    void callPreBarrier(const T& address, MIRType type) {
        Label done;

        if (type == MIRType::Value)
            branchTestGCThing(Assembler::NotEqual, address, &done);

        Push(PreBarrierReg);
        computeEffectiveAddress(address, PreBarrierReg);

        const JitRuntime* rt = GetJitContext()->runtime->jitRuntime();
        JitCode* preBarrier = rt->preBarrier(type);

        call(preBarrier);
        Pop(PreBarrierReg);

        bind(&done);
    }

    template <typename T>
    void patchableCallPreBarrier(const T& address, MIRType type) {
        Label done;

        // All barriers are off by default.
        // They are enabled if necessary at the end of CodeGenerator::generate().
        CodeOffset nopJump = toggledJump(&done);
        writePrebarrierOffset(nopJump);

        callPreBarrier(address, type);
        jump(&done);

        haltingAlign(8);
        bind(&done);
    }

    template<typename T>
    void loadFromTypedArray(Scalar::Type arrayType, const T& src, AnyRegister dest, Register temp, Label* fail,
                            bool canonicalizeDoubles = true, unsigned numElems = 0);

    template<typename T>
    void loadFromTypedArray(Scalar::Type arrayType, const T& src, const ValueOperand& dest, bool allowDouble,
                            Register temp, Label* fail);

    template<typename S, typename T>
    void storeToTypedIntArray(Scalar::Type arrayType, const S& value, const T& dest) {
        switch (arrayType) {
          case Scalar::Int8:
          case Scalar::Uint8:
          case Scalar::Uint8Clamped:
            store8(value, dest);
            break;
          case Scalar::Int16:
          case Scalar::Uint16:
            store16(value, dest);
            break;
          case Scalar::Int32:
          case Scalar::Uint32:
            store32(value, dest);
            break;
          default:
            MOZ_CRASH("Invalid typed array type");
        }
    }

    void storeToTypedFloatArray(Scalar::Type arrayType, FloatRegister value, const BaseIndex& dest,
                                unsigned numElems = 0);
    void storeToTypedFloatArray(Scalar::Type arrayType, FloatRegister value, const Address& dest,
                                unsigned numElems = 0);

    // Load a property from an UnboxedPlainObject or UnboxedArrayObject.
    template <typename T>
    void loadUnboxedProperty(T address, JSValueType type, TypedOrValueRegister output);

    // Store a property to an UnboxedPlainObject, without triggering barriers.
    // If failure is null, the value definitely has a type suitable for storing
    // in the property.
    template <typename T>
    void storeUnboxedProperty(T address, JSValueType type,
                              const ConstantOrRegister& value, Label* failure);

    void checkUnboxedArrayCapacity(Register obj, const RegisterOrInt32Constant& index,
                                   Register temp, Label* failure);

    Register extractString(const Address& address, Register scratch) {
        return extractObject(address, scratch);
    }
    Register extractString(const ValueOperand& value, Register scratch) {
        return extractObject(value, scratch);
    }

    using MacroAssemblerSpecific::extractTag;
    Register extractTag(const TypedOrValueRegister& reg, Register scratch) {
        if (reg.hasValue())
            return extractTag(reg.valueReg(), scratch);
        mov(ImmWord(MIRTypeToTag(reg.type())), scratch);
        return scratch;
    }

    using MacroAssemblerSpecific::extractObject;
    Register extractObject(const TypedOrValueRegister& reg, Register scratch) {
        if (reg.hasValue())
            return extractObject(reg.valueReg(), scratch);
        MOZ_ASSERT(reg.type() == MIRType::Object);
        return reg.typedReg().gpr();
    }

    // Inline version of js_TypedArray_uint8_clamp_double.
    // This function clobbers the input register.
    void clampDoubleToUint8(FloatRegister input, Register output) PER_ARCH;

    using MacroAssemblerSpecific::ensureDouble;

    template <typename S>
    void ensureDouble(const S& source, FloatRegister dest, Label* failure) {
        Label isDouble, done;
        branchTestDouble(Assembler::Equal, source, &isDouble);
        branchTestInt32(Assembler::NotEqual, source, failure);

        convertInt32ToDouble(source, dest);
        jump(&done);

        bind(&isDouble);
        unboxDouble(source, dest);

        bind(&done);
    }

    // Inline allocation.
  private:
    void checkAllocatorState(Label* fail);
    bool shouldNurseryAllocate(gc::AllocKind allocKind, gc::InitialHeap initialHeap);
    void nurseryAllocate(Register result, Register temp, gc::AllocKind allocKind,
                         size_t nDynamicSlots, gc::InitialHeap initialHeap, Label* fail);
    void freeListAllocate(Register result, Register temp, gc::AllocKind allocKind, Label* fail);
    void allocateObject(Register result, Register temp, gc::AllocKind allocKind,
                        uint32_t nDynamicSlots, gc::InitialHeap initialHeap, Label* fail);
    void allocateNonObject(Register result, Register temp, gc::AllocKind allocKind, Label* fail);
    void copySlotsFromTemplate(Register obj, const NativeObject* templateObj,
                               uint32_t start, uint32_t end);
    void fillSlotsWithConstantValue(Address addr, Register temp, uint32_t start, uint32_t end,
                                    const Value& v);
    void fillSlotsWithUndefined(Address addr, Register temp, uint32_t start, uint32_t end);
    void fillSlotsWithUninitialized(Address addr, Register temp, uint32_t start, uint32_t end);

    void initGCSlots(Register obj, Register temp, NativeObject* templateObj, bool initContents);

  public:
    void callMallocStub(size_t nbytes, Register result, Label* fail);
    void callFreeStub(Register slots);
    void createGCObject(Register result, Register temp, JSObject* templateObj,
                        gc::InitialHeap initialHeap, Label* fail, bool initContents = true,
                        bool convertDoubleElements = false);

    void initGCThing(Register obj, Register temp, JSObject* templateObj,
                     bool initContents = true, bool convertDoubleElements = false);
    void initTypedArraySlots(Register obj, Register temp, Register lengthReg,
                             LiveRegisterSet liveRegs, Label* fail,
                             TypedArrayObject* templateObj, TypedArrayLength lengthKind);

    void initUnboxedObjectContents(Register object, UnboxedPlainObject* templateObject);

    void newGCString(Register result, Register temp, Label* fail);
    void newGCFatInlineString(Register result, Register temp, Label* fail);

    // Compares two strings for equality based on the JSOP.
    // This checks for identical pointers, atoms and length and fails for everything else.
    void compareStrings(JSOp op, Register left, Register right, Register result,
                        Label* fail);

  public:
    // Generates code used to complete a bailout.
    void generateBailoutTail(Register scratch, Register bailoutInfo);

  public:
#ifndef JS_CODEGEN_ARM64
    // StackPointer manipulation functions.
    // On ARM64, the StackPointer is implemented as two synchronized registers.
    // Code shared across platforms must use these functions to be valid.
    template <typename T> inline void addToStackPtr(T t);
    template <typename T> inline void addStackPtrTo(T t);

    void subFromStackPtr(Imm32 imm32) DEFINED_ON(mips32, mips64, arm, x86, x64);
    void subFromStackPtr(Register reg);

    template <typename T>
    void subStackPtrFrom(T t) { subPtr(getStackPointer(), t); }

    template <typename T>
    void andToStackPtr(T t) { andPtr(t, getStackPointer()); }
    template <typename T>
    void andStackPtrTo(T t) { andPtr(getStackPointer(), t); }

    template <typename T>
    void moveToStackPtr(T t) { movePtr(t, getStackPointer()); }
    template <typename T>
    void moveStackPtrTo(T t) { movePtr(getStackPointer(), t); }

    template <typename T>
    void loadStackPtr(T t) { loadPtr(t, getStackPointer()); }
    template <typename T>
    void storeStackPtr(T t) { storePtr(getStackPointer(), t); }

    // StackPointer testing functions.
    // On ARM64, sp can function as the zero register depending on context.
    // Code shared across platforms must use these functions to be valid.
    template <typename T>
    inline void branchTestStackPtr(Condition cond, T t, Label* label);
    template <typename T>
    inline void branchStackPtr(Condition cond, T rhs, Label* label);
    template <typename T>
    inline void branchStackPtrRhs(Condition cond, T lhs, Label* label);

    // Move the stack pointer based on the requested amount.
    inline void reserveStack(uint32_t amount);
#else // !JS_CODEGEN_ARM64
    void reserveStack(uint32_t amount);
#endif

  public:
    void enableProfilingInstrumentation() {
        emitProfilingInstrumentation_ = true;
    }

  private:
    // This class is used to surround call sites throughout the assembler. This
    // is used by callWithABI, and callJit functions, except if suffixed by
    // NoProfiler.
    class AutoProfilerCallInstrumentation {
        MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;

      public:
        explicit AutoProfilerCallInstrumentation(MacroAssembler& masm
                                                 MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
        ~AutoProfilerCallInstrumentation() {}
    };
    friend class AutoProfilerCallInstrumentation;

    void appendProfilerCallSite(CodeOffset label) {
        propagateOOM(profilerCallSites_.append(label));
    }

    // Fix up the code pointers to be written for locations where profilerCallSite
    // emitted moves of RIP to a register.
    void linkProfilerCallSites(JitCode* code);

    // This field is used to manage profiling instrumentation output. If
    // provided and enabled, then instrumentation will be emitted around call
    // sites.
    bool emitProfilingInstrumentation_;

    // Record locations of the call sites.
    Vector<CodeOffset, 0, SystemAllocPolicy> profilerCallSites_;

  public:
    void loadBaselineOrIonRaw(Register script, Register dest, Label* failure);
    void loadBaselineOrIonNoArgCheck(Register callee, Register dest, Label* failure);

    void loadBaselineFramePtr(Register framePtr, Register dest);

    void pushBaselineFramePtr(Register framePtr, Register scratch) {
        loadBaselineFramePtr(framePtr, scratch);
        push(scratch);
    }

    void PushBaselineFramePtr(Register framePtr, Register scratch) {
        loadBaselineFramePtr(framePtr, scratch);
        Push(scratch);
    }

  private:
    void handleFailure();

  public:
    Label* exceptionLabel() {
        // Exceptions are currently handled the same way as sequential failures.
        return &failureLabel_;
    }

    Label* failureLabel() {
        return &failureLabel_;
    }

    void finish();
    void link(JitCode* code);

    void assumeUnreachable(const char* output);

    template<typename T>
    void assertTestInt32(Condition cond, const T& value, const char* output);

    void printf(const char* output);
    void printf(const char* output, Register value);

#ifdef JS_TRACE_LOGGING
    void tracelogStartId(Register logger, uint32_t textId, bool force = false);
    void tracelogStartId(Register logger, Register textId);
    void tracelogStartEvent(Register logger, Register event);
    void tracelogStopId(Register logger, uint32_t textId, bool force = false);
    void tracelogStopId(Register logger, Register textId);
#endif

#define DISPATCH_FLOATING_POINT_OP(method, type, arg1d, arg1f, arg2)    \
    MOZ_ASSERT(IsFloatingPointType(type));                              \
    if (type == MIRType::Double)                                        \
        method##Double(arg1d, arg2);                                    \
    else                                                                \
        method##Float32(arg1f, arg2);                                   \

    void loadConstantFloatingPoint(double d, float f, FloatRegister dest, MIRType destType) {
        DISPATCH_FLOATING_POINT_OP(loadConstant, destType, d, f, dest);
    }
    void boolValueToFloatingPoint(ValueOperand value, FloatRegister dest, MIRType destType) {
        DISPATCH_FLOATING_POINT_OP(boolValueTo, destType, value, value, dest);
    }
    void int32ValueToFloatingPoint(ValueOperand value, FloatRegister dest, MIRType destType) {
        DISPATCH_FLOATING_POINT_OP(int32ValueTo, destType, value, value, dest);
    }
    void convertInt32ToFloatingPoint(Register src, FloatRegister dest, MIRType destType) {
        DISPATCH_FLOATING_POINT_OP(convertInt32To, destType, src, src, dest);
    }

#undef DISPATCH_FLOATING_POINT_OP

    void convertValueToFloatingPoint(ValueOperand value, FloatRegister output, Label* fail,
                                     MIRType outputType);
    MOZ_MUST_USE bool convertValueToFloatingPoint(JSContext* cx, const Value& v,
                                                  FloatRegister output, Label* fail,
                                                  MIRType outputType);
    MOZ_MUST_USE bool convertConstantOrRegisterToFloatingPoint(JSContext* cx,
                                                               const ConstantOrRegister& src,
                                                               FloatRegister output, Label* fail,
                                                               MIRType outputType);
    void convertTypedOrValueToFloatingPoint(TypedOrValueRegister src, FloatRegister output,
                                            Label* fail, MIRType outputType);

    void outOfLineTruncateSlow(FloatRegister src, Register dest, bool widenFloatToDouble,
                               bool compilingWasm);

    void convertInt32ValueToDouble(const Address& address, Register scratch, Label* done);
    void convertValueToDouble(ValueOperand value, FloatRegister output, Label* fail) {
        convertValueToFloatingPoint(value, output, fail, MIRType::Double);
    }
    MOZ_MUST_USE bool convertValueToDouble(JSContext* cx, const Value& v, FloatRegister output,
                                           Label* fail) {
        return convertValueToFloatingPoint(cx, v, output, fail, MIRType::Double);
    }
    MOZ_MUST_USE bool convertConstantOrRegisterToDouble(JSContext* cx,
                                                        const ConstantOrRegister& src,
                                                        FloatRegister output, Label* fail)
    {
        return convertConstantOrRegisterToFloatingPoint(cx, src, output, fail, MIRType::Double);
    }
    void convertTypedOrValueToDouble(TypedOrValueRegister src, FloatRegister output, Label* fail) {
        convertTypedOrValueToFloatingPoint(src, output, fail, MIRType::Double);
    }

    void convertValueToFloat(ValueOperand value, FloatRegister output, Label* fail) {
        convertValueToFloatingPoint(value, output, fail, MIRType::Float32);
    }
    MOZ_MUST_USE bool convertValueToFloat(JSContext* cx, const Value& v, FloatRegister output,
                                          Label* fail) {
        return convertValueToFloatingPoint(cx, v, output, fail, MIRType::Float32);
    }
    MOZ_MUST_USE bool convertConstantOrRegisterToFloat(JSContext* cx,
                                                       const ConstantOrRegister& src,
                                                       FloatRegister output, Label* fail)
    {
        return convertConstantOrRegisterToFloatingPoint(cx, src, output, fail, MIRType::Float32);
    }
    void convertTypedOrValueToFloat(TypedOrValueRegister src, FloatRegister output, Label* fail) {
        convertTypedOrValueToFloatingPoint(src, output, fail, MIRType::Float32);
    }

    enum IntConversionBehavior {
        IntConversion_Normal,
        IntConversion_NegativeZeroCheck,
        IntConversion_Truncate,
        IntConversion_ClampToUint8,
    };

    enum IntConversionInputKind {
        IntConversion_NumbersOnly,
        IntConversion_NumbersOrBoolsOnly,
        IntConversion_Any
    };

    //
    // Functions for converting values to int.
    //
    void convertDoubleToInt(FloatRegister src, Register output, FloatRegister temp,
                            Label* truncateFail, Label* fail, IntConversionBehavior behavior);

    // Strings may be handled by providing labels to jump to when the behavior
    // is truncation or clamping. The subroutine, usually an OOL call, is
    // passed the unboxed string in |stringReg| and should convert it to a
    // double store into |temp|.
    void convertValueToInt(ValueOperand value, MDefinition* input,
                           Label* handleStringEntry, Label* handleStringRejoin,
                           Label* truncateDoubleSlow,
                           Register stringReg, FloatRegister temp, Register output,
                           Label* fail, IntConversionBehavior behavior,
                           IntConversionInputKind conversion = IntConversion_Any);
    void convertValueToInt(ValueOperand value, FloatRegister temp, Register output, Label* fail,
                           IntConversionBehavior behavior)
    {
        convertValueToInt(value, nullptr, nullptr, nullptr, nullptr, InvalidReg, temp, output,
                          fail, behavior);
    }
    MOZ_MUST_USE bool convertValueToInt(JSContext* cx, const Value& v, Register output, Label* fail,
                                        IntConversionBehavior behavior);
    MOZ_MUST_USE bool convertConstantOrRegisterToInt(JSContext* cx,
                                                     const ConstantOrRegister& src,
                                                     FloatRegister temp, Register output,
                                                     Label* fail, IntConversionBehavior behavior);
    void convertTypedOrValueToInt(TypedOrValueRegister src, FloatRegister temp, Register output,
                                  Label* fail, IntConversionBehavior behavior);

    //
    // Convenience functions for converting values to int32.
    //
    void convertValueToInt32(ValueOperand value, FloatRegister temp, Register output, Label* fail,
                             bool negativeZeroCheck)
    {
        convertValueToInt(value, temp, output, fail, negativeZeroCheck
                          ? IntConversion_NegativeZeroCheck
                          : IntConversion_Normal);
    }
    void convertValueToInt32(ValueOperand value, MDefinition* input,
                             FloatRegister temp, Register output, Label* fail,
                             bool negativeZeroCheck, IntConversionInputKind conversion = IntConversion_Any)
    {
        convertValueToInt(value, input, nullptr, nullptr, nullptr, InvalidReg, temp, output, fail,
                          negativeZeroCheck
                          ? IntConversion_NegativeZeroCheck
                          : IntConversion_Normal,
                          conversion);
    }
    MOZ_MUST_USE bool convertValueToInt32(JSContext* cx, const Value& v, Register output,
                                          Label* fail, bool negativeZeroCheck)
    {
        return convertValueToInt(cx, v, output, fail, negativeZeroCheck
                                 ? IntConversion_NegativeZeroCheck
                                 : IntConversion_Normal);
    }
    MOZ_MUST_USE bool convertConstantOrRegisterToInt32(JSContext* cx,
                                                       const ConstantOrRegister& src,
                                                       FloatRegister temp, Register output,
                                                       Label* fail, bool negativeZeroCheck)
    {
        return convertConstantOrRegisterToInt(cx, src, temp, output, fail, negativeZeroCheck
                                              ? IntConversion_NegativeZeroCheck
                                              : IntConversion_Normal);
    }
    void convertTypedOrValueToInt32(TypedOrValueRegister src, FloatRegister temp, Register output,
                                    Label* fail, bool negativeZeroCheck)
    {
        convertTypedOrValueToInt(src, temp, output, fail, negativeZeroCheck
                                 ? IntConversion_NegativeZeroCheck
                                 : IntConversion_Normal);
    }

    //
    // Convenience functions for truncating values to int32.
    //
    void truncateValueToInt32(ValueOperand value, FloatRegister temp, Register output, Label* fail) {
        convertValueToInt(value, temp, output, fail, IntConversion_Truncate);
    }
    void truncateValueToInt32(ValueOperand value, MDefinition* input,
                              Label* handleStringEntry, Label* handleStringRejoin,
                              Label* truncateDoubleSlow,
                              Register stringReg, FloatRegister temp, Register output, Label* fail)
    {
        convertValueToInt(value, input, handleStringEntry, handleStringRejoin, truncateDoubleSlow,
                          stringReg, temp, output, fail, IntConversion_Truncate);
    }
    void truncateValueToInt32(ValueOperand value, MDefinition* input,
                              FloatRegister temp, Register output, Label* fail)
    {
        convertValueToInt(value, input, nullptr, nullptr, nullptr, InvalidReg, temp, output, fail,
                          IntConversion_Truncate);
    }
    MOZ_MUST_USE bool truncateValueToInt32(JSContext* cx, const Value& v, Register output,
                                           Label* fail) {
        return convertValueToInt(cx, v, output, fail, IntConversion_Truncate);
    }
    MOZ_MUST_USE bool truncateConstantOrRegisterToInt32(JSContext* cx,
                                                        const ConstantOrRegister& src,
                                                        FloatRegister temp, Register output,
                                                        Label* fail)
    {
        return convertConstantOrRegisterToInt(cx, src, temp, output, fail, IntConversion_Truncate);
    }
    void truncateTypedOrValueToInt32(TypedOrValueRegister src, FloatRegister temp, Register output,
                                     Label* fail)
    {
        convertTypedOrValueToInt(src, temp, output, fail, IntConversion_Truncate);
    }

    // Convenience functions for clamping values to uint8.
    void clampValueToUint8(ValueOperand value, FloatRegister temp, Register output, Label* fail) {
        convertValueToInt(value, temp, output, fail, IntConversion_ClampToUint8);
    }
    void clampValueToUint8(ValueOperand value, MDefinition* input,
                           Label* handleStringEntry, Label* handleStringRejoin,
                           Register stringReg, FloatRegister temp, Register output, Label* fail)
    {
        convertValueToInt(value, input, handleStringEntry, handleStringRejoin, nullptr,
                          stringReg, temp, output, fail, IntConversion_ClampToUint8);
    }
    void clampValueToUint8(ValueOperand value, MDefinition* input,
                           FloatRegister temp, Register output, Label* fail)
    {
        convertValueToInt(value, input, nullptr, nullptr, nullptr, InvalidReg, temp, output, fail,
                          IntConversion_ClampToUint8);
    }
    MOZ_MUST_USE bool clampValueToUint8(JSContext* cx, const Value& v, Register output,
                                        Label* fail) {
        return convertValueToInt(cx, v, output, fail, IntConversion_ClampToUint8);
    }
    MOZ_MUST_USE bool clampConstantOrRegisterToUint8(JSContext* cx,
                                                     const ConstantOrRegister& src,
                                                     FloatRegister temp, Register output,
                                                     Label* fail)
    {
        return convertConstantOrRegisterToInt(cx, src, temp, output, fail,
                                              IntConversion_ClampToUint8);
    }
    void clampTypedOrValueToUint8(TypedOrValueRegister src, FloatRegister temp, Register output,
                                  Label* fail)
    {
        convertTypedOrValueToInt(src, temp, output, fail, IntConversion_ClampToUint8);
    }

  public:
    class AfterICSaveLive {
        friend class MacroAssembler;
        explicit AfterICSaveLive(uint32_t initialStack)
#ifdef JS_DEBUG
          : initialStack(initialStack)
#endif
        {}

      public:
#ifdef JS_DEBUG
        uint32_t initialStack;
#endif
        uint32_t alignmentPadding;
    };

    void alignFrameForICArguments(AfterICSaveLive& aic) PER_ARCH;
    void restoreFrameAlignmentForICArguments(AfterICSaveLive& aic) PER_ARCH;

    AfterICSaveLive icSaveLive(LiveRegisterSet& liveRegs);
    MOZ_MUST_USE bool icBuildOOLFakeExitFrame(void* fakeReturnAddr, AfterICSaveLive& aic);
    void icRestoreLive(LiveRegisterSet& liveRegs, AfterICSaveLive& aic);

    // Align the stack pointer based on the number of arguments which are pushed
    // on the stack, such that the JitFrameLayout would be correctly aligned on
    // the JitStackAlignment.
    void alignJitStackBasedOnNArgs(Register nargs);
    void alignJitStackBasedOnNArgs(uint32_t nargs);

    inline void assertStackAlignment(uint32_t alignment, int32_t offset = 0);
};

static inline Assembler::DoubleCondition
JSOpToDoubleCondition(JSOp op)
{
    switch (op) {
      case JSOP_EQ:
      case JSOP_STRICTEQ:
        return Assembler::DoubleEqual;
      case JSOP_NE:
      case JSOP_STRICTNE:
        return Assembler::DoubleNotEqualOrUnordered;
      case JSOP_LT:
        return Assembler::DoubleLessThan;
      case JSOP_LE:
        return Assembler::DoubleLessThanOrEqual;
      case JSOP_GT:
        return Assembler::DoubleGreaterThan;
      case JSOP_GE:
        return Assembler::DoubleGreaterThanOrEqual;
      default:
        MOZ_CRASH("Unexpected comparison operation");
    }
}

// Note: the op may have been inverted during lowering (to put constants in a
// position where they can be immediates), so it is important to use the
// lir->jsop() instead of the mir->jsop() when it is present.
static inline Assembler::Condition
JSOpToCondition(JSOp op, bool isSigned)
{
    if (isSigned) {
        switch (op) {
          case JSOP_EQ:
          case JSOP_STRICTEQ:
            return Assembler::Equal;
          case JSOP_NE:
          case JSOP_STRICTNE:
            return Assembler::NotEqual;
          case JSOP_LT:
            return Assembler::LessThan;
          case JSOP_LE:
            return Assembler::LessThanOrEqual;
          case JSOP_GT:
            return Assembler::GreaterThan;
          case JSOP_GE:
            return Assembler::GreaterThanOrEqual;
          default:
            MOZ_CRASH("Unrecognized comparison operation");
        }
    } else {
        switch (op) {
          case JSOP_EQ:
          case JSOP_STRICTEQ:
            return Assembler::Equal;
          case JSOP_NE:
          case JSOP_STRICTNE:
            return Assembler::NotEqual;
          case JSOP_LT:
            return Assembler::Below;
          case JSOP_LE:
            return Assembler::BelowOrEqual;
          case JSOP_GT:
            return Assembler::Above;
          case JSOP_GE:
            return Assembler::AboveOrEqual;
          default:
            MOZ_CRASH("Unrecognized comparison operation");
        }
    }
}

static inline size_t
StackDecrementForCall(uint32_t alignment, size_t bytesAlreadyPushed, size_t bytesToPush)
{
    return bytesToPush +
           ComputeByteAlignment(bytesAlreadyPushed + bytesToPush, alignment);
}

static inline MIRType
ToMIRType(MIRType t)
{
    return t;
}

template <class VecT>
class ABIArgIter
{
    ABIArgGenerator gen_;
    const VecT& types_;
    unsigned i_;

    void settle() { if (!done()) gen_.next(ToMIRType(types_[i_])); }

  public:
    explicit ABIArgIter(const VecT& types) : types_(types), i_(0) { settle(); }
    void operator++(int) { MOZ_ASSERT(!done()); i_++; settle(); }
    bool done() const { return i_ == types_.length(); }

    ABIArg* operator->() { MOZ_ASSERT(!done()); return &gen_.current(); }
    ABIArg& operator*() { MOZ_ASSERT(!done()); return gen_.current(); }

    unsigned index() const { MOZ_ASSERT(!done()); return i_; }
    MIRType mirType() const { MOZ_ASSERT(!done()); return ToMIRType(types_[i_]); }
    uint32_t stackBytesConsumedSoFar() const { return gen_.stackBytesConsumedSoFar(); }
};

} // namespace jit
} // namespace js

#endif /* jit_MacroAssembler_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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

#include "wasm/WasmIonCompile.h"

#include "mozilla/MathAlgorithms.h"

#include <algorithm>

#include "jit/ABIArgGenerator.h"
#include "jit/CodeGenerator.h"
#include "jit/CompileInfo.h"
#include "jit/Ion.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/ShuffleAnalysis.h"
#include "js/ScalarType.h"  // js::Scalar::Type
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmCodegenTypes.h"
#include "wasm/WasmGC.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmIntrinsic.h"
#include "wasm/WasmOpIter.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::IsPowerOfTwo;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

namespace {

using BlockVector = Vector<MBasicBlock*, 8, SystemAllocPolicy>;
using DefVector = Vector<MDefinition*, 8, SystemAllocPolicy>;

// To compile try-catch blocks, we extend the IonCompilePolicy's ControlItem
// from being just an MBasicBlock* to a Control structure collecting additional
// information.
using ControlInstructionVector =
    Vector<MControlInstruction*, 8, SystemAllocPolicy>;

struct CatchInfo {
  uint32_t tagIndex;
  MBasicBlock* block;

  CatchInfo(uint32_t tagIndex, MBasicBlock* block)
      : tagIndex(tagIndex), block(block) {}
};

using CatchInfoVector = Vector<CatchInfo, 8, SystemAllocPolicy>;

struct Control {
  MBasicBlock* block;
  MBasicBlock* catchAllBlock;
  // For a try-catch ControlItem, when its block's Labelkind is Try, this
  // collects branches to later bind and create the try's landing pad.
  ControlInstructionVector tryPadPatches;

  // For a try-catch ControlItem, when its block's Labelkind is Catch, this
  // collects the first basic block of each handler and the handler's tag index
  // immediate, both wrapped together into a CatchInfo.
  CatchInfoVector tryCatches;

  Control() : block(nullptr), catchAllBlock(nullptr) {}

  explicit Control(MBasicBlock* block) : block(block), catchAllBlock(nullptr) {}

 public:
  void setBlock(MBasicBlock* newBlock) { block = newBlock; }

  // We ignore handlers whose tag index already appeared.
  bool tagAlreadyHandled(uint32_t tagIndex) {
    for (CatchInfo& info : tryCatches) {
      if (tagIndex == info.tagIndex) {
        return true;
      }
    }
    return false;
  }
};

// [SMDOC] WebAssembly Exception Handling (Wasm-EH) in Ion
// =======================================================
//
// Control struct as ControlItem for WebAssembly Exception Handling (Wasm-EH)
// --------------------------------------------------------------------------
//
// Using the above "struct Control" as a ControlItem in IonCompilePolicy,
// simplifies the compilation of Wasm-EH try-catch blocks in two ways.
//
// 1. By collecting any paths we create from throws or potential throws (Wasm
//    function calls) in the vector tryPadPatches, so they can be bound to
//    create the landing pad.
// 2. By keeping track of each handler with its CatchInfo in the vector
//    tryCatches, to simplify creating the landing pad's control instruction,
//    after we read End. This control instruction, in general a table switch,
//    will direct caught exceptions to the correct catch code.
//
// Without such a Control structure, we'd have to track the tryPadPatches of
// potentially nested try blocks manually in the function compiler. Moreover,
// the landing pad's control instruction, a table switch, would have to be
// modified every time we read a new catch. With the above control structure,
// that table switch is created after we read the last catch and know which
// successors it should have, and whether it has a catch_all block or if it
// rethrows unhandled exceptions.
//
//
// Design and terminology around the Wasm-EH additions in Ion
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// This documentation aims to explain the design and names used for the Wasm-EH
// additions in Ion. We'll go through what happens while compiling a Wasm
// try-catch/catch_all instruction.
//
// When we encounter a try Opcode, we immediately create a new basic block to
// hold the instructions of the try-code, i.e., the Wasm code after "try" and
// before the next "catch", "catch_all", or "end" Opcode appears in the OpIter
// loop.
//
// Catching try control nodes
// ..........................
//
// Wasm exceptions can be thrown by either a throw instruction (local throw),
// or by a direct or indirect Wasm function call. On all these occassions, we
// know we are in try-code, if there is a surrounding ControlItem with
// LabelKind::Try. The innermost such control is called the "catching try
// control". In all these occassions, we create a branch to a new block, which
// contains the exception in its slots, and call this a pre-pad block.
//
// Creating pre-pad blocks
// .......................
//
// There are two possible sorts of pre-pad blocks, depending on whether we
// are branching after a local throw instruction, or after a Wasm function
// call:
//
// - If we encounter a throw instruction while in try-code (a local throw), we
//   create the exception and tag index MDefinitions, create and jump to a
//   pre-pad block. The exception and tag index are pushed to the pre-pad
//   block.
//
// - If we encounter a direct, indirect, or imported Wasm function call, then
//   we set the WasmCall to initialise a WasmTryNote, whose "start", "end", and
//   "entry point" offsets are all inside the WasmCall. After such a Wasm
//   function call, we add an MWasmLoadTls instruction representing a possibly
//   thrown exception, which the throw mechanism would have stored in
//   wasm::TlsData::pendingException. We then add a test which branches to a new
//   pre-pad block if there is a pending exception, or continues with the
//   opcodes in the try-code, if there was no pending exception. During this
//   branch, any found exception is pushed to the pre-pad block, and then an
//   instance call is made from the pre-pad block to clear the pending exception
//   from the TlsData, and retrieve its local tag index. This tag index is
//   pushed to the pre-pad block as well.
//
// We end each pre-pad block with a jump to a nullptr, as is done when using
// ControlFlowPatches. However, we don't need to collect ControlFlowPatches
// for our case, because we only have one successor to a pad patch's last
// instruction. We collect all [1] these last instructíons (jumps-to-be-patched)
// in the catching try control's  `tryPadPatches`.
//
// Creating the landing pad
// ........................
//
// When we exit try-code, i.e., when the next Opcode we read is "catch",
// "catch_all", or "end", we check if tryPadPatches has captured any control
// instructions (pad patches). If not, we don't compile any catches and we mark
// the rest as dead code. If there are pre-pad blocks, we join them to
// create a landing pad (or just "pad"), which becomes the ControlItem's block.
// The pad's last two slots are the caught exception, and the exception's local
// tag index.
//
// There are three different forms of try-catch/catch_all Wasm instructions,
// which result in different form of landing pad.
//
// 1. A catchless try, so a Wasm instruction of the form "try ... end".
//    - In this case, we end the pad by rethrowing the caught exception.
//
// 2. A single catch_all after a try.
//    - If the first catch after a try is a catch_all, then there won't be
//      any more catches, but we need the exception and its local tag index, in
//      case the code in a catch_all contains "rethrow" instructions.
//      - The Wasm instruction "rethrow", gets the exception and tag index to
//        rethrow from the last two slots of the landing pad which, due to
//        validation, is the l'th surrounding ControlItem.
//      - We immediately GoTo to a new block after the pad and pop both the
//        exception and tag index, as we don't need them anymore in this case.
//
// 3. Otherwise, there is one or more catch code blocks following.
//    - In this case, we leave the pad without a last instruction for now, and
//      compile "catch" or "catch_all" each in a new block created [3] from the
//      pad, collecting these blocks together with their tag index, into the
//      ControlItem's CatchInfoVector. Any of these blocks which is not dead
//      code is finished like a br 0 (including the last block of the end of the
//      try code). When we finally reach "end" we use the exception's local tag
//      index (last slot of the pad) to finish the pad with a tableswitch [2].
//      The successors of the table switch and the case (tag index) they
//      correspond to (they handle) are added with the help of the Control's
//      `CatchInfoVector tryCatches`. If there was no catch_all found, the
//      table's default case is a block which rethrows the exception.
//
//
// Throws without a catching try control node
// ..........................................
//
// Such throws finish their current basic block with an instance call that
// triggers the exception throwing runtime. The runtime finds which surrounding
// frame has a try note with matching offsets, or throws the exception to JS.
// Code after a throw is always dead code.
//
//
// Example control flow graph
// --------------------------
//
// The following Wasm code does a conditional throw of an exception carrying the
// f64 value 6. If the "f" called does nothing, then a function with just the
// code below would return 10 if called with argument 0 or 2 otherwise.
//
//      (try (param i32) (result f64)
//        (do
//          (if (result f64)
//            (then
//              (f64.const 3))
//            (else
//              (throw $exn (f64.const 6))))
//          (call $f)
//          (f64.sub (f64.const 2)))
//        (catch $exn
//          (f64.add (f64.const 4)))
//        (catch_all
//          (f64.const 5)))
//
// The above Wasm code should result in roughly the following control flow
// graph. "GoTo ??" indicates a control instruction that was patched later.
// Test branches are marked with the value that would lead to that branch. The
// definitions and the instructions are numbered in the order they were added or
// pushed to a block. Some auxiliary definitions and instructions are not shown
// in order to reduce clutter. You can use your favourite control flow graphing
// tool (for example iongraph [4]) to get a graph with more details. For
// convenience, there is a Wasm module using this code in the test file
// "js/src/jit-test/test/wasm/exceptions/example.js".
//
//
//   __block0__(control Try)__
//  |                         |
//  | v0 = local.get 0        |
//  | v1 = GoTo block1        |
//  |_________________________|
//                 |
//                 V
//   __block1__(control If)______
//  |                            |
//  | v2 = Test v0 block2 block3 |
//  |____________________________|    __block3________________________________
//           1|              0\      |                                        |
//            V                \     | v4 = f64.const 6                       |
//   __block2___________        \--->| v5 = create a new exception (&v6) with |
//  |                   |            |      tag $exn (v7), and store v4 in    |
//  | v3 = f64.const 2  |            |      the exception's VALUES buffer)    |
//  | v10 = GoTo block5 |            | v9 = GoTo block4 (local throw)         |
//  |___________________|            |________________________________________|
//            |                                       |
//            |                                       |
//            V                                       V__ block4__(pre-pad)____
//   __block5_____________________________________    |                        |
//  |                                             |   | v6 = the new exception |
//  | v11 = call $f                               |   |      now carrying v4   |
//  | v12 = load exception from TlsData           |   | v7 = tag index $exn    |
//  | v13 = Test (v12 not nullref?) block7 block6 |   | v8 = GoTo ?? -> block8 |
//  |_____________________________________________|   |________________________|
//       0|              1\                                                |
//        |                \                                               |
//        |                 \                                              |
//        |                  \     __ block7__(pre_pad)_______________     |
//        V                   \-->|                                   |    |
//  (last block in try code)      | v14 = clear the pending exception |    |
//   __block6_________________    |       from TlsData and get v12's  |    |
//  |                         |   |       local tag index &v15        |    |
//  | v17 = f64.const 3       |   | v15 = tag index of v12            |    |
//  | v18 = f64.sub v4 v17    |   | v16 = GoTo ?? -> block8           |    |
//  | v19 GoTo ??? -> block11 |   |___________________________________|    |
//  |_________________________|     |                                      |
//             |                    |          (control Try)               |
//             |                    V__block8__(landing_pad)_______________V
//             |                    |                                      |
//             |                    | v20 = Phi(v6, v12) exception         |
//             |                    | v21 = Phi(v7, v15) tag index         |
//             |                    | v27 = 1 + v21                        |
//             |                    | v28 = TableSwitch v27 block10 block9 |
//             |                    |______________________________________|
//             |                      0|     $exn+1|
//             |                default|           |
//             |                       |           V__block9__(catch_$exn)_____
//             |                       V           |                           |
//             |      __block10__(catch_all)_      | v22 = load the first (and |
//             |     |                       |     |       only) value in      |
//             |     | v26 = f64.const 5     |     |       v20's VALUES buffer |
//             |     | v30 = GoTo block11    |     | v23 = f64.const 4         |
//             |     |_______________________|     | v24 = f64.add v22 v23     |
//             |         |                         | v25 = GoTo ??? -> block11 |
//             |         |                         |___________________________|
//             V         V                           /
//  __block11__(try_catch_join)__                   /
// |                             |<----------------/
// | v29 = Phi(v18, v24, v26)    |
// | v31 = Return v29            |
// |_____________________________|
//
//
// Notes:
// ------
//
// - By creating and branching to pre-pad blocks while compiling the try code
//   we ensure that the landing pad will have the correct information with
//   respect to any local wasm state changes, that may have occurred in the try
//   code before an exception was thrown.
//
// Footnotes:
// ----------
//
// [1] We could potentially optimise this by separately collecting any jumps
//     from pre-pad blocks coming from Wasm function calls, not doing any
//     instance calls in these pre-pad blocks, but join them to an intermediate
//     basic block which only does the instance call to consume the pending
//     exception and get the tag index once. // TODO: Is it worth it?
//
// [2] We could potentially optimise this by compiling the case of a single
//     tagged catch into a plain MTest, although it's possible that in that case
//     Ion automatically simplifies such a table switch to a test anyway.
//
// [3] Each new block created for a catch is "created from the pad block" in the
//     sense of "newBlock(pad, catch)". This is done to make sure that the catch
//     has the correct stack position and contents. Each catch block must hold
//     the exception and tag index in its initial slots, and each must have the
//     same stack position as the pad, because the pad is later added as a
//     predecessor.

struct IonCompilePolicy {
  // We store SSA definitions in the value stack.
  using Value = MDefinition*;
  using ValueVector = DefVector;

  // We store loop headers and then/else blocks in the control flow stack.
  // In the case of try-catch control blocks, we collect additional information
  // regarding the possible paths from throws and calls to a landing pad, as
  // well as information on the landing pad's handlers (its catches).
  using ControlItem = Control;
};

using IonOpIter = OpIter<IonCompilePolicy>;

class FunctionCompiler;

// CallCompileState describes a call that is being compiled.

class CallCompileState {
  // A generator object that is passed each argument as it is compiled.
  WasmABIArgGenerator abi_;

  // Accumulates the register arguments while compiling arguments.
  MWasmCall::Args regArgs_;

  // Reserved argument for passing Instance* to builtin instance method calls.
  ABIArg instanceArg_;

  // The stack area in which the callee will write stack return values, or
  // nullptr if no stack results.
  MWasmStackResultArea* stackResultArea_ = nullptr;

  // Only FunctionCompiler should be directly manipulating CallCompileState.
  friend class FunctionCompiler;
};

// Encapsulates the compilation of a single function in an asm.js module. The
// function compiler handles the creation and final backend compilation of the
// MIR graph.
class FunctionCompiler {
  struct ControlFlowPatch {
    MControlInstruction* ins;
    uint32_t index;
    ControlFlowPatch(MControlInstruction* ins, uint32_t index)
        : ins(ins), index(index) {}
  };

  using ControlFlowPatchVector = Vector<ControlFlowPatch, 0, SystemAllocPolicy>;
  using ControlFlowPatchVectorVector =
      Vector<ControlFlowPatchVector, 0, SystemAllocPolicy>;

  const ModuleEnvironment& moduleEnv_;
  IonOpIter iter_;
  const FuncCompileInput& func_;
  const ValTypeVector& locals_;
  size_t lastReadCallSite_;

  TempAllocator& alloc_;
  MIRGraph& graph_;
  const CompileInfo& info_;
  MIRGenerator& mirGen_;

  MBasicBlock* curBlock_;
  uint32_t maxStackArgBytes_;

  uint32_t loopDepth_;
  uint32_t blockDepth_;
  ControlFlowPatchVectorVector blockPatches_;

  // TLS pointer argument to the current function.
  MWasmParameter* tlsPointer_;
  MWasmParameter* stackResultPointer_;

 public:
  FunctionCompiler(const ModuleEnvironment& moduleEnv, Decoder& decoder,
                   const FuncCompileInput& func, const ValTypeVector& locals,
                   MIRGenerator& mirGen)
      : moduleEnv_(moduleEnv),
        iter_(moduleEnv, decoder),
        func_(func),
        locals_(locals),
        lastReadCallSite_(0),
        alloc_(mirGen.alloc()),
        graph_(mirGen.graph()),
        info_(mirGen.outerInfo()),
        mirGen_(mirGen),
        curBlock_(nullptr),
        maxStackArgBytes_(0),
        loopDepth_(0),
        blockDepth_(0),
        tlsPointer_(nullptr),
        stackResultPointer_(nullptr) {}

  const ModuleEnvironment& moduleEnv() const { return moduleEnv_; }

  IonOpIter& iter() { return iter_; }
  TempAllocator& alloc() const { return alloc_; }
  // FIXME(1401675): Replace with BlockType.
  uint32_t funcIndex() const { return func_.index; }
  const FuncType& funcType() const {
    return *moduleEnv_.funcs[func_.index].type;
  }

  BytecodeOffset bytecodeOffset() const { return iter_.bytecodeOffset(); }
  BytecodeOffset bytecodeIfNotAsmJS() const {
    return moduleEnv_.isAsmJS() ? BytecodeOffset() : iter_.bytecodeOffset();
  }

  bool init() {
    // Prepare the entry block for MIR generation:

    const ArgTypeVector args(funcType());

    if (!mirGen_.ensureBallast()) {
      return false;
    }
    if (!newBlock(/* prev */ nullptr, &curBlock_)) {
      return false;
    }

    for (WasmABIArgIter i(args); !i.done(); i++) {
      MWasmParameter* ins = MWasmParameter::New(alloc(), *i, i.mirType());
      curBlock_->add(ins);
      if (args.isSyntheticStackResultPointerArg(i.index())) {
        MOZ_ASSERT(stackResultPointer_ == nullptr);
        stackResultPointer_ = ins;
      } else {
        curBlock_->initSlot(info().localSlot(args.naturalIndex(i.index())),
                            ins);
      }
      if (!mirGen_.ensureBallast()) {
        return false;
      }
    }

    // Set up a parameter that receives the hidden TLS pointer argument.
    tlsPointer_ =
        MWasmParameter::New(alloc(), ABIArg(WasmTlsReg), MIRType::Pointer);
    curBlock_->add(tlsPointer_);
    if (!mirGen_.ensureBallast()) {
      return false;
    }

    for (size_t i = args.lengthWithoutStackResults(); i < locals_.length();
         i++) {
      MInstruction* ins = nullptr;
      switch (locals_[i].kind()) {
        case ValType::I32:
          ins = MConstant::New(alloc(), Int32Value(0), MIRType::Int32);
          break;
        case ValType::I64:
          ins = MConstant::NewInt64(alloc(), 0);
          break;
        case ValType::V128:
#ifdef ENABLE_WASM_SIMD
          ins =
              MWasmFloatConstant::NewSimd128(alloc(), SimdConstant::SplatX4(0));
          break;
#else
          return iter().fail("Ion has no SIMD support yet");
#endif
        case ValType::F32:
          ins = MConstant::New(alloc(), Float32Value(0.f), MIRType::Float32);
          break;
        case ValType::F64:
          ins = MConstant::New(alloc(), DoubleValue(0.0), MIRType::Double);
          break;
        case ValType::Rtt:
        case ValType::Ref:
          ins = MWasmNullConstant::New(alloc());
          break;
      }

      curBlock_->add(ins);
      curBlock_->initSlot(info().localSlot(i), ins);
      if (!mirGen_.ensureBallast()) {
        return false;
      }
    }

    return true;
  }

  void finish() {
    mirGen().initWasmMaxStackArgBytes(maxStackArgBytes_);

    MOZ_ASSERT(loopDepth_ == 0);
    MOZ_ASSERT(blockDepth_ == 0);
#ifdef DEBUG
    for (ControlFlowPatchVector& patches : blockPatches_) {
      MOZ_ASSERT(patches.empty());
    }
#endif
    MOZ_ASSERT(inDeadCode());
    MOZ_ASSERT(done(), "all bytes must be consumed");
    MOZ_ASSERT(func_.callSiteLineNums.length() == lastReadCallSite_);
  }

  /************************* Read-only interface (after local scope setup) */

  MIRGenerator& mirGen() const { return mirGen_; }
  MIRGraph& mirGraph() const { return graph_; }
  const CompileInfo& info() const { return info_; }

  MDefinition* getLocalDef(unsigned slot) {
    if (inDeadCode()) {
      return nullptr;
    }
    return curBlock_->getSlot(info().localSlot(slot));
  }

  const ValTypeVector& locals() const { return locals_; }

  /***************************** Code generation (after local scope setup) */

  MDefinition* constant(const Value& v, MIRType type) {
    if (inDeadCode()) {
      return nullptr;
    }
    MConstant* constant = MConstant::New(alloc(), v, type);
    curBlock_->add(constant);
    return constant;
  }

  MDefinition* constant(float f) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* cst = MWasmFloatConstant::NewFloat32(alloc(), f);
    curBlock_->add(cst);
    return cst;
  }

  MDefinition* constant(double d) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* cst = MWasmFloatConstant::NewDouble(alloc(), d);
    curBlock_->add(cst);
    return cst;
  }

  MDefinition* constant(int64_t i) {
    if (inDeadCode()) {
      return nullptr;
    }
    MConstant* constant = MConstant::NewInt64(alloc(), i);
    curBlock_->add(constant);
    return constant;
  }

#ifdef ENABLE_WASM_SIMD
  MDefinition* constant(V128 v) {
    if (inDeadCode()) {
      return nullptr;
    }
    MWasmFloatConstant* constant = MWasmFloatConstant::NewSimd128(
        alloc(), SimdConstant::CreateSimd128((int8_t*)v.bytes));
    curBlock_->add(constant);
    return constant;
  }
#endif

  MDefinition* nullRefConstant() {
    if (inDeadCode()) {
      return nullptr;
    }
    // MConstant has a lot of baggage so we don't use that here.
    MWasmNullConstant* constant = MWasmNullConstant::New(alloc());
    curBlock_->add(constant);
    return constant;
  }

  void fence() {
    if (inDeadCode()) {
      return;
    }
    MWasmFence* ins = MWasmFence::New(alloc());
    curBlock_->add(ins);
  }

  template <class T>
  MDefinition* unary(MDefinition* op) {
    if (inDeadCode()) {
      return nullptr;
    }
    T* ins = T::New(alloc(), op);
    curBlock_->add(ins);
    return ins;
  }

  template <class T>
  MDefinition* unary(MDefinition* op, MIRType type) {
    if (inDeadCode()) {
      return nullptr;
    }
    T* ins = T::New(alloc(), op, type);
    curBlock_->add(ins);
    return ins;
  }

  template <class T>
  MDefinition* binary(MDefinition* lhs, MDefinition* rhs) {
    if (inDeadCode()) {
      return nullptr;
    }
    T* ins = T::New(alloc(), lhs, rhs);
    curBlock_->add(ins);
    return ins;
  }

  template <class T>
  MDefinition* binary(MDefinition* lhs, MDefinition* rhs, MIRType type) {
    if (inDeadCode()) {
      return nullptr;
    }
    T* ins = T::New(alloc(), lhs, rhs, type);
    curBlock_->add(ins);
    return ins;
  }

  template <class T>
  MDefinition* binary(MDefinition* lhs, MDefinition* rhs, MIRType type,
                      MWasmBinaryBitwise::SubOpcode subOpc) {
    if (inDeadCode()) {
      return nullptr;
    }
    T* ins = T::New(alloc(), lhs, rhs, type, subOpc);
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* ursh(MDefinition* lhs, MDefinition* rhs, MIRType type) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MUrsh::NewWasm(alloc(), lhs, rhs, type);
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* add(MDefinition* lhs, MDefinition* rhs, MIRType type) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MAdd::NewWasm(alloc(), lhs, rhs, type);
    curBlock_->add(ins);
    return ins;
  }

  bool mustPreserveNaN(MIRType type) {
    return IsFloatingPointType(type) && !moduleEnv().isAsmJS();
  }

  MDefinition* sub(MDefinition* lhs, MDefinition* rhs, MIRType type) {
    if (inDeadCode()) {
      return nullptr;
    }

    // wasm can't fold x - 0.0 because of NaN with custom payloads.
    MSub* ins = MSub::NewWasm(alloc(), lhs, rhs, type, mustPreserveNaN(type));
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* nearbyInt(MDefinition* input, RoundingMode roundingMode) {
    if (inDeadCode()) {
      return nullptr;
    }

    auto* ins = MNearbyInt::New(alloc(), input, input->type(), roundingMode);
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* minMax(MDefinition* lhs, MDefinition* rhs, MIRType type,
                      bool isMax) {
    if (inDeadCode()) {
      return nullptr;
    }

    if (mustPreserveNaN(type)) {
      // Convert signaling NaN to quiet NaNs.
      MDefinition* zero = constant(DoubleValue(0.0), type);
      lhs = sub(lhs, zero, type);
      rhs = sub(rhs, zero, type);
    }

    MMinMax* ins = MMinMax::NewWasm(alloc(), lhs, rhs, type, isMax);
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* mul(MDefinition* lhs, MDefinition* rhs, MIRType type,
                   MMul::Mode mode) {
    if (inDeadCode()) {
      return nullptr;
    }

    // wasm can't fold x * 1.0 because of NaN with custom payloads.
    auto* ins =
        MMul::NewWasm(alloc(), lhs, rhs, type, mode, mustPreserveNaN(type));
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* div(MDefinition* lhs, MDefinition* rhs, MIRType type,
                   bool unsignd) {
    if (inDeadCode()) {
      return nullptr;
    }
    bool trapOnError = !moduleEnv().isAsmJS();
    if (!unsignd && type == MIRType::Int32) {
      // Enforce the signedness of the operation by coercing the operands
      // to signed.  Otherwise, operands that "look" unsigned to Ion but
      // are not unsigned to Baldr (eg, unsigned right shifts) may lead to
      // the operation being executed unsigned.  Applies to mod() as well.
      //
      // Do this for Int32 only since Int64 is not subject to the same
      // issues.
      //
      // Note the offsets passed to MWasmBuiltinTruncateToInt32 are wrong here,
      // but it doesn't matter: they're not codegen'd to calls since inputs
      // already are int32.
      auto* lhs2 = createTruncateToInt32(lhs);
      curBlock_->add(lhs2);
      lhs = lhs2;
      auto* rhs2 = createTruncateToInt32(rhs);
      curBlock_->add(rhs2);
      rhs = rhs2;
    }

    // For x86 and arm we implement i64 div via c++ builtin.
    // A call to c++ builtin requires tls pointer.
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    if (type == MIRType::Int64) {
      auto* ins =
          MWasmBuiltinDivI64::New(alloc(), lhs, rhs, tlsPointer_, unsignd,
                                  trapOnError, bytecodeOffset());
      curBlock_->add(ins);
      return ins;
    }
#endif

    auto* ins = MDiv::New(alloc(), lhs, rhs, type, unsignd, trapOnError,
                          bytecodeOffset(), mustPreserveNaN(type));
    curBlock_->add(ins);
    return ins;
  }

  MInstruction* createTruncateToInt32(MDefinition* op) {
    if (op->type() == MIRType::Double || op->type() == MIRType::Float32) {
      return MWasmBuiltinTruncateToInt32::New(alloc(), op, tlsPointer_);
    }

    return MTruncateToInt32::New(alloc(), op);
  }

  MDefinition* mod(MDefinition* lhs, MDefinition* rhs, MIRType type,
                   bool unsignd) {
    if (inDeadCode()) {
      return nullptr;
    }
    bool trapOnError = !moduleEnv().isAsmJS();
    if (!unsignd && type == MIRType::Int32) {
      // See block comment in div().
      auto* lhs2 = createTruncateToInt32(lhs);
      curBlock_->add(lhs2);
      lhs = lhs2;
      auto* rhs2 = createTruncateToInt32(rhs);
      curBlock_->add(rhs2);
      rhs = rhs2;
    }

    // For x86 and arm we implement i64 mod via c++ builtin.
    // A call to c++ builtin requires tls pointer.
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    if (type == MIRType::Int64) {
      auto* ins =
          MWasmBuiltinModI64::New(alloc(), lhs, rhs, tlsPointer_, unsignd,
                                  trapOnError, bytecodeOffset());
      curBlock_->add(ins);
      return ins;
    }
#endif

    // Should be handled separately because we call BuiltinThunk for this case
    // and so, need to add the dependency from tlsPointer.
    if (type == MIRType::Double) {
      auto* ins = MWasmBuiltinModD::New(alloc(), lhs, rhs, tlsPointer_, type,
                                        bytecodeOffset());
      curBlock_->add(ins);
      return ins;
    }

    auto* ins = MMod::New(alloc(), lhs, rhs, type, unsignd, trapOnError,
                          bytecodeOffset());
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* bitnot(MDefinition* op) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MBitNot::New(alloc(), op);
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* select(MDefinition* trueExpr, MDefinition* falseExpr,
                      MDefinition* condExpr) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MWasmSelect::New(alloc(), trueExpr, falseExpr, condExpr);
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* extendI32(MDefinition* op, bool isUnsigned) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MExtendInt32ToInt64::New(alloc(), op, isUnsigned);
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* signExtend(MDefinition* op, uint32_t srcSize,
                          uint32_t targetSize) {
    if (inDeadCode()) {
      return nullptr;
    }
    MInstruction* ins;
    switch (targetSize) {
      case 4: {
        MSignExtendInt32::Mode mode;
        switch (srcSize) {
          case 1:
            mode = MSignExtendInt32::Byte;
            break;
          case 2:
            mode = MSignExtendInt32::Half;
            break;
          default:
            MOZ_CRASH("Bad sign extension");
        }
        ins = MSignExtendInt32::New(alloc(), op, mode);
        break;
      }
      case 8: {
        MSignExtendInt64::Mode mode;
        switch (srcSize) {
          case 1:
            mode = MSignExtendInt64::Byte;
            break;
          case 2:
            mode = MSignExtendInt64::Half;
            break;
          case 4:
            mode = MSignExtendInt64::Word;
            break;
          default:
            MOZ_CRASH("Bad sign extension");
        }
        ins = MSignExtendInt64::New(alloc(), op, mode);
        break;
      }
      default: {
        MOZ_CRASH("Bad sign extension");
      }
    }
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* convertI64ToFloatingPoint(MDefinition* op, MIRType type,
                                         bool isUnsigned) {
    if (inDeadCode()) {
      return nullptr;
    }
#if defined(JS_CODEGEN_ARM)
    auto* ins = MBuiltinInt64ToFloatingPoint::New(
        alloc(), op, tlsPointer_, type, bytecodeOffset(), isUnsigned);
#else
    auto* ins = MInt64ToFloatingPoint::New(alloc(), op, type, bytecodeOffset(),
                                           isUnsigned);
#endif
    curBlock_->add(ins);
    return ins;
  }

  MDefinition* rotate(MDefinition* input, MDefinition* count, MIRType type,
                      bool left) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MRotate::New(alloc(), input, count, type, left);
    curBlock_->add(ins);
    return ins;
  }

  template <class T>
  MDefinition* truncate(MDefinition* op, TruncFlags flags) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = T::New(alloc(), op, flags, bytecodeOffset());
    curBlock_->add(ins);
    return ins;
  }

#if defined(JS_CODEGEN_ARM)
  MDefinition* truncateWithTls(MDefinition* op, TruncFlags flags) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MWasmBuiltinTruncateToInt64::New(alloc(), op, tlsPointer_,
                                                 flags, bytecodeOffset());
    curBlock_->add(ins);
    return ins;
  }
#endif

  MDefinition* compare(MDefinition* lhs, MDefinition* rhs, JSOp op,
                       MCompare::CompareType type) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MCompare::NewWasm(alloc(), lhs, rhs, op, type);
    curBlock_->add(ins);
    return ins;
  }

  void assign(unsigned slot, MDefinition* def) {
    if (inDeadCode()) {
      return;
    }
    curBlock_->setSlot(info().localSlot(slot), def);
  }

#ifdef ENABLE_WASM_SIMD
  // About Wasm SIMD as supported by Ion:
  //
  // The expectation is that Ion will only ever support SIMD on x86 and x64,
  // since Cranelift will be the optimizing compiler for Arm64, ARMv7 will cease
  // to be a tier-1 platform soon, and MIPS64 will never implement SIMD.
  //
  // The division of the operations into MIR nodes reflects that expectation,
  // and is a good fit for x86/x64.  Should the expectation change we'll
  // possibly want to re-architect the SIMD support to be a little more general.
  //
  // Most SIMD operations map directly to a single MIR node that ultimately ends
  // up being expanded in the macroassembler.
  //
  // Some SIMD operations that do have a complete macroassembler expansion are
  // open-coded into multiple MIR nodes here; in some cases that's just
  // convenience, in other cases it may also allow them to benefit from Ion
  // optimizations.  The reason for the expansions will be documented by a
  // comment.

  // (v128,v128) -> v128 effect-free binary operations
  MDefinition* binarySimd128(MDefinition* lhs, MDefinition* rhs,
                             bool commutative, SimdOp op) {
    if (inDeadCode()) {
      return nullptr;
    }

    MOZ_ASSERT(lhs->type() == MIRType::Simd128 &&
               rhs->type() == MIRType::Simd128);

    auto* ins = MWasmBinarySimd128::New(alloc(), lhs, rhs, commutative, op);
    curBlock_->add(ins);
    return ins;
  }

  // (v128,i32) -> v128 effect-free shift operations
  MDefinition* shiftSimd128(MDefinition* lhs, MDefinition* rhs, SimdOp op) {
    if (inDeadCode()) {
      return nullptr;
    }

    MOZ_ASSERT(lhs->type() == MIRType::Simd128 &&
               rhs->type() == MIRType::Int32);

    int32_t maskBits;
    if (MacroAssembler::MustMaskShiftCountSimd128(op, &maskBits)) {
      MConstant* mask = MConstant::New(alloc(), Int32Value(maskBits));
      curBlock_->add(mask);
      auto* rhs2 = MBitAnd::New(alloc(), rhs, mask, MIRType::Int32);
      curBlock_->add(rhs2);
      rhs = rhs2;
    }

    auto* ins = MWasmShiftSimd128::New(alloc(), lhs, rhs, op);
    curBlock_->add(ins);
    return ins;
  }

  // (v128,scalar,imm) -> v128
  MDefinition* replaceLaneSimd128(MDefinition* lhs, MDefinition* rhs,
                                  uint32_t laneIndex, SimdOp op) {
    if (inDeadCode()) {
      return nullptr;
    }

    MOZ_ASSERT(lhs->type() == MIRType::Simd128);

    auto* ins = MWasmReplaceLaneSimd128::New(alloc(), lhs, rhs, laneIndex, op);
    curBlock_->add(ins);
    return ins;
  }

  // (scalar) -> v128 effect-free unary operations
  MDefinition* scalarToSimd128(MDefinition* src, SimdOp op) {
    if (inDeadCode()) {
      return nullptr;
    }

    auto* ins = MWasmScalarToSimd128::New(alloc(), src, op);
    curBlock_->add(ins);
    return ins;
  }

  // (v128) -> v128 effect-free unary operations
  MDefinition* unarySimd128(MDefinition* src, SimdOp op) {
    if (inDeadCode()) {
      return nullptr;
    }

    MOZ_ASSERT(src->type() == MIRType::Simd128);
    auto* ins = MWasmUnarySimd128::New(alloc(), src, op);
    curBlock_->add(ins);
    return ins;
  }

  // (v128, imm) -> scalar effect-free unary operations
  MDefinition* reduceSimd128(MDefinition* src, SimdOp op, ValType outType,
                             uint32_t imm = 0) {
    if (inDeadCode()) {
      return nullptr;
    }

    MOZ_ASSERT(src->type() == MIRType::Simd128);
    auto* ins =
        MWasmReduceSimd128::New(alloc(), src, op, ToMIRType(outType), imm);
    curBlock_->add(ins);
    return ins;
  }

  // (v128, v128, v128) -> v128 effect-free operations
  MDefinition* ternarySimd128(MDefinition* v0, MDefinition* v1, MDefinition* v2,
                              SimdOp op) {
    if (inDeadCode()) {
      return nullptr;
    }

    MOZ_ASSERT(v0->type() == MIRType::Simd128 &&
               v1->type() == MIRType::Simd128 &&
               v2->type() == MIRType::Simd128);

    auto* ins = MWasmTernarySimd128::New(alloc(), v0, v1, v2, op);
    curBlock_->add(ins);
    return ins;
  }

  // (v128, v128, imm_v128) -> v128 effect-free operations
  MDefinition* shuffleSimd128(MDefinition* v1, MDefinition* v2, V128 control) {
    if (inDeadCode()) {
      return nullptr;
    }

    MOZ_ASSERT(v1->type() == MIRType::Simd128);
    MOZ_ASSERT(v2->type() == MIRType::Simd128);
    auto* ins = BuildWasmShuffleSimd128(
        alloc(), reinterpret_cast<int8_t*>(control.bytes), v1, v2);
    curBlock_->add(ins);
    return ins;
  }

  // Also see below for SIMD memory references

#endif  // ENABLE_WASM_SIMD

  /************************************************ Linear memory accesses */

  // For detailed information about memory accesses, see "Linear memory
  // addresses and bounds checking" in WasmMemory.cpp.

 private:
  // If the platform does not have a HeapReg, load the memory base from Tls.
  MWasmLoadTls* maybeLoadMemoryBase() {
    MWasmLoadTls* load = nullptr;
#ifdef JS_CODEGEN_X86
    AliasSet aliases = !moduleEnv_.memory->canMovingGrow()
                           ? AliasSet::None()
                           : AliasSet::Load(AliasSet::WasmHeapMeta);
    load = MWasmLoadTls::New(alloc(), tlsPointer_,
                             offsetof(wasm::TlsData, memoryBase),
                             MIRType::Pointer, aliases);
    curBlock_->add(load);
#endif
    return load;
  }

 public:
  // A value holding the memory base, whether that's HeapReg or some other
  // register.
  MWasmHeapBase* memoryBase() {
    MWasmHeapBase* base = nullptr;
    AliasSet aliases = !moduleEnv_.memory->canMovingGrow()
                           ? AliasSet::None()
                           : AliasSet::Load(AliasSet::WasmHeapMeta);
    base = MWasmHeapBase::New(alloc(), tlsPointer_, aliases);
    curBlock_->add(base);
    return base;
  }

 private:
  // If the bounds checking strategy requires it, load the bounds check limit
  // from the Tls.
  MWasmLoadTls* maybeLoadBoundsCheckLimit(MIRType type) {
    MOZ_ASSERT(type == MIRType::Int32 || type == MIRType::Int64);
    if (moduleEnv_.hugeMemoryEnabled()) {
      return nullptr;
    }
    AliasSet aliases = !moduleEnv_.memory->canMovingGrow()
                           ? AliasSet::None()
                           : AliasSet::Load(AliasSet::WasmHeapMeta);
    auto* load = MWasmLoadTls::New(alloc(), tlsPointer_,
                                   offsetof(wasm::TlsData, boundsCheckLimit),
                                   type, aliases);
    curBlock_->add(load);
    return load;
  }

  // Return true if the access requires an alignment check.  If so, sets
  // *mustAdd to true if the offset must be added to the pointer before
  // checking.
  bool needAlignmentCheck(MemoryAccessDesc* access, MDefinition* base,
                          bool* mustAdd) {
    MOZ_ASSERT(!*mustAdd);

    // asm.js accesses are always aligned and need no checks.
    if (moduleEnv_.isAsmJS() || !access->isAtomic()) {
      return false;
    }

    // If the EA is known and aligned it will need no checks.
    if (base->isConstant()) {
      // We only care about the low bits, so overflow is OK, as is chopping off
      // the high bits of an i64 pointer.
      uint32_t ptr = 0;
      if (isMem64()) {
        ptr = uint32_t(base->toConstant()->toInt64());
      } else {
        ptr = base->toConstant()->toInt32();
      }
      if (((ptr + access->offset64()) & (access->byteSize() - 1)) == 0) {
        return false;
      }
    }

    // If the offset is aligned then the EA is just the pointer, for
    // the purposes of this check.
    *mustAdd = (access->offset64() & (access->byteSize() - 1)) != 0;
    return true;
  }

  // Fold a constant base into the offset and make the base 0, provided the
  // offset stays below the guard limit.  The reason for folding the base into
  // the offset rather than vice versa is that a small offset can be ignored
  // by both explicit bounds checking and bounds check elimination.
  void foldConstantPointer(MemoryAccessDesc* access, MDefinition** base) {
    uint32_t offsetGuardLimit =
        GetMaxOffsetGuardLimit(moduleEnv_.hugeMemoryEnabled());

    if ((*base)->isConstant()) {
      uint64_t basePtr = 0;
      if (isMem64()) {
        basePtr = uint64_t((*base)->toConstant()->toInt64());
      } else {
        basePtr = uint64_t(int64_t((*base)->toConstant()->toInt32()));
      }

      uint64_t offset = access->offset64();

      if (offset < offsetGuardLimit && basePtr < offsetGuardLimit - offset) {
        offset += uint32_t(basePtr);
        access->setOffset32(uint32_t(offset));

        MConstant* ins = nullptr;
        if (isMem64()) {
          ins = MConstant::NewInt64(alloc(), 0);
        } else {
          ins = MConstant::New(alloc(), Int32Value(0), MIRType::Int32);
        }
        curBlock_->add(ins);
        *base = ins;
      }
    }
  }

  // If the offset must be added because it is large or because the true EA must
  // be checked, compute the effective address, trapping on overflow.
  void maybeComputeEffectiveAddress(MemoryAccessDesc* access,
                                    MDefinition** base, bool mustAddOffset) {
    uint32_t offsetGuardLimit =
        GetMaxOffsetGuardLimit(moduleEnv_.hugeMemoryEnabled());

    if (access->offset64() >= offsetGuardLimit ||
        access->offset64() > UINT32_MAX || mustAddOffset ||
        !JitOptions.wasmFoldOffsets) {
      *base = computeEffectiveAddress(*base, access);
    }
  }

  MWasmLoadTls* needBoundsCheck() {
#ifdef JS_64BIT
    // For 32-bit base pointers:
    //
    // If the bounds check uses the full 64 bits of the bounds check limit, then
    // the base pointer must be zero-extended to 64 bits before checking and
    // wrapped back to 32-bits after Spectre masking.  (And it's important that
    // the value we end up with has flowed through the Spectre mask.)
    //
    // If the memory's max size is known to be smaller than 64K pages exactly,
    // we can use a 32-bit check and avoid extension and wrapping.
    static_assert(0x100000000 % PageSize == 0);
    bool mem32LimitIs64Bits = isMem32() &&
                              !moduleEnv_.memory->boundsCheckLimitIs32Bits() &&
                              MaxMemoryPages(moduleEnv_.memory->indexType()) >=
                                  Pages(0x100000000 / PageSize);
#else
    // On 32-bit platforms we have no more than 2GB memory and the limit for a
    // 32-bit base pointer is never a 64-bit value.
    bool mem32LimitIs64Bits = false;
#endif
    return maybeLoadBoundsCheckLimit(
        mem32LimitIs64Bits || isMem64() ? MIRType::Int64 : MIRType::Int32);
  }

  void performBoundsCheck(MDefinition** base, MWasmLoadTls* boundsCheckLimit) {
    // At the outset, actualBase could be the result of pretty much any integer
    // operation, or it could be the load of an integer constant.  If its type
    // is i32, we may assume the value has a canonical representation for the
    // platform, see doc block in MacroAssembler.h.
    MDefinition* actualBase = *base;

    // Extend an i32 index value to perform a 64-bit bounds check if the memory
    // can be 4GB or larger.
    bool extendAndWrapIndex =
        isMem32() && boundsCheckLimit->type() == MIRType::Int64;
    if (extendAndWrapIndex) {
      auto* extended = MWasmExtendU32Index::New(alloc(), actualBase);
      curBlock_->add(extended);
      actualBase = extended;
    }

    auto* ins = MWasmBoundsCheck::New(alloc(), actualBase, boundsCheckLimit,
                                      bytecodeOffset());
    curBlock_->add(ins);
    actualBase = ins;

    // If we're masking, then we update *base to create a dependency chain
    // through the masked index.  But we will first need to wrap the index
    // value if it was extended above.
    if (JitOptions.spectreIndexMasking) {
      if (extendAndWrapIndex) {
        auto* wrapped = MWasmWrapU32Index::New(alloc(), actualBase);
        curBlock_->add(wrapped);
        actualBase = wrapped;
      }
      *base = actualBase;
    }
  }

  // Perform all necessary checking before a wasm heap access, based on the
  // attributes of the access and base pointer.
  //
  // For 64-bit indices on platforms that are limited to indices that fit into
  // 32 bits (all 32-bit platforms and mips64), this returns a bounds-checked
  // `base` that has type Int32.  Lowering code depends on this and will assert
  // that the base has this type.  See the end of this function.

  void checkOffsetAndAlignmentAndBounds(MemoryAccessDesc* access,
                                        MDefinition** base) {
    MOZ_ASSERT(!inDeadCode());
    MOZ_ASSERT(!moduleEnv_.isAsmJS());

    // Attempt to fold an offset into a constant base pointer so as to simplify
    // the addressing expression.  This may update *base.
    foldConstantPointer(access, base);

    // Determine whether an alignment check is needed and whether the offset
    // must be checked too.
    bool mustAddOffsetForAlignmentCheck = false;
    bool alignmentCheck =
        needAlignmentCheck(access, *base, &mustAddOffsetForAlignmentCheck);

    // If bounds checking or alignment checking requires it, compute the
    // effective address: add the offset into the pointer and trap on overflow.
    // This may update *base.
    maybeComputeEffectiveAddress(access, base, mustAddOffsetForAlignmentCheck);

    // Emit the alignment check if necessary; it traps if it fails.
    if (alignmentCheck) {
      curBlock_->add(MWasmAlignmentCheck::New(
          alloc(), *base, access->byteSize(), bytecodeOffset()));
    }

    // Emit the bounds check if necessary; it traps if it fails.  This may
    // update *base.
    MWasmLoadTls* boundsCheckLimit = needBoundsCheck();
    if (boundsCheckLimit) {
      performBoundsCheck(base, boundsCheckLimit);
    }

#ifndef JS_64BIT
    if (isMem64()) {
      // We must have had an explicit bounds check (or one was elided if it was
      // proved redundant), and on 32-bit systems the index will for sure fit in
      // 32 bits: the max memory is 2GB.  So chop the index down to 32-bit to
      // simplify the back-end.
      MOZ_ASSERT((*base)->type() == MIRType::Int64);
      MOZ_ASSERT(!moduleEnv_.hugeMemoryEnabled());
      auto* chopped = MWasmWrapU32Index::New(alloc(), *base);
      MOZ_ASSERT(chopped->type() == MIRType::Int32);
      curBlock_->add(chopped);
      *base = chopped;
    }
#endif
  }

  bool isSmallerAccessForI64(ValType result, const MemoryAccessDesc* access) {
    if (result == ValType::I64 && access->byteSize() <= 4) {
      // These smaller accesses should all be zero-extending.
      MOZ_ASSERT(!isSignedIntType(access->type()));
      return true;
    }
    return false;
  }

 public:
  bool isMem32() { return moduleEnv_.memory->indexType() == IndexType::I32; }
  bool isMem64() { return moduleEnv_.memory->indexType() == IndexType::I64; }

  // Sometimes, we need to determine the memory type before the opcode reader
  // that will reject a memory opcode in the presence of no-memory gets a chance
  // to do so. This predicate is safe.
  bool isNoMemOrMem32() {
    return !moduleEnv_.usesMemory() ||
           moduleEnv_.memory->indexType() == IndexType::I32;
  }

  // Add the offset into the pointer to yield the EA; trap on overflow.
  MDefinition* computeEffectiveAddress(MDefinition* base,
                                       MemoryAccessDesc* access) {
    if (inDeadCode()) {
      return nullptr;
    }
    uint64_t offset = access->offset64();
    if (offset == 0) {
      return base;
    }
    auto* ins = MWasmAddOffset::New(alloc(), base, offset, bytecodeOffset());
    curBlock_->add(ins);
    access->clearOffset();
    return ins;
  }

  MDefinition* load(MDefinition* base, MemoryAccessDesc* access,
                    ValType result) {
    if (inDeadCode()) {
      return nullptr;
    }

    MWasmLoadTls* memoryBase = maybeLoadMemoryBase();
    MInstruction* load = nullptr;
    if (moduleEnv_.isAsmJS()) {
      MOZ_ASSERT(access->offset64() == 0);
      MWasmLoadTls* boundsCheckLimit =
          maybeLoadBoundsCheckLimit(MIRType::Int32);
      load = MAsmJSLoadHeap::New(alloc(), memoryBase, base, boundsCheckLimit,
                                 access->type());
    } else {
      checkOffsetAndAlignmentAndBounds(access, &base);
#ifndef JS_64BIT
      MOZ_ASSERT(base->type() == MIRType::Int32);
#endif
      load =
          MWasmLoad::New(alloc(), memoryBase, base, *access, ToMIRType(result));
    }
    if (!load) {
      return nullptr;
    }
    curBlock_->add(load);
    return load;
  }

  void store(MDefinition* base, MemoryAccessDesc* access, MDefinition* v) {
    if (inDeadCode()) {
      return;
    }

    MWasmLoadTls* memoryBase = maybeLoadMemoryBase();
    MInstruction* store = nullptr;
    if (moduleEnv_.isAsmJS()) {
      MOZ_ASSERT(access->offset64() == 0);
      MWasmLoadTls* boundsCheckLimit =
          maybeLoadBoundsCheckLimit(MIRType::Int32);
      store = MAsmJSStoreHeap::New(alloc(), memoryBase, base, boundsCheckLimit,
                                   access->type(), v);
    } else {
      checkOffsetAndAlignmentAndBounds(access, &base);
#ifndef JS_64BIT
      MOZ_ASSERT(base->type() == MIRType::Int32);
#endif
      store = MWasmStore::New(alloc(), memoryBase, base, *access, v);
    }
    if (!store) {
      return;
    }
    curBlock_->add(store);
  }

  MDefinition* atomicCompareExchangeHeap(MDefinition* base,
                                         MemoryAccessDesc* access,
                                         ValType result, MDefinition* oldv,
                                         MDefinition* newv) {
    if (inDeadCode()) {
      return nullptr;
    }

    checkOffsetAndAlignmentAndBounds(access, &base);
#ifndef JS_64BIT
    MOZ_ASSERT(base->type() == MIRType::Int32);
#endif

    if (isSmallerAccessForI64(result, access)) {
      auto* cvtOldv =
          MWrapInt64ToInt32::New(alloc(), oldv, /*bottomHalf=*/true);
      curBlock_->add(cvtOldv);
      oldv = cvtOldv;

      auto* cvtNewv =
          MWrapInt64ToInt32::New(alloc(), newv, /*bottomHalf=*/true);
      curBlock_->add(cvtNewv);
      newv = cvtNewv;
    }

    MWasmLoadTls* memoryBase = maybeLoadMemoryBase();
    MInstruction* cas =
        MWasmCompareExchangeHeap::New(alloc(), bytecodeOffset(), memoryBase,
                                      base, *access, oldv, newv, tlsPointer_);
    if (!cas) {
      return nullptr;
    }
    curBlock_->add(cas);

    if (isSmallerAccessForI64(result, access)) {
      cas = MExtendInt32ToInt64::New(alloc(), cas, true);
      curBlock_->add(cas);
    }

    return cas;
  }

  MDefinition* atomicExchangeHeap(MDefinition* base, MemoryAccessDesc* access,
                                  ValType result, MDefinition* value) {
    if (inDeadCode()) {
      return nullptr;
    }

    checkOffsetAndAlignmentAndBounds(access, &base);
#ifndef JS_64BIT
    MOZ_ASSERT(base->type() == MIRType::Int32);
#endif

    if (isSmallerAccessForI64(result, access)) {
      auto* cvtValue =
          MWrapInt64ToInt32::New(alloc(), value, /*bottomHalf=*/true);
      curBlock_->add(cvtValue);
      value = cvtValue;
    }

    MWasmLoadTls* memoryBase = maybeLoadMemoryBase();
    MInstruction* xchg =
        MWasmAtomicExchangeHeap::New(alloc(), bytecodeOffset(), memoryBase,
                                     base, *access, value, tlsPointer_);
    if (!xchg) {
      return nullptr;
    }
    curBlock_->add(xchg);

    if (isSmallerAccessForI64(result, access)) {
      xchg = MExtendInt32ToInt64::New(alloc(), xchg, true);
      curBlock_->add(xchg);
    }

    return xchg;
  }

  MDefinition* atomicBinopHeap(AtomicOp op, MDefinition* base,
                               MemoryAccessDesc* access, ValType result,
                               MDefinition* value) {
    if (inDeadCode()) {
      return nullptr;
    }

    checkOffsetAndAlignmentAndBounds(access, &base);
#ifndef JS_64BIT
    MOZ_ASSERT(base->type() == MIRType::Int32);
#endif

    if (isSmallerAccessForI64(result, access)) {
      auto* cvtValue =
          MWrapInt64ToInt32::New(alloc(), value, /*bottomHalf=*/true);
      curBlock_->add(cvtValue);
      value = cvtValue;
    }

    MWasmLoadTls* memoryBase = maybeLoadMemoryBase();
    MInstruction* binop =
        MWasmAtomicBinopHeap::New(alloc(), bytecodeOffset(), op, memoryBase,
                                  base, *access, value, tlsPointer_);
    if (!binop) {
      return nullptr;
    }
    curBlock_->add(binop);

    if (isSmallerAccessForI64(result, access)) {
      binop = MExtendInt32ToInt64::New(alloc(), binop, true);
      curBlock_->add(binop);
    }

    return binop;
  }

#ifdef ENABLE_WASM_SIMD
  MDefinition* loadSplatSimd128(Scalar::Type viewType,
                                const LinearMemoryAddress<MDefinition*>& addr,
                                wasm::SimdOp splatOp) {
    if (inDeadCode()) {
      return nullptr;
    }

    MemoryAccessDesc access(viewType, addr.align, addr.offset,
                            bytecodeIfNotAsmJS());

    // Generate better code (on x86)
    // If AVX2 is enabled, more broadcast operators are available.
    if (viewType == Scalar::Float64
#  if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
        || (js::jit::CPUInfo::IsAVX2Present() &&
            (viewType == Scalar::Uint8 || viewType == Scalar::Uint16 ||
             viewType == Scalar::Float32))
#  endif
    ) {
      access.setSplatSimd128Load();
      return load(addr.base, &access, ValType::V128);
    }

    ValType resultType = ValType::I32;
    if (viewType == Scalar::Float32) {
      resultType = ValType::F32;
      splatOp = wasm::SimdOp::F32x4Splat;
    }
    auto* scalar = load(addr.base, &access, resultType);
    if (!inDeadCode() && !scalar) {
      return nullptr;
    }
    return scalarToSimd128(scalar, splatOp);
  }

  MDefinition* loadExtendSimd128(const LinearMemoryAddress<MDefinition*>& addr,
                                 wasm::SimdOp op) {
    if (inDeadCode()) {
      return nullptr;
    }

    // Generate better code (on x86) by loading as a double with an
    // operation that sign extends directly.
    MemoryAccessDesc access(Scalar::Float64, addr.align, addr.offset,
                            bytecodeIfNotAsmJS());
    access.setWidenSimd128Load(op);
    return load(addr.base, &access, ValType::V128);
  }

  MDefinition* loadZeroSimd128(Scalar::Type viewType, size_t numBytes,
                               const LinearMemoryAddress<MDefinition*>& addr) {
    if (inDeadCode()) {
      return nullptr;
    }

    MemoryAccessDesc access(viewType, addr.align, addr.offset,
                            bytecodeIfNotAsmJS());
    access.setZeroExtendSimd128Load();
    return load(addr.base, &access, ValType::V128);
  }

  MDefinition* loadLaneSimd128(uint32_t laneSize,
                               const LinearMemoryAddress<MDefinition*>& addr,
                               uint32_t laneIndex, MDefinition* src) {
    if (inDeadCode()) {
      return nullptr;
    }

    MemoryAccessDesc access(Scalar::Simd128, addr.align, addr.offset,
                            bytecodeIfNotAsmJS());
    MWasmLoadTls* memoryBase = maybeLoadMemoryBase();
    MDefinition* base = addr.base;
    MOZ_ASSERT(!moduleEnv_.isAsmJS());
    checkOffsetAndAlignmentAndBounds(&access, &base);
#  ifndef JS_64BIT
    MOZ_ASSERT(base->type() == MIRType::Int32);
#  endif
    MInstruction* load = MWasmLoadLaneSimd128::New(
        alloc(), memoryBase, base, access, laneSize, laneIndex, src);
    if (!load) {
      return nullptr;
    }
    curBlock_->add(load);
    return load;
  }

  void storeLaneSimd128(uint32_t laneSize,
                        const LinearMemoryAddress<MDefinition*>& addr,
                        uint32_t laneIndex, MDefinition* src) {
    if (inDeadCode()) {
      return;
    }
    MemoryAccessDesc access(Scalar::Simd128, addr.align, addr.offset,
                            bytecodeIfNotAsmJS());
    MWasmLoadTls* memoryBase = maybeLoadMemoryBase();
    MDefinition* base = addr.base;
    MOZ_ASSERT(!moduleEnv_.isAsmJS());
    checkOffsetAndAlignmentAndBounds(&access, &base);
#  ifndef JS_64BIT
    MOZ_ASSERT(base->type() == MIRType::Int32);
#  endif
    MInstruction* store = MWasmStoreLaneSimd128::New(
        alloc(), memoryBase, base, access, laneSize, laneIndex, src);
    if (!store) {
      return;
    }
    curBlock_->add(store);
  }
#endif  // ENABLE_WASM_SIMD

  /************************************************ Global variable accesses */

  MDefinition* loadGlobalVar(unsigned globalDataOffset, bool isConst,
                             bool isIndirect, MIRType type) {
    if (inDeadCode()) {
      return nullptr;
    }

    MInstruction* load;
    if (isIndirect) {
      // Pull a pointer to the value out of TlsData::globalArea, then
      // load from that pointer.  Note that the pointer is immutable
      // even though the value it points at may change, hence the use of
      // |true| for the first node's |isConst| value, irrespective of
      // the |isConst| formal parameter to this method.  The latter
      // applies to the denoted value as a whole.
      auto* cellPtr =
          MWasmLoadGlobalVar::New(alloc(), MIRType::Pointer, globalDataOffset,
                                  /*isConst=*/true, tlsPointer_);
      curBlock_->add(cellPtr);
      load = MWasmLoadGlobalCell::New(alloc(), type, cellPtr);
    } else {
      // Pull the value directly out of TlsData::globalArea.
      load = MWasmLoadGlobalVar::New(alloc(), type, globalDataOffset, isConst,
                                     tlsPointer_);
    }
    curBlock_->add(load);
    return load;
  }

  MInstruction* storeGlobalVar(uint32_t globalDataOffset, bool isIndirect,
                               MDefinition* v) {
    if (inDeadCode()) {
      return nullptr;
    }

    MInstruction* store;
    MInstruction* valueAddr = nullptr;
    if (isIndirect) {
      // Pull a pointer to the value out of TlsData::globalArea, then
      // store through that pointer.
      auto* cellPtr =
          MWasmLoadGlobalVar::New(alloc(), MIRType::Pointer, globalDataOffset,
                                  /*isConst=*/true, tlsPointer_);
      curBlock_->add(cellPtr);
      if (v->type() == MIRType::RefOrNull) {
        valueAddr = cellPtr;
        store = MWasmStoreRef::New(alloc(), tlsPointer_, valueAddr, v,
                                   AliasSet::WasmGlobalCell);
      } else {
        store = MWasmStoreGlobalCell::New(alloc(), v, cellPtr);
      }
    } else {
      // Store the value directly in TlsData::globalArea.
      if (v->type() == MIRType::RefOrNull) {
        valueAddr = MWasmDerivedPointer::New(
            alloc(), tlsPointer_,
            offsetof(wasm::TlsData, globalArea) + globalDataOffset);
        curBlock_->add(valueAddr);
        store = MWasmStoreRef::New(alloc(), tlsPointer_, valueAddr, v,
                                   AliasSet::WasmGlobalVar);
      } else {
        store =
            MWasmStoreGlobalVar::New(alloc(), globalDataOffset, v, tlsPointer_);
      }
    }
    curBlock_->add(store);

    return valueAddr;
  }

  void addInterruptCheck() {
    if (inDeadCode()) {
      return;
    }
    curBlock_->add(
        MWasmInterruptCheck::New(alloc(), tlsPointer_, bytecodeOffset()));
  }

  /***************************************************************** Calls */

  // The IonMonkey backend maintains a single stack offset (from the stack
  // pointer to the base of the frame) by adding the total amount of spill
  // space required plus the maximum stack required for argument passing.
  // Since we do not use IonMonkey's MPrepareCall/MPassArg/MCall, we must
  // manually accumulate, for the entire function, the maximum required stack
  // space for argument passing. (This is passed to the CodeGenerator via
  // MIRGenerator::maxWasmStackArgBytes.) This is just be the maximum of the
  // stack space required for each individual call (as determined by the call
  // ABI).

  // Operations that modify a CallCompileState.

  bool passInstance(MIRType instanceType, CallCompileState* args) {
    if (inDeadCode()) {
      return true;
    }

    // Should only pass an instance once.  And it must be a non-GC pointer.
    MOZ_ASSERT(args->instanceArg_ == ABIArg());
    MOZ_ASSERT(instanceType == MIRType::Pointer);
    args->instanceArg_ = args->abi_.next(MIRType::Pointer);
    return true;
  }

  // Do not call this directly.  Call one of the passArg() variants instead.
  bool passArgWorker(MDefinition* argDef, MIRType type,
                     CallCompileState* call) {
    ABIArg arg = call->abi_.next(type);
    switch (arg.kind()) {
#ifdef JS_CODEGEN_REGISTER_PAIR
      case ABIArg::GPR_PAIR: {
        auto mirLow =
            MWrapInt64ToInt32::New(alloc(), argDef, /* bottomHalf = */ true);
        curBlock_->add(mirLow);
        auto mirHigh =
            MWrapInt64ToInt32::New(alloc(), argDef, /* bottomHalf = */ false);
        curBlock_->add(mirHigh);
        return call->regArgs_.append(
                   MWasmCall::Arg(AnyRegister(arg.gpr64().low), mirLow)) &&
               call->regArgs_.append(
                   MWasmCall::Arg(AnyRegister(arg.gpr64().high), mirHigh));
      }
#endif
      case ABIArg::GPR:
      case ABIArg::FPU:
        return call->regArgs_.append(MWasmCall::Arg(arg.reg(), argDef));
      case ABIArg::Stack: {
        auto* mir =
            MWasmStackArg::New(alloc(), arg.offsetFromArgBase(), argDef);
        curBlock_->add(mir);
        return true;
      }
      case ABIArg::Uninitialized:
        MOZ_ASSERT_UNREACHABLE("Uninitialized ABIArg kind");
    }
    MOZ_CRASH("Unknown ABIArg kind.");
  }

  template <typename SpanT>
  bool passArgs(const DefVector& argDefs, SpanT types, CallCompileState* call) {
    MOZ_ASSERT(argDefs.length() == types.size());
    for (uint32_t i = 0; i < argDefs.length(); i++) {
      MDefinition* def = argDefs[i];
      ValType type = types[i];
      if (!passArg(def, type, call)) {
        return false;
      }
    }
    return true;
  }

  bool passArg(MDefinition* argDef, MIRType type, CallCompileState* call) {
    if (inDeadCode()) {
      return true;
    }
    return passArgWorker(argDef, type, call);
  }

  bool passArg(MDefinition* argDef, ValType type, CallCompileState* call) {
    if (inDeadCode()) {
      return true;
    }
    return passArgWorker(argDef, ToMIRType(type), call);
  }

  // If the call returns results on the stack, prepare a stack area to receive
  // them, and pass the address of the stack area to the callee as an additional
  // argument.
  bool passStackResultAreaCallArg(const ResultType& resultType,
                                  CallCompileState* call) {
    if (inDeadCode()) {
      return true;
    }
    ABIResultIter iter(resultType);
    while (!iter.done() && iter.cur().inRegister()) {
      iter.next();
    }
    if (iter.done()) {
      // No stack results.
      return true;
    }

    auto* stackResultArea = MWasmStackResultArea::New(alloc());
    if (!stackResultArea) {
      return false;
    }
    if (!stackResultArea->init(alloc(), iter.remaining())) {
      return false;
    }
    for (uint32_t base = iter.index(); !iter.done(); iter.next()) {
      MWasmStackResultArea::StackResult loc(iter.cur().stackOffset(),
                                            ToMIRType(iter.cur().type()));
      stackResultArea->initResult(iter.index() - base, loc);
    }
    curBlock_->add(stackResultArea);
    if (!passArg(stackResultArea, MIRType::Pointer, call)) {
      return false;
    }
    call->stackResultArea_ = stackResultArea;
    return true;
  }

  bool finishCall(CallCompileState* call) {
    if (inDeadCode()) {
      return true;
    }

    if (!call->regArgs_.append(
            MWasmCall::Arg(AnyRegister(WasmTlsReg), tlsPointer_))) {
      return false;
    }

    uint32_t stackBytes = call->abi_.stackBytesConsumedSoFar();

    maxStackArgBytes_ = std::max(maxStackArgBytes_, stackBytes);
    return true;
  }

  // Wrappers for creating various kinds of calls.

  bool collectUnaryCallResult(MIRType type, MDefinition** result) {
    MInstruction* def;
    switch (type) {
      case MIRType::Int32:
        def = MWasmRegisterResult::New(alloc(), MIRType::Int32, ReturnReg);
        break;
      case MIRType::Int64:
        def = MWasmRegister64Result::New(alloc(), ReturnReg64);
        break;
      case MIRType::Float32:
        def = MWasmFloatRegisterResult::New(alloc(), type, ReturnFloat32Reg);
        break;
      case MIRType::Double:
        def = MWasmFloatRegisterResult::New(alloc(), type, ReturnDoubleReg);
        break;
#ifdef ENABLE_WASM_SIMD
      case MIRType::Simd128:
        def = MWasmFloatRegisterResult::New(alloc(), type, ReturnSimd128Reg);
        break;
#endif
      case MIRType::RefOrNull:
        def = MWasmRegisterResult::New(alloc(), MIRType::RefOrNull, ReturnReg);
        break;
      default:
        MOZ_CRASH("unexpected MIRType result for builtin call");
    }

    if (!def) {
      return false;
    }

    curBlock_->add(def);
    *result = def;

    return true;
  }

  bool collectCallResults(const ResultType& type,
                          MWasmStackResultArea* stackResultArea,
                          DefVector* results) {
    if (!results->reserve(type.length())) {
      return false;
    }

    // The result iterator goes in the order in which results would be popped
    // off; we want the order in which they would be pushed.
    ABIResultIter iter(type);
    uint32_t stackResultCount = 0;
    while (!iter.done()) {
      if (iter.cur().onStack()) {
        stackResultCount++;
      }
      iter.next();
    }

    for (iter.switchToPrev(); !iter.done(); iter.prev()) {
      if (!mirGen().ensureBallast()) {
        return false;
      }
      const ABIResult& result = iter.cur();
      MInstruction* def;
      if (result.inRegister()) {
        switch (result.type().kind()) {
          case wasm::ValType::I32:
            def =
                MWasmRegisterResult::New(alloc(), MIRType::Int32, result.gpr());
            break;
          case wasm::ValType::I64:
            def = MWasmRegister64Result::New(alloc(), result.gpr64());
            break;
          case wasm::ValType::F32:
            def = MWasmFloatRegisterResult::New(alloc(), MIRType::Float32,
                                                result.fpr());
            break;
          case wasm::ValType::F64:
            def = MWasmFloatRegisterResult::New(alloc(), MIRType::Double,
                                                result.fpr());
            break;
          case wasm::ValType::Rtt:
          case wasm::ValType::Ref:
            def = MWasmRegisterResult::New(alloc(), MIRType::RefOrNull,
                                           result.gpr());
            break;
          case wasm::ValType::V128:
#ifdef ENABLE_WASM_SIMD
            def = MWasmFloatRegisterResult::New(alloc(), MIRType::Simd128,
                                                result.fpr());
#else
            return this->iter().fail("Ion has no SIMD support yet");
#endif
        }
      } else {
        MOZ_ASSERT(stackResultArea);
        MOZ_ASSERT(stackResultCount);
        uint32_t idx = --stackResultCount;
        def = MWasmStackResult::New(alloc(), stackResultArea, idx);
      }

      if (!def) {
        return false;
      }
      curBlock_->add(def);
      results->infallibleAppend(def);
    }

    MOZ_ASSERT(results->length() == type.length());

    return true;
  }

  bool callDirect(const FuncType& funcType, uint32_t funcIndex,
                  uint32_t lineOrBytecode, const CallCompileState& call,
                  DefVector* results) {
    if (inDeadCode()) {
      return true;
    }

    CallSiteDesc desc(lineOrBytecode, CallSiteDesc::Func);
    ResultType resultType = ResultType::Vector(funcType.results());
    auto callee = CalleeDesc::function(funcIndex);
    ArgTypeVector args(funcType);
    bool inTry = false;
#ifdef ENABLE_WASM_EXCEPTIONS
    // If we are in Wasm try code, this call must initialise a WasmTryNote
    // during code generation. This flag is set here.
    inTry = inTryCode();
#endif
    auto* ins = MWasmCall::New(alloc(), desc, callee, call.regArgs_,
                               StackArgAreaSizeUnaligned(args), inTry);
    if (!ins) {
      return false;
    }

    curBlock_->add(ins);

    return collectCallResults(resultType, call.stackResultArea_, results);
  }

  bool callIndirect(uint32_t funcTypeIndex, uint32_t tableIndex,
                    MDefinition* index, uint32_t lineOrBytecode,
                    const CallCompileState& call, DefVector* results) {
    if (inDeadCode()) {
      return true;
    }

    const FuncType& funcType = (*moduleEnv_.types)[funcTypeIndex].funcType();
    const TypeIdDesc& funcTypeId = moduleEnv_.typeIds[funcTypeIndex];

    CalleeDesc callee;
    if (moduleEnv_.isAsmJS()) {
      MOZ_ASSERT(tableIndex == 0);
      MOZ_ASSERT(funcTypeId.kind() == TypeIdDescKind::None);
      const TableDesc& table =
          moduleEnv_.tables[moduleEnv_.asmJSSigToTableIndex[funcTypeIndex]];
      MOZ_ASSERT(IsPowerOfTwo(table.initialLength));

      MConstant* mask =
          MConstant::New(alloc(), Int32Value(table.initialLength - 1));
      curBlock_->add(mask);
      MBitAnd* maskedIndex = MBitAnd::New(alloc(), index, mask, MIRType::Int32);
      curBlock_->add(maskedIndex);

      index = maskedIndex;
      callee = CalleeDesc::asmJSTable(table);
    } else {
      MOZ_ASSERT(funcTypeId.kind() != TypeIdDescKind::None);
      const TableDesc& table = moduleEnv_.tables[tableIndex];
      callee = CalleeDesc::wasmTable(table, funcTypeId);
    }

    CallSiteDesc desc(lineOrBytecode, CallSiteDesc::Indirect);
    ArgTypeVector args(funcType);
    ResultType resultType = ResultType::Vector(funcType.results());
    bool inTry = false;
#ifdef ENABLE_WASM_EXCEPTIONS
    // If we are in Wasm try code, this call must initialise a WasmTryNote
    // during code generation. This flag is set here.
    inTry = inTryCode();
#endif
    auto* ins = MWasmCall::New(alloc(), desc, callee, call.regArgs_,
                               StackArgAreaSizeUnaligned(args), inTry, index);
    if (!ins) {
      return false;
    }

    curBlock_->add(ins);

    return collectCallResults(resultType, call.stackResultArea_, results);
  }

  bool callImport(unsigned globalDataOffset, uint32_t lineOrBytecode,
                  const CallCompileState& call, const FuncType& funcType,
                  DefVector* results) {
    if (inDeadCode()) {
      return true;
    }

    CallSiteDesc desc(lineOrBytecode, CallSiteDesc::Import);
    auto callee = CalleeDesc::import(globalDataOffset);
    ArgTypeVector args(funcType);
    ResultType resultType = ResultType::Vector(funcType.results());
    bool inTry = false;
#ifdef ENABLE_WASM_EXCEPTIONS
    // If we are in Wasm try code, this call must initialise a WasmTryNote
    // during code generation. This flag is set here.
    inTry = inTryCode();
#endif
    auto* ins = MWasmCall::New(alloc(), desc, callee, call.regArgs_,
                               StackArgAreaSizeUnaligned(args), inTry);
    if (!ins) {
      return false;
    }

    curBlock_->add(ins);

    return collectCallResults(resultType, call.stackResultArea_, results);
  }

  bool builtinCall(const SymbolicAddressSignature& builtin,
                   uint32_t lineOrBytecode, const CallCompileState& call,
                   MDefinition** def) {
    if (inDeadCode()) {
      *def = nullptr;
      return true;
    }

    MOZ_ASSERT(builtin.failureMode == FailureMode::Infallible);

    CallSiteDesc desc(lineOrBytecode, CallSiteDesc::Symbolic);
    auto callee = CalleeDesc::builtin(builtin.identity);
    auto* ins = MWasmCall::New(alloc(), desc, callee, call.regArgs_,
                               StackArgAreaSizeUnaligned(builtin), false);
    if (!ins) {
      return false;
    }

    curBlock_->add(ins);

    return collectUnaryCallResult(builtin.retType, def);
  }

  bool builtinInstanceMethodCall(const SymbolicAddressSignature& builtin,
                                 uint32_t lineOrBytecode,
                                 const CallCompileState& call,
                                 MDefinition** def = nullptr) {
    MOZ_ASSERT_IF(!def, builtin.retType == MIRType::None);
    if (inDeadCode()) {
      if (def) {
        *def = nullptr;
      }
      return true;
    }

    CallSiteDesc desc(lineOrBytecode, CallSiteDesc::Symbolic);
    auto* ins = MWasmCall::NewBuiltinInstanceMethodCall(
        alloc(), desc, builtin.identity, builtin.failureMode, call.instanceArg_,
        call.regArgs_, StackArgAreaSizeUnaligned(builtin));
    if (!ins) {
      return false;
    }

    curBlock_->add(ins);

    return def ? collectUnaryCallResult(builtin.retType, def) : true;
  }

  /*********************************************** Control flow generation */

  inline bool inDeadCode() const { return curBlock_ == nullptr; }

  bool returnValues(const DefVector& values) {
    if (inDeadCode()) {
      return true;
    }

    if (values.empty()) {
      curBlock_->end(MWasmReturnVoid::New(alloc(), tlsPointer_));
    } else {
      ResultType resultType = ResultType::Vector(funcType().results());
      ABIResultIter iter(resultType);
      // Switch to iterate in FIFO order instead of the default LIFO.
      while (!iter.done()) {
        iter.next();
      }
      iter.switchToPrev();
      for (uint32_t i = 0; !iter.done(); iter.prev(), i++) {
        if (!mirGen().ensureBallast()) {
          return false;
        }
        const ABIResult& result = iter.cur();
        if (result.onStack()) {
          MOZ_ASSERT(iter.remaining() > 1);
          if (result.type().isRefRepr()) {
            auto* loc = MWasmDerivedPointer::New(alloc(), stackResultPointer_,
                                                 result.stackOffset());
            curBlock_->add(loc);
            auto* store =
                MWasmStoreRef::New(alloc(), tlsPointer_, loc, values[i],
                                   AliasSet::WasmStackResult);
            curBlock_->add(store);
          } else {
            auto* store = MWasmStoreStackResult::New(
                alloc(), stackResultPointer_, result.stackOffset(), values[i]);
            curBlock_->add(store);
          }
        } else {
          MOZ_ASSERT(iter.remaining() == 1);
          MOZ_ASSERT(i + 1 == values.length());
          curBlock_->end(MWasmReturn::New(alloc(), values[i], tlsPointer_));
        }
      }
    }
    curBlock_ = nullptr;
    return true;
  }

  void unreachableTrap() {
    if (inDeadCode()) {
      return;
    }

    auto* ins =
        MWasmTrap::New(alloc(), wasm::Trap::Unreachable, bytecodeOffset());
    curBlock_->end(ins);
    curBlock_ = nullptr;
  }

 private:
  static uint32_t numPushed(MBasicBlock* block) {
    return block->stackDepth() - block->info().firstStackSlot();
  }

 public:
  [[nodiscard]] bool pushDefs(const DefVector& defs) {
    if (inDeadCode()) {
      return true;
    }
    MOZ_ASSERT(numPushed(curBlock_) == 0);
    if (!curBlock_->ensureHasSlots(defs.length())) {
      return false;
    }
    for (MDefinition* def : defs) {
      MOZ_ASSERT(def->type() != MIRType::None);
      curBlock_->push(def);
    }
    return true;
  }

  bool popPushedDefs(DefVector* defs) {
    size_t n = numPushed(curBlock_);
    if (!defs->resizeUninitialized(n)) {
      return false;
    }
    for (; n > 0; n--) {
      MDefinition* def = curBlock_->pop();
      MOZ_ASSERT(def->type() != MIRType::Value);
      (*defs)[n - 1] = def;
    }
    return true;
  }

 private:
  bool addJoinPredecessor(const DefVector& defs, MBasicBlock** joinPred) {
    *joinPred = curBlock_;
    if (inDeadCode()) {
      return true;
    }
    return pushDefs(defs);
  }

 public:
  bool branchAndStartThen(MDefinition* cond, MBasicBlock** elseBlock) {
    if (inDeadCode()) {
      *elseBlock = nullptr;
    } else {
      MBasicBlock* thenBlock;
      if (!newBlock(curBlock_, &thenBlock)) {
        return false;
      }
      if (!newBlock(curBlock_, elseBlock)) {
        return false;
      }

      curBlock_->end(MTest::New(alloc(), cond, thenBlock, *elseBlock));

      curBlock_ = thenBlock;
      mirGraph().moveBlockToEnd(curBlock_);
    }

    return startBlock();
  }

  bool switchToElse(MBasicBlock* elseBlock, MBasicBlock** thenJoinPred) {
    DefVector values;
    if (!finishBlock(&values)) {
      return false;
    }

    if (!elseBlock) {
      *thenJoinPred = nullptr;
    } else {
      if (!addJoinPredecessor(values, thenJoinPred)) {
        return false;
      }

      curBlock_ = elseBlock;
      mirGraph().moveBlockToEnd(curBlock_);
    }

    return startBlock();
  }

  bool joinIfElse(MBasicBlock* thenJoinPred, DefVector* defs) {
    DefVector values;
    if (!finishBlock(&values)) {
      return false;
    }

    if (!thenJoinPred && inDeadCode()) {
      return true;
    }

    MBasicBlock* elseJoinPred;
    if (!addJoinPredecessor(values, &elseJoinPred)) {
      return false;
    }

    mozilla::Array<MBasicBlock*, 2> blocks;
    size_t numJoinPreds = 0;
    if (thenJoinPred) {
      blocks[numJoinPreds++] = thenJoinPred;
    }
    if (elseJoinPred) {
      blocks[numJoinPreds++] = elseJoinPred;
    }

    if (numJoinPreds == 0) {
      return true;
    }

    MBasicBlock* join;
    if (!goToNewBlock(blocks[0], &join)) {
      return false;
    }
    for (size_t i = 1; i < numJoinPreds; ++i) {
      if (!goToExistingBlock(blocks[i], join)) {
        return false;
      }
    }

    curBlock_ = join;
    return popPushedDefs(defs);
  }

  bool startBlock() {
    MOZ_ASSERT_IF(blockDepth_ < blockPatches_.length(),
                  blockPatches_[blockDepth_].empty());
    blockDepth_++;
    return true;
  }

  bool finishBlock(DefVector* defs) {
    MOZ_ASSERT(blockDepth_);
    uint32_t topLabel = --blockDepth_;
    return bindBranches(topLabel, defs);
  }

  bool startLoop(MBasicBlock** loopHeader, size_t paramCount) {
    *loopHeader = nullptr;

    blockDepth_++;
    loopDepth_++;

    if (inDeadCode()) {
      return true;
    }

    // Create the loop header.
    MOZ_ASSERT(curBlock_->loopDepth() == loopDepth_ - 1);
    *loopHeader = MBasicBlock::New(mirGraph(), info(), curBlock_,
                                   MBasicBlock::PENDING_LOOP_HEADER);
    if (!*loopHeader) {
      return false;
    }

    (*loopHeader)->setLoopDepth(loopDepth_);
    mirGraph().addBlock(*loopHeader);
    curBlock_->end(MGoto::New(alloc(), *loopHeader));

    DefVector loopParams;
    if (!iter().getResults(paramCount, &loopParams)) {
      return false;
    }
    for (size_t i = 0; i < paramCount; i++) {
      MPhi* phi = MPhi::New(alloc(), loopParams[i]->type());
      if (!phi) {
        return false;
      }
      if (!phi->reserveLength(2)) {
        return false;
      }
      (*loopHeader)->addPhi(phi);
      phi->addInput(loopParams[i]);
      loopParams[i] = phi;
    }
    iter().setResults(paramCount, loopParams);

    MBasicBlock* body;
    if (!goToNewBlock(*loopHeader, &body)) {
      return false;
    }
    curBlock_ = body;
    return true;
  }

 private:
  void fixupRedundantPhis(MBasicBlock* b) {
    for (size_t i = 0, depth = b->stackDepth(); i < depth; i++) {
      MDefinition* def = b->getSlot(i);
      if (def->isUnused()) {
        b->setSlot(i, def->toPhi()->getOperand(0));
      }
    }
  }

  bool setLoopBackedge(MBasicBlock* loopEntry, MBasicBlock* loopBody,
                       MBasicBlock* backedge, size_t paramCount) {
    if (!loopEntry->setBackedgeWasm(backedge, paramCount)) {
      return false;
    }

    // Flag all redundant phis as unused.
    for (MPhiIterator phi = loopEntry->phisBegin(); phi != loopEntry->phisEnd();
         phi++) {
      MOZ_ASSERT(phi->numOperands() == 2);
      if (phi->getOperand(0) == phi->getOperand(1)) {
        phi->setUnused();
      }
    }

    // Fix up phis stored in the slots Vector of pending blocks.
    for (ControlFlowPatchVector& patches : blockPatches_) {
      for (ControlFlowPatch& p : patches) {
        MBasicBlock* block = p.ins->block();
        if (block->loopDepth() >= loopEntry->loopDepth()) {
          fixupRedundantPhis(block);
        }
      }
    }

    // The loop body, if any, might be referencing recycled phis too.
    if (loopBody) {
      fixupRedundantPhis(loopBody);
    }

    // Pending jumps to an enclosing try-catch may reference the recycled phis.
    // We have to search above all enclosing try blocks, as a delegate may move
    // patches around.
    for (uint32_t depth = 0; depth < iter().controlStackDepth(); depth++) {
      if (iter().controlKind(depth) != LabelKind::Try) {
        continue;
      }
      Control& control = iter().controlItem(depth);
      for (MControlInstruction* patch : control.tryPadPatches) {
        MBasicBlock* block = patch->block();
        if (block->loopDepth() >= loopEntry->loopDepth()) {
          fixupRedundantPhis(block);
        }
      }
    }

    // Discard redundant phis and add to the free list.
    for (MPhiIterator phi = loopEntry->phisBegin();
         phi != loopEntry->phisEnd();) {
      MPhi* entryDef = *phi++;
      if (!entryDef->isUnused()) {
        continue;
      }

      entryDef->justReplaceAllUsesWith(entryDef->getOperand(0));
      loopEntry->discardPhi(entryDef);
      mirGraph().addPhiToFreeList(entryDef);
    }

    return true;
  }

 public:
  bool closeLoop(MBasicBlock* loopHeader, DefVector* loopResults) {
    MOZ_ASSERT(blockDepth_ >= 1);
    MOZ_ASSERT(loopDepth_);

    uint32_t headerLabel = blockDepth_ - 1;

    if (!loopHeader) {
      MOZ_ASSERT(inDeadCode());
      MOZ_ASSERT(headerLabel >= blockPatches_.length() ||
                 blockPatches_[headerLabel].empty());
      blockDepth_--;
      loopDepth_--;
      return true;
    }

    // Op::Loop doesn't have an implicit backedge so temporarily set
    // aside the end of the loop body to bind backedges.
    MBasicBlock* loopBody = curBlock_;
    curBlock_ = nullptr;

    // As explained in bug 1253544, Ion apparently has an invariant that
    // there is only one backedge to loop headers. To handle wasm's ability
    // to have multiple backedges to the same loop header, we bind all those
    // branches as forward jumps to a single backward jump. This is
    // unfortunate but the optimizer is able to fold these into single jumps
    // to backedges.
    DefVector backedgeValues;
    if (!bindBranches(headerLabel, &backedgeValues)) {
      return false;
    }

    MOZ_ASSERT(loopHeader->loopDepth() == loopDepth_);

    if (curBlock_) {
      // We're on the loop backedge block, created by bindBranches.
      for (size_t i = 0, n = numPushed(curBlock_); i != n; i++) {
        curBlock_->pop();
      }

      if (!pushDefs(backedgeValues)) {
        return false;
      }

      MOZ_ASSERT(curBlock_->loopDepth() == loopDepth_);
      curBlock_->end(MGoto::New(alloc(), loopHeader));
      if (!setLoopBackedge(loopHeader, loopBody, curBlock_,
                           backedgeValues.length())) {
        return false;
      }
    }

    curBlock_ = loopBody;

    loopDepth_--;

    // If the loop depth still at the inner loop body, correct it.
    if (curBlock_ && curBlock_->loopDepth() != loopDepth_) {
      MBasicBlock* out;
      if (!goToNewBlock(curBlock_, &out)) {
        return false;
      }
      curBlock_ = out;
    }

    blockDepth_ -= 1;
    return inDeadCode() || popPushedDefs(loopResults);
  }

  bool addControlFlowPatch(MControlInstruction* ins, uint32_t relative,
                           uint32_t index) {
    MOZ_ASSERT(relative < blockDepth_);
    uint32_t absolute = blockDepth_ - 1 - relative;

    if (absolute >= blockPatches_.length() &&
        !blockPatches_.resize(absolute + 1)) {
      return false;
    }

    return blockPatches_[absolute].append(ControlFlowPatch(ins, index));
  }

  bool br(uint32_t relativeDepth, const DefVector& values) {
    if (inDeadCode()) {
      return true;
    }

    MGoto* jump = MGoto::New(alloc());
    if (!addControlFlowPatch(jump, relativeDepth, MGoto::TargetIndex)) {
      return false;
    }

    if (!pushDefs(values)) {
      return false;
    }

    curBlock_->end(jump);
    curBlock_ = nullptr;
    return true;
  }

  bool brIf(uint32_t relativeDepth, const DefVector& values,
            MDefinition* condition) {
    if (inDeadCode()) {
      return true;
    }

    MBasicBlock* joinBlock = nullptr;
    if (!newBlock(curBlock_, &joinBlock)) {
      return false;
    }

    MTest* test = MTest::New(alloc(), condition, joinBlock);
    if (!addControlFlowPatch(test, relativeDepth, MTest::TrueBranchIndex)) {
      return false;
    }

    if (!pushDefs(values)) {
      return false;
    }

    curBlock_->end(test);
    curBlock_ = joinBlock;
    return true;
  }

  bool brTable(MDefinition* operand, uint32_t defaultDepth,
               const Uint32Vector& depths, const DefVector& values) {
    if (inDeadCode()) {
      return true;
    }

    size_t numCases = depths.length();
    MOZ_ASSERT(numCases <= INT32_MAX);
    MOZ_ASSERT(numCases);

    MTableSwitch* table =
        MTableSwitch::New(alloc(), operand, 0, int32_t(numCases - 1));

    size_t defaultIndex;
    if (!table->addDefault(nullptr, &defaultIndex)) {
      return false;
    }
    if (!addControlFlowPatch(table, defaultDepth, defaultIndex)) {
      return false;
    }

    using IndexToCaseMap =
        HashMap<uint32_t, uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy>;

    IndexToCaseMap indexToCase;
    if (!indexToCase.put(defaultDepth, defaultIndex)) {
      return false;
    }

    for (size_t i = 0; i < numCases; i++) {
      uint32_t depth = depths[i];

      size_t caseIndex;
      IndexToCaseMap::AddPtr p = indexToCase.lookupForAdd(depth);
      if (!p) {
        if (!table->addSuccessor(nullptr, &caseIndex)) {
          return false;
        }
        if (!addControlFlowPatch(table, depth, caseIndex)) {
          return false;
        }
        if (!indexToCase.add(p, depth, caseIndex)) {
          return false;
        }
      } else {
        caseIndex = p->value();
      }

      if (!table->addCase(caseIndex)) {
        return false;
      }
    }

    if (!pushDefs(values)) {
      return false;
    }

    curBlock_->end(table);
    curBlock_ = nullptr;

    return true;
  }

  /********************************************************** Exceptions ***/

#ifdef ENABLE_WASM_EXCEPTIONS
  bool inTryBlock(uint32_t* relativeDepth) {
    return iter().controlFindInnermost(LabelKind::Try, relativeDepth);
  }

  bool inTryCode() {
    uint32_t relativeDepth;
    return inTryBlock(&relativeDepth);
  }

  bool clearExceptionGetTag(MDefinition** tagIndex) {
    // This clears the pending exception from the tls data and returns the
    // exception's local tag index.
    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
    const SymbolicAddressSignature& callee = SASigConsumePendingException;
    CallCompileState args;
    if (!passInstance(callee.argTypes[0], &args)) {
      return false;
    }
    if (!finishCall(&args)) {
      return false;
    }
    return builtinInstanceMethodCall(callee, lineOrBytecode, args, tagIndex);
  }

  bool addPadPatch(MControlInstruction* ins, size_t relativeTryDepth) {
    Control& tryControl = iter().controlItem(relativeTryDepth);
    ControlInstructionVector& padPatches = tryControl.tryPadPatches;
    return padPatches.emplaceBack(ins);
  }

  bool endWithPadPatch(MBasicBlock* block, MDefinition* exn,
                       MDefinition* tagIndex, uint32_t relativeTryDepth) {
    MOZ_ASSERT(iter().controlKind(relativeTryDepth) == LabelKind::Try);
    MOZ_ASSERT(exn);
    MOZ_ASSERT(exn->type() == MIRType::RefOrNull);
    MOZ_ASSERT(tagIndex && tagIndex->type() == MIRType::Int32);
    MOZ_ASSERT(numPushed(block) == 0);

    // Push the exception and its tag index on the stack to make them available
    // to the landing pad.
    if (!block->ensureHasSlots(2)) {
      return false;
    }
    block->push(exn);
    block->push(tagIndex);

    MGoto* insToPatch = MGoto::New(alloc());
    block->end(insToPatch);

    return addPadPatch(insToPatch, relativeTryDepth);
  }

  bool checkPendingExceptionAndBranch(uint32_t relativeTryDepth) {
    // Assuming we're in a Wasm try block, branch to a new pre-pad block, if
    // there exists a pendingException in the Wasm TlsData.

    MOZ_ASSERT(inTryCode());

    // Get the contents of pendingException from the Wasm TlsData.
    MWasmLoadTls* pendingException = MWasmLoadTls::New(
        alloc(), tlsPointer_, offsetof(wasm::TlsData, pendingException),
        MIRType::RefOrNull, AliasSet::Load(AliasSet::WasmPendingException));
    curBlock_->add(pendingException);

    // Set up a test to see if there was a pending exception or not.
    MBasicBlock* fallthroughBlock = nullptr;
    if (!newBlock(curBlock_, &fallthroughBlock)) {
      return false;
    }
    MBasicBlock* prePadBlock = nullptr;
    if (!newBlock(curBlock_, &prePadBlock)) {
      return false;
    }
    MDefinition* nullVal = nullRefConstant();
    // We use a not-equal comparison to benefit the non-exceptional common case.
    MDefinition* pendingExceptionIsNotNull = compare(
        pendingException, nullVal, JSOp::Ne, MCompare::Compare_RefOrNull);

    // Here we don't null check nullVal and pendingExceptionIsNull because the
    // temp allocator ballast should make allocation infallible.

    MTest* branchIfNull = MTest::New(alloc(), pendingExceptionIsNotNull,
                                     prePadBlock, fallthroughBlock);
    curBlock_->end(branchIfNull);
    curBlock_ = prePadBlock;

    // Clear pending exception and get the exceptions local tag index.
    MDefinition* tagIndex = nullptr;
    if (!clearExceptionGetTag(&tagIndex)) {
      return false;
    }

    // Finish the prePadBlock with a patch.
    if (!endWithPadPatch(prePadBlock, pendingException, tagIndex,
                         relativeTryDepth)) {
      return false;
    }

    // Compilation continues in the fallthroughBlock.
    curBlock_ = fallthroughBlock;
    return true;
  }

  bool maybeCheckPendingExceptionAfterCall() {
    uint32_t relativeTryDepth;
    return !inTryBlock(&relativeTryDepth) ||
           checkPendingExceptionAndBranch(relativeTryDepth);
  }

  // If there are throws or calls in the try block, then there are stored
  // pad-patches (a ControlInstructionVector) for this control item. The
  // following function binds these control instructions (branches) to a join
  // which will become the landing pad, and also become the curBlock_.
  //
  // For the latter to work, the last block in the try code (the curBlock_)
  // should be either dead code or finished before maybeCreateTryPadBlock gets
  // called. This function should only be called when try code ends.
  bool maybeCreateTryPadBlock(Control& catching) {
    // Make sure the last block in try code is finished.
    MOZ_ASSERT(inDeadCode() || curBlock_->hasLastIns());

    // If there are no pad-patches for this try control, it means there are no
    // instructions in the try code that could throw a Wasm exception. In this
    // case, all the catches are dead code, and the try code ends up equivalent
    // to a plain Wasm block.
    ControlInstructionVector& patches = catching.tryPadPatches;
    if (patches.empty()) {
      curBlock_ = nullptr;
      return true;
    }

    // Otherwise, if there are (pad-) branches from places in the try code that
    // may throw a Wasm exception, bind these branches to a new landing pad
    // block. This is done similarly to what is done in bindBranches.
    MControlInstruction* ins = patches[0];
    MBasicBlock* pred = ins->block();
    MBasicBlock* pad = nullptr;
    if (!newBlock(pred, &pad)) {
      return false;
    }
    ins->replaceSuccessor(0, pad);
    for (size_t i = 1; i < patches.length(); i++) {
      ins = patches[i];
      pred = ins->block();
      if (!pad->addPredecessor(alloc(), pred)) {
        return false;
      }
      ins->replaceSuccessor(0, pad);
    }

    // At this point we have finished the try or previous catch block, with a
    // control flow patch to be joined with the end if each catch block. We are
    // now ready to start the landing pad, which will eventually branch to each
    // catch block.
    curBlock_ = pad;
    mirGraph().moveBlockToEnd(curBlock_);
    mirGraph().setHasTryBlock();

    // Clear the now bound pad patches.
    patches.clear();
    return true;
  }

  bool emitTry(MBasicBlock** curBlock) {
    *curBlock = curBlock_;
    return startBlock();
  }

  bool finishTryOrCatchBlock(Control& control) {
    if (inDeadCode()) {
      return true;
    }

    // If we are not in dead code, then this is a split path which we'll need
    // to join later, using a control flow patch.
    MOZ_ASSERT(!curBlock_->hasLastIns());
    MGoto* jump = MGoto::New(alloc());
    if (!addControlFlowPatch(jump, 0, MGoto::TargetIndex)) {
      return false;
    }

    // Finish the current block with the control flow patch instruction.
    curBlock_->end(jump);
    return true;
  }

  bool switchToCatch(const LabelKind& kind, uint32_t tagIndex,
                     Control& control) {
    // Finish the previous block (either a try or catch block) and then setup a
    // new catch block.

    // If there is no control block (which is the entry block for `try` and the
    // landing pad block for `catch`/`catch_all`) then we are in dead code.
    if (!control.block) {
      MOZ_ASSERT(inDeadCode());
      return true;
    }

    if (!finishTryOrCatchBlock(control)) {
      return false;
    }

    // Finish a try block by emitting a landing pad if there was any code that
    // may throw.
    if (kind == LabelKind::Try) {
      if (!maybeCreateTryPadBlock(control)) {
        return false;
      }

      // The landing pad becomes the control block.
      control.block = curBlock_;

      // If there is no landing pad created, the catches are dead code.
      if (curBlock_ == nullptr) {
        return true;
      }

      // If there is a landing pad, then it has exactly two slots pushed, the
      // caught exception and its tag index.
      MOZ_ASSERT(numPushed(curBlock_) == 2);

      // If this is a single catch_all after a try block then we don't need the
      // exception nor its tag index. So we pop these and there's nothing else
      // to do.
      if (tagIndex == CatchAllIndex) {
        MBasicBlock* catchAllBlock = nullptr;
        if (!goToNewBlock(curBlock_, &catchAllBlock)) {
          return false;
        }
        control.catchAllBlock = catchAllBlock;
        curBlock_ = catchAllBlock;
        curBlock_->pop();
        curBlock_->pop();
        return true;
      }

      MOZ_ASSERT(control.tryCatches.empty());
    }

    // Get the landing pad.
    MBasicBlock* padBlock = control.block;

    // If this is not a catch_all and if tagIndex is already handled, then this
    // catch is dead.
    if (tagIndex != CatchAllIndex && control.tagAlreadyHandled(tagIndex)) {
      curBlock_ = nullptr;
      return true;
    }

    // Create a new block for the next catch.
    MBasicBlock* nextCatch = nullptr;
    if (!newBlock(padBlock, &nextCatch)) {
      return false;
    }

    // If this is a catch_all, mark it in the control as such, otherwise collect
    // the catch info into the control's tryCatches.
    if (tagIndex == CatchAllIndex) {
      control.catchAllBlock = nextCatch;
    } else {
      CatchInfo catchInfo(tagIndex, nextCatch);
      if (!control.tryCatches.emplaceBack(catchInfo)) {
        return false;
      }
    }

    // Pop the exception, extract the exception values if necessary, and
    // continue with the instructions in the next catch.
    curBlock_ = nextCatch;
    mirGraph().moveBlockToEnd(curBlock_);
    // Pop the tag index, which we don't need, to get to the exception object.
    curBlock_->pop();
    MDefinition* exn = curBlock_->pop();
    MOZ_ASSERT(exn->type() == MIRType::RefOrNull);

    // Nothing left to do for a catch_all block, as it gets no params.
    if (tagIndex == CatchAllIndex) {
      return true;
    }

    // Since this is not a catch_all, extract the exception values.
    const TagType& tagType = moduleEnv().tags[tagIndex].type;
    const ValTypeVector& tagParams = tagType.argTypes;
    const TagOffsetVector& offsets = tagType.argOffsets;

    MWasmExceptionDataPointer* exnDataPtr =
        MWasmExceptionDataPointer::New(alloc(), exn);
    curBlock_->add(exnDataPtr);
    MWasmExceptionRefsPointer* exnRefsPtr =
        MWasmExceptionRefsPointer::New(alloc(), exn, tagType.refCount);
    curBlock_->add(exnRefsPtr);

    MIRType type;
    size_t count = tagParams.length();
    DefVector loadedValues;
    // Presize the loadedValues vector to the amount of params.
    if (!loadedValues.reserve(count)) {
      return false;
    }

    for (size_t i = 0; i < count; i++) {
      int32_t offset = offsets[i];
      type = ToMIRType(tagParams[i]);
      if (IsNumberType(type) || tagParams[i].kind() == ValType::V128) {
        auto* load =
            MWasmLoadExceptionDataValue::New(alloc(), exnDataPtr, offset, type);
        if (!load || !loadedValues.append(load)) {
          return false;
        }
        MOZ_ASSERT(load->type() != MIRType::None);
        curBlock_->add(load);
      } else {
        MOZ_ASSERT(tagParams[i].kind() == ValType::Rtt ||
                   tagParams[i].kind() == ValType::Ref);
        auto* load =
            MWasmLoadExceptionRefsValue::New(alloc(), exnRefsPtr, offset);
        if (!load || !loadedValues.append(load)) {
          return false;
        }
        MOZ_ASSERT(load->type() != MIRType::None);
        curBlock_->add(load);
      }
    }
    iter().setResults(count, loadedValues);
    return true;
  }

  bool delegatePadPatches(const ControlInstructionVector& delegatePadPatches,
                          uint32_t relativeDepth) {
    if (delegatePadPatches.empty()) {
      return true;
    }

    // Find where we are delegating the pad patches to.
    uint32_t targetRelativeDepth;
    if (!iter().controlFindInnermostFrom(LabelKind::Try, relativeDepth,
                                         &targetRelativeDepth)) {
      MOZ_ASSERT(relativeDepth <= blockDepth_ - 1);
      targetRelativeDepth = blockDepth_ - 1;
    }
    // Append the delegate's pad patches to the target's.
    for (MControlInstruction* ins : delegatePadPatches) {
      if (!addPadPatch(ins, targetRelativeDepth)) {
        return false;
      }
    }
    return true;
  }

  bool finishCatchlessTry(Control& control) {
    // If a try has no catches and nothing that may throw, then we have no work
    // to do.
    if (control.tryPadPatches.empty()) {
      return true;
    }

    // Delegate all the throwing instructions to an enclosing try block if one
    // exists, or else to the body block which will handle it in
    // finishBodyDelegateThrowPad. We specify a relativeDepth of '1' to
    // delegate outside of the still active try block.
    uint32_t relativeDepth = 1;
    if (!delegatePadPatches(control.tryPadPatches, relativeDepth)) {
      return false;
    }
    return true;
  }

  bool finishBodyDelegateThrowPad(Control& control) {
    // If a function has no catches and nothing that may throw, then we have no
    // work to do.
    if (control.tryPadPatches.empty()) {
      return true;
    }

    // Note the curBlock_ to return to it after we create the landing pad.
    MBasicBlock* prevBlock = curBlock_;
    curBlock_ = nullptr;

    // Create a landing pad for the pad patches.
    if (!maybeCreateTryPadBlock(control)) {
      return false;
    }

    // If there are tryPadPatches (which we ensured in the beginning of this
    // function), then `maybeCreateTryPadBlock` should create a padBlock and set
    // curBlock_ to it. So we should not be in dead code but in the landing pad,
    // which should have two slots.
    MOZ_ASSERT(!inDeadCode());
    MOZ_ASSERT(numPushed(curBlock_) == 2);

    // So we are now in a landing pad resulting from pad patches. Get the caught
    // exception and its tag index, and rethrow.
    MDefinition* tagIndex = curBlock_->pop();
    MDefinition* exn = curBlock_->pop();
    if (!throwFrom(exn, tagIndex)) {
      return false;
    }

    // Return to the previous block.
    curBlock_ = prevBlock;
    return true;
  }

  bool finishCatches(LabelKind kind, Control& control) {
    MBasicBlock* padBlock = control.block;
    // If there is no landing pad, there's nothing to do.
    if (!padBlock) {
      return true;
    }

    // If there are no tryCatches then this is a single catch_all after a try,
    // and we don't create a table switch as we can just fallthrough.
    if (control.tryCatches.empty()) {
      MOZ_ASSERT(kind == LabelKind::CatchAll);
      MOZ_ASSERT(padBlock->hasLastIns());
      return true;
    }

    // Otherwise we end the landing pad with a table switch.

    // Put the curBlock_ aside while we set up the switch.
    MBasicBlock* prevBlock = curBlock_;

    // Switch to the landing pad.
    curBlock_ = padBlock;

    // Get the pushed exception and its tag index definition.
    MOZ_ASSERT(numPushed(curBlock_) == 2);
    MDefinition* tagIndex = curBlock_->pop();
    MDefinition* exn = curBlock_->pop();
    MOZ_ASSERT(exn && tagIndex);
    MOZ_ASSERT(tagIndex->type() == MIRType::Int32);
    MOZ_ASSERT(exn->type() == MIRType::RefOrNull);

    // Push the exception and its tag index, so the handlers and the
    // defaultCatch can access it.
    curBlock_->push(exn);
    curBlock_->push(tagIndex);

    // We're going to generate a table switch to branch to the target catch
    // block based off of the tag index of the caught exception. MTableSwitch
    // requires the default case to be '0', while the default case for tags is
    // UINT32_MAX. To resolve this difference, we add '1' to the incoming tag
    // index to force wraparound. This yields an 'adjusted tag index' that we
    // use below.
    //
    // For example:
    //   CatchAllIndex (UINT32_MAX) -> case 0
    //   tagIndex 0 -> case 1
    //   tagIndex n -> case n+1
    MDefinition* one = constant(Int32Value(1), MIRType::Int32);
    MDefinition* adjustedTagIndex = add(tagIndex, one, MIRType::Int32);
    uint32_t numTags = moduleEnv_.tags.length();

    // Set up a table switch test.
    MTableSwitch* table =
        MTableSwitch::New(alloc(), adjustedTagIndex, 0, (int32_t)numTags);

    // Get or create the default successor of the landing pad.
    MBasicBlock* defaultCatch = nullptr;
    if (kind == LabelKind::CatchAll) {
      MOZ_ASSERT(control.catchAllBlock);
      defaultCatch = control.catchAllBlock;
    } else {
      if (!newBlock(curBlock_, &defaultCatch)) {
        return false;
      }
      // Set up default catch behaviour.
      curBlock_ = defaultCatch;
      MDefinition* rethrowTagIndex = curBlock_->pop();
      MDefinition* rethrowExn = curBlock_->pop();
      if (!throwFrom(rethrowExn, rethrowTagIndex)) {
        return false;
      }
      curBlock_ = padBlock;
    }

    // Add the default catch to the table switch.
    size_t defaultIndex;
    if (!table->addDefault(defaultCatch, &defaultIndex)) {
      return false;
    }
    MOZ_ASSERT(defaultIndex == 0);
    using TagMap =
        HashMap<uint32_t, uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy>;

    TagMap tagMap;

    // Add the rest of the catches as table switch successors.
    for (CatchInfo& info : control.tryCatches) {
      uint32_t catchTagIndex = info.tagIndex;
      MBasicBlock* catchBlock = info.block;
      MOZ_ASSERT(catchTagIndex < numTags);
      MOZ_ASSERT(catchBlock);
      size_t switchIndex;
      if (!table->addSuccessor(catchBlock, &switchIndex)) {
        return false;
      }
      if (!tagMap.put(catchTagIndex, switchIndex)) {
        return false;
      }
    }

    // Add cases mapping from 'adjusted tag index' to tag successor to the
    // table.

    // Add the default case to the table.
    if (!table->addCase(0)) {
      return false;
    }

    // Add a case for each possible tag in the module.
    for (size_t catchTagIndex = 0; catchTagIndex < numTags; catchTagIndex++) {
      size_t switchIndex;
      TagMap::Ptr p = tagMap.lookup(catchTagIndex);
      switchIndex = p ? p->value() : 0;
      if (!table->addCase(switchIndex)) {
        return false;
      }
    }

    // End the landing pad with the table switch.
    curBlock_->end(table);

    // Return to the previous block.
    curBlock_ = prevBlock;
    if (prevBlock) {
      mirGraph().moveBlockToEnd(curBlock_);
    }

    return true;
  }

  bool emitThrow(uint32_t tagIndex, const DefVector& argValues) {
    if (inDeadCode()) {
      return true;
    }

    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
    const TagType& tagType = moduleEnv_.tags[tagIndex].type;
    const ResultType& tagParams = tagType.resultType();

    // First call an instance method to allocate a new WasmExceptionObject.
    MDefinition* tagIndexDef =
        constant(Int32Value(int32_t(tagIndex)), MIRType::Int32);
    MDefinition* exnSize =
        constant(Int32Value(tagType.bufferSize), MIRType::Int32);
    MDefinition* exn = nullptr;
    const SymbolicAddressSignature& callee = SASigExceptionNew;
    CallCompileState args;
    if (!passInstance(callee.argTypes[0], &args)) {
      return false;
    }
    if (!passArg(tagIndexDef, callee.argTypes[1], &args)) {
      return false;
    }
    if (!passArg(exnSize, callee.argTypes[2], &args)) {
      return false;
    }
    if (!finishCall(&args)) {
      return false;
    }
    if (!builtinInstanceMethodCall(callee, lineOrBytecode, args, &exn)) {
      return false;
    }
    MOZ_ASSERT(exn);

    // Then store the exception values.
    MWasmExceptionDataPointer* exnDataPtr =
        MWasmExceptionDataPointer::New(alloc(), exn);
    curBlock_->add(exnDataPtr);

    for (int32_t i = (int32_t)tagParams.length() - 1; i >= 0; i--) {
      int32_t offset = tagType.argOffsets[i];
      if (IsNumberType(tagParams[i]) || tagParams[i].kind() == ValType::V128) {
        MWasmStoreExceptionDataValue* store = MWasmStoreExceptionDataValue::New(
            alloc(), exnDataPtr, offset, argValues[i]);
        curBlock_->add(store);
      } else {
        MOZ_ASSERT(tagParams[i].kind() == ValType::Ref ||
                   tagParams[i].kind() == ValType::Rtt);
        MOZ_ASSERT(argValues[i]->type() != MIRType::None);

        const SymbolicAddressSignature& callee = SASigPushRefIntoExn;
        CallCompileState args;
        if (!passInstance(callee.argTypes[0], &args)) {
          return false;
        }
        if (!passArg(exn, callee.argTypes[1], &args)) {
          return false;
        }
        if (!passArg(argValues[i], callee.argTypes[2], &args)) {
          return false;
        }
        if (!finishCall(&args)) {
          return false;
        }
        if (!builtinInstanceMethodCall(callee, lineOrBytecode, args)) {
          return false;
        }
      }
    }

    // Throw the exception.
    return throwFrom(exn, tagIndexDef);
  }

  bool throwFrom(MDefinition* exn, MDefinition* tagIndex) {
    if (inDeadCode()) {
      return true;
    }

    // Check if there is a local catching try control, and if so, then add a
    // pad-patch to its tryPadPatches.
    uint32_t relativeTryDepth;
    if (inTryBlock(&relativeTryDepth)) {
      MBasicBlock* prePadBlock = nullptr;
      if (!newBlock(curBlock_, &prePadBlock)) {
        return false;
      }
      MGoto* ins = MGoto::New(alloc(), prePadBlock);

      // Finish the prePadBlock with a control flow (pad) patch.
      if (!endWithPadPatch(prePadBlock, exn, tagIndex, relativeTryDepth)) {
        return false;
      }
      curBlock_->end(ins);
      curBlock_ = nullptr;
      return true;
    }

    // If there is no surrounding catching block, call an instance method to
    // throw the exception.
    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
    const SymbolicAddressSignature& callee = SASigThrowException;
    CallCompileState args;
    if (!passInstance(callee.argTypes[0], &args)) {
      return false;
    }
    if (!passArg(exn, callee.argTypes[1], &args)) {
      return false;
    }
    if (!finishCall(&args)) {
      return false;
    }
    if (!builtinInstanceMethodCall(callee, lineOrBytecode, args)) {
      return false;
    }
    unreachableTrap();

    curBlock_ = nullptr;
    return true;
  }

  bool rethrow(uint32_t relativeDepth) {
    if (inDeadCode()) {
      return true;
    }

    Control& control = iter().controlItem(relativeDepth);
    MBasicBlock* pad = control.block;
    MOZ_ASSERT(pad);
    MOZ_ASSERT(pad->nslots() > 1);
    MOZ_ASSERT(iter().controlKind(relativeDepth) == LabelKind::Catch ||
               iter().controlKind(relativeDepth) == LabelKind::CatchAll);

    // The exception will always be the last slot in the landing pad.
    size_t exnSlotPosition = pad->nslots() - 2;
    MDefinition* tagIndex = pad->getSlot(exnSlotPosition + 1);
    MDefinition* exn = pad->getSlot(exnSlotPosition);
    MOZ_ASSERT(exn->type() == MIRType::RefOrNull &&
               tagIndex->type() == MIRType::Int32);
    return throwFrom(exn, tagIndex);
  }
#endif

  /************************************************************ DECODING ***/

  uint32_t readCallSiteLineOrBytecode() {
    if (!func_.callSiteLineNums.empty()) {
      return func_.callSiteLineNums[lastReadCallSite_++];
    }
    return iter_.lastOpcodeOffset();
  }

#if DEBUG
  bool done() const { return iter_.done(); }
#endif

  /*************************************************************************/
 private:
  bool newBlock(MBasicBlock* pred, MBasicBlock** block) {
    *block = MBasicBlock::New(mirGraph(), info(), pred, MBasicBlock::NORMAL);
    if (!*block) {
      return false;
    }
    mirGraph().addBlock(*block);
    (*block)->setLoopDepth(loopDepth_);
    return true;
  }

  bool goToNewBlock(MBasicBlock* pred, MBasicBlock** block) {
    if (!newBlock(pred, block)) {
      return false;
    }
    pred->end(MGoto::New(alloc(), *block));
    return true;
  }

  bool goToExistingBlock(MBasicBlock* prev, MBasicBlock* next) {
    MOZ_ASSERT(prev);
    MOZ_ASSERT(next);
    prev->end(MGoto::New(alloc(), next));
    return next->addPredecessor(alloc(), prev);
  }

  bool bindBranches(uint32_t absolute, DefVector* defs) {
    if (absolute >= blockPatches_.length() || blockPatches_[absolute].empty()) {
      return inDeadCode() || popPushedDefs(defs);
    }

    ControlFlowPatchVector& patches = blockPatches_[absolute];
    MControlInstruction* ins = patches[0].ins;
    MBasicBlock* pred = ins->block();

    MBasicBlock* join = nullptr;
    if (!newBlock(pred, &join)) {
      return false;
    }

    pred->mark();
    ins->replaceSuccessor(patches[0].index, join);

    for (size_t i = 1; i < patches.length(); i++) {
      ins = patches[i].ins;

      pred = ins->block();
      if (!pred->isMarked()) {
        if (!join->addPredecessor(alloc(), pred)) {
          return false;
        }
        pred->mark();
      }

      ins->replaceSuccessor(patches[i].index, join);
    }

    MOZ_ASSERT_IF(curBlock_, !curBlock_->isMarked());
    for (uint32_t i = 0; i < join->numPredecessors(); i++) {
      join->getPredecessor(i)->unmark();
    }

    if (curBlock_ && !goToExistingBlock(curBlock_, join)) {
      return false;
    }

    curBlock_ = join;

    if (!popPushedDefs(defs)) {
      return false;
    }

    patches.clear();
    return true;
  }
};

template <>
MDefinition* FunctionCompiler::unary<MToFloat32>(MDefinition* op) {
  if (inDeadCode()) {
    return nullptr;
  }
  auto* ins = MToFloat32::New(alloc(), op, mustPreserveNaN(op->type()));
  curBlock_->add(ins);
  return ins;
}

template <>
MDefinition* FunctionCompiler::unary<MWasmBuiltinTruncateToInt32>(
    MDefinition* op) {
  if (inDeadCode()) {
    return nullptr;
  }
  auto* ins = MWasmBuiltinTruncateToInt32::New(alloc(), op, tlsPointer_,
                                               bytecodeOffset());
  curBlock_->add(ins);
  return ins;
}

template <>
MDefinition* FunctionCompiler::unary<MNot>(MDefinition* op) {
  if (inDeadCode()) {
    return nullptr;
  }
  auto* ins = MNot::NewInt32(alloc(), op);
  curBlock_->add(ins);
  return ins;
}

template <>
MDefinition* FunctionCompiler::unary<MAbs>(MDefinition* op, MIRType type) {
  if (inDeadCode()) {
    return nullptr;
  }
  auto* ins = MAbs::NewWasm(alloc(), op, type);
  curBlock_->add(ins);
  return ins;
}

}  // end anonymous namespace

static bool EmitI32Const(FunctionCompiler& f) {
  int32_t i32;
  if (!f.iter().readI32Const(&i32)) {
    return false;
  }

  f.iter().setResult(f.constant(Int32Value(i32), MIRType::Int32));
  return true;
}

static bool EmitI64Const(FunctionCompiler& f) {
  int64_t i64;
  if (!f.iter().readI64Const(&i64)) {
    return false;
  }

  f.iter().setResult(f.constant(i64));
  return true;
}

static bool EmitF32Const(FunctionCompiler& f) {
  float f32;
  if (!f.iter().readF32Const(&f32)) {
    return false;
  }

  f.iter().setResult(f.constant(f32));
  return true;
}

static bool EmitF64Const(FunctionCompiler& f) {
  double f64;
  if (!f.iter().readF64Const(&f64)) {
    return false;
  }

  f.iter().setResult(f.constant(f64));
  return true;
}

static bool EmitBlock(FunctionCompiler& f) {
  ResultType params;
  return f.iter().readBlock(&params) && f.startBlock();
}

static bool EmitLoop(FunctionCompiler& f) {
  ResultType params;
  if (!f.iter().readLoop(&params)) {
    return false;
  }

  MBasicBlock* loopHeader;
  if (!f.startLoop(&loopHeader, params.length())) {
    return false;
  }

  f.addInterruptCheck();

  f.iter().controlItem().setBlock(loopHeader);
  return true;
}

static bool EmitIf(FunctionCompiler& f) {
  ResultType params;
  MDefinition* condition = nullptr;
  if (!f.iter().readIf(&params, &condition)) {
    return false;
  }

  MBasicBlock* elseBlock;
  if (!f.branchAndStartThen(condition, &elseBlock)) {
    return false;
  }

  f.iter().controlItem().setBlock(elseBlock);
  return true;
}

static bool EmitElse(FunctionCompiler& f) {
  ResultType paramType;
  ResultType resultType;
  DefVector thenValues;
  if (!f.iter().readElse(&paramType, &resultType, &thenValues)) {
    return false;
  }

  if (!f.pushDefs(thenValues)) {
    return false;
  }

  Control& control = f.iter().controlItem();
  if (!f.switchToElse(control.block, &control.block)) {
    return false;
  }

  return true;
}

static bool EmitEnd(FunctionCompiler& f) {
  LabelKind kind;
  ResultType type;
  DefVector preJoinDefs;
  DefVector resultsForEmptyElse;
  if (!f.iter().readEnd(&kind, &type, &preJoinDefs, &resultsForEmptyElse)) {
    return false;
  }

  Control& control = f.iter().controlItem();
  MBasicBlock* block = control.block;

  if (!f.pushDefs(preJoinDefs)) {
    return false;
  }

  // Every label case is responsible to pop the control item at the appropriate
  // time for the label case
  DefVector postJoinDefs;
  switch (kind) {
    case LabelKind::Body:
#ifdef ENABLE_WASM_EXCEPTIONS
      if (!f.finishBodyDelegateThrowPad(control)) {
        return false;
      }
#endif
      if (!f.finishBlock(&postJoinDefs)) {
        return false;
      }
      if (!f.returnValues(postJoinDefs)) {
        return false;
      }
      f.iter().popEnd();
      MOZ_ASSERT(f.iter().controlStackEmpty());
      return f.iter().endFunction(f.iter().end());
    case LabelKind::Block:
      if (!f.finishBlock(&postJoinDefs)) {
        return false;
      }
      f.iter().popEnd();
      break;
    case LabelKind::Loop:
      if (!f.closeLoop(block, &postJoinDefs)) {
        return false;
      }
      f.iter().popEnd();
      break;
    case LabelKind::Then: {
      // If we didn't see an Else, create a trivial else block so that we create
      // a diamond anyway, to preserve Ion invariants.
      if (!f.switchToElse(block, &block)) {
        return false;
      }

      if (!f.pushDefs(resultsForEmptyElse)) {
        return false;
      }

      if (!f.joinIfElse(block, &postJoinDefs)) {
        return false;
      }
      f.iter().popEnd();
      break;
    }
    case LabelKind::Else:
      if (!f.joinIfElse(block, &postJoinDefs)) {
        return false;
      }
      f.iter().popEnd();
      break;
#ifdef ENABLE_WASM_EXCEPTIONS
    case LabelKind::Try: {
      if (block) {
        if (!f.finishCatchlessTry(control)) {
          return false;
        }
      }
      if (!f.finishBlock(&postJoinDefs)) {
        return false;
      }
      f.iter().popEnd();
      break;
    }
    case LabelKind::Catch:
    case LabelKind::CatchAll:
      if (block) {
        if (!f.finishCatches(kind, control)) {
          return false;
        }
      }
      if (!f.finishBlock(&postJoinDefs)) {
        return false;
      }
      f.iter().popEnd();
      break;
#endif
  }

  MOZ_ASSERT_IF(!f.inDeadCode(), postJoinDefs.length() == type.length());
  f.iter().setResults(postJoinDefs.length(), postJoinDefs);

  return true;
}

static bool EmitBr(FunctionCompiler& f) {
  uint32_t relativeDepth;
  ResultType type;
  DefVector values;
  if (!f.iter().readBr(&relativeDepth, &type, &values)) {
    return false;
  }

  return f.br(relativeDepth, values);
}

static bool EmitBrIf(FunctionCompiler& f) {
  uint32_t relativeDepth;
  ResultType type;
  DefVector values;
  MDefinition* condition;
  if (!f.iter().readBrIf(&relativeDepth, &type, &values, &condition)) {
    return false;
  }

  return f.brIf(relativeDepth, values, condition);
}

static bool EmitBrTable(FunctionCompiler& f) {
  Uint32Vector depths;
  uint32_t defaultDepth;
  ResultType branchValueType;
  DefVector branchValues;
  MDefinition* index;
  if (!f.iter().readBrTable(&depths, &defaultDepth, &branchValueType,
                            &branchValues, &index)) {
    return false;
  }

  // If all the targets are the same, or there are no targets, we can just
  // use a goto. This is not just an optimization: MaybeFoldConditionBlock
  // assumes that tables have more than one successor.
  bool allSameDepth = true;
  for (uint32_t depth : depths) {
    if (depth != defaultDepth) {
      allSameDepth = false;
      break;
    }
  }

  if (allSameDepth) {
    return f.br(defaultDepth, branchValues);
  }

  return f.brTable(index, defaultDepth, depths, branchValues);
}

static bool EmitReturn(FunctionCompiler& f) {
  DefVector values;
  if (!f.iter().readReturn(&values)) {
    return false;
  }

  return f.returnValues(values);
}

static bool EmitUnreachable(FunctionCompiler& f) {
  if (!f.iter().readUnreachable()) {
    return false;
  }

  f.unreachableTrap();
  return true;
}

#ifdef ENABLE_WASM_EXCEPTIONS
static bool EmitTry(FunctionCompiler& f) {
  ResultType params;
  if (!f.iter().readTry(&params)) {
    return false;
  }

  MBasicBlock* curBlock = nullptr;
  if (!f.emitTry(&curBlock)) {
    return false;
  }

  f.iter().controlItem().setBlock(curBlock);
  return true;
}

static bool EmitCatch(FunctionCompiler& f) {
  LabelKind kind;
  uint32_t tagIndex;
  ResultType paramType, resultType;
  DefVector tryValues;
  if (!f.iter().readCatch(&kind, &tagIndex, &paramType, &resultType,
                          &tryValues)) {
    return false;
  }

  // Pushing the results of the previous block, to properly join control flow
  // after the try and after each handler, as well as potential control flow
  // patches from other instrunctions. This is similar to what is done for
  // if-then-else control flow and for most other control control flow joins.
  if (!f.pushDefs(tryValues)) {
    return false;
  }

  return f.switchToCatch(kind, tagIndex, f.iter().controlItem());
}

static bool EmitCatchAll(FunctionCompiler& f) {
  LabelKind kind;
  ResultType paramType, resultType;
  DefVector tryValues;
  if (!f.iter().readCatchAll(&kind, &paramType, &resultType, &tryValues)) {
    return false;
  }

  // Pushing the results of the previous block, to properly join control flow
  // after the try and after each handler, as well as potential control flow
  // patches from other instrunctions.
  if (!f.pushDefs(tryValues)) {
    return false;
  }

  return f.switchToCatch(kind, CatchAllIndex, f.iter().controlItem());
}

static bool EmitDelegate(FunctionCompiler& f) {
  uint32_t relativeDepth;
  ResultType resultType;
  DefVector tryValues;
  if (!f.iter().readDelegate(&relativeDepth, &resultType, &tryValues)) {
    return false;
  }

  Control& control = f.iter().controlItem();
  MBasicBlock* block = control.block;

  // Unless the entire try-delegate is dead code, delegate any pad-patches from
  // this try to the next try-block above relativeDepth.
  if (block) {
    ControlInstructionVector& delegatePadPatches = control.tryPadPatches;
    if (!f.delegatePadPatches(delegatePadPatches, relativeDepth)) {
      return false;
    }
  }
  f.iter().popDelegate();

  // Push the results of the previous block, and join control flow with
  // potential control flow patches from other instrunctions in the try code.
  // This is similar to what is done for EmitEnd.
  if (!f.pushDefs(tryValues)) {
    return false;
  }
  DefVector postJoinDefs;
  if (!f.finishBlock(&postJoinDefs)) {
    return false;
  }
  MOZ_ASSERT_IF(!f.inDeadCode(), postJoinDefs.length() == resultType.length());
  f.iter().setResults(postJoinDefs.length(), postJoinDefs);

  return true;
}

static bool EmitThrow(FunctionCompiler& f) {
  uint32_t tagIndex;
  DefVector argValues;
  if (!f.iter().readThrow(&tagIndex, &argValues)) {
    return false;
  }

  return f.emitThrow(tagIndex, argValues);
}

static bool EmitRethrow(FunctionCompiler& f) {
  uint32_t relativeDepth;
  if (!f.iter().readRethrow(&relativeDepth)) {
    return false;
  }

  return f.rethrow(relativeDepth);
}
#endif

static bool EmitCallArgs(FunctionCompiler& f, const FuncType& funcType,
                         const DefVector& args, CallCompileState* call) {
  for (size_t i = 0, n = funcType.args().length(); i < n; ++i) {
    if (!f.mirGen().ensureBallast()) {
      return false;
    }
    if (!f.passArg(args[i], funcType.args()[i], call)) {
      return false;
    }
  }

  ResultType resultType = ResultType::Vector(funcType.results());
  if (!f.passStackResultAreaCallArg(resultType, call)) {
    return false;
  }

  return f.finishCall(call);
}

static bool EmitCall(FunctionCompiler& f, bool asmJSFuncDef) {
  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  uint32_t funcIndex;
  DefVector args;
  if (asmJSFuncDef) {
    if (!f.iter().readOldCallDirect(f.moduleEnv().numFuncImports(), &funcIndex,
                                    &args)) {
      return false;
    }
  } else {
    if (!f.iter().readCall(&funcIndex, &args)) {
      return false;
    }
  }

  if (f.inDeadCode()) {
    return true;
  }

  const FuncType& funcType = *f.moduleEnv().funcs[funcIndex].type;

  CallCompileState call;
  if (!EmitCallArgs(f, funcType, args, &call)) {
    return false;
  }

  DefVector results;
  if (f.moduleEnv().funcIsImport(funcIndex)) {
    uint32_t globalDataOffset =
        f.moduleEnv().funcImportGlobalDataOffsets[funcIndex];
    if (!f.callImport(globalDataOffset, lineOrBytecode, call, funcType,
                      &results)) {
      return false;
    }
  } else {
    if (!f.callDirect(funcType, funcIndex, lineOrBytecode, call, &results)) {
      return false;
    }
  }

#ifdef ENABLE_WASM_EXCEPTIONS
  if (!f.maybeCheckPendingExceptionAfterCall()) {
    return false;
  }
#endif

  f.iter().setResults(results.length(), results);
  return true;
}

static bool EmitCallIndirect(FunctionCompiler& f, bool oldStyle) {
  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  uint32_t funcTypeIndex;
  uint32_t tableIndex;
  MDefinition* callee;
  DefVector args;
  if (oldStyle) {
    tableIndex = 0;
    if (!f.iter().readOldCallIndirect(&funcTypeIndex, &callee, &args)) {
      return false;
    }
  } else {
    if (!f.iter().readCallIndirect(&funcTypeIndex, &tableIndex, &callee,
                                   &args)) {
      return false;
    }
  }

  if (f.inDeadCode()) {
    return true;
  }

  const FuncType& funcType = (*f.moduleEnv().types)[funcTypeIndex].funcType();

  CallCompileState call;
  if (!EmitCallArgs(f, funcType, args, &call)) {
    return false;
  }

  DefVector results;
  if (!f.callIndirect(funcTypeIndex, tableIndex, callee, lineOrBytecode, call,
                      &results)) {
    return false;
  }

#ifdef ENABLE_WASM_EXCEPTIONS
  if (!f.maybeCheckPendingExceptionAfterCall()) {
    return false;
  }
#endif

  f.iter().setResults(results.length(), results);
  return true;
}

static bool EmitGetLocal(FunctionCompiler& f) {
  uint32_t id;
  if (!f.iter().readGetLocal(f.locals(), &id)) {
    return false;
  }

  f.iter().setResult(f.getLocalDef(id));
  return true;
}

static bool EmitSetLocal(FunctionCompiler& f) {
  uint32_t id;
  MDefinition* value;
  if (!f.iter().readSetLocal(f.locals(), &id, &value)) {
    return false;
  }

  f.assign(id, value);
  return true;
}

static bool EmitTeeLocal(FunctionCompiler& f) {
  uint32_t id;
  MDefinition* value;
  if (!f.iter().readTeeLocal(f.locals(), &id, &value)) {
    return false;
  }

  f.assign(id, value);
  return true;
}

static bool EmitGetGlobal(FunctionCompiler& f) {
  uint32_t id;
  if (!f.iter().readGetGlobal(&id)) {
    return false;
  }

  const GlobalDesc& global = f.moduleEnv().globals[id];
  if (!global.isConstant()) {
    f.iter().setResult(f.loadGlobalVar(global.offset(), !global.isMutable(),
                                       global.isIndirect(),
                                       ToMIRType(global.type())));
    return true;
  }

  LitVal value = global.constantValue();
  MIRType mirType = ToMIRType(value.type());

  MDefinition* result;
  switch (value.type().kind()) {
    case ValType::I32:
      result = f.constant(Int32Value(value.i32()), mirType);
      break;
    case ValType::I64:
      result = f.constant(int64_t(value.i64()));
      break;
    case ValType::F32:
      result = f.constant(value.f32());
      break;
    case ValType::F64:
      result = f.constant(value.f64());
      break;
    case ValType::V128:
#ifdef ENABLE_WASM_SIMD
      result = f.constant(value.v128());
      break;
#else
      return f.iter().fail("Ion has no SIMD support yet");
#endif
    case ValType::Ref:
      switch (value.type().refTypeKind()) {
        case RefType::Func:
        case RefType::Extern:
        case RefType::Eq:
          MOZ_ASSERT(value.ref().isNull());
          result = f.nullRefConstant();
          break;
        case RefType::TypeIndex:
          MOZ_CRASH("unexpected reference type in EmitGetGlobal");
      }
      break;
    default:
      MOZ_CRASH("unexpected type in EmitGetGlobal");
  }

  f.iter().setResult(result);
  return true;
}

static bool EmitSetGlobal(FunctionCompiler& f) {
  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  uint32_t id;
  MDefinition* value;
  if (!f.iter().readSetGlobal(&id, &value)) {
    return false;
  }

  const GlobalDesc& global = f.moduleEnv().globals[id];
  MOZ_ASSERT(global.isMutable());
  MInstruction* barrierAddr =
      f.storeGlobalVar(global.offset(), global.isIndirect(), value);

  // We always call the C++ postbarrier because the location will never be in
  // the nursery, and the value stored will very frequently be in the nursery.
  // The C++ postbarrier performs any necessary filtering.

  if (barrierAddr) {
    const SymbolicAddressSignature& callee = SASigPostBarrierFiltering;
    CallCompileState args;
    if (!f.passInstance(callee.argTypes[0], &args)) {
      return false;
    }
    if (!f.passArg(barrierAddr, callee.argTypes[1], &args)) {
      return false;
    }
    f.finishCall(&args);
    if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args)) {
      return false;
    }
  }

  return true;
}

static bool EmitTeeGlobal(FunctionCompiler& f) {
  uint32_t id;
  MDefinition* value;
  if (!f.iter().readTeeGlobal(&id, &value)) {
    return false;
  }

  const GlobalDesc& global = f.moduleEnv().globals[id];
  MOZ_ASSERT(global.isMutable());

  f.storeGlobalVar(global.offset(), global.isIndirect(), value);
  return true;
}

template <typename MIRClass>
static bool EmitUnary(FunctionCompiler& f, ValType operandType) {
  MDefinition* input;
  if (!f.iter().readUnary(operandType, &input)) {
    return false;
  }

  f.iter().setResult(f.unary<MIRClass>(input));
  return true;
}

template <typename MIRClass>
static bool EmitConversion(FunctionCompiler& f, ValType operandType,
                           ValType resultType) {
  MDefinition* input;
  if (!f.iter().readConversion(operandType, resultType, &input)) {
    return false;
  }

  f.iter().setResult(f.unary<MIRClass>(input));
  return true;
}

template <typename MIRClass>
static bool EmitUnaryWithType(FunctionCompiler& f, ValType operandType,
                              MIRType mirType) {
  MDefinition* input;
  if (!f.iter().readUnary(operandType, &input)) {
    return false;
  }

  f.iter().setResult(f.unary<MIRClass>(input, mirType));
  return true;
}

template <typename MIRClass>
static bool EmitConversionWithType(FunctionCompiler& f, ValType operandType,
                                   ValType resultType, MIRType mirType) {
  MDefinition* input;
  if (!f.iter().readConversion(operandType, resultType, &input)) {
    return false;
  }

  f.iter().setResult(f.unary<MIRClass>(input, mirType));
  return true;
}

static bool EmitTruncate(FunctionCompiler& f, ValType operandType,
                         ValType resultType, bool isUnsigned,
                         bool isSaturating) {
  MDefinition* input = nullptr;
  if (!f.iter().readConversion(operandType, resultType, &input)) {
    return false;
  }

  TruncFlags flags = 0;
  if (isUnsigned) {
    flags |= TRUNC_UNSIGNED;
  }
  if (isSaturating) {
    flags |= TRUNC_SATURATING;
  }
  if (resultType == ValType::I32) {
    if (f.moduleEnv().isAsmJS()) {
      if (input && (input->type() == MIRType::Double ||
                    input->type() == MIRType::Float32)) {
        f.iter().setResult(f.unary<MWasmBuiltinTruncateToInt32>(input));
      } else {
        f.iter().setResult(f.unary<MTruncateToInt32>(input));
      }
    } else {
      f.iter().setResult(f.truncate<MWasmTruncateToInt32>(input, flags));
    }
  } else {
    MOZ_ASSERT(resultType == ValType::I64);
    MOZ_ASSERT(!f.moduleEnv().isAsmJS());
#if defined(JS_CODEGEN_ARM)
    f.iter().setResult(f.truncateWithTls(input, flags));
#else
    f.iter().setResult(f.truncate<MWasmTruncateToInt64>(input, flags));
#endif
  }
  return true;
}

static bool EmitSignExtend(FunctionCompiler& f, uint32_t srcSize,
                           uint32_t targetSize) {
  MDefinition* input;
  ValType type = targetSize == 4 ? ValType::I32 : ValType::I64;
  if (!f.iter().readConversion(type, type, &input)) {
    return false;
  }

  f.iter().setResult(f.signExtend(input, srcSize, targetSize));
  return true;
}

static bool EmitExtendI32(FunctionCompiler& f, bool isUnsigned) {
  MDefinition* input;
  if (!f.iter().readConversion(ValType::I32, ValType::I64, &input)) {
    return false;
  }

  f.iter().setResult(f.extendI32(input, isUnsigned));
  return true;
}

static bool EmitConvertI64ToFloatingPoint(FunctionCompiler& f,
                                          ValType resultType, MIRType mirType,
                                          bool isUnsigned) {
  MDefinition* input;
  if (!f.iter().readConversion(ValType::I64, resultType, &input)) {
    return false;
  }

  f.iter().setResult(f.convertI64ToFloatingPoint(input, mirType, isUnsigned));
  return true;
}

static bool EmitReinterpret(FunctionCompiler& f, ValType resultType,
                            ValType operandType, MIRType mirType) {
  MDefinition* input;
  if (!f.iter().readConversion(operandType, resultType, &input)) {
    return false;
  }

  f.iter().setResult(f.unary<MWasmReinterpret>(input, mirType));
  return true;
}

static bool EmitAdd(FunctionCompiler& f, ValType type, MIRType mirType) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(type, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.add(lhs, rhs, mirType));
  return true;
}

static bool EmitSub(FunctionCompiler& f, ValType type, MIRType mirType) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(type, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.sub(lhs, rhs, mirType));
  return true;
}

static bool EmitRotate(FunctionCompiler& f, ValType type, bool isLeftRotation) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(type, &lhs, &rhs)) {
    return false;
  }

  MDefinition* result = f.rotate(lhs, rhs, ToMIRType(type), isLeftRotation);
  f.iter().setResult(result);
  return true;
}

static bool EmitBitNot(FunctionCompiler& f, ValType operandType) {
  MDefinition* input;
  if (!f.iter().readUnary(operandType, &input)) {
    return false;
  }

  f.iter().setResult(f.bitnot(input));
  return true;
}

static bool EmitBitwiseAndOrXor(FunctionCompiler& f, ValType operandType,
                                MIRType mirType,
                                MWasmBinaryBitwise::SubOpcode subOpc) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.binary<MWasmBinaryBitwise>(lhs, rhs, mirType, subOpc));
  return true;
}

template <typename MIRClass>
static bool EmitShift(FunctionCompiler& f, ValType operandType,
                      MIRType mirType) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.binary<MIRClass>(lhs, rhs, mirType));
  return true;
}

static bool EmitUrsh(FunctionCompiler& f, ValType operandType,
                     MIRType mirType) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.ursh(lhs, rhs, mirType));
  return true;
}

static bool EmitMul(FunctionCompiler& f, ValType operandType, MIRType mirType) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(
      f.mul(lhs, rhs, mirType,
            mirType == MIRType::Int32 ? MMul::Integer : MMul::Normal));
  return true;
}

static bool EmitDiv(FunctionCompiler& f, ValType operandType, MIRType mirType,
                    bool isUnsigned) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.div(lhs, rhs, mirType, isUnsigned));
  return true;
}

static bool EmitRem(FunctionCompiler& f, ValType operandType, MIRType mirType,
                    bool isUnsigned) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.mod(lhs, rhs, mirType, isUnsigned));
  return true;
}

static bool EmitMinMax(FunctionCompiler& f, ValType operandType,
                       MIRType mirType, bool isMax) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.minMax(lhs, rhs, mirType, isMax));
  return true;
}

static bool EmitCopySign(FunctionCompiler& f, ValType operandType) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.binary<MCopySign>(lhs, rhs, ToMIRType(operandType)));
  return true;
}

static bool EmitComparison(FunctionCompiler& f, ValType operandType,
                           JSOp compareOp, MCompare::CompareType compareType) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readComparison(operandType, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.compare(lhs, rhs, compareOp, compareType));
  return true;
}

static bool EmitSelect(FunctionCompiler& f, bool typed) {
  StackType type;
  MDefinition* trueValue;
  MDefinition* falseValue;
  MDefinition* condition;
  if (!f.iter().readSelect(typed, &type, &trueValue, &falseValue, &condition)) {
    return false;
  }

  f.iter().setResult(f.select(trueValue, falseValue, condition));
  return true;
}

static bool EmitLoad(FunctionCompiler& f, ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<MDefinition*> addr;
  if (!f.iter().readLoad(type, Scalar::byteSize(viewType), &addr)) {
    return false;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset,
                          f.bytecodeIfNotAsmJS());
  auto* ins = f.load(addr.base, &access, type);
  if (!f.inDeadCode() && !ins) {
    return false;
  }

  f.iter().setResult(ins);
  return true;
}

static bool EmitStore(FunctionCompiler& f, ValType resultType,
                      Scalar::Type viewType) {
  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* value;
  if (!f.iter().readStore(resultType, Scalar::byteSize(viewType), &addr,
                          &value)) {
    return false;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset,
                          f.bytecodeIfNotAsmJS());

  f.store(addr.base, &access, value);
  return true;
}

static bool EmitTeeStore(FunctionCompiler& f, ValType resultType,
                         Scalar::Type viewType) {
  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* value;
  if (!f.iter().readTeeStore(resultType, Scalar::byteSize(viewType), &addr,
                             &value)) {
    return false;
  }

  MOZ_ASSERT(f.isMem32());  // asm.js opcode
  MemoryAccessDesc access(viewType, addr.align, addr.offset,
                          f.bytecodeIfNotAsmJS());

  f.store(addr.base, &access, value);
  return true;
}

static bool EmitTeeStoreWithCoercion(FunctionCompiler& f, ValType resultType,
                                     Scalar::Type viewType) {
  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* value;
  if (!f.iter().readTeeStore(resultType, Scalar::byteSize(viewType), &addr,
                             &value)) {
    return false;
  }

  if (resultType == ValType::F32 && viewType == Scalar::Float64) {
    value = f.unary<MToDouble>(value);
  } else if (resultType == ValType::F64 && viewType == Scalar::Float32) {
    value = f.unary<MToFloat32>(value);
  } else {
    MOZ_CRASH("unexpected coerced store");
  }

  MOZ_ASSERT(f.isMem32());  // asm.js opcode
  MemoryAccessDesc access(viewType, addr.align, addr.offset,
                          f.bytecodeIfNotAsmJS());

  f.store(addr.base, &access, value);
  return true;
}

static bool TryInlineUnaryBuiltin(FunctionCompiler& f, SymbolicAddress callee,
                                  MDefinition* input) {
  if (!input) {
    return false;
  }

  MOZ_ASSERT(IsFloatingPointType(input->type()));

  RoundingMode mode;
  if (!IsRoundingFunction(callee, &mode)) {
    return false;
  }

  if (!MNearbyInt::HasAssemblerSupport(mode)) {
    return false;
  }

  f.iter().setResult(f.nearbyInt(input, mode));
  return true;
}

static bool EmitUnaryMathBuiltinCall(FunctionCompiler& f,
                                     const SymbolicAddressSignature& callee) {
  MOZ_ASSERT(callee.numArgs == 1);

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  MDefinition* input;
  if (!f.iter().readUnary(ValType(callee.argTypes[0]), &input)) {
    return false;
  }

  if (TryInlineUnaryBuiltin(f, callee.identity, input)) {
    return true;
  }

  CallCompileState call;
  if (!f.passArg(input, callee.argTypes[0], &call)) {
    return false;
  }

  if (!f.finishCall(&call)) {
    return false;
  }

  MDefinition* def;
  if (!f.builtinCall(callee, lineOrBytecode, call, &def)) {
    return false;
  }

  f.iter().setResult(def);
  return true;
}

static bool EmitBinaryMathBuiltinCall(FunctionCompiler& f,
                                      const SymbolicAddressSignature& callee) {
  MOZ_ASSERT(callee.numArgs == 2);
  MOZ_ASSERT(callee.argTypes[0] == callee.argTypes[1]);

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  CallCompileState call;
  MDefinition* lhs;
  MDefinition* rhs;
  // This call to readBinary assumes both operands have the same type.
  if (!f.iter().readBinary(ValType(callee.argTypes[0]), &lhs, &rhs)) {
    return false;
  }

  if (!f.passArg(lhs, callee.argTypes[0], &call)) {
    return false;
  }

  if (!f.passArg(rhs, callee.argTypes[1], &call)) {
    return false;
  }

  if (!f.finishCall(&call)) {
    return false;
  }

  MDefinition* def;
  if (!f.builtinCall(callee, lineOrBytecode, call, &def)) {
    return false;
  }

  f.iter().setResult(def);
  return true;
}

static bool EmitMemoryGrow(FunctionCompiler& f) {
  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee =
      f.isNoMemOrMem32() ? SASigMemoryGrowM32 : SASigMemoryGrowM64;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  MDefinition* delta;
  if (!f.iter().readMemoryGrow(&delta)) {
    return false;
  }

  if (!f.passArg(delta, callee.argTypes[1], &args)) {
    return false;
  }

  f.finishCall(&args);

  MDefinition* ret;
  if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitMemorySize(FunctionCompiler& f) {
  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee =
      f.isNoMemOrMem32() ? SASigMemorySizeM32 : SASigMemorySizeM64;
  CallCompileState args;

  if (!f.iter().readMemorySize()) {
    return false;
  }

  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  f.finishCall(&args);

  MDefinition* ret;
  if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitAtomicCmpXchg(FunctionCompiler& f, ValType type,
                              Scalar::Type viewType) {
  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* oldValue;
  MDefinition* newValue;
  if (!f.iter().readAtomicCmpXchg(&addr, type, byteSize(viewType), &oldValue,
                                  &newValue)) {
    return false;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, f.bytecodeOffset(),
                          Synchronization::Full());
  auto* ins =
      f.atomicCompareExchangeHeap(addr.base, &access, type, oldValue, newValue);
  if (!f.inDeadCode() && !ins) {
    return false;
  }

  f.iter().setResult(ins);
  return true;
}

static bool EmitAtomicLoad(FunctionCompiler& f, ValType type,
                           Scalar::Type viewType) {
  LinearMemoryAddress<MDefinition*> addr;
  if (!f.iter().readAtomicLoad(&addr, type, byteSize(viewType))) {
    return false;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, f.bytecodeOffset(),
                          Synchronization::Load());
  auto* ins = f.load(addr.base, &access, type);
  if (!f.inDeadCode() && !ins) {
    return false;
  }

  f.iter().setResult(ins);
  return true;
}

static bool EmitAtomicRMW(FunctionCompiler& f, ValType type,
                          Scalar::Type viewType, jit::AtomicOp op) {
  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* value;
  if (!f.iter().readAtomicRMW(&addr, type, byteSize(viewType), &value)) {
    return false;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, f.bytecodeOffset(),
                          Synchronization::Full());
  auto* ins = f.atomicBinopHeap(op, addr.base, &access, type, value);
  if (!f.inDeadCode() && !ins) {
    return false;
  }

  f.iter().setResult(ins);
  return true;
}

static bool EmitAtomicStore(FunctionCompiler& f, ValType type,
                            Scalar::Type viewType) {
  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* value;
  if (!f.iter().readAtomicStore(&addr, type, byteSize(viewType), &value)) {
    return false;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, f.bytecodeOffset(),
                          Synchronization::Store());
  f.store(addr.base, &access, value);
  return true;
}

static bool EmitWait(FunctionCompiler& f, ValType type, uint32_t byteSize) {
  MOZ_ASSERT(type == ValType::I32 || type == ValType::I64);
  MOZ_ASSERT(SizeOf(type) == byteSize);

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee =
      f.isNoMemOrMem32()
          ? (type == ValType::I32 ? SASigWaitI32M32 : SASigWaitI64M32)
          : (type == ValType::I32 ? SASigWaitI32M64 : SASigWaitI64M64);
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* expected;
  MDefinition* timeout;
  if (!f.iter().readWait(&addr, type, byteSize, &expected, &timeout)) {
    return false;
  }

  MemoryAccessDesc access(type == ValType::I32 ? Scalar::Int32 : Scalar::Int64,
                          addr.align, addr.offset, f.bytecodeOffset());
  MDefinition* ptr = f.computeEffectiveAddress(addr.base, &access);
  if (!f.inDeadCode() && !ptr) {
    return false;
  }

  if (!f.passArg(ptr, callee.argTypes[1], &args)) {
    return false;
  }

  MOZ_ASSERT(ToMIRType(type) == callee.argTypes[2]);
  if (!f.passArg(expected, callee.argTypes[2], &args)) {
    return false;
  }

  if (!f.passArg(timeout, callee.argTypes[3], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  MDefinition* ret;
  if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitFence(FunctionCompiler& f) {
  if (!f.iter().readFence()) {
    return false;
  }

  f.fence();
  return true;
}

static bool EmitWake(FunctionCompiler& f) {
  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee =
      f.isNoMemOrMem32() ? SASigWakeM32 : SASigWakeM64;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* count;
  if (!f.iter().readWake(&addr, &count)) {
    return false;
  }

  MemoryAccessDesc access(Scalar::Int32, addr.align, addr.offset,
                          f.bytecodeOffset());
  MDefinition* ptr = f.computeEffectiveAddress(addr.base, &access);
  if (!f.inDeadCode() && !ptr) {
    return false;
  }

  if (!f.passArg(ptr, callee.argTypes[1], &args)) {
    return false;
  }

  if (!f.passArg(count, callee.argTypes[2], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  MDefinition* ret;
  if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitAtomicXchg(FunctionCompiler& f, ValType type,
                           Scalar::Type viewType) {
  LinearMemoryAddress<MDefinition*> addr;
  MDefinition* value;
  if (!f.iter().readAtomicRMW(&addr, type, byteSize(viewType), &value)) {
    return false;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, f.bytecodeOffset(),
                          Synchronization::Full());
  MDefinition* ins = f.atomicExchangeHeap(addr.base, &access, type, value);
  if (!f.inDeadCode() && !ins) {
    return false;
  }

  f.iter().setResult(ins);
  return true;
}

static bool EmitMemCopyCall(FunctionCompiler& f, MDefinition* dst,
                            MDefinition* src, MDefinition* len) {
  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee =
      (f.moduleEnv().usesSharedMemory()
           ? (f.isMem32() ? SASigMemCopySharedM32 : SASigMemCopySharedM64)
           : (f.isMem32() ? SASigMemCopyM32 : SASigMemCopyM64));
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArg(dst, callee.argTypes[1], &args)) {
    return false;
  }
  if (!f.passArg(src, callee.argTypes[2], &args)) {
    return false;
  }
  if (!f.passArg(len, callee.argTypes[3], &args)) {
    return false;
  }
  MDefinition* memoryBase = f.memoryBase();
  if (!f.passArg(memoryBase, callee.argTypes[4], &args)) {
    return false;
  }
  if (!f.finishCall(&args)) {
    return false;
  }

  return f.builtinInstanceMethodCall(callee, lineOrBytecode, args);
}

static bool EmitMemCopyInlineM32(FunctionCompiler& f, MDefinition* dst,
                                 MDefinition* src, MDefinition* len) {
  MOZ_ASSERT(MaxInlineMemoryCopyLength != 0);

  MOZ_ASSERT(len->isConstant() && len->type() == MIRType::Int32);
  uint32_t length = len->toConstant()->toInt32();
  MOZ_ASSERT(length != 0 && length <= MaxInlineMemoryCopyLength);

  // Compute the number of copies of each width we will need to do
  size_t remainder = length;
#ifdef ENABLE_WASM_SIMD
  size_t numCopies16 = 0;
  if (MacroAssembler::SupportsFastUnalignedFPAccesses()) {
    numCopies16 = remainder / sizeof(V128);
    remainder %= sizeof(V128);
  }
#endif
#ifdef JS_64BIT
  size_t numCopies8 = remainder / sizeof(uint64_t);
  remainder %= sizeof(uint64_t);
#endif
  size_t numCopies4 = remainder / sizeof(uint32_t);
  remainder %= sizeof(uint32_t);
  size_t numCopies2 = remainder / sizeof(uint16_t);
  remainder %= sizeof(uint16_t);
  size_t numCopies1 = remainder;

  // Load all source bytes from low to high using the widest transfer width we
  // can for the system. We will trap without writing anything if any source
  // byte is out-of-bounds.
  size_t offset = 0;
  DefVector loadedValues;

#ifdef ENABLE_WASM_SIMD
  for (uint32_t i = 0; i < numCopies16; i++) {
    MemoryAccessDesc access(Scalar::Simd128, 1, offset, f.bytecodeOffset());
    auto* load = f.load(src, &access, ValType::V128);
    if (!load || !loadedValues.append(load)) {
      return false;
    }

    offset += sizeof(V128);
  }
#endif

#ifdef JS_64BIT
  for (uint32_t i = 0; i < numCopies8; i++) {
    MemoryAccessDesc access(Scalar::Int64, 1, offset, f.bytecodeOffset());
    auto* load = f.load(src, &access, ValType::I64);
    if (!load || !loadedValues.append(load)) {
      return false;
    }

    offset += sizeof(uint64_t);
  }
#endif

  for (uint32_t i = 0; i < numCopies4; i++) {
    MemoryAccessDesc access(Scalar::Uint32, 1, offset, f.bytecodeOffset());
    auto* load = f.load(src, &access, ValType::I32);
    if (!load || !loadedValues.append(load)) {
      return false;
    }

    offset += sizeof(uint32_t);
  }

  if (numCopies2) {
    MemoryAccessDesc access(Scalar::Uint16, 1, offset, f.bytecodeOffset());
    auto* load = f.load(src, &access, ValType::I32);
    if (!load || !loadedValues.append(load)) {
      return false;
    }

    offset += sizeof(uint16_t);
  }

  if (numCopies1) {
    MemoryAccessDesc access(Scalar::Uint8, 1, offset, f.bytecodeOffset());
    auto* load = f.load(src, &access, ValType::I32);
    if (!load || !loadedValues.append(load)) {
      return false;
    }
  }

  // Store all source bytes to the destination from high to low. We will trap
  // without writing anything on the first store if any dest byte is
  // out-of-bounds.
  offset = length;

  if (numCopies1) {
    offset -= sizeof(uint8_t);

    MemoryAccessDesc access(Scalar::Uint8, 1, offset, f.bytecodeOffset());
    auto* value = loadedValues.popCopy();
    f.store(dst, &access, value);
  }

  if (numCopies2) {
    offset -= sizeof(uint16_t);

    MemoryAccessDesc access(Scalar::Uint16, 1, offset, f.bytecodeOffset());
    auto* value = loadedValues.popCopy();
    f.store(dst, &access, value);
  }

  for (uint32_t i = 0; i < numCopies4; i++) {
    offset -= sizeof(uint32_t);

    MemoryAccessDesc access(Scalar::Uint32, 1, offset, f.bytecodeOffset());
    auto* value = loadedValues.popCopy();
    f.store(dst, &access, value);
  }

#ifdef JS_64BIT
  for (uint32_t i = 0; i < numCopies8; i++) {
    offset -= sizeof(uint64_t);

    MemoryAccessDesc access(Scalar::Int64, 1, offset, f.bytecodeOffset());
    auto* value = loadedValues.popCopy();
    f.store(dst, &access, value);
  }
#endif

#ifdef ENABLE_WASM_SIMD
  for (uint32_t i = 0; i < numCopies16; i++) {
    offset -= sizeof(V128);

    MemoryAccessDesc access(Scalar::Simd128, 1, offset, f.bytecodeOffset());
    auto* value = loadedValues.popCopy();
    f.store(dst, &access, value);
  }
#endif

  return true;
}

static bool EmitMemCopy(FunctionCompiler& f) {
  MDefinition *dst, *src, *len;
  uint32_t dstMemIndex;
  uint32_t srcMemIndex;
  if (!f.iter().readMemOrTableCopy(true, &dstMemIndex, &dst, &srcMemIndex, &src,
                                   &len)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  if (f.isMem32()) {
    if (len->isConstant() && len->type() == MIRType::Int32 &&
        len->toConstant()->toInt32() != 0 &&
        uint32_t(len->toConstant()->toInt32()) <= MaxInlineMemoryCopyLength) {
      return EmitMemCopyInlineM32(f, dst, src, len);
    }
  }
  return EmitMemCopyCall(f, dst, src, len);
}

static bool EmitTableCopy(FunctionCompiler& f) {
  MDefinition *dst, *src, *len;
  uint32_t dstTableIndex;
  uint32_t srcTableIndex;
  if (!f.iter().readMemOrTableCopy(false, &dstTableIndex, &dst, &srcTableIndex,
                                   &src, &len)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee = SASigTableCopy;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArg(dst, callee.argTypes[1], &args)) {
    return false;
  }
  if (!f.passArg(src, callee.argTypes[2], &args)) {
    return false;
  }
  if (!f.passArg(len, callee.argTypes[3], &args)) {
    return false;
  }
  MDefinition* dti = f.constant(Int32Value(dstTableIndex), MIRType::Int32);
  if (!dti) {
    return false;
  }
  if (!f.passArg(dti, callee.argTypes[4], &args)) {
    return false;
  }
  MDefinition* sti = f.constant(Int32Value(srcTableIndex), MIRType::Int32);
  if (!sti) {
    return false;
  }
  if (!f.passArg(sti, callee.argTypes[5], &args)) {
    return false;
  }
  if (!f.finishCall(&args)) {
    return false;
  }

  return f.builtinInstanceMethodCall(callee, lineOrBytecode, args);
}

static bool EmitDataOrElemDrop(FunctionCompiler& f, bool isData) {
  uint32_t segIndexVal = 0;
  if (!f.iter().readDataOrElemDrop(isData, &segIndexVal)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee =
      isData ? SASigDataDrop : SASigElemDrop;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  MDefinition* segIndex =
      f.constant(Int32Value(int32_t(segIndexVal)), MIRType::Int32);
  if (!f.passArg(segIndex, callee.argTypes[1], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  return f.builtinInstanceMethodCall(callee, lineOrBytecode, args);
}

static bool EmitMemFillCall(FunctionCompiler& f, MDefinition* start,
                            MDefinition* val, MDefinition* len) {
  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee =
      (f.moduleEnv().usesSharedMemory()
           ? (f.isMem32() ? SASigMemFillSharedM32 : SASigMemFillSharedM64)
           : (f.isMem32() ? SASigMemFillM32 : SASigMemFillM64));
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArg(start, callee.argTypes[1], &args)) {
    return false;
  }
  if (!f.passArg(val, callee.argTypes[2], &args)) {
    return false;
  }
  if (!f.passArg(len, callee.argTypes[3], &args)) {
    return false;
  }
  MDefinition* memoryBase = f.memoryBase();
  if (!f.passArg(memoryBase, callee.argTypes[4], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  return f.builtinInstanceMethodCall(callee, lineOrBytecode, args);
}

static bool EmitMemFillInlineM32(FunctionCompiler& f, MDefinition* start,
                                 MDefinition* val, MDefinition* len) {
  MOZ_ASSERT(MaxInlineMemoryFillLength != 0);

  MOZ_ASSERT(len->isConstant() && len->type() == MIRType::Int32 &&
             val->isConstant() && val->type() == MIRType::Int32);

  uint32_t length = len->toConstant()->toInt32();
  uint32_t value = val->toConstant()->toInt32();
  MOZ_ASSERT(length != 0 && length <= MaxInlineMemoryFillLength);

  // Compute the number of copies of each width we will need to do
  size_t remainder = length;
#ifdef ENABLE_WASM_SIMD
  size_t numCopies16 = 0;
  if (MacroAssembler::SupportsFastUnalignedFPAccesses()) {
    numCopies16 = remainder / sizeof(V128);
    remainder %= sizeof(V128);
  }
#endif
#ifdef JS_64BIT
  size_t numCopies8 = remainder / sizeof(uint64_t);
  remainder %= sizeof(uint64_t);
#endif
  size_t numCopies4 = remainder / sizeof(uint32_t);
  remainder %= sizeof(uint32_t);
  size_t numCopies2 = remainder / sizeof(uint16_t);
  remainder %= sizeof(uint16_t);
  size_t numCopies1 = remainder;

  // Generate splatted definitions for wider fills as needed
#ifdef ENABLE_WASM_SIMD
  MDefinition* val16 = numCopies16 ? f.constant(V128(value)) : nullptr;
#endif
#ifdef JS_64BIT
  MDefinition* val8 =
      numCopies8 ? f.constant(int64_t(SplatByteToUInt<uint64_t>(value, 8)))
                 : nullptr;
#endif
  MDefinition* val4 =
      numCopies4 ? f.constant(Int32Value(SplatByteToUInt<uint32_t>(value, 4)),
                              MIRType::Int32)
                 : nullptr;
  MDefinition* val2 =
      numCopies2 ? f.constant(Int32Value(SplatByteToUInt<uint32_t>(value, 2)),
                              MIRType::Int32)
                 : nullptr;

  // Store the fill value to the destination from high to low. We will trap
  // without writing anything on the first store if any dest byte is
  // out-of-bounds.
  size_t offset = length;

  if (numCopies1) {
    offset -= sizeof(uint8_t);

    MemoryAccessDesc access(Scalar::Uint8, 1, offset, f.bytecodeOffset());
    f.store(start, &access, val);
  }

  if (numCopies2) {
    offset -= sizeof(uint16_t);

    MemoryAccessDesc access(Scalar::Uint16, 1, offset, f.bytecodeOffset());
    f.store(start, &access, val2);
  }

  for (uint32_t i = 0; i < numCopies4; i++) {
    offset -= sizeof(uint32_t);

    MemoryAccessDesc access(Scalar::Uint32, 1, offset, f.bytecodeOffset());
    f.store(start, &access, val4);
  }

#ifdef JS_64BIT
  for (uint32_t i = 0; i < numCopies8; i++) {
    offset -= sizeof(uint64_t);

    MemoryAccessDesc access(Scalar::Int64, 1, offset, f.bytecodeOffset());
    f.store(start, &access, val8);
  }
#endif

#ifdef ENABLE_WASM_SIMD
  for (uint32_t i = 0; i < numCopies16; i++) {
    offset -= sizeof(V128);

    MemoryAccessDesc access(Scalar::Simd128, 1, offset, f.bytecodeOffset());
    f.store(start, &access, val16);
  }
#endif

  return true;
}

static bool EmitMemFill(FunctionCompiler& f) {
  MDefinition *start, *val, *len;
  if (!f.iter().readMemFill(&start, &val, &len)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  if (f.isMem32()) {
    if (len->isConstant() && len->type() == MIRType::Int32 &&
        len->toConstant()->toInt32() != 0 &&
        uint32_t(len->toConstant()->toInt32()) <= MaxInlineMemoryFillLength &&
        val->isConstant() && val->type() == MIRType::Int32) {
      return EmitMemFillInlineM32(f, start, val, len);
    }
  }
  return EmitMemFillCall(f, start, val, len);
}

static bool EmitMemOrTableInit(FunctionCompiler& f, bool isMem) {
  uint32_t segIndexVal = 0, dstTableIndex = 0;
  MDefinition *dstOff, *srcOff, *len;
  if (!f.iter().readMemOrTableInit(isMem, &segIndexVal, &dstTableIndex, &dstOff,
                                   &srcOff, &len)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee =
      isMem ? (f.isMem32() ? SASigMemInitM32 : SASigMemInitM64)
            : SASigTableInit;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArg(dstOff, callee.argTypes[1], &args)) {
    return false;
  }
  if (!f.passArg(srcOff, callee.argTypes[2], &args)) {
    return false;
  }
  if (!f.passArg(len, callee.argTypes[3], &args)) {
    return false;
  }

  MDefinition* segIndex =
      f.constant(Int32Value(int32_t(segIndexVal)), MIRType::Int32);
  if (!f.passArg(segIndex, callee.argTypes[4], &args)) {
    return false;
  }
  if (!isMem) {
    MDefinition* dti = f.constant(Int32Value(dstTableIndex), MIRType::Int32);
    if (!dti) {
      return false;
    }
    if (!f.passArg(dti, callee.argTypes[5], &args)) {
      return false;
    }
  }
  if (!f.finishCall(&args)) {
    return false;
  }

  return f.builtinInstanceMethodCall(callee, lineOrBytecode, args);
}

// Note, table.{get,grow,set} on table(funcref) are currently rejected by the
// verifier.

static bool EmitTableFill(FunctionCompiler& f) {
  uint32_t tableIndex;
  MDefinition *start, *val, *len;
  if (!f.iter().readTableFill(&tableIndex, &start, &val, &len)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee = SASigTableFill;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArg(start, callee.argTypes[1], &args)) {
    return false;
  }
  if (!f.passArg(val, callee.argTypes[2], &args)) {
    return false;
  }
  if (!f.passArg(len, callee.argTypes[3], &args)) {
    return false;
  }

  MDefinition* tableIndexArg =
      f.constant(Int32Value(tableIndex), MIRType::Int32);
  if (!tableIndexArg) {
    return false;
  }
  if (!f.passArg(tableIndexArg, callee.argTypes[4], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  return f.builtinInstanceMethodCall(callee, lineOrBytecode, args);
}

static bool EmitTableGet(FunctionCompiler& f) {
  uint32_t tableIndex;
  MDefinition* index;
  if (!f.iter().readTableGet(&tableIndex, &index)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee = SASigTableGet;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArg(index, callee.argTypes[1], &args)) {
    return false;
  }

  MDefinition* tableIndexArg =
      f.constant(Int32Value(tableIndex), MIRType::Int32);
  if (!tableIndexArg) {
    return false;
  }
  if (!f.passArg(tableIndexArg, callee.argTypes[2], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  // The return value here is either null, denoting an error, or a short-lived
  // pointer to a location containing a possibly-null ref.
  MDefinition* ret;
  if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitTableGrow(FunctionCompiler& f) {
  uint32_t tableIndex;
  MDefinition* initValue;
  MDefinition* delta;
  if (!f.iter().readTableGrow(&tableIndex, &initValue, &delta)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee = SASigTableGrow;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArg(initValue, callee.argTypes[1], &args)) {
    return false;
  }

  if (!f.passArg(delta, callee.argTypes[2], &args)) {
    return false;
  }

  MDefinition* tableIndexArg =
      f.constant(Int32Value(tableIndex), MIRType::Int32);
  if (!tableIndexArg) {
    return false;
  }
  if (!f.passArg(tableIndexArg, callee.argTypes[3], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  MDefinition* ret;
  if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitTableSet(FunctionCompiler& f) {
  uint32_t tableIndex;
  MDefinition* index;
  MDefinition* value;
  if (!f.iter().readTableSet(&tableIndex, &index, &value)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee = SASigTableSet;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArg(index, callee.argTypes[1], &args)) {
    return false;
  }

  if (!f.passArg(value, callee.argTypes[2], &args)) {
    return false;
  }

  MDefinition* tableIndexArg =
      f.constant(Int32Value(tableIndex), MIRType::Int32);
  if (!tableIndexArg) {
    return false;
  }
  if (!f.passArg(tableIndexArg, callee.argTypes[3], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  return f.builtinInstanceMethodCall(callee, lineOrBytecode, args);
}

static bool EmitTableSize(FunctionCompiler& f) {
  uint32_t tableIndex;
  if (!f.iter().readTableSize(&tableIndex)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee = SASigTableSize;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  MDefinition* tableIndexArg =
      f.constant(Int32Value(tableIndex), MIRType::Int32);
  if (!tableIndexArg) {
    return false;
  }
  if (!f.passArg(tableIndexArg, callee.argTypes[1], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  MDefinition* ret;
  if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitRefFunc(FunctionCompiler& f) {
  uint32_t funcIndex;
  if (!f.iter().readRefFunc(&funcIndex)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

  const SymbolicAddressSignature& callee = SASigRefFunc;
  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  MDefinition* funcIndexArg = f.constant(Int32Value(funcIndex), MIRType::Int32);
  if (!funcIndexArg) {
    return false;
  }
  if (!f.passArg(funcIndexArg, callee.argTypes[1], &args)) {
    return false;
  }

  if (!f.finishCall(&args)) {
    return false;
  }

  // The return value here is either null, denoting an error, or a short-lived
  // pointer to a location containing a possibly-null ref.
  MDefinition* ret;
  if (!f.builtinInstanceMethodCall(callee, lineOrBytecode, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitRefNull(FunctionCompiler& f) {
  RefType type;
  if (!f.iter().readRefNull(&type)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  MDefinition* nullVal = f.nullRefConstant();
  if (!nullVal) {
    return false;
  }
  f.iter().setResult(nullVal);
  return true;
}

static bool EmitRefIsNull(FunctionCompiler& f) {
  MDefinition* input;
  if (!f.iter().readRefIsNull(&input)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  MDefinition* nullVal = f.nullRefConstant();
  if (!nullVal) {
    return false;
  }
  f.iter().setResult(
      f.compare(input, nullVal, JSOp::Eq, MCompare::Compare_RefOrNull));
  return true;
}

#ifdef ENABLE_WASM_SIMD
static bool EmitConstSimd128(FunctionCompiler& f) {
  V128 v128;
  if (!f.iter().readV128Const(&v128)) {
    return false;
  }

  f.iter().setResult(f.constant(v128));
  return true;
}

static bool EmitBinarySimd128(FunctionCompiler& f, bool commutative,
                              SimdOp op) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readBinary(ValType::V128, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.binarySimd128(lhs, rhs, commutative, op));
  return true;
}

static bool EmitTernarySimd128(FunctionCompiler& f, wasm::SimdOp op) {
  MDefinition* v0;
  MDefinition* v1;
  MDefinition* v2;
  if (!f.iter().readTernary(ValType::V128, &v0, &v1, &v2)) {
    return false;
  }

  f.iter().setResult(f.ternarySimd128(v0, v1, v2, op));
  return true;
}

static bool EmitShiftSimd128(FunctionCompiler& f, SimdOp op) {
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readVectorShift(&lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.shiftSimd128(lhs, rhs, op));
  return true;
}

static bool EmitSplatSimd128(FunctionCompiler& f, ValType inType, SimdOp op) {
  MDefinition* src;
  if (!f.iter().readConversion(inType, ValType::V128, &src)) {
    return false;
  }

  f.iter().setResult(f.scalarToSimd128(src, op));
  return true;
}

static bool EmitUnarySimd128(FunctionCompiler& f, SimdOp op) {
  MDefinition* src;
  if (!f.iter().readUnary(ValType::V128, &src)) {
    return false;
  }

  f.iter().setResult(f.unarySimd128(src, op));
  return true;
}

static bool EmitReduceSimd128(FunctionCompiler& f, SimdOp op) {
  MDefinition* src;
  if (!f.iter().readConversion(ValType::V128, ValType::I32, &src)) {
    return false;
  }

  f.iter().setResult(f.reduceSimd128(src, op, ValType::I32));
  return true;
}

static bool EmitExtractLaneSimd128(FunctionCompiler& f, ValType outType,
                                   uint32_t laneLimit, SimdOp op) {
  uint32_t laneIndex;
  MDefinition* src;
  if (!f.iter().readExtractLane(outType, laneLimit, &laneIndex, &src)) {
    return false;
  }

  f.iter().setResult(f.reduceSimd128(src, op, outType, laneIndex));
  return true;
}

static bool EmitReplaceLaneSimd128(FunctionCompiler& f, ValType laneType,
                                   uint32_t laneLimit, SimdOp op) {
  uint32_t laneIndex;
  MDefinition* lhs;
  MDefinition* rhs;
  if (!f.iter().readReplaceLane(laneType, laneLimit, &laneIndex, &lhs, &rhs)) {
    return false;
  }

  f.iter().setResult(f.replaceLaneSimd128(lhs, rhs, laneIndex, op));
  return true;
}

static bool EmitShuffleSimd128(FunctionCompiler& f) {
  MDefinition* v1;
  MDefinition* v2;
  V128 control;
  if (!f.iter().readVectorShuffle(&v1, &v2, &control)) {
    return false;
  }

#  ifdef ENABLE_WASM_SIMD_WORMHOLE
  if (f.moduleEnv().simdWormholeEnabled() && IsWormholeTrigger(control)) {
    switch (control.bytes[15]) {
      case 0:
        f.iter().setResult(
            f.binarySimd128(v1, v2, false, wasm::SimdOp::MozWHSELFTEST));
        return true;
#    if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
      case 1:
        f.iter().setResult(
            f.binarySimd128(v1, v2, false, wasm::SimdOp::MozWHPMADDUBSW));
        return true;
      case 2:
        f.iter().setResult(
            f.binarySimd128(v1, v2, false, wasm::SimdOp::MozWHPMADDWD));
        return true;
#    endif
      default:
        return f.iter().fail("Unrecognized wormhole opcode");
    }
  }
#  endif

  f.iter().setResult(f.shuffleSimd128(v1, v2, control));
  return true;
}

static bool EmitLoadSplatSimd128(FunctionCompiler& f, Scalar::Type viewType,
                                 wasm::SimdOp splatOp) {
  LinearMemoryAddress<MDefinition*> addr;
  if (!f.iter().readLoadSplat(Scalar::byteSize(viewType), &addr)) {
    return false;
  }

  f.iter().setResult(f.loadSplatSimd128(viewType, addr, splatOp));
  return true;
}

static bool EmitLoadExtendSimd128(FunctionCompiler& f, wasm::SimdOp op) {
  LinearMemoryAddress<MDefinition*> addr;
  if (!f.iter().readLoadExtend(&addr)) {
    return false;
  }

  f.iter().setResult(f.loadExtendSimd128(addr, op));
  return true;
}

static bool EmitLoadZeroSimd128(FunctionCompiler& f, Scalar::Type viewType,
                                size_t numBytes) {
  LinearMemoryAddress<MDefinition*> addr;
  if (!f.iter().readLoadSplat(numBytes, &addr)) {
    return false;
  }

  f.iter().setResult(f.loadZeroSimd128(viewType, numBytes, addr));
  return true;
}

static bool EmitLoadLaneSimd128(FunctionCompiler& f, uint32_t laneSize) {
  uint32_t laneIndex;
  MDefinition* src;
  LinearMemoryAddress<MDefinition*> addr;
  if (!f.iter().readLoadLane(laneSize, &addr, &laneIndex, &src)) {
    return false;
  }

  f.iter().setResult(f.loadLaneSimd128(laneSize, addr, laneIndex, src));
  return true;
}

static bool EmitStoreLaneSimd128(FunctionCompiler& f, uint32_t laneSize) {
  uint32_t laneIndex;
  MDefinition* src;
  LinearMemoryAddress<MDefinition*> addr;
  if (!f.iter().readStoreLane(laneSize, &addr, &laneIndex, &src)) {
    return false;
  }

  f.storeLaneSimd128(laneSize, addr, laneIndex, src);
  return true;
}

#endif

static bool EmitIntrinsic(FunctionCompiler& f, IntrinsicOp op) {
  const Intrinsic& intrinsic = Intrinsic::getFromOp(op);

  DefVector params;
  if (!f.iter().readIntrinsic(intrinsic, &params)) {
    return false;
  }

  uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();
  const SymbolicAddressSignature& callee = intrinsic.signature;

  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArgs(params, intrinsic.params, &args)) {
    return false;
  }

  MDefinition* memoryBase = f.memoryBase();
  if (!f.passArg(memoryBase, MIRType::Pointer, &args)) {
    return false;
  }

  f.finishCall(&args);

  return f.builtinInstanceMethodCall(callee, lineOrBytecode, args);
}

static bool EmitBodyExprs(FunctionCompiler& f) {
  if (!f.iter().startFunction(f.funcIndex())) {
    return false;
  }

#define CHECK(c)          \
  if (!(c)) return false; \
  break

  while (true) {
    if (!f.mirGen().ensureBallast()) {
      return false;
    }

    OpBytes op;
    if (!f.iter().readOp(&op)) {
      return false;
    }

    switch (op.b0) {
      case uint16_t(Op::End):
        if (!EmitEnd(f)) {
          return false;
        }
        if (f.iter().controlStackEmpty()) {
          return true;
        }
        break;

      // Control opcodes
      case uint16_t(Op::Unreachable):
        CHECK(EmitUnreachable(f));
      case uint16_t(Op::Nop):
        CHECK(f.iter().readNop());
      case uint16_t(Op::Block):
        CHECK(EmitBlock(f));
      case uint16_t(Op::Loop):
        CHECK(EmitLoop(f));
      case uint16_t(Op::If):
        CHECK(EmitIf(f));
      case uint16_t(Op::Else):
        CHECK(EmitElse(f));
#ifdef ENABLE_WASM_EXCEPTIONS
      case uint16_t(Op::Try):
        if (!f.moduleEnv().exceptionsEnabled()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        CHECK(EmitTry(f));
      case uint16_t(Op::Catch):
        if (!f.moduleEnv().exceptionsEnabled()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        CHECK(EmitCatch(f));
      case uint16_t(Op::CatchAll):
        if (!f.moduleEnv().exceptionsEnabled()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        CHECK(EmitCatchAll(f));
      case uint16_t(Op::Delegate):
        if (!f.moduleEnv().exceptionsEnabled()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        if (!EmitDelegate(f)) {
          return false;
        }
        break;
      case uint16_t(Op::Throw):
        if (!f.moduleEnv().exceptionsEnabled()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        CHECK(EmitThrow(f));
      case uint16_t(Op::Rethrow):
        if (!f.moduleEnv().exceptionsEnabled()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        CHECK(EmitRethrow(f));
#endif
      case uint16_t(Op::Br):
        CHECK(EmitBr(f));
      case uint16_t(Op::BrIf):
        CHECK(EmitBrIf(f));
      case uint16_t(Op::BrTable):
        CHECK(EmitBrTable(f));
      case uint16_t(Op::Return):
        CHECK(EmitReturn(f));

      // Calls
      case uint16_t(Op::Call):
        CHECK(EmitCall(f, /* asmJSFuncDef = */ false));
      case uint16_t(Op::CallIndirect):
        CHECK(EmitCallIndirect(f, /* oldStyle = */ false));

      // Parametric operators
      case uint16_t(Op::Drop):
        CHECK(f.iter().readDrop());
      case uint16_t(Op::SelectNumeric):
        CHECK(EmitSelect(f, /*typed*/ false));
      case uint16_t(Op::SelectTyped):
        CHECK(EmitSelect(f, /*typed*/ true));

      // Locals and globals
      case uint16_t(Op::LocalGet):
        CHECK(EmitGetLocal(f));
      case uint16_t(Op::LocalSet):
        CHECK(EmitSetLocal(f));
      case uint16_t(Op::LocalTee):
        CHECK(EmitTeeLocal(f));
      case uint16_t(Op::GlobalGet):
        CHECK(EmitGetGlobal(f));
      case uint16_t(Op::GlobalSet):
        CHECK(EmitSetGlobal(f));
      case uint16_t(Op::TableGet):
        CHECK(EmitTableGet(f));
      case uint16_t(Op::TableSet):
        CHECK(EmitTableSet(f));

      // Memory-related operators
      case uint16_t(Op::I32Load):
        CHECK(EmitLoad(f, ValType::I32, Scalar::Int32));
      case uint16_t(Op::I64Load):
        CHECK(EmitLoad(f, ValType::I64, Scalar::Int64));
      case uint16_t(Op::F32Load):
        CHECK(EmitLoad(f, ValType::F32, Scalar::Float32));
      case uint16_t(Op::F64Load):
        CHECK(EmitLoad(f, ValType::F64, Scalar::Float64));
      case uint16_t(Op::I32Load8S):
        CHECK(EmitLoad(f, ValType::I32, Scalar::Int8));
      case uint16_t(Op::I32Load8U):
        CHECK(EmitLoad(f, ValType::I32, Scalar::Uint8));
      case uint16_t(Op::I32Load16S):
        CHECK(EmitLoad(f, ValType::I32, Scalar::Int16));
      case uint16_t(Op::I32Load16U):
        CHECK(EmitLoad(f, ValType::I32, Scalar::Uint16));
      case uint16_t(Op::I64Load8S):
        CHECK(EmitLoad(f, ValType::I64, Scalar::Int8));
      case uint16_t(Op::I64Load8U):
        CHECK(EmitLoad(f, ValType::I64, Scalar::Uint8));
      case uint16_t(Op::I64Load16S):
        CHECK(EmitLoad(f, ValType::I64, Scalar::Int16));
      case uint16_t(Op::I64Load16U):
        CHECK(EmitLoad(f, ValType::I64, Scalar::Uint16));
      case uint16_t(Op::I64Load32S):
        CHECK(EmitLoad(f, ValType::I64, Scalar::Int32));
      case uint16_t(Op::I64Load32U):
        CHECK(EmitLoad(f, ValType::I64, Scalar::Uint32));
      case uint16_t(Op::I32Store):
        CHECK(EmitStore(f, ValType::I32, Scalar::Int32));
      case uint16_t(Op::I64Store):
        CHECK(EmitStore(f, ValType::I64, Scalar::Int64));
      case uint16_t(Op::F32Store):
        CHECK(EmitStore(f, ValType::F32, Scalar::Float32));
      case uint16_t(Op::F64Store):
        CHECK(EmitStore(f, ValType::F64, Scalar::Float64));
      case uint16_t(Op::I32Store8):
        CHECK(EmitStore(f, ValType::I32, Scalar::Int8));
      case uint16_t(Op::I32Store16):
        CHECK(EmitStore(f, ValType::I32, Scalar::Int16));
      case uint16_t(Op::I64Store8):
        CHECK(EmitStore(f, ValType::I64, Scalar::Int8));
      case uint16_t(Op::I64Store16):
        CHECK(EmitStore(f, ValType::I64, Scalar::Int16));
      case uint16_t(Op::I64Store32):
        CHECK(EmitStore(f, ValType::I64, Scalar::Int32));
      case uint16_t(Op::MemorySize):
        CHECK(EmitMemorySize(f));
      case uint16_t(Op::MemoryGrow):
        CHECK(EmitMemoryGrow(f));

      // Constants
      case uint16_t(Op::I32Const):
        CHECK(EmitI32Const(f));
      case uint16_t(Op::I64Const):
        CHECK(EmitI64Const(f));
      case uint16_t(Op::F32Const):
        CHECK(EmitF32Const(f));
      case uint16_t(Op::F64Const):
        CHECK(EmitF64Const(f));

      // Comparison operators
      case uint16_t(Op::I32Eqz):
        CHECK(EmitConversion<MNot>(f, ValType::I32, ValType::I32));
      case uint16_t(Op::I32Eq):
        CHECK(
            EmitComparison(f, ValType::I32, JSOp::Eq, MCompare::Compare_Int32));
      case uint16_t(Op::I32Ne):
        CHECK(
            EmitComparison(f, ValType::I32, JSOp::Ne, MCompare::Compare_Int32));
      case uint16_t(Op::I32LtS):
        CHECK(
            EmitComparison(f, ValType::I32, JSOp::Lt, MCompare::Compare_Int32));
      case uint16_t(Op::I32LtU):
        CHECK(EmitComparison(f, ValType::I32, JSOp::Lt,
                             MCompare::Compare_UInt32));
      case uint16_t(Op::I32GtS):
        CHECK(
            EmitComparison(f, ValType::I32, JSOp::Gt, MCompare::Compare_Int32));
      case uint16_t(Op::I32GtU):
        CHECK(EmitComparison(f, ValType::I32, JSOp::Gt,
                             MCompare::Compare_UInt32));
      case uint16_t(Op::I32LeS):
        CHECK(
            EmitComparison(f, ValType::I32, JSOp::Le, MCompare::Compare_Int32));
      case uint16_t(Op::I32LeU):
        CHECK(EmitComparison(f, ValType::I32, JSOp::Le,
                             MCompare::Compare_UInt32));
      case uint16_t(Op::I32GeS):
        CHECK(
            EmitComparison(f, ValType::I32, JSOp::Ge, MCompare::Compare_Int32));
      case uint16_t(Op::I32GeU):
        CHECK(EmitComparison(f, ValType::I32, JSOp::Ge,
                             MCompare::Compare_UInt32));
      case uint16_t(Op::I64Eqz):
        CHECK(EmitConversion<MNot>(f, ValType::I64, ValType::I32));
      case uint16_t(Op::I64Eq):
        CHECK(
            EmitComparison(f, ValType::I64, JSOp::Eq, MCompare::Compare_Int64));
      case uint16_t(Op::I64Ne):
        CHECK(
            EmitComparison(f, ValType::I64, JSOp::Ne, MCompare::Compare_Int64));
      case uint16_t(Op::I64LtS):
        CHECK(
            EmitComparison(f, ValType::I64, JSOp::Lt, MCompare::Compare_Int64));
      case uint16_t(Op::I64LtU):
        CHECK(EmitComparison(f, ValType::I64, JSOp::Lt,
                             MCompare::Compare_UInt64));
      case uint16_t(Op::I64GtS):
        CHECK(
            EmitComparison(f, ValType::I64, JSOp::Gt, MCompare::Compare_Int64));
      case uint16_t(Op::I64GtU):
        CHECK(EmitComparison(f, ValType::I64, JSOp::Gt,
                             MCompare::Compare_UInt64));
      case uint16_t(Op::I64LeS):
        CHECK(
            EmitComparison(f, ValType::I64, JSOp::Le, MCompare::Compare_Int64));
      case uint16_t(Op::I64LeU):
        CHECK(EmitComparison(f, ValType::I64, JSOp::Le,
                             MCompare::Compare_UInt64));
      case uint16_t(Op::I64GeS):
        CHECK(
            EmitComparison(f, ValType::I64, JSOp::Ge, MCompare::Compare_Int64));
      case uint16_t(Op::I64GeU):
        CHECK(EmitComparison(f, ValType::I64, JSOp::Ge,
                             MCompare::Compare_UInt64));
      case uint16_t(Op::F32Eq):
        CHECK(EmitComparison(f, ValType::F32, JSOp::Eq,
                             MCompare::Compare_Float32));
      case uint16_t(Op::F32Ne):
        CHECK(EmitComparison(f, ValType::F32, JSOp::Ne,
                             MCompare::Compare_Float32));
      case uint16_t(Op::F32Lt):
        CHECK(EmitComparison(f, ValType::F32, JSOp::Lt,
                             MCompare::Compare_Float32));
      case uint16_t(Op::F32Gt):
        CHECK(EmitComparison(f, ValType::F32, JSOp::Gt,
                             MCompare::Compare_Float32));
      case uint16_t(Op::F32Le):
        CHECK(EmitComparison(f, ValType::F32, JSOp::Le,
                             MCompare::Compare_Float32));
      case uint16_t(Op::F32Ge):
        CHECK(EmitComparison(f, ValType::F32, JSOp::Ge,
                             MCompare::Compare_Float32));
      case uint16_t(Op::F64Eq):
        CHECK(EmitComparison(f, ValType::F64, JSOp::Eq,
                             MCompare::Compare_Double));
      case uint16_t(Op::F64Ne):
        CHECK(EmitComparison(f, ValType::F64, JSOp::Ne,
                             MCompare::Compare_Double));
      case uint16_t(Op::F64Lt):
        CHECK(EmitComparison(f, ValType::F64, JSOp::Lt,
                             MCompare::Compare_Double));
      case uint16_t(Op::F64Gt):
        CHECK(EmitComparison(f, ValType::F64, JSOp::Gt,
                             MCompare::Compare_Double));
      case uint16_t(Op::F64Le):
        CHECK(EmitComparison(f, ValType::F64, JSOp::Le,
                             MCompare::Compare_Double));
      case uint16_t(Op::F64Ge):
        CHECK(EmitComparison(f, ValType::F64, JSOp::Ge,
                             MCompare::Compare_Double));

      // Numeric operators
      case uint16_t(Op::I32Clz):
        CHECK(EmitUnaryWithType<MClz>(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32Ctz):
        CHECK(EmitUnaryWithType<MCtz>(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32Popcnt):
        CHECK(EmitUnaryWithType<MPopcnt>(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32Add):
        CHECK(EmitAdd(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32Sub):
        CHECK(EmitSub(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32Mul):
        CHECK(EmitMul(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32DivS):
      case uint16_t(Op::I32DivU):
        CHECK(
            EmitDiv(f, ValType::I32, MIRType::Int32, Op(op.b0) == Op::I32DivU));
      case uint16_t(Op::I32RemS):
      case uint16_t(Op::I32RemU):
        CHECK(
            EmitRem(f, ValType::I32, MIRType::Int32, Op(op.b0) == Op::I32RemU));
      case uint16_t(Op::I32And):
        CHECK(EmitBitwiseAndOrXor(f, ValType::I32, MIRType::Int32,
                                  MWasmBinaryBitwise::SubOpcode::And));
      case uint16_t(Op::I32Or):
        CHECK(EmitBitwiseAndOrXor(f, ValType::I32, MIRType::Int32,
                                  MWasmBinaryBitwise::SubOpcode::Or));
      case uint16_t(Op::I32Xor):
        CHECK(EmitBitwiseAndOrXor(f, ValType::I32, MIRType::Int32,
                                  MWasmBinaryBitwise::SubOpcode::Xor));
      case uint16_t(Op::I32Shl):
        CHECK(EmitShift<MLsh>(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32ShrS):
        CHECK(EmitShift<MRsh>(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32ShrU):
        CHECK(EmitUrsh(f, ValType::I32, MIRType::Int32));
      case uint16_t(Op::I32Rotl):
      case uint16_t(Op::I32Rotr):
        CHECK(EmitRotate(f, ValType::I32, Op(op.b0) == Op::I32Rotl));
      case uint16_t(Op::I64Clz):
        CHECK(EmitUnaryWithType<MClz>(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64Ctz):
        CHECK(EmitUnaryWithType<MCtz>(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64Popcnt):
        CHECK(EmitUnaryWithType<MPopcnt>(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64Add):
        CHECK(EmitAdd(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64Sub):
        CHECK(EmitSub(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64Mul):
        CHECK(EmitMul(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64DivS):
      case uint16_t(Op::I64DivU):
        CHECK(
            EmitDiv(f, ValType::I64, MIRType::Int64, Op(op.b0) == Op::I64DivU));
      case uint16_t(Op::I64RemS):
      case uint16_t(Op::I64RemU):
        CHECK(
            EmitRem(f, ValType::I64, MIRType::Int64, Op(op.b0) == Op::I64RemU));
      case uint16_t(Op::I64And):
        CHECK(EmitBitwiseAndOrXor(f, ValType::I64, MIRType::Int64,
                                  MWasmBinaryBitwise::SubOpcode::And));
      case uint16_t(Op::I64Or):
        CHECK(EmitBitwiseAndOrXor(f, ValType::I64, MIRType::Int64,
                                  MWasmBinaryBitwise::SubOpcode::Or));
      case uint16_t(Op::I64Xor):
        CHECK(EmitBitwiseAndOrXor(f, ValType::I64, MIRType::Int64,
                                  MWasmBinaryBitwise::SubOpcode::Xor));
      case uint16_t(Op::I64Shl):
        CHECK(EmitShift<MLsh>(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64ShrS):
        CHECK(EmitShift<MRsh>(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64ShrU):
        CHECK(EmitUrsh(f, ValType::I64, MIRType::Int64));
      case uint16_t(Op::I64Rotl):
      case uint16_t(Op::I64Rotr):
        CHECK(EmitRotate(f, ValType::I64, Op(op.b0) == Op::I64Rotl));
      case uint16_t(Op::F32Abs):
        CHECK(EmitUnaryWithType<MAbs>(f, ValType::F32, MIRType::Float32));
      case uint16_t(Op::F32Neg):
        CHECK(EmitUnaryWithType<MWasmNeg>(f, ValType::F32, MIRType::Float32));
      case uint16_t(Op::F32Ceil):
        CHECK(EmitUnaryMathBuiltinCall(f, SASigCeilF));
      case uint16_t(Op::F32Floor):
        CHECK(EmitUnaryMathBuiltinCall(f, SASigFloorF));
      case uint16_t(Op::F32Trunc):
        CHECK(EmitUnaryMathBuiltinCall(f, SASigTruncF));
      case uint16_t(Op::F32Nearest):
        CHECK(EmitUnaryMathBuiltinCall(f, SASigNearbyIntF));
      case uint16_t(Op::F32Sqrt):
        CHECK(EmitUnaryWithType<MSqrt>(f, ValType::F32, MIRType::Float32));
      case uint16_t(Op::F32Add):
        CHECK(EmitAdd(f, ValType::F32, MIRType::Float32));
      case uint16_t(Op::F32Sub):
        CHECK(EmitSub(f, ValType::F32, MIRType::Float32));
      case uint16_t(Op::F32Mul):
        CHECK(EmitMul(f, ValType::F32, MIRType::Float32));
      case uint16_t(Op::F32Div):
        CHECK(EmitDiv(f, ValType::F32, MIRType::Float32,
                      /* isUnsigned = */ false));
      case uint16_t(Op::F32Min):
      case uint16_t(Op::F32Max):
        CHECK(EmitMinMax(f, ValType::F32, MIRType::Float32,
                         Op(op.b0) == Op::F32Max));
      case uint16_t(Op::F32CopySign):
        CHECK(EmitCopySign(f, ValType::F32));
      case uint16_t(Op::F64Abs):
        CHECK(EmitUnaryWithType<MAbs>(f, ValType::F64, MIRType::Double));
      case uint16_t(Op::F64Neg):
        CHECK(EmitUnaryWithType<MWasmNeg>(f, ValType::F64, MIRType::Double));
      case uint16_t(Op::F64Ceil):
        CHECK(EmitUnaryMathBuiltinCall(f, SASigCeilD));
      case uint16_t(Op::F64Floor):
        CHECK(EmitUnaryMathBuiltinCall(f, SASigFloorD));
      case uint16_t(Op::F64Trunc):
        CHECK(EmitUnaryMathBuiltinCall(f, SASigTruncD));
      case uint16_t(Op::F64Nearest):
        CHECK(EmitUnaryMathBuiltinCall(f, SASigNearbyIntD));
      case uint16_t(Op::F64Sqrt):
        CHECK(EmitUnaryWithType<MSqrt>(f, ValType::F64, MIRType::Double));
      case uint16_t(Op::F64Add):
        CHECK(EmitAdd(f, ValType::F64, MIRType::Double));
      case uint16_t(Op::F64Sub):
        CHECK(EmitSub(f, ValType::F64, MIRType::Double));
      case uint16_t(Op::F64Mul):
        CHECK(EmitMul(f, ValType::F64, MIRType::Double));
      case uint16_t(Op::F64Div):
        CHECK(EmitDiv(f, ValType::F64, MIRType::Double,
                      /* isUnsigned = */ false));
      case uint16_t(Op::F64Min):
      case uint16_t(Op::F64Max):
        CHECK(EmitMinMax(f, ValType::F64, MIRType::Double,
                         Op(op.b0) == Op::F64Max));
      case uint16_t(Op::F64CopySign):
        CHECK(EmitCopySign(f, ValType::F64));

      // Conversions
      case uint16_t(Op::I32WrapI64):
        CHECK(EmitConversion<MWrapInt64ToInt32>(f, ValType::I64, ValType::I32));
      case uint16_t(Op::I32TruncF32S):
      case uint16_t(Op::I32TruncF32U):
        CHECK(EmitTruncate(f, ValType::F32, ValType::I32,
                           Op(op.b0) == Op::I32TruncF32U, false));
      case uint16_t(Op::I32TruncF64S):
      case uint16_t(Op::I32TruncF64U):
        CHECK(EmitTruncate(f, ValType::F64, ValType::I32,
                           Op(op.b0) == Op::I32TruncF64U, false));
      case uint16_t(Op::I64ExtendI32S):
      case uint16_t(Op::I64ExtendI32U):
        CHECK(EmitExtendI32(f, Op(op.b0) == Op::I64ExtendI32U));
      case uint16_t(Op::I64TruncF32S):
      case uint16_t(Op::I64TruncF32U):
        CHECK(EmitTruncate(f, ValType::F32, ValType::I64,
                           Op(op.b0) == Op::I64TruncF32U, false));
      case uint16_t(Op::I64TruncF64S):
      case uint16_t(Op::I64TruncF64U):
        CHECK(EmitTruncate(f, ValType::F64, ValType::I64,
                           Op(op.b0) == Op::I64TruncF64U, false));
      case uint16_t(Op::F32ConvertI32S):
        CHECK(EmitConversion<MToFloat32>(f, ValType::I32, ValType::F32));
      case uint16_t(Op::F32ConvertI32U):
        CHECK(EmitConversion<MWasmUnsignedToFloat32>(f, ValType::I32,
                                                     ValType::F32));
      case uint16_t(Op::F32ConvertI64S):
      case uint16_t(Op::F32ConvertI64U):
        CHECK(EmitConvertI64ToFloatingPoint(f, ValType::F32, MIRType::Float32,
                                            Op(op.b0) == Op::F32ConvertI64U));
      case uint16_t(Op::F32DemoteF64):
        CHECK(EmitConversion<MToFloat32>(f, ValType::F64, ValType::F32));
      case uint16_t(Op::F64ConvertI32S):
        CHECK(EmitConversion<MToDouble>(f, ValType::I32, ValType::F64));
      case uint16_t(Op::F64ConvertI32U):
        CHECK(EmitConversion<MWasmUnsignedToDouble>(f, ValType::I32,
                                                    ValType::F64));
      case uint16_t(Op::F64ConvertI64S):
      case uint16_t(Op::F64ConvertI64U):
        CHECK(EmitConvertI64ToFloatingPoint(f, ValType::F64, MIRType::Double,
                                            Op(op.b0) == Op::F64ConvertI64U));
      case uint16_t(Op::F64PromoteF32):
        CHECK(EmitConversion<MToDouble>(f, ValType::F32, ValType::F64));

      // Reinterpretations
      case uint16_t(Op::I32ReinterpretF32):
        CHECK(EmitReinterpret(f, ValType::I32, ValType::F32, MIRType::Int32));
      case uint16_t(Op::I64ReinterpretF64):
        CHECK(EmitReinterpret(f, ValType::I64, ValType::F64, MIRType::Int64));
      case uint16_t(Op::F32ReinterpretI32):
        CHECK(EmitReinterpret(f, ValType::F32, ValType::I32, MIRType::Float32));
      case uint16_t(Op::F64ReinterpretI64):
        CHECK(EmitReinterpret(f, ValType::F64, ValType::I64, MIRType::Double));

#ifdef ENABLE_WASM_GC
      case uint16_t(Op::RefEq):
        if (!f.moduleEnv().gcEnabled()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        CHECK(EmitComparison(f, RefType::extern_(), JSOp::Eq,
                             MCompare::Compare_RefOrNull));
#endif
      case uint16_t(Op::RefFunc):
        CHECK(EmitRefFunc(f));
      case uint16_t(Op::RefNull):
        CHECK(EmitRefNull(f));
      case uint16_t(Op::RefIsNull):
        CHECK(EmitRefIsNull(f));

      // Sign extensions
      case uint16_t(Op::I32Extend8S):
        CHECK(EmitSignExtend(f, 1, 4));
      case uint16_t(Op::I32Extend16S):
        CHECK(EmitSignExtend(f, 2, 4));
      case uint16_t(Op::I64Extend8S):
        CHECK(EmitSignExtend(f, 1, 8));
      case uint16_t(Op::I64Extend16S):
        CHECK(EmitSignExtend(f, 2, 8));
      case uint16_t(Op::I64Extend32S):
        CHECK(EmitSignExtend(f, 4, 8));

      case uint16_t(Op::IntrinsicPrefix): {
        if (!f.moduleEnv().intrinsicsEnabled() ||
            op.b1 >= uint32_t(IntrinsicOp::Limit)) {
          return f.iter().unrecognizedOpcode(&op);
        }
        CHECK(EmitIntrinsic(f, IntrinsicOp(op.b1)));
      }

      // Gc operations
#ifdef ENABLE_WASM_GC
      case uint16_t(Op::GcPrefix): {
        return f.iter().unrecognizedOpcode(&op);
      }
#endif

      // SIMD operations
#ifdef ENABLE_WASM_SIMD
      case uint16_t(Op::SimdPrefix): {
        if (!f.moduleEnv().v128Enabled()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        switch (op.b1) {
          case uint32_t(SimdOp::V128Const):
            CHECK(EmitConstSimd128(f));
          case uint32_t(SimdOp::V128Load):
            CHECK(EmitLoad(f, ValType::V128, Scalar::Simd128));
          case uint32_t(SimdOp::V128Store):
            CHECK(EmitStore(f, ValType::V128, Scalar::Simd128));
          case uint32_t(SimdOp::V128And):
          case uint32_t(SimdOp::V128Or):
          case uint32_t(SimdOp::V128Xor):
          case uint32_t(SimdOp::I8x16AvgrU):
          case uint32_t(SimdOp::I16x8AvgrU):
          case uint32_t(SimdOp::I8x16Add):
          case uint32_t(SimdOp::I8x16AddSatS):
          case uint32_t(SimdOp::I8x16AddSatU):
          case uint32_t(SimdOp::I8x16MinS):
          case uint32_t(SimdOp::I8x16MinU):
          case uint32_t(SimdOp::I8x16MaxS):
          case uint32_t(SimdOp::I8x16MaxU):
          case uint32_t(SimdOp::I16x8Add):
          case uint32_t(SimdOp::I16x8AddSatS):
          case uint32_t(SimdOp::I16x8AddSatU):
          case uint32_t(SimdOp::I16x8Mul):
          case uint32_t(SimdOp::I16x8MinS):
          case uint32_t(SimdOp::I16x8MinU):
          case uint32_t(SimdOp::I16x8MaxS):
          case uint32_t(SimdOp::I16x8MaxU):
          case uint32_t(SimdOp::I32x4Add):
          case uint32_t(SimdOp::I32x4Mul):
          case uint32_t(SimdOp::I32x4MinS):
          case uint32_t(SimdOp::I32x4MinU):
          case uint32_t(SimdOp::I32x4MaxS):
          case uint32_t(SimdOp::I32x4MaxU):
          case uint32_t(SimdOp::I64x2Add):
          case uint32_t(SimdOp::I64x2Mul):
          case uint32_t(SimdOp::F32x4Add):
          case uint32_t(SimdOp::F32x4Mul):
          case uint32_t(SimdOp::F32x4Min):
          case uint32_t(SimdOp::F32x4Max):
          case uint32_t(SimdOp::F64x2Add):
          case uint32_t(SimdOp::F64x2Mul):
          case uint32_t(SimdOp::F64x2Min):
          case uint32_t(SimdOp::F64x2Max):
          case uint32_t(SimdOp::I8x16Eq):
          case uint32_t(SimdOp::I8x16Ne):
          case uint32_t(SimdOp::I16x8Eq):
          case uint32_t(SimdOp::I16x8Ne):
          case uint32_t(SimdOp::I32x4Eq):
          case uint32_t(SimdOp::I32x4Ne):
          case uint32_t(SimdOp::I64x2Eq):
          case uint32_t(SimdOp::I64x2Ne):
          case uint32_t(SimdOp::F32x4Eq):
          case uint32_t(SimdOp::F32x4Ne):
          case uint32_t(SimdOp::F64x2Eq):
          case uint32_t(SimdOp::F64x2Ne):
          case uint32_t(SimdOp::I32x4DotI16x8S):
          case uint32_t(SimdOp::I16x8ExtmulLowI8x16S):
          case uint32_t(SimdOp::I16x8ExtmulHighI8x16S):
          case uint32_t(SimdOp::I16x8ExtmulLowI8x16U):
          case uint32_t(SimdOp::I16x8ExtmulHighI8x16U):
          case uint32_t(SimdOp::I32x4ExtmulLowI16x8S):
          case uint32_t(SimdOp::I32x4ExtmulHighI16x8S):
          case uint32_t(SimdOp::I32x4ExtmulLowI16x8U):
          case uint32_t(SimdOp::I32x4ExtmulHighI16x8U):
          case uint32_t(SimdOp::I64x2ExtmulLowI32x4S):
          case uint32_t(SimdOp::I64x2ExtmulHighI32x4S):
          case uint32_t(SimdOp::I64x2ExtmulLowI32x4U):
          case uint32_t(SimdOp::I64x2ExtmulHighI32x4U):
          case uint32_t(SimdOp::I16x8Q15MulrSatS):
            CHECK(EmitBinarySimd128(f, /* commutative= */ true, SimdOp(op.b1)));
          case uint32_t(SimdOp::V128AndNot):
          case uint32_t(SimdOp::I8x16Sub):
          case uint32_t(SimdOp::I8x16SubSatS):
          case uint32_t(SimdOp::I8x16SubSatU):
          case uint32_t(SimdOp::I16x8Sub):
          case uint32_t(SimdOp::I16x8SubSatS):
          case uint32_t(SimdOp::I16x8SubSatU):
          case uint32_t(SimdOp::I32x4Sub):
          case uint32_t(SimdOp::I64x2Sub):
          case uint32_t(SimdOp::F32x4Sub):
          case uint32_t(SimdOp::F32x4Div):
          case uint32_t(SimdOp::F64x2Sub):
          case uint32_t(SimdOp::F64x2Div):
          case uint32_t(SimdOp::I8x16NarrowI16x8S):
          case uint32_t(SimdOp::I8x16NarrowI16x8U):
          case uint32_t(SimdOp::I16x8NarrowI32x4S):
          case uint32_t(SimdOp::I16x8NarrowI32x4U):
          case uint32_t(SimdOp::I8x16LtS):
          case uint32_t(SimdOp::I8x16LtU):
          case uint32_t(SimdOp::I8x16GtS):
          case uint32_t(SimdOp::I8x16GtU):
          case uint32_t(SimdOp::I8x16LeS):
          case uint32_t(SimdOp::I8x16LeU):
          case uint32_t(SimdOp::I8x16GeS):
          case uint32_t(SimdOp::I8x16GeU):
          case uint32_t(SimdOp::I16x8LtS):
          case uint32_t(SimdOp::I16x8LtU):
          case uint32_t(SimdOp::I16x8GtS):
          case uint32_t(SimdOp::I16x8GtU):
          case uint32_t(SimdOp::I16x8LeS):
          case uint32_t(SimdOp::I16x8LeU):
          case uint32_t(SimdOp::I16x8GeS):
          case uint32_t(SimdOp::I16x8GeU):
          case uint32_t(SimdOp::I32x4LtS):
          case uint32_t(SimdOp::I32x4LtU):
          case uint32_t(SimdOp::I32x4GtS):
          case uint32_t(SimdOp::I32x4GtU):
          case uint32_t(SimdOp::I32x4LeS):
          case uint32_t(SimdOp::I32x4LeU):
          case uint32_t(SimdOp::I32x4GeS):
          case uint32_t(SimdOp::I32x4GeU):
          case uint32_t(SimdOp::I64x2LtS):
          case uint32_t(SimdOp::I64x2GtS):
          case uint32_t(SimdOp::I64x2LeS):
          case uint32_t(SimdOp::I64x2GeS):
          case uint32_t(SimdOp::F32x4Lt):
          case uint32_t(SimdOp::F32x4Gt):
          case uint32_t(SimdOp::F32x4Le):
          case uint32_t(SimdOp::F32x4Ge):
          case uint32_t(SimdOp::F64x2Lt):
          case uint32_t(SimdOp::F64x2Gt):
          case uint32_t(SimdOp::F64x2Le):
          case uint32_t(SimdOp::F64x2Ge):
          case uint32_t(SimdOp::I8x16Swizzle):
          case uint32_t(SimdOp::F32x4PMax):
          case uint32_t(SimdOp::F32x4PMin):
          case uint32_t(SimdOp::F64x2PMax):
          case uint32_t(SimdOp::F64x2PMin):
            CHECK(
                EmitBinarySimd128(f, /* commutative= */ false, SimdOp(op.b1)));
          case uint32_t(SimdOp::I8x16Splat):
          case uint32_t(SimdOp::I16x8Splat):
          case uint32_t(SimdOp::I32x4Splat):
            CHECK(EmitSplatSimd128(f, ValType::I32, SimdOp(op.b1)));
          case uint32_t(SimdOp::I64x2Splat):
            CHECK(EmitSplatSimd128(f, ValType::I64, SimdOp(op.b1)));
          case uint32_t(SimdOp::F32x4Splat):
            CHECK(EmitSplatSimd128(f, ValType::F32, SimdOp(op.b1)));
          case uint32_t(SimdOp::F64x2Splat):
            CHECK(EmitSplatSimd128(f, ValType::F64, SimdOp(op.b1)));
          case uint32_t(SimdOp::I8x16Neg):
          case uint32_t(SimdOp::I16x8Neg):
          case uint32_t(SimdOp::I16x8ExtendLowI8x16S):
          case uint32_t(SimdOp::I16x8ExtendHighI8x16S):
          case uint32_t(SimdOp::I16x8ExtendLowI8x16U):
          case uint32_t(SimdOp::I16x8ExtendHighI8x16U):
          case uint32_t(SimdOp::I32x4Neg):
          case uint32_t(SimdOp::I32x4ExtendLowI16x8S):
          case uint32_t(SimdOp::I32x4ExtendHighI16x8S):
          case uint32_t(SimdOp::I32x4ExtendLowI16x8U):
          case uint32_t(SimdOp::I32x4ExtendHighI16x8U):
          case uint32_t(SimdOp::I32x4TruncSatF32x4S):
          case uint32_t(SimdOp::I32x4TruncSatF32x4U):
          case uint32_t(SimdOp::I64x2Neg):
          case uint32_t(SimdOp::I64x2ExtendLowI32x4S):
          case uint32_t(SimdOp::I64x2ExtendHighI32x4S):
          case uint32_t(SimdOp::I64x2ExtendLowI32x4U):
          case uint32_t(SimdOp::I64x2ExtendHighI32x4U):
          case uint32_t(SimdOp::F32x4Abs):
          case uint32_t(SimdOp::F32x4Neg):
          case uint32_t(SimdOp::F32x4Sqrt):
          case uint32_t(SimdOp::F32x4ConvertI32x4S):
          case uint32_t(SimdOp::F32x4ConvertI32x4U):
          case uint32_t(SimdOp::F64x2Abs):
          case uint32_t(SimdOp::F64x2Neg):
          case uint32_t(SimdOp::F64x2Sqrt):
          case uint32_t(SimdOp::V128Not):
          case uint32_t(SimdOp::I8x16Popcnt):
          case uint32_t(SimdOp::I8x16Abs):
          case uint32_t(SimdOp::I16x8Abs):
          case uint32_t(SimdOp::I32x4Abs):
          case uint32_t(SimdOp::I64x2Abs):
          case uint32_t(SimdOp::F32x4Ceil):
          case uint32_t(SimdOp::F32x4Floor):
          case uint32_t(SimdOp::F32x4Trunc):
          case uint32_t(SimdOp::F32x4Nearest):
          case uint32_t(SimdOp::F64x2Ceil):
          case uint32_t(SimdOp::F64x2Floor):
          case uint32_t(SimdOp::F64x2Trunc):
          case uint32_t(SimdOp::F64x2Nearest):
          case uint32_t(SimdOp::F32x4DemoteF64x2Zero):
          case uint32_t(SimdOp::F64x2PromoteLowF32x4):
          case uint32_t(SimdOp::F64x2ConvertLowI32x4S):
          case uint32_t(SimdOp::F64x2ConvertLowI32x4U):
          case uint32_t(SimdOp::I32x4TruncSatF64x2SZero):
          case uint32_t(SimdOp::I32x4TruncSatF64x2UZero):
          case uint32_t(SimdOp::I16x8ExtaddPairwiseI8x16S):
          case uint32_t(SimdOp::I16x8ExtaddPairwiseI8x16U):
          case uint32_t(SimdOp::I32x4ExtaddPairwiseI16x8S):
          case uint32_t(SimdOp::I32x4ExtaddPairwiseI16x8U):
            CHECK(EmitUnarySimd128(f, SimdOp(op.b1)));
          case uint32_t(SimdOp::V128AnyTrue):
          case uint32_t(SimdOp::I8x16AllTrue):
          case uint32_t(SimdOp::I16x8AllTrue):
          case uint32_t(SimdOp::I32x4AllTrue):
          case uint32_t(SimdOp::I64x2AllTrue):
          case uint32_t(SimdOp::I8x16Bitmask):
          case uint32_t(SimdOp::I16x8Bitmask):
          case uint32_t(SimdOp::I32x4Bitmask):
          case uint32_t(SimdOp::I64x2Bitmask):
            CHECK(EmitReduceSimd128(f, SimdOp(op.b1)));
          case uint32_t(SimdOp::I8x16Shl):
          case uint32_t(SimdOp::I8x16ShrS):
          case uint32_t(SimdOp::I8x16ShrU):
          case uint32_t(SimdOp::I16x8Shl):
          case uint32_t(SimdOp::I16x8ShrS):
          case uint32_t(SimdOp::I16x8ShrU):
          case uint32_t(SimdOp::I32x4Shl):
          case uint32_t(SimdOp::I32x4ShrS):
          case uint32_t(SimdOp::I32x4ShrU):
          case uint32_t(SimdOp::I64x2Shl):
          case uint32_t(SimdOp::I64x2ShrS):
          case uint32_t(SimdOp::I64x2ShrU):
            CHECK(EmitShiftSimd128(f, SimdOp(op.b1)));
          case uint32_t(SimdOp::I8x16ExtractLaneS):
          case uint32_t(SimdOp::I8x16ExtractLaneU):
            CHECK(EmitExtractLaneSimd128(f, ValType::I32, 16, SimdOp(op.b1)));
          case uint32_t(SimdOp::I16x8ExtractLaneS):
          case uint32_t(SimdOp::I16x8ExtractLaneU):
            CHECK(EmitExtractLaneSimd128(f, ValType::I32, 8, SimdOp(op.b1)));
          case uint32_t(SimdOp::I32x4ExtractLane):
            CHECK(EmitExtractLaneSimd128(f, ValType::I32, 4, SimdOp(op.b1)));
          case uint32_t(SimdOp::I64x2ExtractLane):
            CHECK(EmitExtractLaneSimd128(f, ValType::I64, 2, SimdOp(op.b1)));
          case uint32_t(SimdOp::F32x4ExtractLane):
            CHECK(EmitExtractLaneSimd128(f, ValType::F32, 4, SimdOp(op.b1)));
          case uint32_t(SimdOp::F64x2ExtractLane):
            CHECK(EmitExtractLaneSimd128(f, ValType::F64, 2, SimdOp(op.b1)));
          case uint32_t(SimdOp::I8x16ReplaceLane):
            CHECK(EmitReplaceLaneSimd128(f, ValType::I32, 16, SimdOp(op.b1)));
          case uint32_t(SimdOp::I16x8ReplaceLane):
            CHECK(EmitReplaceLaneSimd128(f, ValType::I32, 8, SimdOp(op.b1)));
          case uint32_t(SimdOp::I32x4ReplaceLane):
            CHECK(EmitReplaceLaneSimd128(f, ValType::I32, 4, SimdOp(op.b1)));
          case uint32_t(SimdOp::I64x2ReplaceLane):
            CHECK(EmitReplaceLaneSimd128(f, ValType::I64, 2, SimdOp(op.b1)));
          case uint32_t(SimdOp::F32x4ReplaceLane):
            CHECK(EmitReplaceLaneSimd128(f, ValType::F32, 4, SimdOp(op.b1)));
          case uint32_t(SimdOp::F64x2ReplaceLane):
            CHECK(EmitReplaceLaneSimd128(f, ValType::F64, 2, SimdOp(op.b1)));
          case uint32_t(SimdOp::V128Bitselect):
            CHECK(EmitTernarySimd128(f, SimdOp(op.b1)));
          case uint32_t(SimdOp::I8x16Shuffle):
            CHECK(EmitShuffleSimd128(f));
          case uint32_t(SimdOp::V128Load8Splat):
            CHECK(EmitLoadSplatSimd128(f, Scalar::Uint8, SimdOp::I8x16Splat));
          case uint32_t(SimdOp::V128Load16Splat):
            CHECK(EmitLoadSplatSimd128(f, Scalar::Uint16, SimdOp::I16x8Splat));
          case uint32_t(SimdOp::V128Load32Splat):
            CHECK(EmitLoadSplatSimd128(f, Scalar::Float32, SimdOp::I32x4Splat));
          case uint32_t(SimdOp::V128Load64Splat):
            CHECK(EmitLoadSplatSimd128(f, Scalar::Float64, SimdOp::I64x2Splat));
          case uint32_t(SimdOp::V128Load8x8S):
          case uint32_t(SimdOp::V128Load8x8U):
          case uint32_t(SimdOp::V128Load16x4S):
          case uint32_t(SimdOp::V128Load16x4U):
          case uint32_t(SimdOp::V128Load32x2S):
          case uint32_t(SimdOp::V128Load32x2U):
            CHECK(EmitLoadExtendSimd128(f, SimdOp(op.b1)));
          case uint32_t(SimdOp::V128Load32Zero):
            CHECK(EmitLoadZeroSimd128(f, Scalar::Float32, 4));
          case uint32_t(SimdOp::V128Load64Zero):
            CHECK(EmitLoadZeroSimd128(f, Scalar::Float64, 8));
          case uint32_t(SimdOp::V128Load8Lane):
            CHECK(EmitLoadLaneSimd128(f, 1));
          case uint32_t(SimdOp::V128Load16Lane):
            CHECK(EmitLoadLaneSimd128(f, 2));
          case uint32_t(SimdOp::V128Load32Lane):
            CHECK(EmitLoadLaneSimd128(f, 4));
          case uint32_t(SimdOp::V128Load64Lane):
            CHECK(EmitLoadLaneSimd128(f, 8));
          case uint32_t(SimdOp::V128Store8Lane):
            CHECK(EmitStoreLaneSimd128(f, 1));
          case uint32_t(SimdOp::V128Store16Lane):
            CHECK(EmitStoreLaneSimd128(f, 2));
          case uint32_t(SimdOp::V128Store32Lane):
            CHECK(EmitStoreLaneSimd128(f, 4));
          case uint32_t(SimdOp::V128Store64Lane):
            CHECK(EmitStoreLaneSimd128(f, 8));
#  ifdef ENABLE_WASM_RELAXED_SIMD
          case uint32_t(SimdOp::F32x4RelaxedFma):
          case uint32_t(SimdOp::F32x4RelaxedFms):
          case uint32_t(SimdOp::F64x2RelaxedFma):
          case uint32_t(SimdOp::F64x2RelaxedFms):
          case uint32_t(SimdOp::I8x16LaneSelect):
          case uint32_t(SimdOp::I16x8LaneSelect):
          case uint32_t(SimdOp::I32x4LaneSelect):
          case uint32_t(SimdOp::I64x2LaneSelect): {
            if (!f.moduleEnv().v128RelaxedEnabled()) {
              return f.iter().unrecognizedOpcode(&op);
            }
            CHECK(EmitTernarySimd128(f, SimdOp(op.b1)));
          }
          case uint32_t(SimdOp::F32x4RelaxedMin):
          case uint32_t(SimdOp::F32x4RelaxedMax):
          case uint32_t(SimdOp::F64x2RelaxedMin):
          case uint32_t(SimdOp::F64x2RelaxedMax): {
            if (!f.moduleEnv().v128RelaxedEnabled()) {
              return f.iter().unrecognizedOpcode(&op);
            }
            CHECK(EmitBinarySimd128(f, /* commutative= */ true, SimdOp(op.b1)));
          }
          case uint32_t(SimdOp::I32x4RelaxedTruncSSatF32x4):
          case uint32_t(SimdOp::I32x4RelaxedTruncUSatF32x4):
          case uint32_t(SimdOp::I32x4RelaxedTruncSatF64x2SZero):
          case uint32_t(SimdOp::I32x4RelaxedTruncSatF64x2UZero): {
            if (!f.moduleEnv().v128RelaxedEnabled()) {
              return f.iter().unrecognizedOpcode(&op);
            }
            CHECK(EmitUnarySimd128(f, SimdOp(op.b1)));
          }
          case uint32_t(SimdOp::V8x16RelaxedSwizzle): {
            if (!f.moduleEnv().v128RelaxedEnabled()) {
              return f.iter().unrecognizedOpcode(&op);
            }
            CHECK(
                EmitBinarySimd128(f, /* commutative= */ false, SimdOp(op.b1)));
          }
#  endif

          default:
            return f.iter().unrecognizedOpcode(&op);
        }  // switch (op.b1)
        break;
      }
#endif

      // Miscellaneous operations
      case uint16_t(Op::MiscPrefix): {
        switch (op.b1) {
          case uint32_t(MiscOp::I32TruncSatF32S):
          case uint32_t(MiscOp::I32TruncSatF32U):
            CHECK(EmitTruncate(f, ValType::F32, ValType::I32,
                               MiscOp(op.b1) == MiscOp::I32TruncSatF32U, true));
          case uint32_t(MiscOp::I32TruncSatF64S):
          case uint32_t(MiscOp::I32TruncSatF64U):
            CHECK(EmitTruncate(f, ValType::F64, ValType::I32,
                               MiscOp(op.b1) == MiscOp::I32TruncSatF64U, true));
          case uint32_t(MiscOp::I64TruncSatF32S):
          case uint32_t(MiscOp::I64TruncSatF32U):
            CHECK(EmitTruncate(f, ValType::F32, ValType::I64,
                               MiscOp(op.b1) == MiscOp::I64TruncSatF32U, true));
          case uint32_t(MiscOp::I64TruncSatF64S):
          case uint32_t(MiscOp::I64TruncSatF64U):
            CHECK(EmitTruncate(f, ValType::F64, ValType::I64,
                               MiscOp(op.b1) == MiscOp::I64TruncSatF64U, true));
          case uint32_t(MiscOp::MemoryCopy):
            CHECK(EmitMemCopy(f));
          case uint32_t(MiscOp::DataDrop):
            CHECK(EmitDataOrElemDrop(f, /*isData=*/true));
          case uint32_t(MiscOp::MemoryFill):
            CHECK(EmitMemFill(f));
          case uint32_t(MiscOp::MemoryInit):
            CHECK(EmitMemOrTableInit(f, /*isMem=*/true));
          case uint32_t(MiscOp::TableCopy):
            CHECK(EmitTableCopy(f));
          case uint32_t(MiscOp::ElemDrop):
            CHECK(EmitDataOrElemDrop(f, /*isData=*/false));
          case uint32_t(MiscOp::TableInit):
            CHECK(EmitMemOrTableInit(f, /*isMem=*/false));
          case uint32_t(MiscOp::TableFill):
            CHECK(EmitTableFill(f));
          case uint32_t(MiscOp::TableGrow):
            CHECK(EmitTableGrow(f));
          case uint32_t(MiscOp::TableSize):
            CHECK(EmitTableSize(f));
          default:
            return f.iter().unrecognizedOpcode(&op);
        }
        break;
      }

      // Thread operations
      case uint16_t(Op::ThreadPrefix): {
        // Though thread ops can be used on nonshared memories, we make them
        // unavailable if shared memory has been disabled in the prefs, for
        // maximum predictability and safety and consistency with JS.
        if (f.moduleEnv().sharedMemoryEnabled() == Shareable::False) {
          return f.iter().unrecognizedOpcode(&op);
        }
        switch (op.b1) {
          case uint32_t(ThreadOp::Wake):
            CHECK(EmitWake(f));

          case uint32_t(ThreadOp::I32Wait):
            CHECK(EmitWait(f, ValType::I32, 4));
          case uint32_t(ThreadOp::I64Wait):
            CHECK(EmitWait(f, ValType::I64, 8));
          case uint32_t(ThreadOp::Fence):
            CHECK(EmitFence(f));

          case uint32_t(ThreadOp::I32AtomicLoad):
            CHECK(EmitAtomicLoad(f, ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicLoad):
            CHECK(EmitAtomicLoad(f, ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicLoad8U):
            CHECK(EmitAtomicLoad(f, ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicLoad16U):
            CHECK(EmitAtomicLoad(f, ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicLoad8U):
            CHECK(EmitAtomicLoad(f, ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicLoad16U):
            CHECK(EmitAtomicLoad(f, ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicLoad32U):
            CHECK(EmitAtomicLoad(f, ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicStore):
            CHECK(EmitAtomicStore(f, ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicStore):
            CHECK(EmitAtomicStore(f, ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicStore8U):
            CHECK(EmitAtomicStore(f, ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicStore16U):
            CHECK(EmitAtomicStore(f, ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicStore8U):
            CHECK(EmitAtomicStore(f, ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicStore16U):
            CHECK(EmitAtomicStore(f, ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicStore32U):
            CHECK(EmitAtomicStore(f, ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicAdd):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Int32,
                                AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Int64,
                                AtomicFetchAddOp));
          case uint32_t(ThreadOp::I32AtomicAdd8U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint8,
                                AtomicFetchAddOp));
          case uint32_t(ThreadOp::I32AtomicAdd16U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint16,
                                AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd8U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint8,
                                AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd16U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint16,
                                AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd32U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint32,
                                AtomicFetchAddOp));

          case uint32_t(ThreadOp::I32AtomicSub):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Int32,
                                AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Int64,
                                AtomicFetchSubOp));
          case uint32_t(ThreadOp::I32AtomicSub8U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint8,
                                AtomicFetchSubOp));
          case uint32_t(ThreadOp::I32AtomicSub16U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint16,
                                AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub8U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint8,
                                AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub16U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint16,
                                AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub32U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint32,
                                AtomicFetchSubOp));

          case uint32_t(ThreadOp::I32AtomicAnd):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Int32,
                                AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Int64,
                                AtomicFetchAndOp));
          case uint32_t(ThreadOp::I32AtomicAnd8U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint8,
                                AtomicFetchAndOp));
          case uint32_t(ThreadOp::I32AtomicAnd16U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint16,
                                AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd8U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint8,
                                AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd16U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint16,
                                AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd32U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint32,
                                AtomicFetchAndOp));

          case uint32_t(ThreadOp::I32AtomicOr):
            CHECK(
                EmitAtomicRMW(f, ValType::I32, Scalar::Int32, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr):
            CHECK(
                EmitAtomicRMW(f, ValType::I64, Scalar::Int64, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I32AtomicOr8U):
            CHECK(
                EmitAtomicRMW(f, ValType::I32, Scalar::Uint8, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I32AtomicOr16U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint16,
                                AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr8U):
            CHECK(
                EmitAtomicRMW(f, ValType::I64, Scalar::Uint8, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr16U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint16,
                                AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr32U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint32,
                                AtomicFetchOrOp));

          case uint32_t(ThreadOp::I32AtomicXor):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Int32,
                                AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Int64,
                                AtomicFetchXorOp));
          case uint32_t(ThreadOp::I32AtomicXor8U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint8,
                                AtomicFetchXorOp));
          case uint32_t(ThreadOp::I32AtomicXor16U):
            CHECK(EmitAtomicRMW(f, ValType::I32, Scalar::Uint16,
                                AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor8U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint8,
                                AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor16U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint16,
                                AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor32U):
            CHECK(EmitAtomicRMW(f, ValType::I64, Scalar::Uint32,
                                AtomicFetchXorOp));

          case uint32_t(ThreadOp::I32AtomicXchg):
            CHECK(EmitAtomicXchg(f, ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicXchg):
            CHECK(EmitAtomicXchg(f, ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicXchg8U):
            CHECK(EmitAtomicXchg(f, ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicXchg16U):
            CHECK(EmitAtomicXchg(f, ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicXchg8U):
            CHECK(EmitAtomicXchg(f, ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicXchg16U):
            CHECK(EmitAtomicXchg(f, ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicXchg32U):
            CHECK(EmitAtomicXchg(f, ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicCmpXchg):
            CHECK(EmitAtomicCmpXchg(f, ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicCmpXchg):
            CHECK(EmitAtomicCmpXchg(f, ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicCmpXchg8U):
            CHECK(EmitAtomicCmpXchg(f, ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicCmpXchg16U):
            CHECK(EmitAtomicCmpXchg(f, ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicCmpXchg8U):
            CHECK(EmitAtomicCmpXchg(f, ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicCmpXchg16U):
            CHECK(EmitAtomicCmpXchg(f, ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicCmpXchg32U):
            CHECK(EmitAtomicCmpXchg(f, ValType::I64, Scalar::Uint32));

          default:
            return f.iter().unrecognizedOpcode(&op);
        }
        break;
      }

      // asm.js-specific operators
      case uint16_t(Op::MozPrefix): {
        if (!f.moduleEnv().isAsmJS()) {
          return f.iter().unrecognizedOpcode(&op);
        }
        switch (op.b1) {
          case uint32_t(MozOp::TeeGlobal):
            CHECK(EmitTeeGlobal(f));
          case uint32_t(MozOp::I32Min):
          case uint32_t(MozOp::I32Max):
            CHECK(EmitMinMax(f, ValType::I32, MIRType::Int32,
                             MozOp(op.b1) == MozOp::I32Max));
          case uint32_t(MozOp::I32Neg):
            CHECK(EmitUnaryWithType<MWasmNeg>(f, ValType::I32, MIRType::Int32));
          case uint32_t(MozOp::I32BitNot):
            CHECK(EmitBitNot(f, ValType::I32));
          case uint32_t(MozOp::I32Abs):
            CHECK(EmitUnaryWithType<MAbs>(f, ValType::I32, MIRType::Int32));
          case uint32_t(MozOp::F32TeeStoreF64):
            CHECK(EmitTeeStoreWithCoercion(f, ValType::F32, Scalar::Float64));
          case uint32_t(MozOp::F64TeeStoreF32):
            CHECK(EmitTeeStoreWithCoercion(f, ValType::F64, Scalar::Float32));
          case uint32_t(MozOp::I32TeeStore8):
            CHECK(EmitTeeStore(f, ValType::I32, Scalar::Int8));
          case uint32_t(MozOp::I32TeeStore16):
            CHECK(EmitTeeStore(f, ValType::I32, Scalar::Int16));
          case uint32_t(MozOp::I64TeeStore8):
            CHECK(EmitTeeStore(f, ValType::I64, Scalar::Int8));
          case uint32_t(MozOp::I64TeeStore16):
            CHECK(EmitTeeStore(f, ValType::I64, Scalar::Int16));
          case uint32_t(MozOp::I64TeeStore32):
            CHECK(EmitTeeStore(f, ValType::I64, Scalar::Int32));
          case uint32_t(MozOp::I32TeeStore):
            CHECK(EmitTeeStore(f, ValType::I32, Scalar::Int32));
          case uint32_t(MozOp::I64TeeStore):
            CHECK(EmitTeeStore(f, ValType::I64, Scalar::Int64));
          case uint32_t(MozOp::F32TeeStore):
            CHECK(EmitTeeStore(f, ValType::F32, Scalar::Float32));
          case uint32_t(MozOp::F64TeeStore):
            CHECK(EmitTeeStore(f, ValType::F64, Scalar::Float64));
          case uint32_t(MozOp::F64Mod):
            CHECK(EmitRem(f, ValType::F64, MIRType::Double,
                          /* isUnsigned = */ false));
          case uint32_t(MozOp::F64Sin):
            CHECK(EmitUnaryMathBuiltinCall(f, SASigSinD));
          case uint32_t(MozOp::F64Cos):
            CHECK(EmitUnaryMathBuiltinCall(f, SASigCosD));
          case uint32_t(MozOp::F64Tan):
            CHECK(EmitUnaryMathBuiltinCall(f, SASigTanD));
          case uint32_t(MozOp::F64Asin):
            CHECK(EmitUnaryMathBuiltinCall(f, SASigASinD));
          case uint32_t(MozOp::F64Acos):
            CHECK(EmitUnaryMathBuiltinCall(f, SASigACosD));
          case uint32_t(MozOp::F64Atan):
            CHECK(EmitUnaryMathBuiltinCall(f, SASigATanD));
          case uint32_t(MozOp::F64Exp):
            CHECK(EmitUnaryMathBuiltinCall(f, SASigExpD));
          case uint32_t(MozOp::F64Log):
            CHECK(EmitUnaryMathBuiltinCall(f, SASigLogD));
          case uint32_t(MozOp::F64Pow):
            CHECK(EmitBinaryMathBuiltinCall(f, SASigPowD));
          case uint32_t(MozOp::F64Atan2):
            CHECK(EmitBinaryMathBuiltinCall(f, SASigATan2D));
          case uint32_t(MozOp::OldCallDirect):
            CHECK(EmitCall(f, /* asmJSFuncDef = */ true));
          case uint32_t(MozOp::OldCallIndirect):
            CHECK(EmitCallIndirect(f, /* oldStyle = */ true));

          default:
            return f.iter().unrecognizedOpcode(&op);
        }
        break;
      }

      default:
        return f.iter().unrecognizedOpcode(&op);
    }
  }

  MOZ_CRASH("unreachable");

#undef CHECK
}

bool wasm::IonCompileFunctions(const ModuleEnvironment& moduleEnv,
                               const CompilerEnvironment& compilerEnv,
                               LifoAlloc& lifo,
                               const FuncCompileInputVector& inputs,
                               CompiledCode* code, UniqueChars* error) {
  MOZ_ASSERT(compilerEnv.tier() == Tier::Optimized);
  MOZ_ASSERT(compilerEnv.debug() == DebugEnabled::False);
  MOZ_ASSERT(compilerEnv.optimizedBackend() == OptimizedBackend::Ion);

  TempAllocator alloc(&lifo);
  JitContext jitContext(&alloc);
  MOZ_ASSERT(IsCompilingWasm());
  WasmMacroAssembler masm(alloc, moduleEnv);
#if defined(JS_CODEGEN_ARM64)
  masm.SetStackPointer64(PseudoStackPointer64);
#endif

  // Swap in already-allocated empty vectors to avoid malloc/free.
  MOZ_ASSERT(code->empty());
  if (!code->swap(masm)) {
    return false;
  }

  // Create a description of the stack layout created by GenerateTrapExit().
  MachineState trapExitLayout;
  size_t trapExitLayoutNumWords;
  GenerateTrapExitMachineState(&trapExitLayout, &trapExitLayoutNumWords);

  for (const FuncCompileInput& func : inputs) {
    JitSpewCont(JitSpew_Codegen, "\n");
    JitSpew(JitSpew_Codegen,
            "# ================================"
            "==================================");
    JitSpew(JitSpew_Codegen, "# ==");
    JitSpew(JitSpew_Codegen,
            "# wasm::IonCompileFunctions: starting on function index %d",
            (int)func.index);

    Decoder d(func.begin, func.end, func.lineOrBytecode, error);

    // Build the local types vector.

    const FuncType& funcType = *moduleEnv.funcs[func.index].type;
    const TypeIdDesc& funcTypeId = *moduleEnv.funcs[func.index].typeId;
    ValTypeVector locals;
    if (!locals.appendAll(funcType.args())) {
      return false;
    }
    if (!DecodeLocalEntries(d, *moduleEnv.types, moduleEnv.features, &locals)) {
      return false;
    }

    // Set up for Ion compilation.

    const JitCompileOptions options;
    MIRGraph graph(&alloc);
    CompileInfo compileInfo(locals.length());
    MIRGenerator mir(nullptr, options, &alloc, &graph, &compileInfo,
                     IonOptimizations.get(OptimizationLevel::Wasm));
    if (moduleEnv.usesMemory()) {
      if (moduleEnv.memory->indexType() == IndexType::I32) {
        mir.initMinWasmHeapLength(moduleEnv.memory->initialLength32());
      } else {
        mir.initMinWasmHeapLength(moduleEnv.memory->initialLength64());
      }
    }

    // Build MIR graph
    {
      FunctionCompiler f(moduleEnv, d, func, locals, mir);
      if (!f.init()) {
        return false;
      }

      if (!f.startBlock()) {
        return false;
      }

      if (!EmitBodyExprs(f)) {
        return false;
      }

      f.finish();
    }

    // Compile MIR graph
    {
      jit::SpewBeginWasmFunction(&mir, func.index);
      jit::AutoSpewEndFunction spewEndFunction(&mir);

      if (!OptimizeMIR(&mir)) {
        return false;
      }

      LIRGraph* lir = GenerateLIR(&mir);
      if (!lir) {
        return false;
      }

      CodeGenerator codegen(&mir, lir, &masm);

      BytecodeOffset prologueTrapOffset(func.lineOrBytecode);
      FuncOffsets offsets;
      ArgTypeVector args(funcType);
      if (!codegen.generateWasm(funcTypeId, prologueTrapOffset, args,
                                trapExitLayout, trapExitLayoutNumWords,
                                &offsets, &code->stackMaps, &d)) {
        return false;
      }

      if (!code->codeRanges.emplaceBack(func.index, func.lineOrBytecode,
                                        offsets)) {
        return false;
      }
    }

    JitSpew(JitSpew_Codegen,
            "# wasm::IonCompileFunctions: completed function index %d",
            (int)func.index);
    JitSpew(JitSpew_Codegen, "# ==");
    JitSpew(JitSpew_Codegen,
            "# ================================"
            "==================================");
    JitSpewCont(JitSpew_Codegen, "\n");
  }

  masm.finish();
  if (masm.oom()) {
    return false;
  }

  return code->swap(masm);
}

bool js::wasm::IonPlatformSupport() {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) ||    \
    defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS64) || \
    defined(JS_CODEGEN_ARM64)
  return true;
#else
  return false;
#endif
}

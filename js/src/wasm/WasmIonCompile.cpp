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

struct Control {
  MBasicBlock* block;
  // For a try-catch ControlItem, when its block's Labelkind is Try, this
  // collects branches to later bind and create the try's landing pad.
  ControlInstructionVector tryPadPatches;

  Control() : block(nullptr) {}

  explicit Control(MBasicBlock* block) : block(block) {}

 public:
  void setBlock(MBasicBlock* newBlock) { block = newBlock; }
};

// [SMDOC] WebAssembly Exception Handling in Ion
// =======================================================
//
// ## Throwing instructions
//
// Wasm exceptions can be thrown by either a throw instruction (local throw),
// or by a wasm call.
//
// ## The "catching try control"
//
// We know we are in try-code if there is a surrounding ControlItem with
// LabelKind::Try. The innermost such control is called the
// "catching try control".
//
// ## Throws without a catching try control
//
// Such throws are implemented with an instance call that triggers the exception
// unwinding runtime. The exception unwinding runtime will not return to the
// function.
//
// ## "landing pad" and "pre-pad" blocks
//
// When an exception is thrown, the unwinder will search for the nearest
// enclosing try block and redirect control flow to it. The code that executes
// before any catch blocks is called the 'landing pad'. The 'landing pad' is
// responsible to:
//   1. Consume the pending exception state from
//      Instance::pendingException(Tag)
//   2. Branch to the correct catch block, or else rethrow
//
// There is one landing pad for each try block. The immediate predecessors of
// the landing pad are called 'pre-pad' blocks. There is one pre-pad block per
// throwing instruction.
//
// ## Creating pre-pad blocks
//
// There are two possible sorts of pre-pad blocks, depending on whether we
// are branching after a local throw instruction, or after a wasm call:
//
// - If we encounter a local throw, we create the exception and tag objects,
//   store them to Instance::pendingException(Tag), and then jump to the
//   landing pad.
//
// - If we encounter a wasm call, we construct a MWasmCallCatchable which is a
//   control instruction with either a branch to a fallthrough block or
//   to a pre-pad block.
//
//   The pre-pad block for a wasm call is empty except for a jump to the
//   landing pad. It only exists to avoid critical edges which when split would
//   violate the invariants of MWasmCallCatchable. The pending exception state
//   is taken care of by the unwinder.
//
// Each pre-pad ends with a pending jump to the landing pad. The pending jumps
// to the landing pad are tracked in `tryPadPatches`. These are called
// "pad patches".
//
// ## Creating the landing pad
//
// When we exit try-code, we check if tryPadPatches has captured any control
// instructions (pad patches). If not, we don't compile any catches and we mark
// the rest as dead code.
//
// If there are pre-pad blocks, we join them to create a landing pad (or just
// "pad"). The pad's last two slots are the caught exception, and the
// exception's tag object.
//
// There are three different forms of try-catch/catch_all Wasm instructions,
// which result in different form of landing pad.
//
// 1. A catchless try, so a Wasm instruction of the form "try ... end".
//    - In this case, we end the pad by rethrowing the caught exception.
//
// 2. A single catch_all after a try.
//    - If the first catch after a try is a catch_all, then there won't be
//      any more catches, but we need the exception and its tag object, in
//      case the code in a catch_all contains "rethrow" instructions.
//      - The Wasm instruction "rethrow", gets the exception and tag object to
//        rethrow from the last two slots of the landing pad which, due to
//        validation, is the l'th surrounding ControlItem.
//      - We immediately GoTo to a new block after the pad and pop both the
//        exception and tag object, as we don't need them anymore in this case.
//
// 3. Otherwise, there is one or more catch code blocks following.
//    - In this case, we construct the landing pad by creating a sequence
//      of compare and branch blocks that compare the pending exception tag
//      object to the tag object of the current tagged catch block. This is
//      done incrementally as we visit each tagged catch block in the bytecode
//      stream. At every step, we update the ControlItem's block to point to
//      the next block to be created in the landing pad sequence. The final
//      block will either be a rethrow, if there is no catch_all, or else a
//      jump to a catch_all block.

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
  MWasmCallBase::Args regArgs_;

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

  // Instance pointer argument to the current function.
  MWasmParameter* instancePointer_;
  MWasmParameter* stackResultPointer_;

  // Reference to masm.tryNotes_
  wasm::TryNoteVector& tryNotes_;

 public:
  FunctionCompiler(const ModuleEnvironment& moduleEnv, Decoder& decoder,
                   const FuncCompileInput& func, const ValTypeVector& locals,
                   MIRGenerator& mirGen, TryNoteVector& tryNotes)
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
        instancePointer_(nullptr),
        stackResultPointer_(nullptr),
        tryNotes_(tryNotes) {}

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

    // Set up a parameter that receives the hidden instance pointer argument.
    instancePointer_ =
        MWasmParameter::New(alloc(), ABIArg(InstanceReg), MIRType::Pointer);
    curBlock_->add(instancePointer_);
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
    // A call to c++ builtin requires instance pointer.
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    if (type == MIRType::Int64) {
      auto* ins =
          MWasmBuiltinDivI64::New(alloc(), lhs, rhs, instancePointer_, unsignd,
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
      return MWasmBuiltinTruncateToInt32::New(alloc(), op, instancePointer_);
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
    // A call to c++ builtin requires instance pointer.
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    if (type == MIRType::Int64) {
      auto* ins =
          MWasmBuiltinModI64::New(alloc(), lhs, rhs, instancePointer_, unsignd,
                                  trapOnError, bytecodeOffset());
      curBlock_->add(ins);
      return ins;
    }
#endif

    // Should be handled separately because we call BuiltinThunk for this case
    // and so, need to add the dependency from instancePointer.
    if (type == MIRType::Double) {
      auto* ins = MWasmBuiltinModD::New(alloc(), lhs, rhs, instancePointer_,
                                        type, bytecodeOffset());
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
        alloc(), op, instancePointer_, type, bytecodeOffset(), isUnsigned);
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
  MDefinition* truncateWithInstance(MDefinition* op, TruncFlags flags) {
    if (inDeadCode()) {
      return nullptr;
    }
    auto* ins = MWasmBuiltinTruncateToInt64::New(alloc(), op, instancePointer_,
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
  // If the platform does not have a HeapReg, load the memory base from
  // instance.
  MWasmLoadInstance* maybeLoadMemoryBase() {
    MWasmLoadInstance* load = nullptr;
#ifdef JS_CODEGEN_X86
    AliasSet aliases = !moduleEnv_.memory->canMovingGrow()
                           ? AliasSet::None()
                           : AliasSet::Load(AliasSet::WasmHeapMeta);
    load = MWasmLoadInstance::New(alloc(), instancePointer_,
                                  wasm::Instance::offsetOfMemoryBase(),
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
    base = MWasmHeapBase::New(alloc(), instancePointer_, aliases);
    curBlock_->add(base);
    return base;
  }

 private:
  // If the bounds checking strategy requires it, load the bounds check limit
  // from the instance.
  MWasmLoadInstance* maybeLoadBoundsCheckLimit(MIRType type) {
    MOZ_ASSERT(type == MIRType::Int32 || type == MIRType::Int64);
    if (moduleEnv_.hugeMemoryEnabled()) {
      return nullptr;
    }
    AliasSet aliases = !moduleEnv_.memory->canMovingGrow()
                           ? AliasSet::None()
                           : AliasSet::Load(AliasSet::WasmHeapMeta);
    auto* load = MWasmLoadInstance::New(
        alloc(), instancePointer_, wasm::Instance::offsetOfBoundsCheckLimit(),
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

  MWasmLoadInstance* needBoundsCheck() {
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

  void performBoundsCheck(MDefinition** base,
                          MWasmLoadInstance* boundsCheckLimit) {
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

    auto* ins =
        MWasmBoundsCheck::New(alloc(), actualBase, boundsCheckLimit,
                              bytecodeOffset(), MWasmBoundsCheck::Memory);
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
    MWasmLoadInstance* boundsCheckLimit = needBoundsCheck();
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

    MWasmLoadInstance* memoryBase = maybeLoadMemoryBase();
    MInstruction* load = nullptr;
    if (moduleEnv_.isAsmJS()) {
      MOZ_ASSERT(access->offset64() == 0);
      MWasmLoadInstance* boundsCheckLimit =
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

    MWasmLoadInstance* memoryBase = maybeLoadMemoryBase();
    MInstruction* store = nullptr;
    if (moduleEnv_.isAsmJS()) {
      MOZ_ASSERT(access->offset64() == 0);
      MWasmLoadInstance* boundsCheckLimit =
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

    MWasmLoadInstance* memoryBase = maybeLoadMemoryBase();
    MInstruction* cas = MWasmCompareExchangeHeap::New(
        alloc(), bytecodeOffset(), memoryBase, base, *access, oldv, newv,
        instancePointer_);
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

    MWasmLoadInstance* memoryBase = maybeLoadMemoryBase();
    MInstruction* xchg =
        MWasmAtomicExchangeHeap::New(alloc(), bytecodeOffset(), memoryBase,
                                     base, *access, value, instancePointer_);
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

    MWasmLoadInstance* memoryBase = maybeLoadMemoryBase();
    MInstruction* binop =
        MWasmAtomicBinopHeap::New(alloc(), bytecodeOffset(), op, memoryBase,
                                  base, *access, value, instancePointer_);
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
    MWasmLoadInstance* memoryBase = maybeLoadMemoryBase();
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
    MWasmLoadInstance* memoryBase = maybeLoadMemoryBase();
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
      // Pull a pointer to the value out of Instance::globalArea, then
      // load from that pointer.  Note that the pointer is immutable
      // even though the value it points at may change, hence the use of
      // |true| for the first node's |isConst| value, irrespective of
      // the |isConst| formal parameter to this method.  The latter
      // applies to the denoted value as a whole.
      auto* cellPtr =
          MWasmLoadGlobalVar::New(alloc(), MIRType::Pointer, globalDataOffset,
                                  /*isConst=*/true, instancePointer_);
      curBlock_->add(cellPtr);
      load = MWasmLoadGlobalCell::New(alloc(), type, cellPtr);
    } else {
      // Pull the value directly out of Instance::globalArea.
      load = MWasmLoadGlobalVar::New(alloc(), type, globalDataOffset, isConst,
                                     instancePointer_);
    }
    curBlock_->add(load);
    return load;
  }

  bool storeGlobalVar(uint32_t lineOrBytecode, uint32_t globalDataOffset,
                      bool isIndirect, MDefinition* v) {
    if (inDeadCode()) {
      return true;
    }

    if (isIndirect) {
      // Pull a pointer to the value out of Instance::globalArea, then
      // store through that pointer.
      auto* valueAddr =
          MWasmLoadGlobalVar::New(alloc(), MIRType::Pointer, globalDataOffset,
                                  /*isConst=*/true, instancePointer_);
      curBlock_->add(valueAddr);

      // Handle a store to a ref-typed field specially
      if (v->type() == MIRType::RefOrNull) {
        // Load the previous value for the post-write barrier
        auto* prevValue =
            MWasmLoadGlobalCell::New(alloc(), MIRType::RefOrNull, valueAddr);
        curBlock_->add(prevValue);

        // Store the new value
        auto* store = MWasmStoreRef::New(alloc(), instancePointer_, valueAddr,
                                         v, AliasSet::WasmGlobalCell);
        curBlock_->add(store);

        // Call the post-write barrier
        return postBarrierPrecise(lineOrBytecode, valueAddr, prevValue);
      }

      auto* store = MWasmStoreGlobalCell::New(alloc(), v, valueAddr);
      curBlock_->add(store);
      return true;
    }
    // Or else store the value directly in Instance::globalArea.

    // Handle a store to a ref-typed field specially
    if (v->type() == MIRType::RefOrNull) {
      // Compute the address of the ref-typed global
      auto* valueAddr = MWasmDerivedPointer::New(
          alloc(), instancePointer_,
          wasm::Instance::offsetOfGlobalArea() + globalDataOffset);
      curBlock_->add(valueAddr);

      // Load the previous value for the post-write barrier
      auto* prevValue =
          MWasmLoadGlobalCell::New(alloc(), MIRType::RefOrNull, valueAddr);
      curBlock_->add(prevValue);

      // Store the new value
      auto* store = MWasmStoreRef::New(alloc(), instancePointer_, valueAddr, v,
                                       AliasSet::WasmGlobalVar);
      curBlock_->add(store);

      // Call the post-write barrier
      return postBarrierPrecise(lineOrBytecode, valueAddr, prevValue);
    }

    auto* store = MWasmStoreGlobalVar::New(alloc(), globalDataOffset, v,
                                           instancePointer_);
    curBlock_->add(store);
    return true;
  }

  MDefinition* loadTableField(const TableDesc& table, unsigned fieldOffset,
                              MIRType type) {
    uint32_t globalDataOffset = wasm::Instance::offsetOfGlobalArea() +
                                table.globalDataOffset + fieldOffset;
    auto* load =
        MWasmLoadInstance::New(alloc(), instancePointer_, globalDataOffset,
                               type, AliasSet::Load(AliasSet::WasmTableMeta));
    curBlock_->add(load);
    return load;
  }

  MDefinition* loadTableLength(const TableDesc& table) {
    return loadTableField(table, offsetof(TableInstanceData, length),
                          MIRType::Int32);
  }

  MDefinition* loadTableElements(const TableDesc& table) {
    return loadTableField(table, offsetof(TableInstanceData, elements),
                          MIRType::Pointer);
  }

  MDefinition* tableGetAnyRef(const TableDesc& table, MDefinition* index) {
    // Load the table length and perform a bounds check with spectre index
    // masking
    auto* length = loadTableLength(table);
    auto* check = MWasmBoundsCheck::New(
        alloc(), index, length, bytecodeOffset(), MWasmBoundsCheck::Table);
    curBlock_->add(check);
    if (JitOptions.spectreIndexMasking) {
      index = check;
    }

    // Load the table elements and load the element
    auto* elements = loadTableElements(table);
    auto* element = MWasmLoadTableElement::New(alloc(), elements, index);
    curBlock_->add(element);
    return element;
  }

  [[nodiscard]] bool tableSetAnyRef(const TableDesc& table, MDefinition* index,
                                    MDefinition* value,
                                    uint32_t lineOrBytecode) {
    // Load the table length and perform a bounds check with spectre index
    // masking
    auto* length = loadTableLength(table);
    auto* check = MWasmBoundsCheck::New(
        alloc(), index, length, bytecodeOffset(), MWasmBoundsCheck::Table);
    curBlock_->add(check);
    if (JitOptions.spectreIndexMasking) {
      index = check;
    }

    // Load the table elements
    auto* elements = loadTableElements(table);

    // Load the previous value
    auto* prevValue = MWasmLoadTableElement::New(alloc(), elements, index);
    curBlock_->add(prevValue);

    // Compute the value's location for the post barrier
    auto* loc =
        MWasmDerivedIndexPointer::New(alloc(), elements, index, ScalePointer);
    curBlock_->add(loc);

    // Store the new value
    auto* store = MWasmStoreRef::New(alloc(), instancePointer_, loc, value,
                                     AliasSet::WasmTableElement);
    curBlock_->add(store);

    // Perform the post barrier
    return postBarrierPrecise(lineOrBytecode, loc, prevValue);
  }

  void addInterruptCheck() {
    if (inDeadCode()) {
      return;
    }
    curBlock_->add(
        MWasmInterruptCheck::New(alloc(), instancePointer_, bytecodeOffset()));
  }

  bool postBarrierPrecise(uint32_t lineOrBytecode, MDefinition* valueAddr,
                          MDefinition* value) {
    const SymbolicAddressSignature& callee = SASigPostBarrierPrecise;
    CallCompileState args;
    if (!passInstance(callee.argTypes[0], &args)) {
      return false;
    }
    if (!passArg(valueAddr, callee.argTypes[1], &args)) {
      return false;
    }
    if (!passArg(value, callee.argTypes[2], &args)) {
      return false;
    }
    finishCall(&args);
    if (!builtinInstanceMethodCall(callee, lineOrBytecode, args)) {
      return false;
    }
    return true;
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
                   MWasmCallBase::Arg(AnyRegister(arg.gpr64().low), mirLow)) &&
               call->regArgs_.append(
                   MWasmCallBase::Arg(AnyRegister(arg.gpr64().high), mirHigh));
      }
#endif
      case ABIArg::GPR:
      case ABIArg::FPU:
        return call->regArgs_.append(MWasmCallBase::Arg(arg.reg(), argDef));
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
            MWasmCallBase::Arg(AnyRegister(InstanceReg), instancePointer_))) {
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

  bool catchableCall(const CallSiteDesc& desc, const CalleeDesc& callee,
                     const MWasmCallBase::Args& args,
                     const ArgTypeVector& argTypes,
                     MDefinition* index = nullptr) {
    MWasmCallTryDesc tryDesc;
    if (!beginTryCall(&tryDesc)) {
      return false;
    }

    MInstruction* ins;
    if (tryDesc.inTry) {
      ins = MWasmCallCatchable::New(alloc(), desc, callee, args,
                                    StackArgAreaSizeUnaligned(argTypes),
                                    tryDesc, index);
    } else {
      ins =
          MWasmCallUncatchable::New(alloc(), desc, callee, args,
                                    StackArgAreaSizeUnaligned(argTypes), index);
    }
    if (!ins) {
      return false;
    }
    curBlock_->add(ins);

    if (!finishTryCall(&tryDesc)) {
      return false;
    }
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

    if (!catchableCall(desc, callee, call.regArgs_, args)) {
      return false;
    }
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

    if (!catchableCall(desc, callee, call.regArgs_, args, index)) {
      return false;
    }
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

    if (!catchableCall(desc, callee, call.regArgs_, args)) {
      return false;
    }
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
    auto* ins = MWasmCallUncatchable::New(alloc(), desc, callee, call.regArgs_,
                                          StackArgAreaSizeUnaligned(builtin));
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
    auto* ins = MWasmCallUncatchable::NewBuiltinInstanceMethodCall(
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
      curBlock_->end(MWasmReturnVoid::New(alloc(), instancePointer_));
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
                MWasmStoreRef::New(alloc(), instancePointer_, loc, values[i],
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
          curBlock_->end(
              MWasmReturn::New(alloc(), values[i], instancePointer_));
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
      LabelKind kind = iter().controlKind(depth);
      if (kind != LabelKind::Try && kind != LabelKind::Body) {
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

  bool inTryBlock(uint32_t* relativeDepth) {
    return iter().controlFindInnermost(LabelKind::Try, relativeDepth);
  }

  bool inTryCode() {
    uint32_t relativeDepth;
    return inTryBlock(&relativeDepth);
  }

  MDefinition* loadTag(uint32_t tagIndex) {
    MWasmLoadGlobalVar* tag = MWasmLoadGlobalVar::New(
        alloc(), MIRType::RefOrNull, moduleEnv_.tags[tagIndex].globalDataOffset,
        true, instancePointer_);
    curBlock_->add(tag);
    return tag;
  }

  void loadPendingExceptionState(MInstruction** exception, MInstruction** tag) {
    *exception = MWasmLoadInstance::New(
        alloc(), instancePointer_, wasm::Instance::offsetOfPendingException(),
        MIRType::RefOrNull, AliasSet::Load(AliasSet::WasmPendingException));
    curBlock_->add(*exception);

    *tag = MWasmLoadInstance::New(
        alloc(), instancePointer_,
        wasm::Instance::offsetOfPendingExceptionTag(), MIRType::RefOrNull,
        AliasSet::Load(AliasSet::WasmPendingException));
    curBlock_->add(*tag);
  }

  bool setPendingExceptionState(MDefinition* exception, MDefinition* tag) {
    // Set the pending exception object
    auto* exceptionAddr = MWasmDerivedPointer::New(
        alloc(), instancePointer_, Instance::offsetOfPendingException());
    curBlock_->add(exceptionAddr);
    auto* setException =
        MWasmStoreRef::New(alloc(), instancePointer_, exceptionAddr, exception,
                           AliasSet::WasmPendingException);
    curBlock_->add(setException);
    if (!postBarrierPrecise(0, exceptionAddr, exception)) {
      return false;
    }

    // Set the pending exception tag object
    auto* exceptionTagAddr = MWasmDerivedPointer::New(
        alloc(), instancePointer_, Instance::offsetOfPendingExceptionTag());
    curBlock_->add(exceptionTagAddr);
    auto* setExceptionTag =
        MWasmStoreRef::New(alloc(), instancePointer_, exceptionTagAddr, tag,
                           AliasSet::WasmPendingException);
    curBlock_->add(setExceptionTag);
    return postBarrierPrecise(0, exceptionTagAddr, tag);
  }

  bool addPadPatch(MControlInstruction* ins, size_t relativeTryDepth) {
    Control& tryControl = iter().controlItem(relativeTryDepth);
    ControlInstructionVector& padPatches = tryControl.tryPadPatches;
    return padPatches.emplaceBack(ins);
  }

  bool endWithPadPatch(uint32_t relativeTryDepth) {
    MGoto* jumpToLandingPad = MGoto::New(alloc());
    curBlock_->end(jumpToLandingPad);
    return addPadPatch(jumpToLandingPad, relativeTryDepth);
  }

  bool delegatePadPatches(const ControlInstructionVector& patches,
                          uint32_t relativeDepth) {
    if (patches.empty()) {
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
    for (MControlInstruction* ins : patches) {
      if (!addPadPatch(ins, targetRelativeDepth)) {
        return false;
      }
    }
    return true;
  }

  bool beginTryCall(MWasmCallTryDesc* call) {
    call->inTry = inTryBlock(&call->relativeTryDepth);
    if (!call->inTry) {
      return true;
    }
    // Allocate a try note
    if (!tryNotes_.append(wasm::TryNote())) {
      return false;
    }
    call->tryNoteIndex = tryNotes_.length() - 1;
    // Allocate blocks for fallthrough and exceptions
    return newBlock(curBlock_, &call->fallthroughBlock) &&
           newBlock(curBlock_, &call->prePadBlock);
  }

  bool finishTryCall(MWasmCallTryDesc* call) {
    if (!call->inTry) {
      return true;
    }

    // Switch to the prePadBlock
    MBasicBlock* callBlock = curBlock_;
    curBlock_ = call->prePadBlock;

    // Mark this as the landing pad for the call
    curBlock_->add(
        MWasmCallLandingPrePad::New(alloc(), callBlock, call->tryNoteIndex));

    // End with a pending jump to the landing pad
    if (!endWithPadPatch(call->relativeTryDepth)) {
      return false;
    }

    // Compilation continues in the fallthroughBlock.
    curBlock_ = call->fallthroughBlock;
    return true;
  }

  // Create a landing pad for a try block if there are any throwing
  // instructions.
  bool createTryLandingPadIfNeeded(Control& control, MBasicBlock** landingPad) {
    // If there are no pad-patches for this try control, it means there are no
    // instructions in the try code that could throw an exception. In this
    // case, all the catches are dead code, and the try code ends up equivalent
    // to a plain wasm block.
    ControlInstructionVector& patches = control.tryPadPatches;
    if (patches.empty()) {
      *landingPad = nullptr;
      return true;
    }

    // Otherwise, if there are (pad-) branches from places in the try code that
    // may throw an exception, bind these branches to a new landing pad
    // block. This is done similarly to what is done in bindBranches.
    MControlInstruction* ins = patches[0];
    MBasicBlock* pred = ins->block();
    if (!newBlock(pred, landingPad)) {
      return false;
    }
    ins->replaceSuccessor(0, *landingPad);
    for (size_t i = 1; i < patches.length(); i++) {
      ins = patches[i];
      pred = ins->block();
      if (!(*landingPad)->addPredecessor(alloc(), pred)) {
        return false;
      }
      ins->replaceSuccessor(0, *landingPad);
    }

    // Set up the slots in the landing pad block.
    if (!setupLandingPadSlots(*landingPad)) {
      return false;
    }

    // Clear the now bound pad patches.
    patches.clear();
    return true;
  }

  // Consume the pending exception state from instance, and set up the slots
  // of the landing pad with the exception state.
  bool setupLandingPadSlots(MBasicBlock* landingPad) {
    MBasicBlock* prevBlock = curBlock_;
    curBlock_ = landingPad;

    // Load the pending exception and tag
    MInstruction* exception;
    MInstruction* tag;
    loadPendingExceptionState(&exception, &tag);

    // Clear the pending exception and tag
    auto* null = nullRefConstant();
    if (!setPendingExceptionState(null, null)) {
      return false;
    }

    // Push the exception and its tag on the stack to make them available
    // to the landing pad blocks.
    if (!landingPad->ensureHasSlots(2)) {
      return false;
    }
    landingPad->push(exception);
    landingPad->push(tag);

    curBlock_ = prevBlock;
    return true;
  }

  bool startTry(MBasicBlock** curBlock) {
    *curBlock = curBlock_;
    return startBlock();
  }

  bool joinTryOrCatchBlock(Control& control) {
    // If the try or catch block ended with dead code, there is no need to
    // do any control flow join.
    if (inDeadCode()) {
      return true;
    }

    // This is a split path which we'll need to join later, using a control
    // flow patch.
    MOZ_ASSERT(!curBlock_->hasLastIns());
    MGoto* jump = MGoto::New(alloc());
    if (!addControlFlowPatch(jump, 0, MGoto::TargetIndex)) {
      return false;
    }

    // Finish the current block with the control flow patch instruction.
    curBlock_->end(jump);
    return true;
  }

  // Finish the previous block (either a try or catch block) and then setup a
  // new catch block.
  bool switchToCatch(Control& control, const LabelKind& fromKind,
                     uint32_t tagIndex) {
    // If there is no control block, then either:
    //   - the entry of the try block is dead code, or
    //   - there is no landing pad for the try-catch.
    // In either case, any catch will be dead code.
    if (!control.block) {
      MOZ_ASSERT(inDeadCode());
      return true;
    }

    // Join the previous try or catch block with a patch to the future join of
    // the whole try-catch block.
    if (!joinTryOrCatchBlock(control)) {
      return false;
    }

    // If we are switching from the try block, create the landing pad. This is
    // guaranteed to happen once and only once before processing catch blocks.
    if (fromKind == LabelKind::Try) {
      MBasicBlock* padBlock = nullptr;
      if (!createTryLandingPadIfNeeded(control, &padBlock)) {
        return false;
      }
      // Set the control block for this try-catch to the landing pad.
      control.block = padBlock;
    }

    // If there is no landing pad, then this and following catches are dead
    // code.
    if (!control.block) {
      curBlock_ = nullptr;
      return true;
    }

    // Switch to the landing pad.
    curBlock_ = control.block;

    // Handle a catch_all by immediately jumping to a new block. We require a
    // new block (as opposed to just emitting the catch_all code in the current
    // block) because rethrow requires the exception/tag to be present in the
    // landing pad's slots, while the catch_all block must not have the
    // exception/tag in slots.
    if (tagIndex == CatchAllIndex) {
      MBasicBlock* catchAllBlock = nullptr;
      if (!goToNewBlock(curBlock_, &catchAllBlock)) {
        return false;
      }
      // Compilation will continue in the catch_all block.
      curBlock_ = catchAllBlock;
      // Remove the tag and exception slots from the block, they are no
      // longer necessary.
      curBlock_->pop();
      curBlock_->pop();
      return true;
    }

    // Handle a tagged catch by doing a compare and branch on the tag index,
    // jumping to a catch block if they match, or else to a fallthrough block
    // to continue the landing pad.
    MBasicBlock* catchBlock = nullptr;
    MBasicBlock* fallthroughBlock = nullptr;
    if (!newBlock(curBlock_, &catchBlock) ||
        !newBlock(curBlock_, &fallthroughBlock)) {
      return false;
    }

    // Get the exception and its tag from the slots we pushed when adding
    // control flow patches.
    MDefinition* exceptionTag = curBlock_->pop();
    MDefinition* exception = curBlock_->pop();

    // Branch to the catch block if the exception's tag matches this catch
    // block's tag.
    MDefinition* catchTag = loadTag(tagIndex);
    MDefinition* matchesCatchTag =
        compare(exceptionTag, catchTag, JSOp::Eq, MCompare::Compare_RefOrNull);
    curBlock_->end(
        MTest::New(alloc(), matchesCatchTag, catchBlock, fallthroughBlock));

    // The landing pad will continue in the fallthrough block
    control.block = fallthroughBlock;

    // Set up the catch block by extracting the values from the exception
    // object.
    curBlock_ = catchBlock;

    // Remove the tag and exception slots from the block, they are no
    // longer necessary.
    curBlock_->pop();
    curBlock_->pop();

    // Extract the exception values for the catch block
    DefVector values;
    if (!loadExceptionValues(exception, tagIndex, &values)) {
      return false;
    }
    iter().setResults(values.length(), values);
    return true;
  }

  bool loadExceptionValues(MDefinition* exception, uint32_t tagIndex,
                           DefVector* values) {
    SharedTagType tagType = moduleEnv().tags[tagIndex].type;
    const ValTypeVector& params = tagType->argTypes_;
    const TagOffsetVector& offsets = tagType->argOffsets_;

    // Get the data pointer from the exception object
    auto* data = MWasmLoadObjectField::New(alloc(), exception,
                                           WasmExceptionObject::offsetOfData(),
                                           MIRType::Pointer);
    curBlock_->add(data);

    // Presize the values vector to the number of params
    if (!values->reserve(params.length())) {
      return false;
    }

    // Load each value from the data pointer
    for (size_t i = 0; i < params.length(); i++) {
      auto* load = MWasmLoadObjectDataField::New(
          alloc(), exception, data, offsets[i], ToMIRType(params[i]));
      if (!load || !values->append(load)) {
        return false;
      }
      curBlock_->add(load);
    }
    return true;
  }

  bool finishTryCatch(LabelKind kind, Control& control, DefVector* defs) {
    switch (kind) {
      case LabelKind::Try: {
        // This is a catchless try, we must delegate all throwing instructions
        // to the nearest enclosing try block if one exists, or else to the
        // body block which will handle it in emitBodyDelegateThrowPad. We
        // specify a relativeDepth of '1' to delegate outside of the still
        // active try block.
        uint32_t relativeDepth = 1;
        if (!delegatePadPatches(control.tryPadPatches, relativeDepth)) {
          return false;
        }
        break;
      }
      case LabelKind::Catch: {
        // This is a try without a catch_all, we must have a rethrow at the end
        // of the landing pad (if any).
        MBasicBlock* padBlock = control.block;
        if (padBlock) {
          MBasicBlock* prevBlock = curBlock_;
          curBlock_ = padBlock;
          MDefinition* tag = curBlock_->pop();
          MDefinition* exception = curBlock_->pop();
          if (!throwFrom(exception, tag)) {
            return false;
          }
          curBlock_ = prevBlock;
        }
        break;
      }
      case LabelKind::CatchAll:
        // This is a try with a catch_all, and requires no special handling.
        break;
      default:
        MOZ_CRASH();
    }

    // Finish the block, joining the try and catch blocks
    return finishBlock(defs);
  }

  bool emitBodyDelegateThrowPad(Control& control) {
    // Create a landing pad for any throwing instructions
    MBasicBlock* padBlock;
    if (!createTryLandingPadIfNeeded(control, &padBlock)) {
      return false;
    }

    // If no landing pad was necessary, then we don't need to do anything here
    if (!padBlock) {
      return true;
    }

    // Switch to the landing pad and rethrow the exception
    MBasicBlock* prevBlock = curBlock_;
    curBlock_ = padBlock;
    MDefinition* tag = curBlock_->pop();
    MDefinition* exception = curBlock_->pop();
    if (!throwFrom(exception, tag)) {
      return false;
    }
    curBlock_ = prevBlock;
    return true;
  }

  bool emitNewException(MDefinition* tag, MDefinition** exception) {
    uint32_t bytecodeOffset = readBytecodeOffset();
    const SymbolicAddressSignature& callee = SASigExceptionNew;
    CallCompileState args;
    if (!passInstance(callee.argTypes[0], &args)) {
      return false;
    }
    if (!passArg(tag, callee.argTypes[1], &args)) {
      return false;
    }
    if (!finishCall(&args)) {
      return false;
    }
    return builtinInstanceMethodCall(callee, bytecodeOffset, args, exception);
  }

  bool emitThrow(uint32_t tagIndex, const DefVector& argValues) {
    if (inDeadCode()) {
      return true;
    }
    uint32_t bytecodeOffset = readBytecodeOffset();

    // Load the tag
    MDefinition* tag = loadTag(tagIndex);
    if (!tag) {
      return false;
    }

    // Allocate an exception object
    MDefinition* exception;
    if (!emitNewException(tag, &exception)) {
      return false;
    }

    // Load the data pointer from the object
    auto* data = MWasmLoadObjectField::New(alloc(), exception,
                                           WasmExceptionObject::offsetOfData(),
                                           MIRType::Pointer);
    if (!data) {
      return false;
    }
    curBlock_->add(data);

    // Store the params into the data pointer
    SharedTagType tagType = moduleEnv_.tags[tagIndex].type;
    for (size_t i = 0; i < tagType->argOffsets_.length(); i++) {
      ValType type = tagType->argTypes_[i];
      uint32_t offset = tagType->argOffsets_[i];

      if (!type.isRefRepr()) {
        auto* store = MWasmStoreObjectDataField::New(alloc(), exception, data,
                                                     offset, argValues[i]);
        if (!store) {
          return false;
        }
        curBlock_->add(store);
        continue;
      }

      // Compute the address of the field
      auto* fieldAddr = MWasmDerivedPointer::New(alloc(), data, offset);
      if (!fieldAddr) {
        return false;
      }
      curBlock_->add(fieldAddr);

      // Load the previous value
      auto* prevValue = MWasmLoadObjectDataField::New(alloc(), exception, data,
                                                      offset, ToMIRType(type));
      if (!prevValue) {
        return false;
      }
      curBlock_->add(prevValue);

      // Store the new value
      auto* store = MWasmStoreObjectDataRefField::New(
          alloc(), instancePointer_, exception, fieldAddr, argValues[i]);
      if (!store) {
        return false;
      }
      curBlock_->add(store);

      // Call the post-write barrier
      if (!postBarrierPrecise(bytecodeOffset, fieldAddr, prevValue)) {
        return false;
      }
    }

    // Throw the exception
    return throwFrom(exception, tag);
  }

  bool throwFrom(MDefinition* exn, MDefinition* tag) {
    if (inDeadCode()) {
      return true;
    }

    // Check if there is a local catching try control, and if so, then add a
    // pad-patch to its tryPadPatches.
    uint32_t relativeTryDepth;
    if (inTryBlock(&relativeTryDepth)) {
      // Set the pending exception state, the landing pad will read from this
      if (!setPendingExceptionState(exn, tag)) {
        return false;
      }

      // End with a pending jump to the landing pad
      if (!endWithPadPatch(relativeTryDepth)) {
        return false;
      }
      curBlock_ = nullptr;
      return true;
    }

    // If there is no surrounding catching block, call an instance method to
    // throw the exception.
    uint32_t bytecodeOffset = readBytecodeOffset();
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
    if (!builtinInstanceMethodCall(callee, bytecodeOffset, args)) {
      return false;
    }
    unreachableTrap();

    curBlock_ = nullptr;
    return true;
  }

  bool emitRethrow(uint32_t relativeDepth) {
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
    MDefinition* tag = pad->getSlot(exnSlotPosition + 1);
    MDefinition* exception = pad->getSlot(exnSlotPosition);
    MOZ_ASSERT(exception->type() == MIRType::RefOrNull &&
               tag->type() == MIRType::RefOrNull);
    return throwFrom(exception, tag);
  }

  /************************************************************ DECODING ***/

  // AsmJS adds a line number to `callSiteLineNums` for certain operations that
  // are represented by a JS call, such as math builtins. We use these line
  // numbers when calling builtins. This method will read from
  // `callSiteLineNums` when we are using AsmJS, or else return the current
  // bytecode offset.
  //
  // This method MUST be called from opcodes that AsmJS will emit a call site
  // line number for, or else the arrays will get out of sync. Other opcodes
  // must use `readBytecodeOffset` below.
  uint32_t readCallSiteLineOrBytecode() {
    if (!func_.callSiteLineNums.empty()) {
      return func_.callSiteLineNums[lastReadCallSite_++];
    }
    return iter_.lastOpcodeOffset();
  }

  // Return the current bytecode offset.
  uint32_t readBytecodeOffset() { return iter_.lastOpcodeOffset(); }

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
  auto* ins = MWasmBuiltinTruncateToInt32::New(alloc(), op, instancePointer_,
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
      if (!f.emitBodyDelegateThrowPad(control)) {
        return false;
      }
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
    case LabelKind::Try:
    case LabelKind::Catch:
    case LabelKind::CatchAll:
      if (!f.finishTryCatch(kind, control, &postJoinDefs)) {
        return false;
      }
      f.iter().popEnd();
      break;
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

static bool EmitTry(FunctionCompiler& f) {
  ResultType params;
  if (!f.iter().readTry(&params)) {
    return false;
  }

  MBasicBlock* curBlock = nullptr;
  if (!f.startTry(&curBlock)) {
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

  return f.switchToCatch(f.iter().controlItem(), kind, tagIndex);
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

  return f.switchToCatch(f.iter().controlItem(), kind, CatchAllIndex);
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

  return f.emitRethrow(relativeDepth);
}

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
  uint32_t bytecodeOffset = f.readBytecodeOffset();

  uint32_t id;
  MDefinition* value;
  if (!f.iter().readSetGlobal(&id, &value)) {
    return false;
  }

  const GlobalDesc& global = f.moduleEnv().globals[id];
  MOZ_ASSERT(global.isMutable());
  return f.storeGlobalVar(bytecodeOffset, global.offset(), global.isIndirect(),
                          value);
}

static bool EmitTeeGlobal(FunctionCompiler& f) {
  uint32_t bytecodeOffset = f.readBytecodeOffset();

  uint32_t id;
  MDefinition* value;
  if (!f.iter().readTeeGlobal(&id, &value)) {
    return false;
  }

  const GlobalDesc& global = f.moduleEnv().globals[id];
  MOZ_ASSERT(global.isMutable());

  return f.storeGlobalVar(bytecodeOffset, global.offset(), global.isIndirect(),
                          value);
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
    f.iter().setResult(f.truncateWithInstance(input, flags));
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
  uint32_t bytecodeOffset = f.readBytecodeOffset();

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
  if (!f.builtinInstanceMethodCall(callee, bytecodeOffset, args, &ret)) {
    return false;
  }

  f.iter().setResult(ret);
  return true;
}

static bool EmitMemorySize(FunctionCompiler& f) {
  uint32_t bytecodeOffset = f.readBytecodeOffset();

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
  if (!f.builtinInstanceMethodCall(callee, bytecodeOffset, args, &ret)) {
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

  uint32_t bytecodeOffset = f.readBytecodeOffset();

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
  if (!f.builtinInstanceMethodCall(callee, bytecodeOffset, args, &ret)) {
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
  uint32_t bytecodeOffset = f.readBytecodeOffset();

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
  if (!f.builtinInstanceMethodCall(callee, bytecodeOffset, args, &ret)) {
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
  uint32_t bytecodeOffset = f.readBytecodeOffset();

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

  return f.builtinInstanceMethodCall(callee, bytecodeOffset, args);
}

static bool EmitMemCopyInline(FunctionCompiler& f, MDefinition* dst,
                              MDefinition* src, uint32_t length) {
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

  if (len->isConstant()) {
    uint64_t length = f.isMem32() ? len->toConstant()->toInt32()
                                  : len->toConstant()->toInt64();
    static_assert(MaxInlineMemoryCopyLength <= UINT32_MAX);
    if (length != 0 && length <= MaxInlineMemoryCopyLength) {
      return EmitMemCopyInline(f, dst, src, uint32_t(length));
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

  uint32_t bytecodeOffset = f.readBytecodeOffset();

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

  return f.builtinInstanceMethodCall(callee, bytecodeOffset, args);
}

static bool EmitDataOrElemDrop(FunctionCompiler& f, bool isData) {
  uint32_t segIndexVal = 0;
  if (!f.iter().readDataOrElemDrop(isData, &segIndexVal)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  uint32_t bytecodeOffset = f.readBytecodeOffset();

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

  return f.builtinInstanceMethodCall(callee, bytecodeOffset, args);
}

static bool EmitMemFillCall(FunctionCompiler& f, MDefinition* start,
                            MDefinition* val, MDefinition* len) {
  uint32_t bytecodeOffset = f.readBytecodeOffset();

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

  return f.builtinInstanceMethodCall(callee, bytecodeOffset, args);
}

static bool EmitMemFillInline(FunctionCompiler& f, MDefinition* start,
                              MDefinition* val, uint32_t length) {
  MOZ_ASSERT(length != 0 && length <= MaxInlineMemoryFillLength);
  uint32_t value = val->toConstant()->toInt32();

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

  if (len->isConstant() && val->isConstant()) {
    uint64_t length = f.isMem32() ? len->toConstant()->toInt32()
                                  : len->toConstant()->toInt64();
    static_assert(MaxInlineMemoryFillLength <= UINT32_MAX);
    if (length != 0 && length <= MaxInlineMemoryFillLength) {
      return EmitMemFillInline(f, start, val, uint32_t(length));
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

  uint32_t bytecodeOffset = f.readBytecodeOffset();

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

  return f.builtinInstanceMethodCall(callee, bytecodeOffset, args);
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

  uint32_t bytecodeOffset = f.readBytecodeOffset();

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

  return f.builtinInstanceMethodCall(callee, bytecodeOffset, args);
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

  const TableDesc& table = f.moduleEnv().tables[tableIndex];
  if (table.elemType.tableRepr() == TableRepr::Ref) {
    MDefinition* ret = f.tableGetAnyRef(table, index);
    if (!ret) {
      return false;
    }
    f.iter().setResult(ret);
    return true;
  }

  uint32_t bytecodeOffset = f.readBytecodeOffset();

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
  if (!f.builtinInstanceMethodCall(callee, bytecodeOffset, args, &ret)) {
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

  uint32_t bytecodeOffset = f.readBytecodeOffset();

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
  if (!f.builtinInstanceMethodCall(callee, bytecodeOffset, args, &ret)) {
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

  uint32_t bytecodeOffset = f.readBytecodeOffset();

  const TableDesc& table = f.moduleEnv().tables[tableIndex];
  if (table.elemType.tableRepr() == TableRepr::Ref) {
    return f.tableSetAnyRef(table, index, value, bytecodeOffset);
  }

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

  return f.builtinInstanceMethodCall(callee, bytecodeOffset, args);
}

static bool EmitTableSize(FunctionCompiler& f) {
  uint32_t tableIndex;
  if (!f.iter().readTableSize(&tableIndex)) {
    return false;
  }

  if (f.inDeadCode()) {
    return true;
  }

  const TableDesc& table = f.moduleEnv().tables[tableIndex];

  MDefinition* length = f.loadTableLength(table);
  if (!length) {
    return false;
  }

  f.iter().setResult(length);
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

  uint32_t bytecodeOffset = f.readBytecodeOffset();

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
  if (!f.builtinInstanceMethodCall(callee, bytecodeOffset, args, &ret)) {
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

static bool EmitIntrinsic(FunctionCompiler& f) {
  const Intrinsic* intrinsic;

  DefVector params;
  if (!f.iter().readIntrinsic(&intrinsic, &params)) {
    return false;
  }

  uint32_t bytecodeOffset = f.readBytecodeOffset();
  const SymbolicAddressSignature& callee = intrinsic->signature;

  CallCompileState args;
  if (!f.passInstance(callee.argTypes[0], &args)) {
    return false;
  }

  if (!f.passArgs(params, intrinsic->params, &args)) {
    return false;
  }

  MDefinition* memoryBase = f.memoryBase();
  if (!f.passArg(memoryBase, MIRType::Pointer, &args)) {
    return false;
  }

  f.finishCall(&args);

  return f.builtinInstanceMethodCall(callee, bytecodeOffset, args);
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

        // Gc operations
#ifdef ENABLE_WASM_GC
      case uint16_t(Op::GcPrefix): {
        return f.iter().unrecognizedOpcode(&op);
      }
#endif

      // SIMD operations
#ifdef ENABLE_WASM_SIMD
      case uint16_t(Op::SimdPrefix): {
        if (!f.moduleEnv().simdAvailable()) {
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
          case uint32_t(SimdOp::I8x16RelaxedLaneSelect):
          case uint32_t(SimdOp::I16x8RelaxedLaneSelect):
          case uint32_t(SimdOp::I32x4RelaxedLaneSelect):
          case uint32_t(SimdOp::I64x2RelaxedLaneSelect):
          case uint32_t(SimdOp::I32x4DotI8x16I7x16AddS): {
            if (!f.moduleEnv().v128RelaxedEnabled()) {
              return f.iter().unrecognizedOpcode(&op);
            }
            CHECK(EmitTernarySimd128(f, SimdOp(op.b1)));
          }
          case uint32_t(SimdOp::F32x4RelaxedMin):
          case uint32_t(SimdOp::F32x4RelaxedMax):
          case uint32_t(SimdOp::F64x2RelaxedMin):
          case uint32_t(SimdOp::F64x2RelaxedMax):
          case uint32_t(SimdOp::I16x8RelaxedQ15MulrS): {
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
          case uint32_t(SimdOp::I8x16RelaxedSwizzle):
          case uint32_t(SimdOp::I16x8DotI8x16I7x16S): {
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
        if (op.b1 == uint32_t(MozOp::Intrinsic)) {
          if (!f.moduleEnv().intrinsicsEnabled()) {
            return f.iter().unrecognizedOpcode(&op);
          }
          CHECK(EmitIntrinsic(f));
        }

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
  RegisterOffsets trapExitLayout;
  size_t trapExitLayoutNumWords;
  GenerateTrapExitRegisterOffsets(&trapExitLayout, &trapExitLayoutNumWords);

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
      FunctionCompiler f(moduleEnv, d, func, locals, mir, masm.tryNotes());
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
    defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_LOONG64)
  return true;
#else
  return false;
#endif
}

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

#include "wasm/WasmOpIter.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

#ifdef ENABLE_WASM_GC
#  ifndef ENABLE_WASM_REFTYPES
#    error "GC types require the reftypes feature"
#  endif
#endif

#ifdef DEBUG

#  ifdef ENABLE_WASM_REFTYPES
#    define WASM_REF_OP(code) return code
#  else
#    define WASM_REF_OP(code) break
#  endif
#  ifdef ENABLE_WASM_GC
#    define WASM_GC_OP(code) return code
#  else
#    define WASM_GC_OP(code) break
#  endif
#  ifdef ENABLE_WASM_BULKMEM_OPS
#    define WASM_BULK_OP(code) return code
#  else
#    define WASM_BULK_OP(code) break
#  endif

OpKind wasm::Classify(OpBytes op) {
  switch (Op(op.b0)) {
    case Op::Block:
      return OpKind::Block;
    case Op::Loop:
      return OpKind::Loop;
    case Op::Unreachable:
      return OpKind::Unreachable;
    case Op::Drop:
      return OpKind::Drop;
    case Op::I32Const:
      return OpKind::I32;
    case Op::I64Const:
      return OpKind::I64;
    case Op::F32Const:
      return OpKind::F32;
    case Op::F64Const:
      return OpKind::F64;
    case Op::Br:
      return OpKind::Br;
    case Op::BrIf:
      return OpKind::BrIf;
    case Op::BrTable:
      return OpKind::BrTable;
    case Op::Nop:
      return OpKind::Nop;
    case Op::I32Clz:
    case Op::I32Ctz:
    case Op::I32Popcnt:
    case Op::I64Clz:
    case Op::I64Ctz:
    case Op::I64Popcnt:
    case Op::F32Abs:
    case Op::F32Neg:
    case Op::F32Ceil:
    case Op::F32Floor:
    case Op::F32Trunc:
    case Op::F32Nearest:
    case Op::F32Sqrt:
    case Op::F64Abs:
    case Op::F64Neg:
    case Op::F64Ceil:
    case Op::F64Floor:
    case Op::F64Trunc:
    case Op::F64Nearest:
    case Op::F64Sqrt:
      return OpKind::Unary;
    case Op::I32Add:
    case Op::I32Sub:
    case Op::I32Mul:
    case Op::I32DivS:
    case Op::I32DivU:
    case Op::I32RemS:
    case Op::I32RemU:
    case Op::I32And:
    case Op::I32Or:
    case Op::I32Xor:
    case Op::I32Shl:
    case Op::I32ShrS:
    case Op::I32ShrU:
    case Op::I32Rotl:
    case Op::I32Rotr:
    case Op::I64Add:
    case Op::I64Sub:
    case Op::I64Mul:
    case Op::I64DivS:
    case Op::I64DivU:
    case Op::I64RemS:
    case Op::I64RemU:
    case Op::I64And:
    case Op::I64Or:
    case Op::I64Xor:
    case Op::I64Shl:
    case Op::I64ShrS:
    case Op::I64ShrU:
    case Op::I64Rotl:
    case Op::I64Rotr:
    case Op::F32Add:
    case Op::F32Sub:
    case Op::F32Mul:
    case Op::F32Div:
    case Op::F32Min:
    case Op::F32Max:
    case Op::F32CopySign:
    case Op::F64Add:
    case Op::F64Sub:
    case Op::F64Mul:
    case Op::F64Div:
    case Op::F64Min:
    case Op::F64Max:
    case Op::F64CopySign:
      return OpKind::Binary;
    case Op::I32Eq:
    case Op::I32Ne:
    case Op::I32LtS:
    case Op::I32LtU:
    case Op::I32LeS:
    case Op::I32LeU:
    case Op::I32GtS:
    case Op::I32GtU:
    case Op::I32GeS:
    case Op::I32GeU:
    case Op::I64Eq:
    case Op::I64Ne:
    case Op::I64LtS:
    case Op::I64LtU:
    case Op::I64LeS:
    case Op::I64LeU:
    case Op::I64GtS:
    case Op::I64GtU:
    case Op::I64GeS:
    case Op::I64GeU:
    case Op::F32Eq:
    case Op::F32Ne:
    case Op::F32Lt:
    case Op::F32Le:
    case Op::F32Gt:
    case Op::F32Ge:
    case Op::F64Eq:
    case Op::F64Ne:
    case Op::F64Lt:
    case Op::F64Le:
    case Op::F64Gt:
    case Op::F64Ge:
      return OpKind::Comparison;
    case Op::I32Eqz:
    case Op::I32WrapI64:
    case Op::I32TruncSF32:
    case Op::I32TruncUF32:
    case Op::I32ReinterpretF32:
    case Op::I32TruncSF64:
    case Op::I32TruncUF64:
    case Op::I64ExtendSI32:
    case Op::I64ExtendUI32:
    case Op::I64TruncSF32:
    case Op::I64TruncUF32:
    case Op::I64TruncSF64:
    case Op::I64TruncUF64:
    case Op::I64ReinterpretF64:
    case Op::I64Eqz:
    case Op::F32ConvertSI32:
    case Op::F32ConvertUI32:
    case Op::F32ReinterpretI32:
    case Op::F32ConvertSI64:
    case Op::F32ConvertUI64:
    case Op::F32DemoteF64:
    case Op::F64ConvertSI32:
    case Op::F64ConvertUI32:
    case Op::F64ConvertSI64:
    case Op::F64ConvertUI64:
    case Op::F64ReinterpretI64:
    case Op::F64PromoteF32:
    case Op::I32Extend8S:
    case Op::I32Extend16S:
    case Op::I64Extend8S:
    case Op::I64Extend16S:
    case Op::I64Extend32S:
      return OpKind::Conversion;
    case Op::I32Load8S:
    case Op::I32Load8U:
    case Op::I32Load16S:
    case Op::I32Load16U:
    case Op::I64Load8S:
    case Op::I64Load8U:
    case Op::I64Load16S:
    case Op::I64Load16U:
    case Op::I64Load32S:
    case Op::I64Load32U:
    case Op::I32Load:
    case Op::I64Load:
    case Op::F32Load:
    case Op::F64Load:
      return OpKind::Load;
    case Op::I32Store8:
    case Op::I32Store16:
    case Op::I64Store8:
    case Op::I64Store16:
    case Op::I64Store32:
    case Op::I32Store:
    case Op::I64Store:
    case Op::F32Store:
    case Op::F64Store:
      return OpKind::Store;
    case Op::Select:
      return OpKind::Select;
    case Op::GetLocal:
      return OpKind::GetLocal;
    case Op::SetLocal:
      return OpKind::SetLocal;
    case Op::TeeLocal:
      return OpKind::TeeLocal;
    case Op::GetGlobal:
      return OpKind::GetGlobal;
    case Op::SetGlobal:
      return OpKind::SetGlobal;
    case Op::TableGet:
      WASM_REF_OP(OpKind::TableGet);
    case Op::TableSet:
      WASM_REF_OP(OpKind::TableSet);
    case Op::Call:
      return OpKind::Call;
    case Op::CallIndirect:
      return OpKind::CallIndirect;
    case Op::Return:
    case Op::Limit:
      // Accept Limit, for use in decoding the end of a function after the body.
      return OpKind::Return;
    case Op::If:
      return OpKind::If;
    case Op::Else:
      return OpKind::Else;
    case Op::End:
      return OpKind::End;
    case Op::MemorySize:
      return OpKind::MemorySize;
    case Op::MemoryGrow:
      return OpKind::MemoryGrow;
    case Op::RefNull:
      WASM_REF_OP(OpKind::RefNull);
    case Op::RefIsNull:
      WASM_REF_OP(OpKind::Conversion);
    case Op::RefFunc:
      WASM_REF_OP(OpKind::RefFunc);
    case Op::RefEq:
      WASM_GC_OP(OpKind::Comparison);
    case Op::MiscPrefix: {
      switch (MiscOp(op.b1)) {
        case MiscOp::Limit:
          // Reject Limit for MiscPrefix encoding
          break;
        case MiscOp::I32TruncSSatF32:
        case MiscOp::I32TruncUSatF32:
        case MiscOp::I32TruncSSatF64:
        case MiscOp::I32TruncUSatF64:
        case MiscOp::I64TruncSSatF32:
        case MiscOp::I64TruncUSatF32:
        case MiscOp::I64TruncSSatF64:
        case MiscOp::I64TruncUSatF64:
          return OpKind::Conversion;
        case MiscOp::MemCopy:
        case MiscOp::TableCopy:
          WASM_BULK_OP(OpKind::MemOrTableCopy);
        case MiscOp::DataDrop:
        case MiscOp::ElemDrop:
          WASM_BULK_OP(OpKind::DataOrElemDrop);
        case MiscOp::MemFill:
          WASM_BULK_OP(OpKind::MemFill);
        case MiscOp::MemInit:
        case MiscOp::TableInit:
          WASM_BULK_OP(OpKind::MemOrTableInit);
        case MiscOp::TableFill:
          WASM_REF_OP(OpKind::TableFill);
        case MiscOp::TableGrow:
          WASM_REF_OP(OpKind::TableGrow);
        case MiscOp::TableSize:
          WASM_REF_OP(OpKind::TableSize);
        case MiscOp::StructNew:
          WASM_GC_OP(OpKind::StructNew);
        case MiscOp::StructGet:
          WASM_GC_OP(OpKind::StructGet);
        case MiscOp::StructSet:
          WASM_GC_OP(OpKind::StructSet);
        case MiscOp::StructNarrow:
          WASM_GC_OP(OpKind::StructNarrow);
      }
      break;
    }
    case Op::ThreadPrefix: {
      switch (ThreadOp(op.b1)) {
        case ThreadOp::Limit:
          // Reject Limit for ThreadPrefix encoding
          break;
        case ThreadOp::Wake:
          return OpKind::Wake;
        case ThreadOp::I32Wait:
        case ThreadOp::I64Wait:
          return OpKind::Wait;
        case ThreadOp::I32AtomicLoad:
        case ThreadOp::I64AtomicLoad:
        case ThreadOp::I32AtomicLoad8U:
        case ThreadOp::I32AtomicLoad16U:
        case ThreadOp::I64AtomicLoad8U:
        case ThreadOp::I64AtomicLoad16U:
        case ThreadOp::I64AtomicLoad32U:
          return OpKind::AtomicLoad;
        case ThreadOp::I32AtomicStore:
        case ThreadOp::I64AtomicStore:
        case ThreadOp::I32AtomicStore8U:
        case ThreadOp::I32AtomicStore16U:
        case ThreadOp::I64AtomicStore8U:
        case ThreadOp::I64AtomicStore16U:
        case ThreadOp::I64AtomicStore32U:
          return OpKind::AtomicStore;
        case ThreadOp::I32AtomicAdd:
        case ThreadOp::I64AtomicAdd:
        case ThreadOp::I32AtomicAdd8U:
        case ThreadOp::I32AtomicAdd16U:
        case ThreadOp::I64AtomicAdd8U:
        case ThreadOp::I64AtomicAdd16U:
        case ThreadOp::I64AtomicAdd32U:
        case ThreadOp::I32AtomicSub:
        case ThreadOp::I64AtomicSub:
        case ThreadOp::I32AtomicSub8U:
        case ThreadOp::I32AtomicSub16U:
        case ThreadOp::I64AtomicSub8U:
        case ThreadOp::I64AtomicSub16U:
        case ThreadOp::I64AtomicSub32U:
        case ThreadOp::I32AtomicAnd:
        case ThreadOp::I64AtomicAnd:
        case ThreadOp::I32AtomicAnd8U:
        case ThreadOp::I32AtomicAnd16U:
        case ThreadOp::I64AtomicAnd8U:
        case ThreadOp::I64AtomicAnd16U:
        case ThreadOp::I64AtomicAnd32U:
        case ThreadOp::I32AtomicOr:
        case ThreadOp::I64AtomicOr:
        case ThreadOp::I32AtomicOr8U:
        case ThreadOp::I32AtomicOr16U:
        case ThreadOp::I64AtomicOr8U:
        case ThreadOp::I64AtomicOr16U:
        case ThreadOp::I64AtomicOr32U:
        case ThreadOp::I32AtomicXor:
        case ThreadOp::I64AtomicXor:
        case ThreadOp::I32AtomicXor8U:
        case ThreadOp::I32AtomicXor16U:
        case ThreadOp::I64AtomicXor8U:
        case ThreadOp::I64AtomicXor16U:
        case ThreadOp::I64AtomicXor32U:
        case ThreadOp::I32AtomicXchg:
        case ThreadOp::I64AtomicXchg:
        case ThreadOp::I32AtomicXchg8U:
        case ThreadOp::I32AtomicXchg16U:
        case ThreadOp::I64AtomicXchg8U:
        case ThreadOp::I64AtomicXchg16U:
        case ThreadOp::I64AtomicXchg32U:
          return OpKind::AtomicBinOp;
        case ThreadOp::I32AtomicCmpXchg:
        case ThreadOp::I64AtomicCmpXchg:
        case ThreadOp::I32AtomicCmpXchg8U:
        case ThreadOp::I32AtomicCmpXchg16U:
        case ThreadOp::I64AtomicCmpXchg8U:
        case ThreadOp::I64AtomicCmpXchg16U:
        case ThreadOp::I64AtomicCmpXchg32U:
          return OpKind::AtomicCompareExchange;
        default:
          break;
      }
      break;
    }
    case Op::MozPrefix: {
      switch (MozOp(op.b1)) {
        case MozOp::Limit:
          // Reject Limit for the MozPrefix encoding
          break;
        case MozOp::TeeGlobal:
          return OpKind::TeeGlobal;
        case MozOp::I32BitNot:
        case MozOp::I32Abs:
        case MozOp::I32Neg:
          return OpKind::Unary;
        case MozOp::I32Min:
        case MozOp::I32Max:
        case MozOp::F64Mod:
        case MozOp::F64Pow:
        case MozOp::F64Atan2:
          return OpKind::Binary;
        case MozOp::F64Sin:
        case MozOp::F64Cos:
        case MozOp::F64Tan:
        case MozOp::F64Asin:
        case MozOp::F64Acos:
        case MozOp::F64Atan:
        case MozOp::F64Exp:
        case MozOp::F64Log:
          return OpKind::Unary;
        case MozOp::I32TeeStore8:
        case MozOp::I32TeeStore16:
        case MozOp::I64TeeStore8:
        case MozOp::I64TeeStore16:
        case MozOp::I64TeeStore32:
        case MozOp::I32TeeStore:
        case MozOp::I64TeeStore:
        case MozOp::F32TeeStore:
        case MozOp::F64TeeStore:
        case MozOp::F32TeeStoreF64:
        case MozOp::F64TeeStoreF32:
          return OpKind::TeeStore;
        case MozOp::OldCallDirect:
          return OpKind::OldCallDirect;
        case MozOp::OldCallIndirect:
          return OpKind::OldCallIndirect;
      }
      break;
    }
  }
  MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unimplemented opcode");
}

#  undef WASM_GC_OP
#  undef WASM_BULK_OP
#  undef WASM_REF_OP

#endif

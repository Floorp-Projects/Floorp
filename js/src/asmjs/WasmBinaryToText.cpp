/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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

#include "asmjs/WasmBinaryToText.h"

#include "jsnum.h"
#include "jsprf.h"

#include "asmjs/WasmAST.h"
#include "asmjs/WasmBinaryToAST.h"
#include "asmjs/WasmTextUtils.h"
#include "asmjs/WasmTypes.h"
#include "vm/ArrayBufferObject.h"
#include "vm/StringBuffer.h"

using namespace js;
using namespace js::wasm;

using mozilla::IsInfinite;
using mozilla::IsNaN;
using mozilla::IsNegativeZero;

struct WasmRenderContext
{
    JSContext* cx;
    AstModule* module;
    StringBuffer& buffer;
    uint32_t indent;

    uint32_t currentFuncIndex;

    WasmRenderContext(JSContext* cx, AstModule* module, StringBuffer& buffer)
      : cx(cx), module(module), buffer(buffer), indent(0), currentFuncIndex(0)
    {}
};

/*****************************************************************************/
// utilities

static bool
RenderIndent(WasmRenderContext& c)
{
    for (uint32_t i = 0; i < c.indent; i++) {
        if (!c.buffer.append("  "))
            return false;
    }
    return true;
}

static bool
RenderInt32(WasmRenderContext& c, int32_t num)
{
    return NumberValueToStringBuffer(c.cx, Int32Value(num), c.buffer);
}

static bool
RenderInt64(WasmRenderContext& c, int64_t num)
{
    if (num < 0 && !c.buffer.append("-"))
        return false;
    if (!num)
        return c.buffer.append("0");
    return RenderInBase<10>(c.buffer, mozilla::Abs(num));
}

static bool
RenderDouble(WasmRenderContext& c, RawF64 num)
{
    double d = num.fp();
    if (IsNaN(d))
        return RenderNaN(c.buffer, num);
    if (IsNegativeZero(d))
        return c.buffer.append("-0");
    if (IsInfinite(d)) {
        if (d > 0)
            return c.buffer.append("infinity");
        return c.buffer.append("-infinity");
    }
    return NumberValueToStringBuffer(c.cx, DoubleValue(d), c.buffer);
}

static bool
RenderFloat32(WasmRenderContext& c, RawF32 num)
{
    float f = num.fp();
    if (IsNaN(f))
        return RenderNaN(c.buffer, num);
    return RenderDouble(c, RawF64(double(f)));
}

static bool
RenderEscapedString(WasmRenderContext& c, const AstName& s)
{
    size_t length = s.length();
    const char16_t* p = s.begin();
    for (size_t i = 0; i < length; i++) {
        char16_t byte = p[i];
        switch (byte) {
          case '\n':
            if (!c.buffer.append("\\n"))
                return false;
            break;
          case '\r':
            if (!c.buffer.append("\\0d"))
                return false;
            break;
          case '\t':
            if (!c.buffer.append("\\t"))
                return false;
            break;
          case '\f':
            if (!c.buffer.append("\\0c"))
                return false;
            break;
          case '\b':
            if (!c.buffer.append("\\08"))
                return false;
            break;
          case '\\':
            if (!c.buffer.append("\\\\"))
                return false;
            break;
          case '"' :
            if (!c.buffer.append("\\\""))
                return false;
            break;
          case '\'':
            if (!c.buffer.append("\\'"))
                return false;
            break;
          default:
            if (byte >= 32 && byte < 127) {
                if (!c.buffer.append((char)byte))
                    return false;
            } else {
                char digit1 = byte / 16, digit2 = byte % 16;
                if (!c.buffer.append("\\"))
                    return false;
                if (!c.buffer.append((char)(digit1 < 10 ? digit1 + '0' : digit1 + 'a' - 10)))
                    return false;
                if (!c.buffer.append((char)(digit2 < 10 ? digit2 + '0' : digit2 + 'a' - 10)))
                    return false;
            }
            break;
        }
    }
    return true;
}

static bool
RenderExprType(WasmRenderContext& c, ExprType type)
{
    switch (type) {
      case ExprType::Void: return true; // ignoring void
      case ExprType::I32: return c.buffer.append("i32");
      case ExprType::I64: return c.buffer.append("i64");
      case ExprType::F32: return c.buffer.append("f32");
      case ExprType::F64: return c.buffer.append("f64");
      default:;
    }

    MOZ_CRASH("bad type");
}

static bool
RenderValType(WasmRenderContext& c, ValType type)
{
    return RenderExprType(c, ToExprType(type));
}

static bool
RenderName(WasmRenderContext& c, const AstName& name)
{
    return c.buffer.append(name.begin(), name.end());
}

static bool
RenderRef(WasmRenderContext& c, const AstRef& ref)
{
    if (ref.name().empty())
        return RenderInt32(c, ref.index());

    return RenderName(c, ref.name());
}

static bool
RenderBlockNameAndSignature(WasmRenderContext& c, const AstName& name, ExprType type)
{
    if (!name.empty()) {
        if (!c.buffer.append(' '))
            return false;

        if (!RenderName(c, name))
            return false;
    }

    if (!IsVoid(type)) {
        if (!c.buffer.append(' '))
            return false;

        if (!RenderExprType(c, type))
            return false;
    }

    return true;
}

static bool
RenderExpr(WasmRenderContext& c, AstExpr& expr);

/*****************************************************************************/
// binary format parsing and rendering

static bool
RenderNop(WasmRenderContext& c)
{
    if (!RenderIndent(c))
        return false;
    return c.buffer.append("nop\n");
}

static bool
RenderDrop(WasmRenderContext& c, AstDrop& drop)
{
    if (!RenderExpr(c, drop.value()))
        return false;

    if (!RenderIndent(c))
        return false;
    if (!c.buffer.append("drop\n"))
        return false;
    return true;
}

static bool
RenderUnreachable(WasmRenderContext& c)
{
    if (!RenderIndent(c))
        return false;
    return c.buffer.append("unreachable\n");
}

static bool
RenderCallArgs(WasmRenderContext& c, const AstExprVector& args)
{
    for (uint32_t i = 0; i < args.length(); i++) {
        if (!RenderExpr(c, *args[i]))
            return false;
    }

    return true;
}

static bool
RenderCall(WasmRenderContext& c, AstCall& call)
{
    if (!RenderCallArgs(c, call.args()))
        return false;

    if (!RenderIndent(c))
        return false;

    if (call.expr() == Expr::Call) {
        if (!c.buffer.append("call "))
            return false;
    } else if (call.expr() == Expr::CallImport) {
        if (!c.buffer.append("call_import "))
            return false;
    } else {
        return false;
    }

    if (!RenderRef(c, call.func()))
        return false;

    if (!c.buffer.append('\n'))
        return false;

    return true;
}

static bool
RenderCallIndirect(WasmRenderContext& c, AstCallIndirect& call)
{
    if (!RenderCallArgs(c, call.args()))
        return false;

    if (!RenderExpr(c, *call.index()))
        return false;

    if (!RenderIndent(c))
        return false;

    if (!c.buffer.append("call_indirect "))
        return false;
    if (!RenderRef(c, call.sig()))
        return false;

    if (!c.buffer.append('\n'))
        return false;

    return true;
}

static bool
RenderConst(WasmRenderContext& c, AstConst& cst)
{
    if (!RenderIndent(c))
        return false;

    if (!RenderValType(c, cst.val().type()))
        return false;
    if (!c.buffer.append(".const "))
        return false;

    switch (ToExprType(cst.val().type())) {
      case ExprType::I32:
        if (!RenderInt32(c, (uint32_t)cst.val().i32()))
            return false;
        break;
      case ExprType::I64:
        if (!RenderInt64(c, (uint32_t)cst.val().i64()))
            return false;
        break;
      case ExprType::F32:
        if (!RenderFloat32(c, cst.val().f32()))
            return false;
        break;
      case ExprType::F64:
        if (!RenderDouble(c, cst.val().f64()))
            return false;
        break;
      default:
        return false;
    }

    if (!c.buffer.append('\n'))
        return false;
    return true;
}

static bool
RenderGetLocal(WasmRenderContext& c, AstGetLocal& gl)
{
    if (!RenderIndent(c))
        return false;

    if (!c.buffer.append("get_local "))
        return false;
    if (!RenderRef(c, gl.local()))
        return false;
    if (!c.buffer.append('\n'))
        return false;
    return true;
}

static bool
RenderSetLocal(WasmRenderContext& c, AstSetLocal& sl)
 {
    if (!RenderExpr(c, sl.value()))
        return false;

    if (!RenderIndent(c))
        return false;

    if (!c.buffer.append("set_local "))
        return false;
    if (!RenderRef(c, sl.local()))
        return false;

    if (!c.buffer.append('\n'))
        return false;
    return true;
}

static bool
RenderTeeLocal(WasmRenderContext& c, AstTeeLocal& tl)
{
    if (!RenderExpr(c, tl.value()))
        return false;

    if (!RenderIndent(c))
        return false;

    if (!c.buffer.append("tee_local "))
        return false;
    if (!RenderRef(c, tl.local()))
        return false;

    if (!c.buffer.append('\n'))
        return false;
    return true;
}

static bool
RenderExprList(WasmRenderContext& c, const AstExprVector& exprs)
{
    for (uint32_t i = 0; i < exprs.length(); i++) {
        if (!RenderExpr(c, *exprs[i]))
            return false;
    }
    return true;
}

static bool
RenderBlock(WasmRenderContext& c, AstBlock& block)
{
    if (!RenderIndent(c))
        return false;

    if (block.expr() == Expr::Block) {
        if (!c.buffer.append("block"))
            return false;
    } else if (block.expr() == Expr::Loop) {
        if (!c.buffer.append("loop"))
            return false;
    } else
        return false;

    if (!RenderBlockNameAndSignature(c, block.name(), block.type()))
        return false;

    if (!c.buffer.append('\n'))
        return false;

    c.indent++;
    if (!RenderExprList(c, block.exprs()))
        return false;
    c.indent--;

    if (!RenderIndent(c))
        return false;

    return c.buffer.append("end\n");
}

static bool
RenderFirst(WasmRenderContext& c, AstFirst& first)
{
    if (!RenderExprList(c, first.exprs()))
        return false;

    return true;
}

static bool
RenderNullaryOperator(WasmRenderContext& c, AstNullaryOperator& op)
{
    if (!RenderIndent(c))
      return false;

    const char* opStr;
    switch (op.expr()) {
      case Expr::CurrentMemory:     opStr = "current_memory"; break;
      default: return false;
    }

    if (!c.buffer.append(opStr, strlen(opStr)))
        return false;

    return c.buffer.append('\n');
}

static bool
RenderUnaryOperator(WasmRenderContext& c, AstUnaryOperator& op)
{
    if (!RenderExpr(c, *op.op()))
        return false;

    if (!RenderIndent(c))
        return false;

    const char* opStr;
    switch (op.expr()) {
      case Expr::I32Eqz:     opStr = "i32.eqz"; break;
      case Expr::I32Clz:     opStr = "i32.clz"; break;
      case Expr::I32Ctz:     opStr = "i32.ctz"; break;
      case Expr::I32Popcnt:  opStr = "i32.popcnt"; break;
      case Expr::I64Clz:     opStr = "i64.clz"; break;
      case Expr::I64Ctz:     opStr = "i64.ctz"; break;
      case Expr::I64Popcnt:  opStr = "i64.popcnt"; break;
      case Expr::F32Abs:     opStr = "f32.abs"; break;
      case Expr::F32Neg:     opStr = "f32.neg"; break;
      case Expr::F32Ceil:    opStr = "f32.ceil"; break;
      case Expr::F32Floor:   opStr = "f32.floor"; break;
      case Expr::F32Sqrt:    opStr = "f32.sqrt"; break;
      case Expr::F32Trunc:   opStr = "f32.trunc"; break;
      case Expr::F32Nearest: opStr = "f32.nearest"; break;
      case Expr::F64Abs:     opStr = "f64.abs"; break;
      case Expr::F64Neg:     opStr = "f64.neg"; break;
      case Expr::F64Ceil:    opStr = "f64.ceil"; break;
      case Expr::F64Floor:   opStr = "f64.floor"; break;
      case Expr::F64Sqrt:    opStr = "f64.sqrt"; break;
      case Expr::GrowMemory: opStr = "grow_memory"; break;
      default: return false;
    }
    if (!c.buffer.append(opStr, strlen(opStr)))
        return false;

    return c.buffer.append('\n');
}

static bool
RenderBinaryOperator(WasmRenderContext& c, AstBinaryOperator& op)
{
    if (!RenderExpr(c, *op.lhs()))
        return false;
    if (!RenderExpr(c, *op.rhs()))
        return false;

    if (!RenderIndent(c))
        return false;

    const char* opStr;
    switch (op.expr()) {
      case Expr::I32Add:      opStr = "i32.add"; break;
      case Expr::I32Sub:      opStr = "i32.sub"; break;
      case Expr::I32Mul:      opStr = "i32.mul"; break;
      case Expr::I32DivS:     opStr = "i32.div_s"; break;
      case Expr::I32DivU:     opStr = "i32.div_u"; break;
      case Expr::I32RemS:     opStr = "i32.rem_s"; break;
      case Expr::I32RemU:     opStr = "i32.rem_u"; break;
      case Expr::I32And:      opStr = "i32.and"; break;
      case Expr::I32Or:       opStr = "i32.or"; break;
      case Expr::I32Xor:      opStr = "i32.xor"; break;
      case Expr::I32Shl:      opStr = "i32.shl"; break;
      case Expr::I32ShrS:     opStr = "i32.shr_s"; break;
      case Expr::I32ShrU:     opStr = "i32.shr_u"; break;
      case Expr::I64Add:      opStr = "i64.add"; break;
      case Expr::I64Sub:      opStr = "i64.sub"; break;
      case Expr::I64Mul:      opStr = "i64.mul"; break;
      case Expr::I64DivS:     opStr = "i64.div_s"; break;
      case Expr::I64DivU:     opStr = "i64.div_u"; break;
      case Expr::I64RemS:     opStr = "i64.rem_s"; break;
      case Expr::I64RemU:     opStr = "i64.rem_u"; break;
      case Expr::I64And:      opStr = "i64.and"; break;
      case Expr::I64Or:       opStr = "i64.or"; break;
      case Expr::I64Xor:      opStr = "i64.xor"; break;
      case Expr::I64Shl:      opStr = "i64.shl"; break;
      case Expr::I64ShrS:     opStr = "i64.shr_s"; break;
      case Expr::I64ShrU:     opStr = "i64.shr_u"; break;
      case Expr::F32Add:      opStr = "f32.add"; break;
      case Expr::F32Sub:      opStr = "f32.sub"; break;
      case Expr::F32Mul:      opStr = "f32.mul"; break;
      case Expr::F32Div:      opStr = "f32.div"; break;
      case Expr::F32Min:      opStr = "f32.min"; break;
      case Expr::F32Max:      opStr = "f32.max"; break;
      case Expr::F32CopySign: opStr = "f32.copysign"; break;
      case Expr::F64Add:      opStr = "f64.add"; break;
      case Expr::F64Sub:      opStr = "f64.sub"; break;
      case Expr::F64Mul:      opStr = "f64.mul"; break;
      case Expr::F64Div:      opStr = "f64.div"; break;
      case Expr::F64Min:      opStr = "f64.min"; break;
      case Expr::F64Max:      opStr = "f64.max"; break;
      default: return false;
    }
    if (!c.buffer.append(opStr, strlen(opStr)))
        return false;

    if (!c.buffer.append('\n'))
        return false;

    return true;
}

static bool
RenderTernaryOperator(WasmRenderContext& c, AstTernaryOperator& op)
{
    if (!RenderExpr(c, *op.op0()))
        return false;
    if (!RenderExpr(c, *op.op1()))
        return false;
    if (!RenderExpr(c, *op.op2()))
        return false;

    if (!RenderIndent(c))
        return false;

    const char* opStr;
    switch (op.expr()) {
      case Expr::Select: opStr = "select"; break;
      default: return false;
    }
    if (!c.buffer.append(opStr, strlen(opStr)))
        return false;

    if (!c.buffer.append('\n'))
        return false;

    return true;
}

static bool
RenderComparisonOperator(WasmRenderContext& c, AstComparisonOperator& op)
{
    if (!RenderExpr(c, *op.lhs()))
        return false;
    if (!RenderExpr(c, *op.rhs()))
        return false;

    if (!RenderIndent(c))
        return false;

    const char* opStr;
    switch (op.expr()) {
      case Expr::I32Eq:  opStr = "i32.eq"; break;
      case Expr::I32Ne:  opStr = "i32.ne"; break;
      case Expr::I32LtS: opStr = "i32.lt_s"; break;
      case Expr::I32LtU: opStr = "i32.lt_u"; break;
      case Expr::I32LeS: opStr = "i32.le_s"; break;
      case Expr::I32LeU: opStr = "i32.le_u"; break;
      case Expr::I32GtS: opStr = "i32.gt_s"; break;
      case Expr::I32GtU: opStr = "i32.gt_u"; break;
      case Expr::I32GeS: opStr = "i32.ge_s"; break;
      case Expr::I32GeU: opStr = "i32.ge_u"; break;
      case Expr::I64Eq:  opStr = "i64.eq"; break;
      case Expr::I64Ne:  opStr = "i64.ne"; break;
      case Expr::I64LtS: opStr = "i64.lt_s"; break;
      case Expr::I64LtU: opStr = "i64.lt_u"; break;
      case Expr::I64LeS: opStr = "i64.le_s"; break;
      case Expr::I64LeU: opStr = "i64.le_u"; break;
      case Expr::I64GtS: opStr = "i64.gt_s"; break;
      case Expr::I64GtU: opStr = "i64.gt_u"; break;
      case Expr::I64GeS: opStr = "i64.ge_s"; break;
      case Expr::I64GeU: opStr = "i64.ge_u"; break;
      case Expr::F32Eq:  opStr = "f32.eq"; break;
      case Expr::F32Ne:  opStr = "f32.ne"; break;
      case Expr::F32Lt:  opStr = "f32.lt"; break;
      case Expr::F32Le:  opStr = "f32.le"; break;
      case Expr::F32Gt:  opStr = "f32.gt"; break;
      case Expr::F32Ge:  opStr = "f32.ge"; break;
      case Expr::F64Eq:  opStr = "f64.eq"; break;
      case Expr::F64Ne:  opStr = "f64.ne"; break;
      case Expr::F64Lt:  opStr = "f64.lt"; break;
      case Expr::F64Le:  opStr = "f64.le"; break;
      case Expr::F64Gt:  opStr = "f64.gt"; break;
      case Expr::F64Ge:  opStr = "f64.ge"; break;
      default: return false;
    }
    if (!c.buffer.append(opStr, strlen(opStr)))
        return false;

    if (!c.buffer.append('\n'))
        return false;

    return true;
}

static bool
RenderConversionOperator(WasmRenderContext& c, AstConversionOperator& op)
{
    if (!RenderExpr(c, *op.op()))
        return false;

    if (!RenderIndent(c))
        return false;

    const char* opStr;
    switch (op.expr()) {
      case Expr::I32WrapI64:        opStr = "i32.wrap/i64"; break;
      case Expr::I32TruncSF32:      opStr = "i32.trunc_s/f32"; break;
      case Expr::I32TruncUF32:      opStr = "i32.trunc_u/f32"; break;
      case Expr::I32ReinterpretF32: opStr = "i32.reinterpret/f32"; break;
      case Expr::I32TruncSF64:      opStr = "i32.trunc_s/f64"; break;
      case Expr::I32TruncUF64:      opStr = "i32.trunc_u/f64"; break;
      case Expr::I64ExtendSI32:     opStr = "i64.extend_s/i32"; break;
      case Expr::I64ExtendUI32:     opStr = "i64.extend_u/i32"; break;
      case Expr::I64TruncSF32:      opStr = "i64.trunc_s/f32"; break;
      case Expr::I64TruncUF32:      opStr = "i64.trunc_u/f32"; break;
      case Expr::I64TruncSF64:      opStr = "i64.trunc_s/f64"; break;
      case Expr::I64TruncUF64:      opStr = "i64.trunc_u/f64"; break;
      case Expr::I64ReinterpretF64: opStr = "i64.reinterpret/f64"; break;
      case Expr::F32ConvertSI32:    opStr = "f32.convert_s/i32"; break;
      case Expr::F32ConvertUI32:    opStr = "f32.convert_u/i32"; break;
      case Expr::F32ReinterpretI32: opStr = "f32.reinterpret/i32"; break;
      case Expr::F32ConvertSI64:    opStr = "f32.convert_s/i64"; break;
      case Expr::F32ConvertUI64:    opStr = "f32.convert_u/i64"; break;
      case Expr::F32DemoteF64:      opStr = "f32.demote/f64"; break;
      case Expr::F64ConvertSI32:    opStr = "f64.convert_s/i32"; break;
      case Expr::F64ConvertUI32:    opStr = "f64.convert_u/i32"; break;
      case Expr::F64ConvertSI64:    opStr = "f64.convert_s/i64"; break;
      case Expr::F64ConvertUI64:    opStr = "f64.convert_u/i64"; break;
      case Expr::F64ReinterpretI64: opStr = "f64.reinterpret/i64"; break;
      case Expr::F64PromoteF32:     opStr = "f64.promote/f32"; break;
      case Expr::I32Eqz:            opStr = "i32.eqz"; break;
      case Expr::I64Eqz:            opStr = "i64.eqz"; break;
      default: return false;
    }
    if (!c.buffer.append(opStr, strlen(opStr)))
        return false;

    return c.buffer.append('\n');
}

static bool
RenderIf(WasmRenderContext& c, AstIf& if_)
{
    if (!RenderExpr(c, if_.cond()))
        return false;

    if (!RenderIndent(c))
        return false;

    if (!c.buffer.append("if"))
        return false;

    if (!RenderBlockNameAndSignature(c, if_.name(), if_.type()))
        return false;

    if (!if_.name().empty()) {
        if (!c.buffer.append(' '))
            return false;

        if (!RenderName(c, if_.name()))
            return false;
    }

    if (!c.buffer.append('\n'))
        return false;

    c.indent++;
    if (!RenderExprList(c, if_.thenExprs()))
        return false;
    c.indent--;

    if (if_.hasElse()) {
        if (!RenderIndent(c))
            return false;

        if (!c.buffer.append("else\n"))
            return false;

        c.indent++;
        if (!RenderExprList(c, if_.elseExprs()))
            return false;
        c.indent--;
    }

    if (!RenderIndent(c))
        return false;

    return c.buffer.append("end\n");
}

static bool
RenderLoadStoreBase(WasmRenderContext& c, const AstLoadStoreAddress& lsa)
{
    if (!RenderExpr(c, lsa.base()))
        return false;

    return true;
}

static bool
RenderLoadStoreAddress(WasmRenderContext& c, const AstLoadStoreAddress& lsa, uint32_t defaultAlignLog2)
{
    if (lsa.offset() != 0) {
      if (!c.buffer.append(" offset="))
          return false;
      if (!RenderInt32(c, lsa.offset()))
          return false;
    }

    uint32_t alignLog2 = lsa.flags();
    if (defaultAlignLog2 != alignLog2) {
      if (!c.buffer.append(" align="))
          return false;
      if (!RenderInt32(c, 1 << alignLog2))
          return false;
    }

    return true;
}

static bool
RenderLoad(WasmRenderContext& c, AstLoad& load)
{
    if (!RenderLoadStoreBase(c, load.address()))
        return false;

    if (!RenderIndent(c))
        return false;

    uint32_t defaultAlignLog2;
    switch (load.expr()) {
      case Expr::I32Load8S:
        if (!c.buffer.append("i32.load8_s"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I64Load8S:
        if (!c.buffer.append("i64.load8_s"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I32Load8U:
        if (!c.buffer.append("i32.load8_u"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I64Load8U:
        if (!c.buffer.append("i64.load8_u"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I32Load16S:
        if (!c.buffer.append("i32.load16_s"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Load16S:
        if (!c.buffer.append("i64.load16_s"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I32Load16U:
        if (!c.buffer.append("i32.load16_u"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Load16U:
        if (!c.buffer.append("i64.load16_u"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Load32S:
        if (!c.buffer.append("i64.load32_s"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I64Load32U:
        if (!c.buffer.append("i64.load32_u"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I32Load:
        if (!c.buffer.append("i32.load"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I64Load:
        if (!c.buffer.append("i64.load"))
            return false;
        defaultAlignLog2 = 3;
        break;
      case Expr::F32Load:
        if (!c.buffer.append("f32.load"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::F64Load:
        if (!c.buffer.append("f64.load"))
            return false;
        defaultAlignLog2 = 3;
        break;
      default:
        return false;
    }

    if (!RenderLoadStoreAddress(c, load.address(), defaultAlignLog2))
        return false;

    return c.buffer.append('\n');
}

static bool
RenderStore(WasmRenderContext& c, AstStore& store)
{
    if (!RenderLoadStoreBase(c, store.address()))
        return false;

    if (!RenderExpr(c, store.value()))
        return false;

    if (!RenderIndent(c))
        return false;

    uint32_t defaultAlignLog2;
    switch (store.expr()) {
      case Expr::I32Store8:
        if (!c.buffer.append("i32.store8"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I64Store8:
        if (!c.buffer.append("i64.store8"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I32Store16:
        if (!c.buffer.append("i32.store16"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Store16:
        if (!c.buffer.append("i64.store16"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Store32:
        if (!c.buffer.append("i64.store32"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I32Store:
        if (!c.buffer.append("i32.store"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I64Store:
        if (!c.buffer.append("i64.store"))
            return false;
        defaultAlignLog2 = 3;
        break;
      case Expr::F32Store:
        if (!c.buffer.append("f32.store"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::F64Store:
        if (!c.buffer.append("f64.store"))
            return false;
        defaultAlignLog2 = 3;
        break;
      default:
        return false;
    }

    if (!RenderLoadStoreAddress(c, store.address(), defaultAlignLog2))
        return false;

    return c.buffer.append('\n');
}

static bool
RenderBranch(WasmRenderContext& c, AstBranch& branch)
{
    Expr expr = branch.expr();
    MOZ_ASSERT(expr == Expr::BrIf || expr == Expr::Br);

    if (expr == Expr::BrIf) {
        if (!RenderExpr(c, branch.cond()))
            return false;
    }

    if (branch.maybeValue()) {
        if (!RenderExpr(c, *(branch.maybeValue())))
            return false;
    }

    if (!RenderIndent(c))
        return false;

    if (expr == Expr::BrIf ? !c.buffer.append("br_if ") : !c.buffer.append("br "))
        return false;

    if (!RenderRef(c, branch.target()))
        return false;

    return c.buffer.append('\n');
}

static bool
RenderBrTable(WasmRenderContext& c, AstBranchTable& table)
{
    if (table.maybeValue()) {
      if (!RenderExpr(c, *(table.maybeValue())))
          return false;
    }

    // Index
    if (!RenderExpr(c, table.index()))
        return false;

    if (!RenderIndent(c))
        return false;

    if (!c.buffer.append("br_table "))
        return false;

    uint32_t tableLength = table.table().length();
    for (uint32_t i = 0; i < tableLength; i++) {
        if (!RenderRef(c, table.table()[i]))
            return false;

        if (!c.buffer.append(" "))
            return false;
    }

    if (!RenderRef(c, table.def()))
        return false;

    return c.buffer.append('\n');
}

static bool
RenderReturn(WasmRenderContext& c, AstReturn& ret)
{
    if (ret.maybeExpr()) {
        if (!RenderExpr(c, *(ret.maybeExpr())))
            return false;
    }

    if (!RenderIndent(c))
        return false;

    if (!c.buffer.append("return"))
        return false;

    return c.buffer.append('\n');
}

static bool
RenderExpr(WasmRenderContext& c, AstExpr& expr)
{
    switch (expr.kind()) {
      case AstExprKind::Drop:
        return RenderDrop(c, expr.as<AstDrop>());
      case AstExprKind::Nop:
        return RenderNop(c);
      case AstExprKind::Unreachable:
        return RenderUnreachable(c);
      case AstExprKind::Call:
        return RenderCall(c, expr.as<AstCall>());
      case AstExprKind::CallIndirect:
        return RenderCallIndirect(c, expr.as<AstCallIndirect>());
      case AstExprKind::Const:
        return RenderConst(c, expr.as<AstConst>());
      case AstExprKind::GetLocal:
        return RenderGetLocal(c, expr.as<AstGetLocal>());
      case AstExprKind::SetLocal:
        return RenderSetLocal(c, expr.as<AstSetLocal>());
      case AstExprKind::TeeLocal:
        return RenderTeeLocal(c, expr.as<AstTeeLocal>());
      case AstExprKind::Block:
        return RenderBlock(c, expr.as<AstBlock>());
      case AstExprKind::If:
        return RenderIf(c, expr.as<AstIf>());
      case AstExprKind::NullaryOperator:
        return RenderNullaryOperator(c, expr.as<AstNullaryOperator>());
      case AstExprKind::UnaryOperator:
        return RenderUnaryOperator(c, expr.as<AstUnaryOperator>());
      case AstExprKind::BinaryOperator:
        return RenderBinaryOperator(c, expr.as<AstBinaryOperator>());
      case AstExprKind::TernaryOperator:
        return RenderTernaryOperator(c, expr.as<AstTernaryOperator>());
      case AstExprKind::ComparisonOperator:
        return RenderComparisonOperator(c, expr.as<AstComparisonOperator>());
      case AstExprKind::ConversionOperator:
        return RenderConversionOperator(c, expr.as<AstConversionOperator>());
      case AstExprKind::Load:
        return RenderLoad(c, expr.as<AstLoad>());
      case AstExprKind::Store:
        return RenderStore(c, expr.as<AstStore>());
      case AstExprKind::Branch:
        return RenderBranch(c, expr.as<AstBranch>());
      case AstExprKind::BranchTable:
        return RenderBrTable(c, expr.as<AstBranchTable>());
      case AstExprKind::Return:
        return RenderReturn(c, expr.as<AstReturn>());
      case AstExprKind::First:
        return RenderFirst(c, expr.as<AstFirst>());
      default:
        // Note: it's important not to remove this default since readExpr()
        // can return Expr values for which there is no enumerator.
        break;
    }

    return false;
}

static bool
RenderSignature(WasmRenderContext& c, const AstSig& sig, const AstNameVector* maybeLocals = nullptr)
{
    uint32_t paramsNum = sig.args().length();

    if (maybeLocals) {
      for (uint32_t i = 0; i < paramsNum; i++) {
          if (!c.buffer.append(" (param "))
              return false;
          const AstName& name = (*maybeLocals)[i];
          if (!name.empty()) {
              if (!RenderName(c, name))
                  return false;
              if (!c.buffer.append(" "))
                  return false;
          }
          ValType arg = sig.args()[i];
          if (!RenderValType(c, arg))
              return false;
          if (!c.buffer.append(")"))
              return false;
      }
    } else if (paramsNum > 0) {
      if (!c.buffer.append(" (param"))
          return false;
      for (uint32_t i = 0; i < paramsNum; i++) {
          if (!c.buffer.append(" "))
              return false;
          ValType arg = sig.args()[i];
          if (!RenderValType(c, arg))
              return false;
      }
      if (!c.buffer.append(")"))
          return false;
    }
    if (sig.ret() != ExprType::Void) {
        if (!c.buffer.append(" (result "))
            return false;
        if (!RenderExprType(c, sig.ret()))
            return false;
        if (!c.buffer.append(")"))
            return false;
    }
    return true;
}

static bool
RenderTypeSection(WasmRenderContext& c, const AstModule::SigVector& sigs)
{
    uint32_t numSigs = sigs.length();
    if (!numSigs)
        return true;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        const AstSig* sig = sigs[sigIndex];
        if (!RenderIndent(c))
            return false;
        if (!c.buffer.append("(type"))
            return false;
        if (!sig->name().empty()) {
            if (!c.buffer.append(" "))
                return false;
            if (!RenderName(c, sig->name()))
                return false;
        }
        if (!c.buffer.append(" (func"))
            return false;
        if (!RenderSignature(c, *sig))
            return false;
        if (!c.buffer.append("))\n"))
            return false;
    }
    return true;
}

static bool
RenderTableSection(WasmRenderContext& c, const AstModule& module)
{
    if (module.elemSegments().empty())
        return true;

    const AstElemSegment& segment = *module.elemSegments()[0];

    if (!RenderIndent(c))
        return false;

    if (!c.buffer.append("(table"))
        return false;

    for (const AstRef& elem : segment.elems()) {
        if (!c.buffer.append(" "))
            return false;
        uint32_t index = elem.index();
        AstName name = index < module.funcImportNames().length()
                           ? module.funcImportNames()[index]
                           : module.funcs()[index - module.funcImportNames().length()]->name();
        if (name.empty()) {
            if (!RenderInt32(c, index))
                return false;
        } else {
          if (!RenderName(c, name))
              return false;
        }
    }

    if (!c.buffer.append(")\n"))
        return false;

    return true;
}

static bool
RenderImport(WasmRenderContext& c, AstImport& import, const AstModule::SigVector& sigs)
{
    const AstSig* sig = sigs[import.funcSig().index()];
    if (!RenderIndent(c))
        return false;
    if (!c.buffer.append("(import "))
        return false;
    if (!RenderName(c, import.name()))
        return false;
    if (!c.buffer.append(" \""))
        return false;

    const AstName& moduleName = import.module();
    if (!RenderEscapedString(c, moduleName))
        return false;

    if (!c.buffer.append("\" \""))
        return false;

    const AstName& fieldName = import.field();
    if (!RenderEscapedString(c, fieldName))
        return false;

    if (!c.buffer.append("\""))
        return false;

    if (!RenderSignature(c, *sig))
        return false;
    if (!c.buffer.append(")\n"))
        return false;

    return true;
}


static bool
RenderImportSection(WasmRenderContext& c, const AstModule::ImportVector& imports, const AstModule::SigVector& sigs)
{
    uint32_t numImports = imports.length();

    for (uint32_t i = 0; i < numImports; i++) {
        if (!RenderImport(c, *imports[i], sigs))
            return false;
    }

    return true;
}

static bool
RenderExport(WasmRenderContext& c, AstExport& export_,
             const AstModule::NameVector& funcImportNames,
             const AstModule::FuncVector& funcs)
{
    if (!RenderIndent(c))
        return false;
    if (!c.buffer.append("(export \""))
        return false;
    if (!RenderEscapedString(c, export_.name()))
        return false;
    if (!c.buffer.append("\" "))
        return false;
    if (export_.kind() == DefinitionKind::Memory) {
        if (!c.buffer.append("memory"))
          return false;
    } else {
        uint32_t index = export_.ref().index();
        AstName name = index < funcImportNames.length()
                           ? funcImportNames[index]
                           : funcs[index - funcImportNames.length()]->name();
        if (name.empty()) {
            if (!RenderInt32(c, index))
                return false;
        } else {
            if (!RenderName(c, name))
                return false;
        }
    }
    if (!c.buffer.append(")\n"))
        return false;

    return true;
}

static bool
RenderExportSection(WasmRenderContext& c, const AstModule::ExportVector& exports,
                    const AstModule::NameVector& funcImportNames,
                    const AstModule::FuncVector& funcs)
{
    uint32_t numExports = exports.length();
    for (uint32_t i = 0; i < numExports; i++) {
        if (!RenderExport(c, *exports[i], funcImportNames, funcs))
            return false;
    }
    return true;
}

static bool
RenderFunctionBody(WasmRenderContext& c, AstFunc& func, const AstModule::SigVector& sigs)
{
    const AstSig* sig = sigs[func.sig().index()];

    uint32_t argsNum = sig->args().length();
    uint32_t localsNum = func.vars().length();
    if (localsNum > 0) {
        if (!RenderIndent(c))
            return false;
        for (uint32_t i = 0; i < localsNum; i++) {
            if (!c.buffer.append("(local "))
                return false;
            const AstName& name = func.locals()[argsNum + i];
            if (!name.empty()) {
              if (!RenderName(c, name))
                  return false;
              if (!c.buffer.append(" "))
                  return false;
            }
            ValType local = func.vars()[i];
            if (!RenderValType(c, local))
                return false;
            if (!c.buffer.append(") "))
                return false;
        }
        if (!c.buffer.append("\n"))
            return false;
    }


    uint32_t exprsNum = func.body().length();
    for (uint32_t i = 0; i < exprsNum; i++) {
      if (!RenderExpr(c, *func.body()[i]))
          return false;
    }

    return true;
}

static bool
RenderCodeSection(WasmRenderContext& c, const AstModule::FuncVector& funcs, const AstModule::SigVector& sigs)
{
    uint32_t numFuncBodies = funcs.length();
    for (uint32_t funcIndex = 0; funcIndex < numFuncBodies; funcIndex++) {
        AstFunc* func = funcs[funcIndex];
        uint32_t sigIndex = func->sig().index();
        AstSig* sig = sigs[sigIndex];

        if (!RenderIndent(c))
            return false;
        if (!c.buffer.append("(func "))
            return false;
        if (!func->name().empty()) {
          if (!RenderName(c, func->name()))
              return false;
        }

        if (!RenderSignature(c, *sig, &(func->locals())))
            return false;
        if (!c.buffer.append("\n"))
            return false;

        c.currentFuncIndex = funcIndex;

        c.indent++;
        if (!RenderFunctionBody(c, *func, sigs))
            return false;
        c.indent--;
        if (!RenderIndent(c))
            return false;
        if (!c.buffer.append(")\n"))
            return false;
    }

   return true;
}


static bool
RenderDataSection(WasmRenderContext& c, const AstModule& module)
{
    if (!module.hasMemory())
        return true;

    if (!RenderIndent(c))
        return false;
    if (!c.buffer.append("(memory "))
        return false;
    if (!RenderInt32(c, module.memory().initial))
       return false;
    Maybe<uint32_t> memMax = module.memory().maximum;
    if (memMax) {
        if (!c.buffer.append(" "))
            return false;
        if (!RenderInt32(c, *memMax))
            return false;
    }

    c.indent++;

    uint32_t numSegments = module.dataSegments().length();
    if (!numSegments) {
      if (!c.buffer.append(")\n"))
          return false;
      return true;
    }
    if (!c.buffer.append("\n"))
        return false;

    for (uint32_t i = 0; i < numSegments; i++) {
        const AstDataSegment* segment = module.dataSegments()[i];

        if (!RenderIndent(c))
            return false;
        if (!c.buffer.append("(segment "))
           return false;
        if (!RenderInt32(c, segment->offset()->as<AstConst>().val().i32()))
           return false;
        if (!c.buffer.append(" \""))
           return false;

        RenderEscapedString(c, segment->text());

        if (!c.buffer.append("\")\n"))
           return false;
    }

    c.indent--;
    if (!c.buffer.append(")\n"))
        return false;

    return true;
}

static bool
RenderModule(WasmRenderContext& c, AstModule& module)
{
    if (!c.buffer.append("(module\n"))
        return false;

    c.indent++;

    if (!RenderTypeSection(c, module.sigs()))
        return false;

    if (!RenderImportSection(c, module.imports(), module.sigs()))
        return false;

    if (!RenderTableSection(c, module))
        return false;

    if (!RenderExportSection(c, module.exports(), module.funcImportNames(), module.funcs()))
        return false;

    if (!RenderCodeSection(c, module.funcs(), module.sigs()))
        return false;

    if (!RenderDataSection(c, module))
        return false;

    c.indent--;

    if (!c.buffer.append(")"))
        return false;

    return true;
}

/*****************************************************************************/
// Top-level functions

bool
wasm::BinaryToText(JSContext* cx, const uint8_t* bytes, size_t length, StringBuffer& buffer)
{

    LifoAlloc lifo(AST_LIFO_DEFAULT_CHUNK_SIZE);

    AstModule* module;
    if (!BinaryToAst(cx, bytes, length, lifo, &module))
        return false;

    WasmRenderContext c(cx, module, buffer);

    if (!RenderModule(c, *module)) {
        if (!cx->isExceptionPending())
            ReportOutOfMemory(cx);
        return false;
    }

    return true;
}


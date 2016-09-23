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

#include "asmjs/WasmBinaryToExperimentalText.h"

#include "mozilla/CheckedInt.h"

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

using mozilla::CheckedInt;
using mozilla::IsInfinite;
using mozilla::IsNaN;
using mozilla::IsNegativeZero;

enum PrintOperatorPrecedence
{
    ExpressionPrecedence = 0,
    AssignmentPrecedence = 1,
    StoreOperatorPrecedence = 1,
    BitwiseOrPrecedence = 4,
    BitwiseXorPrecedence = 5,
    BitwiseAndPrecedence = 6,
    EqualityPrecedence = 7,
    ComparisonPrecedence = 8,
    BitwiseShiftPrecedence = 9,
    AdditionPrecedence = 10,
    MultiplicationPrecedence = 11,
    NegatePrecedence = 12,
    EqzPrecedence = 12,
    OperatorPrecedence = 15,
    LoadOperatorPrecedence = 15,
    CallPrecedence = 15,
    GroupPrecedence = 16,
};

// StringBuffer wrapper to track the position (line and column) within the generated
// source.
class WasmPrintBuffer
{
    StringBuffer& stringBuffer_;
    uint32_t lineno_;
    uint32_t column_;

  public:
    explicit WasmPrintBuffer(StringBuffer& stringBuffer) : stringBuffer_(stringBuffer), lineno_(1), column_(1) {}
    inline char processChar(char ch) {
        if (ch == '\n') {
            lineno_++; column_ = 1;
        } else
            column_++;
        return ch;
    }
    inline char16_t processChar(char16_t ch) {
        if (ch == '\n') {
            lineno_++; column_ = 1;
        } else
            column_++;
        return ch;
    }
    bool append(const char ch) {
        return stringBuffer_.append(processChar(ch));
    }
    bool append(const char16_t ch) {
        return stringBuffer_.append(processChar(ch));
    }
    bool append(const char* str, size_t length) {
        for (size_t i = 0; i < length; i++)
            processChar(str[i]);
        return stringBuffer_.append(str, length);
    }
    bool append(const char16_t* begin, const char16_t* end) {
        for (const char16_t* p = begin; p != end; p++)
            processChar(*p);
        return stringBuffer_.append(begin, end);
    }
    bool append(const char16_t* str, size_t length) {
        return append(str, str + length);
    }
    template <size_t ArrayLength>
    bool append(const char (&array)[ArrayLength]) {
        return append(array, ArrayLength - 1);
    }
    char16_t getChar(size_t index) {
        return stringBuffer_.getChar(index);
    }
    size_t length() {
        return stringBuffer_.length();
    }
    StringBuffer& stringBuffer() { return stringBuffer_; }
    uint32_t lineno() { return lineno_; }
    uint32_t column() { return column_; }
};

struct WasmPrintContext
{
    JSContext* cx;
    AstModule* module;
    WasmPrintBuffer& buffer;
    const ExperimentalTextFormatting& f;
    GeneratedSourceMap* maybeSourceMap;
    uint32_t indent;

    uint32_t currentFuncIndex;
    PrintOperatorPrecedence currentPrecedence;

    WasmPrintContext(JSContext* cx, AstModule* module, WasmPrintBuffer& buffer, const ExperimentalTextFormatting& f, GeneratedSourceMap* wasmSourceMap_)
      : cx(cx),
        module(module),
        buffer(buffer),
        f(f),
        maybeSourceMap(wasmSourceMap_),
        indent(0),
        currentFuncIndex(0),
        currentPrecedence(PrintOperatorPrecedence::ExpressionPrecedence)
    {}

    StringBuffer& sb() { return buffer.stringBuffer(); }
};

/*****************************************************************************/
// utilities

static bool
IsDropValueExpr(AstExpr& expr)
{
    // Based on AST information, determines if the expression does not return a value.
    // TODO infer presence of a return value for rest kinds of expressions from
    // the function return type.
    switch (expr.kind()) {
      case AstExprKind::Branch:
        return !expr.as<AstBranch>().maybeValue();
      case AstExprKind::BranchTable:
        return !expr.as<AstBranchTable>().maybeValue();
      case AstExprKind::If:
        return !expr.as<AstIf>().hasElse();
      case AstExprKind::NullaryOperator:
        return expr.as<AstNullaryOperator>().expr() == Expr::Nop;
      case AstExprKind::Unreachable:
      case AstExprKind::Return:
        return true;
      default:
        return false;
    }
}

static bool
PrintIndent(WasmPrintContext& c)
{
    for (uint32_t i = 0; i < c.indent; i++) {
        if (!c.buffer.append("  "))
            return false;
    }
    return true;
}

static bool
PrintInt32(WasmPrintContext& c, int32_t num, bool printSign = false)
{
    // Negative sign will be printed, printing '+' for non-negative values.
    if (printSign && num >= 0) {
        if (!c.buffer.append("+"))
            return false;
    }
    return NumberValueToStringBuffer(c.cx, Int32Value(num), c.buffer.stringBuffer());
}

static bool
PrintInt64(WasmPrintContext& c, int64_t num)
{
    if (num < 0 && !c.buffer.append("-"))
        return false;
    if (!num)
        return c.buffer.append("0");

    uint64_t abs = mozilla::Abs(num);
    uint64_t n = abs;
    uint64_t pow = 1;
    while (n) {
        pow *= 10;
        n /= 10;
    }
    pow /= 10;

    n = abs;
    while (pow) {
        if (!c.buffer.append((char16_t)(u'0' + n / pow)))
            return false;
        n -= (n / pow) * pow;
        pow /= 10;
    }

    return true;
}

static bool
PrintDouble(WasmPrintContext& c, RawF64 num)
{
    double d = num.fp();
    if (IsNegativeZero(d))
        return c.buffer.append("-0.0");
    if (IsNaN(d))
        return RenderNaN(c.sb(), num);
    if (IsInfinite(d)) {
        if (d > 0)
            return c.buffer.append("infinity");
        return c.buffer.append("-infinity");
    }

    uint32_t startLength = c.buffer.length();
    if (!NumberValueToStringBuffer(c.cx, DoubleValue(d), c.buffer.stringBuffer()))
        return false;
    MOZ_ASSERT(startLength < c.buffer.length());

    // Checking if we need to end number with '.0'.
    for (uint32_t i = c.buffer.length() - 1; i >= startLength; i--) {
        char16_t ch = c.buffer.getChar(i);
        if (ch == '.' || ch == 'e')
            return true;
    }
    return c.buffer.append(".0");
}

static bool
PrintFloat32(WasmPrintContext& c, RawF32 num)
{
    float f = num.fp();
    if (IsNaN(f))
        return RenderNaN(c.sb(), num) && c.buffer.append(".f");
    return PrintDouble(c, RawF64(double(f))) &&
           c.buffer.append("f");
}

static bool
PrintEscapedString(WasmPrintContext& c, const AstName& s)
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
                if (!c.buffer.append((char)(digit1 < 10 ? digit1 + '0' : digit1 - 10 + 'a')))
                    return false;
                if (!c.buffer.append((char)(digit2 < 10 ? digit2 + '0' : digit2 - 10 + 'a')))
                    return false;
            }
            break;
        }
    }
    return true;
}

static bool
PrintExprType(WasmPrintContext& c, ExprType type)
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
PrintValType(WasmPrintContext& c, ValType type)
{
    return PrintExprType(c, ToExprType(type));
}

static bool
PrintName(WasmPrintContext& c, const AstName& name)
{
    return c.buffer.append(name.begin(), name.end());
}

static bool
PrintRef(WasmPrintContext& c, const AstRef& ref)
{
    if (ref.name().empty())
        return PrintInt32(c, ref.index());

    return PrintName(c, ref.name());
}

static bool
PrintExpr(WasmPrintContext& c, AstExpr& expr);

static bool
PrintBlockLevelExpr(WasmPrintContext& c, AstExpr& expr, bool isLast)
{
    if (!PrintIndent(c))
        return false;
    if (!PrintExpr(c, expr))
        return false;
    if (!isLast || IsDropValueExpr(expr)) {
        if (!c.buffer.append(';'))
            return false;
    }
    return c.buffer.append('\n');
}

/*****************************************************************************/
// binary format parsing and rendering

static bool
PrintNullaryOperator(WasmPrintContext& c, AstNullaryOperator& op)
{
    const char* opStr;

    switch (op.expr()) {
        case Expr::Nop:             opStr = "nop"; break;
        case Expr::CurrentMemory:   opStr = "curent_memory"; break;
        default:  return false;
    }

    return c.buffer.append(opStr, strlen(opStr));
}

static bool
PrintUnreachable(WasmPrintContext& c, AstUnreachable& unreachable)
{
    return c.buffer.append("unreachable");
}

static bool
PrintCallArgs(WasmPrintContext& c, const AstExprVector& args)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;
    c.currentPrecedence = ExpressionPrecedence;

    if (!c.buffer.append("("))
        return false;
    for (uint32_t i = 0; i < args.length(); i++) {
        if (!PrintExpr(c, *args[i]))
            return false;
        if (i + 1 == args.length())
            break;
        if (!c.buffer.append(", "))
            return false;
    }
    if (!c.buffer.append(")"))
        return false;

    c.currentPrecedence = lastPrecedence;
    return true;
}

static bool
PrintCall(WasmPrintContext& c, AstCall& call)
{
    if (call.expr() == Expr::Call) {
        if (!c.buffer.append("call "))
            return false;
    } else if (call.expr() == Expr::CallImport) {
        if (!c.buffer.append("call_import "))
            return false;
    } else {
        return false;
    }

    if (!PrintRef(c, call.func()))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!PrintCallArgs(c, call.args()))
        return false;

    return true;
}

static bool
PrintCallIndirect(WasmPrintContext& c, AstCallIndirect& call)
{
    if (!c.buffer.append("call_indirect "))
        return false;
    if (!PrintRef(c, call.sig()))
        return false;

    if (!c.buffer.append(" ["))
        return false;

    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;
    c.currentPrecedence = ExpressionPrecedence;

    if (!PrintExpr(c, *call.index()))
        return false;

    c.currentPrecedence = lastPrecedence;

    if (!c.buffer.append("] "))
        return false;
    if (!PrintCallArgs(c, call.args()))
        return false;
    return true;
}

static bool
PrintConst(WasmPrintContext& c, AstConst& cst)
{
    switch (ToExprType(cst.val().type())) {
      case ExprType::I32:
        if (!PrintInt32(c, (uint32_t)cst.val().i32()))
            return false;
        break;
      case ExprType::I64:
        if (!PrintInt64(c, (uint32_t)cst.val().i64()))
            return false;
        if (!c.buffer.append("i64"))
            return false;
        break;
      case ExprType::F32:
        if (!PrintFloat32(c, cst.val().f32()))
            return false;
        break;
      case ExprType::F64:
        if (!PrintDouble(c, cst.val().f64()))
            return false;
        break;
      default:
        return false;
    }
    return true;
}

static bool
PrintGetLocal(WasmPrintContext& c, AstGetLocal& gl)
{
    if (!PrintRef(c, gl.local()))
        return false;
    return true;
}

static bool
PrintSetLocal(WasmPrintContext& c, AstSetLocal& sl)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    if (!c.f.reduceParens || lastPrecedence > AssignmentPrecedence) {
        if (!c.buffer.append("("))
            return false;
    }

    if (!PrintRef(c, sl.local()))
        return false;
    if (!c.buffer.append(" = "))
        return false;

    c.currentPrecedence = AssignmentPrecedence;

    if (!PrintExpr(c, sl.value()))
        return false;

    if (!c.f.reduceParens || lastPrecedence > AssignmentPrecedence) {
        if (!c.buffer.append(")"))
            return false;
    }

    c.currentPrecedence = lastPrecedence;
    return true;
}

static bool
PrintExprList(WasmPrintContext& c, const AstExprVector& exprs, uint32_t startFrom = 0)
{
    for (uint32_t i = startFrom; i < exprs.length(); i++) {
        if (!PrintBlockLevelExpr(c, *exprs[i], i + 1 == exprs.length()))
            return false;
    }
    return true;
}

static bool
PrintGroupedBlock(WasmPrintContext& c, AstBlock& block)
{
    uint32_t skip = 0;
    if (block.exprs().length() > 0 &&
        block.exprs()[0]->kind() == AstExprKind::Block) {
        if (!PrintGroupedBlock(c, *static_cast<AstBlock*>(block.exprs()[0])))
            return false;
        skip = 1;
    }
    c.indent++;
    if (!PrintExprList(c, block.exprs(), skip))
        return false;
    c.indent--;
    if (!PrintIndent(c))
        return false;

    // If no br/br_if/br_table refer this block, use some non-existent label.
    if (block.breakName().empty())
        return c.buffer.append("$label:\n");

    if (!PrintName(c, block.breakName()))
        return false;
    if (!c.buffer.append(":\n"))
        return false;
    return true;
}

static bool
PrintBlockName(WasmPrintContext& c, const AstName& name) {
    if (name.empty())
        return true;

    if (!PrintIndent(c))
        return false;
    if (!PrintName(c, name))
        return false;
    return c.buffer.append(":\n");
}

static bool
PrintBlock(WasmPrintContext& c, AstBlock& block)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;
    if (block.expr() == Expr::Block) {
        if (!c.buffer.append("{\n"))
            return false;
    } else if (block.expr() == Expr::Loop) {
        if (!c.buffer.append("loop"))
            return false;
        if (!block.continueName().empty()) {
            if (!c.buffer.append(" "))
                return false;
            if (!PrintName(c, block.continueName()))
                return false;
        }
        if (!c.buffer.append(" {\n"))
            return false;
    } else
        return false;

    c.currentPrecedence = ExpressionPrecedence;

    bool skip = 0;
    if (c.f.groupBlocks && block.expr() == Expr::Block &&
        block.exprs().length() > 0 && block.exprs()[0]->kind() == AstExprKind::Block) {
        if (!PrintGroupedBlock(c, *static_cast<AstBlock*>(block.exprs()[0])))
            return false;
        skip = 1;
        if (block.exprs().length() == 1 && block.breakName().empty()) {
          // Special case to resolve ambiguity in parsing of optional end block label.
          if (!PrintIndent(c))
              return false;
          if (!c.buffer.append("$exit$:\n"))
              return false;
        }
    }

    c.indent++;
    if (!PrintExprList(c, block.exprs(), skip))
        return false;
    c.indent--;
    c.currentPrecedence = lastPrecedence;

    if (!PrintBlockName(c, block.breakName()))
      return false;

    if (!PrintIndent(c))
        return false;

    return c.buffer.append("}");
}

static bool
PrintUnaryOperator(WasmPrintContext& c, AstUnaryOperator& op)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    const char* opStr;
    const char* prefixStr = nullptr;
    PrintOperatorPrecedence precedence = OperatorPrecedence;
    switch (op.expr()) {
      case Expr::I32Clz:     opStr = "i32.clz"; break;
      case Expr::I32Ctz:     opStr = "i32.ctz"; break;
      case Expr::I32Popcnt:  opStr = "i32.popcnt"; break;
      case Expr::I64Clz:     opStr = "i64.clz"; break;
      case Expr::I64Ctz:     opStr = "i64.ctz"; break;
      case Expr::I64Popcnt:  opStr = "i64.popcnt"; break;
      case Expr::F32Abs:     opStr = "f32.abs"; break;
      case Expr::F32Neg:     opStr = "f32.neg"; prefixStr = "-"; precedence = NegatePrecedence; break;
      case Expr::F32Ceil:    opStr = "f32.ceil"; break;
      case Expr::F32Floor:   opStr = "f32.floor"; break;
      case Expr::F32Sqrt:    opStr = "f32.sqrt"; break;
      case Expr::F32Trunc:   opStr = "f32.trunc"; break;
      case Expr::F32Nearest: opStr = "f32.nearest"; break;
      case Expr::F64Abs:     opStr = "f64.abs"; break;
      case Expr::F64Neg:     opStr = "f64.neg"; prefixStr = "-"; precedence = NegatePrecedence; break;
      case Expr::F64Ceil:    opStr = "f64.ceil"; break;
      case Expr::F64Floor:   opStr = "f64.floor"; break;
      case Expr::F64Sqrt:    opStr = "f64.sqrt"; break;
      case Expr::GrowMemory: opStr = "grow_memory"; break;
      default: return false;
    }

    if (c.f.allowAsciiOperators && prefixStr) {
        if (!c.f.reduceParens || lastPrecedence > precedence) {
            if (!c.buffer.append("("))
                return false;
        }

        c.currentPrecedence = precedence;
        if (!c.buffer.append(prefixStr, strlen(prefixStr)))
            return false;
        if (!PrintExpr(c, *op.op()))
            return false;

        if (!c.f.reduceParens || lastPrecedence > precedence) {
          if (!c.buffer.append(")"))
              return false;
        }
    } else {
        if (!c.buffer.append(opStr, strlen(opStr)))
            return false;
        if (!c.buffer.append("("))
            return false;

        c.currentPrecedence = ExpressionPrecedence;
        if (!PrintExpr(c, *op.op()))
            return false;

        if (!c.buffer.append(")"))
            return false;
    }
    c.currentPrecedence = lastPrecedence;

    return true;
}

static bool
PrintBinaryOperator(WasmPrintContext& c, AstBinaryOperator& op)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    const char* opStr;
    const char* infixStr = nullptr;
    PrintOperatorPrecedence precedence;
    switch (op.expr()) {
      case Expr::I32Add:      opStr = "i32.add"; infixStr = "+"; precedence = AdditionPrecedence; break;
      case Expr::I32Sub:      opStr = "i32.sub"; infixStr = "-"; precedence = AdditionPrecedence; break;
      case Expr::I32Mul:      opStr = "i32.mul"; infixStr = "*"; precedence = MultiplicationPrecedence; break;
      case Expr::I32DivS:     opStr = "i32.div_s"; infixStr = "/s"; precedence = MultiplicationPrecedence; break;
      case Expr::I32DivU:     opStr = "i32.div_u"; infixStr = "/u"; precedence = MultiplicationPrecedence; break;
      case Expr::I32RemS:     opStr = "i32.rem_s"; infixStr = "%s"; precedence = MultiplicationPrecedence; break;
      case Expr::I32RemU:     opStr = "i32.rem_u"; infixStr = "%u"; precedence = MultiplicationPrecedence; break;
      case Expr::I32And:      opStr = "i32.and"; infixStr = "&"; precedence = BitwiseAndPrecedence; break;
      case Expr::I32Or:       opStr = "i32.or"; infixStr = "|"; precedence = BitwiseOrPrecedence; break;
      case Expr::I32Xor:      opStr = "i32.xor"; infixStr = "^"; precedence = BitwiseXorPrecedence; break;
      case Expr::I32Shl:      opStr = "i32.shl"; infixStr = "<<"; precedence = BitwiseShiftPrecedence; break;
      case Expr::I32ShrS:     opStr = "i32.shr_s"; infixStr = ">>s"; precedence = BitwiseShiftPrecedence; break;
      case Expr::I32ShrU:     opStr = "i32.shr_u"; infixStr = ">>u"; precedence = BitwiseShiftPrecedence; break;
      case Expr::I64Add:      opStr = "i64.add"; infixStr = "+"; precedence = AdditionPrecedence; break;
      case Expr::I64Sub:      opStr = "i64.sub"; infixStr = "-"; precedence = AdditionPrecedence; break;
      case Expr::I64Mul:      opStr = "i64.mul"; infixStr = "*"; precedence = MultiplicationPrecedence; break;
      case Expr::I64DivS:     opStr = "i64.div_s"; infixStr = "/s"; precedence = MultiplicationPrecedence; break;
      case Expr::I64DivU:     opStr = "i64.div_u"; infixStr = "/u"; precedence = MultiplicationPrecedence; break;
      case Expr::I64RemS:     opStr = "i64.rem_s"; infixStr = "%s"; precedence = MultiplicationPrecedence; break;
      case Expr::I64RemU:     opStr = "i64.rem_u"; infixStr = "%u"; precedence = MultiplicationPrecedence; break;
      case Expr::I64And:      opStr = "i64.and"; infixStr = "&"; precedence = BitwiseAndPrecedence; break;
      case Expr::I64Or:       opStr = "i64.or"; infixStr = "|"; precedence = BitwiseOrPrecedence; break;
      case Expr::I64Xor:      opStr = "i64.xor"; infixStr = "^"; precedence = BitwiseXorPrecedence; break;
      case Expr::I64Shl:      opStr = "i64.shl"; infixStr = "<<"; precedence = BitwiseShiftPrecedence; break;
      case Expr::I64ShrS:     opStr = "i64.shr_s"; infixStr = ">>s"; precedence = BitwiseShiftPrecedence; break;
      case Expr::I64ShrU:     opStr = "i64.shr_u"; infixStr = ">>u"; precedence = BitwiseShiftPrecedence; break;
      case Expr::F32Add:      opStr = "f32.add"; infixStr = "+"; precedence = AdditionPrecedence; break;
      case Expr::F32Sub:      opStr = "f32.sub"; infixStr = "-"; precedence = AdditionPrecedence; break;
      case Expr::F32Mul:      opStr = "f32.mul"; infixStr = "*"; precedence = MultiplicationPrecedence; break;
      case Expr::F32Div:      opStr = "f32.div"; infixStr = "/"; precedence = MultiplicationPrecedence; break;
      case Expr::F32Min:      opStr = "f32.min"; precedence = OperatorPrecedence; break;
      case Expr::F32Max:      opStr = "f32.max"; precedence = OperatorPrecedence; break;
      case Expr::F32CopySign: opStr = "f32.copysign"; precedence = OperatorPrecedence; break;
      case Expr::F64Add:      opStr = "f64.add"; infixStr = "+"; precedence = AdditionPrecedence; break;
      case Expr::F64Sub:      opStr = "f64.sub"; infixStr = "-"; precedence = AdditionPrecedence; break;
      case Expr::F64Mul:      opStr = "f64.mul"; infixStr = "*"; precedence = MultiplicationPrecedence; break;
      case Expr::F64Div:      opStr = "f64.div"; infixStr = "/"; precedence = MultiplicationPrecedence; break;
      case Expr::F64Min:      opStr = "f64.min"; precedence = OperatorPrecedence; break;
      case Expr::F64Max:      opStr = "f64.max"; precedence = OperatorPrecedence; break;
      case Expr::F64CopySign: opStr = "f64.copysign"; precedence = OperatorPrecedence; break;
      default: return false;
    }

    if (c.f.allowAsciiOperators && infixStr) {
        if (!c.f.reduceParens || lastPrecedence > precedence) {
            if (!c.buffer.append("("))
                return false;
        }

        c.currentPrecedence = precedence;
        if (!PrintExpr(c, *op.lhs()))
            return false;
        if (!c.buffer.append(" "))
            return false;
        if (!c.buffer.append(infixStr, strlen(infixStr)))
            return false;
        if (!c.buffer.append(" "))
            return false;
        // case of  A / (B / C)
        c.currentPrecedence = (PrintOperatorPrecedence)(precedence + 1);

        if (!PrintExpr(c, *op.rhs()))
            return false;
        if (!c.f.reduceParens || lastPrecedence > precedence) {
            if (!c.buffer.append(")"))
                return false;
        }
    } else {
        if (!c.buffer.append(opStr, strlen(opStr)))
            return false;
        if (!c.buffer.append("("))
            return false;

        c.currentPrecedence = ExpressionPrecedence;
        if (!PrintExpr(c, *op.lhs()))
            return false;
        if (!c.buffer.append(", "))
            return false;
        if (!PrintExpr(c, *op.rhs()))
            return false;

        if (!c.buffer.append(")"))
            return false;
    }
    c.currentPrecedence = lastPrecedence;

    return true;
}

static bool
PrintTernaryOperator(WasmPrintContext& c, AstTernaryOperator& op)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    const char* opStr;
    switch (op.expr()) {
      case Expr::Select: opStr = "select"; break;
      default: return false;
    }

    if (!c.buffer.append(opStr, strlen(opStr)))
        return false;
    if (!c.buffer.append("("))
        return false;

    c.currentPrecedence = ExpressionPrecedence;
    if (!PrintExpr(c, *op.op0()))
        return false;
    if (!c.buffer.append(", "))
        return false;
    if (!PrintExpr(c, *op.op1()))
        return false;
    if (!c.buffer.append(", "))
        return false;
    if (!PrintExpr(c, *op.op2()))
        return false;

    if (!c.buffer.append(")"))
        return false;
    c.currentPrecedence = lastPrecedence;

    return true;
}

static bool
PrintComparisonOperator(WasmPrintContext& c, AstComparisonOperator& op)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    const char* opStr;
    const char* infixStr = nullptr;
    PrintOperatorPrecedence precedence;
    switch (op.expr()) {
      case Expr::I32Eq:  opStr = "i32.eq"; infixStr = "=="; precedence = EqualityPrecedence; break;
      case Expr::I32Ne:  opStr = "i32.ne"; infixStr = "!="; precedence = EqualityPrecedence; break;
      case Expr::I32LtS: opStr = "i32.lt_s"; infixStr = "<s"; precedence = ComparisonPrecedence; break;
      case Expr::I32LtU: opStr = "i32.lt_u"; infixStr = "<u"; precedence = ComparisonPrecedence; break;
      case Expr::I32LeS: opStr = "i32.le_s"; infixStr = "<=s"; precedence = ComparisonPrecedence; break;
      case Expr::I32LeU: opStr = "i32.le_u"; infixStr = "<=u"; precedence = ComparisonPrecedence; break;
      case Expr::I32GtS: opStr = "i32.gt_s"; infixStr = ">s"; precedence = ComparisonPrecedence; break;
      case Expr::I32GtU: opStr = "i32.gt_u"; infixStr = ">u"; precedence = ComparisonPrecedence; break;
      case Expr::I32GeS: opStr = "i32.ge_s"; infixStr = ">=s"; precedence = ComparisonPrecedence; break;
      case Expr::I32GeU: opStr = "i32.ge_u"; infixStr = ">=u"; precedence = ComparisonPrecedence; break;
      case Expr::I64Eq:  opStr = "i64.eq"; infixStr = "=="; precedence = EqualityPrecedence; break;
      case Expr::I64Ne:  opStr = "i64.ne"; infixStr = "!="; precedence = EqualityPrecedence; break;
      case Expr::I64LtS: opStr = "i64.lt_s"; infixStr = "<s"; precedence = ComparisonPrecedence; break;
      case Expr::I64LtU: opStr = "i64.lt_u"; infixStr = "<u"; precedence = ComparisonPrecedence; break;
      case Expr::I64LeS: opStr = "i64.le_s"; infixStr = "<=s"; precedence = ComparisonPrecedence; break;
      case Expr::I64LeU: opStr = "i64.le_u"; infixStr = "<=u"; precedence = ComparisonPrecedence; break;
      case Expr::I64GtS: opStr = "i64.gt_s"; infixStr = ">s"; precedence = ComparisonPrecedence; break;
      case Expr::I64GtU: opStr = "i64.gt_u"; infixStr = ">u"; precedence = ComparisonPrecedence; break;
      case Expr::I64GeS: opStr = "i64.ge_s"; infixStr = ">=s"; precedence = ComparisonPrecedence; break;
      case Expr::I64GeU: opStr = "i64.ge_u"; infixStr = ">=u"; precedence = ComparisonPrecedence; break;
      case Expr::F32Eq:  opStr = "f32.eq"; infixStr = "=="; precedence = EqualityPrecedence; break;
      case Expr::F32Ne:  opStr = "f32.ne"; infixStr = "!="; precedence = EqualityPrecedence; break;
      case Expr::F32Lt:  opStr = "f32.lt"; infixStr = "<"; precedence = ComparisonPrecedence; break;
      case Expr::F32Le:  opStr = "f32.le"; infixStr = "<="; precedence = ComparisonPrecedence; break;
      case Expr::F32Gt:  opStr = "f32.gt"; infixStr = ">"; precedence = ComparisonPrecedence; break;
      case Expr::F32Ge:  opStr = "f32.ge"; infixStr = ">="; precedence = ComparisonPrecedence; break;
      case Expr::F64Eq:  opStr = "f64.eq"; infixStr = "=="; precedence = ComparisonPrecedence; break;
      case Expr::F64Ne:  opStr = "f64.ne"; infixStr = "!="; precedence = EqualityPrecedence; break;
      case Expr::F64Lt:  opStr = "f64.lt"; infixStr = "<"; precedence = EqualityPrecedence; break;
      case Expr::F64Le:  opStr = "f64.le"; infixStr = "<="; precedence = ComparisonPrecedence; break;
      case Expr::F64Gt:  opStr = "f64.gt"; infixStr = ">"; precedence = ComparisonPrecedence; break;
      case Expr::F64Ge:  opStr = "f64.ge"; infixStr = ">="; precedence = ComparisonPrecedence; break;
      default: return false;
    }

    if (c.f.allowAsciiOperators && infixStr) {
        if (!c.f.reduceParens || lastPrecedence > precedence) {
            if (!c.buffer.append("("))
                return false;
        }
        c.currentPrecedence = precedence;
        if (!PrintExpr(c, *op.lhs()))
            return false;
        if (!c.buffer.append(" "))
            return false;
        if (!c.buffer.append(infixStr, strlen(infixStr)))
            return false;
        if (!c.buffer.append(" "))
            return false;
        // case of  A == (B == C)
        c.currentPrecedence = (PrintOperatorPrecedence)(precedence + 1);
        if (!PrintExpr(c, *op.rhs()))
            return false;
        if (!c.f.reduceParens || lastPrecedence > precedence) {
            if (!c.buffer.append(")"))
                return false;
        }
    } else {
        if (!c.buffer.append(opStr, strlen(opStr)))
            return false;
        c.currentPrecedence = ExpressionPrecedence;
        if (!c.buffer.append("("))
            return false;
        if (!PrintExpr(c, *op.lhs()))
            return false;
        if (!c.buffer.append(", "))
            return false;
        if (!PrintExpr(c, *op.rhs()))
            return false;
        if (!c.buffer.append(")"))
            return false;
    }
    c.currentPrecedence = lastPrecedence;

    return true;
}

static bool
PrintConversionOperator(WasmPrintContext& c, AstConversionOperator& op)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    const char* opStr;
    const char* prefixStr = nullptr;
    PrintOperatorPrecedence precedence = ExpressionPrecedence;
    switch (op.expr()) {
      case Expr::I32Eqz:            opStr = "i32.eqz"; prefixStr = "!"; precedence = EqzPrecedence; break;
      case Expr::I32WrapI64:        opStr = "i32.wrap/i64"; break;
      case Expr::I32TruncSF32:      opStr = "i32.trunc_s/f32"; break;
      case Expr::I32TruncUF32:      opStr = "i32.trunc_u/f32"; break;
      case Expr::I32ReinterpretF32: opStr = "i32.reinterpret/f32"; break;
      case Expr::I32TruncSF64:      opStr = "i32.trunc_s/f64"; break;
      case Expr::I32TruncUF64:      opStr = "i32.trunc_u/f64"; break;
      case Expr::I64Eqz:            opStr = "i64.eqz"; prefixStr = "!"; precedence = EqzPrecedence; break;
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
      default: return false;
    }

    if (c.f.allowAsciiOperators && prefixStr) {
        if (!c.f.reduceParens || lastPrecedence > precedence) {
            if (!c.buffer.append("("))
                return false;
        }

        c.currentPrecedence = precedence;
        if (!c.buffer.append(prefixStr, strlen(prefixStr)))
            return false;
        if (!PrintExpr(c, *op.op()))
            return false;

        if (!c.f.reduceParens || lastPrecedence > precedence) {
          if (!c.buffer.append(")"))
              return false;
        }
    } else {
        if (!c.buffer.append(opStr, strlen(opStr)))
            return false;
        if (!c.buffer.append("("))
            return false;

        c.currentPrecedence = ExpressionPrecedence;
        if (!PrintExpr(c, *op.op()))
            return false;

        if (!c.buffer.append(")"))
            return false;
    }
    c.currentPrecedence = lastPrecedence;

    return true;
}

static bool
PrintIf(WasmPrintContext& c, AstIf& if_)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    c.currentPrecedence = ExpressionPrecedence;
    if (!c.buffer.append("if ("))
        return false;
    if (!PrintExpr(c, if_.cond()))
        return false;

    if (!c.buffer.append(") {\n"))
        return false;

    c.indent++;
    if (!PrintExprList(c, if_.thenExprs()))
        return false;
    c.indent--;

    if (!PrintBlockName(c, if_.thenName()))
        return false;

    if (if_.hasElse()) {
        if (!PrintIndent(c))
            return false;
        if (!c.buffer.append("} else {\n"))
            return false;

        c.indent++;
        if (!PrintExprList(c, if_.elseExprs()))
            return false;
        c.indent--;
        if (!PrintBlockName(c, if_.elseName()))
            return false;
    }

    if (!PrintIndent(c))
        return false;

    c.currentPrecedence = lastPrecedence;

    return c.buffer.append("}");
}

static bool
PrintLoadStoreAddress(WasmPrintContext& c, const AstLoadStoreAddress& lsa, uint32_t defaultAlignLog2)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    c.currentPrecedence = ExpressionPrecedence;

    if (!c.buffer.append("["))
        return false;
    if (!PrintExpr(c, lsa.base()))
        return false;

    if (lsa.offset() != 0) {
        if (!c.buffer.append(", "))
            return false;
        if (!PrintInt32(c, lsa.offset(), true))
            return false;
    }
    if (!c.buffer.append("]"))
        return false;

    uint32_t alignLog2 = lsa.flags();
    if (defaultAlignLog2 != alignLog2) {
        if (!c.buffer.append(", align="))
            return false;
        if (!PrintInt32(c, 1 << alignLog2))
            return false;
    }

    c.currentPrecedence = lastPrecedence;
    return true;
}

static bool
PrintLoad(WasmPrintContext& c, AstLoad& load)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    c.currentPrecedence = LoadOperatorPrecedence;
    if (!c.f.reduceParens || lastPrecedence > LoadOperatorPrecedence) {
        if (!c.buffer.append("("))
            return false;
    }

    uint32_t defaultAlignLog2;
    switch (load.expr()) {
      case Expr::I32Load8S:
        if (!c.buffer.append("i32:8s"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I64Load8S:
        if (!c.buffer.append("i64:8s"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I32Load8U:
        if (!c.buffer.append("i32:8u"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I64Load8U:
        if (!c.buffer.append("i64:8u"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I32Load16S:
        if (!c.buffer.append("i32:16s"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Load16S:
        if (!c.buffer.append("i64:16s"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I32Load16U:
        if (!c.buffer.append("i32:16u"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Load16U:
        if (!c.buffer.append("i64:16u"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Load32S:
        if (!c.buffer.append("i64:32s"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I64Load32U:
        if (!c.buffer.append("i64:32u"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I32Load:
        if (!c.buffer.append("i32"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I64Load:
        if (!c.buffer.append("i64"))
            return false;
        defaultAlignLog2 = 3;
        break;
      case Expr::F32Load:
        if (!c.buffer.append("f32"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::F64Load:
        if (!c.buffer.append("f64"))
            return false;
        defaultAlignLog2 = 3;
        break;
      default:
        return false;
    }

    if (!PrintLoadStoreAddress(c, load.address(), defaultAlignLog2))
        return false;

    if (!c.f.reduceParens || lastPrecedence > LoadOperatorPrecedence) {
        if (!c.buffer.append(")"))
            return false;
    }
    c.currentPrecedence = lastPrecedence;

    return true;
}

static bool
PrintStore(WasmPrintContext& c, AstStore& store)
{
    PrintOperatorPrecedence lastPrecedence = c.currentPrecedence;

    c.currentPrecedence = StoreOperatorPrecedence;
    if (!c.f.reduceParens || lastPrecedence > StoreOperatorPrecedence) {
        if (!c.buffer.append("("))
            return false;
    }

    uint32_t defaultAlignLog2;
    switch (store.expr()) {
      case Expr::I32Store8:
        if (!c.buffer.append("i32:8"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I64Store8:
        if (!c.buffer.append("i64:8"))
            return false;
        defaultAlignLog2 = 0;
        break;
      case Expr::I32Store16:
        if (!c.buffer.append("i32:16"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Store16:
        if (!c.buffer.append("i64:16"))
            return false;
        defaultAlignLog2 = 1;
        break;
      case Expr::I64Store32:
        if (!c.buffer.append("i64:32"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I32Store:
        if (!c.buffer.append("i32"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::I64Store:
        if (!c.buffer.append("i64"))
            return false;
        defaultAlignLog2 = 3;
        break;
      case Expr::F32Store:
        if (!c.buffer.append("f32"))
            return false;
        defaultAlignLog2 = 2;
        break;
      case Expr::F64Store:
        if (!c.buffer.append("f64"))
            return false;
        defaultAlignLog2 = 3;
        break;
      default:
        return false;
    }

    if (!PrintLoadStoreAddress(c, store.address(), defaultAlignLog2))
        return false;

    if (!c.buffer.append(" = "))
        return false;

    if (!PrintExpr(c, store.value()))
        return false;

    if (!c.f.reduceParens || lastPrecedence > StoreOperatorPrecedence) {
        if (!c.buffer.append(")"))
            return false;
    }

    c.currentPrecedence = lastPrecedence;
    return true;
}

static bool
PrintBranch(WasmPrintContext& c, AstBranch& branch)
{
    Expr expr = branch.expr();
    MOZ_ASSERT(expr == Expr::BrIf || expr == Expr::Br);

    if (expr == Expr::BrIf ? !c.buffer.append("br_if ") : !c.buffer.append("br "))
        return false;

    if (expr == Expr::BrIf || branch.maybeValue()) {
        if (!c.buffer.append('('))
            return false;
    }

    if (expr == Expr::BrIf) {
        if (!PrintExpr(c, branch.cond()))
            return false;
    }

    if (branch.maybeValue()) {
        if (!c.buffer.append(", "))
            return false;

        if (!PrintExpr(c, *(branch.maybeValue())))
            return false;
    }

    if (expr == Expr::BrIf || branch.maybeValue()) {
        if (!c.buffer.append(") "))
            return false;
    }

    if (!PrintRef(c, branch.target()))
        return false;

    return true;
}

static bool
PrintBrTable(WasmPrintContext& c, AstBranchTable& table)
{
    if (!c.buffer.append("br_table "))
        return false;

    if (!c.buffer.append('('))
        return false;

    // Index
    if (!PrintExpr(c, table.index()))
        return false;

    if (table.maybeValue()) {
      if (!c.buffer.append(", "))
          return false;

      if (!PrintExpr(c, *(table.maybeValue())))
          return false;
    }

    if (!c.buffer.append(") "))
        return false;

    uint32_t tableLength = table.table().length();
    if (tableLength > 0) {
        if (!c.buffer.append("["))
            return false;
        for (uint32_t i = 0; i < tableLength; i++) {
            if (!PrintRef(c, table.table()[i]))
                return false;
            if (i + 1 == tableLength)
                break;
            if (!c.buffer.append(", "))
                return false;
        }
        if (!c.buffer.append("], "))
            return false;
    }

    if (!PrintRef(c, table.def()))
        return false;

    return true;
}

static bool
PrintReturn(WasmPrintContext& c, AstReturn& ret)
{
    if (!c.buffer.append("return"))
        return false;

    if (ret.maybeExpr()) {
        if (!c.buffer.append(" "))
            return false;
        if (!PrintExpr(c, *(ret.maybeExpr())))
            return false;
    }

    return true;
}

static bool
PrintExpr(WasmPrintContext& c, AstExpr& expr)
{
    if (c.maybeSourceMap) {
        uint32_t lineno = c.buffer.lineno();
        uint32_t column = c.buffer.column();
        if (!c.maybeSourceMap->exprlocs().emplaceBack(lineno, column, expr.offset()))
            return false;
    }

    switch (expr.kind()) {
      case AstExprKind::NullaryOperator:
        return PrintNullaryOperator(c, expr.as<AstNullaryOperator>());
      case AstExprKind::Unreachable:
        return PrintUnreachable(c, expr.as<AstUnreachable>());
      case AstExprKind::Call:
        return PrintCall(c, expr.as<AstCall>());
      case AstExprKind::CallIndirect:
        return PrintCallIndirect(c, expr.as<AstCallIndirect>());
      case AstExprKind::Const:
        return PrintConst(c, expr.as<AstConst>());
      case AstExprKind::GetLocal:
        return PrintGetLocal(c, expr.as<AstGetLocal>());
      case AstExprKind::SetLocal:
        return PrintSetLocal(c, expr.as<AstSetLocal>());
      case AstExprKind::Block:
        return PrintBlock(c, expr.as<AstBlock>());
      case AstExprKind::If:
        return PrintIf(c, expr.as<AstIf>());
      case AstExprKind::UnaryOperator:
        return PrintUnaryOperator(c, expr.as<AstUnaryOperator>());
      case AstExprKind::BinaryOperator:
        return PrintBinaryOperator(c, expr.as<AstBinaryOperator>());
      case AstExprKind::TernaryOperator:
        return PrintTernaryOperator(c, expr.as<AstTernaryOperator>());
      case AstExprKind::ComparisonOperator:
        return PrintComparisonOperator(c, expr.as<AstComparisonOperator>());
      case AstExprKind::ConversionOperator:
        return PrintConversionOperator(c, expr.as<AstConversionOperator>());
      case AstExprKind::Load:
        return PrintLoad(c, expr.as<AstLoad>());
      case AstExprKind::Store:
        return PrintStore(c, expr.as<AstStore>());
      case AstExprKind::Branch:
        return PrintBranch(c, expr.as<AstBranch>());
      case AstExprKind::BranchTable:
        return PrintBrTable(c, expr.as<AstBranchTable>());
      case AstExprKind::Return:
        return PrintReturn(c, expr.as<AstReturn>());
      default:
        // Note: it's important not to remove this default since readExpr()
        // can return Expr values for which there is no enumerator.
        break;
    }

    return false;
}

static bool
PrintSignature(WasmPrintContext& c, const AstSig& sig, const AstNameVector* maybeLocals = nullptr)
{
    uint32_t paramsNum = sig.args().length();

    if (!c.buffer.append("("))
        return false;
    if (maybeLocals) {
      for (uint32_t i = 0; i < paramsNum; i++) {
          const AstName& name = (*maybeLocals)[i];
          if (!name.empty()) {
              if (!PrintName(c, name))
                  return false;
              if (!c.buffer.append(": "))
                  return false;
          }
          ValType arg = sig.args()[i];
          if (!PrintValType(c, arg))
              return false;
          if (i + 1 == paramsNum)
              break;
          if (!c.buffer.append(", "))
              return false;
      }
    } else if (paramsNum > 0) {
      for (uint32_t i = 0; i < paramsNum; i++) {
          ValType arg = sig.args()[i];
          if (!PrintValType(c, arg))
              return false;
          if (i + 1 == paramsNum)
              break;
          if (!c.buffer.append(", "))
              return false;
      }
    }
    if (!c.buffer.append(") : ("))
        return false;
    if (sig.ret() != ExprType::Void) {
        if (!PrintExprType(c, sig.ret()))
            return false;
    }
    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
PrintTypeSection(WasmPrintContext& c, const AstModule::SigVector& sigs)
{
    uint32_t numSigs = sigs.length();
    if (!numSigs)
        return true;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        const AstSig* sig = sigs[sigIndex];
        if (!PrintIndent(c))
            return false;
        if (!c.buffer.append("type "))
            return false;
        if (!sig->name().empty()) {
          if (!PrintName(c, sig->name()))
              return false;
          if (!c.buffer.append(" of "))
              return false;
        }
        if (!c.buffer.append("function "))
            return false;
        if (!PrintSignature(c, *sig))
            return false;
        if (!c.buffer.append(";\n"))
            return false;
    }

    if (!c.buffer.append("\n"))
        return false;

    return true;
}

static bool
PrintTableSection(WasmPrintContext& c, const AstModule& module)
{
    if (module.elemSegments().empty())
        return true;

    const AstElemSegment& segment = *module.elemSegments()[0];

    if (!c.buffer.append("table ["))
        return false;

    for (uint32_t i = 0; i < segment.elems().length(); i++) {
        const AstRef& elem = segment.elems()[i];
        AstFunc* func = module.funcs()[elem.index()];
        if (func->name().empty()) {
            if (!PrintInt32(c, elem.index()))
                return false;
        } else {
          if (!PrintName(c, func->name()))
              return false;
        }
        if (i + 1 == segment.elems().length())
            break;
        if (!c.buffer.append(", "))
            return false;
    }

    if (!c.buffer.append("];\n\n"))
        return false;

    return true;
}

static bool
PrintImport(WasmPrintContext& c, AstImport& import, const AstModule::SigVector& sigs)
{
    const AstSig* sig = sigs[import.funcSig().index()];
    if (!PrintIndent(c))
        return false;
    if (!c.buffer.append("import "))
        return false;
    if (!c.buffer.append("\""))
        return false;

    const AstName& fieldName = import.field();
    if (!PrintEscapedString(c, fieldName))
        return false;

    if (!c.buffer.append("\" as "))
        return false;

    if (!PrintName(c, import.name()))
        return false;

    if (!c.buffer.append(" from \""))
        return false;

    const AstName& moduleName = import.module();
    if (!PrintEscapedString(c, moduleName))
        return false;

    if (!c.buffer.append("\" typeof function "))
        return false;

    if (!PrintSignature(c, *sig))
        return false;
    if (!c.buffer.append(";\n"))
        return false;

    return true;
}


static bool
PrintImportSection(WasmPrintContext& c, const AstModule::ImportVector& imports, const AstModule::SigVector& sigs)
{
    uint32_t numImports = imports.length();

    for (uint32_t i = 0; i < numImports; i++) {
        if (!PrintImport(c, *imports[i], sigs))
            return false;
    }

    if (numImports) {
      if (!c.buffer.append("\n"))
          return false;
    }

    return true;
}

static bool
PrintExport(WasmPrintContext& c, AstExport& export_, const AstModule::FuncVector& funcs)
{
    if (!PrintIndent(c))
        return false;
    if (!c.buffer.append("export "))
        return false;
    if (export_.kind() == DefinitionKind::Memory) {
        if (!c.buffer.append("memory"))
          return false;
    } else {
        const AstFunc* func = funcs[export_.ref().index()];
        if (func->name().empty()) {
            if (!PrintInt32(c, export_.ref().index()))
                return false;
        } else {
            if (!PrintName(c, func->name()))
                return false;
        }
    }
    if (!c.buffer.append(" as \""))
        return false;
    if (!PrintEscapedString(c, export_.name()))
        return false;
    if (!c.buffer.append("\";\n"))
        return false;

    return true;
}

static bool
PrintExportSection(WasmPrintContext& c, const AstModule::ExportVector& exports, const AstModule::FuncVector& funcs)
{
    uint32_t numExports = exports.length();
    for (uint32_t i = 0; i < numExports; i++) {
        if (!PrintExport(c, *exports[i], funcs))
            return false;
    }
    if (numExports) {
      if (!c.buffer.append("\n"))
          return false;
    }
    return true;
}

static bool
PrintFunctionBody(WasmPrintContext& c, AstFunc& func, const AstModule::SigVector& sigs)
{
    const AstSig* sig = sigs[func.sig().index()];
    c.indent++;

    size_t startExprIndex = c.maybeSourceMap ? c.maybeSourceMap->exprlocs().length() : 0;
    uint32_t startLineno = c.buffer.lineno();

    uint32_t argsNum = sig->args().length();
    uint32_t localsNum = func.vars().length();
    if (localsNum > 0) {
        if (!PrintIndent(c))
            return false;
        if (!c.buffer.append("var "))
            return false;
        for (uint32_t i = 0; i < localsNum; i++) {
            const AstName& name = func.locals()[argsNum + i];
            if (!name.empty()) {
              if (!PrintName(c, name))
                  return false;
              if (!c.buffer.append(": "))
                  return false;
            }
            ValType local = func.vars()[i];
            if (!PrintValType(c, local))
                return false;
            if (i + 1 == localsNum)
                break;
            if (!c.buffer.append(", "))
                return false;
        }
        if (!c.buffer.append(";\n"))
            return false;
    }


    uint32_t exprsNum = func.body().length();
    for (uint32_t i = 0; i < exprsNum; i++) {
      if (!PrintBlockLevelExpr(c, *func.body()[i], i + 1 == exprsNum))
          return false;
    }

    c.indent--;

    size_t endExprIndex = c.maybeSourceMap ? c.maybeSourceMap->exprlocs().length() : 0;
    uint32_t endLineno = c.buffer.lineno();

    if (c.maybeSourceMap) {
        if (!c.maybeSourceMap->functionlocs().emplaceBack(startExprIndex, endExprIndex, startLineno, endLineno))
            return false;
    }
    return true;
}

static bool
PrintCodeSection(WasmPrintContext& c, const AstModule::FuncVector& funcs, const AstModule::SigVector& sigs)
{
    uint32_t numFuncBodies = funcs.length();
    for (uint32_t funcIndex = 0; funcIndex < numFuncBodies; funcIndex++) {
        AstFunc* func = funcs[funcIndex];
        uint32_t sigIndex = func->sig().index();
        AstSig* sig = sigs[sigIndex];

        if (!PrintIndent(c))
            return false;
        if (!c.buffer.append("function "))
            return false;
        if (!func->name().empty()) {
          if (!PrintName(c, func->name()))
              return false;
        }

        if (!PrintSignature(c, *sig, &(func->locals())))
            return false;
        if (!c.buffer.append(" {\n"))
            return false;

        c.currentFuncIndex = funcIndex;

        if (!PrintFunctionBody(c, *func, sigs))
            return false;

        if (!PrintIndent(c))
            return false;
        if (!c.buffer.append("}\n\n"))
            return false;
    }

   return true;
}


static bool
PrintDataSection(WasmPrintContext& c, const AstModule& module)
{
    if (!module.hasMemory())
        return true;

    if (!PrintIndent(c))
        return false;
    if (!c.buffer.append("memory "))
        return false;
    if (!PrintInt32(c, module.memory().initial()))
       return false;
    if (module.memory().maximum()) {
        if (!c.buffer.append(", "))
            return false;
        if (!PrintInt32(c, *module.memory().maximum()))
            return false;
    }

    c.indent++;

    uint32_t numSegments = module.dataSegments().length();
    if (!numSegments) {
      if (!c.buffer.append(" {}\n\n"))
          return false;
      return true;
    }
    if (!c.buffer.append(" {\n"))
        return false;

    for (uint32_t i = 0; i < numSegments; i++) {
        const AstDataSegment* segment = module.dataSegments()[i];

        if (!PrintIndent(c))
            return false;
        if (!c.buffer.append("segment "))
           return false;
        if (!PrintInt32(c, segment->offset()->as<AstConst>().val().i32()))
           return false;
        if (!c.buffer.append(" \""))
           return false;

        PrintEscapedString(c, segment->text());

        if (!c.buffer.append("\";\n"))
           return false;
    }

    c.indent--;
    if (!c.buffer.append("}\n\n"))
        return false;

    return true;
}

static bool
PrintModule(WasmPrintContext& c, AstModule& module)
{
    if (!PrintTypeSection(c, module.sigs()))
        return false;

    if (!PrintImportSection(c, module.imports(), module.sigs()))
        return false;

    if (!PrintTableSection(c, module))
        return false;

    if (!PrintExportSection(c, module.exports(), module.funcs()))
        return false;

    if (!PrintCodeSection(c, module.funcs(), module.sigs()))
        return false;

    if (!PrintDataSection(c, module))
        return false;

    return true;
}

/*****************************************************************************/
// Top-level functions

bool
wasm::BinaryToExperimentalText(JSContext* cx, const uint8_t* bytes, size_t length,
                               StringBuffer& buffer, const ExperimentalTextFormatting& formatting,
                               GeneratedSourceMap* sourceMap)
{

    LifoAlloc lifo(AST_LIFO_DEFAULT_CHUNK_SIZE);

    AstModule* module;
    if (!BinaryToAst(cx, bytes, length, lifo, &module))
        return false;

    WasmPrintBuffer buf(buffer);
    WasmPrintContext c(cx, module, buf, formatting, sourceMap);

    if (!PrintModule(c, *module)) {
        if (!cx->isExceptionPending())
            ReportOutOfMemory(cx);
        return false;
    }

    return true;
}


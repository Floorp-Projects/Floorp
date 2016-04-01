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

#include "mozilla/CheckedInt.h"

#include "jsnum.h"
#include "jsprf.h"

#include "asmjs/Wasm.h"
#include "asmjs/WasmTypes.h"
#include "vm/ArrayBufferObject.h"
#include "vm/StringBuffer.h"

using namespace js;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::IsInfinite;
using mozilla::IsNaN;
using mozilla::IsNegativeZero;

struct WasmRenderContext
{
    JSContext* cx;
    Decoder& d;
    StringBuffer& buffer;
    uint32_t indent;

    DeclaredSigVector signatures;
    Uint32Vector funcSigs;
    Uint32Vector funcLocals;
    Uint32Vector importSigs;

    uint32_t currentFuncIndex;

    WasmRenderContext(JSContext* cx, Decoder& d, StringBuffer& buffer)
      : cx(cx), d(d), buffer(buffer), indent(0), currentFuncIndex(0)
    {}
};

/*****************************************************************************/
// utilities

static bool
RenderFail(WasmRenderContext& c, const char* str)
{
    uint32_t offset = c.d.currentOffset();
    char offsetStr[sizeof "4294967295"];
    JS_snprintf(offsetStr, sizeof offsetStr, "%" PRIu32, offset);
    JS_ReportErrorNumber(c.cx, GetErrorMessage, nullptr, JSMSG_WASM_DECODE_FAIL, offsetStr, str);
    return false;
}

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

    int64_t abs = mozilla::Abs(num);
    int64_t n = abs;
    uint64_t pow = 1;
    while (n) {
        pow *= 10;
        n /= 10;
    }
    pow /= 10;

    n = abs;
    while (pow) {
        if (!c.buffer.append("0123456789"[n / pow]))
            return false;
        n -= (n / pow) * pow;
        pow /= 10;
    }

    return true;
}

static bool
RenderDouble(WasmRenderContext& c, double num)
{
    if (IsNegativeZero(num))
        return c.buffer.append("-0");
    if (IsNaN(num))
        return c.buffer.append("nan");
    if (IsInfinite(num)) {
        if (num > 0)
            return c.buffer.append("infinity");
        return c.buffer.append("-infinity");
    }
    return NumberValueToStringBuffer(c.cx, DoubleValue(num), c.buffer);
}

static bool
RenderString(WasmRenderContext& c, const uint8_t* s, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        uint8_t byte = s[i];
        bool success;
        switch (byte) {
            case '\n': success = c.buffer.append("\\n"); break;
            case '\r': success = c.buffer.append("\\0d"); break;
            case '\t': success = c.buffer.append("\\t"); break;
            case '\f': success = c.buffer.append("\\0c"); break;
            case '\b': success = c.buffer.append("\\08"); break;
            case '\\': success = c.buffer.append("\\\\"); break;
            case '"' : success = c.buffer.append("\\\""); break;
            case '\'' : success = c.buffer.append("\\'"); break;
            default: {
                if (byte >= 32 && byte < 127) {
                    success = c.buffer.append((char)byte);
                } else {
                  char digit1 = byte / 16, digit2 = byte % 16;
                  success = c.buffer.append("\\") &&
                            c.buffer.append((char)(digit1 < 10 ? digit1 + '0' : digit1 + 'a' - 10)) &&
                            c.buffer.append((char)(digit2 < 10 ? digit2 + '0' : digit2 + 'a' - 10));
                }
            }
        }
        if (!success)
            return false;
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
      default: /* we don't care about asm.js types */ return false;
    }
}

static bool
RenderValType(WasmRenderContext& c, ValType type)
{
    return RenderExprType(c, ToExprType(type));
}

static bool
RenderExpr(WasmRenderContext& c);

static bool
RenderFullLine(WasmRenderContext& c)
{
    if (!RenderIndent(c))
        return false;
    if (!RenderExpr(c))
        return false;
    return c.buffer.append('\n');
}

/*****************************************************************************/
// binary format parsing and rendering

static bool
RenderNop(WasmRenderContext& c)
{
    return c.buffer.append("(nop)");
}

static bool
RenderUnreachable(WasmRenderContext& c)
{
    return c.buffer.append("(trap)");
}

static bool
RenderCallWithSig(WasmRenderContext& c, uint32_t sigIndex)
{
    const DeclaredSig& sig = c.signatures[sigIndex];
    for (uint32_t i = 0; i < sig.args().length(); i++) {
        if (!c.buffer.append(" "))
            return false;
        if (!RenderExpr(c))
            return false;
    }

    return true;
}

static bool
RenderCall(WasmRenderContext& c)
{
    uint32_t funcIndex;
    if (!c.d.readVarU32(&funcIndex))
        return RenderFail(c, "unable to read import index");

    if (!c.buffer.append("(call $func$"))
        return false;
    if (!RenderInt32(c, funcIndex))
        return false;

    if (funcIndex >= c.funcSigs.length())
        return RenderFail(c, "callee index out of range");

    uint32_t sigIndex = c.funcSigs[funcIndex];
    if (!RenderCallWithSig(c, sigIndex))
        return false;
    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderCallImport(WasmRenderContext& c)
{
    uint32_t importIndex;
    if (!c.d.readVarU32(&importIndex))
        return RenderFail(c, "unable to read import index");

    if (!c.buffer.append("(call_import $import$"))
        return false;
    if (!RenderInt32(c, importIndex))
        return false;

    if (importIndex >= c.importSigs.length())
        return RenderFail(c, "import index out of range");

    uint32_t sigIndex = c.importSigs[importIndex];
    if (!RenderCallWithSig(c, sigIndex))
        return false;
    if (!c.buffer.append(")"))
        return false;

    return true;
}

static bool
RenderCallIndirect(WasmRenderContext& c)
{
    uint32_t sigIndex;
    if (!c.d.readVarU32(&sigIndex))
        return RenderFail(c, "unable to read indirect call signature index");

    if (!c.buffer.append("(call_indirect $type$"))
        return false;
    if (!RenderInt32(c, sigIndex))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderCallWithSig(c, sigIndex))
        return false;
    if (!c.buffer.append(")"))
        return false;

    return true;
}

static bool
RenderConstI32(WasmRenderContext& c)
{
    if (!c.buffer.append("(i32.const "))
        return false;

    int32_t i32;
    if (!c.d.readVarS32(&i32))
        return RenderFail(c, "unable to read i32.const immediate");

    if (!RenderInt32(c, (uint32_t)i32))
        return false;

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderConstI64(WasmRenderContext& c)
{
    if (!c.buffer.append("(i64.const "))
        return false;

    int64_t i64;
    if (!c.d.readVarS64(&i64))
        return RenderFail(c, "unable to read i64.const immediate");

    if (!RenderInt64(c, i64))
        return false;

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderConstF32(WasmRenderContext& c)
{
    if (!c.buffer.append("(f32.const "))
        return false;

    float value;
    if (!c.d.readFixedF32(&value))
        return RenderFail(c, "unable to read f32.const immediate");

    if (IsNaN(value)) {
        const float jsNaN = (float)JS::GenericNaN();
        if (memcmp(&value, &jsNaN, sizeof(value)) != 0)
            return RenderFail(c, "NYI: NaN literals with custom payloads");
    }

    if (!RenderDouble(c, (double)value))
        return false;

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderConstF64(WasmRenderContext& c)
{
    if (!c.buffer.append("(f64.const "))
        return false;

    double value;
    if (!c.d.readFixedF64(&value))
        return RenderFail(c, "unable to read f64.const immediate");

    if (IsNaN(value)) {
        const double jsNaN = JS::GenericNaN();
        if (memcmp(&value, &jsNaN, sizeof(value)) != 0)
            return RenderFail(c, "NYI: NaN literals with custom payloads");
    }

    if (!RenderDouble(c, value))
        return false;

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderGetLocal(WasmRenderContext& c)
{
    uint32_t localIndex;
    if (!c.d.readVarU32(&localIndex))
        return RenderFail(c, "unable to read get_local index");

    if (localIndex >= c.funcLocals[c.currentFuncIndex])
        return RenderFail(c, "get_local index out of range");

    if (!c.buffer.append("(get_local $var$"))
        return false;
    if (!RenderInt32(c, localIndex))
        return false;
    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderSetLocal(WasmRenderContext& c)
{
    uint32_t localIndex;
    if (!c.d.readVarU32(&localIndex))
        return RenderFail(c, "unable to read set_local index");

    if (localIndex >= c.funcLocals[c.currentFuncIndex])
        return RenderFail(c, "set_local index out of range");

    if (!c.buffer.append("(set_local $var$"))
        return false;
    if (!RenderInt32(c, localIndex))
        return false;
    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderBlock(WasmRenderContext& c)
{
    if (!c.buffer.append("(block"))
        return false;

    c.indent++;

    uint32_t numExprs;
    if (!c.d.readVarU32(&numExprs))
        return RenderFail(c, "unable to read block's number of expressions");

    for (uint32_t i = 0; i < numExprs; i++) {
        if (!c.buffer.append(" "))
            return false;
        if (!RenderExpr(c))
            return false;
    }

    c.indent--;
    if (!c.buffer.append(")"))
        return false;

    return true;
}

static bool
RenderLoop(WasmRenderContext& c)
{
    if (!c.buffer.append("(loop"))
        return false;

    c.indent++;

    uint32_t numExprs;
    if (!c.d.readVarU32(&numExprs))
        return RenderFail(c, "unable to read block's number of expressions");

    for (uint32_t i = 0; i < numExprs; i++) {
        if (!c.buffer.append(" "))
            return false;
        if (!RenderExpr(c))
            return false;
    }

    c.indent--;
    if (!c.buffer.append(")"))
        return false;

    return true;
}

static bool
RenderUnaryOperator(WasmRenderContext& c, Expr expr, ValType argType)
{
    if (!c.buffer.append("("))
      return false;

    bool success = false;
    switch (expr) {
      case Expr::I32Eqz:     success = c.buffer.append("i32.eqz"); break;
      case Expr::I32Clz:     success = c.buffer.append("i32.clz"); break;
      case Expr::I32Ctz:     success = c.buffer.append("i32.ctz"); break;
      case Expr::I32Popcnt:  success = c.buffer.append("i32.popcnt"); break;
      case Expr::I64Clz:     success = c.buffer.append("i64.clz"); break;
      case Expr::I64Ctz:     success = c.buffer.append("i64.ctz"); break;
      case Expr::I64Popcnt:  success = c.buffer.append("i64.popcnt"); break;
      case Expr::F32Abs:     success = c.buffer.append("f32.abs"); break;
      case Expr::F32Neg:     success = c.buffer.append("f32.neg"); break;
      case Expr::F32Ceil:    success = c.buffer.append("f32.ceil"); break;
      case Expr::F32Floor:   success = c.buffer.append("f32.floor"); break;
      case Expr::F32Sqrt:    success = c.buffer.append("f32.sqrt"); break;
      case Expr::F32Trunc:   success = c.buffer.append("f32.trunc"); break;
      case Expr::F32Nearest: success = c.buffer.append("f32.nearest"); break;
      case Expr::F64Abs:     success = c.buffer.append("f64.abs"); break;
      case Expr::F64Neg:     success = c.buffer.append("f64.neg"); break;
      case Expr::F64Ceil:    success = c.buffer.append("f64.ceil"); break;
      case Expr::F64Floor:   success = c.buffer.append("f64.floor"); break;
      case Expr::F64Sqrt:    success = c.buffer.append("f64.sqrt"); break;
      default: return false;
    }
    if (!success)
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderBinaryOperator(WasmRenderContext& c, Expr expr, ValType argType)
{
    if (!c.buffer.append("("))
      return false;

    bool success = false;
    switch (expr) {
      case Expr::I32Add:      success = c.buffer.append("i32.add"); break;
      case Expr::I32Sub:      success = c.buffer.append("i32.sub"); break;
      case Expr::I32Mul:      success = c.buffer.append("i32.mul"); break;
      case Expr::I32DivS:     success = c.buffer.append("i32.div_s"); break;
      case Expr::I32DivU:     success = c.buffer.append("i32.div_u"); break;
      case Expr::I32RemS:     success = c.buffer.append("i32.rem_s"); break;
      case Expr::I32RemU:     success = c.buffer.append("i32.rem_u"); break;
      case Expr::I32And:      success = c.buffer.append("i32.and"); break;
      case Expr::I32Or:       success = c.buffer.append("i32.or"); break;
      case Expr::I32Xor:      success = c.buffer.append("i32.xor"); break;
      case Expr::I32Shl:      success = c.buffer.append("i32.shl"); break;
      case Expr::I32ShrS:     success = c.buffer.append("i32.shr_s"); break;
      case Expr::I32ShrU:     success = c.buffer.append("i32.shr_u"); break;
      case Expr::I64Add:      success = c.buffer.append("i64.add"); break;
      case Expr::I64Sub:      success = c.buffer.append("i64.sub"); break;
      case Expr::I64Mul:      success = c.buffer.append("i64.mul"); break;
      case Expr::I64DivS:     success = c.buffer.append("i64.div_s"); break;
      case Expr::I64DivU:     success = c.buffer.append("i64.div_u"); break;
      case Expr::I64RemS:     success = c.buffer.append("i64.rem_s"); break;
      case Expr::I64RemU:     success = c.buffer.append("i64.rem_u"); break;
      case Expr::I64And:      success = c.buffer.append("i64.and"); break;
      case Expr::I64Or:       success = c.buffer.append("i64.or"); break;
      case Expr::I64Xor:      success = c.buffer.append("i64.xor"); break;
      case Expr::I64Shl:      success = c.buffer.append("i64.shl"); break;
      case Expr::I64ShrS:     success = c.buffer.append("i64.shr_s"); break;
      case Expr::I64ShrU:     success = c.buffer.append("i64.shr_u"); break;
      case Expr::F32Add:      success = c.buffer.append("f32.add"); break;
      case Expr::F32Sub:      success = c.buffer.append("f32.sub"); break;
      case Expr::F32Mul:      success = c.buffer.append("f32.mul"); break;
      case Expr::F32Div:      success = c.buffer.append("f32.div"); break;
      case Expr::F32Min:      success = c.buffer.append("f32.min"); break;
      case Expr::F32Max:      success = c.buffer.append("f32.max"); break;
      case Expr::F32CopySign: success = c.buffer.append("f32.copysign"); break;
      case Expr::F64Add:      success = c.buffer.append("f64.add"); break;
      case Expr::F64Sub:      success = c.buffer.append("f64.sub"); break;
      case Expr::F64Mul:      success = c.buffer.append("f64.mul"); break;
      case Expr::F64Div:      success = c.buffer.append("f64.div"); break;
      case Expr::F64Min:      success = c.buffer.append("f64.min"); break;
      case Expr::F64Max:      success = c.buffer.append("f64.max"); break;
      default: return false;
    }
    if (!success)
        return false;
    if (!c.buffer.append(" "))
        return false;
    if (!RenderExpr(c))
        return false;
    if (!c.buffer.append(" "))
        return false;
    if (!RenderExpr(c))
        return false;
    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderComparisonOperator(WasmRenderContext& c, Expr expr, ValType argType)
{
    if (!c.buffer.append("("))
      return false;

    bool success = false;
    switch (expr) {
      case Expr::I32Eq:  success = c.buffer.append("i32.eq"); break;
      case Expr::I32Ne:  success = c.buffer.append("i32.ne"); break;
      case Expr::I32LtS: success = c.buffer.append("i32.lt_s"); break;
      case Expr::I32LtU: success = c.buffer.append("i32.lt_u"); break;
      case Expr::I32LeS: success = c.buffer.append("i32.le_s"); break;
      case Expr::I32LeU: success = c.buffer.append("i32.le_u"); break;
      case Expr::I32GtS: success = c.buffer.append("i32.gt_s"); break;
      case Expr::I32GtU: success = c.buffer.append("i32.gt_u"); break;
      case Expr::I32GeS: success = c.buffer.append("i32.ge_s"); break;
      case Expr::I32GeU: success = c.buffer.append("i32.ge_u"); break;
      case Expr::I64Eq:  success = c.buffer.append("i64.eq"); break;
      case Expr::I64Ne:  success = c.buffer.append("i64.ne"); break;
      case Expr::I64LtS: success = c.buffer.append("i64.lt_s"); break;
      case Expr::I64LtU: success = c.buffer.append("i64.lt_u"); break;
      case Expr::I64LeS: success = c.buffer.append("i64.le_s"); break;
      case Expr::I64LeU: success = c.buffer.append("i64.le_u"); break;
      case Expr::I64GtS: success = c.buffer.append("i64.gt_s"); break;
      case Expr::I64GtU: success = c.buffer.append("i64.gt_u"); break;
      case Expr::I64GeS: success = c.buffer.append("i64.ge_s"); break;
      case Expr::I64GeU: success = c.buffer.append("i64.ge_u"); break;
      case Expr::F32Eq:  success = c.buffer.append("f32.eq"); break;
      case Expr::F32Ne:  success = c.buffer.append("f32.ne"); break;
      case Expr::F32Lt:  success = c.buffer.append("f32.lt"); break;
      case Expr::F32Le:  success = c.buffer.append("f32.le"); break;
      case Expr::F32Gt:  success = c.buffer.append("f32.gt"); break;
      case Expr::F32Ge:  success = c.buffer.append("f32.ge"); break;
      case Expr::F64Eq:  success = c.buffer.append("f64.eq"); break;
      case Expr::F64Ne:  success = c.buffer.append("f64.ne"); break;
      case Expr::F64Lt:  success = c.buffer.append("f64.lt"); break;
      case Expr::F64Le:  success = c.buffer.append("f64.le"); break;
      case Expr::F64Gt:  success = c.buffer.append("f64.gt"); break;
      case Expr::F64Ge:  success = c.buffer.append("f64.ge"); break;
      default: return false;
    }
    if (!success)
        return false;

    if (!c.buffer.append(" "))
        return false;
    if (!RenderExpr(c))
        return false;
    if (!c.buffer.append(" "))
        return false;
    if (!RenderExpr(c))
        return false;
    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderConversionOperator(WasmRenderContext& c, Expr expr, ValType to, ValType argType)
{
    if (!c.buffer.append("("))
      return false;

    bool success = false;
    switch (expr) {
      case Expr::I32WrapI64:        success = c.buffer.append("i32.wrap/i64"); break;
      case Expr::I32TruncSF32:      success = c.buffer.append("i32.trunc_s/f32"); break;
      case Expr::I32TruncUF32:      success = c.buffer.append("i32.trunc_u/f32"); break;
      case Expr::I32ReinterpretF32: success = c.buffer.append("i32.reinterpret/f32"); break;
      case Expr::I32TruncSF64:      success = c.buffer.append("i32.trunc_s/f64"); break;
      case Expr::I32TruncUF64:      success = c.buffer.append("i32.trunc_u/f64"); break;
      case Expr::I64ExtendSI32:     success = c.buffer.append("i64.extend_s/i32"); break;
      case Expr::I64ExtendUI32:     success = c.buffer.append("i64.extend_u/i32"); break;
      case Expr::I64TruncSF32:      success = c.buffer.append("i64.trunc_s/f32"); break;
      case Expr::I64TruncUF32:      success = c.buffer.append("i64.trunc_u/f32"); break;
      case Expr::I64TruncSF64:      success = c.buffer.append("i64.trunc_s/f64"); break;
      case Expr::I64TruncUF64:      success = c.buffer.append("i64.trunc_u/f64"); break;
      case Expr::I64ReinterpretF64: success = c.buffer.append("i64.reinterpret/f64"); break;
      case Expr::F32ConvertSI32:    success = c.buffer.append("f32.convert_s/i32"); break;
      case Expr::F32ConvertUI32:    success = c.buffer.append("f32.convert_u/i32"); break;
      case Expr::F32ReinterpretI32: success = c.buffer.append("f32.reinterpret/i32"); break;
      case Expr::F32ConvertSI64:    success = c.buffer.append("f32.convert_s/i64"); break;
      case Expr::F32ConvertUI64:    success = c.buffer.append("f32.convert_u/i64"); break;
      case Expr::F32DemoteF64:      success = c.buffer.append("f32.demote/f64"); break;
      case Expr::F64ConvertSI32:    success = c.buffer.append("f64.convert_s/i32"); break;
      case Expr::F64ConvertUI32:    success = c.buffer.append("f64.convert_u/i32"); break;
      case Expr::F64ConvertSI64:    success = c.buffer.append("f64.convert_s/i64"); break;
      case Expr::F64ConvertUI64:    success = c.buffer.append("f64.convert_u/i64"); break;
      case Expr::F64ReinterpretI64: success = c.buffer.append("f64.reinterpret/i64"); break;
      case Expr::F64PromoteF32:     success = c.buffer.append("f64.promote/f32"); break;
      default: return false;
    }
    if (!success)
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderIfElse(WasmRenderContext& c, bool hasElse)
{
    if (!c.buffer.append("(if "))
        return false;
    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(" "))
        return false;

    c.indent++;
    if (!RenderExpr(c))
        return false;
    c.indent--;

    if (hasElse) {
        if (!c.buffer.append(" "))
            return false;

        c.indent++;
        if (!RenderExpr(c))
            return false;
        c.indent--;
    }

    if (!c.buffer.append(")"))
        return false;

    return true;
}

static bool
RenderLoadStoreAddress(WasmRenderContext& c)
{
    uint32_t flags;
    if (!c.d.readVarU32(&flags))
        return RenderFail(c, "expected memory access flags");

    uint32_t offset;
    if (!c.d.readVarU32(&offset))
        return RenderFail(c, "expected memory access offset");

    if (offset != 0) {
      if (!c.buffer.append(" offset="))
          return false;
      if (!RenderInt32(c, offset))
          return false;
    }

    uint32_t alignLog2 = flags;
    if (!c.buffer.append(" align="))
        return false;
    if (!RenderInt32(c, 1 << alignLog2))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    return true;
}

static bool
RenderLoad(WasmRenderContext& c, Expr expr, ValType loadType)
{
    if (!c.buffer.append("("))
        return false;
    if (!RenderValType(c, loadType))
        return false;
    if (!c.buffer.append(".load"))
        return false;
    switch (expr) {
      case Expr::I32Load8S:
      case Expr::I64Load8S:
        if (!c.buffer.append("8_s"))
            return false;
        break;
      case Expr::I32Load8U:
      case Expr::I64Load8U:
        if (!c.buffer.append("8_u"))
            return false;
        break;
      case Expr::I32Load16S:
      case Expr::I64Load16S:
        if (!c.buffer.append("16_s"))
            return false;
        break;
      case Expr::I32Load16U:
      case Expr::I64Load16U:
        if (!c.buffer.append("16_u"))
            return false;
        break;
      case Expr::I64Load32S:
        if (!c.buffer.append("32_s"))
            return false;
        break;
      case Expr::I64Load32U:
        if (!c.buffer.append("32_u"))
            return false;
        break;
      default: /* rest of them have not prefix */
        break;
    }

    if (!RenderLoadStoreAddress(c))
        return false;

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderStore(WasmRenderContext& c, Expr expr, ValType storeType)
{
    if (!c.buffer.append("("))
        return false;
    if (!RenderValType(c, storeType))
        return false;
    if (!c.buffer.append(".store"))
        return false;
    switch (expr) {
      case Expr::I32Store8:
      case Expr::I64Store8:
        if (!c.buffer.append("8"))
            return false;
        break;
      case Expr::I32Store16:
      case Expr::I64Store16:
        if (!c.buffer.append("16"))
            return false;
        break;
      case Expr::I64Store32:
        if (!c.buffer.append("32"))
            return false;
        break;
      default: /* rest of them have not prefix */
        break;
    }

    if (!RenderLoadStoreAddress(c))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(")"))
        return false;

    return true;
}

static bool
RenderBranch(WasmRenderContext& c, Expr expr)
{
    MOZ_ASSERT(expr == Expr::BrIf || expr == Expr::Br);

    uint32_t relativeDepth;
    if (!c.d.readVarU32(&relativeDepth))
        return RenderFail(c, "expected relative depth");

    if (expr == Expr::BrIf ? !c.buffer.append("(br_if ") : !c.buffer.append("(br "))
        return false;

    if (!RenderInt32(c, relativeDepth))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (expr == Expr::BrIf) {
        if (!c.buffer.append(" "))
            return false;
        if (!RenderExpr(c))
            return false;
    }

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderBrTable(WasmRenderContext& c)
{
    uint32_t tableLength;
    if (!c.d.readVarU32(&tableLength))
        return false;

    if (!c.buffer.append("(br_table "))
        return false;

    for (uint32_t i = 0; i < tableLength; i++) {
        uint32_t depth;
        if (!c.d.readFixedU32(&depth))
            return RenderFail(c, "missing br_table entry");

        if (!RenderInt32(c, depth))
            return false;

        if (!c.buffer.append(" "))
            return false;
    }

    uint32_t defaultDepth;
    if (!c.d.readFixedU32(&defaultDepth))
        return RenderFail(c, "expected default relative depth");

    if (!RenderInt32(c, defaultDepth))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(")"))
        return false;

    return true;
}

static bool
RenderReturn(WasmRenderContext& c)
{
    if (!c.buffer.append("(return"))
        return false;

    if (c.signatures[c.funcSigs[c.currentFuncIndex]].ret() != ExprType::Void) {
        if (!c.buffer.append(" "))
            return false;
        if (!RenderExpr(c))
            return false;
    }

    if (!c.buffer.append(")"))
        return false;
    return true;
}

static bool
RenderSelect(WasmRenderContext& c)
{
    if (!c.buffer.append("(select "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    if (!c.buffer.append(" "))
        return false;

    if (!RenderExpr(c))
        return false;

    return c.buffer.append(")");
}

static bool
RenderExpr(WasmRenderContext& c)
{
    Expr expr;
    if (!c.d.readExpr(&expr))
        return RenderFail(c, "unable to read expression");

    switch (expr) {
      case Expr::Nop:
        return RenderNop(c);
      case Expr::Unreachable:
        return RenderUnreachable(c);
      case Expr::Call:
        return RenderCall(c);
      case Expr::CallImport:
        return RenderCallImport(c);
      case Expr::CallIndirect:
        return RenderCallIndirect(c);
      case Expr::I32Const:
        return RenderConstI32(c);
      case Expr::I64Const:
        return RenderConstI64(c);
      case Expr::F32Const:
        return RenderConstF32(c);
      case Expr::F64Const:
        return RenderConstF64(c);
      case Expr::GetLocal:
        return RenderGetLocal(c);
      case Expr::SetLocal:
        return RenderSetLocal(c);
      case Expr::Block:
        return RenderBlock(c);
      case Expr::Loop:
        return RenderLoop(c);
      case Expr::If:
        return RenderIfElse(c, false);
      case Expr::IfElse:
        return RenderIfElse(c, true);
      case Expr::I32Clz:
      case Expr::I32Ctz:
      case Expr::I32Popcnt:
      case Expr::I32Eqz:
        return RenderUnaryOperator(c, expr, ValType::I32);
      case Expr::I64Clz:
      case Expr::I64Ctz:
      case Expr::I64Popcnt:
        return RenderFail(c, "NYI: i64") &&
               RenderUnaryOperator(c, expr, ValType::I64);
      case Expr::F32Abs:
      case Expr::F32Neg:
      case Expr::F32Ceil:
      case Expr::F32Floor:
      case Expr::F32Sqrt:
        return RenderUnaryOperator(c, expr, ValType::F32);
      case Expr::F32Trunc:
        return RenderFail(c, "NYI: trunc");
      case Expr::F32Nearest:
        return RenderFail(c, "NYI: nearest");
      case Expr::F64Abs:
      case Expr::F64Neg:
      case Expr::F64Ceil:
      case Expr::F64Floor:
      case Expr::F64Sqrt:
        return RenderUnaryOperator(c, expr, ValType::F64);
      case Expr::F64Trunc:
        return RenderFail(c, "NYI: trunc");
      case Expr::F64Nearest:
        return RenderFail(c, "NYI: nearest");
      case Expr::I32Add:
      case Expr::I32Sub:
      case Expr::I32Mul:
      case Expr::I32DivS:
      case Expr::I32DivU:
      case Expr::I32RemS:
      case Expr::I32RemU:
      case Expr::I32And:
      case Expr::I32Or:
      case Expr::I32Xor:
      case Expr::I32Shl:
      case Expr::I32ShrS:
      case Expr::I32ShrU:
        return RenderBinaryOperator(c, expr, ValType::I32);
      case Expr::I64Add:
      case Expr::I64Sub:
      case Expr::I64Mul:
      case Expr::I64DivS:
      case Expr::I64DivU:
      case Expr::I64RemS:
      case Expr::I64RemU:
      case Expr::I64And:
      case Expr::I64Or:
      case Expr::I64Xor:
      case Expr::I64Shl:
      case Expr::I64ShrS:
      case Expr::I64ShrU:
        return RenderBinaryOperator(c, expr, ValType::I64);
      case Expr::F32Add:
      case Expr::F32Sub:
      case Expr::F32Mul:
      case Expr::F32Div:
      case Expr::F32Min:
      case Expr::F32Max:
        return RenderBinaryOperator(c, expr, ValType::F32);
      case Expr::F32CopySign:
        return RenderFail(c, "NYI: copysign");
      case Expr::F64Add:
      case Expr::F64Sub:
      case Expr::F64Mul:
      case Expr::F64Div:
      case Expr::F64Min:
      case Expr::F64Max:
        return RenderBinaryOperator(c, expr, ValType::F64);
      case Expr::F64CopySign:
        return RenderFail(c, "NYI: copysign");
      case Expr::I32Eq:
      case Expr::I32Ne:
      case Expr::I32LtS:
      case Expr::I32LtU:
      case Expr::I32LeS:
      case Expr::I32LeU:
      case Expr::I32GtS:
      case Expr::I32GtU:
      case Expr::I32GeS:
      case Expr::I32GeU:
        return RenderComparisonOperator(c, expr, ValType::I32);
      case Expr::I64Eq:
      case Expr::I64Ne:
      case Expr::I64LtS:
      case Expr::I64LtU:
      case Expr::I64LeS:
      case Expr::I64LeU:
      case Expr::I64GtS:
      case Expr::I64GtU:
      case Expr::I64GeS:
      case Expr::I64GeU:
        return RenderComparisonOperator(c, expr, ValType::I64);
      case Expr::F32Eq:
      case Expr::F32Ne:
      case Expr::F32Lt:
      case Expr::F32Le:
      case Expr::F32Gt:
      case Expr::F32Ge:
        return RenderComparisonOperator(c, expr, ValType::F32);
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Lt:
      case Expr::F64Le:
      case Expr::F64Gt:
      case Expr::F64Ge:
        return RenderComparisonOperator(c, expr, ValType::F64);
      case Expr::I32WrapI64:
        return RenderConversionOperator(c, expr, ValType::I32, ValType::I64);
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
      case Expr::I32ReinterpretF32:
        return RenderConversionOperator(c, expr, ValType::I32, ValType::F32);
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        return RenderConversionOperator(c, expr, ValType::I32, ValType::F64);
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
        return RenderConversionOperator(c, expr, ValType::I64, ValType::I32);
      case Expr::I64TruncSF32:
      case Expr::I64TruncUF32:
        return RenderConversionOperator(c, expr, ValType::I64, ValType::F32);
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF64:
      case Expr::I64ReinterpretF64:
        return RenderConversionOperator(c, expr, ValType::I64, ValType::F64);
      case Expr::F32ConvertSI32:
      case Expr::F32ConvertUI32:
      case Expr::F32ReinterpretI32:
        return RenderConversionOperator(c, expr, ValType::F32, ValType::I32);
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
        return RenderConversionOperator(c, expr, ValType::F32, ValType::I64);
      case Expr::F32DemoteF64:
        return RenderConversionOperator(c, expr, ValType::F32, ValType::I64);
      case Expr::F64ConvertSI32:
      case Expr::F64ConvertUI32:
        return RenderConversionOperator(c, expr, ValType::F64, ValType::I32);
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
      case Expr::F64ReinterpretI64:
        return RenderConversionOperator(c, expr, ValType::F64, ValType::I64);
      case Expr::F64PromoteF32:
        return RenderConversionOperator(c, expr, ValType::F64, ValType::F32);
      case Expr::I32Load8S:
      case Expr::I32Load8U:
      case Expr::I32Load16S:
      case Expr::I32Load16U:
      case Expr::I32Load:
        return RenderLoad(c, expr, ValType::I32);
      case Expr::I64Load:
      case Expr::I64Load8S:
      case Expr::I64Load8U:
      case Expr::I64Load16S:
      case Expr::I64Load16U:
      case Expr::I64Load32S:
      case Expr::I64Load32U:
        return RenderFail(c, "NYI: i64") &&
               RenderLoad(c, expr, ValType::I64);
      case Expr::F32Load:
        return RenderLoad(c, expr, ValType::F32);
      case Expr::F64Load:
        return RenderLoad(c, expr, ValType::F64);
      case Expr::I32Store8:
        return RenderStore(c, expr, ValType::I32);
      case Expr::I32Store16:
        return RenderStore(c, expr, ValType::I32);
      case Expr::I32Store:
        return RenderStore(c, expr, ValType::I32);
      case Expr::I64Store:
      case Expr::I64Store8:
      case Expr::I64Store16:
      case Expr::I64Store32:
        return RenderFail(c, "NYI: i64") &&
               RenderStore(c, expr, ValType::I64);
      case Expr::F32Store:
        return RenderStore(c, expr, ValType::F32);
      case Expr::F64Store:
        return RenderStore(c, expr, ValType::F64);
      case Expr::Br:
        return RenderBranch(c, expr);
      case Expr::BrIf:
        return RenderBranch(c, expr);
      case Expr::BrTable:
        return RenderBrTable(c);
      case Expr::Return:
        return RenderReturn(c);
      case Expr::Select:
        return RenderSelect(c);
      default:
        // Note: it's important not to remove this default since readExpr()
        // can return Expr values for which there is no enumerator.
        break;
    }

    return RenderFail(c, "bad expression code");
}

static bool
RenderSignature(WasmRenderContext& c, const DeclaredSig& sig, bool varAssignment)
{
    uint32_t paramsNum = sig.args().length();

    if (varAssignment) {
      for (uint32_t i = 0; i < paramsNum; i++) {
          if (!c.buffer.append(" (param "))
              return false;
          if (!c.buffer.append("$var$"))
              return false;
          if (!RenderInt32(c, i))
              return false;
          if (!c.buffer.append(" "))
              return false;
          ValType arg = sig.arg(i);
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
          ValType arg = sig.arg(i);
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
RenderSignatures(WasmRenderContext& c)
{
    uint32_t sectionStart;
    if (!c.d.startSection(SignaturesId, &sectionStart))
        return RenderFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSigs;
    if (!c.d.readVarU32(&numSigs))
        return RenderFail(c, "expected number of signatures");

    if (!c.signatures.resize(numSigs))
        return false;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t numArgs;
        if (!c.d.readVarU32(&numArgs))
            return RenderFail(c, "bad number of signature args");

        ValTypeVector args;
        if (!args.resize(numArgs))
            return false;

        ExprType result;
        if (!c.d.readExprType(&result))
            return RenderFail(c, "bad expression type");

        for (uint32_t i = 0; i < numArgs; i++) {
            ValType arg;
            if (!c.d.readValType(&arg))
                return RenderFail(c, "bad value type");
            args[i] = arg;
        }

        c.signatures[sigIndex] = Sig(Move(args), result);

        if (!RenderIndent(c))
            return false;
        if (!c.buffer.append("(type $type$"))
            return false;
        if (!RenderInt32(c, sigIndex))
            return false;
        if (!c.buffer.append(" (func"))
            return false;
        if (!RenderSignature(c, c.signatures[sigIndex], false))
            return false;
        if (!c.buffer.append("))\n"))
            return false;
    }

    if (!c.d.finishSection(sectionStart))
        return RenderFail(c, "decls section byte size mismatch");

    return true;
}

static bool
RenderFunctionSignatures(WasmRenderContext& c)
{
    uint32_t sectionStart;
    if (!c.d.startSection(FunctionSignaturesId, &sectionStart))
        return RenderFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numDecls;
    if (!c.d.readVarU32(&numDecls))
        return RenderFail(c, "expected number of declarations");

    if (!c.funcSigs.resize(numDecls))
        return false;

    for (uint32_t i = 0; i < numDecls; i++) {
        uint32_t sigIndex;
        if (!c.d.readVarU32(&sigIndex))
            return RenderFail(c, "expected signature index");
        c.funcSigs[i] = sigIndex;
    }

    if (!c.d.finishSection(sectionStart))
        return RenderFail(c, "decls section byte size mismatch");

    return true;
}

static bool
RenderFunctionTable(WasmRenderContext& c)
{
    uint32_t sectionStart;
    if (!c.d.startSection(FunctionTableId, &sectionStart))
        return RenderFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numTableElems;
    if (!c.d.readVarU32(&numTableElems))
        return RenderFail(c, "expected number of table elems");

    if (!c.buffer.append("(table "))
        return false;

    for (uint32_t i = 0; i < numTableElems; i++) {
        uint32_t funcIndex;
        if (!c.d.readVarU32(&funcIndex))
            return RenderFail(c, "expected table element");
        if (!RenderInt32(c, funcIndex))
            return false;
        if (!c.buffer.append(" "))
            return false;
    }

    if (!c.buffer.append(")"))
        return false;

    if (!c.d.finishSection(sectionStart))
        return RenderFail(c, "table section byte size mismatch");

    return true;
}

static bool
RenderImport(WasmRenderContext& c, uint32_t importIndex)
{
    uint32_t sigIndex;
    if (!c.d.readVarU32(&sigIndex))
        return RenderFail(c, "expected signature index");

    c.importSigs[importIndex] = sigIndex;

    if (!RenderIndent(c))
        return false;
    if (!c.buffer.append("(import $import$"))
        return false;
    if (!RenderInt32(c, importIndex))
        return false;
    if (!c.buffer.append(" \""))
        return false;

    Bytes moduleName;
    if (!c.d.readBytes(&moduleName))
        return RenderFail(c, "expected import module name");

    if (moduleName.empty())
        return RenderFail(c, "module name cannot be empty");

    if (!RenderString(c, moduleName.begin(), moduleName.length()))
        return false;

    if (!c.buffer.append("\" \""))
        return false;

    Bytes funcName;
    if (!c.d.readBytes(&funcName))
        return RenderFail(c, "expected import func name");

    if (!RenderString(c, funcName.begin(), funcName.length()))
        return false;

    if (!c.buffer.append("\""))
        return false;

    if (!RenderSignature(c, c.signatures[sigIndex], false))
        return false;
    if (!c.buffer.append(")\n"))
        return false;

    return true;
}


static bool
RenderImportTable(WasmRenderContext& c)
{
    uint32_t sectionStart;
    if (!c.d.startSection(ImportTableId, &sectionStart))
        return RenderFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numImports;
    if (!c.d.readVarU32(&numImports))
        return RenderFail(c, "failed to read number of imports");

    if (!c.importSigs.resize(numImports))
        return false;

    for (uint32_t i = 0; i < numImports; i++) {
        if (!RenderImport(c, i))
            return false;
    }

    if (!c.d.finishSection(sectionStart))
        return RenderFail(c, "import section byte size mismatch");

    return true;
}

static bool
RenderMemory(WasmRenderContext& c, uint32_t* memInitial, uint32_t* memMax)
{
    *memInitial = 0;
    *memMax = 0;

    uint32_t sectionStart;
    if (!c.d.startSection(MemoryId, &sectionStart))
        return RenderFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t initialSizePages;
    if (!c.d.readVarU32(&initialSizePages))
        return RenderFail(c, "expected initial memory size");
    *memInitial = initialSizePages;

    uint32_t maxSizePages;
    if (!c.d.readVarU32(&maxSizePages))
        return RenderFail(c, "expected initial memory size");
    *memMax = maxSizePages;

    uint8_t exported;
    if (!c.d.readFixedU8(&exported))
        return RenderFail(c, "expected exported byte");

    if (!c.d.finishSection(sectionStart))
        return RenderFail(c, "memory section byte size mismatch");

    if (exported && !c.buffer.append("(export \"memory\" memory)"))
        return false;

    return true;
}

static bool
RenderExportName(WasmRenderContext& c)
{
    Bytes fieldBytes;
    if (!c.d.readBytes(&fieldBytes)) {
        RenderFail(c, "expected export name");
        return false;
    }

    if (!RenderString(c, fieldBytes.begin(), fieldBytes.length()))
        return false;

    return true;
}

static bool
RenderFunctionExport(WasmRenderContext& c)
{
    uint32_t funcIndex;
    if (!c.d.readVarU32(&funcIndex))
        return RenderFail(c, "expected export internal index");

    if (funcIndex >= c.funcSigs.length())
        return RenderFail(c, "export function index out of range");

    if (!RenderIndent(c))
        return false;
    if (!c.buffer.append("(export \""))
        return false;
    if (!RenderExportName(c))
        return false;
    if (!c.buffer.append("\" "))
        return false;
    if (!c.buffer.append("$func$"))
        return false;
    if (!RenderInt32(c, funcIndex))
        return false;
    if (!c.buffer.append(")\n"))
        return false;

    return true;
}

static bool
RenderExportTable(WasmRenderContext& c)
{
    uint32_t sectionStart;
    if (!c.d.startSection(ExportTableId, &sectionStart))
        return RenderFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numExports;
    if (!c.d.readVarU32(&numExports))
        return RenderFail(c, "failed to read number of exports");

    for (uint32_t i = 0; i < numExports; i++) {
        if (!RenderFunctionExport(c))
            return false;
    }

    if (!c.d.finishSection(sectionStart))
        return RenderFail(c, "export section byte size mismatch");

    return true;
}

static bool
RenderFunctionBody(WasmRenderContext& c, uint32_t funcIndex, uint32_t paramsNum)
{
    uint32_t bodySize;
    if (!c.d.readVarU32(&bodySize))
        return RenderFail(c, "expected number of function body bytes");

    if (c.d.bytesRemain() < bodySize)
        return RenderFail(c, "function body length too big");

    const uint8_t* bodyBegin = c.d.currentPosition();
    const uint8_t* bodyEnd = bodyBegin + bodySize;

    c.indent++;

    ValTypeVector locals;
    if (!DecodeLocalEntries(c.d, &locals))
        return RenderFail(c, "failed decoding local entries");

    uint32_t localsNum = locals.length();
    if (localsNum > 0) {
        if (!RenderIndent(c))
            return false;
        for (uint32_t i = 0; i < localsNum; i++) {
            if (!c.buffer.append("(local "))
                return false;
            if (!c.buffer.append("$var$"))
                return false;
            if (!RenderInt32(c, i + paramsNum))
                return false;
            ValType local = locals[i];
            if (!c.buffer.append(" "))
                return false;
            if (!RenderValType(c, local))
                return false;
            if (!c.buffer.append(") "))
                return false;
        }
        if (!c.buffer.append("\n"))
            return false;
    }

    if (funcIndex >= c.funcLocals.length() && !c.funcLocals.resize(funcIndex + 1))
        return false;

    c.funcLocals[funcIndex] = localsNum + paramsNum;

    while (c.d.currentPosition() < bodyEnd) {
      if (!RenderFullLine(c))
          return false;
    }

    if (c.d.currentPosition() != bodyEnd)
        return RenderFail(c, "function body length mismatch");

    c.indent--;

    return true;
}

static bool
RenderFunctionBodies(WasmRenderContext& c)
{
    uint32_t sectionStart;
    if (!c.d.startSection(FunctionBodiesId, &sectionStart))
        return RenderFail(c, "failed to start section");

    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numFuncSigs = c.funcSigs.length();

    uint32_t numFuncBodies;
    if (!c.d.readVarU32(&numFuncBodies))
        return RenderFail(c, "expected function body count");
    if (numFuncBodies != numFuncSigs)
        return RenderFail(c, "function body count does not match function signature count");
    for (uint32_t funcIndex = 0; funcIndex < numFuncBodies; funcIndex++) {
        uint32_t sigIndex = c.funcSigs[funcIndex];
        const DeclaredSig& sig = c.signatures[sigIndex];

        if (!RenderIndent(c))
            return false;
        if (!c.buffer.append("(func $func$"))
            return false;
        if (!RenderInt32(c, funcIndex))
            return false;

        if (!RenderSignature(c, sig, true))
            return false;
        if (!c.buffer.append("\n"))
            return false;

        c.currentFuncIndex = funcIndex;

        c.indent++;
        if (!RenderFunctionBody(c, funcIndex, sig.args().length()))
            return false;
        c.indent--;
        if (!RenderIndent(c))
            return false;
        if (!c.buffer.append(")\n"))
            return false;
    }

    if (!c.d.finishSection(sectionStart))
        return RenderFail(c, "function section byte size mismatch");

   return true;
}


static bool
RenderDataSegments(WasmRenderContext& c, uint32_t memInitial, uint32_t memMax)
{
    if (!RenderIndent(c))
        return false;
    if (!c.buffer.append("(memory "))
        return false;
    if (!RenderInt32(c, memInitial))
       return false;
    if (memMax != 0 && ~memMax != 0) {
        if (!c.buffer.append(" "))
            return false;
        if (!RenderInt32(c, memMax))
            return false;
    }

    c.indent++;
    uint32_t sectionStart;
    if (!c.d.startSection(DataSegmentsId, &sectionStart))
        return RenderFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted) {
      if (!c.buffer.append(")\n"))
          return false;
      return true;
    }
    if (!c.buffer.append("\n"))
        return false;

    uint32_t numSegments;
    if (!c.d.readVarU32(&numSegments))
        return RenderFail(c, "failed to read number of data segments");

    for (uint32_t i = 0; i < numSegments; i++) {
        uint32_t dstOffset;
        if (!c.d.readVarU32(&dstOffset))
            return RenderFail(c, "expected segment destination offset");

        if (!RenderIndent(c))
            return false;
        if (!c.buffer.append("(segment "))
           return false;
        if (!RenderInt32(c, dstOffset))
           return false;
        if (!c.buffer.append(" \""))
           return false;

        uint32_t numBytes;
        if (!c.d.readVarU32(&numBytes))
            return RenderFail(c, "expected segment size");

        const uint8_t* src;
        if (!c.d.readBytesRaw(numBytes, &src))
            return RenderFail(c, "data segment shorter than declared");

        RenderString(c, src, numBytes);

        if (!c.buffer.append("\")\n"))
           return false;
    }

    if (!c.d.finishSection(sectionStart))
        return RenderFail(c, "data section byte size mismatch");

    c.indent--;
    if (!c.buffer.append(")\n"))
        return false;

    return true;
}

static bool
RenderModule(WasmRenderContext& c)
{
    uint32_t u32;
    if (!c.d.readFixedU32(&u32) || u32 != MagicNumber)
        return RenderFail(c, "failed to match magic number");

    if (!c.d.readFixedU32(&u32) || u32 != EncodingVersion)
        return RenderFail(c, "failed to match binary version");

    if (!c.buffer.append("(module\n"))
        return false;

    c.indent++;

    if (!RenderSignatures(c))
        return false;

    if (!RenderImportTable(c))
        return false;

    if (!RenderFunctionSignatures(c))
        return false;

    if (!RenderFunctionTable(c))
        return false;

    uint32_t memInitial, memMax;
    if (!RenderMemory(c, &memInitial, &memMax))
        return false;

    if (!RenderExportTable(c))
        return false;

    if (!RenderFunctionBodies(c))
        return false;

    if (!RenderDataSegments(c, memInitial, memMax))
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
    Decoder d(bytes, bytes + length);

    WasmRenderContext c(cx, d, buffer);

    if (!RenderModule(c)) {
        if (!cx->isExceptionPending())
            ReportOutOfMemory(cx);
        return false;
    }

    return true;
}


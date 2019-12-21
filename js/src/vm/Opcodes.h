/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=0 ft=c:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Opcodes_h
#define vm_Opcodes_h

#include "mozilla/Attributes.h"

#include <stddef.h>

/*
 * [SMDOC] Bytecode Definitions
 *
 * JavaScript operation bytecodes. Add a new bytecode by claiming one of the
 * JSOP_UNUSED* here or by extracting the first unused opcode from
 * FOR_EACH_TRAILING_UNUSED_OPCODE.
 *
 * Includers must define a macro with the following form:
 *
 * #define MACRO(op, name, token, length, nuses, ndefs, format) ...
 *
 * Then simply use FOR_EACH_OPCODE(MACRO) to invoke MACRO for every opcode.
 * Selected arguments can be expanded in initializers.
 *
 * Field        Description
 * -----        -----------
 * op           Bytecode name, which is the JSOp enumerator name
 * name         C string containing name for disassembler
 * token        Pretty-printer string, or null if ugly
 * length       Number of bytes including any immediate operands
 * nuses        Number of stack slots consumed by bytecode, -1 if variadic
 * ndefs        Number of stack slots produced by bytecode, -1 if variadic
 * format       Bytecode plus immediate operand encoding format
 *
 */

// clang-format off
/*
 * SpiderMonkey bytecode categorization (as used in generated documentation):
 *
 * [Index]
 *   [Statements]
 *     Jumps
 *     Switch Statement
 *     For-In Statement
 *     With Statement
 *     Exception Handling
 *     Function
 *     Generator
 *     Debugger
 *   [Variables and Scopes]
 *     Variables
 *     Free Variables
 *     Local Variables
 *     Aliased Variables
 *     Intrinsics
 *     Block-local Scope
 *     This
 *     Super
 *     Arguments
 *     Var Scope
 *     Modules
 *   [Operators]
 *     Comparison Operators
 *     Arithmetic Operators
 *     Bitwise Logical Operators
 *     Bitwise Shift Operators
 *     Logical Operators
 *     Special Operators
 *     Stack Operations
 *     Debugger
 *   [Literals]
 *     Constants
 *     Object
 *     Array
 *     RegExp
 *     Class
 *   [Other]
 */
// clang-format on

// clang-format off
#define FOR_EACH_OPCODE(MACRO) \
    /*
     * Pushes 'undefined' onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => undefined
     */ \
    MACRO(JSOP_UNDEFINED, js_undefined_str, "", 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes 'null' onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => null
     */ \
    MACRO(JSOP_NULL, js_null_str, js_null_str, 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes boolean value onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => true/false
     */ \
    MACRO(JSOP_FALSE, js_false_str, js_false_str, 1, 0, 1, JOF_BYTE) \
    MACRO(JSOP_TRUE, js_true_str, js_true_str, 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes '0' onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => 0
     */ \
    MACRO(JSOP_ZERO, "zero", "0", 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes '1' onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => 1
     */ \
    MACRO(JSOP_ONE, "one", "1", 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes 8-bit int immediate integer operand onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: int8_t val
     *   Stack: => val
     */ \
    MACRO(JSOP_INT8, "int8", NULL, 2, 0, 1, JOF_INT8) \
    /*
     * Pushes unsigned 16-bit int immediate integer operand onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint16_t val
     *   Stack: => val
     */ \
    MACRO(JSOP_UINT16, "uint16", NULL, 3, 0, 1, JOF_UINT16) \
    /*
     * Pushes unsigned 24-bit int immediate integer operand onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint24_t val
     *   Stack: => val
     */ \
    MACRO(JSOP_UINT24, "uint24", NULL, 4, 0, 1, JOF_UINT24) \
    /*
     * Pushes 32-bit int immediate integer operand onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: int32_t val
     *   Stack: => val
     */ \
    MACRO(JSOP_INT32, "int32", NULL, 5, 0, 1, JOF_INT32) \
    /*
     * Pushes numeric constant onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: DoubleValue literal
     *   Stack: => val
     */ \
    MACRO(JSOP_DOUBLE, "double", NULL, 9, 0, 1, JOF_DOUBLE) \
    /*
     * Pushes a BigInt constant onto the stack.
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint32_t constIndex
     *   Stack: => val
     */ \
    MACRO(JSOP_BIGINT, "bigint", NULL, 5, 0, 1, JOF_BIGINT) \
    /*
     * Pushes string constant onto the stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint32_t atomIndex
     *   Stack: => atom
     */ \
    MACRO(JSOP_STRING, "string", NULL, 5, 0, 1, JOF_ATOM) \
    /*
     * Push a well-known symbol onto the operand stack.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint8_t symbol (the JS::SymbolCode of the symbol to use)
     *   Stack: => symbol
     */ \
    MACRO(JSOP_SYMBOL, "symbol", NULL, 2, 0, 1, JOF_UINT8) \
    /*
     * Pops the top value on the stack and pushes 'undefined'.
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: val => undefined
     */ \
    MACRO(JSOP_VOID, js_void_str, NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Pops the value 'val' from the stack, then pushes 'typeof val'.
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: val => (typeof val)
     */ \
    MACRO(JSOP_TYPEOF, js_typeof_str, NULL, 1, 1, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    /*
     * Pops the top stack value as 'val' and pushes 'typeof val'. Note that
     * this opcode isn't used when, in the original source code, 'val' is a
     * name -- see 'JSOP_TYPEOF' for that.
     * (This is because 'typeof undefinedName === "undefined"'.)
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: val => (typeof val)
     */ \
    MACRO(JSOP_TYPEOFEXPR, "typeofexpr", NULL, 1, 1, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    /*
     * Pops the value 'val' from the stack, then pushes '+val'.
     * ('+val' is the value converted to a number.)
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => (+val)
     */ \
    MACRO(JSOP_POS, "pos", "+ ", 1, 1, 1, JOF_BYTE) \
    /*
     * Pops the value 'val' from the stack, then pushes '-val'.
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => (-val)
     */ \
    MACRO(JSOP_NEG, "neg", "- ", 1, 1, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the value 'val' from the stack, then pushes '~val'.
     *
     *   Category: Operators
     *   Type: Bitwise Logical Operators
     *   Operands:
     *   Stack: val => (~val)
     */ \
    MACRO(JSOP_BITNOT, "bitnot", "~", 1, 1, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the value 'val' from the stack, then pushes '!val'.
     *
     *   Category: Operators
     *   Type: Logical Operators
     *   Operands:
     *   Stack: val => (!val)
     */ \
    MACRO(JSOP_NOT, "not", "!", 1, 1, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    /*
     * Pops the top two values 'lval' and 'rval' from the stack, then pushes
     * the result of the operation applied to the two operands, converting both
     * to 32-bit signed integers if necessary.
     *
     *   Category: Operators
     *   Type: Bitwise Logical Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_BITOR, "bitor", "|",  1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_BITXOR, "bitxor", "^", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_BITAND, "bitand", "&", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the top two values from the stack and pushes the result of
     * comparing them.
     *
     *   Category: Operators
     *   Type: Comparison Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_EQ, "eq", "==", 1, 2, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    MACRO(JSOP_NE, "ne", "!=", 1, 2, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    /*
     * Pops the top two values from the stack, then pushes the result of
     * applying the operator to the two values.
     *
     *   Category: Operators
     *   Type: Comparison Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_STRICTEQ, "stricteq", "===", 1, 2, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    MACRO(JSOP_STRICTNE, "strictne", "!==", 1, 2, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    MACRO(JSOP_LT, "lt", "<",  1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_GT, "gt", ">",  1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_LE, "le", "<=", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_GE, "ge", ">=", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the top two values 'obj' and 'ctor' from the stack, then pushes
     * 'obj instanceof ctor'. This will throw a 'TypeError' if 'obj' is not an
     * object.
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: obj, ctor => (obj instanceof ctor)
     */ \
    MACRO(JSOP_INSTANCEOF, js_instanceof_str, js_instanceof_str, 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the top two values 'id' and 'obj' from the stack, then pushes
     * 'id in obj'. This will throw a 'TypeError' if 'obj' is not an object.
     *
     * Note that 'obj' is the top value.
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: id, obj => (id in obj)
     */ \
    MACRO(JSOP_IN, js_in_str, js_in_str, 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the top two values 'lval' and 'rval' from the stack, then pushes
     * the result of the operation applied to the operands.
     *
     *   Category: Operators
     *   Type: Bitwise Shift Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_LSH, "lsh", "<<", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_RSH, "rsh", ">>", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the top two values 'lval' and 'rval' from the stack, then pushes
     * 'lval >>> rval'.
     *
     *   Category: Operators
     *   Type: Bitwise Shift Operators
     *   Operands:
     *   Stack: lval, rval => (lval >>> rval)
     */ \
    MACRO(JSOP_URSH, "ursh", ">>>", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the top two values 'lval' and 'rval' from the stack, then pushes
     * the result of 'lval + rval'.
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: lval, rval => (lval + rval)
     */ \
    MACRO(JSOP_ADD, "add", "+", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the top two values 'lval' and 'rval' from the stack, then pushes
     * the result of applying the arithmetic operation to them.
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_SUB, "sub", "-", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the numeric value 'val' from the stack, then pushes 'val + 1'.
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => (val + 1)
     */ \
    MACRO(JSOP_INC, "inc", NULL, 1, 1, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the numeric value 'val' from the stack, then pushes 'val - 1'.
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => (val - 1)
     */ \
    MACRO(JSOP_DEC, "dec", NULL, 1, 1, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_MUL, "mul", "*", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_DIV, "div", "/", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_MOD, "mod", "%", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pops the top two values 'lval' and 'rval' from the stack, then pushes
     * the result of 'Math.pow(lval, rval)'.
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: lval, rval => (lval ** rval)
     */ \
    MACRO(JSOP_POW, "pow", "**", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Replace the top-of-stack value propertyNameValue with
     * ToPropertyKey(propertyNameValue).
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: propertyNameValue => propertyKey
     */ \
    MACRO(JSOP_TOID, "toid", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Pop 'val' from the stack, then push the result of 'ToNumeric(val)'.
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => ToNumeric(val)
     */ \
    MACRO(JSOP_TONUMERIC, "tonumeric", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Converts the value on the top of the stack to a String.
     *
     *   Category: Other
     *   Operands:
     *   Stack: val => ToString(val)
     */ \
    MACRO(JSOP_TOSTRING, "tostring", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Pushes 'this' value for current stack frame onto the stack. Emitted when
     * 'this' refers to the global 'this'.
     *
     *   Category: Variables and Scopes
     *   Type: This
     *   Operands:
     *   Stack: => this
     */ \
    MACRO(JSOP_GLOBALTHIS, "globalthis", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Push "new.target"
     *
     *   Category: Variables and Scopes
     *   Type: Arguments
     *   Operands:
     *   Stack: => new.target
     */ \
    MACRO(JSOP_NEWTARGET, "newtarget", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Dynamic import of the module specified by the string value on the top of
     * the stack.
     *
     *   Category: Variables and Scopes
     *   Type: Modules
     *   Operands:
     *   Stack: arg => rval
     */ \
    MACRO(JSOP_DYNAMIC_IMPORT, "call-import", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Push "import.meta"
     *
     *   Category: Variables and Scopes
     *   Type: Modules
     *   Operands:
     *   Stack: => import.meta
     */ \
    MACRO(JSOP_IMPORTMETA, "importmeta", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes newly created object onto the stack.
     *
     * This opcode has four extra bytes so it can be exchanged with
     * JSOP_NEWOBJECT during emit.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: (uint32_t extra)
     *   Stack: => obj
     */ \
    MACRO(JSOP_NEWINIT, "newinit", NULL, 5, 0, 1, JOF_UINT32|JOF_IC) \
    /*
     * Pushes newly created object onto the stack.
     *
     * This opcode takes an object with the final shape, which can be set at
     * the start and slots then filled in directly. We compute a group based on
     * allocation site (or new group if the template's group is a singleton);
     * see JSOP_NEWOBJECT_WITHGROUP for a variant that uses the same group as
     * the template object.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t baseobjIndex
     *   Stack: => obj
     */ \
    MACRO(JSOP_NEWOBJECT, "newobject", NULL, 5, 0, 1, JOF_OBJECT|JOF_IC) \
    /*
     * Pushes newly created object onto the stack.
     *
     * This opcode takes an object with the final shape, which can be set at
     * the start and slots then filled in directly. Uses the group from the
     * template object; see JSOP_NEWOBJECT for a variant with different
     * heuristics.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t baseobjIndex
     *   Stack: => obj
     */ \
    MACRO(JSOP_NEWOBJECT_WITHGROUP, "newobjectwithgroup", NULL, 5, 0, 1, JOF_OBJECT|JOF_IC) \
    /*
     * Pushes deep-cloned object literal or singleton onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t objectIndex
     *   Stack: => obj
     */ \
    MACRO(JSOP_OBJECT, "object", NULL, 5, 0, 1, JOF_OBJECT) \
    /*
     * Pushes newly created object onto the stack with provided [[Prototype]].
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: proto => obj
     */ \
    MACRO(JSOP_OBJWITHPROTO, "objwithproto", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Initialize a named property in an object literal, like '{a: x}'.
     *
     * Pops the top two values on the stack as 'val' and 'obj', defines
     * 'nameIndex' property of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITPROP, "initprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Initialize a non-enumerable data-property on an object.
     *
     * Pops the top two values on the stack as 'val' and 'obj', defines
     * 'nameIndex' property of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITHIDDENPROP, "inithiddenprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Initialize a non-configurable, non-writable, non-enumerable
     * data-property on an object.
     *
     * Pops the top two values on the stack as 'val' and 'obj', defines
     * 'nameIndex' property of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITLOCKEDPROP, "initlockedprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Initialize a numeric property in an object literal, like '{1: x}'.
     *
     * Pops the top three values on the stack as 'val', 'id' and 'obj', defines
     * 'id' property of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, val => obj
     */ \
    MACRO(JSOP_INITELEM, "initelem", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Initialize a non-enumerable numeric property in an object literal, like
     * '{1: x}'.
     *
     * Pops the top three values on the stack as 'val', 'id' and 'obj', defines
     * 'id' property of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, val => obj
     */ \
    MACRO(JSOP_INITHIDDENELEM, "inithiddenelem", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Initialize a getter in an object literal.
     *
     * Pops the top two values on the stack as 'val' and 'obj', defines getter
     * of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITPROP_GETTER, "initprop_getter", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Initialize a non-enumerable getter in an object literal.
     *
     * Pops the top two values on the stack as 'val' and 'obj', defines getter
     * of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITHIDDENPROP_GETTER, "inithiddenprop_getter", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Initialize a numeric getter in an object literal like
     * '{get 2() {}}'.
     *
     * Pops the top three values on the stack as 'val', 'id' and 'obj', defines
     * 'id' getter of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, val => obj
     */ \
    MACRO(JSOP_INITELEM_GETTER, "initelem_getter", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Initialize a non-enumerable numeric getter in an object literal like
     * '{get 2() {}}'.
     *
     * Pops the top three values on the stack as 'val', 'id' and 'obj', defines
     * 'id' getter of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, val => obj
     */ \
    MACRO(JSOP_INITHIDDENELEM_GETTER, "inithiddenelem_getter", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Initialize a setter in an object literal.
     *
     * Pops the top two values on the stack as 'val' and 'obj', defines setter
     * of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITPROP_SETTER, "initprop_setter", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Initialize a non-enumerable setter in an object literal.
     *
     * Pops the top two values on the stack as 'val' and 'obj', defines setter
     * of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITHIDDENPROP_SETTER, "inithiddenprop_setter", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Initialize a numeric setter in an object literal like
     * '{set 2(v) {}}'.
     *
     * Pops the top three values on the stack as 'val', 'id' and 'obj', defines
     * 'id' setter of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, val => obj
     */ \
    MACRO(JSOP_INITELEM_SETTER, "initelem_setter", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Initialize a non-enumerable numeric setter in an object literal like
     * '{set 2(v) {}}'.
     *
     * Pops the top three values on the stack as 'val', 'id' and 'obj', defines
     * 'id' setter of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, val => obj
     */ \
    MACRO(JSOP_INITHIDDENELEM_SETTER, "inithiddenelem_setter", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Pops the top of stack value, pushes property of it onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj => obj[name]
     */ \
    MACRO(JSOP_GETPROP, "getprop", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_IC) \
    /*
     * Pops the top of stack value, pushes property of it onto the stack.
     * Requires the value under 'obj' to be the receiver of the following call.
     *
     * Like JSOP_GETPROP but for call context.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj => obj[name]
     */ \
    MACRO(JSOP_CALLPROP, "callprop", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_IC) \
    /*
     * Pops the top two values on the stack as 'propval' and 'obj', pushes
     * 'propval' property of 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, propval => obj[propval]
     */ \
    MACRO(JSOP_GETELEM, "getelem", NULL, 1, 2, 1, JOF_BYTE|JOF_ELEM|JOF_TYPESET|JOF_IC) \
    /*
     * Pops the top two values on the stack as 'propval' and 'obj', pushes
     * 'propval' property of 'obj' onto the stack. Requires the value under
     * 'obj' to be the receiver of the following call.
     *
     * Like JSOP_GETELEM but for call context.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, propval => obj[propval]
     */ \
    MACRO(JSOP_CALLELEM, "callelem", NULL, 1, 2, 1, JOF_BYTE|JOF_ELEM|JOF_TYPESET|JOF_IC) \
    /*
     * Pops the top of stack value, pushes the 'length' property of it onto the
     * stack.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands: uint32_t nameIndex
     *   Stack: obj => obj['length']
     */ \
    MACRO(JSOP_LENGTH, "length", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_IC) \
    /*
     * Pops the top two values on the stack as 'val' and 'obj' and performs
     * 'obj.prop = val', pushing 'val' back onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => val
     */ \
    MACRO(JSOP_SETPROP, "setprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY|JOF_IC) \
    /*
     * Pops the top two values on the stack as 'val' and 'obj', and performs
     * 'obj.prop = val', pushing 'val' back onto the stack. Throws a TypeError
     * if the set-operation failed (per strict mode semantics).
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => val
     */ \
    MACRO(JSOP_STRICTSETPROP, "strict-setprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT|JOF_IC) \
    /*
     * Pops the top three values on the stack as 'val', 'propval' and 'obj',
     * sets 'propval' property of 'obj' as 'val', pushes 'val' onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, propval, val => val
     */ \
    MACRO(JSOP_SETELEM, "setelem", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY|JOF_IC) \
    /*
     * Pops the top three values on the stack as 'val', 'propval' and 'obj',
     * sets 'propval' property of 'obj' as 'val', pushes 'val' onto the stack.
     * Throws a TypeError if the set fails, per strict mode semantics.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, propval, val => val
     */ \
    MACRO(JSOP_STRICTSETELEM, "strict-setelem", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT|JOF_IC) \
    /*
     * Pops the top of stack value, deletes property from it, pushes 'true'
     * onto the stack if succeeded, 'false' if not.
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands: uint32_t nameIndex
     *   Stack: obj => succeeded
     */ \
    MACRO(JSOP_DELPROP, "delprop", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_CHECKSLOPPY) \
    /*
     * Pops the top of stack value and attempts to delete the given property
     * from it. Pushes 'true' onto success, else throws a TypeError per strict
     * mode property-deletion requirements.
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands: uint32_t nameIndex
     *   Stack: obj => succeeded
     */ \
    MACRO(JSOP_STRICTDELPROP, "strict-delprop", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_CHECKSTRICT) \
    /*
     * Pops the top two values on the stack as 'propval' and 'obj', deletes
     * 'propval' property from 'obj', pushes 'true'  onto the stack if
     * succeeded, 'false' if not.
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: obj, propval => succeeded
     */ \
    MACRO(JSOP_DELELEM, "delelem", NULL, 1, 2, 1, JOF_BYTE|JOF_ELEM|JOF_CHECKSLOPPY) \
    /*
     * Pops the top two values on the stack as 'propval' and 'obj', and
     * attempts to delete 'propval' property from 'obj'. Pushes 'true' onto the
     * stack on success, else throws a TypeError per strict mode property
     * deletion requirements.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, propval => succeeded
     */ \
    MACRO(JSOP_STRICTDELELEM, "strict-delelem", NULL, 1, 2, 1, JOF_BYTE|JOF_ELEM|JOF_CHECKSTRICT) \
    /*
     * Pops the top two values 'id' and 'obj' from the stack, then pushes
     * obj.hasOwnProperty(id)
     *
     * Note that 'obj' is the top value.
     *
     *   Category: Other
     *   Type:
     *   Operands:
     *   Stack: id, obj => (obj.hasOwnProperty(id))
     */ \
    MACRO(JSOP_HASOWN, "hasown", NULL, 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pushes the prototype of the home object for |callee| onto the
     * stack.
     *
     *   Category: Variables and Scopes
     *   Type: Super
     *   Operands:
     *   Stack: callee => homeObjectProto
     */ \
    MACRO(JSOP_SUPERBASE, "superbase", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Pops the top two values, and pushes the property of one, using the other
     * as the receiver.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: receiver, obj => obj[name]
     */ \
    MACRO(JSOP_GETPROP_SUPER, "getprop-super", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_IC) \
    /*
     * LIKE JSOP_GETELEM but takes receiver on stack, and the propval is
     * evaluated before the obj.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: receiver, propval, obj => obj[propval]
     */ \
    MACRO(JSOP_GETELEM_SUPER, "getelem-super", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_TYPESET|JOF_IC) \
    /*
     * Pops the top three values on the stack as 'val', 'obj' and 'receiver',
     * and performs 'obj.prop = val', pushing 'val' back onto the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: receiver, obj, val => val
     */ \
    MACRO(JSOP_SETPROP_SUPER, "setprop-super", NULL, 5, 3, 1, JOF_ATOM|JOF_PROP|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY) \
    /*
     * Pops the top three values on the stack as 'val' and 'obj', and
     * 'receiver', and performs 'obj.prop = val', pushing 'val' back onto the
     * stack. Throws a TypeError if the set-operation failed (per strict mode
     * semantics).
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: receiver, obj, val => val
     */ \
    MACRO(JSOP_STRICTSETPROP_SUPER, "strictsetprop-super", NULL, 5, 3, 1, JOF_ATOM|JOF_PROP|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT) \
    /*
     * LIKE JSOP_SETELEM, but takes receiver on the stack, and the propval is
     * evaluated before the base.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: receiver, propval, obj, val => val
     */ \
    MACRO(JSOP_SETELEM_SUPER, "setelem-super", NULL, 1, 4, 1, JOF_BYTE|JOF_ELEM|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY) \
    /*
     * LIKE JSOP_STRICTSETELEM, but takes receiver on the stack, and the
     * propval is evaluated before the base.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: receiver, propval, obj, val => val
     */ \
    MACRO(JSOP_STRICTSETELEM_SUPER, "strict-setelem-super", NULL, 1, 4, 1, JOF_BYTE|JOF_ELEM|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT) \
    /*
     * Sets up a for-in loop. It pops the top of stack value as 'val' and
     * pushes 'iter' which is an iterator for 'val'.
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: val => iter
     */ \
    MACRO(JSOP_ITER, "iter", NULL, 1, 1, 1, JOF_BYTE|JOF_IC) \
    /*
     * Pushes the next iterated value onto the stack. If no value is available,
     * MagicValue(JS_NO_ITER_VALUE) is pushed.
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: iter => iter, val
     */ \
    MACRO(JSOP_MOREITER, "moreiter", NULL, 1, 1, 2, JOF_BYTE) \
    /*
     * Pushes a boolean indicating whether the value on top of the stack is
     * MagicValue(JS_NO_ITER_VALUE).
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: val => val, res
     */ \
    MACRO(JSOP_ISNOITER, "isnoiter", NULL, 1, 1, 2, JOF_BYTE) \
    /*
     * NOP opcode to hint to IonBuilder that the value on top of the stack is
     * the (likely string) key in a for-in loop.
     *
     *   Category: Other
     *   Operands:
     *   Stack: val => val
     */ \
    MACRO(JSOP_ITERNEXT, "iternext", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Exits a for-in loop by popping the iteration value and the iterator
     * object from the stack and closing the iterator object.
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: iter, iterval =>
     */ \
    MACRO(JSOP_ENDITER, "enditer", NULL, 1, 2, 0, JOF_BYTE) \
    /*
     * Checks that the top value on the stack is an object, and throws a
     * TypeError if not. The operand 'kind' is used only to generate an
     * appropriate error message.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint8_t kind
     *   Stack: result => result
     */ \
    MACRO(JSOP_CHECKISOBJ, "checkisobj", NULL, 2, 1, 1, JOF_UINT8) \
    /*
     * Checks that the top value on the stack is callable, and throws a
     * TypeError if not. The operand 'kind' is used only to generate an
     * appropriate error message.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint8_t kind
     *   Stack: obj => obj
     */ \
    MACRO(JSOP_CHECKISCALLABLE, "checkiscallable", NULL, 2, 1, 1, JOF_UINT8) \
    /*
     * Throw if the value on the stack is not coerscible to an object (is
     * |null| or |undefined|).
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: val => val
     */ \
    MACRO(JSOP_CHECKOBJCOERCIBLE, "checkobjcoercible", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Pops the iterator and its next method from the top of the stack, and
     * create async iterator from it and push the async iterator back onto the
     * stack.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands:
     *   Stack: iter, next => asynciter
     */ \
    MACRO(JSOP_TOASYNCITER, "toasynciter", NULL, 1, 2, 1, JOF_BYTE) \
    /*
     * '__proto__: v' inside an object initializer.
     *
     * Pops the top two values on the stack as 'newProto' and 'obj', sets
     * prototype of 'obj' as 'newProto', pushes 'true' onto the stack if
     * succeeded, 'false' if not.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, newProto => succeeded
     */ \
    MACRO(JSOP_MUTATEPROTO, "mutateproto", NULL, 1, 2, 1, JOF_BYTE) \
    /*
     * Pushes newly created array onto the stack.
     *
     * This opcode takes the final length, which is preallocated.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands: uint32_t length
     *   Stack: => obj
     */ \
    MACRO(JSOP_NEWARRAY, "newarray", NULL, 5, 0, 1, JOF_UINT32|JOF_IC) \
    /*
     * Initialize an array element.
     *
     * Pops the top two values on the stack as 'val' and 'obj', sets 'index'
     * property of 'obj' as 'val', pushes 'obj' onto the stack.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands: uint32_t index
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITELEM_ARRAY, "initelem_array", NULL, 5, 2, 1, JOF_UINT32|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Pops the top three values on the stack as 'val', 'index' and 'obj', sets
     * 'index' property of 'obj' as 'val', pushes 'obj' and 'index + 1' onto
     * the stack.
     *
     * This opcode is used in Array literals with spread and spreadcall
     * arguments.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands:
     *   Stack: obj, index, val => obj, (index + 1)
     */ \
    MACRO(JSOP_INITELEM_INC, "initelem_inc", NULL, 1, 3, 2, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_IC) \
    /*
     * Pushes a JS_ELEMENTS_HOLE value onto the stack, representing an omitted
     * property in an array literal (e.g. property 0 in the array '[, 1]').
     *
     * This opcode is used with the JSOP_NEWARRAY opcode.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands:
     *   Stack: => hole
     */ \
    MACRO(JSOP_HOLE, "hole", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes a newly created array onto the stack, whose elements are the same
     * as that of a template object's copy on write elements.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands: uint32_t objectIndex
     *   Stack: => obj
     */ \
    MACRO(JSOP_NEWARRAY_COPYONWRITE, "newarray_copyonwrite", NULL, 5, 0, 1, JOF_OBJECT) \
    /*
     * Pushes a regular expression literal onto the stack. It requires special
     * "clone on exec" handling.
     *
     *   Category: Literals
     *   Type: RegExp
     *   Operands: uint32_t regexpIndex
     *   Stack: => regexp
     */ \
    MACRO(JSOP_REGEXP, "regexp", NULL, 5, 0, 1, JOF_REGEXP) \
    /*
     * Pushes a closure for a named or anonymous function expression onto the
     * stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint32_t funcIndex
     *   Stack: => obj
     */ \
    MACRO(JSOP_LAMBDA, "lambda", NULL, 5, 0, 1, JOF_OBJECT) \
    /*
     * Pops the top of stack value as 'new.target', pushes an arrow function
     * with lexical 'new.target' onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint32_t funcIndex
     *   Stack: new.target => obj
     */ \
    MACRO(JSOP_LAMBDA_ARROW, "lambda_arrow", NULL, 5, 1, 1, JOF_OBJECT) \
    /*
     * Pops the top two values on the stack as 'name' and 'fun', defines the
     * name of 'fun' to 'name' with prefix if any, and pushes 'fun' back onto
     * the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint8_t prefixKind
     *   Stack: fun, name => fun
     */ \
    MACRO(JSOP_SETFUNNAME, "setfunname", NULL, 2, 2, 1, JOF_UINT8) \
    /*
     * Initialize the home object for functions with super bindings.
     *
     * This opcode takes the function and the object to be the home object,
     * does the set, and leaves the function on the stack.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: fun, homeObject => fun
     */ \
    MACRO(JSOP_INITHOMEOBJECT, "inithomeobject", NULL, 1, 2, 1, JOF_BYTE) \
    /*
     * Ensures the result of a class's heritage expression is either null or a
     * constructor.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: heritage => heritage
     */ \
    MACRO(JSOP_CHECKCLASSHERITAGE, "checkclassheritage", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Pushes a clone of a function with a given [[Prototype]] onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint32_t funcIndex
     *   Stack: proto => obj
     */ \
    MACRO(JSOP_FUNWITHPROTO, "funwithproto", NULL, 5, 1, 1, JOF_OBJECT) \
    /*
     * Push a default constructor for a base class literal.
     *
     * The sourceStart/sourceEnd offsets are the start/end offsets of the class
     * definition in the source buffer and are used for toString().
     *
     *   Category: Literals
     *   Type: Class
     *   Operands: atom className, uint32_t sourceStart, uint32_t sourceEnd
     *   Stack: => constructor
     */ \
    MACRO(JSOP_CLASSCONSTRUCTOR, "classconstructor", NULL, 13, 0, 1, JOF_CLASS_CTOR) \
    /*
     * Push a default constructor for a derived class literal.
     *
     * The sourceStart/sourceEnd offsets are the start/end offsets of the class
     * definition in the source buffer and are used for toString().
     *
     *   Category: Literals
     *   Type: Class
     *   Operands: atom className, uint32_t sourceStart, uint32_t sourceEnd
     *   Stack: proto => constructor
     */ \
    MACRO(JSOP_DERIVEDCONSTRUCTOR, "derivedconstructor", NULL, 13, 1, 1, JOF_CLASS_CTOR) \
    /*
     * Pushes the current global's builtin prototype for a given proto key.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint8_t kind
     *   Stack: => %BuiltinPrototype%
     */ \
    MACRO(JSOP_BUILTINPROTO, "builtinproto", NULL, 2, 0, 1, JOF_UINT8) \
    /*
     * Invokes 'callee' with 'this' and 'args', pushes return value onto the
     * stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc
     *   Stack: callee, this, args[0], ..., args[argc-1] => rval
     *   nuses: (argc+2)
     */ \
    MACRO(JSOP_CALL, "call", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * Like JSOP_CALL, but used as part of for-of and destructuring bytecode to
     * provide better error messages.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc (must be 0)
     *   Stack: callee, this => rval
     *   nuses: 2
     */ \
    MACRO(JSOP_CALLITER, "calliter", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * Invokes 'callee' with 'this' and 'args', pushes return value onto the
     * stack.
     *
     * This is for 'f.apply'.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc
     *   Stack: callee, this, args[0], ..., args[argc-1] => rval
     *   nuses: (argc+2)
     */ \
    MACRO(JSOP_FUNAPPLY, "funapply", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * Invokes 'callee' with 'this' and 'args', pushes return value onto the
     * stack.
     *
     * If 'callee' is determined to be the canonical 'Function.prototype.call'
     * function, then this operation is optimized to directly call 'callee'
     * with 'args[0]' as 'this', and the remaining arguments as formal args to
     * 'callee'.
     *
     * Like JSOP_FUNAPPLY but for 'f.call' instead of 'f.apply'.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc
     *   Stack: callee, this, args[0], ..., args[argc-1] => rval
     *   nuses: (argc+2)
     */ \
    MACRO(JSOP_FUNCALL, "funcall", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * Like JSOP_CALL, but tells the function that the return value is ignored.
     * stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc
     *   Stack: callee, this, args[0], ..., args[argc-1] => rval
     *   nuses: (argc+2)
     */ \
    MACRO(JSOP_CALL_IGNORES_RV, "call-ignores-rv", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * spreadcall variant of JSOP_CALL.
     *
     * Invokes 'callee' with 'this' and 'args', pushes the return value onto
     * the stack.
     *
     * 'args' is an Array object which contains actual arguments.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: callee, this, args => rval
     */ \
    MACRO(JSOP_SPREADCALL, "spreadcall", NULL, 1, 3, 1, JOF_BYTE|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * Pops the top stack value, pushes the value and a boolean value that
     * indicates whether the spread operation for the value can be optimized in
     * spread call.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: arr => arr, optimized
     */ \
    MACRO(JSOP_OPTIMIZE_SPREADCALL, "optimize-spreadcall", NULL, 1, 1, 2, JOF_BYTE) \
    /*
     * Invokes 'eval' with 'args' and pushes return value onto the stack.
     *
     * If 'eval' in global scope is not original one, invokes the function with
     * 'this' and 'args', and pushes return value onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc
     *   Stack: callee, this, args[0], ..., args[argc-1] => rval
     *   nuses: (argc+2)
     */ \
    MACRO(JSOP_EVAL, "eval", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_CHECKSLOPPY|JOF_IC) \
    /*
     * spreadcall variant of JSOP_EVAL
     *
     * Invokes 'eval' with 'args' and pushes the return value onto the stack.
     *
     * If 'eval' in global scope is not original one, invokes the function with
     * 'this' and 'args', and pushes return value onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: callee, this, args => rval
     */ \
    MACRO(JSOP_SPREADEVAL, "spreadeval", NULL, 1, 3, 1, JOF_BYTE|JOF_INVOKE|JOF_TYPESET|JOF_CHECKSLOPPY|JOF_IC) \
    /*
     * Invokes 'eval' with 'args' and pushes return value onto the stack.
     *
     * If 'eval' in global scope is not original one, invokes the function with
     * 'this' and 'args', and pushes return value onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc
     *   Stack: callee, this, args[0], ..., args[argc-1] => rval
     *   nuses: (argc+2)
     */ \
    MACRO(JSOP_STRICTEVAL, "strict-eval", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_CHECKSTRICT|JOF_IC) \
    /*
     * spreadcall variant of JSOP_EVAL
     *
     * Invokes 'eval' with 'args' and pushes the return value onto the stack.
     *
     * If 'eval' in global scope is not original one, invokes the function with
     * 'this' and 'args', and pushes return value onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: callee, this, args => rval
     */ \
    MACRO(JSOP_STRICTSPREADEVAL, "strict-spreadeval", NULL, 1, 3, 1, JOF_BYTE|JOF_INVOKE|JOF_TYPESET|JOF_CHECKSTRICT|JOF_IC) \
    /*
     * Pushes the implicit 'this' value for calls to the associated name onto
     * the stack.
     *
     *   Category: Variables and Scopes
     *   Type: This
     *   Operands: uint32_t nameIndex
     *   Stack: => this
     */ \
    MACRO(JSOP_IMPLICITTHIS, "implicitthis", "", 5, 0, 1, JOF_ATOM) \
    /*
     * Pushes the implicit 'this' value for calls to the associated name onto
     * the stack; only used when the implicit this might be derived from a
     * non-syntactic scope (instead of the global itself).
     *
     * Note that code evaluated via the Debugger API uses DebugEnvironmentProxy
     * objects on its scope chain, which are non-syntactic environments that
     * refer to syntactic environments. As a result, the binding we want may be
     * held by a syntactic environments such as CallObject or
     * VarEnvrionmentObject.
     *
     *   Category: Variables and Scopes
     *   Type: This
     *   Operands: uint32_t nameIndex
     *   Stack: => this
     */ \
    MACRO(JSOP_GIMPLICITTHIS, "gimplicitthis", "", 5, 0, 1, JOF_ATOM) \
    /*
     * Pushes the call site object specified by objectIndex onto the stack.
     * Defines the raw property specified by objectIndex + 1 on the call site
     * object and freezes both the call site object as well as its raw
     * property.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t objectIndex
     *   Stack: => obj
     */ \
    MACRO(JSOP_CALLSITEOBJ, "callsiteobj", NULL, 5, 0, 1, JOF_OBJECT) \
    /*
     * Pushes 'JS_IS_CONSTRUCTING'
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => JS_IS_CONSTRUCTING
     */ \
    MACRO(JSOP_IS_CONSTRUCTING, "is-constructing", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Invokes 'callee' as a constructor with 'this' and 'args', pushes return
     * value onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc
     *   Stack: callee, this, args[0], ..., args[argc-1], newTarget => rval
     *   nuses: (argc+3)
     */ \
    MACRO(JSOP_NEW, "new", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_IC|JOF_IC) \
    /*
     * spreadcall variant of JSOP_NEW
     *
     * Invokes 'callee' as a constructor with 'this' and 'args', pushes the
     * return value onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: callee, this, args, newTarget => rval
     */ \
    MACRO(JSOP_SPREADNEW, "spreadnew", NULL, 1, 4, 1, JOF_BYTE|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * Push the function to invoke with |super()|. This is the prototype of the
     * function passed in as |callee|.
     *
     *   Category: Variables and Scopes
     *   Type: Super
     *   Operands:
     *   Stack: callee => superFun
     */ \
    MACRO(JSOP_SUPERFUN, "superfun", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Behaves exactly like JSOP_NEW, but allows JITs to distinguish the two
     * cases.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint16_t argc
     *   Stack: callee, this, args[0], ..., args[argc-1], newTarget => rval
     *   nuses: (argc+3)
     */ \
    MACRO(JSOP_SUPERCALL, "supercall", NULL, 3, -1, 1, JOF_ARGC|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * spreadcall variant of JSOP_SUPERCALL.
     *
     * Behaves exactly like JSOP_SPREADNEW.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: callee, this, args, newTarget => rval
     */ \
    MACRO(JSOP_SPREADSUPERCALL, "spreadsupercall", NULL, 1, 4, 1, JOF_BYTE|JOF_INVOKE|JOF_TYPESET|JOF_IC) \
    /*
     * Throw an exception if the value on top of the stack is not the TDZ
     * MagicValue. Used in derived class constructors.
     *
     *   Category: Variables and Scopes
     *   Type: This
     *   Operands:
     *   Stack: this => this
     */ \
    MACRO(JSOP_CHECKTHISREINIT, "checkthisreinit", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Initializes generator frame, creates a generator and pushes it on the
     * stack.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands:
     *   Stack: => generator
     */ \
    MACRO(JSOP_GENERATOR, "generator", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Pops the generator from the top of the stack, suspends it and stops
     * interpretation.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint24_t resumeIndex
     *   Stack: generator => generator
     */ \
    MACRO(JSOP_INITIALYIELD, "initialyield", NULL, 4, 1, 1, JOF_RESUMEINDEX) \
    /*
     * Bytecode emitted after 'yield' expressions. This is useful for the
     * Debugger and AbstractGeneratorObject::isAfterYieldOrAwait. It's treated
     * as jump target op so that the Baseline Interpreter can efficiently
     * restore the frame's interpreterICEntry when resuming a generator.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint32_t icIndex
     *   Stack: =>
     */ \
    MACRO(JSOP_AFTERYIELD, "afteryield", NULL, 5, 0, 0, JOF_ICINDEX) \
    /*
     * Pops the generator and suspends and closes it. Yields the value in the
     * frame's return value slot.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands:
     *   Stack: gen =>
     */ \
    MACRO(JSOP_FINALYIELDRVAL, "finalyieldrval", NULL, 1, 1, 0, JOF_BYTE) \
    /*
     * Pops the generator and the return value 'rval1', stops interpretation
     * and returns 'rval1'. Pushes sent value from 'send()' onto the stack.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint24_t resumeIndex
     *   Stack: rval1, gen => rval2
     */ \
    MACRO(JSOP_YIELD, "yield", NULL, 4, 2, 1, JOF_RESUMEINDEX) \
    /*
     * Pushes a boolean indicating whether the top of the stack is
     * MagicValue(JS_GENERATOR_CLOSING).
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: val => val, res
     */ \
    MACRO(JSOP_ISGENCLOSING, "isgenclosing", NULL, 1, 1, 2, JOF_BYTE) \
    /*
     * Pops the top two values 'value' and 'gen' from the stack, then starts
     * "awaiting" for 'value' to be resolved, which will then resume the
     * execution of 'gen'. Pushes the async function promise on the stack, so
     * that it'll be returned to the caller on the very first "await".
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands:
     *   Stack: value, gen => promise
     */ \
    MACRO(JSOP_ASYNCAWAIT, "async-await", NULL, 1, 2, 1, JOF_BYTE) \
    /*
     * Pops the top two values 'valueOrReason' and 'gen' from the stack, then
     * pushes the promise resolved with 'valueOrReason'. `gen` must be the
     * internal generator object created in async functions. The pushed promise
     * is the async function's result promise, which is stored in `gen`.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint8_t fulfillOrReject
     *   Stack: valueOrReason, gen => promise
     */ \
    MACRO(JSOP_ASYNCRESOLVE, "async-resolve", NULL, 2, 2, 1, JOF_UINT8) \
    /*
     * Pops the generator and the return value 'promise', stops interpretation
     * and returns 'promise'. Pushes resolved value onto the stack.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint24_t resumeIndex
     *   Stack: promise, gen => resolved
     */ \
    MACRO(JSOP_AWAIT, "await", NULL, 4, 2, 1, JOF_RESUMEINDEX) \
    /*
     * Pops the top of stack value as 'value', checks if the await for 'value'
     * can be skipped. If the await operation can be skipped and the resolution
     * value for 'value' can be acquired, pushes the resolution value and
     * 'true' onto the stack. Otherwise, pushes 'value' and 'false' on the
     * stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: value => value_or_resolved, canskip
     */ \
    MACRO(JSOP_TRYSKIPAWAIT, "tryskipawait", NULL, 1, 1, 2, JOF_BYTE) \
    /*
     * Pops the generator and argument from the stack, pushes a new generator
     * frame and resumes execution of it. Pushes the return value after the
     * generator yields.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: resume kind (AbstractGeneratorObject::ResumeKind)
     *   Stack: gen, val => rval
     */ \
    MACRO(JSOP_RESUME, "resume", NULL, 2, 2, 1, JOF_UINT8|JOF_INVOKE) \
    /*
     * This opcode is a no-op and it indicates the location of a jump
     * instruction target. Some other opcodes act as jump targets as well, see
     * BytecodeIsJumpTarget. The IC index is used by the Baseline interpreter.
     *
     *   Category: Other
     *   Operands: uint32_t icIndex
     *   Stack: =>
     */ \
    MACRO(JSOP_JUMPTARGET, "jumptarget", NULL, 5, 0, 0, JOF_ICINDEX) \
    /*
     * This opcode is the target of the backwards jump for some loop.
     *
     * The depthHint value is a loop depth hint for Ion. It starts at 1 and
     * deeply nested loops all have the same value.
     *
     * See JSOP_JUMPTARGET for the icIndex operand.
     *
     *   Category: Statements
     *   Type: Jumps
     *   Operands: uint32_t icIndex, uint8_t depthHint
     *   Stack: =>
     */ \
    MACRO(JSOP_LOOPHEAD, "loophead", NULL, 6, 0, 0, JOF_LOOPHEAD) \
    /*
     * Jumps to a 32-bit offset from the current bytecode.
     *
     *   Category: Statements
     *   Type: Jumps
     *   Operands: int32_t offset
     *   Stack: =>
     */ \
    MACRO(JSOP_GOTO, "goto", NULL, 5, 0, 0, JOF_JUMP) \
    /*
     * Pops the top of stack value, converts it into a boolean, if the result
     * is 'false', jumps to a 32-bit offset from the current bytecode.
     *
     * The idea is that a sequence like
     * JSOP_ZERO; JSOP_ZERO; JSOP_EQ; JSOP_IFEQ; JSOP_RETURN;
     * reads like a nice linear sequence that will execute the return.
     *
     *   Category: Statements
     *   Type: Jumps
     *   Operands: int32_t offset
     *   Stack: cond =>
     */ \
    MACRO(JSOP_IFEQ, "ifeq", NULL, 5, 1, 0, JOF_JUMP|JOF_DETECTING|JOF_IC) \
    /*
     * Pops the top of stack value, converts it into a boolean, if the result
     * is 'true', jumps to a 32-bit offset from the current bytecode.
     *
     *   Category: Statements
     *   Type: Jumps
     *   Operands: int32_t offset
     *   Stack: cond =>
     */ \
    MACRO(JSOP_IFNE, "ifne", NULL, 5, 1, 0, JOF_JUMP|JOF_IC) \
    /*
     * Converts the top of stack value into a boolean, if the result is
     * 'false', jumps to a 32-bit offset from the current bytecode.
     *
     *   Category: Statements
     *   Type: Jumps
     *   Operands: int32_t offset
     *   Stack: cond => cond
     */ \
    MACRO(JSOP_AND, "and", NULL, 5, 1, 1, JOF_JUMP|JOF_DETECTING|JOF_IC) \
    /*
     * Converts the top of stack value into a boolean, if the result is 'true',
     * jumps to a 32-bit offset from the current bytecode.
     *
     *   Category: Statements
     *   Type: Jumps
     *   Operands: int32_t offset
     *   Stack: cond => cond
     */ \
    MACRO(JSOP_OR, "or", NULL, 5, 1, 1, JOF_JUMP|JOF_DETECTING|JOF_IC) \
    /*
     * If the value on top of the stack is not null or undefined, jumps to a 32-bit offset from the
     * current bytecode.
     *
     *   Category: Statements
     *   Type: Jumps
     *   Operands: int32_t offset
     *   Stack: cond => cond
     */ \
    MACRO(JSOP_COALESCE, "coalesce", NULL, 5, 1, 1, JOF_JUMP|JOF_DETECTING) \
    /*
     * Pops the top two values on the stack as 'val' and 'cond'. If 'cond' is
     * 'true', jumps to a 32-bit offset from the current bytecode, re-pushes
     * 'val' onto the stack if 'false'.
     *
     *   Category: Statements
     *   Type: Switch Statement
     *   Operands: int32_t offset
     *   Stack: val, cond => val(if !cond)
     */ \
    MACRO(JSOP_CASE, "case", NULL, 5, 2, 1, JOF_JUMP) \
    /*
     * This appears after all cases in a JSOP_CONDSWITCH, whether there is a
     * 'default:' label in the switch statement or not. Pop the switch operand
     * from the stack and jump to a 32-bit offset from the current bytecode.
     * offset from the current bytecode.
     *
     *   Category: Statements
     *   Type: Switch Statement
     *   Operands: int32_t offset
     *   Stack: lval =>
     */ \
    MACRO(JSOP_DEFAULT, "default", NULL, 5, 1, 0, JOF_JUMP) \
    /*
     * Pops the top of stack value as 'i', if 'low <= i <= high',
     * jumps to a 32-bit offset: offset is stored in the script's resumeOffsets
     *                           list at index 'firstResumeIndex + (i - low)'
     * jumps to a 32-bit offset: 'len' from the current bytecode otherwise
     *
     *   Category: Statements
     *   Type: Switch Statement
     *   Operands: int32_t len, int32_t low, int32_t high,
     *             uint24_t firstResumeIndex
     *   Stack: i =>
     *   len: len
     */ \
    MACRO(JSOP_TABLESWITCH, "tableswitch", NULL, 16, 1, 0, JOF_TABLESWITCH|JOF_DETECTING) \
    /*
     * Pops the top of stack value as 'rval', stops interpretation of current
     * script and returns 'rval'.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: rval =>
     */ \
    MACRO(JSOP_RETURN, "return", NULL, 1, 1, 0, JOF_BYTE) \
    /*
     * Pushes stack frame's 'rval' onto the stack.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: => rval
     */ \
    MACRO(JSOP_GETRVAL, "getrval", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Pops the top of stack value as 'rval', sets the return value in stack
     * frame as 'rval'.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: rval =>
     */ \
    MACRO(JSOP_SETRVAL, "setrval", NULL, 1, 1, 0, JOF_BYTE) \
    /*
     * Stops interpretation and returns value set by JSOP_SETRVAL. When not
     * set, returns 'undefined'.
     *
     * Also emitted at end of script so interpreter don't need to check if
     * opcode is still in script range.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_RETRVAL, "retrval", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Check if a derived class constructor has a valid return value and 'this'
     * value before it returns. If the return value is not an object, stores
     * the 'this' value to the return value slot.
     *
     *   Category: Variables and Scopes
     *   Type: This
     *   Operands:
     *   Stack: this =>
     */ \
    MACRO(JSOP_CHECKRETURN, "checkreturn", NULL, 1, 1, 0, JOF_BYTE) \
    /*
     * Pops the top of stack value as 'v', sets pending exception as 'v', then
     * raises error.
     *
     *   Category: Statements
     *   Type: Exception Handling
     *   Operands:
     *   Stack: v =>
     */ \
    MACRO(JSOP_THROW, js_throw_str, NULL, 1, 1, 0, JOF_BYTE) \
    /*
     * Sometimes we know when emitting that an operation will always throw.
     *
     * Throws the indicated JSMSG.
     *
     *   Category: Statements
     *   Type: Exception Handling
     *   Operands: uint16_t msgNumber
     *   Stack: =>
     */ \
    MACRO(JSOP_THROWMSG, "throwmsg", NULL, 3, 0, 0, JOF_UINT16) \
    /*
     * Throws a runtime TypeError for invalid assignment to 'const'. The
     * environment coordinate is used for better error messages.
     *
     *   Category: Variables and Scopes
     *   Type: Aliased Variables
     *   Operands: uint8_t hops, uint24_t slot
     *   Stack: v => v
     */ \
    MACRO(JSOP_THROWSETALIASEDCONST, "throwsetaliasedconst", NULL, 5, 1, 1, JOF_ENVCOORD|JOF_NAME|JOF_DETECTING) \
    /*
     * Throws a runtime TypeError for invalid assignment to the callee in a
     * named lambda, which is always a 'const' binding. This is a different
     * bytecode than JSOP_SETCONST because the named lambda callee, if not
     * closed over, does not have a frame slot to look up the name with for the
     * error message.
     *
     *   Category: Variables and Scopes
     *   Type: Local Variables
     *   Operands:
     *   Stack: v => v
     */ \
    MACRO(JSOP_THROWSETCALLEE, "throwsetcallee", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Throws a runtime TypeError for invalid assignment to 'const'. The
     * localno is used for better error messages.
     *
     *   Category: Variables and Scopes
     *   Type: Local Variables
     *   Operands: uint24_t localno
     *   Stack: v => v
     */ \
    MACRO(JSOP_THROWSETCONST, "throwsetconst", NULL, 4, 1, 1, JOF_LOCAL|JOF_NAME|JOF_DETECTING) \
    /*
     * This no-op appears at the top of the bytecode for a 'TryStatement'.
     *
     * Location information for catch/finally blocks is stored in a side table,
     * 'script->trynotes()'.
     *
     *   Category: Statements
     *   Type: Exception Handling
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_TRY, "try", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * No-op used by the exception unwinder to determine the correct
     * environment to unwind to when performing IteratorClose due to
     * destructuring.
     *
     *   Category: Other
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_TRY_DESTRUCTURING, "try-destructuring", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Pushes the current pending exception onto the stack and clears the
     * pending exception. This is only emitted at the beginning of code for a
     * catch-block, so it is known that an exception is pending. It is used to
     * implement catch-blocks and 'yield*'.
     *
     *   Category: Statements
     *   Type: Exception Handling
     *   Operands:
     *   Stack: => exception
     */ \
    MACRO(JSOP_EXCEPTION, "exception", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes a resumeIndex (stored as 24-bit operand) on the stack.
     *
     * Resume indexes are used for ops like JSOP_YIELD and JSOP_GOSUB.
     * JSScript and BaselineScript have lists of resume entries (one for each
     * resumeIndex); this lets the JIT resume at these ops from JIT code.
     *
     *   Category: Other
     *   Operands: uint24_t resumeIndex
     *   Stack: => resumeIndex
     */ \
    MACRO(JSOP_RESUMEINDEX, "resume-index", NULL, 4, 0, 1, JOF_RESUMEINDEX) \
    /*
     * This opcode is used for entering a 'finally' block. Jumps to a 32-bit
     * offset from the current pc.
     *
     * Note: this op doesn't actually push/pop any values, but it has a use
     * count of 2 (for the 'false' + resumeIndex values pushed by preceding
     * bytecode ops) because the 'finally' entry point does not expect these
     * values on the stack. See also JSOP_FINALLY (it has a def count of 2).
     *
     * When the execution resumes from 'finally' block, those stack values are
     * popped.
     *
     *   Category: Statements
     *   Type: Exception Handling
     *   Operands: int32_t offset
     *   Stack: false, resumeIndex =>
     */ \
    MACRO(JSOP_GOSUB, "gosub", NULL, 5, 2, 0, JOF_JUMP) \
    /*
     * This opcode has a def count of 2, but these values are already on the
     * stack (they're pushed by JSOP_GOSUB).
     *
     *   Category: Statements
     *   Type: Exception Handling
     *   Operands:
     *   Stack: => false, resumeIndex
     */ \
    MACRO(JSOP_FINALLY, "finally", NULL, 1, 0, 2, JOF_BYTE) \
    /*
     * This opcode is used for returning from a 'finally' block.
     *
     * Pops the top two values on the stack as 'rval' and 'lval'. Then:
     * - If 'lval' is true, throws 'rval'.
     * - If 'lval' is false, jumps to the resumeIndex stored in 'lval'.
     *
     *   Category: Statements
     *   Type: Exception Handling
     *   Operands:
     *   Stack: lval, rval =>
     */ \
    MACRO(JSOP_RETSUB, "retsub", NULL, 1, 2, 0, JOF_BYTE) \
    /*
     * Pushes a JS_UNINITIALIZED_LEXICAL value onto the stack, representing an
     * uninitialized lexical binding.
     *
     * This opcode is used with the JSOP_INITLEXICAL opcode.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => uninitialized
     */ \
    MACRO(JSOP_UNINITIALIZED, "uninitialized", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Initializes an uninitialized local lexical binding with the top of stack
     * value.
     *
     *   Category: Variables and Scopes
     *   Type: Local Variables
     *   Operands: uint24_t localno
     *   Stack: v => v
     */ \
    MACRO(JSOP_INITLEXICAL, "initlexical", NULL, 4, 1, 1, JOF_LOCAL|JOF_NAME|JOF_DETECTING) \
    /*
     * Initializes an uninitialized global lexical binding with the top of
     * stack value.
     *
     *   Category: Variables and Scopes
     *   Type: Free Variables
     *   Operands: uint32_t nameIndex
     *   Stack: val => val
     */ \
    MACRO(JSOP_INITGLEXICAL, "initglexical", NULL, 5, 1, 1, JOF_ATOM|JOF_NAME|JOF_PROPINIT|JOF_GNAME|JOF_IC) \
    /*
     * Initializes an uninitialized aliased lexical binding with the top of
     * stack value.
     *
     *   Category: Variables and Scopes
     *   Type: Aliased Variables
     *   Operands: uint8_t hops, uint24_t slot
     *   Stack: v => v
     */ \
    MACRO(JSOP_INITALIASEDLEXICAL, "initaliasedlexical", NULL, 5, 1, 1, JOF_ENVCOORD|JOF_NAME|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Checks if the value of the local variable is the
     * JS_UNINITIALIZED_LEXICAL magic, throwing an error if so.
     *
     *   Category: Variables and Scopes
     *   Type: Local Variables
     *   Operands: uint24_t localno
     *   Stack: =>
     */ \
    MACRO(JSOP_CHECKLEXICAL, "checklexical", NULL, 4, 0, 0, JOF_LOCAL|JOF_NAME) \
    /*
     * Checks if the value of the aliased variable is the
     * JS_UNINITIALIZED_LEXICAL magic, throwing an error if so.
     *
     *   Category: Variables and Scopes
     *   Type: Aliased Variables
     *   Operands: uint8_t hops, uint24_t slot
     *   Stack: =>
     */ \
    MACRO(JSOP_CHECKALIASEDLEXICAL, "checkaliasedlexical", NULL, 5, 0, 0, JOF_ENVCOORD|JOF_NAME) \
    /*
     * Throw if the value on top of the stack is the TDZ MagicValue. Used in
     * derived class constructors.
     *
     *   Category: Variables and Scopes
     *   Type: This
     *   Operands:
     *   Stack: this => this
     */ \
    MACRO(JSOP_CHECKTHIS, "checkthis", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Pushes the global environment onto the stack if the script doesn't have
     * a non-syntactic global scope. Otherwise will act like JSOP_BINDNAME.
     *
     * 'nameIndex' is only used when acting like JSOP_BINDNAME.
     *
     *   Category: Variables and Scopes
     *   Type: Free Variables
     *   Operands: uint32_t nameIndex
     *   Stack: => global
     */ \
    MACRO(JSOP_BINDGNAME, "bindgname", NULL, 5, 0, 1, JOF_ATOM|JOF_NAME|JOF_GNAME|JOF_IC) \
    /*
     * Looks up name on the environment chain and pushes the environment which
     * contains the name onto the stack. If not found, pushes global lexical
     * environment onto the stack.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: => env
     */ \
    MACRO(JSOP_BINDNAME, "bindname", NULL, 5, 0, 1, JOF_ATOM|JOF_NAME|JOF_IC) \
    /*
     * Looks up name on the environment chain and pushes its value onto the
     * stack.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: => val
     */ \
    MACRO(JSOP_GETNAME, "getname", NULL, 5, 0, 1, JOF_ATOM|JOF_NAME|JOF_TYPESET|JOF_IC) \
    /*
     * Looks up name on global environment and pushes its value onto the stack,
     * unless the script has a non-syntactic global scope, in which case it
     * acts just like JSOP_NAME.
     *
     * Free variable references that must either be found on the global or a
     * ReferenceError.
     *
     *   Category: Variables and Scopes
     *   Type: Free Variables
     *   Operands: uint32_t nameIndex
     *   Stack: => val
     */ \
    MACRO(JSOP_GETGNAME, "getgname", NULL, 5, 0, 1, JOF_ATOM|JOF_NAME|JOF_TYPESET|JOF_GNAME|JOF_IC) \
    /*
     * Fast get op for function arguments and local variables.
     *
     * Pushes 'arguments[argno]' onto the stack.
     *
     *   Category: Variables and Scopes
     *   Type: Arguments
     *   Operands: uint16_t argno
     *   Stack: => arguments[argno]
     */ \
    MACRO(JSOP_GETARG, "getarg", NULL, 3, 0, 1, JOF_QARG|JOF_NAME) \
    /*
     * Pushes the value of local variable onto the stack.
     *
     *   Category: Variables and Scopes
     *   Type: Local Variables
     *   Operands: uint24_t localno
     *   Stack: => val
     */ \
    MACRO(JSOP_GETLOCAL, "getlocal", NULL, 4, 0, 1, JOF_LOCAL|JOF_NAME) \
    /*
     * Pushes aliased variable onto the stack.
     *
     * An "aliased variable" is a var, let, or formal arg that is aliased.
     * Sources of aliasing include: nested functions accessing the vars of an
     * enclosing function, function statements that are conditionally executed,
     * 'eval', 'with', and 'arguments'. All of these cases require creating a
     * CallObject to own the aliased variable.
     *
     * An ALIASEDVAR opcode contains the following immediates:
     *  uint8 hops: the number of environment objects to skip to find the
     *               EnvironmentObject containing the variable being accessed
     *  uint24 slot: the slot containing the variable in the EnvironmentObject
     *               (this 'slot' does not include RESERVED_SLOTS).
     *
     *   Category: Variables and Scopes
     *   Type: Aliased Variables
     *   Operands: uint8_t hops, uint24_t slot
     *   Stack: => aliasedVar
     */ \
    MACRO(JSOP_GETALIASEDVAR, "getaliasedvar", NULL, 5, 0, 1, JOF_ENVCOORD|JOF_NAME|JOF_TYPESET|JOF_IC) \
    /*
     * Gets the value of a module import by name and pushes it onto the stack.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: => val
     */ \
    MACRO(JSOP_GETIMPORT, "getimport", NULL, 5, 0, 1, JOF_ATOM|JOF_NAME|JOF_TYPESET|JOF_IC) \
    /*
     * Pops an environment, gets the value of a bound name on it. If the name
     * is not bound to the environment, throw a ReferenceError. Used in
     * conjunction with BINDNAME.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: env => v
     */ \
    MACRO(JSOP_GETBOUNDNAME, "getboundname", NULL, 5, 1, 1, JOF_ATOM|JOF_NAME|JOF_TYPESET|JOF_IC) \
    /*
     * Pushes the value of the intrinsic onto the stack.
     *
     * Intrinsic names are emitted instead of JSOP_*NAME ops when the
     * 'CompileOptions' flag 'selfHostingMode' is set.
     *
     * They are used in self-hosted code to access other self-hosted values and
     * intrinsic functions the runtime doesn't give client JS code access to.
     *
     *   Category: Variables and Scopes
     *   Type: Intrinsics
     *   Operands: uint32_t nameIndex
     *   Stack: => intrinsic[name]
     */ \
    MACRO(JSOP_GETINTRINSIC, "getintrinsic", NULL, 5, 0, 1, JOF_ATOM|JOF_NAME|JOF_TYPESET|JOF_IC) \
    /*
     * Pushes current callee onto the stack.
     *
     * Used for named function expression self-naming, if lightweight.
     *
     *   Category: Variables and Scopes
     *   Type: Arguments
     *   Operands:
     *   Stack: => callee
     */ \
    MACRO(JSOP_CALLEE, "callee", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Load the callee stored in a CallObject on the environment chain. The
     * numHops operand is the number of environment objects to skip on the
     * environment chain.
     *
     *   Category: Variables and Scopes
     *   Type: Arguments
     *   Operands: uint8_t numHops
     *   Stack: => callee
     */ \
    MACRO(JSOP_ENVCALLEE, "envcallee", NULL, 2, 0, 1, JOF_UINT8) \
    /*
     * Pops an environment and value from the stack, assigns value to the given
     * name, and pushes the value back on the stack
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: env, val => val
     */ \
    MACRO(JSOP_SETNAME, "setname", NULL, 5, 2, 1, JOF_ATOM|JOF_NAME|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY|JOF_IC) \
    /*
     * Pops a environment and value from the stack, assigns value to the given
     * name, and pushes the value back on the stack. If the set failed, then
     * throw a TypeError, per usual strict mode semantics.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: env, val => val
     */ \
    MACRO(JSOP_STRICTSETNAME, "strict-setname", NULL, 5, 2, 1, JOF_ATOM|JOF_NAME|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT|JOF_IC) \
    /*
     * Pops the top two values on the stack as 'val' and 'env', sets property
     * of 'env' as 'val' and pushes 'val' back on the stack.
     *
     * 'env' should be the global lexical environment unless the script has a
     * non-syntactic global scope, in which case acts like JSOP_SETNAME.
     *
     *   Category: Variables and Scopes
     *   Type: Free Variables
     *   Operands: uint32_t nameIndex
     *   Stack: env, val => val
     */ \
    MACRO(JSOP_SETGNAME, "setgname", NULL, 5, 2, 1, JOF_ATOM|JOF_NAME|JOF_PROPSET|JOF_DETECTING|JOF_GNAME|JOF_CHECKSLOPPY|JOF_IC) \
    /*
     * Pops the top two values on the stack as 'val' and 'env', sets property
     * of 'env' as 'val' and pushes 'val' back on the stack. Throws a TypeError
     * if the set fails, per strict mode semantics.
     *
     * 'env' should be the global lexical environment unless the script has a
     * non-syntactic global scope, in which case acts like JSOP_STRICTSETNAME.
     *
     *   Category: Variables and Scopes
     *   Type: Free Variables
     *   Operands: uint32_t nameIndex
     *   Stack: env, val => val
     */ \
    MACRO(JSOP_STRICTSETGNAME, "strict-setgname", NULL, 5, 2, 1, JOF_ATOM|JOF_NAME|JOF_PROPSET|JOF_DETECTING|JOF_GNAME|JOF_CHECKSTRICT|JOF_IC) \
    /*
     * Fast set op for function arguments and local variables.
     *
     * Sets 'arguments[argno]' as the top of stack value.
     *
     *   Category: Variables and Scopes
     *   Type: Arguments
     *   Operands: uint16_t argno
     *   Stack: v => v
     */ \
    MACRO(JSOP_SETARG, "setarg", NULL, 3, 1, 1, JOF_QARG|JOF_NAME) \
    /*
     * Stores the top stack value to the given local.
     *
     *   Category: Variables and Scopes
     *   Type: Local Variables
     *   Operands: uint24_t localno
     *   Stack: v => v
     */ \
    MACRO(JSOP_SETLOCAL, "setlocal", NULL, 4, 1, 1, JOF_LOCAL|JOF_NAME|JOF_DETECTING) \
    /*
     * Sets aliased variable as the top of stack value.
     *
     *   Category: Variables and Scopes
     *   Type: Aliased Variables
     *   Operands: uint8_t hops, uint24_t slot
     *   Stack: v => v
     */ \
    MACRO(JSOP_SETALIASEDVAR, "setaliasedvar", NULL, 5, 1, 1, JOF_ENVCOORD|JOF_NAME|JOF_PROPSET|JOF_DETECTING) \
    /*
     * Stores the top stack value in the specified intrinsic.
     *
     *   Category: Variables and Scopes
     *   Type: Intrinsics
     *   Operands: uint32_t nameIndex
     *   Stack: val => val
     */ \
    MACRO(JSOP_SETINTRINSIC, "setintrinsic", NULL, 5, 1, 1, JOF_ATOM|JOF_NAME|JOF_DETECTING) \
    /*
     * Pushes lexical environment onto the env chain.
     *
     *   Category: Variables and Scopes
     *   Type: Block-local Scope
     *   Operands: uint32_t scopeIndex
     *   Stack: =>
     */ \
    MACRO(JSOP_PUSHLEXICALENV, "pushlexicalenv", NULL, 5, 0, 0, JOF_SCOPE) \
    /*
     * Pops lexical environment from the env chain.
     *
     *   Category: Variables and Scopes
     *   Type: Block-local Scope
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_POPLEXICALENV, "poplexicalenv", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * The opcode to assist the debugger.
     *
     *   Category: Statements
     *   Type: Debugger
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_DEBUGLEAVELEXICALENV, "debugleavelexicalenv", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Recreates the current block on the env chain with a fresh block with
     * uninitialized bindings. This operation implements the behavior of
     * inducing a fresh lexical environment for every iteration of a for-in/of
     * loop whose loop-head has a (captured) lexical declaration.
     *
     *   Category: Variables and Scopes
     *   Type: Block-local Scope
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_RECREATELEXICALENV, "recreatelexicalenv", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Replaces the current block on the env chain with a fresh block that
     * copies all the bindings in the block. This operation implements the
     * behavior of inducing a fresh lexical environment for every iteration of
     * a for(let ...; ...; ...) loop, if any declarations induced by such a
     * loop are captured within the loop.
     *
     *   Category: Variables and Scopes
     *   Type: Block-local Scope
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_FRESHENLEXICALENV, "freshenlexicalenv", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Pushes a var environment onto the env chain.
     *
     *   Category: Variables and Scopes
     *   Type: Var Scope
     *   Operands: uint32_t scopeIndex
     *   Stack: =>
     */ \
    MACRO(JSOP_PUSHVARENV, "pushvarenv", NULL, 5, 0, 0, JOF_SCOPE) \
    /*
     * Pops a var environment from the env chain.
     *
     *   Category: Variables and Scopes
     *   Type: Var Scope
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_POPVARENV, "popvarenv", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Pops the top of stack value, converts it to an object, and adds a
     * 'WithEnvironmentObject' wrapping that object to the environment chain.
     *
     * There is a matching JSOP_LEAVEWITH instruction later. All name
     * lookups between the two that may need to consult the With object
     * are deoptimized.
     *
     *   Category: Statements
     *   Type: With Statement
     *   Operands: uint32_t staticWithIndex
     *   Stack: val =>
     */ \
    MACRO(JSOP_ENTERWITH, "enterwith", NULL, 5, 1, 0, JOF_SCOPE) \
    /*
     * Pops the environment chain object pushed by JSOP_ENTERWITH.
     *
     *   Category: Statements
     *   Type: With Statement
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_LEAVEWITH, "leavewith", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Pushes the nearest 'var' environment.
     *
     *   Category: Variables and Scopes
     *   Type: Free Variables
     *   Operands:
     *   Stack: => env
     */ \
    MACRO(JSOP_BINDVAR, "bindvar", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Defines the new binding on the frame's current variables-object (the
     * environment on the environment chain designated to receive new
     * variables).
     *
     * Throws if the current variables-object is the global object and a
     * binding with the same name exists on the global lexical environment.
     *
     * This is used for global scripts and also in some cases for function
     * scripts where use of dynamic scoping inhibits optimization.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: =>
     */ \
    MACRO(JSOP_DEFVAR, "defvar", NULL, 5, 0, 0, JOF_ATOM) \
    /*
     * Defines the given function on the current scope.
     *
     * This is used for global scripts and also in some cases for function
     * scripts where use of dynamic scoping inhibits optimization.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands:
     *   Stack: fun =>
     */ \
    MACRO(JSOP_DEFFUN, "deffun", NULL, 1, 1, 0, JOF_BYTE) \
    /*
     * Defines the new mutable binding on global lexical environment.
     *
     * Throws if a binding with the same name already exists on the
     * environment, or if a var binding with the same name exists on the
     * global.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: =>
     */ \
    MACRO(JSOP_DEFLET, "deflet", NULL, 5, 0, 0, JOF_ATOM) \
    /*
     * Defines the new constant binding on global lexical environment.
     *
     * Throws if a binding with the same name already exists on the
     * environment, or if a var binding with the same name exists on the
     * global.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: =>
     */ \
    MACRO(JSOP_DEFCONST, "defconst", NULL, 5, 0, 0, JOF_ATOM) \
    /*
     * Looks up name on the environment chain and deletes it, pushes 'true'
     * onto the stack if succeeded (if the property was present and deleted or
     * if the property wasn't present in the first place), 'false' if not.
     *
     * Strict mode code should never contain this opcode.
     *
     *   Category: Variables and Scopes
     *   Type: Variables
     *   Operands: uint32_t nameIndex
     *   Stack: => succeeded
     */ \
    MACRO(JSOP_DELNAME, "delname", NULL, 5, 0, 1, JOF_ATOM|JOF_NAME|JOF_CHECKSLOPPY) \
    /*
     * Pushes the 'arguments' object for the current function activation.
     *
     * If 'JSScript' is not marked 'needsArgsObj', then a
     * JS_OPTIMIZED_ARGUMENTS magic value is pushed. Otherwise, a proper
     * arguments object is constructed and pushed.
     *
     * This opcode requires that the function does not have rest parameter.
     *
     *   Category: Variables and Scopes
     *   Type: Arguments
     *   Operands:
     *   Stack: => arguments
     */ \
    MACRO(JSOP_ARGUMENTS, "arguments", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Creates rest parameter array for current function call, and pushes it
     * onto the stack.
     *
     *   Category: Variables and Scopes
     *   Type: Arguments
     *   Operands:
     *   Stack: => rest
     */ \
    MACRO(JSOP_REST, "rest", NULL, 1, 0, 1, JOF_BYTE|JOF_TYPESET|JOF_IC) \
    /*
     * Determines the 'this' value for current function frame and pushes it
     * onto the stack. Emitted in the prologue of functions with a
     * this-binding.
     *
     *   Category: Variables and Scopes
     *   Type: This
     *   Operands:
     *   Stack: => this
     */ \
    MACRO(JSOP_FUNCTIONTHIS, "functionthis", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Pops the top value off the stack.
     *
     *   Category: Operators
     *   Type: Stack Operations
     *   Operands:
     *   Stack: v =>
     */ \
    MACRO(JSOP_POP, "pop", NULL, 1, 1, 0, JOF_BYTE) \
    /*
     * Pops the top 'n' values from the stack.
     *
     *   Category: Operators
     *   Type: Stack Operations
     *   Operands: uint16_t n
     *   Stack: v[n-1], ..., v[1], v[0] =>
     *   nuses: n
     */ \
    MACRO(JSOP_POPN, "popn", NULL, 3, -1, 0, JOF_UINT16) \
    /*
     * Pushes a copy of the top value on the stack.
     *
     *   Category: Operators
     *   Type: Stack Operations
     *   Operands:
     *   Stack: v => v, v
     */ \
    MACRO(JSOP_DUP, "dup", NULL, 1, 1, 2, JOF_BYTE) \
    /*
     * Duplicates the top two values on the stack.
     *
     *   Category: Operators
     *   Type: Stack Operations
     *   Operands:
     *   Stack: v1, v2 => v1, v2, v1, v2
     */ \
    MACRO(JSOP_DUP2, "dup2", NULL, 1, 2, 4, JOF_BYTE) \
    /*
     * Duplicates the Nth value from the top onto the stack.
     *
     *   Category: Operators
     *   Type: Stack Operations
     *   Operands: uint24_t n
     *   Stack: v[n], v[n-1], ..., v[1], v[0] =>
     *          v[n], v[n-1], ..., v[1], v[0], v[n]
     */ \
    MACRO(JSOP_DUPAT, "dupat", NULL, 4, 0, 1, JOF_UINT24) \
    /*
     * Swaps the top two values on the stack. This is useful for things like
     * post-increment/decrement.
     *
     *   Category: Operators
     *   Type: Stack Operations
     *   Operands:
     *   Stack: v1, v2 => v2, v1
     */ \
    MACRO(JSOP_SWAP, "swap", NULL, 1, 2, 2, JOF_BYTE) \
    /*
     * Picks the nth element from the stack and moves it to the top of the
     * stack.
     *
     *   Category: Operators
     *   Type: Stack Operations
     *   Operands: uint8_t n
     *   Stack: v[n], v[n-1], ..., v[1], v[0] => v[n-1], ..., v[1], v[0], v[n]
     */ \
    MACRO(JSOP_PICK, "pick", NULL, 2, 0, 0, JOF_UINT8) \
    /*
     * Moves the top of the stack value under the nth element of the stack.
     * Note: n must NOT be 0.
     *
     *   Category: Operators
     *   Type: Stack Operations
     *   Operands: uint8_t n
     *   Stack: v[n], v[n-1], ..., v[1], v[0] => v[0], v[n], v[n-1], ..., v[1]
     */ \
    MACRO(JSOP_UNPICK, "unpick", NULL, 2, 0, 0, JOF_UINT8) \
    /*
     * No operation is performed.
     *
     *   Category: Other
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_NOP, "nop", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Embedded lineno to speedup 'pc->line' mapping.
     *
     *   Category: Other
     *   Operands: uint32_t lineno
     *   Stack: =>
     */ \
    MACRO(JSOP_LINENO, "lineno", NULL, 5, 0, 0, JOF_UINT32) \
    /*
     * No-op used by the decompiler to produce nicer error messages about
     * destructuring code.
     *
     *   Category: Other
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_NOP_DESTRUCTURING, "nop-destructuring", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * No-op bytecode only emitted in some self-hosted functions. Not handled
     * by the JITs or Baseline Interpreter so the script always runs in the C++
     * interpreter.
     *
     *   Category: Other
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_FORCEINTERPRETER, "forceinterpreter", NULL, 1, 0, 0, JOF_BYTE) \
    /*
     * Examines the top stack value, asserting that it's either a self-hosted
     * function or a self-hosted intrinsic. This opcode does nothing in a
     * non-debug build.
     *
     *   Category: Other
     *   Operands:
     *   Stack: checkVal => checkVal
     */ \
    MACRO(JSOP_DEBUGCHECKSELFHOSTED, "debug-checkselfhosted", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Pushes a boolean indicating if instrumentation is active.
     *   Category: Other
     *   Operands:
     *   Stack: => val
     */ \
    MACRO(JSOP_INSTRUMENTATION_ACTIVE, "instrumentationActive", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes the instrumentation callback for the current realm.
     *   Category: Other
     *   Operands:
     *   Stack: => val
     */ \
    MACRO(JSOP_INSTRUMENTATION_CALLBACK, "instrumentationCallback", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Pushes the current script's instrumentation ID.
     *   Category: Other
     *   Operands:
     *   Stack: => val
     */ \
    MACRO(JSOP_INSTRUMENTATION_SCRIPT_ID, "instrumentationScriptId", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Invokes debugger.
     *
     *   Category: Statements
     *   Type: Debugger
     *   Operands:
     *   Stack: =>
     */ \
    MACRO(JSOP_DEBUGGER, "debugger", NULL, 1, 0, 0, JOF_BYTE)

// clang-format on

/*
 * In certain circumstances it may be useful to "pad out" the opcode space to
 * a power of two.  Use this macro to do so.
 */
#define FOR_EACH_TRAILING_UNUSED_OPCODE(MACRO) \
  MACRO(238)                                   \
  MACRO(239)                                   \
  MACRO(240)                                   \
  MACRO(241)                                   \
  MACRO(242)                                   \
  MACRO(243)                                   \
  MACRO(244)                                   \
  MACRO(245)                                   \
  MACRO(246)                                   \
  MACRO(247)                                   \
  MACRO(248)                                   \
  MACRO(249)                                   \
  MACRO(250)                                   \
  MACRO(251)                                   \
  MACRO(252)                                   \
  MACRO(253)                                   \
  MACRO(254)                                   \
  MACRO(255)

namespace js {

// Sanity check that opcode values and trailing unused opcodes completely cover
// the [0, 256) range.  Avert your eyes!  You don't want to know how the
// sausage gets made.

// clang-format off
#define PLUS_ONE(...) \
    + 1
#define TRAILING_VALUE_AND_VALUE_PLUS_ONE(val) \
    val) && (val + 1 ==
static_assert((0 FOR_EACH_OPCODE(PLUS_ONE) ==
               FOR_EACH_TRAILING_UNUSED_OPCODE(TRAILING_VALUE_AND_VALUE_PLUS_ONE)
               256),
              "trailing unused opcode values monotonically increase "
              "from JSOP_LIMIT to 255");
#undef TRAILING_VALUE_AND_VALUE_PLUS_ONE
#undef PLUS_ONE
// clang-format on

// Define JSOP_*_LENGTH constants for all ops.
#define DEFINE_LENGTH_CONSTANT(op, name, image, len, ...) \
  constexpr size_t op##_LENGTH = len;
FOR_EACH_OPCODE(DEFINE_LENGTH_CONSTANT)
#undef DEFINE_LENGTH_CONSTANT

}  // namespace js

#endif  // vm_Opcodes_h

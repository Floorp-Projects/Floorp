/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=0 ft=c:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Opcodes_h
#define vm_Opcodes_h

#include "mozilla/Attributes.h"

#include <stddef.h>

/*
 * JavaScript operation bytecodes.  Add a new bytecode by claiming one of the
 * JSOP_UNUSED* here or by extracting the first unused opcode from
 * FOR_EACH_TRAILING_UNUSED_OPCODE and updating js::detail::LastDefinedOpcode
 * below.
 *
 * When changing the bytecode, don't forget to update XDR_BYTECODE_VERSION in
 * vm/Xdr.h!
 *
 * Includers must define a macro with the following form:
 *
 * #define MACRO(op,val,name,image,length,nuses,ndefs,format) ...
 *
 * Then simply use FOR_EACH_OPCODE(MACRO) to invoke MACRO for every opcode.
 * Selected arguments can be expanded in initializers.
 *
 * Field        Description
 * op           Bytecode name, which is the JSOp enumerator name
 * value        Bytecode value, which is the JSOp enumerator value
 * name         C string containing name for disassembler
 * image        C string containing "image" for pretty-printer, null if ugly
 * length       Number of bytes including any immediate operands
 * nuses        Number of stack slots consumed by bytecode, -1 if variadic
 * ndefs        Number of stack slots produced by bytecode, -1 if variadic
 * format       Bytecode plus immediate operand encoding format
 *
 * This file is best viewed with 128 columns:
12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
 */

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
 *     Arguments
 *   [Operator]
 *     Comparison Operators
 *     Arithmetic Operators
 *     Bitwise Logical Operators
 *     Bitwise Shift Operators
 *     Logical Operators
 *     Special Operators
 *     Stack Operations
 *   [Literals]
 *     Constants
 *     Object
 *     Array
 *     RegExp
 *   [Other]
 */

#define FOR_EACH_OPCODE(macro) \
    /* legend:  op      val   name          image       len use def  format */ \
    /*
     * No operation is performed.
     *   Category: Other
     *   Operands:
     *   Stack: =>
     */ \
    macro(JSOP_NOP,       0,  "nop",        NULL,         1,  0,  0, JOF_BYTE) \
    \
    /* Long-standing JavaScript bytecodes. */ \
    macro(JSOP_UNDEFINED, 1,  js_undefined_str, "",       1,  0,  1, JOF_BYTE) \
    macro(JSOP_UNUSED2,   2,  "unused2",    NULL,         1,  1,  0, JOF_BYTE) \
    macro(JSOP_ENTERWITH, 3,  "enterwith",  NULL,         5,  1,  0, JOF_OBJECT) \
    macro(JSOP_LEAVEWITH, 4,  "leavewith",  NULL,         1,  0,  0, JOF_BYTE) \
    macro(JSOP_RETURN,    5,  "return",     NULL,         1,  1,  0, JOF_BYTE) \
    macro(JSOP_GOTO,      6,  "goto",       NULL,         5,  0,  0, JOF_JUMP) \
    macro(JSOP_IFEQ,      7,  "ifeq",       NULL,         5,  1,  0, JOF_JUMP|JOF_DETECTING) \
    macro(JSOP_IFNE,      8,  "ifne",       NULL,         5,  1,  0, JOF_JUMP) \
    \
    /* Get the arguments object for the current, lightweight function activation. */ \
    macro(JSOP_ARGUMENTS, 9,  "arguments",  NULL,         1,  0,  1, JOF_BYTE) \
    \
    macro(JSOP_SWAP,      10, "swap",       NULL,         1,  2,  2, JOF_BYTE) \
    macro(JSOP_POPN,      11, "popn",       NULL,         3, -1,  0, JOF_UINT16) \
    \
    /* More long-standing bytecodes. */ \
    macro(JSOP_DUP,       12, "dup",        NULL,         1,  1,  2, JOF_BYTE) \
    macro(JSOP_DUP2,      13, "dup2",       NULL,         1,  2,  4, JOF_BYTE) \
    macro(JSOP_SETCONST,  14, "setconst",   NULL,         5,  1,  1, JOF_ATOM|JOF_NAME|JOF_SET) \
    macro(JSOP_BITOR,     15, "bitor",      "|",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_BITXOR,    16, "bitxor",     "^",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_BITAND,    17, "bitand",     "&",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_EQ,        18, "eq",         "==",         1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH|JOF_DETECTING) \
    macro(JSOP_NE,        19, "ne",         "!=",         1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH|JOF_DETECTING) \
    macro(JSOP_LT,        20, "lt",         "<",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_LE,        21, "le",         "<=",         1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_GT,        22, "gt",         ">",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_GE,        23, "ge",         ">=",         1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_LSH,       24, "lsh",        "<<",         1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_RSH,       25, "rsh",        ">>",         1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_URSH,      26, "ursh",       ">>>",        1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_ADD,       27, "add",        "+",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_SUB,       28, "sub",        "-",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_MUL,       29, "mul",        "*",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_DIV,       30, "div",        "/",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_MOD,       31, "mod",        "%",          1,  2,  1, JOF_BYTE|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_NOT,       32, "not",        "!",          1,  1,  1, JOF_BYTE|JOF_ARITH|JOF_DETECTING) \
    macro(JSOP_BITNOT,    33, "bitnot",     "~",          1,  1,  1, JOF_BYTE|JOF_ARITH) \
    macro(JSOP_NEG,       34, "neg",        "- ",         1,  1,  1, JOF_BYTE|JOF_ARITH) \
    macro(JSOP_POS,       35, "pos",        "+ ",         1,  1,  1, JOF_BYTE|JOF_ARITH) \
    macro(JSOP_DELNAME,   36, "delname",    NULL,         5,  0,  1, JOF_ATOM|JOF_NAME) \
    macro(JSOP_DELPROP,   37, "delprop",    NULL,         5,  1,  1, JOF_ATOM|JOF_PROP) \
    macro(JSOP_DELELEM,   38, "delelem",    NULL,         1,  2,  1, JOF_BYTE |JOF_ELEM) \
    macro(JSOP_TYPEOF,    39, js_typeof_str,NULL,         1,  1,  1, JOF_BYTE|JOF_DETECTING) \
    macro(JSOP_VOID,      40, js_void_str,  NULL,         1,  1,  1, JOF_BYTE) \
    \
    /* spreadcall variant of JSOP_CALL */ \
    macro(JSOP_SPREADCALL,41, "spreadcall", NULL,         1,  3,  1, JOF_BYTE|JOF_INVOKE|JOF_TYPESET) \
    /* spreadcall variant of JSOP_NEW */ \
    macro(JSOP_SPREADNEW, 42, "spreadnew",  NULL,         1,  3,  1, JOF_BYTE|JOF_INVOKE|JOF_TYPESET) \
    /* spreadcall variant of JSOP_EVAL */ \
    macro(JSOP_SPREADEVAL,43, "spreadeval", NULL,         1,  3,  1, JOF_BYTE|JOF_INVOKE|JOF_TYPESET) \
    \
    /* Dup the Nth value from the top. */ \
    macro(JSOP_DUPAT,     44, "dupat",      NULL,         4,  0,  1,  JOF_UINT24) \
    \
    macro(JSOP_UNUSED45,  45, "unused45",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED46,  46, "unused46",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED47,  47, "unused47",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED48,  48, "unused48",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED49,  49, "unused49",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED50,  50, "unused50",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED51,  51, "unused51",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED52,  52, "unused52",   NULL,         1,  0,  0,  JOF_BYTE) \
    \
    macro(JSOP_GETPROP,   53, "getprop",    NULL,         5,  1,  1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_TMPSLOT3) \
    macro(JSOP_SETPROP,   54, "setprop",    NULL,         5,  2,  1, JOF_ATOM|JOF_PROP|JOF_SET|JOF_DETECTING|JOF_TMPSLOT) \
    macro(JSOP_GETELEM,   55, "getelem",    NULL,         1,  2,  1, JOF_BYTE |JOF_ELEM|JOF_TYPESET|JOF_LEFTASSOC) \
    macro(JSOP_SETELEM,   56, "setelem",    NULL,         1,  3,  1, JOF_BYTE |JOF_ELEM|JOF_SET|JOF_DETECTING) \
    macro(JSOP_CALLNAME,  57, "callname",   NULL,         5,  0,  1, JOF_ATOM|JOF_NAME|JOF_TYPESET) \
    macro(JSOP_CALL,      58, "call",       NULL,         3, -1,  1, JOF_UINT16|JOF_INVOKE|JOF_TYPESET) \
    macro(JSOP_NAME,      59, "name",       NULL,         5,  0,  1, JOF_ATOM|JOF_NAME|JOF_TYPESET) \
    macro(JSOP_DOUBLE,    60, "double",     NULL,         5,  0,  1, JOF_DOUBLE) \
    macro(JSOP_STRING,    61, "string",     NULL,         5,  0,  1, JOF_ATOM) \
    macro(JSOP_ZERO,      62, "zero",       "0",          1,  0,  1, JOF_BYTE) \
    macro(JSOP_ONE,       63, "one",        "1",          1,  0,  1, JOF_BYTE) \
    macro(JSOP_NULL,      64, js_null_str,  js_null_str,  1,  0,  1, JOF_BYTE) \
    macro(JSOP_THIS,      65, js_this_str,  js_this_str,  1,  0,  1, JOF_BYTE) \
    macro(JSOP_FALSE,     66, js_false_str, js_false_str, 1,  0,  1, JOF_BYTE) \
    macro(JSOP_TRUE,      67, js_true_str,  js_true_str,  1,  0,  1, JOF_BYTE) \
    macro(JSOP_OR,        68, "or",         NULL,         5,  1,  1, JOF_JUMP|JOF_DETECTING|JOF_LEFTASSOC) \
    macro(JSOP_AND,       69, "and",        NULL,         5,  1,  1, JOF_JUMP|JOF_DETECTING|JOF_LEFTASSOC) \
    \
    /* The switch bytecodes have variable length. */ \
    macro(JSOP_TABLESWITCH, 70, "tableswitch", NULL,     -1,  1,  0,  JOF_TABLESWITCH|JOF_DETECTING) \
    \
    /*
     * Prologue emitted in scripts expected to run once, which deoptimizes code if
     * it executes multiple times.
     */ \
    macro(JSOP_RUNONCE,   71, "runonce",    NULL,         1,  0,  0,  JOF_BYTE) \
    \
    /* New, infallible/transitive identity ops. */ \
    macro(JSOP_STRICTEQ,  72, "stricteq",   "===",        1,  2,  1, JOF_BYTE|JOF_DETECTING|JOF_LEFTASSOC|JOF_ARITH) \
    macro(JSOP_STRICTNE,  73, "strictne",   "!==",        1,  2,  1, JOF_BYTE|JOF_DETECTING|JOF_LEFTASSOC|JOF_ARITH) \
    \
    /*
     * Sometimes web pages do 'o.Item(i) = j'. This is not an early SyntaxError,
     * for web compatibility. Instead we emit JSOP_SETCALL after the function call,
     * an opcode that always throws.
     */ \
    macro(JSOP_SETCALL,   74, "setcall",    NULL,         1,  0,  0, JOF_BYTE) \
    \
    /*
     * JSOP_ITER sets up a for-in or for-each-in loop using the JSITER_* flag bits
     * in this op's uint8_t immediate operand. It replaces the top of stack value
     * with an iterator for that value.
     *
     * JSOP_MOREITER stores the next iterated value into cx->iterValue and pushes
     * true if another value is available, and false otherwise. It is followed
     * immediately by JSOP_IFNE.
     *
     * JSOP_ENDITER cleans up after the loop. It uses the slot above the iterator
     * for temporary GC rooting.
     */ \
    macro(JSOP_ITER,      75, "iter",       NULL,         2,  1,  1,  JOF_UINT8) \
    macro(JSOP_MOREITER,  76, "moreiter",   NULL,         1,  1,  2,  JOF_BYTE) \
    macro(JSOP_ITERNEXT,  77, "iternext",   "<next>",     1,  0,  1,  JOF_BYTE) \
    macro(JSOP_ENDITER,   78, "enditer",    NULL,         1,  1,  0,  JOF_BYTE) \
    \
    macro(JSOP_FUNAPPLY,  79, "funapply",   NULL,         3, -1,  1,  JOF_UINT16|JOF_INVOKE|JOF_TYPESET) \
    \
    /* Push object initializer literal. */ \
    macro(JSOP_OBJECT,    80, "object",     NULL,         5,  0,  1,  JOF_OBJECT) \
    \
    /* Pop value and discard it. */ \
    macro(JSOP_POP,       81, "pop",        NULL,         1,  1,  0,  JOF_BYTE) \
    \
    /* Call a function as a constructor; operand is argc. */ \
    macro(JSOP_NEW,       82, js_new_str,   NULL,         3, -1,  1,  JOF_UINT16|JOF_INVOKE|JOF_TYPESET) \
    \
    macro(JSOP_SPREAD,    83, "spread",     NULL,         1,  3,  2,  JOF_BYTE|JOF_ELEM|JOF_SET) \
    \
    /* Fast get/set ops for function arguments and local variables. */ \
    macro(JSOP_GETARG,    84, "getarg",     NULL,         3,  0,  1,  JOF_QARG |JOF_NAME) \
    macro(JSOP_SETARG,    85, "setarg",     NULL,         3,  1,  1,  JOF_QARG |JOF_NAME|JOF_SET) \
    macro(JSOP_GETLOCAL,  86,"getlocal",    NULL,         4,  0,  1,  JOF_LOCAL|JOF_NAME) \
    macro(JSOP_SETLOCAL,  87,"setlocal",    NULL,         4,  1,  1,  JOF_LOCAL|JOF_NAME|JOF_SET|JOF_DETECTING) \
    \
    /* Push unsigned 16-bit int constant. */ \
    macro(JSOP_UINT16,    88, "uint16",     NULL,         3,  0,  1,  JOF_UINT16) \
    \
    /*
     * Object and array literal support.  NEWINIT takes the kind of initializer
     * (JSProto_Array or JSProto_Object).  NEWARRAY is an array initializer
     * taking the final length, which can be filled in at the start and initialized
     * directly.  NEWOBJECT is an object initializer taking an object with the final
     * shape, which can be set at the start and slots then filled in directly.
     * NEWINIT has an extra byte so it can be exchanged with NEWOBJECT during emit.
     */ \
    macro(JSOP_NEWINIT,   89, "newinit",    NULL,         5,  0,  1, JOF_UINT8) \
    macro(JSOP_NEWARRAY,  90, "newarray",   NULL,         4,  0,  1, JOF_UINT24) \
    macro(JSOP_NEWOBJECT, 91, "newobject",  NULL,         5,  0,  1, JOF_OBJECT) \
    macro(JSOP_ENDINIT,   92, "endinit",    NULL,         1,  0,  0, JOF_BYTE) \
    macro(JSOP_INITPROP,  93, "initprop",   NULL,         5,  2,  1, JOF_ATOM|JOF_PROP|JOF_SET|JOF_DETECTING) \
    \
    /* Initialize a numeric property in an object literal, like {1: x}. */ \
    macro(JSOP_INITELEM,  94, "initelem",   NULL,         1,  3,  1, JOF_BYTE|JOF_ELEM|JOF_SET|JOF_DETECTING) \
    \
    /* Used in array literals with spread. */ \
    macro(JSOP_INITELEM_INC,95, "initelem_inc", NULL,     1,  3,  2, JOF_BYTE|JOF_ELEM|JOF_SET) \
    \
    /* Initialize an array element. */ \
    macro(JSOP_INITELEM_ARRAY,96, "initelem_array", NULL, 4,  2,  1,  JOF_UINT24|JOF_ELEM|JOF_SET|JOF_DETECTING) \
    \
    /*
     * Initialize a getter/setter in an object literal. The INITELEM* ops are used
     * for numeric properties like {get 2() {}}.
     */ \
    macro(JSOP_INITPROP_GETTER,  97, "initprop_getter",   NULL, 5,  2,  1, JOF_ATOM|JOF_PROP|JOF_SET|JOF_DETECTING) \
    macro(JSOP_INITPROP_SETTER,  98, "initprop_setter",   NULL, 5,  2,  1, JOF_ATOM|JOF_PROP|JOF_SET|JOF_DETECTING) \
    macro(JSOP_INITELEM_GETTER,  99, "initelem_getter",   NULL, 1,  3,  1, JOF_BYTE|JOF_ELEM|JOF_SET|JOF_DETECTING) \
    macro(JSOP_INITELEM_SETTER, 100, "initelem_setter",   NULL, 1,  3,  1, JOF_BYTE|JOF_ELEM|JOF_SET|JOF_DETECTING) \
    \
    macro(JSOP_UNUSED101,  101, "unused101",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED102,  102, "unused102",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED103,  103, "unused103",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED104,  104, "unused104",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED105,  105, "unused105",   NULL,         1,  0,  0,  JOF_BYTE) \
    \
    /* The argument is the offset to the next statement and is used by IonMonkey. */ \
    macro(JSOP_LABEL,     106,"label",     NULL,          5,  0,  0,  JOF_JUMP) \
    \
    macro(JSOP_UNUSED107, 107,"unused107",  NULL,         1,  0,  0,  JOF_BYTE) \
    \
    /* Like JSOP_FUNAPPLY but for f.call instead of f.apply. */ \
    macro(JSOP_FUNCALL,   108,"funcall",    NULL,         3, -1,  1, JOF_UINT16|JOF_INVOKE|JOF_TYPESET) \
    \
    /* This opcode is the target of the backwards jump for some loop. */ \
    macro(JSOP_LOOPHEAD,  109,"loophead",   NULL,         1,  0,  0,  JOF_BYTE) \
    \
    /* ECMA-compliant assignment ops. */ \
    macro(JSOP_BINDNAME,  110,"bindname",   NULL,         5,  0,  1,  JOF_ATOM|JOF_NAME|JOF_SET) \
    macro(JSOP_SETNAME,   111,"setname",    NULL,         5,  2,  1,  JOF_ATOM|JOF_NAME|JOF_SET|JOF_DETECTING) \
    \
    /* Exception handling ops. */ \
    macro(JSOP_THROW,     112,js_throw_str, NULL,         1,  1,  0,  JOF_BYTE) \
    \
    /* 'in' and 'instanceof' ops. */ \
    macro(JSOP_IN,        113,js_in_str,    js_in_str,    1,  2,  1, JOF_BYTE|JOF_LEFTASSOC) \
    macro(JSOP_INSTANCEOF,114,js_instanceof_str,js_instanceof_str,1,2,1,JOF_BYTE|JOF_LEFTASSOC|JOF_TMPSLOT) \
    \
    /* debugger op */ \
    macro(JSOP_DEBUGGER,  115,"debugger",   NULL,         1,  0,  0, JOF_BYTE) \
    \
    /* gosub/retsub for finally handling */ \
    macro(JSOP_GOSUB,     116,"gosub",      NULL,         5,  0,  0,  JOF_JUMP) \
    macro(JSOP_RETSUB,    117,"retsub",     NULL,         1,  2,  0,  JOF_BYTE) \
    \
    /* More exception handling ops. */ \
    macro(JSOP_EXCEPTION, 118,"exception",  NULL,         1,  0,  1,  JOF_BYTE) \
    \
    /* Embedded lineno to speedup pc->line mapping. */ \
    macro(JSOP_LINENO,    119,"lineno",     NULL,         3,  0,  0,  JOF_UINT16) \
    \
    /*
     * ECMA-compliant switch statement ops.
     * CONDSWITCH is a decompilable NOP; CASE is ===, POP, jump if true, re-push
     * lval if false; and DEFAULT is POP lval and GOTO.
     */ \
    macro(JSOP_CONDSWITCH,120,"condswitch", NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_CASE,      121,"case",       NULL,         5,  2,  1,  JOF_JUMP) \
    macro(JSOP_DEFAULT,   122,"default",    NULL,         5,  1,  0,  JOF_JUMP) \
    \
    /*
     * ECMA-compliant call to eval op
     */ \
    macro(JSOP_EVAL,      123,"eval",       NULL,         3, -1,  1, JOF_UINT16|JOF_INVOKE|JOF_TYPESET) \
    \
    macro(JSOP_UNUSED124,  124, "unused124", NULL,      1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED125,  125, "unused125", NULL,      1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED126,  126, "unused126", NULL,      1,  0,  0,  JOF_BYTE) \
    \
    /* Prolog bytecodes for defining function, var, and const names. */ \
    macro(JSOP_DEFFUN,    127,"deffun",     NULL,         5,  0,  0,  JOF_OBJECT) \
    macro(JSOP_DEFCONST,  128,"defconst",   NULL,         5,  0,  0,  JOF_ATOM) \
    macro(JSOP_DEFVAR,    129,"defvar",     NULL,         5,  0,  0,  JOF_ATOM) \
    \
    /* Push a closure for a named or anonymous function expression. */ \
    macro(JSOP_LAMBDA,    130, "lambda",    NULL,         5,  0,  1, JOF_OBJECT) \
    macro(JSOP_LAMBDA_ARROW, 131, "lambda_arrow", NULL,   5,  1,  1, JOF_OBJECT) \
    \
    /* Used for named function expression self-naming, if lightweight. */ \
    macro(JSOP_CALLEE,    132, "callee",    NULL,         1,  0,  1, JOF_BYTE) \
    \
    /* Pick an element from the stack. */ \
    macro(JSOP_PICK,        133, "pick",      NULL,       2,  0,  0,  JOF_UINT8|JOF_TMPSLOT2) \
    \
    /*
     * Exception handling no-op, for more economical byte-coding than SRC_TRYFIN
     * srcnote-annotated JSOP_NOPs and to simply stack balance handling.
     */ \
    macro(JSOP_TRY,         134,"try",        NULL,       1,  0,  0,  JOF_BYTE) \
    macro(JSOP_FINALLY,     135,"finally",    NULL,       1,  0,  2,  JOF_BYTE) \
    \
    /*
     * An "aliased variable" is a var, let, or formal arg that is aliased. Sources
     * of aliasing include: nested functions accessing the vars of an enclosing
     * function, function statements that are conditionally executed, 'eval',
     * 'with', and 'arguments'. All of these cases require creating a CallObject to
     * own the aliased variable.
     *
     * An ALIASEDVAR opcode contains the following immediates:
     *  uint8 hops:  the number of scope objects to skip to find the ScopeObject
     *               containing the variable being accessed
     *  uint24 slot: the slot containing the variable in the ScopeObject (this
     *               'slot' does not include RESERVED_SLOTS).
     */ \
    macro(JSOP_GETALIASEDVAR, 136,"getaliasedvar",NULL,   5,  0,  1, JOF_SCOPECOORD|JOF_NAME|JOF_TYPESET) \
    macro(JSOP_CALLALIASEDVAR,137,"callaliasedvar",NULL,  5,  0,  1, JOF_SCOPECOORD|JOF_NAME|JOF_TYPESET) \
    macro(JSOP_SETALIASEDVAR, 138,"setaliasedvar",NULL,   5,  1,  1, JOF_SCOPECOORD|JOF_NAME|JOF_SET|JOF_DETECTING) \
    \
    macro(JSOP_UNUSED139,  139, "unused139",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED140,  140, "unused140",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED141,  141, "unused141",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED142,  142, "unused142",   NULL,         1,  0,  0,  JOF_BYTE) \
    \
    /*
     * Intrinsic names are emitted instead of JSOP_*NAME ops when the
     * CompileOptions flag "selfHostingMode" is set.
     *
     * They are used in self-hosted code to access other self-hosted values and
     * intrinsic functions the runtime doesn't give client JS code access to.
     */ \
    macro(JSOP_GETINTRINSIC,  143, "getintrinsic",  NULL, 5,  0,  1, JOF_ATOM|JOF_NAME|JOF_TYPESET) \
    macro(JSOP_CALLINTRINSIC, 144, "callintrinsic", NULL, 5,  0,  1, JOF_ATOM|JOF_NAME|JOF_TYPESET) \
    macro(JSOP_SETINTRINSIC,  145, "setintrinsic",  NULL, 5,  2,  1, JOF_ATOM|JOF_NAME|JOF_SET|JOF_DETECTING) \
    macro(JSOP_BINDINTRINSIC, 146, "bindintrinsic", NULL, 5,  0,  1, JOF_ATOM|JOF_NAME|JOF_SET) \
    \
    /* Unused. */ \
    macro(JSOP_UNUSED147,     147,"unused147", NULL,      1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED148,     148,"unused148", NULL,      1,  0,  0,  JOF_BYTE) \
    \
    /* Placeholders for a real jump opcode set during backpatch chain fixup. */ \
    macro(JSOP_BACKPATCH,     149,"backpatch", NULL,      5,  0,  0,  JOF_JUMP) \
    macro(JSOP_UNUSED150,     150,"unused150", NULL,      1,  0,  0,  JOF_BYTE) \
    \
    /* Set pending exception from the stack, to trigger rethrow. */ \
    macro(JSOP_THROWING,      151,"throwing", NULL,       1,  1,  0,  JOF_BYTE) \
    \
    /* Set the return value pseudo-register in stack frame. */ \
    macro(JSOP_SETRVAL,       152,"setrval",    NULL,       1,  1,  0,  JOF_BYTE) \
    /*
     * Stop interpretation and return value set by JSOP_SETRVAL. When not set,
     * returns UndefinedValue. Also emitted at end of script so interpreter
     * don't need to check if opcode is still in script range.
     */ \
    macro(JSOP_RETRVAL,       153,"retrval",    NULL,       1,  0,  0,  JOF_BYTE) \
    \
    /* Free variable references that must either be found on the global or a ReferenceError */ \
    macro(JSOP_GETGNAME,      154,"getgname",  NULL,       5,  0,  1, JOF_ATOM|JOF_NAME|JOF_TYPESET|JOF_GNAME) \
    macro(JSOP_SETGNAME,      155,"setgname",  NULL,       5,  2,  1, JOF_ATOM|JOF_NAME|JOF_SET|JOF_DETECTING|JOF_GNAME) \
    \
    macro(JSOP_UNUSED156,  156, "unused156",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED157,  157, "unused157",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED158,  158, "unused158",   NULL,         1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED159,  159, "unused159",   NULL,         1,  0,  0,  JOF_BYTE) \
    \
    /* Regular expression literal requiring special "fork on exec" handling. */ \
    macro(JSOP_REGEXP,        160,"regexp",   NULL,       5,  0,  1, JOF_REGEXP) \
    \
    macro(JSOP_UNUSED161,     161,"unused161",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED162,     162,"unused162",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED163,     163,"unused163",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED164,     164,"unused164",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED165,     165,"unused165",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED166,     166,"unused166",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED167,     167,"unused167",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED168,     168,"unused168",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED169,     169,"unused169",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED170,     170,"unused170",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED171,     171,"unused171",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED172,     172,"unused172",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED173,     173,"unused173",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED174,     174,"unused174",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED175,     175,"unused175",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED176,     176,"unused176",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED177,     177,"unused177",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED178,     178,"unused178",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED179,     179,"unused179",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED180,     180,"unused180",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED181,     181,"unused181",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED182,     182,"unused182",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED183,     183,"unused183",  NULL,     1,  0,  0,  JOF_BYTE) \
    \
    macro(JSOP_CALLPROP,      184,"callprop",   NULL,     5,  1,  1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_TMPSLOT3) \
    \
    macro(JSOP_UNUSED185,     185,"unused185",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED186,     186,"unused186",  NULL,     1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED187,     187,"unused187",  NULL,     1,  0,  0,  JOF_BYTE) \
    \
    /* Opcode to hold 24-bit immediate integer operands. */ \
    macro(JSOP_UINT24,        188,"uint24",     NULL,     4,  0,  1, JOF_UINT24) \
    \
    macro(JSOP_UNUSED189,     189,"unused189",   NULL,    1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED190,     190,"unused190",   NULL,    1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED191,     191,"unused191",   NULL,    1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED192,     192,"unused192",   NULL,    1,  0,  0,  JOF_BYTE) \
    \
    macro(JSOP_CALLELEM,      193, "callelem",   NULL,    1,  2,  1, JOF_BYTE |JOF_ELEM|JOF_TYPESET|JOF_LEFTASSOC) \
    \
    /* __proto__: v inside an object initializer. */ \
    macro(JSOP_MUTATEPROTO,   194, "mutateproto",NULL,    1,  2,  1, JOF_BYTE) \
    \
    /*
     * Get an extant property value, throwing ReferenceError if the identified
     * property does not exist.
     */ \
    macro(JSOP_GETXPROP,      195,"getxprop",    NULL,    5,  1,  1, JOF_ATOM|JOF_PROP|JOF_TYPESET) \
    \
    macro(JSOP_UNUSED196,     196,"unused196",   NULL,    1,  0,  0, JOF_BYTE) \
    \
    /* Specialized JSOP_TYPEOF to avoid reporting undefined for typeof(0, undef). */ \
    macro(JSOP_TYPEOFEXPR,    197,"typeofexpr",  NULL,    1,  1,  1, JOF_BYTE|JOF_DETECTING) \
    \
    /* Block-local scope support. */ \
    macro(JSOP_PUSHBLOCKSCOPE,198,"pushblockscope", NULL, 5,  0,  0,  JOF_OBJECT) \
    macro(JSOP_POPBLOCKSCOPE, 199,"popblockscope", NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_DEBUGLEAVEBLOCK, 200,"debugleaveblock", NULL, 1,  0,  0,  JOF_BYTE) \
    \
    macro(JSOP_UNUSED201,     201,"unused201",  NULL,     1,  0,  0,  JOF_BYTE) \
    \
    /* Generator and array comprehension support. */ \
    macro(JSOP_GENERATOR,     202,"generator",   NULL,    1,  0,  0,  JOF_BYTE) \
    macro(JSOP_YIELD,         203,"yield",       NULL,    1,  1,  1,  JOF_BYTE) \
    macro(JSOP_ARRAYPUSH,     204,"arraypush",   NULL,    1,  2,  0,  JOF_BYTE) \
    \
    macro(JSOP_UNUSED205,     205, "unused205",    NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED206,     206, "unused206",    NULL,  1,  0,  0,  JOF_BYTE) \
    \
    macro(JSOP_UNUSED207,     207, "unused207",    NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED208,     208, "unused208",    NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED209,     209, "unused209",    NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED210,     210, "unused210",    NULL,  1,  0,  0,  JOF_BYTE) \
    \
    macro(JSOP_CALLGNAME,     211, "callgname",    NULL,  5,  0,  1,  JOF_ATOM|JOF_NAME|JOF_TYPESET|JOF_GNAME) \
    macro(JSOP_CALLLOCAL,     212, "calllocal",    NULL,  4,  0,  1,  JOF_LOCAL|JOF_NAME) \
    macro(JSOP_CALLARG,       213, "callarg",      NULL,  3,  0,  1,  JOF_QARG |JOF_NAME) \
    macro(JSOP_BINDGNAME,     214, "bindgname",    NULL,  5,  0,  1,  JOF_ATOM|JOF_NAME|JOF_SET|JOF_GNAME) \
    \
    /* Opcodes to hold 8-bit and 32-bit immediate integer operands. */ \
    macro(JSOP_INT8,          215, "int8",         NULL,  2,  0,  1, JOF_INT8) \
    macro(JSOP_INT32,         216, "int32",        NULL,  5,  0,  1, JOF_INT32) \
    \
    /* Get the value of the 'length' property from a stacked value. */ \
    macro(JSOP_LENGTH,        217, "length",       NULL,  5,  1,  1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_TMPSLOT3) \
    \
    /*
     * Push a JSVAL_HOLE value onto the stack, representing an omitted property in
     * an array literal (e.g. property 0 in the array [, 1]).  This opcode is used
     * with the JSOP_NEWARRAY opcode.
     */ \
    macro(JSOP_HOLE,          218, "hole",         NULL,  1,  0,  1,  JOF_BYTE) \
    \
    macro(JSOP_UNUSED219,     219,"unused219",     NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED220,     220,"unused220",     NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED221,     221,"unused221",     NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED222,     222,"unused222",     NULL,  1,  0,  0,  JOF_BYTE) \
    macro(JSOP_UNUSED223,     223,"unused223",     NULL,  1,  0,  0,  JOF_BYTE) \
    \
    macro(JSOP_REST,          224, "rest",         NULL,  1,  0,  1,  JOF_BYTE|JOF_TYPESET) \
    \
    /* Pop the stack, convert to a jsid (int or string), and push back. */ \
    macro(JSOP_TOID,          225, "toid",         NULL,  1,  1,  1,  JOF_BYTE) \
    \
    /* Push the implicit 'this' value for calls to the associated name. */ \
    macro(JSOP_IMPLICITTHIS,  226, "implicitthis", "",    5,  0,  1,  JOF_ATOM) \
    \
    /*
     * This opcode is the target of the entry jump for some loop. The uint8
     * argument is a bitfield. The lower 7 bits of the argument indicate the
     * loop depth. This value starts at 1 and is just a hint: deeply nested
     * loops all have the same value.  The upper bit is set if Ion should be
     * able to OSR at this point, which is true unless there is non-loop state
     * on the stack.
     */ \
    macro(JSOP_LOOPENTRY,     227, "loopentry",    NULL,  2,  0,  0,  JOF_UINT8)

/*
 * In certain circumstances it may be useful to "pad out" the opcode space to
 * a power of two.  Use this macro to do so.
 */
#define FOR_EACH_TRAILING_UNUSED_OPCODE(macro) \
    macro(228) \
    macro(229) \
    macro(230) \
    macro(231) \
    macro(232) \
    macro(233) \
    macro(234) \
    macro(235) \
    macro(236) \
    macro(237) \
    macro(238) \
    macro(239) \
    macro(240) \
    macro(241) \
    macro(242) \
    macro(243) \
    macro(244) \
    macro(245) \
    macro(246) \
    macro(247) \
    macro(248) \
    macro(249) \
    macro(250) \
    macro(251) \
    macro(252) \
    macro(253) \
    macro(254) \
    macro(255)

namespace js {

// Sanity check that opcode values and trailing unused opcodes completely cover
// the [0, 256) range.  Avert your eyes!  You don't want to know how the
// sausage gets made.

#define VALUE_AND_VALUE_PLUS_ONE(op, val, ...) \
    val) && (val + 1 ==
#define TRAILING_VALUE_AND_VALUE_PLUS_ONE(val) \
    val) && (val + 1 ==
static_assert((0 ==
               FOR_EACH_OPCODE(VALUE_AND_VALUE_PLUS_ONE)
               FOR_EACH_TRAILING_UNUSED_OPCODE(TRAILING_VALUE_AND_VALUE_PLUS_ONE)
               256),
              "opcode values and trailing unused opcode values monotonically "
              "increase from zero to 255");
#undef TRAILING_VALUE_AND_VALUE_PLUS_ONE
#undef VALUE_AND_VALUE_PLUS_ONE

// Define JSOP_*_LENGTH constants for all ops.
#define DEFINE_LENGTH_CONSTANT(op, val, name, image, len, ...) \
    MOZ_CONSTEXPR_VAR size_t op##_LENGTH = len;
FOR_EACH_OPCODE(DEFINE_LENGTH_CONSTANT)
#undef DEFINE_LENGTH_CONSTANT

} // namespace js

#endif // vm_Opcodes_h

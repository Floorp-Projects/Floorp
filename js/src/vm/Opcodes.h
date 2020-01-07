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
 * SpiderMonkey bytecode instructions.
 *
 * To use this header, define a macro of the form:
 *
 *     #define MACRO(op, name, token, length, nuses, ndefs, format) ...
 *
 * Then `FOR_EACH_OPCODE(MACRO)` invokes `MACRO` for every opcode.
 *
 * Field        Description
 * -----        -----------
 * op           Bytecode name, which is the JSOp enumerator name
 * name         C string containing name for disassembler
 * token        Pretty-printer string, or null if ugly
 * length       Number of bytes including any immediate operands
 * nuses        Number of stack slots consumed by bytecode, -1 if variadic
 * ndefs        Number of stack slots produced by bytecode
 * format       JOF_ flags describing instruction operand layout, etc.
 *
 * For more about `format`, see the comments on the `JOF_` constants defined in
 * BytecodeUtil.h.
 *
 *
 * [SMDOC] Bytecode Invariants
 *
 * Creating scripts that do not follow the rules can lead to undefined
 * behavior. Bytecode has many consumers, not just the interpreter: JITs,
 * analyses, the debugger. That's why the rules below apply even to code that
 * can't be reached in ordinary execution (such as code after an infinite loop
 * or inside an `if (false)` block).
 *
 * The `code()` of a script must be a packed (not aligned) sequence of valid
 * instructions from start to end. Each instruction has a single byte opcode
 * followed by a number of operand bytes based on the opcode.
 *
 * ## Jump instructions
 *
 * Operands named `offset` or `forwardOffset` are jump offsets, the distance in
 * bytes from the start of the current instruction to the start of another
 * instruction in the same script. Operands named `forwardOffset` must be
 * positive.
 *
 * Forward jumps must jump to a `JSOP_JUMPTARGET` instruction.  Backward jumps,
 * indicated by negative offsets, must jump to a `JSOP_LOOPHEAD` instruction.
 * Jump offsets can't be zero.
 *
 * Needless to say, scripts must not contain overlapping instruction sequences
 * (in the sense of <https://en.wikipedia.org/wiki/Overlapping_gene>).
 *
 * A script's `trynotes` and `scopeNotes` impose further constraints. Each try
 * note and each scope note marks a region of the bytecode where some invariant
 * holds, or some cleanup behavior is needed--that there's a for-in iterator in
 * a particular stack slot, for instance, which must be closed on error. All
 * paths into the span must establish that invariant. In practice, this means
 * other code never jumps into the span: the only way in is to execute the
 * bytecode instruction that sets up the invariant (in our example,
 * `JSOP_ITER`).
 *
 * If a script's `trynotes` (see "Try Notes" in JSScript.h) contain a
 * `JSTRY_CATCH` or `JSTRY_FINALLY` span, there must be a `JSOP_TRY`
 * instruction immediately before the span and a `JSOP_JUMPTARGET immediately
 * after it. Instructions must not jump to this `JSOP_JUMPTARGET`. (The VM puts
 * us there on exception.) Furthermore, the instruction sequence immediately
 * following a `JSTRY_CATCH` span must read `JUMPTARGET; EXCEPTION` or, in
 * non-function scripts, `JUMPTARGET; UNDEFINED; SETRVAL; EXCEPTION`. (These
 * instructions run with an exception pending; other instructions aren't
 * designed to handle that.)
 *
 * Unreachable instructions are allowed, but they have to follow all the rules.
 *
 * Control must not reach the end of a script. (Currently, the last instruction
 * is always JSOP_RETRVAL.)
 *
 * ## Other operands
 *
 * Operands named `nameIndex` or `atomIndex` (which appear on instructions that
 * have `JOF_ATOM` in the `format` field) must be valid indexes into
 * `script->atoms()`.
 *
 * Operands named `argc` (`JOF_ARGC`) are argument counts for call
 * instructions. `argc` must be small enough that the instruction's nuses is <=
 * the current stack depth (see "Stack depth" below).
 *
 * Operands named `argno` (`JOF_QARG`) refer to an argument of the current
 * function. `argno` must be in the range `0..script->function()->nargs()`.
 * Instructions with these operands must appear only in function scripts.
 *
 * Operands named `localno` (`JOF_LOCAL`) refer to a local variable stored in
 * the stack frame. `localno` must be in the range `0..script->nfixed()`.
 *
 * Operands named `resumeIndex` (`JOF_RESUMEINDEX`) refer to a resume point in
 * the current script. `resumeIndex` must be a valid index into
 * `script->resumeOffsets()`.
 *
 * Operands named `hops` and `slot` (`JOF_ENVCOORD`) refer a slot in an
 * `EnvironmentObject`. At run time, they must point to a fixed slot in an
 * object on the current environment chain. See `EnvironmentCoordinates`.
 *
 * Operands with the following names must be valid indexes into
 * `script->gcthings()`, and the pointer in the vector must point to the right
 * type of thing:
 *
 * -   `objectIndex` (`JOF_OBJECT`): `PlainObject*` or `ArrayObject*`
 * -   `baseobjIndex` (`JOF_OBJECT`): `PlainObject*`
 * -   `funcIndex` (`JOF_OBJECT`): `JSFunction*`
 * -   `regexpIndex` (`JOF_REGEXP`): `RegExpObject*`
 * -   `scopeIndex` (`JOF_SCOPE`): `Scope*`
 * -   `lexicalScopeIndex` (`JOF_SCOPE`): `LexicalScope*`
 * -   `withScopeIndex` (`JOF_SCOPE`): `WithScope*`
 * -   `bigIntIndex` (`JOF_BIGINT`): `BigInt*`
 *
 * Operands named `icIndex` (`JOF_ICINDEX`) must be exactly the number of
 * preceding instructions in the script that have the JOF_IC flag.
 * (Rationale: Each JOF_IC instruction has a unique entry in
 * `script->jitScript()->icEntries()`.  At run time, in the bytecode
 * interpreter, we have to find that entry. We could store the IC index as an
 * operand to each JOF_IC instruction, but it's more memory-efficient to use a
 * counter and reset the counter to `icIndex` after each jump.)
 *
 * ## Stack depth
 *
 * Each instruction has a compile-time stack depth, the number of values on the
 * interpreter stack just before executing the instruction. It isn't explicitly
 * present in the bytecode itself, but (for reachable instructions, anyway)
 * it's a function of the bytecode.
 *
 * -   The first instruction has stack depth 0.
 *
 * -   Each successor of an instruction X has a stack depth equal to
 *
 *         X's stack depth - `js::StackUses(X)` + `js::StackDefs(X)`
 *
 *     except for `JSOP_CASE` (below).
 *
 *     X's "successors" are: the next instruction in the script, if
 *     `js::FlowsIntoNext(op)` is true for X's opcode; one or more
 *     `JSOP_JUMPTARGET`s elsewhere, if X is a forward jump or
 *     `JSOP_TABLESWITCH`; and/or a `JSOP_LOOPHEAD` if it's a backward jump.
 *
 * -   `JSOP_CASE` is a special case because its stack behavior is eccentric.
 *     The formula above is correct for the next instruction. The jump target
 *     has a stack depth that is 1 less.
 *
 * -   See `JSOP_GOSUB` for another special case.
 *
 * -   The `JSOP_JUMPTARGET` instruction immediately following a `JSTRY_CATCH`
 *     or `JSTRY_FINALLY` span has the same stack depth as the `JSOP_TRY`
 *     instruction that precedes the span.
 *
 *     Every instruction covered by the `JSTRY_CATCH` or `JSTRY_FINALLY` span
 *     must have a stack depth >= that value, so that error recovery is
 *     guaranteed to find enough values on the stack to resume there.
 *
 * -   `script->nslots() - script->nfixed()` must be >= the maximum stack
 *     depth of any instruction in `script`.  (The stack frame must be big
 *     enough to run the code.)
 *
 * `BytecodeParser::parse()` computes stack depths for every reachable
 * instruction in a script.
 *
 * ## Scopes and environments
 *
 * As with stack depth, each instruction has a static scope, which is a
 * compile-time characterization of the eventual run-time environment chain
 * when that instruction executes. Just as every instruction has a stack budget
 * (nuses/ndefs), every instruction either pushes a scope, pops a scope, or
 * neither. The same successor relation applies as above.
 *
 * Every scope used in a script is stored in the `JSScript::gcthings()` vector.
 * They can be accessed using `getScope(index)` if you know what `index` to
 * pass.
 *
 * The scope of every instruction (that's reachable via the successor relation)
 * is given in two independent ways: by the bytecode itself and by the scope
 * notes. The two sources must agree.
 *
 * ## Further rules
 *
 * All reachable instructions must be reachable without taking any backward
 * edges.
 *
 * Instructions with the `JOF_CHECKSLOPPY` flag must not be used in strict mode
 * code. `JOF_CHECKSTRICT` instructions must not be used in nonstrict code.
 *
 * Many instructions have their own additional rules. These are documented on
 * the various opcodes below (look for the word "must").
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
     * Push `undefined`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => undefined
     */ \
    MACRO(JSOP_UNDEFINED, js_undefined_str, "", 1, 0, 1, JOF_BYTE) \
    /*
     * Push `null`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => null
     */ \
    MACRO(JSOP_NULL, js_null_str, js_null_str, 1, 0, 1, JOF_BYTE) \
    /*
     * Push a boolean constant.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => true/false
     */ \
    MACRO(JSOP_FALSE, js_false_str, js_false_str, 1, 0, 1, JOF_BYTE) \
    MACRO(JSOP_TRUE, js_true_str, js_true_str, 1, 0, 1, JOF_BYTE) \
    /*
     * Push the `int32_t` immediate operand as an `Int32Value`.
     *
     * `JSOP_ZERO`, `JSOP_ONE`, `JSOP_INT8`, `JSOP_UINT16`, and `JSOP_UINT24`
     * are all compact encodings for `JSOP_INT32`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: int32_t val
     *   Stack: => val
     */ \
    MACRO(JSOP_INT32, "int32", NULL, 5, 0, 1, JOF_INT32) \
    /*
     * Push the number `0`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => 0
     */ \
    MACRO(JSOP_ZERO, "zero", "0", 1, 0, 1, JOF_BYTE) \
    /*
     * Push the number `1`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands:
     *   Stack: => 1
     */ \
    MACRO(JSOP_ONE, "one", "1", 1, 0, 1, JOF_BYTE) \
    /*
     * Push the `int8_t` immediate operand as an `Int32Value`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: int8_t val
     *   Stack: => val
     */ \
    MACRO(JSOP_INT8, "int8", NULL, 2, 0, 1, JOF_INT8) \
    /*
     * Push the `uint16_t` immediate operand as an `Int32Value`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint16_t val
     *   Stack: => val
     */ \
    MACRO(JSOP_UINT16, "uint16", NULL, 3, 0, 1, JOF_UINT16) \
    /*
     * Push the `uint24_t` immediate operand as an `Int32Value`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint24_t val
     *   Stack: => val
     */ \
    MACRO(JSOP_UINT24, "uint24", NULL, 4, 0, 1, JOF_UINT24) \
    /*
     * Push the 64-bit floating-point immediate operand as a `DoubleValue`.
     *
     * If the operand is a NaN, it must be the canonical NaN (see
     * `JS::detail::CanonicalizeNaN`).
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: double val
     *   Stack: => val
     */ \
    MACRO(JSOP_DOUBLE, "double", NULL, 9, 0, 1, JOF_DOUBLE) \
    /*
     * Push the BigInt constant `script->getBigInt(bigIntIndex)`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint32_t bigIntIndex
     *   Stack: => bigint
     */ \
    MACRO(JSOP_BIGINT, "bigint", NULL, 5, 0, 1, JOF_BIGINT) \
    /*
     * Push the string constant `script->getAtom(atomIndex)`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint32_t atomIndex
     *   Stack: => string
     */ \
    MACRO(JSOP_STRING, "string", NULL, 5, 0, 1, JOF_ATOM) \
    /*
     * Push a well-known symbol.
     *
     * `symbol` must be in range for `JS::SymbolCode`.
     *
     *   Category: Literals
     *   Type: Constants
     *   Operands: uint8_t symbol (the JS::SymbolCode of the symbol to use)
     *   Stack: => symbol
     */ \
    MACRO(JSOP_SYMBOL, "symbol", NULL, 2, 0, 1, JOF_UINT8) \
    /*
     * Pop the top value on the stack, discard it, and push `undefined`.
     *
     * Implements: [The `void` operator][1], step 3.
     *
     * [1]: https://tc39.es/ecma262/#sec-void-operator
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: val => undefined
     */ \
    MACRO(JSOP_VOID, js_void_str, NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * [The `typeof` operator][1].
     *
     * Infallible. The result is always a string that depends on the [type][2]
     * of `val`.
     *
     * `JSOP_TYPEOF` and `JSOP_TYPEOFEXPR` are the same except
     * that--amazingly--`JSOP_TYPEOF` affects the behavior of an immediately
     * *preceding* `JSOP_GETNAME` or `JSOP_GETGNAME` instruction! This is how
     * we implement [`typeof`][1] step 2, making `typeof nonExistingVariable`
     * return `"undefined"` instead of throwing a ReferenceError.
     *
     * In a global scope:
     *
     * -   `typeof x` compiles to `GETGNAME "x"; TYPEOF`.
     * -   `typeof (0, x)` compiles to `GETGNAME "x"; TYPEOFEXPR`.
     *
     * Emitting the same bytecode for these two expressions would be a bug.
     * Per spec, the latter throws a ReferenceError if `x` doesn't exist.
     *
     * [1]: https://tc39.es/ecma262/#sec-typeof-operator
     * [2]: https://tc39.es/ecma262/#sec-ecmascript-language-types
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: val => (typeof val)
     */ \
    MACRO(JSOP_TYPEOF, js_typeof_str, NULL, 1, 1, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    MACRO(JSOP_TYPEOFEXPR, "typeofexpr", NULL, 1, 1, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    /*
     * [The unary `+` operator][1].
     *
     * `+val` doesn't do any actual math. It just calls [ToNumber][2](val).
     *
     * The conversion can call `.toString()`/`.valueOf()` methods and can
     * throw. The result on success is always a Number. (Per spec, unary `-`
     * supports BigInts, but unary `+` does not.)
     *
     * [1]: https://tc39.es/ecma262/#sec-unary-plus-operator
     * [2]: https://tc39.es/ecma262/#sec-tonumber
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => (+val)
     */ \
    MACRO(JSOP_POS, "pos", "+ ", 1, 1, 1, JOF_BYTE) \
    /*
     * [The unary `-` operator][1].
     *
     * Convert `val` to a numeric value, then push `-val`. The conversion can
     * call `.toString()`/`.valueOf()` methods and can throw. The result on
     * success is always numeric.
     *
     * [1]: https://tc39.es/ecma262/#sec-unary-minus-operator
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => (-val)
     */ \
    MACRO(JSOP_NEG, "neg", "- ", 1, 1, 1, JOF_BYTE|JOF_IC) \
    /*
     * [The bitwise NOT operator][1] (`~`).
     *
     * `val` is converted to an integer, then bitwise negated. The conversion
     * can call `.toString()`/`.valueOf()` methods and can throw. The result on
     * success is always an Int32 or BigInt value.
     *
     * [1]: https://tc39.es/ecma262/#sec-bitwise-not-operator
     *
     *   Category: Operators
     *   Type: Bitwise Logical Operators
     *   Operands:
     *   Stack: val => (~val)
     */ \
    MACRO(JSOP_BITNOT, "bitnot", "~", 1, 1, 1, JOF_BYTE|JOF_IC) \
    /*
     * [The logical NOT operator][1] (`!`).
     *
     * `val` is first converted with [ToBoolean][2], then logically
     * negated. The result is always a boolean value. This does not call
     * user-defined methods and can't throw.
     *
     * [1]: https://tc39.es/ecma262/#sec-logical-not-operator
     * [2]: https://tc39.es/ecma262/#sec-toboolean
     *
     *   Category: Operators
     *   Type: Logical Operators
     *   Operands:
     *   Stack: val => (!val)
     */ \
    MACRO(JSOP_NOT, "not", "!", 1, 1, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    /*
     * [Binary bitwise operations][1] (`|`, `^`, `&`).
     *
     * The arguments are converted to integers first. The conversion can call
     * `.toString()`/`.valueOf()` methods and can throw. The result on success
     * is always an Int32 or BigInt Value.
     *
     * [1]: https://tc39.es/ecma262/#sec-binary-bitwise-operators
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
     * Loose equality operators (`==` and `!=`).
     *
     * Pop two values, compare them, and push the boolean result. The
     * comparison may perform conversions that call `.toString()`/`.valueOf()`
     * methods and can throw.
     *
     * Implements: [Abstract Equality Comparison][1].
     *
     * [1]: https://tc39.es/ecma262/#sec-abstract-equality-comparison
     *
     *   Category: Operators
     *   Type: Comparison Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_EQ, "eq", "==", 1, 2, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    MACRO(JSOP_NE, "ne", "!=", 1, 2, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    /*
     * Strict equality operators (`===` and `!==`).
     *
     * Pop two values, check whether they're equal, and push the boolean
     * result. This does not call user-defined methods and can't throw
     * (except possibly due to OOM while flattening a string).
     *
     * Implements: [Strict Equality Comparison][1].
     *
     * [1]: https://tc39.es/ecma262/#sec-strict-equality-comparison
     *
     *   Category: Operators
     *   Type: Comparison Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_STRICTEQ, "stricteq", "===", 1, 2, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    MACRO(JSOP_STRICTNE, "strictne", "!==", 1, 2, 1, JOF_BYTE|JOF_DETECTING|JOF_IC) \
    /*
     * Relative operators (`<`, `>`, `<=`, `>=`).
     *
     * Pop two values, compare them, and push the boolean result. The
     * comparison may perform conversions that call `.toString()`/`.valueOf()`
     * methods and can throw.
     *
     * Implements: [Relational Operators: Evaluation][1].
     *
     * [1]: https://tc39.es/ecma262/#sec-relational-operators-runtime-semantics-evaluation
     *
     *   Category: Operators
     *   Type: Comparison Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_LT, "lt", "<",  1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_GT, "gt", ">",  1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_LE, "le", "<=", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_GE, "ge", ">=", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * [The `instanceof` operator][1].
     *
     * This throws a `TypeError` if `target` is not an object. It calls
     * `target[Symbol.hasInstance](value)` if the method exists. On success,
     * the result is always a boolean value.
     *
     * [1]: https://tc39.es/ecma262/#sec-instanceofoperator
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: value, target => (value instanceof target)
     */ \
    MACRO(JSOP_INSTANCEOF, js_instanceof_str, js_instanceof_str, 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * [The `in` operator][1].
     *
     * Push `true` if `obj` has a property with the key `id`. Otherwise push `false`.
     *
     * This throws a `TypeError` if `obj` is not an object. This can fire
     * proxy hooks and can throw. On success, the result is always a boolean
     * value.
     *
     * [1]: https://tc39.es/ecma262/#sec-relational-operators-runtime-semantics-evaluation
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: id, obj => (id in obj)
     */ \
    MACRO(JSOP_IN, js_in_str, js_in_str, 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * [Bitwise shift operators][1] (`<<`, `>>`, `>>>`).
     *
     * Pop two values, convert them to integers, perform a bitwise shift, and
     * push the result.
     *
     * Conversion can call `.toString()`/`.valueOf()` methods and can throw.
     * The result on success is always an Int32 or BigInt Value.
     *
     * [1]: https://tc39.es/ecma262/#sec-bitwise-shift-operators
     *
     *   Category: Operators
     *   Type: Bitwise Shift Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_LSH, "lsh", "<<", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_RSH, "rsh", ">>", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_URSH, "ursh", ">>>", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * [The binary `+` operator][1].
     *
     * Pop two values, convert them to primitive values, add them, and push the
     * result. If both values are numeric, add them; if either is a
     * string, do string concatenation instead.
     *
     * The conversion can call `.toString()`/`.valueOf()` methods and can throw.
     *
     * [1]: https://tc39.es/ecma262/#sec-addition-operator-plus-runtime-semantics-evaluation
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: lval, rval => (lval + rval)
     */ \
    MACRO(JSOP_ADD, "add", "+", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * [The binary `-` operator][1].
     *
     * Pop two values, convert them to numeric values, subtract the top value
     * from the other one, and push the result.
     *
     * The conversion can call `.toString()`/`.valueOf()` methods and can
     * throw. On success, the result is always numeric.
     *
     * [1]: https://tc39.es/ecma262/#sec-subtraction-operator-minus-runtime-semantics-evaluation
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: lval, rval => (lval - rval)
     */ \
    MACRO(JSOP_SUB, "sub", "-", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Add or subtract 1.
     *
     * `val` must already be a numeric value, such as the result of
     * `JSOP_TONUMERIC`.
     *
     * Implements: [The `++` and `--` operators][1], step 3 of each algorithm.
     *
     * [1]: https://tc39.es/ecma262/#sec-postfix-increment-operator
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => (val +/- 1)
     */ \
    MACRO(JSOP_INC, "inc", NULL, 1, 1, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_DEC, "dec", NULL, 1, 1, 1, JOF_BYTE|JOF_IC) \
    /*
     * [The multiplicative operators][1] (`*`, `/`, `%`).
     *
     * Pop two values, convert them to numeric values, do math, and push the
     * result.
     *
     * The conversion can call `.toString()`/`.valueOf()` methods and can
     * throw. On success, the result is always numeric.
     *
     * [1]: https://tc39.es/ecma262/#sec-multiplicative-operators-runtime-semantics-evaluation
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: lval, rval => (lval OP rval)
     */ \
    MACRO(JSOP_MUL, "mul", "*", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_DIV, "div", "/", 1, 2, 1, JOF_BYTE|JOF_IC) \
    MACRO(JSOP_MOD, "mod", "%", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * [The exponentiation operator][1] (`**`).
     *
     * Pop two values, convert them to numeric values, do exponentiation, and
     * push the result. The top value is the exponent.
     *
     * The conversion can call `.toString()`/`.valueOf()` methods and can
     * throw. This throws a RangeError if both values are BigInts and the
     * exponent is negative.
     *
     * [1]: https://tc39.es/ecma262/#sec-exp-operator
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: lval, rval => (lval ** rval)
     */ \
    MACRO(JSOP_POW, "pow", "**", 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Convert a value to a property key.
     *
     * Implements: [ToPropertyKey][1], except that if the result would be the
     * string representation of some integer in the range 0..2^31, we push the
     * corresponding Int32 value instead. This is because the spec insists that
     * array indices are strings, whereas for us they are integers.
     *
     * This is used for code like `++obj[index]`, which must do both a
     * `JSOP_GETELEM` and a `JSOP_SETELEM` with the same property key. Both
     * instructions would convert `index` to a property key for us, but the
     * spec says to convert it only once.
     *
     * The conversion can call `.toString()`/`.valueOf()` methods and can
     * throw.
     *
     * [1]: https://tc39.es/ecma262/#sec-topropertykey
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: propertyNameValue => propertyKey
     */ \
    MACRO(JSOP_TOID, "toid", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Convert a value to a numeric value (a Number or BigInt).
     *
     * Implements: [ToNumeric][1](val).
     *
     * Note: This is used to implement [`++` and `--`][2]. Surprisingly, it's
     * not possible to get the right behavior using `JSOP_ADD` and `JSOP_SUB`
     * alone. For one thing, `JSOP_ADD` sometimes does string concatenation,
     * while `++` always does numeric addition. More fundamentally, the result
     * of evaluating `--x` is ToNumeric(old value of `x`), a value that the
     * sequence `GETLOCAL "x"; ONE; SUB; SETLOCAL "x"` does not give us.
     *
     * [1]: https://tc39.es/ecma262/#sec-tonumeric
     * [2]: https://tc39.es/ecma262/#sec-postfix-increment-operator
     *
     *   Category: Operators
     *   Type: Arithmetic Operators
     *   Operands:
     *   Stack: val => ToNumeric(val)
     */ \
    MACRO(JSOP_TONUMERIC, "tonumeric", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Convert a value to a string.
     *
     * Implements: [ToString][1](val).
     *
     * Note: This is used in code for template literals, like `${x}${y}`. Each
     * substituted value must be converted using ToString. `JSOP_ADD` by itself
     * would do a slightly wrong kind of conversion (hint="number" rather than
     * hint="string").
     *
     * [1]: https://tc39.es/ecma262/#sec-tostring
     *
     *   Category: Other
     *   Operands:
     *   Stack: val => ToString(val)
     */ \
    MACRO(JSOP_TOSTRING, "tostring", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Push the global `this` value. Not to be confused with the `globalThis`
     * property on the global.
     *
     * This must be used only in scopes where `this` refers to the global
     * `this`.
     *
     *   Category: Variables and Scopes
     *   Type: This
     *   Operands:
     *   Stack: => this
     */ \
    MACRO(JSOP_GLOBALTHIS, "globalthis", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Push the value of `new.target`.
     *
     * The result is a constructor or `undefined`.
     *
     * This must be used only in scripts where `new.target` is allowed:
     * non-arrow function scripts and other scripts that have a non-arrow
     * function script on the scope chain.
     *
     * Implements: [GetNewTarget][1].
     *
     * [1]: https://tc39.es/ecma262/#sec-getnewtarget
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
     * Implements: [Import Calls][1].
     *
     * [1]: https://tc39.es/ecma262/#sec-import-calls
     *
     *   Category: Variables and Scopes
     *   Type: Modules
     *   Operands:
     *   Stack: moduleId => promise
     */ \
    MACRO(JSOP_DYNAMIC_IMPORT, "dynamic-import", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Push the `import.meta` object.
     *
     * This must be used only in module code.
     *
     *   Category: Variables and Scopes
     *   Type: Modules
     *   Operands:
     *   Stack: => import.meta
     */ \
    MACRO(JSOP_IMPORTMETA, "importmeta", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Create and push a new object with no properties.
     *
     * (This opcode has 4 unused bytes so it can be easily turned into
     * `JSOP_NEWOBJECT` during bytecode generation.)
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t _unused
     *   Stack: => obj
     */ \
    MACRO(JSOP_NEWINIT, "newinit", NULL, 5, 0, 1, JOF_UINT32|JOF_IC) \
    /*
     * Create and push a new object of a predetermined shape.
     *
     * The new object has the shape of the template object
     * `script->getObject(baseobjIndex)`. Subsequent `INITPROP` instructions
     * must fill in all slots of the new object before it is used in any other
     * way.
     *
     * For `JSOP_NEWOBJECT`, the new object has a group based on the allocation
     * site (or a new group if the template's group is a singleton). For
     * `JSOP_NEWOBJECT_WITHGROUP`, the new object has the same group as the
     * template object.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t baseobjIndex
     *   Stack: => obj
     */ \
    MACRO(JSOP_NEWOBJECT, "newobject", NULL, 5, 0, 1, JOF_OBJECT|JOF_IC) \
    MACRO(JSOP_NEWOBJECT_WITHGROUP, "newobjectwithgroup", NULL, 5, 0, 1, JOF_OBJECT|JOF_IC) \
    /*
     * Push a preconstructed object.
     *
     * Going one step further than `JSOP_NEWOBJECT`, this instruction doesn't
     * just reuse the shape--it actually pushes the preconstructed object
     * `script->getObject(objectIndex)` right onto the stack. The object must
     * be a singleton `PlainObject` or `ArrayObject`.
     *
     * The spec requires that an *ObjectLiteral* or *ArrayLiteral* creates a
     * new object every time it's evaluated, so this instruction must not be
     * used anywhere it might be executed more than once.
     *
     * There's a shell-only option, `newGlobal({cloneSingletons: true})`, that
     * makes this instruction do a deep copy of the object. A few tests use it.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t objectIndex
     *   Stack: => obj
     */ \
    MACRO(JSOP_OBJECT, "object", NULL, 5, 0, 1, JOF_OBJECT) \
    /*
     * Create and push a new ordinary object with the provided [[Prototype]].
     *
     * This is used to create the `.prototype` object for derived classes.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: proto => obj
     */ \
    MACRO(JSOP_OBJWITHPROTO, "objwithproto", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Define a data property on an object.
     *
     * `obj` must be an object.
     *
     * Implements: [CreateDataPropertyOrThrow][1] as used in
     * [PropertyDefinitionEvaluation][2] of regular and shorthand
     * *PropertyDefinition*s.
     *
     *    [1]: https://tc39.es/ecma262/#sec-createdatapropertyorthrow
     *    [2]: https://tc39.es/ecma262/#sec-object-initializer-runtime-semantics-propertydefinitionevaluation
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITPROP, "initprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Like `JSOP_INITPROP`, but define a non-enumerable property.
     *
     * This is used to define class methods.
     *
     * Implements: [PropertyDefinitionEvaluation][1] for methods, steps 3 and
     * 4, when *enumerable* is false.
     *
     *    [1]: https://tc39.es/ecma262/#sec-method-definitions-runtime-semantics-propertydefinitionevaluation
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITHIDDENPROP, "inithiddenprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Like `JSOP_INITPROP`, but define a non-enumerable, non-writable,
     * non-configurable property.
     *
     * This is used to define the `.prototype` property on classes.
     *
     * Implements: [MakeConstructor][1], step 8, when *writablePrototype* is
     * false.
     *
     *    [1]: https://tc39.es/ecma262/#sec-makeconstructor
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => obj
     */ \
    MACRO(JSOP_INITLOCKEDPROP, "initlockedprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Define a data property on `obj` with property key `id` and value `val`.
     *
     * Implements: [CreateDataPropertyOrThrow][1]. This instruction is used for
     * object literals like `{0: val}` and `{[id]: val}`, and methods like
     * `*[Symbol.iterator]() {}`.
     *
     * `JSOP_INITHIDDENELEM` is the same but defines a non-enumerable property,
     * for class methods.
     *
     *    [1]: https://tc39.es/ecma262/#sec-createdatapropertyorthrow
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, val => obj
     */ \
    MACRO(JSOP_INITELEM, "initelem", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    MACRO(JSOP_INITHIDDENELEM, "inithiddenelem", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Define an accessor property on `obj` with the given `getter`.
     * `nameIndex` gives the property name.
     *
     * `JSOP_INITHIDDENPROP_GETTER` is the same but defines a non-enumerable
     * property, for getters in classes.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, getter => obj
     */ \
    MACRO(JSOP_INITPROP_GETTER, "initprop_getter", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING) \
    MACRO(JSOP_INITHIDDENPROP_GETTER, "inithiddenprop_getter", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Define an accessor property on `obj` with property key `id` and the given `getter`.
     *
     * This is used to implement getters like `get [id]() {}` or `get 0() {}`.
     *
     * `JSOP_INITHIDDENELEM_GETTER` is the same but defines a non-enumerable
     * property, for getters in classes.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, getter => obj
     */ \
    MACRO(JSOP_INITELEM_GETTER, "initelem_getter", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING) \
    MACRO(JSOP_INITHIDDENELEM_GETTER, "inithiddenelem_getter", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Define an accessor property on `obj` with the given `setter`.
     *
     * This is used to implement ordinary setters like `set foo(v) {}`.
     *
     * `JSOP_INITHIDDENPROP_SETTER` is the same but defines a non-enumerable
     * property, for setters in classes.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, setter => obj
     */ \
    MACRO(JSOP_INITPROP_SETTER, "initprop_setter", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING) \
    MACRO(JSOP_INITHIDDENPROP_SETTER, "inithiddenprop_setter", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Define an accesssor property on `obj` with property key `id` and the
     * given `setter`.
     *
     * This is used to implement setters with computed property keys or numeric
     * keys.
     *
     * `JSOP_INITHIDDENELEM_SETTER` is the same but defines a non-enumerable
     * property, for setters in classes.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, id, setter => obj
     */ \
    MACRO(JSOP_INITELEM_SETTER, "initelem_setter", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING) \
    MACRO(JSOP_INITHIDDENELEM_SETTER, "inithiddenelem_setter", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING) \
    /*
     * Get the value of the property `obj.name`. This can call getters and
     * proxy traps.
     *
     * `JSOP_CALLPROP` is exactly like `JSOP_GETPROP` but hints to the VM that we're
     * getting a method in order to call it.
     *
     * Implements: [GetV][1], [GetValue][2] step 5.
     *
     * [1]: https://tc39.es/ecma262/#sec-getv
     * [2]: https://tc39.es/ecma262/#sec-getvalue
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj => obj[name]
     */ \
    MACRO(JSOP_GETPROP, "getprop", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_IC) \
    MACRO(JSOP_CALLPROP, "callprop", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_IC) \
    /*
     * Get the value of the property `obj[key]`.
     *
     * `JSOP_CALLELEM` is exactly like `JSOP_GETELEM` but hints to the VM that
     * we're getting a method in order to call it.
     *
     * Implements: [GetV][1], [GetValue][2] step 5.
     *
     * [1]: https://tc39.es/ecma262/#sec-getv
     * [2]: https://tc39.es/ecma262/#sec-getvalue
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, key => obj[key]
     */ \
    MACRO(JSOP_GETELEM, "getelem", NULL, 1, 2, 1, JOF_BYTE|JOF_ELEM|JOF_TYPESET|JOF_IC) \
    MACRO(JSOP_CALLELEM, "callelem", NULL, 1, 2, 1, JOF_BYTE|JOF_ELEM|JOF_TYPESET|JOF_IC) \
    /*
     * Push the value of `obj.length`.
     *
     * `nameIndex` must be the index of the atom `"length"`. This then behaves
     * exactly like `JSOP_GETPROP`.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands: uint32_t nameIndex
     *   Stack: obj => obj.length
     */ \
    MACRO(JSOP_LENGTH, "length", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_IC) \
    /*
     * Non-strict assignment to a property, `obj.name = val`.
     *
     * This throws a TypeError if `obj` is null or undefined. If it's a
     * primitive value, the property is set on ToObject(`obj`), typically with
     * no effect.
     *
     * Implements: [PutValue][1] step 6 for non-strict code.
     *
     * [1]: https://tc39.es/ecma262/#sec-putvalue
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => val
     */ \
    MACRO(JSOP_SETPROP, "setprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY|JOF_IC) \
    /*
     * Like `JSOP_SETPROP`, but for strict mode code. Throw a TypeError if
     * `obj[key]` exists but is non-writable, if it's an accessor property with
     * no setter, or if `obj` is a primitive value.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: obj, val => val
     */ \
    MACRO(JSOP_STRICTSETPROP, "strict-setprop", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT|JOF_IC) \
    /*
     * Non-strict assignment to a property, `obj[key] = val`.
     *
     * Implements: [PutValue][1] step 6 for non-strict code.
     *
     * [1]: https://tc39.es/ecma262/#sec-putvalue
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, key, val => val
     */ \
    MACRO(JSOP_SETELEM, "setelem", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY|JOF_IC) \
    /*
     * Like `JSOP_SETELEM`, but for strict mode code. Throw a TypeError if
     * `obj[key]` exists but is non-writable, if it's an accessor property with
     * no setter, or if `obj` is a primitive value.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, key, val => val
     */ \
    MACRO(JSOP_STRICTSETELEM, "strict-setelem", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT|JOF_IC) \
    /*
     * Delete a property from `obj`. Push true on success, false if the
     * property existed but could not be deleted. This implements `delete
     * obj.name` in non-strict code.
     *
     * Throws if `obj` is null or undefined. Can call proxy traps.
     *
     * Implements: [`delete obj.propname`][1] step 5 in non-strict code.
     *
     * [1]: https://tc39.es/ecma262/#sec-delete-operator-runtime-semantics-evaluation
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands: uint32_t nameIndex
     *   Stack: obj => succeeded
     */ \
    MACRO(JSOP_DELPROP, "delprop", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_CHECKSLOPPY) \
    /*
     * Like `JSOP_DELPROP`, but for strict mode code. Push `true` on success,
     * else throw a TypeError.
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands: uint32_t nameIndex
     *   Stack: obj => succeeded
     */ \
    MACRO(JSOP_STRICTDELPROP, "strict-delprop", NULL, 5, 1, 1, JOF_ATOM|JOF_PROP|JOF_CHECKSTRICT) \
    /*
     * Delete the property `obj[key]` and push `true` on success, `false`
     * if the property existed but could not be deleted.
     *
     * This throws if `obj` is null or undefined. Can call proxy traps.
     *
     * Implements: [`delete obj[key]`][1] step 5 in non-strict code.
     *
     * [1]: https://tc39.es/ecma262/#sec-delete-operator-runtime-semantics-evaluation
     *
     *   Category: Operators
     *   Type: Special Operators
     *   Operands:
     *   Stack: obj, key => succeeded
     */ \
    MACRO(JSOP_DELELEM, "delelem", NULL, 1, 2, 1, JOF_BYTE|JOF_ELEM|JOF_CHECKSLOPPY) \
    /*
     * Like `JSOP_DELELEM, but for strict mode code. Push `true` on success,
     * else throw a TypeError.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, key => succeeded
     */ \
    MACRO(JSOP_STRICTDELELEM, "strict-delelem", NULL, 1, 2, 1, JOF_BYTE|JOF_ELEM|JOF_CHECKSTRICT) \
    /*
     * Push true if `obj` has an own property `id`.
     *
     * Note that `obj` is the top value, like `JSOP_IN`.
     *
     * This opcode is not used for normal JS. Self-hosted code uses it by
     * calling the intrinsic `hasOwn(id, obj)`. For example,
     * `Object.prototype.hasOwnProperty` is implemented this way (see
     * js/src/builtin/Object.js).
     *
     *   Category: Other
     *   Type:
     *   Operands:
     *   Stack: id, obj => (obj.hasOwnProperty(id))
     */ \
    MACRO(JSOP_HASOWN, "hasown", NULL, 1, 2, 1, JOF_BYTE|JOF_IC) \
    /*
     * Push the SuperBase of the method `callee`. The SuperBase is
     * `callee`.[[HomeObject]].[[GetPrototypeOf]](), the object where `super`
     * property lookups should begin.
     *
     * `callee` must be a function that has a HomeObject that's an object,
     * typically produced by `JSOP_CALLEE` or `JSOP_ENVCALLEE`.
     *
     * Implements: [GetSuperBase][1], except that instead of the environment,
     * the argument supplies the callee.
     *
     * [1]: https://tc39.es/ecma262/#sec-getsuperbase
     *
     *   Category: Variables and Scopes
     *   Type: Super
     *   Operands:
     *   Stack: callee => superBase
     */ \
    MACRO(JSOP_SUPERBASE, "superbase", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Get the value of `receiver.name`, starting the property search at `obj`.
     * In spec terms, obj.[[Get]](name, receiver).
     *
     * Implements: [GetValue][1] for references created by [`super.name`][2].
     * The `receiver` is `this` and `obj` is the SuperBase of the enclosing
     * method.
     *
     * [1]: https://tc39.es/ecma262/#sec-getvalue
     * [2]: https://tc39.es/ecma262/#sec-super-keyword-runtime-semantics-evaluation
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: receiver, obj => super.name
     */ \
    MACRO(JSOP_GETPROP_SUPER, "getprop-super", NULL, 5, 2, 1, JOF_ATOM|JOF_PROP|JOF_TYPESET|JOF_IC) \
    /*
     * Get the value of `receiver[key]`, starting the property search at `obj`.
     * In spec terms, obj.[[Get]](key, receiver).
     *
     * Implements: [GetValue][1] for references created by [`super[key]`][2]
     * (where the `receiver` is `this` and `obj` is the SuperBase of the enclosing
     * method); [`Reflect.get(obj, key, receiver)`][3].
     *
     * [1]: https://tc39.es/ecma262/#sec-getvalue
     * [2]: https://tc39.es/ecma262/#sec-super-keyword-runtime-semantics-evaluation
     * [3]: https://tc39.es/ecma262/#sec-reflect.get
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: receiver, key, obj => super[key]
     */ \
    MACRO(JSOP_GETELEM_SUPER, "getelem-super", NULL, 1, 3, 1, JOF_BYTE|JOF_ELEM|JOF_TYPESET|JOF_IC) \
    /*
     * Assign `val` to `receiver.name`, starting the search for an existing
     * property at `obj`. In spec terms, obj.[[Set]](name, val, receiver).
     *
     * Implements: [PutValue][1] for references created by [`super.name`][2] in
     * non-strict code. The `receiver` is `this` and `obj` is the SuperBase of
     * the enclosing method.
     *
     * [1]: https://tc39.es/ecma262/#sec-putvalue
     * [2]: https://tc39.es/ecma262/#sec-super-keyword-runtime-semantics-evaluation
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: receiver, obj, val => val
     */ \
    MACRO(JSOP_SETPROP_SUPER, "setprop-super", NULL, 5, 3, 1, JOF_ATOM|JOF_PROP|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY) \
    /*
     * Like `JSOP_SETPROP_SUPER`, but for strict mode code.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands: uint32_t nameIndex
     *   Stack: receiver, obj, val => val
     */ \
    MACRO(JSOP_STRICTSETPROP_SUPER, "strictsetprop-super", NULL, 5, 3, 1, JOF_ATOM|JOF_PROP|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT) \
    /*
     * Assign `val` to `receiver[key]`, strating the search for an existing
     * property at `obj`. In spec terms, obj.[[Set]](key, val, receiver).
     *
     * Implements: [PutValue][1] for references created by [`super[key]`][2] in
     * non-strict code. The `receiver` is `this` and `obj` is the SuperBase of
     * the enclosing method.
     *
     * [1]: https://tc39.es/ecma262/#sec-putvalue
     * [2]: https://tc39.es/ecma262/#sec-super-keyword-runtime-semantics-evaluation
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: receiver, key, obj, val => val
     */ \
    MACRO(JSOP_SETELEM_SUPER, "setelem-super", NULL, 1, 4, 1, JOF_BYTE|JOF_ELEM|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSLOPPY) \
    /*
     * Like `JSOP_SETELEM_SUPER`, but for strict mode code.
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: receiver, key, obj, val => val
     */ \
    MACRO(JSOP_STRICTSETELEM_SUPER, "strict-setelem-super", NULL, 1, 4, 1, JOF_BYTE|JOF_ELEM|JOF_PROPSET|JOF_DETECTING|JOF_CHECKSTRICT) \
    /*
     * Set up a for-in loop by pushing a `PropertyIteratorObject` over the
     * enumerable properties of `val`.
     *
     * Implements: [ForIn/OfHeadEvaluation][1] step 6,
     * [EnumerateObjectProperties][1]. (The spec refers to an "Iterator object"
     * with a `next` method, but notes that it "is never directly accessible"
     * to scripts. The object we use for this has no public methods.)
     *
     * If `val` is null or undefined, this pushes an empty iterator.
     *
     * The `iter` object pushed by this instruction must not be used or removed
     * from the stack except by `JSOP_MOREITER` and `JSOP_ENDITER`, or by error
     * handling.
     *
     * The script's `JSScript::trynotes()` must mark the body of the `for-in`
     * loop, i.e. exactly those instructions that begin executing with `iter`
     * on the stack, starting with the next instruction (always
     * `JSOP_LOOPHEAD`). Code must not jump into or out of this region: control
     * can enter only by executing `JSOP_ITER` and can exit only by executing a
     * `JSOP_ENDITER` or by exception unwinding. (A `JSOP_ENDITER` is always
     * emitted at the end of the loop, and extra copies are emitted on "exit
     * slides", where a `break`, `continue`, or `return` statement exits the
     * loop.)
     *
     * Typically a single try note entry marks the contiguous chunk of bytecode
     * from the instruction after `JSOP_ITER` to `JSOP_ENDITER` (inclusive);
     * but if that range contains any instructions on exit slides, after a
     * `JSOP_ENDITER`, then those must be correctly noted as *outside* the
     * loop.
     *
     * [1]: https://tc39.es/ecma262/#sec-runtime-semantics-forin-div-ofheadevaluation-tdznames-expr-iterationkind
     * [2]: https://tc39.es/ecma262/#sec-enumerate-object-properties
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: val => iter
     */ \
    MACRO(JSOP_ITER, "iter", NULL, 1, 1, 1, JOF_BYTE|JOF_IC) \
    /*
     * Get the next property name for a for-in loop.
     *
     * `iter` must be a `PropertyIteratorObject` produced by `JSOP_ITER`.  This
     * pushes the property name for the next loop iteration, or
     * `MagicValue(JS_NO_ITER_VALUE)` if there are no more enumerable
     * properties to iterate over. The magic value must be used only by
     * `JSOP_ISNOITER` and `JSOP_ENDITER`.
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: iter => iter, name
     */ \
    MACRO(JSOP_MOREITER, "moreiter", NULL, 1, 1, 2, JOF_BYTE) \
    /*
     * Test whether the value on top of the stack is
     * `MagicValue(JS_NO_ITER_VALUE)` and push the boolean result.
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: val => val, done
     */ \
    MACRO(JSOP_ISNOITER, "isnoiter", NULL, 1, 1, 2, JOF_BYTE) \
    /*
     * No-op instruction to hint to IonBuilder that the value on top of the
     * stack is the (likely string) key in a for-in loop.
     *
     *   Category: Other
     *   Operands:
     *   Stack: val => val
     */ \
    MACRO(JSOP_ITERNEXT, "iternext", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Exit a for-in loop, closing the iterator.
     *
     * `iter` must be a `PropertyIteratorObject` pushed by `JSOP_ITER`.
     *
     *   Category: Statements
     *   Type: For-In Statement
     *   Operands:
     *   Stack: iter, iterval =>
     */ \
    MACRO(JSOP_ENDITER, "enditer", NULL, 1, 2, 0, JOF_BYTE) \
    /*
     * Check that the top value on the stack is an object, and throw a
     * TypeError if not. `kind` is used only to generate an appropriate error
     * message. It must be in range for `js::CheckIsObjectKind`.
     *
     * Implements: [GetIterator][1] step 5, [IteratorNext][2] step 3. Both
     * operations call a JS method which scripts can define however they want,
     * so they check afterwards that the method returned an object.
     *
     * [1]: https://tc39.es/ecma262/#sec-getiterator
     * [2]: https://tc39.es/ecma262/#sec-iteratornext
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint8_t kind
     *   Stack: result => result
     */ \
    MACRO(JSOP_CHECKISOBJ, "checkisobj", NULL, 2, 1, 1, JOF_UINT8) \
    /*
     * Check that the top value on the stack is callable, and throw a TypeError
     * if not. The operand `kind` is used only to generate an appropriate error
     * message. It must be in range for `js::CheckIsCallableKind`.
     *
     *   Category: Statements
     *   Type: Function
     *   Operands: uint8_t kind
     *   Stack: obj => obj
     */ \
    MACRO(JSOP_CHECKISCALLABLE, "checkiscallable", NULL, 2, 1, 1, JOF_UINT8) \
    /*
     * Throw a TypeError if `val` is `null` or `undefined`.
     *
     * Implements: [RequireObjectCoercible][1]. But most instructions that
     * require an object will perform this check for us, so of the dozens of
     * calls to RequireObjectCoercible in the spec, we need this instruction
     * only for [destructuring assignment][2] and [initialization][3].
     *
     * [1]: https://tc39.es/ecma262/#sec-requireobjectcoercible
     * [2]: https://tc39.es/ecma262/#sec-runtime-semantics-destructuringassignmentevaluation
     * [3]: https://tc39.es/ecma262/#sec-destructuring-binding-patterns-runtime-semantics-bindinginitialization
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: val => val
     */ \
    MACRO(JSOP_CHECKOBJCOERCIBLE, "checkobjcoercible", NULL, 1, 1, 1, JOF_BYTE) \
    /*
     * Create and push an async iterator wrapping the sync iterator `iter`.
     * `next` should be `iter`'s `.next` method.
     *
     * Implements: [CreateAsyncToSyncIterator][1]. The spec says this operation
     * takes one argument, but that argument is a Record with two relevant
     * fields, [[Iterator]] and [[NextMethod]].
     *
     * Used for `for await` loops.
     *
     * [1]: https://tc39.es/ecma262/#sec-createasyncfromsynciterator
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands:
     *   Stack: iter, next => asynciter
     */ \
    MACRO(JSOP_TOASYNCITER, "toasynciter", NULL, 1, 2, 1, JOF_BYTE) \
    /*
     * Set the prototype of `obj`.
     *
     * `obj` must be an object.
     *
     * Implements: [B.3.1 __proto__ Property Names in Object Initializers][1], step 7.a.
     *
     * [1]: https://tc39.es/ecma262/#sec-__proto__-property-names-in-object-initializers
     *
     *   Category: Literals
     *   Type: Object
     *   Operands:
     *   Stack: obj, protoVal => obj
     */ \
    MACRO(JSOP_MUTATEPROTO, "mutateproto", NULL, 1, 2, 1, JOF_BYTE) \
    /*
     * Create and push a new Array object with the given `length`,
     * preallocating enough memory to hold that many elements.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands: uint32_t length
     *   Stack: => array
     */ \
    MACRO(JSOP_NEWARRAY, "newarray", NULL, 5, 0, 1, JOF_UINT32|JOF_IC) \
    /*
     * Initialize an array element `array[index]` with value `val`.
     *
     * `val` may be `MagicValue(JS_ELEMENTS_HOLE)`. If it is, this does nothing.
     *
     * This never calls setters or proxy traps.
     *
     * `array` must be an Array object created by `JSOP_NEWARRAY` with length >
     * `index`, and never used except by `JSOP_INITELEM_ARRAY`.
     *
     * Implements: [ArrayAccumulation][1], the third algorithm, step 4, in the
     * common case where *nextIndex* is known.
     *
     * [1]: https://tc39.es/ecma262/#sec-runtime-semantics-arrayaccumulation
     *
     *   Category: Literals
     *   Type: Array
     *   Operands: uint32_t index
     *   Stack: array, val => array
     */ \
    MACRO(JSOP_INITELEM_ARRAY, "initelem_array", NULL, 5, 2, 1, JOF_UINT32|JOF_ELEM|JOF_PROPINIT|JOF_DETECTING|JOF_IC) \
    /*
     * Initialize an array element `array[index++]` with value `val`.
     *
     * `val` may be `MagicValue(JS_ELEMENTS_HOLE)`. If it is, no element is
     * defined, but the array length and the stack value `index` are still
     * incremented.
     *
     * This never calls setters or proxy traps.
     *
     * `array` must be an Array object created by `JSOP_NEWARRAY` and never used
     * except by `JSOP_INITELEM_ARRAY` and `JSOP_INITELEM_INC`.
     *
     * `index` must be an integer, `0 <= index <= INT32_MAX`. If `index` is
     * `INT32_MAX`, this throws a RangeError.
     *
     * This instruction is used when an array literal contains a
     * *SpreadElement*. In `[a, ...b, c]`, `INITELEM_ARRAY 0` is used to put
     * `a` into the array, but `INITELEM_INC` is used for the elements of `b`
     * and for `c`.
     *
     * Implements: Several steps in [ArrayAccumulation][1] that call
     * CreateDataProperty, set the array length, and/or increment *nextIndex*.
     *
     * [1]: https://tc39.es/ecma262/#sec-runtime-semantics-arrayaccumulation
     *
     *   Category: Literals
     *   Type: Array
     *   Operands:
     *   Stack: array, index, val => array, (index + 1)
     */ \
    MACRO(JSOP_INITELEM_INC, "initelem_inc", NULL, 1, 3, 2, JOF_BYTE|JOF_ELEM|JOF_PROPINIT|JOF_IC) \
    /*
     * Push `MagicValue(JS_ELEMENTS_HOLE)`, representing an *Elision* in an
     * array literal (like the missing property 0 in the array `[, 1]`).
     *
     * This magic value must be used only by `JSOP_INITELEM_ARRAY` or
     * `JSOP_INITELEM_INC`.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands:
     *   Stack: => hole
     */ \
    MACRO(JSOP_HOLE, "hole", NULL, 1, 0, 1, JOF_BYTE) \
    /*
     * Create and push a new array that shares the elements of a template
     * object.
     *
     * `script->getObject(objectIndex)` must be a copy-on-write array whose
     * elements are all primitive values.
     *
     * This is an optimization. This single instruction implements an entire
     * array literal, saving run time, code, and memory compared to
     * `JSOP_NEWARRAY` and a series of `JSOP_INITELEM` instructions.
     *
     *   Category: Literals
     *   Type: Array
     *   Operands: uint32_t objectIndex
     *   Stack: => array
     */ \
    MACRO(JSOP_NEWARRAY_COPYONWRITE, "newarray_copyonwrite", NULL, 5, 0, 1, JOF_OBJECT) \
    /*
     * Clone and push a new RegExp object.
     *
     * Implements: [Evaluation for *RegularExpressionLiteral*][1].
     *
     * [1]: https://tc39.es/ecma262/#sec-regular-expression-literals-runtime-semantics-evaluation
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
     *   Operands: uint32_t nameIndex, uint32_t sourceStart, uint32_t sourceEnd
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
     *   Operands: uint32_t nameIndex, uint32_t sourceStart, uint32_t sourceEnd
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
     * Push the call site object for a tagged template call.
     *
     * `script->getObject(objectIndex)` is the call site object;
     * `script->getObject(objectIndex + 1)` is the raw object.
     *
     * The first time this instruction runs for a given template, it assembles
     * the final value, defining the `.raw` property on the call site object
     * and freezing both objects.
     *
     * Implements: [GetTemplateObject][1], steps 4 and 12-16.
     *
     * [1]: https://tc39.es/ecma262/#sec-gettemplateobject
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
     * execution.
     *
     * When resuming execution, JSOP_RESUME pushes the rval, gen and resumeKind
     * values. resumeKind is the GeneratorResumeKind stored as int32.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint24_t resumeIndex
     *   Stack: gen => rval, gen, resumeKind
     */ \
    MACRO(JSOP_INITIALYIELD, "initialyield", NULL, 4, 1, 3, JOF_RESUMEINDEX) \
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
     * Pops the generator and the return value 'rval1', stops execution and
     * returns 'rval1'.
     *
     * When resuming execution, JSOP_RESUME pushes the rval2, gen and resumeKind
     * values.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint24_t resumeIndex
     *   Stack: rval1, gen => rval2, gen, resumeKind
     */ \
    MACRO(JSOP_YIELD, "yield", NULL, 4, 2, 3, JOF_RESUMEINDEX) \
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
     * Pops the generator and the return value 'promise', stops execution and
     * returns 'promise'.
     *
     * When resuming execution, JSOP_RESUME pushes the resolved, gen and
     * resumeKind values. resumeKind is the GeneratorResumeKind stored as int32.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: uint24_t resumeIndex
     *   Stack: promise, gen => resolved, gen, resumeKind
     */ \
    MACRO(JSOP_AWAIT, "await", NULL, 4, 2, 3, JOF_RESUMEINDEX) \
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
     * Pushes one of the GeneratorResumeKind values as Int32Value.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands: resumeKind (GeneratorResumeKind)
     *   Stack: => resumeKind
     */ \
    MACRO(JSOP_RESUMEKIND, "resumekind", NULL, 2, 0, 1, JOF_UINT8) \
    /*
     * Pops the generator and resumeKind values. resumeKind is the
     * GeneratorResumeKind stored as int32. If resumeKind is Next, continue
     * execution. If resumeKind is Throw or Return, these completions are
     * handled by throwing an exception. See GeneratorThrowOrReturn.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands:
     *   Stack: rval, gen, resumeKind => rval
     */ \
    MACRO(JSOP_CHECK_RESUMEKIND, "check-resumekind", NULL, 1, 3, 1, JOF_BYTE) \
    /*
     * Pops the generator, argument and resumeKind from the stack, pushes a new
     * generator frame and resumes execution of it. Pushes the return value
     * after the generator yields.
     *
     *   Category: Statements
     *   Type: Generator
     *   Operands:
     *   Stack: gen, val, resumeKind => rval
     */ \
    MACRO(JSOP_RESUME, "resume", NULL, 1, 3, 1, JOF_BYTE|JOF_INVOKE) \
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
     * The jumpAtEndOffset operand is the offset (relative to the current op) of
     * the JSOP_GOTO at the end of the try-block body. This is used by bytecode
     * analysis and JIT compilation.
     *
     * Location information for catch/finally blocks is stored in a side table,
     * 'script->trynotes()'.
     *
     *   Category: Statements
     *   Type: Exception Handling
     *   Operands: int32_t jumpAtEndOffset
     *   Stack: =>
     */ \
    MACRO(JSOP_TRY, "try", NULL, 5, 0, 0, JOF_CODE_OFFSET) \
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
     * Assign `val` to the binding in `env` with the name given by `nameIndex`,
     * then push `val` back onto the stack. If assignment failed, throw a
     * TypeError.
     *
     * `env` must be an environment currently on the environment chain.
     *
     * Implements: [PutValue][1] step 7 for strict mode code.
     *
     * [1]: https://tc39.es/ecma262/#sec-putvalue
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
     *   Operands: uint32_t lexicalScopeIndex
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

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* most of this file will get auto-generated from the icode metadata */

#ifndef __icodemap_h

#define __icodemap_h

#include "systemtypes.h"
#include "icodeasm.h"

namespace JavaScript {
namespace ICodeASM {

static uint icodemap_size = 72;

static struct {
    char *name;
    OperandType otype[4];
} icodemap [] =
{
    {"ADD", {otRegister, otRegister, otRegister}},
    {"AND", {otRegister, otRegister, otRegister}},
    {"BITNOT", {otRegister, otRegister}},
    {"BRANCH", {otLabel}},
    {"BRANCH_FALSE", {otLabel, otRegister}},
    {"BRANCH_INITIALIZED", {otLabel, otRegister}},
    {"BRANCH_TRUE", {otLabel, otRegister}},
    {"CALL", {otRegister, otRegister, otRegister, otArgumentList}},
    {"CAST", {otRegister, otRegister, otJSType}},
    {"COMPARE_EQ", {otRegister, otRegister, otRegister}},
    {"COMPARE_GE", {otRegister, otRegister, otRegister}},
    {"COMPARE_GT", {otRegister, otRegister, otRegister}},
    {"COMPARE_IN", {otRegister, otRegister, otRegister}},
    {"COMPARE_LE", {otRegister, otRegister, otRegister}},
    {"COMPARE_LT", {otRegister, otRegister, otRegister}},
    {"COMPARE_NE", {otRegister, otRegister, otRegister}},
    {"DEBUGGER", {otNone}},
    {"DELETE_PROP", {otRegister, otRegister, otStringAtom}},
    {"DIRECT_CALL", {otRegister, otJSFunction, otArgumentList}},
    {"DIVIDE", {otRegister, otRegister, otRegister}},
    {"ELEM_XCR", {otRegister, otRegister, otRegister, otDouble}},
    {"GENERIC_BINARY_OP", {otRegister, otBinaryOp, otRegister, otRegister}},
    {"GET_ELEMENT", {otRegister, otRegister, otRegister}},
    {"GET_METHOD", {otRegister, otRegister, otUInt32}},
    {"GET_PROP", {otRegister, otRegister, otStringAtom}},
    {"GET_SLOT", {otRegister, otRegister, otUInt32}},
    {"GET_STATIC", {otRegister, otJSClass, otUInt32}},
    {"INSTANCEOF", {otRegister, otRegister, otRegister}},
    {"JSR", {otLabel}},
    {"LOAD_BOOLEAN", {otRegister, otBool}},
    {"LOAD_IMMEDIATE", {otRegister, otDouble}},
    {"LOAD_NAME", {otRegister, otStringAtom}},
    {"LOAD_STRING", {otRegister, otJSString}},
    {"MOVE", {otRegister, otRegister}},
    {"MULTIPLY", {otRegister, otRegister, otRegister}},
    {"NAME_XCR", {otRegister, otStringAtom, otDouble}},
    {"NEGATE", {otRegister, otRegister}},
    {"NEW_ARRAY", {otRegister}},
    {"NEW_CLASS", {otRegister, otJSClass}},
    {"NEW_FUNCTION", {otRegister, otICodeModule}},
    {"NEW_OBJECT", {otRegister, otRegister}},
    {"NOP", {otNone}},
    {"NOT", {otRegister, otRegister}},
    {"OR", {otRegister, otRegister, otRegister}},
    {"POSATE", {otRegister, otRegister}},
    {"PROP_XCR", {otRegister, otRegister, otStringAtom, otDouble}},
    {"REMAINDER", {otRegister, otRegister, otRegister}},
    {"RETURN", {otRegister}},
    {"RETURN_VOID", {otNone}},
    {"RTS", {otNone}},
    {"SAVE_NAME", {otStringAtom, otRegister}},
    {"SET_ELEMENT", {otRegister, otRegister, otRegister}},
    {"SET_PROP", {otRegister, otStringAtom, otRegister}},
    {"SET_SLOT", {otRegister, otUInt32, otRegister}},
    {"SET_STATIC", {otJSClass, otUInt32, otRegister}},
    {"SHIFTLEFT", {otRegister, otRegister, otRegister}},
    {"SHIFTRIGHT", {otRegister, otRegister, otRegister}},
    {"SLOT_XCR", {otRegister, otRegister, otUInt32, otDouble}},
    {"STATIC_XCR", {otRegister, otJSClass, otUInt32, otDouble}},
    {"STRICT_EQ", {otRegister, otRegister, otRegister}},
    {"STRICT_NE", {otRegister, otRegister, otRegister}},
    {"SUBTRACT", {otRegister, otRegister, otRegister}},
    {"SUPER", {otRegister}},
    {"TEST", {otRegister, otRegister}},
    {"THROW", {otRegister}},
    {"TRYIN", {otLabel, otLabel}},
    {"TRYOUT", {otNone}},
    {"USHIFTRIGHT", {otRegister, otRegister, otRegister}},
    {"VAR_XCR", {otRegister, otRegister, otDouble}},
    {"WITHIN", {otRegister}},
    {"WITHOUT", {otNone}},
    {"XOR", {otRegister, otRegister, otRegister}},
};
 
}
}

#endif /* #ifdef __icodemap_h */

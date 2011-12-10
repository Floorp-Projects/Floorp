/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsion_vm_functions_h__
#define jsion_vm_functions_h__

#include "jspubtd.h"

namespace js {
namespace ion {

enum DataType {
    Type_Void,
    Type_Bool,
    Type_Object,
    Type_Value
};

// Contains information about a virtual machine function that can be called
// from JIT code. Functions described in this manner must conform to a simple
// protocol: the return type must have a special "failure" value (for example,
// false for bool, or NULL for Objects). If the function is designed to return
// a value that does not meet this requirement - such as object-or-NULL, or an
// integer, an optional, final outParam can be specified. In this case, the
// return type must be boolean to indicate failure.
//
// All functions described by VMFunction take a JSContext * as a first
// argument, and are treated as re-entrant into the VM and therefore fallible.
struct VMFunction
{
    // Address of the C function.
    void *wrapped;

    // Number of arguments expected, excluding JSContext * as an implicit
    // first argument, and the outparam as an implicit last argument.
    uint32 explicitArgs;

    // The outparam may be any Type_*, and must be the final argument to the
    // function, if not Void. outParam != Void implies that the return type
    // has a boolean failure mode.
    DataType outParam;

    // Identify which values are returned to the Jitted code. If and only if
    // the outParam is set to OutParam_Value, then the returnType must be
    // set to the ReturnValue.  Otherwise it should be either consistent
    // with the FallibleType or ReturnNothing.
    DataType returnType;

    uint32 argc() const {
        // JSContext * + args + (OutParam? *)
        return 1 + explicitArgs + ((outParam == Type_Void) ? 0 : 1);
    }

    DataType failType() const {
        JS_ASSERT(returnType == Type_Object || returnType == Type_Bool);
        JS_ASSERT_IF(outParam != Type_Void, returnType == Type_Bool);
        return returnType;
    }

    VMFunction(void *wrapped, uint32 explicitArgs, DataType outParam,
               DataType returnType)
      : wrapped(wrapped), explicitArgs(explicitArgs), outParam(outParam),
        returnType(returnType)
    {
    }
};

template <class> struct TypeToDataType { };
template <> struct TypeToDataType<bool> { static const DataType result = Type_Bool; };
template <> struct TypeToDataType<JSObject *> { static const DataType result = Type_Object; };
template <class> struct OutParamToDataType { static const DataType result = Type_Void; };
template <> struct OutParamToDataType<Value *> { static const DataType result = Type_Value; };

template <class R, class A1, class A2, R pf(JSContext *, A1, A2)>
struct FunctionInfo : public VMFunction {
    static inline DataType returnType() {
        return TypeToDataType<R>::result;
    }
    static inline DataType outParam() {
        return OutParamToDataType<A2>::result;
    }
    static inline size_t explicitArgs() {
        return 2 + (outParam() != Type_Void ? 1 : 0);
    }
    FunctionInfo()
      : VMFunction(JS_FUNC_TO_DATA_PTR(void *, pf), explicitArgs(), outParam(),
                   returnType())
    { }
};

} // namespace ion
} // namespace js

#endif // jsion_vm_functions_h_


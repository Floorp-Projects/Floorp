/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Error Message definitions. */

/* 
* The format here is the same as that in js/src/js.msg
* However, arguments are not supported. And, the error->exception
* mapping is not appropriate here. So, the arg count and exception id 
* columns are left with zero values.
*
* Also, the idetifiers here are read in as members of an anonymous enum in
* the class XPCJSError rather than at the top level as in JS.
*/

MSG_DEF(NOT_AN_ERROR,             0, 0, 0, "<Error #0 is reserved>")
MSG_DEF(NOT_ENOUGH_ARGS,          1, 0, 0, "Not enough arguments")
MSG_DEF(NEED_OUT_OBJECT,          2, 0, 0, "'Out' argument must be an object")
MSG_DEF(CANT_SET_OUT_VAL,         3, 0, 0, "Can not set 'value' property of 'out' argument")
MSG_DEF(NATIVE_RETURNED_FAILURE,  4, 0, 0, "Component returned failure code:")
MSG_DEF(CANT_GET_INTERFACE_INFO,  5, 0, 0, "Can not find interface information")
MSG_DEF(CANT_GET_PARAM_IFACE_INFO,6, 0, 0, "Can not find interface information for parameter")
MSG_DEF(CANT_GET_METHOD_INFO,     7, 0, 0, "Can not find method information")
MSG_DEF(UNEXPECTED,               8, 0, 0, "Unexpected error in XPConnect")
MSG_DEF(BAD_CONVERT_JS,           9, 0, 0, "Could not convert JavaScript argument")
MSG_DEF(BAD_CONVERT_NATIVE,      10, 0, 0, "Could not convert Native argument")
MSG_DEF(BAD_CONVERT_JS_NULL_REF, 11, 0, 0, "Could not convert JavaScript argument (NULL value can not be used for a C++ reference type)")
MSG_DEF(BAD_OP_ON_WN_PROTO,      12, 0, 0, "Illegal operation on WrappedNative prototype object")
MSG_DEF(CANT_CONVERT_WN_TO_FUN,  13, 0, 0, "Cannot convert WrappedNative to function")
MSG_DEF(CANT_DEFINE_PROP_ON_WN,  14, 0, 0, "Cannot define new property in a WrappedNative")
MSG_DEF(CANT_WATCH_WN_STATIC,    15, 0, 0, "Cannot place watchpoints on WrappedNative object static properties")
MSG_DEF(CANT_EXPORT_WN_STATIC,   16, 0, 0, "Cannot export a WrappedNative object's static properties")
MSG_DEF(SCRIPTABLE_CALL_FAILED,  17, 0, 0, "nsIXPCScriptable::Call failed")
MSG_DEF(SCRIPTABLE_CTOR_FAILED,  18, 0, 0, "nsIXPCScriptable::Construct failed")
MSG_DEF(CANT_CALL_WO_SCRIPTABLE, 19, 0, 0, "Can not use wrapper as function unless it implements nsIXPCScriptable")
MSG_DEF(CANT_CTOR_WO_SCRIPTABLE, 20, 0, 0, "Can not use wrapper as constuctor unless it implements nsIXPCScriptable")
MSG_DEF(CI_RETURNED_FAILURE,     21, 0, 0, "ComponentManager::CreateInstance returned failure code:")
MSG_DEF(GS_RETURNED_FAILURE,     22, 0, 0, "ServiceManager::GetService returned failure code:")
MSG_DEF(BAD_CID,                 23, 0, 0, "Invalid ClassID or ProgID")
MSG_DEF(BAD_IID,                 24, 0, 0, "Invalid InterfaceID")
MSG_DEF(CANT_CREATE_WN,          25, 0, 0, "Can not create wrapper around native interface")

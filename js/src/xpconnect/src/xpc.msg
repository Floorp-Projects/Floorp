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

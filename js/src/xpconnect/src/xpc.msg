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


/* xpconnect specific codes (from nsIXPConnect.h) */

XPC_MSG_DEF(NS_ERROR_XPC_NOT_ENOUGH_ARGS              , "Not enough arguments")
XPC_MSG_DEF(NS_ERROR_XPC_NEED_OUT_OBJECT              , "'Out' argument must be an object")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_OUT_VAL             , "Can not set 'value' property of 'out' argument")
XPC_MSG_DEF(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE      , "Component returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_INTERFACE_INFO      , "Can not find interface information")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO    , "Can not find interface information for parameter")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_METHOD_INFO         , "Can not find method information")
XPC_MSG_DEF(NS_ERROR_XPC_UNEXPECTED                   , "Unexpected error in XPConnect")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_JS               , "Could not convert JavaScript argument")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_NATIVE           , "Could not convert Native argument")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF      , "Could not convert JavaScript argument (NULL value can not be used for a C++ reference type)")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO           , "Illegal operation on WrappedNative prototype object")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CONVERT_WN_TO_FUN       , "Cannot convert WrappedNative to function")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_DEFINE_PROP_ON_WN       , "Cannot define new property in a WrappedNative")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_WATCH_WN_STATIC         , "Cannot place watchpoints on WrappedNative object static properties")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_EXPORT_WN_STATIC        , "Cannot export a WrappedNative object's static properties")
XPC_MSG_DEF(NS_ERROR_XPC_SCRIPTABLE_CALL_FAILED       , "nsIXPCScriptable::Call failed")
XPC_MSG_DEF(NS_ERROR_XPC_SCRIPTABLE_CTOR_FAILED       , "nsIXPCScriptable::Construct failed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CALL_WO_SCRIPTABLE      , "Can not use wrapper as function unless it implements nsIXPCScriptable")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CTOR_WO_SCRIPTABLE      , "Can not use wrapper as constuctor unless it implements nsIXPCScriptable")
XPC_MSG_DEF(NS_ERROR_XPC_CI_RETURNED_FAILURE          , "ComponentManager::CreateInstance returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_GS_RETURNED_FAILURE          , "ServiceManager::GetService returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CID                      , "Invalid ClassID or ProgID")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_IID                      , "Invalid InterfaceID")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CREATE_WN               , "Can not create wrapper around native interface")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_EXCEPTION           , "JavaScript component threw exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_NATIVE_OBJECT       , "JavaScript component threw a native object that is not an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_NULL                , "JavaScript component threw a null value as an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_STRING              , "JavaScript component threw a string as an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JAVASCRIPT_ERROR             , "JavaScript component caused a JavaScript error")
XPC_MSG_DEF(NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS, "JavaScript component caused a JavaScript error (detailed report attached)")

/* common global codes (from nsError.h) */

XPC_MSG_DEF(NS_OK                                  , "Success")
XPC_MSG_DEF(NS_ERROR_NOT_INITIALIZED               , "Component not initialized")
XPC_MSG_DEF(NS_ERROR_ALREADY_INITIALIZED           , "Component already initialized")
XPC_MSG_DEF(NS_ERROR_NOT_IMPLEMENTED               , "Method not implemented")
XPC_MSG_DEF(NS_NOINTERFACE                         , "Component does not have requested interface")
XPC_MSG_DEF(NS_ERROR_NO_INTERFACE                  , "Component does not have requested interface")
XPC_MSG_DEF(NS_ERROR_INVALID_POINTER               , "Invalid pointer")
XPC_MSG_DEF(NS_ERROR_NULL_POINTER                  , "Null pointer")
XPC_MSG_DEF(NS_ERROR_ABORT                         , "Abort")
XPC_MSG_DEF(NS_ERROR_FAILURE                       , "Failure")
XPC_MSG_DEF(NS_ERROR_UNEXPECTED                    , "Unexpected error")
XPC_MSG_DEF(NS_ERROR_OUT_OF_MEMORY                 , "Out of Memory")
XPC_MSG_DEF(NS_ERROR_ILLEGAL_VALUE                 , "Illegal value")
XPC_MSG_DEF(NS_ERROR_INVALID_ARG                   , "Invalid argument")
XPC_MSG_DEF(NS_ERROR_NO_AGGREGATION                , "Component doe not support aggregation")
XPC_MSG_DEF(NS_ERROR_NOT_AVAILABLE                 , "Component is not available")
XPC_MSG_DEF(NS_ERROR_FACTORY_NOT_REGISTERED        , "Factory not registered")
XPC_MSG_DEF(NS_ERROR_FACTORY_NOT_LOADED            , "Factory not loaded")
XPC_MSG_DEF(NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT  , "Factory does not support signatures")
XPC_MSG_DEF(NS_ERROR_FACTORY_EXISTS                , "Factory already exists")
XPC_MSG_DEF(NS_ERROR_PROXY_INVALID_IN_PARAMETER    , "Invalid input parameter in call to proxy")
XPC_MSG_DEF(NS_ERROR_PROXY_INVALID_OUT_PARAMETER   , "Invalid output parameter in call to proxy")


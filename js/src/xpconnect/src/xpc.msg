/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
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

/* Error Message definitions. */


/* xpconnect specific codes (from nsIXPConnect.h) */

XPC_MSG_DEF(NS_ERROR_XPC_NOT_ENOUGH_ARGS                , "Not enough arguments")
XPC_MSG_DEF(NS_ERROR_XPC_NEED_OUT_OBJECT                , "'Out' argument must be an object")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_OUT_VAL               , "Can not set 'value' property of 'out' argument")
XPC_MSG_DEF(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE        , "Component returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_INTERFACE_INFO        , "Can not find interface information")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO      , "Can not find interface information for parameter")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_METHOD_INFO           , "Can not find method information")
XPC_MSG_DEF(NS_ERROR_XPC_UNEXPECTED                     , "Unexpected error in XPConnect")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_JS                 , "Could not convert JavaScript argument")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_NATIVE             , "Could not convert Native argument")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF        , "Could not convert JavaScript argument (NULL value can not be used for a C++ reference type)")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO             , "Illegal operation on WrappedNative prototype object")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CONVERT_WN_TO_FUN         , "Cannot convert WrappedNative to function")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_DEFINE_PROP_ON_WN         , "Cannot define new property in a WrappedNative")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_WATCH_WN_STATIC           , "Cannot place watchpoints on WrappedNative object static properties")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_EXPORT_WN_STATIC          , "Cannot export a WrappedNative object's static properties")
XPC_MSG_DEF(NS_ERROR_XPC_SCRIPTABLE_CALL_FAILED         , "nsIXPCScriptable::Call failed")
XPC_MSG_DEF(NS_ERROR_XPC_SCRIPTABLE_CTOR_FAILED         , "nsIXPCScriptable::Construct failed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CALL_WO_SCRIPTABLE        , "Can not use wrapper as function unless it implements nsIXPCScriptable")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CTOR_WO_SCRIPTABLE        , "Can not use wrapper as constuctor unless it implements nsIXPCScriptable")
XPC_MSG_DEF(NS_ERROR_XPC_CI_RETURNED_FAILURE            , "ComponentManager::CreateInstance returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_GS_RETURNED_FAILURE            , "ServiceManager::GetService returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CID                        , "Invalid ClassID or ContractID")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_IID                        , "Invalid InterfaceID")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CREATE_WN                 , "Can not create wrapper around native interface")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_EXCEPTION             , "JavaScript component threw exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_NATIVE_OBJECT         , "JavaScript component threw a native object that is not an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_JS_OBJECT             , "JavaScript component threw a JavaScript object")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_NULL                  , "JavaScript component threw a null value as an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_STRING                , "JavaScript component threw a string as an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_NUMBER                , "JavaScript component threw a number as an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JAVASCRIPT_ERROR               , "JavaScript component caused a JavaScript error")
XPC_MSG_DEF(NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS  , "JavaScript component caused a JavaScript error (detailed report attached)")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY, "Can not convert primitive JavaScript value into an array")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CONVERT_OBJECT_TO_ARRAY   , "Can not convert JavaScript object into an array")
XPC_MSG_DEF(NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY   , "JavaScript Array does not have as many elements as indicated by size argument")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_ARRAY_INFO            , "Can not find array information")
XPC_MSG_DEF(NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING     , "JavaScript String does not have as many characters as indicated by size argument")
XPC_MSG_DEF(NS_ERROR_XPC_SECURITY_MANAGER_VETO          , "Security Manager vetoed action")
XPC_MSG_DEF(NS_ERROR_XPC_INTERFACE_NOT_SCRIPTABLE       , "Failed to build a wrapper because the interface that was not declared [scriptable]")
XPC_MSG_DEF(NS_ERROR_XPC_INTERFACE_NOT_FROM_NSISUPPORTS , "Failed to build a wrapper because the interface does not inherit from nsISupports")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_JSOBJECT_OF_DOM_OBJECT, "Can not get JavaScript object for DOM object")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_READ_ONLY_CONSTANT    , "Property is a constant and can not be changed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_READ_ONLY_ATTRIBUTE   , "Property is a read only attribute and can not be changed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_READ_ONLY_METHOD      , "Property is an interface method and can not be changed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_ADD_PROP_TO_WRAPPED_NATIVE, "Can not add property to WrappedNative object")
XPC_MSG_DEF(NS_ERROR_XPC_CALL_TO_SCRIPTABLE_FAILED      , "Call to nsIXPCScriptable interface for WrappedNative failed unexpecedly")
XPC_MSG_DEF(NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED , "JavaScript component does not have a method named:")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_ID_STRING                  , "Bad ID string")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_INITIALIZER_NAME           , "Bad initializer name in Constructor - Component has no method with that name")

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
XPC_MSG_DEF(NS_ERROR_NO_AGGREGATION                , "Component does not support aggregation")
XPC_MSG_DEF(NS_ERROR_NOT_AVAILABLE                 , "Component is not available")
XPC_MSG_DEF(NS_ERROR_FACTORY_NOT_REGISTERED        , "Factory not registered")
XPC_MSG_DEF(NS_ERROR_FACTORY_REGISTER_AGAIN        , "Factory not registered (may be tried again)")
XPC_MSG_DEF(NS_ERROR_FACTORY_NOT_LOADED            , "Factory not loaded")
XPC_MSG_DEF(NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT  , "Factory does not support signatures")
XPC_MSG_DEF(NS_ERROR_FACTORY_EXISTS                , "Factory already exists")
XPC_MSG_DEF(NS_ERROR_PROXY_INVALID_IN_PARAMETER    , "Invalid input parameter in call to proxy")
XPC_MSG_DEF(NS_ERROR_PROXY_INVALID_OUT_PARAMETER   , "Invalid output parameter in call to proxy")


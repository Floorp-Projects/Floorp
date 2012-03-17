/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   John Bandhauer <jband@netscape.com> (original author)
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

/* Error Message definitions. */


/* xpconnect specific codes (from nsIXPConnect.h) */

XPC_MSG_DEF(NS_ERROR_XPC_NOT_ENOUGH_ARGS                , "Not enough arguments")
XPC_MSG_DEF(NS_ERROR_XPC_NEED_OUT_OBJECT                , "'Out' argument must be an object")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_OUT_VAL               , "Cannot set 'value' property of 'out' argument")
XPC_MSG_DEF(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE        , "Component returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_INTERFACE_INFO        , "Cannot find interface information")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO      , "Cannot find interface information for parameter")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_METHOD_INFO           , "Cannot find method information")
XPC_MSG_DEF(NS_ERROR_XPC_UNEXPECTED                     , "Unexpected error in XPConnect")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_JS                 , "Could not convert JavaScript argument")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_NATIVE             , "Could not convert Native argument")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF        , "Could not convert JavaScript argument (NULL value cannot be used for a C++ reference type)")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO             , "Illegal operation on WrappedNative prototype object")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CONVERT_WN_TO_FUN         , "Cannot convert WrappedNative to function")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_DEFINE_PROP_ON_WN         , "Cannot define new property in a WrappedNative")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_WATCH_WN_STATIC           , "Cannot place watchpoints on WrappedNative object static properties")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_EXPORT_WN_STATIC          , "Cannot export a WrappedNative object's static properties")
XPC_MSG_DEF(NS_ERROR_XPC_SCRIPTABLE_CALL_FAILED         , "nsIXPCScriptable::Call failed")
XPC_MSG_DEF(NS_ERROR_XPC_SCRIPTABLE_CTOR_FAILED         , "nsIXPCScriptable::Construct failed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CALL_WO_SCRIPTABLE        , "Cannot use wrapper as function unless it implements nsIXPCScriptable")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CTOR_WO_SCRIPTABLE        , "Cannot use wrapper as constructor unless it implements nsIXPCScriptable")
XPC_MSG_DEF(NS_ERROR_XPC_CI_RETURNED_FAILURE            , "ComponentManager::CreateInstance returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_GS_RETURNED_FAILURE            , "ServiceManager::GetService returned failure code:")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CID                        , "Invalid ClassID or ContractID")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_IID                        , "Invalid InterfaceID")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CREATE_WN                 , "Cannot create wrapper around native interface")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_EXCEPTION             , "JavaScript component threw exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_NATIVE_OBJECT         , "JavaScript component threw a native object that is not an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_JS_OBJECT             , "JavaScript component threw a JavaScript object")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_NULL                  , "JavaScript component threw a null value as an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_STRING                , "JavaScript component threw a string as an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JS_THREW_NUMBER                , "JavaScript component threw a number as an exception")
XPC_MSG_DEF(NS_ERROR_XPC_JAVASCRIPT_ERROR               , "JavaScript component caused a JavaScript error")
XPC_MSG_DEF(NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS  , "JavaScript component caused a JavaScript error (detailed report attached)")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY, "Cannot convert primitive JavaScript value into an array")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_CONVERT_OBJECT_TO_ARRAY   , "Cannot convert JavaScript object into an array")
XPC_MSG_DEF(NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY   , "JavaScript Array does not have as many elements as indicated by size argument")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_ARRAY_INFO            , "Cannot find array information")
XPC_MSG_DEF(NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING     , "JavaScript String does not have as many characters as indicated by size argument")
XPC_MSG_DEF(NS_ERROR_XPC_SECURITY_MANAGER_VETO          , "Security Manager vetoed action")
XPC_MSG_DEF(NS_ERROR_XPC_INTERFACE_NOT_SCRIPTABLE       , "Failed to build a wrapper because the interface that was not declared [scriptable]")
XPC_MSG_DEF(NS_ERROR_XPC_INTERFACE_NOT_FROM_NSISUPPORTS , "Failed to build a wrapper because the interface does not inherit from nsISupports")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_GET_JSOBJECT_OF_DOM_OBJECT, "Cannot get JavaScript object for DOM object")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_READ_ONLY_CONSTANT    , "Property is a constant and cannot be changed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_READ_ONLY_ATTRIBUTE   , "Property is a read only attribute and cannot be changed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_SET_READ_ONLY_METHOD      , "Property is an interface method and cannot be changed")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_ADD_PROP_TO_WRAPPED_NATIVE, "Cannot add property to WrappedNative object")
XPC_MSG_DEF(NS_ERROR_XPC_CALL_TO_SCRIPTABLE_FAILED      , "Call to nsIXPCScriptable interface for WrappedNative failed unexpecedly")
XPC_MSG_DEF(NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED , "JavaScript component does not have a method named:")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_ID_STRING                  , "Bad ID string")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_INITIALIZER_NAME           , "Bad initializer name in Constructor - Component has no method with that name")
XPC_MSG_DEF(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN              , "Operation failed because the XPConnect subsystem has been shutdown")
XPC_MSG_DEF(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN         , "Cannot modify properties of a WrappedNative")
XPC_MSG_DEF(NS_ERROR_XPC_BAD_CONVERT_JS_ZERO_ISNOT_NULL , "Could not convert JavaScript argument - 0 was passed, expected object. Did you mean null?")


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

/* added from nsError.h on Feb 28 2001... */

XPC_MSG_DEF(NS_BASE_STREAM_CLOSED                   , "Stream closed")
XPC_MSG_DEF(NS_BASE_STREAM_OSERROR                  , "Error from the operating system")
XPC_MSG_DEF(NS_BASE_STREAM_ILLEGAL_ARGS             , "Illegal arguments")
XPC_MSG_DEF(NS_BASE_STREAM_NO_CONVERTER             , "No converter for unichar streams")
XPC_MSG_DEF(NS_BASE_STREAM_BAD_CONVERSION           , "Bad converter for unichar streams")
XPC_MSG_DEF(NS_BASE_STREAM_WOULD_BLOCK              , "Stream would block")

XPC_MSG_DEF(NS_ERROR_FILE_UNRECOGNIZED_PATH         , "File error: Unrecognized path")
XPC_MSG_DEF(NS_ERROR_FILE_UNRESOLVABLE_SYMLINK      , "File error: Unresolvable symlink")
XPC_MSG_DEF(NS_ERROR_FILE_EXECUTION_FAILED          , "File error: Execution failed")
XPC_MSG_DEF(NS_ERROR_FILE_UNKNOWN_TYPE              , "File error: Unknown type")
XPC_MSG_DEF(NS_ERROR_FILE_DESTINATION_NOT_DIR       , "File error: Destination not dir")
XPC_MSG_DEF(NS_ERROR_FILE_TARGET_DOES_NOT_EXIST     , "File error: Target does not exist")
XPC_MSG_DEF(NS_ERROR_FILE_COPY_OR_MOVE_FAILED       , "File error: Copy or move failed")
XPC_MSG_DEF(NS_ERROR_FILE_ALREADY_EXISTS            , "File error: Already exists")
XPC_MSG_DEF(NS_ERROR_FILE_INVALID_PATH              , "File error: Invalid path")
XPC_MSG_DEF(NS_ERROR_FILE_DISK_FULL                 , "File error: Disk full")
XPC_MSG_DEF(NS_ERROR_FILE_CORRUPTED                 , "File error: Corrupted")
XPC_MSG_DEF(NS_ERROR_FILE_NOT_DIRECTORY             , "File error: Not directory")
XPC_MSG_DEF(NS_ERROR_FILE_IS_DIRECTORY              , "File error: Is directory")
XPC_MSG_DEF(NS_ERROR_FILE_IS_LOCKED                 , "File error: Is locked")
XPC_MSG_DEF(NS_ERROR_FILE_TOO_BIG                   , "File error: Too big")
XPC_MSG_DEF(NS_ERROR_FILE_NO_DEVICE_SPACE           , "File error: No device space")
XPC_MSG_DEF(NS_ERROR_FILE_NAME_TOO_LONG             , "File error: Name too long")
XPC_MSG_DEF(NS_ERROR_FILE_NOT_FOUND                 , "File error: Not found")
XPC_MSG_DEF(NS_ERROR_FILE_READ_ONLY                 , "File error: Read only")
XPC_MSG_DEF(NS_ERROR_FILE_DIR_NOT_EMPTY             , "File error: Dir not empty")
XPC_MSG_DEF(NS_ERROR_FILE_ACCESS_DENIED             , "File error: Access denied")

/* added from nsError.h on Sept 6 2001... */

XPC_MSG_DEF(NS_ERROR_CANNOT_CONVERT_DATA            , "Data conversion error")
XPC_MSG_DEF(NS_ERROR_OBJECT_IS_IMMUTABLE            , "Can not modify immutable data container")
XPC_MSG_DEF(NS_ERROR_LOSS_OF_SIGNIFICANT_DATA       , "Data conversion failed because significant data would be lost")
XPC_MSG_DEF(NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA   , "Data conversion succeeded but data was rounded to fit")

/* network related codes (from nsNetError.h) */

XPC_MSG_DEF(NS_BINDING_FAILED                       , "The async request failed for some unknown reason")
XPC_MSG_DEF(NS_BINDING_ABORTED                      , "The async request failed because it was aborted by some user action")
XPC_MSG_DEF(NS_BINDING_REDIRECTED                   , "The async request has been redirected to a different async request")
XPC_MSG_DEF(NS_BINDING_RETARGETED                   , "The async request has been retargeted to a different handler")
XPC_MSG_DEF(NS_ERROR_MALFORMED_URI                  , "The URI is malformed")
XPC_MSG_DEF(NS_ERROR_UNKNOWN_PROTOCOL               , "The URI scheme corresponds to an unknown protocol handler")
XPC_MSG_DEF(NS_ERROR_NO_CONTENT                     , "Channel opened successfully but no data will be returned")
XPC_MSG_DEF(NS_ERROR_IN_PROGRESS                    , "The requested action could not be completed while the object is busy")
XPC_MSG_DEF(NS_ERROR_ALREADY_OPENED                 , "Channel is already open")
XPC_MSG_DEF(NS_ERROR_INVALID_CONTENT_ENCODING       , "The content encoding of the source document is incorrect")
XPC_MSG_DEF(NS_ERROR_CORRUPTED_CONTENT              , "Corrupted content was received from server")
XPC_MSG_DEF(NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY, "Couldn't extract first component from potentially corrupted header field")
XPC_MSG_DEF(NS_ERROR_ALREADY_CONNECTED              , "The connection is already established")
XPC_MSG_DEF(NS_ERROR_NOT_CONNECTED                  , "The connection does not exist")
XPC_MSG_DEF(NS_ERROR_CONNECTION_REFUSED             , "The connection was refused")
XPC_MSG_DEF(NS_ERROR_PROXY_CONNECTION_REFUSED       , "The connection to the proxy server was refused")
XPC_MSG_DEF(NS_ERROR_NET_TIMEOUT                    , "The connection has timed out")
XPC_MSG_DEF(NS_ERROR_OFFLINE                        , "The requested action could not be completed in the offline state")
XPC_MSG_DEF(NS_ERROR_PORT_ACCESS_NOT_ALLOWED        , "Establishing a connection to an unsafe or otherwise banned port was prohibited")
XPC_MSG_DEF(NS_ERROR_NET_RESET                      , "The connection was established, but no data was ever received")
XPC_MSG_DEF(NS_ERROR_NET_INTERRUPT                  , "The connection was established, but the data transfer was interrupted")
XPC_MSG_DEF(NS_ERROR_NOT_RESUMABLE                  , "This request is not resumable, but it was tried to resume it, or to request resume-specific data")
XPC_MSG_DEF(NS_ERROR_ENTITY_CHANGED                 , "It was attempted to resume the request, but the entity has changed in the meantime")
XPC_MSG_DEF(NS_ERROR_REDIRECT_LOOP                  , "The request failed as a result of a detected redirection loop")
XPC_MSG_DEF(NS_ERROR_UNSAFE_CONTENT_TYPE            , "The request failed because the content type returned by the server was not a type expected by the channel")
XPC_MSG_DEF(NS_ERROR_REMOTE_XUL                     , "Attempt to access remote XUL document that is not in website's whitelist")

XPC_MSG_DEF(NS_ERROR_FTP_LOGIN                      , "FTP error while logging in")
XPC_MSG_DEF(NS_ERROR_FTP_CWD                        , "FTP error while changing directory")
XPC_MSG_DEF(NS_ERROR_FTP_PASV                       , "FTP error while changing to passive mode")
XPC_MSG_DEF(NS_ERROR_FTP_PWD                        , "FTP error while retrieving current directory")
XPC_MSG_DEF(NS_ERROR_FTP_LIST                       , "FTP error while retrieving a directory listing")
XPC_MSG_DEF(NS_ERROR_UNKNOWN_HOST                   , "The lookup of the hostname failed")
XPC_MSG_DEF(NS_ERROR_DNS_LOOKUP_QUEUE_FULL          , "The DNS lookup queue is full")
XPC_MSG_DEF(NS_ERROR_UNKNOWN_PROXY_HOST             , "The lookup of the proxy hostname failed")
XPC_MSG_DEF(NS_ERROR_UNKNOWN_SOCKET_TYPE            , "The specified socket type does not exist")
XPC_MSG_DEF(NS_ERROR_SOCKET_CREATE_FAILED           , "The specified socket type could not be created")
XPC_MSG_DEF(NS_ERROR_CACHE_KEY_NOT_FOUND            , "Cache key could not be found")
XPC_MSG_DEF(NS_ERROR_CACHE_DATA_IS_STREAM           , "Cache data is a stream")
XPC_MSG_DEF(NS_ERROR_CACHE_DATA_IS_NOT_STREAM       , "Cache data is not a stream")
XPC_MSG_DEF(NS_ERROR_CACHE_WAIT_FOR_VALIDATION      , "Cache entry exists but needs to be validated first")
XPC_MSG_DEF(NS_ERROR_CACHE_ENTRY_DOOMED             , "Cache entry has been  doomed")
XPC_MSG_DEF(NS_ERROR_CACHE_READ_ACCESS_DENIED       , "Read access to cache denied")
XPC_MSG_DEF(NS_ERROR_CACHE_WRITE_ACCESS_DENIED      , "Write access to cache denied")
XPC_MSG_DEF(NS_ERROR_CACHE_IN_USE                   , "Cache is currently in use")
XPC_MSG_DEF(NS_ERROR_DOCUMENT_NOT_CACHED            , "Document does not exist in cache")
XPC_MSG_DEF(NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS     , "The requested number of domain levels exceeds those present in the host string")
XPC_MSG_DEF(NS_ERROR_HOST_IS_IP_ADDRESS             , "The host string is an IP address")
XPC_MSG_DEF(NS_ERROR_NOT_SAME_THREAD                , "Can't access a wrapped JS object from a different thread")

/* storage related codes (from mozStorage.h) */
XPC_MSG_DEF(NS_ERROR_STORAGE_BUSY                   , "SQLite database connection is busy")
XPC_MSG_DEF(NS_ERROR_STORAGE_IOERR                  , "SQLite encountered an IO error")
XPC_MSG_DEF(NS_ERROR_STORAGE_CONSTRAINT             , "SQLite database operation failed because a constraint was violated")

/* plugin related codes (from nsPluginError.h) */
XPC_MSG_DEF(NS_ERROR_PLUGIN_TIME_RANGE_NOT_SUPPORTED, "Clearing site data by time range not supported by plugin")

/* character converter related codes (from nsIUnicodeDecoder.h) */
XPC_MSG_DEF(NS_ERROR_ILLEGAL_INPUT                  , "The input characters have illegal sequences")

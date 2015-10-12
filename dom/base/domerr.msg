/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Error Message definitions. */


/* DOM4 errors from http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#domexception */

DOM4_MSG_DEF(IndexSizeError, "Index or size is negative or greater than the allowed amount", NS_ERROR_DOM_INDEX_SIZE_ERR)
DOM4_MSG_DEF(HierarchyRequestError, "Node cannot be inserted at the specified point in the hierarchy", NS_ERROR_DOM_HIERARCHY_REQUEST_ERR)
DOM4_MSG_DEF(WrongDocumentError, "Node cannot be used in a document other than the one in which it was created", NS_ERROR_DOM_WRONG_DOCUMENT_ERR)
DOM4_MSG_DEF(InvalidCharacterError, "String contains an invalid character", NS_ERROR_DOM_INVALID_CHARACTER_ERR)
DOM4_MSG_DEF(NoModificationAllowedError, "Modifications are not allowed for this document", NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR)
DOM4_MSG_DEF(NotFoundError, "Node was not found", NS_ERROR_DOM_NOT_FOUND_ERR)
DOM4_MSG_DEF(NotSupportedError, "Operation is not supported", NS_ERROR_DOM_NOT_SUPPORTED_ERR)
DOM4_MSG_DEF(InUseAttributeError, "Attribute already in use", NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR)
DOM4_MSG_DEF(InvalidStateError, "An attempt was made to use an object that is not, or is no longer, usable", NS_ERROR_DOM_INVALID_STATE_ERR)
DOM4_MSG_DEF(SyntaxError, "An invalid or illegal string was specified", NS_ERROR_DOM_SYNTAX_ERR)
DOM4_MSG_DEF(InvalidModificationError, "An attempt was made to modify the type of the underlying objec", NS_ERROR_DOM_INVALID_MODIFICATION_ERR)
DOM4_MSG_DEF(NamespaceError, "An attempt was made to create or change an object in a way which is incorrect with regard to namespaces", NS_ERROR_DOM_NAMESPACE_ERR)
DOM4_MSG_DEF(InvalidAccessError, "A parameter or an operation is not supported by the underlying object", NS_ERROR_DOM_INVALID_ACCESS_ERR)
DOM4_MSG_DEF(TypeMismatchError, "The type of an object is incompatible with the expected type of the parameter associated to the object", NS_ERROR_DOM_TYPE_MISMATCH_ERR)
DOM4_MSG_DEF(SecurityError, "The operation is insecure.", NS_ERROR_DOM_SECURITY_ERR)
DOM4_MSG_DEF(NetworkError, "A network error occurred.", NS_ERROR_DOM_NETWORK_ERR)
DOM4_MSG_DEF(AbortError, "The operation was aborted. ", NS_ERROR_DOM_ABORT_ERR)
DOM4_MSG_DEF(URLMismatchError, "The given URL does not match another URL.", NS_ERROR_DOM_URL_MISMATCH_ERR)
DOM4_MSG_DEF(QuotaExceededError, "The quota has been exceeded.", NS_ERROR_DOM_QUOTA_EXCEEDED_ERR)
DOM4_MSG_DEF(TimeoutError, "The operation timed out.", NS_ERROR_DOM_TIMEOUT_ERR)
DOM4_MSG_DEF(InvalidNodeTypeError, "The supplied node is incorrect or has an incorrect ancestor for this operation.", NS_ERROR_DOM_INVALID_NODE_TYPE_ERR)
DOM4_MSG_DEF(DataCloneError, "The object could not be cloned.", NS_ERROR_DOM_DATA_CLONE_ERR)
DOM4_MSG_DEF(InvalidPointerId, "Invalid pointer id.", NS_ERROR_DOM_INVALID_POINTER_ERR)

/* XXX Should be JavaScript native TypeError */
DOM4_MSG_DEF(TypeError, "The method parameter is missing or invalid.", NS_ERROR_TYPE_ERR)
DOM4_MSG_DEF(RangeError, "The method parameter is out of valid range.", NS_ERROR_RANGE_ERR)

/* StringEncoding API errors from http://wiki.whatwg.org/wiki/StringEncoding */
DOM4_MSG_DEF(EncodingError, "The given encoding is not supported.", NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR)

/* WebCrypto API errors from http://www.w3.org/TR/WebCryptoAPI/ */
DOM4_MSG_DEF(UnknownError, "The operation failed for an unknown transient reason", NS_ERROR_DOM_UNKNOWN_ERR)
DOM4_MSG_DEF(DataError, "Data provided to an operation does not meet requirements", NS_ERROR_DOM_DATA_ERR)
DOM4_MSG_DEF(OperationError, "The operation failed for an operation-specific reason", NS_ERROR_DOM_OPERATION_ERR)

/* SVG DOM errors from http://www.w3.org/TR/SVG11/svgdom.html */

DOM4_MSG_DEF(TypeError, "Unknown or invalid type", NS_ERROR_DOM_SVG_WRONG_TYPE_ERR)
DOM4_MSG_DEF(InvalidStateError, "The matrix could not be computed", NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE)

/* XPath errors from http://www.w3.org/TR/DOM-Level-3-XPath/ */

DOM4_MSG_DEF(SyntaxError, "The expression is not a legal expression.", NS_ERROR_DOM_INVALID_EXPRESSION_ERR)
DOM4_MSG_DEF(TypeError, "The expression cannot be converted to return the specified type.", NS_ERROR_DOM_TYPE_ERR)

/* IndexedDB errors http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#exceptions */

DOM4_MSG_DEF(UnknownError, "The operation failed for reasons unrelated to the database itself and not covered by any other error code.", NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR)
DOM4_MSG_DEF(ConstraintError, "A mutation operation in the transaction failed because a constraint was not satisfied. For example, an object such as an object store or index already exists and a new one was being attempted to be created.", NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR)
DOM4_MSG_DEF(DataError, "Data provided to an operation does not meet requirements.", NS_ERROR_DOM_INDEXEDDB_DATA_ERR)
DOM4_MSG_DEF(TransactionInactiveError, "A request was placed against a transaction which is currently not active, or which is finished.", NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR)
DOM4_MSG_DEF(ReadOnlyError, "A mutation operation was attempted in a READ_ONLY transaction.", NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR)
DOM4_MSG_DEF(VersionError, "The operation failed because the stored database is a higher version than the version requested.", NS_ERROR_DOM_INDEXEDDB_VERSION_ERR)

DOM4_MSG_DEF(NotFoundError, "The operation failed because the requested database object could not be found. For example, an object store did not exist but was being opened.", NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR)
DOM4_MSG_DEF(InvalidStateError, "A mutation operation was attempted on a database that did not allow mutations.", NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR)
DOM4_MSG_DEF(AbortError, "A request was aborted, for example through a call to IDBTransaction.abort.", NS_ERROR_DOM_INDEXEDDB_ABORT_ERR)
DOM4_MSG_DEF(TimeoutError, "A lock for the transaction could not be obtained in a reasonable time.", NS_ERROR_DOM_INDEXEDDB_TIMEOUT_ERR)
DOM4_MSG_DEF(QuotaExceededError, "The current transaction exceeded its quota limitations.", NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR)

/* A Non-standard IndexedDB error */

DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_RECOVERABLE_ERR, "The operation failed because the database was prevented from taking an action. The operation might be able to succeed if the application performs some recovery steps and retries the entire transaction. For example, there was not enough remaining storage space, or the storage quota was reached and the user declined to give more space to the database.")

/* FileSystem DOM errors. */
DOM4_MSG_DEF(InvalidAccessError, "Invalid file system path.", NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR)
DOM4_MSG_DEF(InvalidModificationError, "Failed to modify the file.", NS_ERROR_DOM_FILESYSTEM_INVALID_MODIFICATION_ERR)
DOM4_MSG_DEF(NoModificationAllowedError, "Modifications are not allowed for this file", NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR)
DOM4_MSG_DEF(AbortError, "File already exists.", NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR)
DOM4_MSG_DEF(TypeMismatchError, "The type of the file is incompatible with the expected type.", NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR)
DOM4_MSG_DEF(UnknownError, "The operation failed for reasons unrelated to the file system itself and not covered by any other error code.", NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR)

/* DOM error codes defined by us */

/* XXX string should be specified by norris */
DOM_MSG_DEF(NS_ERROR_DOM_SECMAN_ERR, "Unable to obtain security manager")
DOM_MSG_DEF(NS_ERROR_DOM_WRONG_TYPE_ERR, "Object is of wrong type")
DOM_MSG_DEF(NS_ERROR_DOM_NOT_OBJECT_ERR, "Parameter is not an object")
DOM_MSG_DEF(NS_ERROR_DOM_NOT_XPC_OBJECT_ERR, "Parameter is not a XPConnect object")
DOM_MSG_DEF(NS_ERROR_DOM_NOT_NUMBER_ERR, "Parameter is not a number")
DOM_MSG_DEF(NS_ERROR_DOM_NOT_BOOLEAN_ERR, "Parameter is not a boolean")
DOM_MSG_DEF(NS_ERROR_DOM_NOT_FUNCTION_ERR, "Parameter is not a Function")
DOM_MSG_DEF(NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR, "Too few parameters to method")
DOM_MSG_DEF(NS_ERROR_DOM_BAD_DOCUMENT_DOMAIN, "Illegal document.domain value")
DOM_MSG_DEF(NS_ERROR_DOM_PROP_ACCESS_DENIED, "Access to property denied")
DOM_MSG_DEF(NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED, "Access to XPConnect service denied")
DOM_MSG_DEF(NS_ERROR_DOM_BAD_URI, "Access to restricted URI denied")
DOM_MSG_DEF(NS_ERROR_DOM_RETVAL_UNDEFINED, "Return value is undefined")
DOM_MSG_DEF(NS_ERROR_DOM_QUOTA_REACHED, "Persistent storage maximum size reached")

DOM4_MSG_DEF(NotFoundError, "File was not found", NS_ERROR_DOM_FILE_NOT_FOUND_ERR)
DOM4_MSG_DEF(NotReadableError, "File could not be read", NS_ERROR_DOM_FILE_NOT_READABLE_ERR)

/* Non-standard Bluetooth DOM errors. */
DOM4_MSG_DEF(BtFailError,         "Fail",  NS_ERROR_DOM_BLUETOOTH_FAIL)
DOM4_MSG_DEF(BtNotReadyError,     "Not ready",  NS_ERROR_DOM_BLUETOOTH_NOT_READY)
DOM4_MSG_DEF(BtNoMemError,        "No memory",  NS_ERROR_DOM_BLUETOOTH_NOMEM)
DOM4_MSG_DEF(BtBusyError,         "Device busy with another command",  NS_ERROR_DOM_BLUETOOTH_BUSY)
DOM4_MSG_DEF(BtDoneError,         "Request already done",  NS_ERROR_DOM_BLUETOOTH_DONE)
DOM4_MSG_DEF(BtUnsupportedError,  "Unsupported",  NS_ERROR_DOM_BLUETOOTH_UNSUPPORTED)
DOM4_MSG_DEF(BtParmInvalidError,  "Invalid parameter",  NS_ERROR_DOM_BLUETOOTH_PARM_INVALID)
DOM4_MSG_DEF(BtUnhandledError,    "Unhandled",  NS_ERROR_DOM_BLUETOOTH_UNHANDLED)
DOM4_MSG_DEF(BtAuthFailureError,  "Authentication failure",  NS_ERROR_DOM_BLUETOOTH_AUTH_FAILURE)
DOM4_MSG_DEF(BtRmtDevDownError,   "Remote device down",  NS_ERROR_DOM_BLUETOOTH_RMT_DEV_DOWN)
DOM4_MSG_DEF(BtAuthRejectedError, "Authentication rejected",  NS_ERROR_DOM_BLUETOOTH_AUTH_REJECTED)

/* common global codes (from nsError.h) */

DOM_MSG_DEF(NS_OK                                  , "Success")
DOM_MSG_DEF(NS_ERROR_NOT_INITIALIZED               , "Component not initialized")
DOM_MSG_DEF(NS_ERROR_ALREADY_INITIALIZED           , "Component already initialized")
DOM_MSG_DEF(NS_ERROR_NOT_IMPLEMENTED               , "Method not implemented")
DOM_MSG_DEF(NS_NOINTERFACE                         , "Component does not have requested interface")
DOM_MSG_DEF(NS_ERROR_NO_INTERFACE                  , "Component does not have requested interface")
DOM_MSG_DEF(NS_ERROR_INVALID_POINTER               , "Invalid pointer")
DOM_MSG_DEF(NS_ERROR_NULL_POINTER                  , "Null pointer")
DOM_MSG_DEF(NS_ERROR_ABORT                         , "Abort")
DOM_MSG_DEF(NS_ERROR_FAILURE                       , "Failure")
DOM_MSG_DEF(NS_ERROR_UNEXPECTED                    , "Unexpected error")
DOM_MSG_DEF(NS_ERROR_OUT_OF_MEMORY                 , "Out of Memory")
DOM_MSG_DEF(NS_ERROR_ILLEGAL_VALUE                 , "Illegal value")
DOM_MSG_DEF(NS_ERROR_INVALID_ARG                   , "Invalid argument")
DOM_MSG_DEF(NS_ERROR_NO_AGGREGATION                , "Component does not support aggregation")
DOM_MSG_DEF(NS_ERROR_NOT_AVAILABLE                 , "Component is not available")
DOM_MSG_DEF(NS_ERROR_FACTORY_NOT_REGISTERED        , "Factory not registered")
DOM_MSG_DEF(NS_ERROR_FACTORY_NOT_LOADED            , "Factory not loaded")
DOM_MSG_DEF(NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT  , "Factory does not support signatures")
DOM_MSG_DEF(NS_ERROR_FACTORY_EXISTS                , "Factory already exists")

DOM4_MSG_DEF(UnknownError, "The operation failed for reasons unrelated to the file storage itself and not covered by any other error code.", NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR)
DOM4_MSG_DEF(FileHandleInactiveError, "A request was placed against a file handle which is currently not active, or which is finished.", NS_ERROR_DOM_FILEHANDLE_INACTIVE_ERR)
DOM4_MSG_DEF(ReadOnlyError, "A mutation operation was attempted in a READ_ONLY file handle.", NS_ERROR_DOM_FILEHANDLE_READ_ONLY_ERR)

DOM4_MSG_DEF(InvalidStateError, "A mutation operation was attempted on a file storage that did not allow mutations.", NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR)
DOM4_MSG_DEF(AbortError, "A request was aborted, for example through a call to FileHandle.abort.", NS_ERROR_DOM_FILEHANDLE_ABORT_ERR)
DOM4_MSG_DEF(QuotaExceededError, "The current file handle exceeded its quota limitations.", NS_ERROR_DOM_FILEHANDLE_QUOTA_ERR)

DOM_MSG_DEF(NS_ERROR_DOM_JS_EXCEPTION, "A callback threw an exception")
DOM_MSG_DEF(NS_ERROR_DOM_DOMEXCEPTION, "A DOMException was thrown")

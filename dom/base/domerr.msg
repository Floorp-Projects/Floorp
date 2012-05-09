/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
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

/* XXX Should be JavaScript native TypeError */
DOM4_MSG_DEF(TypeError, "The method parameter is missing or invalid.", NS_ERROR_TYPE_ERR)

/* SVG DOM error codes from http://www.w3.org/TR/SVG11/svgdom.html */

DOM_MSG_DEF(NS_ERROR_DOM_SVG_WRONG_TYPE_ERR, "Unknown or invalid type")
DOM_MSG_DEF(NS_ERROR_DOM_SVG_INVALID_VALUE_ERR, "One of the parameters has an invalid value")
DOM_MSG_DEF(NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE, "The matrix could not be computed")

/* DOM error codes from http://www.w3.org/TR/DOM-Level-3-XPath/ */

DOM_MSG_DEF(NS_ERROR_DOM_INVALID_EXPRESSION_ERR, "The expression is not a legal expression.")
DOM_MSG_DEF(NS_ERROR_DOM_TYPE_ERR, "The expression cannot be converted to return the specified type.")

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

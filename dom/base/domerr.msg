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


/* DOM error codes from http://www.w3.org/TR/REC-DOM-Level-1/ */

DOM_MSG_DEF(NS_ERROR_DOM_INDEX_SIZE_ERR, "Index or size is negative or greater than the allowed amount")
DOM_MSG_DEF(NS_ERROR_DOM_DOMSTRING_SIZE_ERR, "The specified range of text does not fit in a DOM string")
DOM_MSG_DEF(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR, "Node cannot be inserted at the specified point in the hierarchy")
DOM_MSG_DEF(NS_ERROR_DOM_WRONG_DOCUMENT_ERR, "Node cannot be used in a document other than the one in which it was created")
DOM_MSG_DEF(NS_ERROR_DOM_INVALID_CHARACTER_ERR, "String contains an invalid character")
DOM_MSG_DEF(NS_ERROR_DOM_NO_DATA_ALLOWED_ERR, "Node does not contain data")
DOM_MSG_DEF(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, "Modifications are not allowed for this document")
DOM_MSG_DEF(NS_ERROR_DOM_NOT_FOUND_ERR, "Node was not found")
DOM_MSG_DEF(NS_ERROR_DOM_NOT_SUPPORTED_ERR, "Operation is not supported")
DOM_MSG_DEF(NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR, "Attribute already in use")

/* DOM error codes from http://www.w3.org/TR/DOM-Level-2/ */

DOM_MSG_DEF(NS_ERROR_DOM_INVALID_ACCESS_ERR, "A parameter or an operation is not supported by the underlying object")
DOM_MSG_DEF(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, "An attempt was made to modify the type of the underlying objec")
DOM_MSG_DEF(NS_ERROR_DOM_INVALID_STATE_ERR, "An attempt was made to use an object that is not, or is no longer, usable")
DOM_MSG_DEF(NS_ERROR_DOM_NAMESPACE_ERR, "An attempt was made to create or change an object in a way which is incorrect with regard to namespaces")
DOM_MSG_DEF(NS_ERROR_DOM_SYNTAX_ERR, "An invalid or illegal string was specified")

/* DOM error codes from http://www.w3.org/TR/DOM-Level-2/range.html */

DOM_MSG_DEF(NS_ERROR_DOM_RANGE_BAD_BOUNDARYPOINTS_ERR, "The boundary-points of a range does not meet specific requirements.")
DOM_MSG_DEF(NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR, "The container of an boundary-point of a range is being set to either a node of an invalid type or a node with an ancestor of an invalid type.")

/* DOM error codes from http://www.w3.org/TR/DOM-Level-3/ */

DOM_MSG_DEF(NS_ERROR_DOM_VALIDATION_ERR, "A call to a method would make the Node invalid with respect to \"partial validity\", so the operation was not done")
DOM_MSG_DEF(NS_ERROR_DOM_TYPE_MISMATCH_ERR, "The type of an object is incompatible with the expected type of the parameter associated to the object")

/* SVG DOM error codes from http://www.w3.org/TR/SVG11/svgdom.html */

DOM_MSG_DEF(NS_ERROR_DOM_SVG_WRONG_TYPE_ERR, "Unknown or invalid type")
DOM_MSG_DEF(NS_ERROR_DOM_SVG_INVALID_VALUE_ERR, "One of the parameters has an invalid value")
DOM_MSG_DEF(NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE, "The matrix could not be computed")

/* DOM error codes from http://www.w3.org/TR/DOM-Level-3-XPath/ */

DOM_MSG_DEF(NS_ERROR_DOM_INVALID_EXPRESSION_ERR, "The expression is not a legal expression.")
DOM_MSG_DEF(NS_ERROR_DOM_TYPE_ERR, "The expression cannot be converted to return the specified type.")

/* HTML5 error codes http://dev.w3.org/html5/spec/Overview.html */

DOM_MSG_DEF(NS_ERROR_DOM_DATA_CLONE_ERR, "The object could not be cloned.")

/* IndexedDB error codes http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html */

DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, "The operation failed for reasons unrelated to the database itself and not covered by any other error code.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR, "This error occurred because an operation was not allowed on an object. A retry of the same operation would fail unless the cause of the error is corrected.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR, "The operation failed because the requested database object could not be found. For example, an object store did not exist but was being opened.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR, "A mutation operation in the transaction failed because a constraint was not satisfied. For example, an object such as an object store or index already exists and a new one was being attempted to be created.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_DATA_ERR, "Data provided to an operation does not meet requirements.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR, "A mutation operation was attempted on a database that did not allow mutations.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR, "A request was placed against a transaction which is currently not active, or which is finished.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR, "A request was aborted, for example through a call to IDBTransaction.abort.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR, "A mutation operation was attempted in a READ_ONLY transaction.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_RECOVERABLE_ERR, "The operation failed because the database was prevented from taking an action. The operation might be able to succeed if the application performs some recovery steps and retries the entire transaction. For example, there was not enough remaining storage space, or the storage quota was reached and the user declined to give more space to the database.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_TRANSIENT_ERR, "The operation failed because of some temporary problems. The failed operation might be able to succeed when the operation is retried without any intervention by application-level functionality.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_TIMEOUT_ERR, "A lock for the transaction could not be obtained in a reasonable time.")
DOM_MSG_DEF(NS_ERROR_DOM_INDEXEDDB_DEADLOCK_ERR, "The current transaction was automatically rolled back by the database because of deadlock or other transaction serialization failures.")

/* DOM error codes defined by us */

/* XXX string should be specified by norris */
DOM_MSG_DEF(NS_ERROR_DOM_SECURITY_ERR, "Security error")
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

DOM_MSG_DEF(NS_ERROR_DOM_FILE_NOT_FOUND_ERR, "File was not found")
DOM_MSG_DEF(NS_ERROR_DOM_FILE_NOT_READABLE_ERR, "File could not be read")

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

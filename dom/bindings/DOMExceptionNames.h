/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how DOMEXCEPTION is defined, possibly multiple times in a
// single translation unit.

// XXXbz This list sort of duplicates the DOM4_MSG_DEF bits of domerr.msg,
// except that has various extra errors that are not in specs
// (e.g. EncodingError) and has multiple definitions for the same error
// name using different messages, which we don't need because we get the
// message passed in.  We should try to convert all consumers of the "extra"
// error codes in there to these APIs, remove the extra bits, and just
// include domerr.msg here.
DOMEXCEPTION(IndexSizeError, NS_ERROR_DOM_INDEX_SIZE_ERR)
// We don't have a DOMStringSizeError and it's deprecated anyway.
DOMEXCEPTION(HierarchyRequestError, NS_ERROR_DOM_HIERARCHY_REQUEST_ERR)
DOMEXCEPTION(WrongDocumentError, NS_ERROR_DOM_WRONG_DOCUMENT_ERR)
DOMEXCEPTION(InvalidCharacterError, NS_ERROR_DOM_INVALID_CHARACTER_ERR)
// We don't have a NoDataAllowedError and it's deprecated anyway.
DOMEXCEPTION(NoModificationAllowedError,
             NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR)
DOMEXCEPTION(NotFoundError, NS_ERROR_DOM_NOT_FOUND_ERR)
DOMEXCEPTION(NotSupportedError, NS_ERROR_DOM_NOT_SUPPORTED_ERR)
DOMEXCEPTION(InUseAttributeError, NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR)
DOMEXCEPTION(InvalidStateError, NS_ERROR_DOM_INVALID_STATE_ERR)
DOMEXCEPTION(SyntaxError, NS_ERROR_DOM_SYNTAX_ERR)
DOMEXCEPTION(InvalidModificationError, NS_ERROR_DOM_INVALID_MODIFICATION_ERR)
DOMEXCEPTION(NamespaceError, NS_ERROR_DOM_NAMESPACE_ERR)
DOMEXCEPTION(InvalidAccessError, NS_ERROR_DOM_INVALID_ACCESS_ERR)
// We don't have a ValidationError and it's deprecated anyway.
DOMEXCEPTION(TypeMismatchError, NS_ERROR_DOM_TYPE_MISMATCH_ERR)
DOMEXCEPTION(SecurityError, NS_ERROR_DOM_SECURITY_ERR)
DOMEXCEPTION(NetworkError, NS_ERROR_DOM_NETWORK_ERR)
DOMEXCEPTION(AbortError, NS_ERROR_DOM_ABORT_ERR)
DOMEXCEPTION(URLMismatchError, NS_ERROR_DOM_URL_MISMATCH_ERR)
DOMEXCEPTION(QuotaExceededError, NS_ERROR_DOM_QUOTA_EXCEEDED_ERR)
DOMEXCEPTION(TimeoutError, NS_ERROR_DOM_TIMEOUT_ERR)
DOMEXCEPTION(InvalidNodeTypeError, NS_ERROR_DOM_INVALID_NODE_TYPE_ERR)
DOMEXCEPTION(DataCloneError, NS_ERROR_DOM_DATA_CLONE_ERR)
DOMEXCEPTION(EncodingError, NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR)
DOMEXCEPTION(NotReadableError, NS_ERROR_DOM_FILE_NOT_READABLE_ERR)
DOMEXCEPTION(UnknownError, NS_ERROR_DOM_UNKNOWN_ERR)
DOMEXCEPTION(ConstraintError, NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR)
DOMEXCEPTION(DataError, NS_ERROR_DOM_DATA_ERR)
DOMEXCEPTION(TransactionInactiveError,
             NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR)
DOMEXCEPTION(ReadOnlyError, NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR)
DOMEXCEPTION(VersionError, NS_ERROR_DOM_INDEXEDDB_VERSION_ERR)
DOMEXCEPTION(OperationError, NS_ERROR_DOM_OPERATION_ERR)
DOMEXCEPTION(NotAllowedError, NS_ERROR_DOM_NOT_ALLOWED_ERR)

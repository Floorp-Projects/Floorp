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

#ifndef nsDOMError_h__
#define nsDOMError_h__

#include "nsError.h"

// XXX If you add a new error code, also add an error string to
// dom/src/base/domerr.msg

/* DOM error codes from http://www.w3.org/TR/REC-DOM-Level-1/ */

#define NS_ERROR_DOM_INDEX_SIZE_ERR              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1)
#define NS_ERROR_DOM_DOMSTRING_SIZE_ERR          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,2)
#define NS_ERROR_DOM_HIERARCHY_REQUEST_ERR       NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,3)
#define NS_ERROR_DOM_WRONG_DOCUMENT_ERR          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,4)
#define NS_ERROR_DOM_INVALID_CHARACTER_ERR       NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,5)
#define NS_ERROR_DOM_NO_DATA_ALLOWED_ERR         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,6)
#define NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,7)
#define NS_ERROR_DOM_NOT_FOUND_ERR               NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,8)
#define NS_ERROR_DOM_NOT_SUPPORTED_ERR           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,9)
#define NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,10)

/* DOM error codes from http://www.w3.org/TR/DOM-Level-2/ */

#define NS_ERROR_DOM_INVALID_STATE_ERR           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,11)
#define NS_ERROR_DOM_SYNTAX_ERR                  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,12)
#define NS_ERROR_DOM_INVALID_MODIFICATION_ERR    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,13)
#define NS_ERROR_DOM_NAMESPACE_ERR               NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,14)
#define NS_ERROR_DOM_INVALID_ACCESS_ERR          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,15)

/* DOM error codes from http://www.w3.org/TR/DOM-Level-2/range.html */

#define NS_ERROR_DOM_RANGE_BAD_BOUNDARYPOINTS_ERR NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM_RANGE, 1)
#define NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM_RANGE, 2)

/* DOM error codes from http://www.w3.org/TR/DOM-Level-3/ */

#define NS_ERROR_DOM_VALIDATION_ERR              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,16)
#define NS_ERROR_DOM_TYPE_MISMATCH_ERR           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,17)


/* DOM error codes defined by us */

#define NS_ERROR_DOM_SECURITY_ERR                NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1000)
#define NS_ERROR_DOM_SECMAN_ERR                  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1001)
#define NS_ERROR_DOM_WRONG_TYPE_ERR              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1002)
#define NS_ERROR_DOM_NOT_OBJECT_ERR              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1003)
#define NS_ERROR_DOM_NOT_XPC_OBJECT_ERR          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1004)
#define NS_ERROR_DOM_NOT_NUMBER_ERR              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1005)
#define NS_ERROR_DOM_NOT_BOOLEAN_ERR             NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1006)
#define NS_ERROR_DOM_NOT_FUNCTION_ERR            NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1007)
#define NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR      NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1008)
#define NS_ERROR_DOM_BAD_DOCUMENT_DOMAIN         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1009)
#define NS_ERROR_DOM_PROP_ACCESS_DENIED          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1010)
#define NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED     NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1011)
#define NS_ERROR_DOM_BAD_URI                     NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1012)
#define NS_ERROR_DOM_RETVAL_UNDEFINED            NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,1013)

#endif // nsDOMError_h__

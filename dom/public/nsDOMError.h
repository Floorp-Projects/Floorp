/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDOMError_h__
#define nsDOMError_h__

#include "nsError.h"

// XXX If you add a new error code, also add an error string to
// dom/base/src/domerr.msg

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

#define NS_ERROR_DOM_INVALID_ACCESS_ERR          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,11)
#define NS_ERROR_DOM_INVALID_MODIFICATION_ERR    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,12)
#define NS_ERROR_DOM_INVALID_STATE_ERR           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,13)
#define NS_ERROR_DOM_NAMESPACE_ERR               NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,14)
#define NS_ERROR_DOM_SYNTAX_ERR                  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM,15)

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

// XXX Not the right place for this.
#include "nsIDOMDOMException.h"

extern "C" NS_DOM nsresult NS_NewDOMException(nsIDOMDOMException** aException,
                                              nsresult aResult, 
                                              const char* aName, 
                                              const char* aMessage,
                                              const char* aLocation);

#endif // nsDOMError_h__

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsDOMError_h__
#define nsDOMError_h__

#include "nsError.h"

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


// XXX Not the right place for this.
#include "nsIDOMDOMException.h"

extern "C" NS_DOM nsresult NS_NewDOMException(nsIDOMDOMException** aException,
                                              nsresult aResult, 
                                              const char* aName, 
                                              const char* aMessage,
                                              const char* aLocation);

#endif // nsDOMError_h__

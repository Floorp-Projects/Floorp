/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIGenericFactory.h"

#include "nsTextServicesDocument.h"
#include "nsFindAndReplace.h"
#include "nsTextServicesCID.h"

#define NS_TEXTSERVICESFINDANDREPLACE_CID       \
{ /* 8B0EEFE1-C4AE-11d4-A401-000064657374 */    \
0x8b0eefe1, 0xc4ae, 0x11d4,                     \
{ 0xa4, 0x1, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }


////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextServicesDocument)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFindAndReplace)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
  { NULL, NS_TEXTSERVICESDOCUMENT_CID, "@mozilla.org/textservices/textservicesdocument;1", nsTextServicesDocumentConstructor },
  { NULL, NS_TEXTSERVICESFINDANDREPLACE_CID, NS_FINDANDREPLACE_CONTRACTID, nsFindAndReplaceConstructor },
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE(nsTextServicesModule, components)

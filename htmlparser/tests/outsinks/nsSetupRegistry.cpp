/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

#define NS_IMPL_IDS

#include "nsParserCIID.h"
#include "nsDOMCID.h"

#ifdef XP_PC
    #define PARSER_DLL "gkparser.dll"
    #define DOM_DLL    "jsdom.dll"

#elif defined(XP_MAC)
    #define PARSER_DLL    "PARSER_DLL"
    #define DOM_DLL        "DOM_DLL"

#else /* XP_UNIX etc. */
    #define PARSER_DLL "libhtmlpars"MOZ_DLL_SUFFIX
    #define DOM_DLL    "libjsdom"MOZ_DLL_SUFFIX
#endif

// Class ID's
// PARSER
static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);
static NS_DEFINE_CID(kCWellFormedDTDCID, NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kCNavDTDCID, NS_CNAVDTD_CID);

// DOM
static NS_DEFINE_IID(kCDOMScriptObjectFactory, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_IID(kCScriptNameSetRegistry, NS_SCRIPT_NAMESET_REGISTRY_CID);


extern "C" void
NS_SetupRegistry()
{
  nsComponentManager::RegisterComponentLib(kCParserCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCWellFormedDTDCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCNavDTDCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);

  // DOM
  nsComponentManager::RegisterComponentLib(kCDOMScriptObjectFactory, NULL, NULL, DOM_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kCScriptNameSetRegistry, NULL, NULL, DOM_DLL, PR_FALSE, PR_FALSE);
}


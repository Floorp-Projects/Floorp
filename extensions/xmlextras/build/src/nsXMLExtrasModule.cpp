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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsDOMSerializer.h"
#include "nsXMLHttpRequest.h"
#include "nsDOMParser.h"
#include "nsIAppStartupNotifier.h"
#include "nsICategoryManager.h"
#include "nsIObserver.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsDOMCID.h"
#include "prprf.h"
#include "nsIDOMClassInfo.h"
#include "nsCRT.h"
#include "nsFIXptr.h"
#include "nsXPointer.h"

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMSerializer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXMLHttpRequest)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFIXptr)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPointer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPointerResult)

NS_DECL_DOM_CLASSINFO(XMLSerializer)
NS_DECL_DOM_CLASSINFO(XMLHttpRequest)
NS_DECL_DOM_CLASSINFO(DOMParser)
NS_DECL_DOM_CLASSINFO(XPointerResult)

/* 6fb64081-1da6-11d6-a7f2-9babb25552bc */
#define XMLEXTRAS_DOMCI_EXTENSION_CID   \
{ 0x6fb64081, 0x1da6, 0x11d6, {0xa7, 0xf2, 0x9b, 0xab, 0xb2, 0x55, 0x52, 0xbc} }

#define XMLEXTRAS_DOMCI_EXTENSION_CONTRACTID \
"@mozilla.org/xmlextras-domci-extender;1"

NS_DOMCI_EXTENSION(XMLExtras)
    static NS_DEFINE_CID(kXMLSerializerCID, NS_XMLSERIALIZER_CID);
    NS_DOMCI_EXTENSION_ENTRY_BEGIN(XMLSerializer)
        NS_DOMCI_EXTENSION_ENTRY_INTERFACE(nsIDOMSerializer)
    NS_DOMCI_EXTENSION_ENTRY_END_NO_PRIMARY_IF(XMLSerializer, PR_TRUE, &kXMLSerializerCID)

    static NS_DEFINE_CID(kXMLHttpRequestCID, NS_XMLHTTPREQUEST_CID);
    NS_DOMCI_EXTENSION_ENTRY_BEGIN(XMLHttpRequest)
        NS_DOMCI_EXTENSION_ENTRY_INTERFACE(nsIXMLHttpRequest)
        NS_DOMCI_EXTENSION_ENTRY_INTERFACE(nsIJSXMLHttpRequest)
        NS_DOMCI_EXTENSION_ENTRY_INTERFACE(nsIDOMEventTarget)
    NS_DOMCI_EXTENSION_ENTRY_END_NO_PRIMARY_IF(XMLHttpRequest, PR_TRUE, &kXMLHttpRequestCID)

    static NS_DEFINE_CID(kDOMParserCID, NS_DOMPARSER_CID);
    NS_DOMCI_EXTENSION_ENTRY_BEGIN(DOMParser)
        NS_DOMCI_EXTENSION_ENTRY_INTERFACE(nsIDOMParser)
    NS_DOMCI_EXTENSION_ENTRY_END_NO_PRIMARY_IF(DOMParser, PR_TRUE, &kDOMParserCID)

    NS_DOMCI_EXTENSION_ENTRY_BEGIN(XPointerResult)
        NS_DOMCI_EXTENSION_ENTRY_INTERFACE(nsIXPointerResult)
    NS_DOMCI_EXTENSION_ENTRY_END_NO_PRIMARY_IF(XPointerResult, PR_TRUE, nsnull)
NS_DOMCI_EXTENSION_END

#define NS_XPOINTER_RESULT_CID                       \
{ /* 31CC4700-FC53-41d2-90D4-8D608F67AD39 */         \
  0x31cc4700, 0xfc53, 0x41d2,                        \
{ 0x90, 0xd4, 0x8d, 0x60, 0x8f, 0x67, 0xad, 0x39 } }
#define NS_XPOINTER_RESULT_CONTRACTID \
"@mozilla.org/xmlextras/xpointerresult;1"

// {CD6BD59C-75C8-451b-831B-B81B7C03DC53}
#define NS_FIXPTR_EVALUATOR_CID \
  { 0xcd6bd59c, 0x75c8, 0x451b, \
    { 0x83, 0x1b, 0xb8, 0x1b, 0x7c, 0x3, 0xdc, 0x53 } }
#define NS_FIXPTR_EVALUATOR_CONTRACTID \
"@mozilla.org/xmlextras/fixptrevaluator;1"

// {07AC4E3A-EE18-4e37-B29F-532BE4BE0609}
#define NS_XPOINTER_EVALUATOR_CID \
  { 0x7ac4e3a, 0xee18, 0x4e37, \
    { 0xb2, 0x9f, 0x53, 0x2b, 0xe4, 0xbe, 0x6, 0x9 } }
#define NS_XPOINTER_EVALUATOR_CONTRACTID \
"@mozilla.org/xmlextras/xpointerevaluator;1"

static NS_METHOD 
RegisterXMLExtras(nsIComponentManager *aCompMgr,
                  nsIFile *aPath,
                  const char *registryLocation,
                  const char *componentType,
                  const nsModuleComponentInfo *info)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);

  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString previous;
  rv = catman->AddCategoryEntry(JAVASCRIPT_DOM_CLASS,
                                "XMLSerializer",
                                XMLEXTRAS_DOMCI_EXTENSION_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_DOM_CLASS,
                                "XMLHttpRequest",
                                XMLEXTRAS_DOMCI_EXTENSION_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_DOM_CLASS,
                                "DOMParser",
                                XMLEXTRAS_DOMCI_EXTENSION_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_DOM_CLASS,
                                "XPointerResult",
                                XMLEXTRAS_DOMCI_EXTENSION_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  char* iidString = NS_GET_IID(nsIXMLHttpRequest).ToString();
  if (!iidString)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = catman->AddCategoryEntry(JAVASCRIPT_DOM_INTERFACE,
                                "nsIXMLHttpRequest",
                                iidString,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  nsCRT::free(iidString);
  NS_ENSURE_SUCCESS(rv, rv);

  iidString = NS_GET_IID(nsIJSXMLHttpRequest).ToString();
  if (!iidString)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = catman->AddCategoryEntry(JAVASCRIPT_DOM_INTERFACE,
                                "nsIJSXMLHttpRequest",
                                iidString,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  nsCRT::free(iidString);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static const nsModuleComponentInfo components[] = {
  { "XML Serializer", NS_XMLSERIALIZER_CID, NS_XMLSERIALIZER_CONTRACTID,
    nsDOMSerializerConstructor,
    RegisterXMLExtras /* Register all of the components in one go */ },
  { "XMLHttpRequest", NS_XMLHTTPREQUEST_CID, NS_XMLHTTPREQUEST_CONTRACTID,
    nsXMLHttpRequestConstructor },
  { "DOM Parser", NS_DOMPARSER_CID, NS_DOMPARSER_CONTRACTID,
    nsDOMParserConstructor },
  { "FIXptr Evaluator", NS_FIXPTR_EVALUATOR_CID, NS_FIXPTR_EVALUATOR_CONTRACTID,
    nsFIXptrConstructor },
  { "XPointer Evaluator", NS_XPOINTER_EVALUATOR_CID, NS_XPOINTER_EVALUATOR_CONTRACTID,
    nsXPointerConstructor },
  { "XPointer Result", NS_XPOINTER_RESULT_CID, NS_XPOINTER_RESULT_CONTRACTID,
    nsXPointerResultConstructor },
  { "XML Extras DOMCI Extender",
    XMLEXTRAS_DOMCI_EXTENSION_CID, XMLEXTRAS_DOMCI_EXTENSION_CONTRACTID,
    NS_DOMCI_EXTENSION_CONSTRUCTOR(XMLExtras) }
};

void PR_CALLBACK
XMLExtrasModuleDestructor(nsIModule* self)
{
  NS_IF_RELEASE(NS_CLASSINFO_NAME(XMLSerializer));
  NS_IF_RELEASE(NS_CLASSINFO_NAME(XMLHttpRequest));
  NS_IF_RELEASE(NS_CLASSINFO_NAME(DOMParser));
  NS_IF_RELEASE(NS_CLASSINFO_NAME(XPointerResult));
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsXMLExtrasModule, components, 
                              XMLExtrasModuleDestructor)

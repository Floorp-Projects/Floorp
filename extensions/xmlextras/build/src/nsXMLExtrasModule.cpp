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

#ifdef MOZ_SOAP
#include "nsSOAPParameter.h"
#include "nsSOAPCall.h"
#include "nsDefaultSOAPEncoder.h"
#include "nsHTTPSOAPTransport.h"
#endif
#include "nsString.h"
#include "prprf.h"
#include "nsIScriptNameSpaceManager.h"


////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMSerializer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXMLHttpRequest)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMParser)
#ifdef MOZ_SOAP
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPCall)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPParameter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDefaultSOAPEncoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTTPSOAPTransport)
#endif

class nsXMLExtrasNameset : public nsISupports
{
public:
  nsXMLExtrasNameset();
  virtual ~nsXMLExtrasNameset();

  // nsISupports
  NS_DECL_ISUPPORTS
};

nsXMLExtrasNameset::nsXMLExtrasNameset()
{
  NS_INIT_ISUPPORTS();
}

nsXMLExtrasNameset::~nsXMLExtrasNameset()
{
}

NS_INTERFACE_MAP_BEGIN(nsXMLExtrasNameset)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsXMLExtrasNameset)
NS_IMPL_RELEASE(nsXMLExtrasNameset)

#define NS_XML_EXTRAS_CID                          \
 { /* 33e569b0-40f8-11d4-9a41-000064657374 */      \
  0x33e569b0, 0x40f8, 0x11d4,                      \
 {0x9a, 0x41, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74} }

#define NS_XML_EXTRAS_CONTRACTID "@mozilla.org/xmlextras;1"

class nsXMLExtras : public nsISupports {
public:
  nsXMLExtras();
  virtual ~nsXMLExtras();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_XML_EXTRAS_CID);

  // nsISupports
  NS_DECL_ISUPPORTS
};

nsXMLExtras::nsXMLExtras()
{
  NS_INIT_ISUPPORTS();
}

nsXMLExtras::~nsXMLExtras()
{
}

NS_IMPL_ADDREF(nsXMLExtras)
NS_IMPL_RELEASE(nsXMLExtras)

NS_INTERFACE_MAP_BEGIN(nsXMLExtras)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


NS_GENERIC_FACTORY_CONSTRUCTOR(nsXMLExtras)

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
  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "XMLSerializer",
                                NS_XMLSERIALIZER_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "XMLHttpRequest",
                                NS_XMLHTTPREQUEST_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "DOMParser",
                                NS_DOMPARSER_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_SOAP
  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SOAPCall",
                                NS_SOAPCALL_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SOAPParameter",
                                NS_SOAPPARAMETER_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  return rv;
}

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
  { "XMLExtras component", NS_XML_EXTRAS_CID, NS_XML_EXTRAS_CONTRACTID,
    nsXMLExtrasConstructor, RegisterXMLExtras },
  { "XML Serializer", NS_XMLSERIALIZER_CID, NS_XMLSERIALIZER_CONTRACTID,
    nsDOMSerializerConstructor },
  { "XMLHttpRequest", NS_XMLHTTPREQUEST_CID, NS_XMLHTTPREQUEST_CONTRACTID,
    nsXMLHttpRequestConstructor },
  { "DOM Parser", NS_DOMPARSER_CID, NS_DOMPARSER_CONTRACTID,
    nsDOMParserConstructor },
#ifdef MOZ_SOAP
  { "SOAP Call", NS_SOAPCALL_CID, NS_SOAPCALL_CONTRACTID,
    nsSOAPCallConstructor },
  { "SOAP Parameter", NS_SOAPPARAMETER_CID, NS_SOAPPARAMETER_CONTRACTID,
    nsSOAPParameterConstructor },
  { "Default SOAP Encoder", NS_DEFAULTSOAPENCODER_CID,
    NS_DEFAULTSOAPENCODER_CONTRACTID, nsDefaultSOAPEncoderConstructor },
  { "HTTP SOAP Transport", NS_HTTPSOAPTRANSPORT_CID,
    NS_HTTPSOAPTRANSPORT_CONTRACTID, nsHTTPSOAPTransportConstructor },
#endif
};

NS_IMPL_NSGETMODULE(nsXMLExtrasModule, components)

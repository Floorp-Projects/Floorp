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
#include "nsDOMSerializer.h"
#include "nsXMLHttpRequest.h"
#include "nsDOMParser.h"
#include "nsIAppStartupNotifier.h"
#include "nsICategoryManager.h"
#include "nsIObserver.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIRegistry.h"
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

static NS_DEFINE_CID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

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

class nsXMLExtrasNameset : public nsIScriptExternalNameSet {
public:
  nsXMLExtrasNameset();
  virtual ~nsXMLExtrasNameset();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIScriptExternalNameSet
  NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
  NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

nsXMLExtrasNameset::nsXMLExtrasNameset()
{
  NS_INIT_ISUPPORTS();
}

nsXMLExtrasNameset::~nsXMLExtrasNameset()
{
}

NS_IMPL_ISUPPORTS1(nsXMLExtrasNameset, nsIScriptExternalNameSet)

NS_IMETHODIMP
nsXMLExtrasNameset::InitializeClasses(nsIScriptContext* aScriptContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLExtrasNameset::AddNameSet(nsIScriptContext* aScriptContext)
{
  static NS_DEFINE_CID(kXMLSerializer_CID, NS_XMLSERIALIZER_CID);
  static NS_DEFINE_CID(kXMLHttpRequest_CID, NS_XMLHTTPREQUEST_CID);
  static NS_DEFINE_CID(kDOMParser_CID, NS_DOMPARSER_CID);
#ifdef MOZ_SOAP
  static NS_DEFINE_CID(kSOAPCall_CID, NS_SOAPCALL_CID);
  static NS_DEFINE_CID(kSOAPParameter_CID, NS_SOAPPARAMETER_CID);
#endif
  nsresult result;
  nsCOMPtr<nsIScriptNameSpaceManager> manager;
  
  result = aScriptContext->GetNameSpaceManager(getter_AddRefs(manager));
  if (NS_SUCCEEDED(result)) {
    result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("XMLSerializer"), 
                                         NS_GET_IID(nsIDOMSerializer),
                                         kXMLSerializer_CID, 
                                         PR_TRUE);
    NS_ENSURE_SUCCESS(result, result);

    result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("XMLHttpRequest"), 
                                         NS_GET_IID(nsIXMLHttpRequest),
                                         kXMLHttpRequest_CID, 
                                         PR_TRUE);
    NS_ENSURE_SUCCESS(result, result);

    result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("DOMParser"), 
                                         NS_GET_IID(nsIDOMParser),
                                         kDOMParser_CID, 
                                         PR_TRUE);
    NS_ENSURE_SUCCESS(result, result);

#ifdef MOZ_SOAP
    result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("SOAPCall"), 
                                         NS_GET_IID(nsISOAPCall),
                                         kSOAPCall_CID, 
                                         PR_TRUE);
    NS_ENSURE_SUCCESS(result, result);

    result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("SOAPParameter"), 
                                         NS_GET_IID(nsISOAPParameter),
                                         kSOAPParameter_CID, 
                                         PR_TRUE);
    NS_ENSURE_SUCCESS(result, result);
#endif
  }

  return result;
}

#define NS_XML_EXTRAS_CID                          \
 { /* 33e569b0-40f8-11d4-9a41-000064657374 */      \
  0x33e569b0, 0x40f8, 0x11d4,                      \
 {0x9a, 0x41, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74} }

#define NS_XML_EXTRAS_CONTRACTID "@mozilla.org/xmlextras;1"

class nsXMLExtras : public nsIObserver {
public:
  nsXMLExtras();
  virtual ~nsXMLExtras();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_XML_EXTRAS_CID);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER
};

nsXMLExtras::nsXMLExtras()
{
  NS_INIT_ISUPPORTS();
}

nsXMLExtras::~nsXMLExtras()
{
}

NS_IMPL_ISUPPORTS1(nsXMLExtras, nsIObserver)

NS_IMETHODIMP
nsXMLExtras::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *aData) 
{
  nsresult rv;
  nsCOMPtr<nsIScriptNameSetRegistry> 
    namesetService(do_GetService(kCScriptNameSetRegistryCID, &rv));
  
  if (NS_SUCCEEDED(rv)) {
    nsXMLExtrasNameset* nameset = new nsXMLExtrasNameset();
    if (!nameset)
      return NS_ERROR_OUT_OF_MEMORY;
    // the NameSet service will AddRef this one
    rv = namesetService->AddExternalNameSet(nameset);
  }
  
  return rv;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsXMLExtras)

static NS_METHOD 
RegisterXMLExtras(nsIComponentManager *aCompMgr,
                  nsIFile *aPath,
                  const char *registryLocation,
                  const char *componentType,
                  const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService("@mozilla.org/categorymanager;1", &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = categoryManager->AddCategoryEntry(APPSTARTUP_CATEGORY, "XML Extras Module",
                        "service," NS_XML_EXTRAS_CONTRACTID,
                        PR_TRUE, PR_TRUE,
                        nsnull);
  }
  return rv;
}

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
  { "XMLExtras component", NS_XML_EXTRAS_CID, NS_XML_EXTRAS_CONTRACTID, nsXMLExtrasConstructor, RegisterXMLExtras },
  { "XML Serializer", NS_XMLSERIALIZER_CID, NS_XMLSERIALIZER_CONTRACTID, nsDOMSerializerConstructor },
  { "XMLHttpRequest", NS_XMLHTTPREQUEST_CID, NS_XMLHTTPREQUEST_CONTRACTID, nsXMLHttpRequestConstructor },
  { "DOM Parser", NS_DOMPARSER_CID, NS_DOMPARSER_CONTRACTID, nsDOMParserConstructor },
#ifdef MOZ_SOAP
  { "SOAP Call", NS_SOAPCALL_CID, NS_SOAPCALL_CONTRACTID, nsSOAPCallConstructor },
  { "SOAP Parameter", NS_SOAPPARAMETER_CID, NS_SOAPPARAMETER_CONTRACTID, nsSOAPParameterConstructor },
  { "Default SOAP Encoder", NS_DEFAULTSOAPENCODER_CID, NS_DEFAULTSOAPENCODER_CONTRACTID, nsDefaultSOAPEncoderConstructor },
  { "HTTP SOAP Transport", NS_HTTPSOAPTRANSPORT_CID, NS_HTTPSOAPTRANSPORT_CONTRACTID, nsHTTPSOAPTransportConstructor },
#endif  
};

NS_IMPL_NSGETMODULE("nsXMLExtrasModule", components)

#ifdef XP_WIN32
  //in addition to returning a version number for this module,
  //this also provides a convenient hook for the preloader
  //to keep (some if not all) of the module resident.
extern "C" __declspec(dllexport) float GetVersionNumber(void) {
  return 1.0;
}
#endif


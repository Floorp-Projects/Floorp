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
#include "nsSOAPParameter.h"
#include "nsSOAPCall.h"
#include "nsDefaultSOAPEncoder.h"
#include "nsHTTPSOAPTransport.h"
#include "nsIAppShellComponent.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIRegistry.h"
#include "nsString.h"
#include "nsDOMCID.h"
#include "prprf.h"

static NS_DEFINE_CID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMSerializer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXMLHttpRequest)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPCall)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPParameter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDefaultSOAPEncoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTTPSOAPTransport)

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
  static NS_DEFINE_CID(kSOAPCall_CID, NS_SOAPCALL_CID);
  static NS_DEFINE_CID(kSOAPParameter_CID, NS_SOAPPARAMETER_CID);
  nsresult result = NS_OK;
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
  }

  return result;
}

#define NS_XML_EXTRAS_CID                          \
 { /* 33e569b0-40f8-11d4-9a41-000064657374 */      \
  0x33e569b0, 0x40f8, 0x11d4,                      \
 {0x9a, 0x41, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74} }

#define NS_XML_EXTRAS_CONTRACTID NS_IAPPSHELLCOMPONENT_CONTRACTID "/xmlextras;1"

class nsXMLExtras : public nsIAppShellComponent {
public:
  nsXMLExtras();
  virtual ~nsXMLExtras();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_XML_EXTRAS_CID);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIAppShellComponent
  NS_DECL_NSIAPPSHELLCOMPONENT

  static nsXMLExtras *GetInstance();
  static nsXMLExtras*  mInstance;
};

nsXMLExtras* nsXMLExtras::mInstance = nsnull;

nsXMLExtras *
nsXMLExtras::GetInstance()
{
  if (mInstance == nsnull) {
    mInstance = new nsXMLExtras();
    // Will be released in the module destructor
    NS_IF_ADDREF(mInstance);
  }

  NS_IF_ADDREF(mInstance);
  return mInstance;
}

nsXMLExtras::nsXMLExtras()
{
  NS_INIT_ISUPPORTS();
}

nsXMLExtras::~nsXMLExtras()
{
}

NS_IMPL_ISUPPORTS1(nsXMLExtras, nsIAppShellComponent)

NS_IMETHODIMP
nsXMLExtras::Initialize(nsIAppShellService *anAppShell, 
                        nsICmdLineService  *aCmdLineService) 
{
  nsresult rv;
  nsCOMPtr<nsIScriptNameSetRegistry> namesetService = 
    do_GetService(kCScriptNameSetRegistryCID, &rv);
  
  if (NS_SUCCEEDED(rv)) {
    nsXMLExtrasNameset* nameset = new nsXMLExtrasNameset();
    // the NameSet service will AddRef this one
    namesetService->AddExternalNameSet(nameset);
  }
  
  return rv;

}

NS_IMETHODIMP
nsXMLExtras::Shutdown()
{
  return NS_OK;
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsXMLExtras,nsXMLExtras::GetInstance);

static NS_METHOD 
RegisterXMLExtras(nsIComponentManager *aCompMgr,
                  nsIFile *aPath,
                  const char *registryLocation,
                  const char *componentType)
{
  // get the registry
  nsIRegistry* registry;
  nsresult rv = nsServiceManager::GetService(NS_REGISTRY_CONTRACTID,
                                             NS_GET_IID(nsIRegistry),
                                             (nsISupports**)&registry);
  if (NS_SUCCEEDED(rv)) {
    registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    char buffer[256];
    char *cid = nsXMLExtras::GetCID().ToString();
    PR_snprintf(buffer,
                sizeof buffer,
                "%s/%s",
                NS_IAPPSHELLCOMPONENT_KEY,
                cid ? cid : "unknown" );
    nsCRT::free(cid);

    nsRegistryKey key;
    rv = registry->AddSubtree(nsIRegistry::Common,
                              buffer,
                              &key );
    nsServiceManager::ReleaseService(NS_REGISTRY_CONTRACTID, registry);
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
  { "SOAP Call", NS_SOAPCALL_CID, NS_SOAPCALL_CONTRACTID, nsSOAPCallConstructor },
  { "SOAP Parameter", NS_SOAPPARAMETER_CID, NS_SOAPPARAMETER_CONTRACTID, nsSOAPParameterConstructor },
  { "Default SOAP Encoder", NS_DEFAULTSOAPENCODER_CID, NS_DEFAULTSOAPENCODER_CONTRACTID, nsDefaultSOAPEncoderConstructor },
  { "HTTP SOAP Transport", NS_HTTPSOAPTRANSPORT_CID, NS_HTTPSOAPTRANSPORT_CONTRACTID, nsHTTPSOAPTransportConstructor },
};

static void PR_CALLBACK
XMLExtrasModuleDtor(nsIModule* self)
{
  NS_IF_RELEASE(nsXMLExtras::mInstance);
}

NS_IMPL_NSGETMODULE_WITH_DTOR("nsXMLExtrasModule", components, XMLExtrasModuleDtor)

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
 * The Original Code is the web services module code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

#include "nsICategoryManager.h"
#include "nsIScriptNameSpaceManager.h"

#include "nsSOAPHeaderBlock.h"
#include "nsSOAPParameter.h"
#include "nsSOAPCall.h"
#include "nsSOAPResponse.h"
#include "nsSOAPEncoding.h"
#include "nsSOAPFault.h"
#include "nsSOAPException.h"
#include "nsDefaultSOAPEncoder.h"
#include "nsHTTPSOAPTransport.h"
#include "nsSOAPPropertyBag.h"
#include "nsSchemaLoader.h"
#include "nsSchemaPrivate.h"

#include "nsWSDLLoader.h"
#include "nsWSDLPrivate.h"
#include "wspprivate.h"
#include "iixprivate.h"

#include "nsWebScriptsAccess.h"

////////////////////////////////////////////////////////////////////////
// Define the constructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGenericInterfaceInfoSet)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableInterfaceInfo)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebScriptsAccess)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPCall)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPResponse)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPEncoding)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPFault)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPHeaderBlock)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPParameter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDefaultSOAPEncoding_1_1)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDefaultSOAPEncoding_1_2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTTPSOAPTransport)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTTPSSOAPTransport)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSOAPPropertyBagMutator)
NS_DECL_CLASSINFO(nsSOAPCall)
NS_DECL_CLASSINFO(nsSOAPResponse)
NS_DECL_CLASSINFO(nsSOAPEncoding)
NS_DECL_CLASSINFO(nsSOAPFault)
NS_DECL_CLASSINFO(nsSOAPHeaderBlock)
NS_DECL_CLASSINFO(nsSOAPParameter)
NS_DECL_CLASSINFO(nsHTTPSOAPTransport)
NS_DECL_CLASSINFO(nsHTTPSOAPTransportCompletion)
NS_DECL_CLASSINFO(nsHTTPSSOAPTransport)
NS_DECL_CLASSINFO(nsSOAPPropertyBagMutator)
NS_DECL_CLASSINFO(nsSOAPProperty)
NS_DECL_CLASSINFO(nsSOAPPropertyBag)
NS_DECL_CLASSINFO(nsSOAPPropertyBagEnumerator)
NS_DECL_CLASSINFO(nsSOAPException)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSchemaLoader)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBuiltinSchemaCollection)
NS_DECL_CLASSINFO(nsSchemaLoader)
NS_DECL_CLASSINFO(nsSchema)
NS_DECL_CLASSINFO(nsSchemaBuiltinType)
NS_DECL_CLASSINFO(nsSchemaListType)
NS_DECL_CLASSINFO(nsSchemaUnionType)
NS_DECL_CLASSINFO(nsSchemaRestrictionType)
NS_DECL_CLASSINFO(nsSchemaComplexType)
NS_DECL_CLASSINFO(nsSchemaTypePlaceholder)
NS_DECL_CLASSINFO(nsSchemaModelGroup)
NS_DECL_CLASSINFO(nsSchemaModelGroupRef)
NS_DECL_CLASSINFO(nsSchemaAnyParticle)
NS_DECL_CLASSINFO(nsSchemaElement)
NS_DECL_CLASSINFO(nsSchemaElementRef)
NS_DECL_CLASSINFO(nsSchemaAttribute)
NS_DECL_CLASSINFO(nsSchemaAttributeRef)
NS_DECL_CLASSINFO(nsSchemaAttributeGroup)
NS_DECL_CLASSINFO(nsSchemaAttributeGroupRef)
NS_DECL_CLASSINFO(nsSchemaAnyAttribute)
NS_DECL_CLASSINFO(nsSchemaFacet)
NS_DECL_CLASSINFO(nsSOAPArray)
NS_DECL_CLASSINFO(nsSOAPArrayType)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWSDLLoader, Init)
NS_DECL_CLASSINFO(nsWSDLLoader)
NS_DECL_CLASSINFO(nsWSDLPort)
NS_DECL_CLASSINFO(nsWSDLOperation)
NS_DECL_CLASSINFO(nsWSDLMessage)
NS_DECL_CLASSINFO(nsWSDLPart)
NS_DECL_CLASSINFO(nsSOAPPortBinding)
NS_DECL_CLASSINFO(nsSOAPOperationBinding)
NS_DECL_CLASSINFO(nsSOAPMessageBinding)
NS_DECL_CLASSINFO(nsSOAPPartBinding)

NS_DECL_CLASSINFO(WSPComplexTypeWrapper)
NS_DECL_CLASSINFO(WSPCallContext)
NS_DECL_CLASSINFO(WSPException)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWSPInterfaceInfoService)
NS_GENERIC_FACTORY_CONSTRUCTOR(WSPFactory)
NS_DECL_CLASSINFO(WSPFactory)

NS_DECL_CLASSINFO(nsWebScriptsAccess)

// {79998DE9-1E34-45e0-A587-F9CCC8DB00DD}
#define NS_WSP_INTERFACE_INFO_SERVICE_CID             \
  { 0x79998de9, 0x1e34, 0x45e0,                       \
    { 0xa5, 0x87, 0xf9, 0xcc, 0xc8, 0xdb, 0x0, 0xdd } \
  }

// {B404670D-1EB1-4af8-86E5-FB3E9C4ECC4B}
#define NS_GENERIC_INTERFACE_INFO_SET_CID              \
  { 0xb404670d, 0x1eb1, 0x4af8,                        \
    { 0x86, 0xe5, 0xfb, 0x3e, 0x9c, 0x4e, 0xcc, 0x4b } \
  }

// {ED150A6A-4592-4e2c-82F8-70C8D65F74D2}
#define NS_SCRIPTABLE_INTERFACE_INFO_CID               \
  { 0xed150a6a, 0x4592, 0x4e2c,                        \
    { 0x82, 0xf8, 0x70, 0xc8, 0xd6, 0x5f, 0x74, 0xd2 } \
  }

// {0E1BDCED-4A4F-491b-906D-F4B908DBE854}
#define NS_WEB_SRVC_CID                                \
  { 0xe1bdced, 0x4a4f, 0x491b,                         \
    { 0x90, 0x6d, 0xf4, 0xb9, 0x8, 0xdb, 0xe8, 0x54 }  \
  }

static NS_METHOD 
RegisterWebService(nsIComponentManager *aCompMgr,
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
                                "SOAPCall",
                                NS_SOAPCALL_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SOAPResponse",
                                NS_SOAPRESPONSE_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SOAPEncoding",
                                NS_SOAPENCODING_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SOAPFault",
                                NS_SOAPFAULT_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SOAPHeaderBlock",
                                NS_SOAPHEADERBLOCK_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SOAPParameter",
                                NS_SOAPPARAMETER_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SOAPPropertyBagMutator",
                                NS_SOAPPROPERTYBAGMUTATOR_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "SchemaLoader",
                                NS_SCHEMALOADER_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "WSDLLoader",
                                NS_WSDLLOADER_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "WebServiceProxyFactory",
                                NS_WEBSERVICEPROXYFACTORY_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static const nsModuleComponentInfo gComponents[] = {
  { "SOAP Call", NS_SOAPCALL_CID, NS_SOAPCALL_CONTRACTID,
    nsSOAPCallConstructor, RegisterWebService, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPCall), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPCall), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Response", NS_SOAPRESPONSE_CID, NS_SOAPRESPONSE_CONTRACTID,
    nsSOAPResponseConstructor, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPResponse), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPResponse), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Encoding", NS_SOAPENCODING_CID, NS_SOAPENCODING_CONTRACTID,
    nsSOAPEncodingConstructor, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPEncoding), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPEncoding), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Fault", NS_SOAPFAULT_CID, NS_SOAPFAULT_CONTRACTID,
    nsSOAPFaultConstructor, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPFault), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPFault), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Header Block", NS_SOAPHEADERBLOCK_CID, NS_SOAPHEADERBLOCK_CONTRACTID,
    nsSOAPHeaderBlockConstructor, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPHeaderBlock), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPHeaderBlock), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Parameter", NS_SOAPPARAMETER_CID, NS_SOAPPARAMETER_CONTRACTID,
    nsSOAPParameterConstructor, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPParameter), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPParameter), 
    nsIClassInfo::DOM_OBJECT },
  { "Default SOAP 1.1 Encoding", NS_DEFAULTSOAPENCODING_1_1_CID,
    NS_DEFAULTSOAPENCODING_1_1_CONTRACTID, 
    nsDefaultSOAPEncoding_1_1Constructor },
  { "Default SOAP 1.2 Encoding", NS_DEFAULTSOAPENCODING_1_2_CID,
    NS_DEFAULTSOAPENCODING_1_2_CONTRACTID, 
    nsDefaultSOAPEncoding_1_2Constructor },
  { "HTTP SOAP Transport", NS_HTTPSOAPTRANSPORT_CID,
    NS_HTTPSOAPTRANSPORT_CONTRACTID, 
    nsHTTPSOAPTransportConstructor, nsnull, nsnull, nsnull,
    NS_CI_INTERFACE_GETTER_NAME(nsHTTPSOAPTransport), 
    nsnull, &NS_CLASSINFO_NAME(nsHTTPSOAPTransport), 
    nsIClassInfo::DOM_OBJECT },
  { "HTTP SOAP Transport Completion", NS_HTTPSOAPTRANSPORTCOMPLETION_CID,
    NS_HTTPSOAPTRANSPORTCOMPLETION_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsHTTPSOAPTransportCompletion), 
    nsnull, &NS_CLASSINFO_NAME(nsHTTPSOAPTransportCompletion), 
    nsIClassInfo::DOM_OBJECT },
  { "HTTPS SOAP Transport", NS_HTTPSSOAPTRANSPORT_CID,
    NS_HTTPSSOAPTRANSPORT_CONTRACTID, 
    nsHTTPSSOAPTransportConstructor, nsnull, nsnull, nsnull,
    NS_CI_INTERFACE_GETTER_NAME(nsHTTPSSOAPTransport), 
    nsnull, &NS_CLASSINFO_NAME(nsHTTPSSOAPTransport), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Property Bag Mutator", NS_SOAPPROPERTYBAGMUTATOR_CID,
    NS_SOAPPROPERTYBAGMUTATOR_CONTRACTID, 
    nsSOAPPropertyBagMutatorConstructor, nsnull, nsnull, nsnull,
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPPropertyBagMutator), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPPropertyBagMutator), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Property", NS_SOAPPROPERTY_CID,
    NS_SOAPPROPERTY_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPProperty), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPProperty), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Property Bag", NS_SOAPPROPERTYBAG_CID,
    NS_SOAPPROPERTYBAG_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPPropertyBag), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPPropertyBag), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Property Bag Enumerator", NS_SOAPPROPERTYBAGENUMERATOR_CID,
    NS_SOAPPROPERTYBAGENUMERATOR_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPPropertyBagEnumerator), 
    nsnull, &NS_CLASSINFO_NAME(nsSOAPPropertyBagEnumerator), 
    nsIClassInfo::DOM_OBJECT },
  { "SOAP Exception", NS_SOAPEXCEPTION_CLASSID, 
    NS_SOAPEXCEPTION_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPException), nsnull, 
    &NS_CLASSINFO_NAME(nsSOAPException), nsIClassInfo::DOM_OBJECT },
  { "SchemaLoader", NS_SCHEMALOADER_CID, NS_SCHEMALOADER_CONTRACTID,
    nsSchemaLoaderConstructor, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSchemaLoader), nsnull,
    &NS_CLASSINFO_NAME(nsSchemaLoader), nsIClassInfo::DOM_OBJECT },
  { "Schema", NS_SCHEMA_CID, NS_SCHEMA_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchema), nsnull, 
    &NS_CLASSINFO_NAME(nsSchema), nsIClassInfo::DOM_OBJECT },
  { "SchemaBuiltinType", NS_SCHEMABUILTINTYPE_CID, 
    NS_SCHEMABUILTINTYPE_CONTRACTID, nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSchemaBuiltinType), nsnull, 
    &NS_CLASSINFO_NAME(nsSchemaBuiltinType), nsIClassInfo::DOM_OBJECT },
  { "SchemaListType", NS_SCHEMALISTTYPE_CID, NS_SCHEMALISTTYPE_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSchemaListType), nsnull, 
    &NS_CLASSINFO_NAME(nsSchemaListType), nsIClassInfo::DOM_OBJECT },
  { "SchemaUnionType", NS_SCHEMAUNIONTYPE_CID, NS_SCHEMAUNIONTYPE_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSchemaUnionType), nsnull, 
    &NS_CLASSINFO_NAME(nsSchemaUnionType), nsIClassInfo::DOM_OBJECT },
  { "SchemaRestrictionType", NS_SCHEMARESTRICTIONTYPE_CID, 
    NS_SCHEMARESTRICTIONTYPE_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaRestrictionType), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaRestrictionType), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaComplexType", NS_SCHEMACOMPLEXTYPE_CID, 
    NS_SCHEMACOMPLEXTYPE_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaComplexType), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaComplexType), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaTypePlaceholder", NS_SCHEMATYPEPLACEHOLDER_CID, 
    NS_SCHEMATYPEPLACEHOLDER_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaTypePlaceholder), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaTypePlaceholder), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaModelGroup", NS_SCHEMAMODELGROUP_CID, 
    NS_SCHEMAMODELGROUP_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaModelGroup), nsnull, 
    &NS_CLASSINFO_NAME(nsSchemaModelGroup), nsIClassInfo::DOM_OBJECT },
  { "SchemaModelGroupRef", NS_SCHEMAMODELGROUPREF_CID, 
    NS_SCHEMAMODELGROUPREF_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaModelGroupRef), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaModelGroupRef), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaAnyParticle", NS_SCHEMAANYPARTICLE_CID, 
    NS_SCHEMAANYPARTICLE_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaAnyParticle), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaAnyParticle), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaElement", NS_SCHEMAELEMENT_CID, NS_SCHEMAELEMENT_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSchemaElement), nsnull, 
    &NS_CLASSINFO_NAME(nsSchemaElement), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaElementRef", NS_SCHEMAELEMENTREF_CID, 
    NS_SCHEMAELEMENTREF_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaElementRef), nsnull, 
    &NS_CLASSINFO_NAME(nsSchemaElementRef), nsIClassInfo::DOM_OBJECT },
  { "SchemaAttribute", NS_SCHEMAATTRIBUTE_CID, NS_SCHEMAATTRIBUTE_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSchemaAttribute), nsnull, 
    &NS_CLASSINFO_NAME(nsSchemaAttribute), nsIClassInfo::DOM_OBJECT },
  { "SchemaAttributeRef", NS_SCHEMAATTRIBUTEREF_CID, 
    NS_SCHEMAATTRIBUTEREF_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaAttributeRef), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaAttributeRef), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaAttributeGroup", NS_SCHEMAATTRIBUTEGROUP_CID, 
    NS_SCHEMAATTRIBUTEGROUP_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaAttributeGroup), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaAttributeGroup), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaAttributeGroupRef", NS_SCHEMAATTRIBUTEGROUPREF_CID, 
    NS_SCHEMAATTRIBUTEGROUPREF_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaAttributeGroupRef), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaAttributeGroupRef), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaAnyAttribute", NS_SCHEMAANYATTRIBUTE_CID, 
    NS_SCHEMAANYATTRIBUTE_CONTRACTID, nsnull, nsnull, 
    nsnull, nsnull, NS_CI_INTERFACE_GETTER_NAME(nsSchemaAnyAttribute), 
    nsnull, &NS_CLASSINFO_NAME(nsSchemaAnyAttribute), 
    nsIClassInfo::DOM_OBJECT },
  { "SchemaFacet", NS_SCHEMAFACET_CID, NS_SCHEMAFACET_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSchemaFacet), nsnull, 
    &NS_CLASSINFO_NAME(nsSchemaFacet), nsIClassInfo::DOM_OBJECT },
  { "SOAPArray", NS_SOAPARRAY_CID, NS_SOAPARRAY_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPArray), nsnull, 
    &NS_CLASSINFO_NAME(nsSOAPArray), nsIClassInfo::DOM_OBJECT },
  { "SOAPArrayType", NS_SOAPARRAYTYPE_CID, NS_SOAPARRAYTYPE_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPArrayType), nsnull, 
    &NS_CLASSINFO_NAME(nsSOAPArrayType), nsIClassInfo::DOM_OBJECT },
  { "Builtin Schema Collection", NS_BUILTINSCHEMACOLLECTION_CID,
    NS_BUILTINSCHEMACOLLECTION_CONTRACTID, 
    nsBuiltinSchemaCollectionConstructor },
  { "WSDLLoader", NS_WSDLLOADER_CID, NS_WSDLLOADER_CONTRACTID,
    nsWSDLLoaderConstructor, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsWSDLLoader), nsnull,
    &NS_CLASSINFO_NAME(nsWSDLLoader), nsIClassInfo::DOM_OBJECT },
  { "WSDLPort", NS_WSDLPORT_CID, NS_WSDLPORT_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsWSDLPort), nsnull, 
    &NS_CLASSINFO_NAME(nsWSDLPort), nsIClassInfo::DOM_OBJECT },
  { "WSDLOperation", NS_WSDLOPERATION_CID, NS_WSDLOPERATION_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsWSDLOperation), nsnull, 
    &NS_CLASSINFO_NAME(nsWSDLOperation), nsIClassInfo::DOM_OBJECT },
  { "WSDLMessage", NS_WSDLMESSAGE_CID, NS_WSDLMESSAGE_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsWSDLMessage), nsnull, 
    &NS_CLASSINFO_NAME(nsWSDLMessage), nsIClassInfo::DOM_OBJECT },
  { "WSDLPart", NS_WSDLPART_CID, NS_WSDLPART_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsWSDLPart), nsnull, 
    &NS_CLASSINFO_NAME(nsWSDLPart), nsIClassInfo::DOM_OBJECT },
  { "SOAPPortBinding", NS_SOAPPORTBINDING_CID, NS_SOAPPORTBINDING_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPPortBinding), nsnull, 
    &NS_CLASSINFO_NAME(nsSOAPPortBinding), nsIClassInfo::DOM_OBJECT },
  { "SOAPOperationBinding", NS_SOAPOPERATIONBINDING_CID, 
    NS_SOAPOPERATIONBINDING_CONTRACTID, nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPOperationBinding), nsnull, 
    &NS_CLASSINFO_NAME(nsSOAPOperationBinding), nsIClassInfo::DOM_OBJECT },
  { "SOAPMessageBinding", NS_SOAPMESSAGEBINDING_CID, 
    NS_SOAPMESSAGEBINDING_CONTRACTID, nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPMessageBinding), nsnull, 
    &NS_CLASSINFO_NAME(nsSOAPMessageBinding), nsIClassInfo::DOM_OBJECT },
  { "SOAPPartBinding", NS_SOAPPARTBINDING_CID, NS_SOAPPARTBINDING_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(nsSOAPPartBinding), nsnull, 
    &NS_CLASSINFO_NAME(nsSOAPPartBinding), nsIClassInfo::DOM_OBJECT },
  { "WebServiceProxy", NS_WEBSERVICEPROXY_CLASSID, 
    NS_WEBSERVICEPROXY_CONTRACTID, WSPProxy::Create },
  { "WebServiceComplexTypeWrapper", NS_WEBSERVICECOMPLEXTYPEWRAPPER_CLASSID,
    NS_WEBSERVICECOMPLEXTYPEWRAPPER_CONTRACTID, 
    WSPComplexTypeWrapper::Create, nsnull, nsnull, nsnull,
    NS_CI_INTERFACE_GETTER_NAME(WSPComplexTypeWrapper), nsnull, 
    &NS_CLASSINFO_NAME(WSPComplexTypeWrapper), nsIClassInfo::DOM_OBJECT },
  { "WebServicePropertyBagWrapper", NS_WEBSERVICEPROPERTYBAGWRAPPER_CLASSID, 
    NS_WEBSERVICEPROPERTYBAGWRAPPER_CONTRACTID, 
    WSPPropertyBagWrapper::Create }, 
  { "WebServiceCallContext", NS_WEBSERVICECALLCONTEXT_CLASSID, 
    NS_WEBSERVICECALLCONTEXT_CONTRACTID, nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(WSPCallContext), nsnull, 
    &NS_CLASSINFO_NAME(WSPCallContext), nsIClassInfo::DOM_OBJECT },
  { "WebServiceException", NS_WEBSERVICEEXCEPTION_CLASSID, 
    NS_WEBSERVICEEXCEPTION_CONTRACTID, 
    nsnull, nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(WSPException), nsnull, 
    &NS_CLASSINFO_NAME(WSPException), nsIClassInfo::DOM_OBJECT },
  { "nsWSPInterfaceInfoService", NS_WSP_INTERFACE_INFO_SERVICE_CID, 
    NS_WSP_INTERFACEINFOSERVICE_CONTRACTID,
    nsWSPInterfaceInfoServiceConstructor},
  { "WSPFactory", NS_WEBSERVICEPROXYFACTORY_CLASSID, 
    NS_WEBSERVICEPROXYFACTORY_CONTRACTID, WSPFactoryConstructor, 
    nsnull, nsnull, nsnull, 
    NS_CI_INTERFACE_GETTER_NAME(WSPFactory), nsnull, 
    &NS_CLASSINFO_NAME(WSPFactory), nsIClassInfo::DOM_OBJECT },
  { "InterfaceInfo", 
    NS_GENERIC_INTERFACE_INFO_SET_CID, 
    NS_GENERIC_INTERFACE_INFO_SET_CONTRACTID, 
    nsGenericInterfaceInfoSetConstructor
  },
  { "ScriptableInterfaceInfo", 
    NS_SCRIPTABLE_INTERFACE_INFO_CID, 
    NS_SCRIPTABLE_INTERFACE_INFO_CONTRACTID, 
    nsScriptableInterfaceInfoConstructor
  },
  { "WebScriptsAccess",
    NS_WEBSCRIPTSACCESSSERVICE_CID,
    NS_WEBSCRIPTSACCESSSERVICE_CONTRACTID,
    nsWebScriptsAccessConstructor
  }
};

nsSOAPStrings *gSOAPStrings;

PR_STATIC_CALLBACK(nsresult)
nsWebServicesModuleConstructor(nsIModule* self)
{
  NS_ASSERTION(gSOAPStrings == nsnull, "already initialized");
  gSOAPStrings = new nsSOAPStrings();
  if (!gSOAPStrings)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

PR_STATIC_CALLBACK(void)
nsWebServicesModuleDestructor(nsIModule* self)
{
  nsSchemaAtoms::DestroySchemaAtoms();
  nsWSDLAtoms::DestroyWSDLAtoms();

  delete gSOAPStrings;
  gSOAPStrings = nsnull;
}

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsWebServicesModule, gComponents, 
                                   nsWebServicesModuleConstructor,
                                   nsWebServicesModuleDestructor)


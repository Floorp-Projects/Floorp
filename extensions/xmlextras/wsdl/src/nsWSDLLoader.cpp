/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#include "nsWSDLLoader.h"

// loading includes
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsNetUtil.h"

// content includes
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocument.h"

// string includes
#include "nsReadableUtils.h"

// XPConnect includes
#include "nsIXPConnect.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"

// XPCOM includes
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

////////////////////////////////////////////////////////////
//
// nsWSDLAtoms implementation
//
////////////////////////////////////////////////////////////

// Statics
nsIAtom* nsWSDLAtoms::sDefinitions_atom = nsnull;
nsIAtom* nsWSDLAtoms::sImport_atom = nsnull;
nsIAtom* nsWSDLAtoms::sTypes_atom = nsnull;
nsIAtom* nsWSDLAtoms::sMessage_atom = nsnull;
nsIAtom* nsWSDLAtoms::sPortType_atom = nsnull;
nsIAtom* nsWSDLAtoms::sBinding_atom = nsnull;
nsIAtom* nsWSDLAtoms::sService_atom = nsnull;
nsIAtom* nsWSDLAtoms::sPort_atom = nsnull;
nsIAtom* nsWSDLAtoms::sOperation_atom = nsnull;
nsIAtom* nsWSDLAtoms::sPart_atom = nsnull;
nsIAtom* nsWSDLAtoms::sDocumentation_atom = nsnull;
nsIAtom* nsWSDLAtoms::sInput_atom = nsnull;
nsIAtom* nsWSDLAtoms::sOutput_atom = nsnull;
nsIAtom* nsWSDLAtoms::sFault_atom = nsnull;

nsIAtom* nsWSDLAtoms::sBody_atom = nsnull;
nsIAtom* nsWSDLAtoms::sHeader_atom = nsnull;
nsIAtom* nsWSDLAtoms::sHeaderFault_atom = nsnull;
nsIAtom* nsWSDLAtoms::sAddress_atom = nsnull;

void
nsWSDLAtoms::CreateWSDLAtoms()
{
  sDefinitions_atom = NS_NewAtom("definitions");
  sImport_atom = NS_NewAtom("import");
  sTypes_atom = NS_NewAtom("types");
  sMessage_atom = NS_NewAtom("message");
  sPortType_atom = NS_NewAtom("portType");
  sBinding_atom = NS_NewAtom("binding");
  sService_atom = NS_NewAtom("service");
  sPort_atom = NS_NewAtom("port");
  sOperation_atom = NS_NewAtom("operation");
  sPart_atom = NS_NewAtom("part");
  sDocumentation_atom = NS_NewAtom("documentation");
  sInput_atom = NS_NewAtom("input");
  sOutput_atom = NS_NewAtom("output");
  sFault_atom = NS_NewAtom("fault");

  sBody_atom = NS_NewAtom("body");
  sHeader_atom = NS_NewAtom("header");
  sHeaderFault_atom = NS_NewAtom("headerFault");
  sAddress_atom = NS_NewAtom("address");
}

void
nsWSDLAtoms::DestroyWSDLAtoms()
{
  if (sDefinitions_atom) {
    NS_RELEASE(sDefinitions_atom);
    NS_RELEASE(sImport_atom);
    NS_RELEASE(sTypes_atom);
    NS_RELEASE(sMessage_atom);
    NS_RELEASE(sPortType_atom);
    NS_RELEASE(sBinding_atom);
    NS_RELEASE(sService_atom);
    NS_RELEASE(sPort_atom);
    NS_RELEASE(sOperation_atom);
    NS_RELEASE(sPart_atom);
    NS_RELEASE(sDocumentation_atom);
    NS_RELEASE(sInput_atom);
    NS_RELEASE(sOutput_atom);
    NS_RELEASE(sFault_atom);

    NS_RELEASE(sBody_atom);
    NS_RELEASE(sHeader_atom);
    NS_RELEASE(sHeaderFault_atom);
    NS_RELEASE(sAddress_atom);
  }
}

////////////////////////////////////////////////////////////
//
// nsWSDLLoader implementation
//
////////////////////////////////////////////////////////////

nsWSDLLoader::nsWSDLLoader()
{
  NS_INIT_ISUPPORTS();
  if (!nsWSDLAtoms::sDefinitions_atom) {
    nsWSDLAtoms::CreateWSDLAtoms();
  }
}

nsWSDLLoader::~nsWSDLLoader()
{
}

NS_IMPL_ISUPPORTS1_CI(nsWSDLLoader, nsIWSDLLoader)

nsresult
nsWSDLLoader::GetResolvedURI(const nsAReadableString& aWSDLURI,
                             const char* aMethod,
                             nsIURI** aURI)
{
  nsresult rv;
  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIScriptSecurityManager> secMan(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIPrincipal> principal;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
      if (codebase) {
        codebase->GetURI(getter_AddRefs(baseURI));
      }
    }
    
    rv = NS_NewURI(aURI, aWSDLURI, baseURI);
    if (NS_FAILED(rv)) return rv;
    
    rv = secMan->CheckConnect(cx, *aURI, "nsWSDLLoader", aMethod);
    if (NS_FAILED(rv))
    {
      // Security check failed. The above call set a JS exception. The
      // following lines ensure that the exception is propagated.
      cc->SetExceptionWasThrown(PR_TRUE);
      return rv;
    }
  }
  else {
    rv = NS_NewURI(aURI, aWSDLURI, nsnull);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

/* nsIWSDLService load (in AString wsdlURI, in AString portName); */
NS_IMETHODIMP 
nsWSDLLoader::Load(const nsAReadableString& wsdlURI,
                   const nsAReadableString& portName,
                   nsIWSDLPort **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsCOMPtr<nsIURI> resolvedURI;
  nsresult rv = GetResolvedURI(wsdlURI, "load", getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsXPIDLCString spec;
  resolvedURI->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsIDOMEventListener> listener;
  nsWSDLLoadRequest* request = new nsWSDLLoadRequest(PR_TRUE, nsnull,
                                                     portName);
  if (!request) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  listener = request;

  rv = request->LoadDefinition(NS_ConvertASCIItoUCS2(spec.get()));

  if (NS_SUCCEEDED(rv)) {
    request->GetPort(_retval);
  }

  return rv;
}

/* void loadAsync (in AString wsdlURI, in AString portName, 
   in nsIWSDLLoadListener listener); */
NS_IMETHODIMP 
nsWSDLLoader::LoadAsync(const nsAReadableString& wsdlURI, 
                        const nsAReadableString& portName,
                        nsIWSDLLoadListener *aListener)
{
  NS_ENSURE_ARG(aListener);

  nsCOMPtr<nsIURI> resolvedURI;
  nsresult rv = GetResolvedURI(wsdlURI, "loadAsync", getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsXPIDLCString spec;
  resolvedURI->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsIDOMEventListener> listener;
  nsWSDLLoadRequest* request = new nsWSDLLoadRequest(PR_FALSE, aListener,
                                                     portName);
  if (!request) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  listener = request;

  return request->LoadDefinition(NS_ConvertASCIItoUCS2(spec.get()));
}

/*  nsISupports createPortProxy(in nsIWSDLPort service, in AString nameSpace); */
NS_IMETHODIMP 
nsWSDLLoader::CreatePortProxy(nsIWSDLPort *port, 
                              const nsAReadableString& nameSpace,
                              PRBool sync,
                              nsISupports **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////
//
// nsWSDLLoadRequest implementation
//
////////////////////////////////////////////////////////////
nsWSDLLoadRequest::nsWSDLLoadRequest(PRBool aIsSync,
                                     nsIWSDLLoadListener* aListener,
                                     const nsAReadableString& aPortName)
  : mListener(aListener), mIsSync(aIsSync), mPortName(aPortName)
{
  NS_INIT_ISUPPORTS();
}

nsWSDLLoadRequest::~nsWSDLLoadRequest()
{
  while (GetCurrentContext() != nsnull) {
    PopContext();
  }
}

NS_IMPL_ISUPPORTS1(nsWSDLLoadRequest, 
                   nsIDOMEventListener)

static inline PRBool
IsElementOfNamespace(nsIDOMElement* aElement,
                     const nsAReadableString& aNamespace)
{
	nsAutoString namespaceURI;           
	aElement->GetNamespaceURI(namespaceURI);  
	return namespaceURI.Equals(aNamespace);
}

nsresult 
nsWSDLLoadRequest::LoadDefinition(const nsAReadableString& aURI)
{
  nsresult rv;
  
  if (!mSchemaLoader) {
    mSchemaLoader = do_GetService(NS_SCHEMALOADER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mRequest = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (!mRequest) {
    return rv;
  }

  rv = mRequest->OpenRequest("GET",
                             NS_ConvertUCS2toUTF8(aURI).get(),
                             !mIsSync, nsnull, nsnull);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mIsSync) {
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mRequest));
    if (!target) {
      return NS_ERROR_UNEXPECTED;
    }
    
    rv = target->AddEventListener(NS_LITERAL_STRING("load"),
                                  this, PR_FALSE);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    rv = target->AddEventListener(NS_LITERAL_STRING("error"),
                                  this, PR_FALSE);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  rv = mRequest->Send(nsnull);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  if (mIsSync) {
    nsCOMPtr<nsIDOMDocument> document;
    rv = mRequest->GetResponseXML(getter_AddRefs(document));
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    nsCOMPtr<nsIDOMElement> element;
    document->GetDocumentElement(getter_AddRefs(element));
    if (element) {
      if (IsElementOfNamespace(element, 
                               NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
        rv = PushContext(document, aURI);
        if (NS_FAILED(rv)) {
          return rv;
        }
        
        rv = ResumeProcessing();
        
        PopContext();
        
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
      else if (IsElementOfNamespace(element,
                                    NS_LITERAL_STRING(NS_WSDL_SCHEMA_NAMESPACE))) {
        nsCOMPtr<nsISchema> schema;
        rv = mSchemaLoader->ProcessSchemaElement(element, 
                                                 getter_AddRefs(schema));
        if (NS_FAILED(rv)) {
          return NS_ERROR_WSDL_SCHEMA_PROCESSING_ERROR;
        }
        
        nsAutoString targetNamespace;
        schema->GetTargetNamespace(targetNamespace);
        
        nsStringKey key(targetNamespace);
        mTypes.Put(&key, schema);
      }
      else {
        rv = NS_ERROR_WSDL_NOT_WSDL_ELEMENT; // element of unknown namespace
      }
    }
    else {
      return NS_ERROR_WSDL_NOT_WSDL_ELEMENT;
    }
  }

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ContineProcessingTillDone()
{
  nsresult rv;
  do {
    rv = ResumeProcessing();
    
    if (NS_FAILED(rv) || (rv == NS_ERROR_WSDL_LOADPENDING)) {
      break;
    }
              
    PopContext();
    
  } while (GetCurrentContext() != nsnull);

  return rv;
} 

/* void handleEvent (in nsIDOMEvent event); */
NS_IMETHODIMP 
nsWSDLLoadRequest::HandleEvent(nsIDOMEvent *event)
{
  nsresult rv;
  nsAutoString eventType;
  
  event->GetType(eventType);
  
  if (eventType.Equals(NS_LITERAL_STRING("load"))) {
    nsCOMPtr<nsIDOMDocument> document;
    
    rv = mRequest->GetResponseXML(getter_AddRefs(document));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIDOMElement> element;
      
      document->GetDocumentElement(getter_AddRefs(element));
      if (element) {
        if (IsElementOfNamespace(element, 
                                 NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {

          nsCOMPtr<nsIChannel> channel;
          nsCOMPtr<nsIURI> uri;
          nsXPIDLCString spec;
          
          mRequest->GetChannel(getter_AddRefs(channel));
          
          if (channel) {
            channel->GetURI(getter_AddRefs(uri));
            if (uri) {
              uri->GetSpec(getter_Copies(spec));
            }
          }
          
          rv = PushContext(document, NS_ConvertASCIItoUCS2(spec.get()));
          
          if (NS_SUCCEEDED(rv)) {
            rv = ContineProcessingTillDone();
          }
        }
        else if (IsElementOfNamespace(element,
                                      NS_LITERAL_STRING(NS_WSDL_SCHEMA_NAMESPACE))) {
          nsCOMPtr<nsISchema> schema;
          rv = mSchemaLoader->ProcessSchemaElement(element, 
                                                   getter_AddRefs(schema));
          if (NS_FAILED(rv)) {
            return NS_ERROR_WSDL_SCHEMA_PROCESSING_ERROR;
          }
          
          nsAutoString targetNamespace;
          schema->GetTargetNamespace(targetNamespace);
          
          nsStringKey key(targetNamespace);
          mTypes.Put(&key, schema);

          rv = ContineProcessingTillDone();
        }
        else {
          rv = NS_ERROR_WSDL_NOT_WSDL_ELEMENT; // element of unknown namespace
        }
      }
      else {
        rv = NS_ERROR_WSDL_NOT_WSDL_ELEMENT;
      }
    }

    if (NS_FAILED(rv)) {
      mListener->OnError(rv, NS_LITERAL_STRING("Failure processing WSDL document"));
      return NS_OK;
    }
  }
  else if (eventType.Equals(NS_LITERAL_STRING("error"))) {
    mListener->OnError(NS_ERROR_WSDL_LOADING_ERROR, 
                       NS_LITERAL_STRING("Failure loading"));
    return NS_OK;
  }

  if (GetCurrentContext() == nsnull) {
    if (mPort) {
      mListener->OnLoad(mPort);
    }
    else {
      mListener->OnError(NS_ERROR_WSDL_BINDING_NOT_FOUND, 
                         NS_LITERAL_STRING("Binding not found"));
    }
    mRequest = nsnull;
  }

  return NS_OK;  
}

nsresult
nsWSDLLoadRequest::ResumeProcessing()
{
  nsresult rv = NS_OK;

  nsWSDLLoadingContext* context = GetCurrentContext();
  if (!context) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDOMElement> element;
  context->GetRootElement(getter_AddRefs(element));
  PRUint32 childIndex = context->GetChildIndex();

  nsChildElementIterator iterator(element, 
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE));
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  // If we don't yet have a port, find the service element, create one
  // and record the port's its binding name
  if (mBindingName.IsEmpty()) {
    while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                              getter_AddRefs(tagName))) &&
           childElement) {
      if (tagName == nsWSDLAtoms::sService_atom) {
        rv = ProcessServiceElement(childElement);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
  }  

  iterator.Reset(childIndex);
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsWSDLAtoms::sImport_atom) {
      rv = ProcessImportElement(childElement, 
                                iterator.GetCurrentIndex()+1);
      if (NS_FAILED(rv) || 
          (rv == NS_ERROR_WSDL_LOADPENDING)) {
        return rv;
      }
    }
    else if (tagName == nsWSDLAtoms::sTypes_atom) {
      rv = ProcessTypesElement(childElement);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else if (tagName == nsWSDLAtoms::sMessage_atom) {
      rv = ProcessMessageElement(childElement);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else if (tagName == nsWSDLAtoms::sPortType_atom) {
      rv = ProcessPortTypeElement(childElement);     
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else if (tagName == nsWSDLAtoms::sBinding_atom) {
      nsAutoString name, targetNamespace;
      childElement->GetAttribute(NS_LITERAL_STRING("name"), name);
      context->GetTargetNamespace(targetNamespace);

      // Only process binding for the requested port
      // XXX We should be checking the binding namespace as
      // well, but some authors are not qualifying their
      // binding names
      if (mBindingName.Equals(name)) {
        rv = ProcessBindingElement(childElement);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
  }    

  return rv;
}

nsresult
nsWSDLLoadRequest::GetPort(nsIWSDLPort** aPort)
{
  *aPort = mPort;
  NS_IF_ADDREF(*aPort);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::PushContext(nsIDOMDocument* aDocument, 
                               const nsAReadableString& aURISpec)
{
  nsWSDLLoadingContext* context = new nsWSDLLoadingContext(aDocument,
                                                           aURISpec);
  if (!context) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mContextStack.AppendElement((void*)context);
  
  return NS_OK;
}

nsWSDLLoadingContext* 
nsWSDLLoadRequest::GetCurrentContext()
{
  PRUint32 count = mContextStack.Count();
  if (count > 0) {
    return NS_STATIC_CAST(nsWSDLLoadingContext*,
                          mContextStack.ElementAt(count-1));
  }

  return nsnull;
}

void 
nsWSDLLoadRequest::PopContext()
{
  PRUint32 count = mContextStack.Count();
  if (count > 0) {
    nsWSDLLoadingContext* context = NS_STATIC_CAST(nsWSDLLoadingContext*,
                                               mContextStack.ElementAt(count-1));
    delete context;
    mContextStack.RemoveElementAt(count-1);
  }
}

nsresult
nsWSDLLoadRequest::GetSchemaElement(const nsAReadableString& aName,
                                    const nsAReadableString& aNamespace,
                                    nsISchemaElement** aSchemaComponent)
{
  nsStringKey key(aNamespace);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mTypes.Get(&key));
  nsCOMPtr<nsISchema> schema(do_QueryInterface(sup));
  if (!schema) {
    return NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT;
  }

  nsCOMPtr<nsISchemaElement> element;
  schema->GetElementByName(aName, getter_AddRefs(element));
  if (!element) {
    return NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT;
  }

  *aSchemaComponent = element;
  NS_IF_ADDREF(*aSchemaComponent);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::GetSchemaType(const nsAReadableString& aName,
                                 const nsAReadableString& aNamespace,
                                 nsISchemaType** aSchemaComponent)
{
  if (aNamespace.Equals(NS_LITERAL_STRING(NS_WSDL_SCHEMA_NAMESPACE))) {
    nsCOMPtr<nsISchemaCollection> collection(do_QueryInterface(mSchemaLoader));
    return collection->GetType(aName, aNamespace, aSchemaComponent);
  }

  nsStringKey key(aNamespace);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mTypes.Get(&key));
  nsCOMPtr<nsISchema> schema(do_QueryInterface(sup));
  if (!schema) {
    return NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT;
  }
  
  nsCOMPtr<nsISchemaType> type;
  schema->GetTypeByName(aName, getter_AddRefs(type));
  if (!type) {
    return NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT;
  }
  
  *aSchemaComponent = type;
  NS_IF_ADDREF(*aSchemaComponent);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::GetMessage(const nsAReadableString& aName,
                              const nsAReadableString& aNamespace,
                              nsIWSDLMessage** aMessage)
{
  nsAutoString keyStr;
  keyStr.Assign(aName);
  keyStr.Append(aNamespace);
  nsStringKey key(keyStr);

  nsCOMPtr<nsISupports> sup = dont_AddRef(mMessages.Get(&key));
  nsCOMPtr<nsIWSDLMessage> message(do_QueryInterface(sup));
  if (!message) {
    return NS_ERROR_WSDL_UNKNOWN_WSDL_COMPONENT;
  }

  *aMessage = message;
  NS_IF_ADDREF(*aMessage);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::GetPortType(const nsAReadableString& aName,
                               const nsAReadableString& aNamespace,
                               nsIWSDLPort** aPort)
{
  nsAutoString keyStr;
  keyStr.Assign(aName);
  keyStr.Append(aNamespace);
  nsStringKey key(keyStr);

  nsCOMPtr<nsISupports> sup = dont_AddRef(mPortTypes.Get(&key));
  nsCOMPtr<nsIWSDLPort> port(do_QueryInterface(sup));
  if (!port) {
    return NS_ERROR_WSDL_UNKNOWN_WSDL_COMPONENT;
  }

  *aPort = port;
  NS_IF_ADDREF(*aPort);

  return NS_OK;
}

static nsresult
ParseQualifiedName(nsIDOMElement* aContext,
                   const nsAReadableString& aQualifiedName,
                   nsAWritableString& aPrefix,
                   nsAWritableString& aLocalName,
                   nsAWritableString& aNamespaceURI) 
{
  nsReadingIterator<PRUnichar> pos, begin, end;
  
  aQualifiedName.BeginReading(begin);
  aQualifiedName.EndReading(end); 
  pos = begin;
  
  if (FindCharInReadable(PRUnichar(':'), pos, end)) {
    CopyUnicodeTo(begin, pos, aPrefix);
    CopyUnicodeTo(++pos, end, aLocalName);
  }
  else {
    CopyUnicodeTo(begin, end, aLocalName);
  }
  
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(aContext));
  if (!node) {
    return NS_ERROR_UNEXPECTED;
  }

  return node->LookupNamespaceURI(aPrefix, aNamespaceURI);
}


nsresult
nsWSDLLoadRequest::ProcessImportElement(nsIDOMElement* aElement, 
                                        PRUint32 aIndex)
{
  nsresult rv = NS_OK;

  // XXX Is there a need to record the namespace? Can it be different
  // from the targetNamespace of the imported file?

  nsAutoString location, documentLocation;
  aElement->GetAttribute(NS_LITERAL_STRING("location"), location);

  nsWSDLLoadingContext* context = GetCurrentContext();
  if (!context) {
    return NS_ERROR_UNEXPECTED;
  }
  context->GetDocumentLocation(documentLocation);

  nsCOMPtr<nsIURI> uri, baseURI;
  rv = NS_NewURI(getter_AddRefs(baseURI), documentLocation);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_NewURI(getter_AddRefs(uri), location, baseURI);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsXPIDLCString spec;
  uri->GetSpec(getter_Copies(spec));

  rv = LoadDefinition(NS_ConvertUTF8toUCS2(spec.get()));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mIsSync) {
    context->SetChildIndex(aIndex);
    return NS_ERROR_WSDL_LOADPENDING;
  }

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessTypesElement(nsIDOMElement* aElement)
{
  nsresult rv = NS_OK;

  nsChildElementIterator iterator(aElement, 
                                  NS_LITERAL_STRING(NS_WSDL_SCHEMA_NAMESPACE));
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  nsCOMPtr<nsIAtom> schemaAtom(dont_AddRef(NS_NewAtom("schema")));

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == schemaAtom) {
      nsCOMPtr<nsISchema> schema;
      rv = mSchemaLoader->ProcessSchemaElement(childElement, 
                                               getter_AddRefs(schema));
      if (NS_FAILED(rv)) {
        return NS_ERROR_WSDL_SCHEMA_PROCESSING_ERROR;
      }

      nsAutoString targetNamespace;
      schema->GetTargetNamespace(targetNamespace);
      
      nsStringKey key(targetNamespace);
      mTypes.Put(&key, schema);

      break;
    }
  }

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessAbstractPartElement(nsIDOMElement* aElement,
                                              nsWSDLMessage* aMessage)
{
  nsresult rv = NS_OK;

  nsAutoString name;
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);

  nsCOMPtr<nsIWSDLPart> part;
  nsWSDLPart* partInst = new nsWSDLPart(name);
  if (!partInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  part = partInst;

  nsCOMPtr<nsISchemaComponent> schemaComponent;
  nsAutoString elementQName, typeQName;
  aElement->GetAttribute(NS_LITERAL_STRING("element"), elementQName);
  aElement->GetAttribute(NS_LITERAL_STRING("type"), typeQName);

  if (!elementQName.IsEmpty()) {
    nsAutoString elementPrefix, elementLocalName, elementNamespace;
    
    rv = ParseQualifiedName(aElement,
                            elementQName, 
                            elementPrefix,
                            elementLocalName, 
                            elementNamespace);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsISchemaElement> schemaElement;
    rv = GetSchemaElement(elementLocalName, elementNamespace,
                          getter_AddRefs(schemaElement));
    if (NS_FAILED(rv)) {
      return rv;
    }

    schemaComponent = schemaElement;
  }
  else if (!typeQName.IsEmpty()) {
    nsAutoString typePrefix, typeLocalName, typeNamespace;
    
    rv = ParseQualifiedName(aElement,
                            typeQName, 
                            typePrefix,
                            typeLocalName, 
                            typeNamespace);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsISchemaType> schemaType;
    rv = GetSchemaType(typeLocalName, typeNamespace,
                       getter_AddRefs(schemaType));
    if (NS_FAILED(rv)) {
      return rv;
    }

    schemaComponent = schemaType;
  }
  
  partInst->SetTypeInfo(typeQName, elementQName,
                        schemaComponent);
  aMessage->AddPart(part);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessMessageElement(nsIDOMElement* aElement)
{
  nsresult rv = NS_OK;

  nsAutoString name;
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);

  nsCOMPtr<nsIWSDLMessage> message;
  nsWSDLMessage* messageInst = new nsWSDLMessage(name);
  if (!messageInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  message = messageInst;

  nsChildElementIterator iterator(aElement, 
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE));
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsWSDLAtoms::sDocumentation_atom) {
      messageInst->SetDocumentationElement(childElement);
    }
    else if (tagName == nsWSDLAtoms::sPart_atom) {
      rv = ProcessAbstractPartElement(childElement, messageInst);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  nsAutoString targetNamespace;
  nsWSDLLoadingContext* context = GetCurrentContext();
  if (!context) {
    return NS_ERROR_UNEXPECTED;
  }
  context->GetTargetNamespace(targetNamespace);
  
  name.Append(targetNamespace);
  nsStringKey key(name);
  mMessages.Put(&key, message);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessOperationComponent(nsIDOMElement* aElement,
                                             nsIWSDLMessage** aMessage)
{
  nsresult rv;
  
  nsAutoString messageQName, messagePrefix, messageLocalName, messageNamespace;
  aElement->GetAttribute(NS_LITERAL_STRING("message"), messageQName);

  rv = ParseQualifiedName(aElement,
                          messageQName, 
                          messagePrefix,
                          messageLocalName, 
                          messageNamespace);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  rv = GetMessage(messageLocalName, messageNamespace, aMessage);
  if (NS_FAILED(rv)) {
    // XXX It seem that some WSDL authors eliminate prefixes
    // from qualified names in attribute values, assuming that 
    // the names will resolve to the targetNamespace, while
    // they should technically resolve to the default namespace.
    if (messagePrefix.IsEmpty()) {
      nsAutoString targetNamespace;
      nsWSDLLoadingContext* context = GetCurrentContext();
      if (!context) {
        return NS_ERROR_UNEXPECTED;
      }
      context->GetTargetNamespace(targetNamespace);
      rv = GetMessage(messageLocalName, targetNamespace, aMessage);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  
  return NS_OK;
}
                                            
nsresult
nsWSDLLoadRequest::ProcessAbstractOperation(nsIDOMElement* aElement,
                                            nsWSDLPort* aPort)
{
  nsresult rv = NS_OK;

  nsAutoString name;
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);
  
  nsCOMPtr<nsIWSDLOperation> operation;
  nsWSDLOperation* operationInst = new nsWSDLOperation(name);
  if (!operationInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  operation = operationInst;

  nsAutoString parameterOrder;
  aElement->GetAttribute(NS_LITERAL_STRING("parameterOrder"), parameterOrder);
  if (!parameterOrder.IsEmpty()) {
    nsReadingIterator<PRUnichar> start, end, delimiter;
    parameterOrder.BeginReading(start);
    parameterOrder.EndReading(end);
    
    PRBool found;
    do {
      delimiter = start;

      // Find the next delimiter
      found = FindCharInReadable(PRUnichar(' '), delimiter, end);
          
      // Use the string from the current start position to the
      // delimiter.
      nsAutoString paramName;         
      CopyUnicodeTo(start, delimiter, paramName);
      
      if (!paramName.IsEmpty()) {
        operationInst->AddParameter(paramName);
      }

      // If we did find a delimeter, advance past it
      if (found) {
        start = delimiter;
        ++start;
      }
    } while (found);
  }

  nsChildElementIterator iterator(aElement, 
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE));
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    nsCOMPtr<nsIWSDLMessage> message;
    if (tagName == nsWSDLAtoms::sDocumentation_atom) {
      operationInst->SetDocumentationElement(childElement);
    }
    else if (tagName == nsWSDLAtoms::sInput_atom) {
      rv = ProcessOperationComponent(childElement,
                                     getter_AddRefs(message));
      if (NS_FAILED(rv)) {
        return rv;
      }
      operationInst->SetInput(message);
    }
    else if (tagName == nsWSDLAtoms::sOutput_atom) {
      rv = ProcessOperationComponent(childElement,
                                     getter_AddRefs(message));
      if (NS_FAILED(rv)) {
        return rv;
      }
      operationInst->SetOutput(message);
    }
    else if (tagName == nsWSDLAtoms::sFault_atom) {
      rv = ProcessOperationComponent(childElement,
                                     getter_AddRefs(message));
      if (NS_FAILED(rv)) {
        return rv;
      }
      operationInst->AddFault(message);
    }
  }
  
  aPort->AddOperation(operation);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessPortTypeElement(nsIDOMElement* aElement)
{
  nsresult rv = NS_OK;

  nsAutoString name;
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);
  
  nsCOMPtr<nsIWSDLPort> port;
  nsWSDLPort* portInst = new nsWSDLPort(name);
  if (!portInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  port = portInst;

  nsChildElementIterator iterator(aElement, 
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE));
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsWSDLAtoms::sDocumentation_atom) {
      portInst->SetDocumentationElement(childElement);
    }
    else if (tagName == nsWSDLAtoms::sOperation_atom) {
      rv = ProcessAbstractOperation(childElement, portInst);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  nsAutoString targetNamespace;
  nsWSDLLoadingContext* context = GetCurrentContext();
  if (!context) {
    return NS_ERROR_UNEXPECTED;
  }
  context->GetTargetNamespace(targetNamespace);
  
  name.Append(targetNamespace);
  nsStringKey key(name);
  mPortTypes.Put(&key, port);
  
  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessMessageBinding(nsIDOMElement* aElement, 
                                         nsIWSDLMessage* aMessage)
{
  
  nsChildElementIterator iterator(aElement,
                                  NS_LITERAL_STRING(NS_WSDL_SOAP_NAMESPACE));
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsWSDLAtoms::sBody_atom) {
      nsAutoString partsStr, useStr, encodingStyle, namespaceStr;
      childElement->GetAttribute(NS_LITERAL_STRING("parts"), partsStr);
      childElement->GetAttribute(NS_LITERAL_STRING("use"), useStr);
      childElement->GetAttribute(NS_LITERAL_STRING("encodingStyle"), 
                                 encodingStyle);
      childElement->GetAttribute(NS_LITERAL_STRING("namespace"), namespaceStr);

      PRUint16 use = nsISOAPPartBinding::USE_LITERAL;
      if (useStr.Equals(NS_LITERAL_STRING("encoded"))) {
        use = nsISOAPPartBinding::USE_ENCODED;
      }

      nsCOMPtr<nsISOAPPartBinding> binding;
      nsSOAPPartBinding* bindingInst = new nsSOAPPartBinding(nsISOAPPartBinding::LOCATION_BODY,
                                                             use,
                                                             encodingStyle,
                                                             namespaceStr);
      if (!bindingInst) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      binding = bindingInst;

      nsCOMPtr<nsIWSDLPart> part;
      nsWSDLPart* partInst;
      if (partsStr.IsEmpty()) {
        PRUint32 index, count;
        
        aMessage->GetPartCount(&count);
        for (index = 0; index < count; index++) {
          aMessage->GetPart(index, getter_AddRefs(part));
          partInst = NS_REINTERPRET_CAST(nsWSDLPart*, part.get());
          if (partInst) {
            partInst->SetBinding(binding);
          }
        }
      }
      else {
        nsReadingIterator<PRUnichar> start, end, delimiter;
        partsStr.BeginReading(start);
        partsStr.EndReading(end);

        PRBool found;
        do {
          delimiter = start;

          // Find the next delimiter
          found = FindCharInReadable(PRUnichar(' '), delimiter, end);
          
          // Use the string from the current start position to the
          // delimiter.
          nsAutoString partName;         
          CopyUnicodeTo(start, delimiter, partName);

          if (!partName.IsEmpty()) {
            aMessage->GetPartByName(partName, getter_AddRefs(part));
            partInst = NS_REINTERPRET_CAST(nsWSDLPart*, part.get());
            if (partInst) {
              partInst->SetBinding(binding);
            }
          }

          // If we did find a delimeter, advance past it
          if (found) {
            start = delimiter;
            ++start;
          }
        } while (found);
      }
    }
  }  

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessOperationBinding(nsIDOMElement* aElement,
                                           nsIWSDLPort* aPort)
{
  nsresult rv = NS_OK;

  nsAutoString name;
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);

  nsCOMPtr<nsIWSDLOperation> operation;
  aPort->GetOperationByName(name, getter_AddRefs(operation));
  if (!operation) {
    return NS_ERROR_WSDL_UNKNOWN_WSDL_COMPONENT;
  }
  nsWSDLOperation* operationInst = NS_REINTERPRET_CAST(nsWSDLOperation*,
                                                       operation.get());

  nsCOMPtr<nsISOAPOperationBinding> binding;
  nsSOAPOperationBinding* bindingInst = new nsSOAPOperationBinding();
  if (!bindingInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  binding = bindingInst;

  nsChildElementIterator iterator(aElement);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if ((tagName == nsWSDLAtoms::sDocumentation_atom) &&
        IsElementOfNamespace(childElement, 
                             NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
      bindingInst->SetDocumentationElement(childElement);
    }
    else if ((tagName == nsWSDLAtoms::sOperation_atom) &&
             IsElementOfNamespace(childElement,
                                  NS_LITERAL_STRING(NS_WSDL_SOAP_NAMESPACE))) {
      nsAutoString action, style;
      childElement->GetAttribute(NS_LITERAL_STRING("soapAction"), action);
      childElement->GetAttribute(NS_LITERAL_STRING("style"), style);
      
      bindingInst->SetSoapAction(action);
      if (style.Equals(NS_LITERAL_STRING("rpc"))) {
        bindingInst->SetStyle(nsISOAPPortBinding::STYLE_RPC);
      }
      else if (style.Equals(NS_LITERAL_STRING("document"))) {
        bindingInst->SetStyle(nsISOAPPortBinding::STYLE_DOCUMENT);
      }
    }
    else if ((tagName == nsWSDLAtoms::sInput_atom) &&
             IsElementOfNamespace(childElement,
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
      nsCOMPtr<nsIWSDLMessage> input;
      
      operation->GetInput(getter_AddRefs(input));
      rv = ProcessMessageBinding(childElement, input);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else if ((tagName == nsWSDLAtoms::sOutput_atom) &&
             IsElementOfNamespace(childElement,
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
      nsCOMPtr<nsIWSDLMessage> output;
      
      operation->GetOutput(getter_AddRefs(output));
      rv = ProcessMessageBinding(childElement, output);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else if ((tagName == nsWSDLAtoms::sFault_atom) &&
             IsElementOfNamespace(childElement,
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
      // XXX TO BE IMPLEMENTED
    }    
  }

  operationInst->SetBinding(binding);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessBindingElement(nsIDOMElement* aElement)
{
  nsresult rv = NS_OK;

  nsAutoString name;
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);

  PRBool foundSOAPBinding;
  nsCOMPtr<nsIWSDLBinding> binding;
  nsSOAPPortBinding* bindingInst = new nsSOAPPortBinding(name);
  if (!bindingInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  binding = bindingInst;
  bindingInst->SetAddress(mAddress);

  nsCOMPtr<nsIWSDLPort> port;
  nsAutoString typeQName, typePrefix, typeLocalName, typeNamespace;
  aElement->GetAttribute(NS_LITERAL_STRING("type"), typeQName);
  rv = ParseQualifiedName(aElement,
                          typeQName,
                          typePrefix,
                          typeLocalName,
                          typeNamespace);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  rv = GetPortType(typeLocalName, typeNamespace, getter_AddRefs(port));
  if (NS_FAILED(rv)) {
    // XXX It seem that some WSDL authors eliminate prefixes
    // from qualified names in attribute values, assuming that 
    // the names will resolve to the targetNamespace, while
    // they should technically resolve to the default namespace.
    if (typePrefix.IsEmpty()) {
      nsAutoString targetNamespace;
      nsWSDLLoadingContext* context = GetCurrentContext();
      if (!context) {
        return NS_ERROR_UNEXPECTED;
      }
      context->GetTargetNamespace(targetNamespace);
      rv = GetPortType(typeLocalName, targetNamespace, getter_AddRefs(port));
      if (NS_FAILED(rv)) {
        return rv;    // Can't find a port type of the specified name
      }
    }
  }

  nsChildElementIterator iterator(aElement);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if ((tagName == nsWSDLAtoms::sDocumentation_atom) &&
        IsElementOfNamespace(childElement, 
                             NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
      bindingInst->SetDocumentationElement(childElement);
    }
    else if ((tagName == nsWSDLAtoms::sBinding_atom) &&
             IsElementOfNamespace(childElement, 
                                  NS_LITERAL_STRING(NS_WSDL_SOAP_NAMESPACE))) {
      nsAutoString style, transport;
      childElement->GetAttribute(NS_LITERAL_STRING("style"), style);
      childElement->GetAttribute(NS_LITERAL_STRING("transport"), transport);

      if (style.Equals(NS_LITERAL_STRING("rpc"))) {
        bindingInst->SetStyle(nsISOAPPortBinding::STYLE_RPC);
      }
      else if (style.Equals(NS_LITERAL_STRING("document"))) {
        bindingInst->SetStyle(nsISOAPPortBinding::STYLE_DOCUMENT);
      }
      bindingInst->SetTransport(transport);
      foundSOAPBinding = PR_TRUE;
    }
    else if ((tagName == nsWSDLAtoms::sOperation_atom) && 
             IsElementOfNamespace(childElement, 
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
      rv = ProcessOperationBinding(childElement,
                                   port);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }  

  if (!foundSOAPBinding) {
    // If we don't have a SOAP binding, we can't continue
    return NS_ERROR_WSDL_BINDING_NOT_FOUND; 
  }
  nsWSDLPort* portInst = NS_REINTERPRET_CAST(nsWSDLPort*, port.get());
  portInst->SetBinding(binding);
  
  mPort = port;

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessPortBinding(nsIDOMElement* aElement)
{  
  nsChildElementIterator iterator(aElement);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if ((tagName == nsWSDLAtoms::sAddress_atom) && 
        IsElementOfNamespace(childElement, 
                             NS_LITERAL_STRING(NS_WSDL_SOAP_NAMESPACE))) {
      childElement->GetAttribute(NS_LITERAL_STRING("location"),
                                 mAddress);
    }
  }
  
  return NS_OK;
}

nsresult
nsWSDLLoadRequest::ProcessServiceElement(nsIDOMElement* aElement)
{
  nsresult rv;

  nsChildElementIterator iterator(aElement, 
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE));
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsWSDLAtoms::sPort_atom) {
      nsAutoString name;
      childElement->GetAttribute(NS_LITERAL_STRING("name"), name);
      if (name.Equals(mPortName)) {
        nsAutoString bindingQName, bindingPrefix;
        
        childElement->GetAttribute(NS_LITERAL_STRING("binding"), bindingQName);
        rv = ParseQualifiedName(childElement,
                                bindingQName, 
                                bindingPrefix,
                                mBindingName, 
                                mBindingNamespace);
        if (NS_FAILED(rv)) {
          return rv; // binding of an unknown namespace
        }
        
        rv = ProcessPortBinding(childElement);
        if (NS_FAILED(rv)) {
          return rv;
        }

        break;
      }
    }
  }  

  return NS_OK;
}

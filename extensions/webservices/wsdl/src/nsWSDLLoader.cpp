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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
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

#include "nsIWebServiceErrorHandler.h"

#include "nsWSDLLoader.h"

// loading includes
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsNetUtil.h"

// content includes
#include "nsIDOMDocument.h"
#include "nsIDOM3Node.h"

// string includes
#include "nsReadableUtils.h"

// XPConnect includes
#include "nsIXPConnect.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"

// XPCOM includes
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsStaticAtom.h"


// XMLExtras includes
#include "nsISOAPMessage.h"

// pref. includes
#include "nsIPrefService.h"

#define SCHEMA_2001_NAMESPACE "http://www.w3.org/2001/XMLSchema"
#define SCHEMA_1999_NAMESPACE "http://www.w3.org/1999/XMLSchema"

////////////////////////////////////////////////////////////
//
// nsWSDLAtoms implementation
//
////////////////////////////////////////////////////////////

// define storage for all atoms
#define WSDL_ATOM(_name, _value) nsIAtom* nsWSDLAtoms::_name;
#include "nsWSDLAtomList.h"
#undef WSDL_ATOM

static const nsStaticAtom atomInfo[] = {
#define WSDL_ATOM(_name, _value) { _value, &nsWSDLAtoms::_name },
#include "nsWSDLAtomList.h"
#undef WSDL_ATOM
};

nsresult
nsWSDLAtoms::AddRefAtoms()
{
  return NS_RegisterStaticAtoms(atomInfo, NS_ARRAY_LENGTH(atomInfo));
}

////////////////////////////////////////////////////////////
//
// nsWSDLLoader implementation
//
////////////////////////////////////////////////////////////

nsWSDLLoader::nsWSDLLoader()
{
}

nsWSDLLoader::~nsWSDLLoader()
{
}

NS_IMPL_ISUPPORTS1_CI(nsWSDLLoader, nsIWSDLLoader)

nsresult
nsWSDLLoader::Init()
{
  PRBool disabled = PR_FALSE;
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    if (NS_FAILED(prefBranch->GetBoolPref("xml.xmlextras.soap.wsdl.disabled",
                                          &disabled))) {
      // We default to enabling WSDL, it'll only be disabled if
      // specificly disabled in the prefs.
      disabled = PR_FALSE;
    }
  }

  return disabled ? NS_ERROR_WSDL_NOT_ENABLED : NS_OK;
}
nsresult
nsWSDLLoader::GetResolvedURI(const nsAString& aWSDLURI, const char* aMethod,
                             nsIURI** aURI)
{
  nsresult rv;
  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
  if(xpc) {
    xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (cc) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIPrincipal> principal;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    if (NS_SUCCEEDED(rv)) {
      principal->GetURI(getter_AddRefs(baseURI));
    }

    rv = NS_NewURI(aURI, aWSDLURI, nsnull, baseURI);
    if (NS_FAILED(rv))
      return rv;

    rv = secMan->CheckLoadURIFromScript(cx, *aURI);
    if (NS_FAILED(rv)) {
      // Security check failed. The above call set a JS exception. The
      // following lines ensure that the exception is propagated.
      cc->SetExceptionWasThrown(PR_TRUE);
      return rv;
    }
  }
  else {
    rv = NS_NewURI(aURI, aWSDLURI, nsnull);
    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

// Internal helper called by Load() and Loadasync(). If aListener is
// non-null, it's an async load, if it's null, it's sync.
nsresult
nsWSDLLoader::doLoad(const nsAString& wsdlURI, const nsAString& portName,
                     nsIWSDLLoadListener *aListener, nsIWSDLPort **_retval)
{
  nsCOMPtr<nsIURI> resolvedURI;
  nsresult rv = GetResolvedURI(wsdlURI, aListener ? "loadAsync" : "load",
                               getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDOMEventListener> listener;
  nsWSDLLoadRequest* request = new nsWSDLLoadRequest(!aListener, aListener,
                                                     portName);
  if (!request) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  listener = request;

  nsCAutoString spec;
  resolvedURI->GetSpec(spec);

  rv = request->LoadDefinition(NS_ConvertUTF8toUCS2(spec));

  if (NS_SUCCEEDED(rv) && !aListener) {
    request->GetPort(_retval);
  }

  return rv;
}

/* nsIWSDLService load (in AString wsdlURI, in AString portName); */
NS_IMETHODIMP
nsWSDLLoader::Load(const nsAString& wsdlURI, const nsAString& portName,
                   nsIWSDLPort **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  return doLoad(wsdlURI, portName, nsnull, _retval);
}

/* void loadAsync (in AString wsdlURI, in AString portName,
   in nsIWSDLLoadListener listener); */
NS_IMETHODIMP
nsWSDLLoader::LoadAsync(const nsAString& wsdlURI, const nsAString& portName,
                        nsIWSDLLoadListener *aListener)
{
  NS_ENSURE_ARG(aListener);

  return doLoad(wsdlURI, portName, aListener, nsnull);
}

////////////////////////////////////////////////////////////
//
// nsWSDLLoadRequest implementation
//
////////////////////////////////////////////////////////////
nsWSDLLoadRequest::nsWSDLLoadRequest(PRBool aIsSync,
                                     nsIWSDLLoadListener* aListener,
                                     const nsAString& aPortName)
  : mListener(aListener), mIsSync(aIsSync), mPortName(aPortName)
{
  mErrorHandler = mListener;

  if (!mErrorHandler) {
    NS_WARNING("nsWSDLLoadRequest::<init>: Error about interface "
               "nsIWebserviceErrorHandler");

  }
}

nsWSDLLoadRequest::~nsWSDLLoadRequest()
{
  while (GetCurrentContext() != nsnull) {
    PopContext();
  }
}

NS_IMPL_ISUPPORTS1(nsWSDLLoadRequest, nsIDOMEventListener)

static inline PRBool
IsElementOfNamespace(nsIDOMElement* aElement, const nsAString& aNamespace)
{
	nsAutoString namespaceURI;
	aElement->GetNamespaceURI(namespaceURI);
	return namespaceURI.Equals(aNamespace);
}

nsresult
nsWSDLLoadRequest::LoadDefinition(const nsAString& aURI)
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

  const nsAString& empty = EmptyString();
  rv = mRequest->OpenRequest(NS_LITERAL_CSTRING("GET"),
                             NS_ConvertUTF16toUTF8(aURI), !mIsSync, empty,
                             empty);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Force the mimetype of the returned stream to be xml.
  rv = mRequest->OverrideMimeType(NS_LITERAL_CSTRING("text/xml"));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mIsSync) {
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mRequest));
    if (!target) {
      return NS_ERROR_UNEXPECTED;
    }

    rv = target->AddEventListener(NS_LITERAL_STRING("load"), this, PR_FALSE);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = target->AddEventListener(NS_LITERAL_STRING("error"), this, PR_FALSE);
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
      nsAutoString errorMsg(NS_LITERAL_STRING("Failure retrieving XML "));
      errorMsg.AppendLiteral("response for WSDL");
      NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }

    nsCOMPtr<nsIDOMElement> element;
    if (document)
      document->GetDocumentElement(getter_AddRefs(element));
    if (element) {
      if (IsElementOfNamespace(element,
                               NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
        rv = PushContext(document, aURI);
        if (NS_FAILED(rv)) {
          nsAutoString elementName;
          nsresult rc = element->GetTagName(elementName);
          NS_ENSURE_SUCCESS(rc, rc);

          nsAutoString errorMsg;
          errorMsg.AppendLiteral("Failure queuing element \"");
          errorMsg.Append(elementName);
          errorMsg.AppendLiteral("\" to be processed");

          NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

          return rv;
        }

        rv = ResumeProcessing();

        PopContext();

        if (NS_FAILED(rv)) {
          nsAutoString elementName;
          nsresult rc = element->GetTagName(elementName);
          NS_ENSURE_SUCCESS(rc, rc);

          nsAutoString errorMsg;
          errorMsg.AppendLiteral("Failure processing WSDL element \"");
          errorMsg.Append(elementName);
          errorMsg.AppendLiteral("\"");

          NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

          return rv;
        }
      }
      else if (IsElementOfNamespace(element,
                             NS_LITERAL_STRING(SCHEMA_2001_NAMESPACE)) ||
               IsElementOfNamespace(element,
                             NS_LITERAL_STRING(SCHEMA_1999_NAMESPACE))) {
        nsCOMPtr<nsISchema> schema;
        rv = mSchemaLoader->ProcessSchemaElement(mErrorHandler, element,
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
        // element of unknown namespace
        rv = NS_ERROR_WSDL_NOT_WSDL_ELEMENT;
        nsAutoString elementName;
        nsresult rc = element->GetTagName(elementName);
        NS_ENSURE_SUCCESS(rc, rc);

        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing WSDL, element of ");
        errorMsg.AppendLiteral("unknown namespace \"");
        errorMsg.Append(elementName);
        errorMsg.AppendLiteral("\"");

        NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

        return rv;        
      }
    }
    else {
      nsAutoString errorMsg(NS_LITERAL_STRING("Failure processing WSDL, no document"));
      NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_NOT_WSDL_ELEMENT, errorMsg);

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

  if (eventType.EqualsLiteral("load")) {
    nsCOMPtr<nsIDOMDocument> document;

    rv = mRequest->GetResponseXML(getter_AddRefs(document));
    if (document) {
      nsCOMPtr<nsIDOMElement> element;

      document->GetDocumentElement(getter_AddRefs(element));
      if (element) {
        if (IsElementOfNamespace(element,
                                 NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
          nsCOMPtr<nsIChannel> channel;
          nsCOMPtr<nsIURI> uri;
          nsCAutoString spec;

          mRequest->GetChannel(getter_AddRefs(channel));

          if (channel) {
            channel->GetURI(getter_AddRefs(uri));
            if (uri) {
              uri->GetSpec(spec);
            }
          }

          rv = PushContext(document, NS_ConvertUTF8toUCS2(spec));

          if (NS_SUCCEEDED(rv)) {
            rv = ContineProcessingTillDone();

            if (NS_FAILED(rv)) {
              nsAutoString elementName;
              element->GetTagName(elementName);

              NS_ENSURE_SUCCESS(rv, rv);

              nsAutoString errorMsg;
              errorMsg.AppendLiteral("Failure processing WSDL element \"");
              errorMsg.Append(elementName);
              errorMsg.AppendLiteral("\"");
              
              NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_LOADING_ERROR, errorMsg);

              return NS_ERROR_WSDL_LOADING_ERROR;
            }
          }
          else {
            nsAutoString elementName;
            element->GetTagName(elementName);

            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoString errorMsg;
            errorMsg.AppendLiteral("Failure queuing WSDL element \"");
            errorMsg.Append(elementName);
            errorMsg.AppendLiteral("\" for processing");

            NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_LOADING_ERROR, errorMsg);

            return NS_ERROR_WSDL_LOADING_ERROR;
          }
        }
        else if (IsElementOfNamespace(element,
                                NS_LITERAL_STRING(SCHEMA_2001_NAMESPACE)) ||
                 IsElementOfNamespace(element,
                                NS_LITERAL_STRING(SCHEMA_1999_NAMESPACE))) {
          nsCOMPtr<nsISchema> schema;
          rv = mSchemaLoader->ProcessSchemaElement(mErrorHandler, element,
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
          // element of unknown namespace
          nsAutoString elementName;
          rv = element->GetTagName(elementName);
          NS_ENSURE_SUCCESS(rv, rv);

          rv = NS_ERROR_WSDL_NOT_WSDL_ELEMENT;

          nsAutoString errorMsg;
          errorMsg.AppendLiteral("Failure processing WSDL, ");
          errorMsg.AppendLiteral("element of unknown namespace \"");
          errorMsg.Append(elementName);
          errorMsg.AppendLiteral("\"");

          NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

          return rv;
        }
      }
      else {
        rv = NS_ERROR_WSDL_NOT_WSDL_ELEMENT;
        
        nsAutoString errorMsg(NS_LITERAL_STRING("Failure processing WSDL document"));
        NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

        return rv;
      }
    }
    if (NS_FAILED(rv)) {
      mListener->OnError(rv, NS_LITERAL_STRING("Failure processing WSDL document"));
      return NS_OK;
    }
  }
  else if (eventType.EqualsLiteral("error")) {
    nsAutoString errorMsg(NS_LITERAL_STRING("Failure loading WSDL document"));
    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_LOADING_ERROR, errorMsg);

    return NS_OK;
  }

  if (GetCurrentContext() == nsnull) {
    if (mPort) {
      mListener->OnLoad(mPort);
    }
    else {
      nsAutoString errorMsg(NS_LITERAL_STRING("WSDL Binding not found"));
      NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_BINDING_NOT_FOUND, errorMsg);
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
    // The service element tends to be at the end of WSDL files, so
    // this would be more efficient if we iterated from last to first
    // child...
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
                               const nsAString& aURISpec)
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
    nsWSDLLoadingContext* context =
      NS_STATIC_CAST(nsWSDLLoadingContext*, mContextStack.ElementAt(count-1));
    delete context;
    mContextStack.RemoveElementAt(count-1);
  }
}

nsresult
nsWSDLLoadRequest::GetSchemaElement(const nsAString& aName,
                                    const nsAString& aNamespace,
                                    nsISchemaElement** aSchemaComponent)
{
  nsStringKey key(aNamespace);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mTypes.Get(&key));
  nsCOMPtr<nsISchema> schema(do_QueryInterface(sup));
  if (!schema) {
    nsAutoString errorMsg(NS_LITERAL_STRING("Failure processing WSDL, "));
    errorMsg.AppendLiteral("element is not schema");
    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT, errorMsg);

    return NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT;
  }

  nsCOMPtr<nsISchemaElement> element;
  schema->GetElementByName(aName, getter_AddRefs(element));
  if (!element) {
    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing WSDL, unknown schema component \"");
    errorMsg.Append(aNamespace);
    errorMsg.AppendLiteral(":");
    errorMsg.Append(aName);
    errorMsg.AppendLiteral("\"");

    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT, errorMsg);

    return NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT;
  }

  *aSchemaComponent = element;
  NS_IF_ADDREF(*aSchemaComponent);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::GetSchemaType(const nsAString& aName,
                                 const nsAString& aNamespace,
                                 nsISchemaType** aSchemaComponent)
{
  if (aNamespace.EqualsLiteral(SCHEMA_2001_NAMESPACE) ||
      aNamespace.EqualsLiteral(SCHEMA_1999_NAMESPACE)) {
    nsCOMPtr<nsISchemaCollection> collection(do_QueryInterface(mSchemaLoader));
    return collection->GetType(aName, aNamespace, aSchemaComponent);
  }

  nsStringKey key(aNamespace);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mTypes.Get(&key));
  nsCOMPtr<nsISchema> schema(do_QueryInterface(sup));
  if (!schema) {
    nsAutoString errorMsg(NS_LITERAL_STRING("Failure processing WSDL, not schema"));
    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT, errorMsg);

    return NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT;
  }

  nsCOMPtr<nsISchemaType> type;
  schema->GetTypeByName(aName, getter_AddRefs(type));
  if (!type) {
    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing WSDL, unknown schema type \"");
    errorMsg.Append(aNamespace);
    errorMsg.AppendLiteral(":");
    errorMsg.Append(aName);
    errorMsg.AppendLiteral("\"");

    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT, errorMsg);

    return NS_ERROR_WSDL_UNKNOWN_SCHEMA_COMPONENT;
  }

  *aSchemaComponent = type;
  NS_IF_ADDREF(*aSchemaComponent);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::GetMessage(const nsAString& aName,
                              const nsAString& aNamespace,
                              nsIWSDLMessage** aMessage)
{
  nsAutoString keyStr(aName);
  keyStr.Append(aNamespace);

  nsStringKey key(keyStr);

  nsCOMPtr<nsISupports> sup = dont_AddRef(mMessages.Get(&key));
  nsCOMPtr<nsIWSDLMessage> message(do_QueryInterface(sup));
  if (!message) {
    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing WSDL, unknown WSDL component \"");
    errorMsg.Append(aNamespace);
    errorMsg.AppendLiteral(":");
    errorMsg.Append(aName);
    errorMsg.AppendLiteral("\"");

    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_UNKNOWN_WSDL_COMPONENT, errorMsg);

    return NS_ERROR_WSDL_UNKNOWN_WSDL_COMPONENT;
  }

  *aMessage = message;
  NS_IF_ADDREF(*aMessage);

  return NS_OK;
}

nsresult
nsWSDLLoadRequest::GetPortType(const nsAString& aName,
                               const nsAString& aNamespace,
                               nsIWSDLPort** aPort)
{
  nsAutoString keyStr(aName);
  keyStr.Append(aNamespace);

  nsStringKey key(keyStr);

  nsCOMPtr<nsISupports> sup = dont_AddRef(mPortTypes.Get(&key));
  nsCOMPtr<nsIWSDLPort> port(do_QueryInterface(sup));
  if (!port) {
    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing WSDL, unknown WSDL port type \"");
    errorMsg.Append(aNamespace);
    errorMsg.AppendLiteral(":");
    errorMsg.Append(aName);
    errorMsg.AppendLiteral("\"");

    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_UNKNOWN_WSDL_COMPONENT, errorMsg);

    return NS_ERROR_WSDL_UNKNOWN_WSDL_COMPONENT;
  }

  *aPort = port;
  NS_IF_ADDREF(*aPort);

  return NS_OK;
}

static nsresult
ParseQualifiedName(nsIDOMElement* aContext, const nsAString& aQualifiedName,
                   nsAString& aPrefix, nsAString& aLocalName,
                   nsAString& aNamespaceURI)
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
    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing WSDL, ");
    errorMsg.AppendLiteral("cannot find base URI for document location \"");
    errorMsg.Append(documentLocation);
    errorMsg.AppendLiteral("\" for import \"");
    errorMsg.Append(location);
    errorMsg.AppendLiteral("\"");

    NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

    return rv;
  }

  rv = NS_NewURI(getter_AddRefs(uri), location, nsnull, baseURI);
  if (NS_FAILED(rv)) {
    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing WSDL, Cannot find URI for import \"");
    errorMsg.Append(location);
    errorMsg.AppendLiteral("\"");

    NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

    return rv;
  }

  // Fix ( bug 202478 ) a potential stack overflow by 
  // preventing the wsdl file from loading if it was  
  // already loaded via the import element.
  PRUint32 count = mImportList.Count();
  PRUint32 i;
  for (i = 0; i < count; i++) {
    PRBool equal;  
    mImportList[i]->Equals(uri, &equal);
    if (equal) {
      // Looks like this uri has already been loaded.
      // Loading it again will end up in an infinite loop.
      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing WSDL, import \"");
      errorMsg.Append(location);
      errorMsg.AppendLiteral("\" could cause recursive import");

      NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_RECURSIVE_IMPORT, errorMsg);

      return NS_ERROR_WSDL_RECURSIVE_IMPORT;
    }
  }

  mImportList.AppendObject(uri);
  
  nsCAutoString spec;
  uri->GetSpec(spec);

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
  static const char* kSchemaNamespaces[] =
    {
      SCHEMA_1999_NAMESPACE,
      SCHEMA_2001_NAMESPACE
    };
  static const PRUint32 kSchemaNamespacesLength =
    sizeof(kSchemaNamespaces) / sizeof(const char*);

  nsresult rv = NS_OK;

  nsChildElementIterator iterator(aElement,
                                  kSchemaNamespaces, kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    // XXX : We need to deal with xs:import elements too.
    if (tagName == nsWSDLAtoms::sSchema_atom) {
      nsCOMPtr<nsISchema> schema;
      rv = mSchemaLoader->ProcessSchemaElement(mErrorHandler, childElement,
                                               getter_AddRefs(schema));
      if (NS_FAILED(rv)) {
        return NS_ERROR_WSDL_SCHEMA_PROCESSING_ERROR;
      }

      nsAutoString targetNamespace;
      schema->GetTargetNamespace(targetNamespace);

      nsStringKey key(targetNamespace);
      mTypes.Put(&key, schema);
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

    rv = ParseQualifiedName(aElement, elementQName, elementPrefix,
                            elementLocalName, elementNamespace);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsISchemaElement> schemaElement;
    rv = GetSchemaElement(elementLocalName, elementNamespace,
                          getter_AddRefs(schemaElement));
    if (NS_FAILED(rv)) {
      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing WSDL, cannot find schema element \"");
      errorMsg.Append(elementNamespace);
      errorMsg.AppendLiteral(":");
      errorMsg.Append(elementLocalName);
      errorMsg.AppendLiteral("\"");

      NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }

    schemaComponent = schemaElement;
  }
  else if (!typeQName.IsEmpty()) {
    nsAutoString typePrefix, typeLocalName, typeNamespace;

    rv = ParseQualifiedName(aElement, typeQName, typePrefix, typeLocalName,
                            typeNamespace);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsISchemaType> schemaType;
    rv = GetSchemaType(typeLocalName, typeNamespace,
                       getter_AddRefs(schemaType));
    if (NS_FAILED(rv)) {
      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing WSDL, cannot find schema type \"");
      errorMsg.Append(typeNamespace);
      errorMsg.AppendLiteral(":");
      errorMsg.Append(typeLocalName);
      errorMsg.AppendLiteral("\"");

      NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }

    schemaComponent = schemaType;
  }

  partInst->SetTypeInfo(typeQName, elementQName, schemaComponent);
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

  rv = ParseQualifiedName(aElement, messageQName, messagePrefix,
                          messageLocalName, messageNamespace);
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
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing WSDL, cannot find message \"");
        errorMsg.Append(targetNamespace);
        errorMsg.AppendLiteral(":");
        errorMsg.Append(messageLocalName);
        errorMsg.AppendLiteral("\"");

        NS_WSDLLOADER_FIRE_ERROR(rv, errorMsg);

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

      // If we did find a delimiter, advance past it
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
      rv = ProcessOperationComponent(childElement, getter_AddRefs(message));
      if (NS_FAILED(rv)) {
        return rv;
      }
      operationInst->SetInput(message);
    }
    else if (tagName == nsWSDLAtoms::sOutput_atom) {
      rv = ProcessOperationComponent(childElement, getter_AddRefs(message));
      if (NS_FAILED(rv)) {
        return rv;
      }
      operationInst->SetOutput(message);
    }
    else if (tagName == nsWSDLAtoms::sFault_atom) {
      rv = ProcessOperationComponent(childElement, getter_AddRefs(message));
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
      if (useStr.EqualsLiteral("encoded")) {
        use = nsISOAPPartBinding::USE_ENCODED;
      }

      nsCOMPtr<nsISOAPMessageBinding> messageBinding;
      nsSOAPMessageBinding* messageBindingInst =
        new nsSOAPMessageBinding(namespaceStr);
      if (!messageBindingInst) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      messageBinding = messageBindingInst;
      nsWSDLMessage* messageInst = NS_REINTERPRET_CAST(nsWSDLMessage*,
                                                       aMessage);
      messageInst->SetBinding(messageBinding);

      nsCOMPtr<nsISOAPPartBinding> binding;
      nsSOAPPartBinding* bindingInst =
        new nsSOAPPartBinding(nsISOAPPartBinding::LOCATION_BODY, use,
                              encodingStyle, namespaceStr);
      if (!bindingInst) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      binding = bindingInst;

      nsCOMPtr<nsIWSDLPart> part;
      nsWSDLPart* partInst;
      // If there is no explicit parts attribute, this binding applies
      // to all the parts.
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

          // If we did find a delimiter, advance past it
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
    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing WSDL, cannot find operation \"");
    errorMsg.Append(name);
    errorMsg.AppendLiteral("\"");

    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_UNKNOWN_WSDL_COMPONENT, errorMsg);

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
      if (style.EqualsLiteral("rpc")) {
        bindingInst->SetStyle(nsISOAPPortBinding::STYLE_RPC);
      }
      else if (style.EqualsLiteral("document")) {
        bindingInst->SetStyle(nsISOAPPortBinding::STYLE_DOCUMENT);
      }
      // If one isn't explicitly specified, we inherit from the port
      else {
        nsCOMPtr<nsIWSDLBinding> portBinding;
        aPort->GetBinding(getter_AddRefs(portBinding));
        nsCOMPtr<nsISOAPPortBinding> soapPortBinding =
          do_QueryInterface(portBinding);
        if (soapPortBinding) {
          PRUint16 styleVal;
          soapPortBinding->GetStyle(&styleVal);
          bindingInst->SetStyle(styleVal);
        }
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

      //XXX To be implemented
      nsAutoString errorMsg(NS_LITERAL_STRING("Fault management not yet "));
      errorMsg.AppendLiteral("implemented in WSDL support");
      NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_NOT_IMPLEMENTED, errorMsg);
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

  PRBool foundSOAPBinding = PR_FALSE;
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
  rv = ParseQualifiedName(aElement, typeQName, typePrefix, typeLocalName,
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
      nsWSDLLoadingContext* context = GetCurrentContext();
      if (!context) {
        return NS_ERROR_UNEXPECTED;
      }

      nsAutoString targetNamespace;
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
      // XXX There should be different namespaces for newer versions
      // of SOAP.
      bindingInst->SetSoapVersion(nsISOAPPortBinding::SOAP_VERSION_1_1);

      nsAutoString style, transport;
      childElement->GetAttribute(NS_LITERAL_STRING("style"), style);
      childElement->GetAttribute(NS_LITERAL_STRING("transport"), transport);

      if (style.EqualsLiteral("rpc")) {
        bindingInst->SetStyle(nsISOAPPortBinding::STYLE_RPC);
      }
      else if (style.EqualsLiteral("document")) {
        bindingInst->SetStyle(nsISOAPPortBinding::STYLE_DOCUMENT);
      }
      bindingInst->SetTransport(transport);
      foundSOAPBinding = PR_TRUE;
    }
    else if ((tagName == nsWSDLAtoms::sOperation_atom) &&
             IsElementOfNamespace(childElement,
                                  NS_LITERAL_STRING(NS_WSDL_NAMESPACE))) {
      rv = ProcessOperationBinding(childElement, port);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  if (!foundSOAPBinding) {
    // If we don't have a SOAP binding, we can't continue
    nsAutoString errorMsg(NS_LITERAL_STRING("Failure processing WSDL, "));
    errorMsg.AppendLiteral("no SOAP binding found");
    NS_WSDLLOADER_FIRE_ERROR(NS_ERROR_WSDL_BINDING_NOT_FOUND, errorMsg);

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
      childElement->GetAttribute(NS_LITERAL_STRING("location"), mAddress);
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
        rv = ParseQualifiedName(childElement, bindingQName, bindingPrefix,
                                mBindingName, mBindingNamespace);
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

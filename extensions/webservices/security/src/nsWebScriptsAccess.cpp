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
 * The Original Code is the web scripts access security code.
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

#include "nsWebScriptsAccess.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIPrincipal.h"
#include "nsIURL.h"
#include "nsReadableUtils.h"
#include "nsIHttpChannel.h"
#include "nsNetUtil.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsISOAPCall.h"
#include "nsISOAPEncoding.h"
#include "nsISOAPResponse.h"
#include "nsISOAPFault.h"
#include "nsISOAPParameter.h"
#include "nsISOAPBlock.h"
#include "nsIVariant.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsIJSContextStack.h"

#define WSA_GRANT_ACCESS_TO_ALL     (1 << 0)
#define WSA_FILE_NOT_FOUND          (1 << 1)
#define WSA_FILE_DELEGATED          (1 << 2)
#define SERVICE_LISTED_PUBLIC       (1 << 3)
#define HAS_MASTER_SERVICE_DECISION (1 << 4)

static PRBool PR_CALLBACK 
FreeEntries(nsHashKey *aKey, void *aData, void* aClosure)
{
  AccessInfoEntry* entry = NS_REINTERPRET_CAST(AccessInfoEntry*, aData);
  delete entry;
  return PR_TRUE;
}

NS_IMPL_ISUPPORTS1(nsWebScriptsAccess, 
                   nsIWebScriptsAccessService)

nsWebScriptsAccess::nsWebScriptsAccess()
  : NS_LITERAL_STRING_INIT(kNamespace2002, "http://www.mozilla.org/2002/soap/security")
  , NS_LITERAL_STRING_INIT(kWebScriptAccessTag, "webScriptAccess")
  , NS_LITERAL_STRING_INIT(kDelegateTag, "delegate")
  , NS_LITERAL_STRING_INIT(kAllowTag, "allow")
  , NS_LITERAL_STRING_INIT(kTypeAttr, "type")
  , NS_LITERAL_STRING_INIT(kFromAttr, "from")
  , NS_LITERAL_STRING_INIT(kAny, "any")
  , NS_LITERAL_STRING_INIT(kIsServicePublic, "isServicePublic")
{
}

nsWebScriptsAccess::~nsWebScriptsAccess()
{
  mAccessInfoTable.Enumerate(FreeEntries, this);
}

NS_IMETHODIMP 
nsWebScriptsAccess::CanAccess(nsIURI* aTransportURI,
                              const nsAString& aRequestType,
                              PRBool* aAccessGranted)
{
  *aAccessGranted = PR_FALSE;
  NS_ENSURE_ARG_POINTER(aTransportURI);

  nsresult rv;
  if (!mSecurityManager) {
    mSecurityManager = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
   
  rv =
    mSecurityManager->IsCapabilityEnabled("UniversalBrowserRead", 
                                          aAccessGranted);
  if (NS_FAILED(rv) || *aAccessGranted)
    return rv;
  
  mServiceURI = aTransportURI;

  nsXPIDLCString path;
  aTransportURI->GetPrePath(path);
  path += '/';

  AccessInfoEntry* entry = 0;
  rv = GetAccessInfoEntry(path, &entry);
  if (!entry) {
    rv = mSecurityManager->CheckSameOrigin(0, aTransportURI);
    if (NS_SUCCEEDED(rv)) {
      // script security manager has granted access
      *aAccessGranted = PR_TRUE;
      return rv;
    }
    else {
      // Script security manager has denied access and has set an
      // exception. Clear the exception and fall back on the new
      // security model's decision.
      nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
      if (xpc) {
        nsCOMPtr<nsIXPCNativeCallContext> cc;
        xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
        if (cc) {
          JSContext* cx;
          rv = cc->GetJSContext(&cx);
          NS_ENSURE_SUCCESS(rv, rv);

          JS_ClearPendingException(cx);
          cc->SetExceptionWasThrown(PR_FALSE);
        }
      }
    }

    rv = CreateEntry(path, PR_FALSE, &entry);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return CheckAccess(entry, aRequestType, aAccessGranted);
}

NS_IMETHODIMP 
nsWebScriptsAccess::InvalidateCache(const char* aTransportURI)
{
  if (aTransportURI) {
    nsCStringKey key(aTransportURI);
    if (mAccessInfoTable.Exists(&key)) {
      AccessInfoEntry* entry = 
        NS_REINTERPRET_CAST(AccessInfoEntry*, mAccessInfoTable.Remove(&key));
      delete entry;
    }
  }
  else {
    // If a URI is not specified then we clear the entire cache.
    mAccessInfoTable.Enumerate(FreeEntries, this);
  }
  return NS_OK;
}

nsresult 
nsWebScriptsAccess::GetAccessInfoEntry(const char* aKey,
                                       AccessInfoEntry** aEntry)
{
  nsCStringKey key(aKey);

  *aEntry = NS_REINTERPRET_CAST(AccessInfoEntry*, mAccessInfoTable.Get(&key));
  if (*aEntry  && ((*aEntry)->mFlags & WSA_FILE_DELEGATED)) {
    nsresult rv;
    nsCOMPtr<nsIURL> url(do_QueryInterface(mServiceURI, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
  
    nsCAutoString path;
    url->GetPrePath(path);
    nsCAutoString directory;
    url->GetDirectory(directory);
    path += directory;

    return GetAccessInfoEntry(path.get(), aEntry);
  }
  return NS_OK;
}

nsresult 
nsWebScriptsAccess::GetDocument(const nsACString& aDeclFilePath,
                                nsIDOMDocument** aDocument)
{
  nsresult rv = NS_OK;
  
  if (!mRequest) {
    mRequest = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  const nsAString& empty = EmptyString();
  rv = mRequest->OpenRequest(NS_LITERAL_CSTRING("GET"), aDeclFilePath,
                             PR_FALSE, empty, empty);
  NS_ENSURE_SUCCESS(rv, rv);
    
  rv = mRequest->OverrideMimeType(NS_LITERAL_CSTRING("text/xml"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mRequest->Send(0);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  mRequest->GetChannel(getter_AddRefs(channel));
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel, &rv));
  NS_ENSURE_TRUE(httpChannel, rv);

  PRBool succeeded;
  httpChannel->GetRequestSucceeded(&succeeded);
 
  if (succeeded) {
    rv = mRequest->GetResponseXML(aDocument);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsresult
nsWebScriptsAccess::GetCodebaseURI(nsIURI** aCodebase)
{
  nsresult rv = NS_OK;
 
  if (!mSecurityManager) {
    mSecurityManager = 
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = mSecurityManager->GetSubjectPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);
  
  return principal->GetURI(aCodebase);
}

nsresult 
nsWebScriptsAccess::CreateEntry(const char* aKey,
                                const PRBool aIsDelegated,
                                AccessInfoEntry** aEntry)
{
  NS_ENSURE_ARG_POINTER(aEntry);
  *aEntry = nsnull;
  // create an entry by loading the declaration file (
  // web-scripts-access.xml ) and extracting access information from
  // it. Record the extracted info. for this session
  nsCOMPtr<nsIDOMDocument> document;
  nsresult rv = 
    GetDocument(nsDependentCString(aKey) +
                NS_LITERAL_CSTRING("web-scripts-access.xml"),
                getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);
  if (document) {
    // Create an entry by extracting access information from the document.
    rv = CreateEntry(document, aIsDelegated, aEntry);
    NS_ENSURE_SUCCESS(rv, rv);

    // If the document is invalid then an entry will not be created.
    if (!*aEntry)
      return NS_OK;
  }
  else {
    rv = CreateEntry(WSA_FILE_NOT_FOUND, aEntry);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  nsCStringKey key(aKey);
  mAccessInfoTable.Put(&key, *aEntry);

  NS_ASSERTION(*aEntry, "unexpected: access info entry is null!");
  if (*aEntry  && ((*aEntry)->mFlags & WSA_FILE_DELEGATED))
    rv = CreateDelegatedEntry(aEntry);
  return rv;
}

nsresult 
nsWebScriptsAccess::CreateEntry(nsIDOMDocument* aDocument,
                                const PRBool aIsDelegated,
                                AccessInfoEntry** aEntry)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aEntry);
  *aEntry = nsnull;
  
  PRBool valid;
  nsresult rv = ValidateDocument(aDocument, &valid);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!valid) {
    return NS_OK; // XXX should I return an error instead ?
  }

  if (!aIsDelegated) {
    nsCOMPtr<nsIDOMNodeList> delegateList; 
    rv = aDocument->GetElementsByTagNameNS(kNamespace2002, kDelegateTag, 
                                           getter_AddRefs(delegateList));
    NS_ENSURE_TRUE(delegateList, rv);
    nsCOMPtr<nsIDOMNode> node;
    delegateList->Item(0, getter_AddRefs(node));
    if (node)
      return CreateEntry(WSA_FILE_DELEGATED, aEntry);
  }

  nsCOMPtr<nsIDOMNodeList> allowList;
  rv = aDocument->GetElementsByTagNameNS(kNamespace2002, kAllowTag, 
                                         getter_AddRefs(allowList));
  NS_ENSURE_TRUE(allowList, rv);

  PRUint32 count;
  allowList->GetLength(&count);
  if (count) {
    rv = CreateEntry(allowList, aEntry);
  }
  else {
    // Since there are no ALLOW elements present grant access to all.
    rv = CreateEntry(WSA_GRANT_ACCESS_TO_ALL, aEntry);
  }

  return rv;
}

nsresult
nsWebScriptsAccess::CreateEntry(const PRInt32 aFlags, 
                                AccessInfoEntry** aEntry)
{
  *aEntry = new AccessInfoEntry(aFlags);
  NS_ENSURE_TRUE(*aEntry, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsresult
nsWebScriptsAccess::CreateEntry(nsIDOMNodeList* aAllowList,
                                AccessInfoEntry** aEntry)
{
  NS_ENSURE_ARG_POINTER(aAllowList);
  NS_ENSURE_ARG_POINTER(aEntry);
  *aEntry = nsnull;

  nsAutoPtr<AccessInfoEntry> entry(new AccessInfoEntry());
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 count;
  aAllowList->GetLength(&count);

  PRUint32 index; 
  nsCOMPtr<nsIDOMNode> node;
  nsAutoString type, from;
  for (index = 0; index < count; index++) {
    aAllowList->Item(index, getter_AddRefs(node));
    NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);
     
    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(node));
    element->GetAttribute(kTypeAttr, type);
    element->GetAttribute(kFromAttr, from);

    PRBool found_type = !type.IsEmpty();
    PRBool found_from = !from.IsEmpty();

    if (!found_type && !found_from) {
      // Minor optimization - If the "type" and "from"
      // attributes aren't present then no need to check
      // for attributes in other "allow" elements because
      // access will be granted to all regardless.
      entry->mFlags |= WSA_GRANT_ACCESS_TO_ALL;
      break;
    }

    nsAutoPtr<AccessInfo> access_info(new AccessInfo());
    NS_ENSURE_TRUE(access_info, NS_ERROR_OUT_OF_MEMORY);
    
    if (found_type) {
      access_info->mType = ToNewUnicode(type);
      NS_ENSURE_TRUE(access_info->mType, NS_ERROR_OUT_OF_MEMORY);
    }

    if (found_from) {
      access_info->mFrom = ToNewUnicode(from);
      NS_ENSURE_TRUE(access_info->mFrom, NS_ERROR_OUT_OF_MEMORY);
    }

    entry->mInfoArray.AppendElement(access_info.forget());
    
    type.Truncate();
    from.Truncate();
  }

  *aEntry = entry.forget();

  return NS_OK;
}

nsresult
nsWebScriptsAccess::CreateDelegatedEntry(AccessInfoEntry** aEntry)
{
  NS_ENSURE_ARG_POINTER(aEntry);
  *aEntry = nsnull;
  
  nsresult rv;
  nsCOMPtr<nsIURL> url(do_QueryInterface(mServiceURI, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCAutoString path;
  url->GetPrePath(path);
  nsCAutoString directory;
  url->GetDirectory(directory);
  path += directory;

  return CreateEntry(path.get(), PR_TRUE, aEntry);
}

nsresult
nsWebScriptsAccess::CheckAccess(AccessInfoEntry* aEntry,
                                const nsAString& aRequestType, 
                                PRBool* aAccessGranted)
{
#ifdef DEBUG
  static PRBool verified = PR_FALSE;
  if (!verified) {
    verified = PR_TRUE;
    nsWSAUtils::VerifyIsEqual();
  }
#endif

  *aAccessGranted = PR_FALSE;
  NS_ENSURE_ARG_POINTER(aEntry);

  nsresult rv = NS_OK;
  if (aEntry->mFlags & WSA_FILE_NOT_FOUND) {
    if (aEntry->mFlags & HAS_MASTER_SERVICE_DECISION) {
      if (aEntry->mFlags & SERVICE_LISTED_PUBLIC)
         *aAccessGranted = PR_TRUE;
      return rv;
    }
    nsCAutoString fqdn;
    rv = nsWSAUtils::GetOfficialHostName(mServiceURI, fqdn);
    if (NS_FAILED(rv) || fqdn.IsEmpty())
      return rv;
    
    PRBool isPublic = PR_FALSE;
    rv = IsPublicService(fqdn.get(), &isPublic);
    if (NS_SUCCEEDED(rv)) {
      if (isPublic) {
        aEntry->mFlags |= SERVICE_LISTED_PUBLIC;
        *aAccessGranted = PR_TRUE;
      }
      aEntry->mFlags |= HAS_MASTER_SERVICE_DECISION; 
    }
    return rv;
  }
 
  if (aEntry->mFlags & WSA_GRANT_ACCESS_TO_ALL) {
    *aAccessGranted = PR_TRUE;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> codebase_uri;
  rv = GetCodebaseURI(getter_AddRefs(codebase_uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString tmp;
  codebase_uri->GetSpec(tmp);
  const nsAString& codebase = NS_ConvertUTF8toUCS2(tmp);

  PRUint32 count = aEntry->mInfoArray.Count();
  PRUint32 index;
  for (index = 0; index < count; index++) {
    AccessInfo* access_info = 
      NS_REINTERPRET_CAST(AccessInfo*, aEntry->mInfoArray.ElementAt(index));
    NS_ASSERTION(access_info, "Entry is missing attribute information");
    
    if (!access_info->mType || kAny.Equals(access_info->mType) || 
        aRequestType.Equals(access_info->mType)) {
      if (!access_info->mFrom) {
        // If "from" is not specified, then all scripts will be  allowed 
        *aAccessGranted = PR_TRUE;
        break;
      }
      else {
        if (nsWSAUtils::IsEqual(nsDependentString(access_info->mFrom), 
                                codebase)) {
          *aAccessGranted = PR_TRUE;
          break;
        }
      }
    }
  }

  return NS_OK;
}

/** 
  * Validation is based on the following syntax:
  * 
  * <!ELEMENT webScriptAccess (delegate?|allow*)>
  * <!ELEMENT delegate EMPTY>
  * <!ELEMENT allow EMPTY>
  * <!ATTLIST allow type|from CDATA #IMPLIED>.
  *
  */
nsresult
nsWebScriptsAccess::ValidateDocument(nsIDOMDocument* aDocument,
                                     PRBool* aIsValid)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  *aIsValid = PR_FALSE;
  nsCOMPtr<nsIDOMElement> rootElement;
  aDocument->GetDocumentElement(getter_AddRefs(rootElement));
  
  nsAutoString ns;
  nsAutoString name;
  nsresult rv = rootElement->GetNamespaceURI(ns);
  if (NS_FAILED(rv))
    return rv;
  rootElement->GetLocalName(name);
  if (NS_FAILED(rv))
    return rv;
  
  if (!ns.Equals(kNamespace2002)) {
    const PRUnichar *inputs[1]  = { ns.get() };
    return nsWSAUtils::ReportError(
                         NS_LITERAL_STRING("UnsupportedNamespace").get(), 
                         inputs, 1);
  }
  if (!name.Equals(kWebScriptAccessTag)) {
    const PRUnichar *inputs[1]  = { name.get() };
    return nsWSAUtils::ReportError(
                         NS_LITERAL_STRING("UnknownRootElement").get(), 
                         inputs, 1);
  }

  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));
  NS_ENSURE_TRUE(rootNode, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMNodeList> children;
  rootNode->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_TRUE(children, NS_ERROR_UNEXPECTED);

  PRUint32 length;
  children->GetLength(&length);

  PRBool hadDelegate = PR_FALSE;
  nsCOMPtr<nsIDOMNode> child, attr;
  nsCOMPtr<nsIDOMNamedNodeMap> attrs;
  PRUint32 i;
  for (i = 0; i < length; i++) {
    children->Item(i, getter_AddRefs(child));
    NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);
  
    PRUint16 type;
    child->GetNodeType(&type);

    if (nsIDOMNode::ELEMENT_NODE == type) {
      rv = child->GetNamespaceURI(ns);
      if (NS_FAILED(rv))
        return rv;
      rv = child->GetLocalName(name);
      if (NS_FAILED(rv))
        return rv;
 
      if (!ns.Equals(kNamespace2002))
        continue; // ignore elements with different ns.

      PRBool hasChildNodes = PR_FALSE;
      if (name.Equals(kDelegateTag)) {
        // There can me no more than one delegate element.
        if (hadDelegate) {
          const PRUnichar *inputs[1] = { name.get() };
          return nsWSAUtils::ReportError(
                               NS_LITERAL_STRING("TooManyElements").get(), 
                               inputs, 1);
        }
        // Make sure that the delegate element is EMPTY.
        child->HasChildNodes(&hasChildNodes);
        if (hasChildNodes) {
          const PRUnichar *inputs[1] = { name.get() };
          return nsWSAUtils::ReportError(
                               NS_LITERAL_STRING("ElementNotEmpty").get(), 
                               inputs, 1);
        }
        hadDelegate = PR_TRUE;
      }
      else if (name.Equals(kAllowTag)) {
        // Make sure that the allow element is EMPTY.
        child->HasChildNodes(&hasChildNodes);
        if (hasChildNodes) {
          const PRUnichar *inputs[1] = { name.get() };
          return nsWSAUtils::ReportError(
                               NS_LITERAL_STRING("ElementNotEmpty").get(), 
                               inputs, 1);
        }
        rv = child->GetAttributes(getter_AddRefs(attrs));
        if (NS_FAILED(rv))
          return rv;
        
        PRUint32 count, i;
        attrs->GetLength(&count);
        for (i = 0; i < count; i++) {
          attrs->Item(i, getter_AddRefs(attr));
          if (attr) {
            rv = attr->GetLocalName(name);
            if (NS_FAILED(rv))
              return rv;
            if (!name.Equals(kTypeAttr) && !name.Equals(kFromAttr)) {
              const PRUnichar *inputs[1] = { name.get() };
              return nsWSAUtils::ReportError(
                                   NS_LITERAL_STRING("UnknownAttribute").get(), 
                                   inputs, 1);
            }
          }
        }
      }
      else {
        const PRUnichar *inputs[1] = { name.get() };
        return nsWSAUtils::ReportError(
                             NS_LITERAL_STRING("UnknownElement").get(), 
                             inputs, 1);
      }

    }
  }
  *aIsValid = PR_TRUE;
  return NS_OK;
}

static PRBool
IsCharInSet(const char* aSet,
            const PRUnichar aChar)
{
  PRUnichar ch;
  while ((ch = *aSet)) {
    if (aChar == PRUnichar(ch)) {
      return PR_TRUE;
    }
    ++aSet;
  }
  return PR_FALSE;
}

nsresult
nsWebScriptsAccess::IsPublicService(const char* aHost, PRBool* aReturn)
{
  *aReturn = PR_FALSE;
  nsresult rv = NS_OK;
  // Cache the master services included in the prefs.
  if (mMasterServices.Count() == 0) {
    nsCOMPtr<nsIPrefBranch> prefBranch = 
      do_GetService(NS_PREFSERVICE_CONTRACTID);
  
    if (!prefBranch)
      return rv;

    nsXPIDLCString value;
    rv = prefBranch->GetCharPref("xml.webservice.security.masterservices",
                                 getter_Copies(value));
    
    if (NS_FAILED(rv) || value.IsEmpty())
      return rv;
      
    nsACString::const_iterator begin, end, curr;
    nsACString::const_iterator uri_begin, uri_end;
    value.BeginReading(begin);
    value.EndReading(end);
    
    // Parse the comma separated pref. value
    static const char* kWhitespace = " \n\r\t\b";
    while (begin != end) {
      curr = begin;
      // strip leading whitespaces
      while (IsCharInSet(kWhitespace, *curr) && ++curr != end);
      uri_begin = curr;
      // consume until the delimiter ( comma ).
      while (curr != end && *curr != ',')
        ++curr;
      uri_end = curr;
      // strip trailing whitespaces
      while (uri_end != uri_begin) {
        if (!IsCharInSet(kWhitespace, *(--uri_end))) {
          ++uri_end; // include the last non whitespace char.
          break;
        }
      }
      const nsAFlatString& transportURI =
        NS_ConvertUTF8toUCS2(Substring(uri_begin, uri_end));
      if (!transportURI.IsEmpty())
        mMasterServices.AppendString(transportURI);
      begin = (*curr == ',' && curr != end) ? ++curr : curr;
    }
  }

  // Do nothing if the pref value turns out to be 
  // strings with nothing but whitespaces.
  if (mMasterServices.Count() == 0)
    return rv;

  // Allocate param block.
  nsISOAPParameter** bodyBlocks = 
    NS_STATIC_CAST(nsISOAPParameter**,
                  nsMemory::Alloc(1 * sizeof(nsISOAPParameter*)));
  if (!bodyBlocks)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = 
    CallCreateInstance(NS_SOAPPARAMETER_CONTRACTID, &bodyBlocks[0]);
  
  if (NS_FAILED(rv)) {
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(0, bodyBlocks);
    return rv;
  }

  nsCOMPtr<nsISOAPBlock> block = do_QueryInterface(bodyBlocks[0], &rv);

  if (NS_FAILED(rv))
    return rv;

  block->SetName(NS_LITERAL_STRING("fqdn"));

  nsCOMPtr <nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);

  if (NS_FAILED(rv))
    return rv;

  variant->SetAsString(aHost);

  block->SetValue(variant);

    // Create the call instance
  nsCOMPtr<nsISOAPCall> call = 
    do_CreateInstance(NS_SOAPCALL_CONTRACTID, &rv);
  
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISOAPEncoding> encoding =
    do_CreateInstance(NS_SOAPENCODING_CONTRACTID, &rv);
  
  if (NS_FAILED(rv))
    return rv;

  call->SetEncoding(encoding);
  
  // Since the SOAP request to the central server will be made
  // from the native code we can safely override cross-domain 
  // checks by pushing in a null jscontext on the context stack.
  nsCOMPtr<nsIJSContextStack> stack = 
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  if (stack)
    stack->Push(nsnull);

  nsCOMPtr<nsISOAPResponse> response;
  PRInt32 i, count = mMasterServices.Count();
  for (i = 0; i < count && !response; i++) {
    rv = 
      call->SetTransportURI(*mMasterServices.StringAt(i));
    
    if (NS_FAILED(rv))
      break;
    
    rv = call->Encode(nsISOAPMessage::VERSION_1_1,
                      kIsServicePublic, 
                      kNamespace2002, // The target URI
                      0, 0, 1, bodyBlocks);
    if (NS_FAILED(rv))
      break;
  
    call->Invoke(getter_AddRefs(response)); // XXX - How to handle time out and 404?
  }
  
  if (stack) {
    JSContext* cx;
    stack->Pop(&cx);
    NS_ASSERTION(!cx, "context should be null");
  }

  if (!response)
    return rv;
  
  nsCOMPtr<nsISOAPFault> fault;
  response->GetFault(getter_AddRefs(fault));
    
  if (fault) {
    nsAutoString faultNamespaceURI, faultCode, faultString;
    fault->GetFaultNamespaceURI(faultNamespaceURI);
    fault->GetFaultCode(faultCode);
    fault->GetFaultString(faultString);
    const PRUnichar *inputs[5]  = 
      { 
        kNamespace2002.get(),
        kIsServicePublic.get(),
        faultNamespaceURI.get(),
        faultCode.get(),
        faultString.get()
      };
    return nsWSAUtils::ReportError(
                        NS_LITERAL_STRING("SOAPFault").get(), 
                        inputs, 5);
  }
  else {
    PRUint32 bodyCount;
    rv = response->GetParameters(PR_FALSE, &bodyCount, &bodyBlocks);
    NS_ASSERTION(bodyBlocks, "insufficient information");

    if (!bodyBlocks || NS_FAILED(rv))
      return rv;
    
    NS_ASSERTION(bodyCount == 1, "body seems to contain unnecessary information.");
    block = do_QueryInterface(bodyBlocks[0], &rv);
    
    if (NS_FAILED(rv))
      return rv;
    
    nsCOMPtr<nsIVariant> value;
    rv = block->GetValue(getter_AddRefs(value));
    
    if (NS_FAILED(rv) || !value)
      return rv;
    
    rv = value->GetAsBool(aReturn);
  }

  return rv;
}


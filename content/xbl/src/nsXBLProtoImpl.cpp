/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXBLProtoImpl.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsIServiceManager.h"
#include "nsIXBLDocumentInfo.h"

nsresult
nsXBLProtoImpl::InstallImplementation(nsIXBLPrototypeBinding* aBinding, nsIContent* aBoundElement)
{
  // This function is called to install a concrete implementation on a bound element using
  // this prototype implementation as a guide.  The prototype implementation is compiled lazily,
  // so for the first bound element that needs a concrete implementation, we also build the
  // prototype implementation.
  if (!mMembers)
    return NS_OK; // Nothing to do, so let's not waste time.

  nsCOMPtr<nsIDocument> document;
  aBoundElement->GetDocument(*getter_AddRefs(document));
  if (!document) return NS_OK;

  nsCOMPtr<nsIScriptGlobalObject> global;
  document->GetScriptGlobalObject(getter_AddRefs(global));
  if (!global) return NS_OK;

  nsCOMPtr<nsIScriptContext> context;
  nsresult rv = global->GetContext(getter_AddRefs(context));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!context) return NS_OK;

  // InitTarget objects gives us back the JS object that represents the bound element and the
  // class object in the bound document that represents the concrete version of this implementation.
  // This function also has the side effect of building up the prototype implementation if it has
  // not been built already.
  void * targetScriptObject = nsnull;
  void * targetClassObject = nsnull;
  rv = InitTargetObjects(aBinding, context, aBoundElement, &targetScriptObject, &targetClassObject);
  NS_ENSURE_SUCCESS(rv, rv); // kick out if we were unable to properly intialize our target objects

  // Walk our member list and install each one in turn.
  for (nsXBLProtoImplMember* curr = mMembers;
       curr;
       curr = curr->GetNext())
    curr->InstallMember(context, aBoundElement, targetScriptObject, targetClassObject);
  return NS_OK;
}

nsresult 
nsXBLProtoImpl::InitTargetObjects(nsIXBLPrototypeBinding* aBinding,
                                  nsIScriptContext* aContext, 
                                  nsIContent* aBoundElement, 
                                  void** aScriptObject, 
                                  void** aTargetClassObject)
{
  if (!mClassObject)
    CompilePrototypeMembers(aBinding); // This is the first time we've ever installed this binding on an element.
                                 // We need to go ahead and compile all methods and properties on a class
                                 // in our prototype binding.
  
  if (!mClassObject)
    return NS_OK; // This can be ok, if all we've got are fields (and no methods/properties).

  // Because our prototype implementation has a class, we need to build up a corresponding
  // class for the concrete implementation in the bound document.
  nsresult rv = NS_OK;
  JSContext* jscontext = (JSContext*)aContext->GetNativeContext();
  JSObject* global = ::JS_GetGlobalObject(jscontext);
  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = xpc->WrapNative(jscontext, global, aBoundElement,
                       NS_GET_IID(nsISupports), getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);
  JSObject * object = nsnull;
  rv = wrapper->GetJSObject(&object);
  NS_ENSURE_SUCCESS(rv, rv);

  // All of the above code was just obtaining the bound element's script object and its immediate
  // concrete base class.  We need to alter the object so that our concrete class is interposed
  // between the object and its base class.  We become the new base class of the object, and the
  // object's old base class becomes the new class' base class.
  *aScriptObject = object;
  aBinding->InitClass(mClassName, aContext, (void *) object, aTargetClassObject);

  // Root ourselves in the document.
  nsCOMPtr<nsIDocument> doc;
  aBoundElement->GetDocument(*getter_AddRefs(doc));
  if (doc) {
    nsCOMPtr<nsIXPConnectWrappedNative> nativeWrapper(do_QueryInterface(wrapper));
    if (nativeWrapper)
      doc->AddReference(aBoundElement, nativeWrapper);
  }
  
  return rv;
}

nsresult
nsXBLProtoImpl::CompilePrototypeMembers(nsIXBLPrototypeBinding* aBinding)
{
  // We want to pre-compile our implementation's members against a "prototype context". Then when we actually 
  // bind the prototype to a real xbl instance, we'll clone the pre-compiled JS into the real instance's 
  // context.
  nsCOMPtr<nsIXBLDocumentInfo> docInfo;
  aBinding->GetXBLDocumentInfo(nsnull, getter_AddRefs(docInfo));
  if (!docInfo)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner(do_QueryInterface(docInfo));
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  globalOwner->GetScriptGlobalObject(getter_AddRefs(globalObject));

  nsCOMPtr<nsIScriptContext> context;
  globalObject->GetContext(getter_AddRefs(context));
 
  void* classObject;
  JSObject* scopeObject = globalObject->GetGlobalJSObject();
  aBinding->InitClass(mClassName, context, scopeObject, &classObject);
  mClassObject = (JSObject*) classObject;
  if (!mClassObject)
    return NS_ERROR_FAILURE;

  // Now that we have a class object installed, we walk our member list and compile each of our
  // properties and methods in turn.
  for (nsXBLProtoImplMember* curr = mMembers;
       curr;
       curr = curr->GetNext()) {
    curr->CompileMember(context, mClassName, mClassObject);
  }
  return NS_OK;
}
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
nsXBLProtoImpl::InstallImplementation(nsXBLPrototypeBinding* aBinding, nsIContent* aBoundElement)
{
  // This function is called to install a concrete implementation on a bound element using
  // this prototype implementation as a guide.  The prototype implementation is compiled lazily,
  // so for the first bound element that needs a concrete implementation, we also build the
  // prototype implementation.
  if (!mMembers)
    return NS_OK; // Nothing to do, so let's not waste time.

  nsIDocument* document = aBoundElement->GetDocument();
  if (!document) return NS_OK;

  nsIScriptGlobalObject *global = document->GetScriptGlobalObject();
  if (!global) return NS_OK;

  nsCOMPtr<nsIScriptContext> context = global->GetContext();
  if (!context) return NS_OK;

  // InitTarget objects gives us back the JS object that represents the bound element and the
  // class object in the bound document that represents the concrete version of this implementation.
  // This function also has the side effect of building up the prototype implementation if it has
  // not been built already.
  void * targetScriptObject = nsnull;
  void * targetClassObject = nsnull;
  nsresult rv = InitTargetObjects(aBinding, context, aBoundElement,
                                  &targetScriptObject, &targetClassObject);
  NS_ENSURE_SUCCESS(rv, rv); // kick out if we were unable to properly intialize our target objects

  // Walk our member list and install each one in turn.
  for (nsXBLProtoImplMember* curr = mMembers;
       curr;
       curr = curr->GetNext())
    curr->InstallMember(context, aBoundElement, targetScriptObject,
                        targetClassObject, mClassName);
  return NS_OK;
}

nsresult 
nsXBLProtoImpl::InitTargetObjects(nsXBLPrototypeBinding* aBinding,
                                  nsIScriptContext* aContext, 
                                  nsIContent* aBoundElement, 
                                  void** aScriptObject, 
                                  void** aTargetClassObject)
{
  nsresult rv = NS_OK;
  if (!mClassObject) {
    rv = CompilePrototypeMembers(aBinding); // This is the first time we've ever installed this binding on an element.
                                 // We need to go ahead and compile all methods and properties on a class
                                 // in our prototype binding.
    if (NS_FAILED(rv))
      return rv;

    if (!mClassObject)
      return NS_OK; // This can be ok, if all we've got are fields (and no methods/properties).
  }

  // Because our prototype implementation has a class, we need to build up a corresponding
  // class for the concrete implementation in the bound document.
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
  rv = aBinding->InitClass(mClassName, aContext, (void *) object, aTargetClassObject);
  if (NS_FAILED(rv))
    return rv;

  // Root ourselves in the document.
  nsIDocument* doc = aBoundElement->GetDocument();
  if (doc) {
    nsCOMPtr<nsIXPConnectWrappedNative> nativeWrapper(do_QueryInterface(wrapper));
    if (nativeWrapper)
      doc->AddReference(aBoundElement, nativeWrapper);
  }
  
  return rv;
}

nsresult
nsXBLProtoImpl::CompilePrototypeMembers(nsXBLPrototypeBinding* aBinding)
{
  // We want to pre-compile our implementation's members against a "prototype context". Then when we actually 
  // bind the prototype to a real xbl instance, we'll clone the pre-compiled JS into the real instance's 
  // context.
  nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner(
      do_QueryInterface(aBinding->XBLDocumentInfo()));
  nsIScriptGlobalObject* globalObject = globalOwner->GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, NS_ERROR_UNEXPECTED);

  nsIScriptContext *context = globalObject->GetContext();

  void* classObject;
  JSObject* scopeObject = globalObject->GetGlobalJSObject();
  nsresult rv = aBinding->InitClass(mClassName, context, scopeObject, &classObject);
  if (NS_FAILED(rv))
    return rv;

  mClassObject = (JSObject*) classObject;
  if (!mClassObject)
    return NS_ERROR_FAILURE;

  // Now that we have a class object installed, we walk our member list and compile each of our
  // properties and methods in turn.
  for (nsXBLProtoImplMember* curr = mMembers;
       curr;
       curr = curr->GetNext()) {
    nsresult rv = curr->CompileMember(context, mClassName, mClassObject);
    if (NS_FAILED(rv)) {
      DestroyMembers(curr);
      return rv;
    }
  }
  return NS_OK;
}

void
nsXBLProtoImpl::DestroyMembers(nsXBLProtoImplMember* aBrokenMember)
{
  NS_ASSERTION(mClassObject, "This should never be called when there is no class object");
  PRBool compiled = PR_TRUE;
  for (nsXBLProtoImplMember* curr = mMembers; curr; curr = curr->GetNext()) {
    if (curr == aBrokenMember) {
      compiled = PR_FALSE;
    }
    curr->Destroy(compiled);
  }
}

nsresult
NS_NewXBLProtoImpl(nsXBLPrototypeBinding* aBinding, 
                   const PRUnichar* aClassName, 
                   nsXBLProtoImpl** aResult)
{
  nsXBLProtoImpl* impl = new nsXBLProtoImpl();
  if (!impl)
    return NS_ERROR_OUT_OF_MEMORY;
  if (aClassName)
    impl->mClassName.AssignWithConversion(aClassName);
  else
    aBinding->BindingURI()->GetSpec(impl->mClassName);
  aBinding->SetImplementation(impl);
  *aResult = impl;

  return NS_OK;
}


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
 * Original Author: Scott MacGregor (mscott@netscape.com)
 *
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

#include "nsCOMPtr.h"
#include "nsXBLPrototypeProperty.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsXULAtoms.h"
#include "nsIXPConnect.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsINameSpaceManager.h"
#include "nsXBLService.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIDOMText.h"

PRUint32 nsXBLPrototypeProperty::gRefCnt = 0;

nsIAtom* nsXBLPrototypeProperty::kMethodAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kParameterAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kBodyAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kPropertyAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kFieldAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kOnSetAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kOnGetAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kGetterAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kSetterAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kNameAtom = nsnull;
nsIAtom* nsXBLPrototypeProperty::kReadOnlyAtom = nsnull;

#include "nsIJSRuntimeService.h"
static nsIJSRuntimeService* gJSRuntimeService = nsnull;
static JSRuntime* gScriptRuntime = nsnull;
static PRInt32 gScriptRuntimeRefcnt = 0;

static nsresult
AddJSGCRoot(void* aScriptObjectRef, const char* aName)
{
    if (++gScriptRuntimeRefcnt == 1 || !gScriptRuntime) {
        CallGetService("@mozilla.org/js/xpc/RuntimeService;1",
                       &gJSRuntimeService);
        if (! gJSRuntimeService) {
            NS_NOTREACHED("couldn't add GC root");
            return NS_ERROR_FAILURE;
        }

        gJSRuntimeService->GetRuntime(&gScriptRuntime);
        if (! gScriptRuntime) {
            NS_NOTREACHED("couldn't add GC root");
            return NS_ERROR_FAILURE;
        }
    }

    PRBool ok;
    ok = ::JS_AddNamedRootRT(gScriptRuntime, aScriptObjectRef, aName);
    if (! ok) {
        NS_NOTREACHED("couldn't add GC root");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

static nsresult
RemoveJSGCRoot(void* aScriptObjectRef)
{
    if (! gScriptRuntime) {
        NS_NOTREACHED("couldn't remove GC root");
        return NS_ERROR_FAILURE;
    }

    ::JS_RemoveRootRT(gScriptRuntime, aScriptObjectRef);

    if (--gScriptRuntimeRefcnt == 0) {
        NS_RELEASE(gJSRuntimeService);
        gScriptRuntime = nsnull;
    }

    return NS_OK;
}

nsXBLPrototypeProperty::nsXBLPrototypeProperty(nsIXBLPrototypeBinding * aPrototypeBinding)
: mJSMethodObject(nsnull), mJSGetterObject(nsnull), mJSSetterObject(nsnull), mPropertyIsCompiled(PR_FALSE)
{
  NS_INIT_REFCNT();
  mClassObject = nsnull;
  gRefCnt++;
  if (gRefCnt == 1) 
  {
    kMethodAtom = NS_NewAtom("method");
    kParameterAtom = NS_NewAtom("parameter");
    kBodyAtom = NS_NewAtom("body");
    kPropertyAtom = NS_NewAtom("property");
    kFieldAtom = NS_NewAtom("field");
    kOnSetAtom = NS_NewAtom("onset");
    kOnGetAtom = NS_NewAtom("onget");
    kGetterAtom = NS_NewAtom("getter");
    kSetterAtom = NS_NewAtom("setter");    
    kNameAtom = NS_NewAtom("name");
    kReadOnlyAtom = NS_NewAtom("readonly");

  }

  mPrototypeBinding = do_GetWeakReference(aPrototypeBinding);
}

nsXBLPrototypeProperty::~nsXBLPrototypeProperty()
{
  if (mJSMethodObject)
    RemoveJSGCRoot(&mJSMethodObject);
  if (mJSGetterObject)
    RemoveJSGCRoot(&mJSGetterObject);
  if (mJSSetterObject)
    RemoveJSGCRoot(&mJSSetterObject);

  gRefCnt--;
  if (gRefCnt == 0) 
  {
    NS_RELEASE(kMethodAtom);
    NS_RELEASE(kParameterAtom);
    NS_RELEASE(kBodyAtom);
    NS_RELEASE(kPropertyAtom); 
    NS_RELEASE(kFieldAtom); 
    NS_RELEASE(kOnSetAtom);
    NS_RELEASE(kOnGetAtom);
    NS_RELEASE(kGetterAtom);
    NS_RELEASE(kSetterAtom);
    NS_RELEASE(kNameAtom);
    NS_RELEASE(kReadOnlyAtom);
  }
}

NS_IMPL_ISUPPORTS1(nsXBLPrototypeProperty, nsIXBLPrototypeProperty)

NS_IMETHODIMP
nsXBLPrototypeProperty::GetNextProperty(nsIXBLPrototypeProperty** aResult)
{
  *aResult = mNextProperty;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}
  
NS_IMETHODIMP
nsXBLPrototypeProperty::SetNextProperty(nsIXBLPrototypeProperty* aProperty)
{
  mNextProperty = aProperty;
  return NS_OK;
}

// Assumes the class object has already been initialized!!
NS_IMETHODIMP 
nsXBLPrototypeProperty::InitTargetObjects(nsIScriptContext * aContext, nsIContent * aBoundElement, void ** aScriptObject, void ** aTargetClassObject)
{
  if (!mClassObject)
  {
    DelayedPropertyConstruction();
  }

  NS_ENSURE_TRUE(mClassObject, NS_ERROR_FAILURE);
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

  *aScriptObject = object;
  nsCOMPtr<nsIXBLPrototypeBinding> binding = do_QueryReferent(mPrototypeBinding);
  if (binding)
  {
    binding->InitClass(mClassStr, aContext, (void *) object, aTargetClassObject);

    // Root mBoundElement so that it doesn't loose it's binding
    nsCOMPtr<nsIDocument> doc;
    aBoundElement->GetDocument(*getter_AddRefs(doc));

    if (doc) 
    {
      nsCOMPtr<nsIXPConnectWrappedNative> native_wrapper = do_QueryInterface(wrapper);
      if (native_wrapper)
       doc->AddReference(aBoundElement, native_wrapper);
    }
  } // if binding

  return rv;
}

NS_IMETHODIMP
nsXBLPrototypeProperty::InstallProperty(nsIScriptContext * aContext, nsIContent * aBoundElement, void * aScriptObject, void * aTargetClassObject)
{
  if (!mPropertyIsCompiled)
  {
    DelayedPropertyConstruction();
  }

  JSContext* cx = (JSContext*) aContext->GetNativeContext();
  JSObject * scriptObject = (JSObject *) aScriptObject;
  NS_ASSERTION(scriptObject, "uhoh, script Object should NOT be null or bad things will happen");

  JSObject * targetClassObject = (JSObject *) aTargetClassObject;
  JSObject * globalObject = ::JS_GetGlobalObject(cx);

  // now we want to re-evaluate our property using aContext and the script object for this window...

  if (mJSMethodObject && targetClassObject)
  {
    JSObject * method = ::JS_CloneFunctionObject(cx, mJSMethodObject, globalObject);
    ::JS_DefineUCProperty(cx, targetClassObject, NS_REINTERPRET_CAST(const jschar*, mName.get()),  mName.Length(), OBJECT_TO_JSVAL(method),
                          NULL, NULL, JSPROP_ENUMERATE);


  }
  else if ((mJSGetterObject || mJSSetterObject) && targetClassObject)
  {
    // Having either a getter or setter results in the
    // destruction of any initial value that might be set.
    // This means we only have to worry about defining the getter
    // or setter.
    
    JSObject * getter = nsnull;
    if (mJSGetterObject)
      getter = ::JS_CloneFunctionObject(cx, mJSGetterObject, globalObject);
    
    JSObject * setter = nsnull;
    if (mJSSetterObject)
      setter = ::JS_CloneFunctionObject(cx, mJSSetterObject, globalObject);

    ::JS_DefineUCProperty(cx, targetClassObject, NS_REINTERPRET_CAST(const jschar*, mName.get()), 
                          mName.Length(), JSVAL_VOID,  (JSPropertyOp) getter, 
                          (JSPropertyOp) setter, mJSAttributes); 
  }
  else if (!mFieldString.IsEmpty())
  {
    // compile the literal string 
    jsval result = nsnull;
    PRBool undefined;
    aContext->EvaluateStringWithValue(mFieldString, 
                                      scriptObject,
                                      nsnull, nsnull, 0, nsnull,
                                      (void*) &result, &undefined);
              
    if (!undefined) 
    {
      // Define that value as a property
     ::JS_DefineUCProperty(cx, scriptObject, NS_REINTERPRET_CAST(const jschar*, mName.get()), 
                           mName.Length(), result,nsnull, nsnull, mJSAttributes); 
    }
  }

  return NS_OK;
}

nsresult nsXBLPrototypeProperty::DelayedPropertyConstruction()
{
  // FIRST: get the name attribute on the property!!! need to pass in the property tag 
  // into here in order to do that.

  // See if we're a property or a method.
  nsCOMPtr<nsIAtom> tagName;
  mPropertyElement->GetTag(*getter_AddRefs(tagName));

  // We want to pre-compile the properties against a "special context". Then when we actual bind
  // the proto type to a real xbl instance, we'll resolve the pre-compiled JS against the file context.
  // right now this special context is attached to the xbl document info....

  nsCOMPtr<nsIXBLDocumentInfo> docInfo;
  nsCOMPtr<nsIXBLPrototypeBinding> binding = do_QueryReferent(mPrototypeBinding);
  NS_ENSURE_TRUE(binding, NS_ERROR_FAILURE);

  binding->GetXBLDocumentInfo(nsnull, getter_AddRefs(docInfo));
  NS_ENSURE_TRUE(docInfo, NS_ERROR_FAILURE);

  nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner (do_QueryInterface(docInfo));
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  globalOwner->GetScriptGlobalObject(getter_AddRefs(globalObject));

  nsCOMPtr<nsIScriptContext> context;
  globalObject->GetContext(getter_AddRefs(context));
 
  void * classObject;
  JSObject * scopeObject = globalObject->GetGlobalJSObject();
  binding->GetCompiledClassObject(mClassStr, context, (void *) scopeObject, &classObject);
  mClassObject = (JSObject *) classObject;

  if (tagName.get() == kMethodAtom) 
    ParseMethod(context);
  else if (tagName.get() == kPropertyAtom) 
    ParseProperty(context);
  else if (tagName.get() == kFieldAtom)
    ParseField(context);

  mPropertyIsCompiled = PR_TRUE;
  mInterfaceElement = nsnull;
  mPropertyElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeProperty::ConstructProperty(nsIContent * aInterfaceElement, nsIContent* aPropertyElement)
{
  NS_ENSURE_ARG(aPropertyElement);
  NS_ENSURE_ARG(aInterfaceElement);

  mInterfaceElement = aInterfaceElement;
  mPropertyElement = aPropertyElement;

  // Init our class and insert it into the prototype chain.
  nsAutoString className;
  mInterfaceElement->GetAttr(kNameSpaceID_None, kNameAtom, className);

  if (!className.IsEmpty()) {
    mClassStr.AssignWithConversion(className);
  }
  else {
    nsCOMPtr<nsIXBLPrototypeBinding> binding = do_QueryReferent(mPrototypeBinding);
    NS_ENSURE_TRUE(binding, NS_ERROR_FAILURE);
    binding->GetBindingURI(mClassStr);
  }

  return NS_OK;
}

const char* gPropertyArgs[] = { "val" };

nsresult nsXBLPrototypeProperty::ParseProperty(nsIScriptContext * aContext)
{
  // Obtain our name attribute.
  nsresult rv = NS_OK;
  mPropertyElement->GetAttr(kNameSpaceID_None, kNameAtom, mName);

  if (!mName.IsEmpty()) 
  {
    // We have a property.
    nsAutoString getter, setter, readOnly;
    mPropertyElement->GetAttr(kNameSpaceID_None, kOnGetAtom, getter);
    mPropertyElement->GetAttr(kNameSpaceID_None, kOnSetAtom, setter);
    mPropertyElement->GetAttr(kNameSpaceID_None, kReadOnlyAtom, readOnly);

    mJSAttributes = JSPROP_ENUMERATE;

    if (readOnly == NS_LITERAL_STRING("true"))
      mJSAttributes |= JSPROP_READONLY;

    // try for first <getter> tag
    if (getter.IsEmpty()) 
    {
      PRInt32 childCount;
      mPropertyElement->ChildCount(childCount);

      nsCOMPtr<nsIContent> getterElement;
      for (PRInt32 j=0; j<childCount; j++) 
      {
        mPropertyElement->ChildAt(j, *getter_AddRefs(getterElement));
        
        if (!getterElement) continue;
        
        nsCOMPtr<nsIAtom> getterTag;
        getterElement->GetTag(*getter_AddRefs(getterTag));
        
        if (getterTag.get() == kGetterAtom) 
        {
          GetTextData(getterElement, getter);
          break;          // stop at first tag
        }
      } // for each childCount
    } // if getter is empty
          
    
    if (!getter.IsEmpty() && mClassObject) 
    {
      nsCAutoString functionUri;
      functionUri.Assign(mClassStr);
      functionUri += ".";
      functionUri.AppendWithConversion(mName.get());
      functionUri += " (getter)";
      rv = aContext->CompileFunction(mClassObject,
                                    nsCAutoString("onget"),
                                    0,
                                    nsnull,
                                    getter, 
                                    functionUri.get(),
                                    0,
                                    PR_FALSE,
                                    (void **) &mJSGetterObject);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      mJSAttributes |= JSPROP_GETTER | JSPROP_SHARED;
      if (mJSGetterObject) 
      {
        // Root the compiled prototype script object.
        JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                            aContext->GetNativeContext());
        if (!cx) return NS_ERROR_UNEXPECTED;

        rv = AddJSGCRoot(&mJSGetterObject, "nsXBLPrototypeProperty::mJSGetterObject");
        if (NS_FAILED(rv)) return rv;
      }
    } // if getter is not empty
  
    // try for first <setter> tag
    if (setter.IsEmpty()) 
    {
      PRInt32 childCount;
      mPropertyElement->ChildCount(childCount);

      nsCOMPtr<nsIContent> setterElement;
      for (PRInt32 j=0; j<childCount; j++) 
      {
        mPropertyElement->ChildAt(j, *getter_AddRefs(setterElement));
        
        if (!setterElement) continue;
        
        nsCOMPtr<nsIAtom> setterTag;
        setterElement->GetTag(*getter_AddRefs(setterTag));
        if (setterTag.get() == kSetterAtom) 
        {
          GetTextData(setterElement, setter);
          break;          // stop at first tag
        }
      }
    } // if setter is empty
          
    if (!setter.IsEmpty() && mClassObject) 
    {
      nsCAutoString functionUri (mClassStr);
      functionUri += ".";
      functionUri.AppendWithConversion(mName.get());
      functionUri += " (setter)";
      rv = aContext->CompileFunction(mClassObject,
                                    nsCAutoString("onset"),
                                    1,
                                    gPropertyArgs,
                                    setter, 
                                    functionUri.get(),
                                    0,
                                    PR_FALSE,
                                    (void **) &mJSSetterObject);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      mJSAttributes |= JSPROP_SETTER | JSPROP_SHARED;
      if (mJSSetterObject) 
      {
        // Root the compiled prototype script object.
        JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                            aContext->GetNativeContext());
        if (!cx) return NS_ERROR_UNEXPECTED;

        rv = AddJSGCRoot(&mJSSetterObject, "nsXBLPrototypeProperty::mJSSetterObject");
        if (NS_FAILED(rv)) return rv;
      }
    } // if setter wasn't empty....

    // if we came through all of this without a getter or setter, look for a raw
    // literal property string...
    if (mJSSetterObject || mJSGetterObject) 
      return NS_OK;
    
    return ParseField(aContext);
  } // if name isn't empty

  return rv;
}

nsresult nsXBLPrototypeProperty::ParseField(nsIScriptContext * aContext)
{
  nsresult rv = NS_OK;
  mPropertyElement->GetAttr(kNameSpaceID_None, kNameAtom, mName);
  if (mName.IsEmpty())
    return NS_OK;

  nsAutoString readOnly;
  mPropertyElement->GetAttr(kNameSpaceID_None, kReadOnlyAtom, readOnly);
  mJSAttributes = JSPROP_ENUMERATE;
  if (readOnly == NS_LITERAL_STRING("true"))
    mJSAttributes |= JSPROP_READONLY; // Fields can be read-only.

  // Look for a normal value and just define that.
  nsCOMPtr<nsIContent> textChild;
  PRInt32 textCount;
  mPropertyElement->ChildCount(textCount);
  for (PRInt32 j = 0; j < textCount; j++) 
  {
    // Get the child.
    mPropertyElement->ChildAt(j, *getter_AddRefs(textChild));
    nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
    if (text)
    {
      nsAutoString data;
      text->GetData(data);
      mFieldString += data;
    }
  } // for each element

  return NS_OK;
}

nsresult nsXBLPrototypeProperty::ParseMethod(nsIScriptContext * aContext)
{
  // TO DO: fix up class name and class object

  // Obtain our name attribute.
  nsAutoString body;
  nsresult rv = NS_OK;
  mPropertyElement->GetAttr(kNameSpaceID_None, kNameAtom, mName);

  // Now walk all of our args.
  // XXX I'm lame. 32 max args allowed.
  char* args[32];
  PRUint32 argCount = 0;
  PRInt32 kidCount;
  mPropertyElement->ChildCount(kidCount);
  for (PRInt32 j = 0; j < kidCount; j++)
  {
    nsCOMPtr<nsIContent> arg;
    mPropertyElement->ChildAt(j, *getter_AddRefs(arg));
    nsCOMPtr<nsIAtom> kidTagName;
    arg->GetTag(*getter_AddRefs(kidTagName));
    
    if (kidTagName.get() == kParameterAtom) 
    {
      // Get the argname and add it to the array.
      nsAutoString argName;
      arg->GetAttr(kNameSpaceID_None, kNameAtom, argName);
      char* argStr = argName.ToNewCString();
      args[argCount] = argStr;
      argCount++;
    }
    else if (kidTagName.get() == kBodyAtom) 
    {
      PRInt32 textCount;
      arg->ChildCount(textCount);
    
      for (PRInt32 k = 0; k < textCount; k++) 
      {
        // Get the child.
        nsCOMPtr<nsIContent> textChild;
        arg->ChildAt(k, *getter_AddRefs(textChild));
        nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
        if (text) 
        {
          nsAutoString data;
          text->GetData(data);
          body += data;
        }
      } // for each body line
    } // if we have a body atom
  } // for each node in the method

  // Now that we have a body and args, compile the function
  // and then define it as a property.....
  if (!body.IsEmpty()) 
  {
    nsCAutoString cname; cname.AssignWithConversion(mName.get());
    nsCAutoString functionUri (mClassStr);
    functionUri += ".";
    functionUri += cname;
    functionUri += "()";
    
    rv = aContext->CompileFunction(mClassObject,
                                  cname,
                                  argCount,
                                  (const char**)args,
                                  body, 
                                  functionUri.get(),
                                  0,
                                  PR_FALSE,
                                  (void **) &mJSMethodObject);

    if (mJSMethodObject) 
    {
      // Root the compiled prototype script object.
      JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                          aContext->GetNativeContext());
      if (!cx) return NS_ERROR_UNEXPECTED;

      rv = AddJSGCRoot(&mJSMethodObject, "nsXBLPrototypeProperty::mJSMethod");
    }
  }
  
  for (PRUint32 l = 0; l < argCount; l++) 
    nsMemory::Free(args[l]);

  return rv;
}

nsresult
nsXBLPrototypeProperty::GetTextData(nsIContent *aParent, nsString& aResult)
{
  aResult.Truncate(0);

  nsCOMPtr<nsIContent> textChild;
  PRInt32 textCount;
  aParent->ChildCount(textCount);
  nsAutoString answer;
  for (PRInt32 j = 0; j < textCount; j++) {
    // Get the child.
    aParent->ChildAt(j, *getter_AddRefs(textChild));
    nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
    if (text) 
    {
      nsAutoString data;
      text->GetData(data);
      aResult += data;
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLPrototypeProperty(nsIXBLPrototypeBinding * aPrototypeBinding, nsIXBLPrototypeProperty ** aResult)
{
  *aResult = new nsXBLPrototypeProperty(aPrototypeBinding);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

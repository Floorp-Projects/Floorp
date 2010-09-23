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

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsString.h"
#include "mozilla/FunctionTimer.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsXBLProtoImplMethod.h"
#include "nsIScriptContext.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"

nsXBLProtoImplMethod::nsXBLProtoImplMethod(const PRUnichar* aName) :
  nsXBLProtoImplMember(aName), 
  mUncompiledMethod(BIT_UNCOMPILED)
{
  MOZ_COUNT_CTOR(nsXBLProtoImplMethod);
}

nsXBLProtoImplMethod::~nsXBLProtoImplMethod()
{
  MOZ_COUNT_DTOR(nsXBLProtoImplMethod);

  if (!IsCompiled()) {
    delete GetUncompiledMethod();
  }
}

void 
nsXBLProtoImplMethod::AppendBodyText(const nsAString& aText)
{
  NS_PRECONDITION(!IsCompiled(),
                  "Must not be compiled when accessing uncompiled method");

  nsXBLUncompiledMethod* uncompiledMethod = GetUncompiledMethod();
  if (!uncompiledMethod) {
    uncompiledMethod = new nsXBLUncompiledMethod();
    if (!uncompiledMethod)
      return;
    SetUncompiledMethod(uncompiledMethod);
  }

  uncompiledMethod->AppendBodyText(aText);
}

void 
nsXBLProtoImplMethod::AddParameter(const nsAString& aText)
{
  NS_PRECONDITION(!IsCompiled(),
                  "Must not be compiled when accessing uncompiled method");

  nsXBLUncompiledMethod* uncompiledMethod = GetUncompiledMethod();
  if (!uncompiledMethod) {
    uncompiledMethod = new nsXBLUncompiledMethod();
    if (!uncompiledMethod)
      return;
    SetUncompiledMethod(uncompiledMethod);
  }

  uncompiledMethod->AddParameter(aText);
}

void
nsXBLProtoImplMethod::SetLineNumber(PRUint32 aLineNumber)
{
  NS_PRECONDITION(!IsCompiled(),
                  "Must not be compiled when accessing uncompiled method");

  nsXBLUncompiledMethod* uncompiledMethod = GetUncompiledMethod();
  if (!uncompiledMethod) {
    uncompiledMethod = new nsXBLUncompiledMethod();
    if (!uncompiledMethod)
      return;
    SetUncompiledMethod(uncompiledMethod);
  }

  uncompiledMethod->SetLineNumber(aLineNumber);
}

nsresult
nsXBLProtoImplMethod::InstallMember(nsIScriptContext* aContext,
                                    nsIContent* aBoundElement, 
                                    void* aScriptObject,
                                    void* aTargetClassObject,
                                    const nsCString& aClassStr)
{
  NS_PRECONDITION(IsCompiled(),
                  "Should not be installing an uncompiled method");
  JSContext* cx = (JSContext*) aContext->GetNativeContext();

  nsIDocument *ownerDoc = aBoundElement->GetOwnerDoc();
  nsIScriptGlobalObject *sgo;

  if (!ownerDoc || !(sgo = ownerDoc->GetScopeObject())) {
    return NS_ERROR_UNEXPECTED;
  }

  JSObject * scriptObject = (JSObject *) aScriptObject;
  NS_ASSERTION(scriptObject, "uh-oh, script Object should NOT be null or bad things will happen");
  if (!scriptObject)
    return NS_ERROR_FAILURE;

  JSObject * targetClassObject = (JSObject *) aTargetClassObject;
  JSObject * globalObject = sgo->GetGlobalJSObject();

  // now we want to reevaluate our property using aContext and the script object for this window...
  if (mJSMethodObject && targetClassObject) {
    nsDependentString name(mName);
    JSAutoRequest ar(cx);
    JSAutoEnterCompartment ac;

    if (!ac.enter(cx, mJSMethodObject)) {
      return NS_ERROR_UNEXPECTED;
    }

    JSObject * method = ::JS_CloneFunctionObject(cx, mJSMethodObject, globalObject);
    if (!method) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv;
    nsAutoGCRoot root(&method, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (!::JS_DefineUCProperty(cx, targetClassObject,
                               reinterpret_cast<const jschar*>(mName), 
                               name.Length(), OBJECT_TO_JSVAL(method),
                               NULL, NULL, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

nsresult 
nsXBLProtoImplMethod::CompileMember(nsIScriptContext* aContext, const nsCString& aClassStr,
                                    void* aClassObject)
{
  NS_TIME_FUNCTION_MIN(5);
  NS_PRECONDITION(!IsCompiled(),
                  "Trying to compile an already-compiled method");
  NS_PRECONDITION(aClassObject,
                  "Must have class object to compile");

  nsXBLUncompiledMethod* uncompiledMethod = GetUncompiledMethod();

  // No parameters or body was supplied, so don't install method.
  if (!uncompiledMethod) {
    // Early return after which we consider ourselves compiled.
    mJSMethodObject = nsnull;

    return NS_OK;
  }

  // Don't install method if no name was supplied.
  if (!mName) {
    delete uncompiledMethod;

    // Early return after which we consider ourselves compiled.
    mJSMethodObject = nsnull;

    return NS_OK;
  }

  // We have a method.
  // Allocate an array for our arguments.
  PRInt32 paramCount = uncompiledMethod->GetParameterCount();
  char** args = nsnull;
  if (paramCount > 0) {
    args = new char*[paramCount];
    if (!args)
      return NS_ERROR_OUT_OF_MEMORY;

    // Add our parameters to our args array.
    PRInt32 argPos = 0; 
    for (nsXBLParameter* curr = uncompiledMethod->mParameters; 
         curr; 
         curr = curr->mNext) {
      args[argPos] = curr->mName;
      argPos++;
    }
  }

  // Get the body
  nsDependentString body;
  PRUnichar *bodyText = uncompiledMethod->mBodyText.GetText();
  if (bodyText)
    body.Rebind(bodyText);

  // Now that we have a body and args, compile the function
  // and then define it.
  NS_ConvertUTF16toUTF8 cname(mName);
  nsCAutoString functionUri(aClassStr);
  PRInt32 hash = functionUri.RFindChar('#');
  if (hash != kNotFound) {
    functionUri.Truncate(hash);
  }

  JSObject* methodObject = nsnull;
  nsresult rv = aContext->CompileFunction(aClassObject,
                                          cname,
                                          paramCount,
                                          (const char**)args,
                                          body, 
                                          functionUri.get(),
                                          uncompiledMethod->mBodyText.GetLineNumber(),
                                          JSVERSION_LATEST,
                                          PR_TRUE,
                                          (void **) &methodObject);

  // Destroy our uncompiled method and delete our arg list.
  delete uncompiledMethod;
  delete [] args;
  if (NS_FAILED(rv)) {
    SetUncompiledMethod(nsnull);
    return rv;
  }

  mJSMethodObject = methodObject;

  return NS_OK;
}

void
nsXBLProtoImplMethod::Trace(TraceCallback aCallback, void *aClosure) const
{
  if (IsCompiled() && mJSMethodObject) {
    aCallback(nsIProgrammingLanguage::JAVASCRIPT, mJSMethodObject, aClosure);
  }
}

nsresult
nsXBLProtoImplAnonymousMethod::Execute(nsIContent* aBoundElement)
{
  NS_PRECONDITION(IsCompiled(), "Can't execute uncompiled method");
  
  if (!mJSMethodObject) {
    // Nothing to do here
    return NS_OK;
  }

  // Get the script context the same way
  // nsXBLProtoImpl::InstallImplementation does.
  nsIDocument* document = aBoundElement->GetOwnerDoc();
  if (!document) {
    return NS_OK;
  }

  nsIScriptGlobalObject* global = document->GetScriptGlobalObject();
  if (!global) {
    return NS_OK;
  }

  nsCOMPtr<nsIScriptContext> context = global->GetContext();
  if (!context) {
    return NS_OK;
  }
  
  JSContext* cx = (JSContext*) context->GetNativeContext();

  JSObject* globalObject = global->GetGlobalJSObject();

  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  jsval v;
  nsresult rv =
    nsContentUtils::WrapNative(cx, globalObject, aBoundElement, &v,
                               getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* thisObject = JSVAL_TO_OBJECT(v);

  JSAutoRequest ar(cx);

  // Clone the function object, using thisObject as the parent so "this" is in
  // the scope chain of the resulting function (for backwards compat to the
  // days when this was an event handler).
  JSObject* method = ::JS_CloneFunctionObject(cx, mJSMethodObject, thisObject);
  if (!method)
    return NS_ERROR_OUT_OF_MEMORY;

  // Now call the method

  // Use nsCxPusher to make sure we call ScriptEvaluated when we're done.
  nsCxPusher pusher;
  NS_ENSURE_STATE(pusher.Push(aBoundElement));

  // Check whether it's OK to call the method.
  rv = nsContentUtils::GetSecurityManager()->CheckFunctionAccess(cx, method,
                                                                 thisObject);

  JSBool ok = JS_TRUE;
  if (NS_SUCCEEDED(rv)) {
    jsval retval;
    ok = ::JS_CallFunctionValue(cx, thisObject, OBJECT_TO_JSVAL(method),
                                0 /* argc */, nsnull /* argv */, &retval);
  }

  if (!ok) {
    // If a constructor or destructor threw an exception, it doesn't stop
    // anything else.  We just report it.  Note that we need to set aside the
    // frame chain here, since the constructor invocation is not related to
    // whatever is on the stack right now, really.
    JSStackFrame* frame = JS_SaveFrameChain(cx);
    ::JS_ReportPendingException(cx);
    JS_RestoreFrameChain(cx, frame);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

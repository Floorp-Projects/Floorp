/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "mozilla/FunctionTimer.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsXBLProtoImplMethod.h"
#include "nsIScriptContext.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "nsXBLPrototypeBinding.h"

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
                                    JSObject* aScriptObject,
                                    JSObject* aTargetClassObject,
                                    const nsCString& aClassStr)
{
  NS_PRECONDITION(IsCompiled(),
                  "Should not be installing an uncompiled method");
  JSContext* cx = aContext->GetNativeContext();

  nsIScriptGlobalObject* sgo = aBoundElement->OwnerDoc()->GetScopeObject();

  if (!sgo) {
    return NS_ERROR_UNEXPECTED;
  }

  NS_ASSERTION(aScriptObject, "uh-oh, script Object should NOT be null or bad things will happen");
  if (!aScriptObject)
    return NS_ERROR_FAILURE;

  JSObject* globalObject = sgo->GetGlobalJSObject();

  // now we want to reevaluate our property using aContext and the script object for this window...
  if (mJSMethodObject && aTargetClassObject) {
    nsDependentString name(mName);
    JSAutoRequest ar(cx);
    JSAutoEnterCompartment ac;

    if (!ac.enter(cx, globalObject)) {
      return NS_ERROR_UNEXPECTED;
    }

    JSObject * method = ::JS_CloneFunctionObject(cx, mJSMethodObject, globalObject);
    if (!method) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!::JS_DefineUCProperty(cx, aTargetClassObject,
                               static_cast<const jschar*>(mName),
                               name.Length(), OBJECT_TO_JSVAL(method),
                               NULL, NULL, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

nsresult 
nsXBLProtoImplMethod::CompileMember(nsIScriptContext* aContext, const nsCString& aClassStr,
                                    JSObject* aClassObject)
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
                                          const_cast<const char**>(args),
                                          body, 
                                          functionUri.get(),
                                          uncompiledMethod->mBodyText.GetLineNumber(),
                                          JSVERSION_LATEST,
                                          true,
                                          &methodObject);

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
    aCallback(mJSMethodObject, "mJSMethodObject", aClosure);
  }
}

nsresult
nsXBLProtoImplMethod::Read(nsIScriptContext* aContext,
                           nsIObjectInputStream* aStream)
{
  nsresult rv = XBL_DeserializeFunction(aContext, aStream, &mJSMethodObject);
  if (NS_FAILED(rv)) {
    SetUncompiledMethod(nsnull);
    return rv;
  }

#ifdef DEBUG
  mIsCompiled = true;
#endif

  return NS_OK;
}

nsresult
nsXBLProtoImplMethod::Write(nsIScriptContext* aContext,
                            nsIObjectOutputStream* aStream)
{
  if (mJSMethodObject) {
    nsresult rv = aStream->Write8(XBLBinding_Serialize_Method);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->WriteWStringZ(mName);
    NS_ENSURE_SUCCESS(rv, rv);

    return XBL_SerializeFunction(aContext, aStream, mJSMethodObject);
  }

  return NS_OK;
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
  nsIDocument* document = aBoundElement->OwnerDoc();

  nsIScriptGlobalObject* global = document->GetScriptGlobalObject();
  if (!global) {
    return NS_OK;
  }

  nsCOMPtr<nsIScriptContext> context = global->GetContext();
  if (!context) {
    return NS_OK;
  }

  nsAutoMicroTask mt;

  JSContext* cx = context->GetNativeContext();

  JSObject* globalObject = global->GetGlobalJSObject();

  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  jsval v;
  nsresult rv =
    nsContentUtils::WrapNative(cx, globalObject, aBoundElement, &v,
                               getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* thisObject = JSVAL_TO_OBJECT(v);

  JSAutoRequest ar(cx);
  JSAutoEnterCompartment ac;

  if (!ac.enter(cx, thisObject))
    return NS_ERROR_UNEXPECTED;

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
    JSBool saved = JS_SaveFrameChain(cx);
    JS_ReportPendingException(cx);
    if (saved)
        JS_RestoreFrameChain(cx);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsXBLProtoImplAnonymousMethod::Write(nsIScriptContext* aContext,
                                     nsIObjectOutputStream* aStream,
                                     XBLBindingSerializeDetails aType)
{
  if (mJSMethodObject) {
    nsresult rv = aStream->Write8(aType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = XBL_SerializeFunction(aContext, aStream, mJSMethodObject);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

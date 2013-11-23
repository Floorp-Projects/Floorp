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
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsXBLProtoImplMethod.h"
#include "nsIScriptContext.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "xpcpublic.h"
#include "nsXBLPrototypeBinding.h"

using namespace mozilla;

nsXBLProtoImplMethod::nsXBLProtoImplMethod(const PRUnichar* aName) :
  nsXBLProtoImplMember(aName),
  mMethod()
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

  if (aText.IsEmpty()) {
    NS_WARNING("Empty name attribute in xbl:parameter!");
    return;
  }

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
nsXBLProtoImplMethod::SetLineNumber(uint32_t aLineNumber)
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
nsXBLProtoImplMethod::InstallMember(JSContext* aCx,
                                    JS::Handle<JSObject*> aTargetClassObject)
{
  NS_PRECONDITION(IsCompiled(),
                  "Should not be installing an uncompiled method");
  MOZ_ASSERT(js::IsObjectInContextCompartment(aTargetClassObject, aCx));

  JS::Rooted<JSObject*> globalObject(aCx, JS_GetGlobalForObject(aCx, aTargetClassObject));
  MOZ_ASSERT(xpc::IsInXBLScope(globalObject) ||
             globalObject == xpc::GetXBLScope(aCx, globalObject));

  JS::Rooted<JSObject*> jsMethodObject(aCx, GetCompiledMethod());
  if (jsMethodObject) {
    nsDependentString name(mName);

    JS::Rooted<JSObject*> method(aCx, JS_CloneFunctionObject(aCx, jsMethodObject, globalObject));
    NS_ENSURE_TRUE(method, NS_ERROR_OUT_OF_MEMORY);

    JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*method));
    if (!::JS_DefineUCProperty(aCx, aTargetClassObject,
                               static_cast<const jschar*>(mName),
                               name.Length(), value,
                               nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

nsresult 
nsXBLProtoImplMethod::CompileMember(const nsCString& aClassStr,
                                    JS::Handle<JSObject*> aClassObject)
{
  AssertInCompilationScope();
  NS_PRECONDITION(!IsCompiled(),
                  "Trying to compile an already-compiled method");
  NS_PRECONDITION(aClassObject,
                  "Must have class object to compile");

  nsXBLUncompiledMethod* uncompiledMethod = GetUncompiledMethod();

  // No parameters or body was supplied, so don't install method.
  if (!uncompiledMethod) {
    // Early return after which we consider ourselves compiled.
    SetCompiledMethod(nullptr);

    return NS_OK;
  }

  // Don't install method if no name was supplied.
  if (!mName) {
    delete uncompiledMethod;

    // Early return after which we consider ourselves compiled.
    SetCompiledMethod(nullptr);

    return NS_OK;
  }

  // We have a method.
  // Allocate an array for our arguments.
  int32_t paramCount = uncompiledMethod->GetParameterCount();
  char** args = nullptr;
  if (paramCount > 0) {
    args = new char*[paramCount];
    if (!args)
      return NS_ERROR_OUT_OF_MEMORY;

    // Add our parameters to our args array.
    int32_t argPos = 0; 
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
  nsAutoCString functionUri(aClassStr);
  int32_t hash = functionUri.RFindChar('#');
  if (hash != kNotFound) {
    functionUri.Truncate(hash);
  }

  AutoJSContext cx;
  JSAutoCompartment ac(cx, aClassObject);
  JS::CompileOptions options(cx);
  options.setFileAndLine(functionUri.get(),
                         uncompiledMethod->mBodyText.GetLineNumber())
         .setVersion(JSVERSION_LATEST);
  JS::Rooted<JSObject*> methodObject(cx);
  nsresult rv = nsJSUtils::CompileFunction(cx, JS::NullPtr(), options, cname,
                                           paramCount,
                                           const_cast<const char**>(args),
                                           body, methodObject.address());

  // Destroy our uncompiled method and delete our arg list.
  delete uncompiledMethod;
  delete [] args;
  if (NS_FAILED(rv)) {
    SetUncompiledMethod(nullptr);
    return rv;
  }

  SetCompiledMethod(methodObject);

  return NS_OK;
}

void
nsXBLProtoImplMethod::Trace(const TraceCallbacks& aCallbacks, void *aClosure)
{
  if (IsCompiled() && GetCompiledMethodPreserveColor()) {
    aCallbacks.Trace(&mMethod.AsHeapObject(), "mMethod", aClosure);
  }
}

nsresult
nsXBLProtoImplMethod::Read(nsIObjectInputStream* aStream)
{
  AssertInCompilationScope();
  MOZ_ASSERT(!IsCompiled() && !GetUncompiledMethod());

  AutoJSContext cx;
  JS::Rooted<JSObject*> methodObject(cx);
  nsresult rv = XBL_DeserializeFunction(aStream, &methodObject);
  if (NS_FAILED(rv)) {
    SetUncompiledMethod(nullptr);
    return rv;
  }

  SetCompiledMethod(methodObject);

  return NS_OK;
}

nsresult
nsXBLProtoImplMethod::Write(nsIObjectOutputStream* aStream)
{
  AssertInCompilationScope();
  MOZ_ASSERT(IsCompiled());
  if (GetCompiledMethodPreserveColor()) {
    nsresult rv = aStream->Write8(XBLBinding_Serialize_Method);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->WriteWStringZ(mName);
    NS_ENSURE_SUCCESS(rv, rv);

    // Calling fromMarkedLocation() is safe because mMethod is traced by the
    // Trace() method above, and because its value is never changed after it has
    // been set to a compiled method.
    JS::Handle<JSObject*> method =
      JS::Handle<JSObject*>::fromMarkedLocation(mMethod.AsHeapObject().address());
    return XBL_SerializeFunction(aStream, method);
  }

  return NS_OK;
}

nsresult
nsXBLProtoImplAnonymousMethod::Execute(nsIContent* aBoundElement)
{
  NS_PRECONDITION(IsCompiled(), "Can't execute uncompiled method");

  if (!GetCompiledMethod()) {
    // Nothing to do here
    return NS_OK;
  }

  // Get the script context the same way
  // nsXBLProtoImpl::InstallImplementation does.
  nsIDocument* document = aBoundElement->OwnerDoc();

  nsCOMPtr<nsIScriptGlobalObject> global =
    do_QueryInterface(document->GetWindow());
  if (!global) {
    return NS_OK;
  }

  nsCOMPtr<nsIScriptContext> context = global->GetContext();
  if (!context) {
    return NS_OK;
  }

  nsAutoMicroTask mt;

  AutoPushJSContext cx(context->GetNativeContext());

  JS::Rooted<JSObject*> globalObject(cx, global->GetGlobalJSObject());

  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  JS::Rooted<JS::Value> v(cx);
  nsresult rv =
    nsContentUtils::WrapNative(cx, globalObject, aBoundElement, &v,
                               getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  // Use nsCxPusher to make sure we call ScriptEvaluated when we're done.
  //
  // Make sure to do this before entering the compartment, since pushing Push()
  // may call JS_SaveFrameChain(), which puts us back in an unentered state.
  nsCxPusher pusher;
  if (!pusher.Push(aBoundElement))
    return NS_ERROR_UNEXPECTED;
  MOZ_ASSERT(cx == nsContentUtils::GetCurrentJSContext());

  JS::Rooted<JSObject*> thisObject(cx, &v.toObject());
  JS::Rooted<JSObject*> scopeObject(cx, xpc::GetXBLScope(cx, globalObject));
  NS_ENSURE_TRUE(scopeObject, NS_ERROR_OUT_OF_MEMORY);

  JSAutoCompartment ac(cx, scopeObject);
  if (!JS_WrapObject(cx, &thisObject))
      return NS_ERROR_OUT_OF_MEMORY;

  // Clone the function object, using thisObject as the parent so "this" is in
  // the scope chain of the resulting function (for backwards compat to the
  // days when this was an event handler).
  JS::Rooted<JSObject*> method(cx, ::JS_CloneFunctionObject(cx, GetCompiledMethod(), thisObject));
  if (!method)
    return NS_ERROR_OUT_OF_MEMORY;

  // Now call the method

  // Check whether script is enabled.
  bool scriptAllowed = nsContentUtils::GetSecurityManager()->
                         ScriptAllowed(js::GetGlobalForObjectCrossCompartment(method));

  bool ok = true;
  if (scriptAllowed) {
    JS::Rooted<JS::Value> retval(cx);
    ok = ::JS_CallFunctionValue(cx, thisObject, OBJECT_TO_JSVAL(method),
                                0 /* argc */, nullptr /* argv */, retval.address());
  }

  if (!ok) {
    // If a constructor or destructor threw an exception, it doesn't stop
    // anything else.  We just report it.  Note that we need to set aside the
    // frame chain here, since the constructor invocation is not related to
    // whatever is on the stack right now, really.
    nsJSUtils::ReportPendingException(cx);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsXBLProtoImplAnonymousMethod::Write(nsIObjectOutputStream* aStream,
                                     XBLBindingSerializeDetails aType)
{
  AssertInCompilationScope();
  MOZ_ASSERT(IsCompiled());
  if (GetCompiledMethodPreserveColor()) {
    nsresult rv = aStream->Write8(aType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->WriteWStringZ(mName);
    NS_ENSURE_SUCCESS(rv, rv);

    // Calling fromMarkedLocation() is safe because mMethod is traced by the
    // Trace() method above, and because its value is never changed after it has
    // been set to a compiled method.
    JS::Handle<JSObject*> method =
      JS::Handle<JSObject*>::fromMarkedLocation(mMethod.AsHeapObject().address());
    rv = XBL_SerializeFunction(aStream, method);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIGlobalObject.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsXBLProtoImplMethod.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "xpcpublic.h"
#include "nsXBLPrototypeBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;
using namespace mozilla::dom;

nsXBLProtoImplMethod::nsXBLProtoImplMethod(const char16_t* aName) :
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
  MOZ_ASSERT(!IsCompiled(),
             "Must not be compiled when accessing uncompiled method");

  nsXBLUncompiledMethod* uncompiledMethod = GetUncompiledMethod();
  if (!uncompiledMethod) {
    uncompiledMethod = new nsXBLUncompiledMethod();
    SetUncompiledMethod(uncompiledMethod);
  }

  uncompiledMethod->AppendBodyText(aText);
}

void
nsXBLProtoImplMethod::AddParameter(const nsAString& aText)
{
  MOZ_ASSERT(!IsCompiled(),
             "Must not be compiled when accessing uncompiled method");

  if (aText.IsEmpty()) {
    NS_WARNING("Empty name attribute in xbl:parameter!");
    return;
  }

  nsXBLUncompiledMethod* uncompiledMethod = GetUncompiledMethod();
  if (!uncompiledMethod) {
    uncompiledMethod = new nsXBLUncompiledMethod();
    SetUncompiledMethod(uncompiledMethod);
  }

  uncompiledMethod->AddParameter(aText);
}

void
nsXBLProtoImplMethod::SetLineNumber(uint32_t aLineNumber)
{
  MOZ_ASSERT(!IsCompiled(),
             "Must not be compiled when accessing uncompiled method");

  nsXBLUncompiledMethod* uncompiledMethod = GetUncompiledMethod();
  if (!uncompiledMethod) {
    uncompiledMethod = new nsXBLUncompiledMethod();
    SetUncompiledMethod(uncompiledMethod);
  }

  uncompiledMethod->SetLineNumber(aLineNumber);
}

nsresult
nsXBLProtoImplMethod::InstallMember(JSContext* aCx,
                                    JS::Handle<JSObject*> aTargetClassObject)
{
  MOZ_ASSERT(IsCompiled(),
                  "Should not be installing an uncompiled method");
  MOZ_ASSERT(js::IsObjectInContextCompartment(aTargetClassObject, aCx));

#ifdef DEBUG
  {
    JS::Rooted<JSObject*> globalObject(aCx, JS::GetNonCCWObjectGlobal(aTargetClassObject));
    MOZ_ASSERT(xpc::IsInContentXBLScope(globalObject) ||
               globalObject == xpc::GetXBLScope(aCx, globalObject));
    MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx) == globalObject);
  }
#endif

  JS::Rooted<JSObject*> jsMethodObject(aCx, GetCompiledMethod());
  if (jsMethodObject) {
    nsDependentString name(mName);

    JS::Rooted<JSObject*> method(aCx, JS::CloneFunctionObject(aCx, jsMethodObject));
    NS_ENSURE_TRUE(method, NS_ERROR_OUT_OF_MEMORY);

    if (!::JS_DefineUCProperty(aCx, aTargetClassObject,
                               static_cast<const char16_t*>(mName),
                               name.Length(), method,
                               JSPROP_ENUMERATE)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

nsresult
nsXBLProtoImplMethod::CompileMember(AutoJSAPI& jsapi, const nsString& aClassStr,
                                    JS::Handle<JSObject*> aClassObject)
{
  AssertInCompilationScope();
  MOZ_ASSERT(!IsCompiled(),
                  "Trying to compile an already-compiled method");
  MOZ_ASSERT(aClassObject,
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
  char16_t *bodyText = uncompiledMethod->mBodyText.GetText();
  if (bodyText)
    body.Rebind(bodyText);

  // Now that we have a body and args, compile the function
  // and then define it.
  NS_ConvertUTF16toUTF8 cname(mName);
  NS_ConvertUTF16toUTF8 functionUri(aClassStr);
  int32_t hash = functionUri.RFindChar('#');
  if (hash != kNotFound) {
    functionUri.Truncate(hash);
  }

  JSContext *cx = jsapi.cx();
  JSAutoRealm ar(cx, aClassObject);
  JS::CompileOptions options(cx);
  options.setFileAndLine(functionUri.get(),
                         uncompiledMethod->mBodyText.GetLineNumber());
  JS::Rooted<JSObject*> methodObject(cx);
  JS::AutoObjectVector emptyVector(cx);
  nsresult rv = nsJSUtils::CompileFunction(jsapi, emptyVector, options, cname,
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

    JS::Rooted<JSObject*> method(RootingCx(), GetCompiledMethod());
    return XBL_SerializeFunction(aStream, method);
  }

  return NS_OK;
}

nsresult
nsXBLProtoImplAnonymousMethod::Execute(nsIContent* aBoundElement,
                                       const nsXBLPrototypeBinding& aProtoBinding)
{
  MOZ_ASSERT(aBoundElement->IsElement());
  MOZ_ASSERT(IsCompiled(), "Can't execute uncompiled method");

  if (!GetCompiledMethod()) {
    // Nothing to do here
    return NS_OK;
  }

  // Get the script context the same way
  // nsXBLProtoImpl::InstallImplementation does.
  nsIDocument* document = aBoundElement->OwnerDoc();

  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(document->GetInnerWindow());
  if (!global) {
    return NS_OK;
  }

  nsAutoMicroTask mt;

  // We are going to run script via JS::Call, so we need a script entry point,
  // but as this is XBL related it does not appear in the HTML spec.
  // We need an actual JSContext to do GetXBLScopeOrGlobal, and it needs to
  // be in the compartment of globalObject.  But we want our XBL execution scope
  // to be our entry global.
  AutoJSAPI jsapi;
  if (!jsapi.Init(global)) {
    return NS_ERROR_UNEXPECTED;
  }

  JS::Rooted<JSObject*> scopeObject(jsapi.cx(),
    xpc::GetXBLScopeOrGlobal(jsapi.cx(), global->GetGlobalJSObject()));
  NS_ENSURE_TRUE(scopeObject, NS_ERROR_OUT_OF_MEMORY);

  dom::AutoEntryScript aes(scopeObject,
                           "XBL <constructor>/<destructor> invocation",
                           true);
  JSContext* cx = aes.cx();
  JS::AutoObjectVector scopeChain(cx);
  if (!nsJSUtils::GetScopeChainForXBL(cx, aBoundElement->AsElement(),
                                      aProtoBinding,
                                      scopeChain)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  MOZ_ASSERT(scopeChain.length() != 0);

  // Clone the function object, using our scope chain (for backwards
  // compat to the days when this was an event handler).
  JS::Rooted<JSObject*> jsMethodObject(cx, GetCompiledMethod());
  JS::Rooted<JSObject*> method(cx, JS::CloneFunctionObject(cx, jsMethodObject,
                                                           scopeChain));
  if (!method)
    return NS_ERROR_OUT_OF_MEMORY;

  // Now call the method

  // Check whether script is enabled.
  bool scriptAllowed = xpc::Scriptability::Get(method).Allowed();

  if (scriptAllowed) {
    JS::Rooted<JS::Value> retval(cx);
    JS::Rooted<JS::Value> methodVal(cx, JS::ObjectValue(*method));
    // No need to check the return here as AutoEntryScript has taken ownership
    // of error reporting.
    ::JS::Call(cx, scopeChain[0], methodVal, JS::HandleValueArray::empty(), &retval);
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

    JS::Rooted<JSObject*> method(RootingCx(), GetCompiledMethod());
    rv = XBL_SerializeFunction(aStream, method);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

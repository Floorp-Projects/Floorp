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

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsXBLProtoImplMethod.h"
#include "nsIScriptContext.h"

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
  if (!gScriptRuntime) {
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

MOZ_DECL_CTOR_COUNTER(nsXBLProtoImplMethod);

nsXBLProtoImplMethod::nsXBLProtoImplMethod(const nsAReadableString& aName)
:nsXBLProtoImplMember(&aName), 
 mUncompiledMethod(nsnull)
{
  MOZ_COUNT_CTOR(nsXBLProtoImplMethod);
}

nsXBLProtoImplMethod::~nsXBLProtoImplMethod()
{
  MOZ_COUNT_DTOR(nsXBLProtoImplMethod);
}

void
nsXBLProtoImplMethod::Destroy(PRBool aIsCompiled)
{
  if (aIsCompiled) {
    if (mJSMethodObject)
      RemoveJSGCRoot(&mJSMethodObject);
    mJSMethodObject = nsnull;
  }
  else {
    delete mUncompiledMethod;
    mUncompiledMethod = nsnull;
  }
}

void 
nsXBLProtoImplMethod::AppendBodyText(const nsAReadableString& aText)
{
  if (!mUncompiledMethod) {
    mUncompiledMethod = new nsXBLUncompiledMethod();
    if (!mUncompiledMethod)
      return;
  }

  mUncompiledMethod->AppendBodyText(aText);
}

void 
nsXBLProtoImplMethod::AddParameter(const nsAReadableString& aText)
{
  if (!mUncompiledMethod) {
    mUncompiledMethod = new nsXBLUncompiledMethod();
    if (!mUncompiledMethod)
      return;
  }

  mUncompiledMethod->AddParameter(aText);
}

nsresult
nsXBLProtoImplMethod::InstallMember(nsIScriptContext* aContext, nsIContent* aBoundElement, 
                                      void* aScriptObject, void* aTargetClassObject)
{
  JSContext* cx = (JSContext*) aContext->GetNativeContext();
  JSObject * scriptObject = (JSObject *) aScriptObject;
  NS_ASSERTION(scriptObject, "uh-oh, script Object should NOT be null or bad things will happen");
  if (!scriptObject)
    return NS_ERROR_FAILURE;

  JSObject * targetClassObject = (JSObject *) aTargetClassObject;
  JSObject * globalObject = ::JS_GetGlobalObject(cx);

  // now we want to reevaluate our property using aContext and the script object for this window...
  if (mJSMethodObject && targetClassObject) {
    nsDependentString name(mName);
    JSObject * method = ::JS_CloneFunctionObject(cx, mJSMethodObject, globalObject);
    ::JS_DefineUCProperty(cx, targetClassObject, NS_REINTERPRET_CAST(const jschar*, mName), 
                          name.Length(), OBJECT_TO_JSVAL(method),
                          NULL, NULL, JSPROP_ENUMERATE);
  }
  return NS_OK;
}

nsresult 
nsXBLProtoImplMethod::CompileMember(nsIScriptContext* aContext, const nsCString& aClassStr,
                                    void* aClassObject)
{
  if (!aClassObject)
    return NS_OK; // Nothing to do.

  if (!mName)
    return NS_ERROR_FAILURE; // Without a valid name, we can't install the member.

  // We have a method.
  nsresult rv = NS_OK;

  // Allocate an array for our arguments.
  PRInt32 paramCount = mUncompiledMethod->GetParameterCount();
  char** args = nsnull;
  if (paramCount > 0) {
    args = new char*[paramCount];
    if (!args)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // Add our parameters to our args array.
  PRInt32 argPos = 0; 
  for (nsXBLParameter* curr = mUncompiledMethod->mParameters; 
       curr; 
       curr = curr->mNext) {
    args[argPos] = curr->mName;
    argPos++;
  }

  // Now that we have a body and args, compile the function
  // and then define it.
  nsDependentString body(mUncompiledMethod->mBodyText);
  if (!body.IsEmpty()) {
    nsCAutoString cname; cname.AssignWithConversion(mName);
    nsCAutoString functionUri(aClassStr);
    functionUri += ".";
    functionUri += cname;
    functionUri += "()";
    
    JSObject* methodObject = nsnull;
    aContext->CompileFunction(aClassObject,
                              cname,
                              paramCount,
                              (const char**)args,
                              body, 
                              functionUri.get(),
                              0,
                              PR_FALSE,
                              (void **) &methodObject);

    // Destroy our uncompiled method and delete our arg list.
    delete mUncompiledMethod;
    delete [] args;
    mJSMethodObject = methodObject;

    if (methodObject) {
      // Root the compiled prototype script object.
      JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                          aContext->GetNativeContext());
      if (!cx) return NS_ERROR_UNEXPECTED;
      AddJSGCRoot(&mJSMethodObject, "nsXBLProtoImplMethod::mJSMethodObject");
    }
  }
  
  return NS_OK;
}

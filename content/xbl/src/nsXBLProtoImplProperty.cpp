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
#include "nsXBLProtoImplProperty.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
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

MOZ_DECL_CTOR_COUNTER(nsXBLProtoImplProperty);

nsXBLProtoImplProperty::nsXBLProtoImplProperty(const PRUnichar* aName,
                                               const PRUnichar* aGetter, 
                                               const PRUnichar* aSetter,
                                               const PRUnichar* aReadOnly)
:nsXBLProtoImplMember(aName), 
 mGetterText(nsnull),
 mSetterText(nsnull)
{
  MOZ_COUNT_CTOR(nsXBLProtoImplProperty);

  mJSAttributes = JSPROP_ENUMERATE;
  if (aReadOnly) {
    nsAutoString readOnly; readOnly.Assign(*aReadOnly);
    if (readOnly.EqualsIgnoreCase("true"))
      mJSAttributes |= JSPROP_READONLY;
  }

  if (aGetter)
    AppendGetterText(nsDependentString(aGetter));
  if (aSetter)
    AppendSetterText(nsDependentString(aSetter));
}

nsXBLProtoImplProperty::~nsXBLProtoImplProperty()
{
  MOZ_COUNT_DTOR(nsXBLProtoImplProperty);
}

void
nsXBLProtoImplProperty::Destroy(PRBool aIsCompiled)
{
  if (aIsCompiled) {
    if (mJSGetterObject)
      RemoveJSGCRoot(&mJSGetterObject);
    if (mJSSetterObject)
      RemoveJSGCRoot(&mJSSetterObject);
    mJSGetterObject = mJSSetterObject = nsnull;
  }
  else {
    nsMemory::Free(mGetterText);
    nsMemory::Free(mSetterText);
    mGetterText = mSetterText = nsnull;
  }
}

void 
nsXBLProtoImplProperty::AppendGetterText(const nsAString& aText)
{
  if (mGetterText) {
    PRUnichar* temp = mGetterText;
    mGetterText = ToNewUnicode(nsDependentString(temp) + aText);
    nsMemory::Free(temp);
  }
  else
    mGetterText = ToNewUnicode(aText);
}

void 
nsXBLProtoImplProperty::AppendSetterText(const nsAString& aText)
{
  if (mSetterText) {
    PRUnichar* temp = mSetterText;
    mSetterText = ToNewUnicode(nsDependentString(temp) + aText);
    nsMemory::Free(temp);
  }
  else
    mSetterText = ToNewUnicode(aText);
}

const char* gPropertyArgs[] = { "val" };

nsresult
nsXBLProtoImplProperty::InstallMember(nsIScriptContext* aContext, nsIContent* aBoundElement, 
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
  if ((mJSGetterObject || mJSSetterObject) && targetClassObject) {
    JSObject * getter = nsnull;
    if (mJSGetterObject)
      getter = ::JS_CloneFunctionObject(cx, mJSGetterObject, globalObject);
    
    JSObject * setter = nsnull;
    if (mJSSetterObject)
      setter = ::JS_CloneFunctionObject(cx, mJSSetterObject, globalObject);

    nsDependentString name(mName);
    ::JS_DefineUCProperty(cx, targetClassObject, NS_REINTERPRET_CAST(const jschar*, mName), 
                          name.Length(), JSVAL_VOID,  (JSPropertyOp) getter, 
                          (JSPropertyOp) setter, mJSAttributes); 
  }
  return NS_OK;
}

nsresult 
nsXBLProtoImplProperty::CompileMember(nsIScriptContext* aContext, const nsCString& aClassStr,
                                      void* aClassObject)
{
  if (!aClassObject)
    return NS_OK; // Nothing to do.

  if (!mName)
    return NS_ERROR_FAILURE; // Without a valid name, we can't install the member.

  // We have a property.
  nsresult rv = NS_OK;

  // Do we have a getter?
  nsAutoString getter(mGetterText);

  // Make sure we free mGetterText here before calling
  // CompileFunction() since that'll overwrite mGetterText
  nsMemory::Free(mGetterText);
  mGetterText = nsnull;

  nsCAutoString functionUri;
  if (!getter.IsEmpty() && aClassObject) {
    functionUri = aClassStr + NS_LITERAL_CSTRING(".");
    AppendUTF16toUTF8(mName, functionUri);
    functionUri += NS_LITERAL_CSTRING(" (getter)");
    rv = aContext->CompileFunction(aClassObject,
                                   nsCAutoString("onget"),
                                   0,
                                   nsnull,
                                   getter, 
                                   functionUri.get(),
                                   0,
                                   PR_FALSE,
                                   (void **) &mJSGetterObject);
    if (mJSGetterObject && NS_SUCCEEDED(rv)) {
      mJSAttributes |= JSPROP_GETTER | JSPROP_SHARED;
      // Root the compiled prototype script object.
      JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                          aContext->GetNativeContext());
      rv = (cx)
            ? AddJSGCRoot(&mJSGetterObject, "nsXBLProtoImplProperty::mJSGetterObject")
            : NS_ERROR_UNEXPECTED;
    }
    if (NS_FAILED(rv)) {
      mJSGetterObject = nsnull;
      mJSAttributes &= ~JSPROP_GETTER;
      /*chaining to return failure*/
    }
  } // if getter is not empty
  nsresult rvG=rv;

  // Do we have a setter?
  nsAutoString setter(mSetterText);

  // Make sure we free mGetterText here before calling
  // CompileFunction() since that'll overwrite mGetterText
  nsMemory::Free(mSetterText);
  mSetterText = nsnull;

  if (!setter.IsEmpty() && aClassObject) {
    functionUri = aClassStr + NS_LITERAL_CSTRING(".");
    AppendUTF16toUTF8(mName, functionUri);
    functionUri += NS_LITERAL_CSTRING(" (setter)");
    rv = aContext->CompileFunction(aClassObject,
                                   nsCAutoString("onset"),
                                   1,
                                   gPropertyArgs,
                                   setter, 
                                   functionUri.get(),
                                   0,
                                   PR_FALSE,
                                   (void **) &mJSSetterObject);
    if (mJSSetterObject && NS_SUCCEEDED(rv)) {
      mJSAttributes |= JSPROP_SETTER | JSPROP_SHARED;
      // Root the compiled prototype script object.
      JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                          aContext->GetNativeContext());
      rv = (cx)
            ? AddJSGCRoot(&mJSSetterObject, "nsXBLProtoImplProperty::mJSSetterObject")
            : NS_ERROR_UNEXPECTED;
    }
    if (NS_FAILED(rv)) {
      mJSSetterObject = nsnull;
      mJSAttributes &= ~JSPROP_SETTER;
      /*chaining to return failure*/
    }
  } // if setter wasn't empty....

  return NS_SUCCEEDED(rv) ? rvG : rv;
}

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com>
 *   L. David Baron <dbaron@mozillafoundation.org>
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

/**
 * This is not a generated file. It contains common utility functions
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "nsJSUtils.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "prprf.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsIDOMGCParticipant.h"
#include "nsIWeakReference.h"

#include "nsDOMJSUtils.h" // for GetScriptContextFromJSContext

JSBool
nsJSUtils::GetCallingLocation(JSContext* aContext, const char* *aFilename,
                              PRUint32 *aLineno)
{
  // Get the current filename and line number
  JSStackFrame* frame = nsnull;
  JSScript* script = nsnull;
  do {
    frame = ::JS_FrameIterator(aContext, &frame);

    if (frame) {
      script = ::JS_GetFrameScript(aContext, frame);
    }
  } while (frame && !script);

  if (script) {
    const char* filename = ::JS_GetScriptFilename(aContext, script);

    if (filename) {
      PRUint32 lineno = 0;
      jsbytecode* bytecode = ::JS_GetFramePC(aContext, frame);

      if (bytecode) {
        lineno = ::JS_PCToLineNumber(aContext, script, bytecode);
      }

      *aFilename = filename;
      *aLineno = lineno;
      return JS_TRUE;
    }
  }

  return JS_FALSE;
}

jsval
nsJSUtils::ConvertStringToJSVal(const nsString& aProp, JSContext* aContext)
{
  JSString *jsstring =
    ::JS_NewUCStringCopyN(aContext, NS_REINTERPRET_CAST(const jschar*,
                                                        aProp.get()),
                          aProp.Length());

  // set the return value
  return STRING_TO_JSVAL(jsstring);
}

PRBool
nsJSUtils::ConvertJSValToXPCObject(nsISupports** aSupports, REFNSIID aIID,
                                   JSContext* aContext, jsval aValue)
{
  *aSupports = nsnull;
  if (JSVAL_IS_NULL(aValue)) {
    return JS_TRUE;
  }

  if (JSVAL_IS_OBJECT(aValue)) {
    // WrapJS does all the work to recycle an existing wrapper and/or do a QI
    nsresult rv = nsContentUtils::XPConnect()->
      WrapJS(aContext, JSVAL_TO_OBJECT(aValue), aIID, (void**)aSupports);

    return NS_SUCCEEDED(rv);
  }

  return JS_FALSE;
}

void
nsJSUtils::ConvertJSValToString(nsAString& aString, JSContext* aContext,
                                jsval aValue)
{
  JSString *jsstring;
  if ((jsstring = ::JS_ValueToString(aContext, aValue)) != nsnull) {
    aString.Assign(NS_REINTERPRET_CAST(const PRUnichar*,
                                       ::JS_GetStringChars(jsstring)),
                   ::JS_GetStringLength(jsstring));
  }
  else {
    aString.Truncate();
  }
}

PRBool
nsJSUtils::ConvertJSValToUint32(PRUint32* aProp, JSContext* aContext,
                                jsval aValue)
{
  uint32 temp;
  if (::JS_ValueToECMAUint32(aContext, aValue, &temp)) {
    *aProp = (PRUint32)temp;
  }
  else {
    ::JS_ReportError(aContext, "Parameter must be an integer");
    return JS_FALSE;
  }

  return JS_TRUE;
}

nsIScriptGlobalObject *
nsJSUtils::GetStaticScriptGlobal(JSContext* aContext, JSObject* aObj)
{
  nsISupports* supports;
  JSClass* clazz;
  JSObject* parent;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return nsnull;

  while ((parent = ::JS_GetParent(aContext, glob)))
    glob = parent;

  clazz = JS_GET_CLASS(aContext, glob);

  if (!clazz ||
      !(clazz->flags & JSCLASS_HAS_PRIVATE) ||
      !(clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) ||
      !(supports = (nsISupports*)::JS_GetPrivate(aContext, glob))) {
    return nsnull;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper(do_QueryInterface(supports));
  NS_ENSURE_TRUE(wrapper, nsnull);

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));

  // We're returning a pointer to something that's about to be
  // released, but that's ok here.
  return sgo;
}

nsIScriptContext *
nsJSUtils::GetStaticScriptContext(JSContext* aContext, JSObject* aObj)
{
  nsIScriptGlobalObject *nativeGlobal = GetStaticScriptGlobal(aContext, aObj);
  if (!nativeGlobal)
    return nsnull;

  return nativeGlobal->GetScriptContext(nsIProgrammingLanguage::JAVASCRIPT);
}

nsIScriptGlobalObject *
nsJSUtils::GetDynamicScriptGlobal(JSContext* aContext)
{
  nsIScriptContext *scriptCX = GetDynamicScriptContext(aContext);
  if (!scriptCX)
    return nsnull;
  return scriptCX->GetGlobalObject();
}

nsIScriptContext *
nsJSUtils::GetDynamicScriptContext(JSContext *aContext)
{
  return GetScriptContextFromJSContext(aContext);
}

#define MARKED_OBJECT_BIT (PRWord(1<<0))

void
nsMarkedJSFunctionHolder_base::Set(nsISupports *aPotentialFunction,
                                   nsIDOMGCParticipant *aParticipant)
{
  if (PRWord(mObject) & MARKED_OBJECT_BIT) {
    nsDOMClassInfo::ReleaseWrapper(this);
  }
  nsISupports *oldVal = (nsISupports*)(PRWord(mObject) & ~MARKED_OBJECT_BIT);
  if (!TryMarkedSet(aPotentialFunction, aParticipant)) {
    NS_ASSERTION((PRWord(aPotentialFunction) & MARKED_OBJECT_BIT) == 0,
                 "low bit set");
    NS_IF_ADDREF(aPotentialFunction);
    mObject = aPotentialFunction;
  }
  NS_IF_RELEASE(oldVal);
}

static nsIXPConnectJSObjectHolder* HolderToWrappedJS(void *aKey)
{
  nsMarkedJSFunctionHolder_base *holder = NS_STATIC_CAST(
    nsMarkedJSFunctionHolder_base*, aKey);

  NS_ASSERTION(PRWord(holder->mObject) & MARKED_OBJECT_BIT,
               "yikes, not a marked object");

  nsIWeakReference* weakRef =
    (nsIWeakReference*)(PRWord(holder->mObject) & ~MARKED_OBJECT_BIT);

  // This entire interface is a hack to avoid reference counting, so
  // this actually doesn't do any reference counting, and we don't leak
  // anything.  This is needed so we don't add and remove GC roots in
  // the middle of GC.
  nsWeakRefToIXPConnectWrappedJS *result;
  if (NS_FAILED(CallQueryReferent(weakRef, &result)))
    result = nsnull;
  return result;
}

PRBool
nsMarkedJSFunctionHolder_base::TryMarkedSet(nsISupports *aPotentialFunction,
                                            nsIDOMGCParticipant *aParticipant)
{
  if (!aParticipant)
    return PR_FALSE;

  nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS =
    do_QueryInterface(aPotentialFunction);
  if (!wrappedJS) // a non-JS implementation
    return PR_FALSE;

  // XXX We really only need to pass PR_TRUE for
  // root-if-externally-referenced if this is an onload, onerror,
  // onreadystatechange, etc., so we could pass the responsibility for
  // choosing that to the caller.
  nsresult rv =
    nsDOMClassInfo::PreserveWrapper(this, HolderToWrappedJS, aParticipant,
                                    PR_TRUE);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  nsIWeakReference* weakRef; // [STRONG]
  wrappedJS->GetWeakReference(&weakRef);
  NS_ENSURE_TRUE(weakRef, PR_FALSE);

  NS_ASSERTION((PRWord(weakRef) & MARKED_OBJECT_BIT) == 0, "low bit set");
  mObject = (nsISupports*)(PRWord(weakRef) | MARKED_OBJECT_BIT);
  return PR_TRUE;
}

already_AddRefed<nsISupports>
nsMarkedJSFunctionHolder_base::Get(REFNSIID aIID)
{
  nsISupports *result;
  if (PRWord(mObject) & MARKED_OBJECT_BIT) {
    nsIWeakReference* weakRef =
      (nsIWeakReference*)(PRWord(mObject) & ~MARKED_OBJECT_BIT);
    nsresult rv =
      weakRef->QueryReferent(aIID, NS_REINTERPRET_CAST(void**, &result));
    if (NS_FAILED(rv)) {
      NS_NOTREACHED("GC preservation didn't work");
      result = nsnull;
    }
  } else {
    NS_IF_ADDREF(result = mObject);
  }
  return result;
}

nsMarkedJSFunctionHolder_base::~nsMarkedJSFunctionHolder_base()
{
  if (PRWord(mObject) & MARKED_OBJECT_BIT) {
    nsDOMClassInfo::ReleaseWrapper(this);
  }
  nsISupports *obj = (nsISupports*)(PRWord(mObject) & ~MARKED_OBJECT_BIT);
  NS_IF_RELEASE(obj);
}

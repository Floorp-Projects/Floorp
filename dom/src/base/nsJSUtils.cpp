/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsString.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsIScriptEventListener.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsIDOMDOMException.h"
#include "nsDOMError.h"
#include "nsCOMPtr.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSNativeInitializer.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);

struct ResultMap {nsresult rv; const char* name; const char* format;} map[] = {
#define DOM_MSG_DEF(val, format) \
    {(val), #val, format},
#include "domerr.msg"
#undef DOM_MSG_DEF
    {0,0,0}   // sentinel to mark end of array
};

PRBool
nsJSUtils::NameAndFormatForNSResult(nsresult rv,
                                    const char** name,
                                    const char** format)
{
  for (ResultMap* p = map; p->name; p++)
  {
    if (rv == p->rv)
    {
      if (name) *name = p->name;
      if (format) *format = p->format;
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_EXPORT JSBool
nsJSUtils::nsGetCallingLocation(JSContext* aContext,
                                const char* *aFilename,
                                PRUint32 *aLineno)
{
  // Get the current filename and line number
  JSStackFrame* frame = nsnull;
  JSScript* script = nsnull;
  do {
    frame = JS_FrameIterator(aContext, &frame);
    if (frame) {
      script = JS_GetFrameScript(aContext, frame);
    }
  } while ((nsnull != frame) && (nsnull == script));

  if (script) {
    const char* filename = JS_GetScriptFilename(aContext, script);
    if (filename) {
      PRUint32 lineno = 0;
      jsbytecode* bytecode = JS_GetFramePC(aContext, frame);
      if (bytecode) {
        lineno = JS_PCToLineNumber(aContext, script, bytecode);
      }
      *aFilename = filename;
      *aLineno = lineno;
      return JS_TRUE;
    }
  }

  return JS_FALSE;
}

NS_EXPORT JSBool
nsJSUtils::nsReportError(JSContext* aContext, 
                         JSObject* aObj,
                         nsresult aResult,
                         const char* aMessage)
{
  const char* name = nsnull;
  const char* format = nsnull;

  // Get the name and message
  if (!aMessage)
    NameAndFormatForNSResult(aResult, &name, &format);
  else
    format = aMessage;

  const char* filename;
  PRUint32 lineno;
  char* location = nsnull;

  if (nsJSUtils::nsGetCallingLocation(aContext, &filename, &lineno))
    location = PR_smprintf("%s Line: %d", filename, lineno);
  
  nsCOMPtr<nsIDOMDOMException> exc;
  nsresult rv = NS_NewDOMException(getter_AddRefs(exc), 
                                   aResult,
                                   name, 
                                   format,
                                   location);
  
  if (location) {
    PR_smprintf_free(location);
  }
    
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(exc);
    if (owner) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsGetStaticScriptContext(aContext, aObj, getter_AddRefs(scriptCX));
      if (scriptCX) {
        JSObject* obj;
        rv = owner->GetScriptObject(scriptCX, (void**)&obj);
        if (NS_SUCCEEDED(rv)) {
          JS_SetPendingException(aContext, OBJECT_TO_JSVAL(obj));
        }
      }
    }
  }

  return JS_FALSE;
}

NS_EXPORT PRBool 
nsJSUtils::nsCallJSScriptObjectGetProperty(nsISupports* aSupports,
                                           JSContext* aContext,
                                           JSObject* aObj,
                                           jsval aId,
                                           jsval* aReturn)
{
  nsIJSScriptObject *object;
 
  if (NS_OK == aSupports->QueryInterface(kIJSScriptObjectIID, 
                                         (void**)&object)) {
    PRBool rval;
    rval =  object->GetProperty(aContext, aObj, aId, aReturn);
    NS_RELEASE(object);
    return rval;
  }

  return JS_TRUE;
}

NS_EXPORT PRBool 
nsJSUtils::nsCallJSScriptObjectSetProperty(nsISupports* aSupports,
                                           JSContext* aContext,
                                           JSObject* aObj,
                                           jsval aId,
                                           jsval* aReturn)
{
  nsIJSScriptObject *object;
 
  if (NS_OK == aSupports->QueryInterface(kIJSScriptObjectIID, 
                                         (void**)&object)) {
    PRBool rval;
    rval =  object->SetProperty(aContext, aObj, aId, aReturn);
    NS_RELEASE(object);
    return rval;
  }

  return JS_TRUE;
}

NS_EXPORT void 
nsJSUtils::nsConvertObjectToJSVal(nsISupports* aSupports,
                                  JSContext* aContext,
                                  JSObject* aObj,
                                  jsval* aReturn)
{
  // get the js object\n"
  *aReturn = JSVAL_NULL;
  if (aSupports != nsnull) {
    nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(aSupports);
    if (owner) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsGetStaticScriptContext(aContext, aObj, getter_AddRefs(scriptCX));
      if (scriptCX) {
        JSObject *object = nsnull;
        if (NS_OK == owner->GetScriptObject(scriptCX, (void**)&object)) {
          // set the return value
          *aReturn = OBJECT_TO_JSVAL(object);
        }
      }
    }
    // XXX the caller really ought to be doing this!
    NS_RELEASE(aSupports);
  }
}

NS_EXPORT void
nsJSUtils::nsConvertXPCObjectToJSVal(nsISupports* aSupports,
                                     const nsIID& aIID,
                                     JSContext* aContext,
                                     JSObject* aScope,
                                     jsval* aReturn)
{
  *aReturn = JSVAL_NULL; // a sane value, just in case something blows up
  if (aSupports != nsnull) {
    nsresult rv;

    // WrapNative will deal with wrapper reuse and/or a wrappedJS or DOM obj
    NS_WITH_SERVICE(nsIXPConnect, xpc, kXPConnectCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
      rv = xpc->WrapNative(aContext, aScope, aSupports, aIID, getter_AddRefs(wrapper));
      if (NS_SUCCEEDED(rv)) {
        JSObject* obj;
        rv = wrapper->GetJSObject(&obj);
        if (NS_SUCCEEDED(rv)) {
          // set the return value
          *aReturn = OBJECT_TO_JSVAL(obj);
        }
      }
    }

    // Yes, this is bizarre, but since this method used in a very
    // specific ways in idlc-generated code, it's okay. And yes, it's
    // really the semantics that we want.
    NS_RELEASE(aSupports);
  }
}

NS_EXPORT void 
nsJSUtils::nsConvertStringToJSVal(const nsString& aProp,
                                  JSContext* aContext,
                                  jsval* aReturn)
{
  JSString *jsstring = JS_NewUCStringCopyN(aContext, NS_REINTERPRET_CAST(const jschar*, aProp.GetUnicode()), aProp.Length());
  // set the return value
  *aReturn = STRING_TO_JSVAL(jsstring);
}


NS_EXPORT PRBool 
nsJSUtils::nsConvertJSValToObject(nsISupports** aSupports,
                                  REFNSIID aIID,
                                  const nsString& aTypeName,
                                  JSContext* aContext,
                                  jsval aValue)
{
  if (JSVAL_IS_NULL(aValue)) {
    *aSupports = nsnull;
  }
  else if (JSVAL_IS_OBJECT(aValue)) {
    JSObject* jsobj = JSVAL_TO_OBJECT(aValue); 
    JSClass* jsclass = JS_GetClass(aContext, jsobj);
    if ((nsnull != jsclass) && 
        (jsclass->flags & JSCLASS_HAS_PRIVATE) &&
        (jsclass->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS)) {
      nsISupports *supports = (nsISupports *)JS_GetPrivate(aContext, jsobj);
      if (NS_OK != supports->QueryInterface(aIID, (void **)aSupports)) {
        char buf[128];
        char typeName[128];
        aTypeName.ToCString(typeName, sizeof(typeName));
        sprintf(buf, "Parameter must of type %s", typeName);
        JS_ReportError(aContext, buf);
        return JS_FALSE;
      }
    }
    else {
      JS_ReportError(aContext, "Parameter isn't a object");
      return JS_FALSE;
    }
  }
  else {
    JS_ReportError(aContext, "Parameter must be an object");
    return JS_FALSE;
  }

  return JS_TRUE;
}

NS_EXPORT PRBool
nsJSUtils::nsConvertJSValToXPCObject(nsISupports** aSupports,
                                     REFNSIID aIID,
                                     JSContext* aContext,
                                     jsval aValue)
{
  *aSupports = nsnull;
  if (JSVAL_IS_NULL(aValue)) {
    return JS_TRUE;
  }
  else if (JSVAL_IS_OBJECT(aValue)) {
    nsresult rv;
    NS_WITH_SERVICE(nsIXPConnect, xpc, kXPConnectCID, &rv);
    if (NS_FAILED(rv)) return JS_FALSE;

    // WrapJS does all the work to recycle an existing wrapper and/or do a QI
    rv = xpc->WrapJS(aContext, JSVAL_TO_OBJECT(aValue), aIID, (void**)aSupports);
    if (NS_FAILED(rv)) return JS_FALSE;
    return JS_TRUE;
  }
  else {
    return JS_FALSE;
  }
}


NS_EXPORT void 
nsJSUtils::nsConvertJSValToString(nsAWritableString& aString,
                                  JSContext* aContext,
                                  jsval aValue)
{
  JSString *jsstring;
  if ((jsstring = JS_ValueToString(aContext, aValue)) != nsnull) {
    aString.Assign(NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars(jsstring)));
  }
  else {
    aString.Truncate();
  }
}

NS_EXPORT PRBool
nsJSUtils::nsConvertJSValToBool(PRBool* aProp,
                                JSContext* aContext,
                                jsval aValue)
{
  JSBool temp;
  if (JS_ValueToBoolean(aContext, aValue, &temp)) {
    *aProp = (PRBool)temp;
  }
  else {
    JS_ReportError(aContext, "Parameter must be a boolean");
    return JS_FALSE;
  }
  
  return JS_TRUE;
}

NS_EXPORT PRBool
nsJSUtils::nsConvertJSValToUint32(PRUint32* aProp,
                                  JSContext* aContext,
                                  jsval aValue)
{
  uint32 temp;
  if (JS_ValueToECMAUint32(aContext, aValue, &temp)) {
    *aProp = (PRUint32)temp;
  }
  else {
    JS_ReportError(aContext, "Parameter must be an integer");
    return JS_FALSE;
  }
  
  return JS_TRUE;
}

NS_EXPORT PRBool 
nsJSUtils::nsConvertJSValToFunc(nsIDOMEventListener** aListener,
                                JSContext* aContext,
                                JSObject* aObj,
                                jsval aValue)
{
  if (JSVAL_IS_NULL(aValue)) {
    *aListener = nsnull;
  }
  else if (JSVAL_IS_OBJECT(aValue)) {
    if (JS_TypeOfValue(aContext, aValue) == JSTYPE_FUNCTION){
      nsIScriptContext* scriptContext = (nsIScriptContext*)JS_GetContextPrivate(aContext);
      
      if (NS_OK == NS_NewScriptEventListener(aListener, scriptContext, (void*)aObj, (void*)JSVAL_TO_OBJECT(aValue))) {
        return JS_TRUE;
      }
      JS_ReportError(aContext, "Out of memory");
      return JS_FALSE;
    }
    else {
      JS_ReportError(aContext, "Parameter isn't a callable object");
      return JS_FALSE;
    }
  }
  else {
    JS_ReportError(aContext, "Parameter must be an object");
    return JS_FALSE;
  }

  return JS_TRUE;
}

NS_EXPORT void PR_CALLBACK
nsJSUtils::nsGenericFinalize(JSContext* aContext,
                             JSObject* aObj)
{
  nsISupports *nativeThis = (nsISupports*)JS_GetPrivate(aContext, 
                                                        aObj);
  
  if (nsnull != nativeThis) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == nativeThis->QueryInterface(kIScriptObjectOwnerIID, 
                                            (void**)&owner)) {
      owner->SetScriptObject(nsnull);
      NS_RELEASE(owner);
    }
    
    // The addref was part of JSObject construction
    NS_RELEASE(nativeThis);
  }
}

NS_EXPORT JSBool 
nsJSUtils::nsGenericEnumerate(JSContext* aContext,
                              JSObject* aObj)
{
  nsISupports* nativeThis = (nsISupports*)JS_GetPrivate(aContext, 
                                                        aObj);
  
  if (nsnull != nativeThis) {
    // get the js object
    nsIJSScriptObject *object;
    if (NS_OK == nativeThis->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      object->EnumerateProperty(aContext, aObj);
      NS_RELEASE(object);
    }
  }
  return JS_TRUE;
}

static JSBool PR_CALLBACK
StubConstructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
  JSFunction *fun;
  const char *name;
  
  fun = JS_ValueToFunction(cx, argv[-2]);
  if (!fun)
    return JS_FALSE;
  name = JS_GetFunctionName(fun);
 
  nsCOMPtr<nsIScriptNameSpaceManager> manager;
  nsIID interfaceID, classID;
  nsISupports* native;

  nsCOMPtr<nsIScriptContext> scriptContext;
  nsJSUtils::nsGetStaticScriptContext(cx, obj, getter_AddRefs(scriptContext));
  if (!scriptContext) {
    return JS_FALSE;
  }

  nsresult result =  scriptContext->GetNameSpaceManager(getter_AddRefs(manager));
  if (NS_FAILED(result)) {
    return JS_FALSE;
  }
  
  PRBool isConstructor;
  nsAutoString nameStr;
  nameStr.AssignWithConversion(name);
  result = manager->LookupName(nameStr, 
                               isConstructor, 
                               interfaceID, 
                               classID);
  if (NS_OK == result) {
    result = nsComponentManager::CreateInstance(classID,
                                                nsnull,
                                                interfaceID,
                                                (void **)&native);
    if (NS_FAILED(result)) {
      return JS_FALSE;
    }
    
    nsCOMPtr<nsIJSNativeInitializer> initializer = do_QueryInterface(native);
    if (initializer) {
      result = initializer->Initialize(cx, obj, argc, argv);
      if (NS_FAILED(result)) {
        return JS_FALSE;
      }
    }
    
    if (interfaceID.Equals(kIScriptObjectOwnerIID)) {
      nsJSUtils::nsConvertObjectToJSVal(native, cx, obj, rval);
    }
    else {
      nsJSUtils::nsConvertXPCObjectToJSVal(native, interfaceID, cx, obj, rval);
    }
    return JS_TRUE;
  }
 
  return JS_TRUE;
}

NS_EXPORT JSBool
nsJSUtils::nsGlobalResolve(JSContext* aContext,
                           JSObject* aObj, 
                           jsval aId)
{
  nsresult result;
  jsval val;

  if (JSVAL_IS_STRING(aId)) {
    JSString* jsstring = JSVAL_TO_STRING(aId);
    nsAutoString name(NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars(jsstring)));
    nsIID interfaceID, classID;
    nsISupports* native;

    nsCOMPtr<nsIScriptContext> scriptContext;
    nsGetStaticScriptContext(aContext, aObj, getter_AddRefs(scriptContext));
    if (!scriptContext) {
      return JS_TRUE;
    }

    result = scriptContext->IsContextInitialized();
    if (NS_FAILED(result)) {
      return JS_TRUE;
    }

    nsCOMPtr<nsIScriptNameSpaceManager> manager;
    result = scriptContext->GetNameSpaceManager(getter_AddRefs(manager));
    if (!manager) {
      return JS_FALSE;
    }

    PRBool isConstructor;
    result = manager->LookupName(name, isConstructor, interfaceID, classID);
    if (NS_SUCCEEDED(result)) {
      if (isConstructor) {
        JS_DefineFunction(aContext, aObj, 
                          JS_GetStringBytes(jsstring),
                          StubConstructor, 0,
                          JSPROP_READONLY);
        return JS_TRUE;
      }
      else {
        result = nsComponentManager::CreateInstance(classID,
                                                    nsnull,
                                                    kISupportsIID,
                                                    (void **)&native);
        if (NS_FAILED(result)) {
          return JS_FALSE;
        }

        if (interfaceID.Equals(kIScriptObjectOwnerIID)) {
          nsConvertObjectToJSVal(native, aContext, aObj, &val);
        }
        else {
          nsConvertXPCObjectToJSVal(native, interfaceID, aContext, 
                                    aObj, &val);
        }

        return JS_DefineProperty(aContext, aObj, JS_GetStringBytes(jsstring),
                                 val, nsnull, nsnull, 
                                 JSPROP_ENUMERATE | JSPROP_READONLY);
      }
    }
  }
  
  return nsGenericResolve(aContext, aObj, aId);
}

NS_EXPORT JSBool 
nsJSUtils::nsGenericResolve(JSContext* aContext,
                            JSObject* aObj, 
                            jsval aId)
{
  nsISupports* nativeThis = (nsISupports*)JS_GetPrivate(aContext, 
                                                        aObj);
  
  if (nsnull != nativeThis) {
    // get the js object
    nsIJSScriptObject *object;
    if (NS_OK == nativeThis->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      object->Resolve(aContext, aObj, aId);
      NS_RELEASE(object);
    }
  }
  return JS_TRUE;
}

/**
 * due to some prototype chain hackery, we may have to walk the prototype chain
 * until we find an object that has an nsISupports in its private field.
 */

NS_EXPORT nsISupports*
nsJSUtils::nsGetNativeThis(JSContext* aContext, JSObject* aObj)
{
  while (aObj != nsnull) {
    JSClass* js_class = JS_GetClass(aContext, aObj);
    if (js_class != nsnull &&
       (js_class->flags & JSCLASS_HAS_PRIVATE) &&
       (js_class->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS))
      return (nsISupports*) JS_GetPrivate(aContext, aObj);
    aObj = JS_GetPrototype(aContext, aObj);
  }
  return nsnull;
}

NS_EXPORT nsresult 
nsJSUtils::nsGetStaticScriptGlobal(JSContext* aContext,
                                   JSObject* aObj,
                                   nsIScriptGlobalObject** aNativeGlobal)
{
  nsISupports* supports;
  JSClass* clazz;
  JSObject* parent;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return NS_ERROR_FAILURE;

  while (nsnull != (parent = JS_GetParent(aContext, glob)))
    glob = parent;

#ifdef JS_THREADSAFE
  clazz = JS_GetClass(aContext, glob);
#else
  clazz = JS_GetClass(glob);
#endif

  if (!clazz ||
      !(clazz->flags & JSCLASS_HAS_PRIVATE) ||
      !(clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) ||
      !(supports = (nsISupports*) JS_GetPrivate(aContext, glob))) {
    return NS_ERROR_FAILURE;
  }

  return supports->QueryInterface(NS_GET_IID(nsIScriptGlobalObject),
                                  (void**) aNativeGlobal);
}

NS_EXPORT nsresult 
nsJSUtils::nsGetStaticScriptContext(JSContext* aContext,
                                    JSObject* aObj,
                                    nsIScriptContext** aScriptContext)
{
  nsCOMPtr<nsIScriptGlobalObject> nativeGlobal;
  nsGetStaticScriptGlobal(aContext, aObj, getter_AddRefs(nativeGlobal));
  if (!nativeGlobal)    
    return NS_ERROR_FAILURE;
  nsIScriptContext* scriptContext = nsnull;
  nativeGlobal->GetContext(&scriptContext);
  *aScriptContext = scriptContext;
  return scriptContext ? NS_OK : NS_ERROR_FAILURE;
}  

NS_EXPORT nsresult 
nsJSUtils::nsGetDynamicScriptGlobal(JSContext* aContext,
                                   nsIScriptGlobalObject** aNativeGlobal)
{
  nsIScriptGlobalObject* nativeGlobal = nsnull;
  nsCOMPtr<nsIScriptContext> scriptCX;
  nsGetDynamicScriptContext(aContext, getter_AddRefs(scriptCX));
  if (scriptCX) {
    *aNativeGlobal = nativeGlobal = scriptCX->GetGlobalObject();
  }
  return nativeGlobal ? NS_OK : NS_ERROR_FAILURE;
}  

NS_EXPORT nsresult 
nsJSUtils::nsGetDynamicScriptContext(JSContext *aContext,
                                     nsIScriptContext** aScriptContext)
{
  // XXX We rely on the rule that if any JSContext in our JSRuntime has a 
  // private set then that private *must* be a pointer to an nsISupports.
  nsISupports *supports = (nsIScriptContext*) JS_GetContextPrivate(aContext);
  if (!supports)
      return nsnull;
  return supports->QueryInterface(NS_GET_IID(nsIScriptContext),
                                  (void**)aScriptContext);
}

NS_EXPORT JSBool PR_CALLBACK
nsJSUtils::nsCheckAccess(JSContext *cx, JSObject *obj, 
                         jsid id, JSAccessMode mode,
                         jsval *vp)
{
  if (mode == JSACC_WATCH) {
    jsval value, dummy;
    if (!JS_IdToValue(cx, id, &value))
      return JS_FALSE;
    JSString *str = JS_ValueToString(cx, value);
    if (!str)
      return JS_FALSE;
    return JS_GetUCProperty(cx, obj, JS_GetStringChars(str), JS_GetStringLength(str), &dummy);
  }
  return JS_TRUE;
}

NS_EXPORT nsIScriptSecurityManager * 
nsJSUtils::nsGetSecurityManager(JSContext *cx, JSObject *obj)
{
  if (!mCachedSecurityManager) {
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
      return nsnull;
    }
    mCachedSecurityManager = secMan;
    NS_ADDREF(mCachedSecurityManager);
  }
  return mCachedSecurityManager;
}

NS_EXPORT void 
nsJSUtils::nsClearCachedSecurityManager()
{
  if (mCachedSecurityManager) {
    NS_RELEASE(mCachedSecurityManager);
    mCachedSecurityManager = nsnull;
  }
}

nsIScriptSecurityManager *nsJSUtils::mCachedSecurityManager;


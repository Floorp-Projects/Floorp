/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
nsJSUtils::nsReportError(JSContext* aContext, 
                         nsresult aResult,
                         const char* aMessage)
{
  const char* name = nsnull;
  const char* format = nsnull;
  char* location = nsnull;

  // Get the name and message
  if (aMessage) {
    format = aMessage;
  }
  else {
    NameAndFormatForNSResult(aResult, &name, &format);
  }

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
      PRUint32 lineNo = 0;
      jsbytecode* bytecode = JS_GetFramePC(aContext, frame);
      if (bytecode) {
        lineNo = JS_PCToLineNumber(aContext, script, bytecode);
      }
      location = PR_smprintf("%s Line: %d", filename, lineNo);
    }
  }
  
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
      JSObject* obj;
      nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
      rv = owner->GetScriptObject(scriptCX, (void**)&obj);
      if (NS_SUCCEEDED(rv)) {
        JS_SetPendingException(aContext, OBJECT_TO_JSVAL(obj));
      }
    }
  }

  return JS_FALSE;
}

NS_EXPORT PRBool 
nsJSUtils::nsCallJSScriptObjectGetProperty(nsISupports* aSupports,
                                           JSContext* aContext,
                                           jsval aId,
                                           jsval* aReturn)
{
  nsIJSScriptObject *object;
 
  if (NS_OK == aSupports->QueryInterface(kIJSScriptObjectIID, 
                                         (void**)&object)) {
    PRBool rval;
    rval =  object->GetProperty(aContext, aId, aReturn);
    NS_RELEASE(object);
    return rval;
  }

  return JS_TRUE;
}

NS_EXPORT PRBool 
nsJSUtils::nsLookupGlobalName(nsISupports* aSupports,
                              JSContext* aContext,
                              jsval aId,
                              jsval* aReturn)
{
  nsresult result;

  if (JSVAL_IS_STRING(aId)) {
    JSString* jsstring = JS_ValueToString(aContext, aId);
    nsAutoString name(JS_GetStringChars(jsstring));
    nsIScriptNameSpaceManager* manager;
    nsIScriptContext* scriptContext = (nsIScriptContext*)JS_GetContextPrivate(aContext);
    nsIID classID;
    nsISupports* native;

    result =  scriptContext->GetNameSpaceManager(&manager);
    if (NS_OK == result) {
      result = manager->LookupName(name, PR_FALSE, classID);
      NS_RELEASE(manager);
      if (NS_OK == result) {
        result = nsComponentManager::CreateInstance(classID,
                                              nsnull,
                                              kISupportsIID,
                                              (void **)&native);
        if (NS_OK == result) {
          nsConvertObjectToJSVal(native, aContext, aReturn);
          return PR_TRUE;
        }
        else {
          return PR_FALSE;
        }
      }
    }
    else {
      return PR_FALSE;
    }
  }
  
  return nsCallJSScriptObjectGetProperty(aSupports, aContext, aId, aReturn);
}

NS_EXPORT PRBool 
nsJSUtils::nsCallJSScriptObjectSetProperty(nsISupports* aSupports,
                                           JSContext* aContext,
                                           jsval aId,
                                           jsval* aReturn)
{
  nsIJSScriptObject *object;
 
  if (NS_OK == aSupports->QueryInterface(kIJSScriptObjectIID, 
                                         (void**)&object)) {
    PRBool rval;
    rval =  object->SetProperty(aContext, aId, aReturn);
    NS_RELEASE(object);
    return rval;
  }

  return JS_TRUE;
}

NS_EXPORT void 
nsJSUtils::nsConvertObjectToJSVal(nsISupports* aSupports,
                                  JSContext* aContext,
                                  jsval* aReturn)
{
  // get the js object\n"
  if (aSupports != nsnull) {
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == aSupports->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(aContext);
      if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
        // set the return value
        *aReturn = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(aSupports);
  }
  else {
    *aReturn = JSVAL_NULL;
  }
}

NS_EXPORT void
nsJSUtils::nsConvertXPCObjectToJSVal(nsISupports* aSupports,
                                     const nsIID& aIID,
                                     JSContext* aContext,
                                     jsval* aReturn)
{
  *aReturn = JSVAL_NULL; // a sane value, just in case something blows up
  if (aSupports != nsnull) {
    nsresult rv;
    NS_WITH_SERVICE(nsIXPConnect, xpc, kXPConnectCID, &rv);
    if (NS_FAILED(rv)) return;

    nsIXPConnectWrappedNative* wrapper;
    rv = xpc->WrapNative(aContext, aSupports, aIID, &wrapper);
    if (NS_SUCCEEDED(rv)) {
      JSObject* obj;
      rv = wrapper->GetJSObject(&obj);
      if (NS_SUCCEEDED(rv)) {
        // set the return value
        *aReturn = OBJECT_TO_JSVAL(obj);
      }
      NS_RELEASE(wrapper);
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
  JSString *jsstring = JS_NewUCStringCopyN(aContext, aProp.GetUnicode(), aProp.Length());
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
    if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) {
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

    nsIXPConnectWrappedNative* wrapper;
    rv = xpc->GetWrappedNativeOfJSObject(aContext, JSVAL_TO_OBJECT(aValue), &wrapper);
    if (NS_FAILED(rv)) return JS_FALSE;

    nsISupports* native;
    rv = wrapper->GetNative(&native);
    NS_RELEASE(wrapper);
    if (NS_FAILED(rv)) return JS_FALSE;

    rv = native->QueryInterface(aIID, (void**) aSupports);
    NS_RELEASE(native);
    if (NS_FAILED(rv)) return JS_FALSE;

    return JS_TRUE;
  }
  else {
    return JS_FALSE;
  }
}


NS_EXPORT void 
nsJSUtils::nsConvertJSValToString(nsString& aString,
                                  JSContext* aContext,
                                  jsval aValue)
{
  JSString *jsstring;
  if ((jsstring = JS_ValueToString(aContext, aValue)) != nsnull) {
    aString.SetString(JS_GetStringChars(jsstring));
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
  if (JSVAL_IS_BOOLEAN(aValue) && JS_ValueToBoolean(aContext, aValue, &temp)) {
    *aProp = (PRBool)temp;
  }
  else {
    JS_ReportError(aContext, "Parameter must be a boolean");
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
    JSFunction* jsfun = JS_ValueToFunction(aContext, aValue);
    if (jsfun){
      nsIScriptContext* scriptContext = (nsIScriptContext*)JS_GetContextPrivate(aContext);
      
      if (NS_OK == NS_NewScriptEventListener(aListener, scriptContext, (void*)aObj, (void*)jsfun)) {
        return JS_TRUE;
      }
      else {
        JS_ReportError(aContext, "Out of memory");
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

NS_EXPORT void 
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
      object->EnumerateProperty(aContext);
      NS_RELEASE(object);
    }
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
    JSString* jsstring = JS_ValueToString(aContext, aId);
    nsAutoString name(JS_GetStringChars(jsstring));
    nsIScriptNameSpaceManager* manager;
    nsIScriptContext* scriptContext = (nsIScriptContext*)JS_GetContextPrivate(aContext);
    nsIID classID;
    nsISupports* native;

    if (NS_COMFALSE == scriptContext->IsContextInitialized()) {
      return JS_TRUE;
    }

    result =  scriptContext->GetNameSpaceManager(&manager);
    if (NS_OK == result) {
      result = manager->LookupName(name, PR_FALSE, classID);
      NS_RELEASE(manager);
      if (NS_OK == result) {
        result = nsComponentManager::CreateInstance(classID,
                                              nsnull,
                                              kISupportsIID,
                                              (void **)&native);
        if (NS_OK == result) {
          nsConvertObjectToJSVal(native, aContext, &val);
          if (JS_DefineProperty(aContext, aObj, JS_GetStringBytes(jsstring),
                                val, nsnull, nsnull, 
                                JSPROP_ENUMERATE | JSPROP_READONLY)) {
            return PR_TRUE;
          }
          else {
            return PR_FALSE;
          }
        }
        else {
          return PR_FALSE;
        }
      }
      else {
        return nsGenericResolve(aContext, aObj, aId);
      }
    }
    else {
      return PR_FALSE;
    }
  }
  
  return PR_TRUE;
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
      object->Resolve(aContext, aId);
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


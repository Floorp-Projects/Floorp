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

#include "jsapi.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsString.h"

static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

PRBool 
nsCallJSScriptObjectGetProperty(nsISupports* aSupports,
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

PRBool 
nsCallJSScriptObjectSetProperty(nsISupports* aSupports,
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

void 
nsConvertObjectToJSVal(nsISupports* aSupports,
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

void 
nsConvertStringToJSVal(const nsString& aProp,
		       JSContext* aContext,
		       jsval* aReturn)
{
  JSString *jsstring = JS_NewUCStringCopyN(aContext, aProp, aProp.Length());
  // set the return value
  *aReturn = STRING_TO_JSVAL(jsstring);
}


PRBool 
nsConvertJSValToObject(nsISupports** aSupports,
		       REFNSIID aIID,
		       const nsString& aTypeName,
		       JSContext* aContext,
		       jsval aValue)
{
  if (JSVAL_IS_NULL(aValue)) {
    *aSupports = nsnull;
  }
  else if (JSVAL_IS_OBJECT(aValue)) {
    JSObject *jsobj = JSVAL_TO_OBJECT(aValue); 
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
    JS_ReportError(aContext, "Parameter must be an object");
    return JS_FALSE;
  }

  return JS_TRUE;
}

void 
nsConvertJSValToString(nsString& aString,
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

PRBool
nsConvertJSValToBool(PRBool* aProp,
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

void 
nsGenericFinalize(JSContext* aContext,
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

JSBool 
nsGenericEnumerate(JSContext* aContext,
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

JSBool 
nsGenericResolve(JSContext* aContext,
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

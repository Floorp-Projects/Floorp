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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMTextRange.h"
#include "nsIDOMTextRangeList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kITextRangeIID, NS_IDOMTEXTRANGE_IID);
static NS_DEFINE_IID(kITextRangeListIID, NS_IDOMTEXTRANGELIST_IID);

NS_DEF_PTR(nsIDOMTextRange);
NS_DEF_PTR(nsIDOMTextRangeList);

//
// TextRange property ids
//
enum TextRange_slots {
  TEXTRANGE_RANGESTART = -1,
  TEXTRANGE_RANGEEND = -2,
  TEXTRANGE_RANGETYPE = -3,
  TEXTRANGELIST_LENGTH = -4
};

/***********************************************************************/
//
// TextRange Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetTextRangeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMTextRange *a = (nsIDOMTextRange*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case TEXTRANGE_RANGESTART:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "textrange.rangestart", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        if (NS_OK == a->GetRangeStart(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case TEXTRANGE_RANGEEND:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "textrange.rangeend", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        if (NS_OK == a->GetRangeEnd(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case TEXTRANGE_RANGETYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "textrange.rangetype", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        if (NS_OK == a->GetRangeType(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case TEXTRANGELIST_LENGTH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "textrangelist.length", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        nsIDOMTextRangeList* b;
        if (NS_OK == a->QueryInterface(kITextRangeListIID, (void **)&b)) {
          if(NS_OK == b->GetLength(&prop)) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type TextRangeList");
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// TextRange Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetTextRangeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMTextRange *a = (nsIDOMTextRange*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case TEXTRANGE_RANGESTART:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "textrange.rangestart", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRUint16)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetRangeStart(prop);
        
        break;
      }
      case TEXTRANGE_RANGEEND:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "textrange.rangeend", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRUint16)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetRangeEnd(prop);
        
        break;
      }
      case TEXTRANGE_RANGETYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "textrange.rangetype", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRUint16)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetRangeType(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// TextRange finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeTextRange(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// TextRange enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateTextRange(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// TextRange resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveTextRange(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
TextRangeListItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTextRange *privateThis = (nsIDOMTextRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMTextRangeList *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kITextRangeListIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type TextRangeList");
    return JS_FALSE;
  }

  nsIDOMTextRange* nativeRet;
  PRUint32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "textrangelist.item", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }
  else {
    return JS_FALSE;
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Item(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function Item requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for TextRange
//
JSClass TextRangeClass = {
  "TextRange", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetTextRangeProperty,
  SetTextRangeProperty,
  EnumerateTextRange,
  ResolveTextRange,
  JS_ConvertStub,
  FinalizeTextRange
};


//
// TextRange class properties
//
static JSPropertySpec TextRangeProperties[] =
{
  {"rangeStart",    TEXTRANGE_RANGESTART,    JSPROP_ENUMERATE},
  {"rangeEnd",    TEXTRANGE_RANGEEND,    JSPROP_ENUMERATE},
  {"rangeType",    TEXTRANGE_RANGETYPE,    JSPROP_ENUMERATE},
  {"length",    TEXTRANGELIST_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// TextRange class methods
//
static JSFunctionSpec TextRangeMethods[] = 
{
  {"Item",          TextRangeListItem,     1},
  {0}
};


//
// TextRange constructor
//
PR_STATIC_CALLBACK(JSBool)
TextRange(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// TextRange class initialization
//
extern "C" NS_DOM nsresult NS_InitTextRangeClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "TextRange", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &TextRangeClass,      // JSClass
                         TextRange,            // JSNative ctor
                         0,             // ctor args
                         TextRangeProperties,  // proto props
                         TextRangeMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "TextRange", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMTextRange::TEXTRANGE_CARETPOSITION);
      JS_SetProperty(jscontext, constructor, "TEXTRANGE_CARETPOSITION", &vp);

      vp = INT_TO_JSVAL(nsIDOMTextRange::TEXTRANGE_RAWINPUT);
      JS_SetProperty(jscontext, constructor, "TEXTRANGE_RAWINPUT", &vp);

      vp = INT_TO_JSVAL(nsIDOMTextRange::TEXTRANGE_SELECTEDRAWTEXT);
      JS_SetProperty(jscontext, constructor, "TEXTRANGE_SELECTEDRAWTEXT", &vp);

      vp = INT_TO_JSVAL(nsIDOMTextRange::TEXTRANGE_CONVERTEDTEXT);
      JS_SetProperty(jscontext, constructor, "TEXTRANGE_CONVERTEDTEXT", &vp);

      vp = INT_TO_JSVAL(nsIDOMTextRange::TEXTRANGE_SELECTEDCONVERTEDTEXT);
      JS_SetProperty(jscontext, constructor, "TEXTRANGE_SELECTEDCONVERTEDTEXT", &vp);

    }

  }
  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) {
    proto = JSVAL_TO_OBJECT(vp);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (aPrototype) {
    *aPrototype = proto;
  }
  return NS_OK;
}


//
// Method for creating a new TextRange JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptTextRange(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptTextRange");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMTextRange *aTextRange;

  if (nsnull == aParent) {
    parent = nsnull;
  }
  else if (NS_OK == aParent->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
    if (NS_OK != owner->GetScriptObject(aContext, (void **)&parent)) {
      NS_RELEASE(owner);
      return NS_ERROR_FAILURE;
    }
    NS_RELEASE(owner);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (NS_OK != NS_InitTextRangeClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kITextRangeIID, (void **)&aTextRange);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &TextRangeClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aTextRange);
  }
  else {
    NS_RELEASE(aTextRange);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}

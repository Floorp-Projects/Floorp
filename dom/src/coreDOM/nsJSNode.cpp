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
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNode.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNodeList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kINamedNodeMapIID, NS_IDOMNAMEDNODEMAP_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIEventTargetIID, NS_IDOMEVENTTARGET_IID);
static NS_DEFINE_IID(kINodeListIID, NS_IDOMNODELIST_IID);

NS_DEF_PTR(nsIDOMDocument);
NS_DEF_PTR(nsIDOMNamedNodeMap);
NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMEventListener);
NS_DEF_PTR(nsIDOMEventTarget);
NS_DEF_PTR(nsIDOMNodeList);

//
// Node property ids
//
enum Node_slots {
  NODE_NODENAME = -1,
  NODE_NODEVALUE = -2,
  NODE_NODETYPE = -3,
  NODE_PARENTNODE = -4,
  NODE_CHILDNODES = -5,
  NODE_FIRSTCHILD = -6,
  NODE_LASTCHILD = -7,
  NODE_PREVIOUSSIBLING = -8,
  NODE_NEXTSIBLING = -9,
  NODE_ATTRIBUTES = -10,
  NODE_OWNERDOCUMENT = -11
};

/***********************************************************************/
//
// Node Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetNodeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMNode *a = (nsIDOMNode*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case NODE_NODENAME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetNodeName(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_NODEVALUE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetNodeValue(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_NODETYPE:
      {
        PRUint16 prop;
        if (NS_OK == a->GetNodeType(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_PARENTNODE:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetParentNode(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_CHILDNODES:
      {
        nsIDOMNodeList* prop;
        if (NS_OK == a->GetChildNodes(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_FIRSTCHILD:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetFirstChild(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_LASTCHILD:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetLastChild(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_PREVIOUSSIBLING:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetPreviousSibling(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_NEXTSIBLING:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetNextSibling(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_ATTRIBUTES:
      {
        nsIDOMNamedNodeMap* prop;
        if (NS_OK == a->GetAttributes(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NODE_OWNERDOCUMENT:
      {
        nsIDOMDocument* prop;
        if (NS_OK == a->GetOwnerDocument(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// Node Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetNodeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMNode *a = (nsIDOMNode*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case NODE_NODEVALUE:
      {
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetNodeValue(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// Node finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeNode(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Node enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateNode(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Node resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveNode(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method InsertBefore
//
PR_STATIC_CALLBACK(JSBool)
NodeInsertBefore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *nativeThis = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;
  nsIDOMNodePtr b0;
  nsIDOMNodePtr b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->InsertBefore(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function insertBefore requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ReplaceChild
//
PR_STATIC_CALLBACK(JSBool)
NodeReplaceChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *nativeThis = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;
  nsIDOMNodePtr b0;
  nsIDOMNodePtr b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ReplaceChild(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function replaceChild requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveChild
//
PR_STATIC_CALLBACK(JSBool)
NodeRemoveChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *nativeThis = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->RemoveChild(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function removeChild requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AppendChild
//
PR_STATIC_CALLBACK(JSBool)
NodeAppendChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *nativeThis = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AppendChild(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function appendChild requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method HasChildNodes
//
PR_STATIC_CALLBACK(JSBool)
NodeHasChildNodes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *nativeThis = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRBool nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->HasChildNodes(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function hasChildNodes requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CloneNode
//
PR_STATIC_CALLBACK(JSBool)
NodeCloneNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *nativeThis = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;
  PRBool b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!nsJSUtils::nsConvertJSValToBool(&b0, cx, argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->CloneNode(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function cloneNode requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AddEventListener
//
PR_STATIC_CALLBACK(JSBool)
EventTargetAddEventListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *privateThis = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  nsIDOMEventTarget *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventTarget");
    return JS_FALSE;
  }

  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMEventListener* b1;
  PRBool b2;
  PRBool b3;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 4) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (!nsJSUtils::nsConvertJSValToFunc(&b1,
                                         cx,
                                         obj,
                                         argv[1])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b3, cx, argv[3])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AddEventListener(b0, b1, b2, b3)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function addEventListener requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveEventListener
//
PR_STATIC_CALLBACK(JSBool)
EventTargetRemoveEventListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *privateThis = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  nsIDOMEventTarget *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventTarget");
    return JS_FALSE;
  }

  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMEventListener* b1;
  PRBool b2;
  PRBool b3;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 4) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (!nsJSUtils::nsConvertJSValToFunc(&b1,
                                         cx,
                                         obj,
                                         argv[1])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b3, cx, argv[3])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->RemoveEventListener(b0, b1, b2, b3)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function removeEventListener requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Node
//
JSClass NodeClass = {
  "Node", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetNodeProperty,
  SetNodeProperty,
  EnumerateNode,
  ResolveNode,
  JS_ConvertStub,
  FinalizeNode
};


//
// Node class properties
//
static JSPropertySpec NodeProperties[] =
{
  {"nodeName",    NODE_NODENAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"nodeValue",    NODE_NODEVALUE,    JSPROP_ENUMERATE},
  {"nodeType",    NODE_NODETYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"parentNode",    NODE_PARENTNODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"childNodes",    NODE_CHILDNODES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"firstChild",    NODE_FIRSTCHILD,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"lastChild",    NODE_LASTCHILD,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"previousSibling",    NODE_PREVIOUSSIBLING,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"nextSibling",    NODE_NEXTSIBLING,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"attributes",    NODE_ATTRIBUTES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"ownerDocument",    NODE_OWNERDOCUMENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Node class methods
//
static JSFunctionSpec NodeMethods[] = 
{
  {"insertBefore",          NodeInsertBefore,     2},
  {"replaceChild",          NodeReplaceChild,     2},
  {"removeChild",          NodeRemoveChild,     1},
  {"appendChild",          NodeAppendChild,     1},
  {"hasChildNodes",          NodeHasChildNodes,     0},
  {"cloneNode",          NodeCloneNode,     1},
  {"addEventListener",          EventTargetAddEventListener,     4},
  {"removeEventListener",          EventTargetRemoveEventListener,     4},
  {0}
};


//
// Node constructor
//
PR_STATIC_CALLBACK(JSBool)
Node(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Node class initialization
//
extern "C" NS_DOM nsresult NS_InitNodeClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Node", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &NodeClass,      // JSClass
                         Node,            // JSNative ctor
                         0,             // ctor args
                         NodeProperties,  // proto props
                         NodeMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "Node", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMNode::ELEMENT_NODE);
      JS_SetProperty(jscontext, constructor, "ELEMENT_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::ATTRIBUTE_NODE);
      JS_SetProperty(jscontext, constructor, "ATTRIBUTE_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::TEXT_NODE);
      JS_SetProperty(jscontext, constructor, "TEXT_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::CDATA_SECTION_NODE);
      JS_SetProperty(jscontext, constructor, "CDATA_SECTION_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::ENTITY_REFERENCE_NODE);
      JS_SetProperty(jscontext, constructor, "ENTITY_REFERENCE_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::ENTITY_NODE);
      JS_SetProperty(jscontext, constructor, "ENTITY_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::PROCESSING_INSTRUCTION_NODE);
      JS_SetProperty(jscontext, constructor, "PROCESSING_INSTRUCTION_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::COMMENT_NODE);
      JS_SetProperty(jscontext, constructor, "COMMENT_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::DOCUMENT_NODE);
      JS_SetProperty(jscontext, constructor, "DOCUMENT_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::DOCUMENT_TYPE_NODE);
      JS_SetProperty(jscontext, constructor, "DOCUMENT_TYPE_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::DOCUMENT_FRAGMENT_NODE);
      JS_SetProperty(jscontext, constructor, "DOCUMENT_FRAGMENT_NODE", &vp);

      vp = INT_TO_JSVAL(nsIDOMNode::NOTATION_NODE);
      JS_SetProperty(jscontext, constructor, "NOTATION_NODE", &vp);

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
// Method for creating a new Node JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptNode(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptNode");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMNode *aNode;

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

  if (NS_OK != NS_InitNodeClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kINodeIID, (void **)&aNode);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &NodeClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aNode);
  }
  else {
    NS_RELEASE(aNode);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}

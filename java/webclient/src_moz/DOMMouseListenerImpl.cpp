/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 *               
 */

#include "DOMMouseListenerImpl.h"

#include "nsString.h"
#include "nsFileSpec.h" // for nsAutoCString
#include "nsIDOMNamedNodeMap.h"
#include "DOMMouseListenerImpl.h"
#include "jni_util.h"
#include "dom_util.h"
#include "nsActions.h"

#include "prprf.h" // for PR_snprintf

#include "prlog.h" // for PR_ASSERT
#include "ns_globals.h" // for prLogModuleInfo and gComponentManager

static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);

//
// Local functions
//

jlong DOMMouseListenerImpl::maskValues[] = { -1L };

char *DOMMouseListenerImpl::maskNames[] = {
    "MOUSE_DOWN_EVENT_MASK",
    "MOUSE_UP_EVENT_MASK",
    "MOUSE_CLICK_EVENT_MASK",
    "MOUSE_DOUBLE_CLICK_EVENT_MASK",
    "MOUSE_OVER_EVENT_MASK",
    "MOUSE_OUT_EVENT_MASK",
    nsnull
};

NS_IMPL_ADDREF(DOMMouseListenerImpl);
NS_IMPL_RELEASE(DOMMouseListenerImpl);  

DOMMouseListenerImpl::DOMMouseListenerImpl(){
}

DOMMouseListenerImpl::DOMMouseListenerImpl(JNIEnv *env,
                                           WebShellInitContext *yourInitContext,
                                           jobject yourTarget) :
    mJNIEnv(env), mInitContext(yourInitContext), mTarget(yourTarget)
{
    if (nsnull == gVm) { // declared in jni_util.h
        ::util_GetJavaVM(env, &gVm);  // save this vm reference away for the callback!
    }
#ifndef BAL_INTERFACE
    PR_ASSERT(gVm);
#endif
    
    if (-1 == maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/WCMouseEvent",
                                                maskNames, maskValues);
    }
    mRefCnt = 1; // PENDING(edburns): not sure about how right this is to do.
}

NS_IMETHODIMP DOMMouseListenerImpl::QueryInterface(REFNSIID aIID, void** aInstance)
{
  if (nsnull == aInstance)
    return NS_ERROR_NULL_POINTER;

  *aInstance = nsnull;

 
  if (aIID.Equals(NS_GET_IID(nsIDOMMouseListener))) {
      *aInstance = (void*) ((nsIDOMMouseListener*)this);
      NS_ADDREF_THIS();
      return NS_OK;
  }
  

  return NS_NOINTERFACE;
}

/* nsIDOMEventListener methods */

nsresult DOMMouseListenerImpl::HandleEvent(nsIDOMEvent* aEvent)
{
    return NS_OK;
}

  /* nsIDOMMouseListener methods */
nsresult DOMMouseListenerImpl::MouseDown(nsIDOMEvent *aMouseEvent) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DOMMouseListenerImpl::MouseDown\n"));
    }
#endif
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mTarget, 
                         maskValues[MOUSE_DOWN_EVENT_MASK], 
                         properties);
    util_DestroyPropertiesObject(mInitContext->env, properties, nsnull);
    return NS_OK;
}

nsresult DOMMouseListenerImpl::MouseUp(nsIDOMEvent *aMouseEvent) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DOMMouseListenerImpl::MouseUp\n"));
    }
#endif
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mTarget, 
                         maskValues[MOUSE_UP_EVENT_MASK], 
                         properties);
    util_DestroyPropertiesObject(mInitContext->env, properties, nsnull);
    return NS_OK;
}

nsresult DOMMouseListenerImpl::MouseClick(nsIDOMEvent *aMouseEvent) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DOMMouseListenerImpl::MouseClick\n"));
    }
#endif
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mTarget, 
                         maskValues[MOUSE_CLICK_EVENT_MASK], 
                         properties);
    util_DestroyPropertiesObject(mInitContext->env, properties, nsnull);
    return NS_OK;
}

nsresult DOMMouseListenerImpl::MouseDblClick(nsIDOMEvent *aMouseEvent) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DOMMouseListenerImpl::MouseDoubleClick\n"));
    }
#endif
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mTarget, 
                         maskValues[MOUSE_DOUBLE_CLICK_EVENT_MASK], 
                         properties);
    util_DestroyPropertiesObject(mInitContext->env, properties, nsnull);
    return NS_OK;
}

nsresult DOMMouseListenerImpl::MouseOver(nsIDOMEvent *aMouseEvent) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DOMMouseListenerImpl::MouseOver\n"));
    }
#endif
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mTarget, 
                         maskValues[MOUSE_OVER_EVENT_MASK], 
                         properties);
    util_DestroyPropertiesObject(mInitContext->env, properties, nsnull);

    return NS_OK;
}

nsresult DOMMouseListenerImpl::MouseOut(nsIDOMEvent *aMouseEvent) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DOMMouseListenerImpl::MouseOut\n"));
    }
#endif
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, 
                         mTarget, 
                         maskValues[MOUSE_OUT_EVENT_MASK], 
                         properties);
    util_DestroyPropertiesObject(mInitContext->env, properties, nsnull);
    return NS_OK;
}

jobject JNICALL DOMMouseListenerImpl::getPropertiesFromEvent(nsIDOMEvent *aMouseEvent)
{
    nsCOMPtr<nsIDOMNode> currentNode;
    nsresult rv = NS_OK;;

    rv = aMouseEvent->GetTarget(getter_AddRefs(currentNode));
    if (NS_FAILED(rv)) {
        return properties;
    }
    if (nsnull == currentNode) {
        return properties;
    }
    inverseDepth = 0;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
    
    properties = util_CreatePropertiesObject(env, nsnull);
    dom_iterateToRoot(currentNode, DOMMouseListenerImpl::takeActionOnNode, 
                      (void *)this);
    return properties;
}

nsresult JNICALL DOMMouseListenerImpl::takeActionOnNode(nsCOMPtr<nsIDOMNode> currentNode,
                                                        void *myObject)
{
    nsresult rv = NS_OK;
    nsString nodeInfo, nodeName, nodeValue, nodeDepth;
    jstring jNodeName, jNodeValue;
    PRUint32 depth = -1;
    DOMMouseListenerImpl *curThis = nsnull;
    const PRUint32 depthStrLen = 20;
    char depthStr[depthStrLen];
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);

    PR_ASSERT(nsnull != myObject);
    curThis = (DOMMouseListenerImpl *) myObject;
    depth = curThis->inverseDepth++; 

    if (!(curThis->properties)) {
        return rv;
    }
    // encode the depth of the node
    PR_snprintf(depthStr, depthStrLen, "depthFromLeaf:%d", depth);
    nodeDepth = (const char *) depthStr;

    // Store the name and the value of this node

    {
        // get the name and prepend the depth
        rv = currentNode->GetNodeName(nodeInfo);
        if (NS_FAILED(rv)) {
            return rv;
        }
        //        nodeName = nodeDepth;
        //        nodeName += nodeInfo;
        nodeName = nodeInfo;
        
        if (prLogModuleInfo) {
            nsAutoCString nodeInfoCStr(nodeName);
            PR_LOG(prLogModuleInfo, 3, ("%s", (const char *)nodeInfoCStr));
        }
        
        rv = currentNode->GetNodeValue(nodeInfo);
        if (NS_FAILED(rv)) {
            return rv;
        }
        //        nodeValue = nodeDepth;
        //        nodeValue += nodeInfo;
        nodeValue = nodeInfo;
        
        if (prLogModuleInfo) {
            nsAutoCString nodeInfoCStr(nodeValue);
            PR_LOG(prLogModuleInfo, 3, ("%s", (const char *)nodeInfoCStr));
        }
        
        jNodeName = ::util_NewString(env, nodeName.GetUnicode(), 
                                     nodeName.Length());
        jNodeValue = ::util_NewString(env, nodeValue.GetUnicode(), 
                                      nodeValue.Length());
        
        util_StoreIntoPropertiesObject(env, (jobject) curThis->properties,
                                       (jobject) jNodeName, (jobject) jNodeValue);
        if (jNodeName) {
            ::util_DeleteString(env, jNodeName);
        }
        if (jNodeValue) {
            ::util_DeleteString(env, jNodeValue);
        }
    } // end of Storing the name and value of this node

    // store any attributes of this node
    {
        nsCOMPtr<nsIDOMNamedNodeMap> nodeMap;
        rv = currentNode->GetAttributes(getter_AddRefs(nodeMap));
        if (NS_FAILED(rv) || !nodeMap) {
            return rv;
        }
        PRUint32 length, i;
        rv = nodeMap->GetLength(&length);
        if (NS_FAILED(rv)) {
            return rv;
        }
        for (i = 0; i < length; i++) {
            rv = nodeMap->Item(i, getter_AddRefs(currentNode));
            
            if (nsnull == currentNode) {
                return rv;
            }
            
            rv = currentNode->GetNodeName(nodeInfo);
            if (NS_FAILED(rv)) {
                return rv;
            }
            //            nodeName = nodeDepth;
            //            nodeName += nodeInfo;
            nodeName = nodeInfo;

            if (prLogModuleInfo) {
                nsAutoCString nodeInfoCStr(nodeName);
                PR_LOG(prLogModuleInfo, 3, 
                       ("attribute[%d], %s", i, (const char *)nodeInfoCStr));
            }
            
            rv = currentNode->GetNodeValue(nodeInfo);
            if (NS_FAILED(rv)) {
                return rv;
            }
            //            nodeValue = nodeDepth;
            //            nodeValue += nodeInfo;
            nodeValue = nodeInfo;
            
            if (prLogModuleInfo) {
                nsAutoCString nodeInfoCStr(nodeValue);
                PR_LOG(prLogModuleInfo, 3, 
                       ("attribute[%d] %s", i,(const char *)nodeInfoCStr));
            }
            jNodeName = ::util_NewString(env, nodeName.GetUnicode(), 
                                         nodeName.Length());
            jNodeValue = ::util_NewString(env, nodeValue.GetUnicode(), 
                                          nodeValue.Length());
            
            util_StoreIntoPropertiesObject(env, (jobject) curThis->properties,
                                           (jobject) jNodeName, (jobject) jNodeValue);
            if (jNodeName) {
                ::util_DeleteString(env, jNodeName);
            }
            if (jNodeValue) {
                ::util_DeleteString(env, jNodeValue);
            }
        }
    } // end of storing the attributes
        
    return rv;
}





/*
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <nsCOMPtr.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMNamedNodeMap.h>

#include "nsIDOMKeyEvent.h"
#include "nsString.h"

#include "EmbedEventListener.h"
#include "NativeBrowserControl.h"

#include "ns_globals.h" // for prLogModuleInfo
#include "dom_util.h" // for dom_iterateToRoot
#include <stdlib.h>


EmbedEventListener::EmbedEventListener(void) : mOwner(nsnull), mEventRegistration(nsnull), mProperties(nsnull), mInverseDepth(-1)
{
    if (-1 == DOMMouseListener_maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/WCMouseEvent",
                                                DOMMouseListener_maskNames, 
                                                DOMMouseListener_maskValues);
    }
}

EmbedEventListener::~EmbedEventListener()
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    if (mProperties) {
        ::util_DeleteGlobalRef(env, mProperties);
    }
    if (mEventRegistration) {
	JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
	::util_DeleteGlobalRef(env, mEventRegistration);
	mEventRegistration = nsnull;
    }
    
    mInverseDepth = -1;
    mOwner = nsnull;
}

NS_IMPL_ADDREF(EmbedEventListener)
NS_IMPL_RELEASE(EmbedEventListener)
NS_INTERFACE_MAP_BEGIN(EmbedEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
NS_INTERFACE_MAP_END

nsresult
EmbedEventListener::Init(NativeBrowserControl *aOwner)
{
    mOwner = aOwner;
    mProperties = nsnull;
    return NS_OK;
}

// All of the event listeners below return NS_OK to indicate that the
// event should not be consumed in the default case.

NS_IMETHODIMP
EmbedEventListener::HandleEvent(nsIDOMEvent* aDOMEvent)
{
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyDown(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMKeyEvent> keyEvent;
    keyEvent = do_QueryInterface(aDOMEvent);
    if (!keyEvent)
	return NS_OK;
    // Return FALSE to this function to mark the event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_KEY_DOWN],
		  (void *)keyEvent, &return_val);
    **********/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyUp(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMKeyEvent> keyEvent;
    keyEvent = do_QueryInterface(aDOMEvent);
    if (!keyEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /***************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_KEY_UP],
		  (void *)keyEvent, &return_val);
    ****************/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyPress(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMKeyEvent> keyEvent;
    keyEvent = do_QueryInterface(aDOMEvent);
    if (!keyEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /***********
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_KEY_PRESS],
		  (void *)keyEvent, &return_val);
    ************/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
    if (!aMouseEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;

    PopulatePropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(nsnull, 
                         mEventRegistration, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_DOWN_EVENT_MASK], 
                         mProperties);

    if (return_val) {
	aMouseEvent->StopPropagation();
	aMouseEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
    if (!aMouseEvent)
	return NS_OK;
    // Return FALSE to this function to mark the event as not
    // consumed...
    PRBool return_val = PR_FALSE;

    PopulatePropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(nsnull, 
                         mEventRegistration, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_UP_EVENT_MASK], 
                         mProperties);

    if (return_val) {
	aMouseEvent->StopPropagation();
	aMouseEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
    if (!aMouseEvent)
	return NS_OK;
    // Return FALSE to this function to mark the event as not
    // consumed...
    PRBool return_val = FALSE;

    PopulatePropertiesFromEvent(aMouseEvent);

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    ::util_StoreIntoPropertiesObject(env, mProperties,  CLICK_COUNT_KEY,
                                     ONE_VALUE, (jobject) 
                                     &(mOwner->GetWrapperFactory()->shareContext));

    util_SendEventToJava(nsnull, 
                         mEventRegistration, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_CLICK_EVENT_MASK], 
                         mProperties);

    if (return_val) {
	aMouseEvent->StopPropagation();
	aMouseEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
    if (!aMouseEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    ::util_StoreIntoPropertiesObject(env, mProperties,  CLICK_COUNT_KEY,
                                     TWO_VALUE, (jobject) 
                                     &(mOwner->GetWrapperFactory()->shareContext));

    util_SendEventToJava(nsnull, 
                         mEventRegistration, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_DOUBLE_CLICK_EVENT_MASK], 
                         mProperties);

    if (return_val) {
	aMouseEvent->StopPropagation();
	aMouseEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
    if (!aMouseEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;

    PopulatePropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(nsnull, 
                         mEventRegistration, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_OVER_EVENT_MASK], 
                         mProperties);

    if (return_val) {
	aMouseEvent->StopPropagation();
	aMouseEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
    if (!aMouseEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;

    PopulatePropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(nsnull, 
                         mEventRegistration, 
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_OUT_EVENT_MASK], 
                         mProperties);

    if (return_val) {
	aMouseEvent->StopPropagation();
	aMouseEvent->PreventDefault();
    }
    return NS_OK;
}

nsresult
EmbedEventListener::SetEventRegistration(jobject yourEventRegistration)
{
    nsresult rv = NS_OK;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    mEventRegistration = ::util_NewGlobalRef(env, yourEventRegistration);
    if (nsnull == mEventRegistration) {
        ::util_ThrowExceptionToJava(env, "Exception: EmbedEventListener->SetEventRegistration(): can't create NewGlobalRef\n\tfor eventRegistration");
	rv = NS_ERROR_FAILURE;
    }
    
    return rv;
}


nsresult EmbedEventListener::PopulatePropertiesFromEvent(nsIDOMEvent *event)
{
    nsCOMPtr<nsIDOMEventTarget> eventTarget;
    nsCOMPtr<nsIDOMNode> currentNode;
    nsCOMPtr<nsIDOMEvent> aMouseEvent = event;
    nsresult rv = NS_OK;;
    
    rv = aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
    if (NS_FAILED(rv)) {
        return rv;
    }
    currentNode = do_QueryInterface(eventTarget);
    if (!currentNode) {
        return rv;
    }
    mInverseDepth = 0;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (mProperties) {
        util_ClearPropertiesObject(env, mProperties, (jobject) 
                                   &(mOwner->GetWrapperFactory()->shareContext));
    }
    else {
        if (!(mProperties = 
              util_CreatePropertiesObject(env, (jobject)
                                          &(mOwner->GetWrapperFactory()->shareContext)))) {
            return rv;
        }
    }
    dom_iterateToRoot(currentNode, EmbedEventListener::takeActionOnNode, 
                      (void *)this);
    rv = addMouseEventDataToProperties(aMouseEvent);

    return rv;
}

nsresult EmbedEventListener::addMouseEventDataToProperties(nsIDOMEvent *aMouseEvent)
{
    // if the initialization failed, don't modify the mProperties
    if (!mProperties || !util_StringConstantsAreInitialized()) {
        return NS_ERROR_INVALID_ARG;
    }
    nsresult rv;

    // Add modifiers, keys, mouse buttons, etc, to the mProperties table
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
    
    rv = aMouseEvent->QueryInterface(nsIDOMMouseEvent::GetIID(),
                                     getter_AddRefs(mouseEvent));
    if (NS_FAILED(rv)) {
        return rv;
    }

    PRInt32 intVal;
    PRUint16 int16Val;
    PRBool boolVal;
    char buf[20];
    jstring strVal;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    // PENDING(edburns): perhaps use a macro to speed this up?
    rv = mouseEvent->GetScreenX(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, mProperties, SCREEN_X_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    
    rv = mouseEvent->GetScreenY(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, mProperties, SCREEN_Y_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    
    rv = mouseEvent->GetClientX(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, mProperties, CLIENT_X_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    
    rv = mouseEvent->GetClientY(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, mProperties, CLIENT_Y_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    
    int16Val = 0;
    rv = mouseEvent->GetButton(&int16Val);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(int16Val, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, mProperties, BUTTON_KEY,
                                         (jobject) strVal,
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    
    rv = mouseEvent->GetAltKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, mProperties, ALT_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    
    rv = mouseEvent->GetCtrlKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, mProperties, CTRL_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    
    rv = mouseEvent->GetShiftKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, mProperties, SHIFT_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    
    rv = mouseEvent->GetMetaKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, mProperties, META_KEY,
                                         (jobject) strVal, 
                                         (jobject) 
                                         &(mOwner->GetWrapperFactory()->shareContext));
    }
    return rv;
}

nsresult JNICALL EmbedEventListener::takeActionOnNode(nsCOMPtr<nsIDOMNode> currentNode,
						      void *myObject)
{
    nsresult rv = NS_OK;
    nsString nodeInfo, nodeName, nodeValue; //, nodeDepth;
    jstring jNodeName, jNodeValue;
    PRUint32 depth = 0;
    EmbedEventListener *curThis = nsnull;
    //const PRUint32 depthStrLen = 20;
    //    char depthStr[depthStrLen];
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    PR_ASSERT(nsnull != myObject);
    curThis = (EmbedEventListener *) myObject;
    depth = curThis->mInverseDepth++; 

    if (!(curThis->mProperties)) {
        return rv;
    }
    // encode the depth of the node
    //    PR_snprintf(depthStr, depthStrLen, "depthFromLeaf:%d", depth);
    //    nodeDepth = (const char *) depthStr;

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
            char * nodeInfoCStr = ToNewCString(nodeName);
            PR_LOG(prLogModuleInfo, 4, ("%s", nodeInfoCStr));
            nsMemory::Free(nodeInfoCStr);
        }
        
        rv = currentNode->GetNodeValue(nodeInfo);
        if (NS_FAILED(rv)) {
            return rv;
        }
        //        nodeValue = nodeDepth;
        //        nodeValue += nodeInfo;
        nodeValue = nodeInfo;
        
        if (prLogModuleInfo) {
            char * nodeInfoCStr = ToNewCString(nodeName);
            PR_LOG(prLogModuleInfo, 4, ("%s", (const char *)nodeInfoCStr));
            nsMemory::Free(nodeInfoCStr);
        }
        
        jNodeName = ::util_NewString(env, nodeName.get(), 
                                     nodeName.Length());
        jNodeValue = ::util_NewString(env, nodeValue.get(), 
                                      nodeValue.Length());
        
        util_StoreIntoPropertiesObject(env, (jobject) curThis->mProperties,
                                       (jobject) jNodeName, 
                                       (jobject) jNodeValue, 
                                       (jobject) 
                                       &(curThis->mOwner->GetWrapperFactory()->shareContext));
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
                char * nodeInfoCStr = ToNewCString(nodeName);
                PR_LOG(prLogModuleInfo, 4, 
                       ("attribute[%d], %s", i, (const char *)nodeInfoCStr));
                nsMemory::Free(nodeInfoCStr);
            }
            
            rv = currentNode->GetNodeValue(nodeInfo);
            if (NS_FAILED(rv)) {
                return rv;
            }
            //            nodeValue = nodeDepth;
            //            nodeValue += nodeInfo;
            nodeValue = nodeInfo;
            
            if (prLogModuleInfo) {
                char * nodeInfoCStr = ToNewCString(nodeName);
                PR_LOG(prLogModuleInfo, 4, 
                       ("attribute[%d] %s", i,(const char *)nodeInfoCStr));
                nsMemory::Free(nodeInfoCStr);
            }
            jNodeName = ::util_NewString(env, nodeName.get(), 
                                         nodeName.Length());
            jNodeValue = ::util_NewString(env, nodeValue.get(), 
                                          nodeValue.Length());
            
            util_StoreIntoPropertiesObject(env, (jobject) curThis->mProperties,
                                           (jobject) jNodeName, 
                                           (jobject) jNodeValue, 
                                           (jobject) 
                                           &(curThis->mOwner->GetWrapperFactory()->shareContext));
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

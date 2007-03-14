/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

#include "prlog.h"
#include "nsIDOMNode.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventTarget.h"
#include "javaDOMEventsGlobals.h"
#include "org_mozilla_dom_events_MouseEventImpl.h"

#include "nsCOMPtr.h"


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getAltKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetAltKey
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetAltKey: cannot QI to nsIDOMMouseEvent from nsIDOMEvent"));
        JavaDOMGlobals::ThrowException(env,
				       "MouseEvent.getAltKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool altKey = PR_FALSE;
    rv = event->GetAltKey(&altKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getAltKey: failed", rv);
        return JNI_FALSE;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetAltKey: result: %d", altKey));

    return (altKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;

}
  

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getButton
 * Signature: ()S
 */
JNIEXPORT jshort JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetButton
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetButton: cannot QI to nsIDOMMouseEvent from nsIDOMEvent"));
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getButton: NULL pointer");
        return 0;
    }

    PRUint16 code = 0;
    rv = event->GetButton(&code);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getButton: failed", rv);
        return 0;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetButton: result: %d", code));

    return (jshort) code;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getClientX
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetClientX
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetClientX: cannot QI to nsIDOMMouseEvent from nsIDOMEvent"));
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getClientX: NULL pointer");
        return JNI_FALSE;
    }

    PRInt32 clientX = 0;
    rv = event->GetClientX(&clientX);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getClientX: failed", rv);
        return 0;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetClientX: result: %d", clientX));

    return (jint) clientX;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getClientY
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetClientY
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetClientY: cannot QI to nsIDOMMouseEvent from nsIDOMEvent"));
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getClientY: NULL pointer");
        return JNI_FALSE;
    }

    PRInt32 clientY = 0;
    rv = event->GetClientY(&clientY);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getClientY: failed", rv);
        return 0;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetClientY: result: %d", clientY));

    return (jint) clientY;
}


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getCtrlKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetCtrlKey
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetCtrlKey: cannot QI to nsIDOMMouseEvent from nsIDOMEvent"));
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getCtrlKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool ctrlKey = PR_FALSE;
    rv = event->GetCtrlKey(&ctrlKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getCtrlKey: failed", rv);
        return JNI_FALSE;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetCtrlKey: result: %d", ctrlKey));

    return (ctrlKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getMetaKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetMetaKey
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetMetaKey: cannot QI to nsIDOMMouseEvent from nsIDOMEvent"));
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getMetaKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool metaKey = PR_FALSE;
    rv = event->GetMetaKey(&metaKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getMetaKey: failed", rv);
        return JNI_FALSE;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetMetaKey: result: %d", metaKey));

    return (metaKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getScreenX
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetScreenX
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetScreenX: cannot QI to nsIDOMMouseEvent from nsIDOMEvent"));
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getScreenX: NULL pointer");
        return JNI_FALSE;
    }

    PRInt32 screenX = 0;
    rv = event->GetScreenX(&screenX);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getScreenX: failed", rv);
        return 0;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetScreenX: result: %d", screenX));

    return (jint) screenX;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getScreenY
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetScreenY
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetScreenY: cannot QI to nsIDOMMouseEvent from nsIDOMEvent"));
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getScreenY: NULL pointer");
        return JNI_FALSE;
    }

    PRInt32 screenY = 0;
    rv = event->GetScreenY(&screenY);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getScreenY: failed", rv);
        return 0;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetScreenY: result: %d", screenY));

    return (jint) screenY;
}


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getShiftKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetShiftKey
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetShiftKey: eventPtr: %p", eventPtr));
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetShiftKey: QI nsCOMPtr<nsIDOMMouseEvent> from: %p, rv: %d", eventPtr, rv));

    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getShiftKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool shiftKey = PR_FALSE;
    rv = event->GetShiftKey(&shiftKey);
    if (NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::nativeGetShiftKey: rv: %d", rv));
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getShiftKey: failed", rv);
        return JNI_FALSE;
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::nativeGetShiftKey: result: %d", shiftKey));

    return (shiftKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getRelatedTarget
 * Signature: ()Lorg/w3c/dom/events/EventTarget;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeGetRelatedTarget
  (JNIEnv *env, jobject jthis)
{
    nsresult rv = NS_OK;
    nsIDOMEvent *eventPtr = (nsIDOMEvent*)
	env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    nsCOMPtr<nsIDOMMouseEvent> event =  do_QueryInterface(eventPtr, &rv); 

    if (!event || NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::getRelatedTarget: cannot QI nsIDOMMouseEvent from nsIDOMEvent"));
	JavaDOMGlobals::ThrowException(env,
				       "MouseEvent.getRelatedNode: NULL pointer");
	return NULL;
    }
    
    nsCOMPtr<nsIDOMEventTarget> ret = nsnull;
    rv = event->GetRelatedTarget(getter_AddRefs(ret));
    if (NS_FAILED(rv)) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::getRelatedTarget: cannot get related target"));

	JavaDOMGlobals::ThrowException(env,
				       "MouseEvent.getRelatedNode: failed", rv);
	return NULL;
    }
    if (!ret) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::getRelatedTarget: related target is null"));
	
	return NULL;
    }
    
    nsCOMPtr<nsIDOMNode> node = nsnull;
    node = do_QueryInterface(ret, &rv);
    jobject result = nsnull;
    if (NS_SUCCEEDED(rv) && node) {
	result = JavaDOMGlobals::CreateNodeSubtype(env, node);
    }
    else {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	       ("MouseEventImpl::getRelatedTarget: can't QI nsIDOMNode from nsIDOMEventTarget"));
	
    }
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("MouseEventImpl::getRelatedTarget: returning %p", result));
    
    return result;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    initMouseEvent
 * Signature: (Ljava/lang/String;ZZLorg/w3c/dom/views/AbstractView;IIIIIZZZZSLorg/w3c/dom/Node;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_events_MouseEventImpl_nativeInitMouseEvent
  (JNIEnv *env, jobject jthis, 
   jstring jtypeArg, 
   jboolean jcanBubbleArg, 
   jboolean jcancelableArg, 
   jobject jviewArg, 
   jint jdetailArg, 
   jint jscreenXArg, 
   jint jscreenYArg, 
   jint jclientXArg, 
   jint jclientYArg, 
   jboolean jctrlKeyArg, 
   jboolean jaltKeyArg, 
   jboolean jshiftKeyArg, 
   jboolean jmetaKeyArg, 
   jshort jbuttonArg, 
   jobject jrelatedNodeArg)
{
  nsIDOMMouseEvent* event = (nsIDOMMouseEvent*) 
    env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
  if (!event) {
     JavaDOMGlobals::ThrowException(env,
        "MouseEvent.initMouseEvent: NULL pointer");
    return;
  }

  nsString* cvalue = JavaDOMGlobals::GetUnicode(env, jtypeArg);
  if (!cvalue)
    return;

  PRBool canBubble   = jcanBubbleArg  == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool cancelable  = jcancelableArg == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool ctrlKeyArg  = jctrlKeyArg    == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool altKeyArg   = jaltKeyArg     == JNI_TRUE ? PR_TRUE : PR_FALSE; 
  PRBool shiftKeyArg = jshiftKeyArg   == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool metaKeyArg  = jmetaKeyArg    == JNI_TRUE ? PR_TRUE : PR_FALSE;

  nsresult rv = event->InitMouseEvent(*cvalue,
				      canBubble,
				      cancelable,
				      (nsIDOMAbstractView*) jviewArg,
				      (PRUint16)jdetailArg,
				      (PRInt32)jscreenXArg, 
				      (PRInt32)jscreenYArg, 
				      (PRInt32)jclientXArg, 
				      (PRInt32)jclientYArg, 			       
				      ctrlKeyArg,
				      altKeyArg,
				      shiftKeyArg,
				      metaKeyArg,
				      (PRUint16)jbuttonArg, 
				      (nsIDOMEventTarget*) jrelatedNodeArg);

  nsMemory::Free(cvalue);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
        "UIEvent.initUIEvent: failed", rv);
  }

}



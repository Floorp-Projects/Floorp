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
#include "nsIDOMMouseEvent.h"
#include "javaDOMEventsGlobals.h"
#include "org_mozilla_dom_events_MouseEventImpl.h"

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getAltKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getAltKey
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*) 
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getAltKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool altKey = PR_FALSE;
    nsresult rv = event->GetAltKey(&altKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getAltKey: failed", rv);
        return JNI_FALSE;
    }

    return (altKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;

}
  

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getButton
 * Signature: ()S
 */
JNIEXPORT jshort JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getButton
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getButton: NULL pointer");
        return 0;
    }

    PRUint16 code = 0;
    nsresult rv = event->GetButton(&code);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getButton: failed", rv);
        return 0;
    }

    return (jshort) code;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getClientX
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getClientX
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getClientX: NULL pointer");
        return 0;
    }

    PRInt32 clientX = 0;
    nsresult rv = event->GetClientX(&clientX);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getClientX: failed", rv);
        return 0;
    }

    return (jint) clientX;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getClientY
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getClientY
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getClientY: NULL pointer");
        return 0;
    }

    PRInt32 clientY = 0;
    nsresult rv = event->GetClientY(&clientY);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getClientY: failed", rv);
        return 0;
    }

    return (jint) clientY;
}


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getCtrlKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getCtrlKey
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*) 
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getCtrlKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool ctrlKey = PR_FALSE;
    nsresult rv = event->GetCtrlKey(&ctrlKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getCtrlKey: failed", rv);
        return JNI_FALSE;
    }

    return (ctrlKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getMetaKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getMetaKey
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*) 
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getMetaKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool metaKey = PR_FALSE;
    nsresult rv = event->GetMetaKey(&metaKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getMetaKey: failed", rv);
        return JNI_FALSE;
    }

    return (metaKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getScreenX
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getScreenX
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getScreenX: NULL pointer\n");
        return 0;
    }

    PRInt32 screenX = 0;
    nsresult rv = event->GetScreenX(&screenX);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getScreenX: failed", rv);
        return 0;
    }

    return (jint) screenX;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getScreenY
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getScreenY
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getScreenY: NULL pointer");
        return 0;
    }

    PRInt32 screenY = 0;
    nsresult rv = event->GetScreenY(&screenY);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getScreenY: failed", rv);
        return 0;
    }

    return (jint) screenY;
}


/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getShiftKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getShiftKey
  (JNIEnv *env, jobject jthis)
{
    nsIDOMMouseEvent* event = (nsIDOMMouseEvent*) 
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getShiftKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool shiftKey = PR_FALSE;
    nsresult rv = event->GetShiftKey(&shiftKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "MouseEvent.getShiftKey: failed", rv);
        return JNI_FALSE;
    }

    return (shiftKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    getRelatedNode
 * Signature: ()Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_events_MouseEventImpl_getRelatedNode
  (JNIEnv *env, jobject jthis)
{
  nsIDOMMouseEvent* event = (nsIDOMMouseEvent*) 
    env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
  if (!event) {
    JavaDOMGlobals::ThrowException(env,
        "MouseEvent.getRelatedNode: NULL pointer");
    return NULL;
  }

  nsIDOMNode* node = nsnull;
  nsresult rv = event->GetRelatedNode(&node);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
        "MouseEvent.getRelatedNode: failed", rv);
    return NULL;
  }
  if (!node)
    return NULL;

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
}

/*
 * Class:     org_mozilla_dom_events_MouseEventImpl
 * Method:    initMouseEvent
 * Signature: (Ljava/lang/String;ZZLorg/w3c/dom/views/AbstractView;IIIIIZZZZSLorg/w3c/dom/Node;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_events_MouseEventImpl_initMouseEvent
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

  jboolean iscopy = JNI_FALSE;
  const char* cvalue = env->GetStringUTFChars(jtypeArg, &iscopy);
  if (!cvalue) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("UIEvent.initUIEvent: GetStringUTFChars failed"));
    return;
  }

  PRBool canBubble   = jcanBubbleArg  == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool cancelable  = jcancelableArg == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool ctrlKeyArg  = jctrlKeyArg    == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool altKeyArg   = jaltKeyArg     == JNI_TRUE ? PR_TRUE : PR_FALSE; 
  PRBool shiftKeyArg = jshiftKeyArg   == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool metaKeyArg  = jmetaKeyArg    == JNI_TRUE ? PR_TRUE : PR_FALSE;

  nsresult rv = event->InitMouseEvent(cvalue,
				      ctrlKeyArg, 
				      altKeyArg, 
				      shiftKeyArg, 
				      metaKeyArg, 
				      (PRInt32)jscreenXArg, 
				      (PRInt32)jscreenYArg, 
				      (PRInt32)jclientXArg, 
				      (PRInt32)jclientYArg, 
				      (PRUint16)jbuttonArg, 
				      (PRUint16)jdetailArg);

  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jtypeArg, cvalue);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
        "UIEvent.initUIEvent: failed", rv);
  }

}



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
#include "nsIDOMUIEvent.h"
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*) 
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
            "MouseEvent.getAltKey: failed");
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*)
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
            "MouseEvent.getButton: failed");
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*)
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
            "MouseEvent.getClientX: failed");
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*)
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
            "MouseEvent.getClientY: failed");
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*) 
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
            "MouseEvent.getCtrlKey: failed");
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*) 
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
            "MouseEvent.getMetaKey: failed");
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*)
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
            "MouseEvent.getScreenX: failed");
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*)
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
            "MouseEvent.getScreenY: failed");
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
    nsIDOMUIEvent* event = (nsIDOMUIEvent*) 
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
            "MouseEvent.getShiftKey: failed");
        return JNI_FALSE;
    }

    return (shiftKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}



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
#include"nsIDOMEvent.h"
#include"javaDOMEventsGlobals.h"
#include "org_mozilla_dom_events_EventImpl.h"

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    getCurrentNode
 * Signature: ()Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_events_EventImpl_getCurrentNode
  (JNIEnv *env, jobject jthis)
{
    nsIDOMEvent* event = (nsIDOMEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getCurrentNode: NULL pointer\n");
        return NULL;
    }

    nsIDOMNode* ret = nsnull;
    nsresult rv = event->GetCurrentNode(&ret);
    if (NS_FAILED(rv) || !ret) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getCurrentNode: failed", rv);
        return NULL;
    }

    return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    getEventPhase
 * Signature: ()S
 */
JNIEXPORT jshort JNICALL Java_org_mozilla_dom_events_EventImpl_getEventPhase
  (JNIEnv *env, jobject jthis)
{
    nsIDOMEvent* event = (nsIDOMEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getEventPhase: NULL native pointer\n");
        return 0;
    }

    PRUint16 eventPhase = 0;
    nsresult rv = event->GetEventPhase(&eventPhase);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getEventPhase: failed", rv);
        return 0;
    }

    jfieldID phaseFID = NULL;
    switch(eventPhase) {
        case nsIDOMEvent::BUBBLING_PHASE:
            phaseFID = JavaDOMEventsGlobals::eventPhaseBubblingFID;
            break;
        case nsIDOMEvent::CAPTURING_PHASE:
            phaseFID = JavaDOMEventsGlobals::eventPhaseCapturingFID;
            break;
        case nsIDOMEvent::AT_TARGET:
            phaseFID = JavaDOMEventsGlobals::eventPhaseAtTargetFID;
            break;
    }

    jshort ret = 0;
    if (phaseFID) {
        ret = env->GetStaticShortField(JavaDOMEventsGlobals::eventClass, phaseFID);
        if (env->ExceptionOccurred()) {
            PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
                ("Event.getEventPhase: slection of phaseFID failed\n"));
            return ret;
        }

    } else {
        JavaDOMGlobals::ThrowException(env,
            "Event.getEventPhase: illegal phase");
    }

    return ret;
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    getBubbles
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_EventImpl_getBubbles
  (JNIEnv *env, jobject jthis)
{
  nsIDOMEvent* event = (nsIDOMEvent*)
    env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
  if (!event) {
    JavaDOMGlobals::ThrowException(env,
	"Event.getBubbles: NULL pointer");
    return JNI_FALSE;
  }

  PRBool canBubble = PR_FALSE;
  nsresult rv = event->GetBubbles(&canBubble);
    if (NS_FAILED(rv)) {
      JavaDOMGlobals::ThrowException(env,
          "Event.getBubbles: failed", rv);
    return JNI_FALSE;
  }

  return (canBubble == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    getCancelable
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_EventImpl_getCancelable
  (JNIEnv *env, jobject jthis)
{
  nsIDOMEvent* event = (nsIDOMEvent*)
    env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
  if (!event) {
    JavaDOMGlobals::ThrowException(env,
        "Event.getCancelable: NULL pointer");
    return JNI_FALSE;
  }

  PRBool cancelable = PR_FALSE;
  nsresult rv = event->GetCancelable(&cancelable);
    if (NS_FAILED(rv)) {
      JavaDOMGlobals::ThrowException(env,
          "Event.getCancelable: failed", rv);
    return JNI_FALSE;
  }

  return (cancelable == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    getTarget
 * Signature: ()Lorg/w3c/dom/events/EventTarget;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_events_EventImpl_getTarget
  (JNIEnv *env, jobject jthis)
{
    nsIDOMEvent* event = (nsIDOMEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getTarget: NULL pointer");
        return NULL;
    }

    nsIDOMNode* ret = nsnull;
    nsresult rv = event->GetTarget(&ret);
    if (NS_FAILED(rv) || !ret) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getTarget: failed", rv);
        return NULL;
    }

    return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    getType
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_events_EventImpl_getType
  (JNIEnv *env, jobject jthis)
{
    nsIDOMEvent* event = (nsIDOMEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getType: NULL pointer");
        return NULL;
    }

    nsString ret;
    nsresult rv = event->GetType(ret);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getType: failed", rv);
        return NULL;
    }

    jstring jret = env->NewString(ret.GetUnicode(), ret.Length());
    if (!jret) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getType: NewStringUTF failed");
        return NULL;
    }

    return jret;
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    preventBubble
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_events_EventImpl_preventBubble
  (JNIEnv *env, jobject jthis)
{
    nsIDOMEvent* event = (nsIDOMEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventBubble: NULL pointer");
        return;
    }

    nsresult rv = event->PreventBubble();
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventBubble: failed", rv);
    }
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    preventCapture
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_events_EventImpl_preventCapture
  (JNIEnv *env, jobject jthis)
{
    nsIDOMEvent* event = (nsIDOMEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventCapture: NULL pointer");
        return;
    }

    nsresult rv = event->PreventCapture();
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventCapture: failed", rv);
    }
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    preventDefault
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_events_EventImpl_preventDefault
  (JNIEnv *env, jobject jthis)
{
    nsIDOMEvent* event = (nsIDOMEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventDefault: NULL pointer");
        return;
    }

    nsresult rv = event->PreventDefault();
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventDefault: failed", rv);
    }
}

/*
 * Class:     org_mozilla_dom_events_EventImpl
 * Method:    initEvent
 * Signature: (Ljava/lang/String;ZZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_events_EventImpl_initEvent
  (JNIEnv *env, jobject jthis, jstring jeventTypeArg, jboolean jcanBubbleArg, jboolean jcancelableArg)
{
  nsIDOMEvent* event = (nsIDOMEvent*)
    env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
  if (!event || !jeventTypeArg) {
    JavaDOMGlobals::ThrowException(env,
        "Event.initEvent: NULL pointer");
    return;
  }

  jboolean iscopy = JNI_FALSE;
  const char* cvalue = env->GetStringUTFChars(jeventTypeArg, &iscopy);
  if (!cvalue) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Event.initEvent: GetStringUTFChars failed\n"));
    return;
  }

  PRBool canBubble = jcanBubbleArg == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool cancelable = jcancelableArg == JNI_TRUE ? PR_TRUE : PR_FALSE;

  nsresult rv = event->InitEvent(cvalue, canBubble, cancelable);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jeventTypeArg, cvalue);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Event.initEvent: failed (%x)\n", rv));
  }
}

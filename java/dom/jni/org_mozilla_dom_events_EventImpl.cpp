/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
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
            "Event.getCurrentNode: failed");
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
            "Event.getEventPhase: failed");
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
            "Event.getTarget: failed");
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
            "Event.getType: failed");
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

    nsIDOMNode* ret = nsnull;
    nsresult rv = event->PreventBubble();
    if (NS_FAILED(rv) || !ret) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventBubble: failed");
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

    nsIDOMNode* ret = nsnull;
    nsresult rv = event->PreventCapture();
    if (NS_FAILED(rv) || !ret) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventCapture: failed");
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

    nsIDOMNode* ret = nsnull;
    nsresult rv = event->PreventDefault();
    if (NS_FAILED(rv) || !ret) {
        JavaDOMGlobals::ThrowException(env,
            "Event.preventDefault: failed");
    }
}


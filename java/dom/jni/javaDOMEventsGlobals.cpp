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
#include "prmon.h"
#include "nsAutoLock.h"
#include "javaDOMEventsGlobals.h"
#include "nsIDOMEvent.h"
#include "nsIDOMUIEvent.h"

jclass JavaDOMEventsGlobals::eventClass = NULL;
jclass JavaDOMEventsGlobals::uiEventClass = NULL;
jclass JavaDOMEventsGlobals::eventListenerClass = NULL;
jclass JavaDOMEventsGlobals::eventTargetClass = NULL;
jclass JavaDOMEventsGlobals::mouseEventClass = NULL;
jclass JavaDOMEventsGlobals::mutationEventClass = NULL;

jfieldID JavaDOMEventsGlobals::eventPtrFID = NULL;
jfieldID JavaDOMEventsGlobals::eventTargetPtrFID = NULL;

jfieldID JavaDOMEventsGlobals::eventPhaseBubblingFID = NULL;
jfieldID JavaDOMEventsGlobals::eventPhaseCapturingFID = NULL;
jfieldID JavaDOMEventsGlobals::eventPhaseAtTargetFID = NULL;

jmethodID JavaDOMEventsGlobals::eventListenerHandleEventMID = NULL;

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMUIEventIID, NS_IDOMUIEVENT_IID);

void JavaDOMEventsGlobals::Initialize(JNIEnv *env) 
{
  eventClass = env->FindClass("org/mozilla/dom/events/EventImpl");
  if (!eventClass) {
      JavaDOMGlobals::ThrowException(env, "Class org.mozilla.dom.events.EventImpl not found");
      return;
  }
  eventClass = (jclass) env->NewGlobalRef(eventClass);
  if (!eventClass) 
      return;
  eventPtrFID = 
      env->GetFieldID(eventClass, "p_nsIDOMEvent", "J");
  if (!eventPtrFID) {
      JavaDOMGlobals::ThrowException(env, "There is no field p_nsIDOMEvent in org.mozilla.dom.events.EventImpl"); 
      return;
  }
  
  eventListenerClass = env->FindClass("org/w3c/dom/events/EventListener");
  if (!eventListenerClass) {
      JavaDOMGlobals::ThrowException(env, "Class org.w3c.dom.events.EventListener not found");
      return;
  }
  eventListenerClass = (jclass) env->NewGlobalRef(eventListenerClass);
  if (!eventListenerClass) 
      return;

  eventListenerHandleEventMID = env->GetMethodID(
                         eventListenerClass, "handleEvent", "(Lorg/w3c/dom/events/Event;)V");
  if (!eventListenerHandleEventMID) {
      JavaDOMGlobals::ThrowException(env, "There is no method handleEvent in org.w3c.dom.events.EventListener"); 
      return;
  }

  uiEventClass = env->FindClass("org/mozilla/dom/events/UIEventImpl");
  if (!uiEventClass) {
      JavaDOMGlobals::ThrowException(env, "Class org.mozilla.dom.events.UIEventImpl not found");
      return;
  }
  uiEventClass = (jclass) env->NewGlobalRef(uiEventClass);
  if (!uiEventClass) 
      return;

  eventPhaseBubblingFID = 
    env->GetStaticFieldID(eventClass, "BUBBLING_PHASE", "S");
  if (!eventPhaseBubblingFID) {
      JavaDOMGlobals::ThrowException(env, "There is no static field BUBBLING_PHASE in org.w3c.dom.events.Event"); 
      return;
  }

  eventPhaseCapturingFID = 
    env->GetStaticFieldID(eventClass, "CAPTURING_PHASE", "S");
  if (!eventPhaseCapturingFID) {
      JavaDOMGlobals::ThrowException(env, "There is no static field CAPTURING_PHASE in org.w3c.dom.events.Event"); 
      return;
  }

  eventPhaseAtTargetFID = 
    env->GetStaticFieldID(eventClass, "AT_TARGET", "S");
  if (!eventPhaseAtTargetFID) {
      JavaDOMGlobals::ThrowException(env, "There is no static field AT_TARGET in org.w3c.dom.events.Event"); 
      return;
  }

  mouseEventClass = env->FindClass("org/mozilla/dom/events/MouseEventImpl");
  if (!mouseEventClass) {
      JavaDOMGlobals::ThrowException(env, "Class org.mozilla.dom.events.MouseEventImpl not found");
      return;
  }
  mouseEventClass = (jclass) env->NewGlobalRef(mouseEventClass);
  if (!mouseEventClass) 
      return;
}

void JavaDOMEventsGlobals::Destroy(JNIEnv *env) 
{
  env->DeleteGlobalRef(eventClass);
  if (env->ExceptionOccurred()) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
          ("JavaDOMEventsGlobals::Destroy: failed to delete Event global ref %x\n", 
            eventClass));
      return;
  }
  eventClass = NULL;

  env->DeleteGlobalRef(eventListenerClass);
  if (env->ExceptionOccurred()) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
          ("JavaDOMEventsGlobals::Destroy: failed to delete EventListener global ref %x\n", 
           eventListenerClass));
      return;
  }
  eventListenerClass = NULL;

  env->DeleteGlobalRef(uiEventClass);
  if (env->ExceptionOccurred()) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
           ("JavaDOMEventsGlobals::Destroy: failed to delete UIEvent global ref %x\n", 
            uiEventClass));
      return;
  }
  uiEventClass = NULL;

  env->DeleteGlobalRef(mouseEventClass);
  if (env->ExceptionOccurred()) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
           ("JavaDOMEventsGlobals::Destroy: failed to delete mouseEvent global ref %x\n", 
            mouseEventClass));
      return;
  }
  mouseEventClass = NULL;

}

//returns true if specified event "type" exists in the given list of types
// NOTE: it is assumed that "types" list is enden with NULL
static jboolean isEventOfType(const char* const* types, const char* type) 
{
    int i=0;

    while (type && types && types[i]) {
      if (!strcmp(type,types[i]))
	return JNI_TRUE;
      i++;
    }

    return JNI_FALSE;
}

jobject JavaDOMEventsGlobals::CreateEventSubtype(JNIEnv *env, 
                      nsIDOMEvent *event) 
{
  jobject jevent;
  jclass clazz = eventClass;
  nsISupports *isupports;
  void *target;
  nsresult rv;
  
  isupports = (nsISupports *) event;

  //check whenever our Event is UIEvent
  rv = isupports->QueryInterface(kIDOMUIEventIID, (void **) &target);
  if (!NS_FAILED(rv) && target) {
    // At the moment DOM2 draft specifies set of UIEvent subclasses 
    // However Mozilla still presents these events as nsUIEvent
    // So we need a cludge to determine proper java class to be created
    
    static const char* const uiEventTypes[] = { 
      "resize", 
      "scroll", 
      "focusin", 
      "focusout", 
      "gainselection",
      "loseselection", 
      "activate", 
      NULL
    };
  
    static const char* const mouseEventTypes[] = { 
      "click", 
      "mousedown",
      "mouseup", 
      "mouseover",
      "mousemove", 
      "mouseout", 
      NULL
    };
  
    nsString eventType;
    rv = event->GetType(eventType);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getType at JavaDOMEventsGlobals: failed");
        return NULL;
    }

    char* buffer = eventType.ToNewUTF8String();

    if (isEventOfType(mouseEventTypes, buffer) == JNI_TRUE) {
      clazz = mouseEventClass;
    } else if (isEventOfType(uiEventTypes, buffer) == JNI_TRUE) {  
      clazz = uiEventClass;
    } else {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING,
	     ("Unknown type of UI event (%s)", buffer));

      clazz = uiEventClass;
    }
    nsMemory::Free(buffer);

    event->Release();
    event = (nsIDOMEvent *) target;
  }

  PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
      ("JavaDOMEventsGlobals::CreateEventSubtype: allocating Node of  clazz=%x\n", 
       clazz));

  jevent = env->AllocObject(clazz);
  if (!jevent) {
        JavaDOMGlobals::ThrowException(env,
            "JavaDOMEventsGlobals::CreateEventSubtype: failed to allocate Event object");
      return NULL;
  }

  env->SetLongField(jevent, eventPtrFID, (jlong) event);
  if (env->ExceptionOccurred()) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	      ("JavaDOMEventGlobals::CreateEventSubtype: failed to set native ptr %x\n", 
	      (jlong) event));
      return NULL;
  }
  event->AddRef();

  return jevent;
}


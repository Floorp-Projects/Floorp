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
#include"javaDOMEventsGlobals.h"
#include "org_mozilla_dom_events_UIEventImpl.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMAbstractView.h"

/*
 * Class:     org_mozilla_dom_events_UIEventImpl
 * Method:    getView
 * Signature: ()Lorg/w3c/dom/views/AbstractView;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_events_UIEventImpl_nativeGetView
  (JNIEnv *env, jobject jthis)
{
  nsIDOMUIEvent* event = (nsIDOMUIEvent*)
    env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
  if (!event) {
    JavaDOMGlobals::ThrowException(env,
	"Event.getCurrentNode: NULL pointer");
    return NULL;
  }

  nsIDOMAbstractView* aView = nsnull;
  nsresult rv = event->GetView(&aView);
  if (NS_FAILED(rv) || !aView) {
    JavaDOMGlobals::ThrowException(env,
	"UIEvent.getView: failed", rv);
    return NULL;
  }

  // REMIND: Abstract View
  return NULL; //JavaDOMEventGlobals::CreateEventSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_events_UIEventImpl
 * Method:    getDetail
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_UIEventImpl_nativeGetDetail
  (JNIEnv *env, jobject jthis)
{
  nsIDOMUIEvent* event = (nsIDOMUIEvent*)
    env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
  if (!event) {
    JavaDOMGlobals::ThrowException(env,
	"UIEvent.getDetail: NULL pointer");
    return 0;
  }

  PRInt32 aDetail = 0;
  nsresult rv = event->GetDetail(&aDetail);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
        "UIEvent.getDetail: failed", rv);
    return 0;
  }

  return (jint)aDetail;
}

/*
 * Class:     org_mozilla_dom_events_UIEventImpl
 * Method:    initUIEvent
 * Signature: (Ljava/lang/String;ZZLorg/w3c/dom/views/AbstractView;I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_events_UIEventImpl_nativeInitUIEvent
  (JNIEnv *env, jobject jthis, 
   jstring jtypeArg, 
   jboolean jcanBubbleArg, 
   jboolean jcancelableArg, 
   jobject jviewArg, 
   jint jdetailArg)
{
  nsIDOMUIEvent* event = (nsIDOMUIEvent*)
    env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
  if (!event || !jtypeArg || !jviewArg) {
    JavaDOMGlobals::ThrowException(env,
	"Event.initUIEvent: NULL pointer\n");
    return;
  }

  nsString* cvalue = JavaDOMGlobals::GetUnicode(env, jtypeArg);
  if (!cvalue)
    return;

  PRBool canBubble = jcanBubbleArg == JNI_TRUE ? PR_TRUE : PR_FALSE;
  PRBool cancelable = jcancelableArg == JNI_TRUE ? PR_TRUE : PR_FALSE;


  // REMIND: need to deal with AbstractView
  // NS_IMETHOD    InitUIEvent(const nsString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMAbstractView* aViewArg, PRInt32 aDetailArg)=0;
  nsresult rv = event->InitUIEvent(*cvalue, canBubble, cancelable, NULL, (PRUint32)jdetailArg);
  nsMemory::Free(cvalue);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
        "UIEvent.initUIEvent: failed", rv);
  }
}


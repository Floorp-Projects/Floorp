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
#include "org_mozilla_dom_events_KeyEventImpl.h"

/*
 * Class:     org_mozilla_dom_events_KeyEventImpl
 * Method:    getAltKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_KeyEventImpl_getAltKey
  (JNIEnv *env, jobject jthis)
{
    nsIDOMUIEvent* event = (nsIDOMUIEvent*) 
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getAltKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool altKey = PR_FALSE;
    nsresult rv = event->GetAltKey(&altKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getAltKey: failed");
        return JNI_FALSE;
    }

    return (altKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_events_KeyEventImpl
 * Method:    getCharCode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_KeyEventImpl_getCharCode
  (JNIEnv *env, jobject jthis)
{
    nsIDOMUIEvent* event = (nsIDOMUIEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getCharCode: NULL pointer");
        return 0;
    }

    PRUint32 code = 0;
    nsresult rv = event->GetCharCode(&code);
    if (NS_FAILED(rv)) {
        PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("KeyEvent.getCharCode: failed (%x)\n", rv));
        return 0;
    }

    return (jint) code;
}


/*
 * Class:     org_mozilla_dom_events_KeyEventImpl
 * Method:    getCtrlKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_KeyEventImpl_getCtrlKey
  (JNIEnv *env, jobject jthis)
{
    nsIDOMUIEvent* event = (nsIDOMUIEvent*) 
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getCtrlKey: NULL pointer\n");
        return JNI_FALSE;
    }

    PRBool ctrlKey = PR_FALSE;
    nsresult rv = event->GetCtrlKey(&ctrlKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getCtrlKey: failed", rv);
        return JNI_FALSE;
    }

    return (ctrlKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}


/*
 * Class:     org_mozilla_dom_events_KeyEventImpl
 * Method:    getKeyCode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_events_KeyEventImpl_getKeyCode
  (JNIEnv *env, jobject jthis)
{
    nsIDOMUIEvent* event = (nsIDOMUIEvent*)
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID);
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getKeyCode: NULL pointer\n");
        return 0;
    }

    PRUint32 code = 0;
    nsresult rv = event->GetKeyCode(&code);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getKeyCode: failed");
        return 0;
    }

    jfieldID keyCodeFID = NULL;
    switch(code) {
        case nsIDOMUIEvent::DOM_VK_0:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_0_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_1:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_1_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_2:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_2_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_3:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_3_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_4:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_4_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_5:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_5_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_6:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_6_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_7:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_7_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_8:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_8_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_9:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_9_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_A:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_A_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_ADD:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_ADD_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_ALT:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_ALT_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_B:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_B_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_BACK:
// renamed in Mozilla's implementation of DOM2 events 
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_BACK_SPACE_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_BACK_QUOTE:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_BACK_QUOTE_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_BACK_SLASH:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_BACK_SLASH_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_C:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_C_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_CANCEL:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_CANCEL_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_CAPS_LOCK:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_CAPS_LOCK_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_CLEAR:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_CLEAR_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_CLOSE_BRACKET:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_CLOSE_BRACKET_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_COMMA:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_COMMA_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_CONTROL:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_CONTROL_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_D:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_D_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_DECIMAL:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_DECIMAL_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_DELETE:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_DELETE_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_DIVIDE:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_DIVIDE_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_DOWN:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_DOWN_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_E:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_E_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_END:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_END_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_ENTER:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_ENTER_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_EQUALS:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_EQUALS_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_ESCAPE:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_ESCAPE_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F1:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F1_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F10:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F10_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F11:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F11_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F12:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F12_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F13:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F13_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F14:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F14_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F15:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F15_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F16:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F16_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F17:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F17_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F18:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F18_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F19:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F19_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F2:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F2_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F20:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F20_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F21:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F21_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F22:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F22_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F23:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F23_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F24:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F24_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F3:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F3_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F4:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F4_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F5:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F5_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F6:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F6_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F7:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F7_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F8:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F8_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_F9:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_F9_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_G:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_G_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_H:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_H_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_HOME:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_HOME_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_I:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_I_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_INSERT:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_INSERT_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_J:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_J_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_K:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_K_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_L:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_L_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_LEFT:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_LEFT_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_M:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_M_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_MULTIPLY:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_MULTIPLY_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_N:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_N_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD0:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD0_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD1:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD1_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD2:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD2_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD3:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD3_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD4:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD4_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD5:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD5_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD6:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD6_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD7:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD7_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD8:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD8_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUMPAD9:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD9_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_NUM_LOCK:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_NUM_LOCK_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_O:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_O_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_OPEN_BRACKET:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_OPEN_BRACKET_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_P:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_P_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_PAGE_DOWN:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_PAGE_DOWN_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_PAGE_UP:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_PAGE_UP_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_PAUSE:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_PAUSE_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_PERIOD:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_PERIOD_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_PRINTSCREEN:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_PRINTSCREEN_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_Q:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_Q_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_QUOTE:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_QUOTE_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_R:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_R_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_RETURN:
// does not exists in DOM2 specs
//          keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_RETURN_FID;
            PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
                ("UIEvent.getKeyCode: RETURN is pressed. But there is no corresponding constant in Java DOM specs."));
            break;
        case nsIDOMUIEvent::DOM_VK_RIGHT:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_RIGHT_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_S:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_S_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_SCROLL_LOCK:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_SCROLL_LOCK_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_SEMICOLON:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_SEMICOLON_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_SEPARATOR:
// misspelling is fixed in Mozilla but not in DOM2 specs ... 
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_SEPARATER_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_SHIFT:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_SHIFT_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_SLASH:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_SLASH_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_SPACE:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_SPACE_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_SUBTRACT:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_SUBTRACT_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_T:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_T_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_TAB:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_TAB_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_U:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_U_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_UP:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_UP_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_V:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_V_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_W:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_W_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_X:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_X_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_Y:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_Y_FID;
            break;
        case nsIDOMUIEvent::DOM_VK_Z:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_DOM_VK_Z_FID;
            break;
        default:
            keyCodeFID = JavaDOMEventsGlobals::keyEvent_CHAR_UNDEFINED_FID;
            PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
                        ("UIEvent.getKeyCode: illegal code %d\n", code));
            break;
    }

    jint ret;
    if (keyCodeFID) {
        ret = env->GetStaticIntField(JavaDOMEventsGlobals::keyEventClass, keyCodeFID);
        if (env->ExceptionOccurred()) {
            PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
                ("KeyEvent.getKeyCode: keyCodeFID failed\n"));
            ret = 0;
        }
    } 

    return ret;
}

/*
 * Class:     org_mozilla_dom_events_KeyEventImpl
 * Method:    getMetaKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_KeyEventImpl_getMetaKey
  (JNIEnv *env, jobject jthis)
{
    nsIDOMUIEvent* event = (nsIDOMUIEvent*) 
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getMetaKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool metaKey = PR_FALSE;
    nsresult rv = event->GetMetaKey(&metaKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getMetaKey: failed");
        return JNI_FALSE;
    }

    return (metaKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_events_KeyEventImpl
 * Method:    getShiftKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_events_KeyEventImpl_getShiftKey
  (JNIEnv *env, jobject jthis)
{
    nsIDOMUIEvent* event = (nsIDOMUIEvent*) 
        env->GetLongField(jthis, JavaDOMEventsGlobals::eventPtrFID); 
    if (!event) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getShiftKey: NULL pointer");
        return JNI_FALSE;
    }

    PRBool shiftKey = PR_FALSE;
    nsresult rv = event->GetShiftKey(&shiftKey);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "KeyEvent.getShiftKey: failed");
        return JNI_FALSE;
    }

    return (shiftKey == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}


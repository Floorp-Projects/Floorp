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
#include "prmon.h"
#include "nsAutoLock.h"
#include "javaDOMEventsGlobals.h"
#include "nsIDOMEvent.h"
#include "nsIDOMUIEvent.h"

jclass JavaDOMEventsGlobals::eventClass = NULL;
jclass JavaDOMEventsGlobals::uiEventClass = NULL;
jclass JavaDOMEventsGlobals::eventListenerClass = NULL;
jclass JavaDOMEventsGlobals::eventTargetClass = NULL;
jclass JavaDOMEventsGlobals::keyEventClass = NULL;
jclass JavaDOMEventsGlobals::mouseEventClass = NULL;
jclass JavaDOMEventsGlobals::mutationEventClass = NULL;

jfieldID JavaDOMEventsGlobals::eventPtrFID = NULL;
jfieldID JavaDOMEventsGlobals::eventTargetPtrFID = NULL;

jfieldID JavaDOMEventsGlobals::eventPhaseBubblingFID = NULL;
jfieldID JavaDOMEventsGlobals::eventPhaseCapturingFID = NULL;
jfieldID JavaDOMEventsGlobals::eventPhaseAtTargetFID = NULL;

jfieldID JavaDOMEventsGlobals::keyEvent_CHAR_UNDEFINED_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_0_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_1_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_2_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_3_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_4_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_5_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_6_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_7_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_8_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_9_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_A_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ACCEPT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ADD_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_AGAIN_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ALL_CANDIDATES_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ALPHANUMERIC_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ALT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ALT_GRAPH_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_AMPERSAND_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ASTERISK_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_AT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_B_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_BACK_QUOTE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_BACK_SLASH_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_BACK_SPACE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_BRACELEFT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_BRACERIGHT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_C_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CANCEL_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CAPS_LOCK_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CIRCUMFLEX_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CLEAR_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CLOSE_BRACKET_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CODE_INPUT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_COLON_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_COMMA_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_COMPOSE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CONTROL_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CONVERT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_COPY_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_CUT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_D_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_ABOVEDOT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_ABOVERING_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_ACUTE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_BREVE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_CARON_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_CEDILLA_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_CIRCUMFLEX_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_DIAERESIS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_DOUBLEACUTE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_GRAVE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_IOTA_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_MACRON_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_OGONEK_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_SEMIVOICED_SOUND_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_TILDE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DEAD_VOICED_SOUND_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DECIMAL_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DELETE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DIVIDE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DOLLAR_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_DOWN_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_E_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_END_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ENTER_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_EQUALS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ESCAPE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_EURO_SIGN_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_EXCLAMATION_MARK_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F1_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F10_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F11_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F12_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F13_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F14_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F15_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F16_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F17_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F18_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F19_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F2_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F20_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F21_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F22_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F23_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F24_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F3_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F4_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F5_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F6_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F7_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F8_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_F9_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_FINAL_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_FIND_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_FULL_WIDTH_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_G_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_GREATER_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_H_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_HALF_WIDTH_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_HELP_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_HIRAGANA_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_HOME_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_I_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_INSERT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_INVERTED_EXCLAMATION_MARK_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_J_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_JAPANESE_HIRAGANA_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_JAPANESE_KATAKANA_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_JAPANESE_ROMAN_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_K_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_KANA_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_KANJI_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_KATAKANA_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_KP_DOWN_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_KP_LEFT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_KP_RIGHT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_KP_UP_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_L_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_LEFT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_LEFT_PARENTHESIS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_LESS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_M_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_META_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_MINUS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_MODECHANGE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_MULTIPLY_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_N_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NONCONVERT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMBER_SIGN_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD0_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD1_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD2_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD3_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD4_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD5_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD6_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD7_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD8_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUMPAD9_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_NUM_LOCK_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_O_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_OPEN_BRACKET_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_P_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PAGE_DOWN_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PAGE_UP_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PASTE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PAUSE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PERIOD_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PLUS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PREVIOUS_CANDIDATE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PRINTSCREEN_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_PROPS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_Q_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_QUOTE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_QUOTEDBL_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_R_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_RIGHT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_RIGHT_PARENTHESIS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_ROMAN_CHARACTERS_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_S_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_SCROLL_LOCK_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_SEMICOLON_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_SEPARATER_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_SHIFT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_SLASH_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_SPACE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_STOP_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_SUBTRACT_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_T_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_TAB_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_U_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_UNDEFINED_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_UNDERSCORE_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_UNDO_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_UP_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_V_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_W_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_X_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_Y_FID = NULL;
jfieldID JavaDOMEventsGlobals::keyEvent_DOM_VK_Z_FID = NULL;


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

  keyEventClass = env->FindClass("org/mozilla/dom/events/KeyEventImpl");
  if (!keyEventClass) {
      JavaDOMGlobals::ThrowException(env, "Class org.mozilla.dom.events.KeyEventImpl not found");
      return;
  }
  keyEventClass = (jclass) env->NewGlobalRef(keyEventClass);
  if (!keyEventClass) 
      return;

  mouseEventClass = env->FindClass("org/mozilla/dom/events/MouseEventImpl");
  if (!mouseEventClass) {
      JavaDOMGlobals::ThrowException(env, "Class org.mozilla.dom.events.MouseEventImpl not found");
      return;
  }
  mouseEventClass = (jclass) env->NewGlobalRef(mouseEventClass);
  if (!mouseEventClass) 
      return;

  keyEvent_CHAR_UNDEFINED_FID =
    env->GetStaticFieldID(keyEventClass, "CHAR_UNDEFINED", "I");
  if (!keyEvent_CHAR_UNDEFINED_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field CHAR_UNDEFINED at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_0_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_0", "I");
  if (!keyEvent_DOM_VK_0_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_0 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_1_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_1", "I");
  if (!keyEvent_DOM_VK_1_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_1 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_2_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_2", "I");
  if (!keyEvent_DOM_VK_2_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_2 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_3_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_3", "I");
  if (!keyEvent_DOM_VK_3_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_3 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_4_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_4", "I");
  if (!keyEvent_DOM_VK_4_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_4 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_5_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_5", "I");
  if (!keyEvent_DOM_VK_5_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_5 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_6_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_6", "I");
  if (!keyEvent_DOM_VK_6_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_6 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_7_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_7", "I");
  if (!keyEvent_DOM_VK_7_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_7 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_8_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_8", "I");
  if (!keyEvent_DOM_VK_8_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_8 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_9_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_9", "I");
  if (!keyEvent_DOM_VK_9_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_9 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_A_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_A", "I");
  if (!keyEvent_DOM_VK_A_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_A at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ACCEPT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ACCEPT", "I");
  if (!keyEvent_DOM_VK_ACCEPT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ACCEPT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ADD_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ADD", "I");
  if (!keyEvent_DOM_VK_ADD_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ADD at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_AGAIN_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_AGAIN", "I");
  if (!keyEvent_DOM_VK_AGAIN_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_AGAIN at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ALL_CANDIDATES_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ALL_CANDIDATES", "I");
  if (!keyEvent_DOM_VK_ALL_CANDIDATES_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ALL_CANDIDATES at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ALPHANUMERIC_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ALPHANUMERIC", "I");
  if (!keyEvent_DOM_VK_ALPHANUMERIC_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ALPHANUMERIC at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ALT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ALT", "I");
  if (!keyEvent_DOM_VK_ALT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ALT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ALT_GRAPH_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ALT_GRAPH", "I");
  if (!keyEvent_DOM_VK_ALT_GRAPH_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ALT_GRAPH at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_AMPERSAND_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_AMPERSAND", "I");
  if (!keyEvent_DOM_VK_AMPERSAND_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_AMPERSAND at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ASTERISK_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ASTERISK", "I");
  if (!keyEvent_DOM_VK_ASTERISK_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ASTERISK at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_AT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_AT", "I");
  if (!keyEvent_DOM_VK_AT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_AT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_B_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_B", "I");
  if (!keyEvent_DOM_VK_B_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_B at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_BACK_QUOTE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_BACK_QUOTE", "I");
  if (!keyEvent_DOM_VK_BACK_QUOTE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_BACK_QUOTE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_BACK_SLASH_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_BACK_SLASH", "I");
  if (!keyEvent_DOM_VK_BACK_SLASH_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_BACK_SLASH at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_BACK_SPACE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_BACK_SPACE", "I");
  if (!keyEvent_DOM_VK_BACK_SPACE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_BACK_SPACE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_BRACELEFT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_BRACELEFT", "I");
  if (!keyEvent_DOM_VK_BRACELEFT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_BRACELEFT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_BRACERIGHT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_BRACERIGHT", "I");
  if (!keyEvent_DOM_VK_BRACERIGHT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_BRACERIGHT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_C_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_C", "I");
  if (!keyEvent_DOM_VK_C_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_C at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CANCEL_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CANCEL", "I");
  if (!keyEvent_DOM_VK_CANCEL_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CANCEL at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CAPS_LOCK_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CAPS_LOCK", "I");
  if (!keyEvent_DOM_VK_CAPS_LOCK_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CAPS_LOCK at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CIRCUMFLEX_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CIRCUMFLEX", "I");
  if (!keyEvent_DOM_VK_CIRCUMFLEX_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CIRCUMFLEX at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CLEAR_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CLEAR", "I");
  if (!keyEvent_DOM_VK_CLEAR_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CLEAR at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CLOSE_BRACKET_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CLOSE_BRACKET", "I");
  if (!keyEvent_DOM_VK_CLOSE_BRACKET_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CLOSE_BRACKET at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CODE_INPUT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CODE_INPUT", "I");
  if (!keyEvent_DOM_VK_CODE_INPUT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CODE_INPUT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_COLON_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_COLON", "I");
  if (!keyEvent_DOM_VK_COLON_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_COLON at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_COMMA_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_COMMA", "I");
  if (!keyEvent_DOM_VK_COMMA_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_COMMA at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_COMPOSE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_COMPOSE", "I");
  if (!keyEvent_DOM_VK_COMPOSE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_COMPOSE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CONTROL_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CONTROL", "I");
  if (!keyEvent_DOM_VK_CONTROL_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CONTROL at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CONVERT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CONVERT", "I");
  if (!keyEvent_DOM_VK_CONVERT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CONVERT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_COPY_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_COPY", "I");
  if (!keyEvent_DOM_VK_COPY_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_COPY at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_CUT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_CUT", "I");
  if (!keyEvent_DOM_VK_CUT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_CUT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_D_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_D", "I");
  if (!keyEvent_DOM_VK_D_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_D at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_ABOVEDOT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_ABOVEDOT", "I");
  if (!keyEvent_DOM_VK_DEAD_ABOVEDOT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_ABOVEDOT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_ABOVERING_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_ABOVERING", "I");
  if (!keyEvent_DOM_VK_DEAD_ABOVERING_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_ABOVERING at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_ACUTE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_ACUTE", "I");
  if (!keyEvent_DOM_VK_DEAD_ACUTE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_ACUTE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_BREVE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_BREVE", "I");
  if (!keyEvent_DOM_VK_DEAD_BREVE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_BREVE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_CARON_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_CARON", "I");
  if (!keyEvent_DOM_VK_DEAD_CARON_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_CARON at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_CEDILLA_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_CEDILLA", "I");
  if (!keyEvent_DOM_VK_DEAD_CEDILLA_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_CEDILLA at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_CIRCUMFLEX_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_CIRCUMFLEX", "I");
  if (!keyEvent_DOM_VK_DEAD_CIRCUMFLEX_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_CIRCUMFLEX at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_DIAERESIS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_DIAERESIS", "I");
  if (!keyEvent_DOM_VK_DEAD_DIAERESIS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_DIAERESIS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_DOUBLEACUTE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_DOUBLEACUTE", "I");
  if (!keyEvent_DOM_VK_DEAD_DOUBLEACUTE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_DOUBLEACUTE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_GRAVE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_GRAVE", "I");
  if (!keyEvent_DOM_VK_DEAD_GRAVE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_GRAVE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_IOTA_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_IOTA", "I");
  if (!keyEvent_DOM_VK_DEAD_IOTA_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_IOTA at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_MACRON_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_MACRON", "I");
  if (!keyEvent_DOM_VK_DEAD_MACRON_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_MACRON at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_OGONEK_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_OGONEK", "I");
  if (!keyEvent_DOM_VK_DEAD_OGONEK_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_OGONEK at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_SEMIVOICED_SOUND_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_SEMIVOICED_SOUND", "I");
  if (!keyEvent_DOM_VK_DEAD_SEMIVOICED_SOUND_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_SEMIVOICED_SOUND at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_TILDE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_TILDE", "I");
  if (!keyEvent_DOM_VK_DEAD_TILDE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_TILDE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DEAD_VOICED_SOUND_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DEAD_VOICED_SOUND", "I");
  if (!keyEvent_DOM_VK_DEAD_VOICED_SOUND_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DEAD_VOICED_SOUND at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DECIMAL_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DECIMAL", "I");
  if (!keyEvent_DOM_VK_DECIMAL_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DECIMAL at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DELETE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DELETE", "I");
  if (!keyEvent_DOM_VK_DELETE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DELETE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DIVIDE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DIVIDE", "I");
  if (!keyEvent_DOM_VK_DIVIDE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DIVIDE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DOLLAR_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DOLLAR", "I");
  if (!keyEvent_DOM_VK_DOLLAR_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DOLLAR at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_DOWN_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_DOWN", "I");
  if (!keyEvent_DOM_VK_DOWN_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_DOWN at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_E_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_E", "I");
  if (!keyEvent_DOM_VK_E_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_E at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_END_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_END", "I");
  if (!keyEvent_DOM_VK_END_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_END at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ENTER_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ENTER", "I");
  if (!keyEvent_DOM_VK_ENTER_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ENTER at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_EQUALS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_EQUALS", "I");
  if (!keyEvent_DOM_VK_EQUALS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_EQUALS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ESCAPE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ESCAPE", "I");
  if (!keyEvent_DOM_VK_ESCAPE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ESCAPE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_EURO_SIGN_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_EURO_SIGN", "I");
  if (!keyEvent_DOM_VK_EURO_SIGN_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_EURO_SIGN at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_EXCLAMATION_MARK_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_EXCLAMATION_MARK", "I");
  if (!keyEvent_DOM_VK_EXCLAMATION_MARK_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_EXCLAMATION_MARK at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F", "I");
  if (!keyEvent_DOM_VK_F_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F1_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F1", "I");
  if (!keyEvent_DOM_VK_F1_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F1 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F10_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F10", "I");
  if (!keyEvent_DOM_VK_F10_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F10 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F11_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F11", "I");
  if (!keyEvent_DOM_VK_F11_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F11 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F12_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F12", "I");
  if (!keyEvent_DOM_VK_F12_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F12 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F13_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F13", "I");
  if (!keyEvent_DOM_VK_F13_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F13 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F14_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F14", "I");
  if (!keyEvent_DOM_VK_F14_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F14 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F15_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F15", "I");
  if (!keyEvent_DOM_VK_F15_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F15 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F16_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F16", "I");
  if (!keyEvent_DOM_VK_F16_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F16 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F17_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F17", "I");
  if (!keyEvent_DOM_VK_F17_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F17 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F18_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F18", "I");
  if (!keyEvent_DOM_VK_F18_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F18 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F19_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F19", "I");
  if (!keyEvent_DOM_VK_F19_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F19 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F2_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F2", "I");
  if (!keyEvent_DOM_VK_F2_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F2 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F20_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F20", "I");
  if (!keyEvent_DOM_VK_F20_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F20 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F21_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F21", "I");
  if (!keyEvent_DOM_VK_F21_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F21 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F22_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F22", "I");
  if (!keyEvent_DOM_VK_F22_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F22 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F23_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F23", "I");
  if (!keyEvent_DOM_VK_F23_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F23 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F24_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F24", "I");
  if (!keyEvent_DOM_VK_F24_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F24 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F3_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F3", "I");
  if (!keyEvent_DOM_VK_F3_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F3 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F4_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F4", "I");
  if (!keyEvent_DOM_VK_F4_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F4 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F5_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F5", "I");
  if (!keyEvent_DOM_VK_F5_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F5 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F6_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F6", "I");
  if (!keyEvent_DOM_VK_F6_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F6 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F7_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F7", "I");
  if (!keyEvent_DOM_VK_F7_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F7 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F8_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F8", "I");
  if (!keyEvent_DOM_VK_F8_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F8 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_F9_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_F9", "I");
  if (!keyEvent_DOM_VK_F9_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_F9 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_FINAL_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_FINAL", "I");
  if (!keyEvent_DOM_VK_FINAL_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_FINAL at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_FIND_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_FIND", "I");
  if (!keyEvent_DOM_VK_FIND_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_FIND at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_FULL_WIDTH_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_FULL_WIDTH", "I");
  if (!keyEvent_DOM_VK_FULL_WIDTH_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_FULL_WIDTH at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_G_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_G", "I");
  if (!keyEvent_DOM_VK_G_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_G at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_GREATER_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_GREATER", "I");
  if (!keyEvent_DOM_VK_GREATER_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_GREATER at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_H_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_H", "I");
  if (!keyEvent_DOM_VK_H_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_H at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_HALF_WIDTH_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_HALF_WIDTH", "I");
  if (!keyEvent_DOM_VK_HALF_WIDTH_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_HALF_WIDTH at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_HELP_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_HELP", "I");
  if (!keyEvent_DOM_VK_HELP_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_HELP at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_HIRAGANA_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_HIRAGANA", "I");
  if (!keyEvent_DOM_VK_HIRAGANA_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_HIRAGANA at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_HOME_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_HOME", "I");
  if (!keyEvent_DOM_VK_HOME_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_HOME at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_I_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_I", "I");
  if (!keyEvent_DOM_VK_I_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_I at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_INSERT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_INSERT", "I");
  if (!keyEvent_DOM_VK_INSERT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_INSERT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_INVERTED_EXCLAMATION_MARK_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_INVERTED_EXCLAMATION_MARK", "I");
  if (!keyEvent_DOM_VK_INVERTED_EXCLAMATION_MARK_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_INVERTED_EXCLAMATION_MARK at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_J_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_J", "I");
  if (!keyEvent_DOM_VK_J_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_J at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_JAPANESE_HIRAGANA_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_JAPANESE_HIRAGANA", "I");
  if (!keyEvent_DOM_VK_JAPANESE_HIRAGANA_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_JAPANESE_HIRAGANA at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_JAPANESE_KATAKANA_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_JAPANESE_KATAKANA", "I");
  if (!keyEvent_DOM_VK_JAPANESE_KATAKANA_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_JAPANESE_KATAKANA at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_JAPANESE_ROMAN_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_JAPANESE_ROMAN", "I");
  if (!keyEvent_DOM_VK_JAPANESE_ROMAN_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_JAPANESE_ROMAN at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_K_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_K", "I");
  if (!keyEvent_DOM_VK_K_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_K at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_KANA_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_KANA", "I");
  if (!keyEvent_DOM_VK_KANA_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_KANA at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_KANJI_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_KANJI", "I");
  if (!keyEvent_DOM_VK_KANJI_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_KANJI at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_KATAKANA_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_KATAKANA", "I");
  if (!keyEvent_DOM_VK_KATAKANA_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_KATAKANA at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_KP_DOWN_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_KP_DOWN", "I");
  if (!keyEvent_DOM_VK_KP_DOWN_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_KP_DOWN at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_KP_LEFT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_KP_LEFT", "I");
  if (!keyEvent_DOM_VK_KP_LEFT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_KP_LEFT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_KP_RIGHT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_KP_RIGHT", "I");
  if (!keyEvent_DOM_VK_KP_RIGHT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_KP_RIGHT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_KP_UP_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_KP_UP", "I");
  if (!keyEvent_DOM_VK_KP_UP_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_KP_UP at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_L_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_L", "I");
  if (!keyEvent_DOM_VK_L_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_L at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_LEFT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_LEFT", "I");
  if (!keyEvent_DOM_VK_LEFT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_LEFT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_LEFT_PARENTHESIS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_LEFT_PARENTHESIS", "I");
  if (!keyEvent_DOM_VK_LEFT_PARENTHESIS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_LEFT_PARENTHESIS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_LESS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_LESS", "I");
  if (!keyEvent_DOM_VK_LESS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_LESS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_M_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_M", "I");
  if (!keyEvent_DOM_VK_M_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_M at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_META_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_META", "I");
  if (!keyEvent_DOM_VK_META_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_META at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_MINUS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_MINUS", "I");
  if (!keyEvent_DOM_VK_MINUS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_MINUS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_MODECHANGE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_MODECHANGE", "I");
  if (!keyEvent_DOM_VK_MODECHANGE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_MODECHANGE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_MULTIPLY_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_MULTIPLY", "I");
  if (!keyEvent_DOM_VK_MULTIPLY_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_MULTIPLY at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_N_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_N", "I");
  if (!keyEvent_DOM_VK_N_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_N at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NONCONVERT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NONCONVERT", "I");
  if (!keyEvent_DOM_VK_NONCONVERT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NONCONVERT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMBER_SIGN_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMBER_SIGN", "I");
  if (!keyEvent_DOM_VK_NUMBER_SIGN_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMBER_SIGN at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD0_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD0", "I");
  if (!keyEvent_DOM_VK_NUMPAD0_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD0 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD1_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD1", "I");
  if (!keyEvent_DOM_VK_NUMPAD1_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD1 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD2_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD2", "I");
  if (!keyEvent_DOM_VK_NUMPAD2_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD2 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD3_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD3", "I");
  if (!keyEvent_DOM_VK_NUMPAD3_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD3 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD4_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD4", "I");
  if (!keyEvent_DOM_VK_NUMPAD4_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD4 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD5_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD5", "I");
  if (!keyEvent_DOM_VK_NUMPAD5_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD5 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD6_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD6", "I");
  if (!keyEvent_DOM_VK_NUMPAD6_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD6 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD7_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD7", "I");
  if (!keyEvent_DOM_VK_NUMPAD7_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD7 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD8_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD8", "I");
  if (!keyEvent_DOM_VK_NUMPAD8_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD8 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUMPAD9_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUMPAD9", "I");
  if (!keyEvent_DOM_VK_NUMPAD9_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUMPAD9 at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_NUM_LOCK_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_NUM_LOCK", "I");
  if (!keyEvent_DOM_VK_NUM_LOCK_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_NUM_LOCK at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_O_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_O", "I");
  if (!keyEvent_DOM_VK_O_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_O at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_OPEN_BRACKET_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_OPEN_BRACKET", "I");
  if (!keyEvent_DOM_VK_OPEN_BRACKET_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_OPEN_BRACKET at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_P_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_P", "I");
  if (!keyEvent_DOM_VK_P_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_P at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PAGE_DOWN_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PAGE_DOWN", "I");
  if (!keyEvent_DOM_VK_PAGE_DOWN_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PAGE_DOWN at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PAGE_UP_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PAGE_UP", "I");
  if (!keyEvent_DOM_VK_PAGE_UP_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PAGE_UP at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PASTE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PASTE", "I");
  if (!keyEvent_DOM_VK_PASTE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PASTE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PAUSE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PAUSE", "I");
  if (!keyEvent_DOM_VK_PAUSE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PAUSE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PERIOD_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PERIOD", "I");
  if (!keyEvent_DOM_VK_PERIOD_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PERIOD at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PLUS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PLUS", "I");
  if (!keyEvent_DOM_VK_PLUS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PLUS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PREVIOUS_CANDIDATE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PREVIOUS_CANDIDATE", "I");
  if (!keyEvent_DOM_VK_PREVIOUS_CANDIDATE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PREVIOUS_CANDIDATE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PRINTSCREEN_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PRINTSCREEN", "I");
  if (!keyEvent_DOM_VK_PRINTSCREEN_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PRINTSCREEN at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_PROPS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_PROPS", "I");
  if (!keyEvent_DOM_VK_PROPS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_PROPS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_Q_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_Q", "I");
  if (!keyEvent_DOM_VK_Q_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_Q at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_QUOTE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_QUOTE", "I");
  if (!keyEvent_DOM_VK_QUOTE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_QUOTE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_QUOTEDBL_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_QUOTEDBL", "I");
  if (!keyEvent_DOM_VK_QUOTEDBL_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_QUOTEDBL at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_R_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_R", "I");
  if (!keyEvent_DOM_VK_R_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_R at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_RIGHT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_RIGHT", "I");
  if (!keyEvent_DOM_VK_RIGHT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_RIGHT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_RIGHT_PARENTHESIS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_RIGHT_PARENTHESIS", "I");
  if (!keyEvent_DOM_VK_RIGHT_PARENTHESIS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_RIGHT_PARENTHESIS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_ROMAN_CHARACTERS_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_ROMAN_CHARACTERS", "I");
  if (!keyEvent_DOM_VK_ROMAN_CHARACTERS_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_ROMAN_CHARACTERS at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_S_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_S", "I");
  if (!keyEvent_DOM_VK_S_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_S at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_SCROLL_LOCK_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_SCROLL_LOCK", "I");
  if (!keyEvent_DOM_VK_SCROLL_LOCK_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_SCROLL_LOCK at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_SEMICOLON_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_SEMICOLON", "I");
  if (!keyEvent_DOM_VK_SEMICOLON_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_SEMICOLON at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_SEPARATER_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_SEPARATER", "I");
  if (!keyEvent_DOM_VK_SEPARATER_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_SEPARATER at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_SHIFT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_SHIFT", "I");
  if (!keyEvent_DOM_VK_SHIFT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_SHIFT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_SLASH_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_SLASH", "I");
  if (!keyEvent_DOM_VK_SLASH_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_SLASH at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_SPACE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_SPACE", "I");
  if (!keyEvent_DOM_VK_SPACE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_SPACE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_STOP_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_STOP", "I");
  if (!keyEvent_DOM_VK_STOP_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_STOP at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_SUBTRACT_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_SUBTRACT", "I");
  if (!keyEvent_DOM_VK_SUBTRACT_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_SUBTRACT at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_T_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_T", "I");
  if (!keyEvent_DOM_VK_T_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_T at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_TAB_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_TAB", "I");
  if (!keyEvent_DOM_VK_TAB_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_TAB at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_U_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_U", "I");
  if (!keyEvent_DOM_VK_U_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_U at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_UNDEFINED_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_UNDEFINED", "I");
  if (!keyEvent_DOM_VK_UNDEFINED_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_UNDEFINED at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_UNDERSCORE_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_UNDERSCORE", "I");
  if (!keyEvent_DOM_VK_UNDERSCORE_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_UNDERSCORE at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_UNDO_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_UNDO", "I");
  if (!keyEvent_DOM_VK_UNDO_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_UNDO at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_UP_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_UP", "I");
  if (!keyEvent_DOM_VK_UP_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_UP at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_V_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_V", "I");
  if (!keyEvent_DOM_VK_V_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_V at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_W_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_W", "I");
  if (!keyEvent_DOM_VK_W_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_W at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_X_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_X", "I");
  if (!keyEvent_DOM_VK_X_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_X at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_Y_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_Y", "I");
  if (!keyEvent_DOM_VK_Y_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_Y at class org.w3c.mozilla.KeyEvent"));
    return;
  }

  keyEvent_DOM_VK_Z_FID =
    env->GetStaticFieldID(keyEventClass, "DOM_VK_Z", "I");
  if (!keyEvent_DOM_VK_Z_FID) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR,
            ("Can't get FID of field DOM_VK_Z at class org.w3c.mozilla.KeyEvent"));
    return;
  }



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

  env->DeleteGlobalRef(keyEventClass);
  if (env->ExceptionOccurred()) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
           ("JavaDOMEventsGlobals::Destroy: failed to delete keyEvent global ref %x\n", 
            keyEventClass));
      return;
  }
  keyEventClass = NULL;

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
static jboolean isEventOfType(char **types, nsString type) 
{
    int i=0;

    while (types && types[i]) {
      if (type == types[i])
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
  
  isupports = (nsISupports *) event;

  //check whenever our Event is UIEvent
  isupports->QueryInterface(kIDOMUIEventIID, (void **) &target);
  if (target) {
    // At the moment DOM2 draft specifies set of UIEvent subclasses 
    // However Mozilla still presents these events as nsUIEvent
    // So we need a cludge to determine proper java class to be created
    
    static char *uiEventTypes[] = { "resize", "scroll", "focusin", 
                                   "focusout", "gainselection",
                                   "loseselection", "activate", NULL};
  
    static char *mouseEventTypes[] = { "click", "mousedown",
                                      "mouseup", "mouseover",
                                      "mousemove", "mouseout", NULL};
  
    static char *keyEventTypes[] = { "keypress", "keydown", "keyup", NULL};


    nsString nsType;
    nsresult rv = event->GetType(nsType);
    if (NS_FAILED(rv)) {
        JavaDOMGlobals::ThrowException(env,
            "Event.getType at JavaDOMEventsGlobals: failed");
        return NULL;
    }

    if (isEventOfType(mouseEventTypes, nsType) == JNI_TRUE) {
        clazz = mouseEventClass;
    } else if (isEventOfType(keyEventTypes, nsType) == JNI_TRUE) {
        clazz = keyEventClass;
    } else if (isEventOfType(uiEventTypes, nsType) == JNI_TRUE) {  
            clazz = uiEventClass;
    } else {
      /* Being paranoid here, make sure the (unicode) string is
         NULL-terminated before passing it to PR_LOG. Otherwise, we
         may cause a crash */

            PRInt32 length = nsType.Length();
            char* buffer = new char[length+1];
	    strncpy(buffer, nsType.GetBuffer(), length);
	    buffer[length] = 0;

            PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING,
                ("Unknown type of UI event (%s)", buffer));
	    delete[] buffer;
            clazz = uiEventClass;
    }

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


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

#ifndef __JavaDOMEventsGlobals_h__
#define __JavaDOMEventsGlobals_h__

#include"javaDOMGlobals.h"
#include"nsIDOMEvent.h"

class JavaDOMEventsGlobals {

 public:
  static jclass eventClass;
  static jclass eventListenerClass;
  static jclass eventTargetClass;
  static jclass uiEventClass;
  static jclass mutationEventClass;
  static jclass keyEventClass;
  static jclass mouseEventClass;
  
  static jfieldID eventPtrFID;
  static jfieldID eventTargetPtrFID;

  static jfieldID eventPhaseBubblingFID;
  static jfieldID eventPhaseCapturingFID;
  static jfieldID eventPhaseAtTargetFID;   

  static jfieldID keyEvent_CHAR_UNDEFINED_FID;
  static jfieldID keyEvent_DOM_VK_0_FID;
  static jfieldID keyEvent_DOM_VK_1_FID;
  static jfieldID keyEvent_DOM_VK_2_FID;
  static jfieldID keyEvent_DOM_VK_3_FID;
  static jfieldID keyEvent_DOM_VK_4_FID;
  static jfieldID keyEvent_DOM_VK_5_FID;
  static jfieldID keyEvent_DOM_VK_6_FID;
  static jfieldID keyEvent_DOM_VK_7_FID;
  static jfieldID keyEvent_DOM_VK_8_FID;
  static jfieldID keyEvent_DOM_VK_9_FID;
  static jfieldID keyEvent_DOM_VK_A_FID;
  static jfieldID keyEvent_DOM_VK_ACCEPT_FID;
  static jfieldID keyEvent_DOM_VK_ADD_FID;
  static jfieldID keyEvent_DOM_VK_AGAIN_FID;
  static jfieldID keyEvent_DOM_VK_ALL_CANDIDATES_FID;
  static jfieldID keyEvent_DOM_VK_ALPHANUMERIC_FID;
  static jfieldID keyEvent_DOM_VK_ALT_FID;
  static jfieldID keyEvent_DOM_VK_ALT_GRAPH_FID;
  static jfieldID keyEvent_DOM_VK_AMPERSAND_FID;
  static jfieldID keyEvent_DOM_VK_ASTERISK_FID;
  static jfieldID keyEvent_DOM_VK_AT_FID;
  static jfieldID keyEvent_DOM_VK_B_FID;
  static jfieldID keyEvent_DOM_VK_BACK_QUOTE_FID;
  static jfieldID keyEvent_DOM_VK_BACK_SLASH_FID;
  static jfieldID keyEvent_DOM_VK_BACK_SPACE_FID;
  static jfieldID keyEvent_DOM_VK_BRACELEFT_FID;
  static jfieldID keyEvent_DOM_VK_BRACERIGHT_FID;
  static jfieldID keyEvent_DOM_VK_C_FID;
  static jfieldID keyEvent_DOM_VK_CANCEL_FID;
  static jfieldID keyEvent_DOM_VK_CAPS_LOCK_FID;
  static jfieldID keyEvent_DOM_VK_CIRCUMFLEX_FID;
  static jfieldID keyEvent_DOM_VK_CLEAR_FID;
  static jfieldID keyEvent_DOM_VK_CLOSE_BRACKET_FID;
  static jfieldID keyEvent_DOM_VK_CODE_INPUT_FID;
  static jfieldID keyEvent_DOM_VK_COLON_FID;
  static jfieldID keyEvent_DOM_VK_COMMA_FID;
  static jfieldID keyEvent_DOM_VK_COMPOSE_FID;
  static jfieldID keyEvent_DOM_VK_CONTROL_FID;
  static jfieldID keyEvent_DOM_VK_CONVERT_FID;
  static jfieldID keyEvent_DOM_VK_COPY_FID;
  static jfieldID keyEvent_DOM_VK_CUT_FID;
  static jfieldID keyEvent_DOM_VK_D_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_ABOVEDOT_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_ABOVERING_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_ACUTE_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_BREVE_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_CARON_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_CEDILLA_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_CIRCUMFLEX_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_DIAERESIS_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_DOUBLEACUTE_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_GRAVE_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_IOTA_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_MACRON_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_OGONEK_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_SEMIVOICED_SOUND_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_TILDE_FID;
  static jfieldID keyEvent_DOM_VK_DEAD_VOICED_SOUND_FID;
  static jfieldID keyEvent_DOM_VK_DECIMAL_FID;
  static jfieldID keyEvent_DOM_VK_DELETE_FID;
  static jfieldID keyEvent_DOM_VK_DIVIDE_FID;
  static jfieldID keyEvent_DOM_VK_DOLLAR_FID;
  static jfieldID keyEvent_DOM_VK_DOWN_FID;
  static jfieldID keyEvent_DOM_VK_E_FID;
  static jfieldID keyEvent_DOM_VK_END_FID;
  static jfieldID keyEvent_DOM_VK_ENTER_FID;
  static jfieldID keyEvent_DOM_VK_EQUALS_FID;
  static jfieldID keyEvent_DOM_VK_ESCAPE_FID;
  static jfieldID keyEvent_DOM_VK_EURO_SIGN_FID;
  static jfieldID keyEvent_DOM_VK_EXCLAMATION_MARK_FID;
  static jfieldID keyEvent_DOM_VK_F_FID;
  static jfieldID keyEvent_DOM_VK_F1_FID;
  static jfieldID keyEvent_DOM_VK_F10_FID;
  static jfieldID keyEvent_DOM_VK_F11_FID;
  static jfieldID keyEvent_DOM_VK_F12_FID;
  static jfieldID keyEvent_DOM_VK_F13_FID;
  static jfieldID keyEvent_DOM_VK_F14_FID;
  static jfieldID keyEvent_DOM_VK_F15_FID;
  static jfieldID keyEvent_DOM_VK_F16_FID;
  static jfieldID keyEvent_DOM_VK_F17_FID;
  static jfieldID keyEvent_DOM_VK_F18_FID;
  static jfieldID keyEvent_DOM_VK_F19_FID;
  static jfieldID keyEvent_DOM_VK_F2_FID;
  static jfieldID keyEvent_DOM_VK_F20_FID;
  static jfieldID keyEvent_DOM_VK_F21_FID;
  static jfieldID keyEvent_DOM_VK_F22_FID;
  static jfieldID keyEvent_DOM_VK_F23_FID;
  static jfieldID keyEvent_DOM_VK_F24_FID;
  static jfieldID keyEvent_DOM_VK_F3_FID;
  static jfieldID keyEvent_DOM_VK_F4_FID;
  static jfieldID keyEvent_DOM_VK_F5_FID;
  static jfieldID keyEvent_DOM_VK_F6_FID;
  static jfieldID keyEvent_DOM_VK_F7_FID;
  static jfieldID keyEvent_DOM_VK_F8_FID;
  static jfieldID keyEvent_DOM_VK_F9_FID;
  static jfieldID keyEvent_DOM_VK_FINAL_FID;
  static jfieldID keyEvent_DOM_VK_FIND_FID;
  static jfieldID keyEvent_DOM_VK_FULL_WIDTH_FID;
  static jfieldID keyEvent_DOM_VK_G_FID;
  static jfieldID keyEvent_DOM_VK_GREATER_FID;
  static jfieldID keyEvent_DOM_VK_H_FID;
  static jfieldID keyEvent_DOM_VK_HALF_WIDTH_FID;
  static jfieldID keyEvent_DOM_VK_HELP_FID;
  static jfieldID keyEvent_DOM_VK_HIRAGANA_FID;
  static jfieldID keyEvent_DOM_VK_HOME_FID;
  static jfieldID keyEvent_DOM_VK_I_FID;
  static jfieldID keyEvent_DOM_VK_INSERT_FID;
  static jfieldID keyEvent_DOM_VK_INVERTED_EXCLAMATION_MARK_FID;
  static jfieldID keyEvent_DOM_VK_J_FID;
  static jfieldID keyEvent_DOM_VK_JAPANESE_HIRAGANA_FID;
  static jfieldID keyEvent_DOM_VK_JAPANESE_KATAKANA_FID;
  static jfieldID keyEvent_DOM_VK_JAPANESE_ROMAN_FID;
  static jfieldID keyEvent_DOM_VK_K_FID;
  static jfieldID keyEvent_DOM_VK_KANA_FID;
  static jfieldID keyEvent_DOM_VK_KANJI_FID;
  static jfieldID keyEvent_DOM_VK_KATAKANA_FID;
  static jfieldID keyEvent_DOM_VK_KP_DOWN_FID;
  static jfieldID keyEvent_DOM_VK_KP_LEFT_FID;
  static jfieldID keyEvent_DOM_VK_KP_RIGHT_FID;
  static jfieldID keyEvent_DOM_VK_KP_UP_FID;
  static jfieldID keyEvent_DOM_VK_L_FID;
  static jfieldID keyEvent_DOM_VK_LEFT_FID;
  static jfieldID keyEvent_DOM_VK_LEFT_PARENTHESIS_FID;
  static jfieldID keyEvent_DOM_VK_LESS_FID;
  static jfieldID keyEvent_DOM_VK_M_FID;
  static jfieldID keyEvent_DOM_VK_META_FID;
  static jfieldID keyEvent_DOM_VK_MINUS_FID;
  static jfieldID keyEvent_DOM_VK_MODECHANGE_FID;
  static jfieldID keyEvent_DOM_VK_MULTIPLY_FID;
  static jfieldID keyEvent_DOM_VK_N_FID;
  static jfieldID keyEvent_DOM_VK_NONCONVERT_FID;
  static jfieldID keyEvent_DOM_VK_NUMBER_SIGN_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD0_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD1_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD2_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD3_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD4_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD5_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD6_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD7_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD8_FID;
  static jfieldID keyEvent_DOM_VK_NUMPAD9_FID;
  static jfieldID keyEvent_DOM_VK_NUM_LOCK_FID;
  static jfieldID keyEvent_DOM_VK_O_FID;
  static jfieldID keyEvent_DOM_VK_OPEN_BRACKET_FID;
  static jfieldID keyEvent_DOM_VK_P_FID;
  static jfieldID keyEvent_DOM_VK_PAGE_DOWN_FID;
  static jfieldID keyEvent_DOM_VK_PAGE_UP_FID;
  static jfieldID keyEvent_DOM_VK_PASTE_FID;
  static jfieldID keyEvent_DOM_VK_PAUSE_FID;
  static jfieldID keyEvent_DOM_VK_PERIOD_FID;
  static jfieldID keyEvent_DOM_VK_PLUS_FID;
  static jfieldID keyEvent_DOM_VK_PREVIOUS_CANDIDATE_FID;
  static jfieldID keyEvent_DOM_VK_PRINTSCREEN_FID;
  static jfieldID keyEvent_DOM_VK_PROPS_FID;
  static jfieldID keyEvent_DOM_VK_Q_FID;
  static jfieldID keyEvent_DOM_VK_QUOTE_FID;
  static jfieldID keyEvent_DOM_VK_QUOTEDBL_FID;
  static jfieldID keyEvent_DOM_VK_R_FID;
  static jfieldID keyEvent_DOM_VK_RIGHT_FID;
  static jfieldID keyEvent_DOM_VK_RIGHT_PARENTHESIS_FID;
  static jfieldID keyEvent_DOM_VK_ROMAN_CHARACTERS_FID;
  static jfieldID keyEvent_DOM_VK_S_FID;
  static jfieldID keyEvent_DOM_VK_SCROLL_LOCK_FID;
  static jfieldID keyEvent_DOM_VK_SEMICOLON_FID;
  static jfieldID keyEvent_DOM_VK_SEPARATER_FID;
  static jfieldID keyEvent_DOM_VK_SHIFT_FID;
  static jfieldID keyEvent_DOM_VK_SLASH_FID;
  static jfieldID keyEvent_DOM_VK_SPACE_FID;
  static jfieldID keyEvent_DOM_VK_STOP_FID;
  static jfieldID keyEvent_DOM_VK_SUBTRACT_FID;
  static jfieldID keyEvent_DOM_VK_T_FID;
  static jfieldID keyEvent_DOM_VK_TAB_FID;
  static jfieldID keyEvent_DOM_VK_U_FID;
  static jfieldID keyEvent_DOM_VK_UNDEFINED_FID;
  static jfieldID keyEvent_DOM_VK_UNDERSCORE_FID;
  static jfieldID keyEvent_DOM_VK_UNDO_FID;
  static jfieldID keyEvent_DOM_VK_UP_FID;
  static jfieldID keyEvent_DOM_VK_V_FID;
  static jfieldID keyEvent_DOM_VK_W_FID;
  static jfieldID keyEvent_DOM_VK_X_FID;
  static jfieldID keyEvent_DOM_VK_Y_FID;
  static jfieldID keyEvent_DOM_VK_Z_FID;

  static jmethodID eventListenerHandleEventMID;

  static void Initialize(JNIEnv *env);
  static void Destroy(JNIEnv *env);
  static jobject CreateEventSubtype(JNIEnv *env, 
				   nsIDOMEvent *event);

  static jlong RegisterNativeEventListener();
  static jlong UnregisterNativeEventListener();
};

#endif /* __JavaDOMEventsGlobals_h__ */

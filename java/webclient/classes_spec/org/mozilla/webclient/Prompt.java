/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient;

import java.util.Properties;


/**

 * The custom app must implement this interface in order to supply the
 * underlying browser with basic authentication behavior.  The custom
 * app must tell webclient about its Prompt implementation by calling
 * Navigation.setPrompt().  This must be done FOR EACH BrowserControl
 * instance!

 */

public interface Prompt
{

public static final String USER_NAME_KEY = "userName";
public static final String PASSWORD_KEY = "password";
public static final String EDIT_FIELD_1_KEY = "editfield1Value";
public static final String EDIT_FIELD_2_KEY = "editfield2Value";
public static final String CHECKBOX_STATE_KEY = "checkboxState";
public static final String BUTTON_PRESSED_KEY = "buttonPressed";

/**

 * Puts up a username/password dialog with OK and Cancel buttons.

 * @param fillThis a pre-allocated properties object
 * that the callee fills in.

 * keys: userName, password
 
 * @return true for OK, false for Cancel

 */

public boolean promptUsernameAndPassword(String dialogTitle,
					 String text,
					 String passwordRealm,
					 int savePassword,
					 Properties fillThis);

/**

 * Tells the custom app to put up a modal dialog using the information
 * in the params for the dialog ui. <P>

 * keys: <P>

 * <DL>

 * <DT>editfield1Value</DT>
 * <DD>initial and final value for first edit field</DD>

 * <DT>editfield2Value</DT>
 * <DD>initial and final value for second edit field</DD>

 * <DT>checkboxState</DT>
 * <DD>initial and final state of checkbox: true or false</DD>

 * <DT>buttonPressed</DT>
 * <DD>number of button that was pressed (0 to 3)</DD>

 * </DL><P>

 * @param titleMessage

 * @param dialogTitle e.g., alert, confirm, prompt, prompt password 
 
 * @param text main message for dialog

 * @param checkboxMsg message for checkbox 

 * @param button0Text text for first button

 * @param button1Text text for second button

 * @param button2Text text for third button 

 * @param button3Text text for fourth button

 * @param editfield1Msg message for first edit field
 
 * @param editfield2Msg message for second edit field

 * @param numberButtons total number of buttons (0 to 4)

 * @param numberEditfields total number of edit fields (0 to 2) 
 
 * @param editField1Password whether or not editField1 is a password

 * @param fillThis the properties object to be filled with the above
 * keys

 */

public boolean universalDialog(String titleMessage,
                               String dialogTitle,
                               String text,
                               String checkboxMsg,
                               String button0Text,
                               String button1Text,
                               String button2Text,
                               String button3Text,
                               String editfield1Msg,
                               String editfield2Msg,
                               int numberButtons,
                               int numberEditfields,
                               boolean editField1Password,
                               Properties fillThis);
    
} // end of interface History

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define SIMULATE_ERROR  0

#if (SIMULATE_ERROR)
  extern long SIMULATED_SEND_ERROR_ID;

  enum
  {
    SIMULATED_SEND_ERROR_1 = 1, // Failure during the initialization
    SIMULATED_SEND_ERROR_2,     // Failure while openning message to copy
    SIMULATED_SEND_ERROR_3,     // Failure while opening a temp file for attachment handling
    SIMULATED_SEND_ERROR_4,     // Failure while opening a temp file for converting to plain text a part. JFD: IMPORTANT TO TEST THIS CASE!
    SIMULATED_SEND_ERROR_5,     // Failure while Locate a Message Folder
    SIMULATED_SEND_ERROR_6,     // Simulate no sender
    SIMULATED_SEND_ERROR_7,     // Simulate no recipient - nsMsgSend
    SIMULATED_SEND_ERROR_8,     // Simulate no recipient - inside smtp
    SIMULATED_SEND_ERROR_9,     // Simulate failure in mime_write_message_body
    SIMULATED_SEND_ERROR_10,    // Simulate SMTP logging error
    SIMULATED_SEND_ERROR_11,    // Simulate SMTP protocol error (1)
    SIMULATED_SEND_ERROR_12,    // Simulate SMTP protocol error (2)
    SIMULATED_SEND_ERROR_13,    // Simulate Sent unsent messages error
    SIMULATED_SEND_ERROR_14,    // Simulate charset conversion error (plaintext only)
    SIMULATED_SEND_ERROR_15,    // Simulate large message warning
    SIMULATED_SEND_ERROR_16,    // Failure getting user email address in smtp
    SIMULATED_SEND_ERROR_17,    // Failure while retreiving multipart related object
    
    SIMULATED_SEND_ERROR_EOL
  };
  
  //JFD: last error simulated is NS_ERROR_BUT_DONT_SHOW_ALERT

  long SIMULATED_SEND_ERROR_ID = 0;
  
  #define SET_SIMULATED_ERROR(id, var, err) \
    if (SIMULATED_SEND_ERROR_ID == id)      \
      var = err;

  #define RETURN_SIMULATED_ERROR(id, err) \
    if (SIMULATED_SEND_ERROR_ID == id)      \
      return err;

  #define CHECK_SIMULATED_ERROR(id) \
    (SIMULATED_SEND_ERROR_ID == id)

#else

  #define SET_SIMULATED_ERROR(id, var, err)
  #define RETURN_SIMULATED_ERROR(id, err)
  #define CHECK_SIMULATED_ERROR(id) (0)
  
#endif

/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 * Seth Spitzer <sspitzer@netscape.com>
 */

var protocolinfo = null;
var Bundle = srGetStrBundle("chrome://messenger/locale/prefs.properties");

function validate() {
  var username = document.getElementById("server.username").value;

  if (protocolinfo.requiresUsername && (!username || username == "")) { 
      var alertText = Bundle.GetStringFromName("enterUserName");
      window.alert(alertText);
      return false;
  }

  return true;
}

function onInit() {
    var loginNameInput = document.getElementById("server.username");
    var type = document.getElementById("server.type").value;

    protocolinfo = Components.classes["component://netscape/messenger/protocol/info;type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);

    if (loginNameInput.value == "") {
      if (protocolinfo.requiresUsername) {
        // since we require a username, use the uid from the email address
        var email = document.getElementById("identity.email").value;
        var emailParts = email.split("@");
        loginNameInput.value = emailParts[0];
      }
    }
}


var savedPassword="";

function onSavePassword(target) {
    dump("savePassword changed! (" + target.checked + ")\n");
    var passwordField = document.getElementById("server.password");
    
    if (target.checked) {
        passwordField.removeAttribute("disabled");
        passwordField.value = savedPassword;
    }
    else {
        passwordField.setAttribute("disabled", "true");
        savedPassword = passwordField.value;
        passwordField.value = "";
    }
    
}

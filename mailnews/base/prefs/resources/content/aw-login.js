/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Alec Flett <alecf@netscape.com>
 * Seth Spitzer <sspitzer@netscape.com>
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

var protocolinfo = null;
var gPrefsBundle;

function validate() {
  var username = document.getElementById("username").value;

  if (protocolinfo && protocolinfo.requiresUsername && (!username || username == "")) { 
      var alertText = gPrefsBundle.getString("enterUserName");
      window.alert(alertText);
      return false;
  }

  var pageData = parent.GetPageData();
  var serverType = parent.getCurrentServerType(pageData);
  var hostName = parent.getCurrentHostname(pageData);

  if (parent.AccountExists(username,hostName,serverType)) {
    alertText = gPrefsBundle.getString("accountExists");
    window.alert(alertText);
    return false;
  }
  return true;
}

function onInit() {
    gPrefsBundle = document.getElementById("bundle_prefs");
    var loginNameInput = document.getElementById("username");
    
    if (loginNameInput.value == "") {
      // retrieve data from previously entered pages
      var pageData = parent.wizardManager.WSM.PageData;
      var type = parent.getCurrentServerType(pageData);

      dump("type = " + type + "\n");
      protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);

      if (protocolinfo.requiresUsername) {
        // since we require a username, use the uid from the email address
        var email = pageData.identity.email.value;
        var emailParts = email.split("@");
        loginNameInput.value = emailParts[0];
      }
    }
}


var savedPassword="";

function onSavePassword(target) {
    dump("savePassword changed! (" + target.checked + ")\n");
    var passwordField = document.getElementById("server.password");
    if (!passwordField) return;
    
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

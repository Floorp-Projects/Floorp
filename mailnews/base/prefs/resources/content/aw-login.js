/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Alec Flett <alecf@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var protocolinfo = null;
var gPrefsBundle;

function loginPageValidate() {
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

  // If SMTP username box is blank it is because the
  // incoming and outgoing server names were the same,
  // so set to be same as incoming username
  var smtpusername = document.getElementById("smtpusername").value || username;

  setPageData(pageData, "login", "username", username);
  setPageData(pageData, "login", "smtpusername", smtpusername);

  return true;
}

function loginPageInit() {
    gPrefsBundle = document.getElementById("bundle_prefs");
    var pageData = parent.GetPageData();
    var loginNameInput = document.getElementById("username");
    
    if (loginNameInput.value == "") {
      // retrieve data from previously entered pages
      var type = parent.getCurrentServerType(pageData);

      dump("type = " + type + "\n");
      protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);

      if (protocolinfo.requiresUsername) {
        // since we require a username, use the uid from the email address
        loginNameInput.value = parent.getUsernameFromEmail(pageData.identity.email.value);
      }
    }

    var smtpNameInput = document.getElementById("smtpusername");
    var smtpServer = parent.smtpService.defaultServer;
    if (smtpServer && smtpServer.hostname && smtpServer.username &&
        smtpServer.redirectorType == null) {
      // we have a default SMTP server, so modify and show the static text
      // and store the username for the default server in the textbox.
      modifyStaticText(smtpServer.username, "2")
      hideShowLoginSettings(2, 1, 3);
      smtpNameInput.value = smtpServer.username;
    }
    else {
      // no default SMTP server yet, so need to compare
      // incoming and outgoing server names
      var smtpServerName = pageData.server.smtphostname.value;
      var incomingServerName = pageData.server.hostname.value;
      if (smtpServerName == incomingServerName) {
        // incoming and outgoing server names are the same, so show
        // the static text and make sure textbox blank for later tests.
        modifyStaticText(smtpServerName, "3")
        hideShowLoginSettings(3, 1, 2);
        smtpNameInput.value = "";
      }
      else {
        // incoming and outgoing server names are different, so set smtp
        // username's textbox to be the same as incoming's one, unless already set.
        hideShowLoginSettings(1, 2, 3);
        smtpNameInput.value = smtpNameInput.value || loginNameInput.value;
      }
    }
}

function hideShowLoginSettings(aEle, bEle, cEle)
{
    document.getElementById("loginSet" + aEle).hidden = false;
    document.getElementById("loginSet" + bEle).hidden = true;
    document.getElementById("loginSet" + cEle).hidden = true;
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

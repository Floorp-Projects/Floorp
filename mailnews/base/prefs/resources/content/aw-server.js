/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

var gPrefsBundle;

function hostnameIsIllegal(hostname)
{
  // XXX TODO do a complete check.
  // this only checks for illegal characters in the hostname
  // but hostnames like "...." and "_" and ".111" will get by
  // my test.  
  var validChars = hostname.match(/[A-Za-z0-9.-]/g);
  if (!validChars || (validChars.length != hostname.length)) {
    return true;
  }

  return false;
}

function validate() 
{
  var servername = document.getElementById("hostname");
  var smtpserver = document.getElementById("smtphostname");

  if ((servername && hostnameIsIllegal(servername.value)) ||
      (smtpserver && hostnameIsIllegal(smtpserver.value))) {
    var alertText = gPrefsBundle.getString("enterValidHostname");
    window.alert(alertText);
    return false;
  }

  /* if this is for a server that doesn't require a username, 
   * check if the account exists. 
   * for other types, we check after the user provides the username (see aw-login.js)
   */
  var pageData = parent.GetPageData();
  var serverType = parent.getCurrentServerType(pageData);
  var protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + serverType].getService(Components.interfaces.nsIMsgProtocolInfo);
  if (!protocolinfo.requiresUsername) {
    var userName = parent.getCurrentUserName(pageData);
    var hostName = servername.value;

    if (parent.AccountExists(userName,hostName,serverType)) {
      alertText = gPrefsBundle.getString("accountExists");
      window.alert(alertText);
      return false;
    }
  }

  return true;
}

function onInit() {
  // Server type selection (pop3 or imap) is for mail accounts only
  var isMailAccount = parent.GetPageData().accounttype.mailaccount;
  if (isMailAccount && isMailAccount.value) {
    var serverTypeRadioGroup = document.getElementById("servertype");
    /* 
     * Check to see if the radiogroup has any value. If there is no
     * value, this must be the first time user visting this page in the
     * account setup process. So, the default is set to pop3. If there 
     * is a value (it's used automatically), user has already visited 
     * page and server type selection is done. Once user visits the page, 
     * the server type value from then on will persist (whether the selection 
     * came from the default or the user action).
     */
    if (!serverTypeRadioGroup.value) {
      // Set pop3 server type as default selection
      var pop3RadioItem = document.getElementById("pop3");
      serverTypeRadioGroup.selectedItem = pop3RadioItem;
    }
  }

  gPrefsBundle = document.getElementById("bundle_prefs");
  var smtpTextBox = document.getElementById("smtphostname");

  var smtpServer = parent.smtpService.defaultServer;
  if (smtpTextBox && smtpTextBox.value == "" &&
      smtpServer.hostname)
    smtpTextBox.value = smtpServer.hostname;

  // modify the value in the smtp display if we already have a 
  // smtp server so that the single string displays the 
  // name of the smtp server.
  var smtpStatic = document.getElementById("smtpStaticText");
  if (smtpServer && smtpServer.hostname && smtpStatic &&
      smtpStatic.hasChildNodes()) {
    var staticText = smtpStatic.firstChild.nodeValue;
    staticText = staticText.replace(/@server_name@/, smtpServer.hostname);
    while (smtpStatic.hasChildNodes())
      smtpStatic.removeChild(smtpStatic.firstChild);
    var staticTextNode = document.createTextNode(staticText);
    smtpStatic.appendChild(staticTextNode);
  }
  
  hideShowSmtpSettings(smtpServer);
}

function hideShowSmtpSettings(smtpServer) {

  var noSmtpBox = document.getElementById("noSmtp");
  var haveSmtpBox = document.getElementById("haveSmtp");

  var boxToHide;
  var boxToShow;
  
  if (smtpServer && smtpServer.hostname &&
      smtpServer.hostname != "") {
    // we have a hostname, so show the static text
    boxToShow = haveSmtpBox;
    boxToHide = noSmtpBox;
  } else {
    // no default hostname yet
    boxToShow = noSmtpBox;
    boxToHide = haveSmtpBox;
  }

  if (boxToHide)
    boxToHide.setAttribute("hidden", "true");

  if (boxToShow)
    boxToShow.removeAttribute("hidden");


}

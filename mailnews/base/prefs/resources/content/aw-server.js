/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

var Bundle = srGetStrBundle("chrome://messenger/locale/prefs.properties");

function validate() {
  var servername = document.getElementById("hostname");
  var smtpserver = document.getElementById("smtphostname");

  if ((servername && servername.value =="") ||
      (smtpserver && smtpserver.value == "")) {
    var alertText = Bundle.GetStringFromName("enterValidHostname");
    window.alert(alertText);
    return false;
  }

  /* if this is for a server that doesn't require a username, 
   * check if the account exists. 
   * for other types, we check after the user provides the username (see aw-login.js)
   */
  var pageData = parent.GetPageData();
  var serverType = parent.getCurrentServerType(pageData);
  var protocolinfo = Components.classes["component://netscape/messenger/protocol/info;type=" + serverType].getService(Components.interfaces.nsIMsgProtocolInfo);
  if (!protocolinfo.requiresUsername) {
    var userName = parent.getCurrentUserName(pageData);
    var hostName = servername.value;

    if (parent.AccountExists(userName,hostName,serverType)) {
      var alertText = Bundle.GetStringFromName("accountExists");
      window.alert(alertText);
      return false;
    }
  }

  return true;
}

function onInit() {
  
  var smtpTextField = document.getElementById("smtphostname");

  var smtpServer = parent.smtpService.defaultServer;
  if (smtpTextField && smtpTextField.value == "" &&
      smtpServer.hostname)
    smtpTextField.value = smtpServer.hostname;

  setDivText("smtpStaticText", smtpServer.hostname);

  
  hideShowSmtpSettings(smtpServer);
}

function setDivText(id, value) {
  if (!value) return;
  
  var div = document.getElementById("smtpStaticText");
  if (!div) return;

  div.setAttribute("value", value);
}

function hideShowSmtpSettings(smtpServer) {

  var noSmtpBox = document.getElementById("noSmtp");
  var haveSmtpBox = document.getElementById("haveSmtp");

  var boxToHide;
  var boxToShow;
  
  if (smtpServer && smtpServer.hostname &&
      smtpServer.hostname != "") {
    // we have a hostname, so show the static text
    
    // XXX, UI - this text provides little value to the vast majority of
    //           users who will not need or want to specify a second
    //           smtp server. As this panel is cramped for space, and
    //           as this text offers nothing other than visual clutter,
    //           it is being removed. - Ben (09/06/2000)
    
    boxToShow = haveSmtpBox;
    boxToHide = noSmtpBox;

    // turn off but leave logic behind, just in case.
    if (boxToShow) 
      boxToShow.setAttribute("hidden", "true");
    boxToShow = null;
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

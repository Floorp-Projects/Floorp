/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *   Henrik Gemal <mozilla@gemal.dk>
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

// be real hacky with document.getElementById until document.controls works
// with the new XUL widgets

var gSmtpUsername;
var gSmtpUsernameLabel;
var gSmtpHostname;
var gSmtpPort;
var gSmtpUseUsername;
var gSmtpAuthMethod;
var gSmtpTrySSL;
var gSmtpPrefBranch;
var gPrefBranch = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
var gSmtpService = Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);
var gSavedUsername="";
var gPort;
var gDefaultPort;

function initSmtpSettings(server) {

    gSmtpUsername = document.getElementById("smtp.username");
    gSmtpUsernameLabel = document.getElementById("smtpusernamelabel");
    gSmtpHostname = document.getElementById("smtp.hostname");
    gSmtpPort = document.getElementById("smtp.port");
    gSmtpUseUsername = document.getElementById("smtp.useUsername");
    gSmtpAuthMethod = document.getElementById("smtp.authMethod");
    gSmtpTrySSL = document.getElementById("smtp.trySSL");
    gDefaultPort = document.getElementById("smtp.defaultPort");
    gPort = document.getElementById("smtp.port");

    if (server) {
        gSmtpHostname.value = server.hostname;
        gSmtpPort.value = server.port ? server.port : "";
        gSmtpUsername.value = server.username;
        gSmtpAuthMethod.setAttribute("value", server.authMethod);
        gSmtpTrySSL.value = (server.trySSL < 4) ? server.trySSL : 1;
    } else {
        gSmtpAuthMethod.setAttribute("value", "1");
        gSmtpTrySSL.value = 1;
    }

    gSmtpUseUsername.checked = (gSmtpAuthMethod.getAttribute("value") == "1");

    //dump("gSmtpAuthMethod = <" + gSmtpAuthMethod.localName + ">\n");
    //dump("gSmtpAuthMethod.value = " + gSmtpAuthMethod.getAttribute("value") + "\n");

    onUseUsername(gSmtpUseUsername, false);
    updateControls();
    selectProtocol(1);
    if (gSmtpService.defaultServer)
      onLockPreference();
}

// Disables xul elements that have associated preferences locked.
function onLockPreference()
{
    var defaultSmtpServerKey = gPrefBranch.getCharPref("mail.smtp.defaultserver");
    var finalPrefString = "mail.smtpserver." + defaultSmtpServerKey + "."; 

    var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);

    var allPrefElements = [
      { prefstring:"hostname", id:"smtp.hostname"},
      { prefstring:"port", id:"smtp.port"},
      { prefstring:"use_username", id:"smtp.useUsername"},
      { prefstring:"try_ssl", id:"smtp.trySSL"}
    ];

    gSmtpPrefBranch = prefService.getBranch(finalPrefString);
    disableIfLocked( allPrefElements );
} 

// Does the work of disabling an element given the array which contains xul id/prefstring pairs.
// Also saves the id/locked state in an array so that other areas of the code can avoid
// stomping on the disabled state indiscriminately.
function disableIfLocked( prefstrArray )
{
  for (var i=0; i<prefstrArray.length; i++) {
    var id = prefstrArray[i].id;
    var element = document.getElementById(id);
    if (gSmtpPrefBranch.prefIsLocked(prefstrArray[i].prefstring)) {
      if (id == "smtp.trySSL")
      {
        document.getElementById("smtp.neverSecure").setAttribute("disabled", "true");
        document.getElementById("smtp.sometimesSecure").setAttribute("disabled", "true");
        document.getElementById("smtp.alwaysSecure").setAttribute("disabled", "true");
      }
      else
        element.setAttribute("disabled", "true");
    }
  }
}

function saveSmtpSettings(server)
{

    if (gSmtpUseUsername.checked)
        gSmtpAuthMethod.setAttribute("value", "1");
    else
        gSmtpAuthMethod.setAttribute("value", "0");

    //dump("Saving to " + server + "\n");
    if (server) {
        server.hostname = gSmtpHostname.value;
        server.port = gSmtpPort.value;
        server.authMethod = (gSmtpUseUsername.checked ? 1 : 0);
        //dump("Saved authmethod = " + server.authMethod +
        //     " but checked = " + gSmtpUseUsername.checked + "\n");
        server.username = gSmtpUsername.value;
        server.trySSL = gSmtpTrySSL.value;
    }
}

function onUseUsername(checkbox, dofocus)
{
    if (checkbox.checked) {
        // not only do we enable the elements when the check box is checked,
        // but we also make sure that it's not disabled (ie locked) as well.
        if (!checkbox.disabled) {
            gSmtpUsername.removeAttribute("disabled");
            gSmtpUsernameLabel.removeAttribute("disabled");
        }
        if (dofocus)
            gSmtpUsername.focus();
        if (gSavedUsername && gSavedUsername != "")
            gSmtpUsername.value = gSavedUsername;
    } else {
        gSavedUsername = gSmtpUsername.value;
        gSmtpUsername.value = "";
        gSmtpUsername.setAttribute("disabled", "true");
        gSmtpUsernameLabel.setAttribute("disabled", "true");
    }
}

function updateControls() {
    if (gSmtpTrySSL.disabled)  // see bug 70033 on why this is necessary for radiobuttons
        gSmtpTrySSL.disabled = gSmtpTrySSL.disabled;
}

function selectProtocol(init) {
  var prevDefaultPort = gDefaultPort.value;

  if (gSmtpTrySSL.value == 3) {
    gDefaultPort.value = "465";
    if(gPort.value == "" || (!init && gPort.value == "25" && prevDefaultPort != gDefaultPort.value))
        gPort.value = gDefaultPort.value;
  } else {
    gDefaultPort.value = "25";
    if(gPort.value == "" || (!init && gPort.value == "465" && prevDefaultPort != gDefaultPort.value))
        gPort.value = gDefaultPort.value;
  }
}

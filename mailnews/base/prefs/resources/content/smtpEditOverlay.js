/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

// be real hacky with document.getElementById until document.controls works
// with the new XUL widgets

var gSmtpUsername;
var gSmtpHostname;
var gSmtpUseUsername;
var gSmtpSavePassword;

var gSavedUsername="";

function initSmtpSettings(server) {

    gSmtpUsername = document.getElementById("smtp.username");
    gSmtpHostname = document.getElementById("smtp.hostname");
    gSmtpUseUsername = document.getElementById("smtp.useUsername");
    gSmtpAuthMethod = document.getElementById("smtp.authMethod");
    gSmtpSavePassword = document.getElementById("smtp.savePassword")
    
    if (server) {
        gSmtpHostname.value = server.hostname;
        gSmtpUsername.value = server.username;
        gSmtpSavePassword.checked = server.savePassword;
        gSmtpAuthMethod.setAttribute("value", server.authMethod);
        // radio groups not implemented
        //document.getElementById("smtp.trySSL").value = server.trySSL;

    }

    if (gSmtpAuthMethod.getAttribute("value") == "1")
        gSmtpUseUsername.checked = true;
    
    dump("gSmtpAuthMethod = <" + gSmtpAuthMethod.localName + ">\n");
    dump("gSmtpAuthMethod.value = " + gSmtpAuthMethod.getAttribute("value") + "\n");
    
    onUseUsername(gSmtpUseUsername, false);
    updateControls();
}

function saveSmtpSettings(server)
{

    if (gSmtpUseUsername.checked)
        gSmtpAuthMethod.setAttribute("value", "1");
    else
        gSmtpAuthMethod.setAttribute("value", "0");

    dump("Saving to " + server + "\n");
    if (server) {
        server.hostname = gSmtpHostname.value;
        server.authMethod = (gSmtpUseUsername.checked ? 1 : 0);
        dump("Saved authmethod = " + server.authMethod +
             " but checked = " + gSmtpUseUsername.checked + "\n");
        server.username = gSmtpUsername.value;
        //server.savePassword = gSmtpSavePassword.checked;
    }
}

function onUseUsername(checkbox, dofocus)
{
    if (checkbox.checked) {
        gSmtpUsername.removeAttribute("disabled");
        gSmtpSavePassword.removeAttribute("disabled");
        if (dofocus)
            gSmtpUsername.focus();
        if (gSavedUsername && gSavedUsername != "")
            gSmtpUsername.value = gSavedUsername;
    } else {
        gSavedUsername = gSmtpUsername.value;
        gSmtpUsername.value = "";
        gSmtpUsername.setAttribute("disabled", "true");
        gSmtpSavePassword.setAttribute("disabled", "true");
    }        
}

function updateControls() {

    dump("Update controls..\n");

    var isSecure =
        document.getElementById("smtp.isSecure").checked;

    if (isSecure) {
        document.getElementById("smtp.alwaysSecure").removeAttribute("disabled");
        document.getElementById("smtp.sometimesSecure").removeAttribute("disabled");
    }

    else {
        document.getElementById("smtp.alwaysSecure").setAttribute("disabled", "true");
        document.getElementById("smtp.sometimesSecure").setAttribute("disabled", "true");
    }

}

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
 *   Alec Flett <alecf@netscape.com>
 *   Henrik Gemal <gemal@gemal.dk>
 */

// be real hacky with document.getElementById until document.controls works
// with the new XUL widgets

var gSmtpUsername;
var gSmtpUsernameLabel;
var gSmtpHostname;
var gSmtpUseUsername;
var gSmtpAuthMethod;
var gSmtpTrySSL;

var gSavedUsername="";

function initSmtpSettings(server) {

    gSmtpUsername = document.getElementById("smtp.username");
    gSmtpUsernameLabel = document.getElementById("smtpusernamelabel");
    gSmtpHostname = document.getElementById("smtp.hostname");
    gSmtpUseUsername = document.getElementById("smtp.useUsername");
    gSmtpAuthMethod = document.getElementById("smtp.authMethod");
    gSmtpTrySSL = document.getElementById("smtp.trySSL");
    
    if (server) {
        gSmtpHostname.value = server.hostname;
        gSmtpUsername.value = server.username;
        gSmtpAuthMethod.setAttribute("value", server.authMethod);

        var elements = [];
        if (server.trySSL != "")
            elements = gSmtpTrySSL.getElementsByAttribute("data", server.trySSL);
        if (elements.length == 0)
            elements = gSmtpTrySSL.getElementsByAttribute("data", "1");
        gSmtpTrySSL.selectedItem = elements[0];
    }

    if (gSmtpAuthMethod.getAttribute("value") == "1")
        gSmtpUseUsername.checked = true;
    
    //dump("gSmtpAuthMethod = <" + gSmtpAuthMethod.localName + ">\n");
    //dump("gSmtpAuthMethod.value = " + gSmtpAuthMethod.getAttribute("value") + "\n");
    
    onUseUsername(gSmtpUseUsername, false);
    updateControls();
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
        server.authMethod = (gSmtpUseUsername.checked ? 1 : 0);
        //dump("Saved authmethod = " + server.authMethod +
        //     " but checked = " + gSmtpUseUsername.checked + "\n");
        server.username = gSmtpUsername.value;
        server.trySSL = gSmtpTrySSL.selectedItem.data;
    }
}

function onUseUsername(checkbox, dofocus)
{
    if (checkbox.checked) {
        gSmtpUsername.removeAttribute("disabled");
        gSmtpUsernameLabel.removeAttribute("disabled");
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

}

/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

        var elements = gSmtpTrySSL.getElementsByAttribute("value", server.trySSL);
        if (elements.length == 0)
            elements = gSmtpTrySSL.getElementsByAttribute("value", "1");
        gSmtpTrySSL.selectedItem = elements[0];
    } else {
        gSmtpAuthMethod.setAttribute("value", "1");
        gSmtpTrySSL.selectedItem =
            gSmtpTrySSL.getElementsByAttribute("value", "1")[0];
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
        server.trySSL = gSmtpTrySSL.selectedItem.value;
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

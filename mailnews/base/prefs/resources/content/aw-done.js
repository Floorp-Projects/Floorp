/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

function onInit() {
    var pageData = parent.wizardManager.WSM.PageData;

    var email = "";
    if (pageData.identity && pageData.identity.email) {
        // fixup the email
        email = pageData.identity.email.value;
        if (email.split('@').length < 2 && parent.gCurrentAccountData.domain)
            email += "@" + parent.gCurrentAccountData.domain;
    }
    setDivTextFromForm("identity.email", email);

    var username="";
    if (pageData.login && pageData.login.username)
        username = pageData.login.username.value;
    setDivTextFromForm("server.username", username);
}

function setDivTextFromForm(divid, value) {

    // hide the .label if the div has no value
    if (!value || value =="") {
        var div = document.getElementById(divid + ".label");
        div.setAttribute("hidden","true");
        return;
    }

    // otherwise fill in the .text element
    div = document.getElementById(divid+".text");
    if (!div) return;

    div.setAttribute("value", value);
}

function setupAnother(event)
{
    window.alert("Unimplemented, see bug #19982");
}

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

function initSmtpSettings(server) {
    if (!server) return;
    
    document.getElementById("smtp.hostname").value = server.hostname;
    document.getElementById("smtp.alwaysUseUsername").checked =
        server.alwaysUseUsername;
    document.getElementById("smtp.username").value = server.username;
    document.getElementById("smtp.savePassword").checked = server.savePassword;
    // radio groups not implemented
    //document.getElementById("smtp.trySSL").value = server.trySSL;
}

function saveSmtpSettings(server)
{
    if (!server) return;

    server.hostname = document.getElementById("smtp.hostname").value;
    //    server.alwaysUseUsername = document.getElementById("smtp.alwaysUseUsername").checked;
    server.username = document.getElementById("smtp.username").value;
    //server.savePassword = document.getElementById("smtp.savePassword").checked;
    
}

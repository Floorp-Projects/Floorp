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

function onAdvanced()
{
    var oldSmtpServer = null; // where will I get this from?
    var arg = {result: false,
               smtpServer: oldSmtpServer };
    window.openDialog('am-identity-advanced.xul','advanced',
                      'modal,chrome', arg);
    
    if (arg.result && arg.smtpServer != oldSmtpServer) {
        // save the identity back to the page as a key
        document.getElementById("identity.smtpServerKey").value =
            arg.smtpServer.key;
    }
}

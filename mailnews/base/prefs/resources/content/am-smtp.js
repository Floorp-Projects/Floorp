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

function onLoad()
{
    parent.onPanelLoaded('am-smtp.xul');
    initSmtpSettings(null);
}

function onSave()
{
    saveSmtpSettings(null);
}


function onAdvanced(event)
{
    var args = {result: false};
    window.openDialog('chrome://messenger/content/SmtpServerList.xul', 'smtp', 'modal,titlebar,chrome', args);

    if (args.result) {
        // this is the wrong way to do this.
        dump("reloading panel...\n");
        onLoad();
    }
}


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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

function onInit() {
    // select the first radio button
    document.getElementById("mailaccount").checked=true;
}

function onUnload() {
    dump("OnUnload!\n");
    parent.UpdateWizardMap();

    initializeIspData();
    
    return true;
}


function initializeIspData()
{
    if (!document.getElementById("mailaccount").checked) {
        parent.SetCurrentAccountData(null);
    }
    
    // now reflect the datasource up into the parent
    var accountSelection = document.getElementById("acctyperadio");

    var ispName = accountSelection.selectedItem.id;

    dump("initializing ISP data for " + ispName + "\n");
    
    if (!ispName || ispName == "") return;

    parent.PrefillAccountForIsp(ispName);
    parent.UpdateWizardMap();
}


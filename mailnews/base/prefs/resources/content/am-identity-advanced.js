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

var serverList;

function init() {

    serverList = document.getElementById("smtpServerList");

}

function preSelectServer(key)
{
    if (!key) {
        // no key, so preselect the "useDefault" item.
        serverList.selectedItem = document.getElementById("useDefaultItem");
        return;
    }
    
    var preselectedItems = serverList.getElementsByAttribute("key", key);

    if (preselectedItems && preselectedItems.length > 0)
        serverList.selectedItem = preselectedItems[0];
}

function onLoad()
{
    init();
    if (window.arguments && window.arguments[0]) {
        var selectedServerKey = window.arguments[0].smtpServerKey;
        preSelectServer(selectedServerKey);
    }


    doSetOKCancel(onOk, 0);
}

function onOk()
{

    var selectedServerElement = serverList.selectedItem;
    dump("selectedServerKey: " + selectedServerElement + "\n");

    var key = selectedServerElement.getAttribute("key");
    if (key)
        window.arguments[0].smtpServerKey = key;
    else
        window.arguments[0].smtpServerKey = null;
    
    window.close();
}

function onSelected(event)
{
    serverList.setAttribute("selectedKey", event.target.getAttribute("key"));
    dump("Something selected on " + event.target.localName + "\n");
}

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

// pull stuff out of window.arguments
var server=window.arguments[0];

// initialize the controls with the "server" argument

var gControls;
function getControls() {
    if (!gControls)
        gControls = document.getElementsByAttribute("imap_persist", "true");
    return gControls;
}
    
function onLoad()
{
    var controls = getControls();

    for (var i=0; i<controls.length; i++) {

        var slot = controls[i].id;
        var val = server[slot];

        if (val) {
            if (controls[i].tagName.toLowerCase() == "checkbox") {
                controls[i].checked = val;
            }
            else
                controls[i].value = val;
        }
    }

    doSetOKCancel(onOk, 0);
}

// save the controls back to the "server" array
function onOk()
{
    var controls = getControls();
    
    for (var i=0; i<controls.length; i++) {
        var slot = controls[i].id;
        if (slot) {
            if (controls[i].tagName.toLowerCase() == "checkbox") {
                 server[slot] = controls[i].checked;
            }
            else
                server[slot] = controls[i].value;
        }
    }

    return true;
}

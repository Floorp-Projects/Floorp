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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
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

        if (controls[i].localName.toLowerCase() == "checkbox")
            controls[i].checked = val;
        else
            controls[i].value = val;
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
            if (controls[i].localName.toLowerCase() == "checkbox") {
                 server[slot] = controls[i].checked;
            }
            else
                server[slot] = controls[i].value;
        }
    }

    return true;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
    fixLabels(document.getElementById("ispBox"));

    
}


// this is a workaround for a template shortcoming.
// basically: templates can't set the "id" attribute on a node, so we
// have to set "fakeid" and then transfer it manually
function fixLabels(box) {
    if (!box) return;
    var child = box.firstChild;

    var haveDynamicInputs = false;
    while (child) {
        if (child.tagName.toLowerCase() == "html") {
            var input = child.childNodes[0];
            var label = child.childNodes[1];

            if (input.tagName.toLowerCase() == "input" &&
                label.tagName.toLowerCase() == "label") {
                
                var fakeid = input.getAttribute("fakeid");
                if (fakeid && fakeid != "") {
                    input.setAttribute("id", fakeid);
                    haveDynamicInputs = true;
                }
            }
        }

        child = child.nextSibling;
    }

    if (haveDynamicInputs) {
        var subButtons = document.getElementById("mailSubButtons");
        subButtons.removeAttribute("hidden");
    }
}

function onMailChanged(event) {
    //    enableControls(document.getElementById("ispBox"),
    //                   event.target.checked);
}

function enableControls(node, enabled)
{
    var tagName = node.tagName.toLowerCase();
    if (tagName == "input" || tagName == "label") {
        if (enabled)
            node.setAttribute("disabled", "true");
        else
            node.removeAttribute("disabled");
    }

    var child = node.firstChild();
    while (child) {
        enableIspButtons(child, enabled);
        child = child.nextSibling;
    }
}

function onUnload() {
    parent.UpdateWizardMap();

    initializeIspData();
    
    return true;
}


function initializeIspData()
{
    if (!document.getElementById("mailaccount").checked) {
        parent.SetCurrentAccountData(null);
        return;
    }
    
    var ispName;
    // now reflect the datasource up into the parent
    var controls = document.controls;
    
    for (var i=0; i<controls.length ;i++) {
        var formElement = controls[i];
        if (formElement.name == "ispchoice" &&
            formElement.checked) {
            dump("ispName = " + formElement.parentNode.id + "\n");
            ispName = formElement.parentNode.id;
            break;
        }
    }
    
    if (!ispName || ispName == "") return;
    
    dump("initializing ISP data for " + ispName + "\n");

    parent.PrefillAccountForIsp(ispName);
    parent.UpdateWizardMap();
}

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

var DEBUG_alecf = false; 
function _dd(aString)
{
  if (DEBUG_alecf)
    dump(aString);
}
 
function onInit() {
    fixLabels(document.getElementById("ispBox"));

    preselectRadioButtons();
    
    onTypeChanged();
}

// this is a workaround for #44713 - I can't preselect radio buttons
// by marking them with checked="true"
function preselectRadioButtons()
{
    try {
        // select the first item in the outer group (mail account)
        var radiogroup = document.getElementById("acctyperadio");
        if (!radiogroup.selectedItem) {
            radiogroup.selectedItem =
                radiogroup.getElementsByAttribute("group", radiogroup.id)[0];
        }

        // select the last item in the inner group (other ISP)
        radiogroup = document.getElementById("ispchoice");
        if (!radiogroup.selectedItem) {
            var radiobuttons =
                radiogroup.getElementsByAttribute("group", radiogroup.id);
            radiogroup.selectedItem = radiobuttons[radiobuttons.length-1];
        }
        
    } catch (ex) {
        _dd("Error preselecting a radio button: " + ex + "!\n");
    }
}

// this is a workaround for a template shortcoming.
// basically: templates can't set the "id" attribute on a node, so we
// have to set "fakeid" and then transfer it manually
function fixLabels(box) {
    _dd("Fixing labels on " + box + "\n");
    if (!box) return;
    var child = box.firstChild;

    _dd("starting looking with " + child + "\n");
    var haveDynamicInputs = false;
    while (child) {

        _dd("found child: " + child + "\n");
        if (child.localName.toLowerCase() == "radio") {
            _dd("Found dynamic inputs!\n");
            haveDynamicInputs = true;
        }

        child = child.nextSibling;
    }

    if (haveDynamicInputs) {
        var subButtons = document.getElementById("ispchoice");
        _dd("** Have dynamic inputs: showing " + subButtons + "\n");
        subButtons.removeAttribute("hidden");

        var otherIspRadio = document.getElementById("otherIspRadio");
        _dd("** Also showing the other ISP button\n");
        otherIspRadio.removeAttribute("hidden");
    }
}

function onTypeChanged() {
    var ispBox = document.getElementById("ispBox");

    var mailaccount = document.getElementById("mailaccount");
    enableControls(ispBox, mailaccount.checked);
}

function enableControls(container, enabled)
{
    if (!container) return;
    var child = container.firstChild;
    while (child) {
        enableIspButtons(child, enabled);
        child = child.nextSibling;
    }
    var otherIspRadio = document.getElementById("otherIspRadio");
    enableIspButtons(otherIspRadio, enabled);
}

function enableIspButtons(child, enabled)
{
    _dd("disabling " + child.id + "\n");
    if (enabled)
        child.removeAttribute("disabled");
    else
        child.setAttribute("disabled", "true");

}

function onUnload() {
    _dd("OnUnload!\n");
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
    var controls = document.getElementsByAttribute("wsm_persist", "true");
    
    for (var i=0; i<controls.length ;i++) {
        var formElement = controls[i];
        if (formElement.getAttribute("group") == "ispchoice" &&
            formElement.checked) {
            _dd("ispName = " + formElement.id + "\n");
            ispName = formElement.id;
            break;
        }
    }

    _dd("initializing ISP data for " + ispName + "\n");
    
    if (!ispName || ispName == "") return;

    parent.PrefillAccountForIsp(ispName);
    parent.UpdateWizardMap();
}


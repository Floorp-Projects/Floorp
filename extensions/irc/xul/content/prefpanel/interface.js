/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is ChatZilla.
 *
 * The Initial Developer of the Original Code is
 * ____________________________________________.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Ross, <twpol@aol.com>, original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function Init() {
    parent.initPanel("chrome://chatzilla/content/prefpanel/interface.xul");
    initPMTabsPref();
}

function initPMTabsPref()
{
    var allowTabs = document.getElementById("czCreateTabsForPMs");
    var limitTabs = document.getElementById("czLimitPMTabs");
    var userTabs = document.getElementById("csPMTabLimitUser");
    var prefBox = document.getElementById("czPMTabLimit");
    
    if (prefBox.value == 0)
    {
        allowTabs.checked = true;
        limitTabs.checked = false;
        userTabs.value = "15";
    }
    else if (prefBox.value == 1)
    {
        allowTabs.checked = false;
        limitTabs.checked = false;
        userTabs.value = "15";
    }
    else
    {
        allowTabs.checked = true;
        limitTabs.checked = true;
        userTabs.value = prefBox.value;
    }
    limitTabs.disabled = ! allowTabs.checked;
    userTabs.disabled = ! (allowTabs.checked && limitTabs.checked);
}

function updatePMTabPref()
{
    var allowTabs = document.getElementById("czCreateTabsForPMs");
    var limitTabs = document.getElementById("czLimitPMTabs");
    var userTabs = document.getElementById("csPMTabLimitUser");
    var prefBox = document.getElementById("czPMTabLimit");
    
    if (allowTabs.checked)
    {
        if (limitTabs.checked)
            prefBox.value = userTabs.value;
        else
            prefBox.value = "0";
    }
    else
    {
        prefBox.value = "1";
    }
    limitTabs.disabled = ! allowTabs.checked;
    userTabs.disabled = ! (allowTabs.checked && limitTabs.checked);
    
    if (! userTabs.disabled)
        userTabs.focus();
}

/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* dialog stuff */
function onLoad()
{
    var args = window.arguments[0];

    window.onAcceptCallback = args.onOk;
    window.calendarEvent = args.calendarEvent;
    window.originalRecurrenceInfo = args.recurrenceInfo;

    loadDialog();

    updateDeck();

    opener.setCursor("auto");

    self.focus();
}

function onAccept()
{
    var event = window.calendarEvent;

    var recurrenceInfo = saveDialog();

    window.onAcceptCallback(recurrenceInfo);

    return true;
}

function onCancel()
{

}

function loadDialog()
{

}

function saveDialog()
{
    var deckNumber = Number(getElementValue("period-list"));
    dump("decknumber = " + deckNumber + "\n");
    
    var recurrenceInfo = createRecurrenceInfo();
    recurrenceInfo.initialize(window.calendarEvent);

    var recRule = new calRecurrenceRule();

    switch (deckNumber) {
    case 0:
        dump("daily recurrence\n");
        recRule.type = "DAILY";
        var ndays = Number(getElementValue("daily-days"));
        if (ndays == null)
            ndays = 1;
        recRule.interval = ndays;
        break;
    case 1: // weekly
        recRule.type = "WEEKLY";
        break;
    case 2: // monthly
        recRule.type = "MONTHLY";
        break;
    case 3: // annually
        recRule.type = "YEARLY";
        break;
    }

    switch(document.getElementById("recurrence-duration").selectedItem.value) {
    case "forever":
        dump("setting forever!\n");
        recRule.count = -1;
        break;
    case "ntimes":
        recRule.count = Math.max(1, getElementValue("ntimes-count"));
        break;
    case "until":
        //var recurEndDate = getElementValue("repeat-end-date-picker");
        //recRule.endDate = jsDateToDateTime(recurEndDate);
        break;
    default:
        dump("error finding duration\n");
        break;
    }

    recurrenceInfo.appendRecurrenceItem(recRule);

    return recurrenceInfo;
}



function updateDeck()
{
    document.getElementById("period-deck").selectedIndex = Number(getElementValue("period-list"));
}


/* utility functions */
function setEventProperty(event, propertyName, value)
{
    if (value)
        event.setProperty(propertyName, value);
}

function setElementValue(elementName, value, name)
{
    var element = document.getElementById(elementName);
    if (!element) {
        dump("unable to find " + elementName + "\n");
        return;
    }

    if (value === false) {
        element.removeAttribute(name ? name : "value");
    } else if (name) {
        //dump("element.setAttribute(" + name + ", " + value + ")\n");
        element.setAttribute(name, value);
    } else {
        //dump("element.value = " + value + "\n");
        element.value = value;
    }
}

function getElementValue(elementName, name)
{
    var element = document.getElementById(elementName);
    if (!element) {
        dump("unable to find " + elementName + "\n");
        return null;
    }

    if (name)
        return element[name];

    return element.value;
}

function enableElement(elementId)
{
    setElementValue(elementId, false, "disabled");
}

function disableElement(elementId)
{
    setElementValue(elementId, "true", "disabled");
}

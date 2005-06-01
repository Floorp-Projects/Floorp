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
 * The Original Code is mozilla calendar code.
 *
 * The Initial Developer of the Original Code is
 *  Michiel van Leeuwen <mvl@exedo.nl>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

function getListItem(aCalendar) {
    var calendarList = document.getElementById("list-calendars-listbox");
    for (item = calendarList.firstChild;
         item;
         item = item.nextSibling) {
        if (item.calendar == aCalendar ) {
            return item;
        }
    }
    return false;
}

// For observing changes to the list of calendars
var calCalendarManagerObserver = {
    QueryInterface: function(aIID) {
        if (!aIID.equals(Components.interfaces.calICalendarManagerObserver) &&
            !aIID.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    onCalendarRegistered: function(aCalendar) {
        setCalendarManagerUI();
    },

    onCalendarUnregistering: function(aCalendar) {
        var item = getListItem(aCalendar);
        if (item) {
            document.getElementById("list-calendars-listbox").removeChild(item);
        }
    },

    onCalendarDeleting: function(aCalendar) {
        setCalendarManagerUI();
    },

    onCalendarPrefSet: function(aCalendar, aName, aValue) {
        if (aName == 'color') {
            var item = getListItem(aCalendar);
            if (item) {
                var colorCell = item.childNodes[1];
                colorCell.style.background = aValue;
            }
        }
    },

    onCalendarPrefDeleting: function(aCalendar, aName) {
        setCalendarManagerUI();
    }
};

// For observing changes to the list of _active_ calendars
var calCompositeCalendarObserver = {
    QueryInterface: function(aIID) {
        if (!aIID.equals(Components.interfaces.calIObserver) &&
            !aIID.equals(Components.interfaces.calICompositeObserver) &&
            !aIID.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    // calICompositeObserver
    onCalendarAdded: function (aCalendar) {
        var item = getListItem(aCalendar);
        if (item) {
            var checkCell = item.firstChild;
            checkCell.setAttribute('checked', true);
        }
    },

    onCalendarRemoved: function (aCalendar) {
        var item = getListItem(aCalendar);
        if (item) {
            var checkCell = item.firstChild;
            checkCell.setAttribute('checked', false);
        }
    },

    onDefaultCalendarChanged: function (aNewDefaultCalendar) {
        // make the calendar bold in the tree
    },

    // calIObserver
    onStartBatch: function() { },
    onEndBatch: function() { },
    onLoad: function() { },
    onAddItem: function(aItem) { },
    onModifyItem: function(aNewItem, aOldItem) { },
    onDeleteItem: function(aDeletedItem) { },
    onAlarm: function(aAlarmItem) { },
    onError: function(aErrNo, aMessage) { }
};

function onCalendarCheckboxClick(event) {
    // This is a HACK.
    // It seems to be impossible to add onClick to a listCell.
    // Workaround that by checking the coordinates of the click.
    var checkElem = event.target.firstChild;
    if ((event.clientX < checkElem.boxObject.x + checkElem.boxObject.width) &&
        (event.clientX > checkElem.boxObject.x)) {

        // clicked the checkbox cell

        var cal = event.target.calendar;
        if (checkElem.getAttribute('checked') == "true") {
            getDisplayComposite().removeCalendar(cal.uri);
        } else {
            getDisplayComposite().addCalendar(cal);
        }

        event.preventDefault();
    }
}

function setCalendarManagerUI()
{
    var calendarList = document.getElementById("list-calendars-listbox");
    var child;
    while ((child = calendarList.lastChild) && (child.tagName == "listitem")) {
        calendarList.removeChild(child);
    }

    var calmgr = getCalendarManager();
    var calendars = calmgr.getCalendars({});
    for each (var calendar in calendars) {
        var listItem = document.createElement("listitem");

        var checkCell = document.createElement("listcell");
        checkCell.setAttribute("type", "checkbox");
        checkCell.setAttribute("checked", true);
        listItem.appendChild(checkCell);
        listItem.addEventListener("click", onCalendarCheckboxClick, true);

        var colorCell = document.createElement("listcell");
        try {
           var calColor = calmgr.getCalendarPref(calendar, 'color');
           colorCell.style.background = calColor;
        } catch(e) {}
        listItem.appendChild(colorCell);

        var nameCell = document.createElement("listcell");
        nameCell.setAttribute("label", calendar.name);
        listItem.appendChild(nameCell);
        listItem.calendar = calendar;
        calendarList.appendChild(listItem);

        if (calendarList.selectedIndex == -1)
            calendarList.selectedIndex = 0;    
    }
}

function initCalendarManager()
{
    getCalendarManager().addObserver(calCalendarManagerObserver);
    getDisplayComposite().addObserver(calCompositeCalendarObserver, 0);
    setCalendarManagerUI();
}

function getDefaultCalendar()
{
   var calendarList = document.getElementById("list-calendars-listbox");
   try {
       var selectedCalendar = calendarList.selectedItem.calendar;
       return selectedCalendar;
   } catch(e) {
       newCalendarDialog();
       selectedCalendar = calendarList.selectedItem.calendar;
       return selectedCalendar;
   }
   return false;
}

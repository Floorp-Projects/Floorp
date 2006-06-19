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
        if (item.calendar && item.calendar.uri.equals(aCalendar.uri)) {
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
        // Enable new calendars by default
        getDisplayComposite().addCalendar(aCalendar);
        setCalendarManagerUI();
        document.getElementById("new_command").removeAttribute("disabled");
        document.getElementById("new_todo_command").removeAttribute("disabled");

        if (aCalendar.canRefresh) {
            var remoteCommand = document.getElementById("reload_remote_calendars");
            remoteCommand.removeAttribute("disabled");
        }
    },

    onCalendarUnregistering: function(aCalendar) {
        var item = getListItem(aCalendar);
        if (item) {
            var listbox = document.getElementById("list-calendars-listbox");
            listbox.removeChild(item);
            // This is called before the calendar is actually deleted, so ==1 is correct
            if (getCalendarManager().getCalendars({}).length <= 1) {
                // If there are no calendars, you can't create events/tasks
                document.getElementById("new_command").setAttribute("disabled", true);
                document.getElementById("new_todo_command").setAttribute("disabled", true);
            }
        }

        if (aCalendar.canRefresh) {
            // We may have just removed our last remote cal.  Then we'd need to
            // disabled the reload-remote-calendars command
            var calendars = getCalendarManager().getCalendars({});
            function calCanRefresh(cal) {
                return (cal.canRefresh && !cal.uri.equals(aCalendar.uri));
            }
            if (!calendars.some(calCanRefresh)) {
                var remoteCommand = document.getElementById("reload_remote_calendars");
                remoteCommand.setAttribute("disabled", true);
            }
        }
    },

    onCalendarDeleting: function(aCalendar) {
        setCalendarManagerUI();
    },

    onCalendarPrefSet: function(aCalendar, aName, aValue) {
        var item = getListItem(aCalendar);
        if (aName == 'color') {
            if (item) {
                var colorCell = item.childNodes[1];
                colorCell.style.background = aValue;
            }
            updateStyleSheetForObject(aCalendar);
        } else if (aName == 'name') {
            if (item) {
                var nameCell = item.childNodes[2];
                nameCell.setAttribute("label", aCalendar.name);
            }
        }
    },

    onCalendarPrefDeleting: function(aCalendar, aName) {
        if (aName != 'color' && aName != 'name') {
            return;
        }
        setCalendarManagerUI();
        if (aName == 'color')
            updateStyleSheetForObject(aCalendar);
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
        var newSelectedEvents = new Array();
        var oldSelectedEvents = gCalendarWindow.EventSelection.selectedEvents;
        for (var i in oldSelectedEvents) {
            if (!oldSelectedEvents[i].calendar.uri.equals(aCalendar.uri)) 
                newSelectedEvents.push(oldSelectedEvents[i]);
        }
        gCalendarWindow.EventSelection.setArrayToSelection(newSelectedEvents);
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

    var composite = getDisplayComposite();
    var calmgr = getCalendarManager();
    var calendars = calmgr.getCalendars({});
    var hasRefreshableCal = false;
    for each (var calendar in calendars) {
        if (calendar.canRefresh) {
            hasRefreshableCal = true;
        }

        var listItem = document.createElement("listitem");

        var checkCell = document.createElement("listcell");
        checkCell.setAttribute("type", "checkbox");

        checkCell.setAttribute("checked", 
                               composite.getCalendar(calendar.uri) ? true : false);
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
    var remoteCommand = document.getElementById("reload_remote_calendars");
    if (!hasRefreshableCal) {
        remoteCommand.setAttribute("disabled", true);
    } else {
        remoteCommand.removeAttribute("disabled");
    }
}

function onCalendarListSelect() {
    var selectedCalendar = getSelectedCalendarOrNull();
    if (!selectedCalendar) {
        return;
    }
    getDisplayComposite().defaultCalendar = selectedCalendar;
}

function initCalendarManager()
{
    var calMgr = getCalendarManager();
    var composite = getDisplayComposite();
    if (calMgr.getCalendars({}).length == 0) {
        var homeCalendar = calMgr.createCalendar("storage", makeURL("moz-profile-calendar://"));
        calMgr.registerCalendar(homeCalendar);
        var name = srGetStrBundle("chrome://calendar/locale/calendar.properties")
                                 .GetStringFromName("homeCalendarName");
                                 
        homeCalendar.name = name;
        composite.addCalendar(homeCalendar);
    }
    calMgr.addObserver(calCalendarManagerObserver);
    composite.addObserver(calCompositeCalendarObserver);
    setCalendarManagerUI();
    initColors();
    var calendarList = document.getElementById("list-calendars-listbox");
    calendarList.addEventListener("select", onCalendarListSelect, true);
 
     // Set up a pref listener so the proper UI bits can be refreshed when prefs
     // are changed.
     var pb2 = prefService.getBranch("").QueryInterface(
         Components.interfaces.nsIPrefBranch2);
     pb2.addObserver("calendar.", calPrefObserver, false);
}

function finishCalendarManager() {
    // Remove the category color pref observer
    var pbi = prefService.getBranch("");
    pbi = pbi.QueryInterface(Components.interfaces.nsIPrefBranch2);
    pbi.removeObserver("calendar.category.color.", categoryPrefObserver);
    pbi.removeObserver("calendar.", calPrefObserver);

    var calendarList = document.getElementById("list-calendars-listbox");
    calendarList.removeEventListener("select", onCalendarListSelect, true);

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

function reloadCalendars()
{
    getDisplayComposite().refresh();
}

function getCalendarStyleSheet() {
    var calStyleSheet = null;
    for (var i = 0; i < document.styleSheets.length; i++) {
        if (document.styleSheets[i].href.match(
                /chrome.*\/skin.*\/calendar.css$/ )) {
        calStyleSheet = document.styleSheets[i];
        break;
        }
    }
    return calStyleSheet;
}

function initColors() {
    var calendars = getCalendarManager().getCalendars({});
    for (var j in calendars) 
        updateStyleSheetForObject(calendars[j]);

    var categoryPrefBranch = prefService.getBranch("calendar.category.color.");
    var prefArray = categoryPrefBranch.getChildList("", {});
    for (var i = 0; i < prefArray.length; i++) 
        updateStyleSheetForObject(prefArray[i]);
   
    // Setup css classes for category colors
    var catergoryPrefBranch = prefService.getBranch("");
    var pbi = catergoryPrefBranch.QueryInterface(
        Components.interfaces.nsIPrefBranch2);
    pbi.addObserver("calendar.category.color.", categoryPrefObserver, false);
    categoryPrefObserver.observe(null, null, "");
}

function updateStyleSheetForObject(object) {
    var calStyleSheet = getCalendarStyleSheet();
    var name;
    if (object.uri)
        name = object.uri.spec;
    else
        name = object.replace(' ','_');

    // Returns an equality selector for calendars (which have a uri), since an
    // event can only belong to one calendar.  For categories, however, returns
    // the ~= selector which matches anything in a space-separated list.
    function selectorForObject(name)
    {
        if (object.uri)
            return '*[item-calendar="' + name + '"]';
        return '*[item-category~="' + name + '"]';
    }
    
    function getRuleForObject(name)
    {
        for (var i = 0; i < calStyleSheet.cssRules.length; i++) {
            var rule = calStyleSheet.cssRules[i];
            if (rule.selectorText && (rule.selectorText == selectorForObject(name)))
                return rule;
        }
        return null;
    }
    
    var rule = getRuleForObject(name);
    if (!rule) {
        calStyleSheet.insertRule(selectorForObject(name) + ' { }',
                                 calStyleSheet.cssRules.length);
        rule = calStyleSheet.cssRules[calStyleSheet.cssRules.length-1];
    }

    var color;
    if (object.uri) {
        color = getCalendarManager().getCalendarPref(object, 'color');
        if (!color)
            color = "#A8C2E1";
        rule.style.backgroundColor = color;
        rule.style.color = getContrastingTextColor(color);
        return;
    }
    var categoryPrefBranch = prefService.getBranch("calendar.category.color.");
    try {
        color = categoryPrefBranch.getCharPref(object);
    }
    catch(ex) { return; }

    rule.style.border = color + " solid 2px";
}

var categoryPrefObserver =
{
   observe: function(aSubject, aTopic, aPrefName)
   {
       var name = aPrefName;
       // We only want the actual category name.  24 is the length of the 
       // leading 'calendar.category.color.' term
       name = name.substr(24, aPrefName.length - 24);
       updateStyleSheetForObject(name);
   }
}

var calPrefObserver =
{
   observe: function(aSubject, aTopic, aPrefName)
   {
       subject = aSubject.QueryInterface(Components.interfaces.nsIPrefBranch2);

       switch (aPrefName) {
            case "calendar.week.start":
                document.getElementById("lefthandcalendar").refreshDisplay(true);
                break;
            case "calendar.date.format":
                var currentView = document.getElementById("view-deck").selectedPanel;
                currentView.goToDay(currentView.selectedDay);
                refreshEventTree();
                toDoUnifinderRefresh();
                break;

            case "calendar.timezone.local":
                gDefaultTimezone = subject.getCharPref(aPrefName);

                var currentView = document.getElementById("view-deck").selectedPanel;
                currentView.goToDay(currentView.selectedDay);
                refreshEventTree();
                toDoUnifinderRefresh();
                break;

            default :
                break;
       }
   }
}

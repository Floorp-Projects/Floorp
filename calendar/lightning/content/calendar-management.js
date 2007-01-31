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
 * The Original Code is Lightning code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
 *   Dan Mosedale <dmose@mozilla.org>
 *   Joey Minta <jminta@gmail.com>
 *   Stefan Sitter <ssitter@googlemail.com>
 *   Stefan Schaefer <stephan.schaefer@sun.com>
 *   Michael Buettner <michael.buettner@sun.com>
 *   gekacheka@yahoo.com
 *   Daniel Boelzle <daniel.boelzle@sun.com>
 *   Robin Edrenius <robin.edrenius@gmail.com>
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

//
//  calendar-management.js
//

var gCachedStyleSheet;
function addCalendarToTree(aCalendar)
{
    if (!gCachedStyleSheet) {
        gCachedStyleSheet = getStyleSheet("chrome://calendar/content/calendar-view-bindings.css");
    }
    updateStyleSheetForObject(aCalendar, gCachedStyleSheet);
    updateLtnStyleSheet(aCalendar);

    var boxobj = document.getElementById("calendarTree").treeBoxObject;
    boxobj.rowCountChanged(getIndexForCalendar(aCalendar), 1);
}

function removeCalendarFromTree(aCalendar)
{
    var calTree = document.getElementById("calendarTree")
    var index = getIndexForCalendar(aCalendar);
    calTree.boxObject.rowCountChanged(index, -1);

    // Just select the new last row, if we removed the last listed calendar
    if (index == calTree.view.rowCount-1) {
        index--;
    }

    calTree.view.selection.select(index);
}

var ltnCalendarManagerObserver = {
    QueryInterface: function(aIID) {
        if (!aIID.equals(Components.interfaces.calICalendarManagerObserver) &&
            !aIID.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    onCalendarRegistered: function(aCalendar) {
        addCalendarToTree(aCalendar);
        getCompositeCalendar().addCalendar(aCalendar);
    },

    onCalendarUnregistering: function(aCalendar) {
        removeCalendarFromTree(aCalendar);
        getCompositeCalendar().removeCalendar(aCalendar.uri);
    },

    onCalendarDeleting: function(aCalendar) {
        removeCalendarFromTree(aCalendar); // XXX what else?
        getCompositeCalendar().removeCalendar(aCalendar.uri);
    },

    onCalendarPrefSet: function(aCalendar, aName, aValue) {
        if (!gCachedStyleSheet) {
            gCachedStyleSheet = getStyleSheet("chrome://calendar/content/calendar-view-bindings.css");
        }
        updateStyleSheetForObject(aCalendar, gCachedStyleSheet);
        updateTreeView(aCalendar);
    },

    onCalendarPrefDeleting: function(aCalendar, aName) {
    }
};

var ltnCompositeCalendarObserver = {
    QueryInterface: function(aIID) {
        // I almost wish that calICompositeObserver did not inherit from calIObserver,
        // and that the composite calendar maintined its own observer list
        if (!aIID.equals(Components.interfaces.calIObserver) &&
            !aIID.equals(Components.interfaces.calICompositeObserver) &&
            !aIID.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    // calICompositeObserver
    onCalendarAdded: function (aCalendar) {
        document.getElementById("calendarTree").boxObject.invalidate();
    },

    onCalendarRemoved: function (aCalendar) {
        document.getElementById("calendarTree").boxObject.invalidate();
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

var activeCompositeCalendar = null;
function getCompositeCalendar()
{
    if (activeCompositeCalendar == null) {
        activeCompositeCalendar =
            ltnCreateInstance("@mozilla.org/calendar/calendar;1?type=composite",
                              "calICompositeCalendar");
        activeCompositeCalendar.prefPrefix = "lightning-main";
        activeCompositeCalendar.addObserver(ltnCompositeCalendarObserver, 0);
    }

    return activeCompositeCalendar;
}

var activeCalendarManager;
function getCalendarManager()
{
    if (!activeCalendarManager) {
        activeCalendarManager = ltnGetService("@mozilla.org/calendar/manager;1",
                                              "calICalendarManager");
        activeCalendarManager.addObserver(ltnCalendarManagerObserver);
    }

    if (activeCalendarManager.getCalendars({}).length == 0) {
        var homeCalendar = activeCalendarManager.createCalendar("storage", 
                           makeURL("moz-profile-calendar://"));
        activeCalendarManager.registerCalendar(homeCalendar);

        homeCalendar.name = calGetString("calendar", "homeCalendarName");

        var composite = getCompositeCalendar();
        composite.addCalendar(homeCalendar);
        // XXX this doesn't make it selected, but you do add to it
    }

    return activeCalendarManager;
}

function getCalendars()
{
    try {
        return getCalendarManager().getCalendars({});
    } catch (e) {
        dump("Error getting calendars: " + e + "\n");
        return [];
    }
}

function ltnNewCalendar()
{
    openCalendarWizard(ltnSetTreeView);
}

function ltnRemoveCalendar(cal)
{
    // XXX in the future, we should ask the user if they want to delete the
    // XXX files associated with this calendar or not!
    getCalendarManager().unregisterCalendar(cal);
    getCalendarManager().deleteCalendar(cal);
}

function ltnEditCalendarProperties(cal)
{
    return openCalendarProperties(cal, function() { });
}

var ltnCalendarTreeView = {
    get rowCount()
    {
        try {
            return getCalendars().length;
        } catch (e) {
            return 0;
        }
    },

    getCellProperties: function (row, col, properties)
    {
        var cal = getCalendars()[row];
        if (col.id == "col-calendar-Checkbox") {
            // We key off this to set the images for the checkboxes
            if (getCompositeCalendar().getCalendar(cal.uri)) {
                properties.AppendElement(ltnGetAtom("checked"));
            }
            else {
                properties.AppendElement(ltnGetAtom("unchecked"));
            }
        } else if (col.id == "col-calendar-Color") {
            var color = getCalendarManager().getCalendarPref(cal, "color");
            properties.AppendElement(ltnGetAtom(getColorPropertyName(color)));
        }
    },

    cycleCell: function (row, col)
    {
        var cal = getCalendars()[row];
        if (getCompositeCalendar().getCalendar(cal.uri)) {
            // need to remove it
            getCompositeCalendar().removeCalendar(cal.uri);
        } else {
            // need to add it
            getCompositeCalendar().addCalendar(cal);
        }
        document.getElementById("calendarTree").boxObject.invalidateRow(row);
    },

    getCellValue: function (row, col)
    {
        if (col.id == "col-calendar-Checkbox") {
            var cal = getCalendars()[row];
            if (getCompositeCalendar().getCalendar(cal.uri))
                return "true";
            return "false";
        }

        dump ("*** Bad getCellValue (row: " + row + " col id: " + col.id + ")\n");
        return null;
    },

    setCellValue: function (row, col, value)
    {
        if (col.id == "col-calendar-Checkbox") {
            var cal = getCellValue()[row];
            if (value == "true") {
                getCompositeCalendar().addCalendar(cal);
            } else {
                getCompositeCalendar().removeCalendar(cal.uri);
            }
            return;
        }

        dump ("*** Bad setCellText (row: " + row + " col id: " + col.id + " val: " + value + ")\n");
    },

    getCellText: function (row, col)
    {
        if (col.id == "col-calendar-Checkbox" ||
            col.id == "col-calendar-Color") {
            return "";          // tooltip
        }

        if (col.id == "col-calendar-Calendar") {
            try {
                return getCalendars()[row].name;
            } catch (e) {
                return "<Unknown " + row + ">";
            }
        }

        dump ("*** Bad getCellText (row: " + row + " col id: " + col.id + ")\n");
        return null;
    },

    isEditable: function(row, col) { return false; },
    setTree: function(treebox) { this.treebox = treebox; },
    isContainer: function(row) { return false; },
    isSeparator: function(row) { return false; },
    isSorted: function(row) { return false; },
    getLevel: function(row) { return 0; },
    getImageSrc: function(row, col) { return null; },
    getRowProperties: function(row, props) { },
    getColumnProperties: function(colid, col, props) { },
    cycleHeader: function() { },
    onDoubleClick: function(event)
    {
        // We only care about left-clicks
        if (event.button != 0) 
            return;

        // Find the row clicked on
        var tree = document.getElementById("agenda-tree");
        var row = tree.treeBoxObject.getRowAt(event.clientX, event.clientY);

        // If we clicked on a calendar, edit it, otherwise create a new one
        var cal = getCalendars()[row];
        if (!cal) {
            ltnNewCalendar();
        } else {
            ltnEditCalendarProperties(cal);
        }
    }
};

function ltnSetTreeView()
{
    document.getElementById("calendarTree").view = ltnCalendarTreeView;

    // Ensure that a calendar is selected in calendar tree after startup.
    if (document.getElementById("calendarTree").currentIndex == -1) {
        var index = getIndexForCalendar(getCompositeCalendar().defaultCalendar);
        var indexToSelect = (index != - 1) ? index : 0;
        document.getElementById("calendarTree").view.selection.select(indexToSelect);
    }
}

function getIndexForCalendar(aCalendar) {
    // Special trick to compare interface pointers, since normal, ==
    // comparison can fail due to javascript wrapping.
    var sip = Components.classes["@mozilla.org/supports-interface-pointer;1"]
                        .createInstance(Components.interfaces.nsISupportsInterfacePointer);
    sip.data = aCalendar;
    sip.dataIID = Components.interfaces.calICalendar;
    return getCalendars().indexOf(sip.data);
}

function getColorPropertyName(color) {
    // strip hash (#) from color string
    return "color-" + (color ? color.substr(1) : "default");
}

var gLtnStyleSheet;

function updateLtnStyleSheet(aCalendar) {
    // check if rule already exists in style sheet
    if (!gLtnStyleSheet) {
        gLtnStyleSheet = getStyleSheet("chrome://lightning/skin/lightning.css");
    }
    var selectorPrefix = "treechildren::-moz-tree-cell";
    var color = getCalendarManager().getCalendarPref(aCalendar, "color");
    if (!color) {
        return;
    }
    var propertyName = getColorPropertyName(color);
    var selectorText = selectorPrefix + propertyName;
    for (var i = 0; i < gLtnStyleSheet.cssRules.length; i++) {
        if (gLtnStyleSheet.cssRules[i].selectorText == selectorText) {
            return;
        }
    }

    // add rule to style sheet
    var ruleString = selectorPrefix + "(" + propertyName + ") { }";
    gLtnStyleSheet.insertRule(ruleString, gLtnStyleSheet.cssRules.length);
    var rule = gLtnStyleSheet.cssRules[gLtnStyleSheet.cssRules.length-1];
    rule.style.backgroundColor = color;
    rule.style.margin = "1px";
}

function updateColorCell(aCalendar) {
    var row = getIndexForCalendar(aCalendar);
    var treeBoxObject = document.getElementById("calendarTree").treeBoxObject;
    var column = treeBoxObject.columns["col-calendar-Color"];
    treeBoxObject.invalidateCell(row, column);
}

function updateTreeView(aCalendar) {
    updateLtnStyleSheet(aCalendar);
    updateColorCell(aCalendar);
}

window.addEventListener("load", ltnSetTreeView, false);
// Wire up the calendar observers.
window.addEventListener("load", getCalendarManager, false);

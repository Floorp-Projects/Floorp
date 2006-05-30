//
//  calendar-management.js
//

var calendarPrefStyleSheet = null;
for each(sheet in document.styleSheets) {
    if (sheet.href == "chrome://lightning/skin/lightning.css") {
        calendarPrefStyleSheet = sheet;
        break;
    }
}
if (!calendarPrefStyleSheet)
    Components.utils.reportError("Couldn't find the lightning style sheet.")

function updateStyleSheetForCalendar(aCalendar)
{
    var spec = aCalendar.uri.spec;
    var cpss = calendarPrefStyleSheet;
    function selectorForCalendar(spec)
    {
        return '*[item-calendar="' + spec + '"]';
    }
    
    function getRuleForCalendar(spec)
    {
        for (var i = 0; i < cpss.cssRules.length; i++) {
            var rule = cpss.cssRules[i];
            if (rule.selectorText == selectorForCalendar(spec))
                return rule;
        }
        return null;
    }
    
    var rule = getRuleForCalendar(spec);
    if (!rule) {
        cpss.insertRule(selectorForCalendar(spec) + ' { }', 0);
        rule = cpss.cssRules[0];
    }
    
    var color = getCalendarManager().getCalendarPref(aCalendar, 'color');
    // This color looks nice with the gripbars, etc.
    if (!color)
        color = "#A8C2E1";
    
    rule.style.backgroundColor = color;
    rule.style.color = getContrastingTextColor(color);
}

function addCalendarToTree(aCalendar)
{
    var boxobj = document.getElementById("calendarTree").treeBoxObject;

    // Special trick to compare interface pointers, since normal, ==
    // comparison can fail due to javascript wrapping.
    var sip = Components.classes["@mozilla.org/supports-interface-pointer;1"]
                         .createInstance(Components.interfaces.nsISupportsInterfacePointer);
    sip.data = aCalendar;
    sip.dataIID = Components.interfaces.calICalendar;

    boxobj.rowCountChanged(getCalendars().indexOf(sip.data), 1);
    
    updateStyleSheetForCalendar(aCalendar);
}

function removeCalendarFromTree(aCalendar)
{
    var boxobj = document.getElementById("calendarTree").treeBoxObject;

    // Special trick to compare interface pointers, since normal, ==
    // comparison can fail due to javascript wrapping.
    var sip = Components.classes["@mozilla.org/supports-interface-pointer;1"]
                         .createInstance(Components.interfaces.nsISupportsInterfacePointer);
    sip.data = aCalendar;
    sip.dataIID = Components.interfaces.calICalendar;
    boxobj.rowCountChanged(getCalendars().indexOf(sip.data), -1);
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
        updateStyleSheetForCalendar(aCalendar);
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

var ltnCalendarViewController = {
    QueryInterface: function(aIID) {
        if (!aIID.equals(Components.interfaces.calICalendarViewController) &&
            !aIID.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    createNewEvent: function (aCalendar, aStartTime, aEndTime) {
        // XXX If we're adding an item from the view, let's make sure that
        // XXX the calendar in question is visible!
        
        if (!aCalendar) {
            aCalendar = ltnSelectedCalendar();
        }

        // if we're given both times, skip the dialog
        if (aStartTime && aEndTime && !aStartTime.isDate && !aEndTime.isDate) {
            var event = createEvent();
            event.startDate = aStartTime;
            event.endDate = aEndTime;
            var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                .getService(Components.interfaces.nsIStringBundleService);
            var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
            event.title = props.GetStringFromName("newEvent");
            setDefaultAlarmValues(event);
            doTransaction('add', event, aCalendar, null, null);
        } else if (aStartTime && aStartTime.isDate) {
            var event = createEvent();
            event.startDate = aStartTime;
            setDefaultAlarmValues(event);
            doTransaction('add', event, aCalendar, null, null);
        } else {
            // default pop up the dialog
            var date = document.getElementById("calendar-view-box").selectedPanel.selectedDay.clone();
            date.isDate = false;
            createEventWithDialog(aCalendar, date, date);
        }
    },

    modifyOccurrence: function (aOccurrence, aNewStartTime, aNewEndTime) {
        // if we can modify this thing directly (e.g. just the time changed),
        // then do so; otherwise pop up the dialog
        var itemToEdit = getOccurrenceOrParent(aOccurrence);
        if (!itemToEdit) {
            return;
        }
        if (aNewStartTime && aNewEndTime && !aNewStartTime.isDate && !aNewEndTime.isDate) {
        
            var instance = itemToEdit.clone();

            // if we're about to modify the parentItem, we need to account
            // for the possibility that the item passed as argument was
            // some other ocurrence, but the user said she would like to
            // modify all ocurrences instead.
            if (instance.parentItem == instance) {

                var startDiff = instance.startDate.subtractDate(aOccurrence.startDate);
                aNewStartTime.addDuration(startDiff);
                var endDiff = instance.endDate.subtractDate(aOccurrence.endDate);
                aNewEndTime.addDuration(endDiff);
            }

            instance.startDate = aNewStartTime;
            instance.endDate = aNewEndTime;

            doTransaction('modify', instance, instance.calendar, itemToEdit, null);
        } else {
            modifyEventWithDialog(itemToEdit);
        }
    },

    deleteOccurrence: function (aOccurrence) {
        var itemToDelete = getOccurrenceOrParent(aOccurrence);
        if (!itemToDelete) {
            return;
        }
        if (itemToDelete.parentItem != itemToDelete) {
            var event = itemToDelete.parentItem.clone();
            event.recurrenceInfo.removeOccurrenceAt(itemToDelete.recurrenceId);
            doTransaction('modify', event, event.calendar, itemToDelete.parentItem, null);
        } else {
            doTransaction('delete', itemToDelete, itemToDelete.calendar, null, null);
        }
    }
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

        var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                            .getService(
                             Components.interfaces.nsIStringBundleService);
        var props = sbs.createBundle(
                    "chrome://calendar/locale/calendar.properties");
        homeCalendar.name = props.GetStringFromName("homeCalendarName");

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
        if (col.id == "col-calendar-Checkbox") {
            var cal = getCalendars()[row];
            // We key off this to set the images for the checkboxes
            if (getCompositeCalendar().getCalendar(cal.uri)) {
                properties.AppendElement(ltnGetAtom("checked"));
            }
            else {
                properties.AppendElement(ltnGetAtom("unchecked"));
            }
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
        if (col.id == "col-calendar-Checkbox") {
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
        document.getElementById("calendarTree").view.selection.select(0);
    }
}

window.addEventListener("load", ltnSetTreeView, false);
// Wire up the calendar observers.
window.addEventListener("load", getCalendarManager, false);

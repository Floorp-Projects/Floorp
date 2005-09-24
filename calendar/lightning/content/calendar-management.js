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
    if (!color)
        color = "black";
    
    rule.style.color = color;
}

function addCalendarToTree(aCalendar)
{
    var boxobj = document.getElementById("calendarTree").treeBoxObject;
    boxobj.rowCountChanged(getCalendars().indexOf(aCalendar), 1);
    updateStyleSheetForCalendar(aCalendar);
}

function removeCalendarFromTree(aCalendar)
{
    var boxobj = document.getElementById("calendarTree").treeBoxObject;
    boxobj.rowCountChanged(getCalendars().indexOf(aCalendar), -1);
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
    },

    onCalendarUnregistering: function(aCalendar) {
        removeCalendarFromTree(aCalendar);
    },

    onCalendarDeleting: function(aCalendar) {
        removeCalendarFromTree(aCalendar); // XXX what else?
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
    onAlarm: function(aAlarmItem) { },
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
        
        // if we're given both times, skip the dialog
        if (aStartTime && aEndTime && !aStartTime.isDate && !aEndTime.isDate) {
            var event = createEvent();
            event.startDate = aStartTime;
            event.endDate = aEndTime;
            event.title = "New Event";
            aCalendar.addItem(event, null);
        } else if (aStartTime && aStartTime.isDate) {
            var event = createEvent();
            event.startDate = aStartTime;
            aCalendar.addItem(event, null);
        } else {
            // default pop up the dialog
            createEventWithDialog(aCalendar, aStartTime, aEndTime);
        }
    },

    modifyOccurrence: function (aOccurrence, aNewStartTime, aNewEndTime) {
        // if we can modify this thing directly (e.g. just the time changed),
        // then do so; otherwise pop up the dialog
        if (aNewStartTime && aNewEndTime && !aNewStartTime.isDate && !aNewEndTime.isDate) {
            var instance = aOccurrence.clone();

            instance.startDate = aNewStartTime;
            instance.endDate = aNewEndTime;

            instance.calendar.modifyItem(instance, aOccurrence, null);
        } else {
            modifyEventWithDialog(aOccurrence);
        }
    },

    deleteOccurrence: function (aOccurrence) {
        if (aOccurrence.parentItem != aOccurrence) {
            var event = aOccurrence.parentItem.clone();
            event.recurrenceInfo.removeOccurrenceAt(aOccurrence.recurrenceId);
            event.calendar.modifyItem(event, aOccurrence, null);
        } else {
            aOccurrence.calendar.deleteItem(aOccurrence, null);
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
        var homeCalendar = activeCalendarManager.createCalendar("storage", makeURL("moz-profile-calendar://"));
        activeCalendarManager.registerCalendar(homeCalendar);
        homeCalendar.name = "Home";

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
            if (getCompositeCalendar().getCalendar(cal.uri)) {
                properties.AppendElement(ltnGetAtom("checked"));
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
    cycleHeader: function() { }
};

function ltnSetTreeView()
{
    document.getElementById("calendarTree").view = ltnCalendarTreeView;
}

window.addEventListener("load", ltnSetTreeView, false);
// Wire up the calendar observers.
window.addEventListener("load", getCalendarManager, false);

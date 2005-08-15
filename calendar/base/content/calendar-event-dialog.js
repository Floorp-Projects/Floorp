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
    window.calendarItem = args.calendarEvent;
    window.mode = args.mode;
    window.recurrenceInfo = null;

    /* add calendars to the calendar menulist */
    var calendarList = document.getElementById("item-calendar");
    var calendars = getCalendarManager().getCalendars({});
    for (i in calendars) {
        var calendar = calendars[i];
        var menuitem = calendarList.appendItem(calendar.name, i);
        menuitem.calendar = calendar;
    }

    loadDialog(window.calendarItem);

    // figure out what the title of the dialog should be and set it
    updateTitle();

    // hide rows based on if this is an event or todo
    updateStyle();

    // update the accept button
    updateAccept();

    // update datetime pickers
    updateDuedate();

    // update datetime pickers
    updateAllDay();

    // update recurrence button
    updateRecurrence();

    // update alarm checkbox/label/settings button
    updateAlarm();

    // update our size!
    window.sizeToContent();

    document.getElementById("item-title").focus();

    opener.setCursor("auto");

    self.focus();
}

function onAccept()
{
    // if this event isn't mutable, we need to clone it like a sheep
    var originalItem = window.calendarItem;
    var item = (originalItem.isMutable) ? originalItem : originalItem.clone();

    saveDialog(item);

    var calendar = document.getElementById("item-calendar").selectedItem.calendar;

    window.onAcceptCallback(item, calendar, originalItem);

    return true;
}

function onCancel()
{

}

function loadDialog(item)
{
    var kDefaultTimezone = calendarDefaultTimezone();

    setElementValue("item-title",       item.title);
    setElementValue("item-location",    item.getProperty("LOCATION"));
    setElementValue("item-url",         item.getProperty("URL"));
    setElementValue("item-description", item.getProperty("DESCRIPTION"));

    /* event specific properties */
    if (isEvent(item)) {
        var startDate = item.startDate.getInTimezone(kDefaultTimezone);
        var endDate = item.endDate.getInTimezone(kDefaultTimezone);
        if (startDate.isDate) {
            setElementValue("event-all-day", true, "checked");
            endDate.day -= 1;
            endDate.normalize();
        }
        setElementValue("event-starttime",   startDate.jsDate);
        setElementValue("event-endtime",     endDate.jsDate);
    }

    /* todo specific properties */
    if (isToDo(item)) {
        var hasDueDate = (item.dueDate != null);
        setElementValue("todo-has-duedate", hasDueDate, "checked");
        if (hasDueDate)
            setElementValue("todo-duedate", item.dueDate.jsDate);
        setElementValue("todo-completed", item.isCompleted, "checked");
    }

    /* attendence */
    var attendeeString = "";
    for each (var attendee in item.getAttendees({})) {
        if (attendeeString != "")
            attendeeString += ",";
        attendeeString += attendee.id.split("mailto:")[1];
    }
    setElementValue("item-attendees", attendeeString);

    /* item default calendar */
    if (item.calendar) {
        var calendarList = document.getElementById("item-calendar");
        var calendars = getCalendarManager().getCalendars({});
        for (i in calendars) {
            if (item.calendar.uri.equals(calendars[i].uri))
                calendarList.selectedIndex = i;
        }
    }

    /* recurrence */
    /* if the item is a proxy occurrence/instance, a few things aren't valid:
     * - Setting recurrence on the item
     * - changing the calendar
     */
    if (item.parentItem != item) {
        setElementValue("item-recurrence", "true", "disabled");
        setElementValue("set-recurrence", "true", "disabled");
        setElementValue("item-calendar", "true", "disabled");
    } else if (item.recurrenceInfo)
        setElementValue("item-recurrence", "true", "checked");

    /* alarms */
    if (item.alarmTime) {
        var alarmLength = item.getProperty("alarmLength");
        if (alarmLength != null) {
            setElementValue("alarm-length-field", alarmLength);
            setElementValue("alarm-length-units", event.getProperty("alarmUnits"));
            setElementValue("alarm-trigger-relation", item.getProperty("alarmRelated"));
        }
        setElementValue("item-alarm", "custom");
    }
}

function saveDialog(item)
{
    setItemProperty(item, "title",       getElementValue("item-title"));
    setItemProperty(item, "LOCATION",    getElementValue("item-location"));
    setItemProperty(item, "URL",         getElementValue("item-url"));
    setItemProperty(item, "DESCRIPTION", getElementValue("item-description"));

    if (isEvent(item)) {
        var startDate = jsDateToDateTime(getElementValue("event-starttime"));
        var endDate = jsDateToDateTime(getElementValue("event-endtime"));

        var isAllDay = getElementValue("event-all-day", "checked");
        if (isAllDay) {
            startDate.isDate = true;
            startDate.normalize();

            endDate.isDate = true;
            endDate.day += 1;
            endDate.normalize();
        }
        setItemProperty(item, "startDate",   startDate);
        setItemProperty(item, "endDate",     endDate);
    }

    if (isToDo(item)) {
        var dueDate = getElementValue("todo-has-duedate", "checked") ? 
            jsDateToDateTime(getElementValue("todo-duedate")) : null;
        setItemProperty(item, "dueDate",     dueDate);
        setItemProperty(item, "isCompleted", getElementValue("todo-completed", "checked"));
    }

    /* attendence */
    item.removeAllAttendees();
    var attendees = getElementValue("item-attendees");
    if (attendees != "") {
        for each (var addr in attendees.split(",")) {
            if (addr != "") {
                var attendee = createAttendee();
                attendee.id = "mailto:" + addr;
                item.addAttendee(attendee);
            }
        }
    }

    /* recurrence */
    if (getElementValue("item-recurrence", "checked")) {
        if (window.recurrenceInfo) {
            item.recurrenceInfo = window.recurrenceInfo;
        }
    } else {
        item.recurrenceInfo = null;
    }

    /* alarms */
    var hasAlarm = (getElementValue("item-alarm") != "none");
    if (!hasAlarm) {
        item.deleteProperty("alarmLength");
        item.deleteProperty("alarmUnits");
        item.deleteProperty("alarmRelated");
        item.alarmTime = null;
    } else {
        var alarmLength = getElementValue("alarm-length-field");
        var alarmUnits = document.getElementById("alarm-length-units").selectedItem.value;
        var alarmRelated = document.getElementById("alarm-trigger-relation").selectedItem.value;

        setItemProperty(item, "alarmLength",  alarmLength);
        setItemProperty(item, "alarmUnits",   alarmUnits);
        setItemProperty(item, "alarmRelated", alarmRelated);

        var alarmTime = null;

        if (alarmRelated == "START") {
            alarmTime = item.startDate.clone();
        } else if (alarmRelated == "END") {
            alarmTime = item.endDate.clone();
        }

        switch (alarmUnits) {
        case "minutes":
            alarmTime.minute -= alarmLength;
            break;
        case "hours":
            alarmTime.hour -= alarmLength;
            break;
        case "days":
            alarmTime.day -= alarmLength;
            break;
        }

        alarmTime.normalize();

        setItemProperty(item, "alarmTime", alarmTime);
    }

    dump(item.icalString + "\n");
}

function updateTitle()
{
    // XXX make this use string bundles
    var isNew = window.calendarItem.isMutable;
    if (isEvent(window.calendarItem)) {
        if (isNew)
            document.title = "New Event";
        else
            document.title = "Edit Event";
    } else if (isToDo(window.calendarItem)) {
        if (isNew)
            document.title = "New Task";
        else
            document.title = "Edit Task";        
    }
}

function updateStyle()
{
    const kDialogStylesheet = "chrome://calendar/content/calendar-event-dialog.css";

    for each(var stylesheet in document.styleSheets) {
        if (stylesheet.href == kDialogStylesheet) {
            if (isEvent(window.calendarItem))
                stylesheet.insertRule(".todo-only { display: none; }", 0);
            else if (isToDo(window.calendarItem))
                stylesheet.insertRule(".event-only { display: none; }", 0);
            return;
        }
    }
}

function updateAccept()
{
    var acceptButton = document.getElementById("calendar-event-dialog").getButton("accept");

    var title = getElementValue("item-title");
    if (title.length == 0)
        acceptButton.setAttribute("disabled", "true");
    else if (acceptButton.getAttribute("disabled"))
        acceptButton.removeAttribute("disabled");

    // don't allow for end dates to be before start dates
    var startDate = jsDateToDateTime(getElementValue("event-starttime"));
    var endDate = jsDateToDateTime(getElementValue("event-endtime"));
    if (endDate.compare(startDate) == -1) {
        acceptButton.setAttribute("disabled", "true");
    } else if (acceptButton.getAttribute("disabled")) {
        acceptButton.removeAttribute("disabled");
    }

    return;
}

function updateDuedate()
{
    if (!isToDo(window.calendarItem))
        return;

    // force something to get set if there was nothing there before
    setElementValue("todo-duedate", getElementValue("todo-duedate"));

    setElementValue("todo-duedate", !getElementValue("todo-has-duedate", "checked"), "disabled");
}


function updateAllDay()
{
    if (!isEvent(window.calendarItem))
        return;

    var allDay = getElementValue("event-all-day", "checked");
    setElementValue("event-starttime", allDay, "timepickerdisabled");
    setElementValue("event-endtime", allDay, "timepickerdisabled");
}


function updateRecurrence()
{
    var recur = getElementValue("item-recurrence", "checked");
    if (recur) {
        setElementValue("set-recurrence", false, "disabled");
    } else {
        setElementValue("set-recurrence", "true", "disabled");
    }
}

var prevAlarmItem = null;
function setAlarmFields(alarmItem)
{
    var alarmLength = alarmItem.getAttribute("length");
    if (alarmLength != "") {
        var alarmUnits = alarmItem.getAttribute("unit");
        var alarmRelation = alarmItem.getAttribute("relation");
        setElementValue("alarm-length-field", alarmLength);
        setElementValue("alarm-length-units", alarmUnits);
        setElementValue("alarm-trigger-relation", alarmRelation);
    }
}
function updateAlarm()
{
    var alarmMenu = document.getElementById("item-alarm");
    var alarmItem = alarmMenu.selectedItem;

    var alarmItemValue = alarmItem.getAttribute("value");
    switch (alarmItemValue) {
    case "custom":
        /* restore old values if they're around */
        setAlarmFields(alarmItem);
        
        document.getElementById("alarm-details").style.visibility = "visible";
        break;
    default:
        document.getElementById("alarm-details").style.visibility = "hidden";

        var customItem = document.getElementById("alarm-custom-menuitem");
        if (prevAlarmItem == customItem) {
            customItem.setAttribute("length", getElementValue("alarm-length-field"));
            customItem.setAttribute("unit", getElementValue("alarm-length-units"));
            customItem.setAttribute("relation", getElementValue("alarm-trigger-relation"));
        }
        setAlarmFields(alarmItem);

        break;
    }

    prevAlarmItem = alarmItem;
}

function editRecurrence()
{
    var args = new Object();
    args.calendarEvent = window.calendarItem;
    args.recurrenceInfo = window.recurrenceInfo || args.calendarEvent.recurrenceInfo;

    var savedWindow = window;
    args.onOk = function(recurrenceInfo) {
        savedWindow.recurrenceInfo = recurrenceInfo;
    };

    // wait cursor will revert to auto in eventDialog.js loadCalendarEventDialog
    window.setCursor("wait");

    // open the dialog modally
    openDialog("chrome://calendar/content/calendar-recurrence-dialog.xul", "_blank", "chrome,titlebar,modal", args);
}



/* utility functions */
function setItemProperty(item, propertyName, value)
{
    switch(propertyName) {
    case "startDate":
        if (value.isDate && !item.startDate.isDate ||
            !value.isDate && item.startDate.isDate ||
            value.compare(item.startDate) != 0)
            item.startDate = value;
        break;
    case "endDate":
        if (value.isDate && !item.endDate.isDate ||
            !value.isDate && item.endDate.isDate ||
            value.compare(item.endDate) != 0)
            item.endDate = value;
        break;


    case "dueDate":
        if (value == item.dueDate)
            break;
        if ((value && !item.dueDate) ||
            (!value && item.dueDate) ||
            (value.compare(item.dueDate) != 0))
            item.dueDate = value;
        break;
    case "isCompleted":
        if (value != item.isCompleted)
            item.isCompleted = value;
        break;

    case "title":
        if (value != item.title)
            item.title = value;
        break;

    case "alarmTime":
        if ((value && !item.alarmTime) ||
            (!value && item.alarmTime) ||
            (value.compare(item.alarmTime) != 0))
            item.alarmTime = value;
        break;
    default:
        if (!value || value == "")
            item.deleteProperty(propertyName);
        else if (item.getProperty(propertyName) != value)
            item.setProperty(propertyName, value);
        break;
    }
}

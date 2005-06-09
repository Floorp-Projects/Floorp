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
    window.mode = args.mode;
    window.recurrenceInfo = null;

    /* add calendars to the calendar menulist */
    var calendarList = document.getElementById("event-calendar");
    var calendars = getCalendarManager().getCalendars({});
    for (i in calendars) {
        var calendar = calendars[i];
        var menuitem = calendarList.appendItem(calendar.name, i);
        menuitem.calendar = calendar;
    }

    loadDialog();

    // update datetime pickers
    updateAllDay();

    // update recurrence button
    updateRecurrence();

    // update alarm checkbox/label/settings button
    updateAlarm();

    document.getElementById("event-title").focus();

    opener.setCursor("auto");

    self.focus();
}

function onAccept()
{
    // if this event isn't mutable, we need to clone it like a sheep
    var originalEvent = window.calendarEvent;
    var event = null;

    event = (originalEvent.isMutable) ? originalEvent : originalEvent.clone();

    saveDialog(event);

    var calendar = document.getElementById("event-calendar").selectedItem.calendar;

    window.onAcceptCallback(event, calendar, originalEvent);

    return true;
}

function onCancel()
{

}

function loadDialog()
{
    const kDefaultTimezone = calendarDefaultTimezone();

    var event = window.calendarEvent;

    setElementValue("event-title",       event.title);
    setElementValue("event-all-day",     event.isAllDay, "checked");

    setElementValue("event-location",    event.getProperty("LOCATION"));
    setElementValue("event-url",         event.getProperty("URL"));
    setElementValue("event-description", event.getProperty("DESCRIPTION"));

    /* all day */
    var startDate = event.startDate.getInTimezone(kDefaultTimezone);
    var endDate = event.endDate.getInTimezone(kDefaultTimezone);
    if (startDate.isDate) {
        endDate.day -= 1;
        endDate.normalize();
    }
    setElementValue("event-starttime",   startDate.jsDate);
    setElementValue("event-endtime",     endDate.jsDate);

    /* attendence */
    var attendeeString = "";
    for each (var attendee in event.getAttendees({})) {
        if (attendeeString != "")
            attendeeString += ",";
        attendeeString += attendee.id.split("mailto:")[1];
    }
    setElementValue("event-attendees", attendeeString);

    /* event default calendar */
    if (event.calendar) {
        var calendarList = document.getElementById("event-calendar");
        var calendars = getCalendarManager().getCalendars({});
        for (i in calendars) {
            if (event.calendar.uri.equals(calendars[i].uri))
                calendarList.selectedIndex = i;
        }
    }

    /* recurrence */
    if (event.recurrenceInfo) {
        setElementValue("event-recurrence", "true", "checked");
    }

    /* alarms */
    if (event.hasAlarm) {
        var alarmLength = event.getProperty("alarmLength");
        if (alarmLength != null) {
            setElementValue("alarm-length-field", alarmLength);
            setElementValue("alarm-length-units", event.getProperty("alarmUnits"));
            setElementValue("alarm-trigger-relation", event.getProperty("alarmRelated"));
        }
        setElementValue("event-alarm", "custom");
    }
}

function saveDialog(event)
{
    event.title = getElementValue("event-title");
    event.isAllDay = getElementValue("event-all-day", "checked");
    event.startDate = jsDateToDateTime(getElementValue("event-starttime"));
    event.endDate = jsDateToDateTime(getElementValue("event-endtime"));

    setEventProperty(event, "LOCATION",    getElementValue("event-location"));
    setEventProperty(event, "URL",         getElementValue("event-url"));
    setEventProperty(event, "DESCRIPTION", getElementValue("event-description"));

    /*  all day */
    if (event.isAllDay) {
        event.startDate.isDate = true;
        event.endDate.isDate = true;
        event.endDate.day += 1;
        event.endDate.normalize();
    }

    /* attendence */
    event.removeAllAttendees();
    var attendees = getElementValue("event-attendees");
    if (attendees != "") {
        for each (var addr in attendees.split(",")) {
            var attendee = createAttendee();
            attendee.id = "mailto:" + addr;
            event.addAttendee(attendee);
        }
    }

    /* recurrence */
    if (getElementValue("event-recurrence", "checked")) {
        if (window.recurrenceInfo) {
            event.recurrenceInfo = window.recurrenceInfo;
        }
    } else {
        event.recurrenceInfo = null;
    }

    /* alarms */
    event.hasAlarm = (getElementValue("event-alarm") != "none");
    if (!event.hasAlarm) {
        event.deleteProperty("alarmLength");
        event.deleteProperty("alarmUnits");
        event.deleteProperty("alarmRelated");
        event.alarmTime = null;
    } else {
        var alarmLength = getElementValue("alarm-length-field");
        var alarmUnits = document.getElementById("alarm-length-units").selectedItem.value;
        var alarmRelated = document.getElementById("alarm-trigger-relation").selectedItem.value;

        event.setProperty("alarmLength",  alarmLength);
        event.setProperty("alarmUnits",   alarmUnits);
        event.setProperty("alarmRelated", alarmRelated);

        var alarmTime = null;

        if (alarmRelated == "START") {
            alarmTime = event.startDate.clone();
        } else if (alarmRelated == "END") {
            alarmTime = event.endDate.clone();
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

        event.alarmTime = alarmTime;
    }
}


function updateAllDay()
{
    var allDay = getElementValue("event-all-day", "checked");
    setElementValue("event-starttime", allDay, "timepickerdisabled");
    setElementValue("event-endtime", allDay, "timepickerdisabled");
}


function updateRecurrence()
{
    var recur = getElementValue("event-recurrence", "checked");
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
    var alarmMenu = document.getElementById("event-alarm");
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
    args.calendarEvent = window.calendarEvent;
    args.recurrenceInfo = (window.recurrenceInfo) ? window.recurrenceInfo : window.calendarEvent.recurrenceInfo;

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
function setEventProperty(event, propertyName, value)
{
    if (!value || value == "")
        event.deleteProperty(propertyName);
    else
        event.setProperty(propertyName, value);
}

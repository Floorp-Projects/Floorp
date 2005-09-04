/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley <garths@oeone.com>
 *                 Mike Potter <mikep@oeone.com>
 *                 Colin Phillips <colinp@oeone.com> 
 *                 Chris Charabaruk <coldacid@meldstar.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Mostafa Hosseini <mostafah@oeone.com>
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Stelian Pop <stelian.pop@fr.alcove.com>
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



/***** calendar/eventDialog.js
* AUTHOR
*   Garth Smedley
* REQUIRED INCLUDES 
*   <script type="application/x-javascript" src="chrome://calendar/content/dateUtils.js"/>
*   <script type="application/x-javascript" src="chrome://calendar/content/applicationUtil.js"/>
* NOTES
*   Code for the calendar's new/edit event dialog.
*
*  Invoke this dialog to create a new event as follows:

   var args = new Object();
   args.mode = "new";               // "new" or "edit"
   args.onOk = <function>;          // funtion to call when OK is clicked
   args.calendarEvent = calendarEvent;    // newly creatd calendar event to be editted
   
   openDialog("chrome://calendar/content/eventDialog.xul", "caEditEvent", "chrome,titlebar,modal", args );
*
*  Invoke this dialog to edit an existing event as follows:
*
   var args = new Object();
   args.mode = "edit";                    // "new" or "edit"
   args.onOk = <function>;                // funtion to call when OK is clicked
   args.calendarEvent = calendarEvent;    // javascript object containin the event to be editted

* When the user clicks OK the onOk function will be called with a calendar event object.
*
*  
* IMPLEMENTATION NOTES 
**********
*/


/*-----------------------------------------------------------------
*   W I N D O W      V A R I A B L E S
*/

var gDebugEnabled=true;

var gOnOkFunction;   // function to be called when user clicks OK
var gDurationMSec;   // current duration, for updating end date when start date is changed

const DEFAULT_ALARM_LENGTH = 15; //default number of time units, an alarm goes off before an event

/*-----------------------------------------------------------------
*   W I N D O W      F U N C T I O N S
*/

/**
*   Called when the dialog is loaded.
*/
function loadCalendarEventDialog()
{
    const kDefaultTimezone = calendarDefaultTimezone();

    // Get arguments, see description at top of file
    var args = window.arguments[0];

    gOnOkFunction = args.onOk;

    var event = args.calendarEvent;

    // Set up dialog as event or todo
    var componentType;
    if (isEvent(event)) {
        processComponentType("event");
        componentType = "event";
    } else if (isToDo(event)) {
        processComponentType("todo");
        componentType = "todo";
    } else {
        dump("loadCalendarEventDialog: ERROR! Got a bogus event! Not event or todo!\n");
        // close the dialog before we screw anything else up
        return false;
    }

    // fill in fields from the event
    switch(componentType) {
    case "event":
        var startDate = event.startDate.getInTimezone(kDefaultTimezone).jsDate;
        setElementValue("start-datetime", startDate);

        // only events have end dates. todos have due dates
        var endDate   = event.endDate.getInTimezone(kDefaultTimezone).jsDate;
        gDurationMSec = endDate - startDate;

        var displayEndDate = new Date(endDate);
        if (event.startDate.isDate) {
            //displayEndDate == icalEndDate - 1, in the case of allday events
            displayEndDate.setDate(displayEndDate.getDate() - 1);
        }
        setElementValue("end-datetime", displayEndDate);

        // event status fields
        switch(event.status) {
        case "TENTATIVE":
            setElementValue("event-status-field", "TENTATIVE");
            break;
        case "CONFIRMED":
            setElementValue("event-status-field", "CONFIRMED");
            break;
        case "CANCELLED":
            setElementValue("event-status-field", "CANCELLED");
            break;
        default:
            dump("loadCalendarEventDialog: ERROR! Got an invalid status: " +
                  event.status + "\n");
        }
        
        setElementValue("all-day-event-checkbox", event.startDate.isDate, "checked");
        break;
    case "todo":
        var hasEntry = event.entryDate ? true : false;
        var selectedDay = window.opener.gCalendarWindow.currentView.getNewEventDate();
        var entryDate = (hasEntry ? event.entryDate.jsDate : selectedDay );

        setElementValue("start-datetime", entryDate);
        setElementValue("start-datetime", !hasEntry, "disabled");
        setElementValue("start-checkbox", hasEntry,  "checked");

        var hasDue = event.dueDate ? true : false;
        var dueDate = (hasDue? event.dueDate.jsDate : selectedDay );

        setElementValue("due-datetime", dueDate);
        setElementValue("due-datetime", !hasDue, "disabled");
        setElementValue("due-checkbox", hasDue,  "checked" );

        /*integrate this into onDateTimePick()
        if (hasStart && hasDue)
            duration = dueDate.getTime() - startDate.getTime(); //in ms
        else
            duration = 0;*/

        setElementValue("percent-complete-menulist", event.percentComplete);

        // todo status fields
        if (event.completedDate)
            processToDoStatus(event.status, event.completedDate.jsDate);
        else
            processToDoStatus(event.status);
    }


    // GENERAL -----------------------------------------------------------
    setElementValue("title-field",       event.title  );
    setElementValue("description-field", event.getProperty("DESCRIPTION"));
    setElementValue("location-field",    event.getProperty("LOCATION"));
    setElementValue("uri-field",         event.getProperty("URL"));
    // only enable "Go" button if there's a url
    processTextboxWithButton("uri-field", "load-url-button");


    // PRIVACY -----------------------------------------------------------
    switch(event.privacy) {
    case "PUBLIC":
    case "PRIVATE":
    case "CONFIDENTIAL":
        setElementValue("privacy-menulist", event.privacy);
        break;
    case "":
        setElementValue("private-menulist", "PUBLIC");
        break;
    default:  // bogus value
        dump("loadCalendarEventDialog: ERROR! Event has invalid privacy string: " + event.privacy + "\n");
        break;
    }


    // PRIORITY ----------------------------------------------------------
    var priorityInteger = parseInt( event.priority );
    if( priorityInteger == 0 ) {
        menuListSelectItem("priority-levels", "0"); // not defined
    } else if( priorityInteger >= 1 && priorityInteger <= 4 ) {
        menuListSelectItem("priority-levels", "1"); // high priority
    } else if( priorityInteger == 5 ) {
        menuListSelectItem("priority-levels", "5"); // medium priority
    } else if( priorityInteger >= 6 && priorityInteger <= 9 ) {
        menuListSelectItem("priority-levels", "9"); // low priority
    } else {
        dump("loadCalendarEventDialog: ERROR! Event has invalid priority: " + event.priority +"\n");
    }


    // ALARMS ------------------------------------------------------------
    if (event.alarmTime == null) {
        menuListSelectItem("alarm-type", "none");
    } else {
        setElementValue("alarm-length-field", event.getProperty("alarmLength"));
        setElementValue("alarm-length-units", event.getProperty("alarmUnits"));
        if (componentType == "event" ||
           (componentType == "todo" && !(startPicker.disabled && duePicker.disabled) ) ) {
            // If the event has an alarm email address, assume email alarm type
            var alarmEmailAddress = event.getProperty("alarmEmailAddress");
            if (alarmEmailAddress && alarmEmailAddress != "") {
                menuListSelectItem("alarm-type", "email");
                setElementValue("alarm-email-field", alarmEmailAddress);
            } else {
                menuListSelectItem("alarm-type", "popup");
                /* XXX lilmatt: finish this by selection between popup and 
                   popupAndSound by checking pref "calendar.alarms.playsound" */
            }
            var alarmRelated = event.getProperty("alarmRelated");
            if (alarmRelated && alarmRelated != "") {
                // if only one picker is enabled, check that the appropriate related
                // parameter is chosen
                if ( (componentType == "event") ||
                     (componentType == "todo" && !startPicker.disabled &&
                         duePicker.disabled && alarmRelated == "START") ||
                     (componentType == "todo" && startPicker.disabled &&
                         !duePicker.disabled && alarmRelated == "END")  ||
                     (componentType == "todo" && !startPicker.disabled && !duePicker.disabled) )
                {
                     setElementValue("alarm-trigger-relation", alarmRelated);
                } else {
                     dump("loadCalendarEventDialog: ERROR! alarmRelated: " +
                           alarmRelated + " is invalid! (trying to set " +
                          "START/END when the necessary datepicker isn't " +
                          "enabled?)\n");
                }
            }
        }
        // hide/show fields and widgets for alarm type
        processAlarmType();
    }


    // RECURRENCE --------------------------------------------------------
    // Set up widgets so they're not empty. Event values will override.
    setElementValue("repeat-end-date-picker", new Date() );
    setElementValue("exceptions-date-picker", new Date() );

    if (event.recurrenceInfo) {
        // we can only display at most one rule and one set of exceptions;
        // nothing else.
        var theRule = null;
        var theExceptions = Array();

        var ritems = event.recurrenceInfo.getRecurrenceItems({});
        for (i in ritems) {
            if (ritems[i] instanceof calIRecurrenceRule) {
                if (theRule) {
                    dump ("eventDialog.js: Already found a calIRecurrenceRule, we can't handle multiple ones!\n");
                } else {
                    theRule = ritems[i].QueryInterface(calIRecurrenceRule);
                    if (theRule.isNegative) {
                        dump ("eventDialog.js: Found an EXRULE, we can't handle this!\n");
                        theRule = null;
                    }
                }
            } else if (ritems[i] instanceof calIRecurrenceDate) {
                var exc = ritems[i].QueryInterface(calIRecurrenceDate);
                if (exc.isNegative)
                    theExceptions.push(exc);
                else
                    dump ("eventDialog.js: found a calIRecurrenceDate that wasn't an exception!\n");
            }
        }

        if (theRule) {
            setElementValue("repeat-checkbox", true, "checked");
            setElementValue("repeat-length-field", theRule.interval);
            menuListSelectItem("repeat-length-units", theRule.type);

            //XXX display an error when there is no UI to display all the rules
            switch(theRule.type) {
            case "WEEKLY":
                var recurWeekdays = theRule.getComponent("BYDAY", {});
                for (i = 0; i < 7; i++ ) {
                    setElementValue("advanced-repeat-week-" + i, false, "checked");
                    setElementValue("advanced-repeat-week-" + i, false, "today"  );
                    for (var j = 0; j < recurWeekdays.length; j++) {
                        //libical day #s are one more than ours
                        if ((i + 1) == recurWeekdays[j])
                            setElementValue("advanced-repeat-week-"+i, true, "checked");
                    }
                }

                //get day number for event's start date, and check and disable it
                var dayNumber = getElementValue("start-datetime").getDay();
                setElementValue("advanced-repeat-week-"+dayNumber, true, "checked" );
                setElementValue("advanced-repeat-week-"+dayNumber, true, "disabled");
                setElementValue("advanced-repeat-week-"+dayNumber, true, "today"   );
                break;
            case "MONTHLY":
                recDays = theRule.getComponent("BYDAY", {});
                dump("rd: "+recDays[0]+"\n");
                if (recDays.length && recDays[0] > 8) {
                    radioGroupSelectItem("advanced-repeat-month", "advanced-repeat-dayofweek");
                }
                break;
            }

            if (!theRule.isFinite) {
                radioGroupSelectItem("repeat-until-group", "repeat-forever-radio");
            } else {
                if (theRule.isByCount) {
                    radioGroupSelectItem("repeat-until-group", "repeat-numberoftimes-radio");
                    setElementValue("repeat-numberoftimes-textbox", theRule.count );
                } else {
                    radioGroupSelectItem("repeat-until-group", "repeat-until-radio");
                    setElementValue("repeat-end-date-picker", theRule.endDate.jsDate);
                }
            }
        } else {
            menuListSelectItem("repeat-length-units", "DAILY");
        }
        updateRepeatUnitExtensions()

        if (theExceptions.length > 0) {
            for (i in theExceptions) {
                var date = theExceptions[i].date.jsDate;
                addException(date);
            }
        }
    }

    // Only show Add/Delete Exception buttons when appropriate
    updateAddExceptionButton();
    updateDeleteExceptionButton();

    /* Old crap:
    setElementValue("advanced-repeat-dayofmonth", (gEvent.recurWeekNumber == 0 || gEvent.recurWeekNumber == undefined), "selected");
    setElementValue("advanced-repeat-dayofweek", (gEvent.recurWeekNumber > 0 && gEvent.recurWeekNumber != 5), "selected");
    setElementValue("advanced-repeat-dayofweek-last", (gEvent.recurWeekNumber == 5), "selected");
    */


    // INVITEES ----------------------------------------------------------
    var inviteEmailAddress = event.getProperty("inviteEmailAddress");
    if (inviteEmailAddress != undefined && inviteEmailAddress != "") {
        setElementValue("invite-checkbox", true, "checked");
        setElementValue("invite-email-field", inviteEmailAddress);
    } else {
        setElementValue("invite-checkbox", false, "checked");
    }
    processInviteCheckbox();

    // handle attendees
    var attendeeList = event.getAttendees({});
    for (var i = 0; i < attendeeList.length; i++) {
        var attendee = attendeeList[i];
        addAttendee(attendee.id);
    }


    // ATTACHMENTS -------------------------------------------------------
    /* XXX this could work when attachments are supported by calItemBase
    var count = event.attachments.length;
    for (i = 0; i < count; i++) {
        var thisAttachment = event.attachments.queryElementAt(i, Components.interfaces.calIAttachment);
        addAttachment(thisAttachment);
    }
    */
    // Only show the Remove Attachment button if there are attachments
    updateRemoveAttachmentButton();


    // CATEGORIES --------------------------------------------------------
    // Load categories from preferences
    try {
    var categoriesString = opener.GetUnicharPref(opener.gCalendarWindow.calendarPreferences.calendarPref, "categories.names", getDefaultCategories());
    var categoriesList = categoriesString.split( "," );

    // insert the category already in the task so it doesn't get lost
    var categories = event.getProperty("CATEGORIES");
    if (categories) {
        if (categoriesString.indexOf(categories) == -1)
            categoriesList[categoriesList.length] = categories;
    }
    categoriesList.sort();

    var oldMenulist = document.getElementById( "categories-menulist-menupopup" );
    while( oldMenulist.hasChildNodes() )
        oldMenulist.removeChild( oldMenulist.lastChild );
    for (i = 0; i < categoriesList.length ; i++) {
        document.getElementById("categories-field").appendItem(categoriesList[i], categoriesList[i]);
    }
    if (categories)
        menuListSelectItem("categories-field", categories);
    else
        document.getElementById("categories-field").selectedIndex = -1;
    } catch (ex) {
        dump("unable to do categories stuff in event dialog\n");
    }
    // The event calendar is its current calendar, or for a new event,
    // the selected calendar in the calendar list, passed in args.calendar.
    var eventCalendar = event.calendar || args.calendar;

    // Initialize calendar names in drop down list.
    var calendarField = document.getElementById('server-field');
    var calendars = getCalendarManager().getCalendars({});
    for (i in calendars) {
        try {
            var menuitem = calendarField.appendItem(calendars[i].name,
                                                    calendars[i].uri);
            menuitem.calendar = calendars[i];
            // compare cal.uri because there may be multiple instances of
            // calICalendar or uri for the same spec, and those instances are
            // not ==.
            if (eventCalendar && eventCalendar.uri.equals(calendars[i].uri))
                calendarField.selectedIndex = i;
        } catch(ex) {
            dump("eventCalendar.uri not found...\n");
        }
    }

    // update enabling and disabling
    updateRepeatItemEnabled();
    updateStartEndItemEnabled();

    // start focus on title
    var firstFocus = document.getElementById("title-field");
    firstFocus.focus();

    // revert cursor from "wait" set in calendar.js editEvent, editNewEvent
    opener.setCursor("auto");

    self.focus();

    // fix a strict warning about not always returning a value
    return true;
}


/*
 * Called when the OK button is clicked.
 */

function onOKCommand()
{
    // Get arguments, see description at top of file
    var args = window.arguments[0];
    var event = args.calendarEvent;

    // if this event isn't mutable, we need to clone it like a sheep
    var originalEvent = event;

    // get values from the form and put them into the event
    var componentType = getElementValue("component-type");

    // calIEvent properties
    if (componentType == "event") {
        if (!event.isMutable) // I will cut vlad for making me do this QI
            event = originalEvent.clone().QueryInterface(Components.interfaces.calIEvent);

        event.startDate = jsDateToFloatingDateTime(getElementValue("start-datetime"));
        var endDate = getElementValue("end-datetime");
        if (getElementValue("all-day-event-checkbox", "checked") ) {
            event.startDate.isDate = true;
            event.endDate.isDate = true;
            // displayed all day end date is inclusive date, convert to exclusive end date.
            endDate.setDate(endDate.getDate() + 1); 
        }
        event.endDate = jsDateToFloatingDateTime(endDate);
        if (event.startDate.isDate) {
            event.endDate.isDate = true;
        }

        var status = getElementValue("event-status-field");
        if (status)
            event.status = status;
    } else if (isToDo(event)) {
        componentType = "todo";
        if (!event.isMutable) // I will cut vlad for making me do this QI
            event = originalEvent.clone().QueryInterface(Components.interfaces.calITodo);

        if ( getElementValue("start-checkbox", "checked") ) {
            event.entryDate = jsDateToFloatingDateTime(getElementValue("start-datetime"));
        } else {
            event.entryDate = null;
        }

        if ( getElementValue("due-checkbox", "checked") ) {
            event.dueDate = jsDateToFloatingDateTime(getElementValue("due-datetime"));
        } else {
            event.dueDate = null;
        }

        event.status          = getElementValue("todo-status-field");
        event.percentComplete = getElementValue("percent-complete-menulist");
        if ( event.status == "COMPLETED" && event.percentComplete == 100 ) {
            event.completedDate = jsDateToFloatingDateTime(getElementValue("completed-date-picker"));
        } else {
            event.completedDate = null;
        }
    } else {
        dump("eventDialog.js: ERROR! onOKCommand() found neither an event nor a todo!\n");
    }

    // XXX should do an idiot check here to see if duration is negative

    // calIItemBase properties
    event.title    = getElementValue("title-field");
    event.priority = getElementValue("priority-levels");

    // other properties
    var cats = getElementValue("categories-field");
    if (cats)
        event.setProperty("CATEGORIES", cats);

    var desc = getElementValue("description-field");
    if (desc)
        event.setProperty("DESCRIPTION", desc);

    var location = getElementValue("location-field");
    if (location)
        event.setProperty("LOCATION", location);

    var url = getElementValue("uri-field") ;
    if (url)
        event.setProperty("URL", url);


    // PRIVACY -----------------------------------------------------------
    var privacyValue = getElementValue("privacy-menulist");
    switch(privacyValue) {
    case "PUBLIC":
    case "PRIVATE":
    case "CONFIDENTIAL":
        event.privacy = privacyValue;
        break;
    default:  // bogus value
        dump("onOKCommand: ERROR! Event has invalid privacy-menulist value: " +
             privacyValue + "\n");
        break;
    }


    // ALARMS ------------------------------------------------------------
    var alarmType = getElementValue("alarm-type");
    if (alarmType != "" && alarmType != "none") {
        var alarmLength = getElementValue("alarm-length-field");
        var alarmUnits = getElementValue("alarm-length-units");
        var alarmRelated = getElementValue("alarm-trigger-relation");

        event.setProperty("alarmLength",  alarmLength);
        event.setProperty("alarmUnits",   alarmUnits);
        event.setProperty("alarmRelated", alarmRelated);

        var alarmTime = null;

        if (alarmRelated == "START") {
            if (event.startDate)
                alarmTime = event.startDate.clone();
            else if (event.entryDate)
                alarmTime = event.entryDate.clone();
        } else if (alarmRelated == "END") {
            if (event.endDate)
                alarmTime = event.endDate.clone();
            else if(event.dueDate)
                alarmTime = event.dueDate.clone();
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

    if (alarmType == "email")
        event.setProperty("alarmEmailAddress", getElementValue("alarm-email-field"));
    else
        event.deleteProperty("alarmEmailAddress");


    // RECURRENCE --------------------------------------------------------
    event.recurrenceInfo = null;
    
    if (getElementValue("repeat-checkbox", "checked")) {
        var recurrenceInfo = createRecurrenceInfo();
        debug("** recurrenceInfo: " + recurrenceInfo);
        recurrenceInfo.item = event;

        var recRule = new calRecurrenceRule();

        if (getElementValue("repeat-forever-radio", "selected")) {
            recRule.count = -1;
        } else if (getElementValue("repeat-numberoftimes-radio", "selected")) {
            recRule.count = Math.max(1, getElementValue("repeat-numberoftimes-textbox"));
        } else if (getElementValue("repeat-until-radio", "selected")) {
            var recurEndDate = getElementValue("repeat-end-date-picker");
            recRule.endDate = jsDateToFloatingDateTime(recurEndDate);
        }

        // don't allow for a null interval here; js
        // silently converts that to 0, which confuses
        // libical.
        var recurInterval = getElementValue("repeat-length-field");
        if (recurInterval == null)
            recurInterval = 1;
        recRule.interval = recurInterval;
        recRule.type     = getElementValue("repeat-length-units");

        switch(recRule.type) {
        case "WEEKLY":
            var checkedState;
            var byDay = new Array();
            for (var i = 0; i < 7; i++) {
                checkedState = getElementValue("advanced-repeat-week-" + i, "checked");
                if (checkedState) {
                    byDay.push(i + 1);
                }
            }
            recRule.setComponent("BYDAY", byDay.length, byDay);
            break;
        case "MONTHLY":
            if (getElementValue("advanced-repeat-dayofweek", "selected")) {
                var dow = getElementValue("start-datetime").getDay() + 1;
                var weekno = getWeekNumberOfMonth();
                recRule.setComponent("BYDAY", 1, [weekno * 8 + dow]);
            }
            break;
        }

        recurrenceInfo.appendRecurrenceItem(recRule);

        // Exceptions
        var listbox = document.getElementById("exception-dates-listbox");

        var exceptionArray = new Array();
        for (i = 0; i < listbox.childNodes.length; i++) {
            dump ("valuestr '" + listbox.childNodes[i].value + "'\n");
            var dateObj = new Date(parseInt(listbox.childNodes[i].value));
            var dt = jsDateToFloatingDateTime(dateObj);
            dt.isDate = true;

            var dateitem = new calRecurrenceDate();
            dateitem.isNegative = true;
            dateitem.date = dt;
            recurrenceInfo.appendRecurrenceItem(dateitem);
        }

        // Finally, set the recurrenceInfo
        event.recurrenceInfo = recurrenceInfo;
    }
    debug("RECURRENCE INFO ON EVENT: " + event.recurrenceInfo);


    // INVITEES ----------------------------------------------------------
    event.removeAllAttendees();
    var attendeeList = document.getElementById("bucketBody").getElementsByTagName("treecell");
    for (i = 0; i < attendeeList.length; i++) {
        label = attendeeList[i].getAttribute("label");
        attendee = createAttendee();
        attendee.id = label;
        event.addAttendee(attendee);
    }

    if (getElementValue("invite-checkbox", "checked"))
        event.setProperty("inviteEmailAddress", getElementValue("invite-email-field"));
    else
        event.deleteProperty("inviteEmailAddress");


    // ATTACHMENTS -------------------------------------------------------
    /* XXX this could will work when attachments are supported by calItemBase
    var attachmentListbox = documentgetElementById("attachmentBucket");
    var attachments = event.attachments.QueryInterface(Components.interfaces.nsIMutableArray);

    attachments.clear();
    for (i = 0; i < attachmentListbox.childNodes.length; i++) {
        attachment = Components.classes["@mozilla.org/calendar/attachment;1"].createInstance(Components.interfaces.calIAttachment);
        attachment.url = makeURL(attachmentListbox.childNodes[i].getAttribute("label"));
        attachments.appendElement(attachment);
    }
    */


    var calendar = document.getElementById("server-field").selectedItem.calendar;

    // :TODO: REALLY only do this if the alarm or start settings change.
    // if the end time is later than the start time... alert the user using text from the dtd.
    // call caller's on OK function

    gOnOkFunction(event, calendar, originalEvent);

    // tell standard dialog stuff to close the dialog
    return true;
}


/*
 * Compare dateA with dateB ignoring time of day of each date object.
 * Comparison based on year, month, and day, ignoring rest.
 * Returns
 *   -1 if dateA <  dateB (ignoring time of day)
 *    0 if dateA == dateB (ignoring time of day)
 *   +1 if dateA >  dateB (ignoring time of day)
 */
function compareIgnoringTimeOfDay(dateA, dateB)
{
    if (dateA.getFullYear() == dateB.getFullYear() &&
        dateA.getMonth() == dateB.getMonth() &&
        dateA.getDate() == dateB.getDate() ) {
        return 0;
    } else if (dateA < dateB) {
        return -1;
    } else if (dateA > dateB) {
        return 1;
    }
}


/*
 * Check that end date is not before start date, and update warning message
 * Return true if problem found.
 */
function checkSetTimeDate()
{
    var startDate      = getElementValue("start-datetime");
    var endDate        = getElementValue("end-datetime");
    var dateComparison = compareIgnoringTimeOfDay(endDate, startDate);

    if (dateComparison < 0) {
        // end date before start date
        setDateError(true);
        setTimeError(false);
        return false;
    } else if (dateComparison == 0) {
        // ok even for all day events, end date will become exclusive when saved.
        setDateError(false); 
        // start & end date same, so compare entire time (ms since 1970) if not allday.
        var isAllDay = getElementValue("all-day-event-checkbox", "checked");
        var isBadEndTime = (!isAllDay && (endDate.getTime() < startDate.getTime()));
        setTimeError(isBadEndTime);
        return !isBadEndTime;
    } else {
        // endDate > startDate
        setDateError(false);
        setTimeError(false);
        return true;
    }
}


/*
 * Check that the recurrence end date is after the end date of the event.
 * Unlike the time/date versions this one sets the error message too as it
 * doesn't depend on the outcome of any of the other tests
 */
function checkSetRecur()
{
    var endDate        = getElementValue("end-datetime");
    var recur          = getElementValue("repeat-checkbox", "checked");
    var recurUntil     = getElementValue("repeat-until-radio", "selected");
    var recurUntilDate = getElementValue("repeat-end-date-picker");

    var untilDateIsBeforeEndDate =
      ( recur && recurUntil && 
        // until date is less than end date if total milliseconds time is less
        recurUntilDate.getTime() < endDate.getTime() && 
        // if on same day, ignore time of day for now
        // (may change in future for repeats within a day, such as hourly)
        !( recurUntilDate.getFullYear() == endDate.getFullYear() &&
           recurUntilDate.getMonth() == endDate.getMonth() &&
           recurUntilDate.getDate() == endDate.getDate() ) );
    setRecurError(untilDateIsBeforeEndDate);

    var missingField = false;

    // Make sure the user puts data in all the necessary fields
    if (getElementValue("repeat-numberoftimes-radio", "selected") &&
       !hasPositiveIntegerValue("repeat-numberoftimes-textbox")) {
        setElementValue("repeat-numberoftimes-warning", false, "hidden");
        missingField = true;
    }
    else
        setElementValue("repeat-numberoftimes-warning", true, "hidden");

    if (getElementValue("repeat-checkbox", "checked")) {
        if (!hasPositiveIntegerValue("repeat-length-field")) {
            setElementValue("repeat-interval-warning", false, "hidden");
            missingField = true;
        }
        else
            setElementValue("repeat-interval-warning", true, "hidden");
    }
    // If recur isn't selected, there shouldn't be warnings.
    else {
        missingField = false;
        setElementValue("repeat-numberoftimes-warning", true, "hidden");
        setElementValue("repeat-interval-warning", true, "hidden");
    }

    return (!untilDateIsBeforeEndDate && !missingField);
}


/*
 * Called when the start or due datetime checkbox is clicked.
 * Enables/disables corresponding datetime picker and alarm relation.
 */
function onDateTimeCheckbox(checkbox, pickerId)
{
    setElementValue(pickerId, !checkbox.checked, "disabled");
    processAlarmType();
    updateOKButton();
}


function setRecurError(state)
{
    setElementValue("repeat-time-warning", !state, "hidden");
}


function setDateError(state)
{ 
    setElementValue("end-date-warning", !state, "hidden");
}


function setTimeError(state)
{ 
    setElementValue("end-time-warning", !state, "hidden");
}

/*
 * Check that start datetime <= due datetime if both exist.
 * Provide separate error message for startDate > dueDate or
 * startDate == dueDate && startTime > dueTime.
 */
function checkDueSetTimeDate()
{
    var startCheckbox = getElementValue("start-checkbox", "checked");
    var dueCheckbox   = getElementValue("due-checkbox", "checked");

    if (startCheckbox && dueCheckbox) {
        var startDate = getElementValue("start-datetime");
        var dueDate = getElementValue("due-datetime");
        var dateComparison = compareIgnoringTimeOfDay(dueDate, startDate);
        if ( dateComparison < 0 ) {
            // due before start
            setDueDateError(true);
            setDueTimeError(false);
            return false;
        } else if ( dateComparison == 0 ) {
            setDueDateError(false);
            var isBadEndTime = dueDate.getTime() < startDate.getTime();
            setDueTimeError(isBadEndTime);
            return !isBadEndTime;
        }
    }
    setDueDateError(false);
    setDueTimeError(false);
    return true;
}


function setDueDateError(state)
{
    setElementValue("due-date-warning", !state, "hidden");
}


function setDueTimeError(state)
{
    setElementValue("due-time-warning", !state, "hidden");
}


function setOkButton(state)
{
    if (state == false)
        document.getElementById("calendar-new-component-window").getButton("accept").setAttribute("disabled", true);
    else
        document.getElementById("calendar-new-component-window").getButton("accept").removeAttribute("disabled");
}


function updateOKButton()
{
    var checkRecur = checkSetRecur();
    var componentType = getElementValue("component-type");
    var checkTimeDate;
    if (componentType == "event")
        checkTimeDate = checkSetTimeDate();
    else
        checkTimeDate = checkDueSetTimeDate();
    setOkButton(checkRecur && checkTimeDate);
    //this.sizeToContent();
}


function updateDuration() {
    var startDate = getElementValue("start-datetime");
    var endDate = new Date(getElementValue("end-datetime"));
    if (getElementValue("all-day-event-checkbox", "checked")) {
        // all-day display end date is inclusive end date
        // but duration depends on exclusive end date
        endDate.setDate(endDate.getDate() + 1);
    }
    gDurationMSec = endDate - startDate;
}


function onDueDateTimePick(dateTimePicker) {
    // check for due < start
    updateOKButton();
    return;
}

function onEndDateTimePick(dateTimePicker) {
    updateDuration();
    // check for end < start    
    updateOKButton();
    return;
}

function onStartDateTimePick(dateTimePicker) {
    var startDate = new Date(dateTimePicker.value);

    // update the end date keeping the same duration
    var displayEndDate = new Date(startDate.getTime() + gDurationMSec);
    if (getElementValue("all-day-event-checkbox", "checked")) {
        // all-day display end date is inclusive end date
        // but duration depends on exclusive end date
        displayEndDate.setDate( displayEndDate.getDate() - 1 );
    }
    setElementValue("end-datetime", displayEndDate);

    // Initialize the until-date picker for recurring events to picked day,
    // if the picked date is in future and repeat is not checked.
    // UNTIL dates may be last occurrence start date, inclusive
    // [rfc2445sec4.3.10].
    if (startDate > new Date() && !getElementValue("repeat-checkbox", "checked")) {
        setElementValue("repeat-end-date-picker", startDate);
    }

    updateAdvancedWeekRepeat();
    updateAdvancedRepeatDayOfMonth();
    updateAddExceptionButton();
    updateOKButton();
}


/*
 * Called when the repeat checkbox is clicked.
 */
function commandRepeat()
{
    updateRepeatItemEnabled();
}


/*
 * Called when the until radio is clicked.
 */
function commandUntil()
{
    updateUntilItemEnabled();
    updateOKButton();
}


/*
 * Called when the all day checkbox is clicked.
 */
function commandAllDay()
{
    updateDuration(); // Same start/end means 24hrs if all-day, 0hrs if not.
    updateStartEndItemEnabled();
    updateOKButton();
}


/*
 * Called when the invite checkbox is clicked.
 */
function processInviteCheckbox()
{
    processEnableCheckbox("invite-checkbox" , "invite-email-field");
}


/*
 * Enable/Disable Repeat items
 */
function updateRepeatItemEnabled()
{
   var repeatDisableList = document.getElementsByAttribute("disable-controller", "repeat");

   if ( getElementValue("repeat-checkbox", "checked") ) {
      for( var i = 0; i < repeatDisableList.length; ++i ) {
         if( repeatDisableList[i].getAttribute("today") != "true" )
            repeatDisableList[i].removeAttribute("disabled");
      }
   } else {
      for( var j = 0; j < repeatDisableList.length; ++j )
         repeatDisableList[j].setAttribute("disabled", "true");
   }

   // udpate plural/singular
   updateRepeatPlural();
   updateAlarmPlural();

   // update until items whenever repeat changes
   updateUntilItemEnabled();

   // extra interface depending on units
   updateRepeatUnitExtensions();

   // show/hide the exception buttons
   updateAddExceptionButton();
   updateDeleteExceptionButton();
}


/*
 * Update plural singular menu items
 */
function updateRepeatPlural()
{
    updateMenuLabels("repeat-length-field", "repeat-length-units");
}


function updateAlarmPlural()
{
    updateMenuLabels("alarm-length-field", "alarm-length-units");
}


/*
 * Enable/Disable Until items
 */
function updateUntilItemEnabled()
{
    if (getElementValue("repeat-checkbox", "checked") &&
        getElementValue("repeat-numberoftimes-radio", "selected") ) {
        enableElement("repeat-numberoftimes-textbox");
    } else
        disableElement("repeat-numberoftimes-textbox");

    if (getElementValue("repeat-checkbox", "checked") &&
        getElementValue("repeat-until-radio", "selected") ) {
        enableElement("repeat-end-date-picker");
    } else
        disableElement("repeat-end-date-picker");
}


function updateRepeatUnitExtensions()
{
    var repeatMenu = document.getElementById( "repeat-length-units" );
    if( repeatMenu.selectedItem ) {
        switch( repeatMenu.selectedItem.value ) {
        case "DAILY":
            hideElement("repeat-extensions-week");
            hideElement("repeat-extensions-month");
            break;
        case "WEEKLY":
            showElement("repeat-extensions-week");
            hideElement("repeat-extensions-month");
            updateAdvancedWeekRepeat();
            break;
        case "MONTHLY":
            hideElement("repeat-extensions-week");
            showElement("repeat-extensions-month");
            //the following line causes resize problems after turning on repeating events
            updateAdvancedRepeatDayOfMonth();
            break;
        case "YEARLY":
            hideElement("repeat-extensions-week");
            hideElement("repeat-extensions-month");
            break;
        }
    }
}


/*
 * Enable/Disable Start/End items
 */
function updateStartEndItemEnabled()
{
    var editTimeDisabled = getElementValue("all-day-event-checkbox", "checked");
    setElementValue("start-datetime", editTimeDisabled, "timepickerdisabled");
    setElementValue("end-datetime",   editTimeDisabled, "timepickerdisabled");
}


/*
 * Handle key down in repeat field
 */
function repeatLengthKeyDown( repeatField )
{
    updateRepeatPlural();
}


/*
 * Handle key down in alarm field
 */
function alarmLengthKeyDown( repeatField )
{
    updateAlarmPlural();
}


function repeatUnitCommand( repeatMenu )
{
    updateRepeatUnitExtensions();
}


/*
 * function to set the menu items text
 */
function updateAdvancedWeekRepeat()
{
   //get the day number for today.
   var dayNumber = getElementValue("start-datetime").getDay();

   //uncheck them all if the repeat checkbox is checked
   var repeatCheckBox = getElementValue("repeat-checkbox", "checked");

   if(repeatCheckBox) {
      //uncheck them all
      for( var i = 0; i < 7; i++ ) {
         setElementValue("advanced-repeat-week-"+i, false, "disabled");
         setElementValue("advanced-repeat-week-"+i, false, "today");
      }
   }

   if(!repeatCheckBox) {
      for( i = 0; i < 7; i++ ) {
         setElementValue("advanced-repeat-week-"+i, false, "checked");
      }
   }

   setElementValue("advanced-repeat-week-"+dayNumber, "true", "checked" );
   setElementValue("advanced-repeat-week-"+dayNumber, "true", "disabled");
   setElementValue("advanced-repeat-week-"+dayNumber, "true", "today"   );
}


/*
 * function to set the menu items text
 */
function updateAdvancedRepeatDayOfMonth()
{
    //get the day number for today.
    var dayNumber      = getElementValue("start-datetime").getDate();
    var dayExtension   = getDayExtension(dayNumber);
    var weekNumber     = getWeekNumberOfMonth();
    var calStrings     = document.getElementById("bundle_calendar");
    var weekNumberText = getWeekNumberText(weekNumber);
    var dayOfWeekText  = getDayOfWeek(dayNumber);
    var ofTheMonthText = document.getElementById("ofthemonth-text" ).getAttribute("value");
    var lastText       = document.getElementById("last-text" ).getAttribute("value");
    var onTheText      = document.getElementById("onthe-text").getAttribute("value");
    var dayNumberWithOrdinal = dayNumber + dayExtension;
    var repeatSentence = calStrings.getFormattedString( "weekDayMonthLabel", [onTheText, dayNumberWithOrdinal, ofTheMonthText] );

    document.getElementById("advanced-repeat-dayofmonth").setAttribute("label", repeatSentence);

    if( weekNumber == 4 && isLastDayOfWeekOfMonth() ) {
        document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", calStrings.getFormattedString( "weekDayMonthLabel", [weekNumberText, dayOfWeekText, ofTheMonthText] ) );
        showElement("advanced-repeat-dayofweek-last");
        document.getElementById( "advanced-repeat-dayofweek-last" ).setAttribute( "label", calStrings.getFormattedString( "weekDayMonthLabel", [lastText, dayOfWeekText, ofTheMonthText] ) );
    } else if( weekNumber == 4 && !isLastDayOfWeekOfMonth() ) {
        document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", calStrings.getFormattedString( "weekDayMonthLabel", [weekNumberText, dayOfWeekText, ofTheMonthText] ) );
        hideElement("advanced-repeat-dayofweek-last");
    } else {
        document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", calStrings.getFormattedString( "weekDayMonthLabel", [weekNumberText, dayOfWeekText, ofTheMonthText] ) );
        hideElement("advanced-repeat-dayofweek-last");
    }
}


/*
 * function to enable or disable the add exception button
 */

function updateAddExceptionButton()
{
    //get the date from the picker
    var datePickerValue = getElementValue("exceptions-date-picker");

    if( isAlreadyException( datePickerValue ) || !getElementValue("repeat-checkbox", "checked") )
        disableElement("exception-add-button");
    else
        enableElement("exception-add-button");
}


function updateDeleteExceptionButton()
{
    if (!getElementValue("repeat-checkbox", "checked"))
        disableElement("delete-exception-button");
    else
        updateListboxDeleteButton("exception-dates-listbox", "delete-exception-button");
}


function removeSelectedExceptionDate()
{
    var exceptionsListbox = document.getElementById("exception-dates-listbox");
    var SelectedItem = exceptionsListbox.selectedItem;

    if ( SelectedItem )
        exceptionsListbox.removeChild( SelectedItem );

    // the file list changed - see if we have to enable/disable any buttons
    updateAddExceptionButton()
    updateDeleteExceptionButton();
}


function addException(dateToAdd)
{
    if (!dateToAdd) {
       // get the date from the date and time box.
       // returns a date object
       dateToAdd = getElementValue("exceptions-date-picker");
    }

    if ( isAlreadyException( dateToAdd ) )
       return;

    var DateLabel = (new DateFormater()).getFormatedDate(dateToAdd);

    // add a row to the listbox.
    var listbox = document.getElementById("exception-dates-listbox");
    // ensure user can see that add occurred (also, avoid bug 231765, bug 250123)
    listbox.ensureElementIsVisible( listbox.appendItem( DateLabel, dateToAdd.getTime() ));

    // the file list changed - see if we have to enable/disable any buttons
    updateAddExceptionButton()
    updateDeleteExceptionButton();
}


function isAlreadyException(dateObj)
{
   //check to make sure that the date is not already added.
   var listbox = document.getElementById( "exception-dates-listbox" );

   for( var i = 0; i < listbox.childNodes.length; i++ ) {
      var dateToMatch = new Date( );

      dateToMatch.setTime( listbox.childNodes[i].value );
      if( dateToMatch.getMonth() == dateObj.getMonth() && dateToMatch.getFullYear() == dateObj.getFullYear() && dateToMatch.getDate() == dateObj.getDate() )
         return true;
   }
   return false;
}


function getDayExtension(dayNumber)
{
   var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

   if ( dayNumber >= 1 && dayNumber <= 31 )
      return( dateStringBundle.GetStringFromName( "ordinal.suffix."+dayNumber ) );
   else {
      dump("ERROR: Invalid day number: " + dayNumber);
      return ( false );
   }
}


function getDayOfWeek()
{
    //get the day number for today.
    var dayNumber = getElementValue("start-datetime").getDay();

    var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

    //add one to the dayNumber because in the above prop. file, it starts at day1, but JS starts at 0
    var oneBasedDayNumber = parseInt(dayNumber) + 1;

    return(dateStringBundle.GetStringFromName("day." + oneBasedDayNumber + ".name") );
}


function getWeekNumberOfMonth()
{
    //get the day number for today.
    var startTime = getElementValue("start-datetime");
    var oldStartTime = startTime;
    var thisMonth = startTime.getMonth();
    var monthToCompare = thisMonth;
    var weekNumber = 0;

    while(monthToCompare == thisMonth) {
        startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );
        monthToCompare = startTime.getMonth();
        weekNumber++;
    }

    return(weekNumber);
}


function isLastDayOfWeekOfMonth()
{
    //get the day number for today.
    var startTime = getElementValue("start-datetime");
    var oldStartTime = startTime;
    var thisMonth = startTime.getMonth();
    var monthToCompare = thisMonth;
    var weekNumber = 0;

    while(monthToCompare == thisMonth) {
        startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );
        monthToCompare = startTime.getMonth();
        weekNumber++;
    }

    if (weekNumber > 3) {
        var nextWeek = new Date( oldStartTime.getTime() + ( 1000 * 60 * 60 * 24 * 7 ) );

        if (nextWeek.getMonth() != thisMonth) {
            //its the last week of the month
            return(true);
        }
    }
    return(false);
}


function getWeekNumberText(weekNumber)
{
    var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");
    if ( weekNumber >= 1 && weekNumber <= 4 )
        return( dateStringBundle.GetStringFromName( "ordinal.name."+weekNumber) );
    else if ( weekNumber == 5 )
        return( dateStringBundle.GetStringFromName( "ordinal.name.last" ) );
    else
        return(false);
}


/* FILE ATTACHMENTS */

function removeSelectedAttachment()
{
    var attachmentListbox = document.getElementById("attachmentBucket");
    var SelectedItem = attachmentListbox.selectedItem;

    if(SelectedItem)
        attachmentListbox.removeChild(SelectedItem);

    updateRemoveAttachmentButton()
}


function addAttachment(attachmentToAdd)
{
    if( !attachmentToAdd || isAlreadyAttachment(attachmentToAdd) )
        return;

    //add a row to the listbox
    document.getElementById("attachmentBucket").appendItem(attachmentToAdd.url, attachmentToAdd.url);

    updateRemoveAttachmentButton();
}


function isAlreadyAttachment(attachmentToAdd)
{
    //check to make sure that the file is not already attached
    var listbox = document.getElementById("attachmentBucket");

    for( var i = 0; i < listbox.childNodes.length; i++ ) {
        if( attachmentToAdd.url == listbox.childNodex[i].url )
            return true;
    }
    return false;
}


function updateRemoveAttachmentButton()
{
    updateListboxDeleteButton("attachmentBucket", "attachment-delete-button");
}


/* URL */

var launch = true;

function loadURL()
{
    if( launch == false ) //stops them from clicking on it twice
       return;

    launch = false;
    //get the URL from the text box
    var UrlToGoTo = getElementValue("uri-field");

    //it has to be > 4, since it needs at least 1 letter, a . and a two letter domain name.
    if(UrlToGoTo.length < 4)
       return;

    //check if it has a : in it
    if(UrlToGoTo.indexOf( ":" ) == -1)
       UrlToGoTo = "http://"+UrlToGoTo;

    //launch the browser to that URL
    launchBrowser( UrlToGoTo );
    launch = true;
}

/*
 *  A textbox changed - see if its button needs to be enabled or disabled
 */
function processTextboxWithButton( textboxId, buttonId )
{
    var theTextbox = document.getElementById( textboxId );
    var theButton  = document.getElementById( buttonId );

    // Only change things if we have to
    if ( theTextbox.textLength == 0 && !theButton.disabled ) {
        disableElement( buttonId );
    } else if ( theTextbox.textLength > 0 && theButton.disabled ) {
        enableElement( buttonId );
    }
}


function processAlarmType()
{
    var alarmMenu = document.getElementById("alarm-type");
    var componentType = getElementValue("component-type");
    if( alarmMenu.selectedItem ) {
        if( componentType == "event" ) {
            debug("processAlarmType: EVENT " + alarmMenu.selectedItem.value );
            switch( alarmMenu.selectedItem.value ) {
            case "none":
                disableElement("alarm-length-field");
                disableElement("alarm-length-units");
                disableElement("alarm-trigger-relation");
                disableElement("before-this-event-label");
                disableElement("alarm-email-field-label");
                disableElement("alarm-email-field");
                break;
            //case "popupAndSound":
            case "popup":
                enableElement("alarm-length-field");
                enableElement("alarm-length-units");
                enableElement("alarm-trigger-relation");
                enableElement("before-this-event-label");
                disableElement("alarm-email-field-label");
                disableElement("alarm-email-field");
                break;
            case "email":
                enableElement("alarm-length-field");
                enableElement("alarm-length-units");
                enableElement("alarm-trigger-relation");
                enableElement("before-this-event-label");
                enableElement("alarm-email-field-label");
                enableElement("alarm-email-field");
                break;
            }
        } else if( componentType == "todo" ) {
            debug("processAlarmType: TODO " + alarmMenu.selectedItem.value );
            var startChecked = getElementValue("start-checkbox", "checked");
            var dueChecked = getElementValue("due-checkbox", "checked");

            // disable alarms if and only if neither checked
            if ( (!startChecked && !dueChecked) ) {
                menuListSelectItem("alarm-type", "none");
                disableElement("alarm-length-field");
                disableElement("alarm-length-units");
                disableElement("alarm-trigger-relation");
                disableElement("before-this-todo-label");
                disableElement("alarm-email-field-label");
                disableElement("alarm-email-field");
                disableElement("alarm-type");
            } else {
                enableElement("alarm-type")
                switch( alarmMenu.selectedItem.value ) {
                case "none":
                    disableElement("alarm-length-field");
                    disableElement("alarm-length-units");
                    disableElement("alarm-trigger-relation");
                    disableElement("before-this-todo-label");
                    disableElement("alarm-email-field-label");
                    disableElement("alarm-email-field");
                    break;
                //case "popupAndSound":
                case "popup":
                    enableElement("alarm-length-field");
                    enableElement("alarm-length-units");
                    enableElement("before-this-todo-label");
                    disableElement("alarm-email-field-label");
                    disableElement("alarm-email-field");
                    if (startChecked && !dueChecked) {
                        menuListSelectItem("alarm-trigger-relation", "START");
                        disableElement("alarm-trigger-relation");
                    } else if (!startChecked && dueChecked) {
                        menuListSelectItem("alarm-trigger-relation", "END");
                        disableElement("alarm-trigger-relation");
                    } else {
                        enableElement("alarm-trigger-relation");
                    }
                    break;
                case "email":
                    enableElement("alarm-length-field");
                    enableElement("alarm-length-units");
                    enableElement("before-this-todo-label");
                    enableElement("alarm-email-field-label");
                    enableElement("alarm-email-field");
                    if (startChecked && !dueChecked) {
                        menuListSelectItem("alarm-trigger-relation", "START");
                        disableElement("alarm-trigger-relation");
                    } else if (!startChecked && dueChecked) {
                        menuListSelectItem("alarm-trigger-relation", "END");
                        disableElement("alarm-trigger-relation");
                    } else {
                        enableElement("alarm-trigger-relation");
                    }
                    break;
                }
            }
        } else
            dump("processAlarmType: ERROR! Invalid componentType passed: \n");
    } else
        dump("processAlarmType: ERROR! No alarmMenu.selectedItem!\n");
}


function processComponentType(componentType)
{
    debug("processComponentType: " + componentType );
    switch( componentType ) {
    case "event":
         // Hide and show the appropriate fields and widgets
        changeMenuState("todo", "event");
         // Set the menu properly if it isn't already
        menuListSelectItem("component-type", "event");
         // calling just enableElement _should_ work here, but it doesn't
        setElementValue("start-datetime", false, "disabled");
        enableElement("start-datetime");
        enableElement("end-datetime");
         // Set alarm trigger menulist text correctly (using singular/plural code)
        setElementValue("alarm-trigger-text-kludge", "2"); // event text is "plural"
        updateMenuLabels("alarm-trigger-text-kludge", "alarm-trigger-relation");
        enableElement("alarm-type")
         // Set menubar title correctly
        changeTitleBar("event");
        break;
    case "todo":
         // Hide and show the appropriate fields and widgets
        changeMenuState("event", "todo");
         // Set the menu properly if it isn't already
        menuListSelectItem("component-type", "todo");
         // Enable/disable date/time pickers as need be
        onDateTimeCheckbox("start-checkbox", "start-datetime");
        onDateTimeCheckbox("due-checkbox", "due-datetime");
         // Set alarm trigger menulist text correctly (using singular/plural code)
        setElementValue("alarm-trigger-text-kludge", "1"); // todo text is "singular"
        updateMenuLabels("alarm-trigger-text-kludge", "alarm-trigger-relation");
        // Set menubar title correctly
        changeTitleBar("todo");
        break;
    //case "journal":
    default:
        // We were passed an invalid value:
        dump("processComponentType: ERROR! Tried to select invalid component type: "+componentType+"\n");
        break;
    }
    processAlarmType();
}


/*
 *  Hides/shows/enables/disables fields and widgets using "hidden-controller"
 *  and "disable-controller" in the .xul
 */
function changeMenuState(hiddenController, showController, disableController, enableController)
{
    var hiddenList  = document.getElementsByAttribute( "hidden-controller",  hiddenController );
    var showList    = document.getElementsByAttribute( "hidden-controller",  showController );
    var disableList = document.getElementsByAttribute( "disable-controller", disableController );
    var enableList  = document.getElementsByAttribute( "disable-controller", enableController );

    for( var i = 0; i < hiddenList.length; ++i ) {
        hideElement( hiddenList[i].id );
    }

    for( i = 0; i < showList.length; ++i ) {
        showElement( showList[i].id );
    }

    for( i = 0; i < disableList.length; ++i ) {
        disableElement( disableList[i].id );
    }

    for( i = 0; i < enableList.length; ++i ) {
        enableElement( enableList[i].id );
    }
}


/*
 *  Changes window title bar title ("New Event", "Edit Task", etc.)
 */
function changeTitleBar(componentType)
{
    var title;
    // Sanity check input
    if ( componentType == "event" || componentType == "todo" ) {
        var args = window.arguments[0];

        // Is this a NEW event/todo, or are we EDITing an existing event/todo?
        if("new" == args.mode)
          title = document.getElementById("data-" + componentType + "-title-new" ).getAttribute("value");
        else
          title = document.getElementById("data-" + componentType + "-title-edit").getAttribute("value");
        debug("changeTitleBar: "+title);
        document.title = title;
    } else {
        dump("changeTitleBar: ERROR! Tried to change titlebar to invalid value: "+componentType+"\n");
    }
}


function processToDoStatus(status, passedInCompletedDate)
{
    // RFC2445 doesn't support completedDates without the todo's status
    // being "COMPLETED", however twiddling the status menulist shouldn't
    // nuking that information at this point (in case you change status
    // back to COMPLETED). When we go to store this VTODO as .ics the
    // date will get lost.

    var completedDate;
    if (passedInCompletedDate)
        completedDate = passedInCompletedDate;
    else
        completedDate = null;

    // remember the original values
    var oldPercentComplete = getElementValue("percent-complete-menulist", "value");
    var oldCompletedDate   = getElementValue("completed-date-picker");

    switch(status) {
    case "":
    case "NONE":
        menuListSelectItem("todo-status-field", "NONE");
        disableElement("completed-date-picker");
        disableElement("percent-complete-menulist");
        disableElement("percent-complete-label");
        break;
    case "CANCELLED":
        menuListSelectItem("todo-status-field", "CANCELLED");
        disableElement("completed-date-picker");
        disableElement("percent-complete-menulist");
        disableElement("percent-complete-label");
        break;
    case "COMPLETED":
        menuListSelectItem("todo-status-field", "COMPLETED");
        enableElement("completed-date-picker");
        enableElement("percent-complete-menulist");
        enableElement("percent-complete-label");
        // if there isn't a completedDate, set it to now
        if (!completedDate)
            completedDate = new Date();
        break;
    case "IN-PROCESS":
        menuListSelectItem("todo-status-field", "IN-PROCESS");
        disableElement("completed-date-picker");
        enableElement("percent-complete-menulist");
        enableElement("percent-complete-label");
        break;
    case "NEEDS-ACTION":
        menuListSelectItem("todo-status-field", "NEEDS-ACTION");
        disableElement("completed-date-picker");
        enableElement("percent-complete-menulist");
        enableElement("percent-complete-label");
        break;
    }
    if (status == "COMPLETED") {
        setElementValue("percent-complete-menulist", "100");
        setElementValue("completed-date-picker", completedDate);
    } else {
        setElementValue("percent-complete-menulist", oldPercentComplete);
        setElementValue("completed-date-picker", oldCompletedDate);
    }
}


function onInviteeAdd()
{
    var email = "mailto:" + getElementValue("invite-email-field");
    addAttendee(email);
}


function addAttendee(email)
{
    var treeItem = document.createElement("treeitem");
    var treeRow = document.createElement("treerow");
    var treeCell = document.createElement("treecell");
    treeCell.setAttribute("label", email);
    treeItem.appendChild(treeRow);
    treeRow.appendChild(treeCell);
    document.getElementById("bucketBody").appendChild(treeItem);
}



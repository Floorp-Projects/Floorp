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

var gEvent;          // event being edited
var gOnOkFunction;   // function to be called when user clicks OK

var gDuration = -1;   // used to preserve duration when changing event start.

const DEFAULT_ALARM_LENGTH = 15; //default number of time units, an alarm goes off before an event

const kRepeatDay_0 = 1<<0;//Sunday
const kRepeatDay_1 = 1<<1;//Monday
const kRepeatDay_2 = 1<<2;//Tuesday
const kRepeatDay_3 = 1<<3;//Wednesday
const kRepeatDay_4 = 1<<4;//Thursday
const kRepeatDay_5 = 1<<5;//Friday
const kRepeatDay_6 = 1<<6;//Saturday


var gStartDate = new Date( );
// Note: gEndDate is *exclusive* end date, so duration = end - start.
// For all-day(s) events (no times), displayed end date is one day earlier.
var gEndDate = new Date( ); // for events
var gDueDate = new Date( ); // for todos

/*-----------------------------------------------------------------
*   W I N D O W      F U N C T I O N S
*/

/**
*   Called when the dialog is loaded.
*/

function loadCalendarEventDialog()
{
    // Get arguments, see description at top of file
    var args = window.arguments[0];

    gOnOkFunction = args.onOk;

    // XXX I want to get rid of the use of gEvent
    gEvent = args.calendarEvent;

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

    debug("-----");
    debug("event: "+event);
    debug("-----");
    debug("event.startDate: "+event.startDate);
    debug("-----");

    // fill in fields from the event
    switch(componentType) {
    case "event":
        gStartDate = event.startDate.jsDate;
        document.getElementById("start-datetime").value = gStartDate;

        // only events have end dates. todos have due dates
        gEndDate = event.endDate.jsDate;
        var displayEndDate = new Date(gEndDate);
        /*
        if (event.isAllDay) {
            //displayEndDate == icalEndDate - 1, in the case of allday events
            displayEndDate.setDate(displayEndDate.getDate() - 1);
        }
        */
        document.getElementById("end-datetime").value = displayEndDate;

        gDuration = gEndDate.getTime() - gStartDate.getTime(); //in ms

        // event status fields
        switch(event.status) {
        case "TENTATIVE":
            setFieldValue("event-status-field", "TENTATIVE");
            break;
        case "CONFIRMED":
            setFieldValue("event-status-field", "CONFIRMED");
            break;
        case "CANCELLED":
            setFieldValue("event-status-field", "CANCELLED");
            break;
        }
        
        setFieldValue("all-day-event-checkbox", event.isAllDay, "checked");
        break;
    case "todo":
        var hasStart = event.start && event.start.isSet;
        if (hasStart) 
            gStartDate = event.startDate.jsDate;
        else
            gStartDate = null;

        var startPicker = document.getElementById( "start-datetime" );
        startPicker.value = gStartDate;
        startPicker.disabled = !hasStart;
        document.getElementById("start-checkbox").checked = hasStart;

        var hasDue = event.due && event.due.isSet;
        if (hasDue)
            gDueDate = event.dueDate.jsDate;
        else
            gDueDate = null;

        var duePicker = document.getElementById( "due-datetime" );
        duePicker.value = gDueDate;
        duePicker.disabled = !hasDue;
        document.getElementById("due-checkbox").checked = hasDue;

        if (hasStart && hasDue) {
            gDuration = gDueDate.getTime() - gStartDate.getTime(); //in ms
        } else {
            gDuration = 0;
        }

        // todo status fields
        processToDoStatus(event.status);

        var completedDatePicker = document.getElementById( "completed-date-picker" );
        if (event.completedDate) {
            var completedDate = event.completedDate.jsDate;
            completedDatePicker.value = completedDate;
        } else {
            completedDatePicker.value = null;
        }

        if (event.percentComplete && event.status == "IN-PROCESS" ) {
            setFieldValue( "percent-complete-menulist", event.percentComplete );
        }
        break;
    }


    // GENERAL -----------------------------------------------------------
    setFieldValue("title-field",       event.title  );
    setFieldValue("description-field", event.getProperty("description"));
    setFieldValue("location-field",    event.getProperty("location"));
    setFieldValue("uri-field",         event.getProperty("url"));
    // only enable "Go" button if there's a url
    processTextboxWithButton( "uri-field", "load-url-button" );


    // PRIVACY -----------------------------------------------------------
    switch(event.privacy) {
    case "PUBLIC":
    case "PRIVATE":
    case "CONFIDENTIAL":
        setFieldValue("privacy-menulist", event.privacy);
        break;
    case "":
        setFieldValue("private-menulist", "PUBLIC");
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
    if (!event.hasAlarm) {
        menuListSelectItem("alarm-type", "none");
    } else {
        setFieldValue("alarm-length-field",     event.getProperty("alarmLength"));
        setFieldValue("alarm-length-units",     event.getProperty("alarmUnits"));
        if (componentType == "event" ||
           (componentType == "todo" && !(startPicker.disabled && duePicker.disabled) ) ) {
            // If the event has an alarm email address, assume email alarm type
            var alarmEmailAddress = event.getProperty("alarmEmailAddress");
            if (alarmEmailAddress && alarmEmailAddress != "") {
                menuListSelectItem("alarm-type", "email");
                setFieldValue("alarm-email-field", alarmEmailAddress);
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
                     setFieldValue("alarm-trigger-relation", alarmRelated);
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
                if (exc.isNegative) {
                    theExceptions.push(exc);
                } else {
                    dump ("eventDialog.js: found a calIRecurrenceDate that wasn't an exception!\n");
                }
            }
        }

        if (theRule) {
            setFieldValue("repeat-checkbox", true, "checked");
            setFieldValue("repeat-length-field", theRule.interval);
            menuListSelectItem("repeat-length-units", theRule.type);

            if (theRule.count == -1) {
                setFieldValue("repeat-forever-radio", true, "selected");
            } else {
                if (theRule.isByCount) {
                    setFieldValue("repeat-numberoftimes-radio", true, "selected");
                    setFieldValue("repeat-numberoftimes-textbox", theRule.count);
                } else {
                    setFieldValue("repeat-until-radio", true, "selected" );
                    document.getElementById("repeat-end-date-picker").value = theRule.endDate.jsDate;
                }
            }
        } else {
            menuListSelectItem("repeat-length-units", "DAILY");
        }

        if (theExceptions.length > 0) {
            for (i in theExceptions) {
                var date = theExceptions[i].date.jsDate;
                addException(date);
            }
        }
    }
    // Put today's date in the recurrence exceptions datepicker
    document.getElementById("exceptions-date-picker").value = gStartDate;


    // INVITEES ----------------------------------------------------------
    var inviteEmailAddress = event.getProperty("inviteEmailAddress");
    if (inviteEmailAddress != undefined && inviteEmailAddress != "") {
        setFieldValue("invite-checkbox", true, "checked");
        setFieldValue("invite-email-field", inviteEmailAddress);
    } else {
        setFieldValue("invite-checkbox", false, "checked");
    }
    processInviteCheckbox();

    // handle attendees
    var attendeeList = event.getAttendees({});
    for (var i = 0; i < attendeeList.length; i++) {
        attendee = attendeeList[i];
        addAttendee(attendee.id);
    }


    // ATTACHMENTS -------------------------------------------------------
    /* XXX this could work when attachments are supported by calItemBase
    var count = event.attachments.length;
    for(var i = 0; i < count; i++) {
        var thisAttachment = event.attachments.queryElementAt(i, Components.interfaces.calIAttachment);
        addAttachment(thisAttachment);
    }
    */


    // CATEGORIES --------------------------------------------------------
    // Load categories from preferences
    var categoriesString = opener.GetUnicharPref(opener.gCalendarWindow.calendarPreferences.calendarPref, "categories.names", getDefaultCategories());
    var categoriesList = categoriesString.split( "," );

    // insert the category already in the task so it doesn't get lost
    var categories = event.getProperty("categories");
    if (categories) {
        if (categoriesString.indexOf(categories) == -1)
            categoriesList[categoriesList.length] = categories;
    }
    categoriesList.sort();

    var oldMenulist = document.getElementById( "categories-menulist-menupopup" );
    while( oldMenulist.hasChildNodes() )
        oldMenulist.removeChild( oldMenulist.lastChild );
    for (i = 0; i < categoriesList.length ; i++) {
        document.getElementById( "categories-field" ).appendItem(categoriesList[i], categoriesList[i]);
    }
    document.getElementById( "categories-field" ).selectedIndex = -1;
    menuListSelectItem("categories-field", event.getProperty("categories") );

    /* XXX

    // Server stuff
    document.getElementById( "server-menulist-menupopup" ).database.AddDataSource( opener.gCalendarWindow.calendarManager.rdf.getDatasource() );
    document.getElementById( "server-menulist-menupopup" ).builder.rebuild();
   
    if (args.mode == "new") {
        if ("server" in args)
            setFieldValue( "server-field", args.server );
        else
            document.getElementById( "server-field" ).selectedIndex = 1;
    } else {
        if (gEvent.parent)
            setFieldValue( "server-field", gEvent.parent.server );
        else
            document.getElementById( "server-field" ).selectedIndex = 1;
          
        //for now you can't edit which file the event is in.
        setFieldValue( "server-field", "true", "disabled" );
        setFieldValue( "server-field-label", "true", "disabled" );
    }
    */


    // update enabling and disabling
    updateRepeatItemEnabled();
    updateStartEndItemEnabled();

    updateAddExceptionButton();

    //set the advanced weekly repeating stuff
    setAdvancedWeekRepeat();

    /*
    setFieldValue("advanced-repeat-dayofmonth", (gEvent.recurWeekNumber == 0 || gEvent.recurWeekNumber == undefined), "selected");
    setFieldValue("advanced-repeat-dayofweek", (gEvent.recurWeekNumber > 0 && gEvent.recurWeekNumber != 5), "selected");
    setFieldValue("advanced-repeat-dayofweek-last", (gEvent.recurWeekNumber == 5), "selected");
    */


    // enable/disable subordinate buttons of textboxes
    /*  these are listboxes, not textboxes
    processTextboxWithButton( "exception-dates-listbox", "delete-exception-button" );
    processTextboxWithButton( "invite-email-field", "invite-email-button" ); */

    // start focus on title
    var firstFocus = document.getElementById("title-field");
    firstFocus.focus();

    // revert cursor from "wait" set in calendar.js editEvent, editNewEvent
    opener.setCursor( "auto" );

    self.focus();
    return;
}


/*
 * Called when the OK button is clicked.
 */

function onOKCommand()
{
    var event = gEvent;

    // if this event isn't mutable, we need to clone it like a sheep
    var originalEvent = event;
    // get values from the form and put them into the event

    var componentType;

    // calIEvent properties
    if (isEvent(event)) {
        componentType = "event";
        if (!event.isMutable) // I will cut vlad for making me do this QI
            event = originalEvent.clone().QueryInterface(Components.interfaces.calIEvent);

        event.startDate.jsDate = gStartDate;
        event.endDate.jsDate   = gEndDate;
        event.isAllDay = getFieldValue("all-day-event-checkbox", "checked");
        event.status   = getFieldValue("event-status-field");
    } else if (isToDo(event)) {
        componentType = "todo";
        if (!event.isMutable) // I will cut vlad for making me do this QI
            event = originalEvent.clone().QueryInterface(Components.interfaces.calITodo);

        dump ("this todo is: " + event + "\n");
        if (gStartDate) {
            event.entryDate = gStartDate;
        } else {
            event.entryDate.reset();
        }

        if (gDueDate) {
            event.dueDate = gDueDate;
        } else {
            event.dueDate.reset();
        }
        event.status          = getFieldValue("todo-status-field");
        event.percentComplete = getFieldValue("percent-complete-menulist");
    }

    // XXX should do an idiot check here to see if duration is negative

    // calIItemBase properties
    event.title    = getFieldValue("title-field");
    event.priority = getFieldValue("priority-levels");

    // other properties
    event.setProperty("categories",   getFieldValue("categories-field"));
    event.setProperty("description",  getFieldValue("description-field"));
    event.setProperty("location",     getFieldValue("location-field"));
    event.setProperty("url",          getFieldValue("uri-field"));


    // PRIVACY -----------------------------------------------------------
    var privacyValue = getFieldValue( "privacy-menulist" );
    switch(privacyValue) {
    case "PUBLIC":
    case "PRIVATE":
    case "CONFIDENTIAL":
        event.privacy = privacyValue;
        break;
    default:  // bogus value
        dump( "onOKCommand: ERROR! Event has invalid privacy-menulist value: " +
             privacyValue + "\n" );
        break;
    }


    // ALARMS ------------------------------------------------------------
    var alarmType = getFieldValue( "alarm-type" );
    if (alarmType != "" && alarmType != "none") {
        event.hasAlarm = true;
        event.setProperty("alarmLength",  getFieldValue("alarm-length-field"));
        event.setProperty("alarmUnits",   getFieldValue("alarm-length-units"));
        event.setProperty("alarmRelated", getFieldValue("alarm-trigger-relation"));
        //event.alarmTime = ...
    }
    if (alarmType == "email" )
        event.setProperty("alarmEmailAddress", getFieldValue("alarm-email-field"));
    else
        event.deleteProperty("alarmEmailAddress");


    // RECURRENCE --------------------------------------------------------
    event.recurrenceInfo = null;
    
    if (getFieldValue("repeat-checkbox", "checked")) {
        var recurrenceInfo = createRecurrenceInfo();
        debug("** recurrenceInfo: " + recurrenceInfo );
        recurrenceInfo.initialize(event);

        var recRule = new calRecurrenceRule();

        if (getFieldValue("repeat-forever-radio", "selected")) {
            recRule.count = -1;
        } else if (getFieldValue("repeat-numberoftimes-radio", "selected")) {
            recRule.count = Math.max(1, getFieldValue("repeat-numberoftimes-textbox"));
        } else if (getFieldValue("repeat-until-radio", "selected")) {
            var recurEndDate = document.getElementById("repeat-end-date-picker").value;
            recRule.endDate = jsDateToDateTime(recurEndDate);
        }

        // don't allow for a null interval here; js
        // silently converts that to 0, which confuses
        // libical.
        var recurInterval = getFieldValue("repeat-length-field");
        if (recurInterval == null)
            recurInterval = 1;
        recRule.interval = recurInterval;
        recRule.type     = getFieldValue("repeat-length-units");

        // XXX need to do extra work for weeks here and for months incase 
        // extra things are checked

        recurrenceInfo.appendRecurrenceItem(recRule);

        // Exceptions
        var listbox = document.getElementById("exception-dates-listbox");

        var exceptionArray = new Array();
        for (var i = 0; i < listbox.childNodes.length; i++) {
            dump ("valuestr '" + listbox.childNodes[i].value + "'\n");
            var dateObj = new Date(parseInt(listbox.childNodes[i].value));
            var dt = jsDateToDateTime(dateObj);
            dt.isDate = true;

            var dateitem = new calRecurrenceDate();
            dateitem.isNegative = true;
            dateitem.date = dt;
            recurrenceInfo.appendRecurrenceItem(dateitem);
        }

        // Finally, set the recurrenceInfo
        event.recurrenceInfo = recurrenceInfo;
    }
    debug("RECURRENCE INFO ON EVENT: " + event.recurrenceInfo );


    // INVITEES ----------------------------------------------------------
    event.removeAllAttendees();
    var attendeeList = document.getElementById("bucketBody").getElementsByTagName("treecell");
    for (i = 0; i < attendeeList.length; i++) {
        label = attendeeList[i].getAttribute("label");
        attendee = createAttendee();
        attendee.id = label;
        event.addAttendee(attendee);
    }

    if (getFieldValue("invite-checkbox", "checked"))
        event.setProperty("inviteEmailAddress", getFieldValue("invite-email-field"));
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


   var Server = getFieldValue( "server-field" );

   // :TODO: REALLY only do this if the alarm or start settings change.?
   //if the end time is later than the start time... alert the user using text from the dtd.
   // call caller's on OK function

   gOnOkFunction(event, Server, originalEvent);

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
   var dateComparison = compareIgnoringTimeOfDay(gEndDate, gStartDate);

   if (dateComparison < 0 ||
       (dateComparison == 0 && getFieldValue( "all-day-event-checkbox",
                                              "checked" )))
   {
      // end before start, or all day event and end date is not exclusive.
      setDateError(true);
      //showElement("end-date-warning");
      setTimeError(false);
      //hideElement("end-time-warning");
      return false;
   } else if (dateComparison == 0) {
      setDateError(false);
      //hideElement("end-date-warning");
      // start & end date same, so compare entire time (ms since 1970)
      var isBadEndTime = gEndDate.getTime() < gStartDate.getTime();
      setTimeError(isBadEndTime);
      return !isBadEndTime;
   } else {
      setDateError(false);
      //hideElement("end-date-warning");
      setTimeError(false);
      //hideElement("end-time-warning");
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
   var recur = getFieldValue( "repeat-checkbox", "checked" );
   var recurUntil = getFieldValue( "repeat-until-radio", "selected" );
   var recurUntilDate = document.getElementById("repeat-end-date-picker").value;
   
   var untilDateIsBeforeEndDate =
     ( recur && recurUntil && 
       // until date is less than end date if total milliseconds time is less
       recurUntilDate.getTime() < gEndDate.getTime() && 
       // if on same day, ignore time of day for now
       // (may change in future for repeats within a day, such as hourly)
       !( recurUntilDate.getFullYear() == gEndDate.getFullYear() &&
          recurUntilDate.getMonth() == gEndDate.getMonth() &&
          recurUntilDate.getDate() == gEndDate.getDate() ));
   setRecurError(untilDateIsBeforeEndDate);
   return(!untilDateIsBeforeEndDate);
}


/*
 * Called when the start or due datetime checkbox is clicked.
 * Enables/disables corresponding datetime picker and alarm relation.
 */

function onDateTimeCheckbox(checkbox, pickerId)
{
    var picker = document.getElementById( pickerId );
    picker.disabled = !checkbox.checked;

    processAlarmType();
    updateOKButton();
}


function setRecurError(state)
{
   document.getElementById("repeat-time-warning" ).setAttribute( "hidden", !state);
}


function setDateError(state)
{ 
   document.getElementById( "end-date-warning" ).setAttribute( "hidden", !state );
}


function setTimeError(state)
{ 
   document.getElementById( "end-time-warning" ).setAttribute( "hidden", !state );
}


/*
 * Check that the due date is after the start date, if they are the same day
 * then the checkDueTime function should catch the problem (if there is one).
 */

function checkDueDate()
{
  var startDate = document.getElementById( "start-datetime" ).value;
  var dueDate = document.getElementById( "due-datetime" ).value;
  compareIgnoringTimeOfDay(dueDate, startDate);
}


/*
 * Check that start datetime <= due datetime if both exist.
 * Provide separate error message for startDate > dueDate or
 * startDate == dueDate && startTime > dueTime.
 */

function checkDueSetTimeDate()
{
  var startCheckBox = document.getElementById( "start-checkbox" );
  var dueCheckBox = document.getElementById( "due-checkbox" );
  if (startCheckBox.checked && dueCheckBox.checked) {
    var CheckDueDate = checkDueDate();

    if ( CheckDueDate < 0 ) {
      // due before start
      setDueDateError(true);
      setDueTimeError(false);
      return false;
    } else if ( CheckDueDate == 0 ) {
      setDueDateError(false);
      // start & due same
      var CheckDueTime = checkDueTime();
      setDueTimeError(CheckDueTime);
      return !CheckDueTime;
    }
  }
  setDueDateError(false);
  setDueTimeError(false);
  return true;
}


function setDueDateError(state)
{
    document.getElementById( "due-date-warning" ).setAttribute( "hidden", !state );
}


function setDueTimeError(state)
{
    document.getElementById( "due-time-warning" ).setAttribute( "hidden", !state );
}


function setOkButton(state)
{
   if (state == false)
      document.getElementById( "calendar-new-component-window" ).getButton( "accept" ).setAttribute( "disabled", true );
   else
      document.getElementById( "calendar-new-component-window" ).getButton( "accept" ).removeAttribute( "disabled" );
}


function updateOKButton()
{
   var checkRecur = checkSetRecur();
   var checkTimeDate = checkSetTimeDate();
   setOkButton(checkRecur && checkTimeDate);
   //this.sizeToContent();
}


/*
 * Called when a datepicker is finished, and a date was picked.
 */

function onDateTimePick( dateTimePicker )
{
   var pickedDateTime = new Date( dateTimePicker.value );

   if( dateTimePicker.id == "end-datetime" ) {
     if( getFieldValue( "all-day-event-checkbox", "checked" ) ) {
       //display enddate == ical enddate - 1 (for allday events)
       pickedDateTime.setDate( pickedDateTime.getDate() + 1 );
     }
     gEndDate = pickedDateTime;
     // save the duration
     gDuration = gEndDate.getTime() - gStartDate.getTime();

     updateOKButton();
     return;
   }

   if( dateTimePicker.id == "start-datetime" ) {
     gStartDate = pickedDateTime;
     // preserve the previous duration by changing end
     gEndDate.setTime( gStartDate.getTime() + gDuration );
     
     var displayEndDate = new Date(gEndDate)
     if( getFieldValue( "all-day-event-checkbox", "checked" ) ) {
       //display enddate == ical enddate - 1 (for allday events)
       displayEndDate.setDate( displayEndDate.getDate() - 1 );
     }
     document.getElementById( "end-datetime" ).value = displayEndDate;
   }

   var now = new Date();

   // change the end date of recurring events to today, 
   // if the new date is after today and repeat is not checked.
   if ( pickedDateTime.getTime() > now.getTime() &&
        !getFieldValue( "repeat-checkbox", "checked" ) ) 
   {
     document.getElementById( "repeat-end-date-picker" ).value = pickedDateTime;
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
  //user enddate == ical enddate - 1 (for allday events)
  if( getFieldValue( "all-day-event-checkbox", "checked" ) ) {
    gEndDate.setDate( gEndDate.getDate() + 1 );
  } else { 
    gEndDate.setDate( gEndDate.getDate() - 1 );
  }
  // update the duration
  gDuration = gEndDate.getTime() - gStartDate.getTime();

   updateStartEndItemEnabled();
   updateOKButton();
}


/*
 * Called when the invite checkbox is clicked.
 */

function processInviteCheckbox()
{
    processEnableCheckbox( "invite-checkbox" , "invite-email-field" );
}


/*
 * Enable/Disable Repeat items
 */

function updateRepeatItemEnabled()
{
   var repeatCheckBox    = document.getElementById( "repeat-checkbox" );
   var repeatDisableList = document.getElementsByAttribute( "disable-controller", "repeat" );

   if( repeatCheckBox.checked ) {
      for( var i = 0; i < repeatDisableList.length; ++i ) {
         if( repeatDisableList[i].getAttribute( "today" ) != "true" )
            repeatDisableList[i].removeAttribute( "disabled" );
      }
   } else {
      for( var j = 0; j < repeatDisableList.length; ++j )
         repeatDisableList[j].setAttribute( "disabled", "true" );
   }
   
   // udpate plural/singular
   updateRepeatPlural();
   
   updateAlarmPlural();
   
   // update until items whenever repeat changes
   updateUntilItemEnabled();
   
   // extra interface depending on units
   updateRepeatUnitExtensions();
}


/*
 * Update plural singular menu items
 */

function updateRepeatPlural()
{
    updateMenuLabels( "repeat-length-field", "repeat-length-units" );
}


function updateAlarmPlural()
{
    updateMenuLabels( "alarm-length-field", "alarm-length-units" );
}


/*
 * Enable/Disable Until items
 */

function updateUntilItemEnabled()
{
    var repeatCheckBox = document.getElementById("repeat-checkbox");
    var numberRadio    = document.getElementById("repeat-numberoftimes-radio");
    var untilRadio     = document.getElementById("repeat-until-radio");

    if( repeatCheckBox.checked && numberRadio.selected  )
        enableElement("repeat-numberoftimes-textbox");
    else
        disableElement("repeat-numberoftimes-textbox");

    if( repeatCheckBox.checked && untilRadio.selected )
        enableElement("repeat-end-date-picker");
    else
        disableElement("repeat-end-date-picker");
}


function updateRepeatUnitExtensions( )
{
    var repeatMenu = document.getElementById( "repeat-length-units" );

    // XXX FIX ME! WHEN THE WINDOW LOADS, THIS DOESN'T EXIST
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
        //sizeToContent();
    }
}


/*
 * Enable/Disable Start/End items
 */

function updateStartEndItemEnabled()
{
   var allDayCheckBox = document.getElementById( "all-day-event-checkbox" );
   var editTimeDisabled = allDayCheckBox.checked;
   
   var startTimePicker = document.getElementById( "start-datetime" );
   var endTimePicker = document.getElementById( "end-datetime" );

   startTimePicker.timepickerdisabled = editTimeDisabled;
   endTimePicker.timepickerdisabled = editTimeDisabled;
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
 * Functions for advanced repeating elements
 */

function setAdvancedWeekRepeat()
{
   var checked = false;

   if( gEvent.recurWeekdays > 0 ) {
      for( var i = 0; i < 7; i++ ) {
         checked = ( ( gEvent.recurWeekdays | eval( "kRepeatDay_"+i ) ) == eval( gEvent.recurWeekdays ) );
         setFieldValue( "advanced-repeat-week-"+i, checked, "checked" );
         setFieldValue( "advanced-repeat-week-"+i, false, "today" );
      }
   }

   //get the day number for today.
   var dayNumber = document.getElementById( "start-datetime" ).value.getDay();

   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "checked" );
   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "disabled" );
   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "today" );
}


/*
 * Functions for advanced repeating elements
 */

function getAdvancedWeekRepeat()
{
   var Total = 0;

   for( var i = 0; i < 7; i++ ) {
      if( getFieldValue( "advanced-repeat-week-"+i, "checked" ) == true )
         Total += eval( "kRepeatDay_"+i );
   }
   return( Total );
}


/*
 * function to set the menu items text
 */

function updateAdvancedWeekRepeat()
{
   //get the day number for today.
   var dayNumber = document.getElementById( "start-datetime" ).value.getDay();
   
   //uncheck them all if the repeat checkbox is checked
   var repeatCheckBox = document.getElementById( "repeat-checkbox" );
   
   if( repeatCheckBox.checked ) {
      //uncheck them all
      for( var i = 0; i < 7; i++ ) {
         setFieldValue( "advanced-repeat-week-"+i, false, "disabled" );
         setFieldValue( "advanced-repeat-week-"+i, false, "today" );
      }
   }

   if( !repeatCheckBox.checked ) {
      for( i = 0; i < 7; i++ ) {
         setFieldValue( "advanced-repeat-week-"+i, false, "checked" );
      }
   }

   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "checked" );
   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "disabled" );
   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "today" );
}


/*
 * function to set the menu items text
 */

function updateAdvancedRepeatDayOfMonth()
{
    //get the day number for today.
    var dayNumber = document.getElementById( "start-datetime" ).value.getDate();
    var dayExtension = getDayExtension( dayNumber );
    var weekNumber = getWeekNumberOfMonth();
    var calStrings = document.getElementById("bundle_calendar");
    var weekNumberText = getWeekNumberText( weekNumber );
    var dayOfWeekText = getDayOfWeek( dayNumber );
    var ofTheMonthText = getFieldValue( "ofthemonth-text" );
    var lastText = getFieldValue( "last-text" );
    var onTheText = getFieldValue( "onthe-text" );
    var dayNumberWithOrdinal = dayNumber + dayExtension;
    var repeatSentence = calStrings.getFormattedString( "weekDayMonthLabel", [onTheText, dayNumberWithOrdinal, ofTheMonthText] );

    document.getElementById( "advanced-repeat-dayofmonth" ).setAttribute( "label", repeatSentence );

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
    var datePickerValue = document.getElementById( "exceptions-date-picker" ).value;

    if( isAlreadyException( datePickerValue ) ||
        document.getElementById( "repeat-checkbox" ).getAttribute( "checked" ) != "true" )
    {
        disableElement("exception-add-button");
    } else {
        enableElement("exception-add-button");
    }
}


function removeSelectedExceptionDate()
{
    var exceptionsListbox = document.getElementById( "exception-dates-listbox" );
    var SelectedItem = exceptionsListbox.selectedItem;

    if( SelectedItem )
        exceptionsListbox.removeChild( SelectedItem );
}


function addException( dateToAdd )
{
   if( !dateToAdd ) {
      //get the date from the date and time box.
      //returns a date object
      dateToAdd = document.getElementById( "exceptions-date-picker" ).value;
   }
   
   if( isAlreadyException( dateToAdd ) )
      return;

   var DateLabel = formatDate( dateToAdd );

   //add a row to the listbox.
   var listbox = document.getElementById( "exception-dates-listbox" );
   //ensure user can see that add occurred (also, avoid bug 231765, bug 250123)
   listbox.ensureElementIsVisible( listbox.appendItem( DateLabel, dateToAdd.getTime() ));

   //sizeToContent();
}


function isAlreadyException( dateObj )
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


function getDayExtension( dayNumber )
{
   var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

   if ( dayNumber >= 1 && dayNumber <= 31 )
      return( dateStringBundle.GetStringFromName( "ordinal.suffix."+dayNumber ) );
   else {
      dump("ERROR: Invalid day number: " + dayNumber);
      return ( false );
   }
}


function getDayOfWeek( )
{
   //get the day number for today.
   var dayNumber = document.getElementById( "start-datetime" ).value.getDay();
   
   var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

   //add one to the dayNumber because in the above prop. file, it starts at day1, but JS starts at 0
   var oneBasedDayNumber = parseInt( dayNumber ) + 1;
   
   return( dateStringBundle.GetStringFromName( "day."+oneBasedDayNumber+".name" ) );
}


function getWeekNumberOfMonth()
{
   //get the day number for today.
   var startTime = document.getElementById( "start-datetime" ).value;
   
   var oldStartTime = startTime;

   var thisMonth = startTime.getMonth();
   
   var monthToCompare = thisMonth;

   var weekNumber = 0;

   while( monthToCompare == thisMonth ) {
      startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );

      monthToCompare = startTime.getMonth();
      
      weekNumber++;
   }
   
   return( weekNumber );
}


function isLastDayOfWeekOfMonth()
{
   //get the day number for today.
   var startTime = document.getElementById( "start-datetime" ).value;
   
   var oldStartTime = startTime;

   var thisMonth = startTime.getMonth();
   
   var monthToCompare = thisMonth;

   var weekNumber = 0;

   while( monthToCompare == thisMonth ) {
      startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );

      monthToCompare = startTime.getMonth();

      weekNumber++;
   }

   if( weekNumber > 3 ) {
      var nextWeek = new Date( oldStartTime.getTime() + ( 1000 * 60 * 60 * 24 * 7 ) );

      if( nextWeek.getMonth() != thisMonth ) {
         //its the last week of the month
         return( true );
      }
   }
   return( false );
}


/* FILE ATTACHMENTS */

function removeSelectedAttachment()
{
    var attachmentListbox = document.getElementById("attachmentBucket");
    var SelectedItem = attachmentListbox.selectedItem;

    if(SelectedItem)
        attachmentListbox.removeChild(SelectedItem);
}


function addAttachment(attachmentToAdd)
{
   if(!attachmentToAdd)
      return;
   
   //add a row to the listbox
   document.getElementById("attachmentBucket").appendItem(attachmentToAdd.url, attachmentToAdd.url);

   //sizeToContent();
}


function getWeekNumberText( weekNumber )
{
    var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");
    if ( weekNumber >= 1 && weekNumber <= 4 )
        return( dateStringBundle.GetStringFromName( "ordinal.name."+weekNumber) );
    else if( weekNumber == 5 )
        return( dateStringBundle.GetStringFromName( "ordinal.name.last" ) );
    else
        return( false );
}


/* URL */

var launch = true;

function loadURL()
{
   if( launch == false ) //stops them from clicking on it twice
      return;

   launch = false;
   //get the URL from the text box
   var UrlToGoTo = document.getElementById( "uri-field" ).value;
   
   if( UrlToGoTo.length < 4 ) //it has to be > 4, since it needs at least 1 letter, a . and a two letter domain name.
      return;

   //check if it has a : in it
   if( UrlToGoTo.indexOf( ":" ) == -1 )
      UrlToGoTo = "http://"+UrlToGoTo;

   //launch the browser to that URL
   launchBrowser( UrlToGoTo );

   launch = true;
}


/**
*   Helper function for filling the form, set the value of a property of a XUL element
*
* PARAMETERS
*      elementId     - ID of XUL element to set 
*      newValue      - value to set property to ( if undefined no change is made )
*      propertyName  - OPTIONAL name of property to set, default is "value", use "checked" for 
*                               radios & checkboxes, "data" for drop-downs
*/

function setFieldValue( elementId, newValue, propertyName  )
{
    var undefined;

    if( newValue !== undefined ) {
        var field = document.getElementById( elementId );

        if( newValue === false ) {
            try {
                field.removeAttribute( propertyName );
            } catch (e) {
                dump("setFieldValue: field.removeAttribute couldn't remove "+propertyName+
                     " from "+elementId+" e: "+e+"\n");
            }
        } else if( propertyName ) {
            try {
                field.setAttribute( propertyName, newValue );
            } catch (e) {
                dump("setFieldValue: field.setAttribute couldn't set "+propertyName+
                     " from "+elementId+" to "+newValue+" e: "+e+"\n");
            }
        } else {
            field.value = newValue;
        }
    }
}


/**
*   Helper function for getting data from the form, 
*   Get the value of a property of a XUL element
*
* PARAMETERS
*      elementId     - ID of XUL element to get from 
*      propertyName  - OPTIONAL name of property to set, default is "value", use "checked" for 
*                               radios & checkboxes, "data" for drop-downs
*   RETURN
*      newValue      - value of property
*/

function getFieldValue( elementId, propertyName )
{
    var field = document.getElementById( elementId );

    if( propertyName )
        return field[ propertyName ];
    else
        return field.value;
}


/**
*   Helper function for getting a date/time from the form.
*   The element must have been set up with  setDateFieldValue or setTimeFieldValue.
*
* PARAMETERS
*      elementId     - ID of XUL element to get from 
* RETURN
*      newValue      - Date value of element
*/

function getDateTimeFieldValue( elementId )
{
   var field = document.getElementById( elementId );
   return field.editDate;
}


/**
*   Helper function for filling the form, set the value of a date field
*
* PARAMETERS
*      elementId     - ID of time textbox to set 
*      newDate       - Date Object to use
*/

function setDateFieldValue( elementId, newDate  )
{
   // set the value to a formatted date string
   
   var field = document.getElementById( elementId );
   field.value = formatDate( newDate );
   
   // add an editDate property to the item to hold the Date object
   // used in onDatePick to update the date from the date picker.
   // used in getDateTimeFieldValue to get the Date back out.
   
   // we clone the date object so changes made in place do not propagte
   
   field.editDate = new Date( newDate );
}


/**
*   Helper function for filling the form, set the value of a time field
*
* PARAMETERS
*      elementId     - ID of time textbox to set 
*      newDate       - Date Object to use
*/

function setTimeFieldValue( elementId, newDate  )
{
   // set the value to a formatted time string 
   var field = document.getElementById( elementId );
   field.value = formatTime( newDate );
   
   // add an editDate property to the item to hold the Date object
   // used in onTimePick to update the date from the time picker.
   // used in getDateTimeFieldValue to get the Date back out.
   
   // we clone the date object so changes made in place do not propagte
   
   field.editDate = new Date( newDate );
}


/*
 * Take a Date object and return a displayable date string i.e.: May 5, 1959
 */

function formatDate( date )
{
   return( opener.gCalendarWindow.dateFormater.getFormatedDate( date ) );
}


/*
 * Take a Date object and return a displayable time string i.e.: 12:30 PM
 */

function formatTime( time )
{
   var timeString = opener.gCalendarWindow.dateFormater.getFormatedTime( time );
   return timeString;
}


// XXX lilmatt's new crap for the new XUL


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
    var componentType = getFieldValue("component-type");
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
            var startChecked = getFieldValue("start-checkbox", "checked");
            var dueChecked = getFieldValue("due-checkbox", "checked");

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
        document.getElementById("start-datetime").setAttribute( "disabled", "false" );
        enableElement("start-datetime");
        enableElement("end-datetime");
         // Set alarm trigger menulist text correctly (using singular/plural code)
        setFieldValue("alarm-trigger-text-kludge", "2"); // event text is "plural"
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
        setFieldValue("alarm-trigger-text-kludge", "1"); // todo text is "singular"
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
        if( "new" == args.mode )
          title = document.getElementById( "data-"+componentType+"-title-new" ).getAttribute("value");
        else
          title = document.getElementById( "data-"+componentType+"-title-edit" ).getAttribute("value");
        debug("changeTitleBar: "+title);
        document.title = title;
    } else {
        dump("changeTitleBar: ERROR! Tried to change titlebar to invalid value: "+componentType+"\n");
    }
}


function processToDoStatus(status)
{
    var completedDatePicker = document.getElementById( "completed-date-picker" );
    switch(status) {
    case "":
    case "None":
        menuListSelectItem("todo-status-field", "None");
        disableElement( "completed-date-picker" );
        disableElement( "percent-complete-menulist" );
        disableElement( "percent-complete-label" );
        break;
    case "CANCELLED":
        menuListSelectItem("todo-status-field", "CANCELLED");
        disableElement( "completed-date-picker" );
        disableElement( "percent-complete-menulist" );
        disableElement( "percent-complete-label" );
        break;
    case "COMPLETED":
        menuListSelectItem("todo-status-field", "COMPLETED");
        enableElement( "completed-date-picker" );
        enableElement( "percent-complete-menulist" );
        enableElement( "percent-complete-label" );
        completedDatePicker.value = new Date();
        setFieldValue( "percent-complete-menulist", "100" );
        break;
    case "IN-PROCESS":
        menuListSelectItem("todo-status-field", "IN-PROCESS");
        enableElement( "completed-date-picker" );
        enableElement( "percent-complete-menulist" );
        enableElement( "percent-complete-label" );
        break;
    case "NEEDS-ACTION":
        menuListSelectItem("todo-status-field", "NEEDS-ACTION");
        enableElement( "percent-complete-menulist" );
        enableElement( "percent-complete-label" );
        break;
    }
}


function onInviteeAdd()
{
    textBox = document.getElementById("invite-email-field");
    var email = "mailto:" + textBox.value;
    addAttendee(email);
}


function addAttendee(email)
{
    treeItem = document.createElement("treeitem");
    treeRow = document.createElement("treerow");
    treeCell = document.createElement("treecell");
    treeCell.setAttribute("label", email);
    treeItem.appendChild(treeRow);
    treeRow.appendChild(treeCell);
    document.getElementById("bucketBody").appendChild(treeItem);
}



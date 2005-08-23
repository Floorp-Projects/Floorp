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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
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

/*
 * Utility functions used by all calendar hosts (Sunbird, Lightning, etc.).
 * Functions in this file must _not_ depend on functions in any other files.
 */
 
function getCalendarManager()
{
    var calendarManager = Components.classes["@mozilla.org/calendar/manager;1"]
                                    .getService(Components.interfaces.calICalendarManager);
    return calendarManager;
}

var gDisplayComposite = null;
function getDisplayComposite()
{
    if (!gDisplayComposite) {
       gDisplayComposite = Components.classes["@mozilla.org/calendar/calendar;1?type=composite"]
                                     .createInstance(Components.interfaces.calICompositeCalendar);
       gDisplayComposite.prefPrefix = 'calendar-main';
    }
    return gDisplayComposite;
}

function openCalendarWizard(callback)
{
    openDialog("chrome://calendar/content/calendarCreation.xul", "caEditServer", "chrome,titlebar,modal", callback);
}

function openCalendarProperties(aCalendar, callback)
{
    openDialog("chrome://calendar/content/calendarProperties.xul",
               "caEditServer", "chrome,titlebar,modal",
               {calendar: aCalendar, onOk: callback});
}

function makeURL(uriString)
{
    var ioservice = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    return ioservice.newURI(uriString, null, null);
}

var calIDateTime = Components.interfaces.calIDateTime;

var calIEvent = Components.interfaces.calIEvent;
var calITodo = Components.interfaces.calITodo;

var calIRecurrenceInfo = Components.interfaces.calIRecurrenceInfo;
var calIRecurrenceRule = Components.interfaces.calIRecurrenceRule;
var calIRecurrenceDate = Components.interfaces.calIRecurrenceDate;

var calRecurrenceInfo = Components.Constructor("@mozilla.org/calendar/recurrence-info;1", calIRecurrenceInfo);
var calRecurrenceRule = Components.Constructor("@mozilla.org/calendar/recurrence-rule;1", calIRecurrenceRule);
var calRecurrenceDate = Components.Constructor("@mozilla.org/calendar/recurrence-date;1", calIRecurrenceDate);

function createEvent()
{
    return Components.classes["@mozilla.org/calendar/event;1"].createInstance(Components.interfaces.calIEvent);
}

function createToDo()
{
    return Components.classes["@mozilla.org/calendar/todo;1"].createInstance(Components.interfaces.calITodo);
}

function createRecurrenceInfo()
{
    return Components.classes["@mozilla.org/calendar/recurrence-info;1"].createInstance(Components.interfaces.calIRecurrenceInfo);
}

function createDateTime()
{
    return Components.classes["@mozilla.org/calendar/datetime;1"].createInstance(Components.interfaces.calIDateTime);
}

function createAttendee()
{
    return Components.classes["@mozilla.org/calendar/attendee;1"].createInstance(Components.interfaces.calIAttendee);
}

function createAttachment()
{
    return Components.classes["@mozilla.org/calendar/attachment;1"].createInstance(Components.interfaces.calIAttachment);
}

function jsDateToDateTime(date)
{
    var newDate = createDateTime();
    newDate.jsDate = date;
    return newDate;
}

function isEvent(aObject)
{
   return aObject instanceof Components.interfaces.calIEvent;
}


function isToDo(aObject)
{
   return aObject instanceof Components.interfaces.calITodo;
}

/** If now is during an occurrence, return the ocurrence.
    Else if now is before an ocurrence, return the next ocurrence.
    Otherwise return the previous ocurrence. **/
function getCurrentNextOrPreviousRecurrence(calendarEvent)
{
    var isValid = false;
    var eventStartDate;

    if (calendarEvent.recur) {
        var now = new Date();
        var result = new Object();
        var dur = calendarEvent.endDate.jsDate - calendarEvent.startDate.jsDate;

        // To find current event when now is during event, look for occurrence
        // starting duration ago.
        var probeTime = now.getTime() - dur;
        isValid = calendarEvent.getNextRecurrence(probeTime, result);

        if (isValid) {
            eventStartDate = new Date(result.value);
        } else {
            isValid = calendarEvent.getPreviousOccurrence(probeTime, result);
            if (isValid) {
                eventStartDate = new Date(result.value);
            }
        }
    }
   
    if (!isValid) {
        eventStartDate = new Date( calendarEvent.startDate.jsDate );
    }
      
    return eventStartDate;
}

//
// timezone pref bits
// XXX is this really the function we want here?
//

var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefService);
var rootPrefNode = prefService.getBranch(null); // preferences root node

// returns the TZID of the timezone pref
var gDefaultTimezone = -1;
function calendarDefaultTimezone() {
    if (gDefaultTimezone == -1) {
        var prefobj = prefService.getBranch("calendar.");
        try {
            gDefaultTimezone = prefobj.getCharPref("timezone.local");
        } catch (e) {
            gDefaultTimezone = guessSystemTimezone();
            dump("gDefaultTimezone: " + gDefaultTimezone + "\n");
        }
    }
    
    return gDefaultTimezone;
}

/* We're going to do everything in our power, short of rumaging through the
*  user's actual file-system, to figure out the time-zone they're in.  The
*  deciding factors are the offsets given by (northern-hemisphere) summer and 
*  winter JSdates.  However, when available, we also use the name of the 
*  timezone in the JSdate, or a string-bundle term from the locale.
*  Returns a ICS timezone string.
*/
function guessSystemTimezone()
{
    var probableTZ = null;
    var summerTZname = null;
    var winterTZname = null;
    var summerDate = (new Date(2005,6,20)).toString();
    var winterDate = (new Date(2005,12,20)).toString();
    var summerData = summerDate.match(/[^(]* ([^ ]*) \(([^)]+)\)/);
    var winterData = winterDate.match(/[^(]* ([^ ]*) \(([^)]+)\)/);
    if (summerData && summerData[2]) 
        summerTZname = summerData[2];
    if (winterData && winterData[2])
        winterTZname = winterData[2];

    var index = summerDate.indexOf('+');
    if (index < 0)
        index = summerDate.indexOf('-');
    // the offset is always 5 characters long
    var summerOffset = summerDate.substr(index, 5);
    index = winterDate.indexOf('+');
    if (index < 0)
        index = winterDate.indexOf('-');
    // the offset is always 5 characters long
    var winterOffset = winterDate.substr(index, 5);

    dump("Guessing system timezone:\n");
    dump("summerOffset: " + summerOffset + "\nwinterOffset: " + winterOffset + "\n");
    if (summerTZname && winterTZname)
        dump("summerTZname: " + summerTZname + "\nwinterTZname: " + winterTZname + "\n");

    var icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);

    // returns 0=definitely not, 1=maybe, 2=likely
    function checkTZ(someTZ)
    {
        var comp = icssrv.getTimezone(someTZ);
        var subComp = comp.getFirstSubcomponent("VTIMEZONE");
        var standard = subComp.getFirstSubcomponent("STANDARD");
        var standardTZOffset = standard.getFirstProperty("TZOFFSETTO").stringValue;
        var standardName = standard.getFirstProperty("TZNAME").stringValue;
        var daylight = subComp.getFirstSubcomponent("DAYLIGHT");
        var daylightTZOffset = null;
        var daylightName = null;
        if (daylight) {
            daylightTZOffset = daylight.getFirstProperty("TZOFFSETTO").stringValue;
            daylightName = daylight.getFirstProperty("TZNAME").stringValue;
        }
        if (winterOffset == standardTZOffset && winterOffset == summerOffset &&
           !daylight) {
            if(!standardName || standardName == summerTZname)
                return 2;
            return 1;
        }
        if (winterOffset == standardTZOffset && summerOffset == daylightTZOffset) {
            // This seems backwards to me too, but it's how the data is written
            if ((!standardName || standardName == summerTZname) &&
                (!daylightName || daylightName == winterTZname))
                return 2;
            return 1;
        }
        return 0;
    }
    try {
        var stringBundleTZ = gCalendarBundle.getString("likelyTimezone");
        switch (checkTZ(stringBundleTZ)) {
            case 0: break;
            case 1: 
                if (!probableTZ)
                    probableTZ = stringBundleTZ;
                break;
            case 2:
                return stringBundleTZ;
        }
    }
    catch(ex) { } //Oh well, this didn't work, next option...
        
    var tzIDs = icssrv.timezoneIds;
    while (tzIDs.hasMore()) {
        theTZ = tzIDs.getNext();
        switch (checkTZ(theTZ)) {
            case 0: break;
            case 1: 
                if (!probableTZ)
                    probableTZ = theTZ;
                break;
            case 2:
                return theTZ;
        }
    }
    // If we get to this point, should we alert the user?
    if (probableTZ)
        return probableTZ;
    // Everything failed, so this is our only option.
    return "floating";
}

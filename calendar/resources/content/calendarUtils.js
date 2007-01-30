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

function now()
{
    var d = Components.classes['@mozilla.org/calendar/datetime;1'].createInstance(Components.interfaces.calIDateTime);
    d.jsDate = new Date();
    return d.getInTimezone(calendarDefaultTimezone());
}

function jsDateToDateTime(date)
{
    var newDate = createDateTime();
    newDate.jsDate = date;
    return newDate;
}

function jsDateToFloatingDateTime(date)
{
    var newDate = createDateTime();
    newDate.timezone = "floating";
    newDate.year = date.getFullYear();
    newDate.month = date.getMonth();
    newDate.day = date.getDate();
    newDate.hour = date.getHours();
    newDate.minute = date.getMinutes();
    newDate.second = date.getSeconds();
    newDate.normalize();
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

/**
 * Normal get*Pref calls will throw if the pref is undefined.  This function
 * will get a bool, int, or string pref.  If the pref is undefined, it will
 * return aDefault.
 *
 * @param aPrefName   the (full) name of preference to get
 * @param aDefault    (optional) the value to return if the pref is undefined
 */
function getPrefSafe(aPrefName, aDefault) {
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    const prefB = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(nsIPrefBranch);
    var value;
    try {
        switch (prefB.getPrefType(aPrefName)) {
            case nsIPrefBranch.PREF_BOOL:
                value = prefB.getBoolPref(aPrefName);
                break;
            case nsIPrefBranch.PREF_INT:
                value = prefB.getIntPref(aPrefName);
                break;
            case nsIPrefBranch.PREF_STRING:
                value = prefB.getCharPref(aPrefName);
                break;
            default: // includes nsIPrefBranch.PREF_INVALID
                return aDefault;
        }
        return value;
    } catch(ex) {
        return aDefault;
    }
}

/**
 * Wrapper for setting prefs of various types
 *
 * @param aPrefName   the (full) name of preference to set
 * @param aPrefType   the type of preference to set.  Valid valuse are:
                        BOOL, INT, and CHAR
 * @param aPrefValue  the value to set the pref to
 */
function setPref(aPrefName, aPrefType, aPrefValue) {
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    const prefB = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(nsIPrefBranch);
    switch (aPrefType) {
        case "BOOL":
            prefB.setBoolPref(aPrefName, aPrefValue);
            break;
        case "INT":
            prefB.setIntPref(aPrefName, aPrefValue);
            break;
        case "CHAR":
            prefB.setCharPref(aPrefName, aPrefValue);
            break;
    }
}

/**
 * Helper function to set a localized (complex) pref from a given string
 *
 * @param aPrefName   the (full) name of preference to set
 * @param aString     the string to which the preference value should be set
 */
function setLocalizedPref(aPrefName, aString) {
    const prefB = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefBranch);
    var str = Components.classes["@mozilla.org/supports-string;1"]
                        .createInstance(Components.interfaces.nsISupportsString);
    str.data = aString;
    prefB.setComplexValue(aPrefName, Components.interfaces.nsISupportsString, str);
}

/**
 * Like getPrefSafe, except for complex prefs (those used for localized data).
 *
 * @param aPrefName   the (full) name of preference to get
 * @param aDefault    (optional) the value to return if the pref is undefined
 */
function getLocalizedPref(aPrefName, aDefault) {
    const pb2 = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch2);
    var result;
    try {
        result = pb2.getComplexValue(aPrefName, 
                                     Components.interfaces.nsISupportsString).data;
    } catch(ex) {
        return aDefault;
    }
    return result;
}

/**
 * Gets the value of a string in a .properties file
 *
 * @param aBundleName  the name of the properties file.  It is assumed that the
 *                     file lives in chrome://calendar/locale/
 * @param aStringName the name of the string within the properties file
 */
function calGetString(aBundleName, aStringName)
{
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/"+aBundleName+".properties");
    return props.GetStringFromName(aStringName);
}

/** If now is during an occurrence, return the ocurrence.
    Else if now is before an ocurrence, return the next ocurrence.
    Otherwise return the previous ocurrence. **/
function getCurrentNextOrPreviousRecurrence(calendarEvent)
{
    if (!calendarEvent.recurrenceInfo) {
        return calendarEvent;
    }

    var dur = calendarEvent.duration.clone();
    dur.isNegative = true;

    // To find current event when now is during event, look for occurrence
    // starting duration ago.
    var probeTime = now();
    probeTime.addDuration(dur);
    //XXX getNextOccurrence doesn't work, bug 337346
    //var occ = calendarEvent.recurrenceInfo.getNextOccurrence(probeTime);
    var occtime = calendarEvent.recurrenceInfo.getNextOccurrenceDate(probeTime);

    var occ;
    if (!occtime) {
        var occs = calendarEvent.recurrenceInfo.getOccurrences(calendarEvent.startDate, probeTime, 0, {});
        occ = occs[occs.length -1];
    } else {
        occ = calendarEvent.recurrenceInfo.getOccurrenceFor(occtime);
    }
    return occ;
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

            var icsSvc = Components.classes["@mozilla.org/calendar/ics-service;1"]
                                   .getService(Components.interfaces.calIICSService);

            // Update this tzid if necessary.
            if (icsSvc.latestTzId(gDefaultTimezone).length) {
                gDefaultTimezone = icsSvc.latestTzId(gDefaultTimezone);
                prefobj.setCharPref("timezone.local", gDefaultTimezone);
            }
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
    var TZname1 = null;
    var TZname2 = null;
    var Date1 = (new Date(2005,6,20)).toString();
    var Date2 = (new Date(2005,12,20)).toString();
    var nameData1 = Date1.match(/[^(]* ([^ ]*) \(([^)]+)\)/);
    var nameData2 = Date2.match(/[^(]* ([^ ]*) \(([^)]+)\)/);
    if (nameData1 && nameData1[2]) 
        TZname1 = nameData1[2];
    if (nameData2 && nameData2[2])
        TZname2 = nameData2[2];

    var index = Date1.indexOf('+');
    if (index < 0)
        index = Date2.indexOf('-');
    // the offset is always 5 characters long
    var TZoffset1 = Date1.substr(index, 5);
    index = Date2.indexOf('+');
    if (index < 0)
        index = Date2.indexOf('-');
    // the offset is always 5 characters long
    var TZoffset2 = Date2.substr(index, 5);

    dump("Guessing system timezone:\n");
    dump("TZoffset1: " + TZoffset1 + "\nTZoffset2: " + TZoffset2 + "\n");
    if (TZname1 && TZname2)
        dump("TZname1: " + TZname1 + "\nTZname2: " + TZname2 + "\n");

    var icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);

    // returns 0=definitely not, 1=maybe, 2=likely
    function checkTZ(someTZ)
    {
        var comp = icssrv.getTimezone(someTZ);
        var subComp = comp.getFirstSubcomponent("VTIMEZONE");
        var standard = subComp.getFirstSubcomponent("STANDARD");
        var standardTZOffset = standard.getFirstProperty("TZOFFSETTO").valueAsIcalString;
        var standardName = standard.getFirstProperty("TZNAME").valueAsIcalString;
        var daylight = subComp.getFirstSubcomponent("DAYLIGHT");
        var daylightTZOffset = null;
        var daylightName = null;
        if (daylight) {
            daylightTZOffset = daylight.getFirstProperty("TZOFFSETTO").valueAsIcalString;
            daylightName = daylight.getFirstProperty("TZNAME").valueAsIcalString;
        }
        if (TZoffset2 == standardTZOffset && TZoffset2 == TZoffset1 &&
           !daylight) {
            if(!standardName || standardName == TZname1)
                return 2;
            return 1;
        }
        if (TZoffset2 == standardTZOffset && TZoffset1 == daylightTZOffset) {
            if ((!standardName || standardName == TZname1) &&
                (!daylightName || daylightName == TZname2))
                return 2;
            return 1;
        }

        // Now flip them and check again, to cover the southern hemisphere case
        if (TZoffset1 == standardTZOffset && TZoffset2 == TZoffset1 &&
           !daylight) {
            if(!standardName || standardName == TZname2)
                return 2;
            return 1;
        }
        if (TZoffset1 == standardTZOffset && TZoffset2 == daylightTZOffset) {
            if ((!standardName || standardName == TZname2) &&
                (!daylightName || daylightName == TZname1))
                return 2;
            return 1;
        }
        return 0;
    }
    try {
        var stringBundleTZ = gCalendarBundle.getString("likelyTimezone");

        if (stringBundleTZ.indexOf("/mozilla.org/") == -1) {
            // This happens if the l10n team didn't know how to get a time from
            // tzdata.c.  To convert an Olson time to a ics-timezone-string we
            // need to append this prefix.
            // XXX Get this prefix from calIICSService.tzIdPrefix
            stringBundleTZ = "/mozilla.org/20070129_1/" + stringBundleTZ;
        }

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
        var theTZ = tzIDs.getNext();
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

/**
 * Pick whichever of "black" or "white" will look better when used as a text
 * color against a background of bgColor. 
 *
 * @param bgColor   the background color as a "#RRGGBB" string
 */
function getContrastingTextColor(bgColor)
{
    var calcColor = bgColor.replace(/#/g, "");
    var red = parseInt(calcColor.substring(0, 2), 16);
    var green = parseInt(calcColor.substring(2, 4), 16);
    var blue = parseInt(calcColor.substring(4, 6), 16);

    // Calculate the L(ightness) value of the HSL color system.
    // L = (max(R, G, B) + min(R, G, B)) / 2
    var max = Math.max(Math.max(red, green), blue);
    var min = Math.min(Math.min(red, green), blue);
    var lightness = (max + min) / 2;

    // Consider all colors with less than 50% Lightness as dark colors
    // and use white as the foreground color; otherwise use black.
    // Actually we use a threshold a bit below 50%, so colors like
    // #FF0000, #00FF00 and #0000FF still get black text which looked
    // better when we tested this.
    if (lightness < 120) {
        return "white";
    }
    
    return "black";
}

/**
 * Returns the selected day in the views in a platform (Sunbird vs. Lightning)
 * neutral way
 */
function getSelectedDay() {
    var sbView = document.getElementById("view-deck");
    var ltnView = document.getElementById("calendar-view-box");
    var viewDeck = sbView || ltnView;
    return viewDeck.selectedPanel.selectedDay;
}


/**
 * Read default alarm settings from user preferences and apply them to
 * the event/todo passed in.
 *
 * @param aItem   The event or todo the settings should be applied to.
 */
function setDefaultAlarmValues(aItem)
{
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefService);
    var alarmsBranch = prefService.getBranch("calendar.alarms.");

    if (isEvent(aItem)) {
        try {
            if (alarmsBranch.getIntPref("onforevents") == 1) {
                var alarmOffset = Components.classes["@mozilla.org/calendar/duration;1"]
                                            .createInstance(Components.interfaces.calIDuration);
                try {
                    var units = alarmsBranch.getCharPref("eventalarmunit");
                    alarmOffset[units] = alarmsBranch.getIntPref("eventalarmlen");
                    alarmOffset.isNegative = true;
                } catch(ex) {
                    alarmOffset.minutes = 15;
                }
                aItem.alarmOffset  = alarmOffset;
                aItem.alarmRelated = Components.interfaces.calIItemBase.ALARM_RELATED_START;
            }
        } catch (ex) {
            Components.utils.reportError(
                "Failed to apply default alarm settings to event: " + ex);
        }
    } else if (isToDo(aItem)) {
        try {
            if (alarmsBranch.getIntPref("onfortodos") == 1) {
                // You can't have an alarm if the entryDate doesn't exist.
                if (!aItem.entryDate) {
                    aItem.entryDate = getSelectedDay().clone();
                }
                var alarmOffset = Components.classes["@mozilla.org/calendar/duration;1"]
                                            .createInstance(Components.interfaces.calIDuration);
                try {
                    var units = alarmsBranch.getCharPref("todoalarmunit");
                    alarmOffset[units] = alarmsBranch.getIntPref("todoalarmlen");
                    alarmOffset.isNegative = true;
                } catch(ex) {
                    alarmOffset.minutes = 15;
                }
                aItem.alarmOffset  = alarmOffset;
                aItem.alarmRelated = Components.interfaces.calIItemBase.ALARM_RELATED_START;
            }
        } catch (ex) {
            Components.utils.reportError(
                "Failed to apply default alarm settings to task: " + ex);
        }
    }
}

/**
 * Opens the print dialog
 */
function calPrint()
{
    window.openDialog("chrome://calendar/content/printDialog.xul", "Print",
                      "centerscreen,chrome,resizable");
}

function calRadioGroupSelectItem(radioGroupId, id)
{
    var radioGroup = document.getElementById(radioGroupId);
    var index = calRadioGroupIndexOf(radioGroup, id);
    if (index != -1) {
        radioGroup.selectedIndex = index;
    } else {
        throw "radioGroupSelectItem: No such Element: "+id;
    }
}

function calRadioGroupIndexOf(radioGroup, id)
{
    var items = radioGroup.getElementsByTagName("radio");
    var i;
    for (i in items) {
        if (items[i].getAttribute("id") == id) {
            return i;
        }
    }
    return -1; // not found
}

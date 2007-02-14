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
 * The Original Code is Calendar component utils.
 *
 * The Initial Developer of the Original Code is
 *   Joey Minta <jminta@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
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

/* This file contains commonly used functions in a centralized place so that
 * various components (and other js scopes) don't need to replicate them. Note
 * that loading this file twice in the same scope will throw errors.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

/* Returns a clean new calIEvent */
function createEvent() {
    return Cc["@mozilla.org/calendar/event;1"].createInstance(Ci.calIEvent);
}

/* Returns a clean new calITodo */
function createTodo() {
    return Cc["@mozilla.org/calendar/todo;1"].createInstance(Ci.calITodo);
}

/* Returns a clean new calIDateTime */
function createDateTime() {
    return Cc["@mozilla.org/calendar/datetime;1"].
           createInstance(Ci.calIDateTime);
}

/* Returns a clean new calIRecurrenceInfo */
function createRecurrenceInfo() {
    return Cc["@mozilla.org/calendar/recurrence-info;1"].
           createInstance(Ci.calIRecurrenceInfo);
}

/* Returns a clean new calIAttendee */
function createAttendee() {
    return Cc["@mozilla.org/calendar/attendee;1"].
           createInstance(Ci.calIAttendee);
}

/* Shortcut to the calendar-manager service */
function getCalendarManager() {
    return Components.classes["@mozilla.org/calendar/manager;1"].
           getService(Ci.calICalendarManager);
}

/**
 * Function to get the (cached) best guess at a user's default timezone.  We'll
 * use the value of the calendar.timezone.local preference, if it exists.  If
 * not, we'll do our best guess.
 *
 * @returns  a string of the Mozilla TZID for the user's default timezone.
 */
var gDefaultTimezone;
function calendarDefaultTimezone() {
    if (!gDefaultTimezone) {
        gDefaultTimezone = getPrefSafe("calendar.timezone.local", null);
        if (!gDefaultTimezone) {
            gDefaultTimezone = guessSystemTimezone();
        }
    }
    return gDefaultTimezone;
}

/**
 * We're going to do everything in our power, short of rumaging through the
 * user's actual file-system, to figure out the time-zone they're in.  The
 * deciding factors are the offsets given by (northern-hemisphere) summer and
 * winter JSdates.  However, when available, we also use the name of the
 * timezone in the JSdate, or a string-bundle term from the locale.
 *
 * @returns  a ICS timezone string.
*/
function guessSystemTimezone() {
    var probableTZ = null;
    var TZname1 = null;
    var TZname2 = null;
    var Date1 = (new Date(2005,6,20)).toString();
    var Date2 = (new Date(2005,12,20)).toString();
    var nameData1 = Date1.match(/[^(]* ([^ ]*) \(([^)]+)\)/);
    var nameData2 = Date2.match(/[^(]* ([^ ]*) \(([^)]+)\)/);

    if (nameData1 && nameData1[2]) {
        TZname1 = nameData1[2];
    }
    if (nameData2 && nameData2[2]) {
        TZname2 = nameData2[2];
    }

    var index = Date1.indexOf('+');
    if (index < 0) {
        index = Date2.indexOf('-');
    }

    // the offset is always 5 characters long
    var TZoffset1 = Date1.substr(index, 5);
    index = Date2.indexOf('+');
    if (index < 0)
        index = Date2.indexOf('-');
    // the offset is always 5 characters long
    var TZoffset2 = Date2.substr(index, 5);

    dump("Guessing system timezone:\n");
    dump("TZoffset1: " + TZoffset1 + "\nTZoffset2: " + TZoffset2 + "\n");
    if (TZname1 && TZname2) {
        dump("TZname1: " + TZname1 + "\nTZname2: " + TZname2 + "\n");
    }

    var icsSvc = Cc["@mozilla.org/calendar/ics-service;1"].
                 getService(Ci.calIICSService);

    // returns 0=definitely not, 1=maybe, 2=likely
    function checkTZ(someTZ)
    {
        var comp = icsSvc.getTimezone(someTZ);
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
            if (!standardName || standardName == TZname1) {
                return 2;
            }
            return 1;
        }

        if (TZoffset2 == standardTZOffset && TZoffset1 == daylightTZOffset) {
            if ((!standardName || standardName == TZname1) &&
                (!daylightName || daylightName == TZname2)) {
                return 2;
            }
            return 1;
        }

        // Now flip them and check again, to cover the southern hemisphere case
        if (TZoffset1 == standardTZOffset && TZoffset2 == TZoffset1 &&
           !daylight) {
            if (!standardName || standardName == TZname2) {
                return 2;
            }
            return 1;
        }

        if (TZoffset1 == standardTZOffset && TZoffset2 == daylightTZOffset) {
            if ((!standardName || standardName == TZname2) &&
                (!daylightName || daylightName == TZname1)) {
                return 2;
            }
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
            stringBundleTZ = "/mozilla.org/20050126_1/" + stringBundleTZ;
        }

        switch (checkTZ(stringBundleTZ)) {
            case 0:
                break;
            case 1:
                if (!probableTZ)
                    probableTZ = stringBundleTZ;
                break;
            case 2:
                return stringBundleTZ;
        }
    }
    catch (ex) { // Oh well, this didn't work, next option...
    }
        
    var tzIDs = icsSvc.timezoneIds;
    while (tzIDs.hasMore()) {
        var theTZ = tzIDs.getNext();
        switch (checkTZ(theTZ)) {
            case 0: break;
            case 1: 
                if (!probableTZ) {
                    probableTZ = theTZ;
                }
                break;
            case 2:
                return theTZ;
        }
    }

    // If we get to this point, should we alert the user?
    if (probableTZ) {
        return probableTZ;
    }

    // Everything failed, so this is our only option.
    return "floating";
}

/**
 * Shared dialog functions
 */

/**
 * Opens the Create Calendar wizard
 *
 * @param aCallback  a function to be performed after calendar creation
 */
function openCalendarWizard(aCallback) {
    openDialog("chrome://calendar/content/calendarCreation.xul", "caEditServer",
               "chrome,titlebar,modal", aCallback);
}

/**
 * Opens the calendar properties window for aCalendar
 *
 * @param aCalendar  the calendar whose properties should be displayed
 * @param aCallback  function that should be run when the dialog is accepted
 */
function openCalendarProperties(aCalendar, aCallback) {
    openDialog("chrome://calendar/content/calendarProperties.xul",
               "caEditServer", "chrome,titlebar,modal",
               {calendar: aCalendar, onOk: aCallback});
}

/**
 * Opens the print dialog
 */
function calPrint() {
    openDialog("chrome://calendar/content/printDialog.xul", "Print",
               "centerscreen,chrome,resizable");
}

/**
 * Other functions
 */

/**
 * Takes a string and returns an nsIURI
 *
 * @param aUriString  the string of the address to for the spec of the nsIURI
 *
 * @returns  an nsIURI whose spec is aUriString
 */
function makeURL(aUriString) {
    var ioSvc = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
    return ioSvc.newURI(aUriString, null, null);
}

/**
 * Returns a calIDateTime that corresponds to the current time in the user's
 * default timezone.
 */
function now() {
    var d = createDateTime();
    d.jsDate = new Date();
    return d.getInTimezone(calendarDefaultTimezone());
}

/**
 * Returns a calIDateTime corresponding to a javascript Date
 *
 * @param aDate  a javascript date
 * @returns      a calIDateTime whose jsDate is aDate
 *
 * @warning  Use of this function is strongly discouraged.  calIDateTime should
 *           be used directly whenever possible.
 */
function jsDateToDateTime(aDate) {
    var newDate = createDateTime();
    newDate.jsDate = aDate;
    return newDate;
}

/**
 * Returns a calIDateTime with a floating timezone and each field the same as
 * the equivalent field of the javascript date aDate
 *
 * @param aDate a javascript date
 * @returns     a calIDateTime with all of the fields the same as aDate
 *
 * @warning  Like jsDateToDateTime, use of the function is strongly discouraged.
 */
function jsDateToFloatingDateTime(aDate) {
    var newDate = createDateTime();
    newDate.timezone = "floating";
    newDate.year = aDate.getFullYear();
    newDate.month = aDate.getMonth();
    newDate.day = aDate.getDate();
    newDate.hour = aDate.getHours();
    newDate.minute = aDate.getMinutes();
    newDate.second = aDate.getSeconds();
    newDate.normalize();
    return newDate;
}

/**
 * Selects an item with id aItemId in the radio group with id aRadioGroupId
 *
 * @param aRadioGroupId  the id of the radio group which contains the item
 * @param aItemId        the item to be selected
 */
function calRadioGroupSelectItem(aRadioGroupId, aItemId) {
    var radioGroup = document.getElementById(aRadioGroupId);
    var items = radioGroup.getElementsByTagName("radio");
    var index;
    for (var i in items) {
        if (items[i].getAttribute("id") == aItemId) {
            index = i;
            break;
        }
    }
    ASSERT(index && index != 0, "Can't find radioGroup item to select.", true);
    radioGroup.selectedIndex = index;
}

/**
 * Determines whether or not the aObject is a calIEvent
 *
 * @param aObject  the object to test
 * @returns        true if the object is a calIEvent, false otherwise
 */
function isEvent(aObject) {
    return aObject instanceof Ci.calIEvent;
}

/**
 * Determines whether or not the aObject is a calITodo
 *
 * @param aObject  the object to test
 * @returns        true if the object is a calITodo, false otherwise
 */
function isToDo(aObject) {
    return aObject instanceof Ci.calITodo;
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
    switch (prefB.getPrefType(aPrefName)) {
        case nsIPrefBranch.PREF_BOOL:
            return prefB.getBoolPref(aPrefName);
        case nsIPrefBranch.PREF_INT:
            return prefB.getIntPref(aPrefName);
        case nsIPrefBranch.PREF_STRING:
            return prefB.getCharPref(aPrefName);
        default: // includes nsIPrefBranch.PREF_INVALID
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
    const prefB = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefBranch);
    var str = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
    str.data = aString;
    prefB.setComplexValue(aPrefName, Ci.nsISupportsString, str);
}

/**
 * Like getPrefSafe, except for complex prefs (those used for localized data).
 *
 * @param aPrefName   the (full) name of preference to get
 * @param aDefault    (optional) the value to return if the pref is undefined
 */
function getLocalizedPref(aPrefName, aDefault) {
    const pb2 = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch2);
    var result;
    try {
        result = pb2.getComplexValue(aPrefName, Ci.nsISupportsString).data;
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
function calGetString(aBundleName, aStringName) {
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/"+aBundleName+".properties");
    return props.GetStringFromName(aStringName);
}

/** Returns a best effort at making a UUID.  If we have the UUIDGenerator
 * service available, we'll use that.  If we're somewhere where it doesn't
 * exist, like Lightning in TB 1.5, we'll just use the current time.
 */
function getUUID() {
    if ("@mozilla.org/uuid-generator;1" in Components.classes) {
        var uuidGen = Cc["@mozilla.org/uuid-generator;1"].
                      getService(Ci.nsIUUIDGenerator);
        // generate uuids without braces to avoid problems with 
        // CalDAV servers that don't support filenames with {}
        return uuidGen.generateUUID().toString().replace(/[{}]/g, '');
    }
    // No uuid service (we're on the 1.8.0 branch)
    return "uuid" + (new Date()).getTime();
}

/** Due to a bug in js-wrapping, normal == comparison can fail when we
 * have 2 objects.  Use these functions to force them both to get wrapped
 * the same way, allowing for normal comparison.
 */
 
/**
 * calIItemBase comparer
 */
function compareItems(aItem, aOtherItem) {
    var sip1 = Cc["@mozilla.org/supports-interface-pointer;1"].
               createInstance(Ci.nsISupportsInterfacePointer);
    sip1.data = aItem;
    sip1.dataIID = Ci.calIItemBase;

    var sip2 = Cc["@mozilla.org/supports-interface-pointer;1"].
               createInstance(Ci.nsISupportsInterfacePointer);
    sip2.data = aOtherItem;
    sip2.dataIID = Ci.calIItemBase;
    return sip1.data == sip2.data;
}

/**
 * Generic object comparer
 * Use to compare two objects which are not of type calIItemBase, in order
 * to avoid the js-wrapping issues mentioned above.
 *
 * @param aObject        first object to be compared
 * @param aOtherObject   second object to be compared
 * @param aIID           IID to use in comparison
 */
function compareObjects(aObject, aOtherObject, aIID) {
    var sip1 = Cc["@mozilla.org/supports-interface-pointer;1"].
               createInstance(Ci.nsISupportsInterfacePointer);
    sip1.data = aObject;
    sip1.dataIID = aIID;

    var sip2 = Cc["@mozilla.org/supports-interface-pointer;1"].
               createInstance(Ci.nsISupportsInterfacePointer);
    sip2.data = aOtherObject;
    sip2.dataIID = aIID;
    return sip1.data == sip2.data;
}

/****
 **** debug code
 ****/

/**
 * Logs a string or an object to both stderr and the js-console only in the case 
 * where the calendar.debug.log pref is set to true.
 *
 * @param aArg  either a string to log or an object whose entire set of 
 *              properties should be logged.
 */
function LOG(aArg) {
    var prefB = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    var shouldLog = false;
    try {
        shouldLog = prefB.getBoolPref("calendar.debug.log");
    } catch(ex) {}

    if (!shouldLog) {
        return;
    }
    ASSERT(aArg, "Bad log argument.", false);
    var string;
    // We should just dump() both String objects, and string primitives.
    if (!(aArg instanceof String) && !(typeof(aArg) == "string")) {
        var string = "Logging object...\n";
        for (var prop in aArg) {
            string += prop + ': ' + aArg[prop] + '\n';
        }
        string += "End object\n";
    } else {
        string = aArg;
    }
 
    dump(string + '\n');
    var consoleSvc = Cc["@mozilla.org/consoleservice;1"].
                     getService(Ci.nsIConsoleService);
    consoleSvc.logStringMessage(string);
}

/**
 * Returns a string describing the current js-stack.  Note that this is
 * different than Components.stack, in that STACK just returns that js
 * functions that were called on the way to this function.
 *
 * @param aDepth (optional) The number of frames to include
 */
function STACK(aDepth) {
    var depth = aDepth || 5;
    var stack = "";
    var frame = arguments.callee.caller;
    for (var i = 1; i <= depth; i++) {
        stack += i+": "+ frame.name+ "\n";
        frame = frame.arguments.callee.caller;
        if (!frame) {
            break;
        }
    }
    return stack;
}

/**
 * Logs a message and the current js-stack, if aCondition fails
 *
 * @param aCondition  the condition to test for
 * @param aMessage    the message to report in the case the assert fails
 * @param aCritical   if true, throw an error to stop current code execution
 *                    if false, code flow will continue
 */
function ASSERT(aCondition, aMessage, aCritical) {
    if (aCondition) {
        return;
    }

    var string = "Assert failed: " + aMessage + '\n' + STACK();
    if (aCritical) {
        throw new Error(string);
    } else {
        Components.utils.reportError(string);
    }
}


/**
 * Auth prompt implementation - Uses password manager if at all possible.
 */
function calAuthPrompt() {
    // use the window watcher service to get a nsIAuthPrompt impl
    this.mPrompter = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                               .getService(Components.interfaces.nsIWindowWatcher)
                               .getNewAuthPrompter(null);
    this.mTriedStoredPassword = false;
}

calAuthPrompt.prototype = {
    prompt: function capP(aDialogTitle, aText, aPasswordRealm, aSavePassword,
                          aDefaultText, aResult) {
        return this.mPrompter.prompt(aDialogTitle, aText, aPasswordRealm,
                                     aSavePassword, aDefaultText, aResult);
    },

    getPasswordInfo: function capGPI(aPasswordRealm) {
        var username;
        var password;
        var found = false;
        var passwordManager = Components.classes["@mozilla.org/passwordmanager;1"]
                                        .getService(Components.interfaces.nsIPasswordManager);
        var pwenum = passwordManager.enumerator;
        // step through each password in the password manager until we find the one we want:
        while (pwenum.hasMoreElements()) {
            try {
                var pass = pwenum.getNext().QueryInterface(Components.interfaces.nsIPassword);
                if (pass.host == aPasswordRealm) {
                     // found it!
                     username = pass.user;
                     password = pass.password;
                     found = true;
                     break;
                }
            } catch (ex) {
                // don't do anything here, ignore the password that could not
                // be read
            }
        }
        return {found: found, username: username, password: password};
    },

    promptUsernameAndPassword: function capPUAP(aDialogTitle, aText,
                                                aPasswordRealm,aSavePassword,
                                                aUser, aPwd) {
        var pw;
        if (!this.mTriedStoredPassword) {
            pw = this.getPasswordInfo(aPasswordRealm);
        }

        if (pw && pw.found) {
            this.mTriedStoredPassword = true;
            aUser.value = pw.username;
            aPwd.value = pw.password;
            return true;
        } else {
            return this.mPrompter.promptUsernameAndPassword(aDialogTitle, aText,
                                                            aPasswordRealm,
                                                            aSavePassword,
                                                            aUser, aPwd);
        }
    },

    // promptAuth is needed/used on trunk only
    promptAuth: function capPA(aChannel, aLevel, aAuthInfo) {
        // need to match the way the password manager stores host/realm
        var hostRealm = aChannel.URI.host + ":" + aChannel.URI.port + " (" +
                        aAuthInfo.realm + ")";
        var pw;
        if (!this.mTriedStoredPassword) {
            pw = this.getPasswordInfo(hostRealm);
        }

        if (pw && pw.found) {
            this.mTriedStoredPassword = true;
            aAuthInfo.username = pw.username;
            aAuthInfo.password = pw.password;
            return true;
        } else {
            var prompter2 = 
                Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                          .getService(Components.interfaces.nsIPromptFactory)
                          .getPrompt(null, Components.interfaces.nsIAuthPrompt2);
            return prompter2.promptAuth(aChannel, aLevel, aAuthInfo);
        }
    },

    promptPassword: function capPP(aDialogTitle, aText, aPasswordRealm,
                             aSavePassword, aPwd) {
        var found = false;
        var pw;
        if (!this.mTriedStoredPassword) {
            pw = this.getPasswordInfo(aPasswordRealm);
        }

        if (pw && pw.found) {
            this.mTriedStoredPassword = true;
            aPwd.value = pw.password;
            return true;
        } else {
            return this.mPrompter.promptPassword(aDialogTitle, aText,
                                                 aPasswordRealm, aSavePassword,
                                                 aPwd);
        }
    }
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
 * Returns the start date of an item, ie either an event's start date or a task's entry date.
 */
function calGetStartDate(aItem)
{
    return (isEvent(aItem) ? aItem.startDate : aItem.entryDate);
}

/**
 * Returns the end date of an item, ie either an event's end date or a task's due date.
 */
function calGetEndDate(aItem)
{
    return (isEvent(aItem) ? aItem.endDate : aItem.dueDate);
}

/**
 *  Delete the current selected items with focus from the unifinder list
 */
function deleteEventCommand(doNotConfirm)
{
    var selectedItems = currentView().getSelectedItems({});
    deleteItems(selectedItems,doNotConfirm);
}

/**
 *  This is called from the unifinder's delete command
 */
function deleteItems(selectedItems, doNotConfirm)
{
    if (!selectedItems) {
        return;
    }

    startBatchTransaction();
    for (var i in selectedItems) {
        var aOccurrence = selectedItems[i];
        if (aOccurrence.parentItem != aOccurrence) {
            var event = aOccurrence.parentItem.clone();
            event.recurrenceInfo.removeOccurrenceAt(aOccurrence.recurrenceId);
            doTransaction('modify', event, event.calendar, aOccurrence.parentItem, null);
        } else {
            doTransaction('delete', aOccurrence, aOccurrence.calendar, null, null);
        }
    }
    endBatchTransaction();
}

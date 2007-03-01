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

/* Shortcut to the calendar-manager service */
function getCalendarManager() {
    return Cc["@mozilla.org/calendar/manager;1"].
           getService(Ci.calICalendarManager);
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
    const nsIPrefBranch = Ci.nsIPrefBranch;
    const prefB = Cc["@mozilla.org/preferences-service;1"].
                  getService(nsIPrefBranch);

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
    const nsIPrefBranch = Ci.nsIPrefBranch;
    const prefB = Cc["@mozilla.org/preferences-service;1"].
                  getService(nsIPrefBranch);
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
 * Gets the value of a string in a .properties file
 *
 * @param aBundleName  the name of the properties file.  It is assumed that the
 *                     file lives in chrome://calendar/locale/
 * @param aStringName the name of the string within the properties file
 */
function calGetString(aBundleName, aStringName) {
    var sbs = Cc["@mozilla.org/intl/stringbundle;1"].
              getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/"+aBundleName+".properties");
    return props.GetStringFromName(aStringName);
}

/** Returns a best effort at making a UUID.  If we have the UUIDGenerator
 * service available, we'll use that.  If we're somewhere where it doesn't
 * exist, like Lightning in TB 1.5, we'll just use the current time.
 */
function getUUID() {
    if ("@mozilla.org/uuid-generator;1" in Cc) {
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

/**
 * debug code
 */

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

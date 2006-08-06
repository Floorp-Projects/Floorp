/* -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2006 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Daniel Boelzle (daniel.boelzle@sun.com)
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function logMessage( context, msg )
{
    if (LOG_LEVEL > 0) {
        var now = getTime();
        if (LOG_TIMEZONE != null)
            now = now.getInTimezone(LOG_TIMEZONE);
        var str = ("\n### WCAP log " + now + "\n### [" + context + "]\n### " +
                   (msg ? msg : ""));
        getConsoleService().logStringMessage( str );
        str += "\n\n";
        dump( str );
        if (LOG_FILE_STREAM != null) {
            try {
                // xxx todo?
                // assuming ANSI chars here, for logging sufficient:
                LOG_FILE_STREAM.write( str, str.length );
            }
            catch (exc) { // catching any io errors here:
                var err = ("error writing log file: " + exc);
                Components.utils.reportError( exc );
                getConsoleService().logStringMessage( err );
                dump( err  + "\n\n" );
            }
        }
        return str;
    }
    else
        return msg;
}

// late-init service accessors:

var g_consoleService = null;
function getConsoleService()
{
    if (g_consoleService == null) {
        g_consoleService = Components.classes["@mozilla.org/consoleservice;1"]
                           .getService(Components.interfaces.nsIConsoleService);
    }
    return g_consoleService;
}

var g_windowWatcher = null;
function getWindowWatcher()
{
    if (g_windowWatcher == null) {
        g_windowWatcher =
            Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
            .getService(Components.interfaces.nsIWindowWatcher);
    }
    return g_windowWatcher;
}

var g_ioService = null;
function getIoService()
{
    if (g_ioService == null) {
        g_ioService = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
    }
    return g_ioService;
}

var g_icsService = null;
function getIcsService()
{
    if (g_icsService == null) {
        g_icsService = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);
    }
    return g_icsService;
}

var g_domParser = null;
function getDomParser()
{
    if (g_domParser == null) {
        g_domParser = Components.classes["@mozilla.org/xmlextras/domparser;1"]
                      .getService(Components.interfaces.nsIDOMParser);
    }
    return g_domParser;
}

var g_calendarManager = null;
function getCalendarManager()
{
    if (g_calendarManager == null) {
        g_calendarManager =
            Components.classes["@mozilla.org/calendar/manager;1"]
            .getService(Components.interfaces.calICalendarManager);
    }
    return g_calendarManager;
};

var g_wcapBundle = null;
function getWcapBundle()
{
    if (g_wcapBundle == null) {
        var stringBundleService =
            Components.classes["@mozilla.org/intl/stringbundle;1"]
            .getService(Components.interfaces.nsIStringBundleService);
        g_wcapBundle = stringBundleService.createBundle(
            "chrome://calendar/locale/wcap.properties" );
    }
    return g_wcapBundle;
}

function isEvent( item )
{
    return (item instanceof Components.interfaces.calIEvent);
}

function forEachIcalComponent( icalRootComp, componentType, func, maxResult )
{
    var itemCount = 0;
    // libical returns the vcalendar component if there is just
    // one vcalendar. If there are multiple vcalendars, it returns
    // an xroot component, with those vcalendar childs. We need to
    // handle both.
    for ( var calComp = (icalRootComp.componentType == "VCALENDAR"
                         ? icalRootComp
                         : icalRootComp.getFirstSubcomponent("VCALENDAR"));
          calComp != null && (!maxResult || itemCount < maxResult);
          calComp = icalRootComp.getNextSubcomponent("VCALENDAR") )
    {
        for ( var subComp = calComp.getFirstSubcomponent(componentType);
              subComp != null && (!maxResult || itemCount < maxResult);
              subComp = calComp.getNextSubcomponent(componentType) )
        {
            func( subComp );
            ++itemCount;
        }
    }
}

function trimString( str )
{
    return str.replace( /(^\s+|\s+$)/g, "" );
}

function getTime()
{
    var ret = new CalDateTime();
    ret.jsDate = new Date();
    return ret;
}

function getIcalUTC( dt )
{
    if (!dt)
        return "0";
    else {
        var dtz = dt.timezone;
        if (dtz == "UTC" || dtz == "floating")
            return dt.icalString;
        else
            return dt.getInTimezone("UTC").icalString;
    }
}

function getDatetimeFromIcalProp( prop )
{
    if (!prop)
        return null;
    var val = prop.valueAsIcalString;
    if (val.length == 0 || val == "0")
        return null;
    // assuming timezone is known:
    var dt = new CalDateTime();
    dt.icalString = val;
//     dt.makeImmutable();
    return dt;
}

function getPref(prefName, defaultValue)
{
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(nsIPrefBranch);
    try {
        switch (prefBranch.getPrefType(prefName)) {
        case nsIPrefBranch.PREF_BOOL:
            return prefBranch.getBoolPref(prefName);
        case nsIPrefBranch.PREF_INT:
            return prefBranch.getIntPref(prefName);
        case nsIPrefBranch.PREF_STRING:
            return prefBranch.getCharPref(prefName);
        default:
            return defaultValue;
        }
    }
    catch (exc) {
        return defaultValue;
    }
}

function setPref(prefName, value)
{
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(nsIPrefBranch);
    switch (typeof(value)) {
    case "boolean":
        prefBranch.setBoolPref(prefName, value);
        break;
    case "number":
        prefBranch.setIntPref(prefName, value);
        break;
    case "string":
        prefBranch.setCharPref(prefName, value);
        break;
    default:
        throw new Error("unsupported pref value: " + typeof(value));
    }
}


//
// init code for globals, prefs:
//

// ctors:
var CalEvent = new Components.Constructor(
    "@mozilla.org/calendar/event;1", "calIEvent" );
var CalTodo = new Components.Constructor(
    "@mozilla.org/calendar/todo;1", "calITodo" );
var CalDateTime = new Components.Constructor(
    "@mozilla.org/calendar/datetime;1", "calIDateTime" );
var XmlHttpRequest = new Components.Constructor(
    "@mozilla.org/xmlextras/xmlhttprequest;1", "nsIXMLHttpRequest" );

// global preferences:

// caching: off|memory|storage:
var CACHE = "off";
// denotes where to host local storage calendar(s)
var CACHE_DIR = null;

// logging:
#expand var LOG_LEVEL = __LOG_LEVEL__;
var LOG_TIMEZONE = getPref("calendar.timezone.local", null);
var LOG_FILE_STREAM = null;

try {
    var logLevel = getPref("calendar.wcap.log_level", null);
    if (logLevel == null) { // log_level pref undefined:
        if (getPref("calendar.debug.log", false))
            logLevel = 1; // at least basic logging when calendar.debug.log
    }
    if (logLevel > LOG_LEVEL) {
        LOG_LEVEL = logLevel;
    }
    
    if (LOG_LEVEL > 0) {
        var logFileName = getPref("calendar.wcap.log_file", null);
        if (logFileName != null) {
            // set up file:
            var logFile = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);
            logFile.initWithPath( logFileName );
            // create output stream:
            var logFileStream = Components.classes[
                "@mozilla.org/network/file-output-stream;1"]
                .createInstance(Components.interfaces.nsIFileOutputStream);
            logFileStream.init(
                logFile,
                0x02 /* PR_WRONLY */ |
                0x08 /* PR_CREATE_FILE */ |
                0x10 /* PR_APPEND */,
                0700 /* read, write, execute/search by owner */,
                0 /* unused */ );
            LOG_FILE_STREAM = logFileStream;
        }
        logMessage( "init sequence",
                    "################################# NEW LOG " +
                    "#################################" );
    }
    
    // init cache dir directory:
    CACHE = getPref("calendar.wcap.cache", "off" /* xxx todo */ );
    logMessage( "calendar.wcap.cache", CACHE );
    if (CACHE == "storage") {
        var cacheDir = null;
        var sCacheDir = getPref("calendar.wcap.cache_dir", null);
        if (sCacheDir != null) {
            cacheDir = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
            cacheDir.initWithPath( sCacheDir );
        }
        else { // not found: default to wcap/ directory in profile
            var dirService = Components.classes[
                "@mozilla.org/file/directory_service;1"]
                .getService(Components.interfaces.nsIProperties);
            cacheDir = dirService.get(
                "ProfD", Components.interfaces.nsILocalFile );
            cacheDir.append( "wcap" );
        }
        CACHE_DIR = cacheDir;
        logMessage( "calendar.wcap.cache_dir", CACHE_DIR.path );
        if (!CACHE_DIR.exists()) {
            CACHE_DIR.create( Components.interfaces.nsIFile.DIRECTORY_TYPE,
                              0700 /* read, write, execute/search by owner */ );
        }
    }
}
catch (exc) {
    logMessage( "error in init sequence", exc );
}

// some string resources:
var g_privateItemTitle = getWcapBundle().GetStringFromName(
    "privateItem.title.text");
var g_confidentialItemTitle = getWcapBundle().GetStringFromName(
    "confidentialItem.title.text");
var g_busyItemTitle = getWcapBundle().GetStringFromName(
    "busyItem.title.text");
var g_busyPhantomItemUuidPrefix = ("PHANTOM_uuid" + getTime().icalString);


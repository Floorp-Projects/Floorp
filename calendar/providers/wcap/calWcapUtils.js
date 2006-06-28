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

// globals:

// ctors:
var CalEvent;
var CalTodo;
var CalDateTime;
var XmlHttpRequest;

// preferences:

// memory|storage:
var CACHE = "off"; // xxx todo: off by default for now

// denotes where to host local storage calendar(s)
var CACHE_DIR = null;

// logging:
#expand var LOG_LEVEL = __LOG_LEVEL__;
var LOG_TIMEZONE = null;
var LOG_FILE_STREAM = null;

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

function init()
{
    try {
        // ctors:
        CalEvent = new Components.Constructor(
            "@mozilla.org/calendar/event;1", "calIEvent" );
        CalTodo = new Components.Constructor(
            "@mozilla.org/calendar/todo;1", "calITodo" );
        CalDateTime = new Components.Constructor(
            "@mozilla.org/calendar/datetime;1", "calIDateTime" );
        XmlHttpRequest = new Components.Constructor(
            "@mozilla.org/xmlextras/xmlhttprequest;1", "nsIXMLHttpRequest" );
        
        var prefService =
            Components.classes["@mozilla.org/preferences-service;1"]
            .getService(Components.interfaces.nsIPrefService);
        var prefCalBranch = prefService.getBranch("calendar.");
        
        try {
            LOG_TIMEZONE = prefCalBranch.getCharPref("timezone.local");
        }
        catch (exc) {
        }
        
        var logLevel = 0;
        try {
            logLevel = prefCalBranch.getIntPref( "wcap.log_level" );
        }
        catch (exc) {
        }
        if (logLevel > LOG_LEVEL) {
            LOG_LEVEL = logLevel;
        }
        
        if (LOG_LEVEL > 0) {
            try {
                var logFileName = prefCalBranch.getCharPref("wcap.log_file");
                if (logFileName != null) {
                    // set up file:
                    var logFile =
                        Components.classes["@mozilla.org/file/local;1"]
                        .createInstance(Components.interfaces.nsILocalFile);
                    logFile.initWithPath( logFileName );
                    // create output stream:
                    var logFileStream = Components.classes[
                        "@mozilla.org/network/file-output-stream;1"]
                        .createInstance(
                            Components.interfaces.nsIFileOutputStream);
                    logFileStream.init(
                        logFile,
                        0x02 /* PR_WRONLY */ |
                        0x08 /* PR_CREATE_FILE */ |
                        0x10 /* PR_APPEND */,
                        0700 /* read, write, execute/search by owner */,
                        0 /* unused */ );
                    LOG_FILE_STREAM = logFileStream;
                }
            }
            catch (exc) {
            }
            logMessage( "init()",
                        "################################# NEW LOG " +
                        "#################################" );
        }
        
        // init cache dir directory:
        try {
            CACHE = prefCalBranch.getCharPref( "wcap.cache" );
        }
        catch (exc) {
        }
        logMessage( "calendar.wcap.cache", CACHE );
        if (CACHE == "storage") {
            var cacheDir = null;
            try {
                var sCacheDir = prefCalBranch.getCharPref( "wcap.cache_dir" );
                cacheDir = Components.classes["@mozilla.org/file/local;1"]
                    .createInstance(Components.interfaces.nsILocalFile);
                cacheDir.initWithPath( sCacheDir );
            }
            catch (exc) { // not found: default to wcap/ directory in profile
                var dirService = Components.classes[
                    "@mozilla.org/file/directory_service;1"]
                    .getService(Components.interfaces.nsIProperties);
                cacheDir = dirService.get(
                    "ProfD", Components.interfaces.nsILocalFile );
                cacheDir.append( "wcap" );
            }
            CACHE_DIR = cacheDir;
            logMessage( "calendar.wcap.cache_dir", CACHE_DIR.path );
            if (! CACHE_DIR.exists()) {
                CACHE_DIR.create(
                    Components.interfaces.nsIFile.DIRECTORY_TYPE,
                    0700 /* read, write, execute/search by owner */ );
            }
        }
    }
    catch (exc) {
        logMessage( "error in init()", exc );
    }
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

var g_bundle = null;
function getBundle()
{
    if (g_bundle == null) {
        var stringBundleService =
            Components.classes["@mozilla.org/intl/stringbundle;1"]
            .getService(Components.interfaces.nsIStringBundleService);
        g_bundle = stringBundleService.createBundle(
            "chrome://calendar/locale/wcap.properties" );
    }
    return g_bundle;
}

function isEvent( item )
{
    var bRet = (item instanceof Components.interfaces.calIEvent);
    if (!bRet && !(item instanceof Components.interfaces.calITodo)) {
        throw new Error("item is no calIEvent nor calITodo!");
    }
    return bRet;
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

function getTime()
{
    var ret = new CalDateTime();
    ret.jsDate = new Date();
    return ret;
}

function getIcalUTC( dt )
{
    if (! dt)
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
    if (! prop)
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


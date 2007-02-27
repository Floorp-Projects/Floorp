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

var g_bShutdown = false;
var g_logTimezone = null;
var g_logFilestream = null;
var g_logPrefObserver = null;

function initLogging()
{
    g_logTimezone = getPref("calendar.timezone.local", null);
    
    if (g_logFilestream) {
        try {
            g_logFilestream.close();
        }
        catch (exc) {
        }
        g_logFilestream = null;
    }
    
    LOG_LEVEL = getPref("calendar.wcap.log_level", 0);
    if (LOG_LEVEL < 1 && getPref("calendar.debug.log", false))
        LOG_LEVEL = 1; // at least basic logging when calendar.debug.log is set
    
    if (LOG_LEVEL > 0) {
        var logFileName = getPref("calendar.wcap.log_file", null);
        if (logFileName) {
            try {
                // set up file:
                var logFile = Components.classes["@mozilla.org/file/local;1"]
                                        .createInstance(Components.interfaces.nsILocalFile);
                logFile.initWithPath(logFileName);
                // create output stream:
                var logFileStream =
                    Components.classes["@mozilla.org/network/file-output-stream;1"]
                              .createInstance(Components.interfaces.nsIFileOutputStream);
                logFileStream.init(
                    logFile,
                    0x02 /* PR_WRONLY */ |
                    0x08 /* PR_CREATE_FILE */ |
                    0x10 /* PR_APPEND */,
                    0700 /* read, write, execute/search by owner */,
                    0 /* unused */);
                g_logFilestream = logFileStream;
            }
            catch (exc) {
                logError(exc, "init logging");
            }
        }
        log("################################# NEW LOG #################################",
            "init logging");
    }
    if (!g_logPrefObserver) {
        g_logPrefObserver = { // nsIObserver:
            observe: function logPrefObserver_observe(subject, topic, data) {
                if (topic == "nsPref:changed") {
                    switch (data) {
                    case "calendar.wcap.log_level":
                    case "calendar.wcap.log_file":
                    case "calendar.debug.log":
                        initLogging();
                        break;
                    }
                }
            }
        };
        var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                                   .getService(Components.interfaces.nsIPrefBranch2);
        prefBranch.addObserver("calendar.wcap.log_level", g_logPrefObserver, false);
        prefBranch.addObserver("calendar.wcap.log_file", g_logPrefObserver, false);
        prefBranch.addObserver("calendar.debug.log", g_logPrefObserver, false);
        
        var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                        .getService(Components.interfaces.nsIObserverService);
        var appObserver = { // nsIObserver:
            observe: function app_observe(subject, topic, data) {
                if (topic == "quit-application")
                    prefBranch.removeObserver("calendar.", g_logPrefObserver);
            }
        };
        observerService.addObserver(appObserver, "quit-application", false);
    }
}

function log(msg, context, bForce)
{
    if (bForce || LOG_LEVEL > 0) {
        var ret = "";
        if (context)
            ret += ("[" + context + "]");
        if (msg) {
            if (ret.length > 0)
                ret += "\n";
            ret += msg;
        }
        var now = getTime();
        if (now && g_logTimezone)
            now = now.getInTimezone(g_logTimezone);
        var str = ("### WCAP log entry: " + now + "\n" + ret);
        getConsoleService().logStringMessage(str);
        str = ("\n" + str + "\n");
        dump(str);
        if (g_logFilestream) {
            try {
                // xxx todo?
                // assuming ANSI chars here, for logging sufficient:
                g_logFilestream.write(str, str.length);
            }
            catch (exc) { // catching any io errors here:
                var err = ("error writing log file: " + errorToString(exc));
                Components.utils.reportError(exc);
                getConsoleService().logStringMessage(err);
                dump(err  + "\n\n");
            }
        }
        return ret;
    }
    else
        return msg;
}

function logError(err, context)
{
    var msg = errorToString(err);
    Components.utils.reportError( log("error: " + msg, context, true) );
    return msg;
}

// late-inited service accessors:

var g_consoleService = null;
function getConsoleService() {
    if (!g_consoleService) {
        g_consoleService =
            Components.classes["@mozilla.org/consoleservice;1"]
                      .getService(Components.interfaces.nsIConsoleService);
    }
    return g_consoleService;
}

var g_windowWatcher = null;
function getWindowWatcher() {
    if (!g_windowWatcher) {
        g_windowWatcher =
            Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                      .getService(Components.interfaces.nsIWindowWatcher);
    }
    return g_windowWatcher;
}

var g_icsService = null;
function getIcsService() {
    if (!g_icsService) {
        g_icsService =
            Components.classes["@mozilla.org/calendar/ics-service;1"]
                      .getService(Components.interfaces.calIICSService);
    }
    return g_icsService;
}

var g_domParser = null;
function getDomParser() {
    if (!g_domParser) {
        g_domParser =
            Components.classes["@mozilla.org/xmlextras/domparser;1"]
                      .getService(Components.interfaces.nsIDOMParser);
    }
    return g_domParser;
}

var g_calendarManager = null;
function getCalendarManager() {
    if (!g_calendarManager) {
        g_calendarManager =
            Components.classes["@mozilla.org/calendar/manager;1"]
                      .getService(Components.interfaces.calICalendarManager);
    }
    return g_calendarManager;
};

var g_wcapBundle = null;
function getWcapBundle() {
    if (!g_wcapBundle) {
        var stringBundleService =
            Components.classes["@mozilla.org/intl/stringbundle;1"]
                      .getService(Components.interfaces.nsIStringBundleService);
        g_wcapBundle = stringBundleService.createBundle(
            "chrome://calendar/locale/wcap.properties");
    }
    return g_wcapBundle;
}

function subClass(subCtor, baseCtor) {
    subCtor.prototype = new baseCtor();
    subCtor.prototype.constructor = subCtor;
    subCtor.prototype.superClass = baseCtor;
}

function qiface(list, iid) {
    if (!list.some( function(i) { return i.equals(iid); } ))
        throw Components.results.NS_ERROR_NO_INTERFACE;
}

function isEvent(item) {
    return (item instanceof Components.interfaces.calIEvent);
}

function isParent(item) {
    if (item.id != item.parentItem.id) {
        throw new Components.Exception("proxy has different id than its parent!");
    }
    return (!item.recurrenceId);
}

function forEachIcalComponent(icalRootComp, componentType, func, maxResults)
{
    var itemCount = 0;
    // libical returns the vcalendar component if there is just
    // one vcalendar. If there are multiple vcalendars, it returns
    // an xroot component, with those vcalendar childs. We need to
    // handle both.
    for ( var calComp = (icalRootComp.componentType == "VCALENDAR"
                         ? icalRootComp
                         : icalRootComp.getFirstSubcomponent("VCALENDAR"));
          calComp != null && (!maxResults || itemCount < maxResults);
          calComp = icalRootComp.getNextSubcomponent("VCALENDAR") )
    {
        for ( var subComp = calComp.getFirstSubcomponent(componentType);
              subComp != null && (!maxResults || itemCount < maxResults);
              subComp = calComp.getNextSubcomponent(componentType) )
        {
            func( subComp );
            ++itemCount;
        }
    }
}

function filterXmlNodes(name, rootNode)
{
    var ret = [];
    if (rootNode) {
        var nodeList = rootNode.getElementsByTagName(name);
        for (var i = 0; i < nodeList.length; ++i) {
            var node = nodeList.item(i);
            ret.push( trimString(node.textContent) );
        }
    }
    return ret;
}

function trimString(str) {
    return str.replace(/(^\s+|\s+$)/g, "");
}

function getTime() {
    if (g_bShutdown)
        return null;
    var ret = new CalDateTime();
    ret.jsDate = new Date();
    return ret;
}

function getIcalUTC(dt) {
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

function getDatetimeFromIcalString(val) {
    if (!val || val.length == 0 || val == "0")
        return null;
    // assuming timezone is known:
    var dt = new CalDateTime();
    dt.icalString = val;
    if (LOG_LEVEL > 1) {
        var dt_ = dt.clone();
        dt_.normalize();
        if (dt.icalString != val || dt_.icalString != val) {
            logError(dt.icalString + " vs. " + val, "date-time error");
            logError(dt_.icalString + " vs. " + val, "date-time error");
            debugger;
        }
    }
    return dt;
}

function getDatetimeFromIcalProp(prop) {
    if (!prop)
        return null;
    return getDatetimeFromIcalString(prop.valueAsIcalString);
}

function getPref(prefName, defaultValue) {
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                               .getService(nsIPrefBranch);
    var ret;
    switch (prefBranch.getPrefType(prefName)) {
    case nsIPrefBranch.PREF_BOOL:
        ret = prefBranch.getBoolPref(prefName);
        break;
    case nsIPrefBranch.PREF_INT:
        ret = prefBranch.getIntPref(prefName);
        break;
    case nsIPrefBranch.PREF_STRING:
        ret = prefBranch.getCharPref(prefName);
        break;
    default:
        ret = defaultValue;
        break;
    }
    log(ret, "getPref(): prefName=" + prefName);
    return ret;
}

function setPref(prefName, value) {
    log(value, "setPref(): prefName=" + prefName);
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
        throw new Components.Exception("unsupported pref value: " +
                                       typeof(value));
    }
}


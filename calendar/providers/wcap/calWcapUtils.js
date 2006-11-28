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

function getResultCode( exc )
{
    return (exc instanceof Components.interfaces.nsIException
            ? exc.result : exc);
}

function testResultCode( exc, rc )
{
    return (getResultCode(exc) == rc);
}

function isEvent( item )
{
    return (item instanceof Components.interfaces.calIEvent);
}

function isParent( item )
{
    if (item.id != item.parentItem.id) {
        throw new Components.Exception(
            "proxy has different id than its parent!");
    }
    return (!item.recurrenceId);
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

function filterCalProps(propName, calProps)
{
    var ret = [];
    if (calProps) {
        var nodeList = calProps.getElementsByTagName(propName);
        for ( var i = 0; i < nodeList.length; ++i ) {
            var node = nodeList.item(i);
            ret.push( trimString(node.textContent) );
        }
    }
    return ret;
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

function getDatetimeFromIcalString( val )
{
    if (!val || val.length == 0 || val == "0")
        return null;
    // assuming timezone is known:
    var dt = new CalDateTime();
    dt.icalString = val;
//     if (dt.icalString != val)
//         logMessage("date-time error", dt.icalString + " vs. " + val);
    return dt;
}

function getDatetimeFromIcalProp( prop )
{
    if (!prop)
        return null;
    return getDatetimeFromIcalString(prop.valueAsIcalString);
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
        throw new Components.Exception("unsupported pref value: " +
                                       typeof(value));
    }
}

function AsyncQueue()
{
    this.wrappedJSObject = this;
    this.m_queue = [];
}
AsyncQueue.prototype = {
    m_queue: null,
    
    reset:
    function()
    {
        this.m_queue = [];
    },
    
    m_proxy: null,
    get proxy() {
        if (!this.m_proxy) {
            var eventTarget = null;
            try {
                var eventQueueService =
                    Components.classes["@mozilla.org/event-queue-service;1"]
                    .getService(Components.interfaces.nsIEventQueueService);
                eventTarget = eventQueueService.createThreadEventQueue(
                    Components.classes["@mozilla.org/thread;1"]
                    .createInstance(Components.interfaces.nsIThread), true);
            }
            catch (exc) { // eventQueue has vanished on trunk:
                var threadManager =
                    Components.classes["@mozilla.org/thread-manager;1"]
                    .getService(Components.interfaces.nsIThreadManager);
                eventTarget = threadManager.newThread(0);
            }
            var proxyMgr = Components.classes["@mozilla.org/xpcomproxy;1"]
                .getService(Components.interfaces.nsIProxyObjectManager);
            this.m_proxy = proxyMgr.getProxyForObject(
                eventTarget, Components.interfaces.nsIRunnable, this,
                Components.interfaces.nsIProxyObjectManager.INVOKE_ASYNC );
        }
        return this.m_proxy;
    },
    
    queuedExec:
    function(func)
    {
        this.m_queue.push(func);
        if (LOG_LEVEL > 1)
            logMessage("enqueued: q=" + this.m_queue.length);
        if (this.m_queue.length == 1) {
            this.proxy.run(); // empty queue
        }
    },
    
    m_ifaces: [ Components.interfaces.nsIRunnable,
                Components.interfaces.nsIClassInfo,
                Components.interfaces.nsISupports ],
    
    // nsISupports:
    QueryInterface:
    function( iid )
    {
        for each ( var iface in this.m_ifaces ) {
            if (iid.equals( iface ))
                return this;
        }
        throw Components.results.NS_ERROR_NO_INTERFACE;
    },
    
    // nsIClassInfo:
    getInterfaces: function( count ) {
        count.value = this.m_ifaces.length;
        return this.m_ifaces;
    },
    
    get classDescription() {
        return "Async Queue";
    },
    get contractID() {
        return "@mozilla.org/calendar/calendar/wcap/async-queue;1";
    },
    get classID() {
        return Components.ID("{C50F7442-C43E-43f6-AA3F-1ADB87E7A962}");
    },
    getHelperForLanguage: function( language ) { return null; },
    implementationLanguage:
    Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0,
    
    // nsIRunnable:
    run:
    function() {
        while (this.m_queue.length > 0) {
            if (LOG_LEVEL > 1)
                logMessage("queue exec: " + this.m_queue.length);
            try {
                this.m_queue[0]();
            }
            catch (exc) { // swallow all exceptions,
                          // they does not belong to this call
                debugger;
                var msg = errorToString(exc);
                Components.utils.reportError(
                    logMessage("error: " + msg, "swallowed exception") );
            }
            // don't remove element unless func has been executed:
            this.m_queue.shift();
            if (LOG_LEVEL > 1)
                logMessage("dequeued: " + this.m_queue.length);
        }
    }
};

function makeQueuedCall(asyncQueue, obj, func)
{
    return function() {
        var args = [];
        for ( var i = 0; i < arguments.length; ++i )
            args.push(arguments[i]);
        asyncQueue.queuedExec( function() { func.apply(obj, args); } );
    }
}

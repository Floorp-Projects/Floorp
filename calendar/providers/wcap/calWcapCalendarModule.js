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

//
// init code for globals, prefs:
//

// ctors:
var CalEvent;
var CalTodo;
var CalDateTime;
var XmlHttpRequest;

// some string resources:
var g_privateItemTitle;
var g_confidentialItemTitle;
var g_busyItemTitle;
var g_busyPhantomItemUuidPrefix;

// global preferences:
// caching: off|memory|storage:
var CACHE = "off";
// denotes where to host local storage calendar(s)
var CACHE_DIR = null;

// logging:
#expand var LOG_LEVEL = __LOG_LEVEL__;
var LOG_TIMEZONE = null;
var LOG_FILE_STREAM = null;

// whether alarms are by default turned on/off:
var SUPPRESS_ALARMS = true;

function initWcapProvider()
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
        
        // some string resources:
        g_privateItemTitle = getWcapBundle().GetStringFromName(
            "privateItem.title.text");
        g_confidentialItemTitle = getWcapBundle().GetStringFromName(
            "confidentialItem.title.text");
        g_busyItemTitle = getWcapBundle().GetStringFromName(
            "busyItem.title.text");
        g_busyPhantomItemUuidPrefix = ("PHANTOM_uuid" + getTime().icalString);
        
        LOG_TIMEZONE = getPref("calendar.timezone.local", null);
        
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
                var logFile =
                    Components.classes["@mozilla.org/file/local;1"]
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
        
        SUPPRESS_ALARMS = getPref("calendar.wcap.suppress_alarms", true);
        if (SUPPRESS_ALARMS)
            logMessage( "calendar.wcap.suppress_alarms", SUPPRESS_ALARMS );
        
        // init cache dir directory:
        CACHE = getPref("calendar.wcap.cache", "off");
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
                CACHE_DIR.create(
                    Components.interfaces.nsIFile.DIRECTORY_TYPE,
                    0700 /* read, write, execute/search by owner */ );
            }
        }
    }
    catch (exc) {
        logMessage( "error in init sequence", exc );
    }
}

var calWcapCalendarModule = {
    
    WcapCalendarInfo: {
        classDescription: "Sun Java System Calendar Server WCAP Provider",
        contractID: "@mozilla.org/calendar/calendar;1?type=wcap",
        classID: Components.ID("{CF4D93E5-AF79-451a-95F3-109055B32EF0}")
    },
    
    WcapSessionInfo: {
        classDescription: "Sun Java System Calendar Server WCAP Session",
        contractID: "@mozilla.org/calendar/session;1?type=wcap",
        classID: Components.ID("{CBF803FD-4469-4999-AE39-367AF1C7B077}")
    },
    
    registerSelf:
    function( compMgr, fileSpec, location, type )
    {
        compMgr = compMgr.QueryInterface(
            Components.interfaces.nsIComponentRegistrar );
        compMgr.registerFactoryLocation(
            this.WcapCalendarInfo.classID,
            this.WcapCalendarInfo.classDescription,
            this.WcapCalendarInfo.contractID,
            fileSpec, location, type );
        compMgr.registerFactoryLocation(
            this.WcapSessionInfo.classID,
            this.WcapSessionInfo.classDescription,
            this.WcapSessionInfo.contractID,
            fileSpec, location, type );
    },
    
    m_scriptsLoaded: false,
    getClassObject:
    function( compMgr, cid, iid )
    {
        if (!this.m_scriptsLoaded) {
            // loading extra scripts from ../js:
            const scripts = [ "calWcapUtils.js", "calWcapErrors.js",
                              "calWcapRequest.js", "calWcapSession.js",
                              "calWcapCalendar.js", "calWcapCalendarItems.js",
                              "calWcapCachedCalendar.js" ];
            var scriptLoader =
                Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                .createInstance(Components.interfaces.mozIJSSubScriptLoader);
            var ioService =
                Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
            var baseDir = __LOCATION__.parent.parent;
            baseDir.append("js");
            for each ( var script in scripts ) {
                var scriptFile = baseDir.clone();
                scriptFile.append(script);
                scriptLoader.loadSubScript(
                    ioService.newFileURI(scriptFile).spec, null );
            }
            initWcapProvider();
            this.m_scriptsLoaded = true;
        }
        
        if (!cid.equals( calWcapCalendar.prototype.classID ))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        if (!iid.equals( Components.interfaces.nsIFactory ))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        
        return {
            createInstance:
            function( outer, iid ) {
                if (outer != null)
                    throw Components.results.NS_ERROR_NO_AGGREGATION;
                var session = new calWcapSession();
                var cal = createWcapCalendar(
                    null /* calId: null indicates default calendar */,
                    session );
                session.defaultCalendar = cal;
                return cal.QueryInterface( iid );
            }
        };
    },
    
    canUnload: function( compMgr ) { return true; }
};

/** module export */
function NSGetModule( compMgr, fileSpec ) {
    return calWcapCalendarModule;
}


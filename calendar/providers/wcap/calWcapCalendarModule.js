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

// constants:
const NS_OK = Components.results.NS_OK;
const nsIException = Components.interfaces.nsIException;
const calIWcapSession = Components.interfaces.calIWcapSession;
const calIWcapCalendar = Components.interfaces.calIWcapCalendar;
const calIWcapErrors = Components.interfaces.calIWcapErrors;
const calICalendar = Components.interfaces.calICalendar;
const calIItemBase = Components.interfaces.calIItemBase;
const calIOperationListener = Components.interfaces.calIOperationListener;

// ctors:
var CalEvent;
var CalTodo;
var CalDateTime;
var CalDuration;
var CalPeriod;
var Timer;

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

// caching the last data retrievals:
var CACHE_LAST_RESULTS = 4;
// timer secs for invalidation:
var CACHE_LAST_RESULTS_INVALIDATE = 120;

// logging:
var LOG_LEVEL = 0;

// whether alarms are turned on/off:
var SUPPRESS_ALARMS = false;

var g_ioService = null;
function getIoService() {
    if (!g_ioService) {
        g_ioService = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
    }
    return g_ioService;
}

function initWcapProvider()
{
    try {
        // xxx todo: hack
        // the master password prompt is currently not guarded against
        // multiple prompt; this initializes/raises the pw db at early stage.
        var passwordManager = Components.classes["@mozilla.org/passwordmanager;1"]
                                        .getService(Components.interfaces.nsIPasswordManager);
        var enumerator = passwordManager.enumerator;
        if (enumerator.hasMoreElements())
            enumerator.getNext(); // actually prompts...
    }
    catch (exc) {
    }
    
    try {        
        // ctors:
        CalEvent = new Components.Constructor("@mozilla.org/calendar/event;1",
                                              "calIEvent");
        CalTodo = new Components.Constructor("@mozilla.org/calendar/todo;1",
                                             "calITodo");
        CalDateTime = new Components.Constructor("@mozilla.org/calendar/datetime;1",
                                                 "calIDateTime");
        CalDuration = new Components.Constructor("@mozilla.org/calendar/duration;1",
                                                 "calIDuration");
        CalPeriod = new Components.Constructor("@mozilla.org/calendar/period;1",
                                               "calIPeriod");
        Timer = new Components.Constructor("@mozilla.org/timer;1",
                                           "nsITimer");
        
        initLogging();
        
        // some string resources:
        g_privateItemTitle = getWcapBundle().GetStringFromName("privateItem.title.text");
        g_confidentialItemTitle = getWcapBundle().GetStringFromName("confidentialItem.title.text");
        g_busyItemTitle = getWcapBundle().GetStringFromName("busyItem.title.text");
        g_busyPhantomItemUuidPrefix = ("PHANTOM_uuid" + getTime().icalString);
        
        SUPPRESS_ALARMS = getPref("calendar.wcap.suppress_alarms", true);
        
        CACHE_LAST_RESULTS = getPref("calendar.wcap.cache_last_results", 4);
        CACHE_LAST_RESULTS_INVALIDATE = getPref("calendar.wcap.cache_last_results_invalidate", 120);
        
        // init cache dir directory:
        CACHE = getPref("calendar.wcap.cache", "off");
        if (CACHE == "storage") {
            var cacheDir = null;
            var sCacheDir = getPref("calendar.wcap.cache_dir", null);
            if (sCacheDir != null) {
                cacheDir = Components.classes["@mozilla.org/file/local;1"]
                           .createInstance(Components.interfaces.nsILocalFile);
                cacheDir.initWithPath( sCacheDir );
            }
            else { // not found: default to wcap/ directory in profile
                var dirService = Components.classes["@mozilla.org/file/directory_service;1"]
                                           .getService(Components.interfaces.nsIProperties);
                cacheDir = dirService.get("ProfD", Components.interfaces.nsILocalFile);
                cacheDir.append("wcap");
            }
            CACHE_DIR = cacheDir;
            log(CACHE_DIR.path, "cache dir");
            if (!CACHE_DIR.exists()) {
                CACHE_DIR.create(Components.interfaces.nsIFile.DIRECTORY_TYPE,
                                 0700 /* read, write, execute/search by owner */);
            }
        }
    }
    catch (exc) {
        logError(exc, "error in init sequence");
    }
}

var calWcapCalendarFactory = { // nsIFactory:
    lockFactory: function calWcapCalendarFactory_lockFactory(lock) {},
    
    createInstance: function calWcapCalendarFactory_createInstance(outer, iid) {
        if (outer)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
        var session = new calWcapSession();
        return session.defaultCalendar.QueryInterface(iid);
    }
};

var calWcapCalendarModule = { // nsIModule:
    
    WcapCalendarInfo: {
        classDescription: "Sun Java System Calendar Server WCAP Provider",
        contractID: "@mozilla.org/calendar/calendar;1?type=wcap",
        classID: Components.ID("{CF4D93E5-AF79-451a-95F3-109055B32EF0}")
    },
    
    WcapSessionInfo: {
        classDescription: "Sun Java System Calendar Server WCAP Session",
        contractID: "@mozilla.org/calendar/wcap/session;1",
        classID: Components.ID("{CBF803FD-4469-4999-AE39-367AF1C7B077}")
    },
    
    registerSelf: function calWcapCalendarModule_registerSelf(compMgr, fileSpec, location, type)
    {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.WcapCalendarInfo.classID,
                                        this.WcapCalendarInfo.classDescription,
                                        this.WcapCalendarInfo.contractID,
                                        fileSpec, location, type);
        compMgr.registerFactoryLocation(this.WcapSessionInfo.classID,
                                        this.WcapSessionInfo.classDescription,
                                        this.WcapSessionInfo.contractID,
                                        fileSpec, location, type);
    },
    
    unregisterSelf: function calWcapCalendarModule_unregisterSelf(compMgr, fileSpec, location)
    {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.unregisterFactoryLocation(this.WcapCalendarInfo.classID, fileSpec);
        compMgr.unregisterFactoryLocation(this.WcapSessionInfo.classID, fileSpec);
    },
    
    m_scriptsLoaded: false,
    getClassObject: function calWcapCalendarModule_getClassObject(compMgr, cid, iid)
    {
        if (!this.m_scriptsLoaded) {
            // loading extra scripts from ../js:
            const scripts = [ "calWcapUtils.js", "calWcapErrors.js",
                              "calWcapRequest.js", "calWcapSession.js",
                              "calWcapCalendar.js", "calWcapCalendarItems.js" ];
            var scriptLoader =
                Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                          .createInstance(Components.interfaces.mozIJSSubScriptLoader);
            var ioService = getIoService();
            var baseDir = __LOCATION__.parent.parent;
            baseDir.append("js");
            for each (var script in scripts) {
                var scriptFile = baseDir.clone();
                scriptFile.append(script);
                scriptLoader.loadSubScript(ioService.newFileURI(scriptFile).spec, null);
            }
            initWcapProvider();
            this.m_scriptsLoaded = true;
        }
        
        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        if (!cid.equals(calWcapCalendar.prototype.classID))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        return calWcapCalendarFactory;
    },
    
    canUnload: function calWcapCalendarModule_canUnload(compMgr) { return true; }
};

/** module export */
function NSGetModule(compMgr, fileSpec) {
    return calWcapCalendarModule;
}


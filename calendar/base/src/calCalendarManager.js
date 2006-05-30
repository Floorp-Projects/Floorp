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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
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


const kStorageServiceContractID = "@mozilla.org/storage/service;1";
const kStorageServiceIID = Components.interfaces.mozIStorageService;

const kMozStorageStatementWrapperContractID = "@mozilla.org/storage/statement-wrapper;1";
const kMozStorageStatementWrapperIID = Components.interfaces.mozIStorageStatementWrapper;
var MozStorageStatementWrapper = null;

function createStatement (dbconn, sql) {
    var stmt = dbconn.createStatement(sql);
    var wrapper = MozStorageStatementWrapper();
    wrapper.initialize(stmt);
    return wrapper;
}

function onCalCalendarManagerLoad() {
    MozStorageStatementWrapper = new Components.Constructor(kMozStorageStatementWrapperContractID, kMozStorageStatementWrapperIID);
}

function calCalendarManager() {
    this.wrappedJSObject = this;
    this.initDB();
    this.mCache = {};
    this.setUpReadOnlyObservers();
}

function makeURI(uriString)
{
    var ioservice = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    return ioservice.newURI(uriString, null, null);
}

var calCalendarManagerClassInfo = {
    getInterfaces: function (count) {
        var ifaces = [
            Components.interfaces.nsISupports,
            Components.interfaces.calICalendarManager,
            Components.interfaces.nsIClassInfo
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function (language) {
        return null;
    },

    contractID: "@mozilla.org/calendar/manager;1",
    classDescription: "Calendar Manager",
    classID: Components.ID("{f42585e7-e736-4600-985d-9624c1c51992}"),
    implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0
};

calCalendarManager.prototype = {
    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.nsIClassInfo))
            return calCalendarManagerClassInfo;

        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calICalendarManager))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    // When a calendar fails, its onError doesn't point back to the calendar.
    // Therefore, we add a new announcer for each calendar to tell the user that
    // a specific calendar has failed.  The calendar itself is responsible for
    // putting itself in readonly mode.
    setUpReadOnlyObservers: function() {
        var calendars = this.getCalendars({});
        for each(calendar in calendars) {
            var newObserver = new errorAnnouncer(calendar);
            calendar.addObserver(newObserver.observer);
        }
    },

    initDB: function() {
        var sqlTables = { cal_calendars: "id INTEGER PRIMARY KEY, type STRING, uri STRING",
                          cal_calendars_prefs: "id INTEGER PRIMARY KEY, calendar INTEGER, name STRING, value STRING"
        };

        var dbService = Components.classes[kStorageServiceContractID].getService(kStorageServiceIID);

	if ( "getProfileStorage" in dbService ) {
	  // 1.8 branch
	  this.mDB = dbService.getProfileStorage("profile");
	} else {
	  // trunk 
	  this.mDB = dbService.openSpecialDatabase("profile");
	}

        for (table in sqlTables) {
            try {
                this.mDB.createTable(table, sqlTables[table]);
            } catch (ex) {
                dump("error creating table " + table + " -- probably already exists\n");
            }
        }

        this.mSelectCalendars = createStatement (
            this.mDB,
            "SELECT oid,* FROM cal_calendars");

        this.mFindCalendar = createStatement (
            this.mDB,
            "SELECT id FROM cal_calendars WHERE type = :type AND uri = :uri");

        this.mRegisterCalendar = createStatement (
            this.mDB,
            "INSERT INTO cal_calendars (type, uri) " +
            "VALUES (:type, :uri)"
            );

        this.mUnregisterCalendar = createStatement (
            this.mDB,
            "DELETE FROM cal_calendars WHERE id = :id"
            );

        this.mGetPref = createStatement (
            this.mDB,
            "SELECT value FROM cal_calendars_prefs WHERE calendar = :calendar AND name = :name");

        this.mDeletePref = createStatement (
            this.mDB,
            "DELETE FROM cal_calendars_prefs WHERE calendar = :calendar AND name = :name");

        this.mInsertPref = createStatement (
            this.mDB,
            "INSERT INTO cal_calendars_prefs (calendar, name, value) " +
            "VALUES (:calendar, :name, :value)");

        this.mDeletePrefs = createStatement (
            this.mDB,
            "DELETE FROM cal_calendars_prefs WHERE calendar = :calendar");

    },

    findCalendarID: function(calendar) {
        var stmt = this.mFindCalendar;
        stmt.reset();
        var pp = stmt.params;
        pp.type = calendar.type;
        pp.uri = calendar.uri.spec;

        var id = -1;
        if (stmt.step()) {
            id = stmt.row.id;
        }
        stmt.reset();
        return id;
    },
    
    notifyObservers: function(functionName, args) {
        function notify(obs) {
            try { obs[functionName].apply(obs, args);  }
            catch (e) { }
        }
        this.mObservers.forEach(notify);
    },

    /**
     * calICalendarManager interface
     */
    createCalendar: function(type, uri) {
        var calendar = Components.classes["@mozilla.org/calendar/calendar;1?type=" + type].createInstance(Components.interfaces.calICalendar);
        calendar.uri = uri;
        return calendar;
    },

    registerCalendar: function(calendar) {
        // bail if this calendar (or one that looks identical to it) is already registered
        if (this.findCalendarID(calendar) > 0) {
            dump ("registerCalendar: calendar already registered\n");
            throw Components.results.NS_ERROR_FAILURE;
        }

        // Add an observer to track readonly-mode triggers
        var newObserver = new errorAnnouncer(calendar);
        calendar.addObserver(newObserver.observer);

        var pp = this.mRegisterCalendar.params;
        pp.type = calendar.type;
        pp.uri = calendar.uri.spec;

        this.mRegisterCalendar.step();
        this.mRegisterCalendar.reset();
        //dump("adding [" + this.mDB.lastInsertRowID + "]\n");
        //this.mCache[this.mDB.lastInsertRowID] = calendar;
        this.mCache[this.findCalendarID(calendar)] = calendar;

        this.notifyObservers("onCalendarRegistered", [calendar]);
    },

    unregisterCalendar: function(calendar) {
        this.notifyObservers("onCalendarUnregistering", [calendar]);

        var calendarID = this.findCalendarID(calendar);

        var pp = this.mUnregisterCalendar.params;
        pp.id = calendarID;
        this.mUnregisterCalendar.step();
        this.mUnregisterCalendar.reset();

        // delete prefs for the calendar too
        pp = this.mDeletePrefs.params;
        pp.calendar = calendarID;
        this.mDeletePrefs.step();
        this.mDeletePrefs.reset();

        delete this.mCache[calendarID];
    },

    deleteCalendar: function(calendar) {
        /* check to see if calendar is unregistered first... */
        /* delete the calendar for good */
        
        this.notifyObservers("onCalendarDeleting", [calendar]);
    },

    getCalendars: function(count) {
        var calendars = [];

        var stmt = this.mSelectCalendars;
        stmt.reset();

        var newCalendarData = [];

        while (stmt.step()) {
            var id = stmt.row.id;
            if (!(id in this.mCache)) {
                newCalendarData.push ({id: id, type: stmt.row.type, uri: stmt.row.uri });
            }
        }
        stmt.reset();

        for each (var caldata in newCalendarData) {
            try {
                this.mCache[caldata.id] = this.createCalendar(caldata.type, makeURI(caldata.uri));
            } catch (e) {
                dump("Can't create calendar for " + caldata.id + " (" + caldata.type + ", " + 
                     caldata.uri + "): " + e + "\n");
                continue;
            }
        }

        for each (var cal in this.mCache)
            calendars.push (cal);

        count.value = calendars.length;
        return calendars;
    },

    getCalendarPref: function(calendar, name) {
        // pref names must be lower case
        name = name.toLowerCase();

        var stmt = this.mGetPref;
        stmt.reset();
        var pp = stmt.params;
        pp.calendar = this.findCalendarID(calendar);
        pp.name = name;

        var value = null;
        if (stmt.step()) {
            value = stmt.row.value;
        }
        stmt.reset();
        return value;
    },

    setCalendarPref: function(calendar, name, value) {
        // pref names must be lower case
        name = name.toLowerCase();

        var calendarID = this.findCalendarID(calendar);

        this.mDB.beginTransaction();

        var pp = this.mDeletePref.params;
        pp.calendar = calendarID;
        pp.name = name;
        this.mDeletePref.step();
        this.mDeletePref.reset();

        pp = this.mInsertPref.params;
        pp.calendar = calendarID;
        pp.name = name;
        pp.value = value;
        this.mInsertPref.step();
        this.mInsertPref.reset();

        this.mDB.commitTransaction();

        this.notifyObservers("onCalendarPrefSet", [calendar, name, value])
    },

    deleteCalendarPref: function(calendar, name) {
        // pref names must be lower case
        name = name.toLowerCase();

        this.notifyObservers("onCalendarPrefDeleting", [calendar, name]);

        var calendarID = this.findCalendarID(calendar);

        var pp = this.mDeletePref.params;
        pp.calendar = calendarID;
        pp.name = name;
        this.mDeletePref.step();
        this.mDeletePref.reset();
    },

    mObservers: Array(),
    addObserver: function(aObserver) {
        if (this.mObservers.indexOf(aObserver) != -1)
            return;

        this.mObservers.push(aObserver);
    },

    removeObserver: function(aObserver) {
        function notThis(v) {
            return v != aObserver;
        }
        
        this.mObservers = this.mObservers.filter(notThis);
    }
};

// This is a prototype object for announcing the fact that a calendar error has
// happened and that the calendar has therefore been put in readOnly mode.  We
// implement a new one of these for each calendar registered to the calmgr.
function errorAnnouncer(calendar) {
    this.calendar = calendar;
    // We compare this to determine if the state actually changed.
    this.storedReadOnly = calendar.readOnly;
    var announcer = this;
    this.observer = {
        // calIObserver:
        onStartBatch: function() {},
        onEndBatch: function() {},
        onLoad: function() {},
        onAddItem: function(aItem) {},
        onModifyItem: function(aNewItem, aOldItem) {},
        onDeleteItem: function(aDeletedItem) {},
        onError: function(aErrNo, aMessage) {
            announcer.announceError(aErrNo, aMessage);
        }
    }
}

errorAnnouncer.prototype.announceError = function(aErrNo, aMessage) {
    var paramBlock = Components.classes["@mozilla.org/embedcomp/dialogparam;1"]
                               .createInstance(Components.interfaces.nsIDialogParamBlock);
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
    var errMsg;
    paramBlock.SetNumberStrings(3);
    if (!this.storedReadOnly && this.calendar.readOnly) {
        // Major errors change the calendar to readOnly
        errMsg = props.formatStringFromName("readOnlyMode", [this.calendar.name], 1);
    } else if (!this.storedReadOnly && !this.calendar.readOnly) {
        // Minor errors don't, but still tell the user something went wrong
        errMsg = props.formatStringFromName("minorError", [this.calendar.name], 1);
    } else {
        // The calendar was already in readOnly mode, but still tell the user
        errMsg = props.formatStringFromName("stillReadOnlyError", [this.calendar.name], 1);
    }

    // When possible, change the error number into its name, to
    // make it slightly more readable.
    var errCode = "0x"+aErrNo.toString(16);
    const calIErrors = Components.interfaces.calIErrors;
    // Check if it is worth enumerating all the error codes.
    if (aErrNo & calIErrors.ERROR_BASE) {
        for (var err in calIErrors) {
            if (calIErrors[err] == aErrNo) {
                errCode = err;
            }
        }
    }

    var message;    
    switch (aErrNo) {
        case calIErrors.CAL_UTF8_DECODING_FAILED:
            message = props.GetStringFromName("utf8DecodeError");
            break;
        case calIErrors.ICS_MALFORMEDDATA:
            message = props.GetStringFromName("icsMalformedError");
            break;
        default:
            message = aMessage
    }

    paramBlock.SetString(0, errMsg);
    paramBlock.SetString(1, errCode);
    paramBlock.SetString(2, message);

    this.storedReadOnly = this.calendar.readOnly;

    var wWatcher = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                             .getService(Components.interfaces.nsIWindowWatcher);
    wWatcher.openWindow(null,
                        "chrome://calendar/content/calErrorPrompt.xul",
                        "_blank",
                        "chrome,dialog=yes",
                        paramBlock);
}

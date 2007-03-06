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
 *   Matthew Willis <lilmatt@mozilla.com>
 *   Michiel van Leeuwen <mvl@exedo.nl>
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

const SUNBIRD_UID = "{718e30fb-e89b-41dd-9da7-e25a45638b28}";

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
    this.mRefreshTimer = null;
    this.setUpPrefObservers();
    this.setUpReadOnlyObservers();
    this.setUpRefreshTimer();
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
            Components.interfaces.nsIObserver,
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
            !aIID.equals(Components.interfaces.calICalendarManager) &&
            !aIID.equals(Components.interfaces.nsIObserver))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    setUpPrefObservers: function ccm_setUpPrefObservers() {
        var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefBranch2);
        prefBranch.addObserver("calendar.autorefresh.enabled", this, false);
        prefBranch.addObserver("calendar.autorefresh.timeout", this, false);
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

    setUpRefreshTimer: function ccm_setUpRefreshTimer() {
        if (this.mRefreshTimer) {
            this.mRefreshTimer.cancel();
        }

        var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefBranch);

        var refreshEnabled = false;
        try {
            var refreshEnabled = prefBranch.getBoolPref("calendar.autorefresh.enabled");
        } catch (e) {
        }

        // Read and convert the minute-based pref to msecs
        var refreshTimeout = 0;
        try {
            var refreshTimeout = prefBranch.getIntPref("calendar.autorefresh.timeout") * 60000;
        } catch (e) {
        }

        if (refreshEnabled && refreshTimeout > 0) {
            this.mRefreshTimer = Components.classes["@mozilla.org/timer;1"]
                                    .createInstance(Components.interfaces.nsITimer);
            this.mRefreshTimer.init(this, refreshTimeout, 
                                   Components.interfaces.nsITimer.TYPE_REPEATING_SLACK);
        }
    },
    
    observe: function ccm_observe(aSubject, aTopic, aData) {
        if (aTopic == 'timer-callback') {
            // Refresh all the calendars that can be refreshed.
            var cals = this.getCalendars({});
            for each (var cal in cals) {
                if (cal.canRefresh) {
                    cal.refresh();
                }
            }
        } else if (aTopic == 'nsPref:changed') {
            if (aData == "calendar.autorefresh.enabled" ||
                aData == "calendar.autorefresh.timeout") {
                this.setUpRefreshTimer();
            }
        }
    },
    
    DB_SCHEMA_VERSION: 7,

    upgradeDB: function (oldVersion) {
        // some common helpers
        function addColumn(db, tableName, colName, colType) {
            db.executeSimpleSQL("ALTER TABLE " + tableName + " ADD COLUMN " + colName + " " + colType);
        }

        if (oldVersion <= 5 && this.DB_SCHEMA_VERSION >= 6) {
            dump ("**** Upgrading calCalendarManager schema to 6\n");

            this.mDB.beginTransaction();
            try {
                // Schema changes in v6:
                //
                // - Change all STRING columns to TEXT to avoid SQLite's
                //   "feature" where it will automatically convert strings to
                //   numbers (ex: 10e4 -> 10000). See bug 333688.

                // Create the new tables.

                try { 
                    this.mDB.executeSimpleSQL("DROP TABLE cal_calendars_v6;" +
                                              "DROP TABLE cal_calendars_prefs_v6;");
                } catch (e) {
                    // We should get exceptions for trying to drop tables
                    // that don't (shouldn't) exist.
                }

                this.mDB.executeSimpleSQL("CREATE TABLE cal_calendars_v6 " +
                                          "(id   INTEGER PRIMARY KEY," +
                                          " type TEXT," +
                                          " uri  TEXT);");

                this.mDB.executeSimpleSQL("CREATE TABLE cal_calendars_prefs_v6 " +
                                          "(id       INTEGER PRIMARY KEY," +
                                          " calendar INTEGER," +
                                          " name     TEXT," +
                                          " value    TEXT);");

                // Copy in the data.
                var calendarCols = ["id", "type", "uri"];
                var calendarPrefsCols = ["id", "calendar", "name", "value"];

                this.mDB.executeSimpleSQL("INSERT INTO cal_calendars_v6(" + calendarCols.join(",") + ") " +
                                          "     SELECT " + calendarCols.join(",") +
                                          "       FROM cal_calendars");

                this.mDB.executeSimpleSQL("INSERT INTO cal_calendars_prefs_v6(" + calendarPrefsCols.join(",") + ") " +
                                          "     SELECT " + calendarPrefsCols.join(",") +
                                          "       FROM cal_calendars_prefs");


                // Delete each old table and rename the new ones to use the
                // old tables' names.
                var tableNames = ["cal_calendars", "cal_calendars_prefs"];

                for (var i in tableNames) {
                    this.mDB.executeSimpleSQL("DROP TABLE " + tableNames[i] + ";" +
                                              "ALTER TABLE " + tableNames[i] + "_v6 " + 
                                              "  RENAME TO " + tableNames[i] + ";");
                }

                this.mDB.commitTransaction();
                oldVersion = 6;
            } catch (e) {
                dump ("+++++++++++++++++ DB Error: " + this.mDB.lastErrorString + "\n");
                Components.utils.reportError("Upgrade failed! DB Error: " +
                                             this.mDB.lastErrorString);
                this.mDB.rollbackTransaction();
                throw e;
            }
        }

        if (oldVersion != 6) {
            dump ("#######!!!!! calCalendarManager Schema Update failed! " +
                  " db version: " + oldVersion + 
                  " this version: " + this.DB_SCHEMA_VERSION + "\n");
            throw Components.results.NS_ERROR_FAILURE;
        }
    },

    initDB: function() {
        var dbService = Components.classes[kStorageServiceContractID]
                                  .getService(kStorageServiceIID);

        if ("getProfileStorage" in dbService) {
            // 1.8 branch
            this.mDB = dbService.getProfileStorage("profile");
        } else {
            // trunk
            this.mDB = dbService.openSpecialDatabase("profile");
        }

        var sqlTables = { cal_calendars: "id INTEGER PRIMARY KEY, type TEXT, uri TEXT",
                          cal_calendars_prefs: "id INTEGER PRIMARY KEY, calendar INTEGER, name TEXT, value TEXT"
                        };

        // Should we check the schema version to see if we need to upgrade?
        var checkSchema = true;

        for (table in sqlTables) {
            if (!this.mDB.tableExists(table)) {
                this.mDB.createTable(table, sqlTables[table]);
                checkSchema = false;
            }
        }

        if (checkSchema) {
            // Check if we need to upgrade
            var version = this.getSchemaVersion();
            //dump ("*** Calendar schema version is: " + version + "\n");

            if (version < this.DB_SCHEMA_VERSION) {
                this.upgradeDB(version);
            } else if (version > this.DB_SCHEMA_VERSION) {
                // Schema version is newer than what we know how to deal with.
                // Alert the user, and quit the app.
                var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                    .getService(Components.interfaces.nsIStringBundleService);

                var brandSb = sbs.createBundle("chrome://branding/locale/brand.properties");
                var calSb = sbs.createBundle("chrome://calendar/locale/calendar.properties");

                var hostAppName = brandSb.GetStringFromName("brandShortName");

                // If we're Lightning, we want to include the extension name
                // in the error message rather than blaming Thunderbird.
                var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                                        .getService(Ci.nsIXULAppInfo);
                var calAppName;
                if (appInfo.ID == SUNBIRD_UID) {
                    calAppName = hostAppName;
                } else {
                    lightningSb = sbs.createBundle("chrome://lightning/locale/lightning.properties");
                    calAppName = lightningSb.GetStringFromName("brandShortName");
                }

                var promptSvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                          .getService(Components.interfaces.nsIPromptService);

                buttonFlags = promptSvc.BUTTON_POS_0 *
                              promptSvc.BUTTON_TITLE_IS_STRING +
                              promptSvc.BUTTON_POS_0_DEFAULT;

                var choice = promptSvc.confirmEx(
                                null,
                                calSb.formatStringFromName("tooNewSchemaErrorBoxTitle",
                                                           [calAppName], 1),
                                calSb.formatStringFromName("tooNewSchemaErrorBoxText",
                                                           [calAppName, hostAppName], 2),
                                buttonFlags,
                                calSb.formatStringFromName("tooNewSchemaButtonQuit",
                                                           [hostAppName], 1),
                                null, // No second button text
                                null, // No third button text
                                null, // No checkbox
                                {value: false}); // Unnecessary checkbox state

                if (choice == 0) {
                    var startup = Components.classes["@mozilla.org/toolkit/app-startup;1"]
                                            .getService(Components.interfaces.nsIAppStartup);
                    startup.quit(Components.interfaces.nsIAppStartup.eForceQuit);
                }
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

    /** 
     * @return      db schema version
     * @exception   various, depending on error
     */
    getSchemaVersion: function calMgrGetSchemaVersion() {
        var stmt;
        var version = null;

        try {
            stmt = createStatement(this.mDB,
                    "SELECT version FROM cal_calendar_schema_version LIMIT 1");
            if (stmt.step()) {
                version = stmt.row.version;
            }
            stmt.reset();

            if (version !== null) {
                // This is the only place to leave this function gracefully.
                return version;
            }
        } catch (e) {
            if (stmt) {
                stmt.reset();
            }
            dump ("++++++++++++ calMgrGetSchemaVersion() error: " +
                  this.mDB.lastErrorString + "\n");
            Components.utils.reportError("Error getting calendar " +
                                         "schema version! DB Error: " + 
                                         this.mDB.lastErrorString);
            throw e;
        }

        throw "cal_calendar_schema_version SELECT returned no results";
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
        if (this.findCalendarID(calendar) in this.mCache) {
            throw "Can't delete a registered calendar";
        }
        this.notifyObservers("onCalendarDeleting", [calendar]);

        // XXX This is a workaround for bug 351499. We should remove it once
        // we sort out the whole "delete" vs. "unsubscribe" UI thing.
        //
        // We only want to delete the contents of calendars from local
        // providers (storage and memory). Otherwise we may nuke someone's
        // calendar stored on a server when all they really wanted to do was
        // unsubscribe.
        if (calendar instanceof Components.interfaces.calICalendarProvider &&
           (calendar.type == "storage" || calendar.type == "memory")) {
            try {
                calendar.deleteCalendar(calendar, null);
            } catch (e) {
                Components.utils.reportError("error purging calendar: " + e);
            }
        }
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

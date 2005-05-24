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
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

//
// calStorageCalendar.js
//

const kStorageServiceContractID = "@mozilla.org/storage/service;1";
const kStorageServiceIID = Components.interfaces.mozIStorageService;

const kCalICalendar = Components.interfaces.calICalendar;

const kCalCalendarManagerContractID = "@mozilla.org/calendar/manager;1";
const kCalICalendarManager = Components.interfaces.calICalendarManager;

const kCalEventContractID = "@mozilla.org/calendar/event;1";
const kCalIEvent = Components.interfaces.calIEvent;
var CalEvent;

const kCalTodoContractID = "@mozilla.org/calendar/todo;1";
const kCalITodo = Components.interfaces.calITodo;
var CalTodo;

const kCalDateTimeContractID = "@mozilla.org/calendar/datetime;1";
const kCalIDateTime = Components.interfaces.calIDateTime;
var CalDateTime;

const kCalAttendeeContractID = "@mozilla.org/calendar/attendee;1";
const kCalIAttendee = Components.interfaces.calIAttendee;
var CalAttendee;

const kCalItemOccurrenceContractID = "@mozilla.org/calendar/item-occurrence;1";
const kCalIItemOccurrence = Components.interfaces.calIItemOccurrence;
var CalItemOccurrence;

const kCalRecurrenceInfoContractID = "@mozilla.org/calendar/recurrence-info;1";
const kCalIRecurrenceInfo = Components.interfaces.calIRecurrenceInfo;
var CalRecurrenceInfo;

const kCalRecurrenceRuleContractID = "@mozilla.org/calendar/recurrence-rule;1";
const kCalIRecurrenceRule = Components.interfaces.calIRecurrenceRule;
var CalRecurrenceRule;

const kCalRecurrenceDateSetContractID = "@mozilla.org/calendar/recurrence-date-set;1";
const kCalIRecurrenceDateSet = Components.interfaces.calIRecurrenceDateSet;
var CalRecurrenceDateSet;

const kCalRecurrenceDateContractID = "@mozilla.org/calendar/recurrence-date;1";
const kCalIRecurrenceDate = Components.interfaces.calIRecurrenceDate;
var CalRecurrenceDate;

const kMozStorageStatementWrapperContractID = "@mozilla.org/storage/statement-wrapper;1";
const kMozStorageStatementWrapperIID = Components.interfaces.mozIStorageStatementWrapper;
var MozStorageStatementWrapper;

if (!kMozStorageStatementWrapperIID) {
    dump("*** mozStorage not available, calendar/storage provider will not function\n");
}

const CAL_ITEM_TYPE_EVENT = 0;
const CAL_ITEM_TYPE_TODO = 1;

// bitmasks
const CAL_ITEM_FLAG_PRIVATE = 1;
const CAL_ITEM_FLAG_HAS_ATTENDEES = 2;
const CAL_ITEM_FLAG_HAS_PROPERTIES = 4;
const CAL_ITEM_FLAG_EVENT_ALLDAY = 8;
const CAL_ITEM_FLAG_HAS_RECURRENCE = 16;

function initCalStorageCalendarComponent() {
    CalEvent = new Components.Constructor(kCalEventContractID, kCalIEvent);
    CalTodo = new Components.Constructor(kCalTodoContractID, kCalITodo);
    CalDateTime = new Components.Constructor(kCalDateTimeContractID, kCalIDateTime);
    CalAttendee = new Components.Constructor(kCalAttendeeContractID, kCalIAttendee);
    CalItemOccurrence = new Components.Constructor(kCalItemOccurrenceContractID, kCalIItemOccurrence);
    CalRecurrenceInfo = new Components.Constructor(kCalRecurrenceInfoContractID, kCalIRecurrenceInfo);
    CalRecurrenceRule = new Components.Constructor(kCalRecurrenceRuleContractID, kCalIRecurrenceRule);
    CalRecurrenceDateSet = new Components.Constructor(kCalRecurrenceDateSetContractID, kCalIRecurrenceDateSet);
    CalRecurrenceDate = new Components.Constructor(kCalRecurrenceDateContractID, kCalIRecurrenceDate);
    MozStorageStatementWrapper = new Components.Constructor(kMozStorageStatementWrapperContractID, kMozStorageStatementWrapperIID);
}

//
// Storage helpers
//

function createStatement (dbconn, sql) {
    try {
        var stmt = dbconn.createStatement(sql);
        var wrapper = MozStorageStatementWrapper();
        wrapper.initialize(stmt);
        return wrapper;
    } catch (e) {
        Components.reportError("mozStorage exception: createStatement failed, statement: '" + 
                               sql + "', error: '" + dbconn.lastErrorString + "'");
    }

    return null;
}

function textToDate(d) {
    var dval = parseInt(d.substr(2));
    var date;
    if (d[0] == 'U') {
        // isutc
        date = newDateTime(dval, "UTC");
    } else if (d[0] == 'L') {
        // is local time
        date = newDateTime(dval, "floating");
    }
    if (d[1] == 'D')
        date.isDate = true;
    return date;
}

function dateToText(d) {
    var datestr;
    if (d.timezone != "floating") {
        datestr = "U";
    } else {
        datestr = "L";
    }

    if (d.isDate) {
        datestr += "D";
    } else {
        datestr += "T";
    }

    datestr += d.nativeTime;
    return datestr;
}

// 
// other helpers
//

function newDateTime(aNativeTime, aTimezone) {
    var t = new CalDateTime();
    t.nativeTime = aNativeTime;
    if (aTimezone && aTimezone != "floating") {
        t = t.getInTimezone(aTimezone);
    } else {
        t.timezone = "floating";
    }

    return t;
}

function makeOccurrence(item, start, end)
{
    var occ = CalItemOccurrence();
    // XXX poor form
    occ.wrappedJSObject.item = item;
    occ.wrappedJSObject.occurrenceStartDate = start;
    occ.wrappedJSObject.occurrenceEndDate = end;
    return occ;
}

//
// calStorageCalendar
//

var activeCalendarManager = null;
function getCalendarManager()
{
    if (!activeCalendarManager) {
        activeCalendarManager = 
            Components.classes[kCalCalendarManagerContractID].getService(kCalICalendarManager);
    }
    return activeCalendarManager;
}

function calStorageCalendar() {
    this.wrappedJSObject = this;
}

calStorageCalendar.prototype = {
    //
    // private members
    //
    mObservers: Array(),
    mDB: null,
    mDBTwo: null,
    mItemCache: Array(),
    mCalId: 0,

    //
    // nsISupports interface
    // 
    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calICalendar))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    //
    // calICalendar interface
    //

    // attribute AUTF8String name;
    get name() {
        return getCalendarManager().getCalendarPref(this, "NAME");
    },
    set name(name) {
        getCalendarManager().setCalendarPref(this, "NAME", name);
    },
    // readonly attribute AUTF8String type;
    get type() { return "storage"; },

    mURI: null,
    // attribute nsIURI uri;
    get uri() { return this.mURI; },
    set uri(aURI) {
        // we can only load once
        if (this.mURI)
            throw Components.results.NS_ERROR_FAILURE;

        var id = 0;

        // check if there's a ?id=
        var path = aURI.path;
        var pos = path.indexOf("?id=");

        if (pos != -1) {
            id = parseInt(path.substr(pos+4));
            path = path.substr(0, pos);
        }

        var dbService;
        if (aURI.scheme == "file") {
            var fileURL = aURI.QueryInterface(Components.interfaces.nsIFileURL);
            if (!fileURL)
                throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

            // open the database
            dbService = Components.classes[kStorageServiceContractID].getService(kStorageServiceIID);
            this.mDB = dbService.openDatabase (fileURL.file);
            this.mDBTwo = dbService.openDatabase (fileURL.file);
        } else if (aURI.scheme == "moz-profile-calendar") {
            dbService = Components.classes[kStorageServiceContractID].getService(kStorageServiceIID);
            this.mDB = dbService.getProfileStorage("profile");
            this.mDBTwo = dbService.getProfileStorage("profile");
        }

        this.initDB();

        this.mCalId = id;
        this.mURI = aURI;
    },

    // attribute boolean readOnly;
    get readOnly() { return false; },
    set readOnly() { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

    // attribute boolean suppressAlarms;
    get suppressAlarms() { return false; },
    set suppressAlarms(aSuppressAlarms) { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

    // void addObserver( in calIObserver observer );
    addObserver: function (aObserver, aItemFilter) {
        for each (obs in this.mObservers) {
            if (obs == aObserver)
                return;
        }

        this.mObservers.push(aObserver);
    },

    // void removeObserver( in calIObserver observer );
    removeObserver: function (aObserver) {
        var newObservers = Array();
        for each (obs in this.mObservers) {
            if (obs != aObserver)
                newObservers.push(obs);
        }
        this.mObservers = newObservers;
    },

    // void addItem( in calIItemBase aItem, in calIOperationListener aListener );
    addItem: function (aItem, aListener) {
        if (aItem.id == null) {
            // is this an error?  Or should we generate an IID?
            aItem.id = "uuid:" + (new Date()).getTime();
        } else {
            var olditem = this.getItemById(aItem.id);
            if (olditem) {
                if (aListener)
                    aListener.onOperationComplete (this,
                                                   Components.results.NS_ERROR_FAILURE,
                                                   aListener.ADD,
                                                   aItem.id,
                                                   "ID already exists for addItem");
                return;
            }
        }

        var newItem = aItem.clone();
        newItem.parent = this;
        newItem.generation = 1;
        newItem.makeImmutable();

        this.flushItem (newItem, null);

        // notify the listener
        if (aListener)
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.ADD,
                                           newItem.id,
                                           newItem);

        // notify observers
        this.observeAddItem(newItem);
    },

    // void modifyItem( in calIItemBase aItem, in calIOperationListener aListener );
    modifyItem: function (aItem, aListener) {
        if (aItem.id == null) {
            // this is definitely an error
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aItem.id,
                                               "ID for modifyItem item is null");
            return;
        }

        // get the old item
        var olditem = this.getItemById(aItem.id);
        if (!olditem) {
            // no old item found?  should be using addItem, then.
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aItem.id,
                                               "ID does not already exit for modifyItem");
            return;
        }

        if (olditem.generation != aItem.generation) {
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aItem.id,
                                               "generation too old for for modifyItem");
            return;
        }

        var newItem = aItem.clone();
        newItem.generation += 1;
        newItem.makeImmutable();

        this.flushItem (newItem, olditem);

        if (aListener)
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.MODIFY,
                                           newItem.id,
                                           newItem);

        // notify observers
        this.observeModifyItem(newItem, olditem);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aItem, aListener) {
        if (aItem.id == null) {
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.DELETE,
                                               null,
                                               "ID is null for deleteItem");
            return;
        }

        this.deleteItemById(aItem.id);

        if (aListener)
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.DELETE,
                                           aItem.id,
                                           null);

        // notify observers 
        this.observeDeleteItem(aItem);
    },

    // void getItem( in string id, in calIOperationListener aListener );
    getItem: function (aId, aListener) {
        if (!aListener)
            return;

        var item = this.getItemById (aId);
        if (!item) {
            aListener.onOperationComplete (this,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.GET,
                                           aId,
                                           "ID doesn't exist for getItem");
        }

        var item_iid = null;
        if (item instanceof kCalIEvent)
            item_iid = kCalIEvent;
        else if (item instanceof kCalITodo)
            item_iid = kCalITodo;
        else {
            aListener.onOperationComplete (this,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.GET,
                                           aId,
                                           "Can't deduce item type based on QI");
            return;
        }

        aListener.onGetResult (this,
                               Components.results.NS_OK,
                               item_iid, null,
                               1, [item]);

        aListener.onOperationComplete (this,
                                       Components.results.NS_OK,
                                       aListener.GET,
                                       aId,
                                       null);
    },

    // void getItems( in unsigned long aItemFilter, in unsigned long aCount, 
    //                in calIDateTime aRangeStart, in calIDateTime aRangeEnd,
    //                in calIOperationListener aListener );
    getItems: function (aItemFilter, aCount,
                        aRangeStart, aRangeEnd, aListener)
    {
        //var profStartTime = Date.now();
        if (!aListener)
            return;

        var self = this;

        var itemsFound = Array();
        var startTime = -0x7fffffffffffffff;
        // endTime needs to be the max value a PRTime can be
        var endTime = 0x7fffffffffffffff;
        var count = 0;
        if (aRangeStart)
            startTime = aRangeStart.nativeTime;
        if (aRangeEnd)
            endTime = aRangeEnd.nativeTime;

        // only events for now
        var wantEvents = ((aItemFilter & kCalICalendar.ITEM_FILTER_TYPE_EVENT) != 0);
        var wantTodos = ((aItemFilter & kCalICalendar.ITEM_FILTER_TYPE_TODO) != 0);
        var asOccurrences = ((aItemFilter & kCalICalendar.ITEM_FILTER_CLASS_OCCURRENCES) != 0);
        if (!wantEvents && !wantTodos) {
            // nothing to do
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.GET,
                                           null,
                                           null);
            return;
        }

        // sending items to the listener 1 at a time sucks. instead,
        // queue them up.
        // if we ever have more than maxQueueSize items outstanding,
        // call the listener.  Calling with null theItems forces
        // a send and a queue clear.
        var maxQueueSize = 10;
        var queuedItems = [ ];
        var queuedItemsIID;
        function queueItems(theItems, theIID) {
            // if we're about to start sending a different IID,
            // flush the queue
            if (theIID && queuedItemsIID != theIID) {
                if (queuedItemsIID)
                    queueItems(null);
                queuedItemsIID = theIID;
            }

            if (theItems)
                queuedItems = queuedItems.concat(theItems);

            if (queuedItems.length != 0 && (!theItems || queuedItems.length > maxQueueSize)) {
                //var listenerStart = Date.now();
                aListener.onGetResult(self,
                                      Components.results.NS_OK,
                                      queuedItemsIID, null,
                                      queuedItems.length, queuedItems);
                //var listenerEnd = Date.now();
                //dump ("++++ listener callback took: " + (listenerEnd - listenerStart) + " ms\n");

                queuedItems = [ ];
            }
        }

        // helper function to handle converting a row to an item,
        // expanding occurrences, and queue the items for the listener
        function handleResultItems(theRow, theConversionFunction, theIID) {
            var flags = {};
            var item = theConversionFunction.call(self, theRow, flags);
            self.getAdditionalDataForItem(item, flags.value);
            item.makeImmutable();

            var expandedItems;

            if (asOccurrences && item.recurrenceInfo) {
                expandedItems = item.recurrenceInfo.getOccurrences (aRangeStart, aRangeEnd, 0, {});
            } else {
                if (asOccurrences)
                    expandedItems = [ makeOccurrence(item, item.startDate, item.endDate) ];
                else
                    expandedItems = [ item ];
            }

            queueItems (expandedItems, asOccurrences ? kCalIItemOccurrence : theIID);

            return expandedItems.length;
        }

        // check the count and send end if count is exceeded
        function checkCount() {
            if (aCount && count >= aCount) {
                // flush queue
                queueItems(null);

                // send operation complete
                aListener.onOperationComplete (self,
                                               Components.results.NS_OK,
                                               aListener.GET,
                                               null,
                                               null);

                // tell caller we're done
                return true;
            }

            return false;
        }

        //
        // First fetch all the events
        if (wantEvents) {
            // this will contain a lookup table of item ids that we've already dealt with,
            // if we have recurrence to care about.
            var handledRecurringEvents = { };
            var sp;             // stmt params

            // first get non-recurring events and recurring events that happen
            // to fall within the range
            sp = this.mSelectEventsByRange.params;
            sp.cal_id = this.mCalId;
            sp.event_start = startTime;
            sp.event_end = endTime;

            while (this.mSelectEventsByRange.step()) {
                var row = this.mSelectEventsByRange.row;
                count += handleResultItems(row, this.getEventFromRow, kCalIEvent);
                if (asOccurrences && row.flags & CAL_ITEM_FLAG_HAS_RECURRENCE)
                    handledRecurringEvents[row.id] = true;

                if (aCount && count >= aCount)
                    break;
            }
            this.mSelectEventsByRange.reset();
            if (checkCount())
                return;

            // then, if we want occurrences, we need to query database-wide.. yuck
            if (asOccurrences) {
                sp = this.mSelectEventsWithRecurrence.params;
                sp.cal_id = this.mCalId;
                while (this.mSelectEventsWithRecurrence.step()) {
                    var row = this.mSelectEventsWithRecurrence.row;
                    // did we already deal with this event id?
                    if (handledRecurringEvents[row.id] == true)
                        continue;

                    count += handleResultItems(row, this.getEventFromRow, kCalIEvent);
                    if (aCount && count >= aCount)
                        break;
                }
                this.mSelectEventsWithRecurrence.reset();
                if (checkCount())
                    return;
            }
        }

        // if todos are wanted, do them next
        if (wantTodos) {
            // this will contain a lookup table of item ids that we've already dealt with,
            // if we have recurrence to care about.
            var handledRecurringTodos = { };
            var sp;             // stmt params

            // first get non-recurring todos and recurring todos that happen
            // to fall within the range
            sp = this.mSelectTodosByRange.params;
            sp.cal_id = this.mCalId;
            sp.todo_start = startTime;
            sp.todo_end = endTime;

            while (this.mSelectTodosByRange.step()) {
                var row = this.mSelectTodosByRange.row;
                count += handleResultItems(row, this.getTodoFromRow, kCalITodo);
                if (asOccurrences && row.flags & CAL_ITEM_FLAG_HAS_RECURRENCE)
                    handledRecurringTodos[row.id] = true;
                if (aCount && count >= aCount)
                    break;
            }
            this.mSelectTodosByRange.reset();
            if (checkCount())
                return;

            // then, if we want occurrences, we need to query database-wide.. yuck
            if (asOccurrences) {
                sp = this.mSelectTodosWithRecurrence.params;
                sp.cal_id = this.mCalId;
                while (this.mSelectTodosWithRecurrence.step()) {
                    var row = this.mSelectTodosByRange.row;
                    // did we already deal with this todo id?
                    if (handledRecurringTodos[row.id] == true)
                        continue;

                    count += handleResultItems(row, this.getTodoFromRow, kCalITodo);
                    if (aCount && count >= aCount)
                        break;
                }
                this.mSelectTodosWithRecurrence.reset();
                if (checkCount())
                    return;
            }
        }

        // flush the queue
        queueItems(null);

        // and finish
        aListener.onOperationComplete (this,
                                       Components.results.NS_OK,
                                       aListener.GET,
                                       null,
                                       null);

        //var profEndTime = Date.now();
        //dump ("++++ getItems took: " + (profEndTime - profStartTime) + " ms\n");
    },

    //
    // Helper functions
    //
    observeLoad: function () {
        for each (obs in this.mObservers)
            obs.onLoad ();
    },

    observeBatchChange: function (aNewBatchMode) {
        for each (obs in this.mObservers) {
            if (aNewBatchMode)
                obs.onStartBatch ();
            else
                obs.onEndBatch ();
        }
    },

    observeAddItem: function (aItem) {
        for each (obs in this.mObservers)
            obs.onAddItem (aItem);
    },

    observeModifyItem: function (aOldItem, aNewItem) {
        for each (obs in this.mObservers)
            obs.onModifyItem (aOldItem, aNewItem);
    },

    observeDeleteItem: function (aDeletedItem) {
        for each (obs in this.mObservers)
            obs.onDeleteItem (aDeletedItem);
    },

    //
    // database handling
    //

    // initialize the database schema.
    // needs to do some version checking
    initDBSchema: function () {
        for (table in sqlTables) {
            dump (table + "\n");
            try {
                this.mDB.executeSimpleSQL("DROP TABLE " + table);
            } catch (e) { }
            this.mDB.createTable(table, sqlTables[table]);
        }

        this.mDB.executeSimpleSQL("INSERT INTO cal_calendar_schema_version VALUES(" + this.DB_SCHEMA_VERSION + ")");
    },

    // check db version
    DB_SCHEMA_VERSION: 3,
    versionCheck: function () {
        var version = -1;
        var selectSchemaVersion;

        try {
            selectSchemaVersion = createStatement (this.mDB, "SELECT version FROM cal_calendar_schema_version LIMIT 1");
            if (selectSchemaVersion.step()) {
                version = selectSchemaVersion.row.version;
            }
        } catch (e) {
            // either the cal_calendar_schema_version table is not
            // found, or something else happened
            version = -1;
        }
        if (selectSchemaVersion)
            selectSchemaVersion.reset();

        return version;
    },

    upgradeDB: function (oldVersion) {
        // some common helpers
        function addColumn(tableName, colName, colType) {
            this.mDB.executeSimpleSQL("ALTER TABLE " + tableName + " ADD COLUMN " + colName + " " + colType);
        }

        if (oldVersion == 2 && this.DB_SCHEMA_VERSION == 3) {
            this.mDB.beginTransaction();
            try {
                // the change between 2 and 3 includes the splitting of cal_items into
                // cal_events and cal_todos, and the addition of columns for
                // event_start_tz, event_end_tz, todo_entry_tz, todo_due_tz.
                // These need to default to "UTC" if their corresponding time is
                // given, since that's what the default was for v2 calendars

                dump ("**** Upgrading schema from 2 -> 3\n");

                // create the two new tables
                dump ("dropping\n");
                try { this.mDB.executeSimpleSQL("DROP TABLE cal_events; DROP TABLE cal_todos;"); } catch (e) { }
                dump ("cal_events\n");
                this.mDB.createTable("cal_events", sqlTables["cal_events"]);
                dump ("cal_todos\n");
                this.mDB.createTable("cal_todos", sqlTables["cal_todos"]);

                dump ("copy stuff over\n");

                // copy stuff over
                var eventCols = ["cal_id", "id", "time_created", "last_modified", "title",
                                 "priority", "privacy", "ical_status", "flags",
                                 "event_start", "event_end", "event_stamp"];
                var todoCols = ["cal_id", "id", "time_created", "last_modified", "title",
                                "priority", "privacy", "ical_status", "flags",
                                "todo_entry", "todo_due", "todo_completed", "todo_complete"];

                this.mDB.executeSimpleSQL("INSERT INTO cal_events(" + eventCols.join(",") + ") " +
                                          "     SELECT " + eventCols.join(",") +
                                          "       FROM cal_items WHERE item_type = 0");
                this.mDB.executeSimpleSQL("INSERT INTO cal_todos(" + todoCols.join(",") + ") " +
                                          "     SELECT " + todoCols.join(",") +
                                          "       FROM cal_items WHERE item_type = 1");

                dump ("update stuff\n");

                // now fix up the new _tz columns
                this.mDB.executeSimpleSQL("UPDATE cal_events SET event_start_tz = 'UTC' WHERE event_start IS NOT NULL");
                this.mDB.executeSimpleSQL("UPDATE cal_events SET event_end_tz = 'UTC' WHERE event_end IS NOT NULL");
                this.mDB.executeSimpleSQL("UPDATE cal_todos SET todo_entry_tz = 'UTC' WHERE todo_entry IS NOT NULL");
                this.mDB.executeSimpleSQL("UPDATE cal_todos SET todo_due_tz = 'UTC' WHERE todo_due IS NOT NULL");
                this.mDB.executeSimpleSQL("UPDATE cal_todos SET todo_completed_tz = 'UTC' WHERE todo_completed IS NOT NULL");

                // finally update the version
                this.mDB.executeSimpleSQL("DELETE FROM cal_calendar_schema_version; INSERT INTO cal_calendar_schema_version VALUES (3);");

                this.mDB.commitTransaction();
            } catch (e) {
                dump ("+++++++++++++++++ DB Error: " + this.mDB.lastErrorString + "\n");
                Components.reportError("Upgrade failed! DB Error: " + this.mDB.lastErrorString);
                this.mDB.rollbackTransaction();
                throw e;
            }
        } else {
            throw "Can't update calendar schema!";
        }
    },

    // database initialization
    // assumes mDB is valid

    initDB: function () {
        var version = this.versionCheck();
        dump ("*** Calendar schema version is: " + version + "\n");
        if (version == -1) {
            this.initDBSchema();
        } else if (version != this.DB_SCHEMA_VERSION) {
            this.upgradeDB(version);
        }

        this.mSelectEvent = createStatement (
            this.mDB,
            "SELECT * FROM cal_events " +
            "WHERE id = :id "+
            "LIMIT 1"
            );

        this.mSelectTodo = createStatement (
            this.mDB,
            "SELECT * FROM cal_todos " +
            "WHERE id = :id "+
            "LIMIT 1"
            );

        this.mSelectEventsByRange = createStatement(
            this.mDB,
            "SELECT * FROM cal_events " +
            "WHERE event_end >= :event_start AND event_start < :event_end " +
            "  AND cal_id = :cal_id"
            );

        this.mSelectTodosByRange = createStatement(
            this.mDB,
            "SELECT * FROM cal_todos " +
            "WHERE todo_entry >= :todo_start AND todo_entry < :todo_end " +
            "  AND cal_id = :cal_id"
            );

        this.mSelectEventsWithRecurrence = createStatement(
            this.mDB,
            "SELECT * FROM cal_events " +
            " WHERE flags & 16 == 16 " +
            "   AND cal_id = :cal_id"
            );

        this.mSelectTodosWithRecurrence = createStatement(
            this.mDB,
            "SELECT * FROM cal_todos " +
            " WHERE flags & 16 == 16 " +
            "   AND cal_id = :cal_id"
            );

        // For the extra-item data, note that we use mDBTwo, so that
        // these can be executed while a selectItems is running!
        this.mSelectAttendeesForItem = createStatement(
            this.mDBTwo,
            "SELECT * FROM cal_attendees " +
            "WHERE item_id = :item_id"
            );

        this.mSelectPropertiesForItem = createStatement(
            this.mDBTwo,
            "SELECT * FROM cal_properties " +
            "WHERE item_id = :item_id"
            );

        this.mSelectRecurrenceForItem = createStatement(
            this.mDBTwo,
            "SELECT * FROM cal_recurrence " +
            "WHERE item_id = :item_id " +
            "ORDER BY recur_index"
            );

        // insert statements
        this.mInsertEvent = createStatement (
            this.mDB,
            "INSERT INTO cal_events " +
            "  (cal_id, id, time_created, last_modified, " +
            "   title, priority, privacy, ical_status, flags, " +
            "   event_start, event_start_tz, event_end, event_end_tz, event_stamp, " +
            "   alarm_time, alarm_time_tz) " +
            "VALUES (:cal_id, :id, :time_created, :last_modified, " +
            "        :title, :priority, :privacy, :ical_status, :flags, " +
            "        :event_start, :event_start_tz, :event_end, :event_end_tz, :event_stamp, " +
            "        :alarm_time, :alarm_time_tz)"
            );

        this.mInsertTodo = createStatement (
            this.mDB,
            "INSERT INTO cal_todos " +
            "  (cal_id, id, time_created, last_modified, " +
            "   title, priority, privacy, ical_status, flags, " +
            "   todo_entry, todo_entry_tz, todo_due, todo_due_tz, todo_completed, " +
            "   todo_completed_tz, todo_complete, " +
            "   alarm_time, alarm_time_tz) " +
            "VALUES (:cal_id, :id, :time_created, :last_modified, " +
            "        :title, :priority, :privacy, :ical_status, :flags, " +
            "        :todo_entry, :todo_entry_tz, :todo_due, :todo_due_tz, " +
            "        :todo_completed, :todo_completed_tz, :todo_complete, " +
            "        :alarm_time, :alarm_time_tz)"
            );
        this.mInsertProperty = createStatement (
            this.mDB,
            "INSERT INTO cal_properties (item_id, key, value) " +
            "VALUES (:item_id, :key, :value)"
            );
        this.mInsertAttendee = createStatement (
            this.mDB,
            "INSERT INTO cal_attendees " +
            "  (item_id, attendee_id, common_name, rsvp, role, status, type) " +
            "VALUES (:item_id, :attendee_id, :common_name, :rsvp, :role, :status, :type)"
            );
        this.mInsertRecurrence = createStatement (
            this.mDB,
            "INSERT INTO cal_recurrence " +
            "  (item_id, recur_index, recur_type, is_negative, dates, count, end_date, interval, second, minute, hour, day, monthday, yearday, weekno, month, setpos) " +
            "VALUES (:item_id, :recur_index, :recur_type, :is_negative, :dates, :count, :end_date, :interval, :second, :minute, :hour, :day, :monthday, :yearday, :weekno, :month, :setpos)"
            );

        // delete statements
        this.mDeleteEvent = createStatement (
            this.mDB,
            "DELETE FROM cal_events WHERE id = :id"
            );
        this.mDeleteTodo = createStatement (
            this.mDB,
            "DELETE FROM cal_todos WHERE id = :id"
            );
        this.mDeleteAttendees = createStatement (
            this.mDB,
            "DELETE FROM cal_attendees WHERE item_id = :item_id"
            );
        this.mDeleteProperties = createStatement (
            this.mDB,
            "DELETE FROM cal_properties WHERE item_id = :item_id"
            );
        this.mDeleteRecurrence = createStatement (
            this.mDB,
            "DELETE FROM cal_recurrence WHERE item_id = :item_id"
            );
    },


    // database helper functions

    // read in the common ItemBase attributes from aDBRow, and stick
    // them on item
    getItemBaseFromRow: function (row, flags, item) {
        item.creationDate = newDateTime(row.time_created, "UTC");
        item.lastModifiedTime = newDateTime(row.last_modified, "UTC");
        item.parent = this;
        item.id = row.id;
        item.title = row.title;
        item.priority = row.priority;
        item.privacy = row.privacy;
        item.status = row.ical_status;

        if (row.alarm_time) {
            item.alarmTime = newDateTime(row.alarm_time, row.alarm_time_tz);
            item.hasAlarm = true;
        } else {
            item.hasAlarm = false;
        }

        if (flags)
            flags.value = row.flags;
    },

    getEventFromRow: function (row, flags) {
        var item = new CalEvent();

        this.getItemBaseFromRow (row, flags, item);

        item.startDate = newDateTime(row.event_start, row.event_start_tz);
        item.endDate = newDateTime(row.event_end, row.event_end_tz);
        item.stampTime = newDateTime(row.event_stamp, "UTC");
        item.isAllDay = ((row.flags & CAL_ITEM_FLAG_EVENT_ALLDAY) != 0);

        return item;
    },

    getTodoFromRow: function (row, flags) {
        var item = new CalTodo();

        this.getItemBaseFromRow (row, flags, item);

        item.entryDate = newDateTime(row.todo_entry, row.todo_entry_tz);
        item.dueDate = newDateTime(row.todo_due, row.todo_due_tz);
        item.completedDate = newDateTime(row.todo_complete, row.todo_complete_tz);
        item.percentComplete = row.todo_complete;

        return item;
    },

    // after we get the base item, we need to check if we need to pull in
    // any extra data from other tables.  We do that here.

    // note that we use mDBTwo for this, so this can be run while a
    // select is executing; don't use any statements that aren't
    // against mDBTwo in here!
    
    getAdditionalDataForItem: function (item, flags) {
        if (flags & CAL_ITEM_FLAG_HAS_ATTENDEES) {
            this.mSelectAttendeesForItem.params.item_id = item.id;
            while (this.mSelectAttendeesForItem.step()) {
                var attendee = this.getAttendeeFromRow(this.mSelectAttendeesForItem.row);
                item.addAttendee(attendee);
            }
            this.mSelectAttendeesForItem.reset();
        }

        var row;
        if (flags & CAL_ITEM_FLAG_HAS_PROPERTIES) {
            this.mSelectPropertiesForItem.params.item_id = item.id;
            while (this.mSelectPropertiesForItem.step()) {
                row = this.mSelectPropertiesForItem.row;
                item.setProperty (row.key, row.value);
            }
            this.mSelectPropertiesForItem.reset();
        }

        var i;
        if (flags & CAL_ITEM_FLAG_HAS_RECURRENCE) {
            var rec = null;

            this.mSelectRecurrenceForItem.params.item_id = item.id;
            while (this.mSelectRecurrenceForItem.step()) {
                row = this.mSelectRecurrenceForItem.row;

                var ritem = null;

                if (row.recur_type == null ||
                    row.recur_type == "x-dateset")
                {
                    ritem = new CalRecurrenceDateSet();

                    var dates = row.dates.split(",");
                    for (i = 0; i < dates.length; i++) {
                        var date = textToDate(dates[i]);
                        ritem.addDate(date);
                    }
                } else if (row.recur_type == "x-date") {
                    ritem = new CalRecurrenceDate();
                    var d = row.dates;
                    ritem.date = textToDate(d);
                } else {
                    ritem = new CalRecurrenceRule();

                    ritem.type = row.recur_type;
                    if (row.count) {
                        ritem.count = row.count;
                    } else {
                        ritem.endDate = newDateTime(row.end_date);
                    }
                    ritem.interval = row.interval;

                    var rtypes = ["second",
                                  "minute",
                                  "hour",
                                  "day",
                                  "monthday",
                                  "yearday",
                                  "weekno",
                                  "month",
                                  "setpos"];

                    for (i = 0; i < rtypes.length; i++) {
                        var comp = "BY" + rtypes[i].toUpperCase();
                        if (row[rtypes[i]]) {
                            var rstr = row[rtypes[i]].split(",");
                            var rarray = [];
                            for (var j = 0; j < rstr.length; j++) {
                                rarray[j] = parseInt(rstr[j]);
                            }

                            ritem.setComponent (comp, rarray.length, rarray);
                        }
                    }
                }

                if (row.is_negative)
                    ritem.isNegative = true;
                if (rec == null) {
                    rec = new CalRecurrenceInfo();
                    rec.initialize(item);
                }
                rec.appendRecurrenceItem(ritem);
            }

            if (rec == null) {
                dump ("XXXX Expected to find recurrence, but got no items!\n");
            }
            item.recurrenceInfo = rec;

            this.mSelectRecurrenceForItem.reset();
        }
    },

    getAttendeeFromRow: function (row) {
        var a = CalAttendee();

        a.id = row.attendee_id;
        a.commonName = row.common_name;
        a.rsvp = (row.rsvp != 0);
        a.role = row.role;
        a.participationStatus = row.status;
        a.userType = row.type;

        return a;
    },

    //
    // get item from db or from cache with given iid
    //
    getItemById: function (aID) {
        // cached?
        if (this.mItemCache[aID] != null)
            return this.mItemCache[aID];

        var retval = null;

        // not cached; need to read from the db
        var flags = {};
        var item = null;

        // try events first
        this.mSelectEvent.params.id = aID;
        if (this.mSelectEvent.step())
            item = this.getEventFromRow(this.mSelectEvent.row, flags);
        this.mSelectEvent.reset();

        // try todo if event fails
        if (!item) {
            this.mSelectTodo.params.id = aID;
            if (this.mSelectTodo.step())
                item = this.getTodoFromRow(this.mSelectTodo.row, flags);
            this.mSelectTodo.reset();
        }

        // bail if still not found
        if (!item)
            return null;

        this.getAdditionalDataForItem(item, flags.value);

        item.makeImmutable();

        // cache it
        this.mItemCache[aID] = item;

        return item;
    },

    // write this item's changed data to the db.  olditem may be null
    flushItem: function (item, olditem) {
        // for now, we just delete and insert
        // set up params before transaction
        var oldItemDeleteStmt;
        var newItemType;

        if (olditem) {
            if (olditem instanceof kCalIEvent)
                oldItemDeleteStmt = this.mDeleteEvent;
            else if (olditem instanceof kCalITodo)
                oldItemDeleteStmt = this.mDeleteTodo;

            oldItemDeleteStmt.params.id = olditem.id;
            this.mDeleteAttendees.params.item_id = olditem.id;
            this.mDeleteProperties.params.item_id = olditem.id;
            this.mDeleteRecurrence.params.item_id = olditem.id;
        }

        // the insert params
        var ip;

        if (item instanceof kCalIEvent) {
            newItemType = CAL_ITEM_TYPE_EVENT;
            ip = this.mInsertEvent.params;
        } else if (item instanceof kCalITodo) {
            newItemType = CAL_ITEM_TYPE_TODO;
            ip = this.mInsertTodo.params;
        }

        ip.cal_id = this.mCalId;
        ip.id = item.id;
        ip.time_created = item.creationDate.nativeTime;
        ip.last_modified = item.lastModifiedTime.nativeTime;
        ip.title = item.title;
        ip.priority = item.priority;
        ip.privacy = item.privacy;
        ip.ical_status = item.status;

        function setDateParamHelper(params, entryname, cdt) {
            params[entryname] = cdt.nativeTime;
            params[entryname + "_tz"] = cdt.timezone;
        }

        // build flags up as we go
        var flags = 0;

        if (newItemType == CAL_ITEM_TYPE_EVENT) {
            if (item.isAllDay)
                flags |= CAL_ITEM_FLAG_EVENT_ALLDAY;

            setDateParamHelper(ip, "event_start", item.startDate);
            setDateParamHelper(ip, "event_end", item.endDate);
            // we want this to always be utc
            ip.event_stamp = item.stampTime.nativeTime;
        } else if (newItemType == CAL_ITEM_TYPE_TODO) {
            setDateParamHelper(ip, "todo_entry", item.entryDate);
            setDateParamHelper(ip, "todo_due", item.dueDate);
            setDateParamHelper(ip, "todo_completed", item.completedDate);
            ip.todo_complete = item.percentComplete;
        }

        if (item.hasAlarm) {
            setDateParamHelper(ip, "alarm_time", item.alarmTime);
        }

        // start the transaction
        this.mDB.beginTransaction();
        try {
            // first delete the old item's data
            if (olditem) {
                this.mDeleteRecurrence.execute();
                this.mDeleteAttendees.execute();
                this.mDeleteProperties.execute();
                oldItemDeleteStmt.execute();
            }

            // then add in auxiliary bits (attendees, properties, recurrence)

            var attendees = item.getAttendees({});
            if (attendees && attendees.length > 0) {
                flags |= CAL_ITEM_FLAG_HAS_ATTENDEES;
                for each (var att in attendees) {
                    var ap = this.mInsertAttendee.params;
                    ap.item_id = item.id;
                    ap.attendee_id = att.id;
                    ap.common_name = att.commonName;
                    ap.rsvp = att.rsvp;
                    ap.role = att.role;
                    ap.status = att.participationStatus;
                    ap.type = att.userType;

                    this.mInsertAttendee.execute();
                }
            }

            var propEnumerator = item.propertyEnumerator;
            while (propEnumerator.hasMoreElements()) {
                flags |= CAL_ITEM_FLAG_HAS_PROPERTIES;

                var prop = propEnumerator.getNext().QueryInterface(Components.interfaces.nsIProperty);

                var pp = this.mInsertProperty.params;
                pp.item_id = item.id;
                pp.key = prop.name;
                pp.value = prop.value;

                this.mInsertProperty.execute();
            }

            var rec = item.recurrenceInfo;
            if (rec) {
                flags |= CAL_ITEM_FLAG_HAS_RECURRENCE;

                var ritems = rec.getRecurrenceItems ({});
                for (i in ritems) {
                    var ritem = ritems[i];
                    ap = this.mInsertRecurrence.params;
                    ap.item_id = item.id;
                    ap.recur_index = i;
                    ap.is_negative = ritem.isNegative;
                    if (ritem instanceof kCalIRecurrenceDate) {
                        ritem = ritem.QueryInterface(kCalIRecurrenceDate);
                        ap.recur_type = "x-date";
                        ap.dates = dateToText(ritem.date);

                    } else if (ritem instanceof kCalIRecurrenceDateSet) {
                        ritem = ritem.QueryInterface(kCalIRecurrenceDateSet);
                        ap.recur_type = "x-dateset";

                        var rdates = ritem.getDates({});
                        var datestr = "";
                        for (j in rdates) {
                            if (j != 0)
                                datestr += ",";

                            datestr += dateToText(rdates[j]);
                        }

                        ap.dates = datestr;

                    } else if (ritem instanceof kCalIRecurrenceRule) {
                        ritem = ritem.QueryInterface(kCalIRecurrenceRule);
                        ap.recur_type = ritem.type;

                        if (ritem.isByCount)
                            ap.count = ritem.count;
                        else
                            ap.end_date = ritem.endDate.nativeTime;

                        ap.interval = ritem.interval;

                        var rtypes = ["second",
                                      "minute",
                                      "hour",
                                      "day",
                                      "monthday",
                                      "yearday",
                                      "weekno",
                                      "month",
                                      "setpos"];
                        for (j = 0; j < rtypes.length; j++) {
                            var comp = "BY" + rtypes[i].toUpperCase();
                            var comps = ritem.getComponent(comp, {});
                            if (comps && comps.length > 0) {
                                var compstr = comps.join(",");
                                ap[rtypes[j]] = compstr;
                            }
                        }
                    } else {
                        dump ("XXX Don't know how to serialize recurrence item " + ritem + "!\n");
                    }

                    this.mInsertRecurrence.execute();
                }
            }

            // finally insert the item itself
            ip.flags = flags;
            if (newItemType == CAL_ITEM_TYPE_EVENT)
                this.mInsertEvent.execute();
            else if (newItemType == CAL_ITEM_TYPE_TODO)
                this.mInsertTodo.execute();

            this.mDB.commitTransaction();
        } catch (e) {
            Components.reportError("flushItem DB error: " + this.mDB.lastErrorString);
            this.mDB.rollbackTransaction();
            throw e;
        }

        this.mItemCache[item.id] = item;
    },

    // delete the event with the given uid
    deleteItemById: function (aID) {
        this.mDB.beginTransaction();
        try {
            this.mDeleteAttendees(aID);
            this.mDeleteProperties(aID);
            this.mDeleteRecurrence(aID);
            this.mDeleteEvent(aID);
            this.mDeleteTodo(aID);
            this.mDB.commitTransaction();
        } catch (e) {
            Components.reportError("deleteItemById DB error: " + this.mDB.lastErrorString);
            this.mDB.rollbackTransaction();
            throw e;
        }

        delete this.mItemCache[aID];
    }
}


/****
 **** module registration
 ****/

var calStorageCalendarModule = {
    mCID: Components.ID("{b3eaa1c4-5dfe-4c0a-b62a-b3a514218461}"),
    mContractID: "@mozilla.org/calendar/calendar;1?type=storage",
    
    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.mCID,
                                        "Calendar mozStorage/SQL back-end",
                                        this.mContractID,
                                        fileSpec,
                                        location,
                                        type);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.mCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        if (!kStorageServiceIID)
            throw Components.results.NS_ERROR_NOT_INITIALIZED;

        if (!CalEvent) {
            initCalStorageCalendarComponent();
        }

        return this.mFactory;
    },

    mFactory: {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            return (new calStorageCalendar()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return calStorageCalendarModule;
}



//
// sqlTables generated from schema.sql via makejsschema.pl
//

var sqlTables = {
  cal_calendar_schema_version:
    "	version	INTEGER" +
    "",

  cal_events:
    /* 	REFERENCES cal_calendars.id, */
    "	cal_id		INTEGER, " +
    /*  ItemBase bits */
    "	id		STRING," +
    "	time_created	INTEGER," +
    "	last_modified	INTEGER," +
    "	title		STRING," +
    "	priority	INTEGER," +
    "	privacy		STRING," +
    "	ical_status	STRING," +
    /*  CAL_ITEM_FLAG_PRIVATE = 1 */
    /*  CAL_ITEM_FLAG_HAS_ATTENDEES = 2 */
    /*  CAL_ITEM_FLAG_HAS_PROPERTIES = 4 */
    /*  CAL_ITEM_FLAG_EVENT_ALLDAY = 8 */
    /*  CAL_ITEM_FLAG_HAS_RECURRENCE = 16 */
    "	flags		INTEGER," +
    /*  Event bits */
    "	event_start	INTEGER," +
    "	event_start_tz	VARCHAR," +
    "	event_end	INTEGER," +
    "	event_end_tz	VARCHAR," +
    "	event_stamp	INTEGER," +
    /*  alarm time */
    "	alarm_time	INTEGER," +
    "	alarm_time_tz	VARCHAR" +
    "",

  cal_todos:
    /* 	REFERENCES cal_calendars.id, */
    "	cal_id		INTEGER, " +
    /*  ItemBase bits */
    "	id		STRING," +
    "	time_created	INTEGER," +
    "	last_modified	INTEGER," +
    "	title		STRING," +
    "	priority	INTEGER," +
    "	privacy		STRING," +
    "	ical_status	STRING," +
    /*  CAL_ITEM_FLAG_PRIVATE = 1 */
    /*  CAL_ITEM_FLAG_HAS_ATTENDEES = 2 */
    /*  CAL_ITEM_FLAG_HAS_PROPERTIES = 4 */
    /*  CAL_ITEM_FLAG_EVENT_ALLDAY = 8 */
    /*  CAL_ITEM_FLAG_HAS_RECURRENCE = 16 */
    "	flags		INTEGER," +
    /*  Todo bits */
    /*  date the todo is to be displayed */
    "	todo_entry	INTEGER," +
    "	todo_entry_tz	VARCHAR," +
    /*  date the todo is due */
    "	todo_due	INTEGER," +
    "	todo_due_tz	VARCHAR," +
    /*  date the todo is completed */
    "	todo_completed	INTEGER," +
    "	todo_completed_tz VARCHAR," +
    /*  percent the todo is complete (0-100) */
    "	todo_complete	INTEGER," +
    /*  alarm time */
    "	alarm_time	INTEGER," +
    "	alarm_time_tz	VARCHAR" +
    "",

  cal_attendees:
    "	item_id         STRING," +
    "	attendee_id	STRING," +
    "	common_name	STRING," +
    "	rsvp		INTEGER," +
    "	role		STRING," +
    "	status		STRING," +
    "	type		STRING" +
    "",

  cal_recurrence:
    "	item_id		STRING," +
    /*  the index in the recurrence array of this thing */
    "	recur_index	INTEGER, " +
    /*  values from calIRecurrenceInfo; if null, date-based. */
    "	recur_type	STRING, " +
    "	is_negative	BOOLEAN," +
    /*  */
    /*  these are for date-based recurrence */
    /*  */
    /*  comma-separated list of dates */
    "	dates		STRING," +
    /*  */
    /*  these are for rule-based recurrence */
    /*  */
    "	count		INTEGER," +
    "	end_date	INTEGER," +
    "	interval	INTEGER," +
    /*  components, comma-separated list or null */
    "	second		STRING," +
    "	minute		STRING," +
    "	hour		STRING," +
    "	day		STRING," +
    "	monthday	STRING," +
    "	yearday		STRING," +
    "	weekno		STRING," +
    "	month		STRING," +
    "	setpos		STRING" +
    "",

  cal_properties:
    "	item_id		STRING," +
    "	key		STRING," +
    "	value		BLOB" +
    "",

};

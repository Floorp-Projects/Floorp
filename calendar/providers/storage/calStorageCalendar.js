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

const kCalEventContractID = "@mozilla.org/calendar/event;1";
const kCalEventIID = Components.interfaces.calIEvent;

const kCalTodoContractID = "@mozilla.org/calendar/todo;1";
const kCalTodoIID = Components.interfaces.calITodo;

const kCalDateTimeContractID = "@mozilla.org/calendar/datetime;1";
const kCalDateTimeIID = Components.interfaces.calIDateTime;

// column ids in the base columns
var ITEM_COLUMN_COUNTER = 0;
const ITEM_HASHID = ITEM_COLUMN_COUNTER++;
const ITEM_TIME_CREATED = ITEM_COLUMN_COUNTER++;
const ITEM_LAST_MODIFIED = ITEM_COLUMN_COUNTER++;
const ITEM_TITLE = ITEM_COLUMN_COUNTER++;
const ITEM_PRIORITY = ITEM_COLUMN_COUNTER++;
const ITEM_FLAGS = ITEM_COLUMN_COUNTER++;
const ITEM_LOCATION = ITEM_COLUMN_COUNTER++;
const ITEM_CATEGORIES = ITEM_COLUMN_COUNTER++;
const ITEM_ALARM_DATA = ITEM_COLUMN_COUNTER++;
const ITEM_ALARM_TIME = ITEM_COLUMN_COUNTER++;
const ITEM_RECUR_TYPE = ITEM_COLUMN_COUNTER++;
const ITEM_RECUR_INTERVAL = ITEM_COLUMN_COUNTER++;
const ITEM_RECUR_DATA = ITEM_COLUMN_COUNTER++;
const ITEM_RECUR_END = ITEM_COLUMN_COUNTER++;
const ITEM_RECUR_FLAGS = ITEM_COLUMN_COUNTER++;

// column ids in cal_events
var EV_COLUMN_COUNTER = ITEM_COLUMN_COUNTER;
const EV_TIME_START = EV_COLUMN_COUNTER++;
const EV_TIME_END = EV_COLUMN_COUNTER++;

// column ids in cal_todos
var TODO_COLUMN_COUNTER = ITEM_COLUMN_COUNTER;
const TODO_TIME_ENTRY = TODO_COLUMN_COUNTER++;
const TODO_TIME_DUE = TODO_COLUMN_COUNTER++;
const TODO_TIME_COMPLETED = TODO_COLUMN_COUNTER++;
const TODO_PERCENT_COMPLETE = TODO_COLUMN_COUNTER++;

// helpers
function newDateTime(aPRTime) {
    var t = Components.classes[kCalDateTimeContractID]
                      .createInstance(kCalDateTimeIID);
    t.isUtc = true;
    t.nativeTime = aPRTime;
    return t;
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
    mItemCache: Array(),

    // database statements

    // events
    mSelectEventByHash: null,
    mSelectEventByOid: null,
    mSelectEventsByRange: null,
    mSelectEventsByRangeWithLimit: null,
    mDeleteEventByHash: null,
    mInsertEvent: null,

    // todos
    mSelectTodoByHash: null,
    mSelectTodoByOid: null,
    mSelectTodosByRange: null,
    mSelectTodosByRangeWithLimit: null,
    mDeleteTodoByHash: null,
    mInsertTodo: null,

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
    // nsICalendar interface
    //

    mURI: null,
    // attribute nsIURI uri;
    get uri() { return this.mURI; },
    set uri(aURI) {
        // we can only load once
        if (this.mURI)
            throw Components.results.NS_ERROR_FAILURE;

        // we can only load storage bits from a file
        this.mURI = aURI;
        var fileURL = aURI.QueryInterface(Components.interfaces.nsIFileURL);
        if (!fileURL)
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        // open the database
        var dbService = Components.classes[kStorageServiceContractID]
                                  .getService(kStorageServiceIID);
        this.mDB = dbService.openDatabase (fileURL.file);

        initDB();
    },

    // attribute boolean suppressAlarms;
    get suppressAlarms() { return false; },
    set suppressAlarms(aSuppressAlarms) { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

    // void addObserver( in calIObserver observer, in unsigned long aItemFilter );
    addObserver: function (aObserver, aItemFilter) {
        for (var i = 0; i < this.mObservers.length; i++) {
            if (this.mObservers[i].observer == aObserver &&
                this.mObservers[i].filter == aItemFilter)
            {
                return;
            }
        }

        this.mObservers.push( {observer: aObserver, filter: aItemFilter} );
    },

    // void removeObserver( in calIObserver observer );
    removeObserver: function (aObserver) {
        var newObservers = Array();
        for (var i = 0; i < this.mObservers.length; i++) {
            if (this.mObservers[i].observer != aObserver)
                newObservers.push(this.mObservers[i]);
        }
        this.mObservers = newObservers;
    },

    // void addItem( in calIItemBase aItem, in calIOperationListener aListener );
    addItem: function (aItem, aListener) {
        if (aItem.id == null) {
            // is this an error?  Or should we generate an IID?
            aItem.id = "uuid:" + (new Date()).getTime();
        }
        if (this.mItems[aItem.id] != null) {
            // is this an error?
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.ADD,
                                               aItem.id,
                                               "ID already eists for addItem");
            return;
        }

        // is this item an event?
        var event = aItem.QueryInterface(kCalEventIID);
        if (event) {
            flushEvent (event);
        } else {
            aListener.onOperationComplete (this,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.ADD,
                                           aItem.id,
                                           "Don't know how to add items of the given type");
            return;
        }

        // notify observers
        observeAddItem(aItem);

        // notify the listener
        if (aListener)
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.ADD,
                                           aItem.id,
                                           aItem);
    },

    // void modifyItem( in calIItemBase aItem, in calIOperationListener aListener );
    modifyItem: function (aItem, aListener) {
        if (aItem.id == null)
        {
            // this is definitely an error
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aItem.id,
                                               "ID for modifyItem item is null");
            return;
        }

        // is this item an event?
        var event = aItem.QueryInterface(kCalEventIID);
        if (event) {
            flushEvent (event);
        } else {
            aListener.onOperationComplete (this,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.MODIFY,
                                           aItem.id,
                                           "Don't know how to modify items of the given type");
            return;
        }

        // notify observers
        observeModifyItem(modifiedItem, aItem);

        if (aListener)
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.MODIFY,
                                           aItem.id,
                                           aItem);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aId, aListener) {
        if (aId == null)
        {
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.DELETE,
                                               aId,
                                               "ID is null for deleteItem");
            return;
        }

        // is this item an event?
        var item = getItemByHash (aId);
        var event = item.QueryInterface(kCalEventIID);
        if (event) {
            deleteEvent (aId);
        } else {
            aListener.onOperationComplete (this,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.DELETE,
                                           aItem.id,
                                           "Don't know how to delete items of the given type");
            return;
        }

        // notify observers 
        observeDeleteItem(item);

        if (aListener)
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.DELETE,
                                           aId,
                                           null);

    },

    // void getItem( in string id, in calIOperationListener aListener );
    getItem: function (aId, aListener) {
        if (!aListener)
            return;

        var item = getItemByHash (aId);
        if (!item) {
        }

        var item_iid = null;
        if (item.QueryInterface (kCalEventIID)) {
            item_iid = kCalEventIID;
        } else if (item.QueryInterface (kCalTodoIID)) {
            item_iid = kCalTodoIID;
        } else {
            aListener.onOperationComplete (this,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.GET,
                                           aId,
                                           "Can't deduce item type based on QI");
            return;
        }

        aListener.onGetResult (Components.results.NS_OK,
                               iid,
                               null, 1, [item]);

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
        if (!aListener)
            return;

        const calICalendar = Components.interfaces.calICalendar;
        const calIItemBase = Components.interfaces.calIItemBase;
        const calIEvent = Components.interfaces.calIEvent;
        const calITodo = Components.interfaces.calITodo;
        const calIItemOccurrence = Components.interfaces.calIItemOccurrence;

        var itemsFound = Array();
        var startTime = 0;
        // endTime needs to be the max value a PRTime can be
        var endTime = 0x7fffffffffffffff;
        if (aRangeStart)
            startTime = aRangeStart.utcTime;
        if (aRangeEnd)
            endTime = aRangeEnd.utcTime;

        // only events for now
        if (!(aItemFilter & calICalendar.ITEM_FILTER_TYPE_EVENT)) {
            aListener.onOperationComplete(this,
                                          Components.results.NS_ERROR_FAILURE,
                                          aListener.GET,
                                          null,
                                          "Don't know how to getItems for anything other than events");
            return;
        }

        var asOccurrences = false;
        if (aItemFilter & calICalendar.ITEM_FILTER_CLASS_OCCURRENCES)
            asOccurrences = true;

        var events = getEventsByRange(startTime, endTime, aCount, asOccurrences);

        if (aListener)
            aListener.onGetComplete (this,
                                     Components.results.NS_OK,
                                     aItemType,
                                     null,
                                     itemsFound.length,
                                     itemsFound);
    },

    //
    // Helper functions
    //
    observeLoad: function () {
        for (var i = 0; i < mObservers.length; i++)
            mObservers[i].onLoad ();
    },

    observeBatchChange: function (aNewBatchMode) {
        for (var i = 0; i < mObservers.length; i++) {
            if (aNewBatchMode)
                mObservers[i].onStartBatch ();
            else
                mObservers[i].onEndBatch ();
        }
    },

    observeAddItem: function (aItem) {
        for (var i = 0; i < mObservers.length; i++)
            mObservers[i].onAddItem (aItem);
    },

    observeModifyItem: function (aOldItem, aNewItem) {
        for (var i = 0; i < mObservers.length; i++)
            mObservers[i].onModifyItem (aOldItem, aNewItem);
    },

    observeDeleteItem: function (aDeletedItem) {
        for (var i = 0; i < mObservers.length; i++)
            mObservers[i].onDeleteItem (aDeletedItem);
    },

    // database initialization
    // assumes mDB is valid
    initDB: function () {
        // these are columns that appear in all concrete items; SQLite could use
        // an "extends" thing like postgresql has
        const item_base_columns =
            "hashid, time_created, last_modified, " +
            "title, priority, flags, location, categories, " +
            "alarm_data, alarm_time, " +
            "recur_type, recur_interval, recur_data, recur_end, recur_flags";
        try {
            mDB.createTable ("cal_events",
                             item_base_columns + ", " +
                             "time_start, time_end");
        } catch (e) { }
        try {
            mDB.createTable ("cal_todos",
                             item_base_columns + ", " +
                             "time_entry, time_due, time_completed, percent_complete");
        } catch (e) { }
        try {
            mDB.createTable ("cal_recur_exceptions",
                             "item_id, skip_date");
        } catch (e) { }
        try {
            mDB.createTable ("cal_event_attendees",
                             "event_id, attendee, attendee_status");
        } catch (e) { }
        try {
            mDB.createTable ("cal_event_attachments",
                             "event_id, attachment_type, attachment_data");

        } catch (e) { }
        try {
            mDB.executeSimpleSQL ("CREATE INDEX cal_events_hash_idx ON cal_events.hashid");
        } catch (e) { }

        var qs;

        // event statements

        mSelectEventByHash = mDB.createStatement ("SELECT * FROM cal_events WHERE hashid = ? LIMIT 1");
        mSelectEventByOid = mDB.createStatement ("SELECT * FROM cal_events WHERE oid = ? LIMIT 1");
        // this needs to have the range bound like this: start, end
        mSelectEventsByRange = mDB.createStatement ("SELECT * FROM cal_events WHERE (time_end >= ? AND time_start <= ?)");
        mSelectEventsByRangeAndLimit = mDB.createStatement ("SELECT * FROM cal_events WHERE (time_end >= ? AND time_start <= ?) LIMIT ?");
        mDeleteEventByHash = mDB.createStatement ("DELETE FROM cal_events WHERE hashid = ?");
        qs = "?"; for (var i = 1; i < EV_COLUMN_COUNTER; i++) qs += ",?";
        mInsertEvent = mDB.createStatement ("INSERT INTO cal_events VALUES (" + qs + ")");

        // todo statements
        mSelectTodoByHash = mDB.createStatement ("SELECT * FROM cal_todos WHERE hashid = ? LIMIT 1");
        mSelectTodoByOid = mDB.createStatement ("SELECT * from cal_todos WHERE oid = ? LIMIT 1");
        mSelectTodosByRange = mDB.createStatement ("SELECT * FROM cal_todos WHERE (time_entry >= ? AND time_entry <= time_end)");
        mSelectTodosByRangeAndLimit = mDB.createStatement ("SELECT * FROM cal_todos WHERE (time_entry >= ? AND time_entry <= time_end) LIMIT ?");
        mDeleteTodoByHash = mDB.createStatement ("DELETE FROM cal_todos WHERE hashid = ?");
        qs = "?"; for (var i = 1; i < TODO_COLUMN_COUNTER; i++) qs += ",?";
        mInsertTodo = mDB.createStatement ("INSERT INTO cal_todos VALUES (" + qs + ")");
    },

    // database helper functions

    // read in the common ItemBase attributes from aDBRow, and stick
    // them on aItemBase
    getItemBaseFromRow: function (aDBRow, aItemBase) {
        aItemBase.title = aDBRow.getAsString(ITEM_TITLE);
        aItemBase.description = aDBRow.getAsString(ITEM_DESCRIPTION);
        aItemBase.location = aDBRow.getAsString(ITEM_LOCATION);
        aItemBase.categories = aDBRow.getAsString(ITEM_CATEGORIES);

        aItemBase.id = aDBRow.getAsString(ITEM_HASHID);
        aItemBase.parent = this;
    },

    // create a new event and read in its attributes from aDBRow
    getEventFromRow: function (aDBRow) {
        // create a new generic event
        var e = Components.classes[kCalEventContractID]
        .createInstance(kCalEventIID);

        getItemBaseFromRow (aDBRow, e);

        // XXX do I need getAsPRTime?
        e.startDate = newDateTime (aDBRow.getAsInt64(EV_TIME_START));
        e.endDate = newDateTime (aDBRow.getAsInt64(EV_TIME_END));

        return e;
    },

    // create a new todo and read in its attributes from aDBRow
    getTodoFromRow: function (aDBRow) {
        // create a new generic todo
        var todo = Components.classes[kCalTodoContractID]
        .createInstance(kCalTodoIID);

        getItemBaseFromRow (aDBRow, todo);

        todo.entryTime = newDateTime (aDBRow.getAsInt64(TODO_TIME_ENTRY));
        todo.dueDate = newDateTime (aDBRow.getAsInt64(TODO_TIME_DUE));
        todo.completedDate = newDateTime (aDBRow.getAsInt64(TODO_TIME_COMPLETED));
        todo.percent_complete = aDBRow.getAsInt32(TODO_PERCENT_COMPLETE);

        return todo;
    },

    //
    // get an todo from the db or cache with the given ID
    //
    getItemByHash: function (aID) {
        // cached?
        if (mItemHash[aID] != null)
            return mItemHash[aID];

        var retval = null;

        // not cached; need to read from the db

        // try event first
        mSelectEventByHash.bindCStringParameter (0, aID);
        if (mSelectEventByHash.executeStep()) {
            var e = getEventFromRow (mSelectEventByHash);
            retval = e;
        }

        if (retval == null) {
            // not found; try a todo
            mSelectTodoByHash.bindCStringParameter (0, aID);
            if (mSelectTodoByHash.executeStep()) {
                var e = getTodoFromRow (mSelectTodoByHash);
                retval = e;
            }
        }

        mSelectEventByHash.reset();

        if (retval)
            mItemHash[aID] = retval;
        return retval;
    },

    // get a list of events from the db in the given range
    getEventsByRange: function (aRangeStart, aRangeEnd, aCount, aAsOccurrences) {
        var stmt = mSelectEventsByRange;
        if (aCount)
            stmt = mSelectEventsByRangeAndLimit;

        stmt.bindInt64Parameter(0, aRangeStart);
        stmt.bindInt64Parameter(1, aRangeEnd);
        if (aCount)
            stmt.bindInt32Parameter(3, aCount);

        var events = Array();
        while (stmt.executeStep()) {
            var e = getEventFromRow(stmt);
            mItemHash[e.id] = e;

            if (aAsOccurrences) {
                var rec = Components.classes["@mozilla.org/calendar/item-occurrence;1"].createInstance(calIItemOccurrence);
                rec.wrappedJSObject.item = e;
                rec.wrappedJSObject.occurrenceStartDate = e.startDate;
                rec.wrappedJSObject.occurrenceEndDate = e.endDate;

                events.push (rec);
            } else {
                events.push (e);
            }
        }

        return events;
    },

    // write this event's changed data to the db
    flushEvent: function (aEvent) {
        // set up statements before executing the transaction
        mDeleteEventByHash.bindCStringParameter (0, aEvent.id);

        mInsertEvent.bindCStringParameter (ITEM_HASHID, aEvent.id);

        mInsertEvent.bindInt64Parameter (EV_TIME_START, aEvent.start.utcTime);
        mInsertEvent.bindInt64Parameter (EV_TIME_END, aEvent.end.utcTime);

        mInsertEvent.bindStringParameter (ITEM_TITLE, aEvent.title);
        mInsertEvent.bindStringParameter (ITEM_DESCRIPTION, aEvent.description);
        mInsertEvent.bindStringParameter (ITEM_LOCATION, aEvent.location);
        mInsertEvent.bindStringParameter (ITEM_CATEGORIES, aEvent.categories);

        mDB.beginTransaction();
        mDeleteEventByHash.execute();
        mInsertEvent.execute();
        mDB.commitTransaction();

        mDeleteEventByHash.reset();
        mInsertEvent.reset();

        mItemHash[aEvent.id] = aEvent;
    },

    // delete the event with the given uid
    deleteEvent: function (aID) {
        mDeleteEventByHash.bindCStringParameter (0, aID);
        mDeleteEventByHash.execute ();
        mDeleteEventByHash.reset ();

        delete mItemHash[aID];
    },
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

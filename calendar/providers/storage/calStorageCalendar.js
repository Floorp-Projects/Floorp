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

const kCalEventContractID = "@mozilla.org/calendar/event;1";
const kCalIEvent = Components.interfaces.calIEvent;
const CalEvent = new Components.Constructor(kCalEventContractID, kCalIEvent);

const kCalTodoContractID = "@mozilla.org/calendar/todo;1";
const kCalITodo = Components.interfaces.calITodo;
const CalTodo = new Components.Constructor(kCalTodoContractID, kCalITodo);

const kCalDateTimeContractID = "@mozilla.org/calendar/datetime;1";
const kCalIDateTime = Components.interfaces.calIDateTime;
const CalDateTime = new Components.Constructor(kCalDateTimeContractID, kCalIDateTime);

const kCalAttendeeContractID = "@mozilla.org/calendar/attendee;1";
const kCalIAttendee = Components.interfaces.calIAttendee;
const CalAttendee = new Components.Constructor(kCalAttendeeContractID, kCalIAttendee);

const kCalItemOccurrenceContractID = "@mozilla.org/calendar/item-occurrence;1";
const kCalIItemOccurrence = Components.interfaces.calIItemOccurrence;
const CalItemOccurrence = new Components.Constructor(kCalItemOccurrenceContractID, kCalIItemOccurrence);

const kMozStorageStatementWrapperContractID = "@mozilla.org/storage/statement-wrapper;1";
const kMozStorageStatementWrapperIID = Components.interfaces.mozIStorageStatementWrapper;
if (kMozStorageStatementWrapperIID) {
    const MozStorageStatementWrapper = new Components.Constructor(kMozStorageStatementWrapperContractID, kMozStorageStatementWrapperIID);
 } else {
    dump("*** mozStorage not available, calendar/storage provider will not function\n");
 }

const CAL_ITEM_TYPE_EVENT = 0;
const CAL_ITEM_TYPE_TODO = 1;

// bitmasks
const CAL_ITEM_FLAG_PRIVATE = 1;
const CAL_ITEM_FLAG_HAS_ATTENDEES = 2;
const CAL_ITEM_FLAG_HAS_PROPERTIES = 4;
const CAL_ITEM_FLAG_EVENT_ALLDAY = 8;

//
// Storage helpers
//

function createStatement (dbconn, sql) {
    var stmt = dbconn.createStatement(sql);
    var wrapper = MozStorageStatementWrapper();
    wrapper.initialize(stmt);
    return wrapper;
}

// 
// other helpers
//

function newDateTime(aPRTime) {
    var t = CalDateTime();
    t.isUtc = true;
    t.nativeTime = aPRTime;
    return t;
}

function makeOccurrence(item, start, end)
{
    var occ = CalItemOccurrence();
    // XXX poor form
    occ.wrappedJSObject.item = item;
    occ.wrappedJSObject.occurrenceStartDate = item.startDate;
    occ.wrappedJSObject.occurrenceEndDate = item.endDate;
    return occ;
}

//
// calStorageCalendar
//

function calStorageCalendar(cal_id) {
    this.wrappedJSObject = this;

    if (cal_id)
        this.mCalId = cal_id;
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

    mURI: null,
    // attribute nsIURI uri;
    get uri() { return this.mURI; },
    set uri(aURI) {
        // we can only load once
        if (this.mURI)
            throw Components.results.NS_ERROR_FAILURE;

        // we can only load storage bits from a file
        var fileURL = aURI.QueryInterface(Components.interfaces.nsIFileURL);
        if (!fileURL)
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        // open the database
        var dbService = Components.classes[kStorageServiceContractID].getService(kStorageServiceIID);
        this.mDB = dbService.openDatabase (fileURL.file);
        this.mDBTwo = dbService.openDatabase (fileURL.file);

        this.initDB();

        this.mURI = aURI;
    },

    // attribute boolean readOnly;
    get readOnly() { return false; },
    set readOnly() { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

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

        newItem = aItem.clone();
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
        this.observeModifyItem(modifiedItem, newItem);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aItem, aListener) {
        if (aItem.id == null) {
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.DELETE,
                                               aId,
                                               "ID is null for deleteItem");
            return;
        }

        this.deleteItem(aItem.id);

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
                                           aID,
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

        aListener.onGetResult (Components.results.NS_OK,
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
        if (!aListener)
            return;

        var itemsFound = Array();
        var startTime = 0;
        // endTime needs to be the max value a PRTime can be
        var endTime = 0x7fffffffffffffff;
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

        var stmt = this.mSelectItemsByRange;
        stmt.reset();
        var sp = stmt.params;
        sp.cal_id = this.mCalId;
        sp.event_start = startTime;
        sp.event_end = endTime;
        sp.todo_start = startTime;
        sp.todo_end = endTime;

        var count = 0;
        while (stmt.step()) {
            var iid;
            if (stmt.row.item_type == CAL_ITEM_TYPE_EVENT) {
                if (!wantEvents)
                    continue;
                iid = kCalIEvent;
            } else if (stmt.row.item_type == CAL_ITEM_TYPE_TODO) {
                if (!wantTodos)
                    continue;
                iid = kCalITodo;
            } else {
                // errrr..
                dump ("***** Unknown item type in db: " + stmt.row.item_type + "\n");
                continue;
            }
            var item = this.getItemFromRow(stmt.row);
            // uses mDBTwo, so can be interleaved
            this.getAdditionalDataForItem(item);

            item.makeImmutable();

            if (asOccurrences) {
                if (iid == kCalIEvent) {
                    item = makeOccurrence(iid, item.startDate, item.endDate);
                } else if (iid == kCalITodo) {
                    item = makeOccurrence(iid, item.entryTime, item.entryTime);
                }
                iid = kCalIItemOccurrence;
            }
            
            aListener.onGetResult (this,
                                   Components.results.NS_OK,
                                   iid, null,
                                   1, [item]);

            count++;
            if (aCount && count == aCount)
                break;
        }

        this.mSelectItemsByRange.reset();

        aListener.onOperationComplete (this,
                                       Components.results.NS_OK,
                                       aListener.GET,
                                       null,
                                       null);
    },

    //
    // Helper functions
    //
    observeLoad: function () {
        for (var i = 0; i < this.mObservers.length; i++)
            this.mObservers[i].onLoad ();
    },

    observeBatchChange: function (aNewBatchMode) {
        for (var i = 0; i < this.mObservers.length; i++) {
            if (aNewBatchMode)
                this.mObservers[i].onStartBatch ();
            else
                this.mObservers[i].onEndBatch ();
        }
    },

    observeAddItem: function (aItem) {
        for (var i = 0; i < this.mObservers.length; i++)
            this.mObservers[i].onAddItem (aItem);
    },

    observeModifyItem: function (aOldItem, aNewItem) {
        for (var i = 0; i < this.mObservers.length; i++)
            this.mObservers[i].onModifyItem (aOldItem, aNewItem);
    },

    observeDeleteItem: function (aDeletedItem) {
        for (var i = 0; i < this.mObservers.length; i++)
            this.mObservers[i].onDeleteItem (aDeletedItem);
    },

    //
    // database handling
    //

    // initialize the database schema.
    // needs to do some version checking
    initDBSchema: function () {
        dump ("cal_calendars\n");
        this.mDB.createTable ("cal_calendars", sqlTables.cal_calendars);
        dump ("cal_items\n");
        this.mDB.createTable ("cal_items", sqlTables.cal_items);
        dump ("cal_attendees\n");
        this.mDB.createTable ("cal_attendees", sqlTables.cal_attendees);
        dump ("cal_alarms\n");
        this.mDB.createTable ("cal_alarms", sqlTables.cal_alarms);
        dump ("cal_recurrence\n");
        this.mDB.createTable ("cal_recurrence", sqlTables.cal_recurrence);
        dump ("cal_properties\n");
        this.mDB.createTable ("cal_properties", sqlTables.cal_properties);
    },

    // database initialization
    // assumes mDB is valid
    initDB: function () {
        try {
            this.initDBSchema();
        } catch (e) {
            dump ("initDBSchema error: " + this.mDB.lastErrorString + "\n");
            // ignored
        }

        // select statements
        this.mSelectItem = createStatement (
            this.mDB,
            "SELECT * FROM cal_items " +
            "WHERE id = :id AND cal_id = :cal_id "+
            "LIMIT 1"
            );

        this.mSelectItemsByRange = createStatement(
            this.mDB,
            "SELECT * FROM cal_items " +
            "WHERE (item_type = 0 AND event_end >= :event_start AND event_start <= :event_end) " +
            "   OR (item_type = 1 AND todo_entry >= :todo_start AND todo_entry <= :todo_end) " +
            "  AND cal_id = :cal_id"
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

        // insert statements
        this.mInsertItem = createStatement (
            this.mDB,
            "INSERT INTO cal_items " +
            "  (cal_id, item_type, id, time_created, last_modified, " +
            "   title, priority, privacy, ical_method, ical_status, flags, " +
            "   event_start, event_end, event_stamp, todo_entry, todo_due, " +
            "   todo_completed, todo_complete) " +
            "VALUES (:cal_id, :item_type, :id, :time_created, :last_modified, " +
            "        :title, :priority, :privacy, :ical_method, :ical_status, :flags, " +
            "        :event_start, :event_end, :event_stamp, :todo_entry, :todo_due, " +
            "        :todo_completed, :todo_complete)"
            );
        this.mInsertAlarm = createStatement (
            this.mDB,
            "INSERT INTO cal_alarms (alarm_data) " +
            "VALUES (:alarm_data)"
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
//        this.mInsertRecurrence = createStatement
//        (this.mDB,
//         "INSERT INTO cal_recurrence;");

        // delete statements
        this.mDeleteItem = createStatement (
            this.mDB,
            "DELETE FROM cal_items WHERE id = :id AND cal_id = :cal_id"
            );
        this.mDeleteAttendees = createStatement (
            this.mDB,
            "DELETE FROM cal_attendees WHERE item_id = :item_id"
            );
        this.mDeleteProperties = createStatement (
            this.mDB,
            "DELETE FROM cal_properties WHERE item_id = :item_id"
            );
    },


    // database helper functions

    // read in the common ItemBase attributes from aDBRow, and stick
    // them on aItemBase
    getItemFromRow: function (row) {
        var item;

        if (row.cal_id != this.mCalId) {
            // the row selected doesn't belong to this calendar, hmm.
            
            throw Components.results.NS_ERROR_FAILURE;
        }

        // create a new item based on the type from the db
        if (row.item_type == CAL_ITEM_TYPE_EVENT)
            item = CalEvent();
        else if (row.item_type == CAL_ITEM_TYPE_TODO)
            item = CalTodo();
        else
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        // fill in common bits
        item.creationDate = newDateTime(row.time_created);
        item.lastModifiedTime = newDateTime(row.last_modified);
        item.parent = this;
        item.id = row.id;
        item.title = row.title;
        item.priority = row.priority;
        item.isPrivate = ((row.flags & CAL_ITEM_FLAG_PRIVATE) != 0);
        item.method = row.ical_method;
        item.status = row.ical_status;

        if (row.item_type == CAL_ITEM_TYPE_EVENT) {
            item.startDate = newDateTime(row.event_start);
            item.endDate = newDateTime(row.event_end);
            item.stampDate = newDateTime(row.event_stamp);
            item.isAllDay = ((row.flags & CAL_ITEM_FLAG_EVENT_ALLDAY) != 0);
        } else if (row.item_type == CAL_ITEM_TYPE_TODO) {
            item.entryTime = newDateTime(row.todo_entry);
            item.dueDate = newDateTime(row.todo_due);
            item.completedDate = newDateTime(row.todo_complete);
            item.percentComplete = row.todo_complete;
        }

        // we need a way to do this without using wrappedJSObject
        item.wrappedJSObject.storage_calendar_id = row.id;

        return item;
    },

    // after we get the base item, we need to check if we need to pull in
    // any extra data from other tables.  We do that here.

    // note that we use mDBTwo for this, so this can be run while a
    // select is executing; don't use any statements that aren't
    // against mDBTwo in here!
    
    getAdditionalDataForItem: function (item) {
        if (item.flags & CAL_ITEM_FLAG_HAS_ATTENDEES) {
            this.mSelectAttendeesForItem.params.event_id = item.id;
            while (this.mSelectAttendeesForItem.step()) {
                var attendee = this.mNewAttendeeFromRow(this.mSelectAttendeesForItem.row);
                item.addAttendee(attendee);
            }
        }

        if (item.flags & CAL_ITEM_FLAG_HAS_PROPERTIES) {
            this.mSelectPropertiesForItem.params.event_id = item.id;
            // ...
        }
    },

    newAttendeeFromRow: function (row) {
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
        this.mSelectItem.params.id = aID;
        this.mSelectItem.params.cal_id = this.mCalId;
        if (!this.mSelectItem.step()) {
            // not found
            return null;
        }

        var item = this.getItemFromRow(this.mSelectItem.row);
        this.getAdditionalDataForItem(item);

        item.makeImmutable();

        // cache it
        this.mItemCache[aID] = item;

        return item;
    },

    // write this item's changed data to the db.  olditem may be null
    flushItem: function (item, olditem) {
        // for now, we just delete and insert
        // set up params before transaction
        this.mDeleteItem.params.id = item.id;
        this.mDeleteItem.params.cal_id = this.mCalId;
        this.mDeleteAttendees.params.item_id = item.id;
        this.mDeleteProperties.params.item_id = item.id;

        var ip = this.mInsertItem.params;
        ip.cal_id = this.mCalId;
        ip.id = item.id;
        ip.time_created = item.creationDate.jsDate;
        ip.last_modified = item.lastModifiedTime.jsDate;
        ip.title = item.title;
        ip.priority = item.priority;
        ip.privacy = (item.isPrivate ? 1 : 0);

        ip.ical_method = item.method;
        ip.ical_status = item.status;

        var flags = 0;
        if (item instanceof kCalIEvent) {
            ip.item_type = CAL_ITEM_TYPE_EVENT;

            if (item.isAllDay)
                flags |= CAL_ITEM_FLAG_EVENT_ALLDAY;

            ip.event_start = item.startDate.jsDate;
            ip.event_end = item.endDate.jsDate;
            ip.event_stamp = item.stampDate.jsDate;
        } else if (item instanceof kCalITodo) {
            ip.item_type = CAL_ITEM_TYPE_TODO;

            ip.todo_entry = item.entryTime.jsDate;
            ip.todo_due = item.dueDate.jsDate;
            ip.todo_completed = item.completedDate.jsDate;
            ip.todo_complete = item.percentComplete;
        } else {
            // XXX fixme error
            throw Components.results.NS_ERROR_FAILURE;
        }

        // start the transaction
        this.mDB.beginTransaction();
        try {
            this.mDeleteAttendees.step();
            this.mDeleteProperties.step();
            this.mDeleteItem.step();

            var attendees = item.getAttendees({});
            if (attendees && attendees.length > 0) {
                flags |= CAL_ITEM_FLAG_HAS_ATTENDEES;
                for (var i = 0; i < attendees.length; i++) {
                    var a = attendees[i];
                    var ap = this.mInsertAttendee.params;
                    ap.item_id = item.id;
                    ap.attendee_id = a.id;
                    ap.common_name = a.commonName;
                    ap.rsvp = a.rsvp;
                    ap.role = a.role;
                    ap.status = a.status;
                    ap.type = a.type;

                    this.mInsertAttendee.step();
                }
            }

            ip.flags = flags;
            this.mInsertItem.step();
            this.mDB.commitTransaction();
        } catch (e) {
            dump ("EXCEPTION; error is: " + this.mDB.lastErrorString + "\n");
            dump (e);
            this.mDB.rollbackTransaction();
        }

        this.mItemCache[item.id] = item;
    },

    // delete the event with the given uid
    deleteItem: function (aID) {
        this.mDeleteItem.params.id = aID;
        this.mDeleteItem.params.cal_id = this.mCalId;
        this.mDeleteAttendees.params.item_id = aID;
        this.mDeleteProperties.params.id = aID;

        this.mDB.beginTransaction();
        try {
            this.mDeleteAttendees.step();
            this.mDeleteProperties.step();
            this.mDeleteItem.step();
            this.mDB.commitTransaction();
        } catch (e) {
            dump ("EXCEPTION; error is: " + this.mDB.lastErrorString + "\n");
            dump (e);
            this.mDB.rollbackTransaction();
        }

        delete this.mItemCache[aID];
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

        if (!kStorageServiceIID)
            throw Components.results.NS_ERROR_NOT_INITIALIZED;

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
  cal_calendars:
    "	id	INTEGER PRIMARY KEY," +
    "	name	STRING" +
    "",

  cal_items:
    "	cal_id		INTEGER, " +
    "	item_type	INTEGER," +
    "	id		STRING," +
    "	time_created	INTEGER," +
    "	last_modified	INTEGER," +
    "	title		STRING," +
    "	priority	INTEGER," +
    "	privacy		INTEGER, " +
    "	ical_method	INTEGER," +
    "	ical_status	INTEGER," +
    "	flags		INTEGER," +
    "	event_start	INTEGER," +
    "	event_end	INTEGER," +
    "	event_stamp	INTEGER," +
    "	todo_entry	INTEGER," +
    "	todo_due	INTEGER," +
    "	todo_completed	INTEGER," +
    "	todo_complete	INTEGER," +
    "	alarm_id	INTEGER, " +
    "	recurrence_id	INTEGER  " +
    "",

  cal_attendees:
    "	item_id         STRING," +
    "	attendee_id	STRING," +
    "	common_name	STRING," +
    "	rsvp		INTEGER," +
    "	role		INTEGER," +
    "	status		INTEGER," +
    "	type		INTEGER" +
    "",

  cal_alarms:
    "	id		INTEGER PRIMARY KEY," +
    "	alarm_data	BLOB" +
    "",

  cal_recurrence:
    "	id		INTEGER PRIMARY KEY" +
    "",

  cal_properties:
    "	item_id		STRING," +
    "	key		STRING," +
    "	value		BLOB" +
    "",

};

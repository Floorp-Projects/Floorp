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
 * Portions created by the Initial Developer are Copyright (C) 2005, 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Joey Minta <jminta@gmail.com>
 *   Dan Mosedale <dan.mosedale@oracle.com>
 *   Thomas Benisch <thomas.benisch@sun.com>
 *   Matthew Willis <lilmatt@mozilla.com>
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

const kCalAttendeeContractID = "@mozilla.org/calendar/attendee;1";
const kCalIAttendee = Components.interfaces.calIAttendee;
var CalAttendee;

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
const CAL_ITEM_FLAG_HAS_EXCEPTIONS = 32;

const USECS_PER_SECOND = 1000000;

function initCalStorageCalendarComponent() {
    CalAttendee = new Components.Constructor(kCalAttendeeContractID, kCalIAttendee);
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
        Components.utils.reportError(
            "mozStorage exception: createStatement failed, statement: '" + 
            sql + "', error: '" + dbconn.lastErrorString + "'");
    }

    return null;
}

function textToDate(d) {
    var dval;
    var tz = "UTC";

    if (d[0] == 'Z') {
        var strs = d.substr(2).split(":");
        dval = parseInt(strs[0]);
        tz = strs[1].replace(/%:/g, ":").replace(/%%/g, "%");
    } else {
        dval = parseInt(d.substr(2));
    }

    var date;
    if (d[0] == 'U' || d[0] == 'Z') {
        date = newDateTime(dval, tz);
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
    var tz = null;
    if (d.timezone != "floating") {
        if (d.timezone == "UTC") {
            datestr = "U";
        } else {
            datestr = "Z";
            tz = d.timezone;
        }
    } else {
        datestr = "L";
    }

    if (d.isDate) {
        datestr += "D";
    } else {
        datestr += "T";
    }

    datestr += d.nativeTime;

    if (tz) {
        // replace '%' with '%%', then replace ':' with '%:'
        tz = tz.replace(/%/g, "%%");
        tz = tz.replace(/:/g, "%:");
        datestr += ":" + tz;
    }
    return datestr;
}

// 
// other helpers
//

function newDateTime(aNativeTime, aTimezone) {
    var t = createDateTime();
    t.nativeTime = aNativeTime;
    if (aTimezone && aTimezone != "floating") {
        t = t.getInTimezone(aTimezone);
    } else {
        t.timezone = "floating";
    }

    return t;
}

//
// calStorageCalendar
//

function calStorageCalendar() {
    this.wrappedJSObject = this;
    this.mObservers = new Array();
    this.mItemCache = new Array();
}

calStorageCalendar.prototype = {
    //
    // private members
    //
    mDB: null,
    mDBTwo: null,
    mCalId: 0,

    //
    // nsISupports interface
    // 
    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calICalendarProvider) &&
            !aIID.equals(Components.interfaces.calICalendar))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    //
    // calICalendarProvider interface
    //
    get prefChromeOverlay() {
        return null;
    },

    get displayName() {
        return calGetString("calendar", "storageName");
    },

    createCalendar: function stor_createCal() {
        throw NS_ERROR_NOT_IMPLEMENTED;
    },

    deleteCalendar: function stor_deleteCal(cal, listener) {
        cal = cal.wrappedJSObject;

        for (var i in this.mDeleteEventExtras) {
            this.mDeleteEventExtras[i].params.cal_id = cal.mCalId;
            this.mDeleteEventExtras[i].execute();
        }

        for (var i in this.mDeleteTodoExtras) {
            this.mDeleteTodoExtras[i].params.cal_id = cal.mCalId;
            this.mDeleteTodoExtras[i].execute();
        }

        this.mDeleteAllEvents.params.cal_id = cal.mCalId;
        this.mDeleteAllEvents.execute();

        this.mDeleteAllTodos.params.cal_id = cal.mCalId;
        this.mDeleteAllTodos.execute();

        try {
            listener.onDeleteCalendar(cal, Components.results.NS_OK, null);
        } catch (ex) {
        }
    },

    //
    // calICalendar interface
    //
    
    // attribute AUTF8String id;
    mID: null,
    get id() {
        return this.mID;
    },
    set id(id) {
        if (this.mID)
            throw Components.results.NS_ERROR_ALREADY_INITIALIZED;
        return (this.mID = id);
    },

    // attribute AUTF8String name;
    get name() {
        return getCalendarManager().getCalendarPref(this, "NAME");
    },
    set name(name) {
        getCalendarManager().setCalendarPref(this, "NAME", name);
    },
    // readonly attribute AUTF8String type;
    get type() { return "storage"; },

    mReadOnly: false,

    get readOnly() { 
        return this.mReadOnly;
    },
    set readOnly(bool) {
        this.mReadOnly = bool;
    },

    get canRefresh() {
        return false;
    },

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
	    if ( "getProfileStorage" in dbService ) {
	      // 1.8 branch
	      this.mDB = dbService.getProfileStorage("profile");
	      this.mDBTwo = dbService.getProfileStorage("profile");
	    } else {
	      // trunk 
	      this.mDB = dbService.openSpecialDatabase("profile");
	      this.mDBTwo = dbService.openSpecialDatabase("profile");
	    }
	}

        this.initDB();

        this.mCalId = id;
        this.mURI = aURI;
    },

    refresh: function() {
        // no-op
    },

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
        var newItem = aItem.clone();
        return this.adoptItem(newItem, aListener);
    },

    // void adoptItem( in calIItemBase aItem, in calIOperationListener aListener );
    adoptItem: function (aItem, aListener) {
        if (this.readOnly) 
            throw Components.interfaces.calIErrors.CAL_IS_READONLY;
        // Ensure that we're looking at the base item
        // if we were given an occurrence.  Later we can
        // optimize this.
        if (aItem.parentItem != aItem) {
            aItem.parentItem.recurrenceInfo.modifyException(aItem);
        }
        aItem = aItem.parentItem;

        if (aItem.id == null) {
            // is this an error?  Or should we generate an IID?
            aItem.id = getUUID();
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

        aItem.calendar = this;
        aItem.generation = 1;
        aItem.makeImmutable();

        this.flushItem (aItem, null);

        // notify the listener
        if (aListener)
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.ADD,
                                           aItem.id,
                                           aItem);

        // notify observers
        this.observeAddItem(aItem);
    },

    // void modifyItem( in calIItemBase aNewItem, in calIItemBase aOldItem, in calIOperationListener aListener );
    modifyItem: function (aNewItem, aOldItem, aListener) {
        if (this.readOnly) 
            throw Components.interfaces.calIErrors.CAL_IS_READONLY;
        function reportError(errId, errStr) {
            if (aListener)
                aListener.onOperationComplete (this,
                                               errId ? errId : Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aNewItem.id,
                                               errStr);
        }

        if (aNewItem.id == null) {
            // this is definitely an error
            reportError (null, "ID for modifyItem item is null");
            return;
        }

        // Ensure that we're looking at the base item
        // if we were given an occurrence.  Later we can
        // optimize this.
        if (aNewItem.parentItem != aNewItem) {
            aNewItem.parentItem.recurrenceInfo.modifyException(aNewItem);
        }

        aNewItem = aNewItem.parentItem;

        // get the old item
        var olditem = this.getItemById(aOldItem.id);
        if (!olditem) {
            // no old item found?  should be using addItem, then.
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aNewItem.id,
                                               "ID does not already exist for modifyItem");
            return;
        }

        if (aOldItem.generation != aNewItem.generation) {
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aNewItem.id,
                                               "generation too old for for modifyItem");
            return;
        }

        var modifiedItem = aNewItem.clone();
        modifiedItem.generation += 1;
        modifiedItem.makeImmutable();

        this.flushItem (modifiedItem, aOldItem);

        if (aListener)
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.MODIFY,
                                           modifiedItem.id,
                                           modifiedItem);

        // notify observers
        this.observeModifyItem(modifiedItem, aOldItem);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aItem, aListener) {
        if (this.readOnly) 
            throw Components.interfaces.calIErrors.CAL_IS_READONLY;
        if (aItem.parentItem != aItem) {
            aItem.parentItem.recurrenceInfo.removeExceptionFor(aItem.recurrenceId);
            return;
        }

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
        if (item instanceof Ci.calIEvent)
            item_iid = Ci.calIEvent;
        else if (item instanceof Ci.calITodo)
            item_iid = Ci.calITodo;
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

        var wantCompletedItems = ((aItemFilter & kCalICalendar.ITEM_FILTER_COMPLETED_YES) != 0);
        var wantNotCompletedItems = ((aItemFilter & kCalICalendar.ITEM_FILTER_COMPLETED_NO) != 0);
        
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
        function handleResultItem(item, flags, theIID) {
            self.getAdditionalDataForItem(item, flags);
            item.makeImmutable();

            var expandedItems;

            if (asOccurrences && item.recurrenceInfo) {
                expandedItems = item.recurrenceInfo.getOccurrences (aRangeStart, aRangeEnd, 0, {});
            } else {
                expandedItems = [ item ];
            }

            queueItems (expandedItems, theIID);

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

        // First fetch all the events
        if (wantEvents) {
            // this will contain a lookup table of item ids that we've already dealt with,
            // if we have recurrence to care about.
            var handledRecurringEvents = { };
            var sp;             // stmt params

            var resultItems = [];

            // first get non-recurring events and recurring events that happen
            // to fall within the range
            sp = this.mSelectEventsByRange.params;
            sp.cal_id = this.mCalId;
            sp.range_start = startTime;
            sp.range_end = endTime;
            sp.start_offset = aRangeStart ? aRangeStart.timezoneOffset * USECS_PER_SECOND : 0;
            sp.end_offset = aRangeEnd ? aRangeEnd.timezoneOffset * USECS_PER_SECOND : 0;

            while (this.mSelectEventsByRange.step()) {
                var row = this.mSelectEventsByRange.row;
                var flags = {};
                var item = this.getEventFromRow(row, flags);
                flags = flags.value;

                resultItems.push({item: item, flags: flags});

                if (asOccurrences && flags & CAL_ITEM_FLAG_HAS_RECURRENCE)
                    handledRecurringEvents[row.id] = true;
            }
            this.mSelectEventsByRange.reset();

            // then, if we want occurrences, we need to query database-wide.. yuck
            if (asOccurrences) {
                sp = this.mSelectEventsWithRecurrence.params;
                sp.cal_id = this.mCalId;
                while (this.mSelectEventsWithRecurrence.step()) {
                    var row = this.mSelectEventsWithRecurrence.row;
                    // did we already deal with this event id?
                    if (row.id in handledRecurringEvents &&
                        handledRecurringEvents[row.id] == true) {
                        continue;
                    }

                    var flags = {};
                    var item = this.getEventFromRow(row, flags);
                    flags = flags.value;

                    resultItems.push({item: item, flags: flags});
                }
                this.mSelectEventsWithRecurrence.reset();
            }

            // process the events
            for each (var evitem in resultItems) {
                count += handleResultItem(evitem.item, evitem.flags, Ci.calIEvent);
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

            var resultItems = [];

            // first get non-recurring todos and recurring todos that happen
            // to fall within the range
            sp = this.mSelectTodosByRange.params;
            sp.cal_id = this.mCalId;
            sp.range_start = startTime;
            sp.range_end = endTime;
            sp.start_offset = aRangeStart ? aRangeStart.timezoneOffset * USECS_PER_SECOND : 0;
            sp.end_offset = aRangeEnd ? aRangeEnd.timezoneOffset * USECS_PER_SECOND : 0;

            while (this.mSelectTodosByRange.step()) {
                var row = this.mSelectTodosByRange.row;
                var flags = {};
                var item = this.getTodoFromRow(row, flags);
                flags = flags.value;

                if (!item.isCompleted && !wantNotCompletedItems)
                    continue;
                if (item.isCompleted && !wantCompletedItems)
                    continue;

                var completed = 
                resultItems.push({item: item, flags: flags});
                if (asOccurrences && row.flags & CAL_ITEM_FLAG_HAS_RECURRENCE)
                    handledRecurringTodos[row.id] = true;
            }
            this.mSelectTodosByRange.reset();

            // then, if we want occurrences, we need to query database-wide.. yuck
            if (asOccurrences) {
                sp = this.mSelectTodosWithRecurrence.params;
                sp.cal_id = this.mCalId;
                while (this.mSelectTodosWithRecurrence.step()) {
                    var row = this.mSelectTodosWithRecurrence.row;
                    // did we already deal with this todo id?
                    if (handledRecurringTodos[row.id] == true)
                        continue;

                    var flags = {};
                    var item = this.getTodoFromRow(row, flags);
                    flags = flags.value;

                    var itemIsCompleted = false;
                    if (item.todo_complete == 100 ||
                        item.todo_completed != null ||
                        item.ical_status == Ci.calITodo.CAL_TODO_STATUS_COMPLETED)
                        itemIsCompleted = true;

                    if (!itemIsCompleted && !wantNotCompletedItems)
                        continue;
                    if (itemIsCompleted && !wantCompletedItems)
                        continue;

                    resultItems.push({item: item, flags: flags});
                }
                this.mSelectTodosWithRecurrence.reset();
            }

            // process the todos
            for each (var todoitem in resultItems) {
                count += handleResultItem(todoitem.item, todoitem.flags, Ci.calITodo);
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

    startBatch: function ()
    {
        this.observeBatchChange(true);
    },
    endBatch: function ()
    {
        this.observeBatchChange(false);
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

    observeModifyItem: function (aNewItem, aOldItem) {
        for each (obs in this.mObservers)
            obs.onModifyItem (aNewItem, aOldItem);
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

        // Add a version stamp to the schema
        this.mDB.executeSimpleSQL("INSERT INTO cal_calendar_schema_version VALUES(" + this.DB_SCHEMA_VERSION + ")");
    },

    DB_SCHEMA_VERSION: 7,

    /** 
     * @return      db schema version
     * @exception   various, depending on error
     */
    getVersion: function calStorageGetVersion() {
        var selectSchemaVersion;
        var version = null;

        try {
            selectSchemaVersion = createStatement(this.mDB, 
                                  "SELECT version FROM " +
                                  "cal_calendar_schema_version LIMIT 1");
            if (selectSchemaVersion.step()) {
                version = selectSchemaVersion.row.version;
            }
            selectSchemaVersion.reset();

            if (version !== null) {
                // This is the only place to leave this function gracefully.
                return version;
            }
        } catch (e) {
            if (selectSchemaVersion) {
                selectSchemaVersion.reset();
            }
            dump ("++++++++++++ calStorageGetVersion() error: " +
                  this.mDB.lastErrorString + "\n");
            Components.utils.reportError("Error getting storage calendar " +
                                         "schema version! DB Error: " + 
                                         this.mDB.lastErrorString);
            throw e;
        }

        throw "cal_calendar_schema_version SELECT returned no results";
    },

    upgradeDB: function (oldVersion) {
        // some common helpers
        function addColumn(db, tableName, colName, colType) {
            db.executeSimpleSQL("ALTER TABLE " + tableName + " ADD COLUMN " + colName + " " + colType);
        }

        if (oldVersion == 2 && this.DB_SCHEMA_VERSION >= 3) {
            dump ("**** Upgrading schema from 2 -> 3\n");

            this.mDB.beginTransaction();
            try {
                // the change between 2 and 3 includes the splitting of cal_items into
                // cal_events and cal_todos, and the addition of columns for
                // event_start_tz, event_end_tz, todo_entry_tz, todo_due_tz.
                // These need to default to "UTC" if their corresponding time is
                // given, since that's what the default was for v2 calendars

                // create the two new tables
                try { this.mDB.executeSimpleSQL("DROP TABLE cal_events; DROP TABLE cal_todos;"); } catch (e) { }
                this.mDB.createTable("cal_events", sqlTables["cal_events"]);
                this.mDB.createTable("cal_todos", sqlTables["cal_todos"]);

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

                // now fix up the new _tz columns
                this.mDB.executeSimpleSQL("UPDATE cal_events SET event_start_tz = 'UTC' WHERE event_start IS NOT NULL");
                this.mDB.executeSimpleSQL("UPDATE cal_events SET event_end_tz = 'UTC' WHERE event_end IS NOT NULL");
                this.mDB.executeSimpleSQL("UPDATE cal_todos SET todo_entry_tz = 'UTC' WHERE todo_entry IS NOT NULL");
                this.mDB.executeSimpleSQL("UPDATE cal_todos SET todo_due_tz = 'UTC' WHERE todo_due IS NOT NULL");
                this.mDB.executeSimpleSQL("UPDATE cal_todos SET todo_completed_tz = 'UTC' WHERE todo_completed IS NOT NULL");

                // finally update the version
                this.mDB.executeSimpleSQL("DELETE FROM cal_calendar_schema_version; INSERT INTO cal_calendar_schema_version VALUES (3);");

                this.mDB.commitTransaction();

                oldVersion = 3;
            } catch (e) {
                dump ("+++++++++++++++++ DB Error: " + this.mDB.lastErrorString + "\n");
                Components.utils.reportError("Upgrade failed! DB Error: " + 
                                             this.mDB.lastErrorString);
                this.mDB.rollbackTransaction();
                throw e;
            }
        }

        if (oldVersion == 3 && this.DB_SCHEMA_VERSION >= 4) {
            dump ("**** Upgrading schema from 3 -> 4\n");

            this.mDB.beginTransaction();
            try {
                // the change between 3 and 4 is the addition of
                // recurrence_id and recurrence_id_tz columns to
                // cal_events, cal_todos, cal_attendees, and cal_properties
                addColumn(this.mDB, "cal_events", "recurrence_id", "INTEGER");
                addColumn(this.mDB, "cal_events", "recurrence_id_tz", "VARCHAR");

                addColumn(this.mDB, "cal_todos", "recurrence_id", "INTEGER");
                addColumn(this.mDB, "cal_todos", "recurrence_id_tz", "VARCHAR");

                addColumn(this.mDB, "cal_attendees", "recurrence_id", "INTEGER");
                addColumn(this.mDB, "cal_attendees", "recurrence_id_tz", "VARCHAR");

                addColumn(this.mDB, "cal_properties", "recurrence_id", "INTEGER");
                addColumn(this.mDB, "cal_properties", "recurrence_id_tz", "VARCHAR");

                this.mDB.executeSimpleSQL("DELETE FROM cal_calendar_schema_version; INSERT INTO cal_calendar_schema_version VALUES (4);");
                this.mDB.commitTransaction();

                oldVersion = 4;
            } catch (e) {
                dump ("+++++++++++++++++ DB Error: " + this.mDB.lastErrorString + "\n");
                Components.utils.reportError("Upgrade failed! DB Error: " +
                                             this.mDB.lastErrorString);
                this.mDB.rollbackTransaction();
                throw e;
            }
        }

        if (oldVersion == 4 && this.DB_SCHEMA_VERSION >= 5) {
            dump ("**** Upgrading schema from 4 -> 5\n");

            this.mDB.beginTransaction();
            try {
                // the change between 4 and 5 is the addition of alarm_offset
                // and alarm_last_ack columns.  The alarm_time column is not
                // used in this version, but will likely return in future versions
                // so it is not being removed
                addColumn(this.mDB, "cal_events", "alarm_offset", "INTEGER");
                addColumn(this.mDB, "cal_events", "alarm_related", "INTEGER");
                addColumn(this.mDB, "cal_events", "alarm_last_ack", "INTEGER");

                addColumn(this.mDB, "cal_todos", "alarm_offset", "INTEGER");
                addColumn(this.mDB, "cal_todos", "alarm_related", "INTEGER");
                addColumn(this.mDB, "cal_todos", "alarm_last_ack", "INTEGER");

                this.mDB.executeSimpleSQL("UPDATE cal_calendar_schema_version SET version = 5;");
                this.mDB.commitTransaction();
                oldVersion = 5;
            } catch (e) {
                dump ("+++++++++++++++++ DB Error: " + this.mDB.lastErrorString + "\n");
                Components.utils.reportError("Upgrade failed! DB Error: " +
                                             this.mDB.lastErrorString);
                this.mDB.rollbackTransaction();
                throw e;
            }
        }

        if (oldVersion == 5 && this.DB_SCHEMA_VERSION >= 6) {
            dump ("**** Upgrading schema from 5 -> 6\n");

            this.mDB.beginTransaction();
            try {
                // Schema changes between v5 and v6:
                //
                // - Change all STRING columns to TEXT to avoid SQLite's
                //   "feature" where it will automatically convert strings to
                //   numbers (ex: 10e4 -> 10000). See bug 333688.


                // Create the new tables.
                var tableNames = ["cal_events", "cal_todos", "cal_attendees",
                                  "cal_recurrence", "cal_properties"];

                var query = "";
                try { 
                    for (var i in tableNames) {
                        query += "DROP TABLE " + tableNames[i] + "_v6;"
                    }
                    this.mDB.executeSimpleSQL(query);
                } catch (e) {
                    // We should get exceptions for trying to drop tables
                    // that don't (shouldn't) exist.
                }

                this.mDB.createTable("cal_events_v6", sqlTables["cal_events"]);
                this.mDB.createTable("cal_todos_v6", sqlTables["cal_todos"]);
                this.mDB.createTable("cal_attendees_v6", sqlTables["cal_attendees"]);
                this.mDB.createTable("cal_recurrence_v6", sqlTables["cal_recurrence"]);
                this.mDB.createTable("cal_properties_v6", sqlTables["cal_properties"]);


                // Copy in the data.
                var cal_events_cols = ["cal_id", "id", "time_created",
                                       "last_modified", "title", "priority",
                                       "privacy", "ical_status",
                                       "recurrence_id", "recurrence_id_tz",
                                       "flags", "event_start",
                                       "event_start_tz", "event_end",
                                       "event_end_tz", "event_stamp",
                                       "alarm_time", "alarm_time_tz",
                                       "alarm_offset", "alarm_related",
                                       "alarm_last_ack"];

                var cal_todos_cols = ["cal_id", "id", "time_created",
                                      "last_modified", "title", "priority",
                                      "privacy", "ical_status",
                                      "recurrence_id", "recurrence_id_tz",
                                      "flags", "todo_entry", "todo_entry_tz",
                                      "todo_due", "todo_due_tz",
                                      "todo_completed", "todo_completed_tz",
                                      "todo_complete", "alarm_time",
                                      "alarm_time_tz", "alarm_offset",
                                      "alarm_related", "alarm_last_ack"];

                var cal_attendees_cols = ["item_id", "recurrence_id",
                                          "recurrence_id_tz", "attendee_id",
                                          "common_name", "rsvp", "role",
                                          "status", "type"];

                var cal_recurrence_cols = ["item_id", "recur_index",
                                           "recur_type", "is_negative",
                                           "dates", "count", "end_date",
                                           "interval", "second", "minute",
                                           "hour", "day", "monthday",
                                           "yearday", "weekno", "month",
                                           "setpos"];

                var cal_properties_cols = ["item_id", "recurrence_id",
                                           "recurrence_id_tz", "key",
                                           "value"];

                var theDB = this.mDB;
                function copyDataOver(aTableName, aColumnNames) {
                    theDB.executeSimpleSQL("INSERT INTO " + aTableName + "_v6(" + aColumnNames.join(",") + ") " + 
                                           "     SELECT " + aColumnNames.join(",") + 
                                           "       FROM " + aTableName + ";");
                }

                copyDataOver("cal_events", cal_events_cols);
                copyDataOver("cal_todos", cal_todos_cols);
                copyDataOver("cal_attendees", cal_attendees_cols);
                copyDataOver("cal_recurrence", cal_recurrence_cols);
                copyDataOver("cal_properties", cal_properties_cols);


                // Delete each old table and rename the new ones to use the
                // old tables' names.
                for (var i in tableNames) {
                    this.mDB.executeSimpleSQL("DROP TABLE  " + tableNames[i] + ";" +
                                              "ALTER TABLE " + tableNames[i] + "_v6" + 
                                              "  RENAME TO " + tableNames[i] + ";");
                }


                // Update the version stamp, and commit.
                this.mDB.executeSimpleSQL("UPDATE cal_calendar_schema_version SET version = 6;");
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

        if (oldVersion == 6 && this.DB_SCHEMA_VERSION >= 7) {
            dump ("**** Upgrading schema from 6 -> 7\n");

            var getTzIds;
            this.mDB.beginTransaction();
            try {
                // Schema changes between v6 and v7:
                //
                // - Migrate all stored mozilla.org timezones from 20050126_1
                //   to 20070129_1.  Note that there are some exceptions where
                //   timezones were deleted and/or renamed.

                // Get a list of the /mozilla.org/* timezones used in the db
                var tzId;
                getTzIds = createStatement(this.mDB,
                    "SELECT DISTINCT(zone) FROM ("+
                        "SELECT recurrence_id_tz AS zone FROM cal_attendees  WHERE recurrence_id_tz LIKE '/mozilla.org%' UNION " +
                        "SELECT recurrence_id_tz AS zone FROM cal_events     WHERE recurrence_id_tz LIKE '/mozilla.org%' UNION " +
                        "SELECT event_start_tz   AS zone FROM cal_events     WHERE event_start_tz   LIKE '/mozilla.org%' UNION " +
                        "SELECT event_end_tz     AS zone FROM cal_events     WHERE event_end_tz     LIKE '/mozilla.org%' UNION " +
                        "SELECT alarm_time_tz    AS zone FROM cal_events     WHERE alarm_time_tz    LIKE '/mozilla.org%' UNION " +
                        "SELECT recurrence_id_tz AS zone FROM cal_properties WHERE recurrence_id_tz LIKE '/mozilla.org%' UNION " +
                        "SELECT recurrence_id_tz AS zone FROM cal_todos      WHERE recurrence_id_tz LIKE '/mozilla.org%' UNION " +
                        "SELECT todo_entry_tz    AS zone FROM cal_todos      WHERE todo_entry_tz    LIKE '/mozilla.org%' UNION " +
                        "SELECT todo_due_tz      AS zone FROM cal_todos      WHERE todo_due_tz      LIKE '/mozilla.org%' UNION " +
                        "SELECT alarm_time_tz    AS zone FROM cal_todos      WHERE alarm_time_tz    LIKE '/mozilla.org%'" +
                    ");");

                var tzIdsToUpdate = [];
                var updateTzIds = false; // Perform the SQL UPDATE, or not.
                while (getTzIds.step()) {
                    tzId = getTzIds.row.zone;

                    // Send the timezones off to the ICS service to attempt
                    // conversion.
                    icsSvc = Components.classes["@mozilla.org/calendar/ics-service;1"]
                                       .getService(Components.interfaces.calIICSService);
                    var latestTzId = icsSvc.latestTzId(tzId);
                    if ((latestTzId) && (tzId != latestTzId)) {
                        tzIdsToUpdate.push({oldTzId: tzId, newTzId: latestTzId});
                        updateTzIds = true;
                    }
                }
                getTzIds.reset();

                if (updateTzIds) {
                    // We've got stuff to update!
                    for each (var update in tzIdsToUpdate) {
                        this.mDB.executeSimpleSQL(
                            "UPDATE cal_attendees  SET recurrence_id_tz = '" + update.newTzId + "' WHERE recurrence_id_tz = '" + update.oldTzId + "'; " +
                            "UPDATE cal_events     SET recurrence_id_tz = '" + update.newTzId + "' WHERE recurrence_id_tz = '" + update.oldTzId + "'; " +
                            "UPDATE cal_events     SET event_start_tz   = '" + update.newTzId + "' WHERE event_start_tz   = '" + update.oldTzId + "'; " +
                            "UPDATE cal_events     SET event_end_tz     = '" + update.newTzId + "' WHERE event_end_tz     = '" + update.oldTzId + "'; " +
                            "UPDATE cal_events     SET alarm_time_tz    = '" + update.newTzId + "' WHERE alarm_time_tz    = '" + update.oldTzId + "'; " +
                            "UPDATE cal_properties SET recurrence_id_tz = '" + update.newTzId + "' WHERE recurrence_id_tz = '" + update.oldTzId + "'; " +
                            "UPDATE cal_todos      SET recurrence_id_tz = '" + update.newTzId + "' WHERE recurrence_id_tz = '" + update.oldTzId + "'; " +
                            "UPDATE cal_todos      SET todo_entry_tz    = '" + update.newTzId + "' WHERE todo_entry_tz    = '" + update.oldTzId + "'; " +
                            "UPDATE cal_todos      SET todo_due_tz      = '" + update.newTzId + "' WHERE todo_due_tz      = '" + update.oldTzId + "'; " +
                            "UPDATE cal_todos      SET alarm_time_tz    = '" + update.newTzId + "' WHERE recurrence_id_tz = '" + update.oldTzId + "';");
                    }
                }

                // Update the version stamp, and commit.
                this.mDB.executeSimpleSQL("UPDATE cal_calendar_schema_version SET version = 7;");
                this.mDB.commitTransaction();
                oldVersion = 7;
            } catch (e) {
                dump ("+++++++++++++++++ DB Error: " + this.mDB.lastErrorString + "\n");
                Components.utils.reportError("Upgrade failed! DB Error: " +
                                             this.mDB.lastErrorString);
                this.mDB.rollbackTransaction();
                throw e;
            }
        }

        if (oldVersion != 7) {
            dump ("#######!!!!! calStorageCalendar Schema Update failed -- db version: " + oldVersion + " this version: " + this.DB_SCHEMA_VERSION + "\n");
            throw Components.results.NS_ERROR_FAILURE;
        }
    },

    // database initialization
    // assumes mDB is valid

    initDB: function () {
        if (!this.mDB.tableExists("cal_calendar_schema_version")) {
            dump("*** cal_calendar_schema_version not found; " +
		 "initializing storage provider tables\n");
            this.initDBSchema();
        } else {
            var version = this.getVersion();
            dump ("*** Calendar schema version is: " + version + "\n");

            if (version != this.DB_SCHEMA_VERSION) {
                this.upgradeDB(version);
            }
        }
        
        // (Conditionally) add index
        this.mDB.executeSimpleSQL(
            "CREATE INDEX IF NOT EXISTS " + 
            "idx_cal_properies_item_id ON cal_properties(item_id);"
            );

        this.mSelectEvent = createStatement (
            this.mDB,
            "SELECT * FROM cal_events " +
            "WHERE id = :id AND recurrence_id IS NULL " +
            "LIMIT 1"
            );

        this.mSelectTodo = createStatement (
            this.mDB,
            "SELECT * FROM cal_todos " +
            "WHERE id = :id AND recurrence_id IS NULL " +
            "LIMIT 1"
            );

        // The more readable version of the next where-clause is:
        //   WHERE event_end >= :range_start AND event_start < :range_end
        // but that doesn't work with floating start or end times. The logic
        // is the same though.
        // For readability, a few helpers:
        var floatingEventStart = "event_start_tz = 'floating' AND event_start"
        var nonFloatingEventStart = "event_start_tz != 'floating' AND event_start"
        var floatingEventEnd = "event_end_tz = 'floating' AND event_end"
        var nonFloatingEventEnd = "event_end_tz != 'floating' AND event_end"
        // The query needs to take both floating and non floating into account
        this.mSelectEventsByRange = createStatement(
            this.mDB,
            "SELECT * FROM cal_events " +
            "WHERE " +
            " (("+floatingEventEnd+" >= :range_start + :start_offset) OR " +
            "  ("+nonFloatingEventEnd+" >= :range_start)) AND " +
            " (("+floatingEventStart+" < :range_end + :end_offset) OR " +
            "  ("+nonFloatingEventStart+" < :range_end)) " +
            "  AND cal_id = :cal_id AND recurrence_id IS NULL"
            );

        var floatingTodoEntry = "todo_entry_tz = 'floating' AND todo_entry";
        var nonFloatingTodoEntry = "todo_entry_tz != 'floating' AND todo_entry";
        var floatingTodoDue = "todo_due_tz = 'floating' AND todo_due";
        var nonFloatingTodoDue = "todo_due_tz != 'floating' AND todo_due";

        this.mSelectTodosByRange = createStatement(
            this.mDB,
            "SELECT * FROM cal_todos " +
            "WHERE " +
            " ((("+floatingTodoDue+" >= :range_start + :start_offset) OR " +
            "   ("+nonFloatingTodoDue+" >= :range_start)) OR " +
            "  (todo_due IS NULL)) AND " +
            " ((("+floatingTodoEntry+" < :range_end + :end_offset) OR " +
            "   ("+nonFloatingTodoEntry+" < :range_end)) OR " +
            "  (todo_entry IS NULL)) " +
            " AND cal_id = :cal_id AND recurrence_id IS NULL"
            );

        this.mSelectEventsWithRecurrence = createStatement(
            this.mDB,
            "SELECT * FROM cal_events " +
            " WHERE flags & 16 == 16 " +
            "   AND cal_id = :cal_id AND recurrence_id is NULL"
            );

        this.mSelectTodosWithRecurrence = createStatement(
            this.mDB,
            "SELECT * FROM cal_todos " +
            " WHERE flags & 16 == 16 " +
            "   AND cal_id = :cal_id AND recurrence_id IS NULL"
            );

        this.mSelectEventExceptions = createStatement (
            this.mDB,
            "SELECT * FROM cal_events " +
            "WHERE id = :id AND recurrence_id IS NOT NULL"
            );

        this.mSelectTodoExceptions = createStatement (
            this.mDB,
            "SELECT * FROM cal_todos " +
            "WHERE id = :id AND recurrence_id IS NOT NULL"
            );

        // For the extra-item data, note that we use mDBTwo, so that
        // these can be executed while a selectItems is running!
        this.mSelectAttendeesForItem = createStatement(
            this.mDBTwo,
            "SELECT * FROM cal_attendees " +
            "WHERE item_id = :item_id AND recurrence_id IS NULL"
            );

        this.mSelectAttendeesForItemWithRecurrenceId = createStatement(
            this.mDBTwo,
            "SELECT * FROM cal_attendees " +
            "WHERE item_id = :item_id AND recurrence_id = :recurrence_id AND recurrence_id_tz = :recurrence_id_tz"
            );

        this.mSelectPropertiesForItem = createStatement(
            this.mDBTwo,
            "SELECT * FROM cal_properties " +
            "WHERE item_id = :item_id AND recurrence_id IS NULL"
            );

        this.mSelectPropertiesForItemWithRecurrenceId = createStatement(
            this.mDBTwo,
            "SELECT * FROM cal_properties " +
            "WHERE item_id = :item_id AND recurrence_id = :recurrence_id AND recurrence_id_tz = :recurrence_id_tz"
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
            "   alarm_time, alarm_time_tz, recurrence_id, recurrence_id_tz, " +
            "   alarm_offset, alarm_related, alarm_last_ack) " +
            "VALUES (:cal_id, :id, :time_created, :last_modified, " +
            "        :title, :priority, :privacy, :ical_status, :flags, " +
            "        :event_start, :event_start_tz, :event_end, :event_end_tz, :event_stamp, " +
            "        :alarm_time, :alarm_time_tz, :recurrence_id, :recurrence_id_tz," + 
            "        :alarm_offset, :alarm_related, :alarm_last_ack)"
            );

        this.mInsertTodo = createStatement (
            this.mDB,
            "INSERT INTO cal_todos " +
            "  (cal_id, id, time_created, last_modified, " +
            "   title, priority, privacy, ical_status, flags, " +
            "   todo_entry, todo_entry_tz, todo_due, todo_due_tz, todo_completed, " +
            "   todo_completed_tz, todo_complete, " +
            "   alarm_time, alarm_time_tz, recurrence_id, recurrence_id_tz, " +
            "   alarm_offset, alarm_related, alarm_last_ack)" +
            "VALUES (:cal_id, :id, :time_created, :last_modified, " +
            "        :title, :priority, :privacy, :ical_status, :flags, " +
            "        :todo_entry, :todo_entry_tz, :todo_due, :todo_due_tz, " +
            "        :todo_completed, :todo_completed_tz, :todo_complete, " +
            "        :alarm_time, :alarm_time_tz, :recurrence_id, :recurrence_id_tz," + 
            "        :alarm_offset, :alarm_related, :alarm_last_ack)"
            );
        this.mInsertProperty = createStatement (
            this.mDB,
            "INSERT INTO cal_properties (item_id, recurrence_id, recurrence_id_tz, key, value) " +
            "VALUES (:item_id, :recurrence_id, :recurrence_id_tz, :key, :value)"
            );
        this.mInsertAttendee = createStatement (
            this.mDB,
            "INSERT INTO cal_attendees " +
            "  (item_id, recurrence_id, recurrence_id_tz, attendee_id, common_name, rsvp, role, status, type) " +
            "VALUES (:item_id, :recurrence_id, :recurrence_id_tz, :attendee_id, :common_name, :rsvp, :role, :status, :type)"
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

        // These are only used when deleting an entire calendar
        var extrasTables = [ "cal_attendees", "cal_properties", "cal_recurrence" ];

        this.mDeleteEventExtras = new Array();
        this.mDeleteTodoExtras = new Array();

        for (var table in extrasTables) {
            this.mDeleteEventExtras[table] = createStatement (
                this.mDB,
                "DELETE FROM " + extrasTables[table] + " WHERE item_id IN" +
                "  (SELECT id FROM cal_events WHERE cal_id = :cal_id)"
                );
            this.mDeleteTodoExtras[table] = createStatement (
                this.mDB,
                "DELETE FROM " + extrasTables[table] + " WHERE item_id IN" +
                "  (SELECT id FROM cal_todos WHERE cal_id = :cal_id)"
                );
        }

        // Note that you must delete the "extras" _first_ using the above two
        // statements, before you delete the events themselves.
        this.mDeleteAllEvents = createStatement (
            this.mDB,
            "DELETE from cal_events WHERE cal_id = :cal_id"
            );
        this.mDeleteAllTodos = createStatement (
            this.mDB,
            "DELETE from cal_todos WHERE cal_id = :cal_id"
            );

    },


    //
    // database reading functions
    //

    // read in the common ItemBase attributes from aDBRow, and stick
    // them on item
    getItemBaseFromRow: function (row, flags, item) {
        if (row.time_created)
            item.creationDate = newDateTime(row.time_created, "UTC");
        if (row.last_modified)
            item.lastModifiedTime = newDateTime(row.last_modified, "UTC");
        item.calendar = this;
        item.id = row.id;
        if (row.title)
            item.title = row.title;
        if (row.priority)
            item.priority = row.priority;
        if (row.privacy)
            item.privacy = row.privacy;
        if (row.ical_status)
            item.status = row.ical_status;

        if (row.alarm_time) {
            // Old (schema version 4) data, need to convert this nicely to the
            // new alarm interface.  Eventually, we're going to want to be able
            // to deal with both types of data in a calIAlarm interface, but
            // not yet.  Leaving this column around though may help ease that
            // transition in the future.
            var alarmTime = newDateTime(row.alarm_time, row.alarm_time_tz);
            var time;
            var related = Components.interfaces.calIItemBase.ALARM_RELATED_START;
            if (item instanceof Components.interfaces.calIEvent) {
                time = newDateTime(row.event_start, row.event_start_tz);
            } else { //tasks
                if (row.todo_entry) {
                    time = newDateTime(row.todo_entry, row.todo_entry_tz);
                } else if (row.todo_due) {
                    related = Components.interfaces.calIItemBase.ALARM_RELATED_END;
                    time = newDateTime(row.todo_due, row.todo_due_tz);
                }
            }
            if (time) {
                var duration = alarmTime.subtractDate(time);
                item.alarmOffset = duration;
                item.alarmRelated = related;
            } else {
                Components.utils.reportError("WARNING! Couldn't do alarm conversion for item:"+
                                             item.title+','+item.id+"!\n");
            }
        }

        // Alarm offset could be 0, but this is ok, so compare with null
        if (row.alarm_offset != null) {
            var duration = Components.classes["@mozilla.org/calendar/duration;1"]
                                     .createInstance(Components.interfaces.calIDuration);
            duration.inSeconds = row.alarm_offset;
            duration.normalize();

            item.alarmOffset = duration;
            item.alarmRelated = row.alarm_related;
            if (row.alarm_last_ack) {
                // alarm acks are always in utc
                item.alarmLastAck = newDateTime(row.alarm_last_ack, "UTC");
            }
        }

        if (row.recurrence_id)
            item.recurrenceId = newDateTime(row.recurrence_id, row.recurrence_id_tz);

        if (flags)
            flags.value = row.flags;
    },

    getEventFromRow: function (row, flags) {
        var item = createEvent();

        this.getItemBaseFromRow (row, flags, item);

        if (row.event_start)
            item.startDate = newDateTime(row.event_start, row.event_start_tz);
        if (row.event_end)
            item.endDate = newDateTime(row.event_end, row.event_end_tz);
        if (row.event_stamp)
            item.stampTime = newDateTime(row.event_stamp, "UTC");
        if ((row.flags & CAL_ITEM_FLAG_EVENT_ALLDAY) != 0) {
            item.startDate.isDate = true;
            item.endDate.isDate = true;
        }

        return item;
    },

    getTodoFromRow: function (row, flags) {
        var item = createTodo();

        this.getItemBaseFromRow (row, flags, item);

        if (row.todo_entry)
            item.entryDate = newDateTime(row.todo_entry, row.todo_entry_tz);
        if (row.todo_due)
            item.dueDate = newDateTime(row.todo_due, row.todo_due_tz);
        if (row.todo_completed)
            item.completedDate = newDateTime(row.todo_completed, row.todo_completed_tz);
        if (row.todo_complete)
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
            var selectItem = null;
            if (item.recurrenceId == null)
                selectItem = this.mSelectAttendeesForItem;
            else {
                selectItem = this.mSelectAttendeesForItemWithRecurrenceId;
                this.setDateParamHelper(selectItem.params, "recurrence_id", item.recurrenceId);
            }

            selectItem.params.item_id = item.id;

            while (selectItem.step()) {
                var attendee = this.getAttendeeFromRow(selectItem.row);
                item.addAttendee(attendee);
            }
            selectItem.reset();
        }

        var row;
        if (flags & CAL_ITEM_FLAG_HAS_PROPERTIES) {
            var selectItem = null;
            if (item.recurrenceId == null)
                selectItem = this.mSelectPropertiesForItem;
            else {
                selectItem = this.mSelectPropertiesForItemWithRecurrenceId;
                this.setDateParamHelper(selectItem.params, "recurrence_id", item.recurrenceId);
            }
                
            selectItem.params.item_id = item.id;
            
            while (selectItem.step()) {
                row = selectItem.row;
                item.setProperty (row.key, row.value);
            }
            selectItem.reset();
        }

        var i;
        if (flags & CAL_ITEM_FLAG_HAS_RECURRENCE) {
            if (item.recurrenceId)
                throw Components.results.NS_ERROR_UNEXPECTED;

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
                        if (row.end_date)
                            ritem.endDate = newDateTime(row.end_date);
                        else
                            ritem.endDate = null;
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
                            var rstr = row[rtypes[i]].toString().split(",");
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
                    rec.item = item;
                }
                rec.appendRecurrenceItem(ritem);
            }

            if (rec == null) {
                dump ("XXXX Expected to find recurrence, but got no items!\n");
            }
            item.recurrenceInfo = rec;

            this.mSelectRecurrenceForItem.reset();
        }

        if (flags & CAL_ITEM_FLAG_HAS_EXCEPTIONS) {
            if (item.recurrenceId)
                throw Components.results.NS_ERROR_UNEXPECTED;

            var rec = item.recurrenceInfo;

            var exceptions = [];

            if (item instanceof Ci.calIEvent) {
                this.mSelectEventExceptions.params.id = item.id;
                while (this.mSelectEventExceptions.step()) {
                    var row = this.mSelectEventExceptions.row;
                    var flags = {};
                    var exc = this.getEventFromRow(row, flags);
                    exceptions.push({item: exc, flags: flags.value});
                }
                this.mSelectEventExceptions.reset();
            } else if (item instanceof Ci.calITodo) {
                this.mSelectTodoExceptions.params.id = item.id;
                while (this.mSelectTodoExceptions.step()) {
                    var row = this.mSelectTodoExceptions.row;
                    var flags = {};
                    var exc = this.getTodoFromRow(row, flags);
                    
                    exceptions.push({item: exc, flags: flags.value});
                }
                this.mSelectTodoExceptions.reset();
            } else {
                throw Components.results.NS_ERROR_UNEXPECTED;
            }

            for each (var exc in exceptions) {
                this.getAdditionalDataForItem(exc.item, exc.flags);
                exc.item.parentItem = item;
                rec.modifyException(exc.item);
            }
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

    //
    // database writing functions
    //

    setDateParamHelper: function (params, entryname, cdt) {
        if (cdt) {
            params[entryname] = cdt.nativeTime;
            params[entryname + "_tz"] = cdt.timezone;
        } else {
            params[entryname] = null;
            params[entryname + "_tz"] = null;
        }
    },

    flushItem: function (item, olditem) {
        this.mDB.beginTransaction();
        try {
            this.writeItem(item, olditem);
            this.mDB.commitTransaction();
        } catch (e) {
            dump("flushItem DB error: " + this.mDB.lastErrorString + "\n");
            Components.utils.reportError("flushItem DB error: " +
                                         this.mDB.lastErrorString);
            this.mDB.rollbackTransaction();
            throw e;
        }

        this.mItemCache[item.id] = item;
    },

    //
    // Nuke olditem, if any
    //

    deleteOldItem: function (item, olditem) {
        if (olditem) {
            var oldItemDeleteStmt;
            if (olditem instanceof Ci.calIEvent)
                oldItemDeleteStmt = this.mDeleteEvent;
            else if (olditem instanceof Ci.calITodo)
                oldItemDeleteStmt = this.mDeleteTodo;

            oldItemDeleteStmt.params.id = olditem.id;
            this.mDeleteAttendees.params.item_id = olditem.id;
            this.mDeleteProperties.params.item_id = olditem.id;
            this.mDeleteRecurrence.params.item_id = olditem.id;

            this.mDeleteRecurrence.execute();
            this.mDeleteProperties.execute();
            this.mDeleteAttendees.execute();
            oldItemDeleteStmt.execute();
        }
    },

    //
    // The write* functions execute the database bits
    // to write the given item type.  They're to return
    // any bits they want or'd into flags, which will be passed
    // to writeEvent/writeTodo to actually do the writing.
    //

    writeItem: function (item, olditem) {
        var flags = 0;

        this.deleteOldItem(item, olditem);

        flags |= this.writeAttendees(item, olditem);
        flags |= this.writeRecurrence(item, olditem);
        flags |= this.writeProperties(item, olditem);
        flags |= this.writeAttachments(item, olditem);

        if (item instanceof Ci.calIEvent)
            this.writeEvent(item, olditem, flags);
        else if (item instanceof Ci.calITodo)
            this.writeTodo(item, olditem, flags);
        else
            throw Components.results.NS_ERROR_UNEXPECTED;
    },

    writeEvent: function (item, olditem, flags) {
        var ip = this.mInsertEvent.params;
        this.setupItemBaseParams(item, olditem,ip);

        var tmp;

        tmp = item.getUnproxiedProperty("DTSTART");
        //if (tmp instanceof Ci.calIDateTime) {}
        this.setDateParamHelper(ip, "event_start", tmp);
        tmp = item.getUnproxiedProperty("DTEND");
        //if (tmp instanceof Ci.calIDateTime) {}
        this.setDateParamHelper(ip, "event_end", tmp);

        if (item.startDate.isDate)
            flags |= CAL_ITEM_FLAG_EVENT_ALLDAY;

        ip.flags = flags;

        this.mInsertEvent.execute();
    },

    writeTodo: function (item, olditem, flags) {
        var ip = this.mInsertTodo.params;

        this.setupItemBaseParams(item, olditem,ip);

        this.setDateParamHelper(ip, "todo_entry", item.getUnproxiedProperty("DTSTART"));
        this.setDateParamHelper(ip, "todo_due", item.getUnproxiedProperty("DUE"));
        this.setDateParamHelper(ip, "todo_completed", item.getUnproxiedProperty("COMPLETED"));

        ip.todo_complete = item.getUnproxiedProperty("PERCENT-COMPLETED");

        ip.flags = flags;

        this.mInsertTodo.execute();
    },

    setupItemBaseParams: function (item, olditem, ip) {
        ip.cal_id = this.mCalId;
        ip.id = item.id;

        if (item.recurrenceId)
            this.setDateParamHelper(ip, "recurrence_id", item.recurrenceId);

        var tmp;

        if ((tmp = item.getUnproxiedProperty("CREATED")))
            ip.time_created = tmp.nativeTime;
        if ((tmp = item.getUnproxiedProperty("LAST-MODIFIED")))
            ip.last_modified = tmp.nativeTime;

        ip.title = item.getUnproxiedProperty("SUMMARY");
        ip.priority = item.getUnproxiedProperty("PRIORITY");
        ip.privacy = item.getUnproxiedProperty("CLASS");
        ip.ical_status = item.getUnproxiedProperty("STATUS");

        if (!item.parentItem)
            ip.event_stamp = item.stampTime.nativeTime;

        if (item.alarmOffset) {
            ip.alarm_offset = item.alarmOffset.inSeconds;
            ip.alarm_related = item.alarmRelated;
            if (item.alarmLastAck) {
                ip.alarm_last_ack = item.alarmLastAck.nativeTime;
            }
        }
    },

    writeAttendees: function (item, olditem) {
        // XXX how does this work for proxy stuffs?
        var attendees = item.getAttendees({});
        if (attendees && attendees.length > 0) {
            for each (var att in attendees) {
                var ap = this.mInsertAttendee.params;
                ap.item_id = item.id;
                this.setDateParamHelper(ap, "recurrence_id", item.recurrenceId);
                ap.attendee_id = att.id;
                ap.common_name = att.commonName;
                ap.rsvp = att.rsvp;
                ap.role = att.role;
                ap.status = att.participationStatus;
                ap.type = att.userType;

                this.mInsertAttendee.execute();
            }

            return CAL_ITEM_FLAG_HAS_ATTENDEES;
        }

        return 0;
    },

    writeProperties: function (item, olditem) {
        var ret = 0;
        var propEnumerator = item.unproxiedPropertyEnumerator;
        while (propEnumerator.hasMoreElements()) {
            ret = CAL_ITEM_FLAG_HAS_PROPERTIES;

            var prop = propEnumerator.getNext().QueryInterface(Components.interfaces.nsIProperty);

            if (item.isPropertyPromoted(prop.name))
                continue;

            var pp = this.mInsertProperty.params;

            pp.key = prop.name;
            var pval = prop.value;
            if (pval instanceof Ci.calIDateTime) {
                pp.value = pval.nativeTime;
            } else {
                pp.value = pval;
            }
            pp.item_id = item.id;
            this.setDateParamHelper(pp, "recurrence_id", item.recurrenceId);

            this.mInsertProperty.execute();
        }

        return ret;
    },

    writeRecurrence: function (item, olditem) {
        var flags = 0;

        var rec = item.recurrenceInfo;
        if (rec) {
            flags = CAL_ITEM_FLAG_HAS_RECURRENCE;
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
                        ap.end_date = ritem.endDate ? ritem.endDate.nativeTime : null;

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
                        var comp = "BY" + rtypes[j].toUpperCase();
                        var comps = ritem.getComponent(comp, {});
                        if (comps && comps.length > 0) {
                            var compstr = comps.join(",");
                            ap[rtypes[j]] = compstr;
                        }
                    }
                } else {
                    dump ("##### Don't know how to serialize recurrence item " + ritem + "!\n");
                }

                this.mInsertRecurrence.execute();
            }

            var exceptions = rec.getExceptionIds ({});
            if (exceptions.length > 0) {
                flags |= CAL_ITEM_FLAG_HAS_EXCEPTIONS;

                // we need to serialize each exid as a separate
                // event/todo; setupItemBase will handle
                // writing the recurrenceId for us
                for each (exid in exceptions) {
                    var ex = rec.getExceptionFor(exid, false);
                    if (!ex)
                        throw Components.results.NS_ERROR_UNEXPECTED;
                    this.writeItem(ex, null);
                }
            }
        }

        return flags;
    },

    writeAttachments: function (item, olditem) {
        // XXX write me?
        return 0;
    },

    //
    // delete the item with the given uid
    //
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
            Components.utils.reportError("deleteItemById DB error: " + 
                                         this.mDB.lastErrorString);
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

    mUtilsLoaded: false,
    loadUtils: function storageLoadUtils() {
        if (this.mUtilsLoaded)
            return;

        const jssslContractID = "@mozilla.org/moz/jssubscript-loader;1";
        const jssslIID = Components.interfaces.mozIJSSubScriptLoader;

        const iosvcContractID = "@mozilla.org/network/io-service;1";
        const iosvcIID = Components.interfaces.nsIIOService;

        var loader = Components.classes[jssslContractID].getService(jssslIID);
        var iosvc = Components.classes[iosvcContractID].getService(iosvcIID);

        // Note that unintuitively, __LOCATION__.parent == .
        // We expect to find utils in ./../js
        var appdir = __LOCATION__.parent.parent;
        appdir.append("js");
        var scriptName = "calUtils.js";

        var f = appdir.clone();
        f.append(scriptName);

        try {
            var fileurl = iosvc.newFileURI(f);
            loader.loadSubScript(fileurl.spec, this.__parent__.__parent__);
        } catch (e) {
            dump("Error while loading " + fileurl.spec + "\n");
            throw e;
        }

        this.mUtilsLoaded = true;
    },
    
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

        this.loadUtils();

        if (!CalAttendee) {
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
    "	id		TEXT," +
    "	time_created	INTEGER," +
    "	last_modified	INTEGER," +
    "	title		TEXT," +
    "	priority	INTEGER," +
    "	privacy		TEXT," +
    "	ical_status	TEXT," +
    "	recurrence_id	INTEGER," +
    "	recurrence_id_tz	TEXT," +
    /*  CAL_ITEM_FLAG_PRIVATE = 1 */
    /*  CAL_ITEM_FLAG_HAS_ATTENDEES = 2 */
    /*  CAL_ITEM_FLAG_HAS_PROPERTIES = 4 */
    /*  CAL_ITEM_FLAG_EVENT_ALLDAY = 8 */
    /*  CAL_ITEM_FLAG_HAS_RECURRENCE = 16 */
    /*  CAL_ITEM_FLAG_HAS_EXCEPTIONS = 32 */
    "	flags		INTEGER," +
    /*  Event bits */
    "	event_start	INTEGER," +
    "	event_start_tz	TEXT," +
    "	event_end	INTEGER," +
    "	event_end_tz	TEXT," +
    "	event_stamp	INTEGER," +
    /*  alarm time */
    "	alarm_time	INTEGER," +
    "	alarm_time_tz	TEXT," +
    "	alarm_offset	INTEGER," +
    "	alarm_related	INTEGER," +
    "	alarm_last_ack	INTEGER" +
    "",

  cal_todos:
    /* 	REFERENCES cal_calendars.id, */
    "	cal_id		INTEGER, " +
    /*  ItemBase bits */
    "	id		TEXT," +
    "	time_created	INTEGER," +
    "	last_modified	INTEGER," +
    "	title		TEXT," +
    "	priority	INTEGER," +
    "	privacy		TEXT," +
    "	ical_status	TEXT," +
    "	recurrence_id	INTEGER," +
    "	recurrence_id_tz	TEXT," +
    /*  CAL_ITEM_FLAG_PRIVATE = 1 */
    /*  CAL_ITEM_FLAG_HAS_ATTENDEES = 2 */
    /*  CAL_ITEM_FLAG_HAS_PROPERTIES = 4 */
    /*  CAL_ITEM_FLAG_EVENT_ALLDAY = 8 */
    /*  CAL_ITEM_FLAG_HAS_RECURRENCE = 16 */
    /*  CAL_ITEM_FLAG_HAS_EXCEPTIONS = 32 */
    "	flags		INTEGER," +
    /*  Todo bits */
    /*  date the todo is to be displayed */
    "	todo_entry	INTEGER," +
    "	todo_entry_tz	TEXT," +
    /*  date the todo is due */
    "	todo_due	INTEGER," +
    "	todo_due_tz	TEXT," +
    /*  date the todo is completed */
    "	todo_completed	INTEGER," +
    "	todo_completed_tz TEXT," +
    /*  percent the todo is complete (0-100) */
    "	todo_complete	INTEGER," +
    /*  alarm time */
    "	alarm_time	INTEGER," +
    "	alarm_time_tz	TEXT," +
    "	alarm_offset	INTEGER," +
    "	alarm_related	INTEGER," +
    "	alarm_last_ack	INTEGER" +
    "",

  cal_attendees:
    "	item_id         TEXT," +
    "	recurrence_id	INTEGER," +
    "	recurrence_id_tz	TEXT," +
    "	attendee_id	TEXT," +
    "	common_name	TEXT," +
    "	rsvp		INTEGER," +
    "	role		TEXT," +
    "	status		TEXT," +
    "	type		TEXT" +
    "",

  cal_recurrence:
    "	item_id		TEXT," +
    /*  the index in the recurrence array of this thing */
    "	recur_index	INTEGER, " +
    /*  values from calIRecurrenceInfo; if null, date-based. */
    "	recur_type	TEXT, " +
    "	is_negative	BOOLEAN," +
    /*  */
    /*  these are for date-based recurrence */
    /*  */
    /*  comma-separated list of dates */
    "	dates		TEXT," +
    /*  */
    /*  these are for rule-based recurrence */
    /*  */
    "	count		INTEGER," +
    "	end_date	INTEGER," +
    "	interval	INTEGER," +
    /*  components, comma-separated list or null */
    "	second		TEXT," +
    "	minute		TEXT," +
    "	hour		TEXT," +
    "	day		TEXT," +
    "	monthday	TEXT," +
    "	yearday		TEXT," +
    "	weekno		TEXT," +
    "	month		TEXT," +
    "	setpos		TEXT" +
    "",

  cal_properties:
    "	item_id		TEXT," +
    "	recurrence_id	INTEGER," +
    "	recurrence_id_tz	TEXT," +
    "	key		TEXT," +
    "	value		BLOB" +
    "",

};

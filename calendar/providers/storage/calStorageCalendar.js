
//
// calStorageCalendar.js
//

const kStorageServiceContractID = "@mozilla.org/storage/service;1";
const kStorageServiceIID = Components.interfaces.mozIStorageService;

const kCalEventContractID = "@mozilla.org/calendar/item;1&type=event";
const kCalEventIID = Components.interfaces.calIEvent;

const kCalDateTimeContractID = "@mozilla.org/calendar/datetime;1";
const kCalDateTimeIID = Components.interfaces.calIDateTime;

// column ids in cal_events
const EV_HASHID = 0;
const EV_TITLE = 1;
const EV_DESCRIPTION = 2;
const EV_LOCATION = 3;
const EV_CATEGORIES = 4;
const EV_TIME_START = 5;
const EV_TIME_END = 6;
const EV_FLAGS = 7;
const EV_LAST_MODIFIED = 8;
const EV_ALARM_DATA = 9;
const EV_ALARM_TIME = 10;
const EV_RECUR_TYPE = 11;
const EV_RECUR_INTERVAL = 12;
const EV_RECUR_DATA = 13;
const EV_RECUR_END = 14;
const EV_RECUR_FLAGS = 15;

// helpers
function newDateTime(aPRTime) {
    var t = Components.classes[kCalDateTimeContractID]
                      .createInstance(kCalDateTimeIID);
    t.utcTime = aPRTime;
    return t;
}

function calStorageCalendar() {
}

calStorageCalendar.prototype = {
    //
    // private members
    //
    mObservers: Array(),
    mDB: null,
    mItemCache: Array(),

    // database statements
    mSelectEventByHash: null,
    mSelectEventByOid: null,
    mSelectEventsByRange: null,
    mSelectEventsByRangeWithLimit: null,
    mDeleteEventByHash: null,
    mInsertEvent: null,

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

    mBatchMode: false,
    // attribute boolean batchMode;
    get batchMode() { return this.mBatchMode; },
    set batchMode(aBatchMode) {
        this.mBatchMode = aBatchMode;
        
    },

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

    // XXX what happens when we add an observer that already exists with a different filter?
    // we can add them separately, but when we remove, we'll remove all instances
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
        }
        if (this.mItems[aItem.id] != null) {
            // is this an error?
            if (aListener)
                aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                               aItem.id,
                                               aListener.ADD,
                                               "ID already eists for addItem");
            return;
        }

        // is this item an event?
        var event = aItem.QueryInterface(kCalEventIID);
        if (event) {
            flushEvent (event);
        } else {
            aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                           aItem.id,
                                           aListener.ADD,
                                           "Don't know how to add items of the given type");
            return;
        }

        // notify observers
        observeAddItem(aItem);

        // notify the listener
        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aItem.id,
                                           aListener.ADD,
                                           aItem);
    },

    // void modifyItem( in calIItemBase aItem, in calIOperationListener aListener );
    modifyItem: function (aItem, aListener) {
        if (aItem.id == null)
        {
            // this is definitely an error
            if (aListener)
                aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                               aItem.id,
                                               aListener.MODIFY,
                                               "ID for modifyItem item is null");
            return;
        }

        // is this item an event?
        var event = aItem.QueryInterface(kCalEventIID);
        if (event) {
            flushEvent (event);
        } else {
            aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                           aItem.id,
                                           aListener.MODIFY,
                                           "Don't know how to modify items of the given type");
            return;
        }

        // notify observers
        observeModifyItem(modifiedItem, aItem);

        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aItem.id,
                                           aListener.MODIFY,
                                           aItem);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aId, aListener) {
        if (aId == null)
        {
            if (aListener)
                aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                               aId,
                                               aListener.DELETE,
                                               "ID is null for deleteItem");
            return;
        }

        // is this item an event?
        var event = aItem.QueryInterface(kCalEventIID);
        if (event) {
            deleteEvent (aId);
        } else {
            aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                           aItem.id,
                                           aListener.DELETE,
                                           "Don't know how to delete items of the given type");
            return;
        }

        // notify observers XXX
        //observeDeleteItem(deletedItem);

        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aId,
                                           aListener.DELETE,
                                           null);

    },

    // void getItem( in string id, in calIOperationListener aListener );
    getItem: function (aId, aListener) {
        if (!aListener)
            return;

        // XXX what do we load from? this thing is horribly underspecified
        var item = getEventByHash (aId);
        if (item) {
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aId,
                                           aListener.GET,
                                           item);
        } else {
            aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                           aId,
                                           aListener.GET,
                                           "ID not found");
        }
    },

    // void getItems( in nsIIDRef aItemType, in unsigned long aItemFilter, 
    //                in unsigned long aCount, in calIDateTime aRangeStart,
    //                in calIDateTime aRangeEnd, 
    //                in calIOperationListener aListener );
    getItems: function (aItemType, aItemFilter, aCount,
                        aRangeStart, aRangeEnd, aListener)
    {
        if (!aListener)
            return;

        // we ignore aItemFilter; we always return all items.  if
        // aCount != 0, we don't attempt to sort anything, and instead
        // return the first aCount items that match.

        var itemsFound = Array();
        var startTime = 0;
        // endTime needs to be the max value a PRTime can be
        var endTime = 0x7fffffffffffffff;
        if (aRangeStart)
            startTime = aRangeStart.utcTime;
        if (aRangeEnd)
            endTime = aRangeEnd.utcTime;

        if (aItemType != null && aItemType != kCalEventIID) {
            // we don't know what to do with anything but events for now
            aListener.onGetComplete(Components.results.NS_ERROR_FAILURE,
                                    aItemType,
                                    "Don't know how to get the requested item type",
                                    aCount, Array());
            return;
        }

        var events = getEventsByRange();

        if (aListener)
            aListener.onGetComplete (Components.results.NS_OK,
                                     aItemType,
                                     null,
                                     itemsFound);
    },

    // void reportError( in unsigned long errorid, in AUTF8String aMessage );
    reportError: function (aErrorId, aMessage)
    {
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
        try {
            mDB.createTable ("cal_events",
                             "hashid, title, description, location, categories, " +
                             "time_start, time_end, flags, last_modified, " +
                             "alarm_data, alarm_time, " +
                             "recur_type, recur_interval, recur_data, recur_end, recur_flags");
            mDB.createTable ("cal_event_recur_exceptions",
                             "event_id, skip_date");
            mDB.createTable ("cal_event_attendees",
                             "event_id, attendee, attendee_status");
            mDB.createTable ("cal_evnent_attachments",
                             "event_id, attachment_type, attachment_data");

            mDB.executeSimpleSQL ("CREATE INDEX cal_events_hash_idx ON cal_events.hashid");
        } catch (e) {
            // ...
        }

        mSelectEventByHash = mDB.createStatement ("SELECT * FROM cal_events WHERE hashid = ? LIMIT 1");
        mSelectEventByOid = mDB.createStatement ("SELECT * FROM cal_events WHERE oid = ? LIMIT 1");
        // this needs to have the range bound like this: start, end, start, start, end, end
        mSelectEventsByRange = mDB.createStatement ("SELECT * FROM cal_events WHERE (time_start >= ? AND time_end <= ?) OR (time_start <= ? AND time_end > ?) OR (time_start <= ? AND time_end <= ?)");
        mSelectEventsByRangeAndLimit = mDB.createStatement ("SELECT * FROM cal_events WHERE (time_start >= ? AND time_end <= ?) OR (time_start <= ? AND time_end > ?) OR (time_start <= ? AND time_end <= ?) LIMIT ?");

        mDeleteEventByHash = mDB.createStatement ("DELETE FROM cal_events WHERE hashid = ?");

        // XXX create a function helper for this, too easy to screw up with all the ?'s
        mInsertEvent = mDB.createStatement ("INSERT INTO cal_events VALUES (?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?)");
    },

    // database helper functions
    getEventFromRow: function (aDBRow) {
        // create a new generic event
        var e = Components.classes[kCalEventContractID]
        .createInstance(kCalEventIID);

        // read the data from the statement row
        // XXX do I need getAsPRTime?
        e.startDate = newDateTime (aDBRow.getAsInt64(EV_TIME_START));
        e.endDate = newDateTime (aDBRow.getAsInt64(EV_TIME_END));
        e.title = aDBRow.getAsString(EV_TITLE);
        e.description = aDBRow.getAsString(EV_DESCRIPTION);
        e.location = aDBRow.getAsString(EV_LOCATION);
        e.categories = aDBRow.getAsString(EV_CATEGORIES);

        e.id = aID;
        e.parent = this;

        return e;
    },

    // get an event from the db or cache with the given ID
    getEventByHash: function (aID) {
        // cached?
        if (mEventHash[aID] != null)
            return mEventHash[aID];

        var retval;

        // not cached; need to read from the db
        mSelectEventByHash.bindCStringParameter (0, aID);
        if (mSelectEventByHash.executeStep()) {
            var e = getEventFromRow (mSelectEventByHash);
            retval = e;
            // stick into cache
            mEventHash[aID] = e;
        } else {
            // not found
            // throw? return null?
            retval = null;
        }

        mSelectEventByHash.reset();

        return retval;
    },

    // get a list of events from the db in the given range
    getEventsByRange: function (aRangeStart, aRangeEnd, aCount) {
        var stmt = mSelectEventsByRange;
        if (aCount)
            stmt = mSelectEventsByRangeAndLimit;

        stmt.bindInt64Parameter(0, aRangeStart);
        stmt.bindInt64Parameter(1, aRangeEnd);
        stmt.bindInt64Parameter(2, aRangeStart);
        stmt.bindInt64Parameter(3, aRangeStart);
        stmt.bindInt64Parameter(4, aRangeEnd);
        stmt.bindInt64Parameter(5, aRangeEnd);
        if (aCount)
            stmt.bindInt32Parameter(6, aCount);

        var events = Array();
        while (stmt.executeStep()) {
            var e = getEventFromRow(stmt);
            events.push (e);
            mEventHash[e.id] = e;
        }

        return events;
    },

    // write this event's changed data to the db
    flushEvent: function (aEvent) {
        // set up statements before executing the transaction
        mDeleteEventByHash.bindCStringParameter (0, aEvent.id);

        mInsertEvent.bindCStringParameter (EV_HASHID, aEvent.id);

        mInsertEvent.bindInt64Parameter (EV_TIME_START, aEvent.start.utcTime);
        mInsertEvent.bindInt64Parameter (EV_TIME_END, aEvent.end.utcTime);

        mInsertEvent.bindStringParameter (EV_TITLE, aEvent.title);
        mInsertEvent.bindStringParameter (EV_DESCRIPTION, aEvent.description);
        mInsertEvent.bindStringParameter (EV_LOCATION, aEvent.location);
        mInsertEvent.bindStringParameter (EV_CATEGORIES, aEvent.categories);

        mDB.beginTransaction();
        mDeleteEventByHash.execute();
        mInsertEvent.execute();
        mDB.commitTransaction();

        mDeleteEventByHash.reset();
        mInsertEvent.reset();

        mEventHash[aEvent.id] = aEvent;
    },

    // delete the event with the given uid
    deleteEvent: function (aID) {
        mDeleteEventByHash.bindCStringParameter (0, aID);
        mDeleteEventByHash.execute ();
        mDeleteEventByHash.reset ();

        delete mEventHash[aID];
    },
}


/****
 **** module registration
 ****/

var calStorageCalendarModule = {
    mCID: Components.ID("{b3eaa1c4-5dfe-4c0a-b62a-b3a514218461}"),
    mContractID: "@mozilla.org/calendar/calendar;1&type=storage",
    
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

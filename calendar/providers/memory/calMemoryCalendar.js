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
// calMemoryCalendar.js
//

const calCalendarManagerContractID = "@mozilla.org/calendar/manager;1";
const calICalendarManager = Components.interfaces.calICalendarManager;

const USECS_PER_SECOND = 1000000;

function calMemoryCalendar() {
    this.wrappedJSObject = this;
    this.calendarToReturn = this,
    this.initMemoryCalendar();
}

// END_OF_TIME needs to be the max value a PRTime can be
const START_OF_TIME = -0x7fffffffffffffff;
const END_OF_TIME = 0x7fffffffffffffff;

calMemoryCalendar.prototype = {
    // This will be returned from getItems as the calendar. The ics
    // calendar overwrites this.
    calendarToReturn: null,

    //
    // nsISupports interface
    // 
    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calICalendarProvider) &&
            !aIID.equals(Components.interfaces.calICalendar)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    initMemoryCalendar: function() {
        this.mObservers = Array();
        this.mItems = { };
    },

    //
    // calICalendarProvider interface
    //
    get prefChromeOverlay() {
        return null;
    },

    get displayName() {
        return calGetString("calendar", "memoryName");
    },

    createCalendar: function mem_createCal() {
        throw NS_ERROR_NOT_IMPLEMENTED;
    },

    deleteCalendar: function mem_deleteCal(cal, listener) {
        cal = cal.wrappedJSObject;
        cal.mItems = {};

        try {
            listener.onDeleteCalendar(cal, Components.results.NS_OK, null);
        } catch(ex) {}
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
    get type() { return "memory"; },

    mReadOnly: false,

    // Most of the time you can just let the ics calendar handle this
    get readOnly() { 
        return this.mReadOnly;
    },
    set readOnly(bool) {
        this.mReadOnly = bool;
    },

    get canRefresh() {
        return false;
    },

    // attribute nsIURI uri;
    mUri: null,
    get uri() { return this.mUri; },
    set uri(aURI) { this.mUri = aURI; },


    refresh: function() {
        // no-op
    },

    // attribute boolean suppressAlarms;
    get suppressAlarms() { return false; },
    set suppressAlarms(aSuppressAlarms) { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

    // void addObserver( in calIObserver observer );
    addObserver: function (aObserver, aItemFilter) {
        if (this.mObservers.some(function(o) { return (o == aObserver); }))
            return;

        this.mObservers.push(aObserver);
    },

    // void removeObserver( in calIObserver observer );
    removeObserver: function (aObserver) {
        var newObservers = this.mObservers.filter(function(o) { return (o != aObserver); });
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
        if (aItem.id == null && aItem.isMutable)
            aItem.id = getUUID();

        if (aItem.id == null) {
            if (aListener)
                aListener.onOperationComplete (this.calendarToReturn,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.ADD,
                                               aItem.id,
                                               "Can't set ID on non-mutable item to addItem");
            return;
        }

        if (this.mItems[aItem.id] != null) {
            // is this an error?
            if (aListener)
                aListener.onOperationComplete (this.calendarToReturn,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.ADD,
                                               aItem.id,
                                               "ID already exists for addItem");
            return;
        }

        aItem.calendar = this.calendarToReturn;
        var rec = aItem.recurrenceInfo;
        if (rec) {
            var exceptions = rec.getExceptionIds({});
            for each (var exid in exceptions) {
                var exception = rec.getExceptionFor(exid, false);
                if (exception) {
                    if (!exception.isMutable) {
                        exception = exception.clone();
                    }
                    exception.calendar = this.calendarToReturn;
                    rec.modifyException(exception);
                }
            }
        }
        
        aItem.generation = 1;
        aItem.makeImmutable();
        this.mItems[aItem.id] = aItem;

        // notify the listener
        if (aListener)
            aListener.onOperationComplete (this.calendarToReturn,
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
        if (!aNewItem) {
            throw Components.results.NS_ERROR_FAILURE;
        }
        if (aNewItem.id == null || this.mItems[aNewItem.id] == null) {
            // this is definitely an error
            if (aListener)
                aListener.onOperationComplete (this.calendarToReturn,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aNewItem.id,
                                               "ID for modifyItem doesn't exist, is null, or is from different calendar");
            return;
        }

        // do the old and new items match?
        if (aOldItem.id != aNewItem.id) {
            if (aListener)
                aListener.onOperationComplete (this.calendarToReturn,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aNewItem.id,
                                               "item ID mismatch between old and new items");
            return;
        }
        
        if (aNewItem.parentItem != aNewItem) {
            aNewItem.parentItem.recurrenceInfo.modifyException(aNewItem);
            aNewItem = aNewItem.parentItem;
        }
        aOldItem = aOldItem.parentItem;

        if (!compareItems(this.mItems[aOldItem.id], aOldItem)) {
            if (aListener)
                aListener.onOperationComplete (this.calendarToReturn,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aNewItem.id,
                                               "old item mismatch in modifyItem");
            return;
        }

        if (aOldItem.generation != aNewItem.generation) {
            if (aListener)
                aListener.onOperationComplete (this.calendarToReturn,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aNewItem.id,
                                               "generation mismatch in modifyItem");
            return;
        }

        var modifiedItem = aNewItem.clone();
        modifiedItem.generation += 1;
        modifiedItem.makeImmutable();
        this.mItems[aNewItem.id] = modifiedItem;

        if (aListener)
            aListener.onOperationComplete (this.calendarToReturn,
                                           Components.results.NS_OK,
                                           aListener.MODIFY,
                                           modifiedItem.id,
                                           modifiedItem);
        // notify observers
        this.observeModifyItem(modifiedItem, aOldItem);
    },

    // void deleteItem( in calIItemBase aItem, in calIOperationListener aListener );
    deleteItem: function (aItem, aListener) {
        if (this.readOnly) 
            throw Components.interfaces.calIErrors.CAL_IS_READONLY;
        if (aItem.id == null || this.mItems[aItem.id] == null) {
            if (aListener)
                aListener.onOperationComplete (this.calendarToReturn,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.DELETE,
                                               aItem.id,
                                               "ID is null or is from different calendar in deleteItem");
            return;
        }

        var oldItem = this.mItems[aItem.id];
        if (oldItem.generation != aItem.generation) {
            if (aListener)
                aListener.onOperationComplete (this.calendarToReturn,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.DELETE,
                                               aItem.id,
                                               "generation mismatch in deleteItem");
            return;
        }

        delete this.mItems[aItem.id];

        if (aListener)
            aListener.onOperationComplete (this.calendarToReturn,
                                           Components.results.NS_OK,
                                           aListener.DELETE,
                                           aItem.id,
                                           null);
        // notify observers
        this.observeDeleteItem(oldItem);
    },

    // void getItem( in string id, in calIOperationListener aListener );
    getItem: function (aId, aListener) {
        if (!aListener)
            return;

        if (aId == null ||
            this.mItems[aId] == null) {
            aListener.onOperationComplete(this.calendarToReturn,
                                          Components.results.NS_ERROR_FAILURE,
                                          aListener.GET,
                                          null,
                                          "IID doesn't exist for getItem");
            return;
        }

        var item = this.mItems[aId];
        var iid = null;

        if (item instanceof Ci.calIEvent) {
            iid = Ci.calIEvent;
        } else if (item instanceof Ci.calITodo) {
            iid = Ci.calITodo;
        } else {
            aListener.onOperationComplete (this.calendarToReturn,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.GET,
                                           aId,
                                           "Can't deduce item type based on QI");
            return;
        }

        aListener.onGetResult (this.calendarToReturn,
                               Components.results.NS_OK,
                               iid,
                               null, 1, [item]);

        aListener.onOperationComplete (this.calendarToReturn,
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
        const calIRecurrenceInfo = Components.interfaces.calIRecurrenceInfo;

        var itemsFound = Array();
        var startTime = START_OF_TIME;
        var endTime = END_OF_TIME;
        if (aRangeStart)
            startTime = aRangeStart.nativeTime;
        if (aRangeEnd)
            endTime = aRangeEnd.nativeTime;

        //
        // filters
        //

        // item base type
        var wantEvents = ((aItemFilter & calICalendar.ITEM_FILTER_TYPE_EVENT) != 0);
        var wantTodos = ((aItemFilter & calICalendar.ITEM_FILTER_TYPE_TODO) != 0);
        if(!wantEvents && !wantTodos) {
            // bail.
            aListener.onOperationComplete (this.calendarToReturn,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.GET,
                                           null,
                                           "Bad aItemFilter passed to getItems");
            return;
        }

        // completed?
        var itemCompletedFilter = ((aItemFilter & calICalendar.ITEM_FILTER_COMPLETED_YES) != 0);
        var itemNotCompletedFilter = ((aItemFilter & calICalendar.ITEM_FILTER_COMPLETED_NO) != 0);

        // return occurrences?
        var itemReturnOccurrences = ((aItemFilter & calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) != 0);

        // figure out the return interface type
        var typeIID = null;
        if (itemReturnOccurrences) {
            typeIID = Ci.calIItemBase;
        } else {
            if (wantEvents && wantTodos) {
                typeIID = Ci.calIItemBase;
            } else if (wantEvents) {
                typeIID = Ci.calIEvent;
            } else if (wantTodos) {
                typeIID = Ci.calITodo;
            }
        }

        //  if aCount != 0, we don't attempt to sort anything, and
        //  instead return the first aCount items that match.

        for (var itemIndex in this.mItems) {
            var item = this.mItems[itemIndex];
            var itemtoadd = null;

            var itemStartTime = 0;
            var itemEndTime = 0;

            var tmpitem = item;
            if (wantEvents && (item instanceof Ci.calIEvent)) {
                tmpitem = item.QueryInterface(Ci.calIEvent);
                itemStartTime = (item.startDate
                                 ? item.startDate.nativeTime
                                 : START_OF_TIME);
                itemEndTime = (item.endDate
                               ? item.endDate.nativeTime
                               : END_OF_TIME);
            } else if (wantTodos && (item instanceof Ci.calITodo)) {
                // if it's a todo, also filter based on completeness
                if (item.isCompleted && !itemCompletedFilter)
                    continue;
                else if (item.isCompleted && !itemNotCompletedFilter)
                    continue;

                itemStartTime = (item.entryDate
                                 ? item.entryDate.nativeTime
                                 : START_OF_TIME);
                itemEndTime = (item.dueDate
                               ? item.dueDate.nativeTime
                               : END_OF_TIME);
            } else {
                // XXX unknown item type, wth do we do?
                continue;
            }

            // Correct for floating
            if (aRangeStart && item.startDate && item.startDate.timezone == 'floating')
                itemStartTime -= aRangeStart.timezoneOffset * USECS_PER_SECOND;
            if (aRangeEnd && item.endDate && item.endDate.timezone == 'floating')
                itemEndTime -= aRangeEnd.timezoneOffset * USECS_PER_SECOND;

            if (itemStartTime < endTime) {
                // figure out if there are recurrences here we care about
                if (itemReturnOccurrences && item.recurrenceInfo)
                {
                    // there might be some recurrences here that we need to handle
                    var recs = item.recurrenceInfo.getOccurrences (aRangeStart, aRangeEnd, 0, {});
                    itemsFound = itemsFound.concat(recs);
                } else if (itemEndTime >= startTime) {
                    itemsFound.push(item);
                }
            }

            if (aCount && itemsFound.length >= aCount)
                break;
        }

        aListener.onGetResult (this.calendarToReturn,
                               Components.results.NS_OK,
                               typeIID,
                               null,
                               itemsFound.length,
                               itemsFound);

        aListener.onOperationComplete (this.calendarToReturn,
                                       Components.results.NS_OK,
                                       aListener.GET,
                                       null,
                                       null);
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
    }
}

/****
 **** module registration
 ****/

var calMemoryCalendarModule = {
    mCID: Components.ID("{bda0dd7f-0a2f-4fcf-ba08-5517e6fbf133}"),
    mContractID: "@mozilla.org/calendar/calendar;1?type=memory",

    mUtilsLoaded: false,
    loadUtils: function memoryLoadUtils() {
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
                                        "Calendar in-memory back-end",
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

        this.loadUtils();

        return this.mFactory;
    },

    mFactory: {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            return (new calMemoryCalendar()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return calMemoryCalendarModule;
}

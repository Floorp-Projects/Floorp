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
 *   Dan Mosedale <dan.mosedale@oracle.com>
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
// calDavCalendar.js
//

function calDavCalendar() {
    this.wrappedJSObject = this;
}

calDavCalendar.prototype = {
    //
    // private members
    //
    mObservers: Array(),
    mItems: Array(),
    mUri: "",

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

    // attribute nsIURI uri;
    get uri() { return mURI },
    set uri(aURI) { mURI = aURI; },

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
                newObservers.push(mObservers[i]);
        }
        this.mObservers = newObservers;
    },

    // void addItem( in calIItemBase aItem, in calIOperationListener aListener );
    addItem: function (aItem, aListener) {
        if (aItem.id == null) {
            aItem.id = "uuid:" + (new Date()).getTime();
        }
        if (this.mItems[aItem.id] != null) {
            // is this an error?
            if (aListener)
                aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                               aListener.ADD,
                                               aItem.id,
                                               "ID already eists for addItem");
            return;
        }

        this.mItems[aItem.id] = aItem;

        // notify observers
        this.observeAddItem(aItem);

        // notify the listener
        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aListener.ADD,
                                           aItem.id,
                                           aItem);
    },

    // void modifyItem( in calIItemBase aItem, in calIOperationListener aListener );
    modifyItem: function (aItem, aListener) {
        if (aItem.id == null ||
            this.mItems[aItem.id] == null)
        {
            // this is definitely an error
            if (aListener)
                aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                               aListener.MODIFY,
                                               aItem.id,
                                               "ID for modifyItem doesn't exist or is null");
            return;
        }

        // technically, they should be the same item
        var modifiedItem = this.mItems[aItem.id];
        this.mItems[aItem.id] = aItem;

        // notify observers
        this.observeModifyItem(modifiedItem, aItem);

        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aListener.MODIFY,
                                           aItem.id,
                                           aItem);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aId, aListener) {
        if (aId == null ||
            this.mItems[aId] == null)
        {
            if (aListener)
                aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                               aListener.DELETE,
                                               aId,
                                               "ID doesn't exist for deleteItem");
            return;
        }

        deletedItem = this.mItems[aId];
        delete this.mItems[aId];

        // notify observers
        observeDeleteItem(deletedItem);

        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aListener.DELETE,
                                           aId,
                                           null);

    },

    // void getItem( in string id, in calIOperationListener aListener );
    getItem: function (aId, aListener) {
        if (!aListener)
            return;

        if (aId == null ||
            this.mItems[aId] == null)
        {
            aListener.onOperationComplete(Components.results.NS_ERROR_FAILURE,
                                          aListener.GET,
                                          null,
                                          "IID doesn't exist for getItem");
            return;
        }

        var item = this.mItems[aId];
        var iid = null;

        if (item.QueryInterface(Components.interfaces.calIEvent)) {
            iid = Components.interfaces.calIEvent;
        } else if (item.QueryInterface(Components.interfaces.calITodo)) {
            iid = Components.interfaces.calITodo;
        } else {
            aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                           aListener.GET,
                                           aId,
                                           "Can't deduce item type based on QI");
            return;
        }

        aListener.onGetResult (Components.results.NS_OK,
                               iid,
                               null, 1, [item]);

        aListener.onOperationComplete (Components.results.NS_OK,
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

        //
        // filters
        //

        // item base type
        var itemTypeFilter = null;
        if (aItemFilter & calICalendar.ITEM_FILTER_TYPE_ALL)
            itemTypeFilter = calIItemBase;
        else if (aItemFilter & calICalendar.ITEM_FILTER_TYPE_EVENT)
            itemTypeFilter = calIEvent;
        else if (aItemFilter & calICalendar.ITEM_FILTER_TYPE_TODO)
            itemTypeFilter = calITodo;
        else {
            // bail.
            aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                           aListener.GET,
                                           null,
                                           "Bad aItemFilter passed to getItems");
            return;
        }

        // completed?
        var itemCompletedFilter = ((aItemFilter & calICalendar.ITEM_FILTER_COMPLETED_YES) != 0);
        var itemNotCompletedFilter = ((aItemFilter & calICalendar.ITEM_FILTER_COMPLETED_YES) != 0);

        // return occurrences?
        var itemReturnOccurrences = ((aItemFilter & calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) != 0);

        //  if aCount != 0, we don't attempt to sort anything, and
        //  instead return the first aCount items that match.

        for (var i = 0; i < this.mItems.length; i++) {
            var item = this.mItems[i];
            var itemtoadd = null;

            if (itemTypeFilter)
                item = item.QueryInterface(itemTypeFilter);

            if (item) {
                var itemStartTime = 0;
                var itemEndTime = 0;

                var tmpitem = item;
                if ((tmpitem = item.QueryInterface(calIEvent)) != null)
                {
                    itemStartTime = tmpitem.startDate.utcTime;
                    itemEndTime = tmpitem.endDate.utcTime;

                    if (itemReturnOccurrences) {
                        itemtoadd = Components.classes["@mozilla.org/calendar/item-occurrence;1"].createInstance(calIItemOccurrence);
                        itemtoadd.wrappedJSObject.item = item;
                        itemtoadd.wrappedJSObject.occurrenceStartDate = tmpitem.startDate;
                        itemtoadd.wrappedJSObject.occurrenceEndDate = tmpitem.endDate;
                    }
                } else if ((tmpitem = item.QueryInterface(calITodo)) != null)
                {
                    // if it's a todo, also filter based on completeness
                    if (tmpitem.percentComplete == 100 && !itemCompletedFilter)
                        continue;
                    else if (tmpitem.percentComplete < 100 && !itemNotCompletedFilter)
                        continue;

                    itemStartTime = tmpitem.entryTime.utcTime;
                    itemEndTime = tmpitem.entryTime.utcTime;

                    if (itemReturnOccurrences) {
                        itemtoadd = Components.classes["@mozilla.org/calendar/item-occurrence;1"].createInstance(calIItemOccurrence);
                        itemtoadd.wrappedJSObject.item = item;
                        itemtoadd.wrappedJSObject.occurrenceStartDate = tmpitem.entryTime;
                        itemtoadd.wrappedJSObject.occurrenceEndDate = tmpitem.entryTime;
                    }
                } else {
                    // XXX unknown item type, wth do we do?
                    continue;
                }

                // determine whether any endpoint falls within the range
                if (itemStartTime <= endTime && itemEndTime >= startTime) {
                    if (itemtoadd == null)
                        itemtoadd = item;

                    itemsFound.push(itemtoadd);
                }
            }

            if (aCount && itemsFound.length >= aCount)
                break;
        }

        aListener.onGetResult (Components.results.NS_OK,
                               itemReturnOccurrences ? calIItemOccurrence : itemTypeFilter,
                               null,
                               itemsFound.length,
                               itemsFound);

        aListener.onOperationComplete (Components.results.NS_OK,
                                       aListener.GET,
                                       null,
                                       null);
    },

    //
    // Helper functions
    //
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
}

/****
 **** module registration
 ****/

var calDavCalendarModule = {
    mCID: Components.ID("{a35fc6ea-3d92-11d9-89f9-00045ace3b8d}"),
    mContractID: "@mozilla.org/calendar/calendar;1&type=caldav",
    
    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.mCID,
                                        "Calendar CalDAV back-end",
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
            return (new calDavCalendar()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return calDavCalendarModule;
}

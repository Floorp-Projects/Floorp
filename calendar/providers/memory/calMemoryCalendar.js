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

function calMemoryCalendar() {
    this.wrappedJSObject = this;
}

calMemoryCalendar.prototype = {
    //
    // private members
    //
    mObservers: Array(),
    mItems: Array(),

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

    // attribute nsIURI uri;
    get uri() { return false; },
    set uri(aURI) { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

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
                newObservers.push(mObservers[i]);
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

        this.mItems[aItem.id] = aItem;

        // notify observers
        this.observeAddItem(aItem);

        // notify the listener
        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aItem.id,
                                           aListener.ADD,
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
                                               aItem.id,
                                               aListener.MODIFY,
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
                                           aItem.id,
                                           aListener.MODIFY,
                                           aItem);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aId, aListener) {
        if (aId == null ||
            this.mItems[aId] == null)
        {
            if (aListener)
                aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                               aId,
                                               aListener.DELETE,
                                               "ID doesn't exist for deleteItem");
            return;
        }

        deletedItem = this.mItems[aId];
        delete this.mItems[aId];

        // notify observers
        observeDeleteItem(deletedItem);

        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aId,
                                           aListener.DELETE,
                                           null);

    },

    // void getItem( in string id, in calIOperationListener aListener );
    getItem: function (aId, aListener) {
        if (aId == null ||
            this.mItems[aId] == null)
        {
            if (aListener)
                aListener.onOperationComplete (Components.results.NS_ERROR_FAILURE,
                                               aId,
                                               aListener.GET,
                                               "ID doesn't exist for getItem");
            return;
        }

        if (aListener)
            aListener.onOperationComplete (Components.results.NS_OK,
                                           aId,
                                           aListener.GET,
                                           this.mItems[aId]);
    },

    // void getItems( in nsIIDRef aItemType, in unsigned long aItemFilter, 
    //                in unsigned long aCount, in calIDateTime aRangeStart,
    //                in calIDateTime aRangeEnd, 
    //                in calIOperationListener aListener );
    getItems: function (aItemType, aItemFilter, aCount,
                        aRangeStart, aRangeEnd, aListener)
    {
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

        const calIEvent = Components.interfaces.calIEvent;
        const calITodo = Components.interfaces.calITodo;

        for (var i = 0; i < this.mItems.length; i++) {
            var item = this.mItems[i];

            if (aItemType)
                item = item.QueryInterface(aItemType);
            if (item) {
                var itemStartTime = 0;
                var itemEndTime = 0;

                var tmpitem = item;
                if (aItemType == calIEvent ||
                    (tmpitem = item.QueryInterface(calIEvent)) != null)
                {
                    itemStartTime = tmpitem.startDate.utcTime;
                    itemEndTime = tmpitem.endDate.utcTime;
                } else if (aItemType == calITodo ||
                           (tmpitem = item.QueryInterface(calITodo)) != null)
                {
                    itemStartTime = tmpitem.entryTime.utcTime;
                    itemEndTime = tmpitem.entryTime.utcTime;
                } else {
                    // XXX unknown item type, wth do we do?
                    continue;
                }

                // determine whether any endpoint falls within the range
                if ((itemStartTime >= startTime && itemEndTime <= endTime) ||
                    (itemStartTime <= startTime && itemEndTime > startTime) ||
                    (itemStartTime <= endTime && itemEndTime >= endTime))
                {
                    itemsFound.push(item);
                }

                if (aCount && itemsFound.length >= aCount)
                    break;
            }
        }

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

var calMemoryCalendarModule = {
    mCID: Components.ID("{bda0dd7f-0a2f-4fcf-ba08-5517e6fbf133}"),
    mContractID: "@mozilla.org/calendar/calendar;1&type=memory",
    
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

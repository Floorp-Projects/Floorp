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
 *   Mike Shaver <mike.x.shaver@oracle.com>
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
    this.mObservers = { };
    this.mItems = { };
}

function makeOccurrence(item, start, end)
{
    var occ = Components.classes["@mozilla.org/calendar/item-occurrence;1"].
        createInstance(calIItemOccurrence);
    // XXX poor form
    occ.wrappedJSObject.item = item;
    occ.wrappedJSObject.occurrenceStartDate = item.startDate;
    occ.wrappedJSObject.occurrenceEndDate = item.endDate;
    return occ;
}
  
// END_OF_TIME needs to be the max value a PRTime can be
const END_OF_TIME = 0x7fffffffffffffff;

calDavCalendar.prototype = {
    //
    // nsISupports interface
    // 
    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calICalendar)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    //
    // nsICalendar interface
    //

    // attribute nsIURI uri;
    mUri: null,
    get uri() { return this.mUri },
    set uri(aUri) { this.mUri = aUri; },

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
        if (aItem.id == null)
            aItem.id = "uuid:" + (new Date()).getTime();

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

        this.mItems[aItem.id] = aItem;

        // notify observers
        this.observeAddItem(aItem);

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
        if (aItem.id == null ||
            this.mItems[aItem.id] == null) {
            // this is definitely an error
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
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
            aListener.onOperationComplete (this,
                                           Components.results.NS_OK,
                                           aListener.MODIFY,
                                           aItem.id,
                                           aItem);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aId, aListener) {
        if (aId == null ||
            this.mItems[aId] == null) {
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
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

        if (aId == null ||
            this.mItems[aId] == null) {
            aListener.onOperationComplete(this,
                                          Components.results.NS_ERROR_FAILURE,
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
            aListener.onOperationComplete (this,
                                           Components.results.NS_ERROR_FAILURE,
                                           aListener.GET,
                                           aId,
                                           "Can't deduce item type based on QI");
            return;
        }

        aListener.onGetResult (this,
                               Components.results.NS_OK,
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
        if (!aListener) {
            throw Components.results.NS_ERROR_ILLEGAL_VALUE;
        }

        // XXX for now, we're just gonna start off getting events.
        // worry about todos and other stuff later

        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);

        // XXX we really should use DASL SEARCH or CalDAV REPORT, but the only
        // server i can possibly test against doesn't yet support those

        // So we need a list of items to iterate over.  Weirdly, we have to 
        // search 
        var eventDirUri = this.mUri.clone();
        eventDirUri.spec = eventDirUri.spec + "calendar/events/";
        var eventDirResource = new WebDavResource(eventDirUri);

        var propsToGet = ["DAV: getlastmodified"];
        webSvc.getResourceProperties(eventDirResource, propsToGet.length, 
                                     propsToGet, true, this, aListener);

    },

    // onOperation* methods for nsIWebDavListener
    onOperationComplete: function (aStatusCode, aResource, aOperation, 
                                   aClosure) 
    {
        // XXX aClosure is the listener for now
        aClosure.onOperationComplete(this, aStatusCode, 0, null, null);
    },

    onOperationDetail: function(aStatusCode, aResource, aOperation, aDetail,
                                aClosure)
    {
        dump("calDavCalendar.onOperationDetail() called\n");
    },

    oldGetItems: function (aItemFilter, aCount,
                           aRangeStart, aRangeEnd, aListener)
    {
        const calICalendar = Components.interfaces.calICalendar;
        const calIItemBase = Components.interfaces.calIItemBase;
        const calIEvent = Components.interfaces.calIEvent;
        const calITodo = Components.interfaces.calITodo;
        const calIItemOccurrence = Components.interfaces.calIItemOccurrence;

        var itemsFound = Array();
        var startTime = 0;
        var endTime = END_OF_TIME;
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
        var itemNotCompletedFilter = ((aItemFilter & calICalendar.ITEM_FILTER_COMPLETED_NO) != 0);

        // return occurrences?
        var itemReturnOccurrences = ((aItemFilter & calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) != 0);

        //  if aCount != 0, we don't attempt to sort anything, and
        //  instead return the first aCount items that match.

        for (var i in this.mItems) {
            var item = this.mItems[i];
            var itemtoadd = null;
            if (itemTypeFilter && !(item instanceof itemTypeFilter))
                continue;

            var itemStartTime = 0;
            var itemEndTime = 0;
            
            var tmpitem = item;
            if (item instanceof calIEvent) {
                tmpitem = item.QueryInterface(calIEvent);
                itemStartTime = item.startDate.utcTime || 0
                itemEndTime = item.endDate.utcTime || END_OF_TIME;
                
                if (itemReturnOccurrences)
                    itemtoadd = makeOccurrence(item, item.startDate, item.endDate);
            } else if (item instanceof calITodo) {
                // if it's a todo, also filter based on completeness
                if (item.percentComplete == 100 && !itemCompletedFilter)
                    continue;
                else if (item.percentComplete < 100 && !itemNotCompletedFilter)
                    continue;
                    
                itemEndTime = itemStartTime = item.entryTime.utcTime || 0;
                
                if (itemReturnOccurrences)
                    itemtoadd = makeOccurrence(item, item.entryTime, item.entryTime);
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

            if (aCount && itemsFound.length >= aCount)
                break;
        }

        aListener.onGetResult (this,
                               Components.results.NS_OK,
                               itemReturnOccurrences ? calIItemOccurrence : itemTypeFilter,
                               null,
                               itemsFound.length,
                               itemsFound);

        aListener.onOperationComplete (this,
                                       Components.results.NS_OK,
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

function WebDavResource(url)
{
    this.mResourceURL = url;
}

WebDavResource.prototype = {
    mResourceURL: {},
    get resourceURL() { 
        dump(this.mResourceURL + "\n");
        return this.mResourceURL;}  ,
    QueryInterface: function(outer, iid) {
        if (iid.equals(CI.nsIWebDAVResource) ||
            iid.equals(CI.nsISupports)) {
            return this;
        }
       
        throw Components.interfaces.NS_NO_INTERFACE;
    }
};

/****
 **** module registration
 ****/

var calDavCalendarModule = {
    mCID: Components.ID("{a35fc6ea-3d92-11d9-89f9-00045ace3b8d}"),
    mContractID: "@mozilla.org/calendar/calendar;1?type=caldav",
    
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

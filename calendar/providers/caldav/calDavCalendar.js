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


// XXXdmose need to make and use better error reporting interface for webdav
// (all uses of aStatusCode, probably)

// XXXdmose translate most/all dumps() to NSPR logging, once that's available
// from JS

// XXXdmose deal with etags

// XXXdmose deal with locking

// XXXdmose observers or listeners first?

function calDavCalendar() {
    this.wrappedJSObject = this;
    this.mObservers = { };
    this.mItems = { };
}

// some shorthand
const nsIWebDAVOperationListener = 
    Components.interfaces.nsIWebDAVOperationListener;
const calICalendar = Components.interfaces.calICalendar;
const nsISupportsCString = Components.interfaces.nsISupportsCString;
const calIEvent = Components.interfaces.calIEvent;
const calEventClass = Components.classes["@mozilla.org/calendar/event;1"];

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
            !aIID.equals(calICalendar)) {
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

        // XXX do we need to check the server to see if this already exists?

        // XXX how are we REALLY supposed to figure this out?
        var eventUri = this.mUri.clone();
        eventUri.spec = eventUri.spec + "calendar/events/" + aItem.id + ".ics";
        var eventResource = new WebDavResource(eventUri);

        var listener = new WebDavListener();
        listener.onOperationComplete = function(aStatusCode, aResource,
                                                aOperation, aClosure) {

            // 201 = HTTP "Created"
            //
            if (aStatusCode == 201) {
                dump("Item added successfully.\n");
                var retVal = Components.results.NS_OK;

                // notify observers
                // XXX should be called after listener?
                this.observeAddItem(aItem);

            } else {
                dump("Error adding item: " + aStatusCode + "\n");
                retVal = Components.results.NS_ERROR_FAILURE;

            }

            // XXX ensure immutable version returned

            // notify the listener
            if (aListener)
                aListener.onOperationComplete (this,
                                               retVal,
                                               aListener.ADD,
                                               aItem.id,
                                               aItem);
        }

        dump("icalString = " + aItem.icalString + "\n");
        // do WebDAV put
        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);
        webSvc.putFromString(eventResource, "text/calendar", aItem.icalString, 
                             listener, null);

        return;
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

        // XXX we really should use DASL SEARCH or CalDAV REPORT, but the only
        // server we can possibly test against doesn't yet support those

        function searchItemsLocally(calendarToReturn, aAllItems) {

            if (aId == null || aAllItems[aId] == null) {
                aListener.onOperationComplete(this,
                                              Components.results.NS_ERROR_FAILURE,
                                              aListener.GET,
                                              null,
                                              "IID doesn't exist for getItem");
                // XXX FAILURE is a bad choice
                return;
            }

            var item = aAllItems[aId];
            var iid = null;

            if (item.QueryInterface(calIEvent)) {
                iid = calIEvent;
            } else if (item.QueryInterface(Components.interfaces.calITodo)) {
                iid = Components.interfaces.calITodo;
            } else {
                aListener.onOperationComplete(this,
                                              Components.results.NS_ERROR_FAILURE,
                                              aListener.GET,
                                              aId,
                                              "Can't deduce item type based on QI");
                return;
            }

            aListener.onGetResult(calendarToReturn, Components.results.NS_OK,
                                  iid, null, 1, [item]);

            aListener.onOperationComplete(calendarToReturn,
                                          Components.results.NS_OK,
                                          aListener.GET, aId, null);
        }

        this.getAllEvents(aListener, searchItemsLocally);

        return;
    },


    // void getItems( in unsigned long aItemFilter, in unsigned long aCount, 
    //                in calIDateTime aRangeStart, in calIDateTime aRangeEnd,
    //                in calIOperationListener aListener );
    getItems: function (aItemFilter, aCount, aRangeStart, aRangeEnd, aListener)
    {
        // XXX we really should use DASL SEARCH or CalDAV REPORT, but the only
        // server we can possibly test against doesn't yet support those

        // this code copy-pasted from calMemoryCalendar.getItems()
        function searchItemsLocally(calendarToReturn, aAllItems){
            const calIItemBase = Components.interfaces.calIItemBase;
            const calITodo = Components.interfaces.calITodo;
            const calIItemOccurrence = Components.interfaces.calIItemOccurrence;
            var itemsFound = Array();
            var startTime = 0;
            var endTime = END_OF_TIME;
            if (aRangeStart)
                startTime = aRangeStart.nativeTime;
            if (aRangeEnd)
                endTime = aRangeEnd.nativeTime;

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

            for (var i in aAllItems) {
                var item = aAllItems[i];
                var itemtoadd = null;
                if (itemTypeFilter && !(item instanceof itemTypeFilter))
                    continue;

                var itemStartTime = 0;
                var itemEndTime = 0;
            
                var tmpitem = item;
                if (item instanceof calIEvent) {
                    tmpitem = item.QueryInterface(calIEvent);
                    itemStartTime = item.startDate.nativeTime || 0;
                    itemEndTime = item.endDate.nativeTime || END_OF_TIME;
                
                    if (itemReturnOccurrences)
                        itemtoadd = makeOccurrence(item, item.startDate, item.endDate);
                } else if (item instanceof calITodo) {
                    // if it's a todo, also filter based on completeness
                    if (item.percentComplete == 100 && !itemCompletedFilter)
                        continue;
                    else if (item.percentComplete < 100 && !itemNotCompletedFilter)
                        continue;
                    
                    itemEndTime = itemStartTime = item.entryTime.nativeTime || 0;
                
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

            dump("itemsFound = " + itemsFound + "\n");
            aListener.onGetResult (calendarToReturn,
                                   Components.results.NS_OK,
                                   itemReturnOccurrences ? calIItemOccurrence : itemTypeFilter,
                                   null,
                                   itemsFound.length,
                                   itemsFound);

            aListener.onOperationComplete (calendarToReturn,
                                           Components.results.NS_OK,
                                           aListener.GET,
                                           null,
                                           null);
            return;
        };

        // XXX for now, we're just gonna start off getting events.
        // worry about todos and other stuff later
        this.getAllEvents(aListener, searchItemsLocally);
    },

    // A nasty little hack until there is a CalDAV server in existence to 
    // test against which implements DASL SEARCH or CalDAV REPORT.   Basically
    // we get all the events here, and then the caller specifies an 
    // aInternalCallback function that it uses to do the actual searching
    // and callbacks
    //
    getAllEvents: function(aListener, aInternalCallback) {
        if (!aListener) {
            throw Components.results.NS_ERROR_ILLEGAL_VALUE;
        }

        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);

        // So we need a list of items to iterate over.  Should theoretically 
        // use search to find this out
        var eventDirUri = this.mUri.clone();
        eventDirUri.spec = eventDirUri.spec + "calendar/events/";
        var eventDirResource = new WebDavResource(eventDirUri);

        var propsToGet = ["DAV: getlastmodified"];

        var listener = new WebDavListener();
        var allItems = new Object();
        var listCompleted = false;
        var itemsPending = 0;
        var itemsCompleted = 0;
        listener.onOperationDetail = function(aStatusCode, aResource,
                                              aOperation, aDetail, aClosure) {
            if (aStatusCode != 200) {
                // this property had an error

                // XXX report error to caller, or just log?
                dump("calDavCalendar.listener.onOperationDetail: " + 
                     " property " + aResource + " returned error " + 
                     aStatusCode + "\n");

                return;
            }
                         
            var getListener = new WebDavListener();
            getListener.onOperationDetail = function(aStatusCode, aResource,
                                                     aOperation, aDetail,
                                                     aClosure) {

                dump("getListener.onOperationDetail called\n");
                if (aStatusCode == 200) {

                    // turn the detail item into a datatype we can deal with
                    aDetail.QueryInterface(nsISupportsCString);

                    // create a local event item
                    item = calEventClass.createInstance(calIEvent);

                    // cause returned data to be parsed into the event item
                    item.icalString = aDetail.data;

                    // add the event to the array of all items
                    allItems[item.id] = item;
                    dump("getListener.onOperationDetail: item " + item.id +
                         " added\n");

                } else {
                    // XXX do real error handling here
                    dump("getListener.onOperationDetail: aStatusCode = " +
                         aStatusCode + "\n");
                }

            };

            getListener.onOperationComplete = function(aStatusCode, 
                                                       aResource, aOperation,
                                                       aClosure) {
                dump("getListener.onOperationComplete() called\n");

                if (aStatusCode != 200) {
                    dump("getListener.onOperationComplete: " + 
                         " property " + aResource + " returned error " + 
                         aStatusCode + "\n");

                    aClosure.onGetResult();// XXX call back error
                }

                // we've finished this get; make a note of that
                ++itemsCompleted;

                // if this was the last get we were waiting on, we're done
                if ( listCompleted && itemsPending == itemsCompleted) {

                    // we're done with all the gets
                    aInternalCallback(this, allItems);
                }
            };

            // we don't care about the directory itself, only the children
            // (think of the children!!)  This is a gross hack, but it'll go
            // away once we start using REPORT or SEARCH
            // 
            if (aResource.path == 
                eventDirUri.path.substr(0, eventDirUri.path.length-1)) {
                return;
            }

            // make a note that this request is pending
            ++itemsPending;
            
            // XXX doesn't exist yet
            webSvc.getToString(new WebDavResource(aResource), getListener, 
                               null);

        }
        listener.onOperationComplete = function(aStatusCode, aResource,
                                                aOperation, aClosure) {
            listCompleted = true;

            // if something went wrong while listing, call the error
            // back to the caller, and then give up.

            if (aStatusCode != 207 && aStatusCode != 200) {

                // XXX test to find out return code if no children of events

                dump("calDavCalendar.listener.onOperationComplete: " + 
                     " property " + aResource + " returned error " + 
                     aStatusCode + "\n");

                // aClosure contains the calIOperationListener
                aClosure.onOperationComplete(this, aStatusCode, 
                                             calICalendar.GET, null, null);
            }
        }

        webSvc.getResourceProperties(eventDirResource, propsToGet.length, 
                                     propsToGet, true, listener, aListener);
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
    }
};


function WebDavResource(url) {
    this.mResourceURL = url;
}

WebDavResource.prototype = {
    mResourceURL: {},
    get resourceURL() { 
        return this.mResourceURL;}  ,
    QueryInterface: function(outer, iid) {
        if (iid.equals(CI.nsIWebDAVResource) ||
            iid.equals(CI.nsISupports)) {
            return this;
        }
       
        throw Components.interfaces.NS_NO_INTERFACE;
    }
};

function WebDavListener() {
}

WebDavListener.prototype = { 

    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(nsIWebDavOperationListener)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    onOperationComplete: function(aStatusCode, aResource, aOperation,
                                  aClosure) {
        // aClosure is the listener
        aClosure.onOperationComplete(this, aStatusCode, 0, null, null);

        dump("WebDavListener.onOperationComplete() called\n");
        return;
    },

    onOperationDetail: function(aStatusCode, aResource, aOperation, aDetail,
                                aClosure) {
        dump("WebDavListener.onOperationDetail() called\n");
        return;
    }
}

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

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

// XXXdmose first listeners, then observers

// XXXdmose getItem and getItems() should return immutable events

function calDavCalendar() {
    this.wrappedJSObject = this;
    this.mObservers = [ ];
    this.mItems = { };
}

// some shorthand
const nsIWebDAVOperationListener = 
    Components.interfaces.nsIWebDAVOperationListener;
const calICalendar = Components.interfaces.calICalendar;
const nsISupportsCString = Components.interfaces.nsISupportsCString;
const calIEvent = Components.interfaces.calIEvent;
const calITodo = Components.interfaces.calITodo;
const calEventClass = Components.classes["@mozilla.org/calendar/event;1"];
const calIItemOccurrence = Components.interfaces.calIItemOccurrence;

const kCalCalendarManagerContractID = "@mozilla.org/calendar/manager;1";
const kCalICalendarManager = Components.interfaces.calICalendarManager;


function makeOccurrence(item, start, end)
{
    var occ = Components.classes["@mozilla.org/calendar/item-occurrence;1"].
        createInstance(calIItemOccurrence);
    occ.initialize(item, start, end);

    return occ;
}

var activeCalendarManager = null;
function getCalendarManager()
{
    if (!activeCalendarManager) {
        activeCalendarManager = 
            Components.classes[kCalCalendarManagerContractID].getService(kCalICalendarManager);
    }
    return activeCalendarManager;
}
  
// END_OF_TIME needs to be the max value a PRTime can be
const START_OF_TIME = -0x7fffffffffffffff;
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

    // attribute AUTF8String name;
    get name() {
        return getCalendarManager().getCalendarPref(this, "NAME");
    },
    set name(name) {
        getCalendarManager().setCalendarPref(this, "NAME", name);
    },

    // readonly attribute AUTF8String type;
    get type() { return "caldav"; },

    // attribute nsIURI uri;
    mUri: null,
    get uri() { return this.mUri; },
    set uri(aUri) { this.mUri = aUri; },

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

        if (aItem.id == null && aItem.isMutable)
            aItem.id = "uuid" + (new Date()).getTime();

        if (aItem.id == null) {
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.ADD,
                                               aItem.id,
                                               "Can't set ID on non-mutable item to addItem");
            return;
        }

        // XXX how are we REALLY supposed to figure this out?
        var itemUri = this.mUri.clone();
        itemUri.spec = itemUri.spec + aItem.id + ".ics";
        var eventResource = new WebDavResource(itemUri);

        var listener = new WebDavListener();
        var savedthis = this;
        listener.onOperationComplete = function(aStatusCode, aResource,
                                                aOperation, aClosure) {

            // 201 = HTTP "Created"
            //
            if (aStatusCode == 201) {
                dump("Item added successfully.\n");
                var retVal = Components.results.NS_OK;

                // XXX deal with Location header

                // notify observers
                // XXX should be called after listener?
                savedthis.observeAddItem(newItem);

            } else {
                // XXX real error handling
                dump("Error adding item: " + aStatusCode + "\n");
                retVal = Components.results.NS_ERROR_FAILURE;

            }

            // notify the listener
            if (aListener)
                aListener.onOperationComplete (savedthis,
                                               retVal,
                                               aListener.ADD,
                                               newItem.id,
                                               newItem);
        }

        var newItem = aItem.clone();
        newItem.parent = this;
        newItem.generation = 1;
        newItem.setProperty("locationURI", itemUri.spec);
        newItem.makeImmutable();

        dump("icalString = " + newItem.icalString + "\n");

        // XXX use if not exists
        // do WebDAV put
        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);
        webSvc.putFromString(eventResource, "text/calendar", 
                             newItem.icalString, listener, null);

        return;
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
                                               "ID for modifyItem doesn't exist or is null");
            return;
        }

        var eventUri = this.mUri.clone();
        try {
            eventUri.spec = aItem.getProperty("locationURI");
            dump("using locationURI: " + eventUri.spec + "\n");
        } catch (ex) {
            // XXX how are we REALLY supposed to figure this out?
            eventUri.spec = eventUri.spec + aItem.id + ".ics";
        }

        var eventResource = new WebDavResource(eventUri);

        var listener = new WebDavListener();
        var savedthis = this;
        listener.onOperationComplete = function(aStatusCode, aResource,
                                                aOperation, aClosure) {

            // 204 = HTTP "No Content"
            //
            if (aStatusCode == 204) {
                dump("Item modified successfully.\n");
                var retVal = Components.results.NS_OK;

            } else {
                dump("Error modifying item: " + aStatusCode + "\n");

                // XXX deal with non-existent item here, other
                // real error handling

                // XXX aStatusCode will be 201 Created for a PUT on an item that
                // didn't exist before.

                retVal = Components.results.NS_ERROR_FAILURE;
            }

            // XXX ensure immutable version returned
            // notify listener
            if (aListener) {
                aListener.onOperationComplete (savedthis, retVal,
                                               aListener.MODIFY, aItem.id,
                                               aItem);
            }

            // XXX only if retVal is successn
            // notify observers
            // XXX modified item should be first arg
            savedthis.observeModifyItem(null, aItem);

            return;
        }

        // XXX use if-exists stuff here
        // XXX use etag as generation
        // do WebDAV put
        dump("modifyItem: aItem.icalString = " + aItem.icalString + "\n");
        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);
        webSvc.putFromString(eventResource, "text/calendar", aItem.icalString, 
                             listener, null);

        return;
    },


    // void deleteItem( in calIItemBase aItem, in calIOperationListener aListener );
    deleteItem: function (aItem, aListener) {

        if (aItem.id == null) {
            if (aListener)
                aListener.onOperationComplete (this,
                                               Components.results.NS_ERROR_FAILURE,
                                               aListener.DELETE,
                                               aItem.id,
                                               "ID doesn't exist for deleteItem");
            return;
        }

        // XXX how are we REALLY supposed to figure this out?
        var eventUri = this.mUri.clone();
        eventUri.spec = eventUri.spec + aItem.id + ".ics";
        var eventResource = new WebDavResource(eventUri);

        var listener = new WebDavListener();
        var savedthis = this;
        listener.onOperationComplete = function(aStatusCode, aResource,
                                                aOperation, aClosure) {

            // 204 = HTTP "No content"
            //
            if (aStatusCode == 204) {
                dump("Item deleted successfully.\n");
                var retVal = Components.results.NS_OK;
            } else {
                dump("Error deleting item: " + aStatusCode + "\n");
                // XXX real error handling here
                retVal = Components.results.NS_ERROR_FAILURE;
            }

            // notify the listener
            if (aListener)
                aListener.onOperationComplete (savedthis,
                                               Components.results.NS_OK,
                                               aListener.DELETE,
                                               aItem.id,
                                               null);
            // notify observers
            // XXX call only if successful
            savedthis.observeDeleteItem(aItem);
        }

        // XXX check etag/generation
        // do WebDAV remove
        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);
        webSvc.remove(eventResource, listener, null);

        return;
    },

    // void getItem( in string id, in calIOperationListener aListener );
    getItem: function (aId, aListener) {

        if (!aListener)
            return;

        if (aId == null) {
            // XXX FAILURE is a bad choice
            aListener.onOperationComplete(this,
                                          Components.results.NS_ERROR_FAILURE,
                                          aListener.GET,
                                          null,
                                          "passed in empty iid");
            return;
        }


        // this is our basic search-by-uid xml
        // XXX get rid of vevent filter?
        // XXX need a prefix in the namespace decl?
        default xml namespace = "urn:ietf:params:xml:ns:caldav";
        queryXml = 
          <calendar-query xmlns:C="urn:ietf:params:xml:ns:caldav">
            <calendar-query-result/>
              <filter>
                <icalcomp-filter name="VCALENDAR">
                  <icalcomp-filter name="VEVENT">
                    <icalprop-filter name="UID">
                      <text-match caseless="no">
                       {aId}
                      </text-match>
                    </icalprop-filter>
                  </icalcomp-filter>
                </icalcomp-filter>
              </filter>
          </calendar-query>;

        this.reportInternal(queryXml.toXMLString(), false, null, null, 1, aListener);
        return;
    },

    reportInternal: function (aQuery, aOccurrences, aRangeStart, aRangeEnd, aCount, aListener)
    {
        var reportListener = new WebDavListener();
        var count = 0;  // maximum number of hits to return

        reportListener.onOperationDetail = function(aStatusCode, aResource,
                                                    aOperation, aDetail,
                                                    aClosure) {
            var rv;
            var errString;

            // is this detail the entire search result, rather than a single
            // detail within a set?
            //
            if (aResource.path == calendarDirUri.path) {
                // XXX is this even valid?  what should we do here?
                throw("XXX report result for calendar, not event\n");
            }

            var items = null;

            // XXX need to do better than looking for just 200
            if (aStatusCode == 200) {

                // we've already called back the maximum number of hits, so 
                // we're done here.
                // 
                if (aCount && count >= aCount) {
                    return;
                }
                ++count;

                // aDetail is the response element from the multi-status
                // XXX try-catch
                var xSerializer = Components.classes
                    ['@mozilla.org/xmlextras/xmlserializer;1']
                    .getService(Components.interfaces.nsIDOMSerializer);
                //XXXdump(xSerializer.serializeToString(aDetail));
                responseElement = new XML(
                    xSerializer.serializeToString(aDetail));

                // create calIItemBase from e4x object
                // XXX error-check that we only have one result, etc
                var C = new Namespace("urn:ietf:params:xml:ns:caldav");
                var D = new Namespace("DAV:");

                // create a local event item
                // XXX not just events
                var item = calEventClass.createInstance(calIEvent);

                // cause returned data to be parsed into the event item
                // XXX try-catch
                dump ("ITEM RESULT: " + responseElement..C::["calendar-query-result"] + "\n");
                item.icalString = 
                    responseElement..C::["calendar-query-result"]; 
                item.parent = this;

                // save the location name in case we need to modify
                item.setProperty("locationURI", aResource.spec);

                // figure out what type of item to return
                var iid;
                if(aOccurrences) {
                    iid = calIItemOccurrence;
                    if (item.recurrenceInfo) {
                        //dump ("ITEM has recurrence: " + item + " (" + item.title + ")\n");
                        //dump ("rangestart: " + aRangeStart.jsDate + " -> " + aRangeEnd.jsDate + "\n");
                        items = item.recurrenceInfo.getOccurrences (aRangeStart, aRangeEnd, 0, {});
                    } else {
                        item = makeOccurrence(item, item.startDate, item.endDate);
                        items = [ item ];
                    }
                    rv = Components.results.NS_OK;
                } else if (item.QueryInterface(calIEvent)) {
                    iid = calIEvent;
                    rv = Components.results.NS_OK;
                    items = [ item ];
                } else if (item.QueryInterface(calITodo)) {
                    iid = calITodo;
                    rv = Components.results.NS_OK;
                    items = [ item ];
                } else {
                    errString = "Can't deduce item type based on query";
                    rv = Components.results.NS_ERROR_FAILURE;
                }

            } else { 
                // XXX
                errString = "XXX";
                rv = Components.results.NS_ERROR_FAILURE;
            }

            // XXX  handle aCount
            dump("errString = " + errString + "\n");
            if (items) {
                aListener.onGetResult(this, rv, iid, null, items ? items.length : 0,
                                      errString ? errString : items);
            }
            return;
        };

        reportListener.onOperationComplete = function(aStatusCode, aResource,
                                                      aOperation, aClosure) {
            
            // XXX test that something reasonable happens w/notfound

            // parse aStatusCode
            var rv;
            var errString;
            if (aStatusCode == 200) { // XXX better error checking
                rv = Components.results.NS_OK;
            } else {
                rv = Components.results.NS_ERROR_FAILURE;
                errString = "XXX something bad happened";
            }

            // call back the listener
            aListener.onOperationComplete(this,
                                          Components.results.NS_ERROR_FAILURE,
                                          aListener.GET, null, errString);
            return;
        };

        // convert this into a form the WebDAV service can use
        var xParser = Components.classes['@mozilla.org/xmlextras/domparser;1']
                      .getService(Components.interfaces.nsIDOMParser);
        queryDoc = xParser.parseFromString(aQuery, "application/xml");

        // construct the resource we want to search against
        // XXX adding "calendar/" to the uri is wrong
        var calendarDirUri = this.mUri.clone();
        var calendarDirResource = new WebDavResource(calendarDirUri);

        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);
        webSvc.report(calendarDirResource, queryDoc, true, reportListener,
                      null);
        return;    

    },


    // void getItems( in unsigned long aItemFilter, in unsigned long aCount, 
    //                in calIDateTime aRangeStart, in calIDateTime aRangeEnd,
    //                in calIOperationListener aListener );
    getItems: function (aItemFilter, aCount, aRangeStart, aRangeEnd, aListener)
    {
        if (!aListener)
            return;

        filterTypes = 0;
        if ( aItemFilter & calICalendar.ITEM_FILTER_TYPE_TODO ) {
            filterTypes++;
            dump("getItems called with TODO filter\n");
        }
        if ( aItemFilter & calICalendar.ITEM_FILTER_TYPE_EVENT ) {
            filterTypes++;
        }
        if ( aItemFilter & calICalendar.ITEM_FILTER_TYPE_JOURNAL ) {
            filterTypes++;
        }
        if ( filterTypes > 1 ) {
            dump("Multiple simultaneous filter types not supported by " + 
                 "this provider.\n");
            throw Components.results.NS_ERROR_FAILURE;
        }

        // this is our basic report xml
        // XXX get rid of vevent filter?
        var C = new Namespace("urn:ietf:params:xml:ns:caldav")
        default xml namespace = C;
        var queryXml = <calendar-query>
                         <calendar-query-result/>
                         <filter>
                           <icalcomp-filter name="VCALENDAR">
                             <icalcomp-filter/>
                           </icalcomp-filter>
                         </filter>
                       </calendar-query>;

        switch (aItemFilter & calICalendar.ITEM_FILTER_TYPE_ALL) {
        case calICalendar.ITEM_FILTER_TYPE_EVENT:
            queryXml[0].C::filter.C::["icalcomp-filter"]
                .C::["icalcomp-filter"].@name="VEVENT";
            break;

        case calICalendar.ITEM_FILTER_TYPE_TODO:
            queryXml[0].C::filter.C::["icalcomp-filter"]
                .C::["icalcomp-filter"].@name="VTODO";
            break;

        case calICalendar.ITEM_FILTER_TYPE_JOURNAL:
            break;
            queryXml[0].C::filter.C::["icalcomp-filter"]
                .C::["icalcomp-filter"].@name="VJOURNAL";

        default:
            dump("No item types specified\n");
            // XXX should we just quietly call back the completion method?
            throw NS_ERROR_FAILURE;

        }

        // if a time range has been specified, do the appropriate restriction.
        // XXX express "end of time" in caldav by leaving off "start", "end"
        if (aRangeStart && aRangeStart.isValid && 
            aRangeEnd && aRangeEnd.isValid) {

            var rangeXml = <time-range start={aRangeStart.icalString}
                                       end={aRangeEnd.icalString}/>;

            // append the time-range as a child of our innermost 
            // icalcomp-filter
            queryXml[0].C::filter.C::["icalcomp-filter"]
                .C::["icalcomp-filter"].appendChild(rangeXml);
        }

        // XXX aItemFilter

        var queryString = queryXml.toXMLString();
        dump("queryString = " + queryString + "\n");
        var occurrences = (aItemFilter &
                           calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) != 0; 
        this.reportInternal(queryString, occurrences, aRangeStart, aRangeEnd, aCount, aListener);
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
    },
};


function WebDavResource(url) {
    this.mResourceURL = url;
}

WebDavResource.prototype = {
    mResourceURL: {},
    get resourceURL() { 
        return this.mResourceURL;}  ,
    QueryInterface: function(iid) {
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

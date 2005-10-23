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

// XXXdmose deal with generation goop

// XXXdmose need to re-query for add & modify to get up-to-date items

// XXXdmose deal with etags

// XXXdmose deal with locking

// XXXdmose need to make and use better error reporting interface for webdav
// (all uses of aStatusCode, probably)

// XXXdmose use real calendar result codes, not NS_ERROR_FAILURE for everything

const xmlHeader = '<?xml version="1.0" encoding="UTF-8"?>\n';

function debug(s) {
    const debugging = true;
    if (debugging) {
        dump(s);
    }
}

function calDavCalendar() {
    this.wrappedJSObject = this;
    this.mObservers = Array();
}

// some shorthand
const nsIWebDAVOperationListener = 
    Components.interfaces.nsIWebDAVOperationListener;
const calICalendar = Components.interfaces.calICalendar;
const nsISupportsCString = Components.interfaces.nsISupportsCString;
const calIEvent = Components.interfaces.calIEvent;
const calIItemBase = Components.interfaces.calIItemBase;
const calITodo = Components.interfaces.calITodo;
const calEventClass = Components.classes["@mozilla.org/calendar/event;1"];

const kCalCalendarManagerContractID = "@mozilla.org/calendar/manager;1";
const kCalICalendarManager = Components.interfaces.calICalendarManager;


function makeOccurrence(item, start, end)
{
    var occ = item.createProxy();
    occ.recurrenceId = start;
    occ.startDate = start;
    occ.endDate = end;

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

    mReadOnly: false,

    get readOnly() { 
        return this.mReadOnly;
    },
    set readOnly(bool) {
        this.mReadOnly = bool;
    },

    // attribute nsIURI uri;
    mUri: null,
    get uri() { return this.mUri; },
    set uri(aUri) { this.mUri = aUri; },

    get mCalendarUri() { 
        calUri = this.mUri.clone();
        calUri.spec += "/";
        return calUri;
    },

    refresh: function() {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },

    // attribute boolean suppressAlarms;
    get suppressAlarms() { return false; },
    set suppressAlarms(aSuppressAlarms) { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; },

    // void addObserver( in calIObserver observer );
    addObserver: function (aObserver, aItemFilter) {
        for each (obs in aObserver) {
            if (obs == aObserver) {
                return;
            }
        }

        this.mObservers.push(aObserver);
    },

    // void removeObserver( in calIObserver observer );
    removeObserver: function (aObserver) {
        this.mObservers = this.mObservers.filter(
            function(o) {return (o != aObserver);});
    },

    // void addItem( in calIItemBase aItem, in calIOperationListener aListener );
    addItem: function (aItem, aListener) {
        if (this.readOnly) {
            throw Components.interfaces.calIErrors.CAL_IS_READONLY;
        }

        if (aItem.id == null && aItem.isMutable)
            // XXX real UUID here!!
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
        var itemUri = this.mCalendarUri.clone();
        itemUri.spec = itemUri.spec + aItem.id + ".ics";
        debug("itemUri.spec = " + itemUri.spec + "\n");
        var eventResource = new WebDavResource(itemUri);

        var listener = new WebDavListener();
        var thisCalendar = this;
        listener.onOperationComplete = 
        function onPutComplete(aStatusCode, aResource, aOperation, aClosure) {

            // 201 = HTTP "Created"
            //
            if (aStatusCode == 201) {
                debug("Item added successfully\n");

                var retVal = Components.results.NS_OK;

            } else if (aStatusCode == 200) {
                // XXXdmose once we get etag stuff working, this should
                // 
                debug("200 received from clients, until we have etags working"
                      + " this probably means a collision; after that it'll"
                      + " mean server malfunction/n");
                retVal = Components.results.NS_ERROR_FAILURE;
            } else {
                if (aStatusCode > 999) {
                    aStatusCode = "0x" + aStatusCode.toString(16);
                }

                // XXX real error handling
                debug("Error adding item: " + aStatusCode + "\n");
                retVal = Components.results.NS_ERROR_FAILURE;
            }

            // notify the listener
            if (aListener) {
                try {
                    aListener.onOperationComplete(thisCalendar,
                                                  retVal,
                                                  aListener.ADD,
                                                  newItem.id,
                                                  newItem);
                } catch (ex) {
                    debug("addItem's onOperationComplete threw an exception "
                          + ex + "; ignoring\n");
                }
            }

            // notify observers
            if (Components.isSuccessCode(retVal)) {
                thisCalendar.observeAddItem(newItem);
            }
        }
  
        var newItem = aItem.clone();
        newItem.calendar = this;
        newItem.generation = 1;
        newItem.setProperty("locationURI", itemUri.spec);
        newItem.makeImmutable();

        debug("icalString = " + newItem.icalString + "\n");

        // XXX use if not exists
        // do WebDAV put
        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);
        webSvc.putFromString(eventResource, "text/calendar", 
                             newItem.icalString, listener, null);

        return;
    },

    // void modifyItem( in calIItemBase aNewItem, in calIItemBase aOldItem, in calIOperationListener aListener );
    modifyItem: function modifyItem(aNewItem, aOldItem, aListener) {
        if (this.readOnly) {
            throw Components.interfaces.calIErrors.CAL_IS_READONLY;
        }

        if (aNewItem.id == null) {

            // XXXYYY fix to match iface spec
            // this is definitely an error
            if (aListener) {
                try {
                    aListener.onOperationComplete(this,
                                                  Components.results.NS_ERROR_FAILURE,
                                                  aListener.MODIFY,
                                                  aItem.id,
                                                  "ID for modifyItem doesn't exist or is null");
                } catch (ex) {
                    debug("modifyItem's onOperationComplete threw an"
                          + " exception " + ex + "; ignoring\n");
                }
            }

            return;
        }

        var eventUri = this.mCalendarUri.clone();
        try {
            eventUri.spec = aNewItem.getProperty("locationURI");
            debug("using locationURI: " + eventUri.spec + "\n");
        } catch (ex) {
            // XXX how are we REALLY supposed to figure this out?
            eventUri.spec = eventUri.spec + aNewItem.id + ".ics";
        }

        var eventResource = new WebDavResource(eventUri);

        var listener = new WebDavListener();
        var thisCalendar = this;
        listener.onOperationComplete = function(aStatusCode, aResource,
                                                aOperation, aClosure) {

            // 204 = HTTP "No Content"
            //
            if (aStatusCode == 204) {
                debug("Item modified successfully.\n");
                var retVal = Components.results.NS_OK;

            } else {
                if (aStatusCode > 999) {
                    aStatusCode = "0x " + aStatusCode.toString(16);
                }
                debug("Error modifying item: " + aStatusCode + "\n");

                // XXX deal with non-existent item here, other
                // real error handling

                // XXX aStatusCode will be 201 Created for a PUT on an item
                // that didn't exist before.

                retVal = Components.results.NS_ERROR_FAILURE;
            }

            // XXX ensure immutable version returned
            // notify listener
            if (aListener) {
                try {
                    aListener.onOperationComplete(thisCalendar, retVal,
                                                  aListener.MODIFY,
                                                  aNewItem.id, aNewItem);
                } catch (ex) {
                    debug("modifyItem's onOperationComplete threw an"
                          + " exception " + ex + "; ignoring\n");
                }
            }

            // notify observers
            if (Components.isSuccessCode(retVal)) {
                thisCalendar.observeModifyItem(aNewItem, aOldItem);
            }

            return;
        }

        // XXX use if-exists stuff here
        // XXX use etag as generation
        // do WebDAV put
        debug("modifyItem: aNewItem.icalString = " + aNewItem.icalString 
              + "\n");
        var webSvc = Components.classes['@mozilla.org/webdav/service;1']
            .getService(Components.interfaces.nsIWebDAVService);
        webSvc.putFromString(eventResource, "text/calendar",
                             aNewItem.icalString, listener, null);

        return;
    },


    // void deleteItem( in calIItemBase aItem, in calIOperationListener aListener );
    deleteItem: function (aItem, aListener) {
        if (this.readOnly) {
            throw Components.interfaces.calIErrors.CAL_IS_READONLY;
        }

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
        var eventUri = this.mCalendarUri.clone();
        eventUri.spec = eventUri.spec + aItem.id + ".ics";
        var eventResource = new WebDavResource(eventUri);

        var listener = new WebDavListener();
        var thisCalendar = this;

        listener.onOperationComplete = 
        function onOperationComplete(aStatusCode, aResource, aOperation,
                                     aClosure) {

            // 204 = HTTP "No content"
            //
            if (aStatusCode == 204) {
                debug("Item deleted successfully.\n");
                var retVal = Components.results.NS_OK;
            } else {
                debug("Error deleting item: " + aStatusCode + "\n");
                // XXX real error handling here
                retVal = Components.results.NS_ERROR_FAILURE;
            }

            // notify the listener
            if (aListener) {
                try {
                    aListener.onOperationComplete(thisCalendar,
                                                  Components.results.NS_OK,
                                                  aListener.DELETE,
                                                  aItem.id,
                                                  null);
                } catch (ex) {
                    debug("deleteItem's onOperationComplete threw an"
                          + " exception " + ex + "; ignoring\n");
                }
            }

            // notify observers
            if (Components.isSuccessCode(retVal)) {
                thisCalendar.observeDeleteItem(aItem);
            }
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
            <calendar-data/>
            <filter>
              <comp-filter name="VCALENDAR">
                <comp-filter name="VEVENT">
                  <prop-filter name="UID">
                    <text-match caseless="no">
                      {aId}
                    </text-match>
                  </prop-filter>
                </comp-filter>
              </comp-filter>
            </filter>
          </calendar-query>;

        this.reportInternal(xmlHeader + queryXml.toXMLString(), 
                            false, null, null, 1, aListener);
        return;
    },

    reportInternal: function (aQuery, aOccurrences, aRangeStart, aRangeEnd, aCount, aListener)
    {
        var reportListener = new WebDavListener();
        var count = 0;  // maximum number of hits to return
        var thisCalendar = this; // need to access from inside the callback

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
                // XXX if it's an error, it might be valid?
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
                var response = xSerializer.serializeToString(aDetail);
                var responseElement = new XML(response);

                // create calIItemBase from e4x object
                // XXX error-check that we only have one result, etc
                var C = new Namespace("urn:ietf:params:xml:ns:caldav");
                var D = new Namespace("DAV:");

                // create a local event item
                // XXX not just events
                var item = calEventClass.createInstance(calIEvent);

                // cause returned data to be parsed into the event item
                var calData = responseElement..C::["calendar-data"];
                if (calData.length == 0) {
                    debug("server returned empty or non-existing "
                          + "calendar-data element!\n");
                }
                debug("item result = \n" + calData + "\n");
                // XXX try-catch
                item.icalString = calData;
                item.calendar = thisCalendar;

                // save the location name in case we need to modify
                item.setProperty("locationURI", aResource.spec);
                debug("locationURI = " + aResource.spec + "\n");
                item.makeImmutable();

                // figure out what type of item to return
                var iid;
                if(aOccurrences) {
                    iid = calIItemBase;
                    if (item.recurrenceInfo) {
                        debug("ITEM has recurrence: " + item + " (" + item.title + ")\n");
                        debug("rangestart: " + aRangeStart.jsDate + " -> " + aRangeEnd.jsDate + "\n");
                        // XXX does getOcc call makeImmutable?
                        items = item.recurrenceInfo.getOccurrences(aRangeStart,
                                                                   aRangeEnd,
                                                                   0, {});
                    } else {
                        // XXX need to make occurrences immutable?
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
                debug("aStatusCode = " + aStatusCode + "\n");
                errString = "XXX";
                rv = Components.results.NS_ERROR_FAILURE;
            }

            // XXX  handle aCount
            if (errString) {
                debug("errString = " + errString + "\n");
            }

            try {
                aListener.onGetResult(thisCalendar, rv, iid, null,
                                      items ? items.length : 0,
                                      errString ? errString : items);
            } catch (ex) {
                    debug("reportInternal's onGetResult threw an"
                          + " exception " + ex + "; ignoring\n");
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
            try {
                if (aListener) {
                    aListener.onOperationComplete(thisCalendar,
                                                  Components.results.
                                                  NS_ERROR_FAILURE,
                                                  aListener.GET, null,
                                                  errString);
                }
            } catch (ex) {
                    debug("reportInternal's onOperationComplete threw an"
                          + " exception " + ex + "; ignoring\n");
            }

            return;
        };

        // convert this into a form the WebDAV service can use
        var xParser = Components.classes['@mozilla.org/xmlextras/domparser;1']
                      .getService(Components.interfaces.nsIDOMParser);
        queryDoc = xParser.parseFromString(aQuery, "application/xml");

        // construct the resource we want to search against
        var calendarDirUri = this.mCalendarUri.clone();
        debug("report uri = " + calendarDirUri.spec + "\n");
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

        // this is our basic report xml
        var C = new Namespace("urn:ietf:params:xml:ns:caldav")
        default xml namespace = C;
        var queryXml = 
          <calendar-query>
            <calendar-data/>
            <filter>
              <comp-filter name="VCALENDAR">
                <comp-filter/>
              </comp-filter>
            </filter>
          </calendar-query>;

        // figure out 
        var compFilterNames = new Array();
        compFilterNames[calICalendar.ITEM_FILTER_TYPE_TODO] = "VTODO";
        compFilterNames[calICalendar.ITEM_FILTER_TYPE_VJOURNAL] = "VJOURNAL";
        compFilterNames[calICalendar.ITEM_FILTER_TYPE_EVENT] = "VEVENT";

        var filterTypes = 0;
        for (var i in compFilterNames) {
            if (aItemFilter & i) {
                // XXX CalDAV only allows you to ask for one filter-type
                // at once, so if the caller wants multiple filtern
                // types, all they're going to get for now is events
                // (since that's last in the Array).  Sorry!
                ++filterTypes;
                queryXml[0].C::filter.C::["comp-filter"]
                    .C::["comp-filter"] = 
                    <comp-filter name={compFilterNames[i]}/>;
            }
        }

        if (filterTypes < 1) {
            debug("No item types specified\n");
            // XXX should we just quietly call back the completion method?
            throw NS_ERROR_FAILURE;
        }

        // if a time range has been specified, do the appropriate restriction.
        // XXX express "end of time" in caldav by leaving off "start", "end"
        if (aRangeStart && aRangeStart.isValid && 
            aRangeEnd && aRangeEnd.isValid) {

            var rangeXml = <time-range start={aRangeStart.icalString}
                                       end={aRangeEnd.icalString}/>;

            // append the time-range as a child of our innermost comp-filter
            queryXml[0].C::filter.C::["comp-filter"]
                .C::["comp-filter"].appendChild(rangeXml);
        }

        var queryString = xmlHeader + queryXml.toXMLString();
        debug("getItems(): querying CalDAV server for events: " + 
              queryString + "\n");

        var occurrences = (aItemFilter &
                           calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) != 0; 
        this.reportInternal(queryString, occurrences, aRangeStart, aRangeEnd,
                            aCount, aListener);
    },

    //
    // Helper functions
    //
    observeBatchChange: function observeBatchChange (aNewBatchMode) {
        for each (obs in this.mObservers) {
            if (aNewBatchMode) {
                try {
                    obs.onStartBatch();
                } catch (ex) {
                    debug("observer's onStartBatch threw an exception " + ex 
                          + "; ignoring\n");
                }
            } else {
                try {
                    obs.onEndBatch();
                } catch (ex) {
                    debug("observer's onEndBatch threw an exception " + ex 
                          + "; ignoring\n");
                }
            }
        }
        return;
    },

    observeAddItem: 
        function observeAddItem(aItem) {
            for each (obs in this.mObservers) {
                try {
                    obs.onAddItem(aItem);
                } catch (ex) {
                    debug("observer's onAddItem threw an exception " + ex 
                          + "; ignoring\n");
                }
            }
            return;
        },

    observeModifyItem: 
        function observeModifyItem(aNewItem, aOldItem) {
            for each (obs in this.mObservers) {
                try {
                    obs.onModifyItem(aNewItem, aOldItem);
                } catch (ex) {
                    debug("observer's onModifyItem threw an exception " + ex 
                          + "; ignoring\n");
                }
            }
            return;
        },

    observeDeleteItem: 
        function observeDeleteItem(aDeletedItem) {
            for each (obs in this.mObservers) {
                try {
                    obs.onDeleteItem(aDeletedItem);
                } catch (ex) {
                    debug("observer's onDeleteItem threw an exception " + ex 
                          + "; ignoring\n");
                }
            }
            return;
        }
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

        debug("WebDavListener.onOperationComplete() called\n");
        return;
    },

    onOperationDetail: function(aStatusCode, aResource, aOperation, aDetail,
                                aClosure) {
        debug("WebDavListener.onOperationDetail() called\n");
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

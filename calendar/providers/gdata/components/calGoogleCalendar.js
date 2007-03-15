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
 * The Original Code is Google Calendar Provider code.
 *
 * The Initial Developer of the Original Code is
 *   Philipp Kewisch (mozilla@kewis.ch)
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joey Minta <jminta@gmail.com>
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

/**
 * calGoogleCalendar
 * This Implements a calICalendar Object adapted to the Google Calendar
 * Provider.
 *
 * @class
 * @constructor
 */
function calGoogleCalendar() {

    this.mObservers = new Array();

    var calObject = this;

    function calAttrHelper(aAttr) {
        this.getAttr = function calAttrHelper_get() {
            // Note that you need to declare this in here, to avoid cyclic
            // getService calls.
            var calMgr = Cc["@mozilla.org/calendar/manager;1"].
                         getService(Ci.calICalendarManager);
            return calMgr.getCalendarPref(calObject, aAttr);
        };
        this.setAttr = function calAttrHelper_set(aValue) {
            var calMgr = Cc["@mozilla.org/calendar/manager;1"].
                         getService(Ci.calICalendarManager);
            calMgr.setCalendarPref(calObject, aAttr, aValue);
            return aValue;
        };
    }

    var prefAttrs = ["name", "supressAlarms"];
    for each (var attr in prefAttrs) {
        var helper = new calAttrHelper(attr);
        this.__defineGetter__(attr, helper.getAttr);
        this.__defineSetter__(attr, helper.setAttr);
    }
}

calGoogleCalendar.prototype = {

    QueryInterface: function cGC_QueryInterface(aIID) {
        if (!aIID.equals(Ci.nsISupports) &&
            !aIID.equals(Ci.calICalendar)) {
            throw Cr.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    /* Member Variables */
    mSession: null,
    mObservers: null,
    mReadOnly: false,
    mUri: null,
    mFullUri: null,
    mCalendarName: null,

    /*
     * Google Calendar Provider attributes
     */

    /**
     * readonly attribute googleCalendarName
     * Google's Calendar name. This represents the <calendar name> in
     * http://www.google.com/calendar/feeds/<calendar name>/private/full
     */
    get googleCalendarName() {
        return this.mCalendarName;
    },

    /**
     * attribute session
     * An calGoogleSession Object that handles the session requests.
     */
    get session() {
        return this.mSession;
    },
    set session(v) {
        return this.mSession = v;
    },

    /**
     * notifyObservers
     * Notify this calendar's observers that a specific event has happened
     *
     * @param aEvent       The Event that happened.
     * @param aArgs        An array of arguments that is passed to the observer
     */
    notifyObservers: function cGC_notifyObservers(aEvent, aArgs) {
        for each (var obs in this.mObservers) {
            try {
                obs[aEvent].apply(obs, aArgs);
            } catch (e) {
                Components.utils.reportError(e);
            }
        }
    },

    /**
     * findSession
     * Populates the Session Object based on the preferences or the result of a
     * login prompt.
     *
     * @param aIgnoreExistingSession If set, find the session regardless of
     *                               whether the session has been previously set
     *                               or not
     */
    findSession: function cGC_findSession(aIgnoreExistingSession) {
        if (this.mSession && !aIgnoreExistingSession) {
            return;
        }

        // We need to find out which Google account fits to this calendar.
        var googleUser = getCalendarPref(this, "googleUser");
        if (googleUser) {
            this.mSession = getSessionByUsername(googleUser);
        } else {
            // We have no user, therefore we need to ask the user. Show a
            // user/password prompt and set the session based on those
            // values.

            var username = { value: this.mCalendarName };
            var password = { value: null };
            var savePassword = { value: false };

            if (getCalendarCredentials(this.mCalendarName,
                                       username,
                                       password,
                                       savePassword)) {
                this.mSession = getSessionByUsername(username.value);
                this.mSession.googlePassword = password.value;
                this.mSession.savePassword = savePassword.value;
                setCalendarPref(this,
                                "googleUser",
                                "CHAR",
                                this.mSession.googleUser);
            }
        }
    },

    /*
     * implement calICalendar
     */
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

    get readOnly() {
        return this.mReadOnly;
    },
    set readOnly(v) {
        return this.mReadOnly = v;
    },

    get type() {
        return "gdata";
    },

    get uri() {
        return this.mUri;
    },

    get fullUri() {
        return this.mFullUri;
    },
    set uri(aUri) {
        // Parse google url, catch private cookies, public calendars,
        // basic and full types, bogus ics file extensions, invalid hostnames
        var re = new RegExp("/calendar/(feeds|ical)/" +
                            "([^/]+)/(public|private)-?([^/]+)?/" +
                            "(full|basic)(.ics)?$");

        var matches = aUri.path.match(re);

        if (!matches) {
            throw new Components.Exception(aUri, Cr.NS_ERROR_MALFORMED_URI);
        }

        // Set internal Calendar Name
        this.mCalendarName = decodeURIComponent(matches[2]);

        var ioService = Cc["@mozilla.org/network/io-service;1"].
                        getService(Ci.nsIIOService);

        // Set normalized url. We need the full xml stream and private
        // access. We need private visibility and full projection
        this.mFullUri = ioService.newURI("http://www.google.com" +
                                         "/calendar/feeds/" +
                                         matches[2] +
                                         "/private/full",
                                         null,
                                         null);

        // Remember the uri as it was passed, in case the calendar manager
        // relies on it.
        this.mUri = aUri;

        this.findSession(true);
        return this.mUri;
    },

    get canRefresh() {
        return false;
    },

    addObserver: function cGC_addObserver(aObserver) {
        if (this.mObservers.indexOf(aObserver) == -1) {
            this.mObservers.push(aObserver);
        }
    },

    removeObserver: function cGC_removeObserver(aObserver) {
        function cGC_removeObserver_remove(obj) {
            return ( obj != aObserver );
        }
        this.mObservers = this.mObservers.filter(cGC_removeObserver_remove);
    },

    adoptItem: function cGC_adoptItem(aItem, aListener) {
        LOG("Adding item " + aItem.title);

        try {
            // Check if calendar is readonly
            if (this.mReadOnly) {
                throw new Components.Exception("",
                                               Ci.calIErrors.CAL_IS_READONLY);
            }

            // Make sure the item is an event
            aItem = aItem.QueryInterface(Ci.calIEvent);

            // Check if we have a session. If not, then the user has canceled
            // the login prompt.
            if (!this.mSession) {
                this.findSession();
            }

            this.mSession.addItem(this,
                                  aItem,
                                  this.addItem_response,
                                  aListener);
        } catch (e) {
            LOG("adoptItem failed before request " + aItem.title + "\n:" + e);
            if (e.result == Ci.calIErrors.CAL_IS_READONLY) {
                // The calendar is readonly, make sure this is set and
                // notify the user. This can come from above or from
                // mSession.addItem which checks for the editURI
                this.mReadOnly = true;
                this.notifyObservers("onError", [e.result, e.message]);
            }

            if (aListener != null) {
                aListener.onOperationComplete(this,
                                              e.result,
                                              Ci.calIOperationListener.ADD,
                                              null,
                                              e.message);
            }
        }
    },

    addItem: function cGC_addItem(aItem, aListener) {
        // Google assigns an ID to every event added. Any id set here or in
        // adoptItem will be overridden.
        return this.adoptItem( aItem.clone(), aListener );
    },

    modifyItem: function cGC_modifyItem(aNewItem, aOldItem, aListener) {
        LOG("Modifying item " + aOldItem.title);

        try {
            if (this.mReadOnly) {
                throw new Components.Exception("",
                                               Ci.calIErrors.CAL_IS_READONLY);
            }

            // Check if we have a session. If not, then the user has canceled
            // the login prompt.
            if (!this.mSession) {
                this.findSession();
            }

            // Check if the item is an occurrence. Until bug 362650 is solved,
            // there is no support for changing single ocurrences.
            if (aOldItem.getProperty("X-GOOGLE-ITEM-IS-OCCURRENCE")) {
                throw new Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
            }

            // Check if enough fields have changed to warrant sending the event
            // to google. This saves network traffic.
            if (relevantFieldsMatch(aOldItem, aNewItem)) {
                LOG("Not requesting item modification for " + aOldItem.id +
                "(" + aOldItem.title + "), relevant fields match");

                if (aListener != null) {
                    aListener.onOperationComplete(this,
                                                  Cr.NS_OK,
                                                  Ci.calIOperationListener.MODIFY,
                                                  aNewItem.id,
                                                  aNewItem);
                }
                this.notifyObservers("onModifyItem", [aNewItem, aOldItem]);
                return;
            }

            // We need the old item in the response so the observer can be
            // called correctly.
            var extradata = { olditem: aOldItem, listener: aListener };

            this.mSession.modifyItem(this,
                                     aNewItem,
                                     this.modifyItem_response,
                                     extradata);
        } catch (e) {
            LOG("modifyItem failed before request " +
                aNewItem.title + "(" + aNewItem.id + "):\n" + e);

            if (e.result == Ci.calIErrors.CAL_IS_READONLY) {
                // The calendar is readonly, make sure this is set and
                // notify the user. This can come from above or from
                // mSession.modifyItem which checks for the editURI
                this.mReadOnly = true;
                this.notifyObservers("onError", [e.result, e.message]);
            }

            if (aListener != null) {
                aListener.onOperationComplete(this,
                                              e.result,
                                              Ci.calIOperationListener.MODIFY,
                                              null,
                                              e.message);
            }
        }
    },

    deleteItem: function cGC_deleteItem(aItem, aListener) {
        LOG("Deleting item " + aItem.title + "(" + aItem.id + ")");

        try {
            if (this.mReadOnly) {
                throw new Components.Exception("",
                                               Ci.calIErrors.CAL_IS_READONLY);
            }

            // Check if we have a session. If not, then the user has canceled
            // the login prompt.
            if (!this.mSession) {
                this.findSession();
            }

            // We need the item in the response, since google dosen't return any
            // item XML data on delete, and we need to call the observers.
            var extradata = { listener: aListener, item: aItem };

            this.mSession.deleteItem(this,
                                     aItem,
                                     this.deleteItem_response,
                                     extradata);
        } catch (e) {
            LOG("deleteItem failed before request for " +
                aItem.title + "(" + aItem.id + "):\n" + e);

            if (e.result == Ci.calIErrors.CAL_IS_READONLY) {
                // The calendar is readonly, make sure this is set and
                // notify the user. This can come from above or from
                // mSession.deleteItem which checks for the editURI
                this.mReadOnly = true;
                this.notifyObservers("onError", [e.result, e.message]);
            }

            if (aListener != null) {
                aListener.onOperationComplete(this,
                                              e.result,
                                              Ci.calIOperationListener.DELETE,
                                              null,
                                              e.message);
            }
        }
    },

    getItem: function cGC_getItem(aId, aListener) {
        // This function needs a test case using mechanisms in bug 365212
        LOG("Getting item with id " + aId);
        try {

            // Check if we have a session. If not, then the user has canceled
            // the login prompt.
            if (!this.mSession) {
                this.findSession();
            }

            this.mSession.getItem(this, aId, this.getItem_response, aListener);
        } catch (e) {
            LOG("getItem failed before request " + aItem.title + "(" + aItem.id
                + "):\n" + e);

            if (aListener != null) {
                aListener.onOperationComplete(this,
                                              e.result,
                                              Ci.calIOperationListener.GET,
                                              null,
                                              e.message);
            }
        }
    },

    getItems: function cGC_getItems(aItemFilter,
                                    aCount,
                                    aRangeStart,
                                    aRangeEnd,
                                    aListener) {
        try {
            // Check if we have a session. If not, then the user has canceled
            // the login prompt.
            if (!this.mSession) {
                this.findSession();
            }

            // item base type
            var wantEvents = ((aItemFilter &
                               Ci.calICalendar.ITEM_FILTER_TYPE_EVENT) != 0);
            var wantTodos = ((aItemFilter &
                              Ci.calICalendar.ITEM_FILTER_TYPE_TODO) != 0);

            // check if events are wanted
            if (!wantEvents && !wantTodos) {
                // Nothing to do. The onOperationComplete in the catch block
                // below will catch this.
                throw new Components.Exception("", Cr.NS_OK);
            } else if (wantTodos && !wantEvents) {
                throw new Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
            }
            // return occurrences?
            var itemReturnOccurrences = ((aItemFilter &
                Ci.calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) != 0);

            var extradata = { itemfilter: aItemFilter, listener: aListener };

            this.mSession.getItems(this,
                                   aCount,
                                   aRangeStart,
                                   aRangeEnd,
                                   itemReturnOccurrences,
                                   this.getItems_response,
                                   extradata);
        } catch (e) {
            if (aListener != null) {
                aListener.onOperationComplete(this,
                                              e.result,
                                              Ci.calIOperationListener.GET,
                                              null, e.message);
            }
        }
    },

    refresh: function cGC_refresh() { },

    startBatch: function cGC_startBatch() {
        this.notifyObservers("onStartBatch", []);
    },

    endBatch: function cGC_endBatch() {
        this.notifyObservers("onEndBatch", []);
    },

    /*
     * Google Calendar Provider Response functions
     */

    /**
     * addItem_response
     * Response callback, called by the session object when an item was added
     *
     * @param aRequest  The request object that initiated the request
     * @param aStatus   The response code. This is a Cr.* code
     * @param aResult   In case of an error, this is the error string, otherwise
     *                  an XML representation of the added item.
     */
    addItem_response: function cGC_addItem_response(aRequest,
                                                    aStatus,
                                                    aResult) {
        var item = this.general_response(Ci.calIOperationListener.ADD,
                                         aResult,
                                         aStatus,
                                         aRequest.extraData);
        // Notify Observers
        if (item) {
            this.notifyObservers("onAddItem", [item]);
        }
    },

    /**
     * modifyItem_response
     * Response callback, called by the session object when an item was modified
     *
     * @param aRequest  The request object that initiated the request
     * @param aStatus   The response code. This is a Cr.* Code
     * @param aResult   In case of an error, this is the error string, otherwise
     *                  an XML representation of the modified item
     */
    modifyItem_response: function cGC_modifyItem_response(aRequest,
                                                          aStatus,
                                                          aResult) {
        var item = this.general_response(Ci.calIOperationListener.MODIFY,
                                         aResult,
                                         aStatus,
                                         aRequest.extraData.listener,
                                         aRequest);
        // Notify Observers
        if (item) {
            this.notifyObservers("onModifyItem",
                                 [item, aRequest.extraData.olditem]);
        }
    },

    /**
     * deleteItem_response
     * Response callback, called by the session object when an Item was deleted
     *
     * @param aRequest  The request object that initiated the request
     * @param aStatus   The response code. This is a Cr.* Code
     * @param aResult   In case of an error, this is the error string, otherwise
     *                  this may be empty.
     */
    deleteItem_response: function cGC_deleteItem_response(aRequest,
                                                          aStatus,
                                                          aResult) {
        // The reason we are not using general_response here is because deleted
        // items are not returned as xml from google. We need to pass the item
        // we saved with the request.

        try {
            // Check if the call succeeded
            if (aStatus != Cr.NS_OK) {
                throw new Components.Exception(aResult, aStatus);
            }

            // All operations need to call onOperationComplete
            if (aRequest.extraData.listener) {
                LOG("Deleting item " + aRequest.extraData.item.id +
                    " successful");

                aRequest.extraData.listener.onOperationComplete(this,
                                                                Cr.NS_OK,
                                                                Ci.calIOperationListener.DELETE,
                                                                aRequest.extraData.item.id,
                                                                aRequest.extraData.item);
            }

            // Notify Observers
            this.notifyObservers("onDeleteItem", [aRequest.extraData.item]);
        } catch (e) {
            LOG("Deleting item " + aRequest.extraData.item.id + " failed");
            // Operation failed
            if (aRequest.extraData.listener) {
                aRequest.extraData.listener.onOperationComplete(this,
                                                                e.result,
                                                                Ci.calIOperationListener.DELETE,
                                                                null,
                                                                e.message);
            }
        }
    },

    /**
     * getItem_response
     * Response callback, called by the session object when a single Item was
     * downloaded.
     *
     * @param aRequest  The request object that initiated the request
     * @param aStatus   The response code. This is a Cr.* Code
     * @param aResult   In case of an error, this is the error string, otherwise
     *                  an XML representation of the requested item
     */
    getItem_response: function cGC_getItem_response(aRequest,
                                                    aStatus,
                                                    aResult) {
        // our general response does it all for us. I hope.
        this.general_response(Ci.calIOperationListener.GET,
                              aResult,
                              aStatus,
                              aRequest.extraData.listener);
    },

    /**
     * getItems_response
     * Response callback, called by the session object when an Item feed was
     * downloaded.
     *
     * @param aRequest  The request object that initiated the request
     * @param aStatus   The response code. This is a Cr.* Code
     * @param aResult   In case of an error, this is the error string, otherwise
     *                  an XML feed with the requested items.
     */
    getItems_response: function cGC_getItems_response(aRequest,
                                                      aStatus,
                                                      aResult) {
        LOG("Recieved response for " + aRequest.uri);
        try {
            // Check if the call succeeded
            if (aStatus != Cr.NS_OK) {
                throw new Components.Exception(aResult, aStatus);
            }

            // Prepare Namespaces
            var gCal = new Namespace("gCal",
                                     "http://schemas.google.com/gCal/2005");
            var gd = new Namespace("gd", "http://schemas.google.com/g/2005");
            var atom = new Namespace("", "http://www.w3.org/2005/Atom");
            default xml namespace = atom;

            // A feed was passed back, parse it. Due to bug 336551 we need to
            // filter out the <?xml...?> part.
            var xml = new XML(aResult.substring(38));
            var timezone = xml.gCal::timezone.@value.toString() || "UTC";

            // This line is needed, otherwise the for each () block will never
            // be entered. It may seem strange, but if you don't believe me, try
            // it!
            xml.link.(@rel);

            // We might be able to get the full name through this feed's author
            // tags. We need to make sure we have a session for that.
            if (!this.mSession) {
                this.findSession();
            }

            if (xml.author.email == this.mSession.googleUser) {
                this.mSession.googleFullName = xml.author.name.toString();
            }

            // Parse all <entry> tags
            for each (var entry in xml.entry) {
                var item = XMLEntryToItem(entry, timezone);

                if (item) {
                    var itemReturnOccurrences =
                        ((aRequest.extraData.itemfilter &
                          Ci.calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) != 0);

                    var itemIsOccurrence = item.getProperty("X-GOOGLE-ITEM-IS-OCCURRENCE");

                    if ((itemReturnOccurrences && itemIsOccurrence) ||
                        !itemIsOccurrence) {
                        LOG("Parsing entry:\n" + entry + "\n");

                        item.calendar = this;
                        item.makeImmutable();
                        LOGitem(item);

                        aRequest.extraData.listener.onGetResult(this,
                                                                Cr.NS_OK,
                                                                Ci.calIEvent,
                                                                null,
                                                                1,
                                                                [item]);
                    }
                } else {
                    LOG("Notice: An Item was skipped. Probably it has" +
                        " features that are not supported, or it was" +
                        " canceled");
                }
            }

            // Operation Completed successfully.
            if (aRequest.extraData.listener != null) {
                aRequest.extraData.listener.onOperationComplete(this,
                                                                Cr.NS_OK,
                                                                Ci.calIOperationListener.GET,
                                                                null,
                                                                null);
            }
        } catch (e) {
            LOG("Error getting items:\n" + e);
            // Operation failed
            if (aRequest.extraData.listener != null) {
                aRequest.extraData.listener.onOperationComplete(this,
                                                                e.result,
                                                                Ci.calIOperationListener.GET,
                                                                null,
                                                                e.message);
            }
        }
    },

    /**
     * general_response
     * Handles common actions for multiple response types. This does not notify
     * observers.
     *
     * @param aOperation    The operation type (Ci.calIOperationListener.*)
     * @param aItemString   The string represenation of the item
     * @param aStatus       The response code. This is a Cr.*
     *                      error code
     * @param aListener     The listener to be called on completion
     *                      (an instance of calIOperationListener)
     *
     * @return              The Item as a calIEvent
     */
    general_response: function cGC_general_response(aOperation,
                                                    aItemString,
                                                    aStatus,
                                                    aListener) {

        try {
            // Check if the call succeeded, if not then aItemString is an error
            // message

            if (aStatus != Cr.NS_OK) {
                throw new Components.Exception(aItemString, aStatus);
            }

            // An Item was passed back, parse it. Due to bug 336551 we need to
            // filter out the <?xml...?> part.
            var xml = new XML(aItemString.substring(38));

            // Get the local timezone from the preferences
            var timezone = getPrefSafe("calendar.timezone.local");

            // Parse the Item with the given timezone
            var item = XMLEntryToItem(xml, timezone);

            LOGitem(item);
            item.calendar = this;
            item.makeImmutable();

            // GET operations need to call onGetResult
            if (aOperation == Ci.calIOperationListener.GET) {
                aListener.onGetResult(this,
                                      Cr.NS_OK,
                                      Ci.calIEvent,
                                      null,
                                      1,
                                      [item]);
            }

            // All operations need to call onOperationComplete
            if (aListener) {
                aListener.onOperationComplete(this,
                                              Cr.NS_OK,
                                              aOperation,
                                              (item ? item.id : null),
                                              item);
            }
            return item;
        } catch (e) {
            LOG("General response failed: " + e);

            if (e.result == Ci.calIErrors.CAL_IS_READONLY) {
                // The calendar is readonly, make sure this is set and
                // notify the user.
                this.mReadOnly = true;
                this.notifyObservers("onError", [e.result, e.message]);
            }

            // Operation failed
            aListener.onOperationComplete(this,
                                          e.result,
                                          aOperation,
                                          null,
                                          e.message);
        }
        // Returning null to avoid js strict warning.
        return null;
    }
};

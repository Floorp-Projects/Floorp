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
 * The Original Code is Lightning code.
 *
 * The Initial Developer of the Original Code is Simdesk Technologies Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Clint Talbert <ctalbert.moz@gmail.com>
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

/**
 * Constructor of calItipItem object
 */
function calItipItem() {
    this.wrappedJSObject = this;
    this.mIsInitialized = false;
    this.mCurrentItemIndex = 0;
}

calItipItem.prototype = {
    getInterfaces: function ciiGI(count) {
        var ifaces = [
            Ci.nsIClassInfo,
            Ci.nsISupports,
            Ci.calIItipItem
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function ciiGHFL(aLanguage) {
        return null;
    },

    contractID: "@mozilla.org/calendar/itip-item;1",
    classDescription: "Calendar iTIP item",
    classID: Components.ID("{f41392ab-dcad-4bad-818f-b3d1631c4d93}"),
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0,

    QueryInterface: function ciiQI(aIid) {
        if (!aIid.equals(Ci.nsISupports) && !aIid.equals(Ci.calIItipItem)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    mIsSend: false,
    get isSend() {
        return this.mIsSend;
    },
    set isSend(aValue) {
        return (this.mIsSend = aValue);
    },

    mReceivedMethod: null,
    get receivedMethod() {
        return this.mReceivedMethod;
    },
    set receivedMethod(aMethod) {
        return (this.mReceivedMethod = aMethod.toUpperCase());
    },

    mResponseMethod: null,
    get responseMethod() {
        if (this.mIsInitialized) {
            var method = null;
            for each (var prop in this.mPropertiesList) {
                if (prop.propertyName == "METHOD") {
                    method = prop.value;
                    break;
                }
            }
            return method;
        } else {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }
    },
    set responseMethod(aMethod) {
        this.mResponseMethod = aMethod.toUpperCase();
        // Setting this also sets the global method attribute inside the
        // encapsulated VCALENDAR.
        if (this.mIsInitialized) {
            var methodExists = false;
            for each (var prop in this.mPropertiesList) {
                if (prop.propertyName == "METHOD") {
                    methodExists = true;
                    prop.value = this.mResponseMethod;
                }
            }

            if (!methodExists) {
                var newProp = { propertyName: "METHOD",
                                value: this.mResponseMethod };
                this.mPropertiesList.push(newProp);
            }
        } else {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }
        return this.mResponseMethod;
    },

    mAutoResponse: null,
    get autoResponse() {
        return this.mAutoResponse;
    },
    set autoResponse(aValue) {
        return (this.mAutoResponse = aValue);
    },

    mTargetCalendar: null,
    get targetCalendar() {
        return this.mTargetCalendar;
    },
    set targetCalendar(aValue) {
        return (this.mTargetCalendar = aValue);
    },

    mLocalStatus: null,
    get localStatus() {
        return this.mLocalStatus;
    },
    set localStatus(aValue) {
        return (this.mLocalStatus = aValue);
     },

    modifyItem: function ciiMI(item) {
        // Bug 348666: This will be used when we support delegation and COUNTER.
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },

    mItemList: null,
    mPropertiesList: null,

    init: function ciiI(aIcalString) {
        var parser = Cc["@mozilla.org/calendar/ics-parser;1"].
                     createInstance(Ci.calIIcsParser);
        parser.parseString(aIcalString);
        this.mItemList = parser.getItems({});
        this.mPropertiesList = parser.getProperties({});

        // We set both methods now for safety's sake. It's the ItipProcessor's
        // responsibility to properly ascertain what the correct response
        // method is (using user feedback, prefs, etc.) for the given
        // receivedMethod.  The RFC tells us to treat items without a METHOD
        // as if they were METHOD:REQUEST.
        var method;
        for each (var prop in this.mPropertiesList) {
            if (prop.propertyName == "METHOD") {
                method = prop.value;
            }
        }
        this.mReceivedMethod = method;
        this.mResponseMethod = method;

        this.mIsInitialized = true;
    },

    clone: function ciiC() {
        // Iterate through all the calItems in the original calItipItem to
        // concatenate all the calItems' icalStrings.
        var icalString = "";

        var itemList = this.getItemList({ });
        for (var i = 0; i < itemList.length; i++) {
            icalString += itemList[i].icalString;
        }

        // Create a new calItipItem and initialize it using the icalString
        // from above.
        var newItem = Cc["@mozilla.org/calendar/itip-item;1"].
                      createInstance(Ci.calIItipItem);
        newItem.init(icalString);

        // Copy over the exposed attributes.
        newItem.receivedMethod = this.receivedMethod;
        newItem.responseMethod = this.responseMethod;
        newItem.autoResponse = this.autoResponse;
        newItem.targetCalendar = this.targetCalendar;
        newItem.localStatus = this.localStatus;
        newItem.isSend = this.isSend;

        return newItem;
    },

    /**
     * This returns both the array and the number of items. An easy way to
     * call it is: var itemArray = itipItem.getItemList({ });
     */
    getItemList: function ciiGIL(itemCountRef) {
        if (!this.mIsInitialized || (this.mItemList.length == 0)) {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }
        itemCountRef.value = this.mItemList.length;
        return this.mItemList;
    },

    /*getFirstItem: function ciiGFI() {
        if (!this.mIsInitialized || (this.mItemList.length == 0)) {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }
        this.mCurrentItemIndex = 0;
        return this.mItemList[0];
    },

    getNextItem: function ciiGNI() {
        if (!this.mIsInitialized || (this.mItemList.length == 0)) {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }
        ++this.mCurrentItemIndex;
        if (this.mCurrentItemIndex < this.mItemList.length) {
            return this.mItemList[this.mCurrentItemIndex];
        } else {
            return null;
        }
    },*/

    /**
     * Note that this code forces the user to respond to all items in the same
     * way, which is a current limitation of the spec.
     */
    setAttendeeStatus: function ciiSAS(aAttendeeId, aStatus) {
        // Append "mailto:" to the attendee if it is missing it.
        var attId = aAttendeeId.toLowerCase();
        if (!attId.match(/mailto:/i)) {
            attId = "mailto:" + attId;
        }

        for each (var item in this.mItemList) {
            var attendee = item.getAttendeeById(attId);
            if (attendee) {
                // XXX BUG 351589: workaround for updating an attendee
                item.removeAttendee(attendee);

                // Replies should not have the RSVP property.
                // Since attendee.deleteProperty("RSVP") doesn't work, we must
                // create a new attendee from scratch WITHOUT the RSVP
                // property and copy in the other relevant data.
                // XXX use deleteProperty after bug 358498 is fixed.
                newAttendee = Cc["@mozilla.org/calendar/attendee;1"].
                              createInstance(Ci.calIAttendee);
                if (attendee.commonName) {
                    newAttendee.commonName = attendee.commonName;
                }
                if (attendee.role) {
                    newAttendee.role = attendee.role;
                }
                if (attendee.userType) {
                    newAttendee.userType = attendee.userType;
                }
                newAttendee.id = attendee.id;
                newAttendee.participationStatus = aStatus;
                item.addAttendee(newAttendee);
            }
        }
    }
};

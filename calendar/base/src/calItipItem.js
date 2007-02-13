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
 *   Clint Talbert <cmtalbert@myfastmail.com>
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

var calItipItemClassInfo = {
    getInterfaces: function (count) {
        var ifaces = [
            Components.interfaces.nsISupports,
            Components.interfaces.calIItipItem,
            Components.interfaces.nsIClassInfo
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function (language) {
        return null;
    },

    contractID: "@mozilla.org/calendar/itip-item;1",
    classDescription: "Calendar iTIP item",
    classID: Components.ID("{b84de879-4b85-4d68-8550-e0C527e46f98}"),
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0
};

calItipItem.prototype = {
    QueryInterface: function (aIID) {
        if (aIID.equals(Ci.nsIClassInfo)) {
            return calItipItemClassInfo;
        }

        if (aIID.equals(Ci.nsISupports) || aIID.equals(Ci.calIItipItem)) {
            return this;
        } else {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    mIsSend: false,

    get isSend() {
        return this.mIsSend;
    },
    set isSend(value) {
        this.mIsSend = value;
    },

    mReceivedMethod: null,

    get receivedMethod() {
        return this.mReceivedMethod;
    },
    set receivedMethod(value) {
        this.mReceivedMethod = value;
    },

    mResponseMethod: null,

    get responseMethod() {
        return this.mResponseMethod;
    },
    set responseMethod(value) {
        this.mResponseMethod = value;
    },

    mAutoResponse: null,

    get autoResponse() {
        return this.mAutoResponse;
    },
    set autoResponse(value) {
        this.mAutoResponse = value;
    },

    mTargetCalendar: null,

    get targetCalendar() {
        return this.mTargetCalendar;
    },
    set targetCalendar(value) {
        this.mTargetCalendar = value;
    },

    modifyItem: function(item) {
        // Bug 348666: This will be used when we support delegation and COUNTER
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },

    mItemList: null,

    init: function(icalData) {
        this.mItemList = new Array();
        var icsService = Cc["@mozilla.org/calendar/ics-service;1"]
                         .getService(Components.interfaces.calIICSService);
        var rootComp = icsService.parseICS(icalData);
        var calComp;
        // libical returns the vcalendar component if there is just
        // one vcalendar. If there are multiple vcalendars, it returns
        // an xroot component, with those vcalendar children. We need to
        // handle both cases.
        if (rootComp.componentType == 'VCALENDAR') {
            calComp = rootComp;
        } else {
            calComp = rootComp.getFirstSubcomponent('VCALENDAR');
        }
        // Get the method property out of the calendar to set our methods
        var method = calComp.method;
        // We set both methods now for safety's sake. It is the iTIP
        // processor's responsibility to properly ascertain what the correct
        // response method is (using user feedback, pref's etc) for this given
        // ReceivedMethod
        this.mReceivedMethod = method;
        this.mResponseMethod = method;

        while (calComp) {
            var subComp = calComp.getFirstSubcomponent("ANY");
            while (subComp) {
                switch (subComp.componentType) {
                case "VEVENT":
                    var event = Cc["@mozilla.org/calendar/event;1"]
                                .createInstance(Ci.calIEvent);
                    event.icalComponent = subComp;
                    this.mItemList.push(event);
                    break;
                case "VTODO":
                    var todo = Cc["@mozilla.org/calendar/todo;1"]
                               .createInstance(Ci.calITodo);
                    todo.icalComponent = subComp;
                    this.mItemList.push(todo);
                    break;
                default:
                    // Nothing -- Bug 185537: Implement VFREEBUSY, VJOURNAL
                }
                subComp = calComp.getNextSubcomponent("ANY");
            }
            calComp = rootComp.getNextSubcomponent('VCALENDAR');
        }
        this.mIsInitialized = true;
    },

    getFirstItem: function() {
        if (!this.mIsInitialized || (this.mItemList.length == 0)) {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }
        this.mCurrentItemIndex = 0;
        return this.mItemList[0];
    },

    getNextItem: function() {
        if (!this.mIsInitialized || (this.mItemList.length == 0)) {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }
        ++this.mCurrentItemIndex;
        if (this.mCurrentItemIndex < this.mItemList.length) {
            return this.mItemList[this.mCurrentItemIndex];
        } else {
            return null;
        }
    },

    setAttendeeStatus: function (attendeeID, status) {
        // Note that this code forces the user to respond to all items
        // in the same way, which is a current limitation of the spec
        if (attendeeID.match(/mailto:/i)) {
            attendeeID = "mailto:" + attendeeID; // prepend mailto
        }
        for (var i=0; i < this.mItemList.length; ++i) {
            var item = this.mItemList[i];
            var attendee = item.getAttendeeById(attendeeID);
            if (attendee) {
                // XXX BUG 351589: workaround for updating an attendee
                item.removeAttendee(attendee);
                attendee.participationStatus = status;
                item.addAttendee(attendee);
            }
        }
    }
};

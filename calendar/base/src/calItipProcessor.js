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
 * The Original Code is Simdesk Technologies code.
 *
 * The Initial Developer of the Original Code is Simdesk Technologies Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Clint Talbert <ctalbert.moz@gmail.com>
 *   Eva Or <evaor1012@yahoo.ca>
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


// Operations on the calendar
const CAL_ITIP_PROC_ADD_OP = 1;
const CAL_ITIP_PROC_UPDATE_OP = 2;
const CAL_ITIP_PROC_DELETE_OP = 3;

/**
 * Constructor of calItipItem object
 */
function calItipProcessor() {
    this.wrappedJSObject = this;
}

calItipProcessor.prototype = {
    getInterfaces: function cipGI(count) {
        var ifaces = [
            Ci.nsIClassInfo,
            Ci.nsISupports,
            Ci.calIItipProcessor
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function cipGHFL(aLanguage) {
        return null;
    },

    contractID: "@mozilla.org/calendar/itip-processor;1",
    classDescription: "Calendar iTIP processor",
    classID: Components.ID("{9787876b-0780-4464-8282-b7f86fb221e8}"),
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0,

    QueryInterface: function cipQI(aIid) {
        if (!aIid.equals(Ci.nsIClassInfo) && !aIid.equals(Ci.nsISupports) &&
            !aIid.equals(Ci.calIItipProcessor))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    mIsUserInvolved: false,
    get isUserInvolved() {
        return this.mIsUserInvolved;
    },
    set isUserInvolved(aValue) {
        return (this.mIsUserInvolved = aValue);
    },

    mRecvMethod: null,
    mRespMethod: null,
    mAutoResponse: null,
    mTargetCalendar: null,
    mCalItem: null,
    mCalItemType: null,

    /**
     * Processes the given calItipItem based on the settings inside it.
     * @param calIItipItem  A calItipItem to process.
     * @param calIOperationListener A calIOperationListener to return status
     * @return boolean  Whether processing succeeded or not.
     */
    processItipItem: function cipPII(aItipItem, aListener) {
        // Sanity check the input
        if (!aItipItem) {
            throw new Components.Exception("processItipItem: " +
                                           "Invalid or non-existant " +
                                           "itipItem passed in.",
                                           Components.results.NS_ERROR_INVALID_ARG);
        }

        // Clone the passed in itipItem like a sheep.
        var respItipItem = aItipItem.clone();

        this.mRecvMethod = respItipItem.receivedMethod;
        respItipItem.responseMethod = this._suggestResponseMethod(respItipItem);
        this.mRespMethod = respItipItem.responseMethod;

        this.mAutoResponse = respItipItem.autoResponse;
        this.mTargetCalendar = respItipItem.targetCalendar;

        // XXX Support for transports other than email go here.
        //     For now we just assume it's email.
        var transport = Cc["@mozilla.org/calendar/itip-transport;1?type=email"].
                        createInstance(Ci.calIItipTransport);

        // Sanity checks using the first item
        var itemList = respItipItem.getItemList({ });
        this.mCalItem = itemList[0];
        if (!this.mCalItem) {
            throw new Error ("processItipItem: " +
                             "getFirstItem() found no items!");
        }

        this.mCalItemType = this._getCalItemType(this.mCalItem);
        if (!this.mCalItemType) {
            throw new Error ("processItipItem: " +
                             "_getCalItemType() found no item type!");
        }

        // Sanity check that mRespMethod is a valid response per the spec.
        if (!this._isValidResponseMethod()) {
            throw new Error ("processItipItem: " +
                             "_isValidResponseMethod() found an invalid " +
                             "response method: " + mRespMethod);
        }

        var i = 0;
        while (this.mCalItem) {
            switch (this.mRecvMethod) {
                case "REQUEST":
                    // Only add to calendar if we accepted invite
                    var replyStat = this._getReplyStatus(this.mCalItem,
                                                         transport.defaultIdentity);
                    if (replyStat != "DECLINED") {
                        if (!this._processCalendarAction(this.mCalItem,
                                                         CAL_ITIP_PROC_ADD_OP,
                                                         aListener))
                        {
                            throw new Error ("processItipItem: " +
                                             "_processCalendarAction failed!");
                        }
                    }
                    break;

                case "PUBLISH":
                case "REPLY":
                case "REFRESH":
                case "ADD":
                case "CANCEL":
                case "COUNTER":
                case "DECLINECOUNTER":
                    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

                default:
                    throw new Error("processItipItem: " +
                                    "Received unknown method: " +
                                    this.mRecvMethod);
            }
            ++i;
            this.mCalItem = itemList[i];
        }

        // When replying, the reply must only contain the ORGANIZER and the
        // status of the ATTENDEE that represents ourselves. Therefore we must
        // remove all other ATTENDEEs from the itipItem we send back.
        if (this.mRespMethod == "REPLY") {
            // Get the id that represents me.
            // XXX Note that this doesn't take into consideration invitations
            //     sent to email aliases. (ex: lilmatt vs mwillis)
            var me;
            var idPrefix;
            if (transport.type == "email") {
                me = transport.defaultIdentity;
                idPrefix = "mailto:";
            } else {
                throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
            }

            for (var j=0; j < itemList.length; ++j) {
                var respCalItem = itemList[j];
                var attendees = respCalItem.getAttendees({});

                for each (var attendee in attendees) {
                    // Leave the ORGANIZER alone.
                    if (!attendee.isOrganizer) {
                        // example: mailto:joe@domain.com
                        var meString = idPrefix + me;
                        if (attendee.id.toLowerCase() != meString.toLowerCase()) {
                            respCalItem.removeAttendee(attendee);
                        }
                    }
                }
            }
        }

        // Send the appropriate response
        transport.simpleSendResponse(respItipItem);

        // Yay it worked!
        // XXX TODO: Actually tie this to success/failure of the transport
        return true;
    },


    /**
     * @return integer  The next recommended iTIP state.
     */
    _suggestResponseMethod: function cipSRM() {
        switch (this.mRecvMethod) {
            case "REQUEST":
                return "REPLY";

            case "REFRESH":
            case "COUNTER":
                return "REQUEST";

            case "PUBLISH":
            case "REPLY":
            case "ADD":
            case "CANCEL":
            case "DECLINECOUNTER":
                return this.mRecvMethod;

            default:
                throw new Error("_suggestResponseMethod: " +
                                "Received unknown method: " +
                                this.mRecvMethod);
        }
    },

    /**
     * Given mRecvMethod and mRespMethod, this checks that mRespMethod is
     * valid according to the spec.
     *
     * @return boolean  Whether or not mRespMethod is valid.
     */
    _isValidResponseMethod: function cipIAR() {
        switch (this.mRecvMethod) {
            // We set response to ADD automatically, but if the GUI did not
            // find the event the user may set it to REFRESH as per the spec.
            // These are the only two valid responses.
            case "ADD":
                if (!(this.mRespMethod == "ADD" ||
                     (this.mRespMethod == "REFRESH" &&
                     // REFRESH is not a valid response to an ADD for VJOURNAL
                     (this.mCalItemType == Ci.calIEvent ||
                      this.mCalItemType == Ci.calITodo))))
                {
                    return false;
                }
                break;

            // Valid responses to COUNTER are REQUEST or DECLINECOUNTER.
            case "COUNTER":
                if (!(this.mRespMethod == "REQUEST" ||
                      this.mRespMethod == "DECLINECOUNTER"))
                {
                    return false;
                }
                break;

            // Valid responses to REQUEST are:
            //     REPLY   (accept or error)
            //     REQUEST (delegation, inviting someone else)
            //     COUNTER (propose a change)
            case "REQUEST":
                if (!(this.mRespMethod == "REPLY" ||
                      this.mRespMethod == "REQUEST" ||
                      this.mRespMethod == "COUNTER"))
                {
                    return false;
                }
                break;

            // REFRESH should respond with a request
            case "REFRESH":
                if (this.mRespMethod == "REQUEST") {
                    return false;
                }
                break;

            // The rest are easiest represented as:
            //     (mRecvMethod != mRespMethod) == return false
            case "PUBLISH":
            case "CANCEL":
            case "REPLY":
            case "PUBLISH":
            case "DECLINECOUNTER":
                if (this.mRespMethod != this.mRecvMethod) {
                    return false;
                }
                break;

            default:
                throw new Error("_isValidResponseMethod: " +
                                "Received unknown method: " +
                                this.mRecvMethod);
        }

        // If we got to here, then the combination is valid.
        return true;
    },

    /**
     * Helper to return whether an item is an event, todo, etc.
     */
    _getCalItemType: function cipGCIT() {
        if (this.mCalItem instanceof Ci.calIEvent) {
            return Ci.calIEvent;
        } else if (aCalItem instanceof Ci.calITodo) {
            return Ci.calITodo;
        }

        throw new Error ("_getCalItemType: " +
                         "mCalItem item type is unknown");
    },

    /**
     * This performs the actual add/update/delete of an event on the user's
     * calendar.
     */
    _processCalendarAction: function cipPCA(aCalItem, aOperation, aListener) {
        switch (aOperation) {
            case CAL_ITIP_PROC_ADD_OP:
                // We assume all adds take place on the target calendar
                this.mTargetCalendar.addItem(aCalItem, aListener);

                // XXX Change this to reflect the success or failure of adding
                //     the item to the calendar.
                return true;

            case CAL_ITIP_PROC_UPDATE_OP:
            case CAL_ITIP_PROC_DELETE_OP:
                throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

            default:
                throw new Error("_processCalendarAction: " +
                                "Undefined Operator: " + aOperator);
        }

        // If you got to here, something went horribly, horribly wrong.
        return false;
    },

    /**
     * Helper function to get the PARTSTAT value from the given calendar
     * item for the specified attendee
     */
    _getReplyStatus: function cipGRS(aCalItem, aAttendeeId) {
        var idPrefix = "mailto:";
        var replyStatus;

        // example: mailto:joe@domain.com
        var idString = idPrefix + aAttendeeId;
        var attendee = aCalItem.getAttendeeById(idString);
        if (attendee) {
            replyStatus = attendee.participationStatus;
        }
        return replyStatus;
    }
}

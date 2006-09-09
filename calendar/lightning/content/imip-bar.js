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
 * Clint Talbert <cmtalbert@myfastmail.com>
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
 * This bar lives inside the message window, its lifetime is the lifetime of
 * the main thunderbird message window.
 */

var gItipItem;

/**
 * Sets up iTIP creation event listener
 * When the mime parser discovers a text/calendar attachment to a message,
 * it creates a calIItipItem and calls an observer. We watch for that
 * observer. Bug 351610 is open so that we can find a way to make this 
 * communication mechanism more robust.
*/
const onItipItem = {
    observe: function observe(subject, topic, state) {
        if (topic == "onItipItemCreation") {
            try {
                var itipItem = 
                    subject.QueryInterface(Components.interfaces.calIItipItem);
                // We are only called upon receipt of an invite, so
                // ensure that isSend is false.
                itipItem.isSend = false;

                // Until the iTIPResponder code lands, we do no response
                // at all.
                itipItem.autoResponse = Components.interfaces.calIItipItem.NONE;
                itipItem.targetCalendar = getTargetCalendar();

                var imipMethod = getMsgImipMethod();
                if (imipMethod.length) {
                    itipItem.receivedMethod = imipMethod;
                } else {
                    // Thunderbird 1.5 case, we cannot get the iMIPMethod
                    imipMethod = itipItem.receivedMethod;
                }

                gItipItem = itipItem;

                // XXX Bug 351742: no security yet
                // handleImipSecurity(imipMethod);

                setupBar(imipMethod);

            } catch (e) {
                Components.utils.reportError(e);
            }
        }
    }
};

addEventListener('messagepane-loaded', imipOnLoad, true);
addEventListener('messagepane-unloaded', imipOnUnload, true);

/**
 * Attempt to add self to gMessageListeners defined in msgHdrViewOverlay.js
 */
function imipOnLoad()
{
    var listener = {};
    listener.onStartHeaders = onImipStartHeaders;
    listener.onEndHeaders = onImipEndHeaders;
    gMessageListeners.push(listener);

    // Set up our observers
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                    .getService(Components.interfaces.nsIObserverService);
    observerService.addObserver(onItipItem, "onItipItemCreation", false);
}

function imipOnUnload()
{
    removeEventListener('messagepane-loaded', imipOnLoad, true);
    removeEventListener('messagepane-unloaded', imipOnUnload, true);
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                          .getService(Components.interfaces.nsIObserverService);
    observerService.removeObserver(onItipItem, "onItipItemCreation");
    gItipItem = null;
}

function onImipStartHeaders()
{
    var imipBar = document.getElementById("imip-bar");
    imipBar.setAttribute("collapsed", "true");

    // New Message is starting, clear our iMIP/iTIP stuff so that we don't set
    // it by accident
    imipMethod = "";
    gItipItem = null;
}

// We need an onEndHeader or else MessageListner will throw. However,
// we do not need to actually do anything in this function.
function onImipEndHeaders()
{
}

function setupBar(imipMethod)
{
    // XXX - Bug 348666 - Currently we only do PUBLISH requests
    // In the future this function will set up the proper actions
    // and attributes for the buttons as based on the iMIP Method
    var imipBar = document.getElementById("imip-bar");
    imipBar.setAttribute("collapsed","false");
    var description = document.getElementById("imip-description");

    // Bug 348666: here is where we would check if this event was already
    // added to calendar or not and display correct information here
    if (description.firstChild.data) {
        description.firstChild.data = ltnGetString("lightning","imipBarText");
    }

    // Since there is only a PUBLISH, this is easy
    var button1 = document.getElementById("imip-btn1");
    button1.removeAttribute("hidden");
    button1.setAttribute("label", ltnGetString("lightning",
                                               "imipAddToCalendar.label"));
    button1.setAttribute("oncommand", 
                         "setAttendeeResponse('PUBLISH', 'CONFIRMED');");
}

function getMsgImipMethod()
{
    var imipMethod = "";
    var msgURI = GetLoadedMessage();
    var msgHdr = messenger.messageServiceFromURI(msgURI)
                          .messageURIToMsgHdr(msgURI);
    imipMethod = msgHdr.getStringProperty("imip_method");
    return imipMethod;
}

function getMsgRecipient()
{
    var imipRecipient = "";
    var msgURI = GetLoadedMessage();
    var msgHdr = messenger.messageServiceFromURI(msgURI)
                          .messageURIToMsgHdr(msgURI);
    // msgHdr.recipients is always the one recipient - the one looking at
    // the email, which is who we are interested in.
    if (msgHdr) {
        imipRecipient = msgHdr.recipients;
    }
    return imipRecipient;
}

/**
 * Bug 351745 - Call calendar picker here
 */
function getTargetCalendar()
{
    var calMgr = Components.classes["@mozilla.org/calendar/manager;1"]
                           .getService(Components.interfaces.calICalendarManager);
    var cals = calMgr.getCalendars({});
    return cals[0];
}

/**
 * Type is type of response
 * event_status is an optional directive to set the Event STATUS property
 */
function setAttendeeResponse(type, eventStatus)
{
    var myAddress = getMsgRecipient();
    if (type && gItipItem) {
        switch (type) { // We set the attendee status appropriately
            case "ACCEPTED":
            case "TENTATIVE":
            case "DECLINED":
                gItipItem.setAttendeeStatus(myAddress, type);
                doResponse();
                break;
            case "REPLY":
            case "PUBLISH":
                doResponse(eventStatus);
                break;
            default:
                // Nothing -- if given nothing then the attendee wishes to
                // disregard the mail, so no further action required
                break;
        }
    }

    finishItipAction(type, eventStatus);
}

/**
 * doResponse performs the iTIP action for the current iTIPItem that we
 * parsed from the email.
 * Takes an optional parameter to set the event STATUS property
 */
function doResponse(eventStatus)
{
    // XXX For now, just add the item to the calendar
    // The spec is unclear if we must add all the items or if the
    // user should get to pick which item gets added.
    var cal = getTargetCalendar();

    var item = gItipItem.getFirstItem();
    while (item != null) {
        if (eventStatus.length)
            item.status = eventStatus;
        cal.addItem(item, null);
        item = gItipItem.getNextItem();
    }
}

/**
 * Bug 348666 (complete iTIP support) - This gives the user an indication
 * that the Action occurred.
 * In the future we want to store the status of invites that you have added
 * to your calendar and provide the ability to request updates from the
 * organizer. This function will be responsible for setting up and
 * maintaining the proper lists of events and informing the user as to the
 * state of the iTIP action
 * Additionally, this function should ONLY be called once we KNOW what the
 * status of the event addition was. It must wait for a clear signal from the
 * iTIP Processor as to whether or not the event added/failed to add or
 * what have you. It should alert the user to the true status of the operation
 */
function finishItipAction(type, eventStatus)
{
    // For now, we just do something very simple
    var description = document.getElementById("imip-description");
    if (description.firstChild.data) {
        description.firstChild.data = ltnGetString("lightning",
                                                   "imipAddedItemToCal");
    }
    document.getElementById("imip-btn1").setAttribute("hidden", true);
}


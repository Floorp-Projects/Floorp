/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

/**
 * RILSystemMessenger
 */
this.RILSystemMessenger = function() {};
RILSystemMessenger.prototype = {

  /**
   * Hook of Broadcast function
   *
   * @param aType
   *        The type of the message to be sent.
   * @param aMessage
   *        The message object to be broadcasted.
   */
  broadcastMessage: function(aType, aMessage) {
    // Function stub to be replaced by the owner of this messenger.
  },

  /**
   * Wrapper to send "telephony-new-call" system message.
   */
  notifyNewCall: function() {
    this.broadcastMessage("telephony-new-call", {});
  },

  /**
   * Wrapper to send "telephony-call-ended" system message.
   */
  notifyCallEnded: function(aServiceId, aNumber, aCdmaWaitingNumber, aEmergency,
                            aDuration, aOutgoing, aHangUpLocal) {
    let data = {
      serviceId: aServiceId,
      number: aNumber,
      emergency: aEmergency,
      duration: aDuration,
      direction: aOutgoing ? "outgoing" : "incoming",
      hangUpLocal: aHangUpLocal
    };

    if (aCdmaWaitingNumber != null) {
      data.secondNumber = aCdmaWaitingNumber;
    }

    this.broadcastMessage("telephony-call-ended", data);
  },

  _convertSmsMessageClass: function(aMessageClass) {
    return RIL.GECKO_SMS_MESSAGE_CLASSES[aMessageClass] || null;
  },

  _convertSmsDelivery: function(aDelivery) {
    return ["received", "sending", "sent", "error"][aDelivery] || null;
  },

  _convertSmsDeliveryStatus: function(aDeliveryStatus) {
    return [
      RIL.GECKO_SMS_DELIVERY_STATUS_NOT_APPLICABLE,
      RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS,
      RIL.GECKO_SMS_DELIVERY_STATUS_PENDING,
      RIL.GECKO_SMS_DELIVERY_STATUS_ERROR
    ][aDeliveryStatus] || null;
  },

  /**
   * Wrapper to send 'sms-received', 'sms-delivery-success', 'sms-sent' system message.
   */
  notifySms: function(aNotificationType, aId, aThreadId, aIccId, aDelivery,
                      aDeliveryStatus, aSender, aReceiver, aBody, aMessageClass,
                      aTimestamp, aSentTimestamp, aDeliveryTimestamp, aRead) {
    let msgType =
      ["sms-received", "sms-sent", "sms-delivery-success"][aNotificationType];

    if (!msgType) {
      throw new Error("Invalid Notification Type: " + aNotificationType);
    }

    this.broadcastMessage(msgType, {
      iccId:             aIccId,
      type:              "sms",
      id:                aId,
      threadId:          aThreadId,
      delivery:          this._convertSmsDelivery(aDelivery),
      deliveryStatus:    this._convertSmsDeliveryStatus(aDeliveryStatus),
      sender:            aSender,
      receiver:          aReceiver,
      body:              aBody,
      messageClass:      this._convertSmsMessageClass(aMessageClass),
      timestamp:         aTimestamp,
      sentTimestamp:     aSentTimestamp,
      deliveryTimestamp: aDeliveryTimestamp,
      read:              aRead
    });
  }
};

this.EXPORTED_SYMBOLS = [
  'RILSystemMessenger'
];

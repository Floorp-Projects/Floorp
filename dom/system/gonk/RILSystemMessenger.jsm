/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
  }
};

this.EXPORTED_SYMBOLS = [
  'RILSystemMessenger'
];

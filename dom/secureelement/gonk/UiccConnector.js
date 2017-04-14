/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copyright © 2014, Deutsche Telekom, Inc. */

"use strict";

/* globals Components, XPCOMUtils, SE, dump, libcutils, Services,
   iccService, SEUtils */

const { interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/systemlibs.js");

XPCOMUtils.defineLazyGetter(this, "SE", function() {
  let obj = {};
  Cu.import("resource://gre/modules/se_consts.js", obj);
  return obj;
});

// set to true in se_consts.js to see debug messages
var DEBUG = SE.DEBUG_CONNECTOR;
function debug(s) {
  if (DEBUG) {
    dump("-*- UiccConnector: " + s + "\n");
  }
}

XPCOMUtils.defineLazyModuleGetter(this, "SEUtils",
                                  "resource://gre/modules/SEUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "iccService",
                                   "@mozilla.org/icc/iccservice;1",
                                   "nsIIccService");

const UICCCONNECTOR_CONTRACTID =
  "@mozilla.org/secureelement/connector/uicc;1";
const UICCCONNECTOR_CID =
  Components.ID("{8e040e5d-c8c3-4c1b-ac82-c00d25d8c4a4}");
const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

// TODO: Bug 1118099  - Add multi-sim support.
// In the Multi-sim, there is more than one client.
// For now, use default clientID as 0. Ideally, SE parent process would like to
// know which clients (uicc slot) are connected to CLF over SWP interface.
const PREFERRED_UICC_CLIENTID =
  libcutils.property_get("ro.moz.se.def_client_id", "0");

/**
 * 'UiccConnector' object is a wrapper over iccService's channel management
 * related interfaces that implements nsISecureElementConnector interface.
 */
function UiccConnector() {
  this._init();
}

UiccConnector.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISecureElementConnector,
                                         Ci.nsIIccListener]),
  classID: UICCCONNECTOR_CID,
  classInfo: XPCOMUtils.generateCI({
    classID: UICCCONNECTOR_CID,
    contractID: UICCCONNECTOR_CONTRACTID,
    classDescription: "UiccConnector",
    interfaces: [Ci.nsISecureElementConnector,
                 Ci.nsIIccListener,
                 Ci.nsIObserver]
  }),

  _SEListeners: [],
  _isPresent: false,

  _init: function() {
    Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    let icc = iccService.getIccByServiceId(PREFERRED_UICC_CLIENTID);
    icc.registerListener(this);

    // Update the state in order to avoid race condition.
    // By this time, 'notifyCardStateChanged (with proper card state)'
    // may have occurred already before this module initialization.
    this._updatePresenceState();
  },

  _shutdown: function() {
    Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    let icc = iccService.getIccByServiceId(PREFERRED_UICC_CLIENTID);
    icc.unregisterListener(this);
  },

  _updatePresenceState: function() {
    let uiccNotReadyStates = [
      Ci.nsIIcc.CARD_STATE_UNKNOWN,
      Ci.nsIIcc.CARD_STATE_ILLEGAL,
      Ci.nsIIcc.CARD_STATE_PERSONALIZATION_IN_PROGRESS,
      Ci.nsIIcc.CARD_STATE_PERMANENT_BLOCKED,
      Ci.nsIIcc.CARD_STATE_UNDETECTED
    ];

    let cardState = iccService.getIccByServiceId(PREFERRED_UICC_CLIENTID).cardState;
    let uiccPresent = cardState !== null &&
                      uiccNotReadyStates.indexOf(cardState) == -1;

    if (this._isPresent === uiccPresent) {
      return;
    }

    debug("Uicc presence changed " + this._isPresent + " -> " + uiccPresent);
    this._isPresent = uiccPresent;
    this._SEListeners.forEach((listener) => {
      listener.notifySEPresenceChanged(SE.TYPE_UICC, this._isPresent);
    });
  },

  // See GP Spec, 11.1.4 Class Byte Coding
  _setChannelToCLAByte: function(cla, channel) {
    if (channel < SE.LOGICAL_CHANNEL_NUMBER_LIMIT) {
      // b7 = 0 indicates the first interindustry class byte coding
      cla = (cla & 0x9C) & 0xFF | channel;
    } else if (channel < SE.SUPPLEMENTARY_LOGICAL_CHANNEL_NUMBER_LIMIT) {
      // b7 = 1 indicates the further interindustry class byte coding
      cla = (cla & 0xB0) & 0xFF | 0x40 | (channel - SE.LOGICAL_CHANNEL_NUMBER_LIMIT);
    } else {
      debug("Channel number must be within [0..19]");
      return SE.ERROR_GENERIC;
    }
    return cla;
  },

  _doGetOpenResponse: function(channel, length, callback) {
    // Le value is set. It means that this is a request for all available
    // response bytes.
    let cla = this._setChannelToCLAByte(SE.CLA_GET_RESPONSE, channel);
    this.exchangeAPDU(channel, cla, SE.INS_GET_RESPONSE, 0x00, 0x00,
                      null, length, {
      notifyExchangeAPDUResponse: function(sw1, sw2, response) {
        debug("GET Response : " + response);
        if (callback) {
          callback({
            error: SE.ERROR_NONE,
            sw1: sw1,
            sw2: sw2,
            response: response
          });
        }
      },

      notifyError: function(reason) {
        debug("Failed to get open response: " +
              ", Rejected with Reason : " + reason);
        if (callback) {
          callback({ error: SE.ERROR_INVALIDAPPLICATION, reason: reason });
        }
      }
    });
  },

  _doIccExchangeAPDU: function(channel, cla, ins, p1, p2, p3,
                               data, appendResp, callback) {
    let icc = iccService.getIccByServiceId(PREFERRED_UICC_CLIENTID);
    icc.iccExchangeAPDU(channel, cla & 0xFC, ins, p1, p2, p3, data, {
      notifyExchangeAPDUResponse: (sw1, sw2, response) => {
        debug("sw1 : " + sw1 + ", sw2 : " + sw2 + ", response : " + response);

        // According to ETSI TS 102 221 , Section 7.2.2.3.1,
        // Enforce 'Procedure bytes' checks before notifying the callback.
        // Note that 'Procedure bytes'are special cases.
        // There is no need to handle '0x60' procedure byte as it implies
        // no-action from SE stack perspective. This procedure byte is not
        // notified to application layer.
        if (sw1 === 0x6C) {
          // Use the previous command header with length as second procedure
          // byte (SW2) as received and repeat the procedure.

          // Recursive! and Pass empty response '' as args, since '0x6C'
          // procedure does not have to deal with appended responses.
          this._doIccExchangeAPDU(channel, cla, ins, p1, p2,
                                  sw2, data, "", callback);
        } else if (sw1 === 0x61) {
          // Since the terminal waited for a second procedure byte and
          // received it (sw2), send a GET RESPONSE command header to the UICC
          // with a maximum length of 'XX', where 'XX' is the value of the
          // second procedure byte (SW2).

          let claWithChannel = this._setChannelToCLAByte(SE.CLA_GET_RESPONSE,
                                                         channel);

          // Recursive, with GET RESPONSE bytes and '0x61' procedure IS interested
          // in appended responses. Pass appended response and note that p3=sw2.
          this._doIccExchangeAPDU(channel, claWithChannel, SE.INS_GET_RESPONSE,
                                  0x00, 0x00, sw2, null,
                                  (response ? response + appendResp : appendResp),
                                  callback);
        } else if (callback) {
          callback.notifyExchangeAPDUResponse(sw1, sw2, response);
        }
      },

      notifyError: (reason) => {
        debug("Failed to trasmit C-APDU over the channel #  : " + channel +
              ", Rejected with Reason : " + reason);
        if (callback) {
          callback.notifyError(reason);
        }
      }
    });
  },

  /**
   * nsISecureElementConnector interface methods.
   */

  /**
   * Opens a channel on a default clientId
   */
  openChannel: function(aid, callback) {
    if (!this._isPresent) {
      callback.notifyError(SE.ERROR_NOTPRESENT);
      return;
    }

    // TODO: Bug 1118106: Handle Resource management / leaks by persisting
    // the newly opened channel in some persistent storage so that when this
    // module gets restarted (say after opening a channel) in the event of
    // some erroneous conditions such as gecko restart /, crash it can read
    // the persistent storage to check if there are any held resources
    // (opened channels) and close them.
    let icc = iccService.getIccByServiceId(PREFERRED_UICC_CLIENTID);
    icc.iccOpenChannel(aid, {
      notifyOpenChannelSuccess: (channel) => {
        this._doGetOpenResponse(channel, 0x00, function(result) {
          if (callback) {
            callback.notifyOpenChannelSuccess(channel, result.response);
          }
        });
      },

      notifyError: (reason) => {
        debug("Failed to open the channel to AID : " + aid +
              ", Rejected with Reason : " + reason);
        if (callback) {
          callback.notifyError(reason);
        }
      }
    });
  },

  /**
   * Transmit the C-APDU (command) on default clientId.
   */
  exchangeAPDU: function(channel, cla, ins, p1, p2, data, le, callback) {
    if (!this._isPresent) {
      callback.notifyError(SE.ERROR_NOTPRESENT);
      return;
    }

    if (data && data.length % 2 !== 0) {
      callback.notifyError("Data should be a hex string with length % 2 === 0");
      return;
    }

    cla = this._setChannelToCLAByte(cla, channel);
    let lc = data ? data.length / 2 : 0;
    let p3 = lc || le;

    if (lc && (le !== -1)) {
      data += SEUtils.byteArrayToHexString([le]);
    }

    // Pass empty response '' as args as we are not interested in appended
    // responses yet!
    debug("exchangeAPDU on Channel # " + channel);
    this._doIccExchangeAPDU(channel, cla, ins, p1, p2, p3, data, "",
                            callback);
  },

  /**
   * Closes the channel on default clientId.
   */
  closeChannel: function(channel, callback) {
    if (!this._isPresent) {
      callback.notifyError(SE.ERROR_NOTPRESENT);
      return;
    }

    let icc = iccService.getIccByServiceId(PREFERRED_UICC_CLIENTID);
    icc.iccCloseChannel(channel, {
      notifyCloseChannelSuccess: function() {
        debug("closeChannel successfully closed the channel # : " + channel);
        if (callback) {
          callback.notifyCloseChannelSuccess();
        }
      },

      notifyError: function(reason) {
        debug("Failed to close the channel #  : " + channel +
              ", Rejected with Reason : " + reason);
        if (callback) {
          callback.notifyError(reason);
        }
      }
    });
  },

  registerListener: function(listener) {
    if (this._SEListeners.indexOf(listener) !== -1) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._SEListeners.push(listener);
    // immediately notify listener about the current state
    listener.notifySEPresenceChanged(SE.TYPE_UICC, this._isPresent);
  },

  unregisterListener: function(listener) {
    let idx = this._SEListeners.indexOf(listener);
    if (idx !== -1) {
      this._SEListeners.splice(idx, 1);
    }
  },

  /**
   * nsIIccListener interface methods.
   */
  notifyStkCommand: function() {},

  notifyStkSessionEnd: function() {},

  notifyIccInfoChanged: function() {},

  notifyCardStateChanged: function() {
    debug("Card state changed, updating UICC presence.");
    this._updatePresenceState();
  },

  /**
   * nsIObserver interface methods.
   */

  observe: function(subject, topic, data) {
    if (topic === NS_XPCOM_SHUTDOWN_OBSERVER_ID) {
      this._shutdown();
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([UiccConnector]);

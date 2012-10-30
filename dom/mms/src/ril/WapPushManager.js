/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/WspPduHelper.jsm", this);

const DEBUG = false; // set to true to see debug messages

/**
 * Helpers for WAP PDU processing.
 */
this.WapPushManager = {

  /**
   * Parse raw PDU data and deliver to a proper target.
   *
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param options
   *        Extra context for decoding.
   */
  processMessage: function processMessage(data, options) {
    try {
      PduHelper.parse(data, true, options);
      debug("options: " + JSON.stringify(options));
    } catch (ex) {
      debug("Failed to parse sessionless WSP PDU: " + ex.message);
      return;
    }

    let appid = options.headers["x-wap-application-id"];
    if (!appid) {
      debug("Push message doesn't contains X-Wap-Application-Id.");
      return;
    }

    if (appid == "x-wap-application:mms.ua") {
      let mmsService = Cc["@mozilla.org/mms/rilmmsservice;1"]
                       .getService(Ci.nsIMmsService);
      mmsService.QueryInterface(Ci.nsIWapPushApplication)
                .receiveWapPush(data.array, data.array.length, data.offset, options);
    } else {
      debug("No WAP Push application registered for " + appid);
    }
  },

  /**
   * @param array
   *        A Uint8Array or an octet array representing raw PDU data.
   * @param length
   *        Length of the array.
   * @param offset
   *        Offset of the array that a raw PDU data begins.
   * @param options
   *        WDP bearer information.
   */
  receiveWdpPDU: function receiveWdpPDU(array, length, offset, options) {
    if ((options.bearer == null) || !options.sourceAddress
        || (options.sourcePort == null) || !array) {
      debug("Incomplete WDP PDU");
      return;
    }

    if (options.destinationPort != WDP_PORT_PUSH) {
      debug("Not WAP Push port: " + options.destinationPort);
      return;
    }

    this.processMessage({array: array, offset: offset}, options);
  },
};

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- WapPushManager: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

this.EXPORTED_SYMBOLS = ALL_CONST_SYMBOLS.concat([
  "WapPushManager",
]);


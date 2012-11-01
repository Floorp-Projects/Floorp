/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function PaymentFlowInfo() {
};

PaymentFlowInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentFlowInfo]),
  classID: Components.ID("{b8bce4e7-fbf0-4719-a634-b1bf9018657c}"),
  uri: null,
  jwt: null,
  requestMethod: null
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentFlowInfo]);

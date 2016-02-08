/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

const PAYMENT_IPC_MSG_NAMES = ["Payment:Success",
                               "Payment:Failed"];

const PREF_DEBUG = "dom.payment.debug";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

var _debug;
try {
  _debug = Services.prefs.getPrefType(PREF_DEBUG) == Ci.nsIPrefBranch.PREF_BOOL
           && Services.prefs.getBoolPref(PREF_DEBUG);
} catch(e) {
  _debug = false;
}

function LOG(s) {
  if (!_debug) {
    return;
  }
  dump("-*- PaymentContentHelper: " + s + "\n");
}

function PaymentContentHelper(aWindow) {
  this.initDOMRequestHelper(aWindow, PAYMENT_IPC_MSG_NAMES);
};

PaymentContentHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  receiveMessage: function receiveMessage(aMessage) {
    let name = aMessage.name;
    let msg = aMessage.json;
    if (_debug) {
      LOG("Received message '" + name + "': " + JSON.stringify(msg));
    }
    let requestId = msg.requestId;
    let request = this.takeRequest(requestId);
    if (!request) {
      return;
    }
    switch (name) {
      case "Payment:Success":
        Services.DOMRequest.fireSuccess(request, msg.result);
        break;
      case "Payment:Failed":
        Services.DOMRequest.fireError(request, msg.errorMsg);
        break;
    }
  },
};

function PaymentContentHelperService() {
};

PaymentContentHelperService.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentContentHelperService]),
  classID: Components.ID("{80035846-6732-4fcc-961b-f336b65218f4}"),
  contractID: "@mozilla.org/payment/content-helper-service;1",

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(PaymentContentHelperService),

  // keys are windows and values are PaymentContentHelpers
  helpers: new WeakMap(),

  // nsINavigatorPayment
  pay: function pay(aWindow, aJwts) {
    let requestHelper = this.helpers.get(aWindow);
    if (!requestHelper) {
      requestHelper = new PaymentContentHelper(aWindow);
      this.helpers.set(aWindow, requestHelper);
    }
    let request = requestHelper.createRequest();
    let requestId = requestHelper.getRequestId(request);

    let docShell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShell);
    if (!docShell.isActive) {
      if (_debug) {
        LOG("The caller application is a background app. No request will be " +
            "sent");
      }

      let runnable = {
        run: function run() {
          Services.DOMRequest.fireError(request, "BACKGROUND_APP");
        }
      }
      Services.tm.currentThread.dispatch(runnable,
                                         Ci.nsIThread.DISPATCH_NORMAL);
      return request;
    }

    if (!Array.isArray(aJwts)) {
      aJwts = [aJwts];
    }

    cpmm.sendAsyncMessage("Payment:Pay", {
      jwts: aJwts,
      requestId: requestId
    });
    return request;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentContentHelperService]);

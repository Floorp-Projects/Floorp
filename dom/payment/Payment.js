/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

const PAYMENTCONTENTHELPER_CID =
  Components.ID("{a920adc0-c36e-4fd0-8de0-aac1ac6ebbd0}");

const PAYMENT_IPC_MSG_NAMES = ["Payment:Success",
                               "Payment:Failed"];

const PREF_DEBUG = "dom.payment.debug";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function PaymentContentHelper() {
};

PaymentContentHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavigatorPayment,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),
  classID:        PAYMENTCONTENTHELPER_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID: PAYMENTCONTENTHELPER_CID,
    contractID: "@mozilla.org/payment/content-helper;1",
    classDescription: "Payment Content Helper",
    flags: Ci.nsIClassInfo.DOM_OBJECT,
    interfaces: [Ci.nsINavigatorPayment]
  }),

  // nsINavigatorPayment

  pay: function pay(aJwts) {
    let request = this.createRequest();
    let requestId = this.getRequestId(request);

    let docShell = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShell);
    if (!docShell.isActive) {
      if (this._debug) {
        this.LOG("The caller application is a background app. No request " +
                  "will be sent");
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

  // nsIDOMGlobalPropertyInitializer

  init: function(aWindow) {
    try {
      if (!Services.prefs.getBoolPref("dom.mozPay.enabled")) {
        return null;
      }
    } catch (e) {
      return null;
    }

    this._window = aWindow;
    this.initDOMRequestHelper(aWindow, PAYMENT_IPC_MSG_NAMES);

    try {
      this._debug =
        Services.prefs.getPrefType(PREF_DEBUG) == Ci.nsIPrefBranch.PREF_BOOL
        && Services.prefs.getBoolPref(PREF_DEBUG);
    } catch(e) {
      this._debug = false;
    }

    return Cu.exportFunction(this.pay.bind(this), aWindow);
  },

  // nsIFrameMessageListener

  receiveMessage: function receiveMessage(aMessage) {
    let name = aMessage.name;
    let msg = aMessage.json;
    if (this._debug) {
      this.LOG("Received message '" + name + "': " + JSON.stringify(msg));
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

  LOG: function LOG(s) {
    if (!this._debug) {
      return;
    }
    dump("-*- PaymentContentHelper: " + s + "\n");
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentContentHelper]);

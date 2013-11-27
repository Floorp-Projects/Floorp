/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// JS shim that contains the callback functions to be triggered from the
// payment provider's code in order to fire DOMRequest events.
const kPaymentShimFile = "chrome://browser/content/payment.js";

// Type of MozChromEvents to handle payment dialogs.
const kOpenPaymentConfirmationEvent = "open-payment-confirmation-dialog";
const kOpenPaymentFlowEvent = "open-payment-flow-dialog";

const PREF_DEBUG = "dom.payment.debug";

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

function PaymentUI() {
  try {
    this._debug =
      Services.prefs.getPrefType(PREF_DEBUG) == Ci.nsIPrefBranch.PREF_BOOL
      && Services.prefs.getBoolPref(PREF_DEBUG);
  } catch(e) {
    this._debug = false;
  }
}

PaymentUI.prototype = {

  confirmPaymentRequest: function confirmPaymentRequest(aRequestId,
                                                        aRequests,
                                                        aSuccessCb,
                                                        aErrorCb) {
    let _error = function _error(errorMsg) {
      if (aErrorCb) {
        aErrorCb.onresult(aRequestId, errorMsg);
      }
    };

    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    let content = browser.getContentWindow();
    if (!content) {
      _error("NO_CONTENT_WINDOW");
      return;
    }

    // The UI should listen for mozChromeEvent 'open-payment-confirmation-dialog'
    // type in order to create and show the payment request confirmation frame
    // embeded within a trusted dialog.
    let id = kOpenPaymentConfirmationEvent + "-" + this.getRandomId();
    let detail = {
      type: kOpenPaymentConfirmationEvent,
      id: id,
      requestId: aRequestId,
      paymentRequests: aRequests
    };

    // Once the user confirm the payment request and makes his choice, we get
    // back to the DOM part to get the appropriate payment flow information
    // based on the selected payment provider.
    this._handleSelection = (function _handleSelection(evt) {
      let msg = evt.detail;
      if (msg.id != id) {
        return;
      }

      if (msg.userSelection && aSuccessCb) {
        aSuccessCb.onresult(aRequestId, msg.userSelection);
      } else if (msg.errorMsg) {
        _error(msg.errorMsg);
      }

      content.removeEventListener("mozContentEvent", this._handleSelection);
      this._handleSelection = null;
    }).bind(this);
    content.addEventListener("mozContentEvent", this._handleSelection);

    browser.shell.sendChromeEvent(detail);
  },

  showPaymentFlow: function showPaymentFlow(aRequestId,
                                            aPaymentFlowInfo,
                                            aErrorCb) {
    let _error = function _error(errorMsg) {
      if (aErrorCb) {
        aErrorCb.onresult(aRequestId, errorMsg);
      }
    };

    // We ask the UI to browse to the selected payment flow.
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    let content = browser.getContentWindow();
    if (!content) {
      _error("NO_CONTENT_WINDOW");
      return;
    }

    let id = kOpenPaymentFlowEvent + "-" + this.getRandomId();
    let detail = {
      type: kOpenPaymentFlowEvent,
      id: id,
      requestId: aRequestId,
      uri: aPaymentFlowInfo.uri,
      method: aPaymentFlowInfo.requestMethod,
      jwt: aPaymentFlowInfo.jwt
    };

    // At some point the UI would send the created iframe back so the
    // callbacks for firing DOMRequest events can be loaded on its
    // content.
    this._loadPaymentShim = (function _loadPaymentShim(evt) {
      let msg = evt.detail;
      if (msg.id != id) {
        return;
      }

      if (msg.errorMsg) {
        content.removeEventListener("mozContentEvent", this._loadPaymentShim);
        this._loadPaymentShim = null;
        _error("ERROR_LOADING_PAYMENT_SHIM: " + msg.errorMsg);
        return;
      }

      if (!msg.frame) {
        content.removeEventListener("mozContentEvent", this._loadPaymentShim);
        this._loadPaymentShim = null;
        _error("ERROR_LOADING_PAYMENT_SHIM");
        return;
      }

      // Try to load the payment shim file containing the payment callbacks
      // in the content script.
      let frame = msg.frame;
      let frameLoader = frame.QueryInterface(Ci.nsIFrameLoaderOwner)
                             .frameLoader;
      let mm = frameLoader.messageManager;
      try {
        mm.loadFrameScript(kPaymentShimFile, true);
        mm.sendAsyncMessage("Payment:LoadShim", { requestId: aRequestId });
      } catch (e) {
        if (this._debug) {
          this.LOG("Error loading " + kPaymentShimFile + " as a frame script: "
                    + e);
        }
        _error("ERROR_LOADING_PAYMENT_SHIM");
      } finally {
        content.removeEventListener("mozContentEvent", this._loadPaymentShim);
        this._loadPaymentShim = null;
      }
    }).bind(this);
    content.addEventListener("mozContentEvent", this._loadPaymentShim);

    // We also listen for UI notifications about a closed payment flow. The UI
    // should provide the reason of the closure within the 'errorMsg' parameter
    this._notifyPayFlowClosed = (function _notifyPayFlowClosed(evt) {
      let msg = evt.detail;
      if (msg.id != id) {
        return;
      }

      if (msg.type != 'cancel') {
        return;
      }

      if (msg.errorMsg) {
        _error(msg.errorMsg);
      }
      content.removeEventListener("mozContentEvent",
                                  this._notifyPayFlowClosed);
      this._notifyPayFlowClosed = null;
    }).bind(this);
    content.addEventListener("mozContentEvent",
                             this._notifyPayFlowClosed);

    browser.shell.sendChromeEvent(detail);
  },

  cleanup: function cleanup() {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    let content = browser.getContentWindow();
    if (!content) {
      return;
    }

    if (this._handleSelection) {
      content.removeEventListener("mozContentEvent", this._handleSelection);
      this._handleSelection = null;
    }

    if (this._notifyPayFlowClosed) {
      content.removeEventListener("mozContentEvent", this._notifyPayFlowClosed);
      this._notifyPayFlowClosed = null;
    }

    if (this._loadPaymentShim) {
      content.removeEventListener("mozContentEvent", this._loadPaymentShim);
      this._loadPaymentShim = null;
    }
  },

  getRandomId: function getRandomId() {
    return uuidgen.generateUUID().toString();
  },

  LOG: function LOG(s) {
    if (!this._debug) {
      return;
    }
    dump("-*- PaymentGlue: " + s + "\n");
  },

  classID: Components.ID("{8b83eabc-7929-47f4-8b48-4dea8d887e4b}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIGlue])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentUI]);

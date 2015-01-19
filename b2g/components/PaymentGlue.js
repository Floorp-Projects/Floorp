/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

// Type of MozChromEvents to handle payment dialogs.
const kOpenPaymentConfirmationEvent = "open-payment-confirmation-dialog";
const kOpenPaymentFlowEvent = "open-payment-flow-dialog";
const kClosePaymentFlowEvent = "close-payment-flow-dialog";

// Observer notification topic for payment flow cancelation.
const kPaymentFlowCancelled = "payment-flow-cancelled";

const PREF_DEBUG = "dom.payment.debug";

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

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

      SystemAppProxy.removeEventListener("mozContentEvent", this._handleSelection);
      this._handleSelection = null;
    }).bind(this);
    SystemAppProxy.addEventListener("mozContentEvent", this._handleSelection);

    SystemAppProxy.dispatchEvent(detail);
  },

  showPaymentFlow: function showPaymentFlow(aRequestId,
                                            aPaymentFlowInfo,
                                            aErrorCb) {
    let _error = (errorMsg) => {
      if (aErrorCb) {
        aErrorCb.onresult(aRequestId, errorMsg);
      }
    };

    // We ask the UI to browse to the selected payment flow.
    let id = kOpenPaymentFlowEvent + "-" + this.getRandomId();
    let detail = {
      type: kOpenPaymentFlowEvent,
      id: id,
      requestId: aRequestId
    };

    this._setPaymentRequest = (event) => {
      let message = event.detail;
      if (message.id != id) {
        return;
      }

      let frame = message.frame;
      let docshell = frame.contentWindow
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
      docshell.paymentRequestId = aRequestId;
      frame.src = aPaymentFlowInfo.uri + aPaymentFlowInfo.jwt;
      SystemAppProxy.removeEventListener("mozContentEvent",
                                         this._setPaymentRequest);
    };
    SystemAppProxy.addEventListener("mozContentEvent",
                                    this._setPaymentRequest);

    // We listen for UI notifications about a closed payment flow. The UI
    // should provide the reason of the closure within the 'errorMsg' parameter
    this._notifyPayFlowClosed = (evt) => {
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

      SystemAppProxy.removeEventListener("mozContentEvent",
                                         this._notifyPayFlowClosed);
      this._notifyPayFlowClosed = null;

      Services.obs.notifyObservers(null, kPaymentFlowCancelled, null);
    };
    SystemAppProxy.addEventListener("mozContentEvent",
                                    this._notifyPayFlowClosed);

    SystemAppProxy.dispatchEvent(detail);
  },

  closePaymentFlow: function(aRequestId) {
    return new Promise((aResolve) => {
      // After receiving the payment provider confirmation about the
      // successful or failed payment flow, we notify the UI to close the
      // payment flow dialog and return to the caller application.
      let id = kClosePaymentFlowEvent + "-" + uuidgen.generateUUID().toString();

      let detail = {
        type: kClosePaymentFlowEvent,
        id: id,
        requestId: aRequestId
      };

      // In order to avoid race conditions, we wait for the UI to notify that
      // it has successfully closed the payment flow and has recovered the
      // caller app, before notifying the parent process to fire the success
      // or error event over the DOMRequest.
      SystemAppProxy.addEventListener("mozContentEvent",
                                      (function closePaymentFlowReturn() {
        SystemAppProxy.removeEventListener("mozContentEvent",
                                    closePaymentFlowReturn);
        this.cleanup();
        aResolve();
      }).bind(this));

      SystemAppProxy.dispatchEvent(detail);
    });
  },

  cleanup: function cleanup() {
    if (this._handleSelection) {
      SystemAppProxy.removeEventListener("mozContentEvent",
                                         this._handleSelection);
      this._handleSelection = null;
    }

    if (this._notifyPayFlowClosed) {
      SystemAppProxy.removeEventListener("mozContentEvent",
                                         this._notifyPayFlowClosed);
      this._notifyPayFlowClosed = null;
    }
  },

  getRandomId: function getRandomId() {
    return uuidgen.generateUUID().toString();
  },

  classID: Components.ID("{8b83eabc-7929-47f4-8b48-4dea8d887e4b}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIGlue])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentUI]);

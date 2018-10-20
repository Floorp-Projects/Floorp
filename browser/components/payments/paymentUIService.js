/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Singleton service acting as glue between the DOM APIs and the payment dialog UI.
 *
 * Communication from the DOM to the UI happens via the nsIPaymentUIService interface.
 * The UI talks to the DOM code via the nsIPaymentRequestService interface.
 * PaymentUIService is started by the DOM code lazily.
 *
 * For now the UI is shown in a native dialog but that is likely to change.
 * Tests should try to avoid relying on that implementation detail.
 */

"use strict";

const XHTML_NS = "http://www.w3.org/1999/xhtml";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserWindowTracker",
                               "resource:///modules/BrowserWindowTracker.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "paymentSrv",
                                   "@mozilla.org/dom/payments/payment-request-service;1",
                                   "nsIPaymentRequestService");

function PaymentUIService() {
  this.wrappedJSObject = this;
  XPCOMUtils.defineLazyGetter(this, "log", () => {
    let {ConsoleAPI} = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
    return new ConsoleAPI({
      maxLogLevelPref: "dom.payments.loglevel",
      prefix: "Payment UI Service",
    });
  });
  this.log.debug("constructor");
}

PaymentUIService.prototype = {
  classID: Components.ID("{01f8bd55-9017-438b-85ec-7c15d2b35cdc}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),
  DIALOG_URL: "chrome://payments/content/paymentDialogWrapper.xul",
  REQUEST_ID_PREFIX: "paymentRequest-",

  // nsIPaymentUIService implementation:

  showPayment(requestId) {
    this.log.debug("showPayment:", requestId);
    let request = paymentSrv.getPaymentRequestById(requestId);
    let merchantBrowser = this.findBrowserByTabId(request.tabId);
    let chromeWindow = merchantBrowser.ownerGlobal;
    let {gBrowser} = chromeWindow;
    let browserContainer = gBrowser.getBrowserContainer(merchantBrowser);
    let container = chromeWindow.document.createElementNS(XHTML_NS, "div");
    container.dataset.requestId = requestId;
    container.classList.add("paymentDialogContainer");
    container.hidden = true;
    let paymentsBrowser = chromeWindow.document.createElementNS(XHTML_NS, "iframe");
    paymentsBrowser.classList.add("paymentDialogContainerFrame");
    paymentsBrowser.setAttribute("type", "content");
    paymentsBrowser.setAttribute("remote", "true");
    paymentsBrowser.setAttribute("src", `${this.DIALOG_URL}?requestId=${requestId}`);
    // append the frame to start the loading
    container.appendChild(paymentsBrowser);
    browserContainer.prepend(container);

    // Only show the frame and change the UI when the dialog is ready to show.
    paymentsBrowser.addEventListener("tabmodaldialogready", function readyToShow() {
      if (!container) {
        // The dialog was closed by the DOM code before it was ready to be shown.
        return;
      }
      container.hidden = false;

      // Prevent focusing or interacting with the <browser>.
      merchantBrowser.setAttribute("tabmodalPromptShowing", "true");

      // Darken the merchant content area.
      let tabModalBackground = chromeWindow.document.createElement("box");
      tabModalBackground.classList.add("tabModalBackground", "paymentDialogBackground");
      // Insert the same way as <tabmodalprompt>.
      merchantBrowser.parentNode.insertBefore(tabModalBackground,
                                              merchantBrowser.nextElementSibling);
    }, {
      once: true,
    });
  },

  abortPayment(requestId) {
    this.log.debug("abortPayment:", requestId);
    let abortResponse = Cc["@mozilla.org/dom/payments/payment-abort-action-response;1"]
                          .createInstance(Ci.nsIPaymentAbortActionResponse);
    let found = this.closeDialog(requestId);

    // if `win` is falsy, then we haven't found the dialog, so the abort fails
    // otherwise, the abort is successful
    let response = found ?
      Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED :
      Ci.nsIPaymentActionResponse.ABORT_FAILED;

    abortResponse.init(requestId, response);
    paymentSrv.respondPayment(abortResponse);
  },

  completePayment(requestId) {
    // completeStatus should be one of "timeout", "success", "fail", ""
    let {completeStatus} = paymentSrv.getPaymentRequestById(requestId);
    this.log.debug(`completePayment: requestId: ${requestId}, completeStatus: ${completeStatus}`);

    let closed;
    switch (completeStatus) {
      case "fail":
      case "timeout":
        break;
      default:
        closed = this.closeDialog(requestId);
        break;
    }

    let dialogContainer;
    if (!closed) {
      // We need to call findDialog before we respond below as getPaymentRequestById
      // may fail due to the request being removed upon completion.
      dialogContainer = this.findDialog(requestId).dialogContainer;
      if (!dialogContainer) {
        this.log.error("completePayment: no dialog found");
        return;
      }
    }

    let responseCode = closed ?
        Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED :
        Ci.nsIPaymentActionResponse.COMPLETE_FAILED;
    let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"]
                             .createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(requestId, responseCode);
    paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));

    if (!closed) {
      dialogContainer.querySelector("iframe").contentWindow.paymentDialogWrapper.updateRequest();
    }
  },

  updatePayment(requestId) {
    let {dialogContainer} = this.findDialog(requestId);
    this.log.debug("updatePayment:", requestId);
    if (!dialogContainer) {
      this.log.error("updatePayment: no dialog found");
      return;
    }
    dialogContainer.querySelector("iframe").contentWindow.paymentDialogWrapper.updateRequest();
  },

  closePayment(requestId) {
    this.closeDialog(requestId);
  },

  // other helper methods

  /**
   * @param {string} requestId - Payment Request ID of the dialog to close.
   * @returns {boolean} whether the specified dialog was closed.
   */
  closeDialog(requestId) {
    let {
      browser,
      dialogContainer,
    } = this.findDialog(requestId);
    if (!dialogContainer) {
      return false;
    }
    this.log.debug(`closing: ${requestId}`);
    dialogContainer.remove();
    if (!dialogContainer.hidden) {
      // If the container is no longer hidden then the background was added after
      // `tabmodaldialogready` so remove it.
      browser.parentElement.querySelector(".paymentDialogBackground").remove();

      if (!browser.tabModalPromptBox || browser.tabModalPromptBox.listPrompts().length == 0) {
        browser.removeAttribute("tabmodalPromptShowing");
      }
    }
    return true;
  },

  findDialog(requestId) {
    for (let win of BrowserWindowTracker.orderedWindows) {
      for (let dialogContainer of win.document.querySelectorAll(".paymentDialogContainer")) {
        if (dialogContainer.dataset.requestId == requestId) {
          return {
            dialogContainer,
            browser: dialogContainer.parentElement.querySelector("browser"),
          };
        }
      }
    }
    return {};
  },

  findBrowserByTabId(tabId) {
    for (let win of BrowserWindowTracker.orderedWindows) {
      for (let browser of win.gBrowser.browsers) {
        if (!browser.frameLoader || !browser.frameLoader.tabParent) {
          continue;
        }
        if (browser.frameLoader.tabParent.tabId == tabId) {
          return browser;
        }
      }
    }

    this.log.error("findBrowserByTabId: No browser found for tabId:", tabId);
    return null;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentUIService]);

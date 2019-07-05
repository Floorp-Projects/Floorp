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

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "paymentSrv",
  "@mozilla.org/dom/payments/payment-request-service;1",
  "nsIPaymentRequestService"
);

function PaymentUIService() {
  this.wrappedJSObject = this;
  XPCOMUtils.defineLazyGetter(this, "log", () => {
    let { ConsoleAPI } = ChromeUtils.import(
      "resource://gre/modules/Console.jsm"
    );
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

  // nsIPaymentUIService implementation:

  showPayment(requestId) {
    this.log.debug("showPayment:", requestId);
    let request = paymentSrv.getPaymentRequestById(requestId);
    let merchantBrowser = this.findBrowserByOuterWindowId(
      request.topOuterWindowId
    );
    let chromeWindow = merchantBrowser.ownerGlobal;
    let { gBrowser } = chromeWindow;
    let browserContainer = gBrowser.getBrowserContainer(merchantBrowser);
    let container = chromeWindow.document.createElementNS(XHTML_NS, "div");
    container.dataset.requestId = requestId;
    container.classList.add("paymentDialogContainer");
    container.hidden = true;
    let paymentsBrowser = this._createPaymentFrame(
      chromeWindow.document,
      requestId
    );

    let pdwGlobal = {};
    Services.scriptloader.loadSubScript(
      "chrome://payments/content/paymentDialogWrapper.js",
      pdwGlobal
    );

    paymentsBrowser.paymentDialogWrapper = pdwGlobal.paymentDialogWrapper;

    // Create an <html:div> wrapper to absolutely position the <xul:browser>
    // because XUL elements don't support position:absolute.
    let absDiv = chromeWindow.document.createElementNS(XHTML_NS, "div");
    container.appendChild(absDiv);

    // append the frame to start the loading
    absDiv.appendChild(paymentsBrowser);
    browserContainer.prepend(container);

    // Initialize the wrapper once the <browser> is connected.
    paymentsBrowser.paymentDialogWrapper.init(requestId, paymentsBrowser);

    this._attachBrowserEventListeners(merchantBrowser);

    // Only show the frame and change the UI when the dialog is ready to show.
    paymentsBrowser.addEventListener(
      "tabmodaldialogready",
      function readyToShow() {
        if (!container) {
          // The dialog was closed by the DOM code before it was ready to be shown.
          return;
        }
        container.hidden = false;
        this._showDialog(merchantBrowser);
      }.bind(this),
      {
        once: true,
      }
    );
  },

  abortPayment(requestId) {
    this.log.debug("abortPayment:", requestId);
    let abortResponse = Cc[
      "@mozilla.org/dom/payments/payment-abort-action-response;1"
    ].createInstance(Ci.nsIPaymentAbortActionResponse);
    let found = this.closeDialog(requestId);

    // if `win` is falsy, then we haven't found the dialog, so the abort fails
    // otherwise, the abort is successful
    let response = found
      ? Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED
      : Ci.nsIPaymentActionResponse.ABORT_FAILED;

    abortResponse.init(requestId, response);
    paymentSrv.respondPayment(abortResponse);
  },

  completePayment(requestId) {
    // completeStatus should be one of "timeout", "success", "fail", ""
    let { completeStatus } = paymentSrv.getPaymentRequestById(requestId);
    this.log.debug(
      `completePayment: requestId: ${requestId}, completeStatus: ${completeStatus}`
    );

    let closed;
    switch (completeStatus) {
      case "fail":
      case "timeout":
        break;
      default:
        closed = this.closeDialog(requestId);
        break;
    }

    let paymentFrame;
    if (!closed) {
      // We need to call findDialog before we respond below as getPaymentRequestById
      // may fail due to the request being removed upon completion.
      paymentFrame = this.findDialog(requestId).paymentFrame;
      if (!paymentFrame) {
        this.log.error("completePayment: no dialog found");
        return;
      }
    }

    let responseCode = closed
      ? Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED
      : Ci.nsIPaymentActionResponse.COMPLETE_FAILED;
    let completeResponse = Cc[
      "@mozilla.org/dom/payments/payment-complete-action-response;1"
    ].createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(requestId, responseCode);
    paymentSrv.respondPayment(
      completeResponse.QueryInterface(Ci.nsIPaymentActionResponse)
    );

    if (!closed) {
      paymentFrame.paymentDialogWrapper.updateRequest();
    }
  },

  updatePayment(requestId) {
    let { paymentFrame } = this.findDialog(requestId);
    this.log.debug("updatePayment:", requestId);
    if (!paymentFrame) {
      this.log.error("updatePayment: no dialog found");
      return;
    }
    paymentFrame.paymentDialogWrapper.updateRequest();
  },

  closePayment(requestId) {
    this.closeDialog(requestId);
  },

  // other helper methods

  _createPaymentFrame(doc, requestId) {
    let frame = doc.createXULElement("browser");
    frame.classList.add("paymentDialogContainerFrame");
    frame.setAttribute("type", "content");
    frame.setAttribute("remote", "true");
    frame.setAttribute("disablehistory", "true");
    frame.setAttribute("nodefaultsrc", "true");
    frame.setAttribute("transparent", "true");
    frame.setAttribute("selectmenulist", "ContentSelectDropdown");
    frame.setAttribute("autocompletepopup", "PopupAutoComplete");
    return frame;
  },

  _attachBrowserEventListeners(merchantBrowser) {
    merchantBrowser.addEventListener("SwapDocShells", this);
  },

  _showDialog(merchantBrowser) {
    let chromeWindow = merchantBrowser.ownerGlobal;
    // Prevent focusing or interacting with the <browser>.
    merchantBrowser.setAttribute("tabmodalPromptShowing", "true");

    // Darken the merchant content area.
    let tabModalBackground = chromeWindow.document.createXULElement("box");
    tabModalBackground.classList.add(
      "tabModalBackground",
      "paymentDialogBackground"
    );
    // Insert the same way as <tabmodalprompt>.
    merchantBrowser.parentNode.insertBefore(
      tabModalBackground,
      merchantBrowser.nextElementSibling
    );
  },

  /**
   * @param {string} requestId - Payment Request ID of the dialog to close.
   * @returns {boolean} whether the specified dialog was closed.
   */
  closeDialog(requestId) {
    let { browser, dialogContainer, paymentFrame } = this.findDialog(requestId);
    if (!dialogContainer) {
      return false;
    }
    this.log.debug(`closing: ${requestId}`);
    paymentFrame.paymentDialogWrapper.uninit();
    dialogContainer.remove();
    browser.removeEventListener("SwapDocShells", this);

    if (!dialogContainer.hidden) {
      // If the container is no longer hidden then the background was added after
      // `tabmodaldialogready` so remove it.
      browser.parentElement.querySelector(".paymentDialogBackground").remove();

      if (
        !browser.tabModalPromptBox ||
        browser.tabModalPromptBox.listPrompts().length == 0
      ) {
        browser.removeAttribute("tabmodalPromptShowing");
      }
    }
    return true;
  },

  getDialogContainerForMerchantBrowser(merchantBrowser) {
    return merchantBrowser.ownerGlobal.gBrowser
      .getBrowserContainer(merchantBrowser)
      .querySelector(".paymentDialogContainer");
  },

  findDialog(requestId) {
    for (let win of BrowserWindowTracker.orderedWindows) {
      for (let dialogContainer of win.document.querySelectorAll(
        ".paymentDialogContainer"
      )) {
        if (dialogContainer.dataset.requestId == requestId) {
          return {
            dialogContainer,
            paymentFrame: dialogContainer.querySelector(
              ".paymentDialogContainerFrame"
            ),
            browser: dialogContainer.parentElement.querySelector(
              ".browserStack > browser"
            ),
          };
        }
      }
    }
    return {};
  },

  findBrowserByOuterWindowId(outerWindowId) {
    for (let win of BrowserWindowTracker.orderedWindows) {
      let browser = win.gBrowser.getBrowserForOuterWindowID(outerWindowId);
      if (!browser) {
        continue;
      }
      return browser;
    }

    this.log.error(
      "findBrowserByOuterWindowId: No browser found for outerWindowId:",
      outerWindowId
    );
    return null;
  },

  _moveDialogToNewBrowser(oldBrowser, newBrowser) {
    // Re-attach event listeners to the new browser.
    newBrowser.addEventListener("SwapDocShells", this);

    let dialogContainer = this.getDialogContainerForMerchantBrowser(oldBrowser);
    let newBrowserContainer = newBrowser.ownerGlobal.gBrowser.getBrowserContainer(
      newBrowser
    );

    // Clone the container tree
    let newDialogContainer = newBrowserContainer.ownerDocument.importNode(
      dialogContainer,
      true
    );

    let oldFrame = dialogContainer.querySelector(
      ".paymentDialogContainerFrame"
    );
    let newFrame = newDialogContainer.querySelector(
      ".paymentDialogContainerFrame"
    );

    // We need a document to be synchronously loaded in order to do the swap and
    // there's no point in wasting resources loading a dialog we're going to replace.
    newFrame.setAttribute("src", "about:blank");
    newFrame.setAttribute("nodefaultsrc", "true");

    newBrowserContainer.prepend(newDialogContainer);

    // Force the <browser> to be created so that it'll have a document loaded and frame created.
    // See `ourChildDocument` and `ourFrame` checks in nsFrameLoader::SwapWithOtherLoader.
    /* eslint-disable-next-line no-unused-expressions */
    newFrame.clientTop;

    // Swap the frameLoaders to preserve the frame state
    newFrame.swapFrameLoaders(oldFrame);
    newFrame.paymentDialogWrapper = oldFrame.paymentDialogWrapper;
    newFrame.paymentDialogWrapper.changeAttachedFrame(newFrame);
    dialogContainer.remove();

    this._showDialog(newBrowser);
  },

  handleEvent(event) {
    switch (event.type) {
      case "SwapDocShells": {
        this._moveDialogToNewBrowser(event.target, event.detail);
        break;
      }
    }
  },
};

var EXPORTED_SYMBOLS = ["PaymentUIService"];

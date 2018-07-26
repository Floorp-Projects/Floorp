/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Loaded in the unprivileged frame of each payment dialog.
 *
 * Communicates with privileged code via DOM Events.
 */

/* import-globals-from unprivileged-fallbacks.js */

var paymentRequest = {
  domReadyPromise: null,

  init() {
    // listen to content
    window.addEventListener("paymentChromeToContent", this);

    window.addEventListener("keydown", this);

    this.domReadyPromise = new Promise(function dcl(resolve) {
      window.addEventListener("DOMContentLoaded", resolve, {once: true});
    }).then(this.handleEvent.bind(this));

    // This scope is now ready to listen to the initialization data
    this.sendMessageToChrome("initializeRequest");
  },

  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded": {
        this.onPaymentRequestLoad();
        break;
      }
      case "keydown": {
        if (event.code != "KeyD" || !event.altKey || !event.ctrlKey) {
          break;
        }
        this.toggleDebuggingConsole();
        break;
      }
      case "unload": {
        this.onPaymentRequestUnload();
        break;
      }
      case "paymentChromeToContent": {
        this.onChromeToContent(event);
        break;
      }
      default: {
        throw new Error("Unexpected event type");
      }
    }
  },

  sendMessageToChrome(messageType, detail = {}) {
    log.debug("sendMessageToChrome:", messageType, detail);
    let event = new CustomEvent("paymentContentToChrome", {
      bubbles: true,
      detail: Object.assign({
        messageType,
      }, detail),
    });
    document.dispatchEvent(event);
  },

  toggleDebuggingConsole() {
    let debuggingConsole = document.getElementById("debugging-console");
    if (debuggingConsole.hidden && !debuggingConsole.src) {
      debuggingConsole.src = "debugging.html";
    }
    debuggingConsole.hidden = !debuggingConsole.hidden;
  },

  onChromeToContent({detail}) {
    let {messageType} = detail;
    log.debug("onChromeToContent:", messageType);

    switch (messageType) {
      case "responseSent": {
        document.querySelector("payment-dialog").requestStore.setState({
          changesPrevented: true,
          completionState: "processing",
        });
        break;
      }
      case "showPaymentRequest": {
        this.onShowPaymentRequest(detail);
        break;
      }
      case "updateState": {
        document.querySelector("payment-dialog").setStateFromParent(detail);
        break;
      }
    }
  },

  onPaymentRequestLoad() {
    log.debug("onPaymentRequestLoad");
    window.addEventListener("unload", this, {once: true});
    this.sendMessageToChrome("paymentDialogReady");

    // Automatically show the debugging console if loaded with a truthy `debug` query parameter.
    if (new URLSearchParams(location.search).get("debug")) {
      this.toggleDebuggingConsole();
    }
  },

  async onShowPaymentRequest(detail) {
    // Handle getting called before the DOM is ready.
    log.debug("onShowPaymentRequest:", detail);
    await this.domReadyPromise;

    log.debug("onShowPaymentRequest: domReadyPromise resolved");
    log.debug("onShowPaymentRequest, isPrivate?", detail.isPrivate);

    let paymentDialog = document.querySelector("payment-dialog");
    let hasSavedAddresses = Object.keys(detail.savedAddresses).length != 0;
    let hasSavedCards = Object.keys(detail.savedBasicCards).length != 0;
    let shippingRequested = detail.request.paymentOptions.requestShipping;
    let state = {
      request: detail.request,
      savedAddresses: detail.savedAddresses,
      savedBasicCards: detail.savedBasicCards,
      isPrivate: detail.isPrivate,
      page: {
        id: "payment-summary",
      },
    };

    // Onboarding wizard flow.
    if (!hasSavedAddresses && (shippingRequested || !hasSavedCards)) {
      state.page = {
        id: "address-page",
        onboardingWizard: true,
      };

      state["address-page"] = {
        addressFields: null,
        guid: null,
      };

      if (shippingRequested) {
        Object.assign(state["address-page"], {
          title: paymentDialog.dataset.shippingAddressTitleAdd,
        });
        state.page.selectedStateKey = ["selectedShippingAddress"];
      } else {
        Object.assign(state["address-page"], {
          title: paymentDialog.dataset.billingAddressTitleAdd,
        });
        state.page.selectedStateKey = ["basic-card-page", "billingAddressGUID"];
      }
    } else if (!hasSavedCards) {
      state.page = {
        id: "basic-card-page",
        onboardingWizard: true,
      };
    }

    paymentDialog.setStateFromParent(state);
  },

  cancel() {
    this.sendMessageToChrome("paymentCancel");
  },

  pay(data) {
    this.sendMessageToChrome("pay", data);
  },

  closeDialog() {
    this.sendMessageToChrome("closeDialog");
  },

  changeShippingAddress(data) {
    this.sendMessageToChrome("changeShippingAddress", data);
  },

  changeShippingOption(data) {
    this.sendMessageToChrome("changeShippingOption", data);
  },

  /**
   * Add/update an autofill storage record.
   *
   * If the the `guid` argument is provided update the record; otherwise, add it.
   * @param {string} collectionName The autofill collection that record belongs to.
   * @param {object} record The autofill record to add/update
   * @param {string} [guid] The guid of the autofill record to update
   */
  updateAutofillRecord(collectionName, record, guid, {
    errorStateChange,
    preserveOldProperties,
    selectedStateKey,
    successStateChange,
  }) {
    this.sendMessageToChrome("updateAutofillRecord", {
      collectionName,
      guid,
      record,
      errorStateChange,
      preserveOldProperties,
      selectedStateKey,
      successStateChange,
    });
  },

  /**
   * @param {object} state object representing the UI state
   * @param {string} methodID (GUID) uniquely identifying the selected payment method
   * @returns {object?} the applicable modifier for the payment method
   */
  getModifierForPaymentMethod(state, methodID) {
    let method = state.savedBasicCards[methodID] || null;
    if (method && method.methodName !== "basic-card") {
      throw new Error(`${method.methodName} (${methodID}) is not a supported payment method`);
    }
    let modifiers = state.request.paymentDetails.modifiers;
    if (!modifiers || !modifiers.length) {
      return null;
    }
    let modifier = modifiers.find(m => {
      // take the first matching modifier
      // TODO (bug 1429198): match on supportedTypes and supportedNetworks
      return m.supportedMethods == "basic-card";
    });
    return modifier || null;
  },

  /**
   * @param {object} state object representing the UI state
   * @returns {object} in the shape of `nsIPaymentItem` representing the total
   *                   that applies to the selected payment method.
   */
  getTotalItem(state) {
    let methodID = state.selectedPaymentCard;
    if (methodID) {
      let modifier = paymentRequest.getModifierForPaymentMethod(state, methodID);
      if (modifier && modifier.hasOwnProperty("total")) {
        return modifier.total;
      }
    }
    return state.request.paymentDetails.totalItem;
  },

  onPaymentRequestUnload() {
    // remove listeners that may be used multiple times here
    window.removeEventListener("paymentChromeToContent", this);
  },

  getAddresses(state) {
    let addresses = Object.assign({}, state.savedAddresses, state.tempAddresses);
    return addresses;
  },

  getBasicCards(state) {
    let cards = Object.assign({}, state.savedBasicCards, state.tempBasicCards);
    return cards;
  },
};

paymentRequest.init();

export default paymentRequest;

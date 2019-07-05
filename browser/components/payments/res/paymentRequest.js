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
  _nextMessageID: 1,
  domReadyPromise: null,

  init() {
    // listen to content
    window.addEventListener("paymentChromeToContent", this);

    window.addEventListener("keydown", this);

    this.domReadyPromise = new Promise(function dcl(resolve) {
      window.addEventListener("DOMContentLoaded", resolve, { once: true });
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

  /**
   * @param {string} messageType
   * @param {[object]} detail
   * @returns {number} message ID to be able to identify a reply (where applicable).
   */
  sendMessageToChrome(messageType, detail = {}) {
    let messageID = this._nextMessageID++;
    log.debug("sendMessageToChrome:", messageType, messageID, detail);
    let event = new CustomEvent("paymentContentToChrome", {
      bubbles: true,
      detail: Object.assign(
        {
          messageType,
          messageID,
        },
        detail
      ),
    });
    document.dispatchEvent(event);
    return messageID;
  },

  toggleDebuggingConsole() {
    let debuggingConsole = document.getElementById("debugging-console");
    if (debuggingConsole.hidden && !debuggingConsole.src) {
      debuggingConsole.src = "debugging.html";
    }
    debuggingConsole.hidden = !debuggingConsole.hidden;
  },

  onChromeToContent({ detail }) {
    let { messageType } = detail;
    log.debug("onChromeToContent:", messageType);

    switch (messageType) {
      case "responseSent": {
        let { request } = document
          .querySelector("payment-dialog")
          .requestStore.getState();
        document.querySelector("payment-dialog").requestStore.setState({
          changesPrevented: true,
          request: Object.assign({}, request, { completeStatus: "processing" }),
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
    window.addEventListener("unload", this, { once: true });

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
    let state = {
      request: detail.request,
      savedAddresses: detail.savedAddresses,
      savedBasicCards: detail.savedBasicCards,
      // Temp records can exist upon a reload during development.
      tempAddresses: detail.tempAddresses,
      tempBasicCards: detail.tempBasicCards,
      isPrivate: detail.isPrivate,
      page: {
        id: "payment-summary",
      },
    };

    let hasSavedAddresses = Object.keys(this.getAddresses(state)).length != 0;
    let hasSavedCards = Object.keys(this.getBasicCards(state)).length != 0;
    let shippingRequested = state.request.paymentOptions.requestShipping;

    // Onboarding wizard flow.
    if (!hasSavedAddresses && shippingRequested) {
      state.page = {
        id: "shipping-address-page",
        onboardingWizard: true,
      };

      state["shipping-address-page"] = {
        guid: null,
      };
    } else if (!hasSavedAddresses && !hasSavedCards) {
      state.page = {
        id: "billing-address-page",
        onboardingWizard: true,
      };

      state["billing-address-page"] = {
        guid: null,
      };
    } else if (!hasSavedCards) {
      state.page = {
        id: "basic-card-page",
        onboardingWizard: true,
      };
      state["basic-card-page"] = {
        selectedStateKey: "selectedPaymentCard",
      };
    }

    await paymentDialog.setStateFromParent(state);

    this.sendMessageToChrome("paymentDialogReady");
  },

  openPreferences() {
    this.sendMessageToChrome("openPreferences");
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

  changePaymentMethod(data) {
    this.sendMessageToChrome("changePaymentMethod", data);
  },

  changeShippingAddress(data) {
    this.sendMessageToChrome("changeShippingAddress", data);
  },

  changeShippingOption(data) {
    this.sendMessageToChrome("changeShippingOption", data);
  },

  changePayerAddress(data) {
    this.sendMessageToChrome("changePayerAddress", data);
  },

  /**
   * Add/update an autofill storage record.
   *
   * If the the `guid` argument is provided update the record; otherwise, add it.
   * @param {string} collectionName The autofill collection that record belongs to.
   * @param {object} record The autofill record to add/update
   * @param {string} [guid] The guid of the autofill record to update
   * @returns {Promise} when the update response is received
   */
  updateAutofillRecord(collectionName, record, guid) {
    return new Promise((resolve, reject) => {
      let messageID = this.sendMessageToChrome("updateAutofillRecord", {
        collectionName,
        guid,
        record,
      });

      window.addEventListener("paymentChromeToContent", function onMsg({
        detail,
      }) {
        if (
          detail.messageType != "updateAutofillRecord:Response" ||
          detail.messageID != messageID
        ) {
          return;
        }
        log.debug("updateAutofillRecord: response:", detail);
        window.removeEventListener("paymentChromeToContent", onMsg);
        document
          .querySelector("payment-dialog")
          .setStateFromParent(detail.stateChange);
        if (detail.error) {
          reject(detail);
        } else {
          resolve(detail);
        }
      });
    });
  },

  /**
   * @param {object} state object representing the UI state
   * @param {string} selectedMethodID (GUID) uniquely identifying the selected payment method
   * @returns {object?} the applicable modifier for the payment method
   */
  getModifierForPaymentMethod(state, selectedMethodID) {
    let basicCards = this.getBasicCards(state);
    let selectedMethod = basicCards[selectedMethodID] || null;
    if (selectedMethod && selectedMethod.methodName !== "basic-card") {
      throw new Error(
        `${selectedMethod.methodName} (${selectedMethodID}) ` +
          `is not a supported payment method`
      );
    }
    let modifiers = state.request.paymentDetails.modifiers;
    if (!selectedMethod || !modifiers || !modifiers.length) {
      return null;
    }
    let appliedModifier = modifiers.find(modifier => {
      // take the first matching modifier
      if (
        modifier.supportedMethods &&
        modifier.supportedMethods != selectedMethod.methodName
      ) {
        return false;
      }
      let supportedNetworks =
        (modifier.data && modifier.data.supportedNetworks) || [];
      return (
        supportedNetworks.length == 0 ||
        supportedNetworks.includes(selectedMethod["cc-type"])
      );
    });
    return appliedModifier || null;
  },

  /**
   * @param {object} state object representing the UI state
   * @returns {object} in the shape of `nsIPaymentItem` representing the total
   *                   that applies to the selected payment method.
   */
  getTotalItem(state) {
    let methodID = state.selectedPaymentCard;
    if (methodID) {
      let modifier = paymentRequest.getModifierForPaymentMethod(
        state,
        methodID
      );
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

  _sortObjectsByTimeLastUsed(objects) {
    let sortedValues = Object.values(objects).sort((a, b) => {
      let aLastUsed = a.timeLastUsed || a.timeLastModified;
      let bLastUsed = b.timeLastUsed || b.timeLastModified;
      return bLastUsed - aLastUsed;
    });
    let sortedObjects = {};
    for (let obj of sortedValues) {
      sortedObjects[obj.guid] = obj;
    }
    return sortedObjects;
  },

  getAddresses(state) {
    let addresses = Object.assign(
      {},
      state.savedAddresses,
      state.tempAddresses
    );
    return this._sortObjectsByTimeLastUsed(addresses);
  },

  getBasicCards(state) {
    let cards = Object.assign({}, state.savedBasicCards, state.tempBasicCards);
    return this._sortObjectsByTimeLastUsed(cards);
  },

  maybeCreateFieldErrorElement(container) {
    let span = container.querySelector(".error-text");
    if (!span) {
      span = document.createElement("span");
      span.className = "error-text";
      container.appendChild(span);
    }
    return span;
  },
};

paymentRequest.init();

export default paymentRequest;

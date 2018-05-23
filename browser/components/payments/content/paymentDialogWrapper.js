/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Runs in the privileged outer dialog. Each dialog loads this script in its
 * own scope.
 */

"use strict";

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"]
                     .getService(Ci.nsIPaymentRequestService);

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "MasterPassword",
                               "resource://formautofill/MasterPassword.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "formAutofillStorage", () => {
  let storage;
  try {
    storage = ChromeUtils.import("resource://formautofill/FormAutofillStorage.jsm", {})
                         .formAutofillStorage;
    storage.initialize();
  } catch (ex) {
    storage = null;
    Cu.reportError(ex);
  }

  return storage;
});

class TempCollection {
  constructor(data = {}) {
    this._data = data;
  }
  get(guid) {
    return this._data[guid];
  }
  update(guid, record, preserveOldProperties) {
    if (preserveOldProperties) {
      Object.assign(this._data[guid], record);
    } else {
      this._data[guid] = record;
    }
    return this._data[guid];
  }
  add(record) {
    let guid = "temp-" + Math.abs(Math.random() * 0xffffffff|0);
    this._data[guid] = record;
    return guid;
  }
  getAll() {
    return this._data;
  }
}

var paymentDialogWrapper = {
  componentsLoaded: new Map(),
  frame: null,
  mm: null,
  request: null,
  temporaryStore: null,

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),

  /**
   * Note: This method is async because formAutofillStorage plans to become async.
   *
   * @param {string} guid
   * @returns {object} containing only the requested payer values.
   */
  async _convertProfileAddressToPayerData(guid) {
    let addressData = this.temporaryStore.addresses.get(guid) ||
                      formAutofillStorage.addresses.get(guid);
    if (!addressData) {
      throw new Error(`Payer address not found: ${guid}`);
    }

    let {
      requestPayerName,
      requestPayerEmail,
      requestPayerPhone,
    } = this.request.paymentOptions;

    let payerData = {
      payerName: requestPayerName ? addressData.name : "",
      payerEmail: requestPayerEmail ? addressData.email : "",
      payerPhone: requestPayerPhone ? addressData.tel : "",
    };

    return payerData;
  },

  /**
   * Note: This method is async because formAutofillStorage plans to become async.
   *
   * @param {string} guid
   * @returns {nsIPaymentAddress}
   */
  async _convertProfileAddressToPaymentAddress(guid) {
    let addressData = this.temporaryStore.addresses.get(guid) ||
                      formAutofillStorage.addresses.get(guid);
    if (!addressData) {
      throw new Error(`Shipping address not found: ${guid}`);
    }

    let address = this.createPaymentAddress({
      country: addressData.country,
      addressLines: addressData["street-address"].split("\n"),
      region: addressData["address-level1"],
      city: addressData["address-level2"],
      postalCode: addressData["postal-code"],
      organization: addressData.organization,
      recipient: addressData.name,
      phone: addressData.tel,
    });

    return address;
  },

  /**
   * @param {string} guid The GUID of the basic card record from storage.
   * @param {string} cardSecurityCode The associated card security code (CVV/CCV/etc.)
   * @throws if the user cancels entering their master password or an error decrypting
   * @returns {nsIBasicCardResponseData?} returns response data or null (if the
   *                                      master password dialog was cancelled);
   */
  async _convertProfileBasicCardToPaymentMethodData(guid, cardSecurityCode) {
    let cardData = this.temporaryStore.creditCards.get(guid) ||
                   formAutofillStorage.creditCards.get(guid);
    if (!cardData) {
      throw new Error(`Basic card not found in storage: ${guid}`);
    }

    let cardNumber;
    if (cardData.isTemporary) {
      cardNumber = cardData["cc-number"];
    } else {
      try {
        cardNumber = await MasterPassword.decrypt(cardData["cc-number-encrypted"], true);
      } catch (ex) {
        if (ex.result != Cr.NS_ERROR_ABORT) {
          throw ex;
        }
        // User canceled master password entry
        return null;
      }
    }

    let billingAddressGUID = cardData.billingAddressGUID;
    let billingAddress;
    try {
      billingAddress = await this._convertProfileAddressToPaymentAddress(billingAddressGUID);
    } catch (ex) {
      // The referenced address may not exist if it was deleted or hasn't yet synced to this profile
      Cu.reportError(ex);
    }
    let methodData = this.createBasicCardResponseData({
      cardholderName: cardData["cc-name"],
      cardNumber,
      expiryMonth: cardData["cc-exp-month"].toString().padStart(2, "0"),
      expiryYear: cardData["cc-exp-year"].toString(),
      cardSecurityCode,
      billingAddress,
    });

    return methodData;
  },

  init(requestId, frame) {
    if (!requestId || typeof(requestId) != "string") {
      throw new Error("Invalid PaymentRequest ID");
    }

    // The Request object returned by the Payment Service is live and
    // will automatically get updated if event.updateWith is used.
    this.request = paymentSrv.getPaymentRequestById(requestId);

    if (!this.request) {
      throw new Error(`PaymentRequest not found: ${requestId}`);
    }

    this.frame = frame;
    this.mm = frame.frameLoader.messageManager;
    this.mm.addMessageListener("paymentContentToChrome", this);
    this.mm.loadFrameScript("chrome://payments/content/paymentDialogFrameScript.js", true);
    if (AppConstants.platform == "win") {
      this.frame.setAttribute("selectmenulist", "ContentSelectDropdown-windows");
    }
    this.frame.loadURI("resource://payments/paymentRequest.xhtml");

    this.temporaryStore = {
      addresses: new TempCollection(),
      creditCards: new TempCollection(),
    };
  },

  createShowResponse({
    acceptStatus,
    methodName = "",
    methodData = null,
    payerName = "",
    payerEmail = "",
    payerPhone = "",
  }) {
    let showResponse = this.createComponentInstance(Ci.nsIPaymentShowActionResponse);

    showResponse.init(this.request.requestId,
                      acceptStatus,
                      methodName,
                      methodData,
                      payerName,
                      payerEmail,
                      payerPhone);
    return showResponse;
  },

  createBasicCardResponseData({
    cardholderName = "",
    cardNumber,
    expiryMonth = "",
    expiryYear = "",
    cardSecurityCode = "",
    billingAddress = null,
  }) {
    const basicCardResponseData = Cc["@mozilla.org/dom/payments/basiccard-response-data;1"]
                                  .createInstance(Ci.nsIBasicCardResponseData);
    basicCardResponseData.initData(cardholderName,
                                   cardNumber,
                                   expiryMonth,
                                   expiryYear,
                                   cardSecurityCode,
                                   billingAddress);
    return basicCardResponseData;
  },

  createPaymentAddress({
    country = "",
    addressLines = [],
    region = "",
    city = "",
    dependentLocality = "",
    postalCode = "",
    sortingCode = "",
    languageCode = "",
    organization = "",
    recipient = "",
    phone = "",
  }) {
    const paymentAddress = Cc["@mozilla.org/dom/payments/payment-address;1"]
                           .createInstance(Ci.nsIPaymentAddress);
    const addressLine = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    for (let line of addressLines) {
      const address = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
      address.data = line;
      addressLine.appendElement(address);
    }
    paymentAddress.init(country,
                        addressLine,
                        region,
                        city,
                        dependentLocality,
                        postalCode,
                        sortingCode,
                        languageCode,
                        organization,
                        recipient,
                        phone);
    return paymentAddress;
  },

  createComponentInstance(componentInterface) {
    let componentName;
    switch (componentInterface) {
      case Ci.nsIPaymentShowActionResponse: {
        componentName = "@mozilla.org/dom/payments/payment-show-action-response;1";
        break;
      }
      case Ci.nsIGeneralResponseData: {
        componentName = "@mozilla.org/dom/payments/general-response-data;1";
        break;
      }
    }
    let component = this.componentsLoaded.get(componentName);

    if (!component) {
      component = Cc[componentName];
      this.componentsLoaded.set(componentName, component);
    }

    return component.createInstance(componentInterface);
  },

  fetchSavedAddresses() {
    let savedAddresses = {};
    for (let address of formAutofillStorage.addresses.getAll()) {
      savedAddresses[address.guid] = address;
    }
    return savedAddresses;
  },

  fetchSavedPaymentCards() {
    let savedBasicCards = {};
    for (let card of formAutofillStorage.creditCards.getAll()) {
      savedBasicCards[card.guid] = card;
      // Filter out the encrypted card number since the dialog content is
      // considered untrusted and runs in a content process.
      delete card["cc-number-encrypted"];

      // ensure each card has a methodName property
      if (!card.methodName) {
        card.methodName = "basic-card";
      }
    }
    return savedBasicCards;
  },

  onAutofillStorageChange() {
    this.sendMessageToContent("updateState", {
      savedAddresses: this.fetchSavedAddresses(),
      savedBasicCards: this.fetchSavedPaymentCards(),
    });
  },

  sendMessageToContent(messageType, data = {}) {
    this.mm.sendAsyncMessage("paymentChromeToContent", {
      data,
      messageType,
    });
  },

  updateRequest() {
    // There is no need to update this.request since the object is live
    // and will automatically get updated if event.updateWith is used.
    let requestSerialized = this._serializeRequest(this.request);

    this.sendMessageToContent("updateState", {
      request: requestSerialized,
    });
  },

  /**
   * Recursively convert and filter input to the subset of data types supported by JSON
   *
   * @param {*} value - any type of input to serialize
   * @param {string?} name - name or key associated with this input.
   *                         E.g. property name or array index.
   * @returns {*} serialized deep copy of the value
   */
  _serializeRequest(value, name = null) {
    // Primitives: String, Number, Boolean, null
    let type = typeof value;
    if (value === null ||
        type == "string" ||
        type == "number" ||
        type == "boolean") {
      return value;
    }
    if (name == "topLevelPrincipal") {
      // Manually serialize the nsIPrincipal.
      let displayHost = value.URI.displayHost;
      return {
        URI: {
          displayHost,
        },
      };
    }
    if (type == "function" || type == "undefined") {
      return undefined;
    }
    // Structures: nsIArray
    if (value instanceof Ci.nsIArray) {
      let iface;
      let items = [];
      switch (name) {
        case "displayItems": // falls through
        case "additionalDisplayItems":
          iface = Ci.nsIPaymentItem;
          break;
        case "shippingOptions":
          iface = Ci.nsIPaymentShippingOption;
          break;
        case "paymentMethods":
          iface = Ci.nsIPaymentMethodData;
          break;
        case "modifiers":
          iface = Ci.nsIPaymentDetailsModifier;
          break;
      }
      if (!iface) {
        throw new Error(`No interface associated with the members of the ${name} nsIArray`);
      }
      for (let i = 0; i < value.length; i++) {
        let item = value.queryElementAt(i, iface);
        let result = this._serializeRequest(item, i);
        if (result !== undefined) {
          items.push(result);
        }
      }
      return items;
    }
    // Structures: Arrays
    if (Array.isArray(value)) {
      let items = value.map(item => { this._serializeRequest(item); })
                       .filter(item => item !== undefined);
      return items;
    }
    // Structures: Objects
    let obj = {};
    for (let [key, item] of Object.entries(value)) {
      let result = this._serializeRequest(item, key);
      if (result !== undefined) {
        obj[key] = result;
      }
    }
    return obj;
  },

  initializeFrame() {
    let requestSerialized = this._serializeRequest(this.request);
    let chromeWindow = Services.wm.getMostRecentWindow("navigator:browser");
    let isPrivate = PrivateBrowsingUtils.isWindowPrivate(chromeWindow);

    this.sendMessageToContent("showPaymentRequest", {
      request: requestSerialized,
      savedAddresses: this.fetchSavedAddresses(),
      tempAddresses: this.temporaryStore.addresses.getAll(),
      savedBasicCards: this.fetchSavedPaymentCards(),
      tempBasicCards: this.temporaryStore.creditCards.getAll(),
      isPrivate,
    });

    Services.obs.addObserver(this, "formautofill-storage-changed", true);
  },

  debugFrame() {
    // To avoid self-XSS-type attacks, ensure that Browser Chrome debugging is enabled.
    if (!Services.prefs.getBoolPref("devtools.chrome.enabled", false)) {
      Cu.reportError("devtools.chrome.enabled must be enabled to debug the frame");
      return;
    }
    let chromeWindow = Services.wm.getMostRecentWindow(null);
    let {
      gDevToolsBrowser,
    } = ChromeUtils.import("resource://devtools/client/framework/gDevTools.jsm", {});
    gDevToolsBrowser.openContentProcessToolbox({
      selectedBrowser: chromeWindow.document.getElementById("paymentRequestFrame").frameLoader,
    });
  },

  onPaymentCancel() {
    const showResponse = this.createShowResponse({
      acceptStatus: Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
    });
    paymentSrv.respondPayment(showResponse);
    window.close();
  },

  async onPay({
    selectedPayerAddressGUID: payerGUID,
    selectedPaymentCardGUID: paymentCardGUID,
    selectedPaymentCardSecurityCode: cardSecurityCode,
  }) {
    let methodData = await this._convertProfileBasicCardToPaymentMethodData(paymentCardGUID,
                                                                            cardSecurityCode);

    if (!methodData) {
      // TODO (Bug 1429265/Bug 1429205): Handle when a user hits cancel on the
      // Master Password dialog.
      Cu.reportError("Bug 1429265/Bug 1429205: User canceled master password entry");
      return;
    }

    let {
      payerName,
      payerEmail,
      payerPhone,
    } = await this._convertProfileAddressToPayerData(payerGUID);

    this.pay({
      methodName: "basic-card",
      methodData,
      payerName,
      payerEmail,
      payerPhone,
    });
  },

  pay({
    payerName,
    payerEmail,
    payerPhone,
    methodName,
    methodData,
  }) {
    const showResponse = this.createShowResponse({
      acceptStatus: Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      payerName,
      payerEmail,
      payerPhone,
      methodName,
      methodData,
    });
    paymentSrv.respondPayment(showResponse);
    this.sendMessageToContent("responseSent");
  },

  async onChangeShippingAddress({shippingAddressGUID}) {
    if (shippingAddressGUID) {
      // If a shipping address was de-selected e.g. the selected address was deleted, we'll
      // just wait to send the address change when the shipping address is eventually selected
      // before clicking Pay since it's a required field.
      let address = await this._convertProfileAddressToPaymentAddress(shippingAddressGUID);
      paymentSrv.changeShippingAddress(this.request.requestId, address);
    }
  },

  onChangeShippingOption({optionID}) {
    // Note, failing here on browser_host_name.js because the test closes
    // the dialog before the onChangeShippingOption is called, thus
    // deleting the request and making the requestId invalid. Unclear
    // why we aren't seeing the same issue with onChangeShippingAddress.
    paymentSrv.changeShippingOption(this.request.requestId, optionID);
  },

  async onUpdateAutofillRecord(collectionName, record, guid, {
    errorStateChange,
    preserveOldProperties,
    selectedStateKey,
    successStateChange,
  }) {
    if (collectionName == "creditCards" && !guid && !record.isTemporary) {
      // We need to be logged in so we can encrypt the credit card number and
      // that's only supported when we're adding a new record.
      // TODO: "MasterPassword.ensureLoggedIn" can be removed after the storage
      // APIs are refactored to be async functions (bug 1399367).
      if (!await MasterPassword.ensureLoggedIn()) {
        Cu.reportError("User canceled master password entry");
        return;
      }
    }

    let isTemporary = record.isTemporary;
    let collection = isTemporary ? this.temporaryStore[collectionName] :
                                   formAutofillStorage[collectionName];

    try {
      if (guid) {
        await collection.update(guid, record, preserveOldProperties);
      } else {
        guid = await collection.add(record);
      }

      if (isTemporary && collectionName == "addresses") {
        // there will be no formautofill-storage-changed event to update state
        // so add updated collection here
        Object.assign(successStateChange, {
          tempAddresses: this.temporaryStore.addresses.getAll(),
        });
      }
      if (isTemporary && collectionName == "creditCards") {
        // there will be no formautofill-storage-changed event to update state
        // so add updated collection here
        Object.assign(successStateChange, {
          tempBasicCards: this.temporaryStore.creditCards.getAll(),
        });
      }

      // Select the new record
      if (selectedStateKey) {
        if (selectedStateKey.length == 1) {
          Object.assign(successStateChange, {
            [selectedStateKey[0]]: guid,
          });
        } else if (selectedStateKey.length == 2) {
          // Need to keep properties like preserveFieldValues from getting removed.
          let subObj = Object.assign({}, successStateChange[selectedStateKey[0]]);
          subObj[selectedStateKey[1]] = guid;
          Object.assign(successStateChange, {
            [selectedStateKey[0]]: subObj,
          });
        } else {
          throw new Error(`selectedStateKey not supported: '${selectedStateKey}'`);
        }
      }

      this.sendMessageToContent("updateState", successStateChange);
    } catch (ex) {
      this.sendMessageToContent("updateState", errorStateChange);
    }
  },

  /**
   * @implements {nsIObserver}
   * @param {nsISupports} subject
   * @param {string} topic
   * @param {string} data
   */
  observe(subject, topic, data) {
    switch (topic) {
      case "formautofill-storage-changed": {
        if (data == "notifyUsed") {
          break;
        }
        this.onAutofillStorageChange();
        break;
      }
    }
  },

  receiveMessage({data}) {
    let {messageType} = data;

    switch (messageType) {
      case "debugFrame": {
        this.debugFrame();
        break;
      }
      case "initializeRequest": {
        this.initializeFrame();
        break;
      }
      case "changeShippingAddress": {
        this.onChangeShippingAddress(data);
        break;
      }
      case "changeShippingOption": {
        this.onChangeShippingOption(data);
        break;
      }
      case "paymentCancel": {
        this.onPaymentCancel();
        break;
      }
      case "pay": {
        this.onPay(data);
        break;
      }
      case "updateAutofillRecord": {
        this.onUpdateAutofillRecord(data.collectionName, data.record, data.guid, {
          errorStateChange: data.errorStateChange,
          preserveOldProperties: data.preserveOldProperties,
          selectedStateKey: data.selectedStateKey,
          successStateChange: data.successStateChange,
        });
        break;
      }
    }
  },
};

if ("document" in this) {
  // Running in a browser, not a unit test
  let frame = document.getElementById("paymentRequestFrame");
  let requestId = (new URLSearchParams(window.location.search)).get("requestId");
  paymentDialogWrapper.init(requestId, frame);
}

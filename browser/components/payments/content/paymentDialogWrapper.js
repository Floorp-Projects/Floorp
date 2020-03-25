/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Runs in the privileged outer dialog. Each dialog loads this script in its
 * own scope.
 */

/* exported paymentDialogWrapper */

"use strict";

const paymentSrv = Cc[
  "@mozilla.org/dom/payments/payment-request-service;1"
].getService(Ci.nsIPaymentRequestService);

const paymentUISrv = Cc[
  "@mozilla.org/dom/payments/payment-ui-service;1"
].getService(Ci.nsIPaymentUIService);

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillUtils",
  "resource://formautofill/FormAutofillUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "OSKeyStore",
  "resource://gre/modules/OSKeyStore.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "formAutofillStorage", () => {
  let storage;
  try {
    storage = ChromeUtils.import(
      "resource://formautofill/FormAutofillStorage.jsm",
      {}
    ).formAutofillStorage;
    storage.initialize();
  } catch (ex) {
    storage = null;
    Cu.reportError(ex);
  }

  return storage;
});

XPCOMUtils.defineLazyGetter(this, "reauthPasswordPromptMessage", () => {
  const brandShortName = FormAutofillUtils.brandBundle.GetStringFromName(
    "brandShortName"
  );
  return FormAutofillUtils.stringBundle.formatStringFromName(
    `useCreditCardPasswordPrompt.${AppConstants.platform}`,
    [brandShortName]
  );
});

/**
 * Temporary/transient storage for address and credit card records
 *
 * Implements a subset of the FormAutofillStorage collection class interface, and delegates to
 * those classes for some utility methods
 */
class TempCollection {
  constructor(type, data = {}) {
    /**
     * The name of the collection. e.g. 'addresses' or 'creditCards'
     * Used to access methods from the FormAutofillStorage collections
     */
    this._type = type;
    this._data = data;
  }

  get _formAutofillCollection() {
    // lazy getter for the formAutofill collection - to resolve on first access
    Object.defineProperty(this, "_formAutofillCollection", {
      value: formAutofillStorage[this._type],
      writable: false,
      configurable: true,
    });
    return this._formAutofillCollection;
  }

  get(guid) {
    return this._data[guid];
  }

  async update(guid, record, preserveOldProperties) {
    let recordToSave = Object.assign(
      preserveOldProperties ? this._data[guid] : {},
      record
    );
    await this._formAutofillCollection.computeFields(recordToSave);
    return (this._data[guid] = recordToSave);
  }

  async add(record) {
    let guid = "temp-" + Math.abs((Math.random() * 0xffffffff) | 0);
    let timeLastModified = Date.now();
    let recordToSave = Object.assign({ guid, timeLastModified }, record);
    await this._formAutofillCollection.computeFields(recordToSave);
    this._data[guid] = recordToSave;
    return guid;
  }

  getAll() {
    return this._data;
  }
}

var paymentDialogWrapper = {
  componentsLoaded: new Map(),
  frameWeakRef: null,
  mm: null,
  request: null,
  temporaryStore: null,

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),

  /**
   * @param {string} guid
   * @returns {object} containing only the requested payer values.
   */
  async _convertProfileAddressToPayerData(guid) {
    let addressData =
      this.temporaryStore.addresses.get(guid) ||
      (await formAutofillStorage.addresses.get(guid));
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
   * @param {string} guid
   * @returns {nsIPaymentAddress}
   */
  async _convertProfileAddressToPaymentAddress(guid) {
    let addressData =
      this.temporaryStore.addresses.get(guid) ||
      (await formAutofillStorage.addresses.get(guid));
    if (!addressData) {
      throw new Error(`Address not found: ${guid}`);
    }

    let address = this.createPaymentAddress({
      addressLines: addressData["street-address"].split("\n"),
      city: addressData["address-level2"],
      country: addressData.country,
      dependentLocality: addressData["address-level3"],
      organization: addressData.organization,
      phone: addressData.tel,
      postalCode: addressData["postal-code"],
      recipient: addressData.name,
      region: addressData["address-level1"],
      // TODO (bug 1474905), The regionCode will be available when bug 1474905 is fixed
      // and the region text box is changed to a dropdown with the regionCode being the
      // value of the option and the region being the label for the option.
      // A regionCode should be either the empty string or one to three code points
      // that represent a region as the code element of an [ISO3166-2] country subdivision
      // name (i.e., the characters after the hyphen in an ISO3166-2 country subdivision
      // code element, such as "CA" for the state of California in the USA, or "11" for
      // the Lisbon district of Portugal).
      regionCode: "",
    });

    return address;
  },

  /**
   * @param {string} guid The GUID of the basic card record from storage.
   * @param {string} cardSecurityCode The associated card security code (CVV/CCV/etc.)
   * @throws If there is an error decrypting
   * @returns {nsIBasicCardResponseData?} returns response data or null (if the
   *                                      master password dialog was cancelled);
   */
  async _convertProfileBasicCardToPaymentMethodData(guid, cardSecurityCode) {
    let cardData =
      this.temporaryStore.creditCards.get(guid) ||
      (await formAutofillStorage.creditCards.get(guid));
    if (!cardData) {
      throw new Error(`Basic card not found in storage: ${guid}`);
    }

    let cardNumber;
    try {
      cardNumber = await OSKeyStore.decrypt(
        cardData["cc-number-encrypted"],
        reauthPasswordPromptMessage
      );
    } catch (ex) {
      if (ex.result != Cr.NS_ERROR_ABORT) {
        throw ex;
      }
      // User canceled master password entry
      return null;
    }

    let billingAddressGUID = cardData.billingAddressGUID;
    let billingAddress;
    try {
      billingAddress = await this._convertProfileAddressToPaymentAddress(
        billingAddressGUID
      );
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
    if (!requestId || typeof requestId != "string") {
      throw new Error("Invalid PaymentRequest ID");
    }

    // The Request object returned by the Payment Service is live and
    // will automatically get updated if event.updateWith is used.
    this.request = paymentSrv.getPaymentRequestById(requestId);

    if (!this.request) {
      throw new Error(`PaymentRequest not found: ${requestId}`);
    }

    this._attachToFrame(frame);
    this.mm.loadFrameScript(
      "chrome://payments/content/paymentDialogFrameScript.js",
      true
    );
    // Until we have bug 1446164 and bug 1407418 we use form autofill's temporary
    // shim for data-localization* attributes.
    this.mm.loadFrameScript("chrome://formautofill/content/l10n.js", true);
    frame.setAttribute("src", "resource://payments/paymentRequest.xhtml");

    this.temporaryStore = {
      addresses: new TempCollection("addresses"),
      creditCards: new TempCollection("creditCards"),
    };
  },

  uninit() {
    try {
      Services.obs.removeObserver(this, "message-manager-close");
      Services.obs.removeObserver(this, "formautofill-storage-changed");
    } catch (ex) {
      // Observers may not have been added yet
    }
  },

  /**
   * Code here will be re-run at various times, e.g. initial show and
   * when a tab is detached to a different window.
   *
   * Code that should only run once belongs in `init`.
   * Code to only run upon detaching should be in `changeAttachedFrame`.
   *
   * @param {Element} frame
   */
  _attachToFrame(frame) {
    this.frameWeakRef = Cu.getWeakReference(frame);
    this.mm = frame.frameLoader.messageManager;
    this.mm.addMessageListener("paymentContentToChrome", this);
    Services.obs.addObserver(this, "message-manager-close", true);
  },

  /**
   * Called only when a frame is changed from one to another.
   *
   * @param {Element} frame
   */
  changeAttachedFrame(frame) {
    this.mm.removeMessageListener("paymentContentToChrome", this);
    this._attachToFrame(frame);
    // This isn't in `attachToFrame` because we only want to do it once we've sent records.
    Services.obs.addObserver(this, "formautofill-storage-changed", true);
  },

  createShowResponse({
    acceptStatus,
    methodName = "",
    methodData = null,
    payerName = "",
    payerEmail = "",
    payerPhone = "",
  }) {
    let showResponse = this.createComponentInstance(
      Ci.nsIPaymentShowActionResponse
    );

    showResponse.init(
      this.request.requestId,
      acceptStatus,
      methodName,
      methodData,
      payerName,
      payerEmail,
      payerPhone
    );
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
    const basicCardResponseData = Cc[
      "@mozilla.org/dom/payments/basiccard-response-data;1"
    ].createInstance(Ci.nsIBasicCardResponseData);
    basicCardResponseData.initData(
      cardholderName,
      cardNumber,
      expiryMonth,
      expiryYear,
      cardSecurityCode,
      billingAddress
    );
    return basicCardResponseData;
  },

  createPaymentAddress({
    addressLines = [],
    city = "",
    country = "",
    dependentLocality = "",
    organization = "",
    postalCode = "",
    phone = "",
    recipient = "",
    region = "",
    regionCode = "",
    sortingCode = "",
  }) {
    const paymentAddress = Cc[
      "@mozilla.org/dom/payments/payment-address;1"
    ].createInstance(Ci.nsIPaymentAddress);
    const addressLine = Cc["@mozilla.org/array;1"].createInstance(
      Ci.nsIMutableArray
    );
    for (let line of addressLines) {
      const address = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      address.data = line;
      addressLine.appendElement(address);
    }
    paymentAddress.init(
      country,
      addressLine,
      region,
      regionCode,
      city,
      dependentLocality,
      postalCode,
      sortingCode,
      organization,
      recipient,
      phone
    );
    return paymentAddress;
  },

  createComponentInstance(componentInterface) {
    let componentName;
    switch (componentInterface) {
      case Ci.nsIPaymentShowActionResponse: {
        componentName =
          "@mozilla.org/dom/payments/payment-show-action-response;1";
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

  async fetchSavedAddresses() {
    let savedAddresses = {};
    for (let address of await formAutofillStorage.addresses.getAll()) {
      savedAddresses[address.guid] = address;
    }
    return savedAddresses;
  },

  async fetchSavedPaymentCards() {
    let savedBasicCards = {};
    for (let card of await formAutofillStorage.creditCards.getAll()) {
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

  fetchTempPaymentCards() {
    let creditCards = this.temporaryStore.creditCards.getAll();
    for (let card of Object.values(creditCards)) {
      // Ensure each card has a methodName property.
      if (!card.methodName) {
        card.methodName = "basic-card";
      }
    }
    return creditCards;
  },

  async onAutofillStorageChange() {
    let [savedAddresses, savedBasicCards] = await Promise.all([
      this.fetchSavedAddresses(),
      this.fetchSavedPaymentCards(),
    ]);

    this.sendMessageToContent("updateState", {
      savedAddresses,
      savedBasicCards,
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
    if (
      value === null ||
      type == "string" ||
      type == "number" ||
      type == "boolean"
    ) {
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
        throw new Error(
          `No interface associated with the members of the ${name} nsIArray`
        );
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
      let items = value
        .map(item => this._serializeRequest(item))
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

  async initializeFrame() {
    // We don't do this earlier as it's only necessary once this function sends
    // the initial saved records.
    Services.obs.addObserver(this, "formautofill-storage-changed", true);

    let requestSerialized = this._serializeRequest(this.request);
    let chromeWindow = this.frameWeakRef.get().ownerGlobal;
    let isPrivate = PrivateBrowsingUtils.isWindowPrivate(chromeWindow);

    let [savedAddresses, savedBasicCards] = await Promise.all([
      this.fetchSavedAddresses(),
      this.fetchSavedPaymentCards(),
    ]);

    this.sendMessageToContent("showPaymentRequest", {
      request: requestSerialized,
      savedAddresses,
      tempAddresses: this.temporaryStore.addresses.getAll(),
      savedBasicCards,
      tempBasicCards: this.fetchTempPaymentCards(),
      isPrivate,
    });
  },

  debugFrame() {
    // To avoid self-XSS-type attacks, ensure that Browser Chrome debugging is enabled.
    if (!Services.prefs.getBoolPref("devtools.chrome.enabled", false)) {
      Cu.reportError(
        "devtools.chrome.enabled must be enabled to debug the frame"
      );
      return;
    }
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    const {
      gDevToolsBrowser,
    } = require("devtools/client/framework/devtools-browser");
    gDevToolsBrowser.openContentProcessToolbox({
      selectedBrowser: this.frameWeakRef.get(),
    });
  },

  onOpenPreferences() {
    BrowserWindowTracker.getTopWindow().openPreferences(
      "privacy-form-autofill"
    );
  },

  onPaymentCancel() {
    const showResponse = this.createShowResponse({
      acceptStatus: Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
    });

    paymentSrv.respondPayment(showResponse);
    paymentUISrv.closePayment(this.request.requestId);
  },

  async onPay({
    selectedPayerAddressGUID: payerGUID,
    selectedPaymentCardGUID: paymentCardGUID,
    selectedPaymentCardSecurityCode: cardSecurityCode,
    selectedShippingAddressGUID: shippingGUID,
  }) {
    let methodData;
    try {
      methodData = await this._convertProfileBasicCardToPaymentMethodData(
        paymentCardGUID,
        cardSecurityCode
      );
    } catch (ex) {
      // TODO (Bug 1498403): Some kind of "credit card storage error" here, perhaps asking user
      // to re-enter credit card # from management UI.
      Cu.reportError(ex);
      return;
    }

    if (!methodData) {
      // TODO (Bug 1429265/Bug 1429205): Handle when a user hits cancel on the
      // Master Password dialog.
      Cu.reportError(
        "Bug 1429265/Bug 1429205: User canceled master password entry"
      );
      return;
    }

    let payerName = "";
    let payerEmail = "";
    let payerPhone = "";
    if (payerGUID) {
      let payerData = await this._convertProfileAddressToPayerData(payerGUID);
      payerName = payerData.payerName;
      payerEmail = payerData.payerEmail;
      payerPhone = payerData.payerPhone;
    }

    // Update the lastUsedTime for the payerAddress and paymentCard. Check if
    // the record exists in formAutofillStorage because it may be temporary.
    if (
      shippingGUID &&
      (await formAutofillStorage.addresses.get(shippingGUID))
    ) {
      formAutofillStorage.addresses.notifyUsed(shippingGUID);
    }
    if (payerGUID && (await formAutofillStorage.addresses.get(payerGUID))) {
      formAutofillStorage.addresses.notifyUsed(payerGUID);
    }
    if (await formAutofillStorage.creditCards.get(paymentCardGUID)) {
      formAutofillStorage.creditCards.notifyUsed(paymentCardGUID);
    }

    this.pay({
      methodName: "basic-card",
      methodData,
      payerName,
      payerEmail,
      payerPhone,
    });
  },

  pay({ payerName, payerEmail, payerPhone, methodName, methodData }) {
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

  async onChangePayerAddress({ payerAddressGUID }) {
    if (payerAddressGUID) {
      // If a payer address was de-selected e.g. the selected address was deleted, we'll
      // just wait to send the address change when the payer address is eventually selected
      // before clicking Pay since it's a required field.
      let {
        payerName,
        payerEmail,
        payerPhone,
      } = await this._convertProfileAddressToPayerData(payerAddressGUID);
      paymentSrv.changePayerDetail(
        this.request.requestId,
        payerName,
        payerEmail,
        payerPhone
      );
    }
  },

  async onChangePaymentMethod({
    selectedPaymentCardBillingAddressGUID: billingAddressGUID,
  }) {
    const methodName = "basic-card";
    let methodDetails;
    try {
      let billingAddress = await this._convertProfileAddressToPaymentAddress(
        billingAddressGUID
      );
      const basicCardChangeDetails = Cc[
        "@mozilla.org/dom/payments/basiccard-change-details;1"
      ].createInstance(Ci.nsIBasicCardChangeDetails);
      basicCardChangeDetails.initData(billingAddress);
      methodDetails = basicCardChangeDetails.QueryInterface(
        Ci.nsIMethodChangeDetails
      );
    } catch (ex) {
      // TODO (Bug 1498403): Some kind of "credit card storage error" here, perhaps asking user
      // to re-enter credit card # from management UI.
      Cu.reportError(ex);
      return;
    }

    paymentSrv.changePaymentMethod(
      this.request.requestId,
      methodName,
      methodDetails
    );
  },

  async onChangeShippingAddress({ shippingAddressGUID }) {
    if (shippingAddressGUID) {
      // If a shipping address was de-selected e.g. the selected address was deleted, we'll
      // just wait to send the address change when the shipping address is eventually selected
      // before clicking Pay since it's a required field.
      let address = await this._convertProfileAddressToPaymentAddress(
        shippingAddressGUID
      );
      paymentSrv.changeShippingAddress(this.request.requestId, address);
    }
  },

  onChangeShippingOption({ optionID }) {
    // Note, failing here on browser_host_name.js because the test closes
    // the dialog before the onChangeShippingOption is called, thus
    // deleting the request and making the requestId invalid. Unclear
    // why we aren't seeing the same issue with onChangeShippingAddress.
    paymentSrv.changeShippingOption(this.request.requestId, optionID);
  },

  onCloseDialogMessage() {
    // The PR is complete(), just close the dialog
    paymentUISrv.closePayment(this.request.requestId);
  },

  async onUpdateAutofillRecord(collectionName, record, guid, messageID) {
    let responseMessage = {
      guid,
      messageID,
      stateChange: {},
    };
    try {
      let isTemporary = record.isTemporary;
      let collection = isTemporary
        ? this.temporaryStore[collectionName]
        : formAutofillStorage[collectionName];

      if (guid) {
        // We want to preserve old properties since the edit forms are often
        // shown without all fields visible/enabled and we don't want those
        // fields to be blanked upon saving. Examples of hidden/disabled fields:
        // email, cc-number, mailing-address on the payer forms, and payer fields
        // not requested in the payer form.
        let preserveOldProperties = true;
        await collection.update(guid, record, preserveOldProperties);
      } else {
        responseMessage.guid = await collection.add(record);
      }

      if (isTemporary && collectionName == "addresses") {
        // there will be no formautofill-storage-changed event to update state
        // so add updated collection here
        Object.assign(responseMessage.stateChange, {
          tempAddresses: this.temporaryStore.addresses.getAll(),
        });
      }
      if (isTemporary && collectionName == "creditCards") {
        // there will be no formautofill-storage-changed event to update state
        // so add updated collection here
        Object.assign(responseMessage.stateChange, {
          tempBasicCards: this.fetchTempPaymentCards(),
        });
      }
    } catch (ex) {
      responseMessage.error = true;
      Cu.reportError(ex);
    } finally {
      this.sendMessageToContent(
        "updateAutofillRecord:Response",
        responseMessage
      );
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
      case "message-manager-close": {
        if (this.mm && subject == this.mm) {
          // Remove the observer to avoid message manager errors while the dialog
          // is closing and tests are cleaning up autofill storage.
          Services.obs.removeObserver(this, "formautofill-storage-changed");
        }
        break;
      }
    }
  },

  receiveMessage({ data }) {
    let { messageType } = data;

    switch (messageType) {
      case "debugFrame": {
        this.debugFrame();
        break;
      }
      case "initializeRequest": {
        this.initializeFrame();
        break;
      }
      case "changePayerAddress": {
        this.onChangePayerAddress(data);
        break;
      }
      case "changePaymentMethod": {
        this.onChangePaymentMethod(data);
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
      case "closeDialog": {
        this.onCloseDialogMessage();
        break;
      }
      case "openPreferences": {
        this.onOpenPreferences();
        break;
      }
      case "paymentCancel": {
        this.onPaymentCancel();
        break;
      }
      case "paymentDialogReady": {
        this.frameWeakRef.get().dispatchEvent(
          new Event("tabmodaldialogready", {
            bubbles: true,
          })
        );
        break;
      }
      case "pay": {
        this.onPay(data);
        break;
      }
      case "updateAutofillRecord": {
        this.onUpdateAutofillRecord(
          data.collectionName,
          data.record,
          data.guid,
          data.messageID
        );
        break;
      }
      default: {
        throw new Error(
          `paymentDialogWrapper: Unexpected messageType: ${messageType}`
        );
      }
    }
  },
};

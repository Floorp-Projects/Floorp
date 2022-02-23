"use strict";

/* eslint
  "no-unused-vars": ["error", {
    vars: "local",
    args: "none",
  }],
*/

const BLANK_PAGE_PATH =
  "/browser/browser/components/payments/test/browser/blank_page.html";
const BLANK_PAGE_URL = "https://example.com" + BLANK_PAGE_PATH;
const RESPONSE_TIMEOUT_PREF = "dom.payments.response.timeout";
const SAVE_CREDITCARD_DEFAULT_PREF = "dom.payments.defaults.saveCreditCard";
const SAVE_ADDRESS_DEFAULT_PREF = "dom.payments.defaults.saveAddress";

const paymentSrv = Cc[
  "@mozilla.org/dom/payments/payment-request-service;1"
].getService(Ci.nsIPaymentRequestService);
const paymentUISrv = Cc[
  "@mozilla.org/dom/payments/payment-ui-service;1"
].getService(Ci.nsIPaymentUIService).wrappedJSObject;
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { formAutofillStorage } = ChromeUtils.import(
  "resource://formautofill/FormAutofillStorage.jsm"
);
const { OSKeyStoreTestUtils } = ChromeUtils.import(
  "resource://testing-common/OSKeyStoreTestUtils.jsm"
);
const { PaymentTestUtils: PTU } = ChromeUtils.import(
  "resource://testing-common/PaymentTestUtils.jsm"
);
var { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
);
var { CreditCard } = ChromeUtils.import(
  "resource://gre/modules/CreditCard.jsm"
);

function getPaymentRequests() {
  return Array.from(paymentSrv.enumerate());
}

/**
 * Return the container (e.g. dialog or overlay) that the payment request contents are shown in.
 * This abstracts away the details of the widget used so that this can more easily transition to
 * another kind of dialog/overlay.
 * @param {string} requestId
 * @returns {Promise}
 */
async function getPaymentWidget(requestId) {
  return BrowserTestUtils.waitForCondition(() => {
    let { dialogContainer } = paymentUISrv.findDialog(requestId);
    if (!dialogContainer) {
      return false;
    }
    let paymentFrame = dialogContainer.querySelector(
      ".paymentDialogContainerFrame"
    );
    if (!paymentFrame) {
      return false;
    }
    return {
      get closed() {
        return !paymentFrame.isConnected;
      },
      frameElement: paymentFrame,
    };
  }, "payment dialog should be opened");
}

async function getPaymentFrame(widget) {
  return widget.frameElement;
}

function waitForMessageFromWidget(messageType, widget = null) {
  info("waitForMessageFromWidget: " + messageType);
  return new Promise(resolve => {
    Services.mm.addMessageListener(
      "paymentContentToChrome",
      function onMessage({ data, target }) {
        if (data.messageType != messageType) {
          return;
        }
        if (widget && widget != target) {
          return;
        }
        resolve();
        info(`Got ${messageType} from widget`);
        Services.mm.removeMessageListener("paymentContentToChrome", onMessage);
      }
    );
  });
}

async function waitForWidgetReady(widget = null) {
  return waitForMessageFromWidget("paymentDialogReady", widget);
}

function spawnPaymentDialogTask(paymentDialogFrame, taskFn, args = null) {
  return SpecialPowers.spawn(paymentDialogFrame.frameLoader, [args], taskFn);
}

async function withMerchantTab(
  { browser = gBrowser, url = BLANK_PAGE_URL } = {
    browser: gBrowser,
    url: BLANK_PAGE_URL,
  },
  taskFn
) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: browser,
      url,
    },
    taskFn
  );

  paymentSrv.cleanup(); // Temporary measure until bug 1408234 is fixed.

  await new Promise(resolve => {
    SpecialPowers.exactGC(resolve);
  });
}

async function withNewTabInPrivateWindow(args = {}, taskFn) {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let tabArgs = Object.assign(args, {
    browser: privateWin.gBrowser,
  });
  await withMerchantTab(tabArgs, taskFn);
  await BrowserTestUtils.closeWindow(privateWin);
}

async function addAddressRecord(address) {
  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "add"
  );
  let guid = await formAutofillStorage.addresses.add(address);
  await onChanged;
  return guid;
}

async function addCardRecord(card) {
  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "add"
  );
  let guid = await formAutofillStorage.creditCards.add(card);
  await onChanged;
  return guid;
}

/**
 * Add address and creditCard records to the formautofill store
 *
 * @param {array=} addresses  - The addresses to add to the formautofill address store
 * @param {array=} cards - The cards to add to the formautofill creditCards store
 * @returns {Promise}
 */
async function addSampleAddressesAndBasicCard(
  addresses = [PTU.Addresses.TimBL, PTU.Addresses.TimBL2],
  cards = [PTU.BasicCards.JohnDoe]
) {
  let guids = {};

  for (let i = 0; i < addresses.length; i++) {
    guids[`address${i + 1}GUID`] = await addAddressRecord(addresses[i]);
  }

  for (let i = 0; i < cards.length; i++) {
    guids[`card${i + 1}GUID`] = await addCardRecord(cards[i]);
  }

  return guids;
}

/**
 * Checks that an address from autofill storage matches a Payment Request PaymentAddress.
 * @param {PaymentAddress} paymentAddress
 * @param {object} storageAddress
 * @param {string} msg to describe the check
 */
function checkPaymentAddressMatchesStorageAddress(
  paymentAddress,
  storageAddress,
  msg
) {
  info(msg);
  let addressLines = storageAddress["street-address"].split("\n");
  is(
    paymentAddress.addressLine[0],
    addressLines[0],
    "Address line 1 should match"
  );
  is(
    paymentAddress.addressLine[1],
    addressLines[1],
    "Address line 2 should match"
  );
  is(paymentAddress.country, storageAddress.country, "Country should match");
  is(
    paymentAddress.region,
    storageAddress["address-level1"] || "",
    "Region should match"
  );
  is(
    paymentAddress.city,
    storageAddress["address-level2"],
    "City should match"
  );
  is(
    paymentAddress.postalCode,
    storageAddress["postal-code"],
    "Zip code should match"
  );
  is(
    paymentAddress.organization,
    storageAddress.organization,
    "Org should match"
  );
  is(
    paymentAddress.recipient,
    `${storageAddress["given-name"]} ${storageAddress["additional-name"]} ` +
      `${storageAddress["family-name"]}`,
    "Recipient name should match"
  );
  is(paymentAddress.phone, storageAddress.tel, "Phone should match");
}

/**
 * Checks that a card from autofill storage matches a Payment Request MethodDetails response.
 * @param {MethodDetails} methodDetails
 * @param {object} card
 * @param {string} msg to describe the check
 */
function checkPaymentMethodDetailsMatchesCard(methodDetails, card, msg) {
  info(msg);
  // The card expiry month should be a zero-padded two-digit string.
  let cardExpiryMonth = card["cc-exp-month"].toString().padStart(2, "0");
  is(methodDetails.cardholderName, card["cc-name"], "Check cardholderName");
  is(methodDetails.cardNumber, card["cc-number"], "Check cardNumber");
  is(methodDetails.expiryMonth, cardExpiryMonth, "Check expiryMonth");
  is(methodDetails.expiryYear, card["cc-exp-year"], "Check expiryYear");
}

/**
 * Create a PaymentRequest object with the given parameters, then
 * run the given merchantTaskFn.
 *
 * @param {Object} browser
 * @param {Object} options
 * @param {Object} options.methodData
 * @param {Object} options.details
 * @param {Object} options.options
 * @param {Function} options.merchantTaskFn
 * @returns {Object} References to the window, requestId, and frame
 */
async function setupPaymentDialog(
  browser,
  { methodData, details, options, merchantTaskFn }
) {
  let dialogReadyPromise = waitForWidgetReady();
  let { requestId } = await SpecialPowers.spawn(
    browser,
    [
      {
        methodData,
        details,
        options,
      },
    ],
    merchantTaskFn
  );
  ok(requestId, "requestId should be defined");

  // get a reference to the UI dialog and the requestId
  let [win] = await Promise.all([
    getPaymentWidget(requestId),
    dialogReadyPromise,
  ]);
  ok(win, "Got payment widget");
  is(win.closed, false, "dialog should not be closed");

  let frame = await getPaymentFrame(win);
  ok(frame, "Got payment frame");

  await dialogReadyPromise;
  info("dialog ready");

  await spawnPaymentDialogTask(frame, () => {
    let elementHeight = element => element.getBoundingClientRect().height;
    content.isHidden = element => elementHeight(element) == 0;
    content.isVisible = element => elementHeight(element) > 0;
    content.fillField = async function fillField(field, value) {
      // Keep in-sync with the copy in payments_common.js but with EventUtils methods called on a
      // EventUtils object.
      field.focus();
      if (field.localName == "select") {
        if (field.value == value) {
          // Do nothing
          return;
        }
        field.value = value;
        field.dispatchEvent(
          new content.window.Event("input", { bubbles: true })
        );
        field.dispatchEvent(
          new content.window.Event("change", { bubbles: true })
        );
        return;
      }
      while (field.value) {
        EventUtils.sendKey("BACK_SPACE", content.window);
      }
      EventUtils.sendString(value, content.window);
    };
  });

  return { win, requestId, frame };
}

/**
 * Open a merchant tab with the given merchantTaskFn to create a PaymentRequest
 * and then open the associated PaymentRequest dialog in a new tab and run the
 * associated dialogTaskFn. The same taskArgs are passed to both functions.
 *
 * @param {Function} merchantTaskFn
 * @param {Function} dialogTaskFn
 * @param {Object} taskArgs
 * @param {Object} options
 * @param {string} options.origin
 */
async function spawnInDialogForMerchantTask(
  merchantTaskFn,
  dialogTaskFn,
  taskArgs,
  { browser, origin = "https://example.com" } = {
    origin: "https://example.com",
  }
) {
  await withMerchantTab(
    {
      browser,
      url: origin + BLANK_PAGE_PATH,
    },
    async merchBrowser => {
      let { win, frame } = await setupPaymentDialog(merchBrowser, {
        ...taskArgs,
        merchantTaskFn,
      });

      await spawnPaymentDialogTask(frame, dialogTaskFn, taskArgs);
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
}

async function loginAndCompletePayment(frame) {
  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);
  await osKeyStoreLoginShown;
}

async function setupFormAutofillStorage() {
  await formAutofillStorage.initialize();
}

function cleanupFormAutofillStorage() {
  formAutofillStorage.addresses.removeAll();
  formAutofillStorage.creditCards.removeAll();
}

add_task(async function setup_head() {
  SpecialPowers.registerConsoleListener(function onConsoleMessage(msg) {
    if (msg.isWarning || !msg.errorMessage) {
      // Ignore warnings and non-errors.
      return;
    }
    if (
      msg.category == "CSP_CSPViolationWithURI" &&
      msg.errorMessage.includes("at inline")
    ) {
      // Ignore unknown CSP error.
      return;
    }
    if (
      msg.message &&
      msg.message.match(/docShell is null.*BrowserUtils.jsm/)
    ) {
      // Bug 1478142 - Console spam from the Find Toolbar.
      return;
    }
    if (msg.message && msg.message.match(/PrioEncoder is not defined/)) {
      // Bug 1492638 - Console spam from TelemetrySession.
      return;
    }
    if (
      msg.message &&
      msg.message.match(/devicePixelRatio.*FaviconLoader.jsm/)
    ) {
      return;
    }
    if (
      msg.errorMessage == "AbortError: The operation was aborted. " &&
      msg.sourceName == "" &&
      msg.lineNumber == 0
    ) {
      return;
    }
    ok(false, msg.message || msg.errorMessage);
  });
  OSKeyStoreTestUtils.setup();
  await setupFormAutofillStorage();
  registerCleanupFunction(async function cleanup() {
    paymentSrv.cleanup();
    cleanupFormAutofillStorage();
    await OSKeyStoreTestUtils.cleanup();
    Services.prefs.clearUserPref(RESPONSE_TIMEOUT_PREF);
    Services.prefs.clearUserPref(SAVE_CREDITCARD_DEFAULT_PREF);
    Services.prefs.clearUserPref(SAVE_ADDRESS_DEFAULT_PREF);
    SpecialPowers.postConsoleSentinel();
    // CreditCard.jsm is imported into the global scope. It needs to be deleted
    // else it outlives the test and is reported as a leak.
    delete window.CreditCard;
  });
});

function deepClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

async function selectPaymentDialogShippingAddressByCountry(frame, country) {
  await spawnPaymentDialogTask(
    frame,
    PTU.DialogContentTasks.selectShippingAddressByCountry,
    country
  );
}

async function navigateToAddAddressPage(frame, aOptions = {}) {
  ok(aOptions.initialPageId, "initialPageId option supplied");
  ok(aOptions.addressPageId, "addressPageId option supplied");
  ok(aOptions.addLinkSelector, "addLinkSelector option supplied");

  await spawnPaymentDialogTask(
    frame,
    async options => {
      let { PaymentTestUtils } = ChromeUtils.import(
        "resource://testing-common/PaymentTestUtils.jsm"
      );

      info("navigateToAddAddressPage: check we're on the expected page first");
      await PaymentTestUtils.DialogContentUtils.waitForState(
        content,
        state => {
          info(
            "current page state: " +
              state.page.id +
              " waiting for: " +
              options.initialPageId
          );
          return state.page.id == options.initialPageId;
        },
        "Check initial page state"
      );

      // click through to add/edit address page
      info("navigateToAddAddressPage: click the link");
      let addLink = content.document.querySelector(options.addLinkSelector);
      addLink.click();

      info("navigateToAddAddressPage: wait for address page");
      await PaymentTestUtils.DialogContentUtils.waitForState(
        content,
        state => {
          return state.page.id == options.addressPageId && !state.page.guid;
        },
        "Check add page state"
      );
    },
    aOptions
  );
}

async function navigateToAddShippingAddressPage(frame, aOptions = {}) {
  let options = Object.assign(
    {
      addLinkSelector:
        'address-picker[selected-state-key="selectedShippingAddress"] .add-link',
      initialPageId: "payment-summary",
      addressPageId: "shipping-address-page",
    },
    aOptions
  );
  await navigateToAddAddressPage(frame, options);
}

async function fillInBillingAddressForm(frame, aAddress, aOptions = {}) {
  // For now billing and shipping address forms have the same fields but that may
  // change so use separarate helpers.
  let address = Object.assign({}, aAddress);
  // Email isn't used on address forms, only payer/contact ones.
  delete address.email;
  let options = Object.assign(
    {
      addressPageId: "billing-address-page",
      expectedSelectedStateKey: ["basic-card-page", "billingAddressGUID"],
    },
    aOptions
  );
  return fillInAddressForm(frame, address, options);
}

async function fillInShippingAddressForm(frame, aAddress, aOptions) {
  let address = Object.assign({}, aAddress);
  // Email isn't used on address forms, only payer/contact ones.
  delete address.email;
  return fillInAddressForm(frame, address, {
    expectedSelectedStateKey: ["selectedShippingAddress"],
    ...aOptions,
  });
}

async function fillInPayerAddressForm(frame, aAddress) {
  let address = Object.assign({}, aAddress);
  let payerFields = [
    "given-name",
    "additional-name",
    "family-name",
    "tel",
    "email",
  ];
  for (let fieldName of Object.keys(address)) {
    if (payerFields.includes(fieldName)) {
      continue;
    }
    delete address[fieldName];
  }
  return fillInAddressForm(frame, address, {
    expectedSelectedStateKey: ["selectedPayerAddress"],
  });
}

/**
 * @param {HTMLElement} frame
 * @param {object} aAddress
 * @param {object} [aOptions = {}]
 * @param {boolean} [aOptions.setPersistCheckedValue = undefined] How to set the persist checkbox.
 * @param {string[]} [expectedSelectedStateKey = undefined] The expected selectedStateKey for
                                                            address-page.
 */
async function fillInAddressForm(frame, aAddress, aOptions = {}) {
  await spawnPaymentDialogTask(
    frame,
    async args => {
      let { address, options = {} } = args;
      let { requestStore } = Cu.waiveXrays(
        content.document.querySelector("payment-dialog")
      );
      let currentState = requestStore.getState();
      let addressForm = content.document.getElementById(currentState.page.id);
      ok(
        addressForm,
        "found the addressForm: " + addressForm.getAttribute("id")
      );

      if (options.expectedSelectedStateKey) {
        Assert.deepEqual(
          addressForm.getAttribute("selected-state-key").split("|"),
          options.expectedSelectedStateKey,
          "Check address page selectedStateKey"
        );
      }

      if (typeof address.country != "undefined") {
        // Set the country first so that the appropriate fields are visible.
        let countryField = addressForm.querySelector("#country");
        ok(!countryField.disabled, "Country Field shouldn't be disabled");
        await content.fillField(countryField, address.country);
        is(
          countryField.value,
          address.country,
          "country value is correct after fillField"
        );
      }

      // fill the form
      info(
        "fillInAddressForm: fill the form with address: " +
          JSON.stringify(address)
      );
      for (let [key, val] of Object.entries(address)) {
        let field = addressForm.querySelector(`#${key}`);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
        await content.fillField(field, val);
        is(field.value, val, `${key} value is correct after fillField`);
      }
      let persistCheckbox = Cu.waiveXrays(
        addressForm.querySelector(".persist-checkbox")
      );
      // only touch the checked state if explicitly told to in the options
      if (options.hasOwnProperty("setPersistCheckedValue")) {
        info(
          "fillInAddressForm: Manually setting the persist checkbox checkedness to: " +
            options.setPersistCheckedValue
        );
        Cu.waiveXrays(persistCheckbox).checked = options.setPersistCheckedValue;
      }
      info(
        `fillInAddressForm, persistCheckbox.checked: ${persistCheckbox.checked}`
      );
    },
    { address: aAddress, options: aOptions }
  );
}

async function verifyPersistCheckbox(frame, aOptions = {}) {
  await spawnPaymentDialogTask(
    frame,
    async args => {
      let { options = {} } = args;
      // ensure card/address is persisted or not based on the temporary option given
      info("verifyPersistCheckbox, got options: " + JSON.stringify(options));
      let persistCheckbox = Cu.waiveXrays(
        content.document.querySelector(options.checkboxSelector)
      );

      if (options.isEditing) {
        ok(
          persistCheckbox.hidden,
          "checkbox should be hidden when editing a record"
        );
      } else {
        ok(
          !persistCheckbox.hidden,
          "checkbox should be visible when adding a new record"
        );
        is(
          persistCheckbox.checked,
          options.expectPersist,
          `persist checkbox state is expected to be ${options.expectPersist}`
        );
      }
    },
    { options: aOptions }
  );
}

async function verifyCardNetwork(frame, aOptions = {}) {
  aOptions.supportedNetworks = CreditCard.SUPPORTED_NETWORKS;

  await spawnPaymentDialogTask(
    frame,
    async args => {
      let { options = {} } = args;
      // ensure the network picker is visible, has the right contents and expected value
      let networkSelect = Cu.waiveXrays(
        content.document.querySelector(options.networkSelector)
      );
      ok(
        content.isVisible(networkSelect),
        "The network selector should always be visible"
      );
      is(
        networkSelect.childElementCount,
        options.supportedNetworks.length + 1,
        "Should have one more than the number of supported networks"
      );
      is(
        networkSelect.children[0].value,
        "",
        "The first option should be the blank/empty option"
      );
      is(
        networkSelect.value,
        options.expectedNetwork,
        `The network picker should have the expected value`
      );
    },
    { options: aOptions }
  );
}

async function submitAddressForm(
  frame,
  aAddress,
  aOptions = {
    nextPageId: "payment-summary",
  }
) {
  await spawnPaymentDialogTask(
    frame,
    async args => {
      let { options = {} } = args;
      let nextPageId = options.nextPageId || "payment-summary";
      let { PaymentTestUtils } = ChromeUtils.import(
        "resource://testing-common/PaymentTestUtils.jsm"
      );

      let oldState = await PaymentTestUtils.DialogContentUtils.getCurrentState(
        content
      );
      let pageId = oldState.page.id;

      // submit the form to return to summary page
      content.document.querySelector(`#${pageId} button.primary`).click();

      let currState = await PaymentTestUtils.DialogContentUtils.waitForState(
        content,
        state => {
          return state.page.id == nextPageId;
        },
        `submitAddressForm: Switched back to ${nextPageId}`
      );

      let savedCount = Object.keys(currState.savedAddresses).length;
      let tempCount = Object.keys(currState.tempAddresses).length;
      let oldSavedCount = Object.keys(oldState.savedAddresses).length;
      let oldTempCount = Object.keys(oldState.tempAddresses).length;

      if (options.isEditing) {
        is(tempCount, oldTempCount, "tempAddresses count didn't change");
        is(savedCount, oldSavedCount, "savedAddresses count didn't change");
      } else if (options.expectPersist) {
        is(tempCount, oldTempCount, "tempAddresses count didn't change");
        is(savedCount, oldSavedCount + 1, "Entry added to savedAddresses");
      } else {
        is(tempCount, oldTempCount + 1, "Entry added to tempAddresses");
        is(savedCount, oldSavedCount, "savedAddresses count didn't change");
      }
    },
    { address: aAddress, options: aOptions }
  );
}

async function manuallyAddShippingAddress(frame, aAddress, aOptions = {}) {
  let options = Object.assign(
    {
      expectPersist: true,
      isEditing: false,
    },
    aOptions,
    {
      checkboxSelector: "#shipping-address-page .persist-checkbox",
    }
  );
  await navigateToAddShippingAddressPage(frame);
  info(
    "manuallyAddShippingAddress, fill in address form with options: " +
      JSON.stringify(options)
  );
  await fillInShippingAddressForm(frame, aAddress, options);
  info(
    "manuallyAddShippingAddress, verifyPersistCheckbox with options: " +
      JSON.stringify(options)
  );
  await verifyPersistCheckbox(frame, options);
  await submitAddressForm(frame, aAddress, options);
}

async function navigateToAddCardPage(
  frame,
  aOptions = {
    addLinkSelector: "payment-method-picker .add-link",
  }
) {
  await spawnPaymentDialogTask(
    frame,
    async options => {
      let { PaymentTestUtils } = ChromeUtils.import(
        "resource://testing-common/PaymentTestUtils.jsm"
      );

      // check were on the summary page first
      await PaymentTestUtils.DialogContentUtils.waitForState(
        content,
        state => {
          return !state.page.id || state.page.id == "payment-summary";
        },
        "Check summary page state"
      );

      // click through to add/edit card page
      let addLink = content.document.querySelector(options.addLinkSelector);
      addLink.click();

      // wait for card page
      await PaymentTestUtils.DialogContentUtils.waitForState(
        content,
        state => {
          return state.page.id == "basic-card-page";
        },
        "Check add/edit page state"
      );
    },
    aOptions
  );
}

async function fillInCardForm(frame, aCard, aOptions = {}) {
  await spawnPaymentDialogTask(
    frame,
    async args => {
      let { card, options = {} } = args;

      // fill the form
      info("fillInCardForm: fill the form with card: " + JSON.stringify(card));
      for (let [key, val] of Object.entries(card)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
        // Reset the value first so that we properly handle typing the value
        // already selected which may select another option with the same prefix.
        field.value = "";
        ok(!field.value, "Field value should be reset before typing");
        field.blur();
        field.focus();
        // Using waitForEvent here causes the test to hang, but
        // waitForCondition and checking activeElement does the trick. The root cause
        // of this should be investigated further.
        await ContentTaskUtils.waitForCondition(
          () => field == content.document.activeElement,
          `Waiting for field #${key} to get focus`
        );
        if (key == "billingAddressGUID") {
          // Can't type the value in, press Down until the value is found
          content.fillField(field, val);
        } else {
          // cc-exp-* fields are numbers so convert to strings and pad left with 0
          let fillValue = val.toString().padStart(2, "0");
          EventUtils.synthesizeKey(
            fillValue,
            {},
            Cu.waiveXrays(content.window)
          );
        }
        // cc-exp-* field values are not padded, so compare with unpadded string.
        is(
          field.value,
          val.toString(),
          `${key} value is correct after sendString`
        );
      }

      info(
        [...content.document.getElementById("cc-exp-year").options]
          .map(op => op.label)
          .join(",")
      );

      let persistCheckbox = content.document.querySelector(
        options.checkboxSelector
      );
      // only touch the checked state if explicitly told to in the options
      if (options.hasOwnProperty("setPersistCheckedValue")) {
        info(
          "fillInCardForm: Manually setting the persist checkbox checkedness to: " +
            options.setPersistCheckedValue
        );
        Cu.waiveXrays(persistCheckbox).checked = options.setPersistCheckedValue;
      }
    },
    { card: aCard, options: aOptions }
  );
}

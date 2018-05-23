"use strict";

/* eslint
  "no-unused-vars": ["error", {
    vars: "local",
    args: "none",
  }],
*/


const BLANK_PAGE_PATH = "/browser/browser/components/payments/test/browser/blank_page.html";
const BLANK_PAGE_URL = "https://example.com" + BLANK_PAGE_PATH;

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"]
                     .getService(Ci.nsIPaymentRequestService);
const paymentUISrv = Cc["@mozilla.org/dom/payments/payment-ui-service;1"]
                     .getService().wrappedJSObject;
const {formAutofillStorage} = ChromeUtils.import(
  "resource://formautofill/FormAutofillStorage.jsm", {});
const {PaymentTestUtils: PTU} = ChromeUtils.import(
  "resource://testing-common/PaymentTestUtils.jsm", {});

function getPaymentRequests() {
  let requestsEnum = paymentSrv.enumerate();
  let requests = [];
  while (requestsEnum.hasMoreElements()) {
    requests.push(requestsEnum.getNext().QueryInterface(Ci.nsIPaymentRequest));
  }
  return requests;
}

/**
 * Return the container (e.g. dialog or overlay) that the payment request contents are shown in.
 * This abstracts away the details of the widget used so that this can more earily transition from a
 * dialog to another kind of overlay.
 * Consumers shouldn't rely on a dialog window being returned.
 * @returns {Promise}
 */
async function getPaymentWidget() {
  let win;
  await BrowserTestUtils.waitForCondition(() => {
    win = Services.wm.getMostRecentWindow(null);
    return win.name.startsWith(paymentUISrv.REQUEST_ID_PREFIX);
  }, "payment dialog should be the most recent");

  return win;
}

async function getPaymentFrame(widget) {
  return widget.document.getElementById("paymentRequestFrame");
}

function waitForMessageFromWidget(messageType, widget = null) {
  info("waitForMessageFromWidget: " + messageType);
  return new Promise(resolve => {
    Services.mm.addMessageListener("paymentContentToChrome", function onMessage({data, target}) {
      if (data.messageType != messageType) {
        return;
      }
      if (widget && widget != target) {
        return;
      }
      resolve();
      info(`Got ${messageType} from widget`);
      Services.mm.removeMessageListener("paymentContentToChrome", onMessage);
    });
  });
}

async function waitForWidgetReady(widget = null) {
  return waitForMessageFromWidget("paymentDialogReady", widget);
}

function spawnPaymentDialogTask(paymentDialogFrame, taskFn, args = null) {
  return ContentTask.spawn(paymentDialogFrame.frameLoader, args, taskFn);
}

async function withMerchantTab({browser = gBrowser, url = BLANK_PAGE_URL} = {
  browser: gBrowser,
  url: BLANK_PAGE_URL,
}, taskFn) {
  await BrowserTestUtils.withNewTab({
    gBrowser: browser,
    url,
  }, taskFn);

  paymentSrv.cleanup(); // Temporary measure until bug 1408234 is fixed.

  await new Promise(resolve => {
    SpecialPowers.exactGC(resolve);
  });
}

/**
 * Load the privileged payment dialog wrapper document in a new tab and run the
 * task function.
 *
 * @param {string} requestId of the PaymentRequest
 * @param {Function} taskFn to run in the dialog with the frame as an argument.
 * @returns {Promise} which resolves when the dialog document is loaded
 */
function withNewDialogFrame(requestId, taskFn) {
  async function dialogTabTask(dialogBrowser) {
    let paymentRequestFrame = dialogBrowser.contentDocument.getElementById("paymentRequestFrame");
    // Ensure the inner frame is loaded
    await spawnPaymentDialogTask(paymentRequestFrame, async function ensureLoaded() {
      await ContentTaskUtils.waitForCondition(() => content.document.readyState == "complete",
                                              "Waiting for the unprivileged frame to load");
    });
    await taskFn(paymentRequestFrame);
  }

  let args = {
    gBrowser,
    url: `chrome://payments/content/paymentDialogWrapper.xul?requestId=${requestId}`,
  };
  return BrowserTestUtils.withNewTab(args, dialogTabTask);
}

async function withNewTabInPrivateWindow(args = {}, taskFn) {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let tabArgs = Object.assign(args, {
    browser: privateWin.gBrowser,
  });
  await withMerchantTab(tabArgs, taskFn);
  await BrowserTestUtils.closeWindow(privateWin);
}

/**
 * Spawn a content task inside the inner unprivileged frame of a privileged Payment Request dialog.
 *
 * @param {string} requestId
 * @param {Function} contentTaskFn
 * @param {object?} [args = null] for the content task
 * @returns {Promise}
 */
function spawnTaskInNewDialog(requestId, contentTaskFn, args = null) {
  return withNewDialogFrame(requestId, async function spawnTaskInNewDialog_tabTask(reqFrame) {
    await spawnPaymentDialogTask(reqFrame, contentTaskFn, args);
  });
}

async function addSampleAddressesAndBasicCard() {
  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  let address1GUID = formAutofillStorage.addresses.add(PTU.Addresses.TimBL);
  await onChanged;

  onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                      (subject, data) => data == "add");
  let address2GUID = formAutofillStorage.addresses.add(PTU.Addresses.TimBL2);
  await onChanged;

  onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                      (subject, data) => data == "add");
  let card1GUID = formAutofillStorage.creditCards.add(PTU.BasicCards.JohnDoe);
  await onChanged;

  return {
    address1GUID,
    address2GUID,
    card1GUID,
  };
}

/**
 * Checks that an address from autofill storage matches a Payment Request PaymentAddress.
 * @param {PaymentAddress} paymentAddress
 * @param {object} storageAddress
 * @param {string} msg to describe the check
 */
function checkPaymentAddressMatchesStorageAddress(paymentAddress, storageAddress, msg) {
  info(msg);
  let addressLines = storageAddress["street-address"].split("\n");
  is(paymentAddress.addressLine[0], addressLines[0], "Address line 1 should match");
  is(paymentAddress.addressLine[1], addressLines[1], "Address line 2 should match");
  is(paymentAddress.country, storageAddress.country, "Country should match");
  is(paymentAddress.region, storageAddress["address-level1"], "Region should match");
  is(paymentAddress.city, storageAddress["address-level2"], "City should match");
  is(paymentAddress.postalCode, storageAddress["postal-code"], "Zip code should match");
  is(paymentAddress.organization, storageAddress.organization, "Org should match");
  is(paymentAddress.recipient,
     `${storageAddress["given-name"]} ${storageAddress["additional-name"]} ` +
     `${storageAddress["family-name"]}`,
     "Recipient name should match");
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
  let cardExpiryMonth = card["cc-exp-month"] < 10 ?
                       "0" + card["cc-exp-month"] :
                       card["cc-exp-month"].toString();
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
async function setupPaymentDialog(browser, {methodData, details, options, merchantTaskFn}) {
  let dialogReadyPromise = waitForWidgetReady();
  await ContentTask.spawn(browser,
                          {
                            methodData,
                            details,
                            options,
                          },
                          merchantTaskFn);

  // get a reference to the UI dialog and the requestId
  let [win] = await Promise.all([getPaymentWidget(), dialogReadyPromise]);
  ok(win, "Got payment widget");
  let requestId = paymentUISrv.requestIdForWindow(win);
  ok(requestId, "requestId should be defined");
  is(win.closed, false, "dialog should not be closed");

  let frame = await getPaymentFrame(win);
  ok(frame, "Got payment frame");

  await dialogReadyPromise;
  info("dialog ready");

  await spawnPaymentDialogTask(frame, () => {
    let elementHeight = (element) =>
      element.getBoundingClientRect().height;
    content.isHidden = (element) => elementHeight(element) == 0;
    content.isVisible = (element) => elementHeight(element) > 0;
  });
  info("helper functions injected into frame");

  return {win, requestId, frame};
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
async function spawnInDialogForMerchantTask(merchantTaskFn, dialogTaskFn, taskArgs, {
  browser,
  origin = "https://example.com",
} = {
  origin: "https://example.com",
}) {
  await withMerchantTab({
    browser,
    url: origin + BLANK_PAGE_PATH,
  }, async merchBrowser => {
    let {win, frame} = await setupPaymentDialog(merchBrowser, {
      ...taskArgs,
      merchantTaskFn,
    });

    await spawnPaymentDialogTask(frame, dialogTaskFn, taskArgs);
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
}

async function setupFormAutofillStorage() {
  await formAutofillStorage.initialize();
}

function cleanupFormAutofillStorage() {
  formAutofillStorage.addresses.removeAll();
  formAutofillStorage.creditCards.removeAll();
}

add_task(async function setup_head() {
  await setupFormAutofillStorage();
  registerCleanupFunction(function cleanup() {
    paymentSrv.cleanup();
    cleanupFormAutofillStorage();
  });
});

function deepClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

async function selectPaymentDialogShippingAddressByCountry(frame, country) {
  await spawnPaymentDialogTask(frame,
                               PTU.DialogContentTasks.selectShippingAddressByCountry,
                               country);
}

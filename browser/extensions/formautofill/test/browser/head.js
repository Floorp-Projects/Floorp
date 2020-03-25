/* exported MANAGE_ADDRESSES_DIALOG_URL, MANAGE_CREDIT_CARDS_DIALOG_URL, EDIT_ADDRESS_DIALOG_URL, EDIT_CREDIT_CARD_DIALOG_URL,
            BASE_URL, TEST_ADDRESS_1, TEST_ADDRESS_2, TEST_ADDRESS_3, TEST_ADDRESS_4, TEST_ADDRESS_5, TEST_ADDRESS_CA_1, TEST_ADDRESS_DE_1,
            TEST_ADDRESS_IE_1,
            TEST_CREDIT_CARD_1, TEST_CREDIT_CARD_2, TEST_CREDIT_CARD_3, FORM_URL, CREDITCARD_FORM_URL, CREDITCARD_FORM_IFRAME_URL
            FTU_PREF, ENABLED_AUTOFILL_ADDRESSES_PREF, AUTOFILL_CREDITCARDS_AVAILABLE_PREF, ENABLED_AUTOFILL_CREDITCARDS_PREF,
            SUPPORTED_COUNTRIES_PREF,
            SYNC_USERNAME_PREF, SYNC_ADDRESSES_PREF, SYNC_CREDITCARDS_PREF, SYNC_CREDITCARDS_AVAILABLE_PREF, CREDITCARDS_USED_STATUS_PREF,
            DEFAULT_REGION_PREF,
            sleep, expectPopupOpen, openPopupOn, openPopupForSubframe, expectPopupClose, closePopup, closePopupForSubframe,
            clickDoorhangerButton, getAddresses, saveAddress, removeAddresses, saveCreditCard,
            getDisplayedPopupItems, getDoorhangerCheckbox,
            getNotification, getDoorhangerButton, removeAllRecords, expectWarningText, testDialog */

"use strict";

ChromeUtils.import("resource://gre/modules/OSKeyStore.jsm", this);
ChromeUtils.import("resource://testing-common/OSKeyStoreTestUtils.jsm", this);

const MANAGE_ADDRESSES_DIALOG_URL =
  "chrome://formautofill/content/manageAddresses.xhtml";
const MANAGE_CREDIT_CARDS_DIALOG_URL =
  "chrome://formautofill/content/manageCreditCards.xhtml";
const EDIT_ADDRESS_DIALOG_URL =
  "chrome://formautofill/content/editAddress.xhtml";
const EDIT_CREDIT_CARD_DIALOG_URL =
  "chrome://formautofill/content/editCreditCard.xhtml";

const HTTP_TEST_PATH = "/browser/browser/extensions/formautofill/test/browser/";
const BASE_URL = "http://mochi.test:8888" + HTTP_TEST_PATH;
const FORM_URL = BASE_URL + "autocomplete_basic.html";
const CREDITCARD_FORM_URL =
  "https://example.org" +
  HTTP_TEST_PATH +
  "creditCard/autocomplete_creditcard_basic.html";
const CREDITCARD_FORM_IFRAME_URL =
  "https://example.org" +
  HTTP_TEST_PATH +
  "creditCard/autocomplete_creditcard_iframe.html";

const FTU_PREF = "extensions.formautofill.firstTimeUse";
const CREDITCARDS_USED_STATUS_PREF = "extensions.formautofill.creditCards.used";
const ENABLED_AUTOFILL_ADDRESSES_PREF =
  "extensions.formautofill.addresses.enabled";
const AUTOFILL_CREDITCARDS_AVAILABLE_PREF =
  "extensions.formautofill.creditCards.available";
const ENABLED_AUTOFILL_CREDITCARDS_PREF =
  "extensions.formautofill.creditCards.enabled";
const SUPPORTED_COUNTRIES_PREF = "extensions.formautofill.supportedCountries";
const SYNC_USERNAME_PREF = "services.sync.username";
const SYNC_ADDRESSES_PREF = "services.sync.engine.addresses";
const SYNC_CREDITCARDS_PREF = "services.sync.engine.creditcards";
const SYNC_CREDITCARDS_AVAILABLE_PREF =
  "services.sync.engine.creditcards.available";
const DEFAULT_REGION_PREF = "browser.search.region";

const TEST_ADDRESS_1 = {
  "given-name": "John",
  "additional-name": "R.",
  "family-name": "Smith",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  "address-level2": "Cambridge",
  "address-level1": "MA",
  "postal-code": "02139",
  country: "US",
  tel: "+16172535702",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_2 = {
  "street-address": "Some Address",
  country: "US",
};

const TEST_ADDRESS_3 = {
  "street-address": "Other Address",
  "postal-code": "12345",
};

const TEST_ADDRESS_4 = {
  "given-name": "Timothy",
  "family-name": "Berners-Lee",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  country: "US",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_5 = {
  tel: "+16172535702",
};

const TEST_ADDRESS_CA_1 = {
  "given-name": "John",
  "additional-name": "R.",
  "family-name": "Smith",
  organization: "Mozilla",
  "street-address": "163 W Hastings\nSuite 209",
  "address-level2": "Vancouver",
  "address-level1": "BC",
  "postal-code": "V6B 1H5",
  country: "CA",
  tel: "+17787851540",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_DE_1 = {
  "given-name": "John",
  "additional-name": "R.",
  "family-name": "Smith",
  organization: "Mozilla",
  "street-address":
    "Geb\u00E4ude 3, 4. Obergeschoss\nSchlesische Stra\u00DFe 27",
  "address-level2": "Berlin",
  "postal-code": "10997",
  country: "DE",
  tel: "+4930983333000",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_IE_1 = {
  "given-name": "Bob",
  "additional-name": "Z.",
  "family-name": "Builder",
  organization: "Best Co.",
  "street-address": "123 Kilkenny St.",
  "address-level3": "Some Townland",
  "address-level2": "Dublin",
  "address-level1": "Co. Dublin",
  "postal-code": "A65 F4E2",
  country: "IE",
  tel: "+13534564947391",
  email: "ie@example.com",
};

const TEST_CREDIT_CARD_1 = {
  "cc-name": "John Doe",
  "cc-number": "4111111111111111",
  "cc-exp-month": 4,
  "cc-exp-year": new Date().getFullYear(),
  "cc-type": "visa",
};

const TEST_CREDIT_CARD_2 = {
  "cc-name": "Timothy Berners-Lee",
  "cc-number": "4929001587121045",
  "cc-exp-month": 12,
  "cc-exp-year": new Date().getFullYear() + 10,
  "cc-type": "visa",
};

const TEST_CREDIT_CARD_3 = {
  "cc-number": "5103059495477870",
  "cc-exp-month": 1,
  "cc-exp-year": 2000,
  "cc-type": "mastercard",
};

const MAIN_BUTTON = "button";
const SECONDARY_BUTTON = "secondaryButton";
const MENU_BUTTON = "menubutton";

function getDisplayedPopupItems(
  browser,
  selector = ".autocomplete-richlistitem"
) {
  info("getDisplayedPopupItems");
  const {
    autoCompletePopup: { richlistbox: itemsBox },
  } = browser;
  const listItemElems = itemsBox.querySelectorAll(selector);

  return [...listItemElems].filter(
    item => item.getAttribute("collapsed") != "true"
  );
}

async function sleep(ms = 500) {
  await new Promise(resolve => setTimeout(resolve, ms));
}

async function focusAndWaitForFieldsIdentified(browserOrContext, selector) {
  info("expecting the target input being focused and identified");
  /* eslint no-shadow: ["error", { "allow": ["selector", "previouslyFocused", "previouslyIdentified"] }] */

  const { FormAutofillParent } = ChromeUtils.import(
    "resource://formautofill/FormAutofillParent.jsm"
  );

  // If the input is previously focused, no more notifications will be
  // sent as the notification goes along with focus event.
  let fieldsIdentifiedPromiseResolver;
  let fieldsIdentifiedObserver = {
    fieldsIdentified() {
      fieldsIdentifiedPromiseResolver();
    },
  };

  let fieldsIdentifiedPromise = new Promise(resolve => {
    fieldsIdentifiedPromiseResolver = resolve;
    FormAutofillParent.addMessageObserver(fieldsIdentifiedObserver);
  });

  const { previouslyFocused, previouslyIdentified } = await SpecialPowers.spawn(
    browserOrContext,
    [selector],
    async function(selector) {
      const { FormLikeFactory } = ChromeUtils.import(
        "resource://gre/modules/FormLikeFactory.jsm"
      );
      const input = content.document.querySelector(selector);
      const rootElement = FormLikeFactory.findRootForField(input);
      const previouslyFocused = content.document.activeElement == input;
      const previouslyIdentified = rootElement.hasAttribute(
        "test-formautofill-identified"
      );

      input.focus();

      return { previouslyFocused, previouslyIdentified };
    }
  );

  // Only wait for the fields identified notification if the
  // focus was not previously assigned to the input.
  if (previouslyFocused) {
    fieldsIdentifiedPromiseResolver();
  } else {
    info("!previouslyFocused");
  }

  // If a browsing context was supplied, focus its parent frame as well.
  if (
    browserOrContext instanceof BrowsingContext &&
    browserOrContext.parent != browserOrContext
  ) {
    await SpecialPowers.spawn(
      browserOrContext.parent,
      [browserOrContext],
      async function(browsingContext) {
        browsingContext.embedderElement.focus();
      }
    );
  }

  if (previouslyIdentified) {
    info("previouslyIdentified");
    FormAutofillParent.removeMessageObserver(fieldsIdentifiedObserver);
    return;
  }

  // Wait 500ms to ensure that "markAsAutofillField" is completely finished.
  await fieldsIdentifiedPromise;
  info("FieldsIdentified");
  FormAutofillParent.removeMessageObserver(fieldsIdentifiedObserver);

  await sleep();
  await SpecialPowers.spawn(browserOrContext, [], async function() {
    const { FormLikeFactory } = ChromeUtils.import(
      "resource://gre/modules/FormLikeFactory.jsm"
    );
    FormLikeFactory.findRootForField(
      content.document.activeElement
    ).setAttribute("test-formautofill-identified", "true");
  });
}

async function expectPopupOpen(browser) {
  info("expectPopupOpen");
  const { autoCompletePopup } = browser;
  await BrowserTestUtils.waitForCondition(
    () => autoCompletePopup.popupOpen,
    "popup should be open"
  );
  await BrowserTestUtils.waitForCondition(() => {
    const listItemElems = getDisplayedPopupItems(browser);
    return (
      !![...listItemElems].length &&
      [...listItemElems].every(item => {
        return (
          (item.getAttribute("originaltype") == "autofill-profile" ||
            item.getAttribute("originaltype") == "autofill-insecureWarning" ||
            item.getAttribute("originaltype") == "autofill-footer") &&
          item.hasAttribute("formautofillattached")
        );
      })
    );
  }, "The popup should be a form autofill one");
}

async function openPopupOn(browser, selector) {
  await SimpleTest.promiseFocus(browser);
  await focusAndWaitForFieldsIdentified(browser, selector);
  info("openPopupOn: before VK_DOWN");
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  await expectPopupOpen(browser);
}

async function openPopupForSubframe(browser, frameBrowsingContext, selector) {
  await SimpleTest.promiseFocus(browser);
  await focusAndWaitForFieldsIdentified(frameBrowsingContext, selector);
  info("openPopupOn: before VK_DOWN");
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, frameBrowsingContext);
  await expectPopupOpen(browser);
}

async function expectPopupClose(browser) {
  await BrowserTestUtils.waitForCondition(
    () => !browser.autoCompletePopup.popupOpen,
    "popup should have closed"
  );
}

async function closePopup(browser) {
  await SpecialPowers.spawn(browser, [], async function() {
    content.document.activeElement.blur();
  });

  await expectPopupClose(browser);
}

async function closePopupForSubframe(browser, frameBrowsingContext) {
  await SpecialPowers.spawn(frameBrowsingContext, [], async function() {
    content.document.activeElement.blur();
  });

  await expectPopupClose(browser);
}

function emulateMessageToBrowser(name, data) {
  let actor = gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getActor(
    "FormAutofill"
  );
  return actor.receiveMessage({ name, data });
}

function getRecords(data) {
  info(`expecting record retrievals: ${data.collectionName}`);
  return emulateMessageToBrowser("FormAutofill:GetRecords", data);
}

function getAddresses() {
  return getRecords({ collectionName: "addresses" });
}

function getCreditCards() {
  return getRecords({ collectionName: "creditCards" });
}

async function saveAddress(address) {
  info("expecting address saved");
  let observePromise = TestUtils.topicObserved("formautofill-storage-changed");
  await emulateMessageToBrowser("FormAutofill:SaveAddress", { address });
  await observePromise;
}

async function saveCreditCard(creditcard) {
  info("expecting credit card saved");
  let creditcardClone = Object.assign({}, creditcard);
  let observePromise = TestUtils.topicObserved("formautofill-storage-changed");
  await emulateMessageToBrowser("FormAutofill:SaveCreditCard", {
    creditcard: creditcardClone,
  });
  await observePromise;
}

async function removeAddresses(guids) {
  info("expecting address removed");
  let observePromise = TestUtils.topicObserved("formautofill-storage-changed");
  await emulateMessageToBrowser("FormAutofill:RemoveAddresses", { guids });
  await observePromise;
}

async function removeCreditCards(guids) {
  info("expecting credit card removed");
  let observePromise = TestUtils.topicObserved("formautofill-storage-changed");
  await emulateMessageToBrowser("FormAutofill:RemoveCreditCards", { guids });
  await observePromise;
}

function getNotification(index = 0) {
  let notifications = PopupNotifications.panel.childNodes;
  ok(!!notifications.length, "at least one notification displayed");
  ok(true, notifications.length + " notification(s)");
  return notifications[index];
}

/**
 * Clicks the popup notification button and wait for popup hidden.
 *
 * @param {string} button The button type in popup notification.
 * @param {number} index The action's index in menu list.
 */
async function clickDoorhangerButton(button, index) {
  let popuphidden = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );

  if (button == MAIN_BUTTON || button == SECONDARY_BUTTON) {
    EventUtils.synthesizeMouseAtCenter(getNotification()[button], {});
  } else if (button == MENU_BUTTON) {
    // Click the dropmarker arrow and wait for the menu to show up.
    info("expecting notification menu button present");
    await BrowserTestUtils.waitForCondition(() => getNotification().menubutton);
    await sleep(2000); // menubutton needs extra time for binding
    let notification = getNotification();
    ok(notification.menubutton, "notification menupopup displayed");
    let dropdownPromise = BrowserTestUtils.waitForEvent(
      notification.menupopup,
      "popupshown"
    );
    await EventUtils.synthesizeMouseAtCenter(notification.menubutton, {});
    info("expecting notification popup show up");
    await dropdownPromise;

    let actionMenuItem = notification.querySelectorAll("menuitem")[index];
    await EventUtils.synthesizeMouseAtCenter(actionMenuItem, {});
  }
  info("expecting notification popup hidden");
  await popuphidden;
}

function getDoorhangerCheckbox() {
  return getNotification().checkbox;
}

function getDoorhangerButton(button) {
  return getNotification()[button];
}

async function removeAllRecords() {
  let addresses = await getAddresses();
  if (addresses.length) {
    await removeAddresses(addresses.map(address => address.guid));
  }

  if (Services.prefs.getBoolPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF)) {
    let creditCards = await getCreditCards();
    if (creditCards.length) {
      await removeCreditCards(creditCards.map(cc => cc.guid));
    }
  }
}

async function waitForFocusAndFormReady(win) {
  return Promise.all([
    new Promise(resolve => waitForFocus(resolve, win)),
    BrowserTestUtils.waitForEvent(win, "FormReady"),
  ]);
}

// Verify that the warning in the autocomplete popup has the expected text.
async function expectWarningText(browser, expectedText) {
  const {
    autoCompletePopup: { richlistbox: itemsBox },
  } = browser;
  let warningBox = itemsBox.querySelector(
    ".autocomplete-richlistitem:last-child"
  );

  while (warningBox.collapsed) {
    warningBox = warningBox.previousSibling;
  }
  warningBox = warningBox._warningTextBox;

  await BrowserTestUtils.waitForCondition(() => {
    return warningBox.textContent == expectedText;
  }, `Waiting for expected warning text: ${expectedText}, Got ${warningBox.textContent}`);
  ok(true, `Got expected warning text: ${expectedText}`);
}

async function testDialog(url, testFn, arg = undefined) {
  // Skip this step for test cards that lack an encrypted
  // number since they will fail to decrypt.
  if (
    url == EDIT_CREDIT_CARD_DIALOG_URL &&
    arg &&
    arg.record &&
    arg.record["cc-number-encrypted"]
  ) {
    arg.record = Object.assign({}, arg.record, {
      "cc-number": await OSKeyStore.decrypt(arg.record["cc-number-encrypted"]),
    });
  }
  let win = window.openDialog(url, null, "width=600,height=600", arg);
  await waitForFocusAndFormReady(win);
  let unloadPromise = BrowserTestUtils.waitForEvent(win, "unload");
  await testFn(win);
  return unloadPromise;
}

add_task(function setup() {
  OSKeyStoreTestUtils.setup();
});

registerCleanupFunction(removeAllRecords);
registerCleanupFunction(async () => {
  await OSKeyStoreTestUtils.cleanup();
});

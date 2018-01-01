/* exported MANAGE_ADDRESSES_DIALOG_URL, MANAGE_CREDIT_CARDS_DIALOG_URL, EDIT_ADDRESS_DIALOG_URL, EDIT_CREDIT_CARD_DIALOG_URL,
            BASE_URL, TEST_ADDRESS_1, TEST_ADDRESS_2, TEST_ADDRESS_3, TEST_ADDRESS_4, TEST_ADDRESS_5, TEST_ADDRESS_CA_1, TEST_ADDRESS_DE_1,
            TEST_CREDIT_CARD_1, TEST_CREDIT_CARD_2, TEST_CREDIT_CARD_3, FORM_URL, CREDITCARD_FORM_URL,
            FTU_PREF, ENABLED_AUTOFILL_ADDRESSES_PREF, AUTOFILL_CREDITCARDS_AVAILABLE_PREF, ENABLED_AUTOFILL_CREDITCARDS_PREF,
            SYNC_USERNAME_PREF, SYNC_ADDRESSES_PREF, SYNC_CREDITCARDS_PREF, SYNC_CREDITCARDS_AVAILABLE_PREF, CREDITCARDS_USED_STATUS_PREF,
            sleep, expectPopupOpen, openPopupOn, expectPopupClose, closePopup, clickDoorhangerButton,
            getAddresses, saveAddress, removeAddresses, saveCreditCard,
            getDisplayedPopupItems, getDoorhangerCheckbox, waitForMasterPasswordDialog,
            getNotification, getDoorhangerButton, removeAllRecords */

"use strict";

Cu.import("resource://testing-common/LoginTestUtils.jsm", this);

const MANAGE_ADDRESSES_DIALOG_URL = "chrome://formautofill/content/manageAddresses.xhtml";
const MANAGE_CREDIT_CARDS_DIALOG_URL = "chrome://formautofill/content/manageCreditCards.xhtml";
const EDIT_ADDRESS_DIALOG_URL = "chrome://formautofill/content/editAddress.xhtml";
const EDIT_CREDIT_CARD_DIALOG_URL = "chrome://formautofill/content/editCreditCard.xhtml";
const BASE_URL = "http://mochi.test:8888/browser/browser/extensions/formautofill/test/browser/";
const FORM_URL = "http://mochi.test:8888/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";
const CREDITCARD_FORM_URL =
  "https://example.org/browser/browser/extensions/formautofill/test/browser/autocomplete_creditcard_basic.html";
const FTU_PREF = "extensions.formautofill.firstTimeUse";
const CREDITCARDS_USED_STATUS_PREF = "extensions.formautofill.creditCards.used";
const ENABLED_AUTOFILL_ADDRESSES_PREF = "extensions.formautofill.addresses.enabled";
const AUTOFILL_CREDITCARDS_AVAILABLE_PREF = "extensions.formautofill.creditCards.available";
const ENABLED_AUTOFILL_CREDITCARDS_PREF = "extensions.formautofill.creditCards.enabled";
const SYNC_USERNAME_PREF = "services.sync.username";
const SYNC_ADDRESSES_PREF = "services.sync.engine.addresses";
const SYNC_CREDITCARDS_PREF = "services.sync.engine.creditcards";
const SYNC_CREDITCARDS_AVAILABLE_PREF = "services.sync.engine.creditcards.available";

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
  "street-address": "Geb\u00E4ude 3, 4. Obergeschoss\nSchlesische Stra\u00DFe 27",
  "address-level2": "Berlin",
  "postal-code": "10997",
  country: "DE",
  tel: "+4930983333000",
  email: "timbl@w3.org",
};

const TEST_CREDIT_CARD_1 = {
  "cc-name": "John Doe",
  "cc-number": "1234567812345678",
  "cc-exp-month": 4,
  "cc-exp-year": (new Date()).getFullYear(),
};

const TEST_CREDIT_CARD_2 = {
  "cc-name": "Timothy Berners-Lee",
  "cc-number": "1111222233334444",
  "cc-exp-month": 12,
  "cc-exp-year": 2022,
};

const TEST_CREDIT_CARD_3 = {
  "cc-number": "9999888877776666",
  "cc-exp-month": 1,
  "cc-exp-year": 2000,
};

const MAIN_BUTTON = "button";
const SECONDARY_BUTTON = "secondaryButton";
const MENU_BUTTON = "menubutton";

function getDisplayedPopupItems(browser, selector = ".autocomplete-richlistitem") {
  const {autoCompletePopup: {richlistbox: itemsBox}} = browser;
  const listItemElems = itemsBox.querySelectorAll(selector);

  return [...listItemElems].filter(item => item.getAttribute("collapsed") != "true");
}

async function sleep(ms = 500) {
  await new Promise(resolve => setTimeout(resolve, ms));
}

async function focusAndWaitForFieldsIdentified(browser, selector) {
  info("expecting the target input being focused and indentified");
  /* eslint no-shadow: ["error", { "allow": ["selector", "previouslyFocused", "previouslyIdentified"] }] */
  const {previouslyFocused, previouslyIdentified} = await ContentTask.spawn(browser, {selector}, async function({selector}) {
    Components.utils.import("resource://gre/modules/FormLikeFactory.jsm");
    const input = content.document.querySelector(selector);
    const rootElement = FormLikeFactory.findRootForField(input);
    const previouslyFocused = content.document.activeElement == input;
    const previouslyIdentified = rootElement.hasAttribute("test-formautofill-identified");

    input.focus();

    return {previouslyFocused, previouslyIdentified};
  });

  if (previouslyIdentified) {
    return;
  }

  // Once the input is previously focused, no more FormAutofill:FieldsIdentified will be
  // sent as the message goes along with focus event.
  if (!previouslyFocused) {
    await new Promise(resolve => {
      Services.mm.addMessageListener("FormAutofill:FieldsIdentified", function onIdentified() {
        Services.mm.removeMessageListener("FormAutofill:FieldsIdentified", onIdentified);
        resolve();
      });
    });
  }
  // Wait 500ms to ensure that "markAsAutofillField" is completely finished.
  await sleep();
  await ContentTask.spawn(browser, {}, async function() {
    Components.utils.import("resource://gre/modules/FormLikeFactory.jsm");
    FormLikeFactory
      .findRootForField(content.document.activeElement)
      .setAttribute("test-formautofill-identified", "true");
  });
}

async function expectPopupOpen(browser) {
  const {autoCompletePopup} = browser;
  const listItemElems = getDisplayedPopupItems(browser);

  await BrowserTestUtils.waitForCondition(() => autoCompletePopup.popupOpen,
                                         "popup should be open");
  await BrowserTestUtils.waitForCondition(() => {
    return [...listItemElems].every(item => {
      return (item.getAttribute("originaltype") == "autofill-profile" ||
             item.getAttribute("originaltype") == "insecureWarning" ||
             item.getAttribute("originaltype") == "autofill-footer") &&
             item.hasAttribute("formautofillattached");
    });
  }, "The popup should be a form autofill one");
}

async function openPopupOn(browser, selector) {
  await SimpleTest.promiseFocus(browser);
  await focusAndWaitForFieldsIdentified(browser, selector);
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  await expectPopupOpen(browser);
}

async function expectPopupClose(browser) {
  await BrowserTestUtils.waitForCondition(() => !browser.autoCompletePopup.popupOpen,
    "popup should have closed");
}

async function closePopup(browser) {
  await ContentTask.spawn(browser, {}, async function() {
    content.document.activeElement.blur();
  });

  await expectPopupClose(browser);
}

function getRecords(data) {
  info(`expecting record retrievals: ${data.collectionName}`);
  return new Promise(resolve => {
    Services.cpmm.addMessageListener("FormAutofill:Records", function getResult(result) {
      Services.cpmm.removeMessageListener("FormAutofill:Records", getResult);
      resolve(result.data);
    });
    Services.cpmm.sendAsyncMessage("FormAutofill:GetRecords", data);
  });
}

function getAddresses() {
  return getRecords({collectionName: "addresses"});
}

function getCreditCards() {
  return getRecords({collectionName: "creditCards"});
}

function saveAddress(address) {
  info("expecting address saved");
  Services.cpmm.sendAsyncMessage("FormAutofill:SaveAddress", {address});
  return TestUtils.topicObserved("formautofill-storage-changed");
}

function saveCreditCard(creditcard) {
  info("expecting credit card saved");
  let creditcardClone = Object.assign({}, creditcard);
  Services.cpmm.sendAsyncMessage("FormAutofill:SaveCreditCard", {
    creditcard: creditcardClone,
  });
  return TestUtils.topicObserved("formautofill-storage-changed");
}

function removeAddresses(guids) {
  info("expecting address removed");
  Services.cpmm.sendAsyncMessage("FormAutofill:RemoveAddresses", {guids});
  return TestUtils.topicObserved("formautofill-storage-changed");
}

function removeCreditCards(guids) {
  info("expecting credit card removed");
  Services.cpmm.sendAsyncMessage("FormAutofill:RemoveCreditCards", {guids});
  return TestUtils.topicObserved("formautofill-storage-changed");
}

function getNotification(index = 0) {
  let notifications = PopupNotifications.panel.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
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
  let popuphidden = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");

  if (button == MAIN_BUTTON || button == SECONDARY_BUTTON) {
    EventUtils.synthesizeMouseAtCenter(getNotification()[button], {});
  } else if (button == MENU_BUTTON) {
    // Click the dropmarker arrow and wait for the menu to show up.
    info("expecting notification menu button present");
    await BrowserTestUtils.waitForCondition(() => getNotification().menubutton);
    await sleep(2000); // menubutton needs extra time for binding
    let notification = getNotification();
    ok(notification.menubutton, "notification menupopup displayed");
    let dropdownPromise =
      BrowserTestUtils.waitForEvent(notification.menupopup, "popupshown");
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


// Wait for the master password dialog to popup and enter the password to log in
// if "login" is "true" or dismiss it directly if otherwise.
function waitForMasterPasswordDialog(login = false) {
  info("expecting master password dialog loaded");
  let dialogShown = TestUtils.topicObserved("common-dialog-loaded");
  return dialogShown.then(([subject]) => {
    let dialog = subject.Dialog;
    is(dialog.args.title, "Password Required", "Master password dialog shown");
    if (login) {
      dialog.ui.password1Textbox.value = LoginTestUtils.masterPassword.masterPassword;
      dialog.ui.button0.click();
    } else {
      dialog.ui.button1.click();
    }
  });
}

async function removeAllRecords() {
  let addresses = await getAddresses();
  if (addresses.length) {
    await removeAddresses(addresses.map(address => address.guid));
  }

  let creditCards = await getCreditCards();
  if (creditCards.length) {
    await removeCreditCards(creditCards.map(cc => cc.guid));
  }
}

registerCleanupFunction(removeAllRecords);

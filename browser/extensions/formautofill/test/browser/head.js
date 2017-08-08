/* exported MANAGE_ADDRESSES_DIALOG_URL, EDIT_ADDRESS_DIALOG_URL, BASE_URL,
            TEST_ADDRESS_1, TEST_ADDRESS_2, TEST_ADDRESS_3,
            sleep, expectPopupOpen, openPopupOn, clickDoorhangerButton,
            getAddresses, saveAddress, removeAddresses */

"use strict";

const MANAGE_ADDRESSES_DIALOG_URL = "chrome://formautofill/content/manageAddresses.xhtml";
const EDIT_ADDRESS_DIALOG_URL = "chrome://formautofill/content/editAddress.xhtml";
const BASE_URL = "http://mochi.test:8888/browser/browser/extensions/formautofill/test/browser/";

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

const MAIN_BUTTON_INDEX = 0;
const SECONDARY_BUTTON_INDEX = 1;

async function sleep(ms = 500) {
  await new Promise(resolve => setTimeout(resolve, ms));
}

async function expectPopupOpen(browser) {
  const {autoCompletePopup, autoCompletePopup: {richlistbox: itemsBox}} = browser;
  const listItemElems = itemsBox.querySelectorAll(".autocomplete-richlistitem");

  await BrowserTestUtils.waitForCondition(() => autoCompletePopup.popupOpen,
                                         "popup should be open");
  await BrowserTestUtils.waitForCondition(() => {
    return [...listItemElems].every(item => {
      return (item.getAttribute("originaltype") == "autofill-profile" ||
             item.getAttribute("originaltype") == "autofill-footer") &&
             item.hasAttribute("formautofillattached");
    });
  }, "The popup should be a form autofill one");
}

async function openPopupOn(browser, selector) {
  await SimpleTest.promiseFocus(browser);
  /* eslint no-shadow: ["error", { "allow": ["selector"] }] */
  await ContentTask.spawn(browser, {selector}, async function({selector}) {
    content.document.querySelector(selector).focus();
  });
  await sleep(2000);
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  await expectPopupOpen(browser);
}

function getRecords(data) {
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

function saveAddress(address) {
  Services.cpmm.sendAsyncMessage("FormAutofill:SaveAddress", {address});
  return TestUtils.topicObserved("formautofill-storage-changed");
}

function removeAddresses(guids) {
  Services.cpmm.sendAsyncMessage("FormAutofill:RemoveAddresses", {guids});
  return TestUtils.topicObserved("formautofill-storage-changed");
}

/**
 * Clicks the popup notification button and wait for popup hidden.
 *
 * @param {number} buttonIndex Number indicating which button to click.
 *                             See the constants in this file.
 */
async function clickDoorhangerButton(buttonIndex) {
  let popuphidden = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
  let notifications = PopupNotifications.panel.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  ok(true, notifications.length + " notification(s)");
  let notification = notifications[0];

  if (buttonIndex == MAIN_BUTTON_INDEX) {
    ok(true, "Triggering main action");
    EventUtils.synthesizeMouseAtCenter(notification.button, {});
  } else if (buttonIndex == SECONDARY_BUTTON_INDEX) {
    ok(true, "Triggering secondary action");
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});
  } else if (notification.childNodes[buttonIndex - 1]) {
    ok(true, "Triggering secondary action with index " + buttonIndex);
    EventUtils.synthesizeMouseAtCenter(notification.childNodes[buttonIndex - 1], {});
  }
  await popuphidden;
}

registerCleanupFunction(async function() {
  let addresses = await getAddresses();
  if (addresses.length) {
    await removeAddresses(addresses.map(address => address.guid));
  }
});

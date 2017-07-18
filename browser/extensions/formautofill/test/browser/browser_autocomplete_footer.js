"use strict";

const URL = BASE_URL + "autocomplete_basic.html";
const PRIVACY_PREF_URL = "about:preferences#privacy";

async function expectWarningText(browser, expectedText) {
  const {autoCompletePopup: {richlistbox: itemsBox}} = browser;
  const warningBox = itemsBox.querySelector(".autocomplete-richlistitem:last-child")._warningTextBox;

  await BrowserTestUtils.waitForCondition(() => {
    return warningBox.textContent == expectedText;
  }, `Waiting for expected warning text: ${expectedText}, Got ${warningBox.textContent}`);
  ok(true, `Got expected warning text: ${expectedText}`);
}

add_task(async function setup_storage() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);
});

add_task(async function test_click_on_footer() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    const {autoCompletePopup, autoCompletePopup: {richlistbox: itemsBox}} = browser;

    await openPopupOn(browser, "#organization");
    // Click on the footer
    const optionButton = itemsBox.querySelector(".autocomplete-richlistitem:last-child")._optionButton;
    const prefTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, PRIVACY_PREF_URL);
    await EventUtils.synthesizeMouseAtCenter(optionButton, {});
    await BrowserTestUtils.removeTab(await prefTabPromise);
    ok(true, "Tab: preferences#privacy was successfully opened by clicking on the footer");

    // Ensure the popup is closed before entering the next test.
    await ContentTask.spawn(browser, {}, async function() {
      content.document.getElementById("organization").blur();
    });
    await BrowserTestUtils.waitForCondition(() => !autoCompletePopup.popupOpen);
  });
});

add_task(async function test_press_enter_on_footer() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    const {autoCompletePopup, autoCompletePopup: {richlistbox: itemsBox}} = browser;

    await openPopupOn(browser, "#organization");
    // Navigate to the footer and press enter.
    const listItemElems = itemsBox.querySelectorAll(".autocomplete-richlistitem");
    const prefTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, PRIVACY_PREF_URL);
    for (let i = 0; i < listItemElems.length; i++) {
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    }
    await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
    await BrowserTestUtils.removeTab(await prefTabPromise);
    ok(true, "Tab: preferences#privacy was successfully opened by pressing enter on the footer");

    // Ensure the popup is closed before entering the next test.
    await ContentTask.spawn(browser, {}, async function() {
      content.document.getElementById("organization").blur();
    });
    await BrowserTestUtils.waitForCondition(() => !autoCompletePopup.popupOpen);
  });
});

add_task(async function test_phishing_warning() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    const {autoCompletePopup, autoCompletePopup: {richlistbox: itemsBox}} = browser;

    await openPopupOn(browser, "#street-address");
    const warningBox = itemsBox.querySelector(".autocomplete-richlistitem:last-child")._warningTextBox;
    ok(warningBox, "Got phishing warning box");
    await expectWarningText(browser, "Also autofills company, phone, email");
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectWarningText(browser, "Also autofills company, phone, email");
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectWarningText(browser, "Autofills address");
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectWarningText(browser, "Also autofills company, phone, email");

    // Ensure the popup is closed before entering the next test.
    await ContentTask.spawn(browser, {}, async function() {
      content.document.getElementById("street-address").blur();
    });
    await BrowserTestUtils.waitForCondition(() => !autoCompletePopup.popupOpen);
  });
});

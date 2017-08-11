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
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);
  await saveAddress(TEST_ADDRESS_4);
  await saveAddress(TEST_ADDRESS_5);
});

add_task(async function test_click_on_footer() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    const {autoCompletePopup: {richlistbox: itemsBox}} = browser;

    await openPopupOn(browser, "#organization");
    // Click on the footer
    const optionButton = itemsBox.querySelector(".autocomplete-richlistitem:last-child")._optionButton;
    const prefTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, PRIVACY_PREF_URL);
    await EventUtils.synthesizeMouseAtCenter(optionButton, {});
    await BrowserTestUtils.removeTab(await prefTabPromise);
    ok(true, "Tab: preferences#privacy was successfully opened by clicking on the footer");

    await closePopup(browser);
  });
});

add_task(async function test_press_enter_on_footer() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    const {autoCompletePopup: {richlistbox: itemsBox}} = browser;

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

    await closePopup(browser);
  });
});

add_task(async function test_phishing_warning_single_category() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    const {autoCompletePopup: {richlistbox: itemsBox}} = browser;

    await openPopupOn(browser, "#tel");
    const warningBox = itemsBox.querySelector(".autocomplete-richlistitem:last-child")._warningTextBox;
    ok(warningBox, "Got phishing warning box");

    await expectWarningText(browser, "Autofills phone");

    await closePopup(browser);
  });
});

add_task(async function test_phishing_warning_complex_categories() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    await openPopupOn(browser, "#street-address");

    await expectWarningText(browser, "Also autofills company, email");
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectWarningText(browser, "Autofills address");
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectWarningText(browser, "Also autofills company, email");
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectWarningText(browser, "Also autofills company, email");

    await closePopup(browser);
  });
});

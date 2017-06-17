"use strict";

const URL = BASE_URL + "autocomplete_basic.html";
const PRIVACY_PREF_URL = "about:preferences#privacy";

add_task(async function setup_storage() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);
});

add_task(async function test_click_on_footer() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    const {autoCompletePopup, autoCompletePopup: {richlistbox: itemsBox}} = browser;

    await ContentTask.spawn(browser, {}, async function() {
      content.document.getElementById("organization").focus();
    });
    await sleep(2000);
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectPopupOpen(browser);

    // Click on the footer
    const listItemElems = itemsBox.querySelectorAll(".autocomplete-richlistitem");
    const prefTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, PRIVACY_PREF_URL);
    await EventUtils.synthesizeMouseAtCenter(listItemElems[listItemElems.length - 1], {});
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
    const {autoCompletePopup: {richlistbox: itemsBox}} = browser;

    await ContentTask.spawn(browser, {}, async function() {
      const input = content.document.getElementById("organization");
      input.focus();
    });
    await sleep(2000);
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectPopupOpen(browser);

    // Navigate to the footer and press enter.
    const listItemElems = itemsBox.querySelectorAll(".autocomplete-richlistitem");
    const prefTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, PRIVACY_PREF_URL);
    for (let i = 0; i < listItemElems.length; i++) {
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    }
    await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
    await BrowserTestUtils.removeTab(await prefTabPromise);
    ok(true, "Tab: preferences#privacy was successfully opened by pressing enter on the footer");
  });
});

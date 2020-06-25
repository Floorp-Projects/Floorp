"use strict";

const URL =
  "http://example.org/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";

add_task(async function setup_storage() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);
});

add_task(async function test_active_delay() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(
    browser
  ) {
    const focusInput = "#organization";

    // Open the popup -- we don't use openPopupOn() because there
    // are things we need to check between these steps.
    await SimpleTest.promiseFocus(browser);
    await focusAndWaitForFieldsIdentified(browser, focusInput);
    const start = Date.now();
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await expectPopupOpen(browser);
    const firstItem = getDisplayedPopupItems(browser)[0];
    ok(firstItem.disabled, "Popup should be disbled upon opening.");
    is(
      browser.autoCompletePopup.selectedIndex,
      -1,
      "No item selected at first"
    );

    // Check that clicking on menu doesn't do anything while
    // it is disabled
    firstItem.click();
    is(
      browser.autoCompletePopup.selectedIndex,
      -1,
      "No item selected after clicking on disabled item"
    );

    // Check that the delay before enabling is as long as expected
    await waitForPopupEnabled(browser);
    const delta = Date.now() - start;
    info(`Popup was disabled for ${delta} ms`);
    ok(delta >= 500, "Popup was disabled for at least 500 ms");

    // Check the clicking on the menu works now
    firstItem.click();
    is(
      browser.autoCompletePopup.selectedIndex,
      0,
      "First item selected after clicking on enabled item"
    );

    // Clean up
    await closePopup(browser);
  });
});

"use strict";

const ADDRESS_URL =
  "http://example.org/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";
const CC_URL =
  "https://example.org/browser/browser/extensions/formautofill/test/browser/creditCard/autocomplete_creditcard_basic.html";

add_task(async function setup_storage() {
  await setStorage(
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
    TEST_ADDRESS_3,
    TEST_CREDIT_CARD_1,
    TEST_CREDIT_CARD_2,
    TEST_CREDIT_CARD_3
  );
});

add_task(async function test_active_delay() {
  // This is a workaround for the fact that we don't have a way
  // to know when the popup was opened exactly and this makes our test
  // racy when ensuring that we first test for disabled items before
  // the delayed enabling happens.
  //
  // In the future we should consider adding an event when a popup
  // gets opened and listen for it in this test before we check if the item
  // is disabled.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.notification_enable_delay", 1000],
      ["extensions.formautofill.reauth.enabled", false],
    ],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CC_URL },
    async function (browser) {
      const focusInput = "#cc-number";

      // Open the popup -- we don't use openPopupOn() because there
      // are things we need to check between these steps.
      await SimpleTest.promiseFocus(browser);
      const start = performance.now();
      await runAndWaitForAutocompletePopupOpen(browser, async () => {
        await focusAndWaitForFieldsIdentified(browser, focusInput);
      });
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
      const delta = performance.now() - start;
      info(`Popup was disabled for ${delta} ms`);
      Assert.greaterOrEqual(
        delta,
        1000,
        "Popup was disabled for at least 1000 ms"
      );

      // Check the clicking on the menu works now
      firstItem.click();
      is(
        browser.autoCompletePopup.selectedIndex,
        0,
        "First item selected after clicking on enabled item"
      );

      // Clean up
      await closePopup(browser);
    }
  );
});

add_task(async function test_no_delay() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.notification_enable_delay", 1000],
      ["extensions.formautofill.reauth.enabled", false],
    ],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_URL },
    async function (browser) {
      const focusInput = "#organization";

      // Open the popup -- we don't use openPopupOn() because there
      // are things we need to check between these steps.
      await SimpleTest.promiseFocus(browser);
      await runAndWaitForAutocompletePopupOpen(browser, async () => {
        await focusAndWaitForFieldsIdentified(browser, focusInput);
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      });
      const firstItem = getDisplayedPopupItems(browser)[0];
      ok(!firstItem.disabled, "Popup should be enabled upon opening.");
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
        0,
        "First item selected after clicking on enabled item"
      );

      // Clean up
      await closePopup(browser);
    }
  );
});

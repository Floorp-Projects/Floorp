"use strict";

const ADDRESS_VALUES = {
  "#given-name": "Test User",
  "#organization": "Sesame Street",
  "#street-address": "123 Sesame Street",
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.heuristics.captureOnPageNavigation", true],
    ],
  });
});

/**
 * Tests if the address is captured (address doorhanger is shown)
 * after adding an entry to the browser's session history stack
 */
add_task(async function test_address_captured_after_changing_request_state() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false
      );

      info("Change request state");
      await SpecialPowers.spawn(browser, [], () => {
        const historyPushStateButton =
          content.document.getElementById("historyPushState");
        historyPushStateButton.click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

/**
 * Tests if the address is captured (address doorhanger is shown)
 * after navigating by opening another resource
 */
add_task(async function test_address_captured_after_navigation_same_window() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false
      );

      info("Navigate with window.location");
      await SpecialPowers.spawn(browser, [], () => {
        const windowLocationButton =
          content.document.getElementById("windowLocation");
        windowLocationButton.click();
      });

      info("Wait for address doorhanger");
      await onPopupShown;

      ok(true, "Address doorhanger is shown");
    }
  );
});

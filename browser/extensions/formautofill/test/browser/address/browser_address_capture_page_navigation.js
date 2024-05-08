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
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: ADDRESS_VALUES,
        },
        false // We don't submit the form
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
        false // We don't submit the form
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

/**
 * Test that a form submission is infered only once.
 */
add_task(async function test_form_submission_infered_only_once() {
  await setStorage(TEST_ADDRESS_1);

  let onUsed = waitForStorageChangedEvents("notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
    async function (browser) {
      // Progress listener is added on address field identification
      await openPopupOn(browser, "form #given-name");

      info("Fill address input fields without changing the values");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

      info("Submit form");
      await SpecialPowers.spawn(browser, [], async function () {
        // Progress listener is removed after form submission
        let form = content.document.getElementById("form");
        form.querySelector("input[type=submit]").click();
      });
    }
  );
  await onUsed;

  const addresses = await getAddresses();

  is(
    addresses[0].timesUsed,
    1,
    "timesUsed field set to 1, so form submission was only infered once"
  );
  await removeAllRecords();
});

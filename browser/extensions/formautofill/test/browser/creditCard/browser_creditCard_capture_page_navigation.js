"use strict";

const CC_VALUES = {
  "#cc-name": "User",
  "#cc-number": "5577000055770004",
  "#cc-exp-month": 12,
  "#cc-exp-year": 2017,
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
      ["extensions.formautofill.heuristics.captureOnPageNavigation", true],
    ],
  });
});

/**
 * Tests if the credit card is captured (cc doorhanger is shown)
 * after adding an entry to the browser's session history stack
 */
add_task(
  async function test_creditCard_captured_after_changing_request_state() {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
      async function (browser) {
        const onPopupShown = waitForPopupShown();
        info("Update identified credit card fields");
        // We don't submit the form
        await focusUpdateSubmitForm(
          browser,
          {
            focusSelector: "#cc-name",
            newValues: CC_VALUES,
          },
          false
        );

        info("Change request state");
        await SpecialPowers.spawn(browser, [], () => {
          const historyPushStateButton =
            content.document.getElementById("historyPushState");
          historyPushStateButton.click();
        });

        info("Wait for credit card doorhanger");
        await onPopupShown;

        ok(true, "Credit card doorhanger is shown");
      }
    );
  }
);

/**
 * Tests if the credit card is captured (cc doorhanger is shown) after a
 * after navigating by opening another resource
 */
add_task(
  async function test_creditCard_captured_after_navigation_same_window() {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_WITH_PAGE_NAVIGATION_BUTTONS },
      async function (browser) {
        const onPopupShown = waitForPopupShown();
        info("Update identified credit card fields");
        // We don't submit the form
        await focusUpdateSubmitForm(
          browser,
          {
            focusSelector: "#cc-name",
            newValues: CC_VALUES,
          },
          false
        );

        info("Change window location");
        await SpecialPowers.spawn(browser, [], () => {
          const windowLocationButton =
            content.document.getElementById("windowLocation");
          windowLocationButton.click();
        });

        info("Wait for credit card doorhanger");
        await onPopupShown;

        ok(true, "Credit card doorhanger is shown");
      }
    );
  }
);

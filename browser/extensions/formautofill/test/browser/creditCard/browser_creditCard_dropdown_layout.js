"use strict";

const CC_URL =
  "https://example.org/browser/browser/extensions/formautofill/test/browser/creditCard/autocomplete_creditcard_basic.html";

add_task(async function setup_storage() {
  await setStorage(TEST_CREDIT_CARD_1, TEST_CREDIT_CARD_2, TEST_CREDIT_CARD_3);
});

async function reopenPopupWithResizedInput(browser, selector, newSize) {
  await closePopup(browser);
  /* eslint no-shadow: ["error", { "allow": ["selector", "newSize"] }] */
  await SpecialPowers.spawn(
    browser,
    [{ selector, newSize }],
    async function ({ selector, newSize }) {
      const input = content.document.querySelector(selector);

      input.style.boxSizing = "border-box";
      input.style.width = newSize + "px";
    }
  );
  await openPopupOn(browser, selector);
}

add_task(async function test_credit_card_dropdown() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CC_URL },
    async function (browser) {
      const focusInput = "#cc-number";
      await openPopupOn(browser, focusInput);
      const firstItem = getDisplayedPopupItems(browser)[0];

      isnot(firstItem.getAttribute("ac-image"), "", "Should show icon");
      ok(
        firstItem.getAttribute("aria-label").startsWith("Visa "),
        "aria-label should start with Visa"
      );

      // The breakpoint of two-lines layout is 185px
      await reopenPopupWithResizedInput(browser, focusInput, 175);
      is(
        firstItem._itemBox.getAttribute("size"),
        "small",
        "Show two-lines layout"
      );
      await reopenPopupWithResizedInput(browser, focusInput, 195);
      is(
        firstItem._itemBox.hasAttribute("size"),
        false,
        "Show one-line layout"
      );

      await closePopup(browser);
    }
  );
});

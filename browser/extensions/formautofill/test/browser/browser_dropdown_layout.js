"use strict";

const URL =
  "http://example.org/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";

add_task(async function setup_storage() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);
});

async function reopenPopupWithResizedInput(browser, selector, newSize) {
  await closePopup(browser);
  /* eslint no-shadow: ["error", { "allow": ["selector", "newSize"] }] */
  await SpecialPowers.spawn(browser, [{ selector, newSize }], async function({
    selector,
    newSize,
  }) {
    const input = content.document.querySelector(selector);

    input.style.boxSizing = "border-box";
    input.style.width = newSize + "px";
  });
  await openPopupOn(browser, selector);
}

add_task(async function test_address_dropdown() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(
    browser
  ) {
    const focusInput = "#organization";
    await openPopupOn(browser, focusInput);
    const firstItem = getDisplayedPopupItems(browser)[0];

    is(firstItem.getAttribute("ac-image"), "", "Should not show icon");

    // The breakpoint of two-lines layout is 150px
    await reopenPopupWithResizedInput(browser, focusInput, 140);
    is(
      firstItem._itemBox.getAttribute("size"),
      "small",
      "Show two-lines layout"
    );
    await reopenPopupWithResizedInput(browser, focusInput, 160);
    is(firstItem._itemBox.hasAttribute("size"), false, "Show one-line layout");

    await closePopup(browser);
  });
});

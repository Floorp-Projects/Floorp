"use strict";

const TEST_URL_PATH_CC = "://example.org/browser/browser/extensions/formautofill/test/browser/autocomplete_creditcard_basic.html";
const TEST_URL_PATH = "://example.org/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";

add_task(async function setup_storage() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);

  await saveCreditCard(TEST_CREDIT_CARD_1);
  await saveCreditCard(TEST_CREDIT_CARD_2);
  await saveCreditCard(TEST_CREDIT_CARD_3);
});

add_task(async function test_insecure_form() {
  async function runTest({urlPath, protocol, focusInput, expectedType, expectedResultLength}) {
    await BrowserTestUtils.withNewTab({gBrowser, url: protocol + urlPath}, async function(browser) {
      await openPopupOn(browser, focusInput);

      const items = getDisplayedPopupItems(browser);
      is(items.length, expectedResultLength, `Should show correct amount of results in "${protocol}"`);
      const firstItem = items[0];
      is(firstItem.getAttribute("originaltype"), expectedType, `Item should attach with correct binding in "${protocol}"`);

      await closePopup(browser);
    });
  }

  const testSets = [{
    urlPath: TEST_URL_PATH,
    protocol: "https",
    focusInput: "#organization",
    expectedType: "autofill-profile",
    expectedResultLength: 2,
  }, {
    urlPath: TEST_URL_PATH,
    protocol: "http",
    focusInput: "#organization",
    expectedType: "autofill-profile",
    expectedResultLength: 2,
  }, {
    urlPath: TEST_URL_PATH_CC,
    protocol: "https",
    focusInput: "#cc-name",
    expectedType: "autofill-profile",
    expectedResultLength: 3,
  }, {
    urlPath: TEST_URL_PATH_CC,
    protocol: "http",
    focusInput: "#cc-name",
    expectedType: "autofill-insecureWarning", // insecure warning field
    expectedResultLength: 1,
  }];

  for (const test of testSets) {
    await runTest(test);
  }
});

add_task(async function test_click_on_insecure_warning() {
  await BrowserTestUtils.withNewTab({gBrowser, url: "http" + TEST_URL_PATH_CC}, async function(browser) {
    await openPopupOn(browser, "#cc-name");

    const insecureItem = getDisplayedPopupItems(browser)[0];
    await EventUtils.synthesizeMouseAtCenter(insecureItem, {});
    // Check input's value after popup closed to ensure the completion of autofilling.
    await expectPopupClose(browser);
    const inputValue = await ContentTask.spawn(browser, {}, async function() {
      return content.document.querySelector("#cc-name").value;
    });
    is(inputValue, "");

    await closePopup(browser);
  });
});

add_task(async function test_press_enter_on_insecure_warning() {
  await BrowserTestUtils.withNewTab({gBrowser, url: "http" + TEST_URL_PATH_CC}, async function(browser) {
    await openPopupOn(browser, "#cc-name");

    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
    // Check input's value after popup closed to ensure the completion of autofilling.
    await expectPopupClose(browser);
    const inputValue = await ContentTask.spawn(browser, {}, async function() {
      return content.document.querySelector("#cc-name").value;
    });
    is(inputValue, "");

    await closePopup(browser);
  });
});

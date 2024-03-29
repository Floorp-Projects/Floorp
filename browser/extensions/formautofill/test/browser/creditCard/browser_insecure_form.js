"use strict";

// Remove the scheme from the URLs so we can switch between http: and https: later.
const TEST_URL_PATH_CC =
  "://example.org" +
  HTTP_TEST_PATH +
  "creditCard/autocomplete_creditcard_basic.html";
const TEST_URL_PATH =
  "://example.org" + HTTP_TEST_PATH + "autocomplete_basic.html";

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

add_task(async function test_insecure_form() {
  async function runTest({
    urlPath,
    protocol,
    focusInput,
    expectedType,
    expectedResultLength,
  }) {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: protocol + urlPath },
      async function (browser) {
        await openPopupOn(browser, focusInput);

        const items = getDisplayedPopupItems(browser);
        is(
          items.length,
          expectedResultLength,
          `Should show correct amount of results in "${protocol}"`
        );
        const firstItem = items[0];
        is(
          firstItem.getAttribute("originaltype"),
          expectedType,
          `Item should attach with correct binding in "${protocol}"`
        );

        await closePopup(browser);
      }
    );
  }

  const testSets = [
    {
      urlPath: TEST_URL_PATH,
      protocol: "https",
      focusInput: "#organization",
      expectedType: "autofill",
      expectedResultLength: 3, // add one for the status row
    },
    {
      urlPath: TEST_URL_PATH,
      protocol: "http",
      focusInput: "#organization",
      expectedType: "autofill",
      expectedResultLength: 3, // add one for the status row
    },
    {
      urlPath: TEST_URL_PATH_CC,
      protocol: "https",
      focusInput: "#cc-name",
      expectedType: "autofill",
      expectedResultLength: 3, // no status row here
    },
    {
      urlPath: TEST_URL_PATH_CC,
      protocol: "http",
      focusInput: "#cc-name",
      expectedType: "insecureWarning", // insecure warning field
      expectedResultLength: 1,
    },
  ];

  for (const test of testSets) {
    await runTest(test);
  }
});

add_task(async function test_click_on_insecure_warning() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "http" + TEST_URL_PATH_CC },
    async function (browser) {
      await openPopupOn(browser, "#cc-name");
      const insecureItem = getDisplayedPopupItems(browser)[0];
      let popupClosePromise = BrowserTestUtils.waitForPopupEvent(
        browser.autoCompletePopup,
        "hidden"
      );
      await EventUtils.synthesizeMouseAtCenter(insecureItem, {});
      // Check input's value after popup closed to ensure the completion of autofilling.
      await popupClosePromise;

      const inputValue = await SpecialPowers.spawn(
        browser,
        [],
        async function () {
          return content.document.querySelector("#cc-name").value;
        }
      );
      is(inputValue, "");

      await closePopup(browser);
    }
  );
});

add_task(async function test_press_enter_on_insecure_warning() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "http" + TEST_URL_PATH_CC },
    async function (browser) {
      await openPopupOn(browser, "#cc-name");

      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);

      let popupClosePromise = BrowserTestUtils.waitForPopupEvent(
        browser.autoCompletePopup,
        "hidden"
      );
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      // Check input's value after popup closed to ensure the completion of autofilling.
      await popupClosePromise;

      const inputValue = await SpecialPowers.spawn(
        browser,
        [],
        async function () {
          return content.document.querySelector("#cc-name").value;
        }
      );
      is(inputValue, "");

      await closePopup(browser);
    }
  );
});

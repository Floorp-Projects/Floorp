/**
 * Test that autofill autocomplete works after back/forward navigation
 */

"use strict";

const URL = BASE_URL + "autocomplete_basic.html";

function checkPopup(autoCompletePopup) {
  let first = autoCompletePopup.view.results[0];
  const { primary, secondary } = JSON.parse(first.label);
  ok(
    primary.startsWith(TEST_ADDRESS_1["street-address"].split("\n")[0]),
    "Check primary label is street address"
  );
  is(
    secondary,
    TEST_ADDRESS_1["address-level2"],
    "Check secondary label is address-level2"
  );
}

add_task(async function setup_storage() {
  await setStorage(TEST_ADDRESS_1, TEST_ADDRESS_2, TEST_ADDRESS_3);
});

add_task(async function test_back_forward() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      const { autoCompletePopup } = browser;

      // Check the page after the initial load
      await openPopupOn(browser, "#street-address");
      checkPopup(autoCompletePopup);

      // Now navigate forward and make sure autofill autocomplete results are still attached
      let loadPromise = BrowserTestUtils.browserLoaded(browser);
      BrowserTestUtils.startLoadingURIString(browser, `${URL}?load=2`);
      info("expecting browser loaded");
      await loadPromise;

      // Check the second page
      await openPopupOn(browser, "#street-address");
      checkPopup(autoCompletePopup);

      // Check after hitting back to the first page
      let stoppedPromise = BrowserTestUtils.browserStopped(browser);
      browser.goBack();
      info("expecting browser stopped");
      await stoppedPromise;
      await openPopupOn(browser, "#street-address");
      checkPopup(autoCompletePopup);

      // Check after hitting forward to the second page
      stoppedPromise = BrowserTestUtils.browserStopped(browser);
      browser.goForward();
      info("expecting browser stopped");
      await stoppedPromise;
      await openPopupOn(browser, "#street-address");
      checkPopup(autoCompletePopup);

      await closePopup(browser);
    }
  );
});

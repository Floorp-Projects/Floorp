/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable no-unused-vars */
function testVisibility(browser, expected) {
  const { document } = browser.contentWindow;
  for (let [selector, shouldBeVisible] of Object.entries(
    expected.expectedVisible
  )) {
    const elem = document.querySelector(selector);
    if (shouldBeVisible) {
      ok(
        BrowserTestUtils.is_visible(elem),
        `Expected ${selector} to be visible`
      );
    } else {
      ok(BrowserTestUtils.is_hidden(elem), `Expected ${selector} to be hidden`);
    }
  }
}

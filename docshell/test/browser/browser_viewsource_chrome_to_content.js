/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_URI = `view-source:${TEST_PATH}dummy_page.html`;

add_task(async function chrome_to_content_view_source() {
  await BrowserTestUtils.withNewTab("about:mozilla", async browser => {
    is(browser.documentURI.spec, "about:mozilla");

    // This process switch would previously crash in debug builds due to assertion failures.
    BrowserTestUtils.startLoadingURIString(browser, TEST_URI);
    await BrowserTestUtils.browserLoaded(browser);
    is(browser.documentURI.spec, TEST_URI);
  });
});

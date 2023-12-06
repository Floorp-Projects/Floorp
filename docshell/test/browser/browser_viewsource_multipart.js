/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const MULTIPART_URI = `${TEST_PATH}file_basic_multipart.sjs`;

add_task(async function viewsource_multipart_uri() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    BrowserTestUtils.startLoadingURIString(browser, MULTIPART_URI);
    await BrowserTestUtils.browserLoaded(browser);
    is(browser.currentURI.spec, MULTIPART_URI);

    // Continue probing the URL until we find the h1 we're expecting. This
    // should handle cases where we somehow beat the second document having
    // loaded.
    await TestUtils.waitForCondition(async () => {
      let value = await SpecialPowers.spawn(browser, [], async () => {
        let headers = content.document.querySelectorAll("h1");
        is(headers.length, 1, "only one h1 should be present");
        return headers[0].textContent;
      });

      ok(value == "First" || value == "Second", "some other value was found?");
      return value == "Second";
    });

    // Load a view-source version of the page, which should show the full
    // content, not handling multipart.
    BrowserTestUtils.startLoadingURIString(
      browser,
      `view-source:${MULTIPART_URI}`
    );
    await BrowserTestUtils.browserLoaded(browser);

    let viewSourceContent = await SpecialPowers.spawn(browser, [], async () => {
      return content.document.body.textContent;
    });

    ok(viewSourceContent.includes("<h1>First</h1>"), "first header");
    ok(viewSourceContent.includes("<h1>Second</h1>"), "second header");
    ok(viewSourceContent.includes("BOUNDARY"), "boundary");
  });
});

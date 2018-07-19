/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* When a user clears the URL bar, and then the page pushes state, we should
 * re-fill the URL bar so it doesn't remain empty indefinitely. See bug 1441039.
 * For normal loads, this happens automatically because a non-same-document state
 * change takes place.
 */
add_task(async function() {
  const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
  await BrowserTestUtils.withNewTab(TEST_PATH + "dummy_page.html", async function(browser) {
    gURLBar.value = "";

    let locationChangePromise = BrowserTestUtils.waitForLocationChange(gBrowser, TEST_PATH + "dummy_page2.html");
    await ContentTask.spawn(browser, null, function() {
      content.history.pushState({}, "Page 2", "dummy_page2.html");
    });
    await locationChangePromise;
    ok(gURLBar.value, TEST_PATH + "dummy_page2.html", "Should have updated the URL bar.");
  });
});

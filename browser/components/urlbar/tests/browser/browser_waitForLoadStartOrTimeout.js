/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the waitForLoadStartOrTimeout test helper function in head.js.
 */

"use strict";

add_task(async function load() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let url = "https://example.com/";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: url,
    });

    let loadPromise = waitForLoadStartOrTimeout();
    EventUtils.synthesizeKey("KEY_Enter");
    let uri = await loadPromise;
    info("Page should have loaded before timeout");

    Assert.equal(uri.spec, url, "example.com should have loaded");
  });
});

add_task(async function timeout() {
  await Assert.rejects(
    waitForLoadStartOrTimeout(),
    /timed out/,
    "Should have timed out"
  );
});

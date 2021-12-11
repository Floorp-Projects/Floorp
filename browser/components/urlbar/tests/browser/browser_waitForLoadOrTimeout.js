/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the waitForLoadOrTimeout test helper function in head.js.
 */

"use strict";

add_task(async function load() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let url = "http://example.com/";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: url,
    });

    let loadPromise = waitForLoadOrTimeout();
    EventUtils.synthesizeKey("KEY_Enter");
    let loadEvent = await loadPromise;

    Assert.ok(loadEvent, "Page should have loaded before timeout");
    Assert.equal(
      loadEvent.target.currentURI.spec,
      url,
      "example.com should have loaded"
    );
  });
});

add_task(async function timeout() {
  let loadEvent = await waitForLoadOrTimeout();
  Assert.ok(
    !loadEvent,
    "No page should have loaded, and timeout should have fired"
  );
});

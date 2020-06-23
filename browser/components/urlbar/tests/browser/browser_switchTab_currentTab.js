/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures that switch to tab still works when the URI contains an
 * encoded part.
 */

"use strict";

add_task(async function test_switchTab_currentTab() {
  registerCleanupFunction(PlacesUtils.history.clear);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:robots#1" },
    async () => {
      await BrowserTestUtils.withNewTab(
        { gBrowser, url: "about:robots#2" },
        async () => {
          let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
            window,
            value: "robot",
          });
          Assert.ok(
            context.results.some(
              result =>
                result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH &&
                result.payload.url == "about:robots#1"
            )
          );
          Assert.ok(
            !context.results.some(
              result =>
                result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH &&
                result.payload.url == "about:robots#2"
            )
          );
        }
      );
    }
  );
});

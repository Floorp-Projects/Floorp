/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_setup() {
  let originalIoOffline = Services.io.offline;
  Services.io.offline = true;

  registerCleanupFunction(() => {
    Services.io.offline = originalIoOffline;
  });
});

/**
 * Tests that only the loading state appears when there is no network connection.
 */
add_task(async function test_offline_warning() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let shoppingContainer = await getAnalysisDetails(browser, null);

      ok(shoppingContainer.isOffline, "Offline status detected");
      ok(shoppingContainer.loadingEl, "Render loading state");
      verifyAnalysisDetailsHidden(shoppingContainer);
      verifyFooterHidden(shoppingContainer);
    }
  );
});

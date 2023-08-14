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
 * Tests that the correct shopping-message-bar component appears if there is no network connection.
 * Only settings should be visible.
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
      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarType,
        "offline",
        "shopping-message-bar type should be correct"
      );

      verifyAnalysisDetailsHidden(shoppingContainer);

      ok(shoppingContainer.settingsEl, "Got the shopping-settings element");
    }
  );
});

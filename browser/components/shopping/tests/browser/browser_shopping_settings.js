/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the settings component is rendered as expected.
 */
add_task(async function test_shopping_settings() {
  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = MOCK_POPULATED_DATA;
      await shoppingContainer.updateComplete;

      let shoppingSettings = shoppingContainer.settingsEl;
      await shoppingSettings.updateComplete;
      ok(shoppingSettings, "Got the shopping-settings element");

      let toggle = shoppingSettings.recommendationsToggleEl;
      ok(toggle, "There should be a toggle");

      let optOutButton = shoppingSettings.optOutButtonEl;
      ok(optOutButton, "There should be an opt-out button");
    }
  );
});

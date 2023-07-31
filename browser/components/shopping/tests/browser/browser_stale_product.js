/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the correct shopping-message-bar component appears if a product analysis is stale.
 * Other analysis details should be visible.
 */
add_task(async function test_stale_product() {
  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = MOCK_STALE_PRODUCT_RESPONSE;
      await shoppingContainer.updateComplete;

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
        "stale",
        "shopping-message-bar type should be correct"
      );

      verifyAnalysisDetailsVisible(shoppingContainer);

      ok(shoppingContainer.settingsEl, "Got the shopping-settings element");
    }
  );
});

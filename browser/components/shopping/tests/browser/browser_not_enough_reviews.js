/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the correct shopping-message-bar component appears if there are not enough
 * reviews to provided an analysis.
 * Settings should be the only other component that is visible.
 */
add_task(async function test_not_enough_reviews() {
  await BrowserTestUtils.withNewTab(
    {
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = MOCK_NOT_ENOUGH_REVIEWS_PRODUCT_RESPONSE;
      await shoppingContainer.updateComplete;

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
        "not-enough-reviews",
        "shopping-message-bar type should be correct"
      );

      verifyAnalysisDetailsHidden(shoppingContainer);

      ok(shoppingContainer.settingsEl, "Got the shopping-settings element");
    }
  );
});

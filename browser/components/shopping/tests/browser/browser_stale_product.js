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
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let shoppingContainer = await getAnalysisDetails(
        browser,
        MOCK_STALE_PRODUCT_RESPONSE
      );

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarType,
        "stale",
        "shopping-message-bar type should be correct"
      );

      verifyAnalysisDetailsVisible(shoppingContainer);
      verifyFooterVisible(shoppingContainer);
    }
  );
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the correct shopping-message-bar component appears if there are not enough
 * reviews to provided an analysis.
 * The footer should be visible.
 */
add_task(async function test_not_enough_reviews() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let shoppingContainer = await getAnalysisDetails(
        browser,
        MOCK_NOT_ENOUGH_REVIEWS_PRODUCT_RESPONSE
      );

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarType,
        "not-enough-reviews",
        "shopping-message-bar type should be correct"
      );

      verifyAnalysisDetailsHidden(shoppingContainer);
      verifyFooterVisible(shoppingContainer);
    }
  );
});

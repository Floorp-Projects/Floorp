/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the unanalyzed product card appears if a product has no analysis yet.
 * Settings should be the only other component that is visible.
 */
add_task(async function test_unanalyzed_product() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let shoppingContainer = await getAnalysisDetails(
        browser,
        MOCK_UNANALYZED_PRODUCT_RESPONSE
      );

      ok(
        shoppingContainer.unanalyzedProductEl,
        "Got the unanalyzed-product-card element"
      );

      verifyAnalysisDetailsHidden(shoppingContainer);
      verifyFooterVisible(shoppingContainer);
    }
  );
});

/**
 * Tests that the unanalyzed product card is hidden if a product already has an up-to-date analysis.
 * Other analysis details should be visible.
 */
add_task(async function test_analyzed_product() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let shoppingContainer = await getAnalysisDetails(
        browser,
        MOCK_ANALYZED_PRODUCT_RESPONSE
      );

      ok(
        !shoppingContainer.unanalyzedProductEl,
        "unanalyzed-product-card should not be visible"
      );

      verifyAnalysisDetailsVisible(shoppingContainer);
      verifyFooterVisible(shoppingContainer);
    }
  );
});

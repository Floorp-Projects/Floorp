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
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = MOCK_UNANALYZED_PRODUCT_RESPONSE;
      await shoppingContainer.updateComplete;

      ok(
        shoppingContainer.unanalyzedProductEl,
        "Got the unanalyzed-product-card element"
      );

      verifyAnalysisDetailsHidden(shoppingContainer);

      ok(shoppingContainer.settingsEl, "Got the shopping-settings element");
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
      url: "chrome://browser/content/shopping/shopping.html",
      gBrowser,
    },
    async browser => {
      const { document } = browser.contentWindow;

      let shoppingContainer = document.querySelector("shopping-container");
      shoppingContainer.data = MOCK_ANALYZED_PRODUCT_RESPONSE;
      await shoppingContainer.updateComplete;

      ok(
        !shoppingContainer.unanalyzedProductEl,
        "unanalyzed-product-card should not be visible"
      );

      verifyAnalysisDetailsVisible(shoppingContainer);

      ok(shoppingContainer.settingsEl, "Got the shopping-settings element");
    }
  );
});

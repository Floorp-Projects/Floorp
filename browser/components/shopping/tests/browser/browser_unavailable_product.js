/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the correct shopping-message-bar component appears if a product was marked as unavailable.
 */
add_task(async function test_unavailable_product() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_UNAVAILABLE_PRODUCT_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let productNotAvailableMessageBar =
            shoppingContainer.shoppingMessageBarEl;

          ok(productNotAvailableMessageBar, "Got shopping-message-bar element");
          is(
            productNotAvailableMessageBar?.getAttribute("type"),
            "product-not-available",
            "shopping-message-bar type should be correct"
          );

          let productAvailableBtn =
            productNotAvailableMessageBar?.productAvailableBtnEl;

          ok(productAvailableBtn, "Got report product available button");

          let thanksForReportMessageBarVisible =
            ContentTaskUtils.waitForCondition(() => {
              return (
                !!shoppingContainer.shoppingMessageBarEl &&
                ContentTaskUtils.is_visible(
                  shoppingContainer.shoppingMessageBarEl
                )
              );
            }, "Waiting for shopping-message-bar to be visible");

          productAvailableBtn.click();

          await thanksForReportMessageBarVisible;

          is(
            shoppingContainer.shoppingMessageBarEl?.getAttribute("type"),
            "thanks-for-reporting",
            "shopping-message-bar type should be correct"
          );
        }
      );
    }
  );
});

/**
 * Tests that the correct shopping-message-bar component appears if a product marked as unavailable
 * was reported to be back in stock by another user.
 */
add_task(async function test_unavailable_product_reported() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_UNAVAILABLE_PRODUCT_REPORTED_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          ok(
            shoppingContainer.shoppingMessageBarEl,
            "Got shopping-message-bar element"
          );
          is(
            shoppingContainer.shoppingMessageBarEl?.getAttribute("type"),
            "product-not-available-reported",
            "shopping-message-bar type should be correct"
          );
        }
      );
    }
  );
});

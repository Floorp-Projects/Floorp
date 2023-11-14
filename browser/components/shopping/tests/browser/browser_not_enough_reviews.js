/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the unanalyzed card is shown when not_enough_reviews is not present.

add_task(async function test_show_unanalyzed_on_initial_load() {
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
        "Got unanalyzed card on first try"
      );

      verifyAnalysisDetailsHidden(shoppingContainer);
      verifyFooterVisible(shoppingContainer);
    }
  );
});

// Tests that the not enough reviews card is shown when not_enough_reviews is true.

add_task(async function test_show_not_enough_reviews() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },

    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_NOT_ENOUGH_REVIEWS_PRODUCT_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;

          shoppingContainer.data = Cu.cloneInto(mockData, content);

          let messageBarVisiblePromise = ContentTaskUtils.waitForCondition(
            () => {
              return (
                !!shoppingContainer.shoppingMessageBarEl &&
                ContentTaskUtils.is_visible(
                  shoppingContainer.shoppingMessageBarEl
                )
              );
            },
            "Waiting for shopping-message-bar to be visible"
          );

          await messageBarVisiblePromise;
          await shoppingContainer.updateComplete;

          ok(
            shoppingContainer.shoppingMessageBarEl,
            "Got shopping-message-bar element"
          );

          is(
            shoppingContainer.shoppingMessageBarEl?.getAttribute("type"),
            "not-enough-reviews",
            "shopping-message-bar type should be correct"
          );
        }
      );
    }
  );
});

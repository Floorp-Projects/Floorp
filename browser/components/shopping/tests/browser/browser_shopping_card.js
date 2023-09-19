/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the chevron button's accessible name and state.
 */
add_task(async function test_chevron_button_markup() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_UNANALYZED_PRODUCT_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let shoppingSettings = content.document
            .querySelector("shopping-container")
            .shadowRoot.querySelector("shopping-settings");
          let shoppingCard =
            shoppingSettings.shadowRoot.querySelector("shopping-card");
          let detailsEl = shoppingCard.shadowRoot.querySelector("details");

          // Need to wait for different async events to complete on the lit component:
          await ContentTaskUtils.waitForCondition(() =>
            detailsEl.querySelector(".chevron-icon")
          );

          let chevronButton = detailsEl.querySelector(".chevron-icon");

          is(
            chevronButton.getAttribute("aria-labelledby"),
            "header",
            "The chevron button is has an accessible name"
          );
        }
      );
    }
  );
});

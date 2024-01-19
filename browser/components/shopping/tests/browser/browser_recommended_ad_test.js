/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_ads_requested_after_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.ads.enabled", true],
      ["browser.shopping.experience2023.ads.userEnabled", false],
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  await BrowserTestUtils.withNewTab(
    {
      url: PRODUCT_TEST_URL,
      gBrowser,
    },
    async browser => {
      let sidebar = gBrowser
        .getPanel(browser)
        .querySelector("shopping-sidebar");
      Assert.ok(sidebar, "Sidebar should exist");
      Assert.ok(
        BrowserTestUtils.isVisible(sidebar),
        "Sidebar should be visible."
      );
      info("Waiting for sidebar to update.");
      await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

      await SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          await shoppingContainer.updateComplete;

          Assert.ok(
            !shoppingContainer.recommendedAdEl,
            "Recommended card should not exist"
          );

          let shoppingSettings = shoppingContainer.settingsEl;
          await shoppingSettings.updateComplete;
          shoppingSettings.shoppingCardEl.detailsEl.open = true;

          let recommendationsToggle = shoppingSettings.recommendationsToggleEl;
          recommendationsToggle.click();

          await ContentTaskUtils.waitForCondition(() => {
            return shoppingContainer.recommendedAdEl;
          });

          await shoppingContainer.updateComplete;

          let recommendedCard = shoppingContainer.recommendedAdEl;
          await recommendedCard.updateComplete;
          Assert.ok(recommendedCard, "Recommended card should exist");
          Assert.ok(
            ContentTaskUtils.isVisible(recommendedCard),
            "Recommended card is visible"
          );
        }
      );
    }
  );
});

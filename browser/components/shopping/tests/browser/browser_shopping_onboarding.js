/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test to check onboarding message container is rendered
 * when user is not opted-in
 */
add_task(async function test_showOnboarding_notOptedIn() {
  // OptedIn pref Value is 0 when a user hasn't opted-in
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.optedIn", 0]],
  });
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector("shopping-container"),
          "shopping-container"
        );

        let containerElem =
          shoppingContainer.shadowRoot.getElementById("shopping-container");
        let messageSlot = containerElem.getElementsByTagName("slot");

        // Check multi-stage-message-slot used to show opt-In message
        // is rendered inside shopping container when user optedIn pref Value is 0
        ok(messageSlot.length, `message slot element exists`);
        is(
          messageSlot[0].name,
          "multi-stage-message-slot",
          "multi-stage-message-slot showing opt-in message rendered"
        );
      });
    }
  );
});

/**
 * Test to check onboarding message is not shown for opted-in users
 */
add_task(async function test_hideOnboarding_optedIn() {
  // OptedIn pref value is 1 for opted-in users
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.optedIn", 1]],
  });
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector("shopping-container"),
          "shopping-container"
        );

        let containerElem =
          shoppingContainer.shadowRoot.getElementById("shopping-container");
        let messageSlot = containerElem.getElementsByTagName("slot");

        ok(!messageSlot.length, `message slot element doesn't exist`);
      });
    }
  );
});

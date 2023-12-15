/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SHOPPING_SIDEBAR_WIDTH_PREF =
  "browser.shopping.experience2023.sidebarWidth";

add_task(async function test_sidebar_opens_correct_size() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
      ["browser.shopping.experience2023.active", true],
      [SHOPPING_SIDEBAR_WIDTH_PREF, 0],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: PRODUCT_TEST_URL,
  });

  let browserPanel = gBrowser.getPanel(tab.linkedBrowser);
  let sidebar = browserPanel.querySelector("shopping-sidebar");

  await TestUtils.waitForCondition(() => sidebar.scrollWidth === 320);

  is(sidebar.scrollWidth, 320, "Shopping sidebar should default to 320px");

  let prefChangedPromise = TestUtils.waitForPrefChange(
    SHOPPING_SIDEBAR_WIDTH_PREF
  );
  sidebar.style.width = "345px";
  await TestUtils.waitForCondition(() => sidebar.scrollWidth === 345);
  await prefChangedPromise;

  let shoppingButton = document.getElementById("shopping-sidebar-button");
  shoppingButton.click();
  await BrowserTestUtils.waitForMutationCondition(
    shoppingButton,
    {
      attributeFilter: ["shoppingsidebaropen"],
    },
    () => shoppingButton.getAttribute("shoppingsidebaropen") == "false"
  );

  shoppingButton.click();
  await BrowserTestUtils.waitForMutationCondition(
    shoppingButton,
    {
      attributeFilter: ["shoppingsidebaropen"],
    },
    () => shoppingButton.getAttribute("shoppingsidebaropen") == "true"
  );

  await TestUtils.waitForCondition(() => sidebar.scrollWidth === 345);

  is(
    sidebar.scrollWidth,
    345,
    "Shopping sidebar should open to previous set width of 345"
  );

  gBrowser.removeTab(tab);
});

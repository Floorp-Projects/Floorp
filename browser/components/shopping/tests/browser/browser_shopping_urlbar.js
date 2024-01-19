/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PAGE = "https://example.com";
const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";

add_task(async function test_button_hidden() {
  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "Shopping Button should be hidden on a content page"
    );
  });
});

add_task(async function test_button_shown() {
  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "Shopping Button should be visible on a product page"
    );
  });
});

// Button is hidden on navigation to a content page
add_task(async function test_button_changes_with_location() {
  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "Shopping Button should be hidden on a content page"
    );
    BrowserTestUtils.startLoadingURIString(browser, PRODUCT_PAGE);
    await BrowserTestUtils.browserLoaded(browser);
    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "Shopping Button should be visible on a product page"
    );
    BrowserTestUtils.startLoadingURIString(browser, CONTENT_PAGE);
    await BrowserTestUtils.browserLoaded(browser);
    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "Shopping Button should be hidden on a content page"
    );
  });
});

add_task(async function test_button_active() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", true);

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    Assert.equal(
      shoppingButton.getAttribute("shoppingsidebaropen"),
      "true",
      "Shopping Button should be active when sidebar is open"
    );
  });
});

add_task(async function test_button_inactive() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", false);

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    Assert.equal(
      shoppingButton.getAttribute("shoppingsidebaropen"),
      "false",
      "Shopping Button should be inactive when sidebar is closed"
    );
  });
});

// Switching Tabs shows and hides the button
add_task(async function test_button_changes_with_tabswitch() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", true);

  let shoppingButton = document.getElementById("shopping-sidebar-button");

  let productTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: PRODUCT_PAGE,
  });
  let contentTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: CONTENT_PAGE,
  });

  await BrowserTestUtils.switchTab(gBrowser, productTab);
  ok(
    BrowserTestUtils.isVisible(shoppingButton),
    "Shopping Button should be visible on a product page"
  );

  await BrowserTestUtils.switchTab(gBrowser, contentTab);
  ok(
    BrowserTestUtils.isHidden(shoppingButton),
    "Shopping Button should be hidden on a content page"
  );

  await BrowserTestUtils.removeTab(productTab);
  await BrowserTestUtils.removeTab(contentTab);
});

add_task(async function test_button_toggles_sidebars() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", false);

  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    let browserPanel = gBrowser.getPanel(browser);

    BrowserTestUtils.startLoadingURIString(browser, PRODUCT_PAGE);
    await BrowserTestUtils.browserLoaded(browser);

    let sidebar = browserPanel.querySelector("shopping-sidebar");

    is(sidebar, null, "Shopping sidebar should be closed");

    // open
    shoppingButton.click();
    await BrowserTestUtils.waitForMutationCondition(
      shoppingButton,
      {
        attributeFilter: ["shoppingsidebaropen"],
      },
      () => shoppingButton.getAttribute("shoppingsidebaropen") == "true"
    );

    sidebar = browserPanel.querySelector("shopping-sidebar");
    ok(BrowserTestUtils.isVisible(sidebar), "Shopping sidebar should be open");

    // close
    shoppingButton.click();
    await BrowserTestUtils.waitForMutationCondition(
      shoppingButton,
      {
        attributeFilter: ["shoppingsidebaropen"],
      },
      () => shoppingButton.getAttribute("shoppingsidebaropen") == "false"
    );

    ok(BrowserTestUtils.isHidden(sidebar), "Shopping sidebar should be closed");
  });
});

// Button changes all Windows
add_task(async function test_button_toggles_all_windows() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", false);

  let shoppingButton = document.getElementById("shopping-sidebar-button");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PRODUCT_PAGE);

  let browserPanelA = gBrowser.getPanel(gBrowser.selectedBrowser);
  let sidebarA = browserPanelA.querySelector("shopping-sidebar");

  let newWindow = await BrowserTestUtils.openNewBrowserWindow();

  BrowserTestUtils.startLoadingURIString(
    newWindow.gBrowser.selectedBrowser,
    PRODUCT_PAGE
  );
  await BrowserTestUtils.browserLoaded(newWindow.gBrowser.selectedBrowser);

  let browserPanelB = newWindow.gBrowser.getPanel(
    newWindow.gBrowser.selectedBrowser
  );
  let sidebarB = browserPanelB.querySelector("shopping-sidebar");

  is(
    sidebarA,
    null,
    "Shopping sidebar should not exist yet for new tab in current window"
  );
  is(sidebarB, null, "Shopping sidebar closed in new window");

  // open
  shoppingButton.click();
  await BrowserTestUtils.waitForMutationCondition(
    shoppingButton,
    {
      attributeFilter: ["shoppingsidebaropen"],
    },
    () => shoppingButton.getAttribute("shoppingsidebaropen") == "true"
  );
  sidebarA = browserPanelA.querySelector("shopping-sidebar");
  ok(
    BrowserTestUtils.isVisible(sidebarA),
    "Shopping sidebar should be open in current window"
  );
  sidebarB = browserPanelB.querySelector("shopping-sidebar");
  ok(
    BrowserTestUtils.isVisible(sidebarB),
    "Shopping sidebar should be open in new window"
  );

  // close
  shoppingButton.click();
  await BrowserTestUtils.waitForMutationCondition(
    shoppingButton,
    {
      attributeFilter: ["shoppingsidebaropen"],
    },
    () => shoppingButton.getAttribute("shoppingsidebaropen") == "false"
  );

  ok(
    BrowserTestUtils.isHidden(sidebarA),
    "Shopping sidebar should be closed in current window"
  );
  ok(
    BrowserTestUtils.isHidden(sidebarB),
    "Shopping sidebar should be closed in new window"
  );

  BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(newWindow);
});

add_task(async function test_button_right_click_doesnt_affect_sidebars() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", false);

  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    let browserPanel = gBrowser.getPanel(browser);

    BrowserTestUtils.startLoadingURIString(browser, PRODUCT_PAGE);
    await BrowserTestUtils.browserLoaded(browser);

    let sidebar = browserPanel.querySelector("shopping-sidebar");

    is(sidebar, null, "Shopping sidebar should be closed");
    EventUtils.synthesizeMouseAtCenter(shoppingButton, { button: 1 });
    // Wait a tick.
    await new Promise(executeSoon);
    sidebar = browserPanel.querySelector("shopping-sidebar");
    is(sidebar, null, "Shopping sidebar should still be closed");
  });
});

add_task(async function test_button_deals_with_tabswitches() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", true);

  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");

    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "The shopping button is hidden on a non product page"
    );

    let newProductTab = BrowserTestUtils.addTab(gBrowser, PRODUCT_PAGE);
    let newProductBrowser = newProductTab.linkedBrowser;
    await BrowserTestUtils.browserLoaded(
      newProductBrowser,
      false,
      PRODUCT_PAGE
    );

    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "The shopping button is still hidden after opening a background product tab"
    );

    let shoppingButtonVisiblePromise =
      BrowserTestUtils.waitForMutationCondition(
        shoppingButton,
        { attributes: true, attributeFilter: ["hidden"] },
        () => !shoppingButton.hidden
      );
    await BrowserTestUtils.switchTab(gBrowser, newProductTab);
    await shoppingButtonVisiblePromise;

    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "The shopping button is now visible"
    );

    let newProductTab2 = BrowserTestUtils.addTab(gBrowser, PRODUCT_PAGE);
    let newProductBrowser2 = newProductTab2.linkedBrowser;
    await BrowserTestUtils.browserLoaded(
      newProductBrowser2,
      false,
      PRODUCT_PAGE
    );

    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "The shopping button is still visible after opening background product tab"
    );

    shoppingButtonVisiblePromise = BrowserTestUtils.waitForMutationCondition(
      shoppingButton,
      { attributes: true, attributeFilter: ["hidden"] },
      () => !shoppingButton.hidden
    );
    await BrowserTestUtils.switchTab(gBrowser, newProductTab2);
    await shoppingButtonVisiblePromise;

    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "The shopping button is still visible"
    );

    BrowserTestUtils.removeTab(newProductTab2);

    BrowserTestUtils.removeTab(newProductTab);
  });
});

add_task(async function test_button_deals_with_tabswitches_post_optout() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", true);

  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");

    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "The shopping button is hidden on a non product page"
    );

    let newProductTab = BrowserTestUtils.addTab(gBrowser, PRODUCT_PAGE);
    let newProductBrowser = newProductTab.linkedBrowser;
    await BrowserTestUtils.browserLoaded(
      newProductBrowser,
      false,
      PRODUCT_PAGE
    );

    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "The shopping button is still hidden after opening a background product tab"
    );

    let shoppingButtonVisiblePromise =
      BrowserTestUtils.waitForMutationCondition(
        shoppingButton,
        { attributes: true, attributeFilter: ["hidden"] },
        () => !shoppingButton.hidden
      );
    await BrowserTestUtils.switchTab(gBrowser, newProductTab);
    await shoppingButtonVisiblePromise;

    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "The shopping button is now visible"
    );

    let newProductTab2 = BrowserTestUtils.addTab(gBrowser, PRODUCT_PAGE);
    let newProductBrowser2 = newProductTab2.linkedBrowser;
    await BrowserTestUtils.browserLoaded(
      newProductBrowser2,
      false,
      PRODUCT_PAGE
    );

    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "The shopping button is still visible after opening background product tab"
    );

    shoppingButtonVisiblePromise = BrowserTestUtils.waitForMutationCondition(
      shoppingButton,
      { attributes: true, attributeFilter: ["hidden"] },
      () => !shoppingButton.hidden
    );
    await BrowserTestUtils.switchTab(gBrowser, newProductTab2);
    await shoppingButtonVisiblePromise;

    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "The shopping button is still visible"
    );

    // Simulate opt-out
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.shopping.experience2023.active", false],
        ["browser.shopping.experience2023.optedIn", 2],
      ],
    });

    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "The shopping button is still visible after opting out."
    );
    Assert.equal(
      shoppingButton.getAttribute("shoppingsidebaropen"),
      "false",
      "Button not marked as open."
    );

    // Switch to non-product tab.
    await BrowserTestUtils.switchTab(
      gBrowser,
      gBrowser.getTabForBrowser(browser)
    );
    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "The shopping button is hidden on non-product page."
    );
    Assert.equal(
      shoppingButton.getAttribute("shoppingsidebaropen"),
      "false",
      "Button not marked as open."
    );
    // Switch to non-product tab.
    await BrowserTestUtils.switchTab(gBrowser, newProductTab);
    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "The shopping button is still visible on a different product tab after opting out."
    );
    Assert.equal(
      shoppingButton.getAttribute("shoppingsidebaropen"),
      "false",
      "Button not marked as open."
    );

    BrowserTestUtils.removeTab(newProductTab2);

    BrowserTestUtils.removeTab(newProductTab);
  });
});

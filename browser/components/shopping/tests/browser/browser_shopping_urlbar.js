/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PAGE = "https://example.com";
const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";

add_task(async function test_button_hidden() {
  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.is_hidden(shoppingButton),
      "Shopping Button should be hidden on a content page"
    );
  });
});

add_task(async function test_button_shown() {
  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.is_visible(shoppingButton),
      "Shopping Button should be visible on a product page"
    );
  });
});

// Button is hidden on navigation to a content page
add_task(async function test_button_changes_with_location() {
  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.is_hidden(shoppingButton),
      "Shopping Button should be hidden on a content page"
    );
    BrowserTestUtils.loadURIString(browser, PRODUCT_PAGE);
    await BrowserTestUtils.browserLoaded(browser);
    ok(
      BrowserTestUtils.is_visible(shoppingButton),
      "Shopping Button should be visible on a product page"
    );
    BrowserTestUtils.loadURIString(browser, CONTENT_PAGE);
    await BrowserTestUtils.browserLoaded(browser);
    ok(
      BrowserTestUtils.is_hidden(shoppingButton),
      "Shopping Button should be hidden on a content page"
    );
  });
});

add_task(async function test_button_active() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", true);

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      shoppingButton.getAttribute("shoppingsidebaropen") == "true",
      "Shopping Button should be active when sidebar is open"
    );
  });
});

add_task(async function test_button_inactive() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", false);

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      shoppingButton.getAttribute("shoppingsidebaropen") == "false",
      "Shopping Button should be inactive when sidebar is closed"
    );
  });
});

// Switching Tabs shows and hides the button
add_task(async function test_button_changes_with_location() {
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
    BrowserTestUtils.is_visible(shoppingButton),
    "Shopping Button should be visible on a product page"
  );

  await BrowserTestUtils.switchTab(gBrowser, contentTab);
  ok(
    BrowserTestUtils.is_hidden(shoppingButton),
    "Shopping Button should be hidden on a content page"
  );

  await BrowserTestUtils.removeTab(productTab);
  await BrowserTestUtils.removeTab(contentTab);
});

add_task(async function test_button_toggles_sidebars() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", false);

  let shoppingButton = document.getElementById("shopping-sidebar-button");
  let browser = gBrowser.selectedBrowser;
  let browserPanel = gBrowser.getPanel(browser);

  BrowserTestUtils.loadURIString(browser, PRODUCT_PAGE);
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
  ok(BrowserTestUtils.is_visible(sidebar), "Shopping sidebar should be open");

  // close
  shoppingButton.click();
  await BrowserTestUtils.waitForMutationCondition(
    shoppingButton,
    {
      attributeFilter: ["shoppingsidebaropen"],
    },
    () => shoppingButton.getAttribute("shoppingsidebaropen") == "false"
  );

  ok(BrowserTestUtils.is_hidden(sidebar), "Shopping sidebar should be closed");
});

// Button changes all Windows
add_task(async function test_button_toggles_all_windows() {
  Services.prefs.setBoolPref("browser.shopping.experience2023.active", false);

  let shoppingButton = document.getElementById("shopping-sidebar-button");

  BrowserTestUtils.loadURIString(gBrowser.selectedBrowser, PRODUCT_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let browserPanelA = gBrowser.getPanel(gBrowser.selectedBrowser);
  let sidebarA = browserPanelA.querySelector("shopping-sidebar");

  let newWindow = await BrowserTestUtils.openNewBrowserWindow();

  BrowserTestUtils.loadURIString(
    newWindow.gBrowser.selectedBrowser,
    PRODUCT_PAGE
  );
  await BrowserTestUtils.browserLoaded(newWindow.gBrowser.selectedBrowser);

  let browserPanelB = gBrowser.getPanel(newWindow.gBrowser.selectedBrowser);
  let sidebarB = browserPanelB.querySelector("shopping-sidebar");

  ok(
    BrowserTestUtils.is_hidden(sidebarA),
    "Shopping sidebar should be closed in current window"
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
    BrowserTestUtils.is_visible(sidebarA),
    "Shopping sidebar should be open in current window"
  );
  sidebarB = browserPanelB.querySelector("shopping-sidebar");
  ok(
    BrowserTestUtils.is_visible(sidebarB),
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
    BrowserTestUtils.is_hidden(sidebarA),
    "Shopping sidebar should be closed in current window"
  );
  ok(
    BrowserTestUtils.is_hidden(sidebarB),
    "Shopping sidebar should be closed in new window"
  );

  await BrowserTestUtils.closeWindow(newWindow);
});

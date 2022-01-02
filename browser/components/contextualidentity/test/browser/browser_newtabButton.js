"use strict";

// Testing that when the user opens the add tab menu and clicks menu items
// the correct context id is opened

function findPopup(browser = gBrowser) {
  return browser.tabContainer.querySelector(".new-tab-popup");
}

function findContextPopup() {
  return document.querySelector("#new-tab-button-popup");
}

add_task(async function test_containers_no_left_click() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userContext.enabled", true],
      ["privacy.userContext.newTabContainerOnLeftClick.enabled", false],
    ],
  });

  let newTabButton = gBrowser.tabContainer.newTabButton;
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");
  let popup = findPopup();
  ok(popup, "new tab should have a popup");

  // Test context menu
  let contextMenu = findContextPopup();
  is(contextMenu.state, "closed", "Context menu is initally closed.");

  let popupshown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");

  let target = gBrowser.tabContainer.newTabButton;
  EventUtils.synthesizeMouseAtCenter(target, { type: "contextmenu" });
  await popupshown;

  is(contextMenu.state, "open", "Context menu is open.");
  let contextIdItems = contextMenu.querySelectorAll("menuitem");
  // 4 + default + manage containers
  is(contextIdItems.length, 6, "Has 6 menu items");
  contextMenu.hidePopup();
});

add_task(async function test_containers_with_left_click() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userContext.enabled", true],
      ["privacy.userContext.newTabContainerOnLeftClick.enabled", true],
    ],
  });

  let newTabButton = gBrowser.tabContainer.newTabButton;
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");

  await BrowserTestUtils.waitForCondition(
    () => !!findPopup(),
    "Wait for popup to exist"
  );
  let popup = findPopup();

  let popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(newTabButton, { type: "mousedown" });
  await popupShownPromise;
  let contextIdItems = popup.querySelectorAll("menuitem");
  // 4 + default + manage containers
  is(contextIdItems.length, 6, "Has 6 menu items");
  popup.hidePopup();
  await popupHiddenPromise;

  for (let i = 0; i <= 4; i++) {
    popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(newTabButton, { type: "mousedown" });

    await popupShownPromise;
    let contextIdItem = popup.querySelector(
      `menuitem[data-usercontextid="${i}"]`
    );

    ok(contextIdItem, `User context id ${i} exists`);

    // waitForNewTab doesn't work for default tabs due to a different code path that doesn't cause a load event
    let waitForTabPromise = BrowserTestUtils.waitForEvent(
      gBrowser.tabContainer,
      "TabOpen"
    );
    EventUtils.synthesizeMouseAtCenter(contextIdItem, {});

    let tabEvent = await waitForTabPromise;
    let tab = tabEvent.target;
    if (i > 0) {
      is(
        tab.getAttribute("usercontextid"),
        "" + i,
        `New tab has UCI equal ${i}`
      );
    } else {
      ok(!tab.hasAttribute("usercontextid"), `New tab has no UCI`);
    }
    BrowserTestUtils.removeTab(tab);
  }

  // Test context menu
  let contextMenu = findContextPopup();
  is(contextMenu.state, "closed", "Context menu is initally closed.");

  let popupshown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");

  let target = gBrowser.tabContainer.newTabButton;
  EventUtils.synthesizeMouseAtCenter(target, { type: "contextmenu" });
  await popupshown;

  is(contextMenu.state, "open", "Context menu is open.");
  contextIdItems = contextMenu.querySelectorAll("menuitem");
  // 4 + default + manage containers
  is(contextIdItems.length, 6, "Has 6 menu items");
  contextMenu.hidePopup();
});

add_task(async function test_no_containers_no_left_click() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userContext.enabled", false],
      ["privacy.userContext.newTabContainerOnLeftClick.enabled", false],
    ],
  });

  let newTabButton = gBrowser.tabContainer.newTabButton;
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");
  let popup = findPopup();
  ok(!popup, "new tab should not have a popup");

  // Test context menu
  let contextMenu = findContextPopup();
  let target = gBrowser.tabContainer.newTabButton;
  EventUtils.synthesizeMouseAtCenter(target, { type: "contextmenu" });
  is(contextMenu.state, "closed", "Context menu does not open.");
});

add_task(async function test_private_mode() {
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let privateDocument = privateWindow.document;
  let { tabContainer } = privateWindow.gBrowser;
  let newTab = tabContainer.newTabButton;
  let newTab2 = privateDocument.getElementById("new-tab-button");
  // Check to ensure we are talking about the right button
  ok(!!newTab.clientWidth, "new tab button should not be hidden");
  ok(!newTab2.clientWidth, "overflow new tab button should be hidden");
  let popup = findPopup(privateWindow.gBrowser);
  ok(!popup, "new tab should not have a popup");
  await BrowserTestUtils.closeWindow(privateWindow);
});

add_task(async function test_opening_container_tab_context() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userContext.enabled", true],
      ["privacy.userContext.newTabContainerOnLeftClick.enabled", true],
    ],
  });

  let newTabButton = gBrowser.tabContainer.newTabButton;
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");

  let popup = findContextPopup();

  let popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(newTabButton, { type: "contextmenu" });
  await popupShownPromise;
  let contextIdItems = popup.querySelectorAll("menuitem");
  // 4 + default + manage containers
  is(contextIdItems.length, 6, "Has 6 menu items");
  popup.hidePopup();
  await popupHiddenPromise;

  for (let i = 0; i <= 4; i++) {
    popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(newTabButton, { type: "contextmenu" });

    await popupShownPromise;
    let contextIdItem = popup.querySelector(
      `menuitem[data-usercontextid="${i}"]`
    );

    ok(contextIdItem, `User context id ${i} exists`);

    // waitForNewTab doesn't work for default tabs due to a different code path that doesn't cause a load event
    let waitForTabPromise = BrowserTestUtils.waitForEvent(
      gBrowser.tabContainer,
      "TabOpen"
    );
    popup.activateItem(contextIdItem);

    let tabEvent = await waitForTabPromise;
    let tab = tabEvent.target;
    if (i > 0) {
      is(
        tab.getAttribute("usercontextid"),
        "" + i,
        `New tab has UCI equal ${i}`
      );
    } else {
      ok(!tab.hasAttribute("usercontextid"), `New tab has no UCI`);
    }
    BrowserTestUtils.removeTab(tab);
  }
});

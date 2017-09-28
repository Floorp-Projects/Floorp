"use strict";

// Testing that when the user opens the add tab menu and clicks menu items
// the correct context id is opened

add_task(async function test_menu_with_timeout() {
  await SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true],
      ["privacy.userContext.longPressBehavior", 2]
  ]});

  let newTab = document.getElementById("tabbrowser-tabs");
  let newTabButton = document.getAnonymousElementByAttribute(newTab, "anonid", "tabs-newtab-button");
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");
  await BrowserTestUtils.waitForCondition(() => !!document.getAnonymousElementByAttribute(newTab, "anonid", "newtab-popup"), "Wait for popup to exist");
  let popup = document.getAnonymousElementByAttribute(newTab, "anonid", "newtab-popup");

  for (let i = 1; i <= 4; i++) {
    let popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(newTabButton, {type: "mousedown"});

    await popupShownPromise;
    let contextIdItem = popup.querySelector(`menuitem[data-usercontextid="${i}"]`);

    ok(contextIdItem, `User context id ${i} exists`);

    let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    EventUtils.synthesizeMouseAtCenter(contextIdItem, {});

    let tab = await waitForTabPromise;

    is(tab.getAttribute("usercontextid"), i, `New tab has UCI equal ${i}`);
    await BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_menu_without_timeout() {
  await SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true],
      ["privacy.userContext.longPressBehavior", 1]
  ]});

  let newTab = document.getElementById("tabbrowser-tabs");
  let newTabButton = document.getAnonymousElementByAttribute(newTab, "anonid", "tabs-newtab-button");
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");
  await BrowserTestUtils.waitForCondition(() => !!document.getAnonymousElementByAttribute(newTab, "anonid", "newtab-popup"), "Wait for popup to exist");
  let popup = document.getAnonymousElementByAttribute(newTab, "anonid", "newtab-popup");

  let popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(newTabButton, {type: "mousedown"});
  await popupShownPromise;
  let contextIdItems = popup.querySelectorAll("menuitem");
  // 4 + default + manage containers
  is(contextIdItems.length, 6, "Has 6 menu items");
  popup.hidePopup();
  await popupHiddenPromise;

  for (let i = 0; i <= 4; i++) {
    popupShownPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(newTabButton, {type: "mousedown"});

    await popupShownPromise;
    let contextIdItem = popup.querySelector(`menuitem[data-usercontextid="${i}"]`);

    ok(contextIdItem, `User context id ${i} exists`);

    // waitForNewTab doesn't work for default tabs due to a different code path that doesn't cause a load event
    let waitForTabPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
    EventUtils.synthesizeMouseAtCenter(contextIdItem, {});

    let tabEvent = await waitForTabPromise;
    let tab = tabEvent.target;
    if (i > 0) {
      is(tab.getAttribute("usercontextid"), i, `New tab has UCI equal ${i}`);
    } else {
      ok(!tab.hasAttribute("usercontextid"), `New tab has no UCI`);
    }
    await BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_no_menu() {
  await SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true],
      ["privacy.userContext.longPressBehavior", 0]
  ]});

  let newTab = document.getElementById("tabbrowser-tabs");
  let newTabButton = document.getAnonymousElementByAttribute(newTab, "anonid", "tabs-newtab-button");
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");
  let popup = document.getAnonymousElementByAttribute(newTab, "anonid", "newtab-popup");
  ok(!popup, "new tab should not have a popup");
});

add_task(async function test_private_mode() {
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let privateDocument = privateWindow.document;
  let {tabContainer} = privateWindow.gBrowser;
  let newTab = privateDocument.getAnonymousElementByAttribute(tabContainer, "anonid", "tabs-newtab-button");
  let newTab2 = privateDocument.getElementById("new-tab-button");
  // Check to ensure we are talking about the right button
  ok(!!newTab.clientWidth, "new tab button should not be hidden");
  ok(!newTab2.clientWidth, "overflow new tab button should be hidden");
  let popup = privateDocument.getAnonymousElementByAttribute(newTab, "anonid", "newtab-popup");
  ok(!popup, "new tab should not have a popup");
  await BrowserTestUtils.closeWindow(privateWindow);
});

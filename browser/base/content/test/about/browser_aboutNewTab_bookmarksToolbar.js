/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function bookmarks_toolbar_shown_on_newtab() {
  for (let featureEnabled of [true, false]) {
    info(
      "Testing with the feature " + (featureEnabled ? "enabled" : "disabled")
    );
    await SpecialPowers.pushPrefEnv({
      set: [["browser.toolbars.bookmarks.2h2020", featureEnabled]],
    });
    let newtab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });
    if (featureEnabled) {
      await waitForToolbarVisibility({ visible: true });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar should be visible on newtab if enabled"
    );

    let example = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "https://example.com",
    });
    await waitForToolbarVisibility({ visible: false });
    ok(!isBookmarksToolbarVisible(), "Toolbar should be hidden on example.com");

    await BrowserTestUtils.switchTab(gBrowser, newtab);
    if (featureEnabled) {
      await waitForToolbarVisibility({ visible: true });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible with switch to newtab if enabled"
    );

    await BrowserTestUtils.switchTab(gBrowser, example);
    await waitForToolbarVisibility({ visible: false });
    ok(
      !isBookmarksToolbarVisible(),
      "Toolbar is hidden with switch to example"
    );

    await BrowserTestUtils.loadURI(example.linkedBrowser, "about:newtab");
    if (featureEnabled) {
      await waitForToolbarVisibility({ visible: true });
    }
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible with newtab load if enabled"
    );

    await BrowserTestUtils.switchTab(gBrowser, newtab);
    is(
      isBookmarksToolbarVisible(),
      featureEnabled,
      "Toolbar is visible switch newtab to newtab if enabled"
    );

    await BrowserTestUtils.removeTab(newtab);
    await BrowserTestUtils.removeTab(example);
  }
});

add_task(async function bookmarks_toolbar_open_persisted() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });
  let newtab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:newtab",
    waitForLoad: false,
  });
  let example = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
  });
  let isToolbarPersistedOpen = () =>
    Services.prefs.getBoolPref("browser.toolbars.bookmarks.persist_open");
  let tabsBar = gNavToolbox.querySelector("#TabsToolbar");
  let contextMenu = document.querySelector("#toolbar-context-menu");

  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouse(tabsBar, 10, 10, { type: "contextmenu" }, window);
  await popupShown;
  ok(!isToolbarPersistedOpen(), "Toolbar is not persisted open");
  let menuItem = document.querySelector("#toggle_PersonalToolbar");
  is(menuItem.getAttribute("checked"), "false", "Menuitem is not checked");
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");

  EventUtils.synthesizeMouseAtCenter(menuItem, {});

  await waitForToolbarVisibility({ visible: true });
  popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouse(tabsBar, 10, 10, { type: "contextmenu" }, window);
  await popupShown;
  menuItem = document.querySelector("#toggle_PersonalToolbar");
  is(menuItem.getAttribute("checked"), "true", "Menuitem is checked");
  contextMenu.hidePopup();
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  ok(isToolbarPersistedOpen(), "Toolbar is persisted open");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");

  popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouse(tabsBar, 10, 10, { type: "contextmenu" }, window);
  await popupShown;
  menuItem = document.querySelector("#toggle_PersonalToolbar");
  is(menuItem.getAttribute("checked"), "true", "Menuitem is checked");
  EventUtils.synthesizeMouseAtCenter(menuItem, {});
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");

  await BrowserTestUtils.removeTab(newtab);
  await BrowserTestUtils.removeTab(example);
});

add_task(async function test_with_newtabpage_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });
  let newtab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:newtab",
    waitForLoad: false,
  });
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");

  let blank = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:blank",
    waitForLoad: false,
  });
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");

  let example = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
  });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.enabled", false]],
  });
  await BrowserTestUtils.switchTab(gBrowser, blank);
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, example);
  await waitForToolbarVisibility({ visible: false });
  ok(!isBookmarksToolbarVisible(), "Toolbar is hidden");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForToolbarVisibility({ visible: true });
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");
  await BrowserTestUtils.switchTab(gBrowser, blank);
  ok(isBookmarksToolbarVisible(), "Toolbar is visible");

  await BrowserTestUtils.removeTab(newtab);
  await BrowserTestUtils.removeTab(blank);
  await BrowserTestUtils.removeTab(example);
});

async function waitForToolbarVisibility({ visible }) {
  return TestUtils.waitForCondition(() => {
    let toolbar = gNavToolbox.querySelector("#PersonalToolbar");
    let isVisible = toolbar.getAttribute("collapsed") != "true";
    return visible ? isVisible : !isVisible;
  }, "waiting for toolbar to become " + (visible ? "visible" : "hidden"));
}

function isBookmarksToolbarVisible() {
  let toolbar = gNavToolbox.querySelector("#PersonalToolbar");
  return toolbar.getAttribute("collapsed") != "true";
}
